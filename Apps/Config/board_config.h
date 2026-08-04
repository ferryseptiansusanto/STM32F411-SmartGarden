/**
 * @file    board_config.h
 * @brief   Definisi absolut topologi perangkat keras fisik (Peta Hardware).
 * @note    HANYA diubah jika ada revisi pada PCB (contoh: ganti IC EEPROM/RTC).
 * File ini menjembatani Layer Logic dengan Spesifikasi Komponen.
 *
 *  Created on: 3 Aug 2026
 *      Author: ferry
 */


#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* Mengawinkan Pinout otomatis CubeMX dengan konfigurasi lanjutan kita */
#include "main.h"

/* --- VERSI HARDWARE --- */
#define HARDWARE_REVISION "V1.0"

/* ==============================================================================
 * 1. KONFIGURASI EEPROM (Default: AT24C32)
 * ============================================================================== */
#define EEPROM_I2C_ADDR        0xAE     // Alamat fisik IC di bus I2C DS3231
#define EEPROM_PAGE_SIZE       32       // Batas tulis per halaman (bytes)
#define EEPROM_TOTAL_SIZE      4096     // Total kapasitas (bytes) -> 4KB
#define EEPROM_CONFIG_ADDRESS  0x0000   // Alamat blok awal untuk menyimpan struct sys_calib

/* ==============================================================================
 * 2. KONFIGURASI RTC (Default: DS3231)
 * ============================================================================== */
#define RTC_I2C_ADDR           0xD0     // Alamat fisik IC RTC di bus I2C kenapa bukan 0x68 karena 0x68<<1 ~ 0xD0

/* ==============================================================================
 * 3. [OPSIONAL] KONFIGURASI PENGGANTI DI MASA DEPAN (Contoh)
 * ============================================================================== */
// Jika tahun depan ganti ke AT24C64, Engineer cukup mengomentari blok atas,
// lalu membuka komentar blok di bawah ini tanpa perlu membongkar logika utama.
/*
#define EEPROM_I2C_ADDR        0xA0
#define EEPROM_PAGE_SIZE       32
#define EEPROM_TOTAL_SIZE      8192     // Kapasitas 8KB
*/

/* ========================================================================== */
/* 4. AKTUATOR & VALVE HARDWARE BOUNDARIES                     */
/* ========================================================================== */

/**
 * @brief Jumlah fisik valve/pompa pupuk terpasang pada papan sirkuit (VALVE_FERT1..5).
 */
#define BOARD_NUM_FERTILIZER_VALVES     5

#endif /* BOARD_CONFIG_H */
