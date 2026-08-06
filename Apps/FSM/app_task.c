/**
 * @file    app_task.c
 * @brief   Modul Task Utama Aplikasi (State Machine) untuk Smart Garden.
 * @details Mengelola FSM Irigasi Otomatis dan Sekuensial Pemupukan berbasis
 * Jadwal File SD Card dan sinkronisasi RTC (DS3231) dengan kepatuhan Zero-Blocking
 * serta manajemen memori Pinjam-Pakai (Zero-Copy Queue).
 * serta integrasi tingkat tinggi dengan Fail-Safe Manager.
 * @author  ferry
 * @date    22 Jul 2026 (Updated Aug 2026)
 */

#include "app_task.h"
#include "../Config/app_config.h"
#include "../Config/config_data.h"
#include "command_event.h"

/* Drivers & Wrappers (Sesuaikan include ini dengan nama file Anda yang sebenarnya) */
#include "../../Drivers/Actuator/actuator_driver.h"
#include "../../Drivers/flowmeter/flowmeter_driver.h"
#include "../../Drivers/water_lvl/water_lvl_driver.h"
#include "../../Drivers/water_quality/water_quality_driver.h"
#include "../../Wrappers/ds3231_wrapper.h"  /* Asumsi RTC menggunakan wrapper ini */

/* Managers */
#include "../Config/config_manager.h"
#include "../Managers/schedule_manager.h"
#include "../Managers/recipe_manager.h"
#include "../Managers/log_manager.h"
#include "../Managers/failsafe_manager.h"

/* --- Konteks FreeRTOS --- */
TaskHandle_t appTaskHandle;
QueueHandle_t appQueue;
extern QueueHandle_t wtrLvlQueue;
QueueSetHandle_t appQueueSet;

/* --- FSM State Tracker --- */
static AppFSMState_t currentState = STATE_INIT_HARDWARE;

/* --- Variabel Pelacakan Operasional Fluida --- */
static ScheduleItem_t active_sched;     /* Murni menggunakan struct dari schedule_manager.h Anda */
static uint8_t current_fert_index = 0;
static TickType_t mixing_start_tick = 0;
static TickType_t response_start_tick = 0;
static bool waiting_user_response = false;

/* --- INTEGRASI FAIL-SAFE: Pelacakan durasi aliran --- */
static TickType_t flow_start_tick = 0;
static bool is_flow_active = false;

/* Pemetaan array aktuator pupuk O(1) */
static const ActuatorType_t fert_valves[5] = {
		ACT_VALVE_FERT_1, ACT_VALVE_FERT_2, ACT_VALVE_FERT_3, ACT_VALVE_FERT_4, ACT_VALVE_FERT_5
};

/* Referensi Sensor Eksternal & Global (Sesuaikan dengan nama aktual di project Anda) */
extern FlowSensor_t fm_inlet;
extern FlowSensor_t fm_outlet;
extern FlowSensor_t fm_fert;

extern SystemConfig_t sys_config;
extern DS3231_Device_t DS3231_Ctx;


/* ============================================================================
 * PROTOTIPE STATE HANDLER (Fungsi Internal / Private)
 * ==========================================================================*/
static void FSM_Handle_InitHardware(void);
static void FSM_Handle_LoadCalibration(void);
static void FSM_Handle_Load_Schedule(void);
static void FSM_Handle_Set_Next_Alarm(void);
static void FSM_Handle_EvaluateMissedSchedule(void);
static void FSM_Handle_Irrigating(void);
static void FSM_Handle_Pre_Flushing(void);
static void FSM_Handle_Dosing(void);
static void FSM_Handle_Mixing(void);
static void FSM_Handle_Flushing(void);
static void FSM_Handle_Sync_Config_Calib(void);
static void FSM_Handle_Fault(void);


/**
 * @brief   Task FreeRTOS Utama untuk FSM Tersentralisasi
 */
