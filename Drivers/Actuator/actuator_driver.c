/**
 * @file actuator_driver.c
 * @brief Implementasi dari driver aktuator.
 * @author Ferry
 * @date 22 Jul 2026
 */

#include "actuator_driver.h"

/**
 * @brief Struktur pemetaan untuk mengikat Enum ke definisi Pin MCU.
 */
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} ActuatorMap_t;

/**
 * @brief Tabel pencarian (Lookup Table) hardware aktuator.
 * MENGAPA DIBUAT ARRAY? Agar kita tidak perlu memakai Switch-Case/If-Else
 * yang panjang. Eksekusi menjadi O(1) konstan (sangat ringan untuk CPU).
 * @note Pastikan macro seperti VALVE_WATER_IN_GPIO_Port sesuai dengan
 * Aturan 9 (Hardware Pinout Mapping) di CubeMX (main.h).
 */
static const ActuatorMap_t actuatorMap[] = {
    {VALVE_WATER_IN_GPIO_Port, VALVE_WATER_IN_Pin}, // Posisi 0
    {VALVE_TANK_IN_GPIO_Port, VALVE_TANK_IN_Pin},   // Posisi 1
    {VALVE_TANK_OUT_GPIO_Port, VALVE_TANK_OUT_Pin}, // Posisi 2
    {VALVE_FERT1_GPIO_Port, VALVE_FERT1_Pin},    	// Posisi 3
    {VALVE_FERT2_GPIO_Port, VALVE_FERT2_Pin},  		// Posisi 4
    {VALVE_FERT3_GPIO_Port, VALVE_FERT3_Pin},  		// Posisi 5
    {VALVE_FERT4_GPIO_Port, VALVE_FERT4_Pin},  		// Posisi 6
    {VALVE_FERT5_GPIO_Port, VALVE_FERT5_Pin}, 		// Posisi 7
    {PUMP_OUT_GPIO_Port, PUMP_OUT_Pin},             // Posisi 8
    {PUMP_FERT_GPIO_Port, PUMP_FERT_Pin},           // Posisi 9
    {MIXER_GPIO_Port, MIXER_Pin}                    // Posisi 10
};

// Perlindungan Kompilasi (Compile-Time Safety):
// MENGAPA ADA _Static_assert? Mencegah Crash! Jika kelak ada engineer lain
// menambahkan aktuator baru di .h tapi lupa menambahkannya di array .c,
// proses "Build/Compile" akan otomatis gagal dan memunculkan pesan error ini.
_Static_assert(sizeof(actuatorMap)/sizeof(actuatorMap[0]) == ACT_MAX,
               "FATAL ERROR: Ukuran actuatorMap tidak sinkron dengan enumerasi ActuatorType_t!");

void Actuator_Init(void) {
    for (int i = 0; i < ACT_MAX; i++) {
        // MENGAPA CEK NULL? Untuk menghindari HardFault (Pointer memory error)
        // jika ada pin yang dinonaktifkan sementara dan di-set NULL.
        if (actuatorMap[i].port != NULL) {
            // MENGAPA RESET? Fail-Safe utama (Aturan 10). Mencegah aktuator
            // berputar/terbuka liar saat pertama kali STM32 mendapatkan listrik.
            HAL_GPIO_WritePin(actuatorMap[i].port, actuatorMap[i].pin, GPIO_PIN_RESET);
        }
    }
}

void Actuator_SetState(ActuatorType_t actuator, ActuatorState_t state) {
    // Validasi input di luar batas (Out-of-bound array protection).
    // Mencegah memory leak atau modifikasi register asing jika dikirim nilai ngawur.
    if (actuator >= ACT_MAX) return;

    if (actuatorMap[actuator].port != NULL) {
        HAL_GPIO_WritePin(
            actuatorMap[actuator].port,
            actuatorMap[actuator].pin,
            (state == ACT_ON) ? GPIO_PIN_SET : GPIO_PIN_RESET
        );
    }
}
