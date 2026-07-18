/*
 * logger.h
 *
 *  Created on: 10 Apr 2026
 *      Author: ferry
 */

#ifndef INC_LOGGER_H_
#define INC_LOGGER_H_

#include "ff.h"
#include "spi_wrapper.h"
#define LOG_Header "Date,Time,Temperature,Current,Voltage\r\n"
extern SPI_Context SDCard_Ctx;
// Status hasil operasi
typedef enum {
    LOG_OK = 0,
    LOG_ERROR
} LOG_Status;

// Inisialisasi filesystem
LOG_Status LOG_Init(void);

LOG_Status LOG_Open(const char *filename);

// Append baris CSV ke file log
LOG_Status LOG_Append(const char *data);

// Membaca isi file CSV ke buffer
LOG_Status LOG_Read(const char *filename, char *buffer, UINT bufsize, UINT *bytesRead);

// Menulis isi data CSV ke file
LOG_Status LOG_Write(const char *value);

// Sync isi data CSV ke file
LOG_Status LOG_Sync(void);

// Hapus file CSV
LOG_Status LOG_Delete(const char *filename);

// Membuat file CSV baru dengan header
LOG_Status LOG_CreateHeader(void);

// Menutup file
LOG_Status LOG_Close(void);

// Unmount Drive
LOG_Status LOG_Unmount(void);

// Example
void LOG_Example(void);

#endif /* INC_LOGGER_H_ */
