/**
 * @file command_event.h
 * @brief Kontrak Data FSM Utama (Union Memory Pooling & Zero-Copy)
 */
#ifndef INC_TASKS_COMMAND_EVENT_H_
#define INC_TASKS_COMMAND_EVENT_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CMD_ACTIVATE_PUMP = 0x01,
    CMD_WRITE_SCHED   = 0x02,
    CMD_BLUETOOTH_CSV = 0x03, // String mentah dari luar
	CMD_USER_CONFIRM_YES = 0x04,
	CMD_USER_CONFIRM_NO = 0x05,
	CMD_EMERGENCY_STOP = 0x06

} CommandID_t;

/**
 * @brief Struktur data utama penampung pesan (Maksimal 8 Byte di RAM)
 * MENGAPA UNION? Agar antrean FSM berukuran kecil dan konstan,
 * menghemat puluhan byte per slot antrean.
 */
typedef struct {
    CommandID_t cmd_id;
    union {
        struct {
            uint8_t actuator_id;
            bool state;
        } pump;

        struct {
            char* str_ptr;  // ZERO-COPY: Hanya menyimpan alamat memori dari heap
            uint16_t len;
        } csv_data;
    } payload;
} CommandEvent_t;

#endif /* INC_TASKS_COMMAND_EVENT_H_ */
