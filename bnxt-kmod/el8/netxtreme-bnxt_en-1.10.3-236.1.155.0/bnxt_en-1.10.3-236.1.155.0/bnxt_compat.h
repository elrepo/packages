/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2014-2016 Broadcom Corporation
 * Copyright (c) 2016-2018 Broadcom Limited
 * Copyright (c) 2018-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */
#ifndef _BNXT_COMPAT_H_
#define _BNXT_COMPAT_H_

#include <linux/pci.h>
#include <linux/version.h>
#include <linux/ethtool.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/sched.h>
#include <linux/dma-buf.h>
#include <linux/if_vlan.h>
#ifdef HAVE_IEEE1588_SUPPORT
#include <linux/ptp_classify.h>
#include <linux/ptp_clock_kernel.h>
#endif
#if defined(HAVE_TC_FLOW_CLS_OFFLOAD) || defined(HAVE_TC_CLS_FLOWER_OFFLOAD)
#include <net/pkt_cls.h>
#endif
#ifdef HAVE_DIM
#include <linux/dim.h>
#endif
#ifdef HAVE_DEVLINK
#include <net/devlink.h>
#endif
#if defined(HAVE_SWITCHDEV)
#include <net/switchdev.h>
#endif
#ifdef HAVE_NDO_XDP
#include <linux/bpf.h>
#endif
#include <linux/filter.h>
#ifdef HAVE_XDP_RXQ_INFO
#include <net/xdp.h>
#endif
#ifdef HAVE_XSK_SUPPORT
#include <net/xdp_sock_drv.h>
#endif
#ifdef HAVE_NETDEV_QMGMT_OPS
#include <net/netdev_queues.h>
#endif

#ifndef IS_ENABLED
#define __ARG_PLACEHOLDER_1	0,
#define config_enabled(cfg)	_config_enabled(cfg)
#define _config_enabled(value)	__config_enabled(__ARG_PLACEHOLDER_##value)
#define __config_enabled(arg1_or_junk)	___config_enabled(arg1_or_junk 1, 0)
#define ___config_enabled(__ignored, val, ...)	val
#define IS_ENABLED(option)	\
	(config_enabled(option) || config_enabled(option##_MODULE))
#endif

#if !IS_ENABLED(CONFIG_NET_DEVLINK)
#undef HAVE_DEVLINK
#endif

#ifndef HAVE_DEVLINK
#undef HAVE_DEVLINK_INFO
#undef HAVE_DEVLINK_PARAM
#undef HAVE_NDO_DEVLINK_PORT
#undef HAVE_DEVLINK_FLASH_UPDATE
#undef HAVE_DEVLINK_HEALTH_REPORT
#undef HAVE_DEVLINK_RELOAD_ACTION
#endif

/* Reconcile all dependencies for VF reps:
 * SRIOV, Devlink, Switchdev and HW port info in metadata_dst
 */
#if defined(CONFIG_BNXT_SRIOV) && defined(HAVE_DEVLINK) && \
	defined(CONFIG_NET_SWITCHDEV) && defined(HAVE_METADATA_HW_PORT_MUX) && \
	(LINUX_VERSION_CODE >= 0x030a00)
#define CONFIG_VF_REPS		1
#endif
/* DEVLINK code has dependencies on VF reps */
#ifdef HAVE_DEVLINK_PARAM
#define CONFIG_VF_REPS		1
#endif
#ifdef CONFIG_VF_REPS
#ifndef SWITCHDEV_SET_OPS
#define SWITCHDEV_SET_OPS(netdev, ops) ((netdev)->switchdev_ops = (ops))
#endif
#endif

/* Reconcile all dependencies for TC Flower offload
 * Need the following to be defined to build TC flower offload
 * HAVE_TC_FLOW_CLS_OFFLOAD OR HAVE_TC_CLS_FLOWER_OFFLOAD
 * HAVE_RHASHTABLE
 * HAVE_FLOW_DISSECTOR_KEY_ICMP
 * HAVE_FLOW_DISSECTOR_KEY_ENC_IP
 * CONFIG_NET_SWITCHDEV
 * HAVE_TCF_EXTS_TO_LIST (its possible to do without this but
 * the code gets a bit complicated. So, for now depend on this.)
 * HAVE_TCF_TUNNEL
 * Instead of checking for all of the above defines, enable one
 * define when all are enabled.
 */
#if (defined(HAVE_TC_FLOW_CLS_OFFLOAD) || \
	defined(HAVE_TC_CLS_FLOWER_OFFLOAD)) && \
	(defined(HAVE_TCF_EXTS_TO_LIST) || \
	defined(HAVE_TC_EXTS_FOR_ACTION)) && \
	defined(HAVE_RHASHTABLE) && defined(HAVE_FLOW_DISSECTOR_KEY_ICMP) && \
	defined(HAVE_FLOW_DISSECTOR_KEY_ENC_IP) && \
	defined(HAVE_TCF_TUNNEL) && \
	(defined(CONFIG_NET_SWITCHDEV) || defined(BNXT_ENABLE_TRUFLOW)) && \
	(LINUX_VERSION_CODE >= 0x030a00)
#define	CONFIG_BNXT_FLOWER_OFFLOAD	1
#ifndef HAVE_NDO_GET_PORT_PARENT_ID
#define netdev_port_same_parent_id(a, b) switchdev_port_same_parent_id(a, b)
#endif
#else
#undef CONFIG_BNXT_FLOWER_OFFLOAD
#endif

/* With upstream kernels >= v5.2.0, struct tc_cls_flower_offload has been
 * replaced by struct flow_cls_offload. For older kernels(< v5.2.0), rename
 * the respective definitions here.
 */
#ifndef HAVE_TC_FLOW_CLS_OFFLOAD
#ifdef HAVE_TC_CLS_FLOWER_OFFLOAD
#define flow_cls_offload		tc_cls_flower_offload
#define flow_cls_offload_flow_rule	tc_cls_flower_offload_flow_rule
#define FLOW_CLS_REPLACE		TC_CLSFLOWER_REPLACE
#define FLOW_CLS_DESTROY		TC_CLSFLOWER_DESTROY
#define FLOW_CLS_STATS			TC_CLSFLOWER_STATS
#endif
#endif

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)
#ifndef NUM_FLOW_ACTIONS
#define	NUM_FLOW_ACTIONS	64
#endif
#endif

#if defined(CONFIG_HWMON) || defined(CONFIG_HWMON_MODULE)
#if defined(HAVE_NEW_HWMON_API)
#define CONFIG_BNXT_HWMON	1
#endif
#endif

#ifndef SPEED_2500
#define SPEED_2500		2500
#endif

#ifndef SPEED_5000
#define SPEED_5000		5000
#endif

#ifndef SPEED_14000
#define SPEED_14000		14000
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

#ifndef SPEED_56000
#define SPEED_56000		56000
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

#ifndef SPEED_UNKNOWN
#define SPEED_UNKNOWN		-1
#endif

#ifndef DUPLEX_UNKNOWN
#define DUPLEX_UNKNOWN		0xff
#endif

#ifndef PORT_DA
#define PORT_DA			0x05
#endif

#ifndef PORT_NONE
#define PORT_NONE		0xef
#endif

#if !defined(SUPPORTED_40000baseCR4_Full)
#define SUPPORTED_40000baseCR4_Full	(1 << 24)

#define ADVERTISED_40000baseCR4_Full	(1 << 24)
#endif

#if !defined(ETHTOOL_FEC_LLRS)
#define ETHTOOL_FEC_LLRS		(1 << 5)
#else
#define HAVE_ETHTOOL_FEC_LLRS
#endif

#if !defined(HAVE_ETH_TEST_FL_EXTERNAL_LB)
#define ETH_TEST_FL_EXTERNAL_LB		0
#define ETH_TEST_FL_EXTERNAL_LB_DONE	0
#endif

#if !defined(IPV4_FLOW)
#define IPV4_FLOW	0x10
#endif

#if !defined(IPV6_FLOW)
#define IPV6_FLOW	0x11
#endif

#if defined(HAVE_ETH_GET_HEADLEN) || (LINUX_VERSION_CODE > 0x040900)
#define BNXT_RX_PAGE_MODE_SUPPORT	1
#endif

#if !defined(ETH_P_8021AD)
#define ETH_P_8021AD		0x88A8
#endif

#if !defined(ETH_P_ROCE)
#define ETH_P_ROCE		0x8915
#endif

#if !defined(ROCE_V2_UDP_PORT)
#define ROCE_V2_UDP_DPORT	4791
#endif

#ifndef NETIF_F_GSO_UDP_TUNNEL
#define NETIF_F_GSO_UDP_TUNNEL	0
#endif

#ifndef NETIF_F_GSO_UDP_TUNNEL_CSUM
#define NETIF_F_GSO_UDP_TUNNEL_CSUM	0
#endif

#ifndef NETIF_F_GSO_GRE
#define NETIF_F_GSO_GRE		0
#endif

#ifndef NETIF_F_GSO_GRE_CSUM
#define NETIF_F_GSO_GRE_CSUM	0
#endif

#ifndef NETIF_F_GSO_IPIP
#define NETIF_F_GSO_IPIP	0
#endif

#ifndef NETIF_F_GSO_SIT
#define NETIF_F_GSO_SIT		0
#endif

#ifndef NETIF_F_GSO_IPXIP4
#define NETIF_F_GSO_IPXIP4	(NETIF_F_GSO_IPIP | NETIF_F_GSO_SIT)
#endif

#ifndef NETIF_F_GSO_PARTIAL
#define NETIF_F_GSO_PARTIAL	0
#else
#define HAVE_GSO_PARTIAL_FEATURES	1
#endif

#ifndef NETIF_F_GSO_UDP_L4
#define NETIF_F_GSO_UDP_L4	0
#define SKB_GSO_UDP_L4		0
#endif

/* Tie rx checksum offload to tx checksum offload for older kernels. */
#ifndef NETIF_F_RXCSUM
#define NETIF_F_RXCSUM		NETIF_F_IP_CSUM
#endif

#ifndef NETIF_F_CSUM_MASK
#define NETIF_F_CSUM_MASK	(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM | \
				 NETIF_F_HW_CSUM)
#endif

#ifndef NETIF_F_NTUPLE
#define NETIF_F_NTUPLE		0
#endif

#ifndef NETIF_F_RXHASH
#define NETIF_F_RXHASH		0
#else
#define HAVE_NETIF_F_RXHASH
#endif

#ifdef NETIF_F_GRO_HW
#define HAVE_NETIF_F_GRO_HW	1
#else
#define NETIF_F_GRO_HW		0
#endif

#ifndef NETIF_F_HW_TLS_RX
#define NETIF_F_HW_TLS_RX	0
#endif

#ifndef NETIF_F_HW_TLS_TX
#define NETIF_F_HW_TLS_TX	0
#endif

#ifndef HAVE_SKB_GSO_UDP_TUNNEL_CSUM
#ifndef HAVE_SKB_GSO_UDP_TUNNEL
#define SKB_GSO_UDP_TUNNEL 0
#endif
#define SKB_GSO_UDP_TUNNEL_CSUM SKB_GSO_UDP_TUNNEL
#endif

#ifndef BRIDGE_MODE_VEB
#define BRIDGE_MODE_VEB		0
#endif

#ifndef BRIDGE_MODE_VEPA
#define BRIDGE_MODE_VEPA	1
#endif

#ifndef BRIDGE_MODE_UNDEF
#define BRIDGE_MODE_UNDEF	0xffff
#endif

#ifndef DEFINE_DMA_UNMAP_ADDR
#define DEFINE_DMA_UNMAP_ADDR(mapping) DECLARE_PCI_UNMAP_ADDR(mapping)
#endif

#ifndef DEFINE_DMA_UNMAP_LEN
#define DEFINE_DMA_UNMAP_LEN(len) DECLARE_PCI_UNMAP_LEN(len)
#endif

#ifndef dma_unmap_addr_set
#define dma_unmap_addr_set pci_unmap_addr_set
#endif

#ifndef dma_unmap_addr
#define dma_unmap_addr pci_unmap_addr
#endif

#ifndef dma_unmap_len
#define dma_unmap_len pci_unmap_len
#endif

#ifdef HAVE_DMA_ATTRS_H
#define dma_map_single_attrs(dev, cpu_addr, size, dir, attrs) \
	dma_map_single_attrs(dev, cpu_addr, size, dir, NULL)

#define dma_unmap_single_attrs(dev, dma_addr, size, dir, attrs) \
	dma_unmap_single_attrs(dev, dma_addr, size, dir, NULL)

#ifdef HAVE_DMA_MAP_PAGE_ATTRS
#define dma_map_page_attrs(dev, page, offset, size, dir, attrs) \
	dma_map_page_attrs(dev, page, offset, size, dir, NULL)

#define dma_unmap_page_attrs(dev, dma_addr, size, dir, attrs) \
	dma_unmap_page_attrs(dev, dma_addr, size, dir, NULL)
#endif
#endif

#ifndef HAVE_DMA_MAP_PAGE_ATTRS
#define dma_map_page_attrs(dev, page, offset, size, dir, attrs) \
	dma_map_page(dev, page, offset, size, dir)

#define dma_unmap_page_attrs(dev, dma_addr, size, dir, attrs) \
	dma_unmap_page(dev, dma_addr, size, dir)
#endif

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a, b) 0
#endif

#if defined(RHEL_RELEASE_CODE) && (RHEL_RELEASE_CODE == RHEL_RELEASE_VERSION(6,3))
#if defined(CONFIG_X86_64) && !defined(CONFIG_NEED_DMA_MAP_STATE)
#undef DEFINE_DMA_UNMAP_ADDR
#define DEFINE_DMA_UNMAP_ADDR(ADDR_NAME)        dma_addr_t ADDR_NAME
#undef DEFINE_DMA_UNMAP_LEN
#define DEFINE_DMA_UNMAP_LEN(LEN_NAME)          __u32 LEN_NAME
#undef dma_unmap_addr
#define dma_unmap_addr(PTR, ADDR_NAME)           ((PTR)->ADDR_NAME)
#undef dma_unmap_addr_set
#define dma_unmap_addr_set(PTR, ADDR_NAME, VAL)  (((PTR)->ADDR_NAME) = (VAL))
#undef dma_unmap_len
#define dma_unmap_len(PTR, LEN_NAME)             ((PTR)->LEN_NAME)
#undef dma_unmap_len_set
#define dma_unmap_len_set(PTR, LEN_NAME, VAL)    (((PTR)->LEN_NAME) = (VAL))
#endif
#endif

#ifdef HAVE_NDO_SET_VF_VLAN_RH73
#define ndo_set_vf_vlan ndo_set_vf_vlan_rh73
#endif

#ifdef HAVE_NDO_CHANGE_MTU_RH74
#define ndo_change_mtu ndo_change_mtu_rh74
#undef HAVE_MIN_MTU
#endif

