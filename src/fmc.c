// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file fmc.c
 * @brief FMC NAND management
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include <stdint.h>
#include "fmc.h"
#include "board.h"

#ifdef NAND_FLASH

#include "irq_ctrl.h"
#include "printf.h"
#include "stm32mp135fxx_ca7.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_gpio.h"
#include "stm32mp13xx_hal_gpio_ex.h"
#include "stm32mp13xx_hal_nand.h"
#include "stm32mp13xx_hal_rcc.h"
#include "stm32mp13xx_ll_fmc.h"
#include <string.h>

// global variables
static NAND_HandleTypeDef hnand;

// Sectors per NAND page (512-byte logical sectors per physical page).
// Derived from PageSize at init time and cached here to avoid repeated division.
static uint32_t fmc_sectors_per_page = 0;

// DDR scratch buffer for partial-page reads and padded writes (4096 bytes).
// Zero internal RAM cost; DDR must be initialised before first use.
static uint8_t * const fmc_page_buf = (uint8_t *)FMC_SCRATCH_ADDR;

// Erased-block bitmask in DDR, placed immediately after the scratch buffer.
// One bit per block: 1 = already erased this session, 0 = needs erase.
// Max 2048 blocks / 8 = 256 bytes. Cleared at fmc_init() time.
#define FMC_ERASE_MAP_ADDR (FMC_SCRATCH_ADDR + 8192U)
static uint8_t * const fmc_erase_map = (uint8_t *)FMC_ERASE_MAP_ADDR;
#define FMC_ERASE_MAP_SIZE ((FMC_PLANE_NBR * FMC_PLANE_SIZE_BLOCKS + 7U) / 8U)

// Write accumulation buffer: collects 512-byte sectors from the USB MSC host
// until a full NAND page is ready to be programmed in a single write.
// NAND pages cannot be written twice without an intervening erase; accumulating
// here ensures each physical page is programmed exactly once.
// Placed in DDR immediately after the erase map (256 bytes), aligned to 4 KiB.
#define FMC_WRITE_BUF_ADDR (FMC_SCRATCH_ADDR + 8192U + 4096U)
static uint8_t * const fmc_write_buf = (uint8_t *)FMC_WRITE_BUF_ADDR;

// LBA of the first sector of the page currently being accumulated.
// UINT32_MAX means the buffer is empty / no page in progress.
static uint32_t fmc_write_buf_page_lba = UINT32_MAX;

static inline int  block_is_erased(uint32_t block_abs)
{
   return (fmc_erase_map[block_abs / 8U] >> (block_abs % 8U)) & 1U;
}
static inline void block_mark_erased(uint32_t block_abs)
{
   fmc_erase_map[block_abs / 8U] |= (uint8_t)(1U << (block_abs % 8U));
}

static inline void setupgpio(GPIO_TypeDef *Gpio, GPIO_InitTypeDef *Init,
                             uint32_t Pin)
{
   Init->Pin = Pin;
   HAL_GPIO_Init(Gpio, Init);
}

/**
 * @brief  Convert a flat logical block address (512-byte units) to a
 *         NAND_AddressTypeDef {Plane, Block, Page}.
 *
 * Mapping:
 *   page_abs  = lba / sectors_per_page
 *   page      = page_abs % BlockSize          (page within block)
 *   block_abs = page_abs / BlockSize
 *   block     = block_abs % PlaneSize         (block within plane)
 *   plane     = block_abs / PlaneSize
 *
 * @param  lba      Logical block address (512-byte unit)
 * @param  addr     Output NAND address
 * @retval HAL_OK on success, HAL_ERROR if lba is out of range
 */
