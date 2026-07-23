/*
 * logger.c
 *
 *  Created on: 10 Apr 2026
 *      Author: ferry
 */


#include <logger.h>
#include <string.h>
#include <stdio.h>
#include "storage.h"
#include "delay.h"
static FATFS fs;
static FIL file;

LOG_Status LOG_Init(void) {
	 FRESULT res = f_mount(&fs, "", 1);
	 return (res == FR_OK) ? LOG_OK : LOG_ERROR;
}

LOG_Status LOG_Open(const char *filename) {
    FRESULT res = f_open(&file, filename, FA_WRITE | FA_OPEN_APPEND);
    if (res == FR_OK) {
        if (f_size(&file) == 0) {
            // tulis header hanya sekali saat file kosong
            LOG_Write(LOG_Header);
        }
        return LOG_OK;
    }
    return LOG_ERROR;
}

LOG_Status LOG_Append(const char *data) {
    char line[256];
    snprintf(line, sizeof(line), "%s\n", data);
    return LOG_Write(line);
}

LOG_Status LOG_Read(const char *filename, char *buffer, UINT bufsize, UINT *bytesRead) {
    FRESULT res = f_open(&file, filename, FA_READ);
    if (res == FR_OK) {
        res = f_read(&file, buffer, bufsize, bytesRead);
        buffer[*bytesRead] = '\0'; // null-terminate agar aman diprint
        f_close(&file);
        return (res == FR_OK) ? LOG_OK : LOG_ERROR;
    }
    return LOG_ERROR;
}

LOG_Status LOG_Write(const char *value) {
	UINT bw, length;
	length = strlen(value);
	FRESULT res = f_write(&file, value, length, &bw);
	//printf("value:%s\r\n",value );
    return (res == FR_OK && bw == length) ? LOG_OK : LOG_ERROR;
}

LOG_Status LOG_Sync(void) {
	FRESULT res = f_sync(&file);
    return (res == FR_OK) ? LOG_OK : LOG_ERROR;
}

LOG_Status LOG_Delete(const char *filename) {
    return (f_unlink(filename) == FR_OK) ? LOG_OK : LOG_ERROR;
}

LOG_Status LOG_Last() {
    return (f_lseek(&file, f_size(&file)) == FR_OK) ? LOG_OK : LOG_ERROR;
}


// Tutup file setelah semua logging selesai
LOG_Status LOG_Close(void) {

    FRESULT res = f_close(&file);
	return (res == FR_OK) ? LOG_OK : LOG_ERROR;
}

LOG_Status LOG_Unmount(void){
    STORAGE_Deinit();
	FRESULT res = f_mount(NULL, "", 0);   // unmount
	return (res == FR_OK) ? LOG_OK : LOG_ERROR;
}

void LOG_Example(void) {
	uint8_t buffer[512];
	UINT bytesRead;

	// Inisialisasi FatFs
	if (LOG_Init() == LOG_OK) {
	  //LOG_Delete("log.csv");
	  if (LOG_Open("log.csv") == LOG_OK) {
		  //LOG_Last();
		  LOG_Append("6") ;
		  //LOG_Sync();
		  LOG_Close();
	  }
	}
	// Baca isi file
	if (LOG_Read("log.csv", (char*)buffer, sizeof(buffer), &bytesRead) == LOG_OK) {
	  buffer[bytesRead] = '\0'; // null-terminate
	  printf("Preview File:\r\n%s\n", buffer);
	} else {
	  printf("Gagal membaca file log.\n");
	}
}
