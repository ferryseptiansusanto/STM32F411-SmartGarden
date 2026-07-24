/*
 * storage.h
 *
 *  Created on: 30 Mar 2026
 *      Author: ferry
 */

#ifndef INC_STORAGE_H_
#define INC_STORAGE_H_

#include <stdint.h>
#include "stdbool.h"
#include "stdio.h"
#include "spi_wrapper.h"

typedef enum {
    STORAGE_OK = 0,
    STORAGE_ERROR,
	STORAGE_TIMEOUT
} StorageStatus_t;

typedef struct {
	SPI_Context *ctx;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    SPI_Mode mode;
    bool is_card_present;
    bool is_initialized;
    bool is_sdhc;
    uint32_t sector_count;
} SPI_StorageDevice;

// Initialize STORAGE Device Parameters for SPI
void STORAGE_SetDeviceParameter(SPI_StorageDevice *dev, SPI_Context *ctx, GPIO_TypeDef *port, uint16_t pin, SPI_Mode mode);

// Initialize SD card over SPI
StorageStatus_t STORAGE_Init(SPI_StorageDevice *dev);

// Get current initialization status
StorageStatus_t STORAGE_GetStatus(SPI_StorageDevice *dev);

// Read 'count' sectors starting from 'sector' into buffer
StorageStatus_t STORAGE_ReadBlocks(SPI_StorageDevice *dev, uint8_t *buff, uint32_t sector, uint32_t count);

// Write 'count' sectors starting from 'sector' from buffer
StorageStatus_t STORAGE_WriteBlocks(SPI_StorageDevice *dev, const uint8_t *buff, uint32_t sector, uint32_t count);

// Return total sector count (from CSD parsing)
uint32_t STORAGE_GetSectorCount(SPI_StorageDevice *dev);

bool STORAGE_IsCardPresent(SPI_StorageDevice *dev);
uint32_t STORAGE_CardSize(SPI_StorageDevice *dev);
uint32_t STORAGE_GetCapacity(SPI_StorageDevice *dev);
uint64_t STORAGE_GetSizeBytes(SPI_StorageDevice *dev);
uint32_t STORAGE_GetSectorCount(SPI_StorageDevice *dev);
bool STORAGE_IsWriteProtected(SPI_StorageDevice *dev);
void STORAGE_Deinit(SPI_StorageDevice *dev);

#endif /* INC_STORAGE_H_ */
