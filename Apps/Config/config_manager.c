/*
 * config_manager.c
 *
 * @file    config_manager.c
 * @brief   Implementasi manajemen konfigurasi dan kalibrasi terpusat.
 *
 * Created on: 14 Jul 2026
 * Author: ferry
 */

#include "config_manager.h"
#include "../Managers/log_manager.h"

#include "default_config.h"  // Menarik nilai ROM baku (static const)
#include "eeprom_wrapper.h"  // API Layer 2 untuk memori
#include "board_config.h"    // <<-- BARU: Menarik parameter spesifikasi Hardware

/* Definisi Variabel Global (Akan menempati RAM) */
SystemConfig_t sys_config;

/* Deklarasikan Objek EEPROM untuk AT24C32 (Kapasitas 32Kb / 4KB, Page Size 32 Byte) */
EEPROM_Device_t sys_eeprom;
extern I2C_Context i2c1_ctx; // Diasumsikan instance I2C Layer 1 sudah dibuat di main/driver
extern CRC_HandleTypeDef hcrc;
/**
 * @brief Private function untuk menghitung CRC32.
 * MENGAPA: Melindungi sistem dari 'Data Tearing' jika mati listrik saat Save.
 */
static uint32_t Calculate_CRC32(SystemConfig_t *data) {
	if (data == NULL) return 0;

	/* 1. Hitung jumlah byte yang akan dikalkulasi.
	 * MENGAPA: Kita HARUS mengecualikan field 'crc32' yang berada di paling
	 * bawah struct, karena kita tidak bisa menghitung CRC dari CRC itu sendiri.
	 */
	uint32_t bytes_to_calc = sizeof(SystemConfig_t) - sizeof(uint32_t);

	/* 2. Hardware CRC STM32 membutuhkan panjang data dalam satuan WORD (32-bit).
	 * Pastikan sizeof(SystemConfig_t) dikurangi sizeof(uint32_t) adalah kelipatan 4.
	 */
	uint32_t words_to_calc = bytes_to_calc / 4;

	/* 3. Kalkulasi menggunakan Hardware CRC STM32.
	 * HAL_CRC_Calculate menerima array of uint32_t. Kita casting pointer struct kita.
	 */
	uint32_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)data, words_to_calc);

	return calculated_crc;
}

void ConfigManager_LoadDefault(void) {
	// Salin nilai baku (dari Flash/ROM) ke RAM
	sys_config = factory_default_calib;
	// Kalkulasi CRC untuk data baku agar sistem mengenalinya sebagai data valid
	sys_config.crc32 = Calculate_CRC32(&sys_config);

	LogManager_Write(LOG_INFO, "Memuat Factory Default Config ke RAM.");
}

bool ConfigManager_Init(void) {
    /*
     * MENGAPA (Why): FSM pada saat booting (STATE_LOAD_CALIBRATION) akan mengeksekusi ini.
     * Jika I2C sehat, kita periksa Magic Word-nya. Jika Magic Word salah (misal 0xFFFFFFFF),
     * itu berarti EEPROM belum pernah ditulisi atau memori geser.
     */
	// 1. Inisialisasi Objek EEPROM terlebih dahulu (Kapasitas 32Kb / 4KB, Page 32 Byte)
	// Alamat I2C AT24C32 biasanya 0xA0, Page Size 32 byte, Total 4096 byte
	EEPROM_Init(&sys_eeprom, &i2c1_ctx, EEPROM_I2C_ADDR, EEPROM_PAGE_SIZE, EEPROM_TOTAL_SIZE);

	// 2. Baca menggunakan API Wrapper asinkron
	bool i2c_success = EEPROM_ReadBytes(&sys_eeprom, EEPROM_CONFIG_ADDRESS, (uint8_t*)&sys_config, sizeof(SystemConfig_t));

	// 3. Kalkulasi ekspektasi CRC dari data yang baru saja dibaca dari EEPROM
	    uint32_t expected_crc = Calculate_CRC32(&sys_config);

	// 4. Verifikasi Integritas Data
    if (!i2c_success || sys_config.crc32 != expected_crc) {
        // Jika gagal atau data sampah, aktifkan protokol FAIL-SAFE:
    	// FAIL-SAFE TRIGGERED: EEPROM pertama kali diinitialize, I2C Putus, atau Memory Corrupt!
		LogManager_Write(LOG_ERROR, "CRITICAL: EEPROM CRC tidak cocok atau I2C error. Memuat Default!");

    	ConfigManager_LoadDefault();
        // Kita paksa tulis default ini ke EEPROM agar boot berikutnya sudah normal
    	ConfigManager_Save();
        return false;
    }
    LogManager_Write(LOG_INFO, "Konfigurasi sistem berhasil dimuat dari EEPROM.");
    return true; // Berhasil dimuat secara utuh dari EEPROM
}

bool ConfigManager_Save(void) {
    /*
     * MENGAPA (Why): Fungsi ini di-trigger dari Task Komunikasi (Command Task)
     * ketika ada perintah (misal JSON kalibrasi masuk dari Bluetooth/HP).
     * Akan langsung me-replace isi dari EEPROM AT24C32.
     */


	// Perbarui CRC32 sebelum menuliskannya secara fisik ke chip memori
	sys_config.crc32 = Calculate_CRC32(&sys_config);

	bool result = EEPROM_WriteBytes(&sys_eeprom, EEPROM_CONFIG_ADDRESS, (uint8_t*)&sys_config, sizeof(SystemConfig_t));

	if(result) {
		LogManager_Write(LOG_INFO, "Konfigurasi baru berhasil disimpan ke EEPROM.");
	} else {
		LogManager_Write(LOG_ERROR, "Gagal menyimpan konfigurasi ke EEPROM.");
	}

	return result;
}