static void vTaskApp(void *pvParameters) {
    (void)pvParameters;

    CommandEvent_t *evt_ptr = NULL;
    WtrLvl_Event_t wtrEvt;

    /* Inisialisasi QueueSet untuk multi-event non-blocking */
    appQueueSet = xQueueCreateSet(APP_QUEUE_SET_SIZE);
    if (appQueueSet != NULL) {
        xQueueAddToSet(appQueue, appQueueSet);
        xQueueAddToSet(wtrLvlQueue, appQueueSet);
    }

    for (;;) {
        /* -------------------------------------------------------------
         * 1. EVENT LISTENER (ZERO-BLOCKING DENGAN TIMEOUT 10MS)
         * ------------------------------------------------------------- */
        QueueSetMemberHandle_t activatedQueue = xQueueSelectFromSet(appQueueSet, pdMS_TO_TICKS(10));

        if (activatedQueue == appQueue) {
            if (xQueueReceive(appQueue, &evt_ptr, 0) == pdPASS && evt_ptr != NULL) {

                /* Penanganan konfirmasi jadwal URGENT */
                if (currentState == STATE_EVALUATE_MISSED_SCHEDULE && waiting_user_response) {
                    if (evt_ptr->cmd_id == CMD_USER_CONFIRM_YES) {
                        waiting_user_response = false;
                        LogManager_Write(LOG_INFO, "Operator menyetujui eksekusi jadwal tertinggal.");

                        /* Routing Jalur Berdasarkan Enum dari file Anda */
                        if (active_sched.type == SCHED_TYPE_IRRIGATION) {
                            FlowSensor_ResetVolume(&fm_inlet);
                            currentState = STATE_IRRIGATING;
                        } else {
                            current_fert_index = 0;
                            currentState = STATE_PRE_FLUSHING;
                        }
                    }
                    else if (evt_ptr->cmd_id == CMD_USER_CONFIRM_NO) {
                        waiting_user_response = false;
                        LogManager_Write(LOG_WARN, "Operator menolak jadwal tertinggal. Skip dieksekusi.");

                        /* Gunakan fungsi Update & Reschedule Anda */
                        ScheduleManager_UpdateStatus(active_sched.type, active_sched.line_number, SCHED_STATUS_SKIPPED);
                        ScheduleManager_AutoReschedule(&active_sched);
                        currentState = STATE_SET_NEXT_ALARM;
                    }
                }

                if (evt_ptr->cmd_id == CMD_EMERGENCY_STOP) {
                	/* INTEGRASI FAIL-SAFE: Panggil Lockdown jika User menekan Emergency */
					FailSafeManager_ExecuteLockdown(FAILSAFE_ERR_SYSTEM_PANIC);
                    currentState = STATE_FAULT;
                    LogManager_Write(LOG_ERROR, "CRITICAL: Emergency Stop ditekan!");
                }

                /* BEBASKAN MEMORI (ATURAN PINJAM PAKAI) */
                if (evt_ptr->payload.csv_data.str_ptr != NULL) vPortFree(evt_ptr->payload.csv_data.str_ptr);
                vPortFree(evt_ptr);
            }
        }
        else if (activatedQueue == wtrLvlQueue) {
            if (xQueueReceive(wtrLvlQueue, &wtrEvt, 0)) {
                if (wtrEvt.sensor == LVL_TANK_FULL && wtrEvt.is_reached && currentState == STATE_DOSING) {
                    LogManager_Write(LOG_WARN, "Tangki Penuh terdeteksi saat Dosing. Memaksa Mixing.");
                    currentState = STATE_MIXING;
                }
            }
        }

        /* -------------------------------------------------------------
         * 2. CENTRALIZED FINITE STATE MACHINE (15 STATE V3.6)
         * ------------------------------------------------------------- */
        switch (currentState) {

            case STATE_INIT_HARDWARE: // State 1
            	FSM_Handle_InitHardware(); break;
            case STATE_LOAD_CALIBRATION: // State 2
            	FSM_Handle_LoadCalibration(); break;
            case STATE_LOAD_SCHEDULE: // State 3
            	FSM_Handle_Load_Schedule(); break;
            case STATE_SET_NEXT_ALARM: // State 4
            	FSM_Handle_Set_Next_Alarm(); break;
            case STATE_SLEEP: // State 5
                /* Low Power Mode menunggu interupsi RTC / Bluetooth */
                break;
            /* --- EVALUASI JADWAL MENGGUNAKAN API BARU ANDA --- */
            case STATE_EVALUATE_MISSED_SCHEDULE: { // State 6
            	FSM_Handle_EvaluateMissedSchedule(); break;
            }
            /* --- JALUR A: IRIGASI MURNI --- */
            case STATE_IRRIGATING: // State 7
            	FSM_Handle_Irrigating(); break;
            /* --- JALUR B: FERTIGASI --- */
            case STATE_PRE_FLUSHING: // State 8
            	FSM_Handle_Pre_Flushing(); break;
            case STATE_DOSING: // State 9
            	FSM_Handle_Dosing(); break;
            case STATE_MIXING: // State 10
            	FSM_Handle_Mixing(); break;
            case STATE_FLUSHING: // State 11
            	FSM_Handle_Flushing(); break;
            /* --- FASE 4: USER INTERVENTION & FAIL-SAFE --- */
            case STATE_BT_INTERACTIVE: // State 12
            	/* Mode Standby khusus saat koneksi Bluetooth aktif.
				 * Sistem diam di sini melayani ping/request dari Smartphone. */
                break;

            case STATE_SYNC_CONFIG_CALIB: // State 13 (GABUNGAN KALIBRASI & JADWAL)
            	FSM_Handle_Sync_Config_Calib(); break;
            case STATE_FAULT: // State 14
            	FSM_Handle_Fault(); break;
            default:
                currentState = STATE_INIT_HARDWARE;
                break;
        }

        WaterQuality_ProcessAnalog();
    }
}


