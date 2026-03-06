/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2017-2018 Broadcom Limited
 * Copyright (c) 2018-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BNXT_TC_H
#define BNXT_TC_H

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD

#include <net/ip_tunnels.h>

/* Structs used for storing the filter/actions of the TC cmd.
 */
struct bnxt_tc_l2_key {
	u16		src_fid;
	u8		dmac[ETH_ALEN];
	u8		smac[ETH_ALEN];
	__be16		inner_vlan_tpid;
	__be16		inner_vlan_tci;
	__be16		ether_type;
	u8		num_vlans;
	u8		dir;
#define BNXT_DIR_RX	1
#define BNXT_DIR_TX	0
};

struct bnxt_tc_l3_key {
	union {
		struct {
			struct in_addr daddr;
			struct in_addr saddr;
		} ipv4;
		struct {
			struct in6_addr daddr;
			struct in6_addr saddr;
		} ipv6;
	};
};

struct bnxt_tc_l4_key {
	u8  ip_proto;
	union {
		struct {
			__be16 sport;
			__be16 dport;
		} ports;
		struct {
			u8 type;
			u8 code;
		} icmp;
	};
};

struct bnxt_tc_tunnel_key {
	struct bnxt_tc_l2_key	l2;
	struct bnxt_tc_l3_key	l3;
	struct bnxt_tc_l4_key	l4;
	__be32			id;
};

#define bnxt_eth_addr_key_mask_invalid(eth_addr, eth_addr_mask)		\
	((is_wildcard(&(eth_addr)[0], ETH_ALEN) &&			\
	 is_wildcard(&(eth_addr)[ETH_ALEN / 2], ETH_ALEN)) ||		\
	(is_wildcard(&(eth_addr_mask)[0], ETH_ALEN) &&			\
	 is_wildcard(&(eth_addr_mask)[ETH_ALEN / 2], ETH_ALEN)))

struct bnxt_tc_actions {
	u32				flags;
#define BNXT_TC_ACTION_FLAG_FWD			BIT(0)
#define BNXT_TC_ACTION_FLAG_FWD_VXLAN		BIT(1)
#define BNXT_TC_ACTION_FLAG_PUSH_VLAN		BIT(3)
#define BNXT_TC_ACTION_FLAG_POP_VLAN		BIT(4)
#define BNXT_TC_ACTION_FLAG_DROP		BIT(5)
#define BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP	BIT(6)
#define BNXT_TC_ACTION_FLAG_TUNNEL_DECAP	BIT(7)
#define BNXT_TC_ACTION_FLAG_L2_REWRITE		BIT(8)
#define BNXT_TC_ACTION_FLAG_NAT_XLATE		BIT(9)
#define BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP_IPV4	BIT(10)
#define BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP_IPV6	BIT(11)

	u16				dst_fid;
	struct net_device		*dst_dev;
	__be16				push_vlan_tpid;
	__be16				push_vlan_tci;

	/* tunnel encap */
	struct ip_tunnel_key		tun_encap_key;
#define	PEDIT_OFFSET_SMAC_LAST_4_BYTES		0x8
	__be16				l2_rewrite_dmac[3];
	__be16				l2_rewrite_smac[3];
	struct {
		bool src_xlate;  /* true => translate src,
				  * false => translate dst
				  * Mutually exclusive, i.e cannot set both
				  */
		bool l3_is_ipv4; /* false means L3 is ipv6 */
		struct bnxt_tc_l3_key l3;
		struct bnxt_tc_l4_key l4;
	} nat;
};

