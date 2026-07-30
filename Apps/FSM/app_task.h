/*
 * app_task.h
 *
 *  Created on: 3 Jul 2026
 *      Author: ferry
 */

#ifndef APPS_APP_TASK_H_
#define APPS_APP_TASK_H_

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Ekspos Queue agar modul lain (seperti Bluetooth/UART) bisa mengirim perintah
extern QueueHandle_t appQueue;

// FSM Utama Aplikasi
typedef enum {
    APP_STATE_IDLE,
    APP_STATE_IRRIGATION,
    APP_STATE_FERTILIZATION
} AppState_t;

// Sub-State untuk Irigasi
typedef enum {
    IRR_STATE_PREPARE,
    IRR_STATE_WATERING,
    IRR_STATE_COMPLETE
} IrrigationState_t;

// Sub-State untuk Pemupukan
typedef enum {
    FERT_STATE_PREPARE,
    FERT_STATE_DOSING,
    FERT_STATE_MIXING,
    FERT_STATE_DISTRIBUTING,
    FERT_STATE_COMPLETE
} FertilizationState_t;

// Deklarasi Fungsi Utama
void APP_TaskCreate(UBaseType_t priority);

#endif /* APPS_APP_TASK_H_ */
