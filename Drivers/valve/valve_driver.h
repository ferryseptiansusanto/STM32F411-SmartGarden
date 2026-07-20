/*
 * valve_driver.h
 *
 *  Created on: 3 Jul 2026
 *      Author: ferry
 */


#ifndef DRIVERS_VALVE_DRIVER_H_
#define DRIVERS_VALVE_DRIVER_H_

#include "main.h"

typedef enum {
    VALVE_TANK_IN = 0,
    VALVE_PUPUK_1,
    VALVE_PUPUK_2,
    VALVE_PUPUK_3,
    VALVE_PUPUK_4,
    VALVE_PUPUK_5,
    VALVE_TANK_OUT,
    VALVE_WATER_IN,
    VALVE_MAX  // <-- Sentinel Value: Otomatis menyimpan jumlah total elemen valve
} ValveType;

void Valve_Init(void);
void Valve_SetState(ValveType valve, GPIO_PinState state);
void Valve_Open(ValveType valve);
void Valve_Close(ValveType valve);

#endif /* DRIVERS_VALVE_DRIVER_H_ */
