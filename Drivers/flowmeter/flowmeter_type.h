/**
 * @file flowmeter_type.h
 * @brief Definisi tipe dan ID Logis untuk sensor Flowmeter.
 * @author Ferry
 * @date 11 Jun 2026
 */

#ifndef FLOWMETER_FLOWMETER_TYPE_H_
#define FLOWMETER_FLOWMETER_TYPE_H_

// Definisi Standar Pulsa per Liter berdasarkan tipe fisik sensor
#define YFDN50    20
#define YFS201    450
#define YFB5      450
#define YFB1      660
#define YFB10     352
#define YFS401    5880
#define YFB1S     1077
#define OF10ZAT   400
#define OF10ZZT   400
#define OF05ZAT  2174
#define OF05ZZT  2174

/**
 * @brief Enum ID Logis memetakan peran flowmeter sesuai Blueprint Pinout.
 * Terpisah dari konfigurasi hardware Timer (Layer Abstraksi Logis).
 */
typedef enum {
	FM_TANK_IN = 0,  ///< Flowmeter Jalur Masuk (TIM5 Channel 2)
    FM_MAIN_OUTLET,      ///< Flowmeter Jalur Irigasi ke Tanaman (TIM9 Channel 1)
    FM_FERT,             ///< Flowmeter Jalur Pupuk (TIM2 Channel 1)
    FM_MAX               ///< Sentinel value untuk batas array/validasi
} FlowSensorID_t;

#endif /* FLOWMETER_FLOWMETER_TYPE_H_ */
