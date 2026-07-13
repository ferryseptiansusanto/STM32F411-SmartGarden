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

typedef struct {
    UART_HandleTypeDef *huart;
    SemaphoreHandle_t tx_sem;
    SemaphoreHandle_t rx_sem;
} UART_Context;

extern UART_HandleTypeDef huart2;
extern UART_Context uart2_ctx;

void UART_Init(UART_Context *dev, UART_HandleTypeDef *huart);
HAL_StatusTypeDef UART_Send(UART_Context *dev, const uint8_t *data, uint16_t len);
HAL_StatusTypeDef UART_Receive(UART_Context *dev, uint8_t *data, uint16_t len);
HAL_StatusTypeDef UART_Wrapper_Start_Receive_DMA(UART_Context *dev, uint8_t *rx_buf, uint16_t max_len);
void UART_Hardware_IRQHandler(UART_Context *dev);

#endif /* UART_WRAPPER_H_ */