#ifdef HAVE_NDO_SETUP_TC_RH72
#define ndo_setup_tc ndo_setup_tc_rh72
#endif

#ifdef HAVE_NDO_SET_VF_TRUST_RH
#define ndo_set_vf_trust extended.ndo_set_vf_trust
#endif

#ifdef HAVE_NDO_UDP_TUNNEL_RH
#define ndo_udp_tunnel_add extended.ndo_udp_tunnel_add
#define ndo_udp_tunnel_del extended.ndo_udp_tunnel_del
#endif

#ifndef HAVE_TC_SETUP_QDISC_MQPRIO
#define TC_SETUP_QDISC_MQPRIO	TC_SETUP_MQPRIO
#endif

#if defined(HAVE_NDO_SETUP_TC_RH) || defined(HAVE_EXT_GET_PHYS_PORT_NAME) || defined(HAVE_NDO_SET_VF_TRUST_RH)
#define HAVE_NDO_SIZE	1
#endif

#ifndef FLOW_RSS
#define FLOW_RSS	0x20000000
#endif

#ifndef FLOW_MAC_EXT
#define FLOW_MAC_EXT	0x40000000
#endif

#ifndef ETHTOOL_RX_FLOW_SPEC_RING
#define ETHTOOL_RX_FLOW_SPEC_RING	0x00000000FFFFFFFFLL
#define ETHTOOL_RX_FLOW_SPEC_RING_VF	0x000000FF00000000LL
#define ETHTOOL_RX_FLOW_SPEC_RING_VF_OFF 32
static inline __u64 ethtool_get_flow_spec_ring(__u64 ring_cookie)
{
	return ETHTOOL_RX_FLOW_SPEC_RING & ring_cookie;
};

static inline __u64 ethtool_get_flow_spec_ring_vf(__u64 ring_cookie)
{
	return (ETHTOOL_RX_FLOW_SPEC_RING_VF & ring_cookie) >>
				ETHTOOL_RX_FLOW_SPEC_RING_VF_OFF;
};
#endif

#ifndef ETHTOOL_GEEE
struct ethtool_eee {
	__u32	cmd;
	__u32	supported;
	__u32	advertised;
	__u32	lp_advertised;
	__u32	eee_active;
	__u32	eee_enabled;
	__u32	tx_lpi_enabled;
	__u32	tx_lpi_timer;
	__u32	reserved[2];
};
#endif

#ifndef HAVE_ETHTOOL_KEEE
/* ethtool_keee must be compatible with ethtool_eee.  Do not follow
 * the upstream structure.
 */
struct ethtool_keee {
	__u32	cmd;
	__u32	supported;
	__u32	advertised;
	__u32	lp_advertised;
	__u32	eee_active;
	__u32	eee_enabled;
	__u32	tx_lpi_enabled;
	__u32	tx_lpi_timer;
	__u32	reserved[2];
};

#define _bnxt_fw_to_linkmode(mode, fw_speeds)	\
	mode = _bnxt_fw_to_ethtool_adv_spds(fw_speeds, 0)

#endif

#ifndef HAVE_ETHTOOL_RESET_CRASHDUMP
enum compat_ethtool_reset_flags { ETH_RESET_CRASHDUMP = 1 << 9 };
#endif

#ifndef HAVE_CQE_ETHTOOL_COALESCE
#define bnxt_get_coalesce(dev, coal, kernel_coal, extack)	\
	bnxt_get_coalesce(dev, coal)

#define bnxt_set_coalesce(dev, coal, kernel_coal, extack)	\
	bnxt_set_coalesce(dev, coal)

#define ETHTOOL_COALESCE_USE_CQE	0
#endif

#ifndef HAVE_ETHTOOL_GET_RING_EXT
#define bnxt_get_ringparam(dev, ering, kernel_ering, extack)	\
	bnxt_get_ringparam(dev, ering)

#define bnxt_set_ringparam(dev, ering, kernel_ering, extack)	\
	bnxt_set_ringparam(dev, ering)
#endif

#if defined(HAVE_DIM) && !defined(HAVE_DIM_SAMPLE_PTR)
#define net_dim(dim, end_sample)	net_dim(dim, *(end_sample))
#endif

#ifndef HAVE_KERNEL_ETHTOOL_TS_INFO
#define kernel_ethtool_ts_info ethtool_ts_info
#endif

#ifndef HAVE_SKB_FRAG_PAGE
static inline struct page *skb_frag_page(const skb_frag_t *frag)
{
	return frag->page;
}

static inline void *skb_frag_address_safe(const skb_frag_t *frag)
{
	void *ptr = page_address(skb_frag_page(frag));
	if (unlikely(!ptr))
		return NULL;

	return ptr + frag->page_offset;
}

static inline void __skb_frag_set_page(skb_frag_t *frag, struct page *page)
{
	frag->page = page;
}

#define skb_frag_dma_map(x, frag, y, len, z) \
	pci_map_page(bp->pdev, (frag)->page, \
		     (frag)->page_offset, (len), PCI_DMA_TODEVICE)
#endif /* HAVE_SKB_FRAG_PAGE */

#ifndef HAVE_SKB_FRAG_FILL_PAGE_DESC
#ifdef SKB_FRAG_USES_BIO
static inline void skb_frag_fill_page_desc(skb_frag_t *frag,
					   struct page *page,
					   int off, int size)
{
	frag->bv_page = page;
	frag->bv_offset = off;
	skb_frag_size_set(frag, size);
}

#else

static inline void skb_frag_fill_page_desc(skb_frag_t *frag,
					   struct page *page,
					   int off, int size)
{
	frag->page_offset = off;
	skb_frag_size_set(frag, size);
	__skb_frag_set_page(frag, page);
}
#endif
#endif /* HAVE_SKB_FRAG_FILL_PAGE_DESC */

#ifndef HAVE_SKB_FRAG_ACCESSORS
static inline void skb_frag_off_add(skb_frag_t *frag, int delta)
{
	frag->page_offset += delta;
}
#endif

#ifndef HAVE_SKB_FREE_FRAG
static inline void skb_free_frag(void *addr)
{
	put_page(virt_to_head_page(addr));
}
#endif

#ifndef HAVE_PCI_VFS_ASSIGNED
static inline int pci_vfs_assigned(struct pci_dev *dev)
{
	return 0;
}
#endif

#ifndef HAVE_PCI_NUM_VF
#include <../drivers/pci/pci.h>

static inline int pci_num_vf(struct pci_dev *dev)
{
	if (!dev->is_physfn)
		return 0;

	return dev->sriov->nr_virtfn;
}
#endif

#ifndef SKB_ALLOC_NAPI
static inline struct sk_buff *napi_alloc_skb(struct napi_struct *napi,
					     unsigned int length)
{
	struct sk_buff *skb;

	length += NET_SKB_PAD + NET_IP_ALIGN;
	skb = netdev_alloc_skb(napi->dev, length);

	if (likely(skb))
		skb_reserve(skb, NET_SKB_PAD + NET_IP_ALIGN);

	return skb;
}
#endif

#ifndef HAVE_SKB_HASH_TYPE

enum pkt_hash_types {
	PKT_HASH_TYPE_NONE,	/* Undefined type */
	PKT_HASH_TYPE_L2,	/* Input: src_MAC, dest_MAC */
	PKT_HASH_TYPE_L3,	/* Input: src_IP, dst_IP */
	PKT_HASH_TYPE_L4,	/* Input: src_IP, dst_IP, src_port, dst_port */
};

static inline void
skb_set_hash(struct sk_buff *skb, __u32 hash, enum pkt_hash_types type)
{
#ifdef HAVE_NETIF_F_RXHASH
	skb->rxhash = hash;
#endif
}

#endif

#define GET_NET_STATS(x) (unsigned long)le64_to_cpu(x)

#if !defined(NETDEV_RX_FLOW_STEER) || (LINUX_VERSION_CODE < 0x030300) || \
		defined(NO_NETDEV_CPU_RMAP)
#undef CONFIG_RFS_ACCEL
#endif

#if !defined(IEEE_8021QAZ_APP_SEL_DGRAM) || !defined(CONFIG_DCB) || !defined(HAVE_IEEE_DELAPP)
#undef CONFIG_BNXT_DCB
#endif

#ifdef CONFIG_BNXT_DCB
#ifndef IEEE_8021QAZ_APP_SEL_DSCP
#define IEEE_8021QAZ_APP_SEL_DSCP	5
#endif
#endif

#ifndef NETDEV_HW_FEATURES
#define hw_features features
#endif

#ifndef HAVE_NETDEV_FEATURES_T
#ifdef HAVE_NDO_FIX_FEATURES
typedef u32 netdev_features_t;
#else
typedef unsigned long netdev_features_t;
#endif
#endif

#ifndef HAVE_NEW_BUILD_SKB
#define build_skb(data, frag) build_skb(data)
#endif

#ifndef HAVE_NAPI_ALLOC_FRAG
#define napi_alloc_frag(fragsz) netdev_alloc_frag(fragsz)
#endif

#ifndef HAVE_NAPI_BUILD_SKB
#define napi_build_skb(data, frag_size) build_skb(data, frag_size)
#endif

#ifndef __rcu
#define __rcu
#endif

#ifndef rcu_dereference_protected
#define rcu_dereference_protected(p, c)	\
	rcu_dereference((p))
#endif

#ifndef rcu_access_pointer
#define rcu_access_pointer rcu_dereference
#endif

#ifndef rtnl_dereference
#define rtnl_dereference(p)		\
	rcu_dereference_protected(p, lockdep_rtnl_is_held())
#endif

#ifndef RCU_INIT_POINTER
#define RCU_INIT_POINTER(p, v)	\
	p = (typeof(*v) __force __rcu *)(v)
#endif

#ifdef HAVE_OLD_HLIST
#define __hlist_for_each_entry_rcu(f, n, h, m) \
	hlist_for_each_entry_rcu(f, n, h, m)
#define __hlist_for_each_entry_safe(f, n, t, h, m) \
	hlist_for_each_entry_safe(f, n, t, h, m)
#else
#define __hlist_for_each_entry_rcu(f, n, h, m) \
	hlist_for_each_entry_rcu(f, h, m)
#define __hlist_for_each_entry_safe(f, n, t, h, m) \
	hlist_for_each_entry_safe(f, t, h, m)
#endif

#ifndef VLAN_PRIO_SHIFT
#define VLAN_PRIO_SHIFT		13
#endif

#ifndef IEEE_8021Q_MAX_PRIORITIES
#define IEEE_8021Q_MAX_PRIORITIES	8
#endif

#ifndef NETIF_F_HW_VLAN_CTAG_TX
#define NETIF_F_HW_VLAN_CTAG_TX NETIF_F_HW_VLAN_TX
#define NETIF_F_HW_VLAN_CTAG_RX NETIF_F_HW_VLAN_RX
/* 802.1AD not supported on older kernels */
#define NETIF_F_HW_VLAN_STAG_TX 0
#define NETIF_F_HW_VLAN_STAG_RX 0

#define __vlan_hwaccel_put_tag(skb, proto, tag) \
do {						\
	if (proto == ntohs(ETH_P_8021Q))	\
		__vlan_hwaccel_put_tag(skb, tag);\
} while (0)

#define vlan_proto protocol

#if defined(HAVE_VLAN_RX_REGISTER)
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define OLD_VLAN	1
#define OLD_VLAN_VALID	(1 << 31)
#endif
#endif

#endif

#ifndef HAVE_ETH_TYPE_VLAN

static inline bool eth_type_vlan(__be16 ethertype)
{
	switch (ethertype) {
	case htons(ETH_P_8021Q):
	case htons(ETH_P_8021AD):
		return true;
	default:
		return false;
	}
}
#endif

#ifndef HAVE_NETDEV_NOTIFIER_INFO_TO_DEV
#ifndef netdev_notifier_info_to_dev
static inline struct net_device *
netdev_notifier_info_to_dev(void *ptr)
{
	return (struct net_device *)ptr;
}
#endif
#endif

static inline int bnxt_en_register_netdevice_notifier(struct notifier_block *nb)
{
	int rc;
#ifdef HAVE_REGISTER_NETDEVICE_NOTIFIER_RH
	rc = register_netdevice_notifier_rh(nb);
#else
	rc = register_netdevice_notifier(nb);
#endif
	return rc;
}

static inline int bnxt_en_unregister_netdevice_notifier(struct notifier_block *nb)
{
	int rc;
#ifdef HAVE_REGISTER_NETDEVICE_NOTIFIER_RH
	rc = unregister_netdevice_notifier_rh(nb);
#else
	rc = unregister_netdevice_notifier(nb);
#endif
	return rc;
}

#ifndef HAVE_NETDEV_UPDATE_FEATURES
static inline void netdev_update_features(struct net_device *dev)
{
	/* Do nothing, since we can't set default VLAN on these old kernels. */
}
#endif

#if !defined(netdev_printk) && (LINUX_VERSION_CODE < 0x020624)

#ifndef HAVE_NETDEV_NAME
static inline const char *netdev_name(const struct net_device *dev)
{
	if (dev->reg_state != NETREG_REGISTERED)
		return "(unregistered net_device)";
	return dev->name;
}
#endif

#define NET_PARENT_DEV(netdev)  ((netdev)->dev.parent)

