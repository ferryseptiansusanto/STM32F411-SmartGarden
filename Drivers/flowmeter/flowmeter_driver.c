/*
 * flowmeter_driver.c
 *
 *  Created on: 11 Jun 2026
 *      Author: ferry
 */



#include "flowmeter/flowmeter_driver.h"
#include <stddef.h>

// Maksimal channel per Timer STM32 adalah 4
#define MAX_TIMER_CHANNELS 4

// Direct Mapping Pointer untuk eksekusi O(1) di ISR.
// Asumsi: Semua sensor flow menggunakan channel dari Timer yang sama (misal TIM2).
static FlowSensor_t* channel_map[MAX_TIMER_CHANNELS] = {NULL};

// Lookup table untuk mengubah nilai bitmask HAL_TIM_ACTIVE_CHANNEL_x menjadi index 0-3
// HAL_TIM_ACTIVE_CHANNEL_1 = 1 (0x01) -> Index 0
// HAL_TIM_ACTIVE_CHANNEL_2 = 2 (0x02) -> Index 1
// HAL_TIM_ACTIVE_CHANNEL_3 = 4 (0x04) -> Index 2
// HAL_TIM_ACTIVE_CHANNEL_4 = 8 (0x08) -> Index 3
static const uint8_t active_ch_to_index[9] = {
    0, 0, 1, 0, 2, 0, 0, 0, 3  // Indexing aman tanpa fungsi kompleks
};

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

    // 1. Pre-calculate HAL Active Channel (Agar tidak dilakukan di dalam ISR)
    // 2. Petakan sensor ke array channel_map (Direct Mapping)
    uint8_t map_index = 0;
    switch(channel) {
        case TIM_CHANNEL_1:
            sensor->hal_active_channel = HAL_TIM_ACTIVE_CHANNEL_1;
            map_index = 0; break;
        case TIM_CHANNEL_2:
            sensor->hal_active_channel = HAL_TIM_ACTIVE_CHANNEL_2;
            map_index = 1; break;
        case TIM_CHANNEL_3:
            sensor->hal_active_channel = HAL_TIM_ACTIVE_CHANNEL_3;
            map_index = 2; break;
        case TIM_CHANNEL_4:
            sensor->hal_active_channel = HAL_TIM_ACTIVE_CHANNEL_4;
            map_index = 3; break;
        default:
            sensor->hal_active_channel = HAL_TIM_ACTIVE_CHANNEL_CLEARED;
            return; // Channel tidak valid
    }

    // Daftarkan pointer sensor ke mapping global agar dipanggil oleh ISR
    channel_map[map_index] = sensor;
}

void FlowSensor_Start(FlowSensor_t *sensor) {
    if (sensor != NULL && sensor->htim != NULL) {
        HAL_TIM_IC_Start_IT(sensor->htim, sensor->tim_channel);
    }
}

void FlowSensor_Stop(FlowSensor_t *sensor) {
    if (sensor != NULL && sensor->htim != NULL) {
        HAL_TIM_IC_Stop_IT(sensor->htim, sensor->tim_channel);
    }
}

void FlowSensor_SetType(FlowSensor_t *sensor, uint16_t type) {
    if (sensor == NULL) return;
    sensor->pulse1liter = (float)type;
}

// -------------------------------------------------------------------------
// ISR Handler - SANGAT CEPAT, TANPA LOOPING, TANPA SWITCH-CASE
// -------------------------------------------------------------------------
void FlowSensor_ProcessIC(TIM_HandleTypeDef *htim) {
    uint32_t active_ch = htim->Channel;

    // Pastikan channel valid (bukan CLEARED dan max nilai mask adalah 8)
    if (active_ch == HAL_TIM_ACTIVE_CHANNEL_CLEARED || active_ch > 8) {
        return;
    }

    // O(1) Lookup: Ubah flag bitmask channel ke index 0-3
    uint8_t idx = active_ch_to_index[active_ch];

    // Cek apakah ada sensor yang terdaftar di index ini dan menggunakan timer yang sama
    if (channel_map[idx] != NULL && channel_map[idx]->htim->Instance == htim->Instance) {
        channel_map[idx]->pulse++;
    }
}

// -------------------------------------------------------------------------
// Fungsi Pembacaan Volume - Dioptimasi dengan CRITICAL SECTION yang presisi
// -------------------------------------------------------------------------
void FlowSensor_Read(FlowSensor_t *sensor, long calibration) {
    if (sensor == NULL) return;

    TickType_t current_time = xTaskGetTickCount();
    TickType_t elapsed_ticks = current_time - sensor->time_before;

    // Cegah pembagian dengan nol jika fungsi dipanggil terlalu cepat
    if (elapsed_ticks == 0) return;

    float elapsed_seconds = ((float)elapsed_ticks * portTICK_PERIOD_MS) / 1000.0f;

    // Amankan pengambilan pulsa lokal dari gangguan ISR
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

