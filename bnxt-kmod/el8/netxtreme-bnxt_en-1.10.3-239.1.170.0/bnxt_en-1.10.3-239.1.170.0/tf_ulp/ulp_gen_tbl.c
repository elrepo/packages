// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */
#include "ulp_linux.h"
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "tf_core.h"
#include "ulp_mapper.h"
#include "ulp_flow_db.h"
#include "ulp_template_debug_proto.h"
#include "ulp_tf_debug.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
/* Retrieve the generic table  initialization parameters for the tbl_idx */
static const struct bnxt_ulp_generic_tbl_params*
ulp_mapper_gen_tbl_params_get(struct bnxt_ulp_context *ulp_ctx,
			      u32 tbl_idx)
{
	const struct bnxt_ulp_generic_tbl_params *gen_tbl;
	struct bnxt_ulp_device_params *dparms;
	u32 dev_id;

	if (tbl_idx >= BNXT_ULP_GEN_TBL_MAX_SZ)
		return NULL;

	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctx, &dev_id))
		return NULL;

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		netdev_dbg(ulp_ctx->bp->dev, "Failed to get device parms\n");
		return NULL;
	}

	gen_tbl = &dparms->gen_tbl_params[tbl_idx];
	return gen_tbl;
}

/**
 * Initialize the generic table list
 *
 * @mapper_data: Pointer to the mapper data and the generic table is
 * part of it
 *
 * returns 0 on success
 */
int
ulp_mapper_generic_tbl_list_init(struct bnxt_ulp_context *ulp_ctx,
				 struct bnxt_ulp_mapper_data *mapper_data)
{
	struct rhashtable_params bnxt_tf_tc_ht_params = { 0 };
	const struct bnxt_ulp_generic_tbl_params *tbl;
	struct ulp_mapper_gen_tbl_list *entry;
	u32 idx, size, key_sz;
	int rc = 0;

	/* Allocate the generic tables. */
	for (idx = 0; idx < BNXT_ULP_GEN_TBL_MAX_SZ; idx++) {
		tbl = ulp_mapper_gen_tbl_params_get(ulp_ctx, idx);
		if (!tbl) {
			netdev_dbg(ulp_ctx->bp->dev, "Failed to get gen table parms %d\n",
				   idx);
			return -EINVAL;
		}
		entry = &mapper_data->gen_tbl_list[idx];

		/* For simple list allocate memory for key storage*/
		if (tbl->gen_tbl_type == BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST &&
		    tbl->key_num_bytes) {
			key_sz = tbl->key_num_bytes +
				tbl->partial_key_num_bytes;
			entry->container.byte_key_ex_size = tbl->key_num_bytes;
			entry->container.byte_key_par_size =
				tbl->partial_key_num_bytes;
		} else {
			key_sz = 0;
		}

		/* Allocate memory for result data and key data */
		if (tbl->result_num_entries != 0) {
			/* assign the name */
			entry->gen_tbl_name = tbl->name;
			entry->tbl_type = tbl->gen_tbl_type;
			/* add 4 bytes for reference count */
			entry->mem_data_size = (tbl->result_num_entries + 1) *
				(tbl->result_num_bytes + sizeof(u32) + key_sz);

			/* allocate the big chunk of memory */
			entry->mem_data = vzalloc(entry->mem_data_size);
			if (!entry->mem_data)
				return -ENOMEM;

			/* Populate the generic table container */
			entry->container.num_elem = tbl->result_num_entries;
			entry->container.byte_data_size = tbl->result_num_bytes;
			entry->container.ref_count =
				(u32 *)entry->mem_data;
			size = sizeof(u32) * (tbl->result_num_entries + 1);
			entry->container.byte_data = &entry->mem_data[size];
			entry->container.byte_order = tbl->result_byte_order;
		} else {
			netdev_dbg(ulp_ctx->bp->dev, "%s: Unused Gen tbl entry is %d\n",
				   tbl->name, idx);
			continue;
		}

		/* assign the memory for key data */
		if (tbl->gen_tbl_type == BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST &&
		    key_sz) {
			size += tbl->result_num_bytes *
				(tbl->result_num_entries + 1);
			entry->container.byte_key =
				&entry->mem_data[size];
		}

		/* Initialize Hash list for hash based generic table */
		if (tbl->gen_tbl_type == BNXT_ULP_GEN_TBL_TYPE_HASH_LIST &&
		    tbl->hash_tbl_entries) {
			bnxt_tf_tc_ht_params.head_offset =
				offsetof(struct ulp_gen_hash_entry_params, node);
			bnxt_tf_tc_ht_params.key_offset =
				offsetof(struct ulp_gen_hash_entry_params, key_data);
			bnxt_tf_tc_ht_params.key_len = tbl->key_num_bytes;
			bnxt_tf_tc_ht_params.automatic_shrinking = true;
			bnxt_tf_tc_ht_params.nelem_hint = /* Set to about 75% */
				(tbl->result_num_entries * 75) / 100;
			bnxt_tf_tc_ht_params.max_size =	tbl->result_num_entries;
			entry->hash_tbl_params = bnxt_tf_tc_ht_params;
			entry->hash_tbl = vzalloc(sizeof(*entry->hash_tbl));
			rc = rhashtable_init(entry->hash_tbl, &entry->hash_tbl_params);
			if (rc) {
				netdev_dbg(ulp_ctx->bp->dev, "HASH_TABLE initialization failed\n");
				return rc;
			}
		}
	}
	/* success */
	return 0;
}

