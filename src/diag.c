// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file diag.c
 * @brief Diagnostic tests
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#include "diag.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#define INFO "  INFO: "
#define OK   "    \033[32mOK\033[0m: "
#define WARN "  \033[33mWARN\033[0m: "

struct sctlr_bit {
   uint32_t mask;
   const char *msg_set;
   const char *prefix_set;
   const char *msg_unset;
   const char *prefix_unset;
};

static const struct sctlr_bit sctlr_map[] = {
    {1U << 0U,  "MMU enabled (M=1)",                WARN, "MMU disabled (M=0)",         OK  },
    {1U << 1U,  "alignment checking enabled (A=1)", INFO,
     "alignment checking disabled (A=0)",                                               INFO},
    {1U << 2U,  "D-cache enabled (C=1)",            WARN, "D-cache disabled (C=0)",     OK  },
    {1U << 11U, "branch prediction enabled (Z=1)",  INFO,
     "branch prediction disabled (Z=0)",                                                INFO},
    {1U << 12U, "I-cache enabled (I=1)",            WARN, "I-cache disabled (I=0)",     OK  },
    {1U << 13U, "high exception vectors (V=1)",     INFO,
     "low exception vectors (V=0)",                                                     INFO},
    {1U << 14U, "round-robin cache replacement",    INFO,
     "pseudo-random cache replacement",                                                 INFO},
    {1U << 15U, "loads may speculate (L4=1)",       INFO,
     "loads strongly ordered (L4=0)",                                                   INFO},
    {1U << 16U, "data TCM enabled (DT=1)",          INFO, "data TCM disabled (DT=0)",
     INFO                                                                                   },
    {1U << 18U, "instruction TCM enabled (IT=1)",   INFO,
     "instruction TCM disabled (IT=0)",                                                 INFO},
    {1U << 19U, "divide-by-zero traps enabled",     INFO,
     "divide-by-zero traps disabled",                                                   INFO},
    {1U << 10U, "SWP/SWPB enabled (SW=1)",          INFO, "SWP/SWPB disabled (SW=0)",
     INFO                                                                                   },
    {1U << 25U, "big-endian data access (EE=1)",    WARN,
     "little-endian data access (EE=0)",                                                OK  },
    {1U << 27U, "FIQs are non-maskable (NMFI=1)",   INFO,
     "FIQs are maskable (NMFI=0)",                                                      INFO},
    {1U << 28U, "TEX remap enabled (TRE=1)",        INFO, "TEX remap disabled (TRE=0)",
     INFO                                                                                   },
    {1U << 29U, "access flag enabled (AFE=1)",      INFO,
     "access flag disabled (AFE=0)",                                                    INFO},
    {1U << 30U, "exceptions taken in Thumb (TE=1)", WARN,
     "exceptions taken in ARM (TE=0)",                                                  OK  },
};

static const char *decprot_periph[64] = {
    /* DECPROT0 */
    "VREFBUF", "LPTIM2", "LPTIM3", "LTDC layer 2", "DCMIPP", "USBPHYC",
    "DDRCTRL/DDRPHYC", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "IWDG1", "STGENC", "Reserved", "Reserved",
    /* DECPROT1 */
    "USART1", "USART2", "SPI4", "SPI5", "I2C3", "I2C4", "I2C5", "TIM12",
    "TIM13", "TIM14", "TIM15", "TIM16", "TIM17", "Reserved", "Reserved",
    "Reserved",
    /* DECPROT2 */
    "ADC1", "ADC2", "OTG", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "RNG", "HASH", "CRYP", "SAES", "PKA", "BKPSRAM", "Reserved",
    "Reserved",
    /* DECPROT3 */
    "ETH1", "ETH2", "SDMMC1/DLBSD1", "SDMMC2/DLBSD2", "Reserved", "DDR MCE",
    "FMC", "QSPI/DLBQ", "Reserved", "Reserved", "Reserved", "Reserved",
    "SRAM1 MLAHB", "SRAM2 MLAHB", "SRAM3 MLAHB", "Reserved"};

static void cpsr_print_flags(uint32_t cpsr)
{
   char buf[16];
   size_t n = 0U;

   if ((cpsr & (1U << 31U)) != 0U)
      buf[n++] = 'N';

   if ((cpsr & (1U << 30U)) != 0U)
      buf[n++] = 'Z';

   if ((cpsr & (1U << 29U)) != 0U)
      buf[n++] = 'C';

   if ((cpsr & (1U << 28U)) != 0U)
      buf[n++] = 'V';

   if ((cpsr & (1U << 27U)) != 0U)
      buf[n++] = 'Q';

   buf[n] = '\0';

   if (n != 0U)
      my_printf(INFO "condition flags set: %s\r\n", buf);
   else
      my_printf(INFO "condition flags set: none\r\n");
}

