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
#include "FreeRTOS.h"
#include "queue.h"

typedef enum {
    WATER_LVL_FULL = 0,
    WATER_LVL_EMPTY,
    WATER_LVL_COUNT
} WtrLvl_Types;

// Struktur pesan yang akan dikirim dari Interrupt ke Task
typedef struct {
    WtrLvl_Types sensor;
    bool is_reached; // true = air menyentuh pelampung (pin LOW)
} WtrLvl_Event_t;

// Queue global agar bisa dibaca oleh FSM Task
extern QueueHandle_t wtrLvlQueue;

void WtrLvl_Init(void);

// Membaca status debounce terakhir (bukan raw pin)
bool WtrLvl_Read(WtrLvl_Types type);
bool isWtrLvl_Full(void);
bool isWtrLvl_Empty(void);

// Harus dipanggil di dalam HAL_GPIO_EXTI_Callback() di stm32f4xx_it.c atau main.c
void WtrLvl_EXTI_Callback(uint16_t GPIO_Pin);

#endif /* WATER_LVL_DRIVER_H_ */
