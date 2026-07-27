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

// Konteks dan Handle Queue Internal Modul Bluetooth
static BL_Device bl_device;
static QueueHandle_t btQueueTx = NULL;
static QueueHandle_t btQueueRx = NULL;

// Handle Queue tujuan (FSM Utama / App Task) yang di-passing saat inisialisasi
static QueueHandle_t systemAppQueue = NULL;

/**
 * @brief  Membuat dan menginisialisasi Task RTOS untuk modul Bluetooth.
 * @param  priority Prioritas task yang akan di-set.
 * @param  phy_device Pointer ke konteks UART perangkat fisik Bluetooth.
 * @param  app_queue Handle Queue milik FSM Utama (app_task) untuk meneruskan perintah.
 * @note   Fungsi ini harus dipanggil sebelum osKernelStart().
 */
void BLUETOOTH_AppTaskCreate(UBaseType_t priority, UART_Context *phy_device, QueueHandle_t app_queue) {
    if (phy_device == NULL || app_queue == NULL) return;

    // Simpan referensi queue tujuan secara aman
    systemAppQueue = app_queue;

    // Inisialisasi Device Context Bluetooth
    BLUETOOTH_Init(&bl_device, phy_device);

    // Alokasi Queue TX dan RX Internal Bluetooth
    btQueueTx = xQueueCreate(10, sizeof(USART_Message));
    btQueueRx = xQueueCreate(10, sizeof(USART_Message));

    // Nyalakan mesin DMA Receiver di latar belakang (menunggu Interupsi IDLE)
    UART_Wrapper_Start_Receive_DMA(bl_device.ctx);

    // Buat Task RTOS untuk TX dan RX dengan prioritas yang disesuaikan
    xTaskCreate(BLUETOOTH_TaskTx, "BT_TxTask", 256, NULL, priority, NULL);
    xTaskCreate(BLUETOOTH_TaskRx, "BT_RxTask", 512, NULL, priority + 1, NULL);
}

/**
 * @brief  Task khusus untuk memproses pengiriman data (TX) ke Smartphone.
 * @param  pvParameters Parameter task (tidak digunakan).
 * @note   Task ini akan memblokir (Tidur/0% CPU) hingga ada pesan masuk ke btQueueTx.
 */
void BLUETOOTH_TaskTx(void *pvParameters) {
    (void)pvParameters;
    USART_Message tx_msg;

    for (;;) {
        // Task tidur menunggu data. Begitu FSM Utama / Sensor mengirim data
        // melalui btQueueTx, Task ini akan langsung terbangun.
        if (xQueueReceive(btQueueTx, &tx_msg, portMAX_DELAY) == pdPASS) {
            if (bl_device.ctx != NULL) {
                /*
                 * Merakit Frame dan mengirimkannya lewat DMA.
                 */
                UART_Protocol_Send(bl_device.ctx, &tx_msg);
            }
        }
    }
}

/**
 * @brief  Task khusus untuk memantau data yang diterima (RX) dari Smartphone.
 * @param  pvParameters Parameter task (tidak digunakan).
 * @note   Task ini akan terbangun dari ISR ketika DMA menerima paket data.
 */
void BLUETOOTH_TaskRx(void *pvParameters) {
    (void)pvParameters;

    // Buffer akumulasi dinaikkan sedikit agar aman jika frame bertumpuk
    static uint8_t rx_accumulator[512];
    static uint16_t accum_index = 0;

    uint8_t temp_buf[UART_DMA_RX_BUFFER_SIZE];
    USART_Frame rx_frame;
    USART_Message rx_msg;

    for (;;) {
        // Tidur menunggu ISR melemparkan potongan data (Chunks) via MessageBuffer/Wrapper
        uint16_t length = UART_Receive_Message(bl_device.ctx, temp_buf, sizeof(temp_buf), portMAX_DELAY);

        if (length > 0) {
            // Pengaman: Kosongkan akumulator jika ukuran data melebihi sisa buffer (Mencegah buffer overflow)
            if (accum_index + length > sizeof(rx_accumulator)) {
                accum_index = 0;
            }

            // Gabungkan data baru ke ujung akumulator
            memcpy(&rx_accumulator[accum_index], temp_buf, length);
            accum_index += length;

            // Gunakan WHILE untuk memproses frame yang bertumpuk secara berurutan
            while (accum_index > 0) {

                // Coba pecah byte mentah menjadi Frame utuh
            	int consumed_bytes = USART_DatalinkDMA_ParseBuffer(rx_accumulator, accum_index, &rx_frame);
                if (consumed_bytes > 0) {

                    // Terjemahkan Frame Datalink ke Pesan Protokol Aplikasi
                    UART_ProtocolDMA_Parse(&rx_frame, &rx_msg);

                    // PERBAIKAN: Menggunakan systemAppQueue yang di-passing saat Init,
                    // menghilangkan ketergantungan pada extern variable yang rawan linker error.
                    if (systemAppQueue != NULL) {
                        xQueueSend(systemAppQueue, &rx_msg, pdMS_TO_TICKS(50));
                    }

                    // PERBAIKAN: Geser buffer menggunakan jumlah bytes yang akurat (TIDAK ADA LAGI CONFLICT)
					if (accum_index >= consumed_bytes) {
						uint16_t remaining_bytes = accum_index - consumed_bytes;
						if (remaining_bytes > 0) {
							memmove(&rx_accumulator[0], &rx_accumulator[consumed_bytes], remaining_bytes);
						}
						accum_index = remaining_bytes;
					} else {
						accum_index = 0;
					}
                } else {
                    // Frame belum utuh, keluar dari while dan tunggu byte berikutnya
                    break;
                }
            } // End of While
        }
    }
}

/**
 * @brief  Fungsi publik bagi modul lain untuk mengirim pesan ke antrean Bluetooth TX.
 * @param  msg Pointer ke struktur USART_Message yang akan dikirim.
 * @return pdTRUE jika berhasil masuk antrean, pdFALSE jika gagal/penuh.
 */
BaseType_t BLUETOOTH_Task_SendMessage(const USART_Message *msg) {
    // Gunakan btQueueTx yang sudah static di file ini
    if (btQueueTx == NULL || msg == NULL) return pdFALSE;
    return xQueueSend(btQueueTx, msg, pdMS_TO_TICKS(100));
}
