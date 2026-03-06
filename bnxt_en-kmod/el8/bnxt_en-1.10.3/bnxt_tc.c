/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2017-2018 Broadcom Limited
 * Copyright (c) 2018-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/netdevice.h>
#include <net/netevent.h>
#include <linux/inetdevice.h>
#include <linux/if_vlan.h>
#if defined(HAVE_TC_FLOW_CLS_OFFLOAD) || defined(HAVE_TC_CLS_FLOWER_OFFLOAD)
#include <net/flow_dissector.h>
#include <net/pkt_cls.h>
#include <net/tc_act/tc_gact.h>
#include <net/tc_act/tc_skbedit.h>
#include <net/tc_act/tc_mirred.h>
#include <net/tc_act/tc_vlan.h>
#include <net/tc_act/tc_pedit.h>
#ifdef HAVE_TCF_TUNNEL
#include <net/tc_act/tc_tunnel_key.h>
#endif
#include <net/vxlan.h>
#endif /* HAVE_TC_FLOW_CLS_OFFLOAD || HAVE_TC_CLS_FLOWER_OFFLOAD */

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_sriov.h"
#include "bnxt_tc.h"
#include "bnxt_vfr.h"
#include "ulp_udcc.h"
#include "bnxt_ulp_flow.h"

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD

#include "bnxt_tc_compat.h"

#define BNXT_FID_INVALID			INVALID_HW_RING_ID
#define VLAN_TCI(vid, prio)	((vid) | ((prio) << VLAN_PRIO_SHIFT))
#define BNXT_MAX_NEIGH_TIMEOUT	10

#define is_vlan_pcp_wildcarded(vlan_tci_mask)	\
	((ntohs(vlan_tci_mask) & VLAN_PRIO_MASK) == 0x0000)
#define is_vlan_pcp_exactmatch(vlan_tci_mask)	\
	((ntohs(vlan_tci_mask) & VLAN_PRIO_MASK) == VLAN_PRIO_MASK)
#define is_vlan_pcp_zero(vlan_tci)	\
	((ntohs(vlan_tci) & VLAN_PRIO_MASK) == 0x0000)
#define is_vid_exactmatch(vlan_tci_mask)	\
	((ntohs(vlan_tci_mask) & VLAN_VID_MASK) == VLAN_VID_MASK)

static bool is_wildcard(void *mask, int len);
static bool is_exactmatch(void *mask, int len);
/* Return the dst fid of the func for flow forwarding
 * For PFs: src_fid is the fid of the PF
 * For VF-reps: src_fid the fid of the VF
 */
u16 bnxt_flow_get_dst_fid(struct bnxt *pf_bp, struct net_device *dev)
{
	struct bnxt *bp;

	/* check if dev belongs to the same switch */
	if (!netdev_port_same_parent_id(pf_bp->dev, dev)) {
		netdev_info(pf_bp->dev, "dev(ifindex=%d) not on same switch\n",
			    dev->ifindex);
		return BNXT_FID_INVALID;
	}

#ifdef CONFIG_VF_REPS
	/* Is dev a VF-rep? */
	if (bnxt_dev_is_vf_rep(dev))
		return bnxt_vf_rep_get_fid(dev);
#endif

	bp = netdev_priv(dev);
	return bp->pf.fw_fid;
}

#ifdef HAVE_FLOW_OFFLOAD_H
static int bnxt_tc_parse_redir(struct bnxt *bp,
			       struct bnxt_tc_actions *actions,
			       const struct flow_action_entry *act)
{
	struct net_device *dev = act->dev;

	if (!dev) {
		netdev_info(bp->dev, "no dev in mirred action\n");
		return -EINVAL;
	}

	actions->flags |= BNXT_TC_ACTION_FLAG_FWD;
	actions->dst_dev = dev;
	return 0;
}

#else

static int bnxt_tc_parse_redir(struct bnxt *bp,
			       struct bnxt_tc_actions *actions,
			       const struct tc_action *tc_act)
{
#ifdef HAVE_TCF_MIRRED_DEV
	struct net_device *dev = tcf_mirred_dev(tc_act);

	if (!dev) {
		netdev_info(bp->dev, "no dev in mirred action");
		return -EINVAL;
	}
#else
	int ifindex = tcf_mirred_ifindex(tc_act);
	struct net_device *dev;

	dev = __dev_get_by_index(dev_net(bp->dev), ifindex);
	if (!dev) {
		netdev_info(bp->dev, "no dev for ifindex=%d", ifindex);
		return -EINVAL;
	}
#endif

	actions->flags |= BNXT_TC_ACTION_FLAG_FWD;
	actions->dst_dev = dev;
	return 0;
}
#endif

#ifdef HAVE_FLOW_OFFLOAD_H
static int bnxt_tc_parse_vlan(struct bnxt *bp,
			      struct bnxt_tc_actions *actions,
			      const struct flow_action_entry *act)
{
	switch (act->id) {
	case FLOW_ACTION_VLAN_POP:
		actions->flags |= BNXT_TC_ACTION_FLAG_POP_VLAN;
		break;
	case FLOW_ACTION_VLAN_PUSH:
		actions->flags |= BNXT_TC_ACTION_FLAG_PUSH_VLAN;
		actions->push_vlan_tci = htons(act->vlan.vid);
		actions->push_vlan_tpid = act->vlan.proto;
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

#else

static int bnxt_tc_parse_vlan(struct bnxt *bp,
			      struct bnxt_tc_actions *actions,
			      const struct tc_action *tc_act)
{
	switch (tcf_vlan_action(tc_act)) {
	case TCA_VLAN_ACT_POP:
		actions->flags |= BNXT_TC_ACTION_FLAG_POP_VLAN;
		break;
	case TCA_VLAN_ACT_PUSH:
		actions->flags |= BNXT_TC_ACTION_FLAG_PUSH_VLAN;
		actions->push_vlan_tci = htons(tcf_vlan_push_vid(tc_act));
		actions->push_vlan_tpid = tcf_vlan_push_proto(tc_act);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}
#endif

#ifdef HAVE_FLOW_OFFLOAD_H
static int bnxt_tc_parse_tunnel_set(struct bnxt *bp,
				    struct bnxt_tc_actions *actions,
				    const struct flow_action_entry *act)
{
	const struct ip_tunnel_info *tun_info = act->tunnel;
	const struct ip_tunnel_key *tun_key = &tun_info->key;

#else
static int bnxt_tc_parse_tunnel_set(struct bnxt *bp,
				    struct bnxt_tc_actions *actions,
				    const struct tc_action *tc_act)
{
	struct ip_tunnel_info *tun_info = tcf_tunnel_info(tc_act);
	struct ip_tunnel_key *tun_key = &tun_info->key;
#endif

	switch (ip_tunnel_info_af(tun_info)) {
	case AF_INET:
		actions->flags |= BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP_IPV4;
		break;
	case AF_INET6:
		actions->flags |= BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP_IPV6;
		break;
	default:
		return -EOPNOTSUPP;
	}

	actions->tun_encap_key = *tun_key;
	actions->flags |= BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP;
	return 0;
}


/* Key & Mask from the stack comes unaligned in multiple iterations of 4 bytes
 * each(u32).
 * This routine consolidates such multiple unaligned values into one
 * field each for Key & Mask (for src and dst macs separately)
 * For example,
 *			Mask/Key	Offset	Iteration
 *			==========	======	=========
 *	dst mac		0xffffffff	0	1
 *	dst mac		0x0000ffff	4	2
 *
 *	src mac		0xffff0000	4	1
 *	src mac		0xffffffff	8	2
 *
 * The above combination coming from the stack will be consolidated as
 *			Mask/Key
 *			==============
 *	src mac:	0xffffffffffff
 *	dst mac:	0xffffffffffff
 */
static void bnxt_set_l2_key_mask(u32 part_key, u32 part_mask,
				 u8 *actual_key, u8 *actual_mask)
{
	u32 key = get_unaligned((u32 *)actual_key);
	u32 mask = get_unaligned((u32 *)actual_mask);

	part_key &= part_mask;
	part_key |= key & ~part_mask;

	put_unaligned(mask | part_mask, (u32 *)actual_mask);
	put_unaligned(part_key, (u32 *)actual_key);
}

static int
bnxt_fill_l2_rewrite_fields(struct bnxt_tc_actions *actions,
			    u16 *eth_addr, u16 *eth_addr_mask)
{
	u16 *p;
	int j;

	if (unlikely(bnxt_eth_addr_key_mask_invalid(eth_addr, eth_addr_mask)))
		return -EINVAL;

	if (!is_wildcard(&eth_addr_mask[0], ETH_ALEN)) {
		if (!is_exactmatch(&eth_addr_mask[0], ETH_ALEN))
			return -EINVAL;
		/* FW expects dmac to be in u16 array format */
		p = eth_addr;
		for (j = 0; j < 3; j++)
			actions->l2_rewrite_dmac[j] = cpu_to_be16(*(p + j));
	}

	if (!is_wildcard(&eth_addr_mask[ETH_ALEN / 2], ETH_ALEN)) {
		if (!is_exactmatch(&eth_addr_mask[ETH_ALEN / 2], ETH_ALEN))
			return -EINVAL;
		/* FW expects smac to be in u16 array format */
		p = &eth_addr[ETH_ALEN / 2];
		for (j = 0; j < 3; j++)
			actions->l2_rewrite_smac[j] = cpu_to_be16(*(p + j));
	}

	return 0;
}

#ifdef HAVE_FLOW_OFFLOAD_H
static int
bnxt_tc_parse_pedit(struct bnxt *bp, struct bnxt_tc_actions *actions,
		    struct flow_action_entry *act, int act_idx, u8 *eth_addr,
		    u8 *eth_addr_mask)
{
	size_t offset_of_ip6_daddr = offsetof(struct ipv6hdr, daddr);
	size_t offset_of_ip6_saddr = offsetof(struct ipv6hdr, saddr);
	u32 mask, val, offset, idx;
	u8 htype;

	offset = act->mangle.offset;
	htype = act->mangle.htype;
	mask = ~act->mangle.mask;
	val = act->mangle.val;

	switch (htype) {
	case FLOW_ACT_MANGLE_HDR_TYPE_ETH:
		if (offset > PEDIT_OFFSET_SMAC_LAST_4_BYTES) {
			netdev_err(bp->dev,
				   "%s: eth_hdr: Invalid pedit field\n",
				   __func__);
			return -EINVAL;
		}
		actions->flags |= BNXT_TC_ACTION_FLAG_L2_REWRITE;

		bnxt_set_l2_key_mask(val, mask, &eth_addr[offset],
				     &eth_addr_mask[offset]);
		break;
	case FLOW_ACT_MANGLE_HDR_TYPE_IP4:
		actions->flags |= BNXT_TC_ACTION_FLAG_NAT_XLATE;
		actions->nat.l3_is_ipv4 = true;
		if (offset ==  offsetof(struct iphdr, saddr)) {
			actions->nat.src_xlate = true;
			actions->nat.l3.ipv4.saddr.s_addr = htonl(val);
		}
		else if (offset ==  offsetof(struct iphdr, daddr)) {
			actions->nat.src_xlate = false;
			actions->nat.l3.ipv4.daddr.s_addr = htonl(val);
		} else {
			netdev_err(bp->dev,
				   "%s: IPv4_hdr: Invalid pedit field\n",
				   __func__);
			return -EINVAL;
		}

		netdev_dbg(bp->dev, "nat.src_xlate = %d src IP: %pI4 dst ip : %pI4\n",
			   actions->nat.src_xlate, &actions->nat.l3.ipv4.saddr,
			   &actions->nat.l3.ipv4.daddr);
		break;

	case FLOW_ACT_MANGLE_HDR_TYPE_IP6:
		actions->flags |= BNXT_TC_ACTION_FLAG_NAT_XLATE;
		actions->nat.l3_is_ipv4 = false;
		if (offset >= offsetof(struct ipv6hdr, saddr) &&
		    offset < offset_of_ip6_daddr) {
			/* 16 byte IPv6 address comes in 4 iterations of
			 * 4byte chunks each
			 */
			actions->nat.src_xlate = true;
			idx = (offset - offset_of_ip6_saddr) / 4;
			/* First 4bytes will be copied to idx 0 and so on */
			actions->nat.l3.ipv6.saddr.s6_addr32[idx] = htonl(val);
		} else if (offset >= offset_of_ip6_daddr &&
			   offset < offset_of_ip6_daddr + 16) {
			actions->nat.src_xlate = false;
			idx = (offset - offset_of_ip6_daddr) / 4;
			actions->nat.l3.ipv6.saddr.s6_addr32[idx] = htonl(val);
		} else {
			netdev_err(bp->dev,
				   "%s: IPv6_hdr: Invalid pedit field\n",
				   __func__);
			return -EINVAL;
		}
		break;
	case FLOW_ACT_MANGLE_HDR_TYPE_TCP:
	case FLOW_ACT_MANGLE_HDR_TYPE_UDP:
		/* HW does not support L4 rewrite alone without L3
		 * rewrite
		 */
		if (!(actions->flags & BNXT_TC_ACTION_FLAG_NAT_XLATE)) {
			netdev_err(bp->dev,
				   "Need to specify L3 rewrite as well\n");
			return -EINVAL;
		}
		if (actions->nat.src_xlate)
			actions->nat.l4.ports.sport = htons(val);
		else
			actions->nat.l4.ports.dport = htons(val);
		netdev_dbg(bp->dev, "actions->nat.sport = %d dport = %d\n",
			   actions->nat.l4.ports.sport,
			   actions->nat.l4.ports.dport);
		break;
	default:
		netdev_err(bp->dev, "%s: Unsupported pedit hdr type\n",
			   __func__);
		return -EINVAL;
	}
	return 0;
}

static int bnxt_tc_parse_actions(struct bnxt *bp,
				 struct bnxt_tc_actions *actions,
				 struct flow_action *flow_action,
				 struct netlink_ext_ack *extack)
{
	/* Used to store the L2 rewrite mask for dmac (6 bytes) followed by
	 * smac (6 bytes) if rewrite of both is specified, otherwise either
	 * dmac or smac
	 */
	u16 eth_addr_mask[ETH_ALEN] = { 0 };
	/* Used to store the L2 rewrite key for dmac (6 bytes) followed by
	 * smac (6 bytes) if rewrite of both is specified, otherwise either
	 * dmac or smac
	 */
	u16 eth_addr[ETH_ALEN] = { 0 };
	struct flow_action_entry *act;
	int i, rc;

	if (!flow_action_has_entries(flow_action)) {
		netdev_info(bp->dev, "no actions\n");
		return -EINVAL;
	}

	if (!flow_action_basic_hw_stats_check(flow_action, extack))
		return -EOPNOTSUPP;

	flow_action_for_each(i, act, flow_action) {
		switch (act->id) {
		case FLOW_ACTION_DROP:
			actions->flags |= BNXT_TC_ACTION_FLAG_DROP;
			return 0; /* don't bother with other actions */
		case FLOW_ACTION_REDIRECT:
			rc = bnxt_tc_parse_redir(bp, actions, act);
			if (rc)
				return rc;
			break;
		case FLOW_ACTION_VLAN_POP:
		case FLOW_ACTION_VLAN_PUSH:
		case FLOW_ACTION_VLAN_MANGLE:
			rc = bnxt_tc_parse_vlan(bp, actions, act);
			if (rc)
				return rc;
			break;
		case FLOW_ACTION_TUNNEL_ENCAP:
			rc = bnxt_tc_parse_tunnel_set(bp, actions, act);
			if (rc)
				return rc;
			break;
		case FLOW_ACTION_TUNNEL_DECAP:
			actions->flags |= BNXT_TC_ACTION_FLAG_TUNNEL_DECAP;
			break;
		/* Packet edit: L2 rewrite, NAT, NAPT */
		case FLOW_ACTION_MANGLE:
			rc = bnxt_tc_parse_pedit(bp, actions, act, i,
						 (u8 *)eth_addr,
						 (u8 *)eth_addr_mask);
			if (rc)
				return rc;
			break;
		default:
			break;
		}
	}

	if (actions->flags & BNXT_TC_ACTION_FLAG_L2_REWRITE) {
		rc = bnxt_fill_l2_rewrite_fields(actions, eth_addr,
						 eth_addr_mask);
		if (rc)
			return rc;
	}

	if (actions->flags & BNXT_TC_ACTION_FLAG_FWD) {
		if (actions->flags & BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP) {
			/* dst_fid is PF's fid */
			actions->dst_fid = bp->pf.fw_fid;
		} else {
			/* find the FID from dst_dev */
			actions->dst_fid =
				bnxt_flow_get_dst_fid(bp, actions->dst_dev);
			if (actions->dst_fid == BNXT_FID_INVALID)
				return -EINVAL;
		}
	}

	return 0;
}

#else

static int
bnxt_tc_parse_pedit(struct bnxt *bp, const struct tc_action *tc_act,
		    struct bnxt_tc_actions *actions,
		    u8 *eth_addr, u8 *eth_addr_mask)
{
	size_t offset_of_ip6_daddr = offsetof(struct ipv6hdr, daddr);
	size_t offset_of_ip6_saddr = offsetof(struct ipv6hdr, saddr);
	u32 mask, val, offset, idx;
	u8 cmd, htype;
	int nkeys, j;

	nkeys = tcf_pedit_nkeys(tc_act);
	for (j = 0 ; j < nkeys; j++) {
		cmd = tcf_pedit_cmd(tc_act, j);
		/* L2 rewrite comes as TCA_PEDIT_KEY_EX_CMD_SET type from TC.
		 * Return error, if the TC pedit cmd is not of this type.
		 */
		if (cmd != TCA_PEDIT_KEY_EX_CMD_SET) {
			netdev_err(bp->dev, "%s: pedit cmd not supported\n",
				   __func__);
			return -EINVAL;
		}

		offset = tcf_pedit_offset(tc_act, j);
		htype = tcf_pedit_htype(tc_act, j);
		mask = ~tcf_pedit_mask(tc_act, j);
		val = tcf_pedit_val(tc_act, j);

		switch (htype) {
		case TCA_PEDIT_KEY_EX_HDR_TYPE_ETH:
			if (offset > PEDIT_OFFSET_SMAC_LAST_4_BYTES) {
				netdev_err(bp->dev,
					   "%s: eth_hdr: Invalid pedit field\n",
					   __func__);
				return -EINVAL;
			}
			actions->flags |=
				BNXT_TC_ACTION_FLAG_L2_REWRITE;

			bnxt_set_l2_key_mask(val, mask,
					     &eth_addr[offset],
					     &eth_addr_mask[offset]);
			break;

		case TCA_PEDIT_KEY_EX_HDR_TYPE_IP4:
			actions->flags |= BNXT_TC_ACTION_FLAG_NAT_XLATE;
			actions->nat.l3_is_ipv4 = true;
			if (offset == offsetof(struct iphdr, saddr)) {
				actions->nat.src_xlate = true;
				actions->nat.l3.ipv4.saddr.s_addr = htonl(val);
			} else if (offset == offsetof(struct iphdr, daddr)) {
				actions->nat.src_xlate = false;
				actions->nat.l3.ipv4.daddr.s_addr = htonl(val);
			} else {
				netdev_err(bp->dev,
					   "%s: IPv4_hdr: Invalid pedit field\n",
					   __func__);
				return -EINVAL;
			}
			break;

		case TCA_PEDIT_KEY_EX_HDR_TYPE_IP6:
			actions->flags |= BNXT_TC_ACTION_FLAG_NAT_XLATE;
			actions->nat.l3_is_ipv4 = false;

			if (offset >= offsetof(struct ipv6hdr, saddr) &&
			    offset < offset_of_ip6_daddr) {
				/* 16 byte IPv6 address comes in 4 iterations of
				 * 4byte chunks each
				 */
				actions->nat.src_xlate = true;
				idx = (offset - offset_of_ip6_saddr) / 4;
				/* First 4bytes will be copied to idx 0 and so on */
				actions->nat.l3.ipv6.saddr.s6_addr32[idx] =
								htonl(val);
			} else if (offset >= offset_of_ip6_daddr &&
				   offset < offset_of_ip6_daddr + 16) {
				actions->nat.src_xlate = false;
				idx = (offset - offset_of_ip6_daddr) / 4;
				actions->nat.l3.ipv6.daddr.s6_addr32[idx] =
								htonl(val);
			} else {
				netdev_err(bp->dev,
					   "%s: IPv6_hdr: Invalid pedit field\n",
					   __func__);
				return -EINVAL;
			}
			break;

		case TCA_PEDIT_KEY_EX_HDR_TYPE_TCP:
		case TCA_PEDIT_KEY_EX_HDR_TYPE_UDP:
			/* HW does not support L4 rewrite alone without L3
			 * rewrite
			 */
			if (!(actions->flags & BNXT_TC_ACTION_FLAG_NAT_XLATE)) {
				netdev_err(bp->dev,
					   "Need to specify L3 rewrite as well\n");
				return -EINVAL;
			}
			if (actions->nat.src_xlate)
				actions->nat.l4.ports.sport = htons(val);
			else
				actions->nat.l4.ports.dport = htons(val);
			break;
		/* Return, if the packet edit is not for L2/L3/L4 */
		default:
			netdev_err(bp->dev, "%s: Unsupported pedit hdr type\n",
				   __func__);
			return -EINVAL;
		}
	}

	return 0;
}

static int bnxt_tc_parse_actions(struct bnxt *bp,
				 struct bnxt_tc_actions *actions,
				 struct tcf_exts *tc_exts)
{
	u16 eth_addr_mask[ETH_ALEN] = { 0 };
	u16 eth_addr[ETH_ALEN] = { 0 };
	const struct tc_action *tc_act;
#ifndef HAVE_TC_EXTS_FOR_ACTION
	LIST_HEAD(tc_actions);
	int rc;
#else
	int i, rc;
#endif

	if (!tcf_exts_has_actions(tc_exts)) {
		netdev_info(bp->dev, "no actions");
		return -EINVAL;
	}

#ifndef HAVE_TC_EXTS_FOR_ACTION
	tcf_exts_to_list(tc_exts, &tc_actions);
	list_for_each_entry(tc_act, &tc_actions, list) {
#else
	tcf_exts_for_each_action(i, tc_act, tc_exts) {
#endif
		/* Drop action */
		if (is_tcf_gact_shot(tc_act)) {
			actions->flags |= BNXT_TC_ACTION_FLAG_DROP;
			return 0; /* don't bother with other actions */
		}

		/* Redirect action */
		if (is_tcf_mirred_egress_redirect(tc_act)) {
			rc = bnxt_tc_parse_redir(bp, actions, tc_act);
			if (rc)
				return rc;
			continue;
		}

		/* Push/pop VLAN */
		if (is_tcf_vlan(tc_act)) {
			rc = bnxt_tc_parse_vlan(bp, actions, tc_act);
			if (rc)
				return rc;
			continue;
		}

		/* Tunnel encap */
		if (is_tcf_tunnel_set(tc_act)) {
			rc = bnxt_tc_parse_tunnel_set(bp, actions, tc_act);
			if (rc)
				return rc;
			continue;
		}

		/* Tunnel decap */
		if (is_tcf_tunnel_release(tc_act)) {
			actions->flags |= BNXT_TC_ACTION_FLAG_TUNNEL_DECAP;
			continue;
		}

		/* Packet edit: L2 rewrite, NAT, NAPT */
		if (is_tcf_pedit(tc_act)) {
			rc = bnxt_tc_parse_pedit(bp, tc_act, actions,
						 (u8 *)eth_addr,
						 (u8 *)eth_addr_mask);
			if (rc)
				return rc;

			if (actions->flags & BNXT_TC_ACTION_FLAG_L2_REWRITE) {
				rc = bnxt_fill_l2_rewrite_fields(actions,
								 eth_addr,
								 eth_addr_mask);
				if (rc)
					return rc;
			}
		}
	}

	if (actions->flags & BNXT_TC_ACTION_FLAG_FWD) {
		if (actions->flags & BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP) {
			/* dst_fid is PF's fid */
			actions->dst_fid = bp->pf.fw_fid;
		} else {
			/* find the FID from dst_dev */
			actions->dst_fid =
				bnxt_flow_get_dst_fid(bp, actions->dst_dev);
			if (actions->dst_fid == BNXT_FID_INVALID)
				return -EINVAL;
		}
	}

	return 0;
}
#endif

static int bnxt_tc_parse_flow(struct bnxt *bp,
			      struct flow_cls_offload *tc_flow_cmd,
			      struct bnxt_tc_flow *flow)
{
	struct flow_rule *rule = flow_cls_offload_flow_rule(tc_flow_cmd);
#ifdef HAVE_TC_CAN_OFFLOAD_EXTACK
	struct netlink_ext_ack *extack = tc_flow_cmd->common.extack;
#else
	struct netlink_ext_ack *extack = NULL;
#endif
	struct flow_dissector *dissector = rule->match.dissector;

	/* KEY_CONTROL and KEY_BASIC are needed for forming a meaningful key */
	if ((dissector->used_keys & BIT_ULL(FLOW_DISSECTOR_KEY_CONTROL)) == 0 ||
	    (dissector->used_keys & BIT_ULL(FLOW_DISSECTOR_KEY_BASIC)) == 0) {
		netdev_info(bp->dev, "cannot form TC key: used_keys = 0x%llx\n",
			    (u64)dissector->used_keys);
		return -EOPNOTSUPP;
	}

	if (flow_rule_match_has_control_flags(rule, extack))
		return -EOPNOTSUPP;

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_BASIC)) {
		struct flow_match_basic match;

		flow_rule_match_basic(rule, &match);
		flow->l2_key.ether_type = match.key->n_proto;
		flow->l2_mask.ether_type = match.mask->n_proto;

		if (match.key->n_proto == htons(ETH_P_IP) ||
		    match.key->n_proto == htons(ETH_P_IPV6)) {
			flow->l4_key.ip_proto = match.key->ip_proto;
			flow->l4_mask.ip_proto = match.mask->ip_proto;
		}
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ETH_ADDRS)) {
		struct flow_match_eth_addrs match;

		flow_rule_match_eth_addrs(rule, &match);
		flow->flags |= BNXT_TC_FLOW_FLAGS_ETH_ADDRS;
		ether_addr_copy(flow->l2_key.dmac, match.key->dst);
		ether_addr_copy(flow->l2_mask.dmac, match.mask->dst);
		ether_addr_copy(flow->l2_key.smac, match.key->src);
		ether_addr_copy(flow->l2_mask.smac, match.mask->src);
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_VLAN)) {
		struct flow_match_vlan match;

		flow_rule_match_vlan(rule, &match);
		flow->l2_key.inner_vlan_tci =
			cpu_to_be16(VLAN_TCI(match.key->vlan_id,
					     match.key->vlan_priority));
		flow->l2_mask.inner_vlan_tci =
			cpu_to_be16((VLAN_TCI(match.mask->vlan_id,
					      match.mask->vlan_priority)));
		flow->l2_key.inner_vlan_tpid = htons(ETH_P_8021Q);
		flow->l2_mask.inner_vlan_tpid = htons(0xffff);
		flow->l2_key.num_vlans = 1;
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_CONTROL)) {
		struct flow_match_control match;
		u16 addr_type;

		flow_rule_match_control(rule, &match);
		addr_type = match.key->addr_type;
		if (addr_type == FLOW_DISSECTOR_KEY_IPV4_ADDRS) {
			struct flow_match_ipv4_addrs match;

			flow_rule_match_ipv4_addrs(rule, &match);
			flow->flags |= BNXT_TC_FLOW_FLAGS_IPV4_ADDRS;
			flow->l3_key.ipv4.daddr.s_addr = match.key->dst;
			flow->l3_mask.ipv4.daddr.s_addr = match.mask->dst;
			flow->l3_key.ipv4.saddr.s_addr = match.key->src;
			flow->l3_mask.ipv4.saddr.s_addr = match.mask->src;
		} else if (addr_type == FLOW_DISSECTOR_KEY_IPV6_ADDRS) {
			struct flow_match_ipv6_addrs match;

			flow_rule_match_ipv6_addrs(rule, &match);
			flow->flags |= BNXT_TC_FLOW_FLAGS_IPV6_ADDRS;
			flow->l3_key.ipv6.daddr = match.key->dst;
			flow->l3_mask.ipv6.daddr = match.mask->dst;
			flow->l3_key.ipv6.saddr = match.key->src;
			flow->l3_mask.ipv6.saddr = match.mask->src;
		}
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_PORTS)) {
		struct flow_match_ports match;

		flow_rule_match_ports(rule, &match);
		flow->flags |= BNXT_TC_FLOW_FLAGS_PORTS;
		flow->l4_key.ports.dport = match.key->dst;
		flow->l4_mask.ports.dport = match.mask->dst;
		flow->l4_key.ports.sport = match.key->src;
		flow->l4_mask.ports.sport = match.mask->src;
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ICMP)) {
		struct flow_match_icmp match;

		flow_rule_match_icmp(rule, &match);
		flow->flags |= BNXT_TC_FLOW_FLAGS_ICMP;
		flow->l4_key.icmp.type = match.key->type;
		flow->l4_key.icmp.code = match.key->code;
		flow->l4_mask.icmp.type = match.mask->type;
		flow->l4_mask.icmp.code = match.mask->code;
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ENC_CONTROL)) {
		struct flow_match_control match;
		u16 addr_type;

		flow_rule_match_enc_control(rule, &match);
		addr_type = match.key->addr_type;

		if (addr_type == FLOW_DISSECTOR_KEY_IPV4_ADDRS) {
			struct flow_match_ipv4_addrs match;

			flow_rule_match_enc_ipv4_addrs(rule, &match);
			flow->flags |= BNXT_TC_FLOW_FLAGS_TUNL_IPV4_ADDRS;
			flow->tun_key.u.ipv4.dst = match.key->dst;
			flow->tun_mask.u.ipv4.dst = match.mask->dst;
			flow->tun_key.u.ipv4.src = match.key->src;
			flow->tun_mask.u.ipv4.src = match.mask->src;
		} else if (addr_type == FLOW_DISSECTOR_KEY_IPV6_ADDRS) {
			struct flow_match_ipv6_addrs match;

			flow_rule_match_enc_ipv6_addrs(rule, &match);
			flow->flags |= BNXT_TC_FLOW_FLAGS_TUNL_IPV6_ADDRS;
			flow->tun_key.u.ipv6.dst = match.key->dst;
			flow->tun_mask.u.ipv6.dst = match.mask->dst;
			flow->tun_key.u.ipv6.src = match.key->src;
			flow->tun_mask.u.ipv6.src = match.mask->src;
		}
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ENC_KEYID)) {
		struct flow_match_enc_keyid match;

		flow_rule_match_enc_keyid(rule, &match);
		flow->flags |= BNXT_TC_FLOW_FLAGS_TUNL_ID;
		flow->tun_key.tun_id = key32_to_tunnel_id(match.key->keyid);
		flow->tun_mask.tun_id = key32_to_tunnel_id(match.mask->keyid);
	}

	if (flow_rule_match_key(rule, FLOW_DISSECTOR_KEY_ENC_PORTS)) {
		struct flow_match_ports match;

		flow_rule_match_enc_ports(rule, &match);
		flow->flags |= BNXT_TC_FLOW_FLAGS_TUNL_PORTS;
		flow->tun_key.tp_dst = match.key->dst;
		flow->tun_mask.tp_dst = match.mask->dst;
		flow->tun_key.tp_src = match.key->src;
		flow->tun_mask.tp_src = match.mask->src;
	}

