/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_IDENTIFIER_H_
#define _TF_IDENTIFIER_H_

#include "tf_core.h"

/* The Identifier module provides processing of Identifiers. */

/**
 * Identifer config params.
 *
 * @num_elements:	Number of identifier types in each of the
 *			configuration arrays
 * @cfg:		Identifier configuration array
 * @shadow_copy:	Boolean controlling the request shadow copy.
 * @resources:		Session resource allocations
 */
struct tf_ident_cfg_parms {
	u16				num_elements;
	struct tf_rm_element_cfg	*cfg;
	struct tf_session_resources	*resources;
};

/**
 * Identifier allocation parameter definition
 *
 * @dir:	receive or transmit direction
 * @type:	Identifier type
 * @id:		Identifier allocated
 */
struct tf_ident_alloc_parms {
	enum tf_dir		dir;
	enum tf_identifier_type	type;
	u16			*id;
};

/**
 * Identifier free parameter definition
 *
 * @dir:	receive or transmit direction
 * @type:	Identifier type
 * @id:		ID to free
 * @ref_cnt:	(experimental)Current refcnt after free
 */
struct tf_ident_free_parms {
	enum tf_dir		dir;
	enum tf_identifier_type	type;
	u16			id;
	u32			*ref_cnt;
};

/**
 * Identifier search parameter definition
 *
 * @dir:	receive or transmit direction
 * @type:	Identifier type
 * @search_id:	Identifier data to search for
 * @hit:	Set if matching identifier found
 * @ref_cnt:	Current ref count after allocation
 */
struct tf_ident_search_parms {
	enum tf_dir		dir;
	enum tf_identifier_type	type;
	u16			search_id;
	bool			*hit;
	u32			*ref_cnt;
};

/* Identifier RM database */
struct ident_rm_db {
	struct rm_db *ident_db[TF_DIR_MAX];
};

/**
 * Initializes the Identifier module with the requested DBs. Must be
 * invoked as the first thing before any of the access functions.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_ident_bind(struct tf *tfp, struct tf_ident_cfg_parms *parms);

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
int tf_ident_unbind(struct tf *tfp);

/**
 * Allocates a single identifier type.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_ident_alloc(struct tf *tfp, struct tf_ident_alloc_parms *parms);

/**
 * Free's a single identifier type.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_ident_free(struct tf *tfp, struct tf_ident_free_parms *parms);

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
int tf_ident_get_resc_info(struct tf *tfp,
			   struct tf_identifier_resource_info *parms);

#endif /* _TF_IDENTIFIER_H_ */
