/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Broadcom
 * All rights reserved.
 *
 * TFC (Truflow Core v3) API Header File
 * API Guidance:
 *
 * 1.  If more than 5-6 parameters, please define structures
 *
 * 2.  Design structures that can be used with multiple APIs
 *
 * 3.  If items in structures are not to be used, these must
 *     be documented in the API header IN DETAIL.
 *
 * 4.  Use defines in cfa_types.h where possible. These are shared
 *     firmware types to avoid duplication. These types do
 *     not represent the HWRM interface and may need to be mapped
 *     to HWRM definitions.
 *
 * 5.  Resource types and subtypes are defined in cfa_resources.h
 */

#ifndef _TFC_H_
#define _TFC_H_

#include "cfa_resources.h"
#include "cfa_types.h"

/* TFC handle
 */
struct tfc {
	void *tfo;	/* Pointer to the private tfc object */
	void *bp;	/* the pointer to the parent bp struct */
};

/********* BEGIN API FUNCTION PROTOTYPES/PARAMETERS **********/
/**
 * Allocate the TFC state for this DPDK port/function. The TF
 * object memory is allocated during this API call.
 *
 * @tfcp: Pointer to TFC handle
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 *
 * This API will initialize only the software state.
 */
int tfc_open(struct tfc *tfcp);

/**
 * De-allocate the TFC state for this DPDK port/function. The TF
 * object memory is deallocated during this API call.
 *
 * @tfcp: Pointer to TFC handle
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 *
 * This API will reset only the software state.
 */
int tfc_close(struct tfc *tfcp);

/* The maximum number of foreseeable resource types.
 * Use cfa_resource_types enum internally.
 */
#define TFC_MAX_RESOURCE_TYPES 32

/* Supported resource information
 */
struct tfc_resources {
	u32 rtypes_mask;	/* Resource subtype mask of valid resource types */
	u8 max_rtype;		/* Maximum resource type number */
	u32 rsubtypes_mask[TFC_MAX_RESOURCE_TYPES]; /* Array indicating valid subtypes */
};

/**
 * Get all supported CFA resource types for the device
 *
 * This API goes to the firmware to query all supported resource
 * types and subtypes supported.
 *
 * @tfcp: Pointer to TFC handle
 * @resources: Pointer to a structure containing information about the supported
 *	       CFA device resources.
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_resource_types_query(struct tfc *tfcp, struct tfc_resources *resources);

/**
 * Allocate a TFC session
 *
 * This API goes to the firmware to allocate a TFC session id and associate a
 * forwarding function with the session.
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function id to associated with the session
 * @sid: Pointer to the where the session id will be Returned
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_session_id_alloc(struct tfc *tfcp, u16 fid, u16 *sid);
/**
 * This API sets the session id to a pre-existing session
 *
 * When NIC flow tracking by function, the session should be set to the
 * AFM session.
 *
 * @tfcp: Pointer to TFC handle
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_session_id_set(struct tfc *tfcp, u16 sid);

/**
 * Associate a forwarding function with an existing TFC session
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function id to associated with the session
 * @sid: The session id to associate with
 * @fid_cnt: The number of forwarding functions currently associated with the
 *	      session
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_session_fid_add(struct tfc *tfcp, u16 fid, u16 sid,
			u16 *fid_cnt);
/**
 * Disassociate a forwarding function from an existing TFC session
 *
 * Once the last function has been removed from the session in the firmware
 * the session is freed and all associated resources freed.
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function id to associated with the session
 * @fid_cnt: The number of forwarding functions currently associated with the session
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_session_fid_rem(struct tfc *tfcp, u16 fid, u16 *fid_cnt);

/* Domain id range
 */
enum tfc_domain_id {
	TFC_DOMAIN_ID_INVALID = 0,
	TFC_DOMAIN_ID_1,
	TFC_DOMAIN_ID_2,
	TFC_DOMAIN_ID_3,
	TFC_DOMAIN_ID_4,
	TFC_DOMAIN_ID_MAX = TFC_DOMAIN_ID_4
};

/* Global id request definition
 */
struct tfc_global_id_req {
	enum cfa_resource_type rtype;	/* Resource type */
	u8 rsubtype;			/* Resource subtype */
	enum cfa_dir dir;		/* Direction */
	uint8_t *context_id;
	uint16_t context_len;
	uint16_t resource_id;
};