static void cpsr_dump(void)
{
   uint32_t cpsr = 0;
   uint32_t mode = 0;
   uint32_t it   = 0;

   __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));

   my_printf(INFO "CPSR = 0x%08" PRIX32 "\r\n", cpsr);

   /* condition flags */
   cpsr_print_flags(cpsr);

   /* IT state (If-Then execution) */
   it = (cpsr >> 25U) & 0x3U;
   it <<= 6U;
   it |= (cpsr >> 10U) & 0x3FU;

   if (it == 0U)
      my_printf(OK "IT state inactive\r\n");
   else
      my_printf(WARN "IT state active (0x%02" PRIX32 ")\r\n", it);

   /* endianness */
   if ((cpsr & (1U << 9U)) != 0U)
      my_printf(WARN "big-endian data (E=1)\r\n");
   else
      my_printf(OK "little-endian data (E=0)\r\n");

   /* interrupt masks */
   if ((cpsr & (1U << 8U)) != 0U)
      my_printf(OK "asynchronous aborts masked (A=1)\r\n");
   else
      my_printf(WARN "asynchronous aborts enabled (A=0)\r\n");

   if ((cpsr & (1U << 7U)) != 0U)
      my_printf(OK "IRQs masked (I=1)\r\n");
   else
      my_printf(WARN "IRQs enabled (I=0)\r\n");

   if ((cpsr & (1U << 6U)) != 0U)
      my_printf(OK "FIQs masked (F=1)\r\n");
   else
      my_printf(WARN "FIQs enabled (F=0)\r\n");

   /* instruction set state */
   if ((cpsr & (1U << 5U)) == 0U)
      my_printf(OK "instruction set: ARM (T=0)\r\n");
   else
      my_printf(WARN "instruction set: Thumb (T=1)\r\n");

   /* processor mode */
   mode = cpsr & 0X1FU;

   if (mode == 0x13U)
      my_printf(OK "mode: SVC (0x13)\r\n");
   else if (mode == 0x10U)
      my_printf(WARN "mode: USR (0x10)\r\n");
   else if (mode == 0x11U)
      my_printf(WARN "mode: FIQ (0x11)\r\n");
   else if (mode == 0x12U)
      my_printf(WARN "mode: IRQ (0x12)\r\n");
   else if (mode == 0x17U)
      my_printf(WARN "mode: ABT (0x17)\r\n");
   else if (mode == 0x1BU)
      my_printf(WARN "mode: UND (0x1B)\r\n");
   else if (mode == 0X1FU)
      my_printf(WARN "mode: SYS (0x1F)\r\n");
   else
      my_printf(WARN "mode: unknown (0x%02" PRIX32 ")\r\n", mode);
}

static void sctlr_dump(void)
{
   uint32_t sctlr = 0;
   __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));

   my_printf(INFO "SCTLR = 0x%08" PRIX32 "\r\n", sctlr);

   for (size_t i = 0; i < (sizeof(sctlr_map) / sizeof(sctlr_map[0])); i++) {
      const struct sctlr_bit *bit = &sctlr_map[i];
      if ((sctlr & bit->mask) != 0U) {
         my_printf("%s%s\r\n", bit->prefix_set, bit->msg_set);
      } else {
         my_printf("%s%s\r\n", bit->prefix_unset, bit->msg_unset);
      }
   }

   /* Handle the special TLB status derived from MMU bit */
   if ((sctlr & (1U << 0U)) != 0U) {
      my_printf(WARN "TLBs active (MMU on)\r\n");
   } else {
      my_printf(OK "TLBs inactive (MMU off)\r\n");
   }
}

static void vbar_dump(void)
{
   uint32_t vbar = 0;

   __asm__ volatile("mrc p15, 0, %0, c12, c0, 0" : "=r"(vbar));

   my_printf(INFO "VBAR = 0x%08" PRIX32 "\r\n", vbar);

   /* check if base is non-zero */
   if (vbar != 0U)
      my_printf(OK "Vector table set (non-zero base)\r\n");
   else
      my_printf(WARN "VBAR is zero\r\n");

   /* check 32-byte alignment (low 5 bits must be zero) */
   if ((vbar & 0X1FU) == 0U)
      my_printf(OK "VBAR 32-byte aligned\r\n");
   else
      my_printf(WARN "VBAR misaligned (0x%08" PRIX32 ")\r\n", vbar);
}

