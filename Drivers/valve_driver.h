/*
 * valve_driver.h
 *
 *  Created on: 3 Jul 2026
 *      Author: ferry
 */

#ifndef VALVE_DRIVER_H_
#define VALVE_DRIVER_H_

#include "main.h"

typedef enum {
    VALVE_INPUT,
    VALVE_PUPUK_1,
    VALVE_PUPUK_2,
    VALVE_PUPUK_3,
    VALVE_PUPUK_4,
    VALVE_PUPUK_5,
    VALVE_OUTPUT
} ValveType;

void Valve_Init(void);
void Valve_Open(ValveType valve);
void Valve_Close(ValveType valve);

#endif /* VALVE_DRIVER_H_ */
