/*
 * water_quality_driver.c
 *
 * @brief   Driver Kualitas Air dengan integrasi Sensor pH, TDS.
 * Refactored: Transisi dari Polling Flag ke FreeRTOS Task Notification
 *
 * Created on: 18 Jul 2026
 * Author: ferry
 */

#include "water_quality_driver.h"
#include "config_data.h"    // Jembatan ke kalibrasi global
#include "FreeRTOS.h"
#include "task.h"

// Ambil Handle Task aplikasi dari app_task.c agar kita bisa menembakkan notifikasi
extern TaskHandle_t appTaskHandle;

static ADC_HandleTypeDef *sensor_hadc;
static WaterQualityData_t sensor_data = {0};

// 1. Buffer DMA utama (Jangan diakses langsung oleh Task)
static volatile uint16_t adc_dma_buffer[2];

// 2. Buffer aman (Shadow buffer) untuk proses matematis
static volatile uint16_t adc_safe_buffer[2];

void WaterQuality_Init(ADC_HandleTypeDef *hadc) {
    if (hadc == NULL) return;
    sensor_hadc = hadc;

    // Pastikan ADC di CubeMX di-set Circular DMA dan mode Continuous Conversion
    HAL_ADC_Start_DMA(sensor_hadc, (uint32_t*)adc_dma_buffer, 2);
}

// ------------------------------------------------------------------
// CALLBACK: Dipanggil otomatis oleh STM32 saat DMA selesai 1 siklus
// ------------------------------------------------------------------
void WaterQuality_ADC_Callback(ADC_HandleTypeDef *hadc) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hadc->Instance == sensor_hadc->Instance) {
        // Amankan nilai buffer ke shadow buffer
        adc_safe_buffer[ADC_INDEX_TDS] = adc_dma_buffer[ADC_INDEX_TDS];
        adc_safe_buffer[ADC_INDEX_PH]  = adc_dma_buffer[ADC_INDEX_PH];

        // Kirim Notification ke App Task seketika tanpa menunggu siklus tick OS (Zero-Latency)
        if (appTaskHandle != NULL) {
            vTaskNotifyGiveFromISR(appTaskHandle, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

// ------------------------------------------------------------------
// TASK LOGIC: Dipanggil dari dalam infinite loop Task FreeRTOS
// ------------------------------------------------------------------
void WaterQuality_ProcessAnalog(void) {
    // Mengecek apakah DMA sudah selesai mengambil data
    // Parameter 0 di belakang berarti fungsi ini "Non-Blocking" (tidak membuat CPU terdiam jika tidak ada data)
    if (ulTaskNotifyTake(pdTRUE, 0) > 0) {

        taskENTER_CRITICAL();
        uint16_t raw_TDS = adc_safe_buffer[ADC_INDEX_TDS];
        uint16_t raw_PH  = adc_safe_buffer[ADC_INDEX_PH];
        taskEXIT_CRITICAL();

        // Tegangan Aktual Sensor
        float v_tds = ((float)raw_TDS / 4095.0f) * 3.3f;
        float v_ph  = ((float)raw_PH  / 4095.0f) * 3.3f;

        // 1. Rumus Kalibrasi pH = (Slope * Tegangan) + Offset
        // Menggunakan variabel global sys_calib
        sensor_data.ph_val  = (sys_calib.ph_slope * v_ph) + sys_calib.ph_offset;

        // 2. Rumus Kalibrasi TDS = TDS Mentah * K-Value (TDS Factor)
        float raw_tds_val = (133.42f * v_tds * v_tds * v_tds) - (255.86f * v_tds * v_tds) + (857.39f * v_tds);
        sensor_data.tds_val = raw_tds_val * sys_calib.tds_factor;

        // 3. EC Value (Tergantung Konstanta Konversi TDS)
        sensor_data.ec_val  = sensor_data.tds_val * 0.5f;
    }
}

WaterQualityData_t WaterQuality_GetData(void) {
    return sensor_data;
}