/**
 * Free the generic table list
 *
 * @mapper_data: Pointer to the mapper data and the generic table is
 * part of it
 *
 * returns 0 on success
 */
int
ulp_mapper_generic_tbl_list_deinit(struct bnxt_ulp_mapper_data *mapper_data)
{
	struct ulp_mapper_gen_tbl_list *tbl_list;
	u32 idx;

	/* iterate the generic table. */
	for (idx = 0; idx < BNXT_ULP_GEN_TBL_MAX_SZ; idx++) {
		tbl_list = &mapper_data->gen_tbl_list[idx];
		vfree(tbl_list->mem_data);
		tbl_list->mem_data = NULL;
		tbl_list->container.byte_data = NULL;
		tbl_list->container.byte_key = NULL;
		tbl_list->container.ref_count = NULL;
		if (tbl_list->hash_tbl) {
			rhashtable_destroy(tbl_list->hash_tbl);
			vfree(tbl_list->hash_tbl);
		}
	}
	/* success */
	return 0;
}

/**
 * Get the generic table list entry
 *
 * @tbl_list: - Ptr to generic table
 * @key: - Key index to the table
 * @entry: - output will include the entry if found
 *
 * returns 0 on success.
 */
int
ulp_mapper_gen_tbl_entry_get(struct bnxt_ulp_context *ulp_ctx,
			     struct ulp_mapper_gen_tbl_list *tbl_list,
			     u32 key,
			     struct ulp_mapper_gen_tbl_entry *entry)
{
	/* populate the output and return the values */
	if (key > tbl_list->container.num_elem) {
		netdev_dbg(ulp_ctx->bp->dev, "%s: invalid key %x:%x\n",
			   tbl_list->gen_tbl_name, key,
			   tbl_list->container.num_elem);
		return -EINVAL;
	}
	entry->ref_count = &tbl_list->container.ref_count[key];
	entry->byte_data_size = tbl_list->container.byte_data_size;
	entry->byte_data = &tbl_list->container.byte_data[key *
		entry->byte_data_size];
	entry->byte_order = tbl_list->container.byte_order;
	if (tbl_list->tbl_type == BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST) {
		entry->byte_key_size = tbl_list->container.byte_key_ex_size +
			tbl_list->container.byte_key_par_size;
		entry->byte_key = &tbl_list->container.byte_key[key *
			entry->byte_key_size];
	} else {
		entry->byte_key = NULL;
		entry->byte_key_size = 0;
	}
	return 0;
}

/**
 * utility function to calculate the table idx
 *
 * @res_sub_type: - Resource sub type
 * @dir: - Direction
 *
 * returns None
 */
int
ulp_mapper_gen_tbl_idx_calculate(struct bnxt_ulp_context *ulp_ctx,
				 u32 res_sub_type, u32 dir)
{
	int tbl_idx;

	/* Validate for direction */
	if (dir >= TF_DIR_MAX) {
		netdev_dbg(ulp_ctx->bp->dev, "invalid argument %x\n", dir);
		return -EINVAL;
	}
	tbl_idx = (res_sub_type << 1) | (dir & 0x1);
	if (tbl_idx >= BNXT_ULP_GEN_TBL_MAX_SZ) {
		netdev_dbg(ulp_ctx->bp->dev, "invalid table index %x\n", tbl_idx);
		return -EINVAL;
	}
	return tbl_idx;
}

