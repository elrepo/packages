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
 * Description: Compat file for supporting multiple distros
 */

#include <linux/types.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_umem.h>

#if defined(HAVE_IB_UMEM_DMABUF) && !defined(HAVE_IB_UMEM_DMABUF_PINNED)
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>
#endif

#include "bnxt_ulp.h"
#include "bnxt_re.h"
#include "bnxt_re-abi.h"

#ifndef RHEL_RELEASE_CODE
#define RHEL_RELEASE_CODE 0
#endif

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a, b)       (((a) << 8) + (b))
#endif

int bnxt_re_register_netdevice_notifier(struct notifier_block *nb)
{
	int rc;
#ifdef HAVE_REGISTER_NETDEVICE_NOTIFIER_RH
	rc = register_netdevice_notifier_rh(nb);
#else
	rc = register_netdevice_notifier(nb);
#endif
	return rc;
}

int bnxt_re_unregister_netdevice_notifier(struct notifier_block *nb)
{
	int rc;
#ifdef HAVE_REGISTER_NETDEVICE_NOTIFIER_RH
	rc = unregister_netdevice_notifier_rh(nb);
#else
	rc = unregister_netdevice_notifier(nb);
#endif
	return rc;
}

#ifdef HAVE_IB_MW_BIND_INFO
struct ib_mw_bind_info *get_bind_info(struct ib_send_wr *wr)
{
#ifdef HAVE_IB_BIND_MW_WR
	struct ib_bind_mw_wr *bind_mw = bind_mw_wr(wr);

	return &bind_mw->bind_info;
#else
	return &wr->wr.bind_mw.bind_info;
#endif
}

struct ib_mw *get_ib_mw(struct ib_send_wr *wr)
{
#ifdef HAVE_IB_BIND_MW_WR
	struct ib_bind_mw_wr *bind_mw = bind_mw_wr(wr);

	return bind_mw->mw;
#else
	return wr->wr.bind_mw.mw;
#endif
}
#endif

struct scatterlist *get_ib_umem_sgl(struct ib_umem *umem, u32 *nmap)
{
#if !defined(HAVE_IB_UMEM_SG_TABLE) && !defined(HAVE_IB_UMEM_SG_APPEND_TABLE)
	struct ib_umem_chunk *chunk;
	struct scatterlist **sg = NULL;
	u32 sg_nmap = 0;
	int i = 0, j;
	size_t n = 0;
#endif

#if defined HAVE_IB_UMEM_SG_APPEND_TABLE
	*nmap = umem->sgt_append.sgt.nents;
	return umem->sgt_append.sgt.sgl;
#elif defined HAVE_IB_UMEM_SG_TABLE
	*nmap = umem->nmap;
	return umem->sg_head.sgl;
#else
	list_for_each_entry(chunk, &umem->chunk_list, list)
		n += chunk->nmap;

	*sg = kcalloc(n, sizeof(*sg), GFP_KERNEL);
	if (!(*sg)) {
		*nmap = 0;
		return NULL;
	}
	list_for_each_entry(chunk, &umem->chunk_list, list) {
		for (j = 0; j < chunk->nmap; ++j)
			sg[i++] = &chunk->page_list[j];
		sg_nmap += chunk->nmap;
	}
	*nmap = sg_nmap;
	return *sg;
#endif
}

u8 bnxt_re_get_bond_link_status(struct bnxt_re_bond_info *binfo)
{
#ifdef HAVE_NET_BONDING_H
	struct list_head *iter;
	struct bonding *bond;
	struct slave *slave;
	u8 active_port_map = 0;
	u32 link;
	u8 port;

	if (!binfo)
		return 0;

	bond = netdev_priv(binfo->master);
	bond_for_each_slave(bond, slave, iter) {
		if (slave->dev == binfo->slave1)
			port = 0;
		else
			port = 1;
		link = bond_slave_can_tx(slave);
		active_port_map |= (link << port);
	}

	return active_port_map;
#else
	return (netif_carrier_ok(binfo->slave1)) |
		(netif_carrier_ok(binfo->slave2) << 1);
#endif
}

/* Checks if adapter supports LAG. For Thor, LAG support is
 * implemented in FW.  For Thor2, LAG is implemented in HW. For Wh+
 * and Stratus only active-active LAG is supported.
 */