static void mpidr_dump(void)
{
   uint32_t mpidr = 0;

   /* Read MPIDR directly */
   __asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));

   my_printf(INFO "MPIDR = 0x%08" PRIX32 "\r\n", mpidr);

   /* Multiprocessor extension (U) */
   if ((mpidr & (1U << 31U)) != 0U)
      my_printf(INFO "Multiprocessor extension (U=1) present\r\n");
   else
      my_printf(INFO "Multiprocessor extension (U=0) not present\r\n");

   /* Multi-threaded flag (MT) */
   if ((mpidr & (1U << 24U)) != 0U)
      my_printf(INFO "Multi-threaded CPU (MT=1)\r\n");
   else
      my_printf(INFO "Single-threaded CPU (MT=0)\r\n");

   /* Affinity level 3 */
   uint32_t aff3 = (mpidr >> 16U) & 0XFFU;
   my_printf(INFO "Affinity level 3 (Aff3) = %" PRIu32 "\r\n", aff3);

   /* Affinity level 2 */
   uint32_t aff2 = (mpidr >> 8U) & 0XFFU;
   my_printf(INFO "Affinity level 2 (Aff2) = %" PRIu32 "\r\n", aff2);

   /* Affinity level 1 / cluster ID */
   uint32_t aff1 = mpidr & 0XFFU;
   my_printf(INFO "Affinity level 1 / CPU ID (Aff1/Aff0) = %" PRIu32 "\r\n",
             aff1);

   /* Boot CPU detection */
   if (aff1 == 0U)
      my_printf(OK "Boot CPU\r\n");
   else
      my_printf(WARN "Non-boot CPU detected\r\n");
}

static void check_ddr_rw(void)
{
   volatile uint32_t *ptr = (uint32_t *)DRAM_MEM_BASE;
   uint32_t orig          = ptr[0];

   ptr[0] = 0xA5A55A5AU;

   if (ptr[0] == 0xA5A55A5AU)
      my_printf(OK "DDR write/read test passed\r\n");
   else
      my_printf(WARN "DDR readback mismatch\r\n");

   ptr[0] = orig;
}

static void check_sp_alignment(void)
{
   uint32_t sp = 0;

   __asm__ volatile("mov %0, sp" : "=r"(sp));

   if ((sp & 7U) == 0U)
      my_printf(OK "SP 8-byte aligned\r\n");
   else
      my_printf(WARN "SP not 8-byte aligned (sp=0x%08" PRIX32 ")\r\n", sp);
}

static void check_gic_dist(void)
{
   const volatile uint32_t *gicd = (uint32_t *)GIC_DISTRIBUTOR_BASE;
   uint32_t ctlr                 = gicd[0];

   if (ctlr & 1U)
      my_printf(OK "GIC Distributor enabled\r\n");
   else
      my_printf(WARN "GIC Distributor disabled\r\n");
}

static void check_gic_cpuif(void)
{
   const volatile uint32_t *gicc = (uint32_t *)GIC_INTERFACE_BASE;
   uint32_t ctlr                 = gicc[0];

   if (ctlr & 1U)
      my_printf(OK "GIC CPU interface enabled\r\n");
   else
      my_printf(WARN "GIC CPU interface disabled\r\n");
}

static void actlr_dump(void)
{
   uint32_t actlr = 0;

   /* Read ACTLR */
   __asm__ volatile("mrc p15, 0, %0, c1, c0, 1" : "=r"(actlr));

   my_printf(INFO "ACTLR = 0x%08" PRIX32 "\r\n", actlr);

   /* Bit 28: DDI */
   if ((actlr & (1U << 28U)) != 0U)
      my_printf(INFO "DDI (Disable dual issue) = 1 → dual issue disabled\r\n");
   else
      my_printf(
          INFO "DDI (Disable dual issue) = 0 → dual issue enabled (reset)\r\n");

   /* Bit 15: DDVM */
   if ((actlr & (1U << 15U)) != 0U)
      my_printf(INFO "DDVM (Disable DVM) = 1 → DVM disabled\r\n");
   else
      my_printf(INFO "DDVM (Disable DVM) = 0 → DVM enabled (reset)\r\n");

   /* Bits 14-13: L1PCTL */
   uint32_t l1pctl = (actlr >> 13U) & 0x3U;
   my_printf(INFO "L1PCTL (L1 data prefetch control) = 0b%02" PRIX32 " → ",
             l1pctl);
   switch (l1pctl) {
      case 0: my_printf("prefetch disabled\r\n"); break;
      case 1: my_printf("1 outstanding prefetch\r\n"); break;
      case 2: my_printf("2 outstanding prefetches\r\n"); break;
      case 3: my_printf("3 outstanding prefetches (reset)\r\n"); break;
      default: my_printf(WARN "malformed value\r\n"); break;
   }

   /* Bit 12: L1RADIS */
   if ((actlr & (1U << 12U)) != 0U)
      my_printf(INFO "L1RADIS = 1 → L1 data cache read-allocate disabled\r\n");
   else
      my_printf(
          INFO "L1RADIS = 0 → L1 data cache read-allocate enabled (reset)\r\n");

   /* Bit 11: L2RADIS */
   if ((actlr & (1U << 11U)) != 0U)
      my_printf(INFO "L2RADIS = 1 → L2 data cache read-allocate disabled\r\n");
   else
      my_printf(
          INFO "L2RADIS = 0 → L2 data cache read-allocate enabled (reset)\r\n");

   /* Bit 10: DODMBS */
   if ((actlr & (1U << 10U)) != 0U)
      my_printf(INFO "DODMBS = 1 → optimized DMB disabled\r\n");
   else
      my_printf(INFO "DODMBS = 0 → optimized DMB enabled (reset)\r\n");

   /* Bit 6: SMP */
   if ((actlr & (1U << 6U)) != 0U)
      my_printf(
          OK
          "SMP = 1 → coherent requests enabled (correct for multi-core)\r\n");
   else
      my_printf(WARN "SMP = 0 → coherent requests disabled (must be 1 before "
                     "enabling caches/MMU)\r\n");

   /* Bits 31-29, 27-16, 9-7, 5-0: reserved */
   uint32_t reserved_mask =
       ((0x7U << 29U) | (0xFFFU << 16U) | (0x7U << 7U) | 0x3FU) & actlr;
   if (reserved_mask != 0U)
      my_printf(
          INFO
          "Other reserved/implementation-defined ACTLR bits set: 0x%08" PRIX32
          "\r\n",
          reserved_mask);
}