static void FSM_Handle_InitHardware(void){
    Actuator_Init();
    WtrLvl_Init();
    FailSafeManager_Init(); /* --- INTEGRASI FAIL-SAFE --- */
	is_flow_active = false;
    currentState = STATE_LOAD_CALIBRATION;
}

static void FSM_Handle_LoadCalibration(void){
    ConfigManager_Init();
    currentState = STATE_LOAD_SCHEDULE;
}

static void FSM_Handle_Load_Schedule(void){
    ScheduleManager_Init(); /* Memanggil init dari file Anda */
    currentState = STATE_EVALUATE_MISSED_SCHEDULE;
}

static void FSM_Handle_Set_Next_Alarm(void){
	waiting_user_response = false;
	    uint32_t current_epoch = DS3231_GetEpochTime(&DS3231_Ctx);

	    uint32_t next_irrigation_epoch = ScheduleManager_GetNextUpcomingEpoch(SCHED_TYPE_IRRIGATION, current_epoch);
	    uint32_t next_fertilizer_epoch = ScheduleManager_GetNextUpcomingEpoch(SCHED_TYPE_FERTILIZER, current_epoch);
	    uint32_t target_alarm_epoch = 0;

	    if (next_irrigation_epoch > 0 && next_fertilizer_epoch > 0) {
	        target_alarm_epoch = (next_irrigation_epoch < next_fertilizer_epoch) ? next_irrigation_epoch : next_fertilizer_epoch;
	    } else if (next_irrigation_epoch > 0) {
	        target_alarm_epoch = next_irrigation_epoch;
	    } else if (next_fertilizer_epoch > 0) {
	        target_alarm_epoch = next_fertilizer_epoch;
	    }

	    if (target_alarm_epoch > 0) {
	        DS3231_DateTime_t alarm_dt;

	        DS3231_EpochToDateTime(target_alarm_epoch, &alarm_dt);

	        if (DS3231_SetAlarm1(&DS3231_Ctx, &alarm_dt, DS3231_ALARM1_MATCH_DATE_HOURS_MIN_SEC)) {
	            LogManager_Write(LOG_INFO, "FSM: Alarm disetel ke Tgl %02d Jam %02d:%02d:%02d. SLEEP.",
	                             alarm_dt.date.date, alarm_dt.time.hours, alarm_dt.time.minutes, alarm_dt.time.seconds);
	        } else {
	            FailSafeManager_ExecuteLockdown(FAILSAFE_ERR_I2C_DISCONNECT);
	            currentState = STATE_FAULT;
	            return;
	        }
	    } else {
	        LogManager_Write(LOG_WARN, "FSM: Tidak ada jadwal tersisa di SD Card.");
	    }

	    DS3231_ClearAlarm1Flag(&DS3231_Ctx);
	    currentState = STATE_SLEEP;
}