/* Global id resource definition
 */
struct tfc_global_id {
	enum cfa_resource_type rtype;	/* Resource type */
	u8 rsubtype;			/* Resource subtype */
	enum cfa_dir dir;		/* Direction */
	u16 id;				/* Resource id */
};

/**
 * Allocate global TFC resources
 *
 * Some resources are not owned by a single session.  They are "global" in that
 * they will be in use as long as any associated session exists.  Once all
 * sessions/functions hve been removed, all associated global ids are freed.
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @glb_id_req: The global id request
 * @glb_id_rsp: The response buffer
 * @first: This is the first domain request for the indicated domain id.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_global_id_alloc(struct tfc *tfcp, u16 fid,
			const struct tfc_global_id_req *glb_id_req,
			struct tfc_global_id *glb_id_rsp, bool *first);

/**
 * Free global TFC resources
 *
 * Some resources are not owned by a single session.  They are "global" in that
 * they will be in use as long as any associated session exists.  Once all
 * sessions/functions hve been removed, all associated global ids are freed.
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @glb_id_req: The global id req
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_global_id_free(struct tfc *tfcp, uint16_t fid,
		       const struct tfc_global_id_req *req);

/* Identifier resource structure
 */
struct tfc_identifier_info {
	enum cfa_resource_subtype_ident rsubtype;	/* resource subtype */
	enum cfa_dir dir;				/* direction rx/tx */
	u16 id;						/* alloc/free index */
};

/**
 * allocate a TFC Identifier
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @tt: Track type - either track by session or by function
 * @ident_info: All the information related to the requested identifier (subtype/dir) and
 *   the Returned identifier id.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_identifier_alloc(struct tfc *tfcp, u16 fid, enum cfa_track_type tt,
			 struct tfc_identifier_info *ident_info);

/**
 * free a TFC Identifier
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @ident_info: All the information related to the requested identifier (subtype/dir) and
 *		the identifier id to free.
 *
 * Returns success or failure code.
 */
int tfc_identifier_free(struct tfc *tfcp, u16 fid,
			const struct tfc_identifier_info *ident_info);

/* Index table resource structure
 */
struct tfc_idx_tbl_info {
	enum cfa_resource_subtype_idx_tbl rsubtype;	/* resource subtype */
	enum cfa_dir dir;				/* direction rx/tx */
	u16 id;						/* alloc/free index */
	enum cfa_resource_blktype_idx_tbl blktype;	/* block type */
};

/**
 * allocate a TFC index table entry
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @tt: either track by session or by function
 * @tbl_info: All the information related to the requested index table entry
 *	      (subtype/dir) and the Returned id.
 *
 * Returns
 *	 0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_idx_tbl_alloc(struct tfc *tfcp, u16 fid, enum cfa_track_type tt,
		      struct tfc_idx_tbl_info *tbl_info);

/**
 * allocate and set a TFC index table entry
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @tt: either track by session or by function
 * @tbl_info: All the information related to the requested index table entry
 *	      (subtype/dir) and the Returned id.
 * @data: Pointer to the data to write to the entry. The data is aligned
 *	  correctly in the buffer for writing to the hardware.
 * @data_sz_in_bytes: The size of the entry in bytes for Thor2.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_idx_tbl_alloc_set(struct tfc *tfcp, u16 fid,
			  enum cfa_track_type tt,
			  struct tfc_idx_tbl_info *tbl_info,
			  const u32 *data, u8 data_sz_in_bytes);

/**
 * Set a TFC index table entry
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @tbl_info: All the information related to the requested index table entry
 *	      (subtype/dir) including the id.
 * @data: Pointer to the data to write to the entry. The data is aligned
 *	  correctly in the buffer for writing to the hardware.
 * @data_sz_in_bytes: The size of the entry in device sized bytes for Thor2.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_idx_tbl_set(struct tfc *tfcp, u16 fid,
		    const struct tfc_idx_tbl_info *tbl_info,
		    const u32 *data, u8 data_sz_in_bytes);

/**
 * Get a TFC index table entry
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @tbl_info: All the information related to the requested index table entry
 *	      (subtype/dir) including the id.
 * @data: Pointer to the data to read from the entry.
 * @data_sz_in_bytes: The size of the entry in device sized bytes for Thor2.
 *		      Input is the size of the buffer, output is the actual size.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_idx_tbl_get(struct tfc *tfcp, u16 fid,
		    const struct tfc_idx_tbl_info *tbl_info,
		    u32 *data, u8 *data_sz_in_bytes);

/**
 * Free a TFC index table entry
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @tbl_info: All the information related to the requested index table entry
 *	      (subtype/dir) and the Returned id.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_idx_tbl_free(struct tfc *tfcp, u16 fid,
		     const struct tfc_idx_tbl_info *tbl_info);

/* Tcam table info structure
 */
