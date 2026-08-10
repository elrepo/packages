/*
 * Copyright (c) 2015-2024, Broadcom. All rights reserved.  The term
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
 * Description: Uverbs ABI header file
 */

#ifndef __BNXT_RE_UVERBS_ABI_H__
#define __BNXT_RE_UVERBS_ABI_H__

#define BNXT_RE_ABI_VERSION			1

#define BNXT_RE_CHIP_ID0_CHIP_NUM_SFT		0x00
#define BNXT_RE_CHIP_ID0_CHIP_REV_SFT		0x10
#define BNXT_RE_CHIP_ID0_CHIP_MET_SFT		0x18
enum {
	BNXT_RE_UCNTX_CMASK_HAVE_CCTX = 0x1ULL,
	BNXT_RE_UCNTX_CMASK_HAVE_MODE = 0x02ULL,
	BNXT_RE_UCNTX_CMASK_WC_DPI_ENABLED = 0x04ULL,
	BNXT_RE_UCNTX_CMASK_DBR_PACING_ENABLED = 0x08ULL,
	BNXT_RE_UCNTX_CMASK_POW2_DISABLED = 0x10ULL,
	BNXT_RE_UCNTX_CMASK_MSN_TABLE_ENABLED = 0x40,
	BNXT_RE_UCNTX_CMASK_UAPI_COMPAT_SUPPORTED = 0x80,
	BNXT_RE_UCNTX_CMASK_RSVD_WQE_DISABLED = 0x100,
	BNXT_RE_UCNTX_CMASK_MQP_EX_SUPPORTED = 0x200,
	BNXT_RE_UCNTX_CMASK_SMALL_RECV_WQE_DRV_SUP = 0x400,
	BNXT_RE_UCNTX_CMASK_MAX_RQ_WQES = 0x800,
	BNXT_RE_UCNTX_CMASK_CQ_IGNORE_OVERRUN_DRV_SUP = 0x1000,
	BNXT_RE_UCNTX_CMASK_MASK_ECE = 0x2000,
	BNXT_RE_UCNTX_CMASK_INTERNAL_QUEUE_MEMORY = 0x4000,
	BNXT_RE_UCNTX_CMASK_EXPRESS_MODE_ENABLED = 0x8000,
	BNXT_RE_UCNTX_CMASK_CHG_UDP_SRC_PORT_WQE_SUPPORTED = 0x10000,
	BNXT_RE_UCNTX_CMASK_DEFERRED_DB_ENABLED = 0x20000,
	BNXT_RE_UCNTX_CMASK_INLINE_OPTIMIZER_SUPPORTED = 0x40000,
	BNXT_RE_UCNTX_CMASK_COMPLETION_TS_SUPPORTED = 0x80000,
	BNXT_RE_UCNTX_CMASK_UC_DB_OFFSET = 0x100000,
	BNXT_RE_UCNTX_CMASK_OOB_DRIVER = 0x8000000000000000ULL,
};

/* TBD - check the enum list */
enum bnxt_re_req_to_drv {
	BNXT_RE_COMP_MASK_REQ_UCNTX_POW2_SUPPORT = 0x01,
	BNXT_RE_COMP_MASK_REQ_UCNTX_VAR_WQE_SUPPORT = 0x02,
	BNXT_RE_COMP_MASK_REQ_UCNTX_RSVD_WQE = 0x04,
	BNXT_RE_COMP_MASK_REQ_UCNTX_SMALL_RECV_WQE_LIB_SUP = 0x08,
	BNXT_RE_UCNTX_CMASK_OOB_LIB = 0x8000000000000000ULL,
};

/* bit wise modes can be extended here. */
enum bnxt_re_wqe_mode {
	BNXT_RE_WQE_MODE_STATIC	= 0x00,
	BNXT_RE_WQE_MODE_VARIABLE	= 0x01,
	BNXT_RE_WQE_MODE_INVALID	= 0x02,
};

struct bnxt_re_uctx_req {
	__aligned_u64 comp_mask;
};

struct bnxt_re_uctx_resp {
	__u32 dev_id;
	__u32 max_qp; /* To allocate qp-table */
	__u32 pg_size;
	__u32 cqe_sz;
	__u32 max_cqd;
	__u32 rsvd;
	__aligned_u64 comp_mask;
	__u32 chip_id0;
	__u32 chip_id1;
	__u32 mode;
	__u32 uc_db_offset;
	__u32 max_rq_wqes;
	__u32 wcdpi;
	__u64 uc_db_mmap_key;
	__u64 wc_db_mmap_key;
	__u32 dpi;
	__u8 db_push_mode;
	__u8 deferred_db_enabled;
	__u16 rsvd1;
};

