// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include "tfc.h"

#include "tfc_msg.h"
#include "cfa_types.h"
#include "tfo.h"
#include "bnxt_compat.h"
#include "bnxt.h"

int tfc_session_id_alloc(struct tfc *tfcp, u16 fid, u16 *sid)
{
	struct bnxt *bp = tfcp->bp;
	u16 current_sid;
	int rc;

	if (!sid) {
		netdev_dbg(bp->dev, "%s: Invalid sid pointer\n", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &current_sid);
	if (!rc) {
		netdev_dbg(bp->dev, "%s: Cannot allocate SID, current session is %u.\n", __func__,
			   current_sid);
		return -EBUSY;
	} else if (rc != -ENODATA) {
		netdev_dbg(bp->dev, "%s: Getting current sid failed, rc:%d.\n", __func__, -rc);
		return rc;
	}
	/* -ENODATA ==> current SID is invalid */

	rc = tfc_msg_session_id_alloc(tfcp, fid, sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: session id alloc message failed, rc:%d\n", __func__, -rc);
		return rc;
	}

	rc = tfo_sid_set(tfcp->tfo, *sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to store session id, rc:%d\n", __func__, -rc);
		return rc;
	}

	return rc;
}

int tfc_session_id_set(struct tfc *tfcp, u16 sid)
{
	u16 current_sid = INVALID_SID;
	struct bnxt *bp = tfcp->bp;
	int rc;

	rc = tfo_sid_get(tfcp->tfo, &current_sid);
	if (!rc) {
		/* SID is valid if rc == 0 */
		if (current_sid != sid) {
			netdev_dbg(bp->dev, "%s: Cannot update SID %u, current session is %u\n",
				   __func__, sid, current_sid);
			return -EBUSY;
		}
	} else if (rc != -ENODATA) {
		netdev_dbg(bp->dev, "%s: Getting current sid failed, rc:%d.\n", __func__, rc);
		return rc;
	}
	if (current_sid != sid) {
		rc = tfo_sid_set(tfcp->tfo, sid);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Failed to store session id, rc:%d\n", __func__,
				   rc);
			return rc;
		}
	}
	return rc;
}
int tfc_session_fid_add(struct tfc *tfcp, u16 fid, u16 sid,
			u16 *fid_cnt)
{
	u16 current_sid = INVALID_SID;
	struct bnxt *bp = NULL;
	int rc;

	if (!tfcp) {
		netdev_dbg(NULL, "%s: Invalid tfcp pointer\n", __func__);
		return -EINVAL;
	}

	bp = tfcp->bp;
	if (!fid_cnt) {
		netdev_dbg(bp->dev, "%s: Invalid fid_cnt pointer\n", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &current_sid);
	if (!rc) {
		/* SID is valid if rc == 0 */
		if (current_sid != sid) {
			netdev_dbg(bp->dev, "%s: Cannot add FID to SID %u, current session is %u\n",
				   __func__, sid, current_sid);
			return -EBUSY;
		}
	} else if (rc != -ENODATA) {
		netdev_dbg(bp->dev, "%s: Getting current sid failed, rc:%d.\n", __func__, rc);
		return rc;
	}
	/* -ENODATA ==> current SID is invalid */

	rc = tfc_msg_session_fid_add(tfcp, fid, sid, fid_cnt);
	if (rc) {
		netdev_dbg(bp->dev, "%s: session fid add message failed, rc:%d\n", __func__, rc);
		return rc;
	}

	if (current_sid != sid) {
		rc = tfo_sid_set(tfcp->tfo, sid);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Failed to store session id, rc:%d\n", __func__,
				   rc);
			return rc;
		}
	}

	return rc;
}

int tfc_session_fid_rem(struct tfc *tfcp, u16 fid, u16 *fid_cnt)
{
	struct bnxt *bp = NULL;
	u16 sid;
	int rc;

	if (!tfcp) {
		netdev_dbg(NULL, "%s: Invalid tfcp pointer\n", __func__);
		return -EINVAL;
	}

	bp = tfcp->bp;
	if (!fid_cnt) {
		netdev_dbg(bp->dev, "%s: Invalid fid_cnt pointer\n", __func__);
		return -EINVAL;
	}

	rc = tfo_sid_get(tfcp->tfo, &sid);
	if (rc) {
		netdev_dbg(bp->dev, "%s: no sid allocated, rc:%d\n", __func__, rc);
		return rc;
	}

	rc = tfc_msg_session_fid_rem(tfcp, fid, sid, fid_cnt);
	if (rc) {
		netdev_dbg(bp->dev, "%s: session fid rem message failed, rc:%d\n", __func__, rc);
		return rc;
	}

	if (bp->pf.fw_fid == fid) {
		rc = tfo_sid_set(tfcp->tfo, INVALID_SID);
		if (rc)
			netdev_dbg(bp->dev, "%s: Failed to reset session id, rc:%d\n",
				   __func__, rc);
	}

	return rc;
}
