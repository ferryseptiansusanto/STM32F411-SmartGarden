/**
 * @file    temp_driver.h
 * @brief   Driver Sensor Temperatur Digital DS18B20 (1-Wire Protocol).
 * @note    Mendukung integrasi RTOS (Zero-Blocking) dan parameter kalibrasi terpusat.
 * @author  Ferry
 * @date    20 Jul 2026
 */

#ifndef DRIVERS_TEMP_TEMP_DRIVER_H_
#define DRIVERS_TEMP_TEMP_DRIVER_H_

#include "stm32f4xx_hal.h"
#include <stdbool.h>

/**
 * @brief   Inisialisasi GPIO dan pin untuk bus 1-Wire DS18B20.
 * @param   GPIOx    Port GPIO (misal: GPIOB)
 * @param   GPIO_Pin Pin GPIO (misal: GPIO_PIN_1)
 */
void TempSensor_Init(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

/**
 * @brief   Memicu konversi suhu baru pada sensor DS18B20.
 * @note    Fungsi ini TIDAK memblokir. Task pemanggil wajib melakukan vTaskDelay(~750ms)
 * setelah fungsi ini dipanggil sebelum membaca suhu.
 * @return  true jika sensor merespon, false jika sensor terputus.
 */
bool TempSensor_StartConversion(void);

/**
 * @brief   Membaca hasil suhu aktual dari DS18B20 dan menerapkan kalibrasi.
 * @return  true jika pembacaan sukses, false jika gagal.
 */
bool TempSensor_ReadTemperature(void);

/**
 * @brief   Getter Thread-Safe untuk mendapatkan nilai temperatur akhir.
 * @return  Suhu terkalibrasi dalam satuan derajat Celcius.
 */
float TempSensor_GetTemperature(void);

#endif /* DRIVERS_TEMP_TEMP_DRIVER_H_ */
