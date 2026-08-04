/*
 * uart_wrapper.c
 *
 * Deskripsi: Implementasi driver abstraksi UART dengan dukungan DMA,
 * perlindungan Mutex untuk Thread-Safety, sinkronisasi Semaphore,
 * dan penanganan IDLE Line Interrupt menggunakan FreeRTOS Message Buffer.
 *
 * Created on: 29 Jun 2026
 * Author: ferry
 */

#include "uart_wrapper.h"
#include <string.h>

#define UART_TIMEOUT_MS 1000

// Maksimal instance UART yang didukung sistem (HC-05, ESP32, GPS, dll)
#define MAX_UART_INSTANCES 3

// Registry dinamis untuk manajemen callback interupsi multi-instance
static UART_Context* uart_registry[MAX_UART_INSTANCES] = {NULL};
static uint8_t registered_uarts_count = 0;

// Inisialisasi struktur konteks default untuk USART2
UART_Context uart1_ctx = { 0 };

/**
 * @brief  Mendaftarkan instance UART ke dalam registry internal untuk keperluan ISR Callback.
 */
static void Register_UART_Instance(UART_Context *dev) {
    if (registered_uarts_count < MAX_UART_INSTANCES) {
        // Cek apakah sudah terdaftar
        for (uint8_t i = 0; i < registered_uarts_count; i++) {
            if (uart_registry[i] == dev) return;
        }
        uart_registry[registered_uarts_count++] = dev;
    }
}

/**
 * @brief  Menginisialisasi objek kontrol UART beserta sinkronisasi FreeRTOS-nya.
 * @param  dev          Pointer ke konteks UART_Context target.
 * @param  huart_handle Pointer ke handle HAL UART bawaan CubeMX (misal: &huart1).
 */
void UART_Init(UART_Context *dev, UART_HandleTypeDef *huart_handle) {
    if (dev == NULL || dev->huart == NULL) return;

    dev->huart = huart_handle;

    // Alokasikan objek sinkronisasi RTOS
    dev->tx_sem = xSemaphoreCreateBinary();
    dev->tx_mutex = xSemaphoreCreateRecursiveMutex(); // Mutex pengaman tabrakan antar-task
    dev->rx_msg_buf = xMessageBufferCreate(UART_MSG_BUFFER_SIZE);

    // Pastikan memori buffer dibersihkan saat startup awal
    memset(dev->dma_rx_buffer, 0, sizeof(dev->dma_rx_buffer));
    memset(dev->dma_tx_buffer, 0, sizeof(dev->dma_tx_buffer));

    // Daftarkan ke sistem untuk ISR
	Register_UART_Instance(dev);
}

/**
 * @brief  Mengirim data secara asinkron (Non-Blocking) menggunakan DMA dengan proteksi Mutex.
 * @param  dev   Pointer ke konteks UART_Context target.
 * @param  data  Pointer ke array data yang akan dikirim.
 * @param  len   Panjang data yang dikirim dalam hitungan Byte.
 * @retval HAL_StatusTypeDef Status eksekusi pengiriman (OK, ERROR, TIMEOUT).
 * @note   Menggunakan Shadow Buffer (dma_tx_buffer) agar data asli (yang mungkin ada di stack lokal Task)
 * aman dari kehancuran sebelum DMA selesai menyedotnya.
 */

HAL_StatusTypeDef UART_Send(UART_Context *dev, const uint8_t *data, uint16_t len) {
    if (dev == NULL || dev->huart == NULL || dev->tx_sem == NULL || dev->tx_mutex == NULL || data == NULL || len == 0) {
        return HAL_ERROR;
    }

    if (len > sizeof(dev->dma_tx_buffer)) return HAL_ERROR;

    // AMBIL KUNCI: Blokir Task lain yang ingin memakai UART ini
    if (xSemaphoreTakeRecursive(dev->tx_mutex, portMAX_DELAY) != pdPASS) {
        return HAL_TIMEOUT;
    }

    // Salin ke Shadow Buffer yang persisten
    memcpy(dev->dma_tx_buffer, data, len);

    // Tembakkan DMA
    if (HAL_UART_Transmit_DMA(dev->huart, dev->dma_tx_buffer, len) != HAL_OK) {
        // [PERBAIKAN]: Mutex yang diambil secara rekursif WAJIB dilepas secara rekursif!
        xSemaphoreGiveRecursive(dev->tx_mutex);
        return HAL_ERROR;
    }

    // Tunggu notifikasi dari ISR (HAL_UART_TxCpltCallback)
    HAL_StatusTypeDef status = HAL_OK;
    if (xSemaphoreTake(dev->tx_sem, pdMS_TO_TICKS(UART_TIMEOUT_MS)) != pdPASS) {
        HAL_UART_DMAStop(dev->huart); // Fail-Safe: Cegah hardware lockup
        status = HAL_TIMEOUT;
    }

    // LEPASKAN KUNCI: Izinkan Task lain menggunakan UART
    xSemaphoreGiveRecursive(dev->tx_mutex);
    return status;
}

