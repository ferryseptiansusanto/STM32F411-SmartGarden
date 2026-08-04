/*
 * @file    bluetooth_wrapper.c
 * @brief   Implementasi lapisan abstraksi aplikasi (Wrapper) Modem/Bluetooth
 * @note    Menggunakan standar Pointer Passing (pvPortMalloc) FreeRTOS untuk
 * efisiensi memori Queue tingkat tinggi.
 *
 * Created on: 8 May 2026
 * Author: ferry
 */

#include "bluetooth_wrapper.h"
#include "usart_protocol.h"
#include <string.h>

/**
 * @brief Inisialisasi modul Bluetooth
 */
void BLUETOOTH_Init(BL_Device *dev, UART_Context *uart_ctx) {
    if (dev != NULL && uart_ctx != NULL) {
        dev->ctx = uart_ctx;
    }
}

/**
 * @brief Mengirimkan pesan keluar ke perangkat eksternal via Bluetooth secara aman (Thread-Safe)
 */
bool BLUETOOTH_SendMessage(BL_Device *dev, CommandID_t cmd, const char *str) {
    if (dev == NULL || dev->ctx == NULL || str == NULL) return false;

    USART_Message msg;
    msg.cmd = (int)cmd;
    msg.len = strlen(str);

    if (msg.len > sizeof(msg.payload)) {
        msg.len = sizeof(msg.payload); // Batasi agar tidak buffer overflow
    }

    memcpy(msg.payload, str, msg.len);

    // Kirim menggunakan lapisan protokol yang sudah kita refactor
    return (UART_Protocol_Send(dev->ctx, &msg) == 1);
}