#define netdev_printk(level, netdev, format, args...)		\
	dev_printk(level, NET_PARENT_DEV(netdev),		\
		   "%s: " format,				\
		   netdev_name(netdev), ##args)

#endif

#ifndef netdev_err
#define netdev_err(dev, format, args...)			\
	netdev_printk(KERN_ERR, dev, format, ##args)
#endif

#ifndef netdev_info
#define netdev_info(dev, format, args...)			\
	netdev_printk(KERN_INFO, dev, format, ##args)
#endif

#ifndef netdev_warn
#define netdev_warn(dev, format, args...)			\
	netdev_printk(KERN_WARNING, dev, format, ##args)
#endif

#ifndef dev_warn_ratelimited
#define dev_warn_ratelimited(dev, format, args...)		\
	dev_warn(dev, format, ##args)
#endif

#ifndef netdev_level_once
#define netdev_level_once(level, dev, fmt, ...)			\
do {								\
	static bool __print_once __read_mostly;			\
								\
	if (!__print_once) {					\
		__print_once = true;				\
		netdev_printk(level, dev, fmt, ##__VA_ARGS__);	\
	}							\
} while (0)

#define netdev_info_once(dev, fmt, ...) \
	netdev_level_once(KERN_INFO, dev, fmt, ##__VA_ARGS__)
#define netdev_warn_once(dev, fmt, ...) \
	netdev_level_once(KERN_WARNING, dev, fmt, ##__VA_ARGS__)
#endif

#ifndef netif_level
#define netif_level(level, priv, type, dev, fmt, args...)	\
do {								\
	if (netif_msg_##type(priv))				\
		netdev_##level(dev, fmt, ##args);		\
} while (0)
#endif

#ifndef netif_warn
#define netif_warn(priv, type, dev, fmt, args...)		\
	netif_level(warn, priv, type, dev, fmt, ##args)
#endif

#ifndef netif_notice
#define netif_notice(priv, type, dev, fmt, args...)		\
	netif_level(notice, priv, type, dev, fmt, ##args)
#endif

#ifndef netif_info
#define netif_info(priv, type, dev, fmt, args...)		\
	netif_level(info, priv, type, dev, fmt, ##args)
#endif

#ifndef pci_warn
#define pci_warn(pdev, fmt, arg...)	dev_warn(&(pdev)->dev, fmt, ##arg)
#endif

#ifndef netdev_uc_count
#define netdev_uc_count(dev)	((dev)->uc.count)
#endif

#ifndef netdev_for_each_uc_addr
#define netdev_for_each_uc_addr(ha, dev) \
	list_for_each_entry(ha, &dev->uc.list, list)
#endif

#ifndef netdev_for_each_mc_addr
#define netdev_for_each_mc_addr(mclist, dev) \
	for (mclist = dev->mc_list; mclist; mclist = mclist->next)
#endif

#ifndef smp_mb__before_atomic
#define smp_mb__before_atomic()	smp_mb()
#endif

#ifndef smp_mb__after_atomic
#define smp_mb__after_atomic()	smp_mb()
#endif

#ifndef dma_rmb
#define dma_rmb() rmb()
#endif

#ifndef writel_relaxed
#define writel_relaxed(v, a)	writel(v, a)
#endif

#ifndef writeq_relaxed
#define writeq_relaxed(v, a)	writeq(v, a)
#endif

#ifndef HAVE_LO_HI_WRITEQ
static inline void lo_hi_writeq(u64 val, volatile void __iomem *addr)
{
	writel(val, addr);
	writel(val >> 32, addr + 4);
}

static inline void lo_hi_writeq_relaxed(u64 val, volatile void __iomem *addr)
{
	writel_relaxed(val, addr);
	writel_relaxed(val >> 32, addr + 4);
}
#endif

#ifndef WRITE_ONCE
#define WRITE_ONCE(x, val)	(x = val)
#endif

#ifndef READ_ONCE
#define READ_ONCE(val)		(val)
#endif

#ifdef CONFIG_NET_RX_BUSY_POLL
#include <net/busy_poll.h>
#if defined(HAVE_NAPI_HASH_ADD) && defined(NETDEV_BUSY_POLL)
#define BNXT_PRIV_RX_BUSY_POLL	1
#endif
#endif

#if defined(HAVE_NETPOLL_POLL_DEV)
#undef CONFIG_NET_POLL_CONTROLLER
#endif

#if !defined(CONFIG_PTP_1588_CLOCK) && !defined(CONFIG_PTP_1588_CLOCK_MODULE)
#undef HAVE_IEEE1588_SUPPORT
#endif

#ifdef HAVE_IEEE1588_SUPPORT

#if !defined(HAVE_PTP_HEADER)
struct clock_identity {
	u8 id[8];
} __packed;

struct port_identity {
	struct clock_identity	clock_identity;
	__be16			port_number;
} __packed;

struct ptp_header {
	u8			tsmt;  /* transportSpecific | messageType */
	u8			ver;   /* reserved          | versionPTP  */
	__be16			message_length;
	u8			domain_number;
	u8			reserved1;
	u8			flag_field[2];
	__be64			correction;
	__be32			reserved2;
	struct port_identity	source_port_identity;
	__be16			sequence_id;
	u8			control;
	u8			log_message_interval;
} __packed;
#endif

#if !defined(HAVE_PTP_CLASSES)
#define PTP_CLASS_V2    0x02 /* protocol version 2 */
#define PTP_CLASS_IPV4  0x10 /* event in an IPV4 UDP packet */
#define PTP_CLASS_IPV6  0x20 /* event in an IPV6 UDP packet */
#define PTP_CLASS_L2    0x30 /* event in a L2 packet */
#define PTP_CLASS_VLAN  0x40 /* event in a VLAN tagged L2 packet */
#define PTP_CLASS_PMASK 0xf0 /* mask for the packet type field */
#define OFF_IHL		14
#define IPV4_HLEN(data) (((struct iphdr *)((data) + OFF_IHL))->ihl << 2)
#define IP6_HLEN	40
#define UDP_HLEN	8
#endif

#if !defined(HAVE_PTP_CLASSIFY_RAW)
static inline unsigned int ptp_classify_raw(const struct sk_buff *skb)
{
	u32 ptp_class = PTP_CLASS_V2;

	if (skb_vlan_tag_present(skb))
		ptp_class |= PTP_CLASS_VLAN;
	if (skb->protocol == htons(ETH_P_IP))
		ptp_class |= PTP_CLASS_IPV4;
	if (skb->protocol == htons(ETH_P_IPV6))
		ptp_class |= PTP_CLASS_IPV6;
	if (skb->protocol == htons(ETH_P_1588))
		ptp_class |= PTP_CLASS_L2;

	return ptp_class;
}
#endif

#if !defined(HAVE_PTP_PARSE_HEADER)
static inline struct ptp_header *ptp_parse_header(struct sk_buff *skb,
						  unsigned int type)
{
	u8 *ptr = skb_mac_header(skb);

	if (type & PTP_CLASS_VLAN)
		ptr += VLAN_HLEN;

	switch (type & PTP_CLASS_PMASK) {
	case PTP_CLASS_IPV4:
		ptr += IPV4_HLEN(ptr) + UDP_HLEN;
		break;
	case PTP_CLASS_IPV6:
		ptr += IP6_HLEN + UDP_HLEN;
		break;
	case PTP_CLASS_L2:
		break;
	default:
		return NULL;
	}

	ptr += ETH_HLEN;

	/* Ensure that the entire header is present in this packet. */
	if (ptr + sizeof(struct ptp_header) > skb->data + skb->len)
		return NULL;

	return (struct ptp_header *)ptr;
}
#endif
#endif	/* HAVE_IEEE1588_SUPPORT */

#if !defined(HAVE_PTP_GETTIMEX64)
#if !defined(HAVE_TIMESPEC64)
struct timespec64 {
	__signed__ long	tv_sec;
	long		tv_nsec;
};
#endif

#ifndef HAVE_PTP_SYS_TIMESTAMP
struct ptp_system_timestamp {
	struct timespec64 pre_ts;
	struct timespec64 post_ts;
};
#endif

static inline void ptp_read_system_prets(struct ptp_system_timestamp *sts)
{
}

static inline void ptp_read_system_postts(struct ptp_system_timestamp *sts)
{
}
#endif

#if !defined(HAVE_PTP_DO_AUX_WORK) || !defined(HAVE_PTP_CANCEL_WORKER_SYNC)
struct tx_ts_cmp;
#include "bnxt_ptp.h"

static inline void bnxt_ptp_cancel_worker_sync(struct bnxt_ptp_cfg *ptp)
{
#if defined(HAVE_PTP_DO_AUX_WORK)
	/* We don't have access to inetrnal kernel struct ptp_clock */
	ptp->shutdown = 1;
	msleep(50);
#else
	cancel_work_sync(&ptp->ptp_ts_task);
#endif
}

static inline void bnxt_ptp_schedule_worker(struct bnxt_ptp_cfg *ptp)
{
#if defined(HAVE_PTP_DO_AUX_WORK)
	ptp->shutdown = 0;
	ptp_schedule_worker(ptp->ptp_clock, 0);
#else
	schedule_work(&ptp->ptp_ts_task);
#endif
}
#endif

#ifndef HAVE_TIMER_DELETE_SYNC
#define timer_delete_sync(t)	del_timer_sync(t)
#endif

#ifndef timer_container_of
#define timer_container_of(v, c, f)	from_timer(v, c, f)
#endif

#if defined(RHEL_RELEASE_CODE) && (RHEL_RELEASE_CODE == RHEL_RELEASE_VERSION(7,0))
#undef CONFIG_CRASH_DUMP
#endif

#ifndef skb_vlan_tag_present
#define skb_vlan_tag_present(skb) vlan_tx_tag_present(skb)
#define skb_vlan_tag_get(skb) vlan_tx_tag_get(skb)
#endif

#if !defined(HAVE_NAPI_HASH_DEL)
static inline void napi_hash_del(struct napi_struct *napi)
{
}
#endif

#if !defined(LL_FLUSH_FAILED) || !defined(HAVE_NAPI_HASH_ADD)
static inline void napi_hash_add(struct napi_struct *napi)
{
}
#endif

#ifndef HAVE_SET_COHERENT_MASK
static inline int dma_set_coherent_mask(struct device *dev, u64 mask)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);

	return pci_set_consistent_dma_mask(pdev, mask);
}
#endif

#ifndef HAVE_SET_MASK_AND_COHERENT
static inline int dma_set_mask_and_coherent(struct device *dev, u64 mask)
{
	int rc = dma_set_mask(dev, mask);
	if (rc == 0)
		dma_set_coherent_mask(dev, mask);
	return rc;
}
#endif

#ifndef HAVE_DMA_ZALLOC_COHERENT
static inline void *dma_zalloc_coherent(struct device *dev, size_t size,
					dma_addr_t *dma_handle, gfp_t flag)
{
	void *ret = dma_alloc_coherent(dev, size, dma_handle,
				       flag | __GFP_ZERO);
	return ret;
}
#endif

#ifndef HAVE_IFLA_TX_RATE
#define ndo_set_vf_rate ndo_set_vf_tx_rate
#endif

#ifndef HAVE_NDO_ETH_IOCTL
#define ndo_eth_ioctl ndo_do_ioctl
#endif

#ifndef HAVE_PRANDOM_BYTES
#define prandom_bytes get_random_bytes
#endif

#ifndef rounddown
#define rounddown(x, y) (				\
{							\
	typeof(x) __x = (x);				\
	__x - (__x % (y));				\
}							\
)
#endif

#ifdef NO_SKB_FRAG_SIZE
static inline unsigned int skb_frag_size(const skb_frag_t *frag)
{
	return frag->size;
}
#endif

#ifdef NO_ETH_RESET_AP
#define ETH_RESET_AP (1<<8)
#endif

#ifndef HAVE_SKB_CHECKSUM_NONE_ASSERT
static inline void skb_checksum_none_assert(struct sk_buff *skb)
{
	skb->ip_summed = CHECKSUM_NONE;
}
#endif

#ifndef HAVE_ETHER_ADDR_EQUAL
static inline bool ether_addr_equal(const u8 *addr1, const u8 *addr2)
{
	return !compare_ether_addr(addr1, addr2);
}
#endif

#ifndef HAVE_ETHER_ADDR_COPY
static inline void ether_addr_copy(u8 *dst, const u8 *src)
{
	memcpy(dst, src, ETH_ALEN);
}
#endif

#ifndef HAVE_ETH_BROADCAST_ADDR
static inline void eth_broadcast_addr(u8 *addr)
{
	memset(addr, 0xff, ETH_ALEN);
}
#endif

#ifndef HAVE_ETH_HW_ADDR_RANDOM
static inline void eth_hw_addr_random(struct net_device *dev)
{
#if defined(NET_ADDR_RANDOM)
	dev->addr_assign_type = NET_ADDR_RANDOM;
#endif
	random_ether_addr(dev->dev_addr);
}
#endif

#ifndef HAVE_NETDEV_TX_QUEUE_CTRL
static inline void netdev_tx_sent_queue(struct netdev_queue *dev_queue,
				unsigned int bytes)
{
}

static inline void netdev_tx_completed_queue(struct netdev_queue *dev_queue,
				unsigned int pkts, unsigned int bytes)
{
}

static inline void netdev_tx_reset_queue(struct netdev_queue *q)
{
}
#endif

#ifndef HAVE_NETIF_SET_REAL_NUM_RX
static inline int netif_set_real_num_rx_queues(struct net_device *dev,
				unsigned int rxq)
{
	return 0;
}
#endif

#ifndef HAVE_NETIF_SET_REAL_NUM_TX
static inline void netif_set_real_num_tx_queues(struct net_device *dev,
						unsigned int txq)
{
	dev->real_num_tx_queues = txq;
}
#endif

#ifndef TSO_MAX_SEGS
static inline void netif_set_tso_max_segs(struct net_device *dev,
					  unsigned int segs)
{
	dev->gso_max_segs = segs;
}
#endif

#ifndef HAVE_NETIF_GET_DEFAULT_RSS
static inline int netif_get_num_default_rss_queues(void)
{
	return min_t(int, 8, num_online_cpus());
}
#endif

#ifndef HAVE_NETIF_IS_RXFH_CONFIGURED
#define IFF_RXFH_CONFIGURED	0
#undef HAVE_SET_RXFH
static inline bool netif_is_rxfh_configured(const struct net_device *dev)
{
	return false;
}
#endif

#if defined(HAVE_NETDEV_TX_DROPPED)
#if !defined(HAVE_NETDEV_TX_DROPPED_CORE_STATS)
#if defined(HAVE_NETDEV_RH_TX_DROPPED)
#define dev_core_stats_tx_dropped_inc(dev) atomic_long_inc(&(dev)->rh_tx_dropped)
#else
#define dev_core_stats_tx_dropped_inc(dev) atomic_long_inc(&(dev)->tx_dropped)
#endif
#endif
#else
#define dev_core_stats_tx_dropped_inc(dev)
#endif

#if !defined(HAVE_TCP_V6_CHECK)
static __inline__ __sum16 tcp_v6_check(int len,
				const struct in6_addr *saddr,
				const struct in6_addr *daddr,
				__wsum base)
{
	return csum_ipv6_magic(saddr, daddr, len, IPPROTO_TCP, base);
}
#endif

#if !defined(HAVE_SKB_TCP_ALL_HEADERS)
#include <linux/tcp.h>

static inline int skb_tcp_all_headers(const struct sk_buff *skb)
{
	return skb_transport_offset(skb) + tcp_hdrlen(skb);
}

static inline int skb_inner_tcp_all_headers(const struct sk_buff *skb)
{
	return skb_inner_network_offset(skb) + skb_inner_network_header_len(skb) +
	       inner_tcp_hdrlen(skb);
}
#endif

#ifndef ipv6_authlen
#define ipv6_authlen(p) (((p)->hdrlen+2) << 2)
#endif

#ifdef HAVE_NDO_FEATURES_CHECK
#if defined(HAVE_INNER_NETWORK_OFFSET) && !defined(HAVE_INNER_ETH_HDR)
static inline struct ethhdr *inner_eth_hdr(const struct sk_buff *skb)
{
	return (struct ethhdr *)(skb->head + skb->inner_mac_header);
}
#endif
#endif

#ifndef HAVE_USLEEP_RANGE
static inline void usleep_range(unsigned long min, unsigned long max)
{
	if (min < 1000)
		udelay(min);
	else
		msleep(min / 1000);
}
#endif

#ifndef HAVE_GET_NUM_TC
static inline int netdev_get_num_tc(struct net_device *dev)
{
	return 0;
}

static inline void netdev_reset_tc(struct net_device *dev)
{
}

static inline int netdev_set_tc_queue(struct net_device *devi, u8 tc,
				      u16 count, u16 offset)
{
	return 0;
}
#endif

#ifndef HAVE_VZALLOC
static inline void *vzalloc(size_t size)
{
	void *ret = vmalloc(size);
	if (ret)
		memset(ret, 0, size);
	return ret;
}
#endif

#ifndef HAVE_KMALLOC_ARRAY
static inline void *kmalloc_array(unsigned n, size_t s, gfp_t gfp)
{
	return kmalloc(n * s, gfp);
}
#endif

#ifndef ETH_MODULE_SFF_8436
#define ETH_MODULE_SFF_8436             0x4
#endif

#ifndef ETH_MODULE_SFF_8436_LEN
#define ETH_MODULE_SFF_8436_LEN         256
#endif

#ifndef ETH_MODULE_SFF_8636
#define ETH_MODULE_SFF_8636             0x3
#endif

#ifndef ETH_MODULE_SFF_8636_LEN
#define ETH_MODULE_SFF_8636_LEN         256
#endif

#ifndef PCI_IRQ_MSIX
#ifndef HAVE_MSIX_RANGE
static inline int
pci_enable_msix_range(struct pci_dev *dev, struct msix_entry *entries,
		      int minvec, int maxvec)
{
	int rc = -ERANGE;

	while (maxvec >= minvec) {
		rc = pci_enable_msix(dev, entries, maxvec);
		if (!rc)
			return maxvec;
		if (rc < 0)
			return rc;
		if (rc < minvec)
			return -ENOSPC;
		maxvec = rc;
	}

	return rc;
}
#endif /* HAVE_MSIX_RANGE */
#endif /* PCI_IRQ_MSIX */

#ifndef HAVE_MSIX_DYN

static inline bool pci_msix_can_alloc_dyn(struct pci_dev *dev)
{
	return false;
}

struct msi_map {
	int	index;
	int	virq;
};

static inline struct msi_map
pci_msix_alloc_irq_at(struct pci_dev *pdev, unsigned int index,
		      const void *affdesc)
{
	struct msi_map map = { .index = -ENOSYS, };

	return map;
}

static inline void pci_msix_free_irq(struct pci_dev *pdev, struct msi_map map)
{
}

#endif /* HAVE_MSIX_DYN */

#ifndef HAVE_PCI_PHYSFN
static inline struct pci_dev *pci_physfn(struct pci_dev *dev)
{
#ifdef CONFIG_PCI_IOV
	if (dev->is_virtfn)
		dev = dev->physfn;
#endif

	return dev;
}
#endif

#ifndef HAVE_PCI_PRINT_LINK_STATUS
#ifndef HAVE_PCI_LINK_WIDTH
enum pcie_link_width {
	PCIE_LNK_WIDTH_UNKNOWN		= 0xFF,
};
#endif

#ifndef HAVE_PCIE_BUS_SPEED
enum pci_bus_speed {
	PCIE_SPEED_2_5GT		= 0x14,
	PCIE_SPEED_5_0GT		= 0x15,
	PCIE_SPEED_8_0GT		= 0x16,
#ifndef PCIE_SPEED_16_0GT
	PCIE_SPEED_16_0GT		= 0x17,
#endif
	PCI_SPEED_UNKNOWN		= 0xFF,
};
#endif

#ifndef PCIE_SPEED_16_0GT
#define PCIE_SPEED_16_0GT	0x17
#endif

static const unsigned char pcie_link_speed[] = {
	PCI_SPEED_UNKNOWN,		/* 0 */
	PCIE_SPEED_2_5GT,		/* 1 */
	PCIE_SPEED_5_0GT,		/* 2 */
	PCIE_SPEED_8_0GT,		/* 3 */
	PCIE_SPEED_16_0GT,		/* 4 */
	PCI_SPEED_UNKNOWN,		/* 5 */
	PCI_SPEED_UNKNOWN,		/* 6 */
	PCI_SPEED_UNKNOWN,		/* 7 */
	PCI_SPEED_UNKNOWN,		/* 8 */
	PCI_SPEED_UNKNOWN,		/* 9 */
	PCI_SPEED_UNKNOWN,		/* A */
	PCI_SPEED_UNKNOWN,		/* B */
	PCI_SPEED_UNKNOWN,		/* C */
	PCI_SPEED_UNKNOWN,		/* D */
	PCI_SPEED_UNKNOWN,		/* E */
	PCI_SPEED_UNKNOWN		/* F */
};

#ifndef PCI_EXP_LNKSTA_NLW_SHIFT
#define PCI_EXP_LNKSTA_NLW_SHIFT	4
#endif

#ifndef PCI_EXP_LNKCAP2
#define PCI_EXP_LNKCAP2		44	/* Link Capabilities 2 */
#endif

#ifndef PCI_EXP_LNKCAP2_SLS_2_5GB
#define PCI_EXP_LNKCAP2_SLS_2_5GB	0x00000002 /* Supported Speed 2.5GT/s */
#define PCI_EXP_LNKCAP2_SLS_5_0GB	0x00000004 /* Supported Speed 5GT/s */
#define PCI_EXP_LNKCAP2_SLS_8_0GB	0x00000008 /* Supported Speed 8GT/s */
#endif

#ifndef PCI_EXP_LNKCAP2_SLS_16_0GB
#define PCI_EXP_LNKCAP2_SLS_16_0GB	0x00000010 /* Supported Speed 16GT/s */
#endif

#ifndef PCI_EXP_LNKCAP_SLS_2_5GB
#define PCI_EXP_LNKCAP_SLS_2_5GB	0x00000001 /* LNKCAP2 SLS Vector bit 0*/
#define PCI_EXP_LNKCAP_SLS_5_0GB	0x00000002 /* LNKCAP2 SLS Vector bit 1*/
#endif

#ifndef PCI_EXP_LNKCAP_SLS_8_0GB
#define PCI_EXP_LNKCAP_SLS_8_0GB	0x00000003 /* LNKCAP2 SLS Vector bit2 */
#endif

#ifndef PCI_EXP_LNKCAP_SLS_16_0GB
#define PCI_EXP_LNKCAP_SLS_16_0GB	0x00000004 /* LNKCAP2 SLS Vector bit 3 */
#endif

#ifndef PCIE_SPEED2STR
/* PCIe link information */
#define PCIE_SPEED2STR(speed) \
	((speed) == PCIE_SPEED_16_0GT ? "16 GT/s" : \
	 (speed) == PCIE_SPEED_8_0GT ? "8 GT/s" : \
	 (speed) == PCIE_SPEED_5_0GT ? "5 GT/s" : \
	 (speed) == PCIE_SPEED_2_5GT ? "2.5 GT/s" : \
	 "Unknown speed")

/* PCIe speed to Mb/s reduced by encoding overhead */
#define PCIE_SPEED2MBS_ENC(speed) \
	((speed) == PCIE_SPEED_16_0GT ? 16000 * 128 / 130 : \
	 (speed) == PCIE_SPEED_8_0GT  ?  8000 * 128 / 130 : \
	 (speed) == PCIE_SPEED_5_0GT  ?  5000 * 8 / 10 : \
	 (speed) == PCIE_SPEED_2_5GT  ?  2500 * 8 / 10 : \
	 0)
#endif /* PCIE_SPEED2STR */

#define BNXT_PCIE_CAP		0xAC
#ifndef HAVE_PCI_UPSTREAM_BRIDGE
static inline struct pci_dev *pci_upstream_bridge(struct pci_dev *dev)
{
	dev = pci_physfn(dev);
	if (pci_is_root_bus(dev->bus))
		return NULL;

	return dev->bus->self;
}
#endif

static u32
pcie_bandwidth_available(struct pci_dev *dev, struct pci_dev **limiting_dev,
			 enum pci_bus_speed *speed, enum pcie_link_width *width)
{
	enum pcie_link_width next_width;
	enum pci_bus_speed next_speed;
	u32 bw, next_bw;
	u16 lnksta;

	if (speed)
		*speed = PCI_SPEED_UNKNOWN;
	if (width)
		*width = PCIE_LNK_WIDTH_UNKNOWN;

	bw = 0;
#ifdef HAVE_PCIE_CAPABILITY_READ_WORD
	while (dev) {
		pcie_capability_read_word(dev, PCI_EXP_LNKSTA, &lnksta);

		next_speed = pcie_link_speed[lnksta & PCI_EXP_LNKSTA_CLS];
		next_width = (lnksta & PCI_EXP_LNKSTA_NLW) >>
			PCI_EXP_LNKSTA_NLW_SHIFT;

		next_bw = next_width * PCIE_SPEED2MBS_ENC(next_speed);

		/* Check if current device limits the total bandwidth */
		if (!bw || next_bw <= bw) {
			bw = next_bw;

			if (limiting_dev)
				*limiting_dev = dev;
			if (speed)
				*speed = next_speed;
			if (width)
				*width = next_width;
		}

		dev = pci_upstream_bridge(dev);
	}
#else
	pci_read_config_word(dev, BNXT_PCIE_CAP + PCI_EXP_LNKSTA, &lnksta);
	next_speed = pcie_link_speed[lnksta & PCI_EXP_LNKSTA_CLS];
	next_width = (lnksta & PCI_EXP_LNKSTA_NLW) >> PCI_EXP_LNKSTA_NLW_SHIFT;
	next_bw = next_width * PCIE_SPEED2MBS_ENC(next_speed);

	if (limiting_dev)
		*limiting_dev = dev;
	if (speed)
		*speed = next_speed;
	if (width)
		*width = next_width;
#endif /* HAVE_PCIE_CAPABILITY_READ_WORD */

	return bw;
}

static enum pci_bus_speed pcie_get_speed_cap(struct pci_dev *dev)
{
	/*
	 * Link Capabilities 2 was added in PCIe r3.0, sec 7.8.18.  The
	 * implementation note there recommends using the Supported Link
	 * Speeds Vector in Link Capabilities 2 when supported.
	 *
	 * Without Link Capabilities 2, i.e., prior to PCIe r3.0, software
	 * should use the Supported Link Speeds field in Link Capabilities,
	 * where only 2.5 GT/s and 5.0 GT/s speeds were defined.
	 */
#ifdef HAVE_PCIE_CAPABILITY_READ_WORD
	u32 lnkcap2, lnkcap;

	pcie_capability_read_dword(dev, PCI_EXP_LNKCAP2, &lnkcap2);
#else
	u16 lnkcap2, lnkcap;

	pci_read_config_word(dev, BNXT_PCIE_CAP + PCI_EXP_LNKCAP2, &lnkcap2);
#endif
	if (lnkcap2) { /* PCIe r3.0-compliant */
		if (lnkcap2 & PCI_EXP_LNKCAP2_SLS_16_0GB)
			return PCIE_SPEED_16_0GT;
		else if (lnkcap2 & PCI_EXP_LNKCAP2_SLS_8_0GB)
			return PCIE_SPEED_8_0GT;
		else if (lnkcap2 & PCI_EXP_LNKCAP2_SLS_5_0GB)
			return PCIE_SPEED_5_0GT;
		else if (lnkcap2 & PCI_EXP_LNKCAP2_SLS_2_5GB)
			return PCIE_SPEED_2_5GT;
		return PCI_SPEED_UNKNOWN;
	}

#ifdef HAVE_PCIE_CAPABILITY_READ_WORD
	pcie_capability_read_dword(dev, PCI_EXP_LNKCAP, &lnkcap);
#else
	pci_read_config_word(dev, BNXT_PCIE_CAP + PCI_EXP_LNKCAP, &lnkcap);
#endif
	if ((lnkcap & PCI_EXP_LNKCAP_SLS) == PCI_EXP_LNKCAP_SLS_5_0GB)
		return PCIE_SPEED_5_0GT;
	else if ((lnkcap & PCI_EXP_LNKCAP_SLS) == PCI_EXP_LNKCAP_SLS_2_5GB)
		return PCIE_SPEED_2_5GT;

	return PCI_SPEED_UNKNOWN;
}

static enum pcie_link_width pcie_get_width_cap(struct pci_dev *dev)
{
#ifdef HAVE_PCIE_CAPABILITY_READ_WORD
	u32 lnkcap;

	pcie_capability_read_dword(dev, PCI_EXP_LNKCAP, &lnkcap);
#else
	u16 lnkcap;

	pci_read_config_word(dev, BNXT_PCIE_CAP + PCI_EXP_LNKCAP, &lnkcap);
#endif
	if (lnkcap)
		return (lnkcap & PCI_EXP_LNKCAP_MLW) >> 4;

	return PCIE_LNK_WIDTH_UNKNOWN;
}

static u32
pcie_bandwidth_capable(struct pci_dev *dev, enum pci_bus_speed *speed,
		       enum pcie_link_width *width)
{
	*speed = pcie_get_speed_cap(dev);
	*width = pcie_get_width_cap(dev);

	if (*speed == PCI_SPEED_UNKNOWN || *width == PCIE_LNK_WIDTH_UNKNOWN)
		return 0;

	return *width * PCIE_SPEED2MBS_ENC(*speed);
}

static inline void pcie_print_link_status(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	enum pcie_link_width width, width_cap;
	enum pci_bus_speed speed, speed_cap;
	struct pci_dev *limiting_dev = NULL;
	u32 bw_avail, bw_cap;

	bw_cap = pcie_bandwidth_capable(pdev, &speed_cap, &width_cap);
	bw_avail = pcie_bandwidth_available(pdev, &limiting_dev, &speed,
					    &width);

	if (bw_avail >= bw_cap)
		netdev_info(dev, "%u.%03u Gb/s available PCIe bandwidth (%s x%d link)\n",
			    bw_cap / 1000, bw_cap % 1000,
			    PCIE_SPEED2STR(speed_cap), width_cap);
	else
		netdev_info(dev, "%u.%03u Gb/s available PCIe bandwidth, limited by %s x%d link at %s (capable of %u.%03u Gb/s with %s x%d link)\n",
			    bw_avail / 1000, bw_avail % 1000,
			    PCIE_SPEED2STR(speed), width,
			    limiting_dev ? pci_name(limiting_dev) : "<unknown>",
			    bw_cap / 1000, bw_cap % 1000,
			    PCIE_SPEED2STR(speed_cap), width_cap);
}
#endif /* HAVE_PCI_PRINT_LINK_STATUS */

#ifndef HAVE_PCI_IS_BRIDGE
static inline bool pci_is_bridge(struct pci_dev *dev)
{
	return dev->hdr_type == PCI_HEADER_TYPE_BRIDGE ||
		dev->hdr_type == PCI_HEADER_TYPE_CARDBUS;
}
#endif

#ifndef HAVE_PCI_GET_DSN
static inline u64 pci_get_dsn(struct pci_dev *dev)
{
	u32 dword;
	u64 dsn;
	int pos;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_DSN);
	if (!pos)
		return 0;

	/*
	 * The Device Serial Number is two dwords offset 4 bytes from the
	 * capability position. The specification says that the first dword is
	 * the lower half, and the second dword is the upper half.
	 */
	pos += 4;
	pci_read_config_dword(dev, pos, &dword);
	dsn = (u64)dword;
	pci_read_config_dword(dev, pos + 4, &dword);
	dsn |= ((u64)dword) << 32;

	return dsn;
}
#endif

#ifndef PCI_VPD_RO_KEYWORD_SERIALNO
#define PCI_VPD_RO_KEYWORD_SERIALNO	"SN"
#endif

#ifdef HAVE_OLD_VPD_FIND_TAG
#define pci_vpd_find_tag(buf, len, rdt)	pci_vpd_find_tag(buf, 0, len, rdt)
#endif

#ifndef HAVE_PCI_VPD_ALLOC

#define BNXT_VPD_LEN   512
static inline void *pci_vpd_alloc(struct pci_dev *dev, unsigned int *size)
{
	unsigned int len = BNXT_VPD_LEN;
	void *buf;
	int cnt;

	buf = kmalloc(len, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	cnt = pci_read_vpd(dev, 0, len, buf);
	if (cnt <= 0) {
		kfree(buf);
		return ERR_PTR(-EIO);
	}

	if (size)
		*size = cnt;

	return buf;
}

static inline
int pci_vpd_find_ro_info_keyword(const void *buf, unsigned int len,
				 const char *kw, unsigned int *size)
{
	int ro_start, infokw_start;
	unsigned int ro_len, infokw_size;

	ro_start = pci_vpd_find_tag(buf, len, PCI_VPD_LRDT_RO_DATA);
	if (ro_start < 0)
		return ro_start;

	ro_len = pci_vpd_lrdt_size(buf + ro_start);
	ro_start += PCI_VPD_LRDT_TAG_SIZE;

	if (ro_start + ro_len > len)
		return -EINVAL;

	infokw_start = pci_vpd_find_info_keyword(buf, ro_start, ro_len, kw);
	if (infokw_start < 0)
		return infokw_start;

	infokw_size = pci_vpd_info_field_size(buf + infokw_start);
	infokw_start += PCI_VPD_INFO_FLD_HDR_SIZE;

	if (infokw_start + infokw_size > len)
		return -EINVAL;

	if (size)
		*size = infokw_size;

	return infokw_start;
}
#endif

#ifndef HAVE_NDO_XDP
struct netdev_bpf;
#ifndef HAVE_EXT_NDO_XDP_XMIT
struct xdp_buff {
	void *data;
};
#endif
#elif !defined(HAVE_NDO_BPF)
#define ndo_bpf		ndo_xdp
#define netdev_bpf	netdev_xdp
#endif

#ifndef XDP_PACKET_HEADROOM
#define XDP_PACKET_HEADROOM	0
#endif

#ifndef HAVE_XDP_FRAME
#define xdp_do_flush()
#ifndef HAVE_XDP_REDIRECT
struct bpf_prog;
static inline int xdp_do_redirect(struct net_device *dev, struct xdp_buff *xdp,
				  struct bpf_prog *prog)
{
	return 0;
}
#endif
#else
#ifndef HAVE_XDP_DO_FLUSH
#define xdp_do_flush xdp_do_flush_map
#endif
#endif

#ifndef HAVE_XDP_ACTION
enum xdp_action {
        XDP_ABORTED = 0,
        XDP_DROP,
        XDP_PASS,
        XDP_TX,
#ifndef HAVE_XDP_REDIRECT
        XDP_REDIRECT,
#endif
};
#else
#ifndef HAVE_XDP_REDIRECT
#define XDP_REDIRECT	4
#endif
#endif

#if defined(HAVE_NDO_XDP) && defined(HAVE_LEGACY_RCU_BH)
static inline u32
bnxt_compat_bpf_prog_run_xdp(const struct bpf_prog *prog, struct xdp_buff *xdp)
{
	u32 act;

	rcu_read_lock();
	act = bpf_prog_run_xdp(prog, xdp);
	rcu_read_unlock();
	if (act == XDP_REDIRECT) {
		WARN_ONCE(1, "bnxt_en does not support XDP_REDIRECT on this kernel");
		return XDP_ABORTED;
	}
	return act;
}

#define bpf_prog_run_xdp(prog, xdp) bnxt_compat_bpf_prog_run_xdp(prog, xdp)
#endif

#if defined(HAVE_NDO_XDP)
#ifndef HAVE_BPF_TRACE
#define trace_xdp_exception(dev, xdp_prog, act)
#define bpf_warn_invalid_xdp_action(dev, xdp_prog, act)
#elif !defined(HAVE_BPF_WARN_INVALID_XDP_ACTION_EXT)
#define bpf_warn_invalid_xdp_action(dev, xdp_prog, act)		\
	bpf_warn_invalid_xdp_action(act)
#endif
#endif

#ifdef HAVE_XDP_RXQ_INFO
#ifndef HAVE_XDP_RXQ_INFO_IS_REG

#define REG_STATE_REGISTERED	0x1

static inline bool xdp_rxq_info_is_reg(struct xdp_rxq_info *xdp_rxq)
{
	return (xdp_rxq->reg_state == REG_STATE_REGISTERED);
}
#endif
#ifndef HAVE_NEW_XDP_RXQ_INFO_REG
#define xdp_rxq_info_reg(q, dev, qidx, napi_id) xdp_rxq_info_reg(q, dev, qidx)
#endif
#else
struct xdp_rxq_info {
	struct net_device *dev;
	u32 queue_index;
	u32 reg_state;
};
#endif

#ifndef HAVE_XDP_MEM_TYPE
enum xdp_mem_type {
	MEM_TYPE_PAGE_SHARED = 0, /* Split-page refcnt based model */
	MEM_TYPE_PAGE_ORDER0,     /* Orig XDP full page model */
	MEM_TYPE_PAGE_POOL,
	MEM_TYPE_ZERO_COPY,
	MEM_TYPE_MAX,
};
static inline int xdp_rxq_info_reg_mem_model(struct xdp_rxq_info *xdp_rxq,
			       enum xdp_mem_type type, void *allocator)
{
	return 0;
}
#endif

#if defined(HAVE_NDO_XDP) && !defined(HAVE_XDP_INIT_BUFF)
static __always_inline void
xdp_init_buff(struct xdp_buff *xdp, u32 frame_sz, struct xdp_rxq_info *rxq)
{
#ifdef HAVE_XDP_FRAME_SZ
	xdp->frame_sz = frame_sz;
#endif
#ifdef HAVE_XDP_RXQ_INFO
	xdp->rxq = rxq;
#endif
}

#ifndef HAVE_XDP_RXQ_INFO
#define xdp_init_buff(xdp, frame_sz, rxq) xdp_init_buff(xdp, frame_sz, NULL)
#endif

static __always_inline void
xdp_prepare_buff(struct xdp_buff *xdp, unsigned char *hard_start,
		 int headroom, int data_len, const bool meta_valid)
{
	unsigned char *data = hard_start + headroom;

#if XDP_PACKET_HEADROOM
	xdp->data_hard_start = hard_start;
#endif
	xdp->data = data;
	xdp->data_end = data + data_len;
#ifdef HAVE_XDP_DATA_META
	xdp->data_meta = meta_valid ? data : data + 1;
#endif
}
#endif

#ifndef HAVE_XDP_SHARED_INFO_FROM_BUFF
static inline struct skb_shared_info *
xdp_get_shared_info_from_buff(struct xdp_buff *xdp)
{
	return NULL;
}
#endif

#ifndef HAVE_XDP_MULTI_BUFF
static __always_inline bool xdp_buff_has_frags(struct xdp_buff *xdp)
{
	return false;
}

static __always_inline void xdp_buff_set_frags_flag(struct xdp_buff *xdp)
{
}

static __always_inline void xdp_buff_set_frag_pfmemalloc(struct xdp_buff *xdp)
{
}

static inline void
xdp_update_skb_shared_info(struct sk_buff *skb, u8 nr_frags,
			   unsigned int size, unsigned int truesize,
			   bool pfmemalloc)
{
}
#endif /* HAVE_XDP_MULTI_BUFF */
#if !defined(CONFIG_PAGE_POOL) || !defined(HAVE_PAGE_POOL_RELEASE_PAGE) || defined(HAVE_SKB_MARK_RECYCLE)
#define page_pool_release_page(page_pool, page)
#endif

#ifndef HAVE_TCF_EXTS_HAS_ACTIONS
#define tcf_exts_has_actions(x)			(!tc_no_actions(x))
#endif

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) && !defined(HAVE_FLOW_OFFLOAD_H) && !defined(HAVE_TCF_STATS_UPDATE)
static inline void
tcf_exts_stats_update(const struct tcf_exts *exts,
		      u64 bytes, u64 packets, u64 lastuse)
{
#ifdef CONFIG_NET_CLS_ACT
	int i;

	preempt_disable();

	for (i = 0; i < exts->nr_actions; i++) {
		struct tc_action *a = exts->actions[i];

		tcf_action_stats_update(a, bytes, packets, lastuse);
	}

	preempt_enable();
#endif
}
#endif

#ifndef HAVE_TC_CB_REG_EXTACK
#define tcf_block_cb_register(block, cb, cb_ident, cb_priv, extack)	\
	tcf_block_cb_register(block, cb, cb_ident, cb_priv)
#endif

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
#if !defined(HAVE_TC_CLS_CAN_OFFLOAD_AND_CHAIN0) && defined(HAVE_TC_SETUP_BLOCK)
static inline bool
tc_cls_can_offload_and_chain0(const struct net_device *dev,
			      struct tc_cls_common_offload *common)
{
	if (!tc_can_offload(dev))
		return false;
	if (common->chain_index)
		return false;
	return true;
}
#endif

#ifdef HAVE_TC_CB_EGDEV

static inline void bnxt_reg_egdev(const struct net_device *dev,
				  void *cb, void *cb_priv, int vf_idx)
{
	if (tc_setup_cb_egdev_register(dev, (tc_setup_cb_t *)cb, cb_priv))
		netdev_warn(dev,
			    "Failed to register egdev for VF-Rep: %d", vf_idx);
}

static inline void bnxt_unreg_egdev(const struct net_device *dev,
				    void *cb, void *cb_priv)
{
	tc_setup_cb_egdev_unregister(dev, (tc_setup_cb_t *)cb, cb_priv);
}

#else

static inline void bnxt_reg_egdev(const struct net_device *dev,
				  void *cb, void *cb_priv, int vf_idx)
{
}

static inline void bnxt_unreg_egdev(const struct net_device *dev,
				    void *cb, void *cb_priv)
{
}

#endif /* HAVE_TC_CB_EGDEV */

#ifdef HAVE_TC_SETUP_BLOCK
#ifndef HAVE_SETUP_TC_BLOCK_HELPER

static inline int
flow_block_cb_setup_simple(struct tc_block_offload *f,
			   struct list_head *driver_block_list,
			   tc_setup_cb_t *cb, void *cb_ident, void *cb_priv,
			   bool ingress_only)
{
	if (ingress_only &&
	    f->binder_type != TCF_BLOCK_BINDER_TYPE_CLSACT_INGRESS)
		return -EOPNOTSUPP;

	switch (f->command) {
	case TC_BLOCK_BIND:
		return tcf_block_cb_register(f->block, cb, cb_ident, cb_priv,
					     f->extack);
	case TC_BLOCK_UNBIND:
		tcf_block_cb_unregister(f->block, cb, cb_ident);
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

#endif /* !HAVE_SETUP_TC_BLOCK_HELPER */
#endif /* HAVE_TC_SETUP_BLOCK */

#ifndef HAVE_FLOW_RULE_MATCH_CONTROL_FLAGS

struct flow_rule;
struct netlink_ext_ack;

static inline bool flow_rule_match_has_control_flags(struct flow_rule *rule,
						     struct netlink_ext_ack *extack)
{
	return false;
}

#endif /* HAVE_FLOW_RULE_MATCH_CONTROL_FLAGS */
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */

#ifndef BIT_ULL
#define BIT_ULL(nr)		(1ULL << (nr))
#endif

#ifndef HAVE_SIMPLE_OPEN
static inline int simple_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;
	return 0;
}
#endif

#if !defined(HAVE_DEVLINK_PORT_ATTRS_SET_NEW) && defined(HAVE_DEVLINK_PORT_ATTRS)
#define devlink_port_attrs_set(dl_port, attrs)			\
	devlink_port_attrs_set(dl_port, (*attrs).flavour,		\
			       (*attrs).phys.port_number, false, 0,	\
			       (*attrs).switch_id.id, (*attrs).switch_id.id_len)
#endif /* !HAVE_DEVLINK_PORT_ATTRS_SET_NEW && HAVE_DEVLINK_PORT_ATTRS */

#if !defined(HAVE_DEVLINK_PARAM_PUBLISH) && defined(HAVE_DEVLINK_PARAM)
static inline void devlink_params_publish(struct devlink *devlink)
{
}
#endif

#ifdef HAVE_DEVLINK_HEALTH_REPORT
#ifndef HAVE_DEVLINK_HEALTH_REPORTER_STATE_UPDATE

#define DEVLINK_HEALTH_REPORTER_STATE_HEALTHY	0
#define DEVLINK_HEALTH_REPORTER_STATE_ERROR	1

static inline void
devlink_health_reporter_state_update(struct devlink_health_reporter *reporter,
				     int state)
{
}
#endif

#ifndef HAVE_DEVLINK_HEALTH_REPORTER_RECOVERY_DONE
static inline void
devlink_health_reporter_recovery_done(struct devlink_health_reporter *reporter)
{
}
#endif

#ifndef HAVE_DEVLINK_HEALTH_REPORT_EXTACK
#define bnxt_fw_diagnose(reporter, priv_ctx, extack)	\
	bnxt_fw_diagnose(reporter, priv_ctx)
#define bnxt_fw_dump(reporter, fmsg, priv_ctx, extack)	\
	bnxt_fw_dump(reporter, fmsg, priv_ctx)
#define bnxt_fw_recover(reporter, priv_ctx, extack)	\
	bnxt_fw_recover(reporter, priv_ctx)
#define bnxt_hw_diagnose(reporter, priv_ctx, extack)	\
	bnxt_hw_diagnose(reporter, priv_ctx)
#define bnxt_hw_recover(reporter, priv_ctx, extack)	\
	bnxt_hw_recover(reporter, priv_ctx)
#endif /* HAVE_DEVLINK_HEALTH_REPORT_EXTACK */
#endif /* HAVE_DEVLINK_HEALTH_REPORT */

#ifdef SET_NETDEV_DEVLINK_PORT
#define HAVE_SET_NETDEV_DEVLINK_PORT	1
#else
#define SET_NETDEV_DEVLINK_PORT(dev, port)
#endif

#ifndef mmiowb
#define mmiowb() do {} while (0)
#endif

#ifndef HAVE_ETH_GET_HEADLEN_NEW
#define eth_get_headlen(dev, data, len)	eth_get_headlen(data, len)
#endif

#ifndef HAVE_NETDEV_XMIT_MORE
#ifdef HAVE_SKB_XMIT_MORE
#define netdev_xmit_more()	skb->xmit_more
#else
#define netdev_xmit_more()	0
#endif
#ifndef HAVE_NETIF_XMIT_STOPPED
#define netif_xmit_stopped(q)	0
#endif
#endif

#ifndef HAVE_NDO_TX_TIMEOUT_QUEUE
#define bnxt_tx_timeout(dev, queue)	bnxt_tx_timeout(dev)
#endif

#ifdef HAVE_NDO_ADD_VXLAN
#ifndef HAVE_VXLAN_GET_RX_PORT
static inline void vxlan_get_rx_port(struct net_device *netdev)
{
}
#endif
#endif

#ifndef HAVE_DEVLINK_VALIDATE_NEW
#define bnxt_dl_msix_validate(dl, id, val, extack)	\
	bnxt_dl_msix_validate(dl, id, val)
#endif

#ifndef kfree_rcu
#define kfree_rcu(ptr, rcu_head)			\
	do {						\
		synchronize_rcu();			\
		kfree(ptr);				\
	} while (0)
#endif

#ifdef HAVE_DEVLINK_FLASH_UPDATE
#ifndef HAVE_DEVLINK_FLASH_UPDATE_BEGIN
static inline void devlink_flash_update_begin_notify(struct devlink *devlink)
{
}

static inline void devlink_flash_update_end_notify(struct devlink *devlink)
{
}
#endif /* HAVE_DEVLINK_FLASH_UPDATE_BEGIN */

#ifndef HAVE_DEVLINK_FLASH_UPDATE_STATUS
static inline void devlink_flash_update_status_notify(struct devlink *devlink,
						      const char *status_msg,
						      const char *component,
						      unsigned long done,
						      unsigned long total)
{
}
#endif
#endif /* HAVE_DEVLINK_FLASH_UPDATE */

#ifdef HAVE_DEVLINK_INFO
#ifndef DEVLINK_INFO_VERSION_GENERIC_ASIC_ID
#define DEVLINK_INFO_VERSION_GENERIC_ASIC_ID	"asic.id"
#define DEVLINK_INFO_VERSION_GENERIC_ASIC_REV	"asic.rev"
#define DEVLINK_INFO_VERSION_GENERIC_FW		"fw"
#endif
#ifndef DEVLINK_INFO_VERSION_GENERIC_FW_PSID
#define DEVLINK_INFO_VERSION_GENERIC_FW_PSID	"fw.psid"
#endif
#ifndef DEVLINK_INFO_VERSION_GENERIC_FW_ROCE
#define DEVLINK_INFO_VERSION_GENERIC_FW_ROCE	"fw.roce"
#endif
#ifndef DEVLINK_INFO_VERSION_GENERIC_FW_MGMT_API
#define DEVLINK_INFO_VERSION_GENERIC_FW_MGMT_API	"fw.mgmt.api"
#endif

#ifndef HAVE_DEVLINK_INFO_BSN_PUT
static inline int devlink_info_board_serial_number_put(struct devlink_info_req *req,
						       const char *bsn)
{
	return 0;
}
#endif
#endif /* HAVE_DEVLINK_INFO */

#ifdef HAVE_DEVLINK_REGISTER_DEV
static inline struct devlink *
bnxt_compat_devlink_alloc(const struct devlink_ops *ops, size_t size,
			  struct device *dev)
{
	struct devlink *d = devlink_alloc(ops, size);

	d->dev = dev;
	return d;
}

#define devlink_register(dl)	devlink_register(dl, &bp->pdev->dev)
#define devlink_alloc	bnxt_compat_devlink_alloc
#endif /* HAVE_DEVLINK_REGISTER_NEW */

#ifndef HAVE_PCIE_FLR
static inline int pcie_flr(struct pci_dev *dev)
{
	pcie_capability_set_word(dev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_BCR_FLR);

	msleep(100);

	return 0;
}
#endif /* pcie_flr */

#ifndef fallthrough
#if defined __has_attribute
#ifndef __GCC4_has_attribute___fallthrough__
#define __GCC4_has_attribute___fallthrough__ 0
#endif
#if __has_attribute(__fallthrough__)
#define fallthrough	__attribute__((__fallthrough__))
#else
#define fallthrough do {} while (0)  /* fall through */
#endif
#else
#define fallthrough do {} while (0)  /* fall through */
#endif
#endif

#ifndef __ALIGN_KERNEL_MASK
#define __ALIGN_KERNEL_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#endif
#ifndef __ALIGN_KERNEL
#define __ALIGN_KERNEL(x, a)	__ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)
#endif
#ifndef ALIGN_DOWN
#define ALIGN_DOWN(x, a)	__ALIGN_KERNEL((x) - ((a) - 1), (a))
#endif

#ifndef HAVE_DMA_POOL_ZALLOC
struct bnxt_compat_dma_pool {
	struct dma_pool *pool;
	size_t size;
};

static inline struct bnxt_compat_dma_pool*
bnxt_compat_dma_pool_create(const char *name, struct device *dev, size_t size,
			    size_t align, size_t allocation)
{
	struct bnxt_compat_dma_pool *wrapper;

	wrapper = kmalloc_node(sizeof(*wrapper), GFP_KERNEL, dev_to_node(dev));
	if (!wrapper)
		return NULL;

	wrapper->pool = dma_pool_create(name, dev, size, align, allocation);
	wrapper->size = size;

	return wrapper;
}

static inline void
bnxt_compat_dma_pool_destroy(struct bnxt_compat_dma_pool *wrapper)
{
	dma_pool_destroy(wrapper->pool);
	kfree(wrapper);
}

static inline void *
bnxt_compat_dma_pool_alloc(struct bnxt_compat_dma_pool *wrapper,
			   gfp_t mem_flags, dma_addr_t *handle)
{
	void *mem;

	mem = dma_pool_alloc(wrapper->pool, mem_flags & ~__GFP_ZERO, handle);
	if (mem_flags & __GFP_ZERO)
		memset(mem, 0, wrapper->size);
	return mem;
}

static inline void
bnxt_compat_dma_pool_free(struct bnxt_compat_dma_pool *wrapper, void *vaddr,
			  dma_addr_t addr)
{
	dma_pool_free(wrapper->pool, vaddr, addr);
}

#define dma_pool_create bnxt_compat_dma_pool_create
#define dma_pool_destroy bnxt_compat_dma_pool_destroy
#define dma_pool_alloc bnxt_compat_dma_pool_alloc
#define dma_pool_free bnxt_compat_dma_pool_free
#define dma_pool bnxt_compat_dma_pool
#endif /* HAVE_DMA_POOL_ZALLOC */

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

#ifndef _NET_NETMEM_H
typedef struct page *netmem_ref;
#endif

#include "bnxt.h"
#ifndef PCI_IRQ_MSIX

static inline int
pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
		      unsigned int max_vecs, unsigned int flags)
{
	struct net_device *netdev = pci_get_drvdata(dev);
	struct bnxt *bp = netdev_priv(netdev);
	int i;

	if (!bp->msix_ent)
		bp->msix_ent = kcalloc(max_vecs, sizeof(*bp->msix_ent),
				       GFP_KERNEL);
	if (!bp->msix_ent)
		return -ENOMEM;

	for (i = 0; i < max_vecs; i++)
		bp->msix_ent[i].entry = i;

	return pci_enable_msix_range(dev, bp->msix_ent, min_vecs, max_vecs);
}

static inline int pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
	struct net_device *netdev = pci_get_drvdata(dev);
	struct bnxt *bp = netdev_priv(netdev);

	return bp->msix_ent[nr].vector;
}

static inline void pci_free_irq_vectors(struct pci_dev *dev)
{
	struct net_device *netdev = pci_get_drvdata(dev);
	struct bnxt *bp = netdev_priv(netdev);

	pci_disable_msix(dev);
	kfree(bp->msix_ent);
	bp->msix_ent = NULL;
}
#endif /* PCI_IRQ_MSIX */

#ifndef HAVE_NETIF_NAPI_DEL_NEW
static inline void __netif_napi_del(struct napi_struct *napi)
{
	napi_hash_del(napi);
	netif_napi_del(napi);
}
#endif /* HAVE_NETIF_NAPI_DEL_NEW */

#ifndef HAVE_NETIF_NAPI_ADD_CONFIG
#define netif_napi_add_config(dev, napi, fn, index)	\
	netif_napi_add(dev, napi, fn)
#endif /* HAVE_NETIF_NAPI_ADD_CONFIG */

#ifndef HAVE_NAPI_HASH_ADD
static inline void netif_hash_add(struct napi_struct *napi)
{
}
#endif

#ifdef HAVE_NETIF_NAPI_ADD_WITH_WEIGHT_ARG
#define netif_napi_add(dev, napi, fn)			\
do {							\
	netif_napi_add(dev, napi, fn, 64);		\
	netif_hash_add(napi);				\
} while (0)
#endif

#include "bnxt_compat_link_modes.h"

#ifndef HAVE_ETHTOOL_LINK_KSETTINGS
struct ethtool_link_settings {
	u32	cmd;
	u32	speed;
	u8	duplex;
	u8	port;
	u8	phy_address;
	u8	autoneg;
};

struct ethtool_link_ksettings {
	struct ethtool_link_settings base;
	struct {
		DECLARE_BITMAP(supported, __ETHTOOL_LINK_MODE_MASK_NBITS);
		DECLARE_BITMAP(advertising, __ETHTOOL_LINK_MODE_MASK_NBITS);
		DECLARE_BITMAP(lp_advertising, __ETHTOOL_LINK_MODE_MASK_NBITS);
	} link_modes;
};

#define ethtool_link_ksettings_zero_link_mode(ptr, name)		\
	(ptr)->link_modes.name[0] = 0

int bnxt_get_settings(struct net_device *dev, struct ethtool_cmd *cmd);
int bnxt_set_settings(struct net_device *dev, struct ethtool_cmd *cmd);
#endif

#if defined(HAVE_ETHTOOL_RXFH_PARAM) && \
	defined(HAVE_ETH_RXFH_CONTEXT_ALLOC) && \
	!defined(HAVE_NEW_RSSCTX_INTERFACE)
#define BNXT_RSSCTX_DISABLED
#endif

#if !defined(HAVE_ETHTOOL_RXFH_PARAM)
#if defined(HAVE_ETH_RXFH_CONTEXT_ALLOC)
int bnxt_set_rxfh_context(struct net_device *dev, const u32 *indir,
			  const u8 *key, const u8 hfunc, u32 *rss_context,
			  bool delete);
int bnxt_get_rxfh_context(struct net_device *dev, u32 *indir, u8 *key,
			  u8 *hfunc, u32 rss_context);
#endif
int bnxt_get_rxfh(struct net_device *dev, u32 *indir, u8 *key, u8 *hfunc);
int bnxt_set_rxfh(struct net_device *dev, const u32 *indir, const u8 *key,
		  const u8 hfunc);
#endif

#ifndef HAVE_SET_RXFH_FIELDS
int bnxt_grxfh(struct bnxt *bp, struct ethtool_rxnfc *cmd);
int bnxt_srxfh(struct bnxt *bp, struct ethtool_rxnfc *cmd);
#endif

#if !defined(HAVE_ETHTOOL_KEEE)
int bnxt_set_eee(struct net_device *dev, struct ethtool_eee *edata);
int bnxt_get_eee(struct net_device *dev, struct ethtool_eee *edata);
#endif

#if defined(HAVE_DEVLINK) && !defined(HAVE_DEVLINK_RELOAD_DISABLE)
#define devlink_reload_enable(x)
#define devlink_reload_disable(x)
#endif

#if defined(HAVE_DEVLINK) && !defined(HAVE_DEVLINK_SET_FEATURES)
#define devlink_set_features(x, y)
#endif

#if defined(HAVE_DEVLINK) && !defined(HAVE_DEVLINK_PARAM_SET_EXTACK)
#define bnxt_dl_nvm_param_set(dl, id, ctx, extack)	\
	bnxt_dl_nvm_param_set(dl, id, ctx)

#define bnxt_dl_truflow_param_set(dl, id, ctx, extack)	\
	bnxt_dl_truflow_param_set(dl, id, ctx)

#define __NL_SET_ERR_MSG_MOD(extack, msg)
#define __NL_SET_ERR_MSG_FMT_MOD(extack, fmt, args...)

#define bnxt_remote_dev_reset_set(dl, id, ctx, extack)	\
	bnxt_remote_dev_reset_set(dl, id, ctx)
#else
#define __NL_SET_ERR_MSG_MOD		NL_SET_ERR_MSG_MOD
#define __NL_SET_ERR_MSG_FMT_MOD	NL_SET_ERR_MSG_FMT_MOD
#endif

#ifndef HAVE_STRSCPY
static inline ssize_t strscpy(char *dest, const char *src, size_t count)
{
	int len = strlcpy(dest, src, count);

	return (!count || count <= len) ? -E2BIG : len;
}
#elif defined(HAVE_OLD_STRSCPY)
#define strscpy(d, s, c) strlcpy(d, s, c)
#endif

#ifndef HAVE_ETHTOOL_PARAMS_FROM_LINK_MODE
struct link_mode_info {
	int                             speed;
	u8                              lanes;
	u8                              duplex;
};

void
ethtool_params_from_link_mode(struct ethtool_link_ksettings *link_ksettings,
			      enum ethtool_link_mode_bit_indices link_mode);
#endif

static inline void bnxt_compat_linkmode_set_bit(int nr, unsigned long *addr)
{
	if (nr < __ETHTOOL_LINK_MODE_MASK_NBITS)
		__set_bit(nr, addr);
}

static inline int bnxt_compat_linkmode_test_bit(int nr, const unsigned long *addr)
{
	return (nr < __ETHTOOL_LINK_MODE_MASK_NBITS) ? test_bit(nr, addr) : 0;
}

#ifndef HAVE_NEW_LINKMODE
#define linkmode_set_bit bnxt_compat_linkmode_set_bit
#define linkmode_test_bit bnxt_compat_linkmode_test_bit
#endif

#ifndef PFC_STORM_PREVENTION_DISABLE
#define ETHTOOL_PFC_PREVENTION_TOUT	3
#define PFC_STORM_PREVENTION_AUTO	0xffff
#endif

#if !defined(HAVE_FLOW_DISSECTOR) || \
	!defined(HAVE_SKB_FLOW_DISSECT_WITH_FLAGS) || \
	!defined(HAVE_FLOW_KEY_CONTROL_FLAGS)

struct bnxt_compat_key_control {
	u32 flags;
};

struct bnxt_compat_key_basic {
	__be16 n_proto;
	u8 ip_proto;
};

struct bnxt_compat_key_ports {
	__be16 src;
	__be16 dst;
};

struct bnxt_compat_key_ipv4 {
	__be32 src;
	__be32 dst;
};

struct bnxt_compat_key_ipv6 {
	struct in6_addr src;
	struct in6_addr dst;
};

struct bnxt_compat_key_addrs {
	union {
		struct bnxt_compat_key_ipv4 v4addrs;
		struct bnxt_compat_key_ipv6 v6addrs;
	};
};

struct bnxt_compat_flow_keys {
	struct bnxt_compat_key_control control;
#define FLOW_KEYS_HASH_START_FIELD basic
	struct bnxt_compat_key_basic basic;
	struct bnxt_compat_key_ports ports;
	struct bnxt_compat_key_addrs addrs;
};

#define FLOW_KEYS_HASH_OFFSET	\
	offsetof(struct flow_keys, FLOW_KEYS_HASH_START_FIELD)

#ifdef HAVE_FLOW_KEYS
#include <net/flow_keys.h>
#endif

static inline bool
skb_compat_flow_dissect_flow_keys(const struct sk_buff *skb,
				  struct bnxt_compat_flow_keys *flow,
				  unsigned int flags)
{
#if defined(HAVE_FLOW_KEYS)
	/* this is the legacy structure from flow_keys.h */
	struct flow_keys legacy_flow = { 0 };

	if (skb->protocol != htons(ETH_P_IP))
		return false;

	if (!skb_flow_dissect(skb, &legacy_flow))
		return false;

	flow->addrs.v4addrs.src = legacy_flow.src;
	flow->addrs.v4addrs.dst = legacy_flow.dst;
	flow->ports.src = legacy_flow.port16[0];
	flow->ports.dst = legacy_flow.port16[1];
	flow->basic.n_proto = htons(ETH_P_IP);
	flow->basic.ip_proto = legacy_flow.ip_proto;
	flow->control.flags = 0;

	return true;
#elif defined(HAVE_FLOW_DISSECTOR)
	/* this is the older version of flow_keys, which excludes flags in
	 * flow_dissector_key_control, as defined in 4.2's flow_dissector.h
	 */
	struct flow_keys legacy_flow;

	memset(&legacy_flow, 0, sizeof(legacy_flow));
	if (!skb_flow_dissect_flow_keys(skb, &legacy_flow))
		return false;

	if (legacy_flow.basic.ip_proto == htons(ETH_P_IP)) {
		flow->addrs.v4addrs.src = legacy_flow.addrs.v4addrs.src;
		flow->addrs.v4addrs.dst = legacy_flow.addrs.v4addrs.dst;
	} else if (legacy_flow.basic.ip_proto == htons(ETH_P_IPV6)) {
		flow->addrs.v6addrs.src = legacy_flow.addrs.v6addrs.src;
		flow->addrs.v6addrs.dst = legacy_flow.addrs.v6addrs.dst;
	} else {
		return false;
	}

	flow->ports.src = legacy_flow.ports.src;
	flow->ports.dst = legacy_flow.ports.dst;
	flow->basic.n_proto = legacy_flow.basic.n_proto;
	flow->basic.ip_proto = legacy_flow.basic.ip_proto;
	flow->control.flags = 0;

	return true;
#else
	return false;
#endif
}

#define skb_flow_dissect_flow_keys skb_compat_flow_dissect_flow_keys

#ifndef HAVE_FLOW_KEY_CONTROL_FLAGS
#define FLOW_DIS_IS_FRAGMENT	1
#define FLOW_DIS_ENCAPSULATION	4
#endif
#define flow_dissector_key_ports bnxt_compat_key_ports
#define flow_dissector_key_addrs bnxt_compat_key_addrs
#define flow_keys bnxt_compat_flow_keys
#endif

#ifndef HAVE_ETH_HW_ADDR_SET
static inline void eth_hw_addr_set(struct net_device *dev, const u8 *addr)
{
	memcpy(dev->dev_addr, addr, ETH_ALEN);
}
#endif

#ifndef HAVE_BITMAP_ZALLOC

static inline unsigned long *bitmap_alloc(unsigned int nbits, gfp_t flags)
{
	return kmalloc_array(BITS_TO_LONGS(nbits), sizeof(unsigned long),
			     flags);
}

static inline unsigned long *bitmap_zalloc(unsigned int nbits, gfp_t flags)
{
	return bitmap_alloc(nbits, flags | __GFP_ZERO);
}

static inline void bitmap_free(const unsigned long *bitmap)
{
	kfree(bitmap);
}

#endif

#ifndef HAVE_DEFINE_STATIC_KEY
#ifndef HAVE_STATIC_KEY_INITIALIZED
#define STATIC_KEY_CHECK_USE()
#endif
#define DEFINE_STATIC_KEY_FALSE(name)	\
	struct static_key name = STATIC_KEY_INIT_FALSE
#define DECLARE_STATIC_KEY_FALSE(name)	\
	extern struct static_key name
#define static_branch_unlikely(x)	unlikely(static_key_enabled(&(x)->key))
static inline void static_branch_enable(struct static_key *key)
{
	STATIC_KEY_CHECK_USE();

	if (atomic_read(&key->enabled) != 0) {
		WARN_ON_ONCE(atomic_read(&key->enabled) != 1);
		return;
	}
	atomic_set(&key->enabled, 1);
}

static inline void static_branch_disable(struct static_key *key)
{
	STATIC_KEY_CHECK_USE();

	if (atomic_read(&key->enabled) != 1) {
		WARN_ON_ONCE(atomic_read(&key->enabled) != 0);
		return;
	}
	atomic_set(&key->enabled, 0);
}
#else
#if !defined(HAVE_DECLARE_STATIC_KEY)
#define DECLARE_STATIC_KEY_FALSE(name)	\
	extern struct static_key_false name
#endif
#endif /* !defined(HAVE_DEFINE_STATIC_KEY) */

#ifdef HAVE_ARTNS_TO_TSC
#ifndef CONFIG_X86
static inline struct system_counterval_t convert_art_ns_to_tsc(u64 art_ns)
{
	WARN_ONCE(1, "%s is only supported on X86", __func__);
	return (struct system_counterval_t){};
}
#endif /* CONFIG_X86 */
#endif /* HAVE_ARTNS_TO_TSC */

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)) && \
	(defined(UEK_KABI_UNIQUE_ID)))
#define UEK_KERNEL_WITH_NS_DMA_BUF
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0)) || \
	(defined(RHEL_RELEASE_CODE) && \
	(RHEL_RELEASE_VERSION(9, 0) <= RHEL_RELEASE_CODE)) || \
	(defined(CONFIG_SUSE_KERNEL) && \
	((CONFIG_SUSE_VERSION == 15) && (CONFIG_SUSE_PATCHLEVEL >= 5))) || \
	(defined(UEK_KERNEL_WITH_NS_DMA_BUF))
#define HAVE_MODULE_IMPORT_NS_DMA_BUF
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0))
#define HAVE_MODULE_IMPORT_NS_NETDEV_INTERNAL
#endif