static bool _is_bnxt_re_dev_lag_supported(const struct bnxt_re_dev *rdev)
{
	return _is_bnxt_re_dev_lag_capable(rdev->dev_attr->dev_cap_flags) ^
		BNXT_EN_HW_LAG(rdev->en_dev);
}

bool bnxt_re_is_lag_allowed(ifbond *master, ifslave *slave,
			    struct bnxt_re_dev *rdev)
{
	struct net_device *pf_peer_master;
	struct net_device *pf_in_master;
	struct bnxt_re_dev *pf_peer;
	bool lag_supported = false;
	int pf_cnt;

	rtnl_lock();
	pf_in_master = netdev_master_upper_dev_get(rdev->netdev);

	if (strncmp(rdev->netdev->name, slave->slave_name, IFNAMSIZ))
		goto exit;

	/* Check if fw/hw supports LAG */
	if (!_is_bnxt_re_dev_lag_supported(rdev)) {
		dev_info(rdev_to_dev(rdev), "Device is not capable of supporting RoCE LAG\n");
		goto exit;
	}

	if (rdev->en_dev->port_count != 2) {
		dev_info(rdev_to_dev(rdev),
			 "RoCE LAG not supported on phy port count %d\n",
			 rdev->en_dev->port_count);
		goto exit;
	}

	if (BNXT_EN_MH(rdev->en_dev)) {
		dev_info(rdev_to_dev(rdev), "RoCE LAG not supported on multi host\n");
		goto exit;
	}

	if (BNXT_EN_MR(rdev->en_dev)) {
		dev_info(rdev_to_dev(rdev), "RoCE LAG not supported on multi root\n");
		goto exit;
	}

	/* Master must have only 2 slaves */
	if (master->num_slaves != 2)
		goto exit;

	/* PF count on our device can't be more than 2 */
	pf_cnt = bnxt_re_get_slot_pf_count(rdev);
	if (pf_cnt > BNXT_RE_BOND_PF_MAX)
		goto exit;

	/* Get the other PF */
	pf_peer = bnxt_re_get_peer_pf(rdev);
	if (!pf_peer)
		goto exit;

	/* Check if the PF-peer has a Master netdev */
	pf_peer_master = netdev_master_upper_dev_get(pf_peer->netdev);
	if (!pf_peer_master)
		goto exit;

	/* Master netdev of PF-peer must be same as ours */
	if (pf_in_master != pf_peer_master)
		goto exit;

	/* Don't allow LAG on NPAR PFs */
	if (BNXT_EN_NPAR(rdev->en_dev) || BNXT_EN_NPAR(pf_peer->en_dev))
		goto exit;

	/* In Thor+, bond mode 0 is not supported */
	if (BNXT_EN_HW_LAG(rdev->en_dev) &&
	    master->bond_mode == BOND_MODE_ROUNDROBIN) {
		dev_info(rdev_to_dev(rdev),
			 "RoCE LAG not supported for bond_mode %d when hw lag enabled.\n",
			 master->bond_mode);
		goto exit;
	}

	/* Bonding mode must be 1, 2 or 4 */
	if ((master->bond_mode != BOND_MODE_ACTIVEBACKUP) &&
	    (master->bond_mode != BOND_MODE_XOR) &&
	    (master->bond_mode != BOND_MODE_ROUNDROBIN) &&
	    (master->bond_mode != BOND_MODE_8023AD)) {
		dev_info(rdev_to_dev(rdev),
			 "RoCE LAG not supported for bond_mode %d\n",
			 master->bond_mode);
		goto exit;
	}

	lag_supported = true;

exit:
	rtnl_unlock();
	return lag_supported;
}

static u32 bnxt_re_get_bond_info_port(struct bnxt_re_bond_info *binfo,
		struct net_device *netdev)
{
	u32 port = 0;

	if (!binfo)
		return 0;

	if (binfo->slave1 == netdev)
		port = 1;
	else if (binfo->slave2 == netdev)
		port = 2;
	return port;
}

int bnxt_re_get_port_map(struct netdev_bonding_info *netdev_binfo,
		struct bnxt_re_bond_info *binfo,
		struct net_device *netdev)
{
	u32 port = bnxt_re_get_bond_info_port(binfo, netdev);

