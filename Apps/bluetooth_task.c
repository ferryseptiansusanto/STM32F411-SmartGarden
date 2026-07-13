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
#define RX_BUFFER_SIZE 64
uint8_t bluetooth_rx_raw[RX_BUFFER_SIZE];
extern QueueHandle_t uartQueue; // Queue utama milik command_task

void BLUETOOTH_AppTaskCreate(UART_Context *phy_device) {
    // Buat Queue di sini sebelum task berjalan
    btQueueTx = xQueueCreate(5, sizeof(char[64]));
    btQueueRx = xQueueCreate(5, sizeof(USART_Message));

    // Buat Task RTOS dan oper parameter object phy ke masing-masing task
    xTaskCreate(BLUETOOTH_TaskTx, "BT_TxTask", 256, (void*)phy_device, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(BLUETOOTH_TaskRx, "BT_RxTask", 256, (void*)phy_device, tskIDLE_PRIORITY + 2, NULL);
}

void BLUETOOTH_TaskTx(void *pvParameters) {
	UART_Context *phy = (UART_Context*)pvParameters;
    char data[64];

    for (;;) {
        if (xQueueReceive(btQueueTx, &data, portMAX_DELAY) == pdPASS) {
            if (phy != NULL) {
                BLUETOOTH_SendString(phy, data);
            }
        }
    }
}

void BLUETOOTH_TaskRx(void *pvParameters) {
	UART_Context *phy = (UART_Context*)pvParameters;
    USART_Message msg;

    for (;;) {
        if (phy != NULL && UART_Protocol_Receive(phy, &msg) == 1) {
            // Kirim ke queue utama supaya command_task bisa eksekusi langsung
            if (uartQueue != NULL) {
                xQueueSend(uartQueue, &msg, portMAX_DELAY);
            }
        }
    }
}

void Bluetooth_Receiver_Task(void *argument) {
    USART_Frame rx_frame;
    USART_Message rx_msg;

    // 1. Hidupkan receiver DMA di awal sekali
    UART_Wrapper_Start_Receive_DMA(&uart2_ctx, bluetooth_rx_raw, RX_BUFFER_SIZE);

    while(1) {
        // 2. Task akan tidur di sini sampai di-wake up oleh IDLE Line ISR
        if (xSemaphoreTake(uart2_ctx.rx_sem, portMAX_DELAY) == pdPASS) {

            // Hitung berapa panjang data yang benar-benar masuk
            uint16_t received_bytes = RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(uart2_ctx.huart->hdmarx);

            // 3. Masuk ke Data Link Layer: Validasi struktur & CRC
            if (USART_DatalinkDMA_ParseBuffer(bluetooth_rx_raw, received_bytes, &rx_frame)) {

                // 4. Masuk ke Protocol Layer: Konversi ke Message format
                UART_ProtocolDMA_Parse(&rx_frame, &rx_msg);

                // 5. Eksekusi perintah sesuai aplikasi kamu
                if (rx_msg.cmd == CMD_SET_PARAM) {
                    // Lakukan sesuatu dengan rx_msg.payload (ex: Ganti nama perangkat, dll)
                }
            }

            // 6. Reset kembali DMA untuk menerima paket berikutnya
            UART_Start_Receive_DMA(&uart2_ctx, bluetooth_rx_raw, RX_BUFFER_SIZE);
        }
    }
}
