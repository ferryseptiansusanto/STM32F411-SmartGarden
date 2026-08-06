/**
 * @file failsafe_config.h
 * @brief Pusat konfigurasi batas toleransi dan timeout untuk sistem Fail-Safe.
 * DILARANG menaruh angka hardcode di dalam failsafe_manager.c!
 *
 *  Created on: 6 Aug 2026
 *      Author: ferry
 */

#ifndef FAILSAFE_CONFIG_H
#define FAILSAFE_CONFIG_H

/* --- Batasan Toleransi Waktu (Timeouts) --- */
/** Maksimal waktu (ms) pompa menyala tanpa ada perubahan volume dari Flowmeter (Indikasi dry-run) */
#define CONFIG_FLOW_STALL_TIMEOUT_MS        5000

/** Maksimal toleransi deviasi waktu RTC (detik) dari jadwal yang seharusnya */
#define CONFIG_MAX_EPOCH_DELAY_TOLERANCE    300  // 5 Menit

/* --- Batasan Sensor --- */
/** Selisih minimal (Delta) sensor kualitas air setelah mixing untuk dianggap valid */
#define CONFIG_MIN_MIXING_DELTA_PH          0.2f
#define CONFIG_MIN_MIXING_DELTA_TDS         10.0f

#endif /* FAILSAFE_CONFIG_H */
