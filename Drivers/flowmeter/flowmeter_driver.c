/**
 * @file flowmeter_driver.c
 * @brief Implementasi Hardware-Counter Flowmeter.
 * @author Ferry
 */

#include "flowmeter/flowmeter_driver.h"
#include "../Apps/Config/config_data.h" // Mengakses sys_calib global (RAM tersinkronisasi dari EEPROM)
#include <stddef.h>

void FlowSensor_Init(FlowSensor_t *sensor, FlowSensorID_t id, uint16_t type, TIM_HandleTypeDef* htim, uint32_t channel) {
    if (sensor == NULL) return;

    sensor->sensor_id   = id;
    sensor->htim        = htim;
    sensor->tim_channel = channel;
    sensor->pulse1liter = (float)type;

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
        // MENGAPA MENGGUNAKAN HAL_TIM_Base_Start?
        // Kita HANYA menghidupkan counter hardware periferal.
        // TIDAK ADA _IT (Interrupt) yang dihidupkan, sehingga CPU terbebas dari beban hitung.
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

    // Cegah pembagian dengan nol (Divide-by-Zero Protection)
    if (elapsed_ticks == 0) return;

    float elapsed_seconds = ((float)elapsed_ticks * portTICK_PERIOD_MS) / 1000.0f;

    // 1. Baca isi Counter dari Hardware Register secara langsung
    uint32_t current_cnt = sensor->htim->Instance->CNT;
    uint32_t local_pulse = 0;

    // 2. MENGAPA ADA CASTING (uint16_t) dan (uint32_t)? (OVERFLOW HANDLING)
    // TIM9 adalah timer 16-bit (max 65535). Jika last_cnt = 65000, lalu current_cnt meluap ke 500,
    // (500 - 65000) bernilai negatif secara matematis, TETAPI karena kita paksa di-cast
    // ke (uint16_t), sifat "Integer Underflow" C otomatis melipat gandakan nilainya menjadi
    // jarak yang persis benar (500 + (65536 - 65000) = 1036 pulsa).
    // Sangat ringan (O(1)) tanpa if-else bersarang.
    if (sensor->htim->Instance == TIM9) {
        local_pulse = (uint16_t)(current_cnt - sensor->last_cnt);
    } else {
        // TIM2 & TIM5 adalah timer 32-bit
        local_pulse = (uint32_t)(current_cnt - sensor->last_cnt);
    }

    sensor->last_cnt = current_cnt;

    // 3. Mapping Nilai Kalibrasi berdasarkan LOGICAL ID
    float total_pulses_per_liter = sensor->pulse1liter; // Fallback

    // SINKRONISASI ENUM DENGAN file .h
    switch (sensor->sensor_id) {
        case FM_TANK_IN:
            total_pulses_per_liter = (float)sys_calib.fm_inlet_pulse_per_liter;
            break;

        case FM_MAIN_OUTLET:
            total_pulses_per_liter = (float)sys_calib.fm_outlet_pulse_per_liter;
            break;

        case FM_FERT:
            total_pulses_per_liter = (float)sys_calib.fm_fert_pulse_per_liter;
            break;

        default:
            break;
    }

    // 4. Kalkulasi Volume dan Aliran
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
