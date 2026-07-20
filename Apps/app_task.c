/*
 * app_task.c
 *
 * Created on: 3 Jul 2026
 * Author: ferry
 */

#include "app_task.h"
#include "valve/valve_driver.h"
#include "mixer/mixer_driver.h"
#include "flowmeter/flowmeter_driver.h"
#include "water_lvl/water_lvl_driver.h"
#include "water_quality/water_quality_driver.h"
#include "FreeRTOS.h"
#include "task.h"
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

QueueHandle_t appQueue;
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

                // Siapkan transisi ke tahap berikutnya
                FlowSensor_ResetVolume(&sensor_fert);
                currentFertState = FERT_STATE_ISI_PUPUK;
            }
            break;

        case FERT_STATE_ISI_PUPUK: // --- TAHAP 2: Pengisian Pupuk Komponen ---
            Valve_Open(VALVE_PUPUK_1);    // Pilih jenis pupuk sesuai jadwal
            PUMP_FERT_ON();
            FlowSensor_Start(&sensor_fert);

            // SAFETY INTERLOCK: Berhenti jika volume terpenuhi ATAU sensor penuh aktif (overflow prevention)
            if (FlowSensor_GetVolume(&sensor_fert) >= TARGET_VOL_PUPUK || isWtrLvl_Full()) {
                Valve_Close(VALVE_PUPUK_1);
                PUMP_FERT_OFF();
                FlowSensor_Stop(&sensor_fert);

                mixing_start_tick = xTaskGetTickCount(); // Catat waktu awal mixing
                currentFertState = FERT_STATE_MIXING;
            }
            break;

        case FERT_STATE_MIXING: // --- TAHAP 3: Pengadukan & Koreksi Parameter ---
            MIXER_ON();
            wq = WaterQuality_GetData();

            // Logika Koreksi Dinamis: Jika larutan terlalu pekat, encerkan kembali dengan air bersih
            if (wq.ec_val > TARGET_EC_MAX && !isWtrLvl_Full()) {
                Valve_Open(VALVE_TANK_IN);
            } else {
                Valve_Close(VALVE_TANK_IN);
            }

            // Validasi Software Timer Waktu Aduk Non-Blocking
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

            // CRITICAL SAFETY: Matikan pompa jika volume target tercapai ATAU tangki sudah kosong kering
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
            // Mode Proteksi Kegagalan: Amankan semua aktuator ke posisi mati
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

    // --- Inisialisasi Seluruh Lapisan Driver Periferal ---
    Valve_Init();
    WtrLvl_Init();
    WaterQuality_Init(&hadc1);

    // Mendaftarkan pin hardware ke tabel pemetaan O(1) Input Capture Timer 2
    FlowSensor_Init(&sensor_inlet, 450, &htim2, TIM_CHANNEL_1);
    FlowSensor_Init(&sensor_outlet, 450, &htim2, TIM_CHANNEL_2);
    FlowSensor_Init(&sensor_fert, 450, &htim2, TIM_CHANNEL_3);

    printf("[APP] Smart Garden OS Awal Booting Berhasil.\n");

    for (;;) {
        // Blokir task maksimal selama 50ms untuk mendengarkan event Queue
        if (xQueueReceive(appQueue, &evt, pdMS_TO_TICKS(50))) {
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

        // --- Sinkronisasi Nilai Sensor Berkelanjutan (Non-Blocking) ---
        FlowSensor_Read(&sensor_inlet);
        FlowSensor_Read(&sensor_outlet);
        FlowSensor_Read(&sensor_fert);
        WaterQuality_ProcessAnalog();

        // --- Siklus FSM Utama yang Dievaluasi Tiap 50ms ---
        HandleIrrigationRoutine();
        HandleFertilizationRoutine();
    }
}

void APP_TaskCreate(UBaseType_t priority) {
    appQueue = xQueueCreate(10, sizeof(CommandEvent));
    xTaskCreate(vTaskApp, "AppTask", 512, NULL, priority, NULL);
}
