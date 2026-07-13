/*
 * freertos_hooks.c
 *
 *  Created on: 18 Feb 2026
 *      Author: ferry
 */

#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* Hook untuk stack overflow */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("Stack overflow di task: %s\r\n", pcTaskName);
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        // Bisa tambahkan LED blink error
    }
}

/* Hook untuk malloc gagal */
void vApplicationMallocFailedHook(void)
{
    printf("Heap habis!\r\n");
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        // Bisa tambahkan LED blink error
    }
}

/* Optional: Idle hook */
void vApplicationIdleHook(void)
{
    // Dipanggil setiap kali Idle Task jalan
}

/* Optional: Tick hook */
void vApplicationTickHook(void)
{
    // Dipanggil setiap tick interrupt
	//DebugPrintISRPriority();
}
