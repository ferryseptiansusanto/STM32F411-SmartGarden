/*
 * bluetooth_wrapper.c
 *
 * Deskripsi: Implementasi lapisan abstraksi aplikasi (Wrapper) Bluetooth
 * untuk merakit pesan protokol komunikasi asinkron tanpa memblokir task FSM utama.
 *
 * Created on: 8 May 2026
 * Author: ferry
 */

#include "bluetooth_wrapper.h"
#include <string.h>

// Panggil akses Queue asinkron global milik bluetooth_task
extern QueueHandle_t btQueueTx;

/**
 * @brief  Menginisialisasi abstraksi driver fisik komunikasi Bluetooth.
 */
void BLUETOOTH_Init(UART_Context *dev, UART_HandleTypeDef *huart) {
    UART_Init(dev, huart);
}

/**
 * @brief  Mengemas pesan berbasis string dan mengirimkannya ke Queue TX Bluetooth secara asinkron.
 * @param  dev  Pointer ke konteks UART_Context fisik Bluetooth.
 * @param  cmd  Jenis perintah (Command Type) berdasarkan protokol usart_protocol.h.
 * @param  str  Pesan payload berbentuk string.
 * @note   Fungsi ini aman dipanggil langsung oleh FSM Utama / App Task karena bersifat non-blocking.
 */
void BLUETOOTH_SendMessage(UART_Context *dev, USART_Command cmd, const char *str) {
    // Validasi pengaman parameter masukan dan kesiapan antrean FreeRTOS
    if (dev == NULL || str == NULL || btQueueTx == NULL) {
        return;
    }

    USART_Message msg;
    memset(&msg, 0, sizeof(USART_Message));

    // Pasang metadata instruksi protokol aplikasi
    msg.cmd = cmd;
    msg.len = strlen(str);

    // Pengaman: Potong data string secara defensif jika ukurannya melebihi payload maksimum struktur paket
    if (msg.len > sizeof(msg.payload)) {
        msg.len = sizeof(msg.payload);
    }

    // Salin string string payload ke dalam container terstruktur paket
    memcpy(msg.payload, str, msg.len);

    /*
     * [PERBAIKAN KUNCI 3]: Alih-alih langsung memanggil fungsi blocking UART_Protocol_Send di sini,
     * pesan dikirim secara instan ke btQueueTx dengan timeout kecil (10ms).
     * Dengan begini, FSM Utama bebas dari interupsi blocking hardware UART, dan tugas eksekusi fisik
     * pengiriman diserahkan sepenuhnya kepada BLUETOOTH_TaskTx secara asinkron.
     */
    xQueueSend(btQueueTx, &msg, pdMS_TO_TICKS(10));
}
