/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_MAPPER_H_
#define _ULP_MAPPER_H_

#include "linux/kernel.h"
#include "tf_core.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "bnxt_tf_ulp.h"
#include "ulp_utils.h"
#include "ulp_gen_tbl.h"
#include "bitalloc.h"
#include "ulp_alloc_tbl.h"

#define ULP_IDENTS_INVALID ((u16)U16_MAX)

struct bnxt_ulp_mapper_glb_resource_entry {
	enum bnxt_ulp_resource_func	resource_func;
	u32			resource_type; /* TF_ enum type */
	u64			resource_hndl;
	bool				shared;
};

#define BNXT_ULP_KEY_RECIPE_MAX_FLDS 128
struct bnxt_ulp_key_recipe_entry {
	bool in_use;
	u32 cnt;
	struct bnxt_ulp_mapper_key_info	flds[BNXT_ULP_KEY_RECIPE_MAX_FLDS];
};

#define ULP_RECIPE_TYPE_MAX (BNXT_ULP_RESOURCE_SUB_TYPE_KEY_RECIPE_TABLE_WM + 1)
struct bnxt_ulp_key_recipe_info {
	u32 num_recipes;
	u8 max_fields;
	struct bnxt_ulp_key_recipe_entry **recipes[BNXT_ULP_DIRECTION_LAST][ULP_RECIPE_TYPE_MAX];
	struct bitalloc *recipe_ba[BNXT_ULP_DIRECTION_LAST][ULP_RECIPE_TYPE_MAX];
};

struct ulp_mapper_core_ops;

struct bnxt_ulp_mapper_data {
	const struct ulp_mapper_core_ops *mapper_oper;
	struct bnxt_ulp_mapper_glb_resource_entry
		glb_res_tbl[TF_DIR_MAX][BNXT_ULP_GLB_RF_IDX_LAST];
	struct ulp_mapper_gen_tbl_list gen_tbl_list[BNXT_ULP_GEN_TBL_MAX_SZ];
	struct bnxt_ulp_key_recipe_info key_recipe_info;
	struct ulp_allocator_tbl_entry alloc_tbl[BNXT_ULP_ALLOCATOR_TBL_MAX_SZ];
};

/* Internal Structure for passing the arguments around */
struct bnxt_ulp_mapper_parms {
	enum bnxt_ulp_template_type	tmpl_type;
	u32				dev_id;
	u32				act_tid;
	u32				class_tid;
	struct ulp_tc_act_prop		*act_prop;
	struct ulp_tc_hdr_bitmap	*act_bitmap;
	struct ulp_tc_hdr_bitmap	*hdr_bitmap;
	struct ulp_tc_hdr_bitmap	*enc_hdr_bitmap;
	struct ulp_tc_hdr_field		*hdr_field;
	struct ulp_tc_hdr_field		*enc_field;
	struct ulp_tc_field_bitmap	*fld_bitmap;
	u64				*comp_fld;
	struct ulp_regfile		*regfile;
	struct bnxt_ulp_context		*ulp_ctx;
	u32				flow_id;
	u16				func_id;
	u32				rid;
	enum bnxt_ulp_fdb_type		flow_type;
	struct bnxt_ulp_mapper_data	*mapper_data;
	struct bnxt_ulp_device_params	*device_params;
	u32				child_flow;
	u32				parent_flow;
	u8				tun_idx;
	u32				app_priority;
	u64				shared_hndl;
	u32				flow_pattern_id;
	u32				act_pattern_id;
	u8				app_id;
	u16				port_id;
	u16				fw_fid;
	u64				cf_bitmap;
	u64				wc_field_bitmap;
	u64				exclude_field_bitmap;
	struct tfc_mpc_batch_info_t     *batch_info;
};

