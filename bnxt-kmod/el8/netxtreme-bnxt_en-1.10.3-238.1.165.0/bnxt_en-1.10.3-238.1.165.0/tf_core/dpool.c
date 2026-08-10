// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include "dpool.h"

/* Dynamic Pool Allocator
 *
 * The Dynamic Pool Allocator or "dpool" supports the
 * allocation of variable size table entries intended for use
 * with SRAM based EM entries.
 *
 * Dpool maintains a list of all blocks and the current status
 * of each block. A block may be:
 *
 *     . Free, size = 0
 *     . Busy, First, size = n
 *     . Busy, size = n
 *
 * Here's an example of some dpool entries and the associated
 * EM Record Pointer Table
 *
 *     +----------------------+        +----------+
 *     |First, Busy, Size = 3 |        |  FBlock  |
 *     +----------------------+        +----------+
 *     |Busy, Size = 3        |        |  NBlock  |
 *     +----------------------+        +----------+
 *     |Busy, Size = 3        |        |  NBlock  |
 *     +----------------------+        +----------+
 *     |Free, Size = 0        |        |  Free    |
 *     +----------------------+        +----------+
 *     |Free, Size = 0        |        |  Free    |
 *     +----------------------+        +----------+
 *     |Free, Size = 0        |        |  Free    |
 *     +----------------------+        +----------+
 *     |First, Busy, Size = 2 |        |  FBlock  |
 *     +----------------------+        +----------+
 *     |Busy, Size = 2        |        |  NBlock  |
 *     +----------------------+        +----------+
 *     |Free, Size = 0        |        |  Free    |
 *     +----------------------+        +----------+
 *     |Free, Size = 0        |        |  Free    |
 *     +----------------------+        +----------+
 *     .                      .        .          .
 *     .                      .        .          .
 *     .                      .        .          .
 *
 * This shows a three block entry followed by three free
 * entries followed by a two block entry.
 *
 * Dpool supports the ability to defragment the currently
 * allocated entries. For dpool to support defragmentation
 * the firmware must support the "EM Move" HWRM. When an
 * application attempts to insert an entry it will pass an
 * argument indicating if dpool should, in the event of there
 * being insufficient space for the new entry, defragment the
 * existing entries to make space.
 */

/* dpool_init
 *
 * Initialize the dpool
 *
 * *dpool         - Pointer to the dpool structure.
 * start_index    - The lowest permited index.
 * size           -  The number of entries
 * max_alloc_size - Max size of an entry.
 * *user_data     - Pointer to memory that will be passed in
 *                  callbacks.
 * move_callback  - If the EM Move HWRM is supported in FW then
 *                  this function pointer will point to a function
 *                  that will invoke the EM Move HWRM.
 */
int dpool_init(struct dpool *dpool, u32 start_index, u32 size,
	       u8 max_alloc_size, void *user_data,
	       int (*move_callback)(void *, u64, u32))
{
	size_t len;
	u32 i;

	len = size * sizeof(struct dpool_entry);
	dpool->entry = vzalloc(len);
	if (!dpool->entry)
		return -ENOMEM;

	dpool->start_index = start_index;
	dpool->size = size;
	dpool->max_alloc_size = max_alloc_size;
	dpool->user_data = user_data;
	dpool->move_callback = move_callback;
	/* Init entries */
	for (i = 0; i < size; i++) {
		dpool->entry[i].flags = 0;
		dpool->entry[i].index = start_index;
		dpool->entry[i].entry_data = 0UL;
		start_index++;
	}

	return 0;
}

/* dpool_dump_free_list
 *
 * Debug function to dump the free list
 */
static void dpool_dump_free_list(struct dpool_free_list *free_list)
{
	u32 i;

	netdev_dbg(NULL, "FreeList:");

	for (i = 0; i < free_list->size; i++) {
		netdev_dbg(NULL, "[%02d-%d:%d]", i, free_list->entry[i].index,
			   free_list->entry[i].size);
	}

	netdev_dbg(NULL, "\n");
}

