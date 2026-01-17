// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file cmd.h
 * @brief Command line interface
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#ifndef CMD_H
#define CMD_H

#include <stdint.h>

void cmd_init(void);
void cmd_take_char(char byte);
void cmd_poll(void);

void cmd_help(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void cmd_reset(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void cmd_load_one(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void cmd_load_two(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif // CMD_H
