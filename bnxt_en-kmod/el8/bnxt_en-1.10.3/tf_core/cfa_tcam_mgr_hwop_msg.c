// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2021-2022 Broadcom
 * All rights reserved.
 */

/* This file will "do the right thing" for each of the primitives set, get and
 * free.
 *
 * If TCAM manager is running stand-alone, the tables will be shadowed here.
 *
 * If TCAM manager is running in the core, the tables will also be shadowed.
 * Set and free messages will also be sent to the firmware.  Instead of sending
 * get messages, the entry will be read from the shadow copy thus saving a
 * firmware message.
 *
 * Support for running in firmware has not yet been added.
 */

#include "tf_tcam.h"
#include "hcapi_cfa_defs.h"
#include "cfa_tcam_mgr.h"
#include "cfa_tcam_mgr_device.h"
#include "cfa_tcam_mgr_hwop_msg.h"
#include "cfa_tcam_mgr_p58.h"
#include "cfa_tcam_mgr_p4.h"
#include "tf_session.h"
#include "tf_msg.h"
#include "tf_util.h"

int cfa_tcam_mgr_hwops_init(struct cfa_tcam_mgr_data *tcam_mgr_data,
			    enum cfa_tcam_mgr_device_type type)
{
	struct cfa_tcam_mgr_hwops_funcs *hwop_funcs =
			&tcam_mgr_data->hwop_funcs;

	switch (type) {
	case CFA_TCAM_MGR_DEVICE_TYPE_WH:
	case CFA_TCAM_MGR_DEVICE_TYPE_SR:
		return cfa_tcam_mgr_hwops_get_funcs_p4(hwop_funcs);
	case CFA_TCAM_MGR_DEVICE_TYPE_THOR:
		return cfa_tcam_mgr_hwops_get_funcs_p58(hwop_funcs);
	default:
		return -ENODEV;
	}
}

/* This is the glue between the TCAM manager and the firmware HW operations.
 * It is intended to abstract out the location of the TCAM manager so that
 * the TCAM manager code will be the same whether or not it is actually using
 * the firmware.
 *
 * There are three possibilities:
 * - TCAM manager is running in the core.
 *   These APIs will cause HW RM messages to be sent.
 *       CFA_TCAM_MGR_CORE
 * - TCAM manager is running in firmware.
 *   These APIs will call the HW ops functions.
 *
 * - TCAM manager is running standalone.
 *   These APIs will access all data in memory.
 *       CFA_TCAM_MGR_STANDALONE
 */
int cfa_tcam_mgr_entry_set_msg(struct cfa_tcam_mgr_data *tcam_mgr_data,
			       struct tf *tfp,
			       struct cfa_tcam_mgr_set_parms *parms,
			       int row, int slice, int max_slices)
{
	enum tf_tcam_tbl_type type =
		cfa_tcam_mgr_get_phys_table_type(parms->type);
	cfa_tcam_mgr_hwop_set_func_t set_func;
	struct tf_tcam_set_parms sparms;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	u8 fw_session_id;
	int rc;

	set_func = tcam_mgr_data->hwop_funcs.set;
	if (!set_func)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	memset(&sparms, 0, sizeof(sparms));
	sparms.dir	   = parms->dir;
	sparms.type	   = type;
	sparms.hcapi_type  = parms->hcapi_type;
	sparms.idx	   = (row * max_slices) + slice;
	sparms.key	   = parms->key;
	sparms.mask	   = parms->mask;
	sparms.key_size	   = parms->key_size;
	sparms.result	   = parms->result;
	sparms.result_size = parms->result_size;

	netdev_dbg(tfp->bp->dev,
		   "%s: %s row:%d slice:%d set tcam physical idx 0x%x\n",
		   tf_dir_2_str(parms->dir),
		   cfa_tcam_mgr_tbl_2_str(parms->type),
		   row, slice, sparms.idx);

	rc = tf_msg_tcam_entry_set(tfp, &sparms, fw_session_id);
	if (rc) {
		netdev_err(tfp->bp->dev,
			   "%s: %s entry:%d set tcam failed, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   cfa_tcam_mgr_tbl_2_str(parms->type),
			   parms->id, -rc);
		return rc;
	}

	return set_func(tcam_mgr_data, parms, row, slice, max_slices);
}

