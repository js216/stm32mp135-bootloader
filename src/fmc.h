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

/**
 * @brief  Read nblocks 512-byte sectors from NAND into buf.
 *         lba and nblocks must be multiples of (PageSize / 512).
 */
HAL_StatusTypeDef fmc_read_blocks(uint8_t *buf, uint32_t lba,
                                  uint32_t nblocks);

/**
 * @brief  Write nblocks 512-byte sectors from buf to NAND (with ECC).
 *         Target must be pre-erased. lba and nblocks must be multiples
 *         of (PageSize / 512).
 */
HAL_StatusTypeDef fmc_write_blocks(const uint8_t *buf, uint32_t lba,
                                   uint32_t nblocks);

/**
 * @brief  Return total NAND capacity in 512-byte logical sectors.
 *         Returns 0 if fmc_init() has not been called successfully.
 */
uint32_t fmc_block_count(void);

void fmc_erase_all(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void fmc_test_boot(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif // FMC_H
