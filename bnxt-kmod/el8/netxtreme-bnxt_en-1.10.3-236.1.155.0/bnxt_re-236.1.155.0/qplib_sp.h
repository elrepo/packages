/*
 * Copyright (c) 2015-2025, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: Slow Path Operators (header)
 */

#ifndef __BNXT_QPLIB_SP_H__
#define __BNXT_QPLIB_SP_H__

#define BNXT_QPLIB_RESERVED_QP_WRS	128

/* DCN query */
#define QUERY_DCN_QT_ACT_CR_MASK	\
	CREQ_QUERY_ROCE_CC_GEN2_RESP_SB_TLV_DCN_QLEVEL_TBL_ACT_CR_MASK
#define QUERY_DCN_QT_ACT_CR_SFT		\
	CREQ_QUERY_ROCE_CC_GEN2_RESP_SB_TLV_DCN_QLEVEL_TBL_ACT_CR_SFT
#define QUERY_DCN_QT_ACT_INC_CNP	\
	CREQ_QUERY_ROCE_CC_GEN2_RESP_SB_TLV_DCN_QLEVEL_TBL_ACT_INC_CNP
#define QUERY_DCN_QT_ACT_UPD_IMM	\
	CREQ_QUERY_ROCE_CC_GEN2_RESP_SB_TLV_DCN_QLEVEL_TBL_ACT_UPD_IMM
#define QUERY_DCN_QT_ACT_TR_MASK	\
	CREQ_QUERY_ROCE_CC_GEN2_RESP_SB_TLV_DCN_QLEVEL_TBL_ACT_TR_MASK
#define QUERY_DCN_QT_ACT_TR_SFT		\
	CREQ_QUERY_ROCE_CC_GEN2_RESP_SB_TLV_DCN_QLEVEL_TBL_ACT_TR_SFT
#define DCN_GET_CR(act) (((act) & QUERY_DCN_QT_ACT_CR_MASK) >> QUERY_DCN_QT_ACT_CR_SFT)
#define DCN_GET_TR(act) (((act) & QUERY_DCN_QT_ACT_TR_MASK) >> QUERY_DCN_QT_ACT_TR_SFT)
#define DCN_GET_INC_CNP(act) (((act) & QUERY_DCN_QT_ACT_INC_CNP) ? 1 : 0)
#define DCN_GET_UPD_IMM(act) (((act) & QUERY_DCN_QT_ACT_UPD_IMM) ? 1 : 0)

/* DCN modify */
#define MODIFY_MASK_DCN_QLEVEL_TBL_IDX		\
	CMDQ_MODIFY_ROCE_CC_GEN2_TLV_MODIFY_MASK_DCN_QLEVEL_TBL_IDX
#define MODIFY_MASK_DCN_QLEVEL_TBL_THR		\
	CMDQ_MODIFY_ROCE_CC_GEN2_TLV_MODIFY_MASK_DCN_QLEVEL_TBL_THR
#define MODIFY_MASK_DCN_QLEVEL_TBL_CR		\
	CMDQ_MODIFY_ROCE_CC_GEN2_TLV_MODIFY_MASK_DCN_QLEVEL_TBL_CR
#define MODIFY_MASK_DCN_QLEVEL_TBL_INC_CNP	\
	CMDQ_MODIFY_ROCE_CC_GEN2_TLV_MODIFY_MASK_DCN_QLEVEL_TBL_INC_CNP
#define MODIFY_MASK_DCN_QLEVEL_TBL_UPD_IMM	\
	CMDQ_MODIFY_ROCE_CC_GEN2_TLV_MODIFY_MASK_DCN_QLEVEL_TBL_UPD_IMM
#define MODIFY_MASK_DCN_QLEVEL_TBL_TR		\
	CMDQ_MODIFY_ROCE_CC_GEN2_TLV_MODIFY_MASK_DCN_QLEVEL_TBL_TR
#define MODIFY_DCN_QT_ACT_CR_MASK		\
	CMDQ_MODIFY_ROCE_CC_GEN2_TLV_DCN_QLEVEL_TBL_ACT_CR_MASK
#define MODIFY_DCN_QT_ACT_CR_SFT		\
	CMDQ_MODIFY_ROCE_CC_GEN2_TLV_DCN_QLEVEL_TBL_ACT_CR_SFT
#define MODIFY_DCN_QT_ACT_INC_CNP		\
	CMDQ_MODIFY_ROCE_CC_GEN2_TLV_DCN_QLEVEL_TBL_ACT_INC_CNP
