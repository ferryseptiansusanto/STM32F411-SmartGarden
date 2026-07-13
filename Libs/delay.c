#include "delay.h"

uint32_t GetTick(void) {
    return xTaskGetTickCount();   // tick dari FreeRTOS
}

void DelayMs(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms)); // delay multitasking friendly
}

void DelayUs(uint32_t us) {
    // busy loop untuk kebutuhan singkat
    volatile uint32_t count = (SystemCoreClock / 1000000) * us / 5;
    while(count--) {
        __NOP();
    }
}
