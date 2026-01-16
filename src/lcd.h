// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file lcd.h
 * @brief LCD display and CTP touch control.
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#ifndef LCD_H
#define LCD_H

#include <stdint.h>

void lcd_init(void);
void lcd_backlight(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void lcd_color(int argc, uint32_t r, uint32_t g, uint32_t b);

#endif // LCD_H
