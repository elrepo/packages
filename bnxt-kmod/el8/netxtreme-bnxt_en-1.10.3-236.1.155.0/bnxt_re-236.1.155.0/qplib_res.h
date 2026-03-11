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
 * Description: QPLib resource manager (header)
 */

#ifndef __BNXT_QPLIB_RES_H__
#define __BNXT_QPLIB_RES_H__

#include "bnxt_dbr.h"
#include "bnxt_ulp.h"

extern const struct bnxt_qplib_gid bnxt_qplib_gid_zero;

#define CHIP_NUM_57508		0x1750
#define CHIP_NUM_57504		0x1751
#define CHIP_NUM_57502		0x1752
#define CHIP_NUM_58818          0xd818
#define CHIP_NUM_57608		0x1760
#define CHIP_NUM_57700		0x1770
#define CHIP_NUM_57708		0x1778

#define BNXT_QPLIB_MAX_QPC_COUNT	(64 * 1024)
#define BNXT_QPLIB_MAX_SRQC_COUNT	(64 * 1024)
#define BNXT_QPLIB_MAX_CQ_COUNT		(64 * 1024)
#define BNXT_QPLIB_MAX_CQ_COUNT_P5	(128 * 1024)

/* TODO:Remove this temporary define once HSI change is merged */
#define BNXT_QPLIB_DBR_VALID (0x1UL << 26)
#define BNXT_QPLIB_DBR_EPOCH_SHIFT   24
#define BNXT_QPLIB_DBR_TOGGLE_SHIFT  25

#define BNXT_QPLIB_DBR_PF_DB_OFFSET	0x10000
#define BNXT_QPLIB_DBR_VF_DB_OFFSET	0x4000

#define BNXT_QPLIB_DBR_KEY_INVALID	-1

enum bnxt_qplib_wqe_mode {
	BNXT_QPLIB_WQE_MODE_STATIC	= 0x00,
	BNXT_QPLIB_WQE_MODE_VARIABLE	= 0x01,
	BNXT_QPLIB_WQE_MODE_INVALID	= 0x02
};

#define BNXT_RE_PUSH_MODE_NONE	0
#define BNXT_RE_PUSH_MODE_WCB	1
#define BNXT_RE_PUSH_MODE_PPP	2
#define BNXT_RE_PUSH_ENABLED(mode) ((mode) == BNXT_RE_PUSH_MODE_WCB ||\
				    (mode) == BNXT_RE_PUSH_MODE_PPP)
#define BNXT_RE_PPP_ENABLED(cctx) ((cctx)->modes.db_push_mode ==\
				   BNXT_RE_PUSH_MODE_PPP)

#define BNXT_RE_STEERING_TO_HOST	1

struct bnxt_qplib_drv_modes {
	u8	wqe_mode;
	u8	te_bypass;
	u8	db_push_mode;
	u32     toggle_bits;
	/* To control advanced cc params display in configfs */
	u8	cc_pr_mode;
	/* Other modes to follow here e.g. GSI QP mode */
	u16	dbr_pacing:1;
	u16	dbr_pacing_ext:1;
	u16	dbr_drop_recov:1;
	u16	dbr_primary_pf:1;
	u16	dbr_pacing_chip_p7:1;
	u16	hdbr_enabled:1;
	u16	udcc_supported:1;
	u16	multiple_llq:1;
	u16	roce_mirror:1;
	u16	st_tag_supported:1;
	u16	gen_udp_src_supported:1;
	u16     secure_ats_supported:1;
	u16     peer_mmap_supported:1;
};

enum bnxt_re_toggle_modes {
	BNXT_QPLIB_CQ_TOGGLE_BIT = 0x1,
	BNXT_QPLIB_SRQ_TOGGLE_BIT = 0x2,
};

struct bnxt_qplib_chip_ctx {
	u16     chip_num;
	u8      chip_rev;
	u8      chip_metal;
	u64	hwrm_intf_ver;
	struct bnxt_qplib_drv_modes	modes;
	u32	dbr_stat_db_fifo;
	u32	dbr_aeq_arm_reg;
	u32	dbr_throttling_reg;
	u16	hw_stats_size;
	u16	hwrm_cmd_max_timeout;
};

static inline bool _is_chip_num_p7(u16 chip_num)
{
	return (chip_num == CHIP_NUM_58818 ||
		chip_num == CHIP_NUM_57608);
}

static inline bool _is_chip_p7(struct bnxt_qplib_chip_ctx *cctx)
{
	return _is_chip_num_p7(cctx->chip_num);
}

/* SR2 is Gen P5 */
static inline bool _is_chip_gen_p5(struct bnxt_qplib_chip_ctx *cctx)
{
	return (cctx->chip_num == CHIP_NUM_57508 ||
		cctx->chip_num == CHIP_NUM_57504 ||
		cctx->chip_num == CHIP_NUM_57502);
}

static inline bool _is_chip_gen_p5_p7(struct bnxt_qplib_chip_ctx *cctx)
{
	return (_is_chip_gen_p5(cctx) || _is_chip_p7(cctx));
}

static inline bool _is_chip_num_p8(u16 chip_num)
{
	return (chip_num == CHIP_NUM_57700 ||
		chip_num == CHIP_NUM_57708);
}

static inline bool _is_chip_p8(struct bnxt_qplib_chip_ctx *cctx)
{
	return _is_chip_num_p8(cctx->chip_num);
}

static inline bool _is_chip_p5_plus(struct bnxt_qplib_chip_ctx *cctx)
{
	return (_is_chip_gen_p5(cctx) ||
		_is_chip_p7(cctx) ||
		_is_chip_p8(cctx));
}

static inline bool _is_chip_num_p7_plus(u16 chip_num)
{
	return (_is_chip_num_p7(chip_num) ||
		_is_chip_num_p8(chip_num));
}

static inline bool _is_chip_p7_plus(struct bnxt_qplib_chip_ctx *cctx)
{
	return (_is_chip_p7(cctx) || _is_chip_p8(cctx));
}

static inline bool _is_hsi_v3(struct bnxt_qplib_chip_ctx *cctx)
{
	return _is_chip_p8(cctx);
}

static inline bool _is_wqe_mode_variable(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.wqe_mode == BNXT_QPLIB_WQE_MODE_VARIABLE;
}

struct bnxt_qplib_db_pacing_data {
	u32 do_pacing;
	u32 pacing_th;
	u32 dev_err_state;
	u32 alarm_th;
	u32 grc_reg_offset;
	u32 fifo_max_depth;
	u32 fifo_room_mask;
	u8  fifo_room_shift;
};

static inline u8 bnxt_qplib_dbr_pacing_en(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.dbr_pacing;
}

