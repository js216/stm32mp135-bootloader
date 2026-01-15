// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file lcd.c
 * @brief LCD display and CTP touch control.
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "lcd.h"
#include "ctp.h"
#include "printf.h"
#include <stdint.h>
#include "stm32mp13xx_hal_gpio.h"
#include "stm32mp13xx_hal_tim.h"
#include "stm32mp13xx_hal_rcc.h"
#include "stm32mp13xx_hal_ltdc.h"

#define  RK043FN48H_WIDTH    ((uint16_t)480)  /* LCD PIXEL WIDTH            */
#define  RK043FN48H_HEIGHT   ((uint16_t)272)  /* LCD PIXEL HEIGHT           */
#define  RK043FN48H_HSYNC    ((uint16_t)41)   /* Horizontal synchronization */
#define  RK043FN48H_HBP      ((uint16_t)13)   /* Horizontal back porch      */
#define  RK043FN48H_HFP      ((uint16_t)32)   /* Horizontal front porch     */
#define  RK043FN48H_VSYNC    ((uint16_t)10)   /* Vertical synchronization   */
#define  RK043FN48H_VBP      ((uint16_t)2)    /* Vertical back porch        */
#define  RK043FN48H_VFP      ((uint16_t)2)    /* Vertical front porch       */
#define  IMAGE_HOR_SIZE    320
#define  IMAGE_VER_SIZE    240

