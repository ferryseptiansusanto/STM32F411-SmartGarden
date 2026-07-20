/*
 * storage.c
 *
 *  Created on: 30 Mar 2026
 *      Author: ferry
 */
#include "spi.h"
#include "storage.h"
#include "string.h"

#include "delay.h"
//#define USE_DMA 1
#define SPI_TIMEOUT_MS 1000
#define SECTOR_SIZE 512U
#define STORAGE_MODE SPI_MODE_DMA
static bool card_present = true;
static uint8_t card_initialized = 0;
//static uint8_t countFailure=0;
static uint32_t sector_count = 32768; // contoh kapasitas default, nanti bisa di-update dari CSD
uint8_t dummy = 0xFF;
static uint8_t tx_dummy[SECTOR_SIZE] = {0xFF};
static uint8_t sdhc = 0;
extern SPI_HandleTypeDef hspi1;

//extern void test_disk_read(void);

void STORAGE_Init(SPI_Context *dev) {
//	*dev=spi1_ctx;  //hanya copy perubahan pada spi1_ctx tidak berpengaruh pada dev
	dev->hspi = spi1_ctx.hspi;
	dev->cs_pin = STORAGE_PIN_CS;
	dev->cs_port = STORAGE_PORT_CS;
	dev->mode = STORAGE_MODE;
    dev->tx_sem   = spi1_ctx.tx_sem;
    dev->rx_sem   = spi1_ctx.rx_sem;
    dev->txrx_sem = spi1_ctx.txrx_sem;
}

static StorageStatus_t sd_wait_ready(SPI_Context *dev) {
    uint32_t timeout = GetTick() + SPI_TIMEOUT_MS;
    uint8_t resp;
    do {
        if (SPI_TransmitReceive(dev, dummy, &resp, 1) != SPI_OK) {
//        	printf("SPI error in wait_ready\r\n");
        	return STORAGE_ERROR;
        }
//        printf("Timeout wait_ready, last resp=0x%02X\r\n", resp);
        if (resp == 0xFF) return STORAGE_OK;
    } while (GetTick() < timeout);
    return STORAGE_ERROR;
}

// Kirim command ke SD card
static uint8_t sd_send_cmd(SPI_Context *dev, uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t buf[6];
    uint8_t response;
    uint16_t retry = 0xFF;
    if (card_initialized)
    	if (sd_wait_ready(dev) != STORAGE_OK) return 0xFF;
    buf[0] = 0x40 | cmd;
    buf[1] = (uint8_t)(arg >> 24);
    buf[2] = (uint8_t)(arg >> 16);
    buf[3] = (uint8_t)(arg >> 8);
    buf[4] = (uint8_t)(arg);
    buf[5] = crc;

    if (SPI_Transmit(dev, buf, 6) != SPI_OK) return 0xFF;

    // tunggu response
    do {
    	if (SPI_TransmitReceive(dev, dummy, &response, 1) != SPI_OK) return 0xFF;
    } while ((response & 0x80) && --retry);

    return (retry ? response : 0xFF);
}

