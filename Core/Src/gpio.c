/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, SPI1_CS_Pin|VALVE_FERT1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, TEMP_Pin|MIXER_Pin|PUMP_OUT_Pin|VALVE_FERT5_Pin
                          |VALVE_FERT4_Pin|VALVE_FERT3_Pin|VALVE_FERT2_Pin|VALVE_TANK_IN_Pin
                          |VALVE_TANK_OUT_Pin|VALVE_WATER_IN_Pin|PUMP_FERT_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : SPI1_CS_Pin VALVE_FERT1_Pin */
  GPIO_InitStruct.Pin = SPI1_CS_Pin|VALVE_FERT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : TEMP_Pin MIXER_Pin PUMP_OUT_Pin VALVE_FERT5_Pin
                           VALVE_FERT4_Pin VALVE_FERT3_Pin VALVE_FERT2_Pin VALVE_TANK_IN_Pin
                           VALVE_TANK_OUT_Pin VALVE_WATER_IN_Pin PUMP_FERT_Pin */
  GPIO_InitStruct.Pin = TEMP_Pin|MIXER_Pin|PUMP_OUT_Pin|VALVE_FERT5_Pin
                          |VALVE_FERT4_Pin|VALVE_FERT3_Pin|VALVE_FERT2_Pin|VALVE_TANK_IN_Pin
                          |VALVE_TANK_OUT_Pin|VALVE_WATER_IN_Pin|PUMP_FERT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : LVL_TANK_EMPTY_Pin LVL_TANK_FULL_Pin */
  GPIO_InitStruct.Pin = LVL_TANK_EMPTY_Pin|LVL_TANK_FULL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
