/**
 * @file    ds3231_wrapper.h
 * @brief   Middleware Layer 2 untuk Real-Time Clock (RTC) DS3231 (Thread-Safe).
 * @note    Mendukung Date, Time, DateTime, dan Alarm 1 Interrupt untuk EXTI Wake-up.
 * Target:  STM32F411CEU6 (Smart Garden Project)
 *
 *  Created on: 5 May 2026
 *      Author: ferry
 */

#ifndef DS3231_WRAPPER_H
#define DS3231_WRAPPER_H

#include "i2c_wrapper.h" // Menggunakan Layer 1 untuk akses I2C & Mutex
#include <stdbool.h>

#define DS3231_DEFAULT_ADDRESS  0xD0  // Alamat I2C DS3231 (0x68 << 1)

// Alamat Register Kunci DS3231
#define DS3231_REG_TIME         0x00  // Register Awal Detik (0x00 - 0x06)
#define DS3231_REG_ALARM1       0x07  // Register Alarm 1 (0x07 - 0x0A)
#define DS3231_REG_CONTROL      0x0E  // Register Kontrol Interrupt/SQW
#define DS3231_REG_STATUS       0x0F  // Register Status Flag Alarm
#define DS3231_REG_TEMP_MSB     0x11  // Register Suhu Internal DS3231

/**
 * @brief Struktur Data untuk Jam (Time saja).
 */
typedef struct {
    uint8_t seconds;      /**< 0 - 59 */
    uint8_t minutes;      /**< 0 - 59 */
    uint8_t hours;        /**< 0 - 23 (Format 24 Jam) */
} DS3231_Time_t;

/**
 * @brief Struktur Data untuk Kalender (Date saja).
 */
typedef struct {
    uint8_t day_of_week;  /**< 1 (Senin) - 7 (Minggu) */
    uint8_t date;         /**< 1 - 31 */
    uint8_t month;        /**< 1 - 12 */
    uint16_t year;        /**< Format 4 digit (contoh: 2026) */
} DS3231_Date_t;

/**
 * @brief Struktur Data Lengkap Waktu & Tanggal (DateTime).
 */
typedef struct {
    DS3231_Time_t time;   /**< Sub-struktur Jam/Menit/Detik */
    DS3231_Date_t date;   /**< Sub-struktur Tanggal/Bulan/Tahun */
} DS3231_DateTime_t;

/**
 * @brief Mode Pencocokan Alarm 1.
 */
typedef enum {
    DS3231_ALARM1_EVERY_SECOND = 0,     /**< Alarm bunyi tiap detik */
    DS3231_ALARM1_MATCH_SEC,            /**< Cocok Detik */
    DS3231_ALARM1_MATCH_MIN_SEC,        /**< Cocok Menit & Detik */
    DS3231_ALARM1_MATCH_HOURS_MIN_SEC,  /**< Cocok Jam, Menit, & Detik (Harian) */
    DS3231_ALARM1_MATCH_DATE_HOURS_MIN_SEC /**< Cocok Tanggal, Jam, Menit, Detik */
} DS3231_Alarm1Mode_t;

/**
 * @brief Struktur Context untuk Objek DS3231.
 */
typedef struct {
    I2C_Context *i2c_ctx;   /**< Pointer ke Bus I2C fisik (Layer 1) */
    uint16_t dev_address;   /**< Alamat I2C Perangkat */
} DS3231_Device_t;

// --- API Inisialisasi ---
bool DS3231_Init(DS3231_Device_t *dev, I2C_Context *i2c, uint16_t addr);

// --- API DateTime (Lengkap) ---
bool DS3231_GetDateTime(DS3231_Device_t *dev, DS3231_DateTime_t *dt);
bool DS3231_SetDateTime(DS3231_Device_t *dev, const DS3231_DateTime_t *dt);
/**
 * @brief Membaca waktu dari RTC DS3231 dan mengonversinya langsung menjadi detik Unix Epoch.
 * @param dev Pointer ke objek DS3231_Device_t.
 * @return Waktu dalam detik sejak 1 Januari 1970 (Unix Epoch), atau 0 jika I2C gagal.
 */
uint32_t DS3231_GetEpochTime(DS3231_Device_t *dev);

// --- API Time Saja ---
bool DS3231_GetTime(DS3231_Device_t *dev, DS3231_Time_t *t);
bool DS3231_SetTime(DS3231_Device_t *dev, const DS3231_Time_t *t);

// --- API Date Saja ---
bool DS3231_GetDate(DS3231_Device_t *dev, DS3231_Date_t *d);
bool DS3231_SetDate(DS3231_Device_t *dev, const DS3231_Date_t *d);

// --- API Alarm & Interrupt (Penting untuk Low-Power / Sleep Wake-up) ---
bool DS3231_SetAlarm1(DS3231_Device_t *dev, const DS3231_DateTime_t *dt, DS3231_Alarm1Mode_t mode);
bool DS3231_ClearAlarm1Flag(DS3231_Device_t *dev);
bool DS3231_EnableAlarm1Interrupt(DS3231_Device_t *dev, bool enable);

// --- API Sensor Suhu Internal ---
bool DS3231_GetTemperature(DS3231_Device_t *dev, float *temp);

#endif // DS3231_WRAPPER_H
