/*
 * i2c_wrapper.h
 *
 *  Created on: 9 May 2026
 *      Author: ferry
 */

#ifndef I2C_WRAPPER_H_
#define I2C_WRAPPER_H_


#include "main.h"
#define I2C_TIMEOUT_MS 1000
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdbool.h>

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

typedef enum {
    I2C_OK = 0,
    I2C_TIMEOUT,
    I2C_ERROR
} I2C_Status;

typedef enum {
    I2C_MODE_BLOCKING,
    I2C_MODE_IT,
    I2C_MODE_DMA
} I2C_Mode;

typedef struct {
    I2C_HandleTypeDef *hi2c;
    SemaphoreHandle_t tx_sem;
    SemaphoreHandle_t rx_sem;
    SemaphoreHandle_t mutex;
} I2C_Context;

extern I2C_Context i2c1_ctx;
extern I2C_Context i2c2_ctx;

// --- API Wrapper ---
void I2C_Init(I2C_Context *ctx);
I2C_Status I2C_Transmit(I2C_Context *ctx, uint16_t address, I2C_Mode mode, uint8_t *data, uint16_t size, uint32_t timeout);
I2C_Status I2C_Receive(I2C_Context *ctx, uint16_t address, I2C_Mode mode, uint8_t *data, uint16_t size, uint32_t timeout);
I2C_Status I2C_TransmitReceive(I2C_Context *ctx, uint16_t address, I2C_Mode mode, uint8_t *txData, uint16_t txSize, uint32_t timeouttx, uint8_t *rxData, uint16_t rxSize, uint32_t timeoutrx);
I2C_Status I2C_MemRead(I2C_Context *ctx, uint16_t address, I2C_Mode mode, uint16_t reg, uint32_t regsize, uint8_t *data, uint16_t size, uint32_t timeout);
I2C_Status I2C_MemWrite(I2C_Context *ctx, uint16_t address, I2C_Mode mode, uint16_t reg, uint32_t regsize, uint8_t *data, uint16_t size, uint32_t timeout);
void I2C_ScanBus(I2C_HandleTypeDef *hi2c);
I2C_Status I2C_Recovery(I2C_HandleTypeDef *hi2c);

#endif /* I2C_WRAPPER_H_ */
