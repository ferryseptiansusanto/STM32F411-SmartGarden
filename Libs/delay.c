#include "delay.h"

// Fungsi inisialisasi DWT (Panggil di awal program, misalnya di dalam main() sebelum RTOS start)
void DWT_Delay_Init(void) {
    // Aktifkan TRCENA (Trace Enable) di register CoreDebug
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    // Reset nilai counter
    DWT->CYCCNT = 0;

    // Aktifkan Cycle Counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t GetTick(void) {
    // Perhatian: Jika GetTick dipanggil dari dalam ISR,
    // pastikan menggunakan xTaskGetTickCountFromISR()
    if (xPortIsInsideInterrupt()) {
        return xTaskGetTickCountFromISR();
    }
    return xTaskGetTickCount();
}

void DelayMs(uint32_t ms) {
    // Non-blocking RTOS delay
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void DelayUs(uint32_t us) {
    // Kalkulasi target tick berdasarkan System Clock aktual
    // (Misal: 100MHz clock = 100 tick per mikrodetik)
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    uint32_t start_tick = DWT->CYCCNT;

    // Blocking wait yang 100% akurat dan independen dari compiler
    while ((DWT->CYCCNT - start_tick) < ticks) {
        // Kosong, biarkan hardware bekerja
    }
}
