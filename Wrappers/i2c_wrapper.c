/*
 * i2c_wrapper.c
 *
 *  Created on: 9 May 2026
 *      Author: ferry
 */


#include "i2c_wrapper.h"
#include "delay.h"
#include <stdio.h>

I2C_Context i2c1_ctx = { &hi2c1, NULL, NULL };

// --- Callback IT/DMA ---
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    // cari context yang sesuai
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hi2c == i2c1_ctx.hi2c) xSemaphoreGiveFromISR(i2c1_ctx.tx_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hi2c == i2c1_ctx.hi2c) xSemaphoreGiveFromISR(i2c1_ctx.rx_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hi2c == i2c1_ctx.hi2c) xSemaphoreGiveFromISR(i2c1_ctx.tx_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hi2c == i2c1_ctx.hi2c) xSemaphoreGiveFromISR(i2c1_ctx.rx_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
	  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	    // ambil context yang sesuai
	    I2C_Context *ctx = NULL;
	    if (hi2c == i2c1_ctx.hi2c) ctx = &i2c1_ctx;

	    if (ctx) {
	        switch (hi2c->ErrorCode) {
	            case HAL_I2C_ERROR_AF:   // NACK → biasanya TX gagal
	            case HAL_I2C_ERROR_BERR: // Bus error → TX gagal
	            case HAL_I2C_ERROR_OVR:  // Overrun → TX gagal
	                xSemaphoreGiveFromISR(ctx->tx_sem, &xHigherPriorityTaskWoken);
	                break;

	            case HAL_I2C_ERROR_ARLO: // Arbitration lost → bisa TX/RX
	            case HAL_I2C_ERROR_DMA:  // DMA error → bisa TX/RX
	                xSemaphoreGiveFromISR(ctx->tx_sem, &xHigherPriorityTaskWoken);
	                xSemaphoreGiveFromISR(ctx->rx_sem, &xHigherPriorityTaskWoken);
	                break;

	            default:
	                // kalau tidak jelas, lepaskan keduanya supaya task tidak hang
	                xSemaphoreGiveFromISR(ctx->tx_sem, &xHigherPriorityTaskWoken);
	                xSemaphoreGiveFromISR(ctx->rx_sem, &xHigherPriorityTaskWoken);
	                break;
	        }
	    }

	    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// --- Init ---
void I2C_Init(I2C_Context *ctx){
    // buat semaphore untuk sinkronisasi DMA
	ctx->tx_sem   = xSemaphoreCreateBinary();
	ctx->rx_sem   = xSemaphoreCreateBinary();
	ctx->mutex = xSemaphoreCreateMutex();
    // cek alokasi
    if (!ctx->tx_sem || !ctx->rx_sem || !ctx->mutex) {
        // gagal alokasi → bisa tambahkan error handler
    	printf("Gagal alokasi semaphore!\n");
    }
}

// --- Transmit ---
I2C_Status I2C_Transmit(I2C_Context *ctx, uint16_t address,
                        I2C_Mode mode, uint8_t *data, uint16_t size, uint32_t timeout) {
    I2C_Status status = I2C_OK;

    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        if (mode == I2C_MODE_IT) {
            if (HAL_I2C_Master_Transmit_IT(ctx->hi2c, address, data, size) != HAL_OK) {
                status = I2C_ERROR;
            } else if (xSemaphoreTake(ctx->tx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) {
                status = I2C_TIMEOUT;
                I2C_Recovery(ctx->hi2c);
            }
        } else if (mode == I2C_MODE_DMA) {
            if (HAL_I2C_Master_Transmit_DMA(ctx->hi2c, address, data, size) != HAL_OK) {
                status = I2C_ERROR;
            } else if (xSemaphoreTake(ctx->tx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) {
                status = I2C_TIMEOUT;
                I2C_Recovery(ctx->hi2c);
            }
        } else { // blocking
            if (HAL_I2C_Master_Transmit(ctx->hi2c, address, data, size, timeout) != HAL_OK) {
                status = I2C_ERROR;
            }
        }
        xSemaphoreGive(ctx->mutex); // release setelah selesai
    } else {
        status = I2C_ERROR; // gagal ambil mutex
    }

    return status;
}


// --- Receive ---
I2C_Status I2C_Receive(I2C_Context *ctx, uint16_t address,
                       I2C_Mode mode, uint8_t *data, uint16_t size, uint32_t timeout) {
    I2C_Status status = I2C_OK;

    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        if (mode == I2C_MODE_IT) {
            if (HAL_I2C_Master_Receive_IT(ctx->hi2c, address, data, size) != HAL_OK) {
                status = I2C_ERROR;
            } else if (xSemaphoreTake(ctx->rx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) {
                status = I2C_TIMEOUT;
                I2C_Recovery(ctx->hi2c);
            }
        } else if (mode == I2C_MODE_DMA) {
            if (HAL_I2C_Master_Receive_DMA(ctx->hi2c, address, data, size) != HAL_OK) {
                status = I2C_ERROR;
            } else if (xSemaphoreTake(ctx->rx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) {
                status = I2C_TIMEOUT;
                I2C_Recovery(ctx->hi2c);
            }
        } else { // blocking
            if (HAL_I2C_Master_Receive(ctx->hi2c, address, data, size, timeout) != HAL_OK) {
                status = I2C_ERROR;
            }
        }
        xSemaphoreGive(ctx->mutex);
    } else {
        status = I2C_ERROR;
    }

    return status;
}



// --- TransmitReceive ---
I2C_Status I2C_TransmitReceive(I2C_Context *ctx, uint16_t address, I2C_Mode mode, uint8_t *txData, uint16_t txSize, uint32_t timeouttx, uint8_t *rxData, uint16_t rxSize, uint32_t timeoutrx) {
    I2C_Status status;
    // Step 1: Transmit
    status = I2C_Transmit(ctx, address, mode, txData, txSize, timeouttx);
    if (status != I2C_OK) {
        return status; // error atau timeout saat transmit
    }

    // Step 2: Receive
    status = I2C_Receive(ctx, address, mode, rxData, rxSize, timeoutrx);
    if (status != I2C_OK) {
        return status; // error atau timeout saat receive
    }

    return I2C_OK;
}

I2C_Status I2C_MemRead(I2C_Context *ctx, uint16_t address, I2C_Mode mode,
                       uint8_t reg, uint32_t regsize, uint8_t *data, uint16_t size, uint32_t timeout) {
    I2C_Status status = I2C_OK;

    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        if (mode == I2C_MODE_IT) {
            if (HAL_I2C_Mem_Read_IT(ctx->hi2c, address, reg, regsize, data, size) != HAL_OK) {
                status = I2C_ERROR;
            } else if (xSemaphoreTake(ctx->rx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) {
                status = I2C_TIMEOUT;
                I2C_Recovery(ctx->hi2c);
            }
        } else if (mode == I2C_MODE_DMA) {
            if (HAL_I2C_Mem_Read_DMA(ctx->hi2c, address, reg, regsize, data, size) != HAL_OK) {
                status = I2C_ERROR;
            } else if (xSemaphoreTake(ctx->rx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) {
                status = I2C_TIMEOUT;
                I2C_Recovery(ctx->hi2c);
            }
        } else {
            if (HAL_I2C_Mem_Read(ctx->hi2c, address, reg, regsize, data, size, timeout) != HAL_OK) {
                status = I2C_ERROR;
            }
        }
        xSemaphoreGive(ctx->mutex);
    } else {
        status = I2C_ERROR;
    }

    return status;
}


I2C_Status I2C_MemWrite(I2C_Context *ctx,
                        uint16_t address,
                        I2C_Mode mode,
                        uint8_t reg,
                        uint32_t regsize,
                        uint8_t *data,
                        uint16_t size,
						uint32_t timeout) {
    I2C_Status status = I2C_OK;

    // Ambil mutex sebelum transaksi
    if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        if (mode == I2C_MODE_IT) {
            if (HAL_I2C_Mem_Write_IT(ctx->hi2c, address, reg, regsize, data, size) != HAL_OK) {
                status = I2C_ERROR;
            } else if (xSemaphoreTake(ctx->tx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) {
                status = I2C_TIMEOUT;
                I2C_Recovery(ctx->hi2c);
            }
        } else if (mode == I2C_MODE_DMA) {
            if (HAL_I2C_Mem_Write_DMA(ctx->hi2c, address, reg, regsize, data, size) != HAL_OK) {
                status = I2C_ERROR;
            } else if (xSemaphoreTake(ctx->tx_sem, pdMS_TO_TICKS(timeout)) != pdPASS) {
                status = I2C_TIMEOUT;
                I2C_Recovery(ctx->hi2c);
            }
        } else { // Blocking
            if (HAL_I2C_Mem_Write(ctx->hi2c, address, reg, regsize, data, size, timeout) != HAL_OK) {
                status = I2C_ERROR;
            }
        }

        // Release mutex setelah transaksi selesai (sukses/error/timeout)
        xSemaphoreGive(ctx->mutex);
    } else {
        status = I2C_ERROR; // gagal ambil mutex
    }

    return status;
}

void I2C_ScanBus(I2C_HandleTypeDef *hi2c) {
    HAL_StatusTypeDef res;
    I2C_Context *ctx = NULL;

    // cari context yang sesuai untuk ambil mutex
    if (hi2c == i2c1_ctx.hi2c) ctx = &i2c1_ctx;

    if (ctx && xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
        printf("Scanning I2C bus...\r\n");
        for (uint8_t addr = 1; addr < 127; addr++) {
            res = HAL_I2C_IsDeviceReady(hi2c, (addr << 1), 3, 100);
            if (res == HAL_OK) {
                printf("Device found at 0x%02X (7-bit)\r\n", addr);
            }
        }
        printf("Scan done.\r\n");

        xSemaphoreGive(ctx->mutex);
    } else {
        printf("I2C Scan gagal ambil mutex!\r\n");
    }
}

I2C_Status I2C_Recovery(I2C_HandleTypeDef *hi2c) {
    if (hi2c == NULL) return I2C_ERROR;
    printf("I2C Recovery triggered...\r\n");
    // 1. Reset RCC peripheral (paksa reset hardware)
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
#ifdef I2C3
    else if (hi2c->Instance == I2C3) {
        __HAL_RCC_I2C3_FORCE_RESET();
        __HAL_RCC_I2C3_RELEASE_RESET();
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

    // 6. Re-init peripheral
    if (HAL_I2C_Init(hi2c) != HAL_OK) {
        printf("I2C Recovery failed!\r\n");
        return I2C_ERROR;
    }

    printf("I2C Recovery success.\r\n");
    return I2C_OK;
}