#ifndef NL_SET_ERR_MSG_MOD
#define NL_SET_ERR_MSG_MOD(extack, msg)
#endif

#ifndef HAVE_NETLINK_EXT_ACK
struct netlink_ext_ack {
};
#endif

#ifndef nla_for_each_nested_type
#define nla_for_each_nested_type(pos, type, nla, rem) \
	nla_for_each_nested(pos, nla, rem) \
		if (nla_type(pos) == type)
#endif

#ifndef HAVE_SKB_MARK_RECYCLE
#define skb_mark_for_recycle(skb)
#endif
#ifdef HAVE_OLD_SKB_MARK_RECYCLE
#define skb_mark_for_recycle(skb) skb_mark_for_recycle(skb, page, rxr->page_pool)
#endif

#ifdef HAVE_OLD_XSK_BUFF_DMA_SYNC
#define xsk_buff_dma_sync_for_cpu(xdp) xsk_buff_dma_sync_for_cpu((xdp), rxr->xsk_pool)
#endif

#ifdef CONFIG_BNXT_HWMON
#include <linux/hwmon.h>
#ifndef HWMON_CHANNEL_INFO
#define HWMON_CHANNEL_INFO(stype, ...)	\
	(&(struct hwmon_channel_info) {	\
		.type = hwmon_##stype,	\
		.config = (u32 []) {	\
			__VA_ARGS__, 0	\
		}			\
	})
