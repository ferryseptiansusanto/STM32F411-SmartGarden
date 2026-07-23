/*
 * uart_wrapper.h
 *
 *  Created on: 29 Jun 2026
 *      Author: ferry
 */


#ifndef UART_WRAPPER_H_
#define UART_WRAPPER_H_

#include "main.h"
#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "message_buffer.h"

// Ukuran buffer DMA (1 kali transaksi maksimum)
#define UART_DMA_RX_BUFFER_SIZE 128
// Ukuran antrean Message Buffer di dalam RAM FreeRTOS
#define UART_MSG_BUFFER_SIZE 256

typedef struct {
    UART_HandleTypeDef *huart;
    SemaphoreHandle_t tx_sem;       // Untuk sinkronisasi DMA Selesai
    SemaphoreHandle_t tx_mutex;     // Untuk mencegah tabrakan data antar Task
    MessageBufferHandle_t rx_msg_buf;
    uint8_t dma_rx_buffer[UART_DMA_RX_BUFFER_SIZE];
    uint8_t dma_tx_buffer[128];     // Buffer persisten untuk pengiriman DMA
} UART_Context;

extern UART_HandleTypeDef huart1;
extern UART_Context uart1_ctx;

void UART_Init(UART_Context *dev);
HAL_StatusTypeDef UART_Send(UART_Context *dev, const uint8_t *data, uint16_t len);
HAL_StatusTypeDef UART_Receive(UART_Context *dev, uint8_t *data, uint16_t len);

// Fungsi khusus DMA + Message Buffer
HAL_StatusTypeDef UART_Wrapper_Start_Receive_DMA(UART_Context *dev);
uint16_t UART_Receive_Message(UART_Context *dev, uint8_t *out_buf, uint16_t max_len, uint32_t timeout);
void UART_Hardware_IRQHandler(UART_Context *dev);

#endif /* UART_WRAPPER_H_ */