static inline u8 bnxt_qplib_dbr_pacing_ext_en(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.dbr_pacing_ext;
}

static inline u8 bnxt_qplib_dbr_pacing_chip_p7_en(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.dbr_pacing_chip_p7;
}

static inline u8 bnxt_qplib_dbr_pacing_is_primary_pf(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.dbr_primary_pf;
}

static inline void bnxt_qplib_dbr_pacing_set_primary_pf
		(struct bnxt_qplib_chip_ctx *cctx, u8 val)
{
	cctx->modes.dbr_primary_pf = val;
}

static inline u8 bnxt_qplib_udcc_supported(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.udcc_supported;
}

static inline u8 bnxt_qplib_multiple_llq_supported(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.multiple_llq;
}

static inline u8 bnxt_qplib_roce_mirror_supported(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.roce_mirror;
}

static inline u8 bnxt_qplib_gen_udp_src_supported(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.gen_udp_src_supported;
}

static inline u8 bnxt_qplib_peer_mmap_supported(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.peer_mmap_supported;
}

/* Defines for handling the HWRM version check */
#define HWRM_VERSION_DEV_ATTR_MAX_DPI	0x1000A0000000D
/* HWRM version 1.10.3.18 */
#define HWRM_VERSION_READ_CTX		0x1000A00030012
/* HWRM version 1.10.3.72 */
#define HWRM_VERSION_CC_EXT		0x1000A00030048

#define PTR_CNT_PER_PG		(PAGE_SIZE / sizeof(void *))
#define PTR_MAX_IDX_PER_PG	(PTR_CNT_PER_PG - 1)
#define PTR_PG(x)		(((x) & ~PTR_MAX_IDX_PER_PG) / PTR_CNT_PER_PG)
#define PTR_IDX(x)		((x) & PTR_MAX_IDX_PER_PG)

#define HWQ_CMP(idx, hwq)	((idx) & ((hwq)->max_elements - 1))
#define HWQ_FREE_SLOTS(hwq)	(hwq->max_elements - \
				((HWQ_CMP(hwq->prod, hwq)\
				- HWQ_CMP(hwq->cons, hwq))\
				& (hwq->max_elements - 1)))
enum bnxt_qplib_hwq_type {
	HWQ_TYPE_CTX,
	HWQ_TYPE_QUEUE,
	HWQ_TYPE_L2_CMPL,
	HWQ_TYPE_MR
};

#define MAX_PBL_LVL_0_PGS		1
#define MAX_PBL_LVL_1_PGS		(PAGE_SIZE / sizeof(u64))
#define MAX_PBL_LVL_1_PGS_SHIFT		ilog2(MAX_PBL_LVL_1_PGS)
#define MAX_PDL_LVL_SHIFT		ilog2(MAX_PBL_LVL_1_PGS)

enum bnxt_qplib_pbl_lvl {
	PBL_LVL_0,
	PBL_LVL_1,
	PBL_LVL_2,
	PBL_LVL_MAX
};

#define ROCE_PG_SIZE_4K		(4 * 1024)
#define ROCE_PG_SIZE_8K		(8 * 1024)
#define ROCE_PG_SIZE_64K	(64 * 1024)
#define ROCE_PG_SIZE_2M		(2 * 1024 * 1024)
#define ROCE_PG_SIZE_8M		(8 * 1024 * 1024)
#define ROCE_PG_SIZE_1G		(1024 * 1024 * 1024)
enum bnxt_qplib_hwrm_pg_size {
	BNXT_QPLIB_HWRM_PG_SIZE_4K	= 0,
	BNXT_QPLIB_HWRM_PG_SIZE_8K	= 1,
	BNXT_QPLIB_HWRM_PG_SIZE_64K	= 2,
	BNXT_QPLIB_HWRM_PG_SIZE_2M	= 3,
	BNXT_QPLIB_HWRM_PG_SIZE_8M	= 4,
	BNXT_QPLIB_HWRM_PG_SIZE_1G	= 5,
};

struct bnxt_qplib_reg_desc {
	u8		bar_id;
	resource_size_t	bar_base;
	unsigned long	offset;
	void __iomem	*bar_reg;
	size_t		len;
};

struct bnxt_qplib_pbl {
	u32				pg_count;
	u32				pg_size;
	void				**pg_arr;
	dma_addr_t			*pg_map_arr;
	struct bnxt_qplib_res		*res;
};

struct bnxt_qplib_sg_info {
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
        struct scatterlist              *sghead;
        u32                             nmap;
#else
	struct ib_umem			*umem;
#endif
        u32                             npages;
	u32				pgshft;
	u32				pgsize;
#define BNXT_RE_PBL_FWO_MAX		524288
	u32				fwo_offset;
	bool				nopte;
};

struct bnxt_qplib_hwq_attr {
	struct bnxt_qplib_res		*res;
	struct bnxt_qplib_sg_info	*sginfo;
	enum bnxt_qplib_hwq_type	type;
	u32				depth;
	u32				stride;
	u32				aux_stride;
	u32				aux_depth;
};

struct bnxt_qplib_hwq {
	struct pci_dev			*pdev;
	spinlock_t			lock;
	struct bnxt_qplib_pbl		pbl[PBL_LVL_MAX];
	enum bnxt_qplib_pbl_lvl		level;		/* 0, 1, or 2 */
	void				**pbl_ptr;	/* ptr for easy access
							   to the PBL entries */
	dma_addr_t			*pbl_dma_ptr;	/* ptr for easy access
							   to the dma_addr */
	u32				max_elements;
	u32				depth;	/* original requested depth */
	u16				element_size;	/* Size of each entry */
	u32				qe_ppg;		/* queue entry per page */

	u32				prod;		/* raw */
	u32				cons;		/* raw */
	u8				cp_bit;
	u8				is_user;
	u8				pg_sz_lvl;
	u64				*pad_pg;
	u32				pad_stride;
	u32				pad_pgofft;
};

struct bnxt_qplib_db_info {
	void __iomem		*db;
	void __iomem		*priv_db;
	struct bnxt_qplib_hwq	*hwq;
	struct bnxt_qplib_res   *res;
	u32			xid;
	u32			max_slot;
	u32			flags;
	u8			toggle;
	spinlock_t		lock;
	u64			shadow_key;
	u64			shadow_key_arm_ena;
	/* DB copy Thor2 recovery */
	__le64			*dbc; /* offset 0 of the DB copy block */
	int			ktbl_idx;
	void			*app;
	u8			dbc_dt;
	u32			seed; /* For DB pacing */
};

