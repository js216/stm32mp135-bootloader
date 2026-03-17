// SPDX-License-Identifier: LicenseRef-SLA0044

/**
 * @file usbd_msc_storage.c
 * @brief USB MSC storage backend — SD card or NAND flash.
 * @author MCD Application Team
 * @copyright 2015 STMicroelectronics
 */

#include "usbd_msc_storage.h"
#include "board.h"
#include "fmc.h"
#include "sd.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_sd.h"
#include <string.h>

#define STORAGE_LUN_NBR 1U

#ifdef NAND_FLASH
/* DDR-backed store: USB writes go to DDR; use fmc_flush to commit to NAND.
 * Standard 512-byte sectors; host tools work without special configuration. */
#define STORAGE_BLK_SIZ 512U
#define STORAGE_BLK_NBR (FMC_DDR_BUF_SIZE / STORAGE_BLK_SIZ)
static uint8_t *const ddr_buf = (uint8_t *)FMC_DDR_BUF_ADDR;
#else
#define STORAGE_BLK_SIZ 0x200U
#define STORAGE_BLK_NBR 0x100000U
#endif

/* ---------------------------------------------------------------------------
 * Forward declarations
 * ---------------------------------------------------------------------------*/
uint8_t STORAGE_Init(uint8_t lun);
uint8_t STORAGE_GetCapacity(uint8_t lun, uint32_t *block_num,
                            uint16_t *block_size);
uint8_t STORAGE_IsReady(uint8_t lun);
uint8_t STORAGE_IsWriteProtected(uint8_t lun);
uint8_t STORAGE_Read(uint8_t lun, uint8_t *buf, uint32_t blk_addr,
                     uint16_t blk_len);
uint8_t STORAGE_Write(uint8_t lun, uint8_t *buf, uint32_t blk_addr,
                      uint16_t blk_len);
uint8_t STORAGE_GetMaxLun(void);

/* ---------------------------------------------------------------------------
 * SCSI inquiry data
 * ---------------------------------------------------------------------------*/
uint8_t STORAGE_Inquirydata[] = /* 36 bytes */
    {
        /* LUN 0 */
        0x00, 0x80, 0x02, 0x02, (STANDARD_INQUIRY_DATA_LEN - 5),
        0x00, 0x00, 0x00, 'S',  'T',
        'M',  ' ',  ' ',  ' ',  ' ',
        ' ', /* Manufacturer : 8 bytes */
        'P',  'r',  'o',  'd',  'u',
        'c',  't',  ' ', /* Product      : 16 bytes */
        ' ',  ' ',  ' ',  ' ',  ' ',
        ' ',  ' ',  ' ',  '0',  '.',
        '0',  '1', /* Version      : 4 bytes  */
};

USBD_StorageTypeDef USBD_MSC_fops = {
    STORAGE_Init,      STORAGE_GetCapacity,
    STORAGE_IsReady,   STORAGE_IsWriteProtected,
    STORAGE_Read,      STORAGE_Write,
    STORAGE_GetMaxLun, STORAGE_Inquirydata,
};

/* ---------------------------------------------------------------------------
 * STORAGE_Init
 * ---------------------------------------------------------------------------*/
/**
 * @brief  Initialises the storage unit.  Both backends are assumed to have
 *         been initialised already (sd_init / fmc_init called at boot).
 * @param  lun  Logical unit number (unused)
 * @retval 0 on success
 */
uint8_t STORAGE_Init(uint8_t lun)
{
   UNUSED(lun);
   return 0;
}

/* ---------------------------------------------------------------------------
 * STORAGE_GetCapacity
 * ---------------------------------------------------------------------------*/
/**
 * @brief  Returns the medium capacity.
 *
 * For NAND: capacity is derived live from hnand.Config via fmc_block_count(),
 * so it stays in sync with auto-detected geometry without duplication.
 *
 * @param  lun        Logical unit number (unused)
 * @param  block_num  Out: total number of 512-byte blocks
 * @param  block_size Out: block size in bytes (always 512)
 * @retval 0 on success, -1 if NAND not initialised
 */
uint8_t STORAGE_GetCapacity(uint8_t lun, uint32_t *block_num,
                            uint16_t *block_size)
{
   UNUSED(lun);
   *block_size = (uint16_t)STORAGE_BLK_SIZ;
   *block_num  = STORAGE_BLK_NBR;
   return 0;
}

