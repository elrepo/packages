/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef TF_RM_NEW_H_
#define TF_RM_NEW_H_

#include "tf_core.h"
#include "bitalloc.h"
#include "tf_device.h"

struct tf;

/* RM return codes */
#define TF_RM_ALLOCATED_ENTRY_FREE        0
#define TF_RM_ALLOCATED_ENTRY_IN_USE      1
#define TF_RM_ALLOCATED_NO_ENTRY_FOUND   -1

/**
 * The Resource Manager (RM) module provides basic DB handling for
 * internal resources. These resources exists within the actual device
 * and are controlled by the HCAPI Resource Manager running on the
 * firmware.
 *
 * The RM DBs are all intended to be indexed using TF types there for
 * a lookup requires no additional conversion. The DB configuration
 * specifies the TF Type to HCAPI Type mapping and it becomes the
 * responsibility of the DB initialization to handle this static
 * mapping.
 *
 * Accessor functions are providing access to the DB, thus hiding the
 * implementation.
 *
 * The RM DB will work on its initial allocated sizes so the
 * capability of dynamically growing a particular resource is not
 * possible. If this capability later becomes a requirement then the
 * MAX pool size of the chip needs to be added to the tf_rm_elem_info
 * structure and several new APIs would need to be added to allow for
 * growth of a single TF resource type.
 *
 * The access functions do not check for NULL pointers as they are a
 * support module, not called directly.
 */

/**
 * RM Element configuration enumeration. Used by the Device to
 * indicate how the RM elements the DB consists off, are to be
 * configured at time of DB creation. The TF may present types to the
 * ULP layer that is not controlled by HCAPI within the Firmware.
 */
enum tf_rm_elem_cfg_type {
	TF_RM_ELEM_CFG_NULL,	/* No configuration */
	TF_RM_ELEM_CFG_HCAPI,	/* HCAPI 'controlled', no RM storage so
				 * the module using the RM can chose to
				 * handle storage locally.
				 */
	TF_RM_ELEM_CFG_HCAPI_BA,	/* HCAPI 'controlled', uses a bit
					 * allocator pool for internal
					 * storage in the RM.
					 */
	TF_RM_ELEM_CFG_HCAPI_BA_PARENT,	/* HCAPI 'controlled', uses a bit
					 * allocator pool for internal storage
					 * in the RM but multiple TF types map
					 * to a single HCAPI type.  Parent
					 * manages the table.
					 */
	TF_RM_ELEM_CFG_HCAPI_BA_CHILD,	/* HCAPI 'controlled', uses a bit
					 * allocator pool for internal storage
					 * in the RM but multiple TF types map
					 * to a single HCAPI type.  Child
					 * accesses the parent db.
					 */
	TF_RM_TYPE_MAX
};

/* RM Reservation strategy enumeration. Type of strategy comes from
 * the HCAPI RM QCAPS handshake.
 */
enum tf_rm_resc_resv_strategy {
	TF_RM_RESC_RESV_STATIC_PARTITION,
	TF_RM_RESC_RESV_STRATEGY_1,
	TF_RM_RESC_RESV_STRATEGY_2,
	TF_RM_RESC_RESV_STRATEGY_3,
	TF_RM_RESC_RESV_MAX
};

/**
 * RM Element configuration structure, used by the Device to configure
 * how an individual TF type is configured in regard to the HCAPI RM
 * of same type.
 *
 * @cfg_type:	RM Element config controls how the DB for that element is
 *		processed.
 * @hcapi_type:	HCAPI RM Type for the element. Used for
 *		TF to HCAPI type conversion.
 * @parent_subtype: if cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_CHILD/PARENT
 *		    Parent Truflow module subtype associated with this
 *		    resource type.
 * @slices:	    if cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_CHILD/PARENT
 *		    Resource slices.  How many slices will fit in the
 *		    resource pool chunk size.
 */
struct tf_rm_element_cfg {
	enum tf_rm_elem_cfg_type	cfg_type;
	u16				hcapi_type;
	u16				parent_subtype;
	u8				slices;
};

/**
 * Allocation information for a single element.
 * @entry:	HCAPI RM allocated range information.
 *		NOTE: In case of dynamic allocation support this would have
 *		to be changed to linked list of tf_rm_entry instead.
 */
