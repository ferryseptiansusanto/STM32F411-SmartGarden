/**
 * @file    config_data.h
 * @brief   Definisi struktur data konfigurasi dan kalibrasi sistem (Revisi V2 - Include Temp).
 * @note    Struktur ini disimpan di EEPROM AT24C32 agar bersifat non-volatile.
 */

#ifndef APPS_CONFIG_DATA_H_
#define APPS_CONFIG_DATA_H_

#include <stdint.h>

/**
 * @struct  SensorCalibration_t
 * @brief   Menyimpan semua parameter kalibrasi untuk sensor Smart Garden.
 */
typedef struct {
    /* --- Kalibrasi Kualitas Air & Temperatur --- */

    /** * @brief Offset pH pada tegangan 2.5V (Biasanya pH 7.0).
     */
    float ph_offset;

    /** * @brief Slope pH (Kemiringan kurva mV per step pH).
     */
    float ph_slope;

    /** * @brief Faktor kalibrasi dasar TDS (Total Dissolved Solids).
     */
    float tds_factor;

    /** * @brief Offset kalibrasi temperatur (dalam °C).
     * MENGAPA: Untuk menggeser nilai jika sensor memiliki error sistematis konstan
     * (misal selalu membaca lebih tinggi 1.5°C dari termometer standar).
     */
    float temp_offset;

    /** * @brief Kemiringan/Skala konversi temperatur.
     * MENGAPA: Menyesuaikan konversi voltase ke °C berdasarkan spesifikasi rangkaian op-amp.
     */
    float temp_slope;

    /* --- Kalibrasi Flowmeter --- */
    uint16_t fm_inlet_pulse_per_liter;
    uint16_t fm_outlet_pulse_per_liter;
    uint16_t fm_fert_pulse_per_liter;

    /* --- Checksum untuk Validitas Data --- */
    uint32_t crc32;

} SensorCalibration_t;

/* Global instance yang dikelola oleh config_manager.c */
extern SensorCalibration_t sys_calib;

#endif /* APPS_CONFIG_DATA_H_ */
