// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file lcd.c
 * @brief LCD display and CTP touch control.
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "lcd.h"
#include "printf.h"
#include <stdint.h>
#include "stm32mp13xx_hal_gpio.h"
#include "stm32mp13xx_hal_tim.h"
#include "stm32mp13xx_hal_rcc.h"

/* global variables */
static TIM_HandleTypeDef htim1;

void lcd_init(void)
{
    __HAL_RCC_TIM1_CLK_ENABLE();

    GPIO_InitTypeDef gpio;
    gpio.Pin       = GPIO_PIN_15;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOB, &gpio);

    htim1.Instance = TIM1;
    htim1.Init.Prescaler         = 99U;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = 999U;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(&htim1);

    TIM_OC_InitTypeDef oc;
    oc.OCMode       = TIM_OCMODE_PWM1;
    oc.Pulse        = 500U;
    oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    oc.OCFastMode   = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);
    htim1.Instance->BDTR |= TIM_BDTR_MOE;
}

void lcd_backlight(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)arg2;
   (void)arg3;

   if (argc > 0) {
      if (arg1 <= 100U) {
         __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 
               (htim1.Init.Period + 1U) * arg1 / 100U);
      }
   }
}
