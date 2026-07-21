/*
 * bluetooth_wrapper.c
 *
 * Created on: 8 May 2026
 * Author: ferry
 */
#include "bluetooth_wrapper.h"
#include <string.h>

void BLUETOOTH_Init(UART_Context *dev, UART_HandleTypeDef *huart) {
    UART_Init(dev, huart);
}

// Ubah parameternya untuk menerima tipe perintah
void BLUETOOTH_SendMessage(UART_Context *dev, USART_Command cmd, const char *str) {
    if (dev != NULL && str != NULL) {
        USART_Message msg;
        memset(&msg, 0, sizeof(USART_Message));

        msg.cmd = cmd; // Dinamis berdasarkan input parameter
        msg.len = strlen(str);

        if (msg.len > sizeof(msg.payload)) {
            msg.len = sizeof(msg.payload);
        }

        memcpy(msg.payload, str, msg.len);
        UART_Protocol_Send(dev, &msg);
    }
}
