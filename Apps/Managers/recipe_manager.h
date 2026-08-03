/**
 * @file    recipe_manager.h
 * @brief   Modul Parsing Multi-Pupuk (A/B Mix + Additives) & Pengadukan.
 * @note    Thread-Safe, murni berjalan di RAM tanpa I/O fisik langsung.
 *
 * Created on: 3 Aug 2026
 * Author: ferry
 */

#ifndef MANAGERS_RECIPE_MANAGER_H_
#define MANAGERS_RECIPE_MANAGER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "Config/recipe_config.h" /* Mengimpor semua batasan dari file config sub-modul */

/**
 * @brief Struktur data matang dari sebuah resep pemupukan komplit.
 */
typedef struct {
    char     name[MAX_RECIPE_NAME_LEN];   /**< Nama resep, misal "FertKangkung" */
    uint16_t fert_volumes[NUM_FERTILIZERS]; /**< Array volume pupuk (Indeks 0 = fert1) */
    uint16_t water_volume;                /**< Volume air baku (ml) */
    uint16_t mixing_time_sec;             /**< Lama pengadukan motor mixer (detik) */
} FertRecipe_t;

/**
 * @brief   Mereset struktur data resep ke nilai 0/kosong.
 * @param   recipe Pointer ke struktur resep.
 */
void Recipe_Clear(FertRecipe_t* recipe);

/**
 * @brief   Mengekstrak teks custom string menjadi struktur FertRecipe_t.
 * Support handling optional "ml" suffix (e.g., "50" atau "50ml").
 * @param   raw_string String sumber (e.g. "[Name][fert1:100...][water:1000][mixing:100]")
 * @param   out_recipe Pointer tempat menampung hasil ekstrak.
 * @retval  bool true jika parsing sukses dan data valid secara batasan agronomi.
 */
bool Recipe_Parse(const char* raw_string, FertRecipe_t* out_recipe);

/**
 * @brief   Memvalidasi apakah nilai-nilai di dalam resep aman dieksekusi aktuator.
 * @param   recipe Pointer ke resep yang akan diverifikasi.
 * @retval  bool true jika aman.
 */
bool Recipe_Validate(const FertRecipe_t* recipe);

#endif /* MANAGERS_RECIPE_MANAGER_H_ */