#define MODIFY_DCN_QT_ACT_UPD_IMM		\
	CMDQ_MODIFY_ROCE_CC_GEN2_TLV_DCN_QLEVEL_TBL_ACT_UPD_IMM
#define MODIFY_DCN_QT_ACT_TR_MASK		\
	CMDQ_MODIFY_ROCE_CC_GEN2_TLV_DCN_QLEVEL_TBL_ACT_TR_MASK
#define MODIFY_DCN_QT_ACT_TR_SFT		\
	CMDQ_MODIFY_ROCE_CC_GEN2_TLV_DCN_QLEVEL_TBL_ACT_TR_SFT
#define DCN_SET_CR(act, cr)		\
	(((*(act)) & ~MODIFY_DCN_QT_ACT_CR_MASK) | ((cr) << MODIFY_DCN_QT_ACT_CR_SFT))
#define DCN_SET_TR(act, tr)		\
	(((*(act)) & ~MODIFY_DCN_QT_ACT_TR_MASK) | ((tr) << MODIFY_DCN_QT_ACT_TR_SFT))
#define DCN_SET_INC_CNP(act, ic)	\
	((ic) ? ((*(act)) | MODIFY_DCN_QT_ACT_INC_CNP) : ((*(act)) & ~MODIFY_DCN_QT_ACT_INC_CNP))
#define DCN_SET_UPD_IMM(act, ui)	\
	((ui) ? ((*(act)) | MODIFY_DCN_QT_ACT_UPD_IMM) : ((*(act)) & ~MODIFY_DCN_QT_ACT_UPD_IMM))

#define BNXT_RE_DCN_ENABLED(res) \
	((res)->dattr->roce_cc_tlv_en_flags & CREQ_QUERY_FUNC_RESP_SB_ROCE_CC_GEN2_TLV_EN)

#define BNXT_RE_CC_GEN1_EXT_ENABLED(res) \
	((res)->dattr->roce_cc_tlv_en_flags & CREQ_QUERY_FUNC_RESP_SB_ROCE_CC_GEN1_EXT_TLV_EN)

#define BNXT_RE_CC_GEN2_EXT_ENABLED(res) \
	((res)->dattr->roce_cc_tlv_en_flags & CREQ_QUERY_FUNC_RESP_SB_ROCE_CC_GEN2_EXT_TLV_EN)

/* Resource maximums reported by the firmware */
struct bnxt_qplib_dev_attr {
#define FW_VER_ARR_LEN			4
	u8				fw_ver[FW_VER_ARR_LEN];
	u16				max_sgid;
	u16				max_mrw;
	u32				max_qp;
#define BNXT_QPLIB_MAX_OUT_RD_ATOM     253
#define BNXT_QPLIB_MAX_IN_RD_ATOM     255
	u32				max_qp_rd_atom;
	u32				max_qp_init_rd_atom;
	u32				max_sq_wqes;
	u32				max_rq_wqes;
	u32				max_qp_sges;
	u32				max_cq;
	/* HW supports only 8K entries in PBL for Thor and Wh+.
	 * So max CQEs that can be supported per CQ is 1M for Thor and Wh+.
	 */
#define BNXT_QPLIB_MAX_CQ_WQES		0xfffff
	u32				max_cq_wqes;
	u32				max_cq_sges;
	u32				max_mr;
	u64				max_mr_size;
	u32				max_pd;
	u32				max_mw;
	u32				max_raw_ethy_qp;
	u32				max_ah;
	u32				max_fmr;
	u32				max_map_per_fmr;
	u32				max_srq;
	u32				max_srq_wqes;
	u32				max_srq_sges;
	u32				max_pkey;
	u32				max_inline_data;
	u32				l2_db_size;
	u8				tqm_alloc_reqs[MAX_TQM_ALLOC_REQ];
	u8				is_atomic;
	u8				dev_cap_ext_flags;
	u16				dev_cap_ext_flags2;
	u8				dev_cap_ext_flags_1;
	u16				roce_cc_tlv_en_flags;
	u16				dev_cap_flags;
	u64				page_size_cap;
	u32				max_dpi;
	u16                             rate_limit_min;
	u32                             rate_limit_max;
};

struct bnxt_qplib_pd {
	u32				id;
};

struct bnxt_qplib_gid {
	u8				data[16];
};

struct bnxt_qplib_gid_info {
	struct bnxt_qplib_gid gid;
	u16 vlan_id;
	u8 smac[6];
};

