/**
 * @file    ds3231_wrapper.c
 * @brief   Implementasi Driver RTC DS3231 Berbasis Context, Burst Read/Write, dan Manajemen Alarm.
 *
 *  Created on: 5 May 2026
 *      Author: ferry
 */

#include "ds3231_wrapper.h"

// --- Helper Functions Konversi BCD (Binary Coded Decimal) ---
static uint8_t bcdToDec(uint8_t val) {
    return ((val / 16 * 10) + (val % 16));
}

static uint8_t decToBcd(uint8_t val) {
    return ((val / 10 * 16) + (val % 10));
}

/**
 * @brief Inisialisasi Objek Perangkat DS3231.
 */
bool DS3231_Init(DS3231_Device_t *dev, I2C_Context *i2c, uint16_t addr) {
    if (dev == NULL || i2c == NULL) return false;

    dev->i2c_ctx = i2c;
    dev->dev_address = addr;

    return true;
}

/**
 * @brief  Membaca Waktu & Tanggal Lengkap (DateTime) secara Burst Read (7 Byte).
 * @note   MENGAPA HARUS BURST READ 7 BYTE SEKALIGUS?
 * Untuk mencegah "Midnight Rollover Bug". Jika Jam & Tanggal dibaca dalam dua transaksi I2C
 * terpisah, ada potensi waktu berdetak melewati 23:59:59 di antara dua transaksi tersebut,
 * menyebabkan kombinasi Jam baru + Tanggal lama (Data Tearing).
 */
bool DS3231_GetDateTime(DS3231_Device_t *dev, DS3231_DateTime_t *dt) {
    if (dev == NULL || dev->i2c_ctx == NULL || dt == NULL) return false;

    uint8_t buffer[7];

    // Transaksi I2C MemRead melalui Layer 1 (Dilindungi Mutex I2C Bus)
    I2C_Status status = I2C_MemRead(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                                    DS3231_REG_TIME, I2C_MEMADD_SIZE_8BIT,
                                    buffer, 7, 1000);

    if (status != I2C_OK) return false;

    // Dekode BCD
    dt->time.seconds     = bcdToDec(buffer[0] & 0x7F);
    dt->time.minutes     = bcdToDec(buffer[1] & 0x7F);
    dt->time.hours       = bcdToDec(buffer[2] & 0x3F); // Format 24-Jam
    dt->date.day_of_week = bcdToDec(buffer[3] & 0x07);
    dt->date.date        = bcdToDec(buffer[4] & 0x3F);
    dt->date.month       = bcdToDec(buffer[5] & 0x1F);
    dt->date.year        = 2000 + bcdToDec(buffer[6]);

    return true;
}

/**
 * @brief  Mengatur Waktu & Tanggal Lengkap (DateTime) secara Burst Write (7 Byte).
 */
bool DS3231_SetDateTime(DS3231_Device_t *dev, const DS3231_DateTime_t *dt) {
    if (dev == NULL || dev->i2c_ctx == NULL || dt == NULL) return false;

    uint8_t buffer[7];

    buffer[0] = decToBcd(dt->time.seconds);
    buffer[1] = decToBcd(dt->time.minutes);
    buffer[2] = decToBcd(dt->time.hours);
    buffer[3] = decToBcd(dt->date.day_of_week);
    buffer[4] = decToBcd(dt->date.date);
    buffer[5] = decToBcd(dt->date.month);
    buffer[6] = decToBcd((uint8_t)(dt->date.year % 100));

    I2C_Status status = I2C_MemWrite(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                                     DS3231_REG_TIME, I2C_MEMADD_SIZE_8BIT,
                                     buffer, 7, 1000);

    return (status == I2C_OK);
}

/**
 * @brief  Membaca RTC dan mengkalkulasi Unix Epoch Time (Detik sejak 1 Jan 1970).
 * @note   Menggunakan kalkulasi O(1) dengan array hari. Mendukung kompensasi tahun kabisat (Leap Year).
 * @param  dev Pointer ke objek DS3231.
 * @retval 32-bit integer (Total detik), atau 0 jika pembacaan I2C gagal.
 */
