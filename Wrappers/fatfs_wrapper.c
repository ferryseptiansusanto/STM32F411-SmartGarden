/**
 * @file    fatfs_wrapper.c
 * @brief   Implementasi proteksi akses memori SD Card (Recursive Mutex Native).
 */

#include "fatfs_wrapper.h"
#include "storage_wrapper.h"
#include "diskio.h"
#include <string.h>
#include "ds3231_wrapper.h" // Di-include di sini untuk akses RTC

static FATFS fs;
static SemaphoreHandle_t FatFs_Mutex = NULL;

extern DS3231_Device_t DS3231_Ctx; // Objek RTC utama dari Layer Aplikasi

/**
 * @brief Inisialisasi Mutex untuk melindungi SD Card.
 * @note  Wajib dipanggil sebelum Task FreeRTOS mulai berjalan (di STATE_INIT_HARDWARE).
 */
void FATFS_InitMutex(void) {
    if (FatFs_Mutex == NULL) {
        FatFs_Mutex = xSemaphoreCreateRecursiveMutex(); // GUNAKAN CREATERECURSIVEMUTEX
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
 * @brief  Melepas (Unmount) SD Card dari sistem file dan menonaktifkan daya hardware.
 * @note   Sangat krusial dipanggil sebelum FSM masuk ke STOP Mode (Deep Sleep)
 * atau saat operator ingin mencabut SD Card.
 */
FS_Status FATFS_Unmount(const char* drive_path, BYTE pdrv) {
    if (pdrv >= FF_VOLUMES) return FS_ERROR_MOUNT;

    FS_Status status = FS_ERROR_MOUNT;

    // 1. Ambil Recursive Mutex untuk memastikan tidak ada Task lain yang sedang membaca/menulis
    if (FatFs_Mutex != NULL && xSemaphoreTakeRecursive(FatFs_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {

        // 2. Panggil IOCTL untuk de-inisialisasi daya/hardware di level diskio.c (jika didukung)
        disk_ioctl(pdrv, CTRL_POWER, NULL);

        // 3. Unmount sistem file dari RAM FatFs (Passing NULL ke f_mount)
        FRESULT res = f_mount(NULL, (drive_path != NULL) ? drive_path : "", 0);

        if (res == FR_OK) {
            status = FS_OK;
        }

        // 4. Lepas Recursive Mutex
        xSemaphoreGiveRecursive(FatFs_Mutex);
    }

    return status;
}

FS_Status FATFS_Open(FileContext_t *ctx, const char *path, BYTE mode) {
    if (FatFs_Mutex == NULL) return FS_ERROR_OPEN;
    if (xSemaphoreTakeRecursive(FatFs_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (f_open(&ctx->file, path, mode) == FR_OK) {
            return FS_OK;
        }
        xSemaphoreGiveRecursive(FatFs_Mutex); // Batal buka, kembalikan gembok
    }
    return FS_ERROR_OPEN;
}

FS_Status FATFS_Close(FileContext_t *ctx) {
    f_close(&ctx->file);
    xSemaphoreGiveRecursive(FatFs_Mutex); // Lepas gembok
    return FS_OK;
}

FS_Status FATFS_WriteRaw(FileContext_t *ctx, const void *data, UINT len) {
	if (ctx == NULL || data == NULL || FatFs_Mutex == NULL) return FS_ERROR_WRITE;

	FS_Status status = FS_ERROR_WRITE;

	// Ambil gembok rekursif untuk validasi kepemilikan
	if (xSemaphoreTakeRecursive(FatFs_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
		UINT bw;
		if (f_write(&ctx->file, data, len, &bw) == FR_OK && bw == len) {
			status = FS_OK;
		}
		xSemaphoreGiveRecursive(FatFs_Mutex);
	}
	return status;
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
    if (xSemaphoreTakeRecursive(FatFs_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {

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
        xSemaphoreGiveRecursive(FatFs_Mutex);
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
    if (xSemaphoreTakeRecursive(FatFs_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {

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
        xSemaphoreGiveRecursive(FatFs_Mutex);
    }

    return status;
}


FS_Status FATFS_ReadLine(FileContext_t *ctx, char *buffer, int len) {
    if (ctx == NULL || buffer == NULL || FatFs_Mutex == NULL) return FS_ERROR_READ;

    FS_Status status = FS_ERROR_READ;

    if (xSemaphoreTakeRecursive(FatFs_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (f_gets(buffer, len, &ctx->file) != NULL) {
            status = FS_OK;
        }
        xSemaphoreGiveRecursive(FatFs_Mutex);
    }
    return status;
}

/**
 * @brief  Mengganti kata spesifik pada baris yang mengandung keyword tertentu (Temp-Swap Method).
 * @param  filepath         Nama file target (contoh: "jadwal.txt")
 * @param  line_identifier  Keyword penanda baris (contoh: "2026-07-29 08:00:00")
 * @param  old_word         Kata yang ingin diganti (contoh: "pending")
 * @param  new_word         Kata pengganti (contoh: "success")
 * @return FS_OK jika berhasil.
 * * @note   FAIL-SAFE: Menggunakan metode Temp-Swap. Membaca baris demi baris,
 * disalin ke temp.csv, lalu di-rename. Aman 100% dari mati listrik mendadak!
 */
FS_Status FATFS_ReplaceWordInLine(const char *filepath, const char *line_identifier, const char *old_word, const char *new_word) {
    if (FatFs_Mutex == NULL) return FS_ERROR_OPEN;

    FS_Status status = FS_ERROR_WRITE;
    const char *temp_file = "temp.txt";

    // 1. Kunci SD Card secara penuh (Atomic Transaction)
    if (xSemaphoreTakeRecursive(FatFs_Mutex, pdMS_TO_TICKS(3000)) == pdTRUE) {

        FIL f_source, f_temp;
        char buffer[256]; // Alokasi RAM kecil, cukup untuk 1 baris jadwal

        // 2. Buka file asli (READ) dan buat file sementara (WRITE)
        if (f_open(&f_source, filepath, FA_READ) == FR_OK) {
            if (f_open(&f_temp, temp_file, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {

                status = FS_OK;

                // 3. Baca baris demi baris (Hemat RAM)
                while (f_gets(buffer, sizeof(buffer), &f_source)) {

                    // Apakah baris ini adalah baris target yang dicari?
                    if (strstr(buffer, line_identifier) != NULL) {

                        // Cari posisi kata lama yang ingin diganti
                        char *pos = strstr(buffer, old_word);
                        if (pos != NULL) {
                            // Hitung panjang teks sebelum kata target
                            int prefix_len = pos - buffer;

                            // Tulis potongan awal sebelum kata
                            f_write(&f_temp, buffer, prefix_len, NULL);
                            // Tulis kata baru ("success")
                            f_write(&f_temp, new_word, strlen(new_word), NULL);
                            // Tulis sisa teks di belakangnya
                            f_write(&f_temp, pos + strlen(old_word), strlen(pos + strlen(old_word)), NULL);
                            continue; // Lanjut ke iterasi while berikutnya
                        }
                    }

                    // Jika bukan baris target, salin teks utuh apa adanya
                    f_write(&f_temp, buffer, strlen(buffer), NULL);
                }

                f_close(&f_temp);
            }
            f_close(&f_source);
        }

        // 4. Jika penyalinan sukses, lakukan penghapusan dan RENAME
        if (status == FS_OK) {
            f_unlink(filepath);                // Hapus file asli
            f_rename(temp_file, filepath);     // Ubah nama temp.txt menjadi nama file asli
        } else {
            f_unlink(temp_file);               // Jika gagal, bersihkan file sampah
        }

        // 5. Lepas kunci Mutex
        xSemaphoreGiveRecursive(FatFs_Mutex);
    } else {
        status = FS_LOCKED;
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

    if (xSemaphoreTakeRecursive(FatFs_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        FRESULT res = f_unlink(path);

        // FR_NO_FILE dianggap OK karena tujuannya memang agar file tidak ada
        if (res == FR_OK || res == FR_NO_FILE) {
            status = FS_OK;
        } else {
            status = FS_ERROR_WRITE;
        }

        xSemaphoreGiveRecursive(FatFs_Mutex);
    }
    return status;
}

/**
 * @brief  Mengubah nama file secara aman (Thread-Safe).
 * @param  old_name    Nama file saat ini (contoh: "temp.txt").
 * @param  new_name    Nama file tujuan (contoh: "jadwal.txt").
 * @return FS_OK jika sukses.
 * @note   MENGAPA KITA BUTUH INI? Sangat krusial untuk metode "Temp-Swap"
 * (Fail-Safe) di Layer Aplikasi saat memperbarui status baris jadwal.
 * Memastikan FSM tidak pernah kehilangan data jika listrik mati mendadak.
 */
FS_Status FATFS_Rename(const char *old_name, const char *new_name) {
    if (FatFs_Mutex == NULL) return FS_ERROR_OPEN;

    FS_Status status = FS_LOCKED;

    // KUNCI SD CARD (Tunggu maksimal 1 detik)
    if (xSemaphoreTakeRecursive(FatFs_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {

        // Panggil fungsi bawaan FatFs untuk rename
        FRESULT res = f_rename(old_name, new_name);

        if (res == FR_OK) {
            status = FS_OK;
        } else {
            // Bisa jadi karena file old_name tidak ada, atau new_name sudah ada
            status = FS_ERROR_WRITE;
        }

        // LEPASKAN KUNCI SD CARD
        xSemaphoreGiveRecursive(FatFs_Mutex);
    }
    return status;
}

/**
 * @brief  Callback Wajib FatFs untuk memberikan Stempel Waktu (Timestamp).
 * @note   Dipanggil oleh ff.c saat operasi file (f_open, f_write, dll).
 */
DWORD get_fattime(void) {
    DS3231_DateTime_t now;

    // Membaca RTC via DS3231 Wrapper (Layer 2 Middleware)
    if (DS3231_GetDateTime(&DS3231_Ctx, &now)) {
        return ((DWORD)(now.date.year - 1980) << 25)  //Bit 31:25 (7 bit): Tahun (Dihitung dari 1980)
             | ((DWORD)now.date.month         << 21)  //Bit 24:21 (4 bit): Bulan (1-12)
             | ((DWORD)now.date.date          << 16)  //Bit 20:16 (5 bit): Tanggal (1-31)
             | ((DWORD)now.time.hours         << 11)  //Bit 15:11 (5 bit): Jam (0-23)
             | ((DWORD)now.time.minutes       << 5)   //Bit 10:5  (6 bit): Menit (0-59)
             | ((DWORD)(now.time.seconds / 2));       //Bit 4:0   (5 bit): Detik dibagi 2 (0-29)
    }

    // Fail-Safe: Waktu default (1 Jan 2026 00:00:00) jika RTC gagal dibaca
    return ((DWORD)(2026 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16);
}
