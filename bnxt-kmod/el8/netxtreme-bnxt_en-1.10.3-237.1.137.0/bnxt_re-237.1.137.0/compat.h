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
 * Description: Compat file for compilation
 */

#ifndef __BNXT_RE_COMPAT_H__
#define __BNXT_RE_COMPAT_H__

#include <linux/interrupt.h>
#include <linux/configfs.h>
#include <linux/pci.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_mad.h>
#include <rdma/ib_cache.h>

#ifdef HAVE_NET_BONDING_H
#include <net/bonding.h>
#endif /* HAVE_NET_BONDING_H */

#ifdef IB_PEER_MEM_MOD_SUPPORT
#include "peer_mem.h"
#include "peer_umem.h"
#endif

#include "roce_hsi.h"

/* To avoid compilation failures. bnxt_re_dev is defined in bnxt_re.h */
struct bnxt_re_dev;

/* Defined in include/linux/kconfig.h */
#ifndef IS_ENABLED
#define IS_ENABLED(option)	defined(option)
#endif

#if !defined(CONFIG_DCB)
#warning "Data Center Bridging support (CONFIG_DCB) is not enabled in Linux"
#undef CONFIG_BNXT_DCB
#endif

/* include/rdma/ib_verbs.h */
#ifndef HAVE_IB_MR_INIT_ATTR
struct ib_mr_init_attr {
	int		max_reg_descriptors;
	u32		flags;
};
#endif

#ifndef HAVE_IB_MW_TYPE
enum ib_mw_type {
	IB_MW_TYPE_1 = 1,
	IB_MW_TYPE_2 = 2
};

#endif

#ifdef NO_IB_DEVICE
/* Temp workaround to bypass the ib_core vermagic mismatch */
#define ib_register_device(a, b)	0
#define ib_unregister_device(a)
#define ib_alloc_device(a)		kzalloc(a, GFP_KERNEL)
#define ib_dealloc_device(a)		kfree(a)
#endif

#ifndef HAVE_IB_MEM_WINDOW_TYPE
#define IB_DEVICE_MEM_WINDOW_TYPE_2A	(1 << 23)
#define IB_DEVICE_MEM_WINDOW_TYPE_2B	(1 << 24)
#endif

#ifndef HAVE_IP_BASED_GIDS
#define IB_PORT_IP_BASED_GIDS		(1 << 26)
#endif

#ifndef IB_MTU_8192
#define IB_MTU_8192 8192
#endif

#ifndef SPEED_20000
#define SPEED_20000		20000
#endif

#ifndef SPEED_25000
#define SPEED_25000		25000
#endif

#ifndef SPEED_40000
#define SPEED_40000		40000
#endif

#ifndef SPEED_50000
#define SPEED_50000		50000
#endif

#ifndef SPEED_100000
#define SPEED_100000		100000
#endif

#ifndef SPEED_200000
#define SPEED_200000		200000
#endif

#ifndef SPEED_400000
#define SPEED_400000		400000
#endif

#ifndef IB_SPEED_HDR
#define IB_SPEED_HDR		64
#endif

#ifndef IB_SPEED_NDR
#define IB_SPEED_NDR		128
#endif

#ifndef HAVE_GID_TYPE_ROCE_UDP_ENCAP_ROCEV2
#define RDMA_NETWORK_ROCE_V1	0
#define RDMA_NETWORK_IPV4	1
#define RDMA_NETWORK_IPV6	2
#endif

#ifndef HAVE_RDMA_ADDR_FIND_L2_ETH_BY_GRH
#define rdma_addr_find_l2_eth_by_grh(sgid, dgid, dmac, vlan_id, if_index, hoplimit )\
	rdma_addr_find_dmac_by_grh(sgid, dgid, dmac, vlan_id, if_index)
#endif

#ifndef ETHTOOL_GEEE
struct ethtool_eee {
        __u32   cmd;
        __u32   supported;
        __u32   advertised;
        __u32   lp_advertised;
        __u32   eee_active;
        __u32   eee_enabled;
        __u32   tx_lpi_enabled;
        __u32   tx_lpi_timer;
        __u32   reserved[2];
};
#endif

#ifndef HAVE_ETHTOOL_KEEE
#define ethtool_keee ethtool_eee
#endif

#ifndef _NET_NETMEM_H
typedef struct page *netmem_ref;
#endif

#if !defined(NETDEV_RX_FLOW_STEER) || !defined(HAVE_FLOW_KEYS) || (LINUX_VERSION_CODE < 0x030300)
#undef CONFIG_RFS_ACCEL
#endif

