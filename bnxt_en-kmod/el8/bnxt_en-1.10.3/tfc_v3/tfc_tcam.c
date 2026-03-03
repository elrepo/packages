// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include "tfc.h"
#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "tfc.h"
#include "tfc_msg.h"
#include "tfc_util.h"

int tfc_tcam_alloc(struct tfc *tfcp, u16 fid, enum cfa_track_type tt, u16 priority,
		   u8 key_sz_in_bytes, struct tfc_tcam_info *tcam_info)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (!tcam_info) {
		netdev_dbg(bp->dev, "%s: tcam_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (tcam_info->rsubtype >= CFA_RSUBTYPE_TCAM_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid tcam subtype: %d\n", __func__,
			   tcam_info->rsubtype);
		return -EINVAL;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		netdev_dbg(bp->dev, "%s: bp not PF or trusted VF\n", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n", __func__, rc);
		return rc;
	}

	rc = tfc_msg_tcam_alloc(tfcp, fid, sid, tcam_info->dir,
				tcam_info->rsubtype, tt, priority,
				key_sz_in_bytes, &tcam_info->id);
	if (rc)
		netdev_dbg(bp->dev, "%s: alloc failed %s:%s rc:%d\n", __func__,
			   tfc_dir_2_str(tcam_info->dir),
			   tfc_tcam_2_str(tcam_info->rsubtype), rc);

	return rc;
}

int tfc_tcam_alloc_set(struct tfc *tfcp, u16 fid, enum cfa_track_type tt,
		       u16 priority, struct tfc_tcam_info *tcam_info,
		       const struct tfc_tcam_data *tcam_data)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (!tcam_info) {
		netdev_dbg(bp->dev, "%s: tcam_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (!tcam_data) {
		netdev_dbg(bp->dev, "%s: tcam_data is NULL\n", __func__);
		return -EINVAL;
	}

	if (tcam_info->rsubtype >= CFA_RSUBTYPE_TCAM_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid tcam subtype: %d\n", __func__,
			   tcam_info->rsubtype);
		return -EINVAL;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		netdev_dbg(bp->dev, "%s: bp not PF or trusted VF\n", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = tfc_msg_tcam_alloc_set(tfcp, fid, sid, tcam_info->dir,
				    tcam_info->rsubtype, tt, &tcam_info->id,
				    priority, tcam_data->key,
				    tcam_data->key_sz_in_bytes,
				    tcam_data->mask,
				    tcam_data->remap,
				    tcam_data->remap_sz_in_bytes);
	if (rc)
		netdev_dbg(bp->dev, "%s: alloc_set failed: %s:%s rc:%d\n", __func__,
			   tfc_dir_2_str(tcam_info->dir),
			   tfc_tcam_2_str(tcam_info->rsubtype), rc);

	return rc;
}

int tfc_tcam_set(struct tfc *tfcp, u16 fid, const struct tfc_tcam_info *tcam_info,
		 const struct tfc_tcam_data *tcam_data)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (!tcam_info) {
		netdev_dbg(bp->dev, "%s: tcam_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (!tcam_data) {
		netdev_dbg(bp->dev, "%s: tcam_data is NULL\n", __func__);
		return -EINVAL;
	}

	if (tcam_info->rsubtype >= CFA_RSUBTYPE_TCAM_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid tcam subtype: %d\n", __func__,
			   tcam_info->rsubtype);
		return -EINVAL;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		netdev_dbg(bp->dev, "%s: bp not PF or trusted VF\n", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n", __func__, rc);
		return rc;
	}

	rc = tfc_msg_tcam_set(tfcp, fid, sid, tcam_info->dir,
			      tcam_info->rsubtype, tcam_info->id,
			      tcam_data->key,
			      tcam_data->key_sz_in_bytes,
			      tcam_data->mask, tcam_data->remap,
			      tcam_data->remap_sz_in_bytes);
	if (rc)
		netdev_dbg(bp->dev, "%s: set failed: %s:%s %d rc:%d\n", __func__,
			   tfc_dir_2_str(tcam_info->dir),
			   tfc_tcam_2_str(tcam_info->rsubtype), tcam_info->id, rc);

	return rc;
}

int tfc_tcam_get(struct tfc *tfcp, u16 fid, const struct tfc_tcam_info *tcam_info,
		 struct tfc_tcam_data *tcam_data)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (!tcam_info) {
		netdev_dbg(bp->dev, "%s: tcam_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (!tcam_data) {
		netdev_dbg(bp->dev, "%s: tcam_data is NULL\n", __func__);
		return -EINVAL;
	}

	if (tcam_info->rsubtype >= CFA_RSUBTYPE_TCAM_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid tcam subtype: %d\n", __func__,
			   tcam_info->rsubtype);
		return -EINVAL;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		netdev_dbg(bp->dev, "%s: bp not PF or trusted VF\n", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n", __func__, rc);
		return rc;
	}

	rc = tfc_msg_tcam_get(tfcp, fid, sid, tcam_info->dir,
			      tcam_info->rsubtype, tcam_info->id,
			      tcam_data->key, &tcam_data->key_sz_in_bytes,
			      tcam_data->mask, tcam_data->remap,
			      &tcam_data->remap_sz_in_bytes);
	if (rc)
		netdev_dbg(bp->dev, "%s: get failed: %s:%s %d rc:%d\n", __func__,
			   tfc_dir_2_str(tcam_info->dir),
			   tfc_tcam_2_str(tcam_info->rsubtype), tcam_info->id, rc);

	return rc;
}

int tfc_tcam_free(struct tfc *tfcp, u16 fid, const struct tfc_tcam_info *tcam_info)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (!tcam_info) {
		netdev_dbg(bp->dev, "%s: tcam_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (tcam_info->rsubtype >= CFA_RSUBTYPE_TCAM_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid tcam subtype: %d\n", __func__,
			   tcam_info->rsubtype);
		return -EINVAL;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		netdev_dbg(bp->dev, "%s: bp not PF or trusted VF\n", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n", __func__, rc);
		return rc;
	}

	rc = tfc_msg_tcam_free(tfcp, fid, sid, tcam_info->dir,
			       tcam_info->rsubtype, tcam_info->id);
	if (rc)
		netdev_dbg(bp->dev, "%s: free failed: %s:%s:%d rc:%d\n", __func__,
			   tfc_dir_2_str(tcam_info->dir),
			   tfc_tcam_2_str(tcam_info->rsubtype), tcam_info->id, rc);
	return rc;
}
