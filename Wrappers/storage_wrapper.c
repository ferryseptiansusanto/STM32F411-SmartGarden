/**
 * @file    storage_wrapper.c
 * @brief   Implementasi baca/tulis blok SD Card berbasis SPI Mutex & FreeRTOS.
 *
 *  Created on: 30 Mar 2026
 *      Author: ferry
 */

#include "storage_wrapper.h"
#include <string.h>

// Definisi Command Standar SD Card
#define CMD0    0   // GO_IDLE_STATE
#define CMD8    8   // SEND_IF_COND
#define CMD9    9   // READ_CSD
#define CMD10   10  // READ_CID
#define CMD12   12  // STOP_TRANSMISSION
#define CMD17   17  // READ_SINGLE_BLOCK
#define CMD18   18  // READ_MULTIPLE_BLOCK
#define CMD24   24  // WRITE_BLOCK
#define CMD25   25  // WRITE_MULTIPLE_BLOCK
#define CMD55   55  // APP_CMD
#define CMD58   58  // READ_OCR
#define ACMD41  41  // SD_SEND_OP_COND

static uint8_t dummy = 0xFF;

// --- HELPER INTERNAL ---

/**
 * @brief  Menunggu hingga SD Card siap (Merespon dengan 0xFF).
 * @note   MENGAPA PAKAI vTaskDelay? Mencegah FSM diam membeku saat SD Card
 * sedang sibuk menulis (flash cycle) ke sektor memori fisik.
 */
static Storage_Status sd_wait_ready(SPI_StorageDevice *dev, uint32_t timeout_ms) {
    uint32_t start_tick = xTaskGetTickCount();
    uint8_t resp;

    do {
        SPI_TransmitReceive(dev->spi_ctx, SPI_MODE_BLOCKING, &dummy, &resp, 1);
        if (resp == 0xFF) return STORAGE_OK;

        // Zero-Blocking Delay agar Task lain bisa berjalan (Fail-Safe RTOS)
        vTaskDelay(pdMS_TO_TICKS(1));
    } while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(timeout_ms));

    return STORAGE_TIMEOUT;
}

/**
 * @brief  Mengirim perintah mentah ke SD Card.
 * @note   Fungsi ini berasumsi bahwa MUTEX SPI SUDAH DIAMBIL oleh pemanggil.
 */
static uint8_t sd_send_cmd(SPI_StorageDevice *dev, uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t buf[6];
    uint8_t response;
    uint16_t retry = 0xFF;

    if (sd_wait_ready(dev, 500) != STORAGE_OK) return 0xFF;

    buf[0] = 0x40 | cmd;
    buf[1] = (uint8_t)(arg >> 24);
    buf[2] = (uint8_t)(arg >> 16);
    buf[3] = (uint8_t)(arg >> 8);
    buf[4] = (uint8_t)arg;
    buf[5] = crc;

    SPI_Transmit(dev->spi_ctx, SPI_MODE_BLOCKING, buf, 6);

    // Jika CMD12 (Stop Transmission), abaikan satu byte awal
    if (cmd == 12) SPI_TransmitReceive(dev->spi_ctx, SPI_MODE_BLOCKING, &dummy, &response, 1);

    // Tunggu response (Bit 7 harus 0)
    do {
        SPI_TransmitReceive(dev->spi_ctx, SPI_MODE_BLOCKING, &dummy, &response, 1);
    } while ((response & 0x80) && --retry);

    return (retry ? response : 0xFF);
}

// --- IMPLEMENTASI API ---

void STORAGE_SetDeviceParameter(SPI_StorageDevice *dev, SPI_Context *ctx, GPIO_TypeDef *port, uint16_t pin, SPI_Mode mode) {
    dev->spi_ctx = ctx;
    dev->cs_port = port;
    dev->cs_pin = pin;
    dev->mode = mode;
    dev->is_initialized = false;
    dev->is_sdhc = false;
    dev->sector_count = 0;
    dev->capacity_bytes = 0;
}