struct bnxt_tc_flow {
	u32				flags;
#define BNXT_TC_FLOW_FLAGS_ETH_ADDRS		BIT(1)
#define BNXT_TC_FLOW_FLAGS_IPV4_ADDRS		BIT(2)
#define BNXT_TC_FLOW_FLAGS_IPV6_ADDRS		BIT(3)
#define BNXT_TC_FLOW_FLAGS_PORTS		BIT(4)
#define BNXT_TC_FLOW_FLAGS_ICMP			BIT(5)
#define BNXT_TC_FLOW_FLAGS_TUNL_ETH_ADDRS	BIT(6)
#define BNXT_TC_FLOW_FLAGS_TUNL_IPV4_ADDRS	BIT(7)
#define BNXT_TC_FLOW_FLAGS_TUNL_IPV6_ADDRS	BIT(8)
#define BNXT_TC_FLOW_FLAGS_TUNL_PORTS		BIT(9)
#define BNXT_TC_FLOW_FLAGS_TUNL_ID		BIT(10)
#define BNXT_TC_FLOW_FLAGS_TUNNEL	(BNXT_TC_FLOW_FLAGS_TUNL_ETH_ADDRS | \
					 BNXT_TC_FLOW_FLAGS_TUNL_IPV4_ADDRS | \
					 BNXT_TC_FLOW_FLAGS_TUNL_IPV6_ADDRS |\
					 BNXT_TC_FLOW_FLAGS_TUNL_PORTS |\
					 BNXT_TC_FLOW_FLAGS_TUNL_ID)

	/* flow applicable to pkts ingressing on this fid */
	u16				src_fid;
	struct bnxt_tc_l2_key		l2_key;
	struct bnxt_tc_l2_key		l2_mask;
	struct bnxt_tc_l3_key		l3_key;
	struct bnxt_tc_l3_key		l3_mask;
	struct bnxt_tc_l4_key		l4_key;
	struct bnxt_tc_l4_key		l4_mask;
	struct ip_tunnel_key		tun_key;
	struct ip_tunnel_key		tun_mask;

	struct bnxt_tc_actions		actions;

	/* updated stats accounting for hw-counter wrap-around */
	struct bnxt_tc_flow_stats	stats;
	/* previous snap-shot of stats */
	struct bnxt_tc_flow_stats	prev_stats;
	unsigned long			lastused; /* jiffies */
	/* for calculating delta from prev_stats and
	 * updating prev_stats atomically.
	 */
	spinlock_t			stats_lock;
};

enum bnxt_tc_tunnel_node_type {
	BNXT_TC_TUNNEL_NODE_TYPE_NONE,
	BNXT_TC_TUNNEL_NODE_TYPE_ENCAP,
	BNXT_TC_TUNNEL_NODE_TYPE_DECAP
};

/*
 * Tunnel encap/decap hash table
 * This table is used to maintain a list of flows that use
 * the same tunnel encap/decap params (ip_daddrs, vni, udp_dport)
 * and the FW returned handle.
 * A separate table is maintained for encap and decap
 */
struct bnxt_tc_tunnel_node {
	struct ip_tunnel_key		key;
	struct rhash_head		node;
	enum bnxt_tc_tunnel_node_type	tunnel_node_type;

	/* tunnel l2 info */
	struct bnxt_tc_l2_key		l2_info;

#define	INVALID_TUNNEL_HANDLE		cpu_to_le32(0xffffffff)
	/* tunnel handle returned by FW */
	__le32				tunnel_handle;

	u32				refcount;
	/* For the shared encap list maintained in neigh node */
	struct list_head		encap_list_node;
	/* A list of flows that share the encap tunnel node */
	struct list_head		common_encap_flows;
	struct bnxt_tc_neigh_node	*neigh_node;
	struct rcu_head			rcu;
};

/*
 * L2 hash table
 * The same data-struct is used for L2-flow table and L2-tunnel table.
 * The L2 part of a flow or tunnel is stored in a hash table.
 * A flow that shares the same L2 key/mask with an
 * already existing flow/tunnel must refer to it's flow handle or
 * decap_filter_id respectively.
 */
struct bnxt_tc_l2_node {
	/* hash key: first 16b of key */
#define BNXT_TC_L2_KEY_LEN			18
	struct bnxt_tc_l2_key	key;
	struct rhash_head	node;

	/* a linked list of flows that share the same l2 key */
	struct list_head	common_l2_flows;

	/* number of flows/tunnels sharing the l2 key */
	u16			refcount;

	struct rcu_head		rcu;
};

/* Track if the TC offload API is invoked on an ingress or egress device. */
enum {
	BNXT_TC_DEV_INGRESS = 1,
	BNXT_TC_DEV_EGRESS = 2
};

/* Use TC provided cookie along with the src_fid of the device on which
 * the  offload request is received . This is done to handle shared block
 * filters for 2 VFs of the same PF, since they would come with the same
 * cookie
 */
struct bnxt_tc_flow_node_key {
	/* hash key: provided by TC */
	unsigned long	cookie;
	u32		src_fid;
};

struct bnxt_tc_flow_node {
	struct bnxt_tc_flow_node_key	key;
	struct rhash_head		node;

	struct bnxt_tc_flow		flow;

	__le64				ext_flow_handle;
	__le16				flow_handle;
	__le32				flow_id;
	int				tc_dev_dir;

	/* L2 node in l2 hashtable that shares flow's l2 key */
	struct bnxt_tc_l2_node		*l2_node;
	/* for the shared_flows list maintained in l2_node */
	struct list_head		l2_list_node;

	/* tunnel encap related */
	struct bnxt_tc_tunnel_node	*encap_node;

	/* tunnel decap related */
	struct bnxt_tc_tunnel_node	*decap_node;
	/* L2 node in tunnel-l2 hashtable that shares flow's tunnel l2 key */
	struct bnxt_tc_l2_node		*decap_l2_node;
	/* for the shared_flows list maintained in tunnel decap l2_node */
	struct list_head		decap_l2_list_node;
	/* For the shared flows list maintained in tunnel encap node */
	struct list_head		encap_flow_list_node;
	/* For the shared flows list which re-add failed when get neigh event */
	struct list_head		failed_add_flow_node;

