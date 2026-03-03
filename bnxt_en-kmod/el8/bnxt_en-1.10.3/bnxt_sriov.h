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

#ifndef BNXT_SRIOV_H
#define BNXT_SRIOV_H

#define BNXT_FWD_RESP_SIZE_ERR(n)					\
	((offsetof(struct hwrm_fwd_resp_input, encap_resp) + n) >	\
	 sizeof(struct hwrm_fwd_resp_input))

#define BNXT_EXEC_FWD_RESP_SIZE_ERR(n)					\
	((offsetof(struct hwrm_exec_fwd_resp_input, encap_request) + n) >\
	 offsetof(struct hwrm_exec_fwd_resp_input, encap_resp_target_id))

#define BNXT_VF_MIN_RSS_CTX	1
#define BNXT_VF_MAX_RSS_CTX	1
#define BNXT_VF_MIN_L2_CTX	1
#define BNXT_VF_MAX_L2_CTX	4

#ifdef CONFIG_BNXT_SRIOV
#define BNXT_SUPPORTS_SRIOV(pdev)	((pdev)->sriov)
#else
#define BNXT_SUPPORTS_SRIOV(pdev)	0
#endif

#ifdef HAVE_NDO_GET_VF_CONFIG
int bnxt_get_vf_config(struct net_device *, int, struct ifla_vf_info *);
int bnxt_set_vf_mac(struct net_device *, int, u8 *);
#ifdef NEW_NDO_SET_VF_VLAN
int bnxt_set_vf_vlan(struct net_device *, int, u16, u8, __be16);
#else
int bnxt_set_vf_vlan(struct net_device *, int, u16, u8);
#endif
#ifdef HAVE_IFLA_TX_RATE
int bnxt_set_vf_bw(struct net_device *, int, int, int);
#else
int bnxt_set_vf_bw(struct net_device *, int, int);
#endif
#ifdef HAVE_NDO_SET_VF_LINK_STATE
int bnxt_set_vf_link_state(struct net_device *, int, int);
#endif
#ifdef HAVE_VF_SPOOFCHK
int bnxt_set_vf_spoofchk(struct net_device *, int, bool);
#endif
#ifdef HAVE_NDO_SET_VF_TRUST
int bnxt_set_vf_trust(struct net_device *dev, int vf_id, bool trust);
#endif
#endif
int bnxt_sriov_configure(struct pci_dev *pdev, int num_vfs);
#ifndef PCIE_SRIOV_CONFIGURE
void bnxt_start_sriov(struct bnxt *, int);
void bnxt_sriov_init(unsigned int);
void bnxt_sriov_exit(void);
#endif
int bnxt_cfg_hw_sriov(struct bnxt *bp, int *num_vfs, bool reset);
void __bnxt_sriov_disable(struct bnxt *bp);
void bnxt_hwrm_exec_fwd_req(struct bnxt *bp);
void bnxt_update_vf_mac(struct bnxt *bp);
int bnxt_approve_mac(struct bnxt *bp, const u8 *mac, bool strict);
void bnxt_update_vf_vnic(struct bnxt *bp, u32 vf_idx, u32 state);
void bnxt_commit_vf_vnic(struct bnxt *bp, u32 vf_idx);
bool bnxt_vf_vnic_state_is_up(struct bnxt *bp, u32 vf_idx);
bool bnxt_vf_cfg_change(struct bnxt *bp, u16 vf_id, u32 data1);
void bnxt_update_vf_cfg(struct bnxt *bp);
bool bnxt_is_trusted_vf(struct bnxt *bp, struct bnxt_vf_info *vf);
int bnxt_alloc_vf_stats_mem(struct bnxt *bp);
void bnxt_free_vf_stats_mem(struct bnxt *bp);
void bnxt_reset_vf_stats(struct bnxt *bp);
void bnxt_vf_stat_task(struct work_struct *work);
void bnxt_del_vf_stat_ctxs(struct bnxt *bp);
int bnxt_hwrm_tf_oem_cmd(struct bnxt *bp, u32 *in, u16 in_len, u32 *out, u16 out_len);
#endif
