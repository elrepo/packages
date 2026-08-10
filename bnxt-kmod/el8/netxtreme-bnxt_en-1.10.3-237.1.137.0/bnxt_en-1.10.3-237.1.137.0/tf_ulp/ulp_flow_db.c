// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "ulp_linux.h"
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "bnxt_tf_common.h"
#include "ulp_utils.h"
#include "ulp_template_struct.h"
#include "ulp_mapper.h"
#include "ulp_flow_db.h"
#include "ulp_fc_mgr.h"
#include "ulp_sc_mgr.h"
#include "ulp_port_db.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
#define ULP_FLOW_DB_RES_DIR_BIT		31
#define ULP_FLOW_DB_RES_DIR_MASK	0x80000000
#define ULP_FLOW_DB_RES_FUNC_BITS	28
#define ULP_FLOW_DB_RES_FUNC_MASK	0x70000000
#define ULP_FLOW_DB_RES_NXT_MASK	0x0FFFFFFF
#define ULP_FLOW_DB_RES_FUNC_UPPER	5
#define ULP_FLOW_DB_RES_FUNC_NEED_LOWER	0x80
#define ULP_FLOW_DB_RES_FUNC_LOWER_MASK	0x1F

/* Macro to copy the nxt_resource_idx */
#define ULP_FLOW_DB_RES_NXT_SET(dst, src)	{(dst) |= ((src) &\
					 ULP_FLOW_DB_RES_NXT_MASK); }
#define ULP_FLOW_DB_RES_NXT_RESET(dst)	((dst) &= ~(ULP_FLOW_DB_RES_NXT_MASK))

/**
 * Helper function to set the bit in the active flows
 * No validation is done in this function.
 *
 * @flow_db: Ptr to flow database
 * @flow_type: - specify default or regular
 * @idx: The index to bit to be set or reset.
 * @flag: 1 to set and 0 to reset.
 *
 * returns none
 */
static void
ulp_flow_db_active_flows_bit_set(struct bnxt_ulp_flow_db *flow_db,
				 enum bnxt_ulp_fdb_type flow_type,
				 u32 idx,
				 u32 flag)
{
	struct bnxt_ulp_flow_tbl *f_tbl = &flow_db->flow_tbl;
	u32 a_idx = idx / ULP_INDEX_BITMAP_SIZE;

	if (flag) {
		if (flow_type == BNXT_ULP_FDB_TYPE_REGULAR || flow_type ==
		    BNXT_ULP_FDB_TYPE_RID)
			ULP_INDEX_BITMAP_SET(f_tbl->active_reg_flows[a_idx],
					     idx);
		if (flow_type == BNXT_ULP_FDB_TYPE_DEFAULT || flow_type ==
		    BNXT_ULP_FDB_TYPE_RID)
			ULP_INDEX_BITMAP_SET(f_tbl->active_dflt_flows[a_idx],
					     idx);
	} else {
		if (flow_type == BNXT_ULP_FDB_TYPE_REGULAR || flow_type ==
		    BNXT_ULP_FDB_TYPE_RID)
			ULP_INDEX_BITMAP_RESET(f_tbl->active_reg_flows[a_idx],
					       idx);
		if (flow_type == BNXT_ULP_FDB_TYPE_DEFAULT || flow_type ==
		    BNXT_ULP_FDB_TYPE_RID)
			ULP_INDEX_BITMAP_RESET(f_tbl->active_dflt_flows[a_idx],
					       idx);
	}
}

/**
 * Helper function to check if given fid is active flow.
 * No validation being done in this function.
 *
 * @flow_db: Ptr to flow database
 * @flow_type: - specify default or regular
 * @idx: The index to bit to be set or reset.
 *
 * returns 1 on set or 0 if not set.
 */
static int
ulp_flow_db_active_flows_bit_is_set(struct bnxt_ulp_flow_db *flow_db,
				    enum bnxt_ulp_fdb_type flow_type,
				    u32 idx)
{
	struct bnxt_ulp_flow_tbl *f_tbl = &flow_db->flow_tbl;
	u32 a_idx = idx / ULP_INDEX_BITMAP_SIZE;
	u32 reg, dflt;

	if (idx >= f_tbl->num_flows) {
		netdev_err(NULL, "Invalid flow idx 0x%x\n", idx);
		return 0;
	}

	reg = ULP_INDEX_BITMAP_GET(f_tbl->active_reg_flows[a_idx], idx);
	dflt = ULP_INDEX_BITMAP_GET(f_tbl->active_dflt_flows[a_idx], idx);

	switch (flow_type) {
	case BNXT_ULP_FDB_TYPE_REGULAR:
		return (reg && !dflt);
	case BNXT_ULP_FDB_TYPE_DEFAULT:
		return (!reg && dflt);
	case BNXT_ULP_FDB_TYPE_RID:
		return (reg && dflt);
	default:
		return 0;
	}
}

static inline enum tf_dir
ulp_flow_db_resource_dir_get(struct ulp_fdb_resource_info *res_info)
{
	return ((res_info->nxt_resource_idx & ULP_FLOW_DB_RES_DIR_MASK) >>
		ULP_FLOW_DB_RES_DIR_BIT);
}

static u8
ulp_flow_db_resource_func_get(struct ulp_fdb_resource_info *res_info)
{
	u8 func;

	func = (((res_info->nxt_resource_idx & ULP_FLOW_DB_RES_FUNC_MASK) >>
		 ULP_FLOW_DB_RES_FUNC_BITS) << ULP_FLOW_DB_RES_FUNC_UPPER);
	/* The resource func is split into upper and lower */
	if (func & ULP_FLOW_DB_RES_FUNC_NEED_LOWER)
		return (func | res_info->resource_func_lower);
	return func;
}

/**
 * Helper function to copy the resource params to resource info
 *  No validation being done in this function.
 *
 * @resource_info: Ptr to resource information
 * @params: The input params from the caller
 * returns none
 */
static void
ulp_flow_db_res_params_to_info(struct ulp_fdb_resource_info *resource_info,
			       struct ulp_flow_db_res_params *params)
{
	u32 resource_func;

	resource_info->nxt_resource_idx |= ((params->direction <<
				      ULP_FLOW_DB_RES_DIR_BIT) &
				     ULP_FLOW_DB_RES_DIR_MASK);
	resource_func = (params->resource_func >> ULP_FLOW_DB_RES_FUNC_UPPER);
	resource_info->nxt_resource_idx |= ((resource_func <<
					     ULP_FLOW_DB_RES_FUNC_BITS) &
					    ULP_FLOW_DB_RES_FUNC_MASK);

	if (params->resource_func & ULP_FLOW_DB_RES_FUNC_NEED_LOWER) {
		/* Break the resource func into two parts */
		resource_func = (params->resource_func &
				 ULP_FLOW_DB_RES_FUNC_LOWER_MASK);
		resource_info->resource_func_lower = resource_func;
	}

	/* Store the handle as 64bit only for EM table entries */
	if (params->resource_func != BNXT_ULP_RESOURCE_FUNC_EM_TABLE &&
	    params->resource_func != BNXT_ULP_RESOURCE_FUNC_CMM_TABLE &&
	    params->resource_func != BNXT_ULP_RESOURCE_FUNC_CMM_STAT) {
		resource_info->resource_hndl = (u32)params->resource_hndl;
		resource_info->key_data = params->key_data;
		resource_info->resource_type = params->resource_type;
		resource_info->resource_sub_type = params->resource_sub_type;
		resource_info->fdb_flags = params->fdb_flags;
	} else {
		resource_info->resource_em_handle = params->resource_hndl;
		resource_info->reserve_flag = params->reserve_flag;
	}
}

/**
 * Helper function to copy the resource params to resource info
 *  No validation being done in this function.
 *
 * @resource_info: Ptr to resource information
 * @params: The output params to the caller
 *
 * returns none
 */
static void
ulp_flow_db_res_info_to_params(struct ulp_fdb_resource_info *resource_info,
			       struct ulp_flow_db_res_params *params)
{
	memset(params, 0, sizeof(struct ulp_flow_db_res_params));

	/* use the helper function to get the resource func */
	params->direction = ulp_flow_db_resource_dir_get(resource_info);
	params->resource_func = ulp_flow_db_resource_func_get(resource_info);

