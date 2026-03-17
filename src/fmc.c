// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file fmc.c
 * @brief FMC NAND management
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "fmc.h"
#include "board.h"
#include <stdint.h>

#ifdef NAND_FLASH

#include "console.h"
#include "defaults.h"
#include "irq_ctrl.h"
#include "nand_pt.h"
#include "printf.h"
#include "prng.h"
#include "stm32mp135fxx_ca7.h"
#include "stm32mp13xx_hal.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_gpio.h"
#include "stm32mp13xx_hal_gpio_ex.h"
#include "stm32mp13xx_hal_mdma.h"
#include "stm32mp13xx_hal_nand.h"
#include "stm32mp13xx_hal_rcc.h"
#include "stm32mp13xx_ll_fmc.h"
#include <stddef.h>
#include <string.h>

#define BLOCK_BYTES (FMC_BLOCK_SIZE_PAGES * FMC_PAGE_SIZE_BYTES)
#define BUF_A_ADDR  (FMC_SCRATCH_ADDR)
#define BUF_B_ADDR  (FMC_SCRATCH_ADDR + BLOCK_BYTES)
#define TEST_SEED   UINT64_C(0xCAFEBABEDEADBEEF)

static NAND_HandleTypeDef hnand;
static int nand_ready = 0;

/* MDMA channels and ECC buffer for the FMC sequencer.
 * Sectors per page = PAGE/SECTOR = 4096/512 = 8; BCH-8 needs 5 words per
 * sector. */
#define ECC_BUF_WORDS ((FMC_PAGE_SIZE_BYTES / FMC_SECTOR_SIZE) * 5U)
static MDMA_HandleTypeDef hmdma_rd;  /* NAND → DDR  (data)   */
static MDMA_HandleTypeDef hmdma_wr;  /* DDR  → NAND (data)   */
static MDMA_HandleTypeDef hmdma_ecc; /* BCH DSR registers → DDR (ECC read) */
static uint32_t ecc_buf[ECC_BUF_WORDS];

/* Two DDR scratch buffers, each one full NAND block (256 KB). */
static uint8_t *const buf_a = (uint8_t *)BUF_A_ADDR;
static uint8_t *const buf_b = (uint8_t *)BUF_B_ADDR;

/* Bad block table: 1 = bad, 0 = good.  Populated by fmc_init OOB scan. */
static uint8_t bad[FMC_PLANE_NBR * FMC_PLANE_SIZE_BLOCKS];

volatile int fmc_flush_active = 0;

/* Override the weak MspInit to configure the three MDMA channels needed by
 * the FMC NAND sequencer.
 *
 * HdmaRead  (MDMA_Channel0): FMC NAND data FIFO → DDR, one sector per trigger.
 * HdmaWrite (MDMA_Channel1): DDR → FMC NAND data FIFO, one sector per trigger.
 * HdmaEcc   (MDMA_Channel2): FMC BCH DSR registers → DDR ECC buffer.
 *   Each FMC_ERROR trigger fires when one sector's BCH ECC is ready.
 *   We read 5 words (BCHDSR0..4), then SourceBlockAddressOffset resets the
 *   source pointer back to BCHDSR0 for the next sector. */
