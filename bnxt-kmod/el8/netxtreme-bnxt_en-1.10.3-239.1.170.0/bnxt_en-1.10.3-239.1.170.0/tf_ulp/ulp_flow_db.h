/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_FLOW_DB_H_
#define _ULP_FLOW_DB_H_

#include "bnxt_tf_ulp.h"
#include "ulp_template_db_enum.h"
#include "ulp_mapper.h"

#define BNXT_FLOW_DB_DEFAULT_NUM_FLOWS		512
#define BNXT_FLOW_DB_DEFAULT_NUM_RESOURCES	8

/* Defines for the fdb flag */
#define ULP_FDB_FLAG_SHARED_SESSION	0x1
#define ULP_FDB_FLAG_SHARED_WC_SESSION	0x2

/**
 * Structure for the flow database resource information
 * The below structure is based on the below paritions
 * nxt_resource_idx = dir[31],resource_func_upper[30:28],nxt_resource_idx[27:0]
 * If resource_func is EM_TBL then use resource_em_handle.
 * Else the other part of the union is used and
 * resource_func is resource_func_upper[30:28] << 5 | resource_func_lower
 */
struct ulp_fdb_resource_info {
	/* Points to next resource in the chained list. */
	u32			nxt_resource_idx;
	/* TBD: used for tfc stat resource for now */
	u32			reserve_flag;
	union {
		u64		resource_em_handle;
		struct {
			u8		resource_func_lower;
			u8		resource_type;
			u8		resource_sub_type;
			u8		fdb_flags;
			u32		resource_hndl;
			u8		*key_data;
		};
	};
};

/* Structure for the flow database resource information. */
struct bnxt_ulp_flow_tbl {
	/* Flow tbl is the resource object list for each flow id. */
	struct ulp_fdb_resource_info	*flow_resources;

	/* Flow table stack to track free list of resources. */
	u32	*flow_tbl_stack;
	u32	head_index;
	u32	tail_index;

	/* Table to track the active flows. */
	u64	*active_reg_flows;
	u64	*active_dflt_flows;
	u32	num_flows;
	u32	num_resources;
};

/* Structure to maintain parent-child flow relationships */
struct ulp_fdb_parent_info {
	u32	valid;
	u32	parent_fid;
	u32	counter_acc;
	u64	pkt_count;
	u64	byte_count;
	u64	*child_fid_bitset;
	u32	f2_cnt;
	u8	tun_idx;
};

/* Structure to maintain parent-child flow relationships */
struct ulp_fdb_parent_child_db {
	struct ulp_fdb_parent_info	*parent_flow_tbl;
	u32				child_bitset_size;
	u32				entries_count;
	u8				*parent_flow_tbl_mem;
};

/* Structure for the flow database resource information. */
struct bnxt_ulp_flow_db {
	struct bnxt_ulp_flow_tbl	flow_tbl;
	u16				*func_id_tbl;
	u32				func_id_tbl_size;
	struct ulp_fdb_parent_child_db	parent_child_db;
};

/* flow db resource params to add resources */
struct ulp_flow_db_res_params {
	enum tf_dir			direction;
	enum bnxt_ulp_resource_func	resource_func;
	u8				resource_type;
	u8				resource_sub_type;
	u8				fdb_flags;
	u8				critical_resource;
	u8				*key_data;
	u64				resource_hndl;
	u32				reserve_flag;
};

/*
 * Initialize the flow database. Memory is allocated in this
 * call and assigned to the flow database.
 *
 * @ulp_ctxt: Ptr to ulp context
 *
 * Returns 0 on success or negative number on failure.
 */
int	ulp_flow_db_init(struct bnxt_ulp_context *ulp_ctxt);

/*
 * Deinitialize the flow database. Memory is deallocated in
 * this call and all flows should have been purged before this
 * call.
 *
 * @ulp_ctxt: Ptr to ulp context
 *
 * Returns 0 on success.
 */
int	ulp_flow_db_deinit(struct bnxt_ulp_context *ulp_ctxt);

/*
 * Allocate the flow database entry
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @tbl_idx: Specify it is regular or default flow
 * @func_id: The function id of the device.Valid only for regular flows.
 * @fid: The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
int
ulp_flow_db_fid_alloc(struct bnxt_ulp_context *ulp_ctxt,
		      enum bnxt_ulp_fdb_type flow_type,
		      u16 func_id,
		      u32 *fid);

/*
 * Allocate the flow database entry.
 * The params->critical_resource has to be set to 0 to allocate a new resource.
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @tbl_idx: Specify it is regular or default flow
 * @fid: The index to the flow entry
 * @params: The contents to be copied into resource
 *
 * returns 0 on success and negative on failure.
 */
