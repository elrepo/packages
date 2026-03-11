// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

/* Random Number Functions */

#include <linux/types.h>
#include "rand.h"

#define TF_RAND_LFSR_INIT_VALUE 0xACE1u

u16 lfsr = TF_RAND_LFSR_INIT_VALUE;
u32 bit;

/**
 * Generates a 16 bit pseudo random number
 *
 * Returns:
 *   u16 number
 */
static u16 rand16(void)
{
	bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
	return lfsr = (lfsr >> 1) | (bit << 15);
}

/**
 * Generates a 32 bit pseudo random number
 *
 * Returns:
 *   u32 number
 */
u32 rand32(void)
{
	return (rand16() << 16) | rand16();
}

/* Resets the seed used by the pseudo random number generator */
void rand_init(void)
{
	lfsr = TF_RAND_LFSR_INIT_VALUE;
	bit = 0;
}
