/*
 * water_lvl_driver.c
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */

#include "water_lvl_driver.h"

// Waktu debounce 200ms. Riak air di bawah durasi ini akan diabaikan oleh sistem.
#define DEBOUNCE_TIME_MS 200

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint32_t last_irq_tick; // Menyimpan waktu terakhir interupsi terjadi
    bool current_state;     // Menyimpan status pelampung yang sudah ter-debounce
} WtrLvlMap;

static WtrLvlMap wtrLvlMap[WATER_LVL_COUNT] = {
    [WATER_LVL_FULL]  = {WATER_LVL_1_GPIO_Port, WATER_LVL_1_Pin, 0, false},
    [WATER_LVL_EMPTY] = {WATER_LVL_2_GPIO_Port, WATER_LVL_2_Pin, 0, false},
};

QueueHandle_t wtrLvlQueue = NULL;

void WtrLvl_Init(void) {
    // 1. Buat Queue untuk menampung event perubahan level air (Ukuran 5 cukup)
    wtrLvlQueue = xQueueCreate(5, sizeof(WtrLvl_Event_t));

    // 2. Baca status fisik awal saat mesin baru dinyalakan
    wtrLvlMap[WATER_LVL_FULL].current_state =
        (HAL_GPIO_ReadPin(wtrLvlMap[WATER_LVL_FULL].port, wtrLvlMap[WATER_LVL_FULL].pin) == GPIO_PIN_RESET);

    wtrLvlMap[WATER_LVL_EMPTY].current_state =
        (HAL_GPIO_ReadPin(wtrLvlMap[WATER_LVL_EMPTY].port, wtrLvlMap[WATER_LVL_EMPTY].pin) == GPIO_PIN_RESET);
}

// Mengembalikan status yang ada di memori (sudah terproteksi debouncing)
bool WtrLvl_Read(WtrLvl_Types type) {
    if (type >= WATER_LVL_COUNT) return false;
    return wtrLvlMap[type].current_state;
}

bool isWtrLvl_Full(void) {
    return WtrLvl_Read(WATER_LVL_FULL);
}

bool isWtrLvl_Empty(void) {
    return WtrLvl_Read(WATER_LVL_EMPTY);
}

// --- FUNGSI INTERUPSI (EXTI HANDLER) ---
// Fungsi ini dipicu murni oleh hardware, menembus batasan task RTOS
void WtrLvl_EXTI_Callback(uint16_t GPIO_Pin) {
    uint32_t current_tick = xTaskGetTickCountFromISR(); // Ambil waktu OS saat ini
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    for (int i = 0; i < WATER_LVL_COUNT; i++) {
        if (GPIO_Pin == wtrLvlMap[i].pin) {

            // Software Debouncing: Abaikan jika interupsi terjadi sangat berdekatan
            if ((current_tick - wtrLvlMap[i].last_irq_tick) > DEBOUNCE_TIME_MS) {
                wtrLvlMap[i].last_irq_tick = current_tick;

                // Baca status fisik pin (karena EXTI kita set Rising & Falling)
                bool is_reached = (HAL_GPIO_ReadPin(wtrLvlMap[i].port, wtrLvlMap[i].pin) == GPIO_PIN_RESET);

                // Cek apakah status benar-benar berubah
                if (wtrLvlMap[i].current_state != is_reached) {
                    wtrLvlMap[i].current_state = is_reached;

                    // Lempar notifikasi ke Queue agar Task FSM Control terbangun
                    if (wtrLvlQueue != NULL) {
                        WtrLvl_Event_t event;
                        event.sensor = (WtrLvl_Types)i;
                        event.is_reached = is_reached;
                        xQueueSendFromISR(wtrLvlQueue, &event, &xHigherPriorityTaskWoken);
                    }
                }
            }
            break; // Pin ditemukan, hentikan loop
        }
    }

    // Beri tahu OS untuk berpindah ke Task prioritas tinggi jika ada yang sedang menunggu Queue ini
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
