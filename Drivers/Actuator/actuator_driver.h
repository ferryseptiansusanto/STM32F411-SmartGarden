/**
 * @file actuator_driver.h
 * @brief Lapisan Driver (Lapisan 1) untuk pengontrolan aktuator Smart Garden.
 * Mengelola Pompa, Valve, dan Mixer menggunakan pemetaan memori O(1).
 * @author Ferry
 * @date 22 Jul 2026
 */

#ifndef ACTUATOR_ACTUATOR_DRIVER_H_
#define ACTUATOR_ACTUATOR_DRIVER_H_

#include "main.h"

/**
 * @brief Daftar (Enumerasi) seluruh perangkat aktuator pada sistem.
 * @note Urutan ini WAJIB sama dengan urutan array actuatorMap di file .c
 */
typedef enum {
    ACT_VALVE_WATER_IN = 0,
    ACT_VALVE_TANK_IN,
    ACT_VALVE_TANK_OUT,
    ACT_VALVE_FERT_1,
    ACT_VALVE_FERT_2,
    ACT_VALVE_FERT_3,
    ACT_VALVE_FERT_4,
    ACT_VALVE_FERT_5,
    ACT_PUMP_OUT,
    ACT_PUMP_FERT,
    ACT_MIXER,
    ACT_MAX // Sentinel Value: Digunakan untuk batas aman / validasi ukuran array
} ActuatorType_t;

/**
 * @brief Status dari sebuah aktuator.
 */
typedef enum {
    ACT_OFF = 0,
    ACT_ON  = 1
} ActuatorState_t;

/**
 * @brief Menginisialisasi seluruh aktuator ke kondisi mati (Fail-Safe).
 * Dipanggil saat awal booting sistem pada STATE_INIT_HARDWARE.
 */
void Actuator_Init(void);

/**
 * @brief Mengubah status spesifik aktuator secara individual (Zero-Blocking).
 * * @param actuator ID dari perangkat aktuator (contoh: ACT_PUMP_OUT)
 * @param state    Status yang diinginkan (ACT_ON / ACT_OFF)
 */
void Actuator_SetState(ActuatorType_t actuator, ActuatorState_t state);

#endif /* ACTUATOR_ACTUATOR_DRIVER_H_ */
