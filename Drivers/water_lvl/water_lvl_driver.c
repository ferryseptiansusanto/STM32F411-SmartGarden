/**
 * @file    water_lvl_driver.c
 * @brief   Implementasi driver sensor batas air dengan ISR Debouncing.
 * @author  Ferry
 * @date    14 Jul 2026
 */

#include "water_lvl_driver.h"

// MENGAPA pdMS_TO_TICKS? Memastikan 200ms selalu akurat terlepas dari
// berapapun nilai configTICK_RATE_HZ pada FreeRTOS.
#define DEBOUNCE_TIME_TICKS pdMS_TO_TICKS(200)

/**
 * @brief Struktur pemetaan hardware untuk pelampung batas air.
 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint32_t last_irq_tick; ///< Merekam Waktu (Tick) terakhir interupsi valid
    bool current_state;     ///< Status memori stabil (terproteksi debouncing)
} WtrLvlMap;

static WtrLvlMap wtrLvlMap[LVL_TANK_COUNT] = {
    [LVL_TANK_FULL]  = {LVL_TANK_FULL_GPIO_Port, LVL_TANK_FULL_Pin, 0, false},
    [LVL_TANK_EMPTY] = {LVL_TANK_EMPTY_GPIO_Port, LVL_TANK_EMPTY_Pin, 0, false},
};

QueueHandle_t wtrLvlQueue = NULL;

void WtrLvl_Init(void) {
    // MENGAPA UKURAN 5? Cukup untuk menampung riwayat event jika Task FSM
    // sedang sibuk. Jika terjadi "Splashing" ekstrem yang melebihi 5 event,
    // event tambahan akan dibuang (Drop). Ini adalah rate-limiter alami yang aman.
    wtrLvlQueue = xQueueCreate(5, sizeof(WtrLvl_Event_t));

    // MENGAPA CEK AWAL? Memastikan status FSM sinkron dengan realita
    // fisik air sesaat setelah MCU reboot (Fail-Safe Booting).
    if (wtrLvlMap[LVL_TANK_FULL].port != NULL) {
        wtrLvlMap[LVL_TANK_FULL].current_state =
            (HAL_GPIO_ReadPin(wtrLvlMap[LVL_TANK_FULL].port, wtrLvlMap[LVL_TANK_FULL].pin) == GPIO_PIN_RESET);
    }

    if (wtrLvlMap[LVL_TANK_EMPTY].port != NULL) {
        wtrLvlMap[LVL_TANK_EMPTY].current_state =
            (HAL_GPIO_ReadPin(wtrLvlMap[LVL_TANK_EMPTY].port, wtrLvlMap[LVL_TANK_EMPTY].pin) == GPIO_PIN_RESET);
    }
}

bool WtrLvl_Read(WtrLvl_Types type) {
    if (type >= LVL_TANK_COUNT) return false;
    return wtrLvlMap[type].current_state;
}

bool isWtrLvl_Full(void) {
    return WtrLvl_Read(LVL_TANK_FULL);
}

bool isWtrLvl_Empty(void) {
    return WtrLvl_Read(LVL_TANK_EMPTY);
}

/* -------------------------------------------------------------------------
 * INTERRUPT SERVICE ROUTINE (ISR)
 * ------------------------------------------------------------------------- */
void WtrLvl_EXTI_Callback(uint16_t GPIO_Pin) {
    uint32_t current_tick = xTaskGetTickCountFromISR();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    for (int i = 0; i < LVL_TANK_COUNT; i++) {
        if (GPIO_Pin == wtrLvlMap[i].pin) {

            // MENGAPA PENGURANGAN TICK (current - last) AMAN DARI OVERFLOW?
            // Dalam arsitektur 32-bit (unsigned), pengurangan akan menghasilkan
            // selisih waktu yang persis akurat berkat sifat integer underflow C,
            // bahkan jika tick RTOS meluap kembali ke 0 setiap ~49 hari.
            if ((current_tick - wtrLvlMap[i].last_irq_tick) > DEBOUNCE_TIME_TICKS) {
                wtrLvlMap[i].last_irq_tick = current_tick;

                // Baca status hardware nyata di pin
                bool is_reached = (HAL_GPIO_ReadPin(wtrLvlMap[i].port, wtrLvlMap[i].pin) == GPIO_PIN_RESET);

                // Filter lapis kedua: Hanya kirim event jika benar-benar ada perubahan status
                if (wtrLvlMap[i].current_state != is_reached) {
                    wtrLvlMap[i].current_state = is_reached;

                    if (wtrLvlQueue != NULL) {
                        WtrLvl_Event_t event;
                        event.sensor = (WtrLvl_Types)i;
                        event.is_reached = is_reached;
                        // Non-Blocking push ke Queue
                        xQueueSendFromISR(wtrLvlQueue, &event, &xHigherPriorityTaskWoken);
                    }
                }
            }
            break; // Pin ditemukan, cegah loop berlanjut
        }
    }

    // Force Context Switch jika paket berhasil membangunkan Task prioritas tinggi (App Task)
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