/* dpool_dump_adj_list
 *
 * Debug function to dump the adjacencies list
 */
static void dpool_dump_adj_list(struct dpool_adj_list *adj_list)
{
	u32 i;

	netdev_dbg(NULL, "AdjList: ");

	for (i = 0; i < adj_list->size; i++) {
		netdev_dbg(NULL, "[%02d-%d:%d:%d:%d]", i,
			   adj_list->entry[i].index, adj_list->entry[i].size,
			   adj_list->entry[i].left, adj_list->entry[i].right);
	}

	netdev_dbg(NULL, "\n");
}

/* dpool_move
 *
 * Function to invoke the EM HWRM callback. Will only be used
 * if defrag is selected and is required to insert an entry. This
 * function will only be called if the dst_index has sufficient
 * adjacent space for the src_index to be moved in to.
 *
 * dst_index - Table entry index to move to.
 * src_index - Table entry index to move.
 */
static int dpool_move(struct dpool *dpool, u32 dst_index, u32 src_index)
{
	struct dpool_entry *entry = dpool->entry;
	u32 size;
	u32 i;

	netdev_dbg(NULL, "Moving %d to %d\n", src_index, dst_index);
	if (!DP_IS_FREE(entry[dst_index].flags))
		return -1;

	size = DP_FLAGS_SIZE(entry[src_index].flags);

	/* Mark destination as busy. */
	entry[dst_index].flags = entry[src_index].flags;
	entry[dst_index].entry_data = entry[src_index].entry_data;

	/* Invoke EM move HWRM */
	if (dpool->move_callback) {
		dpool->move_callback(dpool->user_data,
				     entry[src_index].entry_data,
				     dst_index + dpool->start_index);
	}

	/* Mark source as free. */
	entry[src_index].flags = 0;
	entry[src_index].entry_data = 0UL;

	/* For multi bock entries mark all dest blocks as busy
	 * and src blocks are free.
	 */
	for (i = 1; i < size; i++) {
		entry[dst_index + i].flags = size;
		entry[src_index + i].flags = 0;
	}

	return 0;
}

/* dpool_defrag_create_free_list
 *
 * Create a list of free entries.
 *
 * *lf_index - Returns the start index of the largest block
 *             of contiguious free entries.
 * *lf_size  - Returns the size of the largest block of
 *             contiguious free entries.
 */
static void dpool_defrag_create_free_list(struct dpool *dpool,
					  struct dpool_free_list *free_list,
					  u32 *lf_index, u32 *lf_size)
{
	u32 count = 0;
	u32 index = 0;
	u32 i;

	for (i = 0; i < dpool->size; i++) {
		if (DP_IS_FREE(dpool->entry[i].flags)) {
			if (count == 0)
				index = i;
			count++;
		} else if (count > 0) {
			free_list->entry[free_list->size].index = index;
			free_list->entry[free_list->size].size = count;

			if (count > *lf_size) {
				*lf_index = free_list->size;
				*lf_size = count;
			}

			free_list->size++;
			count = 0;
		}
	}

	if (free_list->size == 0)
		*lf_size = count;

	netdev_dbg(NULL, "Largest Free Index:%d Size:%d\n", *lf_index,
		   *lf_size);
	dpool_dump_free_list(free_list);
}

/* dpool_defrag_create_adj_list
 *
 * Create a list of busy entries including the number of free
 * entries before and after the busy block.
 */
static void dpool_defrag_create_adj_list(struct dpool *dpool,
					 struct dpool_adj_list *adj_list)
{
	u32 count = 0;
	u32 used = 0;
	u32 i;

