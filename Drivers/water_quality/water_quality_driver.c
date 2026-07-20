/*
 * water_quality_driver.c
 *
 * Created on: 18 Jul 2026
 * Author: ferry
 */

#include "water_quality_driver.h"
#include "eeprom_wrapper.h" // Modul I2C EEPROM Anda
#include "FreeRTOS.h"
#include "task.h"

static ADC_HandleTypeDef *sensor_hadc;
static WaterQualityData_t sensor_data = {0};
static WaterQuality_CalibData_t calib_data = {0};

// 1. Buffer DMA utama (Jangan diakses langsung oleh Task)
static volatile uint16_t adc_dma_buffer[2];

// 2. Buffer aman (Shadow buffer) untuk proses matematis
static volatile uint16_t adc_safe_buffer[2];

// 3. Flag penanda data baru siap
static volatile uint8_t adc_data_ready = 0;


void WaterQuality_Init(ADC_HandleTypeDef *hadc) {
    if (hadc == NULL) return;
    sensor_hadc = hadc;

    // Load data kalibrasi dari EEPROM saat booting
    WaterQuality_LoadCalibration();

    // Pastikan ADC di CubeMX di-set Circular DMA dan mode Continuous Conversion
    HAL_ADC_Start_DMA(sensor_hadc, (uint32_t*)adc_dma_buffer, 2);
}

// ------------------------------------------------------------------
// MANAJEMEN KALIBRASI DENGAN EEPROM AT24C32
// ------------------------------------------------------------------
void WaterQuality_LoadCalibration(void) {
    // Baca memori dari EEPROM menggunakan fungsi wrapper
    bool success = EEPROM_Read(&EEPROM_Ctx, WQ_CALIB_EEPROM_ADDR, (uint8_t*)&calib_data, sizeof(WaterQuality_CalibData_t));

    // Jika I2C gagal atau EEPROM baru/kosong (Magic Word tidak cocok)
    if (!success || calib_data.magic_word != WQ_CALIB_MAGIC_WORD) {
        // Muat nilai default pabrik/ideal
        calib_data.magic_word   = WQ_CALIB_MAGIC_WORD;
        calib_data.ph_slope     = 3.5f;   // Sesuai rumus asli Anda
        calib_data.ph_intercept = 0.0f;
        calib_data.tds_k_value  = 1.0f;   // Faktor pengali 1 (tidak mengubah rumus dasar)

        // (Opsional) Langsung simpan nilai default ini ke EEPROM
        // WaterQuality_SaveCalibration();
    }
}

bool WaterQuality_SaveCalibration(void) {
    // Tulis nilai kalibrasi terbaru ke EEPROM
    return EEPROM_Write(&EEPROM_Ctx, WQ_CALIB_EEPROM_ADDR, (const uint8_t*)&calib_data, sizeof(WaterQuality_CalibData_t));
}

void WaterQuality_SetCalibration(float ph_slope, float ph_offset, float tds_k) {
    // Fungsi ini dipanggil dari Bluetooth/Serial Task saat ada command kalibrasi masuk
    calib_data.ph_slope     = ph_slope;
    calib_data.ph_intercept = ph_offset;
    calib_data.tds_k_value  = tds_k;

    // Simpan permanen ke EEPROM
    WaterQuality_SaveCalibration();
}

WaterQuality_CalibData_t WaterQuality_GetCalibration(void) {
    return calib_data;
}

// ------------------------------------------------------------------
// CALLBACK: Dipanggil otomatis oleh STM32 saat DMA selesai 1 siklus
// ------------------------------------------------------------------
void WaterQuality_ADC_Callback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == sensor_hadc->Instance) {
        adc_safe_buffer[ADC_INDEX_TDS] = adc_dma_buffer[ADC_INDEX_TDS];
        adc_safe_buffer[ADC_INDEX_PH]  = adc_dma_buffer[ADC_INDEX_PH];
        adc_data_ready = 1;
    }
}

// ------------------------------------------------------------------
// TASK LOGIC: Dipanggil dari dalam infinite loop Task FreeRTOS
// ------------------------------------------------------------------
void WaterQuality_ProcessAnalog(void) {
    if (!adc_data_ready) return;

    taskENTER_CRITICAL();
    uint16_t raw_TDS = adc_safe_buffer[ADC_INDEX_TDS];
    uint16_t raw_PH  = adc_safe_buffer[ADC_INDEX_PH];
    adc_data_ready = 0;
    taskEXIT_CRITICAL();

    // Tegangan Aktual Sensor
    float v_tds = ((float)raw_TDS / 4095.0f) * 3.3f;
    float v_ph  = ((float)raw_PH  / 4095.0f) * 3.3f;

    // 1. Rumus Kalibrasi pH = (Slope * Tegangan) + Offset
    sensor_data.ph_val  = (calib_data.ph_slope * v_ph) + calib_data.ph_intercept;

    // 2. Rumus Kalibrasi TDS = TDS Mentah * K-Value
    float raw_tds_val = (133.42f * v_tds * v_tds * v_tds) - (255.86f * v_tds * v_tds) + (857.39f * v_tds);
    sensor_data.tds_val = raw_tds_val * calib_data.tds_k_value;

    // 3. EC Value (Tergantung Konstanta Konversi TDS)
    sensor_data.ec_val  = sensor_data.tds_val * 0.5f;
}

WaterQualityData_t WaterQuality_GetData(void) {
    return sensor_data;
}