	if (!port)
		return -ENODEV;

	dev_dbg(rdev_to_dev(binfo->rdev),
			"%s: port = %d\n", __func__, port);
	if (netdev_binfo->master.bond_mode == BOND_MODE_ACTIVEBACKUP) {
		dev_dbg(rdev_to_dev(binfo->rdev),
			"%s: active backup mode\n", __func__);
		if (netdev_binfo->slave.state == BOND_STATE_BACKUP) {
			/*
			 * If this slave is now in "backup mode", then
			 * other slave is in "active mode".
			 */
			binfo->active_port_map = (port == 1) ?
				BNXT_RE_ACTIVE_MAP_PORT2:
				BNXT_RE_ACTIVE_MAP_PORT1;
		} else { /* Slave state is active */
			/*
			 * If this slave is now in "active mode", then
			 * other slave is in "backup mode".
			 */
			binfo->active_port_map = (port == 1) ?
				BNXT_RE_ACTIVE_MAP_PORT1 :
				BNXT_RE_ACTIVE_MAP_PORT2;
		}
	} else { /* Active - Active */
		binfo->active_port_map = bnxt_re_get_bond_link_status(binfo);
		dev_info(rdev_to_dev(binfo->rdev),
			 "LAG mode = active-active binfo->active_port_map = 0x%x\n",
			 binfo->active_port_map);
	}
	dev_dbg(rdev_to_dev(binfo->rdev),
		"binfo->aggr_mode = 0x%x binfo->active_port_map = 0x%x\n",
		binfo->aggr_mode, binfo->active_port_map);
	return 0;
}

#ifdef CONFIG_INFINIBAND_PEER_MEM
void bnxt_re_set_inflight_invalidation_ctx(struct ib_umem *umem)
{
#ifdef IB_PEER_MEM_MOD_SUPPORT
	struct ib_peer_umem *peer_umem = ib_peer_mem_get_data(umem);

	peer_umem->invalidation_ctx->inflight_invalidation = 1;
#else
#ifdef HAVE_IB_UMEM_GET_FLAGS
		umem->invalidation_ctx->inflight_invalidation = 1;
#endif
#endif /* IB_PEER_MEM_MOD_SUPPORT */
}

void *bnxt_re_get_peer_mem(struct ib_umem *umem)
{

#ifdef IB_PEER_MEM_MOD_SUPPORT
	struct ib_peer_umem *peer_umem = NULL;
	peer_umem = ib_peer_mem_get_data(umem);
	dev_dbg(NULL,
		"%s: %d peer_umem = %p\n", __func__, __LINE__, peer_umem);
	return (void *) peer_umem;
#else
#ifdef HAVE_IB_UMEM_GET_FLAGS
	return (void *)umem->ib_peer_mem;
#else
	if (umem->is_peer)
		return (void *)umem;
	return NULL;
#endif
#endif
}
#endif /* CONFIG_INFINIBAND_PEER_MEM */

void bnxt_re_peer_mem_release(struct ib_umem *umem)
{
#if defined(HAVE_IB_UMEM_DMABUF) && !defined(HAVE_IB_UMEM_DMABUF_PINNED)
	if (umem && umem->is_dmabuf) {
		ib_umem_dmabuf_release_pinned(to_ib_umem_dmabuf(umem));
		return;
	}
#endif

#ifdef CONFIG_INFINIBAND_PEER_MEM
		dev_dbg(NULL, "ib_umem_release_flags getting invoked \n");
#ifdef HAVE_IB_UMEM_GET_FLAGS
		ib_umem_release_flags(umem);
#else
		ib_umem_release(umem);
#endif
#else
		dev_dbg(NULL, "ib_umem_release getting invoked \n");
		ib_umem_release(umem);
#endif
}

void bnxt_re_set_dma_device(struct ib_device *ibdev, struct bnxt_re_dev *rdev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 3) && \
	!(defined(RHEL_RELEASE_CODE) && ((RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,5))))
	/* From 4.11.3 kernel version and RHEL 7.5 onwards, IB HW drivers no longer set
	 * dma_device directly. However,they are expected to set the
	 * ibdev->dev.parent field before calling ib_register_device()
	 */
	ibdev->dma_device = &rdev->en_dev->pdev->dev;