	struct rcu_head			rcu;
};

struct bnxt_tc_neigh_key {
	struct net_device	*dev;
	union {
		struct in_addr	v4;
		struct in6_addr	v6;
	} dst_ip;
	int			family;
};

struct bnxt_tc_neigh_node {
	struct bnxt_tc_neigh_key	key;
	struct rhash_head		node;
	/* An encap tunnel list which use the same neigh node */
	struct list_head		common_encap_list;
	u32				refcount;
	u8				dmac[ETH_ALEN];
	struct rcu_head			rcu;
};

struct bnxt_tf_flow_node {
	struct bnxt_tc_flow_node_key		key;
	struct rhash_head			node;
	u32					flow_id;
#ifdef HAVE_TC_CB_EGDEV
	int					tc_dev_dir;
#endif
	u16					ulp_src_fid;
	u32					dscp_remap;

	/* The below fields are used if the there is a tunnel encap
	 * action associated with the flow. These members are used to
	 * manage neighbour update events on the tunnel neighbour.
	 */
	struct bnxt_tc_tunnel_node		*encap_node;
	/* For the shared flows list maintained in tunnel encap node */
	struct list_head			encap_flow_list_node;
	/* For the shared flows list when re-add fails during neigh event */
	struct list_head			failed_add_flow_node;
	void					*mparms;

	struct rcu_head				rcu;
};

#ifdef HAVE_TC_CB_EGDEV
int bnxt_tc_setup_flower(struct bnxt *bp, u16 src_fid,
			 struct flow_cls_offload *cls_flower,
			 int tc_dev_dir);
#else
int bnxt_tc_setup_flower(struct bnxt *bp, u16 src_fid,
			 struct flow_cls_offload *cls_flower);
#endif

int bnxt_init_tc(struct bnxt *bp);
void bnxt_shutdown_tc(struct bnxt *bp);
void bnxt_tc_flow_stats_work(struct bnxt *bp);
void bnxt_tc_flush_flows(struct bnxt *bp);
#if defined(HAVE_TC_MATCHALL_FLOW_RULE) && defined(HAVE_FLOW_ACTION_POLICE)
int bnxt_tc_setup_matchall(struct bnxt *bp, u16 src_fid,
			   struct tc_cls_matchall_offload *cls_matchall);
#endif

void bnxt_tc_update_neigh_work(struct work_struct *work);
u16 bnxt_flow_get_dst_fid(struct bnxt *pf_bp, struct net_device *dev);
int bnxt_tc_resolve_ipv4_tunnel_hdrs(struct bnxt *bp,
				     struct bnxt_tc_flow_node *flow_node,
				     struct ip_tunnel_key *tun_key,
				     struct bnxt_tc_l2_key *l2_info,
				     struct bnxt_tc_neigh_key *neigh_key);
int bnxt_tc_resolve_ipv6_tunnel_hdrs(struct bnxt *bp,
				     struct bnxt_tc_flow_node *flow_node,
				     struct ip_tunnel_key *tun_key,
				     struct bnxt_tc_l2_key *l2_info,
				     struct bnxt_tc_neigh_key *neigh_key);

static inline bool bnxt_tc_flower_enabled(struct bnxt *bp)
{
	return bp->tc_info && bp->tc_info->enabled;
}

static inline void bnxt_disable_tc_flower(struct bnxt *bp)
{
	mutex_lock(&bp->tc_info->lock);
	bp->tc_info->enabled = false;
	mutex_unlock(&bp->tc_info->lock);
}

static inline void bnxt_enable_tc_flower(struct bnxt *bp)
{
	mutex_lock(&bp->tc_info->lock);
	bp->tc_info->enabled = true;
	mutex_unlock(&bp->tc_info->lock);
}

#else /* CONFIG_BNXT_FLOWER_OFFLOAD */

static inline int bnxt_init_tc(struct bnxt *bp)
{
	return 0;
}

static inline void bnxt_shutdown_tc(struct bnxt *bp)
{
}

static inline void bnxt_tc_flow_stats_work(struct bnxt *bp)
{
}

static inline void bnxt_tc_flush_flows(struct bnxt *bp)
{
}

static inline bool bnxt_tc_flower_enabled(struct bnxt *bp)
{
	return false;
}

static inline void bnxt_disable_tc_flower(struct bnxt *bp)
{
}

static inline void bnxt_enable_tc_flower(struct bnxt *bp)
{
}

#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */

#endif /* BNXT_TC_H */