	if (params->resource_func == BNXT_ULP_RESOURCE_FUNC_EM_TABLE ||
	    params->resource_func == BNXT_ULP_RESOURCE_FUNC_CMM_TABLE ||
	    params->resource_func == BNXT_ULP_RESOURCE_FUNC_CMM_STAT) {
		params->resource_hndl = resource_info->resource_em_handle;
		params->reserve_flag = resource_info->reserve_flag;
	} else if (params->resource_func & ULP_FLOW_DB_RES_FUNC_NEED_LOWER) {
		params->resource_hndl = resource_info->resource_hndl;
		params->key_data = resource_info->key_data;
		params->resource_type = resource_info->resource_type;
		params->resource_sub_type = resource_info->resource_sub_type;
		params->fdb_flags = resource_info->fdb_flags;
	}
}

/**
 * Helper function to allocate the flow table and initialize
 * the stack for allocation operations.
 *
 * @flow_db: Ptr to flow database structure
 *
 * Returns 0 on success or negative number on failure.
 */
static int
ulp_flow_db_alloc_resource(struct bnxt_ulp_context *ulp_ctxt,
			   struct bnxt_ulp_flow_db *flow_db)
{
	struct bnxt_ulp_flow_tbl *flow_tbl;
	u32 idx = 0;
	u32 size;

	flow_tbl = &flow_db->flow_tbl;

	size = sizeof(struct ulp_fdb_resource_info) * flow_tbl->num_resources;
	flow_tbl->flow_resources = vzalloc(size);

	if (!flow_tbl->flow_resources)
		return -ENOMEM;

	size = sizeof(u32) * flow_tbl->num_resources;
	flow_tbl->flow_tbl_stack = vzalloc(size);
	if (!flow_tbl->flow_tbl_stack)
		return -ENOMEM;

	size = (flow_tbl->num_flows / sizeof(u64)) + 1;
	size = ULP_BYTE_ROUND_OFF_8(size);
	flow_tbl->active_reg_flows = vzalloc(size);
	if (!flow_tbl->active_reg_flows)
		return -ENOMEM;

	flow_tbl->active_dflt_flows = vzalloc(size);
	if (!flow_tbl->active_dflt_flows)
		return -ENOMEM;

	/* Initialize the stack table. */
	for (idx = 0; idx < flow_tbl->num_resources; idx++)
		flow_tbl->flow_tbl_stack[idx] = idx;

	/* Ignore the first element in the list. */
	flow_tbl->head_index = 1;
	/* Tail points to the last entry in the list. */
	flow_tbl->tail_index = flow_tbl->num_resources - 1;
	return 0;
}

/**
 * Helper function to deallocate the flow table.
 *
 * @flow_db: Ptr to flow database structure
 *
 * Returns none.
 */
static void
ulp_flow_db_dealloc_resource(struct bnxt_ulp_flow_db *flow_db)
{
	struct bnxt_ulp_flow_tbl *flow_tbl = &flow_db->flow_tbl;

	/* Free all the allocated tables in the flow table. */
	vfree(flow_tbl->active_reg_flows);
	flow_tbl->active_reg_flows = NULL;

	vfree(flow_tbl->active_dflt_flows);
	flow_tbl->active_dflt_flows = NULL;

	vfree(flow_tbl->flow_tbl_stack);
	flow_tbl->flow_tbl_stack = NULL;

	vfree(flow_tbl->flow_resources);
	flow_tbl->flow_resources = NULL;
}

/**
 * Helper function to add function id to the flow table
 *
 * @flow_db: Ptr to flow table
 * @flow_id: The flow id of the flow
 * @func_id: The func_id to be set, for reset pass zero
 *
 * returns none
 */
static void
ulp_flow_db_func_id_set(struct bnxt_ulp_context *ulp_ctxt,
			struct bnxt_ulp_flow_db *flow_db,
			u32 flow_id,
			u32 func_id)
{
	/* set the function id in the function table */
	if (flow_id < flow_db->func_id_tbl_size)
		flow_db->func_id_tbl[flow_id] = func_id;
	else /* This should never happen */
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid flow id, flowdb corrupt\n");
}

/**
 * Initialize the parent-child database. Memory is allocated in this
 * call and assigned to the database
 *
 * @flow_db: [in] Ptr to flow table
 * @num_entries: [in] - number of entries to allocate
 *
 * Returns 0 on success or negative number on failure.
 */
static int
ulp_flow_db_parent_tbl_init(struct bnxt_ulp_flow_db *flow_db,
			    u32 num_entries)
{
	struct ulp_fdb_parent_child_db *p_db;
	u32 size, idx;

	if (!num_entries)
		return 0;

	/* update the sizes for the allocation */
	p_db = &flow_db->parent_child_db;
	p_db->child_bitset_size = (flow_db->flow_tbl.num_flows /
				   sizeof(u64)) + 1; /* size in bytes */
	p_db->child_bitset_size = ULP_BYTE_ROUND_OFF_8(p_db->child_bitset_size);
	p_db->entries_count = num_entries;

	/* allocate the memory */
	p_db->parent_flow_tbl = vzalloc(sizeof(*p_db->parent_flow_tbl) * p_db->entries_count);
	if (!p_db->parent_flow_tbl)
		return -ENOMEM;

	size = p_db->child_bitset_size * p_db->entries_count;

	/* allocate the big chunk of memory to be statically carved into
	 * child_fid_bitset pointer.
	 */
	p_db->parent_flow_tbl_mem = vzalloc(size);
	if (!p_db->parent_flow_tbl_mem)
		return -ENOMEM;

	/* set the pointers in parent table to their offsets */
	for (idx = 0 ; idx < p_db->entries_count; idx++) {
		p_db->parent_flow_tbl[idx].child_fid_bitset =
			(u64 *)&p_db->parent_flow_tbl_mem[idx *
			p_db->child_bitset_size];
	}
	/* success */
	return 0;
}

/*
 * Deinitialize the parent-child database. Memory is deallocated in
 * this call and all flows should have been purged before this
 * call.
 *
 * flow_db [in] Ptr to flow table
 *
 * Returns none
 */
static void
ulp_flow_db_parent_tbl_deinit(struct bnxt_ulp_flow_db *flow_db)
{
	/* free the memory related to parent child database */
	vfree(flow_db->parent_child_db.parent_flow_tbl_mem);
	flow_db->parent_child_db.parent_flow_tbl_mem = NULL;

	vfree(flow_db->parent_child_db.parent_flow_tbl);
	flow_db->parent_child_db.parent_flow_tbl = NULL;
}

/**
 * Initialize the flow database. Memory is allocated in this
 * call and assigned to the flow database.
 *
 * @ulp_ctxt: Ptr to ulp context
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_flow_db_init(struct bnxt_ulp_context *ulp_ctxt)
{
	struct bnxt_ulp_device_params *dparms;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	enum bnxt_ulp_flow_mem_type mtype;
	struct bnxt_ulp_flow_db *flow_db;
	u32 dev_id, num_flows;

	/* Get the dev specific number of flows that needed to be supported. */
	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctxt, &dev_id)) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms) {
		netdev_dbg(ulp_ctxt->bp->dev, "could not fetch the device params\n");
		return -ENODEV;
	}

	flow_db = vzalloc(sizeof(*flow_db));
	if (!flow_db)
		return -ENOMEM;

	/* Attach the flow database to the ulp context. */
	bnxt_ulp_cntxt_ptr2_flow_db_set(ulp_ctxt, flow_db);

	/* Determine the number of flows based on EM type */
	if (bnxt_ulp_cntxt_mem_type_get(ulp_ctxt, &mtype))
		goto error_free;

	if (mtype == BNXT_ULP_FLOW_MEM_TYPE_INT)
		num_flows = dparms->int_flow_db_num_entries;
	else
		num_flows = dparms->ext_flow_db_num_entries;

	/* Populate the regular flow table limits. */
	flow_tbl = &flow_db->flow_tbl;
	flow_tbl->num_flows = num_flows + 1;
	flow_tbl->num_resources = ((num_flows + 1) *
				   dparms->num_resources_per_flow);

	/* Include the default flow table limits. */
	flow_tbl->num_flows += (BNXT_FLOW_DB_DEFAULT_NUM_FLOWS + 1);
	flow_tbl->num_resources += ((BNXT_FLOW_DB_DEFAULT_NUM_FLOWS + 1) *
				    BNXT_FLOW_DB_DEFAULT_NUM_RESOURCES);

	/* Allocate the resource for the flow table. */
	if (ulp_flow_db_alloc_resource(ulp_ctxt, flow_db))
		goto error_free;

	/* add 1 since we are not using index 0 for flow id */
	flow_db->func_id_tbl_size = flow_tbl->num_flows + 1;
	/* Allocate the function Id table */
	flow_db->func_id_tbl = vzalloc(flow_db->func_id_tbl_size * sizeof(u16));
	if (!flow_db->func_id_tbl)
		goto error_free;
	/* initialize the parent child database */
	if (ulp_flow_db_parent_tbl_init(flow_db, dparms->fdb_parent_flow_entries)) {
		netdev_dbg(ulp_ctxt->bp->dev, "Failed to allocate mem for parent child db\n");
		goto error_free;
	}

	/* All good so return. */
	netdev_dbg(ulp_ctxt->bp->dev, "FlowDB initialized with %d flows.\n",
		   flow_tbl->num_flows);
	return 0;
