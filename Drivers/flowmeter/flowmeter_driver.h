/*
 * flowmeter_driver.h
 *
 *  Created on: 11 Jun 2026
 *      Author: ferry
 */
/*
 * flowmeter_driver.h
 */

#ifndef FLOWMETER_FLOWMETER_DRIVER_H_
#define FLOWMETER_FLOWMETER_DRIVER_H_

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "flowmeter_type.h" // Asumsi file ini tetap sama

typedef struct {
    TIM_HandleTypeDef* htim; // Pointer ke Timer, contoh: &htim2
    uint32_t tim_channel;    // Channel Timer, contoh: TIM_CHANNEL_1

    volatile uint32_t total_pulse;
    volatile uint32_t pulse;

    float pulse1liter;
    float flowrate_hour;
    float flowrate_minute;
    float flowrate_second;
    float volume;

    TickType_t time_before;
} FlowSensor_t;

// Menggunakan TIM_HandleTypeDef dan Channel, bukan lagi GPIO
void FlowSensor_Init(FlowSensor_t *sensor, uint16_t type, TIM_HandleTypeDef* htim, uint32_t channel);

// Fungsi baru untuk memulai dan menghentikan interupsi timer (Input Capture)
void FlowSensor_Start(FlowSensor_t *sensor);
void FlowSensor_Stop(FlowSensor_t *sensor);

// Fungsi ini akan menggantikan FlowSensor_ProcessEXTI
void FlowSensor_ProcessIC(TIM_HandleTypeDef *htim);

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

