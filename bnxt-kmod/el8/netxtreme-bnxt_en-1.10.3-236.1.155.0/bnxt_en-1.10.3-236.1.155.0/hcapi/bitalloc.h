/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _BITALLOC_H_
#define _BITALLOC_H_

#include <linux/types.h>
#include <linux/bitops.h>

struct bitalloc {
	u32		size;
	u32		free_count;
	unsigned long	*bitmap;
};

#define BITALLOC_SIZEOF(size) (sizeof(struct bitalloc) + ((size) + 31) / 32)
#define BITALLOC_MAX_SIZE (32 * 32 * 32 * 32 * 32 * 32)

/* Initialize the struct bitalloc and alloc bitmap memory */
int bnxt_ba_init(struct bitalloc *pool, int size, bool free);

/* Deinitialize the struct bitalloc and free bitmap memory */
void bnxt_ba_deinit(struct bitalloc *pool);

/* Allocate a lowest free index */
int bnxt_ba_alloc(struct bitalloc *pool);

/* Allocate the given index */
int bnxt_ba_alloc_index(struct bitalloc *pool, int index);

/* Allocate a highest free index */
int bnxt_ba_alloc_reverse(struct bitalloc *pool);

/* Test if index is in use */
int bnxt_ba_inuse(struct bitalloc *pool, int index);

/* Test if index is in use, but also free the index */
int bnxt_ba_inuse_free(struct bitalloc *pool, int index);

/* Find the next index is in use from a given index */
int bnxt_ba_find_next_inuse(struct bitalloc *pool, int index);

/* Find the next index is in use from a given index, and also free it */
int bnxt_ba_find_next_inuse_free(struct bitalloc *pool, int index);

/* Free the index */
int bnxt_ba_free(struct bitalloc *pool, int index);

/* Available number of indexes for allocation */
int bnxt_ba_free_count(struct bitalloc *pool);

/* Number of indexes that are allocated */
int bnxt_ba_inuse_count(struct bitalloc *pool);

#endif /* _BITALLOC_H_ */
