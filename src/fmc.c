// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file fmc.c
 * @brief FMC NAND management
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include <stdint.h>
#include <string.h>
#include "board.h"
#include "fmc.h"

#ifdef NAND_FLASH

#include "irq_ctrl.h"
#include "prng.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include "stm32mp13xx_hal.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_gpio.h"
#include "stm32mp13xx_hal_gpio_ex.h"
#include "stm32mp13xx_hal_nand.h"
#include "stm32mp13xx_hal_rcc.h"
#include "stm32mp13xx_ll_fmc.h"

#define BLOCK_BYTES  (FMC_BLOCK_SIZE_PAGES * FMC_PAGE_SIZE_BYTES)
#define BUF_A_ADDR   (FMC_SCRATCH_ADDR)
#define BUF_B_ADDR   (FMC_SCRATCH_ADDR + BLOCK_BYTES)
#define TEST_SEED    UINT64_C(0xCAFEBABEDEADBEEF)

static NAND_HandleTypeDef hnand;
static int                nand_ready = 0;

/* Two DDR scratch buffers, each one full NAND block (256 KB). */
static uint8_t * const buf_a = (uint8_t *)BUF_A_ADDR;
static uint8_t * const buf_b = (uint8_t *)BUF_B_ADDR;

/* Bad block table: 1 = bad, 0 = good.  Populated by fmc_init OOB scan. */
static uint8_t bad[FMC_PLANE_NBR * FMC_PLANE_SIZE_BLOCKS];

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

static int page_is_blank(const uint8_t *buf)
{
   for (uint32_t i = 0; i < hnand.Config.PageSize; i++)
      if (buf[i] != 0xFFU)
         return 0;
   return 1;
}

static HAL_StatusTypeDef read_page(uint32_t blk, uint32_t pg, uint8_t *buf)
{
   NAND_AddressTypeDef a = page_addr(blk, pg);
   uint32_t n = 0;
   if (HAL_NAND_ECC_Read_Page_8b(&hnand, &a, buf, 1, &n) != HAL_OK)
      return HAL_ERROR;
   if (!page_is_blank(buf)) {
      NAND_EccStatisticsTypeDef st;
      HAL_NAND_ECC_GetStatistics(&hnand, &st);
      if (st.CorrectibleErrorMax > 8U) {
         my_printf("ECC uncorrectable: blk %lu pg %lu\r\n", blk, pg);
         return HAL_ERROR;
      }
   }
   return HAL_OK;
}

static HAL_StatusTypeDef write_page(uint32_t blk, uint32_t pg,
                                    const uint8_t *buf)
{
   NAND_AddressTypeDef a = page_addr(blk, pg);
   uint32_t n = 0;
   return HAL_NAND_ECC_Write_Page_8b(&hnand, &a, (uint8_t *)buf, 1, &n);
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
   oob[0] = 0x00U;
   NAND_AddressTypeDef a = page_addr(blk, 0);
   HAL_NAND_Write_SpareArea_8b(&hnand, &a, oob, 1);
   a = page_addr(blk, 1);
   HAL_NAND_Write_SpareArea_8b(&hnand, &a, oob, 1);
}

static uint32_t lba_to_phys_block(uint32_t good_idx)
{
   const uint32_t total = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   uint32_t good = 0;
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
      if (read_page(blk, pg, buf + pg * hnand.Config.PageSize) != HAL_OK)
         return HAL_ERROR;
   return HAL_OK;
}

static HAL_StatusTypeDef write_block(uint32_t blk, const uint8_t *buf)
{
   for (uint32_t pg = 0; pg < hnand.Config.BlockSize; pg++)
      if (write_page(blk, pg, buf + pg * hnand.Config.PageSize) != HAL_OK)
         return HAL_ERROR;
   return HAL_OK;
}

static inline uint32_t le32(const uint8_t *p, uint32_t o)
{
   return (uint32_t)p[o]         | ((uint32_t)p[o+1] <<  8) |
          ((uint32_t)p[o+2] << 16) | ((uint32_t)p[o+3] << 24);
}

