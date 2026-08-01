/**
 * @file    storage_wrapper.h
 * @brief   Middleware Layer 2 untuk Blok Penyimpanan Fisik (SD Card via SPI).
 * @note    Didesain dengan pola Object Context. Mendukung CSD Parsing & Multi-Block.
 *
 *  Created on: 30 Mar 2026
 *      Author: ferry
 */

#ifndef STORAGE_WRAPPER_H
#define STORAGE_WRAPPER_H

#include "spi_wrapper.h" // Menggunakan Layer 1 (Akses Bus & Mutex)
#include <stdbool.h>
#include <stdint.h>

#define SECTOR_SIZE 512U

// Status hasil operasi tingkat blok
typedef enum {
    STORAGE_OK = 0,
    STORAGE_ERROR,
    STORAGE_WRPRT,   // Write Protected
    STORAGE_NOTRDY,  // Not Ready / Timeout
    STORAGE_TIMEOUT
} Storage_Status;

/**
 * @brief Struktur Context untuk Objek SD Card.
 */
typedef struct {
    SPI_Context *spi_ctx;       /**< Pointer ke Bus SPI fisik (Layer 1) */
    GPIO_TypeDef *cs_port;      /**< Port GPIO untuk pin Chip Select (CS) */
    uint16_t cs_pin;            /**< Pin GPIO untuk Chip Select (CS) */
    SPI_Mode mode;              /**< Mode Transfer (BLOCKING / DMA) */

    bool is_initialized;        /**< Flag status inisialisasi */
    bool is_sdhc;               /**< Flag tipe kartu (Standard atau High Capacity) */

    uint32_t sector_count;      /**< Total sektor (didapat dari parsing CSD) */
    uint64_t capacity_bytes;    /**< Total kapasitas dalam Bytes */
} SPI_StorageDevice;

// --- API Inisialisasi & Parameter ---
void STORAGE_SetDeviceParameter(SPI_StorageDevice *dev, SPI_Context *ctx, GPIO_TypeDef *port, uint16_t pin, SPI_Mode mode);
Storage_Status STORAGE_Init(SPI_StorageDevice *dev);
void STORAGE_Deinit(SPI_StorageDevice *dev);
Storage_Status STORAGE_GetStatus(SPI_StorageDevice *dev);

// --- API Operasi Blok (Digunakan oleh diskio.c) ---
Storage_Status STORAGE_ReadBlocks(SPI_StorageDevice *dev, uint8_t *buff, uint32_t sector, uint32_t count);
Storage_Status STORAGE_WriteBlocks(SPI_StorageDevice *dev, const uint8_t *buff, uint32_t sector, uint32_t count);

// --- API Identitas & Kapasitas (Hasil Parsing CSD/CID) ---
Storage_Status STORAGE_ReadCID(SPI_StorageDevice *dev, uint8_t *cid);
Storage_Status STORAGE_ReadCSD(SPI_StorageDevice *dev, uint8_t *csd);
uint32_t STORAGE_GetSectorCount(SPI_StorageDevice *dev);
uint64_t STORAGE_GetSizeBytes(SPI_StorageDevice *dev);
bool STORAGE_IsWriteProtected(SPI_StorageDevice *dev);

#endif // STORAGE_WRAPPER_H
