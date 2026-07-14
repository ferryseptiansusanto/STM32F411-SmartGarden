/*
 * eeprom_wrapper.c
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */


/*
 * eeprom_wrapper.c
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */

#include "eeprom_wrapper.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

I2C_EEPROMDevice EEPROM_Ctx;

void EEPROM_Init(I2C_EEPROMDevice *dev, I2C_Context *ctx) {
    dev->ctx = ctx;
    dev->address = EEPROM_ADDR;
    dev->sizereg = I2C_MEMADD_SIZE_16BIT;   // beda dengan DS3231 (8-bit)
    dev->mode = I2C_MODE_IT;
}

bool EEPROM_TestConnection(I2C_EEPROMDevice *dev) {
    uint8_t buf;
    return (I2C_MemRead(dev->ctx, dev->address, dev->mode, 0x0000, dev->sizereg, &buf, 1, 50) == I2C_OK);
}

bool EEPROM_Read(I2C_EEPROMDevice *dev, uint16_t mem_addr, uint8_t *buf, uint16_t size) {
    if (dev == NULL || buf == NULL) return false;
    if ((uint32_t)mem_addr + size > EEPROM_SIZE_BYTES) return false;

    // AT24C32 mendukung sequential read melewati batas halaman tanpa masalah,
    // jadi read tidak perlu dipecah seperti write
    I2C_Status st = I2C_MemRead(dev->ctx, dev->address, dev->mode, mem_addr, dev->sizereg, buf, size, 200);
    if (st != I2C_OK) {
        printf("[EEPROM] Read gagal @0x%04X (%u byte), status=%d\r\n", mem_addr, size, st);
        return false;
    }
    return true;
}

bool EEPROM_Write(I2C_EEPROMDevice *dev, uint16_t mem_addr, const uint8_t *buf, uint16_t size) {
    if (dev == NULL || buf == NULL) return false;
    if ((uint32_t)mem_addr + size > EEPROM_SIZE_BYTES) return false;

    uint16_t written = 0;

    while (written < size) {
        uint16_t current_addr = mem_addr + written;

        // Hitung sisa ruang di halaman saat ini supaya penulisan tidak wrap-around
        uint16_t page_offset   = current_addr % EEPROM_PAGE_SIZE;
        uint16_t space_in_page = EEPROM_PAGE_SIZE - page_offset;
        uint16_t remaining     = size - written;
        uint16_t chunk_size    = (remaining < space_in_page) ? remaining : space_in_page;

        I2C_Status st = I2C_MemWrite(dev->ctx, dev->address, dev->mode,
                                      current_addr, dev->sizereg,
                                      (uint8_t *)(buf + written), chunk_size, 200);
        if (st != I2C_OK) {
            printf("[EEPROM] Write gagal @0x%04X (%u byte), status=%d\r\n", current_addr, chunk_size, st);
            return false;
        }

        // Wajib delay, EEPROM butuh waktu commit internal sebelum transaksi berikutnya
        vTaskDelay(pdMS_TO_TICKS(EEPROM_WRITE_DELAY_MS));

        written += chunk_size;
    }

    return true;
}