	for (i = 0; i < dpool->size; ) {
		if (DP_IS_USED(dpool->entry[i].flags)) {
			used++;

			if (count > 0) {
				adj_list->entry[adj_list->size].index = i;
				adj_list->entry[adj_list->size].size =
					DP_FLAGS_SIZE(dpool->entry[i].flags);
				adj_list->entry[adj_list->size].left = count;

				if (adj_list->size > 0 && used == 1)
					adj_list->entry[adj_list->size - 1].right = count;

				adj_list->size++;
			}

			count = 0;
			i += DP_FLAGS_SIZE(dpool->entry[i].flags);
		} else {
			used = 0;
			count++;
			i++;
		}
	}

	dpool_dump_adj_list(adj_list);
}

/* dpool_defrag_find_adj_entry
 *
 * Using the adjacency and free lists find block with largest
 * adjacent free space to the left and right. Such a block would
 * be the prime target for moving so that the left and right adjacent
 * free space can be combined.
 */
static void dpool_defrag_find_adj_entry(struct dpool_adj_list *adj_list,
					struct dpool_free_list *free_list,
					u32 *lf_index, u32 *lf_size,
					u32 *max, u32 *max_index)
{
	u32 max_size = 0;
	u32 size;
	u32 i;

	/* Using the size of the largest free space available select the
	 * adjacency list entry of that size with the largest left + right +
	 * size count. If there are no entries of that size then decrement
	 * the size and try again.
	 */
	for (size = *lf_size; size > 0; size--) {
		for (i = 0; i < adj_list->size; i++) {
			if (adj_list->entry[i].size == size &&
			    ((size + adj_list->entry[i].left +
			      adj_list->entry[i].right) > *max)) {
				*max = size + adj_list->entry[i].left +
					adj_list->entry[i].right;
				max_size = size;
				*max_index = adj_list->entry[i].index;
			}
		}

		if (*max)
			break;
	}

	/* If the max entry is smaller than the largest_free_size
	 * find the first entry in the free list that it cn fit in to.
	 */
	if (max_size < *lf_size) {
		for (i = 0; i < free_list->size; i++) {
			if (free_list->entry[i].size >= max_size) {
				*lf_index = i;
				break;
			}
		}
	}
}

/* dpool_defrag
 *
 * Defragment the entries. This can either defragment until there's
 * just sufficient space to fit the new entry or defragment until
 * there's no more defragmentation possible. Will only be used if
 * the EM Move callback is supported and the application selects
 * a defrag option on insert.
 */
int dpool_defrag(struct dpool *dpool, u32 entry_size, u8 defrag)
{
	struct dpool_free_list *free_list;
	struct dpool_adj_list *adj_list;
	u32 largest_free_index = 0;
	u32 largest_free_size;
	u32 max_index;
	u32 max;
	int rc;

	free_list = vzalloc(sizeof(*free_list));
	if (!free_list)
		return largest_free_index;

	adj_list = vzalloc(sizeof(*adj_list));
	if (!adj_list) {
		vfree(free_list);
		return largest_free_index;
	}

	while (1) {
		/* Create list of free entries */
		free_list->size = 0;
		largest_free_size = 0;
		largest_free_index = 0;
		dpool_defrag_create_free_list(dpool, free_list,
					      &largest_free_index,
					      &largest_free_size);

		/* If using defrag to fit and there's a large enough
		 * space then we are done.
		 */
		if (defrag == DP_DEFRAG_TO_FIT &&
		    largest_free_size >= entry_size)
			goto end;

		/* Create list of entries adjacent to free entries */
		adj_list->size = 0;
		dpool_defrag_create_adj_list(dpool, adj_list);

		max = 0;
		max_index = 0;
		dpool_defrag_find_adj_entry(adj_list, free_list,
					    &largest_free_index,
					    &largest_free_size,
					    &max, &max_index);
		if (!max)
			break;

		/* If we have a contender then move it to the new spot. */
		rc = dpool_move(dpool,
				free_list->entry[largest_free_index].index,
				max_index);
		if (rc) {
			largest_free_size = rc;
			goto end;
		}
	}

end:
	vfree(free_list);
	vfree(adj_list);
	return largest_free_size;
}

