/* SPDX-License-Identifier: BSD-3-Clause */

/**
 * @file nand_pt.h
 * @brief NAND partition table layout (shared between firmware and nandimage.py).
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 *
 * Block layout:
 *   0   bootloader primary   (ROM scans blocks 0..3 for STM32 header)
 *   1   bootloader redundant
 *   2   partition table      (this struct, first page of block)
 *   3   DTB
 *   4+  kernel
 */

#ifndef NAND_PT_H
#define NAND_PT_H

#include <stdint.h>

#define NAND_PT_MAGIC      0x4E414E44U  /* "NAND" */
#define NAND_PT_VERSION    1U
#define NAND_PT_MAX_PARTS  8U

#define NAND_BLOCK_BOOT    0U  /* bootloader (primary + redundant, 2 blocks) */
#define NAND_BLOCK_PT      2U  /* partition table */
#define NAND_BLOCK_DTB     3U  /* device tree blob */
#define NAND_BLOCK_KERNEL  4U  /* kernel image */

typedef struct {
   char     name[16];
   uint32_t start_block;
   uint32_t num_blocks;
} nand_part_t;

/* Total size: 4+4+4+4 + 8*24 + 4 = 212 bytes; fits in one NAND page. */
typedef struct {
   uint32_t    magic;
   uint32_t    version;
   uint32_t    total_blocks;  /* total used blocks; default n for fmc_flush */
   uint32_t    num_parts;
   nand_part_t parts[NAND_PT_MAX_PARTS];
   uint32_t    checksum;      /* sum of all preceding bytes, mod 2^32 */
} nand_pt_t;

#endif /* NAND_PT_H */
