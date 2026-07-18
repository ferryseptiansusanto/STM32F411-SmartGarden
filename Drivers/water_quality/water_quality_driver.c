/*
 * water_quality_driver.c
 *
 *  Created on: 18 Jul 2026
 *      Author: ferry
 */


#include "water_quality/water_quality_driver.h"
#include "delay.h" // DWT Delay

// Buffer DMA untuk ADC
extern uint16_t adc_dma_buffer[2];
static WaterQualityData_t sensor_data = {0};
static ADC_HandleTypeDef *sensor_hadc;

void WaterQuality_Init(ADC_HandleTypeDef *hadc) {
    sensor_hadc = hadc;
    // Pastikan ADC di CubeMX sudah di-set Circular DMA
    HAL_ADC_Start_DMA(sensor_hadc, (uint32_t*)adc_dma_buffer, 2);
}

void WaterQuality_ProcessAnalog(void) {
    // Menggunakan alias untuk akses array
    uint16_t raw_TDS = adc_dma_buffer[ADC_INDEX_TDS];
    uint16_t raw_PH  = adc_dma_buffer[ADC_INDEX_PH];

    float v_tds = ((float)raw_TDS / 4095.0f) * 3.3f;
    float v_ph  = ((float)raw_PH  / 4095.0f) * 3.3f;

    // Rumus Konversi (Sesuaikan dengan datasheet sensor Anda)
    sensor_data.ph_val  = 3.5f * v_ph;
    sensor_data.tds_val = (133.42f * v_tds * v_tds * v_tds) - (255.86f * v_tds * v_tds) + (857.39f * v_tds);
    sensor_data.ec_val  = sensor_data.tds_val * 0.5f; // Faktor konversi
}

// ... [Fungsi DS18B20 tetap seperti sebelumnya] ...

WaterQualityData_t WaterQuality_GetData(void) {
    return sensor_data;
}
