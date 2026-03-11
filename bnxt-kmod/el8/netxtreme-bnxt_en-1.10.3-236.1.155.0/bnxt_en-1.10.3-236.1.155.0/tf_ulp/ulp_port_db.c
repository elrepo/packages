// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "ulp_linux.h"
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_vfr.h"
#include "bnxt_udcc.h"
#include "bnxt_tf_common.h"
#include "ulp_port_db.h"
#include "ulp_tf_debug.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
/* Truflow definitions */
void
bnxt_get_parent_mac_addr(struct bnxt *bp, u8 *mac)
{
	ether_addr_copy(mac, bp->dev->dev_addr);
}

u16
bnxt_get_svif(struct bnxt *bp, bool func_svif,
	      enum bnxt_ulp_intf_type type)
{
	return func_svif ? bp->func_svif : bp->port_svif;
}

void
bnxt_get_iface_mac(struct bnxt *bp, enum bnxt_ulp_intf_type type,
		   u8 *mac, u8 *parent_mac)
{
	if (type == BNXT_ULP_INTF_TYPE_PF) {
		ether_addr_copy(mac, bp->dev->dev_addr);
	} else if (type == BNXT_ULP_INTF_TYPE_TRUSTED_VF) {
		ether_addr_copy(mac, bp->vf.mac_addr);
		ether_addr_copy(parent_mac, bp->dev->dev_addr);
	}
	return;
}

u16
bnxt_get_parent_vnic_id(struct bnxt *bp, enum bnxt_ulp_intf_type type)
{
	if (type != BNXT_ULP_INTF_TYPE_TRUSTED_VF)
		return 0;

	return bp->pf.dflt_vnic_id;
}

enum bnxt_ulp_intf_type
bnxt_get_interface_type(struct bnxt *bp)
{
	if (BNXT_PF(bp))
		return BNXT_ULP_INTF_TYPE_PF;
	else if (BNXT_VF_IS_TRUSTED(bp))
		return BNXT_ULP_INTF_TYPE_TRUSTED_VF;
	else if (BNXT_VF(bp))
		return BNXT_ULP_INTF_TYPE_VF;

	return BNXT_ULP_INTF_TYPE_INVALID;
}

u16
bnxt_get_vnic_id(struct bnxt *bp, enum bnxt_ulp_intf_type type)
{
#ifdef CONFIG_VF_REPS
	struct bnxt_vf_rep *vf_rep = netdev_priv(bp->dev);

	if (bnxt_dev_is_vf_rep(bp->dev))
		return vf_rep->bp->vnic_info->fw_vnic_id;
#endif

	return bp->vnic_info[0].fw_vnic_id;
}

u16
bnxt_vfr_get_fw_func_id(void *vf_rep)
{
#ifdef CONFIG_VF_REPS
	struct bnxt_vf_rep *vfr = vf_rep;

	if (bnxt_dev_is_vf_rep(vfr->dev))
		return bnxt_vf_rep_get_fid(vfr->dev);
#endif

	return 0;
}

u16
bnxt_get_fw_func_id(struct bnxt *bp, enum bnxt_ulp_intf_type type)
{
#ifdef CONFIG_VF_REPS
	if (bnxt_dev_is_vf_rep(bp->dev))
		return bnxt_vf_rep_get_fid(bp->dev);
#endif

	return BNXT_PF(bp) ? bp->pf.fw_fid : bp->vf.fw_fid;
}

u16
bnxt_get_phy_port_id(struct bnxt *bp)
{
#ifdef CONFIG_VF_REPS
	struct bnxt_vf_rep *vf_rep = netdev_priv(bp->dev);

	if (bnxt_dev_is_vf_rep(bp->dev))
		return vf_rep->bp->pf.port_id;
#endif

	return bp->pf.port_id;
}

u16
bnxt_get_parif(struct bnxt *bp)
{
#ifdef CONFIG_VF_REPS
	if (bnxt_dev_is_vf_rep(bp->dev))
		return (bnxt_vf_rep_get_fid(bp->dev) - 1);
#endif

	return BNXT_PF(bp) ? bp->pf.fw_fid - 1 : bp->vf.fw_fid - 1;
}

#define BNXT_LAG_VPORT(val)    ((1 << 8) | (val) << 5)
u16
bnxt_get_lag_vport(struct bnxt *bp)
{
	struct bnxt_bond_info *binfo = bp->bond_info;

	if (binfo && binfo->bond_active &&
	    (binfo->fw_lag_id != BNXT_INVALID_LAG_ID) &&
	    (bp->flags & BNXT_FLAG_CHIP_P7_PLUS)) {
		netdev_info(bp->dev, "Truflow hardware LAG id 0x%x\n",
			    binfo->fw_lag_id);
		return BNXT_LAG_VPORT(binfo->fw_lag_id);
	}
	return (1 << bnxt_get_phy_port_id(bp));
}

