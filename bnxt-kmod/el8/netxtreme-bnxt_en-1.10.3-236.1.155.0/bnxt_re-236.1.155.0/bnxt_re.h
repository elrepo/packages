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
 * Description: main (header)
 */

#ifndef __BNXT_RE_H__
#define __BNXT_RE_H__

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <net/dcbnl.h>
#include <net/ipv6.h>
#include <net/inet_ecn.h>
#include <net/addrconf.h>
#include <linux/if_ether.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#if defined(HAVE_DISASSOCIATE_UCNTX) && defined(HAVE_SCHED_MM_H)
#include <linux/sched/mm.h>
#endif
#if defined(HAVE_DISASSOCIATE_UCNTX) && defined(HAVE_SCHED_TASK_H)
#include <linux/sched/task.h>
#endif
#include <linux/hashtable.h>

#ifdef LEGACY_BOND_SUPPORT
#ifdef HAVE_NET_BONDING_H
#include <net/bonding.h>
#endif
#endif

#include <rdma/ib_verbs.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_mad.h>
#include <rdma/ib_cache.h>
#if defined (HAVE_UVERBS_IOCTL_H)
#include <rdma/uverbs_ioctl.h>
#endif
#ifdef HAVE_DEVLINK
#include <net/devlink.h>
#endif

#include "bnxt_ulp.h"
#include "bnxt_dcqcn.h"
#include "roce_hsi.h"
#include "qplib_res.h"
#include "qplib_sp.h"
#include "qplib_fp.h"
#include "qplib_rcfw.h"
#include "ib_verbs.h"
#include "stats.h"
#include "compat.h"

#define ROCE_DRV_MODULE_NAME		"bnxt_re"
#define ROCE_DRV_MODULE_VERSION "236.1.155.0"
#define ROCE_DRV_MODULE_RELDATE "December 23, 2025"

#define BNXT_RE_REF_WAIT_COUNT		20
#define BNXT_RE_ROCE_V1_ETH_TYPE	0x8915
#define BNXT_RE_ROCE_V2_PORT_NO		4791
#define BNXT_RE_RES_FREE_WAIT_COUNT	5000

#define BNXT_RE_PAGE_SHIFT_4K		(12)
#define BNXT_RE_PAGE_SHIFT_8K		(13)
#define BNXT_RE_PAGE_SHIFT_64K		(16)
#define BNXT_RE_PAGE_SHIFT_2M		(21)
#define BNXT_RE_PAGE_SHIFT_8M		(23)
#define BNXT_RE_PAGE_SHIFT_1G		(30)

#define BNXT_RE_PAGE_SIZE_4K		BIT(BNXT_RE_PAGE_SHIFT_4K)
#define BNXT_RE_PAGE_SIZE_8K		BIT(BNXT_RE_PAGE_SHIFT_8K)
#define BNXT_RE_PAGE_SIZE_64K		BIT(BNXT_RE_PAGE_SHIFT_64K)
#define BNXT_RE_PAGE_SIZE_2M		BIT(BNXT_RE_PAGE_SHIFT_2M)
#define BNXT_RE_PAGE_SIZE_8M		BIT(BNXT_RE_PAGE_SHIFT_8M)
#define BNXT_RE_PAGE_SIZE_1G		BIT(BNXT_RE_PAGE_SHIFT_1G)

#define BNXT_RE_MAX_MR_SIZE_LOW		BIT(BNXT_RE_PAGE_SHIFT_1G)
#define BNXT_RE_MAX_MR_SIZE_HIGH	BIT(39)
#define BNXT_RE_MAX_MR_SIZE		BNXT_RE_MAX_MR_SIZE_HIGH
#define BNXT_RE_WARN_MR_SZ		BIT(27)

#define BNXT_RE_MAX_GID_PER_VF		128

#define BNXT_RE_RQ_WQE_THRESHOLD	32
#define BNXT_RE_UD_QP_HW_STALL		0x400000

/*
 * Setting the default ack delay value to 16, which means
 * the default timeout is approx. 260ms(4 usec * 2 ^(timeout))
 */

#define BNXT_RE_DEFAULT_ACK_DELAY	16
#define BNXT_RE_BOND_PF_MAX		2

#define BNXT_RE_STATS_CTX_UPDATE_TIMER	250

#define BNXT_RE_CHIP_P7(chip_num)	\
		((chip_num) == CHIP_NUM_58818 ||\
		 (chip_num) == CHIP_NUM_57608)

#define BNXT_RE_MIN_KERNEL_QP_TX_DEPTH	4096
#define BNXT_RE_STOP_QPS_BUDGET		200

#define BNXT_RE_HWRM_CMD_TIMEOUT(rdev) \
		((rdev)->chip_ctx->hwrm_cmd_max_timeout * 1000)

#define BNXT_RE_CONTEXT_TYPE_QPC_SIZE_P5	1088
#define BNXT_RE_CONTEXT_TYPE_CQ_SIZE_P5		128
#define BNXT_RE_CONTEXT_TYPE_MRW_SIZE_P5	128
#define BNXT_RE_CONTEXT_TYPE_SRQ_SIZE_P5	192

#define BNXT_RE_CONTEXT_TYPE_QPC_SIZE_P7	1088
#define BNXT_RE_CONTEXT_TYPE_CQ_SIZE_P7		192
#define BNXT_RE_CONTEXT_TYPE_MRW_SIZE_P7	192
#define BNXT_RE_CONTEXT_TYPE_SRQ_SIZE_P7	192

extern unsigned int min_tx_depth;
extern struct mutex bnxt_re_mutex;
extern struct list_head bnxt_re_dev_list;

struct bnxt_re_ring_attr {
	dma_addr_t	*dma_arr;
	int		pages;
	int	 	type;
	u32		depth;
	u32		lrid; /* Logical ring id */
	u16		flags;
	u8		mode;
};

#define BNXT_RE_MAX_MSIX		64
#define BNXT_RE_MIN_MSIX		2
struct bnxt_re_nq_record {
	struct bnxt_msix_entry	msix_entries[BNXT_RE_MAX_MSIX];
	/* FP Notification Queue (CQ & SRQ) */
	struct bnxt_qplib_nq    nq[BNXT_RE_MAX_MSIX];
	int			num_msix;
	int			max_init;
	/* Serialize access to NQ record */
	struct mutex		load_lock;
};

struct bnxt_re_work {
	struct work_struct	work;
	unsigned long		event;
	struct bnxt_re_dev      *rdev;
	struct net_device	*vlan_dev;
	struct netdev_bonding_info  netdev_binfo;
	struct bnxt_re_bond_info *binfo;
	/* netdev where we received the event */
	struct net_device *netdev;
	struct net_device *bond_netdev;
	struct auxiliary_device *adev;
};