struct bnxt_qplib_ah {
	struct bnxt_qplib_gid		dgid;
	struct bnxt_qplib_pd		*pd;
	u32				id;
	u8				sgid_index;
	/* For Query AH if the hw table and SW table are different */
	u8				host_sgid_index;
	u8				traffic_class;
	u32				flow_label;
	u8				hop_limit;
	u8				sl;
//	u8				static_rate;	/* 0 */
	u8				dmac[6];
	u16				vlan_id;
	u8				nw_type;
	u8				enable_cc;
};

struct bnxt_qplib_mrw {
	struct bnxt_qplib_pd		*pd;
	int				type;
	u32				access_flags;
#define BNXT_QPLIB_MR_ACCESS_MASK	0xFF
#define BNXT_QPLIB_FR_PMR		0x80000000
	u32				lkey;
	u32				rkey;
#define BNXT_QPLIB_RSVD_LKEY		0xFFFFFFFF
	u64				va;
	u64				total_size;
	u32				npages;
	u64				mr_handle;
	struct bnxt_qplib_hwq		hwq;
	u16				ctx_size_sb;
};

struct bnxt_qplib_mrinfo {
	struct bnxt_qplib_mrw		*mrw;
	struct bnxt_qplib_sg_info	sg;
	u64				*ptes;
	bool				is_dma;
	bool				request_relax_order;
};

struct bnxt_qplib_frpl {
	int				max_pg_ptrs;
	struct bnxt_qplib_hwq		hwq;
};

struct bnxt_qplib_cc_param_ext {
	u64 ext_mask;
	u16 inact_th_hi;
	u16 min_delta_cnp;
	u16 init_cp;
	u8 tr_update_mode;
	u8 tr_update_cyls;
	u8 fr_rtt;
	u8 ai_rate_incr;
	u16 rr_rtt_th;
	u16 ar_cr_th;
	u16 cr_min_th;
	u8 bw_avg_weight;
	u8 cr_factor;
	u16 cr_th_max_cp;
	u8 cp_bias_en;
	u8 cp_bias;
	u8 cnp_ecn;
	u8 rtt_jitter_en;
	u16 bytes_per_usec;
	u16 cc_cr_reset_th;
	u8 cr_width;
	u8 min_quota;
	u8 max_quota;
	u8 abs_max_quota;
	u16 tr_lb;
	u8 cr_prob_fac;
	u8 tr_prob_fac;
	u16 fair_cr_th;
	u8 red_div;
	u8 cnp_ratio_th;
	u16 ai_ext_rtt;
	u8 exp_crcp_ratio;
	u8 low_rate_en;
	u16 cpcr_update_th;
	u16 ai_rtt_th1;
	u16 ai_rtt_th2;
	u16 cf_rtt_th;
	u16 sc_cr_th1; /* severe congestion cr threshold 1 */
	u16 sc_cr_th2; /* severe congestion cr threshold 2 */
	u32 l64B_per_rtt;
	u8 cc_ack_bytes;
	u16 reduce_cf_rtt_th;
	u8 random_no_red_en;
	u8 actual_cr_shift_correction_en;
};

struct bnxt_qplib_cc_param_ext2 {
	u64 ext2_mask;
	u8 idx;
	u16 thr;
	u32 cr;
	u32 tr;
	u32 cnp_inc;
	u32 upd_imm;
	u16 dcn_qlevel_tbl_thr[8];
	u32 dcn_qlevel_tbl_act[8];
};

struct bnxt_qplib_cc_param_gen1_ext {
	u64 gen1_ext_mask;
	u16 rnd_no_red_mult;
	u16 no_red_offset;
	u16 reduce2_init_cong_free_rtts_th;
	u8 reduce2_init_en;
	u8 period_adjust_count;
	u16 current_rate_threshold_1;
	u16 current_rate_threshold_2;
	u8 rate_table_idx;
	u16 rate_table_byte_quota[24];
	u8 rate_table_quota_period[24];
};

struct bnxt_qplib_cc_param_gen2_ext {
	u64 gen2_ext_mask;
	u16 cr2bw_64b_ratio;
	u8 sr2_cc_first_cnp_en;
	u8 sr2_cc_actual_cr_en;
	u16 retx_cp;
	u16 retx_tr;
	u16 retx_cr;
	u8 hw_retx_cc_reset_en;
	u16 hw_retx_reset_cc_cr_th;
};

