/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2020-2023 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include "bnxt.h"

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD

#ifdef HAVE_FLOW_OFFLOAD_H
#ifdef HAVE_FLOW_STATS_UPDATE
#if !defined(HAVE_FLOW_STATS_DROPS) && defined(HAVE_FLOW_ACTION_BASIC_HW_STATS_CHECK)
#define flow_stats_update(flow_stats, bytes, pkts, drops, last_used, used_hw_stats) \
	flow_stats_update(flow_stats, bytes, pkts, last_used, used_hw_stats)
#elif !defined(HAVE_FLOW_ACTION_BASIC_HW_STATS_CHECK)
#define flow_stats_update(flow_stats, bytes, pkts, drops, last_used, used_hw_stats) \
	flow_stats_update(flow_stats, bytes, pkts, last_used)
#endif
#endif

#ifndef HAVE_FLOW_ACTION_BASIC_HW_STATS_CHECK
static inline bool
flow_action_basic_hw_stats_check(const struct flow_action *action,
				 struct netlink_ext_ack *extack)
{
	return true;
}
#endif /* HAVE_FLOW_ACTION_BASIC_HW_STATS_CHECK */

#ifndef HAVE_FLOW_INDR_BLOCK_CLEANUP
#ifdef HAVE_FLOW_INDR_BLOCK_CB_QDISC
#define bnxt_tc_setup_indr_block(netdev, sch, bp, f, data, cleanup)	\
	bnxt_tc_setup_indr_block(netdev, bp, f)
#else
#define bnxt_tc_setup_indr_block(netdev, bp, f, data, cleanup)	\
	bnxt_tc_setup_indr_block(netdev, bp, f)
#endif

#ifdef HAVE_FLOW_INDR_BLOCK_CB_QDISC
#define flow_indr_block_cb_alloc(cb, cb_ident, cb_priv, bnxt_tc_setup_indr_rel,	\
				 f, netdev, sch, data, bp, cleanup)			\
	flow_block_cb_alloc(cb, cb_ident, cb_priv, bnxt_tc_setup_indr_rel)
#else
#define flow_indr_block_cb_alloc(cb, cb_ident, cb_priv, bnxt_tc_setup_indr_rel,	\
				 f, netdev, data, bp, cleanup)			\
	flow_block_cb_alloc(cb, cb_ident, cb_priv, bnxt_tc_setup_indr_rel)
#endif

#define flow_indr_block_cb_remove(block_cb, f)	\
	flow_block_cb_remove(block_cb, f)

#ifdef HAVE_FLOW_INDR_BLOCK_CB_QDISC
#define bnxt_tc_setup_indr_cb(netdev, sch, cb_priv, type, type_data, data, cleanup)	\
	bnxt_tc_setup_indr_cb(netdev, cb_priv, type, type_data)
#else
#define bnxt_tc_setup_indr_cb(netdev, cb_priv, type, type_data, data, cleanup)	\
	bnxt_tc_setup_indr_cb(netdev, cb_priv, type, type_data)
#endif
#endif /* HAVE_FLOW_INDR_BLOCK_CLEANUP */
#endif /* HAVE_FLOW_OFFLOAD_H */

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) && defined(HAVE_FLOW_INDR_BLOCK_CB)
#if !defined(HAVE_FLOW_INDR_DEV_RGTR)
int bnxt_tc_indr_block_event(struct notifier_block *nb, unsigned long event,
			     void *ptr);

static inline int flow_indr_dev_register(flow_indr_block_bind_cb_t *cb,
					 void *cb_priv)
{
	struct bnxt *bp = cb_priv;

	bp->tc_netdev_nb.notifier_call = bnxt_tc_indr_block_event;
	return register_netdevice_notifier(&bp->tc_netdev_nb);
}

static inline void flow_indr_dev_unregister(flow_indr_block_bind_cb_t *cb,
					    void *cb_priv,
					    void (*release)(void *cb_priv))
{
	struct bnxt *bp = cb_priv;

	unregister_netdevice_notifier(&bp->tc_netdev_nb);
}
#endif /* !HAVE_FLOW_INDR_DEV_RGTR */

#ifdef HAVE_OLD_FLOW_INDR_DEV_UNRGTR
#define flow_indr_dev_unregister(cb, bp, rel)		\
	flow_indr_dev_unregister(cb, bp, bnxt_tc_setup_indr_block_cb)
#endif /* HAVE_OLD_FLOW_INDR_BLOCK_CB_UNRGTR */
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD && HAVE_FLOW_INDR_BLOCK_CB */

#ifndef HAVE_FLOW_OFFLOAD_H

struct flow_match_basic {
	struct flow_dissector_key_basic *key, *mask;
};

struct flow_match_control {
	struct flow_dissector_key_control *key, *mask;
};

struct flow_match_eth_addrs {
	struct flow_dissector_key_eth_addrs *key, *mask;
};

struct flow_match_vlan {
	struct flow_dissector_key_vlan *key, *mask;
};

struct flow_match_ipv4_addrs {
	struct flow_dissector_key_ipv4_addrs *key, *mask;
};

struct flow_match_ipv6_addrs {
	struct flow_dissector_key_ipv6_addrs *key, *mask;
};

struct flow_match_ip {
	struct flow_dissector_key_ip *key, *mask;
};

struct flow_match_ports {
	struct flow_dissector_key_ports *key, *mask;
};

struct flow_match_icmp {
	struct flow_dissector_key_icmp *key, *mask;
};

struct flow_match_tcp {
	struct flow_dissector_key_tcp *key, *mask;
};

struct flow_match_enc_keyid {
	struct flow_dissector_key_keyid *key, *mask;
};

