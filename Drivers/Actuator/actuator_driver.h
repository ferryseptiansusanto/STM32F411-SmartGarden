/*
 * actuator_driver.h
 *
 *  Created on: 22 Jul 2026
 *      Author: ferry
 */

#ifndef ACTUATOR_ACTUATOR_DRIVER_H_
#define ACTUATOR_ACTUATOR_DRIVER_H_
#include "main.h"

typedef enum {
    ACT_VALVE_WATER_IN = 0,
    ACT_VALVE_TANK_IN,
    ACT_VALVE_TANK_OUT,
    ACT_VALVE_FERT_1,
    ACT_VALVE_FERT_2,
    ACT_VALVE_FERT_3,
    ACT_VALVE_FERT_4,
    ACT_VALVE_FERT_5,
    ACT_PUMP_OUT,
    ACT_PUMP_FERT,
    ACT_MIXER,
    ACT_MAX // Sentinel Value untuk proteksi ukuran elemen
} ActuatorType_t;

typedef enum {
    ACT_OFF = 0,
    ACT_ON  = 1
} ActuatorState_t;

void Actuator_Init(void);
void Actuator_SetState(ActuatorType_t actuator, ActuatorState_t state);

#endif /* ACTUATOR_ACTUATOR_DRIVER_H_ */
