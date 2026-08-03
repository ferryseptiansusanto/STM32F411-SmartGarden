/**
 * @file    app_task.h
 * @brief   Modul Task Utama Aplikasi (FSM Tersentralisasi V3.6)
 * @details Mengelola siklus penuh Smart Garden dari Booting, Standby, hingga Eksekusi Fluida.
 *
 *  Created on: 3 Jul 2026
 *      Author: ferry
 */

#ifndef APPS_APP_TASK_H_
#define APPS_APP_TASK_H_

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Ekspor Queue Utama FSM agar bisa disuplai oleh command_task dan modul interupsi */
extern QueueHandle_t appQueue;

/* ============================================================================
 * ENUMERATIONS (FSM STATES FINAL V3.6)
 * ==========================================================================*/

/**
 * @brief 15 State Tersentralisasi Final V3.6
 * @note  Mengatur alur secara linear dan sekuensial tanpa percabangan ganda
 * (Tidak ada lagi pemisahan IrrigationState dan FertState).
 */
typedef enum {
    /* --- FASE 1: BOOTING & INITIALIZATION --- */
    STATE_INIT_HARDWARE = 0,         /**< Fail-Safe: Force LOW semua relay, inisialisasi DMA/Timer */
    STATE_LOAD_CALIBRATION,          /**< Menarik sys_calib dari EEPROM via I2C */
    STATE_LOAD_SCHEDULE,             /**< Membaca jadwal_pupuk.txt / jadwal_air.txt dari SD Card */

    /* --- FASE 2: STANDBY & ROUTING (LOW POWER) --- */
    STATE_SET_NEXT_ALARM,            /**< Menulis jadwal terdekat ke RTC DS3231 */
    STATE_SLEEP,                     /**< MCU masuk STOP Mode, menunggu EXTI Wake-up */

    /* --- FASE 3: ACTION SEQUENCES (EKSEKUSI FLUIDA) --- */
    STATE_PRE_FLUSHING,              /**< Menguras sisa larutan lama dari tangki (opsional) */
    STATE_DOSING,                    /**< Mengisi pupuk 1-5 dan air secara bergiliran */
    STATE_MIXING,                    /**< Mengaduk larutan menggunakan timer non-blocking */
    STATE_FLUSHING,                  /**< Mendistribusikan racikan nutrisi ke kebun */
    STATE_IRRIGATING,                /**< JALUR A: Menyiram air baku langsung tanpa masuk tangki mixer */

    /* --- FASE 4: USER INTERVENTION & FAIL-SAFE --- */
    STATE_BT_INTERACTIVE,            /**< Mode siaga saat Bluetooth HP terhubung (mencegah sleep) */
    STATE_SENSOR_CALIBRATION,        /**< Menulis ulang batas atas/bawah sensor ke EEPROM */
    STATE_SYNC_CONFIG,               /**< Memperbarui jadwal dari HP ke SD Card */
    STATE_EVALUATE_MISSED_SCHEDULE,  /**< Mengevaluasi jadwal tertinggal & Routing Jalur Eksekusi */
    STATE_FAULT                      /**< EMERGENCY: Hard Stop semua aktuator menunggu Reset */
} AppFSMState_t;


/* ============================================================================
 * FUNCTION PROTOTYPES
 * ==========================================================================*/

/**
 * @brief  Membangun Task FSM Utama dan mengalokasikan Queue-nya.
 * @param  priority Prioritas eksekusi task di dalam scheduler FreeRTOS.
 * @note   Ukuran Queue dan Set diambil dari Apps/Config/app_config.h
 */
void APP_TaskCreate(UBaseType_t priority);

#endif /* APPS_APP_TASK_H_ */
