/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_TC_RTE_FLOW_GEN_H_
#define	_ULP_TC_RTE_FLOW_GEN_H_

#ifdef CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD
/*
 * The following types should be used when handling values according to a
 * specific byte ordering, which may differ from that of the host CPU.
 *
 * Libraries, public APIs and applications are encouraged to use them for
 * documentation purposes.
 */
typedef uint16_t rte_be16_t; /**< 16-bit big-endian value. */
typedef uint32_t rte_be32_t; /**< 32-bit big-endian value. */
typedef uint64_t rte_be64_t; /**< 64-bit big-endian value. */
typedef uint16_t rte_le16_t; /**< 16-bit little-endian value. */
typedef uint32_t rte_le32_t; /**< 32-bit little-endian value. */
typedef uint64_t rte_le64_t; /**< 64-bit little-endian value. */

#define RTE_ETHER_ADDR_LEN 6

/**
 * Matching pattern item types.
 *
 * Pattern items fall in two categories:
 *
 * - Matching protocol headers and packet data, usually associated with a
 *   specification structure. These must be stacked in the same order as the
 *   protocol layers to match inside packets, starting from the lowest.
 *
 * - Matching meta-data or affecting pattern processing, often without a
 *   specification structure. Since they do not match packet contents, their
 *   position in the list is usually not relevant.
 *
 * See the description of individual types for more information. Those
 * marked with [META] fall into the second category.
 */
enum rte_flow_item_type {
	/**
	 * [META]
	 *
	 * End marker for item lists. Prevents further processing of items,
	 * thereby ending the pattern.
	 *
	 * No associated specification structure.
	 */
	RTE_FLOW_ITEM_TYPE_END,

	/**
	 * [META]
	 *
	 * Used as a placeholder for convenience. It is ignored and simply
	 * discarded by PMDs.
	 *
	 * No associated specification structure.
	 */
	RTE_FLOW_ITEM_TYPE_VOID,

	/**
	 * [META]
	 *
	 * Inverted matching, i.e. process packets that do not match the
	 * pattern.
	 *
	 * No associated specification structure.
	 */
	RTE_FLOW_ITEM_TYPE_INVERT,

	/**
	 * Matches any protocol in place of the current layer, a single ANY
	 * may also stand for several protocol layers.
	 *
	 * See struct rte_flow_item_any.
	 */
	RTE_FLOW_ITEM_TYPE_ANY,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR
	 * @see RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT
	 *
	 * [META]
	 *
	 * Matches traffic originating from (ingress) or going to (egress)
	 * the physical function of the current device.
	 *
	 * No associated specification structure.
	 */
	RTE_FLOW_ITEM_TYPE_PF,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR
	 * @see RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT
	 *
	 * [META]
	 *
	 * Matches traffic originating from (ingress) or going to (egress) a
	 * given virtual function of the current device.
	 *
	 * See struct rte_flow_item_vf.
	 */
	RTE_FLOW_ITEM_TYPE_VF,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR
	 * @see RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT
	 *
	 * [META]
	 *
	 * Matches traffic originating from (ingress) or going to (egress) a
	 * physical port of the underlying device.
	 *
	 * See struct rte_flow_item_phy_port.
	 */
	RTE_FLOW_ITEM_TYPE_PHY_PORT,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR
	 * @see RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT
	 *
	 * [META]
	 *
	 * Matches traffic originating from (ingress) or going to (egress) a
	 * given DPDK port ID.
	 *
	 * See struct rte_flow_item_port_id.
	 */
	RTE_FLOW_ITEM_TYPE_PORT_ID,

	/**
	 * Matches a byte string of a given length at a given offset.
	 *
	 * See struct rte_flow_item_raw.
	 */
	RTE_FLOW_ITEM_TYPE_RAW,

	/**
	 * Matches an Ethernet header.
	 *
	 * See struct rte_flow_item_eth.
	 */
	RTE_FLOW_ITEM_TYPE_ETH,

	/**
	 * Matches an 802.1Q/ad VLAN tag.
	 *
	 * See struct rte_flow_item_vlan.
	 */
	RTE_FLOW_ITEM_TYPE_VLAN,

	/**
	 * Matches an IPv4 header.
	 *
	 * See struct rte_flow_item_ipv4.
	 */
	RTE_FLOW_ITEM_TYPE_IPV4,

	/**
	 * Matches an IPv6 header.
	 *
	 * See struct rte_flow_item_ipv6.
	 */
	RTE_FLOW_ITEM_TYPE_IPV6,

	/**
	 * Matches an ICMP header.
	 *
	 * See struct rte_flow_item_icmp.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP,

	/**
	 * Matches a UDP header.
	 *
	 * See struct rte_flow_item_udp.
	 */
	RTE_FLOW_ITEM_TYPE_UDP,

	/**
	 * Matches a TCP header.
	 *
	 * See struct rte_flow_item_tcp.
	 */
	RTE_FLOW_ITEM_TYPE_TCP,

	/**
	 * Matches a SCTP header.
	 *
	 * See struct rte_flow_item_sctp.
	 */
	RTE_FLOW_ITEM_TYPE_SCTP,

	/**
	 * Matches a VXLAN header.
	 *
	 * See struct rte_flow_item_vxlan.
	 */
	RTE_FLOW_ITEM_TYPE_VXLAN,

	/**
	 * Matches a E_TAG header.
	 *
	 * See struct rte_flow_item_e_tag.
	 */
	RTE_FLOW_ITEM_TYPE_E_TAG,

