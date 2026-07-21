/*
 * config_manager.c
 *
 * @file    config_manager.c
 * @brief   Implementasi logika validasi CRC dan manajemen EEPROM secara dinamis.
 *
 * Created on: 14 Jul 2026
 * Author: ferry
 */

#include "config_manager.h"
#include "config_data.h"
#include "default_config.h"
#include "eeprom_wrapper.h"  /* Abstraksi I2C EEPROM (AT24C32) */
#include <string.h>

/* Instansiasi Global Variabel Kalibrasi (di RAM) */
SensorCalibration_t sys_calib;

/* Pointer privat ke device EEPROM yang sedang aktif (Proteksi Modularitas) */
static I2C_EEPROMDevice *active_eeprom_device = NULL;

/**
 * @brief   Fungsi internal untuk menghitung Checksum CRC32.
 */
static uint32_t Calculate_CRC32(const uint8_t *data, uint16_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320; /* Standar IEEE 802.3 Polynomial */
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

/**
 * @brief   Fungsi Boot: Cek EEPROM -> Validasi -> Load / Factory Reset
 */
void ConfigManager_Init(I2C_EEPROMDevice *dev) {
    if (dev == NULL) return;

    /* 1. Simpan pointer ke memori lokal modul ini */
    active_eeprom_device = dev;

    SensorCalibration_t temp_calib;

    /* 2. Baca isi EEPROM ke temporary buffer. */
    bool is_read_success = EEPROM_Read(active_eeprom_device, EEPROM_CALIB_START_ADDR,
                                            (uint8_t*)&temp_calib,
                                            sizeof(SensorCalibration_t));

    if (is_read_success) {
        /* 3. Hitung CRC dari data yang dibaca (Tanpa bit CRC struct di belakang) */
        uint16_t data_len = sizeof(SensorCalibration_t) - sizeof(uint32_t);
        uint32_t calculated_crc = Calculate_CRC32((uint8_t*)&temp_calib, data_len);

        /* 4. Validasi Integritas Data */
        if (calculated_crc == temp_calib.crc32) {
            /* Data utuh! Pindahkan ke RAM operasional (sys_calib) */
            memcpy(&sys_calib, &temp_calib, sizeof(SensorCalibration_t));
            return; /* Selesai dengan sukses */
        }
    }

    /* 5. Jika baca gagal ATAU CRC tidak cocok (Memori Kosong/Korup), panggil Reset. */
    ConfigManager_ResetToDefault();
}

/**
 * @brief   Menyimpan sys_calib terkini dari RAM ke dalam EEPROM I2C.
 */
bool ConfigManager_Save(void) {
    if (active_eeprom_device == NULL) return false; /* Fail-Safe Check */

    /* 1. Kalkulasi CRC baru berdasarkan nilai aktual di RAM */
    uint16_t data_len = sizeof(SensorCalibration_t) - sizeof(uint32_t);
    sys_calib.crc32 = Calculate_CRC32((uint8_t*)&sys_calib, data_len);

    /* 2. Tulis seluruh block (termasuk CRC yang baru) menggunakan Pointer dinamis */
    return EEPROM_Write(active_eeprom_device, EEPROM_CALIB_START_ADDR,
                             (uint8_t*)&sys_calib,
                             sizeof(SensorCalibration_t));
}

/**
 * @brief   Memaksa sistem kembali ke pengaturan pabrik.
 */
void ConfigManager_ResetToDefault(void) {
    /* Menggunakan memcpy untuk mencegah data tearing */
    memcpy(&sys_calib, &factory_default_calib, sizeof(SensorCalibration_t));

    /* Simpan langsung ke EEPROM agar pada boot berikutnya CRC sudah valid */
    ConfigManager_Save();
}