/**
 * Set the data in the generic table entry, Data is in Big endian format
 *
 * @entry: - generic table entry
 * @key: - pointer to the key to be used for setting the value.
 * @key_size: - The length of the key in bytess to be set
 * @data: - pointer to the data to be used for setting the value.
 * @data_size: - length of the data pointer in bytes.
 *
 * returns 0 on success
 */
int
ulp_mapper_gen_tbl_entry_data_set(struct bnxt_ulp_context *ulp_ctx,
				  struct ulp_mapper_gen_tbl_list *tbl_list,
				  struct ulp_mapper_gen_tbl_entry *entry,
				  u8 *key, u32 key_size,
				  u8 *data, u32 data_size)
{
	/* validate the null arguments */
	if (!entry || !key || !data) {
		netdev_dbg(ulp_ctx->bp->dev, "invalid argument\n");
		return -EINVAL;
	}

	/* check the size of the buffer for validation */
	if (data_size > entry->byte_data_size) {
		netdev_dbg(ulp_ctx->bp->dev, "invalid offset or length %x:%x\n",
			     data_size, entry->byte_data_size);
		return -EINVAL;
	}
	memcpy(entry->byte_data, data, data_size);
	if (tbl_list->tbl_type == BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST) {
		if (key_size > entry->byte_key_size) {
			netdev_dbg(ulp_ctx->bp->dev, "invalid offset or length %x:%x\n",
				   key_size, entry->byte_key_size);
			return -EINVAL;
		}
		memcpy(entry->byte_key, key, key_size);
	}
	tbl_list->container.seq_cnt++;
	return 0;
}

/**
 * Get the data in the generic table entry, Data is in Big endian format
 *
 * @entry: - generic table entry
 * @offset: - The offset in bits where the data has to get
 * @len: - The length of the data in bits to be get
 * @data: - pointer to the data to be used for setting the value.
 * @data_size: - The size of data in bytes
 *
 * returns 0 on success
 */
int
ulp_mapper_gen_tbl_entry_data_get(struct bnxt_ulp_context *ulp_ctx,
				  struct ulp_mapper_gen_tbl_entry *entry,
				  u32 offset, u32 len, u8 *data,
				  u32 data_size)
{
	/* validate the null arguments */
	if (!entry || !data) {
		netdev_dbg(ulp_ctx->bp->dev, "invalid argument\n");
		return -EINVAL;
	}

	/* check the size of the buffer for validation */
	if ((offset + len) > ULP_BYTE_2_BITS(entry->byte_data_size) ||
	    len > ULP_BYTE_2_BITS(data_size)) {
		netdev_dbg(ulp_ctx->bp->dev, "invalid offset or length %x:%x:%x\n",
			   offset, len, entry->byte_data_size);
		return -EINVAL;
	}
	if (entry->byte_order == BNXT_ULP_BYTE_ORDER_LE)
		ulp_bs_pull_lsb(entry->byte_data, data, data_size, offset, len);
	else
		ulp_bs_pull_msb(entry->byte_data, data, offset, len);

	return 0;
}

/**
 * Free the generic table list resource
 *
 * @ulp_ctx: - Pointer to the ulp context
 * @res: - Pointer to flow db resource entry
 *
 * returns 0 on success
 */
int
ulp_mapper_gen_tbl_res_free(struct bnxt_ulp_context *ulp_ctx,
			    u32 fid,
			    struct ulp_flow_db_res_params *res)
{
	struct ulp_mapper_gen_tbl_entry entry, *actual_entry;
	struct ulp_gen_hash_entry_params *hash_entry = NULL;
	struct ulp_mapper_gen_tbl_list *gen_tbl_list;
	struct bnxt_ulp_mapper_data *mapper_data;
	int tbl_idx;
	u32 rid = 0;
	u32 key_idx;
	int rc;

	/* Extract the resource sub type and direction */
	tbl_idx = ulp_mapper_gen_tbl_idx_calculate(ulp_ctx, res->resource_sub_type,
						   res->direction);
	if (tbl_idx < 0) {
		netdev_dbg(ulp_ctx->bp->dev, "invalid argument %x:%x\n",
			   res->resource_sub_type, res->direction);
		return -EINVAL;
	}

