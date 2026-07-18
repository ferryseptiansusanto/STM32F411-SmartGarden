/*
 * config_manager.c
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */


#include "config_manager.h"
#include "default_config.h"
#include "eeprom_wrapper.h"
#include "i2c_wrapper.h"
#include <string.h>
#include <stdio.h>

ConfigData_t g_config;

// --- CRC16-CCITT (poly 0x1021, init 0xFFFF) ---
// Dihitung atas seluruh struct KECUALI field crc16 itu sendiri (harus di posisi paling akhir struct)
static uint16_t Config_CalcCRC(const ConfigData_t *cfg) {
    const uint8_t *data = (const uint8_t *)cfg;
    size_t len = sizeof(ConfigData_t) - sizeof(cfg->crc16);
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
        }
    }
    return crc;
}

void Config_LoadDefault(ConfigData_t *cfg) {
    memset(cfg, 0, sizeof(ConfigData_t));
    cfg->magic = CONFIG_MAGIC_NUMBER;
    cfg->version = 1;
    cfg->settings.mixing_duration_ms = DEFAULT_MIXING_DURATION_MS;
    cfg->settings.valve_output_duration_ms = DEFAULT_VALVE_OUTPUT_DURATION_MS;
//    cfg->settings.flow_calibration_offset = DEFAULT_FLOW_CALIBRATION_OFFSET;
}

// Coba baca & validasi config dari EEPROM. return true kalau valid.
static bool Config_TryLoadFromEeprom(ConfigData_t *cfg) {
    if (!EEPROM_Read(&EEPROM_Ctx, CONFIG_EEPROM_ADDR, (uint8_t *)cfg, sizeof(ConfigData_t))) {
        printf("[CONFIG] Gagal baca EEPROM (I2C error)\r\n");
        return false;
    }

    if (cfg->magic != CONFIG_MAGIC_NUMBER) {
        printf("[CONFIG] EEPROM belum pernah diinisialisasi (magic mismatch)\r\n");
        return false;
    }

    uint16_t calc_crc = Config_CalcCRC(cfg);
    if (calc_crc != cfg->crc16) {
        printf("[CONFIG] Data EEPROM korup (CRC mismatch: calc=0x%04X, stored=0x%04X)\r\n",
               calc_crc, cfg->crc16);
        return false;
    }

    return true;
}

bool Config_Save(void) {
    g_config.crc16 = Config_CalcCRC(&g_config);

    bool ok = EEPROM_Write(&EEPROM_Ctx, CONFIG_EEPROM_ADDR, (uint8_t *)&g_config, sizeof(ConfigData_t));
    if (!ok) {
        printf("[CONFIG] Gagal simpan ke EEPROM\r\n");
    }
    return ok;
}

void Config_ResetToDefault(void) {
    Config_LoadDefault(&g_config);
    Config_Save();
    printf("[CONFIG] Direset ke nilai default\r\n");
}

void Config_Init(void) {
    // EEPROM AT24C32 satu bus dengan DS3231, pakai i2c1_ctx yang sama
    EEPROM_Init(&EEPROM_Ctx, &i2c1_ctx);

    if (!EEPROM_TestConnection(&EEPROM_Ctx)) {
        printf("[CONFIG] EEPROM tidak terdeteksi di I2C bus, pakai default (tidak tersimpan)\r\n");
        Config_LoadDefault(&g_config);
        return;
    }

    if (Config_TryLoadFromEeprom(&g_config)) {
        printf("[CONFIG] Config dimuat dari EEPROM (version=%u)\r\n", g_config.version);
    } else {
        printf("[CONFIG] Memuat default dan menyimpan ke EEPROM\r\n");
        Config_LoadDefault(&g_config);
        Config_Save();
    }
}
