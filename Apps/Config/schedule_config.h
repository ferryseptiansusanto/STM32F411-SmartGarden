/**
 * @file    schedule_config.h
 * @brief   Konfigurasi khusus Sub-Modul Schedule Manager.
 *
 * Created on: 3 Aug 2026
 * Author: ferry
 */

#ifndef MANAGERS_SCHEDULE_CONFIG_H_
#define MANAGERS_SCHEDULE_CONFIG_H_

/* ========================================================================== */
/* KONFIGURASI FILE SD CARD & BUFFER                                          */
/* ========================================================================== */

/**
 * @brief Nama file jadwal fertigasi/pemupukan di root SD Card.
 */
#define SCHED_CFG_FILE_FERTILIZER       "jadwal_pupuk.txt"

/**
 * @brief Nama file jadwal irigasi/murni air di root SD Card.
 */
#define SCHED_CFG_FILE_IRRIGATION       "jadwal_air.txt"

/* File temporary ini mutlak dibutuhkan untuk metode "Temp-Swap" (Fail-Safe)
 * saat FSM memperbarui status jadwal agar data tidak rusak saat mati listrik. */
#define SCHED_CFG_FILE_TEMP_SWAP    	"temp_sched.txt"

/**
 * @brief Panjang maksimal 1 baris teks jadwal di SD Card.
 */
#define SCHED_CFG_MAX_LINE_LEN          128

#endif /* MANAGERS_SCHEDULE_CONFIG_H_ */
