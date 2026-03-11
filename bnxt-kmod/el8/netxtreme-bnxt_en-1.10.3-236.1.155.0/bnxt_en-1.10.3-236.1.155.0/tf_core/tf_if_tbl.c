// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include "tf_if_tbl.h"
#include "tf_rm.h"
#include "tf_util.h"
#include "tf_msg.h"
#include "tf_session.h"

struct tf;

/* IF Table database */
struct tf_if_tbl_db {
	struct tf_if_tbl_cfg *if_tbl_cfg_db[TF_DIR_MAX];
};

/**
 * Convert if_tbl_type to hwrm type.
 *
 * @if_tbl_type:	Interface table type
 * @hwrm_type:		HWRM device data type
 *
 * Returns:
 *    0          - Success
 *   -EOPNOTSUPP - Type not supported
 */
static int tf_if_tbl_get_hcapi_type(struct tf_if_tbl_get_hcapi_parms *parms)
{
	enum tf_if_tbl_cfg_type cfg_type;
	struct tf_if_tbl_cfg *tbl_cfg;

	tbl_cfg = (struct tf_if_tbl_cfg *)parms->tbl_db;
	cfg_type = tbl_cfg[parms->db_index].cfg_type;

	if (cfg_type != TF_IF_TBL_CFG)
		return -EOPNOTSUPP;

	*parms->hcapi_type = tbl_cfg[parms->db_index].hcapi_type;

	return 0;
}

int tf_if_tbl_bind(struct tf *tfp, struct tf_if_tbl_cfg_parms *parms)
{
	struct tf_if_tbl_db *if_tbl_db;

	if (!tfp || !parms)
		return -EINVAL;

	if_tbl_db = vzalloc(sizeof(*if_tbl_db));
	if (!if_tbl_db)
		return -ENOMEM;

	if_tbl_db->if_tbl_cfg_db[TF_DIR_RX] = parms->cfg;
	if_tbl_db->if_tbl_cfg_db[TF_DIR_TX] = parms->cfg;
	tf_session_set_if_tbl_db(tfp, (void *)if_tbl_db);

	netdev_dbg(tfp->bp->dev, "Table Type - initialized\n");
	return 0;
}

int tf_if_tbl_unbind(struct tf *tfp)
{
	struct tf_if_tbl_db *if_tbl_db_ptr;
	int rc;

	rc = tf_session_get_if_tbl_db(tfp, (void **)&if_tbl_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "No IF Table DBs initialized\n");
		return 0;
	}

	tf_session_set_if_tbl_db(tfp, NULL);
	vfree(if_tbl_db_ptr);

	return 0;
}

int tf_if_tbl_set(struct tf *tfp, struct tf_if_tbl_set_parms *parms)
{
	struct tf_if_tbl_get_hcapi_parms hparms;
	struct tf_if_tbl_db *if_tbl_db_ptr;
	u8 fw_session_id;
	int rc;

	if (!tfp || !parms || !parms->data)
		return -EINVAL;

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	rc = tf_session_get_if_tbl_db(tfp, (void **)&if_tbl_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "No IF Table DBs initialized\n");
		return 0;
	}

	/* Convert TF type to HCAPI type */
	hparms.tbl_db = if_tbl_db_ptr->if_tbl_cfg_db[parms->dir];
	hparms.db_index = parms->type;
	hparms.hcapi_type = &parms->hcapi_type;
	rc = tf_if_tbl_get_hcapi_type(&hparms);
	if (rc)
		return rc;

	rc = tf_msg_set_if_tbl_entry(tfp, parms, fw_session_id);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s, If Tbl set failed, type:%d, rc:%d\n",
			   tf_dir_2_str(parms->dir), parms->type, rc);
	}

	return 0;
}

int tf_if_tbl_get(struct tf *tfp, struct tf_if_tbl_get_parms *parms)
{
	struct tf_if_tbl_get_hcapi_parms hparms;
	struct tf_if_tbl_db *if_tbl_db_ptr;
	u8 fw_session_id;
	int rc = 0;

	if (!tfp || !parms || !parms->data)
		return -EINVAL;

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	rc = tf_session_get_if_tbl_db(tfp, (void **)&if_tbl_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "No IF Table DBs initialized\n");
		return 0;
	}

	/* Convert TF type to HCAPI type */
	hparms.tbl_db = if_tbl_db_ptr->if_tbl_cfg_db[parms->dir];
	hparms.db_index = parms->type;
	hparms.hcapi_type = &parms->hcapi_type;
	rc = tf_if_tbl_get_hcapi_type(&hparms);
	if (rc)
		return rc;

	/* Get the entry */
	rc = tf_msg_get_if_tbl_entry(tfp, parms, fw_session_id);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s, If Tbl get failed, type:%d, rc:%d\n",
			   tf_dir_2_str(parms->dir), parms->type, -rc);
	}

	return 0;
}
