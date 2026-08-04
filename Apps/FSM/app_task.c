/**
 * @file    app_task.c
 * @brief   Modul Task Utama Aplikasi (State Machine) untuk Smart Garden.
 * @details Mengelola FSM Irigasi Otomatis dan Sekuensial Pemupukan berbasis
 * Jadwal File SD Card dan sinkronisasi RTC (DS3231) dengan kepatuhan Zero-Blocking
 * serta manajemen memori Pinjam-Pakai (Zero-Copy Queue).
 * @author  ferry
 * @date    22 Jul 2026
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

/* Pemetaan array aktuator pupuk O(1) */
static const ActuatorType_t fert_valves[5] = {
		ACT_VALVE_FERT_1, ACT_VALVE_FERT_2, ACT_VALVE_FERT_3, ACT_VALVE_FERT_4, ACT_VALVE_FERT_5
};

/* Referensi Sensor Eksternal & Global (Sesuaikan dengan nama aktual di project Anda) */
extern FlowSensor_t sensor_fert;
extern FlowSensor_t sensor_inlet;
extern FlowSensor_t sensor_outlet;
extern SensorCalibration_t sys_calib;
extern DS3231_Device_t sys_rtc;

/**
 * @brief   Task FreeRTOS Utama untuk FSM Tersentralisasi
 */
