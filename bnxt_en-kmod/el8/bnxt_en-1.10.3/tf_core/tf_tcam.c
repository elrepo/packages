// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include <linux/vmalloc.h>
#include "tf_tcam.h"
#include "tf_util.h"
#include "tf_rm.h"
#include "tf_device.h"
#include "tf_session.h"
#include "tf_msg.h"
#include "tf_tcam_mgr_msg.h"

struct tf;

int tf_tcam_bind(struct tf *tfp, struct tf_tcam_cfg_parms *parms)
{
	struct tf_resource_info resv_res[TF_DIR_MAX][TF_TCAM_TBL_TYPE_MAX];
	struct tf_tcam_resources local_tcam_cnt[TF_DIR_MAX];
	struct tf_rm_get_alloc_info_parms ainfo;
	struct tf_rm_create_db_parms db_cfg = { 0 };
	struct tf_tcam_resources *tcam_cnt;
	struct tf_rm_free_db_parms fparms;
	int db_rc[TF_DIR_MAX] = { 0 };
	struct tf_rm_alloc_info info;
	struct tcam_rm_db *tcam_db;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	u16 num_slices = 1;
	bool no_req = true;
	u32 rx_supported;
	u32 tx_supported;
	int d, t;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	if (!dev->ops->tf_dev_get_tcam_slice_info) {
		rc = -EOPNOTSUPP;
		netdev_dbg(tfp->bp->dev, "Operation not supported, rc:%d\n",
			   rc);
		return rc;
	}

	tcam_cnt = parms->resources->tcam_cnt;

	for (d = 0; d < TF_DIR_MAX; d++) {
		for (t = 0; t < TF_TCAM_TBL_TYPE_MAX; t++) {
			rc = dev->ops->tf_dev_get_tcam_slice_info(tfp, t, 0,
								  &num_slices);
			if (rc)
				return rc;

			if (num_slices == 1)
				continue;

			if (tcam_cnt[d].cnt[t] % num_slices) {
				netdev_err(NULL,
					   "%s: Requested num of %s entries has to be multiple of %d\n",
					   tf_dir_2_str(d),
					   tf_tcam_tbl_2_str(t), num_slices);
				return -EINVAL;
			}
		}
	}

	tcam_db = vzalloc(sizeof(*tcam_db));
	if (!tcam_db)
		return -ENOMEM;

	for (d = 0; d < TF_DIR_MAX; d++)
		tcam_db->tcam_db[d] = NULL;

	tf_session_set_db(tfp, TF_MODULE_TYPE_TCAM, tcam_db);

	db_cfg.module = TF_MODULE_TYPE_TCAM;
	db_cfg.num_elements = parms->num_elements;
	db_cfg.cfg = parms->cfg;

	for (d = 0; d < TF_DIR_MAX; d++) {
		db_cfg.dir = d;
		db_cfg.alloc_cnt = tcam_cnt[d].cnt;
		db_cfg.rm_db = (void *)&tcam_db->tcam_db[d];
		if (tf_session_is_shared_session(tfs) &&
		    (!tf_session_is_shared_session_creator(tfs)))
			db_rc[d] = tf_rm_create_db_no_reservation(tfp, &db_cfg);
		else
			db_rc[d] = tf_rm_create_db(tfp, &db_cfg);

		if (db_rc[d]) {
			netdev_dbg(tfp->bp->dev, "%s: no TCAM DB required\n",
				   tf_dir_2_str(d));
		}
	}

	/* No db created */
	if (db_rc[TF_DIR_RX] && db_rc[TF_DIR_TX]) {
		netdev_dbg(tfp->bp->dev, "No TCAM DB created\n");
		tf_session_set_db(tfp, TF_MODULE_TYPE_TCAM, NULL);
		vfree(tcam_db);
		return db_rc[TF_DIR_RX];
	}

	/* Collect info on which entries were reserved. */
	for (d = 0; d < TF_DIR_MAX; d++) {
		for (t = 0; t < TF_TCAM_TBL_TYPE_MAX; t++) {
			memset(&info, 0, sizeof(info));
			if (tcam_cnt[d].cnt[t] == 0) {
				resv_res[d][t].start  = 0;
				resv_res[d][t].stride = 0;
				continue;
			}
			ainfo.rm_db = tcam_db->tcam_db[d];
			ainfo.subtype = t;
			ainfo.info = &info;
			rc = tf_rm_get_info(&ainfo);
			if (rc)
				goto error;

			rc = dev->ops->tf_dev_get_tcam_slice_info(tfp, t, 0,
								  &num_slices);
			if (rc)
				return rc;

			if (num_slices > 1) {
				/* check if reserved resource for is multiple of
				 * num_slices
				 */
				if (info.entry.start % num_slices != 0 ||
				    info.entry.stride % num_slices != 0) {
					netdev_err(tfp->bp->dev,
						   "%s: %s reserved resource is not multiple of %d\n",
						   tf_dir_2_str(d),
						   tf_tcam_tbl_2_str(t),
						   num_slices);
					rc = -EINVAL;
					goto error;
				}
			}

			resv_res[d][t].start  = info.entry.start;
			resv_res[d][t].stride = info.entry.stride;
		}
	}

	rc = tf_tcam_mgr_bind_msg(tfp, dev, parms, resv_res);
	if (rc)
		return rc;

	rc = tf_tcam_mgr_qcaps_msg(tfp, dev,
				   &rx_supported, &tx_supported);
	if (rc)
		return rc;

	for (t = 0; t < TF_TCAM_TBL_TYPE_MAX; t++) {
		if (rx_supported & 1 << t)
			tfs->tcam_mgr_control[TF_DIR_RX][t] = 1;
		if (tx_supported & 1 << t)
			tfs->tcam_mgr_control[TF_DIR_TX][t] = 1;
	}

	/* Make a local copy of tcam_cnt with only resources not managed by
	 * TCAM Manager requested.
	 */
	memcpy(&local_tcam_cnt, tcam_cnt, sizeof(local_tcam_cnt));
	tcam_cnt = local_tcam_cnt;
	for (d = 0; d < TF_DIR_MAX; d++) {
		for (t = 0; t < TF_TCAM_TBL_TYPE_MAX; t++) {
			/* If controlled by TCAM Manager */
			if (tfs->tcam_mgr_control[d][t])
				tcam_cnt[d].cnt[t] = 0;
			else if (tcam_cnt[d].cnt[t] > 0)
				no_req = false;
		}
	}

	/* If no resources left to request */
	if (no_req)
		goto finished;

finished:
	netdev_dbg(tfp->bp->dev, "TCAM - initialized\n");
	return 0;

error:
	for (d = 0; d < TF_DIR_MAX; d++) {
		if (tcam_db->tcam_db[d]) {
			memset(&fparms, 0, sizeof(fparms));
			fparms.dir = d;
			fparms.rm_db = tcam_db->tcam_db[d];
			/* Ignoring return here as we are in the error case */
			(void)tf_rm_free_db(tfp, &fparms);
			tcam_db->tcam_db[d] = NULL;
		}

		tcam_db->tcam_db[d] = NULL;
		tf_session_set_db(tfp, TF_MODULE_TYPE_TCAM, NULL);
	}
	vfree(tcam_db);

	return rc;
}

