/**
 * @file    schedule_manager.c
 * @brief   Implementasi logika evaluasi jadwal, konversi waktu Epoch, dan Auto-Reschedule.
 *
 * Created on: 3 Aug 2026
 * Author: ferry
 */

#include "../Config/schedule_config.h"
#include "schedule_manager.h"
#include "fatfs_wrapper.h"
#include "log_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Konteks Mutex terpisah untuk Schedule Manager (Aturan 6 & SoC) */
static FileContext_t sched_file_ctx;

bool ScheduleManager_Init(void) {
    return true;
}

static const char* GetFileNameByType(SchedType_t type) {
    return (type == SCHED_TYPE_FERTILIZER) ? SCHED_CFG_FILE_FERTILIZER : SCHED_CFG_FILE_IRRIGATION;
}

static const char* StatusToStr(SchedStatus_t status) {
    switch (status) {
        case SCHED_STATUS_PENDING: return "pending";
        case SCHED_STATUS_SUCCESS: return "success";
        case SCHED_STATUS_SKIPPED: return "skipped";
        case SCHED_STATUS_URGENT:  return "urgent";
        default:                   return "invalid";
    }
}

static SchedStatus_t ParseStatusFromLine(const char* line) {
    if (strstr(line, "status[pending]")) return SCHED_STATUS_PENDING;
    if (strstr(line, "status[success]")) return SCHED_STATUS_SUCCESS;
    if (strstr(line, "status[skipped]")) return SCHED_STATUS_SKIPPED;
    if (strstr(line, "status[urgent]"))  return SCHED_STATUS_URGENT;
    return SCHED_STATUS_INVALID;
}

