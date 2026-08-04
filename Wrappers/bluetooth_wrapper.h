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
#include "command_event.h"
#include <stdbool.h>

typedef struct {
    UART_Context *ctx;
} BL_Device;

extern BL_Device Bluetooth_Ctx;

/**
 * @brief Inisialisasi perangkat Bluetooth
 * @param dev Pointer ke struct BL_Device
 * @param uart_ctx Pointer ke konteks hardware UART
 */
void BLUETOOTH_Init(BL_Device *dev, UART_Context *uart_ctx);

/**
 * @brief Mengirim pesan terstruktur keluar melalui Bluetooth
 * @param dev Pointer ke perangkat Bluetooth
 * @param cmd ID Perintah (CommandID_t)
 * @param str Pointer ke string payload
 * @return true jika sukses, false jika gagal
 */
bool BLUETOOTH_SendMessage(BL_Device *dev, CommandID_t cmd, const char *str);
#endif /* BLUETOOTH_WRAPPER_H_ */