enum {
	BNXT_RE_COMP_MASK_REQ_PD_INLINE_OPT_MRMW = 0x01,
};

struct bnxt_re_pd_req {
	__aligned_u64 comp_mask;
};

enum {
	BNXT_RE_COMP_MASK_PD_INLINE_OPT_MW_RKEY_VALID = 0x01,
};

struct bnxt_re_pd_resp {
	__u32 pdid;
	__u32 dpi;
	__u64 dbr;
	__u64 comp_mask; /*FIXME: Not working if __aligned_u64 is used */
	__u32 inline_opt_mw_rkey;
} __attribute__((packed, aligned(4)));

struct bnxt_re_ah_resp {
	__u32 ah_id;
	__u32 rsvd;
	__u64 comp_mask;
};

struct bnxt_re_packet_pacing_caps {
	__u32 qp_rate_limit_min;
	__u32 qp_rate_limit_max; /* In kpbs */
	/* Corresponding bit will be set if qp type from
	 * 'enum ib_qp_type' is supported, e.g.
	 * supported_qpts |= 1 << IB_QPT_RC for brcm
	 */
	__u32 supported_qpts;
	__u32 reserved;
};

struct bnxt_re_query_device_ex_resp {
	struct bnxt_re_packet_pacing_caps packet_pacing_caps;
};

enum {
	BNXT_RE_COMP_MASK_CQ_REQ_CAP_DBR_RECOVERY = 0x1,
	BNXT_RE_COMP_MASK_CQ_REQ_CAP_DBR_PACING_NOTIFY = 0x2,
	BNXT_RE_COMP_MASK_CQ_REQ_HAS_HDBR_KADDR = 0x4,
	BNXT_RE_COMP_MASK_CQ_REQ_IGNORE_OVERRUN = 0x8,
	BNXT_RE_COMP_MASK_CQ_REQ_L2 = 0x10,
};

#define BNXT_RE_IS_DBR_PACING_NOTIFY_CQ(_req)				\
	(_req.comp_mask & BNXT_RE_COMP_MASK_CQ_REQ_CAP_DBR_PACING_NOTIFY)

#define BNXT_RE_IS_L2_CQ(_req)						\
	(_req.comp_mask & BNXT_RE_COMP_MASK_CQ_REQ_L2)

struct bnxt_re_cq_req_ex {
	__aligned_u64 cq_va;
	__aligned_u64 cq_handle;
	__aligned_u64 comp_mask;
	__u64 cqprodva;
	__u64 cqconsva;
	__u64 cq_wc;
	__u32 cq_wc_sz;
	__u32 rsvd;
};

enum bnxt_re_cq_mask {
	BNXT_RE_CQ_TOGGLE_PAGE_SUPPORT = 0x1,
	BNXT_RE_CQ_HDBR_KADDR_SUPPORT = 0x02,
};

struct bnxt_re_cq_resp_ex {
	__u32 cqid;
	__u32 tail;
	__u32 phase;
	__u32 rsvd;
	__aligned_u64 comp_mask;
	__u64 hdbr_cq_mmap_key;
};

struct bnxt_re_resize_cq_req {
	__aligned_u64 cq_va;
};

/* QP */
enum bnxt_re_qp_req_mask {
	BNXT_RE_QP_REQ_MASK_VAR_WQE_SQ_SLOTS = 0x1,
	BNXT_RE_QP_REQ_MASK_PLACEHOLDER_FOR_DV = 0x2,
	BNXT_RE_QP_REQ_MASK_EXP_MODE = 0x4,
	BNXT_RE_QP_REQ_MASK_DUMP_QP_INDEX = 0x8,
};

struct bnxt_re_qp_req {
	__aligned_u64 qpsva;
	__aligned_u64 qprva;
	__aligned_u64 qp_handle;
	__aligned_u64 comp_mask;
	__u32 sq_slots;
	__u32 exp_mode;
	__u64 sqprodva;
	__u64 sqconsva;
	__u64 rqprodva;
	__u64 rqconsva;
};

enum bnxt_re_qp_resp_mask {
	BNXT_RE_QP_RESP_MASK_HDBR_DEBUG_TRACE = 0x1,
	BNXT_RE_QP_RESP_MASK_HDBR_KADDR_QP = 0x2,
};

