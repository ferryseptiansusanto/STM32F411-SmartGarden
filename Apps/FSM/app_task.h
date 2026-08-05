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
    /* --- FASE 1: BOOTING SEQUENCE --- */
    STATE_INIT_HARDWARE = 0,         /**< Fail-Safe: Force LOW semua relay, inisialisasi DMA/Timer */
    STATE_LOAD_CALIBRATION = 1,      /**< Menarik sys_config dari EEPROM via I2C */
    STATE_LOAD_SCHEDULE = 2,         /**< Membaca jadwal_pupuk.txt / jadwal_air.txt dari SD Card */

    /* --- FASE 2: STANDBY & ROUTING (LOW POWER) --- */
    STATE_SET_NEXT_ALARM = 3,        /**< Menulis jadwal terdekat ke RTC DS3231 */
    STATE_SLEEP = 4,                 /**< MCU masuk STOP Mode, menunggu EXTI Wake-up */
    STATE_WAKE_UP = 5,               /**< Pemulihan clock sistem & pembersihan flag interupsi */
    STATE_EVALUATE_MISSED_SCHEDULE = 6, /**< Mengevaluasi jadwal tertinggal & Routing Jalur Eksekusi */

    /* --- FASE 3: ACTION SEQUENCES (EKSEKUSI FLUIDA) --- */
    STATE_IRRIGATING = 7,            /**< JALUR A: Menyiram air baku langsung tanpa masuk tangki mixer */
    STATE_PRE_FLUSHING = 8,          /**< Menguras sisa larutan lama dari tangki mixer */
    STATE_DOSING = 9,                /**< Mengisi pupuk 1-5 dan air secara bergiliran (Watchdog Active) */
    STATE_MIXING = 10,               /**< Mengaduk larutan dan Sanity Check EC untuk pengenceran */
    STATE_FLUSHING = 11,             /**< Mendistribusikan racikan nutrisi ke kebun */

    /* --- FASE 4: USER INTERVENTION & FAIL-SAFE --- */
    STATE_BT_INTERACTIVE = 12,       /**< Mode siaga saat Bluetooth HP terhubung (mencegah sleep) */
    STATE_SYNC_CONFIG_CALIB = 13,    /**< Menyimpan kalibrasi sensor ke EEPROM ATAU jadwal ke SD Card */
    STATE_FAULT = 14                 /**< EMERGENCY: Hard Stop semua aktuator, menunggu Reset */
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
