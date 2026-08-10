/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef TF_TBL_TYPE_H_
#define TF_TBL_TYPE_H_

#include "tf_core.h"

struct tf;

/* The Table module provides processing of Internal TF table types. */

/**
 * Table configuration parameters
 *
 * @num_elements: Number of table types in each of the configuration arrays
 * @cfg:	  Table Type element configuration array
 * @resources:    Session resource allocations
 */
struct tf_tbl_cfg_parms {
	u16				num_elements;
	struct tf_rm_element_cfg	*cfg;
	struct tf_session_resources	*resources;
};

/**
 * Table allocation parameters
 *
 * @dir:		Receive or transmit direction
 * @type:		Type of the allocation
 * @tbl_scope_id:    Table scope identifier (ignored unless TF_TBL_TYPE_EXT)
 * @idx:	     Idx of allocated entry or found entry (if search_enable)
 */
struct tf_tbl_alloc_parms {
	enum tf_dir		dir;
	enum tf_tbl_type	type;
	u32			tbl_scope_id;
	u32			*idx;
};

/**
 * Table free parameters
 *
 * @dir:		Receive or transmit direction
 * @type:		Type of the allocation
 * @tbl_scope_id:    Table scope identifier (ignored unless TF_TBL_TYPE_EXT)
 * @idx:		Index to free
 */
struct tf_tbl_free_parms {
	enum tf_dir		dir;
	enum tf_tbl_type	type;
	u32			tbl_scope_id;
	u32			idx;
};

/**
 * Table set parameters
 *
 * @dir:		Receive or transmit direction
 * @type:		Type of object to set
 * @tbl_scope_id:    Table scope identifier (ignored unless TF_TBL_TYPE_EXT)
 * @data:		Entry data
 * @data_sz_in_bytes:	Entry size
 * @idx:		Entry index to write to
 */
struct tf_tbl_set_parms {
	enum tf_dir		dir;
	enum tf_tbl_type	type;
	u32			tbl_scope_id;
	u8			*data;
	u16			data_sz_in_bytes;
	u32			idx;
};

/**
 * Table get parameters
 *
 * @dir:		Receive or transmit direction
 * @type:		Type of object to get
 * @data:		Entry data
 * @data_sz_in_bytes:	Entry size
 * @idx:		Entry index to read
 */
struct tf_tbl_get_parms {
	enum tf_dir		dir;
	enum tf_tbl_type	type;
	u8			*data;
	u16			data_sz_in_bytes;
	u32			idx;
};

/**
 * Table get bulk parameters
 *
 * @dir:		Receive or transmit direction
 * @type:		Type of object to get
 * @starting_idx:	Starting index to read from
 * @num_entries:	Number of sequential entries
 * @entry_sz_in_bytes:	Size of the single entry
 * @physical_mem_addr:	Host physical address, where the data will be copied
 *			to by the firmware.
 */
struct tf_tbl_get_bulk_parms {
	enum tf_dir		dir;
	enum tf_tbl_type	type;
	u32			starting_idx;
	u16			num_entries;
	u16			entry_sz_in_bytes;
	u64			physical_mem_addr;
};

/* Table RM database */
struct tbl_rm_db {
	struct rm_db *tbl_db[TF_DIR_MAX];
};

/**
 * Initializes the Table module with the requested DBs. Must be
 * invoked as the first thing before any of the access functions.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to Table configuration parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_bind(struct tf *tfp, struct tf_tbl_cfg_parms *parms);

/**
 * Cleans up the private DBs and releases all the data.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_unbind(struct tf *tfp);

/**
 * Allocates the requested table type from the internal RM DB.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to Table allocation parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_alloc(struct tf *tfp, struct tf_tbl_alloc_parms *parms);

/**
 * Frees the requested table type and returns it to the DB.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to Table free parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_free(struct tf *tfp, struct tf_tbl_free_parms *parms);

/**
 * Configures the requested element by sending a firmware request which
 * then installs it into the device internal structures.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to Table set parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_set(struct tf *tfp, struct tf_tbl_set_parms *parms);

/**
 * Retrieves the requested element by sending a firmware request to get
 * the element.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to Table get parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_get(struct tf *tfp, struct tf_tbl_get_parms *parms);

/**
 * Retrieves bulk block of elements by sending a firmware request to
 * get the elements.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to Table get bulk parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_bulk_get(struct tf *tfp, struct tf_tbl_get_bulk_parms *parms);

/**
 * Retrieves the allocated resource info
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to Table resource info parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tbl_get_resc_info(struct tf *tfp, struct tf_tbl_resource_info *tbl);

#endif /* TF_TBL_TYPE_H */