struct bnxt_re_qp_resp {
	__u32 qpid;
	__u32 rsvd;
	__aligned_u64 comp_mask;
	__u32 hdbr_dt;
	__u32 rsvd1;
	__u64 hdbr_kaddr_sq;
	__u64 hdbr_kaddr_rq;
};

/* SRQ */
enum bnxt_re_srq_req_mask {
	BNXT_RE_QP_REQ_MASK_DUMP_SRQ_INDEX = 0x1,
};

struct bnxt_re_srq_req {
	__aligned_u64 srqva;
	__aligned_u64 srq_handle;
	__aligned_u64 comp_mask;
	__u64 srqprodva;
	__u64 srqconsva;
};

enum bnxt_re_srq_mask {
	BNXT_RE_SRQ_TOGGLE_PAGE_SUPPORT = 0x1,
	BNXT_RE_SRQ_HDBR_MMAP_KEY = 0x2,
};

struct bnxt_re_srq_resp {
	__u32 srqid;
	__u32 rsvd; /* padding */
	__aligned_u64 comp_mask;
	__u64 hdbr_srq_mmap_key;
};

/* Modify QP */
enum bnxt_re_modify_qp_ex_mask {
	BNXT_RE_COMP_MASK_MQP_EX_PPP_REQ_EN_MASK = 0x1UL,
	BNXT_RE_COMP_MASK_MQP_EX_PPP_REQ_EN	= 0x1UL,
	BNXT_RE_COMP_MASK_MQP_EX_PATH_MTU_MASK	= 0x2UL,
	BNXT_RE_COMP_MASK_MQP_EX_PPP_IDX_MASK	= 0x7UL,
	BNXT_RE_COMP_MASK_MQP_EX_PPP_STATE	= 0x10UL
};

struct bnxt_re_modify_qp_ex_req {
	__aligned_u64 comp_mask;
	__u32 dpi;
	__u32 rsvd;
};

struct bnxt_re_modify_qp_ex_resp {
	__aligned_u64 comp_mask;
	__u32 ppp_st_idx;
	__u32 path_mtu;
};

enum bnxt_re_shpg_offt {
	BNXT_RE_BEG_RESV_OFFT	= 0x00,
	BNXT_RE_AVID_OFFT	= 0x10,
	BNXT_RE_AVID_SIZE	= 0x04,
	BNXT_RE_END_RESV_OFFT	= 0xFF0
};

enum bnxt_re_objects {
	BNXT_RE_OBJECT_ALLOC_PAGE = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_OBJECT_NOTIFY_DRV,
	BNXT_RE_OBJECT_GET_TOGGLE_MEM,
	BNXT_RE_OBJECT_DBR,
	BNXT_RE_OBJECT_UMEM,
	BNXT_RE_OBJECT_DV_CQ,
	BNXT_RE_OBJECT_DV_QP,
	BNXT_RE_OBJECT_DV_SEND_FW_MSG,
	BNXT_RE_OBJECT_DV_CREATE_DESTROY_OBJ,
	BNXT_RE_OBJECT_DV_SEND_PT_MSG,
};

enum bnxt_re_alloc_page_type {
	BNXT_RE_ALLOC_WC_PAGE = 0,
	BNXT_RE_ALLOC_DBR_BAR_PAGE,
	BNXT_RE_ALLOC_DBR_PAGE,
};

enum bnxt_re_var_alloc_page_attrs {
	BNXT_RE_ALLOC_PAGE_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_ALLOC_PAGE_TYPE,
	BNXT_RE_ALLOC_PAGE_DPI,
	BNXT_RE_ALLOC_PAGE_MMAP_OFFSET,
	BNXT_RE_ALLOC_PAGE_MMAP_LENGTH,
};

enum bnxt_re_dv_send_fw_msg_attrs {
	BNXT_RE_SEND_FW_MSG_IN_LEN = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_SEND_FW_MSG_IN,
	BNXT_RE_SEND_FW_MSG_OUT_LEN,
	BNXT_RE_SEND_FW_MSG_OUT,
};

enum bnxt_re_dv_send_pt_msg_attrs {
	BNXT_RE_SEND_PT_MSG_IN_LEN = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_SEND_PT_MSG_IN,
};

struct bnxt_re_dv_stats_ctx_attr {
	__u32 update_period_ms;
	__u16 stats_dma_length;
	__u32 stats_size;		/* Size of stats buffer */
};