#ifdef HAVE_FLOW_OFFLOAD_H
	return bnxt_tc_parse_actions(bp, &flow->actions, &rule->action,
				     tc_flow_cmd->common.extack);
#else
	return bnxt_tc_parse_actions(bp, &flow->actions, tc_flow_cmd->exts);
#endif
}

static int bnxt_hwrm_cfa_flow_free(struct bnxt *bp,
				   struct bnxt_tc_flow_node *flow_node)
{
	struct hwrm_cfa_flow_free_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_CFA_FLOW_FREE);
	if (!rc) {
		if (bp->fw_cap & BNXT_FW_CAP_OVS_64BIT_HANDLE)
			req->ext_flow_handle = flow_node->ext_flow_handle;
		else
			req->flow_handle = flow_node->flow_handle;

		rc = hwrm_req_send(bp, req);
	}
	if (rc)
		netdev_info(bp->dev, "%s: Error rc=%d\n", __func__, rc);

	return rc;
}

static int ipv6_mask_len(struct in6_addr *mask)
{
	int mask_len = 0, i;

	for (i = 0; i < 4; i++)
		mask_len += inet_mask_len(mask->s6_addr32[i]);

	return mask_len;
}

static bool is_wildcard(void *mask, int len)
{
	const u8 *p = mask;
	int i;

	for (i = 0; i < len; i++) {
		if (p[i] != 0)
			return false;
	}
	return true;
}

static bool is_exactmatch(void *mask, int len)
{
	const u8 *p = mask;
	int i;

	for (i = 0; i < len; i++)
		if (p[i] != 0xff)
			return false;

	return true;
}

static bool is_vlan_tci_allowed(__be16  vlan_tci_mask,
				__be16  vlan_tci)
{
	/* VLAN priority must be either exactly zero or fully wildcarded and
	 * VLAN id must be exact match.
	 */
	if (is_vid_exactmatch(vlan_tci_mask) &&
	    ((is_vlan_pcp_exactmatch(vlan_tci_mask) &&
	      is_vlan_pcp_zero(vlan_tci)) ||
	     is_vlan_pcp_wildcarded(vlan_tci_mask)))
		return true;

	return false;
}

static bool bits_set(void *key, int len)
{
	const u8 *p = key;
	int i;

	for (i = 0; i < len; i++)
		if (p[i] != 0)
			return true;

	return false;
}

