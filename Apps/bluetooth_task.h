/*
 * bluetooth_task.h
 *
 * Created on: 2026
 * Author: ferry
 */

#ifndef BLUETOOTH_TASK_H_
#define BLUETOOTH_TASK_H_

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "uart_wrapper.h"

// Deklarasi Queue agar bisa diakses extern oleh main.c atau command_task.c
extern QueueHandle_t btQueueTx;
extern QueueHandle_t btQueueRx;
extern QueueHandle_t uartQueue;

// Fungsi untuk membuat Task dari main.c
void BLUETOOTH_AppTaskCreate(UART_Context *phy_device);

// Definisi Task RTOS
void BLUETOOTH_TaskTx(void *pvParameters);
void BLUETOOTH_TaskRx(void *pvParameters);

#endif /* BLUETOOTH_TASK_H_ */
