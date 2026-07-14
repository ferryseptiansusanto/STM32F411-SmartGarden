/*
 * config_data.h
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */

#ifndef CONFIG_CONFIG_DATA_H_
#define CONFIG_CONFIG_DATA_H_

#include <stdint.h>
#include <stdbool.h>

// -----------------------------------------------------------------
// Kapasitas maksimum jadwal (compile-time, sesuaikan dengan kapasitas EEPROM)
// -----------------------------------------------------------------
#define MAX_IRRIGATION_SCHEDULE     10
#define MAX_FERTILIZER_SCHEDULE     20

#define CONFIG_MAGIC_NUMBER         0xC0FFEE01u
#define CONFIG_STRUCT_VERSION       1u

// -----------------------------------------------------------------
// Jadwal irigasi rutin
// -----------------------------------------------------------------
typedef struct {
    uint8_t  hour;                 // 0-23
    uint8_t  minute;                // 0-59
    uint8_t  day_mask;              // bitmask hari aktif: bit0=Senin ... bit6=Minggu
    uint32_t watering_volume_ml;    // target volume air (ml)
    bool     enabled;
} IrrigationSchedule_t;

// -----------------------------------------------------------------
// Jadwal pemupukan
// -----------------------------------------------------------------
typedef struct {
    uint8_t  day;                       // 1-31, 0 = tidak dipakai (pakai day_mask saja)
    uint8_t  month;                     // 1-12, 0 = berulang tiap bulan
    uint16_t year;                      // 0 = berulang tiap tahun
    uint8_t  day_mask;                  // opsional: jadwal mingguan, sama seperti irigasi
    uint8_t  fertilizer_type;           // 1..5 -> mapping ke VALVE_PUPUK_1..5
    uint32_t fertilizer_volume_ml;      // dosis pupuk (ml)
    uint32_t water_volume_ml;           // volume air pengencer (ml)
    bool     enabled;
} FertilizationSchedule_t;

// -----------------------------------------------------------------
// Parameter sistem (durasi, kalibrasi, timeout)
// -----------------------------------------------------------------
typedef struct {
    uint32_t mixing_duration_ms;
    uint32_t valve_output_duration_ms;

    int32_t  flow_calibration_offset_1;   // kalibrasi flowmeter 1
    int32_t  flow_calibration_offset_2;   // kalibrasi flowmeter 2
    int32_t  flow_calibration_offset_3;   // kalibrasi flowmeter 3
    uint32_t irrigation_timeout_ms;          // safety timeout siklus irigasi
    uint32_t fertilization_timeout_ms;       // safety timeout siklus pemupukan
} SystemSettings_t;

// -----------------------------------------------------------------
// Root config struct — disimpan utuh ke EEPROM
// -----------------------------------------------------------------
typedef struct {
    uint32_t magic;       // penanda EEPROM sudah pernah diinisialisasi
    uint16_t version;     // versi struct, untuk migrasi ke depan

    SystemSettings_t settings;
    IrrigationSchedule_t    irrigation[MAX_IRRIGATION_SCHEDULE];
    FertilizationSchedule_t fertilization[MAX_FERTILIZER_SCHEDULE];

    uint16_t crc16;        // integrity check, WAJIB field terakhir
} ConfigData_t;

#endif /* CONFIG_DATA_H_ */