static void check_cntvct(void)
{
   uint64_t a = 1;
   uint64_t b = 0;

   __asm__ volatile("mrrc p15, 1, %0, %H0, c14" : "=r"(a));
   for (volatile int i = 0; i < 1000; i++)
      ;
   __asm__ volatile("mrrc p15, 1, %0, %H0, c14" : "=r"(b));

   if (b > a)
      my_printf(OK "CNTVCT increases (%" PRIu64 " -> %" PRIu64 ")\r\n", a, b);
   else
      my_printf(WARN "CNTVCT not incrementing\r\n");
}

static uint32_t read_cntfrq(void)
{
   uint32_t v = 0;
   __asm__ volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(v));
   return v;
}

static void check_cntfrq(void)
{
   uint32_t f = read_cntfrq();
   my_printf(INFO "CNTFRQ = %" PRIu32 "\r\n", f);

   if (f > 1000000U)
      my_printf(OK "System counter frequency valid\r\n");
   else
      my_printf(WARN "CNTFRQ too low (maybe not initialized)\r\n");
}

static void midr_dump(void)
{
   uint32_t midr = 0;
   uint32_t val  = 0;

   /* Read MIDR */
   __asm__ volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(midr));

   my_printf(INFO "MIDR = 0x%08" PRIX32 "\r\n", midr);

   /* Bits [31:24]: Implementer */
   val = (midr >> 24U) & 0xFFU;
   my_printf(INFO "Implementer      = 0x%02" PRIX32, val);

   if (val == 0x41U)
      my_printf(" (ARM)\r\n");
   else
      my_printf(" (unknown)\r\n");

   /* Bits [23:20]: Variant */
   val = (midr >> 20U) & 0x0FU;
   my_printf(INFO "Variant           = %" PRIu32 "\r\n", val);

   /* Bits [19:16]: Architecture */
   val = (midr >> 16U) & 0x0FU;
   my_printf(INFO "Architecture      = 0x%" PRIX32, val);

   if (val == 0x0FU)
      my_printf(" (architectural)\r\n");
   else
      my_printf(" (pre-ARMv7 encoding)\r\n");

   /* Bits [15:4]: Part number */
   val = (midr >> 4U) & 0xFFFU;
   my_printf(INFO "Part number       = 0x%03" PRIX32 "\r\n", val);

   /* Bits [3:0]: Revision */
   val = midr & 0x0FU;
   my_printf(INFO "Revision          = %" PRIu32 "\r\n", val);
}

static void check_rcc_ns(void)
{
   if (RCC->SECCFGR == 0)
      my_printf(OK "RCC: all registers non-secure\r\n");
   else
      my_printf(WARN "RCC: secure bits set (0x%08" PRIX32 ")\r\n",
                RCC->SECCFGR);
}

static void check_rtc_ns(void)
{
   if (RTC->SECCFGR == 0)
      my_printf(OK "RTC: all registers non-secure\r\n");
   else
      my_printf(WARN "RTC: secure bits set (0x%08" PRIX32 ")\r\n",
                RTC->SECCFGR);
}

static void check_gpio_ns(void)
{
   GPIO_TypeDef *banks[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE,
                            GPIOF, GPIOG, GPIOH, GPIOI};
   for (unsigned int i = 0; i < sizeof(banks) / sizeof(banks[0]); i++) {
      if (banks[i]->SECCFGR == 0)
         my_printf(OK "GPIO%c: non-secure\r\n", (int)('A' + i));
      else
         my_printf(WARN "GPIO%c: secure bits set (0x%08" PRIX32 ")\r\n",
                   (int)('A' + i), banks[i]->SECCFGR);
   }
}

