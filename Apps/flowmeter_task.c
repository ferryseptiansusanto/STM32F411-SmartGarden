/*
 * flowmeter_task.c
 *
 *  Created on: 11 Jun 2026
 *      Author: ferry
 */

#include "flowmeter/flowmeter_driver.h"
#include "tim.h"
#include <stdio.h>

FlowSensor_t fm_inlet;
FlowSensor_t fm_outlet;
FlowSensor_t fm_fert;


// Task implementation function
void vFlowmeterTask(void *pvParameters) {
    // 1. Inisialisasi parameter fisik dan logis sensor
    FlowSensor_Init(&fm_inlet,  FLOW_SENSOR_INLET,  YFS201, &htim2, TIM_CHANNEL_1);
    FlowSensor_Init(&fm_outlet, FLOW_SENSOR_OUTLET, YFS201, &htim5, TIM_CHANNEL_2);
    FlowSensor_Init(&fm_fert,   FLOW_SENSOR_FERT,   YFS201, &htim9, TIM_CHANNEL_1);

    // 2. [SANGAT PENTING] Start Hardware Counter untuk menghitung pulsa!
    FlowSensor_Start(&fm_inlet);
    FlowSensor_Start(&fm_outlet);
    FlowSensor_Start(&fm_fert);

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); // Update setiap 1 detik

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // 3. Kalkulasi flow rate dan volume SEMUA sensor setiap 1 detik
        FlowSensor_Read(&fm_inlet);
        FlowSensor_Read(&fm_outlet);
        FlowSensor_Read(&fm_fert);

        // Mengambil data untuk inlet sebagai contoh
        float l_per_menit = FlowSensor_GetFlowRate_M(&fm_inlet);
        float total_liter = FlowSensor_GetVolume(&fm_inlet);

        // 4. Casting ke (void) menghilangkan warning "unused variable" dari kompiler
        (void)l_per_menit;
        (void)total_liter;

        // Jika Anda ingin menguji di serial monitor, silakan hilangkan komen di bawah:
        // printf("Flow Inlet: %.2f L/min, Total Inlet: %.2f Liter\r\n", l_per_menit, total_liter);
    }
}

// Clean initialization pattern
void Flowmeter_TaskCreate(UBaseType_t priority) {
    // Karena sensor sudah menjadi variabel global, pvParameters bisa diset NULL
    xTaskCreate(vFlowmeterTask, "TaskFlowmeter", 512, NULL, priority, NULL);
}



