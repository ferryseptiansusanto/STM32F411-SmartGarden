/*
 * command_task.h
 *
 *  Created on: 13 May 2026
 *      Author: ferry
 */

#ifndef INC_TASKS_COMMAND_TASK_H_
#define INC_TASKS_COMMAND_TASK_H_

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "command_event.h"

// Queue untuk komunikasi dari modul input → command_task
extern QueueHandle_t commandQueue;

// Membuat task command (dispatcher + executor)
void COMMAND_TaskCreate(UBaseType_t priority);

#endif /* INC_TASKS_COMMAND_TASK_H_ */