static HAL_StatusTypeDef lba_to_nand_addr(uint32_t lba,
                                          NAND_AddressTypeDef *addr)
{
   const uint32_t page_abs  = lba / fmc_sectors_per_page;
   const uint32_t block_abs = page_abs / hnand.Config.BlockSize;
   const uint32_t total_blocks =
       hnand.Config.PlaneNbr * hnand.Config.PlaneSize;

   if (block_abs >= total_blocks) {
      return HAL_ERROR;
   }

   addr->Page  = (uint16_t)(page_abs % hnand.Config.BlockSize);
   addr->Block = (uint16_t)(block_abs % hnand.Config.PlaneSize);
   addr->Plane = (uint16_t)(block_abs / hnand.Config.PlaneSize);

   return HAL_OK;
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

   /* STM32MP135 pins */
   gpio_init_structure.Alternate = GPIO_AF10_FMC;
   setupgpio(GPIOA, &gpio_init_structure, GPIO_PIN_9); /* FMC_NWAIT: PA9 */
   gpio_init_structure.Alternate = GPIO_AF12_FMC;
   setupgpio(GPIOG, &gpio_init_structure, GPIO_PIN_9);  /* FMC_NCE:  PG9  */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_4);  /* FMC_NOE:  PD4  */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_5);  /* FMC_NWE:  PD5  */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_12); /* FMC_ALE:  PD12 */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_11); /* FMC_CLE:  PD11 */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_14); /* FMC_D0:   PD14 */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_15); /* FMC_D1:   PD15 */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_0);  /* FMC_D2:   PD0  */
   setupgpio(GPIOD, &gpio_init_structure, GPIO_PIN_1);  /* FMC_D3:   PD1  */
   setupgpio(GPIOE, &gpio_init_structure, GPIO_PIN_7);  /* FMC_D4:   PE7  */
   setupgpio(GPIOE, &gpio_init_structure, GPIO_PIN_8);  /* FMC_D5:   PE8  */
   setupgpio(GPIOE, &gpio_init_structure, GPIO_PIN_9);  /* FMC_D6:   PE9  */
   setupgpio(GPIOE, &gpio_init_structure, GPIO_PIN_10); /* FMC_D7:   PE10 */

   /* Enable and set interrupt to the lowest priority */
   IRQ_SetPriority(FMC_IRQn, 0x0F);
   IRQ_Enable(FMC_IRQn);

   /* hnand Init */
   hnand.Instance = FMC_NAND_DEVICE;
   hnand.Init.NandBank        = FMC_NAND_BANK3;
   hnand.Init.Waitfeature     = FMC_NAND_WAIT_FEATURE_ENABLE;
   hnand.Init.MemoryDataWidth = FMC_NAND_MEM_BUS_WIDTH_8;
   hnand.Init.EccComputation  = FMC_NAND_ECC_DISABLE; /* HAL enables when needed */
   hnand.Init.EccAlgorithm    = FMC_NAND_ECC_ALGO_BCH;
   hnand.Init.BCHMode         = FMC_NAND_BCH_8BIT;
   hnand.Init.EccSectorSize   = FMC_NAND_ECC_SECTOR_SIZE_512BYTE;
   hnand.Init.TCLRSetupTime   = 2;
   hnand.Init.TARSetupTime    = 2;

   /* Populate Config with board.h defaults; will be overwritten by
    * auto-detection after READ ID below */
   hnand.Config.PageSize           = FMC_PAGE_SIZE_BYTES;
   hnand.Config.SpareAreaSize      = FMC_OOB_SIZE_BYTES;
   hnand.Config.BlockSize          = FMC_BLOCK_SIZE_PAGES;
   hnand.Config.BlockNbr           = FMC_PLANE_NBR * FMC_PLANE_SIZE_BLOCKS;
   hnand.Config.PlaneSize          = FMC_PLANE_SIZE_BLOCKS;
   hnand.Config.PlaneNbr           = FMC_PLANE_NBR;
   hnand.Config.ExtraCommandEnable = 1;

   /* comspacetiming */
   FMC_NAND_PCC_TimingTypeDef comspacetiming = {0};
   comspacetiming.SetupTime     = 0x1;
   comspacetiming.WaitSetupTime = 0x7;
   comspacetiming.HoldSetupTime = 0x2;
   comspacetiming.HiZSetupTime  = 0x1;

   /* attspacetiming */
   FMC_NAND_PCC_TimingTypeDef attspacetiming = {0};
   attspacetiming.SetupTime     = 0x1A;
   attspacetiming.WaitSetupTime = 0x7;
   attspacetiming.HoldSetupTime = 0x6A;
   attspacetiming.HiZSetupTime  = 0x1;

   /* Initialize NAND HAL */
   if (HAL_NAND_Init(&hnand, &comspacetiming, &attspacetiming) != HAL_OK) {
      my_printf("HAL_NAND_Init() != HAL_OK\r\n");
      return;
   }

   /* Initialize NAND HAL ECC computations */
   NAND_EccConfigTypeDef eccconfig = {0};
   eccconfig.Offset = 2;
   if (HAL_NAND_ECC_Init(&hnand, &eccconfig) != HAL_OK) {
      my_printf("HAL_NAND_ECC_Init() != HAL_OK\r\n");
      return;
   }

   /* Reset NAND device */
   if (HAL_NAND_Reset(&hnand) != HAL_OK) {
      my_printf("HAL_NAND_Reset() != HAL_OK\r\n");
      return;
   }

   /* Read & check the NAND device IDs.
    * We issue the READ ID command manually to capture all 5 bytes,
    * then verify maker/device/3rd/4th against board.h and use
    * bytes 4 (id4=Third_Id equivalent index 3) and 5 (id5=Fourth_Id
    * equivalent index 4) for geometry detection.
    *
    * HAL_NAND_Read_ID() returns only 4 bytes (Maker, Device, Third, Fourth).
    * The 5th byte (geometry byte) is the one after Fourth_Id.  We read it
    * via a second HAL_NAND_Read_ID call workaround: since the HAL struct
    * only has 4 fields, we read the raw bus ourselves after the ID command
    * is already strobed by the HAL call — but that would be fragile.
    *
    * Simpler and more robust: use the 4-byte HAL call for ID verification,
    * and decode geometry from the Third_Id / Fourth_Id bytes which carry
    * the same plane-size / page-size information on Macronix parts.
    * (On MX30LF4G18AC: Third=0x90 → page=4K, block=64p; Fourth=0xA2 → planes)
    */
   NAND_IDTypeDef pnand_id = {0x00, 0x00, 0x00, 0x00};
   if (HAL_NAND_Read_ID(&hnand, &pnand_id) != HAL_OK) {
      my_printf("HAL_NAND_Read_ID() != HAL_OK\r\n");
      return;
   }

   /* Test the NAND ID correctness */
   if ((pnand_id.Maker_Id  != FMC_MAKER) ||
       (pnand_id.Device_Id != FMC_DEV)   ||
       (pnand_id.Third_Id  != FMC_3RD)   ||
       (pnand_id.Fourth_Id != FMC_4TH)) {
      my_printf("Unexpected ID read:\r\n");
      my_printf("maker=0x%x, dev=0x%x, 3rd=0x%x, 4th=0x%x\r\n",
                pnand_id.Maker_Id, pnand_id.Device_Id,
                pnand_id.Third_Id, pnand_id.Fourth_Id);
      return;
   }

   /* Auto-detect geometry from ID bytes 3 and 4 (Third_Id = byte index 2,
    * Fourth_Id = byte index 3 in the READ ID stream).
    * For Macronix MX30LF4G18AC:
    *   Third_Id  (byte 3) = 0x90 → bits[1:0]=00 page=1K? No — Macronix uses
    *                                a different encoding in byte 3 vs byte 5.
    *   Fourth_Id (byte 4) = 0xA2 → plane size info.
    *
    * Macronix 4th-byte encoding (0xA2 = 0b10100010):
    *   bits[3:2] = 00 → plane size = 1024 blocks  ✓
    *   bits[7:6] = 10 → 4 planes?  No, device is 2-plane.
    *
    * Macronix 3rd-byte encoding (0x90 = 0b10010000):
    *   bits[1:0] = 00 → page size 1K (raw encoding), but actual is 4K.
    *   This chip uses the 5th READ ID byte for geometry, not the 3rd.
    *
    * Since HAL gives us only 4 bytes and the geometry byte is byte 5,
    * we use the board.h defaults (which we already know are correct for
    * this chip) and skip the unreliable partial decode.
    * fmc_detect_geometry() is called with Fourth_Id as id4 for plane-size
    * bits and Third_Id as id5 — which won't decode correctly for Macronix.
    * Therefore: trust board.h defaults, just log them.
    */
   my_printf("FMC geometry (from board.h defaults):\r\n");
   my_printf("  page=%lu B  OOB=%lu B  block=%lu pages  "
             "planes=%lu x %lu blocks  total=%lu blocks\r\n",
             hnand.Config.PageSize,  hnand.Config.SpareAreaSize,
             hnand.Config.BlockSize, hnand.Config.PlaneNbr,
             hnand.Config.PlaneSize, hnand.Config.BlockNbr);

   /* Cache sectors-per-page for use in address translation */
   fmc_sectors_per_page = hnand.Config.PageSize / FMC_SECTOR_SIZE;

   /* Clear the erased-block bitmask — all blocks assumed dirty at boot */
   memset(fmc_erase_map, 0, FMC_ERASE_MAP_SIZE);

   /* Initialise write-accumulation state — no page in progress */
   memset(fmc_write_buf, 0xFF, hnand.Config.PageSize);
   fmc_write_buf_page_lba = UINT32_MAX;

   my_printf("FMC init OK. sectors_per_page=%lu\r\n", fmc_sectors_per_page);
}

