/*
 * @file    log_config.h
 * @brief   Konfigurasi untuk modul Log Manager.
 * @note    Terpusat di folder Config untuk mencegah hardcoding di logika utama.
 *
 *  Created on: 3 Aug 2026
 *      Author: ferry
 */

#ifndef APPS_CONFIG_LOG_CONFIG_H_
#define APPS_CONFIG_LOG_CONFIG_H_

/* --- NAMA FILE DI SD CARD --- */
#define LOG_SYSTEM_FILENAME "log_sistem.txt"

/* --- BATASAN MEMORI (FAIL-SAFE) --- */
/**
 * @brief Maksimal karakter dalam 1 baris log (termasuk timestamp & newline).
 * MENGAPA: Mencegah Buffer Overflow saat menggunakan sprintf/vsnprintf.
 */
#define MAX_LOG_LENGTH   128

#endif /* APPS_CONFIG_LOG_CONFIG_H_ */
