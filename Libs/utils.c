/*
 * utils.c
 *
 *  Created on: 29 Apr 2026
 *      Author: ferry
 */
#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "utils.h"
#include <math.h>
extern UART_HandleTypeDef huart1;

uint8_t bcd2dec(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

uint8_t dec2bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}


// Konversi float ke string dengan jumlah digit desimal dinamis (0 hingga 4 digit)
// buffer minimal 16 byte
void float_to_string_dynamic(float value, char *buffer, int decimal) {
    // 1. Ambil bagian bilangan bulat
    int int_part = (int)value;

    // 2. Ambil nilai mutlak/absolut dari pecahan agar selalu positif
    float fraction = fabsf(value - (float)int_part);

    // 3. Tentukan nilai pengali (multiplier) berdasarkan jumlah decimal
    float multiplier = 1.0f;
    for (int i = 0; i < decimal; i++) {
        multiplier *= 10.0f;
    }

    // 4. Hitung bagian pecahan sesuai presisi desimal dengan pembulatan +0.5
    int frac_part = (int)(fraction * multiplier + 0.5f);

    // 5. Antisipasi jika pembulatan memicu overflow ke atas (misal 0.99 -> 100)
    if (frac_part >= (int)multiplier) {
        frac_part = 0;
        if (value >= 0) int_part++;
        else int_part--;
    }

    // 6. Cetak ke buffer secara dinamis menggunakan pemformatan lebar argumen (*s)
    // Jika decimal = 0, hanya mencetak bilangan bulatnya saja
    if (decimal <= 0) {
        sprintf(buffer, "%d", int_part);
    } else {
        // Format "%.*d" akan menggunakan nilai parameter 'decimal' untuk mengatur jumlah angka nol di depan
        sprintf(buffer, "%d.%.*d", int_part, decimal, frac_part);
    }
}

void vAssertCalled(const char *file, int line) {
    printf("ASSERT FAILED: %s:%d\n", file, line);
//    taskDISABLE_INTERRUPTS();
//    for(;;); // berhenti di sini
}

int _write(int file, char *ptr, int len)
{
	// Kirim via Console
    for (int i = 0; i < len; i++)
    {
        ITM_SendChar(ptr[i]);
    }

    //Kirim via UART
    HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, 500);
    return len;
}

void DebugTask(void *arg) {
    printf("PendSV priority = %lu\n", NVIC_GetPriority(PendSV_IRQn));
    printf("SVC priority    = %lu\n", NVIC_GetPriority(SVCall_IRQn));
    printf("SysTick priority= %lu\n", NVIC_GetPriority(SysTick_IRQn));
    vTaskDelete(NULL);
}


void DebugPrintISRPriority(void) {
    // Versi library (0–15)
    printf("SysTick priority (lib) = %lu\n", NVIC_GetPriority(SysTick_IRQn));
    printf("PendSV priority (lib)  = %lu\n", NVIC_GetPriority(PendSV_IRQn));
    printf("SVC priority (lib)     = %lu\n", NVIC_GetPriority(SVCall_IRQn));

    printf("DMA1_Channel2 priority = %lu\n", NVIC_GetPriority(DMA1_Stream0_IRQn));
    printf("DMA1_Channel3 priority = %lu\n", NVIC_GetPriority(DMA1_Stream1_IRQn));
    printf("SPI1 priority          = %lu\n", NVIC_GetPriority(SPI1_IRQn));

    // Nilai register mentah (0x00–0xF0)
    printf("SysTick priority (reg) = 0x%02X\n", SCB->SHP[11 - 4]); // SysTick
    printf("PendSV priority (reg)  = 0x%02X\n", SCB->SHP[10 - 4]); // PendSV
    printf("SVC priority (reg)     = 0x%02X\n", SCB->SHP[7 - 4]);  // SVC
}

