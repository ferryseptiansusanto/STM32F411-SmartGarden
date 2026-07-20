/*
 * app_task.h
 *
 *  Created on: 3 Jul 2026
 *      Author: ferry
 */

#ifndef APPS_APP_TASK_H_
#define APPS_APP_TASK_H_

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// --- CONFIGURATION HEADER (Konstanta Target Sistem) ---
#define TARGET_VOL_IRIGASI          20.0f   // Target irigasi rutin: 20 Liter
#define TARGET_VOL_AIR_PENGENCER     2.0f   // Pemupukan Tahap 1: 2 Liter
#define TARGET_VOL_PUPUK             0.01f  // Pemupukan Tahap 2: 10ml = 0.01 Liter
#define TARGET_VOL_IRIGASI_FERT     15.0f   // Pemupukan Tahap 4: Target buang tangki mixing

#define TARGET_EC_MAX                2.5f   // Batas atas kepekatan nutrisi EC
#define MIXING_DURATION_MS           30000  // Tahap 3: Durasi aduk mixer (30 detik)

// --- Struktur Event Queue ---
typedef enum {
    EVENT_TYPE_KEYPAD,
    EVENT_TYPE_BLUETOOTH_CSV,
    EVENT_TYPE_TRIGGER_IRRIG,
    EVENT_TYPE_TRIGGER_FERT
} EventType_t;

typedef struct {
    EventType_t type;
    union {
        struct { char key; } keypad;
        struct { char buffer[64]; } bluetooth;
    } data;
} CommandEvent;

extern QueueHandle_t appQueue;

// --- API Publik ---
void APP_TaskCreate(UBaseType_t priority);
void HandleIrrigationRoutine(void);
void HandleFertilizationRoutine(void);

#endif /* APPS_APP_TASK_H_ */