int tf_tcam_unbind(struct tf *tfp)
{
	struct tf_rm_free_db_parms fparms = { 0 };
	struct tcam_rm_db *tcam_db;
	void *tcam_db_ptr = NULL;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;
	int i;

	if (!tfp)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "Tcam_db is not initialized\n");
		return 0;
	}
	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	for (i = 0; i < TF_DIR_MAX; i++) {
		if (!tcam_db->tcam_db[i])
			continue;
		memset(&fparms, 0, sizeof(fparms));
		fparms.dir = i;
		fparms.rm_db = tcam_db->tcam_db[i];
		rc = tf_rm_free_db(tfp, &fparms);
		if (rc)
			return rc;

		tcam_db->tcam_db[i] = NULL;
	}

	/* free TCAM database pointer */
	tf_session_set_db(tfp, TF_MODULE_TYPE_TCAM, NULL);
	vfree(tcam_db);

	rc = tf_tcam_mgr_unbind_msg(tfp, dev);
	if (rc)
		return rc;
	return 0;
}

int tf_tcam_alloc(struct tf *tfp, struct tf_tcam_alloc_parms *parms)
{
	struct tf_rm_allocate_parms aparms = { 0 };
	struct tcam_rm_db *tcam_db;
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	void *tcam_db_ptr = NULL;
	u16 num_slices = 1;
	int rc, i;
	u32 index;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	if (!dev->ops->tf_dev_get_tcam_slice_info) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Need to retrieve number of slices based on the key_size */
	rc = dev->ops->tf_dev_get_tcam_slice_info(tfp,
						  parms->type,
						  parms->key_size,
						  &num_slices);
	if (rc)
		return rc;

	/* If TCAM controlled by TCAM Manager */
	if (tfs->tcam_mgr_control[parms->dir][parms->type])
		return tf_tcam_mgr_alloc_msg(tfp, dev, parms);
	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to get tcam_db from session, rc:%d\n", rc);
		return rc;
	}
	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	/* For WC TCAM, number of slices could be 4, 2, 1 based on
	 * the key_size. For other TCAM, it is always 1
	 */
	for (i = 0; i < num_slices; i++) {
		aparms.rm_db = tcam_db->tcam_db[parms->dir];
		aparms.subtype = parms->type;
		aparms.priority = parms->priority;
		aparms.index = &index;
		rc = tf_rm_allocate(&aparms);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Failed tcam, type:%d\n",
				   tf_dir_2_str(parms->dir), parms->type);
			return rc;
		}

		/* return the start index of each row */
		if (i == 0)
			parms->idx = index;
	}

	return 0;
}

