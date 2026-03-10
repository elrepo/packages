/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

/* Random Number Functions */
#ifndef __RAND_H__
#define __RAND_H__

/**
 * Generates a 32 bit pseudo random number
 *
 * Returns:
 * u32 number
 */
u32 rand32(void);

/**
 * Resets the seed used by the pseudo random number generator
 *
 * Returns:
 */
void rand_init(void);

#endif /* __RAND_H__ */
