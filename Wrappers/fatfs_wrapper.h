/*
 * fatfs_wrapper.h
 *
 *  Created on: 10 Apr 2026
 *      Author: ferry
 */

#ifndef WRAPPER_FATFS_WRAPPER_H_
#define WRAPPER_FATFS_WRAPPER_H_

#include <storage_wrapper.h>
#include "ff.h"
#include "spi_wrapper.h"
#define FAT_Header "Date,Time,Temperature,Current,Voltage\r\n"
extern SPI_StorageDevice SDCard_Ctx;
// Status hasil operasi
typedef enum {
    F_OK = 0,
    F_ERROR
} F_Status;

// Inisialisasi filesystem
F_Status FAT_Init(const char* drive_path, BYTE pdrv, SPI_StorageDevice *dev);

F_Status FAT_Open(const char *filename);

// Append baris CSV ke file log
F_Status FAT_Append(const char *data);

// Membaca isi file CSV ke buffer
F_Status FAT_Read(const char *filename, char *buffer, UINT bufsize, UINT *bytesRead);

// Menulis isi data CSV ke file
F_Status FAT_Write(const char *value);

// Sync isi data CSV ke file
F_Status FAT_Sync(void);

// Hapus file CSV
F_Status FAT_Delete(const char *filename);

// Membuat file CSV baru dengan header
F_Status FAT_CreateHeader(void);

// Menutup file
F_Status FAT_Close(void);

// Unmount Drive
F_Status FAT_Unmount(const char* drive_path, BYTE pdrv);

#endif /* WRAPPER_FATFS_WRAPPER_H_ */