u8
bnxt_is_hw_lag_enabled(struct bnxt *bp)
{
	struct bnxt_bond_info *binfo = bp->bond_info;

	if (binfo && binfo->bond_active &&
	    (binfo->fw_lag_id != BNXT_INVALID_LAG_ID) &&
	    (bp->flags & BNXT_FLAG_CHIP_P5_PLUS)) {
		return 1;
	}
	return 0;
}

u16
bnxt_get_vport(struct bnxt *bp)
{
	return (1 << bnxt_get_phy_port_id(bp));
}

/**
 * Initialize the port database. Memory is allocated in this
 * call and assigned to the port database.
 *
 * @ulp_ctxt: Ptr to ulp context
 *
 * Returns 0 on success or negative number on failure.
 */
int	ulp_port_db_init(struct bnxt_ulp_context *ulp_ctxt, u8 port_cnt)
{
	struct bnxt_ulp_port_db *port_db;
	int rc;

	port_db = vzalloc(sizeof(*port_db));
	if (!port_db)
		return -ENOMEM;

	/* Attach the port database to the ulp context. */
	rc = bnxt_ulp_cntxt_ptr2_port_db_set(ulp_ctxt, port_db);
	if (rc) {
		vfree(port_db);
		return rc;
	}

	/* 256 VFs + PFs etc. so making it 512*/
	port_db->ulp_intf_list_size = BNXT_PORT_DB_MAX_INTF_LIST * 2;
	/* Allocate the port tables */
	port_db->ulp_intf_list = vzalloc(port_db->ulp_intf_list_size *
					 sizeof(struct ulp_interface_info));
	if (!port_db->ulp_intf_list)
		goto error_free;

	/* Allocate the phy port list */
	port_db->phy_port_list = vzalloc(port_cnt * sizeof(struct ulp_phy_port_info));
	if (!port_db->phy_port_list)
		goto error_free;

	port_db->phy_port_cnt = port_cnt;
	return 0;

error_free:
	ulp_port_db_deinit(ulp_ctxt);
	return -ENOMEM;
}

/**
 * Deinitialize the port database. Memory is deallocated in
 * this call.
 *
 * @ulp_ctxt: Ptr to ulp context
 *
 * Returns 0 on success.
 */
int	ulp_port_db_deinit(struct bnxt_ulp_context *ulp_ctxt)
{
	struct bnxt_ulp_port_db *port_db;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	/* Detach the flow database from the ulp context. */
	bnxt_ulp_cntxt_ptr2_port_db_set(ulp_ctxt, NULL);

	/* Free up all the memory. */
	vfree(port_db->phy_port_list);
	vfree(port_db->ulp_intf_list);
	vfree(port_db);
	return 0;
}

/**
 * Update the port database.This api is called when the port
 * details are available during the startup.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @bp:. ptr to the device function.
 *
 * Returns 0 on success or negative number on failure.
 */
#if defined(CONFIG_VF_REPS) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)

static u32
ulp_port_db_allocate_ifindex(struct bnxt_ulp_context *ulp_ctx,
			     struct bnxt_ulp_port_db *port_db)
{
	u32 idx = 1;

	while (idx < port_db->ulp_intf_list_size &&
	       port_db->ulp_intf_list[idx].type != BNXT_ULP_INTF_TYPE_INVALID)
		idx++;

	if (idx >= port_db->ulp_intf_list_size) {
		netdev_dbg(ulp_ctx->bp->dev, "Port DB interface list is full\n");
		return 0;
	}
	return idx;
}

int ulp_port_db_dev_port_intf_update(struct bnxt_ulp_context *ulp_ctxt,
				     struct bnxt *bp, void *vf_rep)
{
	struct ulp_phy_port_info *port_data;
	struct bnxt_ulp_port_db *port_db;
	struct bnxt_vf_rep *vfr = vf_rep;
	struct ulp_interface_info *intf;
	struct ulp_func_if_info *func;
	u32 ifindex;
	u32 port_id;
	uint8_t tsid;
	int rc;

#if defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
	port_id = bp->pf.fw_fid;
#else
	if (!vfr)
		port_id = bp->pf.fw_fid;
	else
		port_id = bp->pf.vf[vfr->vf_idx].fw_fid;
#endif
	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	rc = ulp_port_db_dev_port_to_ulp_index(ulp_ctxt, port_id, &ifindex);
	if (rc == -ENOENT) {
		/* port not found, allocate one */
		ifindex = ulp_port_db_allocate_ifindex(ulp_ctxt, port_db);
		if (!ifindex)
			return -ENOMEM;
		port_db->dev_port_list[port_id] = ifindex;
	} else if (rc == -EINVAL) {
		return -EINVAL;
	}