#else
	ibdev->dev.parent = &rdev->en_dev->pdev->dev;
#endif
}

#if defined(RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP) || defined(ENABLE_ROCEV2_QP1)
int bnxt_re_get_cached_gid(struct ib_device *dev, u8 port_num, int index,
			   union ib_gid *sgid, struct ib_gid_attr **sgid_attr,
			   struct ib_global_route *grh, struct ib_ah *ah)
{
	int ret = 0;

#ifndef HAVE_GID_ATTR_IN_IB_AH
#ifndef HAVE_IB_GET_CACHED_GID
	if (grh)
		*sgid_attr = grh->sgid_attr;
	else if (ah)
		*sgid_attr = ah->sgid_attr;
	else {
		*sgid_attr = NULL;
		ret = -EFAULT;
	}
#else
	ret = ib_get_cached_gid(dev, port_num, index, sgid, *sgid_attr);
#endif
#endif
	return ret;
}

enum rdma_network_type bnxt_re_gid_to_network_type(IB_GID_ATTR *sgid_attr,
						   union ib_gid *sgid)
{
#ifdef HAVE_RDMA_GID_ATTR_NETWORK_TYPE
	return rdma_gid_attr_network_type(sgid_attr);
#else
	return ib_gid_to_network_type(sgid_attr->gid_type, sgid);
#endif
}
#endif

int ib_register_device_compat(struct bnxt_re_dev *rdev)
{
	struct ib_device *ibdev = &rdev->ibdev;
	char name[IB_DEVICE_NAME_MAX];

	memset(name, 0, IB_DEVICE_NAME_MAX);
	if (rdev->binfo)
		strscpy(name, "bnxt_re_bond%d", IB_DEVICE_NAME_MAX);
	else
		strscpy(name, "bnxt_re%d", IB_DEVICE_NAME_MAX);

#ifndef HAVE_NAME_IN_IB_REGISTER_DEVICE
	strscpy(ibdev->name, name, IB_DEVICE_NAME_MAX);

#ifdef HAVE_DMA_DEVICE_IN_IB_REGISTER_DEVICE
	dma_set_max_seg_size(&rdev->en_dev->pdev->dev, SZ_2G);
	return ib_register_device(ibdev, name, &rdev->en_dev->pdev->dev);
#else
	return ib_register_device(ibdev, NULL);
#endif
#else
#ifndef HAVE_VERB_INIT_PORT
	return ib_register_device(ibdev, name, NULL);
#else
	return ib_register_device(ibdev,name);
#endif
#endif
}

bool ib_modify_qp_is_ok_compat(enum ib_qp_state cur_state, enum ib_qp_state next_state,
			       enum ib_qp_type type, enum ib_qp_attr_mask mask)
{
		return (ib_modify_qp_is_ok(cur_state, next_state,
				        type, mask
#ifdef HAVE_LL_IN_IB_MODIFY_QP_IS_OK
					,IB_LINK_LAYER_ETHERNET
#endif
	));
}

void bnxt_re_init_resolve_wq(struct bnxt_re_dev *rdev)
{
	rdev->resolve_wq = create_singlethread_workqueue("bnxt_re_resolve_wq");
	 INIT_LIST_HEAD(&rdev->mac_wq_list);
}

void bnxt_re_uninit_resolve_wq(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_resolve_dmac_work *tmp_work = NULL, *tmp_st;
	if (!rdev->resolve_wq)
		return;
	flush_workqueue(rdev->resolve_wq);
	list_for_each_entry_safe(tmp_work, tmp_st, &rdev->mac_wq_list, list) {
			list_del(&tmp_work->list);
			kfree(tmp_work);
	}
	destroy_workqueue(rdev->resolve_wq);
	rdev->resolve_wq = NULL;
}

