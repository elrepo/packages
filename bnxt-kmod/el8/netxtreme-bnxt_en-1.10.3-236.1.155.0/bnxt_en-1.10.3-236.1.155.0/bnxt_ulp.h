/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2016-2018 Broadcom Limited
 * Copyright (c) 2018-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BNXT_ULP_H
#define BNXT_ULP_H

#define BNXT_ROCE_ULP	0

#define BNXT_MIN_ROCE_CP_RINGS	2
#define BNXT_MIN_ROCE_STAT_CTXS	1

#define BNXT_MAX_ROCE_MSIX_VF		2
#define BNXT_MAX_ROCE_MSIX_NPAR_PF	5
#define BNXT_MAX_ROCE_MSIX		64
#ifdef BNXT_FPGA
#define BNXT_MAX_ROCE_MSIX_PF		9
#endif

#define BNXT_ULP_MAX_LOG_BUFFERS	1024
#define BNXT_ULP_MAX_LIVE_LOG_SIZE	(32 << 20)

enum bnxt_ulp_auxdev_type {
	BNXT_AUXDEV_RDMA = 0,
	BNXT_AUXDEV_FWCTL,
	__BNXT_AUXDEV_MAX,
};

struct hwrm_async_event_cmpl;
struct bnxt;

struct bnxt_msix_entry {
	u32	vector;
	u32	ring_idx;
	u32	db_offset;
};

#define BNXT_ULP_MAX_DUMP_SEGS	8

/**
 * struct bnxt_ulp_dump - bnxt ULP aux device coredump info
 * @segs:	number of coredump segments with info in the seg_tbl
 * @seg_tbl:	coredump segment table
 * @seg_tbl.seg_id:	coredump segment ID
 * @seg_tbl.seg_len:	coredump segment len
 */
struct bnxt_ulp_dump {
	u32	segs;
	struct bnxt_ulp_dump_tbl {
		u32	seg_id;
		u32	seg_len;
	} seg_tbl[BNXT_ULP_MAX_DUMP_SEGS];
};

struct bnxt_ulp_ops {
	/* async_notifier() cannot sleep (in BH context) */
	void (*ulp_async_notifier)(void *, struct hwrm_async_event_cmpl *);
	void (*ulp_irq_stop)(void *, bool);
	void (*ulp_irq_restart)(void *, struct bnxt_msix_entry *);
	void (*ulp_get_dump_info)(void *handle, u32 dump_flags,
				  struct bnxt_ulp_dump *dump);
	void (*ulp_get_dump_data)(void *handle, u32 seg_id, void *buf, u32 len);
};

struct bnxt_fw_msg {
	void	*msg;
	int	msg_len;
	void	*resp;
	int	resp_max_len;
	int	timeout;
};

struct bnxt_ulp {
	void		*handle;
	struct bnxt_ulp_ops __rcu *ulp_ops;
	unsigned long	*async_events_bmap;
	u16		max_async_event_id;
	u16		msix_requested;
	struct bnxt_ulp_dump	ulp_dump;
};

#define BNXT_MAX_BAR_ADDR			8
struct bnxt_peer_bar_addr {
	__le64			hv_bar_addr;
	__le64			vm_bar_addr;
	__le64			bar_size;
};

struct bnxt_en_ulp_stats {
	u64 link_down_events;
	u64 rx_pcs_symbol_err;
	u64 rx_discard_bytes_cos;
};

struct bnxt_en_dev {
	struct net_device *net;
	struct pci_dev *pdev;
	struct bnxt_msix_entry			msix_entries[BNXT_MAX_ROCE_MSIX];
	u32 flags;
	#define BNXT_EN_FLAG_ROCEV1_CAP		0x1
	#define BNXT_EN_FLAG_ROCEV2_CAP		0x2
	#define BNXT_EN_FLAG_ROCE_CAP		(BNXT_EN_FLAG_ROCEV1_CAP | \
						 BNXT_EN_FLAG_ROCEV2_CAP)
	#define BNXT_EN_FLAG_MSIX_REQUESTED	0x4
	#define BNXT_EN_FLAG_ULP_STOPPED	0x8
	#define BNXT_EN_FLAG_ASYM_Q		0x10
	#define BNXT_EN_FLAG_MULTI_HOST		0x20
	#define BNXT_EN_FLAG_VF			0x40
	#define BNXT_EN_FLAG_HW_LAG		0x80
	#define BNXT_EN_FLAG_ROCE_VF_RES_MGMT	0x100
	#define BNXT_EN_FLAG_MULTI_ROOT		0x200
	#define BNXT_EN_FLAG_SW_RES_LMT		0x400
