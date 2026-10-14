/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : STM32TouchController.cpp
  ******************************************************************************
  * This file was created by TouchGFX Generator 4.26.1. This file is only
  * generated once! Delete this file from your project and re-generate code
  * using STM32CubeMX or change this file manually to update it.
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

/* USER CODE BEGIN STM32TouchController */

#include <STM32TouchController.hpp>
#include "main.h"

extern "C" I2C_HandleTypeDef hi2c1;
extern "C" volatile uint8_t CTP_InterruptPending;

GPIO_InitTypeDef GPIO_InitStruct = {0};

extern Touch_t Touch;
static uint16_t resolution_x;
static uint16_t resolution_y;

static constexpr uint16_t GT911_I2C_ADDRESS = 0xBA;
static constexpr uint16_t GT911_STATUS_REGISTER = 0x814E;
static constexpr uint16_t GT911_COORDINATE_REGISTER = 0x814F;
static constexpr uint16_t DISPLAY_WIDTH = 1024;
static constexpr uint16_t DISPLAY_HEIGHT = 600;

void STM32TouchController::init()
{
    /**
     * Initialize touch controller and driver
     *
     */
    uint8_t resolutionData[4] = {0};

    HAL_NVIC_DisableIRQ(EXTI8_IRQn);
    GPIO_InitStruct.Pin = CTP_INT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(CTP_INT_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(CTP_RST_GPIO_Port, CTP_RST_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(CTP_INT_GPIO_Port, CTP_INT_Pin, GPIO_PIN_RESET);

    HAL_Delay( 20 );
    HAL_GPIO_WritePin(CTP_RST_GPIO_Port, CTP_RST_Pin, GPIO_PIN_SET);
    HAL_Delay( 50 );

    GPIO_InitStruct.Pin = CTP_INT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(CTP_INT_GPIO_Port, &GPIO_InitStruct);

    HAL_Delay( 100 );

    if (HAL_I2C_Mem_Read(&hi2c1, GT911_I2C_ADDRESS, 0x8048,
               I2C_MEMADD_SIZE_16BIT, resolutionData,
               sizeof(resolutionData), 10) != HAL_OK)
    {
      Error_Handler();
      return;
    }

    resolution_x = (uint16_t)resolutionData[0] | ((uint16_t)resolutionData[1] << 8);
    resolution_y = (uint16_t)resolutionData[2] | ((uint16_t)resolutionData[3] << 8);
    if (resolution_x < 2U || resolution_y < 2U)
    {
      Error_Handler();
      return;
    }

    GPIO_InitStruct.Pin = CTP_INT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(CTP_INT_GPIO_Port, &GPIO_InitStruct);
    __HAL_GPIO_EXTI_CLEAR_IT(CTP_INT_Pin);
    CTP_InterruptPending = 0U;
    HAL_NVIC_SetPriority(EXTI8_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI8_IRQn);
}

bool STM32TouchController::sampleTouch(int32_t& x, int32_t& y)
{
    /**
     * By default sampleTouch returns false,
     * return true if a touch has been detected, otherwise false.
     *
     * Coordinates are passed to the caller by reference by x and y.
     *
     * This function is called by the TouchGFX framework.
     * By default sampleTouch is called every tick, this can be adjusted by HAL::setTouchSampleRate(int8_t);
     *
     */
    if (CTP_InterruptPending == 0U)
    {
      return false;
    }
    CTP_InterruptPending = 0U;

    uint8_t status = 0U;
    if (HAL_I2C_Mem_Read(&hi2c1, GT911_I2C_ADDRESS, GT911_STATUS_REGISTER,
               I2C_MEMADD_SIZE_16BIT, &status, 1U, 10U) != HAL_OK)
    {
      CTP_InterruptPending = 1U;
      return false;
    }

    Touch.Status = (status >> 7) & 0x01U;
    Touch.Number = status & 0x0FU;
    if (Touch.Status == 0U)
    {
      Touch.Number = 0U;
      return false;
    }

    uint8_t coordinateData[5] = {0};
    bool hasTouch = Touch.Number >= 1U && Touch.Number <= 5U;
    if (hasTouch &&
      HAL_I2C_Mem_Read(&hi2c1, GT911_I2C_ADDRESS, GT911_COORDINATE_REGISTER,
               I2C_MEMADD_SIZE_16BIT, coordinateData,
               sizeof(coordinateData), 10U) != HAL_OK)
    {
      CTP_InterruptPending = 1U;
      return false;
    }

    uint8_t clearStatus = 0U;
    if (HAL_I2C_Mem_Write(&hi2c1, GT911_I2C_ADDRESS, GT911_STATUS_REGISTER,
                I2C_MEMADD_SIZE_16BIT, &clearStatus, 1U, 10U) != HAL_OK)
    {
      CTP_InterruptPending = 1U;
      return false;
    }

    if (!hasTouch)
    {
      Touch.Number = 0U;
      return false;
    }

    uint16_t rawX = (uint16_t)coordinateData[1] | ((uint16_t)coordinateData[2] << 8);
    uint16_t rawY = (uint16_t)coordinateData[3] | ((uint16_t)coordinateData[4] << 8);
    if (rawX >= resolution_x)
    {
      rawX = resolution_x - 1U;
    }
    if (rawY >= resolution_y)
    {
      rawY = resolution_y - 1U;
    }

    x = ((uint32_t)rawY * (DISPLAY_WIDTH - 1U)) / (resolution_y - 1U);
    y = ((uint32_t)(resolution_x - 1U - rawX) * (DISPLAY_HEIGHT - 1U)) /
      (resolution_x - 1U);
    return true;
}

/* USER CODE END STM32TouchController */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
