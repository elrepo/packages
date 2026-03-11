/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_GENERIC_FLOW_OFFLOAD_H_
#define _ULP_GENERIC_FLOW_OFFLOAD_H_

/* All arguments are expected to be in big-endian */

#define ACT_OFFS_MASK	0x6ffffff
#define ACT_OFFS_DIV	2
#define TSID_SHIFT	26
#define TSID_MASK	0x1f

#define BNXT_IPPROTO_IFA 253
#define BNXT_ROCE_CNP_OPCODE	0x81

/* Fields that are NULL will not be included in the key */
struct bnxt_ulp_gen_eth_hdr {
	u8 *dst;		/* Destination MAC */
	u8 *src;		/* Source MAC. */
	u16 *type;		/* EtherType or TPID. */
};

enum bnxt_ulp_gen_l2_class_type {
	BNXT_ULP_GEN_L2_NONE,
	BNXT_ULP_GEN_L2_L2_FILTER_ID,
	BNXT_ULP_GEN_L2_L2_HDR,
	BNXT_ULP_GEN_L2_LAST
};

struct bnxt_ulp_gen_l2_hdr_parms {
	enum bnxt_ulp_gen_l2_class_type type;
	union {
		uint64_t *l2_filter_id;
		struct {
			struct bnxt_ulp_gen_eth_hdr *eth_spec;
			struct bnxt_ulp_gen_eth_hdr *eth_mask;
		};
	};
};

enum bnxt_ulp_gen_l3_type {
	BNXT_ULP_GEN_L3_NONE,
	BNXT_ULP_GEN_L3_IPV4,
	BNXT_ULP_GEN_L3_IPV6,
	BNXT_ULP_GEN_L3_LAST
};

/* Fields that are NULL will not be included in the key */
struct bnxt_ulp_gen_ipv4_hdr {
	u32 *sip;		/* IPV4 Source Address. */
	u32 *dip;		/* IPV4 Destination Address. */
	u8 *proto;		/* IP Protocol */
};

/* Fields that are NULL will not be included in the key */
struct bnxt_ulp_gen_ipv6_hdr {
	u32 *vtc_flow;		/* IP version, traffic class & flow label. */
	u16 *payload_len;	/* IP payload size, including ext. headers */
	u8 *proto6;		/* Next Header */
	u8 *hop_limits;		/* Hop limits. */
	u8 *sip6;		/* IPV6 Source Address. */
	u8 *dip6;		/* IPV6 Destination Address. */
};

struct bnxt_ulp_gen_l3_hdr_parms {
	enum bnxt_ulp_gen_l3_type type;
	union {
		struct {
			struct bnxt_ulp_gen_ipv6_hdr *v6_spec;
			struct bnxt_ulp_gen_ipv6_hdr *v6_mask;
		};
		struct {
			struct bnxt_ulp_gen_ipv4_hdr *v4_spec;
			struct bnxt_ulp_gen_ipv4_hdr *v4_mask;
		};
	};
};

/* Fields that are NULL will not be included in the key */
struct bnxt_ulp_gen_udp_hdr {
	u16 *sport;		/* Source Port. */
	u16 *dport;		/* Destination Port */
};

/* Fields that are NULL will not be included in the key */
struct bnxt_ulp_gen_tcp_hdr {
	u16 *sport;		/* Source Port. */
	u16 *dport;		/* Destination Port */
};

/* Fields that are NULL will not be included in the key */
struct bnxt_ulp_gen_bth_hdr {
	u16 *op_code;		/* RoCE: L4 dstport == BTH.OpCode */
	u32 *dst_qpn;		/* RoCE: L4 ack_num == BTH.dstQP */
	u16 *bth_flags;		/* RoCE: L4 flags == BTH.flags */
};

enum bnxt_ulp_gen_l4_hdr_type {
	BNXT_ULP_GEN_L4_NONE,
	BNXT_ULP_GEN_L4_UDP,
	BNXT_ULP_GEN_L4_TCP,
	BNXT_ULP_GEN_L4_BTH,
	BNXT_ULP_GEN_L4_LAST
};

