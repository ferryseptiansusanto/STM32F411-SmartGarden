/*
 * water_quality_driver.h
 *
 *  Created on: 18 Jul 2026
 *      Author: ferry
 */

#ifndef DRIVERS_WATER_QUALITY_DRIVER_H_
#define DRIVERS_WATER_QUALITY_DRIVER_H_

#include "main.h"
#include <stdbool.h>

// Makro indeks untuk memperjelas posisi array DMA
#define ADC_INDEX_PH 0
#define ADC_INDEX_TDS  1

// Alamat awal penyimpanan kalibrasi di EEPROM (bisa disesuaikan)
#define WQ_CALIB_EEPROM_ADDR 0x0000

// Tanda pengenal bahwa EEPROM sudah berisi data kalibrasi yang valid
#define WQ_CALIB_MAGIC_WORD  0xAABBCCDD

typedef struct {
    float ph_val;
    float tds_val;
    float ec_val;
    // float temp_val; // (Asumsi untuk DS18B20 jika ada)
} WaterQualityData_t;

// Struktur Data Kalibrasi untuk disimpan di EEPROM
typedef struct {
    uint32_t magic_word;   // Penanda validitas data memori
    float ph_slope;        // Multiplier kalibrasi pH
    float ph_intercept;    // Offset kalibrasi pH
    float tds_k_value;     // Faktor pengali (K-Value) kalibrasi TDS
} WaterQuality_CalibData_t;

// API Inisialisasi & Pengambilan Data
void WaterQuality_Init(ADC_HandleTypeDef *hadc);
void WaterQuality_ProcessAnalog(void);
WaterQualityData_t WaterQuality_GetData(void);


// Jembatan Callback DMA
void WaterQuality_ADC_Callback(ADC_HandleTypeDef *hadc);

#endif /* DRIVERS_WATER_QUALITY_DRIVER_H_ */