#ifndef HAVE_IB_GID_ATTR
#define ib_query_gid(device, port_num, index, gid, attr)       \
	ib_query_gid(device, port_num, index, gid)
#endif

#ifndef HAVE_RDMA_ADDR_FIND_DMAC_BY_GRH_V2
#define rdma_addr_find_dmac_by_grh(sgid, dgid, smac, vlan, if_index)   \
	rdma_addr_find_dmac_by_grh(sgid, dgid, smac, vlan)
#endif

#ifndef smp_mb__before_atomic
#define smp_mb__before_atomic() smp_mb()
#endif

#ifndef HAVE_IB_WIDTH_2X
#define IB_WIDTH_2X 16
#endif

struct ib_mw_bind_info *get_bind_info(struct ib_send_wr *wr);
struct ib_mw *get_ib_mw(struct ib_send_wr *wr);

struct scatterlist *get_ib_umem_sgl(struct ib_umem *umem, u32 *nmap);

int bnxt_re_register_netdevice_notifier(struct notifier_block *nb);
int bnxt_re_unregister_netdevice_notifier(struct notifier_block *nb);
struct bnxt_qplib_swqe;
void bnxt_re_set_fence_flag(struct ib_send_wr *wr, struct bnxt_qplib_swqe *wqe);

#ifndef HAVE_ETHER_ADDR_COPY
static inline void ether_addr_copy(u8 *dst, const u8 *src)
{
	memcpy(dst, src, ETH_ALEN);
}
#endif

#ifdef HAVE_ROCE_AH_ATTR
#define ROCE_DMAC(x) (x)->roce.dmac
#else
#define ROCE_DMAC(x) (x)->dmac
#endif

#ifdef HAVE_OLD_CONFIGFS_API

struct configfs_attr {
	struct configfs_attribute	attr;
	ssize_t			(*show)(struct config_item *item,
					char *buf);
	ssize_t			(*store)(struct config_item *item,
					 const char *buf, size_t count);
};

