// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file main.c
 * @brief Application entry point
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#include "board.h"
#include "cmd.h"
#include "ddr.h"
#include "eth.h"
#include "fmc.h"
#include "setup.h"
#include "stm32mp135fxx_ca7.h"
#include "stm32mp13xx_hal.h"
#include "stm32mp13xx_hal_gpio.h"
#include <stdint.h>

#ifndef NAND_FLASH
#include "sd.h"
#endif

#ifdef LCD_DISPLAY
#include "lcd.h"
#endif

#ifndef BOOT_TIMEOUT
#define BOOT_TIMEOUT 3
#endif

static void blink(void)
{
   static uint32_t last_blink = 0;
   uint32_t now               = HAL_GetTick();
   if (now - last_blink >= 1000) {
      last_blink = now;
      HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_13);
   }
}

int main(void)
{
   HAL_Init();
   sysclk_init();
   pmic_init();
   perclk_init();
   uart4_init();
   etzpc_init();
   gic_init();
   gpio_init();
   ddr_init();
   mmu_init();
#ifndef NAND_FLASH
   sd_init();
#endif
#ifdef NAND_FLASH
   fmc_init(0, 0, 0, 0);
#endif
#ifdef LCD_DISPLAY
   lcd_init();
#endif
   blink();

   cmd_init();
   cmd_autoboot();

   eth_init();
   usb_init();

   while (1) {
      cmd_poll();
      blink();
   }

   return 0;
}

// end file main.c