#ifdef EVB
#define LCD_BL_PORT    GPIOE
#define LCD_BL_PIN     GPIO_PIN_12
#define LCD_CLK_PORT   GPIOD
#define LCD_CLK_PIN    GPIO_PIN_9
#define LCD_CLK_AF     GPIO_AF13_LCD
#define LCD_HSYNC_PORT GPIOC
#define LCD_HSYNC_PIN  GPIO_PIN_6
#define LCD_HSYNC_AF   GPIO_AF14_LCD
#define LCD_VSYNC_PORT GPIOG
#define LCD_VSYNC_PIN  GPIO_PIN_4
#define LCD_VSYNC_AF   GPIO_AF11_LCD
#define LCD_DE_PORT    GPIOH
#define LCD_DE_PIN     GPIO_PIN_9
#define LCD_DE_AF      GPIO_AF11_LCD
#define LCD_R2_PORT    GPIOG
#define LCD_R2_PIN     GPIO_PIN_7
#define LCD_R2_AF      GPIO_AF14_LCD
#define LCD_R3_PORT    GPIOB
#define LCD_R3_PIN     GPIO_PIN_12
#define LCD_R3_AF      GPIO_AF13_LCD
#define LCD_R4_PORT    GPIOD
#define LCD_R4_PIN     GPIO_PIN_14
#define LCD_R4_AF      GPIO_AF14_LCD
#define LCD_R5_PORT    GPIOE
#define LCD_R5_PIN     GPIO_PIN_7
#define LCD_R5_AF      GPIO_AF14_LCD
#define LCD_R6_PORT    GPIOE
#define LCD_R6_PIN     GPIO_PIN_13
#define LCD_R6_AF      GPIO_AF14_LCD
#define LCD_R7_PORT    GPIOE
#define LCD_R7_PIN     GPIO_PIN_9
#define LCD_R7_AF      GPIO_AF14_LCD
#define LCD_G2_PORT    GPIOH
#define LCD_G2_PIN     GPIO_PIN_3
#define LCD_G2_AF      GPIO_AF14_LCD
#define LCD_G3_PORT    GPIOF
#define LCD_G3_PIN     GPIO_PIN_3
#define LCD_G3_AF      GPIO_AF14_LCD
#define LCD_G4_PORT    GPIOD
#define LCD_G4_PIN     GPIO_PIN_5
#define LCD_G4_AF      GPIO_AF14_LCD
#define LCD_G5_PORT    GPIOG
#define LCD_G5_PIN     GPIO_PIN_0
#define LCD_G5_AF      GPIO_AF14_LCD
#define LCD_G6_PORT    GPIOC
#define LCD_G6_PIN     GPIO_PIN_7
#define LCD_G6_AF      GPIO_AF14_LCD
#define LCD_G7_PORT    GPIOA
#define LCD_G7_PIN     GPIO_PIN_15
#define LCD_G7_AF      GPIO_AF11_LCD
#define LCD_B2_PORT    GPIOD
#define LCD_B2_PIN     GPIO_PIN_10
#define LCD_B2_AF      GPIO_AF14_LCD
#define LCD_B3_PORT    GPIOF
#define LCD_B3_PIN     GPIO_PIN_2
#define LCD_B3_AF      GPIO_AF14_LCD
#define LCD_B4_PORT    GPIOH
#define LCD_B4_PIN     GPIO_PIN_4
#define LCD_B4_AF      GPIO_AF11_LCD
#define LCD_B5_PORT    GPIOE
#define LCD_B5_PIN     GPIO_PIN_0
#define LCD_B5_AF      GPIO_AF14_LCD
#define LCD_B6_PORT    GPIOB
#define LCD_B6_PIN     GPIO_PIN_6
#define LCD_B6_AF      GPIO_AF14_LCD
#define LCD_B7_PORT    GPIOF
#define LCD_B7_PIN     GPIO_PIN_1
#define LCD_B7_AF      GPIO_AF13_LCD
#else
#define LCD_BL_PORT    GPIOB
#define LCD_BL_PIN     GPIO_PIN_15
#define LCD_CLK_PORT   GPIOD
#define LCD_CLK_PIN    GPIO_PIN_9
#define LCD_CLK_AF     GPIO_AF13_LCD
#define LCD_HSYNC_PORT GPIOE
#define LCD_HSYNC_PIN  GPIO_PIN_1
#define LCD_HSYNC_AF   GPIO_AF9_LCD
#define LCD_VSYNC_PORT GPIOE
#define LCD_VSYNC_PIN  GPIO_PIN_12
#define LCD_VSYNC_AF   GPIO_AF9_LCD
#define LCD_DE_PORT    GPIOG
#define LCD_DE_PIN     GPIO_PIN_6
#define LCD_DE_AF      GPIO_AF13_LCD
#define LCD_R2_PORT    GPIO_NONE
#define LCD_R2_PIN     0
#define LCD_R2_AF      0
#define LCD_R3_PORT    GPIOD
#define LCD_R3_PIN     GPIO_PIN_9
#define LCD_R3_AF      GPIO_AF13_LCD
#define LCD_R4_PORT    GPIOE
#define LCD_R4_PIN     GPIO_PIN_3
#define LCD_R4_AF      GPIO_AF13_LCD
#define LCD_R5_PORT    GPIOF
#define LCD_R5_PIN     GPIO_PIN_5
#define LCD_R5_AF      GPIO_AF14_LCD
#define LCD_R6_PORT    GPIOF
#define LCD_R6_PIN     GPIO_PIN_0
#define LCD_R6_AF      GPIO_AF13_LCD
#define LCD_R7_PORT    GPIOF
#define LCD_R7_PIN     GPIO_PIN_6
#define LCD_R7_AF      GPIO_AF13_LCD
#define LCD_G2_PORT    GPIOF
#define LCD_G2_PIN     GPIO_PIN_7
#define LCD_G2_AF      GPIO_AF14_LCD
#define LCD_G3_PIN     GPIO_PIN_6
#define LCD_G3_AF      GPIO_AF14_LCD
#define LCD_G4_PORT    GPIOG
#define LCD_G4_PIN     GPIO_PIN_5
#define LCD_G4_AF      GPIO_AF11_LCD
#define LCD_G5_PORT    GPIOG
#define LCD_G5_PIN     GPIO_PIN_0
#define LCD_G5_AF      GPIO_AF14_LCD
#define LCD_G6_PORT    GPIOA
#define LCD_G6_PIN     GPIO_PIN_12
#define LCD_G6_AF      GPIO_AF14_LCD
#define LCD_G7_PORT    GPIOA
#define LCD_G7_PIN     GPIO_PIN_15
#define LCD_G7_AF      GPIO_AF11_LCD
#define LCD_B2_PORT    GPIO_NONE
#define LCD_B2_PIN     0
#define LCD_B2_AF      0
#define LCD_B3_PORT    GPIOG
#define LCD_B3_PIN     GPIO_PIN_15
#define LCD_B3_AF      GPIO_AF14_LCD
#define LCD_B4_PORT    GPIOB
#define LCD_B4_PIN     GPIO_PIN_2
#define LCD_B4_AF      GPIO_AF14_LCD
#define LCD_B5_PORT    GPIOH
#define LCD_B5_PIN     GPIO_PIN_9
#define LCD_B5_AF      GPIO_AF9_LCD
#define LCD_B6_PORT    GPIOF
#define LCD_B6_PIN     GPIO_PIN_4
#define LCD_B6_AF      GPIO_AF13_LCD
#define LCD_B7_PORT    GPIOB
#define LCD_B7_PIN     GPIO_PIN_6
#define LCD_B7_AF      GPIO_AF14_LCD
#endif