	/**
	 * Matches a NVGRE header.
	 *
	 * See struct rte_flow_item_nvgre.
	 */
	RTE_FLOW_ITEM_TYPE_NVGRE,

	/**
	 * Matches a MPLS header.
	 *
	 * See struct rte_flow_item_mpls.
	 */
	RTE_FLOW_ITEM_TYPE_MPLS,

	/**
	 * Matches a GRE header.
	 *
	 * See struct rte_flow_item_gre.
	 */
	RTE_FLOW_ITEM_TYPE_GRE,

	/**
	 * [META]
	 *
	 * Fuzzy pattern match, expect faster than default.
	 *
	 * This is for device that support fuzzy matching option.
	 * Usually a fuzzy matching is fast but the cost is accuracy.
	 *
	 * See struct rte_flow_item_fuzzy.
	 */
	RTE_FLOW_ITEM_TYPE_FUZZY,

	/**
	 * Matches a GTP header.
	 *
	 * Configure flow for GTP packets.
	 *
	 * See struct rte_flow_item_gtp.
	 */
	RTE_FLOW_ITEM_TYPE_GTP,

	/**
	 * Matches a GTP header.
	 *
	 * Configure flow for GTP-C packets.
	 *
	 * See struct rte_flow_item_gtp.
	 */
	RTE_FLOW_ITEM_TYPE_GTPC,

	/**
	 * Matches a GTP header.
	 *
	 * Configure flow for GTP-U packets.
	 *
	 * See struct rte_flow_item_gtp.
	 */
	RTE_FLOW_ITEM_TYPE_GTPU,

	/**
	 * Matches a ESP header.
	 *
	 * See struct rte_flow_item_esp.
	 */
	RTE_FLOW_ITEM_TYPE_ESP,

	/**
	 * Matches a GENEVE header.
	 *
	 * See struct rte_flow_item_geneve.
	 */
	RTE_FLOW_ITEM_TYPE_GENEVE,

	/**
	 * Matches a VXLAN-GPE header.
	 *
	 * See struct rte_flow_item_vxlan_gpe.
	 */
	RTE_FLOW_ITEM_TYPE_VXLAN_GPE,

	/**
	 * Matches an ARP header for Ethernet/IPv4.
	 *
	 * See struct rte_flow_item_arp_eth_ipv4.
	 */
	RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4,

	/**
	 * Matches the presence of any IPv6 extension header.
	 *
	 * See struct rte_flow_item_ipv6_ext.
	 */
	RTE_FLOW_ITEM_TYPE_IPV6_EXT,

	/**
	 * Matches any ICMPv6 header.
	 *
	 * See struct rte_flow_item_icmp6.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6,

	/**
	 * Matches an ICMPv6 neighbor discovery solicitation.
	 *
	 * See struct rte_flow_item_icmp6_nd_ns.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6_ND_NS,

	/**
	 * Matches an ICMPv6 neighbor discovery advertisement.
	 *
	 * See struct rte_flow_item_icmp6_nd_na.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6_ND_NA,

	/**
	 * Matches the presence of any ICMPv6 neighbor discovery option.
	 *
	 * See struct rte_flow_item_icmp6_nd_opt.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT,

	/**
	 * Matches an ICMPv6 neighbor discovery source Ethernet link-layer
	 * address option.
	 *
	 * See struct rte_flow_item_icmp6_nd_opt_sla_eth.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_SLA_ETH,

	/**
	 * Matches an ICMPv6 neighbor discovery target Ethernet link-layer
	 * address option.
	 *
	 * See struct rte_flow_item_icmp6_nd_opt_tla_eth.
	 */
	RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_TLA_ETH,

	/**
	 * Matches specified mark field.
	 *
	 * See struct rte_flow_item_mark.
	 */
	RTE_FLOW_ITEM_TYPE_MARK,

	/**
	 * [META]
	 *
	 * Matches a metadata value.
	 *
	 * See struct rte_flow_item_meta.
	 */
	RTE_FLOW_ITEM_TYPE_META,

	/**
	 * Matches a GRE optional key field.
	 *
	 * The value should a big-endian 32bit integer.
	 *
	 * When this item present the K bit is implicitly matched as "1"
	 * in the default mask.
	 *
	 * @p spec/mask type:
	 * @code rte_be32_t * @endcode
	 */
	RTE_FLOW_ITEM_TYPE_GRE_KEY,

	/**
	 * Matches a GTP extension header: PDU session container.
	 *
	 * Configure flow for GTP packets with extension header type 0x85.
	 *
	 * See struct rte_flow_item_gtp_psc.
	 */
	RTE_FLOW_ITEM_TYPE_GTP_PSC,

	/**
	 * Matches a PPPoE header.
	 *
	 * Configure flow for PPPoE session packets.
	 *
	 * See struct rte_flow_item_pppoe.
	 */
	RTE_FLOW_ITEM_TYPE_PPPOES,

	/**
	 * Matches a PPPoE header.
	 *
	 * Configure flow for PPPoE discovery packets.
	 *
	 * See struct rte_flow_item_pppoe.
	 */
	RTE_FLOW_ITEM_TYPE_PPPOED,

	/**
	 * Matches a PPPoE optional proto_id field.
	 *
	 * It only applies to PPPoE session packets.
	 *
	 * See struct rte_flow_item_pppoe_proto_id.
	 */
	RTE_FLOW_ITEM_TYPE_PPPOE_PROTO_ID,

	/**
	 * Matches Network service header (NSH).
	 * See struct rte_flow_item_nsh.
	 *
	 */
	RTE_FLOW_ITEM_TYPE_NSH,