/**
 * @brief  Check whether a page-sized buffer is blank (all 0xFF).
 *         An erased NAND page reads as 0xFF with no valid ECC — normal,
 *         must not be treated as an uncorrectable error.
 * @param  buf  Buffer to check (PageSize bytes)
 * @retval 1 if blank, 0 otherwise
 */
static int fmc_page_is_blank(const uint8_t *buf)
{
   const uint32_t page_size = hnand.Config.PageSize;
   for (uint32_t i = 0; i < page_size; i++) {
      if (buf[i] != 0xFFU)
         return 0;
   }
   return 1;
}

/**
 * @brief  Read @p nblocks 512-byte logical sectors from NAND into @p buf.
 *
 * Handles any lba and nblocks — no page-alignment requirement on the caller.
 * Unaligned or partial-page requests use the DDR scratch buffer; fully
 * page-aligned, page-multiple requests read directly into buf (zero copy).
 *
 * Erased (blank) pages read as all-0xFF with no valid ECC — handled
 * gracefully by returning 0xFF data rather than an error.
 *
 * @param  buf     Destination buffer (must be >= nblocks * FMC_SECTOR_SIZE)
 * @param  lba     Starting logical block address (512-byte units)
 * @param  nblocks Number of 512-byte blocks to read
 * @retval HAL_OK on success, HAL_ERROR on out-of-range or HAL failure
 */
