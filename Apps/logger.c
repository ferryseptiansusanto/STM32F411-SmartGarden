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
#include "diskio.h"
static FIL file;

// Jika ingin mendukung >1 SD Card menyala bersamaan, ubah menjadi array
static FATFS SDFatFS[FF_VOLUMES];

LOG_Status LOG_Init(const char* drive_path, BYTE pdrv, SPI_StorageDevice *dev) {
	if (dev == NULL || pdrv >= FF_VOLUMES) return LOG_ERROR;

	// 1. Suntikkan (Inject) Hardware Context ke lapisan bawah FatFs (diskio.c)
	disk_register_device(pdrv, dev);

	// 2. Sekarang FatFs tahu siapa yang memegang Drive "pdrv", mari kita mount!
	// Menit ini FatFs akan memanggil disk_initialize(pdrv) di latar belakang
	// parameter ke-3 Option 0 - (Delayed Mount / Mount Malas)
	// Option 1 - (Force Mount / Mount Langsung)
	FRESULT res = f_mount(&SDFatFS[pdrv], drive_path, 1);

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

LOG_Status LOG_Unmount(const char* drive_path, BYTE pdrv){
	if (pdrv >= FF_VOLUMES) return LOG_ERROR;

	    // 1. Panggil pintu belakang deinit hardware di diskio_ioctl
	    disk_ioctl(pdrv, CTRL_POWER, NULL);

	    // 2. Unmount software dari RAM
	    FRESULT res = f_mount(NULL, drive_path, 0);

	    return (res == FR_OK) ? LOG_OK : LOG_ERROR;
}

