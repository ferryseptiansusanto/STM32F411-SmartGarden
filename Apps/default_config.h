/*
 * default_config.h
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 *
 * Nilai default dipakai HANYA saat EEPROM pertama kali diinisialisasi
 * Jangan dipakai langsung di kode aplikasi — selalu akses lewat ConfigData_t runtime
 */

#ifndef APPS_DEFAULT_CONFIG_H_
#define APPS_DEFAULT_CONFIG_H_

#include "config_data.h"

/**
 * @brief Nilai default kalibrasi sistem (Fail-Safe Defaults)
 * MENGAPA: Dibuat sebagai 'static const' agar nilai ini tersimpan di Flash (ROM)
 * dan tidak memakan kapasitas RAM (SRAM) pada STM32.
 */
static const SensorCalibration_t factory_default_calib = {
    .ph_offset = 7.0f,
    .ph_slope = -59.16f,             /* Ideal Nernst equation slope (mV/pH) di 25C */
    .tds_factor = 0.5f,              /* Faktor konversi EC ke TDS umum */
    .temp_offset = 0.0f,
    .temp_slope = 100.0f,            /* Misal untuk LM35: 10mV/°C -> V * 100 */
    .fm_inlet_pulse_per_liter = 450, /* Default YF-S201 */
    .fm_outlet_pulse_per_liter = 450,
    .fm_fert_pulse_per_liter = 450,
    .crc32 = 0                       /* Akan dihitung oleh program saat disave */
};

#endif /* APPS_DEFAULT_CONFIG_H_ */
