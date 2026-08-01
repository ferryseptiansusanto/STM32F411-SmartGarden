/*
 * @file usart_datalink.c
 * @brief Lapisan Ekstraksi Data (OSI Layer 2)
 *
 *  Created on: 13 May 2026
 *      Author: ferry
 */
#include "usart_datalink.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>

static uint8_t calc_crc(USART_Frame *f) {
    uint8_t sum = f->header ^ f->cmd ^ f->len;
    for (int i = 0; i < f->len; i++) sum ^= f->payload[i];
    return sum;
}

int USART_Datalink_SendFrame(UART_Context *dev, USART_Frame *frame) {
    if (dev == NULL || dev->tx_mutex == NULL) return 0;

    // Mutex RTOS untuk perlindungan Data Tearing saat TX
    if (xSemaphoreTakeRecursive(dev->tx_mutex, pdMS_TO_TICKS(100)) != pdPASS) {
        return 0;
    }

    frame->crc = calc_crc(frame);
    dev->dma_tx_buffer[0] = frame->header;
    dev->dma_tx_buffer[1] = frame->cmd;
    dev->dma_tx_buffer[2] = frame->len;
    memcpy(&dev->dma_tx_buffer[3], frame->payload, frame->len);
    dev->dma_tx_buffer[3 + frame->len] = frame->crc;

    int result = (UART_Send(dev, dev->dma_tx_buffer, frame->len + 4) == HAL_OK);

    xSemaphoreGiveRecursive(dev->tx_mutex);
    return result;
}

int USART_DatalinkDMA_ParseBuffer(uint8_t *buf, uint16_t len, USART_Frame *frame) {
    if (len < 4) return 0;

    for (uint16_t i = 0; i <= len - 4; i++) {
        if (buf[i] == FRAME_HEADER) {
            uint8_t potential_len = buf[i + 2];

            if (potential_len <= FRAME_MAX_LEN && (i + 3 + potential_len) < len) {
                frame->header = buf[i];
                frame->cmd    = buf[i + 1];
                frame->len    = potential_len;
                memcpy(frame->payload, &buf[i + 3], frame->len);
                frame->crc = buf[i + 3 + frame->len];

                if (frame->crc == calc_crc(frame)) {
                    return (i + 4 + frame->len); // Paket Valid
                }
            }
        }
    }
    return 0;
}
