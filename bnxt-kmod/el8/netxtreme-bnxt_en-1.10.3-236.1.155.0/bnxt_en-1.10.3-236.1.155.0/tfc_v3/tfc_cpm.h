/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_CPM_H_
#define _TFC_CPM_H_

/* Set to 1 to force using just TS 0
 */
#define TFC_FORCE_POOL_0 1

/* Temp to enable build. Remove when tfc_cmm is added
 */
struct tfc_cmm {
	int a;
};

struct tfc_cpm;

#define TFC_CPM_INVALID_POOL_ID 0xFFFF

/**
 * int tfc_cpm_open
 *
 * Initializes pre-allocated CPM structure. The cpm_db_size argument is
 * validated against the max_pools argument.
 *
 * @cpm: Pointer to pointer of the allocated CPM data structure. The open will
 *	 perform the alloc and return a pointer to the allocated memory.
 * @max_pools: Maximum number of pools
 *
 * Returns:
 * 0 - Success
 * -EINVAL - cpm_db_size is not correct
 * -ENOMEM - Failed to allocate memory for CPM data structures.
 */
int tfc_cpm_open(struct tfc_cpm **cpm, u32 max_pools);

/**
 * int tfc_cpm_close
 *
 * Deinitialize data structures. Note this does not free the memory.
 *
 * @cpm: Pointer to the CPM instance to free.
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_close(struct tfc_cpm *cpm);

/**
 * int tfc_cpm_set_pool_size
 *
 * Sets number of entries for pools in a given region.
 *
 * @cpm: Pointer to the CPM instance
 * @pool_sz_in_records: Max number of entries for each pool must be a power of 2.
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_set_pool_size(struct tfc_cpm *cpm, u32 pool_sz_in_records);

/**
 * int tfc_cpm_get_pool_size
 *
 * Returns the number of entries for pools in a given region.
 *
 * @cpm: Pointer to the CPM instance
 * @pool_sz_in_records: Max number of entries for each pool
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_get_pool_size(struct tfc_cpm *cpm, u32 *pool_sz_in_records);

/**
 * int tfc_cpm_set_cmm_inst
 *
 * Add CMM instance.
 *
 * @cpm: Pointer to the CPM instance
 * @pool_id: Pool ID to use
 * @valid: Is entry valid
 * @cmm: Pointer to the CMM instance
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_set_cmm_inst(struct tfc_cpm *cpm, u16 pool_id, struct tfc_cmm *cmm);

/**
 * int tfc_cpm_get_cmm_inst
 *
 * Get CMM instance.
 *
 * @cpm: Pointer to the CPM instance
 * @pool_id: Pool ID to use
 * @valid: Is entry valid
 * @cmm: Pointer to the CMM instance
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_get_cmm_inst(struct tfc_cpm *cpm, u16 pool_id, struct tfc_cmm **cmm);

/**
 * int tfc_cpm_get_avail_pool
 *
 * Returns the pool_id to use for the next EM  insert
 *
 * @cpm: Pointer to the CPM instance
 * @pool_id: Pool ID to use for EM insert
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_get_avail_pool(struct tfc_cpm *cpm, u16 *pool_id);

/**
 * int tfc_cpm_set_usage
 *
 * Set the usage_count and all_used fields for the specified pool_id
 *
 * @cpm: Pointer to the CPM instance
 * @pool_id: Pool ID to update
 * @used_count: Number of entries used within specified pool
 * @all_used: Set if all pool entries are used
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_set_usage(struct tfc_cpm *cpm, u16 pool_id, u32 used_count, bool all_used);

/**
 * int tfc_cpm_srchm_by_configured_pool
 *
 * Get the next configured pool
 *
 * @cpm: Pointer to the CPM instance
 *
 * @srch_mode: Valid pool id
 *
 * @pool_id: Pointer to a valid pool id
 *
 * @cmm: Pointer to the associated CMM instance
 *
 * Returns:
 * 0 - Success
 * -EINVAL - Invalid argument
 */
int tfc_cpm_srchm_by_configured_pool(struct tfc_cpm *cpm, enum cfa_srch_mode srch_mode,
				     u16 *pool_id, struct tfc_cmm **cmm);

#endif /* _TFC_CPM_H_ */
