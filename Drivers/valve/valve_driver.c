/*
 * valve_driver.c
 *
 * Created on: 3 Jul 2026
 * Author: ferry
 */

#include "valve_driver.h"

#include <stddef.h> // Diperlukan untuk macro NULL

// Struktur mapping valve → port & pin
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} ValveMap_t;

// 1. Tentukan ukuran array secara eksplisit menggunakan VALVE_MAX
static const ValveMap_t valveMap[VALVE_MAX] = {
    [VALVE_TANK_IN]   = {VALVE_TANK_IN_GPIO_Port, VALVE_TANK_IN_Pin},
    [VALVE_PUPUK_1] = {VALVE_FERT1_GPIO_Port, VALVE_FERT1_Pin},
    [VALVE_PUPUK_2] = {VALVE_FERT2_GPIO_Port, VALVE_FERT2_Pin},
    [VALVE_PUPUK_3] = {VALVE_FERT3_GPIO_Port, VALVE_FERT3_Pin},
    [VALVE_PUPUK_4] = {VALVE_FERT4_GPIO_Port, VALVE_FERT4_Pin},
    [VALVE_PUPUK_5] = {VALVE_FERT5_GPIO_Port, VALVE_FERT5_Pin},
	[VALVE_TANK_OUT]  = {VALVE_TANK_OUT_GPIO_Port, VALVE_TANK_OUT_Pin},
	[VALVE_WATER_IN]  = {VALVE_WATER_IN_GPIO_Port, VALVE_WATER_IN_Pin},
};

// 2. COMPILE-TIME PROTECTION (Proteksi saat Build Proyek)
// Jika di masa depan Anda menambah elemen di ValveType (enum) tetapi lupa
// memperbarui tabel valveMap di atas, kode ini akan menggagalkan kompilasi (Build Error).
_Static_assert((sizeof(valveMap) / sizeof(valveMap[0])) == VALVE_MAX,
               "ERROR: Ukuran alokasi valveMap tidak sinkron dengan data enum ValveType!");

void Valve_Init(void) {
    // Semua valve default tertutup
    for (int i = 0; i < VALVE_MAX; i++) {
        // 3. DEFENSIVE CHECK: Pastikan pin terdefinisi (bukan NULL/tidak terlewati)
        // sebelum mengakses HAL_GPIO_WritePin guna mencegah HardFault.
        if (valveMap[i].port != NULL) {
            HAL_GPIO_WritePin(valveMap[i].port, valveMap[i].pin, GPIO_PIN_RESET);
        }
    }
}

void Valve_SetState(ValveType valve, GPIO_PinState state) {
    // 4. RUNTIME PROTECTION: Validasi input indeks array
    if (valve >= VALVE_MAX || valveMap[valve].port == NULL) {
        return; // Mengabaikan instruksi ilegal jika nilai indeks out-of-bounds
    }

    HAL_GPIO_WritePin(valveMap[valve].port, valveMap[valve].pin, state);
}

void Valve_Open(ValveType valve) {
    Valve_SetState(valve, GPIO_PIN_SET);
}

void Valve_Close(ValveType valve) {
    Valve_SetState(valve, GPIO_PIN_RESET);
}