void bnxt_re_resolve_dmac_task(struct work_struct *work)
{
	int rc = -1;
	struct bnxt_re_dev *rdev;
	RDMA_AH_ATTR	*ah_attr;
	struct bnxt_re_ah_info *ah_info;
	int if_index;
	struct bnxt_re_resolve_dmac_work *dmac_work =
			container_of(work, struct bnxt_re_resolve_dmac_work, work);

	if_index = 0;
	rdev = dmac_work->rdev;
	ah_attr = dmac_work->ah_attr;
	ah_info = dmac_work->ah_info;
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
#ifdef HAVE_IB_RESOLVE_ETH_DMAC
		rc = ib_resolve_eth_dmac(&rdev->ibdev, ah_attr);
#else
#ifndef HAVE_CREATE_USER_AH
#ifndef HAVE_RDMA_ADDR_FIND_L2_ETH_BY_GRH_WITH_NETDEV
		if_index = ah_info->sgid_attr.ndev->ifindex;
		rc = rdma_addr_find_l2_eth_by_grh(&ah_info->sgid,
				&ah_attr->grh.dgid, ROCE_DMAC(ah_attr),
				&ah_info->vlan_tag,
				&if_index, NULL);
#endif  /* HAVE_RDMA_ADDR_FIND_L2_ETH_BY_GRH_WITH_NETDEV */
#endif /* else dmac is resolved by stack */
#endif /* HAVE_IB_RESOLVE_ETH_DMAC */

#else /* RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP */
#ifdef HAVE_RDMA_ADDR_FIND_DMAC_BY_GRH_V2
		rc = rdma_addr_find_dmac_by_grh(&ah_info->sgid,
						&ah_attr->grh.dgid,
						ah_attr->dmac, NULL, 0);
#endif
#endif
	if (rc)
		dev_err(rdev_to_dev(dmac_work->rdev),
			"Failed to resolve dest mac rc = %d\n", rc);
	atomic_set(&dmac_work->status_wait, rc << 8);
}

#ifndef HAS_ENABLE_ATOMIC_OPS
/**
 * pci_enable_atomic_ops_to_root - enable AtomicOp requests to root port
 * @dev: the PCI device
 * @cap_mask: mask of desired AtomicOp sizes, including one or more of:
 *	PCI_EXP_DEVCAP2_ATOMIC_COMP32
 *	PCI_EXP_DEVCAP2_ATOMIC_COMP64
 *	PCI_EXP_DEVCAP2_ATOMIC_COMP128
 *
 * Return 0 if all upstream bridges support AtomicOp routing, egress
 * blocking is disabled on all upstream ports, and the root port supports
 * the requested completion capabilities (32-bit, 64-bit and/or 128-bit
 * AtomicOp completion), or negative otherwise.
 */
int pci_enable_atomic_ops_to_root(struct pci_dev *dev, u32 cap_mask)
{
	struct pci_bus *bus = dev->bus;
	struct pci_dev *bridge;
	u32 cap;

	if (!pci_is_pcie(dev))
		return -EINVAL;

	/*
	 * Per PCIe r4.0, sec 6.15, endpoints and root ports may be
	 * AtomicOp requesters.  For now, we only support endpoints as
	 * requesters and root ports as completers.  No endpoints as
	 * completers, and no peer-to-peer.
	 */

	switch (pci_pcie_type(dev)) {
	case PCI_EXP_TYPE_ENDPOINT:
	case PCI_EXP_TYPE_LEG_END:
	case PCI_EXP_TYPE_RC_END:
		break;
	default:
		return -EINVAL;
	}

	while (bus->parent) {
		bridge = bus->self;

		pcie_capability_read_dword(bridge, PCI_EXP_DEVCAP2, &cap);

		switch (pci_pcie_type(bridge)) {
		/* Ensure switch ports support AtomicOp routing */
		case PCI_EXP_TYPE_UPSTREAM:
		case PCI_EXP_TYPE_DOWNSTREAM:
			if (!(cap & PCI_EXP_DEVCAP2_ATOMIC_ROUTE))
				return -EINVAL;
			break;

		/* Ensure root port supports all the sizes we care about */
		case PCI_EXP_TYPE_ROOT_PORT:
			if ((cap & cap_mask) != cap_mask)
				return -EINVAL;
			break;
		}
#if !(defined(KYLIN_MAJOR) && (KYLIN_MAJOR == 10) && (KYLIN_MINOR == 3)) && \
	defined(HAS_PCI_SECONDARY_LINK)
		/* TODO: In old kernel not checking  may cause crashes */
		/* Ensure upstream ports don't block AtomicOps on egress */
		if (!bridge->has_secondary_link) {
			u32 ctl2;
			pcie_capability_read_dword(bridge, PCI_EXP_DEVCTL2,
						   &ctl2);
			if (ctl2 & PCI_EXP_DEVCTL2_ATOMIC_EGRESS_BLOCK)
				return -EINVAL;
		}
#endif
		bus = bus->parent;
	}

	pcie_capability_set_word(dev, PCI_EXP_DEVCTL2,
				 PCI_EXP_DEVCTL2_ATOMIC_REQ);
	return 0;
}
#endif

