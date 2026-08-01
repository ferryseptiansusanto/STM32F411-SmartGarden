/*
 * @file    eeprom_wrapper.h
 * @brief   Middleware Layer 2 untuk memori AT24C32.
 *
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */
#include "eeprom_wrapper.h"

/**
 * @brief Inisialisasi Objek EEPROM.
 */
bool EEPROM_Init(EEPROM_Device_t *dev, I2C_Context *i2c, uint16_t addr, uint16_t page_sz, uint32_t tot_sz) {
    if (dev == NULL || i2c == NULL) return false;

    dev->i2c_ctx = i2c;
    dev->dev_address = addr;
    dev->page_size = page_sz;
    dev->total_size = tot_sz;

    return true;
}

bool EEPROM_ReadBytes(EEPROM_Device_t *dev, uint16_t mem_addr, uint8_t *data, uint16_t size) {
    if (dev == NULL || dev->i2c_ctx == NULL) return false;

    // Gunakan alamat I2C spesifik dari objek ini (Layer 1 akan mengurus Mutex-nya)
    I2C_Status status = I2C_MemRead(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                                    mem_addr, I2C_MEMADD_SIZE_16BIT, data, size, 1000);
    return (status == I2C_OK);
}

/**
 * @brief  Menulis data melintasi batas halaman (Page Boundary).
 */
bool EEPROM_WriteBytes(EEPROM_Device_t *dev, uint16_t mem_addr, uint8_t *data, uint16_t size) {
    if (dev == NULL || dev->i2c_ctx == NULL) return false;

    uint16_t bytes_written = 0;

    while (bytes_written < size) {
        // Hitung batas dinamis berdasarkan spesifikasi (page_size) milik objek ini
        uint16_t current_addr = mem_addr + bytes_written;
        uint16_t space_in_page = dev->page_size - (current_addr % dev->page_size);
        uint16_t bytes_to_write = (size - bytes_written < space_in_page) ? (size - bytes_written) : space_in_page;

        I2C_Status status = I2C_MemWrite(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                                         current_addr, I2C_MEMADD_SIZE_16BIT, &data[bytes_written], bytes_to_write, 1000);

        if (status != I2C_OK) return false;

        bytes_written += bytes_to_write;

        // Zero-Blocking Delay agar CPU bisa mengerjakan Task lain
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    return true;
}