static int bnxt_hwrm_cfa_flow_alloc(struct bnxt *bp, struct bnxt_tc_flow *flow,
				    __le16 ref_flow_handle,
				    __le32 tunnel_handle,
				    struct bnxt_tc_flow_node *flow_node)
{
	struct bnxt_tc_actions *actions = &flow->actions;
	struct bnxt_tc_l3_key *l3_mask = &flow->l3_mask;
	struct bnxt_tc_l3_key *l3_key = &flow->l3_key;
	struct hwrm_cfa_flow_alloc_output *resp;
	struct hwrm_cfa_flow_alloc_input *req;
	u16 flow_flags = 0, action_flags = 0;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_CFA_FLOW_ALLOC);
	if (rc)
		return rc;

	req->src_fid = cpu_to_le16(flow->src_fid);
	req->ref_flow_handle = ref_flow_handle;

	if (actions->flags & BNXT_TC_ACTION_FLAG_L2_REWRITE) {
		memcpy(req->l2_rewrite_dmac, actions->l2_rewrite_dmac,
		       ETH_ALEN);
		memcpy(req->l2_rewrite_smac, actions->l2_rewrite_smac,
		       ETH_ALEN);
		action_flags |=
			CFA_FLOW_ALLOC_REQ_ACTION_FLAGS_L2_HEADER_REWRITE;
	}

	if (actions->flags & BNXT_TC_ACTION_FLAG_NAT_XLATE) {
		if (actions->nat.l3_is_ipv4) {
			action_flags |=
				CFA_FLOW_ALLOC_REQ_ACTION_FLAGS_NAT_IPV4_ADDRESS;

			if (actions->nat.src_xlate) {
				action_flags |=
					CFA_FLOW_ALLOC_REQ_ACTION_FLAGS_NAT_SRC;
				/* L3 source rewrite */
				req->nat_ip_address[0] =
					actions->nat.l3.ipv4.saddr.s_addr;
				/* L4 source port */
				if (actions->nat.l4.ports.sport)
					req->nat_port =
						actions->nat.l4.ports.sport;
			} else {
				action_flags |=
					CFA_FLOW_ALLOC_REQ_ACTION_FLAGS_NAT_DEST;
				/* L3 destination rewrite */
				req->nat_ip_address[0] =
					actions->nat.l3.ipv4.daddr.s_addr;
				/* L4 destination port */
				if (actions->nat.l4.ports.dport)
					req->nat_port =
						actions->nat.l4.ports.dport;
			}
			netdev_dbg(bp->dev,
				   "req.nat_ip_address: %pI4 src_xlate: %d req.nat_port: %x\n",
				   req->nat_ip_address, actions->nat.src_xlate,
				   req->nat_port);
		} else {
			if (actions->nat.src_xlate) {
				action_flags |=
					CFA_FLOW_ALLOC_REQ_ACTION_FLAGS_NAT_SRC;
				/* L3 source rewrite */
				memcpy(req->nat_ip_address,
				       actions->nat.l3.ipv6.saddr.s6_addr32,
				       sizeof(req->nat_ip_address));
				/* L4 source port */
				if (actions->nat.l4.ports.sport)
					req->nat_port =
						actions->nat.l4.ports.sport;
			} else {
				action_flags |=
					CFA_FLOW_ALLOC_REQ_ACTION_FLAGS_NAT_DEST;
				/* L3 destination rewrite */
				memcpy(req->nat_ip_address,
				       actions->nat.l3.ipv6.daddr.s6_addr32,
				       sizeof(req->nat_ip_address));
				/* L4 destination port */
				if (actions->nat.l4.ports.dport)
					req->nat_port =
						actions->nat.l4.ports.dport;
			}
			netdev_dbg(bp->dev,
                                   "req.nat_ip_address: %pI6 src_xlate: %d req.nat_port: %x\n",
				   req->nat_ip_address, actions->nat.src_xlate,
				   req->nat_port);
		}
	}

	if (actions->flags & BNXT_TC_ACTION_FLAG_TUNNEL_DECAP ||
	    actions->flags & BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP) {
		req->tunnel_handle = tunnel_handle;
		flow_flags |= CFA_FLOW_ALLOC_REQ_FLAGS_TUNNEL;
		action_flags |= CFA_FLOW_ALLOC_REQ_ACTION_FLAGS_TUNNEL;
	}

	req->ethertype = flow->l2_key.ether_type;
	req->ip_proto = flow->l4_key.ip_proto;

	if (flow->flags & BNXT_TC_FLOW_FLAGS_ETH_ADDRS) {
		memcpy(req->dmac, flow->l2_key.dmac, ETH_ALEN);
		memcpy(req->smac, flow->l2_key.smac, ETH_ALEN);
	}

	if (flow->l2_key.num_vlans > 0) {
		flow_flags |= CFA_FLOW_ALLOC_REQ_FLAGS_NUM_VLAN_ONE;
		/* FW expects the inner_vlan_tci value to be set
		 * in outer_vlan_tci when num_vlans is 1 (which is
		 * always the case in TC.)
		 */
		req->outer_vlan_tci = flow->l2_key.inner_vlan_tci;
	}

	/* If all IP and L4 fields are wildcarded then this is an L2 flow */
	if (is_wildcard(l3_mask, sizeof(*l3_mask)) &&
	    is_wildcard(&flow->l4_mask, sizeof(flow->l4_mask))) {
		flow_flags |= CFA_FLOW_ALLOC_REQ_FLAGS_FLOWTYPE_L2;
	} else {
		flow_flags |= flow->l2_key.ether_type == htons(ETH_P_IP) ?
				CFA_FLOW_ALLOC_REQ_FLAGS_FLOWTYPE_IPV4 :
				CFA_FLOW_ALLOC_REQ_FLAGS_FLOWTYPE_IPV6;

		if (flow->flags & BNXT_TC_FLOW_FLAGS_IPV4_ADDRS) {
			req->ip_dst[0] = l3_key->ipv4.daddr.s_addr;
			req->ip_dst_mask_len =
				inet_mask_len(l3_mask->ipv4.daddr.s_addr);
			req->ip_src[0] = l3_key->ipv4.saddr.s_addr;
			req->ip_src_mask_len =
				inet_mask_len(l3_mask->ipv4.saddr.s_addr);
		} else if (flow->flags & BNXT_TC_FLOW_FLAGS_IPV6_ADDRS) {
			memcpy(req->ip_dst, l3_key->ipv6.daddr.s6_addr32,
			       sizeof(req->ip_dst));
			req->ip_dst_mask_len =
					ipv6_mask_len(&l3_mask->ipv6.daddr);
			memcpy(req->ip_src, l3_key->ipv6.saddr.s6_addr32,
			       sizeof(req->ip_src));
			req->ip_src_mask_len =
					ipv6_mask_len(&l3_mask->ipv6.saddr);
		}
	}

	if (flow->flags & BNXT_TC_FLOW_FLAGS_PORTS) {
		req->l4_src_port = flow->l4_key.ports.sport;
		req->l4_src_port_mask = flow->l4_mask.ports.sport;
		req->l4_dst_port = flow->l4_key.ports.dport;
		req->l4_dst_port_mask = flow->l4_mask.ports.dport;
	} else if (flow->flags & BNXT_TC_FLOW_FLAGS_ICMP) {
		/* l4 ports serve as type/code when ip_proto is ICMP */
		req->l4_src_port = htons(flow->l4_key.icmp.type);
		req->l4_src_port_mask = htons(flow->l4_mask.icmp.type);
		req->l4_dst_port = htons(flow->l4_key.icmp.code);
		req->l4_dst_port_mask = htons(flow->l4_mask.icmp.code);
	}
	req->flags = cpu_to_le16(flow_flags);

	if (actions->flags & BNXT_TC_ACTION_FLAG_DROP) {
		action_flags |= CFA_FLOW_ALLOC_REQ_ACTION_FLAGS_DROP;
	} else {
		if (actions->flags & BNXT_TC_ACTION_FLAG_FWD) {
			action_flags |= CFA_FLOW_ALLOC_REQ_ACTION_FLAGS_FWD;
			req->dst_fid = cpu_to_le16(actions->dst_fid);
		}
		if (actions->flags & BNXT_TC_ACTION_FLAG_PUSH_VLAN) {
			action_flags |=
			    CFA_FLOW_ALLOC_REQ_ACTION_FLAGS_L2_HEADER_REWRITE;
			req->l2_rewrite_vlan_tpid = actions->push_vlan_tpid;
			req->l2_rewrite_vlan_tci = actions->push_vlan_tci;
			memcpy(&req->l2_rewrite_dmac, &req->dmac, ETH_ALEN);
			memcpy(&req->l2_rewrite_smac, &req->smac, ETH_ALEN);
		}
		if (actions->flags & BNXT_TC_ACTION_FLAG_POP_VLAN) {
			action_flags |=
			    CFA_FLOW_ALLOC_REQ_ACTION_FLAGS_L2_HEADER_REWRITE;
			/* Rewrite config with tpid = 0 implies vlan pop */
			req->l2_rewrite_vlan_tpid = 0;
			memcpy(&req->l2_rewrite_dmac, &req->dmac, ETH_ALEN);
			memcpy(&req->l2_rewrite_smac, &req->smac, ETH_ALEN);
		}
	}
	req->action_flags = cpu_to_le16(action_flags);

	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send_silent(bp, req);
	if (!rc) {
		/* CFA_FLOW_ALLOC response interpretation:
		 *		    fw with	     fw with
		 *		    16-bit	     64-bit
		 *		    flow handle      flow handle
		 *		    ===========	     ===========
		 * flow_handle      flow handle      flow context id
		 * ext_flow_handle  INVALID	     flow handle
		 * flow_id	    INVALID	     flow counter id
		 */
		flow_node->flow_handle = resp->flow_handle;
		if (bp->fw_cap & BNXT_FW_CAP_OVS_64BIT_HANDLE) {
			flow_node->ext_flow_handle = resp->ext_flow_handle;
			flow_node->flow_id = resp->flow_id;
		}
	}
	hwrm_req_drop(bp, req);
	return rc;
}

static int hwrm_cfa_decap_filter_alloc(struct bnxt *bp,
				       struct bnxt_tc_flow *flow,
				       struct bnxt_tc_l2_key *l2_info,
				       __le32 ref_decap_handle,
				       __le32 *decap_filter_handle)
{
	struct hwrm_cfa_decap_filter_alloc_output *resp;
	struct ip_tunnel_key *tun_key = &flow->tun_key;
	struct hwrm_cfa_decap_filter_alloc_input *req;
	u32 enables = 0;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_CFA_DECAP_FILTER_ALLOC);
	if (rc)
		goto exit;

	req->flags = cpu_to_le32(CFA_DECAP_FILTER_ALLOC_REQ_FLAGS_OVS_TUNNEL);
	enables |= CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_TUNNEL_TYPE |
		   CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_IP_PROTOCOL;
	req->tunnel_type = CFA_DECAP_FILTER_ALLOC_REQ_TUNNEL_TYPE_VXLAN;
	req->ip_protocol = CFA_DECAP_FILTER_ALLOC_REQ_IP_PROTOCOL_UDP;

	if (flow->flags & BNXT_TC_FLOW_FLAGS_TUNL_ID) {
		enables |= CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_TUNNEL_ID;
		/* tunnel_id is wrongly defined in hsi defn. as __le32 */
		req->tunnel_id = tunnel_id_to_key32(tun_key->tun_id);
	}

	if (flow->flags & BNXT_TC_FLOW_FLAGS_TUNL_ETH_ADDRS) {
		enables |= CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_DST_MACADDR;
		ether_addr_copy(req->dst_macaddr, l2_info->dmac);
	}
	if (l2_info->num_vlans) {
		enables |= CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_T_IVLAN_VID;
		req->t_ivlan_vid = l2_info->inner_vlan_tci;
	}

	enables |= CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_ETHERTYPE;
	req->ethertype = htons(ETH_P_IP);

	if (flow->flags & BNXT_TC_FLOW_FLAGS_TUNL_IPV4_ADDRS) {
		enables |= CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_SRC_IPADDR |
			   CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_DST_IPADDR |
			   CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_IPADDR_TYPE;
		req->ip_addr_type =
			CFA_DECAP_FILTER_ALLOC_REQ_IP_ADDR_TYPE_IPV4;
		req->dst_ipaddr[0] = tun_key->u.ipv4.dst;
		req->src_ipaddr[0] = tun_key->u.ipv4.src;
	}

	if (flow->flags & BNXT_TC_FLOW_FLAGS_TUNL_IPV6_ADDRS) {
		enables |= CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_SRC_IPADDR |
			   CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_DST_IPADDR |
			   CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_IPADDR_TYPE;
		req->ip_addr_type =
			CFA_DECAP_FILTER_ALLOC_REQ_IP_ADDR_TYPE_IPV6;
		memcpy(req->dst_ipaddr, &tun_key->u.ipv6.dst,
		       sizeof(req->dst_ipaddr));
		memcpy(req->src_ipaddr, &tun_key->u.ipv6.src,
		       sizeof(req->src_ipaddr));
	}

	if (flow->flags & BNXT_TC_FLOW_FLAGS_TUNL_PORTS) {
		enables |= CFA_DECAP_FILTER_ALLOC_REQ_ENABLES_DST_PORT;
		req->dst_port = tun_key->tp_dst;
	}

	/* Even though the decap_handle returned by hwrm_cfa_decap_filter_alloc
	 * is defined as __le32, l2_ctxt_ref_id is defined in HSI as __le16.
	 */
	req->l2_ctxt_ref_id = (__force __le16)ref_decap_handle;
	req->enables = cpu_to_le32(enables);

	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send_silent(bp, req);
	if (!rc)
		*decap_filter_handle = resp->decap_filter_id;
	hwrm_req_drop(bp, req);
exit:
	if (rc == -ENOSPC)
		net_info_ratelimited("%s %s: No HW resources for new flow, rc=%d\n",
				     bp->dev->name, __func__, rc);
	else if (rc)
		netdev_err(bp->dev, "%s: Error rc=%d\n", __func__, rc);

	return rc;
}

static int hwrm_cfa_decap_filter_free(struct bnxt *bp,
				      __le32 decap_filter_handle)
{
	struct hwrm_cfa_decap_filter_free_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_CFA_DECAP_FILTER_FREE);
	if (!rc) {
		req->decap_filter_id = decap_filter_handle;
		rc = hwrm_req_send(bp, req);
	}
	if (rc)
		netdev_info(bp->dev, "%s: Error rc=%d\n", __func__, rc);

	return rc;
}

static int hwrm_cfa_encap_record_alloc(struct bnxt *bp,
				       struct ip_tunnel_key *encap_key,
				       struct bnxt_tc_l2_key *l2_info,
				       __le32 *encap_record_handle)
{
	struct hwrm_cfa_encap_record_alloc_output *resp;
	struct hwrm_cfa_encap_record_alloc_input *req;
	struct hwrm_cfa_encap_data_vxlan *encap;
	struct hwrm_vxlan_ipv4_hdr *encap_ipv4;
	struct hwrm_vxlan_ipv6_hdr *encap_ipv6;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_CFA_ENCAP_RECORD_ALLOC);
	if (rc)
		goto exit;

	encap = (struct hwrm_cfa_encap_data_vxlan *)&req->encap_data;
	req->encap_type = CFA_ENCAP_RECORD_ALLOC_REQ_ENCAP_TYPE_VXLAN;
	ether_addr_copy(encap->dst_mac_addr, l2_info->dmac);
	ether_addr_copy(encap->src_mac_addr, l2_info->smac);
	if (l2_info->num_vlans) {
		encap->num_vlan_tags = l2_info->num_vlans;
		encap->ovlan_tci = l2_info->inner_vlan_tci;
		encap->ovlan_tpid = l2_info->inner_vlan_tpid;
	}

	if (l2_info->ether_type == htons(ETH_P_IPV6)) {
		encap_ipv6 = (struct hwrm_vxlan_ipv6_hdr *)encap->l3;
		encap_ipv6->ver_tc_flow_label =
				6 << VXLAN_IPV6_HDR_VER_TC_FLOW_LABEL_VER_SFT;
		memcpy(encap_ipv6->dest_ip_addr, &encap_key->u.ipv6.dst,
		       sizeof(encap_ipv6->dest_ip_addr));
		memcpy(encap_ipv6->src_ip_addr, &encap_key->u.ipv6.src,
		       sizeof(encap_ipv6->src_ip_addr));
		encap_ipv6->ttl = encap_key->ttl;
		encap_ipv6->next_hdr = IPPROTO_UDP;
	} else {
		encap_ipv4 = (struct hwrm_vxlan_ipv4_hdr *)encap->l3;
		encap_ipv4->ver_hlen = 4 << VXLAN_IPV4_HDR_VER_HLEN_VERSION_SFT;
		encap_ipv4->ver_hlen |=
			5 << VXLAN_IPV4_HDR_VER_HLEN_HEADER_LENGTH_SFT;
		encap_ipv4->ttl = encap_key->ttl;
		encap_ipv4->dest_ip_addr = encap_key->u.ipv4.dst;
		encap_ipv4->src_ip_addr = encap_key->u.ipv4.src;
		encap_ipv4->protocol = IPPROTO_UDP;
	}

	encap->dst_port = encap_key->tp_dst;
	encap->vni = tunnel_id_to_key32(encap_key->tun_id);

	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send_silent(bp, req);
	if (!rc)
		*encap_record_handle = resp->encap_record_id;
	hwrm_req_drop(bp, req);
exit:
	if (rc == -ENOSPC)
		net_info_ratelimited("%s %s: No HW resources for new flow, rc=%d\n",
				     bp->dev->name, __func__, rc);
	else if (rc)
		netdev_err(bp->dev, "%s: Error rc=%d\n", __func__, rc);

	return rc;
}

static int hwrm_cfa_encap_record_free(struct bnxt *bp,
				      __le32 encap_record_handle)
{
	struct hwrm_cfa_encap_record_free_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_CFA_ENCAP_RECORD_FREE);
	if (!rc) {
		req->encap_record_id = encap_record_handle;
		rc = hwrm_req_send(bp, req);
	}
	if (rc)
		netdev_info(bp->dev, "%s: Error rc=%d\n", __func__, rc);

	return rc;
}

static int bnxt_tc_put_l2_node(struct bnxt *bp,
			       struct bnxt_tc_flow_node *flow_node)
{
	struct bnxt_tc_l2_node *l2_node = flow_node->l2_node;
	struct bnxt_tc_info *tc_info = bp->tc_info;
	int rc;

	/* l2_node may be release twice if re-add flow to HW failed when egress
	 * tunnel MAC was changed, return gracefully for second time.
	 */
	if (!l2_node)
		return 0;

	/* remove flow_node from the L2 shared flow list */
	list_del(&flow_node->l2_list_node);
	if (--l2_node->refcount == 0) {
		rc =  rhashtable_remove_fast(&tc_info->l2_table, &l2_node->node,
					     tc_info->l2_ht_params);
		if (rc)
			netdev_err(bp->dev,
				   "Error: %s: rhashtable_remove_fast: %d\n",
				   __func__, rc);
		kfree_rcu(l2_node, rcu);
	}
	flow_node->l2_node = NULL;
	return 0;
}

static struct bnxt_tc_l2_node *
bnxt_tc_get_l2_node(struct bnxt *bp, struct rhashtable *l2_table,
		    struct rhashtable_params ht_params,
		    struct bnxt_tc_l2_key *l2_key)
{
	struct bnxt_tc_l2_node *l2_node;
	int rc;

	l2_node = rhashtable_lookup_fast(l2_table, l2_key, ht_params);
	if (!l2_node) {
		l2_node = kzalloc(sizeof(*l2_node), GFP_KERNEL);
		if (!l2_node)
			return NULL;

		l2_node->key = *l2_key;
		rc = rhashtable_insert_fast(l2_table, &l2_node->node,
					    ht_params);
		if (rc) {
			kfree_rcu(l2_node, rcu);
			netdev_err(bp->dev,
				   "Error: %s: rhashtable_insert_fast: %d\n",
				   __func__, rc);
			return NULL;
		}
		INIT_LIST_HEAD(&l2_node->common_l2_flows);
	}
	return l2_node;
}

/* Get the ref_flow_handle for a flow by checking if there are any other
 * flows that share the same L2 key as this flow.
 */
static int
bnxt_tc_get_ref_flow_handle(struct bnxt *bp, struct bnxt_tc_flow *flow,
			    struct bnxt_tc_flow_node *flow_node,
			    __le16 *ref_flow_handle)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_flow_node *ref_flow_node;
	struct bnxt_tc_l2_node *l2_node;

	l2_node = bnxt_tc_get_l2_node(bp, &tc_info->l2_table,
				      tc_info->l2_ht_params,
				      &flow->l2_key);
	if (!l2_node)
		return -1;

	/* If any other flow is using this l2_node, use it's flow_handle
	 * as the ref_flow_handle
	 */
	if (l2_node->refcount > 0) {
		ref_flow_node = list_first_entry(&l2_node->common_l2_flows,
						 struct bnxt_tc_flow_node,
						 l2_list_node);
		*ref_flow_handle = ref_flow_node->flow_handle;
	} else {
		*ref_flow_handle = cpu_to_le16(0xffff);
	}

	/* Insert the l2_node into the flow_node so that subsequent flows
	 * with a matching l2 key can use the flow_handle of this flow
	 * as their ref_flow_handle
	 */
	flow_node->l2_node = l2_node;
	list_add(&flow_node->l2_list_node, &l2_node->common_l2_flows);
	l2_node->refcount++;
	return 0;
}

/* After the flow parsing is done, this routine is used for checking
 * if there are any aspects of the flow that prevent it from being
 * offloaded.
 */