struct tfc_tcam_info {
	enum cfa_resource_subtype_tcam rsubtype;	/* resource subtype */
	enum cfa_dir dir;				/* direction rx/tx */
	u16 id;						/* alloc/free index */
};

/* Tcam table resource structure
 */
struct tfc_tcam_data {
	u8 *key;		/* tcam key */
	u8 *mask;		/* tcam mask */
	u8 *remap;		/* remap */
	u8 key_sz_in_bytes;	/* key size in bytes */
	u8 remap_sz_in_bytes;	/* remap size in bytes */
};

/**
 * allocate a TFC TCAM entry
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @tt: Either track by session or by function
 * @priority: the priority of the tcam entry
 * @tcam_info: All the information related to the requested index table entry
 *	       (subtype/dir) and the Returned id.
 * @key_sz_in_bytes: The size of the entry in bytes for Thor2.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tcam_alloc(struct tfc *tfcp, u16 fid, enum cfa_track_type tt,
		   u16 priority, u8 key_sz_in_bytes,
		   struct tfc_tcam_info *tcam_info);

/**
 * allocate and set a TFC TCAM entry
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @tt: either track by session or by function
 * @priority: the priority of the tcam entry
 * @tcam_info: All the information related to the requested TCAM table entry
 *	       (subtype/dir) and the Returned id.
 * @tcam_data: Pointer to the tcam data, including tcam, mask, and remap,
 *	       to write tothe entry. The data is aligned in the buffer for
 *	       writing to the hardware.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tcam_alloc_set(struct tfc *tfcp, u16 fid, enum cfa_track_type tt,
		       u16 priority, struct tfc_tcam_info *tcam_info,
		       const struct tfc_tcam_data *tcam_data);

/**
 * Set a TFC TCAM entry
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @tcam_info: All the information related to the requested index table entry
 *	       (subtype/dir) including the id.
 * @tcam_data: Pointer to the tcam data, including tcam, mask, and remap,
 *	       to write to the entry. The data is aligned in the buffer
 *	       for writing to the hardware.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tcam_set(struct tfc *tfcp, u16 fid,
		 const struct tfc_tcam_info *tcam_info,
		 const struct tfc_tcam_data *tcam_data);

/**
 * Get a TFC TCAM entry
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @tcam_info: All the information related to the requested TCAM entry
 *	       (subtype/dir) including the id.
 * @tcam_data: Pointer to the tcam data, including tcam, mask, and remap,
 *	       to read from the entry. The data is aligned in the buffer
 *	       for writing to the hardware.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tcam_get(struct tfc *tfcp, u16 fid,
		 const struct tfc_tcam_info *tcam_info,
		 struct tfc_tcam_data *tcam_data);
/**
 * Free a TFC TCAM entry
 *
 * @fid: Function ID to be used
 * @tfcp: Pointer to TFC handle
 * @tcam_info: All the information related to the requested tcam entry (subtype/dir)
 *	       and the id to be freed.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tcam_free(struct tfc *tfcp, u16 fid,
		  const struct tfc_tcam_info *tcam_info);

/* tfc_tbl_scope_bucket_factor indicates a multiplier factor for determining the
 * static and dynamic buckets counts.  The larger the factor, the more buckets
 * will be allocated.
 *
 * This is necessary because flows will not hash so as to perfectly fill all the
 * buckets.  It is necessary to add some allowance for not fully populated
 * buckets.
 */
