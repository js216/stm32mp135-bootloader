// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file prng.c
 * @brief xorshift64 pseudo-random number generator
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#include "prng.h"
#include <stdint.h>

static inline uint64_t xorshift64(uint64_t *s)
{
   uint64_t x = *s;
   x ^= x << 13U;
   x ^= x >> 7U;
   x ^= x << 17U;
   *s = x;
   return x;
}

void prng_fill(uint8_t *buf, uint32_t len, uint64_t *state)
{
   uint32_t i = 0;
   for (; i + 8U <= len; i += 8U) {
      const uint64_t v = xorshift64(state);
      buf[i + 0]       = (uint8_t)(v);
      buf[i + 1]       = (uint8_t)(v >> 8U);
      buf[i + 2]       = (uint8_t)(v >> 16U);
      buf[i + 3]       = (uint8_t)(v >> 24U);
      buf[i + 4]       = (uint8_t)(v >> 32U);
      buf[i + 5]       = (uint8_t)(v >> 40U);
      buf[i + 6]       = (uint8_t)(v >> 48U);
      buf[i + 7]       = (uint8_t)(v >> 56U);
   }
   if (i < len) {
      uint64_t v = xorshift64(state);
      for (; i < len; i++, v >>= 8U)
         buf[i] = (uint8_t)v;
   }
}
