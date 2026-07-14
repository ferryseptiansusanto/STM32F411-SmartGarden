/*
 * water_lvl_driver.c
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */


#include "water_lvl_driver.h"

// Struktur mapping sensor level -> port & pin
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} WtrLvlMap;

static const WtrLvlMap wtrLvlMap[WATER_LVL_COUNT] = {
    [WATER_LVL_FULL]  = {WATER_LVL_1_GPIO_Port,  WATER_LVL_1_Pin},  // Float switch atas (tangki penuh)
    [WATER_LVL_EMPTY] = {WATER_LVL_2_GPIO_Port, WATER_LVL_2_Pin}, // Float switch bawah (tangki kosong)
};

void WtrLvl_Init(void) {
    // Tidak ada inisialisasi khusus dari sisi MCU untuk pin input
    // (mode GPIO_MODE_INPUT + pull-up sudah diatur di MX_GPIO_Init / CubeMX)
    // Fungsi ini disediakan sebagai tempat untuk validasi/self-test di masa depan.
}

// Fungsi generik pembaca status sensor level
// Asumsi wiring: pull-up internal, float switch NO ke GND
// -> LOW (GPIO_PIN_RESET) = kontak tertutup = level tercapai
bool WtrLvl_Read(WtrLvl_Types type) {
    if (type >= WATER_LVL_COUNT) return false;

    GPIO_PinState state = HAL_GPIO_ReadPin(wtrLvlMap[type].port, wtrLvlMap[type].pin);
    return (state == GPIO_PIN_RESET);
}

bool isWtrLvl_Full(void) {
    return WtrLvl_Read(WATER_LVL_FULL);
}

bool isWtrLvl_Empty(void) {
    return WtrLvl_Read(WATER_LVL_EMPTY);
}
