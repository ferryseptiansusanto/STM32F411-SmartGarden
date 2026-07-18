/*
 * water_quality_driver.h
 *
 *  Created on: 18 Jul 2026
 *      Author: ferry
 */

#ifndef WATER_QUALITY_DRIVER_H_
#define WATER_QUALITY_DRIVER_H_

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

// Alias Index DMA berdasarkan Rank ADC Anda
#define ADC_INDEX_TDS  0  // Rank 1
#define ADC_INDEX_PH   1  // Rank 2

typedef struct {
    float ph_val;
    float tds_val;
    float ec_val;
    float temp_val;
} WaterQualityData_t;

// Inisialisasi ADC & Start DMA
void WaterQuality_Init(ADC_HandleTypeDef *hadc);

// Memproses data dari DMA Buffer
void WaterQuality_ProcessAnalog(void);

// Fungsi DS18B20 (Digital Temp)
void DS18B20_RequestTemperature(void);
float DS18B20_ReadTemperature(void);

// Mengambil data terkini
WaterQualityData_t WaterQuality_GetData(void);

#endif /* WATER_QUALITY_DRIVER_H_ */
