
// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023-2023 Broadcom
 * All rights reserved.
 */

#ifdef CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tf_common.h"
#include "bnxt_ulp_flow.h"
#include "ulp_tc_parser.h"
#include "ulp_matcher.h"
#include "ulp_flow_db.h"
#include "ulp_mapper.h"
#include "ulp_fc_mgr.h"
#include "ulp_port_db.h"
#include "ulp_template_debug_proto.h"
#include "bnxt_ulp_flow.h"
#include "ulp_tc_custom_offload.h"
#include "ulp_tc_rte_flow.h"
#include "bnxt_ulp_flow.h"

static struct rte_flow_item  eth_item = { RTE_FLOW_ITEM_TYPE_ETH, 0, 0, 0 };
static struct rte_flow_item_eth  eth_spec;
static struct rte_flow_item_eth  eth_mask;
static struct rte_flow_item  end_item = { RTE_FLOW_ITEM_TYPE_END, 0, 0, 0 };

static struct rte_flow_item_ipv4 ipv4_spec;
static struct rte_flow_item_ipv4 ipv4_mask;
static struct rte_flow_item_ipv6 ipv6_spec;
static struct rte_flow_item_ipv6 ipv6_mask;

static struct rte_flow_item ipv4_item;
static struct rte_flow_item ipv6_item;
static struct rte_flow_item udp_item;
static struct rte_flow_item tcp_item;

static struct rte_flow_item_tcp tcp_spec;
static struct rte_flow_item_tcp tcp_mask;
static struct rte_flow_item_udp udp_spec;
static struct rte_flow_item_udp udp_mask;

#if 0
static struct rte_flow_item ipv4_item_outer;
static struct rte_flow_item_ipv4 ipv4_spec_outer;
static struct rte_flow_item_ipv4 ipv4_mask_outer;

static struct rte_flow_item_vxlan vxlan_spec;
static struct rte_flow_item_vxlan vxlan_mask;
static struct rte_flow_item vxlan_item;

static struct rte_flow_item_gre gre_spec;
static struct rte_flow_item_gre gre_mask;
static struct rte_flow_item gre_item;
/*
 * struct rte_flow_item_udp udp_spec_outer = { 0 };
 * struct rte_flow_item_udp udp_mask_outer = { 0 };
 * struct rte_flow_item udp_item_outer = { 0 };
 */
#endif

struct rte_flow_action_queue queue_action;
struct rte_flow_action actions[2];

uint8_t ipv6_src_addr[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 9, 9, 1};
uint8_t ipv6_dst_addr[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 9, 9, 2};

static int
ulp_add_eth_dmac_rule(struct bnxt *bp, u8 *dst_addr, u16 q_index)
{
	u8 mask[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[2] = { 0 };

	memset(&eth_spec, 0, sizeof(eth_spec));
	memset(&eth_mask, 0, sizeof(eth_mask));
	memcpy(eth_spec.hdr.dst_addr.addr_bytes, dst_addr, sizeof(eth_spec.hdr.dst_addr));
	memcpy(eth_mask.hdr.dst_addr.addr_bytes, mask, sizeof(eth_mask.hdr.dst_addr));

	eth_item.type = RTE_FLOW_ITEM_TYPE_ETH;
	eth_item.spec = &eth_spec;
	eth_item.mask = &eth_mask;

	pattern[0] = eth_item;
	pattern[1] = end_item;

	queue_action.index = q_index;
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = &queue_action;

	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	actions[1].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions,
					   &flow_info);
}

static int
ulp_add_eth_type_rule(struct bnxt *bp, uint16_t eth_type, u16 q_index)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[2] = { 0 };

	memset(&eth_spec, 0, sizeof(eth_spec));
	memset(&eth_mask, 0, sizeof(eth_mask));

	eth_spec.hdr.ether_type = cpu_to_be16(eth_type);
	eth_mask.hdr.ether_type = cpu_to_be16(0xffff);

	eth_item.type = RTE_FLOW_ITEM_TYPE_ETH;
	eth_item.spec = &eth_spec;
	eth_item.mask = &eth_mask;

	pattern[0] = eth_item;
	pattern[1] = end_item;

	queue_action.index = q_index;
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = &queue_action;

	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	actions[1].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions,
					   &flow_info);
}