struct bnxt_re_dv_ring_attr {
	__u16 logical_id;		/* Logical ring ID */
	__u32 depth;			/* Number of descriptors */
	__u16 rx_buf_size;		/* Applicable for Rx ring only. Buf size */
	__u16 dpi;			/* Default DB region handle */
	__u8  mpc_chnl_type;		/* MPC channel type (TCE, RCE, TE_CFA, RE_CFA) */
	__u8 traffic_class;		/* Traffic class to be used for Tx */
};

struct bnxt_re_dv_vnic_attr {
	uint32_t flags;
	uint16_t virtio_net_fid;
	uint16_t vnic_id;
};

struct bnxt_re_dv_bfid_attr {
	uint32_t flags;
	uint16_t bfid;
};

struct bnxt_re_dv_create_obj_attr {
	__u8 obj_type;			/* BNXT_RE_OBJ_TYPE_TX, RX, RSS, STATS, VNIC */
	__u8 pg_size;			/* Page size */
	struct bnxt_re_dv_umem *umem_handle;	/* Pointer to umem object */
	__u64 offset;
	__aligned_u64 comp_mask;	/* For future expansion */
	/* hwq for TX/RX rings - allocated during create, freed during destroy */
	struct bnxt_qplib_hwq hwq;
	union {
		__u16 fw_ring_id;		/* For TX, RX, RX_AGG rings */
		__u16 fw_vnic_id;		/* For VNIC */
		__u32 fw_stats_ctx_id;	/* For STATS */
		__u16 fw_rss_ctx_id;
		__u16 bfid;
	} resp_data;
};

struct bnxt_re_dv_create_obj_init_attr {
	__u8 obj_type;       /* BNXT_RE_OBJ_TYPE_TX, RX, RX_AGGR, RSS, STATS, VNIC */
	__u8 pg_size;        /* Page size */
	__aligned_u64 umem;  /* return value from bnxt_re_dv_umem_reg */
	__u64 offset;        /* Offset into umem */
	__u32 size;
	__u16 handle;
	void *send_cq;
	__u16 stats_handle;
	__u16 rx_handle;     /* For RX_AGGR: handle to parent RX ring object */
	union {
		struct bnxt_re_dv_stats_ctx_attr stats_attr;
		struct bnxt_re_dv_ring_attr ring_attr;
		struct bnxt_re_dv_vnic_attr vnic_attr;
		struct bnxt_re_dv_bfid_attr  bfid_attr;
	};
	__u64 comp_mask; /* For future expansion */
};

enum bnxt_re_dv_obj_type {
	BNXT_RE_OBJ_TYPE_TX,
	BNXT_RE_OBJ_TYPE_RX,
	BNXT_RE_OBJ_TYPE_RX_AGGR,
	BNXT_RE_OBJ_TYPE_RSS,
	BNXT_RE_OBJ_TYPE_STATS,
	BNXT_RE_OBJ_TYPE_VNIC,
	BNXT_RE_OBJ_TYPE_MPC,
	BNXT_RE_OBJ_TYPE_BFID,
};

enum bnxt_re_dv_create_obj_attrs {
	BNXT_RE_CREATE_OBJ_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_CREATE_OBJ_UMEM_HANDLE,
	BNXT_RE_DV_CREATE_OBJ_CQ_HANDLE,
	BNXT_RE_DV_CREATE_OBJ_STATS_HANDLE,
	BNXT_RE_DV_CREATE_OBJ_RX_HANDLE,
	BNXT_RE_CREATE_OBJ_RESP,
	BNXT_RE_DV_DESTROY_OBJ_HANDLE,
	BNXT_RE_CREATE_OBJ_ATTR_PTR,
};

enum bnxt_re_alloc_page_attrs {
	BNXT_RE_DESTROY_PAGE_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
};

enum bnxt_re_alloc_page_methods {
	BNXT_RE_METHOD_ALLOC_PAGE = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_METHOD_DESTROY_PAGE,
};

enum bnxt_re_dv_send_fw_msg_methods {
	BNXT_RE_METHOD_DV_SEND_FW_MSG = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_METHOD_DV_CREATE_OBJ,
	BNXT_RE_METHOD_DV_DESTROY_OBJ
};

enum bnxt_re_dv_send_pt_msg_methods {
	BNXT_RE_METHOD_DV_SEND_PT_MSG = (1U << UVERBS_ID_NS_SHIFT),
};