error_free:
	ulp_flow_db_deinit(ulp_ctxt);
	return -ENOMEM;
}

/**
 * Deinitialize the flow database. Memory is deallocated in
 * this call and all flows should have been purged before this
 * call.
 *
 * @ulp_ctxt: Ptr to ulp context
 *
 * Returns 0 on success.
 */
int
ulp_flow_db_deinit(struct bnxt_ulp_context *ulp_ctxt)
{
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db)
		return -EINVAL;

	/* Debug dump to confirm there are no active flows */
	ulp_flow_db_debug_dump(ulp_ctxt, 0);

	/* Detach the flow database from the ulp context. */
	bnxt_ulp_cntxt_ptr2_flow_db_set(ulp_ctxt, NULL);

	/* Free up all the memory. */
	ulp_flow_db_parent_tbl_deinit(flow_db);
	ulp_flow_db_dealloc_resource(flow_db);
	vfree(flow_db->func_id_tbl);
	vfree(flow_db);

	return 0;
}

/**
 * Allocate the flow database entry
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @flow_type: - specify default or regular
 * @func_id:.function id of the ingress port
 * @fid: The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
int
ulp_flow_db_fid_alloc(struct bnxt_ulp_context *ulp_ctxt,
		      enum bnxt_ulp_fdb_type flow_type,
		      u16 func_id,
		      u32 *fid)
{
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct bnxt_ulp_flow_db *flow_db;

	*fid = 0; /* Initialize fid to invalid value */
	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;
	/* check for max flows */
	if (flow_tbl->num_flows <= flow_tbl->head_index) {
		netdev_dbg(ulp_ctxt->bp->dev, "Flow database has reached max flows\n");
		return -ENOSPC;
	}

	if (flow_tbl->tail_index <= (flow_tbl->head_index + 1)) {
		netdev_dbg(ulp_ctxt->bp->dev, "Flow database has reached max resources\n");
		return -ENOSPC;
	}
	*fid = flow_tbl->flow_tbl_stack[flow_tbl->head_index];
	flow_tbl->head_index++;

	/* Set the flow type */
	ulp_flow_db_active_flows_bit_set(flow_db, flow_type, *fid, 1);

	/* function id update is only valid for regular flow table */
	if (flow_type == BNXT_ULP_FDB_TYPE_REGULAR)
		ulp_flow_db_func_id_set(ulp_ctxt, flow_db, *fid, func_id);

	netdev_dbg(ulp_ctxt->bp->dev, "flow_id = %u:%u allocated\n", flow_type, *fid);
	/* return success */
	return 0;
}

/**
 * Allocate the flow database entry.
 * The params->critical_resource has to be set to 0 to allocate a new resource.
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @flow_type: Specify it is regular or default flow
 * @fid: The index to the flow entry
 * @params: The contents to be copied into resource
 *
 * returns 0 on success and negative on failure.
 */
int
ulp_flow_db_resource_add(struct bnxt_ulp_context *ulp_ctxt,
			 enum bnxt_ulp_fdb_type flow_type,
			 u32 fid,
			 struct ulp_flow_db_res_params *params)
{
	struct ulp_fdb_resource_info *resource, *fid_resource;
	struct bnxt_ulp_fc_info *ulp_fc_info;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct bnxt_ulp_flow_db *flow_db;
	u32 dev_id;
	u32 idx;

	if (bnxt_ulp_cntxt_dev_id_get(ulp_ctxt, &dev_id)) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid device id\n");
		return -EINVAL;
	}

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;
	/* check for max flows */
	if (fid >= flow_tbl->num_flows || !fid) {
		netdev_dbg(ulp_ctxt->bp->dev,
			   "Invalid flow index fid %d num_flows %d\n",
			   fid, flow_tbl->num_flows);
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type, fid)) {
		netdev_dbg(ulp_ctxt->bp->dev, "flow does not exist %x:%x\n", flow_type, fid);
		return -EINVAL;
	}

	/* check for max resource */
	if ((flow_tbl->head_index + 1) >= flow_tbl->tail_index) {
		netdev_dbg(ulp_ctxt->bp->dev, "Flow db has reached max resources\n");
		return -ENOSPC;
	}
	fid_resource = &flow_tbl->flow_resources[fid];

	if (params->critical_resource && fid_resource->resource_em_handle) {
		netdev_dbg(ulp_ctxt->bp->dev, "Ignore multiple critical resources\n");
		/* Ignore the multiple critical resources */
		params->critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;
	}

	if (!params->critical_resource) {
		/* Not the critical_resource so allocate a resource */
		idx = flow_tbl->flow_tbl_stack[flow_tbl->tail_index];
		resource = &flow_tbl->flow_resources[idx];
		flow_tbl->tail_index--;

		/* Update the chain list of resource*/
		ULP_FLOW_DB_RES_NXT_SET(resource->nxt_resource_idx,
					fid_resource->nxt_resource_idx);
		/* update the contents */
		ulp_flow_db_res_params_to_info(resource, params);
		ULP_FLOW_DB_RES_NXT_RESET(fid_resource->nxt_resource_idx);
		ULP_FLOW_DB_RES_NXT_SET(fid_resource->nxt_resource_idx,
					idx);
	} else {
		/* critical resource. Just update the fid resource */
		ulp_flow_db_res_params_to_info(fid_resource, params);
	}

	if (params->resource_type == CFA_RSUBTYPE_CMM_ACT &&
	    params->resource_sub_type ==
	    BNXT_ULP_RESOURCE_SUB_TYPE_CMM_TABLE_ACT &&
	    params->resource_func == BNXT_ULP_RESOURCE_FUNC_CMM_STAT) {
		if (!ulp_sc_mgr_thread_isstarted(ulp_ctxt))
			ulp_sc_mgr_thread_start(ulp_ctxt);
	} else if (params->resource_type == TF_TBL_TYPE_ACT_STATS_64 &&
		   params->resource_sub_type ==
		   BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT) {
		ulp_fc_info = bnxt_ulp_cntxt_ptr2_fc_info_get(ulp_ctxt);

		if (ulp_fc_info && ulp_fc_info->num_counters) {
			/* Store the first HW counter ID for this table */
			if (!ulp_fc_mgr_start_idx_isset(ulp_ctxt, params->direction))
				ulp_fc_mgr_start_idx_set(ulp_ctxt, params->direction,
							 params->resource_hndl);

			ulp_fc_mgr_cntr_set(ulp_ctxt, params->direction,
					    params->resource_hndl,
					    ulp_flow_db_shared_session_get(params));

			if (!ulp_fc_mgr_thread_isstarted(ulp_ctxt))
				ulp_fc_mgr_thread_start(ulp_ctxt);
		}
	}

	/* all good, return success */
	return 0;
}

/**
 * Free the flow database entry.
 * The params->critical_resource has to be set to 1 to free the first resource.
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @flow_type: Specify it is regular or default flow
 * @fid: The index to the flow entry
 * @params: The contents to be copied into params.
 * Onlythe critical_resource needs to be set by the caller.
 *
 * Returns 0 on success and negative on failure.
 */
