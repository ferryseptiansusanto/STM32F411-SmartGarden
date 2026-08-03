/*
 * config_manager.h
 *
 * @file    config_manager.h
 * @brief   API untuk inisialisasi, penyimpanan, dan pemulihan konfigurasi sistem.
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */

#ifndef APPS_CONFIG_MANAGER_H_
#define APPS_CONFIG_MANAGER_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Memuat konfigurasi dari EEPROM. Melakukan validasi CRC32.
 * Jika gagal/korup, otomatis memuat nilai dari default_config.h.
 * @return true jika data valid dari EEPROM, false jika memuat default.
 */
bool Config_Init(void);

/**
 * @brief Mengkalkulasi ulang CRC32 dan menyimpan struktur ke EEPROM.
 * @return true jika WriteBytes berhasil (Zero-Blocking).
 */
bool Config_Save(void);

/**
 * @brief Reset konfigurasi ke factory default secara manual.
 * @note  Pastikan memanggil Config_Save() setelah fungsi ini jika ingin permanen.
 */
void Config_LoadDefault(void);

#endif /* APPS_CONFIG_MANAGER_H_ */