	/* update the interface details */
	intf = &port_db->ulp_intf_list[ifindex];

	if (!vfr) {
		intf->type = bnxt_get_interface_type(bp);
	} else {
		intf->type = BNXT_ULP_INTF_TYPE_VF_REP;
		intf->vf_id = port_id - BNXT_FIRST_VF_FID;
	}
	intf->drv_func_id = bnxt_get_fw_func_id(bp,
						BNXT_ULP_INTF_TYPE_INVALID);
	intf->rdma_sriov_en = BNXT_RDMA_SRIOV_EN(bp) ? 1 : 0;

	/* Update if LAG is enabled on the PFs */
	intf->hw_lag_en = bnxt_is_hw_lag_enabled(bp);

	/* Update if UDCC is enabled on the PF */
	intf->udcc_en = bnxt_udcc_get_mode(bp);

	func = &port_db->ulp_func_id_tbl[intf->drv_func_id];
	if (!func->func_valid) {
		func->func_spif = bnxt_get_phy_port_id(bp);
		func->func_parif = bnxt_get_parif(bp);
		func->phy_port_id = bnxt_get_phy_port_id(bp);
		func->func_valid = true;
		func->ifindex = ifindex;
	}

	/* Get the default vnics only for PFs.*/
	if (intf->type == BNXT_ULP_INTF_TYPE_PF) {
		bnxt_hwrm_get_dflt_vnic_svif(bp, -1, &func->func_vnic,
				     &func->func_svif);
		bnxt_hwrm_get_dflt_roce_vnic(bp, -1,
				     &func->func_roce_vnic,
				     &func->mirror_vnic);
		func->func_roce_vnic = cpu_to_be16(func->func_roce_vnic);
		func->mirror_vnic = cpu_to_be16(func->mirror_vnic);
	}

	/* Table scope is defined for all devices, ignore failures. */
	if (!bnxt_ulp_cntxt_tsid_get(ulp_ctxt, &tsid))
		func->table_scope = tsid;

	if (intf->type == BNXT_ULP_INTF_TYPE_VF_REP) {
		intf->vf_func_id =
			bnxt_vfr_get_fw_func_id(vfr);
		func = &port_db->ulp_func_id_tbl[intf->vf_func_id];
		bnxt_hwrm_get_dflt_vnic_svif(bp, intf->vf_func_id,
					     &func->func_vnic,
					     &func->func_svif);
		bnxt_hwrm_get_dflt_roce_vnic(bp, intf->vf_func_id,
					     &func->func_roce_vnic,
					     &func->mirror_vnic);
		func->func_roce_vnic = cpu_to_be16(func->func_roce_vnic);
		func->mirror_vnic = cpu_to_be16(func->mirror_vnic);
		func->func_spif = bnxt_get_phy_port_id(bp);
		func->func_parif = bnxt_get_parif(bp);
		func->phy_port_id = bnxt_get_phy_port_id(bp);
		func->ifindex = ifindex;
		func->func_valid = true;
		func->vf_meta_data = cpu_to_be16(BNXT_ULP_META_VF_FLAG |
						 intf->vf_func_id);
		if (!bnxt_ulp_cntxt_tsid_get(ulp_ctxt, &tsid))
			func->table_scope = tsid;
	}

	/* When there is no match, the default action is to send the packet to
	 * the kernel. And to send it to the kernel, we need the PF's vnic id.
	 */
	func->func_parent_vnic = bnxt_get_parent_vnic_id(bp, intf->type);
	func->func_parent_vnic = cpu_to_be16(func->func_parent_vnic);
	bnxt_get_iface_mac(bp, intf->type, func->func_mac,
			   func->func_parent_mac);
	port_data = &port_db->phy_port_list[func->phy_port_id];
	port_data->port_svif = bnxt_get_svif(bp, false,
					     BNXT_ULP_INTF_TYPE_INVALID);
	port_data->port_spif = bnxt_get_phy_port_id(bp);
	port_data->port_parif = bnxt_get_parif(bp);
	port_data->port_vport = bnxt_get_vport(bp);
	port_data->port_lag_vport = bnxt_get_lag_vport(bp);
	port_data->port_valid = true;
	ulp_port_db_dump(ulp_ctxt, port_db, intf, port_id);
	return 0;
}

#else

int ulp_port_db_dev_port_intf_update(struct bnxt_ulp_context *ulp_ctxt,
				     struct bnxt *bp, void *vf_rep)
{
	return -EINVAL;
}

#endif

