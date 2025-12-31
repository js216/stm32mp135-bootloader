// SPDX-License-Identifier: LicenseRef-SLA0044

/**
 * @file usbd_msc_storage.h
 * @brief Debugging and diagnostics.
 * @author MCD Application Team
 * @copyright 2015 STMicroelectronics
 */

#include "usbd_msc_storage.h"
#include "sd.h"
#include "stm32mp13xx_hal_def.h"
#include "stm32mp13xx_hal_sd.h"

#define STORAGE_LUN_NBR 1U
#define STORAGE_BLK_NBR 0x100000U
#define STORAGE_BLK_SIZ 0x200U

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

uint8_t STORAGE_Inquirydata[] = /* 36 */
    {

        /* LUN 0 */
        0x00, 0x80, 0x02, 0x02, (STANDARD_INQUIRY_DATA_LEN - 5),
        0x00, 0x00, 0x00, 'S',  'T',
        'M',  ' ',  ' ',  ' ',  ' ',
        ' ', /* Manufacturer : 8 bytes */
        'P',  'r',  'o',  'd',  'u',
        'c',  't',  ' ', /* Product      : 16 Bytes */
        ' ',  ' ',  ' ',  ' ',  ' ',
        ' ',  ' ',  ' ',  '0',  '.',
        '0',  '1', /* Version      : 4 Bytes */
};

USBD_StorageTypeDef USBD_MSC_fops = {
    STORAGE_Init,      STORAGE_GetCapacity,
    STORAGE_IsReady,   STORAGE_IsWriteProtected,
    STORAGE_Read,      STORAGE_Write,
    STORAGE_GetMaxLun, STORAGE_Inquirydata,

};

/**
 * @brief  Initializes the storage unit (medium)
 * @param  lun: Logical unit number
 * @retval Status (0 : OK / -1 : Error)
 */
uint8_t STORAGE_Init(uint8_t lun)
{
   UNUSED(lun);

   return (0);
}

/**
 * @brief  Returns the medium capacity.
 * @param  lun: Logical unit number
 * @param  block_num: Number of total block number
 * @param  block_size: Block size
 * @retval Status (0: OK / -1: Error)
 */
uint8_t STORAGE_GetCapacity(uint8_t lun, uint32_t *block_num,
                            uint16_t *block_size)
{
   UNUSED(lun);

   *block_num  = STORAGE_BLK_NBR;
   *block_size = STORAGE_BLK_SIZ;
   return (0);
}

/**
 * @brief  Checks whether the medium is ready.
 * @param  lun: Logical unit number
 * @retval Status (0: OK / -1: Error)
 */
uint8_t STORAGE_IsReady(uint8_t lun)
{
   UNUSED(lun);

   return (0);
}

/**
 * @brief  Checks whether the medium is write protected.
 * @param  lun: Logical unit number
 * @retval Status (0: write enabled / -1: otherwise)
 */
uint8_t STORAGE_IsWriteProtected(uint8_t lun)
{
   UNUSED(lun);

   return 0;
}

/**
 * @brief  Reads data from the SD card into the USB MSC buffer.
 * @param  lun       Logical unit number (unused)
 * @param  buf       Output buffer for USB MSC
 * @param  blk_addr  Logical block address to read from SD
 * @param  blk_len   Number of 512-byte blocks to read
 * @retval 0 on success, -1 on error
 */
uint8_t STORAGE_Read(uint8_t lun, uint8_t *buf, uint32_t blk_addr,
                     uint16_t blk_len)
{
   (void)lun;

   if (HAL_SD_ReadBlocks(&sd_handle, buf, blk_addr, blk_len, 3000) != HAL_OK) {
      return USBD_FAIL;
   }

   while (HAL_SD_GetCardState(&sd_handle) != HAL_SD_CARD_TRANSFER)
      ; // wait

   return USBD_OK;
}

/**
 * @brief  Writes data into the medium.
 * @param  lun: Logical unit number
 * @param  buf: data buffer
 * @param  blk_addr: Logical block address
 * @param  blk_len: Blocks number
 * @retval Status (0 : OK / -1 : Error)
 */
uint8_t STORAGE_Write(uint8_t lun, uint8_t *buf, uint32_t blk_addr,
                      uint16_t blk_len)
{
   (void)lun;

   if (HAL_SD_WriteBlocks(&sd_handle, buf, blk_addr, blk_len, 3000) != HAL_OK) {
      return USBD_FAIL;
   }

   while (HAL_SD_GetCardState(&sd_handle) != HAL_SD_CARD_TRANSFER)
      ; // wait

   return USBD_OK;
}

/**
 * @brief  Returns the Max Supported LUNs.
 * @param  None
 * @retval Lun(s) number
 */
uint8_t STORAGE_GetMaxLun(void)
{
   return (STORAGE_LUN_NBR - 1);
}
