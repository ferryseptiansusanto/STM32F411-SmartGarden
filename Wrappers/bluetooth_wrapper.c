/*
 * @file    bluetooth_wrapper.c
 * @brief   Implementasi lapisan abstraksi aplikasi (Wrapper) Modem/Bluetooth
 * @note    Menggunakan standar Pointer Passing (pvPortMalloc) FreeRTOS untuk
 * efisiensi memori Queue tingkat tinggi.
 *
 * Created on: 8 May 2026
 * Author: ferry
 */

#include "bluetooth_wrapper.h"
#include "command_task.h" // Asumsi deklarasi Queue Task pengirim ada di sini
#include "FreeRTOS.h"
#include <string.h>

BL_Device Bluetooth_Ctx;

void BLUETOOTH_Init(BL_Device *dev, UART_Context *ctx) {
    if (dev != NULL && ctx != NULL) {
        dev->uart_ctx = ctx;
    }
}

/**
 * @brief  Mengemas pesan dan mengirimkannya ke Queue TX secara asinkron.
 * @note   MENGAPA KITA PAKAI pvPortMalloc?
 * Jika kita mem-passing struktur USART_Message utuh ke dalam Queue, FreeRTOS akan
 * menyalin seluruh byte (bisa >200 byte) yang memboroskan RAM dan CPU.
 * Dengan pvPortMalloc, kita hanya mem-passing alamat memori (4 byte/pointer) O(1).
 */
bool BLUETOOTH_SendMessage(BL_Device *dev, USART_Command cmd, const char *str) {
    if (dev == NULL || str == NULL) return false;

    // 1. FAIL-SAFE: Alokasi memori dinamis secara aman dari FreeRTOS Heap
    USART_Message *msg = (USART_Message *)pvPortMalloc(sizeof(USART_Message));

    // Jika RAM penuh (Heap Exhaustion), batalkan operasi agar sistem tidak Crash
    if (msg == NULL) return false;

    // Bersihkan memori dari sampah data sebelumnya
    memset(msg, 0, sizeof(USART_Message));

    // 2. Pasang metadata instruksi protokol aplikasi
    msg->cmd = cmd;
    msg->len = strlen(str);

    // 3. PENGAMANAN BUFFER OVERFLOW: Potong string jika kebesaran
    if (msg->len > sizeof(msg->payload) - 1) { // Sisakan 1 byte untuk Null-Terminator
        msg->len = sizeof(msg->payload) - 1;
    }

    // 4. Salin payload
    memcpy(msg->payload, str, msg->len);
    msg->payload[msg->len] = '\0'; // Ekstra perlindungan String C

    // 5. KIRIM POINTER KE QUEUE
    // Fungsi Queue pengirim (misal: xQueueSend) akan mereturn pdPASS jika sukses
    if (BLUETOOTH_Task_SendMessage(msg) != pdPASS) {
        // FAIL-SAFE: Jika Queue penuh, kita WAJIB membebaskan (Free) memori
        // yang tadi di-malloc agar tidak terjadi Memory Leak!
        vPortFree(msg);
        return false;
    }

    return true; // Sukses terkirim ke latar belakang!
}