Storage_Status STORAGE_Init(SPI_StorageDevice *dev) {
    uint8_t response, ocr[4];
    uint32_t start_tick;

    if (dev == NULL || dev->spi_ctx == NULL) return STORAGE_ERROR;

    // 1. Slow down SPI clock for initialization
    SPI_SetSpeed(dev->spi_ctx, SPI_BAUDRATEPRESCALER_128);

    // 2. Dummy clocks (CS HIGH) untuk membangunkan SD Controller
    SPI_SendDummyClocks(dev->spi_ctx, dev->cs_port, dev->cs_pin, 10);

    // MENGUNCI BUS SPI
    SPI_Select_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);

    // CMD0: Software Reset
    if (sd_send_cmd(dev, CMD0, 0, 0x95) != 0x01) {
        SPI_Unselect_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);
        return STORAGE_ERROR;
    }

    // CMD8: Check Voltage
    response = sd_send_cmd(dev, CMD8, 0x000001AA, 0x87);
    if (response == 0x01) {
        // Echo back check
        for (int i = 0; i < 4; i++) {
            SPI_TransmitReceive(dev->spi_ctx, SPI_MODE_BLOCKING, &dummy, &ocr[i], 1);
        }

        if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
            // ACMD41: Initialize SDHC/SDXC
            start_tick = xTaskGetTickCount();
            do {
                sd_send_cmd(dev, CMD55, 0, 0xFF);
                response = sd_send_cmd(dev, ACMD41, 0x40000000, 0xFF);
                vTaskDelay(pdMS_TO_TICKS(10));
            } while (response != 0x00 && (xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(1000));

            if (response == 0x00) {
                // CMD58: Check CCS bit for SDHC
                sd_send_cmd(dev, CMD58, 0, 0xFF);
                for (int i = 0; i < 4; i++) SPI_TransmitReceive(dev->spi_ctx, SPI_MODE_BLOCKING, &dummy, &ocr[i], 1);
                if (ocr[0] & 0x40) dev->is_sdhc = true;
            }
        }
    }

    // LEPASKAN BUS SPI
    SPI_Unselect_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);

    if (response == 0x00) {
        dev->is_initialized = true;
        SPI_SetSpeed(dev->spi_ctx, SPI_BAUDRATEPRESCALER_4); // Speed up SPI

        // Tarik Data Kapasitas secara otomatis setelah berhasil Init
        dev->sector_count = STORAGE_GetSectorCount(dev);
        dev->capacity_bytes = (uint64_t)dev->sector_count * SECTOR_SIZE;

        return STORAGE_OK;
    }

    return STORAGE_ERROR;
}

Storage_Status STORAGE_ReadCSD(SPI_StorageDevice *dev, uint8_t *csd) {
    if (!dev->is_initialized) return STORAGE_ERROR;

    SPI_Select_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);

    if (sd_send_cmd(dev, CMD9, 0, 0x01) != 0x00) {
        SPI_Unselect_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);
        return STORAGE_ERROR;
    }

    uint8_t token;
    uint32_t start_tick = xTaskGetTickCount();
    do {
        SPI_TransmitReceive(dev->spi_ctx, SPI_MODE_BLOCKING, &dummy, &token, 1);
        if (token == 0xFE) break;
    } while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(100));

    if (token != 0xFE) {
        SPI_Unselect_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);
        return STORAGE_TIMEOUT;
    }

    for (int i = 0; i < 16; i++) {
        SPI_TransmitReceive(dev->spi_ctx, SPI_MODE_BLOCKING, &dummy, &csd[i], 1);
    }

    // Buang 2 byte CRC
    SPI_TransmitReceive(dev->spi_ctx, SPI_MODE_BLOCKING, &dummy, &token, 1);
    SPI_TransmitReceive(dev->spi_ctx, SPI_MODE_BLOCKING, &dummy, &token, 1);

    SPI_Unselect_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);
    return STORAGE_OK;
}

uint32_t STORAGE_GetSectorCount(SPI_StorageDevice *dev) {
    uint8_t csd[16];
    if (STORAGE_ReadCSD(dev, csd) != STORAGE_OK) return 0;

    uint8_t csd_structure = (csd[0] >> 6) & 0x03;
    uint32_t capacity = 0;

    if (csd_structure == 0) { // CSD v1.0 (Standard Capacity)
        uint32_t c_size = ((csd[6] & 0x03) << 10) | ((uint32_t)csd[7] << 2) | ((csd[8] & 0xC0) >> 6);
        uint8_t c_size_mult = ((csd[9] & 0x03) << 1) | ((csd[10] & 0x80) >> 7);
        uint8_t read_bl_len = csd[5] & 0x0F;

        uint32_t block_len = 1UL << read_bl_len;
        uint32_t mult = 1UL << (c_size_mult + 2);

        capacity = ((c_size + 1) * mult * block_len) / SECTOR_SIZE;
    } else if (csd_structure == 1) { // CSD v2.0 (SDHC/SDXC)
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint32_t)csd[8] << 8) | (uint32_t)csd[9];
        capacity = (c_size + 1) * 1024UL; // Sudah dalam satuan sektor 512-byte
    }

    return capacity;
}

