/**
 * @file    schedule_manager.h
 * @brief   Header file untuk manajemen jadwal eksekusi pemupukan berbasis RTC dan SD Card.
 *
 *  Created on: 22 Jul 2026
 *      Author: ferry
 */
#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "recipe_manager.h"
#include "ds3231_wrapper.h" // Sesuai file ds3231 Anda

#define MAX_SCHEDULES 20

typedef enum {
    STATUS_PENDING = 0,
    STATUS_SUCCESS,
    STATUS_SKIPPED
} ScheduleStatus_t;

// --- STRUKTUR JADWAL PUPUK ---
typedef struct {
    uint8_t  tipe_pupuk;   // 1 s/d 5
    uint16_t volume_ml;    // misal: 100
} Nutrisi_t;

typedef struct {
    uint32_t timestamp;        // Waktu eksekusi (dikonversi ke Unix Epoch Seconds)
    Nutrisi_t pupuk[5];        // Array untuk maksimal 5 jenis pupuk
    uint8_t  jumlah_pupuk;     // Berapa jenis pupuk yang aktif di resep ini
    uint16_t water_volume_ml;  // Volume air baku
    ScheduleStatus_t status;   // PENDING / SUCCESS / SKIPPED
} JadwalPupuk_t;

// --- STRUKTUR JADWAL PENGAIRAN ---
typedef struct {
    uint32_t timestamp;        // Waktu eksekusi (Unix Epoch Seconds)
    uint16_t repeat_days;      // 0 = none, 1 = daily, >1 = custom days
    uint16_t water_volume_ml;  // Volume air baku
    ScheduleStatus_t status;   // PENDING / SUCCESS / SKIPPED
} JadwalPengairan_t;

typedef struct {
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t hour;
    uint8_t minute;
    char recipe_name[MAX_RECIPE_NAME];
    SchStatus_t status;
    uint32_t file_pos; /**< Pointer penunjuk posisi byte di file SD Card untuk update status cepat */
} Schedule_t;

/**
 * @brief   Memuat semua daftar jadwal dari SD Card ke memori internal RAM.
 * @retval  bool true jika berhasil memuat file jadwal.
 */
bool Schedule_Init(void);

/**
 * @brief   Memeriksa apakah ada jadwal aktif yang cocok dengan waktu RTC saat ini.
 * @param   current_time Struktur waktu dari driver DS3231.
 * @param   out_recipe_name Pointer menampung resep yang harus dijalankan.
 * @param   out_sch_index Pointer untuk merekam index jadwal yang aktif di RAM.
 * @retval  bool true jika ada jadwal yang valid dan siap dieksekusi sekarang.
 */
bool Schedule_CheckTrigger(DS3231_DateTime current_time, char* out_recipe_name, int* out_sch_index);

/**
 * @brief   Memperbarui status jadwal dari 'onschedule' menjadi 'finish' di SD Card dan RAM.
 * @param   sch_index Index jadwal di dalam array RAM.
 * @retval  bool true jika penulisan ulang ke SD Card sukses.
 */
bool Schedule_MarkAsFinish(int sch_index);

#endif /* SCHEDULE_MANAGER_H */