	/**
	 * Matches Internet Group Management Protocol (IGMP).
	 * See struct rte_flow_item_igmp.
	 *
	 */
	RTE_FLOW_ITEM_TYPE_IGMP,

	/**
	 * Matches IP Authentication Header (AH).
	 * See struct rte_flow_item_ah.
	 *
	 */
	RTE_FLOW_ITEM_TYPE_AH,
	/**
	 * Matches the presence of any IPv6 routing extension header.
	 *
	 * See struct rte_flow_item_ipv6_route_ext.
	 */
	RTE_FLOW_ITEM_TYPE_IPV6_ROUTE_EXT,

	/**
	 * Matches a HIGIG header.
	 * see struct rte_flow_item_higig2_hdr.
	 */
	RTE_FLOW_ITEM_TYPE_HIGIG2,

	/**
	 * [META]
	 *
	 * Matches a tag value.
	 *
	 * See struct rte_flow_item_tag.
	 */
	RTE_FLOW_ITEM_TYPE_TAG,

	/**
	 * Matches a L2TPv3 over IP header.
	 *
	 * Configure flow for L2TPv3 over IP packets.
	 *
	 * See struct rte_flow_item_l2tpv3oip.
	 */
	RTE_FLOW_ITEM_TYPE_L2TPV3OIP,

	/**
	 * Matches PFCP Header.
	 * See struct rte_flow_item_pfcp.
	 *
	 */
	RTE_FLOW_ITEM_TYPE_PFCP,

	/**
	 * Matches eCPRI Header.
	 *
	 * Configure flow for eCPRI over ETH or UDP packets.
	 *
	 * See struct rte_flow_item_ecpri.
	 */
	RTE_FLOW_ITEM_TYPE_ECPRI,

	/**
	 * Matches the presence of IPv6 fragment extension header.
	 *
	 * See struct rte_flow_item_ipv6_frag_ext.
	 */
	RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT,

	/**
	 * Matches Geneve Variable Length Option
	 *
	 * See struct rte_flow_item_geneve_opt
	 */
	RTE_FLOW_ITEM_TYPE_GENEVE_OPT,

	/**
	 * [META]
	 *
	 * Matches on packet integrity.
	 * For some devices application needs to enable integration checks in HW
	 * before using this item.
	 *
	 * @see struct rte_flow_item_integrity.
	 */
	RTE_FLOW_ITEM_TYPE_INTEGRITY,

	/**
	 * [META]
	 *
	 * Matches conntrack state.
	 *
	 * @see struct rte_flow_item_conntrack.
	 */
	RTE_FLOW_ITEM_TYPE_CONNTRACK,

	/**
	 * [META]
	 *
	 * Matches traffic entering the embedded switch from the given ethdev.
	 *
	 * @see struct rte_flow_item_ethdev
	 */
	RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR,

	/**
	 * [META]
	 *
	 * Matches traffic entering the embedded switch from
	 * the entity represented by the given ethdev.
	 *
	 * @see struct rte_flow_item_ethdev
	 */
	RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,

	/**
	 * Matches a configured set of fields at runtime calculated offsets
	 * over the generic network header with variable length and
	 * flexible pattern
	 *
	 * @see struct rte_flow_item_flex.
	 */
	RTE_FLOW_ITEM_TYPE_FLEX,

	/**
	 * Matches L2TPv2 Header.
	 *
	 * See struct rte_flow_item_l2tpv2.
	 */
	RTE_FLOW_ITEM_TYPE_L2TPV2,

	/**
	 * Matches PPP Header.
	 *
	 * See struct rte_flow_item_ppp.
	 */
	RTE_FLOW_ITEM_TYPE_PPP,
};

/**
 * Matching pattern item definition.
 *
 * A pattern is formed by stacking items starting from the lowest protocol
 * layer to match. This stacking restriction does not apply to meta items
 * which can be placed anywhere in the stack without affecting the meaning
 * of the resulting pattern.
 *
 * Patterns are terminated by END items.
 *
 * The spec field should be a valid pointer to a structure of the related
 * item type. It may remain unspecified (NULL) in many cases to request
 * broad (nonspecific) matching. In such cases, last and mask must also be
 * set to NULL.
 *
 * Optionally, last can point to a structure of the same type to define an
 * inclusive range. This is mostly supported by integer and address fields,
 * may cause errors otherwise. Fields that do not support ranges must be set
 * to 0 or to the same value as the corresponding fields in spec.
 *
 * Only the fields defined to nonzero values in the default masks (see
 * rte_flow_item_{name}_mask constants) are considered relevant by
 * default. This can be overridden by providing a mask structure of the
 * same type with applicable bits set to one. It can also be used to
 * partially filter out specific fields (e.g. as an alternate mean to match
 * ranges of IP addresses).
 *
 * Mask is a simple bit-mask applied before interpreting the contents of
 * spec and last, which may yield unexpected results if not used
 * carefully. For example, if for an IPv4 address field, spec provides
 * 10.1.2.3, last provides 10.3.4.5 and mask provides 255.255.0.0, the
 * effective range becomes 10.1.0.0 to 10.3.255.255.
 */
struct rte_flow_item {
	enum rte_flow_item_type type; /**< Item type. */
	const void *spec; /**< Pointer to item specification structure. */
	const void *last; /**< Defines an inclusive range (spec to last). */
	const void *mask; /**< Bit-mask applied to spec and last. */
};

