/*
 * command_task.h
 *
 *  Created on: 19 May 2026
 *      Author: ferry
 */
#include "command_task.h"
#include "command_event.h"
//#include "app_task.h"
#include "stdio.h"

QueueHandle_t commandQueue;

static void vTaskCommand(void *pvParameters) {
    CommandEvent cmd;
    while (1) {
        if (xQueueReceive(commandQueue, &cmd, portMAX_DELAY)) {
//            printf("[COMMAND] type=%d, key=%c, long=%d\r\n", cmd.type, cmd.data.keypad.key, cmd.data.keypad.longPress);

            // forward ke appQueue
//            xQueueSend(appQueue, &cmd, 0);
        }
    }
}

void COMMAND_TaskCreate(UBaseType_t priority) {
    commandQueue = xQueueCreate(10, sizeof(CommandEvent));
    xTaskCreate(vTaskCommand, "CommandTask", 256, NULL, priority, NULL);
}
