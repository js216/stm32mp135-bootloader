// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file fmc.h
 * @brief FMC NAND management
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#ifndef FMC_H
#define FMC_H

#include "stm32mp13xx_hal_def.h"
#include <stdint.h>

void fmc_init(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);

/* USB MSC interface.  lba and n are in NAND pages (FMC_PAGE_SIZE_BYTES each). */
HAL_StatusTypeDef fmc_read_blocks(uint8_t *buf, uint32_t lba, uint32_t n);
HAL_StatusTypeDef fmc_write_blocks(const uint8_t *buf, uint32_t lba, uint32_t n);
uint32_t          fmc_block_count(void);  /* returns total pages */

void fmc_erase_all  (int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void fmc_scan       (int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void fmc_test_boot  (int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void fmc_test_write (int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void fmc_test_read  (int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif // FMC_H