int
ulp_flow_db_resource_del(struct bnxt_ulp_context *ulp_ctxt,
			 enum bnxt_ulp_fdb_type flow_type,
			 u32 fid,
			 struct ulp_flow_db_res_params *params)
{
	struct ulp_fdb_resource_info *nxt_resource, *fid_resource;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct bnxt_ulp_flow_db *flow_db;
	u32 nxt_idx = 0;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;
	/* check for max flows */
	if (fid >= flow_tbl->num_flows || !fid) {
		netdev_dbg(ulp_ctxt->bp->dev,
			   "Invalid flow index fid %d num_flows %d\n",
			   fid, flow_tbl->num_flows);
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type, fid)) {
		netdev_dbg(ulp_ctxt->bp->dev, "flow does not exist %x:%x\n", flow_type, fid);
		return -EINVAL;
	}

	fid_resource = &flow_tbl->flow_resources[fid];
	if (!params->critical_resource) {
		/* Not the critical resource so free the resource */
		ULP_FLOW_DB_RES_NXT_SET(nxt_idx,
					fid_resource->nxt_resource_idx);
		if (!nxt_idx) {
			/* reached end of resources */
			return -ENOENT;
		}
		nxt_resource = &flow_tbl->flow_resources[nxt_idx];

		/* connect the fid resource to the next resource */
		ULP_FLOW_DB_RES_NXT_RESET(fid_resource->nxt_resource_idx);
		ULP_FLOW_DB_RES_NXT_SET(fid_resource->nxt_resource_idx,
					nxt_resource->nxt_resource_idx);

		/* update the contents to be given to caller */
		ulp_flow_db_res_info_to_params(nxt_resource, params);

		/* Delete the nxt_resource */
		memset(nxt_resource, 0, sizeof(struct ulp_fdb_resource_info));

		/* add it to the free list */
		flow_tbl->tail_index++;
		if (flow_tbl->tail_index >= flow_tbl->num_resources) {
			netdev_dbg(ulp_ctxt->bp->dev, "FlowDB:Tail reached max\n");
			return -ENOENT;
		}
		flow_tbl->flow_tbl_stack[flow_tbl->tail_index] = nxt_idx;

	} else {
		/* Critical resource. copy the contents and exit */
		ulp_flow_db_res_info_to_params(fid_resource, params);
		ULP_FLOW_DB_RES_NXT_SET(nxt_idx,
					fid_resource->nxt_resource_idx);
		memset(fid_resource, 0, sizeof(struct ulp_fdb_resource_info));
		ULP_FLOW_DB_RES_NXT_SET(fid_resource->nxt_resource_idx,
					nxt_idx);
	}

	/* Now that the HW Flow counter resource is deleted, reset it's
	 * corresponding slot in the SW accumulation table in the Flow Counter
	 * manager
	 */
	if (params->resource_type == TF_TBL_TYPE_ACT_STATS_64 &&
	    params->resource_sub_type ==
	    BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT) {
		ulp_fc_mgr_cntr_reset(ulp_ctxt, params->direction,
				      params->resource_hndl);
	}

	/* all good, return success */
	return 0;
}

/**
 * Free the flow database entry
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @flow_type: - specify default or regular
 * @fid: The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
int
ulp_flow_db_fid_free(struct bnxt_ulp_context *ulp_ctxt,
		     enum bnxt_ulp_fdb_type flow_type,
		     u32 fid)
{
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;

	/* check for limits of fid */
	if (fid >= flow_tbl->num_flows || !fid) {
		netdev_dbg(ulp_ctxt->bp->dev,
			   "Invalid flow index fid %d num_flows %d\n",
			   fid, flow_tbl->num_flows);
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type, fid)) {
		netdev_dbg(ulp_ctxt->bp->dev, "flow does not exist %x:%x\n", flow_type, fid);
		return -EINVAL;
	}
	flow_tbl->head_index--;
	if (!flow_tbl->head_index) {
		netdev_dbg(ulp_ctxt->bp->dev, "FlowDB: Head Ptr is zero\n");
		return -ENOENT;
	}

	flow_tbl->flow_tbl_stack[flow_tbl->head_index] = fid;

	/* Clear the flows bitmap */
	ulp_flow_db_active_flows_bit_set(flow_db, flow_type, fid, 0);

	if (flow_type == BNXT_ULP_FDB_TYPE_REGULAR)
		ulp_flow_db_func_id_set(ulp_ctxt, flow_db, fid, 0);

	netdev_dbg(ulp_ctxt->bp->dev, "flow_id = %u:%u freed\n", flow_type, fid);
	/* all good, return success */
	return 0;
}

/**
 *Get the flow database entry details
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @flow_type: - specify default or regular
 * @fid: The index to the flow entry
 * @nxt_idx: the index to the next entry
 * @params: The contents to be copied into params.
 *
 * returns 0 on success and negative on failure.
 */
int
ulp_flow_db_resource_get(struct bnxt_ulp_context *ulp_ctxt,
			 enum bnxt_ulp_fdb_type flow_type,
			 u32 fid,
			 u32 *nxt_idx,
			 struct ulp_flow_db_res_params *params)
{
	struct ulp_fdb_resource_info *nxt_resource, *fid_resource;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;

	/* check for limits of fid */
	if (fid >= flow_tbl->num_flows || !fid) {
		netdev_dbg(ulp_ctxt->bp->dev,
			   "Invalid flow index fid %d num_flows %d\n",
			   fid, flow_tbl->num_flows);
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type, fid)) {
		netdev_dbg(ulp_ctxt->bp->dev, "flow does not exist\n");
		return -EINVAL;
	}

	if (!*nxt_idx) {
		fid_resource = &flow_tbl->flow_resources[fid];
		ulp_flow_db_res_info_to_params(fid_resource, params);
		ULP_FLOW_DB_RES_NXT_SET(*nxt_idx,
					fid_resource->nxt_resource_idx);
	} else {
		nxt_resource = &flow_tbl->flow_resources[*nxt_idx];
		ulp_flow_db_res_info_to_params(nxt_resource, params);
		*nxt_idx = 0;
		ULP_FLOW_DB_RES_NXT_SET(*nxt_idx,
					nxt_resource->nxt_resource_idx);
	}

	/* all good, return success */
	return 0;
}

/**
 * Get the flow database entry iteratively
 *
 * @flow_tbl: Ptr to flow table
 * @flow_type: - specify default or regular
 * @fid: The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
int
ulp_flow_db_next_entry_get(struct bnxt_ulp_context *ulp_ctxt,
			   struct bnxt_ulp_flow_db *flow_db,
			   enum bnxt_ulp_fdb_type flow_type,
			   u32 *fid)
{
	struct bnxt_ulp_flow_tbl *flowtbl = &flow_db->flow_tbl;
	u32 idx, s_idx, mod_fid;
	u64 *active_flows;
	u32 lfid = *fid;
	u64 bs;

	if (flow_type == BNXT_ULP_FDB_TYPE_REGULAR)
		active_flows = flowtbl->active_reg_flows;
	else if (flow_type == BNXT_ULP_FDB_TYPE_DEFAULT)
		active_flows = flowtbl->active_dflt_flows;
	else
		return -EINVAL;

	do {
		/* increment the flow id to find the next valid flow id */
		lfid++;
		if (lfid >= flowtbl->num_flows)
			return -ENOENT;
		idx = lfid / ULP_INDEX_BITMAP_SIZE;
		mod_fid = lfid % ULP_INDEX_BITMAP_SIZE;
		s_idx = idx;
		while (!(bs = active_flows[idx])) {
			idx++;
			if ((idx * ULP_INDEX_BITMAP_SIZE) >= flowtbl->num_flows)
				return -ENOENT;
		}
		/* remove the previous bits in the bitset bs to find the
		 * next non zero bit in the bitset. This needs to be done
		 * only if the idx is same as he one you started.
		 */
		if (s_idx == idx)
			bs &= (-1UL >> mod_fid);
		lfid = (idx * ULP_INDEX_BITMAP_SIZE) + __builtin_clzl(bs);
		if (*fid >= lfid)
			return -ENOENT;
	} while (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type,
						      lfid));

	/* all good, return success */
	*fid = lfid;
	return 0;
}

/**
 * Flush all flows in the flow database.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @flow_type: - specify default or regular
 *
 * returns 0 on success or negative number on failure
 */
int
ulp_flow_db_flush_flows(struct bnxt_ulp_context *ulp_ctx,
			enum bnxt_ulp_fdb_type flow_type)
{
	struct bnxt_ulp_flow_db *flow_db;
	u32 fid = 0;

