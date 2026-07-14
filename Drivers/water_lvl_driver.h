/*
 * water_lvl_driver.h
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */

#ifndef WATER_LVL_DRIVER_H_
#define WATER_LVL_DRIVER_H_

#include "main.h"
#include <stdbool.h>

typedef enum {
    WATER_LVL_FULL,
	WATER_LVL_EMPTY,
	WATER_LVL_COUNT
} WtrLvl_Types;

void WtrLvl_Init(void);
bool WtrLvl_Read(WtrLvl_Types type);
bool isWtrLvl_Full(void);
bool isWtrLvl_Empty(void);

#endif /* WATER_LVL_DRIVER_H_ */