static void FSM_Handle_EvaluateMissedSchedule(void){
    /* NOTE: Pastikan fungsi ini memanggil waktu Epoch aktual dari RTC hardware Anda */
    uint32_t current_epoch = DS3231_GetEpochTime(&DS3231_Ctx);

    /* --- INTEGRASI FAIL-SAFE: HUKUM KEWARASAN WAKTU --- */
	FailSafeError_t time_err = FailSafeManager_CheckTimeSanity(current_epoch, 0);
	if (time_err != FAILSAFE_OK) {
		FailSafeManager_ExecuteLockdown(time_err);
		currentState = STATE_FAULT;
		return;
	}

    uint32_t tolerance_sec = sys_config.max_delay_tolerance * 60;

    if (!waiting_user_response) {
        /* Memanggil API baru Anda. Cek Irigasi dulu, lalu Fertigasi */
        bool found = ScheduleManager_GetDueSchedule(SCHED_TYPE_IRRIGATION, &active_sched, current_epoch, tolerance_sec);
        if (!found) {
            found = ScheduleManager_GetDueSchedule(SCHED_TYPE_FERTILIZER, &active_sched, current_epoch, tolerance_sec);
        }

        if (found) {
            /* Cek status Enum hasil ekstraksi Anda */
            if (active_sched.status == SCHED_STATUS_URGENT) {
                LogManager_Write(LOG_WARN, "Jadwal Baris %lu URGENT (Terlambat). Menunggu konfirmasi operator.", active_sched.line_number);
                response_start_tick = xTaskGetTickCount();
                waiting_user_response = true;
            }
            else if (active_sched.status == SCHED_STATUS_PENDING) {
                if (active_sched.type == SCHED_TYPE_IRRIGATION) {
                    FlowSensor_ResetVolume(&fm_inlet);
                    currentState = STATE_IRRIGATING;
                } else {
                    current_fert_index = 0;
                    currentState = STATE_PRE_FLUSHING;
                }
            }
        } else {
            currentState = STATE_SET_NEXT_ALARM;
        }
    }
    else {
        /* TIMEOUT KONFIRMASI (Non-Blocking) */
        uint32_t elapsed_sec = (xTaskGetTickCount() - response_start_tick) / configTICK_RATE_HZ;
        if (elapsed_sec >= sys_config.waiting_user_response_time) {
            waiting_user_response = false;
            LogManager_Write(LOG_ERROR, "TIMEOUT Operator. Jadwal URGENT dibatalkan.");

            ScheduleManager_UpdateStatus(active_sched.type, active_sched.line_number, SCHED_STATUS_SKIPPED);
            currentState = STATE_SET_NEXT_ALARM;
        }
    }
}

