/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef TF_IF_TBL_TYPE_H_
#define TF_IF_TBL_TYPE_H_

#include "tf_core.h"

#define CFA_IF_TBL_TYPE_INVALID 65535	/* Invalid CFA types */

struct tf;

/* The IF Table module provides processing of Internal TF
 * interface table types.
 */

/* IF table configuration enumeration. */
enum tf_if_tbl_cfg_type {
	TF_IF_TBL_CFG_NULL,	/* No configuration */
	TF_IF_TBL_CFG,		/* HCAPI 'controlled' */
};

/**
 * IF table configuration structure, used by the Device to configure
 * how an individual TF type is configured in regard to the HCAPI type.
 *
 * @cfg_type:	IF table config controls how the DB for that element is
 *		processed.
 * @hcapi_type:	HCAPI Type for the element. Used for TF to HCAPI type
 *		conversion.
 */
struct tf_if_tbl_cfg {
	enum tf_if_tbl_cfg_type	cfg_type;
	u16			hcapi_type;
};

/**
 * Get HCAPI type parameters for a single element
 *
 * @tbl_db:	IF Tbl DB Handle
 * @db_index:	DB Index, indicates which DB entry to perform the
 *		action on.
 * @hcapi_type:	Pointer to the hcapi type for the specified db_index
 */
struct tf_if_tbl_get_hcapi_parms {
	void	*tbl_db;
	u16	db_index;
	u16	*hcapi_type;
};

/**
 * Table configuration parameters
 *
 * @num_elements:  Number of table types in each of the configuration arrays
 * @cfg:	   Table Type element configuration array
 * @shadow_cfg:	   Shadow table type configuration array
 * @shadow_copy:   Boolean controlling the request shadow copy.
 */
struct tf_if_tbl_cfg_parms {
	u16				num_elements;
	struct tf_if_tbl_cfg		*cfg;
	struct tf_shadow_if_tbl_cfg	*shadow_cfg;
};

/**
 * IF Table set parameters
 *
 * @dir:	Receive or transmit direction
 * @type:	Type of object to set
 * @hcapi_type:	Type of HCAPI
 * @data:	Entry data
 * @data_sz_in_bytes:	Entry size
 * @idx:	Entry index to write to
 */
struct tf_if_tbl_set_parms {
	enum tf_dir		dir;
	enum tf_if_tbl_type	type;
	u16			hcapi_type;
	u8			*data;
	u16			data_sz_in_bytes;
	u32			idx;
};

/**
 * IF Table get parameters
 *
 * @dir:	Receive or transmit direction
 * @type:	Type of object to get
 * @hcapi_type:	Type of HCAPI
 * @data:	Entry data
 * @data_sz_in_bytes:	Entry size
 * @idx:	Entry index to read
 */
struct tf_if_tbl_get_parms {
	enum tf_dir		dir;
	enum tf_if_tbl_type	type;
	u16			hcapi_type;
	u8			*data;
	u16			data_sz_in_bytes;
	u32			idx;
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
int tf_if_tbl_bind(struct tf *tfp, struct tf_if_tbl_cfg_parms *parms);

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
int tf_if_tbl_unbind(struct tf *tfp);

/**
 * Configures the requested element by sending a firmware request which
 * then installs it into the device internal structures.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to Interface Table set parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_if_tbl_set(struct tf *tfp,
		  struct tf_if_tbl_set_parms *parms);

/**
 * Retrieves the requested element by sending a firmware request to get
 * the element.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to Interface Table get parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_if_tbl_get(struct tf *tfp,
		  struct tf_if_tbl_get_parms *parms);

#endif /* TF_IF_TBL_TYPE_H */