static void vTaskApp(void *pvParameters) {
    (void)pvParameters;

    CommandEvent_t *evt_ptr = NULL;
    WtrLvl_Event_t wtrEvt;
    WaterQualityData_t wq;

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
                            FlowSensor_ResetVolume(&sensor_inlet);
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
                    currentState = STATE_FAULT;
                    LogManager_Write(LOG_ERROR, "CRITICAL: Emergency Stop ditekan!");
                }

                /* BEBASKAN MEMORI (ATURAN PINJAM PAKAI) */
                if (evt_ptr->payload.str_ptr != NULL) vPortFree(evt_ptr->payload.str_ptr);
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

            case STATE_INIT_HARDWARE:
                Actuator_Init();
                WtrLvl_Init();
                currentState = STATE_LOAD_CALIBRATION;
                break;

            case STATE_LOAD_CALIBRATION:
                ConfigManager_Init();
                currentState = STATE_LOAD_SCHEDULE;
                break;

            case STATE_LOAD_SCHEDULE:
                ScheduleManager_Init(); /* Memanggil init dari file Anda */
                currentState = STATE_EVALUATE_MISSED_SCHEDULE;
                break;

            case STATE_SET_NEXT_ALARM:
                waiting_user_response = false;
                /* NOTE: Setel alarm ke RTC di sini */
                currentState = STATE_SLEEP;
                break;

            case STATE_SLEEP:
                /* Low Power Mode menunggu interupsi RTC / Bluetooth */
                break;

            /* --- EVALUASI JADWAL MENGGUNAKAN API BARU ANDA --- */
            case STATE_EVALUATE_MISSED_SCHEDULE: {
                /* NOTE: Pastikan fungsi ini memanggil waktu Epoch aktual dari RTC hardware Anda */
                uint32_t current_epoch = DS3231_GetEpochTime(&sys_rtc);
                uint32_t tolerance_sec = sys_calib.max_delay_tolerance * 60;

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
                                FlowSensor_ResetVolume(&sensor_inlet);
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
                    if (elapsed_sec >= sys_calib.waiting_user_response_time) {
                        waiting_user_response = false;
                        LogManager_Write(LOG_ERROR, "TIMEOUT Operator. Jadwal URGENT dibatalkan.");

                        ScheduleManager_UpdateStatus(active_sched.type, active_sched.line_number, SCHED_STATUS_SKIPPED);
                        currentState = STATE_SET_NEXT_ALARM;
                    }
                }
                break;
            }

            /* --- JALUR A: IRIGASI MURNI --- */
            case STATE_IRRIGATING:
                Actuator_SetState(ACT_VALVE_WATER_IN, ACT_ON);
                Actuator_SetState(ACT_PUMP_OUT, ACT_ON);
                FlowSensor_Start(&sensor_inlet);

                /* Mengambil data dari internal struct active_sched.recipe buatan Anda! */
                if (FlowSensor_GetVolume(&sensor_inlet) >= active_sched.recipe.water_volume) {
                    Actuator_SetState(ACT_VALVE_WATER_IN, ACT_OFF);
                    Actuator_SetState(ACT_PUMP_OUT, ACT_OFF);
                    FlowSensor_Stop(&sensor_inlet);

                    /* Gunakan API Update dan AutoReschedule Anda */
                    ScheduleManager_UpdateStatus(active_sched.type, active_sched.line_number, SCHED_STATUS_SUCCESS);
                    ScheduleManager_AutoReschedule(&active_sched);

                    LogManager_Write(LOG_INFO, "Irigasi Murni Selesai.");
                    currentState = STATE_SET_NEXT_ALARM;
                }
                break;

            /* --- JALUR B: FERTIGASI --- */
            case STATE_PRE_FLUSHING:
                if (isWtrLvl_Empty()) {
                    FlowSensor_ResetVolume(&sensor_fert);
                    currentState = STATE_DOSING;
                } else {
                    Actuator_SetState(ACT_VALVE_TANK_OUT, ACT_ON);
                    Actuator_SetState(ACT_PUMP_OUT, ACT_ON);
                }
                break;

            case STATE_DOSING:
                /* Melewati array fert_volumes internal struct Anda yang bernilai 0 */
                while (current_fert_index < RECIPE_NUM_FERTILIZERS && active_sched.recipe.fert_volumes[current_fert_index] == 0) {
                    current_fert_index++;
                }

                if (current_fert_index >= RECIPE_NUM_FERTILIZERS) {
                    Actuator_SetState(ACT_PUMP_FERT, ACT_OFF);
                    FlowSensor_Stop(&sensor_fert);

                    mixing_start_tick = xTaskGetTickCount();
                    currentState = STATE_MIXING;
                    break;
                }

                ActuatorType_t active_valve = fert_valves[current_fert_index];
                Actuator_SetState(active_valve, ACT_ON);
                Actuator_SetState(ACT_PUMP_FERT, ACT_ON);
                FlowSensor_Start(&sensor_fert);

                if (FlowSensor_GetVolume(&sensor_fert) >= active_sched.recipe.fert_volumes[current_fert_index]) {
                    Actuator_SetState(active_valve, ACT_OFF);
                    Actuator_SetState(ACT_PUMP_FERT, ACT_OFF);
                    FlowSensor_ResetVolume(&sensor_fert);
                    current_fert_index++;
                }
                break;

            case STATE_MIXING:
                Actuator_SetState(ACT_MIXER, ACT_ON);
                wq = WaterQuality_GetData();

                if (wq.ec_val > sys_calib.max_ec_limit && !isWtrLvl_Full()) {
                    Actuator_SetState(ACT_VALVE_TANK_IN, ACT_ON);
                } else {
                    Actuator_SetState(ACT_VALVE_TANK_IN, ACT_OFF);
                }

                /* Menggunakan mixing_time_sec dari struct Anda */
                if ((xTaskGetTickCount() - mixing_start_tick) >= pdMS_TO_TICKS(active_sched.recipe.mixing_time_sec * 1000)) {
                    Actuator_SetState(ACT_MIXER, ACT_OFF);
                    Actuator_SetState(ACT_VALVE_TANK_IN, ACT_OFF);
                    FlowSensor_ResetVolume(&sensor_outlet);
                    currentState = STATE_FLUSHING;
                }
                break;

            case STATE_FLUSHING:
                Actuator_SetState(ACT_VALVE_TANK_OUT, ACT_ON);
                Actuator_SetState(ACT_PUMP_OUT, ACT_ON);
                FlowSensor_Start(&sensor_outlet);

                if (FlowSensor_GetVolume(&sensor_outlet) >= active_sched.recipe.water_volume || isWtrLvl_Empty()) {
                    Actuator_SetState(ACT_PUMP_OUT, ACT_OFF);
                    Actuator_SetState(ACT_VALVE_TANK_OUT, ACT_OFF);
                    FlowSensor_Stop(&sensor_outlet);

                    /* Finalisasi Jadwal */
                    ScheduleManager_UpdateStatus(active_sched.type, active_sched.line_number, SCHED_STATUS_SUCCESS);
                    ScheduleManager_AutoReschedule(&active_sched);

                    LogManager_Write(LOG_INFO, "Siklus Fertigasi Selesai.");
                    currentState = STATE_SET_NEXT_ALARM;
                }
                break;

            /* --- FASE 4: USER INTERVENTION & FAIL-SAFE --- */
            case STATE_BT_INTERACTIVE:
                break;
            case STATE_SYNC_CONFIG:
                ScheduleManager_Init();
                currentState = STATE_EVALUATE_MISSED_SCHEDULE;
                break;
            case STATE_SENSOR_CALIBRATION:
                currentState = STATE_EVALUATE_MISSED_SCHEDULE;
                break;
            case STATE_FAULT:
                Actuator_Init();
                break;
            default:
                currentState = STATE_INIT_HARDWARE;
                break;
        }

        WaterQuality_ProcessAnalog();
    }
}

void APP_TaskCreate(UBaseType_t priority){
    appQueue = xQueueCreate(APP_QUEUE_SIZE, sizeof(CommandEvent_t *));
    if (appQueue != NULL) {
        xTaskCreate(vTaskApp, "AppTask", 1024, NULL, priority, &appTaskHandle);
    }
}