#define CONFIGFS_ATTR(_pfx, _name)					\
static struct configfs_attr attr_##_name =			\
	__CONFIGFS_ATTR(_name, S_IRUGO | S_IWUSR, _name##_show, _name##_store)

#define CONFIGFS_ATTR_ADD(_name) &_name.attr
#else
#define CONFIGFS_ATTR_ADD(_name) &_name
#endif /*HAVE_OLD_CONFIGFS_API*/

#ifndef dma_rmb
#define dma_rmb()       rmb()
#endif

#ifndef writel_relaxed
#define writel_relaxed(v, a)	writel(v, a)
#endif

#ifndef writeq_relaxed
#define writeq_relaxed(v, a)	writeq(v, a)
#endif

#ifdef CONFIG_INFINIBAND_PEER_MEM
void bnxt_re_set_inflight_invalidation_ctx(struct ib_umem *umem);
void *bnxt_re_get_peer_mem(struct ib_umem *umem);
#endif /* CONFIG_INFINIBAND_PEER_MEM */

void bnxt_re_peer_mem_release(struct ib_umem *umem);

#ifndef U16_MAX
#define U16_MAX         ((u16)~0U)
#endif

#ifndef BIT_ULL
#define BIT_ULL(nr)             (1ULL << (nr))
#endif

#ifndef HAVE_GID_ATTR_IN_IB_AH
typedef struct ib_gid_attr IB_GID_ATTR;
#else
typedef const struct ib_gid_attr IB_GID_ATTR;
#endif

int __bnxt_re_set_vma_data(void *bnxt_re_uctx,
			   struct vm_area_struct *vma);

int remap_pfn_compat(struct ib_ucontext *ib_uctx,
		     struct vm_area_struct *vma,
		     u64 pfn);

#if defined(RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP) || defined(ENABLE_ROCEV2_QP1)
int bnxt_re_get_cached_gid(struct ib_device *dev, u8 port_num, int index,
			   union ib_gid *sgid, struct ib_gid_attr **sgid_attr,
			   struct ib_global_route *grh, struct ib_ah *ah);
enum rdma_network_type bnxt_re_gid_to_network_type(IB_GID_ATTR *sgid_attr,
						   union ib_gid *sgid);
#endif

int ib_register_device_compat(struct bnxt_re_dev *rdev);
bool ib_modify_qp_is_ok_compat(enum ib_qp_state cur_state, enum ib_qp_state next_state,
			       enum ib_qp_type type, enum ib_qp_attr_mask mask);

#ifndef HAVE_DMA_ZALLOC_COHERENT
static inline void *dma_zalloc_coherent(struct device *dev, size_t size,
					dma_addr_t *dma_handle, gfp_t flag)
{
	void *ret = dma_alloc_coherent(dev, size, dma_handle,
				       flag | __GFP_ZERO);
	return ret;
}
#endif

#ifndef	PCI_EXP_DEVCAP2_ATOMIC_ROUTE
#define	PCI_EXP_DEVCAP2_ATOMIC_ROUTE	0x00000040 /* Atomic Op routing */
#endif

#ifndef PCI_EXP_DEVCTL2_ATOMIC_EGRESS_BLOCK
#define	PCI_EXP_DEVCTL2_ATOMIC_EGRESS_BLOCK 0x0080 /* Block atomic egress */
#endif

#ifndef PCI_EXP_DEVCTL2_ATOMIC_REQ
#define PCI_EXP_DEVCTL2_ATOMIC_REQ	0x0040	/* Set Atomic requests */
#endif

#ifndef HAS_ENABLE_ATOMIC_OPS
#define	PCI_EXP_DEVCAP2_ATOMIC_COMP32	0x00000080 /* 32b AtomicOp completion */
#define	PCI_EXP_DEVCAP2_ATOMIC_COMP64	0x00000100 /* 64b AtomicOp completion */

int pci_enable_atomic_ops_to_root(struct pci_dev *dev, u32 cap_mask);
#endif

#ifdef HAVE_MEMBER_IN_IB_ALLOC_DEVICE
#define compat_ib_alloc_device(size) ib_alloc_device(bnxt_re_dev, ibdev);
#else
#define compat_ib_alloc_device(size) ib_alloc_device(size);
#endif

struct ib_umem *ib_umem_get_compat(struct bnxt_re_dev *rdev,
				   struct ib_ucontext *ucontext,
				   struct ib_udata *udata,
				   unsigned long addr,
				   size_t size, int access, int dmasync);
struct ib_umem *ib_umem_get_flags_compat(struct bnxt_re_dev *rdev,
					 struct ib_ucontext *ucontext,
					 struct ib_udata *udata,
					 unsigned long addr,
					 size_t size, int access, int dmasync);
void bnxt_re_umem_free(struct ib_umem **umem);

#ifndef HAVE_AH_ALLOC_IN_IB_CORE
typedef struct ib_ah* CREATE_AH_RET;
typedef struct ib_pd  CREATE_AH_IN;
#else
typedef int CREATE_AH_RET;
typedef struct ib_ah  CREATE_AH_IN;
#endif

#ifndef HAVE_DESTROY_AH_RET_VOID
typedef int	DESTROY_AH_RET;
#else
typedef void	DESTROY_AH_RET;
#endif

#ifndef HAVE_PD_ALLOC_IN_IB_CORE
typedef struct ib_pd* ALLOC_PD_RET;
typedef struct ib_device ALLOC_PD_IN;
#else
typedef int ALLOC_PD_RET;
typedef struct ib_pd ALLOC_PD_IN;
#endif

#ifndef HAVE_DEALLOC_PD_RET_VOID
typedef int DEALLOC_PD_RET;
#else
typedef void DEALLOC_PD_RET;
#endif

#ifndef HAVE_CQ_ALLOC_IN_IB_CORE
typedef struct ib_cq* ALLOC_CQ_RET;
typedef struct ib_device ALLOC_CQ_IN;
#else
typedef int ALLOC_CQ_RET;
typedef struct ib_cq ALLOC_CQ_IN;
#endif

#ifndef HAVE_DESTROY_CQ_RET_VOID
typedef int DESTROY_CQ_RET;
#else
typedef void DESTROY_CQ_RET;
#endif

#ifndef HAVE_QP_ALLOC_IN_IB_CORE
typedef struct ib_qp *ALLOC_QP_RET;
typedef struct ib_pd ALLOC_QP_IN;
#else
typedef int ALLOC_QP_RET;
typedef struct ib_qp ALLOC_QP_IN;
#endif

#ifndef HAVE_SRQ_CREATE_IN_IB_CORE
typedef struct ib_srq* CREATE_SRQ_RET;
typedef struct ib_pd	CREATE_SRQ_IN;
#else
typedef int CREATE_SRQ_RET;
typedef struct ib_srq CREATE_SRQ_IN;
#endif

#ifndef HAVE_DESTROY_SRQ_RET_VOID
typedef int DESTROY_SRQ_RET;
#else
typedef void DESTROY_SRQ_RET;
#endif

#ifdef HAVE_AUDEV_REM_RET_INT
typedef int AUDEV_REM_RET;
#else
typedef void AUDEV_REM_RET;
#endif

#ifdef HAVE_ALLOC_MW_RET_INT
typedef int ALLOC_MW_RET;
#else
typedef struct ib_mw *ALLOC_MW_RET;
#endif

#ifdef HAVE_REREG_USER_MR_RET_PTR
typedef struct ib_mr *REREG_USER_MR_RET;
#else
typedef int REREG_USER_MR_RET;
#endif

#ifdef HAVE_IB_SUPPORT_MORE_RDMA_PORTS
typedef u32 PORT_NUM;
#else
typedef u8 PORT_NUM;
#endif

#ifndef HAVE_UCONTEXT_ALLOC_IN_IB_CORE
typedef struct ib_ucontext* ALLOC_UCONTEXT_RET;
typedef struct ib_device ALLOC_UCONTEXT_IN;
typedef int DEALLOC_UCONTEXT_RET;
#else
typedef int ALLOC_UCONTEXT_RET;
typedef struct ib_ucontext ALLOC_UCONTEXT_IN;
typedef void DEALLOC_UCONTEXT_RET;
#endif

static inline size_t ib_umem_num_pages_compat(struct ib_umem *umem)
{
#ifdef HAVE_IB_UMEM_NUM_DMA_BLOCKS
	return ib_umem_num_dma_blocks(umem, PAGE_SIZE);
#endif
#ifdef HAVE_IB_UMEM_NUM_PAGES
	return ib_umem_num_pages(umem);
#else
#ifdef HAVE_NPAGES_IB_UMEM
	return umem->npages;
#else
#ifdef HAVE_IB_UMEM_PAGE_COUNT
	return ib_umem_page_count(umem);
#endif
#endif
#endif
}
#ifndef PCI_DEVID
#define PCI_DEVID(bus, devfn)   ((((u16)(bus)) << 8) | (devfn))
#endif /* PCI_DEVID */

#ifdef HAVE_CQ_ALLOC_IN_IB_CORE
#define rdev_from_cq_in(cq_in) to_bnxt_re_dev(cq_in->device, ibdev)
#else
#define rdev_from_cq_in(cq_in) to_bnxt_re_dev(cq_in, ibdev)
#endif

struct bnxt_re_cq *__get_cq_from_cq_in(ALLOC_CQ_IN *cq_in,
				       struct bnxt_re_dev *rdev);
struct bnxt_re_qp *__get_qp_from_qp_in(ALLOC_QP_IN *qp_in,
				       struct bnxt_re_dev *rdev);
bool bnxt_re_check_if_vlan_valid(struct bnxt_re_dev *rdev, u16 vlan_id);

#ifndef IEEE_8021QAZ_APP_SEL_DSCP
#define IEEE_8021QAZ_APP_SEL_DSCP       5
#endif

#ifndef HAVE_PHYS_PORT_STATE_ENUM
enum ib_port_phys_state {
	IB_PORT_PHYS_STATE_SLEEP = 1,
	IB_PORT_PHYS_STATE_POLLING = 2,
	IB_PORT_PHYS_STATE_DISABLED = 3,
	IB_PORT_PHYS_STATE_PORT_CONFIGURATION_TRAINING = 4,
	IB_PORT_PHYS_STATE_LINK_UP = 5,
	IB_PORT_PHYS_STATE_LINK_ERROR_RECOVERY = 6,
	IB_PORT_PHYS_STATE_PHY_TEST = 7,
};
#endif

#ifndef CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_256MB
#define CMDQ_REGISTER_MR_LOG2_PBL_PG_SIZE_PG_256MB 0x1cUL
#endif

#ifdef HAVE_IB_GET_DEV_FW_STR
#ifndef IB_GET_DEV_FW_STR_HAS_STRLEN
#define bnxt_re_query_fw_str(ibdev, str, str_len)	\
	bnxt_re_query_fw_str(ibdev, str)
#endif
#endif

#ifdef HAS_TASKLET_SETUP
typedef void (*tasklet_cb)(struct tasklet_struct *t);
#else
typedef	void (*tasklet_cb)(unsigned long data);
#endif

static inline void compat_tasklet_init(struct tasklet_struct *t,
				       tasklet_cb cb,
				       unsigned long cb_data)
{
#ifndef HAS_TASKLET_SETUP
	tasklet_init(t, cb, cb_data);
#else
	tasklet_setup(t, cb);
#endif
}

#ifndef fallthrough
#if defined __has_attribute
#ifndef __GCC4_has_attribute___fallthrough__
#define __GCC4_has_attribute___fallthrough__ 0
#endif
#if __has_attribute(__fallthrough__)
#define fallthrough     __attribute__((__fallthrough__))
#else
#define fallthrough do {} while (0)
#endif
#else
#define fallthrough do {} while (0)
#endif
#endif

#ifndef HAVE_PCI_NUM_VF
static inline int pci_num_vf(struct pci_dev *dev)
{
	if (!dev->is_physfn)
		return 0;

	return dev->sriov->nr_virtfn;
}
#endif

#ifndef __struct_group
#define __struct_group(TAG, NAME, ATTRS, MEMBERS...) \
	union { \
		struct { MEMBERS } ATTRS; \
		struct TAG { MEMBERS } ATTRS NAME; \
	}
#endif /* __struct_group */
#ifndef struct_group
#define struct_group(NAME, MEMBERS...)	\
	__struct_group(/* no tag */, NAME, /* no attrs */, MEMBERS)
#endif /* struct_group */
#ifndef struct_group_attr
#define struct_group_attr(NAME, ATTRS, MEMBERS...) \
	__struct_group(/* no tag */, NAME, ATTRS, MEMBERS)
#endif /* struct_group_attr */

#ifndef HAVE_IB_POLL_UNBOUND_WORKQUEUE
#define IB_POLL_UNBOUND_WORKQUEUE       IB_POLL_WORKQUEUE
#endif

#ifndef HAVE_VMALLOC_ARRAY
static inline void *vmalloc_array(u32 n, size_t size)
{
	return vmalloc(n * size);
}
#endif

#ifndef HAVE_ADDRCONF_ADDR_EUI48
static inline void addrconf_addr_eui48(u8 *eui, const char *const addr)
{
	memcpy(eui, addr, 3);
	eui[3] = 0xFF;
	eui[4] = 0xFE;
	memcpy(eui + 5, addr + 3, 3);
	eui[0] ^= 2;
}
#endif

#ifndef __counted_by
#define __counted_by(member)
#endif

#if defined(HAVE_EXTERNAL_OFED) && defined(OFED_5_x)
#undef HAVE_IB_UMEM_DMABUF_PINNED
#undef HAVE_IB_UMEM_DMABUF
#if !defined(HAVE_MMU_INTERVAL_NOTIFIER)
#undef HAVE_IB_DEVICE_IN_IB_UMEM_GET
#endif
#endif

#if defined(HAVE_EXTERNAL_OFED) && !defined(HAS_SG_APPEND_TABLE)
#undef HAVE_IB_UMEM_SG_APPEND_TABLE
#endif

#if !defined(HAVE_EXTERNAL_OFED) && !defined(CONFIG_AUXILIARY_BUS)
#undef HAVE_AUXILIARY_DRIVER
#endif

#if defined(HAVE_EXTERNAL_OFED)
#ifdef HAVE_NETDEV_BONDING_INFO
#undef NETDEV_BONDING_INFO
#endif
#endif

#if defined(HAVE_EXTERNAL_OFED)
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(5, 5, 0)))
#undef HAVE_IB_UMEM_DMABUF_PINNED
#undef HAVE_IB_UMEM_DMABUF
#if !defined(HAVE_MMU_INTERVAL_NOTIFIER)
#undef HAVE_IB_DEVICE_IN_IB_UMEM_GET
#endif
#endif
#endif

#ifndef HAVE_RDMA_XARRAY_MMAP_V1
struct rdma_user_mmap_entry {
	/* Below is an unused entry to avoid any compilation
	 * optimization defect.
	 * This is just to get clean compilation for older distros.
	 * older distro will never allocate memory for this.
	 */
	__u64	unused;
};
#endif

#ifndef HAVE_RDMA_AH_GET_SL
static inline u8 rdma_ah_get_sl(const struct rdma_ah_attr *attr)
{
	return attr->sl;
}

static inline const struct ib_global_route
	*rdma_ah_read_grh(const struct rdma_ah_attr *attr)
{
	return &attr->grh;
}
#endif

#ifndef HAVE_RDMA_READ_GID_L2_FIELDS
int rdma_read_gid_l2_fields(const struct ib_gid_attr *attr,
			    u16 *vlan_id, u8 *smac);
#endif

#ifndef HAVE_DEV_IN_IB_CHECK_MR_ACCESS
#define ib_check_mr_access(dev, flags)	ib_check_mr_access(flags)
#endif
#endif /* __BNXT_RE_COMPAT_H__ */