enum tfc_tbl_scope_bucket_factor {
	TFC_TBL_SCOPE_BUCKET_FACTOR_1 = 1,
	TFC_TBL_SCOPE_BUCKET_FACTOR_2 = 2,
	TFC_TBL_SCOPE_BUCKET_FACTOR_4 = 4,
	TFC_TBL_SCOPE_BUCKET_FACTOR_8 = 8,
	TFC_TBL_SCOPE_BUCKET_FACTOR_16 = 16,
	TFC_TBL_SCOPE_BUCKET_FACTOR_32 = 32,
	TFC_TBL_SCOPE_BUCKET_FACTOR_64 = 64,
	TFC_TBL_SCOPE_BUCKET_FACTOR_MAX = TFC_TBL_SCOPE_BUCKET_FACTOR_64
};

/* tfc_tbl_scope_size_query_parms contains the parameters for the
 * tfc_tbl_scope_size_query API.
 */
struct tfc_tbl_scope_size_query_parms {
	/* Scope is one of non-shared, shared-app or global.
	 * If a shared-app or global table scope, dynamic buckets are disabled.
	 * this combined with the multiplier affects the calculation for static
	 * buckets in this function.
	 */
	enum cfa_scope_type scope_type;
	/* Direction indexed array indicating the number of flows.  Must be
	 * at least as large as the number entries that the buckets can point
	 * to.
	 */
	u32 flow_cnt[CFA_DIR_MAX];
	/* tfc_tbl_scope_bucket_factor indicates a multiplier factor for
	 * determining the static and dynamic buckets counts.  The larger the
	 * factor, the more buckets will be allocated.
	 */
	enum tfc_tbl_scope_bucket_factor factor;
	/* The number of pools each region of the table scope will be
	 * divided into.
	 */
	u32 max_pools;
	/* Direction indexed array indicating the key size. */
	u16 key_sz_in_bytes[CFA_DIR_MAX];
	/* Direction indexed array indicating the action record size.  Must
	 * be a multiple of 32B lines on Thor2.
	 */
	u16 act_rec_sz_in_bytes[CFA_DIR_MAX];
	/* Direction indexed array indicating the EM static bucket count
	 * expressed as: log2(static_bucket_count). For example if 1024 static
	 * buckets, 1024=2^10, so the value 10 would be Returned.
	 */
	u8 static_bucket_cnt_exp[CFA_DIR_MAX];
	/* Direction indexed array indicating the EM dynamic bucket count. */
	u32 dynamic_bucket_cnt[CFA_DIR_MAX];
	/* The number of minimum sized lkup records per direction.  In
	 * this usage, records are the minimum lookup memory allocation unit in
	 * a table scope.  This value is the total memory required for buckets
	 * and entries.
	 *
	 * Note: The EAS variously refers to these as words or cache-lines.
	 *
	 * For example, on Thor2 where each bucket consumes one record, if the
	 * key size is such that the LREC and key use 2 records, then the
	 * lkup_rec_cnt = the number of buckets + (2 * the number of flows).
	 */
	u32 lkup_rec_cnt[CFA_DIR_MAX];
	/* The number of minimum sized action records per direction.
	 * Similar to the lkup_rec_cnt, records are the minimum action memory
	 * allocation unit in a table scope.
	 */
	u32 act_rec_cnt[CFA_DIR_MAX];
	/* Direction indexed array indicating the size of each individual
	 * lookup record pool expressed as: log2(max_records/max_pools). For
	 * example if 1024 records and 2 pools 1024/2=512=2^9, so the value 9
	 * would be entered.
	 */
	u8 lkup_pool_sz_exp[CFA_DIR_MAX];
	/* Direction indexed array indicating the size of each individual
	 * action record pool expressed as: log2(max_records/max_pools). For
	 * example if 1024 records and 2 pools 1024/2=512=2^9, so the value 9
	 * would be entered.
	 */
	u8 act_pool_sz_exp[CFA_DIR_MAX];

	/* Direction indexed array indicating the offset in records from
	 * the start of the memory after the static buckets where the first
	 * lrec pool begins.
	 */
	u32 lkup_rec_start_offset[CFA_DIR_MAX];
};

/* tfc_tbl_scope_mem_alloc_parms contains the parameters for allocating memory
 * to be used by a table scope.
 */
