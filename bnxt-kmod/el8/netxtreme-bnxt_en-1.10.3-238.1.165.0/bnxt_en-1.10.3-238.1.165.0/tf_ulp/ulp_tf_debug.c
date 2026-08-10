// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "ulp_tf_debug.h"

#include "tf_core.h"
#include "tf_em.h"
#include "tf_msg.h"
#include "tf_ext_flow_handle.h"

#include "ulp_port_db.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
const char *tf_if_tbl_2_str(u32 type)
{
	enum tf_if_tbl_type id_type = type;

	switch (id_type) {
	case TF_IF_TBL_TYPE_PROF_SPIF_DFLT_L2_CTXT:
		return "spif dflt l2 ctxt";
	case TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR:
		return "parif act rec ptr";
	case TF_IF_TBL_TYPE_PROF_PARIF_ERR_ACT_REC_PTR:
		return "parif err act rec ptr";
	case TF_IF_TBL_TYPE_LKUP_PARIF_DFLT_ACT_REC_PTR:
		return "lkup parif act rec ptr";
	case TF_IF_TBL_TYPE_ILT:
		return "ilt tbl";
	case TF_IF_TBL_TYPE_VSPT:
		return "vspt tbl";
	default:
		return "Invalid identifier";
	}
}

#ifdef TC_BNXT_TRUFLOW_DEBUG

void ulp_port_db_dump(struct bnxt_ulp_context *ulp_ctx,
		      struct bnxt_ulp_port_db *port_db,
		      struct ulp_interface_info *intf, u32 port_id)
{
	struct ulp_func_if_info *func;
	struct ulp_phy_port_info *port_data;

	netdev_dbg(ulp_ctx->bp->dev, "*****Dump for port_id %d ******\n",
		   port_id);
	netdev_dbg(ulp_ctx->bp->dev,
		   "type=0x%x, drv_func_id=0x%x, vf_func_id=0x%x vf_roce=%d udcc_en=%d\n",
		   intf->type, intf->drv_func_id, intf->vf_func_id,
		   intf->rdma_sriov_en, intf->udcc_en);

	netdev_dbg(ulp_ctx->bp->dev,
		   "hw_lag_en=%d\n", intf->hw_lag_en);

	func = &port_db->ulp_func_id_tbl[intf->drv_func_id];
	netdev_dbg(ulp_ctx->bp->dev,
		   "drv_func_svif=0x%0x, drv_func_spif=0x%0x ",
		   func->func_svif, func->func_spif);
	netdev_dbg(ulp_ctx->bp->dev,
		   "drv_func_parif=0x%0x, drv_default_vnic=0x%0x drv_roce_vnic=0x%0x\n",
		   func->func_parif, func->func_vnic, be16_to_cpu(func->func_roce_vnic));
	netdev_dbg(ulp_ctx->bp->dev, "drv_func_parent_vnic=0x%0x\n",
		   be16_to_cpu(func->func_parent_vnic));
	netdev_dbg(ulp_ctx->bp->dev, "Mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
		   func->func_mac[0], func->func_mac[1], func->func_mac[2],
		   func->func_mac[3], func->func_mac[4], func->func_mac[5]);
	netdev_dbg(ulp_ctx->bp->dev,
		   "Parent Mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
		   func->func_parent_mac[0], func->func_parent_mac[1],
		   func->func_parent_mac[2], func->func_parent_mac[3],
		   func->func_parent_mac[4], func->func_parent_mac[5]);

	if (intf->type == BNXT_ULP_INTF_TYPE_VF_REP) {
		func = &port_db->ulp_func_id_tbl[intf->vf_func_id];
		netdev_dbg(ulp_ctx->bp->dev,
			   "vf_func_svif=0x%0x, vf_func_spif=0x%0x ",
			   func->func_svif, func->func_spif);
		netdev_dbg(ulp_ctx->bp->dev,
			   "vf_func_parif=0x%0x,  vf_default_vnic=0x%0x vf_roce_vnic=0x0%x\n",
			   func->func_parif, func->func_vnic, be16_to_cpu(func->func_roce_vnic));
		netdev_dbg(ulp_ctx->bp->dev, "vf_func_parent_vnic=0x%0x ",
			   be16_to_cpu(func->func_parent_vnic));
		netdev_dbg(ulp_ctx->bp->dev,
			   "Mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
			   func->func_mac[0], func->func_mac[1],
			   func->func_mac[2], func->func_mac[3],
			   func->func_mac[4], func->func_mac[5]);
	}
	port_data = &port_db->phy_port_list[func->phy_port_id];
	netdev_dbg(ulp_ctx->bp->dev,
		   "phy_port_svif=0x%0x, phy_port_spif=0x%0x ",
		   port_data->port_svif, port_data->port_spif);
	netdev_dbg(ulp_ctx->bp->dev,
		   "phy_port_parif=0x%0x, phy_port_vport=0x%0x\n",
		   port_data->port_parif, port_data->port_vport);

	netdev_dbg(ulp_ctx->bp->dev, "***** dump complete ******\n");
}

#else /* TC_BNXT_TRUFLOW_DEBUG */

void ulp_port_db_dump(struct bnxt_ulp_context *ulp_ctx,
		      struct bnxt_ulp_port_db *port_db,
		      struct ulp_interface_info *intf, u32 port_id)
{
}

#endif /* TC_BNXT_TRUFLOW_DEBUG */
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
