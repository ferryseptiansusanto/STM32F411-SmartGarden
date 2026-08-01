/*
 * @file    bluetooth_wrapper.h
 * @brief   Lapisan Abstraksi (Wrapper) untuk Protokol Modem / Bluetooth.
 *
 * Created on: 8 May 2026
 * Author: ferry
 */
#ifndef BLUETOOTH_WRAPPER_H_
#define BLUETOOTH_WRAPPER_H_

#include "uart_wrapper.h"
#include "usart_protocol.h"

typedef struct {
    UART_Context *uart_ctx;
} BL_Device;

extern BL_Device Bluetooth_Ctx;

void BLUETOOTH_Init(BL_Device *dev, UART_Context *ctx);

// Fungsi ini mengembalikan true jika pesan berhasil masuk antrean
bool BLUETOOTH_SendMessage(BL_Device *dev, USART_Command cmd, const char *str);

#endif /* BLUETOOTH_WRAPPER_H_ */