struct bnxt_re_bond_info {
	struct bnxt_re_dev *rdev;
	struct bnxt_re_dev *rdev_peer;
	struct net_device *master;
	struct net_device *slave1;
	struct net_device *slave2;
	struct pci_dev *pdev1;
	struct pci_dev *pdev2;
	struct netdev_bonding_info nbinfo;
	struct auxiliary_device *aux_dev1;	/* Corresponds to slave1 */
	struct auxiliary_device *aux_dev2;	/* Corresponds to slave2 */
	u8 active_port_map;
	u8 aggr_mode;
	u8 gsi_qp_mode;
	u8 wqe_mode;
};

/*
 * Data structure and defines to handle
 * recovery
 */
#define BNXT_RE_PRE_RECOVERY_REMOVE 0x1
#define BNXT_RE_COMPLETE_REMOVE 0x2
#define BNXT_RE_POST_RECOVERY_INIT 0x4
#define BNXT_RE_COMPLETE_INIT 0x8

/* QP1 SQ entry data structure */
struct bnxt_re_sqp_entries {
	u64 wrid;
	struct bnxt_qplib_sge sge;
	/* For storing the actual qp1 cqe */
	struct bnxt_qplib_cqe cqe;
	struct bnxt_re_qp *qp1_qp;
};

/* GSI QP mode enum */
enum bnxt_re_gsi_mode {
	BNXT_RE_GSI_MODE_INVALID = 0,
	BNXT_RE_GSI_MODE_ALL = 1,
	BNXT_RE_GSI_MODE_ROCE_V1,
	BNXT_RE_GSI_MODE_ROCE_V2_IPV4,
	BNXT_RE_GSI_MODE_ROCE_V2_IPV6,
	BNXT_RE_GSI_MODE_UD
};

enum bnxt_re_roce_cap {
	BNXT_RE_FLAG_ROCEV1_CAP = 1,
	BNXT_RE_FLAG_ROCEV2_CAP,
	BNXT_RE_FLAG_ROCEV1_V2_CAP,
};

#define BNXT_RE_MAX_GSI_SQP_ENTRIES	1024
struct bnxt_re_gsi_context {
	u8	gsi_qp_mode;
	bool	first_cq_created;
	/* Start: used only in gsi_mode_all */
	struct	bnxt_re_qp *gsi_qp;
	struct	bnxt_re_qp *gsi_sqp;
	struct	bnxt_re_ah *gsi_sah;
	struct	bnxt_re_sqp_entries *sqp_tbl;
	/* End: used only in gsi_mode_all */
};

struct bnxt_re_tc_rec {
	u8 cos_id_roce;
	u8 tc_roce;
	u8 cos_id_cnp;
	u8 tc_cnp;
	u8 tc_def;
	u8 cos_id_def;
	u8 max_tc;
	u8 roce_prio;
	u8 cnp_prio;
	u8 roce_dscp;
	u8 cnp_dscp;
	u8 prio_valid;
	u8 dscp_valid;
	bool ecn_enabled;
	bool serv_type_enabled;
	u64 cnp_dscp_bv;
	u64 roce_dscp_bv;
};

struct bnxt_re_dscp2pri {
	u8 dscp;
	u8 mask;
	u8 pri;
};

struct bnxt_re_cos2bw_cfg {
	u8	pad[3];
	struct_group_attr(cfg, __packed,
		u8	queue_id;
		__le32	min_bw;
		__le32	max_bw;
		u8	tsa;
		u8	pri_lvl;
		u8	bw_weight;
	);
	u8	unused;
};

#define BNXT_RE_AEQ_IDX			0
#define BNXT_RE_MAX_SGID_ENTRIES	256

#define BNXT_RE_DBGFS_FILE_MEM          65536
enum {
	BNXT_RE_STATS_QUERY = 1,
	BNXT_RE_QP_QUERY = 2,
	BNXT_RE_SERVICE_FN_QUERY = 3,
};

struct bnxt_re_en_dev_info {
	struct list_head en_list;
	struct bnxt_en_dev *en_dev;
	struct bnxt_re_dev *rdev;
	unsigned long flags;
#define BNXT_RE_FLAG_EN_DEV_NETDEV_REG		0
#define BNXT_RE_FLAG_EN_DEV_PRIMARY_DEV		1
#define BNXT_RE_FLAG_EN_DEV_SECONDARY_DEV	2
	u8 wqe_mode;
	u8 gsi_mode;
	bool te_bypass;
	bool binfo_valid;
	struct bnxt_re_bond_info binfo;
	u32 event_bitmap[3];
};

#define BNXT_RE_MAX_FIFO_DEPTH_P5       0x2c00
#define BNXT_RE_MAX_FIFO_DEPTH_P7       0x8000

#define BNXT_RE_MAX_FIFO_DEPTH(ctx)	\
	(_is_chip_p7((ctx)) ? \
	 BNXT_RE_MAX_FIFO_DEPTH_P7 :\
	 BNXT_RE_MAX_FIFO_DEPTH_P5)

struct bnxt_dbq_nq_list {
	int num_nql_entries;
	u16 nq_id[16];
};

