/**
 * @file failsafe_manager.c
 * @brief Implementasi logika Cross-Validation dan Error Handling Smart Garden.
 * Menggunakan prinsip O(1) dan murni Non-Blocking.
 *
 *
 *  Created on: 6 Aug 2026
 *      Author: ferry
 */
#include "main.h"
#include "failsafe_manager.h"
#include "log_manager.h"         // Untuk mencetak Error Log ke SD Card
#include "../Drivers/Actuator/actuator_driver.h"     // Untuk mengunci/mematikan semua pompa dan valve

void FailSafeManager_Init(void) {
    // Saat inisialisasi, pastikan tidak ada status error aktif yang menggantung
}

FailSafeError_t FailSafeManager_CheckFlow(uint32_t expected_flow, uint32_t actual_flow, uint32_t elapsed_time_ms) {
    /* * Mengapa kita memeriksa ini? (Why)
     * Untuk mencegah dry-run pada pompa jika pipa pecah atau tandon kosong
     * namun pelampung belum mendeteksi.
     */
    if (elapsed_time_ms > CONFIG_FLOW_STALL_TIMEOUT_MS) {
        if (actual_flow == 0 || actual_flow < (expected_flow * 0.05)) { // Toleransi 5% pergerakan
            return FAILSAFE_ERR_FLOW_STALL;
        }
    }
    return FAILSAFE_OK;
}

FailSafeError_t FailSafeManager_CheckTimeSanity(uint32_t current_epoch, uint32_t target_epoch) {
    /* * Mengapa kita memeriksa ini? (Why)
     * Baterai RTC bisa habis atau kabel SCL/SDA putus, mengembalikan nilai 0 atau tahun 1970.
     * Eksekusi pupuk di waktu yang salah bisa meracuni tanaman.
     */
    if (current_epoch == 0 || current_epoch < 1672531200) { // 1672531200 = 1 Jan 2023 (batas bawah kewarasan)
        return FAILSAFE_ERR_EPOCH_ANOMALY;
    }

    // Cek apakah sistem terlambat terlalu jauh dari target
    if (current_epoch > target_epoch) {
        uint32_t delay_diff = current_epoch - target_epoch;
        if (delay_diff > CONFIG_MAX_EPOCH_DELAY_TOLERANCE) {
            return FAILSAFE_ERR_EPOCH_ANOMALY;
        }
    }

    return FAILSAFE_OK;
}

void FailSafeManager_ExecuteLockdown(FailSafeError_t error_code) {
    /*
     * HUKUM TERAKHIR: HARDWARE LOCKDOWN DAN PENCATATAN LOG
     * Dilarang pindah ke STATE_FAULT secara diam-diam.
     */

    // 1. Matikan seluruh Aktuator dengan kompleksitas O(1) loop / mapping
    Actuator_SetState(ACT_PUMP_OUT, ACT_OFF);
    Actuator_SetState(ACT_PUMP_FERT, ACT_OFF);
    Actuator_SetState(ACT_MIXER, ACT_OFF);
    Actuator_SetState(ACT_VALVE_TANK_IN, ACT_OFF);
    Actuator_SetState(ACT_VALVE_TANK_OUT, ACT_OFF);
    Actuator_SetState(ACT_VALVE_WATER_IN, ACT_OFF);
    Actuator_SetState(ACT_VALVE_FERT_1, ACT_OFF);
    Actuator_SetState(ACT_VALVE_FERT_2, ACT_OFF);
    Actuator_SetState(ACT_VALVE_FERT_3, ACT_OFF);
    Actuator_SetState(ACT_VALVE_FERT_4, ACT_OFF);
    Actuator_SetState(ACT_VALVE_FERT_5, ACT_OFF);

    // 2. Tentukan Pesan Log berdasarkan Error Code
    const char* error_msg = "UNKNOWN_ERROR";
    switch(error_code) {
        case FAILSAFE_ERR_FLOW_STALL:       error_msg = "FAULT: FLOW STALL DETECTED"; break;
        case FAILSAFE_ERR_TANK_EMPTY_LUBER: error_msg = "FAULT: TANK LEVEL ANOMALY"; break;
        case FAILSAFE_ERR_I2C_DISCONNECT:   error_msg = "FAULT: I2C BUS DISCONNECTED"; break;
        case FAILSAFE_ERR_SENSOR_ANOMALY:   error_msg = "FAULT: MIXING DELTA ANOMALY"; break;
        case FAILSAFE_ERR_EPOCH_ANOMALY:    error_msg = "FAULT: RTC TIME CORRUPTED"; break;
        default: break;
    }

    // 3. Catat ke SD Card via Log Manager (Tidak memblokir CPU karena via FreeRTOS Queue)
    LogManager_Write(LOG_ERROR, error_msg);

    // Sistem sekarang aman untuk masuk ke STATE_FAULT di FSM (app_task.c)
}
