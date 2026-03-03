/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2021 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_TCAM_MGR_H_
#define _CFA_TCAM_MGR_H_

#include <linux/errno.h>
#include "bnxt_hsi.h"
#include "tf_core.h"

/* The TCAM module provides processing of Internal TCAM types. */

#define ENTRY_ID_INVALID 65535

#define TF_TCAM_PRIORITY_MIN 0
#define TF_TCAM_PRIORITY_MAX 65535

#define CFA_TCAM_MGR_TBL_TYPE_START 0

/* Logical TCAM tables */
enum cfa_tcam_mgr_tbl_type {
	CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_AFM =
		CFA_TCAM_MGR_TBL_TYPE_START,
	CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_APPS,
	CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_AFM,
	CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_APPS,
	CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_AFM,
	CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_APPS,
	CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_AFM,
	CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_APPS,
	CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_AFM,
	CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_APPS,
	CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_AFM,
	CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_APPS,
	CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_AFM,
	CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_APPS,
	CFA_TCAM_MGR_TBL_TYPE_MAX
};

enum cfa_tcam_mgr_device_type {
	CFA_TCAM_MGR_DEVICE_TYPE_WH = 0, /* Whitney+ */
	CFA_TCAM_MGR_DEVICE_TYPE_SR,     /* Stingray */
	CFA_TCAM_MGR_DEVICE_TYPE_THOR,   /* Thor */
	CFA_TCAM_MGR_DEVICE_TYPE_MAX     /* Maximum */
};

/**
 * TCAM Manager initialization parameters
 *
 * @resc:		TCAM resources reserved type element is not used.
 * @max_entries:	maximum number of entries available.
 */
struct cfa_tcam_mgr_init_parms {
	struct tf_rm_resc_entry resc[TF_DIR_MAX][CFA_TCAM_MGR_TBL_TYPE_MAX];
	u32 max_entries;
};

/**
 * TCAM Manager initialization parameters
 *
 * These are bitmasks. Set if TCAM Manager is managing a logical TCAM.
 * Each bitmask is indexed by logical TCAM table ID.
 */
struct cfa_tcam_mgr_qcaps_parms {
	u32 rx_tcam_supported;
	u32 tx_tcam_supported;
};

/**
 * TCAM Manager configuration parameters
 *
 * @num_elements:	Number of tcam types in each of the configuration arrays
 * @tcam_cnt:		Session resource allocations
 * @tf_rm_resc_entry:	TCAM Locations reserved
 */
struct cfa_tcam_mgr_cfg_parms {
	u16 num_elements;
	u16 tcam_cnt[TF_DIR_MAX][CFA_TCAM_MGR_TBL_TYPE_MAX];
	struct tf_rm_resc_entry (*resv_res)[CFA_TCAM_MGR_TBL_TYPE_MAX];
};

/**
 * TCAM Manager allocation parameters
 *
 * @dir:	Receive or transmit direction
 * @type:	Type of the allocation
 * @hcapi_type:	Type of HCAPI
 * @key_size:	key size (bytes)
 * @priority:	Priority of entry requested (definition TBD)
 * @id:		Id of allocated entry or found entry (if search_enable)
 */
struct cfa_tcam_mgr_alloc_parms {
	enum tf_dir dir;
	enum cfa_tcam_mgr_tbl_type type;
	u16 hcapi_type;
	u16 key_size;
	u16 priority;
	u16 id;
};

/**
 * TCAM Manager free parameters
 *
 * @dir:	Receive or transmit direction
 * @type:	Type of the allocation. If the type is not known, set the
 *		type to CFA_TCAM_MGR_TBL_TYPE_MAX.
 * @hcapi_type:	Type of HCAPI
 * @id:		Entry ID to free
 */
struct cfa_tcam_mgr_free_parms {
	enum tf_dir dir;
	enum cfa_tcam_mgr_tbl_type type;
	u16 hcapi_type;
	u16 id;
};

/**
 * TCAM Manager set parameters
 *
 * @dir:	Receive or transmit direction
 * @type:	Type of object to set
 * @hcapi_type:	Type of HCAPI
 * @id:		Entry ID to write to
 * @key:	array containing key
 * @mask:	array containing mask fields
 * @key_size:	key size (bytes)
 * @result:	array containing result
 * @result_size: result size (bytes)
 */