void bnxt_re_umem_free(struct ib_umem **umem)
{
	if (IS_ERR_OR_NULL(*umem))
		return;

	ib_umem_release(*umem);
	*umem = NULL;
}

struct ib_umem *ib_umem_get_compat(struct bnxt_re_dev *rdev,
				   struct ib_ucontext *ucontext,
				   struct ib_udata *udata,
				   unsigned long addr,
				   size_t size, int access, int dmasync)
{
	struct ib_umem *umem;

#ifdef HAVE_IB_DEVICE_IN_IB_UMEM_GET
	umem = ib_umem_get(&rdev->ibdev, addr, size, access);
#else
#ifndef HAVE_UDATA_IN_IB_UMEM_GET
	umem = ib_umem_get(ucontext, addr, size, access, dmasync);
#else
	umem = ib_umem_get(udata, addr, size, access
#ifdef HAVE_DMASYNC_IB_UMEM_GET
			, dmasync
#endif
			);
#endif
#endif
	if (!umem)
		umem = ERR_PTR(-EIO);
	return umem;
}

#ifdef HAVE_IB_UMEM_GET_PEER
static struct ib_umem *ib_umem_get_peer_compat(struct bnxt_re_dev *rdev,
					       struct ib_udata *udata,
					       unsigned long addr,
					       size_t size, int access, int dmasync)
{
#ifdef HAVE_IB_DEVICE_IN_IB_UMEM_GET
	return ib_umem_get_peer(&rdev->ibdev, addr, size, access,
				IB_PEER_MEM_INVAL_SUPP);
#else
	return ib_umem_get_peer(udata, addr, size, access,
				IB_PEER_MEM_INVAL_SUPP);
#endif
}
#endif

struct ib_umem *ib_umem_get_flags_compat(struct bnxt_re_dev *rdev,
					 struct ib_ucontext *ucontext,
					 struct ib_udata *udata,
					 unsigned long addr,
					 size_t size, int access, int dmasync)
{
	struct ib_umem *umem;

#ifdef HAVE_IB_UMEM_GET_PEER
	umem = ib_umem_get_peer_compat(rdev, udata, addr, size, access,
				       IB_PEER_MEM_INVAL_SUPP);
#else
#ifdef HAVE_IB_UMEM_GET_FLAGS
	umem = ib_umem_get_flags(&rdev->ibdev, ucontext, udata, addr, size,
				 access,
#ifdef CONFIG_INFINIBAND_PEER_MEM
				 IB_UMEM_PEER_ALLOW | IB_UMEM_PEER_INVAL_SUPP |
#endif
				 0);
#else
	umem = ib_umem_get_compat(rdev, ucontext, udata, addr, size,
				  access, 0);
#endif
#endif
	if (!umem)
		umem = ERR_PTR(-EIO);
	return umem;
}

int __bnxt_re_set_vma_data(void *bnxt_re_uctx,
			   struct vm_area_struct *vma)
{
#ifdef HAVE_DISASSOCIATE_UCNTX
#ifndef HAVE_RDMA_USER_MMAP_IO
	return bnxt_re_set_vma_data(bnxt_re_uctx, vma);
#endif
#endif
	return 0;
}