/**
 * Api to get the ulp ifindex for a given device port.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @port_id:.device port id
 * @ifindex: ulp ifindex
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_dev_port_to_ulp_index(struct bnxt_ulp_context *ulp_ctxt,
				  u32 port_id,
				  u32 *ifindex)
{
	struct bnxt_ulp_port_db *port_db;

	*ifindex = 0;
	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || port_id >= TC_MAX_ETHPORTS) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	if (!port_db->dev_port_list[port_id]) {
		netdev_dbg(ulp_ctxt->bp->dev,
			   "Port: %d not present in port_db\n", port_id);
		return -ENOENT;
	}

	*ifindex = port_db->dev_port_list[port_id];
	return 0;
}

/**
 * Api to get the function id for a given ulp ifindex.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @ifindex: ulp ifindex
 * @func_id: the function id of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_function_id_get(struct bnxt_ulp_context *ulp_ctxt,
			    u32 ifindex,
			    u32 fid_type,
			    u16 *func_id)
{
	struct bnxt_ulp_port_db *port_db;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (fid_type == BNXT_ULP_DRV_FUNC_FID)
		*func_id =  port_db->ulp_intf_list[ifindex].drv_func_id;
	else
		*func_id =  port_db->ulp_intf_list[ifindex].vf_func_id;

	return 0;
}

/**
 * Api to get the VF RoCE support for a given ulp ifindex.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @ifindex: ulp ifindex
 * @func_id: the function id of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_vf_roce_get(struct bnxt_ulp_context *ulp_ctxt,
			u32 port_id,
			u16 *vf_roce)
{
	struct bnxt_ulp_port_db *port_db;
	u32 ifindex;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || port_id >= TC_MAX_ETHPORTS) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	ifindex = port_db->dev_port_list[port_id];
	if (!ifindex)
		return -ENOENT;

	*vf_roce =  port_db->ulp_intf_list[ifindex].rdma_sriov_en;

	return 0;
}

/**
 * Api to get the HW LAG support for a given ulp ifindex.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @ifindex: ulp ifindex
 * @func_id: the function id of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_hw_lag_get(struct bnxt_ulp_context *ulp_ctxt,
		       u32 port_id,
		       u16 *hw_lag_en)
{
	struct bnxt_ulp_port_db *port_db;
	u32 ifindex;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || port_id >= TC_MAX_ETHPORTS) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	ifindex = port_db->dev_port_list[port_id];
	if (!ifindex)
		return -ENOENT;

	*hw_lag_en =  port_db->ulp_intf_list[ifindex].hw_lag_en;

	return 0;
}

/**
 * Api to get the UDCC support for a given ulp ifindex.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @ifindex: ulp ifindex
 * @func_id: the function id of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_udcc_get(struct bnxt_ulp_context *ulp_ctxt,
		     u32 port_id,
		     u8 *udcc)
{
	struct bnxt_ulp_port_db *port_db;
	u32 ifindex;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || port_id >= TC_MAX_ETHPORTS) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	ifindex = port_db->dev_port_list[port_id];
	if (!ifindex)
		return -ENOENT;

	*udcc =  port_db->ulp_intf_list[ifindex].udcc_en;

	return 0;
}

/**
 * Api to get the svif for a given ulp ifindex.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @ifindex: ulp ifindex
 * @svif_type: the svif type of the given ifindex.
 * @svif: the svif of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_svif_get(struct bnxt_ulp_context *ulp_ctxt,
		     u32 ifindex,
		     u32 svif_type,
		     u16 *svif)
{
	struct bnxt_ulp_port_db *port_db;
	u16 phy_port_id, func_id;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (svif_type == BNXT_ULP_DRV_FUNC_SVIF) {
		func_id = port_db->ulp_intf_list[ifindex].drv_func_id;
		*svif = port_db->ulp_func_id_tbl[func_id].func_svif;
	} else if (svif_type == BNXT_ULP_VF_FUNC_SVIF) {
		func_id = port_db->ulp_intf_list[ifindex].vf_func_id;
		*svif = port_db->ulp_func_id_tbl[func_id].func_svif;
	} else {
		func_id = port_db->ulp_intf_list[ifindex].drv_func_id;
		phy_port_id = port_db->ulp_func_id_tbl[func_id].phy_port_id;
		*svif = port_db->phy_port_list[phy_port_id].port_svif;
	}

	return 0;
}

/**
 * Api to get the spif for a given ulp ifindex.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @ifindex: ulp ifindex
 * @spif_type: the spif type of the given ifindex.
 * @spif: the spif of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_spif_get(struct bnxt_ulp_context *ulp_ctxt,
		     u32 ifindex,
		     u32 spif_type,
		     u16 *spif)
{
	struct bnxt_ulp_port_db *port_db;
	u16 phy_port_id, func_id;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (spif_type == BNXT_ULP_DRV_FUNC_SPIF) {
		func_id = port_db->ulp_intf_list[ifindex].drv_func_id;
		*spif = port_db->ulp_func_id_tbl[func_id].func_spif;
	} else if (spif_type == BNXT_ULP_VF_FUNC_SPIF) {
		func_id = port_db->ulp_intf_list[ifindex].vf_func_id;
		*spif = port_db->ulp_func_id_tbl[func_id].func_spif;
	} else {
		func_id = port_db->ulp_intf_list[ifindex].drv_func_id;
		phy_port_id = port_db->ulp_func_id_tbl[func_id].phy_port_id;
		*spif = port_db->phy_port_list[phy_port_id].port_spif;
	}

	return 0;
}

/**
 * Api to get the parif for a given ulp ifindex.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @ifindex: ulp ifindex
 * @parif_type: the parif type of the given ifindex.
 * @parif: the parif of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_parif_get(struct bnxt_ulp_context *ulp_ctxt,
		      u32 ifindex,
		      u32 parif_type,
		      u16 *parif)
{
	struct bnxt_ulp_port_db *port_db;
	u16 phy_port_id, func_id;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	if (parif_type == BNXT_ULP_DRV_FUNC_PARIF) {
		func_id = port_db->ulp_intf_list[ifindex].drv_func_id;
		*parif = port_db->ulp_func_id_tbl[func_id].func_parif;
	} else if (parif_type == BNXT_ULP_VF_FUNC_PARIF) {
		func_id = port_db->ulp_intf_list[ifindex].vf_func_id;
		*parif = port_db->ulp_func_id_tbl[func_id].func_parif;
	} else {
		func_id = port_db->ulp_intf_list[ifindex].drv_func_id;
		phy_port_id = port_db->ulp_func_id_tbl[func_id].phy_port_id;
		*parif = port_db->phy_port_list[phy_port_id].port_parif;
	}
	/* Parif needs to be reset to a free partition */
	*parif += BNXT_ULP_FREE_PARIF_BASE;

	return 0;
}

