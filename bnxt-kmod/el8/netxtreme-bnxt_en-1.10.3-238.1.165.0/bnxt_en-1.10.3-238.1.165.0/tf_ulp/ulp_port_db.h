/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_PORT_DB_H_
#define _ULP_PORT_DB_H_

#include "bnxt.h"
#include "bnxt_tf_ulp.h"
#include "bnxt_tf_common.h"

#define BNXT_PORT_DB_MAX_INTF_LIST		256
#define BNXT_PORT_DB_MAX_FUNC			2048
#define BNXT_ULP_FREE_PARIF_BASE		11
#define BNXT_ULP_META_VF_FLAG			0x1000

enum bnxt_ulp_svif_type {
	BNXT_ULP_DRV_FUNC_SVIF = 0,
	BNXT_ULP_VF_FUNC_SVIF,
	BNXT_ULP_PHY_PORT_SVIF
};

enum bnxt_ulp_spif_type {
	BNXT_ULP_DRV_FUNC_SPIF = 0,
	BNXT_ULP_VF_FUNC_SPIF,
	BNXT_ULP_PHY_PORT_SPIF
};

enum bnxt_ulp_parif_type {
	BNXT_ULP_DRV_FUNC_PARIF = 0,
	BNXT_ULP_VF_FUNC_PARIF,
	BNXT_ULP_PHY_PORT_PARIF
};

enum bnxt_ulp_vnic_type {
	BNXT_ULP_DRV_FUNC_VNIC = 0,
	BNXT_ULP_VF_FUNC_VNIC
};

enum bnxt_ulp_fid_type {
	BNXT_ULP_DRV_FUNC_FID,
	BNXT_ULP_VF_FUNC_FID
};

struct ulp_func_if_info {
	u16		func_valid;
	u16		func_svif;
	u16		func_spif;
	u16		func_parif;
	u16		func_vnic;
	u16		func_roce_vnic;
	u8		func_mac[ETH_ALEN];
	u16		func_parent_vnic;
	u8		func_parent_mac[ETH_ALEN];
	u16		phy_port_id;
	u16		ifindex;
	u16		vf_meta_data;
	u8		table_scope;
	u16		mirror_vnic;
};

/* Structure for the Port database resource information. */
struct ulp_interface_info {
	enum bnxt_ulp_intf_type	type;
	u16		drv_func_id;
	u16		vf_func_id;
	u16		type_is_pf;
	u16		rdma_sriov_en;
	u8		udcc_en;
	u16		vf_id;
	u8              hw_lag_en;
};

struct ulp_phy_port_info {
	u16	port_valid;
	u16	port_svif;
	u16	port_spif;
	u16	port_parif;
	u16	port_vport;
	u16	port_lag_vport;
	u32	port_mirror_id_ingress;
	u32	port_mirror_id_egress;
};

/* Structure for the Port database */
struct bnxt_ulp_port_db {
	struct ulp_interface_info	*ulp_intf_list;
	u32			ulp_intf_list_size;

	/* uplink port list */
#define	TC_MAX_ETHPORTS	1024
	u16			dev_port_list[TC_MAX_ETHPORTS];
	struct ulp_phy_port_info	*phy_port_list;
	u16			phy_port_cnt;
	struct ulp_func_if_info		ulp_func_id_tbl[BNXT_PORT_DB_MAX_FUNC];
};

int	ulp_port_db_init(struct bnxt_ulp_context *ulp_ctxt, u8 port_cnt);

int	ulp_port_db_deinit(struct bnxt_ulp_context *ulp_ctxt);

int	ulp_port_db_dev_port_intf_update(struct bnxt_ulp_context *ulp_ctxt,
					 struct bnxt *bp, void *vf_rep);
int
ulp_port_db_dev_port_to_ulp_index(struct bnxt_ulp_context *ulp_ctxt,
				  u32 port_id, u32 *ifindex);

int
ulp_port_db_function_id_get(struct bnxt_ulp_context *ulp_ctxt,
			    u32 ifindex, u32 fid_type,
			    u16 *func_id);
int
ulp_port_db_vf_roce_get(struct bnxt_ulp_context *ulp_ctxt,
			u32 port_id,
			u16 *vf_roce);

int
ulp_port_db_hw_lag_get(struct bnxt_ulp_context *ulp_ctxt,
		       u32 port_id,
		       u16 *hw_lag_en);