#define BNXT_RE_ASYNC_ERR_REP_BASE(_type)				\
	 (ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_##_type)

#define BNXT_RE_ASYNC_ERR_DBR_TRESH(_type)				\
	 (ASYNC_EVENT_CMPL_ERROR_REPORT_DOORBELL_DROP_THRESHOLD_##_type)

#define BNXT_RE_EVENT_DBR_EPOCH(data)					\
	(((data) &							\
	  BNXT_RE_ASYNC_ERR_DBR_TRESH(EVENT_DATA1_EPOCH_MASK)) >>	\
	 BNXT_RE_ASYNC_ERR_DBR_TRESH(EVENT_DATA1_EPOCH_SFT))

#define BNXT_RE_EVENT_ERROR_REPORT_TYPE(data1)				\
	(((data1) &							\
	  BNXT_RE_ASYNC_ERR_REP_BASE(TYPE_MASK))  >>			\
	 BNXT_RE_ASYNC_ERR_REP_BASE(TYPE_SFT))

#define BNXT_RE_RES_LIST_ADD(_rdev, _res, _type)			\
{									\
	spin_lock(&(_rdev)->res_list[_type].lock);			\
	list_add_tail(&(_res)->res_list,				\
		      &(_rdev)->res_list[_type].head);			\
	spin_unlock(&(_rdev)->res_list[_type].lock);			\
}

#define BNXT_RE_RES_LIST_DEL(_rdev, _res, _type)			\
{									\
	spin_lock(&(_rdev)->res_list[_type].lock);			\
	list_del(&(_res)->res_list);					\
	spin_unlock(&(_rdev)->res_list[_type].lock);			\
}

#define BNXT_RE_CQ_PAGE_LIST_ADD(_uctx, _cq)				\
{									\
	mutex_lock(&(_uctx)->cq_lock);					\
	list_add_tail(&(_cq)->cq_list, &(_uctx)->cq_list);		\
	mutex_unlock(&(_uctx)->cq_lock);				\
}

#define BNXT_RE_CQ_PAGE_LIST_DEL(_uctx, _cq)				\
{									\
	mutex_lock(&(_uctx)->cq_lock);					\
	list_del(&(_cq)->cq_list);					\
	mutex_unlock(&(_uctx)->cq_lock);				\
}

#define BNXT_RE_SRQ_PAGE_LIST_ADD(_uctx, _srq)				\
{									\
	mutex_lock(&(_uctx)->srq_lock);					\
	list_add_tail(&(_srq)->srq_list, &(_uctx)->srq_list);		\
	mutex_unlock(&(_uctx)->srq_lock);				\
}

#define BNXT_RE_SRQ_PAGE_LIST_DEL(_uctx, _srq)				\
{									\
	mutex_lock(&(_uctx)->srq_lock);					\
	list_del(&(_srq)->srq_list);					\
	mutex_unlock(&(_uctx)->srq_lock);				\
}

#define BNXT_RE_NETDEV_EVENT(event, x)					\
	do {								\
		if ((event) == (x))					\
			return #x;					\
	} while (0)

/* Do not change the seq of this enum which is followed by dbr recov */
enum {
	BNXT_RE_RES_TYPE_CQ = 0,
	BNXT_RE_RES_TYPE_UCTX,
	BNXT_RE_RES_TYPE_QP,
	BNXT_RE_RES_TYPE_SRQ,
	BNXT_RE_RES_TYPE_MR,
	BNXT_RE_RES_TYPE_MAX
};

struct bnxt_re_res_list {
	struct list_head head;
	spinlock_t lock;
};

enum {
	BNXT_RE_DV_RES_TYPE_QP = 0,
	BNXT_RE_DV_RES_TYPE_CQ,
	BNXT_RE_DV_RES_TYPE_MAX
};

struct bnxt_re_dbr_drop_recov_work {
	struct work_struct work;
	struct bnxt_re_dev *rdev;
	u32 curr_epoch;
};

struct bnxt_re_aer_work {
	struct work_struct work;
	struct bnxt_re_dev *rdev;
};

struct bnxt_re_udcc_work {
	struct work_struct work;
	struct bnxt_re_dev *rdev;
	u32 session_id;
	u8 session_opcode;
};

struct bnxt_re_dbq_stats {
	u64 fifo_occup_slab_1;
	u64 fifo_occup_slab_2;
	u64 fifo_occup_slab_3;
	u64 fifo_occup_slab_4;
	u64 fifo_occup_water_mark;
	u64 do_pacing_slab_1;
	u64 do_pacing_slab_2;
	u64 do_pacing_slab_3;
	u64 do_pacing_slab_4;
	u64 do_pacing_slab_5;
	u64 do_pacing_water_mark;
	u64 do_pacing_retry;
};

struct bnxt_re_dbg_mad {
	u64 mad_consumed;
	u64 mad_processed;
};

struct bnxt_re_dbg_snapdump {
	atomic_t context_saved_count;
};

/* Device debug statistics */
struct bnxt_re_drv_dbg_stats {
	struct bnxt_re_dbq_stats dbq;
	struct bnxt_re_dbg_mad mad;
	struct bnxt_re_dbg_snapdump snapdump;
};

/* DB pacing counters */
struct bnxt_re_dbr_sw_stats {
	u64 dbq_int_recv;
	u64 dbq_int_en;
	u64 dbq_pacing_resched;
	u64 dbq_pacing_complete;
	u64 dbq_pacing_alerts;
	u64 dbr_drop_recov_events;
	u64 dbr_drop_recov_timeouts;
	u64 dbr_drop_recov_timeout_users;
	u64 dbr_drop_recov_event_skips;
};

/* RoCE push counters */
struct bnxt_re_ppp_sw_stats {
	u32 ppp_enabled_ctxs;
	u32 ppp_enabled_qps;
};

#define BNXT_RE_UDCC_MAX_SESSIONS	4096
#define BNXT_RE_UDCC_INPUT_ARG_LENGTH 40
struct bnxt_re_udcc_cfg {
	u8		enable;

	u16		max_comp_cfg_xfer;
	u16		max_comp_data_xfer;
	u16		max_sessions;
	u16		min_sessions;
	u8		session_type;
	u16		flags;

	u8		cfg_arg[BNXT_RE_UDCC_INPUT_ARG_LENGTH];
	int		cfg_arg_len;
	void		*cfg;
	dma_addr_t	cfg_map;
	u32		cfg_len;

	u8		data_arg[BNXT_RE_UDCC_INPUT_ARG_LENGTH];
	int		data_arg_len;
	void		*data;
	dma_addr_t	data_map;
	u32		data_len;

	u8		probe_pad_cnt_cfg;
	bool		probe_pad_cnt_updated;

	struct dentry	*cfg_entry;
	u8		rtt_num_of_buckets;
};


struct qdump_qpinfo {
	u32 id;
	u32 dest_qpid;
	u64 qp_handle;
	u32 mtu;
	u8  type;
	u8  wqe_mode;
	u8  state;
	u8  is_user;
	u64 scq_handle;
	u64 rcq_handle;
	u32 scq_id;
	u32 rcq_id;
};

struct qdump_mrinfo {
	int type;
	u32 lkey;
	u32 rkey;
	u64 total_size;
	u64 mr_handle;
};

struct qdump_element {
	struct bnxt_qplib_pbl pbl[PBL_LVL_MAX];
	enum bnxt_qplib_pbl_lvl level;
	struct bnxt_qplib_hwq *hwq;
	struct bnxt_re_dev *rdev;
	struct ib_umem *uaddr_prod;
	struct ib_umem *uaddr_cons;
	struct ib_umem *umem;
	bool is_user_qp;
	char des[32];
	char *buf;
	size_t len;
	u16 stride;
	u32 prod;
	u32 cons;
};

struct qdump_array {
	struct qdump_qpinfo qpinfo;
	struct qdump_mrinfo mrinfo;
	struct qdump_element sqd;
	struct qdump_element rqd;
	struct qdump_element scqd;
	struct qdump_element rcqd;
	struct qdump_element mrd;
	bool valid;
	bool is_mr;
};

struct bnxt_re_qdump_head {
	struct qdump_array *qdump;
	u32 max_elements;
	struct mutex lock; /* lock qdump array elements */
	u32 index;
};

struct bnxt_re_session_info {
	bool valid_session;
	u16 session_id;
	u8 resp_flags;
	u8 udcc_sess_sz;
	void *udcc_data;
};

struct bnxt_re_udcc_data_head {
	struct bnxt_re_session_info ses[BNXT_RE_UDCC_MAX_SESSIONS];
	void *udcc_data_arr;
	struct mutex lock; /* lock for UDCC data processing */
	u32 max_elements;
	u32 index;
};

#define MAX_DSCP_PRI_TUPLE	64
#define MAX_LOSS_LESS_QUEUES	4

enum {
	BNXT_RE_SNAPDUMP_NONE = 0,
	BNXT_RE_SNAPDUMP_ERR,
	/* Add new entry before this */
	BNXT_RE_SNAPDUMP_ALL
};

struct bnxt_re_udcc_dbg_info {
	u32				session_id;
	struct dentry                   *session_dbgfs_dir;
	struct bnxt_re_dev		*rdev;
};

#define MAX_CQ_HASH_BITS		(16)
#define MAX_SRQ_HASH_BITS		(16)
struct bnxt_re_dev {
	struct ib_device		ibdev;
	struct list_head		list;
	atomic_t			ref_count;
	atomic_t			sched_count;
	unsigned long			flags;
#define BNXT_RE_FLAG_NETDEV_REGISTERED		0
#define BNXT_RE_FLAG_IBDEV_REGISTERED		1
#define BNXT_RE_FLAG_ALLOC_RCFW			4
#define BNXT_RE_FLAG_NET_RING_ALLOC		5
#define BNXT_RE_FLAG_RCFW_CHANNEL_EN		6
#define BNXT_RE_FLAG_ALLOC_CTX			7
#define BNXT_RE_FLAG_STATS_CTX_ALLOC		8
#define BNXT_RE_FLAG_STATS_CTX2_ALLOC		9
#define BNXT_RE_FLAG_RCFW_CHANNEL_INIT		10
#define BNXT_RE_FLAG_WORKER_REG			11
#define BNXT_RE_FLAG_TBLS_ALLOCINIT		12
#define BNXT_RE_FLAG_SETUP_NQ			13
#define BNXT_RE_FLAG_BOND_DEV_REGISTERED	14
#define BNXT_RE_FLAG_PER_PORT_DEBUG_INFO	15
#define BNXT_RE_FLAG_DEV_LIST_INITIALIZED	16
#define BNXT_RE_FLAG_ERR_DEVICE_DETACHED	17
#define BNXT_RE_FLAG_INIT_CC_PARAM		18
#define BNXT_RE_FLAG_STOP_IN_PROGRESS		20
#define BNXT_RE_FLAG_ISSUE_ROCE_STATS		29
#define BNXT_RE_FLAG_ISSUE_CFA_FLOW_STATS	30
#define BNXT_RE_FLAG_STATS_CTX3_ALLOC		31
	struct net_device		*netdev;
	struct auxiliary_device		*adev;
	struct bnxt_qplib_chip_ctx	*chip_ctx;
	struct bnxt_en_dev		*en_dev;
	struct bnxt_re_nq_record	*nqr;
	struct bnxt_re_tc_rec		tc_rec[2];
	struct delayed_work		worker;
	u16				worker_30s;
	/* Max of 2 lossless traffic class supported per port */
	u16				cosq[2];
	u8				cur_prio_map;
	u8				roce_mode;
	/* RCFW Channel */
	struct bnxt_qplib_rcfw		rcfw;
	/* Device Resources */
	struct bnxt_qplib_dev_attr	*dev_attr;
	struct bnxt_qplib_res		qplib_res;
	struct bnxt_qplib_dpi		dpi_privileged;
	struct bnxt_qplib_cc_param	cc_param;
	struct bnxt_qplib_cq_coal_param cq_coalescing;
	/* serialize update of CC param */
	struct mutex			cc_lock;
	/* serialize access to active qp list */
	struct mutex			qp_lock;
	struct list_head		qp_list;

	/* Start: QP for handling QP1 packets */
	struct bnxt_re_gsi_context	gsi_ctx;
	/* End: QP for handling QP1 packets */
	u32				espeed;
	/*
	 * For storing the speed of slave interfaces.
	 * Same as espeed when bond is not configured
	 */
	u32				sl_espeed;
	/* To be used for a workaround for ISER stack */
	u32				min_tx_depth;
	/* To enable qp debug info. Disabled during driver load */
	u32				en_qp_dbg;
	struct bnxt_re_bond_info	*binfo;
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
	/* Array to handle gid mapping */
	char				*gid_map;
#endif /* RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP */

	struct bnxt_re_device_stats	stats;
	struct bnxt_re_drv_dbg_stats	*dbg_stats;
	struct bnxt_re_udcc_dbg_info	*udcc_dbg_info;
	struct workqueue_struct		*udcc_wq;
	/* debugfs to expose per port information*/
	struct dentry                   *port_debug_dir;
	struct dentry                   *info;
	struct dentry                   *drv_dbg_stats;
	struct dentry                   *sp_perf_stats;
	struct dentry                   *pdev_debug_dir;
	struct dentry                   *config_debugfs_dir;
	struct dentry                   *pdev_qpinfo_dir;
	struct dentry                   *pdev_debug_dump_dir;
	struct dentry                   *pte_pbl_info;
	struct dentry                   *peer_mmap;
	struct dentry                   *udcc_dbgfs_dir;
	struct dentry                   *rate_limit_cfg;
	struct workqueue_struct		*resolve_wq;
	struct list_head		mac_wq_list;
	struct workqueue_struct		*dcb_wq;
	struct workqueue_struct		*aer_wq;
	u64	dbr_throttling_reg_off;
	u64	dbr_aeq_arm_reg_off;
	u64	dbr_db_fifo_reg_off;
	void *dbr_pacing_page;
	u64 dbr_pacing_bar;
	u32 pacing_algo_th;
	u32 pacing_en_int_th;
	u32 do_pacing_save;
	refcount_t			pos_destah_cnt;
	struct workqueue_struct		*dest_ah_wq;
	struct workqueue_struct		*dbq_wq;
	struct workqueue_struct		*dbr_drop_recov_wq;
	struct work_struct		dbq_fifo_check_work;
	struct delayed_work		dbq_pacing_work;
	DECLARE_HASHTABLE(cq_hash, MAX_CQ_HASH_BITS);
	DECLARE_HASHTABLE(srq_hash, MAX_SRQ_HASH_BITS);
#define PEER_MMAP_WORK_SCHED_DELAY	100	/* msecs */
	struct delayed_work		peer_mmap_work;
	/* protect DB pacing */
	struct mutex dbq_lock;
	/* Control DBR pacing feature. Set if enabled */
	bool dbr_pacing;
	/* Control DBR recovery feature. Set if enabled */
	bool dbr_drop_recov;
	bool user_dbr_drop_recov;
	/* DBR recovery feature. Set if running */
	bool dbr_recovery_on;
	u32 user_dbr_drop_recov_timeout;
	 /*
	  * Value used for pacing algo when pacing is active
	  */
#define BNXT_RE_MAX_DBR_DO_PACING 0xFFFF
	u32 dbr_do_pacing;
	u32 dbq_watermark; /* Current watermark set in HW registers */
	u32 dbq_nq_id; /* Current NQ ID for DBQ events */
	u32 dbq_pacing_time; /* ms */
	u32 dbr_def_do_pacing; /* do_pacing when no congestion */
	u32 dbr_evt_curr_epoch;
	bool dbq_int_disable;

	bool	is_virtfn;
	bool	unreg_sched;

	atomic_t dbq_intr_running;

	struct bnxt_re_dbr_sw_stats *dbr_sw_stats;
	struct bnxt_re_res_list res_list[BNXT_RE_RES_TYPE_MAX];
	struct bnxt_dbq_nq_list nq_list;
#ifdef IB_PEER_MEM_MOD_SUPPORT
	struct ib_peer_mem_device *peer_dev;
#endif
	char dev_name[IB_DEVICE_NAME_MAX + 1];

	struct bnxt_re_ppp_sw_stats ppp_stats;

	/* HW based DB drop recovery feature */
	bool			hdbr_enabled;
	struct list_head	*hdbr_privileged;
	struct list_head	hdbr_apps;
	struct mutex		hdbr_lock; /* protect hdbr_apps list */
	volatile u8		*hdbr_dt;
	void			*hdbr_dbgfs;
	struct workqueue_struct	*hdbr_wq;
	struct list_head	hdbr_fpgs;
	struct mutex		hdbr_fpg_lock; /* protect free page list */

	/* UDCC */
	struct bnxt_re_udcc_cfg udcc_cfg;

	u8                      p2cos[IEEE_8021QAZ_MAX_TCS];
	u8                      lossless_q_count;
	u8                      d2p_count;
	u8                      *lossless_qid;
	struct bnxt_re_dscp2pri *d2p;

	/* Head to track all QP dump */
	struct bnxt_re_qdump_head qdump_head;

	/* Head to track all udcc session data dump */
	struct bnxt_re_udcc_data_head udcc_data_head;

	/* Config option to tune QP/MR dbg feature */
	u8 snapdump_dbg_lvl;
	u8                      enable_queue_overflow_telemetry;
	u32			qp_rate_limit_cfg;
	struct dentry           *cq_coal_cfg_dir;
	struct bnxt_re_dbg_cq_coal_cfg_params *cq_coal_cfg_params;
	struct dentry                   *cc_config;
	struct bnxt_re_dbg_cc_config_params *cc_config_params;

#ifdef HAVE_ETHTOOL_LINK_KSETTINGS_LANES
	u32			lanes;
#endif
	u16                     active_speed;
	u8                      active_width;

	/* RoCE mirror */
	u16 mirror_vnic_id;
	union ib_gid ugid;

#define BNXT_RE_INVALID_UGID_INDEX	-1
	u32 ugid_index;
	bool deferred_db_enabled;
	u8 sniffer_flow_created:1;

#define BNXT_RE_LIVE_QPDUMP_SZ	(512 * 1024)
	void *qp_dump_buf;

	atomic_t		dv_cq_count;
	atomic_t		dv_qp_count;
};

#define bnxt_re_dev_pcifn_id(rdev)	((rdev)->en_dev->pdev->devfn)

#define BNXT_RE_RESOLVE_RETRY_COUNT_US	5000000 /* 5 sec */
struct bnxt_re_resolve_dmac_work{
        struct work_struct      work;
	struct list_head	list;
	struct bnxt_re_dev 	*rdev;
	RDMA_AH_ATTR 		*ah_attr;
	struct bnxt_re_ah_info *ah_info;
	atomic_t		status_wait;
};

static inline u8 bnxt_re_get_prio(u8 prio_map)
{
	u8 prio = 0xFF;

	for (prio = 0; prio < 8; prio++)
		if (prio_map & (1UL << prio))
			break;
	return prio;
}

static inline bool bnxt_re_is_rdev_valid(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_dev *tmp_rdev;

	list_for_each_entry(tmp_rdev, &bnxt_re_dev_list, list) {
		if (rdev == tmp_rdev) {
			return true;
		}
	}

	pr_debug("bnxt_re: %s : Invalid rdev received rdev = %p\n",
		 __func__, rdev);
	return false;
}

static inline bool _is_bnxt_re_dev_lag_capable(u16 dev_cap_flags)
{
	/*
	 * If the FW provides the LAG support valid bit in device capability
	 * flags, check for the LAG support bit. Otherwise return as LAG
	 * capable by default.
	 */
	if (!(dev_cap_flags & CREQ_QUERY_FUNC_RESP_SB_LINK_AGGR_SUPPORTED_VALID))
		return true;
	if (dev_cap_flags & CREQ_QUERY_FUNC_RESP_SB_LINK_AGGR_SUPPORTED)
		return true;
	return false;
}

bool bnxt_re_is_lag_allowed(ifbond *master, ifslave *slave,
			    struct bnxt_re_dev *rdev);
int bnxt_re_get_port_map(struct netdev_bonding_info *netdev_binfo,
			 struct bnxt_re_bond_info *binfo,
			 struct net_device *netdev);
int bnxt_re_send_hwrm_cmd(struct bnxt_re_dev *rdev, void *cmd,
			  int cmdlen);
void bnxt_re_ib_uninit(struct bnxt_re_dev *rdev);
int bnxt_re_set_hwrm_dscp2pri(struct bnxt_re_dev *rdev,
			      struct bnxt_re_dscp2pri *d2p, u16 count,
			      u16 target_id);
int bnxt_re_query_hwrm_dscp2pri(struct bnxt_re_dev *rdev,
				u16 target_id);
int bnxt_re_query_hwrm_qportcfg(struct bnxt_re_dev *rdev,
				struct bnxt_re_tc_rec *cnprec, u16 tid);
int bnxt_re_hwrm_cos2bw_qcfg(struct bnxt_re_dev *rdev, u16 target_id,
			     struct bnxt_re_cos2bw_cfg *cfg);
int bnxt_re_hwrm_cos2bw_cfg(struct bnxt_re_dev *rdev, u16 target_id,
			    struct bnxt_re_cos2bw_cfg *cfg);
int bnxt_re_hwrm_pri2cos_cfg(struct bnxt_re_dev *rdev,
			     u16 target_id, u16 port_id,
			     u8 *cos_id_map, u8 pri_map);

int bnxt_re_prio_vlan_tx_update(struct bnxt_re_dev *rdev);
int bnxt_re_get_slot_pf_count(struct bnxt_re_dev *rdev);
struct bnxt_re_dev *bnxt_re_get_peer_pf(struct bnxt_re_dev *rdev);
struct bnxt_re_dev *bnxt_re_from_netdev(struct net_device *netdev);
struct bnxt_qplib_nq * bnxt_re_get_nq(struct bnxt_re_dev *rdev);
void bnxt_re_put_nq(struct bnxt_re_dev *rdev, struct bnxt_qplib_nq *nq);

#define to_bnxt_re(ptr, type, member)	\
	container_of(ptr, type, member)

#define to_bnxt_re_dev(ptr, member)	\
	container_of((ptr), struct bnxt_re_dev, member)

/* Even number functions from port 0 and odd number from port 1 */
#define BNXT_RE_IS_PORT0(rdev) (!(rdev->en_dev->pdev->devfn & 1))

#define BNXT_RE_ROCE_V1_PACKET		0
#define BNXT_RE_ROCEV2_IPV4_PACKET	2
#define BNXT_RE_ROCEV2_IPV6_PACKET	3
#define BNXT_RE_ACTIVE_MAP_PORT1    0x1  /*port-1 active */
#define BNXT_RE_ACTIVE_MAP_PORT2    0x2  /*port-2 active */

#define BNXT_RE_MEMBER_PORT_MAP		(BNXT_RE_ACTIVE_MAP_PORT1 | \
					BNXT_RE_ACTIVE_MAP_PORT2)

#define	rdev_to_dev(rdev)	((rdev) ? (&(rdev)->ibdev.dev) : NULL)

void bnxt_re_set_dma_device(struct ib_device *ibdev, struct bnxt_re_dev *rdev);
bool bnxt_re_is_rdev_valid(struct bnxt_re_dev *rdev);

#define bnxt_re_rdev_ready(rdev)	(bnxt_re_is_rdev_valid(rdev) && \
					 (test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags)))
