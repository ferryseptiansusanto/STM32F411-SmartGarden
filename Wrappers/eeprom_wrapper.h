/*
 * eeprom_wrapper.h
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */

#ifndef EEPROM_WRAPPER_H_
#define EEPROM_WRAPPER_H_

#include "main.h"
#include <stdint.h>
#include <stdbool.h>
#include "i2c_wrapper.h"   // gunakan wrapper, bukan langsung HAL

// AT24C32 pada modul DS3231 breakout. A0/A1/A2 pull-up = 0x57 (bukan 0x50 standalone)
// Kalau EEPROM ternyata di alamat lain, konfirmasi dulu pakai I2C_ScanBus()
#define EEPROM_ADDR          (0x57 << 1)

#define EEPROM_PAGE_SIZE     32u     // byte per halaman AT24C32, wajib untuk write splitting
#define EEPROM_SIZE_BYTES    4096u   // AT24C32 = 32Kbit = 4KB
#define EEPROM_WRITE_DELAY_MS 5u     // write cycle time internal EEPROM (datasheet: max 5ms)

typedef struct {
    I2C_Context *ctx;
    uint16_t address;
    uint32_t sizereg;   // I2C_MEMADD_SIZE_16BIT untuk AT24C32
    I2C_Mode mode;
} I2C_EEPROMDevice;

extern I2C_EEPROMDevice EEPROM_Ctx;

// API
void EEPROM_Init(uint16_t dev_addr, I2C_EEPROMDevice *dev, I2C_Context *ctx);
bool EEPROM_TestConnection(I2C_EEPROMDevice *dev);

// mem_addr: alamat byte di dalam EEPROM (0 .. EEPROM_SIZE_BYTES-1)
bool EEPROM_Read(I2C_EEPROMDevice *dev, uint16_t mem_addr, uint8_t *buf, uint16_t size);
bool EEPROM_Write(I2C_EEPROMDevice *dev, uint16_t mem_addr, const uint8_t *buf, uint16_t size);

#endif /* EEPROM_WRAPPER_H_ */
