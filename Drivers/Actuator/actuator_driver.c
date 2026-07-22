/*
 * actuator_driver.c
 *
 *  Created on: 22 Jul 2026
 *      Author: ferry
 */


#include "actuator_driver.h"

// Struct untuk memetakan Enum ke pin STM32
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} ActuatorMap_t;

// Tabel pemetaan yang terikat erat (tightly coupled) dengan ActuatorType_t
static const ActuatorMap_t actuatorMap[] = {
    {VALVE_WATER_IN_GPIO_Port, VALVE_WATER_IN_Pin}, // Posisi 0
    {VALVE_TANK_IN_GPIO_Port, VALVE_TANK_IN_Pin},   // Posisi 1
    {VALVE_TANK_OUT_GPIO_Port, VALVE_TANK_OUT_Pin}, // Posisi 2
    {VALVE_FERT1_GPIO_Port, VALVE_FERT1_Pin},    	// Posisi 3
    {VALVE_FERT2_GPIO_Port, VALVE_FERT2_Pin},  		// Posisi 4
    {VALVE_FERT3_GPIO_Port, VALVE_FERT3_Pin},  		// Posisi 5
    {VALVE_FERT4_GPIO_Port, VALVE_FERT4_Pin},  		// Posisi 6
    {VALVE_FERT5_GPIO_Port, VALVE_FERT5_Pin}, 		// Posisi 7
    {PUMP_OUT_GPIO_Port, PUMP_OUT_Pin},             // Posisi 8
    {PUMP_FERT_GPIO_Port, PUMP_FERT_Pin},           // Posisi 9
    {MIXER_GPIO_Port, MIXER_Pin}                    // Posisi 10
};

void Actuator_Init(void) {
    for (int i = 0; i < ACT_MAX; i++) {
        if (actuatorMap[i].port != NULL) {
            HAL_GPIO_WritePin(actuatorMap[i].port, actuatorMap[i].pin, GPIO_PIN_RESET);
        }
    }
}

void Actuator_SetState(ActuatorType_t actuator, ActuatorState_t state) {
    if (actuator >= ACT_MAX) return;

    if (actuatorMap[actuator].port != NULL) {
        HAL_GPIO_WritePin(
            actuatorMap[actuator].port,
            actuatorMap[actuator].pin,
            (state == ACT_ON) ? GPIO_PIN_SET : GPIO_PIN_RESET
        );
    }
}

// Perlindungan Kompilasi: Menghentikan Build jika Enum dan Array tidak sinkron
_Static_assert(sizeof(actuatorMap)/sizeof(actuatorMap[0]) == ACT_MAX, "Ukuran ActuatorMap tidak sinkron dengan Enum!");
