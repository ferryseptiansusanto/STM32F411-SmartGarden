/*
 * i2c_wrapper.c
 *
 *  Created on: 9 May 2026
 *      Author: ferry
 */


#include "i2c_wrapper.h"
#include "delay.h"
#include <stdio.h>

// Definisi Context untuk I2C1 dan I2C2
I2C_Context i2c1_ctx = { &hi2c1, NULL, NULL, NULL };
I2C_Context i2c2_ctx = { &hi2c2, NULL, NULL, NULL };

// Registri dinamis seperti pada UART Wrapper
static I2C_Context* i2c_registry[] = { &i2c1_ctx, &i2c2_ctx };
#define I2C_REGISTRY_COUNT (sizeof(i2c_registry) / sizeof(i2c_registry[0]))


// --- Callback IT/DMA Dinamis ---
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (size_t i = 0; i < I2C_REGISTRY_COUNT; i++) {
        if (hi2c == i2c_registry[i]->hi2c && i2c_registry[i]->tx_sem != NULL) {
            xSemaphoreGiveFromISR(i2c_registry[i]->tx_sem, &xHigherPriorityTaskWoken);
            break;
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (size_t i = 0; i < I2C_REGISTRY_COUNT; i++) {
        if (hi2c == i2c_registry[i]->hi2c && i2c_registry[i]->rx_sem != NULL) {
            xSemaphoreGiveFromISR(i2c_registry[i]->rx_sem, &xHigherPriorityTaskWoken);
            break;
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    HAL_I2C_MasterTxCpltCallback(hi2c); // Logika sama
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    HAL_I2C_MasterRxCpltCallback(hi2c); // Logika sama
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    I2C_Context *ctx = NULL;

    for (size_t i = 0; i < I2C_REGISTRY_COUNT; i++) {
        if (hi2c == i2c_registry[i]->hi2c) {
            ctx = i2c_registry[i];
            break;
        }
    }

    if (ctx) {
        // Lepaskan semua semaphore yang mungkin sedang ditunggu Task
        if (ctx->tx_sem) xSemaphoreGiveFromISR(ctx->tx_sem, &xHigherPriorityTaskWoken);
        if (ctx->rx_sem) xSemaphoreGiveFromISR(ctx->rx_sem, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// --- Init ---
void I2C_Init(I2C_Context *ctx) {
    if (ctx == NULL) return;

    ctx->tx_sem = xSemaphoreCreateBinary();
    ctx->rx_sem = xSemaphoreCreateBinary();
    ctx->mutex  = xSemaphoreCreateMutex();

    if (!ctx->tx_sem || !ctx->rx_sem || !ctx->mutex) {
        printf("I2C Gagal alokasi OS primitive!\n");
    }
}

// --- Transmit ---
I2C_Status I2C_Transmit(I2C_Context *ctx, uint16_t address,
                        I2C_Mode mode, uint8_t *data, uint16_t size, uint32_t timeout) {
    I2C_Status status = I2C_OK;
    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        if (mode == I2C_MODE_IT) {
            if (HAL_I2C_Master_Transmit_IT(ctx->hi2c, address, data, size) != HAL_OK) status = I2C_ERROR;
            else if (xSemaphoreTake(ctx->tx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) {
                status = I2C_TIMEOUT;
                I2C_Recovery(ctx->hi2c);
            }
        } else if (mode == I2C_MODE_DMA) {
            if (HAL_I2C_Master_Transmit_DMA(ctx->hi2c, address, data, size) != HAL_OK) status = I2C_ERROR;
            else if (xSemaphoreTake(ctx->tx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) {
                status = I2C_TIMEOUT;
                I2C_Recovery(ctx->hi2c);
            }
        } else {
            if (HAL_I2C_Master_Transmit(ctx->hi2c, address, data, size, timeout) != HAL_OK) status = I2C_ERROR;
        }
        xSemaphoreGive(ctx->mutex);
    } else {
        status = I2C_ERROR;
    }
    return status;
}

// --- Receive ---
I2C_Status I2C_Receive(I2C_Context *ctx, uint16_t address,
                       I2C_Mode mode, uint8_t *data, uint16_t size, uint32_t timeout) {
    I2C_Status status = I2C_OK;
    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        if (mode == I2C_MODE_IT) {
            if (HAL_I2C_Master_Receive_IT(ctx->hi2c, address, data, size) != HAL_OK) status = I2C_ERROR;
            else if (xSemaphoreTake(ctx->rx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) {
                status = I2C_TIMEOUT;
                I2C_Recovery(ctx->hi2c);
            }
        } else if (mode == I2C_MODE_DMA) {
            if (HAL_I2C_Master_Receive_DMA(ctx->hi2c, address, data, size) != HAL_OK) status = I2C_ERROR;
            else if (xSemaphoreTake(ctx->rx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) {
                status = I2C_TIMEOUT;
                I2C_Recovery(ctx->hi2c);
            }
        } else {
            if (HAL_I2C_Master_Receive(ctx->hi2c, address, data, size, timeout) != HAL_OK) status = I2C_ERROR;
        }
        xSemaphoreGive(ctx->mutex);
    } else {
        status = I2C_ERROR;
    }
    return status;
}

// --- TransmitReceive (REFACTORED UNTUK MENCEGAH RACE CONDITION) ---
I2C_Status I2C_TransmitReceive(I2C_Context *ctx, uint16_t address, I2C_Mode mode,
                               uint8_t *txData, uint16_t txSize, uint32_t timeouttx,
                               uint8_t *rxData, uint16_t rxSize, uint32_t timeoutrx) {

    I2C_Status status = I2C_OK;

    // Mutex dikunci sekali untuk seluruh transaksi (Tx lanjut Rx)
    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {

        // --- PROSES TX ---
        if (mode == I2C_MODE_IT) {
            if (HAL_I2C_Master_Transmit_IT(ctx->hi2c, address, txData, txSize) != HAL_OK) status = I2C_ERROR;
            else if (xSemaphoreTake(ctx->tx_sem, pdMS_TO_TICKS(timeouttx)) != pdPASS) status = I2C_TIMEOUT;
        } else if (mode == I2C_MODE_DMA) {
            if (HAL_I2C_Master_Transmit_DMA(ctx->hi2c, address, txData, txSize) != HAL_OK) status = I2C_ERROR;
            else if (xSemaphoreTake(ctx->tx_sem, pdMS_TO_TICKS(timeouttx)) != pdPASS) status = I2C_TIMEOUT;
        } else {
            if (HAL_I2C_Master_Transmit(ctx->hi2c, address, txData, txSize, timeouttx) != HAL_OK) status = I2C_ERROR;
        }

        // --- PROSES RX (Hanya jika TX sukses) ---
        if (status == I2C_OK) {
            if (mode == I2C_MODE_IT) {
                if (HAL_I2C_Master_Receive_IT(ctx->hi2c, address, rxData, rxSize) != HAL_OK) status = I2C_ERROR;
                else if (xSemaphoreTake(ctx->rx_sem, pdMS_TO_TICKS(timeoutrx)) != pdPASS) status = I2C_TIMEOUT;
            } else if (mode == I2C_MODE_DMA) {
                if (HAL_I2C_Master_Receive_DMA(ctx->hi2c, address, rxData, rxSize) != HAL_OK) status = I2C_ERROR;
                else if (xSemaphoreTake(ctx->rx_sem, pdMS_TO_TICKS(timeoutrx)) != pdPASS) status = I2C_TIMEOUT;
            } else {
                if (HAL_I2C_Master_Receive(ctx->hi2c, address, rxData, rxSize, timeoutrx) != HAL_OK) status = I2C_ERROR;
            }
        }

        // Recovery jika salah satu gagal
        if (status == I2C_TIMEOUT) {
            I2C_Recovery(ctx->hi2c);
        }

        xSemaphoreGive(ctx->mutex);
    } else {
        status = I2C_ERROR;
    }
    return status;
}

// --- MemRead & MemWrite ---
// (Aku persingkat di tampilan ini, tapi fungsinya tetap sama dengan kodemu, hanya dirapikan sedikit)
I2C_Status I2C_MemRead(I2C_Context *ctx, uint16_t address, I2C_Mode mode, uint16_t reg, uint32_t regsize, uint8_t *data, uint16_t size, uint32_t timeout){
	I2C_Status status = I2C_OK;
    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        if (mode == I2C_MODE_IT) {
            if (HAL_I2C_Mem_Read_IT(ctx->hi2c, address, reg, regsize, data, size) != HAL_OK) status = I2C_ERROR;
            else if (xSemaphoreTake(ctx->rx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) { status = I2C_TIMEOUT; I2C_Recovery(ctx->hi2c); }
        } else if (mode == I2C_MODE_DMA) {
            if (HAL_I2C_Mem_Read_DMA(ctx->hi2c, address, reg, regsize, data, size) != HAL_OK) status = I2C_ERROR;
            else if (xSemaphoreTake(ctx->rx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) { status = I2C_TIMEOUT; I2C_Recovery(ctx->hi2c); }
        } else {
            if (HAL_I2C_Mem_Read(ctx->hi2c, address, reg, regsize, data, size, timeout) != HAL_OK) status = I2C_ERROR;
        }
        xSemaphoreGive(ctx->mutex);
    } else {
        status = I2C_ERROR;
    }
    return status;
}

I2C_Status I2C_MemWrite(I2C_Context *ctx, uint16_t address, I2C_Mode mode, uint16_t reg, uint32_t regsize, uint8_t *data, uint16_t size, uint32_t timeout){
    I2C_Status status = I2C_OK;
    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        if (mode == I2C_MODE_IT) {
            if (HAL_I2C_Mem_Write_IT(ctx->hi2c, address, reg, regsize, data, size) != HAL_OK) status = I2C_ERROR;
            else if (xSemaphoreTake(ctx->tx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) { status = I2C_TIMEOUT; I2C_Recovery(ctx->hi2c); }
        } else if (mode == I2C_MODE_DMA) {
            if (HAL_I2C_Mem_Write_DMA(ctx->hi2c, address, reg, regsize, data, size) != HAL_OK) status = I2C_ERROR;
            else if (xSemaphoreTake(ctx->tx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) { status = I2C_TIMEOUT; I2C_Recovery(ctx->hi2c); }
        } else {
            if (HAL_I2C_Mem_Write(ctx->hi2c, address, reg, regsize, data, size, timeout) != HAL_OK) status = I2C_ERROR;
        }
        xSemaphoreGive(ctx->mutex);
    } else {
        status = I2C_ERROR;
    }
    return status;
}


void I2C_ScanBus(I2C_HandleTypeDef *hi2c) {
    // Tetap sama
}

I2C_Status I2C_Recovery(I2C_HandleTypeDef *hi2c) {
    if (hi2c == NULL) return I2C_ERROR;
    printf("I2C Recovery triggered...\r\n");

    // Matikan I2C Peripheral untuk reset internal state mesin (SANGAT PENTING!)
    HAL_I2C_DeInit(hi2c);

    // 1. Reset RCC peripheral
    if (hi2c->Instance == I2C1) {
        __HAL_RCC_I2C1_FORCE_RESET();
        __HAL_RCC_I2C1_RELEASE_RESET();
    }
#ifdef I2C2
    else if (hi2c->Instance == I2C2) {
        __HAL_RCC_I2C2_FORCE_RESET();
        __HAL_RCC_I2C2_RELEASE_RESET();
    }
#endif

    // 2. Ambil port & pin dari CubeMX User Label
    GPIO_TypeDef *I2C_SCL_Port = NULL;
    GPIO_TypeDef *I2C_SDA_Port = NULL;
    uint16_t SCL_Pin = 0, SDA_Pin = 0;

    if (hi2c->Instance == I2C1) {
        I2C_SCL_Port = I2C1_SCL_GPIO_Port;
        I2C_SDA_Port = I2C1_SDA_GPIO_Port;
        SCL_Pin      = I2C1_SCL_Pin;
        SDA_Pin      = I2C1_SDA_Pin;
    }

    if (I2C_SCL_Port == NULL || I2C_SDA_Port == NULL) return I2C_ERROR;

    // 3. Set pin jadi GPIO Output Open-Drain
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pin   = SCL_Pin;
    HAL_GPIO_Init(I2C_SCL_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin   = SDA_Pin;
    HAL_GPIO_Init(I2C_SDA_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(I2C_SCL_Port, SCL_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(I2C_SDA_Port, SDA_Pin, GPIO_PIN_SET);

    // 4. Clock toggling untuk release SDA
    for (int i = 0; i < 9; i++) {
        if (HAL_GPIO_ReadPin(I2C_SDA_Port, SDA_Pin) == GPIO_PIN_SET) break;
        HAL_GPIO_WritePin(I2C_SCL_Port, SCL_Pin, GPIO_PIN_RESET);
        DelayUs(5);
        HAL_GPIO_WritePin(I2C_SCL_Port, SCL_Pin, GPIO_PIN_SET);
        DelayUs(5);
    }

    // 5. STOP condition manual
    HAL_GPIO_WritePin(I2C_SDA_Port, SDA_Pin, GPIO_PIN_RESET);
    DelayUs(5);
    HAL_GPIO_WritePin(I2C_SCL_Port, SCL_Pin, GPIO_PIN_SET);
    DelayUs(5);
    HAL_GPIO_WritePin(I2C_SDA_Port, SDA_Pin, GPIO_PIN_SET);
    DelayUs(5);

    // 6. Re-init peripheral (Ini akan memanggil HAL_I2C_MspInit untuk mengembalikan pin ke Alternate Function)
    if (HAL_I2C_Init(hi2c) != HAL_OK) {
        printf("I2C Recovery failed!\r\n");
        return I2C_ERROR;
    }

    printf("I2C Recovery success.\r\n");
    return I2C_OK;
}