enum bnxt_qplib_db_info_flags_mask {
	BNXT_QPLIB_FLAG_EPOCH_CONS_SHIFT	= 0x0UL,
	BNXT_QPLIB_FLAG_EPOCH_PROD_SHIFT	= 0x1UL,
	BNXT_QPLIB_FLAG_EPOCH_CONS_MASK		= 0x1UL,
	BNXT_QPLIB_FLAG_EPOCH_PROD_MASK		= 0x2UL,
};

enum bnxt_qplib_db_epoch_flag_shift {
	BNXT_QPLIB_DB_EPOCH_CONS_SHIFT	= BNXT_QPLIB_DBR_EPOCH_SHIFT,
	BNXT_QPLIB_DB_EPOCH_PROD_SHIFT	= (BNXT_QPLIB_DBR_EPOCH_SHIFT - 1)
};

/* Tables */
struct bnxt_qplib_pd_tbl {
	unsigned long			*tbl;
	u32				max;
};

struct bnxt_qplib_sgid_tbl {
	struct bnxt_qplib_gid_info	*tbl;
	u16				*hw_id;
	u16				max;
	u16				active;
	void				*ctx;
	bool                            *vlan;
};

enum bnxt_qplib_dpi_type {
	BNXT_QPLIB_DPI_TYPE_KERNEL	= 0,
	BNXT_QPLIB_DPI_TYPE_UC		= 1,
	BNXT_QPLIB_DPI_TYPE_WC		= 2
};

struct bnxt_qplib_dpi {
	u32				dpi;
	u32				bit;
	u64				umdbr;
	void __iomem			*dbr;
	enum bnxt_qplib_dpi_type	type;
};

#define BNXT_QPLIB_MAX_EXTENDED_PPP_PAGES	512
struct bnxt_qplib_dpi_tbl {
	void				**app_tbl;
	unsigned long			*tbl;
	u16				max;
	u16				avail_ppp;
	struct bnxt_qplib_reg_desc	ucreg; /* Hold entire DB bar. */
	struct bnxt_qplib_reg_desc	wcreg;
	void __iomem			*priv_db;
};

struct bnxt_qplib_stats {
	dma_addr_t			dma_handle;
	void				*cpu_addr;
	u32				size;
	u32				fw_id;
	u64				*sw_stats;
};

struct bnxt_qplib_vf_res {
	u32 max_qp;
	u32 max_mrw;
	u32 max_srq;
	u32 max_cq;
	u32 max_gid;
};

#define BNXT_QPLIB_MAX_QP_CTX_ENTRY_SIZE	448
#define BNXT_QPLIB_MAX_SRQ_CTX_ENTRY_SIZE	64
#define BNXT_QPLIB_MAX_CQ_CTX_ENTRY_SIZE	64
#define BNXT_QPLIB_MAX_MRW_CTX_ENTRY_SIZE	128

#define MAX_TQM_ALLOC_REQ		48
#define MAX_TQM_ALLOC_BLK_SIZE		8
struct bnxt_qplib_tqm_ctx {
	struct bnxt_qplib_hwq		pde;
	enum bnxt_qplib_pbl_lvl		pde_level; /* Original level */
	struct bnxt_qplib_hwq		qtbl[MAX_TQM_ALLOC_REQ];
	u8				qcount[MAX_TQM_ALLOC_REQ];
};

struct bnxt_qplib_hctx {
	struct bnxt_qplib_hwq	hwq;
	u32			max;
};

struct bnxt_qplib_refrec {
	void *handle;
	u32 xid;
};

struct bnxt_qplib_reftbl {
	struct bnxt_qplib_refrec *rec;
	u32 max;
	spinlock_t lock; /* reftbl lock */
};

struct bnxt_qplib_reftbls {
	struct bnxt_qplib_reftbl qpref;
	struct bnxt_qplib_reftbl cqref;
	struct bnxt_qplib_reftbl srqref;
};

struct bnxt_qplib_stats_ext_ctx {
#define INVALID_STATS_EXT_CTX_ID    (-1)
	int				xid;
	u32				update_period_ms;
	u16				steering_tag;
	dma_addr_t			dma_handle;
	void                            *cpu_addr;
	struct bnxt_re_dev		*rdev;
};

#define GET_TBL_INDEX(id, tbl) ((id) % (((tbl)->max) - 1))
static inline u32 map_qp_id_to_tbl_indx(u32 qid, struct bnxt_qplib_reftbl *tbl)
{
	return (qid == 1) ? tbl->max : GET_TBL_INDEX(qid, tbl);
}

/*
 * This structure includes the number of various roce resource table sizes
 * actually allocated by the driver. May be less than the maximums the firmware
 * allows if the driver imposes lower limits than the firmware.
 */
struct bnxt_qplib_ctx {
	struct bnxt_qplib_hctx		qp_ctx;
	struct bnxt_qplib_hctx		mrw_ctx;
	struct bnxt_qplib_hctx		srq_ctx;
	struct bnxt_qplib_hctx		cq_ctx;
	struct bnxt_qplib_hctx		tim_ctx;
	struct bnxt_qplib_tqm_ctx	tqm_ctx;

	struct bnxt_qplib_stats		stats;
	struct bnxt_qplib_stats		stats2;
	struct bnxt_qplib_stats		stats3;
	struct bnxt_qplib_vf_res	vf_res;
	struct bnxt_qplib_stats_ext_ctx stats_ext_ctx;
};

struct bnxt_qplib_res {
	struct pci_dev			*pdev;
	struct bnxt_qplib_chip_ctx	*cctx;
	struct bnxt_qplib_dev_attr      *dattr;
	struct bnxt_qplib_ctx		*hctx;
	struct net_device		*netdev;
	struct bnxt_en_dev		*en_dev;

	struct bnxt_qplib_rcfw		*rcfw;

	struct bnxt_qplib_pd_tbl	pd_tbl;
	struct mutex			pd_tbl_lock;
	struct bnxt_qplib_sgid_tbl	sgid_tbl;
	struct bnxt_qplib_dpi_tbl	dpi_tbl;
	struct mutex			dpi_tbl_lock;
	struct bnxt_qplib_reftbls	reftbl;
	bool				prio;
	bool				is_vf;
	struct bnxt_qplib_db_pacing_data *pacing_data;
	atomic_t			bar_cnt;
	struct bnxt_peer_bar_addr	bar_addr[BNXT_MAX_BAR_ADDR];
};

struct bnxt_qplib_query_stats_info {
	u32 function_id;
	u8 collection_id;
	bool vf_valid;
};

struct bnxt_qplib_query_qp_info {
	u32 function_id;
	u32 num_qps;
	u32 start_index;
	bool vf_valid;
};

struct bnxt_qplib_query_fn_info {
	bool vf_valid;
	u32 host;
	u32 filter;
};

#define to_bnxt_qplib(ptr, type, member)	\
	container_of(ptr, type, member)