static void check_secure_world(void)
{
   uint32_t scr = 0;
   __asm__ volatile("mrc p15, 0, %0, c1, c1, 0" : "=r"(scr));

   if (scr & 1U)
      my_printf(OK "CPU in Non-secure mode (SCR.NS=1)\r\n");
   else
      my_printf(WARN "CPU still in Secure mode (SCR.NS=0)\r\n");
}

static void dump_tzc(void)
{
   uint32_t cfg       = TZC->BUILD_CONFIG;
   uint32_t action    = TZC->ACTION;
   uint32_t gk        = TZC->GATE_KEEPER;
   uint32_t spec      = TZC->SPECULATION_CTRL;
   uint32_t rbase_lo  = TZC->REG_BASE_LOWO;
   uint32_t rbase_hi  = TZC->REG_BASE_HIGHO;
   uint32_t rtop_lo   = TZC->REG_TOP_LOWO;
   uint32_t rtop_hi   = TZC->REG_TOP_HIGHO;
   uint32_t attr      = TZC->REG_ATTRIBUTESO;
   uint32_t id_access = TZC->REG_ID_ACCESSO;

   my_printf("[TZC dump] begin\r\n");

   /* BUILD_CONFIG interpretation */
   my_printf(INFO "BUILD_CONFIG     = 0x%08" PRIX32 "\r\n", cfg);
   my_printf(INFO "  Number of filters = %" PRIu32 "\r\n",
             ((cfg >> 24U) & 1U) + 1U);
   my_printf(INFO "  Address width     = %" PRIu32 " bits\r\n",
             (cfg >> 8U) & 0x3FU);
   my_printf(INFO "  Number of regions = %" PRIu32 "\r\n", (cfg & 0x1FU) + 1U);

   /* ACTION register interpretation */
   my_printf(INFO "ACTION           = 0x%08" PRIX32 "\r\n", action);
   switch (action & 0x3U) {
      case 0:
         my_printf(INFO "  Permission failure reaction: set tzcint low and "
                        "issue OKAY on the bus\r\n");
         break;
      case 1:
         my_printf(INFO "  Permission failure reaction: set tzcint low and "
                        "issue DECERR on the bus\r\n");
         break;
      case 2:
         my_printf(INFO "  Permission failure reaction: set tzcint high and "
                        "issue OKAY on the bus\r\n");
         break;
      case 3:
         my_printf(INFO "  Permission failure reaction: set tzcint high and "
                        "issue DECERR on the bus\r\n");
         break;
      default: my_printf(WARN "malformed value\r\n"); break;
   }

   /* GATE_KEEPER interpretation */
   my_printf(INFO "GATE_KEEPER      = 0x%08" PRIX32 "\r\n", gk);
   my_printf(INFO "  OPENREQ  = %" PRIu32 " → request filter to %s\r\n",
             gk & 1U, (gk & 1U) ? "close" : "open");
   my_printf(INFO "  OPENSTAT = %" PRIu32 " → filter is %s\r\n",
             (gk >> 16U) & 1U, ((gk >> 16U) & 1U) ? "closed" : "opened");

   /* SPECULATION_CTRL interpretation */
   my_printf(INFO "SPECULATION_CTRL = 0x%08" PRIX32 "\r\n", spec);
   my_printf(INFO "  Read speculation disabled  = %s\r\n",
             (spec & 1U) ? "yes" : "no");
   my_printf(INFO "  Write speculation disabled = %s\r\n",
             (spec & 2U) ? "yes" : "no");

   /* Region 0 base/top */
   my_printf(INFO "REG_BASE_LOWO    = 0x%08" PRIX32 "\r\n", rbase_lo);
   my_printf(INFO "REG_BASE_HIGHO   = 0x%08" PRIX32 "\r\n", rbase_hi);
   my_printf(INFO "REG_TOP_LOWO     = 0x%08" PRIX32 "\r\n", rtop_lo);
   my_printf(INFO "REG_TOP_HIGHO    = 0x%08" PRIX32 "\r\n", rtop_hi);
   if (rbase_lo == 0x0 && rtop_lo == 0xFFFFFFFFU && rbase_hi == 0 &&
       rtop_hi == 0)
      my_printf(OK "Region 0 covers full 32-bit address space\r\n");
   else
      my_printf(WARN "Region 0 does not cover full address space\r\n");

   /* Region 0 attributes */
   my_printf(INFO "REG_ATTRIBUTES0  = 0x%08" PRIX32 "\r\n", attr);
   if (attr & 1U)
      my_printf(OK "Region 0 filter enabled\r\n");
   else
      my_printf(WARN "Region 0 filter not enabled\r\n");

   my_printf(INFO "  Secure read  allowed = %s\r\n",
             (attr & (1U << 30U)) ? "yes" : "no");
   my_printf(INFO "  Secure write allowed = %s\r\n",
             (attr & (1U << 31U)) ? "yes" : "no");

   /* NSAID access */
   my_printf(INFO "REG_ID_ACCESS0   = 0x%08" PRIX32 "\r\n", id_access);
   if ((id_access & 0xFFFFU) == 0xFFFFU)
      my_printf(OK "All NSAID reads enabled\r\n");
   else
      my_printf(WARN "Some NSAID reads disabled\r\n");
   if ((id_access >> 16U) == 0xFFFFU)
      my_printf(OK "All NSAID writes enabled\r\n");
   else
      my_printf(WARN "Some NSAID writes disabled\r\n");

   my_printf("[TZC dump] end\r\n\n");
}