static int
ulp_add_non_tunnel_tcp_5tuple(struct bnxt *bp, int i, u16 q_index)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[4];

	/* Outer IPv4 Item */
	ipv4_item.type = RTE_FLOW_ITEM_TYPE_IPV4;
	ipv4_item.spec = &ipv4_spec;
	ipv4_item.mask = &ipv4_mask;
	ipv4_item.last = NULL;
	memset(&ipv4_spec, 0, sizeof(ipv4_spec));
	ipv4_spec.hdr.next_proto_id = 6;
	ipv4_spec.hdr.src_addr = cpu_to_be32(RTE_IPV4(9, 9, 9, 1));
	ipv4_spec.hdr.dst_addr = cpu_to_be32(RTE_IPV4(9, 9, 9, 2));
	memset(&ipv4_mask, 0, sizeof(ipv4_mask));
	ipv4_mask.hdr.next_proto_id = 0xff;
	ipv4_mask.hdr.src_addr = cpu_to_be32(0xffffffff);
	ipv4_mask.hdr.dst_addr = cpu_to_be32(0xffffffff);

	/* Outer TCP Item */
	tcp_item.type = RTE_FLOW_ITEM_TYPE_TCP;
	tcp_item.spec = &tcp_spec;
	tcp_item.mask = &tcp_mask;
	tcp_item.last = NULL;
	memset(&tcp_spec, 0, sizeof(tcp_spec));
	tcp_spec.hdr.src_port = cpu_to_be16(0xBBAA + i);
	tcp_spec.hdr.dst_port = cpu_to_be16(0xDDCC + i);
	memset(&tcp_mask, 0, sizeof(tcp_mask));
	tcp_mask.hdr.src_port = cpu_to_be16(0xffff);
	tcp_mask.hdr.dst_port = cpu_to_be16(0xffff);

	pattern[0] = eth_item;
	pattern[1] = ipv4_item;
	pattern[2] = tcp_item;
	pattern[3] = end_item;

	queue_action.index = q_index;
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = &queue_action;

	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	actions[1].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions,
					   &flow_info);
}

static int
ulp_add_non_tunnel_udp_5tuple(struct bnxt *bp, int i, u16 q_index)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[4];

	/* Outer IPv4 Item */
	ipv4_item.type = RTE_FLOW_ITEM_TYPE_IPV4;
	ipv4_item.spec = &ipv4_spec;
	ipv4_item.mask = &ipv4_mask;
	ipv4_item.last = NULL;
	memset(&ipv4_spec, 0, sizeof(ipv4_spec));
	ipv4_spec.hdr.next_proto_id = 17;
	ipv4_spec.hdr.src_addr = cpu_to_be32(RTE_IPV4(9, 9, 9, 1));
	ipv4_spec.hdr.dst_addr = cpu_to_be32(RTE_IPV4(9, 9, 9, 2));
	memset(&ipv4_mask, 0, sizeof(ipv4_mask));
	ipv4_mask.hdr.next_proto_id = 0xff;
	ipv4_mask.hdr.src_addr = cpu_to_be32(0xffffffff);
	ipv4_mask.hdr.dst_addr = cpu_to_be32(0xffffffff);

	/* Outer UDP Item */
	udp_item.type = RTE_FLOW_ITEM_TYPE_UDP;
	udp_item.spec = &udp_spec;
	udp_item.mask = &udp_mask;
	udp_item.last = NULL;
	memset(&udp_spec, 0, sizeof(udp_spec));
	udp_spec.hdr.src_port = cpu_to_be16(0xBBAA + i);
	udp_spec.hdr.dst_port = cpu_to_be16(0xDDCC + i);
	memset(&udp_mask, 0, sizeof(udp_mask));
	udp_mask.hdr.src_port = cpu_to_be16(0xffff);
	udp_mask.hdr.dst_port = cpu_to_be16(0xffff);

	pattern[0] = eth_item;
	pattern[1] = ipv4_item;
	pattern[2] = udp_item;
	pattern[3] = end_item;

	queue_action.index = q_index;
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = &queue_action;

	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	actions[1].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions,
					   &flow_info);
}

static int
ulp_add_non_tunnel_tcp_5tuple_ipv6(struct bnxt *bp, int i, u16 q_index)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[4];

	/* Outer IPv4 Item */
	ipv6_item.type = RTE_FLOW_ITEM_TYPE_IPV6;
	ipv6_item.spec = &ipv6_spec;
	ipv6_item.mask = &ipv6_mask;
	ipv6_item.last = NULL;
	memset(&ipv6_spec, 0, sizeof(ipv6_spec));
	ipv6_spec.hdr.proto = 6;
	memcpy(ipv6_spec.hdr.src_addr, (uint8_t *)ipv6_src_addr, 16);
	memcpy(ipv6_spec.hdr.dst_addr, (uint8_t *)ipv6_dst_addr, 16);
	memset(&ipv6_mask, 0, sizeof(ipv6_mask));
	ipv6_mask.hdr.proto = 0xff;
	memset(ipv6_mask.hdr.src_addr, 0xff, 16);
	memset(ipv6_mask.hdr.dst_addr, 0xff, 16);

	/* Outer TCP Item */
	tcp_item.type = RTE_FLOW_ITEM_TYPE_TCP;
	tcp_item.spec = &tcp_spec;
	tcp_item.mask = &tcp_mask;
	tcp_item.last = NULL;
	memset(&tcp_spec, 0, sizeof(tcp_spec));
	tcp_spec.hdr.src_port = cpu_to_be16(0xBBAA + i);
	tcp_spec.hdr.dst_port = cpu_to_be16(0xDDCC + i);
	memset(&tcp_mask, 0, sizeof(tcp_mask));
	tcp_mask.hdr.src_port = cpu_to_be16(0xffff);
	tcp_mask.hdr.dst_port = cpu_to_be16(0xffff);

	pattern[0] = eth_item;
	pattern[1] = ipv6_item;
	pattern[2] = tcp_item;
	pattern[3] = end_item;

	queue_action.index = q_index;
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = &queue_action;

	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	actions[1].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions,
					   &flow_info);
}

