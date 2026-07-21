/*
 * app_task.c
 *
 * Created on: 3 Jul 2026
 * Author: ferry
 * Refactored: Integrasi QueueSet, Urutan Booting EEPROM, dan Hapus Race Condition.
 */

#include "app_task.h"
#include "valve/valve_driver.h"
#include "mixer/mixer_driver.h"
#include "flowmeter/flowmeter_driver.h"
#include "water_lvl/water_lvl_driver.h"
#include "water_quality/water_quality_driver.h"
#include "eeprom_wrapper.h"
#include "config_manager.h"
#include "config_data.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>

// --- Abstraksi Perangkat Output Tambahan (Non-Valve) ---
#define PUMP_OUT_ON()    HAL_GPIO_WritePin(GPIOB, PUMP_OUT_Pin, GPIO_PIN_SET)
#define PUMP_OUT_OFF()   HAL_GPIO_WritePin(GPIOB, PUMP_OUT_Pin, GPIO_PIN_RESET)
#define PUMP_FERT_ON()   HAL_GPIO_WritePin(GPIOB, PUMP_FERT_Pin, GPIO_PIN_SET)
#define PUMP_FERT_OFF()  HAL_GPIO_WritePin(GPIOB, PUMP_FERT_Pin, GPIO_PIN_RESET)
#define MIXER_ON()       HAL_GPIO_WritePin(GPIOB, MIXER_Pin, GPIO_PIN_SET)
#define MIXER_OFF()      HAL_GPIO_WritePin(GPIOB, MIXER_Pin, GPIO_PIN_RESET)

// --- Definisi Alur Struktur State Machine ---
typedef enum {
    IRR_STATE_IDLE,
    IRR_STATE_START,
    IRR_STATE_WATERING,
    IRR_STATE_DONE
} IrrigationState_t;

typedef enum {
    FERT_STATE_IDLE,
    FERT_STATE_ISI_AIR,
    FERT_STATE_ISI_PUPUK,
    FERT_STATE_MIXING,
    FERT_STATE_BUANG,
    FERT_STATE_DONE,
    FERT_STATE_SAFETY_ERR
} FertState_t;

// --- Alokasi Objek Driver Sensor Global ---
FlowSensor_t sensor_inlet;
FlowSensor_t sensor_outlet;
FlowSensor_t sensor_fert;

extern TIM_HandleTypeDef htim2;
extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c1;   // Pointer ke I2C untuk EEPROM

// --- Variabel Konteks RTOS ---
TaskHandle_t appTaskHandle;
QueueHandle_t appQueue;
extern QueueHandle_t wtrLvlQueue; // Diambil dari water_lvl_driver.c
QueueSetHandle_t appQueueSet;     // Untuk mendengarkan banyak Queue sekaligus

static IrrigationState_t currentIrrState = IRR_STATE_IDLE;
static FertState_t currentFertState = FERT_STATE_IDLE;
static TickType_t mixing_start_tick = 0;

// ==================================================================
// 1. STATE MACHINE: LOGIKA IRIGASI RUTIN (NON-BLOCKING)
// ==================================================================
void HandleIrrigationRoutine(void) {
    switch (currentIrrState) {
        case IRR_STATE_IDLE:
            break;

        case IRR_STATE_START:
            FlowSensor_ResetVolume(&sensor_outlet);
            Valve_Open(VALVE_WATER_IN);
            PUMP_OUT_ON();
            FlowSensor_Start(&sensor_outlet);
            currentIrrState = IRR_STATE_WATERING;
            break;

        case IRR_STATE_WATERING:
            // Pantau terus volume outlet hingga target tercapai
            if (FlowSensor_GetVolume(&sensor_outlet) >= TARGET_VOL_IRIGASI) {
                Valve_Close(VALVE_WATER_IN);
                PUMP_OUT_OFF();
                FlowSensor_Stop(&sensor_outlet);
                currentIrrState = IRR_STATE_DONE;
            }
            break;

        case IRR_STATE_DONE:
            printf("[FSM] Irigasi Rutin Selesai 100%.\n");
            currentIrrState = IRR_STATE_IDLE;
            break;
    }
}