static void dump_etzpc(void)
{
   my_printf("[ETZPC dump] begin\r\n");

   /* Memory area sizes */
   my_printf(INFO "TZMA0_SIZE       = 0x%08" PRIX32 "\r\n", ETZPC->TZMA0_SIZE);
   my_printf(INFO "TZMA1_SIZE       = 0x%08" PRIX32 "\r\n", ETZPC->TZMA1_SIZE);

   /* DECPROT0–DECPROT3: 16 peripherals each, 2-bit fields */
   for (int x = 0; x <= 3; x++) {
      uint32_t reg = (&ETZPC->DECPROT0)[x];
      my_printf(INFO "DECPROT%d = 0x%08" PRIX32 "\r\n", x, reg);
      for (unsigned int y = 0; y < 16; y++) {
         unsigned int index = (16 * (unsigned int)x) + y;
         uint32_t field     = (reg >> (2U * y)) & 0x3U; /* 2-bit field */
         const char *name   = decprot_periph[index];
         switch (field) {
            case 0:
               my_printf(WARN "  %2u %-20s: read/write secure only (00)\r\n",
                         index, name);
               break;
            case 1:
               my_printf(WARN
                         "  %2u %-20s: read non-secure, write secure (01)\r\n",
                         index, name);
               break;
            case 2:
               my_printf(INFO "  %2u %-20s: reserved (10)\r\n", index, name);
               break;
            case 3:
               my_printf(OK "  %2u %-20s: fully non-secure (11)\r\n", index,
                         name);
               break;
            default: my_printf(WARN "malformed value\r\n"); break;
         }
      }
   }

   /* DECPROT4–DECPROT5: INFO only */
   my_printf(INFO "DECPROT4         = 0x%08" PRIX32 "\r\n", ETZPC->DECPROT4);
   my_printf(INFO "DECPROT5         = 0x%08" PRIX32 "\r\n", ETZPC->DECPROT5);

   /* DECPROT_LOCK registers: INFO only */
   my_printf(INFO "DECPROT_LOCK0    = 0x%08" PRIX32 "\r\n",
             ETZPC->DECPROT_LOCK0);
   my_printf(INFO "DECPROT_LOCK1    = 0x%08" PRIX32 "\r\n",
             ETZPC->DECPROT_LOCK1);
   my_printf(INFO "DECPROT_LOCK2    = 0x%08" PRIX32 "\r\n",
             ETZPC->DECPROT_LOCK2);

   /* Hardware info */
   my_printf(INFO "HWCFGR           = 0x%08" PRIX32 "\r\n", ETZPC->HWCFGR);
   my_printf(INFO "IP_VER           = 0x%08" PRIX32 "\r\n", ETZPC->IP_VER);
   my_printf(INFO "ID               = 0x%08" PRIX32 "\r\n", ETZPC->ID);
   my_printf(INFO "SID              = 0x%08" PRIX32 "\r\n", ETZPC->SID);

   my_printf("[ETZPC dump] end\r\n\n");
}

static void dump_reg6(const char *name, const volatile uint32_t *array,
                      unsigned int count)
{
   for (unsigned int i = 0; i < count; i += 6) {
      my_printf("  %s[%03u] =", name, i);
      for (unsigned int j = 0; j < 6 && i + j < count; j++) {
         my_printf(" 0x%08" PRIX32, array[i + j]);
      }
      my_printf("\r\n");
   }
}

