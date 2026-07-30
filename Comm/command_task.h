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
#include "usart_protocol.h" // Sesuai struktur data Anda
#include "uart_wrapper.h"   // Untuk UART_Context

// ---------------------------------------------------------
// [TIDAK ADA LAGI EXTERN QUEUE DI SINI]
// Karena queue sudah dienkapsulasi (static) di dalam .c
// ---------------------------------------------------------

// Prototipe Fungsi Pembuat Task (Sudah ditambahkan parameter Queue)
void CMD_AppTaskCreate(UBaseType_t priority, UART_Context *phy_device, QueueHandle_t app_queue);

// Prototipe Fungsi Task Utama
void CMD_TaskTx(void *pvParameters);
void CMD_TaskRx(void *pvParameters);

// Prototipe Fungsi Pengirim Pesan (Nama diubah agar tidak bentrok dengan wrapper)
BaseType_t CMD_Task_SendMessage(const USART_Message *msg);

#endif /* INC_TASKS_COMMAND_TASK_H_ */
