/**
 * @file    recipe_config.h
 * @brief   File konfigurasi khusus Sub-Modul Recipe Manager (Agronomy & Parsing Limits).
 * @note    Modul ini mengisolasi semua batasan parameter resep.
 *
 * Created on: 3 Aug 2026
 * Author: ferry
 */

#ifndef MANAGERS_RECIPE_CONFIG_H_
#define MANAGERS_RECIPE_CONFIG_H_

#include "board_config.h"

/* ========================================================================== */
/* BATASAN MEMORI & BUFFER PARSING                        */
/* ========================================================================== */

/**
 * @brief Ukuran maksimal nama resep dalam karakter (termasuk null-terminator).
 */
#define RECIPE_CFG_MAX_NAME_LEN         16

/**
 * @brief Ukuran buffer lokal temporer untuk mengiris tag pupuk.
 */
#define RECIPE_CFG_FERT_BUF_SIZE        80

/* ========================================================================== */
/* BATASAN KEAMANAN AGRONOMI & HARDWARE (FAIL-SAFE)            */
/* ========================================================================== */

/**
 * @brief Volume maksimal satu jenis pupuk per penyiraman (ml).
 * @note  Fail-Safe: Mencegah over-fertilization jika terdapat salah ketik pada jadwal.
 */
#define RECIPE_CFG_MAX_FERT_VOL_ML      2000

/**
 * @brief Volume maksimal air baku per penyiraman (ml).
 * @note  Fail-Safe: Mencegah tangki pencampur luber (overflow).
 */
#define RECIPE_CFG_MAX_WATER_VOL_ML     50000

/**
 * @brief Waktu maksimal motor pengaduk/mixer boleh aktif (detik).
 * @note  Fail-Safe: Mencegah motor mixer terbakar (overheating).
 */
#define RECIPE_CFG_MAX_MIXING_TIME_SEC  600

#endif /* MANAGERS_RECIPE_CONFIG_H_ */
