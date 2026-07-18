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
    char data[64];

    for (;;) {
        // Task ini akan tidur sampai ada bagian sistem lain (misal FSM)
        // yang mengirim string ke btQueueTx.
        if (xQueueReceive(btQueueTx, &data, portMAX_DELAY) == pdPASS) {
            if (phy != NULL) {
                // Bungkus ke frame protokol dan kirim
                BLUETOOTH_SendString(phy, data);
            }
        }
    }
}

// -------------------------------------------------------------
// TASK RECEIVE (RX) - Menerima & Menerjemahkan Perintah
// -------------------------------------------------------------
void BLUETOOTH_TaskRx(void *pvParameters) {
    UART_Context *phy = (UART_Context*)pvParameters;
    uint8_t buffer_tampung[128]; // Buffer lokal task untuk menerima dari MessageBuffer OS
    USART_Frame rx_frame;
    USART_Message rx_msg;

    for (;;) {
        // 1. Task tertidur di sini, bangun HANYA jika ada paket data lengkap dari DMA
        uint16_t length = UART_Receive_Message(phy, buffer_tampung, sizeof(buffer_tampung), portMAX_DELAY);

        if (length > 0) {

            // 2. Data Link Layer: Cari Header, ambil Payload, dan validasi kemurnian data (CRC)
            if (USART_DatalinkDMA_ParseBuffer(buffer_tampung, length, &rx_frame)) {

                // 3. Protocol Layer: Parsing dari bentuk Frame mentah ke Message terstruktur
                UART_ProtocolDMA_Parse(&rx_frame, &rx_msg);

                // 4. Integrasi ke Sistem Utama: Lempar pesan yang sudah valid ke Queue FSM
                if (uartQueue != NULL) {
                    xQueueSend(uartQueue, &rx_msg, portMAX_DELAY);
                } else if (btQueueRx != NULL) {
                    // Fallback jika uartQueue belum tersedia
                    xQueueSend(btQueueRx, &rx_msg, portMAX_DELAY);
                }
            }
        }
    }
}