static bool bnxt_tc_can_offload(struct bnxt *bp, struct bnxt_tc_flow *flow)
{
	/* If L4 ports are specified then ip_proto must be TCP or UDP */
	if ((flow->flags & BNXT_TC_FLOW_FLAGS_PORTS) &&
	    (flow->l4_key.ip_proto != IPPROTO_TCP &&
	     flow->l4_key.ip_proto != IPPROTO_UDP)) {
		netdev_info(bp->dev, "Cannot offload non-TCP/UDP (%d) ports\n",
			    flow->l4_key.ip_proto);
		return false;
	}

	if (is_multicast_ether_addr(flow->l2_key.dmac) ||
	    is_broadcast_ether_addr(flow->l2_key.dmac)) {
		netdev_info(bp->dev,
			    "Broadcast/Multicast flow offload unsupported\n");
		return false;
	}

	/* Currently source/dest MAC cannot be partial wildcard  */
	if (bits_set(&flow->l2_key.smac, sizeof(flow->l2_key.smac)) &&
	    !is_exactmatch(flow->l2_mask.smac, sizeof(flow->l2_mask.smac))) {
		netdev_info(bp->dev, "Wildcard match unsupported for Source MAC\n");
		return false;
	}
	if (bits_set(&flow->l2_key.dmac, sizeof(flow->l2_key.dmac)) &&
	    !is_exactmatch(&flow->l2_mask.dmac, sizeof(flow->l2_mask.dmac))) {
		netdev_info(bp->dev, "Wildcard match unsupported for Dest MAC\n");
		return false;
	}

	/* Currently VLAN fields cannot be partial wildcard */
	if (bits_set(&flow->l2_key.inner_vlan_tci,
		     sizeof(flow->l2_key.inner_vlan_tci)) &&
	    !is_vlan_tci_allowed(flow->l2_mask.inner_vlan_tci,
				 flow->l2_key.inner_vlan_tci)) {
		netdev_info(bp->dev, "Unsupported VLAN TCI\n");
		return false;
	}
	if (bits_set(&flow->l2_key.inner_vlan_tpid,
		     sizeof(flow->l2_key.inner_vlan_tpid)) &&
	    !is_exactmatch(&flow->l2_mask.inner_vlan_tpid,
			   sizeof(flow->l2_mask.inner_vlan_tpid))) {
		netdev_info(bp->dev, "Wildcard match unsupported for VLAN TPID\n");
		return false;
	}

	/* Currently Ethertype must be set */
	if (!is_exactmatch(&flow->l2_mask.ether_type,
			   sizeof(flow->l2_mask.ether_type))) {
		netdev_info(bp->dev, "Wildcard match unsupported for Ethertype\n");
		return false;
	}

	return true;
}

/*
 * Returns the final refcount of the node on success
 * or a -ve error code on failure
 */
static int bnxt_tc_put_tunnel_node(struct bnxt *bp,
				   struct rhashtable *tunnel_table,
				   struct rhashtable_params *ht_params,
				   struct bnxt_tc_tunnel_node *tunnel_node)
{
	int rc;

	if (--tunnel_node->refcount == 0) {
		if (tunnel_node->encap_list_node.prev)
			list_del(&tunnel_node->encap_list_node);

		rc =  rhashtable_remove_fast(tunnel_table, &tunnel_node->node,
					     *ht_params);
		if (rc) {
			netdev_err(bp->dev, "rhashtable_remove_fast rc=%d\n", rc);
			rc = -1;
		}
		kfree_rcu(tunnel_node, rcu);
		return rc;
	} else {
		return tunnel_node->refcount;
	}
}

/*
 * Get (or add) either encap or decap tunnel node from/to the supplied
 * hash table.
 */
static struct bnxt_tc_tunnel_node *
bnxt_tc_get_tunnel_node(struct bnxt *bp, struct rhashtable *tunnel_table,
			struct rhashtable_params *ht_params,
			struct ip_tunnel_key *tun_key,
			enum bnxt_tc_tunnel_node_type tunnel_node_type)
{
	struct bnxt_tc_tunnel_node *tunnel_node;
	int rc;

	tunnel_node = rhashtable_lookup_fast(tunnel_table, tun_key, *ht_params);
	if (!tunnel_node) {
		tunnel_node = kzalloc(sizeof(*tunnel_node), GFP_KERNEL);
		if (!tunnel_node) {
			rc = -ENOMEM;
			goto err;
		}

		tunnel_node->key = *tun_key;
		tunnel_node->tunnel_handle = INVALID_TUNNEL_HANDLE;
		tunnel_node->tunnel_node_type = tunnel_node_type;
		rc = rhashtable_insert_fast(tunnel_table, &tunnel_node->node,
					    *ht_params);
		if (rc) {
			kfree_rcu(tunnel_node, rcu);
			goto err;
		}
		INIT_LIST_HEAD(&tunnel_node->common_encap_flows);
	}
	tunnel_node->refcount++;
	return tunnel_node;
err:
	netdev_info(bp->dev, "error rc=%d\n", rc);
	return NULL;
}

static int bnxt_tc_put_neigh_node(struct bnxt *bp,
				  struct rhashtable *neigh_table,
				  struct rhashtable_params *ht_params,
				  struct bnxt_tc_neigh_node *neigh_node)
{
	int rc;

	if (--neigh_node->refcount > 0)
		return neigh_node->refcount;

	/* Neigh node reference count is 0 */
	rc =  rhashtable_remove_fast(neigh_table, &neigh_node->node,
				     *ht_params);
	if (rc)
		netdev_err(bp->dev, "%s: rhashtable_remove_fast rc=%d\n",
			   __func__, rc);

	kfree_rcu(neigh_node, rcu);
	return rc;
}

static struct bnxt_tc_neigh_node *
bnxt_tc_get_neigh_node(struct bnxt *bp, struct rhashtable *neigh_table,
		       struct rhashtable_params *ht_params,
		       struct bnxt_tc_neigh_key *neigh_key)
{
	struct bnxt_tc_neigh_node *neigh_node;
	int rc;

	neigh_node = rhashtable_lookup_fast(neigh_table, neigh_key, *ht_params);
	if (neigh_node) {
		neigh_node->refcount++;
		return neigh_node;
	}

	neigh_node = kzalloc(sizeof(*neigh_node), GFP_KERNEL);
	if (!neigh_node)
		return NULL;

	neigh_node->key = *neigh_key;
	rc = rhashtable_insert_fast(neigh_table, &neigh_node->node, *ht_params);
	if (rc) {
		kfree_rcu(neigh_node, rcu);
		return NULL;
	}
	INIT_LIST_HEAD(&neigh_node->common_encap_list);
	neigh_node->refcount++;
	return neigh_node;
}

static int bnxt_tc_get_ref_decap_handle(struct bnxt *bp,
					struct bnxt_tc_flow *flow,
					struct bnxt_tc_l2_key *l2_key,
					struct bnxt_tc_flow_node *flow_node,
					__le32 *ref_decap_handle)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_flow_node *ref_flow_node;
	struct bnxt_tc_l2_node *decap_l2_node;

	decap_l2_node = bnxt_tc_get_l2_node(bp, &tc_info->decap_l2_table,
					    tc_info->decap_l2_ht_params,
					    l2_key);
	if (!decap_l2_node)
		return -1;

	/* If any other flow is using this decap_l2_node, use it's decap_handle
	 * as the ref_decap_handle
	 */
	if (decap_l2_node->refcount > 0) {
		ref_flow_node =
			list_first_entry(&decap_l2_node->common_l2_flows,
					 struct bnxt_tc_flow_node,
					 decap_l2_list_node);
		*ref_decap_handle = ref_flow_node->decap_node->tunnel_handle;
	} else {
		*ref_decap_handle = INVALID_TUNNEL_HANDLE;
	}

	/* Insert the l2_node into the flow_node so that subsequent flows
	 * with a matching decap l2 key can use the decap_filter_handle of
	 * this flow as their ref_decap_handle
	 */
	flow_node->decap_l2_node = decap_l2_node;
	list_add(&flow_node->decap_l2_list_node,
		 &decap_l2_node->common_l2_flows);
	decap_l2_node->refcount++;
	return 0;
}

static void bnxt_tc_put_decap_l2_node(struct bnxt *bp,
				      struct bnxt_tc_flow_node *flow_node)
{
	struct bnxt_tc_l2_node *decap_l2_node = flow_node->decap_l2_node;
	struct bnxt_tc_info *tc_info = bp->tc_info;
	int rc;

	/* remove flow_node from the decap L2 sharing flow list */
	list_del(&flow_node->decap_l2_list_node);
	if (--decap_l2_node->refcount == 0) {
		rc =  rhashtable_remove_fast(&tc_info->decap_l2_table,
					     &decap_l2_node->node,
					     tc_info->decap_l2_ht_params);
		if (rc)
			netdev_err(bp->dev, "rhashtable_remove_fast rc=%d\n", rc);
		kfree_rcu(decap_l2_node, rcu);
	}
}

static void bnxt_tc_put_decap_handle(struct bnxt *bp,
				     struct bnxt_tc_flow_node *flow_node)
{
	__le32 decap_handle = flow_node->decap_node->tunnel_handle;
	struct bnxt_tc_info *tc_info = bp->tc_info;
	int rc;

	if (flow_node->decap_l2_node)
		bnxt_tc_put_decap_l2_node(bp, flow_node);

	rc = bnxt_tc_put_tunnel_node(bp, &tc_info->decap_table,
				     &tc_info->decap_ht_params,
				     flow_node->decap_node);
	if (!rc && decap_handle != INVALID_TUNNEL_HANDLE)
		hwrm_cfa_decap_filter_free(bp, decap_handle);
}

static int bnxt_tc_create_neigh_node(struct bnxt *bp, void *flow_node,
				     struct bnxt_tc_neigh_key *neigh_key)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_tunnel_node *encap_node;
	struct bnxt_tc_neigh_node *neigh_node;

	if (BNXT_TRUFLOW_EN(bp)) {
		struct bnxt_tf_flow_node *node = flow_node;

		encap_node = node->encap_node;
	} else {
		struct bnxt_tc_flow_node *node = flow_node;

		encap_node = node->encap_node;
	}

	neigh_node = bnxt_tc_get_neigh_node(bp, &tc_info->neigh_table,
					    &tc_info->neigh_ht_params,
					    neigh_key);
	if (!neigh_node)
		return -ENOMEM;

	ether_addr_copy(neigh_node->dmac, encap_node->l2_info.dmac);
	encap_node->neigh_node = neigh_node;
	list_add(&encap_node->encap_list_node, &neigh_node->common_encap_list);

	return 0;
}

static int bnxt_tc_resolve_vlan(struct bnxt *bp,
				struct bnxt_tc_l2_key *l2_info,
				struct net_device *dst_dev)
{
#ifdef CONFIG_INET
	struct net_device *real_dst_dev = bp->dev;
	int rc = 0;

	/* The route must either point to the real_dst_dev or a dst_dev that
	 * uses the real_dst_dev.
	 */
	if (is_vlan_dev(dst_dev)) {
#if IS_ENABLED(CONFIG_VLAN_8021Q)
		struct vlan_dev_priv *vlan = vlan_dev_priv(dst_dev);

		if (vlan->real_dev != real_dst_dev)
			return -ENETUNREACH;

		l2_info->inner_vlan_tci = htons(vlan->vlan_id);
		l2_info->inner_vlan_tpid = vlan->vlan_proto;
		l2_info->num_vlans = 1;
#endif
	} else if (dst_dev != real_dst_dev) {
		rc = -ENETUNREACH;
	}

	return rc;
#else
	return -EOPNOTSUPP;
#endif
}

static int bnxt_tc_resolve_mac(struct bnxt *bp,
			       struct bnxt_tc_l2_key *l2_info,
			       struct net_device *dst_dev,
			       struct neighbour *nbr)
{
#ifdef CONFIG_INET
	int i = 0;

	neigh_ha_snapshot(l2_info->dmac, nbr, dst_dev);

	if (!is_zero_ether_addr(l2_info->dmac)) {
		ether_addr_copy(l2_info->smac, dst_dev->dev_addr);
		return 0;
	}

	/* Call neigh_event_send to resolve MAC address if didn't
	 * get a valid one.
	 */
	if (!(nbr->nud_state & NUD_VALID))
		neigh_event_send(nbr, NULL);

	while (true) {
		neigh_ha_snapshot(l2_info->dmac, nbr, dst_dev);
		if (!is_zero_ether_addr(l2_info->dmac)) {
			ether_addr_copy(l2_info->smac, dst_dev->dev_addr);
			return 0;
		}
		if (++i > BNXT_MAX_NEIGH_TIMEOUT)
			return -ENETUNREACH;

		usleep_range(200, 600);
	}
#else
	return -EOPNOTSUPP;
#endif
}

static void bnxt_tc_init_neigh_key(struct bnxt *bp,
				   struct bnxt_tc_neigh_key *neigh_key,
				   struct neighbour *nbr)
{
	memcpy(&neigh_key->dst_ip, nbr->primary_key, nbr->tbl->key_len);
	neigh_key->family = nbr->ops->family;
	neigh_key->dev = bp->dev;
}

int bnxt_tc_resolve_ipv4_tunnel_hdrs(struct bnxt *bp,
				     struct bnxt_tc_flow_node *flow_node,
				     struct ip_tunnel_key *tun_key,
				     struct bnxt_tc_l2_key *l2_info,
				     struct bnxt_tc_neigh_key *neigh_key)
{
#ifdef CONFIG_INET
	struct net_device *real_dst_dev = bp->dev;
	struct flowi4 flow = { {0} };
	struct net_device *dst_dev;
	struct rtable *rt = NULL;
	struct neighbour *nbr;
	int rc;

	flow.flowi4_proto = IPPROTO_UDP;
	flow.fl4_dport = tun_key->tp_dst;
	flow.daddr = tun_key->u.ipv4.dst;
	rt = ip_route_output_key(dev_net(real_dst_dev), &flow);
	if (IS_ERR(rt))
		return -ENETUNREACH;

	dst_dev = rt->dst.dev;
	rc = bnxt_tc_resolve_vlan(bp, l2_info, dst_dev);
	if (rc) {
		netdev_info(bp->dev,
			    "dst_dev(%s) for %pI4b is not PF-if(%s)\n",
			    netdev_name(dst_dev), &flow.daddr,
			    netdev_name(real_dst_dev));
		ip_rt_put(rt);
		return rc;
	}

	nbr = dst_neigh_lookup(&rt->dst, &flow.daddr);
	if (!nbr) {
		netdev_info(bp->dev, "can't lookup neighbor for %pI4b\n",
			    &flow.daddr);
		ip_rt_put(rt);
		return -ENETUNREACH;
	}

	if (!tun_key->u.ipv4.src)
		tun_key->u.ipv4.src = flow.saddr;
	tun_key->ttl = ip4_dst_hoplimit(&rt->dst);
	rc = bnxt_tc_resolve_mac(bp, l2_info, dst_dev, nbr);
	if (neigh_key)
		bnxt_tc_init_neigh_key(bp, neigh_key, nbr);
	neigh_release(nbr);
	ip_rt_put(rt);

	return rc;
#else
	return -EOPNOTSUPP;
#endif
}

int bnxt_tc_resolve_ipv6_tunnel_hdrs(struct bnxt *bp,
				     struct bnxt_tc_flow_node *flow_node,
				     struct ip_tunnel_key *tun_key,
				     struct bnxt_tc_l2_key *l2_info,
				     struct bnxt_tc_neigh_key *neigh_key)
{
#ifdef CONFIG_INET
	struct net_device *real_dst_dev = bp->dev;
	struct flowi6 flow6 = { {0} };
	struct dst_entry *dst = NULL;
	struct net_device *dst_dev;
	struct neighbour *nbr;
	int rc;

	flow6.daddr = tun_key->u.ipv6.dst;
	flow6.fl6_dport = tun_key->tp_dst;
	flow6.flowi6_proto = IPPROTO_UDP;
	dst = ip6_route_output(dev_net(real_dst_dev), NULL, &flow6);
	if (dst->error)
		return -ENETUNREACH;

	dst_dev = dst->dev;
	rc = bnxt_tc_resolve_vlan(bp, l2_info, dst_dev);
	if (rc) {
		netdev_info(bp->dev,
			    "dst_dev(%s) for %pI6 is not PF-if(%s)\n",
			    netdev_name(dst_dev), &flow6.daddr,
			    netdev_name(real_dst_dev));
		dst_release(dst);
		return rc;
	}

	nbr = dst_neigh_lookup(dst, &flow6.daddr);
	if (!nbr) {
		netdev_info(bp->dev, "can't lookup neighbor for %pI6\n",
			    &flow6.daddr);
		dst_release(dst);
		return -ENETUNREACH;
	}

	tun_key->ttl = ip6_dst_hoplimit(dst);
	rc = bnxt_tc_resolve_mac(bp, l2_info, dst_dev, nbr);
	if (neigh_key)
		bnxt_tc_init_neigh_key(bp, neigh_key, nbr);
	neigh_release(nbr);
	dst_release(dst);

	return rc;
#else
	return -EOPNOTSUPP;
#endif
}

static int bnxt_tc_resolve_tunnel_hdrs(struct bnxt *bp,
				       struct bnxt_tc_flow_node *flow_node,
				       struct ip_tunnel_key *tun_key,
				       struct bnxt_tc_l2_key *l2_info,
				       struct bnxt_tc_neigh_key *neigh_key)
{
	if ((flow_node->flow.flags & BNXT_TC_FLOW_FLAGS_TUNL_IPV6_ADDRS) ||
	    (flow_node->flow.actions.flags & BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP_IPV6))
		return bnxt_tc_resolve_ipv6_tunnel_hdrs(bp, flow_node, tun_key,
							l2_info, neigh_key);
	else
		return bnxt_tc_resolve_ipv4_tunnel_hdrs(bp, flow_node, tun_key,
							l2_info, neigh_key);
}