/**
 * Api to get the vnic id for a given ulp ifindex.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @ifindex: ulp ifindex
 * @vnic: the vnic of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_default_vnic_get(struct bnxt_ulp_context *ulp_ctxt,
			     u32 ifindex,
			     u32 vnic_type,
			     u16 *vnic)
{
	struct bnxt_ulp_port_db *port_db;
	u16 func_id;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (vnic_type == BNXT_ULP_DRV_FUNC_VNIC) {
		func_id = port_db->ulp_intf_list[ifindex].drv_func_id;
		*vnic = port_db->ulp_func_id_tbl[func_id].func_vnic;
	} else {
		func_id = port_db->ulp_intf_list[ifindex].vf_func_id;
		*vnic = port_db->ulp_func_id_tbl[func_id].func_vnic;
	}

	return 0;
}

/**
 * Api to get the vport id for a given ulp ifindex.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @ifindex: ulp ifindex
 * @vport: the port of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_vport_get(struct bnxt_ulp_context *ulp_ctxt,
		      u32 ifindex, u16 *vport)
{
	struct bnxt_ulp_port_db *port_db;
	u16 phy_port_id, func_id;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	func_id = port_db->ulp_intf_list[ifindex].drv_func_id;
	phy_port_id = port_db->ulp_func_id_tbl[func_id].phy_port_id;
	*vport = port_db->phy_port_list[phy_port_id].port_vport;
	return 0;
}

/**
 * Api to get the lag vport id for a given ulp ifindex.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @ifindex: ulp ifindex
 * @vport: the lag port of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_lag_vport_get(struct bnxt_ulp_context *ulp_ctxt,
			  u32 ifindex, u16 *vport)
{
	struct bnxt_ulp_port_db *port_db;
	u16 phy_port_id, func_id;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	func_id = port_db->ulp_intf_list[ifindex].drv_func_id;
	phy_port_id = port_db->ulp_func_id_tbl[func_id].phy_port_id;
	*vport = port_db->phy_port_list[phy_port_id].port_lag_vport;
	return 0;
}

/**
 * Api to get the vport for a given physical port.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @phy_port: physical port index
 * @out_port: the port of the given physical index
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_phy_port_vport_get(struct bnxt_ulp_context *ulp_ctxt,
			       u32 phy_port,
			       u16 *out_port)
{
	struct bnxt_ulp_port_db *port_db;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || phy_port >= port_db->phy_port_cnt) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	*out_port = port_db->phy_port_list[phy_port].port_vport;
	return 0;
}

/**
 * Api to get the svif for a given physical port.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @phy_port: physical port index
 * @svif: the svif of the given physical index
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_phy_port_svif_get(struct bnxt_ulp_context *ulp_ctxt,
			      u32 phy_port,
			      u16 *svif)
{
	struct bnxt_ulp_port_db *port_db;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || phy_port >= port_db->phy_port_cnt) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	*svif = port_db->phy_port_list[phy_port].port_svif;
	return 0;
}

/**
 * Api to get the port type for a given ulp ifindex.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @ifindex: ulp ifindex
 *
 * Returns port type.
 */
