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
#include "app_config.h"         /* Pusat konfigurasi makro FSM */
#include "command_event.h"      /* Kontrak struktur data Zero-Copy */

#include "actuator_driver.h"
#include "flowmeter_driver.h"
#include "water_lvl_driver.h"
#include "water_quality_driver.h"
#include "config_manager.h"
#include "schedule_manager.h"
#include "log_manager.h"

/* --- Konteks RTOS --- */
TaskHandle_t appTaskHandle;
QueueHandle_t appQueue;
extern QueueHandle_t wtrLvlQueue; /* Antrean dari interupsi Water Level EXTI */
QueueSetHandle_t appQueueSet;

/* --- FSM State Tracker --- */
static AppFSMState_t currentState = STATE_INIT_HARDWARE;

/* --- Variabel Pelacakan Jadwal & Resep --- */
static ScheduleInfo_t active_schedule;
static FertRecipe_t active_recipe;
static uint8_t current_fert_index = 0;
static TickType_t mixing_start_tick = 0;

/* Pemetaan array aktuator pupuk O(1) */
static const ActuatorType_t fert_valves[5] = {
    ACT_VALVE_FERT1, ACT_VALVE_FERT2, ACT_VALVE_FERT3, ACT_VALVE_FERT4, ACT_VALVE_FERT5
};

/* --- Referensi Eksternal --- */
extern FlowSensor_t sensor_fert;
extern FlowSensor_t sensor_inlet;
extern FlowSensor_t sensor_outlet;
extern SystemConfig_t sys_calib; // Dari config_manager

/**
 * @brief Task FreeRTOS Utama untuk FSM
 */