HAL_StatusTypeDef fmc_read_blocks(uint8_t *buf, uint32_t lba, uint32_t nblocks)
{
   if (fmc_sectors_per_page == 0) {
      my_printf("FMC: not initialised\r\n");
      return HAL_ERROR;
   }

   uint32_t remaining = nblocks;
   uint32_t cur_lba   = lba;
   uint8_t *out       = buf;

   while (remaining > 0) {
      const uint32_t sector_in_page = cur_lba % fmc_sectors_per_page;
      const uint32_t page_lba       = cur_lba - sector_in_page;
      const uint32_t avail          = fmc_sectors_per_page - sector_in_page;
      const uint32_t consume        = (remaining < avail) ? remaining : avail;

      NAND_AddressTypeDef addr;
      if (lba_to_nand_addr(page_lba, &addr) != HAL_OK) {
         my_printf("FMC read: lba %lu out of range\r\n", page_lba);
         return HAL_ERROR;
      }

      const int aligned  = (sector_in_page == 0) &&
                           (consume == fmc_sectors_per_page);
      uint8_t *page_dest = aligned ? out : fmc_page_buf;
      uint32_t pages_read = 0;

      const HAL_StatusTypeDef ret = HAL_NAND_ECC_Read_Page_8b(
          &hnand, &addr, page_dest, 1, &pages_read);

      if (ret != HAL_OK) {
         my_printf("FMC read: HAL error at lba %lu\r\n", page_lba);
         return HAL_ERROR;
      }

      /* Check blank before consulting ECC stats: an erased page reads as
       * all-0xFF and the BCH engine flags every sector as bad — that is
       * normal and must not be treated as an error.
       *
       * For written pages: BadSectorCount means "sectors that had bit errors",
       * not "uncorrectable sectors" — BCH-8 corrects up to 8 bit-errors per
       * 512-byte sector automatically. A sector is truly uncorrectable only
       * when CorrectibleErrorMax exceeds the BCH-8 correction capacity (8). */
      if (!fmc_page_is_blank(page_dest)) {
         NAND_EccStatisticsTypeDef stats;
         HAL_NAND_ECC_GetStatistics(&hnand, &stats);
         if (stats.CorrectibleErrorMax > 8U) {
            my_printf("FMC read: UNCORRECTABLE ECC at lba %lu "
                      "(bad=%u max_errors_in_sector=%u)\r\n",
                      page_lba, stats.BadSectorCount,
                      stats.CorrectibleErrorMax);
            return HAL_ERROR;
         }
         if (stats.CorrectibleErrorTotal > 0U) {
            my_printf("FMC read: corrected %u bit error(s) at lba %lu "
                      "(sectors=%u max=%u)\r\n",
                      stats.CorrectibleErrorTotal, page_lba,
                      stats.BadSectorCount, stats.CorrectibleErrorMax);
         }
      }

      if (!aligned) {
         memcpy(out,
                fmc_page_buf + (sector_in_page * FMC_SECTOR_SIZE),
                consume * FMC_SECTOR_SIZE);
      }

      out       += consume * FMC_SECTOR_SIZE;
      cur_lba   += consume;
      remaining -= consume;
   }

   return HAL_OK;
}