enum bnxt_ulp_intf_type
ulp_port_db_port_type_get(struct bnxt_ulp_context *ulp_ctxt,
			  u32 ifindex)
{
	struct bnxt_ulp_port_db *port_db;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || ifindex >= port_db->ulp_intf_list_size || !ifindex) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return BNXT_ULP_INTF_TYPE_INVALID;
	}
	return port_db->ulp_intf_list[ifindex].type;
}

/**
 * Api to get the ulp ifindex for a given function id.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @func_id:.device func id
 * @ifindex: ulp ifindex
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_dev_func_id_to_ulp_index(struct bnxt_ulp_context *ulp_ctxt,
				     u32 func_id, u32 *ifindex)
{
	struct bnxt_ulp_port_db *port_db;

	*ifindex = 0;
	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || func_id >= BNXT_PORT_DB_MAX_FUNC) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	if (!port_db->ulp_func_id_tbl[func_id].func_valid)
		return -ENOENT;

	*ifindex = port_db->ulp_func_id_tbl[func_id].ifindex;
	return 0;
}

/**
 * Api to get the function id for a given port id.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @port_id: fw fid
 * @func_id: the function id of the given ifindex.
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_port_func_id_get(struct bnxt_ulp_context *ulp_ctxt,
			     u16 port_id, u16 *func_id)
{
	struct bnxt_ulp_port_db *port_db;
	u32 ifindex;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || port_id >= TC_MAX_ETHPORTS) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	ifindex = port_db->dev_port_list[port_id];
	if (!ifindex)
		return -ENOENT;

	switch (port_db->ulp_intf_list[ifindex].type) {
	case BNXT_ULP_INTF_TYPE_TRUSTED_VF:
	case BNXT_ULP_INTF_TYPE_PF:
		*func_id =  port_db->ulp_intf_list[ifindex].drv_func_id;
		break;
	case BNXT_ULP_INTF_TYPE_VF:
	case BNXT_ULP_INTF_TYPE_VF_REP:
		*func_id =  port_db->ulp_intf_list[ifindex].vf_func_id;
		break;
	default:
		*func_id = 0;
		break;
	}
	return 0;
}

/* internal function to get the */
static struct ulp_func_if_info*
ulp_port_db_func_if_info_get(struct bnxt_ulp_context *ulp_ctxt,
			     u32 port_id)
{
	struct bnxt_ulp_port_db *port_db;
	u16 func_id;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (ulp_port_db_port_func_id_get(ulp_ctxt, port_id, &func_id)) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid port_id %x\n", port_id);
		return NULL;
	}

	if (!port_db->ulp_func_id_tbl[func_id].func_valid) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid func_id %x\n", func_id);
		return NULL;
	}
	return &port_db->ulp_func_id_tbl[func_id];
}

