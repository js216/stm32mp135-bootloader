// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file sd.h
 * @brief SD card management
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#ifndef SD_H
#define SD_H

#include "stm32mp13xx_hal_sd.h"

extern SD_HandleTypeDef sd_handle;

void sd_init(void);
void load_sd_cmd(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif // SD_H