#define BNXT_RE_SRIOV_CFG_TIMEOUT 6
#define BNXT_RE_TIMEOUT_HZ	(8 * HZ)

int bnxt_re_get_device_stats(struct bnxt_re_dev *rdev);
void bnxt_re_remove_device(struct bnxt_re_dev *rdev, u8 removal_type,
			   struct auxiliary_device *aux_dev);
void bnxt_re_destroy_lag(struct bnxt_re_dev **rdev);
int bnxt_re_add_device(struct bnxt_re_dev **rdev,
		       struct net_device *netdev,
		       struct bnxt_re_bond_info *info, u8 qp_mode,
		       u8 op_type, u8 wqe_mode,
		       struct auxiliary_device *aux_dev,
		       bool is_primary);
void bnxt_re_create_base_interface(struct bnxt_re_bond_info *binfo, bool primary);
int bnxt_re_schedule_work(struct bnxt_re_dev *rdev, unsigned long event,
			  struct net_device *vlan_dev,
			  struct netdev_bonding_info *netdev_binfo,
			  struct bnxt_re_bond_info *binfo,
			  struct net_device *netdev,
			  struct auxiliary_device *aux_dev,
			  struct net_device *bond_netdev);
void bnxt_re_get_link_speed(struct bnxt_re_dev *rdev);
int _bnxt_re_ib_init(struct bnxt_re_dev *rdev);
int _bnxt_re_ib_init2(struct bnxt_re_dev *rdev);
int bnxt_re_create_lag(ifbond *master, ifslave *slave,
		       struct netdev_bonding_info *netdev_binfo,
		       struct net_device *netdev,
		       struct bnxt_re_dev **rdev,
		       u8 gsi_mode, u8 wqe_mode);
