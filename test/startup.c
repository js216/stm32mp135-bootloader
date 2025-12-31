/**
 ******************************************************************************
 * @file    startup_stm32mp135fxx_ca7.c
 * @author  MCD Application Team
 * @brief   CMSIS Cortex-A7 Device Peripheral Access Layer A7 Startup source
 * file for GCC toolchain This file :
 *      - Sets up initial PC => Reset Handler
 *      - Puts all A7 cores except first one on "Wait for interrupt"
 *      - Configures cores
 *      - Sets up Exception vectors
 *      - Sets up stacks for all exception modes
 *      - Call SystemInit to initialize IRQs, caches, MMU, ...
 *
 ******************************************************************************
 * Copyright (c) 2009-2018 ARM Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023-2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "stm32mp135fxx_ca7.h"
#include <stddef.h>

#define USR_MODE 0x10 /* User mode */
#define FIQ_MODE 0x11 /* Fast Interrupt Request mode */
#define IRQ_MODE 0x12 /* Interrupt Request mode */
#define SVC_MODE 0x13 /* Supervisor mode */
#define ABT_MODE 0x17 /* Abort mode */
#define UND_MODE 0x1B /* Undefined Instruction mode */
#define SYS_MODE 0x1F /* System mode */

enum {
   PRIO_STPMIC = 0,
   PRIO_SD     = 1,
   PRIO_USB    = 5,
   PRIO_UART   = 7,
   PRIO_TICK   = 15,
};

extern int main(void);

void SystemInit(void);
void reset_handler(void) __attribute__((naked, target("arm")));
void vectors(void) __attribute__((naked, section("RESET")));

void fiq_handler(void);
void rsvd_handler(void);
void dabt_handler(void);
void pabt_handler(void);
void svc_handler(void);
void undef_handler(void);
void irq_handler(void);

void undef_handler(void)
{
   while (1)
      ;
}

void svc_handler(void)
{
   while (1)
      ;
}

void pabt_handler(void)
{
   while (1)
      ;
}

void dabt_handler(void)
{
   while (1)
      ;
}

void rsvd_handler(void)
{
   while (1)
      ;
}

void fiq_handler(void)
{
   while (1)
      ;
}

void irq_handler(void)
{
   while (1)
      ;
}

void vectors(void)
{
   __asm__ volatile(".align 7                                         \n"
                    "LDR    PC, =reset_handler                        \n"
                    "LDR    PC, =undef_handler                        \n"
                    "LDR    PC, =svc_handler                          \n"
                    "LDR    PC, =pabt_handler                         \n"
                    "LDR    PC, =dabt_handler                         \n"
                    "LDR    PC, =rsvd_handler                         \n"
                    "LDR    PC, =irq_handler                          \n"
                    "LDR    PC, =fiq_handler                          \n");
}

static void __attribute__((noinline)) ZeroBss(void)
{
   __asm volatile("PUSH {R4-R11}          \n"
                  "LDR r2, =ZI_DATA       \n"
                  "b LoopFillZerobss      \n"
                  /* Zero fill the bss segment. */
                  "FillZerobss:           \n"
                  "MOVS r3, #0            \n"
                  "STR  r3, [r2]          \n"
                  "adds r2, r2, #4        \n"

                  "LoopFillZerobss:       \n"
                  "LDR r3, = __BSS_END__  \n"
                  "CMP r2, r3             \n"
                  "BCC FillZerobss        \n"

                  "DSB                    \n"
                  "POP    {R4-R11}        ");
}

void SystemInit(void)
{
   /* Fill BSS Section with '0' */
   ZeroBss();

   /* Invalidate entire Unified TLB */
   MMU_InvalidateTLB();

   /* Disable all interrupts and events */
   EXTI_C1->IMR1 = 0;
   EXTI_C1->IMR2 = 0;
   EXTI_C1->IMR3 = 0;
   EXTI_C1->EMR1 = 0;
   EXTI_C1->EMR2 = 0;
   EXTI_C1->EMR3 = 0;

   /* Invalidate entire branch predictor array */
   L1C_InvalidateBTAC();

   /*  Invalidate instruction cache and flush branch target cache */
   L1C_InvalidateICacheAll();

   /*  Invalidate data cache */
   L1C_InvalidateDCacheAll();

#if ((__FPU_PRESENT == 1) && (__FPU_USED == 1))
   /* Enable FPU */
   __FPU_Enable();
#endif

   /* Enable Caches */
#ifdef CACHE_USE
   L1C_EnableCaches();
#endif
   L1C_EnableBTAC();
}

void reset_handler(void)
{
   main();
}
