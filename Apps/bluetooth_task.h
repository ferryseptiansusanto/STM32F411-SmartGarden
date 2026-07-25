#ifndef BLUETOOTH_TASK_H
#define BLUETOOTH_TASK_H

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
void BLUETOOTH_AppTaskCreate(UBaseType_t priority, UART_Context *phy_device, QueueHandle_t app_queue);

// Prototipe Fungsi Task Utama
void BLUETOOTH_TaskTx(void *pvParameters);
void BLUETOOTH_TaskRx(void *pvParameters);

// Prototipe Fungsi Pengirim Pesan (Nama diubah agar tidak bentrok dengan wrapper)
BaseType_t BLUETOOTH_Task_SendMessage(const USART_Message *msg);

#endif /* BLUETOOTH_TASK_H */
