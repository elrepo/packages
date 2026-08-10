// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "tfc.h"
#include "tfc_msg.h"
#include "tfc_util.h"

#define BLKTYPE_IS_CFA(blktype) \
		(CFA_IDX_TBL_BLKTYPE_CFA == (blktype))

static int tfc_idx_tbl_alloc_check(struct tfc *tfcp, u16 fid,
				   enum cfa_track_type tt,
				   struct tfc_idx_tbl_info *tbl_info)
{
	struct bnxt *bp = tfcp->bp;

	if (!bp)
		return -EINVAL;

	if (!tbl_info) {
		netdev_dbg(bp->dev, "%s: tbl_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (tt >= CFA_TRACK_TYPE_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid track type: %d\n", __func__, tt);
		return -EINVAL;
	}

	if (tbl_info->dir >= CFA_DIR_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid cfa dir: %d\n", __func__, tbl_info->dir);
		return -EINVAL;
	}

	if (BLKTYPE_IS_CFA(tbl_info->blktype) &&
	    tbl_info->rsubtype >= CFA_RSUBTYPE_IDX_TBL_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid idx tbl subtype: %d\n", __func__,
			   tbl_info->rsubtype);
		return -EINVAL;
	}
	return 0;
}

int tfc_idx_tbl_alloc(struct tfc *tfcp, u16 fid,
		      enum cfa_track_type tt,
		      struct tfc_idx_tbl_info *tbl_info)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (tfc_idx_tbl_alloc_check(tfcp, fid, tt, tbl_info))
		return -EINVAL;

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

	rc = tfc_msg_idx_tbl_alloc(tfcp, fid, sid, tt, tbl_info->dir,
				   tbl_info->rsubtype, tbl_info->blktype,
				   &tbl_info->id);
	if (rc)
		netdev_dbg(bp->dev, "%s: hwrm failed: %s:%s %d\n", __func__,
			   tfc_dir_2_str(tbl_info->dir), tfc_idx_tbl_2_str(tbl_info->rsubtype),
			    rc);

	return rc;
}

static int tfc_idx_tbl_alloc_set_check(struct tfc *tfcp, u16 fid,
				       enum cfa_track_type tt,
				       struct tfc_idx_tbl_info *tbl_info,
				       const u32 *data, u8 data_sz_in_bytes)
{
	struct bnxt *bp = tfcp->bp;

	if (!bp)
		return -EINVAL;

	if (!tbl_info) {
		netdev_dbg(bp->dev, "%s: tbl_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (!data) {
		netdev_dbg(bp->dev, "%s: Invalid data pointer\n", __func__);
		return -EINVAL;
	}

	if (tt >= CFA_TRACK_TYPE_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid track type: %d\n", __func__, tt);
		return -EINVAL;
	}

	if (tbl_info->dir >= CFA_DIR_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid cfa dir: %d\n", __func__, tbl_info->dir);
		return -EINVAL;
	}

	if (BLKTYPE_IS_CFA(tbl_info->blktype) &&
	    tbl_info->rsubtype >= CFA_RSUBTYPE_IDX_TBL_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid idx tbl subtype: %d\n", __func__,
			   tbl_info->rsubtype);
		return -EINVAL;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		netdev_dbg(bp->dev, "%s: bp not PF or trusted VF\n", __func__);
		return -EINVAL;
	}

	if (data_sz_in_bytes == 0) {
		netdev_dbg(bp->dev, "%s: Data size must be greater than zero\n",
			   __func__);
		return -EINVAL;
	}

	return 0;
}

int tfc_idx_tbl_alloc_set(struct tfc *tfcp, u16 fid,
			  enum cfa_track_type tt,
			  struct tfc_idx_tbl_info *tbl_info,
			  const u32 *data, u8 data_sz_in_bytes)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (tfc_idx_tbl_alloc_set_check(tfcp, fid, tt, tbl_info, data, data_sz_in_bytes))
		return -EINVAL;

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = tfc_msg_idx_tbl_alloc_set(tfcp, fid, sid, tt, tbl_info->dir,
				       tbl_info->rsubtype,  tbl_info->blktype,
				       data, data_sz_in_bytes, &tbl_info->id);
	if (rc)
		netdev_dbg(bp->dev, "%s: hwrm failed: %s:%s %d\n", __func__,
			   tfc_dir_2_str(tbl_info->dir), tfc_idx_tbl_2_str(tbl_info->rsubtype),
			   rc);

	return rc;
}

static int tfc_idx_tbl_set_check(struct tfc *tfcp, u16 fid,
				 const struct tfc_idx_tbl_info *tbl_info,
				 const u32 *data, u8 data_sz_in_bytes)
{
	struct bnxt *bp = tfcp->bp;

	if (!bp)
		return -EINVAL;

	if (!tbl_info) {
		netdev_dbg(bp->dev, "%s: tbl_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (tbl_info->dir >= CFA_DIR_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid cfa dir: %d\n", __func__, tbl_info->dir);
		return -EINVAL;
	}

	if (BLKTYPE_IS_CFA(tbl_info->blktype) &&
	    tbl_info->rsubtype >= CFA_RSUBTYPE_IDX_TBL_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid idx tbl subtype: %d\n", __func__,
			   tbl_info->rsubtype);
		return -EINVAL;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		netdev_dbg(bp->dev, "%s: bp not PF or trusted VF\n", __func__);
		return -EINVAL;
	}

	return 0;
}

