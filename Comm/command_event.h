/*
 * command_event.h
 * Berisi definisi struktur data event untuk sistem antrean perintah (Command Queue)
 */

#ifndef INC_TASKS_COMMAND_EVENT_H_
#define INC_TASKS_COMMAND_EVENT_H_

#include <stdint.h>
#include <stdbool.h>

// Enumerasi tipe event yang masuk ke sistem
typedef enum {
    EVENT_TYPE_KEYPAD,
    EVENT_TYPE_BLUETOOTH_CSV  // Tipe event baru untuk string CSV dari Bluetooth
} EventType;

// Struktur data utama penampung pesan perintah
typedef struct {
    EventType type;
    union {
        struct {
            char key;
            bool longPress;
        } keypad;

        struct {
            char buffer[64];  // Menampung string CSV (misal: "RTC,26,06...")
            uint8_t len;      // Panjang karakter string
        } bluetooth;
    } data;
} CommandEvent;

#endif /* INC_TASKS_COMMAND_EVENT_H_ */
