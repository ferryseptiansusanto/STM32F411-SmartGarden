/**
 * @file    spi_wrapper.c
 * @brief   Implementasi driver abstraksi SPI.
 */

#include "spi_wrapper.h"
#include "delay.h"
#include <stdio.h>

// Inisialisasi awal. Pastikan cs_port dan cs_pin diisi di main.c nanti sesuai perangkatnya.
SPI_Context spi1_ctx = { &hspi1, NULL, NULL, NULL, NULL };

extern SPI_HandleTypeDef hspi1;

// Registri dinamis untuk Callback ISR
static SPI_Context* spi_registry[] = { &spi1_ctx };
#define SPI_REGISTRY_COUNT (sizeof(spi_registry) / sizeof(spi_registry[0]))

/**
 * @brief Inisialisasi OS Primitives untuk SPI.
 */
void SPI_Init(SPI_Context *ctx) {
    if (ctx == NULL) return;

    ctx->tx_sem   = xSemaphoreCreateBinary();
    ctx->rx_sem   = xSemaphoreCreateBinary();
    ctx->txrx_sem = xSemaphoreCreateBinary();
    ctx->mutex    = xSemaphoreCreateMutex(); // Buat Mutex

    if (!ctx->tx_sem || !ctx->rx_sem || !ctx->txrx_sem || !ctx->mutex) {
		printf("SPI Gagal alokasi OS primitive!\n"); // Fail-Safe monitoring
    }
}

