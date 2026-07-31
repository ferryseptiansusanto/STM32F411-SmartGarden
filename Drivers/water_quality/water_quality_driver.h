/**
 * @file    water_quality_driver.h
 * @brief   Driver Sensor Kualitas Air (pH & TDS) berbasis Circular DMA.
 * @note    Terintegrasi dengan RTOS Task Notification dan EEPROM Calibration.
 * @author  Ferry
 * @date    18 Jul 2026
 */

#ifndef DRIVERS_WATER_QUALITY_DRIVER_H_
#define DRIVERS_WATER_QUALITY_DRIVER_H_

#include "main.h"
#include <stdbool.h>

/**
 * @brief Makro indeks untuk memperjelas posisi array DMA sesuai urutan channel di CubeMX
 */
#define ADC_INDEX_PH   0
#define ADC_INDEX_TDS  1

/**
 * @brief Struktur output data kualitas air yang telah difilter dan dikalibrasi.
 */
typedef struct {
    float ph_val;
    float tds_val;
    float ec_val;
} WaterQualityData_t;

/**
 * @brief Inisialisasi ADC dan memulai proses Circular DMA di background.
 * @param hadc Pointer ke handle ADC (contoh: &hadc1)
 */
void WaterQuality_Init(ADC_HandleTypeDef *hadc);

/**
 * @brief Fungsi pemrosesan matematis (Kalibrasi & Filtering).
 * @note  WAJIB dipanggil di dalam infinite loop Task pemantau kualitas air (Non-Blocking).
 */
void WaterQuality_ProcessAnalog(void);

/**
 * @brief Getter Thread-Safe untuk mengambil paket data kualitas air terakhir.
 * @return Struct berisi nilai pH, TDS, dan EC.
 */
WaterQualityData_t WaterQuality_GetData(void);

/**
 * @brief Jembatan Callback DMA.
 * WAJIB dipanggil di dalam fungsi HAL_ADC_ConvCpltCallback() di stm32f4xx_it.c
 * @param hadc Pointer ke handle ADC yang memicu interupsi
 */
void WaterQuality_ADC_Callback(ADC_HandleTypeDef *hadc);

#endif /* DRIVERS_WATER_QUALITY_DRIVER_H_ */
