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
#include "flowmeter_type.h"

typedef struct {
    GPIO_TypeDef* gpio_port;
    uint16_t gpio_pin;

    volatile uint32_t total_pulse;
    volatile uint32_t pulse;

    float pulse1liter;
    float flowrate_hour;
    float flowrate_minute;
    float flowrate_second;
    float volume;

    TickType_t time_before;
} FlowSensor_t;

void FlowSensor_Init(FlowSensor_t *sensor, uint16_t type, GPIO_TypeDef* port, uint16_t pin);
void FlowSensor_Count(FlowSensor_t *sensor);
void FlowSensor_ProcessEXTI(uint16_t GPIO_Pin);
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