struct tfc_tbl_scope_mem_alloc_parms {
	/* Scope is one of non-shared, shared-app or global.
	 * If a shared-app or global table scope, dynamic buckets are disabled.
	 * this combined with the multiplier affects the calculation for static
	 * buckets in this function.
	 */
	enum cfa_scope_type scope_type;
	/* If a shared-app or global table scope, indicate whether this is the
	 * first.  If so, the table scope memory will be allocated.  Otherwise
	 * only the details of the configuration will be stored internally
	 * for use - i.e. act_rec_cnt/lkup_rec_cnt/lkup_rec_start_offset.
	 */
	bool first;
	/* Direction indexed array indicating the EM static bucket count
	 * expressed as: log2(static_bucket_count). For example if 1024 static
	 * buckets, 1024=2^10, so the value 10 would be entered.
	 */
	u8 static_bucket_cnt_exp[CFA_DIR_MAX];
	/* Direction indexed array indicating the EM dynamic bucket count. */
	u8 dynamic_bucket_cnt[CFA_DIR_MAX];
	/* The number of minimum sized lkup records per direction. In this
	 * usage, records are the minimum lookup memory allocation unit in a
	 * table scope.	 This value is the total memory required for buckets and
	 * entries.
	 */
	u32 lkup_rec_cnt[CFA_DIR_MAX];
	/* The number of minimum sized action records per direction.
	 * Similar to the lkup_rec_cnt, records are the minimum action memory
	 * allocation unit in a table scope.
	 */
	u32 act_rec_cnt[CFA_DIR_MAX];
	/* The page size used for allocation. If running in the kernel
	 * driver, this may be as small as 1KB.	 For huge pages this may be more
	 * commonly 2MB. Supported values include 4K, 8K, 64K, 2M, 8M and 1GB.
	 */
	u32 pbl_page_sz_in_bytes;
	/* Indicates local application vs remote application table scope. A
	 * table scope can be created on a PF for it's own use or for use by
	 * other children.  These may or may not be shared table scopes.  Set
	 * local to false if calling API on behalf of a remote client VF.
	 * (alternatively, we could pass in the remote fid or the local fid).
	 */
	bool local;
	/* The maximum number of pools supported. */
	u8 max_pools;
	/* Direction indexed array indicating the action table pool size
	 * expressed as: log2(act_pool_sz). For example if 1024 static
	 * buckets, 1024=2^10, so the value 10 would be entered.
	 */
	u8 act_pool_sz_exp[CFA_DIR_MAX];
	/* Direction indexed array indicating the lookup table pool size
	 * expressed as: log2(lkup_pool_sz). For example if 1024 static
	 * buckets, 1024=2^10, so the value 10 would be entered.
	 */
	u8 lkup_pool_sz_exp[CFA_DIR_MAX];
	/* Lookup table record start offset.  Offset in 32B records after
	 * the static buckets where the lookup records and dynamic bucket memory
	 * will begin.
	 */
	u32 lkup_rec_start_offset[CFA_DIR_MAX];
};

/* tfc_tbl_scope_qcaps_parms contains the parameters for determining
 * the table scope capabilities
 */
struct tfc_tbl_scope_qcaps_parms {
	/* if true, the device supports a table scope.
	 */
	bool tbl_scope_cap;
	/* if true, the device supports a global table scope.
	 */
	bool global_cap;
	/* if true, the device supports locked regions.
	 */
	bool locked_cap;
	/* the maximum number of static buckets supported.
	 */
	uint8_t max_lkup_static_bucket_exp;
	/* The maximum number of minimum sized lkup records supported.
	 */
	uint32_t max_lkup_rec_cnt;
	/* The maximum number of  minimum sized action records supported.
	 */
	uint32_t max_act_rec_cnt;
};