// ==================================================================
// 2. STATE MACHINE: LOGIKA PEMUPUKAN 4 TAHAP + SAFETY
// ==================================================================
void HandleFertilizationRoutine(void) {
    WaterQualityData_t wq;

    switch (currentFertState) {
        case FERT_STATE_IDLE:
            break;

        case FERT_STATE_ISI_AIR: // --- TAHAP 1: Pengisian Air Pengencer ---
            Valve_Open(VALVE_TANK_IN);
            FlowSensor_Start(&sensor_inlet);

            if (FlowSensor_GetVolume(&sensor_inlet) >= TARGET_VOL_AIR_PENGENCER) {
                Valve_Close(VALVE_TANK_IN);
                FlowSensor_Stop(&sensor_inlet);

                FlowSensor_ResetVolume(&sensor_fert);
                currentFertState = FERT_STATE_ISI_PUPUK;
            }
            break;

        case FERT_STATE_ISI_PUPUK: // --- TAHAP 2: Pengisian Pupuk Komponen ---
            Valve_Open(VALVE_PUPUK_1);
            PUMP_FERT_ON();
            FlowSensor_Start(&sensor_fert);

            // SAFETY INTERLOCK: Overflow prevention
            if (FlowSensor_GetVolume(&sensor_fert) >= TARGET_VOL_PUPUK || isWtrLvl_Full()) {
                Valve_Close(VALVE_PUPUK_1);
                PUMP_FERT_OFF();
                FlowSensor_Stop(&sensor_fert);

                mixing_start_tick = xTaskGetTickCount();
                currentFertState = FERT_STATE_MIXING;
            }
            break;

        case FERT_STATE_MIXING: // --- TAHAP 3: Pengadukan & Koreksi Parameter ---
            MIXER_ON();
            wq = WaterQuality_GetData();

            if (wq.ec_val > TARGET_EC_MAX && !isWtrLvl_Full()) {
                Valve_Open(VALVE_TANK_IN);
            } else {
                Valve_Close(VALVE_TANK_IN);
            }

            if ((xTaskGetTickCount() - mixing_start_tick) >= pdMS_TO_TICKS(MIXING_DURATION_MS)) {
                MIXER_OFF();
                Valve_Close(VALVE_TANK_IN);

                FlowSensor_ResetVolume(&sensor_outlet);
                currentFertState = FERT_STATE_BUANG;
            }
            break;

        case FERT_STATE_BUANG: // --- TAHAP 4: Pembuangan ke Distribusi Irigasi ---
            PUMP_OUT_ON();
            FlowSensor_Start(&sensor_outlet);

            if (FlowSensor_GetVolume(&sensor_outlet) >= TARGET_VOL_IRIGASI_FERT || isWtrLvl_Empty()) {
                PUMP_OUT_OFF();
                FlowSensor_Stop(&sensor_outlet);
                currentFertState = FERT_STATE_DONE;
            }
            break;

        case FERT_STATE_DONE:
            printf("[FSM] Sekuensial Pemupukan 4 Tahap Sukses.\n");
            currentFertState = FERT_STATE_IDLE;
            break;

        case FERT_STATE_SAFETY_ERR:
            Valve_Init();
            PUMP_OUT_OFF();
            PUMP_FERT_OFF();
            MIXER_OFF();
            currentFertState = FERT_STATE_IDLE;
            break;
    }
}

// ==================================================================
// 3. TASK UTAMA KONTROL SISTEM FreeRTOS
// ==================================================================
static void vTaskApp(void *pvParameters) {
    CommandEvent evt;
    WtrLvl_Event_t wtrEvt;

    // --- 1. Inisialisasi Seluruh Lapisan Hardware & Driver ---
    Valve_Init();
    WtrLvl_Init();
    WaterQuality_Init(&hadc1);

    // --- 2. Load Data EEPROM MENCEGAH DIVIDE BY ZERO ---
    EEPROM_Init(0x57, &EEPROM_Ctx, &hi2c1);
    ConfigManager_Init(&EEPROM_Ctx);

    // --- 3. Mendaftarkan sensor dengan Data Kalibrasi yang aman (sys_calib) ---
    FlowSensor_Init(&sensor_inlet, sys_calib.fm_inlet_pulse_per_liter, &htim2, TIM_CHANNEL_1);
    FlowSensor_Init(&sensor_outlet, sys_calib.fm_outlet_pulse_per_liter, &htim2, TIM_CHANNEL_2);
    FlowSensor_Init(&sensor_fert, sys_calib.fm_fert_pulse_per_liter, &htim2, TIM_CHANNEL_3);

    printf("[APP] Smart Garden OS Awal Booting Berhasil.\n");

    // --- 4. Setup Queue Set (Mendengarkan Perintah Command & Interupsi Sensor Air) ---
    appQueueSet = xQueueCreateSet(15);
    xQueueAddToSet(appQueue, appQueueSet);
    xQueueAddToSet(wtrLvlQueue, appQueueSet);

    for (;;) {
        // Blokir task maksimal 50ms (Lebih hemat CPU ketimbang delay biasa)
        QueueSetMemberHandle_t activatedQueue = xQueueSelectFromSet(appQueueSet, pdMS_TO_TICKS(50));

        if (activatedQueue == appQueue) {
            // Event dari komunikasi luar (UART/Bluetooth)
            if (xQueueReceive(appQueue, &evt, 0)) {
                switch (evt.type) {
                    case EVENT_TYPE_TRIGGER_IRRIG:
                        if (currentIrrState == IRR_STATE_IDLE) currentIrrState = IRR_STATE_START;
                        break;
                    case EVENT_TYPE_TRIGGER_FERT:
                        if (currentFertState == FERT_STATE_IDLE) currentFertState = FERT_STATE_ISI_AIR;
                        break;
                    default:
                        break;
                }
            }
        }
        else if (activatedQueue == wtrLvlQueue) {
            // Event Interupsi Water Level
            if (xQueueReceive(wtrLvlQueue, &wtrEvt, 0)) {
                printf("[APP] Tangki Alert: Sensor %d berubah status = %d\n", wtrEvt.sensor, wtrEvt.is_reached);
                // Anda bisa menyambungkan ke currentFertState = FERT_STATE_SAFETY_ERR; jika perlu
            }
        }

        // --- 5. Evaluasi Nilai Sinkronisasi DMA (Non-Blocking) ---
        WaterQuality_ProcessAnalog();

        // --- 6. Evaluasi Mesin Status FSM Utama ---
        HandleIrrigationRoutine();
        HandleFertilizationRoutine();
    }
}

void APP_TaskCreate(UBaseType_t priority) {
    appQueue = xQueueCreate(10, sizeof(CommandEvent));
    // Kita menampung TaskHandle_t di appTaskHandle agar bisa dipanggil notifikasinya dari driver lain
    xTaskCreate(vTaskApp, "AppTask", 1024, NULL, priority, &appTaskHandle);
}