static int
ulp_add_non_tunnel_udp_5tuple_ipv6(struct bnxt *bp, int i, u16 q_index)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[4];

	/* Outer IPv4 Item */
	ipv6_item.type = RTE_FLOW_ITEM_TYPE_IPV6;
	ipv6_item.spec = &ipv6_spec;
	ipv6_item.mask = &ipv6_mask;
	ipv6_item.last = NULL;
	memset(&ipv6_spec, 0, sizeof(ipv6_spec));
	ipv6_spec.hdr.proto = 17;
	memcpy(ipv6_spec.hdr.src_addr, (uint8_t *)ipv6_src_addr, 16);
	memcpy(ipv6_spec.hdr.dst_addr, (uint8_t *)ipv6_dst_addr, 16);
	memset(&ipv6_mask, 0, sizeof(ipv6_mask));
	ipv6_mask.hdr.proto = 0xff;
	memset(ipv6_mask.hdr.src_addr, 0xff, 16);
	memset(ipv6_mask.hdr.dst_addr, 0xff, 16);

	/* Outer UDP Item */
	udp_item.type = RTE_FLOW_ITEM_TYPE_UDP;
	udp_item.spec = &udp_spec;
	udp_item.mask = &udp_mask;
	udp_item.last = NULL;
	memset(&udp_spec, 0, sizeof(udp_spec));
	udp_spec.hdr.src_port = cpu_to_be16(0xBBAA + i);
	udp_spec.hdr.dst_port = cpu_to_be16(0xDDCC + i);
	memset(&udp_mask, 0, sizeof(udp_mask));
	udp_mask.hdr.src_port = cpu_to_be16(0xffff);
	udp_mask.hdr.dst_port = cpu_to_be16(0xffff);

	pattern[0] = eth_item;
	pattern[1] = ipv6_item;
	pattern[2] = udp_item;
	pattern[3] = end_item;

	queue_action.index = q_index;
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = &queue_action;

	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	actions[1].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions,
					   &flow_info);
}

static int
ulp_add_non_tunnel_ip4_proto(struct bnxt *bp, u16 q_index)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[3];

	ipv4_item.type = RTE_FLOW_ITEM_TYPE_IPV4;
	ipv4_item.spec = &ipv4_spec;
	ipv4_item.mask = &ipv4_mask;
	ipv4_item.last = NULL;
	memset(&ipv4_spec, 0, sizeof(ipv4_spec));
	ipv4_spec.hdr.next_proto_id = 0x59;
	memset(&ipv4_mask, 0, sizeof(ipv4_mask));
	ipv4_mask.hdr.next_proto_id = 0xff;

	pattern[0] = eth_item;
	pattern[1] = ipv4_item;
	pattern[2] = end_item;

	queue_action.index = q_index;
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = &queue_action;

	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	actions[1].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions, &flow_info);
}

static int
ulp_add_non_tunnel_ip6_proto(struct bnxt *bp, u16 q_index)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[3];

	ipv6_item.type = RTE_FLOW_ITEM_TYPE_IPV6;
	ipv6_item.spec = &ipv6_spec;
	ipv6_item.mask = &ipv6_mask;
	ipv6_item.last = NULL;
	memset(&ipv6_spec, 0, sizeof(ipv6_spec));
	ipv6_spec.hdr.proto = 0x59;
	memset(&ipv6_mask, 0, sizeof(ipv6_mask));
	ipv6_mask.hdr.proto = 0xff;

	pattern[0] = eth_item;
	pattern[1] = ipv6_item;
	pattern[2] = end_item;

	queue_action.index = q_index;
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = &queue_action;

	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	actions[1].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions, &flow_info);
}

static int
ulp_add_non_tunnel_tcp_dport_v4(struct bnxt *bp, int dport, u16 q_index)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[4];

	ipv4_item.type = RTE_FLOW_ITEM_TYPE_IPV4;
	ipv4_item.spec = &ipv4_spec;
	ipv4_item.mask = &ipv4_mask;
	ipv4_item.last = NULL;
	memset(&ipv4_spec, 0, sizeof(ipv4_spec));
	memset(&ipv4_mask, 0, sizeof(ipv4_mask));

	/* Outer TCP Item */
	tcp_item.type = RTE_FLOW_ITEM_TYPE_TCP;
	tcp_item.spec = &tcp_spec;
	tcp_item.mask = &tcp_mask;
	tcp_item.last = NULL;
	memset(&tcp_spec, 0, sizeof(tcp_spec));
	tcp_spec.hdr.dst_port = cpu_to_be16(dport);
	memset(&tcp_mask, 0, sizeof(tcp_mask));
	tcp_mask.hdr.dst_port = cpu_to_be16(0xffff);

	pattern[0] = eth_item;
	pattern[1] = ipv4_item;
	pattern[2] = tcp_item;
	pattern[3] = end_item;

	queue_action.index = q_index;
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = &queue_action;

	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	actions[1].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions,
					   &flow_info);
}

