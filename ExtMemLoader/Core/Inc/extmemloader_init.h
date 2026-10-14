/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    extmemloader_init.h
  * @author  MCD Application Team
  * @brief   Header file of Loader_Src.c
  *
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
#ifndef EXTMEMLOADER_INIT_H
#define EXTMEMLOADER_INIT_H

/* Includes ------------------------------------------------------------------*/
#include "stm32h7rsxx_hal.h"

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

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

uint32_t extmemloader_Init(void);
void Error_Handler(void);

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#define TFT_EN_Pin GPIO_PIN_15
#define TFT_EN_GPIO_Port GPIOE
#define TFT_SETB_Pin GPIO_PIN_3
#define TFT_SETB_GPIO_Port GPIOE
#define BL_EN_Pin GPIO_PIN_15
#define BL_EN_GPIO_Port GPIOG
#define BL_PWM_Pin GPIO_PIN_8
#define BL_PWM_GPIO_Port GPIOF
#define TFT_NRST_Pin GPIO_PIN_7
#define TFT_NRST_GPIO_Port GPIOE
#define CTP_RST_Pin GPIO_PIN_9
#define CTP_RST_GPIO_Port GPIOE
#define CTP_INT_Pin GPIO_PIN_8
#define CTP_INT_GPIO_Port GPIOE
#endif /* EXTMEMLOADER_INIT_H */