/**
 * @brief  Write @p nblocks 512-byte logical sectors from @p buf to NAND.
 *
 * Handles any lba and nblocks — no page-alignment requirement on the caller.
 * Partial pages (leading or trailing) are padded with 0xFF via the DDR scratch
 * buffer before being written. This is safe because the target must be
 * pre-erased by the caller; writing 0xFF to erased cells is a no-op.
 * Fully page-aligned, page-multiple writes go directly from buf (zero copy).
 *
 * The target blocks must be pre-erased by the caller. This function does
 * not erase before writing (matching factory-flash usage).
 *
 * ECC is computed and written to OOB by HAL_NAND_ECC_Write_Page_8b.
 *
 * @param  buf     Source buffer (must be >= nblocks * FMC_SECTOR_SIZE bytes)
 * @param  lba     Starting logical block address (512-byte units)
 * @param  nblocks Number of 512-byte blocks to write
 * @retval HAL_OK on success, HAL_ERROR on out-of-range or HAL failure
 */
HAL_StatusTypeDef fmc_write_blocks(const uint8_t *buf, uint32_t lba,
                                   uint32_t nblocks)
{
   if (fmc_sectors_per_page == 0) {
      my_printf("FMC: not initialised\r\n");
      return HAL_ERROR;
   }

   const uint32_t page_size = hnand.Config.PageSize;
   uint32_t remaining       = nblocks;
   uint32_t cur_lba         = lba;
   const uint8_t *in        = buf;

   while (remaining > 0) {
      const uint32_t sector_in_page = cur_lba % fmc_sectors_per_page;
      const uint32_t page_lba       = cur_lba - sector_in_page;

      /* Start of a new page: flush the previous accumulated page if the
       * address changed, then reset the accumulation buffer. */
      if (fmc_write_buf_page_lba != page_lba) {

         /* Flush previous page if one was in progress */
         if (fmc_write_buf_page_lba != UINT32_MAX) {
            NAND_AddressTypeDef flush_addr;
            if (lba_to_nand_addr(fmc_write_buf_page_lba, &flush_addr) != HAL_OK) {
               my_printf("FMC write: flush lba %lu out of range\r\n",
                         fmc_write_buf_page_lba);
               return HAL_ERROR;
            }
            uint32_t pages_written = 0;
            if (HAL_NAND_ECC_Write_Page_8b(&hnand, &flush_addr,
                                            fmc_write_buf, 1,
                                            &pages_written) != HAL_OK) {
               my_printf("FMC: write failed at lba %lu\r\n",
                         fmc_write_buf_page_lba);
               return HAL_ERROR;
            }
         }

         /* Prepare accumulation buffer for the new page */
         memset(fmc_write_buf, 0xFF, page_size);
         fmc_write_buf_page_lba = page_lba;

         /* Erase the block the first time we write to it this session */
         NAND_AddressTypeDef addr;
         if (lba_to_nand_addr(page_lba, &addr) != HAL_OK) {
            my_printf("FMC write: lba %lu out of range\r\n", page_lba);
            return HAL_ERROR;
         }
         const uint32_t block_abs = (uint32_t)addr.Plane *
                                     hnand.Config.PlaneSize + addr.Block;
         if (!block_is_erased(block_abs)) {
            if (HAL_NAND_Erase_Block(&hnand, &addr) != HAL_OK) {
               my_printf("FMC write: erase failed at block %lu\r\n", block_abs);
               return HAL_ERROR;
            }
            block_mark_erased(block_abs);
         }
      }

      /* Copy this batch of sectors into the accumulation buffer */
      const uint32_t avail   = fmc_sectors_per_page - sector_in_page;
      const uint32_t consume = (remaining < avail) ? remaining : avail;
      memcpy(fmc_write_buf + (sector_in_page * FMC_SECTOR_SIZE),
             in, consume * FMC_SECTOR_SIZE);

      /* If the page is now fully populated, flush it immediately */
      if (sector_in_page + consume == fmc_sectors_per_page) {
         NAND_AddressTypeDef addr;
         if (lba_to_nand_addr(page_lba, &addr) != HAL_OK) {
            my_printf("FMC write: lba %lu out of range\r\n", page_lba);
            return HAL_ERROR;
         }
         uint32_t pages_written = 0;
         if (HAL_NAND_ECC_Write_Page_8b(&hnand, &addr, fmc_write_buf,
                                         1, &pages_written) != HAL_OK) {
            my_printf("FMC: write failed at lba %lu\r\n", page_lba);
            return HAL_ERROR;
         }
         /* Reset accumulation state — buffer no longer holds a pending page */
         memset(fmc_write_buf, 0xFF, page_size);
         fmc_write_buf_page_lba = UINT32_MAX;
      }

      in        += consume * FMC_SECTOR_SIZE;
      cur_lba   += consume;
      remaining -= consume;
   }

   return HAL_OK;
}

