// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file reg.h
 * @brief SoC register monitoring and printouts.
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#ifndef REG_H
#define REG_H

#include <stdint.h>

void reg_dump(int x0, uint32_t x1, uint32_t x2, uint32_t x3);

#endif // REG_H

// end file reg.h