struct bnxt_qplib_pd;
struct bnxt_qplib_dev_attr;

bool _is_alloc_mr_unified(struct bnxt_qplib_dev_attr *dattr);
void bnxt_qplib_free_hwq(struct bnxt_qplib_res *res,
			 struct bnxt_qplib_hwq *hwq);
int bnxt_qplib_alloc_init_hwq(struct bnxt_qplib_hwq *hwq,
			      struct bnxt_qplib_hwq_attr *hwq_attr);
int bnxt_qplib_alloc_pd(struct bnxt_qplib_res *res,
			struct bnxt_qplib_pd *pd);
int bnxt_qplib_dealloc_pd(struct bnxt_qplib_res *res,
			  struct bnxt_qplib_pd_tbl *pd_tbl,
			  struct bnxt_qplib_pd *pd);
int bnxt_qplib_alloc_dpi(struct bnxt_qplib_res *res,
			 struct bnxt_qplib_dpi *dpi,
			 void *app, enum bnxt_qplib_dpi_type type);
int bnxt_qplib_dealloc_dpi(struct bnxt_qplib_res *res,
			   struct bnxt_qplib_dpi *dpi);
int bnxt_qplib_alloc_uc_dpi(struct bnxt_qplib_res *res,
			    struct bnxt_qplib_dpi *dpi);
int bnxt_qplib_free_uc_dpi(struct bnxt_qplib_res *res,
			   struct bnxt_qplib_dpi *dpi);
void bnxt_qplib_alloc_kernel_dpi(struct bnxt_qplib_res *res,
				 struct bnxt_qplib_dpi *dpi);
void bnxt_qplib_dealloc_kernel_dpi(struct bnxt_qplib_dpi *dpi);
int bnxt_qplib_stop_res(struct bnxt_qplib_res *res);
void bnxt_qplib_clear_tbls(struct bnxt_qplib_res *res);
void bnxt_qplib_init_tbls(struct bnxt_qplib_res *res);
void bnxt_qplib_free_tbls(struct bnxt_qplib_res *res);
int bnxt_qplib_alloc_tbls(struct bnxt_qplib_res *res);
void bnxt_qplib_free_hwctx(struct bnxt_qplib_res *res);
int bnxt_qplib_alloc_hwctx(struct bnxt_qplib_res *res);
int bnxt_qplib_alloc_stat_mem(struct pci_dev *pdev,
			      struct bnxt_qplib_chip_ctx *cctx,
			      struct bnxt_qplib_stats *stats);
void bnxt_qplib_free_stat_mem(struct bnxt_qplib_res *res,
			      struct bnxt_qplib_stats *stats);

int bnxt_qplib_map_db_bar(struct bnxt_qplib_res *res);
void bnxt_qplib_unmap_db_bar(struct bnxt_qplib_res *res);
int bnxt_qplib_enable_atomic_ops_to_root(struct bnxt_en_dev *en_dev, bool is_virtfn);
void bnxt_qplib_init_stats_ext_ctx(struct bnxt_qplib_res *res);
int bnxt_qplib_alloc_stats_ext_ctx(struct bnxt_qplib_res *res);
int bnxt_qplib_dealloc_stats_ext_ctx(struct bnxt_qplib_res *res);
int bnxt_qplib_get_stats_ext_ctx_id(struct bnxt_qplib_res *res);

static inline void *bnxt_qplib_get_qe(struct bnxt_qplib_hwq *hwq,
				      u32 indx, u64 *pg)
{
	u32 pg_num, pg_idx;

	pg_num = (indx / hwq->qe_ppg);
	pg_idx = (indx % hwq->qe_ppg);
	if (pg)
		*pg = (u64)&hwq->pbl_ptr[pg_num];
	return (void *)(hwq->pbl_ptr[pg_num] + hwq->element_size * pg_idx);
}

static inline void bnxt_qplib_hwq_incr_prod(struct bnxt_qplib_db_info *dbinfo,
					    struct bnxt_qplib_hwq *hwq, u32 cnt)
{
	/* move prod and update toggle/epoch if wrap around */
	hwq->prod += cnt;
	if (hwq->prod >= hwq->depth) {
		hwq->prod %= hwq->depth;
		dbinfo->flags ^= 1UL << BNXT_QPLIB_FLAG_EPOCH_PROD_SHIFT;
	}
}

static inline void bnxt_qplib_hwq_incr_cons(u32 max_elements, u32 *cons,
					    u32 cnt, u32 *dbinfo_flags)
{
	/* move cons and update toggle/epoch if wrap around */
	*cons += cnt;
	if (*cons >= max_elements) {
		*cons %= max_elements;
		*dbinfo_flags ^= 1UL << BNXT_QPLIB_FLAG_EPOCH_CONS_SHIFT;
	}
}

static inline u8 _get_pte_pg_size(struct bnxt_qplib_hwq *hwq)
{
	struct bnxt_qplib_pbl *pbl;

	pbl = &hwq->pbl[hwq->level];
	switch (pbl->pg_size) {
	case ROCE_PG_SIZE_4K:
		return BNXT_QPLIB_HWRM_PG_SIZE_4K;
	case ROCE_PG_SIZE_8K:
		return BNXT_QPLIB_HWRM_PG_SIZE_8K;
	case ROCE_PG_SIZE_64K:
		return BNXT_QPLIB_HWRM_PG_SIZE_64K;
	case ROCE_PG_SIZE_2M:
		return BNXT_QPLIB_HWRM_PG_SIZE_2M;
	case ROCE_PG_SIZE_8M:
		return BNXT_QPLIB_HWRM_PG_SIZE_8M;
	case ROCE_PG_SIZE_1G:
		return BNXT_QPLIB_HWRM_PG_SIZE_1G;
	default:
		return BNXT_QPLIB_HWRM_PG_SIZE_4K;
	}
}

static inline u64 _get_base_addr(struct bnxt_qplib_hwq *hwq)
{
	return hwq->pbl[PBL_LVL_0].pg_map_arr[0];
}

static inline u8 _get_base_pg_size(struct bnxt_qplib_hwq *hwq)
{
	struct bnxt_qplib_pbl *pbl;

	pbl = &hwq->pbl[hwq->level];
	switch (pbl->pg_size) {
	case ROCE_PG_SIZE_4K:
		return BNXT_QPLIB_HWRM_PG_SIZE_4K;
	case ROCE_PG_SIZE_8K:
		return BNXT_QPLIB_HWRM_PG_SIZE_8K;
	case ROCE_PG_SIZE_64K:
		return BNXT_QPLIB_HWRM_PG_SIZE_64K;
	case ROCE_PG_SIZE_2M:
		return BNXT_QPLIB_HWRM_PG_SIZE_2M;
	case ROCE_PG_SIZE_8M:
		return BNXT_QPLIB_HWRM_PG_SIZE_8M;
	case ROCE_PG_SIZE_1G:
		return BNXT_QPLIB_HWRM_PG_SIZE_1G;
	default:
		return BNXT_QPLIB_HWRM_PG_SIZE_4K;
	}
	return BNXT_QPLIB_HWRM_PG_SIZE_4K;
}