static inline uint32_t popcount8(uint8_t x)
{
   x = x - ((x >> 1U) & 0x55U);
   x = (x & 0x33U) + ((x >> 2U) & 0x33U);
   return (uint32_t)((x + (x >> 4U)) & 0x0FU);
}

static void print_mbs(uint32_t bytes, uint32_t elapsed_ms)
{
   if (elapsed_ms == 0U)
      return;
   const uint32_t x10 = (uint32_t)(((uint64_t)bytes * 10000ULL) /
                                    ((uint64_t)elapsed_ms * 1048576ULL));
   my_printf("%lu.%lu MB/s",
             (unsigned long)(x10 / 10U), (unsigned long)(x10 % 10U));
}

void fmc_init(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;

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
   setupgpio(GPIOA, &g, GPIO_PIN_9);  /* FMC_NWAIT */
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
      .SetupTime = 0x01, .WaitSetupTime = 0x07,
      .HoldSetupTime = 0x02, .HiZSetupTime = 0x01,
   };
   FMC_NAND_PCC_TimingTypeDef att = {
      .SetupTime = 0x1A, .WaitSetupTime = 0x07,
      .HoldSetupTime = 0x6A, .HiZSetupTime = 0x01,
   };

   if (HAL_NAND_Init(&hnand, &com, &att) != HAL_OK) {
      my_printf("HAL_NAND_Init failed\r\n");
      return;
   }

   NAND_EccConfigTypeDef ecc = { .Offset = 2 };
   if (HAL_NAND_ECC_Init(&hnand, &ecc) != HAL_OK) {
      my_printf("HAL_NAND_ECC_Init failed\r\n");
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
       id.Third_Id != FMC_3RD   || id.Fourth_Id != FMC_4TH) {
      my_printf("unexpected NAND ID: %02x %02x %02x %02x\r\n",
                id.Maker_Id, id.Device_Id, id.Third_Id, id.Fourth_Id);
      return;
   }

   nand_ready = 1;

   const uint32_t total = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   memset(bad, 0, total);
   uint32_t bad_count = 0;
   for (uint32_t blk = 0; blk < total; blk++) {
      if (is_bad_oob(blk)) {
         bad[blk] = 1;
         bad_count++;
      }
   }
   my_printf("FMC: %lu bad block(s) found\r\n", bad_count);
}

/* USB MSC: lba in pages; read_page/write_page used directly because
 * block-level helpers would read 256 KB per 4 KB USB request. */
HAL_StatusTypeDef fmc_read_blocks(uint8_t *buf, uint32_t lba, uint32_t n)
{
   if (!nand_ready)
      return HAL_ERROR;
   const uint32_t ppb = hnand.Config.BlockSize;
   for (uint32_t i = 0; i < n; i++) {
      const uint32_t ap   = lba + i;
      const uint32_t phys = lba_to_phys_block(ap / ppb);
      if (phys == UINT32_MAX)
         return HAL_ERROR;
      if (read_page(phys, ap % ppb, buf + i * hnand.Config.PageSize) != HAL_OK)
         return HAL_ERROR;
   }
   return HAL_OK;
}

HAL_StatusTypeDef fmc_write_blocks(const uint8_t *buf, uint32_t lba, uint32_t n)
{
   if (!nand_ready)
      return HAL_ERROR;
   const uint32_t ppb = hnand.Config.BlockSize;
   for (uint32_t i = 0; i < n; i++) {
      const uint32_t ap   = lba + i;
      const uint32_t phys = lba_to_phys_block(ap / ppb);
      if (phys == UINT32_MAX)
         return HAL_ERROR;
      if (write_page(phys, ap % ppb, buf + i * hnand.Config.PageSize) != HAL_OK)
         return HAL_ERROR;
   }
   return HAL_OK;
}