	if (!ulp_ctx)
		return -EINVAL;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctx);
	if (!flow_db) {
		netdev_dbg(ulp_ctx->bp->dev, "Flow database not found\n");
		return -EINVAL;
	}

	mutex_lock(&ulp_ctx->cfg_data->flow_db_lock);
	while (!ulp_flow_db_next_entry_get(ulp_ctx, flow_db, flow_type, &fid))
		ulp_mapper_resources_free(ulp_ctx, flow_type, fid, NULL);
	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);

	return 0;
}

/**
 * Flush all flows in the flow database that belong to a device function.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @func_id: - The port function id
 *
 * returns 0 on success or negative number on failure
 */
int
ulp_flow_db_function_flow_flush(struct bnxt_ulp_context *ulp_ctx,
				u16 func_id)
{
	struct bnxt_ulp_flow_db *flow_db;
	u32 flow_id = 0;

	if (!ulp_ctx || !func_id)
		return -EINVAL;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctx);
	if (!flow_db) {
		netdev_dbg(ulp_ctx->bp->dev, "Flow database not found\n");
		return -EINVAL;
	}

	mutex_lock(&ulp_ctx->cfg_data->flow_db_lock);
	while (!ulp_flow_db_next_entry_get(ulp_ctx, flow_db, BNXT_ULP_FDB_TYPE_REGULAR, &flow_id)) {
		if (flow_db->func_id_tbl[flow_id] == func_id)
			ulp_mapper_resources_free(ulp_ctx,
						  BNXT_ULP_FDB_TYPE_REGULAR,
						  flow_id, NULL);
	}
	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);

	return 0;
}

/**
 * Flush all flows in the flow database that are associated with the session.
 *
 * @ulp_ctxt: Ptr to ulp context
 *
 * returns 0 on success or negative number on failure
 */
int
ulp_flow_db_session_flow_flush(struct bnxt_ulp_context *ulp_ctx)
{
	/* TBD: Tf core implementation of FW session flush shall change this
	 * implementation.
	 */
	return ulp_flow_db_flush_flows(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR);
}

/**
 * Check that flow id matches the function id or not
 *
 * @ulp_ctxt: Ptr to ulp context
 * @flow_db: Ptr to flow table
 * @func_id: The func_id to be set, for reset pass zero.
 *
 * returns 0 on success else errors
 */
int
ulp_flow_db_validate_flow_func(struct bnxt_ulp_context *ulp_ctxt,
			       u32 flow_id,
			       u32 func_id)
{
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Flow database not found\n");
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, BNXT_ULP_FDB_TYPE_REGULAR, flow_id)) {
		netdev_dbg(ulp_ctxt->bp->dev, "Flow does not exist %x:%x\n",
			   BNXT_ULP_FDB_TYPE_REGULAR, flow_id);
		return -ENOENT;
	}

	/* check the function id in the function table */
	if (flow_id < flow_db->func_id_tbl_size && func_id &&
	    flow_db->func_id_tbl[flow_id] != func_id) {
		netdev_dbg(ulp_ctxt->bp->dev,
			   "Function id %x does not own flow %x:%x\n",
			   func_id, BNXT_ULP_FDB_TYPE_REGULAR, flow_id);
		return -EINVAL;
	}

	return 0;
}

/**
 * Internal api to traverse the resource list within a flow
 * and match a resource based on resource func and resource
 * sub type. This api should be used only for resources that
 * are unique and do not have multiple instances of resource
 * func and sub type combination since it will return only
 * the first match.
 */
static int
ulp_flow_db_resource_params_get(struct bnxt_ulp_context *ulp_ctxt,
				enum bnxt_ulp_fdb_type flow_type,
				u32 flow_id,
				u32 resource_func,
				u32 res_subtype,
				struct ulp_flow_db_res_params *params)
{
	struct ulp_fdb_resource_info *fid_res;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct bnxt_ulp_flow_db *flow_db;
	u32 res_id;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Flow database not found\n");
		return -EINVAL;
	}

	if (!params) {
		netdev_dbg(ulp_ctxt->bp->dev, "invalid argument\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;

	/* check for limits of fid */
	if (flow_id >= flow_tbl->num_flows || !flow_id) {
		netdev_dbg(ulp_ctxt->bp->dev,
			   "Invalid flow index fid %d num_flows %d\n",
			   flow_id, flow_tbl->num_flows);
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type, flow_id)) {
		netdev_dbg(ulp_ctxt->bp->dev, "flow does not exist\n");
		return -EINVAL;
	}
	/* Iterate the resource to get the resource handle */
	res_id =  flow_id;
	memset(params, 0, sizeof(struct ulp_flow_db_res_params));
	while (res_id) {
		fid_res = &flow_tbl->flow_resources[res_id];
		if (ulp_flow_db_resource_func_get(fid_res) == resource_func) {
			if (resource_func & ULP_FLOW_DB_RES_FUNC_NEED_LOWER) {
				if (res_subtype == fid_res->resource_sub_type) {
					ulp_flow_db_res_info_to_params(fid_res,
								       params);
					return 0;
				}

			} else if (resource_func ==
				   BNXT_ULP_RESOURCE_FUNC_EM_TABLE ||
				   resource_func ==
				   BNXT_ULP_RESOURCE_FUNC_CMM_TABLE ||
				   resource_func ==
				   BNXT_ULP_RESOURCE_FUNC_CMM_STAT) {
				ulp_flow_db_res_info_to_params(fid_res,
							       params);
				return 0;
			}
		}
		res_id = 0;
		ULP_FLOW_DB_RES_NXT_SET(res_id, fid_res->nxt_resource_idx);
	}
	return -ENOENT;
}

/**
 * Api to get the cfa action pointer from a flow.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @flow_id: flow id
 * @cfa_action: The resource handle stored in the flow database
 *
 * returns 0 on success
 */
int
ulp_default_flow_db_cfa_action_get(struct bnxt_ulp_context *ulp_ctxt,
				   u32 flow_id,
				   u32 *cfa_action)
{
	u8 sub_typ = BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_VFR_CFA_ACTION;
	struct ulp_flow_db_res_params params;
	int rc;

	rc = ulp_flow_db_resource_params_get(ulp_ctxt,
					     BNXT_ULP_FDB_TYPE_DEFAULT,
					     flow_id,
					     BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
					     sub_typ, &params);
	if (rc) {
		netdev_dbg(ulp_ctxt->bp->dev,
			   "CFA Action ptr not found for flow id %u\n",
			   flow_id);
		return -ENOENT;
	}
	*cfa_action = params.resource_hndl;
	return 0;
}

/* internal validation function for parent flow tbl */
static struct ulp_fdb_parent_info *
ulp_flow_db_pc_db_entry_get(struct bnxt_ulp_context *ulp_ctxt,
			    u32 pc_idx)
{
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return NULL;
	}

	/* check for max flows */
	if (pc_idx >= BNXT_ULP_MAX_TUN_CACHE_ENTRIES) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid tunnel index\n");
		return NULL;
	}

	/* No support for parent child db then just exit */
	if (!flow_db->parent_child_db.entries_count) {
		netdev_dbg(ulp_ctxt->bp->dev, "parent child db not supported\n");
		return NULL;
	}
	if (!flow_db->parent_child_db.parent_flow_tbl[pc_idx].valid) {
		netdev_dbg(ulp_ctxt->bp->dev, "Not a valid tunnel index\n");
		return NULL;
	}

	return &flow_db->parent_child_db.parent_flow_tbl[pc_idx];
}

/* internal validation function for parent flow tbl */
static struct bnxt_ulp_flow_db *
ulp_flow_db_parent_arg_validation(struct bnxt_ulp_context *ulp_ctxt,
				  u32 tun_idx)
{
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return NULL;
	}

	/* check for max flows */
	if (tun_idx >= BNXT_ULP_MAX_TUN_CACHE_ENTRIES) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid tunnel index\n");
		return NULL;
	}

	/* No support for parent child db then just exit */
	if (!flow_db->parent_child_db.entries_count) {
		netdev_dbg(ulp_ctxt->bp->dev, "parent child db not supported\n");
		return NULL;
	}

	return flow_db;
}

/**
 * Allocate the entry in the parent-child database
 *
 * @ulp_ctxt: [in] Ptr to ulp_context
 * @tun_idx: [in] The tunnel index of the flow entry
 *
 * returns index on success and negative on failure.
 */
