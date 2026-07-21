/*
 * bluetooth_task.c
 *
 * Deskripsi: Task FreeRTOS untuk menangani transmisi dan resepsi data
 * modul Bluetooth secara asinkron (Non-Blocking) menggunakan
 * mekanisme DMA dan MessageBuffer.
 * Created on: 2026
 * Author: ferry
 */

#include "bluetooth_task.h"
#include "bluetooth_wrapper.h"
#include <string.h> // Untuk memcpy dan memmove

// Alokasi memori Queue global
QueueHandle_t btQueueTx = NULL;
QueueHandle_t btQueueRx = NULL;

// Queue utama milik command_task (FSM Utama)
extern QueueHandle_t uartQueue;

/**
 * @brief  Membuat dan menginisialisasi Task RTOS untuk modul Bluetooth.
 * @param  phy_device Pointer ke konteks UART perangkat fisik Bluetooth.
 * @note   Fungsi ini harus dipanggil sebelum osKernelStart().
 */
void BLUETOOTH_AppTaskCreate(UART_Context *phy_device) {
    if (phy_device == NULL) return;

    // [PERBAIKAN 1]: Menggunakan sizeof(USART_Message) untuk alokasi
    // item queue agar ukurannya pas dengan struct protokol, mencegah HardFault.
    btQueueTx = xQueueCreate(10, sizeof(USART_Message));
    btQueueRx = xQueueCreate(10, sizeof(USART_Message));

    // Nyalakan mesin DMA Receiver di latar belakang (menunggu Interupsi IDLE)
    UART_Wrapper_Start_Receive_DMA(phy_device);

    // Buat Task RTOS untuk TX dan RX
    xTaskCreate(BLUETOOTH_TaskTx, "BT_TxTask", 256, (void*)phy_device, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(BLUETOOTH_TaskRx, "BT_RxTask", 256, (void*)phy_device, tskIDLE_PRIORITY + 2, NULL);
}

/**
 * @brief  Task khusus untuk memproses pengiriman data (TX) ke Smartphone.
 * @param  pvParameters Pointer menuju UART_Context yang dikirim saat xTaskCreate.
 * @note   Task ini akan memblokir (Tidur/0% CPU) hingga ada pesan masuk ke btQueueTx.
 */
void BLUETOOTH_TaskTx(void *pvParameters) {
    UART_Context *phy = (UART_Context*)pvParameters;
    USART_Message tx_msg;

    for (;;) {
        // Task tidur menunggu data. Begitu FSM Utama / Sensor mengirim data
        // melalui BLUETOOTH_SendMessage, Task ini akan langsung terbangun.
        if (xQueueReceive(btQueueTx, &tx_msg, portMAX_DELAY) == pdPASS) {
            if (phy != NULL) {
                /*
                 * Merakit Frame dan mengirimkannya lewat DMA.
                 * Pastikan UART_Protocol_Send di file usart_protocol.c sudah
                 * diperbaiki agar menggunakan Mutex & DMA persisten buffer.
                 */
                UART_Protocol_Send(phy, &tx_msg);
            }
        }
    }
}

/**
 * @brief  Task khusus untuk memantau data yang diterima (RX) dari Smartphone.
 * @param  pvParameters Pointer menuju UART_Context yang dikirim saat xTaskCreate.
 * @note   Task ini akan terbangun dari ISR (MessageBuffer) ketika DMA menerima paket.
 */
void BLUETOOTH_TaskRx(void *pvParameters) {
    UART_Context *phy = (UART_Context*)pvParameters;

    // Buffer akumulasi dinaikkan sedikit agar aman jika frame bertumpuk
    static uint8_t rx_accumulator[512];
    static uint16_t accum_index = 0;

    uint8_t temp_buf[UART_DMA_RX_BUFFER_SIZE];
    USART_Frame rx_frame;
    USART_Message rx_msg;

    for (;;) {
        // Tidur menunggu ISR melemparkan potongan data (Chunks) via MessageBuffer
        uint16_t length = UART_Receive_Message(phy, temp_buf, sizeof(temp_buf), portMAX_DELAY);

        if (length > 0) {
            // Pengaman: Kosongkan akumulator jika ukuran data melebihi sisa buffer (Mencegah buffer overflow)
            if (accum_index + length > sizeof(rx_accumulator)) {
                accum_index = 0;
            }

            // Gabungkan data baru ke ujung akumulator
            memcpy(&rx_accumulator[accum_index], temp_buf, length);
            accum_index += length;

            // [PERBAIKAN 2]: Gunakan WHILE untuk memproses frame yang bertumpuk.
            // Bisa saja Android mengirim 2 perintah sekaligus dalam hitungan mikrodetik.
            while (accum_index > 0) {

                // Coba pecah byte mentah menjadi Frame utuh
                if (USART_DatalinkDMA_ParseBuffer(rx_accumulator, accum_index, &rx_frame)) {

                    // Terjemahkan Frame Datalink ke Pesan Protokol Aplikasi
                    UART_ProtocolDMA_Parse(&rx_frame, &rx_msg);

                    // Lempar hasil terjemahan ke Queue Utama agar FSM mengambil keputusan
                    if (uartQueue != NULL) {
                        // Gunakan portMAX_DELAY agar terjamin masuk atau timeout kecil (misal 10ms)
                        // jika Anda tidak ingin Task RX terblokir lama.
                        xQueueSend(uartQueue, &rx_msg, pdMS_TO_TICKS(50));
                    }

                    // [PERBAIKAN 3]: Menggeser sisa buffer (Memmove) bukan Hard Reset!
                    // Hitung total byte frame yang selesai diproses (Header+Cmd+Len+Payload+CRC)
                    // Asumsi struktur Datalink: Header(1) + Cmd(1) + Len(1) + Payload(Len) + CRC(1) = Len + 4
                    // *Note: Silakan sesuaikan angka '4' di bawah ini dengan jumlah byte overhead protokol Anda.
                    uint16_t processed_bytes = rx_frame.len + 4;

                    if (accum_index >= processed_bytes) {
                        uint16_t remaining_bytes = accum_index - processed_bytes;
                        if (remaining_bytes > 0) {
                            // Geser byte sisa yang belum ter-parse ke indeks 0
                            memmove(&rx_accumulator[0], &rx_accumulator[processed_bytes], remaining_bytes);
                        }
                        accum_index = remaining_bytes; // Update sisa indeks
                    } else {
                        // Reset penuh (pengaman darurat, normalnya logik ini tidak tereksekusi)
                        accum_index = 0;
                    }
                } else {
                    // Frame belum utuh (mungkin tertahan di pengiriman fisik Bluetooth).
                    // Keluar dari While, biarkan sistem tidur dan menunggu byte selanjutnya datang.
                    break;
                }
            } // End of While
        }
    }
}
