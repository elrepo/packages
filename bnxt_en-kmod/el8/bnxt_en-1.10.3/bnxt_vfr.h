/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2016-2018 Broadcom Limited
 * Copyright (c) 2018-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BNXT_VFR_H
#define BNXT_VFR_H

#include <linux/debugfs.h>

#ifdef CONFIG_VF_REPS

#define	MAX_CFA_CODE			65536

int bnxt_hwrm_release_afm_func(struct bnxt *bp, u16 fid, u16 rfid,
			       u8 type, u32 flags);
int bnxt_vf_reps_create(struct bnxt *bp);
void bnxt_vf_reps_destroy(struct bnxt *bp);
void bnxt_vf_reps_close(struct bnxt *bp);
void bnxt_vf_reps_open(struct bnxt *bp);
void bnxt_vf_rep_rx(struct bnxt *bp, struct sk_buff *skb);
struct net_device *bnxt_get_vf_rep(struct bnxt *bp, u16 cfa_code);
struct net_device *bnxt_tf_get_vf_rep(struct bnxt *bp,
				      struct rx_cmp_ext *rxcmp1,
				      struct bnxt_tpa_info *tpa_info);
int bnxt_vf_reps_alloc(struct bnxt *bp);
void bnxt_vf_reps_free(struct bnxt *bp);
int bnxt_hwrm_cfa_pair_alloc(struct bnxt *bp, void *vfr);
int bnxt_hwrm_cfa_pair_free(struct bnxt *bp, void *vfr);
int bnxt_hwrm_cfa_pair_exists(struct bnxt *bp, void *vfr);
int bnxt_tf_config_promisc_mirror(struct bnxt *bp, struct bnxt_vnic_info *vnic);
void bnxt_tf_force_mirror_en_cfg(struct bnxt *bp, bool enable);
void bnxt_tf_force_mirror_en_get(struct bnxt *bp, bool *tf_en,
				 bool *force_mirror_en);
bool bnxt_tf_can_enable_vf_trust(struct bnxt *bp);

static inline u16 bnxt_vf_rep_get_fid(struct net_device *dev)
{
	struct bnxt_vf_rep *vf_rep = netdev_priv(dev);
	struct bnxt *bp = vf_rep->bp;

	return bnxt_vf_target_id(&bp->pf, vf_rep->vf_idx);
}

static inline bool bnxt_tc_is_switchdev_mode(struct bnxt *bp)
{
	return bp->eswitch_mode == DEVLINK_ESWITCH_MODE_SWITCHDEV;
}

bool bnxt_dev_is_vf_rep(struct net_device *dev);
int bnxt_dl_eswitch_mode_get(struct devlink *devlink, u16 *mode);
#ifdef HAVE_ESWITCH_MODE_SET_EXTACK
int bnxt_dl_eswitch_mode_set(struct devlink *devlink, u16 mode,
			     struct netlink_ext_ack *extack);
#else
int bnxt_dl_eswitch_mode_set(struct devlink *devlink, u16 mode);
#endif

int bnxt_hwrm_get_dflt_vnic_svif(struct bnxt *bp, u16 fid,
				 u16 *vnic_id, u16 *svif);
int bnxt_tf_port_init(struct bnxt *bp, u16 flag);
int bnxt_tfo_init(struct bnxt *bp);
void bnxt_tfo_deinit(struct bnxt *bp);
void bnxt_tf_tbl_scope_cleanup(struct bnxt *bp);
void bnxt_tf_port_deinit(struct bnxt *bp, u16 flag);
void bnxt_custom_tf_port_init(struct bnxt *bp);
void bnxt_custom_tf_port_deinit(struct bnxt *bp);
int bnxt_devlink_tf_port_init(struct bnxt *bp);
void bnxt_devlink_tf_port_deinit(struct bnxt *bp);
void bnxt_tf_devlink_toggle(struct bnxt *bp);
#ifdef CONFIG_DEBUG_FS
void bnxt_tf_debugfs_create_files(struct bnxt *bp, u8 tsid, struct dentry *port_dir);
#endif /* CONFIG_DEBUG_FS */

#elif defined CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD
static inline void bnxt_vf_reps_destroy(struct bnxt *bp)
{
}

static inline void bnxt_vf_reps_close(struct bnxt *bp)
{
}

static inline void bnxt_vf_reps_open(struct bnxt *bp)
{
}

static inline void bnxt_vf_rep_rx(struct bnxt *bp, struct sk_buff *skb)
{
}

static inline struct net_device *bnxt_get_vf_rep(struct bnxt *bp, u16 cfa_code)
{
	return NULL;
}

static inline struct net_device *bnxt_tf_get_vf_rep(struct bnxt *bp,
						    struct rx_cmp_ext *rxcmp1,
						    struct bnxt_tpa_info
							*tpa_info)
{
	return NULL;
}

static inline u16 bnxt_vf_rep_get_fid(struct net_device *dev)
{
	return 0;
}

static inline bool bnxt_dev_is_vf_rep(struct net_device *dev)
{
	return false;
}

static inline int bnxt_vf_reps_alloc(struct bnxt *bp)
{
	return -EINVAL;
}

static inline void bnxt_vf_reps_free(struct bnxt *bp)
{
}