static int
ulp_flow_db_pc_db_idx_alloc(struct bnxt_ulp_context *ulp_ctxt,
			    u32 tun_idx)
{
	struct ulp_fdb_parent_child_db *p_pdb;
	struct bnxt_ulp_flow_db *flow_db;
	u32 idx, free_idx = 0;

	/* validate the arguments */
	flow_db = ulp_flow_db_parent_arg_validation(ulp_ctxt, tun_idx);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "parent child db validation failed\n");
		return -EINVAL;
	}

	p_pdb = &flow_db->parent_child_db;
	for (idx = 0; idx < p_pdb->entries_count; idx++) {
		if (p_pdb->parent_flow_tbl[idx].valid &&
		    p_pdb->parent_flow_tbl[idx].tun_idx == tun_idx) {
			return idx;
		}
		if (!p_pdb->parent_flow_tbl[idx].valid && !free_idx)
			free_idx = idx + 1;
	}
	/* no free slots */
	if (!free_idx) {
		netdev_dbg(ulp_ctxt->bp->dev, "parent child db is full\n");
		return -ENOMEM;
	}

	free_idx -= 1;
	/* set the Fid in the parent child */
	p_pdb->parent_flow_tbl[free_idx].tun_idx = tun_idx;
	p_pdb->parent_flow_tbl[free_idx].valid = 1;
	return free_idx;
}

/**
 * Free the entry in the parent-child database
 *
 * @pc_entry: [in] Ptr to parent child db entry
 *
 * returns none.
 */
static void
ulp_flow_db_pc_db_entry_free(struct bnxt_ulp_context *ulp_ctxt,
			     struct ulp_fdb_parent_info *pc_entry)
{
	struct bnxt_ulp_flow_db *flow_db;
	u64 *tmp_bitset;

	/* free the child bitset*/
	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (flow_db)
		memset(pc_entry->child_fid_bitset, 0,
		       flow_db->parent_child_db.child_bitset_size);

	/* free the contents */
	tmp_bitset = pc_entry->child_fid_bitset;
	memset(pc_entry, 0, sizeof(struct ulp_fdb_parent_info));
	pc_entry->child_fid_bitset = tmp_bitset;
}

/**
 * Set or reset the parent flow in the parent-child database
 *
 * @ulp_ctxt: [in] Ptr to ulp_context
 * @pc_idx: [in] The index to parent child db
 * @parent_fid: [in] The flow id of the parent flow entry
 * @set_flag: [in] Use 1 for setting child, 0 to reset
 *
 * returns zero on success and negative on failure.
 */
int
ulp_flow_db_pc_db_parent_flow_set(struct bnxt_ulp_context *ulp_ctxt,
				  u32 pc_idx,
				  u32 parent_fid,
				  u32 set_flag)
{
	struct ulp_fdb_parent_info *pc_entry;
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "parent child db validation failed\n");
		return -EINVAL;
	}

	/* check for fid validity */
	if (parent_fid >= flow_db->flow_tbl.num_flows || !parent_fid) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid parent flow index %x\n", parent_fid);
		return -EINVAL;
	}

	/* validate the arguments and parent child entry */
	pc_entry = ulp_flow_db_pc_db_entry_get(ulp_ctxt, pc_idx);
	if (!pc_entry) {
		netdev_dbg(ulp_ctxt->bp->dev, "failed to get the parent child entry\n");
		return -EINVAL;
	}

	if (set_flag) {
		pc_entry->parent_fid = parent_fid;
	} else {
		if (pc_entry->parent_fid != parent_fid)
			netdev_dbg(ulp_ctxt->bp->dev, "Panic: invalid parent id\n");
		pc_entry->parent_fid = 0;

		/* Free the parent child db entry if no user present */
		if (!pc_entry->f2_cnt)
			ulp_flow_db_pc_db_entry_free(ulp_ctxt, pc_entry);
	}
	return 0;
}

/**
 * Set or reset the child flow in the parent-child database
 *
 * @ulp_ctxt: [in] Ptr to ulp_context
 * @pc_idx: [in] The index to parent child db
 * @child_fid: [in] The flow id of the child flow entry
 * @set_flag: [in] Use 1 for setting child, 0 to reset
 *
 * returns zero on success and negative on failure.
 */
int
ulp_flow_db_pc_db_child_flow_set(struct bnxt_ulp_context *ulp_ctxt,
				 u32 pc_idx,
				 u32 child_fid,
				 u32 set_flag)
{
	struct ulp_fdb_parent_info *pc_entry;
	struct bnxt_ulp_flow_db *flow_db;
	struct bnxt *bp = ulp_ctxt->bp;
	u32 a_idx;
	u64 *t;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(bp->dev, "parent child db validation failed\n");
		return -EINVAL;
	}

	/* check for fid validity */
	if (child_fid >= flow_db->flow_tbl.num_flows || !child_fid) {
		netdev_dbg(bp->dev, "Invalid child flow index %x\n", child_fid);
		return -EINVAL;
	}

	/* validate the arguments and parent child entry */
	pc_entry = ulp_flow_db_pc_db_entry_get(ulp_ctxt, pc_idx);
	if (!pc_entry) {
		netdev_dbg(bp->dev, "failed to get the parent child entry\n");
		return -EINVAL;
	}

	a_idx = child_fid / ULP_INDEX_BITMAP_SIZE;
	t = pc_entry->child_fid_bitset;
	if (set_flag) {
		ULP_INDEX_BITMAP_SET(t[a_idx], child_fid);
		pc_entry->f2_cnt++;
	} else {
		ULP_INDEX_BITMAP_RESET(t[a_idx], child_fid);
		if (pc_entry->f2_cnt)
			pc_entry->f2_cnt--;
		if (!pc_entry->f2_cnt && !pc_entry->parent_fid)
			ulp_flow_db_pc_db_entry_free(ulp_ctxt, pc_entry);
	}
	return 0;
}

/**
 * Get the next child flow in the parent-child database
 *
 * @ulp_ctxt: [in] Ptr to ulp_context
 * @parent_fid: [in] The flow id of the parent flow entry
 * @child_fid: [in/out] The flow id of the child flow entry
 *
 * returns zero on success and negative on failure.
 * Pass child_fid as zero for first entry.
 */
int
ulp_flow_db_parent_child_flow_next_entry_get(struct bnxt_ulp_flow_db *flow_db,
					     u32 parent_idx,
					     u32 *child_fid)
{
	struct ulp_fdb_parent_child_db *p_pdb;
	u32 next_fid = *child_fid;
	u32 idx, s_idx, mod_fid;
	u64 *child_bitset;
	u64 bs;

	/* check for fid validity */
	p_pdb = &flow_db->parent_child_db;
	if (parent_idx >= p_pdb->entries_count ||
	    !p_pdb->parent_flow_tbl[parent_idx].parent_fid)
		return -EINVAL;

	child_bitset = p_pdb->parent_flow_tbl[parent_idx].child_fid_bitset;
	do {
		/* increment the flow id to find the next valid flow id */
		next_fid++;
		if (next_fid >= flow_db->flow_tbl.num_flows)
			return -ENOENT;
		idx = next_fid / ULP_INDEX_BITMAP_SIZE;
		mod_fid = next_fid % ULP_INDEX_BITMAP_SIZE;
		s_idx = idx;
		while (!(bs = child_bitset[idx])) {
			idx++;
			if ((idx * ULP_INDEX_BITMAP_SIZE) >=
			    flow_db->flow_tbl.num_flows)
				return -ENOENT;
		}
		/*
		 * remove the previous bits in the bitset bs to find the
		 * next non zero bit in the bitset. This needs to be done
		 * only if the idx is same as he one you started.
		 */
		if (s_idx == idx)
			bs &= (-1UL >> mod_fid);
		next_fid = (idx * ULP_INDEX_BITMAP_SIZE) + __builtin_clzl(bs);
		if (*child_fid >= next_fid) {
			netdev_dbg(NULL, "Parent Child Database is corrupt\n");
			return -ENOENT;
		}
		idx = next_fid / ULP_INDEX_BITMAP_SIZE;
	} while (!ULP_INDEX_BITMAP_GET(child_bitset[idx], next_fid));
	*child_fid = next_fid;
	return 0;
}

/**
 * Set the counter accumulation in the parent flow
 *
 * @ulp_ctxt: [in] Ptr to ulp_context
 * @pc_idx: [in] The parent child index of the parent flow entry
 *
 * returns index on success and negative on failure.
 */
