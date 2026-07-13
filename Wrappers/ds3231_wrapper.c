/*
 * ds3231_wrapper.c
 *
 *  Created on: 5 May 2026
 *      Author: ferry
 */

#include <ds3231_wrapper.h>
#include "utils.h"   // bcd2dec, dec2bcd
#include <stdio.h>

// Register map
#define DS3231_REG_SECONDS 0x00
#define DS3231_REG_MINUTES 0x01
#define DS3231_REG_HOURS   0x02
#define DS3231_REG_DAY     0x03
#define DS3231_REG_DATE    0x04
#define DS3231_REG_MONTH   0x05
#define DS3231_REG_YEAR    0x06

void DS3231_Init(I2C_RTCDevice *dev, I2C_Context *ctx) {
	dev->ctx = ctx;
    dev->address = DS3231_ADDR;   // alamat DS3231 (0x68 << 1)
    dev->sizereg = I2C_MEMADD_SIZE_8BIT; // I2C_MEMADD_SIZE_16BIT ~ EEPROM
    dev->mode = I2C_MODE_IT;
}

bool DS3231_TestConnection(I2C_RTCDevice *dev) {
    uint8_t buf;
    return (I2C_MemRead(dev->ctx, dev->address, dev->mode, DS3231_REG_SECONDS, dev->sizereg, &buf, 1, 50) == I2C_OK);
}

DS3231_Time DS3231_GetTime(I2C_RTCDevice *dev) {
    uint8_t buf[3];
    DS3231_Time t;

    if (I2C_MemRead(dev->ctx, dev->address, dev->mode, DS3231_REG_SECONDS, dev->sizereg, buf, 3, 50) != I2C_OK) {
        t.hours = 12; t.minutes = 0; t.seconds = 0;
        return t;
    }

    t.seconds = bcd2dec(buf[0]);
    t.minutes = bcd2dec(buf[1]);
    t.hours   = bcd2dec(buf[2] & 0x3F);
    return t;
}

DS3231_Date DS3231_GetDate(I2C_RTCDevice *dev) {
    uint8_t buf[4];
    DS3231_Date d;

    if (I2C_MemRead(dev->ctx, dev->address, dev->mode, DS3231_REG_DAY, dev->sizereg, buf, 4, 50) != I2C_OK) {
        d.year = 2026; d.month = 1; d.day = 1; d.dayOfWeek = 1;
        return d;
    }

    d.dayOfWeek = bcd2dec(buf[0]);
    d.day       = bcd2dec(buf[1]);
    d.month     = bcd2dec(buf[2] & 0x1F);
    d.year      = 2000 + bcd2dec(buf[3]);
    return d;
}

DS3231_DateTime DS3231_GetDateTime(I2C_RTCDevice *dev) {
    DS3231_DateTime dt;

    if (!DS3231_TestConnection(dev)) {
        dt.time.hours = 12; dt.time.minutes = 0; dt.time.seconds = 0;
        dt.date.year = 2026; dt.date.month = 1; dt.date.day = 1; dt.date.dayOfWeek = 1;
        printf("LoggerTask: DS3231 not connected, using dummy time\r\n");
        return dt;
    }

    dt.time = DS3231_GetTime(dev);
    dt.date = DS3231_GetDate(dev);
    return dt;
}

void DS3231_SetTime(I2C_RTCDevice *dev, DS3231_Time t) {
    uint8_t buf[3];
    buf[0] = dec2bcd(t.seconds);
    buf[1] = dec2bcd(t.minutes);
    buf[2] = dec2bcd(t.hours);
    I2C_MemWrite(dev->ctx, dev->address, dev->mode, DS3231_REG_SECONDS, dev->sizereg, buf, 3, 50);
}

void DS3231_SetDate(I2C_RTCDevice *dev, DS3231_Date d) {
    uint8_t buf[4];
    buf[0] = dec2bcd(d.dayOfWeek);
    buf[1] = dec2bcd(d.day);
    buf[2] = dec2bcd(d.month);
    buf[3] = dec2bcd(d.year - 2000);
    I2C_MemWrite(dev->ctx, dev->address, dev->mode, DS3231_REG_DAY, dev->sizereg, buf, 4, 50);
}

void DS3231_SetDateTime(I2C_RTCDevice *dev, DS3231_DateTime dt) {
    uint8_t buf[7];
    buf[0] = dec2bcd(dt.time.seconds);
    buf[1] = dec2bcd(dt.time.minutes);
    buf[2] = dec2bcd(dt.time.hours);
    buf[3] = dec2bcd(dt.date.dayOfWeek);
    buf[4] = dec2bcd(dt.date.day);
    buf[5] = dec2bcd(dt.date.month);
    buf[6] = dec2bcd(dt.date.year - 2000);
    I2C_MemWrite(dev->ctx, dev->address, dev->mode, DS3231_REG_SECONDS, dev->sizereg, buf, 7, 50);
}