struct cfa_tcam_mgr_set_parms {
	enum tf_dir dir;
	enum cfa_tcam_mgr_tbl_type type;
	u16 hcapi_type;
	u16 id;
	u8 *key;
	u8 *mask;
	u16 key_size;
	u8 *result;
	u16 result_size;
};

/**
 * TCAM Manager get parameters
 *
 * @dir:	Receive or transmit direction
 * @type:	Type of object to get
 * @hcapi_type:	Type of HCAPI
 * @id:		Entry ID to read
 * @key:	array containing key
 * @mask:	array containing mask fields
 * @key_size:	key size (bytes)
 * @result:	array containing result
 * @result_size: result size (bytes)
 */
struct cfa_tcam_mgr_get_parms {
	enum tf_dir dir;
	enum cfa_tcam_mgr_tbl_type type;
	u16 hcapi_type;
	u16 id;
	u8 *key;
	u8 *mask;
	u16 key_size;
	u8 *result;
	u16 result_size;
};

const char *cfa_tcam_mgr_tbl_2_str(enum cfa_tcam_mgr_tbl_type tcam_type);

/**
 * Initializes the TCAM Manager
 *
 * @type:	Device type
 *
 * Returns
 *   - (0) if successful.
 *   - (<0) on failure.
 */
int cfa_tcam_mgr_init(struct tf *tfp, enum cfa_tcam_mgr_device_type type,
		      struct cfa_tcam_mgr_init_parms *parms);

/**
 * Returns the physical TCAM table that a logical TCAM table uses.
 *
 * @type:	Logical table type
 *
 * Returns
 *   - (tf_tcam_tbl_type) if successful.
 *   - (<0) on failure.
 */
int cfa_tcam_mgr_get_phys_table_type(enum cfa_tcam_mgr_tbl_type type);

/**
 * Queries the capabilities of TCAM Manager.
 *
 * @tfp:	Pointer to Truflow handle
 * @parms:	Pointer to parameters to be returned
 *
 * Returns
 *   - (0) if successful.
 *   - (<0) on failure.
 */
int cfa_tcam_mgr_qcaps(struct tf *tfp, struct cfa_tcam_mgr_qcaps_parms *parms);

/**
 * Initializes the TCAM module with the requested DBs. Must be
 * invoked as the first thing before any of the access functions.
 *
 * @tfp:	Pointer to Truflow handle
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int cfa_tcam_mgr_bind(struct tf *tfp, struct cfa_tcam_mgr_cfg_parms *parms);

/**
 * Cleans up the private DBs and releases all the data.
 *
 * @tfp:	Pointer to Truflow handle
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int cfa_tcam_mgr_unbind(struct tf *tfp);

/**
 * Allocates the requested tcam type from the internal RM DB.
 *
 * @tfp:	Pointer to Truflow handle
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int cfa_tcam_mgr_alloc(struct tf *tfp, struct cfa_tcam_mgr_alloc_parms *parms);

/**
 * Free's the requested table type and returns it to the DB. If shadow
 * DB is enabled its searched first and if found the element refcount
 * is decremented. If refcount goes to 0 then its returned to the
 * table type DB.
 *
 * @tfp:	Pointer to Truflow handle
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int cfa_tcam_mgr_free(struct tf *tfp, struct cfa_tcam_mgr_free_parms *parms);

/**
 * Configures the requested element by sending a firmware request which
 * then installs it into the device internal structures.
 *
 * @tfp:	Pointer to Truflow handle
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int cfa_tcam_mgr_set(struct tf *tfp, struct cfa_tcam_mgr_set_parms *parms);

/**
 * Retrieves the requested element by sending a firmware request to get
 * the element.
 *
 * @tfp:	Pointer to Truflow handle
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int cfa_tcam_mgr_get(struct tf *tfp, struct cfa_tcam_mgr_get_parms *parms);

void cfa_tcam_mgr_rows_dump(struct tf *tfp, enum tf_dir dir,
			    enum cfa_tcam_mgr_tbl_type type);
void cfa_tcam_mgr_tables_dump(struct tf *tfp, enum tf_dir dir,
			      enum cfa_tcam_mgr_tbl_type type);
void cfa_tcam_mgr_entries_dump(struct tf *tfp);

#endif /* _CFA_TCAM_MGR_H */
