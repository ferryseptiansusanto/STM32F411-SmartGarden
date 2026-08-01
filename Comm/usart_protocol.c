/*
 * @file usart_protocol.c
 * @brief Implementasi Penterjemah Protokol Komunikasi
 *
 *  Created on: 13 May 2026
 *      Author: ferry
 */

#include "usart_protocol.h"
#include "FreeRTOS.h"
#include <string.h>

/**
 * @brief Mengemas pesan aplikasi menjadi bingkai datalink lalu dikirim.
 */
int UART_Protocol_Send(UART_Context *dev, USART_Message *msg) {
    USART_Frame f;
    f.header = 0xAA;
    f.cmd = (uint8_t)msg->cmd;
    f.len = msg->len;

    // MENGAPA MENGGUNAKAN MEMCPY? Menyalin data payload secara aman ke buffer frame fisik.
    memcpy(f.payload, msg->payload, msg->len);
    return USART_Datalink_SendFrame(dev, &f);
}

/**
 * @brief Menerjemahkan bingkai datalink menjadi CommandEvent_t dengan Pola Pinjam-Pakai.
 * * Jika perintah membawa string atau data panjang (seperti file CSV/Jadwal),
 * memori heap dialokasikan via pvPortMalloc(). Pihak penerima (FSM) wajib membebaskannya.
 */
bool UART_Protocol_ParseFrame(USART_Frame *frame, CommandEvent_t *out_event) {
    if (frame == NULL || out_event == NULL) return false;

    out_event->cmd_id = (CommandID_t)frame->cmd;

    // Menangani alokasi dinamis khusus untuk payload string/panjang (Zero-Copy Architecture)
    if (out_event->cmd_id == CMD_BLUETOOTH_CSV) {
        out_event->payload.csv_data.str_ptr = (char*) pvPortMalloc(frame->len + 1);

        if (out_event->payload.csv_data.str_ptr == NULL) {
            return false; // Gagal alokasi memori, cegah HardFault
        }

        memcpy(out_event->payload.csv_data.str_ptr, frame->payload, frame->len);
        out_event->payload.csv_data.str_ptr[frame->len] = '\0'; // Tambahkan Null-terminator
        out_event->payload.csv_data.len = frame->len;
    }
    else {
        // Untuk perintah sederhana (tanpa alokasi heap tambahan)
        if (frame->len >= 2) {
            out_event->payload.pump.actuator_id = frame->payload[0];
            out_event->payload.pump.state = (bool)frame->payload[1];
        } else {
            out_event->payload.pump.actuator_id = 0;
            out_event->payload.pump.state = false;
        }
    }

    return true;
}