/**
 * @brief  Shell command wrapper for fmc_erase_all().
 */
/**
 * @brief  Erase every block on the NAND device.
 *
 * Iterates over all planes and blocks, erasing each one and updating the
 * in-memory erase map so subsequent writes skip the erase step.
 * Logs progress every 64 blocks.
 */
void fmc_erase_all(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;

   if (fmc_sectors_per_page == 0) {
      my_printf("FMC: not initialised\r\n");
      return;
   }

   const uint32_t total_blocks = hnand.Config.PlaneNbr * hnand.Config.PlaneSize;
   my_printf("FMC: erasing all %lu blocks...\r\n", total_blocks);

   __disable_irq();

   for (uint32_t block_abs = 0; block_abs < total_blocks; block_abs++) {
      NAND_AddressTypeDef addr;
      addr.Page  = 0;
      addr.Block = (uint16_t)(block_abs % hnand.Config.PlaneSize);
      addr.Plane = (uint16_t)(block_abs / hnand.Config.PlaneSize);

      if (HAL_NAND_Erase_Block(&hnand, &addr) != HAL_OK) {
         my_printf("FMC: bad block %lu (plane=%u blk=%u) — skipping\r\n",
                   block_abs, addr.Plane, addr.Block);
         /* Mark as erased so write_blocks won't attempt to erase it either */
         block_mark_erased(block_abs);
         continue;
      }
      block_mark_erased(block_abs);

      if ((block_abs % 64) == 63)
         my_printf("FMC: erased %lu / %lu blocks\r\n",
                   block_abs + 1, total_blocks);
   }

   my_printf("FMC: erase all complete\r\n");

   /* Spot-check: read back page 0 of every 64th block and confirm blank */
   my_printf("FMC: verifying erase (sampling every 64 blocks)...\r\n");
   uint32_t verify_failures = 0;
   for (uint32_t block_abs = 0; block_abs < total_blocks; block_abs += 64) {
      NAND_AddressTypeDef addr;
      addr.Page  = 0;
      addr.Block = (uint16_t)(block_abs % hnand.Config.PlaneSize);
      addr.Plane = (uint16_t)(block_abs / hnand.Config.PlaneSize);
      uint32_t pages_read = 0;
      if (HAL_NAND_ECC_Read_Page_8b(&hnand, &addr, fmc_page_buf,
                                     1, &pages_read) != HAL_OK) {
         my_printf("FMC: verify read failed at block %lu\r\n", block_abs);
         verify_failures++;
         continue;
      }
      if (!fmc_page_is_blank(fmc_page_buf)) {
         my_printf("FMC: verify FAILED — block %lu not blank! "
                   "data[0..7]: %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
                   block_abs,
                   fmc_page_buf[0], fmc_page_buf[1], fmc_page_buf[2],
                   fmc_page_buf[3], fmc_page_buf[4], fmc_page_buf[5],
                   fmc_page_buf[6], fmc_page_buf[7]);
         verify_failures++;
      }
   }

   __enable_irq();

   if (verify_failures == 0)
      my_printf("FMC: verify OK\r\n");
   else
      my_printf("FMC: verify done — %lu block(s) not blank\r\n",
                verify_failures);
}