static int
ulp_flow_db_parent_flow_count_accum_set(struct bnxt_ulp_context *ulp_ctxt,
					u32 pc_idx)
{
	struct ulp_fdb_parent_child_db *p_pdb;
	struct bnxt_ulp_flow_db *flow_db;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	/* check for parent idx validity */
	p_pdb = &flow_db->parent_child_db;
	if (pc_idx >= p_pdb->entries_count ||
	    !p_pdb->parent_flow_tbl[pc_idx].parent_fid) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid parent child index %x\n", pc_idx);
		return -EINVAL;
	}

	p_pdb->parent_flow_tbl[pc_idx].counter_acc = 1;
	return 0;
}

/**
 * Orphan the child flow entry
 * This is called only for child flows that have
 * BNXT_ULP_RESOURCE_FUNC_CHILD_FLOW resource
 *
 * @ulp_ctxt: [in] Ptr to ulp_context
 * @flow_type: [in] Specify it is regular or default flow
 * @fid: [in] The index to the flow entry
 *
 * Returns 0 on success and negative on failure.
 */
int
ulp_flow_db_child_flow_reset(struct bnxt_ulp_context *ulp_ctxt,
			     enum bnxt_ulp_fdb_type flow_type,
			     u32 fid)
{
	struct ulp_fdb_resource_info *fid_res;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct bnxt_ulp_flow_db *flow_db;
	u32 res_id = 0;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (flow_type >= BNXT_ULP_FDB_TYPE_LAST) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid flow type\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;
	/* check for max flows */
	if (fid >= flow_tbl->num_flows || !fid) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid flow index %x\n", fid);
		return -EINVAL;
	}

	/* check if the flow is active or not */
	if (!ulp_flow_db_active_flows_bit_is_set(flow_db, flow_type, fid)) {
		netdev_dbg(ulp_ctxt->bp->dev, "flow does not exist\n");
		return -EINVAL;
	}

	/* Iterate the resource to get the resource handle */
	res_id =  fid;
	while (res_id) {
		fid_res = &flow_tbl->flow_resources[res_id];
		if (ulp_flow_db_resource_func_get(fid_res) ==
		    BNXT_ULP_RESOURCE_FUNC_CHILD_FLOW) {
			/* invalidate the resource details */
			fid_res->resource_hndl = 0;
			return 0;
		}
		res_id = 0;
		ULP_FLOW_DB_RES_NXT_SET(res_id, fid_res->nxt_resource_idx);
	}
	/* failed */
	return -1;
}

/**
 * Create parent flow in the parent flow tbl
 *
 * @parms: [in] Ptr to mapper params
 *
 * Returns 0 on success and negative on failure.
 */
int
ulp_flow_db_parent_flow_create(struct bnxt_ulp_mapper_parms *parms)
{
	u32 sub_typ = BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT;
	struct ulp_flow_db_res_params res_params;
	struct ulp_flow_db_res_params fid_parms;

	int pc_idx;

	/* create or get the parent child database */
	pc_idx = ulp_flow_db_pc_db_idx_alloc(parms->ulp_ctx, parms->tun_idx);
	if (pc_idx < 0) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Error in getting parent child db %x\n",
			   parms->tun_idx);
		return -EINVAL;
	}

	/* Update the parent fid */
	if (ulp_flow_db_pc_db_parent_flow_set(parms->ulp_ctx, pc_idx,
					      parms->flow_id, 1)) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Error in setting parent fid %x\n",
			   parms->tun_idx);
		return -EINVAL;
	}

	/* Add the parent details in the resource list of the flow */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.resource_func	= BNXT_ULP_RESOURCE_FUNC_PARENT_FLOW;
	fid_parms.resource_hndl	= pc_idx;
	fid_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;
	if (ulp_flow_db_resource_add(parms->ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR,
				     parms->flow_id, &fid_parms)) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Error in adding flow res for flow id %x\n",
			   parms->flow_id);
		return -1;
	}

	/* check of the flow has internal counter accumulation enabled */
	if (!ulp_flow_db_resource_params_get(parms->ulp_ctx,
					     BNXT_ULP_FDB_TYPE_REGULAR,
					     parms->flow_id,
					     BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
					     sub_typ,
					     &res_params)) {
		/* Enable the counter accumulation in parent entry */
		if (ulp_flow_db_parent_flow_count_accum_set(parms->ulp_ctx,
							    pc_idx)) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "Error in setting counter acc %x\n",
				   parms->flow_id);
			return -1;
		}
	}

	return 0;
}

/**
 * Create child flow in the parent flow tbl
 *
 * @parms: [in] Ptr to mapper params
 *
 * Returns 0 on success and negative on failure.
 */
int
ulp_flow_db_child_flow_create(struct bnxt_ulp_mapper_parms *parms)
{
	u32 sub_type = BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT;
	struct ulp_flow_db_res_params fid_parms;
	enum bnxt_ulp_resource_func res_fun;
	struct ulp_flow_db_res_params res_p;
	int rc, pc_idx;

	/* create or get the parent child database */
	pc_idx = ulp_flow_db_pc_db_idx_alloc(parms->ulp_ctx, parms->tun_idx);
	if (pc_idx < 0) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Error in getting parent child db %x\n",
			   parms->tun_idx);
		return -1;
	}

	/* create the parent flow entry in parent flow table */
	rc = ulp_flow_db_pc_db_child_flow_set(parms->ulp_ctx, pc_idx,
					      parms->flow_id, 1);
	if (rc) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Error in setting child fid %x\n",
			   parms->flow_id);
		return rc;
	}

	/* Add the parent details in the resource list of the flow */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.resource_func	= BNXT_ULP_RESOURCE_FUNC_CHILD_FLOW;
	fid_parms.resource_hndl	= pc_idx;
	fid_parms.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO;
	rc  = ulp_flow_db_resource_add(parms->ulp_ctx,
				       BNXT_ULP_FDB_TYPE_REGULAR,
				       parms->flow_id, &fid_parms);
	if (rc) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Error in adding flow res for flow id %x\n",
			   parms->flow_id);
		return rc;
	}

	/* check if internal count action included for this flow.*/
	res_fun = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE;
	rc = ulp_flow_db_resource_params_get(parms->ulp_ctx,
					     BNXT_ULP_FDB_TYPE_REGULAR,
					     parms->flow_id,
					     res_fun,
					     sub_type,
					     &res_p);
	/* return success */
	return rc;
}

/**
 * Update the parent counters
 *
 * @ulp_ctxt: [in] Ptr to ulp_context
 * @pc_idx: [in] The parent flow entry idx
 * @packet_count: [in] - packet count
 * @byte_count: [in] - byte count
 *
 * returns 0 on success
 */
int
ulp_flow_db_parent_flow_count_update(struct bnxt_ulp_context *ulp_ctxt,
				     u32 pc_idx,
				     u64 packet_count,
				     u64 byte_count)
{
	struct ulp_fdb_parent_info *pc_entry;

	/* validate the arguments and get parent child entry */
	pc_entry = ulp_flow_db_pc_db_entry_get(ulp_ctxt, pc_idx);
	if (!pc_entry) {
		netdev_dbg(ulp_ctxt->bp->dev, "failed to get the parent child entry\n");
		return -EINVAL;
	}

	if (pc_entry->counter_acc) {
		pc_entry->pkt_count += packet_count;
		pc_entry->byte_count += byte_count;
	}
	return 0;
}

/**
 * Get the parent accumulation counters
 *
 * @ulp_ctxt: [in] Ptr to ulp_context
 * @pc_idx: [in] The parent flow entry idx
 * @packet_count: [out] - packet count
 * @byte_count: [out] - byte count
 *
 * returns 0 on success
 */
int
ulp_flow_db_parent_flow_count_get(struct bnxt_ulp_context *ulp_ctxt,
				  u32 pc_idx, u64 *packet_count,
				  u64 *byte_count, u8 count_reset)
{
	struct ulp_fdb_parent_info *pc_entry;

	/* validate the arguments and get parent child entry */
	pc_entry = ulp_flow_db_pc_db_entry_get(ulp_ctxt, pc_idx);
	if (!pc_entry) {
		netdev_dbg(ulp_ctxt->bp->dev, "failed to get the parent child entry\n");
		return -EINVAL;
	}

	if (pc_entry->counter_acc) {
		*packet_count = pc_entry->pkt_count;
		*byte_count = pc_entry->byte_count;
		if (count_reset) {
			pc_entry->pkt_count = 0;
			pc_entry->byte_count = 0;
		}
	}
	return 0;
}