// cppcheck-suppress unusedFunction  /* HAL callback, called internally by HAL
// */
HAL_StatusTypeDef HAL_NAND_Sequencer_MspInit(NAND_HandleTypeDef *hnand_arg)
{
   (void)hnand_arg;

   /* Common read/write data channel config; only Inc direction differs. */
   MDMA_InitTypeDef d = {
       .Request                  = MDMA_REQUEST_FMC_DATA,
       .TransferTriggerMode      = MDMA_BLOCK_TRANSFER,
       .Priority                 = MDMA_PRIORITY_HIGH,
       .SecureMode               = MDMA_SECURE_MODE_DISABLE,
       .Endianness               = MDMA_LITTLE_ENDIANNESS_PRESERVE,
       .SourceDataSize           = MDMA_SRC_DATASIZE_WORD,
       .DestDataSize             = MDMA_DEST_DATASIZE_WORD,
       .DataAlignment            = MDMA_DATAALIGN_PACKENABLE,
       .BufferTransferLength     = 128,
       .SourceBurst              = MDMA_SOURCE_BURST_SINGLE,
       .DestBurst                = MDMA_DEST_BURST_SINGLE,
       .SourceBlockAddressOffset = 0,
       .DestBlockAddressOffset   = 0,
   };

   /* Read: FMC FIFO (fixed) → DDR (incrementing) */
   d.SourceInc       = MDMA_SRC_INC_DISABLE;
   d.DestinationInc  = MDMA_DEST_INC_WORD;
   hmdma_rd.Instance = MDMA_Channel0;
   hmdma_rd.Init     = d;
   if (HAL_MDMA_Init(&hmdma_rd) != HAL_OK)
      return HAL_ERROR;

   /* Write: DDR (incrementing) → FMC FIFO (fixed) */
   d.SourceInc       = MDMA_SRC_INC_WORD;
   d.DestinationInc  = MDMA_DEST_INC_DISABLE;
   hmdma_wr.Instance = MDMA_Channel1;
   hmdma_wr.Init     = d;
   if (HAL_MDMA_Init(&hmdma_wr) != HAL_OK)
      return HAL_ERROR;

   /* ECC: BCH DSRs (BCHDSR0..4, incrementing within sector, reset between) */
   hmdma_ecc.Instance                      = MDMA_Channel2;
   hmdma_ecc.Init.Request                  = MDMA_REQUEST_FMC_ERROR;
   hmdma_ecc.Init.TransferTriggerMode      = MDMA_BLOCK_TRANSFER;
   hmdma_ecc.Init.Priority                 = MDMA_PRIORITY_HIGH;
   hmdma_ecc.Init.SecureMode               = MDMA_SECURE_MODE_DISABLE;
   hmdma_ecc.Init.Endianness               = MDMA_LITTLE_ENDIANNESS_PRESERVE;
   hmdma_ecc.Init.SourceInc                = MDMA_SRC_INC_WORD;
   hmdma_ecc.Init.DestinationInc           = MDMA_DEST_INC_WORD;
   hmdma_ecc.Init.SourceDataSize           = MDMA_SRC_DATASIZE_WORD;
   hmdma_ecc.Init.DestDataSize             = MDMA_DEST_DATASIZE_WORD;
   hmdma_ecc.Init.DataAlignment            = MDMA_DATAALIGN_RIGHT;
   hmdma_ecc.Init.BufferTransferLength     = 4;
   hmdma_ecc.Init.SourceBurst              = MDMA_SOURCE_BURST_SINGLE;
   hmdma_ecc.Init.DestBurst                = MDMA_DEST_BURST_SINGLE;
   hmdma_ecc.Init.SourceBlockAddressOffset = -(int32_t)(5U * sizeof(uint32_t));
   hmdma_ecc.Init.DestBlockAddressOffset   = 0;
   return HAL_MDMA_Init(&hmdma_ecc);
}

static inline void setupgpio(GPIO_TypeDef *gpio, GPIO_InitTypeDef *init,
                             uint32_t pin)
{
   init->Pin = pin;
   HAL_GPIO_Init(gpio, init);
}

static inline NAND_AddressTypeDef page_addr(uint32_t blk, uint32_t pg)
{
   NAND_AddressTypeDef a;
   a.Page  = (uint16_t)pg;
   a.Block = (uint16_t)(blk % hnand.Config.PlaneSize);
   a.Plane = (uint16_t)(blk / hnand.Config.PlaneSize);
   return a;
}

static HAL_StatusTypeDef read_page(uint32_t blk, uint32_t pg, uint8_t *buf)
{
   NAND_AddressTypeDef a = page_addr(blk, pg);
   /* Clean + invalidate before DMA so no dirty lines can be written back over
    * the incoming data, and the CPU sees fresh DDR after the transfer. */
   L1C_CleanInvalidateDCacheAll();
   if (HAL_NAND_Sequencer_ECC_Read_Page_8b(&hnand, &a, buf) != HAL_OK)
      return HAL_ERROR;
   HAL_StatusTypeDef r = HAL_NAND_Sequencer_WaitCompletion(
       &hnand, HAL_NAND_DEFAULT_SEQUENCER_TIMEOUT);
   /* Invalidate again: MDMA has written buf and ecc_buf to DDR; discard any
    * stale cache lines so the CPU reads the DMA-written data. */
   L1C_CleanInvalidateDCacheAll();
   return r;
}

static HAL_StatusTypeDef write_page(uint32_t blk, uint32_t pg,
                                    const uint8_t *buf)
{
   /* Flush buf to DDR before MDMA reads it — avoids writing stale cache
    * contents (i.e. whatever was in DDR before the CPU filled the buffer). */
   L1C_CleanDCacheAll();
   NAND_AddressTypeDef a = page_addr(blk, pg);
   /* Cast: sequencer takes void*; write path does not modify the buffer. */
   if (HAL_NAND_Sequencer_ECC_Write_Page_8b(&hnand, &a, (uint8_t *)buf) !=
       HAL_OK)
      return HAL_ERROR;
   return HAL_NAND_Sequencer_WaitCompletion(&hnand,
                                            HAL_NAND_DEFAULT_SEQUENCER_TIMEOUT);
}

static HAL_StatusTypeDef erase_block(uint32_t blk)
{
   NAND_AddressTypeDef a = page_addr(blk, 0);
   return HAL_NAND_Erase_Block(&hnand, &a);
}

static int is_bad_oob(uint32_t blk)
{
   uint8_t oob[FMC_OOB_SIZE_BYTES];
   NAND_AddressTypeDef a = page_addr(blk, 0);
   if (HAL_NAND_Read_SpareArea_8b(&hnand, &a, oob, 1) != HAL_OK)
      return 1;
   if (oob[0] != 0xFFU)
      return 1;
   a = page_addr(blk, 1);
   if (HAL_NAND_Read_SpareArea_8b(&hnand, &a, oob, 1) != HAL_OK)
      return 1;
   return oob[0] != 0xFFU;
}

