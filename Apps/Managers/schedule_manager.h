/**
 * @file    schedule_manager.h
 * @brief   Manajer Penjadwalan FSM. Mengurai waktu, mengevaluasi toleransi keterlambatan,
 * dan memanipulasi status jadwal di SD Card.
 * @note    Modul ini murni mengolah logika penjadwalan di RAM. I/O fisik SD Card
 * didelegasikan ke fatfs_wrapper.
 *
 * Created on: 3 Aug 2026
 * Author: ferry
 */

#ifndef MANAGERS_SCHEDULE_MANAGER_H_
#define MANAGERS_SCHEDULE_MANAGER_H_

#include <stdint.h>
#include <stdbool.h>
#include "Config/schedule_config.h"
#include "recipe_manager.h"

/**
 * @brief Enumerasi status eksekusi jadwal.
 */
typedef enum {
    SCHED_STATUS_PENDING = 0, /**< Menunggu dieksekusi */
    SCHED_STATUS_SUCCESS,     /**< Berhasil dieksekusi */
    SCHED_STATUS_SKIPPED,     /**< Dibatalkan oleh operator */
    SCHED_STATUS_URGENT,      /**< Terlambat parah, butuh konfirmasi manual */
    SCHED_STATUS_INVALID      /**< Format baris rusak / Syntax error */
} SchedStatus_t;

/**
 * @brief Enumerasi jenis jadwal.
 */
typedef enum {
    SCHED_TYPE_IRRIGATION = 0, /**< Jadwal pengairan air murni */
    SCHED_TYPE_FERTILIZER      /**< Jadwal pemupukan / fertigasi */
} SchedType_t;

/**
 * @brief Struktur data matang dari 1 baris jadwal yang diekstraksi.
 */
typedef struct {
    uint32_t      epoch_time;     /**< Waktu eksekusi dalam detik Unix Epoch */
    SchedType_t   type;           /**< Tipe jadwal (Irigasi / Fertigasi) */
    SchedStatus_t status;         /**< Status eksekusi jadwal saat ini */
    uint16_t      repeat_days;    /**< Interval pengulangan (hari), 0 = Tanpa pengulangan */
    FertRecipe_t  recipe;         /**< Resep racikan air, pupuk, dan mixing */
    uint32_t      line_number;    /**< Posisi nomor baris di dalam file SD Card */
} ScheduleItem_t;

/**
 * @brief   Menginisialisasi Context Schedule Manager untuk fatfs_wrapper.
 * @retval  bool true jika inisialisasi berhasil.
 */
bool ScheduleManager_Init(void);

/**
 * @brief   Mencari jadwal aktif ('pending' atau 'urgent') yang waktunya sudah tiba.
 * @param   type Jenis file jadwal yang ingin diperiksa (Irrigation/Fertilizer).
 * @param   out_item Pointer tempat menampung data jadwal matang.
 * @param   current_epoch_time Waktu RTC saat ini dalam detik Unix Epoch.
 * @param   max_delay_tolerance Batas toleransi keterlambatan maksimal dalam detik.
 * @retval  bool true jika ditemukan jadwal yang harus dieksekusi/dikonfirmasi.
 */
bool ScheduleManager_GetDueSchedule(SchedType_t type, ScheduleItem_t* out_item,
                                   uint32_t current_epoch_time, uint32_t max_delay_tolerance);

/**
 * @brief   Memperbarui tag status jadwal di file SD Card secara aman (Atomic Temp-Swap).
 * @param   type Jenis file jadwal (Irrigation/Fertilizer).
 * @param   line_number Nomor baris yang akan diperbarui statusnya.
 * @param   new_status Status baru (misal SCHED_STATUS_SUCCESS).
 * @retval  bool true jika update file di SD Card berhasil.
 */
bool ScheduleManager_UpdateStatus(SchedType_t type, uint32_t line_number, SchedStatus_t new_status);

/**
 * @brief   Membuat baris jadwal baru untuk N hari berikutnya jika repeat_days > 0.
 * @param   item Pointer ke jadwal asli yang baru saja diselesaikan.
 * @retval  bool true jika penambahan jadwal baru ke SD Card berhasil.
 */
bool ScheduleManager_AutoReschedule(const ScheduleItem_t* item);

/**
 * @brief   Mengonversi teks tanggal "YYYY-MM-DD HH:MM:SS" menjadi Unix Epoch (detik).
 * @param   time_str String waktu.
 * @retval  uint32_t Waktu dalam Unix Epoch (detik), 0 jika format invalid.
 */
uint32_t Schedule_DateTimeToEpoch(const char* time_str);

/**
 * @brief   Mengonversi Unix Epoch (detik) menjadi string "YYYY-MM-DD HH:MM:SS".
 * @param   epoch Waktu Unix Epoch.
 * @param   out_buf Buffer penampung hasil teks.
 * @param   max_len Ukuran maksimal buffer.
 */
void Schedule_EpochToDateTimeStr(uint32_t epoch, char* out_buf, size_t max_len);

#endif /* MANAGERS_SCHEDULE_MANAGER_H_ */
