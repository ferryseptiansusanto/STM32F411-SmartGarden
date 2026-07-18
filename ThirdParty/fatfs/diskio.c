/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2025        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include <ds3231_wrapper.h>
#include "ff.h"			/* Basic definitions of FatFs */
#include "diskio.h"		/* Declarations FatFs MAI */
/* Example: Declarations of the platform and disk functions in the project */
#include "storage.h"
/* Example: Mapping of physical drive number for each drive */
#define DEV_MMC		0	/* Map MMC/SD card to physical drive 1 */

extern SPI_Context SDCard_Ctx;
extern I2C_RTCDevice DS3231_Ctx;
/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
    if (pdrv == DEV_MMC) {
        return (STORAGE_GetStatus(&SDCard_Ctx) == STORAGE_OK) ? 0 : STA_NOINIT;
    }
    return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
    if (pdrv == DEV_MMC) {
        return (STORAGE_Init_Cmd_Sequence(&SDCard_Ctx) == STORAGE_OK) ? 0 : STA_NOINIT;
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
    if (pdrv == DEV_MMC) {
    	StorageStatus_t stat = STORAGE_ReadBlocks(&SDCard_Ctx, buff, sector, count) ;
        return (stat== STORAGE_OK) ? RES_OK : RES_ERROR;
    }
    return RES_PARERR;
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
    if (pdrv == DEV_MMC) {
        return (STORAGE_WriteBlocks(&SDCard_Ctx, buff, sector, count) == STORAGE_OK) ? RES_OK : RES_ERROR;
    }
    return RES_PARERR;
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
    if (pdrv == DEV_MMC) {
        switch (cmd) {
        case CTRL_SYNC:         return RES_OK;
        case GET_SECTOR_SIZE:   *(WORD*)buff = 512; return RES_OK;
        case GET_BLOCK_SIZE:    *(DWORD*)buff = 1;  return RES_OK;
        case GET_SECTOR_COUNT:  *(DWORD*)buff = STORAGE_GetSectorCount(&SDCard_Ctx); return RES_OK;
        }
    }
    return RES_PARERR;
}

DWORD get_fattime(void)
{
    DS3231_DateTime now = DS3231_GetDateTime(&DS3231_Ctx);

    return ((DWORD)(now.date.year - 1980) << 25)
         | ((DWORD)now.date.month << 21)
         | ((DWORD)now.date.day << 16)
         | ((DWORD)now.time.hours << 11)
         | ((DWORD)now.time.minutes << 5)
         | ((DWORD)(now.time.seconds / 2));
}
