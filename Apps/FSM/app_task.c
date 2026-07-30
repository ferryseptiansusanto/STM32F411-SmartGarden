/**
 * @file    app_task.c
 * @brief   Modul Task Utama Aplikasi (State Machine) untuk Smart Garden.
 * @details Mengelola FSM Irigasi Otomatis dan Sekuensial Pemupukan berbasis
 * Jadwal File SD Card dan sinkronisasi RTC (DS3231).
 * @author  ferry / Senior Embedded Systems Engineer
 * @date    22 Jul 2026
 */

#include <fatfs_wrapper.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <storage_wrapper.h>
#include "app_task.h"
// Include actuator, sensor
#include "actuator/actuator_driver.h"
#include "flowmeter/flowmeter_driver.h"
#include "water_lvl/water_lvl_driver.h"
#include "water_quality/water_quality_driver.h"
#include "temperature/temp_driver.h"
// Include device support
#include "ds3231_wrapper.h"
#include "eeprom_wrapper.h"
// Include management
#include "config_data.h"
#include "config_manager.h"
#include "recipe_manager.h"
#include "schedule_manager.h"

I2C_RTCDevice DS3231_Ctx;
I2C_EEPROMDevice Eeprom_Ctx;
SPI_StorageDevice SDCard_Ctx;

typedef struct {
	SPI_StorageDevice *logger;
} AppParams;

/* --- Definisi Alur Struktur State Machine --- */
typedef enum {
    IRR_STATE_IDLE,
    IRR_STATE_START,
    IRR_STATE_WATERING,
    IRR_STATE_DONE
} IrrigationState_t;

typedef enum {
    FERT_STATE_IDLE,
    FERT_STATE_ISI_AIR,
    FERT_STATE_ISI_PUPUK,  /**< Tahap Multi-Dosing Valve 1 hingga Valve 5 berbasis SD Card */
    FERT_STATE_MIXING,
    FERT_STATE_BUANG,
    FERT_STATE_DONE,
    FERT_STATE_SAFETY_ERR
} FertState_t;


/* --- Alokasi Objek Driver Sensor Global --- */
FlowSensor_t sensor_inlet;
FlowSensor_t sensor_outlet;
FlowSensor_t sensor_fert;

/* --- Eksternal Periferal dari Core Inisialisasi Perangkat Keras --- */
/* --- Eksternal Periferal dari Core Inisialisasi Perangkat Keras --- */
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim5;
extern TIM_HandleTypeDef htim9;

/* --- Variabel Konteks FreeRTOS Kernel --- */
TaskHandle_t appTaskHandle;
QueueHandle_t appQueue;
extern QueueHandle_t wtrLvlQueue;
QueueSetHandle_t appQueueSet;

/* --- Status Internal Mesin Status (FSM) --- */
static IrrigationState_t currentIrrState = IRR_STATE_IDLE;
static FertState_t currentFertState = FERT_STATE_IDLE;
static TickType_t mixing_start_tick = 0;

/* Konteks Data Resep & Pelacakan Jadwal Dinamis */
static FertRecipe_t activeRecipe;
static uint8_t current_fert_index = 0;
static int active_schedule_index = -1; /**< Menyimpan index jadwal yang sedang dieksekusi */

/* Mapping Index ke Actuator ID Aktuator Valve */
static const ActuatorType_t fert_valves[NUM_FERTILIZERS] = {
    ACT_VALVE_FERT_1, ACT_VALVE_FERT_2, ACT_VALVE_FERT_3, ACT_VALVE_FERT_4, ACT_VALVE_FERT_5
};

