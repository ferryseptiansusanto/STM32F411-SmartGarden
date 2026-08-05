/**
 * @file    temp_driver.c
 * @brief   Implementasi Driver DS18B20 menggunakan protokol 1-Wire.
 * @author  Ferry
 * @date    20 Jul 2026
 */

#include "temperature/temp_driver.h"
#include "../Apps/Config/config_data.h"   // Mengakses sys_config untuk offset kalibrasi
#include "delay.h"         // Modul delay mikrodetik (delay_us)
#include "FreeRTOS.h"
#include "task.h"

/* Command standar DS18B20 */
#define DS18B20_CMD_SKIP_ROM        0xCC
#define DS18B20_CMD_CONVERTT        0x44
#define DS18B20_CMD_READSCRATCH     0xBE

static GPIO_TypeDef *ds_port;
static uint16_t ds_pin;
static float current_temperature = 25.0f; // Nilai aman default (Fail-safe)

static void Set_Pin_Output(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = ds_pin;
    // MENGAPA OPEN-DRAIN? Protokol 1-Wire membutuhkan Pull-Up eksternal (4.7k).
    // MCU hanya boleh menarik ke LOW (GND) atau melepasnya (High-Z).
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ds_port, &GPIO_InitStruct);
}

static void Set_Pin_Input(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = ds_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ds_port, &GPIO_InitStruct);
}

static bool DS18B20_Reset(void) {
    bool presence = false;
    Set_Pin_Output();
    HAL_GPIO_WritePin(ds_port, ds_pin, GPIO_PIN_RESET);
    DelayUs(480); // Waktu reset standar 1-Wire (Minimal 480us)

    // MENGAPA CRITICAL SECTION DI SINI?
    // Jendela waktu (window) sensor merespon sangat sempit (sekitar 60-240us).
    // Kita harus mematikan interupsi OS agar pembacaan tidak meleset.
    taskENTER_CRITICAL();
    Set_Pin_Input();
    DelayUs(70);  // Tunggu sensor menarik garis ke LOW (Presence Pulse)

    if (HAL_GPIO_ReadPin(ds_port, ds_pin) == GPIO_PIN_RESET) {
        presence = true;
    }
    taskEXIT_CRITICAL(); // Segera nyalakan OS kembali

    DelayUs(410); // Selesaikan sisa slot waktu reset
    return presence;
}

static void DS18B20_WriteBit(uint8_t bit) {
    // MENGAPA CRITICAL SECTION DI SINI?
    // Bug diperbaiki: Menulis bit 1 butuh presisi delay maksimal 15us.
    // Tanpa pelindung ini, SysTick interrupt bisa membuat delay molor.
    taskENTER_CRITICAL();

    Set_Pin_Output();
    HAL_GPIO_WritePin(ds_port, ds_pin, GPIO_PIN_RESET);

    if (bit) {
        DelayUs(10);
        Set_Pin_Input(); // Lepas ke HIGH (Pull-up mengambil alih)
        DelayUs(55);
    } else {
        DelayUs(65);     // Tahan di LOW untuk menandakan bit 0
        Set_Pin_Input();
        DelayUs(5);
    }

    taskEXIT_CRITICAL();
}

static uint8_t DS18B20_ReadBit(void) {
    uint8_t bit = 0;

    taskENTER_CRITICAL();
    Set_Pin_Output();
    HAL_GPIO_WritePin(ds_port, ds_pin, GPIO_PIN_RESET);
    DelayUs(2);
    Set_Pin_Input();
    DelayUs(10); // Sampling harus dilakukan dalam rentang 15us setelah dilepas

    if (HAL_GPIO_ReadPin(ds_port, ds_pin) == GPIO_PIN_SET) {
        bit = 1;
    }
    taskEXIT_CRITICAL();

    DelayUs(50); // Sisa slot waktu pemulihan bus (Recovery time)
    return bit;
}

static void DS18B20_WriteByte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        DS18B20_WriteBit(data & 0x01);
        data >>= 1;
    }
}

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

    uint8_t lsb = DS18B20_ReadByte();
    uint8_t msb = DS18B20_ReadByte();

    int16_t raw_temp = (msb << 8) | lsb;

    // Menghitung suhu aktual (Resolusi 12-bit)
    float calculated_temp = ((float)raw_temp / 16.0f);

    // MENGAPA CRITICAL SECTION DI SINI?
    // Proteksi "Tearing" Data (Aturan 3). Jika Task lain membaca current_temperature
    // tepat saat variabel ini sedang di-update (tipe float 32-bit), nilainya bisa corrupt.
    taskENTER_CRITICAL();
    current_temperature = calculated_temp + sys_config.temp_offset;
    taskEXIT_CRITICAL();

    return true;
}

float TempSensor_GetTemperature(void) {
    float temp;
    taskENTER_CRITICAL(); // Thread-safe float read
    temp = current_temperature;
    taskEXIT_CRITICAL();
    return temp;
}