static int
ulp_add_non_tunnel_tcp_dport_v6(struct bnxt *bp, int dport, u16 q_index)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[4];

	ipv6_item.type = RTE_FLOW_ITEM_TYPE_IPV6;
	ipv6_item.spec = &ipv6_spec;
	ipv6_item.mask = &ipv6_mask;
	ipv6_item.last = NULL;
	memset(&ipv6_spec, 0, sizeof(ipv6_spec));
	memset(&ipv6_mask, 0, sizeof(ipv6_mask));

	/* Outer TCP Item */
	tcp_item.type = RTE_FLOW_ITEM_TYPE_TCP;
	tcp_item.spec = &tcp_spec;
	tcp_item.mask = &tcp_mask;
	tcp_item.last = NULL;
	memset(&tcp_spec, 0, sizeof(tcp_spec));
	tcp_spec.hdr.dst_port = cpu_to_be16(dport);
	memset(&tcp_mask, 0, sizeof(tcp_mask));
	tcp_mask.hdr.dst_port = cpu_to_be16(0xffff);

	pattern[0] = eth_item;
	pattern[1] = ipv6_item;
	pattern[2] = tcp_item;
	pattern[3] = end_item;

	queue_action.index = q_index;
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = &queue_action;

	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	actions[1].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions,
					   &flow_info);
}

static int
ulp_add_non_tunnel_udp_dport_v4(struct bnxt *bp, int dport, u16 q_index)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[4];

	ipv4_item.type = RTE_FLOW_ITEM_TYPE_IPV4;
	ipv4_item.spec = &ipv4_spec;
	ipv4_item.mask = &ipv4_mask;
	ipv4_item.last = NULL;
	memset(&ipv4_spec, 0, sizeof(ipv4_spec));
	memset(&ipv4_mask, 0, sizeof(ipv4_mask));

	/* Outer TCP Item */
	udp_item.type = RTE_FLOW_ITEM_TYPE_UDP;
	udp_item.spec = &udp_spec;
	udp_item.mask = &udp_mask;
	udp_item.last = NULL;
	memset(&udp_spec, 0, sizeof(udp_spec));
	udp_spec.hdr.dst_port = cpu_to_be16(dport);
	memset(&udp_mask, 0, sizeof(udp_mask));
	udp_mask.hdr.dst_port = cpu_to_be16(0xffff);

	pattern[0] = eth_item;
	pattern[1] = ipv4_item;
	pattern[2] = udp_item;
	pattern[3] = end_item;

	queue_action.index = q_index;
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = &queue_action;

	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	actions[1].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions, &flow_info);
}

static int
ulp_add_non_tunnel_udp_dport_v6(struct bnxt *bp, int dport, u16 q_index)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[4];

	ipv6_item.type = RTE_FLOW_ITEM_TYPE_IPV6;
	ipv6_item.spec = &ipv6_spec;
	ipv6_item.mask = &ipv6_mask;
	ipv6_item.last = NULL;
	memset(&ipv6_spec, 0, sizeof(ipv6_spec));
	memset(&ipv6_mask, 0, sizeof(ipv6_mask));

	/* Outer TCP Item */
	udp_item.type = RTE_FLOW_ITEM_TYPE_UDP;
	udp_item.spec = &udp_spec;
	udp_item.mask = &udp_mask;
	udp_item.last = NULL;
	memset(&udp_spec, 0, sizeof(udp_spec));
	udp_spec.hdr.dst_port = cpu_to_be16(dport);
	memset(&udp_mask, 0, sizeof(udp_mask));
	udp_mask.hdr.dst_port = cpu_to_be16(0xffff);

	pattern[0] = eth_item;
	pattern[1] = ipv6_item;
	pattern[2] = udp_item;
	pattern[3] = end_item;

	queue_action.index = q_index;
	actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	actions[0].conf = &queue_action;

	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	actions[1].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions, &flow_info);
}

