// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/bitmap.h>
#include "bnxt_hsi.h"
#include "bnxt_compat.h"
#include "bitalloc.h"

/**
 * bnxt_ba_init - allocate memory for bitmap
 * @pool:   Pointer to struct bitalloc
 * @free:   free=true, sets all bits to 1
 *	    In a bitmap, bit 1 means index is free and
 *	    0 means index in-use, because we need to allocate
 *	    from reverse also and in those cases we can search
 *	    via find_last_set(), dont have find_last_zero() api.
 *
 * Returns: 0 on success, -ve otherwise
 */
int bnxt_ba_init(struct bitalloc *pool, int size, bool free)
{
	if (unlikely(!pool || size < 1 || size > BITALLOC_MAX_SIZE))
		return -EINVAL;

	pool->bitmap = bitmap_zalloc(size, GFP_KERNEL);
	if (unlikely(!pool->bitmap))
		return -ENOMEM;

	if (free) {
		pool->size = size;
		pool->free_count = size;
		bitmap_set(pool->bitmap, 0, size);
	} else {
		pool->size = size;
		pool->free_count = 0;
	}

	return 0;
}

/**
 * bnxt_ba_deinit - Free the malloced memory for the bitmap
 * @pool:   Pointer to struct bitalloc
 *
 * Returns: void
 */
void bnxt_ba_deinit(struct bitalloc *pool)
{
	if (unlikely(!pool || !pool->bitmap))
		return;

	bitmap_free(pool->bitmap);
	pool->size = 0;
	pool->free_count = 0;
}

/**
 * bnxt_ba_alloc - Allocate a lowest free index
 * @pool:   Pointer to struct bitalloc
 *
 * Returns: -1 on failure, index on success
 */
int bnxt_ba_alloc(struct bitalloc *pool)
{
	int r = -1;

	if (unlikely(!pool || !pool->bitmap || !pool->free_count))
		return r;

	r = find_first_bit(pool->bitmap, pool->size);
	if (likely(r < pool->size)) {
		clear_bit(r, pool->bitmap);
		--pool->free_count;
	}
	return r;
}

/**
 * bnxt_ba_alloc_reverse - Allocate a highest free index
 * @pool:   Pointer to struct bitalloc
 *
 * Returns: -1 on failure, index on success
 */
int bnxt_ba_alloc_reverse(struct bitalloc *pool)
{
	int r = -1;

	if (unlikely(!pool || !pool->bitmap || !pool->free_count))
		return r;

	r = find_last_bit(pool->bitmap, pool->size);
	if (likely(r < pool->size)) {
		clear_bit(r, pool->bitmap);
		--pool->free_count;
	}
	return r;
}

/**
 * bnxt_ba_alloc_index - Allocate the requested index
 * @pool:   Pointer to struct bitalloc
 * @index:  Index to allocate
 *
 * Returns: -1 on failure, index on success
 */
int bnxt_ba_alloc_index(struct bitalloc *pool, int index)
{
	int r = -1;

	if (unlikely(!pool || !pool->bitmap ||
		     index < 0 || index >= (int)pool->size ||
		     !pool->free_count))
		return r;

	if (likely(test_bit(index, pool->bitmap))) {
		clear_bit(index, pool->bitmap);
		--pool->free_count;
		r = index;
	}

	return r;
}

/**
 * bnxt_ba_free - Free the requested index if allocated
 * @pool:   Pointer to struct bitalloc
 * @index:  Index to free
 *
 * Returns: -1 on failure, 0 on success
 */
int bnxt_ba_free(struct bitalloc *pool, int index)
{
	int r = -1;

	if (unlikely(!pool || !pool->bitmap ||
		     index < 0 || index >= (int)pool->size))
		return r;

	if (unlikely(test_bit(index, pool->bitmap)))
		return r;

	set_bit(index, pool->bitmap);
	pool->free_count++;
	return 0;
}

/**
 * bnxt_ba_inuse - Check if the requested index is already allocated
 * @pool:   Pointer to struct bitalloc
 * @index:  Index to check availability
 *
 * Returns: -1 on failure, 0 if it is free, 1 if it is allocated
 */
int bnxt_ba_inuse(struct bitalloc *pool, int index)
{
	if (unlikely(!pool || !pool->bitmap ||
		     index < 0 || index >= (int)pool->size))
		return -1;

	return !test_bit(index, pool->bitmap);
}

/**
 * bnxt_ba_inuse_free - Free the index if it was allocated
 * @pool:   Pointer to struct bitalloc
 * @index:  Index to be freed if it was allocated
 *
 * Returns: -1 on failure, 0 if it is free, 1 if it is in use
 */
int bnxt_ba_inuse_free(struct bitalloc *pool, int index)
{
	if (unlikely(!pool || !pool->bitmap ||
		     index < 0 || index >= (int)pool->size))
		return -1;

	if (bnxt_ba_free(pool, index) == 0)
		return 1;

	return 0;
}

/**
 * bnxt_ba_find_next_inuse - Find the next index allocated
 * @pool:   Pointer to struct bitalloc
 * @index:  Index from where to search for the next inuse index
 *
 * Returns: -1 on failure or if not found, else next index
 */
int bnxt_ba_find_next_inuse(struct bitalloc *pool, int index)
{
	int r = -1;

	if (unlikely(!pool || !pool->bitmap ||
		     index < 0 || index >= (int)pool->size))
		return r;

	r = find_next_zero_bit(pool->bitmap, pool->size, ++index);
	if (unlikely(r == pool->size))
		return -1;

	return r;
}

/**
 * bnxt_ba_find_next_inuse_free - Free the next allocated index
 * @pool:   Pointer to struct bitalloc
 * @index:  Index from where to search for the next inuse index
 *
 * Returns: -1 on failure, else next inuse index that was freed
 */
int bnxt_ba_find_next_inuse_free(struct bitalloc *pool, int index)
{
	int r = -1;

	if (unlikely(!pool || !pool->bitmap ||
		     index < 0 || index >= (int)pool->size))
		return r;

	r = find_next_zero_bit(pool->bitmap, pool->size, ++index);
	if (unlikely(r == pool->size))
		return -1;

	if (likely(bnxt_ba_free(pool, r) == 0))
		return r;

	return -1;
}

/**
 * bnxt_ba_free_count - Available indexes that can be allocated.
 * @pool:   Pointer to struct bitalloc
 *
 * Returns: 0 - size, -ve on error
 */
int bnxt_ba_free_count(struct bitalloc *pool)
{
	if (unlikely(!pool))
		return -EINVAL;

	return (int)pool->free_count;
}

/**
 * bnxt_ba_inuse_count - Number of already allocated indexes.
 * @pool:   Pointer to struct bitalloc
 *
 * Returns: 0 - size, -ve on error
 */
int bnxt_ba_inuse_count(struct bitalloc *pool)
{
	if (unlikely(!pool))
		return -EINVAL;

	return (int)(pool->size) - (int)(pool->free_count);
}
