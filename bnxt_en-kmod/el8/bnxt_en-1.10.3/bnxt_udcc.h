/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2023-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BNXT_UDCC_H
#define BNXT_UDCC_H

#define BNXT_UDCC_MAX_SESSIONS		4096

#define BNXT_UDCC_HASH_SIZE		64

#define BNXT_UDCC_SESSION_CREATE	0
#define BNXT_UDCC_SESSION_DELETE	1
#define BNXT_UDCC_SESSION_UPDATE	2
#define BNXT_UDCC_SESSION_PER_QP(bp)	((bp)->udcc_info && (bp)->udcc_info->session_type & \
				       UDCC_QCAPS_RESP_SESSION_TYPE_PER_QP)
#define BNXT_UDCC_DCQCN_EN(bp)		((bp)->udcc_info && (bp)->udcc_info->flags & \
				       UDCC_QCAPS_RESP_FLAGS_DCQCN_EN)
#define BNXT_UDCC_HYBRID_MODE(bp)	((bp)->udcc_info && (bp)->udcc_info->hybrid_mode)

struct bnxt_udcc_session_entry {
	u32			session_id;
	u32			rx_flow_id;
	u32			tx_flow_id;
	u64			rx_counter_hndl;
	u64			tx_counter_hndl;
	u8			dest_mac[ETH_ALEN];
	u8			src_mac[ETH_ALEN];
	u8			dst_mac_mod[ETH_ALEN];
	u8			src_mac_mod[ETH_ALEN];
	struct in6_addr		dst_ip;
	struct in6_addr		src_ip;
	u32			src_qp_num;
	u32			dest_qp_num;
	struct dentry		*debugfs_dir;
	struct bnxt		*bp;
	u8			state;
	bool			v4_dst;
	bool			skip_subnet_checking;
};

struct bnxt_udcc_work {
	struct work_struct	work;
	struct bnxt		*bp;
	u32			session_id;
	u8			session_opcode;
	bool			session_suspend;
};

struct bnxt_udcc_info {
	u32				max_sessions;
	struct bnxt_udcc_session_entry	*session_db[BNXT_UDCC_MAX_SESSIONS];
	struct mutex			session_db_lock; /* protect session_db */
	u32				session_count;
	u8				session_type;
	u16				flags;
	u16				max_comp_cfg_xfer;
	u16				max_comp_data_xfer;
	unsigned long			tf_events;
#define BNXT_UDCC_INFO_TF_EVENT_SUSPEND BIT(0)
#define BNXT_UDCC_INFO_TF_EVENT_UNSUSPEND BIT(1)
	/* mode is 0 if udcc is disabled */
	u8				mode;
	/* 0 udcc only, 1 udcc with dcqcn*/
	u8				hybrid_mode;
	/* probe packet pad count=1-3, disabled=0 */
	u8				pad_cnt;
	struct workqueue_struct		*bnxt_udcc_wq;
};

static inline u8 bnxt_udcc_get_mode(struct bnxt *bp)
{
	return bp->udcc_info ? bp->udcc_info->mode : 0;
}

int bnxt_alloc_udcc_info(struct bnxt *bp);
void bnxt_free_udcc_info(struct bnxt *bp);
void bnxt_udcc_session_db_cleanup(struct bnxt *bp);
void bnxt_udcc_task(struct work_struct *work);
int bnxt_hwrm_udcc_session_query(struct bnxt *bp, u32 session_id,
				 struct hwrm_udcc_session_query_output *resp_out);
int bnxt_queue_udcc_work(struct bnxt *bp, u32 session_id, u32 session_opcode,
			 bool suspend);
void bnxt_udcc_update_session(struct bnxt *bp, bool suspend);
void bnxt_udcc_session_debugfs_add(struct bnxt *bp);
void bnxt_udcc_session_debugfs_cleanup(struct bnxt *bp);
int bnxt_start_udcc_worker(struct bnxt *bp);
void bnxt_stop_udcc_worker(struct bnxt *bp);
int bnxt_hwrm_udcc_cfg(struct bnxt *bp, u32 enables, u8 mode, u8 padcnt);
int bnxt_hwrm_udcc_qcfg(struct bnxt *bp);
#endif