// --- Callback DMA ---
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (size_t i = 0; i < SPI_REGISTRY_COUNT; i++) {
        if (hspi == spi_registry[i]->hspi && spi_registry[i]->tx_sem != NULL) {
            xSemaphoreGiveFromISR(spi_registry[i]->tx_sem, &xHigherPriorityTaskWoken);
            break;
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (size_t i = 0; i < SPI_REGISTRY_COUNT; i++) {
        if (hspi == spi_registry[i]->hspi && spi_registry[i]->rx_sem != NULL) {
            xSemaphoreGiveFromISR(spi_registry[i]->rx_sem, &xHigherPriorityTaskWoken);
            break;
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (size_t i = 0; i < SPI_REGISTRY_COUNT; i++) {
        if (hspi == spi_registry[i]->hspi && spi_registry[i]->txrx_sem != NULL) {
            xSemaphoreGiveFromISR(spi_registry[i]->txrx_sem, &xHigherPriorityTaskWoken);
            break;
        }
    }
    YIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Handler Error Wajib agar Task tidak hang
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (size_t i = 0; i < SPI_REGISTRY_COUNT; i++) {
        if (hspi == spi_registry[i]->hspi) {
        	// Mencegah Task terblokir selamanya jika hardware SPI error di tengah jalan
            if (spi_registry[i]->tx_sem) xSemaphoreGiveFromISR(spi_registry[i]->tx_sem, &xHigherPriorityTaskWoken);
            if (spi_registry[i]->rx_sem) xSemaphoreGiveFromISR(spi_registry[i]->rx_sem, &xHigherPriorityTaskWoken);
            if (spi_registry[i]->txrx_sem) xSemaphoreGiveFromISR(spi_registry[i]->txrx_sem, &xHigherPriorityTaskWoken);
            break;
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief  Mengirim data via SPI.
 * @note   Pastikan SPI_Select_CS sudah dipanggil (dan memegang Mutex) sebelum ini.
 */
// --- Transmit ---
SPI_Status SPI_Transmit(SPI_Context *ctx, SPI_Mode mode, const uint8_t *data, uint16_t size) {
    if (mode == SPI_MODE_DMA) {
        if (HAL_SPI_Transmit_DMA(ctx->hspi, (uint8_t *)data, size) != HAL_OK) return SPI_ERROR;
        if (xSemaphoreTake(ctx->tx_sem, pdMS_TO_TICKS(SPI_TIMEOUT_MS)) != pdPASS) return SPI_TIMEOUT;
    } else {
        if (HAL_SPI_Transmit(ctx->hspi, (uint8_t *)data, size, SPI_TIMEOUT_MS) != HAL_OK) return SPI_ERROR;
    }
    return SPI_OK;
}

// --- Receive ---
SPI_Status SPI_Receive(SPI_Context *ctx, SPI_Mode mode, uint8_t *data, uint16_t size) {
    if (mode == SPI_MODE_DMA) {
        if (HAL_SPI_Receive_DMA(ctx->hspi, data, size) != HAL_OK) return SPI_ERROR;
        if (xSemaphoreTake(ctx->rx_sem, pdMS_TO_TICKS(SPI_TIMEOUT_MS)) != pdPASS) return SPI_TIMEOUT;
    } else {
        if (HAL_SPI_Receive(ctx->hspi, data, size, SPI_TIMEOUT_MS) != HAL_OK) return SPI_ERROR;
    }
    return SPI_OK;
}

// --- TransmitReceive ---
SPI_Status SPI_TransmitReceive(SPI_Context *ctx, SPI_Mode mode, const uint8_t *txBuf, uint8_t *rxBuf, uint16_t size) {
    if (mode == SPI_MODE_DMA) {
        if (HAL_SPI_TransmitReceive_DMA(ctx->hspi, (uint8_t *)txBuf, rxBuf, size) != HAL_OK) return SPI_ERROR;
        if (xSemaphoreTake(ctx->txrx_sem, pdMS_TO_TICKS(SPI_TIMEOUT_MS)) != pdPASS) return SPI_TIMEOUT;
    } else {
        if (HAL_SPI_TransmitReceive(ctx->hspi, (uint8_t *)txBuf, rxBuf, size, SPI_TIMEOUT_MS) != HAL_OK) return SPI_ERROR;
    }
    return SPI_OK;
}

/**
 * @brief  Memilih (Chip Select) perangkat SPI dan MENGUNCI BUS.
 * @note   MENGAPA KITA MENGUNCI MUTEX DI SINI? Karena transaksi SPI (misal: baca sektor SD Card)
 * terdiri dari beberapa kali panggilan SPI_Transmit dan SPI_Receive. Mutex harus ditahan
 * sejak CS LOW hingga CS HIGH agar Task lain tidak menyela di tengah transaksi.
 */
void SPI_Select_CS(SPI_Context *ctx, GPIO_TypeDef *port, uint16_t pin) {
    if (ctx == NULL || ctx->mutex == NULL) return;

    // Tunggu maksimal 2000ms. Mencegah Deadlock selamanya jika Mutex nyangkut (Fail-Safe)
    if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        if (port != NULL) {
            HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
        }
    }
}

/**
 * @brief  Melepas (Chip Select) perangkat SPI dan MEMBUKA KUNCI BUS.
 */
void SPI_Unselect_CS(SPI_Context *ctx, GPIO_TypeDef *port, uint16_t pin) {
	if (ctx == NULL || ctx->mutex == NULL) return;

	if (port != NULL) {
		HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET); // Deassert CS High
	}

	// SD Card butuh 8 clock pulse (1 byte) setelah CS High untuk melepaskan MISO bus
	uint8_t dummy = 0xFF;
	HAL_SPI_Transmit(ctx->hspi, &dummy, 1, 100);

	// Lepas Mutex
	xSemaphoreGive(ctx->mutex);
}

/**
 * @brief  Mengirim dummy clocks (biasanya untuk inisialisasi awal SD Card).
 */
void SPI_SendDummyClocks(SPI_Context *ctx, GPIO_TypeDef *port, uint16_t pin, uint16_t count){
    if (ctx == NULL || ctx->mutex == NULL) return;

    if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        if (port != NULL) {
            HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
        }
        uint8_t dummy = 0xFF;
        for (uint16_t i = 0; i < count; i++) {
            HAL_SPI_Transmit(ctx->hspi, &dummy, 1, 100);
        }
        xSemaphoreGive(ctx->mutex);
    }
}

// --- Speed Control ---
void SPI_SetSpeed(SPI_Context *ctx, uint32_t prescaler) {
    ctx->hspi->Init.BaudRatePrescaler = prescaler;
    HAL_SPI_Init(ctx->hspi);
    DelayMs(10); // Dipercepat, 100ms terlalu lama untuk sekadar ubah clock
}