/**
 * Action types.
 *
 * Each possible action is represented by a type.
 * An action can have an associated configuration object.
 * Several actions combined in a list can be assigned
 * to a flow rule and are performed in order.
 *
 * They fall in three categories:
 *
 * - Actions that modify the fate of matching traffic, for instance by
 *   dropping or assigning it a specific destination.
 *
 * - Actions that modify matching traffic contents or its properties. This
 *   includes adding/removing encapsulation, encryption, compression and
 *   marks.
 *
 * - Actions related to the flow rule itself, such as updating counters or
 *   making it non-terminating.
 *
 * Flow rules being terminating by default, not specifying any action of the
 * fate kind results in undefined behavior. This applies to both ingress and
 * egress.
 *
 * PASSTHRU, when supported, makes a flow rule non-terminating.
 */
enum rte_flow_action_type {
	/**
	 * End marker for action lists. Prevents further processing of
	 * actions, thereby ending the list.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_END,

	/**
	 * Used as a placeholder for convenience. It is ignored and simply
	 * discarded by PMDs.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_VOID,

	/**
	 * Leaves traffic up for additional processing by subsequent flow
	 * rules; makes a flow rule non-terminating.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_PASSTHRU,

	/**
	 * RTE_FLOW_ACTION_TYPE_JUMP
	 *
	 * Redirects packets to a group on the current device.
	 *
	 * See struct rte_flow_action_jump.
	 */
	RTE_FLOW_ACTION_TYPE_JUMP,

	/**
	 * Attaches an integer value to packets and sets RTE_MBUF_F_RX_FDIR and
	 * RTE_MBUF_F_RX_FDIR_ID mbuf flags.
	 *
	 * See struct rte_flow_action_mark.
	 *
	 * One should negotiate mark delivery from the NIC to the PMD.
	 * @see rte_eth_rx_metadata_negotiate()
	 * @see RTE_ETH_RX_METADATA_USER_MARK
	 */
	RTE_FLOW_ACTION_TYPE_MARK,

	/**
	 * Flags packets. Similar to MARK without a specific value; only
	 * sets the RTE_MBUF_F_RX_FDIR mbuf flag.
	 *
	 * No associated configuration structure.
	 *
	 * One should negotiate flag delivery from the NIC to the PMD.
	 * @see rte_eth_rx_metadata_negotiate()
	 * @see RTE_ETH_RX_METADATA_USER_FLAG
	 */
	RTE_FLOW_ACTION_TYPE_FLAG,

	/**
	 * Assigns packets to a given queue index.
	 *
	 * See struct rte_flow_action_queue.
	 */
	RTE_FLOW_ACTION_TYPE_QUEUE,

	/**
	 * Drops packets.
	 *
	 * PASSTHRU overrides this action if both are specified.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_DROP,

	/**
	 * Enables counters for this flow rule.
	 *
	 * These counters can be retrieved and reset through rte_flow_query() or
	 * rte_flow_action_handle_query() if the action provided via handle,
	 * see struct rte_flow_query_count.
	 *
	 * See struct rte_flow_action_count.
	 */
	RTE_FLOW_ACTION_TYPE_COUNT,

	/**
	 * Similar to QUEUE, except RSS is additionally performed on packets
	 * to spread them among several queues according to the provided
	 * parameters.
	 *
	 * See struct rte_flow_action_rss.
	 */
	RTE_FLOW_ACTION_TYPE_RSS,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR
	 * @see RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT
	 *
	 * Directs matching traffic to the physical function (PF) of the
	 * current device.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_PF,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR
	 * @see RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT
	 *
	 * Directs matching traffic to a given virtual function of the
	 * current device.
	 *
	 * See struct rte_flow_action_vf.
	 */
	RTE_FLOW_ACTION_TYPE_VF,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR
	 * @see RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT
	 *
	 * Directs matching traffic to a given DPDK port ID.
	 *
	 * See struct rte_flow_action_port_id.
	 */
	RTE_FLOW_ACTION_TYPE_PORT_ID,

	/**
	 * Traffic metering and policing (MTR).
	 *
	 * See struct rte_flow_action_meter.
	 * See file rte_mtr.h for MTR object configuration.
	 */
	RTE_FLOW_ACTION_TYPE_METER,

	/**
	 * Redirects packets to security engine of current device for security
	 * processing as specified by security session.
	 *
	 * See struct rte_flow_action_security.
	 */
	RTE_FLOW_ACTION_TYPE_SECURITY,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Implements OFPAT_DEC_NW_TTL ("decrement IP TTL") as defined by
	 * the OpenFlow Switch Specification.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_OF_DEC_NW_TTL,

	/**
	 * Implements OFPAT_POP_VLAN ("pop the outer VLAN tag") as defined
	 * by the OpenFlow Switch Specification.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_OF_POP_VLAN,

	/**
	 * Implements OFPAT_PUSH_VLAN ("push a new VLAN tag") as defined by
	 * the OpenFlow Switch Specification.
	 *
	 * See struct rte_flow_action_of_push_vlan.
	 */
	RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN,

	/**
	 * Implements OFPAT_SET_VLAN_VID ("set the 802.1q VLAN ID") as
	 * defined by the OpenFlow Switch Specification.
	 *
	 * See struct rte_flow_action_of_set_vlan_vid.
	 */
	RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID,

	/**
	 * Implements OFPAT_SET_LAN_PCP ("set the 802.1q priority") as
	 * defined by the OpenFlow Switch Specification.
	 *
	 * See struct rte_flow_action_of_set_vlan_pcp.
	 */
	RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP,

