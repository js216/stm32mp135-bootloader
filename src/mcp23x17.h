// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file mpc23x17.h
 * @brief Super-simple MCP23017T-E/ML driver
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#ifndef MCP23X17_H
#define MCP23X17_H

#include <stdbool.h>
#include <stdint.h>

enum mcp_pin {
   MCP_PIN_0 = 0,
   MCP_PIN_1,
   MCP_PIN_2,
   MCP_PIN_3,
   MCP_PIN_4,
   MCP_PIN_5,
   MCP_PIN_6,
   MCP_PIN_7,
   MCP_PIN_8,
   MCP_PIN_9,
   MCP_PIN_10,
   MCP_PIN_11,
   MCP_PIN_12,
   MCP_PIN_13,
   MCP_PIN_14,
   MCP_PIN_15,
};

/**
 * @brief Initialize I2C connection to the IO expander.
 */
void mcp_init(void);

/**
 * @brief Configures a pin as either an Output or High-Z (Input).
 *
 * @param pin The specific pin index (0-15).
 * @param is_output Set to true for Output mode, false for High-Z (Input) mode.
 */
void mcp_set_pin_mode(enum mcp_pin pin, bool is_output);

/**
 * @brief Sets the logic level (High/Low) of a pin. Only works if pin is an
 * Output.
 *
 * @param pin The specific pin index (0-15).
 * @param is_high Set to true for logic High, false for logic Low.
 */
void mcp_pin_write(enum mcp_pin pin, bool is_high);

#endif // MCP23X17_H

// end file mcp23x17.h
