/*
 * usart_datalink.h
 *
 *  Created on: 13 May 2026
 *      Author: ferry
 */

#ifndef USART_DATALINK_H_
#define USART_DATALINK_H_

#include <uart_wrapper.h>

#define FRAME_MAX_LEN 64

typedef struct {
    uint8_t header;
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[FRAME_MAX_LEN];
    uint8_t crc;
} USART_Frame;

int USART_Datalink_SendFrame(UART_Context *dev, USART_Frame *frame);
int USART_Datalink_ReceiveFrame(UART_Context *dev, USART_Frame *frame);
int USART_DatalinkDMA_ParseBuffer(uint8_t *buf, uint16_t len, USART_Frame *frame);

#endif /* USART_DATALINK_H_ */