#define BNXT_QPLIB_HWRM_PBL_PG_SIZE_PG_4K	0x0UL
#define BNXT_QPLIB_HWRM_PBL_PG_SIZE_PG_8K	0x1UL
#define BNXT_QPLIB_HWRM_PBL_PG_SIZE_PG_64K	0x2UL
#define BNXT_QPLIB_HWRM_PBL_PG_SIZE_PG_2M	0x3UL
#define BNXT_QPLIB_HWRM_PBL_PG_SIZE_PG_8M	0x4UL
#define BNXT_QPLIB_HWRM_PBL_PG_SIZE_PG_1G	0x5UL

static inline u8 _get_pbl_page_size(struct bnxt_qplib_sg_info *sginfo)
{
	return BNXT_QPLIB_HWRM_PBL_PG_SIZE_PG_4K;
}

static inline int bnxt_re_pbl_size_supported(u8 dev_cap_ext_flags_1)
{
	return dev_cap_ext_flags_1 &
		CREQ_QUERY_FUNC_RESP_SB_PBL_PAGE_SIZE_SUPPORTED;
}

static inline enum bnxt_qplib_hwq_type _get_hwq_type(struct bnxt_qplib_res *res)
{
	return _is_chip_p5_plus(res->cctx) ? HWQ_TYPE_QUEUE : HWQ_TYPE_L2_CMPL;
}

static inline bool _is_ext_stats_supported(u16 dev_cap_flags)
{
	return dev_cap_flags &
		CREQ_QUERY_FUNC_RESP_SB_EXT_STATS;
}

static inline int bnxt_ext_stats_supported(struct bnxt_qplib_chip_ctx *ctx,
					   u16 flags, bool virtfn)
{
	/* ext stats supported if cap flag is set AND is a PF OR a Thor2 VF */
	return (_is_ext_stats_supported(flags) &&
		((virtfn && _is_chip_p7(ctx)) || (!virtfn)));
}

static inline bool _is_hw_req_retx_supported(u16 dev_cap_flags)
{
	return dev_cap_flags & CREQ_QUERY_FUNC_RESP_SB_HW_REQUESTER_RETX_ENABLED;
}

static inline bool _is_hw_resp_retx_supported(u16 dev_cap_flags)
{
	return dev_cap_flags & CREQ_QUERY_FUNC_RESP_SB_HW_RESPONDER_RETX_ENABLED;
}

static inline bool _is_internal_queue_memory(u16 dev_cap_flags)
{
	return dev_cap_flags & CREQ_QUERY_FUNC_RESP_SB_INTERNAL_QUEUE_MEMORY;
}

static inline bool _is_express_mode_supported(u16 dev_cap_flags)
{
#ifdef HAVE_X86_FEATURE_MOVDIR64B
	if (cpu_feature_enabled(X86_FEATURE_MOVDIR64B) &&
	    dev_cap_flags & CREQ_QUERY_FUNC_RESP_SB_EXPRESS_MODE_SUPPORTED)
		return true;
#endif
	/* For x86 without movdir64b feature enabled, or other non-x86 arch
	 * such as Arm.
	 */
	return false;
}

static inline bool _is_hw_retx_supported(u16 dev_cap_flags)
{
	return _is_hw_req_retx_supported(dev_cap_flags) ||
		_is_hw_resp_retx_supported(dev_cap_flags);
}

static inline bool _is_drv_ver_reg_supported(u8 dev_cap_ext_flags)
{
	return dev_cap_ext_flags &
		CREQ_QUERY_FUNC_RESP_SB_DRV_VERSION_RGTR_SUPPORTED;
}

static inline bool _is_max_srq_ext_supported(u16 dev_cap_ext_flags_2)
{
	return dev_cap_ext_flags_2 &
		CREQ_QUERY_FUNC_RESP_SB_MAX_SRQ_EXTENDED;
}

static inline bool _is_small_recv_wqe_supported(u8 dev_cap_ext_flags)
{
	return dev_cap_ext_flags &
		CREQ_QUERY_FUNC_RESP_SB_CREATE_SRQ_SGE_SUPPORTED;
}

static inline bool _is_cq_coalescing_supported(u16 dev_cap_ext_flags2)
{
	return dev_cap_ext_flags2 &
		CREQ_QUERY_FUNC_RESP_SB_CQ_COALESCING_SUPPORTED;
}

static inline bool _is_optimize_modify_qp_supported(u16 dev_cap_ext_flags2)
{
	return dev_cap_ext_flags2 & CREQ_QUERY_FUNC_RESP_SB_OPTIMIZE_MODIFY_QP_SUPPORTED;
}

static inline bool _is_min_rnr_in_rtr_rts_mandatory(u16 dev_cap_ext_flags2)
{
	return !!(dev_cap_ext_flags2 & CREQ_QUERY_FUNC_RESP_SB_MIN_RNR_RTR_RTS_OPT_SUPPORTED);
}

static inline bool _is_def_cc_param_supported(u16 dev_cap_ext_flags2)
{
	return (dev_cap_ext_flags2 &
		CREQ_QUERY_FUNC_RESP_SB_DEFAULT_ROCE_CC_PARAMS_SUPPORTED) ? true : false;
}

static inline bool is_roce_context_destroy_sb_enabled(u16 dev_cap_ext_flags2)
{
	return (dev_cap_ext_flags2 &
		CREQ_QUERY_FUNC_RESP_SB_DESTROY_CONTEXT_SB_SUPPORTED) ? true : false;
}

static inline bool is_roce_udcc_session_destroy_sb_enabled(u16 dev_cap_ext_flags2)
{
	return (dev_cap_ext_flags2 &
		CREQ_QUERY_FUNC_RESP_SB_DESTROY_UDCC_SESSION_SB_SUPPORTED) ? true : false;
}

static inline bool is_udcc_session_data_present(u8 flag)
{
	return (flag & CREQ_DESTROY_QP_RESP_FLAGS_UDCC_SESSION_DATA);
}

static inline bool is_udcc_session_rtt_present(u8 flag)
{
	return (flag & CREQ_DESTROY_QP_RESP_FLAGS_UDCC_RTT_DATA);
}

