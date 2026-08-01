/**
 * @file command_task.c
 * @brief Implementasi Kurir Universal (Zero-Copy Architecture & Memory Pooling)
 * * Menerapkan pola Pinjam-Pakai memori untuk mencegah kebocoran RAM
 * serta menjaga efisiensi antrean Event FSM Utama.
 *
 * Created on: 2026
 * Author: ferry
 */

#include "command_task.h"
#include "bluetooth_wrapper.h"
#include "usart_protocol.h"
#include "usart_datalink.h"
#include <string.h>

static BL_Device bl_device;
static QueueHandle_t systemAppQueue = NULL;

/**
 * @brief Membuat dan mendaftarkan Task Kurir Komunikasi ke FreeRTOS Scheduler.
 */
void CMD_AppTaskCreate(UBaseType_t priority, UART_Context *phy_device, QueueHandle_t app_queue) {
    if (phy_device == NULL || app_queue == NULL) return;

    systemAppQueue = app_queue;

    /* Inisialisasi wrapper perangkat keras komunikasi (misal: Bluetooth HC-05) */
    BLUETOOTH_Init(&bl_device, phy_device);

    /* Memulai penerimaan data mentah berbasis Circular DMA + IDLE Interrupt (Zero-Blocking) */
    UART_Wrapper_Start_Receive_DMA(bl_device.ctx);

    /* Membuat Task RTOS untuk menangani penerimaan pesan */
    xTaskCreate(
        CMD_TaskRx,           /* Fungsi Task */
        "CommRxTask",         /* Nama Task untuk Debugging */
        512,                  /* Ukuran Stack (dalam Words) */
        NULL,                 /* Parameter Task */
        priority,             /* Prioritas Task */
        NULL                  /* Task Handle */
    );
}

/**
 * @brief Task utama penerima data berarsitektur Zero-Copy.
 * * Task tertidur murni 0% CPU sampai Interrupt DMA/IDLE membangunkannya
 * melalui fungsi ulTaskNotifyTake().
 */
void CMD_TaskRx(void *pvParameters) {
    (void)pvParameters;

    static uint8_t rx_accumulator[256];
    static uint16_t accum_index = 0;

    uint8_t temp_buf[64];
    USART_Frame rx_frame;

    for (;;) {
        /* Menunggu sinyal/notifikasi asinkron dari ISR tanpa melakukan Poling (0% CPU load) */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* Mengambil data dari buffer DMA perangkat keras */
        uint16_t length = UART_Fetch_DMA_Buffer(bl_device.ctx, temp_buf, sizeof(temp_buf));

        if (length > 0) {
            /* Mencegah limpasan buffer akumulator */
            if (accum_index + length > sizeof(rx_accumulator)) {
                accum_index = 0;
            }

            memcpy(&rx_accumulator[accum_index], temp_buf, length);
            accum_index += length;

            /* Memproses byte yang terkumpul di dalam accumulator */
            while (accum_index > 0) {
                int consumed_bytes = USART_DatalinkDMA_ParseBuffer(rx_accumulator, accum_index, &rx_frame);

                if (consumed_bytes > 0) {
                    /* ===================================================================
                     * MENERAPKAN POLA PINJAM-PAKAI (ZERO-COPY QUEUE)
                     * Alokasi heap dilakukan agar ukuran struct di Antrean FSM tetap kecil (O(1)).
                     * =================================================================== */
                    CommandEvent_t* new_event = (CommandEvent_t*) pvPortMalloc(sizeof(CommandEvent_t));

                    if (new_event != NULL) {
                        if (UART_Protocol_ParseFrame(&rx_frame, new_event)) {

                            /* Mengirim HANYA ALAMAT MEMORI (Pointer) ke Antrean FSM Utama */
                            if (xQueueSend(systemAppQueue, &new_event, pdMS_TO_TICKS(10)) != pdPASS) {
                                /* Antrean penuh: Gagal kirim, bebaskan payload string jika ada untuk cegah Leak! */
                                if (new_event->cmd_id == CMD_BLUETOOTH_CSV && new_event->payload.csv_data.str_ptr != NULL) {
                                    vPortFree(new_event->payload.csv_data.str_ptr);
                                }
                                /* Bebaskan kembali alokasi struct utama */
                                vPortFree(new_event);
                            }
                        } else {
                            /* Jika proses protokol gagal menerjemahkan frame */
                            vPortFree(new_event);
                        }
                    }

                    /* Menggeser sisa data di dalam buffer akumulator */
                    if (accum_index >= consumed_bytes) {
                        uint16_t remaining = accum_index - consumed_bytes;
                        if (remaining > 0) {
                            memmove(&rx_accumulator[0], &rx_accumulator[consumed_bytes], remaining);
                        }
                        accum_index = remaining;
                    } else {
                        accum_index = 0;
                    }
                } else {
                    /* Belum ada frame utuh yang terbentuk, keluar dari loop sambil menunggu data berikutnya */
                    break;
                }
            }
        }
    }
}