/* --- Loop Evaluasi Rutin Irigasi Rutin (Non-Blocking) --- */
void HandleIrrigationRoutine(void) {
    switch (currentIrrState) {
        case IRR_STATE_IDLE: break;
        case IRR_STATE_START:
            FlowSensor_ResetVolume(&sensor_outlet);
            Actuator_SetState(ACT_VALVE_WATER_IN, ACT_ON);
            Actuator_SetState(ACT_PUMP_OUT, ACT_ON);
            FlowSensor_Start(&sensor_outlet);
            currentIrrState = IRR_STATE_WATERING;
            break;
        case IRR_STATE_WATERING:
            if (FlowSensor_GetVolume(&sensor_outlet) >= TARGET_VOL_IRIGASI) {
                Actuator_SetState(ACT_VALVE_WATER_IN, ACT_OFF);
                Actuator_SetState(ACT_PUMP_OUT, ACT_OFF);
                FlowSensor_Stop(&sensor_outlet);
                currentIrrState = IRR_STATE_DONE;
            }
            break;
        case IRR_STATE_DONE:
            printf("[FSM IRR] Irigasi Rutin Selesai 100%%.\n");
            currentIrrState = IRR_STATE_IDLE;
            break;
        default: currentIrrState = IRR_STATE_IDLE; break;
    }
}

/**
 * @brief   Menangani FSM Pemupukan Multi-Dosing berbasis Load File SD Card secara Sekuensial.
 */
void HandleFertilizationRoutine(void) {
    WaterQualityData_t wq;

    switch (currentFertState) {
        case FERT_STATE_IDLE:
            break;

        case FERT_STATE_ISI_AIR:
            Actuator_SetState(ACT_VALVE_TANK_IN, ACT_ON);
            FlowSensor_Start(&sensor_inlet);

            if (FlowSensor_GetVolume(&sensor_inlet) >= TARGET_VOL_AIR_PENGENCER) {
                Actuator_SetState(ACT_VALVE_TANK_IN, ACT_OFF);
                FlowSensor_Stop(&sensor_inlet);

                /* Inisialisasi awal index sekuens multi-dosing pupuk */
                FlowSensor_ResetVolume(&sensor_fert);
                current_fert_index = 0;
                currentFertState = FERT_STATE_ISI_PUPUK;
            }
            break;

        case FERT_STATE_ISI_PUPUK:
            /* MENGAPA DIPERLUKAN: Melewati (auto-skip) pupuk yang di dalam file resep diset 0.0 Liter */
            while (current_fert_index < NUM_FERTILIZERS && activeRecipe.target_vol_liter[current_fert_index] <= 0.0f) {
                current_fert_index++;
            }

            /* Cek apakah 5 katup pupuk selesai dievaluasi */
            if (current_fert_index >= NUM_FERTILIZERS) {
                Actuator_SetState(ACT_PUMP_FERT, ACT_OFF);
                FlowSensor_Stop(&sensor_fert);

                mixing_start_tick = xTaskGetTickCount();
                currentFertState = FERT_STATE_MIXING;
                break;
            }

            /* Jalankan dosing untuk index pupuk yang aktif saat ini */
            ActuatorType_t active_valve = fert_valves[current_fert_index];
            Actuator_SetState(active_valve, ACT_ON);
            Actuator_SetState(ACT_PUMP_FERT, ACT_ON);
            FlowSensor_Start(&sensor_fert);

            if (FlowSensor_GetVolume(&sensor_fert) >= activeRecipe.target_vol_liter[current_fert_index] || isWtrLvl_Full()) {
                /* Amankan katup sebelum pindah ke jenis pupuk berikutnya */
                Actuator_SetState(active_valve, ACT_OFF);
                Actuator_SetState(ACT_PUMP_FERT, ACT_OFF);

                if (isWtrLvl_Full()) {
                    FlowSensor_Stop(&sensor_fert);
                    mixing_start_tick = xTaskGetTickCount();
                    currentFertState = FERT_STATE_MIXING;
                } else {
                    FlowSensor_ResetVolume(&sensor_fert);
                    current_fert_index++; /* Increment sub-state index */
                }
            }
            break;

        case FERT_STATE_MIXING:
            Actuator_SetState(ACT_MIXER, ACT_ON);
            wq = WaterQuality_GetData();

            if (wq.ec_val > TARGET_EC_MAX && !isWtrLvl_Full()) {
                Actuator_SetState(ACT_VALVE_TANK_IN, ACT_ON);
            } else {
                Actuator_SetState(ACT_VALVE_TANK_IN, ACT_OFF);
            }

            if ((xTaskGetTickCount() - mixing_start_tick) >= pdMS_TO_TICKS(MIXING_DURATION_MS)) {
                Actuator_SetState(ACT_MIXER, ACT_OFF);
                Actuator_SetState(ACT_VALVE_TANK_IN, ACT_OFF);
                FlowSensor_ResetVolume(&sensor_outlet);
                currentFertState = FERT_STATE_BUANG;
            }
            break;

        case FERT_STATE_BUANG:
            Actuator_SetState(ACT_VALVE_TANK_OUT, ACT_ON);
            Actuator_SetState(ACT_PUMP_OUT, ACT_ON);
            FlowSensor_Start(&sensor_outlet);

            if (FlowSensor_GetVolume(&sensor_outlet) >= TARGET_VOL_IRIGASI_FERT || isWtrLvl_Empty()) {
                Actuator_SetState(ACT_PUMP_OUT, ACT_OFF);
                Actuator_SetState(ACT_VALVE_TANK_OUT, ACT_OFF);
                FlowSensor_Stop(&sensor_outlet);
                currentFertState = FERT_STATE_DONE;
            }
            break;

        case FERT_STATE_DONE:
            /* MENGAPA CRITICAL: Memperbarui penanda status menjadi 'finish' di SD Card berkas schedule.txt
               agar jadwal ini tidak akan pernah tidak sengaja ter-trigger kembali di menit yang sama */
            if (active_schedule_index != -1) {
                Schedule_MarkAsFinish(active_schedule_index);
                active_schedule_index = -1; /* Reset tracker index */
            }
            currentFertState = FERT_STATE_IDLE;
            break;

        case FERT_STATE_SAFETY_ERR:
            Actuator_Init();
            active_schedule_index = -1;
            currentFertState = FERT_STATE_IDLE;
            break;

        default:
            currentFertState = FERT_STATE_IDLE;
            break;
    }
}