uint32_t Schedule_DateTimeToEpoch(const char* time_str) {
    if (time_str == NULL) return 0;

    int yr = 0, mon = 0, day = 0, hr = 0, min = 0, sec = 0;
    if (sscanf(time_str, "%d-%d-%d %d:%d:%d", &yr, &mon, &day, &hr, &min, &sec) != 6) {
        return 0;
    }

    if (yr < 1970 || mon < 1 || mon > 12 || day < 1 || day > 31) return 0;

    static const uint16_t days_before_month[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint32_t y = (uint32_t)(yr - 1970);
    uint32_t leap_years = (y + 1) / 4; // Koreksi tahun kabisat (1970-2099)
    uint32_t days = y * 365 + leap_years + days_before_month[mon - 1] + (uint32_t)(day - 1);

    if ((yr % 4 == 0) && mon > 2) {
        days += 1;
    }

    return (days * 86400) + ((uint32_t)hr * 3600) + ((uint32_t)min * 60) + (uint32_t)sec;
}

void Schedule_EpochToDateTimeStr(uint32_t epoch, char* out_buf, size_t max_len) {
    if (out_buf == NULL || max_len < 20) return;

    uint32_t sec  = epoch % 60; epoch /= 60;
    uint32_t min  = epoch % 60; epoch /= 60;
    uint32_t hour = epoch % 24; epoch /= 24;

    uint32_t days = epoch;
    uint32_t year = 1970;

    while (1) {
        bool is_leap = (year % 4 == 0);
        uint32_t days_in_year = is_leap ? 366 : 365;
        if (days < days_in_year) break;
        days -= days_in_year;
        year++;
    }

    bool is_leap = (year % 4 == 0);
    static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t month = 1;

    for (uint8_t i = 0; i < 12; i++) {
        uint8_t dim = days_in_month[i];
        if (i == 1 && is_leap) dim = 29;
        if (days < dim) {
            month = i + 1;
            break;
        }
        days -= dim;
    }
    uint8_t day = (uint8_t)days + 1;

    snprintf(out_buf, max_len, "%04lu-%02u-%02u %02lu:%02lu:%02lu",
             (unsigned long)year, month, day, (unsigned long)hour, (unsigned long)min, (unsigned long)sec);
}

bool ScheduleManager_GetDueSchedule(SchedType_t type, ScheduleItem_t* out_item,
                                   uint32_t current_epoch_time, uint32_t max_delay_tolerance) {
    if (out_item == NULL) return false;

    const char* filename = GetFileNameByType(type);
    char line_buf[SCHED_CFG_MAX_LINE_LEN];
    uint32_t current_line = 0;

    /* Lock & Open File via Wrapper */
    if (!FATFS_Open(&sched_file_ctx, filename, FA_READ)) {
        return false;
    }

    bool due_found = false;

    while (FATFS_ReadLine(&sched_file_ctx, line_buf, sizeof(line_buf))) {
        current_line++;
        SchedStatus_t line_status = ParseStatusFromLine(line_buf);

        /* Evaluasi jadwal yang masih PENDING atau tertahan URGENT */
        if (line_status == SCHED_STATUS_PENDING || line_status == SCHED_STATUS_URGENT) {
            uint32_t sched_epoch = Schedule_DateTimeToEpoch(line_buf);

            if (sched_epoch > 0 && current_epoch_time >= sched_epoch) {
                uint32_t delay_sec = current_epoch_time - sched_epoch;

                memset(out_item, 0, sizeof(ScheduleItem_t));
                out_item->epoch_time  = sched_epoch;
                out_item->type        = type;
                out_item->line_number = current_line;

                /* Parse tag repeat[N] jika ada */
                const char* rep_tag = strstr(line_buf, "repeat[");
                if (rep_tag != NULL) {
                    int rep_val = 0;
                    if (sscanf(rep_tag, "repeat[%d]", &rep_val) == 1 && rep_val > 0) {
                        out_item->repeat_days = (uint16_t)rep_val;
                    }
                }

                /* Terapkan Aturan Toleransi Keterlambatan (Dokumen 10) */
                if (line_status == SCHED_STATUS_PENDING && delay_sec > max_delay_tolerance) {
                    out_item->status = SCHED_STATUS_URGENT;
                    LogManager_Write(LOG_WARN, "Jadwal baris %lu terlambat %lus (> tolerance). Ubah ke URGENT.",
                                     current_line, delay_sec);

                    /* Tutup file dan lakukan update status ke URGENT di SD Card */
                    FATFS_Close(&sched_file_ctx);
                    ScheduleManager_UpdateStatus(type, current_line, SCHED_STATUS_URGENT);
                    due_found = true;
                    return true;
                }

                out_item->status = line_status;

                /* Parsing Resep jika tipe Fertigasi */
                if (type == SCHED_TYPE_FERTILIZER) {
                    if (!Recipe_Parse(line_buf, &out_item->recipe)) {
                        LogManager_Write(LOG_ERROR, "Resep corrupt pada baris %lu", current_line);
                        continue; // Abaikan baris corrupt
                    }
                } else {
                    /* Irigasi air murni: Parse tag water[W] */
                    const char* w_tag = strstr(line_buf, "water[");
                    if (w_tag != NULL) {
                        int w_val = 0;
                        if (sscanf(w_tag, "water[%d]", &w_val) == 1) {
                            out_item->recipe.water_volume = (uint16_t)w_val;
                            strncpy(out_item->recipe.name, "IrigasiAir", sizeof(out_item->recipe.name) - 1);
                        }
                    }
                }

                due_found = true;
                break;
            }
        }
    }

    FATFS_Close(&sched_file_ctx);
    return due_found;
}

bool ScheduleManager_UpdateStatus(SchedType_t type, uint32_t line_number, SchedStatus_t new_status) {
	if (line_number == 0) return false;

	    const char* src_file  = GetFileNameByType(type);
	    const char* temp_file = SCHED_CFG_FILE_TEMP_SWAP;

	    // KITA TIDAK LAGI MEMBUKA FILE SECARA MANUAL DI SINI.
	    // Karena kita sudah punya fungsi sakti 'FATFS_ReplaceWordInLine'
	    // di dalam wrapper yang akan mengurus SEMUA proses Buka-Tulis-Tutup-Rename
	    // secara atomik di dalam pelukan Mutex!

	    // Tapi tunggu, ReplaceWordInLine butuh 'line_identifier'.
	    // Sayangnya, di jadwal kita, tidak ada identifier unik selain nomor baris (line_number).
	    // Oleh karena itu, kita harus menulis ulang logika Temp-Swap KHUSUS UNTUK NOMOR BARIS
	    // menggunakan API dasar dari wrapper.

	    char line_buf[SCHED_CFG_MAX_LINE_LEN];
	    uint32_t current_line = 0;

	    // 1. Siapkan Context (Cukup inisialisasi dengan 0)
	    FileContext_t src_ctx = {0};
	    FileContext_t temp_ctx = {0};

	    // 2. Buka File Asli (Read) dan File Temp (Write)
	    // Ingat, gunakan prefix 'FATFS_' bukan 'FatFsWrapper_'
	    // Sayangnya, di fatfs_wrapper.h baru Anda, fungsi Open/Close dasar tidak di-expose.
	    // MARI KITA GUNAKAN PENDEKATAN YANG LEBIH AMAN:

	    // Karena kita menggunakan File System, kita memodifikasi file baris-demi-baris.
	    // Kita harus memastikan wrapper.h Anda memiliki fungsi FATFS_Open dan FATFS_Close.

	    // Asumsi: Kita asumsikan Anda telah mengekspos FATFS_Open dan FATFS_Close di wrapper.h
	    // (Jika belum, silakan tambahkan ke fatfs_wrapper.h/c sesuai kode di bawah)

	    if (FATFS_Open(&src_ctx, src_file, FA_READ) != FS_OK) {
	        return false;
	    }

	    if (FATFS_Open(&temp_ctx, temp_file, FA_CREATE_ALWAYS | FA_WRITE) != FS_OK) {
	        FATFS_Close(&src_ctx);
	        return false;
	    }

	    // 3. Looping baca baris demi baris
	    while (FATFS_ReadLine(&src_ctx, line_buf, sizeof(line_buf)) == FS_OK) {
	        current_line++;

	        // Jika ini adalah baris yang mau diupdate
	        if (current_line == line_number) {
	            char* status_ptr = strstr(line_buf, "status[");
	            if (status_ptr != NULL) {
	                char prefix_buf[SCHED_CFG_MAX_LINE_LEN];
	                size_t prefix_len = (size_t)(status_ptr - line_buf);
	                strncpy(prefix_buf, line_buf, prefix_len);
	                prefix_buf[prefix_len] = '\0';

	                snprintf(line_buf, sizeof(line_buf), "%sstatus[%s]\n", prefix_buf, StatusToStr(new_status));
	            }
	        }

	        // Tulis baris ke temp_file menggunakan fungsi Write mentah (tanpa otomatis close)
	        // Pastikan Anda punya fungsi FATFS_WriteRaw di wrapper Anda.
	        FATFS_WriteRaw(&temp_ctx, line_buf, strlen(line_buf));
	    }

	    // 4. Tutup kedua file agar tersimpan ke SD Card
	    FATFS_Close(&src_ctx);
	    FATFS_Close(&temp_ctx);

	    // 5. Timpa file asli dengan temp (Atomic Swap)
	    FATFS_Delete(src_file);
	    return (FATFS_Rename(temp_file, src_file) == FS_OK);
}

bool ScheduleManager_AutoReschedule(const ScheduleItem_t* item) {
    if (item == NULL || item->repeat_days == 0) return false;

    const char* filename = GetFileNameByType(item->type);
    char time_str[24];
    char line_out[SCHED_CFG_MAX_LINE_LEN];

    /* Hitung waktu eksekusi berikutnya (Epoch + N hari) */
    uint32_t next_epoch = item->epoch_time + ((uint32_t)item->repeat_days * 86400);
    Schedule_EpochToDateTimeStr(next_epoch, time_str, sizeof(time_str));

    if (item->type == SCHED_TYPE_FERTILIZER) {
        /* Rakit baris Fertigasi komplit */
        char fert_tag[96] = "fertilizer[";
        char dose_tmp[20];
        uint8_t count = 0;

        for (uint8_t i = 0; i < RECIPE_NUM_FERTILIZERS; i++) {
            if (item->recipe.fert_volumes[i] > 0) {
                snprintf(dose_tmp, sizeof(dose_tmp), "%sfert%d:%d",
                         (count > 0) ? "," : "", i + 1, item->recipe.fert_volumes[i]);
                strncat(fert_tag, dose_tmp, sizeof(fert_tag) - strlen(fert_tag) - 1);
                count++;
            }
        }
        strncat(fert_tag, "]", sizeof(fert_tag) - strlen(fert_tag) - 1);

        snprintf(line_out, sizeof(line_out), "%s [%s] %s water[%d] mixing[%d] repeat[%d] status[pending]\n",
                 time_str, item->recipe.name, fert_tag,
                 item->recipe.water_volume, item->recipe.mixing_time_sec, item->repeat_days);
    } else {
        /* Rakit baris Irigasi air murni */
        snprintf(line_out, sizeof(line_out), "%s repeat[%d] water[%d] status[pending]\n",
                 time_str, item->repeat_days, item->recipe.water_volume);
    }

    /* Append baris baru ke paling bawah file SD Card */
    return FATFS_WriteAppend(&sched_file_ctx, filename, line_out);
}
