/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Broadcom
 * All rights reserved.
 */
#ifndef _TFC_VF2PF_MSG_H_
#define _TFC_VF2PF_MSG_H_

#include "cfa_types.h"
#include "tfc.h"

/* HWRM_OEM_CMD is used to transport the vf2pf commands and responses.
 * All commands will have a naming_authority set to PCI_SIG, oem_id set to
 * 0x14e4 and message_family set to TRUFLOW. The maximum size of the oem_data
 * is 104 bytes.  The response maximum size is 88 bytes.
 */

/* Truflow VF2PF message types */
enum tfc_vf2pf_type {
	TFC_VF2PF_TYPE_TBL_SCOPE_MEM_ALLOC_CFG_CMD = 1,
	TFC_VF2PF_TYPE_TBL_SCOPE_MEM_FREE_CMD,
	TFC_VF2PF_TYPE_TBL_SCOPE_PFID_QUERY_CMD,
	TFC_VF2PF_TYPE_TBL_SCOPE_POOL_ALLOC_CMD,
	TFC_VF2PF_TYPE_TBL_SCOPE_POOL_FREE_CMD,
};

/* Truflow VF2PF response status */
enum tfc_vf2pf_status {
	TFC_VF2PF_STATUS_OK = 0,
	TFC_VF2PF_STATUS_TSID_CFG_ERR = 1,
	TFC_VF2PF_STATUS_TSID_MEM_ALLOC_ERR = 2,
	TFC_VF2PF_STATUS_TSID_INVALID = 3,
	TFC_VF2PF_STATUS_TSID_NOT_CONFIGURED = 4,
	TFC_VF2PF_STATUS_NO_POOLS_AVAIL = 5,
	TFC_VF2PF_STATUS_FID_ERR = 6,
};

/* Truflow VF2PF header used for all Truflow VF2PF cmds/responses */
struct tfc_vf2pf_hdr {
	u16 type;	/* use enum tfc_vf2pf_type */
	u16 fid;	/* VF fid */
};

/* Truflow VF2PF Table Scope Memory allocate/config command */
struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd {
	struct tfc_vf2pf_hdr hdr;		/* vf2pf header */
	u8 tsid;				/* table scope identifier */
	u8 static_bucket_cnt_exp[CFA_DIR_MAX];	/* lkup static bucket count */
	u16 max_pools; /* maximum number of pools requested - 1 for non-shared */
	u32 dynamic_bucket_cnt[CFA_DIR_MAX];	/* dynamic bucket count */
	u32 lkup_rec_cnt[CFA_DIR_MAX];		/* lkup record count */
	u32 act_rec_cnt[CFA_DIR_MAX];		/* action record count */
	u8 lkup_pool_sz_exp[CFA_DIR_MAX]; /* lkup pool sz expressed as log2(max_recs/max_pools) */
	u8 act_pool_sz_exp[CFA_DIR_MAX]; /* action pool sz expressed as log2(max_recs/max_pools) */
	u32 lkup_rec_start_offset[CFA_DIR_MAX]; /* start offset in 32B records of the lkup recs */
						/* (after buckets) */
	enum cfa_scope_type scope_type;		/* scope type */
};

/* Truflow VF2PF Table Scope Memory allocate/config response */
struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp {
	struct tfc_vf2pf_hdr hdr;	/* vf2pf header copied from cmd */
	enum tfc_vf2pf_status status;	/* status of request */
	u8 tsid;			/* tsid allocated */
};

/* Truflow VF2PF Table Scope Memory free command */
struct tfc_vf2pf_tbl_scope_mem_free_cmd {
	struct tfc_vf2pf_hdr hdr;	/* vf2pf header */
	u8 tsid;			/* table scope identifier */
};

/* Truflow VF2PF Table Scope Memory free response */
struct tfc_vf2pf_tbl_scope_mem_free_resp {
	struct tfc_vf2pf_hdr hdr;	/* vf2pf header copied from cmd */
	enum tfc_vf2pf_status status;	/* status of request */
	u8 tsid;			/* tsid memory freed */
};

