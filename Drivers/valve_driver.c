/*
 * valve_driver.c
 *
 *  Created on: 3 Jul 2026
 *      Author: ferry
 */

/*
 * valve_driver.c
 *
 *  Created on: 3 Jul 2026
 *      Author: ferry
 */
#include "valve_driver.h"

// Struktur mapping valve → port & pin
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} ValveMap;

static const ValveMap valveMap[] = {
    [VALVE_INPUT]   = {VALVE_IN_GPIO_Port, VALVE_IN_Pin},       // Valve Input
    [VALVE_PUPUK_1] = {PUPUK_1_GPIO_Port, PUPUK_1_Pin}, // Valve Pupuk 1
    [VALVE_PUPUK_2] = {PUPUK_2_GPIO_Port, PUPUK_2_Pin}, // Valve Pupuk 2
    [VALVE_PUPUK_3] = {PUPUK_3_GPIO_Port, PUPUK_3_Pin}, // Valve Pupuk 3
    [VALVE_PUPUK_4] = {PUPUK_4_GPIO_Port, PUPUK_4_Pin}, // Valve Pupuk 4
    [VALVE_PUPUK_5] = {PUPUK_5_GPIO_Port, PUPUK_5_Pin}, // Valve Pupuk 5
    [VALVE_OUTPUT]  = {VALVE_OUT_GPIO_Port, VALVE_OUT_Pin}      // Valve Output
};

void Valve_Init(void) {
    // Semua valve default tertutup
    for (int i = 0; i < sizeof(valveMap)/sizeof(valveMap[0]); i++) {
        HAL_GPIO_WritePin(valveMap[i].port, valveMap[i].pin, GPIO_PIN_RESET);
    }
}

void Valve_SetState(ValveType valve, GPIO_PinState state) {
    HAL_GPIO_WritePin(valveMap[valve].port, valveMap[valve].pin, state);
}

void Valve_Open(ValveType valve) {
    Valve_SetState(valve, GPIO_PIN_SET);
}

void Valve_Close(ValveType valve) {
    Valve_SetState(valve, GPIO_PIN_RESET);
}
