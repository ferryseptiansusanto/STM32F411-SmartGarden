/*
 * usart_protocol.h
 *
 *  Created on: 13 May 2026
 *      Author: ferry
 */
/**
 * @file usart_protocol.h
 * @brief Lapisan Protokol Penerjemah Pesan (OSI Layer 6)
 * * Mengubah struktur bingkai mentah (USART_Frame) menjadi
 * Kontrak Event FSM (CommandEvent_t) dengan pendekatan Zero-Copy.
 */

#ifndef USART_PROTOCOL_H_
#define USART_PROTOCOL_H_

#include "usart_datalink.h"
#include "command_event.h"

/**
 * @brief Mengirim pesan terstruktur keluar melalui protokol USART.
 * @param dev Pointer ke konteks hardware UART
 * @param msg Pointer ke struktur pesan yang akan dikirim
 * @return 1 jika sukses, 0 jika gagal
 */
int UART_Protocol_Send(UART_Context *dev, USART_Message *msg);

/**
 * @brief Menerjemahkan frame datalink menjadi CommandEvent menggunakan alokasi heap (pvPortMalloc).
 * @param frame Pointer ke bingkai datalink mentah yang valid
 * @param out_event Pointer ke struct CommandEvent_t tujuan
 * @return true jika parsing sukses, false jika gagal atau memori habis
 */
bool UART_Protocol_ParseFrame(USART_Frame *frame, CommandEvent_t *out_event);

#endif /* USART_PROTOCOL_H_ */
