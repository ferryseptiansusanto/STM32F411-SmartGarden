/**
 * @file    water_quality_driver.c
 * @brief   Implementasi Kualitas Air dengan perlindungan Data Tearing dan Filter EMA.
 * @author  Ferry
 * @date    18 Jul 2026
 */

#include "water_quality_driver.h"
#include "config_data.h"    // Jembatan ke sys_calib (dari EEPROM)
#include "FreeRTOS.h"
#include "task.h"

// Ambil Handle Task aplikasi dari app_task.c agar kita bisa menembakkan notifikasi
extern TaskHandle_t appTaskHandle;

static ADC_HandleTypeDef *sensor_hadc;
static WaterQualityData_t sensor_data = {0};

// 1. Buffer DMA utama (Hardware yang menulis ke sini, CPU dilarang menyentuh langsung)
static volatile uint16_t adc_dma_buffer[2];

// 2. Buffer aman (Shadow buffer) untuk memotong tali memori antara DMA dan CPU
static volatile uint16_t adc_safe_buffer[2];

// Faktor penghalus EMA (Exponential Moving Average).
// Rentang 0.0 -> 1.0. (0.1 berarti = 10% data baru + 90% data lama). Sangat stabil!
#define EMA_ALPHA 0.1f
static float filtered_v_ph  = 0.0f;
static float filtered_v_tds = 0.0f;
static bool  is_first_read  = true; // Flag untuk set nilai awal filter

void WaterQuality_Init(ADC_HandleTypeDef *hadc) {
    if (hadc == NULL) return;
    sensor_hadc = hadc;

    // Fail-safe init state
    filtered_v_ph  = 0.0f;
    filtered_v_tds = 0.0f;
    is_first_read  = true;

    // Mulai transfer data dari Register ADC ke adc_dma_buffer tanpa intervensi CPU (Rule 2)
    HAL_ADC_Start_DMA(sensor_hadc, (uint32_t*)adc_dma_buffer, 2);
}

/* ------------------------------------------------------------------
 * CALLBACK: INTERRUPT SERVICE ROUTINE (Dipanggil oleh Hardware)
 * ------------------------------------------------------------------ */
void WaterQuality_ADC_Callback(ADC_HandleTypeDef *hadc) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hadc->Instance == sensor_hadc->Instance) {
        // MENGAPA DISALIN KE SAFE BUFFER?
        // Melindungi data agar jika DMA menimpa ulang dma_buffer di tengah Task
        // sedang melakukan kalkulasi, nilai yang sedang dihitung Task tidak bergeser.
        adc_safe_buffer[ADC_INDEX_TDS] = adc_dma_buffer[ADC_INDEX_TDS];
        adc_safe_buffer[ADC_INDEX_PH]  = adc_dma_buffer[ADC_INDEX_PH];

        if (appTaskHandle != NULL) {
            // MENGAPA TASK NOTIFICATION? Jauh lebih ringan & cepat dari xSemaphoreGiveFromISR
            vTaskNotifyGiveFromISR(appTaskHandle, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

/* ------------------------------------------------------------------
 * TASK LOGIC: FUNGSI PENGOLAHAN MATEMATIS (Level OS)
 * ------------------------------------------------------------------ */
void WaterQuality_ProcessAnalog(void) {
    // ulTaskNotifyTake akan membersihkan flag notifikasi.
    // Timeout 0 menjadikannya Non-Blocking O(1) sehingga Task tidak tertahan.
    if (ulTaskNotifyTake(pdTRUE, 0) > 0) {

        // MENGAPA CRITICAL SECTION DI SINI?
        // Membaca array 16-bit mungkin butuh 2 siklus clock. Cegah DMA ISR
        // mengubah isi adc_safe_buffer tepat saat kita sedang memindahkannya.
        taskENTER_CRITICAL();
        uint16_t raw_TDS = adc_safe_buffer[ADC_INDEX_TDS];
        uint16_t raw_PH  = adc_safe_buffer[ADC_INDEX_PH];
        taskEXIT_CRITICAL();

        // 1. Konversi ke Tegangan Aktual (Volt)
        float current_v_tds = ((float)raw_TDS / 4095.0f) * 3.3f;
        float current_v_ph  = ((float)raw_PH  / 4095.0f) * 3.3f;

        // 2. Filter Digital EMA (Meredam noise pompa / ombak air)
        if (is_first_read) {
            filtered_v_ph  = current_v_ph;
            filtered_v_tds = current_v_tds;
            is_first_read  = false;
        } else {
            filtered_v_ph  = (EMA_ALPHA * current_v_ph)  + ((1.0f - EMA_ALPHA) * filtered_v_ph);
            filtered_v_tds = (EMA_ALPHA * current_v_tds) + ((1.0f - EMA_ALPHA) * filtered_v_tds);
        }

        // 3. Terapkan Kalibrasi Dinamis dari EEPROM (sys_calib)
        float calc_ph  = (sys_calib.ph_slope * filtered_v_ph) + sys_calib.ph_offset;

        // Rumus Polinomial TDS (Umum untuk sensor Gravity TDS)
        float raw_tds_val = (133.42f * filtered_v_tds * filtered_v_tds * filtered_v_tds)
                          - (255.86f * filtered_v_tds * filtered_v_tds)
                          + (857.39f * filtered_v_tds);

        float calc_tds = raw_tds_val * sys_calib.tds_factor;
        float calc_ec  = calc_tds * 0.5f;

        // 4. Update Struct Global dengan Proteksi Data Tearing
        // MENGAPA CRITICAL SECTION DI SINI?
        // Menyimpan 3 tipe data float secara berurutan BUKAN operasi atomik.
        taskENTER_CRITICAL();
        sensor_data.ph_val  = calc_ph;
        sensor_data.tds_val = calc_tds;
        sensor_data.ec_val  = calc_ec;
        taskEXIT_CRITICAL();
    }
}

WaterQualityData_t WaterQuality_GetData(void) {
    WaterQualityData_t copy_data;

    // BUG FIX: Proteksi pengambilan Struct dari Data Tearing (Aturan 3)
    taskENTER_CRITICAL();
    copy_data = sensor_data;
    taskEXIT_CRITICAL();

    return copy_data;
}
