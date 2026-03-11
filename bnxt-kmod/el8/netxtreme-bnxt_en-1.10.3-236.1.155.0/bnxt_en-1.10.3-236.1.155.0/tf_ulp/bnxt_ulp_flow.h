/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_ULP_FLOW_H_
#define _BNXT_ULP_FLOW_H_

#define BNXT_ULP_GEN_UDP_PORT_VXLAN		4789

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)

struct bnxt_ulp_flow_info {
	u32					flow_id;
	struct ip_tunnel_key			*encap_key;
	struct bnxt_tc_neigh_key		*neigh_key;
	u8					tnl_smac[ETH_ALEN];
	u8					tnl_dmac[ETH_ALEN];
	u16					tnl_ether_type;
	void					*mparms;
	u32					dscp_remap;
};
#endif

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
int bnxt_ulp_flow_create(struct bnxt *bp, u16 src_fid,
			 struct flow_cls_offload *tc_flow_cmd,
			 struct bnxt_ulp_flow_info *flow_info);
int bnxt_ulp_flow_destroy(struct bnxt *bp, u32 flow_id, u16 src_fid,
			  u32 dscp_remap);
void bnxt_ulp_flow_query_count(struct bnxt *bp, u32 flow_id, u64 *packets,
			       u64 *bytes, unsigned long *lastused);
int
bnxt_ulp_update_flow_encap_record(struct bnxt *bp, u8 *tnl_dmac, void *mparms,
				  u32 *flow_id);
void bnxt_ulp_free_mapper_encap_mparams(void *mparms);
bool bnxt_ulp_flow_chain_validate(struct bnxt *bp, u16 src_fid,
				  struct flow_cls_offload *tc_flow_cmd);

#ifdef CONFIG_VF_REPS
int bnxt_ulp_port_init(struct bnxt *bp);
void bnxt_ulp_port_deinit(struct bnxt *bp);
int bnxt_ulp_tfo_init(struct bnxt *bp);
void bnxt_ulp_tfo_deinit(struct bnxt *bp);
int bnxt_ulp_alloc_vf_rep(struct bnxt *bp, void *vfr);
int bnxt_ulp_alloc_vf_rep_p7(struct bnxt *bp, void *vfr);
void bnxt_ulp_free_vf_rep(struct bnxt *bp, void *vfr);
void bnxt_ulp_free_vf_rep_p7(struct bnxt *bp, void *vfr);
int bnxt_ulp_get_mark_from_cfacode(struct bnxt *bp, struct rx_cmp_ext *rxcmp1,
				   struct bnxt_tpa_info *tpa_info,
				   u32 *mark_id);
int bnxt_ulp_get_mark_from_cfacode_p7(struct bnxt *bp, struct rx_cmp_ext *rxcmp1,
				      struct bnxt_tpa_info *tpa_info,
				      u32 *mark_id);
int bnxt_ulp_set_mirror(struct bnxt *bp, bool stat);
int bnxt_ulp_set_mirror_p7(struct bnxt *bp, bool stat);
int bnxt_tf_ulp_force_mirror_en_cfg_p7(struct bnxt *bp, bool enable);
void bnxt_tf_ulp_force_mirror_en_get_p7(struct bnxt *bp, bool *tf_en,
					bool *force_mirror_en);
bool bnxt_ulp_can_enable_vf_trust(struct bnxt *bp);
#endif /* CONFIG_VF_REPS */
#elif defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
int bnxt_ulp_port_init(struct bnxt *bp);
void bnxt_ulp_port_deinit(struct bnxt *bp);
#else /* CONFIG_BNXT_FLOWER_OFFLOAD */
#ifdef CONFIG_VF_REPS
static inline int
bnxt_ulp_port_init(struct bnxt *bp)

{
	return -EINVAL;
}

static inline void
bnxt_ulp_port_deinit(struct bnxt *bp)
{
}

static inline int
bnxt_ulp_tfo_init(struct bnxt *bp)

{
	return -EINVAL;
}

static inline void
bnxt_ulp_tfo_deinit(struct bnxt *bp)
{
}

static inline int bnxt_ulp_alloc_vf_rep(struct bnxt *bp, void *vfr)
{
	return -EINVAL;
}

static inline int bnxt_ulp_alloc_vf_rep_p7(struct bnxt *bp, void *vfr)
{
	return -EINVAL;
}

static inline void bnxt_ulp_free_vf_rep(struct bnxt *bp, void *vfr)
{
}

static inline void bnxt_ulp_free_vf_rep_p7(struct bnxt *bp, void *vfr)
{
}

static inline int
bnxt_ulp_get_mark_from_cfacode(struct bnxt *bp, struct rx_cmp_ext *rxcmp1,
			       struct bnxt_tpa_info *tpa_info, u32 *mark_id)
{
	return -EINVAL;
}

static inline int
bnxt_ulp_get_mark_from_cfacode_p7(struct bnxt *bp, struct rx_cmp_ext *rxcmp1,
				  struct bnxt_tpa_info *tpa_info, u32 *mark_id)
{
	return -EINVAL;
}

static inline int
bnxt_ulp_set_mirror(struct bnxt *bp, bool stat)
{
	return 0;
}

static inline int
bnxt_ulp_set_mirror_p7(struct bnxt *bp, bool stat)
{
	return 0;
}

static inline bool
bnxt_ulp_can_enable_vf_trust(struct bnxt *bp)
{
	return true;
}

static inline int
bnxt_tf_ulp_force_mirror_en_cfg_p7(struct bnxt *bp, bool enable)
{
	return 0;
}

static inline void
bnxt_tf_ulp_force_mirror_en_get_p7(struct bnxt *bp, bool *tf_en,
				   bool *force_mirror_en)
{
}

#endif /* CONFIG_VF_REPS */
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */

#endif /* _BNXT_ULP_FLOW_H_ */
