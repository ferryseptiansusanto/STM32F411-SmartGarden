/*
 * bluetooth_task.c
 *
 * Created on: 2026
 * Author: ferry
 */

#include "bluetooth_task.h"
#include "bluetooth_wrapper.h"

// Alokasi memori Queue global
QueueHandle_t btQueueTx = NULL;
QueueHandle_t btQueueRx = NULL;

// Queue utama milik command_task (FSM Utama)
extern QueueHandle_t uartQueue;

void BLUETOOTH_AppTaskCreate(UART_Context *phy_device) {
    if (phy_device == NULL) return;

    // 1. Buat Queue di sini sebelum task berjalan
    btQueueTx = xQueueCreate(5, sizeof(char[64]));
    btQueueRx = xQueueCreate(5, sizeof(USART_Message));

    // 2. Nyalakan mesin DMA Receiver di latar belakang
    UART_Wrapper_Start_Receive_DMA(phy_device);

    // 3. Buat Task RTOS dan oper parameter object phy ke masing-masing task
    xTaskCreate(BLUETOOTH_TaskTx, "BT_TxTask", 256, (void*)phy_device, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(BLUETOOTH_TaskRx, "BT_RxTask", 256, (void*)phy_device, tskIDLE_PRIORITY + 2, NULL);
}

// -------------------------------------------------------------
// TASK TRANSMIT (TX) - Mengirim Data ke Smartphone
// -------------------------------------------------------------
void BLUETOOTH_TaskTx(void *pvParameters) {
    UART_Context *phy = (UART_Context*)pvParameters;

    // Gunakan struct pesan standar protokol, bukan raw string
    USART_Message tx_msg;

    for (;;) {
        // Task ini akan tidur (0% CPU) sampai ada pesan di Queue
        // Pastikan btQueueTx dibuat dengan item size = sizeof(USART_Message)
        if (xQueueReceive(btQueueTx, &tx_msg, portMAX_DELAY) == pdPASS) {

            if (phy != NULL) {
                /*
                 * Langsung berikan struct ini ke layer protokol.
                 * Layer protokol akan merakitnya menjadi frame (Header + Cmd + Payload + CRC)
                 * lalu memberikannya ke layer datalink yang sudah kita lindungi Mutex & DMA.
                 */
                UART_Protocol_Send(phy, &tx_msg);
            }
        }
    }
}

// -------------------------------------------------------------
// TASK RECEIVE (RX) - Menerima & Menerjemahkan Perintah
// -------------------------------------------------------------
void BLUETOOTH_TaskRx(void *pvParameters) {
    UART_Context *phy = (UART_Context*)pvParameters;

    // Gunakan buffer akumulasi persisten (menyimpan sisa data antar-siklus)
    static uint8_t rx_accumulator[256];
    static uint16_t accum_index = 0;

    uint8_t temp_buf[UART_DMA_RX_BUFFER_SIZE];
    USART_Frame rx_frame;
    USART_Message rx_msg;

    for (;;) {
        // Bangun jika ada potongan data baru dari DMA
        uint16_t length = UART_Receive_Message(phy, temp_buf, sizeof(temp_buf), portMAX_DELAY);

        if (length > 0) {
            // Cegah overflow pada akumulator
            if (accum_index + length > sizeof(rx_accumulator)) {
                accum_index = 0; // Reset jika buffer kepenuhan (pengaman)
            }

            // Tambahkan data baru ke ujung akumulator
            memcpy(&rx_accumulator[accum_index], temp_buf, length);
            accum_index += length;

            // Coba parsing buffer akumulasi
            if (USART_DatalinkDMA_ParseBuffer(rx_accumulator, accum_index, &rx_frame)) {

                // Jika sukses mendapat 1 frame utuh, parse ke format Message
                UART_ProtocolDMA_Parse(&rx_frame, &rx_msg);

                // Lempar ke Queue Utama (FSM)
                if (uartQueue != NULL) {
                    xQueueSend(uartQueue, &rx_msg, portMAX_DELAY);
                }

                // Hapus buffer akumulasi karena paket sudah berhasil diproses
                accum_index = 0;
            }
        }
    }
}