uint32_t fmc_block_count(void)
{
   if (!nand_ready)
      return 0;
   const uint32_t total = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   uint32_t good = 0;
   for (uint32_t b = 0; b < total; b++)
      if (!bad[b])
         good++;
   return good * hnand.Config.BlockSize;
}

void fmc_erase_all(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
   if (!nand_ready) { my_printf("FMC: not initialised\r\n"); return; }

   const uint32_t n       = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   uint32_t       pre     = 0;
   uint32_t       new_bad = 0;
   const uint32_t t0      = HAL_GetTick();
   uint32_t       t_print = t0;

   my_printf("FMC: erasing %lu blocks\r\n", n);
   for (uint32_t blk = 0; blk < n; blk++) {
      if (bad[blk]) {
         my_printf("\rskip %lu (pre-marked bad)\r\n", blk);
         pre++;
         continue;
      }
      if (erase_block(blk) != HAL_OK) {
         my_printf("\rnewly bad %lu (erase fail)\r\n", blk);
         mark_bad_oob(blk);
         bad[blk] = 1;
         new_bad++;
      }
      const uint32_t now = HAL_GetTick();
      if ((now - t_print) >= 2000U) {
         my_printf("\rblk %lu/%lu  (%lu pre-bad, %lu new-bad)  ",
                   blk + 1, n, pre, new_bad);
         print_mbs((blk + 1) * BLOCK_BYTES, now - t0);
         t_print = now;
      }
   }
   const uint32_t elapsed = HAL_GetTick() - t0;
   my_printf("\r\ndone: %lu pre-marked bad, %lu newly bad, %lu s, avg ",
             pre, new_bad, elapsed / 1000U);
   print_mbs(n * BLOCK_BYTES, elapsed);
   my_printf("\r\n");
}

static void check_boot_block(uint32_t blk)
{
   my_printf("boot check: block %lu\r\n", blk);
   if (read_block(blk, buf_a) != HAL_OK) {
      my_printf("  read error\r\n");
      return;
   }

   const uint8_t *h = buf_a;
   if (h[0] != 'S' || h[1] != 'T' || h[2] != 'M' || h[3] != 0x32) {
      my_printf("  bad magic: %02x %02x %02x %02x\r\n",
                h[0], h[1], h[2], h[3]);
      return;
   }

   const uint32_t checksum  = le32(h, 0x44);
   const uint32_t ver_word  = le32(h, 0x48);
   const uint32_t image_len = le32(h, 0x4C);
   const uint32_t entry_pt  = le32(h, 0x50);
   const uint32_t load_addr = le32(h, 0x58);
   const uint8_t  major     = (uint8_t)(ver_word >> 16);
   const uint8_t  minor     = (uint8_t)(ver_word >>  8);

   my_printf("  version %u.%u  image_len %lu  entry 0x%08lx  load 0x%08lx\r\n",
             major, minor, image_len, entry_pt, load_addr);

   if (image_len == 0) { my_printf("  zero image_len\r\n"); return; }

   if (major == 2) {
      const uint8_t *e = h + 0x80;
      if (e[0] == 'S' && e[1] == 'T' && e[2] == 0xFF && e[3] == 0xFF)
         my_printf("  ext header: OK\r\n");
      else
         my_printf("  ext header: bad %02x %02x %02x %02x\r\n",
                   e[0], e[1], e[2], e[3]);
   }

   /* Checksum over image_len bytes starting at byte 512 (after header).
    * Reads block-by-block into buf_a; handles images spanning multiple blocks. */
   uint32_t computed  = 0;
   uint32_t remaining = image_len;
   uint32_t cur_blk   = blk;
   uint32_t byte_off  = 512U;

   while (remaining > 0) {
      if (cur_blk != blk && read_block(cur_blk, buf_a) != HAL_OK) {
         my_printf("  read error at block %lu\r\n", cur_blk);
         return;
      }
      const uint32_t avail = BLOCK_BYTES - byte_off;
      const uint32_t use   = remaining < avail ? remaining : avail;
      for (uint32_t i = byte_off; i < byte_off + use; i++)
         computed += buf_a[i];
      remaining -= use;
      byte_off   = 0;
      cur_blk++;
   }

   if (computed == checksum)
      my_printf("  checksum OK (0x%08lx)\r\n", computed);
   else
      my_printf("  checksum MISMATCH computed 0x%08lx stored 0x%08lx\r\n",
                computed, checksum);
}