uint64_t STORAGE_GetSizeBytes(SPI_StorageDevice *dev) {
    return dev->capacity_bytes;
}

Storage_Status STORAGE_GetStatus(SPI_StorageDevice *dev) {
    return dev->is_initialized ? STORAGE_OK : STORAGE_ERROR;
}

Storage_Status STORAGE_ReadBlocks(SPI_StorageDevice *dev, uint8_t *buff, uint32_t sector, uint32_t count) {
    if (!dev->is_initialized) return STORAGE_ERROR;

    uint32_t addr = dev->is_sdhc ? sector : sector * SECTOR_SIZE;
    uint8_t cmd = (count > 1) ? CMD18 : CMD17;

    SPI_Select_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);

    if (sd_send_cmd(dev, cmd, addr, 0x01) != 0x00) {
        SPI_Unselect_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);
        return STORAGE_ERROR;
    }

    for (uint32_t i = 0; i < count; i++) {
        uint8_t token;
        uint32_t start_tick = xTaskGetTickCount();
        do {
            SPI_TransmitReceive(dev->spi_ctx, SPI_MODE_BLOCKING, &dummy, &token, 1);
            if (token == 0xFE) break;
            vTaskDelay(pdMS_TO_TICKS(1)); // Zero-Blocking tunggu data
        } while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(200));

        if (token != 0xFE) {
            SPI_Unselect_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);
            return STORAGE_TIMEOUT;
        }

        // Baca 512 Byte (Support DMA)
        SPI_Receive(dev->spi_ctx, dev->mode, buff + (i * SECTOR_SIZE), SECTOR_SIZE);

        // Buang CRC
        SPI_Receive(dev->spi_ctx, SPI_MODE_BLOCKING, buff, 2);
    }

    if (count > 1) sd_send_cmd(dev, CMD12, 0, 0x01); // Stop Transmission

    SPI_Unselect_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);
    return STORAGE_OK;
}

Storage_Status STORAGE_WriteBlocks(SPI_StorageDevice *dev, const uint8_t *buff, uint32_t sector, uint32_t count) {
    if (!dev->is_initialized) return STORAGE_ERROR;

    uint32_t addr = dev->is_sdhc ? sector : sector * SECTOR_SIZE;
    uint8_t cmd = (count > 1) ? CMD25 : CMD24;

    SPI_Select_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);

    if (sd_send_cmd(dev, cmd, addr, 0x01) != 0x00) {
        SPI_Unselect_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);
        return STORAGE_ERROR;
    }

    for (uint32_t i = 0; i < count; i++) {
        uint8_t token = (count > 1) ? 0xFC : 0xFE;
        SPI_Transmit(dev->spi_ctx, SPI_MODE_BLOCKING, &token, 1);

        // Tulis 512 Byte (Support DMA)
        SPI_Transmit(dev->spi_ctx, dev->mode, buff + (i * SECTOR_SIZE), SECTOR_SIZE);

        // Dummy CRC
        uint8_t crc[2] = {0xFF, 0xFF};
        SPI_Transmit(dev->spi_ctx, SPI_MODE_BLOCKING, crc, 2);

        // Cek Respon Penerimaan Data
        uint8_t resp;
        SPI_TransmitReceive(dev->spi_ctx, SPI_MODE_BLOCKING, &dummy, &resp, 1);
        if ((resp & 0x1F) != 0x05) {
            SPI_Unselect_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);
            return STORAGE_ERROR;
        }

        // Tunggu SD Card sibuk menulis fisik
        if (sd_wait_ready(dev, 500) != STORAGE_OK) {
            SPI_Unselect_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);
            return STORAGE_TIMEOUT;
        }
    }

    if (count > 1) {
        uint8_t token = 0xFD; // Stop token
        SPI_Transmit(dev->spi_ctx, SPI_MODE_BLOCKING, &token, 1);
        sd_wait_ready(dev, 500);
    }

    SPI_Unselect_CS(dev->spi_ctx, dev->cs_port, dev->cs_pin);
    return STORAGE_OK;
}

bool STORAGE_IsWriteProtected(SPI_StorageDevice *dev) {
    return false; // Hardware MicroSD jarang memiliki pin ini
}

void STORAGE_Deinit(SPI_StorageDevice *dev) {
    dev->is_initialized = false;
    dev->is_sdhc = false;
}
