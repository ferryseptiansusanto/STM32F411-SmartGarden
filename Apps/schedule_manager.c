/*
 * schedule_manager.c
 *
 *  Created on: 22 Jul 2026
 *      Author: ferry
 */


#include "schedule_manager.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

static Schedule_t schedule_list[MAX_SCHEDULES];
static int total_schedules = 0;
static const char* sch_filename = "schedule.txt";

bool Schedule_Init(void) {
    FIL fobj;
    FRESULT fr;
    char line[128];
    total_schedules = 0;

    fr = f_open(&fobj, sch_filename, FA_READ);
    if (fr != FR_OK) {
        printf("[SCH] WARNING: File schedule.txt tidak ditemukan. Membuat berkas baru.\n");
        /* Buat file kosong baru jika belum ada */
        fr = f_open(&fobj, sch_filename, FA_CREATE_ALWAYS | FA_WRITE);
        if (fr == FR_OK) f_close(&fobj);
        return false;
    }

    /* Membaca baris demi baris, maksimal 20 jadwal */
    while (total_schedules < MAX_SCHEDULES) {
        /* Catat offset byte awal baris ini untuk kebutuhan pembaruan status (MarkAsFinish) nanti */
        uint32_t current_offset = f_tell(&fobj);

        if (!f_gets(line, sizeof(line), &fobj)) {
            break;
        }

        int d, m, y, hr, min;
        char rc_name[MAX_RECIPE_NAME];
        char stat_str[16];

        /* Format pembacaan teks: 22/07/2026 08:00 FERT_RECIPE1 onschedule */
        if (sscanf(line, "%d/%d/%d %d:%d %s %s", &d, &m, &y, &hr, &min, rc_name, stat_str) >= 6) {
            schedule_list[total_schedules].day = d;
            schedule_list[total_schedules].month = m;
            schedule_list[total_schedules].year = y;
            schedule_list[total_schedules].hour = hr;
            schedule_list[total_schedules].minute = min;
            strncpy(schedule_list[total_schedules].recipe_name, rc_name, MAX_RECIPE_NAME);
            schedule_list[total_schedules].file_pos = current_offset;

            if (strcmp(stat_str, "finish") == 0) {
                schedule_list[total_schedules].status = SCH_STATUS_FINISH;
            } else {
                schedule_list[total_schedules].status = SCH_STATUS_ONSCHEDULE;
            }
            total_schedules++;
        }
    }

    f_close(&fobj);
    printf("[SCH] Berhasil memuat %d jadwal dari SD Card.\n", total_schedules);
    return true;
}

bool Schedule_CheckTrigger(DS3231_DateTime current_time, char* out_recipe_name, int* out_sch_index) {
    for (int i = 0; i < total_schedules; i++) {
        /* Hanya periksa jadwal yang statusnya masih aktif (onschedule) */
        if (schedule_list[i].status == SCH_STATUS_ONSCHEDULE) {
            if (schedule_list[i].day    == current_time.date.day   &&
                schedule_list[i].month  == current_time.date.month &&
                schedule_list[i].year   == current_time.date.year  &&
                schedule_list[i].hour   == current_time.time.hours &&
                schedule_list[i].minute == current_time.time.minutes) {

                strncpy(out_recipe_name, schedule_list[i].recipe_name, MAX_RECIPE_NAME);
                *out_sch_index = i;
                return true; /* Waktu RTC sinkron, trigger FSM Pupuk! */
            }
        }
    }
    return false;
}

bool Schedule_MarkAsFinish(int sch_index) {
    FIL fobj;
    FRESULT fr;
    char line[128];

    if (sch_index >= total_schedules) return false;

    /* Update status di RAM internal */
    schedule_list[sch_index].status = SCH_STATUS_FINISH;

    /* --- UPDATE STATUS DI SD CARD ---
       MENGAPA FA_WRITE & f_lseek: Kita tidak menulis ulang seluruh file dari awal (yang lambat & boros resource),
       melainkan lompat langsung ke offset baris jadwal bersangkutan dan mengganti teks statusnya saja */
    fr = f_open(&fobj, sch_filename, FA_WRITE | FA_READ);
    if (fr != FR_OK) return false;

    fr = f_lseek(&fobj, schedule_list[sch_index].file_pos);
    if (fr == FR_OK) {
        /* Format ulang baris teks tersebut menjadi string finish */
        sprintf(line, "%02d/%02d/%04d %02d:%02d %s finish\n",
                schedule_list[sch_index].day,
                schedule_list[sch_index].month,
                schedule_list[sch_index].year,
                schedule_list[sch_index].hour,
                schedule_list[sch_index].minute,
                schedule_list[sch_index].recipe_name);

        f_puts(line, &fobj);
    }
    f_close(&fobj);
    printf("[SCH] File Berhasil Diupdate -> %s telah selesai (finish).\n", schedule_list[sch_index].recipe_name);
    return (fr == FR_OK);
}
