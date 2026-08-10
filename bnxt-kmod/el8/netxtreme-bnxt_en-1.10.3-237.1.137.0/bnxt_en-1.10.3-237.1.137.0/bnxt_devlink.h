/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2017-2018 Broadcom Limited
 * Copyright (c) 2018-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BNXT_DEVLINK_H
#define BNXT_DEVLINK_H

#if defined(HAVE_DEVLINK_PARAM)
#include <net/devlink.h>
#endif

#if defined(CONFIG_VF_REPS) || defined(HAVE_DEVLINK_PARAM)
/* Struct to hold housekeeping info needed by devlink interface */
struct bnxt_dl {
	struct bnxt *bp;	/* back ptr to the controlling dev */
	bool remote_reset;
};

static inline struct bnxt *bnxt_get_bp_from_dl(struct devlink *dl)
{
	return ((struct bnxt_dl *)devlink_priv(dl))->bp;
}

static inline bool bnxt_dl_get_remote_reset(struct devlink *dl)
{
	return ((struct bnxt_dl *)devlink_priv(dl))->remote_reset;
}

static inline void bnxt_dl_set_remote_reset(struct devlink *dl, bool value)
{
	((struct bnxt_dl *)devlink_priv(dl))->remote_reset = value;
}

#endif /* CONFIG_VF_REPS || HAVE_DEVLINK_PARAM */

union bnxt_nvm_data {
	u8	val8;
	__le32	val32;
};

#define NVM_OFF_MSIX_VEC_PER_PF_MAX	108
#define NVM_OFF_MSIX_VEC_PER_PF_MIN	114
#define NVM_OFF_IGNORE_ARI		164
#define NVM_OFF_RDMA_CAPABLE		161
#define NVM_OFF_DIS_GRE_VER_CHECK	171
#define NVM_OFF_ENABLE_SRIOV		401
#define NVM_OFF_MSIX_VEC_PER_VF		406
#define NVM_OFF_SUPPORT_RDMA		506
#define NVM_OFF_NVM_CFG_VER		602
#define NVM_OFF_AN_PROTOCOL		312
#define NVM_OFF_MEDIA_AUTO_DETECT	213

#define BNXT_NVM_CFG_VER_BITS		8
#define BNXT_NVM_CFG_VER_BYTES		1

#define BNXT_MSIX_VEC_MAX	512
#define BNXT_MSIX_VEC_MIN_MAX	128
#define BNXT_AN_PROTOCOL_MAX	4

#if defined(CONFIG_VF_REPS) || defined(HAVE_DEVLINK_PARAM)
#ifdef HAVE_DEVLINK_PARAM
enum bnxt_nvm_dir_type {
	BNXT_NVM_SHARED_CFG = 40,
	BNXT_NVM_PORT_CFG,
	BNXT_NVM_FUNC_CFG,
};

struct bnxt_dl_nvm_param {
	u16 id;
	u16 offset;
	u16 dir_type;
	u16 nvm_num_bits;
	u8 dl_num_bytes;
};

enum bnxt_dl_version_type {
	BNXT_VERSION_FIXED,
	BNXT_VERSION_RUNNING,
	BNXT_VERSION_STORED,
};
#else
static inline int bnxt_dl_params_register(struct bnxt *bp)
{
	return 0;
}
#endif /* HAVE_DEVLINK_PARAM */

int bnxt_dl_register(struct bnxt *bp);
void bnxt_dl_unregister(struct bnxt *bp);

#else /* CONFIG_VF_REPS || HAVE_DEVLINK_PARAM */

static inline int bnxt_dl_register(struct bnxt *bp)
{
	return 0;
}

static inline void bnxt_dl_unregister(struct bnxt *bp)
{
}

#endif /* CONFIG_VF_REPS || HAVE_DEVLINK_PARAM */

void bnxt_devlink_health_fw_report(struct bnxt *bp);
void bnxt_dl_health_fw_status_update(struct bnxt *bp, bool healthy);
void bnxt_dl_health_fw_recovery_done(struct bnxt *bp);
#ifdef HAVE_DEVLINK_HEALTH_REPORT
void bnxt_dl_fw_reporters_create(struct bnxt *bp);
void bnxt_dl_fw_reporters_destroy(struct bnxt *bp);
#else
static inline void bnxt_dl_fw_reporters_create(struct bnxt *bp)
{
}

static inline void bnxt_dl_fw_reporters_destroy(struct bnxt *bp)
{
}
#endif /* HAVE_DEVLINK_HEALTH_REPORT */
static inline void bnxt_dl_remote_reload(struct bnxt *bp)
{
#ifdef HAVE_DEVLINK_RELOAD_ACTION
	devlink_remote_reload_actions_performed(bp->dl, 0,
						BIT(DEVLINK_RELOAD_ACTION_DRIVER_REINIT) |
						BIT(DEVLINK_RELOAD_ACTION_FW_ACTIVATE));
#endif
}

int bnxt_hwrm_nvm_get_var(struct bnxt *bp, dma_addr_t data_dma_addr,
			  u16 offset, u16 dim, u16 index, u16 num_bits);
char *bnxt_health_severity_str(enum bnxt_health_severity severity);
char *bnxt_health_remedy_str(enum bnxt_health_remedy remedy);
#endif /* BNXT_DEVLINK_H */
