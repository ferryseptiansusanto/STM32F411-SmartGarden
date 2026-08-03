/**
 * @file    schedule_config.h
 * @brief   Konfigurasi khusus Sub-Modul Schedule Manager.
 *
 * Created on: 3 Aug 2026
 * Author: ferry
 */

#ifndef MANAGERS_SCHEDULE_CONFIG_H_
#define MANAGERS_SCHEDULE_CONFIG_H_

#include "board_config.h"

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

/**
 * @brief Panjang maksimal 1 baris teks jadwal di SD Card.
 */
#define SCHED_CFG_MAX_LINE_LEN          160

/**
 * @brief Identitas unik pemilik Mutex FileContext_t untuk Schedule Manager.
 */
#define SCHED_CFG_OWNER_ID              2

#endif /* MANAGERS_SCHEDULE_CONFIG_H_ */