enum bnxt_re_notify_drv_methods {
	BNXT_RE_METHOD_NOTIFY_DRV = (1U << UVERBS_ID_NS_SHIFT),
};

/* Toggle mem */
enum bnxt_re_get_toggle_mem_type {
	BNXT_RE_CQ_TOGGLE_MEM = 0,
	BNXT_RE_SRQ_TOGGLE_MEM,
};

enum bnxt_re_var_toggle_mem_attrs {
	BNXT_RE_TOGGLE_MEM_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_TOGGLE_MEM_TYPE,
	BNXT_RE_TOGGLE_MEM_RES_ID,
	BNXT_RE_TOGGLE_MEM_MMAP_PAGE,
	BNXT_RE_TOGGLE_MEM_MMAP_OFFSET,
	BNXT_RE_TOGGLE_MEM_MMAP_LENGTH,
};

enum bnxt_re_toggle_mem_attrs {
	BNXT_RE_RELEASE_TOGGLE_MEM_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
};

enum bnxt_re_toggle_mem_methods {
	BNXT_RE_METHOD_GET_TOGGLE_MEM = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_METHOD_RELEASE_TOGGLE_MEM,
};

enum bnxt_re_dv_modify_qp_type {
	BNXT_RE_DV_MODIFY_QP_TYPE_NONE = 0,
	BNXT_RE_DV_MODIFY_QP_UDP_SPORT = 1,
};

enum bnxt_re_var_dv_modify_qp_attrs {
	BNXT_RE_DV_MODIFY_QP_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_DV_MODIFY_QP_TYPE,
	BNXT_RE_DV_MODIFY_QP_VALUE,
	BNXT_RE_DV_MODIFY_QP_REQ,
};

struct bnxt_re_dv_cq_req {
	__u32 ncqe;
	__aligned_u64 va;
	__aligned_u64 comp_mask;
};

struct bnxt_re_dv_cq_resp {
	__u32 cqid;
	__u32 tail;
	__u32 phase;
	__u32 rsvd;
	__aligned_u64 comp_mask;
};

struct bnxt_re_dv_db_region_attr {
	uint32_t dbr_handle;
	uint32_t dpi;
	uint64_t umdbr;
	void *dbr;
};

enum bnxt_re_obj_dbr_alloc_attrs {
	BNXT_RE_DV_ALLOC_DBR_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_DV_ALLOC_DBR_ATTR,
	BNXT_RE_DV_ALLOC_DBR_OFFSET,
};

enum bnxt_re_obj_dbr_free_attrs {
	BNXT_RE_DV_FREE_DBR_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
};

enum bnxt_re_obj_dbr_query_attrs {
	BNXT_RE_DV_QUERY_DBR_ATTR = (1U << UVERBS_ID_NS_SHIFT),
};

enum bnxt_re_obj_dbr_methods {
	BNXT_RE_METHOD_DBR_ALLOC = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_METHOD_DBR_FREE,
	BNXT_RE_METHOD_DBR_QUERY,
};

struct bnxt_re_dv_umem {
	struct bnxt_re_dev *rdev;
	struct ib_umem *umem;
	u64 addr;
	size_t size;
	uint32_t access;
	int dmabuf_fd;
	struct bnxt_re_dv_umem *map;
};

#define BNXT_RE_UMEM_MAX_PA_BUFSZ 0x200000

enum bnxt_re_dv_umem_reg_attrs {
	BNXT_RE_UMEM_OBJ_REG_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_UMEM_OBJ_REG_ADDR,
	BNXT_RE_UMEM_OBJ_REG_LEN,
	BNXT_RE_UMEM_OBJ_REG_ACCESS,
	BNXT_RE_UMEM_OBJ_REG_DMABUF_FD,
	BNXT_RE_UMEM_OBJ_REG_PGSZ_BITMAP,
	BNXT_RE_UMEM_OBJ_REG_PA_ARR,
	BNXT_RE_UMEM_OBJ_REG_PA_ARR_LEN,
	BNXT_RE_UMEM_OBJ_REG_MAP_PG_SZ,
};

enum bnxt_re_dv_umem_dereg_attrs {
	BNXT_RE_UMEM_OBJ_DEREG_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
};

enum bnxt_re_dv_umem_methods {
	BNXT_RE_METHOD_UMEM_REG = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_METHOD_UMEM_DEREG,
};