struct bnxt_qplib_cc_param {
	u8 alt_vlan_pcp;
	u16 alt_tos_dscp;
#define BNXT_QPLIB_USER_DSCP_VALID			0x80
	u8 cnp_dscp_user;
	u8 roce_dscp_user;
	u8 cc_mode;
	u8 enable;
	u8 disable_prio_vlan_tx;
	u32 inact_th;
	u16 init_cr;
	u16 init_tr;
	u16 rtt;
	u8 g;
	u8 nph_per_state;
	u8 time_pph;
	u8 pkts_pph;
	u8 tos_ecn;
	u8 tos_dscp;
	u8 qp1_tos_dscp;
	/* To track if admin has enabled ECN explicitly */
	u8 admin_enable;
	/* Mask used while programming the configfs values */
	u32 mask;
	u8 roce_pri;
#define BNXT_QPLIB_CC_PARAM_MASK_ROCE_PRI		0x80000
	/* prev value to clear dscp table */
	u8 prev_roce_pri;
	u8 prev_alt_vlan_pcp;
	u8 prev_tos_dscp;
	u16 prev_alt_tos_dscp;
	u16 tcp_cp;
	struct bnxt_qplib_cc_param_ext cc_ext;
	struct bnxt_qplib_cc_param_ext2 cc_ext2;
	struct bnxt_qplib_cc_param_gen1_ext cc_gen1_ext;
	struct bnxt_qplib_cc_param_gen2_ext cc_gen2_ext;
};

struct bnxt_qplib_roce_stats {
	u64 to_retransmits;
	u64 seq_err_naks_rcvd;
	/* seq_err_naks_rcvd is 64 b */
	u64 max_retry_exceeded;
	/* max_retry_exceeded is 64 b */
	u64 rnr_naks_rcvd;
	/* rnr_naks_rcvd is 64 b */
	u64 missing_resp;
	u64 unrecoverable_err;
	/* unrecoverable_err is 64 b */
	u64 bad_resp_err;
	/* bad_resp_err is 64 b */
	u64 local_qp_op_err;
	/* local_qp_op_err is 64 b */
	u64 local_protection_err;
	/* local_protection_err is 64 b */
	u64 mem_mgmt_op_err;
	/* mem_mgmt_op_err is 64 b */
	u64 remote_invalid_req_err;
	/* remote_invalid_req_err is 64 b */
	u64 remote_access_err;
	/* remote_access_err is 64 b */
	u64 remote_op_err;
	/* remote_op_err is 64 b */
	u64 dup_req;
	/* dup_req is 64 b */
	u64 res_exceed_max;
	/* res_exceed_max is 64 b */
	u64 res_length_mismatch;
	/* res_length_mismatch is 64 b */
	u64 res_exceeds_wqe;
	/* res_exceeds_wqe is 64 b */
	u64 res_opcode_err;
	/* res_opcode_err is 64 b */
	u64 res_rx_invalid_rkey;
	/* res_rx_invalid_rkey is 64 b */
	u64 res_rx_domain_err;
	/* res_rx_domain_err is 64 b */
	u64 res_rx_no_perm;
	/* res_rx_no_perm is 64 b */
	u64 res_rx_range_err;
	/* res_rx_range_err is 64 b */
	u64 res_tx_invalid_rkey;
	/* res_tx_invalid_rkey is 64 b */
	u64 res_tx_domain_err;
	/* res_tx_domain_err is 64 b */
	u64 res_tx_no_perm;
	/* res_tx_no_perm is 64 b */
	u64 res_tx_range_err;
	/* res_tx_range_err is 64 b */
	u64 res_irrq_oflow;
	/* res_irrq_oflow is 64 b */
	u64 res_unsup_opcode;
	/* res_unsup_opcode is 64 b */
	u64 res_unaligned_atomic;
	/* res_unaligned_atomic is 64 b */
	u64 res_rem_inv_err;
	/* res_rem_inv_err is 64 b */
	u64 res_mem_error;
	/* res_mem_error is 64 b */
	u64 res_srq_err;
	/* res_srq_err is 64 b */
	u64 res_cmp_err;
	/* res_cmp_err is 64 b */
	u64 res_invalid_dup_rkey;
	/* res_invalid_dup_rkey is 64 b */
	u64 res_wqe_format_err;
	/* res_wqe_format_err is 64 b */
	u64 res_cq_load_err;
	/* res_cq_load_err is 64 b */
	u64 res_srq_load_err;
	/* res_srq_load_err is 64 b */
	u64 res_tx_pci_err;
	/* res_tx_pci_err is 64 b */
	u64 res_rx_pci_err;
	/* res_rx_pci_err is 64 b */
	u64 res_oos_drop_count;
	/* res_oos_drop_count */
	u64	active_qp_count_p0;
	/* port 0 active qps */
	u64	active_qp_count_p1;
	/* port 1 active qps */
	u64	active_qp_count_p2;
	/* port 2 active qps */
	u64	active_qp_count_p3;
	/* port 3 active qps */
};