#endif /* HWMON_CHANNEL_INFO */

#ifndef HAVE_HWMON_NOTIFY_EVENT
static inline void hwmon_notify_event(struct device *dev, enum hwmon_sensor_types type,
				      u32 attr, int channel)
{
}
#endif /* HAVE_HWMON_NOTIFY_EVENT */
#endif /* CONFIG_BNXT_HWMON */

#if !defined(HAVE_PAGE_POOL_PP_FRAG_BIT)
#define PP_FLAG_PAGE_FRAG	0

#ifndef HAVE_PAGE_POOL_PAGE_FRAG
#define page_pool_dev_alloc_frag(page_pool, offset, size)	NULL

#ifdef CONFIG_PAGE_POOL
#include <net/page_pool.h>

static inline struct page *
page_pool_alloc_frag(struct page_pool *page_pool, unsigned int *offset,
		     unsigned int size, gfp_t gfp)
{
	*offset = 0;
	return page_pool_alloc_pages(page_pool, gfp);
}
#else
#define page_pool_dev_alloc_frag(page_pool, offset, size)	NULL
#endif	/* CONFIG_PAGE_POOL */

#endif
#endif

#ifndef HAVE_PAGE_POOL_GET_DMA_ADDR
#define page_pool_get_dma_addr(page) \
	dma_map_page_attrs(&bp->pdev->dev, page, 0, \
			   BNXT_RX_PAGE_SIZE, bp->rx_dir, \
			   DMA_ATTR_WEAK_ORDERING)
