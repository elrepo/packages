// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2014-2025 Broadcom
 * All rights reserved.
 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_tbl.h"

/* Mapper templates for header class list */
struct bnxt_ulp_mapper_tmpl_info ulp_wh_plus_class_tmpl_list[] = {
	/* class_tid: 1, ingress */
	[1] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 18,
	.start_tbl_idx = 0,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 0,
		.cond_nums = 2 }
	},
	/* class_tid: 2, egress */
	[2] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 15,
	.start_tbl_idx = 18,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 25,
		.cond_nums = 2 }
	},
	/* class_tid: 3, ingress */
	[3] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 22,
	.start_tbl_idx = 33,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 37,
		.cond_nums = 0 }
	},
	/* class_tid: 4, egress */
	[4] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 19,
	.start_tbl_idx = 55,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 43,
		.cond_nums = 0 }
	},
	/* class_tid: 5, ingress */
	[5] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 1,
	.start_tbl_idx = 74,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 45,
		.cond_nums = 0 }
	},
	/* class_tid: 6, egress */
	[6] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 1,
	.start_tbl_idx = 75,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 45,
		.cond_nums = 0 }
	},
	/* class_tid: 7, ingress */
	[7] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 1,
	.start_tbl_idx = 76,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 45,
		.cond_nums = 0 }
	},
	/* class_tid: 8, ingress */
	[8] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 1,
	.start_tbl_idx = 77,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 45,
		.cond_nums = 0 }
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_wh_plus_class_tbl_list[] = {
	{ /* class_tid: 1, , table: l2_cntxt_tcam_cache.rd */
	.description = "l2_cntxt_tcam_cache.rd",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 5,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 2,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 0,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 0,
	.ident_nums = 1
	},
	{ /* class_tid: 1, , table: mac_addr_cache.rd */
	.description = "mac_addr_cache.rd",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 3,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 1,
	.blob_key_bit_size = 161,
	.key_bit_size = 161,
	.key_num_fields = 8,
	.ident_start_idx = 1,
	.ident_nums = 1
	},
	{ /* class_tid: 1, , table: control.0 */
	.description = "control.0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 3,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* class_tid: 1, , table: l2_cntxt_tcam.0 */
	.description = "l2_cntxt_tcam.0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 4,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 9,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 0,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.ident_start_idx = 2,
	.ident_nums = 1
	},
	{ /* class_tid: 1, , table: mac_addr_cache.wr */
	.description = "mac_addr_cache.wr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 4,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 22,
	.blob_key_bit_size = 161,
	.key_bit_size = 161,
	.key_num_fields = 8,
	.result_start_idx = 13,
	.result_bit_size = 69,
	.result_num_fields = 5
	},
	{ /* class_tid: 1, , table: profile_tcam_cache.rd */
	.description = "profile_tcam_cache.rd",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 4,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 30,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.ident_start_idx = 3,
	.ident_nums = 3
	},
	{ /* class_tid: 1, , table: control.1 */
	.description = "control.1",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 2,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 4,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* class_tid: 1, , table: control.2 */
	.description = "control.2",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 5,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 5,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.func_info = {
		.func_opc = BNXT_ULP_FUNC_OPC_EQ,
		.func_src1 = BNXT_ULP_FUNC_SRC_REGFILE,
		.func_opr1 = BNXT_ULP_RF_IDX_FLOW_SIG_ID,
		.func_src2 = BNXT_ULP_FUNC_SRC_COMP_FIELD,
		.func_opr2 = BNXT_ULP_CF_IDX_FLOW_SIG_ID,
		.func_dst_opr = BNXT_ULP_RF_IDX_CC }
	},
	{ /* class_tid: 1, , table: profile_tcam.ipv4 */
	.description = "profile_tcam.ipv4",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 3,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 6,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 33,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 18,
	.result_bit_size = 38,
	.result_num_fields = 17,
	.ident_start_idx = 6,
	.ident_nums = 1
	},
	{ /* class_tid: 1, , table: profile_tcam.ipv6 */
	.description = "profile_tcam.ipv6",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 8,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 76,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 35,
	.result_bit_size = 38,
	.result_num_fields = 17,
	.ident_start_idx = 7,
	.ident_nums = 1
	},
	{ /* class_tid: 1, , table: profile_tcam.ipv4_vxlan */
	.description = "profile_tcam.ipv4_vxlan",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 10,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 119,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 52,
	.result_bit_size = 38,
	.result_num_fields = 17,
	.ident_start_idx = 8,
	.ident_nums = 1
	},
	{ /* class_tid: 1, , table: profile_tcam_cache.wr */
	.description = "profile_tcam_cache.wr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 12,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 162,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.result_start_idx = 69,
	.result_bit_size = 122,
	.result_num_fields = 5
	},
	{ /* class_tid: 1, , table: em.ipv4 */
	.description = "em.ipv4",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 12,
		.cond_nums = 3 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.key_start_idx = 165,
	.blob_key_bit_size = 176,
	.key_bit_size = 176,
	.key_num_fields = 10,
	.result_start_idx = 74,
	.result_bit_size = 64,
	.result_num_fields = 9
	},
	{ /* class_tid: 1, , table: eem.ipv4 */
	.description = "eem.ipv4",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_EXTERNAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 15,
		.cond_nums = 3 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.key_start_idx = 175,
	.blob_key_bit_size = 448,
	.key_bit_size = 448,
	.key_num_fields = 10,
	.result_start_idx = 83,
	.result_bit_size = 64,
	.result_num_fields = 9
	},
	{ /* class_tid: 1, , table: em.ipv6 */
	.description = "em.ipv6",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 18,
		.cond_nums = 3 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.key_start_idx = 185,
	.blob_key_bit_size = 416,
	.key_bit_size = 416,
	.key_num_fields = 11,
	.result_start_idx = 92,
	.result_bit_size = 64,
	.result_num_fields = 9
	},
	{ /* class_tid: 1, , table: eem.ipv6 */
	.description = "eem.ipv6",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_EXTERNAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 21,
		.cond_nums = 3 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.key_start_idx = 196,
	.blob_key_bit_size = 448,
	.key_bit_size = 448,
	.key_num_fields = 11,
	.result_start_idx = 101,
	.result_bit_size = 64,
	.result_num_fields = 9
	},
	{ /* class_tid: 1, , table: em.vxlan */
	.description = "em.vxlan",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 24,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.key_start_idx = 207,
	.blob_key_bit_size = 200,
	.key_bit_size = 200,
	.key_num_fields = 11,
	.result_start_idx = 110,
	.result_bit_size = 64,
	.result_num_fields = 9
	},
	{ /* class_tid: 1, , table: eem.vxlan */
	.description = "eem.vxlan",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_EXTERNAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 25,
		.cond_nums = 0 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.key_start_idx = 218,
	.blob_key_bit_size = 448,
	.key_bit_size = 448,
	.key_num_fields = 11,
	.result_start_idx = 119,
	.result_bit_size = 64,
	.result_num_fields = 9
	},
	{ /* class_tid: 2, , table: l2_cntxt_tcam_cache.rd */
	.description = "l2_cntxt_tcam_cache.rd",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 5,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 27,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 229,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 9,
	.ident_nums = 1
	},
	{ /* class_tid: 2, , table: mac_addr_cache.rd */
	.description = "mac_addr_cache.rd",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 28,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 230,
	.blob_key_bit_size = 161,
	.key_bit_size = 161,
	.key_num_fields = 8,
	.ident_start_idx = 10,
	.ident_nums = 1
	},
	{ /* class_tid: 2, , table: control.0 */
	.description = "control.0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 28,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* class_tid: 2, , table: l2_cntxt_tcam.0 */
	.description = "l2_cntxt_tcam.0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 29,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 238,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 128,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.ident_start_idx = 11,
	.ident_nums = 1
	},
	{ /* class_tid: 2, , table: mac_addr_cache.wr */
	.description = "mac_addr_cache.wr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 29,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 251,
	.blob_key_bit_size = 161,
	.key_bit_size = 161,
	.key_num_fields = 8,
	.result_start_idx = 141,
	.result_bit_size = 69,
	.result_num_fields = 5
	},
	{ /* class_tid: 2, , table: profile_tcam_cache.rd */
	.description = "profile_tcam_cache.rd",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 29,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 259,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.ident_start_idx = 12,
	.ident_nums = 3
	},
	{ /* class_tid: 2, , table: control.gen_tbl_miss */
	.description = "control.gen_tbl_miss",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 2,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 29,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* class_tid: 2, , table: control.conflict_check */
	.description = "control.conflict_check",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 4,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 30,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.func_info = {
		.func_opc = BNXT_ULP_FUNC_OPC_EQ,
		.func_src1 = BNXT_ULP_FUNC_SRC_REGFILE,
		.func_opr1 = BNXT_ULP_RF_IDX_FLOW_SIG_ID,
		.func_src2 = BNXT_ULP_FUNC_SRC_COMP_FIELD,
		.func_opr2 = BNXT_ULP_CF_IDX_FLOW_SIG_ID,
		.func_dst_opr = BNXT_ULP_RF_IDX_CC }
	},
	{ /* class_tid: 2, , table: profile_tcam.ipv4 */
	.description = "profile_tcam.ipv4",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 2,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 31,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 262,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 146,
	.result_bit_size = 38,
	.result_num_fields = 17,
	.ident_start_idx = 15,
	.ident_nums = 1
	},
	{ /* class_tid: 2, , table: profile_tcam.ipv6 */
	.description = "profile_tcam.ipv6",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 32,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 305,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 163,
	.result_bit_size = 38,
	.result_num_fields = 17,
	.ident_start_idx = 16,
	.ident_nums = 1
	},
	{ /* class_tid: 2, , table: profile_tcam_cache.wr */
	.description = "profile_tcam_cache.wr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 32,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 348,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.result_start_idx = 180,
	.result_bit_size = 122,
	.result_num_fields = 5
	},
	{ /* class_tid: 2, , table: em.ipv4 */
	.description = "em.ipv4",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 32,
		.cond_nums = 2 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.key_start_idx = 351,
	.blob_key_bit_size = 176,
	.key_bit_size = 176,
	.key_num_fields = 10,
	.result_start_idx = 185,
	.result_bit_size = 64,
	.result_num_fields = 9
	},
	{ /* class_tid: 2, , table: eem.ipv4 */
	.description = "eem.ipv4",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_EXTERNAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 34,
		.cond_nums = 2 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.key_start_idx = 361,
	.blob_key_bit_size = 448,
	.key_bit_size = 448,
	.key_num_fields = 10,
	.result_start_idx = 194,
	.result_bit_size = 64,
	.result_num_fields = 9
	},
	{ /* class_tid: 2, , table: em.ipv6 */
	.description = "em.ipv6",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 36,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.key_start_idx = 371,
	.blob_key_bit_size = 416,
	.key_bit_size = 416,
	.key_num_fields = 11,
	.result_start_idx = 203,
	.result_bit_size = 64,
	.result_num_fields = 9
	},
	{ /* class_tid: 2, , table: eem.ipv6 */
	.description = "eem.ipv6",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_EXTERNAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 37,
		.cond_nums = 0 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_PUSH_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.key_start_idx = 382,
	.blob_key_bit_size = 448,
	.key_bit_size = 448,
	.key_num_fields = 11,
	.result_start_idx = 212,
	.result_bit_size = 64,
	.result_num_fields = 9
	},
	{ /* class_tid: 3, , table: int_full_act_record.ing_0 */
	.description = "int_full_act_record.ing_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 37,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.track_type = CFA_TRACK_TYPE_SID,
	.result_start_idx = 221,
	.result_bit_size = 128,
	.result_num_fields = 26
	},
	{ /* class_tid: 3, , table: l2_cntxt_tcam_cache.ing_rd */
	.description = "l2_cntxt_tcam_cache.ing_rd",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 37,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 393,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 17,
	.ident_nums = 0
	},
	{ /* class_tid: 3, , table: control.ing_0 */
	.description = "control.ing_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 37,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* class_tid: 3, , table: l2_cntxt_tcam.ing_0 */
	.description = "l2_cntxt_tcam.ing_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 38,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 394,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 247,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.ident_start_idx = 17,
	.ident_nums = 1
	},
	{ /* class_tid: 3, , table: l2_cntxt_tcam_cache.ing_wr */
	.description = "l2_cntxt_tcam_cache.ing_wr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 38,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 407,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 260,
	.result_bit_size = 70,
	.result_num_fields = 5
	},
	{ /* class_tid: 3, , table: parif_def_lkup_arec_ptr.ing_0 */
	.description = "parif_def_lkup_arec_ptr.ing_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_LKUP_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 38,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD,
	.tbl_operand = BNXT_ULP_CF_IDX_PHY_PORT_PARIF,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 265,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* class_tid: 3, , table: parif_def_arec_ptr.ing_0 */
	.description = "parif_def_arec_ptr.ing_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 38,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD,
	.tbl_operand = BNXT_ULP_CF_IDX_PHY_PORT_PARIF,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 266,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* class_tid: 3, , table: parif_def_err_arec_ptr.ing_0 */
	.description = "parif_def_err_arec_ptr.ing_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_ERR_ACT_REC_PTR,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 38,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD,
	.tbl_operand = BNXT_ULP_CF_IDX_PHY_PORT_PARIF,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 267,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* class_tid: 3, , table: control.egr_0 */
	.description = "control.egr_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 6,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 38,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* class_tid: 3, , table: int_full_act_record.egr_vfr */
	.description = "int_full_act_record.egr_vfr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_VFR_CFA_ACTION,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 39,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.track_type = CFA_TRACK_TYPE_SID,
	.result_start_idx = 268,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0
	},
	{ /* class_tid: 3, , table: l2_cntxt_tcam_cache.egr_rd_vfr */
	.description = "l2_cntxt_tcam_cache.egr_rd_vfr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 39,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 408,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 18,
	.ident_nums = 0
	},
	{ /* class_tid: 3, , table: control.egr_1 */
	.description = "control.egr_1",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 39,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* class_tid: 3, , table: l2_cntxt_tcam_bypass.egr_vfr */
	.description = "l2_cntxt_tcam_bypass.egr_vfr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 40,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 409,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 294,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.ident_start_idx = 18,
	.ident_nums = 0
	},
	{ /* class_tid: 3, , table: l2_cntxt_tcam_cache.egr_wr_vfr */
	.description = "l2_cntxt_tcam_cache.egr_wr_vfr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 40,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 422,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 307,
	.result_bit_size = 70,
	.result_num_fields = 5
	},
	{ /* class_tid: 3, , table: l2_cntxt_tcam_cache.rd */
	.description = "l2_cntxt_tcam_cache.rd",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 40,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 423,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 18,
	.ident_nums = 0
	},
	{ /* class_tid: 3, , table: control.egr_2 */
	.description = "control.egr_2",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 40,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* class_tid: 3, , table: l2_cntxt_tcam.egr_0 */
	.description = "l2_cntxt_tcam.egr_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 41,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 424,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 312,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.ident_start_idx = 18,
	.ident_nums = 1
	},
	{ /* class_tid: 3, , table: l2_cntxt_tcam_cache.egr_wr */
	.description = "l2_cntxt_tcam_cache.egr_wr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 41,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 437,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 325,
	.result_bit_size = 70,
	.result_num_fields = 5
	},
	{ /* class_tid: 3, , table: int_full_act_record.egr_0 */
	.description = "int_full_act_record.egr_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_VFR_CFA_ACTION,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 43,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.track_type = CFA_TRACK_TYPE_SID,
	.result_start_idx = 330,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0
	},
	{ /* class_tid: 3, , table: parif_def_lkup_arec_ptr.egr_0 */
	.description = "parif_def_lkup_arec_ptr.egr_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_LKUP_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 43,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD,
	.tbl_operand = BNXT_ULP_CF_IDX_DRV_FUNC_PARIF,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 356,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* class_tid: 3, , table: parif_def_arec_ptr.egr_0 */
	.description = "parif_def_arec_ptr.egr_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 43,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD,
	.tbl_operand = BNXT_ULP_CF_IDX_DRV_FUNC_PARIF,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 357,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* class_tid: 3, , table: parif_def_err_arec_ptr.egr_0 */
	.description = "parif_def_err_arec_ptr.egr_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_ERR_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 43,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD,
	.tbl_operand = BNXT_ULP_CF_IDX_DRV_FUNC_PARIF,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 358,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* class_tid: 4, , table: int_full_act_record.loopback */
	.description = "int_full_act_record.loopback",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_VFR_CFA_ACTION,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 43,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.track_type = CFA_TRACK_TYPE_SID,
	.result_start_idx = 359,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0
	},
	{ /* class_tid: 4, , table: l2_cntxt_tcam_cache.vf_rd_egr */
	.description = "l2_cntxt_tcam_cache.vf_rd_egr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 43,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 438,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 19,
	.ident_nums = 0
	},
	{ /* class_tid: 4, , table: control.vf_0 */
	.description = "control.vf_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 43,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* class_tid: 4, , table: l2_cntxt_tcam.vf_egr */
	.description = "l2_cntxt_tcam.vf_egr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 44,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 439,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 385,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.ident_start_idx = 19,
	.ident_nums = 1
	},
	{ /* class_tid: 4, , table: l2_cntxt_tcam_cache.vf_egr_wr */
	.description = "l2_cntxt_tcam_cache.vf_egr_wr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 44,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 452,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 398,
	.result_bit_size = 70,
	.result_num_fields = 5
	},
	{ /* class_tid: 4, , table: parif_def_lkup_arec_ptr.vf_egr */
	.description = "parif_def_lkup_arec_ptr.vf_egr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_LKUP_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 44,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_IF_TBL_OPC_WR_CONST,
	.tbl_operand = ULP_WP_SYM_LOOPBACK_PARIF,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 403,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* class_tid: 4, , table: parif_def_arec_ptr.vf_egr */
	.description = "parif_def_arec_ptr.vf_egr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 44,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_IF_TBL_OPC_WR_CONST,
	.tbl_operand = ULP_WP_SYM_LOOPBACK_PARIF,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 404,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* class_tid: 4, , table: parif_def_err_arec_ptr.vf_egr */
	.description = "parif_def_err_arec_ptr.vf_egr",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_ERR_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 44,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_IF_TBL_OPC_WR_CONST,
	.tbl_operand = ULP_WP_SYM_LOOPBACK_PARIF,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 405,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* class_tid: 4, , table: int_full_act_record.vf_ing */
	.description = "int_full_act_record.vf_ing",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 44,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_PUSH_AND_SET_VFR_FLAG,
	.track_type = CFA_TRACK_TYPE_SID,
	.result_start_idx = 406,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0
	},
	{ /* class_tid: 4, , table: l2_cntxt_tcam_bypass.vf_ing */
	.description = "l2_cntxt_tcam_bypass.vf_ing",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 44,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 453,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 432,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.ident_start_idx = 20,
	.ident_nums = 0
	},
	{ /* class_tid: 4, , table: l2_cntxt_tcam_cache.vfr_rd_egr0 */
	.description = "l2_cntxt_tcam_cache.vfr_rd_egr0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 44,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 466,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 20,
	.ident_nums = 0
	},
	{ /* class_tid: 4, , table: control.vfr_0 */
	.description = "control.vfr_0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 44,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* class_tid: 4, , table: l2_cntxt_tcam_bypass.vfr_egr0 */
	.description = "l2_cntxt_tcam_bypass.vfr_egr0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 45,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 467,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 445,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.ident_start_idx = 20,
	.ident_nums = 0
	},
	{ /* class_tid: 4, , table: l2_cntxt_tcam_cache.vfr_wr_egr0 */
	.description = "l2_cntxt_tcam_cache.vfr_wr_egr0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 45,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 480,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 458,
	.result_bit_size = 70,
	.result_num_fields = 5
	},
	{ /* class_tid: 4, , table: int_vtag_encap_record.vfr_egr0 */
	.description = "int_vtag_encap_record.vfr_egr0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_8B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 45,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.track_type = CFA_TRACK_TYPE_SID,
	.record_size = 8,
	.result_start_idx = 463,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 11
	},
	{ /* class_tid: 4, , table: int_full_act_record.vfr_egr0 */
	.description = "int_full_act_record.vfr_egr0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_VFR_CFA_ACTION,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 45,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.track_type = CFA_TRACK_TYPE_SID,
	.result_start_idx = 474,
	.result_bit_size = 128,
	.result_num_fields = 26
	},
	{ /* class_tid: 4, , table: int_full_act_record.vfr_ing0 */
	.description = "int_full_act_record.vfr_ing0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 45,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.track_type = CFA_TRACK_TYPE_SID,
	.result_start_idx = 500,
	.result_bit_size = 128,
	.result_num_fields = 26
	},
	{ /* class_tid: 4, , table: l2_cntxt_tcam_bypass.vfr_dtagged_ing0 */
	.description = "l2_cntxt_tcam_bypass.vfr_dtagged_ing0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 45,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 481,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 526,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.ident_start_idx = 20,
	.ident_nums = 0
	},
	{ /* class_tid: 4, , table: l2_cntxt_tcam_bypass.vfr_stagged_ing0 */
	.description = "l2_cntxt_tcam_bypass.vfr_stagged_ing0",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 45,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.track_type = CFA_TRACK_TYPE_SID,
	.key_start_idx = 494,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 539,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.ident_start_idx = 20,
	.ident_nums = 0
	},
	{ /* class_tid: 5, , table: control.reject */
	.description = "control.reject",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.true_message = "Reject: wh+ not supporting mirror-op template",
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 45,
		.cond_nums = 0 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* class_tid: 6, , table: control.reject */
	.description = "control.reject",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.true_message = "Reject: wh+ not supporting mirror-op template",
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 45,
		.cond_nums = 0 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* class_tid: 7, , table: control.reject */
	.description = "control.reject",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.true_message = "Reject: wh+ not supporting L2 filter template",
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 45,
		.cond_nums = 0 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* class_tid: 8, , table: control.reject */
	.description = "control.reject",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.true_message = "Reject: wh+ not supporting pf veb filter template",
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 45,
		.cond_nums = 0 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	}
};

