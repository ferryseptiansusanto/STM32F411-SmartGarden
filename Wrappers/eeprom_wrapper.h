/**
 * @file    eeprom_wrapper.h
 * @brief   Middleware Layer 2 untuk memori EEPROM (Mendukung Multiple Devices).
 */
#ifndef EEPROM_WRAPPER_H
#define EEPROM_WRAPPER_H

#include "i2c_wrapper.h"

/**
 * @brief Struktur Context untuk EEPROM.
 * @note  Memungkinkan 1 MCU mengontrol banyak EEPROM dengan ukuran/alamat berbeda.
 */
typedef struct {
    I2C_Context *i2c_ctx;   /**< Pointer ke Bus I2C fisik (Layer 1) yang digunakan */
    uint16_t dev_address;   /**< Alamat I2C perangkat (contoh: 0xA0, 0xA2) */
    uint16_t page_size;     /**< Ukuran Halaman untuk Burst Write (contoh: 32) */
    uint32_t total_size;    /**< Total kapasitas memori dalam byte */
} EEPROM_Device_t;

// API
bool EEPROM_Init(EEPROM_Device_t *dev, I2C_Context *i2c, uint16_t addr, uint16_t page_sz, uint32_t tot_sz);
bool EEPROM_ReadBytes(EEPROM_Device_t *dev, uint16_t mem_addr, uint8_t *data, uint16_t size);
bool EEPROM_WriteBytes(EEPROM_Device_t *dev, uint16_t mem_addr, uint8_t *data, uint16_t size);

#endif // EEPROM_WRAPPER_H