int remap_pfn_compat(struct ib_ucontext *ib_uctx,
		     struct vm_area_struct *vma,
		     u64 pfn)
{
	if (vma->vm_pgoff) {
#ifndef HAVE_RDMA_USER_MMAP_IO
		return io_remap_pfn_range(vma, vma->vm_start, pfn, PAGE_SIZE,
					  vma->vm_page_prot);
#else
		return rdma_user_mmap_io(ib_uctx, vma, pfn, PAGE_SIZE,
					 vma->vm_page_prot
#ifdef HAVE_RDMA_USER_MMAP_IO_USE_MMAP_ENTRY
					 , NULL
#endif
					 );
#endif
	} else {
#ifndef HAVE_RDMA_USER_MMAP_IO
		return remap_pfn_range(vma, vma->vm_start,
				       pfn, PAGE_SIZE, vma->vm_page_prot);
#else
		return rdma_user_mmap_io(ib_uctx, vma, pfn, PAGE_SIZE,
					 vma->vm_page_prot
#ifdef HAVE_RDMA_USER_MMAP_IO_USE_MMAP_ENTRY
					 , NULL
#endif
					 );
#endif
	}
}

struct bnxt_re_cq *__get_cq_from_cq_in(ALLOC_CQ_IN *cq_in,
				       struct bnxt_re_dev *rdev)
{
	struct bnxt_re_cq *cq;
#ifndef HAVE_CQ_ALLOC_IN_IB_CORE
	cq = kzalloc(sizeof(*cq), GFP_KERNEL);
	if (!cq)
		dev_err(rdev_to_dev(rdev), "Allocate CQ failed!");
#else
	cq = container_of(cq_in, struct bnxt_re_cq, ib_cq);
#endif
	return cq;
}

struct bnxt_re_qp *__get_qp_from_qp_in(ALLOC_QP_IN *qp_in,
				       struct bnxt_re_dev *rdev)
{
	struct bnxt_re_qp *qp;

#ifdef HAVE_QP_ALLOC_IN_IB_CORE
	qp = container_of(qp_in, struct bnxt_re_qp, ib_qp);
#else
	qp = kzalloc(sizeof(*qp), GFP_KERNEL);
	if (!qp)
		dev_err(rdev_to_dev(rdev), "Allocate QP failed!");
#endif
	return qp;
}

bool bnxt_re_check_if_vlan_valid(struct bnxt_re_dev *rdev,
				 u16 vlan_id)
{
	bool ret = true;
	/*
	 * Check if the vlan is configured in the host.
	 * If not configured, it  can be a transparent
	 * VLAN. So dont report the vlan id.
	 */
#ifdef HAVE_VLAN_FIND_DEV_DEEP_RCU
	if (!__vlan_find_dev_deep_rcu(rdev->netdev,
				      htons(ETH_P_8021Q), vlan_id))
		ret = false;
#endif
	return ret;
}

#if defined(HAVE_IB_UMEM_DMABUF) && !defined(HAVE_IB_UMEM_DMABUF_PINNED)
static void
ib_umem_dmabuf_unsupported_move_notify(struct dma_buf_attachment *attach)
{
	struct ib_umem_dmabuf *umem_dmabuf = attach->importer_priv;

	ibdev_warn_ratelimited(umem_dmabuf->umem.ibdev,
			       "Invalidate callback is called when memory is pinned!\n");
}

static struct dma_buf_attach_ops ib_umem_dmabuf_attach_pinned_ops = {
	.allow_peer2peer = true,
	.move_notify = ib_umem_dmabuf_unsupported_move_notify,
};

struct ib_umem_dmabuf *ib_umem_dmabuf_get_pinned(struct ib_device *device,
						 unsigned long offset,
						 size_t size, int fd,
						 int access)
{
	struct ib_umem_dmabuf *umem_dmabuf;
	struct dma_buf *dmabuf;
	int err;

	umem_dmabuf = ib_umem_dmabuf_get(device, offset, size, fd, access,
					 &ib_umem_dmabuf_attach_pinned_ops);
	if (IS_ERR(umem_dmabuf))
		return umem_dmabuf;

	dmabuf = umem_dmabuf->attach->dmabuf;
	dma_resv_lock(dmabuf->resv, NULL);
	err = dma_buf_pin(umem_dmabuf->attach);
	if (err)
		goto err_release;
	err = ib_umem_dmabuf_map_pages(umem_dmabuf);
	if (err)
		goto err_unpin;
	dma_resv_unlock(dmabuf->resv);

	return umem_dmabuf;

err_unpin:
	dma_buf_unpin(umem_dmabuf->attach);
err_release:
	dma_resv_unlock(dmabuf->resv);
	dma_buf_detach(dmabuf, umem_dmabuf->attach);
	dma_buf_put(dmabuf);
	kfree(umem_dmabuf);
	return ERR_PTR(err);
}