/**
 * Api to get the parent mac address for a given port id.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @port_id: device port id
 * @mac_addr: mac address
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_parent_mac_addr_get(struct bnxt_ulp_context *ulp_ctxt,
				u32 port_id, u8 **mac_addr)
{
	struct ulp_func_if_info *info;

	info = ulp_port_db_func_if_info_get(ulp_ctxt, port_id);
	if (info) {
		*mac_addr = info->func_parent_mac;
		return 0;
	}
	return -EINVAL;
}

/**
 * Api to get the mac address for a given port id.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @port_id: device port id
 * @mac_addr: mac address
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_drv_mac_addr_get(struct bnxt_ulp_context *ulp_ctxt,
			     u32 port_id, u8 **mac_addr)
{
	struct ulp_func_if_info *info;

	info = ulp_port_db_func_if_info_get(ulp_ctxt, port_id);
	if (info) {
		*mac_addr = info->func_mac;
		return 0;
	}
	return -EINVAL;
}

/**
 * Api to get the parent vnic for a given port id.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @port_id: device port id
 * @vnic: parent vnic
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_parent_vnic_get(struct bnxt_ulp_context *ulp_ctxt,
			    u32 port_id, u8 **vnic)
{
	struct ulp_func_if_info *info;

	info = ulp_port_db_func_if_info_get(ulp_ctxt, port_id);
	if (info) {
		*vnic = (u8 *)&info->func_parent_vnic;
		return 0;
	}
	return -EINVAL;
}

/**
 * Api to get the phy port for a given port id.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @port_id: device port id
 * @phy_port: phy_port of the dpdk port_id
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_phy_port_get(struct bnxt_ulp_context *ulp_ctxt,
			 u32 port_id, u16 *phy_port)
{
	struct ulp_func_if_info *info;

	info = ulp_port_db_func_if_info_get(ulp_ctxt, port_id);
	if (info) {
		*phy_port = info->phy_port_id;
		return 0;
	}
	return -EINVAL;
}

/**
 * Api to get the port type for a given port id.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @port_id: device port id
 * @type: type if pf or not
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_port_is_pf_get(struct bnxt_ulp_context *ulp_ctxt,
			   u32 port_id, u8 **type)
{
	struct ulp_func_if_info *info;
	struct bnxt_ulp_port_db *port_db;
	uint16_t pid;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	info = ulp_port_db_func_if_info_get(ulp_ctxt, port_id);
	if (info) {
		pid = info->ifindex;
		*type = (u8 *)&port_db->ulp_intf_list[pid].type_is_pf;
		return 0;
	}
	return -EINVAL;
}

/**
 * Api to get the meta data for a given port id.
 *
 * @ulp_ctxt [in] Ptr to ulp context
 * @port_id [in] dpdk port id
 * @meta data [out] the meta data of the given port
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_port_meta_data_get(struct bnxt_ulp_context *ulp_ctxt,
			       u16 port_id, u8 **meta_data)
{
	struct ulp_func_if_info *info;

	info = ulp_port_db_func_if_info_get(ulp_ctxt, port_id);
	if (info) {
		*meta_data = (uint8_t *)&info->vf_meta_data;
		return 0;
	}
	return -EINVAL;
}

/** Api to get the function id for a given port id
 *
 * @ulp_ctxt: Ptr to ulp context
 * @port_id: dpdk port id
 * @fid_data: the function id of the given port
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_port_vf_fid_get(struct bnxt_ulp_context *ulp_ctxt,
			    u16 port_id, u8 **fid_data)
{
	struct bnxt_ulp_port_db *port_db;
	u32 ifindex;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || port_id >= TC_MAX_ETHPORTS) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	ifindex = port_db->dev_port_list[port_id];
	if (!ifindex)
		return -ENOENT;

	if (port_db->ulp_intf_list[ifindex].type != BNXT_ULP_INTF_TYPE_VF &&
	    port_db->ulp_intf_list[ifindex].type != BNXT_ULP_INTF_TYPE_VF_REP)
		return -EINVAL;

	*fid_data = (uint8_t *)&port_db->ulp_intf_list[ifindex].vf_func_id;
	return 0;
}

int
ulp_port_db_port_table_scope_get(struct bnxt_ulp_context *ulp_ctxt,
				 u16 port_id, u8 **tsid)
{
	struct ulp_func_if_info *info;

	info = ulp_port_db_func_if_info_get(ulp_ctxt, port_id);
	if (info) {
		*tsid = &info->table_scope;
		return 0;
	}
	return -EINVAL;
}

/* Api to set the PF Mirror Id for a given port id
 *
 * ulp_ctxt [in] Ptr to ulp context
 * port_id [in] port id
 * mirror id [in] mirror id
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_port_table_mirror_set(struct bnxt_ulp_context *ulp_ctxt, enum tf_dir dir,
				  u16 port_id, u32 mirror_id)
{
	struct ulp_phy_port_info *port_data;
	struct bnxt_ulp_port_db *port_db;
	struct ulp_interface_info *intf;
	struct ulp_func_if_info *func;
	u32 ifindex;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}

	if (ulp_port_db_dev_port_to_ulp_index(ulp_ctxt, port_id, &ifindex)) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid port id %u\n", port_id);
		return -EINVAL;
	}

	intf = &port_db->ulp_intf_list[ifindex];
	func = &port_db->ulp_func_id_tbl[intf->drv_func_id];
	if (!func->func_valid) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid func for port id %u\n", port_id);
		return -EINVAL;
	}

	port_data = &port_db->phy_port_list[func->phy_port_id];
	if (!port_data->port_valid) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid phy port\n");
		return -EINVAL;
	}

	if (dir == TF_DIR_RX)
		port_data->port_mirror_id_ingress = mirror_id;
	else
		port_data->port_mirror_id_egress = mirror_id;
	return 0;
}

/* Api to get the PF Mirror Id for a given port id
 *
 * ulp_ctxt [in] Ptr to ulp context
 * port_id [in] port id
 * mirror id [in] mirror id
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_port_table_mirror_get(struct bnxt_ulp_context *ulp_ctxt, enum tf_dir dir,
				  u16 port_id, u8 **mirror_id)
{
	struct ulp_phy_port_info *port_data;
	struct bnxt_ulp_port_db *port_db;
	struct ulp_interface_info *intf;
	struct ulp_func_if_info *func;
	u32 ifindex;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	if (ulp_port_db_dev_port_to_ulp_index(ulp_ctxt, port_id, &ifindex)) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid port id %u\n", port_id);
		return -EINVAL;
	}
	intf = &port_db->ulp_intf_list[ifindex];
	func = &port_db->ulp_func_id_tbl[intf->drv_func_id];
	if (!func->func_valid) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid func for port id %u\n", port_id);
		return -EINVAL;
	}
	port_data = &port_db->phy_port_list[func->phy_port_id];
	if (!port_data->port_valid) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid phy port\n");
		return -EINVAL;
	}
	if (dir == TF_DIR_RX)
		*mirror_id = (u8 *)&port_data->port_mirror_id_ingress;
	else
		*mirror_id = (u8 *)&port_data->port_mirror_id_egress;
	return 0;
}

/**
 * Api to get the RoCE vnic for a given port id.
 *
 * @ulp_ctxt:  Ptr to ulp context
 * @port_id:   device port id
 * @roce_vnic: RoCE vnic
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_drv_roce_vnic_get(struct bnxt_ulp_context *ulp_ctxt,
			      u32 port_id, u8 **roce_vnic)
{
	struct ulp_func_if_info *info;

	info = ulp_port_db_func_if_info_get(ulp_ctxt, port_id);
	if (info) {
		*roce_vnic = (uint8_t *)&info->func_roce_vnic;
		return 0;
	}
	return -EINVAL;
}

/**
 * Api to get the socket direct svif for a given device port.
 *
 * ulp_ctxt [in] Ptr to ulp context
 * port_id [in] device port id
 * svif [out] the socket direct svif of the given device index
 *
 * Returns 0 on success or negative number on failure.
 */