struct bnxt_ulp_mapper_cond_list_info ulp_wh_plus_class_cond_oper_list[] = {
};

struct bnxt_ulp_mapper_cond_info ulp_wh_plus_class_cond_list[] = {
	/* cond_reject: wh_plus, class_tid: 1 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_WC_MATCH
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_BTH
	},
	/* cond_execute: class_tid: 1, l2_cntxt_tcam_cache.rd:2*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_FIELD_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_GLB_HF_ID_O_ETH_DMAC
	},
	/* cond_execute: class_tid: 1, control.0:3*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 1, control.1:4*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 1, control.2:5*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_CC
	},
	/* cond_execute: class_tid: 1, profile_tcam.ipv4:6*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: class_tid: 1, profile_tcam.ipv6:8*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV6
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: class_tid: 1, profile_tcam.ipv4_vxlan:10*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	/* cond_execute: class_tid: 1, em.ipv4:12*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET,
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: class_tid: 1, eem.ipv4:15*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_IS_SET,
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: class_tid: 1, em.ipv6:18*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET,
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV6
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: class_tid: 1, eem.ipv6:21*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_IS_SET,
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV6
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: class_tid: 1, em.vxlan:24*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET,
	},
	/* cond_reject: wh_plus, class_tid: 2 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_WC_MATCH
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_BTH
	},
	/* cond_execute: class_tid: 2, l2_cntxt_tcam_cache.rd:27*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_FIELD_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_GLB_HF_ID_O_ETH_SMAC
	},
	/* cond_execute: class_tid: 2, control.0:28*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 2, control.gen_tbl_miss:29*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 2, control.conflict_check:30*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_CC
	},
	/* cond_execute: class_tid: 2, profile_tcam.ipv4:31*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	/* cond_execute: class_tid: 2, em.ipv4:32*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET,
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	/* cond_execute: class_tid: 2, eem.ipv4:34*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_IS_SET,
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	/* cond_execute: class_tid: 2, em.ipv6:36*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET,
	},
	/* cond_execute: class_tid: 3, control.ing_0:37*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 3, control.egr_0:38*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_VFR_MODE
	},
	/* cond_execute: class_tid: 3, control.egr_1:39*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 3, control.egr_2:40*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 3, l2_cntxt_tcam_cache.egr_wr:41*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_NOT_SET,
	.cond_operand = BNXT_ULP_CF_IDX_VFR_MODE
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 4, control.vf_0:43*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 4, control.vfr_0:44*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	}
};

struct bnxt_ulp_mapper_key_info ulp_wh_plus_class_key_info_list[] = {
	/* class_tid: 1, , table: l2_cntxt_tcam_cache.rd */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	/* class_tid: 1, , table: mac_addr_cache.rd */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_ONE_VTAG >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_ONE_VTAG & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
		(BNXT_ULP_GLB_HF_ID_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_OO_VLAN_VID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
		(BNXT_ULP_GLB_HF_ID_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_OO_VLAN_VID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_filter_id",
		.field_bit_size = 64,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_filter_id",
		.field_bit_size = 64,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	/* class_tid: 1, , table: l2_cntxt_tcam.0 */
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
		(BNXT_ULP_GLB_HF_ID_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_OO_VLAN_VID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
		(BNXT_ULP_GLB_HF_ID_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_OO_VLAN_VID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_VTAG_NUM >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_VTAG_NUM & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 1, , table: mac_addr_cache.wr */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_ONE_VTAG >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_ONE_VTAG & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
		(BNXT_ULP_GLB_HF_ID_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_OO_VLAN_VID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
		(BNXT_ULP_GLB_HF_ID_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_OO_VLAN_VID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_filter_id",
		.field_bit_size = 64,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_filter_id",
		.field_bit_size = 64,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	/* class_tid: 1, , table: profile_tcam_cache.rd */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 1, , table: profile_tcam.ipv4 */
	{
	.field_info_mask = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_ONES,
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
		ULP_WP_SYM_L4_HDR_TYPE_TCP},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		ULP_WP_SYM_L4_HDR_TYPE_UDP}
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_L3_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_ONE_VTAG >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_ONE_VTAG & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_L2_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "reserved",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "reserved",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 1, , table: profile_tcam.ipv6 */
	{
	.field_info_mask = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_ONES,
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
		ULP_WP_SYM_L4_HDR_TYPE_TCP},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		ULP_WP_SYM_L4_HDR_TYPE_UDP}
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_L3_HDR_TYPE_IPV6}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_L3_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_ONE_VTAG >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_ONE_VTAG & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_L2_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "reserved",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "reserved",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 1, , table: profile_tcam.ipv4_vxlan */
	{
	.field_info_mask = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_L4_HDR_TYPE_UDP}
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_TUN_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_TL4_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_TL3_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_TL2_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "reserved",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "reserved",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 1, , table: profile_tcam_cache.wr */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 1, , table: em.ipv4 */
	{
	.field_info_mask = {
		.description = "spare",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spare",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_DST_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_SRC_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_PROTO_ID & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L3_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L3_PROTO_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dst",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.dst",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.src",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.src",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2.smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		},
	.field_info_spec = {
		.description = "l2.smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
		}
	},
	/* class_tid: 1, , table: eem.ipv4 */
	{
	.field_info_mask = {
		.description = "spare",
		.field_bit_size = 275,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spare",
		.field_bit_size = 275,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_DST_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_SRC_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_PROTO_ID & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L3_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L3_PROTO_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dst",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.dst",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.src",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.src",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2.smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		},
	.field_info_spec = {
		.description = "l2.smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
		}
	},
	/* class_tid: 1, , table: em.ipv6 */
	{
	.field_info_mask = {
		.description = "spare",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spare",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_DST_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_SRC_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_PROTO_ID & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L3_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L3_PROTO_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dst",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.dst",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.src",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.src",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2.smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		},
	.field_info_spec = {
		.description = "l2.smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2.dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2.dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
		}
	},
	/* class_tid: 1, , table: eem.ipv6 */
	{
	.field_info_mask = {
		.description = "spare",
		.field_bit_size = 35,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spare",
		.field_bit_size = 35,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_DST_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_SRC_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_PROTO_ID & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L3_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L3_PROTO_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dst",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.dst",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.src",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.src",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2.smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		},
	.field_info_spec = {
		.description = "l2.smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2.dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2.dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
		}
	},
	/* class_tid: 1, , table: em.vxlan */
	{
	.field_info_mask = {
		.description = "spare",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spare",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		(4789 >> 8) & 0xff,
		4789 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		17}
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dst",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "tl3.dst",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.src",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3.src",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2.src",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2.src",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
		}
	},
	/* class_tid: 1, , table: eem.vxlan */
	{
	.field_info_mask = {
		.description = "spare",
		.field_bit_size = 251,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spare",
		.field_bit_size = 251,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		(4789 >> 8) & 0xff,
		4789 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		17}
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dst",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "tl3.dst",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.src",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3.src",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2.src",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2.src",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
		}
	},
	/* class_tid: 2, , table: l2_cntxt_tcam_cache.rd */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	/* class_tid: 2, , table: mac_addr_cache.rd */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_ONE_VTAG >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_ONE_VTAG & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
		(BNXT_ULP_GLB_HF_ID_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_OO_VLAN_VID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
		(BNXT_ULP_GLB_HF_ID_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_OO_VLAN_VID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_filter_id",
		.field_bit_size = 64,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_filter_id",
		.field_bit_size = 64,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	/* class_tid: 2, , table: l2_cntxt_tcam.0 */
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
		(BNXT_ULP_GLB_HF_ID_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_OO_VLAN_VID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
		(BNXT_ULP_GLB_HF_ID_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_OO_VLAN_VID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_VTAG_NUM >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_VTAG_NUM & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 2, , table: mac_addr_cache.wr */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_ONE_VTAG >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_ONE_VTAG & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
		(BNXT_ULP_GLB_HF_ID_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_OO_VLAN_VID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
		(BNXT_ULP_GLB_HF_ID_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_OO_VLAN_VID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_filter_id",
		.field_bit_size = 64,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_filter_id",
		.field_bit_size = 64,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	/* class_tid: 2, , table: profile_tcam_cache.rd */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 2, , table: profile_tcam.ipv4 */
	{
	.field_info_mask = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_ONES,
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
		ULP_WP_SYM_L4_HDR_TYPE_TCP},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		ULP_WP_SYM_L4_HDR_TYPE_UDP}
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_L3_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_ONE_VTAG >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_ONE_VTAG & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_L2_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "reserved",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "reserved",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 2, , table: profile_tcam.ipv6 */
	{
	.field_info_mask = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_ONES,
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
		ULP_WP_SYM_L4_HDR_TYPE_TCP},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		ULP_WP_SYM_L4_HDR_TYPE_UDP}
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_L3_HDR_TYPE_IPV6}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_L3_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_ONE_VTAG >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_ONE_VTAG & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_WP_SYM_L2_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "reserved",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "reserved",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 2, , table: profile_tcam_cache.wr */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 2, , table: em.ipv4 */
	{
	.field_info_mask = {
		.description = "spare",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spare",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_DST_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_SRC_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_PROTO_ID & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L3_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L3_PROTO_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dst",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.dst",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.src",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.src",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2.dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		},
	.field_info_spec = {
		.description = "l2.dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
		}
	},
	/* class_tid: 2, , table: eem.ipv4 */
	{
	.field_info_mask = {
		.description = "spare",
		.field_bit_size = 275,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spare",
		.field_bit_size = 275,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_DST_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_SRC_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_PROTO_ID & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L3_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L3_PROTO_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dst",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.dst",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.src",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.src",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2.dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		},
	.field_info_spec = {
		.description = "l2.dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
		}
	},
	/* class_tid: 2, , table: em.ipv6 */
	{
	.field_info_mask = {
		.description = "spare",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spare",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_DST_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_SRC_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_PROTO_ID & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L3_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L3_PROTO_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dst",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.dst",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.src",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.src",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2.smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2.smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2.dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		},
	.field_info_spec = {
		.description = "l2.dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
		}
	},
	/* class_tid: 2, , table: eem.ipv6 */
	{
	.field_info_mask = {
		.description = "spare",
		.field_bit_size = 35,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spare",
		.field_bit_size = 35,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "local_cos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_DST_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L4_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L4_SRC_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_PROTO_ID & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr2 = {
		(BNXT_ULP_CF_IDX_O_L3_PROTO_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_L3_PROTO_ID & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dst",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.dst",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.src",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3.src",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2.smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2.smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2.dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		},
	.field_info_spec = {
		.description = "l2.dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
		}
	},
	/* class_tid: 3, , table: l2_cntxt_tcam_cache.ing_rd */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_PHY_PORT_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_PHY_PORT_SVIF & 0xff}
		}
	},
	/* class_tid: 3, , table: l2_cntxt_tcam.ing_0 */
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_PHY_PORT_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_PHY_PORT_SVIF & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 3, , table: l2_cntxt_tcam_cache.ing_wr */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_PHY_PORT_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_PHY_PORT_SVIF & 0xff}
		}
	},
	/* class_tid: 3, , table: l2_cntxt_tcam_cache.egr_rd_vfr */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff}
		}
	},
	/* class_tid: 3, , table: l2_cntxt_tcam_bypass.egr_vfr */
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 3, , table: l2_cntxt_tcam_cache.egr_wr_vfr */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff}
		}
	},
	/* class_tid: 3, , table: l2_cntxt_tcam_cache.rd */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff}
		}
	},
	/* class_tid: 3, , table: l2_cntxt_tcam.egr_0 */
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 3, , table: l2_cntxt_tcam_cache.egr_wr */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff}
		}
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_cache.vf_rd_egr */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_VF_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_VF_FUNC_SVIF & 0xff}
		}
	},
	/* class_tid: 4, , table: l2_cntxt_tcam.vf_egr */
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_VF_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_VF_FUNC_SVIF & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_cache.vf_egr_wr */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_VF_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_VF_FUNC_SVIF & 0xff}
		}
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_bypass.vf_ing */
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_VF_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_VF_FUNC_SVIF & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_cache.vfr_rd_egr0 */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff}
		}
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_bypass.vfr_egr0 */
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_cache.vfr_wr_egr0 */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff}
		}
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_bypass.vfr_dtagged_ing0 */
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DEV_PORT_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DEV_PORT_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		2}
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_bypass.vfr_stagged_ing0 */
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DEV_PORT_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DEV_PORT_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "sparif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_num_vtags",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "key_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	}
};

