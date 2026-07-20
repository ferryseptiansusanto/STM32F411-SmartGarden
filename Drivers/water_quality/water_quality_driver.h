/*
 * water_quality_driver.h
 *
 *  Created on: 18 Jul 2026
 *      Author: ferry
 */

#ifndef DRIVERS_WATER_QUALITY_DRIVER_H_
#define DRIVERS_WATER_QUALITY_DRIVER_H_

#include "main.h"

// Makro indeks untuk memperjelas posisi array DMA
#define ADC_INDEX_TDS 0
#define ADC_INDEX_PH  1

typedef struct {
    float ph_val;
    float tds_val;
    float ec_val;
    // float temp_val; // (Asumsi untuk DS18B20 jika ada)
} WaterQualityData_t;

void WaterQuality_Init(ADC_HandleTypeDef *hadc);
void WaterQuality_ProcessAnalog(void);
WaterQualityData_t WaterQuality_GetData(void);

// Jembatan Callback DMA
void WaterQuality_ADC_Callback(ADC_HandleTypeDef *hadc);

#endif /* DRIVERS_WATER_QUALITY_DRIVER_H_ */
