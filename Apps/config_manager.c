/*
 * config_manager.c
 *
 * @file    config_manager.c
 * @brief   Implementasi logika validasi CRC dan manajemen EEPROM.
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */

#include "config_manager.h"
#include "config_data.h"
#include "default_config.h"
#include "eeprom_wrapper.h"  /* Abstraksi I2C EEPROM (AT24C32) */
#include <string.h>

/* Instansiasi Global Variabel Kalibrasi (di RAM) */
SensorCalibration_t sys_calib;

/**
 * @brief   Fungsi internal untuk menghitung Checksum CRC32.
 * @note    MENGAPA menggunakan software CRC? Agar library ini independen
 * dan tidak bergantung mutlak pada inisialisasi Hardware CRC peripheral STM32,
 * sehingga lebih portabel.
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
    SensorCalibration_t temp_calib;

    /* 1. Baca isi EEPROM ke temporary buffer.
     * Asumsi fungsi Wrapper: EEPROM_ReadBlock(address, data_ptr, size)
     */

    bool is_read_success = EEPROM_Read(dev, EEPROM_CALIB_START_ADDR,
                                            (uint8_t*)&temp_calib,
                                            sizeof(SensorCalibration_t));

    if (is_read_success) {
        /* 2. Hitung CRC dari data yang dibaca
         * MENGAPA dikurangi sizeof(uint32_t)? Karena kita hanya menghitung
         * CRC dari data parameternya saja, TANPA mengikutsertakan field 'crc32' di akhir struct.
         */
        uint16_t data_len = sizeof(SensorCalibration_t) - sizeof(uint32_t);
        uint32_t calculated_crc = Calculate_CRC32((uint8_t*)&temp_calib, data_len);

        /* 3. Validasi Integritas Data */
        if (calculated_crc == temp_calib.crc32) {
            /* Data utuh! Pindahkan ke RAM operasional (sys_calib) */
            memcpy(&sys_calib, &temp_calib, sizeof(SensorCalibration_t));
            return; /* Selesai dengan sukses */
        }
    }

    /* 4. Jika baca gagal (I2C error) ATAU CRC tidak cocok (Memori Kosong/Korup),
     * maka panggil fungsi Reset (Gunakan Default Data).
     */
    ConfigManager_ResetToDefault();
}

/**
 * @brief   Menyimpan sys_calib terkini dari RAM ke dalam EEPROM I2C.
 */
bool ConfigManager_Save(void) {
    /* 1. Kalkulasi CRC baru berdasarkan nilai aktual di RAM */
    uint16_t data_len = sizeof(SensorCalibration_t) - sizeof(uint32_t);
    sys_calib.crc32 = Calculate_CRC32((uint8_t*)&sys_calib, data_len);

    /* 2. Tulis seluruh block (termasuk CRC yang baru) ke EEPROM */
    return EEPROM_Write(&EEPROM_Ctx, EEPROM_CALIB_START_ADDR,
                             (uint8_t*)&sys_calib,
                             sizeof(SensorCalibration_t));
}

/**
 * @brief   Memaksa sistem kembali ke pengaturan pabrik.
 */
void ConfigManager_ResetToDefault(void) {
    /* MENGAPA menggunakan memcpy dari flash ke RAM?
     * Lebih efisien dan aman dari data tearing ketimbang assign variabel satu persatu.
     */
    memcpy(&sys_calib, &factory_default_calib, sizeof(SensorCalibration_t));

    /* Simpan langsung ke EEPROM agar pada boot berikutnya CRC sudah valid */
    ConfigManager_Save();
}
