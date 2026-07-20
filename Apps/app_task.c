/*
 * app_task.c
 *
 *  Created on: 3 Jul 2026
 *      Author: ferry
 */


#include "app_task.h"
#include "valve/valve_driver.h"
#include "mixer/mixer_driver.h"
#include "flowmeter/flowmeter_driver.h"
#include "water_lvl/water_lvl_driver.h"
#include "delay.h"
#include "task.h"
#include "FreeRTOS.h"
#include <stdio.h>

QueueHandle_t appQueue;

// --- Handler Penyiraman Rutin ---
void HandleIrrigationRoutine(void) {

}

// --- Handler Pemupukan ---
void HandleFertilizationRoutine(void) {

}

static void vTaskApp(void *pvParameters) {
    CommandEvent evt;

    for (;;) {
        if (xQueueReceive(appQueue, &evt, portMAX_DELAY)) {
            switch (evt.type) {
                case EVENT_TYPE_KEYPAD:
                    // contoh: keypad navigasi menu
                    printf("[APP] Keypad event: %c\n", evt.data.keypad.key);
                    break;

                case EVENT_TYPE_BLUETOOTH_CSV:
                    // contoh: perintah dari Bluetooth
                    printf("[APP] BT CSV: %s\n", evt.data.bluetooth.buffer);
                    break;
            }

             //--- Algoritma penyiraman rutin ---
//            if (/* kondisi jadwal rutin */) {
//            	HandleIrrigationRoutine();
//            }
//
//             --- Algoritma pemupukan ---
//            if (/* kondisi jadwal pupuk */) {
//                HandleFertilizationRoutine();
//            }
        }
    }
}

void APP_TaskCreate(UBaseType_t priority) {
    appQueue = xQueueCreate(10, sizeof(CommandEvent));
    xTaskCreate(vTaskApp, "AppTask", 512, NULL, priority, NULL);
}
