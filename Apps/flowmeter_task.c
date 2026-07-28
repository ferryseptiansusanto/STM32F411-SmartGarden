/*
 * flowmeter_task.c
 *
 *  Created on: 11 Jun 2026
 *      Author: ferry
 */

#include "flowmeter/flowmeter_driver.h"
#include <stdio.h>

// Task implementation function
void vFlowmeterTask(void *pvParameters) {
    FlowSensor_t *sensor_ctx = (FlowSensor_t *)pvParameters;
    FlowSensor_Init(&fm_inlet,  FLOW_SENSOR_INLET,  YFS201, &htim2, TIM_CHANNEL_1);
    FlowSensor_Init(&fm_outlet, FLOW_SENSOR_OUTLET, YFS201, &htim5, TIM_CHANNEL_2);
    FlowSensor_Init(&fm_fert,   FLOW_SENSOR_FERT,   YFS201, &htim9, TIM_CHANNEL_1);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Calculate flow based on pulses accumulated over the last 1 second
        FlowSensor_Read(sensor_ctx);

        float l_per_menit = FlowSensor_GetFlowRate_M(sensor_ctx);
        float total_liter  = FlowSensor_GetVolume(sensor_ctx);

       // printf("Flow: %.2f L/min, Total: %.2f Liter\r\n", l_per_menit, total_liter);
    }
}

// Clean initialization pattern using contextual parameters
void Flowmeter_TaskCreate(FlowSensor_t *Flowmeter_Ctx, UBaseType_t priority) {
    if (Flowmeter_Ctx == NULL) return;

    // Explicitly pass the sensor context parameter to the created task
    xTaskCreate(vFlowmeterTask, "TaskFlowmeter", 512, (void *)Flowmeter_Ctx, priority, NULL);
}


