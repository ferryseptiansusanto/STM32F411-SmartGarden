/**
 * @file    temp_driver.h
 * @brief   Driver untuk Sensor Temperatur (Suhu Air/Lingkungan).
 * 		    Driver Sensor Temperatur Digital DS18B20 (1-Wire Protocol).
 * @note    Mendukung integrasi RTOS dan parameter kalibrasi terpusat.
 *
 *  Created on: 20 Jul 2026
 *      Author: ferry
 */

#ifndef DRIVERS_TEMP_TEMP_DRIVER_H_
#define DRIVERS_TEMP_TEMP_DRIVER_H_

#include "stm32f4xx_hal.h"
#include <stdbool.h>

/**
 * @brief   Inisialisasi GPIO dan pin untuk bus 1-Wire DS18B20.
 * @param   GPIOx   Port GPIO (misal: GPIOA)
 * @param   GPIO_Pin Pin GPIO (misal: GPIO_PIN_4)
 */
void TempSensor_Init(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

/**
 * @brief   Memicu konversi suhu baru pada sensor DS18B20.
 * @note    Perintah 'Convert T' dikirim ke sensor (membutuhkan waktu konversi ~750ms untuk resolusi 12-bit).
 */
bool TempSensor_StartConversion(void);

/**
 * @brief   Membaca hasil suhu aktual dari DS18B20 dan menerapkan kalibrasi.
 * @note    Dipanggil berkala oleh Task FreeRTOS setelah proses konversi selesai.
 */
bool TempSensor_ReadTemperature(void);

/**
 * @brief   Getter Thread-Safe untuk mendapatkan nilai temperatur akhir.
 * @return  Suhu dalam satuan derajat Celcius (Float).
 */
float TempSensor_GetTemperature(void);

#endif /* DRIVERS_TEMP_TEMP_DRIVER_H_ */