struct flow_match_mpls {
	struct flow_dissector_key_mpls *key, *mask;
};

struct flow_match {
	struct flow_dissector	*dissector;
	void			*mask;
	void			*key;
};

struct flow_rule {
	struct flow_match	match;
};

#define FLOW_DISSECTOR_MATCH(__rule, __type, __out)			     \
	const struct flow_match *__m = &(__rule)->match;		     \
	struct flow_dissector *__d = (__m)->dissector;			     \
									     \
	(__out)->key = skb_flow_dissector_target(__d, __type, (__m)->key);   \
	(__out)->mask = skb_flow_dissector_target(__d, __type, (__m)->mask)  \

static inline bool flow_rule_match_key(const struct flow_rule *rule,
				       enum flow_dissector_key_id key)
{
	return dissector_uses_key(rule->match.dissector, key);
}

static inline void flow_rule_match_basic(const struct flow_rule *rule,
					 struct flow_match_basic *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_BASIC, out);
}

static inline void flow_rule_match_control(const struct flow_rule *rule,
					   struct flow_match_control *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_CONTROL, out);
}

static inline void flow_rule_match_eth_addrs(const struct flow_rule *rule,
					     struct flow_match_eth_addrs *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_ETH_ADDRS, out);
}

static inline void flow_rule_match_vlan(const struct flow_rule *rule,
					struct flow_match_vlan *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_VLAN, out);
}

static inline void flow_rule_match_ipv4_addrs(const struct flow_rule *rule,
					      struct flow_match_ipv4_addrs *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_IPV4_ADDRS, out);
}

static inline void flow_rule_match_ipv6_addrs(const struct flow_rule *rule,
					      struct flow_match_ipv6_addrs *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_IPV6_ADDRS, out);
}

static inline void flow_rule_match_ip(const struct flow_rule *rule,
				      struct flow_match_ip *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_IP, out);
}

static inline void flow_rule_match_tcp(const struct flow_rule *rule,
				       struct flow_match_tcp *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_TCP, out);
}

static inline void flow_rule_match_ports(const struct flow_rule *rule,
					 struct flow_match_ports *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_PORTS, out);
}

static inline void flow_rule_match_icmp(const struct flow_rule *rule,
					struct flow_match_icmp *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_ICMP, out);
}

static inline void flow_rule_match_enc_control(const struct flow_rule *rule,
					       struct flow_match_control *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_ENC_CONTROL, out);
}

static inline void
flow_rule_match_enc_ipv4_addrs(const struct flow_rule *rule,
			       struct flow_match_ipv4_addrs *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_ENC_IPV4_ADDRS, out);
}

static inline void flow_rule_match_enc_ipv6_addrs(const struct flow_rule *rule,
						  struct flow_match_ipv6_addrs *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_ENC_IPV6_ADDRS, out);
}

static inline void flow_rule_match_enc_ip(const struct flow_rule *rule,
					  struct flow_match_ip *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_ENC_IP, out);
}

static inline void flow_rule_match_enc_ports(const struct flow_rule *rule,
					     struct flow_match_ports *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_ENC_PORTS, out);
}

static inline void flow_rule_match_enc_keyid(const struct flow_rule *rule,
					     struct flow_match_enc_keyid *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_ENC_KEYID, out);
}

static inline void flow_rule_match_mpls(const struct flow_rule *rule,
					struct flow_match_mpls *out)
{
	FLOW_DISSECTOR_MATCH(rule, FLOW_DISSECTOR_KEY_MPLS, out);
}

#ifdef flow_cls_offload_flow_rule
#undef flow_cls_offload_flow_rule
#endif
#define flow_cls_offload_flow_rule(cmd)			\
	(&(struct flow_rule) {				\
		.match = {				\
			.dissector = (cmd)->dissector,	\
			.mask = (cmd)->mask,		\
			.key = (cmd)->key,		\
		}					\
	})

enum flow_action_id {
	FLOW_ACTION_ACCEPT		= 0,
	FLOW_ACTION_DROP,
	FLOW_ACTION_TRAP,
	FLOW_ACTION_GOTO,
	FLOW_ACTION_REDIRECT,
	FLOW_ACTION_MIRRED,
	FLOW_ACTION_REDIRECT_INGRESS,
	FLOW_ACTION_MIRRED_INGRESS,
	FLOW_ACTION_VLAN_PUSH,
	FLOW_ACTION_VLAN_POP,
	FLOW_ACTION_VLAN_MANGLE,
	FLOW_ACTION_TUNNEL_ENCAP,
	FLOW_ACTION_TUNNEL_DECAP,
	FLOW_ACTION_MANGLE,
	FLOW_ACTION_ADD,
	FLOW_ACTION_CSUM,
	FLOW_ACTION_MARK,
	FLOW_ACTION_PTYPE,
	FLOW_ACTION_PRIORITY,
	FLOW_ACTION_WAKE,
	FLOW_ACTION_QUEUE,
	FLOW_ACTION_SAMPLE,
	FLOW_ACTION_POLICE,
	FLOW_ACTION_CT,
	FLOW_ACTION_CT_METADATA,
	FLOW_ACTION_MPLS_PUSH,
	FLOW_ACTION_MPLS_POP,
	FLOW_ACTION_MPLS_MANGLE,
	FLOW_ACTION_GATE,
	FLOW_ACTION_PPPOE_PUSH,
	FLOW_ACTION_INVALID = NUM_FLOW_ACTIONS
};

#endif	/* !HAVE_FLOW_OFFLOAD_H */

#endif	/* CONFIG_BNXT_FLOWER_OFFLOAD */