/**
 * Determine whether table scopes are supported in the hardware.
 *
 * @tfcp: Pointer to TFC handle
 * @parms: Pointer to the table scope query capability parms
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_qcaps(struct tfc *tfcp,
			struct tfc_tbl_scope_qcaps_parms *parms);

/**
 * Determine table scope sizing
 *
 * @tfcp: Pointer to TFC handle
 * @parms: The parameters used by this function.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_size_query(struct tfc *tfcp,
			     struct tfc_tbl_scope_size_query_parms *parms);

/**
 * Allocate a table scope
 *
 * @tfcp: Pointer to TFC handle
 * @scope_type: non-shared, shared-app or global scope.
 * @app_type: application type, TF or AFM.
 * @tsid: The allocated table scope ID.
 * @first: True if the caller is the creator of this table scope.
 *	   If not shared, first is always set.
 *
 * Returns
 *	 0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_id_alloc(struct tfc *tfcp, enum cfa_scope_type scope_type,
			   enum cfa_app_type app_type, u8 *tsid,
			   bool *first);

/**
 * Allocate memory for a table scope
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function id requesting the memory allocation
 * @tsid: Table scope identifier
 * @parms: Memory allocation parameters
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_mem_alloc(struct tfc *tfcp, u16 fid, u8 tsid,
			    struct tfc_tbl_scope_mem_alloc_parms *parms);

/**
 * Free memory for a table scope
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function id for memory to free from the table scope. Set to INVALID_FID
 *	 by default. Populated when VF2PF mem_free message received from a VF
 *	 for a shared or global table scope.
 * @tsid: Table scope identifier
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_mem_free(struct tfc *tfcp, u16 fid, u8 tsid);

/* tfc_tbl_scope_cpm_alloc_parms contains the parameters for allocating a
 * CPM instance to be used by a table scope.
 */
struct tfc_tbl_scope_cpm_alloc_parms {
	/*
	 * Direction indexed array indicating the maximum number of lookup
	 * contiguous records.
	 */
	u8 lkup_max_contig_rec[CFA_DIR_MAX];
	/*
	 * Direction indexed array indicating the maximum number of action
	 * contiguous records.
	 */
	u8 act_max_contig_rec[CFA_DIR_MAX];
	/* The maximum number of pools supported by the table scope. */
	u16 max_pools;
};

/**
 * Allocate CFA Pool Manager (CPM) Instance
 *
 * @tfcp: Pointer to TFC handle
 * @cpm_parms: Pointer to the CFA Pool Manager parameters
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_cpm_alloc(struct tfc *tfcp, u8 tsid,
			    struct tfc_tbl_scope_cpm_alloc_parms *cpm_parms);

/**
 * Free CPM Instance
 *
 * @tfcp: Pointer to TFC handle
 * @tsid: Table scoep identifier
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_cpm_free(struct tfc *tfcp, u8 tsid);

/**
 * Associate a forwarding function with an existing table scope
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function id to associated with the table scope
 * @tsid: Table scope identifier
 * @fid_cnt: The number of forwarding functions currently associated with the table scope
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_fid_add(struct tfc *tfcp, u16 fid, u8 tsid, u16 *fid_cnt);

/**
 * Disassociate a forwarding function from an existing TFC table scope
 *
 * Once the last function has been removed from the session in the firmware
 * the session is freed and all associated resources freed.
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function id to remove from the table scope
 * @tsid: Table scope identifier
 * @fid_cnt: The number of forwarding functions currently associated with the session
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_fid_rem(struct tfc *tfcp, u16 fid, u8 tsid, u16 *fid_cnt);

/**
 * Pool allocation
 *
 * Allocate a pool ID and set it's size
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function id allocating the pool
 * @tsid: Table scope identifier
 * @region: Pool region identifier
 * @dir: Direction
 * @pool_sz_exp: Pool size exponent
 * @pool_id: Used to Return the allocated pool ID.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_pool_alloc(struct tfc *tfcp, u16 fid, u8 tsid,
			     enum cfa_region_type region,
			     enum cfa_dir dir, u8 *pool_sz_exp, u16 *pool_id);

/**
 * Pool free
 *
 * Free a pool ID
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function freeing the pool
 * @tsid: Table scope identifier
 * @region: Pool region identifier
 * @dir: Direction
 * @pool_id: Used to Return the allocated pool ID.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_pool_free(struct tfc *tfcp, u16 fid, u8 tsid,
			    enum cfa_region_type region, enum cfa_dir dir,
			    u16 pool_id);

/**
 * Get configured state
 *
 * This API is intended for DPDK applications where a single table scope is shared
 * across one or more DPDK instances. When one instance succeeds to allocate and
 * configure a table scope, it then sets the config for that table scope; while
 * other sessions polling and waiting for the shared or global table scope to be
 * configured.
 *
 * @tfcp: Pointer to TFC handle
 * @tsid: Table scope identifier
 * @configured: Used to Return the allocated pool ID.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_config_state_get(struct tfc *tfcp, u8 tsid, bool *configured);

/**
 * Table scope function reset
 *
 * Delete resources and EM entries associated with fid.
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Table scope identifier
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_tbl_scope_func_reset(struct tfc *tfcp, u16 fid);

/**
 * Table scope cleanup
 *
 * Free memory and state related to all non local table scopes after
 * firmware device reset.  Local table scopes are used by this driver
 * port.  Non-local table scopes are created on behalf of child VFs
 * of this PF.  This function is only functional on a PF.
 *
 * @tfcp: Pointer to TFC handle
 *
 * Returns
 *   none
 */