static void FSM_Handle_Irrigating(void){
	if (!is_flow_active) {
		Actuator_SetState(ACT_VALVE_WATER_IN, ACT_ON);
		Actuator_SetState(ACT_PUMP_OUT, ACT_ON);
		FlowSensor_Start(&fm_inlet);
		flow_start_tick = xTaskGetTickCount();
		is_flow_active = true;
	}
	/* --- INTEGRASI FAIL-SAFE: HUKUM KEHADIRAN ALIRAN --- */
	uint32_t elapsed_irr_ms = (xTaskGetTickCount() - flow_start_tick) * portTICK_PERIOD_MS;
	uint32_t actual_irr_flow = FlowSensor_GetVolume(&fm_inlet);

	FailSafeError_t err_irr = FailSafeManager_CheckFlow(active_sched.recipe.water_volume, actual_irr_flow, elapsed_irr_ms);
	if (err_irr != FAILSAFE_OK) {
		FailSafeManager_ExecuteLockdown(err_irr);
		is_flow_active = false;
		currentState = STATE_FAULT;
		return;
	}

	/* Mengambil data dari internal struct active_sched.recipe buatan Anda! */
	if (FlowSensor_GetVolume(&fm_inlet) >= active_sched.recipe.water_volume) {
		Actuator_SetState(ACT_VALVE_WATER_IN, ACT_OFF);
		Actuator_SetState(ACT_PUMP_OUT, ACT_OFF);
		FlowSensor_Stop(&fm_inlet);

		/* Gunakan API Update dan AutoReschedule Anda */
		ScheduleManager_UpdateStatus(active_sched.type, active_sched.line_number, SCHED_STATUS_SUCCESS);
		ScheduleManager_AutoReschedule(&active_sched);

		LogManager_Write(LOG_INFO, "Irigasi Murni Selesai.");
		is_flow_active = false;
		currentState = STATE_SET_NEXT_ALARM;
	}
}

static void FSM_Handle_Pre_Flushing(void){
    if (isWtrLvl_Empty()) {
        FlowSensor_ResetVolume(&fm_fert);
        currentState = STATE_DOSING;
    } else {
        Actuator_SetState(ACT_VALVE_TANK_OUT, ACT_ON);
        Actuator_SetState(ACT_PUMP_OUT, ACT_ON);
    }
}

static void FSM_Handle_Dosing(void){
    /* Melewati array fert_volumes internal struct Anda yang bernilai 0 */
    while (current_fert_index < RECIPE_NUM_FERTILIZERS && active_sched.recipe.fert_volumes[current_fert_index] == 0) {
        current_fert_index++;
    }

    if (current_fert_index >= RECIPE_NUM_FERTILIZERS) {
        Actuator_SetState(ACT_PUMP_FERT, ACT_OFF);
        FlowSensor_Stop(&fm_fert);

        mixing_start_tick = xTaskGetTickCount();
        is_flow_active = false;
        currentState = STATE_MIXING;
        return;
    }

    ActuatorType_t active_valve = fert_valves[current_fert_index];

    if (!is_flow_active) {
		Actuator_SetState(active_valve, ACT_ON);
		Actuator_SetState(ACT_PUMP_FERT, ACT_ON);
		FlowSensor_Start(&fm_fert);
		flow_start_tick = xTaskGetTickCount();
		is_flow_active = true;
	}

    /* --- INTEGRASI FAIL-SAFE: HUKUM KEHADIRAN ALIRAN --- */
	uint32_t elapsed_dos_ms = (xTaskGetTickCount() - flow_start_tick) * portTICK_PERIOD_MS;
	uint32_t actual_dos_flow = FlowSensor_GetVolume(&fm_fert);

	FailSafeError_t err_dos = FailSafeManager_CheckFlow(active_sched.recipe.fert_volumes[current_fert_index], actual_dos_flow, elapsed_dos_ms);
	if (err_dos != FAILSAFE_OK) {
		FailSafeManager_ExecuteLockdown(err_dos);
		is_flow_active = false;
		currentState = STATE_FAULT;
		return;
	}

    if (FlowSensor_GetVolume(&fm_fert) >= active_sched.recipe.fert_volumes[current_fert_index]) {
        Actuator_SetState(active_valve, ACT_OFF);
        Actuator_SetState(ACT_PUMP_FERT, ACT_OFF);
        FlowSensor_ResetVolume(&fm_fert);
        current_fert_index++;
        is_flow_active = false; /* Reset flag untuk valve selanjutnya */
    }
}

