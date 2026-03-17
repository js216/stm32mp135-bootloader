// SPDX-License-Identifier: BSD-3-Clause

#ifndef PRNG_H
#define PRNG_H

#include <stdint.h>

void prng_fill(uint8_t *buf, uint32_t len, uint64_t *state);

#endif // PRNG_H