int
ulp_port_db_port_socket_direct_svif_get(struct bnxt_ulp_context *ulp_ctxt,
					uint32_t port_id,
					uint16_t *svif)
{
	struct bnxt_ulp_port_db *port_db;
	uint16_t phy_port_id;
	uint16_t func_id;
	uint32_t ifindex;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);

	if (!port_db || port_id >= TC_MAX_ETHPORTS) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	if (!port_db->dev_port_list[port_id])
		return -ENOENT;

	/* Get physical port id */
	ifindex = port_db->dev_port_list[port_id];
	func_id = port_db->ulp_intf_list[ifindex].drv_func_id;
	phy_port_id = port_db->ulp_func_id_tbl[func_id].phy_port_id;

	/* Calculate physical port id for socket direct port */
	phy_port_id = phy_port_id ? 0 : 1;
	if (phy_port_id >= port_db->phy_port_cnt ||
	    !port_db->phy_port_list[phy_port_id].port_valid) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	*svif = port_db->phy_port_list[phy_port_id].port_svif;
	return 0;
}

/* API to get the VF ID for a given port_id.
 *
 * @ulp_ctxt: Ptr to ulp context
 * @port_id: device port id
 * @vf_id: Zero based global (per-card) VF-id and not per-PF
 * Returns 0 on success or negative errno on failure.
 */
int
ulp_port_db_vf_id_get(struct bnxt_ulp_context *ulp_ctxt,
		      u32 port_id, u16 *vf_id)
{
	struct bnxt_ulp_port_db *port_db;
	u32 ifindex;

	port_db = bnxt_ulp_cntxt_ptr2_port_db_get(ulp_ctxt);
	if (!port_db || port_id >= TC_MAX_ETHPORTS) {
		netdev_dbg(ulp_ctxt->bp->dev, "Invalid Arguments\n");
		return -EINVAL;
	}
	ifindex = port_db->dev_port_list[port_id];
	if (!ifindex)
		return -ENOENT;

	*vf_id =  port_db->ulp_intf_list[ifindex].vf_id;

	return 0;
}

int
ulp_port_db_port_table_mirror_vnic_get(struct bnxt_ulp_context *ulp_ctxt,
				       u16 port_id, u8 **mirror_vnic)
{
	struct ulp_func_if_info *info;

	info = ulp_port_db_func_if_info_get(ulp_ctxt, port_id);
	if (info) {
		*mirror_vnic = (u8 *)&info->mirror_vnic;
		return 0;
	}
	return -EINVAL;
}

int
ulp_port_db_mirror_vnic_enabled(struct bnxt_ulp_context *ulp_ctxt,
				u16 port_id)
{
	struct ulp_func_if_info *info;

	info = ulp_port_db_func_if_info_get(ulp_ctxt, port_id);
	if (info && info->mirror_vnic)
		return 1;

	return 0;
}
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