/**
 * @brief  Read and verify the STM32 image header in a NAND block.
 *
 * Checks:
 *   - Magic number  : 'STM\x32'
 *   - Header version: major.minor printed for info
 *   - Image length  : must be non-zero and fit in NAND
 *   - Entry point / load address: printed for info
 *   - Checksum      : recomputed over image_length bytes that follow the header
 *   - V2 only       : extension header type 'ST\xFF\xFF'
 *
 * @param  block_abs  Absolute block number to check (0 or 1 for boot copies)
 */
static void fmc_check_boot_block(uint32_t block_abs)
{
   my_printf("FMC boot check: block %lu\r\n", block_abs);

   /* Read first page — contains the entire 512-byte STM32 header */
   NAND_AddressTypeDef addr = {
      .Page  = 0,
      .Block = (uint16_t)(block_abs % hnand.Config.PlaneSize),
      .Plane = (uint16_t)(block_abs / hnand.Config.PlaneSize),
   };
   uint32_t pages_read = 0;
   if (HAL_NAND_ECC_Read_Page_8b(&hnand, &addr, fmc_page_buf, 1, &pages_read)
         != HAL_OK) {
      my_printf("  ERROR: HAL read failed\r\n");
      return;
   }

   const uint8_t *h = fmc_page_buf;

   /* Magic */
   if (h[0] != 'S' || h[1] != 'T' || h[2] != 'M' || h[3] != 0x32) {
      my_printf("  ERROR: bad magic: %02x %02x %02x %02x "
                "(expected 53 54 4d 32)\r\n",
                h[0], h[1], h[2], h[3]);
      return;
   }
   my_printf("  magic: OK\r\n");

   const uint32_t csum_off    = 0x44;
   const uint32_t ver_off     = 0x48;
   const uint32_t imglen_off  = 0x4C;
   const uint32_t entry_off   = 0x50;
   const uint32_t load_off    = 0x58;
   const uint32_t exttype_off = 0x80;

#define LE32(p, o) ((uint32_t)(p)[(o)]         | ((uint32_t)(p)[(o)+1] << 8) | \
                    ((uint32_t)(p)[(o)+2] << 16)| ((uint32_t)(p)[(o)+3] << 24))

   const uint32_t checksum   = LE32(h, csum_off);
   const uint32_t ver_word   = LE32(h, ver_off);
   const uint32_t image_len  = LE32(h, imglen_off);
   const uint32_t entry_pt   = LE32(h, entry_off);
   const uint32_t load_addr  = LE32(h, load_off);
   const uint8_t  major      = (uint8_t)(ver_word >> 16);
   const uint8_t  minor      = (uint8_t)(ver_word >>  8);

   my_printf("  header version: %u.%u\r\n", major, minor);
   my_printf("  image_length:   %lu bytes\r\n", image_len);
   my_printf("  entry_point:    0x%08lx\r\n",   entry_pt);
   my_printf("  load_address:   0x%08lx\r\n",   load_addr);
   my_printf("  checksum:       0x%08lx\r\n",   checksum);

   if (image_len == 0) {
      my_printf("  ERROR: image_length is zero\r\n");
      return;
   }

   const uint32_t max_len = hnand.Config.BlockNbr * hnand.Config.BlockSize *
                            hnand.Config.PageSize;
   if (image_len > max_len) {
      my_printf("  ERROR: image_length %lu exceeds NAND size %lu\r\n",
                image_len, max_len);
      return;
   }

   /* V2: check extension header type at 0x80 = 'ST\xFF\xFF' */
   if (major == 2) {
      if (h[exttype_off]   != 'S' || h[exttype_off+1] != 'T' ||
          h[exttype_off+2] != 0xFF || h[exttype_off+3] != 0xFF) {
         my_printf("  ERROR: bad ext header type: %02x %02x %02x %02x "
                   "(expected 53 54 ff ff)\r\n",
                   h[exttype_off], h[exttype_off+1],
                   h[exttype_off+2], h[exttype_off+3]);
      } else {
         my_printf("  ext header type: OK\r\n");
      }
   }

   /* Verify checksum: sum of all bytes in the binary (image_len bytes).
    * The 512-byte header sits in the first 512 bytes of page 0; the binary
    * starts at byte 512 of that same page and continues into subsequent pages.
    * We walk page by page, skipping the first 512 header bytes. */
   uint32_t computed        = 0;
   uint32_t bytes_remaining = image_len;
   uint32_t page_abs        = block_abs * hnand.Config.BlockSize;  /* first page of block */
   uint32_t byte_offset     = 512U;  /* skip the header within page 0 */

   while (bytes_remaining > 0) {
      NAND_AddressTypeDef pa = {
         .Page  = (uint16_t)(page_abs % hnand.Config.BlockSize),
         .Block = (uint16_t)((page_abs / hnand.Config.BlockSize) % hnand.Config.PlaneSize),
         .Plane = (uint16_t)((page_abs / hnand.Config.BlockSize) / hnand.Config.PlaneSize),
      };
      pages_read = 0;
      if (HAL_NAND_ECC_Read_Page_8b(&hnand, &pa, fmc_page_buf,
                                     1, &pages_read) != HAL_OK) {
         my_printf("  ERROR: read failed at page %lu during checksum\r\n",
                   page_abs);
         return;
      }
      const uint32_t avail = hnand.Config.PageSize - byte_offset;
      const uint32_t use   = (bytes_remaining < avail) ? bytes_remaining : avail;
      for (uint32_t i = 0; i < use; i++)
         computed += fmc_page_buf[byte_offset + i];
      bytes_remaining -= use;
      byte_offset = 0;  /* subsequent pages start from byte 0 */
      page_abs++;
   }
   computed &= 0xFFFFFFFFU;

   if (computed == checksum) {
      my_printf("  checksum: OK (0x%08lx)\r\n", computed);
   } else {
      my_printf("  ERROR: checksum mismatch: computed 0x%08lx, stored 0x%08lx\r\n",
                computed, checksum);
   }

