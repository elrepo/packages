/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef ULP_TEMPLATE_DEBUG_PROTO_H_
#define ULP_TEMPLATE_DEBUG_PROTO_H_

#include "tfc.h"

/* Function to dump the tc flow pattern. */
void
ulp_parser_hdr_info_dump(struct ulp_tc_parser_params	*params);

/* Function to dump the tc flow actions. */
void
ulp_parser_act_info_dump(struct ulp_tc_parser_params	*params);

/* Function to dump the error field during matching. */
void
ulp_matcher_act_field_dump(struct bnxt_ulp_context *ulp_ctx,
			   u32	idx,
			   u32	jdx,
			   u32	mask_id);

/* * Function to dump the blob during the mapper processing. */
void
ulp_mapper_field_dump(struct bnxt_ulp_context *ulp_ctx,
		      const char *name,
		      struct bnxt_ulp_mapper_field_info *fld,
		      struct ulp_blob *blob,
		      u16 write_idx,
		      u8 *val,
		      u32 field_size);

/* Function to dump the identifiers during the mapper processing. */
void
ulp_mapper_ident_field_dump(struct bnxt_ulp_context *ulp_ctx,
			    const char *name,
			    struct bnxt_ulp_mapper_ident_info *ident,
			    struct bnxt_ulp_mapper_tbl_info *tbl,
			    int id);
void
ulp_mapper_tcam_entry_dump(struct bnxt_ulp_context *ulp_ctx,
			   const char *name,
			   u32 idx,
			   struct bnxt_ulp_mapper_tbl_info *tbl,
			   struct ulp_blob *key,
			   struct ulp_blob *mask,
			   struct ulp_blob *result);
void
ulp_mapper_result_dump(struct bnxt_ulp_context *ulp_ctx,
		       const char *name,
		       struct bnxt_ulp_mapper_tbl_info *tbl,
		       struct ulp_blob *result);

void
ulp_mapper_act_dump(struct bnxt_ulp_context *ulp_ctx,
		    const char *name,
		    struct bnxt_ulp_mapper_tbl_info *tbl,
		    struct ulp_blob *data);

void
ulp_mapper_em_dump(struct bnxt_ulp_context *ulp_ctx,
		   const char *name,
		   struct ulp_blob *key,
		   struct ulp_blob *data,
		   struct tf_insert_em_entry_parms *iparms);

void
ulp_mapper_tfc_em_dump(struct bnxt_ulp_context *ulp_ctx,
		       const char *name,
		       struct ulp_blob *blob,
		       struct tfc_em_insert_parms *iparms);

void
ulp_mapper_blob_dump(struct bnxt_ulp_context *ulp_ctx,
		     struct ulp_blob *blob);

void
ulp_mapper_table_dump(struct bnxt_ulp_context *ulp_ctx,
		      struct bnxt_ulp_mapper_tbl_info *tbl, u32 idx);

void
ulp_mapper_gen_tbl_dump(struct bnxt_ulp_context *ulp_ctx,
			u32 sub_type, u8 direction,
			struct ulp_blob *key);

const char *
ulp_mapper_key_recipe_type_to_str(u32 sub_type);

void
ulp_mapper_global_register_tbl_dump(struct bnxt_ulp_context *ulp_ctx,
				    u32 sub_type, u16 port);
#endif
