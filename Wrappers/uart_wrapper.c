/*
 * uart_wrapper.c
 *
 * Created on: 29 Jun 2026
 * Author: ferry
 */

#include "uart_wrapper.h"

#define UART_TIMEOUT_MS 1000

// Inisialisasi shell konteks global untuk UART2
UART_Context uart2_ctx = { &huart2, NULL, NULL };

// Registrasi Context UART. Jika menambah UART baru, masukkan ke dalam array ini.
static UART_Context* uart_registry[] = { &uart2_ctx };
#define UART_REGISTRY_COUNT (sizeof(uart_registry) / sizeof(uart_registry[0]))

// Menyesuaikan nama dengan yang dipanggil oleh bluetooth_wrapper.c
void UART_Init(UART_Context *dev, UART_HandleTypeDef *huart) {
    if (dev == NULL) return;

    dev->huart = huart;
    dev->tx_sem = xSemaphoreCreateBinary();
    dev->rx_sem = xSemaphoreCreateBinary();
}

HAL_StatusTypeDef UART_Send(UART_Context *dev, const uint8_t *data, uint16_t len) {
    if (dev == NULL || dev->huart == NULL || dev->tx_sem == NULL) return HAL_ERROR;

    // Memulai transfer DMA
    if (HAL_UART_Transmit_DMA(dev->huart, (uint8_t*)data, len) != HAL_OK) {
        return HAL_ERROR;
    }

    // Menunggu Semaphore hingga ISR selesai mengirim atau timeout
    if (xSemaphoreTake(dev->tx_sem, pdMS_TO_TICKS(UART_TIMEOUT_MS)) != pdPASS) {
        return HAL_TIMEOUT;
    }
    return HAL_OK;
}

HAL_StatusTypeDef UART_Receive(UART_Context *dev, uint8_t *data, uint16_t len) {
    if (dev == NULL || dev->huart == NULL || dev->rx_sem == NULL) return HAL_ERROR;

    // Memulai penerimaan DMA
    if (HAL_UART_Receive_DMA(dev->huart, data, len) != HAL_OK) {
        return HAL_ERROR;
    }

    // Menunggu Semaphore hingga ISR menerima data atau timeout
    if (xSemaphoreTake(dev->rx_sem, pdMS_TO_TICKS(UART_TIMEOUT_MS)) != pdPASS) {
        return HAL_TIMEOUT;
    }
    return HAL_OK;
}

// Memulai penerimaan DMA dalam mode background (Continuous/Circular)
HAL_StatusTypeDef UART_Wrapper_Start_Receive_DMA(UART_Context *dev, uint8_t *rx_buf, uint16_t max_len) {
    if (dev == NULL || dev->huart == NULL) return HAL_ERROR;

    // Aktifkan Interupsi IDLE Line (mendeteksi jika kabel serial sudah sepi/selesai transfer)
    __HAL_UART_ENABLE_IT(dev->huart, UART_IT_IDLE);

    // Mulai terima data secara asinkron
    if (HAL_UART_Receive_DMA(dev->huart, rx_buf, max_len) != HAL_OK) {
        return HAL_ERROR;
    }
    return HAL_OK;
}

// Handler Global yang harus kamu panggil di stm32f4xx_it.c (di dalam USART2_IRQHandler)
void UART_Hardware_IRQHandler(UART_Context *dev) {
    // Periksa apakah interupsi IDLE Line aktif
    if (__HAL_UART_GET_FLAG(dev->huart, UART_FLAG_IDLE) != RESET) {
        // Clear flag IDLE dengan membaca data register
        __HAL_UART_CLEAR_IDLEFLAG(dev->huart);

        // Hentikan DMA sementara untuk membaca jumlah data yang masuk
        HAL_UART_DMAStop(dev->huart);

        // Beritahu task yang menunggu bahwa data telah siap
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (dev->rx_sem != NULL) {
            xSemaphoreGiveFromISR(dev->rx_sem, &xHigherPriorityTaskWoken);
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}


// --- Callback DMA Dinamis ---

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Mencari konteks UART yang cocok secara dinamis dari registry
    for (size_t i = 0; i < UART_REGISTRY_COUNT; i++) {
        if (huart == uart_registry[i]->huart && uart_registry[i]->tx_sem != NULL) {
            xSemaphoreGiveFromISR(uart_registry[i]->tx_sem, &xHigherPriorityTaskWoken);
            break;
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Mencari konteks UART yang cocok secara dinamis dari registry
    for (size_t i = 0; i < UART_REGISTRY_COUNT; i++) {
        if (huart == uart_registry[i]->huart && uart_registry[i]->rx_sem != NULL) {
            xSemaphoreGiveFromISR(uart_registry[i]->rx_sem, &xHigherPriorityTaskWoken);
            break;
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
