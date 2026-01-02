// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file boot.c
 * @brief Bootloading procedures
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#include "boot.h"
#include "defaults.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include <stddef.h>
#include <stdint.h>

extern void handoff_jump(void (*app_entry)(void));

static void gic_clear(void)
{
   const int num_reg = 5;
   for (uint32_t n = 0; n <= num_reg; n++) {
      // disable interrupts
      GICDistributor->ICENABLER[n] = 0xffffffff;

      // make interrupts non-pending
      GICDistributor->ICPENDR[n] = 0xffffffff;

      // all interrupts should be Group 1 (non-secure)
      GICDistributor->IGROUPR[n] = 0xffffffff;
   }
}

void boot_jump(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)(arg2);
   (void)(arg3);
   uint32_t addr = DEF_LINUX_ADDR;

   if ((argc == 1) && (arg1 >= DEF_LINUX_ADDR))
      addr = arg1;

   my_printf("Jumping to app...\r\n");

   // Disable IRQ and FIQ
   __asm volatile("cpsid if\n" ::: "memory");

   MMU_InvalidateTLB();
   MMU_Disable();
   L1C_CleanDCacheAll();
   L1C_InvalidateICacheAll();
   L1C_DisableCaches();

   gic_clear();

   void (*app_entry)(void) = (void (*)(void))(addr);
   handoff_jump(app_entry);
}