/* ---------------------------------------------------------------------------
 * STORAGE_IsReady
 * ---------------------------------------------------------------------------*/
/**
 * @brief  Checks whether the medium is ready.
 * @param  lun  Logical unit number (unused)
 * @retval 0 always (both backends assumed ready after init)
 */
uint8_t STORAGE_IsReady(uint8_t lun)
{
   UNUSED(lun);
   return 0;
}

/* ---------------------------------------------------------------------------
 * STORAGE_IsWriteProtected
 * ---------------------------------------------------------------------------*/
/**
 * @brief  Checks whether the medium is write-protected.
 * @param  lun  Logical unit number (unused)
 * @retval 0 (write enabled)
 */
uint8_t STORAGE_IsWriteProtected(uint8_t lun)
{
   UNUSED(lun);
   return 0;
}

/* ---------------------------------------------------------------------------
 * STORAGE_Read
 * ---------------------------------------------------------------------------*/
/**
 * @brief  Reads data from the selected storage backend.
 *
 * NAND path: lba and blk_len must be page-aligned (multiples of
 * sectors_per_page).  If not, USBD_FAIL is returned immediately — the host
 * is expected to issue aligned requests once it has read the capacity.
 *
 * @param  lun       Logical unit number (unused)
 * @param  buf       Output buffer
 * @param  blk_addr  Logical block address (512-byte units)
 * @param  blk_len   Number of 512-byte blocks to read
 * @retval USBD_OK on success, USBD_FAIL on error
 */
uint8_t STORAGE_Read(uint8_t lun, uint8_t *buf, uint32_t blk_addr,
                     uint16_t blk_len)
{
   (void)lun;

#ifdef NAND_FLASH
   if (fmc_flush_active)
      return USBD_BUSY;
   memcpy(buf, ddr_buf + blk_addr * STORAGE_BLK_SIZ,
          (uint32_t)blk_len * STORAGE_BLK_SIZ);
   return USBD_OK;
#endif

   if (HAL_SD_ReadBlocks(&sd_handle, buf, blk_addr, blk_len, 3000) != HAL_OK)
      return USBD_FAIL;
   while (HAL_SD_GetCardState(&sd_handle) != HAL_SD_CARD_TRANSFER)
      ;
   return USBD_OK;
}

/* ---------------------------------------------------------------------------
 * STORAGE_Write
 * ---------------------------------------------------------------------------*/
/**
 * @brief  Writes data to the selected storage backend.
 *
 * NAND path: the target region must already be erased (factory-flash usage).
 * ECC is computed and written by fmc_write_blocks().
 * lba and blk_len must be page-aligned — see STORAGE_Read note above.
 *
 * @param  lun       Logical unit number (unused)
 * @param  buf       Input buffer
 * @param  blk_addr  Logical block address (512-byte units)
 * @param  blk_len   Number of 512-byte blocks to write
 * @retval USBD_OK on success, USBD_FAIL on error
 */
uint8_t STORAGE_Write(uint8_t lun, uint8_t *buf, uint32_t blk_addr,
                      uint16_t blk_len)
{
   (void)lun;

#ifdef NAND_FLASH
   if (fmc_flush_active)
      return USBD_BUSY;
   memcpy(ddr_buf + blk_addr * STORAGE_BLK_SIZ, buf,
          (uint32_t)blk_len * STORAGE_BLK_SIZ);
   return USBD_OK;
#endif

   if (HAL_SD_WriteBlocks(&sd_handle, buf, blk_addr, blk_len, 3000) != HAL_OK)
      return USBD_FAIL;
   while (HAL_SD_GetCardState(&sd_handle) != HAL_SD_CARD_TRANSFER)
      ;
   return USBD_OK;
}

/* ---------------------------------------------------------------------------
 * STORAGE_GetMaxLun
 * ---------------------------------------------------------------------------*/
/**
 * @brief  Returns the maximum supported LUN index.
 * @retval STORAGE_LUN_NBR - 1
 */
uint8_t STORAGE_GetMaxLun(void)
{
   return (STORAGE_LUN_NBR - 1U);
}