int cfa_tcam_mgr_entry_get_msg(struct cfa_tcam_mgr_data *tcam_mgr_data,
			       struct tf *tfp,
			       struct cfa_tcam_mgr_get_parms *parms,
			       int row, int slice, int max_slices)
{
	cfa_tcam_mgr_hwop_get_func_t get_func;

	get_func = tcam_mgr_data->hwop_funcs.get;
	if (!get_func)
		return -EINVAL;

	return get_func(tcam_mgr_data, parms, row, slice, max_slices);
}

int cfa_tcam_mgr_entry_free_msg(struct cfa_tcam_mgr_data *tcam_mgr_data,
				struct tf *tfp,
				struct cfa_tcam_mgr_free_parms *parms,
				int row, int slice, int key_size,
				int result_size, int max_slices)
{
	enum tf_tcam_tbl_type type =
		cfa_tcam_mgr_get_phys_table_type(parms->type);
	u8 mask[CFA_TCAM_MGR_MAX_KEY_SIZE] = { 0 };
	u8 key[CFA_TCAM_MGR_MAX_KEY_SIZE] = { 0 };
	cfa_tcam_mgr_hwop_free_func_t free_func;
	struct tf_tcam_set_parms sparms;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	u8 fw_session_id;
	int rc;

	free_func = tcam_mgr_data->hwop_funcs.free;
	if (!free_func)
		return -EINVAL;

	/* The free hwop will free more than a single slice (an entire row),
	 * so cannot be used. Use set message to clear an individual entry
	 */

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	if (key_size > CFA_TCAM_MGR_MAX_KEY_SIZE) {
		netdev_dbg(tfp->bp->dev,
			   "%s: %s entry:%d key size:%d > %d\n",
			   tf_dir_2_str(parms->dir),
			   cfa_tcam_mgr_tbl_2_str(parms->type),
			   parms->id, key_size, CFA_TCAM_MGR_MAX_KEY_SIZE);
		return -EINVAL;
	}

	if (result_size > CFA_TCAM_MGR_MAX_KEY_SIZE) {
		netdev_dbg(tfp->bp->dev,
			   "%s: %s entry:%d result size:%d > %d\n",
			   tf_dir_2_str(parms->dir),
			   cfa_tcam_mgr_tbl_2_str(parms->type),
			   parms->id, result_size, CFA_TCAM_MGR_MAX_KEY_SIZE);
		return -EINVAL;
	}

	memset(&sparms, 0, sizeof(sparms));
	memset(&key, 0, sizeof(key));
	memset(&mask, 0xff, sizeof(mask));

	sparms.dir	   = parms->dir;
	sparms.type	   = type;
	sparms.hcapi_type  = parms->hcapi_type;
	sparms.key	   = key;
	sparms.mask	   = mask;
	sparms.result	   = key;
	sparms.idx	   = (row * max_slices) + slice;
	sparms.key_size	   = key_size;
	sparms.result_size = result_size;

	netdev_dbg(tfp->bp->dev,
		   "%s: %s row:%d slice:%d free idx:%d key_sz:%d res_sz:%d\n",
		   tf_dir_2_str(parms->dir),
		   cfa_tcam_mgr_tbl_2_str(parms->type),
		   row, slice, sparms.idx, key_size, result_size);

	rc = tf_msg_tcam_entry_set(tfp, &sparms, fw_session_id);
	if (rc) {
		netdev_err(tfp->bp->dev,
			   "%s: %s row:%d slice:%d set tcam failed, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   cfa_tcam_mgr_tbl_2_str(parms->type),
			   row, slice, rc);
		return rc;
	}

	return free_func(tcam_mgr_data, parms, row, slice, max_slices);
}
