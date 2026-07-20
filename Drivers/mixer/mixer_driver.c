/*
 * mixer_driver.c
 *
 *  Created on: 3 Jul 2026
 *      Author: ferry
 */


#include "mixer_driver.h"

void Mixer_Init(void) {
    HAL_GPIO_WritePin(MIXER_GPIO_Port, MIXER_Pin, GPIO_PIN_RESET); // default OFF
}

void Mixer_On(void) {
    HAL_GPIO_WritePin(MIXER_GPIO_Port, MIXER_Pin, GPIO_PIN_SET);
}

void Mixer_Off(void) {
    HAL_GPIO_WritePin(MIXER_GPIO_Port, MIXER_Pin, GPIO_PIN_RESET);
}