/* Truflow VF2PF Table Scope PFID query command */
struct tfc_vf2pf_tbl_scope_pfid_query_cmd {
	struct tfc_vf2pf_hdr hdr;	/* vf2pf header */
};

/* Truflow VF2PF Table Scope PFID query response */
struct tfc_vf2pf_pfid_query_resp {
	struct tfc_vf2pf_hdr hdr;	/* vf2pf header copied from cmd */
	enum tfc_vf2pf_status status;	/* status of AFM/NIC flow tbl scope */
	u8 tsid;			/* tsid used for AFM/NIC flow tbl scope */
	u8 lkup_pool_sz_exp[CFA_DIR_MAX]; /* lookup tbl pool size = log2(max_recs/max_pools) */
	u8 act_pool_sz_exp[CFA_DIR_MAX];  /* action tbl pool size = log2(max_recs/max_pools) */
	u32 lkup_rec_start_offset[CFA_DIR_MAX]; /* lkup record start offset in 32B records */
	u16 max_pools;			/* maximum number of pools */
};

/* Truflow VF2PF Table Scope pool alloc command */
struct tfc_vf2pf_tbl_scope_pool_alloc_cmd {
	struct tfc_vf2pf_hdr hdr;	/* vf2pf header */
	u8 tsid;			/* table scope identifier */
	enum cfa_dir dir;		/* direction RX or TX */
	enum cfa_region_type region;	/* region lkup or action */
};

/* Truflow VF2PF Table Scope pool alloc response */
struct tfc_vf2pf_tbl_scope_pool_alloc_resp {
	struct tfc_vf2pf_hdr hdr;	/* vf2pf header copied from cmd */
	enum tfc_vf2pf_status status;	/* status of pool allocation */
	u8 tsid;			/* tbl scope identifier */
	u8 pool_sz_exp;			/* pool size expressed as log2(max_recs/max_pools) */
	u16 pool_id;			/* pool_id allocated */
};

/* Truflow VF2PF Table Scope pool free command */
struct tfc_vf2pf_tbl_scope_pool_free_cmd {
	struct tfc_vf2pf_hdr hdr;	/* vf2pf header */
	enum cfa_dir dir;		/* direction RX or TX */
	enum cfa_region_type region;	/* region lkup or action */
	u8 tsid;			/* table scope id */
	u16 pool_id;			/* pool id */
};

/* Truflow VF2PF Table Scope pool free response */
struct tfc_vf2pf_tbl_scope_pool_free_resp {
	struct tfc_vf2pf_hdr hdr;	/* vf2pf header copied from cmd */
	enum tfc_vf2pf_status status;	/* status of pool allocation */
	u8 tsid;			/* table scope id */
};

int
tfc_vf2pf_mem_alloc(struct tfc *tfcp,
		    struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_cmd *req,
		    struct tfc_vf2pf_tbl_scope_mem_alloc_cfg_resp *resp);

int
tfc_vf2pf_mem_free(struct tfc *tfcp,
		   struct tfc_vf2pf_tbl_scope_mem_free_cmd *req,
		   struct tfc_vf2pf_tbl_scope_mem_free_resp *resp);

int
tfc_vf2pf_pool_alloc(struct tfc *tfcp,
		     struct tfc_vf2pf_tbl_scope_pool_alloc_cmd *req,
		     struct tfc_vf2pf_tbl_scope_pool_alloc_resp *resp);

int
tfc_vf2pf_pool_free(struct tfc *tfcp,
		    struct tfc_vf2pf_tbl_scope_pool_free_cmd *req,
		    struct tfc_vf2pf_tbl_scope_pool_free_resp *resp);

int
tfc_oem_cmd_process(struct tfc *tfcp, u32 *oem_data, u32 *resp, u16 *resp_len);
#endif /* _TFC_VF2PF_MSG_H */