void ib_umem_dmabuf_release_pinned(struct ib_umem_dmabuf *umem_dmabuf)
{
	struct dma_buf *dmabuf = umem_dmabuf->attach->dmabuf;

	dma_resv_lock(dmabuf->resv, NULL);
	ib_umem_dmabuf_unmap_pages(umem_dmabuf);
	dma_buf_unpin(umem_dmabuf->attach);
	dma_resv_unlock(dmabuf->resv);

	dma_buf_detach(dmabuf, umem_dmabuf->attach);
	dma_buf_put(dmabuf);
	kfree(umem_dmabuf);
}
#endif

void bnxt_re_get_width_and_speed(u32 netdev_speed, u32 lanes,
				 u16 *speed, u8 *width)
{
	if (!lanes) {
		switch (netdev_speed) {
		case SPEED_1000:
			*speed = IB_SPEED_SDR;
			*width = IB_WIDTH_1X;
			break;
		case SPEED_10000:
			*speed = IB_SPEED_QDR;
			*width = IB_WIDTH_1X;
			break;
		case SPEED_20000:
			*speed = IB_SPEED_DDR;
			*width = IB_WIDTH_4X;
			break;
		case SPEED_25000:
			*speed = IB_SPEED_EDR;
			*width = IB_WIDTH_1X;
			break;
		case SPEED_40000:
			*speed = IB_SPEED_QDR;
			*width = IB_WIDTH_4X;
			break;
		case SPEED_50000:
			*speed = IB_SPEED_EDR;
			*width = IB_WIDTH_2X;
			break;
		case SPEED_100000:
			*speed = IB_SPEED_EDR;
			*width = IB_WIDTH_4X;
			break;
		case SPEED_200000:
			*speed = IB_SPEED_HDR;
			*width = IB_WIDTH_4X;
			break;
		case SPEED_400000:
			*speed = IB_SPEED_NDR;
			*width = IB_WIDTH_4X;
			break;
		default:
			*speed = IB_SPEED_SDR;
			*width = IB_WIDTH_1X;
			break;
		}
		return;
	}

	switch (lanes) {
	case 1:
		*width = IB_WIDTH_1X;
		break;
	case 2:
		*width = IB_WIDTH_2X;
		break;
	case 4:
		*width = IB_WIDTH_4X;
		break;
	case 8:
		*width = IB_WIDTH_8X;
		break;
	case 12:
		*width = IB_WIDTH_12X;
		break;
	default:
		*width = IB_WIDTH_1X;
	}

	switch (netdev_speed / lanes) {
	case SPEED_2500:
		*speed = IB_SPEED_SDR;
		break;
	case SPEED_5000:
		*speed = IB_SPEED_DDR;
		break;
	case SPEED_10000:
		*speed = IB_SPEED_FDR10;
		break;
	case SPEED_14000:
		*speed = IB_SPEED_FDR;
		break;
	case SPEED_25000:
		*speed = IB_SPEED_EDR;
		break;
	case SPEED_50000:
		*speed = IB_SPEED_HDR;
		break;
	case SPEED_100000:
		*speed = IB_SPEED_NDR;
		break;
	default:
		*speed = IB_SPEED_SDR;
	}
}

/* */
#ifndef HAVE_RDMA_READ_GID_L2_FIELDS
int rdma_read_gid_l2_fields(const struct ib_gid_attr *attr,
			    u16 *vlan_id, u8 *smac)
{
	struct net_device *ndev;

	rcu_read_lock();
	ndev = rcu_dereference(attr->ndev);
	if (!ndev) {
		rcu_read_unlock();
		return -ENODEV;
	}
	if (smac)
		ether_addr_copy(smac, ndev->dev_addr);
	if (vlan_id) {
		*vlan_id = 0xffff;
		if (is_vlan_dev(ndev))
			*vlan_id = vlan_dev_vlan_id(ndev);
	}
	rcu_read_unlock();
	return 0;
}
#endif