struct bnxt_ulp_mapper_field_info ulp_wh_plus_class_key_ext_list[] = {
};

struct bnxt_ulp_mapper_field_info ulp_wh_plus_class_result_field_list[] = {
	/* class_tid: 1, , table: l2_cntxt_tcam.0 */
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "prof_func_id",
	.field_bit_size = 7,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff}
	},
	{
	.description = "l2_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_PHY_PORT_PARIF >> 8) & 0xff,
	BNXT_ULP_CF_IDX_PHY_PORT_PARIF & 0xff}
	},
	{
	.description = "allowed_pri",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_tpid",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_tpid",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "bd_act_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "byp_sp_lkup",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pri_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tpid_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, , table: mac_addr_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "prof_func_id",
	.field_bit_size = 7,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, , table: profile_tcam.ipv4 */
	{
	.description = "wc_key_id",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.0",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_mask.1",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
	},
	{
	.description = "em_key_mask.2",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR & 0xff}
	},
	{
	.description = "em_key_mask.3",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
	},
	{
	.description = "em_key_mask.4",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_IPV4_PROTO_ID >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_IPV4_PROTO_ID & 0xff}
	},
	{
	.description = "em_key_mask.5",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2_OR_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
	BNXT_ULP_CF_IDX_O_L4 & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr2 = {
	(BNXT_ULP_GLB_HF_ID_O_UDP_SRC_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_UDP_SRC_PORT & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr3 = {
	(BNXT_ULP_GLB_HF_ID_O_TCP_SRC_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_TCP_SRC_PORT & 0xff}
	},
	{
	.description = "em_key_mask.6",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2_OR_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
	BNXT_ULP_CF_IDX_O_L4 & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr2 = {
	(BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr3 = {
	(BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT & 0xff}
	},
	{
	.description = "em_key_mask.7",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.8",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.9",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	3}
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "em_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pl_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, , table: profile_tcam.ipv6 */
	{
	.description = "wc_key_id",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.0",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_mask.1",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.2",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_ETH_SMAC >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_ETH_SMAC & 0xff}
	},
	{
	.description = "em_key_mask.3",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR & 0xff}
	},
	{
	.description = "em_key_mask.4",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR & 0xff}
	},
	{
	.description = "em_key_mask.5",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_IPV6_PROTO_ID >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_IPV6_PROTO_ID & 0xff}
	},
	{
	.description = "em_key_mask.6",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2_OR_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
	BNXT_ULP_CF_IDX_O_L4 & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr2 = {
	(BNXT_ULP_GLB_HF_ID_O_UDP_SRC_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_UDP_SRC_PORT & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr3 = {
	(BNXT_ULP_GLB_HF_ID_O_TCP_SRC_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_TCP_SRC_PORT & 0xff}
	},
	{
	.description = "em_key_mask.7",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2_OR_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
	BNXT_ULP_CF_IDX_O_L4 & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr2 = {
	(BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr3 = {
	(BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT & 0xff}
	},
	{
	.description = "em_key_mask.8",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.9",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	7}
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "em_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pl_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, , table: profile_tcam.ipv4_vxlan */
	{
	.description = "wc_key_id",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.0",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_mask.1",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.2",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.3",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.4",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_mask.5",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_mask.6",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_mask.7",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.8",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.9",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	20}
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "em_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pl_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, , table: profile_tcam_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "profile_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0 & 0xff}
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_sig_id",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_FLOW_SIG_ID >> 8) & 0xff,
	BNXT_ULP_CF_IDX_FLOW_SIG_ID & 0xff}
	},
	/* class_tid: 1, , table: em.ipv4 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 33,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "ext_flow_cntr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_size",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "key_size",
	.field_bit_size = 9,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	3}
	},
	{
	.description = "l1_cacheable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, , table: eem.ipv4 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 33,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "ext_flow_cntr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_size",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ACTION_REC_SIZE >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ACTION_REC_SIZE & 0xff}
	},
	{
	.description = "key_size",
	.field_bit_size = 9,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(173 >> 8) & 0xff,
	173 & 0xff}
	},
	{
	.description = "reserved",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	3}
	},
	{
	.description = "l1_cacheable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, , table: em.ipv6 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 33,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "ext_flow_cntr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_size",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "key_size",
	.field_bit_size = 9,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	3}
	},
	{
	.description = "l1_cacheable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, , table: eem.ipv6 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 33,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "ext_flow_cntr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_size",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ACTION_REC_SIZE >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ACTION_REC_SIZE & 0xff}
	},
	{
	.description = "key_size",
	.field_bit_size = 9,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(413 >> 8) & 0xff,
	413 & 0xff}
	},
	{
	.description = "reserved",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	3}
	},
	{
	.description = "l1_cacheable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, , table: em.vxlan */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 33,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "ext_flow_cntr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_size",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "key_size",
	.field_bit_size = 9,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	3}
	},
	{
	.description = "l1_cacheable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, , table: eem.vxlan */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 33,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "ext_flow_cntr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_size",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ACTION_REC_SIZE >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ACTION_REC_SIZE & 0xff}
	},
	{
	.description = "key_size",
	.field_bit_size = 9,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(197 >> 8) & 0xff,
	197 & 0xff}
	},
	{
	.description = "reserved",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	3}
	},
	{
	.description = "l1_cacheable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 2, , table: l2_cntxt_tcam.0 */
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "prof_func_id",
	.field_bit_size = 7,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff}
	},
	{
	.description = "l2_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP >> 8) & 0xff,
	BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	ULP_WP_SYM_LOOPBACK_PARIF},
	.field_src3 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr3 = {
	(BNXT_ULP_CF_IDX_DRV_FUNC_PARIF >> 8) & 0xff,
	BNXT_ULP_CF_IDX_DRV_FUNC_PARIF & 0xff}
	},
	{
	.description = "allowed_pri",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_tpid",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_tpid",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "bd_act_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_SP_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_SP_PTR & 0xff}
	},
	{
	.description = "byp_sp_lkup",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pri_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tpid_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 2, , table: mac_addr_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 & 0xff}
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "prof_func_id",
	.field_bit_size = 7,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 2, , table: profile_tcam.ipv4 */
	{
	.description = "wc_key_id",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.0",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_mask.1",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
	},
	{
	.description = "em_key_mask.2",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR & 0xff}
	},
	{
	.description = "em_key_mask.3",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
	},
	{
	.description = "em_key_mask.4",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_IPV4_PROTO_ID >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_IPV4_PROTO_ID & 0xff}
	},
	{
	.description = "em_key_mask.5",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2_OR_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
	BNXT_ULP_CF_IDX_O_L4 & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr2 = {
	(BNXT_ULP_GLB_HF_ID_O_UDP_SRC_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_UDP_SRC_PORT & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr3 = {
	(BNXT_ULP_GLB_HF_ID_O_TCP_SRC_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_TCP_SRC_PORT & 0xff}
	},
	{
	.description = "em_key_mask.6",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2_OR_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
	BNXT_ULP_CF_IDX_O_L4 & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr2 = {
	(BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr3 = {
	(BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT & 0xff}
	},
	{
	.description = "em_key_mask.7",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.8",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.9",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	4}
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "em_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pl_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 2, , table: profile_tcam.ipv6 */
	{
	.description = "wc_key_id",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.0",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_mask.1",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
	},
	{
	.description = "em_key_mask.2",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.3",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_IPV6_SRC_ADDR & 0xff}
	},
	{
	.description = "em_key_mask.4",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_IPV6_DST_ADDR & 0xff}
	},
	{
	.description = "em_key_mask.5",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr1 = {
	(BNXT_ULP_GLB_HF_ID_O_IPV6_PROTO_ID >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_IPV6_PROTO_ID & 0xff}
	},
	{
	.description = "em_key_mask.6",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2_OR_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
	BNXT_ULP_CF_IDX_O_L4 & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr2 = {
	(BNXT_ULP_GLB_HF_ID_O_UDP_SRC_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_UDP_SRC_PORT & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr3 = {
	(BNXT_ULP_GLB_HF_ID_O_TCP_SRC_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_TCP_SRC_PORT & 0xff}
	},
	{
	.description = "em_key_mask.7",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_AND_SRC2_OR_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_O_L4 >> 8) & 0xff,
	BNXT_ULP_CF_IDX_O_L4 & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr2 = {
	(BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
	.field_opr3 = {
	(BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT >> 8) & 0xff,
	BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT & 0xff}
	},
	{
	.description = "em_key_mask.8",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_mask.9",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	7}
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "em_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pl_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 2, , table: profile_tcam_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "profile_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0 & 0xff}
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_sig_id",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_FLOW_SIG_ID >> 8) & 0xff,
	BNXT_ULP_CF_IDX_FLOW_SIG_ID & 0xff}
	},
	/* class_tid: 2, , table: em.ipv4 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 33,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "ext_flow_cntr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_size",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "key_size",
	.field_bit_size = 9,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	3}
	},
	{
	.description = "l1_cacheable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 2, , table: eem.ipv4 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 33,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "ext_flow_cntr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_size",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ACTION_REC_SIZE >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ACTION_REC_SIZE & 0xff}
	},
	{
	.description = "key_size",
	.field_bit_size = 9,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(173 >> 8) & 0xff,
	173 & 0xff}
	},
	{
	.description = "reserved",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	3}
	},
	{
	.description = "l1_cacheable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 2, , table: em.ipv6 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 33,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "ext_flow_cntr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_size",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "key_size",
	.field_bit_size = 9,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	3}
	},
	{
	.description = "l1_cacheable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 2, , table: eem.ipv6 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 33,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "ext_flow_cntr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "act_rec_size",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ACTION_REC_SIZE >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ACTION_REC_SIZE & 0xff}
	},
	{
	.description = "key_size",
	.field_bit_size = 9,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(413 >> 8) & 0xff,
	413 & 0xff}
	},
	{
	.description = "reserved",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	3}
	},
	{
	.description = "l1_cacheable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 3, , table: int_full_act_record.ing_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_DRV_FUNC_VNIC >> 8) & 0xff,
	BNXT_ULP_CF_IDX_DRV_FUNC_VNIC & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 3, , table: l2_cntxt_tcam.ing_0 */
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "prof_func_id",
	.field_bit_size = 7,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID & 0xff}
	},
	{
	.description = "l2_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_PHY_PORT_PARIF >> 8) & 0xff,
	BNXT_ULP_CF_IDX_PHY_PORT_PARIF & 0xff}
	},
	{
	.description = "allowed_pri",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_tpid",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_tpid",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "bd_act_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "byp_sp_lkup",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pri_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tpid_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 3, , table: l2_cntxt_tcam_cache.ing_wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 & 0xff}
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "prof_func_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 3, , table: parif_def_lkup_arec_ptr.ing_0 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	/* class_tid: 3, , table: parif_def_arec_ptr.ing_0 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	/* class_tid: 3, , table: parif_def_err_arec_ptr.ing_0 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	/* class_tid: 3, , table: int_full_act_record.egr_vfr */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_PHY_PORT_VPORT >> 8) & 0xff,
	BNXT_ULP_CF_IDX_PHY_PORT_VPORT & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 3, , table: l2_cntxt_tcam_bypass.egr_vfr */
	{
	.description = "act_record_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_LOOPBACK_PARIF}
	},
	{
	.description = "allowed_pri",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_tpid",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_tpid",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "bd_act_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "byp_sp_lkup",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pri_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tpid_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 3, , table: l2_cntxt_tcam_cache.egr_wr_vfr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 & 0xff}
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "prof_func_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 3, , table: l2_cntxt_tcam.egr_0 */
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "prof_func_id",
	.field_bit_size = 7,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID & 0xff}
	},
	{
	.description = "l2_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_DRV_FUNC_PARIF >> 8) & 0xff,
	BNXT_ULP_CF_IDX_DRV_FUNC_PARIF & 0xff}
	},
	{
	.description = "allowed_pri",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_tpid",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_tpid",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "bd_act_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "byp_sp_lkup",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pri_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tpid_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 3, , table: l2_cntxt_tcam_cache.egr_wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 & 0xff}
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "prof_func_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 3, , table: int_full_act_record.egr_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_PHY_PORT_VPORT >> 8) & 0xff,
	BNXT_ULP_CF_IDX_PHY_PORT_VPORT & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 3, , table: parif_def_lkup_arec_ptr.egr_0 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	/* class_tid: 3, , table: parif_def_arec_ptr.egr_0 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	/* class_tid: 3, , table: parif_def_err_arec_ptr.egr_0 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	/* class_tid: 4, , table: int_full_act_record.loopback */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(ULP_WP_SYM_LOOPBACK_PORT >> 8) & 0xff,
	ULP_WP_SYM_LOOPBACK_PORT & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 4, , table: l2_cntxt_tcam.vf_egr */
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "prof_func_id",
	.field_bit_size = 7,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID & 0xff}
	},
	{
	.description = "l2_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_LOOPBACK_PARIF}
	},
	{
	.description = "allowed_pri",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_tpid",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_tpid",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "bd_act_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "byp_sp_lkup",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pri_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tpid_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_cache.vf_egr_wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 & 0xff}
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "prof_func_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 4, , table: parif_def_lkup_arec_ptr.vf_egr */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR & 0xff}
	},
	/* class_tid: 4, , table: parif_def_arec_ptr.vf_egr */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR & 0xff}
	},
	/* class_tid: 4, , table: parif_def_err_arec_ptr.vf_egr */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR & 0xff}
	},
	/* class_tid: 4, , table: int_full_act_record.vf_ing */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_DRV_FUNC_VNIC >> 8) & 0xff,
	BNXT_ULP_CF_IDX_DRV_FUNC_VNIC & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_bypass.vf_ing */
	{
	.description = "act_record_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "reserved",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_pri",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_tpid",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_tpid",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "bd_act_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "byp_sp_lkup",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pri_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tpid_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_bypass.vfr_egr0 */
	{
	.description = "act_record_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_pri",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_tpid",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_tpid",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "bd_act_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "byp_sp_lkup",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pri_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tpid_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_cache.vfr_wr_egr0 */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 & 0xff}
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "prof_func_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 4, , table: int_vtag_encap_record.vfr_egr0 */
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI}
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		0x81,
		0x00}
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_DEV_PORT_ID >> 8) & 0xff,
	BNXT_ULP_CF_IDX_DEV_PORT_ID & 0xff}
	},
	/* class_tid: 4, , table: int_full_act_record.vfr_egr0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ENCAP_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ENCAP_PTR_0 & 0xff}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(ULP_WP_SYM_LOOPBACK_PORT >> 8) & 0xff,
	ULP_WP_SYM_LOOPBACK_PORT & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 4, , table: int_full_act_record.vfr_ing0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_VF_FUNC_VNIC >> 8) & 0xff,
	BNXT_ULP_CF_IDX_VF_FUNC_VNIC & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_bypass.vfr_dtagged_ing0 */
	{
	.description = "act_record_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "reserved",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_pri",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_tpid",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_tpid",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "bd_act_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "byp_sp_lkup",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pri_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tpid_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 4, , table: l2_cntxt_tcam_bypass.vfr_stagged_ing0 */
	{
	.description = "act_record_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "reserved",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_pri",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "allowed_tpid",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_tpid",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "bd_act_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "byp_sp_lkup",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "pri_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tpid_anti_spoof_ctl",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	}
};