static void dump_gicd(void)
{
   const GICDistributor_Type *gicd = GICDistributor;

   uint32_t typer    = gicd->TYPER;
   uint32_t itlines  = typer & 0x1FU;
   uint32_t num_irqs = 32 * (itlines + 1);

   uint32_t n32 = num_irqs / 32;
   uint32_t n16 = num_irqs / 16;
   uint32_t n4  = num_irqs / 4;

   my_printf("[GICD dump] begin\r\n");
   my_printf("  CTLR             = 0x%08" PRIX32 "\r\n", gicd->CTLR);
   my_printf("  TYPER            = 0x%08" PRIX32 " (%" PRIu32 " IRQs)\r\n",
             typer, num_irqs);
   my_printf("  IIDR             = 0x%08" PRIX32 "\r\n", gicd->IIDR);
   my_printf("  STATUSR          = 0x%08" PRIX32 "\r\n", gicd->STATUSR);

   dump_reg6("IGROUPR    ", gicd->IGROUPR, (unsigned int)n32);
   dump_reg6("ISENABLER  ", gicd->ISENABLER, (unsigned int)n32);
   dump_reg6("ICENABLER  ", gicd->ICENABLER, (unsigned int)n32);
   dump_reg6("ISPENDR    ", gicd->ISPENDR, (unsigned int)n32);
   dump_reg6("ICPENDR    ", gicd->ICPENDR, (unsigned int)n32);
   dump_reg6("ISACTIVER  ", gicd->ISACTIVER, (unsigned int)n32);
   dump_reg6("ICACTIVER  ", gicd->ICACTIVER, (unsigned int)n32);
   dump_reg6("IGRPMODR   ", gicd->IGRPMODR, (unsigned int)n32);
   dump_reg6("ICFGR      ", gicd->ICFGR, (unsigned int)n16);
   dump_reg6("NSACR      ", gicd->NSACR, (unsigned int)n16);
   dump_reg6("IPRIORITYR ", gicd->IPRIORITYR, (unsigned int)n4);
   dump_reg6("ITARGETSR  ", gicd->ITARGETSR, (unsigned int)n4);

   // SGI registers (4 entries)
   my_printf("  CPENDSGIR        =");
   for (unsigned int i = 0; i < 4; i++) {
      my_printf(" 0x%08" PRIX32, gicd->CPENDSGIR[i]);
   }
   my_printf("\r\n");

   my_printf("  SPENDSGIR        =");
   for (unsigned int i = 0; i < 4; i++) {
      my_printf(" 0x%08" PRIX32, gicd->SPENDSGIR[i]);
   }
   my_printf("\r\n");

   my_printf("[GICD dump] end\r\n\n");
}

