// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file lcd.c
 * @brief LCD display and CTP touch control.
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "lcd.h"
#include "board.h"
#include "ctp.h"
#include "irq.h"
#include "irq_ctrl.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_gpio.h"
#include "stm32mp13xx_hal_gpio_ex.h"
#include "stm32mp13xx_hal_ltdc.h"
#include "stm32mp13xx_hal_rcc.h"
#include "stm32mp13xx_hal_tim.h"
#include "stm32mp13xx_hal_tim_ex.h"
#include <stddef.h>
#include <stdint.h>

struct lcd_pin_cfg {
   GPIO_TypeDef *port;
   uint16_t pin;
   uint32_t af;
};

/* global variables */
static TIM_HandleTypeDef htim1;
static LTDC_HandleTypeDef hltdchandler;

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

   htim1.Instance               = TIM1;
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
       {LCD_CLK_PORT,   LCD_CLK_PIN,   LCD_CLK_AF  },
       {LCD_HSYNC_PORT, LCD_HSYNC_PIN, LCD_HSYNC_AF},
       {LCD_VSYNC_PORT, LCD_VSYNC_PIN, LCD_VSYNC_AF},
       {LCD_DE_PORT,    LCD_DE_PIN,    LCD_DE_AF   },
       {LCD_R3_PORT,    LCD_R3_PIN,    LCD_R3_AF   },
       {LCD_R4_PORT,    LCD_R4_PIN,    LCD_R4_AF   },
       {LCD_R5_PORT,    LCD_R5_PIN,    LCD_R5_AF   },
       {LCD_R6_PORT,    LCD_R6_PIN,    LCD_R6_AF   },
       {LCD_R7_PORT,    LCD_R7_PIN,    LCD_R7_AF   },
       {LCD_G2_PORT,    LCD_G2_PIN,    LCD_G2_AF   },
       {LCD_G3_PORT,    LCD_G3_PIN,    LCD_G3_AF   },
       {LCD_G4_PORT,    LCD_G4_PIN,    LCD_G4_AF   },
       {LCD_G5_PORT,    LCD_G5_PIN,    LCD_G5_AF   },
       {LCD_G6_PORT,    LCD_G6_PIN,    LCD_G6_AF   },
       {LCD_G7_PORT,    LCD_G7_PIN,    LCD_G7_AF   },
       {LCD_B3_PORT,    LCD_B3_PIN,    LCD_B3_AF   },
       {LCD_B4_PORT,    LCD_B4_PIN,    LCD_B4_AF   },
       {LCD_B5_PORT,    LCD_B5_PIN,    LCD_B5_AF   },
       {LCD_B6_PORT,    LCD_B6_PIN,    LCD_B6_AF   },
       {LCD_B7_PORT,    LCD_B7_PIN,    LCD_B7_AF   },
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
   hltdchandler.Init.HorizontalSync = (LCD_HSYNC - (uint16_t)1);
   hltdchandler.Init.VerticalSync   = (LCD_VSYNC - (uint16_t)1);
   hltdchandler.Init.AccumulatedHBP = (LCD_HSYNC + LCD_HBP - (uint16_t)1);
   hltdchandler.Init.AccumulatedVBP = (LCD_VSYNC + LCD_VBP - (uint16_t)1);
   hltdchandler.Init.AccumulatedActiveH =
       (LCD_HEIGHT + LCD_VSYNC + LCD_VBP - (uint16_t)1);
   hltdchandler.Init.AccumulatedActiveW =
       (LCD_WIDTH + LCD_HSYNC + LCD_HBP - (uint16_t)1);
   hltdchandler.Init.TotalHeigh =
       (LCD_HEIGHT + LCD_VSYNC + LCD_VBP + LCD_VFP - (uint16_t)1);
   hltdchandler.Init.TotalWidth =
       (LCD_WIDTH + LCD_HSYNC + LCD_HBP + LCD_HFP - (uint16_t)1);

   /* Background value */
   hltdchandler.Init.Backcolor.Blue  = 0;
   hltdchandler.Init.Backcolor.Green = 0;
   hltdchandler.Init.Backcolor.Red   = 0;

   /* Polarity */
   hltdchandler.Init.HSPolarity = LTDC_HSPOLARITY_AL;
   hltdchandler.Init.VSPolarity = LTDC_VSPOLARITY_AL;
   hltdchandler.Init.DEPolarity = LTDC_DEPOLARITY_AL;
   hltdchandler.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
   hltdchandler.Instance        = LTDC;

   if (HAL_LTDC_GetState(&hltdchandler) == HAL_LTDC_STATE_RESET) {
      IRQ_SetPriority(LTDC_IRQn, PRIO_LTDC);
      IRQ_Enable(LTDC_IRQn);

      lcd_panel_pin_setup();

      __HAL_RCC_LTDC_CLK_ENABLE();
      __HAL_RCC_LTDC_FORCE_RESET();
      __HAL_RCC_LTDC_RELEASE_RESET();
   }

   if (HAL_LTDC_Init(&hltdchandler) != HAL_OK) {
      my_printf("HAL_LTDC_Init() != HAL_OK\r\n");
      return;
   }

   // image layer
   LTDC_LayerCfgTypeDef layer_cfg;
   layer_cfg.WindowX0        = 0;
   layer_cfg.WindowX1        = LCD_WIDTH;
   layer_cfg.WindowY0        = 0;
   layer_cfg.WindowY1        = LCD_HEIGHT;
   layer_cfg.PixelFormat     = LTDC_PIXEL_FORMAT_RGB888;
   layer_cfg.FBStartAdress   = DRAM_MEM_BASE;
   layer_cfg.Alpha           = 255;
   layer_cfg.Alpha0          = 0;
   layer_cfg.Backcolor.Blue  = 0;
   layer_cfg.Backcolor.Green = 0;
   layer_cfg.Backcolor.Red   = 0;
   layer_cfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
   layer_cfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
   layer_cfg.ImageWidth      = LCD_WIDTH;
   layer_cfg.ImageHeight     = LCD_HEIGHT;
   layer_cfg.HorMirrorEn     = 0;
   layer_cfg.VertMirrorEn    = 0;
   HAL_LTDC_ConfigLayer(&hltdchandler, &layer_cfg, LTDC_LAYER_1);
   __HAL_LTDC_DISABLE_IT(&hltdchandler, LTDC_IT_FU); // no FIFO underrun IRQ

   // LCD_DISP pin
   GPIO_InitTypeDef gpio;
   gpio.Pin   = LCD_DISP_PIN;
   gpio.Mode  = GPIO_MODE_OUTPUT_PP;
   gpio.Pull  = GPIO_NOPULL;
   gpio.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(LCD_DISP_PORT, &gpio);
   HAL_GPIO_WritePin(LCD_DISP_PORT, LCD_DISP_PIN, GPIO_PIN_SET);
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
         htim1.Instance->CCR3 = (htim1.Init.Period + 1U) * arg1 / 100U;
      }
   }
}

void lcd_color(int argc, uint32_t r, uint32_t g, uint32_t b)
{
   (void)argc;
   volatile uint8_t *lcd_fb = (volatile uint8_t *)DRAM_MEM_BASE;

   for (uint32_t y = 0; y < LCD_HEIGHT; y++) {
      for (uint32_t x = 0; x < LCD_WIDTH; x++) {
         uint32_t p    = (y * LCD_WIDTH + x) * 3U;
         lcd_fb[p + 0] = b; // blue
         lcd_fb[p + 1] = g; // green
         lcd_fb[p + 2] = r; // red
      }
   }

   /* make sure CPU writes reach DDR before LTDC reads */
   L1C_CleanDCacheAll();
}

// end file lcd.c