static inline bool _is_modify_qp_rate_limit_supported(u16 dev_cap_ext_flags2)
{
	return (dev_cap_ext_flags2 &
		CREQ_QUERY_FUNC_RESP_SB_MODIFY_QP_RATE_LIMIT_SUPPORTED) ? true : false;
}

#ifdef HAVE_IB_ACCESS_RELAXED_ORDERING
static inline bool _is_relaxed_ordering_supported(u16 dev_cap_ext_flags2)
{
	return dev_cap_ext_flags2 & CREQ_QUERY_FUNC_RESP_SB_MEMORY_REGION_RO_SUPPORTED;
}
#endif

static inline bool _is_host_msn_table(u16 dev_cap_ext_flags2)
{
	return (dev_cap_ext_flags2 & CREQ_QUERY_FUNC_RESP_SB_REQ_RETRANSMISSION_SUPPORT_MASK) ==
		CREQ_QUERY_FUNC_RESP_SB_REQ_RETRANSMISSION_SUPPORT_HOST_MSN_TABLE;
}

static inline bool _is_modify_udp_sport_supported(u16 dev_cap_ext_flags2)
{
	return dev_cap_ext_flags2 & CREQ_QUERY_FUNC_RESP_SB_CHANGE_UDP_SRC_PORT_SUPPORTED;
}

static inline bool _is_cq_overflow_detection_enabled(u16 dev_cap_ext_flags2)
{
	return !!(dev_cap_ext_flags2 & CREQ_QUERY_FUNC_RESP_SB_CQ_OVERFLOW_DETECTION_ENABLED);
}

static inline bool is_disable_icrc_supported(u16 dev_cap_ext_flags2)
{
	return !!(dev_cap_ext_flags2 & CREQ_QUERY_FUNC_RESP_SB_ICRC_CHECK_DISABLE_SUPPORTED);
}

static inline bool is_force_mirroring_supported(u16 dev_cap_ext_flags2)
{
	return !!(dev_cap_ext_flags2 & CREQ_QUERY_FUNC_RESP_SB_FORCE_MIRROR_ENABLE_SUPPORTED);
}

static inline bool _is_change_udp_src_port_wqe_supported(u16 flags)
{
	return !!(flags &
		  CREQ_QUERY_FUNC_RESP_SB_CHANGE_UDP_SRC_PORT_WQE_SUPPORTED);
}

#define BNXT_RE_INIT_FW_DRV_VER_SUPPORT_CMD_SIZE 16

#define BNXT_RE_CREATE_QP_EXT_STAT_CONTEXT_SIZE 8
#define BNXT_RE_MODIFY_QP_EXT_STAT_CONTEXT_SIZE 8

static inline bool bnxt_ext_stats_v2_supported(u16 flags)
{
	return flags &
		CREQ_QUERY_FUNC_RESP_SB_ROCE_STATS_EXT_CTX_SUPPORTED;
}
/* Disable HW_RETX */
#define BNXT_RE_HW_RETX(a) _is_hw_retx_supported((a))
#define BNXT_RE_HW_REQ_RETX(a) _is_hw_req_retx_supported((a))
#define BNXT_RE_HW_RESP_RETX(a) _is_hw_resp_retx_supported((a))
#define BNXT_RE_IQM(a) _is_internal_queue_memory((a))
#define BNXT_RE_EXPRESS_MODE(a) _is_express_mode_supported((a))
#define BNXT_RE_UDP_SP_WQE(a) _is_change_udp_src_port_wqe_supported((a))

static inline bool _is_cqe_v2_supported(u16 dev_cap_flags)
{
	return dev_cap_flags &
		CREQ_QUERY_FUNC_RESP_SB_CQE_V2;
}

static inline void bnxt_qplib_do_pacing(struct bnxt_qplib_db_info *info)
{
	struct bnxt_qplib_db_pacing_data *pacing_data;
	struct bnxt_qplib_res *res = info->res;

	pacing_data = res->pacing_data;
	if (pacing_data && pacing_data->do_pacing)
		bnxt_do_pacing(res->en_dev->bar0, res->en_dev->en_dbr, &info->seed,
			       pacing_data->pacing_th, pacing_data->do_pacing);
}

static inline void bnxt_qplib_ring_db32(struct bnxt_qplib_db_info *info,
					bool arm)
{
	u32 key = 0;

	key = info->hwq->cons | (CMPL_DOORBELL_IDX_VALID |
		(CMPL_DOORBELL_KEY_CMPL & CMPL_DOORBELL_KEY_MASK));
	if (!arm)
		key |= CMPL_DOORBELL_MASK;
	/* memory barrier */
	wmb();
	writel(key, info->db);
}

#define HDBR_DBC_DEBUG_TRACE    (0x1ULL << 59)
#define HDBR_DBC_OFFSET_SQ		0
#define HDBR_DBC_OFFSET_RQ		0
#define HDBR_DBC_OFFSET_SRQ		0
#define HDBR_DBC_OFFSET_SRQ_ARMENA	1
#define HDBR_DBC_OFFSET_SRQ_ARM		2
#define HDBR_DBC_OFFSET_CQ_ARMENA	0
#define HDBR_DBC_OFFSET_CQ_ARMSE	1
#define HDBR_DBC_OFFSET_CQ_ARMALL	1
#define HDBR_DBC_OFFSET_CQ_CUTOFF_ACK	2
#define HDBR_DBC_OFFSET_CQ		3

static inline int bnxt_re_hdbr_get_dbc_offset(u32 type)
{
	switch (type) {
	case DBC_DBC_TYPE_SQ:
		return HDBR_DBC_OFFSET_SQ;

	case DBC_DBC_TYPE_RQ:
		return HDBR_DBC_OFFSET_RQ;

	case DBC_DBC_TYPE_SRQ:
		return HDBR_DBC_OFFSET_SRQ;

	case DBC_DBC_TYPE_SRQ_ARMENA:
		return HDBR_DBC_OFFSET_SRQ_ARMENA;

	case DBC_DBC_TYPE_SRQ_ARM:
		return HDBR_DBC_OFFSET_SRQ_ARM;

	case DBC_DBC_TYPE_CQ:
		return HDBR_DBC_OFFSET_CQ;

	case DBC_DBC_TYPE_CQ_ARMENA:
		return HDBR_DBC_OFFSET_CQ_ARMENA;

	case DBC_DBC_TYPE_CQ_ARMSE:
		return HDBR_DBC_OFFSET_CQ_ARMSE;

	case DBC_DBC_TYPE_CQ_ARMALL:
		return HDBR_DBC_OFFSET_CQ_ARMALL;

	case DBC_DBC_TYPE_CQ_CUTOFF_ACK:
		return HDBR_DBC_OFFSET_CQ_CUTOFF_ACK;
	}

	return -1;
}