static bool bnxt_tc_need_lkup_tunnel_hdrs(struct bnxt_tc_flow *flow)
{
	bool need_tun_lkup = false;

	/* Some use cases don't want to match tunnel SIP for ingress flow, it will
	 * not specify the tunnel SIP in flow key fields, for these cases, need
	 * skip to lookup tunnel header which include lookup routing table,
	 * otherwise, the lookup result may not point to PF's net device, driver
	 * will not offload this flow. We can use PF's MAC to set up the decap
	 * tunnel to offload this flow successfully since HW supports it.
	 * Use tunnel SIP mask to check whether there has tunnel SIP in the flow
	 * key fields.
	 * For example, following ingress flow doesn't specify to match the tunnel
	 * sip which the tunnel SIP is 0.0.0.0 to linux driver, we can't use tunnel
	 * sip 0.0.0.0 to lookup routing table which may point to non PF's net
	 * device, and driver will not offload below flow but HW actually can
	 * support to offload this flow by using the PF's MAC to set up decap
	 * tunnel.
	 * tc filter add dev vxlan0 ingress prio 100 chain 0 proto ip flower \
	 * enc_dst_ip 2.1.1.195 enc_dst_port 4789 enc_key_id 22 dst_ip 90.1.2.20 \
	 * action tunnel_key unset action pedit ex munge eth dst set \
	 * 46:6c:99:59:cb:15 pipe action mirred egress redirect dev eth0
	 */
	if (flow->flags & BNXT_TC_FLOW_FLAGS_TUNL_IPV6_ADDRS) {
		if (flow->tun_mask.u.ipv6.src.s6_addr32[0] ||
		    flow->tun_mask.u.ipv6.src.s6_addr32[1] ||
		    flow->tun_mask.u.ipv6.src.s6_addr32[2] ||
		    flow->tun_mask.u.ipv6.src.s6_addr32[3])
			need_tun_lkup = true;
	} else {
		if (flow->tun_mask.u.ipv4.src)
			need_tun_lkup = true;
	}

	return need_tun_lkup;
}

static int bnxt_tc_get_decap_handle(struct bnxt *bp, struct bnxt_tc_flow *flow,
				    struct bnxt_tc_flow_node *flow_node,
				    __le32 *decap_filter_handle)
{
	struct ip_tunnel_key *decap_key = &flow->tun_key;
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_l2_key l2_info = { 0 };
	struct bnxt_tc_tunnel_node *decap_node;
	struct ip_tunnel_key tun_key = { 0 };
	struct bnxt_tc_l2_key *decap_l2_info;
	struct bnxt_tc_neigh_key neigh_key;
	__le32 ref_decap_handle;
	int rc;

	/* Check if there's another flow using the same tunnel decap.
	 * If not, add this tunnel to the table and resolve the other
	 * tunnel header fields. Ignore src_port in the tunnel_key,
	 * since it is not required for decap filters.
	 */
	decap_key->tp_src = 0;
	decap_node = bnxt_tc_get_tunnel_node(bp, &tc_info->decap_table,
					     &tc_info->decap_ht_params,
					     decap_key,
					     BNXT_TC_TUNNEL_NODE_TYPE_DECAP);
	if (!decap_node)
		return -ENOMEM;

	flow_node->decap_node = decap_node;

	if (decap_node->tunnel_handle != INVALID_TUNNEL_HANDLE)
		goto done;

	/*
	 * Resolve the L2 fields for tunnel decap
	 * Resolve the route for remote vtep (saddr) of the decap key
	 * Find it's next-hop mac addrs
	 */
	if (flow->flags & BNXT_TC_FLOW_FLAGS_TUNL_IPV6_ADDRS)
		tun_key.u.ipv6.dst = flow->tun_key.u.ipv6.src;
	else
		tun_key.u.ipv4.dst = flow->tun_key.u.ipv4.src;

	tun_key.tp_dst = flow->tun_key.tp_dst;
	decap_l2_info = &decap_node->l2_info;
	if (bnxt_tc_need_lkup_tunnel_hdrs(flow)) {
		rc = bnxt_tc_resolve_tunnel_hdrs(bp, flow_node, &tun_key,
						 &l2_info, &neigh_key);
		if (rc)
			goto put_decap;

		/* decap smac is wildcarded */
		ether_addr_copy(decap_l2_info->dmac, l2_info.smac);
		if (l2_info.num_vlans) {
			decap_l2_info->num_vlans = l2_info.num_vlans;
			decap_l2_info->inner_vlan_tpid = l2_info.inner_vlan_tpid;
			decap_l2_info->inner_vlan_tci = l2_info.inner_vlan_tci;
		}
	} else {
		ether_addr_copy(decap_l2_info->dmac, bp->pf.mac_addr);
	}
	flow->flags |= BNXT_TC_FLOW_FLAGS_TUNL_ETH_ADDRS;

	/* For getting a decap_filter_handle we first need to check if
	 * there are any other decap flows that share the same tunnel L2
	 * key and if so, pass that flow's decap_filter_handle as the
	 * ref_decap_handle for this flow.
	 */
	rc = bnxt_tc_get_ref_decap_handle(bp, flow, decap_l2_info, flow_node,
					  &ref_decap_handle);
	if (rc)
		goto put_decap;

	/* Issue the hwrm cmd to allocate a decap filter handle */
	rc = hwrm_cfa_decap_filter_alloc(bp, flow, decap_l2_info,
					 ref_decap_handle,
					 &decap_node->tunnel_handle);
	if (rc)
		goto put_decap_l2;

done:
	*decap_filter_handle = decap_node->tunnel_handle;
	return 0;

put_decap_l2:
	bnxt_tc_put_decap_l2_node(bp, flow_node);
put_decap:
	bnxt_tc_put_tunnel_node(bp, &tc_info->decap_table,
				&tc_info->decap_ht_params,
				flow_node->decap_node);
	return rc;
}

static void bnxt_tc_put_encap_handle(struct bnxt *bp,
				     struct bnxt_tc_flow_node *flow_node)
{
	__le32 encap_handle = flow_node->encap_node->tunnel_handle;
	struct bnxt_tc_info *tc_info = bp->tc_info;
	int rc;

	list_del(&flow_node->encap_flow_list_node);
	rc = bnxt_tc_put_tunnel_node(bp, &tc_info->encap_table,
				     &tc_info->encap_ht_params,
				     flow_node->encap_node);
	if (!rc && encap_handle != INVALID_TUNNEL_HANDLE) {
		hwrm_cfa_encap_record_free(bp, encap_handle);
		bnxt_tc_put_neigh_node(bp, &tc_info->neigh_table,
				       &tc_info->neigh_ht_params,
				       flow_node->encap_node->neigh_node);
	}
}

/*
 * Lookup the tunnel encap table and check if there's an encap_handle
 * alloc'd already.
 * If not, query L2 info via a route lookup and issue an encap_record_alloc
 * cmd to FW.
 */
static int bnxt_tc_get_encap_handle(struct bnxt *bp, struct bnxt_tc_flow *flow,
				    struct bnxt_tc_flow_node *flow_node,
				    __le32 *encap_handle)
{
	struct ip_tunnel_key *encap_key = &flow->actions.tun_encap_key;
	struct bnxt_tc_neigh_key neigh_key = { 0 };
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_tunnel_node *encap_node;
	int rc;

	/* Check if there's another flow using the same tunnel encap.
	 * If not, add this tunnel to the table and resolve the other
	 * tunnel header fields
	 */
	encap_node = bnxt_tc_get_tunnel_node(bp, &tc_info->encap_table,
					     &tc_info->encap_ht_params,
					     encap_key,
					     BNXT_TC_TUNNEL_NODE_TYPE_ENCAP);
	if (!encap_node)
		return -ENOMEM;

	flow_node->encap_node = encap_node;

	if (encap_node->tunnel_handle != INVALID_TUNNEL_HANDLE)
		goto done;

	if (flow->actions.flags & BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP_IPV6)
		encap_node->l2_info.ether_type = htons(ETH_P_IPV6);
	else
		encap_node->l2_info.ether_type = htons(ETH_P_IP);

	rc = bnxt_tc_resolve_tunnel_hdrs(bp, flow_node, encap_key,
					 &encap_node->l2_info, &neigh_key);
	if (rc)
		goto put_encap;

	rc = bnxt_tc_create_neigh_node(bp, flow_node, &neigh_key);
	if (rc)
		goto put_encap;

	/* Allocate a new tunnel encap record */
	rc = hwrm_cfa_encap_record_alloc(bp, encap_key, &encap_node->l2_info,
					 &encap_node->tunnel_handle);
	if (rc)
		goto put_neigh;

done:
	*encap_handle = encap_node->tunnel_handle;
	/* Add flow to encap list, it will be used by neigh update event */
	list_add(&flow_node->encap_flow_list_node, &encap_node->common_encap_flows);
	return 0;

put_neigh:
	bnxt_tc_put_neigh_node(bp, &tc_info->neigh_table,
			       &tc_info->neigh_ht_params,
			       flow_node->encap_node->neigh_node);
put_encap:
	bnxt_tc_put_tunnel_node(bp, &tc_info->encap_table,
				&tc_info->encap_ht_params, encap_node);
	return rc;
}

static void bnxt_tc_put_tunnel_handle(struct bnxt *bp,
				      struct bnxt_tc_flow *flow,
				      struct bnxt_tc_flow_node *flow_node)
{
	if (flow->actions.flags & BNXT_TC_ACTION_FLAG_TUNNEL_DECAP)
		bnxt_tc_put_decap_handle(bp, flow_node);
	else if (flow->actions.flags & BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP)
		bnxt_tc_put_encap_handle(bp, flow_node);
}

static int bnxt_tc_get_tunnel_handle(struct bnxt *bp,
				     struct bnxt_tc_flow *flow,
				     struct bnxt_tc_flow_node *flow_node,
				     __le32 *tunnel_handle)
{
	if (flow->actions.flags & BNXT_TC_ACTION_FLAG_TUNNEL_DECAP)
		return bnxt_tc_get_decap_handle(bp, flow, flow_node,
						tunnel_handle);
	else if (flow->actions.flags & BNXT_TC_ACTION_FLAG_TUNNEL_ENCAP)
		return bnxt_tc_get_encap_handle(bp, flow, flow_node,
						tunnel_handle);
	else
		return 0;
}

static void bnxt_tc_del_encap_flow(struct bnxt *bp,
				   struct bnxt_tc_flow_node *flow_node)
{
	/* 1. Delete HW cfa flow entry.
	 * 2. Delete SW l2 node, will add SW l2 node when alloc flow again.
	 */
	bnxt_hwrm_cfa_flow_free(bp, flow_node);
	bnxt_tc_put_l2_node(bp, flow_node);
}

static void bnxt_tc_free_encap_flow(struct bnxt *bp,
				    struct bnxt_tc_flow_node *flow_node)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	int rc;

	/* L2 node may be released twice, return gracefully for second time */
	bnxt_tc_put_l2_node(bp, flow_node);
	bnxt_tc_put_tunnel_handle(bp, &flow_node->flow, flow_node);
	rc = rhashtable_remove_fast(&tc_info->flow_table, &flow_node->node,
				    tc_info->flow_ht_params);
	if (rc)
		netdev_err(bp->dev, "%s: Error: rhashtable_remove_fast rc=%d\n",
			   __func__, rc);

	kfree_rcu(flow_node, rcu);
	netdev_dbg(bp->dev, "%s: Failed to re-add flow to HW, freed flow memory\n",
		   __func__);
}

static int bnxt_tc_add_encap_flow(struct bnxt *bp,
				  struct bnxt_tc_neigh_node *neigh_node,
				  struct bnxt_tc_flow_node *flow_node)
{
	struct bnxt_tc_tunnel_node *encap_node;
	struct ip_tunnel_key *encap_key;
	struct bnxt_tc_flow *flow;
	__le16 ref_flow_handle;
	int rc;

	flow = &flow_node->flow;
	encap_key = &flow->actions.tun_encap_key;
	encap_node = flow_node->encap_node;

	/* 1. Get ref_flow_handle.
	 * 2. Add HW encap record.
	 * 3. Add HW cfa flow entry.
	 */
	rc = bnxt_tc_get_ref_flow_handle(bp, flow, flow_node, &ref_flow_handle);
	if (rc)
		return rc;

	/* Allocate a new tunnel encap record */
	if (encap_node->tunnel_handle == INVALID_TUNNEL_HANDLE) {
		rc = hwrm_cfa_encap_record_alloc(bp, encap_key,
						 &encap_node->l2_info,
						 &encap_node->tunnel_handle);
		if (rc)
			return rc;
	}

	rc = bnxt_hwrm_cfa_flow_alloc(bp, flow, ref_flow_handle,
				      encap_node->tunnel_handle, flow_node);

	return rc;
}

static void *bnxt_tc_lkup_neigh_node(struct bnxt *bp,
				     struct neighbour *n)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_neigh_key key = { 0 };

	bnxt_tc_init_neigh_key(bp, &key, n);

	return rhashtable_lookup_fast(&tc_info->neigh_table, &key,
				      tc_info->neigh_ht_params);
}

static void
bnxt_tc_del_add_encap_flows_tf(struct bnxt *bp, struct bnxt_tc_tunnel_node *encap_node,
			       struct bnxt_tc_neigh_node *neigh_node)
{
	struct bnxt_tf_flow_node *flow_node;

	/* Flow may share the same encap node, need delete all the HW
	 * flow and encap record first, then update the SW encap tunnel
	 * handle, add HW encap record and flow at last.
	 */
	list_for_each_entry(encap_node, &neigh_node->common_encap_list,
			    encap_list_node) {
		list_for_each_entry(flow_node, &encap_node->common_encap_flows,
				    encap_flow_list_node)
			bnxt_ulp_update_flow_encap_record(bp, bp->neigh_update.neigh->ha,
							  flow_node->mparms,
							  &flow_node->flow_id);
		memcpy(encap_node->l2_info.dmac, bp->neigh_update.neigh->ha, ETH_ALEN);
	}
}

static void
bnxt_tc_del_add_encap_flows_afm(struct bnxt *bp, struct bnxt_tc_tunnel_node *encap_node,
				struct bnxt_tc_neigh_node *neigh_node)
{
	struct bnxt_tc_flow_node *flow_node;
	struct list_head failed_flows_head;
	int rc;

	INIT_LIST_HEAD(&failed_flows_head);
	/* Flow may share the same encap node, need delete all the HW
	 * flow and encap record first, then update the SW encap tunnel
	 * handle, add HW encap record and flow at last.
	 */
	list_for_each_entry(encap_node, &neigh_node->common_encap_list,
			    encap_list_node) {
		list_for_each_entry(flow_node, &encap_node->common_encap_flows,
				    encap_flow_list_node) {
			bnxt_tc_del_encap_flow(bp, flow_node);
		}

		hwrm_cfa_encap_record_free(bp, encap_node->tunnel_handle);
		encap_node->tunnel_handle = INVALID_TUNNEL_HANDLE;
		memcpy(encap_node->l2_info.dmac, bp->neigh_update.neigh->ha, ETH_ALEN);
	}

	list_for_each_entry(encap_node, &neigh_node->common_encap_list,
			    encap_list_node) {
		list_for_each_entry(flow_node, &encap_node->common_encap_flows,
				    encap_flow_list_node) {
			rc = bnxt_tc_add_encap_flow(bp, neigh_node, flow_node);
			if (rc)
				list_add(&flow_node->failed_add_flow_node,
					 &failed_flows_head);
		}
	}
	/* Free flow node which re-add to HW failed */
	list_for_each_entry(flow_node, &failed_flows_head, failed_add_flow_node)
		bnxt_tc_free_encap_flow(bp, flow_node);
}

void bnxt_tc_update_neigh_work(struct work_struct *work)
{
	struct bnxt *bp = container_of(work, struct bnxt, neigh_update.work);
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_tunnel_node *encap_node = NULL;
	struct bnxt_tc_neigh_node *neigh_node;

	mutex_lock(&tc_info->lock);
	neigh_node = bnxt_tc_lkup_neigh_node(bp, bp->neigh_update.neigh);
	if (!neigh_node)
		goto exit;

	if (ether_addr_equal(neigh_node->dmac, bp->neigh_update.neigh->ha))
		goto exit;

	if (BNXT_TRUFLOW_EN(bp))
		bnxt_tc_del_add_encap_flows_tf(bp, encap_node, neigh_node);
	else
		bnxt_tc_del_add_encap_flows_afm(bp, encap_node, neigh_node);

	memcpy(neigh_node->dmac, bp->neigh_update.neigh->ha, ETH_ALEN);

exit:
	mutex_unlock(&tc_info->lock);
	neigh_release(bp->neigh_update.neigh);
	bp->neigh_update.neigh = NULL;
}

static int __bnxt_tc_del_flow_afm(struct bnxt *bp, void *flow)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_flow_node *flow_node = flow;
	int rc;

	/* send HWRM cmd to free the flow-id */
	bnxt_hwrm_cfa_flow_free(bp, flow_node);

	/* release references to any tunnel encap/decap nodes */
	bnxt_tc_put_tunnel_handle(bp, &flow_node->flow, flow_node);

	/* release reference to l2 node */
	bnxt_tc_put_l2_node(bp, flow_node);

	rc = rhashtable_remove_fast(&tc_info->flow_table, &flow_node->node,
				    tc_info->flow_ht_params);
	if (rc)
		netdev_err(bp->dev, "Error: %s: rhashtable_remove_fast rc=%d\n",
			   __func__, rc);

	kfree_rcu(flow_node, rcu);
	return 0;
}

static void bnxt_tc_put_encap_node(struct bnxt *bp,
				   struct bnxt_tf_flow_node *flow_node)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	int refcnt;

	list_del(&flow_node->encap_flow_list_node);
	refcnt = bnxt_tc_put_tunnel_node(bp, &tc_info->encap_table,
					 &tc_info->encap_ht_params,
					 flow_node->encap_node);

	/* If there are no flows referencing this encap node,
	 * (i.e, encap_node is freed) drop its reference on
	 * the neigh_node.
	 */
	if (!refcnt)
		bnxt_tc_put_neigh_node(bp, &tc_info->neigh_table,
				       &tc_info->neigh_ht_params,
				       flow_node->encap_node->neigh_node);
}

static int bnxt_tc_get_encap_node(struct bnxt *bp,
				  struct bnxt_tf_flow_node *flow_node,
				  struct bnxt_ulp_flow_info *flow_info)
{
	struct ip_tunnel_key *encap_key = flow_info->encap_key;
	struct bnxt_tc_neigh_key *neigh_key = flow_info->neigh_key;
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_tunnel_node *encap_node;
	int rc;

	/* Check if there's another flow using the same tunnel encap.
	 * If not, add this tunnel to the table.
	 */
	encap_node = bnxt_tc_get_tunnel_node(bp, &tc_info->encap_table,
					     &tc_info->encap_ht_params,
					     encap_key,
					     BNXT_TC_TUNNEL_NODE_TYPE_ENCAP);
	if (!encap_node)
		return -ENOMEM;

	flow_node->encap_node = encap_node;

	/* Encap node already exists */
	if (encap_node->refcount > 1)
		goto done;