enum bnxt_re_dv_create_cq_attrs {
	BNXT_RE_DV_CREATE_CQ_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_DV_CREATE_CQ_REQ,
	BNXT_RE_DV_CREATE_CQ_UMEM_HANDLE,
	BNXT_RE_DV_CREATE_CQ_UMEM_OFFSET,
	BNXT_RE_DV_CREATE_CQ_RESP,
};

enum bnxt_re_dv_destroy_cq_attrs {
	BNXT_RE_DV_DESTROY_CQ_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
};

enum bnxt_re_dv_cq_methods {
	BNXT_RE_METHOD_DV_CREATE_CQ = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_METHOD_DV_DESTROY_CQ,
};

struct bnxt_re_dv_create_qp_req {
	int qp_type;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
	__u32 pd_id;
	__aligned_u64 qp_handle;
	__aligned_u64 sq_va;
	__u32 sq_umem_offset;
	__u32 sq_len;   /* total len including MSN area */
	__u32 sq_slots;
	__u32 sq_wqe_sz;
	__u32 sq_psn_sz;
	__u32 sq_npsn;
	__aligned_u64 rq_va;
	__u32 rq_umem_offset;
	__u32 rq_len;
	__u32 rq_slots; /* == max_recv_wr */
	__u32 rq_wqe_sz;
};

struct bnxt_re_dv_create_qp_resp {
	__u32 qpid;
};

/* Response structures for each object type */
struct bnxt_re_dv_ring_resp {
	__u16 ring_id;			/* Allocated ring ID */
	__aligned_u64 comp_mask;	/* For future expansion */
};

struct bnxt_re_dv_stats_ctx_resp {
	__u32 stat_ctx_id;		/* Allocated stats context ID */
	__aligned_u64 comp_mask;	/* For future expansion */
};

struct bnxt_re_dv_vnic_resp {
	__u16 vnic_id;			/* Allocated VNIC ID */
	__aligned_u64 comp_mask;	/* For future expansion */
};

struct bnxt_re_dv_bfid_resp {
	uint16_t bfid;			/* Allocated bfid */
	uint64_t comp_mask;		/* For future expansion */
};

struct bnxt_re_dv_rss_ctx_resp {
	__u16 vnic_id;			/* VNIC ID used */
	__u16 rss_ctx_idx;		/* RSS context index */
	__aligned_u64 comp_mask;	/* For future expansion */
};

/* Union-based response structure */
struct bnxt_re_dv_create_obj_resp {
	__u8 obj_type;			/* Echo back the object type */
	union {
		struct bnxt_re_dv_ring_resp ring_resp;
		struct bnxt_re_dv_stats_ctx_resp stats_resp;
		struct bnxt_re_dv_vnic_resp vnic_resp;
		struct bnxt_re_dv_rss_ctx_resp rss_resp;
		struct bnxt_re_dv_bfid_resp bfid_resp;
	};
};

enum bnxt_re_dv_pt_msg_type {
	BNXT_RE_DV_PT_TYPE_TF,
};

struct bnxt_re_dv_pt_msg_input {
	__u8 pt_type;
	__u64 req;
	__u16 req_len;
	__u64 resp;
	__u16 resp_len;
};

enum bnxt_re_dv_create_qp_attrs {
	BNXT_RE_DV_CREATE_QP_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_DV_CREATE_QP_REQ,
	BNXT_RE_DV_CREATE_QP_SEND_CQ_HANDLE,
	BNXT_RE_DV_CREATE_QP_RECV_CQ_HANDLE,
	BNXT_RE_DV_CREATE_QP_SQ_UMEM_HANDLE,
	BNXT_RE_DV_CREATE_QP_RQ_UMEM_HANDLE,
	BNXT_RE_DV_CREATE_QP_SRQ_HANDLE,
	BNXT_RE_DV_CREATE_QP_DBR_HANDLE,
	BNXT_RE_DV_CREATE_QP_RESP
};

enum bnxt_re_dv_qp_methods {
	BNXT_RE_METHOD_DV_CREATE_QP = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_METHOD_DV_DESTROY_QP,
	BNXT_RE_METHOD_DV_MODIFY_QP,
	BNXT_RE_METHOD_DV_QUERY_QP,
};

enum bnxt_re_dv_destroy_qp_attrs {
	BNXT_RE_DV_DESTROY_QP_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
};

enum bnxt_re_dv_query_qp_attrs {
	BNXT_RE_DV_QUERY_QP_HANDLE = (1U << UVERBS_ID_NS_SHIFT),
	BNXT_RE_DV_QUERY_QP_ATTR,
};
#endif