uint32_t DS3231_GetEpochTime(DS3231_Device_t *dev) {
    if (dev == NULL || dev->i2c_ctx == NULL) return 0;

    DS3231_DateTime_t dt;

    /* Lakukan Burst Read dari Chip RTC (Mencegah Midnight Rollover Bug) */
    if (!DS3231_GetDateTime(dev, &dt)) {
        return 0; // Kembalikan 0 sebagai tanda error/kegagalan bus I2C
    }

    /* Proteksi Kewarasan (Sanity Check) */
    if (dt.date.year < 1970 || dt.date.month < 1 || dt.date.month > 12 ||
        dt.date.date < 1 || dt.date.date > 31) {
        return 0;
    }

    /* Tabel konstan (Disimpan di ROM/Flash) untuk mempercepat perhitungan hari */
    static const uint16_t days_before_month[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    /* 1. Kalkulasi offset tahun dari 1970 */
    uint32_t y = (uint32_t)(dt.date.year - 1970);

    /* 2. Hitung jumlah tahun kabisat yang telah berlalu */
    uint32_t leap_years = (y + 1) / 4;

    /* 3. Hitung total hari dari komponen Tahun, Bulan, dan Tanggal */
    uint32_t total_days = (y * 365) + leap_years + days_before_month[dt.date.month - 1] + (uint32_t)(dt.date.date - 1);

    /* 4. Koreksi ekstra untuk hari di bulan Februari pada tahun kabisat yang SEDANG berjalan */
    if ((dt.date.year % 4 == 0) && (dt.date.month > 2)) {
        total_days += 1;
    }

    /* 5. Konversi semuanya ke dalam detik */
    uint32_t epoch_seconds = (total_days * 86400UL) +
                             ((uint32_t)dt.time.hours * 3600UL) +
                             ((uint32_t)dt.time.minutes * 60UL) +
                             (uint32_t)dt.time.seconds;

    return epoch_seconds;
}


void DS3231_EpochToDateTime(uint32_t epoch, DS3231_DateTime_t *dt) {
    if (dt == NULL) return;

    uint32_t sec  = epoch % 60; epoch /= 60;
    uint32_t min  = epoch % 60; epoch /= 60;
    uint32_t hour = epoch % 24; epoch /= 24;

    uint32_t days = epoch;
    uint32_t year = 1970;

    while (1) {
        bool is_leap = (year % 4 == 0); // Aturan kabisat sederhana (valid untuk 1970-2099)
        uint32_t days_in_year = is_leap ? 366 : 365;
        if (days < days_in_year) break;
        days -= days_in_year;
        year++;
    }

    bool is_leap = (year % 4 == 0);
    static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t month = 1;

    for (uint8_t i = 0; i < 12; i++) {
        uint8_t dim = days_in_month[i];
        if (i == 1 && is_leap) dim = 29;
        if (days < dim) {
            month = i + 1;
            break;
        }
        days -= dim;
    }
    uint8_t day = (uint8_t)days + 1;

    dt->time.seconds = sec;
    dt->time.minutes = min;
    dt->time.hours   = hour;
    dt->date.date    = day;
    dt->date.month   = month;
    dt->date.year    = year;
    dt->date.day_of_week = 0; // Abaikan hari dalam seminggu untuk keperluan Alarm
}

/**
 * @brief Membaca Jam saja (Hours, Minutes, Seconds).
 */
bool DS3231_GetTime(DS3231_Device_t *dev, DS3231_Time_t *t) {
    if (dev == NULL || dev->i2c_ctx == NULL || t == NULL) return false;

    uint8_t buffer[3];
    I2C_Status status = I2C_MemRead(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                                    DS3231_REG_TIME, I2C_MEMADD_SIZE_8BIT,
                                    buffer, 3, 1000);

    if (status != I2C_OK) return false;

    t->seconds = bcdToDec(buffer[0] & 0x7F);
    t->minutes = bcdToDec(buffer[1] & 0x7F);
    t->hours   = bcdToDec(buffer[2] & 0x3F);

    return true;
}

/**
 * @brief Mengatur Jam saja (Hours, Minutes, Seconds).
 */
bool DS3231_SetTime(DS3231_Device_t *dev, const DS3231_Time_t *t) {
    if (dev == NULL || dev->i2c_ctx == NULL || t == NULL) return false;

    uint8_t buffer[3];
    buffer[0] = decToBcd(t->seconds);
    buffer[1] = decToBcd(t->minutes);
    buffer[2] = decToBcd(t->hours);

    I2C_Status status = I2C_MemWrite(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                                     DS3231_REG_TIME, I2C_MEMADD_SIZE_8BIT,
                                     buffer, 3, 1000);

    return (status == I2C_OK);
}

/**
 * @brief Membaca Tanggal saja (DayOfWeek, Date, Month, Year).
 */
bool DS3231_GetDate(DS3231_Device_t *dev, DS3231_Date_t *d) {
    if (dev == NULL || dev->i2c_ctx == NULL || d == NULL) return false;

    uint8_t buffer[4];
    // Register 0x03 adalah Day of Week, 0x04 Date, 0x05 Month, 0x06 Year
    I2C_Status status = I2C_MemRead(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                                    0x03, I2C_MEMADD_SIZE_8BIT,
                                    buffer, 4, 1000);

    if (status != I2C_OK) return false;

    d->day_of_week = bcdToDec(buffer[0] & 0x07);
    d->date        = bcdToDec(buffer[1] & 0x3F);
    d->month       = bcdToDec(buffer[2] & 0x1F);
    d->year        = 2000 + bcdToDec(buffer[3]);

    return true;
}

/**
 * @brief Mengatur Tanggal saja (DayOfWeek, Date, Month, Year).
 */
bool DS3231_SetDate(DS3231_Device_t *dev, const DS3231_Date_t *d) {
    if (dev == NULL || dev->i2c_ctx == NULL || d == NULL) return false;

    uint8_t buffer[4];
    buffer[0] = decToBcd(d->day_of_week);
    buffer[1] = decToBcd(d->date);
    buffer[2] = decToBcd(d->month);
    buffer[3] = decToBcd((uint8_t)(d->year % 100));

    I2C_Status status = I2C_MemWrite(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                                     0x03, I2C_MEMADD_SIZE_8BIT,
                                     buffer, 4, 1000);

    return (status == I2C_OK);
}

/**
 * @brief  Mengatur Alarm 1 DS3231 untuk Pemicu Interrupt EXTI Wake-up MCU.
 * @param  dev   Pointer ke objek DS3231_Device_t.
 * @param  dt    Target Waktu Alarm berbunyi.
 * @param  mode  Mode Pencocokan Alarm (Harian, Detik, dll).
 * @note   MENGAPA FUNGSI INI KRUSIAL?
 * Saat Smart Garden masuk ke FASE DEEP SLEEP (STOP Mode) untuk menghemat listrik,
 * STM32 mematikan clock utamanya. Alarm 1 inilah yang akan menarik jalur INT/SQW menjadi LOW,
 * memicu EXTI0 pada PA0 STM32 untuk membangunkan FSM tepat pada jam penyiraman/pemupukan!
 */
bool DS3231_SetAlarm1(DS3231_Device_t *dev, const DS3231_DateTime_t *dt, DS3231_Alarm1Mode_t mode) {
    if (dev == NULL || dev->i2c_ctx == NULL || dt == NULL) return false;

    uint8_t buffer[4];

    // Format BCD untuk register alarm (Bit 7 adalah bit A1M1..A1M4 untuk mask mode)
    buffer[0] = decToBcd(dt->time.seconds);
    buffer[1] = decToBcd(dt->time.minutes);
    buffer[2] = decToBcd(dt->time.hours);
    buffer[3] = decToBcd(dt->date.date);

    // Atur mask bit A1M1, A1M2, A1M3, A1M4 sesuai mode yang dipilih
    switch (mode) {
        case DS3231_ALARM1_EVERY_SECOND:
            buffer[0] |= 0x80; buffer[1] |= 0x80; buffer[2] |= 0x80; buffer[3] |= 0x80;
            break;
        case DS3231_ALARM1_MATCH_SEC:
            buffer[1] |= 0x80; buffer[2] |= 0x80; buffer[3] |= 0x80;
            break;
        case DS3231_ALARM1_MATCH_MIN_SEC:
            buffer[2] |= 0x80; buffer[3] |= 0x80;
            break;
        case DS3231_ALARM1_MATCH_HOURS_MIN_SEC: // Mode Paling Sering Digunakan (Jadwal Harian)
            buffer[3] |= 0x80;
            break;
        case DS3231_ALARM1_MATCH_DATE_HOURS_MIN_SEC:
            // Semua mask bit = 0 (Match Tanggal + Jam + Menit + Detik)
            break;
    }

    // Tulis ke Register Alarm 1 (0x07 - 0x0A)
    if (I2C_MemWrite(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                     DS3231_REG_ALARM1, I2C_MEMADD_SIZE_8BIT, buffer, 4, 1000) != I2C_OK) {
        return false;
    }

    // Aktifkan Interrupt Alarm 1 di Register Control
    return DS3231_EnableAlarm1Interrupt(dev, true);
}

/**
 * @brief  Mengaktifkan/Mematikan Output Sinyal Interrupt Alarm 1 pada Pin INT/SQW.
 */
bool DS3231_EnableAlarm1Interrupt(DS3231_Device_t *dev, bool enable) {
    if (dev == NULL || dev->i2c_ctx == NULL) return false;

    uint8_t control_reg = 0;

    // Baca Register Control (0x0E)
    if (I2C_MemRead(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                    DS3231_REG_CONTROL, I2C_MEMADD_SIZE_8BIT, &control_reg, 1, 1000) != I2C_OK) {
        return false;
    }

    // Bit 2 (INTCN) = 1 (Gunakan Pin INT/SQW sebagai Interrupt), Bit 0 (A1IE) = Alarm 1 Enable
    control_reg |= (1 << 2); // Pastikan INTCN aktif

    if (enable) {
        control_reg |= (1 << 0);  // Set A1IE
    } else {
        control_reg &= ~(1 << 0); // Clear A1IE
    }

    return (I2C_MemWrite(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                         DS3231_REG_CONTROL, I2C_MEMADD_SIZE_8BIT, &control_reg, 1, 1000) == I2C_OK);
}

/**
 * @brief  Menghapus Flag Alarm 1 (Clear A1F).
 * @note   WAJIB dipanggil setelah FSM bangun dari Sleep agar pin INT/SQW kembali HIGH
 * dan siap menerima pemicu interupsi alarm berikutnya!
 */
bool DS3231_ClearAlarm1Flag(DS3231_Device_t *dev) {
    if (dev == NULL || dev->i2c_ctx == NULL) return false;

    uint8_t status_reg = 0;

    if (I2C_MemRead(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                    DS3231_REG_STATUS, I2C_MEMADD_SIZE_8BIT, &status_reg, 1, 1000) != I2C_OK) {
        return false;
    }

    status_reg &= ~(0x01); // Clear Bit 0 (A1F Flag)

    return (I2C_MemWrite(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                         DS3231_REG_STATUS, I2C_MEMADD_SIZE_8BIT, &status_reg, 1, 1000) == I2C_OK);
}

/**
 * @brief Membaca sensor suhu internal DS3231 (Presisi ±3°C).
 */
bool DS3231_GetTemperature(DS3231_Device_t *dev, float *temp) {
    if (dev == NULL || dev->i2c_ctx == NULL || temp == NULL) return false;

    uint8_t buffer[2];

    I2C_Status status = I2C_MemRead(dev->i2c_ctx, dev->dev_address, I2C_MODE_BLOCKING,
                                    DS3231_REG_TEMP_MSB, I2C_MEMADD_SIZE_8BIT,
                                    buffer, 2, 1000);

    if (status != I2C_OK) return false;

    int8_t integer_part = (int8_t)buffer[0];
    uint8_t fractional_part = buffer[1] >> 6;

    *temp = (float)integer_part + (fractional_part * 0.25f);
    return true;
}