static void mark_bad_oob(uint32_t blk)
{
   uint8_t oob[FMC_OOB_SIZE_BYTES];
   memset(oob, 0xFFU, sizeof(oob));
   oob[0]                = 0x00U;
   NAND_AddressTypeDef a = page_addr(blk, 0);
   HAL_NAND_Write_SpareArea_8b(&hnand, &a, oob, 1);
   a = page_addr(blk, 1);
   HAL_NAND_Write_SpareArea_8b(&hnand, &a, oob, 1);
}

static uint32_t lba_to_phys_block(uint32_t good_idx)
{
   const uint32_t total = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   uint32_t good        = 0;
   for (uint32_t b = 0; b < total; b++) {
      if (!bad[b]) {
         if (good == good_idx)
            return b;
         good++;
      }
   }
   return UINT32_MAX;
}

static HAL_StatusTypeDef read_block(uint32_t blk, uint8_t *buf)
{
   for (uint32_t pg = 0; pg < hnand.Config.BlockSize; pg++)
      if (read_page(blk, pg, buf + (pg * hnand.Config.PageSize)) != HAL_OK)
         return HAL_ERROR;
   return HAL_OK;
}

static HAL_StatusTypeDef write_block(uint32_t blk, const uint8_t *buf)
{
   for (uint32_t pg = 0; pg < hnand.Config.BlockSize; pg++)
      if (write_page(blk, pg, buf + (pg * hnand.Config.PageSize)) != HAL_OK)
         return HAL_ERROR;
   return HAL_OK;
}

static inline uint32_t le32(const uint8_t *p, uint32_t o)
{
   return (uint32_t)p[o] | ((uint32_t)p[o + 1] << 8U) |
          ((uint32_t)p[o + 2] << 16U) | ((uint32_t)p[o + 3] << 24U);
}

static inline uint32_t popcount8(uint8_t x)
{
   uint32_t v = (uint32_t)x;
   v          = v - ((v >> 1U) & 0x55U);
   v          = (v & 0x33U) + ((v >> 2U) & 0x33U);
   return (v + (v >> 4U)) & 0x0FU;
}

static void print_mbs(uint32_t bytes, uint32_t elapsed_ms)
{
   if (elapsed_ms == 0U)
      return;
   const uint32_t x10 = (uint32_t)(((uint64_t)bytes * 10000ULL) /
                                   ((uint64_t)elapsed_ms * 1048576ULL));
   my_printf("%lu.%lu MB/s", (unsigned long)(x10 / 10U),
             (unsigned long)(x10 % 10U));
}

