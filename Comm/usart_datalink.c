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

#define FRAME_HEADER 0xAA

static uint8_t calc_crc(USART_Frame *f) {
    uint8_t sum = f->header ^ f->cmd ^ f->len;
    for (int i = 0; i < f->len; i++) sum ^= f->payload[i];
    return sum;
}

int USART_Datalink_SendFrame(UART_Context *dev, USART_Frame *frame) {
    frame->crc = calc_crc(frame);
    uint8_t buf[FRAME_MAX_LEN + 4];
    buf[0] = frame->header;
    buf[1] = frame->cmd;
    buf[2] = frame->len;
    for (int i = 0; i < frame->len; i++) buf[3 + i] = frame->payload[i];
    buf[3 + frame->len] = frame->crc;

    return (UART_Send(dev, buf, frame->len + 4) == HAL_OK);
}

// ---------------------------------------------------------
// SOLUSI POINT 1: Blocking Receive yang Lebih Tangguh
// ---------------------------------------------------------
int USART_Datalink_ReceiveFrame(UART_Context *dev, USART_Frame *frame) {
    uint8_t header_byte = 0;

    // 1. Cari Header (0xAA) terlebih dahulu
    // Menggunakan sedikit looping untuk membuang "sampah" data jika ada
    int retry = 5;
    while (retry--) {
        if (UART_Receive(dev, &header_byte, 1) != HAL_OK) return 0; // Gagal / Timeout
        if (header_byte == FRAME_HEADER) break;
    }

    if (header_byte != FRAME_HEADER) return 0; // Header tidak ditemukan
    frame->header = header_byte;

    // 2. Baca Command (CMD) dan Length (LEN) sekaligus (2 byte)
    uint8_t cmd_len[2];
    if (UART_Receive(dev, cmd_len, 2) != HAL_OK) return 0;

    frame->cmd = cmd_len[0];
    frame->len = cmd_len[1];

    // 3. Proteksi Buffer Overflow (sangat penting!)
    if (frame->len > FRAME_MAX_LEN) return 0;

    // 4. Baca Payload dan CRC secara bersamaan jika ada payload
    // Panjang total = panjang payload + 1 byte CRC
    uint8_t payload_crc[FRAME_MAX_LEN + 1];
    if (UART_Receive(dev, payload_crc, frame->len + 1) != HAL_OK) return 0;

    // Salin data ke struct
    memcpy(frame->payload, payload_crc, frame->len);
    frame->crc = payload_crc[frame->len];

    // 5. Validasi kemurnian data (CRC)
    return (frame->crc == calc_crc(frame));
}

// ---------------------------------------------------------
// SOLUSI POINT 2: Parsing DMA yang Dinamis
// ---------------------------------------------------------
int USART_DatalinkDMA_ParseBuffer(uint8_t *buf, uint16_t len, USART_Frame *frame) {
    // Minimal panjang frame adalah 4 byte (Header + CMD + LEN + CRC)
    if (len < 4) return 0;

    // Cari lokasi byte Header (0xAA) bergeser ke kanan
    // Batas loop: len - 3 karena sisa buffer minimal harus ada 3 byte lagi (CMD, LEN, CRC)
    for (uint16_t i = 0; i <= len - 4; i++) {
        if (buf[i] == FRAME_HEADER) {

            uint8_t potential_len = buf[i + 2];

            // Validasi apakah ukuran sisa buffer CUKUP untuk menampung Payload + CRC
            // (i + 3 byte tetap + potential_len) tidak boleh melebihi panjang total (len)
            if (potential_len <= FRAME_MAX_LEN && (i + 3 + potential_len) < len) {

                frame->header = buf[i];
                frame->cmd    = buf[i + 1];
                frame->len    = potential_len;

                // Ambil Payload
                memcpy(frame->payload, &buf[i + 3], frame->len);
                // Ambil CRC
                frame->crc = buf[i + 3 + frame->len];

                // Verifikasi CRC
                if (frame->crc == calc_crc(frame)) {
                    return 1; // Paket data valid ditemukan!
                }

                // Jika CRC salah, sistem abaikan dan lanjut loop mencari 0xAA berikutnya
                // karena bisa jadi 0xAA pertama tadi adalah data acak/sampah.
            }
        }
    }

    return 0; // Tidak ditemukan frame data yang valid di dalam keseluruhan buffer
}