static void FSM_Handle_Mixing(void){
    WaterQualityData_t wq;
    Actuator_SetState(ACT_MIXER, ACT_ON);
    wq = WaterQuality_GetData();

    if (wq.ec_val > sys_config.max_ec_limit && !isWtrLvl_Full()) {
        Actuator_SetState(ACT_VALVE_TANK_IN, ACT_ON);
    } else {
        Actuator_SetState(ACT_VALVE_TANK_IN, ACT_OFF);
    }

    /* Menggunakan mixing_time_sec dari struct Anda */
    if ((xTaskGetTickCount() - mixing_start_tick) >= pdMS_TO_TICKS(active_sched.recipe.mixing_time_sec * 1000)) {
        Actuator_SetState(ACT_MIXER, ACT_OFF);
        Actuator_SetState(ACT_VALVE_TANK_IN, ACT_OFF);
        FlowSensor_ResetVolume(&fm_outlet);
        currentState = STATE_FLUSHING;
    }
}

static void FSM_Handle_Flushing(void){
	if (!is_flow_active) {
		Actuator_SetState(ACT_VALVE_TANK_OUT, ACT_ON);
		Actuator_SetState(ACT_PUMP_OUT, ACT_ON);
		FlowSensor_Start(&fm_outlet);
		flow_start_tick = xTaskGetTickCount();
		is_flow_active = true;
	}

	/* --- INTEGRASI FAIL-SAFE: HUKUM KEHADIRAN ALIRAN --- */
	uint32_t elapsed_flush_ms = (xTaskGetTickCount() - flow_start_tick) * portTICK_PERIOD_MS;
	uint32_t actual_flush_flow = FlowSensor_GetVolume(&fm_outlet);

	FailSafeError_t err_flush = FailSafeManager_CheckFlow(active_sched.recipe.water_volume, actual_flush_flow, elapsed_flush_ms);
	if (err_flush != FAILSAFE_OK) {
		FailSafeManager_ExecuteLockdown(err_flush);
		is_flow_active = false;
		currentState = STATE_FAULT;
		return;
	}

	if (FlowSensor_GetVolume(&fm_outlet) >= active_sched.recipe.water_volume || isWtrLvl_Empty()) {
		Actuator_SetState(ACT_PUMP_OUT, ACT_OFF);
		Actuator_SetState(ACT_VALVE_TANK_OUT, ACT_OFF);
		FlowSensor_Stop(&fm_outlet);

		/* Finalisasi Jadwal */
		ScheduleManager_UpdateStatus(active_sched.type, active_sched.line_number, SCHED_STATUS_SUCCESS);
		ScheduleManager_AutoReschedule(&active_sched);

		LogManager_Write(LOG_INFO, "Siklus Fertigasi Selesai.");
		is_flow_active = false;
		currentState = STATE_SET_NEXT_ALARM;
	}
}

static void FSM_Handle_Sync_Config_Calib(void) {
	/* State ini dipanggil jika command_task menerima JSON jadwal baru,
	 * atau command kalibrasi pH/EC baru dari HP.
	 * * Aktivitas:
	 * 1. EEPROM (Config) sudah ditulis oleh command_task/config_manager
	 * 2. SD Card (Schedule) sudah ditimpa oleh command_task
	 * 3. Di sini, FSM hanya bertugas me-reload ulang indeks ke dalam RAM!
	 */
	ScheduleManager_Init(); /* Me-reload ulang pointer memori jadwal */
	ConfigManager_Init();   /* (Opsional) Memastikan RAM sinkron dengan EEPROM terbaru */

	LogManager_Write(LOG_INFO, "Sinkronisasi Konfigurasi/Kalibrasi Selesai.");

	/* Setelah sinkron, cek ulang apakah ada jadwal yang harus segera dieksekusi */
	currentState = STATE_EVALUATE_MISSED_SCHEDULE;
}

static void FSM_Handle_Fault(void) {
    /* HARDWARE LOCKDOWN: Force LOW ke seluruh aktuator seketika */
    Actuator_Init();
    /* Berdiam di sini sampai menerima command RESET */
}


void APP_TaskCreate(UBaseType_t priority){
    appQueue = xQueueCreate(APP_QUEUE_SIZE, sizeof(CommandEvent_t *));
    if (appQueue != NULL) {
        xTaskCreate(vTaskApp, "AppTask", 1024, NULL, priority, &appTaskHandle);
    }
}
