/*
 * spi_wrapper.h
 * Refactored for Thread-Safety and Clean API
 */
#ifndef SPI_WRAPPER_H
#define SPI_WRAPPER_H

#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"
#define SPI_TIMEOUT_MS 1000

extern SPI_HandleTypeDef hspi1;

typedef enum {
    SPI_OK = 0,
    SPI_TIMEOUT,
    SPI_ERROR
} SPI_Status;

typedef enum {
    SPI_MODE_BLOCKING,
    SPI_MODE_DMA
} SPI_Mode;

typedef struct {
    SPI_HandleTypeDef *hspi;
    SemaphoreHandle_t tx_sem;
    SemaphoreHandle_t rx_sem;
    SemaphoreHandle_t txrx_sem;
    SemaphoreHandle_t mutex;     // Tambahan Mutex Wajib untuk Shared Bus SPI
} SPI_Context;

extern SPI_Context spi1_ctx;

// --- API Wrapper ---
void SPI_Init(SPI_Context *ctx);

// Transaksi Data (Mendukung 1 byte maupun buffer multi-byte)
SPI_Status SPI_Transmit(SPI_Context *ctx, SPI_Mode mode, const uint8_t *data, uint16_t size);
SPI_Status SPI_Receive(SPI_Context *ctx, SPI_Mode mode, uint8_t *data, uint16_t size);
SPI_Status SPI_TransmitReceive(SPI_Context *ctx, SPI_Mode mode, const uint8_t *txBuf, uint8_t *rxBuf, uint16_t size);

// CS Control (Dilengkapi dengan otomatisasi Mutex)
void SPI_Select_CS(SPI_Context *ctx, GPIO_TypeDef port, uint16_t pin);
void SPI_Unselect_CS(SPI_Context *ctx, GPIO_TypeDef port, uint16_t pin);

void SPI_SetSpeed(SPI_Context *ctx, uint32_t prescaler);

#endif // SPI_WRAPPER_H
