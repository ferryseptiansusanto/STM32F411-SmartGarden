/**
 * @file    log_manager.c
 * @brief   Implementasi logika perakitan Log Sistem.
 *
 * Created on: 3 Aug 2026
 * Author: ferry
 */

#include "log_manager.h"
#include "ds3231_wrapper.h"   /* Untuk menarik timestamp FSM */
#include "fatfs_wrapper.h"    /* Untuk mengirim string matang ke SD Card via Mutex */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* File target baku sesuai spesifikasi Dokumen 6 */
static const char* LOG_FILENAME = "log_sistem.txt";

/* Konteks Mutlak: Setiap Manager punya "Identitas" antrean untuk akses SD Card */
static FileContext_t log_file_ctx;

/* Maksimal karakter dalam 1 baris log untuk menjaga efisiensi ukuran Stack RTOS */
#define MAX_LOG_LENGTH 128

bool LogManager_Init(void) {
    /* MENGAPA CONTEXT DIBUTUHKAN:
       Mendaftarkan identitas modul ini ke fatfs_wrapper agar saat Log Manager
       berusaha menulis di detik yang sama dengan Schedule Manager membaca jadwal,
       Wrapper bisa mengantrekannya dengan aman via RTOS Mutex. */
    log_file_ctx.owner_id = 1; // ID 1 Khusus untuk Log Manager
    return true;
}

static const char* LogManager_LevelToStr(LogLevel_t level) {
    switch(level) {
        case LOG_INFO:     return "INFO";
        case LOG_WARN:     return "WARN";
        case LOG_ERROR:    return "ERROR";
        case LOG_CRITICAL: return "CRIT";
        default:           return "UNKN";
    }
}

bool LogManager_Write(LogLevel_t level, const char* format, ...) {
    char log_buffer[MAX_LOG_LENGTH];
    char message_buffer[80];
    DS3231_DateTime now;

    /* 1. MENGAMBIL WAKTU TERKINI (ZERO-BLOCKING via ds3231_wrapper) */
    if (!DS3231_GetTime(&now)) {
        /* Jika RTC gagal dibaca (I2C error), gunakan timestamp darurat */
        memset(&now, 0, sizeof(now));
    }

    /* 2. MENGOLAH FORMAT PESAN DINAMIS (Variadic Arguments)
       MENGAPA vsnprintf: Kita WAJIB membatasi jumlah karakter maksimal yang dimasukkan
       ke message_buffer untuk mencegah serangan Buffer Overflow atau Stack Corruption
       yang dapat memicu HardFault pada ARM Cortex-M4. */
    va_list args;
    va_start(args, format);
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    va_end(args);

    /* 3. MERAKIT STRING LOG FINAL
       Format: [YYYY-MM-DD HH:MM:SS] [LEVEL] Pesan
       Contoh: [2026-08-03 10:45:00] [CRIT] Tangki Kering, Pompa Utama Dimatikan! */
    snprintf(log_buffer, sizeof(log_buffer), "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
             now.date.year, now.date.month, now.date.day,
             now.time.hours, now.time.minutes, now.time.seconds,
             LogManager_LevelToStr(level),
             message_buffer);

    /* 4. MELEMPAR STRING MATANG KE LAPISAN WRAPPER
       Tugas Log Manager selesai di sini. FatFs Wrapper akan mengambil alih gembok Mutex
       dan melakukan operasi f_open, f_write, dan f_close secara aman di latar belakang. */
    return FatFsWrapper_AppendText(&log_file_ctx, LOG_FILENAME, log_buffer);
}
