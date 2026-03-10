/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_TCAM_H_
#define _TF_TCAM_H_

#include "tf_core.h"

/* The TCAM module provides processing of Internal TCAM types. */

/* Number of slices per row for WC TCAM */
extern u16 g_wc_num_slices_per_row;

/**
 * TCAM configuration parameters
 *
 * @num_elements:  Number of tcam types in each of the configuration arrays
 * @cfg:	   TCAM configuration array
 * @shadow_cfg:	   Shadow table type configuration array
 * @shadow_copy:   Boolean controlling the request shadow copy.
 * @resources:	   Session resource allocations
 * @wc_num_slices:	WC number of slices per row.
 */
struct tf_tcam_cfg_parms {
	u16				num_elements;
	struct tf_rm_element_cfg	*cfg;
	struct tf_shadow_tcam_cfg	*shadow_cfg;
	struct tf_session_resources	*resources;
	enum tf_wc_num_slice		wc_num_slices;
};

/**
 * TCAM allocation parameters
 *
 * @dir:	Receive or transmit direction
 * @type:	Type of the allocation
 * @key_size:	key size
 * @priority:	Priority of entry requested (definition TBD)
 * @idx:	Idx of allocated entry or found entry (if search_enable)
 */
struct tf_tcam_alloc_parms {
	enum tf_dir		dir;
	enum tf_tcam_tbl_type	type;
	u16			key_size;
	u32			priority;
	u16			idx;
};

/**
 * TCAM free parameters
 *
 * @dir:	Receive or transmit direction
 * @type:	Type of the allocation
 * @hcapi_type:	Type of HCAPI
 * @idx:	Index to free
 * @ref_cnt:	Reference count after free, only valid if session has been
 *		created with shadow_copy.
 */
struct tf_tcam_free_parms {
	enum tf_dir		dir;
	enum tf_tcam_tbl_type	type;
	u16			hcapi_type;
	u16			idx;
	u16			ref_cnt;
};

/**
 * TCAM allocate search parameters
 *
 * @dir:		Receive or transmit direction
 * @type:		TCAM table type
 * @hcapi_type:		Type of HCAPI
 * @key:		Key data to match on
 * @key_size:		Key size in bits
 * @mask:		Mask data to match on
 * @priority:		Priority of entry requested (definition TBD)
 * @alloc:		Allocate on miss.
 * @hit:		Set if matching entry found
 * @search_status:	Search result status (hit, miss, reject)
 * @ref_cnt:		Current refcnt after allocation
 * @result:		The result data from the search is copied here
 * @result_size:	result size in bits for the result data
 * @idx:		Index found
 */
struct tf_tcam_alloc_search_parms {
	enum tf_dir		dir;
	enum tf_tcam_tbl_type	type;
	u16			hcapi_type;
	u8			*key;
	u16			key_size;
	u8			*mask;
	u32			priority;
	u8			alloc;
	u8			hit;
	enum tf_search_status	search_status;
	u16			ref_cnt;
	u8			*result;
	u16			result_size;
	u16			idx;
};

/**
 * TCAM set parameters
 *
 * @dir:		Receive or transmit direction
 * @type:		Type of object to set
 * @hcapi_type:		Type of HCAPI
 * @idx:		Entry index to write to
 * @key:		array containing key
 * @mask:		array containing mask fields
 * @key_size:		key size
 * @result:		array containing result
 * @result_size:	result size
 */
struct tf_tcam_set_parms {
	enum tf_dir		dir;
	enum tf_tcam_tbl_type	type;
	u16			hcapi_type;
	u32			idx;
	u8			*key;
	u8			*mask;
	u16			key_size;
	u8			*result;
	u16			result_size;
};

/**
 * TCAM get parameters
 *
 * @dir:		Receive or transmit direction
 * @type:		Type of object to get
 * @hcapi_type:		Type of HCAPI
 * @idx:		Entry index to read
 * @key:		array containing key
 * @mask:		array containing mask fields
 * @key_size:		key size
 * @result:		array containing result
 * @result_size:	result size
 */
struct tf_tcam_get_parms {
	enum tf_dir		dir;
	enum tf_tcam_tbl_type	type;
	u16			hcapi_type;
	u32			idx;
	u8			*key;
	u8			*mask;
	u16			key_size;
	u8			*result;
	u16			result_size;
};

/* TCAM RM database */
struct tcam_rm_db {
	struct rm_db *tcam_db[TF_DIR_MAX];
};

/**
 * Initializes the TCAM module with the requested DBs. Must be
 * invoked as the first thing before any of the access functions.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_bind(struct tf *tfp, struct tf_tcam_cfg_parms *parms);

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
int tf_tcam_unbind(struct tf *tfp);

/**
 * Allocates the requested tcam type from the internal RM DB.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_alloc(struct tf *tfp, struct tf_tcam_alloc_parms *parms);

/**
 * Free's the requested table type and returns it to the DB. If shadow
 * DB is enabled its searched first and if found the element refcount
 * is decremented. If refcount goes to 0 then its returned to the
 * table type DB.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_free(struct tf *tfp, struct tf_tcam_free_parms *parms);

/**
 * Supported if Shadow DB is configured. Searches the Shadow DB for
 * any matching element. If found the refcount in the shadow DB is
 * updated accordingly. If not found a new element is allocated and
 * installed into the shadow DB.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_alloc_search(struct tf *tfp,
			 struct tf_tcam_alloc_search_parms *parms);

/**
 * Configures the requested element by sending a firmware request which
 * then installs it into the device internal structures.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_set(struct tf *tfp, struct tf_tcam_set_parms *parms);

/**
 * Retrieves the requested element by sending a firmware request to get
 * the element.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_get(struct tf *tfp, struct tf_tcam_get_parms *parms);

/**
 * Retrieves the allocated resource info
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_tcam_get_resc_info(struct tf *tfp, struct tf_tcam_resource_info *parms);

#endif /* _TF_TCAM_H */
