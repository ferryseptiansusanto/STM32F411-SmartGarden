/**
 * @file    recipe_manager.h
 * @brief   Header file untuk manajemen resep pemupukan dari SD Card.
 *
 *  Created on: 22 Jul 2026
 *      Author: ferry
 */
#ifndef RECIPE_MANAGER_H
#define RECIPE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define NUM_FERTILIZERS 5
#define MAX_RECIPE_NAME 16

typedef struct {
    char name[MAX_RECIPE_NAME];
    float target_vol_liter[NUM_FERTILIZERS];
} FertRecipe_t;

/**
 * @brief   Mencari dan memuat data resep dari file SD Card berdasarkan nama resep.
 * @param   recipe_name Nama file resep atau text pencarian di dalam file resep.
 * @param   out_recipe Pointer menampung resep yang berhasil ditemukan.
 * @retval  bool true jika resep berhasil ditemukan dan dimuat.
 */
bool Recipe_Load(const char* recipe_name, FertRecipe_t* out_recipe);

#endif /* RECIPE_MANAGER_H */
