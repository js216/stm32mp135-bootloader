// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file eth.h
 * @brief Ethernet test
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "eth.h"
#include "mcp23x17.h"
#include <stdint.h>

void eth_init(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc;
   (void)arg1;
   (void)arg2;
   (void)arg3;

   mcp_init();
   mcp_set_pin_mode(MCP_PIN_9, true);
   mcp_pin_write(MCP_PIN_9, true);
}

// end file eth.c