struct tf_rm_alloc_info {
	struct tf_resource_info entry;
};

/**
 * Create RM DB parameters
 *
 * @module:	Module type. Used for logging purposes.
 * @dir:	Receive or transmit direction.
 * @num_elements:	Number of elements.
 * @cfg:	Parameter structure array. Array size is num_elements.
 * @alloc_cnt:	Resource allocation count array. This array content
 *		originates from the tf_session_resources that is passed in
 *		on session open. Array size is num_elements.
 * @rm_db:	RM DB Handle[out]
 */
struct tf_rm_create_db_parms {
	enum tf_module_type		module;
	enum tf_dir			dir;
	u16				num_elements;
	struct tf_rm_element_cfg	*cfg;
	u16				*alloc_cnt;
	void				**rm_db;
};

/**
 * Free RM DB parameters
 *
 * @dir:	Receive or transmit direction
 * @rm_db:	RM DB Handle
 */
struct tf_rm_free_db_parms {
	enum tf_dir	dir;
	void		*rm_db;
};

/**
 * Allocate RM parameters for a single element
 *
 * @rm_db:	RM DB Handle
 * @subtype:	Module subtype indicates which DB entry to perform the
 *		action on.  (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
 *		TF_MODULE_TYPE_TCAM)
 * @index:	Pointer to the allocated index in normalized form. Normalized
 *		means the index has been adjusted, i.e. Full Action Record
 *		offsets.
 * @priority:	Priority, indicates the priority of the entry
 *		priority  0: allocate from top of the tcam (from index 0
 *		or lowest available index) priority !0: allocate from bottom
 *		of the tcam (from highest available index)
 * @base_index: Pointer to the allocated index before adjusted.
 */
struct tf_rm_allocate_parms {
	void	*rm_db;
	u16	subtype;
	u32	*index;
	u32	priority;
	u32	*base_index;
};

/**
 * Free RM parameters for a single element
 *
 * @rm_db:	RM DB Handle
 * @subtype:	TF subtype indicates which DB entry to perform the action on.
 *		(e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
 *		TF_MODULE_TYPE_TCAM)
 * @index:	Index to free
 */
struct tf_rm_free_parms {
	void	*rm_db;
	u16	subtype;
	u16	index;
};

/**
 * Is Allocated parameters for a single element
 *
 * @rm_db:	RM DB Handle
 * @subtype:	TF subtype indicates which DB entry to perform the
 *		action on. (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
 *		TF_MODULE_TYPE_TCAM)
 * @index:	Index to free
 * @allocated:	Pointer to flag that indicates the state of the query
 * @base_index:	Pointer to the allocated index before adjusted.
 */
struct tf_rm_is_allocated_parms {
	void	*rm_db;
	u16	subtype;
	u32	index;
	int	*allocated;
	u32	*base_index;
};

/**
 * Get Allocation information for a single element
 *
 * @rm_db:	RM DB Handle
 * @subtype:	TF subtype indicates which DB entry to perform the
 *		action on. (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
 *		TF_MODULE_TYPE_TCAM)
 * @info:	Pointer to the requested allocation information for
 *		the specified subtype
 */
struct tf_rm_get_alloc_info_parms {
	void			*rm_db;
	u16			subtype;
	struct tf_rm_alloc_info	*info;
};

/**
 * Get HCAPI type parameters for a single element
 *
 * @rm_db:	RM DB Handle
 * @subtype:	TF subtype indicates which DB entry to perform the
 *		action on. (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
 *		TF_MODULE_TYPE_TCAM)
 * @hcapi_type:	Pointer to the hcapi type for the specified subtype[out]
 */
struct tf_rm_get_hcapi_parms {
	void	*rm_db;
	u16	subtype;
	u16	*hcapi_type;
};

/**
 * Get Slices parameters for a single element
 *
 * @rm_db:	RM DB Handle
 * @subtype:	TF subtype indicates which DB entry to perform the action on.
 *		(e.g. TF_TBL_TYPE_FULL_ACTION subtype of module
 *		TF_MODULE_TYPE_TABLE)
 * @slices:	Pointer to number of slices for the given type
 */
struct tf_rm_get_slices_parms {
	void	*rm_db;
	u16	subtype;
	u16	*slices;
};

