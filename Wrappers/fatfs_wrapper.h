C
/**
 * @file    fatfs_wrapper.h
 * @brief   Middleware Layer 2 untuk Sistem File SD Card (Thread-Safe).
 * @note    Menggunakan Mutex untuk melindungi akses konkuren dari berbagai Task.
 *
 *  Created on: 10 Apr 2026
 *      Author: ferry
 */

#ifndef WRAPPER_FATFS_WRAPPER_H_
#define WRAPPER_FATFS_WRAPPER_H_

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
FS_Status FATFS_Unmount(void);

// API Operasi File (Aman dari bentrokan Task)
FS_Status FATFS_WriteAppend(FileContext_t *ctx, const char *path, const char *data);
FS_Status FATFS_Read(FileContext_t *ctx, const char *path, char *buffer, UINT buffer_size, UINT *bytes_read);
FS_Status FATFS_Delete(const char *path);

#endif /* WRAPPER_FATFS_WRAPPER_H_ */
