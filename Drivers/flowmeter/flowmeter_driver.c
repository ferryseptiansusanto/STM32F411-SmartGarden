/*
 * flowmeter_driver.c
 *
 *  Created on: 11 Jun 2026
 *      Author: ferry
 */
#include "flowmeter_driver.h"
#include <stddef.h>
// Pointer internal untuk mencatat sensor aktif (Jembatan EXTI ke Driver)
static FlowSensor_t *p_active_sensor = NULL;

void FlowSensor_Init(FlowSensor_t *sensor, uint16_t type, GPIO_TypeDef* port, uint16_t pin) {
    if (sensor == NULL) return;
    sensor->gpio_port = port;
    sensor->gpio_pin = pin;
    sensor->pulse1liter = (float)type;

    sensor->pulse = 0;
    sensor->total_pulse = 0;
    sensor->flowrate_second = 0.0f;
    sensor->flowrate_minute = 0.0f;
    sensor->flowrate_hour = 0.0f;
    sensor->volume = 0.0f;
    sensor->time_before = xTaskGetTickCount();
}

void FlowSensor_SetType(FlowSensor_t *sensor, uint16_t type) {
    if (sensor == NULL) return;
    sensor->pulse1liter = (float)type;
}

// Keep this lightweight since it runs inside HAL_GPIO_EXTI_Callback ISR
void FlowSensor_Count(FlowSensor_t *sensor) {
    if (sensor != NULL) {
        sensor->pulse++;
    }
}

void FlowSensor_ProcessEXTI(uint16_t GPIO_Pin) {
    if (p_active_sensor != NULL && GPIO_Pin == p_active_sensor->gpio_pin) {
        FlowSensor_Count(p_active_sensor);
    }
}

void FlowSensor_Read(FlowSensor_t *sensor, long calibration) {
    if (sensor == NULL) return;

    TickType_t current_time = xTaskGetTickCount();
    TickType_t elapsed_ticks = current_time - sensor->time_before;

    if (elapsed_ticks == 0) return;

    float elapsed_seconds = ((float)elapsed_ticks * portTICK_PERIOD_MS) / 1000.0f;

    // Use task critical section here because this is called inside a Task, not ISR
    taskENTER_CRITICAL();
    uint32_t local_pulse = sensor->pulse;
    sensor->pulse = 0;
    taskEXIT_CRITICAL();

    float total_pulses_per_liter = sensor->pulse1liter + (float)calibration;

    if (total_pulses_per_liter > 0.0f) {
        float current_volume_liters = (float)local_pulse / total_pulses_per_liter;
        sensor->flowrate_second = current_volume_liters / elapsed_seconds;
        sensor->volume += current_volume_liters;
    }

    sensor->total_pulse += local_pulse;
    sensor->time_before = current_time;
}

uint32_t FlowSensor_GetPulse(FlowSensor_t *sensor) {
    return (sensor != NULL) ? sensor->total_pulse : 0;
}

float FlowSensor_GetFlowRate_H(FlowSensor_t *sensor) {
    if (sensor == NULL) return 0.0f;
    sensor->flowrate_hour = sensor->flowrate_second * 3600.0f;
    return sensor->flowrate_hour;
}

float FlowSensor_GetFlowRate_M(FlowSensor_t *sensor) {
    if (sensor == NULL) return 0.0f;
    sensor->flowrate_minute = sensor->flowrate_second * 60.0f;
    return sensor->flowrate_minute;
}

float FlowSensor_GetFlowRate_S(FlowSensor_t *sensor) {
    return (sensor != NULL) ? sensor->flowrate_second : 0.0f;
}

float FlowSensor_GetVolume(FlowSensor_t *sensor) {
    return (sensor != NULL) ? sensor->volume : 0.0f;
}

void FlowSensor_ResetPulse(FlowSensor_t *sensor) {
    if (sensor == NULL) return;
    taskENTER_CRITICAL();
    sensor->pulse = 0;
    sensor->total_pulse = 0;
    taskEXIT_CRITICAL();
}

void FlowSensor_ResetVolume(FlowSensor_t *sensor) {
    if (sensor != NULL) {
        sensor->volume = 0.0f;
    }
}