/**
 * Get InUse count parameters for single element
 *
 * @rm_db:	RM DB Handle
 * @subtype:	TF subtype indicates which DB entry to perform the
 *		action on. (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
 *		TF_MODULE_TYPE_TCAM)
 * @count:	Pointer to the inuse count for the specified subtype[out]
 */
struct tf_rm_get_inuse_count_parms {
	void	*rm_db;
	u16	subtype;
	u16	*count;
};

/**
 * Check if the indexes are in the range of reserved resource
 *
 * @rm_db:	RM DB Handle
 * @subtype:	TF subtype indicates which DB entry to perform the
 *		action on. (e.g. TF_TCAM_TBL_TYPE_L2_CTXT subtype of module
 *		TF_MODULE_TYPE_TCAM)
 * @starting_index:	Starting index
 * @num_entries:	number of entries
 *
 */
struct tf_rm_check_indexes_in_range_parms {
	void	*rm_db;
	u16	subtype;
	u16	starting_index;
	u16	num_entries;
};

/**
 * Creates and fills a Resource Manager (RM) DB with requested
 * elements. The DB is indexed per the parms structure.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to create parameters
 *
 * NOTE:
 * - Fail on parameter check
 * - Fail on DB creation, i.e. alloc amount is not possible or validation fails
 * - Fail on DB creation if DB already exist
 *
 * - Allocs local DB
 * - Does hcapi qcaps
 * - Does hcapi reservation
 * - Populates the pool with allocated elements
 * - Returns handle to the created DB
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_create_db(struct tf *tfp, struct tf_rm_create_db_parms *parms);

/**
 * Creates and fills a Resource Manager (RM) DB with requested
 * elements. The DB is indexed per the parms structure. It only retrieve
 * allocated resource information for a exist session.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to create parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_create_db_no_reservation(struct tf *tfp,
				   struct tf_rm_create_db_parms *parms);

/**
 * Closes the Resource Manager (RM) DB and frees all allocated
 * resources per the associated database.
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to free parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_free_db(struct tf *tfp, struct tf_rm_free_db_parms *parms);

/**
 * Allocates a single element for the type specified, within the DB.
 *
 * @parms:	Pointer to allocate parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 *   - (-ENOMEM) if pool is empty
 */
int tf_rm_allocate(struct tf_rm_allocate_parms *parms);

/**
 * Free's a single element for the type specified, within the DB.
 *
 * @parms:	Pointer to free parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_free(struct tf_rm_free_parms *parms);

/**
 * Performs an allocation verification check on a specified element.
 *
 * @parms:	Pointer to is allocated parameters
 *
 * NOTE:
 *  - If pool is set to Chip MAX, then the query index must be checked
 *    against the allocated range and query index must be allocated as well.
 *  - If pool is allocated size only, then check if query index is allocated.
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_is_allocated(struct tf_rm_is_allocated_parms *parms);

/**
 * Retrieves an elements allocation information from the Resource
 * Manager (RM) DB.
 *
 * @parms:	Pointer to get info parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_get_info(struct tf_rm_get_alloc_info_parms *parms);

/**
 * Retrieves all elements allocation information from the Resource
 * Manager (RM) DB.
 *
 * @parms:	Pointer to get info parameters
 * @size:	number of the elements for the specific module
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_get_all_info(struct tf_rm_get_alloc_info_parms *parms, int size);

/**
 * Performs a lookup in the Resource Manager DB and retrieves the
 * requested HCAPI RM type.
 *
 * @parms:	Pointer to get hcapi parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_get_hcapi_type(struct tf_rm_get_hcapi_parms *parms);

/**
 * Performs a lookup in the Resource Manager DB and retrieves the
 * requested HCAPI RM type inuse count.
 *
 * @parms:	Pointer to get inuse parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_get_inuse_count(struct tf_rm_get_inuse_count_parms *parms);

/**
 * Check if the requested indexes are in the range of reserved resource.
 *
 * @parms:	Pointer to get inuse parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_check_indexes_in_range(struct tf_rm_check_indexes_in_range_parms
					*parms);

/**
 * Get the number of slices per resource bit allocator for the resource type
 *
 * @parms:	Pointer to get inuse parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_rm_get_slices(struct tf_rm_get_slices_parms *parms);

#endif /* TF_RM_NEW_H_ */
