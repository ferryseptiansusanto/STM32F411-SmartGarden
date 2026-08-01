/**
 * @file command_task.h
 * @brief Antarmuka Kurir Universal (Command Task)
 * * Mengelola task FreeRTOS yang mendengarkan interupsi data masuk
 * secara asinkron tanpa membebani siklus CPU.
 *
 *
 *  Created on: 13 May 2026
 *      Author: ferry
 */

#ifndef COMMAND_TASK_H_
#define COMMAND_TASK_H_

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "uart_wrapper.h"

/**
 * @brief Menginisialisasi dan membuat Task penerima komunikasi (Command Task).
 * @param priority Prioritas Task FreeRTOS
 * @param phy_device Pointer ke konteks hardware UART fisik
 * @param app_queue Handle antrean (Queue) FSM utama penerima event
 */
void CMD_AppTaskCreate(UBaseType_t priority, UART_Context *phy_device, QueueHandle_t app_queue);

/**
 * @brief Task FreeRTOS independen penanganan penerimaan data (Rx).
 * @param pvParameters Parameter bawaan task RTOS
 */
void CMD_TaskRx(void *pvParameters);

#endif /* COMMAND_TASK_H_ */
