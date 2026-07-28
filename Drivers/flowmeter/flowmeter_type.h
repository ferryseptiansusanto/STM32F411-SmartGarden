/*
 * flowmeter_type.h
 *
 *  Created on: 11 Jun 2026
 *      Author: ferry
 */

#ifndef FLOWMETER_FLOWMETER_TYPE_H_
#define FLOWMETER_FLOWMETER_TYPE_H_

// Sensor Type (Pulses per Liter)
// -----------------------
// YF
#define YFDN50    20
#define YFS201    450
#define YFB5      450
#define YFB1      660
#define YFB10     352
#define YFS401    5880
#define YFB1S     1077
// OF
#define OF10ZAT   400
#define OF10ZZT   400
#define OF05ZAT  2174
#define OF05ZZT  2174
//------------------------

/**
 * @brief Enum ID Logis untuk memetakan peran sensor flowmeter.
 * Terpisah total dari konfigurasi hardware Timer/Channel (CubeMX).
 */
typedef enum {
    FLOW_SENSOR_INLET = 0,  ///< Flowmeter Jalur Masuk Utama
    FLOW_SENSOR_OUTLET,     ///< Flowmeter Jalur Irigasi ke Tanaman
    FLOW_SENSOR_FERT,       ///< Flowmeter Jalur Pupuk/Nutrisi
    FLOW_SENSOR_MAX
} FlowSensorID_t;

#endif /* FLOWMETER_FLOWMETER_TYPE_H_ */