int
ulp_port_db_udcc_get(struct bnxt_ulp_context *ulp_ctxt,
		     u32 port_id,
		     u8 *udcc);

int
ulp_port_db_svif_get(struct bnxt_ulp_context *ulp_ctxt,
		     u32 ifindex, u32 dir, u16 *svif);

int
ulp_port_db_spif_get(struct bnxt_ulp_context *ulp_ctxt,
		     u32 ifindex, u32 dir, u16 *spif);

int
ulp_port_db_parif_get(struct bnxt_ulp_context *ulp_ctxt,
		      u32 ifindex, u32 dir, u16 *parif);

int
ulp_port_db_default_vnic_get(struct bnxt_ulp_context *ulp_ctxt,
			     u32 ifindex, u32 vnic_type,
			     u16 *vnic);

int
ulp_port_db_vport_get(struct bnxt_ulp_context *ulp_ctxt,
		      u32 ifindex,	u16 *vport);

int
ulp_port_db_lag_vport_get(struct bnxt_ulp_context *ulp_ctxt,
			  u32 ifindex,	u16 *vport);

int
ulp_port_db_phy_port_vport_get(struct bnxt_ulp_context *ulp_ctxt,
			       u32 phy_port,
			       u16 *out_port);

int
ulp_port_db_phy_port_svif_get(struct bnxt_ulp_context *ulp_ctxt,
			      u32 phy_port,
			      u16 *svif);

enum bnxt_ulp_intf_type
ulp_port_db_port_type_get(struct bnxt_ulp_context *ulp_ctxt,
			  u32 ifindex);

int
ulp_port_db_dev_func_id_to_ulp_index(struct bnxt_ulp_context *ulp_ctxt,
				     u32 func_id, u32 *ifindex);

int
ulp_port_db_port_func_id_get(struct bnxt_ulp_context *ulp_ctxt,
			     u16 port_id, u16 *func_id);

int
ulp_port_db_parent_mac_addr_get(struct bnxt_ulp_context *ulp_ctxt,
				u32 port_id, u8 **mac_addr);

int
ulp_port_db_drv_mac_addr_get(struct bnxt_ulp_context *ulp_ctxt,
			     u32 port_id, u8 **mac_addr);

int
ulp_port_db_parent_vnic_get(struct bnxt_ulp_context *ulp_ctxt,
			    u32 port_id, u8 **vnic);
int
ulp_port_db_phy_port_get(struct bnxt_ulp_context *ulp_ctxt,
			 u32 port_id, u16 *phy_port);

int
ulp_port_db_port_is_pf_get(struct bnxt_ulp_context *ulp_ctxt,
			   u32 port_id, u8 **type);

int
ulp_port_db_port_meta_data_get(struct bnxt_ulp_context *ulp_ctxt,
			       u16 port_id, u8 **meta_data);

int
ulp_port_db_port_vf_fid_get(struct bnxt_ulp_context *ulp_ctxt,
			    u16 port_id, u8 **fid_data);

int
ulp_port_db_port_table_scope_get(struct bnxt_ulp_context *ulp_ctxt,
				 u16 port_id, u8 **tsid);

u16 bnxt_vfr_get_fw_func_id(void *vf_rep);

int
ulp_port_db_drv_roce_vnic_get(struct bnxt_ulp_context *ulp_ctxt,
			      u32 port_id, u8 **roce_vnic);

int
ulp_port_db_port_table_mirror_get(struct bnxt_ulp_context *ulp_ctxt, enum tf_dir dir,
				  u16 port_id, u8 **mirror_id);

int
ulp_port_db_port_table_mirror_set(struct bnxt_ulp_context *ulp_ctxt, enum tf_dir dir,
				  u16 port_id, u32 mirror_id);

int
ulp_port_db_port_socket_direct_svif_get(struct bnxt_ulp_context *ulp_ctxt,
					uint32_t port_id,
					uint16_t *svif);

int
ulp_port_db_vf_id_get(struct bnxt_ulp_context *ulp_ctxt,
		      u32 port_id, u16 *vf_id);

int
ulp_port_db_port_table_mirror_vnic_get(struct bnxt_ulp_context *ulp_ctxt,
				       u16 port_id, u8 **mirror_vnic);

int
ulp_port_db_mirror_vnic_enabled(struct bnxt_ulp_context *ulp_ctxt,
				u16 port_id);

#endif /* _ULP_PORT_DB_H_ */