#endif

#ifndef PP_FLAG_DMA_SYNC_DEV
#define PP_FLAG_DMA_SYNC_DEV	0
#endif

#ifdef HAVE_PAGE_POOL_NETMEM

#ifndef HAVE_PAGE_POOL_ALLOC_NETMEMS
#define page_pool_alloc_netmems(pp, gfp)	page_pool_alloc_netmem(pp, gfp)
#endif
#ifndef HAVE_NETMEM_IS_PFMEMALLOC
#define netmem_is_pfmemalloc(n)	page_is_pfmemalloc(netmem_to_page(n))
#endif
#ifndef HAVE_PAGE_POOL_DMA_SYNC_FOR_CPU
#define page_pool_dma_sync_for_cpu(pp, page, off, size)			\
	dma_sync_single_range_for_cpu((pp)->p.dev,			\
				      page_pool_get_dma_addr(page),	\
				      (off) + (pp)->p.offset, size,	\
				      page_pool_get_dma_dir(pp))
#endif
#ifndef HAVE_PAGE_POOL_IS_UNREADABLE
#define page_pool_recycle_direct_netmem(pp, n)				\
	page_pool_put_full_netmem(pp, n, true)
#endif

#else	/* HAVE_PAGE_POOL_NETMEM */

#define page_pool_alloc_netmems(pp, gfp)				\
	(__force netmem_ref)page_pool_alloc_pages(pp, gfp)

