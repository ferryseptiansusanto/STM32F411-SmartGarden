/**
 * @file    temp_driver.c
 * @brief   Implementasi Driver DS18B20 menggunakan protokol 1-Wire dan Delay Mikrodetik.
 *
 *  Created on: 20 Jul 2026
 *      Author: ferry
 */

#include "temperature/temp_driver.h"
#include "config_data.h"   // Mengakses sys_calib untuk offset kalibrasi
#include "delay.h"         // Modul delay mikrodetik (delay_us) yang ada di proyek Anda
#include "FreeRTOS.h"
#include "task.h"

/* Command DS18B20 */
#define DS18B20_CMD_SKIP_ROM        0xCC
#define DS18B20_CMD_CONVERTT        0x44
#define DS18B20_CMD_READSCRATCH     0xBE

static GPIO_TypeDef *ds_port;
static uint16_t ds_pin;
static float current_temperature = 25.0f;

/**
 * @brief   Mengubah arah pin GPIO menjadi Output (untuk menulis sinyal 1-Wire).
 */
static void Set_Pin_Output(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = ds_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; // Open-Drain dengan pull-up eksternal
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ds_port, &GPIO_InitStruct);
}

/**
 * @brief   Mengubah arah pin GPIO menjadi Input (untuk membaca sinyal 1-Wire).
 */
static void Set_Pin_Input(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = ds_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ds_port, &GPIO_InitStruct);
}

/**
 * @brief   Reset pulse untuk memulai komunikasi 1-Wire.
 * @return  true jika ada respon dari sensor (Presence Pulse), false jika tidak.
 */
static bool DS18B20_Reset(void) {
    bool presence = false;
    Set_Pin_Output();
    HAL_GPIO_WritePin(ds_port, ds_pin, GPIO_PIN_RESET);
    DelayUs(480); // Waktu reset standar 1-Wire (480us)

    Set_Pin_Input();
    DelayUs(70);  // Tunggu sensor merespon

    if (HAL_GPIO_ReadPin(ds_port, ds_pin) == GPIO_PIN_RESET) {
        presence = true;
    }
    DelayUs(410); // Selesaikan sisa slot waktu reset
    return presence;
}

/**
 * @brief   Menulis 1 bit data ke bus 1-Wire.
 */
static void DS18B20_WriteBit(uint8_t bit) {
    Set_Pin_Output();
    HAL_GPIO_WritePin(ds_port, ds_pin, GPIO_PIN_RESET);

    if (bit) {
    	DelayUs(10);
        Set_Pin_Input(); // Lepas jalur ke HIGH via pull-up resistor
        DelayUs(55);
    } else {
    	DelayUs(65);
        Set_Pin_Input();
        DelayUs(5);
    }
}

/**
 * @brief   Membaca 1 bit data dari bus 1-Wire.
 */
static uint8_t DS18B20_ReadBit(void) {
    uint8_t bit = 0;
    Set_Pin_Output();
    HAL_GPIO_WritePin(ds_port, ds_pin, GPIO_PIN_RESET);
    DelayUs(2);

    Set_Pin_Input();
    DelayUs(10);

    if (HAL_GPIO_ReadPin(ds_port, ds_pin) == GPIO_PIN_SET) {
        bit = 1;
    }
    DelayUs(50);
    return bit;
}

/**
 * @brief   Menulis 1 byte data.
 */
static void DS18B20_WriteByte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        DS18B20_WriteBit(data & 0x01);
        data >>= 1;
    }
}

/**
 * @brief   Membaca 1 byte data.
 */
static uint8_t DS18B20_ReadByte(void) {
    uint8_t data = 0;
    for (int i = 0; i < 8; i++) {
        data |= (DS18B20_ReadBit() << i);
    }
    return data;
}

/* -------------------------------------------------------------------------
 * FUNGSI PUBLIK DRIVER
 * ------------------------------------------------------------------------- */

void TempSensor_Init(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    ds_port = GPIOx;
    ds_pin = GPIO_Pin;
    current_temperature = 25.0f;
}

bool TempSensor_StartConversion(void) {
    if (!DS18B20_Reset()) return false;
    DS18B20_WriteByte(DS18B20_CMD_SKIP_ROM);
    DS18B20_WriteByte(DS18B20_CMD_CONVERTT);
    return true;
}

bool TempSensor_ReadTemperature(void) {
    if (!DS18B20_Reset()) return false;

    DS18B20_WriteByte(DS18B20_CMD_SKIP_ROM);
    DS18B20_WriteByte(DS18B20_CMD_READSCRATCH);

    /* Baca 2 byte data suhu dari Scratchpad DS18B20 */
    uint8_t lsb = DS18B20_ReadByte();
    uint8_t msb = DS18B20_ReadByte();

    int16_t raw_temp = (msb << 8) | lsb;

    /* Konversi nilai mentah 16-bit ke derajat Celcius
     * Resolusi default 12-bit memiliki step 0.0625 °C (dibagi 16.0)
     */
    float calculated_temp = ((float)raw_temp / 16.0f);

    /* Terapkan parameter kalibrasi (Offset) dari config_data.h (sys_calib) */
    taskENTER_CRITICAL();
    current_temperature = calculated_temp + sys_calib.temp_offset;
    taskEXIT_CRITICAL();

    return true;
}

float TempSensor_GetTemperature(void) {
    float temp;
    taskENTER_CRITICAL();
    temp = current_temperature;
    taskEXIT_CRITICAL();
    return temp;
}