/* Function to initialize any dynamic mapper data. */
struct ulp_mapper_core_ops {
	int
	(*ulp_mapper_core_tcam_tbl_process)(struct bnxt_ulp_mapper_parms *parms,
					    struct bnxt_ulp_mapper_tbl_info *t);
	int
	(*ulp_mapper_core_tcam_entry_free)(struct bnxt_ulp_context *ulp_ctx,
					   struct ulp_flow_db_res_params *res);
	int
	(*ulp_mapper_core_em_tbl_process)(struct bnxt_ulp_mapper_parms *parms,
					  struct bnxt_ulp_mapper_tbl_info *t,
					  void *error);
	int
	(*ulp_mapper_core_em_entry_free)(struct bnxt_ulp_context *ulp,
					 struct ulp_flow_db_res_params *res,
					 void *error);

	int
	(*ulp_mapper_core_index_tbl_process)(struct bnxt_ulp_mapper_parms *parm,
					     struct bnxt_ulp_mapper_tbl_info
					     *t);
	int
	(*ulp_mapper_core_index_entry_free)(struct bnxt_ulp_context *ulp,
					    struct ulp_flow_db_res_params *res);
	int
	(*ulp_mapper_core_cmm_tbl_process)(struct bnxt_ulp_mapper_parms *parm,
					   struct bnxt_ulp_mapper_tbl_info *t,
					   void  *error);
	int
	(*ulp_mapper_core_cmm_entry_free)(struct bnxt_ulp_context *ulp,
					  struct ulp_flow_db_res_params *res,
					  void *error);
	int
	(*ulp_mapper_core_if_tbl_process)(struct bnxt_ulp_mapper_parms *parms,
					  struct bnxt_ulp_mapper_tbl_info *t);

	int
	(*ulp_mapper_core_ident_alloc_process)(struct bnxt_ulp_context *ulp_ctx,
					       u32 session_type,
					       u16 ident_type,
					       u8 direction,
					       enum cfa_track_type tt,
					       u64 *identifier_id);

	int
	(*ulp_mapper_core_index_tbl_alloc_process)(struct bnxt_ulp_context *ulp,
						   u32 session_type,
						   u16 table_type,
						   u8 direction,
						   u64 *index);
	int
	(*ulp_mapper_core_ident_free)(struct bnxt_ulp_context *ulp_ctx,
				      struct ulp_flow_db_res_params *res);
	u32
	(*ulp_mapper_core_dyn_tbl_type_get)(struct bnxt_ulp_mapper_parms *parms,
					    struct bnxt_ulp_mapper_tbl_info *t,
					    u16 blob_len,
					    u16 *out_len);
	int
	(*ulp_mapper_core_app_glb_res_info_init)(struct bnxt_ulp_context *ulp_ctx,
						 struct bnxt_ulp_mapper_data *mapper_data);

	int
	(*ulp_mapper_core_handle_to_offset)(struct bnxt_ulp_mapper_parms *parms,
					    u64 handle,
					    u32 offset,
					    u64 *result);
	int
	(*ulp_mapper_mpc_batch_start)(struct tfc_mpc_batch_info_t *batch_info);

	bool
	(*ulp_mapper_mpc_batch_started)(struct tfc_mpc_batch_info_t *batch_info);

	int
	(*ulp_mapper_mpc_batch_end)(struct bnxt *bp,
				    void *tfcp,
				    struct tfc_mpc_batch_info_t *batch_info);
};

extern const struct ulp_mapper_core_ops ulp_mapper_tf_core_ops;
extern const struct ulp_mapper_core_ops ulp_mapper_tfc_core_ops;

int
ulp_mapper_glb_resource_read(struct bnxt_ulp_mapper_data *mapper_data,
			     enum tf_dir dir,
			     u16 idx,
			     u64 *regval,
			     bool *shared);

int
ulp_mapper_glb_resource_write(struct bnxt_ulp_mapper_data *data,
			      struct bnxt_ulp_glb_resource_info *res,
			      u64 regval, bool shared);

int
ulp_mapper_resource_ident_allocate(struct bnxt_ulp_context *ulp_ctx,
				   struct bnxt_ulp_mapper_data *mapper_data,
				   struct bnxt_ulp_glb_resource_info *glb_res,
				   bool shared);