static void dump_gicc(void)
{
   my_printf("[GICC dump] begin\r\n");
   const GICInterface_Type *gicc = GICInterface;

   // --- GICC_CTLR ---
   uint32_t ctlr = gicc->CTLR;
   my_printf(INFO "CTLR     = 0x%08" PRIX32 " (GICC_CTLR)\r\n", ctlr);
   my_printf(INFO "  Bit 10 EOIMODENS: %" PRIu32
                  " (Alias of Non-secure EOIMODENS)\r\n",
             (ctlr >> 10U) & 1U);
   my_printf(INFO "  Bit 9  EOIMODES:  %" PRIu32 " (%s)\r\n", (ctlr >> 9U) & 1U,
             ((ctlr >> 9U) & 1U) ? "Secure EOI/DIR separate"
                                 : "Secure EOI/DIR combined");
   my_printf(INFO "  Bit 8  IRQBYPDISGRP1: %" PRIu32 "\r\n", (ctlr >> 8U) & 1U);
   my_printf(INFO "  Bit 7  FIQBYPDISGRP1: %" PRIu32 "\r\n", (ctlr >> 7U) & 1U);
   my_printf(INFO "  Bit 6  IRQBYPDISGRP0: %" PRIu32 "\r\n", (ctlr >> 6U) & 1U);
   my_printf(INFO "  Bit 5  FIQBYPDISGRP0: %" PRIu32 "\r\n", (ctlr >> 5U) & 1U);
   my_printf(INFO "  Bit 4  CBPR:      %" PRIu32
                  " (BPR controls both Grp0 and Grp1)\r\n",
             (ctlr >> 4U) & 1U);
   my_printf(INFO "  Bit 3  FIQEN:     %" PRIu32 " (Grp0 signals via %s)\r\n",
             (ctlr >> 3U) & 1U, ((ctlr >> 3U) & 1U) ? "FIQ" : "IRQ");

   if (ctlr & (1U << 2U))
      my_printf(
          WARN
          "  Bit 2  ACKCTL:    1 (DEPRECATED - Should be 0 for Linux)\r\n");
   else
      my_printf(OK "  Bit 2  ACKCTL:    0 (Recommended)\r\n");

   if (ctlr & 0x2U)
      my_printf(OK "  Bit 1  GRP1 EN:   1 (Enabled for Linux handoff)\r\n");
   else
      my_printf(WARN "  Bit 1  GRP1 EN:   0 (Linux IRQs are DISABLED)\r\n");

   if (ctlr & 0x1U)
      my_printf(OK "  Bit 0  GRP0 EN:   1 (Secure interrupts enabled)\r\n");
   else
      my_printf(INFO "  Bit 0  GRP0 EN:   0 (Secure interrupts disabled)\r\n");

   // --- GICC_PMR ---
   uint32_t pmr = gicc->PMR;
   my_printf(INFO "PMR      = 0x%08" PRIX32 " (Priority Mask)\r\n", pmr);
   my_printf(INFO "  Bits 7:3 PRIORITY: 0x%02" PRIX32 "\r\n",
             (pmr >> 3U) & 0x1FU);
   if (pmr >= 0xF0)
      my_printf(OK "  Status: PMR is open (0x%02" PRIX32
                   "). Interrupts can reach CPU.\r\n",
                pmr);
   else
      my_printf(WARN "  Status: PMR is restrictive (0x%02" PRIX32
                     "). IRQs may be blocked!\r\n",
                pmr);

   // --- GICC_BPR / ABPR ---
   uint32_t bpr = gicc->BPR;
   my_printf(INFO "BPR      = 0x%08" PRIX32 " (Binary Point Register)\r\n",
             bpr);
   if ((bpr & 0x7U) < 2)
      my_printf(WARN "  Value %" PRIu32 " is below functional minimum (2)\r\n",
                bpr & 0x7U);
   else
      my_printf(INFO "  Value %" PRIu32 " (Split at bit %" PRIu32 ")\r\n",
                bpr & 0x7U, (bpr & 0x7U) + 1);

   my_printf(INFO "ABPR     = 0x%08" PRIX32 " (Aliased Binary Point)\r\n",
             gicc->ABPR);

   // --- GICC_RPR ---
   uint32_t rpr = gicc->RPR;
   my_printf(INFO "RPR      = 0x%08" PRIX32 " (Running Priority)\r\n", rpr);
   if (rpr == 0xFF)
      my_printf(OK "  Status: Idle (No interrupts active)\r\n");
   else
      my_printf(WARN
                "  Status: Active interrupt running at priority 0x%02" PRIX32
                "\r\n",
                rpr);

   // --- GICC_HPPIR ---
   uint32_t hppir = gicc->HPPIR;
   my_printf(INFO "HPPIR    = 0x%08" PRIX32 " (Highest Priority Pending)\r\n",
             hppir);
   if ((hppir & 0x3FFU) == 1023)
      my_printf(OK "  Status: No pending interrupts\r\n");
   else
      my_printf(INFO "  Pending: ID %" PRIu32 " (CPUID: %" PRIu32 ")\r\n",
                hppir & 0x3FFU, (hppir >> 10U) & 0x7U);

   // --- Active Priority Registers (APR) ---
   for (int i = 0; i < 4; i++) {
      uint32_t apr = gicc->APR[i];
      my_printf(INFO "APR[%d]   = 0x%08" PRIX32 "\r\n", i, apr);
      if (apr != 0)
         my_printf(
             WARN
             "  WARN: Active bit set in APR[%d]! Linux may hang on IRQ.\r\n",
             i);
   }

   for (int i = 0; i < 4; i++) {
      my_printf(INFO "NSAPR[%d] = 0x%08" PRIX32 "\r\n", i, gicc->NSAPR[i]);
   }

   // --- GICC_IIDR ---
   uint32_t iidr = gicc->IIDR;
   my_printf(INFO "IIDR     = 0x%08" PRIX32 " (Interface ID)\r\n", iidr);
   my_printf(INFO "  Implementer: 0x%03" PRIX32 " (%s)\r\n", iidr & 0xFFFU,
             (iidr & 0xFFFU) == 0x43B ? "Arm" : "Unknown");
   my_printf(INFO "  Arch Version: GICv%" PRIu32 "\r\n", (iidr >> 16U) & 0xFU);
   my_printf(INFO "  ProductID: 0x%03" PRIX32 ", Revision: %" PRIu32 "\r\n",
             (iidr >> 20U) & 0xFFFU, (iidr >> 12U) & 0xFU);

   // --- Transient Registers ---
   my_printf(INFO "IAR      = 0x%08" PRIX32 " (Interrupt Acknowledge)\r\n",
             gicc->IAR);
   my_printf(INFO "AIAR     = 0x%08" PRIX32 " (Aliased Acknowledge)\r\n",
             gicc->AIAR);
   my_printf(INFO "DIR      = 0x%08" PRIX32 " (Deactivate Interrupt)\r\n",
             gicc->DIR);

   my_printf("[GICC dump] end\r\n\n");
}

static void check_system(void)
{
   my_printf("[check_system] begin\r\n");
   midr_dump();
   cpsr_dump();
   sctlr_dump();
   vbar_dump();
   mpidr_dump();
   actlr_dump();
   check_ddr_rw();
   check_sp_alignment();
   check_gic_dist();
   check_gic_cpuif();
   check_cntvct();
   check_cntfrq();
   check_rcc_ns();
   check_rtc_ns();
   check_gpio_ns();
   check_secure_world();
   my_printf("[check_system] done\r\n\n");
}

void diag_all(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;

   dump_tzc();
   dump_etzpc();
   dump_gicd();
   dump_gicc();
   check_system();
}
