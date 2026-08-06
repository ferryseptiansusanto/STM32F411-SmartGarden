/*
 * app_config.h
 * @brief   Pusat Konfigurasi Parameter Logika FSM (Tidak ada pinout di sini)
 *
 *
 *  Created on: 3 Aug 2026
 *      Author: ferry
 */

#ifndef CONFIG_APP_CONFIG_H_
#define CONFIG_APP_CONFIG_H_

#define APP_QUEUE_SIZE           10
#define APP_QUEUE_SET_SIZE       15

#define FLOW_STALL_TIMEOUT_MS       5000  // Toleransi 5 detik tanpa aliran air
#define DYNAMIC_WAKEUP_MS           1000  // FSM bangun tiap 1 detik saat pompa aktif

#define MIXING_DURATION_MS       60000 // 1 Menit pengadukan default

#endif /* CONFIG_APP_CONFIG_H_ */
