/*
 * usart_protocol.h
 *
 *  Created on: 13 May 2026
 *      Author: ferry
 */

#ifndef USART_PROTOCOL_H_
#define USART_PROTOCOL_H_

#include "usart_datalink.h"

typedef enum {
    CMD_SET_PARAM = 0x01,
    CMD_GET_DATA  = 0x02,
    CMD_STREAM_ON = 0x03,
    CMD_STREAM_OFF= 0x04
} USART_Command;

typedef struct {
    USART_Command cmd;
    uint8_t payload[FRAME_MAX_LEN];
    uint8_t len;
} USART_Message;

int UART_Protocol_Send(UART_Context *dev, USART_Message *msg);
int UART_Protocol_Receive(UART_Context *dev, USART_Message *msg);
int UART_ProtocolDMA_Parse(USART_Frame *frame, USART_Message *msg);

#endif /* USART_PROTOCOL_H_ */
