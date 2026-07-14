/*
 * config_manager.h
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */

#ifndef CONFIG_CONFIG_MANAGER_H_
#define CONFIG_CONFIG_MANAGER_H_

#include "config_data.h"
#include <stdbool.h>

// Alamat mulai penyimpanan config di EEPROM (byte 0)
// Sisakan area ini eksklusif untuk ConfigData_t, jangan dipakai keperluan lain
#define CONFIG_EEPROM_ADDR   0x0000

// Instance global config aktif — diakses seluruh modul aplikasi setelah Config_Init()
extern ConfigData_t g_config;

// Dipanggil sekali di awal main(), sebelum task lain dibuat.
// Otomatis load dari EEPROM, atau isi default + simpan kalau EEPROM belum pernah diinisialisasi/korup.
void Config_Init(void);

// Simpan ulang g_config ke EEPROM (panggil setelah user mengubah setting/jadwal)
bool Config_Save(void);

// Reset g_config ke nilai pabrik dan simpan ke EEPROM (mis. untuk fitur "factory reset")
void Config_ResetToDefault(void);

#endif /* CONFIG_MANAGER_H_ */