	mapper_data = bnxt_ulp_cntxt_ptr2_mapper_data_get(ulp_ctx);
	if (!mapper_data) {
		netdev_dbg(ulp_ctx->bp->dev, "invalid ulp context %x\n", tbl_idx);
		return -EINVAL;
	}
	/* get the generic table  */
	gen_tbl_list = &mapper_data->gen_tbl_list[tbl_idx];

	/* Get the generic table entry */
	if (gen_tbl_list->hash_tbl) {
		/* use the hash index to get the value */
		hash_entry = rhashtable_lookup_fast(gen_tbl_list->hash_tbl,
						    res->key_data,
						    gen_tbl_list->hash_tbl_params);
		if (!hash_entry) {
			netdev_dbg(ulp_ctx->bp->dev, "invalid hash entry %p\n", hash_entry);
			return -EINVAL;
		}

		if (!hash_entry->entry.hash_ref_count) {
			netdev_dbg(ulp_ctx->bp->dev, "generic table corrupt %x: %llu\n",
				   tbl_idx, res->resource_hndl);
			return -EINVAL;
		}
		hash_entry->entry.hash_ref_count--;
		if (hash_entry->entry.hash_ref_count)
			return 0;

		actual_entry = &hash_entry->entry;
	} else {
		key_idx =  (u32)res->resource_hndl;
		if (ulp_mapper_gen_tbl_entry_get(ulp_ctx, gen_tbl_list,
						 key_idx, &entry)) {
			netdev_dbg(ulp_ctx->bp->dev,
				   "Gen tbl entry get failed %x: %llu\n",
				   tbl_idx, res->resource_hndl);
			return -EINVAL;
		}
		/* Decrement the reference count */
		if (!ULP_GEN_TBL_REF_CNT(&entry)) {
			netdev_dbg(ulp_ctx->bp->dev,
				   "generic table entry already free %x: %llu\n",
				   tbl_idx, res->resource_hndl);
			return 0;
		}
		ULP_GEN_TBL_REF_CNT_DEC(&entry);

		/* retain the details since there are other users */
		if (ULP_GEN_TBL_REF_CNT(&entry))
			return 0;

		actual_entry = &entry;
	}

	/* Delete the generic table entry. First extract the fid */
	if (ulp_mapper_gen_tbl_entry_data_get(ulp_ctx, actual_entry, ULP_GEN_TBL_FID_OFFSET,
					      ULP_GEN_TBL_FID_SIZE_BITS,
					      (u8 *)&rid,
					      sizeof(rid))) {
		netdev_dbg(ulp_ctx->bp->dev, "Unable to get rid %x: %llu\n",
			   tbl_idx, res->resource_hndl);
		return -EINVAL;
	}

	rid = be32_to_cpu(rid);
	/* no need to del if rid is 0 since there is no associated resource
	 * if rid from the entry is equal to the incoming fid, then we have a
	 * recursive delete, so don't follow the rid.
	 */
	if (rid && rid != fid) {
		/* Destroy the flow associated with the shared flow id */
		if (ulp_mapper_flow_destroy(ulp_ctx, BNXT_ULP_FDB_TYPE_RID,
					    rid, NULL))
			netdev_dbg(ulp_ctx->bp->dev,
				   "Error in deleting shared flow id %x\n",
				   fid);
	}

	/* Delete the entry from the hash table */
	if (gen_tbl_list->hash_tbl) {
		rc = rhashtable_remove_fast(gen_tbl_list->hash_tbl, &hash_entry->node,
					    gen_tbl_list->hash_tbl_params);
		if (rc) {
			netdev_dbg(ulp_ctx->bp->dev,
				   "Unable to delete hash entry %p\n", hash_entry);
			return rc;
		}

		vfree(hash_entry->entry.byte_data);
		hash_entry->entry.byte_data = NULL;
		kfree_rcu(hash_entry, rcu);
		return 0;
	}

	/* decrement the count */
	if (gen_tbl_list->tbl_type == BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST &&
	    gen_tbl_list->container.seq_cnt > 0)
		gen_tbl_list->container.seq_cnt--;

	/* clear the byte data of the generic table entry */
	memset(actual_entry->byte_data, 0, actual_entry->byte_data_size);

	return 0;
}

/**
 * Perform add entry in the simple list
 *
 * @tbl_list: - pointer to the generic table list
 * @key: -  Key added as index
 * @data: -  data added as result
 * @key_index: - index to the entry
 * @gen_tbl_ent: - write the output to the entry
 *
 * returns 0 on success.
 */
