/*
 * ds3231_wrapper.h
 *
 *  Created on: 5 May 2026
 *      Author: ferry
 */

#ifndef DS3231_H_
#define DS3231_H_

#include <stdint.h>
#include <stdbool.h>
#include "i2c_wrapper.h"   // gunakan wrapper, bukan langsung HAL

#define DS3231_ADDR  (0x68 << 1)   // 0xD0

typedef struct {
	I2C_Context *ctx;
    uint16_t address;
    uint32_t sizereg;
    I2C_Mode mode;
} I2C_RTCDevice;

typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} DS3231_Time;

typedef struct {
    uint8_t dayOfWeek;   // 1–7
    uint8_t day;         // 1–31
    uint8_t month;       // 1–12
    uint16_t year;       // 2000–2099
} DS3231_Date;

typedef struct {
    DS3231_Time time;
    DS3231_Date date;
} DS3231_DateTime;


extern I2C_RTCDevice DS3231_Ctx;
// API
void DS3231_Init(I2C_RTCDevice *dev, I2C_Context *ctx);
bool DS3231_TestConnection(I2C_RTCDevice *dev);
DS3231_Time DS3231_GetTime(I2C_RTCDevice *dev);
DS3231_Date DS3231_GetDate(I2C_RTCDevice *dev);
DS3231_DateTime DS3231_GetDateTime(I2C_RTCDevice *dev);

void DS3231_SetTime(I2C_RTCDevice *dev, DS3231_Time t);
void DS3231_SetDate(I2C_RTCDevice *dev, DS3231_Date d);
void DS3231_SetDateTime(I2C_RTCDevice *dev, DS3231_DateTime dt);

#endif /* DS3231_H_ */