static int
ulp_add_all_ipv4_rules(struct bnxt *bp, int count)
{
	int i, rc;

	for (i = 0; i < count; i++) {
		rc = ulp_add_non_tunnel_tcp_5tuple(bp, i, 1);
		if (rc) {
			netdev_err(bp->dev, "Failed to add IPv4 TCP 5 tuple rule\n");
			return rc;
		}
	}

	for (i = 0; i < count; i++) {
		rc = ulp_add_non_tunnel_udp_5tuple(bp, i, 1);
		if (rc) {
			netdev_err(bp->dev, "Failed to add IPv4 UDP 5 tuple rule\n");
			return rc;
		}
	}

	rc = ulp_add_non_tunnel_ip4_proto(bp, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add OSPF IPv4 flow, Proto = 0x59\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_tcp_dport_v4(bp, 0xB3, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add BGP flow, TCP dport = 0xB3\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_udp_dport_v4(bp, 0x0EC8, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add BFD flow, UDP dport = 0x0EC8\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_udp_dport_v4(bp, 0x0EC9, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add BFD flow, UDP dport = 0x0EC9\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_udp_dport_v4(bp, 0x12B0, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add BFD flow, UDP dport = 0x12B0\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_udp_dport_v4(bp, 0x1A80, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add BFD flow, UDP dport = 0x1A80\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_udp_dport_v4(bp, 0x286, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add LDP flow, UDP dport = 0x286\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_udp_dport_v4(bp, 0x7B, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add NTP flow, UDP dport = 0x7B\n");
		return rc;
	}

	return 0;
}

static int
ulp_add_all_ipv6_rules(struct bnxt *bp, int count)
{
	int i, rc;

	for (i = 0; i < count; i++) {
		rc = ulp_add_non_tunnel_tcp_5tuple_ipv6(bp, i, 1);
		if (rc) {
			netdev_err(bp->dev, "Failed to add IPv6 TCP 5 tuple rule\n");
			return rc;
		}
	}

	for (i = 0; i < count; i++) {
		rc = ulp_add_non_tunnel_udp_5tuple_ipv6(bp, i, 1);
		if (rc) {
			netdev_err(bp->dev, "Failed to add IPv6 UDP 5 tuple rule\n");
			return rc;
		}
	}

	rc = ulp_add_non_tunnel_ip6_proto(bp, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add OSPF IPv6 flow, Proto = 0x59\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_tcp_dport_v6(bp, 0xB3, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add BGP v6 flow, TCP dport = 0xB3\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_udp_dport_v6(bp, 0x0EC8, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add BFD v6 flow, UDP dport = 0x0EC8\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_udp_dport_v6(bp, 0x0EC9, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add BFD v6 flow, UDP dport = 0x0EC9\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_udp_dport_v6(bp, 0x12B0, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add BFD v6 flow, UDP dport = 0x12B0\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_udp_dport_v6(bp, 0x1A80, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add BFD v6 flow, UDP dport = 0x1A80\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_udp_dport_v6(bp, 0x286, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add LDP v6 flow, UDP dport = 0x286\n");
		return rc;
	}

	rc = ulp_add_non_tunnel_udp_dport_v6(bp, 0x7B, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add NTP v6 flow, UDP dport = 0x7B\n");
		return rc;
	}

	return 0;
}

#if 0
static int
ulp_add_vxlan_inner_5tuple(struct bnxt *bp, int i)
{
	struct rte_flow_item pattern[7];
	struct bnxt_ulp_flow_info flow_info = { 0 };

	/* Outer IPv4 Item */
	ipv4_item_outer.type = RTE_FLOW_ITEM_TYPE_IPV4;
	ipv4_item_outer.spec = &ipv4_spec_outer;
	ipv4_item_outer.mask = &ipv4_mask_outer;
	ipv4_item_outer.last = NULL;

	/* Vxlan Item */
	vxlan_item.type = RTE_FLOW_ITEM_TYPE_VXLAN;
	vxlan_item.spec = &vxlan_spec;
	vxlan_item.mask = &vxlan_mask;
	vxlan_item.last = NULL;
	/*
	 * TBD: Outer UDP Item
	 * udp_item_outer.type = RTE_FLOW_ITEM_TYPE_UDP;
	 * udp_item_outer.spec = &udp_spec_outer;
	 * udp_item_outer.mask = &udp_mask_outer;
	 * udp_item_outer.last = NULL;
	 * memset(&udp_spec_outer, 0, sizeof(udp_spec_outer));
	 * udp_spec_outer.hdr.dst_port = 4789;
	 * memset(&udp_mask_outer, 0, sizeof(udp_mask_outer));
	 * udp_mask_outer.hdr.dst_port = 0xffff;
	 */

	/* Inner IPv4 Item */
	ipv4_item.type = RTE_FLOW_ITEM_TYPE_IPV4;
	ipv4_item.spec = &ipv4_spec;
	ipv4_item.mask = &ipv4_mask;
	ipv4_item.last = NULL;
	memset(&ipv4_spec, 0, sizeof(ipv4_spec));
	ipv4_spec.hdr.next_proto_id = 17;
	ipv4_spec.hdr.src_addr = cpu_to_be32(RTE_IPV4(9, 9, 9, 1));
	ipv4_spec.hdr.dst_addr = cpu_to_be32(RTE_IPV4(9, 9, 9, 2));
	memset(&ipv4_mask, 0, sizeof(ipv4_mask));
	ipv4_mask.hdr.next_proto_id = 0xff;
	ipv4_mask.hdr.src_addr = cpu_to_be32(0xffffffff);
	ipv4_mask.hdr.dst_addr = cpu_to_be32(0xffffffff);

	/* Inner UDP Item */
	udp_item.type = RTE_FLOW_ITEM_TYPE_UDP;
	udp_item.spec = &udp_spec;
	udp_item.mask = &udp_mask;
	udp_item.last = NULL;
	memset(&udp_spec, 0, sizeof(udp_spec));
	udp_spec.hdr.src_port = cpu_to_be16(0xBBAA + i);
	udp_spec.hdr.dst_port = cpu_to_be16(0xDDCC + i);
	memset(&udp_mask, 0, sizeof(udp_mask));
	udp_mask.hdr.src_port = 0xffff;
	udp_mask.hdr.dst_port = 0xffff;

	pattern[0] = eth_item;
	pattern[1] = ipv4_item_outer;
	pattern[2] = vxlan_item;
	/* TBD: pattern[3] = eth_item; */
	pattern[3] = ipv4_item;
	pattern[4] = udp_item;
	pattern[5] = end_item;

	actions[0].type = RTE_FLOW_ACTION_TYPE_END;
	actions[0].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions,
					   &flow_info);
}

static int
ulp_add_gre_inner_5tuple(struct bnxt *bp, int i)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[6];

	/* Outer IPv4 Item */
	ipv4_item_outer.type = RTE_FLOW_ITEM_TYPE_IPV4;
	ipv4_item_outer.spec = &ipv4_spec_outer;
	ipv4_item_outer.mask = &ipv4_mask_outer;
	ipv4_item_outer.last = NULL;

	/*
	 * ipv4_item_outer.type = RTE_FLOW_ITEM_TYPE_IPV4;
	 * ipv4_item_outer.spec = &ipv4_spec_outer;
	 * ipv4_item_outer.mask = &ipv4_mask_outer;
	 * ipv4_item_outer.last = NULL;
	 * memset(&ipv4_spec_outer, 0, sizeof(ipv4_spec_outer));
	 * ipv4_spec_outer.hdr.next_proto_id = 47;
	 * memset(&ipv4_mask_outer, 0, sizeof(ipv4_mask_outer));
	 * ipv4_mask_outer.hdr.next_proto_id = 0xff;
	 */

	/* GRE Item */
	gre_item.type = RTE_FLOW_ITEM_TYPE_GRE;
	gre_item.spec = &gre_spec;
	gre_item.mask = &gre_mask;
	gre_item.last = NULL;

	/* Inner IPv4 Item */
	ipv4_item.type = RTE_FLOW_ITEM_TYPE_IPV4;
	ipv4_item.spec = &ipv4_spec;
	ipv4_item.mask = &ipv4_mask;
	ipv4_item.last = NULL;
	memset(&ipv4_spec, 0, sizeof(ipv4_spec));
	ipv4_spec.hdr.next_proto_id = 17;
	ipv4_spec.hdr.src_addr = cpu_to_be32(RTE_IPV4(9, 9, 9, 1));
	ipv4_spec.hdr.dst_addr = cpu_to_be32(RTE_IPV4(9, 9, 9, 2));
	memset(&ipv4_mask, 0, sizeof(ipv4_mask));
	ipv4_mask.hdr.next_proto_id = 0xff;
	ipv4_mask.hdr.src_addr = cpu_to_be32(0xffffffff);
	ipv4_mask.hdr.dst_addr = cpu_to_be32(0xffffffff);

	/* Inner UDP Item */
	udp_item.type = RTE_FLOW_ITEM_TYPE_UDP;
	udp_item.spec = &udp_spec;
	udp_item.mask = &udp_mask;
	udp_item.last = NULL;
	memset(&udp_spec, 0, sizeof(udp_spec));
	udp_spec.hdr.src_port = cpu_to_be16(0xFFEE + i);
	udp_spec.hdr.dst_port = cpu_to_be16(0xEEDD + i);
	memset(&udp_mask, 0, sizeof(udp_mask));
	udp_mask.hdr.src_port = 0xffff;
	udp_mask.hdr.dst_port = 0xffff;

	pattern[0] = eth_item;
	pattern[1] = ipv4_item_outer;
	pattern[2] = gre_item;
	pattern[3] = ipv4_item;
	pattern[4] = udp_item;
	pattern[5] = end_item;

	actions[0].type = RTE_FLOW_ACTION_TYPE_END;
	actions[0].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions,
					   &flow_info);
}

static int
ulp_add_vxlan_inner_5tuple_ipv6(struct bnxt *bp, int i)
{
	struct rte_flow_item pattern[7];
	struct bnxt_ulp_flow_info flow_info = { 0 };

	/* Outer IPv4 Item */
	ipv4_item_outer.type = RTE_FLOW_ITEM_TYPE_IPV4;
	ipv4_item_outer.spec = &ipv4_spec_outer;
	ipv4_item_outer.mask = &ipv4_mask_outer;
	ipv4_item_outer.last = NULL;

	/* Vxlan Item */
	vxlan_item.type = RTE_FLOW_ITEM_TYPE_VXLAN;
	vxlan_item.spec = &vxlan_spec;
	vxlan_item.mask = &vxlan_mask;
	vxlan_item.last = NULL;
	/*
	 * Outer UDP Item
	 * udp_item_outer.type = RTE_FLOW_ITEM_TYPE_UDP;
	 * udp_item_outer.spec = &udp_spec_outer;
	 * udp_item_outer.mask = &udp_mask_outer;
	 * udp_item_outer.last = NULL;
	 * memset(&udp_spec_outer, 0, sizeof(udp_spec_outer));
	 * udp_spec_outer.hdr.dst_port = 4789;
	 * memset(&udp_mask_outer, 0, sizeof(udp_mask_outer));
	 * udp_mask_outer.hdr.dst_port = 0xffff;
	 */

	/* Inner IPv6 Item */
	ipv6_item.type = RTE_FLOW_ITEM_TYPE_IPV6;
	ipv6_item.spec = &ipv6_spec;
	ipv6_item.mask = &ipv6_mask;
	ipv6_item.last = NULL;
	memset(&ipv6_spec, 0, sizeof(ipv6_spec));
	ipv6_spec.hdr.proto = 17;
	memcpy(ipv6_spec.hdr.src_addr, (uint8_t *)ipv6_src_addr, 16);
	memcpy(ipv6_spec.hdr.dst_addr, (uint8_t *)ipv6_dst_addr, 16);
	memset(&ipv6_mask, 0, sizeof(ipv6_mask));
	ipv6_mask.hdr.proto = 0xff;
	memset(ipv6_mask.hdr.src_addr, 0xff, 16);
	memset(ipv6_mask.hdr.dst_addr, 0xff, 16);

	/* Inner UDP Item */
	udp_item.type = RTE_FLOW_ITEM_TYPE_UDP;
	udp_item.spec = &udp_spec;
	udp_item.mask = &udp_mask;
	udp_item.last = NULL;
	memset(&udp_spec, 0, sizeof(udp_spec));
	udp_spec.hdr.src_port = cpu_to_be16(0xBBAA + i);
	udp_spec.hdr.dst_port = cpu_to_be16(0xDDCC + i);
	memset(&udp_mask, 0, sizeof(udp_mask));
	udp_mask.hdr.src_port = 0xffff;
	udp_mask.hdr.dst_port = 0xffff;

	pattern[0] = eth_item;
	pattern[1] = ipv4_item_outer;
	pattern[2] = vxlan_item;
	pattern[3] = ipv6_item;
	pattern[4] = udp_item;
	pattern[5] = end_item;

	actions[0].type = RTE_FLOW_ACTION_TYPE_END;
	actions[0].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions,
					   &flow_info);
}

static int
ulp_add_gre_inner_5tuple_ipv6(struct bnxt *bp, int i)
{
	struct bnxt_ulp_flow_info flow_info = { 0 };
	struct rte_flow_item pattern[6];

	/* Outer IPv4 Item */
	ipv4_item_outer.type = RTE_FLOW_ITEM_TYPE_IPV4;
	ipv4_item_outer.spec = &ipv4_spec_outer;
	ipv4_item_outer.mask = &ipv4_mask_outer;
	ipv4_item_outer.last = NULL;

	/*
	 * memset(&ipv4_spec_outer, 0, sizeof(ipv4_spec_outer));
	 * ipv4_spec_outer.hdr.next_proto_id = 47;
	 * memset(&ipv4_mask_outer, 0, sizeof(ipv4_mask_outer));
	 * ipv4_mask_outer.hdr.next_proto_id = 0xff;
	 */

	/* GRE Item */
	gre_item.type = RTE_FLOW_ITEM_TYPE_GRE;
	gre_item.spec = &gre_spec;
	gre_item.mask = &gre_mask;
	gre_item.last = NULL;

	/* Inner IPv6 Item */
	ipv6_item.type = RTE_FLOW_ITEM_TYPE_IPV6;
	ipv6_item.spec = &ipv6_spec;
	ipv6_item.mask = &ipv6_mask;
	ipv6_item.last = NULL;
	memset(&ipv6_spec, 0, sizeof(ipv6_spec));
	ipv6_spec.hdr.proto = 17;
	memcpy(ipv6_spec.hdr.src_addr, (uint8_t *)ipv6_src_addr, 16);
	memcpy(ipv6_spec.hdr.dst_addr, (uint8_t *)ipv6_dst_addr, 16);
	memset(&ipv6_mask, 0, sizeof(ipv6_mask));
	ipv6_mask.hdr.proto = 0xff;
	memset(ipv6_mask.hdr.src_addr, 0xff, 16);
	memset(ipv6_mask.hdr.dst_addr, 0xff, 16);

	/* Inner UDP Item */
	udp_item.type = RTE_FLOW_ITEM_TYPE_UDP;
	udp_item.spec = &udp_spec;
	udp_item.mask = &udp_mask;
	udp_item.last = NULL;
	memset(&udp_spec, 0, sizeof(udp_spec));
	udp_spec.hdr.src_port = cpu_to_be16(0xFFEE + i);
	udp_spec.hdr.dst_port = cpu_to_be16(0xEEDD + i);
	memset(&udp_mask, 0, sizeof(udp_mask));
	udp_mask.hdr.src_port = 0xffff;
	udp_mask.hdr.dst_port = 0xffff;

	pattern[0] = eth_item;
	pattern[1] = ipv4_item_outer;
	pattern[2] = gre_item;
	pattern[3] = ipv6_item;
	pattern[4] = udp_item;
	pattern[5] = end_item;

	actions[0].type = RTE_FLOW_ACTION_TYPE_END;
	actions[0].conf = NULL;

	return bnxt_custom_ulp_flow_create(bp, bp->pf.fw_fid, pattern, actions,
					   &flow_info);
}

/* New requirement is to not offload the below flows. Instead, just do the
 * default behavior. Retain the code so that it can be reused if decided
 * to offload in the future.
 */
static int
ulp_add_all_vxlan_rules(struct bnxt *bp, int count)
{
	int i, rc;

	for (i = 0; i < count; i++) {
		rc = ulp_add_vxlan_inner_5tuple(bp, i);
		if (rc) {
			netdev_err(bp->dev, "Failed to add VxLAN Inner IPv4 rule\n");
			return rc;
		}
	}

	for (i = 0; i < count; i++) {
		rc = ulp_add_vxlan_inner_5tuple_ipv6(bp, i);
		if (rc) {
			netdev_err(bp->dev, "Failed to add VxLAN Inner IPv6 rule\n");
			return rc;
		}
	}

	return 0;
}

static int
ulp_add_all_gre_rules(struct bnxt *bp, int count)
{
	int i, rc;

	for (i = 0; i < count; i++) {
		rc = ulp_add_gre_inner_5tuple(bp, i);
		if (rc) {
			netdev_err(bp->dev, "Failed to add GRE Inner IPv4 rule\n");
			return rc;
		}
	}

	for (i = 0; i < count; i++) {
		rc = ulp_add_gre_inner_5tuple_ipv6(bp, i);
		if (rc) {
			netdev_err(bp->dev, "Failed to add GRE Inner IPv6 rule\n");
			return rc;
		}
	}

	return 0;
}
#endif

static int
ulp_add_all_custom_ethtype_rules(struct bnxt *bp)
{
	int rc, i;

	/* Custom Ethtypes: START */
	for (i = 0xfffb; i <= 0xffff; i++) {
		rc = ulp_add_eth_type_rule(bp, i, 1);
		if (rc) {
			netdev_err(bp->dev, "Failed to add CUSTOM ETH rule: EthType = %d\n", i);
			return rc;
		}
	}

	rc = ulp_add_eth_type_rule(bp, 0x8042, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add CUSTOM ETH rule: EthType = 0x8042\n");
		return rc;
	}

	rc = ulp_add_eth_type_rule(bp, 0xF0F1, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add CUSTOM ETH rule: EthType = 0xF0F1\n");
		return rc;
	}

	for (i = 0; i <= 14; i++) {
		rc = ulp_add_eth_type_rule(bp, 0xAAEF + i, i + 2);
		if (rc) {
			netdev_err(bp->dev,
				   "Failed to add CUSTOM ETH rule: EthType = %d\n", 0xAAEF + i);
			return rc;
		}

		rc = ulp_add_eth_type_rule(bp, 0xBAEF + i, i + 2);
		if (rc) {
			netdev_err(bp->dev,
				   "Failed to add CUSTOM ETH rule: EthType = %d\n", 0xAAEF + i);
			return rc;
		}
	}

	/* Custom Ethtypes: END */

	/* Standard Ethtypes: START */
	/* LACP flow */
	rc = ulp_add_eth_type_rule(bp, 0x8809, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add CUSTOM ETH rule: EthType = 0x8042\n");
		return rc;
	}

	/* LLDP flow */
	rc = ulp_add_eth_type_rule(bp, 0x88CC, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add CUSTOM ETH rule: EthType = 0x8042\n");
		return rc;
	}
	/* Standard Ethtypes: END */

	return 0;
}

static int
ulp_add_all_eth_dmac_rules(struct bnxt *bp)
{
	u8 dst_addr1[8] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x14};
	u8 dst_addr2[8] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x15};
	u8 dst_addr3[8] = {0x99, 0x00, 0x2B, 0x00, 0x00, 0x05};
	int rc;

	rc = ulp_add_eth_dmac_rule(bp, dst_addr1, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add IS-IS flow, ETH DMAC = 0x0180C2000014 rule\n");
		return rc;
	}

	rc = ulp_add_eth_dmac_rule(bp, dst_addr2, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add IS-IS flow, ETH DMAC = 0x0180C2000015 rule\n");
		return rc;
	}

	rc = ulp_add_eth_dmac_rule(bp, dst_addr3, 1);
	if (rc) {
		netdev_err(bp->dev, "Failed to add IS-IS flow, ETH DMAC = 0x99002B000005 rule\n");
		return rc;
	}

	return 0;
}

int
ulp_tc_rte_create_all_flows(struct bnxt *bp, int count)
{
	int rc;

	rc = ulp_add_all_eth_dmac_rules(bp);
	if (rc)
		return rc;

	rc = ulp_add_all_custom_ethtype_rules(bp);
	if (rc)
		return rc;

	rc = ulp_add_all_ipv4_rules(bp, count);
	if (rc)
		return rc;

	rc = ulp_add_all_ipv6_rules(bp, count);
	if (rc)
		return rc;

#if 0
	/* New requirement is to not offload the below flows. Instead, just do the
	 * default behavior. Retain the code so that it can be reused if decided
	 * to offload in the future.
	 */
	rc = ulp_add_all_vxlan_rules(bp, count);
	if (rc)
		return rc;

	rc = ulp_add_all_gre_rules(bp, count);
	if (rc)
		return rc;
#endif

	return 0;
}
#endif