	/**
	 * Implements OFPAT_POP_MPLS ("pop the outer MPLS tag") as defined
	 * by the OpenFlow Switch Specification.
	 *
	 * See struct rte_flow_action_of_pop_mpls.
	 */
	RTE_FLOW_ACTION_TYPE_OF_POP_MPLS,

	/**
	 * Implements OFPAT_PUSH_MPLS ("push a new MPLS tag") as defined by
	 * the OpenFlow Switch Specification.
	 *
	 * See struct rte_flow_action_of_push_mpls.
	 */
	RTE_FLOW_ACTION_TYPE_OF_PUSH_MPLS,

	/**
	 * Encapsulate flow in VXLAN tunnel as defined in
	 * rte_flow_action_vxlan_encap action structure.
	 *
	 * See struct rte_flow_action_vxlan_encap.
	 */
	RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP,

	/**
	 * Decapsulate outer most VXLAN tunnel from matched flow.
	 *
	 * If flow pattern does not define a valid VXLAN tunnel (as specified by
	 * RFC7348) then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION
	 * error.
	 */
	RTE_FLOW_ACTION_TYPE_VXLAN_DECAP,

	/**
	 * Encapsulate flow in SRv6 Header as defined in
	 * rte_flow_action_ip_encap action structure.
	 *
	 * See struct rte_flow_action_ip_encap.
	 */
	RTE_FLOW_ACTION_TYPE_IP_ENCAP,

	/**
	 * Decapsulate outer most SRv6 header from matched flow.
	 */
	RTE_FLOW_ACTION_TYPE_IP_DECAP,

	/**
	 * Encapsulate flow in NVGRE tunnel defined in the
	 * rte_flow_action_nvgre_encap action structure.
	 *
	 * See struct rte_flow_action_nvgre_encap.
	 */
	RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP,

	/**
	 * Decapsulate outer most NVGRE tunnel from matched flow.
	 *
	 * If flow pattern does not define a valid NVGRE tunnel (as specified by
	 * RFC7637) then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION
	 * error.
	 */
	RTE_FLOW_ACTION_TYPE_NVGRE_DECAP,

	/**
	 * Add outer header whose template is provided in its data buffer
	 *
	 * See struct rte_flow_action_raw_encap.
	 */
	RTE_FLOW_ACTION_TYPE_RAW_ENCAP,

	/**
	 * Remove outer header whose template is provided in its data buffer.
	 *
	 * See struct rte_flow_action_raw_decap
	 */
	RTE_FLOW_ACTION_TYPE_RAW_DECAP,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify IPv4 source address in the outermost IPv4 header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV4,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_ipv4.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify IPv4 destination address in the outermost IPv4 header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV4,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_ipv4.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV4_DST,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify IPv6 source address in the outermost IPv6 header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV6,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_ipv6.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify IPv6 destination address in the outermost IPv6 header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV6,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_ipv6.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV6_DST,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify source port number in the outermost TCP/UDP header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_TCP
	 * or RTE_FLOW_ITEM_TYPE_UDP, then the PMD should return a
	 * RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_tp.
	 */
	RTE_FLOW_ACTION_TYPE_SET_TP_SRC,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify destination port number in the outermost TCP/UDP header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_TCP
	 * or RTE_FLOW_ITEM_TYPE_UDP, then the PMD should return a
	 * RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_tp.
	 */
	RTE_FLOW_ACTION_TYPE_SET_TP_DST,

	/**
	 * Swap the source and destination MAC addresses in the outermost
	 * Ethernet header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_ETH,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_MAC_SWAP,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Decrease TTL value directly
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_DEC_TTL,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Set TTL value
	 *
	 * See struct rte_flow_action_set_ttl
	 */
	RTE_FLOW_ACTION_TYPE_SET_TTL,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Set source MAC address from matched flow.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_ETH,
	 * the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_mac.
	 */
	RTE_FLOW_ACTION_TYPE_SET_MAC_SRC,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Set destination MAC address from matched flow.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_ETH,
	 * the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_mac.
	 */
	RTE_FLOW_ACTION_TYPE_SET_MAC_DST,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Increase sequence number in the outermost TCP header.
	 *
	 * Action configuration specifies the value to increase
	 * TCP sequence number as a big-endian 32 bit integer.
	 *
	 * @p conf type:
	 * @code rte_be32_t * @endcode
	 *
	 * Using this action on non-matching traffic will result in
	 * undefined behavior.
	 */
	RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Decrease sequence number in the outermost TCP header.
	 *
	 * Action configuration specifies the value to decrease
	 * TCP sequence number as a big-endian 32 bit integer.
	 *
	 * @p conf type:
	 * @code rte_be32_t * @endcode
	 *
	 * Using this action on non-matching traffic will result in
	 * undefined behavior.
	 */
	RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Increase acknowledgment number in the outermost TCP header.
	 *
	 * Action configuration specifies the value to increase
	 * TCP acknowledgment number as a big-endian 32 bit integer.
	 *
	 * @p conf type:
	 * @code rte_be32_t * @endcode

	 * Using this action on non-matching traffic will result in
	 * undefined behavior.
	 */
	RTE_FLOW_ACTION_TYPE_INC_TCP_ACK,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Decrease acknowledgment number in the outermost TCP header.
	 *
	 * Action configuration specifies the value to decrease
	 * TCP acknowledgment number as a big-endian 32 bit integer.
	 *
	 * @p conf type:
	 * @code rte_be32_t * @endcode
	 *
	 * Using this action on non-matching traffic will result in
	 * undefined behavior.
	 */
	RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Set Tag.
	 *
	 * Tag is for internal flow usage only and
	 * is not delivered to the application.
	 *
	 * See struct rte_flow_action_set_tag.
	 */
	RTE_FLOW_ACTION_TYPE_SET_TAG,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Set metadata on ingress or egress path.
	 *
	 * See struct rte_flow_action_set_meta.
	 */
	RTE_FLOW_ACTION_TYPE_SET_META,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify IPv4 DSCP in the outermost IP header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV4,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_dscp.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP,