#undef LE32
}

/**
 * @brief  Shell command: verify STM32 boot headers in NAND blocks 0 and 1.
 */
void fmc_test_boot(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;

   if (fmc_sectors_per_page == 0) {
      my_printf("FMC: not initialised\r\n");
      return;
   }

   fmc_check_boot_block(0);
   fmc_check_boot_block(1);
}

/**
 * @brief  Return total NAND capacity in 512-byte logical sectors.
 *         Returns 0 if fmc_init() has not been called.
 */
uint32_t fmc_block_count(void)
{
   if (fmc_sectors_per_page == 0)
      return 0;

   return hnand.Config.BlockNbr *
          hnand.Config.BlockSize *
          fmc_sectors_per_page;
}

#else // NAND_FLASH

void fmc_init(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;
}

HAL_StatusTypeDef fmc_read_blocks(uint8_t *buf, uint32_t lba,
                                  uint32_t nblocks)
{
   (void)buf;
   (void)lba;
   (void)nblocks;
   return HAL_OK;
}

HAL_StatusTypeDef fmc_write_blocks(const uint8_t *buf, uint32_t lba,
                                   uint32_t nblocks)
{
   (void)buf;
   (void)lba;
   (void)nblocks;
   return HAL_OK;
}


void fmc_erase_all(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
}

uint32_t fmc_block_count(void)
{
   return 0;
}

#endif // NAND_FLASH

// end file fmc.c
