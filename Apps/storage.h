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
#include "device_context.h"
#define STORAGE_PORT_CS GPIOA
#define STORAGE_PIN_CS GPIO_PIN_4

typedef enum {
    STORAGE_OK = 0,
    STORAGE_ERROR,
	STORAGE_TIMEOUT
} StorageStatus_t;

extern SPI_Context SDCard_Ctx;

// Initialize STORAGE Device Parameters for SPI
void STORAGE_Init(SPI_Context *dev);

// Initialize SD card over SPI
StorageStatus_t STORAGE_Init_Cmd_Sequence(SPI_Context *dev);

// Get current initialization status
StorageStatus_t STORAGE_GetStatus(SPI_Context *dev);

// Read 'count' sectors starting from 'sector' into buffer
StorageStatus_t STORAGE_ReadBlocks(SPI_Context *dev, uint8_t *buff, uint32_t sector, uint32_t count);

// Write 'count' sectors starting from 'sector' from buffer
StorageStatus_t STORAGE_WriteBlocks(SPI_Context *dev, const uint8_t *buff, uint32_t sector, uint32_t count);

// Return total sector count (from CSD parsing)
uint32_t STORAGE_GetSectorCount(SPI_Context *dev);

bool STORAGE_IsCardPresent(SPI_Context *dev);
uint32_t STORAGE_CardSize(SPI_Context *dev);
uint32_t STORAGE_GetCapacity(SPI_Context *dev);
uint64_t STORAGE_GetSizeBytes(SPI_Context *dev);
uint32_t STORAGE_GetSectorCount(SPI_Context *dev);
bool STORAGE_IsWriteProtected(SPI_Context *dev);
void STORAGE_Deinit(SPI_Context *dev);

#endif /* INC_STORAGE_H_ */