	/* Initialize encap node */
	ether_addr_copy(encap_node->l2_info.dmac, flow_info->tnl_dmac);
	ether_addr_copy(encap_node->l2_info.smac, flow_info->tnl_smac);
	encap_node->l2_info.ether_type = flow_info->tnl_ether_type;

	rc = bnxt_tc_create_neigh_node(bp, flow_node, neigh_key);
	if (rc)
		goto put_encap;

done:
	/* Add flow to encap list, it will be used by neigh update event */
	list_add(&flow_node->encap_flow_list_node,
		 &encap_node->common_encap_flows);
	return 0;

put_encap:
	bnxt_tc_put_tunnel_node(bp, &tc_info->encap_table,
				&tc_info->encap_ht_params, encap_node);
	return rc;
}

static int __bnxt_tc_del_flow_tf(struct bnxt *bp, void *flow)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tf_flow_node *flow_node = flow;
	int rc;

	rc = bnxt_ulp_flow_destroy(bp, flow_node->flow_id,
				   flow_node->ulp_src_fid, flow_node->dscp_remap);
	if (rc) {
		if (rc != -ENOENT)
			netdev_err(bp->dev,
				   "Failed to destroy flow: cookie:0x%lx src_fid:0x%x error:%d\n",
				   flow_node->key.cookie, flow_node->ulp_src_fid, rc);
		else
			netdev_dbg(bp->dev,
				   "Failed to destroy flow: cookie:0x%lx src_fid:0x%x error:%d\n",
				   flow_node->key.cookie, flow_node->ulp_src_fid, rc);
	}

	/* Release references to any tunnel encap node */
	if (flow_node->encap_node)
		bnxt_tc_put_encap_node(bp, flow_node);

	rc = rhashtable_remove_fast(&tc_info->tf_flow_table, &flow_node->node,
				    tc_info->tf_flow_ht_params);
	if (rc)
		netdev_dbg(bp->dev, "Error: %s: rhashtable_remove_fast rc=%d\n",
			   __func__, rc);

	netdev_dbg(bp->dev,
		   "%s: cookie:0x%lx src_fid:%d flow_id:0x%x\n",
		    __func__, flow_node->key.cookie, flow_node->key.src_fid,
		   flow_node->flow_id);

	if (flow_node->mparms)
		bnxt_ulp_free_mapper_encap_mparams(flow_node->mparms);

	kfree_rcu(flow_node, rcu);
	return rc;
}

static int __bnxt_tc_del_flow(struct bnxt *bp, void *flow)
{
	if (BNXT_TRUFLOW_EN(bp))
		return __bnxt_tc_del_flow_tf(bp, flow);
	else
		return __bnxt_tc_del_flow_afm(bp, flow);
}

#define BNXT_BATCH_FLOWS_NUM		32

static void bnxt_tc_batch_flows_get(struct rhashtable_iter *iter,
				    void *batch_flows[],
				    int *num_flows)
{
	void *flow_node;
	int i = 0;

	rhashtable_walk_start(iter);
	while ((flow_node = rhashtable_walk_next(iter)) != NULL) {
		if (IS_ERR(flow_node))
			continue;

		batch_flows[i++] = flow_node;
		if (i >= BNXT_BATCH_FLOWS_NUM)
			break;
	}
	*num_flows = i;
	rhashtable_walk_stop(iter);
}

void bnxt_tc_flush_flows(struct bnxt *bp)
{
	void *batch_flow_nodes[BNXT_BATCH_FLOWS_NUM];
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct rhashtable_iter iter;
	int i, num_flows;

	mutex_lock(&tc_info->lock);
	num_flows = atomic_read(&tc_info->flow_table.nelems);
	if (!num_flows) {
		mutex_unlock(&tc_info->lock);
		return;
	}

	netdev_warn(bp->dev, "Flushing offloaded flows\n");
	rhashtable_walk_enter(&tc_info->flow_table, &iter);
	do {
		bnxt_tc_batch_flows_get(&iter, batch_flow_nodes, &num_flows);
		for (i = 0; i < num_flows; i++)
			__bnxt_tc_del_flow(bp, batch_flow_nodes[i]);
	} while (num_flows != 0);
	rhashtable_walk_exit(&iter);
	mutex_unlock(&tc_info->lock);
}

static void bnxt_tc_set_l2_dir_fid(struct bnxt *bp, struct bnxt_tc_flow *flow,
				   u16 src_fid)
{
	flow->l2_key.dir = (bp->pf.fw_fid == src_fid) ? BNXT_DIR_RX : BNXT_DIR_TX;
	/* Add src_fid to l2 key field for egress tc flower flows, it will
	 * make sure that egress flow entries from different representor
	 * port have different HW entries for the L2 lookup stage.
	 */
	if (flow->l2_key.dir == BNXT_DIR_TX)
		flow->l2_key.src_fid = flow->src_fid;
}

static void bnxt_tc_set_src_fid(struct bnxt* bp, struct bnxt_tc_flow *flow,
				u16 src_fid)
{
	if (flow->actions.flags & BNXT_TC_ACTION_FLAG_TUNNEL_DECAP)
		flow->src_fid = bp->pf.fw_fid;
	else
		flow->src_fid = src_fid;
}

/* Add a new flow or replace an existing flow.
 * Notes on locking:
 * There are essentially two critical sections here.
 * 1. while adding a new flow
 *    a) lookup l2-key
 *    b) issue HWRM cmd and get flow_handle
 *    c) link l2-key with flow
 * 2. while deleting a flow
 *    a) unlinking l2-key from flow
 * A lock is needed to protect these two critical sections.
 *
 * The hash-tables are already protected by the rhashtable API.
 */
#ifdef HAVE_TC_CB_EGDEV
static int bnxt_tc_add_flow_afm(struct bnxt *bp, u16 src_fid,
				struct flow_cls_offload *tc_flow_cmd,
				int tc_dev_dir)
#else
static int bnxt_tc_add_flow_afm(struct bnxt *bp, u16 src_fid,
				struct flow_cls_offload *tc_flow_cmd)
#endif
{
	struct bnxt_tc_flow_node *new_node, *old_node;
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_flow *flow;
	__le32 tunnel_handle = 0;
	__le16 ref_flow_handle;
	int rc = 0;

	/* Configure tc flower on vxlan interface, it will iterate all BRCM
	 * interfaces, function bnxt_tc_parse_flow will generate an error log
	 * on interfaces which don't enable switchdev mode, need check
	 * switchdev mode before call this function to avoid error log.
	 */
	if (!bnxt_tc_is_switchdev_mode(bp))
		return -EINVAL;

	/* allocate memory for the new flow and it's node */
	new_node = kzalloc(sizeof(*new_node), GFP_KERNEL);
	if (!new_node) {
		rc = -ENOMEM;
		goto done;
	}
	new_node->key.cookie = tc_flow_cmd->cookie;
#ifdef HAVE_TC_CB_EGDEV
	new_node->tc_dev_dir = tc_dev_dir;
#endif
	flow = &new_node->flow;

	rc = bnxt_tc_parse_flow(bp, tc_flow_cmd, flow);
	if (rc)
		goto free_node;

	bnxt_tc_set_src_fid(bp, flow, src_fid);
	bnxt_tc_set_l2_dir_fid(bp, flow, flow->src_fid);
	new_node->key.src_fid = flow->src_fid;

	if (!bnxt_tc_can_offload(bp, flow)) {
		rc = -EOPNOTSUPP;
		kfree_rcu(new_node, rcu);
		return rc;
	}

	mutex_lock(&tc_info->lock);
	/* Synchronize with switchdev mode change via sriov_disable() */
	if (!bnxt_tc_is_switchdev_mode(bp)) {
		mutex_unlock(&tc_info->lock);
		kfree_rcu(new_node, rcu);
		return -EINVAL;
	}
	/* If a flow exists with the same key, delete it */
	old_node = rhashtable_lookup_fast(&tc_info->flow_table,
					  &new_node->key,
					  tc_info->flow_ht_params);
	if (old_node) {
#ifdef HAVE_TC_CB_EGDEV
		if (old_node->tc_dev_dir != tc_dev_dir) {
			/* This happens when TC invokes flow-add for the same
			 * flow a second time through egress dev (e.g, in the
			 * case of VF-VF, VF-Uplink flows). Ignore it and
			 * return success.
			 */
			goto unlock;
		}
#endif
		__bnxt_tc_del_flow(bp, old_node);
	}

	/* Check if the L2 part of the flow has been offloaded already.
	 * If so, bump up it's refcnt and get it's reference handle.
	 */
	rc = bnxt_tc_get_ref_flow_handle(bp, flow, new_node, &ref_flow_handle);
	if (rc)
		goto unlock;

	/* If the flow involves tunnel encap/decap, get tunnel_handle */
	rc = bnxt_tc_get_tunnel_handle(bp, flow, new_node, &tunnel_handle);
	if (rc)
		goto put_l2;

	/* send HWRM cmd to alloc the flow */
	rc = bnxt_hwrm_cfa_flow_alloc(bp, flow, ref_flow_handle,
				      tunnel_handle, new_node);
	if (rc)
		goto put_tunnel;

	flow->lastused = jiffies;
	spin_lock_init(&flow->stats_lock);
	/* add new flow to flow-table */
	rc = rhashtable_insert_fast(&tc_info->flow_table, &new_node->node,
				    tc_info->flow_ht_params);
	if (rc)
		goto hwrm_flow_free;

	mutex_unlock(&tc_info->lock);
	return 0;

hwrm_flow_free:
	bnxt_hwrm_cfa_flow_free(bp, new_node);
put_tunnel:
	bnxt_tc_put_tunnel_handle(bp, flow, new_node);
put_l2:
	bnxt_tc_put_l2_node(bp, new_node);
unlock:
	mutex_unlock(&tc_info->lock);
free_node:
	kfree_rcu(new_node, rcu);
done:
	if (rc == -ENOSPC)
		net_info_ratelimited("%s %s: No resources for new flow, cookie=0x%lx error=%d\n",
				     bp->dev->name, __func__, tc_flow_cmd->cookie, rc);
	else if (rc)
		netdev_err(bp->dev, "Error: %s: cookie=0x%lx error=%d\n",
			   __func__, tc_flow_cmd->cookie, rc);
	return rc;
}

#ifdef HAVE_TC_CB_EGDEV