u8 bnxt_re_get_bond_link_status(struct bnxt_re_bond_info *binfo);

void bnxt_re_init_resolve_wq(struct bnxt_re_dev *rdev);
void bnxt_re_uninit_resolve_wq(struct bnxt_re_dev *rdev);
void bnxt_re_resolve_dmac_task(struct work_struct *work);

#if defined(HAVE_IB_UMEM_DMABUF) && !defined(HAVE_IB_UMEM_DMABUF_PINNED)
struct ib_umem_dmabuf *ib_umem_dmabuf_get_pinned(struct ib_device *device,
						 unsigned long offset,
						 size_t size, int fd,
						 int access);
void ib_umem_dmabuf_release_pinned(struct ib_umem_dmabuf *umem_dmabuf);
#endif
void bnxt_re_get_width_and_speed(u32 netdev_speed, u32 lanes,
				 u16 *speed, u8 *width);
/* The rdev ref_count is to protect immature removal of the device */
static inline void bnxt_re_hold(struct bnxt_re_dev *rdev)
{
	atomic_inc(&rdev->ref_count);
	dev_dbg(rdev_to_dev(rdev),
		"Hold ref_count = 0x%x", atomic_read(&rdev->ref_count));
}

static inline void bnxt_re_put(struct bnxt_re_dev *rdev)
{
	atomic_dec(&rdev->ref_count);
	dev_dbg(rdev_to_dev(rdev),
		"Put ref_count = 0x%x", atomic_read(&rdev->ref_count));
}

