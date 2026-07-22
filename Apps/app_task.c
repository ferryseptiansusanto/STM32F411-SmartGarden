/**
 * @file    app_task.c
 * @brief   Modul Task Utama Aplikasi (State Machine) untuk Smart Garden
 * @author  ferry
 * @date    3 Jul 2026
 * @note    Refactored: Perbaikan Integrasi Non-Blocking QueueSet, Task Notification ADC,
 * serta Pembacaan Flowmeter Berkelanjutan.
 */

#include "app_task.h"
#include "actuator/actuator_driver.h"
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

// --- Variabel Konteks RTOS ---
TaskHandle_t appTaskHandle;
QueueHandle_t appQueue;
extern QueueHandle_t wtrLvlQueue; // Diambil dari water_lvl_driver.c
QueueSetHandle_t appQueueSet;     // Untuk mendengarkan banyak Queue sekaligus

static IrrigationState_t currentIrrState = IRR_STATE_IDLE;
static FertState_t currentFertState = FERT_STATE_IDLE;
static TickType_t mixing_start_tick = 0;

/**
 * @brief   Menangani FSM (Finite State Machine) untuk proses irigasi rutin.
 * @details Mengeksekusi penyiraman berdasarkan target volume air (TARGET_VOL_IRIGASI).
 * Berjalan secara non-blocking sehingga tugas lain di RTOS tidak terganggu.
 */
void HandleIrrigationRoutine(void) {
    switch (currentIrrState) {
        case IRR_STATE_IDLE:
            break;

        case IRR_STATE_START:
            // Persiapan irigasi: reset perhitungan volume dan nyalakan aktuator
            FlowSensor_ResetVolume(&sensor_outlet);
            Actuator_SetState(ACT_VALVE_WATER_IN, ACT_ON);
            Actuator_SetState(ACT_PUMP_OUT, ACT_ON);
            FlowSensor_Start(&sensor_outlet);
            currentIrrState = IRR_STATE_WATERING;
            break;

        case IRR_STATE_WATERING:
            // MENGAPA: Evaluasi volume dilakukan terus menerus di state ini.
            // Karena fungsi pembaca flowmeter dipanggil di luar switch-case ini, nilai volume akan selalu terupdate.
            if (FlowSensor_GetVolume(&sensor_outlet) >= TARGET_VOL_IRIGASI) {
                Actuator_SetState(ACT_VALVE_WATER_IN, ACT_OFF);
                Actuator_SetState(ACT_PUMP_OUT, ACT_OFF);
                FlowSensor_Stop(&sensor_outlet);
                currentIrrState = IRR_STATE_DONE;
            }
            break;

        case IRR_STATE_DONE:
            printf("[FSM] Irigasi Rutin Selesai 100%%.\n");
            currentIrrState = IRR_STATE_IDLE;
            break;
    }
}

/**
 * @brief   Menangani FSM Pemupukan 4 Tahap beserta proteksi keamanannya.
 * @details Sekuensial pencampuran pupuk: Isi Air -> Isi Pupuk -> Mixing & Koreksi -> Pembuangan.
 */
void HandleFertilizationRoutine(void) {
    WaterQualityData_t wq;

    switch (currentFertState) {
        case FERT_STATE_IDLE:
            break;

        case FERT_STATE_ISI_AIR: // --- TAHAP 1: Pengisian Air Pengencer ---
        	Actuator_SetState(ACT_VALVE_TANK_IN, ACT_ON);
            FlowSensor_Start(&sensor_inlet);

            if (FlowSensor_GetVolume(&sensor_inlet) >= TARGET_VOL_AIR_PENGENCER) {
            	Actuator_SetState(ACT_VALVE_TANK_IN, ACT_OFF);
                FlowSensor_Stop(&sensor_inlet);

                FlowSensor_ResetVolume(&sensor_fert);
                currentFertState = FERT_STATE_ISI_PUPUK;
            }
            break;

        case FERT_STATE_ISI_PUPUK: // --- TAHAP 2: Pengisian Pupuk Komponen ---
        	Actuator_SetState(ACT_VALVE_FERT_1, ACT_ON);
        	Actuator_SetState(ACT_PUMP_FERT, ACT_ON);
            FlowSensor_Start(&sensor_fert);

            // MENGAPA SAFETY INTERLOCK: Mencegah pupuk meluber jika sensor flowmeter rusak
            // atau target volume melebihi kapasitas fisik tangki.
            if (FlowSensor_GetVolume(&sensor_fert) >= TARGET_VOL_PUPUK || isWtrLvl_Full()) {
            	Actuator_SetState(ACT_VALVE_FERT_1, ACT_OFF);
            	Actuator_SetState(ACT_PUMP_FERT, ACT_OFF);
                FlowSensor_Stop(&sensor_fert);

                mixing_start_tick = xTaskGetTickCount();
                currentFertState = FERT_STATE_MIXING;
            }
            break;

        case FERT_STATE_MIXING: // --- TAHAP 3: Pengadukan & Koreksi Parameter Kimia ---
            Actuator_SetState(ACT_MIXER, ACT_ON);
            wq = WaterQuality_GetData();

            // Koreksi EC dengan menambahkan air tawar jika kepekatan berlebih
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

        case FERT_STATE_BUANG: // --- TAHAP 4: Pembuangan ke Distribusi Irigasi ---
            Actuator_SetState(ACT_PUMP_OUT, ACT_ON);
            FlowSensor_Start(&sensor_outlet);

            // Berhenti membuang jika target tercapai ATAU jika tangki sudah benar-benar kosong (proteksi pompa kering)
            if (FlowSensor_GetVolume(&sensor_outlet) >= TARGET_VOL_IRIGASI_FERT || isWtrLvl_Empty()) {
            	Actuator_SetState(ACT_PUMP_OUT, ACT_OFF);
                FlowSensor_Stop(&sensor_outlet);
                currentFertState = FERT_STATE_DONE;
            }
            break;

        case FERT_STATE_DONE:
            printf("[FSM] Sekuensial Pemupukan 4 Tahap Sukses.\n");
            currentFertState = FERT_STATE_IDLE;
            break;

        case FERT_STATE_SAFETY_ERR:
            // Fail-safe mode: Matikan paksa seluruh perangkat
        	Actuator_Init();
            currentFertState = FERT_STATE_IDLE;
            break;
    }
}