int tfc_idx_tbl_set(struct tfc *tfcp, u16 fid,
		    const struct tfc_idx_tbl_info *tbl_info,
		    const u32 *data, u8 data_sz_in_bytes)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (tfc_idx_tbl_set_check(tfcp, fid, tbl_info, data, data_sz_in_bytes))
		return -EINVAL;

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = tfc_msg_idx_tbl_set(tfcp, fid, sid, tbl_info->dir,
				 tbl_info->rsubtype, tbl_info->blktype,
				 tbl_info->id, data, data_sz_in_bytes);
	if (rc)
		netdev_dbg(bp->dev, "%s: hwrm failed: %s:%s %d %d\n", __func__,
			   tfc_dir_2_str(tbl_info->dir), tfc_idx_tbl_2_str(tbl_info->rsubtype),
			   tbl_info->id, rc);

	return rc;
}

static int tfc_idx_tbl_get_check(struct tfc *tfcp, u16 fid,
				 const struct tfc_idx_tbl_info *tbl_info,
				 u32 *data, u8 *data_sz_in_bytes)
{
	struct bnxt *bp = tfcp->bp;

	if (!bp)
		return -EINVAL;

	if (!tbl_info) {
		netdev_dbg(bp->dev, "%s: tbl_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (tbl_info->dir >= CFA_DIR_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid cfa dir: %d\n", __func__, tbl_info->dir);
		return -EINVAL;
	}

	if (BLKTYPE_IS_CFA(tbl_info->blktype) &&
	    tbl_info->rsubtype >= CFA_RSUBTYPE_IDX_TBL_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid idx tbl subtype: %d\n", __func__,
			   tbl_info->rsubtype);
		return -EINVAL;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		netdev_dbg(bp->dev, "%s: bp not PF or trusted VF\n", __func__);
		return -EINVAL;
	}

	return 0;
}

int tfc_idx_tbl_get(struct tfc *tfcp, u16 fid,
		    const struct tfc_idx_tbl_info *tbl_info,
		    u32 *data, u8 *data_sz_in_bytes)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (tfc_idx_tbl_get_check(tfcp, fid, tbl_info, data, data_sz_in_bytes))
		return -EINVAL;

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = tfc_msg_idx_tbl_get(tfcp, fid, sid, tbl_info->dir,
				 tbl_info->rsubtype, tbl_info->blktype,
				 tbl_info->id, data, data_sz_in_bytes);
	if (rc)
		netdev_dbg(bp->dev, "%s: hwrm failed: %s:%s %d %d\n", __func__,
			   tfc_dir_2_str(tbl_info->dir),
			   tfc_idx_tbl_2_str(tbl_info->rsubtype), tbl_info->id, rc);
	return rc;
}

static int tfc_idx_tbl_free_check(struct tfc *tfcp, u16 fid,
				  const struct tfc_idx_tbl_info *tbl_info)
{
	struct bnxt *bp = tfcp->bp;

	if (!bp)
		return -EINVAL;

	if (!tbl_info) {
		netdev_dbg(bp->dev, "%s: tbl_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (tbl_info->dir >= CFA_DIR_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid cfa dir: %d\n", __func__, tbl_info->dir);
		return -EINVAL;
	}

	if (BLKTYPE_IS_CFA(tbl_info->blktype) &&
	    tbl_info->rsubtype >= CFA_RSUBTYPE_IDX_TBL_MAX) {
		netdev_dbg(bp->dev, "%s: Invalid idx tbl subtype: %d\n", __func__,
			   tbl_info->rsubtype);
		return -EINVAL;
	}

	if (!BNXT_PF(bp) && !BNXT_VF_IS_TRUSTED(bp)) {
		netdev_dbg(bp->dev, "%s: bp not PF or trusted VF\n", __func__);
		return -EINVAL;
	}

	return 0;
}

int tfc_idx_tbl_free(struct tfc *tfcp, u16 fid,
		     const struct tfc_idx_tbl_info *tbl_info)
{
	struct bnxt *bp = tfcp->bp;
	u16 sid;
	int rc;

	if (tfc_idx_tbl_free_check(tfcp, fid, tbl_info))
		return -EINVAL;

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to retrieve SID, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = tfc_msg_idx_tbl_free(tfcp, fid, sid, tbl_info->dir,
				  tbl_info->rsubtype, tbl_info->blktype,
				  tbl_info->id);
	if (rc)
		netdev_dbg(bp->dev, "%s: hwrm failed: %s:%s %d %d\n", __func__,
			   tfc_dir_2_str(tbl_info->dir),
			   tfc_idx_tbl_2_str(tbl_info->rsubtype), tbl_info->id, rc);
	return rc;
}