StorageStatus_t STORAGE_Init_Cmd_Sequence(SPI_Context *dev) {
	uint8_t i, response;
	uint32_t retry,count;
	uint8_t r7[4];

    // Set Speed Low (ClockSpeed/prescaller)
	SPI_SetSpeed(dev, SPI_BAUDRATEPRESCALER_128);

    SPI_Unselect_CS(dev);
    for (int i = 0; i < 10; i++) SPI_Transmit(dev, &dummy, 1);

    // CMD0: reset
    SPI_Select_CS(dev);
    DelayMs(10);
    if (sd_send_cmd(dev, 0, 0, 0x95) != 0x01) {
        SPI_Unselect_CS(dev);
        return STORAGE_ERROR;
    }
    SPI_Unselect_CS(dev);
    SPI_Transmit(dev, &dummy,1);

    // CMD8: check voltage
    SPI_Select_CS(dev);
	response = sd_send_cmd(dev, 8, 0x000001AA, 0x87);
	// baca 4 byte echo-back
	for (i = 0; i < 4; i++)
		if (SPI_TransmitReceive(dev, dummy, &r7[i], 1) != SPI_OK)
			return STORAGE_ERROR;

	SPI_Unselect_CS(dev);
    SPI_Transmit(dev, &dummy,1);
    sdhc = 0;
    retry = GetTick() + 1000;
    if (response == 0x01 && r7[2] == 0x01 && r7[3] == 0xAA) {
            do {
            	SPI_Select_CS(dev);
            	sd_send_cmd(dev, 55, 0, 0xFF);
                response = sd_send_cmd(dev, 41, 0x40000000, 0xFF);
                SPI_Unselect_CS(dev);
                SPI_Transmit(dev, &dummy,1);
                count = GetTick();
            } while (response != 0x00 && count < retry);

            if (response != 0x00) return STORAGE_ERROR;

            SPI_Select_CS(dev);
            response = sd_send_cmd(dev, 58, 0, 0xFF);
            uint8_t ocr[4];
            for (i = 0; i < 4; i++)
        		if (SPI_TransmitReceive(dev, dummy, &ocr[i], 1) != SPI_OK)
        			return STORAGE_ERROR;
            SPI_Unselect_CS(dev);
            if (ocr[0] & 0x40) sdhc = 1;
	} else {
		do {
			SPI_Select_CS(dev);
			sd_send_cmd(dev, 55, 0, 0xFF);
			response = sd_send_cmd(dev, 41, 0, 0xFF);
			SPI_Unselect_CS(dev);
			SPI_Transmit(dev, &dummy,1);
		} while (response != 0x00 && GetTick() < retry);
		if (response != 0x00) return STORAGE_ERROR;
	}

    card_initialized = 1;

    // Set Speed High
	SPI_SetSpeed(dev, SPI_BAUDRATEPRESCALER_4);

	return STORAGE_OK;
}

StorageStatus_t STORAGE_GetStatus(SPI_Context *dev) {
    return card_initialized ? STORAGE_OK : STORAGE_ERROR;
}

