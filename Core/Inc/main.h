/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SYS_WKUP_Pin GPIO_PIN_0
#define SYS_WKUP_GPIO_Port GPIOA
#define FM_FERTILIZER_Pin GPIO_PIN_1
#define FM_FERTILIZER_GPIO_Port GPIOA
#define FM_OUTLET_Pin GPIO_PIN_2
#define FM_OUTLET_GPIO_Port GPIOA
#define PH_Pin GPIO_PIN_3
#define PH_GPIO_Port GPIOA
#define SPI1_CS_Pin GPIO_PIN_4
#define SPI1_CS_GPIO_Port GPIOA
#define TDS_Pin GPIO_PIN_0
#define TDS_GPIO_Port GPIOB
#define TEMP_Pin GPIO_PIN_1
#define TEMP_GPIO_Port GPIOB
#define MIXER_Pin GPIO_PIN_2
#define MIXER_GPIO_Port GPIOB
#define PUMP_OUT_Pin GPIO_PIN_10
#define PUMP_OUT_GPIO_Port GPIOB
#define VALVE_FERT5_Pin GPIO_PIN_12
#define VALVE_FERT5_GPIO_Port GPIOB
#define VALVE_FERT4_Pin GPIO_PIN_13
#define VALVE_FERT4_GPIO_Port GPIOB
#define VALVE_FERT3_Pin GPIO_PIN_14
#define VALVE_FERT3_GPIO_Port GPIOB
#define VALVE_FERT2_Pin GPIO_PIN_15
#define VALVE_FERT2_GPIO_Port GPIOB
#define VALVE_FERT1_Pin GPIO_PIN_8
#define VALVE_FERT1_GPIO_Port GPIOA
#define USART1_TX_Pin GPIO_PIN_9
#define USART1_TX_GPIO_Port GPIOA
#define USART1_RX_Pin GPIO_PIN_10
#define USART1_RX_GPIO_Port GPIOA
#define LVL_TANK_EMPTY_Pin GPIO_PIN_11
#define LVL_TANK_EMPTY_GPIO_Port GPIOA
#define LVL_TANK_FULL_Pin GPIO_PIN_12
#define LVL_TANK_FULL_GPIO_Port GPIOA
#define FM_INLET_Pin GPIO_PIN_15
#define FM_INLET_GPIO_Port GPIOA
#define VALVE_TANK_IN_Pin GPIO_PIN_4
#define VALVE_TANK_IN_GPIO_Port GPIOB
#define VALVE_TANK_OUT_Pin GPIO_PIN_5
#define VALVE_TANK_OUT_GPIO_Port GPIOB
#define I2C1_SCL_Pin GPIO_PIN_6
#define I2C1_SCL_GPIO_Port GPIOB
#define I2C1_SDA_Pin GPIO_PIN_7
#define I2C1_SDA_GPIO_Port GPIOB
#define VALVE_WATER_IN_Pin GPIO_PIN_8
#define VALVE_WATER_IN_GPIO_Port GPIOB
#define PUMP_FERT_Pin GPIO_PIN_9
#define PUMP_FERT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
