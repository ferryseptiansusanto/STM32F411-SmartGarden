/*
 * app_task.c
 *
 *  Created on: 3 Jul 2026
 *      Author: ferry
 */


#include "app_task.h"
#include "valve_driver.h"
#include "mixer_driver.h"
#include "sensor_driver.h"
#include "flowmeter_driver.h"
#include "water_lvl_driver.h"
#include "delay.h"
#include "task.h"
#include "FreeRTOS.h"
#include <stdio.h>

QueueHandle_t appQueue;

// --- Handler Penyiraman Rutin ---
void HandleIrrigationRoutine(void) {
    Valve_Open(VALVE_INPUT);
    while (!(isWtrLvl_Full())) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    Valve_Close(VALVE_INPUT);

    Valve_Open(VALVE_OUTPUT);
//    vTaskDelay(pdMS_TO_TICKS(CONFIG_VALVE_OUTPUT_DURATION_MS));
    Valve_Close(VALVE_OUTPUT);
}

// --- Handler Pemupukan ---
void HandleFertilizationRoutine(void) {
    Valve_Open(VALVE_PUPUK_1);
//    while (FlowSensor_GetVolume(&flowSensor) < CONFIG_TARGET_VOLUME_A) {
//        vTaskDelay(pdMS_TO_TICKS(100));
//    }
    Valve_Close(VALVE_PUPUK_1);

    Valve_Open(VALVE_PUPUK_2);
//    while (FlowSensor_GetVolume(&flowSensor) < CONFIG_TARGET_VOLUME_B) {
//        vTaskDelay(pdMS_TO_TICKS(100));
//    }
    Valve_Close(VALVE_PUPUK_2);

    Mixer_On();
//    vTaskDelay(pdMS_TO_TICKS(CONFIG_MIXING_DURATION_MS));
    Mixer_Off();

    Valve_Open(VALVE_OUTPUT);
//    vTaskDelay(pdMS_TO_TICKS(CONFIG_VALVE_OUTPUT_DURATION_MS));
    Valve_Close(VALVE_OUTPUT);
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