void tfc_tbl_scope_cleanup(struct tfc *tfcp);

/* Forward ref: defined in bnxt_tfc.h */
struct tfc_mpc_batch_info_t;

/**
 * Start MPC batching
 *
 * @param[in/out] batch_info
 *   Contains batch processing info
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_mpc_batch_start(struct tfc_mpc_batch_info_t *batch_info);

/**
 * Ends MPC batching and returns the accumulated results
 *
 * @param[in/out] batch_info
 *   Contains batch processing info
 *
 * @returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_mpc_batch_end(void *p,
		      struct tfc *tfcp,
		      struct tfc_mpc_batch_info_t *batch_info);

/**
 * Checks to see if batching is active and other MPCs have been sent
 *
 * @param[in/out] batch_info
 *   Contains batch processing info
 *
 * @returns
 *   True is started and MPCs have been sent else False.
 */
bool tfc_mpc_batch_started(struct tfc_mpc_batch_info_t *batch_info);

/* tfc_em_insert_parms contains the parameters for an EM insert. */
struct tfc_em_insert_parms {
	enum cfa_dir dir;	/* Entry direction. */
	u8 *lkup_key_data;	/* ptr to the combined lkup record and key data to be written. */
	u16 lkup_key_sz_words;	/* The size of the entry to write in 32b words. */
	const u8 *key_data;	/* Thor only - The key data to be used to calculate the hash. */
	u16 key_sz_bits;	/* Thor only - Size of key in bits. */
	u64 *flow_handle;	/* Will contain the entry flow handle a unique identifier. */
	struct tfc_mpc_batch_info_t *batch_info; /* batch control data */
};

/**
 * Insert an EM Entry
 *
 * @tfcp: Pointer to TFC handle
 * @tsid: Table scope id
 * @parms: EM insert params
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 *   Error codes -1 through -9 indicate an MPC error and the
 *   positive value of the error code maps directly on to the
 *   MPC error code. For example, if the value -8 is Returned
 *   it indicates a CFA_BLD_MPC_EM_DUPLICATE error occurred.
 */
int tfc_em_insert(struct tfc *tfcp, u8 tsid, struct tfc_em_insert_parms *parms);

/**
 * tfc_em_delete_parms Contains args required to delete an EM Entry
 *
 * @tfcp: Pointer to TFC handle
 * @dir: Direction (CFA_DIR_RX or CFA_DIR_TX)
 * @flow_handle: The flow handle Returned to be used for flow deletion.
 *
 */
struct tfc_em_delete_parms {
	/* Entry direction. */
	enum cfa_dir dir;
	/* Flow handle of flow to delete */
	u64 flow_handle;
	/* batch control data */
	struct tfc_mpc_batch_info_t *batch_info;
};

/**
 * Delete an EM Entry
 *
 * @tfcp: Pointer to TFC handle
 * @parms: EM delete parameters
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 *   Error codes -1 through -9 indicate an MPC error and the
 *   positive value of the error code maps directly on to the
 *   MPC error code. For example, if the value -8 is Returned
 *   it indicates a CFA_BLD_MPC_EM_DUPLICATE error occurred.
 */
int tfc_em_delete(struct tfc *tfcp, struct tfc_em_delete_parms *parms);

