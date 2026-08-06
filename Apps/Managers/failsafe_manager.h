/*
 * @file failsafe_manager.h
 * @brief Prototipe dan definisi struktur data untuk Fail-Safe Manager.
 *
 *  Created on: 6 Aug 2026
 *      Author: ferry
 */

#ifndef FAILSAFE_MANAGER_H
#define FAILSAFE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "../Config/failsafe_config.h"

/**
 * @brief Enumerasi kode error (Fault Codes) sesuai dengan Matrix Fail-Safe (Dokumen 12)
 */
typedef enum {
    FAILSAFE_OK = 0,
    FAILSAFE_ERR_FLOW_STALL,        // Hukum 1: Pompa nyala tapi air tidak mengalir
    FAILSAFE_ERR_TANK_EMPTY_LUBER,  // Hukum 2: Air habis tak terduga atau meluber
    FAILSAFE_ERR_I2C_DISCONNECT,    // Hukum 3: Putus komunikasi I2C (EEPROM/RTC)
    FAILSAFE_ERR_SENSOR_ANOMALY,    // Hukum 4: Nilai sensor tidak masuk akal (Delta 0)
    FAILSAFE_ERR_EPOCH_ANOMALY,     // Hukum 5: Waktu mundur atau selisih ekstrem
    FAILSAFE_ERR_SYSTEM_PANIC       // Kegagalan tak terdefinisi (HardFault terdeteksi)
} FailSafeError_t;

/**
 * @brief Inisialisasi awal manajer Fail-Safe
 */
void FailSafeManager_Init(void);

/**
 * @brief Memeriksa apakah terjadi penyumbatan atau pompa kering (Flow Stall)
 * @param expected_flow Target volume (liter)
 * @param actual_flow Volume saat ini yang terbaca (liter)
 * @param elapsed_time_ms Waktu berlalu sejak pompa menyala
 * @return FAILSAFE_OK jika aman, FAILSAFE_ERR_FLOW_STALL jika macet
 */
FailSafeError_t FailSafeManager_CheckFlow(uint32_t expected_flow, uint32_t actual_flow, uint32_t elapsed_time_ms);

/**
 * @brief Memeriksa kewarasan waktu RTC (Epoch Sanity Check)
 * @param current_epoch Waktu sekarang dari DS3231
 * @param target_epoch Waktu target jadwal
 * @return FAILSAFE_OK jika rasional, FAILSAFE_ERR_EPOCH_ANOMALY jika aneh
 */
FailSafeError_t FailSafeManager_CheckTimeSanity(uint32_t current_epoch, uint32_t target_epoch);

/**
 * @brief Mengeksekusi Lockdown Sistem (Aturan Emas Pencatatan)
 * Mematikan semua aktuator dan mencatat log sebelum FSM masuk ke STATE_FAULT
 * @param error_code Kode error pemicu lockdown
 */
void FailSafeManager_ExecuteLockdown(FailSafeError_t error_code);

#endif /* FAILSAFE_MANAGER_H */