int tf_tcam_free(struct tf *tfp, struct tf_tcam_free_parms *parms)
{
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	struct tf_rm_free_parms fparms = { 0 };
	struct tcam_rm_db *tcam_db;
	void *tcam_db_ptr = NULL;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	u16 num_slices = 1;
	int allocated = 0;
	u8 fw_session_id;
	struct bnxt *bp;
	int rc;
	int i;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

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

	if (!dev->ops->tf_dev_get_tcam_slice_info) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Need to retrieve row size etc */
	rc = dev->ops->tf_dev_get_tcam_slice_info(tfp,
						  parms->type,
						  0,
						  &num_slices);
	if (rc)
		return rc;

	/* If TCAM controlled by TCAM Manager */
	if (tfs->tcam_mgr_control[parms->dir][parms->type]) {
		/* If a session can have multiple references to an entry, check
		 * the reference count here before actually freeing the entry.
		 */
		parms->ref_cnt = 0;
		return tf_tcam_mgr_free_msg(tfp, dev, parms);
	}

	if (parms->idx % num_slices) {
		netdev_dbg(bp->dev,
			   "%s: TCAM reserved resource not multiple of %d\n",
			   tf_dir_2_str(parms->dir), num_slices);
		return -EINVAL;
	}

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to get em_ext_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	/* Check if element is in use */
	aparms.rm_db = tcam_db->tcam_db[parms->dir];
	aparms.subtype = parms->type;
	aparms.index = parms->idx;
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		netdev_dbg(bp->dev,
			   "%s: Entry already free, type:%d, index:%d\n",
			   tf_dir_2_str(parms->dir), parms->type, parms->idx);
		return -EINVAL;
	}

	for (i = 0; i < num_slices; i++) {
		/* Free requested element */
		fparms.rm_db = tcam_db->tcam_db[parms->dir];
		fparms.subtype = parms->type;
		fparms.index = parms->idx + i;
		rc = tf_rm_free(&fparms);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: Free failed, type:%d, index:%d\n",
				   tf_dir_2_str(parms->dir), parms->type,
				   parms->idx);
			return rc;
		}
	}

	/* Convert TF type to HCAPI RM type */
	hparms.rm_db = tcam_db->tcam_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &parms->hcapi_type;

	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc)
		return rc;

	rc = tf_msg_tcam_entry_free(tfp, parms, fw_session_id);
	if (rc) {
		/* Log error */
		netdev_dbg(bp->dev, "%s: %s: Entry %d free failed, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tcam_tbl_2_str(parms->type), parms->idx, rc);
		return rc;
	}

	return 0;
}

int tf_tcam_alloc_search(struct tf *tfp,
			 struct tf_tcam_alloc_search_parms *parms)
{
	struct tf_tcam_alloc_parms aparms = { 0 };
	u16 num_slice_per_row = 1;
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	if (!dev->ops->tf_dev_get_tcam_slice_info) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Need to retrieve row size etc */
	rc = dev->ops->tf_dev_get_tcam_slice_info(tfp, parms->type,
						  parms->key_size,
						  &num_slice_per_row);
	if (rc)
		return rc;

	/* If TCAM controlled by TCAM Manager */
	if (tfs->tcam_mgr_control[parms->dir][parms->type]) {
		/* If a session can have multiple references to an entry,
		 * search the session's entries first. If the caller
		 * requested an alloc and a match was found, update the
		 * ref_cnt before returning.
		 */
		return -EINVAL;
	}

	/* The app didn't request us to alloc the entry, so return now.
	 * The hit should have been updated in the original search parm.
	 */
	if (!parms->alloc || parms->search_status != MISS)
		return rc;

	/* Caller desires an allocate on miss */
	if (!dev->ops->tf_dev_alloc_tcam) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}
	aparms.dir = parms->dir;
	aparms.type = parms->type;
	aparms.key_size = parms->key_size;
	aparms.priority = parms->priority;
	rc = dev->ops->tf_dev_alloc_tcam(tfp, &aparms);
	if (rc)
		return rc;

	/* Add the allocated index to output and done */
	parms->idx = aparms.idx;

	return 0;
}

