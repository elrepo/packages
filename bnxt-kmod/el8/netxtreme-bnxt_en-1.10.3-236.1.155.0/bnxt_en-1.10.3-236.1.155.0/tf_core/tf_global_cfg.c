// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include "tf_global_cfg.h"
#include "tf_util.h"
#include "tf_msg.h"
#include "tf_session.h"

struct tf;

/* Global cfg database */
struct tf_global_cfg_db {
	struct tf_global_cfg_cfg *global_cfg_db[TF_DIR_MAX];
};

/* Get HCAPI type parameters for a single element
 *
 * @global_cfg_db:	Global Cfg DB Handle
 * @db_index:		DB Index, indicates which DB entry to perform the
 *			action on.
 * @hcapi_type:		Pointer to the hcapi type for the specified db_index
 */
struct tf_global_cfg_get_hcapi_parms {
	void *global_cfg_db;
	u16 db_index;
	u16 *hcapi_type;
};

/**
 * Check global_cfg_type and return hwrm type.
 *
 * @global_cfg_type:	Global Cfg type
 * @hwrm_type:		HWRM device data type
 *
 * Returns:
 *    0          - Success
 *   -EOPNOTSUPP - Type not supported
 */
static int tf_global_cfg_get_hcapi_type(struct tf_global_cfg_get_hcapi_parms
					*parms)
{
	struct tf_global_cfg_cfg *global_cfg;
	enum tf_global_cfg_cfg_type cfg_type;

	global_cfg = (struct tf_global_cfg_cfg *)parms->global_cfg_db;
	cfg_type = global_cfg[parms->db_index].cfg_type;

	if (cfg_type != TF_GLOBAL_CFG_CFG_HCAPI)
		return -EOPNOTSUPP;

	*parms->hcapi_type = global_cfg[parms->db_index].hcapi_type;

	return 0;
}

int tf_global_cfg_bind(struct tf *tfp, struct tf_global_cfg_cfg_parms *parms)
{
	struct tf_global_cfg_db *global_cfg_db;

	if (!tfp || !parms)
		return -EINVAL;

	global_cfg_db = vzalloc(sizeof(*global_cfg_db));
	if (!global_cfg_db)
		return -ENOMEM;

	global_cfg_db->global_cfg_db[TF_DIR_RX] = parms->cfg;
	global_cfg_db->global_cfg_db[TF_DIR_TX] = parms->cfg;
	tf_session_set_global_db(tfp, (void *)global_cfg_db);

	netdev_dbg(tfp->bp->dev, "Global Cfg - initialized\n");
	return 0;
}

int tf_global_cfg_unbind(struct tf *tfp)
{
	struct tf_global_cfg_db *global_cfg_db_ptr;
	int rc;

	if (!tfp)
		return -EINVAL;

	rc = tf_session_get_global_db(tfp, (void **)&global_cfg_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "global_cfg_db is not initialized\n");
		return 0;
	}

	tf_session_set_global_db(tfp, NULL);
	vfree(global_cfg_db_ptr);

	return 0;
}

int tf_global_cfg_set(struct tf *tfp, struct tf_global_cfg_parms *parms)
{
	struct tf_global_cfg_get_hcapi_parms hparms;
	struct tf_global_cfg_db *global_cfg_db_ptr;
	u8 fw_session_id;
	u16 hcapi_type;
	int rc;

	if (!tfp || !parms || !parms->config)
		return -EINVAL;

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	rc = tf_session_get_global_db(tfp, (void **)&global_cfg_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "No global cfg DBs initialized\n");
		return 0;
	}

	/* Convert TF type to HCAPI type */
	hparms.global_cfg_db = global_cfg_db_ptr->global_cfg_db[parms->dir];
	hparms.db_index = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_global_cfg_get_hcapi_type(&hparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s, Failed type lookup, type:%d, rc:%d\n",
			   tf_dir_2_str(parms->dir), parms->type, rc);
		return rc;
	}

	rc = tf_msg_set_global_cfg(tfp, parms, fw_session_id);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "%s, Set failed, type:%d, rc:%d\n",
			   tf_dir_2_str(parms->dir), parms->type, -rc);
	}

	return 0;
}

int tf_global_cfg_get(struct tf *tfp, struct tf_global_cfg_parms *parms)
{
	struct tf_global_cfg_get_hcapi_parms hparms;
	struct tf_global_cfg_db *global_cfg_db_ptr;
	u8 fw_session_id;
	u16 hcapi_type;
	int rc;

	if (!tfp || !parms || !parms->config)
		return -EINVAL;

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	rc = tf_session_get_global_db(tfp, (void **)&global_cfg_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "No Global cfg DBs initialized\n");
		return 0;
	}

	hparms.global_cfg_db = global_cfg_db_ptr->global_cfg_db[parms->dir];
	hparms.db_index = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_global_cfg_get_hcapi_type(&hparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s, Failed type lookup, type:%d, rc:%d\n",
			   tf_dir_2_str(parms->dir), parms->type, rc);
		return rc;
	}

	/* Get the entry */
	rc = tf_msg_get_global_cfg(tfp, parms, fw_session_id);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "%s, Get failed, type:%d, rc:%d\n",
			   tf_dir_2_str(parms->dir), parms->type, rc);
	}

	return 0;
}