static void vTaskApp(void *pvParameters) {
    (void)pvParameters;

    CommandEvent_t *evt_ptr = NULL;
    WtrLvl_Event_t wtrEvt;
    WaterQualityData_t wq;

    /* Inisialisasi QueueSet: Menyatukan 2 pintu event (Komunikasi & Sensor Level) */
    appQueueSet = xQueueCreateSet(APP_QUEUE_SET_SIZE);
    if (appQueueSet != NULL) {
        xQueueAddToSet(appQueue, appQueueSet);
        xQueueAddToSet(wtrLvlQueue, appQueueSet);
    }

    for (;;) {
        /* ====================================================================
         * 1. PENDENGAR EVENT NON-BLOCKING (TICK LOOP 10ms)
         * ==================================================================== */
        QueueSetMemberHandle_t activatedQueue = xQueueSelectFromSet(appQueueSet, pdMS_TO_TICKS(10));

        if (activatedQueue == appQueue) {
            /* PENERAPAN ZERO-COPY MEMORY: Terima alamat pointer, baca, lalu BEBASKAN! */
            if (xQueueReceive(appQueue, &evt_ptr, 0) == pdPASS && evt_ptr != NULL) {

                /* Interupsi Perintah Eksternal (Bluetooth/UART) */
                switch (evt_ptr->cmd_id) {
                    case CMD_EMERGENCY_STOP:
                        currentState = STATE_FAULT;
                        LogManager_WriteErrorLog("ERR_EMERGENCY", "Emergency Stop Ditekan via Bluetooth!");
                        break;
                    case CMD_START_CALIBRATION:
                        currentState = STATE_SENSOR_CALIBRATION;
                        break;
                    case CMD_SYNC_CONFIG:
                        currentState = STATE_SYNC_CONFIG;
                        break;
                    // (Perintah lainnya...)
                    default: break;
                }

                /* ATURAN EMAS: Hapus alokasi string internal jika ada, lalu hapus struct-nya */
                if (evt_ptr->payload.str_ptr != NULL) {
                    vPortFree(evt_ptr->payload.str_ptr);
                }
                vPortFree(evt_ptr); /* Wajib! Menangkal Memory Leak & HardFault */
            }
        }
        else if (activatedQueue == wtrLvlQueue) {
            /* Event Darurat dari Sensor Level Air */
            if (xQueueReceive(wtrLvlQueue, &wtrEvt, 0)) {
                if (wtrEvt.sensor == LVL_TANK_FULL && wtrEvt.is_reached && currentState == STATE_DOSING) {
                    /* FAIL-SAFE: Jika tangki mau luber saat isi pupuk, langsung lompat ke Mixing */
                    LogManager_WriteSystemLog("WARN: Tangki Penuh Terdeteksi saat Dosing. Memaksa Mixing.");
                    currentState = STATE_MIXING;
                }
            }
        }

        /* ====================================================================
         * 2. CENTRALIZED FINITE STATE MACHINE (15 STATE FINAL V3.6)
         * ==================================================================== */
        switch (currentState) {

            /* --- FASE 1: BOOTING & INITIALIZATION --- */
            case STATE_INIT_HARDWARE:
                Actuator_Init(); /* FAIL-SAFE: Force LOW semua relay */
                WtrLvl_Init();
                currentState = STATE_LOAD_CALIBRATION;
                break;

            case STATE_LOAD_CALIBRATION:
                ConfigManager_Init(); /* Tarik data sys_calib dari EEPROM I2C */
                currentState = STATE_LOAD_SCHEDULE;
                break;

            case STATE_LOAD_SCHEDULE:
                ScheduleManager_Init(); /* Tarik file txt jadwal dari SD Card ke RAM */
                currentState = STATE_EVALUATE_MISSED_SCHEDULE; /* Cek apakah ada jadwal terlewat pasca mati listrik */
                break;

            /* --- FASE 2: STANDBY & ROUTING --- */
            case STATE_SET_NEXT_ALARM:
                /* Setel Register RTC DS3231 untuk alarm berikutnya, lalu tidur */
                // DS3231_SetAlarm(...);
                currentState = STATE_SLEEP;
                break;

            case STATE_SLEEP:
                /* Mode Hemat Daya (HAL_PWR_EnterSTOPMode). FSM diam di sini sampai RTC EXTI
                   atau Bluetooth membangunkannya. Simulasi Wake Up: */
                // if (WakeUpEvent_Detected) currentState = STATE_EVALUATE_MISSED_SCHEDULE;
                break;

            /* --- ROUTER UTAMA: EVALUASI JADWAL TERDEKAT --- */
            case STATE_EVALUATE_MISSED_SCHEDULE:
                if (Schedule_GetNextPending(&active_schedule) == pdTRUE) {

                    if (active_schedule.type == SCHED_TYPE_WATER_ONLY) {
                        /* JALUR A: Irigasi Air Baku Langsung */
                        FlowSensor_ResetVolume(&sensor_inlet);
                        LogManager_WriteSystemLog("Mengeksekusi Irigasi Murni.");
                        currentState = STATE_IRRIGATING;
                    }
                    else if (active_schedule.type == SCHED_TYPE_FERTILIZER) {
                        /* JALUR B: Fertigasi (Pemupukan) */
                        active_recipe = RecipeManager_GetRecipe(active_schedule.recipe_id);
                        current_fert_index = 0;
                        LogManager_WriteSystemLog("Mengeksekusi Sekuens Fertigasi.");
                        currentState = STATE_PRE_FLUSHING;
                    }
                } else {
                    currentState = STATE_SET_NEXT_ALARM; /* Tidak ada tugas, kembali standby */
                }
                break;

            /* ================================================================
             * JALUR A: IRIGASI MURNI (TANPA PENGADUKAN / LANGSUNG KE KEBUN)
             * ================================================================ */
            case STATE_IRRIGATING:
                Actuator_SetState(ACT_VALVE_WATER_IN, ACT_ON);
                Actuator_SetState(ACT_PUMP_OUT, ACT_ON);
                FlowSensor_Start(&sensor_inlet);

                /* Pengecekan Non-Blocking Volume via Hardware Timer (Setiap 10ms dari Tick Loop FSM) */
                if (FlowSensor_GetVolume(&sensor_inlet) >= active_schedule.target_vol_ml) {
                    Actuator_SetState(ACT_VALVE_WATER_IN, ACT_OFF);
                    Actuator_SetState(ACT_PUMP_OUT, ACT_OFF);
                    FlowSensor_Stop(&sensor_inlet);

                    Schedule_MarkAsFinish(active_schedule.id);
                    currentState = STATE_SET_NEXT_ALARM;
                }
                break;

            /* ================================================================
             * JALUR B: FERTIGASI (PERACIKAN PUPUK & DISTRIBUSI)
             * ================================================================ */
            case STATE_PRE_FLUSHING:
                /* (Opsional) Menguras sisa larutan lama dari tangki.
                   Lompat langsung ke Dosing jika tangki sudah kosong */
                if (isWtrLvl_Empty()) {
                    FlowSensor_ResetVolume(&sensor_fert);
                    currentState = STATE_DOSING;
                } else {
                    Actuator_SetState(ACT_VALVE_TANK_OUT, ACT_ON);
                    Actuator_SetState(ACT_PUMP_OUT, ACT_ON);
                }
                break;

            case STATE_DOSING:
                /* 1. Bypass otomatis jika target mililiter pupuk adalah 0 */
                while (current_fert_index < NUM_FERTILIZERS && active_recipe.target_vol_ml[current_fert_index] <= 0) {
                    current_fert_index++;
                }

                /* 2. Semua pupuk selesai dimasukkan? Transisi ke Pengadukan (Mixing) */
                if (current_fert_index >= NUM_FERTILIZERS) {
                    Actuator_SetState(ACT_PUMP_FERT, ACT_OFF);
                    FlowSensor_Stop(&sensor_fert);

                    mixing_start_tick = xTaskGetTickCount(); /* Kunci waktu untuk delay non-blocking */
                    currentState = STATE_MIXING;
                    break;
                }

                /* 3. Masukkan pupuk sesuai index yang berjalan */
                ActuatorType_t active_valve = fert_valves[current_fert_index];
                Actuator_SetState(active_valve, ACT_ON);
                Actuator_SetState(ACT_PUMP_FERT, ACT_ON);
                FlowSensor_Start(&sensor_fert);

                /* 4. Evaluasi tercapainya target. Interupsi Tangki Penuh dipantau di QueueSet atas */
                if (FlowSensor_GetVolume(&sensor_fert) >= active_recipe.target_vol_ml[current_fert_index]) {
                    Actuator_SetState(active_valve, ACT_OFF);
                    Actuator_SetState(ACT_PUMP_FERT, ACT_OFF);
                    FlowSensor_ResetVolume(&sensor_fert);
                    current_fert_index++; /* Lanjut ke botol pupuk berikutnya di tick OS selanjutnya */
                }
                break;

            case STATE_MIXING:
                Actuator_SetState(ACT_MIXER, ACT_ON);
                wq = WaterQuality_GetData(); /* Diperbarui otomatis oleh DMA di background */

                /* Opsional: Buka keran air baku untuk menurunkan kepekatan jika EC terlalu tinggi */
                if (wq.ec_val > sys_calib.max_ec_limit && !isWtrLvl_Full()) {
                    Actuator_SetState(ACT_VALVE_TANK_IN, ACT_ON);
                } else {
                    Actuator_SetState(ACT_VALVE_TANK_IN, ACT_OFF);
                }

                /* Timer Pengadukan Non-Blocking */
                if ((xTaskGetTickCount() - mixing_start_tick) >= pdMS_TO_TICKS(active_recipe.mixing_duration_ms)) {
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

                /* Distribusikan hingga volume tercapai ATAU tangki pencampur terkuras habis */
                if (FlowSensor_GetVolume(&sensor_outlet) >= active_schedule.target_vol_ml || isWtrLvl_Empty()) {
                    Actuator_SetState(ACT_PUMP_OUT, ACT_OFF);
                    Actuator_SetState(ACT_VALVE_TANK_OUT, ACT_OFF);
                    FlowSensor_Stop(&sensor_outlet);

                    Schedule_MarkAsFinish(active_schedule.id); /* Selesai, Update SD Card */
                    currentState = STATE_SET_NEXT_ALARM;
                }
                break;

            /* --- FASE 4: USER INTERVENTION & FAIL-SAFE --- */
            case STATE_BT_INTERACTIVE:
                /* Mode siaga mencegah sistem masuk deep sleep selama aplikasi HP terhubung */
                break;

            case STATE_SYNC_CONFIG:
                /* Eksekusi penulisan jadwal baru dari Bluetooth ke SD Card.
                   Setelah selesai, otomatis kembali ke evaluasi awal. */
                ScheduleManager_Init();
                currentState = STATE_EVALUATE_MISSED_SCHEDULE;
                break;

            case STATE_SENSOR_CALIBRATION:
                /* Mengkalibrasi ulang batas atas dan kemiringan (slope) sensor. Menulis ke EEPROM. */
                currentState = STATE_EVALUATE_MISSED_SCHEDULE;
                break;

            case STATE_FAULT:
                /* MENGAPA: Hard Stop seketika! Semua penggerak fisik diputus arusnya.
                   Mencegah motor terbakar atau kebun kebanjiran. */
                Actuator_Init();
                /* Berdiam di sini sampai command CMD_CLEAR_ALARM masuk dari Bluetooth */
                break;

            default:
                currentState = STATE_INIT_HARDWARE;
                break;
        }

        /* Pembaruan Kalkulasi DMA Background (Tidak membebani CPU) */
        WaterQuality_ProcessAnalog();
    }
}

/**
 * @brief Membangun FSM Task
 */
void APP_TaskCreate(UBaseType_t priority){
    /* ATURAN EMAS: Harus ukuran pointer! Pass-by-Pointer menghindari Memory Leak */
    appQueue = xQueueCreate(APP_QUEUE_SIZE, sizeof(CommandEvent_t *));

    if (appQueue != NULL) {
        xTaskCreate(vTaskApp, "AppTask", 1024, NULL, priority, &appTaskHandle);
    }
}
