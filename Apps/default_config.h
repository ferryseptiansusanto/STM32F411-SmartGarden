/*
 * default_config.h
 *
 *  Created on: 14 Jul 2026
 *      Author: ferry
 *
 * Nilai default dipakai HANYA saat EEPROM pertama kali diinisialisasi
 * Jangan dipakai langsung di kode aplikasi — selalu akses lewat ConfigData_t runtime
 */

#ifndef DEFAULT_CONFIG_H_
#define DEFAULT_CONFIG_H_


#define DEFAULT_MIXING_DURATION_MS         10000
#define DEFAULT_VALVE_OUTPUT_DURATION_MS    5000
#define DEFAULT_FLOW_CALIBRATION_OFFSET        0

#define DEFAULT_IRRIGATION_TIMEOUT_MS      60000
#define DEFAULT_FERTILIZATION_TIMEOUT_MS   60000

#endif /* DEFAULT_CONFIG_H_ */