static inline void bnxt_re_hdbr_db_copy(struct bnxt_qplib_db_info *info, u64 key)
{
	int offset;

	if (!info->dbc)
		return;
	offset = bnxt_re_hdbr_get_dbc_offset((u32)(key >> 32) & DBC_DBC_TYPE_MASK);
	if (offset < 0)
		return;
	if (info->dbc_dt)
		key |= HDBR_DBC_DEBUG_TRACE;
	*(info->dbc + offset) = cpu_to_le64(key);
	wmb(); /* Sync DB copy before it is written into HW */
}

#define BNXT_QPLIB_INIT_DBHDR(xid, type, indx, toggle)				\
	(((u64)(((xid) & DBC_DBC_XID_MASK) | DBC_DBC_PATH_ROCE |	\
	    (type) | BNXT_QPLIB_DBR_VALID) << 32) | (indx) |	\
	    (((u32)(toggle)) << (BNXT_QPLIB_DBR_TOGGLE_SHIFT)))

static inline void bnxt_qplib_write_db(struct bnxt_qplib_db_info *info,
				       u64 key, void __iomem *db,
				       u64 *shadow_key)
{
	unsigned long flags;

	spin_lock_irqsave(&info->lock, flags);
	bnxt_qplib_do_pacing(info);
	*shadow_key = key;
	bnxt_re_hdbr_db_copy(info, key);
	writeq(key, db);
	spin_unlock_irqrestore(&info->lock, flags);
}

static inline void __replay_writeq(u64 key, void __iomem *db)
{
	/* No need to replay uninitialised shadow_keys */
	if (key != BNXT_QPLIB_DBR_KEY_INVALID)
		writeq(key, db);
}

static inline void bnxt_qplib_replay_db(struct bnxt_qplib_db_info *info,
				        bool is_arm_ena)

{
	unsigned long flags;

	if (!spin_trylock_irqsave(&info->lock, flags))
		return;

	bnxt_qplib_do_pacing(info);
	if (is_arm_ena)
		__replay_writeq(info->shadow_key_arm_ena, info->priv_db);
	else
		__replay_writeq(info->shadow_key, info->db);

	spin_unlock_irqrestore(&info->lock, flags);
}

static inline void bnxt_qplib_ring_db(struct bnxt_qplib_db_info *info,
				      u32 type)
{
	u64 key = 0;
	u32 indx;
	u8 toggle = 0;

	if (type == DBC_DBC_TYPE_CQ_ARMALL ||
	    type == DBC_DBC_TYPE_CQ_ARMSE)
		toggle = info->toggle;

	indx = ((info->hwq->cons & DBC_DBC_INDEX_MASK) |
		((info->flags & BNXT_QPLIB_FLAG_EPOCH_CONS_MASK) <<
		 BNXT_QPLIB_DB_EPOCH_CONS_SHIFT));

	key = BNXT_QPLIB_INIT_DBHDR(info->xid, type, indx, toggle);
	bnxt_qplib_write_db(info, key, info->db, &info->shadow_key);
}

static inline void bnxt_qplib_ring_prod_db(struct bnxt_qplib_db_info *info,
					   u32 type)
{
	u64 key = 0;
	u32 indx;

	indx = (((info->hwq->prod / info->max_slot) & DBC_DBC_INDEX_MASK) |
		((info->flags & BNXT_QPLIB_FLAG_EPOCH_PROD_MASK) <<
		 BNXT_QPLIB_DB_EPOCH_PROD_SHIFT));
	key = BNXT_QPLIB_INIT_DBHDR(info->xid, type, indx, 0);
	bnxt_qplib_write_db(info, key, info->db, &info->shadow_key);
}

static inline void bnxt_qplib_armen_db(struct bnxt_qplib_db_info *info,
				       u32 type)
{
	u64 key = 0;
	u8 toggle = 0;

	if (type == DBC_DBC_TYPE_CQ_ARMENA || type == DBC_DBC_TYPE_SRQ_ARMENA)
		toggle = info->toggle;
	/* Index always at 0 */
	key = BNXT_QPLIB_INIT_DBHDR(info->xid, type, 0, toggle);
	bnxt_qplib_write_db(info, key, info->priv_db,
			    &info->shadow_key_arm_ena);
}

static inline void bnxt_qplib_cq_coffack_db(struct bnxt_qplib_db_info *info)
{
	u64 key = 0;

	/* Index always at 0 */
	key = BNXT_QPLIB_INIT_DBHDR(info->xid, DBC_DBC_TYPE_CQ_CUTOFF_ACK, 0, 0);
	bnxt_qplib_write_db(info, key, info->priv_db, &info->shadow_key);
}

static inline void bnxt_qplib_srq_arm_db(struct bnxt_qplib_db_info *info, u32 th)
{
	u64 key = 0;
	u8 toggle = 0;

	toggle = info->toggle;
	/* Index always at 0 */
	key = BNXT_QPLIB_INIT_DBHDR(info->xid, DBC_DBC_TYPE_SRQ_ARM, th, toggle);
	bnxt_qplib_write_db(info, key, info->priv_db, &info->shadow_key);
}

static inline void bnxt_qplib_ring_nq_db(struct bnxt_qplib_db_info *info,
					 struct bnxt_qplib_chip_ctx *cctx,
					 bool arm)
{
	u32 type;

	type = arm ? DBC_DBC_TYPE_NQ_ARM : DBC_DBC_TYPE_NQ;
	if (_is_chip_p5_plus(cctx))
		bnxt_qplib_ring_db(info, type);
	else
		bnxt_qplib_ring_db32(info, arm);
}

struct bnxt_qplib_max_res {
	u32 max_qp;
	u32 max_mr;
	u32 max_cq;
	u32 max_srq;
	u32 max_ah;
	u32 max_pd;
};

/*
 * Defines for maximum resources supported for chip revisions
 * Maximum PDs supported are restricted to Max QPs
 * GENP4 - Wh+
 * GENP7 - Thor2
 * DEFAULT - Thor
 */
#define BNXT_QPLIB_GENP4_PF_MAX_QP	(16 * 1024)
#define BNXT_QPLIB_GENP4_PF_MAX_MRW	(16 * 1024)
#define BNXT_QPLIB_GENP4_PF_MAX_CQ	(16 * 1024)
#define BNXT_QPLIB_GENP4_PF_MAX_SRQ	(1 * 1024)
#define BNXT_QPLIB_GENP4_PF_MAX_AH	(16 * 1024)
#define BNXT_QPLIB_GENP4_PF_MAX_PD	 BNXT_QPLIB_GENP4_PF_MAX_QP

