/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2025        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include <storage_wrapper.h>
#include "ff.h"			/* Basic definitions of FatFs */
#include "diskio.h"		/* Declarations FatFs MAI */
/* Example: Declarations of the platform and disk functions in the project */
/* Example: Mapping of physical drive number for each drive */
#define DEV_MMC		0	/* Map MMC/SD card to physical drive 1 */

extern SPI_StorageDevice SDCard_Ctx;

// [BUKU CATATAN FATFS]: Array untuk menyimpan pointer hardware berdasarkan nomor drive
static SPI_StorageDevice *disk_devices[FF_VOLUMES] = {NULL};

// Fungsi Injector: Dipanggil sekali di awal untuk mendaftarkan perangkat
void disk_register_device(BYTE pdrv, SPI_StorageDevice *dev) {
    if (pdrv < FF_VOLUMES) {
        disk_devices[pdrv] = dev;
    }
}

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
    if (pdrv >= FF_VOLUMES || disk_devices[pdrv] == NULL) return STA_NOINIT;

	// Cek status inisialisasi dari struct
	if (!disk_devices[pdrv]->is_initialized) return STA_NOINIT;
	return 0;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
    if (pdrv >= FF_VOLUMES || disk_devices[pdrv] == NULL) return STA_NOINIT;

	// Panggil STORAGE_Init milik device yang terdaftar di pdrv ini
	if (STORAGE_Init(disk_devices[pdrv]) == STORAGE_OK) {
		return 0; // Berhasil
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
    if (pdrv >= FF_VOLUMES || disk_devices[pdrv] == NULL) return RES_NOTRDY;

	// FatFs hanya ngasih nomor pdrv, kita terjemahkan ke dev context
	if (STORAGE_ReadBlocks(disk_devices[pdrv], buff, sector, count) == STORAGE_OK) {
		return RES_OK;
	}
	return RES_ERROR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if (pdrv >= FF_VOLUMES || disk_devices[pdrv] == NULL) return RES_NOTRDY;

	if (STORAGE_WriteBlocks(disk_devices[pdrv], (uint8_t*)buff, sector, count) == STORAGE_OK) {
		return RES_OK;
	}
	return RES_ERROR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	if (pdrv >= FF_VOLUMES || disk_devices[pdrv] == NULL) return RES_NOTRDY;

	SPI_StorageDevice *dev = disk_devices[pdrv];

	switch (cmd) {
		case CTRL_SYNC:
			return RES_OK;

		case GET_SECTOR_COUNT:
			*(DWORD*)buff = STORAGE_GetSectorCount(dev);
			return RES_OK;

		case GET_SECTOR_SIZE:
			*(WORD*)buff = 512;
			return RES_OK;

		case GET_BLOCK_SIZE:
			*(DWORD*)buff = 1;
			return RES_OK;

		case CTRL_POWER: // Pintu Belakang untuk LOG_Unmount
			if (buff == NULL) {
				STORAGE_Deinit(dev);
			}
			return RES_OK;

		default:
			return RES_PARERR;
	}
}