int
ulp_flow_db_resource_add(struct bnxt_ulp_context *ulp_ctxt,
			 enum bnxt_ulp_fdb_type flow_type,
			 u32 fid,
			 struct ulp_flow_db_res_params *params);

/*
 * Free the flow database entry.
 * The params->critical_resource has to be set to 1 to free the first resource.
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @tbl_idx: Specify it is regular or default flow
 * @fid: The index to the flow entry
 * @params: The contents to be copied into params.
 * Only the critical_resource needs to be set by the caller.
 *
 * Returns 0 on success and negative on failure.
 */
int
ulp_flow_db_resource_del(struct bnxt_ulp_context *ulp_ctxt,
			 enum bnxt_ulp_fdb_type flow_type,
			 u32 fid,
			 struct ulp_flow_db_res_params *params);

/*
 * Free the flow database entry
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @tbl_idx: Specify it is regular or default flow
 * @fid: The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
int
ulp_flow_db_fid_free(struct bnxt_ulp_context *ulp_ctxt,
		     enum bnxt_ulp_fdb_type tbl_idx,
		     u32 fid);

/*
 * Get the flow database entry details
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @tbl_idx: Specify it is regular or default flow
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
			 struct ulp_flow_db_res_params *params);

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
			   u32 *fid);

/*
 * Flush all flows in the flow database.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @flow_type: - specify default or regular
 *
 * returns 0 on success or negative number on failure
 */
int
ulp_flow_db_flush_flows(struct bnxt_ulp_context *ulp_ctx,
			enum bnxt_ulp_fdb_type flow_type);

/*
 * Flush all flows in the flow database that belong to a device function.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @tbl_idx: The index to table
 *
 * returns 0 on success or negative number on failure
 */
int
ulp_flow_db_function_flow_flush(struct bnxt_ulp_context *ulp_ctx,
				u16 func_id);

/*
 * Flush all flows in the flow database that are associated with the session.
 *
 * @ulp_ctxt: Ptr to ulp context
 *
 * returns 0 on success or negative number on failure
 */
int
ulp_flow_db_session_flow_flush(struct bnxt_ulp_context *ulp_ctx);

/*
 * Check that flow id matches the function id or not
 *
 * @ulp_ctxt: Ptr to ulp context
 * @flow_id: flow id of the flow.
 * @func_id: The func_id to be set, for reset pass zero.
 *
 * returns true on success or false on failure
 */
int
ulp_flow_db_validate_flow_func(struct bnxt_ulp_context *ulp_ctx,
			       u32 flow_id,
			       u32 func_id);

/*
 * Api to get the cfa action pointer from a flow.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @flow_id: flow id
 * @cfa_action: The resource handle stored in the flow database
 *
 * returns 0 on success
 */
int
ulp_default_flow_db_cfa_action_get(struct bnxt_ulp_context *ulp_ctx,
				   u32 flow_id,
				   u32 *cfa_action);

/*
 * Set or reset the parent flow in the parent-child database
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @pc_idx: The index to parent child db
 * @parent_fid: The flow id of the parent flow entry
 * @set_flag: Use 1 for setting child, 0 to reset
 *
 * returns zero on success and negative on failure.
 */
int
ulp_flow_db_pc_db_parent_flow_set(struct bnxt_ulp_context *ulp_ctxt,
				  u32 pc_idx,
				  u32 parent_fid,
				  u32 set_flag);

/*
 * Set or reset the child flow in the parent-child database
 *
 * ulp_ctxt: Ptr to ulp_context
 * pc_idx: The index to parent child db
 * child_fid: The flow id of the child flow entry
 * set_flag: Use 1 for setting child, 0 to reset
 *
 * returns zero on success and negative on failure.
 */
int
ulp_flow_db_pc_db_child_flow_set(struct bnxt_ulp_context *ulp_ctxt,
				 u32 pc_idx,
				 u32 child_fid,
				 u32 set_flag);

/*
 * Get the parent index from the parent-child database
 *
 * @ulp_ctxt; Ptr to ulp_context
 * @parent_fid; The flow id of the parent flow entry
 * @parent_idx: The parent index of parent flow entry
 *
 * returns zero on success and negative on failure.
 */