#define BNXT_EN_ASYM_Q(edev)		((edev)->flags & BNXT_EN_FLAG_ASYM_Q)
#define BNXT_EN_MH(edev)		((edev)->flags & BNXT_EN_FLAG_MULTI_HOST)
#define BNXT_EN_VF(edev)		((edev)->flags & BNXT_EN_FLAG_VF)
#define BNXT_EN_HW_LAG(edev)		((edev)->flags & BNXT_EN_FLAG_HW_LAG)
#define BNXT_EN_MR(edev)		((edev)->flags & BNXT_EN_FLAG_MULTI_ROOT)
#define BNXT_EN_SW_RES_LMT(edev)	((edev)->flags & BNXT_EN_FLAG_SW_RES_LMT)
	struct bnxt_ulp			*ulp_tbl;
	int				l2_db_size;	/* Doorbell BAR size in
							 * bytes mapped by L2
							 * driver.
							 */
	int				l2_db_size_nc;	/* Doorbell BAR size in
							 * bytes mapped as non-
							 * cacheable.
							 */
	u32				ulp_version;	/* bnxt_re checks the
							 * ulp_version is correct
							 * to ensure compatibility
							 * with bnxt_en.
							 */
	#define BNXT_ULP_VERSION	0x695a0013	/* Change this when any interface
							 * structure or API changes
							 * between bnxt_en and bnxt_re.
							 */
	unsigned long			en_state;
	void __iomem			*bar0;
	u16				hw_ring_stats_size;
	u16				pf_port_id;
	u8				port_partition_type;
#define BNXT_EN_NPAR(edev)		((edev)->port_partition_type)
	u8				port_count;
	struct bnxt_dbr			*en_dbr;

	struct bnxt_hdbr_info		*hdbr_info;
	u16				chip_num;
	int				l2_db_offset;	/* Doorbell BAR offset
							 * of non-cacheable.
							 */

	u16				ulp_num_msix_vec;
	u16				ulp_num_ctxs;
	struct mutex			en_dev_lock;	/* serialize ulp operations */
	struct bnxt_peer_bar_addr	bar_addr[BNXT_MAX_BAR_ADDR];
	u16				bar_cnt;
	#define BNXT_VPD_PN_FLD_LEN	32
	char				board_part_number[BNXT_VPD_PN_FLD_LEN];
	u16				ds_port;
	u16				auth_status;
	u16				status[BNXT_MAX_BAR_ADDR];
};

static inline bool bnxt_ulp_registered(struct bnxt_en_dev *edev)
{
	if (edev && rcu_access_pointer(edev->ulp_tbl->ulp_ops))
		return true;
	return false;
}

int bnxt_get_ulp_msix_num(struct bnxt *bp);
int bnxt_get_ulp_msix_num_in_use(struct bnxt *bp);
void bnxt_set_ulp_msix_num(struct bnxt *bp, int num);
int bnxt_get_ulp_stat_ctxs(struct bnxt *bp);
int bnxt_get_ulp_stat_ctxs_in_use(struct bnxt *bp);
void bnxt_set_ulp_stat_ctxs(struct bnxt *bp, int num_ctxs);
void bnxt_set_dflt_ulp_stat_ctxs(struct bnxt *bp);
void bnxt_ulp_stop(struct bnxt *bp);
void bnxt_ulp_start(struct bnxt *bp, int err);
void bnxt_ulp_sriov_cfg(struct bnxt *bp, int num_vfs);
#ifndef HAVE_AUXILIARY_DRIVER
void bnxt_ulp_shutdown(struct bnxt *bp);
#endif
void bnxt_ulp_irq_stop(struct bnxt *bp);
void bnxt_ulp_irq_restart(struct bnxt *bp, int err);
u32 bnxt_get_ulp_dump(struct bnxt *bp, u32 dump_flag, void *buf, u32 *segs);
void bnxt_ulp_async_events(struct bnxt *bp, struct hwrm_async_event_cmpl *cmpl);
void bnxt_aux_device_uninit(struct bnxt *bp, enum bnxt_ulp_auxdev_type type);
void bnxt_aux_device_init(struct bnxt *bp, enum bnxt_ulp_auxdev_type type);
void bnxt_aux_device_add(struct bnxt *bp, enum bnxt_ulp_auxdev_type type);
void bnxt_aux_device_del(struct bnxt *bp, enum bnxt_ulp_auxdev_type type);
int bnxt_register_dev(struct bnxt_en_dev *edev,
		      struct bnxt_ulp_ops *ulp_ops, void *handle);
void bnxt_unregister_dev(struct bnxt_en_dev *edev);
int bnxt_send_msg(struct bnxt_en_dev *edev, struct bnxt_fw_msg *fw_msg);
void bnxt_register_async_events(struct bnxt_en_dev *edev,
				unsigned long *events_bmap, u16 max_id);
void bnxt_dbr_complete(struct bnxt_en_dev *edev, u32 epoch);
int bnxt_udcc_subnet_check(struct bnxt_en_dev *edev, void *dest_ip, u8 *dmac, u8 *smac);
int bnxt_rdma_pkt_capture_set(struct bnxt_en_dev *edev, u8 enable);
void bnxt_force_mirror_en_cfg(struct bnxt_en_dev *edev, bool enable);
void bnxt_force_mirror_en_get(struct bnxt_en_dev *edev, bool *tf_en, bool *force_mirror_en);
void bnxt_ulp_get_stats(struct bnxt_en_dev *edev, struct bnxt_en_ulp_stats *stats);
void bnxt_ulp_get_peer_bar_maps(struct bnxt_en_dev *edev);

/* DCQCN flows */
struct bnxt_dcqcn_session_entry;
int bnxt_en_ulp_dcqcn_flow_create(struct bnxt_en_dev *edev,
				  struct bnxt_dcqcn_session_entry *entry);
int bnxt_en_ulp_dcqcn_flow_delete(struct bnxt_en_dev *edev,
				  struct bnxt_dcqcn_session_entry *entry);

#endif
