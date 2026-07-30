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

#include <stdbool.h>
#include "eeprom_wrapper.h"

/* Alamat memori awal di EEPROM untuk data kalibrasi */
#define EEPROM_CALIB_START_ADDR  0x0000

/** @brief Inisialisasi konfigurasi saat boot */
void ConfigManager_Init(I2C_EEPROMDevice *dev);

/** @brief Menyimpan konfigurasi dari RAM ke EEPROM (beserta update CRC) */
bool ConfigManager_Save(void);

/** @brief Mereset data di RAM dengan factory default dan menyimpannya ke EEPROM */
void ConfigManager_ResetToDefault(void);

#endif /* APPS_CONFIG_MANAGER_H_ */