/**
 * @brief   Task RTOS Utama (Otak dari Smart Garden).
 * @param   pvParameters Parameter opsional bawaan FreeRTOS (tidak digunakan).
 */
static void vTaskApp(void *pvParameters) {
    CommandEvent evt;
    WtrLvl_Event_t wtrEvt;

    // --- 1. Inisialisasi Seluruh Lapisan Hardware & Driver ---
    Actuator_Init();
    WtrLvl_Init();
    WaterQuality_Init(&hadc1);

    // --- 3. Mendaftarkan sensor dengan Data Kalibrasi yang aman dari (sys_calib) ---
    FlowSensor_Init(&sensor_inlet, sys_calib.fm_inlet_pulse_per_liter, &htim2, TIM_CHANNEL_1);
    FlowSensor_Init(&sensor_outlet, sys_calib.fm_outlet_pulse_per_liter, &htim2, TIM_CHANNEL_2);
    FlowSensor_Init(&sensor_fert, sys_calib.fm_fert_pulse_per_liter, &htim2, TIM_CHANNEL_3);

    printf("[APP] Smart Garden OS Awal Booting Berhasil.\n");

    // --- 4. Setup Queue Set (Mendengarkan Perintah Command & Interupsi Sensor Air) ---
    appQueueSet = xQueueCreateSet(15);
    xQueueAddToSet(appQueue, appQueueSet);
    xQueueAddToSet(wtrLvlQueue, appQueueSet);

    for (;;) {
        // PERBAIKAN 2: Menggunakan timeout 0 (Non-Blocking) agar tidak memblokir Task Notification
        // yang ditembakkan secara zero-latency oleh Interupsi DMA ADC Kualitas Air.
        QueueSetMemberHandle_t activatedQueue = xQueueSelectFromSet(appQueueSet, 0);

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

                // Jika sedang buang pupuk dan air habis, paksa pompa mati untuk proteksi Dry-Run
                if(wtrEvt.sensor == LVL_TANK_EMPTY && wtrEvt.is_reached) {
                    if (currentFertState == FERT_STATE_BUANG || currentFertState == FERT_STATE_MIXING) {
                         currentFertState = FERT_STATE_SAFETY_ERR;
                         printf("[APP] FATAL ERROR: Tangki kosong sebelum siklus selesai!\n");
                    }
                }
            }
        }

        // --- 5. Evaluasi Nilai Sinkronisasi DMA Kualitas Air ---
        // Jika ada Task Notification dari DMA, maka akan di proses seketika
        WaterQuality_ProcessAnalog();

        // --- PERBAIKAN 1: Wajib Membaca Pulsa Flowmeter ---
        // MENGAPA: Tanpa fungsi ini, pulsa interupsi tidak akan pernah dikonversi
        // ke dalam Liter (Volume). Fungsi ini krusial agar FSM tidak macet.
        FlowSensor_Read(&sensor_inlet);
        FlowSensor_Read(&sensor_outlet);
        FlowSensor_Read(&sensor_fert);

        // --- 6. Evaluasi Mesin Status FSM Utama ---
        HandleIrrigationRoutine();
        HandleFertilizationRoutine();

        // Beri waktu napas (Yielding CPU) untuk OS karena QueueSet diatur 0 ms (Non-Blocking)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief   Inisialisasi pembuatan Task App untuk RTOS Kernel
 * @param   priority Prioritas task ini dalam hierarki OS (semakin besar semakin prioritas)
 */
void APP_TaskCreate(UBaseType_t priority) {
    appQueue = xQueueCreate(10, sizeof(CommandEvent));
    // Kita menampung TaskHandle_t di appTaskHandle agar bisa dipanggil notifikasinya dari driver Kualitas Air
    xTaskCreate(vTaskApp, "AppTask", 1024, NULL, priority, &appTaskHandle);
}
