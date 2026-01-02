// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file ddr.c
 * @brief DDR RAM management
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#include "ddr.h"
#include "debug.h"
#include "defaults.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include "stm32mp13xx_hal_ddr.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_rcc.h"
#include <inttypes.h>
#include <stdint.h>

void ddr_init(void)
{
   // MCE and TZC config
   __HAL_RCC_MCE_CLK_ENABLE();
   __HAL_RCC_TZC_CLK_ENABLE();

   // configure TZC to allow DDR Region0 R/W non secure for all IDs
   TZC->GATE_KEEPER     = 0;
   TZC->REG_ID_ACCESSO  = 0xFFFFFFFF;
   TZC->REG_ATTRIBUTESO = 0xC0000001;
   TZC->GATE_KEEPER |= 1U;

   // enable BACKUP SRAM for security
   __HAL_RCC_BKPSRAM_CLK_ENABLE();

   // unlock debugger
   BSEC->BSEC_DENABLE = 0x47f;

   // enable clock debug CK_DBG
   RCC->DBGCFGR |= RCC_DBGCFGR_DBGCKEN;

   // init DDR
   static DDR_InitTypeDef hddr;
   hddr.wakeup_from_standby = false;
   hddr.self_refresh        = false;
   hddr.zdata               = 0;
   hddr.clear_bkp           = false;

   if (HAL_DDR_Init(&hddr) != HAL_OK)
      ERROR("DDR Init");
}

static void ddr_print(uint32_t addr, uint32_t num_words)
{
   for (uint32_t i = 0; i < num_words / 4; i += 4) {
      // print address
      my_printf("0x%08" PRIx32 " : ", 4 * i);

      // print in hex
      for (int j = 0; j < 4; j++) {
         const volatile uint32_t *p = (uint32_t *)(addr + (4 * (i + j)));
         const uint32_t i0          = *p;
         for (int k = 0; k < 4; k++) {
            const char c = (i0 & (0XFFU << k * 8U)) >> k * 8U;
            my_printf("%02x ", c);
         }
         my_printf(" ");
      }

      // print as ASCII
      for (int j = 0; j < 4; j++) {
         const volatile uint32_t *p = (uint32_t *)(addr + (4 * (i + j)));
         const uint32_t i0          = *p;
         for (int k = 0; k < 4; k++) {
            const char c = (i0 & (0XFFU << k * 8U)) >> k * 8U;
            if (c >= 0x20 && c <= 0x7e)
               my_printf("%c", c);
            else
               my_printf(".");
         }
      }

      my_printf("\r\n");
   }
}

void ddr_print_cmd(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)arg3;

   uint32_t n    = DEF_PRINT_LEN;
   uint32_t addr = DEF_LINUX_ADDR;

   if ((argc >= 1) && (arg1 > 0))
      n = arg1;

   if ((argc == 2) && (arg2 >= DEF_LINUX_ADDR))
      addr = arg2;

   ddr_print(addr, n);
}
