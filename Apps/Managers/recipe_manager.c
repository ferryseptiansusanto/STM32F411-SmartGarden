/**
 * @file    recipe_manager.c
 * @brief   Implementasi parser teks resep Multi-Pupuk (Zero-Blocking, No Heap/Malloc).
 *
 * Created on: 3 Aug 2026
 * Author: ferry
 */

#include "recipe_manager.h"
#include <stdio.h>
#include <string.h>

void Recipe_Clear(FertRecipe_t* recipe) {
    if (recipe == NULL) return;
    memset(recipe, 0, sizeof(FertRecipe_t));
}

bool Recipe_Parse(const char* raw_string, FertRecipe_t* out_recipe) {
    if (raw_string == NULL || out_recipe == NULL) return false;

    Recipe_Clear(out_recipe);

    /* 1. BLOK 1: NAMA RESEP -> [FertKangkung] */
    const char *p1 = strchr(raw_string, '[');
    const char *p2 = p1 ? strchr(p1, ']') : NULL;
    if (!p1 || !p2) return false;

    size_t len = p2 - p1 - 1;
    if (len >= RECIPE_MAX_NAME_LEN) len = RECIPE_MAX_NAME_LEN - 1;
    strncpy(out_recipe->name, p1 + 1, len);
    out_recipe->name[len] = '\0';

    /* 2. BLOK 2: PUPUK -> [fert1:100,fert2:50ml,...] */
    p1 = strchr(p2, '[');
    p2 = p1 ? strchr(p1, ']') : NULL;
    if (!p1 || !p2) return false;

    char fert_buf[RECIPE_FERT_BUF_SIZE];
    len = p2 - p1 - 1;
    if (len >= sizeof(fert_buf)) return false; // Buffer overflow protection
    strncpy(fert_buf, p1 + 1, len);
    fert_buf[len] = '\0';

    /* Menggunakan strtok_r agar aman dipanggil di dalam lingkungan FreeRTOS */
    char *saveptr;
    char *tok = strtok_r(fert_buf, ",", &saveptr);
    while (tok != NULL) {
        int id = 0, vol = 0;

        // Mendukung ekstrak misal: fert1:100
        if (sscanf(tok, "fert%d:%d", &id, &vol) == 2) {
            // Pastikan ID tidak melampaui limit fisik perangkat keras
            if (id >= 1 && id <= RECIPE_NUM_FERTILIZERS && vol >= 0) {
                out_recipe->fert_volumes[id - 1] = (uint16_t)vol;
            }
        }
        tok = strtok_r(NULL, ",", &saveptr);
    }

    /* 3. BLOK 3: AIR -> [water:1000] */
    p1 = strchr(p2, '[');
    p2 = p1 ? strchr(p1, ']') : NULL;
    if (!p1 || !p2) return false;

    int water_val = 0;
    if (sscanf(p1 + 1, "water:%d", &water_val) == 1 && water_val >= 0) {
        out_recipe->water_volume = (uint16_t)water_val;
    }

    /* 4. BLOK 4: MIXING -> [mixing:100] */
    p1 = strchr(p2, '[');
    p2 = p1 ? strchr(p1, ']') : NULL;
    if (!p1 || !p2) return false;

    int mix_val = 0;
    if (sscanf(p1 + 1, "mixing:%d", &mix_val) == 1 && mix_val >= 0) {
        out_recipe->mixing_time_sec = (uint16_t)mix_val;
    }

    return Recipe_Validate(out_recipe);
}

bool Recipe_Validate(const FertRecipe_t* recipe) {
    if (recipe == NULL) return false;

    /* Pengecekan Batas Maksimal berdasarkan Config Sub-Modul (Fail-Safe) */
    for (uint8_t i = 0; i < RECIPE_NUM_FERTILIZERS; i++) {
        if (recipe->fert_volumes[i] > RECIPE_MAX_FERT_VOL_ML) {
            return false;
        }
    }

    if (recipe->water_volume > RECIPE_MAX_WATER_VOL_ML) {
        return false;
    }

    if (recipe->mixing_time_sec > RECIPE_MAX_MIXING_TIME_SEC) {
        return false;
    }

    // Pastikan tidak mengeksekusi resep kosong sama sekali
    bool has_task = (recipe->water_volume > 0);
    for (uint8_t i = 0; i < RECIPE_NUM_FERTILIZERS; i++) {
        if (recipe->fert_volumes[i] > 0) has_task = true;
    }

    return has_task;
}
