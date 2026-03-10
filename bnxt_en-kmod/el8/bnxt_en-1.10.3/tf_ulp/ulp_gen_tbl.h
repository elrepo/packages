/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_GEN_TBL_H_
#define _ULP_GEN_TBL_H_

#include <linux/rhashtable.h>

/* Macros for reference count manipulation */
#define ULP_GEN_TBL_REF_CNT_INC(entry) {*(entry)->ref_count += 1; }
#define ULP_GEN_TBL_REF_CNT_DEC(entry) {*(entry)->ref_count -= 1; }
#define ULP_GEN_TBL_REF_CNT(entry) (*(entry)->ref_count)

#define ULP_GEN_TBL_FID_OFFSET		0
#define ULP_GEN_TBL_FID_SIZE_BITS	32

enum ulp_gen_list_search_flag {
	ULP_GEN_LIST_SEARCH_MISSED = 1,
	ULP_GEN_LIST_SEARCH_FOUND = 2,
	ULP_GEN_LIST_SEARCH_FOUND_SUBSET = 3,
	ULP_GEN_LIST_SEARCH_FOUND_SUPERSET = 4,
	ULP_GEN_LIST_SEARCH_FULL = 5
};

enum ulp_gen_hash_search_flag {
	ULP_GEN_HASH_SEARCH_MISSED = 1,
	ULP_GEN_HASH_SEARCH_FOUND = 2,
	ULP_GEN_HASH_SEARCH_FULL = 3
};

/* Structure to pass the generic table values across APIs */
struct ulp_mapper_gen_tbl_entry {
	u32				*ref_count;
	u32				byte_data_size;
	u8				*byte_data;
	enum bnxt_ulp_byte_order	byte_order;
	u32				hash_ref_count;
	u32				byte_key_size;
	u8				*byte_key;
};

/* structure to pass hash entry */
struct ulp_gen_hash_entry_params {
#define ULP_MAX_HASH_KEY_LENGTH	57
	struct rhash_head               node;
	struct ulp_mapper_gen_tbl_entry	entry;
	struct rcu_head			rcu;
	u32				key_length;
	enum ulp_gen_hash_search_flag	search_flag;
	u32				hash_index;
	u32				key_idx;
	u8				key_data[]; /* must be the last one */
};

/* Structure to store the generic tbl container
 * The ref count and byte data contain list of "num_elem" elements.
 * The size of each entry in byte_data is of size byte_data_size.
 */
struct ulp_mapper_gen_tbl_cont {
	u32				num_elem;
	u32				byte_data_size;
	enum bnxt_ulp_byte_order	byte_order;
	/* Reference count to track number of users*/
	u32				*ref_count;
	/* First 4 bytes is either tcam_idx or fid and rest are identities */
	u8				*byte_data;
	u8				*byte_key;
	u32				byte_key_ex_size; /* exact match size */
	u32				byte_key_par_size; /* partial match */
	u32				seq_cnt;
};

/* Structure to store the generic tbl container */
struct ulp_mapper_gen_tbl_list {
	const char			*gen_tbl_name;
	enum bnxt_ulp_gen_tbl_type	tbl_type;
	struct ulp_mapper_gen_tbl_cont	container;
	u32				mem_data_size;
	u8				*mem_data;
	struct rhashtable               *hash_tbl;
	struct rhashtable_params        hash_tbl_params;
};

/* Forward declaration */
struct bnxt_ulp_mapper_data;
struct ulp_flow_db_res_params;

/**
 * Initialize the generic table list
 *
 * @ulp_ctx: Pointer to the ulp context
 * @mapper_data: Pointer to the mapper data and the generic table is part of it
 *
 * returns 0 on success
 */
int
ulp_mapper_generic_tbl_list_init(struct bnxt_ulp_context *ulp_ctx,
				 struct bnxt_ulp_mapper_data *mapper_data);

/**
 * Free the generic table list
 *
 * @mapper_data: Pointer to the mapper data and the generic table is part of it
 *
 * returns 0 on success
 */
int
ulp_mapper_generic_tbl_list_deinit(struct bnxt_ulp_mapper_data *mapper_data);

/**
 * Get the generic table list entry
 *
 * @tbl_list: Ptr to generic table
 * @key: Key index to the table
 * @entry: output will include the entry if found
 *
 * returns 0 on success.
 */
int
ulp_mapper_gen_tbl_entry_get(struct bnxt_ulp_context *ulp_ctx,
			     struct ulp_mapper_gen_tbl_list *tbl_list,
			     u32 key,
			     struct ulp_mapper_gen_tbl_entry *entry);

/**
 * utility function to calculate the table idx
 *
 * @res_sub_type: Resource sub type
 * @dir: direction
 *
 * returns None
 */
int
ulp_mapper_gen_tbl_idx_calculate(struct bnxt_ulp_context *ulp_ctx,
				 u32 res_sub_type, u32 dir);

/**
 * Set the data in the generic table entry, Data is in Big endian format
 *
 * @entry: generic table entry
 * @key: pointer to the key to be used for setting the value.
 * @key_size: The length of the key in bytess to be set
 * @data: pointer to the data to be used for setting the value.
 * @data_size: length of the data pointer in bytes.
 *
 * returns 0 on success
 */
int
ulp_mapper_gen_tbl_entry_data_set(struct bnxt_ulp_context *ulp_ctx,
				  struct ulp_mapper_gen_tbl_list *tbl_list,
				  struct ulp_mapper_gen_tbl_entry *entry,
				  u8 *key, u32 key_size,
				  u8 *data, u32 data_size);

/**
 * Get the data in the generic table entry
 *
 * @entry: generic table entry
 * @offset: The offset in bits where the data has to get
 * @len: The length of the data in bits to be get
 * @data: pointer to the data to be used for setting the value.
 * @data_size: The size of data in bytes
 *
 * returns 0 on success
 */
int
ulp_mapper_gen_tbl_entry_data_get(struct bnxt_ulp_context *ulp_ctx,
				  struct ulp_mapper_gen_tbl_entry *entry,
				  u32 offset, u32 len, u8 *data,
				  u32 data_size);

/**
 * Free the generic table list resource
 *
 * @ulp_ctx: Pointer to the ulp context
 * @fid: The fid the generic table is associated with
 * @res: Pointer to flow db resource entry
 *
 * returns 0 on success
 */
int
ulp_mapper_gen_tbl_res_free(struct bnxt_ulp_context *ulp_ctx,
			    u32 fid,
			    struct ulp_flow_db_res_params *res);

/**
 * Perform add entry in the simple list
 *
 * @tbl_list: pointer to the generic table list
 * @key:  Key added as index
 * @data:  data added as result
 * @key_index: index to the entry
 * @gen_tbl_ent: write the output to the entry
 *
 * returns 0 on success.
 */
int
ulp_gen_tbl_simple_list_add_entry(struct ulp_mapper_gen_tbl_list *tbl_list,
				  u8 *key,
				  u8 *data,
				  u32 *key_index,
				  struct ulp_mapper_gen_tbl_entry *ent);

/**
 * Perform add entry in the simple list
 *
 * @tbl_list: pointer to the generic table list
 * @key:  Key added as index
 * @data:  data added as result
 * @key_index: index to the entry
 * @gen_tbl_ent: write the output to the entry
 *
 * returns 0 on success.
 */
int
ulp_gen_tbl_simple_list_search(struct ulp_mapper_gen_tbl_list *tbl_list,
			       u8 *match_key,
			       u32 *key_idx);

#endif /* _ULP_EN_TBL_H_ */
