/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#include "tfc.h"
#include "tfo.h"

/* HWRM Direct messages */

int
tfc_msg_tbl_scope_qcaps(struct tfc *tfcp, bool *tbl_scope_cap, bool *global_scope_cap,
			bool *locked_scope_cap, u32 *max_lkup_rec_cnt,
			u32 *max_act_rec_cnt, u8 *max_lkup_static_buckets_exp);

int tfc_msg_tbl_scope_id_alloc(struct tfc *tfcp, u16 fid, enum cfa_scope_type scope_type,
			       enum cfa_app_type app_type, u8 *tsid,
			       bool *first);

int
tfc_msg_backing_store_cfg_v2(struct tfc *tfcp, u8 tsid, enum cfa_dir dir,
			     enum cfa_region_type region, u64 base_addr,
			     u8 pbl_level, u32 pbl_page_sz,
			     u32 rec_cnt, u8 static_bkt_cnt_exp,
			     bool cfg_done);

int
tfc_msg_tbl_scope_deconfig(struct tfc *tfcp, u8 tsid);

int
tfc_msg_tbl_scope_fid_add(struct tfc *tfcp, u16 fid, u8 tsid, u16 *fid_cnt);

int
tfc_msg_tbl_scope_fid_rem(struct tfc *tfcp, u16 fid, u8 tsid, u16 *fid_cnt);

int
tfc_msg_idx_tbl_alloc(struct tfc *tfcp, u16 fid, u16 sid, enum cfa_track_type tt, enum cfa_dir dir,
		      enum cfa_resource_subtype_idx_tbl subtype,
		      enum cfa_resource_blktype_idx_tbl blktype,
		      u16 *id);

int
tfc_msg_idx_tbl_alloc_set(struct tfc *tfcp, u16 fid, u16 sid, enum cfa_track_type tt,
			  enum cfa_dir dir, enum cfa_resource_subtype_idx_tbl subtype,
			  enum cfa_resource_blktype_idx_tbl blktype,
			  const u32 *dev_data, u8 data_size, u16 *id);

int
tfc_msg_idx_tbl_set(struct tfc *tfcp, u16 fid, u16 sid, enum cfa_dir dir,
		    enum cfa_resource_subtype_idx_tbl subtype,
		    enum cfa_resource_blktype_idx_tbl blktype,
		    u16 id, const u32 *dev_data, u8 data_size);

int
tfc_msg_idx_tbl_get(struct tfc *tfcp, u16 fid, u16 sid, enum cfa_dir dir,
		    enum cfa_resource_subtype_idx_tbl subtype,
		    enum cfa_resource_blktype_idx_tbl blktype,
		    u16 id, u32 *dev_data, u8 *data_size);

int
tfc_msg_idx_tbl_free(struct tfc *tfcp, u16 fid, u16 sid, enum cfa_dir dir,
		     enum cfa_resource_subtype_idx_tbl subtype,
		     enum cfa_resource_blktype_idx_tbl blktype,
		     u16 id);

int tfc_msg_global_id_alloc(struct tfc *tfcp, u16 fid, u16 sid,
			    const struct tfc_global_id_req *glb_id_req,
			    struct tfc_global_id *rsp, bool *first);

int tfc_msg_global_id_free(struct tfc *tfcp, u16 fid, u16 sid,
			   const struct tfc_global_id_req *glb_id_req);

int
tfc_msg_session_id_alloc(struct tfc *tfcp, u16 fid, u16 *tsid);

int
tfc_msg_session_fid_add(struct tfc *tfcp, u16 fid, u16 sid, u16 *fid_cnt);

int
tfc_msg_session_fid_rem(struct tfc *tfcp, u16 fid, u16 sid, u16 *fid_cnt);

int tfc_msg_identifier_alloc(struct tfc *tfcp, enum cfa_dir dir,
			     enum cfa_resource_subtype_ident subtype,
			     enum cfa_track_type tt, u16 fid, u16 sid,
			     u16 *ident_id);

int tfc_msg_identifier_free(struct tfc *tfcp, enum cfa_dir dir,
			    enum cfa_resource_subtype_ident subtype,
			    u16 fid, u16 sid, u16 ident_id);
#ifndef TFC_FORCE_POOL_0
int
tfc_msg_tbl_scope_pool_alloc(struct tfc *tfcp,
			     u8 tsid,
			     enum cfa_dir dir,
			     enum tfc_ts_table_type type,
			     u16 *pool_id,
			     u8 *lkup_pool_sz_exp);

int
tfc_msg_tbl_scope_pool_free(struct tfc *tfcp,
			    u8 tsid,
			    enum cfa_dir dir,
			    enum tfc_ts_table_type type,
			    u16 pool_id);
#endif /* !TFC_FORCE_POOL_0 */

int
tfc_msg_tbl_scope_config_get(struct tfc *tfcp, u8 tsid, bool *configured);

int
tfc_msg_tcam_alloc(struct tfc *tfcp, u16 fid, u16 sid, enum cfa_dir dir,
		   enum cfa_resource_subtype_tcam subtype,
		   enum cfa_track_type tt, u16 pri, u16 key_sz_words,
		   u16 *tcam_id);

int
tfc_msg_tcam_alloc_set(struct tfc *tfcp, u16 fid, u16 sid, enum cfa_dir dir,
		       enum cfa_resource_subtype_tcam subtype,
		       enum cfa_track_type tt, u16 *tcam_id, u16 pri,
		       const u8 *key, u8 key_size, const u8 *mask,
		       const u8 *remap, u8 remap_size);

int
tfc_msg_tcam_set(struct tfc *tfcp, u16 fid, u16 sid, enum cfa_dir dir,
		 enum cfa_resource_subtype_tcam subtype,
		 u16 tcam_id, const u8 *key, u8 key_size,
		 const u8 *mask, const u8 *remap,
		 u8 remap_size);

int
tfc_msg_tcam_get(struct tfc *tfcp, u16 fid, u16 sid, enum cfa_dir dir,
		 enum cfa_resource_subtype_tcam subtype,
		 u16 tcam_id, u8 *key, u8 *key_size,
		 u8 *mask, u8 *remap, u8 *remap_size);

int
tfc_msg_tcam_free(struct tfc *tfcp, u16 fid, u16 sid, enum cfa_dir dir,
		  enum cfa_resource_subtype_tcam subtype, u16 tcam_id);
int
tfc_msg_if_tbl_set(struct tfc *tfcp, u16 fid, u16 sid, enum cfa_dir dir,
		   enum cfa_resource_subtype_if_tbl subtype,
		   u16 index, u8 data_size, const u8 *data);
int
tfc_msg_if_tbl_get(struct tfc *tfcp, u16 fid, u16 sid,
		   enum cfa_dir dir, enum cfa_resource_subtype_if_tbl subtype,
		   u16 index, u8 *data_size, u8 *data);