static inline bool bnxt_tc_is_switchdev_mode(struct bnxt *bp)
{
	return false;
}

static inline int bnxt_hwrm_cfa_pair_alloc(struct bnxt *bp, void *vfr)
{
	return -EINVAL;
}

static inline int bnxt_hwrm_cfa_pair_free(struct bnxt *bp, void *vfr)
{
	return -EINVAL;
}

static inline int bnxt_hwrm_cfa_pair_exists(struct bnxt *bp, void *vfr)
{
	return -EINVAL;
}

int bnxt_tf_port_init(struct bnxt *bp, u16 flag);
int bnxt_tf_port_init_p7(struct bnxt *bp);
void bnxt_tf_port_deinit(struct bnxt *bp, u16 flag);
int bnxt_tfo_init(struct bnxt *bp);
void bnxt_tfo_deinit(struct bnxt *bp);
void bnxt_tf_tbl_scope_cleanup(struct bnxt *bp);
void bnxt_custom_tf_port_init(struct bnxt *bp);
void bnxt_custom_tf_port_deinit(struct bnxt *bp);
int bnxt_hwrm_get_dflt_vnic_svif(struct bnxt *bp, u16 fid, u16 *vnic_id, u16 *svif);
int bnxt_tf_config_promisc_mirror(struct bnxt *bp, struct bnxt_vnic_info *vnic);
void bnxt_tf_force_mirror_en_cfg(struct bnxt *bp, bool enable);
void bnxt_force_mirror_en_get(struct bnxt *bp, bool *tf_en, bool *force_mirror_en);
bool bnxt_tf_can_enable_vf_trust(struct bnxt *bp);
#ifdef CONFIG_DEBUG_FS
void bnxt_tf_debugfs_create_files(struct bnxt *bp, u8 tsid, struct dentry *port_dir);
#endif /* CONFIG_DEBUG_FS */

#else
bool bnxt_tf_can_enable_vf_trust(struct bnxt *bp)
{
	return 0;
}

static inline int bnxt_vf_reps_create(struct bnxt *bp)
{
	return 0;
}

static inline void bnxt_vf_reps_destroy(struct bnxt *bp)
{
}

static inline void bnxt_vf_reps_close(struct bnxt *bp)
{
}

static inline void bnxt_vf_reps_open(struct bnxt *bp)
{
}

static inline void bnxt_vf_rep_rx(struct bnxt *bp, struct sk_buff *skb)
{
}

static inline struct net_device *bnxt_get_vf_rep(struct bnxt *bp, u16 cfa_code)
{
	return NULL;
}

static inline struct net_device *bnxt_tf_get_vf_rep(struct bnxt *bp,
						    struct rx_cmp_ext *rxcmp1,
						    struct bnxt_tpa_info
							*tpa_info)
{
	return NULL;
}

static inline u16 bnxt_vf_rep_get_fid(struct net_device *dev)
{
	return 0;
}

static inline bool bnxt_dev_is_vf_rep(struct net_device *dev)
{
	return false;
}

static inline int bnxt_vf_reps_alloc(struct bnxt *bp)
{
	return -EINVAL;
}

static inline void bnxt_vf_reps_free(struct bnxt *bp)
{
}

static inline bool bnxt_tc_is_switchdev_mode(struct bnxt *bp)
{
	return false;
}

static inline int bnxt_hwrm_cfa_pair_alloc(struct bnxt *bp, void *vfr)
{
	return -EINVAL;
}

static inline int bnxt_hwrm_cfa_pair_free(struct bnxt *bp, void *vfr)
{
	return -EINVAL;
}

static inline int bnxt_hwrm_cfa_pair_exists(struct bnxt *bp, void *vfr)
{
	return -EINVAL;
}

static inline int bnxt_tf_port_init(struct bnxt *bp)
{
	return 0;
}

static inline int bnxt_tf_port_init_p7(struct bnxt *bp)
{
	return 0;
}

static inline int bnxt_tfo_init(struct bnxt *bp)
{
	return 0;
}

static inline void bnxt_tfo_deinit(struct bnxt *bp)
{
}

static inline void bnxt_tf_tbl_scope_cleanup(struct bnxt *bp)
{
}

static inline void bnxt_tf_port_deinit(struct bnxt *bp)
{
}

static inline void bnxt_custom_tf_port_init(struct bnxt *bp)
{
}

static inline void bnxt_custom_tf_port_deinit(struct bnxt *bp)
{
}

static inline int bnxt_tf_config_promisc_mirror(struct bnxt *bp, struct bnxt_vnic_info *vnic);
{
	return 0;
}

static inline int bnxt_tf_config_promisc_mirror(struct bnxt *bp, struct bnxt_vnic_info *vnic);
{
	return 0;
}

static inline void bnxt_tf_force_mirror_en_cfg(struct bnxt *bp, bool enable)
{
}

static inline void bnxt_force_mirror_en_get(struct bnxt *bp,
					    bool *tf_en, bool *force_mirror_en)
{
}

#ifdef CONFIG_DEBUG_FS

void bnxt_tf_debugfs_create_files(struct bnxt *bp, u8 tsid, struct dentry *port_dir)
{
}
#endif /* CONFIG_DEBUG_FS */
#endif /* CONFIG_VF_REPS */
#endif /* BNXT_VFR_H */
