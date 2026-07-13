/*
 * app_task.h
 *
 *  Created on: 3 Jul 2026
 *      Author: ferry
 */

#ifndef APP_TASK_H_
#define APP_TASK_H_

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "command_event.h"

extern QueueHandle_t appQueue;

void APP_TaskCreate(UBaseType_t priority);
// Handler terpisah untuk logika aplikasi
void HandleIrrigationRoutine(void);
void HandleFertilizationRoutine(void);

#endif /* APP_TASK_H_ */