/* -------------------------------------------------------------------------
 * FITUR UTAMA: ASINKRONISASI MESSAGE BUFFER + DMA RECEIVER (RX)
 * ------------------------------------------------------------------------- */

/**
 * @brief  Mengaktifkan deteksi interupsi IDLE Line UART dan mengarahkan DMA RX.
 * @note   Wajib dipanggil satu kali saat fase inisialisasi (STATE_INIT_HARDWARE).
 */
HAL_StatusTypeDef UART_Wrapper_Start_Receive_DMA(UART_Context *dev) {
    if (dev == NULL || dev->huart == NULL) return HAL_ERROR;

    // Aktifkan interupsi jalur kosong (IDLE) secara langsung di register peripheral
    __HAL_UART_ENABLE_IT(dev->huart, UART_IT_IDLE);

    // Mulai siklus penerimaan data DMA melingkar mengarah ke dma_rx_buffer
    return HAL_UART_Receive_DMA(dev->huart, dev->dma_rx_buffer, UART_DMA_RX_BUFFER_SIZE);
}

/**
 * @brief  Mengambil data ter-parsing yang tersimpan di RAM FreeRTOS Message Buffer.
 * @note   Membuat Task Bluetooth RX tertidur efisien (0% CPU) selama tidak ada data masuk dari ISR.
 */
uint16_t UART_Receive_Message(UART_Context *dev, uint8_t *out_buf, uint16_t max_len, uint32_t timeout) {
    if (dev == NULL || dev->rx_msg_buf == NULL || out_buf == NULL) return 0;
    return xMessageBufferReceive(dev->rx_msg_buf, out_buf, max_len, pdMS_TO_TICKS(timeout));
}

/**
 * @brief  Handler Kustom Interupsi IDLE Line Perangkat Keras.
 * @note   Harus ditempatkan di dalam file stm32f4xx_it.c tepat pada fungsi USART2_IRQHandler().
 */
void UART_Hardware_IRQHandler(UART_Context *dev) {
    if (dev == NULL || dev->huart == NULL) return;

    // Cek apakah interupsi dipicu oleh flag deteksi jalur transmisi mati/selesai (IDLE)
    if (__HAL_UART_GET_FLAG(dev->huart, UART_FLAG_IDLE) != RESET) {
        __HAL_UART_CLEAR_IDLEFLAG(dev->huart); // Reset flag agar interupsi tidak berulang tanpa akhir

        // Stop DMA sementara untuk menghitung delta counter pemrosesan data riil
        HAL_UART_DMAStop(dev->huart);

        // Kalkulasi jumlah byte aktual: Ukuran total buffer dikurangi sisa counter hardware registers
        uint16_t bytes_received = UART_DMA_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(dev->huart->hdmarx);

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        // Lempar potongan data mentah langsung ke lorong aman FreeRTOS Message Buffer
        if (bytes_received > 0 && dev->rx_msg_buf != NULL) {
            xMessageBufferSendFromISR(dev->rx_msg_buf, dev->dma_rx_buffer, bytes_received, &xHigherPriorityTaskWoken);
        }

        // Nyalakan ulang siklus lingkaran receiver DMA dari indeks ke-0 untuk transaksi berikutnya
        HAL_UART_Receive_DMA(dev->huart, dev->dma_rx_buffer, UART_DMA_RX_BUFFER_SIZE);

        // Paksa RTOS melakukan Context Switch instan jika Task RX memiliki prioritas lebih tinggi
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* --- Low-Level HAL Callbacks Linker --- */

/**
 * @brief  Callback bawaan HAL yang dieksekusi saat proses Hardware Transmit DMA Selesai.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (size_t i = 0; i < registered_uarts_count; i++) {
        if (huart == uart_registry[i]->huart && uart_registry[i]->tx_sem != NULL) {
            // Berikan Semaphore untuk membangunkan Task pengirim yang sedang tertidur menunggu hardware selesai
            xSemaphoreGiveFromISR(uart_registry[i]->tx_sem, &xHigherPriorityTaskWoken);
            break;
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief  Callback darurat jika buffer melingkar RX penuh 128 byte sebelum interupsi IDLE dipicu.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (size_t i = 0; i < registered_uarts_count; i++) {
        UART_Context *dev = uart_registry[i];
        if (huart == dev->huart && dev->rx_msg_buf != NULL) {
            // Amankan seluruh isi buffer penuh ke FreeRTOS RAM, lalu restart DMA kembali
            xMessageBufferSendFromISR(dev->rx_msg_buf, dev->dma_rx_buffer, UART_DMA_RX_BUFFER_SIZE, &xHigherPriorityTaskWoken);
            HAL_UART_Receive_DMA(dev->huart, dev->dma_rx_buffer, UART_DMA_RX_BUFFER_SIZE);
            break;
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
