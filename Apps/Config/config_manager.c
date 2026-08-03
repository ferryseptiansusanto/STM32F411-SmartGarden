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
#include "default_config.h"  // Menarik nilai ROM baku (static const)
#include "eeprom_wrapper.h"  // API Layer 2 untuk memori
#include "board_config.h"    // <<-- BARU: Menarik parameter spesifikasi Hardware

/* Definisi Variabel Global (Akan menempati RAM) */
SystemConfig_t sys_calib;

/* Deklarasikan Objek EEPROM untuk AT24C32 (Kapasitas 32Kb / 4KB, Page Size 32 Byte) */
EEPROM_Device_t sys_eeprom;
extern I2C_Context i2c1_ctx; // Diasumsikan instance I2C Layer 1 sudah dibuat di main/driver

/**
 * @brief Private function untuk menghitung CRC32.
 * MENGAPA: Melindungi sistem dari 'Data Tearing' jika mati listrik saat Save.
 */
static uint32_t Calculate_CRC32(SystemConfig_t *data) {
    // Hitung panjang bit semua variabel, KECUALI variabel crc32 di baris terakhir
    uint32_t size_to_calc = sizeof(SystemConfig_t) - sizeof(uint32_t);

    // TODO: Ganti dengan Hardware CRC bawaan STM32 (HAL_CRC_Calculate)
    // return HAL_CRC_Calculate(&hcrc, (uint32_t*)data, size_to_calc / 4);

    uint32_t dummy_crc = 0xABCD1234;
    return dummy_crc;
}

void Config_LoadDefault(void) {
	// Salin nilai baku (ROM) ke RAM
	sys_calib = factory_default_calib;
	// Kalkulasi CRC untuk data baku agar sistem mengenalinya sebagai data valid
	sys_calib.crc32 = Calculate_CRC32(&sys_calib);
}

bool Config_Init(void) {
    /*
     * MENGAPA (Why): FSM pada saat booting (STATE_LOAD_CALIBRATION) akan mengeksekusi ini.
     * Jika I2C sehat, kita periksa Magic Word-nya. Jika Magic Word salah (misal 0xFFFFFFFF),
     * itu berarti EEPROM belum pernah ditulisi atau memori geser.
     */
	// 1. Inisialisasi Objek EEPROM terlebih dahulu (Kapasitas 32Kb / 4KB, Page 32 Byte)
	// Alamat I2C AT24C32 biasanya 0xA0, Page Size 32 byte, Total 4096 byte
	EEPROM_Init(&sys_eeprom, &i2c1_ctx, EEPROM_I2C_ADDR, EEPROM_PAGE_SIZE, EEPROM_TOTAL_SIZE);

	// 2. Baca menggunakan API Wrapper asinkron
	bool i2c_success = EEPROM_ReadBytes(&sys_eeprom, EEPROM_CONFIG_ADDRESS, (uint8_t*)&sys_calib, sizeof(SystemConfig_t));

	// 3. Verifikasi Integritas Data
    if (!i2c_success || sys_calib.crc32 != expected_crc) {
        // Jika gagal atau data sampah, aktifkan protokol FAIL-SAFE:
    	// FAIL-SAFE TRIGGERED: EEPROM pertama kali diinitialize, I2C Putus, atau Memory Corrupt!
        Config_LoadDefault();

        // Kita paksa tulis default ini ke EEPROM agar boot berikutnya sudah normal
        Config_Save();
        return false;
    }
    return true; // Berhasil dimuat secara utuh dari EEPROM
}

bool Config_Save(void) {
    /*
     * MENGAPA (Why): Fungsi ini di-trigger dari Task Komunikasi (Command Task)
     * ketika ada perintah (misal JSON kalibrasi masuk dari Bluetooth/HP).
     * Akan langsung me-replace isi dari EEPROM AT24C32.
     */
	return EEPROM_WriteBytes(&sys_eeprom, EEPROM_CONFIG_ADDRESS, (uint8_t*)&sys_calib, sizeof(SystemConfig_t));
}