#define page_pool_alloc_frag_netmem(pp, off, size, gfp)			\
	(__force netmem_ref)page_pool_alloc_frag(pp, off, size, gfp)

#define page_pool_get_dma_addr_netmem(n)				\
	page_pool_get_dma_addr((__force struct page *)n)

#define netmem_is_pfmemalloc(n)						\
	page_is_pfmemalloc((__force struct page *)n)

#define PP_FLAG_ALLOW_UNREADABLE_NETMEM		0

#define skb_add_rx_frag_netmem(skb, i, n, off, size, true_size)		\
do {									\
	struct skb_shared_info *shinfo = skb_shinfo(skb);		\
	skb_frag_t *frag = &shinfo->frags[i];				\
									\
	skb_frag_fill_page_desc(frag, (struct page *)n, off, size);	\
	(skb)->len += size;						\
	(skb)->data_len += size;					\
	(skb)->truesize += true_size;					\
	shinfo->nr_frags = (i) + 1;					\
} while (0)

#define skb_frag_fill_netmem_desc(frag, n, off, size)			\
	skb_frag_fill_page_desc(frag, (struct page *)n, off, size)

#define page_pool_recycle_direct_netmem(pp, n)				\
	page_pool_recycle_direct(pp, (struct page *)n)

#endif /* HAVE_PAGE_POOL_NETMEM */

#ifndef HAVE_PAGE_POOL_IS_UNREADABLE
#define page_pool_is_unreadable(pp)		false
#endif

#ifndef HAVE_PAGE_POOL_DMA_SYNC_NETMEM_FOR_CPU
#if (PP_FLAG_DMA_SYNC_DEV)
#define page_pool_dma_sync_netmem_for_cpu(pp, n, off, size)		\
	dma_sync_single_range_for_cpu((pp)->p.dev,			\
				      page_pool_get_dma_addr_netmem(n),	\
				      (off) + (pp)->p.offset, size,	\
				      page_pool_get_dma_dir(pp))
#else
#define page_pool_dma_sync_netmem_for_cpu(pp, n, off, size)		\
	dma_sync_single_range_for_cpu((pp)->p.dev,			\
				      page_pool_get_dma_addr_netmem(n),	\
				      off, size, page_pool_get_dma_dir(pp))
#endif
#endif

#ifdef netmem_dma_unmap_addr_set
#define HAVE_NETMEM_TX	1
#else
#define netmem_dma_unmap_addr_set(NETMEM, PTR, ADDR_NAME, VAL)		\
	dma_unmap_addr_set(PTR, ADDR_NAME, VAL)

#define netmem_dma_unmap_page_attrs(dev, addr, size, dir, attr)		\
	dma_unmap_page(dev, addr, size, dir)
#endif	/* netmem_dma_unmap_addr_set */

#ifndef HAVE_SKB_FRAGS_READABLE
#define skb_frags_readable(skb)	true
#endif

#if (defined(GRO_MAX_SIZE) && (GRO_MAX_SIZE > 65536))
#define HAVE_IPV6_BIG_TCP
#endif

