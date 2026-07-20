/*
 * flowmeter_driver.h
 *
 *  Created on: 11 Jun 2026
 *      Author: ferry
 */

#ifndef FLOWMETER_FLOWMETER_DRIVER_H_
#define FLOWMETER_FLOWMETER_DRIVER_H_

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "flowmeter/flowmeter_type.h"

typedef struct {
    TIM_HandleTypeDef* htim;     // Pointer ke Timer, contoh: &htim2
    uint32_t tim_channel;        // Channel Timer (CubeMX), contoh: TIM_CHANNEL_1
    uint32_t hal_active_channel; // Cache untuk HAL Active Channel (Optimasi ISR)

    volatile uint32_t total_pulse;
    volatile uint32_t pulse;

    float pulse1liter;
    float flowrate_hour;
    float flowrate_minute;
    float flowrate_second;
    float volume;

    TickType_t time_before;
} FlowSensor_t;

// Prototipe Fungsi
void FlowSensor_Init(FlowSensor_t *sensor, uint16_t type, TIM_HandleTypeDef* htim, uint32_t channel);
void FlowSensor_Start(FlowSensor_t *sensor);
void FlowSensor_Stop(FlowSensor_t *sensor);
void FlowSensor_ProcessIC(TIM_HandleTypeDef *htim); // ISR Handler
void FlowSensor_Read(FlowSensor_t *sensor, long calibration);
void FlowSensor_SetType(FlowSensor_t *sensor, uint16_t type);

uint32_t FlowSensor_GetPulse(FlowSensor_t *sensor);
float FlowSensor_GetFlowRate_H(FlowSensor_t *sensor);
float FlowSensor_GetFlowRate_M(FlowSensor_t *sensor);
float FlowSensor_GetFlowRate_S(FlowSensor_t *sensor);
float FlowSensor_GetVolume(FlowSensor_t *sensor);

void FlowSensor_ResetPulse(FlowSensor_t *sensor);
void FlowSensor_ResetVolume(FlowSensor_t *sensor);

#endif /* FLOWMETER_FLOWMETER_DRIVER_H_ */

