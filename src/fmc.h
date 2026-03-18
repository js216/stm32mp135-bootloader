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

void fmc_erase_all(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void fmc_scan(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void fmc_flush(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void fmc_load(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void fmc_bload(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void fmc_test_boot(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void fmc_test_write(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void fmc_test_read(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);

/* Called by the USB MSC storage write callback to track the high-water mark
 * of host writes.  Reset to zero at the end of fmc_flush so that subsequent
 * writes (e.g. a recovery initrd) are counted independently. */
void fmc_note_usb_write(uint32_t blk_addr, uint16_t blk_len);
uint32_t fmc_usb_written_bytes(void);

#ifdef NAND_FLASH
extern volatile int fmc_flush_active;
#endif

#endif // FMC_H
