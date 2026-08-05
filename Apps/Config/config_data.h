/**
 * @file    config_data.h
 * @brief   Definisi struktur data konfigurasi dan kalibrasi sistem (Revisi V2 - Include Temp).
 * @note    Struktur ini disimpan di EEPROM AT24C32 agar bersifat non-volatile.
 */

#ifndef APPS_CONFIG_DATA_H_
#define APPS_CONFIG_DATA_H_

#include <stdint.h>

/**
 * @struct  SystemConfig_t
 * @brief   Menyimpan semua parameter kalibrasi untuk sensor Smart Garden.
 */
#pragma pack(push, 1) // Paksa compiler merapatkan memori (No Padding)
typedef struct {
    /* --- Kalibrasi Kualitas Air & Temperatur --- */

    /** * @brief Offset pH pada tegangan 2.5V (Biasanya pH 7.0).
     */
    float ph_offset; /* 4 byte */

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

    /* --- Kalibrasi Flowmeter (Pulsa per Liter) --- */
    uint16_t fm_inlet_pulse_per_liter; /* 2 byte */
	uint16_t fm_outlet_pulse_per_liter;
	uint16_t fm_fert_pulse_per_liter;
	/* [MANUAL PADDING] Menambahkan 2 byte kosong agar bagian atas ini menjadi genap kelipatan 4.
	     * 20 byte (float) + 6 byte (uint16) + 2 byte (dummy) = 28 byte (Kelipatan 4) */
	uint16_t _reserved_padding;         /* 2 byte */

    /* --- Konfigurasi Operasional FSM (Aturan 10) --- */
	uint32_t waiting_user_response_time; // ms /* 4 byte */
	uint32_t max_delay_tolerance;        // ms

	/* =========================================================
	 * TAMBAHAN BARU: BATAS KRITIS AGRONOMI (SETPOINT)
	 * ========================================================= */
	float    max_ec_limit;   /**< Batas atas Electroconductivity (mS/cm). Memicu pengenceran/alarm. */ /* 4 byte */
	float    min_ph_limit;   /**< Batas bawah pH (terlalu asam). */
	float    max_ph_limit;   /**< Batas atas pH (terlalu basa). */
	// Anda bisa menambahkan max_tds_limit atau max_temp_limit jika diperlukan FSM.

	/* --- Checksum --- */
	/**
	 * @brief CRC32 dari seluruh variabel di atas.
	 * WAJIB berada di paling bawah (akhir) dari struct ini!
	 */
    uint32_t crc32;

} SystemConfig_t;
#pragma pack(pop)/* Kembalikan sifat compiler ke normal */

/* Global instance yang akan digunakan oleh seluruh FSM dan Driver */
extern SystemConfig_t sys_config;

#endif /* APPS_CONFIG_DATA_H_ */
