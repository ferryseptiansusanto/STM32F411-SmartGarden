/**
 * @file    log_manager.h
 * @brief   Modul "Wartawan" FSM. Bertugas merakit pesan log sebelum dikirim ke SD Card.
 * @note    Modul ini BUKAN Task FreeRTOS dan tidak menyentuh pin/FatFs secara langsung.
 *
 * Created on: 3 Aug 2026
 * Author: ferry
 */

#ifndef MANAGERS_LOG_MANAGER_H_
#define MANAGERS_LOG_MANAGER_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Enumerasi tingkat keparahan (Severity Level) dari sebuah log.
 */
typedef enum {
    LOG_INFO = 0,   /**< Informasi operasional normal (e.g., "Irigasi Selesai") */
    LOG_WARN,       /**< Peringatan non-kritis (e.g., "Air Tangki 30%") */
    LOG_ERROR,      /**< Kegagalan yang butuh atensi (e.g., "I2C RTC Terputus") */
    LOG_CRITICAL    /**< Darurat/Hardware Fault (e.g., "Tangki Luber / Pompa Kering") */
} LogLevel_t;

/**
 * @brief   Menginisialisasi Context Log Manager untuk FatFs Wrapper.
 * @retval  bool true jika inisialisasi berhasil.
 */
bool LogManager_Init(void);

/**
 * @brief   Merakit string log (Timestamp + Level + Pesan) lalu meminta Wrapper menyimpannya.
 * @param   level Tingkat keparahan pesan (INFO/WARN/ERROR/CRITICAL).
 * @param   format String format (seperti printf) untuk isi pesan log.
 * @param   ... Argumen variadic (variabel data) untuk format string.
 * @retval  bool true jika log berhasil dirakit dan diserahkan ke Wrapper.
 */
bool LogManager_Write(LogLevel_t level, const char* format, ...);

#endif /* MANAGERS_LOG_MANAGER_H_ */