int
ulp_flow_db_parent_flow_idx_get(struct bnxt_ulp_context *ulp_ctxt,
				u32 parent_fid,
				u32 *parent_idx);

/*
 * Get the next child flow in the parent-child database
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @parent_fid: The flow id of the parent flow entry
 * @child_fid: The flow id of the child flow entry
 *
 * returns zero on success and negative on failure.
 * Pass child_fid as zero for first entry.
 */
int
ulp_flow_db_parent_child_flow_next_entry_get(struct bnxt_ulp_flow_db *flow_db,
					     u32 parent_idx,
					     u32 *child_fid);

/*
 * Orphan the child flow entry
 * This is called only for child flows that have
 * BNXT_ULP_RESOURCE_FUNC_CHILD_FLOW resource
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @flow_type: Specify it is regular or default flow
 * @fid: The index to the flow entry
 *
 * Returns 0 on success and negative on failure.
 */
int
ulp_flow_db_child_flow_reset(struct bnxt_ulp_context *ulp_ctxt,
			     enum bnxt_ulp_fdb_type flow_type,
			     u32 fid);

/*
 * Create parent flow in the parent flow tbl
 *
 * @parms: Ptr to mapper params
 *
 * Returns 0 on success and negative on failure.
 */
int
ulp_flow_db_parent_flow_create(struct bnxt_ulp_mapper_parms *parms);

/*
 * Create child flow in the parent flow tbl
 *
 * @parms: Ptr to mapper params
 *
 * Returns 0 on success and negative on failure.
 */
int
ulp_flow_db_child_flow_create(struct bnxt_ulp_mapper_parms *parms);

/*
 * Update the parent counters
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @pc_idx: The parent flow entry idx
 * @packet_count: - packet count
 * @byte_count: - byte count
 *
 * returns 0 on success
 */
int
ulp_flow_db_parent_flow_count_update(struct bnxt_ulp_context *ulp_ctxt,
				     u32 pc_idx,
				     u64 packet_count,
				     u64 byte_count);
/*
 * Get the parent accumulation counters
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @pc_idx: The parent flow entry idx
 * @packet_count: - packet count
 * @byte_count: - byte count
 *
 * returns 0 on success
 */

int
ulp_flow_db_parent_flow_count_get(struct bnxt_ulp_context *ulp_ctxt,
				  u32 pc_idx,
				  u64 *packet_count,
				  u64 *byte_count,
				  u8 count_reset);

/*
 * reset the parent accumulation counters
 *
 * @ulp_ctxt: Ptr to ulp_context
 *
 * returns none
 */
void
ulp_flow_db_parent_flow_count_reset(struct bnxt_ulp_context *ulp_ctxt);

/*
 * Set the shared bit for the flow db entry
 *
 * @res: Ptr to fdb entry
 * @s_type: session flag
 *
 * returns none
 */
void ulp_flow_db_shared_session_set(struct ulp_flow_db_res_params *res,
				    enum bnxt_ulp_session_type s_type);

/*
 * get the shared bit for the flow db entry
 *
 * @res: Ptr to fdb entry
 *
 * returns session type
 */
enum bnxt_ulp_session_type
ulp_flow_db_shared_session_get(struct ulp_flow_db_res_params *res);

/*
 * Allocate the flow database entry
 *
 * @bp: Ptr to bnxt structure
 * @flow_id: The index to the flow entry
 *
 * returns 0 on success and negative on failure.
 */
int
ulp_flow_db_reg_fid_alloc_with_lock(struct bnxt *bp, u32 *flow_id);

/*
 * Free the flow database entry
 *
 * @bp: Ptr to bnxt structure
 * @fid: The index to the flow entry
 *
 * returns none.
 */
void
ulp_flow_db_reg_fid_free_with_lock(struct bnxt *bp, u32 flow_id);

/*
 * Dump the flow entry details
 *
 * @flow_db: Ptr to flow db
 * @fid: flow id
 *
 * returns none
 */
void
ulp_flow_db_debug_fid_dump(struct bnxt_ulp_context *ulp_ctx,
			   struct bnxt_ulp_flow_db *flow_db, u32 fid);

/*
 * Dump the flow database entry details
 *
 * @ulp_ctxt: Ptr to ulp_context
 * @flow_id: if zero then all fids are dumped.
 *
 * returns none
 */
int	ulp_flow_db_debug_dump(struct bnxt_ulp_context *ulp_ctxt,
			       u32 flow_id);

#endif /* _ULP_FLOW_DB_H_ */
