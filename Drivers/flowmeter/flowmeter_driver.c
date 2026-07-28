/*
 * flowmeter_driver.c
 *
 * Refactored:
 * 1. Menghapus ISR O(1) -> Diganti menjadi Zero-Interrupt Hardware Counter.
 * 2. Mendukung TIM2/TIM5 (32-bit) dan TIM9 (16-bit).
 * 3. Integrasi sys_calib.
 */
#include "flowmeter/flowmeter_driver.h"
#include "config_data.h" // Mengakses sys_calib global
#include <stddef.h>

void FlowSensor_Init(FlowSensor_t *sensor, FlowSensorID_t id, uint16_t type, TIM_HandleTypeDef* htim, uint32_t channel) {
    if (sensor == NULL) return;

    sensor->sensor_id   = id;     // Simpan Alias/Peran Logis Sensor
    sensor->htim        = htim;   // Hardware Timer
    sensor->tim_channel = channel;// Hardware Channel
    sensor->pulse1liter = (float)type; // Fallback jika sys_calib = 0

    sensor->last_cnt        = 0;
    sensor->total_pulse     = 0;
    sensor->flowrate_second = 0.0f;
    sensor->flowrate_minute = 0.0f;
    sensor->flowrate_hour   = 0.0f;
    sensor->volume          = 0.0f;
    sensor->time_before     = xTaskGetTickCount();
}

void FlowSensor_Start(FlowSensor_t *sensor) {
    if (sensor != NULL && sensor->htim != NULL) {
        // Jalankan Base Hardware Counter tanpa Interrupt
        HAL_TIM_Base_Start(sensor->htim);
        sensor->last_cnt = sensor->htim->Instance->CNT;
    }
}

void FlowSensor_Stop(FlowSensor_t *sensor) {
    if (sensor != NULL && sensor->htim != NULL) {
        HAL_TIM_Base_Stop(sensor->htim);
    }
}

void FlowSensor_SetType(FlowSensor_t *sensor, uint16_t type) {
    if (sensor == NULL) return;
    sensor->pulse1liter = (float)type;
}

void FlowSensor_Read(FlowSensor_t *sensor) {
    if (sensor == NULL || sensor->htim == NULL) return;

    TickType_t current_time = xTaskGetTickCount();
    TickType_t elapsed_ticks = current_time - sensor->time_before;

    if (elapsed_ticks == 0) return;

    float elapsed_seconds = ((float)elapsed_ticks * portTICK_PERIOD_MS) / 1000.0f;

    // 1. Baca Hardware Register CNT
    uint32_t current_cnt = sensor->htim->Instance->CNT;
    uint32_t local_pulse = 0;

    // 2. Penanganan Overflow berdasarkan Resolusi Timer
    if (sensor->htim->Instance == TIM9) {
        // TIM9 = 16-bit Timer (Max 65535)
        local_pulse = (uint16_t)(current_cnt - sensor->last_cnt);
    } else {
        // TIM2 & TIM5 = 32-bit Timer
        local_pulse = (uint32_t)(current_cnt - sensor->last_cnt);
    }

    sensor->last_cnt = current_cnt;

    // 3. Mapping Nilai Kalibrasi berdasarkan LOGICAL ID (AMUNISI UTAMA ABSTRAKSI)
    float total_pulses_per_liter = sensor->pulse1liter; // Default fallback

    switch (sensor->sensor_id) {
        case FLOW_SENSOR_INLET:
            total_pulses_per_liter = (float)sys_calib.fm_inlet_pulse_per_liter;
            break;

        case FLOW_SENSOR_OUTLET:
            total_pulses_per_liter = (float)sys_calib.fm_outlet_pulse_per_liter;
            break;

        case FLOW_SENSOR_FERT:
            total_pulses_per_liter = (float)sys_calib.fm_fert_pulse_per_liter;
            break;

        default:
            break;
    }

    // 4. Kalkulasi Volume dan Aliran Air
    if (total_pulses_per_liter > 0.0f && local_pulse > 0) {
        float current_volume_liters = (float)local_pulse / total_pulses_per_liter;
        sensor->flowrate_second = current_volume_liters / elapsed_seconds;
        sensor->volume += current_volume_liters;
    } else if (local_pulse == 0) {
        sensor->flowrate_second = 0.0f;
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
    sensor->last_cnt = sensor->htim->Instance->CNT;
    sensor->total_pulse = 0;
}

void FlowSensor_ResetVolume(FlowSensor_t *sensor) {
    if (sensor != NULL) {
        sensor->volume = 0.0f;
    }
}