/**
 * @brief   Task Utama FreeRTOS Aplikasi
 */
static void vTaskApp(void *pvParameters) {
    (void)pvParameters;

    //AppParams *params = (AppParams*)pvParameters;
    //SPI_StorageDevice *Logger_Ctx = params->logger;
    CommandEvent evt;
    WtrLvl_Event_t wtrEvt;
    TickType_t last_rtc_check = 0;

    // ========================================================
    // TAMBAHKAN INISIALISASI DRIVER SENSOR, RTC, EEPROM, SDCARD  DI SINI
    // ========================================================
    // 1. Inisialisasi Sensor Kualitas Air (Mengaktifkan DMA ADC)
    WaterQuality_Init(&hadc1);
    // 2. Inisialisasi Sensor Suhu DS18B20 (Sesuaikan Port & Pin dengan CubeMX Anda)
    TempSensor_Init(TEMP_GPIO_Port, TEMP_Pin);
    // 3. Inisialisasi Flowmeter
    // 3. Inisialisasi Flowmeter
        /* MENGAPA DIPERBAIKI:
           - Menyertakan Logical ID (FLOW_SENSOR_INLET, dll) sesuai 5 parameter fungsi agar sistem kalibrasi berjalan tepat sasaran.
           - Menyesuaikan handle hardware (&htim5, &htim9, &htim2) agar selaras dengan skema Pinout Hardware di spesifikasi (PA1, PA2, PA15).
        */
        FlowSensor_Init(&sensor_inlet,  FLOW_SENSOR_INLET,  sys_calib.fm_inlet_pulse_per_liter,  &htim5, TIM_CHANNEL_2);
        FlowSensor_Init(&sensor_outlet, FLOW_SENSOR_OUTLET, sys_calib.fm_outlet_pulse_per_liter, &htim9, TIM_CHANNEL_1);
        FlowSensor_Init(&sensor_fert,   FLOW_SENSOR_FERT,   sys_calib.fm_fert_pulse_per_liter,   &htim2, TIM_CHANNEL_1);
    // 4. Inisialisasi Actuator
    Actuator_Init();
    // 5. Inisialisasi Water Level
    WtrLvl_Init();
    // 6. Initialize Support Device
    STORAGE_SetDeviceParameter(&SDCard_Ctx, &spi1_ctx, SPI1_CS_GPIO_Port, SPI1_CS_Pin, SPI_MODE_DMA); // Wajib setelah spi1_ctx diinitialize terlebih dahulu.
    LOG_Init("0:", 0, &SDCard_Ctx); // SDCard_Ctx wajib di set parameter
    DS3231_Init(&DS3231_Ctx, &i2c1_ctx);
    EEPROM_Init(0x57, &Eeprom_Ctx, &i2c1_ctx);

    // ========================================================

    /* Validasi dan Load Konfigurasi (Manajer) */
    ConfigManager_Init(&Eeprom_Ctx);

    /* --- INITIALIZE JADWAL FILE SD CARD --- */
    /* Mengisi list struktur RAM dari arsip penyimpanan SD Card saat booting pertama kali */
    Schedule_Init();

    appQueueSet = xQueueCreateSet(15);
    if (appQueueSet != NULL) {
        xQueueAddToSet(appQueue, appQueueSet);
        xQueueAddToSet(wtrLvlQueue, appQueueSet);
    }

    last_rtc_check = xTaskGetTickCount();

    for (;;) {
        QueueSetMemberHandle_t activatedQueue = xQueueSelectFromSet(appQueueSet, pdMS_TO_TICKS(10));

        if (activatedQueue == appQueue) {
            if (xQueueReceive(appQueue, &evt, 0)) {
                switch (evt.type) {
                    case EVENT_TYPE_TRIGGER_IRRIG:
                        if (currentIrrState == IRR_STATE_IDLE) currentIrrState = IRR_STATE_START;
                        break;
                    default: break;
                }
            }
        }
        else if (activatedQueue == wtrLvlQueue) {
            if (xQueueReceive(wtrLvlQueue, &wtrEvt, 0)) {
                if (wtrEvt.sensor == LVL_TANK_EMPTY && wtrEvt.is_reached) {
                    if (currentFertState == FERT_STATE_BUANG || currentFertState == FERT_STATE_MIXING) {
                         currentFertState = FERT_STATE_SAFETY_ERR;
                    }
                }
            }
        }

        /* --- SINKRONISASI JADWAL VIA RTC (Setiap 5 Detik Sekali) --- */
        /* MENGAPA 5 DETIK: Membaca chip RTC DS3231 via bus I2C terlalu sering di dalam loop cepat (10ms)
           akan menyita bandwidth bus komunikasi hardware. Interval 5 detik sangat optimal untuk mengecek menit jadwal */
        if ((xTaskGetTickCount() - last_rtc_check) >= pdMS_TO_TICKS(5000)) {
            last_rtc_check = xTaskGetTickCount();

            if (currentFertState == FERT_STATE_IDLE) {

            	DS3231_DateTime now = DS3231_GetDateTime(&DS3231_Ctx); /* Mengambil data jam, menit, tanggal aktual dari perangkat RTC */

                char triggered_recipe_name[MAX_RECIPE_NAME];
                int sch_idx = -1;

                /* Cek ke list jadwal RAM apakah ada yang cocok dengan menit ini */
                if (Schedule_CheckTrigger(now, triggered_recipe_name, &sch_idx)) {
                    /* Ambil file komposisi pupuk dari recipes.csv berdasarkan nama resep */
                    if (Recipe_Load(triggered_recipe_name, &activeRecipe)) {
                        active_schedule_index = sch_idx;
                        currentFertState = FERT_STATE_ISI_AIR; /* Pemicuan sukses, FSM Berjalan! */
                        printf("[APP] JADWAL MATCH! Menjalankan Resep: %s\n", activeRecipe.name);
                    } else {
                        printf("[APP] ERROR: Jadwal cocok ke %s, tapi isi resep gagal dimuat dari SD Card!\n", triggered_recipe_name);
                    }
                }
            }
        }

        /* --- Eksekusi Driver & Evaluasi State Machine --- */
        WaterQuality_ProcessAnalog();
        FlowSensor_Read(&sensor_inlet);
        FlowSensor_Read(&sensor_outlet);
        FlowSensor_Read(&sensor_fert);

        HandleIrrigationRoutine();
        HandleFertilizationRoutine();

    }
}

void APP_TaskCreate(UBaseType_t priority){
    appQueue = xQueueCreate(10, sizeof(CommandEvent));
    if (appQueue != NULL) {
        xTaskCreate(vTaskApp, "AppTask", 1024, NULL, priority, &appTaskHandle);
    }
}
