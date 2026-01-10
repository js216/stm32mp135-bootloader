// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file fmc.h
 * @brief FMC NAND management
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#ifndef FMC_H
#define FMC_H

#include <stdint.h>

void fmc_init(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif // FMC_H
