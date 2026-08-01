#include "fatfs_wrapper.h"
#include <string.h>

static FATFS fs;
static SemaphoreHandle_t FatFs_Mutex = NULL;

/**
 * @brief Inisialisasi Mutex untuk melindungi SD Card.
 * @note  Wajib dipanggil sebelum Task FreeRTOS mulai berjalan (di STATE_INIT_HARDWARE).
 */
void FATFS_InitMutex(void) {
    if (FatFs_Mutex == NULL) {
        FatFs_Mutex = xSemaphoreCreateMutex();
    }
}

/**
 * @brief Mount SD Card ke dalam sistem file.
 * @param drive_path Label drive (contoh: "", "0:", dll).
 * @param pdrv       Physical drive number.
 * @param dev        Pointer ke struktur Context SPI Hardware.
 */
FS_Status FATFS_Mount(const char* drive_path, BYTE pdrv, SPI_StorageDevice *dev) {
    if (dev == NULL || pdrv >= FF_VOLUMES) return FS_ERROR_MOUNT;

    // Suntikkan Hardware Context ke lapisan bawah FatFs (diskio.c)
    disk_register_device(pdrv, dev);

    // Mount sistem file (Force Mount = 1)
    if (f_mount(&fs, drive_path, 1) != FR_OK) {
        return FS_ERROR_MOUNT;
    }
    return FS_OK;
}

/**
 * @brief  Menulis data ke akhir file (Append) secara aman.
 * @note   MENGAPA KITA PAKAI MUTEX DI SINI? Jika log_manager (Prioritas Rendah)
 * sedang menulis log, dan tiba-tiba schedule_manager (Prioritas Tinggi)
 * menyela untuk memperbarui status jadwal, file system akan hancur.
 * Mutex ini memaksa schedule_manager antre menunggu log_manager selesai.
 */
FS_Status FATFS_WriteAppend(FileContext_t *ctx, const char *path, const char *data) {
    if (FatFs_Mutex == NULL) return FS_ERROR_OPEN;

    // Tunggu maksimal 1000ms untuk mendapatkan akses SD Card (Fail-Safe)
    if (xSemaphoreTake(FatFs_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {

        FS_Status status = FS_OK;
        UINT bytes_written;

        // Buka file dengan mode Tulis & Append (tambah di bawah)
        if (f_open(&ctx->file, path, FA_OPEN_APPEND | FA_WRITE) == FR_OK) {
            if (f_write(&ctx->file, data, strlen(data), &bytes_written) != FR_OK) {
                status = FS_ERROR_WRITE;
            }
            f_close(&ctx->file); // Wajib ditutup agar tersimpan ke flash disk
        } else {
            status = FS_ERROR_OPEN;
        }

        // SELALU KEMBALIKAN MUTEX! Jika tidak, seluruh sistem yang butuh SD Card akan Deadlock.
        xSemaphoreGive(FatFs_Mutex);
        return status;
    }

    return FS_LOCKED; // Gagal karena SD Card sedang dikunci Task lain terlalu lama
}

/**
 * @brief  Membaca isi file ke dalam buffer secara utuh dan aman (Thread-Safe).
 * @param  ctx         Pointer ke struktur Context (agar aman antar-Task).
 * @param  path        Nama/lokasi file di SD Card (contoh: "jadwal.txt").
 * @param  buffer      Alokasi memori RAM (array) tempat data diletakkan.
 * @param  buffer_size Ukuran maksimal array buffer.
 * @param  bytes_read  Pointer untuk melaporkan berapa banyak huruf yang berhasil ditarik.
 * @return FS_Status   FS_OK jika sukses, atau kode error lainnya.
 * * @note   MENGAPA KITA MENGGUNAKAN MUTEX DI SINI?
 * Proses membaca dari SD Card melalui SPI membutuhkan beberapa milidetik.
 * Jika Mutex tidak dipakai, Task yang lebih tinggi bisa tiba-tiba merebut
 * SPI untuk menulis log, sehingga data yang sedang kita baca tertimpa
 * atau menghasilkan "HardFault".
 */
FS_Status FATFS_Read(FileContext_t *ctx, const char *path, char *buffer, UINT buffer_size, UINT *bytes_read) {
    if (FatFs_Mutex == NULL) return FS_ERROR_OPEN;

    FS_Status status = FS_LOCKED;

    // 1. KUNCI SD CARD (Tunggu maksimal 1 detik agar tidak Deadlock)
    if (xSemaphoreTake(FatFs_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {

        // 2. Buka file khusus untuk mode baca (Read)
        if (f_open(&ctx->file, path, FA_READ) == FR_OK) {

            // 3. Baca Data
            // FAIL-SAFE: Kita kurangi 1 (buffer_size - 1) agar selalu ada tempat
            // untuk meletakkan karakter penutup teks ('\0') di baris berikutnya.
            FRESULT res = f_read(&ctx->file, buffer, buffer_size - 1, bytes_read);

            if (res == FR_OK) {
                // Pastikan buffer menjadi string C yang valid, mencegah Buffer Overflow
                // saat nanti diparsing oleh fungsi strtok() atau sscanf() di Layer Aplikasi
                buffer[*bytes_read] = '\0';
                status = FS_OK;
            } else {
                status = FS_ERROR_READ;
            }

            // 4. Tutup File
            f_close(&ctx->file);
        } else {
            status = FS_ERROR_OPEN;
        }

        // 5. KEMBALIKAN KUNCI SD CARD KEPADA SISTEM
        xSemaphoreGive(FatFs_Mutex);
    }

    return status;
}

/**
 * @brief  Menghapus file secara aman dari bentrokan Task.
 * @return FS_OK jika berhasil dihapus atau file tidak ada.
 */
FS_Status FATFS_Delete(const char *path) {
    if (FatFs_Mutex == NULL) return FS_ERROR_OPEN;

    FS_Status status = FS_LOCKED;

    if (xSemaphoreTake(FatFs_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        FRESULT res = f_unlink(path);

        // FR_NO_FILE dianggap OK karena tujuannya memang agar file tidak ada
        if (res == FR_OK || res == FR_NO_FILE) {
            status = FS_OK;
        } else {
            status = FS_ERROR_WRITE;
        }

        xSemaphoreGive(FatFs_Mutex);
    }
    return status;
}
