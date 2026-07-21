/*
 * bluetooth_wrapper.h
 *
 * Created on: 8 May 2026
 * Author: ferry
 */

#ifndef BLUETOOTH_WRAPPER_H_
#define BLUETOOTH_WRAPPER_H_

#include "uart_wrapper.h"
#include "usart_protocol.h"

// Inisialisasi Bluetooth (hanya murni init physical driver)
void BLUETOOTH_Init(UART_Context *dev, UART_HandleTypeDef *huart);

// Kirim string (langsung dibungkus ke frame protokol dan dikirim)
void BLUETOOTH_SendMessage(UART_Context *dev, USART_Command cmd, const char *str);

#endif /* BLUETOOTH_WRAPPER_H_ */
