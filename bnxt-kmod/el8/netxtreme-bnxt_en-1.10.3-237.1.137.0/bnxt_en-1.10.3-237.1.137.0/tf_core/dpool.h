/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _DPOOL_H_
#define _DPOOL_H_

#include <linux/types.h>

#define DP_MAX_FREE_SIZE 0x8000 /* 32K */

#define DP_INVALID_INDEX 0xffffffff

#define DP_FLAGS_START   0x80000000
#define DP_IS_START(flags) ((flags) & DP_FLAGS_START)

#define DP_FLAGS_SIZE_SHIFT 0
#define DP_FLAGS_SIZE_MASK  0x07

#define DP_FLAGS_SIZE(flags) (((flags) >> DP_FLAGS_SIZE_SHIFT) & DP_FLAGS_SIZE_MASK)

#define DP_IS_FREE(flags) (DP_FLAGS_SIZE(flags) == 0)
#define DP_IS_USED(flags) (DP_FLAGS_SIZE(flags) != 0)

#define DP_DEFRAG_NONE   0x0
#define DP_DEFRAG_ALL    0x1
#define DP_DEFRAG_TO_FIT 0x2

/**
 * Free list entry
 *
 * @index:	Index in to dpool entry array
 * @size:	The size of the entry in the dpool entry array
 *
 * Each entry includes an index in to the dpool entry array
 * and the size of dpool array entry.
 */
struct dpool_free_list_entry {
	u32 index;
	u32 size;
};

/**
 * Free list
 *
 * @size:	Number of entries in the free list
 * @entry:	List of unused entries in the dpool entry array
 *
 * Used internally to record free entries in the dpool entry array.
 * Each entry represents a single or multiple contiguous entries
 * in the dpool entry array.
 *
 * Used only during the defrag operation.
 */
struct dpool_free_list {
	u32				size;
	struct dpool_free_list_entry	entry[DP_MAX_FREE_SIZE];
};

/**
 * Adjacent list entry
 *
 * @index:	Index in to dpool entry array
 * @size:	The size of the entry in the dpool entry array
 * @left:	Number of free entries directly to the  left of this entry
 * @right:	Number of free entries directly to the right of this entry
 *
 * Each entry includes an index in to the dpool entry array,
 * the size of the entry and the counts of free entries to the
 * right and left off that entry.
 */
struct dpool_adj_list_entry {
	u32 index;
	u32 size;
	u32 left;
	u32 right;
};

/**
 * Adjacent list
 *
 * @size:	Number of entries in the adj list
 * @entry:	List of entries in the dpool entry array that have
 *		free entries directly to their left and right.
 *
 * A list of references to entries in the dpool entry array that
 * have free entries to the left and right. Since we pack to the
 * left entries will always have a non zero left out.
 *
 * Used only during the defrag operation.
 */
struct dpool_adj_list {
	u32				size;
	struct dpool_adj_list_entry	entry[DP_MAX_FREE_SIZE];
};

/**
 * Dpool entry
 *
 * Each entry includes flags and the FW index.
 */
struct dpool_entry {
	u32 flags;
	u32 index;
	u64 entry_data;
};

/**
 * Dpool
 *
 * Used to manage resource pool. Includes the start FW index, the
 * size of the entry array and the entry array it's self.
 */
struct dpool {
	u32			start_index;
	u32			size;
	u8			max_alloc_size;
	void			*user_data;
	int			(*move_callback)(void *user_data,
						 u64 entry_data,
						 u32 new_index);
	struct dpool_entry	*entry;
};

/**
 * dpool_init
 *
 * Initialize the dpool
 *
 * @dpool:	Pointer to a dpool structure that includes an entry field
 *		that points to the entry array. The user is responsible for
 *		allocating memory for the dpool struct and the entry array.
 * @start_index:	The base index to use.
 * @size:		The number of entries
 * @max_alloc_size:	The number of entries
 * @user_data:		Pointer to user data. Will be passed in callbacks.
 * @move_callback:	Pointer to move EM entry callback.
 *
 * Return
 *      -  0 on success
 *      - -1 on failure
 */
int dpool_init(struct dpool *dpool, u32 start_index, u32 size,
	       u8 max_alloc_size, void *user_data,
	       int (*move_callback)(void *, u64, u32));

/**
 * dpool_alloc
 *
 * Request a FW index of size and if necessary de-fragment the dpool
 * array.
 *
 * @dpool:	The dpool
 * @size:	The size of the requested allocation.
 * @defrag:	Operation to apply when there is insufficient space:
 *
 *		DP_DEFRAG_NONE   (0x0) - Don't do anything.
 *		DP_DEFRAG_ALL    (0x1) - Defrag until there is nothing left
 *					 to defrag.
 *		DP_DEFRAG_TO_FIT (0x2) - Defrag until there is just enough
 *					 space to insert the requested
 *					 allocation.
 *
 * Return
 *      - FW index on success
 *      - DP_INVALID_INDEX on failure
 */
u32 dpool_alloc(struct dpool *dpool, u32 size, u8 defrag);

/**
 * dpool_set_entry_data
 *
 * Set the entry data field. This will be passed to callbacks.
 *
 * @dpool:	The dpool
 * @index:	FW index
 * @entry_data:	Entry data value
 *
 * Return
 *      - FW index on success
 *      - DP_INVALID_INDEX on failure
 */
int dpool_set_entry_data(struct dpool *dpool, u32 index, u64 entry_data);

/**
 * dpool_free
 *
 * Free allocated entry. The is responsible for the dpool and dpool
 * entry array memory.
 *
 * @dpool:	The pool
 * @index:	FW index to free up.
 *
 * Result
 *      - 0  on success
 *      - -1 on failure
 *
 */
int dpool_free(struct dpool *dpool, u32 index);

/**
 * dpool_free_all
 *
 * Free all entries.
 *
 * @dpool:	The pool
 *
 * Result
 *      - 0  on success
 *      - -1 on failure
 *
 */
void dpool_free_all(struct dpool *dpool);

/**
 * dpool_dump
 *
 * Debug/util function to dump the dpool array.
 *
 * @dpool:	The pool
 *
 */
void dpool_dump(struct dpool *dpool);

/**
 * dpool_defrag
 *
 * De-fragment the dpool array and apply the specified defrag strategy.
 *
 * @dpool:	The dpool
 * @entry_size:	If using the DP_DEFRAG_TO_FIT stratagy defrag will stop when
 *		there's at least entry_size space available.
 * @defrag:
 *     Defrag strategy:
 *
 *     DP_DEFRAG_ALL    (0x1) - Defrag until there is nothing left
 *                              to defrag.
 *     DP_DEFRAG_TO_FIT (0x2) - Defrag until there is just enough space
 *                              to insert the requested allocation.
 *
 * Return
 *      < 0 - on failure
 *      > 0 - The size of the largest free space
 */
int dpool_defrag(struct dpool *dpool, u32 entry_size, u8 defrag);

#endif /* _DPOOL_H_ */