StorageStatus_t STORAGE_ReadBlocks(SPI_Context *dev, uint8_t *buff, uint32_t sector, uint32_t count) {

    if (!card_initialized) return STORAGE_ERROR;

    SPI_Select_CS(dev);

    uint32_t addr = sdhc ? sector : sector * SECTOR_SIZE;

    if (count == 1) {
        // --- Single block read (CMD17) ---
        uint8_t res = sd_send_cmd(dev, 17, addr, 0x01);
        if (res != 0x00) {
            SPI_Unselect_CS(dev);
            return STORAGE_ERROR;
        }

        // Tunggu token 0xFE
        uint32_t timeout = GetTick() + 100;
        uint8_t token;
        do {
            if (SPI_TransmitReceive(dev, dummy, &token, 1) != SPI_OK) {
                SPI_Unselect_CS(dev);
                return STORAGE_ERROR;
            }
            if (token == 0xFE) break;
        } while (GetTick() < timeout);

        if (token != 0xFE) {
            SPI_Unselect_CS(dev);
            return STORAGE_ERROR;
        }

        // Baca 512 byte + CRC
        if (dev->mode == SPI_MODE_DMA) {

            if (SPI_TransmitReceiveBuffer(dev, tx_dummy, buff, SECTOR_SIZE) != SPI_OK) {
                SPI_Unselect_CS(dev);
                return STORAGE_ERROR;
            }
            uint8_t crc[2] = {0xFF, 0xFF}, crc_tmp[2];
            SPI_TransmitReceiveBuffer(dev, crc, crc_tmp, 2);
        } else {
            for (int j = 0; j < SECTOR_SIZE; j++) {
                if (SPI_TransmitReceive(dev, dummy, &buff[j], 1) != SPI_OK) {
                    SPI_Unselect_CS(dev);
                    return STORAGE_ERROR;
                }
            }
            uint8_t tmp;
            SPI_TransmitReceive(dev, dummy, &tmp, 1);
            SPI_TransmitReceive(dev, dummy, &tmp, 1);
        }

    } else {
        // --- Multi block read (CMD18) ---
        uint8_t res = sd_send_cmd(dev, 18, addr, 0x01);
        if (res != 0x00) {
            SPI_Unselect_CS(dev);
            return STORAGE_ERROR;
        }

        for (uint32_t i = 0; i < count; i++) {
            // Tunggu token 0xFE
            uint32_t timeout = GetTick() + 100;
            uint8_t token;
            do {
                if (SPI_TransmitReceive(dev, dummy, &token, 1) != SPI_OK) {
                    SPI_Unselect_CS(dev);
                    return STORAGE_ERROR;
                }
                if (token == 0xFE) break;
            } while (GetTick() < timeout);

            if (token != 0xFE) {
                SPI_Unselect_CS(dev);
                return STORAGE_ERROR;
            }

            // Baca 512 byte + CRC
            if (dev->mode == SPI_MODE_DMA) {
                if (SPI_TransmitReceiveBuffer(dev, tx_dummy, &buff[i * SECTOR_SIZE], SECTOR_SIZE) != SPI_OK) {
                    SPI_Unselect_CS(dev);
                    return STORAGE_ERROR;
                }
                uint8_t crc[2] = {0xFF, 0xFF}, crc_tmp[2];
                SPI_TransmitReceiveBuffer(dev, crc, crc_tmp, 2);
            } else {
                for (int j = 0; j < SECTOR_SIZE; j++) {
                    if (SPI_TransmitReceive(dev, dummy, &buff[i * SECTOR_SIZE + j], 1) != SPI_OK) {
                        SPI_Unselect_CS(dev);
                        return STORAGE_ERROR;
                    }
                }
                uint8_t tmp;
                SPI_TransmitReceive(dev, dummy, &tmp, 1);
                SPI_TransmitReceive(dev, dummy, &tmp, 1);
            }
        }

        // STOP_TRANSMISSION (CMD12)
        sd_send_cmd(dev, 12, 0, 0x01);
    }

    // Lepas CS
    SPI_Unselect_CS(dev);

    // Kirim dummy clock agar card idle
    uint8_t extra[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    SPI_TransmitBuffer(dev, extra, sizeof(extra));

    return STORAGE_OK;
}

StorageStatus_t STORAGE_WriteBlocks(SPI_Context *dev, const uint8_t *buff, uint32_t sector, uint32_t count) {
    static uint8_t rx_dummy[SECTOR_SIZE]; // dummy RX buffer
    if (!card_initialized) return STORAGE_ERROR;

    SPI_Select_CS(dev);

    if (count == 1) {
        // --- Single block write (CMD24) ---
        uint32_t addr = sdhc ? sector : sector * SECTOR_SIZE;
        uint8_t res = sd_send_cmd(dev, 24, addr, 0x01);
        if (res != 0x00) { SPI_Unselect_CS(dev); return STORAGE_ERROR; }

        // Start token 0xFE
        uint8_t tmp;
        SPI_TransmitReceive(dev, 0xFE, &tmp, 1);

        // Kirim 512 byte data
        if (dev->mode == SPI_MODE_DMA) {
            if (SPI_TransmitReceiveBuffer(dev, &buff[0], rx_dummy, SECTOR_SIZE) != SPI_OK) {
                SPI_Unselect_CS(dev); return STORAGE_ERROR;
            }
        } else {
            for (int j = 0; j < SECTOR_SIZE; j++) {
                SPI_TransmitReceive(dev, buff[j], &tmp, 1);
            }
        }

        // Dummy CRC
        SPI_TransmitReceive(dev, 0xFF, &tmp, 1);
        SPI_TransmitReceive(dev, 0xFF, &tmp, 1);

        // Data response
        SPI_TransmitReceive(dev, 0xFF, &res, 1);
        if ((res & 0x1F) != 0x05) { SPI_Unselect_CS(dev); return STORAGE_ERROR; }

        // Tunggu busy selesai
        uint32_t timeout = GetTick() + 500;
        do {
            SPI_TransmitReceive(dev, 0xFF, &tmp, 1);
            if (tmp != 0x00) break;
        } while (GetTick() < timeout);
        if (tmp != 0xFF) { SPI_Unselect_CS(dev); return STORAGE_ERROR; }

    } else {
        // --- Multi block write (CMD25) ---
        uint32_t addr = sdhc ? sector : sector * SECTOR_SIZE;
        uint8_t res = sd_send_cmd(dev, 25, addr, 0x01);
        if (res != 0x00) { SPI_Unselect_CS(dev); return STORAGE_ERROR; }

        for (uint32_t i = 0; i < count; i++) {
            uint8_t tmp;
            // Start token 0xFC
            SPI_TransmitReceive(dev, 0xFC, &tmp, 1);

            // Kirim 512 byte data
            if (dev->mode == SPI_MODE_DMA) {
                if (SPI_TransmitReceiveBuffer(dev, &buff[i * SECTOR_SIZE], rx_dummy, SECTOR_SIZE) != SPI_OK) {
                    SPI_Unselect_CS(dev); return STORAGE_ERROR;
                }
            } else {
                for (int j = 0; j < SECTOR_SIZE; j++) {
                    SPI_TransmitReceive(dev, buff[i * SECTOR_SIZE + j], &tmp, 1);
                }
            }

            // Dummy CRC
            SPI_TransmitReceive(dev, 0xFF, &tmp, 1);
            SPI_TransmitReceive(dev, 0xFF, &tmp, 1);

            // Data response
            SPI_TransmitReceive(dev, 0xFF, &res, 1);
            if ((res & 0x1F) != 0x05) { SPI_Unselect_CS(dev); return STORAGE_ERROR; }

            // Tunggu busy selesai
            uint32_t timeout = GetTick() + 500;
            do {
                SPI_TransmitReceive(dev, 0xFF, &tmp, 1);
                if (tmp != 0x00) break;
            } while (GetTick() < timeout);
            if (tmp != 0xFF) { SPI_Unselect_CS(dev); return STORAGE_ERROR; }
        }

        // STOP_TRAN token 0xFD
        uint8_t tmp;
        SPI_TransmitReceive(dev, 0xFD, &tmp, 1);

        // Tunggu busy selesai
        uint32_t timeout = GetTick() + 500;
        do {
            SPI_TransmitReceive(dev, 0xFF, &tmp, 1);
            if (tmp != 0x00) break;
        } while (GetTick() < timeout);
        if (tmp != 0xFF) { SPI_Unselect_CS(dev); return STORAGE_ERROR; }
    }

    SPI_Unselect_CS(dev);

    // Dummy clock agar card idle
    uint8_t extra[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    SPI_TransmitBuffer(dev, extra, sizeof(extra));

    return STORAGE_OK;
}


uint32_t STORAGE_GetSectorCount(SPI_Context *dev) {
    return sector_count; // bisa di-update dari parsing CSD
}

// Baca CID (CMD10)
StorageStatus_t STORAGE_ReadCID(SPI_Context *dev, uint8_t *cid) {
    uint8_t resp;

    SPI_Select_CS(dev);

    // Kirim CMD10 (READ_CID)
    resp = sd_send_cmd(dev, 10, 0, 0x01); // CRC dummy 0x01
    if (resp != 0x00) {
        SPI_Unselect_CS(dev);
        return STORAGE_ERROR; // CMD gagal atau respon tidak valid
    }

    // Tunggu start token 0xFE
    uint32_t timeout = GetTick() + 100;
    uint8_t token;
    do {
        if (SPI_TransmitReceive(dev, dummy, &token, 1) != SPI_OK) {
            SPI_Unselect_CS(dev);
            return STORAGE_ERROR;
        }
        if (token == 0xFE) break;
    } while (GetTick() < timeout);

    if (token != 0xFE) {
        SPI_Unselect_CS(dev);
        return STORAGE_ERROR; // timeout
    }

    // Baca 16 byte data CID
    for (int i = 0; i < 16; i++) {
        if (SPI_TransmitReceive(dev, dummy, &cid[i], 1) != SPI_OK) {
            SPI_Unselect_CS(dev);
            return STORAGE_ERROR;
        }
    }

    // Buang 2 byte CRC
    uint8_t tmp;
    SPI_TransmitReceive(dev, dummy, &tmp, 1);
    SPI_TransmitReceive(dev, dummy, &tmp, 1);

    SPI_Unselect_CS(dev);
    SPI_Transmit(dev, &dummy, 1); // dummy clock

    return STORAGE_OK;
}

// Baca CSD (CMD9)
StorageStatus_t STORAGE_ReadCSD(SPI_Context *dev, uint8_t *csd) {
    uint8_t resp;

    SPI_Select_CS( dev);

    // Kirim CMD9 (READ_CSD)
    resp = sd_send_cmd(dev, 9, 0, 0x01); // CRC dummy 0x01
    if (resp != 0x00) {
        SPI_Unselect_CS(dev);
        return STORAGE_ERROR; // CMD gagal atau respon tidak valid
    }

    // Tunggu start token 0xFE
    uint32_t timeout = GetTick() + 100;
    uint8_t token;
    do {
        if (SPI_TransmitReceive(dev, dummy, &token, 1) != SPI_OK) {
            SPI_Unselect_CS(dev);
            return STORAGE_ERROR;
        }
        if (token == 0xFE) break;
    } while (GetTick() < timeout);

    if (token != 0xFE) {
        SPI_Unselect_CS(dev);
        return STORAGE_ERROR; // timeout
    }

    // Baca 16 byte data CSD
    for (int i = 0; i < 16; i++) {
        if (SPI_TransmitReceive(dev, dummy, &csd[i], 1) != SPI_OK) {
            SPI_Unselect_CS(dev);
            return STORAGE_ERROR;
        }
    }

    // Buang 2 byte CRC
    uint8_t tmp;
    SPI_TransmitReceive(dev, dummy, &tmp, 1);
    SPI_TransmitReceive(dev, dummy, &tmp, 1);

    SPI_Unselect_CS(dev);
    SPI_Transmit(dev, &dummy, 1); // dummy clock

    return STORAGE_OK;
}


// Hitung kapasitas dari CSD
uint32_t STORAGE_CardSize(SPI_Context *dev) {
    uint8_t csd[18]; // 16 byte data + 2 byte CRC
    if (STORAGE_ReadCSD(dev, csd) != STORAGE_OK) return 0;

    uint32_t card_capacity = 0;

    // CSD structure version ada di bit [127:126] → csd[0] >> 6
    if ((csd[0] >> 6) == 1) {
        // CSD v2.0 (SDHC/SDXC)
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) |
                          ((uint32_t)csd[8] << 8) |
                          (uint32_t)csd[9];
        // Kapasitas = (C_SIZE+1) * 512KB
        card_capacity = (c_size + 1) * 512UL * 1024UL; // kapasitas dalam byte
    } else {
        // CSD v1.0 (SDSC)
        uint32_t c_size = ((csd[6] & 0x03) << 10) |
                          ((uint32_t)csd[7] << 2) |
                          ((csd[8] & 0xC0) >> 6);
        uint32_t c_size_mult = ((csd[9] & 0x03) << 1) |
                               ((csd[10] & 0x80) >> 7);
        uint32_t read_bl_len = csd[5] & 0x0F;

        // Kapasitas = (C_SIZE+1) * 2^(C_SIZE_MULT+2) * 2^READ_BL_LEN
        card_capacity = (c_size + 1) *
                        (1UL << (c_size_mult + 2)) *
                        (1UL << read_bl_len);
    }

    return card_capacity; // kapasitas dalam byte
}

uint32_t STORAGE_GetCapacity(SPI_Context *dev) {
    uint8_t csd[18]; // 16 byte data + 2 CRC
    if (STORAGE_ReadCSD(dev, csd) != STORAGE_OK) return 0;

    uint8_t csd_structure = (csd[0] >> 6) & 0x03;
    uint32_t capacity = 0;

    if (csd_structure == 0) {
        // SDSC (CSD v1.0)
        uint32_t c_size = ((csd[6] & 0x03) << 10) |
                          ((uint32_t)csd[7] << 2) |
                          ((csd[8] & 0xC0) >> 6);

        uint8_t c_size_mult = ((csd[9] & 0x03) << 1) |
                              ((csd[10] & 0x80) >> 7);

        uint8_t read_bl_len = csd[5] & 0x0F;

        uint32_t block_len = 1UL << read_bl_len;
        uint32_t mult = 1UL << (c_size_mult + 2);

        // Kapasitas dalam jumlah sektor 512 byte
        capacity = ((c_size + 1) * mult * block_len) / SECTOR_SIZE;
    } else if (csd_structure == 1) {
        // SDHC/SDXC (CSD v2.0)
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) |
                          ((uint32_t)csd[8] << 8) |
                          (uint32_t)csd[9];

        // Kapasitas dalam jumlah sektor 512 byte
        capacity = (c_size + 1) * 1024UL;
    }
    return capacity; // jumlah sektor 512 byte
}