/* CMM resource structure */
struct tfc_cmm_info {
	enum cfa_resource_subtype_cmm rsubtype;	/* resource subtype */
	enum cfa_dir dir;			/* direction rx/tx */
	u64 act_handle;				/* alloc/free handle */
};

/**
 * CMM resource clear structure
 */
struct tfc_cmm_clr {
	bool clr; /**< flag for clear */
	u16 offset_in_byte; /**< field offset in byte */
	u16 sz_in_byte; /**<  field size in byte */
};

/**
 * Allocate an action CMM Resource
 *
 * @tfcp: Pointer to TFC handle
 * @tsid: Table scope id
 * @cmm_info: Pointer to cmm info
 * @num_contig_rec: Num contiguous records required. Record size is 8B for
 *		    Thor/32B for Thor2.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_act_alloc(struct tfc *tfcp, u8 tsid, struct tfc_cmm_info *cmm_info,
		  u16 num_contig_rec);

/**
 * Set an action CMM resource
 *
 * @tfcp: Pointer to TFC handle
 * @cmm_info: Pointer to cmm info.
 * @data: Data to be written.
 * @data_sz_words: Data buffer size in words. In 8B increments.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 *   Error codes -1 through -9 indicate an MPC error and the
 *   positive value of the error code maps directly on to the
 *   MPC error code. For example, if the value -8 is Returned
 *   it indicates a CFA_BLD_MPC_EM_DUPLICATE error occurred.
 */
int tfc_act_set(struct tfc *tfcp, const struct tfc_cmm_info *cmm_info,
		const u8 *data, u16 data_sz_words,
		struct tfc_mpc_batch_info_t *batch_info);

/**
 * Get an action CMM resource
 *
 * @tfcp: Pointer to TFC handle
 * @batch_info: Pointer to batch info
 * @cmm_info: Pointer to cmm info
 * @cmm_clr: Pointer to cmm clr
 * @data_pa: Data physical address. Must be word aligned, i.e. [1:0] must be 0.
 * @data_sz_words: Data buffer size in words. Size could be 8/16/24/32/64B
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 *   Error codes -1 through -9 indicate an MPC error and the
 *   positive value of the error code maps directly on to the
 *   MPC error code. For example, if the value -8 is Returned
 *   it indicates a CFA_BLD_MPC_EM_DUPLICATE error occurred.
 */
int tfc_act_get(struct tfc *tfcp, struct tfc_mpc_batch_info_t *batch_info,
		const struct tfc_cmm_info *cmm_info,
		struct tfc_cmm_clr *clr,
		u64 data_pa, u16 *data_sz_words);
/**
 * Free a CMM Resource
 *
 * @tfcp: Pointer to TFC handle
 * @cmm_info: CMM info
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_act_free(struct tfc *tfcp, const struct tfc_cmm_info *cmm_info);

/* IF table resource structure
 */
struct tfc_if_tbl_info {
	enum cfa_resource_subtype_if_tbl rsubtype;	/* resource subtype */
	enum cfa_dir dir;				/* direction rx/tx */
	u16 id;						/* index */
};

/**
 * Set a TFC if table entry
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @tbl_info: All the information related to the requested index table entry
 *	      (subtype/dir) including the id.
 * @data: Pointer to the data to write to the entry. The data is aligned
 *	  correctly in the buffer for writing to the hardware.
 * @data_sz_in_bytes: The size of the entry in device sized bytes for Thor2.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_if_tbl_set(struct tfc *tfcp, u16 fid,
		   const struct tfc_if_tbl_info *tbl_info,
		   const u8 *data, u8 data_sz_in_bytes);

/**
 * Get a TFC if table entry
 *
 * @tfcp: Pointer to TFC handle
 * @fid: Function ID to be used
 * @tbl_info: All the information related to the requested index table entry
 *	      (subtype/dir) including the id.
 * @data: Pointer to the data to read from the entry.
 * @data_sz_in_bytes: The size of the entry in device sized bytes for Thor2.
 *		      Input is the size of the buffer, output is the actual size.
 *
 * Returns
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_if_tbl_get(struct tfc *tfcp, u16 fid,
		   const struct tfc_if_tbl_info *tbl_info,
		   u8 *data, u8 *data_sz_in_bytes);
#endif /* _TFC_H_ */