	/**
	 * @warning This is a legacy action.
	 * @see RTE_FLOW_ACTION_TYPE_MODIFY_FIELD
	 *
	 * Modify IPv6 DSCP in the outermost IP header.
	 *
	 * If flow pattern does not define a valid RTE_FLOW_ITEM_TYPE_IPV6,
	 * then the PMD should return a RTE_FLOW_ERROR_TYPE_ACTION error.
	 *
	 * See struct rte_flow_action_set_dscp.
	 */
	RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP,

	/**
	 * Report as aged flow if timeout passed without any matching on the
	 * flow.
	 *
	 * See struct rte_flow_action_age.
	 * See function rte_flow_get_q_aged_flows
	 * See function rte_flow_get_aged_flows
	 * see enum RTE_ETH_EVENT_FLOW_AGED
	 * See struct rte_flow_query_age
	 * See struct rte_flow_update_age
	 */
	RTE_FLOW_ACTION_TYPE_AGE,

	/**
	 * The matching packets will be duplicated with specified ratio and
	 * applied with own set of actions with a fate action.
	 *
	 * See struct rte_flow_action_sample.
	 */
	RTE_FLOW_ACTION_TYPE_SAMPLE,

	/**
	 * @deprecated
	 * @see RTE_FLOW_ACTION_TYPE_INDIRECT
	 *
	 * Describe action shared across multiple flow rules.
	 *
	 * Allow multiple rules reference the same action by handle (see
	 * struct rte_flow_shared_action).
	 */
	RTE_FLOW_ACTION_TYPE_SHARED,

	/**
	 * Modify a packet header field, tag, mark or metadata.
	 *
	 * Allow the modification of an arbitrary header field via
	 * set, add and sub operations or copying its content into
	 * tag, meta or mark for future processing.
	 *
	 * See struct rte_flow_action_modify_field.
	 */
	RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,

	/**
	 * An action handle is referenced in a rule through an indirect action.
	 *
	 * The same action handle may be used in multiple rules for the same
	 * or different ethdev ports.
	 */
	RTE_FLOW_ACTION_TYPE_INDIRECT,

	/**
	 * [META]
	 *
	 * Enable tracking a TCP connection state.
	 *
	 * @see struct rte_flow_action_conntrack.
	 */
	RTE_FLOW_ACTION_TYPE_CONNTRACK,

	/**
	 * Color the packet to reflect the meter color result.
	 * Set the meter color in the mbuf to the selected color.
	 *
	 * See struct rte_flow_action_meter_color.
	 */
	RTE_FLOW_ACTION_TYPE_METER_COLOR,

	/**
	 * At embedded switch level, sends matching traffic to the given ethdev.
	 *
	 * @see struct rte_flow_action_ethdev
	 */
	RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR,

	/**
	 * At embedded switch level, send matching traffic to
	 * the entity represented by the given ethdev.
	 *
	 * @see struct rte_flow_action_ethdev
	 */
	RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT,

	/**
	 * Traffic metering and marking (MTR).
	 *
	 * @see struct rte_flow_action_meter_mark
	 * See file rte_mtr.h for MTR profile object configuration.
	 */
	RTE_FLOW_ACTION_TYPE_METER_MARK,

	/**
	 * Send packets to the kernel, without going to userspace at all.
	 * The packets will be received by the kernel driver sharing
	 * the same device as the DPDK port on which this action is configured.
	 * This action mostly suits bifurcated driver model.
	 * This is an ingress non-transfer action only.
	 *
	 * No associated configuration structure.
	 */
	RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL,
};

/**
 * Definition of a single action.
 *
 * A list of actions is terminated by a END action.
 *
 * For simple actions without a configuration object, conf remains NULL.
 */
struct rte_flow_action {
	enum rte_flow_action_type type; /**< Action type. */
	const void *conf; /**< Pointer to action configuration object. */
};

/**
 * RTE_FLOW_ACTION_TYPE_QUEUE
 *
 * Assign packets to a given queue index.
 */
struct rte_flow_action_queue {
	uint16_t index; /**< Queue index to use. */
};

/**
 * @warning
 * @b EXPERIMENTAL: this structure may change without prior notice
 *
 * RTE_FLOW_ACTION_TYPE_COUNT
 *
 * Adds a counter action to a matched flow.
 *
 * If more than one count action is specified in a single flow rule, then each
 * action must specify a unique ID.
 *
 * Counters can be retrieved and reset through ``rte_flow_query()``, see
 * ``struct rte_flow_query_count``.
 *
 * For ports within the same switch domain then the counter ID namespace extends
 * to all ports within that switch domain.
 */
struct rte_flow_action_count {
	uint32_t id; /**< Counter ID. */
};

/**
 * Ethernet address:
 * A universally administered address is uniquely assigned to a device by its
 * manufacturer. The first three octets (in transmission order) contain the
 * Organizationally Unique Identifier (OUI). The following three (MAC-48 and
 * EUI-48) octets are assigned by that organization with the only constraint
 * of uniqueness.
 * A locally administered address is assigned to a device by a network
 * administrator and does not contain OUIs.
 * See http://standards.ieee.org/regauth/groupmac/tutorial.html
 */
