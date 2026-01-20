// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file eth.h
 * @brief Ethernet test
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#ifndef ETH_H
#define ETH_H

#include <stdint.h>

void eth_init(void);
void eth_status(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);
void eth_send_test_frame(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif // ETH_H

// end file eth.h
