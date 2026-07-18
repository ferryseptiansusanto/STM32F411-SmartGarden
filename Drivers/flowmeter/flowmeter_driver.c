/*
 * flowmeter_driver.c
 *
 *  Created on: 11 Jun 2026
 *      Author: ferry
 */

#include "flowmeter/flowmeter_driver.h"
#include <stddef.h>

// Maksimal sensor yang kita gunakan: FM_INLET, FM_OUTLET, FM_FERTILIZER
#define MAX_FLOW_SENSORS 3
static FlowSensor_t* flow_sensors[MAX_FLOW_SENSORS] = {NULL};

void FlowSensor_Init(FlowSensor_t *sensor, uint16_t type, TIM_HandleTypeDef* htim, uint32_t channel) {
    if (sensor == NULL) return;

    sensor->htim = htim;
    sensor->tim_channel = channel;
    sensor->pulse1liter = (float)type;

    sensor->pulse = 0;
    sensor->total_pulse = 0;
    sensor->flowrate_second = 0.0f;
    sensor->flowrate_minute = 0.0f;
    sensor->flowrate_hour = 0.0f;
    sensor->volume = 0.0f;
    sensor->time_before = xTaskGetTickCount();

    // Daftarkan sensor ke memori global agar bisa diproses oleh fungsi Interupsi
    for (int i = 0; i < MAX_FLOW_SENSORS; i++) {
        if (flow_sensors[i] == NULL || flow_sensors[i] == sensor) {
            flow_sensors[i] = sensor;
            break;
        }
    }
}

void FlowSensor_Start(FlowSensor_t *sensor) {
    if (sensor != NULL && sensor->htim != NULL) {
        // Mengaktifkan Input Capture Interrupt
        HAL_TIM_IC_Start_IT(sensor->htim, sensor->tim_channel);
    }
}

void FlowSensor_Stop(FlowSensor_t *sensor) {
    if (sensor != NULL && sensor->htim != NULL) {
        // Mematikan Input Capture Interrupt jika tidak digunakan
        HAL_TIM_IC_Stop_IT(sensor->htim, sensor->tim_channel);
    }
}

void FlowSensor_SetType(FlowSensor_t *sensor, uint16_t type) {
    if (sensor == NULL) return;
    sensor->pulse1liter = (float)type;
}

// Fungsi Internal untuk menambah pulsa (Berjalan di dalam ISR)
static inline void FlowSensor_Count(FlowSensor_t *sensor) {
    if (sensor != NULL) {
        sensor->pulse++;
    }
}

// Jembatan Interupsi Timer (Input Capture)
void FlowSensor_ProcessIC(TIM_HandleTypeDef *htim) {
    for (int i = 0; i < MAX_FLOW_SENSORS; i++) {
        if (flow_sensors[i] != NULL && flow_sensors[i]->htim->Instance == htim->Instance) {

            // Konversi TIM_CHANNEL_x ke konvensi HAL_TIM_ACTIVE_CHANNEL_x
            uint32_t expected_hal_channel = HAL_TIM_ACTIVE_CHANNEL_CLEARED;
            switch(flow_sensors[i]->tim_channel) {
                case TIM_CHANNEL_1: expected_hal_channel = HAL_TIM_ACTIVE_CHANNEL_1; break;
                case TIM_CHANNEL_2: expected_hal_channel = HAL_TIM_ACTIVE_CHANNEL_2; break;
                case TIM_CHANNEL_3: expected_hal_channel = HAL_TIM_ACTIVE_CHANNEL_3; break;
                case TIM_CHANNEL_4: expected_hal_channel = HAL_TIM_ACTIVE_CHANNEL_4; break;
            }

            // Jika interupsi berasal dari channel milik sensor ini, tambah pulsanya
            if (htim->Channel == expected_hal_channel) {
                FlowSensor_Count(flow_sensors[i]);
            }
        }
    }
}

// Fungsi pembacaan volume di bawah ini tidak ada perubahan logika dari versi sebelumnya.
void FlowSensor_Read(FlowSensor_t *sensor, long calibration) {
    if (sensor == NULL) return;

    TickType_t current_time = xTaskGetTickCount();
    TickType_t elapsed_ticks = current_time - sensor->time_before;

    if (elapsed_ticks == 0) return;

    float elapsed_seconds = ((float)elapsed_ticks * portTICK_PERIOD_MS) / 1000.0f;

    taskENTER_CRITICAL();
    uint32_t local_pulse = sensor->pulse;
    sensor->pulse = 0; // reset local pulse
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