void fmc_init(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;

   __HAL_RCC_FMC_CLK_ENABLE();
   __HAL_RCC_FMC_FORCE_RESET();
   __HAL_RCC_FMC_RELEASE_RESET();
   __HAL_RCC_MDMA_CLK_ENABLE();

   __HAL_RCC_GPIOA_CLK_ENABLE();
   __HAL_RCC_GPIOB_CLK_ENABLE();
   __HAL_RCC_GPIOD_CLK_ENABLE();
   __HAL_RCC_GPIOE_CLK_ENABLE();
   __HAL_RCC_GPIOG_CLK_ENABLE();

   GPIO_InitTypeDef g = {
       .Mode      = GPIO_MODE_AF_PP,
       .Pull      = GPIO_PULLUP,
       .Speed     = GPIO_SPEED_FREQ_VERY_HIGH,
       .Alternate = GPIO_AF10_FMC,
   };
   setupgpio(GPIOA, &g, GPIO_PIN_9); /* FMC_NWAIT */
   g.Alternate = GPIO_AF12_FMC;
   setupgpio(GPIOG, &g, GPIO_PIN_9);  /* FMC_NCE  */
   setupgpio(GPIOD, &g, GPIO_PIN_4);  /* FMC_NOE  */
   setupgpio(GPIOD, &g, GPIO_PIN_5);  /* FMC_NWE  */
   setupgpio(GPIOD, &g, GPIO_PIN_12); /* FMC_ALE  */
   setupgpio(GPIOD, &g, GPIO_PIN_11); /* FMC_CLE  */
   setupgpio(GPIOD, &g, GPIO_PIN_14); /* FMC_D0   */
   setupgpio(GPIOD, &g, GPIO_PIN_15); /* FMC_D1   */
   setupgpio(GPIOD, &g, GPIO_PIN_0);  /* FMC_D2   */
   setupgpio(GPIOD, &g, GPIO_PIN_1);  /* FMC_D3   */
   setupgpio(GPIOE, &g, GPIO_PIN_7);  /* FMC_D4   */
   setupgpio(GPIOE, &g, GPIO_PIN_8);  /* FMC_D5   */
   setupgpio(GPIOE, &g, GPIO_PIN_9);  /* FMC_D6   */
   setupgpio(GPIOE, &g, GPIO_PIN_10); /* FMC_D7   */

   IRQ_SetPriority(FMC_IRQn, 0x0F);
   IRQ_Enable(FMC_IRQn);

   hnand.Instance                  = FMC_NAND_DEVICE;
   hnand.Init.NandBank             = FMC_NAND_BANK3;
   hnand.Init.Waitfeature          = FMC_NAND_WAIT_FEATURE_ENABLE;
   hnand.Init.MemoryDataWidth      = FMC_NAND_MEM_BUS_WIDTH_8;
   hnand.Init.EccComputation       = FMC_NAND_ECC_DISABLE;
   hnand.Init.EccAlgorithm         = FMC_NAND_ECC_ALGO_BCH;
   hnand.Init.BCHMode              = FMC_NAND_BCH_8BIT;
   hnand.Init.EccSectorSize        = FMC_NAND_ECC_SECTOR_SIZE_512BYTE;
   hnand.Init.TCLRSetupTime        = 2;
   hnand.Init.TARSetupTime         = 2;
   hnand.Config.PageSize           = FMC_PAGE_SIZE_BYTES;
   hnand.Config.SpareAreaSize      = FMC_OOB_SIZE_BYTES;
   hnand.Config.BlockSize          = FMC_BLOCK_SIZE_PAGES;
   hnand.Config.BlockNbr           = FMC_PLANE_NBR * FMC_PLANE_SIZE_BLOCKS;
   hnand.Config.PlaneSize          = FMC_PLANE_SIZE_BLOCKS;
   hnand.Config.PlaneNbr           = FMC_PLANE_NBR;
   hnand.Config.ExtraCommandEnable = 1;

   FMC_NAND_PCC_TimingTypeDef com = {
       .SetupTime     = 0x01,
       .WaitSetupTime = 0x07,
       .HoldSetupTime = 0x02,
       .HiZSetupTime  = 0x01,
   };
   FMC_NAND_PCC_TimingTypeDef att = {
       .SetupTime     = 0x1A,
       .WaitSetupTime = 0x07,
       .HoldSetupTime = 0x6A,
       .HiZSetupTime  = 0x01,
   };

   if (HAL_NAND_Init(&hnand, &com, &att) != HAL_OK) {
      my_printf("HAL_NAND_Init failed\r\n");
      return;
   }

   NAND_EccConfigTypeDef ecc = {.Offset = 2};
   if (HAL_NAND_ECC_Init(&hnand, &ecc) != HAL_OK) {
      my_printf("HAL_NAND_ECC_Init failed\r\n");
      return;
   }

   NAND_SequencerConfigTypeDef seq = {
       .HdmaRead    = &hmdma_rd,
       .HdmaWrite   = &hmdma_wr,
       .HdmaReadEcc = &hmdma_ecc,
       .EccBuffer   = ecc_buf,
   };
   if (HAL_NAND_Sequencer_Init(&hnand, &seq) != HAL_OK) {
      my_printf("HAL_NAND_Sequencer_Init failed\r\n");
      return;
   }

   if (HAL_NAND_Reset(&hnand) != HAL_OK) {
      my_printf("HAL_NAND_Reset failed\r\n");
      return;
   }

   NAND_IDTypeDef id = {0};
   if (HAL_NAND_Read_ID(&hnand, &id) != HAL_OK) {
      my_printf("HAL_NAND_Read_ID failed\r\n");
      return;
   }

   if (id.Maker_Id != FMC_MAKER || id.Device_Id != FMC_DEV ||
       id.Third_Id != FMC_3RD || id.Fourth_Id != FMC_4TH) {
      my_printf("unexpected NAND ID: %02x %02x %02x %02x\r\n", id.Maker_Id,
                id.Device_Id, id.Third_Id, id.Fourth_Id);
      return;
   }

   nand_ready = 1;
   fmc_scan(0, 0, 0, 0);
}

void fmc_erase_all(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;
   if (!nand_ready) {
      my_printf("FMC: not initialised\r\n");
      return;
   }

   const uint32_t total = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   const uint32_t n  = (argc >= 1 && arg1 > 0 && arg1 <= total) ? arg1 : total;
   uint32_t pre      = 0;
   uint32_t new_bad  = 0;
   const uint32_t t0 = HAL_GetTick();
   uint32_t t_print  = t0;

   my_printf("FMC: erasing %lu blocks\r\n", (unsigned long)n);
   for (uint32_t blk = 0; blk < n; blk++) {
      if (bad[blk]) {
         my_printf("\rskip %lu (pre-marked bad)\r\n", (unsigned long)blk);
         pre++;
         continue;
      }
      if (erase_block(blk) != HAL_OK) {
         my_printf("\rnewly bad %lu (erase fail)\r\n", (unsigned long)blk);
         mark_bad_oob(blk);
         bad[blk] = 1;
         new_bad++;
      }
      const uint32_t now = HAL_GetTick();
      if ((now - t_print) >= 2000U) {
         my_printf("\rblk %lu/%lu  (%lu pre-bad, %lu new-bad)  ",
                   (unsigned long)blk + 1UL, (unsigned long)n,
                   (unsigned long)pre, (unsigned long)new_bad);
         print_mbs((blk + 1U) * BLOCK_BYTES, now - t0);
         t_print = now;
         if (console_interrupted()) {
            my_printf("\r\ninterrupted\r\n");
            return;
         }
      }
   }
   const uint32_t elapsed = HAL_GetTick() - t0;
   my_printf("\r\ndone: %lu pre-marked bad, %lu newly bad, %lu s, avg ",
             (unsigned long)pre, (unsigned long)new_bad,
             (unsigned long)(elapsed / 1000U));
   print_mbs(n * BLOCK_BYTES, elapsed);
   my_printf("\r\n");
}

