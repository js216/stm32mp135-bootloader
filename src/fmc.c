// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file fmc.c
 * @brief FMC NAND management
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "fmc.h"
#include "irq_ctrl.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_gpio.h"
#include "stm32mp13xx_hal_gpio_ex.h"
#include "stm32mp13xx_hal_nand.h"
#include "stm32mp13xx_hal_rcc.h"
#include "stm32mp13xx_ll_fmc.h"
#include <stdint.h>

// global variables
static NAND_HandleTypeDef hnand;

static inline void setupgpio(GPIO_TypeDef *Gpio, GPIO_InitTypeDef *Init,
                             uint32_t Pin)
{
   /* Setup GPIO options */
   Init->Pin = Pin;
   HAL_GPIO_Init(Gpio, Init);
}

void fmc_init(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;

   /* Enable FMC clock */
   __HAL_RCC_FMC_CLK_ENABLE();
   __HAL_RCC_FMC_FORCE_RESET();
   __HAL_RCC_FMC_RELEASE_RESET();

   /* Enable MDMA controller clock */
   __HAL_RCC_MDMA_CLK_ENABLE();

   /* Enable GPIOs clock */
   __HAL_RCC_GPIOA_CLK_ENABLE();
   __HAL_RCC_GPIOB_CLK_ENABLE();
   __HAL_RCC_GPIOD_CLK_ENABLE();
   __HAL_RCC_GPIOE_CLK_ENABLE();
   __HAL_RCC_GPIOG_CLK_ENABLE();

   /* Common GPIO configuration */
   GPIO_InitTypeDef gpio_init_structure;
   gpio_init_structure.Mode  = GPIO_MODE_AF_PP;
   gpio_init_structure.Pull  = GPIO_PULLUP;
   gpio_init_structure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

   /* STM32MP135 pins: */
   gpio_init_structure.Alternate = GPIO_AF10_FMC;
   setupgpio(GPIOA, &gpio_init_structure, GPIO_PIN_9); /* FMC_NWAIT: PA9 */
   gpio_init_structure.Alternate = GPIO_AF12_FMC;
   setupgpio(GPIOG, &gpio_init_structure, GPIO_PIN_9);  /* FMC_NCE: PG9 */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_4);  /* FMC_NOE: PD4 */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_5);  /* FMC_NWE: PD5 */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_12); /* FMC_ALE: PD12 */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_11); /* FMC_CLE: PD11 */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_14); /* FMC_D0: PD14 */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_15); /* FMC_D1: PD15 */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_0);  /* FMC_D2: PD0 */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_1);  /* FMC_D3: PD1 */
   setupgpio(GPIOE, &gpio_init_structure, GPIO_PIN_7);  /* FMC_D4: PE7 */
   setupgpio(GPIOE, &gpio_init_structure, GPIO_PIN_8);  /* FMC_D5: PE8 */
   setupgpio(GPIOE, &gpio_init_structure, GPIO_PIN_9);  /* FMC_D6: PE9 */
   setupgpio(GPIOE, &gpio_init_structure, GPIO_PIN_10); /* FMC_D7: PE10 */

   /* Enable and set interrupt to the lowest priority */
   IRQ_SetPriority(FMC_IRQn, 0x0F);
   IRQ_Enable(FMC_IRQn);

   /* hnand Init */
   hnand.Instance = FMC_NAND_DEVICE;
   hnand.Init.NandBank =
       FMC_NAND_BANK3; /* Bank 3 is the only available with STM32MP135 */
   hnand.Init.Waitfeature =
       FMC_NAND_WAIT_FEATURE_ENABLE; /* Waiting enabled when communicating with
                                        the NAND */
   hnand.Init.MemoryDataWidth =
       FMC_NAND_MEM_BUS_WIDTH_8; /* An 8-bit NAND is used */
   hnand.Init.EccComputation =
       FMC_NAND_ECC_DISABLE; /* The HAL enable ECC computation when needed, keep
                                it disabled at initialization */
   hnand.Init.EccAlgorithm =
       FMC_NAND_ECC_ALGO_BCH; /* Hamming or BCH algorithm */
   hnand.Init.BCHMode =
       FMC_NAND_BCH_8BIT; /* BCH4 or BCH8 if BCH algorithm is used */
   hnand.Init.EccSectorSize =
       FMC_NAND_ECC_SECTOR_SIZE_512BYTE; /* BCH works only with 512-byte sectors
                                          */
   hnand.Init.TCLRSetupTime = 2;
   hnand.Init.TARSetupTime  = 2;

   /* hnand Config */
   hnand.Config.PageSize           = 4096; // bytes
   hnand.Config.SpareAreaSize      = 256;  // bytes
   hnand.Config.BlockSize          = 64;   // pages
   hnand.Config.BlockNbr           = 4096; // blocks
   hnand.Config.PlaneSize          = 1024; // blocks
   hnand.Config.PlaneNbr           = 2;    // planes
   hnand.Config.ExtraCommandEnable = 1;

   /* comspacetiming */
   FMC_NAND_PCC_TimingTypeDef comspacetiming = {0};
   comspacetiming.SetupTime                  = 0x1;
   comspacetiming.WaitSetupTime              = 0x7;
   comspacetiming.HoldSetupTime              = 0x2;
   comspacetiming.HiZSetupTime               = 0x1;

   /* attspacetiming */
   FMC_NAND_PCC_TimingTypeDef attspacetiming = {0};
   attspacetiming.SetupTime                  = 0x1A;
   attspacetiming.WaitSetupTime              = 0x7;
   attspacetiming.HoldSetupTime              = 0x6A;
   attspacetiming.HiZSetupTime               = 0x1;

   /* Initialize NAND HAL */
   if (HAL_NAND_Init(&hnand, &comspacetiming, &attspacetiming) != HAL_OK) {
      my_printf("HAL_NAND_Init() != HAL_OK\r\n");
      return;
   }

   /* Initialize NAND HAL ECC computations */
   NAND_EccConfigTypeDef eccconfig = {0};
   eccconfig.Offset                = 2;
   if (HAL_NAND_ECC_Init(&hnand, &eccconfig) != HAL_OK) {
      my_printf("HAL_NAND_ECC_Init() != HAL_OK\r\n");
      return;
   }

   /* Reset NAND device */
   if (HAL_NAND_Reset(&hnand) != HAL_OK) {
      my_printf("HAL_NAND_Reset() != HAL_OK\r\n");
      return;
   }

   /* Read & check the NAND device IDs*/
   NAND_IDTypeDef pnand_id;
   pnand_id.Maker_Id  = 0x00;
   pnand_id.Device_Id = 0x00;
   pnand_id.Third_Id  = 0x00;
   pnand_id.Fourth_Id = 0x00;
   if (HAL_NAND_Read_ID(&hnand, &pnand_id) != HAL_OK) {
      my_printf("HAL_NAND_Read_ID() != HAL_OK\r\n");
      return;
   }

   /* Test the NAND ID correctness */
   if ((pnand_id.Maker_Id != 0x2c) || (pnand_id.Device_Id != 0xd3) ||
       (pnand_id.Third_Id != 0x90) || (pnand_id.Fourth_Id != 0xa6)) {
      my_printf("Unexpected ID read:\r\n");
      my_printf("maker=0x%x, dev=0x%x, 3rd=0x%x, 4th=0x%x\r\n",
                pnand_id.Maker_Id, pnand_id.Device_Id, pnand_id.Third_Id,
                pnand_id.Fourth_Id);
      return;
   }
}
