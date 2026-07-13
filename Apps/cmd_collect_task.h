/*
 * command_collector_task.h
 *
 *  Created on: 13 May 2026
 *      Author: ferry
 */

#ifndef INC_TASKS_CMD_COLLECT_TASK_H_
#define INC_TASKS_CMD_COLLECT_TASK_H_

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "usart_datalink.h"
#include "usart_protocol.h"

// Inisialisasi task collector
void CmdCollectTaskInit(QueueHandle_t queue);

// Fungsi task collector
void cmd_collect_task(void *argument);

#endif /* INC_TASKS_CMD_COLLECT_TASK_H_ */