static void check_boot_block(uint32_t blk)
{
   my_printf("boot check: block %lu\r\n", (unsigned long)blk);
   if (read_block(blk, buf_a) != HAL_OK) {
      my_printf("  read error\r\n");
      return;
   }

   const uint8_t *h = buf_a;
   if (h[0] != 'S' || h[1] != 'T' || h[2] != 'M' || h[3] != 0x32) {
      my_printf("  bad magic: %02x %02x %02x %02x\r\n", h[0], h[1], h[2], h[3]);
      return;
   }

   const uint32_t checksum  = le32(h, 0x44);
   const uint32_t ver_word  = le32(h, 0x48);
   const uint32_t image_len = le32(h, 0x4C);
   const uint32_t entry_pt  = le32(h, 0x50);
   const uint32_t load_addr = le32(h, 0x58);
   const uint8_t major      = (uint8_t)(ver_word >> 16U);
   const uint8_t minor      = (uint8_t)(ver_word >> 8U);

   my_printf("  version %u.%u  image_len %lu  entry 0x%08lx  load 0x%08lx\r\n",
             major, minor, (unsigned long)image_len, (unsigned long)entry_pt,
             (unsigned long)load_addr);

   if (image_len == 0) {
      my_printf("  zero image_len\r\n");
      return;
   }

   if (major == 2) {
      const uint8_t *e = h + 0x80;
      if (e[0] == 'S' && e[1] == 'T' && e[2] == 0xFF && e[3] == 0xFF)
         my_printf("  ext header: OK\r\n");
      else
         my_printf("  ext header: bad %02x %02x %02x %02x\r\n", e[0], e[1],
                   e[2], e[3]);
   }

   /* Checksum over image_len bytes starting at byte 512 (after header).
    * Reads block-by-block into buf_a; handles images spanning multiple blocks.
    */
   uint32_t computed  = 0;
   uint32_t remaining = image_len;
   uint32_t cur_blk   = blk;
   uint32_t byte_off  = 512U;

   while (remaining > 0) {
      if (cur_blk != blk && read_block(cur_blk, buf_a) != HAL_OK) {
         my_printf("  read error at block %lu\r\n", (unsigned long)cur_blk);
         return;
      }
      const uint32_t avail = BLOCK_BYTES - byte_off;
      const uint32_t use   = remaining < avail ? remaining : avail;
      for (uint32_t i = byte_off; i < byte_off + use; i++)
         computed += buf_a[i];
      remaining -= use;
      byte_off = 0;
      cur_blk++;
   }

   if (computed == checksum)
      my_printf("  checksum OK (0x%08lx)\r\n", (unsigned long)computed);
   else
      my_printf("  checksum MISMATCH computed 0x%08lx stored 0x%08lx\r\n",
                (unsigned long)computed, (unsigned long)checksum);
}

static void check_partition_table(void)
{
   my_printf("partition table: block %u\r\n", NAND_BLOCK_PT);
   if (read_block(NAND_BLOCK_PT, buf_a) != HAL_OK) {
      my_printf("  read error\r\n");
      return;
   }
   const nand_pt_t *pt = (const nand_pt_t *)buf_a;
   if (pt->magic != NAND_PT_MAGIC) {
      my_printf("  bad magic: 0x%08lx\r\n", (unsigned long)pt->magic);
      return;
   }
   if (pt->version != NAND_PT_VERSION) {
      my_printf("  unknown version: %lu\r\n", (unsigned long)pt->version);
      return;
   }
   uint32_t sum     = 0;
   const uint8_t *b = (const uint8_t *)pt;
   for (uint32_t i = 0; i < (uint32_t)offsetof(nand_pt_t, checksum); i++)
      sum += b[i];
   if (sum != pt->checksum) {
      my_printf("  checksum MISMATCH computed 0x%08lx stored 0x%08lx\r\n",
                (unsigned long)sum, (unsigned long)pt->checksum);
      return;
   }
   my_printf("  checksum OK  total_blocks %lu  %lu partition(s)\r\n",
             (unsigned long)pt->total_blocks, (unsigned long)pt->num_parts);
   for (uint32_t i = 0; i < pt->num_parts && i < NAND_PT_MAX_PARTS; i++) {
      const nand_part_t *p = &pt->parts[i];
      my_printf("  [%lu] %-16s  block %lu  len %lu\r\n", (unsigned long)i,
                p->name, (unsigned long)p->start_block,
                (unsigned long)p->num_blocks);
   }
}

