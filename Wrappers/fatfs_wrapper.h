/**
 * @file    fatfs_wrapper.h
 * Middleware Layer 2 untuk Sistem File SD Card (Thread-Safe via Recursive Mutex).
 * @note    Menggunakan Mutex untuk melindungi akses konkuren dari berbagai Task.
 *
 *  Created on: 10 Apr 2026
 *      Author: ferry
 */

#ifndef WRAPPER_FATFS_WRAPPER_H_
#define WRAPPER_FATFS_WRAPPER_H_

#include "storage_wrapper.h"
#include "ff.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdbool.h>

// Status hasil operasi
typedef enum {
    FS_OK = 0,
    FS_ERROR_MOUNT,
    FS_ERROR_OPEN,
    FS_ERROR_WRITE,
    FS_ERROR_READ,
    FS_LOCKED // Gagal mengambil Mutex (Timeout)
} FS_Status;

/**
 * @brief Struktur Context untuk file agar dapat digunakan oleh banyak instance.
 */
typedef struct {
    FIL file;
    char file_path[32];
} FileContext_t;

// API Sistem File
void FATFS_InitMutex(void);
FS_Status FATFS_Mount(const char* drive_path, BYTE pdrv, SPI_StorageDevice *dev);
/**
 * @brief  Melepas (Unmount) SD Card dari sistem file secara aman.
 * @param  drive_path Label drive (contoh: "", "0:", dll).
 * @param  pdrv       Physical drive number (contoh: 0).
 * @return FS_Status  FS_OK jika berhasil.
 */
FS_Status FATFS_Unmount(const char* drive_path, BYTE pdrv);

// Tambahan API Dasar untuk manipulasi baris manual
FS_Status FATFS_Open(FileContext_t *ctx, const char *path, BYTE mode);
FS_Status FATFS_Close(FileContext_t *ctx);
FS_Status FATFS_WriteRaw(FileContext_t *ctx, const void *data, UINT len);

// API Operasi File (Aman dari bentrokan Task)
FS_Status FATFS_WriteAppend(FileContext_t *ctx, const char *path, const char *data);
FS_Status FATFS_Read(FileContext_t *ctx, const char *path, char *buffer, UINT buffer_size, UINT *bytes_read);
FS_Status FATFS_ReadLine(FileContext_t *ctx, char *buffer, int len);
FS_Status FATFS_ReplaceWordInLine(const char *filepath, const char *line_identifier, const char *old_word, const char *new_word);
FS_Status FATFS_Delete(const char *path);
FS_Status FATFS_Rename(const char *old_name, const char *new_name);

#endif /* WRAPPER_FATFS_WRAPPER_H_ */