int tf_tcam_set(struct tf *tfp, struct tf_tcam_set_parms *parms)
{
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	struct tcam_rm_db *tcam_db;
	u16 num_slice_per_row = 1;
	void *tcam_db_ptr = NULL;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	u8 fw_session_id;
	int allocated = 0;
	struct bnxt *bp;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

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

	if (!dev->ops->tf_dev_get_tcam_slice_info) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Need to retrieve row size etc */
	rc = dev->ops->tf_dev_get_tcam_slice_info(tfp,
						  parms->type,
						  parms->key_size,
						  &num_slice_per_row);
	if (rc)
		return rc;

	/* If TCAM controlled by TCAM Manager */
	if (tfs->tcam_mgr_control[parms->dir][parms->type])
		return tf_tcam_mgr_set_msg(tfp, dev, parms);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to get em_ext_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	/* Check if element is in use */
	aparms.rm_db = tcam_db->tcam_db[parms->dir];
	aparms.subtype = parms->type;
	aparms.index = parms->idx;
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		netdev_dbg(bp->dev,
			   "%s: Entry is not allocated, type:%d, index:%d\n",
			   tf_dir_2_str(parms->dir), parms->type, parms->idx);
		return -EINVAL;
	}

	/* Convert TF type to HCAPI RM type */
	hparms.rm_db = tcam_db->tcam_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &parms->hcapi_type;

	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc)
		return rc;

	rc = tf_msg_tcam_entry_set(tfp, parms, fw_session_id);
	if (rc) {
		/* Log error */
		netdev_dbg(bp->dev, "%s: %s: Entry %d set failed, rc:%d",
			   tf_dir_2_str(parms->dir),
			   tf_tcam_tbl_2_str(parms->type), parms->idx, rc);
		return rc;
	}

	return 0;
}

int tf_tcam_get(struct tf *tfp, struct tf_tcam_get_parms *parms)
{
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	struct tcam_rm_db *tcam_db;
	void *tcam_db_ptr = NULL;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int allocated = 0;
	u8 fw_session_id;
	struct bnxt *bp;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

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

	/* If TCAM controlled by TCAM Manager */
	if (tfs->tcam_mgr_control[parms->dir][parms->type])
		return tf_tcam_mgr_get_msg(tfp, dev, parms);

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to get em_ext_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	/* Check if element is in use */
	aparms.rm_db = tcam_db->tcam_db[parms->dir];
	aparms.subtype = parms->type;
	aparms.index = parms->idx;
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		netdev_dbg(bp->dev,
			   "%s: Entry is not allocated, type:%d, index:%d\n",
			   tf_dir_2_str(parms->dir), parms->type, parms->idx);
		return -EINVAL;
	}

	/* Convert TF type to HCAPI RM type */
	hparms.rm_db = tcam_db->tcam_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &parms->hcapi_type;

	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc)
		return rc;

	rc = tf_msg_tcam_entry_get(tfp, parms, fw_session_id);
	if (rc) {
		/* Log error */
		netdev_dbg(bp->dev, "%s: %s: Entry %d set failed, rc:%d",
			   tf_dir_2_str(parms->dir),
			   tf_tcam_tbl_2_str(parms->type), parms->idx, rc);
		return rc;
	}

	return 0;
}

int tf_tcam_get_resc_info(struct tf *tfp, struct tf_tcam_resource_info *tcam)
{
	struct tf_rm_get_alloc_info_parms ainfo;
	struct tf_resource_info *dinfo;
	struct tcam_rm_db *tcam_db;
	void *tcam_db_ptr = NULL;
	int rc;
	int d;

	if (!tfp || !tcam)
		return -EINVAL;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TCAM, &tcam_db_ptr);
	if (rc == -ENOMEM)
		return 0;  /* db doesn't exist */
	else if (rc)
		return rc; /* error getting db */

	tcam_db = (struct tcam_rm_db *)tcam_db_ptr;

	/* check if reserved resource for WC is multiple of num_slices */
	for (d = 0; d < TF_DIR_MAX; d++) {
		ainfo.rm_db = tcam_db->tcam_db[d];

		if (!ainfo.rm_db)
			continue;

		dinfo = tcam[d].info;

		ainfo.info = (struct tf_rm_alloc_info *)dinfo;
		ainfo.subtype = 0;
		rc = tf_rm_get_all_info(&ainfo, TF_TCAM_TBL_TYPE_MAX);
		if (rc && rc != -EOPNOTSUPP)
			return rc;
	}

	return 0;
}