void fmc_test_boot(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
   if (!nand_ready) { my_printf("FMC: not initialised\r\n"); return; }
   check_boot_block(0);
   check_boot_block(1);
}

void fmc_test_write(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
   if (!nand_ready) { my_printf("FMC: not initialised\r\n"); return; }

   const uint32_t n  = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   uint64_t       prng     = TEST_SEED;
   uint32_t       errors   = 0;
   const uint32_t t0       = HAL_GetTick();
   uint32_t       t_print  = t0;

   my_printf("FMC write: %lu blocks\r\n", n);
   for (uint32_t blk = 0; blk < n; blk++) {
      prng_fill(buf_a, BLOCK_BYTES, &prng);
      if (write_block(blk, buf_a) != HAL_OK)
         errors++;
      const uint32_t now = HAL_GetTick();
      if ((now - t_print) >= 2000U) {
         my_printf("\rblk %lu/%lu  ", blk + 1, n);
         print_mbs((blk + 1) * BLOCK_BYTES, now - t0);
         my_printf("  (%lu errs)  ", errors);
         t_print = now;
      }
   }
   const uint32_t elapsed = HAL_GetTick() - t0;
   my_printf("\r\ndone: %lu errs, %lu s, avg ", errors, elapsed / 1000U);
   print_mbs(n * BLOCK_BYTES, elapsed);
   my_printf("\r\n");
}

void fmc_test_read(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
   if (!nand_ready) { my_printf("FMC: not initialised\r\n"); return; }

   const uint32_t n  = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   uint64_t       prng     = TEST_SEED;
   uint32_t       bit_errs = 0;
   uint32_t       rd_errs  = 0;
   const uint32_t t0       = HAL_GetTick();
   uint32_t       t_print  = t0;

   my_printf("FMC read: %lu blocks\r\n", n);
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
         my_printf("\rblk %lu/%lu  ", blk + 1, n);
         print_mbs((blk + 1) * BLOCK_BYTES, now - t0);
         my_printf("  (%lu bit errs)  ", bit_errs);
         t_print = now;
      }
   }
   const uint32_t elapsed = HAL_GetTick() - t0;
   my_printf("\r\ndone: %lu rd errs, %lu bit errs (post-ECC), %lu s, avg ",
             rd_errs, bit_errs, elapsed / 1000U);
   print_mbs(n * BLOCK_BYTES, elapsed);
   my_printf("\r\n");
}

void fmc_scan(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
   if (!nand_ready) { my_printf("FMC: not initialised\r\n"); return; }

   const uint32_t total = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   uint32_t count = 0;
   for (uint32_t blk = 0; blk < total; blk++) {
      const int b = is_bad_oob(blk);
      bad[blk] = (uint8_t)b;
      if (b) {
         my_printf("bad: blk %lu\r\n", blk);
         count++;
      }
   }
   my_printf("scan done: %lu bad / %lu total\r\n", count, total);
}

#else // NAND_FLASH

void fmc_init(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
}

HAL_StatusTypeDef fmc_read_blocks(uint8_t *buf, uint32_t lba, uint32_t n)
{
   (void)buf; (void)lba; (void)n;
   return HAL_OK;
}

HAL_StatusTypeDef fmc_write_blocks(const uint8_t *buf, uint32_t lba, uint32_t n)
{
   (void)buf; (void)lba; (void)n;
   return HAL_OK;
}

uint32_t fmc_block_count(void) { return 0; }

void fmc_erase_all(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
}

void fmc_test_boot(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
}

void fmc_test_write(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
}

void fmc_test_read(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
}

void fmc_scan(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
}

#endif // NAND_FLASH

// end file fmc.c