struct rte_ether_addr {
	uint8_t addr_bytes[RTE_ETHER_ADDR_LEN]; /**< Addr bytes in tx order */
/* TBD: } __rte_aligned(2); */
};

/**
 * Ethernet header: Contains the destination address, source address
 * and frame type.
 */
struct rte_ether_hdr {
	struct rte_ether_addr dst_addr; /**< Destination address. */
	struct rte_ether_addr src_addr; /**< Source address. */
	rte_be16_t ether_type; /**< Frame type. */
/* TBD: } __rte_aligned(2); */
};

#define	__rte_packed	__packed
#define RTE_BIG_ENDIAN    1
#define RTE_LITTLE_ENDIAN    2
#define	RTE_BYTE_ORDER	RTE_BIG_ENDIAN
/**
 * IPv4 Header
 */
struct rte_ipv4_hdr {
	__extension__
		union {
			uint8_t version_ihl;    /**< version and header length */
			struct {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
				uint8_t ihl:4;     /**< header length */
				uint8_t version:4; /**< version */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
				uint8_t version:4; /**< version */
				uint8_t ihl:4;     /**< header length */
#endif
			};
		};
	uint8_t  type_of_service;       /**< type of service */
	rte_be16_t total_length;        /**< length of packet */
	rte_be16_t packet_id;           /**< packet ID */
	rte_be16_t fragment_offset;     /**< fragmentation offset */
	uint8_t  time_to_live;          /**< time to live */
	uint8_t  next_proto_id;         /**< protocol ID */
	rte_be16_t hdr_checksum;        /**< header checksum */
	rte_be32_t src_addr;            /**< source address */
	rte_be32_t dst_addr;            /**< destination address */
} __rte_packed;

/**
 * IPv6 Header
 */
struct rte_ipv6_hdr {
	rte_be32_t vtc_flow;    /**< IP version, traffic class & flow label. */
	rte_be16_t payload_len; /**< IP payload size, including ext. headers */
	uint8_t  proto;         /**< Protocol, next header. */
	uint8_t  hop_limits;    /**< Hop limits. */
	uint8_t  src_addr[16];  /**< IP address of source host. */
	uint8_t  dst_addr[16];  /**< IP address of destination host(s). */
} __rte_packed;

/**
 * Ethernet VLAN Header.
 * Contains the 16-bit VLAN Tag Control Identifier and the Ethernet type
 * of the encapsulated frame.
 */
struct rte_vlan_hdr {
	rte_be16_t vlan_tci;  /**< Priority (3) + CFI (1) + Identifier Code (12) */
	rte_be16_t eth_proto; /**< Ethernet type of encapsulated frame. */
} __rte_packed;

/**
 * TCP Header
 */
struct rte_tcp_hdr {
	rte_be16_t src_port; /**< TCP source port. */
	rte_be16_t dst_port; /**< TCP destination port. */
	rte_be32_t sent_seq; /**< TX data sequence number. */
	rte_be32_t recv_ack; /**< RX data acknowledgment sequence number. */
	uint8_t  data_off;   /**< Data offset. */
	uint8_t  tcp_flags;  /**< TCP flags */
	rte_be16_t rx_win;   /**< RX flow control window. */
	rte_be16_t cksum;    /**< TCP checksum. */
	rte_be16_t tcp_urp;  /**< TCP urgent pointer, if any. */
} __rte_packed;

/**
 * UDP Header
 */
struct rte_udp_hdr {
	rte_be16_t src_port;    /**< UDP source port. */
	rte_be16_t dst_port;    /**< UDP destination port. */
	rte_be16_t dgram_len;   /**< UDP datagram length */
	rte_be16_t dgram_cksum; /**< UDP datagram checksum */
} __rte_packed;

/**
 * VXLAN protocol header.
 * Contains the 8-bit flag, 24-bit VXLAN Network Identifier and
 * Reserved fields (24 bits and 8 bits)
 */
struct rte_vxlan_hdr {
	rte_be32_t vx_flags; /**< flag (8) + Reserved (24). */
	rte_be32_t vx_vni;   /**< VNI (24) + Reserved (8). */
} __rte_packed;

/**
 * GRE Header
 */
__extension__
struct rte_gre_hdr {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint16_t res2:4; /**< Reserved */
	uint16_t s:1;    /**< Sequence Number Present bit */
	uint16_t k:1;    /**< Key Present bit */
	uint16_t res1:1; /**< Reserved */
	uint16_t c:1;    /**< Checksum Present bit */
	uint16_t ver:3;  /**< Version Number */
	uint16_t res3:5; /**< Reserved */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint16_t c:1;    /**< Checksum Present bit */
	uint16_t res1:1; /**< Reserved */
	uint16_t k:1;    /**< Key Present bit */
	uint16_t s:1;    /**< Sequence Number Present bit */
	uint16_t res2:4; /**< Reserved */
	uint16_t res3:5; /**< Reserved */
	uint16_t ver:3;  /**< Version Number */
#endif
	uint16_t proto;  /**< Protocol Type */
} __rte_packed;

struct rte_flow_item_eth {
	union {
		struct {
			/*
			 * These fields are retained for compatibility.
			 * Please switch to the new header field below.
			 */
			struct rte_ether_addr dst; /**< Destination MAC. */
			struct rte_ether_addr src; /**< Source MAC. */
			rte_be16_t type; /**< EtherType or TPID. */
		};
		struct rte_ether_hdr hdr;
	};
	uint32_t has_vlan:1; /**< Packet header contains at least one VLAN. */
	uint32_t reserved:31; /**< Reserved, must be zero. */
};