static void check_dtb(void)
{
   my_printf("DTB: block %u\r\n", NAND_BLOCK_DTB);
   if (read_block(NAND_BLOCK_DTB, buf_a) != HAL_OK) {
      my_printf("  read error\r\n");
      return;
   }
   /* FDT magic: 0xD00DFEED big-endian */
   const uint8_t *h     = buf_a;
   const uint32_t magic = ((uint32_t)h[0] << 24U) | ((uint32_t)h[1] << 16U) |
                          ((uint32_t)h[2] << 8U) | (uint32_t)h[3];
   if (magic != 0xD00DFEEDU) {
      my_printf("  bad FDT magic: 0x%08lx\r\n", (unsigned long)magic);
      return;
   }
   const uint32_t totalsize = ((uint32_t)h[4] << 24U) |
                              ((uint32_t)h[5] << 16U) | ((uint32_t)h[6] << 8U) |
                              (uint32_t)h[7];
   my_printf("  FDT magic OK  totalsize %lu bytes\r\n",
             (unsigned long)totalsize);
}

void fmc_test_boot(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;
   if (!nand_ready) {
      my_printf("FMC: not initialised\r\n");
      return;
   }
   check_boot_block(0);
   check_boot_block(1);
   check_partition_table();
   check_dtb();
}

void fmc_test_write(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;
   if (!nand_ready) {
      my_printf("FMC: not initialised\r\n");
      return;
   }

   const uint32_t n  = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   uint64_t prng     = TEST_SEED;
   uint32_t errors   = 0;
   const uint32_t t0 = HAL_GetTick();
   uint32_t t_print  = t0;

   my_printf("FMC write: %lu blocks\r\n", (unsigned long)n);
   for (uint32_t blk = 0; blk < n; blk++) {
      prng_fill(buf_a, BLOCK_BYTES, &prng);
      if (write_block(blk, buf_a) != HAL_OK)
         errors++;
      const uint32_t now = HAL_GetTick();
      if ((now - t_print) >= 2000U) {
         my_printf("\rblk %lu/%lu  ", (unsigned long)blk + 1UL,
                   (unsigned long)n);
         print_mbs((blk + 1) * BLOCK_BYTES, now - t0);
         my_printf("  (%lu errs)  ", (unsigned long)errors);
         t_print = now;
         if (console_interrupted()) {
            my_printf("\r\ninterrupted\r\n");
            return;
         }
      }
   }
   const uint32_t elapsed = HAL_GetTick() - t0;
   my_printf("\r\ndone: %lu errs, %lu s, avg ", (unsigned long)errors,
             (unsigned long)(elapsed / 1000U));
   print_mbs(n * BLOCK_BYTES, elapsed);
   my_printf("\r\n");
}

void fmc_test_read(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;
   if (!nand_ready) {
      my_printf("FMC: not initialised\r\n");
      return;
   }

   const uint32_t n  = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   uint64_t prng     = TEST_SEED;
   uint32_t bit_errs = 0;
   uint32_t rd_errs  = 0;
   const uint32_t t0 = HAL_GetTick();
   uint32_t t_print  = t0;

   my_printf("FMC read: %lu blocks\r\n", (unsigned long)n);
   for (uint32_t blk = 0; blk < n; blk++) {
      prng_fill(buf_a, BLOCK_BYTES, &prng);
      if (read_block(blk, buf_b) != HAL_OK) {
         rd_errs++;
         continue;
      }
      for (uint32_t i = 0; i < BLOCK_BYTES; i++) {
         const uint8_t diff = buf_b[i] ^ buf_a[i];
         if (diff)
            bit_errs += popcount8(diff);
      }
      const uint32_t now = HAL_GetTick();
      if ((now - t_print) >= 2000U) {
         my_printf("\rblk %lu/%lu  ", (unsigned long)blk + 1UL,
                   (unsigned long)n);
         print_mbs((blk + 1) * BLOCK_BYTES, now - t0);
         my_printf("  (%lu bit errs)  ", (unsigned long)bit_errs);
         t_print = now;
         if (console_interrupted()) {
            my_printf("\r\ninterrupted\r\n");
            return;
         }
      }
   }
   const uint32_t elapsed = HAL_GetTick() - t0;
   my_printf("\r\ndone: %lu rd errs, %lu bit errs (post-ECC), %lu s, avg ",
             (unsigned long)rd_errs, (unsigned long)bit_errs,
             (unsigned long)(elapsed / 1000U));
   print_mbs(n * BLOCK_BYTES, elapsed);
   my_printf("\r\n");
}

void fmc_scan(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;
   if (!nand_ready) {
      my_printf("FMC: not initialised\r\n");
      return;
   }

   const uint32_t total = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   uint32_t count       = 0;
   for (uint32_t blk = 0; blk < total; blk++) {
      const int b = is_bad_oob(blk);
      bad[blk]    = (uint8_t)b;
      if (b) {
         my_printf("bad: blk %lu\r\n", (unsigned long)blk);
         count++;
      }
   }
   my_printf("scan done: %lu bad / %lu total\r\n", (unsigned long)count,
             (unsigned long)total);
}

