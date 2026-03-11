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
 * Description: IB Verbs interpreter
 */

#include <linux/crc16.h>
#include <linux/hugetlb.h>
#include <rdma/ib_pma.h>

#include "bnxt_re.h"
#include "ib_verbs.h"
#include "compat.h"
#include "bnxt.h"
#include "bnxt_hdbr.h"
#include "hdbr.h"
#include "hw_counters.h"

/* stub function to check qp state change only for IB_QP_RATE_LIMIT */
const struct {
	int                     valid;
	enum ib_qp_attr_mask    req_param[IB_QPT_MAX];
	enum ib_qp_attr_mask    opt_param[IB_QPT_MAX];
} bnxt_qp_state_table[IB_QPS_ERR + 1][IB_QPS_ERR + 1] = {
	[IB_QPS_RTR]   = {
		[IB_QPS_RESET] = { .valid = 1 },
		[IB_QPS_ERR] =   { .valid = 1 },
		[IB_QPS_RTS]   = {
			.valid = 1,
			.req_param = {
				[IB_QPT_RC]  = (IB_QP_TIMEOUT                   |
						IB_QP_RETRY_CNT                 |
						IB_QP_RNR_RETRY                 |
						IB_QP_SQ_PSN                    |
						IB_QP_MAX_QP_RD_ATOMIC),
			},
			.opt_param = {
				[IB_QPT_RC]  = (IB_QP_CUR_STATE                |
						IB_QP_ALT_PATH                 |
						IB_QP_ACCESS_FLAGS             |
						IB_QP_MIN_RNR_TIMER            |
						IB_QP_PATH_MIG_STATE | IB_QP_RATE_LIMIT),
				[IB_QPT_RAW_PACKET] = IB_QP_RATE_LIMIT,
			}
		}
	},

	[IB_QPS_RTS]   = {
		[IB_QPS_RESET] = { .valid = 1 },
		[IB_QPS_ERR] =   { .valid = 1 },
		[IB_QPS_RTS]   = {
			.valid = 1,
			.opt_param = {
				[IB_QPT_RC]  = (IB_QP_CUR_STATE                 |
						IB_QP_ACCESS_FLAGS              |
						IB_QP_ALT_PATH                  |
						IB_QP_PATH_MIG_STATE            |
						IB_QP_MIN_RNR_TIMER | IB_QP_RATE_LIMIT),
				[IB_QPT_RAW_PACKET] = IB_QP_RATE_LIMIT,
			}
		},
	},
};

static inline bool is_flow_supported(struct bnxt_re_dev *rdev, struct ib_flow_attr *attr)
{
	bool rc = false;

	switch (attr->type) {
	case IB_FLOW_ATTR_SNIFFER:
		if (rdev->rcfw.roce_mirror)
			rc = true;
		break;
	case IB_FLOW_ATTR_NORMAL:
		rc = true;
		break;
	default:
		break;
	}

	return rc;
}

static bool bnxt_re_modify_qp_is_ok(enum ib_qp_state cur_state, enum ib_qp_state next_state,
				    enum ib_qp_type type, enum ib_qp_attr_mask mask)
{
	enum ib_qp_attr_mask req_param, opt_param;

	if (mask & IB_QP_CUR_STATE  &&
	    cur_state != IB_QPS_RTR && cur_state != IB_QPS_RTS &&
	    cur_state != IB_QPS_SQD && cur_state != IB_QPS_SQE)
		return false;

	if (!bnxt_qp_state_table[cur_state][next_state].valid)
		return false;

	req_param = bnxt_qp_state_table[cur_state][next_state].req_param[type];
	opt_param = bnxt_qp_state_table[cur_state][next_state].opt_param[type];

	if ((mask & req_param) != req_param)
		return false;

	if (mask & ~(req_param | opt_param | IB_QP_STATE))
		return false;

	return true;
}

static int __from_ib_access_flags(int iflags)
{
	int qflags = 0;

	if (iflags & IB_ACCESS_LOCAL_WRITE)
		qflags |= BNXT_QPLIB_ACCESS_LOCAL_WRITE;
	if (iflags & IB_ACCESS_REMOTE_READ)
		qflags |= BNXT_QPLIB_ACCESS_REMOTE_READ;
	if (iflags & IB_ACCESS_REMOTE_WRITE)
		qflags |= BNXT_QPLIB_ACCESS_REMOTE_WRITE;
	if (iflags & IB_ACCESS_REMOTE_ATOMIC)
		qflags |= BNXT_QPLIB_ACCESS_REMOTE_ATOMIC;
	if (iflags & IB_ACCESS_MW_BIND)
		qflags |= BNXT_QPLIB_ACCESS_MW_BIND;
#ifdef HAVE_IB_ZERO_BASED
	if (iflags & IB_ZERO_BASED)
		qflags |= BNXT_QPLIB_ACCESS_ZERO_BASED;
#endif
#ifdef HAVE_IB_ACCESS_ON_DEMAND
	if (iflags & IB_ACCESS_ON_DEMAND)
		qflags |= BNXT_QPLIB_ACCESS_ON_DEMAND;
#endif
	return qflags;
}

static int __to_ib_access_flags(int qflags)
{
	int iflags = 0;

	if (qflags & BNXT_QPLIB_ACCESS_LOCAL_WRITE)
		iflags |= IB_ACCESS_LOCAL_WRITE;
	if (qflags & BNXT_QPLIB_ACCESS_REMOTE_WRITE)
		iflags |= IB_ACCESS_REMOTE_WRITE;
	if (qflags & BNXT_QPLIB_ACCESS_REMOTE_READ)
		iflags |= IB_ACCESS_REMOTE_READ;
	if (qflags & BNXT_QPLIB_ACCESS_REMOTE_ATOMIC)
		iflags |= IB_ACCESS_REMOTE_ATOMIC;
	if (qflags & BNXT_QPLIB_ACCESS_MW_BIND)
		iflags |= IB_ACCESS_MW_BIND;
#ifdef HAVE_IB_ZERO_BASED
	if (qflags & BNXT_QPLIB_ACCESS_ZERO_BASED)
		iflags |= IB_ZERO_BASED;
#endif
#ifdef HAVE_IB_ACCESS_ON_DEMAND
	if (qflags & BNXT_QPLIB_ACCESS_ON_DEMAND)
		iflags |= IB_ACCESS_ON_DEMAND;
#endif
	return iflags;
}

static u8 __qp_access_flags_from_ib(struct bnxt_qplib_chip_ctx *cctx, int iflags)
{
	u8 qflags = 0;

	if (!_is_chip_p5_plus(cctx))
		/* For Wh+ */
		return (u8)__from_ib_access_flags(iflags);

	/* For P5, P7 and later chips */
	if (iflags & IB_ACCESS_LOCAL_WRITE)
		qflags |= CMDQ_MODIFY_QP_ACCESS_LOCAL_WRITE;
	if (iflags & IB_ACCESS_REMOTE_WRITE)
		qflags |= CMDQ_MODIFY_QP_ACCESS_REMOTE_WRITE;
	if (iflags & IB_ACCESS_REMOTE_READ)
		qflags |= CMDQ_MODIFY_QP_ACCESS_REMOTE_READ;
	if (iflags & IB_ACCESS_REMOTE_ATOMIC)
		qflags |= CMDQ_MODIFY_QP_ACCESS_REMOTE_ATOMIC;

	return qflags;
}

static int __qp_access_flags_to_ib(struct bnxt_qplib_chip_ctx *cctx, u8 qflags)
{
	int iflags = 0;

	if (!_is_chip_p5_plus(cctx))
		/* For Wh+ */
		return __to_ib_access_flags(qflags);

	/* For P5, P7 and later chips */
	if (qflags & CMDQ_MODIFY_QP_ACCESS_LOCAL_WRITE)
		iflags |= IB_ACCESS_LOCAL_WRITE;
	if (qflags & CMDQ_MODIFY_QP_ACCESS_REMOTE_WRITE)
		iflags |= IB_ACCESS_REMOTE_WRITE;
	if (qflags & CMDQ_MODIFY_QP_ACCESS_REMOTE_READ)
		iflags |= IB_ACCESS_REMOTE_READ;
	if (qflags & CMDQ_MODIFY_QP_ACCESS_REMOTE_ATOMIC)
		iflags |= IB_ACCESS_REMOTE_ATOMIC;

	return iflags;
};

#ifdef HAVE_IB_ACCESS_RELAXED_ORDERING
static void bnxt_re_check_and_set_relaxed_ordering(struct bnxt_re_dev *rdev,
						   struct bnxt_qplib_mrinfo *mrinfo)
{
	if (_is_relaxed_ordering_supported(rdev->dev_attr->dev_cap_ext_flags2) &&
	    pcie_relaxed_ordering_enabled(rdev->en_dev->pdev))
		mrinfo->request_relax_order = true;
}
#endif

static int bnxt_re_copy_to_udata(struct bnxt_re_dev *rdev, void *data,
				 int len, struct ib_udata *udata)
{
	int rc;

	rc = ib_copy_to_udata(udata, data, len);
	if (rc)
		dev_err(rdev_to_dev(rdev),
			"ucontext copy failed from %ps rc %d",
			__builtin_return_address(0), rc);

	return rc;
}

#ifdef HAVE_IB_GET_NETDEV
struct net_device *bnxt_re_get_netdev(struct ib_device *ibdev,
				      PORT_NUM port_num)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct net_device *netdev = NULL;
	struct bonding *bond = NULL;
	u8 active_port_map;

	rcu_read_lock();

	if (!rdev || !rdev->netdev)
		goto end;

	netdev = rdev->netdev;

	/* In case of active-backup bond mode, return active slave */
	if (rdev->binfo) {
		bond = netdev_priv(netdev);

		if (bond && (BOND_MODE(bond) == BOND_MODE_ACTIVEBACKUP)) {
			active_port_map = bnxt_re_get_bond_link_status(rdev->binfo);

			if (active_port_map & BNXT_RE_ACTIVE_MAP_PORT1)
				netdev = rdev->binfo->slave1;
			else if (active_port_map & BNXT_RE_ACTIVE_MAP_PORT2)
				netdev = rdev->binfo->slave2;
		}
	}

	if (netdev)
		dev_hold(netdev);

end:
	rcu_read_unlock();
	return netdev;
}
#endif

#ifdef HAVE_IB_QUERY_DEVICE_UDATA
int bnxt_re_query_device(struct ib_device *ibdev,
			 struct ib_device_attr *ib_attr,
			 struct ib_udata *udata)
#else
int bnxt_re_query_device(struct ib_device *ibdev,
			 struct ib_device_attr *ib_attr)
#endif
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct bnxt_qplib_dev_attr *dev_attr = rdev->dev_attr;

	memset(ib_attr, 0, sizeof(*ib_attr));

	memcpy(&ib_attr->fw_ver, dev_attr->fw_ver, 4);
	addrconf_addr_eui48((u8 *)&ib_attr->sys_image_guid,
			    rdev->netdev->dev_addr);
	ib_attr->max_mr_size = BNXT_RE_MAX_MR_SIZE;
	ib_attr->page_size_cap = dev_attr->page_size_cap;
	ib_attr->vendor_id = rdev->en_dev->pdev->vendor;
	ib_attr->vendor_part_id = rdev->en_dev->pdev->device;
	ib_attr->hw_ver = rdev->en_dev->pdev->revision;
	ib_attr->max_qp = dev_attr->max_qp;
	ib_attr->max_qp_wr = dev_attr->max_sq_wqes;
	/*
	 * Read and set from the module param 'min_tx_depth'
	 * only once after the driver load
	 */
	if (rdev->min_tx_depth == 1 &&
	    min_tx_depth < dev_attr->max_sq_wqes)
		rdev->min_tx_depth = min_tx_depth;
	ib_attr->device_cap_flags =
				    IB_DEVICE_CURR_QP_STATE_MOD
				    | IB_DEVICE_RC_RNR_NAK_GEN
				    | IB_DEVICE_SHUTDOWN_PORT
				    | IB_DEVICE_SYS_IMAGE_GUID
				    | IB_DEVICE_RESIZE_MAX_WR
				    | IB_DEVICE_PORT_ACTIVE_EVENT
				    | IB_DEVICE_N_NOTIFY_CQ
				    | IB_DEVICE_MEM_WINDOW
				    | IB_DEVICE_MEM_WINDOW_TYPE_2B
#ifdef USE_SIGNATURE_HANDOVER
				    | IB_DEVICE_SIGNATURE_HANDOVER
#endif
#ifdef HAVE_IB_UMEM_GET_FLAGS
				    | IB_DEVICE_PEER_MEMORY
#endif
				    | IB_DEVICE_MEM_MGT_EXTENSIONS;
#ifndef HAVE_IB_KERNEL_CAP_FLAGS
	/*
	 * P8 devices do not support reserved local key. Do not advertise
	 * reserved local key for p8 devices. This allows the IB stack to cover
	 * the kernel address space with MR.
	 */
	if (!_is_chip_p8(rdev->chip_ctx))
		ib_attr->device_cap_flags |= IB_DEVICE_LOCAL_DMA_LKEY;
#endif
#ifdef HAVE_SEPARATE_SEND_RECV_SGE
	ib_attr->max_send_sge = dev_attr->max_qp_sges;
	ib_attr->max_recv_sge = dev_attr->max_qp_sges;
#else
	ib_attr->max_sge = dev_attr->max_qp_sges;
#endif
	ib_attr->max_sge_rd = dev_attr->max_qp_sges;
	ib_attr->max_cq = dev_attr->max_cq;
	ib_attr->max_cqe = dev_attr->max_cq_wqes;
	ib_attr->max_mr = dev_attr->max_mr;
	ib_attr->max_pd = dev_attr->max_pd;
	ib_attr->max_qp_rd_atom = dev_attr->max_qp_rd_atom;
	ib_attr->max_qp_init_rd_atom = dev_attr->max_qp_init_rd_atom;
	if (dev_attr->is_atomic) {
		ib_attr->atomic_cap = IB_ATOMIC_GLOB;
		ib_attr->masked_atomic_cap = IB_ATOMIC_GLOB;
	}

	ib_attr->max_ee_rd_atom = 0;
	ib_attr->max_res_rd_atom = 0;
	ib_attr->max_ee_init_rd_atom = 0;
	ib_attr->max_ee = 0;
	ib_attr->max_rdd = 0;
	ib_attr->max_mw = dev_attr->max_mw;
	ib_attr->max_raw_ipv6_qp = 0;
	ib_attr->max_raw_ethy_qp = dev_attr->max_raw_ethy_qp;
	ib_attr->max_mcast_grp = 0;
	ib_attr->max_mcast_qp_attach = 0;
	ib_attr->max_total_mcast_qp_attach = 0;
	ib_attr->max_ah = dev_attr->max_ah;

	ib_attr->max_srq = dev_attr->max_srq;
	ib_attr->max_srq_wr = dev_attr->max_srq_wqes;
	ib_attr->max_srq_sge = dev_attr->max_srq_sges;

	ib_attr->max_fast_reg_page_list_len = MAX_PBL_LVL_1_PGS;
	ib_attr->max_pkeys = 1;
	ib_attr->local_ca_ack_delay = BNXT_RE_DEFAULT_ACK_DELAY;
#ifdef HAVE_IB_ODP_CAPS
	ib_attr->sig_prot_cap = 0;
	ib_attr->sig_guard_cap = 0;
	ib_attr->odp_caps.general_caps = 0;
#endif
#ifdef HAVE_IB_KERNEL_CAP_FLAGS
	/*
	 * P8 devices do not support reserved local key. Do not advertise
	 * reserved local key for p8 devices. This allows the IB stack to cover
	 * the kernel address space with MR.
	 */
	if (!_is_chip_p8(rdev->chip_ctx))
		ib_attr->kernel_cap_flags = IBK_LOCAL_DMA_LKEY;
#endif

#ifdef HAVE_IB_QUERY_DEVICE_UDATA
	if (udata &&
	    _is_modify_qp_rate_limit_supported(rdev->dev_attr->dev_cap_ext_flags2)) {
		struct bnxt_re_query_device_ex_resp resp = {};

		/* Configure full speed shaper value */
		resp.packet_pacing_caps.qp_rate_limit_min = rdev->dev_attr->rate_limit_min;
		resp.packet_pacing_caps.qp_rate_limit_max = rdev->dev_attr->rate_limit_max;
		resp.packet_pacing_caps.supported_qpts = 1 << IB_QPT_RC;
		return bnxt_re_copy_to_udata(rdev, &resp,
					     min(udata->outlen, sizeof(resp)),
					     udata);
	}
#endif
	return 0;
}

int bnxt_re_modify_device(struct ib_device *ibdev,
			  int device_modify_mask,
			  struct ib_device_modify *device_modify)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);

	dev_dbg(rdev_to_dev(rdev), "Modify device with mask 0x%x",
		device_modify_mask);

	if (device_modify_mask & ~IB_DEVICE_MODIFY_NODE_DESC)
		return -EOPNOTSUPP;

	if (!(device_modify_mask & IB_DEVICE_MODIFY_NODE_DESC))
		return 0;

	memcpy(ibdev->node_desc, device_modify->node_desc, IB_DEVICE_NODE_DESC_MAX);
	return 0;
}

#ifdef HAVE_IB_GET_ETH_SPEED_U16
static u32 bnxt_re_to_speed(u16 speed)
{
	u32 lane_speed;

	switch (speed) {
	case IB_SPEED_EDR:
		lane_speed = SPEED_25000;
		break;
	case IB_SPEED_HDR:
		lane_speed = SPEED_50000;
		break;
	case IB_SPEED_NDR:
		lane_speed = SPEED_100000;
		break;
	default:
		lane_speed = SPEED_10000;
	}

	return lane_speed;
}

static u32 bnxt_re_get_lane_count_from_width(u8 width)
{
	switch (width) {
	case IB_WIDTH_1X:
		return 1;
	case IB_WIDTH_2X:
		return 2;
	case IB_WIDTH_4X:
		return 4;
	case IB_WIDTH_8X:
		return 8;
	case IB_WIDTH_12X:
		return 12;
	default:
		return 1;
	}
}
#endif

static void bnxt_re_speed_width(struct bnxt_re_dev *rdev)
{
#ifdef HAVE_IB_GET_ETH_SPEED_U16
	u32 lane_speed, lane_num;
	u32 port_num = 1;
	int rc;
#endif
	u32 lanes = 0;

	bnxt_re_get_link_speed(rdev);
#ifdef HAVE_ETHTOOL_LINK_KSETTINGS_LANES
	lanes = rdev->lanes;
#endif
#ifdef HAVE_IB_GET_ETH_SPEED_U16
	if (lanes != 0) {
		rc = ib_get_eth_speed(&rdev->ibdev, port_num, &rdev->active_speed,
				      &rdev->active_width);
		if (rc)
			return;
		if (rdev->espeed > SPEED_40000) {
			lane_num = bnxt_re_get_lane_count_from_width(rdev->active_width);
			lane_speed = bnxt_re_to_speed(rdev->active_speed);
			if (rdev->espeed != (lane_num * lane_speed))
				bnxt_re_get_width_and_speed(rdev->espeed, lanes,
							    &rdev->active_speed,
							    &rdev->active_width);
		}
	} else {
		bnxt_re_get_width_and_speed(rdev->espeed, lanes,
					    &rdev->active_speed, &rdev->active_width);
	}
#else
	bnxt_re_get_width_and_speed(rdev->espeed, lanes,
				    &rdev->active_speed, &rdev->active_width);
#endif
}

/* Port */
int bnxt_re_query_port(struct ib_device *ibdev, PORT_NUM port_num,
		       struct ib_port_attr *port_attr)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct bnxt_qplib_dev_attr *dev_attr = rdev->dev_attr;

	memset(port_attr, 0, sizeof(*port_attr));

	port_attr->phys_state = IB_PORT_PHYS_STATE_DISABLED;
	port_attr->state = bnxt_re_get_link_state(rdev);
	if (port_attr->state == IB_PORT_ACTIVE)
		port_attr->phys_state = IB_PORT_PHYS_STATE_LINK_UP;
	port_attr->max_mtu = IB_MTU_4096;
	port_attr->active_mtu = iboe_get_mtu(rdev->netdev->mtu);

	/* One GID is reserved for RawEth QP. Report one less */
	port_attr->gid_tbl_len = (rdev->rcfw.roce_mirror ? (dev_attr->max_sgid - 1) :
				  dev_attr->max_sgid);

	/* TODO: port_cap_flags needs to be revisited */
	port_attr->port_cap_flags = IB_PORT_CM_SUP | IB_PORT_REINIT_SUP |
				    IB_PORT_DEVICE_MGMT_SUP |
				    IB_PORT_VENDOR_CLASS_SUP;

#ifdef HAVE_IP_GIDS_IN_PORT_ATTR
	port_attr->ip_gids = true;
#else
	port_attr->port_cap_flags |= IB_PORT_IP_BASED_GIDS;
#endif
	port_attr->max_msg_sz = (u32)BNXT_RE_MAX_MR_SIZE_LOW;
	port_attr->bad_pkey_cntr = 0;
	port_attr->qkey_viol_cntr = 0;
	port_attr->pkey_tbl_len = dev_attr->max_pkey;
	port_attr->lid = 0;
	port_attr->sm_lid = 0;
	port_attr->lmc = 0;
	port_attr->max_vl_num = 4;
	port_attr->sm_sl = 0;
	port_attr->subnet_timeout = 0;
	port_attr->init_type_reply = 0;

	bnxt_re_speed_width(rdev);
	/* update dev_attr with latest fw exported */
	if (_is_modify_qp_rate_limit_supported(rdev->dev_attr->dev_cap_ext_flags2))
		bnxt_qplib_get_dev_attr(&rdev->rcfw);

	port_attr->active_speed = rdev->active_speed;
	port_attr->active_width = rdev->active_width;
	return 0;
}

#ifdef HAVE_IB_GET_PORT_IMMUTABLE
int bnxt_re_get_port_immutable(struct ib_device *ibdev, PORT_NUM port_num,
			       struct ib_port_immutable *immutable)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct ib_port_attr port_attr;

	if (bnxt_re_query_port(ibdev, port_num, &port_attr))
		return -EINVAL;

	immutable->pkey_tbl_len = port_attr.pkey_tbl_len;
	immutable->gid_tbl_len = port_attr.gid_tbl_len;
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
	if (rdev->roce_mode == BNXT_RE_FLAG_ROCEV1_CAP)
		immutable->core_cap_flags = RDMA_CORE_PORT_IBA_ROCE;
	else if (rdev->roce_mode == BNXT_RE_FLAG_ROCEV2_CAP)
		immutable->core_cap_flags = RDMA_CORE_PORT_IBA_ROCE_UDP_ENCAP;
	else
		immutable->core_cap_flags = RDMA_CORE_PORT_IBA_ROCE |
					    RDMA_CORE_PORT_IBA_ROCE_UDP_ENCAP;
#else
	immutable->core_cap_flags = RDMA_CORE_PORT_IBA_ROCE;
#endif
	immutable->max_mad_size = IB_MGMT_MAD_SIZE;
	return 0;
}
#endif

#ifdef HAVE_IB_GET_DEV_FW_STR
void bnxt_re_query_fw_str(struct ib_device *ibdev, char *str, size_t str_len)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);

	sprintf(str, "%d.%d.%d.%d", rdev->dev_attr->fw_ver[0],
		rdev->dev_attr->fw_ver[1], rdev->dev_attr->fw_ver[2],
		rdev->dev_attr->fw_ver[3]);
}
#endif

int bnxt_re_query_pkey(struct ib_device *ibdev, PORT_NUM port_num,
		       u16 index, u16 *pkey)
{
	if (index > 0)
		return -EINVAL;

	*pkey = IB_DEFAULT_PKEY_FULL;

	return 0;
}

int bnxt_re_query_gid(struct ib_device *ibdev, PORT_NUM port_num,
		      int index, union ib_gid *gid)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	int rc = 0;

	/* Ignore port_num */
	memset(gid, 0, sizeof(*gid));
#ifdef USE_ROCE_GID_CACHE
	if (ib_cache_use_roce_gid_cache(ibdev, port_num)) {
		rc = bnxt_qplib_get_sgid(&rdev->qplib_res,
					 &rdev->qplib_res.sgid_tbl, index,
					 (struct bnxt_qplib_gid *)gid);
		goto out;
	}
	rc = ib_get_cached_gid(ibdev, port_num, index, gid, NULL);
	if (rc == -EAGAIN) {
		dev_err(rdev_to_dev(rdev),
			"GID not found in the gid cache table!");
		memcpy(gid, &zgid, sizeof(*gid));
		rc = 0;
	}
out:
#else
	rc = bnxt_qplib_get_sgid(&rdev->qplib_res,
				 &rdev->qplib_res.sgid_tbl, index,
				 (struct bnxt_qplib_gid *)gid);
#endif
	return rc;
}

#ifdef HAVE_IB_ADD_DEL_GID
#ifdef HAVE_SIMPLIFIED_ADD_DEL_GID
int bnxt_re_del_gid(const struct ib_gid_attr *attr, void **context)
#else
int bnxt_re_del_gid(struct ib_device *ibdev, u8 port_num,
		    unsigned int index, void **context)
#endif

{
	int rc = 0;
	struct bnxt_re_gid_ctx *ctx, **ctx_tbl;
#ifdef HAVE_SIMPLIFIED_ADD_DEL_GID
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(attr->device, ibdev);
	unsigned int index = attr->index;
#else
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
#endif
	struct bnxt_qplib_sgid_tbl *sgid_tbl = &rdev->qplib_res.sgid_tbl;
	struct bnxt_qplib_gid *gid_to_del;
	u16 vlan_id = 0xFFFF;

	/* Delete the entry from the hardware */
	ctx = *context;
	if (!ctx) {
		if (test_bit(ERR_DEVICE_DETACHED, &rdev->rcfw.cmdq.flags))
			return 0;

		dev_err(rdev_to_dev(rdev), "GID entry has no ctx?!");
		return -EINVAL;
	}
	if (sgid_tbl->active) {
		if (ctx->idx >= sgid_tbl->max) {
			dev_dbg(rdev_to_dev(rdev), "GID index out of range?!");
			return -EINVAL;
		}
		gid_to_del = &sgid_tbl->tbl[ctx->idx].gid;
		vlan_id = sgid_tbl->tbl[ctx->idx].vlan_id;
		ctx->refcnt--;
		/* DEL_GID is called via WQ context(netdevice_event_work_handler)
		 * or via the ib_unregister_device path. In the former case QP1
		 * may not be destroyed yet, in which case just return as FW
		 * needs that entry to be present and will fail it's deletion.
		 * We could get invoked again after QP1 is destroyed OR get an
		 * ADD_GID call with a different GID value for the same index
		 * where we issue MODIFY_GID cmd to update the GID entry -- TBD
		 */
		if (ctx->idx == 0 &&
		    rdma_link_local_addr((struct in6_addr *)gid_to_del) &&
		    (rdev->gsi_ctx.gsi_sqp ||
		     rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_UD)) {
			dev_dbg(rdev_to_dev(rdev),
				"Trying to delete GID0 while QP1 is alive\n");
			if (!ctx->refcnt) {
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
				rdev->gid_map[index] = -1;
#endif /* RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP */
				ctx_tbl = sgid_tbl->ctx;
				ctx_tbl[ctx->idx] = NULL;
				kfree(ctx);
			}
			return 0;
		}
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
		rdev->gid_map[index] = -1;
#endif /* RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP */
		if (!ctx->refcnt) {
			rc = bnxt_qplib_del_sgid(sgid_tbl, gid_to_del,
						 vlan_id, true);
			if (!rc) {
				dev_dbg(rdev_to_dev(rdev), "GID remove success");
				ctx_tbl = sgid_tbl->ctx;
				ctx_tbl[ctx->idx] = NULL;
				kfree(ctx);
			} else {
				dev_err(rdev_to_dev(rdev),
					"Remove GID failed rc = 0x%x", rc);
			}
		}
	} else {
		dev_dbg(rdev_to_dev(rdev), "GID sgid_tbl does not exist!");
		return -EINVAL;
	}
	return rc;
}

#ifdef HAVE_SIMPLIFIED_ADD_DEL_GID
#ifdef HAVE_SIMPLER_ADD_GID
int bnxt_re_add_gid(const struct ib_gid_attr *attr, void **context)
#else
int bnxt_re_add_gid(const union ib_gid *gid,
		    const struct ib_gid_attr *attr, void **context)
#endif
#else
int bnxt_re_add_gid(struct ib_device *ibdev, u8 port_num,
		    unsigned int index, const union ib_gid *gid,
		    const struct ib_gid_attr *attr, void **context)
#endif
{
	struct bnxt_re_gid_ctx *ctx, **ctx_tbl;
	u8 mac[ETH_ALEN] = {0};
	u16 vlan_id = 0xFFFF;
	u32 tbl_idx = 0;
	int rc;
#ifdef HAVE_SIMPLIFIED_ADD_DEL_GID
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(attr->device, ibdev);
	unsigned int index = attr->index;
#ifdef HAVE_SIMPLER_ADD_GID
	struct bnxt_qplib_gid *gid = (struct bnxt_qplib_gid *)&attr->gid;
#endif
#else
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
#endif
	struct bnxt_qplib_sgid_tbl *sgid_tbl = &rdev->qplib_res.sgid_tbl;

	rc = rdma_read_gid_l2_fields(attr, &vlan_id, mac);
	if (rc)
		return rc;

	rc = bnxt_qplib_add_sgid(sgid_tbl, (struct bnxt_qplib_gid *)gid,
				 mac, vlan_id, true, &tbl_idx, false, 0);
	if (rc == -EALREADY) {
		dev_dbg(rdev_to_dev(rdev), "GID %pI6 is already present", gid);
		ctx_tbl = sgid_tbl->ctx;
		if (!ctx_tbl[tbl_idx]) {
			ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
			if (!ctx)
				return -ENOMEM;
			ctx->idx = tbl_idx;
			ctx->refcnt = 1;
			ctx_tbl[tbl_idx] = ctx;
		} else {
			ctx_tbl[tbl_idx]->refcnt++;
		}
		*context = ctx_tbl[tbl_idx];
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
		/* tbl_idx is the HW table index and index is the stack index */
		rdev->gid_map[index] = tbl_idx;
#endif /* RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP */
		return 0;
	} else if (rc < 0) {
		if (test_bit(ERR_DEVICE_DETACHED, &rdev->rcfw.cmdq.flags))
			return 0;
		dev_err(rdev_to_dev(rdev), "Add GID failed, expected when ethernet bond is active");
		return rc;
	} else {
		ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
		if (!ctx) {
			dev_err(rdev_to_dev(rdev), "Add GID ctx failed");
			return -ENOMEM;
		}
		ctx_tbl = sgid_tbl->ctx;
		ctx->idx = tbl_idx;
		ctx->refcnt = 1;
		ctx_tbl[tbl_idx] = ctx;
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
		/* tbl_idx is the HW table index and index is the stack index */
		rdev->gid_map[index] = tbl_idx;
#endif /* RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP */
		*context = ctx;
	}
	return rc;
}
#endif

#ifdef HAVE_IB_MODIFY_GID
int bnxt_re_modify_gid(struct ib_device *ibdev, u8 port_num,
		    unsigned int index, const union ib_gid *gid,
		    const struct ib_gid_attr *attr, void **context)
{
	int rc = 0;
	u16 vlan_id = 0xFFFF;

#ifdef USE_ROCE_GID_CACHE
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct bnxt_qplib_sgid_tbl *sgid_tbl = &rdev->qplib_res.sgid_tbl;
	struct bnxt_re_gid_ctx *ctx, **ctx_tbl;

	if (ib_cache_use_roce_gid_cache(ibdev, port_num))
		return -EINVAL;
	if (!memcmp(&zgid, gid, sizeof(*gid))) {
		/* Delete the entry from the hardware */
		ctx = *context;
		if (!ctx) {
			dev_err(rdev_to_dev(rdev), "GID entry has no ctx?!");
			return -EINVAL;
		}
		if (sgid_tbl->active) {
			if (ctx->idx >= sgid_tbl->max) {
				dev_dbg(rdev_to_dev(rdev),
					"GID index out of range?!");
				return -EINVAL;
			}
			rc = bnxt_qplib_del_sgid(sgid_tbl,
						 &sgid_tbl->tbl[ctx->idx],
						 vlan_id, true);
			if (!rc)
				dev_dbg(rdev_to_dev(rdev),
					"GID removed successfully");
			else
				dev_err(rdev_to_dev(rdev),
					"Remove GID failed rc = 0x%x", rc);
			ctx_tbl = sgid_tbl->ctx;
			ctx_tbl[ctx->idx] = NULL;
			kfree(ctx);
		} else {
			dev_dbg(rdev_to_dev(rdev),
				"GID sgid_tbl does not exist!");
			return -EINVAL;
		}
	} else {
		ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
		if (!ctx) {
			dev_err(rdev_to_dev(rdev), "Add GID ctx failed");
			return -ENOMEM;
		}
		rc = bnxt_qplib_add_sgid(sgid_tbl, (struct bnxt_qplib_gid *)gid,
					 rdev->qplib_res.netdev->dev_addr,
					 vlan_id, true, &ctx->idx, false, 0);
		if (rc == -EALREADY) {
			dev_dbg(rdev_to_dev(rdev),
				"GID is already present at index %d", ctx->idx);
			ctx_tbl = sgid_tbl->ctx;
			*context = ctx_tbl[ctx->idx];
			kfree(ctx);
			rc = 0;
		} else if (rc < 0) {
			dev_err(rdev_to_dev(rdev), "Add GID failed rc = 0x%x",
				rc);
			kfree(ctx);
		} else {
			dev_dbg(rdev_to_dev(rdev),
				"GID added to index sgid_idx %d", ctx->idx);
			ctx_tbl = sgid_tbl->ctx;
			ctx_tbl[ctx->idx] = ctx;
			*context = ctx;
		}
	}
#endif
	return rc;
}
#endif

enum rdma_link_layer bnxt_re_get_link_layer(struct ib_device *ibdev,
					    PORT_NUM port_num)
{
	return IB_LINK_LAYER_ETHERNET;
}

#define BNXT_RE_LEGACY_FENCE_BYTES	64
#define	BNXT_RE_LEGACY_FENCE_PBL_SIZE	DIV_ROUND_UP(BNXT_RE_LEGACY_FENCE_BYTES, PAGE_SIZE)

static void bnxt_re_legacy_create_fence_wqe(struct bnxt_re_pd *pd)
{
	struct bnxt_re_legacy_fence_data *fence = &pd->fence;
	struct ib_mr *ib_mr = &fence->mr->ib_mr;
	struct bnxt_qplib_swqe *wqe = &fence->bind_wqe;
	struct bnxt_re_dev *rdev = pd->rdev;

	if (_is_chip_p5_plus(rdev->chip_ctx))
		return;

	memset(wqe, 0, sizeof(*wqe));
	wqe->type = BNXT_QPLIB_SWQE_TYPE_BIND_MW;
	wqe->wr_id = BNXT_QPLIB_FENCE_WRID;
	wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
	wqe->bind.zero_based = false;
	wqe->bind.parent_l_key = ib_mr->lkey;
	wqe->bind.va = (u64)fence->va;
	wqe->bind.length = fence->size;
	wqe->bind.access_cntl = __from_ib_access_flags(IB_ACCESS_REMOTE_READ);
	wqe->bind.mw_type = SQ_BIND_MW_TYPE_TYPE1;

	/* Save the initial rkey in fence structure for now;
	 * wqe->bind.r_key will be set at (re)bind time.
	 */
	fence->bind_rkey = ib_inc_rkey(fence->mw->rkey);
}

static int bnxt_re_legacy_bind_fence_mw(struct bnxt_qplib_qp *qplib_qp)
{
	struct bnxt_re_qp *qp = container_of(qplib_qp, struct bnxt_re_qp,
					     qplib_qp);
	struct ib_pd *ib_pd = qp->ib_qp.pd;
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ib_pd);
	struct bnxt_re_legacy_fence_data *fence = &pd->fence;
	struct bnxt_qplib_swqe *fence_wqe = &fence->bind_wqe;
	struct bnxt_qplib_swqe wqe;
	int rc;

	/* TODO: Need SQ locking here when Fence WQE
	 * posting moves up into bnxt_re from bnxt_qplib.
	 */
	memcpy(&wqe, fence_wqe, sizeof(wqe));
	wqe.bind.r_key = fence->bind_rkey;
	fence->bind_rkey = ib_inc_rkey(fence->bind_rkey);

	dev_dbg(rdev_to_dev(qp->rdev),
		"Posting bind fence-WQE: rkey: %#x QP: %d PD: %p\n",
		wqe.bind.r_key, qp->qplib_qp.id, pd);
	rc = bnxt_qplib_post_send(&qp->qplib_qp, &wqe, NULL);
	if (rc) {
		dev_err(rdev_to_dev(qp->rdev), "Failed to bind fence-WQE\n");
		return rc;
	}
	bnxt_qplib_post_send_db(&qp->qplib_qp);

	return rc;
}

static int bnxt_re_legacy_create_fence_mr(struct bnxt_re_pd *pd)
{
	int mr_access_flags = IB_ACCESS_LOCAL_WRITE | IB_ACCESS_MW_BIND;
	struct bnxt_re_legacy_fence_data *fence = &pd->fence;
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_qplib_mrinfo mrinfo;
	struct bnxt_re_mr *mr = NULL;
	struct ib_mw *ib_mw = NULL;
	dma_addr_t dma_addr = 0;
#ifdef HAVE_ALLOC_MW_IN_IB_CORE
	struct bnxt_re_mw *mw = NULL;
#endif
	u32 max_mr_count;
	u64 pbl_tbl;
	int rc;

	if (_is_chip_p5_plus(rdev->chip_ctx))
		return 0;

	if (bnxt_re_get_total_mr_mw_count(rdev) >= rdev->dev_attr->max_mr)
		return -ENOMEM;

	memset(&mrinfo, 0, sizeof(mrinfo));
	/* Allocate a small chunk of memory and dma-map it */
	fence->va = kzalloc(BNXT_RE_LEGACY_FENCE_BYTES, GFP_KERNEL);
	if (!fence->va)
		return -ENOMEM;
	dma_addr = ib_dma_map_single(&rdev->ibdev, fence->va,
				     BNXT_RE_LEGACY_FENCE_BYTES,
				     DMA_BIDIRECTIONAL);
	rc = ib_dma_mapping_error(&rdev->ibdev, dma_addr);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to dma-map fence-MR-mem\n");
		rc = -EIO;
		fence->dma_addr = 0;
		goto free_va;
	}
	fence->dma_addr = dma_addr;

	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr) {
		rc = -ENOMEM;
		goto free_dma_addr;
	}
	fence->mr = mr;
	mr->rdev = rdev;
	mr->qplib_mr.pd = &pd->qplib_pd;
	mr->qplib_mr.type = CMDQ_ALLOCATE_MRW_MRW_FLAGS_PMR;
	mr->qplib_mr.access_flags = __from_ib_access_flags(mr_access_flags);
	if (!_is_alloc_mr_unified(rdev->qplib_res.dattr)) {
		rc = bnxt_qplib_alloc_mrw(&rdev->qplib_res, &mr->qplib_mr);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "Failed to alloc fence-HW-MR\n");
			goto free_mr;
		}
		/* Register MR */
		mr->ib_mr.lkey = mr->qplib_mr.lkey;
	}
	mr->qplib_mr.va         = (u64)fence->va;
	mr->qplib_mr.total_size = BNXT_RE_LEGACY_FENCE_BYTES;
	pbl_tbl = dma_addr;

	mrinfo.mrw = &mr->qplib_mr;
	mrinfo.ptes = &pbl_tbl;
	mrinfo.sg.npages = BNXT_RE_LEGACY_FENCE_PBL_SIZE;

#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	mrinfo.sg.nmap = 0;
	mrinfo.sg.sghead = 0;
#else
	mrinfo.sg.umem = NULL;
#endif
	mrinfo.sg.pgshft = PAGE_SHIFT;
	mrinfo.sg.pgsize = PAGE_SIZE;
	rc = bnxt_qplib_reg_mr(&rdev->qplib_res, &mrinfo, false);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to register fence-MR\n");
		goto free_mr;
	}
	mr->ib_mr.lkey = mr->qplib_mr.lkey;
	mr->ib_mr.rkey = mr->qplib_mr.rkey;
	atomic_inc(&rdev->stats.rsors.mr_count);
	max_mr_count =  atomic_read(&rdev->stats.rsors.mr_count);
	if (max_mr_count > (atomic_read(&rdev->stats.rsors.max_mr_count)))
		atomic_set(&rdev->stats.rsors.max_mr_count, max_mr_count);
	/* Create a fence MW only for kernel consumers */
#ifdef HAVE_ALLOC_MW_IN_IB_CORE
	mw = kzalloc(sizeof(*mw), GFP_KERNEL);
	if (!mw)
		goto free_mr;
	mw->ib_mw.device = &rdev->ibdev;
	mw->ib_mw.pd = &pd->ib_pd;
	mw->ib_mw.type = IB_MW_TYPE_1;
	ib_mw = &mw->ib_mw;
	rc = bnxt_re_alloc_mw(ib_mw, NULL);
	if (rc)
		goto free_mr;
#else
#ifdef HAVE_IB_MW_TYPE
	ib_mw = bnxt_re_alloc_mw(&pd->ib_pd, IB_MW_TYPE_1
#ifdef HAVE_ALLOW_MW_WITH_UDATA
			      , NULL
#endif
			      );
#else
	ib_mw = bnxt_re_alloc_mw(&pd->ib_pd);
#endif
#endif /* HAVE_ALLOC_MW_IN_IB_CORE */
	if (IS_ERR(ib_mw)) {
		dev_err(rdev_to_dev(rdev),
			"Failed to create fence-MW for PD: %p\n", pd);
		rc = PTR_ERR(ib_mw);
		goto free_mr;
	}
	fence->mw = ib_mw;

	bnxt_re_legacy_create_fence_wqe(pd);
	return 0;

free_mr:
	#ifdef HAVE_ALLOC_MW_IN_IB_CORE
	kfree(mw);
	#endif
	if (mr->ib_mr.lkey) {
		bnxt_qplib_free_mrw(&rdev->qplib_res, &mr->qplib_mr, 0, NULL);
		atomic_dec(&rdev->stats.rsors.mr_count);
	}
	kfree(mr);
	fence->mr = NULL;

free_dma_addr:
	ib_dma_unmap_single(&rdev->ibdev, fence->dma_addr,
			    BNXT_RE_LEGACY_FENCE_BYTES, DMA_BIDIRECTIONAL);
	fence->dma_addr = 0;

free_va:
	kfree(fence->va);
	fence->va = NULL;
	return rc;
}

static void bnxt_re_legacy_destroy_fence_mr(struct bnxt_re_pd *pd)
{
	struct bnxt_re_legacy_fence_data *fence = &pd->fence;
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_re_mr *mr = fence->mr;
#ifdef HAVE_ALLOC_MW_IN_IB_CORE
	struct bnxt_re_mw *mw = NULL;
#endif

	if (_is_chip_p5_plus(rdev->chip_ctx))
		return;

	if (fence->mw) {
#ifdef HAVE_ALLOC_MW_IN_IB_CORE
		mw = to_bnxt_re(fence->mw, struct bnxt_re_mw, ib_mw);
#endif
		bnxt_re_dealloc_mw(fence->mw);
#ifdef HAVE_ALLOC_MW_IN_IB_CORE
		kfree(mw);
#endif
		fence->mw = NULL;
	}
	if (mr) {
		if (mr->ib_mr.rkey)
			bnxt_qplib_dereg_mrw(&rdev->qplib_res, &mr->qplib_mr,
					     false);
		if (mr->ib_mr.lkey)
			bnxt_qplib_free_mrw(&rdev->qplib_res, &mr->qplib_mr, 0, NULL);
		kfree(mr);
		fence->mr = NULL;
		atomic_dec(&rdev->stats.rsors.mr_count);
	}
	if (fence->dma_addr) {
		ib_dma_unmap_single(&rdev->ibdev, fence->dma_addr,
				    BNXT_RE_LEGACY_FENCE_BYTES,
				    DMA_BIDIRECTIONAL);
		fence->dma_addr = 0;
	}
	kfree(fence->va);
	fence->va = NULL;
}


static int bnxt_re_get_user_dpi(struct bnxt_re_dev *rdev,
				struct bnxt_re_ucontext *cntx)
{
	struct bnxt_qplib_chip_ctx *cctx = rdev->chip_ctx;
	int ret = 0;
	u8 type;
	/* Allocate DPI in alloc_pd or in create_cq to avoid failing of
	 * ibv_devinfo and family of application when DPIs are depleted.
	 */
	type = BNXT_QPLIB_DPI_TYPE_UC;
	ret = bnxt_qplib_alloc_dpi(&rdev->qplib_res, &cntx->dpi, cntx, type);
	if (ret) {
		dev_err(rdev_to_dev(rdev), "Alloc doorbell page failed!");
		goto out;
	}

	if (BNXT_RE_PUSH_ENABLED(cctx->modes.db_push_mode)) {
		type = BNXT_QPLIB_DPI_TYPE_WC;
		ret = bnxt_qplib_alloc_dpi(&rdev->qplib_res, &cntx->wcdpi,
					   cntx, type);
		if (ret) {
			dev_err(rdev_to_dev(rdev), "push dp alloc failed");
			goto out;
		}
		if (BNXT_RE_PPP_ENABLED(cctx))
			rdev->ppp_stats.ppp_enabled_ctxs++;
	}
out:
	return ret;
}

/* Protection Domains */
#ifdef HAVE_DEALLOC_PD_UDATA
DEALLOC_PD_RET bnxt_re_dealloc_pd(struct ib_pd *ib_pd, struct ib_udata *udata)
#else
DEALLOC_PD_RET bnxt_re_dealloc_pd(struct ib_pd *ib_pd)
#endif
{
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ib_pd);
	struct bnxt_re_dev *rdev = pd->rdev;
	u32 resv_pdid;
	int rc;

	bnxt_re_legacy_destroy_fence_mr(pd);

	resv_pdid = rdev->qplib_res.pd_tbl.max - 1;
#ifndef HAVE_PD_ALLOC_IN_IB_CORE
	if (pd->qplib_pd.id != resv_pdid) {
#endif
		rc = bnxt_qplib_dealloc_pd(&rdev->qplib_res,
					   &rdev->qplib_res.pd_tbl,
					   &pd->qplib_pd);
		if (rc)
			dev_err_ratelimited(rdev_to_dev(rdev),
				   "%s failed rc = %d", __func__, rc);
#ifndef HAVE_PD_ALLOC_IN_IB_CORE
	}
#endif
	atomic_dec(&rdev->stats.rsors.pd_count);

#ifndef HAVE_PD_ALLOC_IN_IB_CORE
	kfree(pd);
#endif

#ifndef HAVE_DEALLOC_PD_RET_VOID
	/* return success for destroy resources */
	return 0;
#endif
}

ALLOC_PD_RET bnxt_re_alloc_pd(ALLOC_PD_IN *pd_in,
#ifdef HAVE_UCONTEXT_IN_ALLOC_PD
		struct ib_ucontext *ucontext,
#endif
		struct ib_udata *udata)
{
#ifdef HAVE_PD_ALLOC_IN_IB_CORE
	struct ib_pd *ibpd = pd_in;
	struct ib_device *ibdev = ibpd->device;
#else
	struct ib_device *ibdev = pd_in;
#endif
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
#ifdef HAVE_RDMA_UDATA_TO_DRV_CONTEXT
	struct bnxt_re_ucontext *ucntx =
		rdma_udata_to_drv_context(udata, struct bnxt_re_ucontext,
					  ib_uctx);
#else
	struct bnxt_re_ucontext *ucntx = to_bnxt_re(ucontext,
						    struct bnxt_re_ucontext,
						    ib_uctx);
#endif
	u32 max_pd_count;
	int rc;
#ifdef HAVE_PD_ALLOC_IN_IB_CORE
	struct bnxt_re_pd *pd = container_of(ibpd, struct bnxt_re_pd, ib_pd);
#else
	struct bnxt_re_pd *pd;
	pd = kzalloc(sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return ERR_PTR(-ENOMEM);
#endif /* HAVE_PD_ALLOC_IN_IB_CORE */

	pd->rdev = rdev;
	if (bnxt_qplib_alloc_pd(&rdev->qplib_res, &pd->qplib_pd)) {
		dev_err(rdev_to_dev(rdev),
			"Allocate HW Protection Domain (ucntx 0x%lx) failed!\n",
			(unsigned long)ucntx);
		rc = -ENOMEM;
		goto fail;
	}

	if (udata) {
		struct bnxt_re_pd_resp resp = {};
		resp.pdid = pd->qplib_pd.id;

		rc = bnxt_re_copy_to_udata(rdev, &resp,
					   min(udata->outlen, sizeof(resp)),
					   udata);
		if (rc)
			goto dbfail;
	}

	if (!udata)
		if (bnxt_re_legacy_create_fence_mr(pd))
			dev_warn(rdev_to_dev(rdev),
				 "Failed to create Fence-MR\n");

	atomic_inc(&rdev->stats.rsors.pd_count);
	max_pd_count = atomic_read(&rdev->stats.rsors.pd_count);
	if (max_pd_count > atomic_read(&rdev->stats.rsors.max_pd_count))
		atomic_set(&rdev->stats.rsors.max_pd_count, max_pd_count);

#ifndef HAVE_PD_ALLOC_IN_IB_CORE
	return &pd->ib_pd;
#else
	return 0;
#endif /* HAVE_PD_ALLOC_IN_IB_CORE */
dbfail:
	(void)bnxt_qplib_dealloc_pd(&rdev->qplib_res, &rdev->qplib_res.pd_tbl,
				    &pd->qplib_pd);
#ifndef HAVE_PD_ALLOC_IN_IB_CORE
fail:
	kfree(pd);
	return ERR_PTR(rc);
#else
fail:
	return rc;
#endif /* HAVE_PD_ALLOC_IN_IB_CORE */
}

static void bnxt_re_posted_destroy_ah(struct work_struct *work)
{
	struct bnxt_destroy_ah *ah = container_of(work, struct bnxt_destroy_ah, dwork.work);
	struct bnxt_re_dev *rdev = ah->rdev;
	u32 ah_ref_cnt = 0;
	int rc = 0;

	ah_ref_cnt = refcount_read(ah->ah_ref_cnt);
	if (rdev->dest_ah_wq && ah_ref_cnt && ah->retry) {
		ah->retry--;
		queue_delayed_work(rdev->dest_ah_wq, &ah->dwork, BNXT_RE_AH_DEST_DELAY);
		return;
	}

	if (!ah->retry)
		dev_dbg(rdev_to_dev(rdev), "Non-zero AH reference %d for AH id %d\n",
			ah_ref_cnt, ah->qplib_ah.id);

	rc = bnxt_qplib_destroy_ah(&rdev->qplib_res, &ah->qplib_ah, 0);
	if (rc)
		dev_err_ratelimited(rdev_to_dev(rdev),
				    "%s failed to destroy AH xid = %d rc = %d",
				    __func__, ah->qplib_ah.id, rc);
	atomic_dec(&rdev->stats.rsors.ah_hw_count);
	if (!ah_ref_cnt)
		kfree(ah->ah_ref_cnt);
	kfree(ah);
	refcount_dec(&rdev->pos_destah_cnt);
}

/* Address Handles */
#ifdef HAVE_SLEEPABLE_AH
DESTROY_AH_RET bnxt_re_destroy_ah(struct ib_ah *ib_ah, u32 flags)
#else
int bnxt_re_destroy_ah(struct ib_ah *ib_ah)
#endif
{
	struct bnxt_re_ah *ah = to_bnxt_re(ib_ah, struct bnxt_re_ah, ib_ah);
	struct bnxt_re_dev *rdev = ah->rdev;
	struct bnxt_destroy_ah *pos_ah;
	bool zero_ref = false;
	bool block = true;
	int rc = 0;

#ifdef HAVE_SLEEPABLE_AH
	block = !(flags & RDMA_DESTROY_AH_SLEEPABLE);
#endif

	zero_ref = refcount_dec_and_test(ah->ref_cnt);
	/* coverity[dead_error_line] */
	if (rdev->dest_ah_wq && (block || !zero_ref)) {
		pos_ah = kzalloc(sizeof(*pos_ah), GFP_ATOMIC);
		if (!pos_ah) {
			rc = bnxt_qplib_destroy_ah(&rdev->qplib_res, &ah->qplib_ah, block);
			if (rc)
				dev_err_ratelimited(rdev_to_dev(rdev),
						    "%s id = %d dest_ah failed rc = %d",
						    __func__, ah->qplib_ah.id, rc);
			goto err;
		}
		INIT_DELAYED_WORK(&pos_ah->dwork, bnxt_re_posted_destroy_ah);
		pos_ah->rdev = rdev;
		pos_ah->ah_ref_cnt = ah->ref_cnt;
		pos_ah->retry = BNXT_RE_AH_DEST_RETRY;
		memcpy(&pos_ah->qplib_ah, &ah->qplib_ah, sizeof(pos_ah->qplib_ah));
		refcount_inc(&rdev->pos_destah_cnt);
		queue_delayed_work(rdev->dest_ah_wq, &pos_ah->dwork, 0);
	} else {
		rc = bnxt_qplib_destroy_ah(&rdev->qplib_res, &ah->qplib_ah, block);
		if (rc)
			dev_err_ratelimited(rdev_to_dev(rdev),
					    "%s id = %d blocking %d failed rc = %d",
					    __func__, ah->qplib_ah.id, block, rc);
		kfree(ah->ref_cnt);
		atomic_dec(&rdev->stats.rsors.ah_hw_count);
	}

err:
	atomic_dec(&rdev->stats.rsors.ah_count);

#ifndef HAVE_AH_ALLOC_IN_IB_CORE
	kfree(ah);
#endif

#ifndef HAVE_DESTROY_AH_RET_VOID
	/* return success for destroy resources */
	return 0;
#endif
}

#ifndef HAVE_IB_AH_DMAC
static void bnxt_re_resolve_dmac(struct bnxt_re_dev *rdev, u8 *dmac,
				 struct bnxt_qplib_gid *dgid)
{
	struct in6_addr in6;

	memcpy(&in6, dgid->data, sizeof(in6));
	if (rdma_is_multicast_addr(&in6))
		rdma_get_mcast_mac(&in6, dmac);
	else if (rdma_link_local_addr(&in6))
		rdma_get_ll_mac(&in6, dmac);
	else
		dev_err(rdev_to_dev(rdev),
			"Unable to resolve Dest MAC from the provided dgid");
}
#endif

#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
static u8 _to_bnxt_re_nw_type(enum rdma_network_type ntype)
{
	u8 nw_type;
	switch (ntype) {
		case RDMA_NETWORK_IPV4:
			nw_type = CMDQ_CREATE_AH_TYPE_V2IPV4;
			break;
		case RDMA_NETWORK_IPV6:
			nw_type = CMDQ_CREATE_AH_TYPE_V2IPV6;
			break;
		default:
			nw_type = CMDQ_CREATE_AH_TYPE_V1;
			break;
	}
	return nw_type;
}
#endif

static int bnxt_re_get_ah_info(struct bnxt_re_dev *rdev,
			       RDMA_AH_ATTR *ah_attr,
			       struct bnxt_re_ah_info *ah_info)
{
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
#ifdef HAVE_GID_ATTR_IN_IB_AH
	const struct ib_global_route *grh = rdma_ah_read_grh(ah_attr);
#endif
	IB_GID_ATTR *gattr;
	enum rdma_network_type ib_ntype;
#endif
	u8 ntype;
	union ib_gid *gid;
	int rc = 0;

	gid = &ah_info->sgid;
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
#ifndef HAVE_GID_ATTR_IN_IB_AH
	gattr = &ah_info->sgid_attr;

	rc = bnxt_re_get_cached_gid(&rdev->ibdev, 1, ah_attr->grh.sgid_index,
				    gid, &gattr, &ah_attr->grh, NULL);
	if (rc)
		return rc;

	if (gattr->ndev) {
		if (is_vlan_dev(gattr->ndev))
			ah_info->vlan_tag = vlan_dev_vlan_id(gattr->ndev);
		dev_put(gattr->ndev);
	}

	/* Get network header type for this GID */
#else
	gattr = grh->sgid_attr;
	memcpy(&ah_info->sgid_attr, gattr, sizeof(*gattr));
	if ((gattr->ndev) && is_vlan_dev(gattr->ndev))
		ah_info->vlan_tag = vlan_dev_vlan_id(gattr->ndev);
#endif /* HAVE_GID_ATTR_IN_IB_AH */

	ib_ntype = bnxt_re_gid_to_network_type(gattr, gid);
	ntype = _to_bnxt_re_nw_type(ib_ntype);
#else
	rc = ib_query_gid(&rdev->ibdev, 1, ah_attr->grh.sgid_index, gid, 0);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query gid at index %d",
			ah_attr->grh.sgid_index);
		return rc;
	}

	ntype = CMDQ_CREATE_AH_TYPE_V1;
#endif /* RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP */
	ah_info->nw_type = ntype;

	return rc;
}

static u8 _get_sgid_index(struct bnxt_re_dev *rdev, u8 gindx)
{
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
	gindx = rdev->gid_map[gindx];
#endif
	return gindx;
}

static int bnxt_re_init_dmac(struct bnxt_re_dev *rdev, RDMA_AH_ATTR *ah_attr,
			     struct bnxt_re_ah_info *ah_info, bool is_user,
			     struct bnxt_re_ah *ah)
{
#ifndef HAVE_IB_AH_DMAC
	u8 dstmac[ETH_ALEN];
#endif
	int rc = 0;
	u8 *dmac;

#ifdef HAVE_IB_AH_DMAC
	if (is_user && !rdma_is_multicast_addr((struct in6_addr *)
						ah_attr->grh.dgid.raw) &&
	    !rdma_link_local_addr((struct in6_addr *)ah_attr->grh.dgid.raw)) {

#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
#if defined (HAVE_IB_RESOLVE_ETH_DMAC) || !defined (HAVE_CREATE_USER_AH)

		u32 retry_count = BNXT_RE_RESOLVE_RETRY_COUNT_US;
		struct bnxt_re_resolve_dmac_work *resolve_dmac_work;


		resolve_dmac_work = kzalloc(sizeof(*resolve_dmac_work), GFP_ATOMIC);
		if (!resolve_dmac_work)
			return -ENOMEM;

		resolve_dmac_work->rdev = rdev;
		resolve_dmac_work->ah_attr = ah_attr;
		resolve_dmac_work->ah_info = ah_info;

		atomic_set(&resolve_dmac_work->status_wait, 1);
		INIT_WORK(&resolve_dmac_work->work, bnxt_re_resolve_dmac_task);
		queue_work(rdev->resolve_wq, &resolve_dmac_work->work);

		do {
			rc = atomic_read(&resolve_dmac_work->status_wait) & 0xFF;
			if (!rc)
				break;
			udelay(1);
		} while (--retry_count);
		if (atomic_read(&resolve_dmac_work->status_wait)) {
			INIT_LIST_HEAD(&resolve_dmac_work->list);
			list_add_tail(&resolve_dmac_work->list,
					&rdev->mac_wq_list);
			return -EFAULT;
		}
		kfree(resolve_dmac_work);
#endif
#endif
	}
	dmac = ROCE_DMAC(ah_attr);
#else /* HAVE_IB_AH_DMAC */
	bnxt_re_resolve_dmac(rdev, dstmac, &ah->qplib_ah.dgid);
	dmac = dstmac;
#endif
	if (dmac)
		memcpy(ah->qplib_ah.dmac, dmac, ETH_ALEN);
	return rc;
}

#ifdef HAVE_IB_CREATE_AH_UDATA
CREATE_AH_RET bnxt_re_create_ah(CREATE_AH_IN *ah_in,
				RDMA_AH_ATTR_IN *attr,
#ifndef HAVE_RDMA_AH_INIT_ATTR
#ifdef HAVE_SLEEPABLE_AH
				u32 flags,
#endif
#endif
				struct ib_udata *udata)
#else
struct ib_ah *bnxt_re_create_ah(CREATE_AH_IN *ah_in,
				RDMA_AH_ATTR_IN *attr)
#endif
{

#ifndef HAVE_AH_ALLOC_IN_IB_CORE
	struct ib_pd *ib_pd = ah_in;
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ib_pd);
	struct bnxt_re_ah *ah;
#else
	struct ib_ah *ib_ah = ah_in;
	struct ib_pd *ib_pd = ib_ah->pd;
	struct bnxt_re_ah *ah = container_of(ib_ah, struct bnxt_re_ah, ib_ah);
	struct bnxt_re_pd *pd = container_of(ib_pd, struct bnxt_re_pd, ib_pd);
#endif
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_re_ah_info ah_info;
	u32 max_ah_count;
	bool is_user;
	int rc;
	bool block = true;
#ifdef HAVE_RDMA_AH_INIT_ATTR
	struct rdma_ah_attr *ah_attr = attr->ah_attr;

	block = !(attr->flags & RDMA_CREATE_AH_SLEEPABLE);
#else
	RDMA_AH_ATTR *ah_attr = attr;
#ifdef HAVE_SLEEPABLE_AH
	block = !(flags & RDMA_CREATE_AH_SLEEPABLE);
#endif
#endif

	if (!(ah_attr->ah_flags & IB_AH_GRH))
		dev_err(rdev_to_dev(rdev), "ah_attr->ah_flags GRH is not set");

	dev_attr = rdev->dev_attr;
	if (atomic_read(&rdev->stats.rsors.ah_count) >= dev_attr->max_ah) {
		dev_err_ratelimited(rdev_to_dev(rdev),
				    "Max AH limit %d reached!", dev_attr->max_ah);
#ifndef HAVE_AH_ALLOC_IN_IB_CORE
		return ERR_PTR(-EINVAL);
#else
		return -EINVAL;
#endif
	}

#ifndef HAVE_AH_ALLOC_IN_IB_CORE
	ah = kzalloc(sizeof(*ah), GFP_ATOMIC);
	if (!ah) {
		rc = -ENOMEM;
		goto fail;
	}
#endif
	ah->rdev = rdev;
	ah->qplib_ah.pd = &pd->qplib_pd;
	is_user = ib_pd->uobject ? true : false;

	/* Supply the configuration for the HW */
	memcpy(ah->qplib_ah.dgid.data, ah_attr->grh.dgid.raw,
			sizeof(union ib_gid));
	ah->qplib_ah.sgid_index = _get_sgid_index(rdev, ah_attr->grh.sgid_index);
	if (ah->qplib_ah.sgid_index == 0xFF) {
		rc = -EINVAL;
		goto fail;
	}
	ah->qplib_ah.host_sgid_index = ah_attr->grh.sgid_index;
	ah->qplib_ah.traffic_class = ah_attr->grh.traffic_class;
	ah->qplib_ah.flow_label = ah_attr->grh.flow_label;
	ah->qplib_ah.hop_limit = ah_attr->grh.hop_limit;
	ah->qplib_ah.sl = ah_attr->sl;
	rc = bnxt_re_get_ah_info(rdev, ah_attr, &ah_info);
	if (rc)
		goto fail;
	ah->qplib_ah.nw_type = ah_info.nw_type;

	rc = bnxt_re_init_dmac(rdev, ah_attr, &ah_info, is_user, ah);
	if (rc)
		goto fail;

	ah->ref_cnt = kzalloc(sizeof(refcount_t), GFP_ATOMIC);
	if (!ah->ref_cnt) {
		rc = -ENOMEM;
		goto fail;
	}

	rc = bnxt_qplib_create_ah(&rdev->qplib_res, &ah->qplib_ah, block);
	if (rc) {
		dev_err_ratelimited(rdev_to_dev(rdev),
				    "Create HW Address Handle failed!");
		goto fail;
	}

	if (udata) {
		struct bnxt_re_ah_resp resp = {};

		resp.ah_id = ah->qplib_ah.id;
		rc = bnxt_re_copy_to_udata(rdev, &resp,
					   min(udata->outlen, sizeof(resp)),
					   udata);
		if (rc)
			goto fail;
	}
	atomic_inc(&rdev->stats.rsors.ah_count);
	atomic_inc(&rdev->stats.rsors.ah_hw_count);
	max_ah_count = atomic_read(&rdev->stats.rsors.ah_count);
	if (max_ah_count > atomic_read(&rdev->stats.rsors.max_ah_count))
		atomic_set(&rdev->stats.rsors.max_ah_count, max_ah_count);
	max_ah_count = atomic_read(&rdev->stats.rsors.ah_hw_count);
	if (max_ah_count > atomic_read(&rdev->stats.rsors.max_ah_hw_count))
		atomic_set(&rdev->stats.rsors.max_ah_hw_count, max_ah_count);

	refcount_set(ah->ref_cnt, 1);

#ifndef HAVE_AH_ALLOC_IN_IB_CORE
	return &ah->ib_ah;
fail:
	if (ah)
		kfree(ah->ref_cnt);
	kfree(ah);
	return ERR_PTR(rc);
#else
	return 0;
fail:
	return rc;
#endif
}

int bnxt_re_query_ah(struct ib_ah *ib_ah, RDMA_AH_ATTR *ah_attr)
{
	struct bnxt_re_ah *ah = to_bnxt_re(ib_ah, struct bnxt_re_ah, ib_ah);

#ifdef HAVE_ROCE_AH_ATTR
	ah_attr->type = ib_ah->type;
#endif
	memcpy(ah_attr->grh.dgid.raw, ah->qplib_ah.dgid.data,
	       sizeof(union ib_gid));
	ah_attr->grh.sgid_index = ah->qplib_ah.host_sgid_index;
	ah_attr->grh.traffic_class = ah->qplib_ah.traffic_class;
	ah_attr->sl = ah->qplib_ah.sl;
#ifdef HAVE_IB_AH_DMAC
	memcpy(ROCE_DMAC(ah_attr), ah->qplib_ah.dmac, ETH_ALEN);
#endif
	ah_attr->ah_flags = IB_AH_GRH;
	ah_attr->port_num = 1;
	ah_attr->static_rate = 0;
	return 0;
}

static inline void bnxt_re_save_resource_context(struct bnxt_re_dev *rdev,
						 void *ctx_sb_data,
						 u8 res_type, bool do_snapdump)
{
	struct bnxt_re_res_list *res_list;
	void *drv_ctx_data;
	u32 *ctx_index;
	u16 ctx_size;

	if (!ctx_sb_data)
		return;

	switch (res_type) {
	case BNXT_RE_RES_TYPE_QP:
		drv_ctx_data = rdev->rcfw.qp_ctxm_data;
		ctx_index = &rdev->rcfw.qp_ctxm_data_index;
		ctx_size = rdev->rcfw.qp_ctxm_size;
		break;
	case BNXT_RE_RES_TYPE_CQ:
		drv_ctx_data = rdev->rcfw.cq_ctxm_data;
		ctx_index = &rdev->rcfw.cq_ctxm_data_index;
		ctx_size = rdev->rcfw.cq_ctxm_size;
		break;
	case BNXT_RE_RES_TYPE_MR:
		drv_ctx_data = rdev->rcfw.mrw_ctxm_data;
		ctx_index = &rdev->rcfw.mrw_ctxm_data_index;
		ctx_size = rdev->rcfw.mrw_ctxm_size;
		break;
	case BNXT_RE_RES_TYPE_SRQ:
		drv_ctx_data = rdev->rcfw.srq_ctxm_data;
		ctx_index = &rdev->rcfw.srq_ctxm_data_index;
		ctx_size = rdev->rcfw.srq_ctxm_size;
		break;
	default:
		return;
	}

	if ((rdev->snapdump_dbg_lvl == BNXT_RE_SNAPDUMP_ALL) ||
	    ((rdev->snapdump_dbg_lvl == BNXT_RE_SNAPDUMP_ERR) && do_snapdump)) {
		res_list = &rdev->res_list[res_type];
		spin_lock(&res_list->lock);
		memcpy(drv_ctx_data + (*ctx_index * ctx_size),
		       ctx_sb_data, ctx_size);
		*ctx_index = *ctx_index + 1;
		*ctx_index = *ctx_index % CTXM_DATA_INDEX_MAX;
		spin_unlock(&res_list->lock);
		dev_dbg(rdev_to_dev(rdev),
			"%s : res_type %d ctx_index %d 0x%lx\n", __func__,
			res_type, *ctx_index,
			(unsigned long)drv_ctx_data + (*ctx_index * ctx_size));

		atomic_inc(&rdev->dbg_stats->snapdump.context_saved_count);
	}
}


/* Shared Receive Queues */
DESTROY_SRQ_RET bnxt_re_destroy_srq(struct ib_srq *ib_srq
#ifdef HAVE_DESTROY_SRQ_UDATA
		    , struct ib_udata *udata
#endif
		)
{
	struct bnxt_re_srq *srq = to_bnxt_re(ib_srq, struct bnxt_re_srq, ib_srq);
	struct bnxt_re_dev *rdev = srq->rdev;
	struct bnxt_qplib_srq *qplib_srq;
	void  *ctx_sb_data = NULL;
	bool do_snapdump;
	u16 ctx_size;
	int rc = 0;

	qplib_srq = &srq->qplib_srq;
	BNXT_RE_RES_LIST_DEL(rdev, srq, BNXT_RE_RES_TYPE_SRQ);

	if (srq->srq_toggle_page) {
		BNXT_RE_SRQ_PAGE_LIST_DEL(srq->uctx, srq);
#ifndef HAVE_UAPI_DEF
		bnxt_re_mmap_entry_remove_compat(srq->srq_toggle_mmap);
		srq->srq_toggle_mmap = NULL;
#endif
		free_page((unsigned long)srq->srq_toggle_page);
		srq->srq_toggle_page = NULL;
		hash_del(&srq->hash_entry);
	}

	if (rdev->hdbr_enabled)
		bnxt_re_hdbr_db_unreg_srq(rdev, srq);

	ctx_size = qplib_srq->ctx_size_sb;
	if (ctx_size)
		ctx_sb_data = vzalloc(ctx_size);

	rc = bnxt_qplib_destroy_srq(&rdev->qplib_res, qplib_srq,
				    ctx_size, ctx_sb_data);

	do_snapdump = test_bit(SRQ_FLAGS_CAPTURE_SNAPDUMP, &qplib_srq->flags);
	if (!rc)
		bnxt_re_save_resource_context(rdev, ctx_sb_data,
					      BNXT_RE_RES_TYPE_SRQ, do_snapdump);
	else
		dev_err_ratelimited(rdev_to_dev(rdev), "%s id = %d failed rc = %d",
				    __func__, qplib_srq->id, rc);

	vfree(ctx_sb_data);

	bnxt_re_umem_free(&srq->umem);
	bnxt_re_umem_free(&srq->qplib_srq.srqprod);
	bnxt_re_umem_free(&srq->qplib_srq.srqcons);

	/* TODO: Must free the actual SRQ DMA memory */

	atomic_dec(&rdev->stats.rsors.srq_count);

#ifndef HAVE_SRQ_CREATE_IN_IB_CORE
	kfree(srq);
#endif

#ifndef HAVE_DESTROY_SRQ_RET_VOID
	/* return success for destroy resources */
	return 0;
#endif
}

static u16 _max_rwqe_sz(struct bnxt_re_dev *rdev, int nsge)
{
	u16 hdr_size;

	hdr_size = _is_hsi_v3(rdev->chip_ctx) ?
		   sizeof(struct rq_wqe_hdr_v3) : sizeof(struct rq_wqe_hdr);
	return hdr_size + (nsge * sizeof(struct sq_sge));
}

static u16 bnxt_re_get_rwqe_size(struct bnxt_re_dev *rdev,
				 struct bnxt_qplib_qp *qplqp,
				 int rsge, int max)
{
	/* For Static wqe mode, use max wqe size only if
	 * FW doesn't support smaller recv wqes
	 */
	if (qplqp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC &&
	    !qplqp->small_recv_wqe_sup)
		rsge = max;
	return _max_rwqe_sz(rdev, rsge);
}

static int bnxt_re_init_user_srq(struct bnxt_re_dev *rdev,
				 struct bnxt_re_pd *pd,
				 struct bnxt_re_srq *srq,
				 struct ib_udata *udata)
{
	struct bnxt_qplib_sg_info *sginfo;
	struct bnxt_re_srq_req ureq = {};
	struct bnxt_qplib_srq *qplib_srq;
	struct bnxt_re_ucontext *cntx;
	struct ib_ucontext *context;
	struct ib_umem *umem;
	int rc, bytes = 0;

	context = pd->ib_pd.uobject->context;
	cntx = to_bnxt_re(context, struct bnxt_re_ucontext, ib_uctx);
	qplib_srq = &srq->qplib_srq;
	sginfo = &qplib_srq->sginfo;

	if (udata->inlen < sizeof(ureq))
		dev_warn_once(rdev_to_dev(rdev),
			 "Update the library ulen %d klen %d",
			 (unsigned int)udata->inlen,
			 (unsigned int)sizeof(ureq));

	rc = ib_copy_from_udata(&ureq, udata,
				min(udata->inlen, sizeof(ureq)));
	if (rc)
		return rc;

	bytes = (qplib_srq->max_wqe * qplib_srq->wqe_size);
	bytes = PAGE_ALIGN(bytes);
	umem = ib_umem_get_compat(rdev, context, udata, ureq.srqva, bytes,
				  IB_ACCESS_LOCAL_WRITE, 1);
	if (IS_ERR(umem)) {
		dev_err(rdev_to_dev(rdev), "%s: ib_umem_get failed with %ld\n",
			__func__, PTR_ERR(umem));
		return PTR_ERR(umem);
	}

	srq->umem = umem;
	bnxt_re_set_sginfo(rdev, umem, ureq.srqva, sginfo, BNXT_RE_RES_TYPE_SRQ);
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	sginfo->sghead = get_ib_umem_sgl(umem, &sginfo->nmap);
#else
	sginfo->umem = umem;
#endif
	qplib_srq->srq_handle = ureq.srq_handle;
	qplib_srq->dpi = &cntx->dpi;
	qplib_srq->is_user = true;

	if (ureq.srqprodva)
		qplib_srq->srqprod = ib_umem_get_compat(rdev, context, udata,
							ureq.srqprodva, sizeof(u32),
							IB_ACCESS_LOCAL_WRITE, 1);
	if (ureq.srqconsva)
		qplib_srq->srqcons = ib_umem_get_compat(rdev, context, udata,
							ureq.srqconsva, sizeof(u32),
							IB_ACCESS_LOCAL_WRITE, 1);
	return 0;
}

CREATE_SRQ_RET bnxt_re_create_srq(CREATE_SRQ_IN *srq_in,
				  struct ib_srq_init_attr *srq_init_attr,
				  struct ib_udata *udata)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_re_ucontext *cntx = NULL;
	struct ib_ucontext *context;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_pd *pd;
	int rc, entries;
	int num_sge;
#ifdef HAVE_SRQ_CREATE_IN_IB_CORE
	struct ib_srq *ib_srq = srq_in;
	struct ib_pd *ib_pd = ib_srq->pd;
	struct bnxt_re_srq *srq =
		container_of(ib_srq, struct bnxt_re_srq, ib_srq);
#else
	struct ib_pd *ib_pd = srq_in;
	struct bnxt_re_srq *srq;
#endif
	u32 max_srq_count;

	pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ib_pd);
	rdev = pd->rdev;
	dev_attr = rdev->dev_attr;

	if (srq_init_attr->srq_type != IB_SRQT_BASIC) {
		dev_err(rdev_to_dev(rdev), "SRQ type not supported");
		rc = -ENOTSUPP;
		goto exit;
	}

	if (udata) {
		context = pd->ib_pd.uobject->context;
		cntx = to_bnxt_re(context, struct bnxt_re_ucontext, ib_uctx);
	}

	if (atomic_read(&rdev->stats.rsors.srq_count) >= dev_attr->max_srq) {
		dev_err(rdev_to_dev(rdev), "Create SRQ failed - max exceeded(SRQs)");
		rc = -EINVAL;
		goto exit;
	}

	if (srq_init_attr->attr.max_wr >= dev_attr->max_srq_wqes) {
		dev_err(rdev_to_dev(rdev), "Create SRQ failed - max exceeded(SRQ_WQs)");
		rc = -EINVAL;
		goto exit;
	}
#ifndef HAVE_SRQ_CREATE_IN_IB_CORE
	srq = kzalloc(sizeof(*srq), GFP_KERNEL);
	if (!srq) {
		rc = -ENOMEM;
		goto exit;
	}
#endif
	srq->rdev = rdev;
	srq->qplib_srq.pd = &pd->qplib_pd;
	srq->qplib_srq.dpi = &rdev->dpi_privileged;

	/* Allocate 1 more than what's provided so posting max doesn't
	   mean empty */
	entries = srq_init_attr->attr.max_wr + 1;
	entries = bnxt_re_init_depth(entries, cntx);
	if (entries > dev_attr->max_srq_wqes + 1)
		entries = dev_attr->max_srq_wqes + 1;
	if (cntx)
		srq->qplib_srq.small_recv_wqe_sup = cntx->small_recv_wqe_sup;
	num_sge = srq->qplib_srq.small_recv_wqe_sup ?
			srq_init_attr->attr.max_sge : 6;
	srq->qplib_srq.wqe_size = _max_rwqe_sz(rdev, num_sge);
	srq->qplib_srq.max_wqe = entries;
	srq->qplib_srq.max_sge = srq_init_attr->attr.max_sge;
	srq->qplib_srq.srq_limit_drv = srq_init_attr->attr.srq_limit;
	srq->qplib_srq.eventq_hw_ring_id = rdev->nqr->nq[0].ring_id;
	srq->qplib_srq.sginfo.pgsize = PAGE_SIZE;
	srq->qplib_srq.sginfo.pgshft = PAGE_SHIFT;

	INIT_LIST_HEAD(&srq->srq_list);

	if (udata) {
		rc = bnxt_re_init_user_srq(rdev, pd, srq, udata);
		if (rc)
			goto fail;
	}

	rc = bnxt_qplib_create_srq(&rdev->qplib_res, &srq->qplib_srq);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Create HW SRQ failed!");
		goto fail;
	}

	if (udata) {
		struct bnxt_re_srq_resp resp = {};

		srq->uctx = cntx;
		if (rdev->hdbr_enabled) {
			rc = bnxt_re_hdbr_db_reg_srq(rdev, srq, cntx, &resp);
			if (rc)
				goto db_reg_fail;
		}

		resp.srqid = srq->qplib_srq.id;

		if (rdev->chip_ctx->modes.toggle_bits & BNXT_QPLIB_SRQ_TOGGLE_BIT) {
			srq->srq_toggle_page = (void *)get_zeroed_page(GFP_KERNEL);
			if (!srq->srq_toggle_page)
				goto srq_page_fail;
#ifndef HAVE_UAPI_DEF
			rc = bnxt_re_mmap_entry_insert_compat(cntx, (u64)srq->srq_toggle_page, 0,
							      BNXT_RE_MMAP_TOGGLE_PAGE,
							      &resp.srq_toggle_mmap_key,
							      &srq->srq_toggle_mmap);
			if (!rc) {
				rc = -ENOMEM;
				goto srq_page_fail;
			}
#endif
			hash_add(rdev->srq_hash, &srq->hash_entry, srq->qplib_srq.id);
			resp.comp_mask |= BNXT_RE_SRQ_TOGGLE_PAGE_SUPPORT;
		}

		rc = bnxt_re_copy_to_udata(rdev, &resp,
					   min(udata->outlen, sizeof(resp)),
					   udata);
		if (rc)
			goto srq_page_fail;

		if (srq->srq_toggle_page)
			BNXT_RE_SRQ_PAGE_LIST_ADD(cntx, srq);
	} else {
		if (rdev->hdbr_enabled) {
			rc = bnxt_re_hdbr_db_reg_srq(rdev, srq, NULL, NULL);
			if (rc)
				goto db_reg_fail;
		}
	}

	atomic_inc(&rdev->stats.rsors.srq_count);
	max_srq_count = atomic_read(&rdev->stats.rsors.srq_count);
	if (max_srq_count > atomic_read(&rdev->stats.rsors.max_srq_count))
		atomic_set(&rdev->stats.rsors.max_srq_count, max_srq_count);
	spin_lock_init(&srq->lock);

	BNXT_RE_RES_LIST_ADD(rdev, srq, BNXT_RE_RES_TYPE_SRQ);
#ifndef HAVE_SRQ_CREATE_IN_IB_CORE
	return &srq->ib_srq;
#else
	return 0;
#endif
srq_page_fail:
	if (srq->srq_toggle_page) {
		/* This error handling. No need to check ABI v7 vs v8 */
		bnxt_re_mmap_entry_remove_compat(srq->srq_toggle_mmap);
		free_page((unsigned long)srq->srq_toggle_page);
		hash_del(&srq->hash_entry);
	}
	if (rdev->hdbr_enabled) {
		bnxt_re_hdbr_db_unreg_srq(rdev, srq);
	}
db_reg_fail:
	bnxt_qplib_destroy_srq(&rdev->qplib_res, &srq->qplib_srq, 0, NULL);
fail:
	if (udata) {
		bnxt_re_umem_free(&srq->umem);
		bnxt_re_umem_free(&srq->qplib_srq.srqprod);
		bnxt_re_umem_free(&srq->qplib_srq.srqcons);
	}
#ifndef HAVE_SRQ_CREATE_IN_IB_CORE
	kfree(srq);
exit:
	return ERR_PTR(rc);
#else
exit:
	return rc;
#endif
}

int bnxt_re_modify_srq(struct ib_srq *ib_srq, struct ib_srq_attr *srq_attr,
		       enum ib_srq_attr_mask srq_attr_mask,
		       struct ib_udata *udata)
{
	struct bnxt_re_srq *srq = to_bnxt_re(ib_srq, struct bnxt_re_srq,
					     ib_srq);
	struct bnxt_re_dev *rdev = srq->rdev;
	int rc;

	switch (srq_attr_mask) {
	case IB_SRQ_MAX_WR:
		/* SRQ resize is not supported */
		return -EINVAL;
	case IB_SRQ_LIMIT:
		/* Change the SRQ threshold */
		if (srq_attr->srq_limit > srq->qplib_srq.max_wqe)
			return -EINVAL;

		srq->qplib_srq.srq_limit_drv = srq_attr->srq_limit;
		bnxt_qplib_modify_srq(&rdev->qplib_res, &srq->qplib_srq);

		if (udata) {
			/* Build and send response back to udata */
			rc = bnxt_re_copy_to_udata(rdev, srq, 0, udata);
			if (rc)
				return rc;
		}
		break;
	default:
		dev_err(rdev_to_dev(rdev),
			"Unsupported srq_attr_mask 0x%x", srq_attr_mask);
		return -EINVAL;
	}
	return 0;
}

int bnxt_re_query_srq(struct ib_srq *ib_srq, struct ib_srq_attr *srq_attr)
{
	struct bnxt_re_srq *srq = to_bnxt_re(ib_srq, struct bnxt_re_srq,
					     ib_srq);
	struct bnxt_re_dev *rdev = srq->rdev;
	int rc;

	/* Get live SRQ attr */
	/*TODO: qplib query_srq is incomplete. */
	rc = bnxt_qplib_query_srq(&rdev->qplib_res, &srq->qplib_srq);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Query HW SRQ (0x%x) failed! rc = %d",
			srq->qplib_srq.id, rc);
		return rc;
	}
	srq_attr->max_wr = srq->qplib_srq.max_wqe;
	srq_attr->max_sge = srq->qplib_srq.max_sge;
	srq_attr->srq_limit = srq->qplib_srq.srq_limit_hw;

	return 0;
}

int bnxt_re_post_srq_recv(struct ib_srq *ib_srq, CONST_STRUCT ib_recv_wr *wr,
			  CONST_STRUCT ib_recv_wr **bad_wr)
{
	struct bnxt_re_srq *srq = to_bnxt_re(ib_srq, struct bnxt_re_srq,
					     ib_srq);
	struct bnxt_qplib_swqe wqe = {};
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&srq->lock, flags);
	while (wr) {
		/* Transcribe each ib_recv_wr to qplib_swqe */
		wqe.num_sge = wr->num_sge;
		wqe.sg_list = (struct bnxt_qplib_sge *)wr->sg_list;
		wqe.wr_id = wr->wr_id;
		wqe.type = BNXT_QPLIB_SWQE_TYPE_RECV;
		rc = bnxt_qplib_post_srq_recv(&srq->qplib_srq, &wqe);
		if (rc) {
			*bad_wr = wr;
			break;
		}
		wr = wr->next;
	}
	spin_unlock_irqrestore(&srq->lock, flags);

	return rc;
}

unsigned long bnxt_re_lock_cqs(struct bnxt_re_qp *qp)
{
	unsigned long flags;

	spin_lock_irqsave(&qp->scq->cq_lock, flags);
	if (qp->rcq && qp->rcq != qp->scq)
		spin_lock(&qp->rcq->cq_lock);

	return flags;
}

void bnxt_re_unlock_cqs(struct bnxt_re_qp *qp,
				  unsigned long flags)
{
	if (qp->rcq && qp->rcq != qp->scq)
		spin_unlock(&qp->rcq->cq_lock);
	spin_unlock_irqrestore(&qp->scq->cq_lock, flags);
}

/* Queue Pairs */
static int bnxt_re_destroy_gsi_sqp(struct bnxt_re_qp *qp)
{
	struct bnxt_re_qp *gsi_sqp;
	struct bnxt_re_ah *gsi_sah;
	struct bnxt_re_dev *rdev;
	unsigned long flags;
	int rc = 0;

	rdev = qp->rdev;
	gsi_sqp = rdev->gsi_ctx.gsi_sqp;
	gsi_sah = rdev->gsi_ctx.gsi_sah;

	mutex_lock(&rdev->qp_lock);
	list_del(&gsi_sqp->list);
	mutex_unlock(&rdev->qp_lock);

	if (gsi_sah) {
		dev_dbg(rdev_to_dev(rdev), "Destroy the shadow AH\n");
		rc = bnxt_qplib_destroy_ah(&rdev->qplib_res, &gsi_sah->qplib_ah,
					   true);
		if (rc)
			dev_err(rdev_to_dev(rdev),
				"Destroy HW AH for shadow QP failed!");
		atomic_dec(&rdev->stats.rsors.ah_count);
		atomic_dec(&rdev->stats.rsors.ah_hw_count);
	}

	dev_dbg(rdev_to_dev(rdev), "Destroy the shadow QP\n");
	rc = bnxt_qplib_destroy_qp(&rdev->qplib_res, &gsi_sqp->qplib_qp, 0, NULL, NULL);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Destroy Shadow QP failed");

	/* Clean the CQ for shadow QP completions */
	flags = bnxt_re_lock_cqs(gsi_sqp);
	bnxt_qplib_clean_qp(&gsi_sqp->qplib_qp);
	bnxt_re_unlock_cqs(gsi_sqp, flags);

	bnxt_qplib_free_qp_res(&rdev->qplib_res, &gsi_sqp->qplib_qp);
	bnxt_qplib_free_hdr_buf(&rdev->qplib_res, &gsi_sqp->qplib_qp);
	kfree(rdev->gsi_ctx.sqp_tbl);
	kfree(gsi_sah);
	kfree(gsi_sqp);
	rdev->gsi_ctx.gsi_sqp = NULL;
	rdev->gsi_ctx.gsi_sah = NULL;
	rdev->gsi_ctx.sqp_tbl = NULL;
	atomic_dec(&rdev->stats.rsors.qp_count);

	return 0;
}

static void bnxt_re_dump_debug_stats(struct bnxt_re_dev *rdev, u32 active_qps)
{
	u32	total_qp = 0;
	u64	avg_time = 0;
	int	i;

	if (!rdev->rcfw.sp_perf_stats_enabled)
		return;

	switch (active_qps) {
	case 1:
		/* Potential hint for Test Stop */
		for (i = 0; i < RCFW_MAX_STAT_INDEX; i++) {
			if (rdev->rcfw.qp_destroy_stats[i]) {
				total_qp++;
				avg_time += rdev->rcfw.qp_destroy_stats[i];
			}
		}
		dev_dbg(rdev_to_dev(rdev),
			"Perf Debug: %ps Total (%d) QP destroyed in (%d) msec",
			__builtin_return_address(0), total_qp,
			jiffies_to_msecs(avg_time));
		break;
	case 2:
		/* Potential hint for Test Start */
		dev_dbg(rdev_to_dev(rdev),
			"Perf Debug: %ps active_qps = %d\n",
			__builtin_return_address(0), active_qps);
		break;
	default:
		/* Potential hint to know latency of QP destroy.
		 * Average time taken for 1K QP Destroy.
		 */
		if (active_qps > 1024 && !(active_qps % 1024))
			dev_dbg(rdev_to_dev(rdev),
				"Perf Debug: %ps Active QP (%d) Watermark (%d)",
				__builtin_return_address(0), active_qps,
				atomic_read(&rdev->stats.rsors.max_qp_count));
		break;
	}
}

void bnxt_re_free_qpdump(struct qdump_element *element)
{
	u32 i;

	vfree(element->buf);
	element->buf = NULL;

	for (i = 0; i <= element->level; i++) {
		vfree(element->pbl[i].pg_map_arr);
		element->pbl[i].pg_map_arr = NULL;
	}
}

static void bnxt_re_kqp_memcp(struct qdump_element *element)
{
	struct bnxt_qplib_hwq *hwq;
	u32 npages, i;
	void *buf;

	hwq = element->hwq;
	buf = element->buf;
	npages = (hwq->max_elements / hwq->qe_ppg);
	if (hwq->max_elements % hwq->qe_ppg)
		npages++;
	npages = min_t(int, npages, (ALIGN(element->len, PAGE_SIZE) / PAGE_SIZE));
	for (i = 0; i < npages; i++) {
		memcpy(buf, hwq->pbl_ptr[i], PAGE_SIZE);
		buf += PAGE_SIZE;
	}
}

static void bnxt_re_copy_qdump_pbl(struct qdump_element *element)
{
	struct bnxt_qplib_pbl *pbl_src, *pbl_dst;
	u32 i, arr_sz;

	pbl_src = element->hwq->pbl;
	pbl_dst = element->pbl;
	element->level = element->hwq->level;
	for (i = 0; i <= element->level; i++) {
		pbl_dst[i].pg_count = pbl_src[i].pg_count;
		pbl_dst[i].pg_size = pbl_src[i].pg_size;
		if (!pbl_dst[i].pg_count)
			continue;

		arr_sz = pbl_dst[i].pg_count * sizeof(dma_addr_t);
		pbl_dst[i].pg_map_arr = vzalloc(arr_sz);
		if (!pbl_dst[i].pg_map_arr)
			return;

		memcpy(pbl_dst[i].pg_map_arr, pbl_src[i].pg_map_arr, arr_sz);
	}
}

static void bnxt_re_copy_usrmem(struct ib_umem *umem, void *dest, u32 len)
{
	memset(dest, 0, len);

	if (IS_ERR_OR_NULL(umem))
		return;

	ib_umem_copy_from(dest, umem, 0, len);
}

static int bnxt_re_alloc_qdump_element(struct qdump_element *element,
				       u16 stride, const char *des)
{
	u32 length;

	length = ALIGN(element->len, PAGE_SIZE);
	element->buf = vzalloc(length);
	if (!element->buf)
		return -ENOMEM;

	if (element->is_user_qp)
		ib_umem_copy_from(element->buf, element->umem, 0,
				  min_t(int, length, element->umem->length));
	else
		bnxt_re_kqp_memcp(element);

	strscpy(element->des, des, sizeof(element->des));
	element->stride = stride;
	if (element->is_user_qp) {
		bnxt_re_copy_usrmem(element->uaddr_prod, &element->prod, sizeof(u32));
		bnxt_re_copy_usrmem(element->uaddr_cons, &element->cons, sizeof(u32));
	} else {
		element->prod = element->hwq->prod;
		element->cons = element->hwq->cons;
	}

	bnxt_re_copy_qdump_pbl(element);

	return 0;
}

static struct qdump_array *bnxt_re_get_next_qpdump(struct bnxt_re_dev *rdev)
{
	struct qdump_array *qdump = NULL;
	u32 index;

	index = rdev->qdump_head.index;
	qdump = &rdev->qdump_head.qdump[index];
	if (qdump->valid) {
		bnxt_re_free_qpdump(&qdump->sqd);
		bnxt_re_free_qpdump(&qdump->rqd);
		bnxt_re_free_qpdump(&qdump->scqd);
		bnxt_re_free_qpdump(&qdump->rcqd);
		bnxt_re_free_qpdump(&qdump->mrd);
	}

	memset(qdump, 0, sizeof(*qdump));

	index++;
	index %= rdev->qdump_head.max_elements;
	rdev->qdump_head.index = index;

	return qdump;
}

/*
 * bnxt_re_capture_qpdump - Capture snapshot of various queues of a QP.
 * @qp	-	Pointer to QP for which data has to be collected
 *
 * This function will capture snapshot of SQ/RQ/SCQ/RCQ of a QP which
 * can be used to debug any issue
 *
 * Returns: Zero on success or any negative value upon failure
 */
int bnxt_re_capture_qpdump(struct bnxt_re_qp *qp, void *qp_dump_buf, u32 buf_len, bool is_live)
{
	struct bnxt_qplib_qp *qpl = &qp->qplib_qp;
	struct bnxt_re_dev *rdev = qp->rdev;
	struct qdump_qpinfo *qpinfo;
	struct qdump_array *qdump;
	struct bnxt_re_srq *srq;
	bool capture_snapdump;
	u32 len = 0;

	if (is_live)
		goto bypass_dbg_check;

	if (rdev->snapdump_dbg_lvl == BNXT_RE_SNAPDUMP_NONE)
		return 0;

	capture_snapdump = test_bit(QP_FLAGS_CAPTURE_SNAPDUMP, &qpl->flags);
	if ((rdev->snapdump_dbg_lvl == BNXT_RE_SNAPDUMP_ERR) &&
	    !capture_snapdump)
		return 0;

	if (qp->is_snapdump_captured)
		return 0;

bypass_dbg_check:
	if (!rdev->qdump_head.qdump)
		return -ENOMEM;

	mutex_lock(&rdev->qdump_head.lock);
	qdump = bnxt_re_get_next_qpdump(rdev);
	if (!qdump) {
		mutex_unlock(&rdev->qdump_head.lock);
		return -ENOMEM;
	}

	qpinfo = &qdump->qpinfo;
	qpinfo->id = qpl->id;
	qpinfo->dest_qpid = qpl->dest_qpn;
	qpinfo->is_user = qpl->is_user;
	qpinfo->mtu = qpl->mtu;
	qpinfo->state = qpl->state;
	qpinfo->type = qpl->type;
	qpinfo->wqe_mode = qpl->wqe_mode;
	qpinfo->qp_handle = qpl->qp_handle;
	qpinfo->scq_handle = qp->scq->qplib_cq.cq_handle;
	qpinfo->rcq_handle = qp->rcq->qplib_cq.cq_handle;
	qpinfo->scq_id = qp->scq->qplib_cq.id;
	qpinfo->rcq_id = qp->rcq->qplib_cq.id;

	qdump->sqd.rdev = rdev;
	qdump->sqd.umem = qp->sumem;
	qdump->sqd.hwq = &qpl->sq.hwq;
	qdump->sqd.is_user_qp = qpl->is_user;
	qdump->sqd.len = (qpl->sq.max_wqe * qpl->sq.wqe_size);
	qdump->sqd.uaddr_prod = qp->sqprod;
	qdump->sqd.uaddr_cons = qp->sqcons;
	bnxt_re_alloc_qdump_element(&qdump->sqd, sizeof(struct sq_sge),
				    "SendQueue");
	if (qp->qplib_qp.srq) {
		srq = container_of(qp->qplib_qp.srq, struct bnxt_re_srq, qplib_srq);
		qdump->rqd.rdev = rdev;
		qdump->rqd.umem = srq->umem;
		qdump->rqd.hwq = &((qp->qplib_qp.srq)->hwq);
		qdump->rqd.is_user_qp = qpl->is_user;
		qdump->rqd.len = (qp->qplib_qp.srq->wqe_size * qp->qplib_qp.srq->max_wqe);
		qdump->rqd.uaddr_prod = qp->qplib_qp.srq->srqprod;
		qdump->rqd.uaddr_cons = qp->qplib_qp.srq->srqcons;
		bnxt_re_alloc_qdump_element(&qdump->rqd, sizeof(struct sq_sge),
					    "SharedRecvQueue");
	} else {
		qdump->rqd.rdev = rdev;
		qdump->rqd.umem = qp->rumem;
		qdump->rqd.hwq = &qpl->rq.hwq;
		qdump->rqd.is_user_qp = qpl->is_user;
		qdump->rqd.len = (qpl->rq.max_wqe * qpl->rq.wqe_size);
		qdump->rqd.uaddr_prod = qp->rqprod;
		qdump->rqd.uaddr_cons = qp->rqcons;
		bnxt_re_alloc_qdump_element(&qdump->rqd, sizeof(struct sq_sge),
					    "RecvQueue");
	}

	if (!qp->scq->is_snapdump_captured) {
		qdump->scqd.rdev = rdev;
		qdump->scqd.umem = qp->scq->umem;
		qdump->scqd.hwq = &qp->scq->qplib_cq.hwq;
		qdump->scqd.is_user_qp = qpl->is_user;
		qdump->scqd.len = (qpl->scq->max_wqe * sizeof(struct cq_base));
		qdump->scqd.uaddr_prod = qp->scq->cqprod;
		qdump->scqd.uaddr_cons = qp->scq->cqcons;
		bnxt_re_alloc_qdump_element(&qdump->scqd, sizeof(struct cq_base),
					    "SendCompQueue");
		qp->scq->is_snapdump_captured = true;
	}

	if (!qp->rcq->is_snapdump_captured) {
		qdump->rcqd.rdev = rdev;
		qdump->rcqd.umem = qp->rcq->umem;
		qdump->rcqd.hwq = &qp->rcq->qplib_cq.hwq;
		qdump->rcqd.is_user_qp = qpl->is_user;
		qdump->rcqd.len = (qpl->rcq->max_wqe * sizeof(struct cq_base));
		qdump->rcqd.uaddr_prod = qp->rcq->cqprod;
		qdump->rcqd.uaddr_cons = qp->rcq->cqcons;
		bnxt_re_alloc_qdump_element(&qdump->rcqd, sizeof(struct cq_base),
					    "RecvCompQueue");
		qp->rcq->is_snapdump_captured = true;
	}

	qdump->valid = true;
	if (is_live) {
		len += bnxt_re_dump_single_qpinfo(qdump, qp_dump_buf, buf_len);
		if (len < BNXT_RE_LIVE_QPDUMP_SZ)
			(void)bnxt_re_dump_single_qp(rdev, qdump, qp_dump_buf + len,
						      buf_len - len);
	} else {
		qp->is_snapdump_captured = true;
	}
	mutex_unlock(&rdev->qdump_head.lock);

	return 0;
}

/**
 * bnxt_re_save_udcc_session_data - Save destroyed session data in internal memory
 *
 * This function will save destroyed session data into local memory which
 * can be used to analyze destroyed UDCC sessions
 *
 * @rdev	-	Pointer to device instance structure
 * @data	-	Session data
 * @session_id	-	Session ID
 * @udcc_off	-	Offset of UDCC data
 * @udcc_sess_sz -	Size of UDCC data
 * @flags	-	Response flag
 */
static void bnxt_re_save_udcc_session_data(struct bnxt_re_dev *rdev, void *data,
					   u16 session_id, u16 udcc_off, u8 udcc_sess_sz,
					   u8 flags)
{
	struct bnxt_re_session_info *ses;
	u32 index;

	if (!rdev->udcc_data_head.udcc_data_arr)
		return;

	mutex_lock(&rdev->udcc_data_head.lock);
	index = rdev->udcc_data_head.index;
	ses = &rdev->udcc_data_head.ses[index];
	memcpy(ses->udcc_data, data + udcc_off, rdev->rcfw.udcc_session_sb_sz);
	ses->session_id = session_id;
	ses->valid_session = true;
	ses->udcc_sess_sz = udcc_sess_sz;
	ses->resp_flags = flags;
	index++;
	index %= rdev->udcc_data_head.max_elements;
	rdev->udcc_data_head.index = index;
	mutex_unlock(&rdev->udcc_data_head.lock);
}

static void bnxt_re_del_unique_gid(struct bnxt_re_dev *rdev)
{
	int rc;

	if (!rdev->rcfw.roce_mirror)
		return;

	rc = bnxt_qplib_del_sgid(&rdev->qplib_res.sgid_tbl,
				 (struct bnxt_qplib_gid *)&rdev->ugid,
				 0xFFFF, true);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to del unique GID. rc = %d\n", rc);
	rdev->ugid_index = BNXT_RE_INVALID_UGID_INDEX;
}

void bnxt_re_qp_free_umem(struct bnxt_re_qp *qp)
{
	if (!qp->rq_umem) {
		bnxt_re_umem_free(&qp->rumem);
	} else {
		bnxt_re_peer_mem_release(qp->rumem);
		kfree(qp->rq_umem);
	}
	if (!qp->sq_umem) {
		bnxt_re_umem_free(&qp->sumem);
	} else {
		bnxt_re_peer_mem_release(qp->sumem);
		kfree(qp->sq_umem);
	}
	bnxt_re_umem_free(&qp->sqprod);
	bnxt_re_umem_free(&qp->sqcons);
	bnxt_re_umem_free(&qp->rqprod);
	bnxt_re_umem_free(&qp->rqcons);
}

int bnxt_re_destroy_qp(struct ib_qp *ib_qp
#ifdef HAVE_DESTROY_QP_UDATA
		, struct ib_udata *udata
#endif
		)
{
	struct bnxt_re_qp *qp = to_bnxt_re(ib_qp, struct bnxt_re_qp, ib_qp);
	struct bnxt_qplib_qp *qplib_qp = &qp->qplib_qp;
	struct creq_destroy_qp_resp resp = {};
	struct bnxt_re_dev *rdev = qp->rdev;
	struct bnxt_qplib_nq *scq_nq = NULL;
	struct bnxt_qplib_nq *rcq_nq = NULL;
	void  *ctx_sb_data = NULL;
	bool do_snapdump;
	unsigned long flags;
	u32 active_qps;
	u16 ctx_size;
	int rc;

	mutex_lock(&rdev->qp_lock);
	bnxt_re_capture_qpdump(qp, NULL, 0, false);
	list_del(&qp->list);
	BNXT_RE_RES_LIST_DEL(rdev, qp, BNXT_RE_RES_TYPE_QP);
	active_qps = atomic_dec_return(&rdev->stats.rsors.qp_count);
	if (qp->qplib_qp.type == CMDQ_CREATE_QP_TYPE_RC)
		atomic_dec(&rdev->stats.rsors.rc_qp_count);
	else if (qp->qplib_qp.type == CMDQ_CREATE_QP_TYPE_UD)
		atomic_dec(&rdev->stats.rsors.ud_qp_count);
	if (qp->qplib_qp.ppp.st_idx_en & CREQ_MODIFY_QP_RESP_PINGPONG_PUSH_ENABLED)
		rdev->ppp_stats.ppp_enabled_qps--;
	mutex_unlock(&rdev->qp_lock);
	cancel_work_sync(&qp->dump_slot_task);

	if (rdev->hdbr_enabled)
		bnxt_re_hdbr_db_unreg_qp(rdev, qp);

	bnxt_re_qp_info_rem_qpinfo(rdev, qp);

	atomic_sub(qplib_qp->sq.max_wqe, &qplib_qp->scq->warn_depth);
	atomic_sub(qplib_qp->rq.max_wqe, &qplib_qp->rcq->warn_depth);

	if (!ib_qp->uobject)
		bnxt_qplib_flush_cqn_wq(&qp->qplib_qp);

	ctx_size = (qplib_qp->ctx_size_sb + rdev->rcfw.udcc_session_sb_sz);
	if (ctx_size)
		ctx_sb_data = vzalloc(ctx_size);

	rc = bnxt_qplib_destroy_qp(&rdev->qplib_res, &qp->qplib_qp,
				   ctx_size, ctx_sb_data,
				   (rdev->rcfw.roce_udcc_session_destroy_sb ?
				    (void *)&resp : NULL));

	do_snapdump = test_bit(QP_FLAGS_CAPTURE_SNAPDUMP, &qplib_qp->flags);
	if (!rc) {
		if (rdev->rcfw.roce_context_destroy_sb)
			bnxt_re_save_resource_context(rdev, ctx_sb_data,
						      BNXT_RE_RES_TYPE_QP, do_snapdump);
		if (ctx_sb_data && is_udcc_session_data_present(resp.flags))
			bnxt_re_save_udcc_session_data(rdev, ctx_sb_data,
						       resp.udcc_session_id,
						       resp.udcc_session_data_offset,
						       resp.udcc_session_data_size,
						       resp.flags);
	} else {
		dev_err_ratelimited(rdev_to_dev(rdev), "%s id = %d failed rc = %d",
				    __func__, qp->qplib_qp.id, rc);
	}

	vfree(ctx_sb_data);

	if (!ib_qp->uobject) {
		flags = bnxt_re_lock_cqs(qp);
		bnxt_qplib_clean_qp(&qp->qplib_qp);
		bnxt_re_unlock_cqs(qp, flags);
	}

	bnxt_qplib_free_qp_res(&rdev->qplib_res, &qp->qplib_qp);
	if (ib_qp->qp_type == IB_QPT_GSI &&
	    rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_UD) {
		if (rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_ALL &&
		    rdev->gsi_ctx.gsi_sqp) {
			bnxt_re_destroy_gsi_sqp(qp);
		}
		bnxt_qplib_free_hdr_buf(&rdev->qplib_res, &qp->qplib_qp);
	}

	if (qp->qplib_qp.type == CMDQ_CREATE_QP_TYPE_RAW_ETHERTYPE &&
	    qp->qplib_qp.is_roce_mirror_qp)
		bnxt_re_del_unique_gid(rdev);

	bnxt_re_qp_free_umem(qp);

	/* Flush all the entries of notification queue associated with
	 * given qp.
	 */
	scq_nq = qplib_qp->scq->nq;
	rcq_nq = qplib_qp->rcq->nq;
	bnxt_re_synchronize_nq(scq_nq);
	if (scq_nq != rcq_nq)
		bnxt_re_synchronize_nq(rcq_nq);

	if (ib_qp->qp_type == IB_QPT_GSI)
		rdev->gsi_ctx.gsi_qp_mode = BNXT_RE_GSI_MODE_INVALID;

#ifndef HAVE_QP_ALLOC_IN_IB_CORE
	kfree(qp);
#endif

	bnxt_re_dump_debug_stats(rdev, active_qps);

	/* return success for destroy resources */
	return 0;
}

u8 __from_ib_qp_type(enum ib_qp_type type)
{
	switch (type) {
	case IB_QPT_GSI:
		return CMDQ_CREATE_QP1_TYPE_GSI;
	case IB_QPT_RC:
		return CMDQ_CREATE_QP_TYPE_RC;
	case IB_QPT_UD:
		return CMDQ_CREATE_QP_TYPE_UD;
	case IB_QPT_RAW_ETHERTYPE:
	case IB_QPT_RAW_PACKET:
		return CMDQ_CREATE_QP_TYPE_RAW_ETHERTYPE;
	default:
		return IB_QPT_MAX;
	}
}

static int _get_sq_send_hdr_sz(struct bnxt_re_dev *rdev, bool compact)
{
	/*
	 * For HSI V3:
	 * compact = true  hdr_size sizeof sq_send_hdr_v3   16B
	 * compact = false hdr_size sizeof sq_udsend_hdr_v3 32B
	 * Other cases     hdr_size sizeof sq_send_hdr      32B
	 */
	return _is_hsi_v3(rdev->chip_ctx) ?
	       ((compact) ? sizeof(struct sq_send_hdr_v3) :
		sizeof(struct sq_udsend_hdr_v3)) :
	       sizeof(struct sq_send_hdr);
}

static u16 _get_swqe_sz(struct bnxt_re_dev *rdev, int nsge, bool compact)
{
	return _get_sq_send_hdr_sz(rdev, compact) + (nsge * sizeof(struct sq_sge));
}

static int bnxt_re_get_swqe_size(struct bnxt_re_dev *rdev, int ilsize, int nsge,
				 int align, bool compact)
{
	u16 wqe_size, calc_ils;

	wqe_size = _get_swqe_sz(rdev, nsge, compact);
	if (ilsize) {
		calc_ils = _get_sq_send_hdr_sz(rdev, compact) + ilsize;
		wqe_size = max_t(int, calc_ils, wqe_size);
		wqe_size = ALIGN(wqe_size, align);
	}
	return wqe_size;
}

static int bnxt_re_setup_swqe_size(struct bnxt_re_qp *qp,
				   struct ib_qp_init_attr *init_attr)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;
	struct bnxt_qplib_q *sq;
	bool compact = false;
	int align, ilsize;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	sq = &qplqp->sq;
	dev_attr = rdev->dev_attr;

	align = _get_sq_send_hdr_sz(rdev, compact);
	if (qplqp->wqe_mode == BNXT_QPLIB_WQE_MODE_VARIABLE && !_is_hsi_v3(rdev->chip_ctx))
		align = sizeof(struct sq_sge);
	ilsize = ALIGN(init_attr->cap.max_inline_data, align);
	sq->wqe_size = bnxt_re_get_swqe_size(rdev, ilsize, sq->max_sge, align, compact);
	if (sq->wqe_size > _get_swqe_sz(rdev, dev_attr->max_qp_sges, compact))
		return -EINVAL;

	/* For Cu/Wh and gen p5 backward compatibility mode
	 * wqe size is fixed to 128 bytes
	 */
	if (sq->wqe_size < _get_swqe_sz(rdev, dev_attr->max_qp_sges, compact) &&
	    qplqp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC)
		sq->wqe_size = _get_swqe_sz(rdev, dev_attr->max_qp_sges, compact);

	if (init_attr->cap.max_inline_data) {
		qplqp->max_inline_data = sq->wqe_size - align;
		init_attr->cap.max_inline_data = qplqp->max_inline_data;
		if (qplqp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC)
			sq->max_sge = qplqp->max_inline_data /
				      sizeof(struct sq_sge);
	}

	/* only when send_flag & IB_SEND_INLINE, inline_data has significance.
	 * so set it here all times, that way apps that not query_qp or not set
	 * qp_attr->max_inline_data but with IB_SEND_INLINE will still work.
	 */
	if (qplqp->wqe_mode == BNXT_QPLIB_WQE_MODE_VARIABLE) {
		qplqp->max_inline_data = sq->wqe_size - sizeof(struct sq_send_hdr);
		init_attr->cap.max_inline_data = qplqp->max_inline_data;
	}

	return 0;
}

static int bnxt_re_init_user_qp(struct bnxt_re_dev *rdev,
				struct bnxt_re_pd *pd, struct bnxt_re_qp *qp,
				struct ib_udata *udata)
{
	struct bnxt_qplib_sg_info *sginfo;
	struct bnxt_re_qp_req ureq = {};
	struct bnxt_qplib_qp *qplib_qp;
	struct bnxt_re_ucontext *cntx;
	struct ib_ucontext *context;
	struct ib_umem *umem;
	int rc, bytes = 0;
	int psn_nume;
	int psn_sz;

	qplib_qp = &qp->qplib_qp;
	context = pd->ib_pd.uobject->context;
	cntx = to_bnxt_re(context, struct bnxt_re_ucontext, ib_uctx);
	sginfo = &qplib_qp->sq.sginfo;

	if (udata->inlen < sizeof(ureq))
		dev_warn_once(rdev_to_dev(rdev),
			 "Update the library ulen %d klen %d",
			 (unsigned int)udata->inlen,
			 (unsigned int)sizeof(ureq));

	rc = ib_copy_from_udata(&ureq, udata,
				min(udata->inlen, sizeof(ureq)));
	if (rc)
		return rc;

	bytes = (qplib_qp->sq.max_wqe * qplib_qp->sq.wqe_size);
	bytes = PAGE_ALIGN(bytes);
	/* Consider mapping PSN search memory only for RC QPs. */
	if (qplib_qp->type == CMDQ_CREATE_QP_TYPE_RC && !_is_chip_p8(rdev->chip_ctx)) {
		psn_sz = _is_chip_gen_p5_p7(rdev->chip_ctx) ?
				sizeof(struct sq_psn_search_ext) :
				sizeof(struct sq_psn_search);
		if (rdev->dev_attr && _is_host_msn_table(rdev->dev_attr->dev_cap_ext_flags2))
			psn_sz = sizeof(struct sq_msn_search);
		psn_nume = (qplib_qp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC) ?
			    qplib_qp->sq.max_wqe :
			    ((qplib_qp->sq.max_wqe * qplib_qp->sq.wqe_size) /
			     sizeof(struct bnxt_qplib_sge));
		if (rdev->dev_attr && _is_host_msn_table(rdev->dev_attr->dev_cap_ext_flags2))
			psn_nume = roundup_pow_of_two(psn_nume);

		bytes += (psn_nume * psn_sz);
		bytes = PAGE_ALIGN(bytes);
	}
	umem = ib_umem_get_compat(rdev, context, udata, ureq.qpsva, bytes,
				  IB_ACCESS_LOCAL_WRITE, 1);
	if (IS_ERR(umem)) {
		dev_err(rdev_to_dev(rdev), "%s: ib_umem_get failed with %ld\n",
			__func__, PTR_ERR(umem));
		return PTR_ERR(umem);
	}

	qp->sumem = umem;
	bnxt_re_set_sginfo(rdev, umem, ureq.qpsva, sginfo, BNXT_RE_RES_TYPE_QP);
	/* pgsize and pgshft were initialize already. */
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	sginfo->sghead = get_ib_umem_sgl(umem, &sginfo->nmap);
#else
	sginfo->umem = umem;
#endif
	qplib_qp->qp_handle = ureq.qp_handle;

	if (!qp->qplib_qp.srq) {
		sginfo = &qplib_qp->rq.sginfo;
		bytes = (qplib_qp->rq.max_wqe * qplib_qp->rq.wqe_size);
		bytes = PAGE_ALIGN(bytes);
		umem = ib_umem_get_compat(rdev,
					  context, udata, ureq.qprva, bytes,
					  IB_ACCESS_LOCAL_WRITE, 1);
		if (IS_ERR(umem)) {
			dev_err(rdev_to_dev(rdev),
				"%s: ib_umem_get failed ret =%ld\n",
				__func__, PTR_ERR(umem));
			goto rqfail;
		}
		qp->rumem = umem;
		bnxt_re_set_sginfo(rdev, umem, ureq.qprva, sginfo, BNXT_RE_RES_TYPE_QP);
		/* pgsize and pgshft were initialize already. */
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
		sginfo->sghead = get_ib_umem_sgl(umem, &sginfo->nmap);
#else
		sginfo->umem = umem;
#endif
	} /* TODO: Add srq. */

	qplib_qp->dpi = &cntx->dpi;
	qplib_qp->exp_mode = ureq.exp_mode;
	qplib_qp->is_user = true;

	if (ureq.sqprodva)
		qp->sqprod = ib_umem_get_compat(rdev, context, udata, ureq.sqprodva,
						sizeof(u32), IB_ACCESS_LOCAL_WRITE, 1);

	if (ureq.sqconsva)
		qp->sqcons = ib_umem_get_compat(rdev, context, udata, ureq.sqconsva,
						sizeof(u32), IB_ACCESS_LOCAL_WRITE, 1);

	if (ureq.rqprodva)
		qp->rqprod = ib_umem_get_compat(rdev, context, udata, ureq.rqprodva,
						sizeof(u32), IB_ACCESS_LOCAL_WRITE, 1);

	if (ureq.rqconsva)
		qp->rqcons = ib_umem_get_compat(rdev, context, udata, ureq.rqconsva,
						sizeof(u32), IB_ACCESS_LOCAL_WRITE, 1);
	return 0;
rqfail:
	bnxt_re_umem_free(&qp->sumem);
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	qplib_qp->sq.sginfo.sghead = NULL;
	qplib_qp->sq.sginfo.nmap = 0;
#else
	qplib_qp->sq.sginfo.umem = NULL;
#endif

	return PTR_ERR(umem);
}

static struct bnxt_re_ah *bnxt_re_create_shadow_qp_ah(struct bnxt_re_pd *pd,
					       struct bnxt_qplib_res *qp1_res,
					       struct bnxt_qplib_qp *qp1_qp)
{
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_re_ah *ah;
	union ib_gid sgid;
	int rc;

	ah = kzalloc(sizeof(*ah), GFP_KERNEL);
	if (!ah)
		return NULL;
	memset(ah, 0, sizeof(*ah));
	ah->rdev = rdev;
	ah->qplib_ah.pd = &pd->qplib_pd;

	rc = bnxt_re_query_gid(&rdev->ibdev, 1, 0, &sgid);
	if (rc)
		goto fail;

	/* supply the dgid data same as sgid */
	memcpy(ah->qplib_ah.dgid.data, &sgid.raw,
	       sizeof(union ib_gid));
	ah->qplib_ah.sgid_index = 0;

	ah->qplib_ah.traffic_class = 0;
	ah->qplib_ah.flow_label = 0;
	ah->qplib_ah.hop_limit = 1;
	ah->qplib_ah.sl = 0;
	/* Have DMAC same as SMAC */
	ether_addr_copy(ah->qplib_ah.dmac, rdev->netdev->dev_addr);
	dev_dbg(rdev_to_dev(rdev), "ah->qplib_ah.dmac = %x:%x:%x:%x:%x:%x\n",
		ah->qplib_ah.dmac[0], ah->qplib_ah.dmac[1], ah->qplib_ah.dmac[2],
		ah->qplib_ah.dmac[3], ah->qplib_ah.dmac[4], ah->qplib_ah.dmac[5]);

	rc = bnxt_qplib_create_ah(&rdev->qplib_res, &ah->qplib_ah, true);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Allocate HW AH for Shadow QP failed!");
		goto fail;
	}
	dev_dbg(rdev_to_dev(rdev), "AH ID = %d\n", ah->qplib_ah.id);
	atomic_inc(&rdev->stats.rsors.ah_count);
	atomic_inc(&rdev->stats.rsors.ah_hw_count);

	return ah;
fail:
	kfree(ah);
	return NULL;
}

void bnxt_re_update_shadow_ah(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_qp *gsi_qp;
	struct bnxt_re_ah *sah;
	struct bnxt_re_pd *pd;
	struct ib_pd *ib_pd;
	int rc;

	if (!rdev)
		return;

	sah = rdev->gsi_ctx.gsi_sah;

	dev_dbg(rdev_to_dev(rdev), "Updating the AH\n");
	if (sah) {
		/* Check if the AH created with current mac address */
		if (!compare_ether_header(sah->qplib_ah.dmac, rdev->netdev->dev_addr)) {
			dev_dbg(rdev_to_dev(rdev),
				"Not modifying shadow AH during AH update\n");
			return;
		}

		gsi_qp = rdev->gsi_ctx.gsi_qp;
		ib_pd = gsi_qp->ib_qp.pd;
		pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ib_pd);
		rc = bnxt_qplib_destroy_ah(&rdev->qplib_res,
					   &sah->qplib_ah, false);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"Failed to destroy shadow AH during AH update");
			return;
		}
		atomic_dec(&rdev->stats.rsors.ah_count);
		atomic_dec(&rdev->stats.rsors.ah_hw_count);
		kfree(sah);
		rdev->gsi_ctx.gsi_sah = NULL;

		sah = bnxt_re_create_shadow_qp_ah(pd, &rdev->qplib_res,
						  &gsi_qp->qplib_qp);
		if (!sah) {
			dev_err(rdev_to_dev(rdev),
				"Failed to update AH for ShadowQP");
			return;
		}
		rdev->gsi_ctx.gsi_sah = sah;
		atomic_inc(&rdev->stats.rsors.ah_count);
		atomic_inc(&rdev->stats.rsors.ah_hw_count);
	}
}

#ifdef POST_QP1_DUMMY_WQE
static int post_qp1_dummy_wqe(struct bnxt_re_qp *qp)
{
	struct bnxt_qplib_qp *lib_qp = &qp->qplib_qp;
	struct bnxt_qplib_swqe wqe = {0};
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&qp->sq_lock, flags);

	wqe.num_sge = 0;
	wqe.wr_id = BNXT_QPLIB_QP1_DUMMY_WRID;
	wqe.type = BNXT_QPLIB_SWQE_TYPE_SEND;
	wqe.flags = 0;

	rc = bnxt_qplib_post_send(lib_qp, &wqe, NULL);
	if (!rc)
		bnxt_qplib_post_send_db(lib_qp);

	spin_unlock_irqrestore(&qp->sq_lock, flags);
	return rc;
}
#endif /* POST_QP1_DUMMY_WQE */

static struct bnxt_re_qp *bnxt_re_create_shadow_qp(struct bnxt_re_pd *pd,
					    struct bnxt_qplib_res *qp1_res,
					    struct bnxt_qplib_qp *qp1_qp)
{
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_re_qp *qp;
	int rc;

	qp = kzalloc(sizeof(*qp), GFP_KERNEL);
	if (!qp)
		return NULL;
	memset(qp, 0, sizeof(*qp));
	qp->rdev = rdev;

	/* Initialize the shadow QP structure from the QP1 values */
	ether_addr_copy(qp->qplib_qp.smac, rdev->netdev->dev_addr);
	qp->qplib_qp.pd_id = pd->qplib_pd.id;
	qp->qplib_qp.qp_handle = (u64)&qp->qplib_qp;
	qp->qplib_qp.type = IB_QPT_UD;
	qp->qplib_qp.cctx = rdev->chip_ctx;

	qp->qplib_qp.max_inline_data = 0;
	qp->qplib_qp.sig_type = true;

	/* Shadow QP SQ depth should be same as QP1 RQ depth */
	qp->qplib_qp.sq.wqe_size = bnxt_re_get_swqe_size(rdev, 0, 6, 32, false);
	qp->qplib_qp.sq.max_wqe = qp1_qp->rq.max_wqe;
	qp->qplib_qp.sq.max_sge = 2;
	qp->qplib_qp.sq.max_sw_wqe = qp->qplib_qp.sq.max_wqe;
	/* Q full delta can be 1 since it is internal QP */
	qp->qplib_qp.sq.q_full_delta = 1;
	qp->qplib_qp.sq.sginfo.pgsize = PAGE_SIZE;
	qp->qplib_qp.sq.sginfo.pgshft = PAGE_SHIFT;

	qp->qplib_qp.scq = qp1_qp->scq;
	qp->qplib_qp.rcq = qp1_qp->rcq;

	qp->qplib_qp.rq.wqe_size = _max_rwqe_sz(rdev, 6); /* 128 Byte wqe size */
	qp->qplib_qp.rq.max_wqe = qp1_qp->rq.max_wqe;
	qp->qplib_qp.rq.max_sge = qp1_qp->rq.max_sge;
	qp->qplib_qp.rq.max_sw_wqe = qp->qplib_qp.sq.max_wqe;
	qp->qplib_qp.rq.sginfo.pgsize = PAGE_SIZE;
	qp->qplib_qp.rq.sginfo.pgshft = PAGE_SHIFT;
	/* Q full delta can be 1 since it is internal QP */
	qp->qplib_qp.rq.q_full_delta = 1;
	qp->qplib_qp.mtu = qp1_qp->mtu;
	qp->qplib_qp.dpi = &rdev->dpi_privileged;

	rc = bnxt_re_setup_qp_hwqs(qp, false);
	if (rc)
		goto fail;

	rc = bnxt_qplib_alloc_hdr_buf(qp1_res, &qp->qplib_qp, 0,
				      BNXT_QPLIB_MAX_GRH_HDR_SIZE_IPV6);
	if (rc)
		goto fail;

	rc = bnxt_qplib_create_qp(qp1_res, &qp->qplib_qp);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "create HW QP failed!");
		goto qp_fail;
	}

	dev_dbg(rdev_to_dev(rdev), "Created shadow QP with ID = %d\n",
		qp->qplib_qp.id);
	spin_lock_init(&qp->sq_lock);
	INIT_LIST_HEAD(&qp->list);
	mutex_lock(&rdev->qp_lock);
	list_add_tail(&qp->list, &rdev->qp_list);
	atomic_inc(&rdev->stats.rsors.qp_count);
	mutex_unlock(&rdev->qp_lock);
	return qp;
qp_fail:
	bnxt_qplib_free_hdr_buf(qp1_res, &qp->qplib_qp);
fail:
	kfree(qp);
	return NULL;
}

static int bnxt_re_init_rq_attr(struct bnxt_re_qp *qp,
				struct ib_qp_init_attr *init_attr,
				struct bnxt_re_ucontext *cntx)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;
	struct bnxt_qplib_q *rq;
	int entries;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	rq = &qplqp->rq;
	dev_attr = rdev->dev_attr;

	if (init_attr->srq) {
		struct bnxt_re_srq *srq;

		srq = to_bnxt_re(init_attr->srq, struct bnxt_re_srq, ib_srq);
		if (!srq) {
			dev_err(rdev_to_dev(rdev), "SRQ not found");
			return -EINVAL;
		}
		qplqp->srq = &srq->qplib_srq;
		rq->max_wqe = 0;
	} else {
		if (cntx)
			qplqp->small_recv_wqe_sup = cntx->small_recv_wqe_sup;
		rq->max_sge = init_attr->cap.max_recv_sge;
		if (rq->max_sge > dev_attr->max_qp_sges)
			rq->max_sge = dev_attr->max_qp_sges;
		init_attr->cap.max_recv_sge = rq->max_sge;
		rq->wqe_size = bnxt_re_get_rwqe_size(rdev, qplqp, rq->max_sge,
						     dev_attr->max_qp_sges);

		/* Allocate 1 more than what's provided so posting max doesn't
		   mean empty */
		entries = init_attr->cap.max_recv_wr + 1;
		entries = bnxt_re_init_depth(entries, cntx);
		/* Additional 1 is to avoid RQ full due to issue in mad agent
		 * in kernel stack.
		 * If ib_mad_port_start and ib_mad_recv_done is happening
		 * in parallel, it is possible that driver detect RQ full
		 * condition due to race (potential kernel issue).
		 * In worst case it can lead QP 1 is not created.
		 * Avoid this race condition providing one extra room as workaround.
		 */
		if (init_attr->qp_type == IB_QPT_GSI)
			entries++;
		rq->max_wqe = min_t(u32, entries, dev_attr->max_rq_wqes);
		rq->max_sw_wqe = rq->max_wqe;
		rq->q_full_delta = 0;
		rq->sginfo.pgsize = PAGE_SIZE;
		rq->sginfo.pgshft = PAGE_SHIFT;
	}

	return 0;
}

static void bnxt_re_adjust_gsi_rq_attr(struct bnxt_re_qp *qp)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	dev_attr = rdev->dev_attr;

	if (rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_UD)
		qplqp->rq.max_sge = dev_attr->max_qp_sges;
}

static int bnxt_re_init_sq_attr(struct bnxt_re_qp *qp,
				struct ib_qp_init_attr *init_attr,
				struct bnxt_re_ucontext *cntx)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;
	struct bnxt_qplib_q *sq;
	int diff = 0;
	int entries;
	u32 sqsz;
	int rc;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	sq = &qplqp->sq;
	dev_attr = rdev->dev_attr;

	sq->max_sge = init_attr->cap.max_send_sge;
	if (_is_hsi_v3(rdev->chip_ctx) && qplqp->wqe_mode == BNXT_QPLIB_WQE_MODE_VARIABLE)
		sq->max_sge = dev_attr->max_qp_sges;

	if (sq->max_sge > dev_attr->max_qp_sges) {
		sq->max_sge = dev_attr->max_qp_sges;
		init_attr->cap.max_send_sge = sq->max_sge;
	}
	rc = bnxt_re_setup_swqe_size(qp, init_attr);
	if (rc)
		return rc;
	/*
	 * Change the SQ depth if user has requested minimum using
	 * configfs. Only supported for kernel consumers. Setting
	 * min_tx_depth to 4096 to handle iser SQ full condition
	 * in most of the newer OS distros
	 */
	entries = init_attr->cap.max_send_wr;
	if (!cntx && rdev->min_tx_depth && init_attr->qp_type != IB_QPT_GSI) {
		/*
		 * If users specify any value greater than 1 use min_tx_depth
		 * provided by user for comparison. Else, compare it with the
		 * BNXT_RE_MIN_KERNEL_QP_TX_DEPTH and adjust it accordingly.
		 */
		if (rdev->min_tx_depth > 1 && entries < rdev->min_tx_depth)
			entries = rdev->min_tx_depth;
		else if (entries < BNXT_RE_MIN_KERNEL_QP_TX_DEPTH)
			entries = BNXT_RE_MIN_KERNEL_QP_TX_DEPTH;
	}
	diff = bnxt_re_get_diff(cntx, rdev->chip_ctx);
	/* FIXME: the min equation at the boundary condition */
	entries = bnxt_re_init_depth(entries + diff + 1, cntx);
	sq->max_wqe = min_t(u32, entries, dev_attr->max_sq_wqes + diff + 1);
	sq->q_full_delta = diff + 1;
	/*
	 * Reserving one slot for Phantom WQE. Application can
	 * post one extra entry in this case. But allowing this to avoid
	 * unexpected Queue full condition
	 */
	sq->q_full_delta -= 1; /* becomes 0 for gen-p5 */
	sq->sginfo.pgsize = PAGE_SIZE;
	sq->sginfo.pgshft = PAGE_SHIFT;

	sqsz = bnxt_re_set_sq_size(sq, qplqp->wqe_mode);
	/* 0xffff is the max sq size hw limits to */
	if (sqsz > BNXT_QPLIB_MAX_SQSZ) {
		dev_err(rdev_to_dev(rdev),
			"QPLIB: FP: QP (0x%x) exceeds sq size %d", qplqp->id,
			sqsz);
		return -EINVAL;
	}
	/* Initialize the max_sw_wqe same as the slot for variable size wqe */
	sq->max_sw_wqe = sqsz;

	return 0;
}

static void bnxt_re_adjust_gsi_sq_attr(struct bnxt_re_qp *qp,
				       struct ib_qp_init_attr *init_attr,
				       void *cntx)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;
	int entries;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	dev_attr = rdev->dev_attr;

	if (rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_UD) {
		entries = init_attr->cap.max_send_wr + 1;
		entries = bnxt_re_init_depth(entries, cntx);
		qplqp->sq.max_wqe = min_t(u32, entries,
					  dev_attr->max_sq_wqes + 1);
		qplqp->sq.q_full_delta = qplqp->sq.max_wqe -
					 init_attr->cap.max_send_wr;
		qplqp->sq.max_sge++; /* Need one extra sge to put UD header */
		if (qplqp->sq.max_sge > dev_attr->max_qp_sges)
			qplqp->sq.max_sge = dev_attr->max_qp_sges;
	}
}

static int bnxt_re_init_qp_type(struct bnxt_re_dev *rdev,
				struct ib_qp_init_attr *init_attr)
{
	struct bnxt_qplib_chip_ctx *chip_ctx;
	struct bnxt_re_gsi_context *gsi_ctx;
	int qptype;

	chip_ctx = rdev->chip_ctx;
	gsi_ctx = &rdev->gsi_ctx;

	qptype = __from_ib_qp_type(init_attr->qp_type);
	if (qptype == IB_QPT_MAX) {
		dev_err(rdev_to_dev(rdev), "QP type 0x%x not supported",
			qptype);
		qptype = -EINVAL;
		goto out;
	}

	if (_is_chip_p5_plus(chip_ctx) &&
	    init_attr->qp_type == IB_QPT_GSI) {
		/* For Thor+ always force UD mode. */
		qptype = CMDQ_CREATE_QP_TYPE_GSI;
		gsi_ctx->gsi_qp_mode = BNXT_RE_GSI_MODE_UD;
	}
out:
	return qptype;
}

static int bnxt_re_init_qp_wqe_mode(struct bnxt_re_dev *rdev)
{
	return rdev->chip_ctx->modes.wqe_mode;
}

static void bnxt_re_check_cq_depth(struct bnxt_re_qp *qp)
{
	int scq_depth, rcq_depth;

	scq_depth = atomic_add_return(qp->qplib_qp.sq.max_wqe,
				      &qp->scq->qplib_cq.warn_depth);
	if (qp->scq->qplib_cq.max_wqe < scq_depth) {
		dev_dbg(rdev_to_dev(qp->rdev),
			"QP 0x%x SQD %d -> low CQ depth %d SCQ 0x%x\n",
			qp->qplib_qp.id, qp->qplib_qp.sq.max_wqe,
			qp->scq->qplib_cq.max_wqe, qp->scq->qplib_cq.id);
	}

	rcq_depth = atomic_add_return(qp->qplib_qp.rq.max_wqe,
				      &qp->rcq->qplib_cq.warn_depth);
	if (qp->rcq->qplib_cq.max_wqe < rcq_depth) {
		dev_dbg(rdev_to_dev(qp->rdev),
			"QP 0x%x RQD %d -> low CQ depth %d for RCQ 0x%x\n",
			qp->qplib_qp.id, qp->qplib_qp.rq.max_wqe,
			qp->rcq->qplib_cq.max_wqe, qp->rcq->qplib_cq.id);
	}
}

static void bnxt_re_qp_calculate_msn_psn_size(struct bnxt_re_qp *qp)
{
	struct bnxt_qplib_qp *qplib_qp = &qp->qplib_qp;
	struct bnxt_qplib_q *sq = &qplib_qp->sq;
	struct bnxt_re_dev *rdev = qp->rdev;
	u8 wqe_mode = qplib_qp->wqe_mode;

	if (rdev->dev_attr)
		qplib_qp->is_host_msn_tbl =
			_is_host_msn_table(rdev->dev_attr->dev_cap_ext_flags2);

	if (qplib_qp->type == CMDQ_CREATE_QP_TYPE_RC) {
		qplib_qp->psn_sz = _is_chip_gen_p5_p7(rdev->chip_ctx) ?
			sizeof(struct sq_psn_search_ext) :
			sizeof(struct sq_psn_search);
		if (qplib_qp->is_host_msn_tbl) {
			qplib_qp->psn_sz = sizeof(struct sq_msn_search);
			qplib_qp->msn = 0;
		}
	}

	/* Update msn tbl size */
	if (qplib_qp->is_host_msn_tbl && qplib_qp->psn_sz) {
		if (wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC)
			qplib_qp->msn_tbl_sz =
				roundup_pow_of_two(bnxt_re_set_sq_size(sq, wqe_mode));
		else
			qplib_qp->msn_tbl_sz =
				roundup_pow_of_two(bnxt_re_set_sq_size(sq, wqe_mode)) / 2;
		qplib_qp->msn = 0;
	}
}

static int bnxt_re_qp_alloc_init_xrrq(struct bnxt_re_qp *qp)
{
	struct bnxt_qplib_res *res = &qp->rdev->qplib_res;
	struct bnxt_qplib_qp *qplib_qp = &qp->qplib_qp;
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_sg_info sginfo = {};
	struct bnxt_qplib_hwq *irrq, *orrq;
	int rc, req_size;

	orrq = &qplib_qp->orrq;
	orrq->max_elements =
			min_t(u32, BNXT_QPLIB_MAX_OUT_RD_ATOM,
			      ORD_LIMIT_TO_ORRQ_SLOTS(qplib_qp->max_rd_atomic));
	req_size = orrq->max_elements *
		BNXT_QPLIB_MAX_ORRQE_ENTRY_SIZE + PAGE_SIZE - 1;
	req_size &= ~(PAGE_SIZE - 1);
	sginfo.pgsize = req_size;
	sginfo.pgshft = PAGE_SHIFT;

	hwq_attr.res = res;
	hwq_attr.sginfo = &sginfo;
	hwq_attr.depth = orrq->max_elements;
	hwq_attr.stride = BNXT_QPLIB_MAX_ORRQE_ENTRY_SIZE;
	hwq_attr.aux_stride = 0;
	hwq_attr.aux_depth = 0;
	hwq_attr.type = HWQ_TYPE_CTX;
	rc = bnxt_qplib_alloc_init_hwq(orrq, &hwq_attr);
	if (rc)
		return rc;

	irrq = &qplib_qp->irrq;
	irrq->max_elements =  min_t(u32, BNXT_QPLIB_MAX_IN_RD_ATOM,
				    IRD_LIMIT_TO_IRRQ_SLOTS(qplib_qp->max_dest_rd_atomic));
	req_size = irrq->max_elements *
		BNXT_QPLIB_MAX_IRRQE_ENTRY_SIZE + PAGE_SIZE - 1;
	req_size &= ~(PAGE_SIZE - 1);
	sginfo.pgsize = req_size;
	hwq_attr.sginfo = &sginfo;
	hwq_attr.depth =  irrq->max_elements;
	hwq_attr.stride = BNXT_QPLIB_MAX_IRRQE_ENTRY_SIZE;
	rc = bnxt_qplib_alloc_init_hwq(irrq, &hwq_attr);
	if (rc)
		goto free_orrq_hwq;
	return 0;
free_orrq_hwq:
	bnxt_qplib_free_hwq(res, orrq);
	return rc;
}

int bnxt_re_setup_qp_hwqs(struct bnxt_re_qp *qp, bool is_dv_qp)
{
	struct bnxt_qplib_res *res = &qp->rdev->qplib_res;
	struct bnxt_qplib_qp *qplib_qp = &qp->qplib_qp;
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_q *sq = &qplib_qp->sq;
	struct bnxt_qplib_q *rq = &qplib_qp->rq;
	u8 wqe_mode = qplib_qp->wqe_mode;
	u8 pg_sz_lvl;
	int rc;

	hwq_attr.res = res;
	hwq_attr.sginfo = &sq->sginfo;
	hwq_attr.stride = bnxt_qplib_get_stride();
	hwq_attr.aux_stride = qplib_qp->psn_sz;
	if (!is_dv_qp) {
		hwq_attr.depth = bnxt_qplib_get_depth(sq, wqe_mode, true);
		hwq_attr.aux_depth = (qplib_qp->psn_sz) ?
				bnxt_re_set_sq_size(sq, wqe_mode) : 0;
		if (qplib_qp->is_host_msn_tbl && qplib_qp->psn_sz)
			hwq_attr.aux_depth = qplib_qp->msn_tbl_sz;
	} else {
		hwq_attr.depth = sq->max_wqe;
		hwq_attr.aux_depth = qplib_qp->msn_tbl_sz;
	}
	hwq_attr.type = HWQ_TYPE_QUEUE;
	rc = bnxt_qplib_alloc_init_hwq(&sq->hwq, &hwq_attr);
	if (rc)
		return rc;

	pg_sz_lvl = _get_base_pg_size(&sq->hwq) << CMDQ_CREATE_QP_SQ_PG_SIZE_SFT;
	pg_sz_lvl |= ((sq->hwq.level & CMDQ_CREATE_QP_SQ_LVL_MASK) <<
		      CMDQ_CREATE_QP_SQ_LVL_SFT);
	sq->hwq.pg_sz_lvl = pg_sz_lvl;

	if (qplib_qp->srq)
		goto done;

	hwq_attr.res = res;
	hwq_attr.sginfo = &rq->sginfo;
	hwq_attr.stride = bnxt_qplib_get_stride();
	if (!is_dv_qp)
		hwq_attr.depth = bnxt_qplib_get_depth(rq, qplib_qp->wqe_mode, false);
	else
		hwq_attr.depth = rq->max_wqe * 3;
	hwq_attr.aux_stride = 0;
	hwq_attr.aux_depth = 0;
	hwq_attr.type = HWQ_TYPE_QUEUE;
	rc = bnxt_qplib_alloc_init_hwq(&rq->hwq, &hwq_attr);
	if (rc)
		goto free_sq_hwq;
	pg_sz_lvl = _get_base_pg_size(&rq->hwq) <<
		CMDQ_CREATE_QP_RQ_PG_SIZE_SFT;
	pg_sz_lvl |= ((rq->hwq.level & CMDQ_CREATE_QP_RQ_LVL_MASK) <<
		      CMDQ_CREATE_QP_RQ_LVL_SFT);
	rq->hwq.pg_sz_lvl = pg_sz_lvl;

done:
	if (qplib_qp->psn_sz) {
		rc = bnxt_re_qp_alloc_init_xrrq(qp);
		if (rc)
			goto free_rq_hwq;
	}

	return 0;
free_rq_hwq:
	bnxt_qplib_free_hwq(res, &rq->hwq);
free_sq_hwq:
	bnxt_qplib_free_hwq(res, &sq->hwq);
	return rc;
}

static int bnxt_re_init_qp_attr(struct bnxt_re_qp *qp, struct bnxt_re_pd *pd,
				struct ib_qp_init_attr *init_attr,
				struct ib_udata *udata)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_re_ucontext *cntx = NULL;
	struct ib_ucontext *context;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_cq *cq;
	int rc = 0, qptype;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	dev_attr = rdev->dev_attr;

	if (udata) {
		context = pd->ib_pd.uobject->context;
		cntx = to_bnxt_re(context, struct bnxt_re_ucontext, ib_uctx);
	}

	/* Setup misc params */
	qplqp->is_user = udata ? true : false;
	qplqp->pd_id = pd->qplib_pd.id;
	qplqp->qp_handle = (u64)qplqp;
	qplqp->sig_type = ((init_attr->sq_sig_type == IB_SIGNAL_ALL_WR) ?
			    true : false);
	qptype = bnxt_re_init_qp_type(rdev, init_attr);
	if (qptype < 0)
		return qptype;
	qplqp->type = (u8)qptype;
	qplqp->wqe_mode = bnxt_re_init_qp_wqe_mode(rdev);
	ether_addr_copy(qplqp->smac, rdev->netdev->dev_addr);
	qplqp->dev_cap_flags = dev_attr->dev_cap_flags;
	qplqp->cctx = rdev->chip_ctx;

	if (init_attr->qp_type == IB_QPT_RC) {
		qplqp->max_rd_atomic = dev_attr->max_qp_rd_atom;
		qplqp->max_dest_rd_atomic = dev_attr->max_qp_init_rd_atom;
	}
	qplqp->mtu = ib_mtu_enum_to_int(iboe_get_mtu(rdev->netdev->mtu));
	qplqp->dpi = &rdev->dpi_privileged; /* Doorbell page */

	/* Setup CQs */
	if (init_attr->send_cq) {
		cq = to_bnxt_re(init_attr->send_cq, struct bnxt_re_cq, ib_cq);
		if (!cq) {
			dev_err(rdev_to_dev(rdev), "Send CQ not found");
			return -EINVAL;
		}
		qplqp->scq = &cq->qplib_cq;
		qp->scq = cq;
	}

	if (init_attr->recv_cq) {
		cq = to_bnxt_re(init_attr->recv_cq, struct bnxt_re_cq, ib_cq);
		if (!cq) {
			dev_err(rdev_to_dev(rdev), "Receive CQ not found");
			return -EINVAL;
		}
		qplqp->rcq = &cq->qplib_cq;
		qp->rcq = cq;
	}

	/* Setup RQ/SRQ */
	rc = bnxt_re_init_rq_attr(qp, init_attr, cntx);
	if (rc)
		return rc;
	if (init_attr->qp_type == IB_QPT_GSI)
		bnxt_re_adjust_gsi_rq_attr(qp);

	/* Setup SQ */
	rc = bnxt_re_init_sq_attr(qp, init_attr, cntx);
	if (rc)
		return rc;
	if (init_attr->qp_type == IB_QPT_GSI)
		bnxt_re_adjust_gsi_sq_attr(qp, init_attr, cntx);

	if (udata) { /* This will update DPI and qp_handle */
		rc = bnxt_re_init_user_qp(rdev, pd, qp, udata);
		if (rc)
			return rc;
	}

	bnxt_re_qp_calculate_msn_psn_size(qp);

	rc = bnxt_re_setup_qp_hwqs(qp, false);
	if (rc)
		goto free_umem;

	return 0;
free_umem:
	if (udata)
		bnxt_re_qp_free_umem(qp);
	return rc;
}

static int bnxt_re_create_shadow_gsi(struct bnxt_re_qp *qp,
				     struct bnxt_re_pd *pd)
{
	struct bnxt_re_sqp_entries *sqp_tbl = NULL;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_qp *sqp;
	struct bnxt_re_ah *sah;

	rdev = qp->rdev;
	/* Create a shadow QP to handle the QP1 traffic */
	sqp_tbl = kzalloc(sizeof(*sqp_tbl) * BNXT_RE_MAX_GSI_SQP_ENTRIES,
			  GFP_KERNEL);
	if (!sqp_tbl)
		return -ENOMEM;
	rdev->gsi_ctx.sqp_tbl = sqp_tbl;

	sqp = bnxt_re_create_shadow_qp(pd, &rdev->qplib_res, &qp->qplib_qp);
	if (!sqp) {
		dev_err(rdev_to_dev(rdev),
			"Failed to create Shadow QP for QP1");
		goto out;
	}

	sqp->rcq = qp->rcq;
	sqp->scq = qp->scq;
	sah = bnxt_re_create_shadow_qp_ah(pd, &rdev->qplib_res,
			&qp->qplib_qp);
	if (!sah) {
		bnxt_qplib_destroy_qp(&rdev->qplib_res,
				&sqp->qplib_qp, 0, NULL, NULL);
		dev_err(rdev_to_dev(rdev),
				"Failed to create AH entry for ShadowQP");
		goto out;
	}
	rdev->gsi_ctx.gsi_sqp = sqp;
	rdev->gsi_ctx.gsi_sah = sah;

	return 0;
out:
	kfree(sqp_tbl);
	return -ENODEV;
}

static int __get_rq_hdr_buf_size(u8 gsi_mode)
{
	return (gsi_mode == BNXT_RE_GSI_MODE_ALL) ?
		BNXT_QPLIB_MAX_QP1_RQ_HDR_SIZE_V2 :
		BNXT_QPLIB_MAX_QP1_RQ_HDR_SIZE;
}

static int __get_sq_hdr_buf_size(u8 gsi_mode)
{
	return (gsi_mode != BNXT_RE_GSI_MODE_ROCE_V1) ?
		BNXT_QPLIB_MAX_QP1_SQ_HDR_SIZE_V2 :
		BNXT_QPLIB_MAX_QP1_SQ_HDR_SIZE;
}

static int bnxt_re_create_gsi_qp(struct bnxt_re_qp *qp, struct bnxt_re_pd *pd)
{
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_qplib_res *res;
	struct bnxt_re_dev *rdev;
	u32 sstep, rstep;
	u8 gsi_mode;
	int rc = 0;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	res = &rdev->qplib_res;
	gsi_mode = rdev->gsi_ctx.gsi_qp_mode;
	qplqp->cctx = rdev->chip_ctx;

	rstep = __get_rq_hdr_buf_size(gsi_mode);
	sstep = __get_sq_hdr_buf_size(gsi_mode);
	rc = bnxt_qplib_alloc_hdr_buf(res, qplqp, sstep, rstep);
	if (rc)
		return rc;

	rc = bnxt_qplib_create_qp1(res, qplqp);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "create HW QP1 failed!");
		goto err_free_hdr_buf;
	}

	if (gsi_mode == BNXT_RE_GSI_MODE_ALL) {
		rc = bnxt_re_create_shadow_gsi(qp, pd);
		if (rc)
			goto err_destroy_qp1;
	}

	return 0;
err_destroy_qp1:
	bnxt_qplib_destroy_qp(res, qplqp, 0, NULL, NULL);
err_free_hdr_buf:
	bnxt_qplib_free_hdr_buf(res, qplqp);
	return rc;
}

static bool bnxt_re_qp_validate_limits(struct bnxt_re_dev *rdev,
				       struct ib_qp_init_attr *init_attr)
{
	struct bnxt_qplib_dev_attr *dev_attr = rdev->dev_attr;
	int ilsize;

	ilsize = ALIGN(init_attr->cap.max_inline_data, sizeof(struct sq_sge));

	if (init_attr->cap.max_send_wr > dev_attr->max_sq_wqes) {
		dev_err(rdev_to_dev(rdev),
			"Create QP failed: Requested max_send_wr(%d) exceeding limit(%d)",
			init_attr->cap.max_send_wr, dev_attr->max_sq_wqes);
		return false;
	}

	if (init_attr->cap.max_recv_wr > dev_attr->max_rq_wqes) {
		dev_err(rdev_to_dev(rdev),
			"Create QP failed: Requested max_recv_wr(%d) exceeding limit(%d)",
			init_attr->cap.max_recv_wr, dev_attr->max_rq_wqes);
		return false;
	}

	if (init_attr->cap.max_send_sge > dev_attr->max_qp_sges) {
		dev_err(rdev_to_dev(rdev),
			"Create QP failed: Requested max_send_sge(%d) exceeding limit(%d)",
			init_attr->cap.max_send_sge, dev_attr->max_qp_sges);
		return false;
	}

	if (init_attr->cap.max_recv_sge > dev_attr->max_qp_sges) {
		dev_err(rdev_to_dev(rdev),
			"Create QP failed: Requested max_recv_sge(%d) exceeding limit(%d)",
			init_attr->cap.max_recv_sge, dev_attr->max_qp_sges);
		return false;
	}

	if (ilsize > dev_attr->max_inline_data) {
		dev_err(rdev_to_dev(rdev),
			"Create QP failed: Requested max_inline_data(%d) exceeding limit(%d)",
			init_attr->cap.max_inline_data, dev_attr->max_inline_data);
		return false;
	}
	return true;
}

static int bnxt_re_qp_validate_attr(struct bnxt_re_dev *rdev,
				    struct ib_qp_init_attr *init_attr)
{
	if (init_attr->create_flags) {
		dev_err(rdev_to_dev(rdev), "Unsupported create flags\n");
		return -EOPNOTSUPP;
	}

	switch (init_attr->qp_type) {
	case IB_QPT_GSI:
	case IB_QPT_RC:
	case IB_QPT_UD:
	case IB_QPT_RAW_ETHERTYPE:
	case IB_QPT_RAW_PACKET:
		return 0;
	default:
		dev_err(rdev_to_dev(rdev), "Unsupported qp type\n");
		return -EOPNOTSUPP;
	}
}

static int bnxt_re_add_unique_gid(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ctx *hctx;
	struct bnxt_qplib_res *res;
	int rc;

	if (!rdev->rcfw.roce_mirror)
		return 0;

	res = &rdev->qplib_res;
	hctx = res->hctx;

	rdev->ugid.global.subnet_prefix = cpu_to_be64(0xfe8000000000abcdLL);
	addrconf_ifid_eui48(&rdev->ugid.raw[8], rdev->netdev);

	rc = bnxt_qplib_add_sgid(&rdev->qplib_res.sgid_tbl,
				 (struct bnxt_qplib_gid *)&rdev->ugid,
				 rdev->qplib_res.netdev->dev_addr,
				 0xFFFF, true, &rdev->ugid_index, true,
				 hctx->stats3.fw_id);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to add unique GID. rc = %d\n", rc);

	return rc;
}

static void bnxt_qperr_slot_dump(struct work_struct *work)
{
	struct bnxt_re_qp *qp = container_of(work, struct bnxt_re_qp, dump_slot_task);
	struct bnxt_qplib_qp *qpl = &qp->qplib_qp;
	struct bnxt_re_dev *rdev = qp->rdev;
	struct qdump_array *qdump;
	struct bnxt_re_srq *srq;
	int i, stride, max_wqe;
	u16 cons_idx;
	char *buf;

	qdump = vzalloc(sizeof(*qdump));
	if (!qdump)
		return;

	stride = sizeof(struct sq_sge);
	buf = vzalloc(BNXT_RE_QPERR_DUMP_SLOTS * stride);
	if (!buf) {
		vfree(qdump);
		return;
	}

	if (qp->req_err_state_reason &&
	    (qp->req_err_state_reason !=
	     CREQ_QP_ERROR_NOTIFICATION_REQ_ERR_STATE_REASON_REQ_TIMEOUT_RETRY_LIMIT)) {
		qdump->sqd.rdev = rdev;
		qdump->sqd.umem = qp->sumem;
		qdump->sqd.hwq = &qpl->sq.hwq;
		qdump->sqd.is_user_qp = qpl->is_user;
		qdump->sqd.len = (qpl->sq.max_wqe * qpl->sq.wqe_size);
		qdump->sqd.uaddr_prod = qp->sqprod;
		qdump->sqd.uaddr_cons = qp->sqcons;
		bnxt_re_alloc_qdump_element(&qdump->sqd, sizeof(struct sq_sge),
					    "SendQueue");
		cons_idx = qp->sq_cons_idx;
		for (i = 0; i < BNXT_RE_QPERR_DUMP_SLOTS; i++) {
			memcpy(buf + (i * stride), (qdump->sqd.buf + (cons_idx * stride)), stride);
			cons_idx++;
			cons_idx %= qpl->sq.max_wqe;
		}
		print_hex_dump(KERN_WARNING, "sq_dump: ", DUMP_PREFIX_OFFSET, stride,
			       4, buf, BNXT_RE_QPERR_DUMP_SLOTS * stride, false);
		bnxt_re_free_qpdump(&qdump->sqd);
	}

	if (qp->res_err_state_reason) {
		if (qp->qplib_qp.srq) {
			srq = container_of(qp->qplib_qp.srq, struct bnxt_re_srq, qplib_srq);
			qdump->rqd.rdev = rdev;
			qdump->rqd.umem = srq->umem;
			qdump->rqd.hwq = &((qp->qplib_qp.srq)->hwq);
			qdump->rqd.is_user_qp = qpl->is_user;
			qdump->rqd.len = (qp->qplib_qp.srq->wqe_size * qp->qplib_qp.srq->max_wqe);
			qdump->rqd.uaddr_prod = qp->qplib_qp.srq->srqprod;
			qdump->rqd.uaddr_cons = qp->qplib_qp.srq->srqcons;
			max_wqe = qp->qplib_qp.srq->max_wqe;
			bnxt_re_alloc_qdump_element(&qdump->rqd, sizeof(struct sq_sge),
						    "SharedRecvQueue");
		} else {
			qdump->rqd.rdev = rdev;
			qdump->rqd.umem = qp->rumem;
			qdump->rqd.hwq = &qpl->rq.hwq;
			qdump->rqd.is_user_qp = qpl->is_user;
			qdump->rqd.len = (qpl->rq.max_wqe * qpl->rq.wqe_size);
			qdump->rqd.uaddr_prod = qp->rqprod;
			qdump->rqd.uaddr_cons = qp->rqcons;
			max_wqe = qpl->rq.max_wqe;
			bnxt_re_alloc_qdump_element(&qdump->rqd, sizeof(struct sq_sge),
						    "RecvQueue");
		}
		cons_idx = qp->rq_cons_idx;
		for (i = 0; i < BNXT_RE_QPERR_DUMP_SLOTS; i++) {
			memcpy(buf + (i * stride), (qdump->rqd.buf + (cons_idx * stride)), stride);
			cons_idx++;
			cons_idx %= max_wqe;
		}
		print_hex_dump(KERN_WARNING, "rq_dump: ", DUMP_PREFIX_OFFSET, stride,
			       4, buf, BNXT_RE_QPERR_DUMP_SLOTS * stride, false);
		bnxt_re_free_qpdump(&qdump->rqd);
	}

	vfree(buf);
	vfree(qdump);
}

ALLOC_QP_RET bnxt_re_create_qp(ALLOC_QP_IN *qp_in,
			       struct ib_qp_init_attr *qp_init_attr,
			       struct ib_udata *udata)
{
	enum ib_qp_type qp_type = qp_init_attr->qp_type;
	struct bnxt_re_pd *pd;
#ifdef HAVE_QP_ALLOC_IN_IB_CORE
	struct ib_pd *ib_pd = qp_in->pd;
#else
	struct ib_pd *ib_pd = qp_in;
#endif
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_res *res;
	struct bnxt_re_dev *rdev;
	u32 active_qps, tmp_qps;
	struct bnxt_re_qp *qp;
	int rc;

	pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ib_pd);
	rdev = pd->rdev;
	dev_attr = rdev->dev_attr;
	res = &rdev->qplib_res;

	if (atomic_read(&rdev->stats.rsors.qp_count) >= dev_attr->max_qp) {
		dev_err(rdev_to_dev(rdev), "Create QP failed - max exceeded(QPs Alloc'd %u of max %u)",
			atomic_read(&rdev->stats.rsors.qp_count), dev_attr->max_qp);
		rc = -EINVAL;
		goto exit;
	}

	rc = bnxt_re_qp_validate_attr(rdev, qp_init_attr);
	if (rc)
		goto exit;

	rc = bnxt_re_qp_validate_limits(rdev, qp_init_attr);
	if (!rc) {
		rc = -EINVAL;
		goto exit;
	}
	qp = __get_qp_from_qp_in(qp_in, rdev);
	if (!qp) {
		rc = -ENOMEM;
		goto exit;
	}
	qp->rdev = rdev;

	rc = bnxt_re_init_qp_attr(qp, pd, qp_init_attr, udata);
	if (rc)
		goto fail;

	if (qp_type == IB_QPT_GSI &&
	    !_is_chip_p5_plus(rdev->chip_ctx)) {
		rc = bnxt_re_create_gsi_qp(qp, pd);
		if (rc)
			goto free_hwq;
	} else {
		if (qp_type != IB_QPT_GSI &&
		    bnxt_ext_stats_v2_supported(res->dattr->dev_cap_ext_flags)) {
			qp->qplib_qp.ext_stats_ctx_id = bnxt_qplib_get_stats_ext_ctx_id(res);
			if (qp->qplib_qp.ext_stats_ctx_id == INVALID_STATS_EXT_CTX_ID) {
				rc = -ENOMEM;
				goto fail;
			}
		}

		rc = bnxt_qplib_create_qp(&rdev->qplib_res, &qp->qplib_qp);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "create HW QP failed!");
			goto free_hwq;
		}

		if (udata) {
			struct bnxt_re_qp_resp resp = {};

			if (rdev->hdbr_enabled) {
				rc = bnxt_re_hdbr_db_reg_qp(rdev, qp, pd, &resp);
				if (rc)
					goto reg_db_fail;
			}
			resp.qpid = qp->qplib_qp.id;
			rc = bnxt_re_copy_to_udata(rdev, &resp,
						   min(udata->outlen, sizeof(resp)),
						   udata);
			if (rc)
				goto qp_destroy;
		} else {
			if (rdev->hdbr_enabled) {
				rc = bnxt_re_hdbr_db_reg_qp(rdev, qp, NULL, NULL);
				if (rc)
					goto reg_db_fail;
			}
		}
	}

	/* Support for RawEth QP is added to capture TCP pkt dump.
	 * So unique SGID is used to avoid incorrect statistics on per
	 * function stats_ctx
	 */
	if (qp->qplib_qp.type == CMDQ_CREATE_QP_TYPE_RAW_ETHERTYPE) {
		if (rdev->ugid_index == BNXT_RE_INVALID_UGID_INDEX) {
			rc = bnxt_re_add_unique_gid(rdev);
			if (rc)
				goto qp_destroy;
		}
		qp->qplib_qp.ugid_index = rdev->ugid_index;
	}

	qp->ib_qp.qp_num = qp->qplib_qp.id;
	if (qp_type == IB_QPT_GSI)
		rdev->gsi_ctx.gsi_qp = qp;
	spin_lock_init(&qp->sq_lock);
	spin_lock_init(&qp->rq_lock);
	INIT_LIST_HEAD(&qp->list);
	INIT_LIST_HEAD(&qp->flow_list);
	INIT_WORK(&qp->dump_slot_task, bnxt_qperr_slot_dump);
	mutex_init(&qp->flow_lock);
	mutex_lock(&rdev->qp_lock);
	list_add_tail(&qp->list, &rdev->qp_list);
	mutex_unlock(&rdev->qp_lock);
	atomic_inc(&rdev->stats.rsors.qp_count);
	active_qps = atomic_read(&rdev->stats.rsors.qp_count);
	if (active_qps > atomic_read(&rdev->stats.rsors.max_qp_count))
		atomic_set(&rdev->stats.rsors.max_qp_count, active_qps);
	bnxt_re_qp_info_add_qpinfo(rdev, qp);
	BNXT_RE_RES_LIST_ADD(rdev, qp, BNXT_RE_RES_TYPE_QP);

	bnxt_re_dump_debug_stats(rdev, active_qps);

	/* Get the counters for RC QPs and UD QPs */
	if (qp_type == IB_QPT_RC) {
		tmp_qps = atomic_inc_return(&rdev->stats.rsors.rc_qp_count);
		if (tmp_qps > atomic_read(&rdev->stats.rsors.max_rc_qp_count))
			atomic_set(&rdev->stats.rsors.max_rc_qp_count, tmp_qps);
	} else if (qp_type == IB_QPT_UD) {
		tmp_qps = atomic_inc_return(&rdev->stats.rsors.ud_qp_count);
		if (tmp_qps > atomic_read(&rdev->stats.rsors.max_ud_qp_count))
			atomic_set(&rdev->stats.rsors.max_ud_qp_count, tmp_qps);
	}

	bnxt_re_check_cq_depth(qp);

#ifdef HAVE_QP_ALLOC_IN_IB_CORE
	return 0;
#else
	return &qp->ib_qp;
#endif

qp_destroy:
	if (rdev->hdbr_enabled)
		bnxt_re_hdbr_db_unreg_qp(rdev, qp);
reg_db_fail:
	bnxt_qplib_destroy_qp(&rdev->qplib_res, &qp->qplib_qp, 0, NULL, NULL);
free_hwq:
	bnxt_qplib_free_qp_res(&rdev->qplib_res, &qp->qplib_qp);
	if (udata)
		bnxt_re_qp_free_umem(qp);
fail:
#ifndef HAVE_QP_ALLOC_IN_IB_CORE
	kfree(qp);
#endif
exit:
#ifdef HAVE_QP_ALLOC_IN_IB_CORE
	return rc;
#else
	return ERR_PTR(rc);
#endif
}

static int bnxt_re_modify_shadow_qp(struct bnxt_re_dev *rdev,
			     struct bnxt_re_qp *qp1_qp,
			     int qp_attr_mask)
{
	struct bnxt_re_qp *qp = rdev->gsi_ctx.gsi_sqp;
	unsigned long flags;
	int rc = 0;

	if (qp_attr_mask & IB_QP_STATE) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_STATE;
		qp->qplib_qp.state = qp1_qp->qplib_qp.state;
	}
	if (qp_attr_mask & IB_QP_PKEY_INDEX) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_PKEY;
		qp->qplib_qp.pkey_index = qp1_qp->qplib_qp.pkey_index;
	}

	if (qp_attr_mask & IB_QP_QKEY) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_QKEY;
		/* Using a Random  QKEY */
		qp->qplib_qp.qkey = BNXT_RE_QP_RANDOM_QKEY;
	}
	if (qp_attr_mask & IB_QP_SQ_PSN) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_SQ_PSN;
		qp->qplib_qp.sq.psn = qp1_qp->qplib_qp.sq.psn;
	}

	rc = bnxt_qplib_modify_qp(&rdev->qplib_res, &qp->qplib_qp);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Modify Shadow QP for QP1 failed");
	} else {
		if ((qp_attr_mask & IB_QP_STATE) && (!qp->qplib_qp.srq) &&
		    (qp->qplib_qp.state == CMDQ_MODIFY_QP_NEW_STATE_RTR)) {
			if (qp->qplib_qp.rq.hwq.prod) {
				spin_lock_irqsave(&qp->rq_lock, flags);
				bnxt_qplib_post_recv_db(&qp->qplib_qp);
				spin_unlock_irqrestore(&qp->rq_lock, flags);
			}
		}
	}
	return rc;
}

#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
static u32 ipv4_from_gid(u8 *gid)
{
	return (gid[15] << 24 | gid[14] << 16 | gid[13] << 8 | gid[12]);
}

static u16 get_source_port(struct bnxt_re_dev *rdev,
			   struct bnxt_re_qp *qp)
{
	u8 ip_off, data[48], smac[ETH_ALEN];
	u16 crc = 0, buf_len = 0, i;
	u8 addr_len;
	u32 qpn;

	if (qp->qplib_qp.nw_type == CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV2_IPV6) {
		addr_len = 16;
		ip_off = 0;
	} else {
		addr_len = 4;
		ip_off = 12;
	}

	if (rdev->binfo) {
		memcpy(&smac[2], &qp->qplib_qp.lag_src_mac, 4);
		smac[0] = qp->qplib_qp.smac[0];
		smac[1] = qp->qplib_qp.smac[1];
	} else {
		memcpy(smac, qp->qplib_qp.smac, ETH_ALEN);
	}

	memset(data, 0, 48);
	memcpy(data, qp->qplib_qp.ah.dmac, ETH_ALEN);
	buf_len += ETH_ALEN;

	memcpy(data + buf_len, smac, ETH_ALEN);
	buf_len += ETH_ALEN;

	memcpy(data + buf_len, qp->qplib_qp.ah.dgid.data + ip_off, addr_len);
	buf_len += addr_len;

	memcpy(data + buf_len, qp->qp_info_entry.sgid.raw + ip_off, addr_len);
	buf_len += addr_len;

	qpn = htonl(qp->qplib_qp.dest_qpn);
	memcpy(data + buf_len, (u8 *)&qpn + 1, 3);
	buf_len += 3;

	for (i = 0; i < buf_len; i++)
		crc = crc16(crc, (data + i), 1);

	if (_is_chip_p7_plus(rdev->chip_ctx))
		crc |= 0xc000;

	return crc;
}

static void bnxt_re_update_qp_info(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp)
{
	u16 type;

	type = __from_hw_to_ib_qp_type(qp->qplib_qp.type);

	/* User-space can extract ip address with sgid_index. */
	if (ipv6_addr_v4mapped((struct in6_addr *)&qp->qplib_qp.ah.dgid)) {
		qp->qp_info_entry.s_ip.ipv4_addr = ipv4_from_gid(qp->qp_info_entry.sgid.raw);
		qp->qp_info_entry.d_ip.ipv4_addr = ipv4_from_gid(qp->qplib_qp.ah.dgid.data);
	} else {
		memcpy(&qp->qp_info_entry.s_ip.ipv6_addr, qp->qp_info_entry.sgid.raw,
		       sizeof(qp->qp_info_entry.s_ip.ipv6_addr));
		memcpy(&qp->qp_info_entry.d_ip.ipv6_addr, qp->qplib_qp.ah.dgid.data,
		       sizeof(qp->qp_info_entry.d_ip.ipv6_addr));
	}

	if (((type == IB_QPT_RC) ||
	     (_is_chip_p8(rdev->chip_ctx) &&
	      !BNXT_RE_UDP_SP_WQE(qp->qplib_qp.dev_cap_ext_flags2))) &&
	    (qp->qplib_qp.nw_type == CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV2_IPV4 ||
	     qp->qplib_qp.nw_type == CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV2_IPV6)) {
		qp->qp_info_entry.s_port = get_source_port(rdev, qp);
	}
	qp->qp_info_entry.d_port = BNXT_RE_QP_DEST_PORT;
}
#endif

static void bnxt_qplib_manage_flush_qp(struct bnxt_re_qp *qp)
{
	struct bnxt_qplib_q *rq, *sq;
	struct bnxt_re_dev *rdev;
	unsigned long flags;

	if (qp->sumem)
		return;

	rdev = qp->rdev;

	if (qp->qplib_qp.state == CMDQ_MODIFY_QP_NEW_STATE_ERR) {
		rq = &qp->qplib_qp.rq;
		sq = &qp->qplib_qp.sq;

		dev_dbg(rdev_to_dev(rdev),
			"Move QP = %p to flush list\n", qp);
		flags = bnxt_re_lock_cqs(qp);
		bnxt_qplib_add_flush_qp(&qp->qplib_qp);
		bnxt_re_unlock_cqs(qp, flags);

		if (sq->hwq.prod != sq->hwq.cons)
			bnxt_re_handle_cqn(&qp->scq->qplib_cq);

		if (qp->rcq && (qp->rcq != qp->scq) &&
		    (rq->hwq.prod != rq->hwq.cons))
			bnxt_re_handle_cqn(&qp->rcq->qplib_cq);
	}

	if (qp->qplib_qp.state == CMDQ_MODIFY_QP_NEW_STATE_RESET) {
		dev_dbg(rdev_to_dev(rdev),
			"Move QP = %p out of flush list\n", qp);
		flags = bnxt_re_lock_cqs(qp);
		bnxt_qplib_clean_qp(&qp->qplib_qp);
		bnxt_re_unlock_cqs(qp, flags);
	}
}

static int bnxt_re_update_ah_dscp_sl(struct bnxt_re_qp *qp, struct ib_qp_attr *qp_attr)
{
	struct bnxt_re_dev *rdev = qp->rdev;
	bool dscp_valid = false;
	bool pcp_valid = false;
	u8 cos, dscp_pri = 0;
	u8 dscp;
	u8 i, j;
	u8 pri;

	/*
	 * The traffic class passed by the applications
	 * contains both dscp and ecn values. DSCP is
	 * upper six bits of the traffic class.
	 */
	dscp = qp_attr->ah_attr.grh.traffic_class >> 2;
	pri = rdma_ah_get_sl(&qp_attr->ah_attr);
	qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_VLAN_ID;
	if (!bnxt_qplib_multiple_llq_supported(rdev->chip_ctx)) {
		qp->qplib_qp.ah.traffic_class = dscp;
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_TRAFFIC_CLASS;
		qp->qplib_qp.ah.sl = qp_attr->ah_attr.sl;
		return 0;
	}

	if (!dscp && !pri)
		goto default_queue;

	if (dscp) {
		for (i = 0; i < rdev->d2p_count; i++) {
			if (rdev->d2p[i].dscp == dscp) {
				dscp_pri = rdev->d2p[i].pri;
				cos = rdev->p2cos[dscp_pri];
				for (j = 0; j < rdev->lossless_q_count; j++) {
					if (cos == rdev->lossless_qid[j]) {
						dscp_valid = true;
						break;
					}
				}
			}
		}
		if (dscp_valid) {
			qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_TRAFFIC_CLASS;
			qp->qplib_qp.ah.traffic_class = dscp;
			qp->qplib_qp.ah.sl = dscp_pri;
			return 0;
		}
	}
	if (pri) {
		cos = rdev->p2cos[pri];
		for (j = 0; j < rdev->lossless_q_count; j++) {
			if (cos == rdev->lossless_qid[j]) {
				pcp_valid = true;
				break;
			}
		}
		if (pcp_valid) {
			qp->qplib_qp.ah.sl = qp_attr->ah_attr.sl;
			qp->qplib_qp.ah.traffic_class = 0;
			return 0;
		}
	}
	if (!dscp_valid && !pcp_valid) {
		dev_warn_ratelimited(rdev_to_dev(qp->rdev),
				     "Given DSCP %d and/or SL %d not mapping to lossless queue",
				     dscp, pri);
		dev_warn_ratelimited(rdev_to_dev(qp->rdev),
				     "Changing to default roce traffic class DSCP %d and SL %d",
				     rdev->tc_rec[0].roce_dscp, rdev->tc_rec[0].roce_prio);
	}
default_queue:
	qp->qplib_qp.ah.traffic_class = rdev->tc_rec[0].roce_dscp;
	qp->qplib_qp.ah.sl = rdev->tc_rec[0].roce_prio;
	return 0;
}

int bnxt_re_modify_qp(struct ib_qp *ib_qp, struct ib_qp_attr *qp_attr,
		      int qp_attr_mask, struct ib_udata *udata)
{
	enum ib_qp_state curr_qp_state, new_qp_state;
	struct bnxt_re_modify_qp_ex_resp resp = {};
	struct bnxt_re_modify_qp_ex_req ureq = {};
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_qplib_ppp *ppp = NULL;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_qp *qp;
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
	IB_GID_ATTR *sgid_attr;
#ifndef HAVE_GID_ATTR_IN_IB_AH
	struct ib_gid_attr gid_attr;
#endif  /* HAVE_GID_ATTR_IN_IB_AH */
	int status;
	union ib_gid sgid, *gid_ptr = NULL;
	u8 nw_type;
#endif /* RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP */
	int rc, entries;
	bool is_copy_to_udata = false;
	unsigned long flags;
	bool rate_limit = false;

#ifdef HAVE_IB_QP_ATTR_STANDARD_BITS
	if (qp_attr_mask & ~(IB_QP_ATTR_STANDARD_BITS | IB_QP_RATE_LIMIT))
		return -EOPNOTSUPP;
#endif

	qp = to_bnxt_re(ib_qp, struct bnxt_re_qp, ib_qp);
	rdev = qp->rdev;
	dev_attr = rdev->dev_attr;

	qp->qplib_qp.modify_flags = 0;
	qp->qplib_qp.udcc_exclude = true;
	ppp = &qp->qplib_qp.ppp;
	if (qp_attr_mask & IB_QP_STATE) {
		curr_qp_state = __to_ib_qp_state(qp->qplib_qp.cur_qp_state);
		new_qp_state = qp_attr->qp_state;
		if (qp_attr_mask & IB_QP_RATE_LIMIT) {
			qp_attr_mask &= ~IB_QP_RATE_LIMIT;
			rate_limit = true;
		}
		if (!ib_modify_qp_is_ok_compat(curr_qp_state, new_qp_state,
					       ib_qp->qp_type, qp_attr_mask)) {
			dev_err(rdev_to_dev(rdev),"invalid attribute mask=0x%x"
				" specified for qpn=0x%x of type=0x%x"
				" current_qp_state=0x%x, new_qp_state=0x%x\n",
				qp_attr_mask, ib_qp->qp_num, ib_qp->qp_type,
				curr_qp_state, new_qp_state);
			return -EINVAL;
		}

		if (rdev->qp_rate_limit_cfg)
			goto skip_rl_check;

		/* Rate limit is valid in RTR or RTS state for RC QPT. Check here ! */
		if (rate_limit &&
		    _is_modify_qp_rate_limit_supported(rdev->dev_attr->dev_cap_ext_flags2)) {
			qp_attr_mask |= IB_QP_RATE_LIMIT;
			if (!bnxt_re_modify_qp_is_ok(curr_qp_state, new_qp_state,
						     ib_qp->qp_type, qp_attr_mask)) {
				dev_err(rdev_to_dev(rdev), "rate limit attr check failed."
					" Invalid attribute mask=0x%x"
					" specified for qpn=0x%x of type=0x%x"
					" current_qp_state=0x%x, new_qp_state=0x%x\n",
					qp_attr_mask, ib_qp->qp_num, ib_qp->qp_type,
					curr_qp_state, new_qp_state);
				return -EINVAL;
			}

			/* check rate limit within device limits */
			if ((qp_attr->rate_limit > rdev->dev_attr->rate_limit_max) ||
			    (qp_attr->rate_limit < rdev->dev_attr->rate_limit_min)) {
				dev_err(rdev_to_dev(rdev), "User provided rate_limit not"
					" within device rate limit min %d and max %d values\n",
					rdev->dev_attr->rate_limit_min,
					rdev->dev_attr->rate_limit_max);
				return -EINVAL;
			}
skip_rl_check:
			qp->qplib_qp.ext_modify_flags = 0;
			qp->qplib_qp.ext_modify_flags |=
				CMDQ_MODIFY_QP_EXT_MODIFY_MASK_RATE_LIMIT_VALID;
			qp->qplib_qp.rate_limit = (qp_attr->rate_limit) ? qp_attr->rate_limit :
				rdev->qp_rate_limit_cfg;
		}

		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_STATE;
		qp->qplib_qp.state = __from_ib_qp_state(qp_attr->qp_state);

		if (udata && curr_qp_state == IB_QPS_RESET &&
		    new_qp_state == IB_QPS_INIT) {
			if (!ib_copy_from_udata(&ureq, udata, sizeof(ureq))) {
				if (ureq.comp_mask &
				    BNXT_RE_COMP_MASK_MQP_EX_PPP_REQ_EN_MASK) {
					ppp->req = BNXT_QPLIB_PPP_REQ;
					ppp->dpi = ureq.dpi;
				}
			}
		}
	}
	if (qp_attr_mask & IB_QP_EN_SQD_ASYNC_NOTIFY) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_EN_SQD_ASYNC_NOTIFY;
		qp->qplib_qp.en_sqd_async_notify = true;
	}
	if (qp_attr_mask & IB_QP_ACCESS_FLAGS) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_ACCESS;
		qp->qplib_qp.access =
			__qp_access_flags_from_ib(qp->qplib_qp.cctx,
						  qp_attr->qp_access_flags);
		/* LOCAL_WRITE access must be set to allow RC receive */
		qp->qplib_qp.access |= CMDQ_MODIFY_QP_ACCESS_LOCAL_WRITE;
	}
	if (qp_attr_mask & IB_QP_PKEY_INDEX) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_PKEY;
		qp->qplib_qp.pkey_index = qp_attr->pkey_index;
	}
	if (qp_attr_mask & IB_QP_QKEY) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_QKEY;
		qp->qplib_qp.qkey = qp_attr->qkey;
	}
	if (qp_attr_mask & IB_QP_AV) {
		const struct ib_global_route *grh =
			rdma_ah_read_grh(&qp_attr->ah_attr);

		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_DGID |
				     CMDQ_MODIFY_QP_MODIFY_MASK_FLOW_LABEL |
				     CMDQ_MODIFY_QP_MODIFY_MASK_SGID_INDEX |
				     CMDQ_MODIFY_QP_MODIFY_MASK_HOP_LIMIT |
				     CMDQ_MODIFY_QP_MODIFY_MASK_DEST_MAC;
		memcpy(qp->qplib_qp.ah.dgid.data, qp_attr->ah_attr.grh.dgid.raw,
		       sizeof(qp->qplib_qp.ah.dgid.data));
		qp->qplib_qp.ah.flow_label = grh->flow_label;
		qp->qplib_qp.ah.sgid_index = _get_sgid_index(rdev, grh->sgid_index);
		qp->qplib_qp.ah.host_sgid_index = grh->sgid_index;
		qp->qplib_qp.ah.hop_limit = grh->hop_limit;
		status = bnxt_re_update_ah_dscp_sl(qp, qp_attr);
		if (status)
			return status;
#ifdef HAVE_IB_AH_DMAC
		ether_addr_copy(qp->qplib_qp.ah.dmac, ROCE_DMAC(&qp_attr->ah_attr));
#endif
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
#ifndef HAVE_GID_ATTR_IN_IB_AH
		sgid_attr = &gid_attr;
		status = bnxt_re_get_cached_gid(&rdev->ibdev, 1,
						qp_attr->ah_attr.grh.sgid_index,
						&sgid, &sgid_attr,
						&qp_attr->ah_attr.grh, NULL);
		if (!status)
			dev_put(sgid_attr->ndev);
		gid_ptr = &sgid;
#else
		sgid_attr = qp_attr->ah_attr.grh.sgid_attr;
		gid_ptr =  (union ib_gid *)&sgid_attr->gid;
#endif
		if (sgid_attr->ndev) {
			memcpy(qp->qplib_qp.smac, sgid_attr->ndev->dev_addr,
			       ETH_ALEN);
			nw_type = bnxt_re_gid_to_network_type(sgid_attr, &sgid);
			switch (nw_type) {
			case RDMA_NETWORK_IPV4:
				qp->qplib_qp.nw_type =
					CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV2_IPV4;
				break;
			case RDMA_NETWORK_IPV6:
				qp->qplib_qp.nw_type =
					CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV2_IPV6;
				break;
			default:
				qp->qplib_qp.nw_type =
					CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV1;
				break;
			}
		}
		memcpy(&qp->qp_info_entry.sgid, gid_ptr, sizeof(qp->qp_info_entry.sgid));
#else
		qp->qplib_qp.nw_type =
			CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV1;
#endif
		qp->qplib_qp.udcc_exclude = false;
		if (!rdev->is_virtfn) {
			rc = bnxt_udcc_subnet_check(rdev->en_dev, qp->qplib_qp.ah.dgid.data,
						    qp->qplib_qp.ah.dmac, qp->qplib_qp.smac);
			if (rc)
				qp->qplib_qp.udcc_exclude = true;
			else
				dev_dbg(rdev_to_dev(rdev),
					"qp %#x valid UDCC subnet: addr %pI6 dmac %pM smac %pM\n",
					qp->qplib_qp.id, qp->qplib_qp.ah.dgid.data,
					qp->qplib_qp.ah.dmac, qp->qplib_qp.smac);
		}
	}

	/* MTU settings allowed only during INIT -> RTR */
	if (qp_attr->qp_state == IB_QPS_RTR) {
		rc = bnxt_re_init_qpmtu(qp, rdev->netdev->mtu, qp_attr_mask, qp_attr);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "qp %#x invalid mtu %d\n",
				qp->qplib_qp.id, ib_mtu_enum_to_int(qp_attr->path_mtu));
			return rc;
		}

		if (udata && !ib_copy_from_udata(&ureq, udata, sizeof(ureq))) {
			if (ureq.comp_mask & BNXT_RE_COMP_MASK_MQP_EX_PATH_MTU_MASK) {
				resp.comp_mask |= BNXT_RE_COMP_MASK_MQP_EX_PATH_MTU_MASK;
				resp.path_mtu = qp->qplib_qp.mtu;
				is_copy_to_udata = true;
			}
		}
	}

	if (qp_attr_mask & IB_QP_TIMEOUT) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_TIMEOUT;
		qp->qplib_qp.timeout = qp_attr->timeout;
	}
	if (qp_attr_mask & IB_QP_RETRY_CNT) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_RETRY_CNT;
		qp->qplib_qp.retry_cnt = qp_attr->retry_cnt;
	}
	if (qp_attr_mask & IB_QP_RNR_RETRY) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_RNR_RETRY;
		qp->qplib_qp.rnr_retry = qp_attr->rnr_retry;
	}
	if (qp_attr_mask & IB_QP_MIN_RNR_TIMER) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_MIN_RNR_TIMER;
		qp->qplib_qp.min_rnr_timer = qp_attr->min_rnr_timer;
	}
	if (qp_attr_mask & IB_QP_RQ_PSN) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_RQ_PSN;
		qp->qplib_qp.rq.psn = qp_attr->rq_psn;
	}
	if (qp_attr_mask & IB_QP_MAX_QP_RD_ATOMIC) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_MAX_RD_ATOMIC;
		/* Cap the max_rd_atomic to device max */
		if (qp_attr->max_rd_atomic > dev_attr->max_qp_rd_atom)
			dev_dbg(rdev_to_dev(rdev),
				"max_rd_atomic requested %d is > device max %d\n",
				qp_attr->max_rd_atomic,
				dev_attr->max_qp_rd_atom);
		qp->qplib_qp.max_rd_atomic = min_t(u32, qp_attr->max_rd_atomic,
						   dev_attr->max_qp_rd_atom);
	}
	if (qp_attr_mask & IB_QP_SQ_PSN) {
		qp->qplib_qp.modify_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_SQ_PSN;
		qp->qplib_qp.sq.psn = qp_attr->sq_psn;
	}
	if (qp_attr_mask & IB_QP_MAX_DEST_RD_ATOMIC) {
		if (qp_attr->max_dest_rd_atomic >
		    dev_attr->max_qp_init_rd_atom) {
			dev_err(rdev_to_dev(rdev),
				"max_dest_rd_atomic requested %d is > device max %d\n",
				qp_attr->max_dest_rd_atomic,
				dev_attr->max_qp_init_rd_atom);
			return -EINVAL;
		}
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_MAX_DEST_RD_ATOMIC;
		qp->qplib_qp.max_dest_rd_atomic = qp_attr->max_dest_rd_atomic;
	}
	if (qp_attr_mask & IB_QP_CAP) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_SQ_SIZE |
				CMDQ_MODIFY_QP_MODIFY_MASK_RQ_SIZE |
				CMDQ_MODIFY_QP_MODIFY_MASK_SQ_SGE |
				CMDQ_MODIFY_QP_MODIFY_MASK_RQ_SGE |
				CMDQ_MODIFY_QP_MODIFY_MASK_MAX_INLINE_DATA;
		if ((qp_attr->cap.max_send_wr >= dev_attr->max_sq_wqes) ||
		    (qp_attr->cap.max_recv_wr >= dev_attr->max_rq_wqes) ||
		    (qp_attr->cap.max_send_sge >= dev_attr->max_qp_sges) ||
		    (qp_attr->cap.max_recv_sge >= dev_attr->max_qp_sges) ||
		    (qp_attr->cap.max_inline_data >=
						dev_attr->max_inline_data)) {
			dev_err(rdev_to_dev(rdev),
				"Create QP failed - max exceeded");
			return -EINVAL;
		}
		entries = roundup_pow_of_two(qp_attr->cap.max_send_wr);
		if (entries > dev_attr->max_sq_wqes)
			entries = dev_attr->max_sq_wqes;
		entries = min_t(u32, entries, dev_attr->max_sq_wqes);
		qp->qplib_qp.sq.max_wqe = entries;
		qp->qplib_qp.sq.q_full_delta = qp->qplib_qp.sq.max_wqe -
						qp_attr->cap.max_send_wr;
		/*
 		 * Reserving one slot for Phantom WQE. Some application can
 		 * post one extra entry in this case. Allowing this to avoid
 		 * unexpected Queue full condition
 		 */
		qp->qplib_qp.sq.q_full_delta -= 1;
		qp->qplib_qp.sq.max_sge = qp_attr->cap.max_send_sge;
		if (qp->qplib_qp.rq.max_wqe) {
			entries = roundup_pow_of_two(qp_attr->cap.max_recv_wr);
			if (entries > dev_attr->max_rq_wqes)
				entries = dev_attr->max_rq_wqes;
			qp->qplib_qp.rq.max_wqe = entries;
			qp->qplib_qp.rq.q_full_delta = qp->qplib_qp.rq.max_wqe -
						       qp_attr->cap.max_recv_wr;
			qp->qplib_qp.rq.max_sge = qp_attr->cap.max_recv_sge;
		} else {
			/* SRQ was used prior, just ignore the RQ caps */
		}
	}
	if (qp_attr_mask & IB_QP_DEST_QPN) {
		qp->qplib_qp.modify_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_DEST_QP_ID;
		qp->qplib_qp.dest_qpn = qp_attr->dest_qp_num;
	}

	rc = bnxt_qplib_modify_qp(&rdev->qplib_res, &qp->qplib_qp);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Modify HW QP failed!");
		return rc;
	}
	if (qp_attr_mask & IB_QP_STATE) {
		/*
		 * When the QP moves to INIT to RTR in the modify_qp
		 * call, firmware has a workaround to update the context
		 * field with CDUDMA read/write. During the QP INIT
		 * state in the post_receive driver needs to ensure
		 * no doorbell is rung to work this properly. And once
		 * QP moves to RTR ring the doorbell if the producer
		 * index is present.
		 */
		if ((qp_attr->qp_state == IB_QPS_RTR) && (!qp->qplib_qp.srq)) {
			if (qp->qplib_qp.rq.hwq.prod) {
				spin_lock_irqsave(&qp->rq_lock, flags);
				bnxt_qplib_post_recv_db(&qp->qplib_qp);
				spin_unlock_irqrestore(&qp->rq_lock, flags);
			}
		}
		bnxt_qplib_manage_flush_qp(qp);
	}
	if (ureq.comp_mask & BNXT_RE_COMP_MASK_MQP_EX_PPP_REQ_EN_MASK &&
	    ppp->st_idx_en & CREQ_MODIFY_QP_RESP_PINGPONG_PUSH_ENABLED) {
		resp.comp_mask |= BNXT_RE_COMP_MASK_MQP_EX_PPP_REQ_EN;
		resp.ppp_st_idx = ppp->st_idx_en >>
				  BNXT_QPLIB_PPP_ST_IDX_SHIFT;
		is_copy_to_udata = true;
		rdev->ppp_stats.ppp_enabled_qps++;
	}

	if (is_copy_to_udata) {
		rc = bnxt_re_copy_to_udata(rdev, &resp,
					   min(udata->outlen, sizeof(resp)),
					   udata);
		if (rc)
			return rc;
	}

	if (ib_qp->qp_type == IB_QPT_GSI &&
	    rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_ALL &&
	    rdev->gsi_ctx.gsi_sqp)
		rc = bnxt_re_modify_shadow_qp(rdev, qp, qp_attr_mask);
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
	/*
	 * Update info when qp_info_info
	 */
	bnxt_re_update_qp_info(rdev, qp);
#endif
#ifdef POST_QP1_DUMMY_WQE
	if (!(_is_chip_p5_plus(rdev->chip_ctx)) &&
	    ib_qp->qp_type == IB_QPT_GSI &&
	    qp_attr->qp_state == IB_QPS_RTS) {
		/* To suppress the WQE completion,
		 * temporarily change the sig_type of QP to 0.
		 * WQE completion is issued based on this flag
		 * inside qplib_post_send. Restore sig_type
		 * once posting is done.
		 */
		u8 tmp_sig_type = qp->qplib_qp.sig_type;
		qp->qplib_qp.sig_type  = 0;
		dev_dbg(rdev_to_dev(rdev), "posting dummy wqe");
		post_qp1_dummy_wqe(qp);
		qp->qplib_qp.sig_type = tmp_sig_type;
	}
#endif /* POST_QP1_DUMMY_WQE */
	return rc;
}

int bnxt_re_query_qp_attr(struct bnxt_re_qp *qp, struct ib_qp_attr *qp_attr,
			  int attr_mask, struct ib_qp_init_attr *qp_init_attr)
{
	struct bnxt_re_dev *rdev = qp->rdev;
	struct bnxt_qplib_qp *qplib_qp;
	int rc;

	qplib_qp = kcalloc(1, sizeof(*qplib_qp), GFP_KERNEL);
	if (!qplib_qp)
		return -ENOMEM;

	qplib_qp->id = qp->qplib_qp.id;
	qplib_qp->ah.host_sgid_index = qp->qplib_qp.ah.host_sgid_index;

	rc = bnxt_qplib_query_qp(&rdev->qplib_res, qplib_qp);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Query HW QP (0x%x) failed! rc = %d",
			qplib_qp->id, rc);
		goto free_mem;
	}
	qp_attr->qp_state = __to_ib_qp_state(qplib_qp->state);
	qp_attr->cur_qp_state = __to_ib_qp_state(qplib_qp->cur_qp_state);
	qp_attr->en_sqd_async_notify = qplib_qp->en_sqd_async_notify ? 1 : 0;
	qp_attr->qp_access_flags = __qp_access_flags_to_ib(qp->qplib_qp.cctx,
							   qplib_qp->access);
	qp_attr->pkey_index = qplib_qp->pkey_index;
	qp_attr->qkey = qplib_qp->qkey;
#ifdef HAVE_ROCE_AH_ATTR
	qp_attr->ah_attr.type = RDMA_AH_ATTR_TYPE_ROCE;
#endif
	memcpy(qp_attr->ah_attr.grh.dgid.raw, qplib_qp->ah.dgid.data,
	       sizeof(qplib_qp->ah.dgid.data));
	qp_attr->ah_attr.grh.flow_label = qplib_qp->udp_sport;
	qp_attr->ah_attr.grh.sgid_index = qplib_qp->ah.host_sgid_index;
	qp_attr->ah_attr.grh.hop_limit = qplib_qp->ah.hop_limit;
	qp_attr->ah_attr.grh.traffic_class = qplib_qp->ah.traffic_class;
	qp_attr->ah_attr.sl = qplib_qp->ah.sl;
#ifdef HAVE_IB_AH_DMAC
	ether_addr_copy(ROCE_DMAC(&qp_attr->ah_attr), qplib_qp->ah.dmac);
#endif
	qp_attr->path_mtu = __to_ib_mtu(qplib_qp->path_mtu);
	qp_attr->timeout = qplib_qp->timeout;
	qp_attr->retry_cnt = qplib_qp->retry_cnt;
	qp_attr->rnr_retry = qplib_qp->rnr_retry;
	qp_attr->min_rnr_timer = qplib_qp->min_rnr_timer;
	qp_attr->port_num = __to_ib_port_num(qplib_qp->port_id);
	qp_attr->rq_psn = qplib_qp->rq.psn;
	qp_attr->max_rd_atomic = qplib_qp->max_rd_atomic;
	qp_attr->sq_psn = qplib_qp->sq.psn;
	qp_attr->max_dest_rd_atomic = qplib_qp->max_dest_rd_atomic;
	qp_init_attr->sq_sig_type = qplib_qp->sig_type ? IB_SIGNAL_ALL_WR :
							IB_SIGNAL_REQ_WR;
	qp_attr->dest_qp_num = qplib_qp->dest_qpn;

	qp_attr->cap.max_send_wr = qp->qplib_qp.sq.max_wqe;
	qp_attr->cap.max_send_sge = qp->qplib_qp.sq.max_sge;
	qp_attr->cap.max_recv_wr = qp->qplib_qp.rq.max_wqe;
	qp_attr->cap.max_recv_sge = qp->qplib_qp.rq.max_sge;
	qp_attr->cap.max_inline_data = qplib_qp->max_inline_data;
	qp_init_attr->cap = qp_attr->cap;

free_mem:
	kfree(qplib_qp);
	return rc;
}

int bnxt_re_query_qp(struct ib_qp *ib_qp, struct ib_qp_attr *qp_attr,
		     int qp_attr_mask, struct ib_qp_init_attr *qp_init_attr)
{
	struct bnxt_re_qp *qp = to_bnxt_re(ib_qp, struct bnxt_re_qp, ib_qp);

	/* Not all of output fields are applicable, make sure to zero them */
	memset(qp_init_attr, 0, sizeof(*qp_init_attr));
	memset(qp_attr, 0, sizeof(*qp_attr));

	return bnxt_re_query_qp_attr(qp, qp_attr, qp_attr_mask, qp_init_attr);
}

/* For Raw, the application is responsible to build the entire packet */
static void bnxt_re_build_raw_send(CONST_STRUCT ib_send_wr *wr,
				   struct bnxt_qplib_swqe *wqe)
{
	switch (wr->send_flags) {
	case IB_SEND_IP_CSUM:
		wqe->rawqp1.lflags |= SQ_SEND_RAWETH_QP1_LFLAGS_IP_CHKSUM;
		break;
	default:
		/* Pad HW RoCE iCRC */
		wqe->rawqp1.lflags |= SQ_SEND_RAWETH_QP1_LFLAGS_ROCE_CRC;
		break;
	}
}

/* For QP1, the driver must build the entire RoCE (v1/v2) packet hdr
   as according to the sgid and AV
 */
static int bnxt_re_build_qp1_send(struct bnxt_re_qp *qp, CONST_STRUCT ib_send_wr *wr,
				  struct bnxt_qplib_swqe *wqe, int payload_size)
{
#ifdef HAVE_IB_RDMA_WR
	struct bnxt_re_ah *ah = to_bnxt_re(ud_wr(wr)->ah, struct bnxt_re_ah,
					   ib_ah);
#else
	struct bnxt_re_ah *ah = to_bnxt_re(wr->wr.ud.ah, struct bnxt_re_ah,
					   ib_ah);
#endif
	struct bnxt_qplib_ah *qplib_ah = &ah->qplib_ah;
	struct bnxt_qplib_sge sge;
	int i, rc = 0, size;
	union ib_gid sgid;
	u16 vlan_id;
	u8 *ptmac;
	void *buf;

	memset(&qp->qp1_hdr, 0, sizeof(qp->qp1_hdr));

	rc = bnxt_re_query_gid(&qp->rdev->ibdev, 1, qplib_ah->sgid_index, &sgid);
	if (rc)
		return rc;

	qp->qp1_hdr.eth_present = 1;
	ptmac = ah->qplib_ah.dmac;
	memcpy(qp->qp1_hdr.eth.dmac_h, ptmac, 4);
	ptmac += 4;
	memcpy(qp->qp1_hdr.eth.dmac_l, ptmac, 2);

	ptmac = qp->qplib_qp.smac;
	memcpy(qp->qp1_hdr.eth.smac_h, ptmac, 2);
	ptmac += 2;
	memcpy(qp->qp1_hdr.eth.smac_l, ptmac, 4);

	qp->qp1_hdr.eth.type = cpu_to_be16(BNXT_QPLIB_ETHTYPE_ROCEV1);

	/* For vlan, check the sgid for vlan existence */
	vlan_id = rdma_get_vlan_id(&sgid);
	if (vlan_id && vlan_id < 0x1000) {
		qp->qp1_hdr.vlan_present = 1;
		qp->qp1_hdr.eth.type = cpu_to_be16(ETH_P_8021Q);
	}
	/* GRH */
	qp->qp1_hdr.grh_present = 1;
	qp->qp1_hdr.grh.ip_version = 6;
	qp->qp1_hdr.grh.payload_length =
		cpu_to_be16((IB_BTH_BYTES + IB_DETH_BYTES + payload_size + 7)
			    & ~3);
	qp->qp1_hdr.grh.next_header = 0x1b;
	memcpy(qp->qp1_hdr.grh.source_gid.raw, sgid.raw, sizeof(sgid));
	memcpy(qp->qp1_hdr.grh.destination_gid.raw, qplib_ah->dgid.data,
	       sizeof(sgid));

	/* BTH */
	if (wr->opcode == IB_WR_SEND_WITH_IMM) {
		qp->qp1_hdr.bth.opcode = IB_OPCODE_UD_SEND_ONLY_WITH_IMMEDIATE;
		qp->qp1_hdr.immediate_present = 1;
	} else {
		qp->qp1_hdr.bth.opcode = IB_OPCODE_UD_SEND_ONLY;
	}
	if (wr->send_flags & IB_SEND_SOLICITED)
		qp->qp1_hdr.bth.solicited_event = 1;
	qp->qp1_hdr.bth.pad_count = (4 - payload_size) & 3;
	/* P_key for QP1 is for all members */
	qp->qp1_hdr.bth.pkey = cpu_to_be16(0xFFFF);
	qp->qp1_hdr.bth.destination_qpn = IB_QP1;
	qp->qp1_hdr.bth.ack_req = 0;
	qp->send_psn++;
	qp->send_psn &= BTH_PSN_MASK;
	qp->qp1_hdr.bth.psn = cpu_to_be32(qp->send_psn);
	/* DETH */
	/* Use the privileged Q_Key for QP1 */
	qp->qp1_hdr.deth.qkey = cpu_to_be32(IB_QP1_QKEY);
	qp->qp1_hdr.deth.source_qpn = IB_QP1;

	/* Pack the QP1 to the transmit buffer */
	buf = bnxt_qplib_get_qp1_sq_buf(&qp->qplib_qp, &sge);
	if (!buf) {
		dev_err(rdev_to_dev(qp->rdev), "QP1 buffer is empty!");
		return -ENOMEM;
	}
	size = ib_ud_header_pack(&qp->qp1_hdr, buf);
	for (i = wqe->num_sge; i; i--) {
		wqe->sg_list[i].addr = wqe->sg_list[i - 1].addr;
		wqe->sg_list[i].lkey = wqe->sg_list[i - 1].lkey;
		wqe->sg_list[i].size = wqe->sg_list[i - 1].size;
	}
	wqe->sg_list[0].addr = sge.addr;
	wqe->sg_list[0].lkey = sge.lkey;
	wqe->sg_list[0].size = sge.size;
	wqe->num_sge++;

	return rc;
}

#ifdef ENABLE_ROCEV2_QP1
/* Routine for sending QP1 packets for RoCE V1 and V2
 */
static int bnxt_re_build_qp1_send_v2(struct bnxt_re_qp *qp,
				     CONST_STRUCT ib_send_wr *wr,
				     struct bnxt_qplib_swqe *wqe,
				     int payload_size)
{
	struct bnxt_re_dev *rdev = qp->rdev;
#ifdef HAVE_IB_RDMA_WR
	struct bnxt_re_ah *ah = to_bnxt_re(ud_wr(wr)->ah, struct bnxt_re_ah,
					   ib_ah);
#else
	struct bnxt_re_ah *ah = to_bnxt_re(wr->wr.ud.ah, struct bnxt_re_ah,
					   ib_ah);
#endif
	struct bnxt_qplib_ah *qplib_ah = &ah->qplib_ah;
	struct bnxt_qplib_sge sge;
	u8 nw_type;
	u16 ether_type;
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
	IB_GID_ATTR *sgid_attr;
	union ib_gid *psgid;
#ifndef HAVE_GID_ATTR_IN_IB_AH
	struct ib_device *ibdev = &qp->rdev->ibdev;
	IB_GID_ATTR gid_attr;
	union ib_gid sgid;

#endif /* HAVE_GID_ATTR_IN_IB_AH */
#endif /* RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP */
	union ib_gid dgid;
	bool is_eth = false;
	bool is_vlan = false;
	bool is_grh = false;
	bool is_udp = false;
	u8 ip_version = 0, gsi_mode;
	u16 vlan_id = 0xFFFF;
	void *buf;
	int i, rc = 0, size;
	unsigned int dscp;
	uint8_t *ip_hdr;

	memset(&qp->qp1_hdr, 0, sizeof(qp->qp1_hdr));

#ifndef HAVE_GID_ATTR_IN_IB_AH
	sgid_attr = &gid_attr;
	psgid = &sgid;
	rc = bnxt_re_get_cached_gid(ibdev, 1, qplib_ah->host_sgid_index, &sgid,
				    &sgid_attr, NULL, &ah->ib_ah);
	if (rc) {
		dev_err(rdev_to_dev(qp->rdev),
			"Failed to query gid at index %d",
			qplib_ah->host_sgid_index);
		return rc;
	}

	if (sgid_attr->ndev) {
		if (is_vlan_dev(sgid_attr->ndev))
			vlan_id = vlan_dev_vlan_id(sgid_attr->ndev);
		dev_put(sgid_attr->ndev);
	}
#else
	sgid_attr = ah->ib_ah.sgid_attr;
	psgid = (union ib_gid *)&sgid_attr->gid;
	if ((sgid_attr->ndev) && is_vlan_dev(sgid_attr->ndev))
		vlan_id = vlan_dev_vlan_id(sgid_attr->ndev);
#endif

	nw_type = bnxt_re_gid_to_network_type(sgid_attr, psgid);
	gsi_mode = rdev->gsi_ctx.gsi_qp_mode;
	switch (nw_type) {
	case RDMA_NETWORK_IPV4:
		if (gsi_mode != BNXT_RE_GSI_MODE_ALL &&
		    gsi_mode != BNXT_RE_GSI_MODE_ROCE_V2_IPV4) {
			rc = -EINVAL;
			goto done;
		}
		nw_type = BNXT_RE_ROCEV2_IPV4_PACKET;
		break;
	case RDMA_NETWORK_IPV6:
		if (gsi_mode != BNXT_RE_GSI_MODE_ALL &&
		    gsi_mode != BNXT_RE_GSI_MODE_ROCE_V2_IPV6) {
			rc = -EINVAL;
			goto done;
		}
		nw_type = BNXT_RE_ROCEV2_IPV6_PACKET;
		break;
	default:
		if (gsi_mode != BNXT_RE_GSI_MODE_ALL) {
			rc = -EINVAL;
			goto done;
		}
		nw_type = BNXT_RE_ROCE_V1_PACKET;
		break;
	}
	memcpy(&dgid.raw, &qplib_ah->dgid, 16);
	is_udp = sgid_attr->gid_type == IB_GID_TYPE_ROCE_UDP_ENCAP;
	if (is_udp) {
		if (ipv6_addr_v4mapped((struct in6_addr *)psgid)) {
			ip_version = 4;
			ether_type = ETH_P_IP;
		} else {
			ip_version = 6;
			ether_type = ETH_P_IPV6;
		}
		is_grh = false;
	} else {
		ether_type = BNXT_QPLIB_ETHTYPE_ROCEV1;
		is_grh = true;
	}

	is_eth = true;
	is_vlan = (vlan_id && (vlan_id < 0x1000)) ? true : false;

	dev_dbg(rdev_to_dev(qp->rdev),
		 "eth = %d grh = %d udp = %d vlan = %d ip_ver = %d\n",
		 is_eth, is_grh, is_udp, is_vlan, ip_version);

	ib_ud_header_init(payload_size, !is_eth, is_eth, is_vlan, is_grh,
			  ip_version, is_udp, 0, &qp->qp1_hdr);

	ether_addr_copy(qp->qp1_hdr.eth.dmac_h, ah->qplib_ah.dmac);
	ether_addr_copy(qp->qp1_hdr.eth.smac_h, qp->qplib_qp.smac);

	if (!is_vlan) {
		qp->qp1_hdr.eth.type = cpu_to_be16(ether_type);
	} else {
		qp->qp1_hdr.vlan.type = cpu_to_be16(ether_type);
		qp->qp1_hdr.vlan.tag = cpu_to_be16(vlan_id);
	}

	if (is_grh || (ip_version == 6)) {
		memcpy(qp->qp1_hdr.grh.source_gid.raw, psgid->raw, sizeof(*psgid));
		memcpy(qp->qp1_hdr.grh.destination_gid.raw, qplib_ah->dgid.data,
		       sizeof(*psgid));
		qp->qp1_hdr.grh.hop_limit     = qplib_ah->hop_limit;
	}

	if (ip_version == 4) {
		/* TODO */
		qp->qp1_hdr.ip4.tos = 0;
		qp->qp1_hdr.ip4.id = 0;
		qp->qp1_hdr.ip4.frag_off = htons(IP_DF);
		qp->qp1_hdr.ip4.ttl = qplib_ah->hop_limit;

		memcpy(&qp->qp1_hdr.ip4.saddr, psgid->raw + 12, 4);
		memcpy(&qp->qp1_hdr.ip4.daddr, qplib_ah->dgid.data + 12, 4);
		qp->qp1_hdr.ip4.check = ib_ud_ip4_csum(&qp->qp1_hdr);
	}

	if (is_udp) {
		qp->qp1_hdr.udp.dport = htons(ROCE_V2_UDP_DPORT);
		qp->qp1_hdr.udp.sport = htons(BNXT_RE_ROCE_V2_UDP_SPORT);
		qp->qp1_hdr.udp.csum = 0;
	}

	/* BTH */
	if (wr->opcode == IB_WR_SEND_WITH_IMM) {
		qp->qp1_hdr.bth.opcode = IB_OPCODE_UD_SEND_ONLY_WITH_IMMEDIATE;
		qp->qp1_hdr.immediate_present = 1;
	} else {
		qp->qp1_hdr.bth.opcode = IB_OPCODE_UD_SEND_ONLY;
	}
	if (wr->send_flags & IB_SEND_SOLICITED)
		qp->qp1_hdr.bth.solicited_event = 1;
	/* pad_count */
	qp->qp1_hdr.bth.pad_count = (4 - payload_size) & 3;

	/* P_key for QP1 is for all members */
	qp->qp1_hdr.bth.pkey = cpu_to_be16(0xFFFF);
	qp->qp1_hdr.bth.destination_qpn = IB_QP1;
	qp->qp1_hdr.bth.ack_req = 0;
	qp->send_psn++;
	qp->send_psn &= BTH_PSN_MASK;
	qp->qp1_hdr.bth.psn = cpu_to_be32(qp->send_psn);
	/* DETH */
	/* Use the privileged Q_Key for QP1 */
	qp->qp1_hdr.deth.qkey = cpu_to_be32(IB_QP1_QKEY);
	qp->qp1_hdr.deth.source_qpn = IB_QP1;

	/* Pack the QP1 to the transmit buffer */
	buf = bnxt_qplib_get_qp1_sq_buf(&qp->qplib_qp, &sge);
	if (!buf) {
		dev_err(rdev_to_dev(qp->rdev), "QP1 buffer is empty!");
		rc = -ENOMEM;
		goto done;
	}
	size = ib_ud_header_pack(&qp->qp1_hdr, buf);
	for (i = wqe->num_sge; i; i--) {
		wqe->sg_list[i].addr = wqe->sg_list[i - 1].addr;
		wqe->sg_list[i].lkey = wqe->sg_list[i - 1].lkey;
		wqe->sg_list[i].size = wqe->sg_list[i - 1].size;
	}
	dscp = (qp->rdev->cc_param.tos_dscp <<  2) |
		qp->rdev->cc_param.tos_ecn;
	/* Fill dscp values on this raw ethernet packet */
	if (dscp) {
		u8 len = is_vlan ? VLAN_ETH_HLEN : ETH_HLEN;
		ip_hdr = (u8 *) buf + len;
		if (ip_version == 4)
			ipv4_copy_dscp(dscp, (struct iphdr *)ip_hdr);
		else
			ipv6_copy_dscp(dscp, (struct ipv6hdr *)ip_hdr);
	}
	/*
	 * Max Header buf size for IPV6 RoCE V2 is 86,
	 * which is same as the QP1 SQ header buffer.
	 * Header buf size for IPV4 RoCE V2 can be 66.
	 * ETH(14) + VLAN(4)+ IP(20) + UDP (8) + BTH(20).
	 * Subtract 20 bytes from QP1 SQ header buf size
	 */
	if (is_udp && ip_version == 4)
		sge.size -= 20;
	/*
	 * Max Header buf size for RoCE V1 is 78.
	 * ETH(14) + VLAN(4) + GRH(40) + BTH(20).
	 * Subtract 8 bytes from QP1 SQ header buf size
	 */
	if (!is_udp)
		sge.size -= 8;
	/* Subtract 4 bytes for non vlan packets */
	if (!is_vlan)
		sge.size -= 4;
	wqe->sg_list[0].addr = sge.addr;
	wqe->sg_list[0].lkey = sge.lkey;
	wqe->sg_list[0].size = sge.size;
	wqe->num_sge++;
done:
	return rc;
}
#endif

static int bnxt_re_build_gsi_send(struct bnxt_re_qp *qp,
				  CONST_STRUCT ib_send_wr *wr,
				  struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_re_dev *rdev;
	int rc, indx, len = 0;

	rdev = qp->rdev;

	/* Mode UD is applicable to Gen P5 only */
	if (rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_UD)
		return 0;

	for (indx = 0; indx < wr->num_sge; indx++) {
		wqe->sg_list[indx].addr = wr->sg_list[indx].addr;
		wqe->sg_list[indx].lkey = wr->sg_list[indx].lkey;
		wqe->sg_list[indx].size = wr->sg_list[indx].length;
		len += wr->sg_list[indx].length;
	}
#ifdef ENABLE_ROCEV2_QP1
	if (rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_ROCE_V1)
		rc = bnxt_re_build_qp1_send_v2(qp, wr, wqe, len);
	else
		rc = bnxt_re_build_qp1_send(qp, wr, wqe, len);
#else
		rc = bnxt_re_build_qp1_send(qp, wr, wqe, len);
#endif
	wqe->rawqp1.lflags |= SQ_SEND_RAWETH_QP1_LFLAGS_ROCE_CRC;

	return rc;
}

/* For the MAD layer, it only provides the recv SGE the size of
   ib_grh + MAD datagram.  No Ethernet headers, Ethertype, BTH, DETH,
   nor RoCE iCRC.  The Cu+ solution must provide buffer for the entire
   receive packet (334 bytes) with no VLAN and then copy the GRH
   and the MAD datagram out to the provided SGE.
*/

static int bnxt_re_build_qp1_recv(struct bnxt_re_qp *qp,
				  CONST_STRUCT ib_recv_wr *wr,
				  struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_re_dev *rdev = qp->rdev;
	struct bnxt_qplib_sge ref, sge;
	u8 udp_hdr_size = 0;
	u8 ip_hdr_size = 0;
	int size;

	if (bnxt_qplib_get_qp1_rq_buf(&qp->qplib_qp, &sge)) {
		/* Create 5 SGEs as according to the following:
		 * Ethernet header (14)
		 * ib_grh (40) - as provided from the wr
		 * ib_bth + ib_deth + UDP(RoCE v2 only)  (28)
		 * MAD (256) - as provided from the wr
		 * iCRC (4)
		 */

		/* Set RoCE v2 header size and offsets */
		if (rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_ROCE_V2_IPV4)
			ip_hdr_size = 20;
		if (rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_ROCE_V1)
			udp_hdr_size = 8;

		/* Save the reference from ULP */
		ref.addr = wr->sg_list[0].addr;
		ref.lkey = wr->sg_list[0].lkey;
		ref.size = wr->sg_list[0].length;

		/* SGE 1 */
		size = sge.size;
		wqe->sg_list[0].addr = sge.addr;
		wqe->sg_list[0].lkey = sge.lkey;
		wqe->sg_list[0].size = BNXT_QPLIB_MAX_QP1_RQ_ETH_HDR_SIZE;
		size -= wqe->sg_list[0].size;
		if (size <= 0) {
			dev_err(rdev_to_dev(qp->rdev),"QP1 rq buffer is empty!");
			return -ENOMEM;
		}
		sge.size = (u32)size;
		sge.addr += wqe->sg_list[0].size;

		/* SGE 2 */
		/* In case of RoCE v2 ipv4 lower 20 bytes should have IP hdr */
		wqe->sg_list[1].addr = ref.addr + ip_hdr_size;
		wqe->sg_list[1].lkey = ref.lkey;
		wqe->sg_list[1].size = sizeof(struct ib_grh) - ip_hdr_size;
		ref.size -= wqe->sg_list[1].size;
		if (ref.size <= 0) {
			dev_err(rdev_to_dev(qp->rdev),
				"QP1 ref buffer is empty!");
			return -ENOMEM;
		}
		ref.addr += wqe->sg_list[1].size + ip_hdr_size;

		/* SGE 3 */
		wqe->sg_list[2].addr = sge.addr;
		wqe->sg_list[2].lkey = sge.lkey;
		wqe->sg_list[2].size = BNXT_QPLIB_MAX_QP1_RQ_BDETH_HDR_SIZE +
				       udp_hdr_size;
		size -= wqe->sg_list[2].size;
		if (size <= 0) {
			dev_err(rdev_to_dev(qp->rdev),
				"QP1 rq buffer is empty!");
			return -ENOMEM;
		}
		sge.size = (u32)size;
		sge.addr += wqe->sg_list[2].size;

		/* SGE 4 */
		wqe->sg_list[3].addr = ref.addr;
		wqe->sg_list[3].lkey = ref.lkey;
		wqe->sg_list[3].size = ref.size;
		ref.size -= wqe->sg_list[3].size;
		if (ref.size) {
			dev_err(rdev_to_dev(qp->rdev),
				"QP1 ref buffer is incorrect!");
			return -ENOMEM;
		}
		/* SGE 5 */
		wqe->sg_list[4].addr = sge.addr;
		wqe->sg_list[4].lkey = sge.lkey;
		wqe->sg_list[4].size = sge.size;
		size -= wqe->sg_list[4].size;
		if (size) {
			dev_err(rdev_to_dev(qp->rdev),
				"QP1 rq buffer is incorrect!");
			return -ENOMEM;
		}
		sge.size = (u32)size;
		wqe->num_sge = 5;
	} else {
		dev_err(rdev_to_dev(qp->rdev), "QP1 buffer is empty!");
		return -ENOMEM;
	}

	return 0;
}

static int bnxt_re_build_qp1_shadow_qp_recv(struct bnxt_re_qp *qp,
					    CONST_STRUCT ib_recv_wr *wr,
					    struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_re_sqp_entries *sqp_entry;
	struct bnxt_qplib_sge sge;
	struct bnxt_re_dev *rdev;
	u32 rq_prod_index;

	rdev = qp->rdev;

	rq_prod_index = bnxt_qplib_get_rq_prod_index(&qp->qplib_qp);

	if (bnxt_qplib_get_qp1_rq_buf(&qp->qplib_qp, &sge)) {
		/* Create 1 SGE to receive the entire
		 * ethernet packet
		 */
		/* SGE 1 */
		wqe->sg_list[0].addr = sge.addr;
		/* TODO check the lkey to be used */
		wqe->sg_list[0].lkey = sge.lkey;
		wqe->sg_list[0].size = BNXT_QPLIB_MAX_QP1_RQ_HDR_SIZE_V2;
		if (sge.size < wqe->sg_list[0].size) {
			dev_err(rdev_to_dev(qp->rdev),
				"QP1 rq buffer is empty!");
			return -ENOMEM;
		}

		sqp_entry = &rdev->gsi_ctx.sqp_tbl[rq_prod_index];
		sqp_entry->sge.addr = wr->sg_list[0].addr;
		sqp_entry->sge.lkey = wr->sg_list[0].lkey;
		sqp_entry->sge.size = wr->sg_list[0].length;
		/* Store the wrid for reporting completion */
		sqp_entry->wrid = wqe->wr_id;
		/* change the wqe->wrid to table index */
		wqe->wr_id = rq_prod_index;
	} else {
		dev_err(rdev_to_dev(qp->rdev), "QP1 rq buffer is empty!");
		return -ENOMEM;
	}

	return 0;
}

static bool is_ud_qp(struct bnxt_re_qp *qp)
{
	return (qp->qplib_qp.type == CMDQ_CREATE_QP_TYPE_UD ||
		qp->qplib_qp.type == CMDQ_CREATE_QP_TYPE_GSI);
}

static int bnxt_re_build_send_wqe(struct bnxt_re_qp *qp,
				  CONST_STRUCT ib_send_wr *wr,
				  struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_re_ah *ah = NULL;
	struct bnxt_re_dev *rdev;
	bool is_v3;
	bool is_ud;

	rdev = qp->rdev;
	is_v3 = _is_hsi_v3(rdev->chip_ctx);
	is_ud = is_ud_qp(qp);

	if (is_ud) {
#ifdef HAVE_IB_RDMA_WR
		ah = to_bnxt_re(ud_wr(wr)->ah, struct bnxt_re_ah, ib_ah);
		if (is_v3) {
			wqe->udsend_v3.q_key = ud_wr(wr)->remote_qkey;
			wqe->udsend_v3.dst_qp = ud_wr(wr)->remote_qpn;
		} else {
			wqe->send.q_key = ud_wr(wr)->remote_qkey;
			wqe->send.dst_qp = ud_wr(wr)->remote_qpn;
		}
#else
		ah = to_bnxt_re(wr->wr.ud.ah, struct bnxt_re_ah, ib_ah);
		if (is_v3) {
			wqe->udsend_v3.q_key = wr->wr.ud.remote_qkey;
			wqe->udsend_v3.dst_qp = wr->wr.ud.remote_qpn;
		} else {
			wqe->send.q_key = wr->wr.ud.remote_qkey;
			wqe->send.dst_qp = wr->wr.ud.remote_qpn;
		}
#endif
		wqe->send.avid = ah->qplib_ah.id;
	}
	switch (wr->opcode) {
	case IB_WR_SEND:
		if (is_v3) {
			wqe->type = is_ud ? BNXT_QPLIB_SWQE_TYPE_SEND_UD_V3 :
					    BNXT_QPLIB_SWQE_TYPE_SEND_V3;
		} else {
			wqe->type = BNXT_QPLIB_SWQE_TYPE_SEND;
		}
		break;
	case IB_WR_SEND_WITH_IMM:
		if (is_v3)
			goto error_exit;
		wqe->type = BNXT_QPLIB_SWQE_TYPE_SEND_WITH_IMM;
		wqe->send.imm_data = be32_to_cpu(wr->ex.imm_data);
		break;
	case IB_WR_SEND_WITH_INV:
		if (is_v3)
			goto error_exit;
		wqe->type = BNXT_QPLIB_SWQE_TYPE_SEND_WITH_INV;
		wqe->send.inv_key = wr->ex.invalidate_rkey;
		break;
	default:
		goto error_exit;
	}
	if (wr->send_flags & IB_SEND_SIGNALED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	if (wr->send_flags & IB_SEND_FENCE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
	if (wr->send_flags & IB_SEND_SOLICITED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SOLICIT_EVENT;
	if (wr->send_flags & IB_SEND_INLINE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_INLINE;

	return 0;

error_exit:
	dev_err(rdev_to_dev(qp->rdev), "%s Invalid opcode %d!",
		__func__, wr->opcode);
	return -EINVAL;
}

static int bnxt_re_build_rdma_wqe(CONST_STRUCT ib_send_wr *wr,
				  struct bnxt_qplib_swqe *wqe)
{
	switch (wr->opcode) {
	case IB_WR_RDMA_WRITE:
		wqe->type = BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE;
		break;
	case IB_WR_RDMA_WRITE_WITH_IMM:
		wqe->type = BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE_WITH_IMM;
		wqe->rdma.imm_data = be32_to_cpu(wr->ex.imm_data);
		break;
	case IB_WR_RDMA_READ:
		wqe->type = BNXT_QPLIB_SWQE_TYPE_RDMA_READ;
		wqe->rdma.inv_key = wr->ex.invalidate_rkey;
		break;
	default:
		return -EINVAL;
	}
#ifdef HAVE_IB_RDMA_WR
	wqe->rdma.remote_va = rdma_wr(wr)->remote_addr;
	wqe->rdma.r_key = rdma_wr(wr)->rkey;
#else
	wqe->rdma.remote_va = wr->wr.rdma.remote_addr;
	wqe->rdma.r_key = wr->wr.rdma.rkey;
#endif
	if (wr->send_flags & IB_SEND_SIGNALED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	if (wr->send_flags & IB_SEND_FENCE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
	if (wr->send_flags & IB_SEND_SOLICITED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SOLICIT_EVENT;
	if (wr->send_flags & IB_SEND_INLINE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_INLINE;

	return 0;
}

static int bnxt_re_build_atomic_wqe(CONST_STRUCT ib_send_wr *wr,
				    struct bnxt_qplib_swqe *wqe)
{
	switch (wr->opcode) {
	case IB_WR_ATOMIC_CMP_AND_SWP:
		if (wr->num_sge > 1)
			return -EINVAL;
		wqe->type = BNXT_QPLIB_SWQE_TYPE_ATOMIC_CMP_AND_SWP;
#ifdef HAVE_IB_RDMA_WR
		wqe->atomic.cmp_data = atomic_wr(wr)->compare_add;
		wqe->atomic.swap_data = atomic_wr(wr)->swap;
#else
		wqe->atomic.cmp_data = wr->wr.atomic.compare_add;
		wqe->atomic.swap_data = wr->wr.atomic.swap;
#endif
		break;
	case IB_WR_ATOMIC_FETCH_AND_ADD:
		if (wr->num_sge > 1)
			return -EINVAL;
		wqe->type = BNXT_QPLIB_SWQE_TYPE_ATOMIC_FETCH_AND_ADD;
#ifdef HAVE_IB_RDMA_WR
		wqe->atomic.cmp_data = atomic_wr(wr)->compare_add;
#else
		wqe->atomic.cmp_data = wr->wr.atomic.compare_add;
#endif
		break;
	default:
		return -EINVAL;
	}
#ifdef HAVE_IB_RDMA_WR
	wqe->atomic.remote_va = atomic_wr(wr)->remote_addr;
	wqe->atomic.r_key = atomic_wr(wr)->rkey;
#else
	wqe->atomic.remote_va = wr->wr.atomic.remote_addr;
	wqe->atomic.r_key = wr->wr.atomic.rkey;
#endif
	if (wr->send_flags & IB_SEND_SIGNALED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	if (wr->send_flags & IB_SEND_FENCE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
	if (wr->send_flags & IB_SEND_SOLICITED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SOLICIT_EVENT;
	return 0;
}

static int bnxt_re_build_inv_wqe(CONST_STRUCT ib_send_wr *wr,
				 struct bnxt_qplib_swqe *wqe)
{
	wqe->type = BNXT_QPLIB_SWQE_TYPE_LOCAL_INV;
	wqe->local_inv.inv_l_key = wr->ex.invalidate_rkey;
	if (wr->send_flags & IB_SEND_SIGNALED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	if (wr->send_flags & IB_SEND_FENCE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
	if (wr->send_flags & IB_SEND_SOLICITED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SOLICIT_EVENT;

	return 0;
}

#ifdef HAVE_IB_REG_MR_WR
static int bnxt_re_build_reg_wqe(CONST_STRUCT ib_reg_wr *wr,
				 struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_re_mr *mr = to_bnxt_re(wr->mr, struct bnxt_re_mr, ib_mr);
	struct bnxt_qplib_frpl *qplib_frpl = &mr->qplib_frpl;
	int reg_len, i, access = wr->access;

	if (mr->npages > qplib_frpl->max_pg_ptrs) {
		dev_err_ratelimited(rdev_to_dev(mr->rdev),
			" %s: failed npages %d > %d", __func__,
			mr->npages, qplib_frpl->max_pg_ptrs);
		return -EINVAL;
	}

	wqe->frmr.pbl_ptr = (__le64 *)qplib_frpl->hwq.pbl_ptr[0];
	wqe->frmr.pbl_dma_ptr = qplib_frpl->hwq.pbl_dma_ptr[0];
	wqe->frmr.levels = qplib_frpl->hwq.level;
	wqe->frmr.page_list = mr->pages;
	wqe->frmr.page_list_len = mr->npages;
	wqe->type = BNXT_QPLIB_SWQE_TYPE_REG_MR;

	if (wr->wr.send_flags & IB_SEND_SIGNALED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	if (access & IB_ACCESS_LOCAL_WRITE)
		wqe->frmr.access_cntl |= SQ_FR_PMR_ACCESS_CNTL_LOCAL_WRITE;
	if (access & IB_ACCESS_REMOTE_READ)
		wqe->frmr.access_cntl |= SQ_FR_PMR_ACCESS_CNTL_REMOTE_READ;
	if (access & IB_ACCESS_REMOTE_WRITE)
		wqe->frmr.access_cntl |= SQ_FR_PMR_ACCESS_CNTL_REMOTE_WRITE;
	if (access & IB_ACCESS_REMOTE_ATOMIC)
		wqe->frmr.access_cntl |= SQ_FR_PMR_ACCESS_CNTL_REMOTE_ATOMIC;
	if (access & IB_ACCESS_MW_BIND)
		wqe->frmr.access_cntl |= SQ_FR_PMR_ACCESS_CNTL_WINDOW_BIND;

	/* TODO: OFED provides the rkey of the MR instead of the lkey */
	wqe->frmr.l_key = wr->key;
	wqe->frmr.length = wr->mr->length;
	wqe->frmr.pbl_pg_sz_log = ilog2(PAGE_SIZE >> PAGE_SHIFT_4K);
	wqe->frmr.pg_sz_log = ilog2(wr->mr->page_size >> PAGE_SHIFT_4K);
	wqe->frmr.va = wr->mr->iova;
	reg_len = wqe->frmr.page_list_len * wr->mr->page_size;

	if (wqe->frmr.length > reg_len) {
		dev_err_ratelimited(rdev_to_dev(mr->rdev),
				    "%s: bnxt_re_mr 0x%px  len (%d > %d)",
				    __func__, (void *)mr, wqe->frmr.length,
				    reg_len);

		for (i = 0; i < mr->npages; i++)
			dev_dbg(rdev_to_dev(mr->rdev),
				"%s: build_reg_wqe page[%d] = 0x%llx",
				__func__, i, mr->pages[i]);

		return -EINVAL;
	}

	return 0;
}
#endif

#ifdef HAVE_IB_MW_BIND_INFO
static int bnxt_re_build_bind_wqe(struct ib_send_wr *wr,
				  struct bnxt_qplib_swqe *wqe)
{
	struct ib_mw_bind_info *bind_info = get_bind_info(wr);
	struct ib_mw *mw = get_ib_mw(wr);

	wqe->type = BNXT_QPLIB_SWQE_TYPE_BIND_MW;
	wqe->wr_id = wr->wr_id;
	if (wr->send_flags & IB_SEND_SIGNALED)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP;
	if (wr->send_flags & IB_SEND_FENCE)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
	wqe->bind.zero_based = false;
	wqe->bind.parent_l_key = bind_info->mr->lkey;
	wqe->bind.r_key = ib_inc_rkey(mw->rkey);
	wqe->bind.va = bind_info->addr;
	wqe->bind.length = bind_info->length;
	wqe->bind.access_cntl = __from_ib_access_flags(
	bind_info->mw_access_flags);
	wqe->bind.mw_type = mw->type == IB_MW_TYPE_1 ?
			SQ_BIND_MW_TYPE_TYPE1 : SQ_BIND_MW_TYPE_TYPE2;
	return 0;
}
#endif

static void bnxt_re_set_sg_list(CONST_STRUCT ib_send_wr *wr,
				struct bnxt_qplib_swqe *wqe)
{
	wqe->sg_list = (struct bnxt_qplib_sge *)wr->sg_list;
	wqe->num_sge = wr->num_sge;
}

static void bnxt_ud_qp_hw_stall_workaround(struct bnxt_re_qp *qp)
{
	if ((qp->ib_qp.qp_type == IB_QPT_UD || qp->ib_qp.qp_type == IB_QPT_GSI ||
	    qp->ib_qp.qp_type == IB_QPT_RAW_ETHERTYPE) &&
	    qp->qplib_qp.wqe_cnt == BNXT_RE_UD_QP_HW_STALL) {
		int qp_attr_mask;
		struct ib_qp_attr qp_attr;

		qp_attr_mask = IB_QP_STATE;
		qp_attr.qp_state = IB_QPS_RTS;
		bnxt_re_modify_qp(&qp->ib_qp, &qp_attr, qp_attr_mask, NULL);
		qp->qplib_qp.wqe_cnt = 0;
	}
}

static int bnxt_re_post_send_shadow_qp(struct bnxt_re_dev *rdev,
				       struct bnxt_re_qp *qp,
				       CONST_STRUCT ib_send_wr *wr)
{
	struct bnxt_qplib_swqe wqe;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&qp->sq_lock, flags);
	while (wr) {
		memset(&wqe, 0, sizeof(wqe));
		if (wr->num_sge > qp->qplib_qp.sq.max_sge) {
			dev_err(rdev_to_dev(rdev), "Limit exceeded for Send SGEs");
			rc = -EINVAL;
			break;
		}

		bnxt_re_set_sg_list(wr, &wqe);
		wqe.wr_id = wr->wr_id;
		wqe.type = BNXT_QPLIB_SWQE_TYPE_SEND;
		rc = bnxt_re_build_send_wqe(qp, wr, &wqe);
		if (rc)
			break;

		rc = bnxt_qplib_post_send(&qp->qplib_qp, &wqe, NULL);
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"bad_wr seen with opcode = 0x%x rc = %d",
				wr->opcode, rc);
			break;
		}
		wr = wr->next;
	}
	bnxt_qplib_post_send_db(&qp->qplib_qp);
	bnxt_ud_qp_hw_stall_workaround(qp);
	spin_unlock_irqrestore(&qp->sq_lock, flags);
	return rc;
}

static void bnxt_re_legacy_set_uc_fence(struct bnxt_qplib_swqe *wqe)
{
	/* Need unconditional fence for non-wire memory opcode
	 * to work as expected.
	 */
	if (wqe->type == BNXT_QPLIB_SWQE_TYPE_LOCAL_INV ||
	    wqe->type == BNXT_QPLIB_SWQE_TYPE_FAST_REG_MR ||
	    wqe->type == BNXT_QPLIB_SWQE_TYPE_REG_MR ||
	    wqe->type == BNXT_QPLIB_SWQE_TYPE_BIND_MW)
		wqe->flags |= BNXT_QPLIB_SWQE_FLAGS_UC_FENCE;
}

int bnxt_re_post_send(struct ib_qp *ib_qp, CONST_STRUCT ib_send_wr *wr,
		      CONST_STRUCT ib_send_wr **bad_wr)
{
	struct bnxt_re_qp *qp = to_bnxt_re(ib_qp, struct bnxt_re_qp, ib_qp);
	struct bnxt_qplib_sge sge[6];
	struct bnxt_qplib_swqe wqe;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_ah *ah;
	unsigned long flags;
	int rc = 0;

	rdev = qp->rdev;
	spin_lock_irqsave(&qp->sq_lock, flags);
	while (wr) {
		memset(&wqe, 0, sizeof(wqe));
		ah = NULL;
		if (wr->num_sge > qp->qplib_qp.sq.max_sge) {
			dev_err(rdev_to_dev(rdev), "Limit exceeded for Send SGEs");
                        rc = -EINVAL;
                        goto bad;
                }

		bnxt_re_set_sg_list(wr, &wqe);
		wqe.wr_id = wr->wr_id;

		switch (wr->opcode) {
		case IB_WR_SEND:
		case IB_WR_SEND_WITH_IMM:
			if (ib_qp->qp_type == IB_QPT_GSI &&
			    rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_UD) {
				memset(sge, 0, sizeof(sge));
				wqe.sg_list = sge;
				rc = bnxt_re_build_gsi_send(qp, wr, &wqe);
				if (rc)
					goto bad;
			} else if (ib_qp->qp_type == IB_QPT_RAW_ETHERTYPE) {
				bnxt_re_build_raw_send(wr, &wqe);
			}
			switch (wr->send_flags) {
			case IB_SEND_IP_CSUM:
				wqe.rawqp1.lflags |=
					SQ_SEND_RAWETH_QP1_LFLAGS_IP_CHKSUM;
				break;
			default:
				break;
			}
			fallthrough;
		case IB_WR_SEND_WITH_INV:
			rc = bnxt_re_build_send_wqe(qp, wr, &wqe);
			if (is_ud_qp(qp)) {
#ifdef HAVE_IB_RDMA_WR
				ah = to_bnxt_re(ud_wr(wr)->ah, struct bnxt_re_ah, ib_ah);
#else
				ah = to_bnxt_re(wr->wr.ud.ah, struct bnxt_re_ah, ib_ah);
#endif
			}
			break;
		case IB_WR_RDMA_WRITE:
		case IB_WR_RDMA_WRITE_WITH_IMM:
		case IB_WR_RDMA_READ:
			rc = bnxt_re_build_rdma_wqe(wr, &wqe);
			break;
		case IB_WR_ATOMIC_CMP_AND_SWP:
		case IB_WR_ATOMIC_FETCH_AND_ADD:
			rc = bnxt_re_build_atomic_wqe(wr, &wqe);
			break;
		case IB_WR_RDMA_READ_WITH_INV:
			dev_err(rdev_to_dev(rdev),
				"RDMA Read with Invalidate is not supported");
			rc = -EINVAL;
			goto bad;
		case IB_WR_LOCAL_INV:
			rc = bnxt_re_build_inv_wqe(wr, &wqe);
			break;
#ifdef HAVE_IB_REG_MR_WR
		case IB_WR_REG_MR:
			rc = bnxt_re_build_reg_wqe(reg_wr(wr), &wqe);
			break;
#endif
#ifdef HAVE_IB_MW_BIND_INFO
		case IB_WR_BIND_MW:
			/* For type 1, 2A, and 2B binding */
			rc = bnxt_re_build_bind_wqe(wr, &wqe);
			break;
#endif
		default:
			dev_err(rdev_to_dev(rdev),
				"WR (0x%x) is not supported", wr->opcode);
			rc = -EINVAL;
			goto bad;
		}

		if (likely(!rc)) {
			if (!_is_chip_p5_plus(rdev->chip_ctx))
				bnxt_re_legacy_set_uc_fence(&wqe);
			rc = bnxt_qplib_post_send(&qp->qplib_qp, &wqe, ah);
		}
bad:
		if (unlikely(rc)) {
			dev_err(rdev_to_dev(rdev),
				"bad_wr seen with opcode = 0x%x", wr->opcode);
			*bad_wr = wr;
			break;
		}
		wr = wr->next;
	}
	bnxt_qplib_post_send_db(&qp->qplib_qp);
	if (!_is_chip_p5_plus(rdev->chip_ctx))
		bnxt_ud_qp_hw_stall_workaround(qp);
	spin_unlock_irqrestore(&qp->sq_lock, flags);

	return rc;
}

static int bnxt_re_post_recv_shadow_qp(struct bnxt_re_dev *rdev,
				struct bnxt_re_qp *qp,
				struct ib_recv_wr *wr)
{
	struct bnxt_qplib_swqe wqe;
	int rc = 0;

	while (wr) {
		memset(&wqe, 0, sizeof(wqe));
		if (wr->num_sge > qp->qplib_qp.rq.max_sge) {
			dev_err(rdev_to_dev(rdev),
				"Limit exceeded for Receive SGEs");
			rc = -EINVAL;
			goto bad;
		}

		wqe.sg_list = (struct bnxt_qplib_sge *)wr->sg_list;
		wqe.num_sge = wr->num_sge;
		wqe.wr_id = wr->wr_id;
		wqe.type = _is_hsi_v3(qp->qplib_qp.cctx) ?
			   BNXT_QPLIB_SWQE_TYPE_RECV_V3 : BNXT_QPLIB_SWQE_TYPE_RECV;
		rc = bnxt_qplib_post_recv(&qp->qplib_qp, &wqe);
bad:
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"bad_wr seen with RQ post");
			break;
		}
		wr = wr->next;
	}
	bnxt_qplib_post_recv_db(&qp->qplib_qp);
	return rc;
}

static int bnxt_re_build_gsi_recv(struct bnxt_re_qp *qp,
				  CONST_STRUCT ib_recv_wr *wr,
				  struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_re_dev *rdev = qp->rdev;
	int rc = 0;

	if (rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_ALL)
		rc = bnxt_re_build_qp1_shadow_qp_recv(qp, wr, wqe);
	else
		rc = bnxt_re_build_qp1_recv(qp, wr, wqe);

	return rc;
}

int bnxt_re_post_recv(struct ib_qp *ib_qp, CONST_STRUCT ib_recv_wr *wr,
		      CONST_STRUCT ib_recv_wr **bad_wr)
{
	struct bnxt_re_qp *qp = to_bnxt_re(ib_qp, struct bnxt_re_qp, ib_qp);
	struct bnxt_re_dev *rdev = qp->rdev;
	struct bnxt_qplib_sge sge[6];
	struct bnxt_qplib_swqe wqe;
	unsigned long flags;
	u32 count = 0;
	int rc = 0;

	spin_lock_irqsave(&qp->rq_lock, flags);
	while (wr) {
		memset(&wqe, 0, sizeof(wqe));
		if (wr->num_sge > qp->qplib_qp.rq.max_sge) {
			dev_err(rdev_to_dev(rdev), "Limit exceeded for Receive SGEs");
                        rc = -EINVAL;
                        goto bad;
                }
		wqe.num_sge = wr->num_sge;
		wqe.sg_list = (struct bnxt_qplib_sge *)wr->sg_list;
		wqe.wr_id = wr->wr_id;
		wqe.type = _is_hsi_v3(qp->qplib_qp.cctx) ?
			    BNXT_QPLIB_SWQE_TYPE_RECV_V3 : BNXT_QPLIB_SWQE_TYPE_RECV;

		if (ib_qp->qp_type == IB_QPT_GSI &&
		    rdev->gsi_ctx.gsi_qp_mode != BNXT_RE_GSI_MODE_UD) {
			memset(sge, 0, sizeof(sge));
			wqe.sg_list = sge;
			rc = bnxt_re_build_gsi_recv(qp, wr, &wqe);
			if (rc)
				goto bad;
		}
		rc = bnxt_qplib_post_recv(&qp->qplib_qp, &wqe);
bad:
		if (rc) {
			dev_err(rdev_to_dev(rdev), "bad_wr seen with RQ post");
			*bad_wr = wr;
			break;
		}
		/* Ring DB if the RQEs posted reaches a threshold value */
		if (++count >= BNXT_RE_RQ_WQE_THRESHOLD) {
			bnxt_qplib_post_recv_db(&qp->qplib_qp);
			count = 0;
		}
		wr = wr->next;
	}

	if (count)
		bnxt_qplib_post_recv_db(&qp->qplib_qp);
	spin_unlock_irqrestore(&qp->rq_lock, flags);

	return rc;
}

/* Completion Queues */
DESTROY_CQ_RET bnxt_re_destroy_cq(struct ib_cq *ib_cq
#ifdef HAVE_DESTROY_CQ_UDATA
	       , struct ib_udata *udata
#endif
		)
{
	struct bnxt_re_cq *cq = to_bnxt_re(ib_cq, struct bnxt_re_cq, ib_cq);
	struct bnxt_re_dev *rdev = cq->rdev;
	void *ctx_sb_data = NULL;
	bool do_snapdump;
	u16 ctx_size;
	int rc =  0;

	if (cq->cq_toggle_page) {
		BNXT_RE_CQ_PAGE_LIST_DEL(cq->uctx, cq);
#ifndef HAVE_UAPI_DEF
		bnxt_re_mmap_entry_remove_compat(cq->cq_toggle_mmap);
#endif
		free_page((unsigned long)cq->cq_toggle_page);
		cq->cq_toggle_page = NULL;
		hash_del(&cq->hash_entry);
	}

	if (cq->is_dbr_soft_cq && cq->uctx) {
		struct bnxt_re_res_list *res_list;
		void *dbr_page;

		if (cq->uctx->dbr_recov_cq) {
			dbr_page = cq->uctx->dbr_recov_cq_page;

			res_list = &rdev->res_list[BNXT_RE_RES_TYPE_UCTX];
			spin_lock(&res_list->lock);
			cq->uctx->dbr_recov_cq_page = NULL;
			cq->uctx->dbr_recov_cq = NULL;
			spin_unlock(&res_list->lock);
			bnxt_re_mmap_entry_remove_compat(cq->uctx->dbr_recovery_cq_mmap);
			free_page((unsigned long)dbr_page);
		}

#ifndef HAVE_CQ_ALLOC_IN_IB_CORE
		kfree(cq);
#endif

#ifndef HAVE_DESTROY_CQ_RET_VOID
		return 0;
#else
		return;
#endif
	}

	BNXT_RE_RES_LIST_DEL(rdev, cq, BNXT_RE_RES_TYPE_CQ);

	if (rdev->hdbr_enabled)
		bnxt_re_hdbr_db_unreg_cq(rdev, cq);

	ctx_size = cq->qplib_cq.ctx_size_sb;
	if (ctx_size)
		ctx_sb_data = vzalloc(ctx_size);

	rc = bnxt_qplib_destroy_cq(&rdev->qplib_res, &cq->qplib_cq,
				   ctx_size, ctx_sb_data);

	do_snapdump = test_bit(CQ_FLAGS_CAPTURE_SNAPDUMP, &cq->qplib_cq.flags);
	if (!rc)
		bnxt_re_save_resource_context(rdev, ctx_sb_data,
					      BNXT_RE_RES_TYPE_CQ, do_snapdump);
	else
		dev_err_ratelimited(rdev_to_dev(rdev), "%s id = %d failed rc = %d",
				    __func__, cq->qplib_cq.id, rc);

	vfree(ctx_sb_data);

	bnxt_re_put_nq(rdev, cq->qplib_cq.nq);

	bnxt_re_umem_free(&cq->umem);
	bnxt_re_umem_free(&cq->cqprod);
	bnxt_re_umem_free(&cq->cqcons);

	kfree(cq->cql);
	atomic_dec(&rdev->stats.rsors.cq_count);

#ifndef HAVE_CQ_ALLOC_IN_IB_CORE
	kfree(cq);
#endif

#ifndef HAVE_DESTROY_CQ_RET_VOID
	/* return success for destroy resources */
	return 0;
#endif
}

#ifdef HAVE_RDMA_XARRAY_MMAP_V2
struct bnxt_re_user_mmap_entry*
bnxt_re_mmap_entry_insert(struct bnxt_re_ucontext *uctx, u64 cpu_addr,
			  dma_addr_t dma_addr, u8 user_mmap_hint, u64 *offset)
{
	struct bnxt_re_user_mmap_entry *entry;
	int ret;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return NULL;

	entry->cpu_addr = cpu_addr;
	entry->dma_addr = dma_addr;
	entry->uctx	= uctx;
	entry->mmap_flag = user_mmap_hint;

	WARN_ON(user_mmap_hint == BNXT_RE_MMAP_SH_PAGE);
	switch (user_mmap_hint) {
	case BNXT_RE_MMAP_DB_RECOVERY_PAGE:
		ret = rdma_user_mmap_entry_insert_range(&uctx->ib_uctx,
							&entry->rdma_entry, PAGE_SIZE,
							user_mmap_hint, user_mmap_hint);
		break;
	case BNXT_RE_MMAP_UC_DB:
	case BNXT_RE_MMAP_WC_DB:
	case BNXT_RE_MMAP_DBR_PACING_BAR:
	case BNXT_RE_MMAP_DBR_PAGE:
	case BNXT_RE_MMAP_TOGGLE_PAGE:
	case BNXT_RE_MMAP_HDBR_BASE:
		/* Dynamic xarray entry will be inserted from index 0xF.
		 * Currently up to index 0x3, library is posting fixed mmap_flag.
		 * To avoid any conflict in future, range of 0x0 - 0xF is reserved
		 * for static mapping.
		 */
		ret = rdma_user_mmap_entry_insert_range(&uctx->ib_uctx,
							&entry->rdma_entry,
							PAGE_SIZE, 0xF, U32_MAX);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret) {
		kfree(entry);
		return NULL;
	}

	dev_dbg(NULL,
		"%s %d uctx->ib_uctx 0x%lx cpu 0x%lx dma 0x%lx mmap hint %s offset 0x%lx\n",
		__func__, __LINE__, (unsigned long)&uctx->ib_uctx,
		(unsigned long)entry->cpu_addr, (unsigned long)entry->dma_addr,
		bnxt_re_mmap_str(user_mmap_hint),
		(unsigned long)rdma_user_mmap_get_offset(&entry->rdma_entry));
	if (offset)
		*offset = rdma_user_mmap_get_offset(&entry->rdma_entry);

	return entry;
}
#endif

#ifdef HAVE_IB_CQ_CREATE_HAS_UVERBS_ATTR_BUNDLE
ALLOC_CQ_RET bnxt_re_create_cq(ALLOC_CQ_IN * cq_in,
			       const struct ib_cq_init_attr *attr,
			       struct uverbs_attr_bundle *attrs)
#else
ALLOC_CQ_RET bnxt_re_create_cq(ALLOC_CQ_IN *cq_in,
			       const struct ib_cq_init_attr *attr,
#ifdef HAVE_CREATE_CQ_UCONTEXT
			       struct ib_ucontext *context,
#endif
			       struct ib_udata *udata)
#endif /* HAVE_IB_CQ_CREATE_HAS_UVERBS_ATTR_BUNDLE */
{
#ifdef HAVE_IB_CQ_CREATE_HAS_UVERBS_ATTR_BUNDLE
	struct ib_udata *udata = &attrs->driver_udata;
#endif
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_re_ucontext *uctx = NULL;
	struct bnxt_re_res_list *res_list;
#ifndef HAVE_CREATE_CQ_UCONTEXT
	struct ib_ucontext *context = NULL;
#endif
	struct bnxt_re_cq_req ureq = {};
	struct bnxt_qplib_cq *qplcq;
	struct bnxt_re_dev *rdev;
	int rc, entries;
	bool is_entry = false;
	struct bnxt_re_cq *cq;
	u32 max_active_cqs;
	int cqe = attr->cqe;

#ifdef HAVE_CQ_ALLOC_IN_IB_CORE
	if (attr->flags)
		return -EOPNOTSUPP;
#endif

	rdev = rdev_from_cq_in(cq_in);
	if (udata) {
#ifdef HAVE_RDMA_UDATA_TO_DRV_CONTEXT
		uctx = rdma_udata_to_drv_context(udata,
						 struct bnxt_re_ucontext,
						 ib_uctx);
#else
#ifdef HAVE_CREATE_CQ_UCONTEXT
		uctx = to_bnxt_re(context, struct bnxt_re_ucontext, ib_uctx);
#endif /* HAVE_CREATE_CQ_UCONTEXT */
#endif /* HAVE_RDMA_UDATA_TO_DRV_CONTEXT */
	}
	dev_attr = rdev->dev_attr;

	if (atomic_read(&rdev->stats.rsors.cq_count) >= dev_attr->max_cq) {
		dev_err(rdev_to_dev(rdev), "Create CQ failed - max exceeded(CQs)");
		rc = -EINVAL;
		goto exit;
	}
	/* Validate CQ fields */
	if (cqe < 1 || cqe > dev_attr->max_cq_wqes) {
		dev_err(rdev_to_dev(rdev), "Create CQ failed - max exceeded(CQ_WQs)");
		rc = -EINVAL;
		goto exit;
	}

	cq = __get_cq_from_cq_in(cq_in, rdev);
	if (!cq) {
		rc = -ENOMEM;
		goto exit;
	}
	cq->rdev = rdev;
	cq->uctx = uctx;
	qplcq = &cq->qplib_cq;
	qplcq->cq_handle = (u64)qplcq;
	/*
	 * Since CQ is for QP1 is shared with Shadow CQ, the size
	 * should be double the size. There is no way to identify
	 * whether this CQ is for GSI QP. So assuming that the first
	 * CQ created is for QP1
	 */
	if (!udata && !rdev->gsi_ctx.first_cq_created &&
	    rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_ALL) {
		rdev->gsi_ctx.first_cq_created = true;
		/*
		 * Total CQE required for the CQ = CQE for QP1 RQ +
		 * CQE for Shadow QP SQEs + CQE for Shadow QP RQEs.
		 * Max entries of shadow QP SQ and RQ = QP1 RQEs = cqe
		 */
		cqe *= 3;
	}

	entries = bnxt_re_init_depth(cqe + 1, uctx);
	if (entries > dev_attr->max_cq_wqes + 1)
		entries = dev_attr->max_cq_wqes + 1;

	qplcq->sginfo.pgshft = PAGE_SHIFT;
	qplcq->sginfo.pgsize = PAGE_SIZE;
	if (udata) {
		if (udata->inlen < sizeof(ureq))
			dev_warn_once(rdev_to_dev(rdev),
				 "Update the library ulen %d klen %d",
				 (unsigned int)udata->inlen,
				 (unsigned int)sizeof(ureq));

		rc = ib_copy_from_udata(&ureq, udata,
					min(udata->inlen, sizeof(ureq)));
		if (rc)
			goto fail;

		if (BNXT_RE_IS_DBR_PACING_NOTIFY_CQ(ureq)) {
			cq->is_dbr_soft_cq = true;
			goto success;
		}

		if (BNXT_RE_IS_DBR_RECOV_CQ(ureq)) {
			void *dbr_page;
			u32 *epoch;

			dbr_page = (void *)__get_free_page(GFP_KERNEL);
			if (!dbr_page) {
				dev_err(rdev_to_dev(rdev),
					"DBR recov CQ page allocation failed!");
				rc = -ENOMEM;
				goto fail;
			}

			/* memset the epoch and epoch_ack to 0 */
			epoch = dbr_page;
			epoch[0] = 0x0;
			epoch[1] = 0x0;

			res_list = &rdev->res_list[BNXT_RE_RES_TYPE_UCTX];
			spin_lock(&res_list->lock);
			uctx->dbr_recov_cq = cq;
			uctx->dbr_recov_cq_page = dbr_page;
			spin_unlock(&res_list->lock);

			is_entry = bnxt_re_mmap_entry_insert_compat(uctx, (u64)uctx->dbr_recov_cq_page, 0,
								    BNXT_RE_MMAP_DB_RECOVERY_PAGE, NULL,
								    &uctx->dbr_recovery_cq_mmap);
			if (!is_entry) {
				rc = -ENOMEM;
				goto fail;
			}
			cq->is_dbr_soft_cq = true;
			goto success;
		}

		if (ureq.cqprodva)
			cq->cqprod = ib_umem_get_compat(rdev, context, udata,
							ureq.cqprodva, sizeof(u32),
							IB_ACCESS_LOCAL_WRITE, 1);
		if (ureq.cqconsva)
			cq->cqcons = ib_umem_get_compat(rdev, context, udata,
							ureq.cqconsva, sizeof(u32),
							IB_ACCESS_LOCAL_WRITE, 1);
		cq->umem = ib_umem_get_compat
				      (rdev, context, udata, ureq.cq_va,
				       entries * sizeof(struct cq_base),
				       IB_ACCESS_LOCAL_WRITE, 1);
		if (IS_ERR(cq->umem)) {
			rc = PTR_ERR(cq->umem);
			dev_err(rdev_to_dev(rdev),
				"%s: ib_umem_get failed! rc = %d\n",
				__func__, rc);
			goto fail;
		}
		bnxt_re_set_sginfo(rdev, cq->umem, ureq.cq_va, &qplcq->sginfo, BNXT_RE_RES_TYPE_CQ);
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
		qplcq->sginfo.sghead = get_ib_umem_sgl(cq->umem,
						       &qplcq->sginfo.nmap);
#else
		qplcq->sginfo.umem = cq->umem;
#endif
		qplcq->dpi = &uctx->dpi;
	} else {
		cq->max_cql = entries > MAX_CQL_PER_POLL ? MAX_CQL_PER_POLL : entries;
		cq->cql = kcalloc(cq->max_cql, sizeof(struct bnxt_qplib_cqe),
				  GFP_KERNEL);
		if (!cq->cql) {
			dev_err(rdev_to_dev(rdev),
				"Allocate CQL for %d failed!", cq->max_cql);
			rc = -ENOMEM;
			goto fail;
		}
		/* TODO: DPI is for privilege app for now */
		qplcq->dpi = &rdev->dpi_privileged;
	}
	/*
	 * Allocating the NQ in a round robin fashion. nq_alloc_cnt is a
	 * used for getting the NQ index.
	 */
	qplcq->max_wqe = entries;
	qplcq->nq = bnxt_re_get_nq(rdev);
	qplcq->cnq_hw_ring_id = qplcq->nq->ring_id;
	qplcq->coalescing = &rdev->cq_coalescing;
	qplcq->overflow_telemetry = rdev->enable_queue_overflow_telemetry;
	if (ureq.comp_mask & BNXT_RE_COMP_MASK_CQ_REQ_IGNORE_OVERRUN)
		qplcq->ignore_overrun = 1;
	rc = bnxt_qplib_create_cq(&rdev->qplib_res, qplcq);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Create HW CQ failed!");
		goto fail;
	}

	INIT_LIST_HEAD(&cq->cq_list);
	cq->ib_cq.cqe = entries;
	cq->cq_period = qplcq->period;

	atomic_inc(&rdev->stats.rsors.cq_count);
	max_active_cqs = atomic_read(&rdev->stats.rsors.cq_count);
	if (max_active_cqs > atomic_read(&rdev->stats.rsors.max_cq_count))
		atomic_set(&rdev->stats.rsors.max_cq_count, max_active_cqs);
	spin_lock_init(&cq->cq_lock);

	if (udata) {
		struct bnxt_re_cq_resp resp = {};

		if (rdev->hdbr_enabled) {
			rc = bnxt_re_hdbr_db_reg_cq(rdev, cq, uctx, &resp, &ureq);
			if (rc)
				goto destroy_cq;
		}

#ifdef HAVE_CREATE_CQ_UCONTEXT
		cq->context = context;
#endif
		resp.cqid = qplcq->id;
		resp.tail = qplcq->hwq.cons;
		resp.phase = qplcq->period;
		if (rdev->chip_ctx->modes.toggle_bits & BNXT_QPLIB_CQ_TOGGLE_BIT) {
			cq->cq_toggle_page = (void *)get_zeroed_page(GFP_KERNEL);
			if (!cq->cq_toggle_page) {
				dev_err(rdev_to_dev(rdev),
					"CQ page allocation failed!");
				(void)bnxt_qplib_destroy_cq(&rdev->qplib_res,
							    qplcq, 0, NULL);
				rc = -ENOMEM;
				goto unreg_db_cq;
			}

#ifndef HAVE_UAPI_DEF
			is_entry = bnxt_re_mmap_entry_insert_compat(uctx, (u64)cq->cq_toggle_page, 0,
								    BNXT_RE_MMAP_TOGGLE_PAGE,
								    &resp.cq_toggle_mmap_key,
								    &cq->cq_toggle_mmap);
			if (!is_entry) {
				rc = -ENOMEM;
				goto unreg_db_cq;
			}
#endif
			hash_add(rdev->cq_hash, &cq->hash_entry, cq->qplib_cq.id);
			resp.comp_mask |= BNXT_RE_CQ_TOGGLE_PAGE_SUPPORT;
		}

		rc = bnxt_re_copy_to_udata(rdev, &resp,
					   min(udata->outlen, sizeof(resp)),
					   udata);
		if (rc)
			goto unreg_db_cq;
		if (cq->cq_toggle_page)
			BNXT_RE_CQ_PAGE_LIST_ADD(uctx, cq);
	} else {
		if (rdev->hdbr_enabled) {
			rc = bnxt_re_hdbr_db_reg_cq(rdev, cq, NULL, NULL, NULL);
			if (rc)
				goto destroy_cq;
		}
	}
	BNXT_RE_RES_LIST_ADD(rdev, cq, BNXT_RE_RES_TYPE_CQ);

success:
#ifdef HAVE_CQ_ALLOC_IN_IB_CORE
	return 0;
#else
	return &cq->ib_cq;
#endif

unreg_db_cq:
	if (cq->cq_toggle_page) {
		/* This error handling. No need to check ABI v7 vs v8 */
		bnxt_re_mmap_entry_remove_compat(cq->cq_toggle_mmap);
		free_page((unsigned long)cq->cq_toggle_page);
		cq->cq_toggle_page = NULL;
		/* TBD - this is missing in upstream code. */
		hash_del(&cq->hash_entry);
	}
	if (rdev->hdbr_enabled)
		bnxt_re_hdbr_db_unreg_cq(rdev, cq);
destroy_cq:
	(void)bnxt_qplib_destroy_cq(&rdev->qplib_res, qplcq, 0, NULL);
fail:
	if (uctx && cq->dbr_recov_cq_page) {
		bnxt_re_mmap_entry_remove_compat(uctx->dbr_recovery_cq_mmap);
		res_list = &rdev->res_list[BNXT_RE_RES_TYPE_UCTX];
		spin_lock(&res_list->lock);
		free_page((unsigned long)uctx->dbr_recov_cq_page);
		uctx->dbr_recov_cq = NULL;
		uctx->dbr_recov_cq_page = NULL;
		spin_unlock(&res_list->lock);
	}

	bnxt_re_umem_free(&cq->umem);
	bnxt_re_umem_free(&cq->cqprod);
	bnxt_re_umem_free(&cq->cqcons);

	if (cq) {
		if (cq->cql)
			kfree(cq->cql);
#ifndef HAVE_CQ_ALLOC_IN_IB_CORE
		kfree(cq);
#endif
	}
exit:
#ifdef HAVE_CQ_ALLOC_IN_IB_CORE
	return rc;
#else
	return ERR_PTR(rc);
#endif
}

int bnxt_re_modify_cq(struct ib_cq *ib_cq, u16 cq_count, u16 cq_period)
{
	struct bnxt_re_cq *cq = to_bnxt_re(ib_cq, struct bnxt_re_cq, ib_cq);
	struct bnxt_re_dev *rdev = cq->rdev;
	int rc;

	if ((cq->cq_count != cq_count) || (cq->cq_period != cq_period)) {
		cq->qplib_cq.count = cq_count;
		cq->qplib_cq.period = cq_period;
		rc = bnxt_qplib_modify_cq(&rdev->qplib_res, &cq->qplib_cq);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "Modify HW CQ %#x failed!",
				cq->qplib_cq.id);
			return rc;
		}
		/* On success, update the shadow */
		cq->cq_count = cq_count;
		cq->cq_period = cq_period;
	}
	return 0;
}

static void bnxt_re_resize_cq_complete(struct bnxt_re_cq *cq)
{
	struct bnxt_re_dev *rdev = cq->rdev;

	bnxt_qplib_resize_cq_complete(&rdev->qplib_res, &cq->qplib_cq);

	cq->qplib_cq.max_wqe = cq->resize_cqe;
	if (cq->resize_umem) {
		bnxt_re_umem_free(&cq->umem);
		cq->umem = cq->resize_umem;
		cq->resize_umem = NULL;
		cq->resize_cqe = 0;
	}
}

int bnxt_re_resize_cq(struct ib_cq *ib_cq, int cqe, struct ib_udata *udata)
{
	struct bnxt_qplib_sg_info sginfo = {};
	struct bnxt_qplib_dpi *orig_dpi = NULL;
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_re_ucontext *uctx = NULL;
	struct bnxt_re_resize_cq_req ureq;
	struct ib_ucontext *context = NULL;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_cq *cq;
	int rc, entries;

	/* Don't allow more than one resize request at the same time.
	 * TODO: need a mutex here when we support kernel consumers of resize.
	 */
	cq =  to_bnxt_re(ib_cq, struct bnxt_re_cq, ib_cq);
	rdev = cq->rdev;
	dev_attr = rdev->dev_attr;
	if (ib_cq->uobject) {
#ifdef HAVE_RDMA_UDATA_TO_DRV_CONTEXT
		uctx = rdma_udata_to_drv_context(udata,
						 struct bnxt_re_ucontext,
						 ib_uctx);
		context = &uctx->ib_uctx;
#else
		context = cq->context;
		uctx = to_bnxt_re(context, struct bnxt_re_ucontext, ib_uctx);
#endif
	}

	if (cq->resize_umem) {
		dev_err(rdev_to_dev(rdev), "Resize CQ %#x failed - Busy",
			cq->qplib_cq.id);
		return -EBUSY;
	}

	/* Check the requested cq depth out of supported depth */
	if (cqe < 1 || cqe > dev_attr->max_cq_wqes) {
		dev_err(rdev_to_dev(rdev), "Resize CQ %#x failed - max exceeded",
			cq->qplib_cq.id);
		return -EINVAL;
	}

	entries = bnxt_re_init_depth(cqe + 1, uctx);
	entries = min_t(u32, (u32)entries, dev_attr->max_cq_wqes + 1);

	/* Check to see if the new requested size can be handled by already
	 * existing CQ
	 */
	if (entries == cq->ib_cq.cqe) {
		dev_info(rdev_to_dev(rdev), "CQ is already at size %d", cqe);
		return 0;
	}

	if (ib_cq->uobject && udata) {
		if (udata->inlen < sizeof(ureq))
			dev_warn_once(rdev_to_dev(rdev),
				 "Update the library ulen %d klen %d",
				 (unsigned int)udata->inlen,
				 (unsigned int)sizeof(ureq));

		rc = ib_copy_from_udata(&ureq, udata,
					min(udata->inlen, sizeof(ureq)));
		if (rc)
			goto fail;

		dev_dbg(rdev_to_dev(rdev), "%s: va %p", __func__,
			(void *)ureq.cq_va);
		cq->resize_umem = ib_umem_get_compat
				       (rdev,
					context, udata, ureq.cq_va,
					entries * sizeof(struct cq_base),
					IB_ACCESS_LOCAL_WRITE, 1);
		if (IS_ERR(cq->resize_umem)) {
			rc = PTR_ERR(cq->resize_umem);
			cq->resize_umem = NULL;
			dev_err(rdev_to_dev(rdev), "%s: ib_umem_get failed! rc = %d\n",
				__func__, rc);
			goto fail;
		}
		cq->resize_cqe = entries;
		dev_dbg(rdev_to_dev(rdev), "%s: ib_umem_get() success\n",
			__func__);
		memcpy(&sginfo, &cq->qplib_cq.sginfo, sizeof(sginfo));
		orig_dpi = cq->qplib_cq.dpi;

		bnxt_re_set_sginfo(rdev, cq->resize_umem, ureq.cq_va,
				   &cq->qplib_cq.sginfo, BNXT_RE_RES_TYPE_CQ);
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
		cq->qplib_cq.sginfo.sghead = get_ib_umem_sgl(cq->resize_umem,
						&cq->qplib_cq.sginfo.nmap);
#else
		cq->qplib_cq.sginfo.umem = cq->resize_umem;
#endif
		cq->qplib_cq.dpi = &uctx->dpi;
	} else {
		/* TODO: kernel consumer */
	}

	rc = bnxt_qplib_resize_cq(&rdev->qplib_res, &cq->qplib_cq, entries);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Resize HW CQ %#x failed!",
			cq->qplib_cq.id);
		goto fail;
	}

	cq->ib_cq.cqe = cq->resize_cqe;
	/* For kernel consumers complete resize here. For uverbs consumers,
	 * we complete it in the context of ibv_poll_cq().
	 */
	if (!cq->resize_umem)
		bnxt_qplib_resize_cq_complete(&rdev->qplib_res, &cq->qplib_cq);

	atomic_inc(&rdev->stats.rsors.resize_count);
	return 0;

fail:
	if (cq->resize_umem) {
		bnxt_re_umem_free(&cq->resize_umem);
		cq->resize_umem = NULL;
		cq->resize_cqe = 0;
		memcpy(&cq->qplib_cq.sginfo, &sginfo, sizeof(sginfo));
		cq->qplib_cq.dpi = orig_dpi;
	}
	return rc;
}

static enum ib_wc_status __req_to_ib_wc_status(u8 qstatus)
{
	switch(qstatus) {
	case CQ_REQ_STATUS_OK:
		return IB_WC_SUCCESS;
	case CQ_REQ_STATUS_BAD_RESPONSE_ERR:
		return IB_WC_BAD_RESP_ERR;
	case CQ_REQ_STATUS_LOCAL_LENGTH_ERR:
		return IB_WC_LOC_LEN_ERR;
	case CQ_REQ_STATUS_LOCAL_QP_OPERATION_ERR:
		return IB_WC_LOC_QP_OP_ERR;
	case CQ_REQ_STATUS_LOCAL_PROTECTION_ERR:
		return IB_WC_LOC_PROT_ERR;
	case CQ_REQ_STATUS_MEMORY_MGT_OPERATION_ERR:
		return IB_WC_GENERAL_ERR;
	case CQ_REQ_STATUS_REMOTE_INVALID_REQUEST_ERR:
		return IB_WC_REM_INV_REQ_ERR;
	case CQ_REQ_STATUS_REMOTE_ACCESS_ERR:
		return IB_WC_REM_ACCESS_ERR;
	case CQ_REQ_STATUS_REMOTE_OPERATION_ERR:
		return IB_WC_REM_OP_ERR;
	case CQ_REQ_STATUS_RNR_NAK_RETRY_CNT_ERR:
		return IB_WC_RNR_RETRY_EXC_ERR;
	case CQ_REQ_STATUS_TRANSPORT_RETRY_CNT_ERR:
		return IB_WC_RETRY_EXC_ERR;
	case CQ_REQ_STATUS_WORK_REQUEST_FLUSHED_ERR:
		return IB_WC_WR_FLUSH_ERR;
	default:
		return IB_WC_GENERAL_ERR;
	}
	return 0;
}

static enum ib_wc_status __rawqp1_to_ib_wc_status(u8 qstatus)
{
	switch(qstatus) {
	case CQ_RES_RAWETH_QP1_STATUS_OK:
		return IB_WC_SUCCESS;
	case CQ_RES_RAWETH_QP1_STATUS_LOCAL_ACCESS_ERROR:
		return IB_WC_LOC_ACCESS_ERR;
	case CQ_RES_RAWETH_QP1_STATUS_HW_LOCAL_LENGTH_ERR:
		return IB_WC_LOC_LEN_ERR;
	case CQ_RES_RAWETH_QP1_STATUS_LOCAL_PROTECTION_ERR:
		return IB_WC_LOC_PROT_ERR;
	case CQ_RES_RAWETH_QP1_STATUS_LOCAL_QP_OPERATION_ERR:
		return IB_WC_LOC_QP_OP_ERR;
	case CQ_RES_RAWETH_QP1_STATUS_MEMORY_MGT_OPERATION_ERR:
		return IB_WC_GENERAL_ERR;
	case CQ_RES_RAWETH_QP1_STATUS_WORK_REQUEST_FLUSHED_ERR:
		return IB_WC_WR_FLUSH_ERR;
	case CQ_RES_RAWETH_QP1_STATUS_HW_FLUSH_ERR:
		return IB_WC_WR_FLUSH_ERR;
	default:
		return IB_WC_GENERAL_ERR;
	}
}

static enum ib_wc_status __rc_v3_to_ib_wc_status(u8 qstatus)
{
	switch (qstatus) {
	case CQ_RES_RC_V3_STATUS_OK:
		return IB_WC_SUCCESS;
	case CQ_RES_RC_V3_STATUS_LOCAL_LENGTH_ERR:
		return IB_WC_LOC_LEN_ERR;
	case CQ_RES_RC_V3_STATUS_LOCAL_QP_OPERATION_ERR:
		return IB_WC_LOC_QP_OP_ERR;
	case CQ_RES_RC_V3_STATUS_LOCAL_PROTECTION_ERR:
		return IB_WC_LOC_PROT_ERR;
	case CQ_RES_RC_V3_STATUS_LOCAL_ACCESS_ERROR:
		return IB_WC_LOC_ACCESS_ERR;
	case CQ_RES_RC_V3_STATUS_REMOTE_INVALID_REQUEST_ERR:
		return IB_WC_REM_INV_REQ_ERR;
	case CQ_RES_RC_V3_STATUS_WORK_REQUEST_FLUSHED_ERR:
		return IB_WC_WR_FLUSH_ERR;
	case CQ_RES_RC_V3_STATUS_HW_FLUSH_ERR:
	case CQ_RES_RC_V3_STATUS_OVERFLOW_ERR:
	default:
		return IB_WC_GENERAL_ERR;
	}
}
static enum ib_wc_status __rc_to_ib_wc_status(u8 qstatus)
{
	switch(qstatus) {
	case CQ_RES_RC_STATUS_OK:
		return IB_WC_SUCCESS;
	case CQ_RES_RC_STATUS_LOCAL_ACCESS_ERROR:
		return IB_WC_LOC_ACCESS_ERR;
	case CQ_RES_RC_STATUS_LOCAL_LENGTH_ERR:
		return IB_WC_LOC_LEN_ERR;
	case CQ_RES_RC_STATUS_LOCAL_PROTECTION_ERR:
		return IB_WC_LOC_PROT_ERR;
	case CQ_RES_RC_STATUS_LOCAL_QP_OPERATION_ERR:
		return IB_WC_LOC_QP_OP_ERR;
	case CQ_RES_RC_STATUS_MEMORY_MGT_OPERATION_ERR:
		return IB_WC_GENERAL_ERR;
	case CQ_RES_RC_STATUS_REMOTE_INVALID_REQUEST_ERR:
		return IB_WC_REM_INV_REQ_ERR;
	case CQ_RES_RC_STATUS_WORK_REQUEST_FLUSHED_ERR:
		return IB_WC_WR_FLUSH_ERR;
	case CQ_RES_RC_STATUS_HW_FLUSH_ERR:
		return IB_WC_WR_FLUSH_ERR;
	default:
		return IB_WC_GENERAL_ERR;
	}
}

static void bnxt_re_process_req_wc(struct bnxt_re_dev *rdev, struct ib_wc *wc,
				   struct bnxt_qplib_cqe *cqe)
{
	switch (cqe->type) {
	case BNXT_QPLIB_SWQE_TYPE_SEND:
	case BNXT_QPLIB_SWQE_TYPE_SEND_UD_V3:
		wc->opcode = IB_WC_SEND;
		break;
	case BNXT_QPLIB_SWQE_TYPE_SEND_WITH_IMM:
		wc->opcode = IB_WC_SEND;
		wc->wc_flags |= IB_WC_WITH_IMM;
		break;
	case BNXT_QPLIB_SWQE_TYPE_SEND_WITH_INV:
		wc->opcode = IB_WC_SEND;
		wc->wc_flags |= IB_WC_WITH_INVALIDATE;
		break;
	case BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE:
		wc->opcode = IB_WC_RDMA_WRITE;
		break;
	case BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE_WITH_IMM:
		wc->opcode = IB_WC_RDMA_WRITE;
		wc->wc_flags |= IB_WC_WITH_IMM;
		break;
	case BNXT_QPLIB_SWQE_TYPE_RDMA_READ:
		wc->opcode = IB_WC_RDMA_READ;
		break;
	case BNXT_QPLIB_SWQE_TYPE_ATOMIC_CMP_AND_SWP:
		if (_is_hsi_v3(rdev->chip_ctx)) {
			wc->status = IB_WC_REM_INV_REQ_ERR;
			return;
		}
		wc->opcode = IB_WC_COMP_SWAP;
		break;
	case BNXT_QPLIB_SWQE_TYPE_ATOMIC_FETCH_AND_ADD:
		if (_is_hsi_v3(rdev->chip_ctx)) {
			wc->status = IB_WC_REM_INV_REQ_ERR;
			return;
		}
		wc->opcode = IB_WC_FETCH_ADD;
		break;
	case BNXT_QPLIB_SWQE_TYPE_LOCAL_INV:
		wc->opcode = IB_WC_LOCAL_INV;
		break;
#ifdef HAVE_IB_REG_MR_WR
	case BNXT_QPLIB_SWQE_TYPE_REG_MR:
		wc->opcode = IB_WC_REG_MR;
		break;
#endif
	case BNXT_QPLIB_SWQE_TYPE_SEND_V3:
	case BNXT_QPLIB_SWQE_TYPE_SEND_UD_WITH_IMM_V3:
	case BNXT_QPLIB_SWQE_TYPE_SEND_WITH_IMM_V3:
	case BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE_V3:
	case BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE_WITH_IMM_V3:
	case BNXT_QPLIB_SWQE_TYPE_RDMA_READ_V3:
		wc->status = IB_WC_GENERAL_ERR;
		return;
	default:
		wc->opcode = IB_WC_SEND;
		break;
	}

	wc->status = __req_to_ib_wc_status(cqe->status);
}

static int bnxt_re_check_packet_type(u16 raweth_qp1_flags, u16 raweth_qp1_flags2)
{
	bool is_udp = false, is_ipv6 = false, is_ipv4 = false;

	/* raweth_qp1_flags Bit 9-6 indicates itype */

	if ((raweth_qp1_flags & CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS_ITYPE_ROCE)
	    != CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS_ITYPE_ROCE)
		return -1;

	if (raweth_qp1_flags2 &
	    CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS2_IP_CS_CALC &&
	    raweth_qp1_flags2 &
	    CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS2_L4_CS_CALC) {
		is_udp = true;
		/* raweth_qp1_flags2 Bit 8 indicates ip_type. 0-v4 1 - v6 */
		(raweth_qp1_flags2 &
		 CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS2_IP_TYPE) ?
			(is_ipv6 = true) : (is_ipv4 = true);
		return ((is_ipv6) ?
			 BNXT_RE_ROCEV2_IPV6_PACKET :
			 BNXT_RE_ROCEV2_IPV4_PACKET);
	} else {
		return BNXT_RE_ROCE_V1_PACKET;
	}
}

#ifdef ENABLE_ROCEV2_QP1
static int bnxt_re_to_ib_nw_type(int nw_type)
{
	u8 nw_hdr_type = 0xFF;

	switch (nw_type) {
	case BNXT_RE_ROCE_V1_PACKET:
		nw_hdr_type = RDMA_NETWORK_ROCE_V1;
		break;
	case BNXT_RE_ROCEV2_IPV4_PACKET:
		nw_hdr_type = RDMA_NETWORK_IPV4;
		break;
	case BNXT_RE_ROCEV2_IPV6_PACKET:
		nw_hdr_type = RDMA_NETWORK_IPV6;
		break;
	}
	return nw_hdr_type;
}
#endif

static bool bnxt_re_is_loopback_packet(struct bnxt_re_dev *rdev,
					    void *rq_hdr_buf)
{
	u8 *tmp_buf = NULL;
	struct ethhdr *eth_hdr;
	u16 eth_type;
	bool rc = false;

	tmp_buf = (u8 *)rq_hdr_buf;
	/*
	 * If dest mac is not same as I/F mac, this could be a
	 * loopback address or multicast address, check whether
	 * it is a loopback packet
	 */
	if (!ether_addr_equal(tmp_buf, rdev->netdev->dev_addr)) {
		tmp_buf += 4;
		eth_hdr = (struct ethhdr *)tmp_buf;
		eth_type = ntohs(eth_hdr->h_proto);
		switch (eth_type) {
		case BNXT_QPLIB_ETHTYPE_ROCEV1:
			rc = true;
			break;
#ifdef ENABLE_ROCEV2_QP1
		case ETH_P_IP:
		case ETH_P_IPV6: {
			u32 len;
			struct udphdr *udp_hdr;

			len = (eth_type == ETH_P_IP ? sizeof(struct iphdr) :
						      sizeof(struct ipv6hdr));
			tmp_buf += sizeof(struct ethhdr) + len;
			udp_hdr = (struct udphdr *)tmp_buf;
			rc = ntohs(udp_hdr->dest) == ROCE_V2_UDP_DPORT;
			}
			break;
#endif
		default:
			break;
		}
	}

	return rc;
}

static bool bnxt_re_is_vlan_in_packet(struct bnxt_re_dev *rdev,
				      void *rq_hdr_buf,
				      struct bnxt_qplib_cqe *cqe)
{
	struct vlan_hdr *vlan_hdr;
	struct ethhdr *eth_hdr;
	u8 *tmp_buf = NULL;
	u16 eth_type;

	tmp_buf = (u8 *)rq_hdr_buf;
	eth_hdr = (struct ethhdr *)tmp_buf;
	eth_type = ntohs(eth_hdr->h_proto);
	if (eth_type == ETH_P_8021Q) {
		tmp_buf += sizeof(struct ethhdr);
		vlan_hdr = (struct vlan_hdr *)tmp_buf;
		cqe->raweth_qp1_metadata =
			ntohs(vlan_hdr->h_vlan_TCI) |
			(eth_type <<
			 CQ_RES_RAWETH_QP1_RAWETH_QP1_METADATA_TPID_SFT);
		cqe->raweth_qp1_flags2 |=
			CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS2_META_FORMAT_VLAN;
		return true;
	}

	return false;
}

static int bnxt_re_process_raw_qp_packet_receive(struct bnxt_re_qp *gsi_qp,
						 struct bnxt_qplib_cqe *cqe)
{
	struct bnxt_re_sqp_entries *sqp_entry = NULL;
	struct bnxt_qplib_hdrbuf *hdr_buf;
	dma_addr_t shrq_hdr_buf_map;
	struct ib_sge s_sge[2] = {};
	struct ib_sge r_sge[2] = {};
	struct ib_recv_wr rwr = {};
	struct bnxt_re_ah *gsi_sah;
	struct bnxt_re_qp *gsi_sqp;
	dma_addr_t rq_hdr_buf_map;
	struct bnxt_re_dev *rdev;
	struct ib_send_wr *swr;
	u32 skip_bytes = 0;
	void *rq_hdr_buf;
	int pkt_type = 0;
	u32 offset = 0;
	u32 tbl_idx;
	int rc;
#ifdef HAVE_IB_UD_WR
	struct ib_ud_wr udwr = {};
#else
	struct ib_send_wr udwr = {};
#endif

#ifdef HAVE_IB_UD_WR
	swr = &udwr.wr;
#else
	swr = &udwr;
#endif
	rdev = gsi_qp->rdev;
	gsi_sqp = rdev->gsi_ctx.gsi_sqp;
	tbl_idx = cqe->wr_id;

	hdr_buf = gsi_qp->qplib_qp.rq_hdr_buf;
	rq_hdr_buf = hdr_buf->va + tbl_idx * hdr_buf->step;
	rq_hdr_buf_map = bnxt_qplib_get_qp_buf_from_index(&gsi_qp->qplib_qp,
							  tbl_idx);
	/* Shadow QP header buffer */
	shrq_hdr_buf_map = bnxt_qplib_get_qp_buf_from_index(&gsi_sqp->qplib_qp,
							    tbl_idx);
	sqp_entry = &rdev->gsi_ctx.sqp_tbl[tbl_idx];

	pkt_type = bnxt_re_check_packet_type(cqe->raweth_qp1_flags,
					     cqe->raweth_qp1_flags2);
	if (pkt_type < 0) {
		dev_err(rdev_to_dev(rdev), "Not handling this packet\n");
		return -EINVAL;
	}

	/* Adjust the offset for the user buffer and post in the rq */

	if (pkt_type == BNXT_RE_ROCEV2_IPV4_PACKET)
		offset = 20;

	/*
	 * QP1 loopback packet has 4 bytes of internal header before
	 * ether header. Skip these four bytes.
	 */
	if (bnxt_re_is_loopback_packet(rdev, rq_hdr_buf))
		skip_bytes = 4;

	if (bnxt_re_is_vlan_in_packet(rdev, rq_hdr_buf, cqe))
		skip_bytes += VLAN_HLEN;

	/* Store this cqe */
	memcpy(&sqp_entry->cqe, cqe, sizeof(struct bnxt_qplib_cqe));
	sqp_entry->qp1_qp = gsi_qp;

	/* First send SGE . Skip the ether header*/
	s_sge[0].addr = rq_hdr_buf_map + BNXT_QPLIB_MAX_QP1_RQ_ETH_HDR_SIZE
			+ skip_bytes;
	s_sge[0].lkey = 0xFFFFFFFF;
	s_sge[0].length = offset ? BNXT_QPLIB_MAX_GRH_HDR_SIZE_IPV4 :
				BNXT_QPLIB_MAX_GRH_HDR_SIZE_IPV6;

	/* Second Send SGE */
	s_sge[1].addr = s_sge[0].addr + s_sge[0].length +
			BNXT_QPLIB_MAX_QP1_RQ_BDETH_HDR_SIZE;
	if (pkt_type != BNXT_RE_ROCE_V1_PACKET)
		s_sge[1].addr += 8;
	s_sge[1].lkey = 0xFFFFFFFF;
	s_sge[1].length = 256;

	/* First recv SGE */
	r_sge[0].addr = shrq_hdr_buf_map;
	r_sge[0].lkey = 0xFFFFFFFF;
	r_sge[0].length = 40;

	r_sge[1].addr = sqp_entry->sge.addr + offset;
	r_sge[1].lkey = sqp_entry->sge.lkey;
	r_sge[1].length = BNXT_QPLIB_MAX_GRH_HDR_SIZE_IPV6 + 256 - offset;

	/* Create receive work request */
	rwr.num_sge = 2;
	rwr.sg_list = r_sge;
	rwr.wr_id = tbl_idx;
	rwr.next = NULL;

	rc = bnxt_re_post_recv_shadow_qp(rdev, gsi_sqp, &rwr);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to post Rx buffers to shadow QP");
		return -ENOMEM;
	}

	swr->num_sge = 2;
	swr->sg_list = s_sge;
	swr->wr_id = tbl_idx;
	swr->opcode = IB_WR_SEND;
	swr->next = NULL;

	gsi_sah = rdev->gsi_ctx.gsi_sah;
#ifdef HAVE_IB_UD_WR
	udwr.ah = &gsi_sah->ib_ah;
	udwr.remote_qpn = gsi_sqp->qplib_qp.id;
	udwr.remote_qkey = gsi_sqp->qplib_qp.qkey;
#else
	udwr.wr.ud.ah = &gsi_sah->ib_ah;
	udwr.wr.ud.remote_qpn = gsi_sqp->qplib_qp.id;
	udwr.wr.ud.remote_qkey = gsi_sqp->qplib_qp.qkey;
#endif
	/* post data received in the send queue */
	rc = bnxt_re_post_send_shadow_qp(rdev, gsi_sqp, swr);

	return rc;
}

static void bnxt_re_process_res_rawqp1_wc(struct ib_wc *wc,
					  struct bnxt_qplib_cqe *cqe)
{
	wc->opcode = IB_WC_RECV;
	wc->status = __rawqp1_to_ib_wc_status(cqe->status);
	wc->wc_flags |= IB_WC_GRH;
}

static void bnxt_re_process_res_rc_wc(struct ib_wc *wc,
				      struct bnxt_qplib_cqe *cqe)
{
	wc->opcode = IB_WC_RECV;
	wc->status = __rc_to_ib_wc_status(cqe->status);

	if (cqe->flags & CQ_RES_RC_FLAGS_IMM)
		wc->wc_flags |= IB_WC_WITH_IMM;
	if (cqe->flags & CQ_RES_RC_FLAGS_INV)
		wc->wc_flags |= IB_WC_WITH_INVALIDATE;
	if ((cqe->flags & (CQ_RES_RC_FLAGS_RDMA | CQ_RES_RC_FLAGS_IMM)) ==
	    (CQ_RES_RC_FLAGS_RDMA | CQ_RES_RC_FLAGS_IMM))
		wc->opcode = IB_WC_RECV_RDMA_WITH_IMM;
}

/* Returns TRUE if pkt has valid VLAN and if VLAN id is non-zero */
static bool bnxt_re_is_nonzero_vlanid_pkt(struct bnxt_qplib_cqe *orig_cqe,
					  u16 *vid, u8 *sl)
{
	u32 metadata;
	u16 tpid;
	bool ret = false;
	metadata = orig_cqe->raweth_qp1_metadata;
	if (orig_cqe->raweth_qp1_flags2 &
	    CQ_RES_RAWETH_QP1_RAWETH_QP1_FLAGS2_META_FORMAT_VLAN) {
		tpid = ((metadata &
			 CQ_RES_RAWETH_QP1_RAWETH_QP1_METADATA_TPID_MASK) >>
			 CQ_RES_RAWETH_QP1_RAWETH_QP1_METADATA_TPID_SFT);
		if (tpid == ETH_P_8021Q) {
			*vid = metadata &
			       CQ_RES_RAWETH_QP1_RAWETH_QP1_METADATA_VID_MASK;
			*sl = (metadata &
			       CQ_RES_RAWETH_QP1_RAWETH_QP1_METADATA_PRI_MASK) >>
			       CQ_RES_RAWETH_QP1_RAWETH_QP1_METADATA_PRI_SFT;
			ret = !!(*vid);
		}
	}

	return ret;
}

static void bnxt_re_process_res_shadow_qp_wc(struct bnxt_re_qp *gsi_sqp,
					     struct ib_wc *wc,
					     struct bnxt_qplib_cqe *cqe)
{
	u32 tbl_idx;
	struct bnxt_re_dev *rdev = gsi_sqp->rdev;
	struct bnxt_re_qp *gsi_qp = NULL;
	struct bnxt_qplib_cqe *orig_cqe = NULL;
	struct bnxt_re_sqp_entries *sqp_entry = NULL;
	int nw_type;
	u16 vlan_id;
	u8 sl;

	tbl_idx = cqe->wr_id;

	sqp_entry = &rdev->gsi_ctx.sqp_tbl[tbl_idx];
	gsi_qp = sqp_entry->qp1_qp;
	orig_cqe = &sqp_entry->cqe;

	wc->wr_id = sqp_entry->wrid;
	/* TODO Check whether this needs to be altered.*/
	wc->byte_len = orig_cqe->length;
	wc->qp = &gsi_qp->ib_qp;

	wc->ex.imm_data = cpu_to_be32(le32_to_cpu(orig_cqe->immdata));
	wc->src_qp = orig_cqe->src_qp;
#ifdef HAVE_IB_WC_SMAC
	memcpy(wc->smac, orig_cqe->smac, ETH_ALEN);
#endif
	if (bnxt_re_is_nonzero_vlanid_pkt(orig_cqe, &vlan_id, &sl)) {
		if (bnxt_re_check_if_vlan_valid(rdev, vlan_id)) {
			wc->sl = sl;
#ifdef HAVE_IB_WC_VLAN_ID
			wc->vlan_id = vlan_id;
			wc->wc_flags |= IB_WC_WITH_VLAN;
#endif
		}
	}
	wc->port_num = 1;
	wc->vendor_err = orig_cqe->status;

	wc->opcode = IB_WC_RECV;
	wc->status = __rawqp1_to_ib_wc_status(orig_cqe->status);
	wc->wc_flags |= IB_WC_GRH;

	nw_type = bnxt_re_check_packet_type(orig_cqe->raweth_qp1_flags,
					    orig_cqe->raweth_qp1_flags2);
	dev_dbg(rdev_to_dev(rdev), "%s nw_type = %d\n", __func__, nw_type);
#ifdef ENABLE_ROCEV2_QP1
	if (nw_type >= 0) {
		wc->network_hdr_type = bnxt_re_to_ib_nw_type(nw_type);
		wc->wc_flags |= IB_WC_WITH_NETWORK_HDR_TYPE;
	}
#endif
}

static void bnxt_re_process_res_ud_wc(struct bnxt_re_dev *rdev,
				      struct bnxt_re_qp *qp, struct ib_wc *wc,
				      struct bnxt_qplib_cqe *cqe)
{
#ifdef ENABLE_ROCEV2_QP1
	u8 nw_type;
#endif
	u16 vlan_id = 0;

	wc->opcode = IB_WC_RECV;
	if (_is_hsi_v3(rdev->chip_ctx)) {
		wc->status = __rc_v3_to_ib_wc_status(cqe->status);
		if (cqe->flags & CQ_RES_UD_V3_FLAGS_IMM)
			wc->wc_flags |= IB_WC_WITH_IMM;
		if (cqe->flags & CQ_RES_RC_V3_FLAGS_INV)
			wc->wc_flags |= IB_WC_WITH_INVALIDATE;
	} else {
		wc->status = __rc_to_ib_wc_status(cqe->status);
		if (cqe->flags & CQ_RES_UD_FLAGS_IMM)
			wc->wc_flags |= IB_WC_WITH_IMM;
		if (cqe->flags & CQ_RES_RC_FLAGS_INV)
			wc->wc_flags |= IB_WC_WITH_INVALIDATE;
	}
	/* report only on GSI QP for Thor */
	if (rdev->gsi_ctx.gsi_qp->qplib_qp.id == qp->qplib_qp.id &&
	    rdev->gsi_ctx.gsi_qp_mode == BNXT_RE_GSI_MODE_UD) {
		wc->wc_flags |= IB_WC_GRH;
#ifdef HAVE_IB_WC_SMAC
		memcpy(wc->smac, cqe->smac, ETH_ALEN);
		wc->wc_flags |= IB_WC_WITH_SMAC;
#endif
		if (_is_cqe_v2_supported(rdev->dev_attr->dev_cap_flags)) {
			if (cqe->flags & CQ_RES_UD_V2_FLAGS_META_FORMAT_MASK) {
				if (cqe->cfa_meta &
				    BNXT_QPLIB_CQE_CFA_META1_VALID)
					vlan_id = (cqe->cfa_meta & 0xFFF);
			}
		} else if (cqe->flags & CQ_RES_UD_FLAGS_META_FORMAT_VLAN) {
			vlan_id = (cqe->cfa_meta & 0xFFF);
		}
		if (_is_hsi_v3(rdev->chip_ctx))
			vlan_id = 0;
#ifdef HAVE_IB_WC_VLAN_ID
		/* Mark only if vlan_id is non zero */
		if (vlan_id && bnxt_re_check_if_vlan_valid(rdev, vlan_id)) {
			wc->vlan_id = vlan_id;
			wc->wc_flags |= IB_WC_WITH_VLAN;
		}
#endif
#ifdef	ENABLE_ROCEV2_QP1
		nw_type = (cqe->flags >> 4) & 0x3;
		wc->network_hdr_type = bnxt_re_to_ib_nw_type(nw_type);
		wc->wc_flags |= IB_WC_WITH_NETWORK_HDR_TYPE;
#endif
	}
}

static int bnxt_re_legacy_send_phantom_wqe(struct bnxt_re_qp *qp)
{
	struct bnxt_qplib_qp *lib_qp = &qp->qplib_qp;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&qp->sq_lock, flags);

	rc = bnxt_re_legacy_bind_fence_mw(lib_qp);
	if (!rc) {
		lib_qp->sq.phantom_wqe_cnt++;
		dev_dbg(&lib_qp->sq.hwq.pdev->dev,
			"qp %#x sq->prod %#x sw_prod %#x phantom_wqe_cnt %d\n",
			lib_qp->id, lib_qp->sq.hwq.prod,
			HWQ_CMP(lib_qp->sq.hwq.prod, &lib_qp->sq.hwq),
			lib_qp->sq.phantom_wqe_cnt);
	}

	spin_unlock_irqrestore(&qp->sq_lock, flags);
	return rc;
}

int bnxt_re_poll_cq(struct ib_cq *ib_cq, int num_entries, struct ib_wc *wc)
{
	struct bnxt_re_cq *cq = to_bnxt_re(ib_cq, struct bnxt_re_cq, ib_cq);
	int i, ncqe, budget, init_budget = 0;
	struct bnxt_re_dev *rdev = cq->rdev;
	struct bnxt_re_qp *qp;
	struct bnxt_qplib_cqe *cqe;
	struct bnxt_qplib_q *sq;
	struct bnxt_qplib_qp *lib_qp;
	u32 tbl_idx;
        struct bnxt_re_sqp_entries *sqp_entry = NULL;
	bool dump_ib_wc = false;
	char prefix_str[50];
	unsigned long flags;
	u8 gsi_mode;

	/*
	 * DB software CQ; only process the door bell pacing alert from
	 * the user lib
	 */
	if (cq->is_dbr_soft_cq) {
		bnxt_re_pacing_alert(rdev);
		return 0;
	}

	/* User CQ; the only processing we do is to
	 * complete any pending CQ resize operation.
	 */
	if (cq->umem) {
		if (cq->resize_umem)
			bnxt_re_resize_cq_complete(cq);
		return 0;
	}

	spin_lock_irqsave(&cq->cq_lock, flags);

	budget = min_t(u32, num_entries, cq->max_cql);
	init_budget = budget;
	if (!cq->cql) {
		dev_err(rdev_to_dev(rdev), "POLL CQ no CQL to use");
		goto exit;
	}
	cqe = &cq->cql[0];
	gsi_mode = rdev->gsi_ctx.gsi_qp_mode;
	while (budget) {
		lib_qp = NULL;
		ncqe = bnxt_qplib_poll_cq(&cq->qplib_cq, cqe, budget, &lib_qp);
		if (lib_qp) {
			sq = &lib_qp->sq;
			if (sq->legacy_send_phantom == true) {
				qp = container_of(lib_qp, struct bnxt_re_qp, qplib_qp);
				if (bnxt_re_legacy_send_phantom_wqe(qp) == -ENOMEM)
					dev_err(rdev_to_dev(rdev),
						"Phantom failed! Scheduled to send again\n");
				else
					sq->legacy_send_phantom = false;
			}
		}
		if (ncqe < budget)
			ncqe += bnxt_qplib_process_flush_list(&cq->qplib_cq,
							      cqe + ncqe,
							      budget - ncqe);

		if (!ncqe)
			break;

		for (i = 0; i < ncqe; i++, cqe++) {
			/* Transcribe each qplib_wqe back to ib_wc */
			memset(wc, 0, sizeof(*wc));

			wc->wr_id = cqe->wr_id;
			wc->byte_len = cqe->length;
			qp = to_bnxt_re((struct bnxt_qplib_qp *)cqe->qp_handle,
					struct bnxt_re_qp, qplib_qp);
			if (!qp) {
				dev_err(rdev_to_dev(rdev),
					"POLL CQ bad QP handle");
				continue;
			}
			wc->qp = &qp->ib_qp;
			if (cqe->flags & CQ_RES_RC_FLAGS_IMM)
				wc->ex.imm_data = cpu_to_be32(cqe->immdata);
			else
				wc->ex.imm_data = cqe->invrkey;
			wc->src_qp = cqe->src_qp;
#ifdef HAVE_IB_WC_SMAC
			memcpy(wc->smac, cqe->smac, ETH_ALEN);
#endif
			wc->port_num = 1;
			wc->vendor_err = cqe->status;

			switch(cqe->opcode) {
			case CQ_BASE_CQE_TYPE_REQ:
			case CQ_BASE_CQE_TYPE_REQ_V3:
				if (gsi_mode == BNXT_RE_GSI_MODE_ALL &&
				    qp->qplib_qp.id ==
				    rdev->gsi_ctx.gsi_sqp->qplib_qp.id) {
					/* Handle this completion with
					 * the stored completion */
					 dev_dbg(rdev_to_dev(rdev),
						 "Skipping this UD Send CQ\n");
					memset(wc, 0, sizeof(*wc));
					continue;
				}
				bnxt_re_process_req_wc(rdev, wc, cqe);
				if ((cqe->status != CQ_REQ_STATUS_OK) &&
				    (cqe->status != CQ_REQ_STATUS_WORK_REQUEST_FLUSHED_ERR))
					dump_ib_wc = true;
				break;
			case CQ_BASE_CQE_TYPE_RES_RAWETH_QP1:
				if (gsi_mode == BNXT_RE_GSI_MODE_ALL) {
					if (!cqe->status) {
						int rc = 0;
						rc = bnxt_re_process_raw_qp_packet_receive(qp, cqe);
						if (!rc) {
							memset(wc, 0,
							       sizeof(*wc));
							continue;
						}
						/* TODO Respond with error to the stack */
						cqe->status = -1;
					}
					/* Errors need not be looped back.
					 * But change the wr_id to the one
					 * stored in the table
					 */
					tbl_idx = cqe->wr_id;
					sqp_entry = &rdev->gsi_ctx.sqp_tbl[tbl_idx];
					wc->wr_id = sqp_entry->wrid;
				}

				bnxt_re_process_res_rawqp1_wc(wc, cqe);
				if ((cqe->status != CQ_RES_RC_STATUS_OK) &&
				    (cqe->status != CQ_RES_RAWETH_QP1_STATUS_WORK_REQUEST_FLUSHED_ERR) &&
				    (cqe->status != CQ_RES_RAWETH_QP1_STATUS_HW_FLUSH_ERR))
					dump_ib_wc = true;
				break;
			case CQ_BASE_CQE_TYPE_RES_RC:
				bnxt_re_process_res_rc_wc(wc, cqe);
				if ((cqe->status != CQ_RES_RC_STATUS_OK) &&
				    (cqe->status != CQ_RES_RC_STATUS_WORK_REQUEST_FLUSHED_ERR) &&
				    (cqe->status != CQ_RES_RC_STATUS_HW_FLUSH_ERR))
					dump_ib_wc = true;
				break;
			case CQ_BASE_CQE_TYPE_RES_UD:
			case CQ_BASE_CQE_TYPE_RES_UD_V3:
				if (gsi_mode == BNXT_RE_GSI_MODE_ALL &&
				    qp->qplib_qp.id ==
				    rdev->gsi_ctx.gsi_sqp->qplib_qp.id) {
					/* Handle this completion with
					 * the stored completion
					 */
					dev_dbg(rdev_to_dev(rdev),
						"Handling the UD receive CQ\n");
					if (cqe->status) {
						/* TODO handle this completion  as a failure in
						 * loopback porocedure
						 */
						continue;
					} else {
						bnxt_re_process_res_shadow_qp_wc(qp, wc, cqe);
						break;
					}
				}
				bnxt_re_process_res_ud_wc(rdev, qp, wc, cqe);
				if ((cqe->status != CQ_RES_UD_STATUS_OK) &&
				    (cqe->status != CQ_RES_UD_STATUS_WORK_REQUEST_FLUSHED_ERR) &&
				    (cqe->status != CQ_RES_UD_STATUS_HW_FLUSH_ERR))
					dump_ib_wc = true;
				break;
			default:
				dev_err(rdev_to_dev(cq->rdev),
					"POLL CQ type 0x%x not handled, skip!",
					cqe->opcode);
				continue;
			}
			if (dump_ib_wc) {
				snprintf(&prefix_str[0], sizeof(prefix_str),
					 "qp_id 0x%x vendor_err 0x%x ib_wc: ", wc->vendor_err,
					 qp->qplib_qp.id);
				print_hex_dump(KERN_WARNING, &prefix_str[0],
					       DUMP_PREFIX_OFFSET, 16, 2, wc,
					       sizeof(struct ib_wc), false);
			}
			dump_ib_wc = false;
			wc++;
			budget--;
		}
	}
exit:
	spin_unlock_irqrestore(&cq->cq_lock, flags);
	return init_budget - budget;
}

int bnxt_re_req_notify_cq(struct ib_cq *ib_cq,
			  enum ib_cq_notify_flags ib_cqn_flags)
{
	struct bnxt_re_cq *cq = to_bnxt_re(ib_cq, struct bnxt_re_cq, ib_cq);
	int type = 0, rc = 0;
	unsigned long flags;

	spin_lock_irqsave(&cq->cq_lock, flags);
	/* Trigger on the very next completion */
	if (ib_cqn_flags & IB_CQ_NEXT_COMP)
		type = DBC_DBC_TYPE_CQ_ARMALL;
	/* Trigger on the next solicited completion */
	else if (ib_cqn_flags & IB_CQ_SOLICITED)
		type = DBC_DBC_TYPE_CQ_ARMSE;

	/* Poll to see if there are missed events */
	if ((ib_cqn_flags & IB_CQ_REPORT_MISSED_EVENTS) &&
	    !(bnxt_qplib_is_cq_empty(&cq->qplib_cq)))
                rc = 1;
	else
		bnxt_qplib_req_notify_cq(&cq->qplib_cq, type);

	spin_unlock_irqrestore(&cq->cq_lock, flags);

	return rc;
}

/* Memory Regions */
struct ib_mr *bnxt_re_get_dma_mr(struct ib_pd *ib_pd, int mr_access_flags)
{
	struct bnxt_qplib_mrinfo mrinfo;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_mr *mr;
	struct bnxt_re_pd *pd;
	u32 max_mr_count;
	u64 pbl = 0;
	int rc;

	memset(&mrinfo, 0, sizeof(mrinfo));
	pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ib_pd);
	rdev = pd->rdev;
	dev_dbg(rdev_to_dev(rdev), "Get DMA MR");

	if (bnxt_re_get_total_mr_mw_count(rdev) >= rdev->dev_attr->max_mr)
		return ERR_PTR(-ENOMEM);

	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);
	mr->rdev = rdev;
	mr->qplib_mr.pd = &pd->qplib_pd;
	mr->qplib_mr.access_flags = __from_ib_access_flags(mr_access_flags);
	mr->qplib_mr.type = CMDQ_ALLOCATE_MRW_MRW_FLAGS_PMR;

	/* Allocate and register 0 as the address */
	rc = bnxt_qplib_alloc_mrw(&rdev->qplib_res, &mr->qplib_mr);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Allocate DMA MR failed!");
		goto fail;
	}
	mr->qplib_mr.total_size = -1; /* Infinite length */
	mrinfo.ptes = &pbl;
	mrinfo.sg.npages = 0;
	mrinfo.sg.pgsize = PAGE_SIZE;
	mrinfo.sg.pgshft = PAGE_SHIFT;
	mrinfo.sg.pgsize = PAGE_SIZE;
	mrinfo.mrw = &mr->qplib_mr;
	mrinfo.is_dma = true;
#ifdef HAVE_IB_ACCESS_RELAXED_ORDERING
	if (mr_access_flags & IB_ACCESS_RELAXED_ORDERING)
		bnxt_re_check_and_set_relaxed_ordering(rdev, &mrinfo);
#endif
	rc = bnxt_qplib_reg_mr(&rdev->qplib_res, &mrinfo, false);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Register DMA MR failed!");
		goto fail_mr;
	}
	mr->ib_mr.lkey = mr->qplib_mr.lkey;
	if (mr_access_flags & (IB_ACCESS_REMOTE_WRITE | IB_ACCESS_REMOTE_READ |
			       IB_ACCESS_REMOTE_ATOMIC))
		mr->ib_mr.rkey = mr->ib_mr.lkey;
	atomic_inc(&rdev->stats.rsors.mr_count);
	max_mr_count =  atomic_read(&rdev->stats.rsors.mr_count);
	if (max_mr_count > atomic_read(&rdev->stats.rsors.max_mr_count))
		atomic_set(&rdev->stats.rsors.max_mr_count, max_mr_count);
	BNXT_RE_RES_LIST_ADD(rdev, mr, BNXT_RE_RES_TYPE_MR);

	return &mr->ib_mr;

fail_mr:
	bnxt_qplib_free_mrw(&rdev->qplib_res, &mr->qplib_mr, 0, NULL);
fail:
	kfree(mr);
	return ERR_PTR(rc);
}

/*
 * bnxt_re_capture_mrdump - Capture PBL list of MemoryRegion
 * @mr	- Pointer to MR for which data has to be collected
 *
 * This function will capture PBL list of a MR which
 * can be used to debug any issue
 *
 * Returns: Nothing. It is a void routine
 */
static void bnxt_re_capture_mrdump(struct bnxt_re_mr *mr)
{
	struct bnxt_re_dev *rdev = mr->rdev;
	struct qdump_array *qdump;

	if ((rdev->snapdump_dbg_lvl == BNXT_RE_SNAPDUMP_NONE) ||
	    (rdev->snapdump_dbg_lvl == BNXT_RE_SNAPDUMP_ERR))
		return;

	if (!rdev->qdump_head.qdump)
		return;

	mutex_lock(&rdev->qdump_head.lock);
	qdump = bnxt_re_get_next_qpdump(rdev);
	if (!qdump) {
		mutex_unlock(&rdev->qdump_head.lock);
		return;
	}

	qdump->mrinfo.total_size = mr->qplib_mr.total_size;
	qdump->mrinfo.mr_handle = mr->qplib_mr.mr_handle;
	qdump->mrinfo.type = mr->qplib_mr.type;
	qdump->mrinfo.lkey = mr->qplib_mr.lkey;
	qdump->mrinfo.rkey = mr->qplib_mr.rkey;
	qdump->mrd.hwq = &mr->qplib_mr.hwq;
	bnxt_re_copy_qdump_pbl(&qdump->mrd);

	qdump->valid = true;
	qdump->is_mr = true;
	mutex_unlock(&rdev->qdump_head.lock);
}

int bnxt_re_dereg_mr(struct ib_mr *ib_mr

#ifdef HAVE_DEREG_MR_UDATA
		, struct ib_udata *udata
#endif
		)
{
	struct bnxt_re_mr *mr = to_bnxt_re(ib_mr, struct bnxt_re_mr, ib_mr);
	struct bnxt_re_dev *rdev = mr->rdev;
	struct bnxt_qplib_mrw *qplib_mr;
	void *ctx_sb_data = NULL;
	u16 ctx_size;
	int rc = 0;

	bnxt_re_capture_mrdump(mr);

#ifdef CONFIG_INFINIBAND_PEER_MEM
	if (atomic_inc_return(&mr->invalidated) > 1) {
		/* The peer is in the process of invalidating the MR.
		 * Wait for it to finish.
		 */
		wait_for_completion(&mr->invalidation_comp);
	} else {
#endif
		qplib_mr = &mr->qplib_mr;
		ctx_size = qplib_mr->ctx_size_sb;
		if (ctx_size)
			ctx_sb_data = vzalloc(ctx_size);

		rc = bnxt_qplib_free_mrw(&rdev->qplib_res, qplib_mr,
					 ctx_size, ctx_sb_data);

		if (!rc)
			bnxt_re_save_resource_context(rdev, ctx_sb_data,
						      BNXT_RE_RES_TYPE_MR, 0);
		else
			dev_err_ratelimited(rdev_to_dev(rdev),
					    "Dereg MR failed (%d): rc - %#x\n",
					    mr->qplib_mr.lkey, rc);

		vfree(ctx_sb_data);

#ifdef CONFIG_INFINIBAND_PEER_MEM
	}
#endif

#ifdef HAVE_IB_ALLOC_MR
	if (mr->pages) {
		bnxt_qplib_free_fast_reg_page_list(&rdev->qplib_res,
						   &mr->qplib_frpl);
		kfree(mr->pages);
		mr->npages = 0;
		mr->pages = NULL;
	}
#endif
	if (!IS_ERR(mr->ib_umem) && mr->ib_umem) {
#ifdef HAVE_IB_UMEM_STOP_INVALIDATION
		if (mr->is_invalcb_active)
			ib_umem_stop_invalidation_notifier(mr->ib_umem);
#endif
		mr->is_invalcb_active = false;
#ifdef CONFIG_INFINIBAND_PEER_MEM
		if (mr->is_peer_mem)
			atomic_dec(&rdev->stats.rsors.peer_mem_mr_count);
#endif
		bnxt_re_peer_mem_release(mr->ib_umem);
	}

	BNXT_RE_RES_LIST_DEL(rdev, mr, BNXT_RE_RES_TYPE_MR);
	atomic_dec(&rdev->stats.rsors.mr_count);
	if (mr->is_dmabuf)
		atomic_dec(&rdev->stats.rsors.mr_dmabuf_count);
	kfree(mr);
	return 0;
}

#ifdef HAVE_IB_MAP_MR_SG
static int bnxt_re_set_page(struct ib_mr *ib_mr, u64 addr)
{
	struct bnxt_re_mr *mr = to_bnxt_re(ib_mr, struct bnxt_re_mr, ib_mr);

	if (unlikely(mr->npages == mr->qplib_frpl.max_pg_ptrs))
		return -ENOMEM;

	mr->pages[mr->npages++] = addr;
	dev_dbg(NULL, "%s: ibdev %p Set MR pages[%d] = 0x%llx",
		ROCE_DRV_MODULE_NAME, ib_mr->device, mr->npages - 1,
		mr->pages[mr->npages - 1]);
	return 0;
}

int bnxt_re_map_mr_sg(struct ib_mr *ib_mr, struct scatterlist *sg, int sg_nents
#ifdef HAVE_IB_MAP_MR_SG_PAGE_SIZE
		      , unsigned int *sg_offset
#else
#ifdef HAVE_IB_MAP_MR_SG_OFFSET
		      , unsigned int sg_offset
#endif
#endif
	)
{
	struct bnxt_re_mr *mr = to_bnxt_re(ib_mr, struct bnxt_re_mr, ib_mr);

	dev_dbg(NULL, "%s: ibdev %p Map MR sg nents = %d", ROCE_DRV_MODULE_NAME,
		ib_mr->device, sg_nents);
	mr->npages = 0;
	return ib_sg_to_pages(ib_mr, sg, sg_nents,
#ifdef HAVE_IB_MAP_MR_SG_OFFSET
			      sg_offset,
#endif
			      bnxt_re_set_page);
}
#endif

#ifdef HAVE_IB_ALLOC_MR
struct ib_mr *bnxt_re_alloc_mr(struct ib_pd *ib_pd, enum ib_mr_type type,
			       u32 max_num_sg
#ifdef HAVE_ALLOC_MR_UDATA
			       , struct ib_udata *udata
#endif
			       )
{
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ib_pd);
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_re_mr *mr;
	u32 max_mr_count;
	int rc;

	dev_dbg(rdev_to_dev(rdev), "Alloc MR");
	if (type != IB_MR_TYPE_MEM_REG) {
		dev_dbg(rdev_to_dev(rdev), "MR type 0x%x not supported", type);
		return ERR_PTR(-EINVAL);
	}
	if (max_num_sg > MAX_PBL_LVL_1_PGS) {
		dev_dbg(rdev_to_dev(rdev), "Max SG exceeded");
		return ERR_PTR(-EINVAL);
	}

	if (bnxt_re_get_total_mr_mw_count(rdev) >= rdev->dev_attr->max_mr)
		return ERR_PTR(-ENOMEM);

	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);
	mr->rdev = rdev;
	mr->qplib_mr.pd = &pd->qplib_pd;
	mr->qplib_mr.access_flags = BNXT_QPLIB_FR_PMR;
	mr->qplib_mr.type = CMDQ_ALLOCATE_MRW_MRW_FLAGS_PMR;

	rc = bnxt_qplib_alloc_mrw(&rdev->qplib_res, &mr->qplib_mr);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Allocate MR failed!");
		goto fail;
	}
	mr->ib_mr.lkey = mr->qplib_mr.lkey;
	mr->ib_mr.rkey = mr->ib_mr.lkey;
	mr->pages = kzalloc(sizeof(u64) * max_num_sg, GFP_KERNEL);
	if (!mr->pages) {
		rc = -ENOMEM;
		goto fail_mr;
	}
	rc = bnxt_qplib_alloc_fast_reg_page_list(&rdev->qplib_res,
						 &mr->qplib_frpl, max_num_sg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Allocate HW Fast reg page list failed!");
		goto free_page;
	}
	dev_dbg(rdev_to_dev(rdev), "Alloc MR pages = 0x%p", mr->pages);

	atomic_inc(&rdev->stats.rsors.mr_count);
	max_mr_count =  atomic_read(&rdev->stats.rsors.mr_count);
	if (max_mr_count > atomic_read(&rdev->stats.rsors.max_mr_count))
		atomic_set(&rdev->stats.rsors.max_mr_count, max_mr_count);
	BNXT_RE_RES_LIST_ADD(rdev, mr, BNXT_RE_RES_TYPE_MR);

	return &mr->ib_mr;

free_page:
	kfree(mr->pages);
fail_mr:
	bnxt_qplib_free_mrw(&rdev->qplib_res, &mr->qplib_mr, 0, NULL);
fail:
	kfree(mr);
	return ERR_PTR(rc);
}
#endif

/* Memory Windows */
#ifdef HAVE_IB_MW_TYPE
ALLOC_MW_RET bnxt_re_alloc_mw
#ifndef HAVE_ALLOC_MW_IN_IB_CORE
			      (struct ib_pd *ib_pd, enum ib_mw_type type
#else
			      (struct ib_mw *ib_mw
#endif /* HAVE_ALLOC_MW_RET_IB_MW*/
#ifdef HAVE_ALLOW_MW_WITH_UDATA
			      , struct ib_udata *udata
#endif
			       )
#else
ALLOC_MW_RET bnxt_re_alloc_mw(struct ib_pd *ib_pd)
#endif
{
#ifndef HAVE_ALLOC_MW_IN_IB_CORE
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ib_pd);
#else
	struct bnxt_re_pd *pd = to_bnxt_re(ib_mw->pd, struct bnxt_re_pd, ib_pd);
	enum ib_mw_type type = ib_mw->type;
#endif
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_re_mw *mw = NULL;
	u32 max_mw_count;
	int rc;

	if (bnxt_re_get_total_mr_mw_count(rdev) >= rdev->dev_attr->max_mr) {
		rc = -ENOMEM;
		goto fail;
	}

#ifndef HAVE_ALLOC_MW_IN_IB_CORE
	mw = kzalloc(sizeof(*mw), GFP_KERNEL);
	if (!mw) {
		rc = -ENOMEM;
		goto exit;
	}
#else
	mw = to_bnxt_re(ib_mw, struct bnxt_re_mw, ib_mw);
#endif
	mw->rdev = rdev;
	mw->qplib_mw.pd = &pd->qplib_pd;

#ifdef HAVE_IB_MW_TYPE
	mw->qplib_mw.type = (type == IB_MW_TYPE_1 ?
			       CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE1 :
			       CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE2B);
#else
	mw->qplib_mw.type = CMDQ_ALLOCATE_MRW_MRW_FLAGS_MW_TYPE1;
#endif
	rc = bnxt_qplib_alloc_mrw(&rdev->qplib_res, &mw->qplib_mw);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Allocate MW failed!");
		goto fail;
	}
	mw->ib_mw.rkey = mw->qplib_mw.rkey;
	atomic_inc(&rdev->stats.rsors.mw_count);
	max_mw_count = atomic_read(&rdev->stats.rsors.mw_count);
	if (max_mw_count > atomic_read(&rdev->stats.rsors.max_mw_count))
		atomic_set(&rdev->stats.rsors.max_mw_count, max_mw_count);

#ifndef HAVE_ALLOC_MW_IN_IB_CORE
	return &mw->ib_mw;
#else
	return rc;
#endif

fail:
#ifndef HAVE_ALLOC_MW_IN_IB_CORE
	kfree(mw);
exit:
	return ERR_PTR(rc);
#else
	return rc;
#endif
}

int bnxt_re_dealloc_mw(struct ib_mw *ib_mw)
{
	struct bnxt_re_mw *mw = to_bnxt_re(ib_mw, struct bnxt_re_mw, ib_mw);
	struct bnxt_re_dev *rdev = mw->rdev;
	struct bnxt_qplib_mrw *qplib_mr;
	void *ctx_sb_data = NULL;
	u16 ctx_size;
	int rc;

	qplib_mr = &mw->qplib_mw;
	ctx_size = qplib_mr->ctx_size_sb;
	if (ctx_size)
		ctx_sb_data = vzalloc(ctx_size);

	rc = bnxt_qplib_free_mrw(&rdev->qplib_res, qplib_mr,
				 ctx_size, ctx_sb_data);

	if (!rc) {
		bnxt_re_save_resource_context(rdev, ctx_sb_data,
					      BNXT_RE_RES_TYPE_MR, 0);
		vfree(ctx_sb_data);
	} else {
		dev_err(rdev_to_dev(rdev), "Free MW failed: %#x\n", rc);
		vfree(ctx_sb_data);
		return rc;
	}

#ifndef HAVE_ALLOC_MW_IN_IB_CORE
	kfree(mw);
#endif
	atomic_dec(&rdev->stats.rsors.mw_count);
	return rc;
}

#ifdef CONFIG_INFINIBAND_PEER_MEM
#ifdef HAVE_IB_UMEM_GET_FLAGS
static void bnxt_re_invalidate_umem(void *invalidation_cookie,
				    struct ib_umem *umem,
				    unsigned long addr, size_t size)
#else
static void bnxt_re_invalidate_umem(struct ib_umem *umem,
				    void *invalidation_cookie)
#endif
{
	struct bnxt_re_mr *mr = (struct bnxt_re_mr *)invalidation_cookie;

	/*
	* This function is called under client peer lock so
	* its resources are race protected.
	*/
	if (atomic_inc_return(&mr->invalidated) > 1) {
		bnxt_re_set_inflight_invalidation_ctx(umem);
		return;
	}

	(void)bnxt_qplib_free_mrw(&mr->rdev->qplib_res, &mr->qplib_mr, 0, NULL);
	complete(&mr->invalidation_comp);
}
#endif

static int bnxt_re_page_size_ok(int page_shift)
{
	switch (page_shift) {
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_4K:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_8K:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_64K:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_2M:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_256K:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_1M:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_4M:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_256MB:
	case CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_1G:
		return 1;
	default:
		return 0;
	}
}

#if defined(HAVE_DMA_BLOCK_ITERATOR) && defined(HAVE_IB_UMEM_FIND_BEST_PGSZ)
static u32 bnxt_re_best_page_shift(struct ib_umem *umem, u64 va, u64 cmask)
{
	return __ffs(ib_umem_find_best_pgsz(umem, cmask, va));
}
#endif

static int bnxt_re_get_page_shift(struct ib_umem *umem,
				  u64 va, u64 st, u64 cmask)
{
	int pgshft;

#if defined(HAVE_DMA_BLOCK_ITERATOR) && defined(HAVE_IB_UMEM_FIND_BEST_PGSZ)
	pgshft = bnxt_re_best_page_shift(umem, va, cmask);
#elif defined(HAVE_IB_UMEM_PAGE_SHIFT)
	pgshft = umem->page_shift;
#elif defined(HAVE_IB_UMEM_PAGE_SIZE)
	pgshft = ilog2(umem->page_size);
#elif defined(HAVE_IB_UMEM_GET_FLAGS)
	pgshft = ib_umem_get_peer_page_shift(umem);
#else
	pgshft = PAGE_SHIFT;
#endif

	return pgshft;
}

static int bnxt_re_get_num_pages(struct ib_umem *umem, u64 start, u64 length, int page_shift)
{
	if (page_shift == PAGE_SHIFT)
		return ib_umem_num_pages_compat(umem);
	else
		return (ALIGN(umem->address + umem->length, BIT(page_shift)) -
				ALIGN_DOWN(umem->address, BIT(page_shift))) >> page_shift;
}

static inline unsigned long bnxt_umem_dma_offset_compat(struct ib_umem *umem,
							unsigned long pgsz)
{
#ifndef HAVE_IB_UMEM_DMA_OFFSET
#if defined HAVE_IB_UMEM_SG_APPEND_TABLE
	return (sg_dma_address(umem->sgt_append.sgt.sgl) + ib_umem_offset(umem)) & (pgsz - 1);
#elif defined HAVE_IB_UMEM_SG_TABLE
	return (sg_dma_address(umem->sg_head.sgl) + ib_umem_offset(umem)) & (pgsz - 1);
#else
	return (ib_umem_offset(umem) & (pgsz - 1));
#endif
#else
	return ib_umem_dma_offset(umem, pgsz);
#endif
}

#define SUPPORTED_PAGE_SIZE (BIT_ULL(21) | BIT_ULL(16) | BIT_ULL(13) |  BIT_ULL(12))

void bnxt_re_set_sginfo(struct bnxt_re_dev *rdev, struct ib_umem *umem,
			__u64 va, struct bnxt_qplib_sg_info *sginfo, u8 res)
{
	int page_shift;

	page_shift = bnxt_re_get_page_shift(umem, va, va, SUPPORTED_PAGE_SIZE);

	if (page_shift < PAGE_SHIFT)
		page_shift = PAGE_SHIFT;

	sginfo->pgshft = page_shift;
	sginfo->pgsize = BIT(page_shift);
	sginfo->npages = bnxt_re_get_num_pages(umem, va, 0, page_shift);
	sginfo->fwo_offset = bnxt_umem_dma_offset_compat(umem, sginfo->pgsize);

	/*
	 * Use 4K PBL if
	 * - the PBL size feature is not supported by the firmware.
	 * - the FWO needs more than 12 bits for QP or SRQ.
	 * The offset into the PBL page has to be in multiples of 4K.
	 * During QP or SRQ create, only 12 bits of the FWO can be passed
	 * to the firmware. This is a HWRM limitation and should be fixed.
	 * Create CQ does not have this limitation.
	 */
	if (!(rdev->dev_attr &&
	      bnxt_re_pbl_size_supported(rdev->dev_attr->dev_cap_ext_flags_1)) ||
	    (res != BNXT_RE_RES_TYPE_CQ && sginfo->fwo_offset >= BNXT_RE_PBL_FWO_MAX)) {
		sginfo->npages = ib_umem_num_pages_compat(umem);
		sginfo->pgshft = PAGE_SHIFT;
		sginfo->pgsize = PAGE_SIZE;
		sginfo->fwo_offset = 0;
	}
}
/* uverbs */
#ifdef HAVE_IB_UMEM_DMABUF
struct ib_mr *bnxt_re_reg_user_mr_dmabuf(struct ib_pd *ib_pd, u64 start,
					 u64 length, u64 virt_addr, int fd,
					 int mr_access_flags,
#ifdef HAVE_REG_USER_MR_DMABUF_HAS_UVERBS_ATTR_BUNDLE
					 struct uverbs_attr_bundle *attrs)
#else
					 struct ib_udata *udata)
#endif
{
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ib_pd);
	u32 max_mr_count, max_mr_dmabuf_count;
	struct bnxt_re_dev *rdev = pd->rdev;
	struct ib_umem_dmabuf *umem_dmabuf;
	struct bnxt_qplib_mrinfo mrinfo;
	int umem_pgs, page_shift, rc;
	struct bnxt_re_mr *mr;
	struct ib_umem *umem;
	int npages;

	dev_dbg(rdev_to_dev(rdev), "Register user DMA-BUF MR");

	if (bnxt_re_get_total_mr_mw_count(rdev) >= rdev->dev_attr->max_mr)
		return ERR_PTR(-ENOMEM);

	memset(&mrinfo, 0, sizeof(mrinfo));
	if (length > BNXT_RE_MAX_MR_SIZE) {
		dev_err(rdev_to_dev(rdev), "Requested MR Size: %llu > Max supported: %ld\n",
			length, BNXT_RE_MAX_MR_SIZE);
		return ERR_PTR(-EINVAL);
	}
	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr)
		return ERR_PTR(-ENOMEM);
	mr->rdev = rdev;
	mr->qplib_mr.pd = &pd->qplib_pd;
	mr->qplib_mr.access_flags = __from_ib_access_flags(mr_access_flags);
	mr->qplib_mr.type = CMDQ_ALLOCATE_MRW_MRW_FLAGS_MR;
	mr->is_dmabuf = true;

	if (!_is_alloc_mr_unified(rdev->qplib_res.dattr)) {
		rc = bnxt_qplib_alloc_mrw(&rdev->qplib_res, &mr->qplib_mr);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "Alloc MR failed!");
			goto fail;
		}
		/* The fixed portion of the rkey is the same as the lkey */
		mr->ib_mr.rkey = mr->qplib_mr.rkey;
	}

	umem_dmabuf = ib_umem_dmabuf_get_pinned(&rdev->ibdev, start, length,
						fd, mr_access_flags);
	if (IS_ERR(umem_dmabuf)) {
		rc = PTR_ERR(umem_dmabuf);
		dev_dbg(rdev_to_dev(rdev), "%s: failed to get umem dmabuf : %d\n",
			__func__, rc);
		goto free_mr;
	}
	umem = &umem_dmabuf->umem;
	mr->ib_umem = umem;
	mr->qplib_mr.va = virt_addr;
	umem_pgs = ib_umem_num_pages_compat(umem);
	if (!umem_pgs) {
		dev_err(rdev_to_dev(rdev), "umem is invalid!");
		rc = -EINVAL;
		goto free_umem;
	}
	mr->qplib_mr.total_size = length;
	page_shift = bnxt_re_get_page_shift(umem, virt_addr, start,
					    rdev->dev_attr->page_size_cap);
	/*
	 * FIXME: We have known issue if best page_size is less than PAGE_SIZE
	 */
	if (page_shift < PAGE_SHIFT)
		page_shift = PAGE_SHIFT;

	if ((length >= BNXT_RE_WARN_MR_SZ) &&
	    (page_shift <= BNXT_RE_PAGE_SHIFT_4K)) {
		dev_dbg(rdev_to_dev(rdev),
			"Possible low perf due to HW PTU logic. MR sz %llu pg_sz %lu\n",
			length, BIT(page_shift));
		dev_warn_ratelimited(rdev_to_dev(rdev),
				     "Possible low perf due to HW PTU logic."
				     " MR sz %llu pg_sz %lu\n",
				     length, BIT(page_shift));
	}

	if (!bnxt_re_page_size_ok(page_shift)) {
		dev_err(rdev_to_dev(rdev), "umem page size unsupported!");
		rc = -ENOTSUPP;
		goto free_umem;
	}
	npages = bnxt_re_get_num_pages(umem, start, length, page_shift);

	mrinfo.sg.npages = npages;
	/* Map umem buf ptrs to the PBL */
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	mrinfo.sg.sghead = get_ib_umem_sgl(umem, &mrinfo.sg.nmap);
#else
	mrinfo.sg.umem = umem;
#endif
	mrinfo.sg.pgshft = page_shift;
	mrinfo.sg.pgsize = BIT(page_shift);

	mrinfo.mrw = &mr->qplib_mr;
#ifdef HAVE_IB_ACCESS_RELAXED_ORDERING
	if (mr_access_flags & IB_ACCESS_RELAXED_ORDERING)
		bnxt_re_check_and_set_relaxed_ordering(rdev, &mrinfo);
#endif

	rc = bnxt_qplib_reg_mr(&rdev->qplib_res, &mrinfo, false);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Reg user MR failed!");
		goto free_umem;
	}

	mr->ib_mr.lkey = mr->qplib_mr.lkey;
	mr->ib_mr.rkey = mr->qplib_mr.lkey;
	atomic_inc(&rdev->stats.rsors.mr_count);
	max_mr_count =  atomic_read(&rdev->stats.rsors.mr_count);
	if (max_mr_count > atomic_read(&rdev->stats.rsors.max_mr_count))
		atomic_set(&rdev->stats.rsors.max_mr_count, max_mr_count);
	max_mr_dmabuf_count = atomic_inc_return(&rdev->stats.rsors.mr_dmabuf_count);
	if (max_mr_dmabuf_count > atomic_read(&rdev->stats.rsors.max_mr_dmabuf_count))
		atomic_set(&rdev->stats.rsors.max_mr_dmabuf_count, max_mr_dmabuf_count);

	BNXT_RE_RES_LIST_ADD(rdev, mr, BNXT_RE_RES_TYPE_MR);

	return &mr->ib_mr;

free_umem:
	bnxt_re_peer_mem_release(mr->ib_umem);
free_mr:
	if (!_is_alloc_mr_unified(rdev->qplib_res.dattr))
		bnxt_qplib_free_mrw(&rdev->qplib_res, &mr->qplib_mr, 0, NULL);
fail:
	kfree(mr);
	return ERR_PTR(rc);
}
#endif

struct ib_mr *bnxt_re_reg_user_mr(struct ib_pd *ib_pd, u64 start, u64 length,
				  u64 virt_addr, int mr_access_flags,
				  struct ib_udata *udata)
{
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ib_pd);
	struct bnxt_re_dev *rdev = pd->rdev;
	struct bnxt_qplib_mrinfo mrinfo;
	int umem_pgs, page_shift, rc;
	struct bnxt_re_mr *mr;
	struct ib_umem *umem;
	u32 max_mr_count;
#ifdef CONFIG_INFINIBAND_PEER_MEM
	u32 peer_cnt;
#endif
	int npages;

	dev_dbg(rdev_to_dev(rdev), "Reg user MR");

	if (bnxt_re_get_total_mr_mw_count(rdev) >= rdev->dev_attr->max_mr)
		return ERR_PTR(-ENOMEM);

	memset(&mrinfo, 0, sizeof(mrinfo));
	if (length > BNXT_RE_MAX_MR_SIZE) {
		dev_err(rdev_to_dev(rdev), "Requested MR Size: %llu "
			"> Max supported: %ld\n", length, BNXT_RE_MAX_MR_SIZE);
		return ERR_PTR(-EINVAL);
	}
	mr = kzalloc(sizeof(*mr), GFP_KERNEL);
	if (!mr)
		return ERR_PTR (-ENOMEM);
	mr->rdev = rdev;
	mr->qplib_mr.pd = &pd->qplib_pd;
	mr->qplib_mr.access_flags = __from_ib_access_flags(mr_access_flags);
	mr->qplib_mr.type = CMDQ_ALLOCATE_MRW_MRW_FLAGS_MR;

	if (!_is_alloc_mr_unified(rdev->qplib_res.dattr)) {
		rc = bnxt_qplib_alloc_mrw(&rdev->qplib_res, &mr->qplib_mr);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "Alloc MR failed!");
			goto fail;
		}
		/* The fixed portion of the rkey is the same as the lkey */
		mr->ib_mr.rkey = mr->qplib_mr.rkey;
	}

	umem = ib_umem_get_flags_compat(rdev, ib_pd->uobject->context,
					udata, start, length,
					mr_access_flags, 0);
	if (IS_ERR(umem)) {
		rc = PTR_ERR(umem);
		dev_err(rdev_to_dev(rdev), "%s: ib_umem_get failed! rc = %d\n",
			__func__, rc);
		goto free_mr;
	}
	mr->ib_umem = umem;

	mr->qplib_mr.va = virt_addr;
	umem_pgs = ib_umem_num_pages_compat(umem);
	if (!umem_pgs) {
		dev_err(rdev_to_dev(rdev), "umem is invalid!");
		rc = -EINVAL;
		goto free_umem;
	}
	mr->qplib_mr.total_size = length;
	page_shift = bnxt_re_get_page_shift(umem, virt_addr, start,
					    rdev->dev_attr->page_size_cap);
	/*
	 * FIXME: We have known issue if best page_size is less than PAGE_SIZE
	 */
	if (page_shift < PAGE_SHIFT)
		page_shift = PAGE_SHIFT;

	if ((length >= BNXT_RE_WARN_MR_SZ) &&
	    (page_shift <= BNXT_RE_PAGE_SHIFT_4K)) {
		dev_dbg(rdev_to_dev(rdev),
			"Possible low perf due to HW PTU logic. MR sz %llu pg_sz %lu\n",
			length, BIT(page_shift));
	}

	if (!bnxt_re_page_size_ok(page_shift)) {
		dev_err(rdev_to_dev(rdev), "umem page size unsupported!");
		rc = -ENOTSUPP;
		goto free_umem;
	}
	npages = bnxt_re_get_num_pages(umem, start, length, page_shift);
	mrinfo.sg.npages = npages;
	/* Map umem buf ptrs to the PBL */
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	mrinfo.sg.sghead = get_ib_umem_sgl(umem, &mrinfo.sg.nmap);
#else
	mrinfo.sg.umem = umem;
#endif
	mrinfo.sg.pgshft = page_shift;
	mrinfo.sg.pgsize = BIT(page_shift);

	mrinfo.mrw = &mr->qplib_mr;
#ifdef HAVE_IB_ACCESS_RELAXED_ORDERING
	if (mr_access_flags & IB_ACCESS_RELAXED_ORDERING)
		bnxt_re_check_and_set_relaxed_ordering(rdev, &mrinfo);
#endif

	rc = bnxt_qplib_reg_mr(&rdev->qplib_res, &mrinfo, false);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Reg user MR failed!");
		goto free_umem;
	}

	mr->ib_mr.lkey = mr->ib_mr.rkey = mr->qplib_mr.lkey;
	atomic_inc(&rdev->stats.rsors.mr_count);
	max_mr_count =  atomic_read(&rdev->stats.rsors.mr_count);
	if (max_mr_count > atomic_read(&rdev->stats.rsors.max_mr_count))
		atomic_set(&rdev->stats.rsors.max_mr_count, max_mr_count);

#ifdef CONFIG_INFINIBAND_PEER_MEM
	if (bnxt_re_get_peer_mem(umem)) {
		mr->is_peer_mem = true;
		atomic_inc(&rdev->stats.rsors.peer_mem_mr_count);
		peer_cnt = atomic_read(&rdev->stats.rsors.peer_mem_mr_count);
		if (peer_cnt > atomic_read(&rdev->stats.rsors.max_peer_mem_mr_count))
			atomic_set(&rdev->stats.rsors.max_peer_mem_mr_count, peer_cnt);
		atomic_set(&mr->invalidated, 0);
		init_completion(&mr->invalidation_comp);
#ifdef HAVE_IB_UMEM_GET_FLAGS
		rc =
#endif
		ib_umem_activate_invalidation_notifier
			(umem, bnxt_re_invalidate_umem, mr);
#ifdef HAVE_IB_UMEM_GET_FLAGS
		if (rc)
			goto free_umem;
#endif
		mr->is_invalcb_active = true;
	}
#endif
	BNXT_RE_RES_LIST_ADD(rdev, mr, BNXT_RE_RES_TYPE_MR);

	return &mr->ib_mr;

free_umem:
	bnxt_re_peer_mem_release(mr->ib_umem);
free_mr:
	if (!_is_alloc_mr_unified(rdev->qplib_res.dattr))
		bnxt_qplib_free_mrw(&rdev->qplib_res, &mr->qplib_mr, 0, NULL);
fail:
	kfree(mr);
	return ERR_PTR(rc);
}

/* TODO The support for rereg_user_mr handler is not enabled in the driver */
#ifdef HAVE_IB_REREG_USER_MR
REREG_USER_MR_RET
bnxt_re_rereg_user_mr(struct ib_mr *ib_mr, int flags, u64 start, u64 length,
		      u64 virt_addr, int mr_access_flags,
		      struct ib_pd *ib_pd, struct ib_udata *udata)
{
	struct bnxt_re_mr *mr = to_bnxt_re(ib_mr, struct bnxt_re_mr, ib_mr);
	struct bnxt_re_pd *pd = to_bnxt_re(ib_pd, struct bnxt_re_pd, ib_pd);
	int umem_pgs = 0, page_shift = PAGE_SHIFT, rc;
	struct bnxt_re_dev *rdev = mr->rdev;
	struct bnxt_qplib_mrinfo mrinfo;
	struct ib_umem *umem;
	u32 npages;

	/* TODO: Must decipher what to modify based on the flags */
	memset(&mrinfo, 0, sizeof(mrinfo));
	if (flags & IB_MR_REREG_TRANS) {
		umem = ib_umem_get_flags_compat(rdev, ib_pd->uobject->context,
						udata, start, length,
						mr_access_flags, 0);
		if (IS_ERR(umem)) {
			rc = PTR_ERR(umem);
			dev_err(rdev_to_dev(rdev),
				"%s: ib_umem_get failed! ret =  %d\n",
				__func__, rc);
			goto fail;
		}
		mr->ib_umem = umem;

		mr->qplib_mr.va = virt_addr;
		umem_pgs = ib_umem_num_pages_compat(umem);
		if (!umem_pgs) {
			dev_err(rdev_to_dev(rdev), "umem is invalid!");
			rc = -EINVAL;
			goto fail_free_umem;
		}
		mr->qplib_mr.total_size = length;
		page_shift = bnxt_re_get_page_shift(umem, virt_addr, start,
					    rdev->dev_attr->page_size_cap);
		/*
		 * FIXME: We have known issue if best page_size is less than PAGE_SIZE
		 */
		if (page_shift < PAGE_SHIFT)
			page_shift = PAGE_SHIFT;
		if (!bnxt_re_page_size_ok(page_shift)) {
			dev_err(rdev_to_dev(rdev),
				"umem page size unsupported!");
			rc = -ENOTSUPP;
			goto fail_free_umem;
		}
		npages = bnxt_re_get_num_pages(umem, start, length, page_shift);
		mrinfo.sg.npages = npages;
		/* Map umem buf ptrs to the PBL */
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
		mrinfo.sg.sghead = get_ib_umem_sgl(umem, &mrinfo.sg.nmap);
#else
		mrinfo.sg.umem = umem;
#endif
		mrinfo.sg.pgshft = page_shift;
		mrinfo.sg.pgsize = BIT(page_shift);
	}

	mrinfo.mrw = &mr->qplib_mr;
	if (flags & IB_MR_REREG_PD)
		mr->qplib_mr.pd = &pd->qplib_pd;

	if (flags & IB_MR_REREG_ACCESS)
		mr->qplib_mr.access_flags = __from_ib_access_flags(mr_access_flags);

	rc = bnxt_qplib_reg_mr(&rdev->qplib_res, &mrinfo, false);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Rereg user MR failed!");
		goto fail_free_umem;
	}
	mr->ib_mr.rkey = mr->qplib_mr.rkey;
#ifndef HAVE_REREG_USER_MR_RET_PTR
	return 0;
#else
	return NULL;
#endif

fail_free_umem:
	bnxt_re_peer_mem_release(mr->ib_umem);
fail:
#ifndef HAVE_REREG_USER_MR_RET_PTR
	return rc;
#else
	return ERR_PTR(rc);
#endif
}
#endif

static inline void bnxt_re_init_small_recv_wqe_sup(struct bnxt_re_ucontext *uctx,
						   struct bnxt_re_uctx_req *req,
						   struct bnxt_re_uctx_resp *resp)
{
	if (req->comp_mask & BNXT_RE_COMP_MASK_REQ_UCNTX_SMALL_RECV_WQE_LIB_SUP &&
	    _is_small_recv_wqe_supported(uctx->rdev->qplib_res.dattr->dev_cap_ext_flags)) {
		uctx->small_recv_wqe_sup = true;
		resp->comp_mask |= BNXT_RE_COMP_MASK_UCNTX_SMALL_RECV_WQE_DRV_SUP;
	}
}

ALLOC_UCONTEXT_RET bnxt_re_alloc_ucontext(ALLOC_UCONTEXT_IN *uctx_in,
					  struct ib_udata *udata)
{
#ifndef HAVE_UCONTEXT_ALLOC_IN_IB_CORE
	struct bnxt_re_ucontext *uctx = NULL;
	struct ib_device *ibdev = uctx_in;
#else
	struct ib_ucontext *ctx = uctx_in;
	struct ib_device *ibdev = ctx->device;
	struct bnxt_re_ucontext *uctx =
		container_of(ctx, struct bnxt_re_ucontext, ib_uctx);
#endif

	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct bnxt_qplib_dev_attr *dev_attr = rdev->dev_attr;
	bool is_entry = false;
	struct bnxt_re_uctx_resp resp = {};
	struct bnxt_re_uctx_req ureq = {};
	struct bnxt_qplib_chip_ctx *cctx;
	bool genp5_plus = false;
	u32 chip_met_rev_num;
	int rc;

	cctx = rdev->chip_ctx;

	if (test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags)) {
		dev_dbg(rdev_to_dev(rdev),
			"%s %d controller is not active flags 0x%lx\n",
			__func__, __LINE__, rdev->flags);
		rc = -EIO;
		goto err_out;
	}

#ifndef HAVE_UCONTEXT_ALLOC_IN_IB_CORE
	uctx = kzalloc(sizeof(*uctx), GFP_KERNEL);
	if (!uctx)
		return ERR_PTR(-ENOMEM);
#endif
	uctx->rdev = rdev;

	rc = bnxt_re_get_user_dpi(rdev, uctx);
	if (rc) {
		rc = -ENOMEM;
		goto err_free_uctx;
	}

	resp.dpi = uctx->dpi.dpi;
	resp.uc_db_offset = uctx->dpi.umdbr & ~PAGE_MASK;
	is_entry = bnxt_re_mmap_entry_insert_compat(uctx,
						    (u64)uctx->dpi.umdbr, 0,
						    BNXT_RE_MMAP_UC_DB,
						    &resp.uc_db_mmap_key,
						    &uctx->uc_db_mmap);
	if (!is_entry) {
		rc = -ENOMEM;
		goto err_free_uctx;
	}

	if (rdev->hdbr_enabled) {
		uctx->hdbr_app = bnxt_re_hdbr_alloc_app(rdev, true);
		if (!uctx->hdbr_app) {
			dev_err(rdev_to_dev(rdev), "HDBR app allocation failed!");
			rc = -ENOMEM;
			goto hdbr_fail;
		}
	}
#ifdef HAVE_DISASSOCIATE_UCNTX
#ifndef HAVE_RDMA_USER_MMAP_IO
	INIT_LIST_HEAD(&uctx->vma_list_head);
	mutex_init(&uctx->list_mutex);
#endif
#endif
	chip_met_rev_num = cctx->chip_num;
	chip_met_rev_num |= ((u32)cctx->chip_rev & 0xFF) <<
			     BNXT_RE_CHIP_ID0_CHIP_REV_SFT;
	chip_met_rev_num |= ((u32)cctx->chip_metal & 0xFF) <<
			     BNXT_RE_CHIP_ID0_CHIP_MET_SFT;
	resp.chip_id0 = chip_met_rev_num;
	resp.chip_id1 = 0; /* future extension of chip info */

	/*Temp, Use idr_alloc instead*/
	resp.dev_id = rdev->en_dev->pdev->devfn;
	resp.max_qp = rdev->qplib_res.hctx->qp_ctx.max;

	genp5_plus = _is_chip_p5_plus(cctx);
	resp.modes = genp5_plus ? cctx->modes.wqe_mode : 0;
	if (rdev->dev_attr && _is_host_msn_table(rdev->dev_attr->dev_cap_ext_flags2))
		resp.comp_mask = BNXT_RE_COMP_MASK_UCNTX_MSN_TABLE_ENABLED;
	if (BNXT_RE_IQM(rdev->dev_attr->dev_cap_flags))
		resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_INTERNAL_QUEUE_MEMORY;
	if (BNXT_RE_EXPRESS_MODE(rdev->dev_attr->dev_cap_flags))
		resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_EXPRESS_MODE_ENABLED;
	if (BNXT_RE_UDP_SP_WQE(rdev->dev_attr->dev_cap_ext_flags2))
		resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_CHG_UDP_SRC_PORT_WQE_SUPPORTED;

	resp.pg_size = PAGE_SIZE;
	resp.cqe_sz = sizeof(struct cq_base);
	resp.max_cqd = dev_attr->max_cq_wqes;
	resp.db_push_mode = cctx->modes.db_push_mode;
	if (rdev->dbr_pacing)
		resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_DBR_PACING_ENABLED;
	if (BNXT_RE_PUSH_ENABLED(cctx->modes.db_push_mode))
		resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_WC_DPI_ENABLED;
	/* TODO: once FW advertises, use that cap */
	if (_is_chip_p7(cctx))
		resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_MASK_ECE;

#ifndef HAVE_UAPI_DEF
	if (BNXT_RE_PUSH_ENABLED(cctx->modes.db_push_mode)) {
		is_entry = bnxt_re_mmap_entry_insert_compat(uctx, (u64)uctx->wcdpi.umdbr, 0,
							    BNXT_RE_MMAP_WC_DB,
							    &resp.wc_db_mmap_key,
							    &uctx->wc_db_mmap);
		if (!is_entry) {
			rc = -ENOMEM;
			goto err_free_uctx;
		}

		resp.wcdpi = uctx->wcdpi.dpi;
	}

	if (rdev->dbr_pacing) {
		WARN_ON(!rdev->dbr_pacing_bar);
		WARN_ON(!rdev->dbr_pacing_page);
		is_entry = bnxt_re_mmap_entry_insert_compat(uctx, (u64)rdev->dbr_pacing_page, 0,
							    BNXT_RE_MMAP_DBR_PAGE,
							    &resp.dbr_pacing_mmap_key,
							    &uctx->dbr_pacing_mmap);
		if (!is_entry) {
			rc = -ENOMEM;
			goto err_free_page;
		}

		is_entry = bnxt_re_mmap_entry_insert_compat(uctx, (u64)rdev->dbr_pacing_bar, 0,
							    BNXT_RE_MMAP_DBR_PACING_BAR,
							    &resp.dbr_pacing_bar_mmap_key,
							    &uctx->dbr_pacing_bar_mmap);
		if (!is_entry) {
			rc = -ENOMEM;
			goto err_free_page;
		}
	}

#endif

#ifdef HAVE_IB_USER_VERBS_EX_CMD_MODIFY_QP
	resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_MQP_EX_SUPPORTED;
#endif

	if (rdev->dbr_drop_recov && rdev->user_dbr_drop_recov)
		resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_DBR_RECOVERY_ENABLED;

	/* TBD - Below code can be removed now */
	if (udata->inlen >= sizeof(ureq)) {
		rc = ib_copy_from_udata(&ureq, udata,
					min(udata->inlen, sizeof(ureq)));
		if (rc)
			goto err_free_page;
		if (bnxt_re_init_pow2_flag(&ureq, &resp))
			dev_warn(rdev_to_dev(rdev),
				 "Enabled roundup logic. Library bug?");
		if (bnxt_re_init_rsvd_wqe_flag(&ureq, &resp, genp5_plus))
			dev_warn(rdev_to_dev(rdev),
				 "Rsvd wqe in use! Try the updated library.");
		bnxt_re_init_small_recv_wqe_sup(uctx, &ureq, &resp);

		resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_MAX_RQ_WQES;
		resp.max_rq_wqes = dev_attr->max_rq_wqes;
	} else {
		dev_warn(rdev_to_dev(rdev),
			 "Enabled roundup logic. Update the library!");
		resp.comp_mask &= ~BNXT_RE_COMP_MASK_UCNTX_POW2_DISABLED;

		dev_warn(rdev_to_dev(rdev),
			 "Rsvd wqe in use. Update the library!");
		resp.comp_mask &= ~BNXT_RE_COMP_MASK_UCNTX_RSVD_WQE_DISABLED;
	}

	if (cctx->modes.hdbr_enabled &&
	    !rdev->enable_queue_overflow_telemetry &&
	    !_is_cq_overflow_detection_enabled(dev_attr->dev_cap_ext_flags2))
		resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_CQ_IGNORE_OVERRUN_DRV_SUP;

	resp.comp_mask |= BNXT_RE_COMP_MASK_UCNTX_DEFERRED_DB_ENABLED;
	resp.deferred_db_enabled = rdev->deferred_db_enabled;

	uctx->cmask = (uint64_t)resp.comp_mask;
	rc = bnxt_re_copy_to_udata(rdev, &resp,
				   min(udata->outlen, sizeof(resp)),
				   udata);
	if (rc)
		goto err_free_page;

	BNXT_RE_RES_LIST_ADD(rdev, uctx, BNXT_RE_RES_TYPE_UCTX);

	INIT_LIST_HEAD(&uctx->cq_list);
	mutex_init(&uctx->cq_lock);
	if (rdev->hdbr_enabled) {
		INIT_LIST_HEAD(&uctx->srq_list);
		mutex_init(&uctx->srq_lock);
	}

#ifndef HAVE_UCONTEXT_ALLOC_IN_IB_CORE
	return &uctx->ib_uctx;
#else
	return 0;
#endif

err_free_page:
	if (rdev->hdbr_enabled) {
		struct bnxt_re_hdbr_app *app = uctx->hdbr_app;

		mutex_lock(&rdev->hdbr_lock);
		list_del(&app->lst);
		mutex_unlock(&rdev->hdbr_lock);
		bnxt_re_hdbr_dealloc_app(rdev, app);
		uctx->hdbr_app = NULL;
	}
hdbr_fail:
err_free_uctx:
	bnxt_re_mmap_entry_remove_compat(uctx->uc_db_mmap);
	uctx->uc_db_mmap = NULL;
	bnxt_re_mmap_entry_remove_compat(uctx->wc_db_mmap);
	uctx->wc_db_mmap = NULL;
	bnxt_re_mmap_entry_remove_compat(uctx->dbr_pacing_mmap);
	uctx->dbr_pacing_mmap = NULL;
	bnxt_re_mmap_entry_remove_compat(uctx->dbr_pacing_bar_mmap);
	uctx->dbr_pacing_bar_mmap = NULL;

	if (uctx->wcdpi.dbr) {
		(void)bnxt_qplib_dealloc_dpi(&rdev->qplib_res, &uctx->wcdpi);
		uctx->wcdpi.dbr = NULL;
		if (BNXT_RE_PPP_ENABLED(rdev->chip_ctx))
			rdev->ppp_stats.ppp_enabled_ctxs--;
	}
	if (uctx->dpi.dbr) {
		(void)bnxt_qplib_dealloc_dpi(&rdev->qplib_res, &uctx->dpi);
		uctx->dpi.dbr = NULL;
	}
#ifndef HAVE_UCONTEXT_ALLOC_IN_IB_CORE
	kfree(uctx);
#endif
err_out:
#ifndef HAVE_UCONTEXT_ALLOC_IN_IB_CORE
	return ERR_PTR(rc);
#else
	return rc;
#endif
}

DEALLOC_UCONTEXT_RET bnxt_re_dealloc_ucontext(struct ib_ucontext *ib_uctx)
{
	struct bnxt_re_ucontext *uctx = to_bnxt_re(ib_uctx,
						   struct bnxt_re_ucontext,
						   ib_uctx);
	struct bnxt_re_dev *rdev = uctx->rdev;
	int rc = 0;

	BNXT_RE_RES_LIST_DEL(rdev, uctx, BNXT_RE_RES_TYPE_UCTX);

	if (rdev->hdbr_enabled && uctx->hdbr_app) {
		struct bnxt_re_hdbr_app *app = uctx->hdbr_app;

		mutex_lock(&rdev->hdbr_lock);
		list_del(&app->lst);
		mutex_unlock(&rdev->hdbr_lock);
		bnxt_re_hdbr_dealloc_app(rdev, app);
		uctx->hdbr_app = NULL;
	}

	bnxt_re_mmap_entry_remove_compat(uctx->uc_db_mmap);
	uctx->uc_db_mmap = NULL;
#ifndef HAVE_UAPI_DEF
	bnxt_re_mmap_entry_remove_compat(uctx->wc_db_mmap);
	uctx->wc_db_mmap = NULL;
	bnxt_re_mmap_entry_remove_compat(uctx->dbr_pacing_mmap);
	uctx->dbr_pacing_mmap = NULL;
	bnxt_re_mmap_entry_remove_compat(uctx->dbr_pacing_bar_mmap);
	uctx->dbr_pacing_bar_mmap = NULL;
#endif

	if (uctx->dpi.dbr) {
		/* Free DPI only if this is the first PD allocated by the
		 * application and mark the context dpi as NULL
		 */
		if (_is_chip_p5_plus(rdev->chip_ctx) && uctx->wcdpi.dbr) {
			rc = bnxt_qplib_dealloc_dpi(&rdev->qplib_res,
						    &uctx->wcdpi);
			if (rc)
				dev_err(rdev_to_dev(rdev),
						"dealloc push dp failed");
			uctx->wcdpi.dbr = NULL;
			if (BNXT_RE_PPP_ENABLED(rdev->chip_ctx))
				rdev->ppp_stats.ppp_enabled_ctxs--;
		}

		rc = bnxt_qplib_dealloc_dpi(&rdev->qplib_res,
					    &uctx->dpi);
		if (rc)
			dev_err(rdev_to_dev(rdev), "Deallocte HW DPI failed!");
			/* Don't fail, continue*/
		uctx->dpi.dbr = NULL;
	}

#ifndef HAVE_UCONTEXT_ALLOC_IN_IB_CORE
	kfree(uctx);
	return 0;
#endif
}

#ifdef HAVE_DISASSOCIATE_UCNTX
#ifndef HAVE_RDMA_USER_MMAP_IO
static void bnxt_re_vma_open(struct vm_area_struct *vma)
{
	/* vma_open is called when a new VMA is created on top of our VMA.  This
	 * is done through either mremap flow or split_vma (usually due to
	 * mlock, madvise, munmap, etc.) We do not support a clone of the VMA
	 * as this VMA is strongly hardware related.  Therefore we set the
	 * vm_ops of the newly created/cloned VMA to NULL, to prevent it from
	 * calling us again and trying to do incorrect actions.  We assume that
	 * the original VMA size is exactly a single page, and therefore all
	 * "splitting" operation will not happen to it.
	 */
	vma->vm_ops = NULL;
}

static void bnxt_re_vma_close(struct vm_area_struct *vma)
{
	struct bnxt_re_vma_data *vma_data;

	vma_data = (struct bnxt_re_vma_data *)vma->vm_private_data;

	vma_data->vma = NULL;
	mutex_lock(vma_data->list_mutex);
	list_del(&vma_data->vma_list);
	mutex_unlock(vma_data->list_mutex);
	kfree(vma_data);
}

static const struct vm_operations_struct bnxt_re_vma_op = {
	.open = bnxt_re_vma_open,
	.close = bnxt_re_vma_close
};

int bnxt_re_set_vma_data(struct bnxt_re_ucontext *uctx,
			 struct vm_area_struct *vma)
{
	struct bnxt_re_vma_data *vma_data;

	vma_data = kzalloc(sizeof(*vma_data), GFP_KERNEL);
	if (!vma_data)
		return -ENOMEM;

	vma_data->vma = vma;
	vma_data->list_mutex = &uctx->list_mutex;
	vma->vm_private_data = vma_data;
	vma->vm_ops = &bnxt_re_vma_op;

	mutex_lock(&uctx->list_mutex);
	list_add_tail(&vma_data->vma_list, &uctx->vma_list_head);
	mutex_unlock(&uctx->list_mutex);

	return 0;
}
#endif
#endif

#ifdef HAVE_RDMA_XARRAY_MMAP_V2
/* Helper function to mmap the virtual memory from user app */
int bnxt_re_mmap(struct ib_ucontext *ib_uctx, struct vm_area_struct *vma)
{
	struct bnxt_re_ucontext *uctx = to_bnxt_re(ib_uctx,
						   struct bnxt_re_ucontext,
						   ib_uctx);
	struct bnxt_re_dev *rdev = uctx->rdev;
	int rc = 0;
	u64 pfn;
	struct bnxt_re_user_mmap_entry *bnxt_entry;
	struct rdma_user_mmap_entry *rdma_entry;

	if (vma->vm_end - vma->vm_start != PAGE_SIZE)
		return -EINVAL;

	rdma_entry = rdma_user_mmap_entry_get(ib_uctx, vma);
	bnxt_entry = container_of(rdma_entry, struct bnxt_re_user_mmap_entry,
				  rdma_entry);
	if (!rdma_entry) {
		/* TBD - remove after few phase. Good for early debugging. */
		dev_err(rdev_to_dev(rdev), "From %s %d  vma->vm_pgoff 0x%lx\n",
			__func__, __LINE__,  (unsigned long)vma->vm_pgoff);
		return -EINVAL;
	}

	switch (bnxt_entry->mmap_flag) {
	case BNXT_RE_MMAP_WC_DB:
		pfn = bnxt_entry->cpu_addr >> PAGE_SHIFT;
		rc = rdma_user_mmap_io(ib_uctx, vma, pfn, PAGE_SIZE,
				       pgprot_writecombine(vma->vm_page_prot),
				       rdma_entry);
		break;
	case BNXT_RE_MMAP_UC_DB:
		pfn = bnxt_entry->cpu_addr >> PAGE_SHIFT;
		rc = rdma_user_mmap_io(ib_uctx, vma, pfn, PAGE_SIZE,
				       pgprot_noncached(vma->vm_page_prot),
				       rdma_entry);
		break;
	case BNXT_RE_MMAP_DBR_PACING_BAR:
	case BNXT_RE_MMAP_DB_RECOVERY_PAGE:
		pfn = bnxt_entry->cpu_addr >> PAGE_SHIFT;
		rc = rdma_user_mmap_io(ib_uctx, vma, pfn, PAGE_SIZE,
				       pgprot_noncached(vma->vm_page_prot),
				       rdma_entry);
		break;
	case BNXT_RE_MMAP_DBR_PAGE:
	case BNXT_RE_MMAP_TOGGLE_PAGE:
		/* Driver doesn't expect write access for user space */
		if (vma->vm_flags & VM_WRITE)
			rc = -EFAULT;
		else
			rc = vm_insert_page(vma, vma->vm_start,
					    virt_to_page((void *)bnxt_entry->cpu_addr));
		break;
	case BNXT_RE_MMAP_HDBR_BASE:
		/* Resetting vm_pgoff is required since vm_pgoff must be an actual offset.
		 * dma_mmap_coherent does sanity check.
		 * We are using vm_pgoff for other purpose like sending
		 * kernel virtual address through vm_pgoff.
		 */
		vma->vm_pgoff = 0;
		rc = dma_mmap_coherent(&rdev->en_dev->pdev->dev, vma,
				       (void *)bnxt_entry->cpu_addr,
				       bnxt_entry->dma_addr,
				       (vma->vm_end - vma->vm_start));
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"DB copy memory mapping failed!");
			rc = -EAGAIN;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}

	dev_dbg(rdev_to_dev(rdev),
		">> %s %s %d 0x%lx user pages"
		"(%ld cpu addr 0x%lx dma addr 0x%lx) ret value %d\n",
		bnxt_re_mmap_str(bnxt_entry->mmap_flag),
		__func__, __LINE__,
		vma_pages(vma),
		PAGE_ALIGN(vma->vm_end - vma->vm_start) >> PAGE_SHIFT,
		(unsigned long)bnxt_entry->cpu_addr,
		(unsigned long)bnxt_entry->dma_addr, rc);

	rdma_user_mmap_entry_put(rdma_entry);
	return rc;
}

void bnxt_re_mmap_free(struct rdma_user_mmap_entry *rdma_entry)
{
	struct bnxt_re_user_mmap_entry *bnxt_entry;

	bnxt_entry = container_of(rdma_entry, struct bnxt_re_user_mmap_entry,
				  rdma_entry);
	if (bnxt_entry)
		dev_dbg(NULL, "<< %s %s %d  cpu addr 0x%lx\n",
			bnxt_re_mmap_str(bnxt_entry->mmap_flag),
			__func__, __LINE__,
			(unsigned long)bnxt_entry->cpu_addr);

	kfree(bnxt_entry);
}
#endif

#ifdef HAVE_PROCESS_MAD_IB_MAD_HDR
static u8 bnxt_re_get_mad_hdr_mgmt_class(const struct ib_mad_hdr *mad_hdr)
{
	return mad_hdr->mgmt_class;
}
#else
static u8 bnxt_re_get_mad_hdr_mgmt_class(const struct ib_mad *mad)
{
	return mad->mad_hdr.mgmt_class;
}
#endif

#ifdef HAVE_PROCESS_MAD_U32_PORT
int bnxt_re_process_mad(struct ib_device *ibdev, int mad_flags,
			u32 port_num, const struct ib_wc *in_wc,
			const struct ib_grh *in_grh,
			const struct ib_mad *in_mad, struct ib_mad *out_mad,
			size_t *out_mad_size, u16 *out_mad_pkey_index)
#else
#ifndef HAVE_PROCESS_MAD_IB_MAD_HDR
int bnxt_re_process_mad(struct ib_device *ibdev, int mad_flags, u8 port_num,
			const struct ib_wc *in_wc, const struct ib_grh *in_grh,
			const struct ib_mad *in_mad, struct ib_mad *out_mad,
			size_t *out_mad_size, u16 *out_mad_pkey_index)
#else
int bnxt_re_process_mad(struct ib_device *ibdev, int process_mad_flags,
			u8 port_num, const struct ib_wc *in_wc,
			const struct ib_grh *in_grh,
			const struct ib_mad_hdr *in_mad, size_t in_mad_size,
			struct ib_mad_hdr *out_mad, size_t *out_mad_size,
			u16 *out_mad_pkey_index)
#endif
#endif
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	int ret = IB_MAD_RESULT_SUCCESS;
	u8 mgmt_class;
#ifndef HAVE_PROCESS_MAD_IB_MAD_HDR
	int rc;
#endif
	rdev->dbg_stats->mad.mad_processed++;

	mgmt_class = bnxt_re_get_mad_hdr_mgmt_class(in_mad);
	if (mgmt_class == 0xd) {
		rdev->dbg_stats->mad.mad_consumed++;
		ret |= IB_MAD_RESULT_CONSUMED;
	}

	if (mgmt_class == IB_MGMT_CLASS_PERF_MGMT) {
#ifndef HAVE_PROCESS_MAD_IB_MAD_HDR
		/* Declaring support of extended counters */
		if (in_mad->mad_hdr.attr_id == IB_PMA_CLASS_PORT_INFO) {
			struct ib_class_port_info cpi = {};

			cpi.capability_mask = IB_PMA_CLASS_CAP_EXT_WIDTH;
			memcpy((out_mad->data + 40), &cpi, sizeof(cpi));
			ret |= IB_MAD_RESULT_REPLY;
			return ret;
		}

		if (in_mad->mad_hdr.attr_id == IB_PMA_PORT_COUNTERS_EXT)
			rc = bnxt_re_assign_pma_port_ext_counters(rdev, out_mad);
		else
			rc = bnxt_re_assign_pma_port_counters(rdev, out_mad);
		if (rc)
			return IB_MAD_RESULT_FAILURE;
#endif
		ret |= IB_MAD_RESULT_REPLY;
	}
	return ret;
}

static int bnxt_re_modify_raweth_sgid(struct bnxt_re_qp *qp)
{
	if (qp->qplib_qp.cur_qp_state != CMDQ_MODIFY_QP_NEW_STATE_RTR)
		return 0;

	qp->qplib_qp.is_roce_mirror_qp = true;
	qp->qplib_qp.modify_flags = CMDQ_MODIFY_QP_MODIFY_MASK_SGID_INDEX;
	qp->qplib_qp.ext_modify_flags = 0;

	return bnxt_qplib_modify_qp(&qp->rdev->qplib_res, &qp->qplib_qp);
}

static int bnxt_re_create_sniffer_flow(struct bnxt_re_dev *rdev, struct bnxt_re_flow *flow)
{
	struct bnxt_re_qp *qp = flow->qp;
	int rc;

	rc = bnxt_re_modify_raweth_sgid(qp);
	if (rc)
		dev_dbg(rdev_to_dev(rdev), "failed to modify SGID for RawEth QP rc = %d\n",
			rc);

	rc = bnxt_qplib_create_flow(&rdev->qplib_res);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev), "failed to create flow rc = %d\n", rc);
		return rc;
	}

	rc = bnxt_rdma_pkt_capture_set(rdev->en_dev, true);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev), "failed to enable RoCE mirror rc = %d\n", rc);
		bnxt_qplib_destroy_flow(&rdev->qplib_res);
	}

	return rc;
}

static void bnxt_re_destroy_sniffer_flow(struct bnxt_re_flow *flow)
{
	struct bnxt_re_dev *rdev = flow->rdev;
	int rc;

	rc = bnxt_rdma_pkt_capture_set(rdev->en_dev, false);
	if (rc)
		dev_dbg(rdev_to_dev(rdev), "failed to disable RoCE mirror rc = %d\n", rc);

	rc = bnxt_qplib_destroy_flow(&rdev->qplib_res);
	if (rc)
		dev_dbg(rdev_to_dev(rdev), "failed to destroy_flow rc = %d\n", rc);
}

static int bnxt_re_create_normal_flow(struct bnxt_re_dev *rdev, struct ib_flow_attr *attr,
				      struct bnxt_re_flow *flow)
{
	bool ipv4_valid = false, tag_valid = false;
	struct ib_flow_spec_action_tag  *flow_tag;
	struct bnxt_dcqcn_session_entry *session;
	struct ib_flow_spec_ipv4 *ipv4_spec;
	struct bnxt_re_qp *qp = flow->qp;
	void *ib_spec;
	int rc;

	if (attr->num_of_specs != 2)
		return -EOPNOTSUPP;

	ib_spec = attr + 1;
	ipv4_spec = (struct ib_flow_spec_ipv4 *)ib_spec;
	flow_tag = (struct ib_flow_spec_action_tag *)(ib_spec + ipv4_spec->size);
	if (ipv4_spec->type == IB_FLOW_SPEC_IPV4)
		ipv4_valid = true;
	if (flow_tag->type == IB_FLOW_SPEC_ACTION_TAG)
		tag_valid = true;

	if (!(ipv4_valid && tag_valid))
		return -EOPNOTSUPP;

	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return -ENOMEM;

	flow->session = session;
	session->l4_proto = ipv4_spec->val.proto;
	session->ifa_gns = flow_tag->tag_id;
	session->vnic_id = qp->qplib_qp.vnic_id;
	session->type = DCQCN_FLOW_TYPE_ROCE_MONITOR;

	rc = bnxt_en_ulp_dcqcn_flow_create(rdev->en_dev, session);
	if (rc)
		kfree(session);
	return rc;
}

static void bnxt_re_destroy_normal_flow(struct bnxt_re_flow *flow)
{
	struct bnxt_re_dev *rdev = flow->rdev;
	int rc;

	rc = bnxt_en_ulp_dcqcn_flow_delete(rdev->en_dev,
					   flow->session);
	if (rc)
		dev_dbg(rdev_to_dev(rdev), "failed to delete normal flow rc = %d\n", rc);

	kfree(flow->session);
}

static int bnxt_re_setup_vnic(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp,
			      bool is_sniffer)
{
	u16 vnic_id = 0;
	int rc;

	if (is_sniffer) {
		rc = bnxt_re_hwrm_qcfg(rdev);
		if (rc)
			return rc;
		vnic_id = rdev->mirror_vnic_id;
	}

	rc = bnxt_re_hwrm_alloc_vnic(rdev, &vnic_id, is_sniffer);
	if (rc)
		return rc;

	rc = bnxt_re_hwrm_cfg_vnic(rdev, qp->qplib_qp.id, vnic_id);
	if (rc)
		goto out_free_vnic;

	qp->qplib_qp.vnic_id = vnic_id;

	return rc;

out_free_vnic:
	bnxt_re_hwrm_free_vnic(rdev, vnic_id);
	return rc;
}

struct ib_flow *bnxt_re_create_flow(struct ib_qp *ib_qp, struct ib_flow_attr *attr,
#if defined(HAVE_CREATE_FLOW_DOMAIN_UDATA)
				    int domain, struct ib_udata *udata)
#elif defined(HAVE_CREATE_FLOW_UDATA)
				    struct ib_udata *udata)
#else
				    int domain)
#endif
{
	struct bnxt_re_qp *qp = to_bnxt_re(ib_qp, struct bnxt_re_qp, ib_qp);
	struct bnxt_re_dev *rdev = qp->rdev;
	struct bnxt_re_flow *flow;
	int rc;

	if (!is_flow_supported(rdev, attr))
		return ERR_PTR(-EOPNOTSUPP);
	mutex_lock(&rdev->qp_lock);
	if (attr->type == IB_FLOW_ATTR_SNIFFER && rdev->sniffer_flow_created) {
		dev_err(rdev_to_dev(rdev), "RoCE Mirroring is already Configured\n");
		mutex_unlock(&rdev->qp_lock);
		return ERR_PTR(-EBUSY);
	}
	flow = kzalloc(sizeof(*flow), GFP_KERNEL);
	if (!flow) {
		mutex_unlock(&rdev->qp_lock);
		return ERR_PTR(-ENOMEM);
	}
	flow->rdev = rdev;
	flow->qp = qp;
	flow->type = attr->type;

	rc = bnxt_re_setup_vnic(rdev, qp, (attr->type == IB_FLOW_ATTR_SNIFFER));
	if (rc)
		goto out_free_flow;

	switch (attr->type) {
	case IB_FLOW_ATTR_SNIFFER:
		rc = bnxt_re_create_sniffer_flow(rdev, flow);
		if (rc)
			goto out_free_vnic;
		rdev->sniffer_flow_created = 1;
		break;
	case IB_FLOW_ATTR_NORMAL:
		rc = bnxt_re_create_normal_flow(rdev, attr, flow);
		if (rc)
			goto out_free_vnic;
		break;
	default:
		rc = -EOPNOTSUPP;
		goto out_free_vnic;
	}
	mutex_unlock(&rdev->qp_lock);
	INIT_LIST_HEAD(&flow->list);
	mutex_lock(&qp->flow_lock);
	list_add_tail(&flow->list, &qp->flow_list);
	mutex_unlock(&qp->flow_lock);

	return &flow->ib_flow;

out_free_vnic:
	bnxt_re_hwrm_free_vnic(rdev, qp->qplib_qp.vnic_id);

out_free_flow:
	mutex_unlock(&rdev->qp_lock);
	kfree(flow);
	return ERR_PTR(rc);
}

int bnxt_re_destroy_flow(struct ib_flow *flow_id)
{
	struct bnxt_re_flow *flow = container_of(flow_id,
						 struct bnxt_re_flow, ib_flow);
	struct bnxt_re_dev *rdev = flow->rdev;
	struct bnxt_re_qp *qp = flow->qp;

	mutex_lock(&rdev->qp_lock);
	switch (flow->type) {
	case IB_FLOW_ATTR_SNIFFER:
		bnxt_re_destroy_sniffer_flow(flow);
		rdev->sniffer_flow_created = 0;
		break;
	case IB_FLOW_ATTR_NORMAL:
		bnxt_re_destroy_normal_flow(flow);
		break;
	default:
		break;
	}
	bnxt_re_hwrm_free_vnic(rdev, qp->qplib_qp.vnic_id);
	mutex_unlock(&rdev->qp_lock);
	mutex_lock(&qp->flow_lock);
	list_del(&flow->list);
	mutex_unlock(&qp->flow_lock);

	kfree(flow);

	return 0;
}

#ifdef HAVE_DISASSOCIATE_UCNTX
#ifndef HAVE_RDMA_USER_MMAP_IO
#ifndef HAVE_NO_MM_MMAP_SEM
static struct mm_struct *bnxt_re_is_task_pending(struct ib_ucontext *ib_uctx,
						 struct task_struct **task)
{
	struct mm_struct   *mm;

	*task = get_pid_task(ib_uctx->tgid, PIDTYPE_PID);
	if (!*task)
		return NULL;

	mm = get_task_mm(*task);
	if (!mm) {
		pr_info("no mm, disassociate ucontext is pending task termination\n");
		while (1) {
			put_task_struct(*task);
			usleep_range(1000, 2000);
			*task = get_pid_task(ib_uctx->tgid, PIDTYPE_PID);
			if (!*task || (*task)->state == TASK_DEAD) {
				pr_info("disassociate ucontext done, task was terminated\n");
				/* in case task was dead need to release the
				 * task struct.
				 */
				if (*task)
					put_task_struct(*task);
				return NULL;
			}
		}
	}

	return mm;
}
#endif /* HAVE_NO_MM_MMAP_SEM */

static void bnxt_re_traverse_vma_list(struct ib_ucontext *ib_uctx)
{
	struct bnxt_re_vma_data *vma_data, *n;
	struct bnxt_re_ucontext *uctx;
	struct vm_area_struct *vma;

	uctx = to_bnxt_re(ib_uctx, struct bnxt_re_ucontext, ib_uctx);

	mutex_lock(&uctx->list_mutex);
	list_for_each_entry_safe(vma_data, n, &uctx->vma_list_head, vma_list) {
		vma = vma_data->vma;
		zap_vma_ptes(vma, vma->vm_start, PAGE_SIZE);
		/* context going to be destroyed, should
		 * not access ops any more
		 */
		vma->vm_flags &= ~(VM_SHARED | VM_MAYSHARE);
		vma->vm_ops = NULL;
		list_del(&vma_data->vma_list);
		kfree(vma_data);
	}
	mutex_unlock(&uctx->list_mutex);
}
#endif /* HAVE_RDMA_USER_MMAP_IO */

void bnxt_re_disassociate_ucntx(struct ib_ucontext *ib_uctx)
{
#ifndef HAVE_RDMA_USER_MMAP_IO
/*
 * For kernels that have rdma_user_mmap_io() the .disassociate_ucontext
 * implementation in driver is a stub
 */
#ifndef HAVE_NO_MM_MMAP_SEM
	struct task_struct *task = NULL;
	struct mm_struct *mm = NULL;

	mm = bnxt_re_is_task_pending(ib_uctx, &task);
	if (!mm)
		return;
	/* need to protect from a race on closing the vma as part of
	 * vma_close.
	 */
	down_write(&mm->mmap_sem);
#endif /* HAVE_NO_MM_MMAP_SEM */

	bnxt_re_traverse_vma_list(ib_uctx);

#ifndef HAVE_NO_MM_MMAP_SEM
	up_write(&mm->mmap_sem);
	mmput(mm);
	put_task_struct(task);
#endif /* HAVE_NO_MM_MMAP_SEM */
#endif /* HAVE_RDMA_USER_MMAP_IO */
}

int bnxt_re_modify_udp_sport(struct bnxt_re_qp *qp, uint32_t port)
{
	if (!qp)
		return -EINVAL;
	if (!_is_modify_udp_sport_supported(qp->rdev->dev_attr->dev_cap_ext_flags2) ||
	    !qp->qplib_qp.is_ses_eligible)
		return -EOPNOTSUPP;
	if (qp->qplib_qp.cur_qp_state != CMDQ_MODIFY_QP_NEW_STATE_RTS)
		return 0;

	qp->qplib_qp.modify_flags = 0;
	qp->qplib_qp.ext_modify_flags = 0;
	qp->qplib_qp.ext_modify_flags |= CMDQ_MODIFY_QP_EXT_MODIFY_MASK_UDP_SRC_PORT_VALID;
	qp->qplib_qp.udp_sport = port;
	return bnxt_qplib_modify_qp(&qp->rdev->qplib_res, &qp->qplib_qp);
}

int bnxt_re_modify_rate_limit(struct bnxt_re_qp *qp, uint32_t rate_limit)
{
	if (!qp)
		return -EINVAL;
	if (!_is_modify_qp_rate_limit_supported(qp->rdev->dev_attr->dev_cap_ext_flags2))
		return -EOPNOTSUPP;

	qp->qplib_qp.modify_flags = 0;
	qp->qplib_qp.ext_modify_flags = CMDQ_MODIFY_QP_EXT_MODIFY_MASK_RATE_LIMIT_VALID;
	qp->qplib_qp.rate_limit = rate_limit;
	return bnxt_qplib_modify_qp(&qp->rdev->qplib_res, &qp->qplib_qp);
}

#endif
