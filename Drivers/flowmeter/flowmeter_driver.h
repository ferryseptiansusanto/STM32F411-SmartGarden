/**
 * @file flowmeter_driver.h
 * @brief Driver Flowmeter menggunakan Zero-Interrupt Hardware Counter.
 * @note Bergantung pada FreeRTOS (xTaskGetTickCount) untuk delta waktu.
 * @author Ferry
 * @date 11 Jun 2026
 */
#ifndef FLOWMETER_FLOWMETER_DRIVER_H_
#define FLOWMETER_FLOWMETER_DRIVER_H_

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "flowmeter/flowmeter_type.h"

/**
 * @brief Struktur Data (Instance) utama untuk objek Flowmeter.
 */
typedef struct {
    FlowSensorID_t sensor_id;    ///< Alias Logis Sensor
    TIM_HandleTypeDef* htim;     ///< Pointer ke Handle Timer Hardware
    uint32_t tim_channel;        ///< Channel Timer

    uint32_t last_cnt;           ///< Rekaman Register Counter Hardware terakhir
    volatile uint32_t total_pulse;

    float pulse1liter;           ///< Fallback kalibrasi jika data EEPROM gagal
    float flowrate_hour;
    float flowrate_minute;
    float flowrate_second;
    float volume;

    TickType_t time_before;      ///< Stamp waktu FreeRTOS terakhir baca
} FlowSensor_t;

void FlowSensor_Init(FlowSensor_t *sensor, FlowSensorID_t id, uint16_t type, TIM_HandleTypeDef* htim, uint32_t channel);
void FlowSensor_Start(FlowSensor_t *sensor);
void FlowSensor_Stop(FlowSensor_t *sensor);
void FlowSensor_Read(FlowSensor_t *sensor);
void FlowSensor_SetType(FlowSensor_t *sensor, uint16_t type);

uint32_t FlowSensor_GetPulse(FlowSensor_t *sensor);
float FlowSensor_GetFlowRate_H(FlowSensor_t *sensor);
float FlowSensor_GetFlowRate_M(FlowSensor_t *sensor);
float FlowSensor_GetFlowRate_S(FlowSensor_t *sensor);
float FlowSensor_GetVolume(FlowSensor_t *sensor);

void FlowSensor_ResetPulse(FlowSensor_t *sensor);
void FlowSensor_ResetVolume(FlowSensor_t *sensor);

#endif /* FLOWMETER_FLOWMETER_DRIVER_H_ */