static uint32_t pt_total_blocks(void)
{
   const nand_pt_t *const pt =
       (const nand_pt_t *)(FMC_DDR_BUF_ADDR +
                           (uint32_t)(NAND_BLOCK_PT * BLOCK_BYTES));
   if (pt->magic != NAND_PT_MAGIC)
      return 0;
   uint32_t sum     = 0;
   const uint8_t *b = (const uint8_t *)pt;
   for (uint32_t i = 0; i < (uint32_t)offsetof(nand_pt_t, checksum); i++)
      sum += b[i];
   if (sum != pt->checksum)
      return 0;
   return pt->total_blocks;
}

/* Flush one good physical block from DDR src to NAND phys.
 * Returns: 1 = skipped (already matches), 0 = written, -1 = newly bad. */
static int flush_one_block(uint32_t phys, const uint8_t *src, uint32_t ppb)
{
   if (read_block(phys, buf_a) == HAL_OK &&
       memcmp(buf_a, src, BLOCK_BYTES) == 0)
      return 1;

   if (erase_block(phys) != HAL_OK) {
      my_printf("\rnewly bad %lu (flush erase)\r\n", (unsigned long)phys);
      mark_bad_oob(phys);
      bad[phys] = 1;
      return -1;
   }

   for (uint32_t pg = 0; pg < ppb; pg++) {
      if (write_page(phys, pg, src + (pg * hnand.Config.PageSize)) != HAL_OK) {
         my_printf("\rnewly bad %lu (flush write)\r\n", (unsigned long)phys);
         mark_bad_oob(phys);
         bad[phys] = 1;
         return -1;
      }
   }
   return 0;
}

void fmc_flush(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)arg2;
   (void)arg3;
   if (!nand_ready) {
      my_printf("FMC: not initialised\r\n");
      return;
   }

   const uint32_t ppb      = hnand.Config.BlockSize;
   const uint32_t total    = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   const uint32_t max_blks = FMC_DDR_BUF_SIZE / BLOCK_BYTES;
   const uint32_t pt_n     = pt_total_blocks();
   uint32_t n              = 0U;
   if (argc >= 1 && arg1 > 0 && arg1 <= max_blks)
      n = arg1;
   else if (pt_n > 0 && pt_n <= max_blks)
      n = pt_n;
   else
      n = max_blks;
   const uint8_t *const ddr = (const uint8_t *)FMC_DDR_BUF_ADDR;

   fmc_flush_active = 1;

   uint32_t written  = 0;
   uint32_t skipped  = 0;
   uint32_t bad_new  = 0;
   uint32_t good_idx = 0;
   uint32_t phys     = 0;
   const uint32_t t0 = HAL_GetTick();
   uint32_t t_print  = t0;

   my_printf("FMC flush: %lu blocks\r\n", (unsigned long)n);

   while (good_idx < n && phys < total) {
      if (!bad[phys]) {
         const uint8_t *const src = ddr + (good_idx * BLOCK_BYTES);
         const int result         = flush_one_block(phys, src, ppb);
         if (result == 1)
            skipped++;
         else if (result == 0)
            written++;
         else
            bad_new++;
         if (result != -1)
            good_idx++;
      }
      phys++;

      const uint32_t now = HAL_GetTick();
      if ((now - t_print) >= 2000U) {
         my_printf("\rblk %lu/%lu  ", (unsigned long)good_idx,
                   (unsigned long)n);
         print_mbs(good_idx * BLOCK_BYTES, now - t0);
         my_printf("  (%lu written, %lu skipped)  ", (unsigned long)written,
                   (unsigned long)skipped);
         t_print = now;
         if (console_interrupted()) {
            fmc_flush_active = 0;
            my_printf("\r\ninterrupted\r\n");
            return;
         }
      }
   }

   fmc_flush_active       = 0;
   const uint32_t elapsed = HAL_GetTick() - t0;
   my_printf("\r\ndone: %lu written, %lu skipped, %lu new-bad, %lu s, avg ",
             (unsigned long)written, (unsigned long)skipped,
             (unsigned long)bad_new, (unsigned long)(elapsed / 1000U));
   print_mbs((written + skipped) * BLOCK_BYTES, elapsed);
   my_printf("\r\n");
}

