/*
 * usart_datalink.c
 *
 *  Created on: 13 May 2026
 *      Author: ferry
 */


// usart_datalink.c
#include "usart_datalink.h"
#include "uart_wrapper.h"
#include <string.h>

static uint8_t calc_crc(USART_Frame *f) {
    uint8_t sum = f->header ^ f->cmd ^ f->len;
    for (int i = 0; i < f->len; i++) sum ^= f->payload[i];
    return sum;
}

int USART_Datalink_SendFrame(UART_Context *dev, USART_Frame *frame) {
    frame->crc = calc_crc(frame);
    uint8_t buf[FRAME_MAX_LEN+4];
    buf[0] = frame->header;
    buf[1] = frame->cmd;
    buf[2] = frame->len;
    for (int i=0;i<frame->len;i++) buf[3+i] = frame->payload[i];
    buf[3+frame->len] = frame->crc;
    return (UART_Send(dev, buf, frame->len+4) == HAL_OK);
}

int USART_Datalink_ReceiveFrame(UART_Context *dev, USART_Frame *frame) {
    // sederhana: blocking receive
    UART_Receive(dev, &frame->header, 1);
    UART_Receive(dev, &frame->cmd, 1);
    UART_Receive(dev, &frame->len, 1);
    UART_Receive(dev, frame->payload, frame->len);
    UART_Receive(dev, &frame->crc, 1);
    return (frame->crc == calc_crc(frame));
}

int USART_DatalinkDMA_ParseBuffer(uint8_t *buf, uint16_t len, USART_Frame *frame) {
    if (len < 4) return 0; // minimal header+cmd+len+crc

    frame->header = buf[0];
    frame->cmd    = buf[1];
    frame->len    = buf[2];

    if (frame->len > FRAME_MAX_LEN) return 0;

    memcpy(frame->payload, &buf[3], frame->len);
    frame->crc = buf[3 + frame->len];

    // validasi CRC
    uint8_t calc = frame->header ^ frame->cmd ^ frame->len;
    for (int i = 0; i < frame->len; i++) calc ^= frame->payload[i];

    return (frame->crc == calc);
}