/**
 * RTE_FLOW_ITEM_TYPE_VLAN
 *
 * Matches an 802.1Q/ad VLAN tag.
 *
 * The corresponding standard outer EtherType (TPID) values are
 * RTE_ETHER_TYPE_VLAN or RTE_ETHER_TYPE_QINQ. It can be overridden by
 * the preceding pattern item.
 * If a @p VLAN item is present in the pattern, then only tagged packets will
 * match the pattern.
 * The field @p has_more_vlan can be used to match any type of tagged packets,
 * instead of using the @p eth_proto field of @p hdr.
 * If the @p eth_proto of @p hdr and @p has_more_vlan fields are not specified,
 * then any tagged packets will match the pattern.
 */
struct rte_flow_item_vlan {
	union {
		struct {
			/*
			 * These fields are retained for compatibility.
			 * Please switch to the new header field below.
			 */
			rte_be16_t tci; /**< Tag control information. */
			rte_be16_t inner_type; /**< Inner EtherType or TPID. */
		};
		struct rte_vlan_hdr hdr;
	};
	/** Packet header contains at least one more VLAN, after this VLAN. */
	uint32_t has_more_vlan:1;
	uint32_t reserved:31; /**< Reserved, must be zero. */
};

/**
 * RTE_FLOW_ITEM_TYPE_IPV4
 *
 * Matches an IPv4 header.
 *
 * Note: IPv4 options are handled by dedicated pattern items.
 */
struct rte_flow_item_ipv4 {
	struct rte_ipv4_hdr hdr; /**< IPv4 header definition. */
};

/**
 * RTE_FLOW_ITEM_TYPE_IPV6.
 *
 * Matches an IPv6 header.
 *
 * Dedicated flags indicate if header contains specific extension headers.
 */
struct rte_flow_item_ipv6 {
	struct rte_ipv6_hdr hdr; /**< IPv6 header definition. */
	/** Header contains Hop-by-Hop Options extension header. */
	uint32_t has_hop_ext:1;
	/** Header contains Routing extension header. */
	uint32_t has_route_ext:1;
	/** Header contains Fragment extension header. */
	uint32_t has_frag_ext:1;
	/** Header contains Authentication extension header. */
	uint32_t has_auth_ext:1;
	/** Header contains Encapsulation Security Payload extension header. */
	uint32_t has_esp_ext:1;
	/** Header contains Destination Options extension header. */
	uint32_t has_dest_ext:1;
	/** Header contains Mobility extension header. */
	uint32_t has_mobil_ext:1;
	/** Header contains Host Identity Protocol extension header. */
	uint32_t has_hip_ext:1;
	/** Header contains Shim6 Protocol extension header. */
	uint32_t has_shim6_ext:1;
	/** Reserved for future extension headers, must be zero. */
	uint32_t reserved:23;
};

/**
 * RTE_FLOW_ITEM_TYPE_UDP.
 *
 * Matches a UDP header.
 */
struct rte_flow_item_udp {
	struct rte_udp_hdr hdr; /**< UDP header definition. */
};

/**
 * RTE_FLOW_ITEM_TYPE_TCP.
 *
 * Matches a TCP header.
 */
struct rte_flow_item_tcp {
	struct rte_tcp_hdr hdr; /**< TCP header definition. */
};

/**
 * RTE_FLOW_ITEM_TYPE_VXLAN.
 *
 * Matches a VXLAN header (RFC 7348).
 */
struct rte_flow_item_vxlan {
	union {
		struct {
			/*
			 * These fields are retained for compatibility.
			 * Please switch to the new header field below.
			 */
			uint8_t flags; /**< Normally 0x08 (I flag). */
			uint8_t rsvd0[3]; /**< Reserved, normally 0x000000. */
			uint8_t vni[3]; /**< VXLAN identifier. */
			uint8_t rsvd1; /**< Reserved, normally 0x00. */
		};
		struct rte_vxlan_hdr hdr;
	};
};

/**
 * RTE_FLOW_ITEM_TYPE_GRE.
 *
 * Matches a GRE header.
 */
struct rte_flow_item_gre {
	/**
	 * Checksum (1b), reserved 0 (12b), version (3b).
	 * Refer to RFC 2784.
	 */
	rte_be16_t c_rsvd0_ver;
	rte_be16_t protocol; /**< Protocol type. */
};

#define RTE_ETHER_GROUP_ADDR  0x01 /**< Multicast or broadcast Eth. address. */
static inline int rte_is_multicast_ether_addr(const struct rte_ether_addr *ea)
{
	return ea->addr_bytes[0] & RTE_ETHER_GROUP_ADDR;
}

/**
 * Check if an Ethernet address is a broadcast address.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is a broadcast address;
 *   false (0) otherwise.
 */
static inline int rte_is_broadcast_ether_addr(const struct rte_ether_addr *ea)
{
	const uint16_t *w = (const uint16_t *)ea;

	return (w[0] & w[1] & w[2]) == 0xFFFF;
}

int ulp_tc_rte_create_all_flows(struct bnxt *bp, int count);
/** Create IPv4 address */
#define RTE_IPV4(a, b, c, d) ((uint32_t)(((a) & 0xff) << 24) | \
			      (((b) & 0xff) << 16) | \
			      (((c) & 0xff) << 8)  | \
			      ((d) & 0xff))

#endif
#endif