int
ulp_mapper_resource_index_tbl_alloc(struct bnxt_ulp_context *ulp_ctx,
				    struct bnxt_ulp_mapper_data *mapper_data,
				    struct bnxt_ulp_glb_resource_info *glb_res,
				    bool shared);

struct bnxt_ulp_mapper_key_info *
ulp_mapper_key_fields_get(struct bnxt_ulp_mapper_parms *mparms,
			  struct bnxt_ulp_mapper_tbl_info *tbl,
			  u32 *num_flds);

uint32_t
ulp_mapper_partial_key_fields_get(struct bnxt_ulp_mapper_parms *mparms,
				  struct bnxt_ulp_mapper_tbl_info *tbl);

int
ulp_mapper_fdb_opc_process(struct bnxt_ulp_mapper_parms *parms,
			   struct bnxt_ulp_mapper_tbl_info *tbl,
			   struct ulp_flow_db_res_params *fid_parms);

int
ulp_mapper_priority_opc_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl,
				u32 *priority);

int
ulp_mapper_tbl_ident_scan_ext(struct bnxt_ulp_mapper_parms *parms,
			      struct bnxt_ulp_mapper_tbl_info *tbl,
			      u8 *byte_data,
			      u32 byte_data_size,
			      enum bnxt_ulp_byte_order byte_order);

int
ulp_mapper_field_opc_process(struct bnxt_ulp_mapper_parms *parms,
			     enum tf_dir dir,
			     struct bnxt_ulp_mapper_field_info *fld,
			     struct ulp_blob *blob,
			     u8 is_key,
			     const char *name);

int
ulp_mapper_key_recipe_field_opc_process(struct bnxt_ulp_mapper_parms *parms,
					enum bnxt_ulp_direction dir,
					struct bnxt_ulp_mapper_field_info *fld,
					u8 is_key,
					const char *name,
					bool *written,
					struct bnxt_ulp_mapper_field_info *ofld);

int
ulp_mapper_tbl_result_build(struct bnxt_ulp_mapper_parms *parms,
			    struct bnxt_ulp_mapper_tbl_info *tbl,
			    struct ulp_blob *data,
			    const char *name);

int
ulp_mapper_mark_gfid_process(struct bnxt_ulp_mapper_parms *parms,
			     struct bnxt_ulp_mapper_tbl_info *tbl,
			     u64 flow_id);

int
ulp_mapper_mark_act_ptr_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl);

int
ulp_mapper_mark_vfr_idx_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl);

int
ulp_mapper_tcam_tbl_ident_alloc(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl);

u32
ulp_mapper_wc_tcam_tbl_dyn_post_process(struct bnxt_ulp_context *ulp_ctx,
					struct bnxt_ulp_device_params *dparms,
					struct ulp_blob *key,
					struct ulp_blob *mask,
					struct ulp_blob *tkey,
					struct ulp_blob *tmask);

void ulp_mapper_wc_tcam_tbl_post_process(struct bnxt_ulp_context *ulp_ctx, struct ulp_blob *blob);

int
ulp_mapper_resources_free(struct bnxt_ulp_context *ulp_ctx,
			  enum bnxt_ulp_fdb_type flow_type,
			  u32 fid,
			  void *error);

int
ulp_mapper_flow_destroy(struct bnxt_ulp_context *ulp_ctx,
			enum bnxt_ulp_fdb_type flow_type,
			u32 fid,
			void *error);

int
ulp_mapper_flow_create(struct bnxt_ulp_context	*ulp_ctx,
		       struct bnxt_ulp_mapper_parms *parms,
		       void *error);

struct bnxt_ulp_mapper_key_info *
ulp_mapper_key_recipe_fields_get(struct bnxt_ulp_mapper_parms *parms,
				 struct bnxt_ulp_mapper_tbl_info *tbl,
				 u32 *num_flds);

int
ulp_mapper_init(struct bnxt_ulp_context	*ulp_ctx);

void
ulp_mapper_deinit(struct bnxt_ulp_context *ulp_ctx);

#endif /* _ULP_MAPPER_H_ */