struct lcd_pin_cfg {
   GPIO_TypeDef *port;
   uint16_t pin;
   uint32_t af;
};

/* global variables */
static TIM_HandleTypeDef htim1;
static LTDC_HandleTypeDef hLtdcHandler;

static void lcd_backlight_init(void)
{
   __HAL_RCC_TIM1_CLK_ENABLE();

   GPIO_InitTypeDef gpio;
   gpio.Pin       = LCD_BL_PIN;
   gpio.Mode      = GPIO_MODE_AF_PP;
   gpio.Pull      = GPIO_NOPULL;
   gpio.Speed     = GPIO_SPEED_FREQ_LOW;
   gpio.Alternate = GPIO_AF1_TIM1;
   HAL_GPIO_Init(LCD_BL_PORT, &gpio);

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

static void lcd_panel_pin_setup(void)
{
   static const struct lcd_pin_cfg pins[] = {
      { LCD_CLK_PORT,   LCD_CLK_PIN,   LCD_CLK_AF   },
      { LCD_HSYNC_PORT, LCD_HSYNC_PIN, LCD_HSYNC_AF },
      { LCD_VSYNC_PORT, LCD_VSYNC_PIN, LCD_VSYNC_AF },
      { LCD_DE_PORT,    LCD_DE_PIN,    LCD_DE_AF    },
      { LCD_R2_PORT,    LCD_R2_PIN,    LCD_R2_AF    },
      { LCD_R3_PORT,    LCD_R3_PIN,    LCD_R3_AF    },
      { LCD_R4_PORT,    LCD_R4_PIN,    LCD_R4_AF    },
      { LCD_R5_PORT,    LCD_R5_PIN,    LCD_R5_AF    },
      { LCD_R6_PORT,    LCD_R6_PIN,    LCD_R6_AF    },
      { LCD_R7_PORT,    LCD_R7_PIN,    LCD_R7_AF    },
      { LCD_G2_PORT,    LCD_G2_PIN,    LCD_G2_AF    },
      { LCD_G3_PORT,    LCD_G3_PIN,    LCD_G3_AF    },
      { LCD_G4_PORT,    LCD_G4_PIN,    LCD_G4_AF    },
      { LCD_G5_PORT,    LCD_G5_PIN,    LCD_G5_AF    },
      { LCD_G6_PORT,    LCD_G6_PIN,    LCD_G6_AF    },
      { LCD_G7_PORT,    LCD_G7_PIN,    LCD_G7_AF    },
      { LCD_B2_PORT,    LCD_B2_PIN,    LCD_B2_AF    },
      { LCD_B3_PORT,    LCD_B3_PIN,    LCD_B3_AF    },
      { LCD_B4_PORT,    LCD_B4_PIN,    LCD_B4_AF    },
      { LCD_B5_PORT,    LCD_B5_PIN,    LCD_B5_AF    },
      { LCD_B6_PORT,    LCD_B6_PIN,    LCD_B6_AF    },
      { LCD_B7_PORT,    LCD_B7_PIN,    LCD_B7_AF    },
   };


   GPIO_InitTypeDef gpio;
   gpio.Mode  = GPIO_MODE_AF_PP;
   gpio.Pull  = GPIO_NOPULL;
   gpio.Speed = GPIO_SPEED_FREQ_HIGH;

   for (size_t i = 0; i < (sizeof(pins) / sizeof(pins[0])); i++) {
      gpio.Pin       = pins[i].pin;
      gpio.Alternate = pins[i].af;
      HAL_GPIO_Init(pins[i].port, &gpio);
   }
}

static void lcd_panel_init(void)
{
   /* Timing Configuration */
   hLtdcHandler.Init.HorizontalSync = (RK043FN48H_HSYNC - (uint16_t)1);
   hLtdcHandler.Init.VerticalSync = (RK043FN48H_VSYNC - (uint16_t)1);
   hLtdcHandler.Init.AccumulatedHBP = (RK043FN48H_HSYNC + RK043FN48H_HBP - (uint16_t)1);
   hLtdcHandler.Init.AccumulatedVBP = (RK043FN48H_VSYNC + RK043FN48H_VBP - (uint16_t)1);
   hLtdcHandler.Init.AccumulatedActiveH = (RK043FN48H_HEIGHT + RK043FN48H_VSYNC + RK043FN48H_VBP - (uint16_t)1);
   hLtdcHandler.Init.AccumulatedActiveW = (RK043FN48H_WIDTH + RK043FN48H_HSYNC + RK043FN48H_HBP - (uint16_t)1);
   hLtdcHandler.Init.TotalHeigh = (RK043FN48H_HEIGHT + RK043FN48H_VSYNC + RK043FN48H_VBP +
	 RK043FN48H_VFP - (uint16_t)1);
   hLtdcHandler.Init.TotalWidth = (RK043FN48H_WIDTH + RK043FN48H_HSYNC + RK043FN48H_HBP +
	 RK043FN48H_HFP - (uint16_t)1);

   /* Background value */
   hLtdcHandler.Init.Backcolor.Blue = 0;
   hLtdcHandler.Init.Backcolor.Green = 0;
   hLtdcHandler.Init.Backcolor.Red = 0;

   /* Polarity */
   hLtdcHandler.Init.HSPolarity = LTDC_HSPOLARITY_AL;
   hLtdcHandler.Init.VSPolarity = LTDC_VSPOLARITY_AL;
   hLtdcHandler.Init.DEPolarity = LTDC_DEPOLARITY_AL;
   hLtdcHandler.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
   hLtdcHandler.Instance = LTDC;

   if (HAL_LTDC_GetState(&hLtdcHandler) == HAL_LTDC_STATE_RESET) {
      IRQ_SetPriority(LTDC_IRQn, 0);
      IRQ_Enable(LTDC_IRQn);

      lcd_panel_pin_setup();

      __HAL_RCC_LTDC_CLK_ENABLE();
      __HAL_RCC_LTDC_FORCE_RESET();
      __HAL_RCC_LTDC_RELEASE_RESET();
   }

   if (HAL_LTDC_Init(&hLtdcHandler) != HAL_OK) {
      my_printf("HAL_LTDC_Init() != HAL_OK\r\n");
      return;
   }

   // image layer
   LTDC_LayerCfgTypeDef layer_cfg;
   layer_cfg.WindowX0 = 0;
   layer_cfg.WindowX1 = IMAGE_HOR_SIZE;
   layer_cfg.WindowY0 = 0;
   layer_cfg.WindowY1 = IMAGE_VER_SIZE;
   layer_cfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB888;
   layer_cfg.FBStartAdress = (uint32_t)0xc0008000; // TODO: was ST_logo_RGB888;
   layer_cfg.Alpha = 255;
   layer_cfg.Alpha0 = 0;
   layer_cfg.Backcolor.Blue = 0;
   layer_cfg.Backcolor.Green = 0;
   layer_cfg.Backcolor.Red = 0;
   layer_cfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
   layer_cfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
   layer_cfg.ImageWidth = RK043FN48H_WIDTH;
   layer_cfg.ImageHeight = RK043FN48H_HEIGHT;
   layer_cfg.HorMirrorEn = 0;
   layer_cfg.VertMirrorEn = 0;
   HAL_LTDC_ConfigLayer(&hLtdcHandler, &layer_cfg, LTDC_LAYER_1);
   __HAL_LTDC_DISABLE_IT(&hLtdcHandler, LTDC_IT_FU); // no FIFO underrun IRQ

   // LCD_DISP pin
   GPIO_InitTypeDef gpio;
   gpio.Pin   = GPIO_PIN_7;
   gpio.Mode  = GPIO_MODE_OUTPUT_PP;
   gpio.Pull  = GPIO_NOPULL;
   gpio.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOG, &gpio);
   HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, GPIO_PIN_SET);
}

void lcd_init(void)
{
   lcd_backlight_init();
   lcd_panel_init();
   ctp_init();
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