int32_t
ulp_gen_tbl_simple_list_add_entry(struct ulp_mapper_gen_tbl_list *tbl_list,
				  u8 *key,
				  u8 *data,
				  u32 *key_index,
				  struct ulp_mapper_gen_tbl_entry *ent)
{
	struct ulp_mapper_gen_tbl_cont	*cont;
	u32 key_size, idx;
	u8 *entry_key;

	/* sequentially search for the matching key */
	cont = &tbl_list->container;
	for (idx = 0; idx < cont->num_elem; idx++) {
		ent->ref_count = &cont->ref_count[idx];
		if (ULP_GEN_TBL_REF_CNT(ent) == 0) {
			/* add the entry */
			key_size = cont->byte_key_ex_size +
				cont->byte_key_par_size;
			entry_key = &cont->byte_key[idx * key_size];
			ent->byte_data_size = cont->byte_data_size;
			ent->byte_data = &cont->byte_data[idx *
				cont->byte_data_size];
			memcpy(entry_key, key, key_size);
			memcpy(ent->byte_data, data, ent->byte_data_size);
			ent->byte_order = cont->byte_order;
			*key_index = idx;
			cont->seq_cnt++;
			return 0;
		}
	}
	/* No more memory */
	return -ENOMEM;
}

/* perform the subset and superset. len should be 64bit multiple*/
static enum ulp_gen_list_search_flag
ulp_gen_tbl_overlap_check(u8 *key1, u8 *key2, u32 len)
{
	u32 sz = 0, superset = 0, subset = 0;
	u64 src, dst;

	while (sz  < len) {
		memcpy(&dst, key2, sizeof(dst));
		memcpy(&src, key1, sizeof(src));
		sz += sizeof(src);
		if (dst == src)
			continue;
		else if (dst == (dst | src))
			superset = 1;
		else if (src == (dst | src))
			subset = 1;
		else
			return ULP_GEN_LIST_SEARCH_MISSED;
	}
	if (superset && !subset)
		return ULP_GEN_LIST_SEARCH_FOUND_SUPERSET;
	if (!superset && subset)
		return ULP_GEN_LIST_SEARCH_FOUND_SUBSET;
	return ULP_GEN_LIST_SEARCH_FOUND;
}

int32_t
ulp_gen_tbl_simple_list_search(struct ulp_mapper_gen_tbl_list *tbl_list,
			       u8 *match_key,
			       u32 *key_idx)
{
	struct ulp_mapper_gen_tbl_cont	*cont = &tbl_list->container;
	enum ulp_gen_list_search_flag rc = ULP_GEN_LIST_SEARCH_FULL;
	u32 idx = 0, key_idx_set = 0, sz = 0, key_size = 0;
	u8 *k1 = NULL, *k2, *entry_key;
	u32 valid_ent = 0;
	u32 *ref_count;

	key_size = cont->byte_key_ex_size + cont->byte_key_par_size;
	if (cont->byte_key_par_size)
		k1 = match_key + cont->byte_key_ex_size;

	/* sequentially search for the matching key */
	while (idx < cont->num_elem) {
		ref_count = &cont->ref_count[idx];
		entry_key = &cont->byte_key[idx * key_size];
		/* check ref count not zero and exact key matches */
		if (*ref_count) {
			/* compare the exact match */
			if (!memcmp(match_key, entry_key,
				    cont->byte_key_ex_size)) {
				/* Match the partial key*/
				if (cont->byte_key_par_size) {
					k2 = entry_key + cont->byte_key_ex_size;
					sz = cont->byte_key_par_size;
					rc = ulp_gen_tbl_overlap_check(k1, k2,
								       sz);
					if (rc != ULP_GEN_LIST_SEARCH_MISSED) {
						*key_idx = idx;
						return rc;
					}
				} else {
					/* found the entry return */
					rc = ULP_GEN_LIST_SEARCH_FOUND;
					*key_idx = idx;
					return rc;
				}
			}
			++valid_ent;
		} else {
			/* empty slot */
			if (!key_idx_set) {
				*key_idx = idx;
				key_idx_set = 1;
				rc = ULP_GEN_LIST_SEARCH_MISSED;
			}
			if (valid_ent >= cont->seq_cnt)
				return rc;
		}
		idx++;
	}
	return rc;
}
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
