/*
 * @file    default_config.h
 * @brief   Nilai default kalibrasi sistem (Fail-Safe Defaults).
 * @note    Dibuat 'static const' agar tersimpan di Flash (ROM).
 * Hanya di-include oleh config_manager.c untuk mencegah duplikasi memori.
 * Nilai default dipakai HANYA saat EEPROM pertama kali diinisialisasi
 * Jangan dipakai langsung di kode aplikasi — selalu akses lewat ConfigData_t runtime
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 */


#ifndef APPS_DEFAULT_CONFIG_H_
#define APPS_DEFAULT_CONFIG_H_

#include "config_data.h"

static const SystemConfig_t factory_default_calib = {
    // Kalibrasi Sensor
    .ph_offset = 7.0f,
    .ph_slope = -59.16f,             /* Ideal Nernst equation slope (mV/pH) di 25C */
    .tds_factor = 0.5f,              /* Faktor konversi EC ke TDS umum */
    .temp_offset = 0.0f,
    .temp_slope = 100.0f,            /* LM35: 10mV/°C -> V * 100 */

    // Kalibrasi Aktuator
    .fm_inlet_pulse_per_liter = 450, /* Default YF-S201 */
    .fm_outlet_pulse_per_liter = 450,
    .fm_fert_pulse_per_liter = 450,

    // Parameter FSM
    .waiting_user_response_time = 60000,   /* 60 Detik (1 Menit) */
    .max_delay_tolerance = 1800000,        /* 30 Menit */

    // Checksum Awal
    .crc32 = 0                       /* Dihitung otomatis saat Load Default */
};

#endif /* APPS_DEFAULT_CONFIG_H_ */
