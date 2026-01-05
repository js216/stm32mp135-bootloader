// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file sd.c
 * @brief SD card management
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#include "sd.h"
#include "cmsis_gcc.h"
#include "debug.h"
#include "defaults.h"
#include "irq.h"
#include "irq_ctrl.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_gpio.h"
#include "stm32mp13xx_hal_gpio_ex.h"
#include "stm32mp13xx_hal_rcc.h"
#include "stm32mp13xx_hal_sd.h"
#include "stm32mp13xx_ll_sdmmc.h"
#include <inttypes.h>
#include <stdint.h>

// global variables
SD_HandleTypeDef sd_handle;

// cppcheck-suppress unusedFunction
void SDMMC1_IRQHandler(void)
{
   HAL_SD_IRQHandler(&sd_handle);
}

void sd_init(void)
{
   /* Enable and reset SDMMC Peripheral Clock */
   __HAL_RCC_SDMMC1_CLK_ENABLE();
   __HAL_RCC_SDMMC1_FORCE_RESET();
   __HAL_RCC_SDMMC1_RELEASE_RESET();

   /* Enable GPIOs clock */
   __HAL_RCC_GPIOC_CLK_ENABLE();
   __HAL_RCC_GPIOD_CLK_ENABLE();

   /* Common GPIO configuration */
   GPIO_InitTypeDef gpioi;
   gpioi.Mode  = GPIO_MODE_AF_PP;
   gpioi.Speed = GPIO_SPEED_FREQ_HIGH;

   /* D0 D1 D2 D3 CK on PC8 PC9 PC10 PC11 PC12 - AF12 PULLUP */
   gpioi.Pull      = GPIO_PULLUP;
   gpioi.Alternate = GPIO_AF12_SDIO1;
   gpioi.Pin =
       GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
   HAL_GPIO_Init(GPIOC, &gpioi);

   /* CMD on PD2 - AF12 NOPULL since there's an external pullup */
   gpioi.Pull      = GPIO_NOPULL;
   gpioi.Alternate = GPIO_AF12_SDIO1;
   gpioi.Pin       = GPIO_PIN_2;
   HAL_GPIO_Init(GPIOD, &gpioi);

   // SD interrupts
   IRQ_SetPriority(SDMMC1_IRQn, PRIO_SD);
   IRQ_Enable(SDMMC1_IRQn);

   // initialize SDMMC1
   sd_handle.Instance = SDMMC1;
   HAL_SD_DeInit(&sd_handle);

   // SDMMC IP setup
   sd_handle.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
   sd_handle.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
   sd_handle.Init.BusWide             = SDMMC_BUS_WIDE_4B;
   sd_handle.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
   sd_handle.Init.ClockDiv            = 4;

   if (HAL_SD_Init(&sd_handle) != HAL_OK) {
      ERROR("HAL_SD_Init");
   }

   while (HAL_SD_GetCardState(&sd_handle) != HAL_SD_CARD_TRANSFER)
      ;
}

void sd_read(uint32_t lba, uint32_t num_blocks, uint32_t dest_addr)
{
   if (num_blocks == 0)
      num_blocks = 1;

   if (dest_addr < DRAM_MEM_BASE)
      dest_addr = DRAM_MEM_BASE;

   my_printf("Copying %" PRIu32 " blocks from LBA %" PRIu32
             " to DDR addr 0x%" PRIX32 " ...\r\n",
             num_blocks, lba, dest_addr);

   __disable_irq();

   if (HAL_SD_ReadBlocks(&sd_handle, (uint8_t *)dest_addr, lba, num_blocks,
                         10000) != HAL_OK) {
      ERROR("Error in HAL_SD_ReadBlocks()");
   }

   while (sd_handle.State != HAL_SD_STATE_READY)
      ; // wait

   while (HAL_SD_GetCardState(&sd_handle) != HAL_SD_CARD_TRANSFER)
      ; // wait

   __enable_irq();
}

void load_sd_cmd(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   uint32_t n   = DEF_LINUX_LEN;
   uint32_t lba = DEF_LINUX_BLK;

   if (argc >= 1)
      n = arg1;

   if (argc >= 2)
      lba = arg2;

   sd_read(lba, n, arg3);
}