void bnxt_re_debugfs_add_port(struct bnxt_re_dev *rdev, char *dev_name);
void bnxt_re_debugfs_rem_port(struct bnxt_re_dev *rdev);
void bnxt_re_add_dbg_files(struct bnxt_re_dev *rdev);
void bnxt_re_rem_dbg_files(struct bnxt_re_dev *rdev);
void bnxt_re_qp_info_add_qpinfo(struct bnxt_re_dev *rdev,
				struct bnxt_re_qp *qp);
void bnxt_re_qp_info_rem_qpinfo(struct bnxt_re_dev *rdev,
				struct bnxt_re_qp *qp);
void bnxt_re_free_qpdump(struct qdump_element *element);

/* Default DCBx and CC values */
#define BNXT_RE_DEFAULT_CNP_DSCP	48
#define BNXT_RE_DEFAULT_CNP_PRI		7
#define BNXT_RE_DEFAULT_ROCE_DSCP	26
#define BNXT_RE_DEFAULT_ROCE_PRI	3

#define ROCE_PRIO_VALID	0x0
#define CNP_PRIO_VALID	0x1
#define ROCE_DSCP_VALID	0x0
#define CNP_DSCP_VALID	0x1

int bnxt_re_get_pri_dscp_settings(struct bnxt_re_dev *rdev,
				  u16 target_id,
				  struct bnxt_re_tc_rec *tc_rec);

int bnxt_re_setup_dscp(struct bnxt_re_dev *rdev);
int bnxt_re_clear_dscp(struct bnxt_re_dev *rdev);
int bnxt_re_setup_cnp_cos(struct bnxt_re_dev *rdev, bool reset);

static inline enum ib_port_state bnxt_re_get_link_state(struct bnxt_re_dev *rdev)
{
	if (netif_running(rdev->netdev) && netif_carrier_ok(rdev->netdev))
		return IB_PORT_ACTIVE;
	return IB_PORT_DOWN;
}

static inline char *bnxt_re_link_state_str(struct bnxt_re_dev *rdev)
{
	return bnxt_re_get_link_state(rdev) == IB_PORT_ACTIVE ? "UP" : "DOWN";
}

static inline int is_cc_enabled(struct bnxt_re_dev *rdev)
{
	return rdev->cc_param.enable;
}