/* dpool_find_free_entries
 *
 * Find size consecutive free entries and if successful then
 * mark those entries as busy.
 */
static u32 dpool_find_free_entries(struct dpool *dpool, u32 size)
{
	u32 first_entry_index;
	u32 count = 0;
	u32 i;
	u32 j;

	for (i = 0; i < dpool->size; i++) {
		if (!DP_IS_FREE(dpool->entry[i].flags)) {
			/* Busy entry, reset count and keep trying */
			count = 0;
			continue;
		}

		/* Found a free entry */
		if (count == 0)
			first_entry_index = i;

		count++;
		if (count < size)
			continue;

		/* Success, found enough entries, mark as busy. */
		for (j = 0; j < size; j++) {
			dpool->entry[j + first_entry_index].flags = size;
		}
		/* mark first entry as start */
		dpool->entry[first_entry_index].flags |= DP_FLAGS_START;

		dpool->entry[i].entry_data = 0UL;

		/* Success */
		return (first_entry_index + dpool->start_index);
	}

	/* Failure */
	return DP_INVALID_INDEX;
}

/* dpool_alloc
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
u32 dpool_alloc(struct dpool *dpool, u32 size, u8 defrag)
{
	u32 index;
	int rc;

	if (size > dpool->max_alloc_size || size == 0)
		return DP_INVALID_INDEX;

	/* Defrag requires EM move support. */
	if (defrag != DP_DEFRAG_NONE && !dpool->move_callback)
		return DP_INVALID_INDEX;

	while (1) {
		/* This will find and allocate the required number
		 * of entries. If there's not enough space then
		 * it will return DP_INVALID_INDEX and we can go
		 * on and defrag if selected.
		 */
		index = dpool_find_free_entries(dpool, size);
		if (index != DP_INVALID_INDEX)
			return index;

		/* If not defragging we are done */
		if (defrag == DP_DEFRAG_NONE)
			break;

		/* If defragging then do it */
		rc = dpool_defrag(dpool, size, defrag);
		if (rc < 0)
			return DP_INVALID_INDEX;

		/* If the defrag created enough space then try the
		 * alloc again else quit.
		 */
		if ((u32)rc < size)
			break;
	}

	return DP_INVALID_INDEX;
}

/* dpool_free
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
 */
int dpool_free(struct dpool *dpool,
	       u32 index)
{
	int start = (index - dpool->start_index);
	u32 size;
	u32 i;

	if (start < 0)
		return -1;

	if (DP_IS_START(dpool->entry[start].flags)) {
		size = DP_FLAGS_SIZE(dpool->entry[start].flags);
		if (size > dpool->max_alloc_size || size == 0)
			return -1;

		for (i = start; i < (start + size); i++)
			dpool->entry[i].flags = 0;

		return 0;
	}

	return -1;
}

/* dpool_free_all
 *
 * Free all entries.
 *
 * @dpool:	The pool
 *
 * Result
 *      - 0  on success
 *      - -1 on failure
 */
void dpool_free_all(struct dpool *dpool)
{
	u32 i;

	for (i = 0; i < dpool->size; i++)
		dpool_free(dpool, dpool->entry[i].index);
}

/* dpool_set_entry_data
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
int dpool_set_entry_data(struct dpool *dpool, u32 index, u64 entry_data)
{
	int start = (index - dpool->start_index);

	if (start < 0)
		return -1;

	if (DP_IS_START(dpool->entry[start].flags)) {
		dpool->entry[start].entry_data = entry_data;
		return 0;
	}

	return -1;
}

void dpool_dump(struct dpool *dpool)
{
	u32 i;

	netdev_dbg(NULL, "Dpool size;%d start:0x%x\n", dpool->size,
		   dpool->start_index);

	for (i = 0; i < dpool->size; i++) {
		netdev_dbg(NULL, "[0x%08x-0x%08x]\n", dpool->entry[i].flags,
			   dpool->entry[i].index);
	}

	netdev_dbg(NULL, "\n");
}