struct bnxt_ulp_mapper_ident_info ulp_wh_plus_class_ident_list[] = {
	/* class_tid: 1, , table: l2_cntxt_tcam_cache.rd */
	{
	.description = "l2_cntxt_id",
	.regfile_idx = BNXT_ULP_RF_IDX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 42
	},
	/* class_tid: 1, , table: mac_addr_cache.rd */
	{
	.description = "l2_cntxt_id",
	.regfile_idx = BNXT_ULP_RF_IDX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 42
	},
	/* class_tid: 1, , table: l2_cntxt_tcam.0 */
	{
	.description = "l2_cntxt_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.regfile_idx = BNXT_ULP_RF_IDX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	},
	/* class_tid: 1, , table: profile_tcam_cache.rd */
	{
	.description = "em_profile_id",
	.regfile_idx = BNXT_ULP_RF_IDX_EM_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 42
	},
	{
	.description = "flow_sig_id",
	.regfile_idx = BNXT_ULP_RF_IDX_FLOW_SIG_ID,
	.ident_bit_size = 64,
	.ident_bit_pos = 58
	},
	{
	.description = "profile_tcam_index",
	.regfile_idx = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 32
	},
	/* class_tid: 1, , table: profile_tcam.ipv4 */
	{
	.description = "em_profile_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_EM_PROF,
	.regfile_idx = BNXT_ULP_RF_IDX_EM_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 28
	},
	/* class_tid: 1, , table: profile_tcam.ipv6 */
	{
	.description = "em_profile_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_EM_PROF,
	.regfile_idx = BNXT_ULP_RF_IDX_EM_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 28
	},
	/* class_tid: 1, , table: profile_tcam.ipv4_vxlan */
	{
	.description = "em_profile_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_EM_PROF,
	.regfile_idx = BNXT_ULP_RF_IDX_EM_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 28
	},
	/* class_tid: 2, , table: l2_cntxt_tcam_cache.rd */
	{
	.description = "l2_cntxt_id",
	.regfile_idx = BNXT_ULP_RF_IDX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 42
	},
	/* class_tid: 2, , table: mac_addr_cache.rd */
	{
	.description = "l2_cntxt_id",
	.regfile_idx = BNXT_ULP_RF_IDX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 42
	},
	/* class_tid: 2, , table: l2_cntxt_tcam.0 */
	{
	.description = "l2_cntxt_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.regfile_idx = BNXT_ULP_RF_IDX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	},
	/* class_tid: 2, , table: profile_tcam_cache.rd */
	{
	.description = "em_profile_id",
	.regfile_idx = BNXT_ULP_RF_IDX_EM_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 42
	},
	{
	.description = "flow_sig_id",
	.regfile_idx = BNXT_ULP_RF_IDX_FLOW_SIG_ID,
	.ident_bit_size = 64,
	.ident_bit_pos = 58
	},
	{
	.description = "profile_tcam_index",
	.regfile_idx = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 32
	},
	/* class_tid: 2, , table: profile_tcam.ipv4 */
	{
	.description = "em_profile_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_EM_PROF,
	.regfile_idx = BNXT_ULP_RF_IDX_EM_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 28
	},
	/* class_tid: 2, , table: profile_tcam.ipv6 */
	{
	.description = "em_profile_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_EM_PROF,
	.regfile_idx = BNXT_ULP_RF_IDX_EM_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 28
	},
	/* class_tid: 3, , table: l2_cntxt_tcam.ing_0 */
	{
	.description = "l2_cntxt_id_low",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT_LOW,
	.regfile_idx = BNXT_ULP_RF_IDX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	},
	/* class_tid: 3, , table: l2_cntxt_tcam.egr_0 */
	{
	.description = "l2_cntxt_id_low",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT_LOW,
	.regfile_idx = BNXT_ULP_RF_IDX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	},
	/* class_tid: 4, , table: l2_cntxt_tcam.vf_egr */
	{
	.description = "l2_cntxt_id_low",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT_LOW,
	.regfile_idx = BNXT_ULP_RF_IDX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	}
};