struct bnxt_ulp_gen_l4_hdr_parms {
	enum bnxt_ulp_gen_l4_hdr_type type;
	struct {
		struct bnxt_ulp_gen_udp_hdr *udp_spec;
		struct bnxt_ulp_gen_udp_hdr *udp_mask;
	};
	struct {
		struct bnxt_ulp_gen_tcp_hdr *tcp_spec;
		struct bnxt_ulp_gen_tcp_hdr *tcp_mask;
	};
	struct {
		struct bnxt_ulp_gen_bth_hdr *bth_spec;
		struct bnxt_ulp_gen_bth_hdr *bth_mask;
	};
};

struct bnxt_ulp_gen_ifa_hdr {
	u32 *gns;		/* Globale Name Space */
};

enum bnxt_ulp_gen_tun_hdr_type {
	BNXT_ULP_GEN_TUN_IFA,
	BNXT_ULP_GEN_TUN_LAST
};

struct bnxt_ulp_gen_tun_hdr_parms {
	enum bnxt_ulp_gen_tun_hdr_type type;
	struct {
		struct bnxt_ulp_gen_ifa_hdr *ifa_spec;
		struct bnxt_ulp_gen_ifa_hdr *ifa_mask;
	};
};

struct bnxt_ulp_gen_action_parms {
#define BNXT_ULP_GEN_ACTION_ENABLES_KID			0x1UL
#define BNXT_ULP_GEN_ACTION_ENABLES_DROP		0x2UL
#define BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT		0x4UL
#define BNXT_ULP_GEN_ACTION_ENABLES_COUNT		0x8UL
#define BNXT_ULP_GEN_ACTION_ENABLES_SET_SMAC		0x10UL
#define BNXT_ULP_GEN_ACTION_ENABLES_SET_DMAC		0x20UL
/* Allows the driver to provide the vnic directly instead of the default
 * vnic for the dst_fid (used for RSS or QUEUE)
 */
#define BNXT_ULP_GEN_ACTION_ENABLES_VNIC		 0x40UL
#define BNXT_ULP_GEN_ACTION_ENABLES_QUEUE		 0x80UL
#define BNXT_ULP_GEN_ACTION_ENABLES_REDIRECT_LOOPBACK	0x100UL
#define BNXT_ULP_GEN_ACTION_ENABLES_RING_TBL_IDX	0x200UL
	uint64_t enables;
	uint32_t kid;
	u8 smac[ETH_ALEN];
	u8 dmac[ETH_ALEN];
	u16 dst_fid;
	bool drop;
	u16 vnic;
	u16 queue;
	bool ignore_lag; /* true means do not use LAG vport */
	u16 ring_tbl_index;
};

enum bnxt_ulp_gen_direction {
	BNXT_ULP_GEN_RX,
	BNXT_ULP_GEN_TX
};

struct bnxt_ulp_gen_flow_parms {
	struct bnxt_ulp_gen_l2_hdr_parms *l2;
	struct bnxt_ulp_gen_l3_hdr_parms *l3;
	struct bnxt_ulp_gen_l4_hdr_parms *l4;
	struct bnxt_ulp_gen_tun_hdr_parms *tun;
	struct bnxt_ulp_gen_action_parms *actions;
	enum bnxt_ulp_gen_direction dir;
	u8 app_id;
	u16 priority;		/* flow priority */
	u8 lkup_strength;	/* em vs wc tie breaker */
	bool enable_em;

	/* Return to caller */
	u32 *flow_id;
	u64 *counter_hndl;
};

/* ULP flow create interface */
int bnxt_ulp_gen_flow_create(struct bnxt *bp,
			     u16 src_fid,
			     struct bnxt_ulp_gen_flow_parms *flow_parms);

/* ULP flow delete interface */
int bnxt_ulp_gen_flow_destroy(struct bnxt *bp, u16 src_fid, u32 flow_id);

/* ULP flow statistics interface */
void bnxt_ulp_gen_flow_query_count(struct bnxt *bp,
				   u32 flow_id,
				   u64 *packets,
				   u64 *bytes, unsigned long *lastused);

#endif /* #ifndef _ULP_GENERIC_FLOW_OFFLOAD_H_ */
