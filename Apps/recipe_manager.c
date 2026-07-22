/*
 * recipe_manager.c
 *
 *  Created on: 22 Jul 2026
 *      Author: ferry
 */

#include "recipe_manager.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

bool Recipe_Load(const char* recipe_name, FertRecipe_t* out_recipe) {
    FIL fobj;
    FRESULT fr;
    char line[128];
    bool found = false;

    /* MENGAPA CSV: Format CSV mudah diedit oleh end-user melalui Excel di komputer,
       lalu tinggal dimasukkan kembali SD Card-nya ke mesin Smart Garden */
    fr = f_open(&fobj, "recipes.csv", FA_READ);
    if (fr != FR_OK) {
        printf("[RECIPE] ERROR: Gagal membuka file recipes.csv (%d)\n", fr);
        return false;
    }

    /* Lewati baris header CSV (e.g., Name,Fert1,Fert2...) */
    f_gets(line, sizeof(line), &fobj);

    /* Baca baris demi baris hingga akhir file */
    while (f_gets(line, sizeof(line), &fobj) && !found) {
        char name[MAX_RECIPE_NAME];
        float v1, v2, v3, v4, v5;

        /* Parsing format CSV: nama_resep,v1,v2,v3,v4,v5 */
        if (sscanf(line, "%[^,],%f,%f,%f,%f,%f", name, &v1, &v2, &v3, &v4, &v5) == 6) {
            if (strcmp(name, recipe_name) == 0) {
                strncpy(out_recipe->name, name, MAX_RECIPE_NAME);
                out_recipe->target_vol_liter[0] = v1;
                out_recipe->target_vol_liter[1] = v2;
                out_recipe->target_vol_liter[2] = v3;
                out_recipe->target_vol_liter[3] = v4;
                out_recipe->target_vol_liter[4] = v5;
                found = true;
            }
        }
    }

    f_close(&fobj);
    return found;
}