void fmc_bload(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;
   if (!nand_ready) {
      my_printf("FMC: not initialised\r\n");
      return;
   }

   /* Read partition table into buf_a. */
   const uint32_t pt_phys = lba_to_phys_block(NAND_BLOCK_PT);
   if (pt_phys == UINT32_MAX) {
      my_printf("bload: cannot find PT block\r\n");
      return;
   }
   if (read_block(pt_phys, buf_a) != HAL_OK) {
      my_printf("bload: PT read error\r\n");
      return;
   }

   const nand_pt_t *pt = (const nand_pt_t *)buf_a;
   if (pt->magic != NAND_PT_MAGIC) {
      my_printf("bload: bad PT magic 0x%08lx\r\n", (unsigned long)pt->magic);
      return;
   }
   uint32_t sum      = 0;
   const uint8_t *pb = (const uint8_t *)pt;
   for (uint32_t i = 0; i < (uint32_t)offsetof(nand_pt_t, checksum); i++)
      sum += pb[i];
   if (sum != pt->checksum) {
      my_printf("bload: PT checksum mismatch\r\n");
      return;
   }

   /* Find kernel and dtb partitions. */
   const nand_part_t *kern_p = NULL;
   const nand_part_t *dtb_p  = NULL;
   for (uint32_t i = 0; i < pt->num_parts && i < NAND_PT_MAX_PARTS; i++) {
      if (strcmp(pt->parts[i].name, "kernel") == 0)
         kern_p = &pt->parts[i];
      else if (strcmp(pt->parts[i].name, "dtb") == 0)
         dtb_p = &pt->parts[i];
   }
   if (!kern_p) {
      my_printf("bload: no kernel partition\r\n");
      return;
   }
   if (!dtb_p) {
      my_printf("bload: no dtb partition\r\n");
      return;
   }

   /* Load DTB. */
   my_printf("bload: DTB  blk %lu+%lu -> 0x%08lx\r\n",
             (unsigned long)dtb_p->start_block,
             (unsigned long)dtb_p->num_blocks, (unsigned long)DEF_DTB_ADDR);
   uint8_t *const dtb_dst = (uint8_t *)DEF_DTB_ADDR;
   for (uint32_t i = 0; i < dtb_p->num_blocks; i++) {
      const uint32_t phys = lba_to_phys_block(dtb_p->start_block + i);
      if (phys == UINT32_MAX) {
         my_printf("bload: DTB block %lu missing\r\n", (unsigned long)i);
         return;
      }
      if (read_block(phys, dtb_dst + (i * BLOCK_BYTES)) != HAL_OK) {
         my_printf("bload: DTB read error blk %lu\r\n", (unsigned long)i);
         return;
      }
   }

   /* Load kernel. */
   my_printf("bload: kernel blk %lu+%lu -> 0x%08lx\r\n",
             (unsigned long)kern_p->start_block,
             (unsigned long)kern_p->num_blocks, (unsigned long)DEF_LINUX_ADDR);
   uint8_t *const kern_dst = (uint8_t *)DEF_LINUX_ADDR;
   for (uint32_t i = 0; i < kern_p->num_blocks; i++) {
      const uint32_t phys = lba_to_phys_block(kern_p->start_block + i);
      if (phys == UINT32_MAX) {
         my_printf("bload: kernel block %lu missing\r\n", (unsigned long)i);
         return;
      }
      if (read_block(phys, kern_dst + (i * BLOCK_BYTES)) != HAL_OK) {
         my_printf("bload: kernel read error blk %lu\r\n", (unsigned long)i);
         return;
      }
   }

   my_printf("bload: done\r\n");
}

void fmc_load(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)arg2;
   (void)arg3;
   if (!nand_ready) {
      my_printf("FMC: not initialised\r\n");
      return;
   }

   const uint32_t total    = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   const uint32_t max_blks = FMC_DDR_BUF_SIZE / BLOCK_BYTES;
   const uint32_t n =
       (argc >= 1 && arg1 > 0 && arg1 <= max_blks) ? arg1 : max_blks;
   uint8_t *const ddr = (uint8_t *)FMC_DDR_BUF_ADDR;

   fmc_flush_active = 1;

   uint32_t rd_errs  = 0;
   uint32_t good_idx = 0;
   uint32_t phys     = 0;
   const uint32_t t0 = HAL_GetTick();
   uint32_t t_print  = t0;

   my_printf("FMC load: %lu blocks\r\n", (unsigned long)n);

   while (good_idx < n && phys < total) {
      if (bad[phys]) {
         phys++;
         continue;
      }

      uint8_t *const dst = ddr + (good_idx * BLOCK_BYTES);
      if (read_block(phys, dst) != HAL_OK)
         rd_errs++;

      good_idx++;
      phys++;

      const uint32_t now = HAL_GetTick();
      if ((now - t_print) >= 2000U) {
         my_printf("\rblk %lu/%lu  ", (unsigned long)good_idx,
                   (unsigned long)n);
         print_mbs(good_idx * BLOCK_BYTES, now - t0);
         my_printf("  (%lu rd errs)  ", (unsigned long)rd_errs);
         t_print = now;
         if (console_interrupted()) {
            fmc_flush_active = 0;
            my_printf("\r\ninterrupted\r\n");
            return;
         }
      }
   }

   fmc_flush_active       = 0;
   const uint32_t elapsed = HAL_GetTick() - t0;
   my_printf("\r\ndone: %lu rd errs, %lu s, avg ", (unsigned long)rd_errs,
             (unsigned long)(elapsed / 1000U));
   print_mbs(n * BLOCK_BYTES, elapsed);
   my_printf("\r\n");
}

#endif // NAND_FLASH

// end file fmc.c