#ifndef HAVE_IPV6_HOPOPT_JUMBO_REMOVE
#ifdef DHAVE_IPV6_BIG_TCP
static inline int ipv6_hopopt_jumbo_remove(struct sk_buff *skb)
{
	const int hophdr_len = sizeof(struct hop_jumbo_hdr);
	int nexthdr = ipv6_has_hopopt_jumbo(skb);
	struct ipv6hdr *h6;

	if (!nexthdr)
		return 0;

	if (skb_cow_head(skb, 0))
		return -1;

	/* Remove the HBH header.
	 * Layout: [Ethernet header][IPv6 header][HBH][L4 Header]
	 */
	memmove(skb_mac_header(skb) + hophdr_len, skb_mac_header(skb),
		skb_network_header(skb) - skb_mac_header(skb) +
		sizeof(struct ipv6hdr));

	__skb_pull(skb, hophdr_len);
	skb->network_header += hophdr_len;
	skb->mac_header += hophdr_len;

	h6 = ipv6_hdr(skb);
	h6->nexthdr = nexthdr;

	return 0;
}
#else
static inline int ipv6_hopopt_jumbo_remove(struct sk_buff *skb)
{
	return 0;
}
#endif /* DHAVE_IPV6_BIG_TCP */
#endif /* HAVE_IPV6_HOPOPT_JUMBO_REMOVE */

#ifndef HAVE_PAGE_POOL_DISABLE_DIRECT_RECYCLING
#define page_pool_disable_direct_recycling(pool)  do { } while (0)
#endif

#ifndef HAVE_XDP_SET_REDIR_TARGET_LOCKED
#ifndef HAVE_XDP_SET_REDIR_TARGET
static inline void
xdp_features_set_redirect_target(struct net_device *dev, bool support_sg)
{
}

static inline void
xdp_features_clear_redirect_target(struct net_device *dev)
{
}
#endif /* HAVE_XDP_SET_REDIR_TARGET */

static inline void
xdp_features_set_redirect_target_locked(struct net_device *dev, bool support_sg)
{
#ifdef HAVE_NETDEV_LOCK
	netdev_unlock(dev);
#endif
	xdp_features_set_redirect_target(dev, support_sg);
#ifdef HAVE_NETDEV_LOCK
	netdev_lock(dev);
#endif
}

static inline void
xdp_features_clear_redirect_target_locked(struct net_device *dev)
{
#ifdef HAVE_NETDEV_LOCK
	netdev_unlock(dev);
#endif
	xdp_features_clear_redirect_target(dev);
#ifdef HAVE_NETDEV_LOCK
	netdev_lock(dev);
#endif
}
#endif	/* HAVE_XDP_SET_REDIR_TARGET_LOCKED */

#ifndef HAVE_PERNET_HASH
#define __inet6_lookup_established(n, h, sa, sp, da, dp, dif, sdif)    \
	__inet6_lookup_established(n, &tcp_hashinfo, sa, sp, da, dp, dif, sdif)
#define inet_lookup_established(n, h, sa, sp, da, dp, dif)    \
	inet_lookup_established(n, &tcp_hashinfo, sa, sp, da, dp, dif)
#endif

#ifndef HAVE_SYSFS_EMIT
#define sysfs_emit	sprintf
#endif /* HAVE_SYSFS_EMIT */

#ifndef class_create
#define class_create(owner, name)	class_create(name)
#endif

#ifndef HAVE_PCIE_ERROR_REPORTING
#define pci_enable_pcie_error_reporting(pdev)
#define pci_disable_pcie_error_reporting(pdev)
#endif

#ifndef HAVE_TLS_IS_SKB_TX_DEVICE_OFFLOADED
#define tls_is_skb_tx_device_offloaded(skb)	tls_is_sk_tx_device_offloaded((skb)->sk)
#endif

#ifndef HAVE_XDP_DATA_META
#define skb_metadata_set(skb, metasize)
#endif

#ifndef HAVE_TXQ_MAYBE_WAKE
#define __netif_txq_maybe_wake(txq, get_desc, start_thrs, down_cond)	\
	({								\
		int _res;						\
									\
		_res = -1;						\
		if (likely(get_desc > start_thrs)) {			\
			/* Make sure that anybody stopping the queue after \
			 * this sees the new next_to_clean.		\
			 */						\
			smp_mb();					\
			_res = 1;					\
			if (unlikely(netif_tx_queue_stopped(txq)) &&	\
			    !(down_cond)) {				\
				netif_tx_wake_queue(txq);		\
				_res = 0;				\
			}						\
		}							\
		_res;							\
	})
#endif /* HAVE_TXQ_MAYBE_WAKE */

#ifndef __netif_txq_completed_wake
static inline void
netdev_txq_completed_mb(struct netdev_queue *dev_queue,
		       unsigned int pkts, unsigned int bytes)
{
	if (IS_ENABLED(CONFIG_BQL))
		netdev_tx_completed_queue(dev_queue, pkts, bytes);
	else if (bytes)
		smp_mb();
}

#define __netif_txq_completed_wake(txq, pkts, bytes,		   \
				  get_desc, start_thrs, down_cond)     \
	({							      \
		int _res;					       \
								       \
		/* Report to BQL and piggy back on its barrier.	 \
		* Barrier makes sure that anybody stopping the queue   \
		* after this point sees the new consumer index.	\
		* Pairs with barrier in netif_txq_try_stop().	  \
		*/						     \
		netdev_txq_completed_mb(txq, pkts, bytes);	      \
								       \
		_res = -1;					      \
		if (pkts && likely(get_desc > start_thrs)) {	    \
			_res = 1;				       \
			if (unlikely(netif_tx_queue_stopped(txq)) &&    \
			    !(down_cond)) {			     \
				netif_tx_wake_queue(txq);	       \
				_res = 0;			       \
			}					       \
		}						       \
		_res;						   \
	})

#define netif_txq_try_stop(txq, get_desc, start_thrs)		  \
	({							      \
		int _res;					       \
								       \
		netif_tx_stop_queue(txq);			       \
		/* Producer index and stop bit must be visible	  \
		* to consumer before we recheck.		       \
		* Pairs with a barrier in __netif_txq_completed_wake(). \
		*/						     \
		smp_mb__after_atomic();				 \
								       \
		/* We need to check again in a case another	     \
		* CPU has just made room available.		    \
		*/						     \
		_res = 0;					       \
		if (unlikely(get_desc >= start_thrs)) {		 \
			netif_tx_start_queue(txq);		      \
			_res = -1;				      \
		}						       \
		_res;						   \
	})							      \

#endif

#ifndef HAVE_NETDEV_LOCK
#define napi_enable_locked(n)		napi_enable(n)
#define napi_disable_locked(n)		napi_disable(n)
#define __netif_napi_del_locked(n)	__netif_napi_del(n)
#define netif_napi_set_irq_locked(n, v) netif_napi_set_irq(n, v)
#define netif_napi_add_locked(d, n, f)	netif_napi_add(d, n, f)
#define netif_napi_add_config_locked(d, n, f, i)	\
	netif_napi_add_config(d, n, f, i)

#define netdev_lock(d)			rtnl_lock()
#define netdev_unlock(d)		rtnl_unlock()
#define netdev_assert_locked(d)		ASSERT_RTNL()
#define netdev_ops_assert_locked(d)	ASSERT_RTNL()
#define netdev_trylock(d)		rtnl_trylock()
#define netdev_lock_dereference(p, dev)	rtnl_dereference(p)

#define netdev_assert_locked_or_invisible(d)		\
do {							\
	if ((d)->reg_state == NETREG_REGISTERED ||	\
	    (d)->reg_state == NETREG_UNREGISTERING)	\
		netdev_assert_locked(d);		\
} while (0)

#define netif_close(d)			dev_close(d)
#endif

#ifndef HAVE_NETIF_QUEUE_SET_NAPI

enum netdev_queue_type {
	NETDEV_QUEUE_TYPE_RX,
	NETDEV_QUEUE_TYPE_TX,
};

static inline
void netif_queue_set_napi(struct net_device *dev, unsigned int queue_index,
			  enum netdev_queue_type type,
			  struct napi_struct *napi)
{
}

static inline void netif_napi_set_irq(struct napi_struct *napi, int irq)
{
}
#endif

#ifndef __counted_by
#define __counted_by(member)
#endif

#ifndef struct_size
#define struct_size(p, member, n) (sizeof(*(p)) + sizeof(*(p)->member) * (n))
#endif

#ifndef sizeof_field
#define sizeof_field(TYPE, MEMBER) sizeof((((TYPE *)0)->MEMBER))
#endif /* sizeof_field */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 14, 0))
#undef HAVE_NETDEV_RX_Q_RESTART
#endif

#if defined(HAVE_NETDEV_QMGMT_OPS) && defined(HAVE_QMGMT_OPS_IN_NETDEV_H)
#ifndef HAVE_NETDEV_RX_Q_RESTART
static inline int netdev_rx_queue_restart(struct net_device *dev, unsigned int rxq_idx)
{
	void *new_mem, *old_mem;
	int err;

	if (!dev->queue_mgmt_ops || !dev->queue_mgmt_ops->ndo_queue_stop ||
	    !dev->queue_mgmt_ops->ndo_queue_mem_free ||
	    !dev->queue_mgmt_ops->ndo_queue_mem_alloc ||
	    !dev->queue_mgmt_ops->ndo_queue_start)
		return -EOPNOTSUPP;

	DEBUG_NET_WARN_ON_ONCE(!rtnl_is_locked());

	new_mem = kvzalloc(dev->queue_mgmt_ops->ndo_queue_mem_size, GFP_KERNEL);
	if (!new_mem)
		return -ENOMEM;

	old_mem = kvzalloc(dev->queue_mgmt_ops->ndo_queue_mem_size, GFP_KERNEL);
	if (!old_mem) {
		err = -ENOMEM;
		goto err_free_new_mem;
	}

	err = dev->queue_mgmt_ops->ndo_queue_mem_alloc(dev, new_mem, rxq_idx);
	if (err)
		goto err_free_old_mem;

	err = dev->queue_mgmt_ops->ndo_queue_stop(dev, old_mem, rxq_idx);
	if (err)
		goto err_free_new_queue_mem;

	err = dev->queue_mgmt_ops->ndo_queue_start(dev, new_mem, rxq_idx);
	if (err)
		goto err_start_queue;

	dev->queue_mgmt_ops->ndo_queue_mem_free(dev, old_mem);

	kvfree(old_mem);
	kvfree(new_mem);

	return 0;

err_start_queue:
	/* Restarting the queue with old_mem should be successful as we haven't
	 * changed any of the queue configuration, and there is not much we can
	 * do to recover from a failure here.
	 *
	 * WARN if the we fail to recover the old rx queue, and at least free
	 * old_mem so we don't also leak that.
	 */
	if (dev->queue_mgmt_ops->ndo_queue_start(dev, old_mem, rxq_idx)) {
		WARN(1,
		     "Failed to restart old queue in error path. RX queue %d may be unhealthy.",
		     rxq_idx);
		dev->queue_mgmt_ops->ndo_queue_mem_free(dev, old_mem);
	}

err_free_new_queue_mem:
	dev->queue_mgmt_ops->ndo_queue_mem_free(dev, new_mem);

err_free_old_mem:
	kvfree(old_mem);

err_free_new_mem:
	kvfree(new_mem);

	return err;
}
#endif
#endif /* HAVE_NETDEV_QMGMT_OPS */

#ifndef HAVE_NEW_RSSCTX_INTERFACE
#define ethtool_rxfh_context bnxt_rss_ctx
#define ethtool_rxfh_context_priv(x)	x
#define ethtool_rxfh_context_indir(x)	((x)->rss_indir_tbl)

static inline void bnxt_clear_rss_ctxs_compat(struct bnxt *bp, bool all)
{
	struct bnxt_rss_ctx *rss_ctx, *tmp;

	list_for_each_entry_safe(rss_ctx, tmp, &bp->rss_ctx_list, list) {
		bnxt_del_one_rss_ctx(bp, rss_ctx, all, true);
	}

	if (all)
		bitmap_free(bp->rss_ctx_bmap);
}

static inline int bnxt_alloc_rss_indir_tbl_compat(struct bnxt *bp, struct bnxt_rss_ctx *rss_ctx)
{
	int entries;
	u32 *tbl;

	if (bp->flags & BNXT_FLAG_CHIP_P5_PLUS)
		entries = BNXT_MAX_RSS_TABLE_ENTRIES_P5;
	else
		entries = HW_HASH_INDEX_SIZE;

	bp->rss_indir_tbl_entries = entries;
	tbl = kmalloc_array(entries, sizeof(*bp->rss_indir_tbl),
			    GFP_KERNEL);
	if (!tbl)
		return -ENOMEM;

	if (rss_ctx)
		rss_ctx->rss_indir_tbl = tbl;
	else
		bp->rss_indir_tbl = tbl;

	return 0;
}

#endif /* HAVE_NEW_RSSCTX_INTERFACE */

static inline int bnxt_alloc_rssctx_bmap(struct bnxt *bp)
{
#ifdef HAVE_NEW_RSSCTX_INTERFACE
	return 0;
#else
	bp->rss_cap &= ~BNXT_RSS_CAP_MULTI_RSS_CTX;
	bp->rss_ctx_bmap = bitmap_zalloc(BNXT_RSS_CTX_BMAP_LEN, GFP_KERNEL);
	if (bp->rss_ctx_bmap) {
		/* burn index 0 since we cannot have context 0 */
		__set_bit(0, bp->rss_ctx_bmap);
		bp->rss_cap |= BNXT_RSS_CAP_MULTI_RSS_CTX;
		return 0;
	}
	return -ENOMEM;
#endif
}

#ifndef HAVE_IRQ_UPDATE_AFFINITY_HINT
#define irq_update_affinity_hint(irq, mask) irq_set_affinity_hint(irq, mask)
#endif /* HAVE_IRQ_UPDATE_AFFINITY_HINT */

#ifndef HAVE_ETHTOOL_SPRINTF
__printf(2, 3) void ethtool_sprintf(u8 **data, const char *fmt, ...);
#endif /* HAVE_ETHTOOL_SPRINTF */

#ifndef HAVE_ETHTOOL_PUTS
static inline void ethtool_puts(u8 **data, const char *str)
{
	strscpy(*data, str, ETH_GSTRING_LEN);
	*data += ETH_GSTRING_LEN;
}
#endif  /* HAVE_ETHTOOL_PUTS */

#ifdef HAVE_BUILD_SKB
#ifndef HAVE_PAGE_POOL_FREE_VA
#define __bnxt_alloc_rx_frag(bp, mapping, rxr, gfp) \
	_bnxt_alloc_rx_frag(bp, mapping, gfp)
#endif
#else
#define __bnxt_alloc_rx_frag(bp, mapping, rxr, gfp) \
	_bnxt_alloc_rx_frag(bp, mapping, gfp)
#endif

#endif /* _BNXT_COMPAT_H_ */
