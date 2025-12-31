// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file boot.h
 * @brief Bootloading procedures
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#ifndef BOOT_H
#define BOOT_H

#include <stdint.h>

void boot_jump(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif // BOOT_H