/**
 * reset the parent accumulation counters
 *
 * @ulp_ctxt: [in] Ptr to ulp_context
 *
 * returns none
 */
void
ulp_flow_db_parent_flow_count_reset(struct bnxt_ulp_context *ulp_ctxt)
{
	struct bnxt_ulp_flow_db *flow_db;
	struct ulp_fdb_parent_child_db *p_pdb;
	u32 idx;

	/* validate the arguments */
	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "parent child db validation failed\n");
		return;
	}

	p_pdb = &flow_db->parent_child_db;
	for (idx = 0; idx < p_pdb->entries_count; idx++) {
		if (p_pdb->parent_flow_tbl[idx].valid &&
		    p_pdb->parent_flow_tbl[idx].counter_acc) {
			p_pdb->parent_flow_tbl[idx].pkt_count = 0;
			p_pdb->parent_flow_tbl[idx].byte_count = 0;
		}
	}
}

/** Set the shared bit for the flow db entry
 * @res: Ptr to fdb entry
 * @shared: shared flag
 *
 * returns none
 */
void ulp_flow_db_shared_session_set(struct ulp_flow_db_res_params *res,
				    enum bnxt_ulp_session_type s_type)
{
	if (res && (s_type & BNXT_ULP_SESSION_TYPE_SHARED))
		res->fdb_flags |= ULP_FDB_FLAG_SHARED_SESSION;
	else if (res && (s_type & BNXT_ULP_SESSION_TYPE_SHARED_WC))
		res->fdb_flags |= ULP_FDB_FLAG_SHARED_WC_SESSION;
}

/**
 * get the shared bit for the flow db entry
 *
 * @res: [in] Ptr to fdb entry
 *
 * returns session type
 */
enum bnxt_ulp_session_type
ulp_flow_db_shared_session_get(struct ulp_flow_db_res_params *res)
{
	enum bnxt_ulp_session_type stype = BNXT_ULP_SESSION_TYPE_DEFAULT;

	if (res && (res->fdb_flags & ULP_FDB_FLAG_SHARED_SESSION))
		stype = BNXT_ULP_SESSION_TYPE_SHARED;
	else if (res && (res->fdb_flags & ULP_FDB_FLAG_SHARED_WC_SESSION))
		stype = BNXT_ULP_SESSION_TYPE_SHARED_WC;

	return stype;
}

int
ulp_flow_db_reg_fid_alloc_with_lock(struct bnxt *bp, u32 *flow_id)
{
	struct bnxt_ulp_context *ulp_ctx;
	u16 port_fid = bp->pf.fw_fid;
	u16 func_id;
	int rc;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx)
		return -EINVAL;

	if (ulp_port_db_port_func_id_get(ulp_ctx, port_fid, &func_id))
		return -EINVAL;

	mutex_lock(&ulp_ctx->cfg_data->flow_db_lock);

	rc = ulp_flow_db_fid_alloc(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR,
				   func_id, flow_id);
	if (rc)
		netdev_dbg(bp->dev, "Unable to allocate flow table entry\n");

	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);
	return rc;
}

void
ulp_flow_db_reg_fid_free_with_lock(struct bnxt *bp, u32 flow_id)
{
	struct bnxt_ulp_context *ulp_ctx;

	ulp_ctx = bnxt_ulp_bp_ptr2_cntxt_get(bp);
	if (!ulp_ctx)
		return;

	mutex_lock(&ulp_ctx->cfg_data->flow_db_lock);
	ulp_flow_db_fid_free(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR, flow_id);
	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);
}

#ifdef TC_BNXT_TRUFLOW_DEBUG

/**
 * Dump the entry details
 *
 * @ulp_ctxt: Ptr to ulp_context
 *
 * returns none
 */
static void ulp_flow_db_res_dump(struct bnxt_ulp_context *ulp_ctxt,
				 struct ulp_fdb_resource_info *r,
				 u32 *nxt_res)
{
	u8 res_func = ulp_flow_db_resource_func_get(r);

	netdev_dbg(ulp_ctxt->bp->dev, "Resource func = %x, nxt_resource_idx = %x\n",
		   res_func, (ULP_FLOW_DB_RES_NXT_MASK & r->nxt_resource_idx));
	if (res_func == BNXT_ULP_RESOURCE_FUNC_EM_TABLE ||
	    res_func == BNXT_ULP_RESOURCE_FUNC_CMM_TABLE ||
	    res_func == BNXT_ULP_RESOURCE_FUNC_CMM_STAT)
		netdev_dbg(ulp_ctxt->bp->dev, "Handle = %llu\n", r->resource_em_handle);
	else
		netdev_dbg(ulp_ctxt->bp->dev, "Handle = 0x%08x\n", r->resource_hndl);

	*nxt_res = 0;
	ULP_FLOW_DB_RES_NXT_SET(*nxt_res,
				r->nxt_resource_idx);
}

/**
 * Dump the flow entry details
 *
 * @flow_db: Ptr to flow db
 * @fid: flow id
 *
 * returns none
 */
void
ulp_flow_db_debug_fid_dump(struct bnxt_ulp_context *ulp_ctxt,
			   struct bnxt_ulp_flow_db *flow_db, u32 fid)
{
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct ulp_fdb_resource_info *r;
	u32 def_flag = 0, reg_flag = 0;
	u32 nxt_res = 0;

	flow_tbl = &flow_db->flow_tbl;
	if (ulp_flow_db_active_flows_bit_is_set(flow_db,
						BNXT_ULP_FDB_TYPE_REGULAR, fid))
		reg_flag = 1;
	if (ulp_flow_db_active_flows_bit_is_set(flow_db,
						BNXT_ULP_FDB_TYPE_DEFAULT, fid))
		def_flag = 1;

	if (reg_flag && def_flag) {
		netdev_dbg(ulp_ctxt->bp->dev, "RID = %u\n", fid);
	} else if (reg_flag) {
		netdev_dbg(ulp_ctxt->bp->dev, "Regular fid = %u and func id = %u\n",
			   fid, flow_db->func_id_tbl[fid]);
	} else if (def_flag) {
		netdev_dbg(ulp_ctxt->bp->dev, "Default fid = %u\n", fid);
	} else {
		return;
	}
	/* iterate the resource */
	nxt_res = fid;
	do {
		r = &flow_tbl->flow_resources[nxt_res];
		ulp_flow_db_res_dump(ulp_ctxt, r, &nxt_res);
	} while (nxt_res);
}

/**
 * Dump the flow database entry details
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @flow_id: if zero then all fids are dumped.
 *
 * returns none
 */
int ulp_flow_db_debug_dump(struct bnxt_ulp_context *ulp_ctxt, u32 flow_id)
{
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct bnxt_ulp_flow_db *flow_db;
	u32 fid;

	if (!ulp_ctxt || !ulp_ctxt->cfg_data)
		return -EINVAL;

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;
	if (flow_id) {
		ulp_flow_db_debug_fid_dump(ulp_ctxt, flow_db, flow_id);
		return 0;
	}

	netdev_dbg(ulp_ctxt->bp->dev, "Dump flows = %u:%u\n",
		   flow_tbl->num_flows,
		   flow_tbl->num_resources);
	netdev_dbg(ulp_ctxt->bp->dev, "Head_index = %u, Tail_index = %u\n",
		   flow_tbl->head_index, flow_tbl->tail_index);
	for (fid = 1; fid < flow_tbl->num_flows; fid++)
		ulp_flow_db_debug_fid_dump(ulp_ctxt, flow_db, fid);
	netdev_dbg(ulp_ctxt->bp->dev, "Done.\n");
	return 0;
}

#else /* TC_BNXT_TRUFLOW_DEBUG */

void ulp_flow_db_debug_fid_dump(struct bnxt_ulp_context *ulp_ctxt,
				struct bnxt_ulp_flow_db *flow_db, u32 fid)
{
}

int ulp_flow_db_debug_dump(struct bnxt_ulp_context *ulp_ctxt, u32 flow_id)
{
	ulp_flow_db_debug_fid_dump(ulp_ctxt, NULL, 0);
	return 0;
}

#endif	/* TC_BNXT_TRUFLOW_DEBUG */
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
