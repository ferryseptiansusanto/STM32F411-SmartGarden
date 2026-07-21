/*
 * uart_wrapper.c
 *
 * Created on: 29 Jun 2026
 * Author: ferry
 */

#include "uart_wrapper.h"
#include <string.h>

#define UART_TIMEOUT_MS 1000

// Inisialisasi awal. Array dma_rx_buffer akan diisi 0.
UART_Context uart2_ctx = { &huart2, NULL, NULL, {0} };

static UART_Context* uart_registry[] = { &uart2_ctx };
#define UART_REGISTRY_COUNT (sizeof(uart_registry) / sizeof(uart_registry[0]))

void UART_Init(UART_Context *dev, UART_HandleTypeDef *huart) {
	if (dev == NULL) return;
	    dev->huart = huart;
	    dev->tx_sem = xSemaphoreCreateBinary();
	    dev->tx_mutex = xSemaphoreCreateMutex(); // Buat Mutex
	    dev->rx_msg_buf = xMessageBufferCreate(UART_MSG_BUFFER_SIZE);
}

HAL_StatusTypeDef UART_Send(UART_Context *dev, const uint8_t *data, uint16_t len) {
    if (dev == NULL || dev->huart == NULL || dev->tx_sem == NULL) return HAL_ERROR;

    if (HAL_UART_Transmit_DMA(dev->huart, (uint8_t*)data, len) != HAL_OK) return HAL_ERROR;

    if (xSemaphoreTake(dev->tx_sem, pdMS_TO_TICKS(UART_TIMEOUT_MS)) != pdPASS) return HAL_TIMEOUT;
    return HAL_OK;
}

// Hanya dipakai jika butuh blocking murni (tanpa Message Buffer)
HAL_StatusTypeDef UART_Receive(UART_Context *dev, uint8_t *data, uint16_t len) {
    if (dev == NULL || dev->huart == NULL) return HAL_ERROR;
    return HAL_UART_Receive(dev->huart, data, len, UART_TIMEOUT_MS);
}

// -------------------------------------------------------------
// FITUR MESSAGE BUFFER + DMA
// -------------------------------------------------------------

HAL_StatusTypeDef UART_Wrapper_Start_Receive_DMA(UART_Context *dev) {
    if (dev == NULL || dev->huart == NULL) return HAL_ERROR;

    // Aktifkan Interupsi IDLE Line
    __HAL_UART_ENABLE_IT(dev->huart, UART_IT_IDLE);

    // Mulai arahkan DMA untuk menaruh data ke dalam dma_rx_buffer
    return HAL_UART_Receive_DMA(dev->huart, dev->dma_rx_buffer, UART_DMA_RX_BUFFER_SIZE);
}

// Fungsi ini yang diambil/dipanggil oleh Task_Bluetooth untuk mengambil antrean pesan
uint16_t UART_Receive_Message(UART_Context *dev, uint8_t *out_buf, uint16_t max_len, uint32_t timeout) {
    if (dev == NULL || dev->rx_msg_buf == NULL) return 0;

    // Task akan "Tertidur" di sini sampai ISR melempar pesan
    return xMessageBufferReceive(dev->rx_msg_buf, out_buf, max_len, pdMS_TO_TICKS(timeout));
}

// Panggil ini di stm32f4xx_it.c dalam USART2_IRQHandler
void UART_Hardware_IRQHandler(UART_Context *dev) {
    if (__HAL_UART_GET_FLAG(dev->huart, UART_FLAG_IDLE) != RESET) {
        __HAL_UART_CLEAR_IDLEFLAG(dev->huart); // Bersihkan flag

        // Hentikan DMA sebentar untuk mengambil data
        HAL_UART_DMAStop(dev->huart);

        // Hitung berapa byte riil yang masuk (Ukuran total dikurangi sisa counter DMA)
        uint16_t bytes_received = UART_DMA_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(dev->huart->hdmarx);

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        // Lempar data aktual ke lorong Message Buffer FreeRTOS
        if (bytes_received > 0 && dev->rx_msg_buf != NULL) {
            xMessageBufferSendFromISR(dev->rx_msg_buf, dev->dma_rx_buffer, bytes_received, &xHigherPriorityTaskWoken);
        }

        // Nyalakan ulang DMA dari indeks ke-0 agar siap menerima pesan berikutnya
        HAL_UART_Receive_DMA(dev->huart, dev->dma_rx_buffer, UART_DMA_RX_BUFFER_SIZE);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// --- Callback Dinamis ---
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (size_t i = 0; i < UART_REGISTRY_COUNT; i++) {
        if (huart == uart_registry[i]->huart && uart_registry[i]->tx_sem != NULL) {
            xSemaphoreGiveFromISR(uart_registry[i]->tx_sem, &xHigherPriorityTaskWoken);
            break;
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Callback ini terpanggil JIKA dma_rx_buffer penuh (128 byte) sebelum IDLE terdeteksi
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (size_t i = 0; i < UART_REGISTRY_COUNT; i++) {
        UART_Context *dev = uart_registry[i];
        if (huart == dev->huart && dev->rx_msg_buf != NULL) {
            xMessageBufferSendFromISR(dev->rx_msg_buf, dev->dma_rx_buffer, UART_DMA_RX_BUFFER_SIZE, &xHigherPriorityTaskWoken);
            HAL_UART_Receive_DMA(dev->huart, dev->dma_rx_buffer, UART_DMA_RX_BUFFER_SIZE); // Restart DMA
            break;
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