uint64_t STORAGE_GetSizeBytes(SPI_Context *dev) {
    uint32_t sectors = STORAGE_GetCapacity(dev);
    return (uint64_t)sectors * SECTOR_SIZE; // kapasitas dalam byte
}

void print_card_size(SPI_Context *dev, uint64_t size_bytes) {
    // Hitung kapasitas dalam MB dan GB
    uint32_t size_mb = (uint32_t)(size_bytes / (1024ULL * 1024ULL));
    uint32_t size_gb = (uint32_t)(size_bytes / (1024ULL * 1024ULL * 1024ULL));

    // Hitung pecahan GB (2 digit desimal)
    uint64_t remainder = size_bytes % (1024ULL * 1024ULL * 1024ULL);
    uint32_t fraction = (uint32_t)((remainder * 100) / (1024ULL * 1024ULL * 1024ULL));

    // Cetak hasil
    printf("Card Size    : %lu MB (%lu.%02lu GB)\n",
           (unsigned long)size_mb,
           (unsigned long)size_gb,
           (unsigned long)fraction);
}

bool STORAGE_IsCardPresent(SPI_Context *dev) {
    // Membaca dari pin gpio jika sdcard mendukung pin IsCardPresent
    //return (HAL_GPIO_ReadPin(SD_CP_GPIO_Port, SD_CP_Pin) == GPIO_PIN_SET);
    return card_present;
}

bool STORAGE_IsWriteProtected(SPI_Context *dev) {
    // Misalnya pin WP aktif high
    //return (HAL_GPIO_ReadPin(SD_WP_GPIO_Port, SD_WP_Pin) == GPIO_PIN_SET);
    return false;
}

void STORAGE_Deinit(SPI_Context *dev) {
    // optional: matikan SPI, release resource
//	if (countFailure==3) NVIC_SystemReset();
    card_present = true;  // ubah ke false jika menggunakan sdcard dengan PIN Card Present
    card_initialized = 0;
    sdhc = 0;
//    countFailure+=1;
}