static bool bnxt_tc_is_action_decap(struct flow_cls_offload *tc_flow_cmd)
{
	struct tcf_exts *tc_exts = tc_flow_cmd->exts;
	struct tc_action *tc_act;
#ifndef HAVE_TC_EXTS_FOR_ACTION
	LIST_HEAD(tc_actions);
#else
	int i;
#endif

#ifndef HAVE_TC_EXTS_FOR_ACTION
	tcf_exts_to_list(tc_exts, &tc_actions);
	list_for_each_entry(tc_act, &tc_actions, list) {
#else
	tcf_exts_for_each_action(i, tc_act, tc_exts) {
#endif
		if (is_tcf_tunnel_release(tc_act))
			return true;
	}

	return false;
}

#endif

#ifdef HAVE_TC_CB_EGDEV
static int bnxt_tc_add_flow_tf(struct bnxt *bp, u16 src_fid,
			       struct flow_cls_offload *tc_flow_cmd,
			       int tc_dev_dir)
#else
static int bnxt_tc_add_flow_tf(struct bnxt *bp, u16 src_fid,
			       struct flow_cls_offload *tc_flow_cmd)
#endif
{
	struct bnxt_tf_flow_node *new_node, *old_node;
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct bnxt_tc_info *tc_info = bp->tc_info;
	int rc = 0;

	/* Allocate memory for the new flow and it's node */
	new_node = kzalloc(sizeof(*new_node), GFP_KERNEL);
	if (!new_node) {
		rc = -ENOMEM;
		goto done;
	}

	new_node->key.cookie = tc_flow_cmd->cookie;

#ifdef HAVE_TC_CB_EGDEV
	new_node->tc_dev_dir = tc_dev_dir;

	/* If it is a decap-flow offloaded on the egress dev, then the
	 * actual src_fid must be that of the PF since it is really an
	 * ingress flow. Pass the right src_fid to the ULP. But use the
	 * VF's src_fid in the flow_node key, since we need that to lookup
	 * the flow in flow_stats() and del_flow(). This is the only case
	 * in which the src_fid in the flow_node key and the src_fid passed
	 * to the ULP are different.
	 */
	new_node->ulp_src_fid = bnxt_tc_is_action_decap(tc_flow_cmd) ?
			bp->pf.fw_fid : src_fid;
#else
	new_node->ulp_src_fid = src_fid;
#endif
	new_node->key.src_fid = src_fid;

	mutex_lock(&tc_info->lock);

	if (!bnxt_tc_flower_enabled(bp)) {
		rc = -EOPNOTSUPP;
		goto unlock;
	}

	/* Synchronize with switchdev mode change via sriov_disable() */
	if (!bnxt_tc_is_switchdev_mode(bp)) {
		rc = -EOPNOTSUPP;
		goto unlock;
	}

	/* If a flow exists with the same cookie, delete it */
	old_node = rhashtable_lookup_fast(&tc_info->tf_flow_table,
					  &new_node->key,
					  tc_info->tf_flow_ht_params);
	if (old_node) {
#ifdef HAVE_TC_CB_EGDEV
		/* This happens when TC invokes flow-add for the same
		 * flow a second time through egress dev (e.g, in the
		 * case of VF-VF, VF-Uplink flows). Ignore it and
		 * return success.
		 */
		if (old_node->tc_dev_dir != tc_dev_dir)
			goto unlock;
#endif
		__bnxt_tc_del_flow(bp, old_node);
	}

	rc = bnxt_ulp_flow_create(bp, new_node->ulp_src_fid, tc_flow_cmd,
				  &flow_info);
	if (rc)
		goto unlock;

	new_node->mparms = flow_info.mparms;
	new_node->flow_id = flow_info.flow_id;
	new_node->dscp_remap = flow_info.dscp_remap;
	netdev_dbg(bp->dev,
		   "%s: cookie:0x%lx src_fid:0x%x flow_id:0x%x\n",
		   __func__, tc_flow_cmd->cookie, src_fid, flow_info.flow_id);

	if (flow_info.encap_key) {
		rc = bnxt_tc_get_encap_node(bp, new_node, &flow_info);
		if (rc)
			goto free_flow;
	}

	/* add new flow to flow-table */
	rc = rhashtable_insert_fast(&tc_info->tf_flow_table, &new_node->node,
				    tc_info->tf_flow_ht_params);
	if (rc)
		goto put_encap;

	mutex_unlock(&tc_info->lock);

	/* flow_info.mparms will be freed during flow destroy */
	vfree(flow_info.encap_key);
	vfree(flow_info.neigh_key);
	return 0;

put_encap:
	if (flow_info.encap_key)
		bnxt_tc_put_encap_node(bp, new_node);
free_flow:
	bnxt_ulp_flow_destroy(bp, new_node->flow_id, new_node->ulp_src_fid,
			      new_node->dscp_remap);
	if (flow_info.encap_key) {
		vfree(flow_info.encap_key);
		vfree(flow_info.neigh_key);
		vfree(flow_info.mparms);
	}
unlock:
	mutex_unlock(&tc_info->lock);
	kfree_rcu(new_node, rcu);
done:
	if (rc == -ENOSPC)
		net_info_ratelimited("%s: No HW resources for new flow: cookie=0x%lx error=%d\n",
				     bp->dev->name, tc_flow_cmd->cookie, rc);
	else if (rc && rc != -EOPNOTSUPP)
		netdev_err(bp->dev,
			   "Failed to create flow: cookie:0x%lx src_fid:0x%x error:%d\n",
			   tc_flow_cmd->cookie, src_fid, rc);
	return rc;
}

#ifdef HAVE_TC_CB_EGDEV
static int bnxt_tc_add_flow(struct bnxt *bp, u16 src_fid,
			    struct flow_cls_offload *tc_flow_cmd,
			    int tc_dev_dir)
#else
static int bnxt_tc_add_flow(struct bnxt *bp, u16 src_fid,
			    struct flow_cls_offload *tc_flow_cmd)
#endif
{
	int rc;

#ifdef HAVE_TC_CB_EGDEV
	if (BNXT_TRUFLOW_EN(bp))
		rc = bnxt_tc_add_flow_tf(bp, src_fid, tc_flow_cmd, tc_dev_dir);
	else
		rc = bnxt_tc_add_flow_afm(bp, src_fid, tc_flow_cmd, tc_dev_dir);
#else
	if (BNXT_TRUFLOW_EN(bp))
		rc = bnxt_tc_add_flow_tf(bp, src_fid, tc_flow_cmd);
	else
		rc = bnxt_tc_add_flow_afm(bp, src_fid, tc_flow_cmd);
#endif

	return rc;
}

#ifdef HAVE_TC_CB_EGDEV
static int bnxt_tc_del_flow_afm(struct bnxt *bp, u16 src_fid,
				struct flow_cls_offload *tc_flow_cmd,
				int tc_dev_dir)
#else
static int bnxt_tc_del_flow_afm(struct bnxt *bp, u16 src_fid,
				struct flow_cls_offload *tc_flow_cmd)
#endif
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_flow_node *flow_node;
	struct bnxt_tc_flow_node_key flow_key;
	int rc;

	memset(&flow_key, 0, sizeof(flow_key));
	flow_key.cookie = tc_flow_cmd->cookie;
	flow_key.src_fid = src_fid;
	mutex_lock(&tc_info->lock);
	flow_node = rhashtable_lookup_fast(&tc_info->flow_table,
					   &flow_key,
					   tc_info->flow_ht_params);
#ifdef HAVE_TC_CB_EGDEV
	if (!flow_node || flow_node->tc_dev_dir != tc_dev_dir) {
#else
	if (!flow_node) {
#endif
		mutex_unlock(&tc_info->lock);
		return -EINVAL;
	}

	rc = __bnxt_tc_del_flow(bp, flow_node);
	mutex_unlock(&tc_info->lock);

	return rc;
}

#ifdef HAVE_TC_CB_EGDEV
static int bnxt_tc_del_flow_tf(struct bnxt *bp, u16 src_fid,
			       struct flow_cls_offload *tc_flow_cmd,
			       int tc_dev_dir)
#else
static int bnxt_tc_del_flow_tf(struct bnxt *bp, u16 src_fid,
			       struct flow_cls_offload *tc_flow_cmd)
#endif
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_flow_node_key flow_key;
	struct bnxt_tf_flow_node *flow_node;
	int rc;

	memset(&flow_key, 0, sizeof(flow_key));
	flow_key.cookie = tc_flow_cmd->cookie;
	flow_key.src_fid = src_fid;

	mutex_lock(&tc_info->lock);
	if (!bnxt_tc_flower_enabled(bp)) {
		rc = -EOPNOTSUPP;
		goto unlock;
	}
	flow_node = rhashtable_lookup_fast(&tc_info->tf_flow_table,
					   &flow_key,
					   tc_info->tf_flow_ht_params);
#ifdef HAVE_TC_CB_EGDEV
	if (!flow_node || flow_node->tc_dev_dir != tc_dev_dir) {
#else
	if (!flow_node) {
#endif
		mutex_unlock(&tc_info->lock);
		return -EINVAL;
	}

	rc = __bnxt_tc_del_flow(bp, flow_node);

unlock:
	mutex_unlock(&tc_info->lock);
	return rc;
}

#ifdef HAVE_TC_CB_EGDEV
static int bnxt_tc_del_flow(struct bnxt *bp, u16 src_fid,
			    struct flow_cls_offload *tc_flow_cmd,
			    int tc_dev_dir)
#else
static int bnxt_tc_del_flow(struct bnxt *bp, u16 src_fid,
			    struct flow_cls_offload *tc_flow_cmd)
#endif
{
	int rc;

#ifdef HAVE_TC_CB_EGDEV
	if (BNXT_TRUFLOW_EN(bp))
		rc = bnxt_tc_del_flow_tf(bp, src_fid, tc_flow_cmd,
					 tc_dev_dir);
	else
		rc = bnxt_tc_del_flow_afm(bp, src_fid, tc_flow_cmd, tc_dev_dir);
#else
	if (BNXT_TRUFLOW_EN(bp))
		rc = bnxt_tc_del_flow_tf(bp, src_fid, tc_flow_cmd);
	else
		rc = bnxt_tc_del_flow_afm(bp, src_fid, tc_flow_cmd);
#endif

	return rc;
}

#ifdef HAVE_TC_CB_EGDEV
static int bnxt_tc_get_flow_stats_afm(struct bnxt *bp, u16 src_fid,
				      struct flow_cls_offload *tc_flow_cmd,
				      int tc_dev_dir)
#else
static int bnxt_tc_get_flow_stats_afm(struct bnxt *bp, u16 src_fid,
				      struct flow_cls_offload *tc_flow_cmd)
#endif
{
	struct bnxt_tc_flow_stats stats, *curr_stats, *prev_stats;
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_flow_node_key flow_key;
	struct bnxt_tc_flow_node *flow_node;
	struct bnxt_tc_flow *flow;
	unsigned long lastused;

	memset(&flow_key, 0, sizeof(flow_key));
	flow_key.cookie = tc_flow_cmd->cookie;
	flow_key.src_fid = src_fid;

	mutex_lock(&tc_info->lock);
	flow_node = rhashtable_lookup_fast(&tc_info->flow_table,
					   &flow_key,
					   tc_info->flow_ht_params);
#ifdef HAVE_TC_CB_EGDEV
	if (!flow_node || flow_node->tc_dev_dir != tc_dev_dir) {
#else
	if (!flow_node) {
#endif
		mutex_unlock(&tc_info->lock);
		return -1;
	}

	flow = &flow_node->flow;
	curr_stats = &flow->stats;
	prev_stats = &flow->prev_stats;

	spin_lock(&flow->stats_lock);
	stats.packets = curr_stats->packets - prev_stats->packets;
	stats.bytes = curr_stats->bytes - prev_stats->bytes;
	*prev_stats = *curr_stats;
	lastused = flow->lastused;
	spin_unlock(&flow->stats_lock);

#if defined(HAVE_FLOW_OFFLOAD_H) && defined(HAVE_FLOW_STATS_UPDATE)
	flow_stats_update(&tc_flow_cmd->stats, stats.bytes, stats.packets, 0,
			  lastused, FLOW_ACTION_HW_STATS_DELAYED);
#else
	tcf_exts_stats_update(tc_flow_cmd->exts, stats.bytes, stats.packets,
			      lastused);
#endif
	mutex_unlock(&tc_info->lock);
	return 0;
}

#ifdef HAVE_TC_CB_EGDEV
static int bnxt_tc_get_flow_stats_tf(struct bnxt *bp, u16 src_fid,
				     struct flow_cls_offload *tc_flow_cmd,
				     int tc_dev_dir)
#else
static int bnxt_tc_get_flow_stats_tf(struct bnxt *bp, u16 src_fid,
				     struct flow_cls_offload *tc_flow_cmd)
#endif
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_tc_flow_node_key flow_key;
	struct bnxt_tf_flow_node *flow_node;
	u64 packets = 0, bytes = 0;
	unsigned long lastused = 0;

	memset(&flow_key, 0, sizeof(flow_key));
	flow_key.cookie = tc_flow_cmd->cookie;
	flow_key.src_fid = src_fid;

	mutex_lock(&tc_info->lock);
	if (!bnxt_tc_flower_enabled(bp)) {
		mutex_unlock(&tc_info->lock);
		return -1;
	}
	flow_node = rhashtable_lookup_fast(&tc_info->tf_flow_table,
					   &flow_key,
					   tc_info->tf_flow_ht_params);
#ifdef HAVE_TC_CB_EGDEV
	if (!flow_node || flow_node->tc_dev_dir != tc_dev_dir) {
#else
	if (!flow_node) {
#endif
		mutex_unlock(&tc_info->lock);
		return -1;
	}

	bnxt_ulp_flow_query_count(bp, flow_node->flow_id, &packets,
				  &bytes, &lastused);

#if defined(HAVE_FLOW_OFFLOAD_H) && defined(HAVE_FLOW_STATS_UPDATE)
	flow_stats_update(&tc_flow_cmd->stats, bytes, packets, 0,
			  lastused, FLOW_ACTION_HW_STATS_DELAYED);
#else
	tcf_exts_stats_update(tc_flow_cmd->exts, bytes, packets,
			      lastused);
#endif
	mutex_unlock(&tc_info->lock);
	return 0;
}

#ifdef HAVE_TC_CB_EGDEV
static int bnxt_tc_get_flow_stats(struct bnxt *bp, u16 src_fid,
				  struct flow_cls_offload *tc_flow_cmd,
				  int tc_dev_dir)
{
	if (BNXT_TRUFLOW_EN(bp))
		return bnxt_tc_get_flow_stats_tf(bp, src_fid, tc_flow_cmd,
						 tc_dev_dir);
	else
		return bnxt_tc_get_flow_stats_afm(bp, src_fid, tc_flow_cmd,
						  tc_dev_dir);
}
#else
static int bnxt_tc_get_flow_stats(struct bnxt *bp, u16 src_fid,
				  struct flow_cls_offload *tc_flow_cmd)
{
	if (BNXT_TRUFLOW_EN(bp))
		return bnxt_tc_get_flow_stats_tf(bp, src_fid, tc_flow_cmd);
	else
		return bnxt_tc_get_flow_stats_afm(bp, src_fid, tc_flow_cmd);
}
#endif

static void bnxt_fill_cfa_stats_req(struct bnxt *bp,
				    struct bnxt_tc_flow_node *flow_node,
				    __le16 *flow_handle, __le32 *flow_id)
{
	u16 handle;

	if (bp->fw_cap & BNXT_FW_CAP_OVS_64BIT_HANDLE) {
		*flow_id = flow_node->flow_id;

		/* If flow_id is used to fetch flow stats then:
		 * 1. lower 12 bits of flow_handle must be set to all 1s.
		 * 2. 15th bit of flow_handle must specify the flow
		 *    direction (TX/RX).
		 */
		if (flow_node->flow.l2_key.dir == BNXT_DIR_RX)
			handle = CFA_FLOW_INFO_REQ_FLOW_HANDLE_DIR_RX |
				 CFA_FLOW_INFO_REQ_FLOW_HANDLE_MAX_MASK;
		else
			handle = CFA_FLOW_INFO_REQ_FLOW_HANDLE_MAX_MASK;

		*flow_handle = cpu_to_le16(handle);
	} else {
		*flow_handle = flow_node->flow_handle;
	}
}

static int
bnxt_hwrm_cfa_flow_stats_get(struct bnxt *bp, int num_flows,
			     struct bnxt_tc_stats_batch stats_batch[])
{
	struct hwrm_cfa_flow_stats_output *resp;
	struct hwrm_cfa_flow_stats_input *req;
	__le16 *req_flow_handles;
	__le32 *req_flow_ids;
	int rc, i;

	rc = hwrm_req_init(bp, req, HWRM_CFA_FLOW_STATS);
	if (rc)
		goto exit;

	req_flow_handles = &req->flow_handle_0;
	req_flow_ids = &req->flow_id_0;

	req->num_flows = cpu_to_le16(num_flows);
	for (i = 0; i < num_flows; i++) {
		struct bnxt_tc_flow_node *flow_node = stats_batch[i].flow_node;

		bnxt_fill_cfa_stats_req(bp, flow_node,
					&req_flow_handles[i], &req_flow_ids[i]);
	}

	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (!rc) {
		__le64 *resp_packets;
		__le64 *resp_bytes;

		resp_packets = &resp->packet_0;
		resp_bytes = &resp->byte_0;

		for (i = 0; i < num_flows; i++) {
			stats_batch[i].hw_stats.packets =
						le64_to_cpu(resp_packets[i]);
			stats_batch[i].hw_stats.bytes =
						le64_to_cpu(resp_bytes[i]);
		}
	}
	hwrm_req_drop(bp, req);
exit:
	if (rc)
		netdev_info(bp->dev, "error rc=%d\n", rc);

	return rc;
}

/*
 * Add val to accum while handling a possible wraparound
 * of val. Even though val is of type u64, its actual width
 * is denoted by mask and will wrap-around beyond that width.
 */
static void accumulate_val(u64 *accum, u64 val, u64 mask)
{
#define low_bits(x, mask)		((x) & (mask))
#define high_bits(x, mask)		((x) & ~(mask))
	bool wrapped = val < low_bits(*accum, mask);

	*accum = high_bits(*accum, mask) + val;
	if (wrapped)
		*accum += (mask + 1);
}

/* The HW counters' width is much less than 64bits.
 * Handle possible wrap-around while updating the stat counters
 */
static void bnxt_flow_stats_accum(struct bnxt_tc_info *tc_info,
				  struct bnxt_tc_flow_stats *acc_stats,
				  struct bnxt_tc_flow_stats *hw_stats)
{
	accumulate_val(&acc_stats->bytes, hw_stats->bytes, tc_info->bytes_mask);
	accumulate_val(&acc_stats->packets, hw_stats->packets,
		       tc_info->packets_mask);
}

static int
bnxt_tc_flow_stats_batch_update(struct bnxt *bp, int num_flows,
				struct bnxt_tc_stats_batch stats_batch[])
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	int rc, i;

	rc = bnxt_hwrm_cfa_flow_stats_get(bp, num_flows, stats_batch);
	if (rc)
		return rc;

	for (i = 0; i < num_flows; i++) {
		struct bnxt_tc_flow_node *flow_node = stats_batch[i].flow_node;
		struct bnxt_tc_flow *flow = &flow_node->flow;

		spin_lock(&flow->stats_lock);
		bnxt_flow_stats_accum(tc_info, &flow->stats,
				      &stats_batch[i].hw_stats);
		if (flow->stats.packets != flow->prev_stats.packets)
			flow->lastused = jiffies;
		spin_unlock(&flow->stats_lock);
	}

	return 0;
}

static int
bnxt_tc_flow_stats_batch_prep(struct bnxt *bp,
			      struct bnxt_tc_stats_batch stats_batch[],
			      int *num_flows)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct rhashtable_iter *iter = &tc_info->iter;
	void *flow_node;
	int rc, i;

	rhashtable_walk_start(iter);

	rc = 0;
	for (i = 0; i < BNXT_FLOW_STATS_BATCH_MAX; i++) {
		flow_node = rhashtable_walk_next(iter);
		if (IS_ERR(flow_node)) {
			i = 0;
			if (PTR_ERR(flow_node) == -EAGAIN) {
				continue;
			} else {
				rc = PTR_ERR(flow_node);
				goto done;
			}
		}

		/* No more flows */
		if (!flow_node)
			goto done;

		stats_batch[i].flow_node = flow_node;
	}
done:
	rhashtable_walk_stop(iter);
	*num_flows = i;
	return rc;
}

void bnxt_tc_flow_stats_work(struct bnxt *bp)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	int num_flows, rc;

	mutex_lock(&tc_info->lock);
	num_flows = atomic_read(&tc_info->flow_table.nelems);
	if (!num_flows) {
		mutex_unlock(&tc_info->lock);
		return;
	}
	rhashtable_walk_enter(&tc_info->flow_table, &tc_info->iter);

	for (;;) {
		rc = bnxt_tc_flow_stats_batch_prep(bp, tc_info->stats_batch,
						   &num_flows);
		if (rc) {
			if (rc == -EAGAIN)
				continue;
			break;
		}

		if (!num_flows)
			break;

		bnxt_tc_flow_stats_batch_update(bp, num_flows,
						tc_info->stats_batch);
	}

	rhashtable_walk_exit(&tc_info->iter);
	mutex_unlock(&tc_info->lock);
}

#ifdef HAVE_TC_SETUP_BLOCK
static bool bnxt_tc_can_offload_and_chain(struct bnxt *bp, u16 src_fid,
					  struct flow_cls_offload *cls_flower)
{
	bool can = true;
	u32 chain_index;

	if (!BNXT_TRUFLOW_EN(bp))
		return tc_cls_can_offload_and_chain0(bp->dev,
						     (void *)cls_flower);

	can = tc_can_offload(bp->dev);
	if (!can) {
		NL_SET_ERR_MSG_MOD(cls_flower->common.extack,
				   "TC offload is disabled on net device");
		return can;
	}

	chain_index = cls_flower->common.chain_index;
	if (!chain_index)
		return can;

	can = bnxt_ulp_flow_chain_validate(bp, src_fid, cls_flower);
	if (!can)
		NL_SET_ERR_MSG_MOD(cls_flower->common.extack,
				   "Driver supports only offload of chain 0");
	return can;
}
#endif

#ifdef HAVE_TC_CB_EGDEV
int bnxt_tc_setup_flower(struct bnxt *bp, u16 src_fid,
			 struct flow_cls_offload *cls_flower,
			 int tc_dev_dir)
{
#ifdef HAVE_TC_SETUP_TYPE
#ifndef HAVE_TC_SETUP_BLOCK
	if (!is_classid_clsact_ingress(cls_flower->common.classid))
		return -EOPNOTSUPP;
#else
	if (!bnxt_tc_can_offload_and_chain(bp, src_fid, cls_flower))
		return -EOPNOTSUPP;
#endif
#endif
	switch (cls_flower->command) {
	case FLOW_CLS_REPLACE:
		return bnxt_tc_add_flow(bp, src_fid, cls_flower, tc_dev_dir);
	case FLOW_CLS_DESTROY:
		return bnxt_tc_del_flow(bp, src_fid, cls_flower, tc_dev_dir);
	case FLOW_CLS_STATS:
		return bnxt_tc_get_flow_stats(bp, src_fid, cls_flower, tc_dev_dir);
	default:
		return -EOPNOTSUPP;
	}
}

#else

int bnxt_tc_setup_flower(struct bnxt *bp, u16 src_fid,
			 struct flow_cls_offload *cls_flower)
{
#ifdef HAVE_TC_SETUP_TYPE
#ifndef HAVE_TC_SETUP_BLOCK
	if (!is_classid_clsact_ingress(cls_flower->common.classid))
		return -EOPNOTSUPP;
#else
	if (!bnxt_tc_can_offload_and_chain(bp, src_fid, cls_flower))
		return -EOPNOTSUPP;
#endif
#endif
	switch (cls_flower->command) {
	case FLOW_CLS_REPLACE:
		return bnxt_tc_add_flow(bp, src_fid, cls_flower);
	case FLOW_CLS_DESTROY:
		return bnxt_tc_del_flow(bp, src_fid, cls_flower);
	case FLOW_CLS_STATS:
		return bnxt_tc_get_flow_stats(bp, src_fid, cls_flower);
	default:
		return -EOPNOTSUPP;
	}
}
#endif /* HAVE_TC_CB_EGDEV */

#ifdef HAVE_TC_SETUP_TYPE
#ifdef HAVE_TC_SETUP_BLOCK
#ifdef HAVE_FLOW_INDR_BLOCK_CB

static int bnxt_tc_setup_indr_block_cb(enum tc_setup_type type,
				       void *type_data, void *cb_priv)
{
	struct bnxt_flower_indr_block_cb_priv *priv = cb_priv;
	struct flow_cls_offload *flower = type_data;
	struct bnxt *bp = priv->bp;

	if (!tc_cls_can_offload_and_chain0(bp->dev, type_data))
		return -EOPNOTSUPP;

	switch (type) {
	case TC_SETUP_CLSFLOWER:
#ifdef HAVE_TC_CB_EGDEV
		return bnxt_tc_setup_flower(bp, bp->pf.fw_fid, flower,
					    BNXT_TC_DEV_INGRESS);
#else
		return bnxt_tc_setup_flower(bp, bp->pf.fw_fid, flower);
#endif
	default:
		return -EOPNOTSUPP;
	}
}

static struct bnxt_flower_indr_block_cb_priv *
bnxt_tc_indr_block_cb_lookup(struct bnxt *bp, struct net_device *netdev)
{
	struct bnxt_flower_indr_block_cb_priv *cb_priv;

#ifndef HAVE_FLOW_INDIR_BLK_PROTECTION
	/* All callback list access should be protected by RTNL. */
	ASSERT_RTNL();
#endif

	list_for_each_entry(cb_priv, &bp->tc_indr_block_list, list)
		if (cb_priv->tunnel_netdev == netdev)
			return cb_priv;

	return NULL;
}

static void bnxt_tc_setup_indr_rel(void *cb_priv)
{
	struct bnxt_flower_indr_block_cb_priv *priv = cb_priv;

	list_del(&priv->list);
	kfree(priv);
}

/* Ensure that the indirect block offload request is for
 * this PF, by comparing with the lower_dev of vxlan-dev.
 */
static bool bnxt_is_vxlan_lower_dev(struct net_device *vxlan_netdev,
				    struct bnxt *bp)
{
	const struct vxlan_dev *vxlan = netdev_priv(vxlan_netdev);
	const struct vxlan_rdst *dst = &vxlan->default_dst;

#ifdef HAVE_VXLAN_RDST_RDEV
	if (dst->remote_dev)
		return bp->dev == dst->remote_dev;
#else
	if (dst->remote_ifindex)
		return (bp->dev == __dev_get_by_index(dev_net(bp->dev),
						      dst->remote_ifindex));
#endif
	/* If lower dev is not specified, this vxlan interface
	 * could be a vport device. Let the offload go through.
	 */
	return true;
}