static inline bool bnxt_re_rtnl_trylock(void)
{
	u32 lock_retry = 10;
	bool locked = false;

	while (lock_retry--) {
		if (!rtnl_trylock()) {
			usleep_range(1000, 2000);
			continue;
		} else {
			locked = true;
			break;
		}
	}
	return locked;
}

static inline void bnxt_re_init_hwrm_hdr(struct input *hdr, u16 opcd, u16 trid)
{
	hdr->req_type = cpu_to_le16(opcd);
	hdr->cmpl_ring = cpu_to_le16(-1);
	hdr->target_id = cpu_to_le16(trid);
}

static inline void bnxt_re_fill_fw_msg(struct bnxt_fw_msg *fw_msg,
				       void *msg, int msg_len, void *resp,
				       int resp_max_len, int timeout)
{
	fw_msg->msg = msg;
	fw_msg->msg_len = msg_len;
	fw_msg->resp = resp;
	fw_msg->resp_max_len = resp_max_len;
	fw_msg->timeout = timeout;
}

static inline bool is_qport_service_type_supported(struct bnxt_re_dev *rdev)
{
	return rdev->tc_rec[0].serv_type_enabled;
}

static inline bool is_bnxt_roce_queue(struct bnxt_re_dev *rdev, u8 ser_prof, u8 prof_type)
{
	if (is_qport_service_type_supported(rdev))
		return (prof_type & QUEUE_QPORTCFG_RESP_QUEUE_ID1_SERVICE_PROFILE_TYPE_ROCE);
	else
		return (ser_prof == QUEUE_QPORTCFG_RESP_QUEUE_ID0_SERVICE_PROFILE_LOSSLESS_ROCE);
}

static inline bool is_bnxt_cnp_queue(struct bnxt_re_dev *rdev, u8 ser_prof, u8 prof_type)
{
	if (is_qport_service_type_supported(rdev))
		return (prof_type & QUEUE_QPORTCFG_RESP_QUEUE_ID1_SERVICE_PROFILE_TYPE_CNP);
	else
		return (ser_prof == QUEUE_QPORTCFG_RESP_QUEUE_ID0_SERVICE_PROFILE_LOSSY_ROCE_CNP);
}

enum bnxt_re_mmap_flag {
	BNXT_RE_MMAP_SH_PAGE, /* Not used after ABIv7 */
	BNXT_RE_MMAP_WC_DB,
	BNXT_RE_MMAP_DBR_PAGE,
	BNXT_RE_MMAP_DB_RECOVERY_PAGE,
	BNXT_RE_MMAP_DBR_PACING_BAR,
	BNXT_RE_MMAP_UC_DB,
	/* Above is fixed index for ABI v7 */
	BNXT_RE_MMAP_TOGGLE_PAGE,
	BNXT_RE_MMAP_HDBR_BASE,
};

static inline char *bnxt_re_mmap_str(enum bnxt_re_mmap_flag mmap_flag)
{
	switch (mmap_flag) {
	case BNXT_RE_MMAP_SH_PAGE:
		return "BNXT_RE_MMAP_SH_PAGE";
	case BNXT_RE_MMAP_WC_DB:
		return "BNXT_RE_MMAP_WC_DB";
	case BNXT_RE_MMAP_DBR_PAGE:
		return "BNXT_RE_MMAP_DBR_PAGE";
	case BNXT_RE_MMAP_DB_RECOVERY_PAGE:
		return "BNXT_RE_MMAP_DB_RECOVERY_PAGE";
	case BNXT_RE_MMAP_DBR_PACING_BAR:
		return "BNXT_RE_MMAP_DBR_PACING_BAR";
	case BNXT_RE_MMAP_UC_DB:
		return "BNXT_RE_MMAP_UC_DB";
	case BNXT_RE_MMAP_HDBR_BASE:
		return "BNXT_RE_MMAP_HDBR_BASE";
	case BNXT_RE_MMAP_TOGGLE_PAGE:
		return "BNXT_RE_MMAP_TOGGLE_PAGE";
	default:
		return "Unknown";
	}
}

static const char * const res_err_reason[] = {
	[0] = "NO_ERROR",
	[1] = "RES_EXCEED_MAX",
	[2] = "RES_PAYLOAD_LENGTH_MISMATCH",
	[3] = "RES_EXCEEDS_WQE",
	[4] = "RES_OPCODE_ERROR",
	[5] = "RES_PSN_SEQ_ERROR_RETRY_LIMIT",
	[6] = "RES_RX_INVALID_R_KEY",
	[7] = "RES_RX_DOMAIN_ERROR",
	[8] = "RES_RX_NO_PERMISSION",
	[9] = "RES_RX_RANGE_ERROR",
	[10] = "RES_TX_INVALID_R_KEY",
	[11] = "RES_TX_DOMAIN_ERROR",
	[12] = "RES_TX_NO_PERMISSION",
	[13] = "RES_TX_RANGE_ERROR",
	[14] = "RES_IRRQ_OFLOW",
	[15] = "RES_UNSUPPORTED_OPCODE",
	[16] = "RES_UNALIGN_ATOMIC",
	[17] = "RES_REM_INVALIDATE",
	[18] = "RES_MEMORY_ERROR",
	[19] = "RES_SRQ_ERROR",
	[20] = "RES_CMP_ERROR",
	[21] = "RES_IVALID_DUP_RKEY",
	[22] = "RES_WQE_FORMAT_ERROR",
	[23] = "RES_IRRQ_FORMAT_ERROR",
	[24] = "RES_CQ_LOAD_ERROR",
	[25] = "RES_SRQ_LOAD_ERROR",
	[27] = "RES_TX_PCI_ERROR",
	[28] = "RES_RX_PCI_ERROR",
	[29] = "RES_PSN_NOT_FOUND",
	[30] = "RES_RQ_OVERFLOW",
};

static inline const char *__attribute_const__ res_err_reason_str(u32 reason_code)
{
	size_t index = reason_code;

	return (index < ARRAY_SIZE(res_err_reason) && res_err_reason[index]) ?
			res_err_reason[index] : "unrecognized reason code";
}