#define BNXT_QPLIB_DEFAULT_PF_MAX_QP	(64 * 1024)
#define BNXT_QPLIB_DEFAULT_PF_MAX_MRW	(256 * 1024)
#define BNXT_QPLIB_DEFAULT_PF_MAX_CQ	(64 * 1024)
#define BNXT_QPLIB_DEFAULT_PF_MAX_SRQ	(4 * 1024)
#define BNXT_QPLIB_DEFAULT_PF_MAX_AH	(128 * 1024)
#define BNXT_QPLIB_DEFAULT_PF_MAX_PD	BNXT_QPLIB_DEFAULT_PF_MAX_QP

#define BNXT_QPLIB_DEFAULT_VF_MAX_QP	(6 * 1024)
#define BNXT_QPLIB_DEFAULT_VF_MAX_MRW	(6 * 1024)
#define BNXT_QPLIB_DEFAULT_VF_MAX_CQ	(6 * 1024)
#define BNXT_QPLIB_DEFAULT_VF_MAX_SRQ	(4 * 1024)
#define BNXT_QPLIB_DEFAULT_VF_MAX_AH	(6 * 1024)
#define BNXT_QPLIB_DEFAULT_VF_MAX_PD	BNXT_QPLIB_DEFAULT_VF_MAX_QP
#define BNXT_QPLIB_UPDATE_PERIOD_MS	250

#ifdef BNXT_FPGA
#define BNXT_QPLIB_GENP7_FPGA_PF_MAX_QP	(256)
#define BNXT_QPLIB_GENP7_FPGA_PF_MAX_MRW (2 * 1024)
#define BNXT_QPLIB_GENP7_FPGA_PF_MAX_CQ	(256)
#define BNXT_QPLIB_GENP7_FPGA_PF_MAX_SRQ (256)
#define BNXT_QPLIB_GENP7_FPGA_PF_MAX_AH	(256)
#define BNXT_QPLIB_GENP7_FPGA_PF_MAX_PD	BNXT_QPLIB_GENP7_FPGA_PF_MAX_QP

#define BNXT_QPLIB_GENP7_FPGA_VF_MAX_QP	(64)
#define BNXT_QPLIB_GENP7_FPGA_VF_MAX_MRW (1024)
#define BNXT_QPLIB_GENP7_FPGA_VF_MAX_CQ	(64)
#define BNXT_QPLIB_GENP7_FPGA_VF_MAX_SRQ (64)
#define BNXT_QPLIB_GENP7_FPGA_VF_MAX_AH (64)
#define BNXT_QPLIB_GENP7_FPGA_VF_MAX_PD BNXT_QPLIB_GENP7_FPGA_VF_MAX_QP
#define BNXT_QPLIB_UPDATE_PERIOD_MS	1000
#endif

static inline void bnxt_qplib_max_res_supported(struct bnxt_qplib_chip_ctx *cctx,
						struct bnxt_qplib_res *qpl_res,
						struct bnxt_qplib_max_res *max_res,
						bool vf_res_limit)
{
	switch (cctx->chip_num) {
	case CHIP_NUM_57608:
	case CHIP_NUM_58818:
#ifdef BNXT_FPGA
		if (vf_res_limit) {
			max_res->max_qp = BNXT_QPLIB_GENP7_FPGA_VF_MAX_QP;
			max_res->max_mr = BNXT_QPLIB_GENP7_FPGA_VF_MAX_MRW;
			max_res->max_cq = BNXT_QPLIB_GENP7_FPGA_VF_MAX_CQ;
			max_res->max_srq = BNXT_QPLIB_GENP7_FPGA_VF_MAX_SRQ;
			max_res->max_ah = BNXT_QPLIB_GENP7_FPGA_VF_MAX_AH;
			max_res->max_pd = BNXT_QPLIB_GENP7_FPGA_VF_MAX_PD;
		} else {
			max_res->max_qp = BNXT_QPLIB_GENP7_FPGA_PF_MAX_QP;
			max_res->max_mr = BNXT_QPLIB_GENP7_FPGA_PF_MAX_MRW;
			max_res->max_cq = BNXT_QPLIB_GENP7_FPGA_PF_MAX_CQ;
			max_res->max_srq = BNXT_QPLIB_GENP7_FPGA_PF_MAX_SRQ;
			max_res->max_ah = BNXT_QPLIB_GENP7_FPGA_PF_MAX_AH;
			max_res->max_pd = BNXT_QPLIB_GENP7_FPGA_PF_MAX_PD;
		}
		break;
#endif
	case CHIP_NUM_57504:
	case CHIP_NUM_57502:
	case CHIP_NUM_57508:
		if (vf_res_limit) {
			max_res->max_qp = BNXT_QPLIB_DEFAULT_VF_MAX_QP;
			max_res->max_mr = BNXT_QPLIB_DEFAULT_VF_MAX_MRW;
			max_res->max_cq = BNXT_QPLIB_DEFAULT_VF_MAX_CQ;
			max_res->max_srq = BNXT_QPLIB_DEFAULT_VF_MAX_SRQ;
			max_res->max_ah = BNXT_QPLIB_DEFAULT_VF_MAX_AH;
			max_res->max_pd = BNXT_QPLIB_DEFAULT_VF_MAX_PD;
		} else {
			max_res->max_qp = BNXT_QPLIB_DEFAULT_PF_MAX_QP;
			max_res->max_mr = BNXT_QPLIB_DEFAULT_PF_MAX_MRW;
			max_res->max_cq = BNXT_QPLIB_DEFAULT_PF_MAX_CQ;
			max_res->max_srq = BNXT_QPLIB_DEFAULT_PF_MAX_SRQ;
			max_res->max_ah = BNXT_QPLIB_DEFAULT_PF_MAX_AH;
			max_res->max_pd = BNXT_QPLIB_DEFAULT_PF_MAX_PD;
		}
		break;
	default:
		/* Wh+/Stratus max resources */
		max_res->max_qp = BNXT_QPLIB_GENP4_PF_MAX_QP;
		max_res->max_mr = BNXT_QPLIB_GENP4_PF_MAX_MRW;
		max_res->max_cq = BNXT_QPLIB_GENP4_PF_MAX_CQ;
		max_res->max_srq = BNXT_QPLIB_GENP4_PF_MAX_SRQ;
		max_res->max_ah = BNXT_QPLIB_GENP4_PF_MAX_AH;
		max_res->max_pd = BNXT_QPLIB_GENP4_PF_MAX_PD;
		break;
	}
}

static inline u32 bnxt_re_cap_fw_res(u32 fw_val, u32 drv_cap, bool sw_max_en)
{
	if (sw_max_en)
		return fw_val;
	return min_t(u32, fw_val, drv_cap);
}
#endif