extern struct list_head bnxt_block_cb_list;
#ifdef HAVE_FLOW_INDR_BLOCK_CB_QDISC
static int bnxt_tc_setup_indr_block(struct net_device *netdev, struct Qdisc *sch, struct bnxt *bp,
				    struct flow_block_offload *f, void *data,
				    void (*cleanup)(struct flow_block_cb *block_cb))
#else
static int bnxt_tc_setup_indr_block(struct net_device *netdev,
				    struct bnxt *bp,
				    struct flow_block_offload *f, void *data,
				    void (*cleanup)(struct flow_block_cb *block_cb))
#endif
{
	struct bnxt_flower_indr_block_cb_priv *cb_priv;
	struct flow_block_cb *block_cb;

	if (f->binder_type != FLOW_BLOCK_BINDER_TYPE_CLSACT_INGRESS)
		return -EOPNOTSUPP;

	if (!bnxt_is_vxlan_lower_dev(netdev, bp))
		return -EOPNOTSUPP;

	switch (f->command) {
	case FLOW_BLOCK_BIND:
		cb_priv = kmalloc(sizeof(*cb_priv), GFP_KERNEL);
		if (!cb_priv)
			return -ENOMEM;

		cb_priv->tunnel_netdev = netdev;
		cb_priv->bp = bp;
		list_add(&cb_priv->list, &bp->tc_indr_block_list);

#ifdef HAVE_FLOW_INDR_BLOCK_CB_QDISC
		block_cb = flow_indr_block_cb_alloc(bnxt_tc_setup_indr_block_cb,
						    cb_priv, cb_priv,
						    bnxt_tc_setup_indr_rel, f,
						    netdev, sch, data, bp, cleanup);
#else
		block_cb = flow_indr_block_cb_alloc(bnxt_tc_setup_indr_block_cb,
						    cb_priv, cb_priv,
						    bnxt_tc_setup_indr_rel, f,
						    netdev, data, bp, cleanup);
#endif
		if (IS_ERR(block_cb)) {
			list_del(&cb_priv->list);
			kfree(cb_priv);
			return PTR_ERR(block_cb);
		}

		flow_block_cb_add(block_cb, f);
		list_add_tail(&block_cb->driver_list, &bnxt_block_cb_list);
		break;
	case FLOW_BLOCK_UNBIND:
		cb_priv = bnxt_tc_indr_block_cb_lookup(bp, netdev);
		if (!cb_priv)
			return -ENOENT;

		block_cb = flow_block_cb_lookup(f->block,
						bnxt_tc_setup_indr_block_cb,
						cb_priv);
		if (!block_cb)
			return -ENOENT;

		flow_indr_block_cb_remove(block_cb, f);
		list_del(&block_cb->driver_list);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static bool bnxt_is_netdev_indr_offload(struct net_device *netdev)
{
	return netif_is_vxlan(netdev);
}

#ifdef HAVE_FLOW_INDR_BLOCK_CB_QDISC
static int bnxt_tc_setup_indr_cb(struct net_device *netdev, struct Qdisc *sch, void *cb_priv,
				 enum tc_setup_type type, void *type_data,
				 void *data,
				 void (*cleanup)(struct flow_block_cb *block_cb))
#else
static int bnxt_tc_setup_indr_cb(struct net_device *netdev, void *cb_priv,
				 enum tc_setup_type type, void *type_data,
				 void *data,
				 void (*cleanup)(struct flow_block_cb *block_cb))
#endif
{
	if (!netdev || !bnxt_is_netdev_indr_offload(netdev))
		return -EOPNOTSUPP;

	switch (type) {
	case TC_SETUP_BLOCK:
#ifdef HAVE_FLOW_INDR_BLOCK_CB_QDISC
		return bnxt_tc_setup_indr_block(netdev, sch, cb_priv, type_data, data,
						cleanup);
#else
		return bnxt_tc_setup_indr_block(netdev, cb_priv, type_data, data,
						cleanup);
#endif
	default:
		break;
	}

	return -EOPNOTSUPP;
}

#ifndef HAVE_FLOW_INDR_DEV_RGTR
int bnxt_tc_indr_block_event(struct notifier_block *nb, unsigned long event,
			     void *ptr)
{
	struct net_device *netdev;
	struct bnxt *bp;
	int rc;

	netdev = netdev_notifier_info_to_dev(ptr);
	if (!bnxt_is_netdev_indr_offload(netdev))
		return NOTIFY_OK;

	bp = container_of(nb, struct bnxt, tc_netdev_nb);

	switch (event) {
	case NETDEV_REGISTER:
		rc = __flow_indr_block_cb_register(netdev, bp,
						   bnxt_tc_setup_indr_cb,
						   bp);
		if (rc)
			netdev_info(bp->dev,
				    "Failed to register indirect blk: dev: %s\n",
				    netdev->name);
		break;
	case NETDEV_UNREGISTER:
		__flow_indr_block_cb_unregister(netdev,
						bnxt_tc_setup_indr_cb,
						bp);
		break;
	}

	return NOTIFY_DONE;
}
#endif /* HAVE_FLOW_INDR_DEV_RGTR */
#endif /* HAVE_FLOW_INDR_BLOCK_CB */

#if defined(HAVE_TC_MATCHALL_FLOW_RULE) && defined(HAVE_FLOW_ACTION_POLICE)

static inline int bnxt_tc_find_vf_by_fid(struct bnxt *bp, u16 fid)
{
	int num_vfs = pci_num_vf(bp->pdev);
	int i;

	for (i = 0; i < num_vfs; i++) {
		if (bp->pf.vf[i].fw_fid == fid)
			break;
	}
	if (i >= num_vfs)
		return -EINVAL;
	return i;
}

static int bnxt_tc_del_matchall(struct bnxt *bp, u16 src_fid,
				struct tc_cls_matchall_offload *matchall_cmd)
{
	int vf_idx;

	vf_idx = bnxt_tc_find_vf_by_fid(bp, src_fid);
	if (vf_idx < 0)
		return vf_idx;

	if (bp->pf.vf[vf_idx].police_id != matchall_cmd->cookie)
		return -ENOENT;

	bnxt_set_vf_bw(bp->dev, vf_idx, 0, 0);
	bp->pf.vf[vf_idx].police_id = 0;
	return 0;
}

static int bnxt_tc_add_matchall(struct bnxt *bp, u16 src_fid,
				struct tc_cls_matchall_offload *matchall_cmd)
{
	struct flow_action_entry *action;
	int vf_idx;
	s64 burst;
	u64 rate;
	int rc;

	vf_idx = bnxt_tc_find_vf_by_fid(bp, src_fid);
	if (vf_idx < 0)
		return vf_idx;

	action = &matchall_cmd->rule->action.entries[0];
	if (action->id != FLOW_ACTION_POLICE) {
		netdev_err(bp->dev, "%s: Unsupported matchall action: %d",
			   __func__, action->id);
		return -EOPNOTSUPP;
	}
	if (bp->pf.vf[vf_idx].police_id && bp->pf.vf[vf_idx].police_id !=
	    matchall_cmd->cookie) {
		netdev_err(bp->dev,
			   "%s: Policer is already configured for VF: %d",
			   __func__, vf_idx);
		return -EEXIST;
	}

	rate = (u32)div_u64(action->police.rate_bytes_ps, 1024 * 1000) * 8;
	burst = (u32)div_u64(action->police.rate_bytes_ps *
			     PSCHED_NS2TICKS(action->police.burst),
			     PSCHED_TICKS_PER_SEC);
	burst = (u32)PSCHED_TICKS2NS(burst) / (1 << 20);

	rc = bnxt_set_vf_bw(bp->dev, vf_idx, burst, rate);
	if (rc) {
		netdev_err(bp->dev,
			   "Error: %s: VF: %d rate: %llu burst: %llu rc: %d",
			   __func__, vf_idx, rate, burst, rc);
		return rc;
	}

	bp->pf.vf[vf_idx].police_id = matchall_cmd->cookie;
	return 0;
}

int bnxt_tc_setup_matchall(struct bnxt *bp, u16 src_fid,
			   struct tc_cls_matchall_offload *cls_matchall)
{
	if (!tc_cls_can_offload_and_chain0(bp->dev, (void *)cls_matchall))
		return -EOPNOTSUPP;

	switch (cls_matchall->command) {
	case TC_CLSMATCHALL_REPLACE:
		return bnxt_tc_add_matchall(bp, src_fid, cls_matchall);
	case TC_CLSMATCHALL_DESTROY:
		return bnxt_tc_del_matchall(bp, src_fid, cls_matchall);
	default:
		return -EOPNOTSUPP;
	}
}

#endif /* HAVE_TC_MATCHALL_FLOW_RULE && HAVE_FLOW_ACTION_POLICE */
#endif /* HAVE_TC_SETUP_BLOCK */
#endif /* HAVE_TC_SETUP_TYPE */

static const struct rhashtable_params bnxt_tc_flow_ht_params = {
	.head_offset = offsetof(struct bnxt_tc_flow_node, node),
	.key_offset = offsetof(struct bnxt_tc_flow_node, key),
	.key_len = sizeof(struct bnxt_tc_flow_node_key),
	.automatic_shrinking = true
};

static const struct rhashtable_params bnxt_tf_flow_ht_params = {
	.head_offset = offsetof(struct bnxt_tf_flow_node, node),
	.key_offset = offsetof(struct bnxt_tf_flow_node, key),
	.key_len = sizeof(struct bnxt_tc_flow_node_key),
	.automatic_shrinking = true
};

static const struct rhashtable_params bnxt_tc_l2_ht_params = {
	.head_offset = offsetof(struct bnxt_tc_l2_node, node),
	.key_offset = offsetof(struct bnxt_tc_l2_node, key),
	.key_len = BNXT_TC_L2_KEY_LEN,
	.automatic_shrinking = true
};

static const struct rhashtable_params bnxt_tc_decap_l2_ht_params = {
	.head_offset = offsetof(struct bnxt_tc_l2_node, node),
	.key_offset = offsetof(struct bnxt_tc_l2_node, key),
	.key_len = BNXT_TC_L2_KEY_LEN,
	.automatic_shrinking = true
};

static const struct rhashtable_params bnxt_tc_tunnel_ht_params = {
	.head_offset = offsetof(struct bnxt_tc_tunnel_node, node),
	.key_offset = offsetof(struct bnxt_tc_tunnel_node, key),
	.key_len = sizeof(struct ip_tunnel_key),
	.automatic_shrinking = true
};

static const struct rhashtable_params bnxt_tc_neigh_ht_params = {
	.head_offset = offsetof(struct bnxt_tc_neigh_node, node),
	.key_offset = offsetof(struct bnxt_tc_neigh_node, key),
	.key_len = sizeof(struct bnxt_tc_neigh_key),
	.automatic_shrinking = true
};

static const struct rhashtable_params bnxt_ulp_udcc_v6_subnet_ht_params = {
	.head_offset = offsetof(struct bnxt_ulp_udcc_v6_subnet_node, node),
	.key_offset = offsetof(struct bnxt_ulp_udcc_v6_subnet_node, key),
	.key_len = sizeof(struct bnxt_ulp_udcc_v6_subnet_key),
	.automatic_shrinking = true
};

/* convert counter width in bits to a mask */
#define mask(width)		((u64)~0 >> (64 - (width)))

static int bnxt_rep_netevent_cb(struct notifier_block *nb,
				unsigned long event, void *ptr)
{
	struct bnxt *bp = container_of(nb, struct bnxt, neigh_update.netevent_nb);
	struct bnxt_tc_neigh_node *neigh_node;
	struct neighbour *n;

	switch (event) {
	case NETEVENT_NEIGH_UPDATE:
		n = ptr;
		neigh_node = bnxt_tc_lkup_neigh_node(bp, n);
		if (!neigh_node)
			break;

		/* We currently support serial processing of neighbor events; if
		 * there is a pending work item, return without scheduling a new
		 * one. This logic can be revisited in the future if we need to
		 * support multiple neighbor update events.
		 */
		spin_lock_bh(&bp->neigh_update.lock);
		if (bp->neigh_update.neigh) {
			spin_unlock_bh(&bp->neigh_update.lock);
			break;
		}
		bp->neigh_update.neigh = n;
		spin_unlock_bh(&bp->neigh_update.lock);
		/* Do not schedule the work if FW reset is in progress. */
		if (test_bit(BNXT_STATE_IN_FW_RESET, &bp->state)) {
			netdev_dbg(bp->dev,
				   "FW reset, dropping neigh update event\n");
			bp->neigh_update.neigh = NULL;
			break;
		}
		/* Release neighbor in queue work handler if put work task successfully */
		neigh_hold(n);
		if (schedule_work(&bp->neigh_update.work))
			break;

		neigh_release(n);
		bp->neigh_update.neigh = NULL;
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

int bnxt_init_tc(struct bnxt *bp)
{
	struct bnxt_tc_info *tc_info;
	int rc;

	if (bp->hwrm_spec_code < 0x10800)
		return 0;

	tc_info = kzalloc(sizeof(*tc_info), GFP_KERNEL);
	if (!tc_info)
		return -ENOMEM;
	mutex_init(&tc_info->lock);

	/* Counter widths are programmed by FW */
	tc_info->bytes_mask = mask(36);
	tc_info->packets_mask = mask(28);

	tc_info->flow_ht_params = bnxt_tc_flow_ht_params;
	rc = rhashtable_init(&tc_info->flow_table, &tc_info->flow_ht_params);
	if (rc)
		goto free_tc_info;

	tc_info->tf_flow_ht_params = bnxt_tf_flow_ht_params;
	rc = rhashtable_init(&tc_info->tf_flow_table,
			     &tc_info->tf_flow_ht_params);
	if (rc)
		goto destroy_flow_table;

	tc_info->l2_ht_params = bnxt_tc_l2_ht_params;
	rc = rhashtable_init(&tc_info->l2_table, &tc_info->l2_ht_params);
	if (rc)
		goto destroy_tf_flow_table;

	tc_info->decap_l2_ht_params = bnxt_tc_decap_l2_ht_params;
	rc = rhashtable_init(&tc_info->decap_l2_table,
			     &tc_info->decap_l2_ht_params);
	if (rc)
		goto destroy_l2_table;

	tc_info->decap_ht_params = bnxt_tc_tunnel_ht_params;
	rc = rhashtable_init(&tc_info->decap_table,
			     &tc_info->decap_ht_params);
	if (rc)
		goto destroy_decap_l2_table;

	tc_info->encap_ht_params = bnxt_tc_tunnel_ht_params;
	rc = rhashtable_init(&tc_info->encap_table,
			     &tc_info->encap_ht_params);
	if (rc)
		goto destroy_decap_table;

	tc_info->neigh_ht_params = bnxt_tc_neigh_ht_params;
	rc = rhashtable_init(&tc_info->neigh_table,
			     &tc_info->neigh_ht_params);
	if (rc)
		goto destroy_encap_table;

	tc_info->v6_subnet_ht_params = bnxt_ulp_udcc_v6_subnet_ht_params;
	rc = rhashtable_init(&tc_info->v6_subnet_table,
			     &tc_info->v6_subnet_ht_params);
	if (rc)
		goto destroy_neigh_table;

	rc = bnxt_ba_init(&tc_info->v6_subnet_pool,
			  BNXT_ULP_MAX_V6_SUBNETS,
			  true);
	if (rc)
		goto destroy_v6_subnet_table;

	tc_info->enabled = true;
	bp->dev->hw_features |= NETIF_F_HW_TC;
	bp->dev->features |= NETIF_F_HW_TC;
	bp->tc_info = tc_info;

	bp->neigh_update.neigh = NULL;
	spin_lock_init(&bp->neigh_update.lock);
	INIT_WORK(&bp->neigh_update.work, bnxt_tc_update_neigh_work);
	bp->neigh_update.netevent_nb.notifier_call = bnxt_rep_netevent_cb;
	rc = register_netevent_notifier(&bp->neigh_update.netevent_nb);
	if (rc)
		goto destroy_v6_subnet_table;

#ifndef HAVE_FLOW_INDR_BLOCK_CB
	netdev_dbg(bp->dev, "Not registering indirect block notification\n");
	return 0;
#else
	netdev_dbg(bp->dev, "Registering indirect block notification\n");
	/* init indirect block notifications */
	INIT_LIST_HEAD(&bp->tc_indr_block_list);
	rc = flow_indr_dev_register(bnxt_tc_setup_indr_cb, bp);
	if (!rc)
		return 0;

	unregister_netevent_notifier(&bp->neigh_update.netevent_nb);
#endif

destroy_v6_subnet_table:
	rhashtable_destroy(&tc_info->v6_subnet_table);
destroy_neigh_table:
	rhashtable_destroy(&tc_info->neigh_table);
destroy_encap_table:
	rhashtable_destroy(&tc_info->encap_table);
destroy_decap_table:
	rhashtable_destroy(&tc_info->decap_table);
destroy_decap_l2_table:
	rhashtable_destroy(&tc_info->decap_l2_table);
destroy_l2_table:
	rhashtable_destroy(&tc_info->l2_table);
destroy_tf_flow_table:
	rhashtable_destroy(&tc_info->tf_flow_table);
destroy_flow_table:
	rhashtable_destroy(&tc_info->flow_table);
free_tc_info:
	kfree(tc_info);
	bp->tc_info = NULL;
	return rc;
}

void bnxt_shutdown_tc(struct bnxt *bp)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;

	if (!bnxt_tc_flower_enabled(bp))
		return;

#ifdef HAVE_FLOW_INDR_BLOCK_CB
	flow_indr_dev_unregister(bnxt_tc_setup_indr_cb, bp,
				 bnxt_tc_setup_indr_rel);
#endif
	unregister_netevent_notifier(&bp->neigh_update.netevent_nb);
	cancel_work_sync(&bp->neigh_update.work);
	rhashtable_destroy(&tc_info->flow_table);
	rhashtable_destroy(&tc_info->tf_flow_table);
	rhashtable_destroy(&tc_info->l2_table);
	rhashtable_destroy(&tc_info->decap_l2_table);
	rhashtable_destroy(&tc_info->decap_table);
	rhashtable_destroy(&tc_info->encap_table);
	rhashtable_destroy(&tc_info->neigh_table);
	rhashtable_destroy(&tc_info->v6_subnet_table);
	bnxt_ba_deinit(&tc_info->v6_subnet_pool);

	kfree(tc_info);
	bp->tc_info = NULL;
}

#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