struct bnxt_qplib_ext_stat {
	u64  tx_atomic_req;
	u64  tx_read_req;
	u64  tx_read_res;
	u64  tx_write_req;
	u64  tx_send_req;
	u64  tx_roce_pkts;
	u64  tx_roce_bytes;
	u64  rx_atomic_req;
	u64  rx_read_req;
	u64  rx_read_res;
	u64  rx_write_req;
	u64  rx_send_req;
	u64  rx_roce_pkts;
	u64  rx_roce_bytes;
	u64  rx_roce_good_pkts;
	u64  rx_roce_good_bytes;
	u64  rx_out_of_buffer;
	u64  rx_out_of_sequence;
	u64  tx_cnp;
	u64  rx_cnp;
	u64  rx_ecn_marked;
	u64  seq_err_naks_rcvd;
	u64  rnr_naks_rcvd;
	u64  missing_resp;
	u64  to_retransmits;
	u64  dup_req;
	u64  rx_dcn_payload_cut;
	u64  te_bypassed;
	u64  tx_dcn_cnp;
	u64  rx_dcn_cnp;
	u64  rx_payload_cut;
	u64  rx_payload_cut_ignored;
	u64  rx_dcn_cnp_ignored;
};

struct bnxt_qplib_ext_stat_v2 {
	u64	tx_atomic_req;
	u64	tx_read_req;
	u64	tx_read_res;
	u64	tx_write_req;
	u64	tx_rc_send_req;
	u64	tx_ud_send_req;
	u64	tx_cnp;
	u64	tx_roce;
	u64	tx_roce_bytes;
	u64	rx_out_of_buffer;
	u64	rx_out_of_sequence;
	u64	dup_req;
	u64	missing_resp;
	u64	seq_err_naks_rcvd;
	u64	rnr_naks_rcvd;
	u64	to_retransmits;
	u64	rx_atomic_req;
	u64	rx_read_req;
	u64	rx_read_res;
	u64	rx_write_req;
	u64	rx_rc_send;
	u64	rx_ud_send;
	u64	rx_dcn_payload_cut;
	u64	rx_ecn_marked;
	u64	rx_cnp;
	u64	rx_roce;
	u64	rx_roce_bytes;
	u64	rx_roce_good;
	u64	rx_roce_good_bytes;
};

#define BNXT_QPLIB_ACCESS_LOCAL_WRITE	(1 << 0)
#define BNXT_QPLIB_ACCESS_REMOTE_READ	(1 << 1)
#define BNXT_QPLIB_ACCESS_REMOTE_WRITE	(1 << 2)
#define BNXT_QPLIB_ACCESS_REMOTE_ATOMIC	(1 << 3)
#define BNXT_QPLIB_ACCESS_MW_BIND	(1 << 4)
#define BNXT_QPLIB_ACCESS_ZERO_BASED	(1 << 5)
#define BNXT_QPLIB_ACCESS_ON_DEMAND	(1 << 6)

int bnxt_qplib_get_sgid(struct bnxt_qplib_res *res,
			struct bnxt_qplib_sgid_tbl *sgid_tbl, int index,
			struct bnxt_qplib_gid *gid);
int bnxt_qplib_del_sgid(struct bnxt_qplib_sgid_tbl *sgid_tbl,
			struct bnxt_qplib_gid *gid, u16 vlan_id, bool update);
int bnxt_qplib_add_sgid(struct bnxt_qplib_sgid_tbl *sgid_tbl,
			struct bnxt_qplib_gid *gid, const u8 *mac, u16 vlan_id,
			bool update, u32 *index, bool is_ugid, u32 stats_ctx_id);
int bnxt_qplib_update_sgid(struct bnxt_qplib_sgid_tbl *sgid_tbl,
			   struct bnxt_qplib_gid *gid, u16 gid_idx, const u8 *smac);
int bnxt_qplib_get_dev_attr(struct bnxt_qplib_rcfw *rcfw);
int bnxt_qplib_set_func_resources(struct bnxt_qplib_res *res);
int bnxt_qplib_create_ah(struct bnxt_qplib_res *res, struct bnxt_qplib_ah *ah,
			 bool block);
