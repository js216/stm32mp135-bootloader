// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file setup.h
 * @brief Driver and board low-level setup
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#ifndef SETUP_H
#define SETUP_H

#include "stm32mp13xx_hal.h"
#include "stm32mp13xx_hal_ddr.h"
#include "stm32mp13xx_hal_pcd.h"
#include "stm32mp13xx_hal_sd.h"
#include "stm32mp13xx_hal_uart.h"

void sysclk_init(void);
void pmic_init(void);
void perclk_init(void);
void etzpc_init(void);
void gpio_init(void);
void uart4_init(void);
void gic_init(void);
void mmu_init(void);
void usb_init(void);

#endif // SETUP_H

// end file setup.h
