/**
 * @file    water_lvl_driver.h
 * @brief   Driver Sensor Batas Air (Water Level Float Switch) berbasis EXTI.
 * @note    Event-Driven dengan Software Debouncing terintegrasi FreeRTOS Queue.
 * @author  Ferry
 * @date    14 Jul 2026
 */

#ifndef WATER_LVL_DRIVER_H_
#define WATER_LVL_DRIVER_H_

#include "main.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"

/**
 * @brief Enumerasi ID pelampung tangki.
 */
typedef enum {
    LVL_TANK_FULL = 0,
    LVL_TANK_EMPTY,
    LVL_TANK_COUNT
} WtrLvl_Types;

/**
 * @brief Struktur data pesan (Event) yang dikirim dari ISR ke FSM Task.
 * MENGAPA STRUCT? Memudahkan pengiriman multiple data (ID sensor & Status)
 * dalam satu paket antrean (Queue) secara atomic.
 */
typedef struct {
    WtrLvl_Types sensor;
    bool is_reached; ///< true = air menyentuh pelampung (pin ditarik LOW)
} WtrLvl_Event_t;

/**
 * @brief Handle Antrean (Queue) global untuk diakses oleh Task Pendengar (FSM).
 */
extern QueueHandle_t wtrLvlQueue;

/**
 * @brief Menginisialisasi sensor batas air dan membuat RTOS Queue.
 */
void WtrLvl_Init(void);

/**
 * @brief Membaca status sensor terakhir dari memori (Bebas Bouncing).
 * @param type ID Sensor (LVL_TANK_FULL / LVL_TANK_EMPTY)
 * @return true jika air mencapai sensor, false jika tidak.
 */
bool WtrLvl_Read(WtrLvl_Types type);

bool isWtrLvl_Full(void);
bool isWtrLvl_Empty(void);

/**
 * @brief ISR Handler (Callback) untuk EXTI Water Level.
 * WAJIB dipanggil di dalam HAL_GPIO_EXTI_Callback().
 * @param GPIO_Pin Pin EXTI yang memicu interupsi.
 */
void WtrLvl_EXTI_Callback(uint16_t GPIO_Pin);

#endif /* WATER_LVL_DRIVER_H_ */