static const char * const req_err_reason[] = {
	[0] = "NO_ERROR",
	[1] = "REQ_OPCODE_ERROR",
	[2] = "REQ_TIMOUT_RETRY_LIMIT",
	[3] = "REQ_RNR_NAK_RETRY_LIMIT",
	[4] = "REQ_NAK_ARRIVAL_1",
	[5] = "REQ_NAK_ARRIVAL_2",
	[6] = "REQ_NAK_ARRIVAL_3",
	[7] = "REQ_NAK_ARRIVAL_4",
	[8] = "REQ_RX_MEMORY_ERROR",
	[9] = "REQ_TX_MEMORY_ERROR",
	[10] = "REQ_READ_RESP_LENGTH",
	[11] = "REQ_INVALID_READ_RESP",
	[12] = "REQ_ILLEGAL_BIND",
	[13] = "REQ_ILLEGAL_FAST_REG",
	[14] = "REQ_ILLEGAL_INVALIDATE",
	[15] = "REQ_CMP_ERROR",
	[16] = "REQ_RETRAN_LOCAL_ERROR",
	[17] = "REQ_WQE_FORMAT_ERROR",
	[18] = "REQ_ORRQ_FORMAT_ERROR",
	[19] = "REQ_INVALID_AVID_ERROR",
	[20] = "REQ_AV_DOMAIN_ERROR",
	[21] = "REQ_CQ_LOAD_ERROR",
	[22] = "REQ_SERV_TYPE_ERROR",
	[23] = "REQ_INVALID_OP_ERROR",
	[24] = "REQ_TX_PCI_ERROR",
	[25] = "REQ_RX_PCI_ERROR",
	[26] = "REQ_PROD_WQE_MSMTCH_ERROR",
	[27] = "REQ_PSN_RANGE_CHECK_ERROR",
	[28] = "REQ_RETX_SETUP_ERROR",
	[29] = "REQ_SQ_OVERFLOW",
};

static inline const char *__attribute_const__ req_err_reason_str(u32 reason_code)
{
	size_t index = reason_code;

	return (index < ARRAY_SIZE(req_err_reason) && req_err_reason[index]) ?
			req_err_reason[index] : "unrecognized reason code";
}

#ifdef HAVE_RDMA_XARRAY_MMAP_V2
struct bnxt_re_user_mmap_entry*
bnxt_re_mmap_entry_insert(struct bnxt_re_ucontext *uctx, u64 cpu_addr,
			  dma_addr_t dma_addr, u8 user_mmap_hint, u64 *offset);
#endif

#define BNXT_RE_DBR_RECOV_USERLAND_TIMEOUT (20)  /*  20 ms */
#define BNXT_RE_DBR_INT_TIME 5 /* ms */
#define BNXT_RE_PACING_EN_INT_THRESHOLD 50 /* Entries in DB FIFO */
#define BNXT_RE_PACING_ALGO_THRESHOLD(ctx) (_is_chip_p7(ctx) ? 5000 : 250) /* Entries in DB FIFO */
/* Percentage of DB FIFO depth */
#define BNXT_RE_PACING_DBQ_THRESHOLD BNXT_RE_PACING_DBQ_HIGH_WATERMARK
/*
 * Alarm threshold multiple, number of times pacing algo threshold.
 */
#define BNXT_RE_PACING_ALARM_TH_MULTIPLE 2

/*
 * Maximum Percentage of configurable DB FIFO depth.
 * The Doorbell FIFO depth is 0x2c00. But the DBR_REG_DB_THROTTLING register has only 12 bits
 * to program the high watermark. This means user can configure maximum 36% only(4095/11264).
 */
#define BNXT_RE_PACING_DBQ_HIGH_WATERMARK 36

/* Default do_pacing value when there is no congestion */
#define BNXT_RE_DBR_DO_PACING_NO_CONGESTION 0x7F /* 1 in 512 probability */

enum {
	BNXT_RE_DBQ_EVENT_SCHED = 0,
	BNXT_RE_DBR_PACING_EVENT = 1,
	BNXT_RE_DBR_NQ_PACING_NOTIFICATION = 2,
};

struct bnxt_re_dbq_work {
	struct work_struct work;
	struct bnxt_re_dev *rdev;
	struct hwrm_async_event_cmpl cmpl;
	u32 event;
};

void bnxt_re_hwrm_free_vnic(struct bnxt_re_dev *rdev, u16 vnic_id);
int bnxt_re_hwrm_alloc_vnic(struct bnxt_re_dev *rdev, u16 *vnic_id, bool is_vnic_valid);
int bnxt_re_hwrm_cfg_vnic(struct bnxt_re_dev *rdev, u32 qp_id, u16 vnic_id);
int bnxt_re_hwrm_qcfg(struct bnxt_re_dev *rdev);
int bnxt_re_hwrm_qcaps(struct bnxt_re_dev *rdev);
int bnxt_re_enable_dbr_pacing(struct bnxt_re_dev *rdev);
int bnxt_re_disable_dbr_pacing(struct bnxt_re_dev *rdev);
int bnxt_re_set_dbq_throttling_reg(struct bnxt_re_dev *rdev,
				   u16 nq_id, u32 throttle);
void bnxt_re_pacing_alert(struct bnxt_re_dev *rdev);
int bnxt_re_hwrm_pri2cos_qcfg(struct bnxt_re_dev *rdev, struct bnxt_re_tc_rec *tc_rec,
			      u16 target_id);
void bnxt_re_rename_debugfs_entry(struct bnxt_re_dev *rdev);
void bnxt_re_debugfs_add_pdev(struct bnxt_re_dev *rdev);
void bnxt_re_debugfs_rem_pdev(struct bnxt_re_dev *rdev);
int bnxt_re_read_context_allowed(struct bnxt_re_dev *rdev);
int bnxt_re_roce_cfg_hwrm(struct bnxt_re_dev *rdev, u32 feat_cfg,
			  u32 feat_enables, u32 *feat_cfg_cur);
int bnxt_re_init_countersets(struct bnxt_re_dev *rdev);
void bnxt_re_deinit_countersets(struct bnxt_re_dev *rdev);
u32 bnxt_re_dump_single_qpinfo(struct qdump_array *qdump, void *buf, u32 buf_len);
u32 bnxt_re_dump_single_qp(struct bnxt_re_dev *rdev, struct qdump_array *qdump,
			   void *buf, u32 buf_len);

static inline unsigned int bnxt_re_get_total_mr_mw_count(struct bnxt_re_dev *rdev)
{
	return (atomic_read(&rdev->stats.rsors.mr_count) +
		atomic_read(&rdev->stats.rsors.mw_count));
}

static inline void bnxt_re_set_def_pacing_threshold(struct bnxt_re_dev *rdev)
{
	rdev->qplib_res.pacing_data->pacing_th = rdev->pacing_algo_th;
	rdev->qplib_res.pacing_data->alarm_th =
		rdev->pacing_algo_th * BNXT_RE_PACING_ALARM_TH_MULTIPLE;
}

static inline void bnxt_re_set_def_do_pacing(struct bnxt_re_dev *rdev)
{
	rdev->qplib_res.pacing_data->do_pacing = rdev->dbr_def_do_pacing;
}

static inline void bnxt_re_set_pacing_dev_state(struct bnxt_re_dev *rdev)
{
	rdev->qplib_res.pacing_data->dev_err_state =
		test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags);
}

#ifdef HAVE_UAPI_DEF
extern const struct uapi_definition bnxt_re_uapi_defs[];
#endif

#ifdef HAVE_UAPI_DEF
void bnxt_re_dv_destroy_qp(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp);
void bnxt_re_dv_destroy_cq(struct bnxt_re_dev *rdev, struct bnxt_re_cq *cq);
#endif
#endif
