/*
 * usart_protocol.c
 *
 *  Created on: 13 May 2026
 *      Author: ferry
 */


// usart_protocol.c
#include "usart_protocol.h"
#include <string.h>

int UART_Protocol_Send(UART_Context *dev, USART_Message *msg) {
    USART_Frame f;
    f.header = 0xAA;
    f.cmd = msg->cmd;
    f.len = msg->len;
    memcpy(f.payload, msg->payload, msg->len);
    return USART_Datalink_SendFrame(dev, &f);
}

int UART_Protocol_Receive(UART_Context *dev, USART_Message *msg) {
    USART_Frame f;
    if (!USART_Datalink_ReceiveFrame(dev, &f)) return 0;
    msg->cmd = f.cmd;
    msg->len = f.len;
    for (int i=0;i<f.len;i++) msg->payload[i] = f.payload[i];
    return 1;
}

int UART_ProtocolDMA_Parse(USART_Frame *frame, USART_Message *msg) {
    msg->cmd = (USART_Command)frame->cmd;
    msg->len = frame->len;
    memcpy(msg->payload, frame->payload, msg->len);
    return 1;
}