int bnxt_qplib_destroy_ah(struct bnxt_qplib_res *res, struct bnxt_qplib_ah *ah,
			  bool block);
int bnxt_qplib_alloc_mrw(struct bnxt_qplib_res *res, struct bnxt_qplib_mrw *mrw);
int bnxt_qplib_dereg_mrw(struct bnxt_qplib_res *res, struct bnxt_qplib_mrw *mrw,
			 bool block);
int bnxt_qplib_reg_mr(struct bnxt_qplib_res *res,
		      struct bnxt_qplib_mrinfo *mrinfo, bool block);
int bnxt_qplib_free_mrw(struct bnxt_qplib_res *res,
			struct bnxt_qplib_mrw *mr,
			u16 ctx_size, void *ctx_sb_data);
int bnxt_qplib_alloc_fast_reg_mr(struct bnxt_qplib_res *res,
				 struct bnxt_qplib_mrw *mr, int max);
int bnxt_qplib_alloc_fast_reg_page_list(struct bnxt_qplib_res *res,
					struct bnxt_qplib_frpl *frpl, int max);
void bnxt_qplib_free_fast_reg_page_list(struct bnxt_qplib_res *res,
					struct bnxt_qplib_frpl *frpl);
int bnxt_qplib_map_tc2cos(struct bnxt_qplib_res *res, u16 *cids);
int bnxt_qplib_modify_cc(struct bnxt_qplib_res *res,
			 struct bnxt_qplib_cc_param *cc_param);
int bnxt_qplib_query_cc_param(struct bnxt_qplib_res *res,
			      struct bnxt_qplib_cc_param *cc_param);
int bnxt_qplib_set_link_aggr_mode(struct bnxt_qplib_res *res,
				  u8 aggr_mode, u8 member_port_map,
				  u8 active_port_map, bool aggr_en,
				  u32 stats_fw_id);
int bnxt_qplib_get_roce_error_stats(struct bnxt_qplib_rcfw *rcfw,
				    struct bnxt_qplib_roce_stats *stats,
				    struct bnxt_qplib_query_stats_info *sinfo);
int bnxt_qplib_qext_stat(struct bnxt_qplib_rcfw *rcfw, u32 fid,
			 struct bnxt_qplib_ext_stat *estat,
			 struct bnxt_qplib_query_stats_info *sinfo);
int bnxt_qplib_read_context(struct bnxt_qplib_rcfw *rcfw, u8 type, u32 xid,
			    u32 resp_size, void *resp_va);
void bnxt_qplib_query_version(struct bnxt_qplib_rcfw *rcfw);
int bnxt_qplib_create_flow(struct bnxt_qplib_res *res);
int bnxt_qplib_destroy_flow(struct bnxt_qplib_res *res);
int bnxt_qplib_roce_cfg(struct bnxt_qplib_rcfw *rcfw, u32 feat_cfg,
			u32 feat_enables, u32 *feat_cfg_cur);
int bnxt_qplib_alloc_stats_ext_ctx_mem(struct bnxt_qplib_rcfw *rcfw,
				       struct bnxt_qplib_stats_ext_ctx
				       *roce_stats_ext_ctx);
void bnxt_qplib_free_stats_ext_ctx_mem(struct bnxt_qplib_rcfw *rcfw,
				       struct bnxt_qplib_stats_ext_ctx
				       *roce_stats_ext_ctx);
int bnxt_qplib_alloc_stats_ext_ctx_res(struct bnxt_qplib_rcfw *rcfw,
				       struct bnxt_qplib_stats_ext_ctx *roce_stat_ctx);
int bnxt_qplib_dealloc_stats_ext_ctx_res(struct bnxt_qplib_rcfw *rcfw,
					 struct bnxt_qplib_stats_ext_ctx *roce_stat_ctx);
/* In variable wqe mode, sq_size is hwq.depth. FW is capping sq_size at 65535.
 * In order to ensure hwq.depth <= 65535 after align up with 256, we need to
 * define maximum wqe slightly smaller.
 */
#define BNXT_VAR_MAX_WQE       4352
#define BNXT_VAR_MAX_WQE_P8    2040
#define BNXT_VAR_MAX_SLOT_ALIGN	256
#define BNXT_VAR_MAX_SGE	13
#define BNXT_RE_MAX_RQ_WQES	65536

#endif
