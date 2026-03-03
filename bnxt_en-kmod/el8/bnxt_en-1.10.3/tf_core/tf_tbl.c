// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

/* Truflow Table APIs and supporting code */

#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include "tf_tbl.h"
#include "tf_rm.h"
#include "tf_util.h"
#include "tf_msg.h"
#include "tf_session.h"
#include "tf_device.h"

struct tf;

#define TF_TBL_RM_TO_PTR(new_idx, idx, base, shift) {		\
		*(new_idx) = (((idx) + (base)) << (shift));	\
}

int tf_tbl_bind(struct tf *tfp, struct tf_tbl_cfg_parms *parms)
{
	struct tf_rm_create_db_parms db_cfg = { 0 };
	int db_rc[TF_DIR_MAX] = { 0 };
	struct tbl_rm_db *tbl_db;
	struct tf_session *tfs;
	int rc, d, i;

	if (!tfp || !parms)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tbl_db = vzalloc(sizeof(*tbl_db));
	if (!tbl_db)
		return -ENOMEM;

	for (i = 0; i < TF_DIR_MAX; i++)
		tbl_db->tbl_db[i] = NULL;
	tf_session_set_db(tfp, TF_MODULE_TYPE_TABLE, tbl_db);

	db_cfg.num_elements = parms->num_elements;
	db_cfg.module = TF_MODULE_TYPE_TABLE;
	db_cfg.num_elements = parms->num_elements;

	for (d = 0; d < TF_DIR_MAX; d++) {
		db_cfg.dir = d;
		db_cfg.cfg = &parms->cfg[d ? TF_TBL_TYPE_MAX : 0];
		db_cfg.alloc_cnt = parms->resources->tbl_cnt[d].cnt;
		db_cfg.rm_db = (void *)&tbl_db->tbl_db[d];
		if (tf_session_is_shared_session(tfs) &&
		    (!tf_session_is_shared_session_creator(tfs)))
			db_rc[d] = tf_rm_create_db_no_reservation(tfp,
								  &db_cfg);
		else
			db_rc[d] = tf_rm_create_db(tfp, &db_cfg);

		if (db_rc[d]) {
			netdev_dbg(tfp->bp->dev,
				   "%s: No Table DB creation required\n",
				   tf_dir_2_str(d));
		}
	}

	/* No db created */
	if (db_rc[TF_DIR_RX] && db_rc[TF_DIR_TX]) {
		netdev_dbg(tfp->bp->dev, "%s: No Table DB created\n",
			   tf_dir_2_str(d));
		return db_rc[TF_DIR_RX];
	}

	netdev_dbg(tfp->bp->dev, "Table Type - initialized\n");
	return 0;
}

int tf_tbl_unbind(struct tf *tfp)
{
	struct tf_rm_free_db_parms fparms = { 0 };
	struct tbl_rm_db *tbl_db;
	void *tbl_db_ptr = NULL;
	int rc;
	int i;

	if (!tfp)
		return -EINVAL;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "Tbl_db is not initialized\n");
		return 0;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	for (i = 0; i < TF_DIR_MAX; i++) {
		if (!tbl_db->tbl_db[i])
			continue;
		fparms.dir = i;
		fparms.rm_db = tbl_db->tbl_db[i];
		rc = tf_rm_free_db(tfp, &fparms);
		if (rc)
			return rc;

		tbl_db->tbl_db[i] = NULL;
	}
	tf_session_set_db(tfp, TF_MODULE_TYPE_TABLE, NULL);
	vfree(tbl_db);
	return 0;
}

int tf_tbl_alloc(struct tf *tfp, struct tf_tbl_alloc_parms *parms)
{
	struct tf_rm_allocate_parms aparms = { 0 };
	struct bnxt *bp;
	struct tbl_rm_db *tbl_db;
	struct tf_dev_info *dev;
	void *tbl_db_ptr = NULL;
	struct tf_session *tfs;
	u32 idx;
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

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to get tbl_db from session, rc:%d\n", rc);
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	/* Allocate requested element */
	aparms.rm_db = tbl_db->tbl_db[parms->dir];
	aparms.subtype = parms->type;
	aparms.index = &idx;
	rc = tf_rm_allocate(&aparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed allocate, type:%s\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type));
		return rc;
	}

	*parms->idx = idx;

	return 0;
}

int tf_tbl_free(struct tf *tfp, struct tf_tbl_free_parms *parms)
{
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_free_parms fparms = { 0 };
	struct tbl_rm_db *tbl_db;
	struct tf_dev_info *dev;
	void *tbl_db_ptr = NULL;
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

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to get em_ext_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	/* Check if element is in use */
	aparms.rm_db = tbl_db->tbl_db[parms->dir];
	aparms.subtype = parms->type;
	aparms.index = parms->idx;
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		netdev_dbg(bp->dev,
			   "%s: Entry already free, type:%s, index:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), parms->idx);
		return -EINVAL;
	}

	/* If this is counter table, clear the entry on free */
	if (parms->type == TF_TBL_TYPE_ACT_STATS_64) {
		u8 data[8] = { 0 };
		u16 hcapi_type = 0;
		struct tf_rm_get_hcapi_parms hparms = { 0 };

		/* Get the hcapi type */
		hparms.rm_db = tbl_db->tbl_db[parms->dir];
		hparms.subtype = parms->type;
		hparms.hcapi_type = &hcapi_type;
		rc = tf_rm_get_hcapi_type(&hparms);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s, Failed type lookup, type:%s, rc:%d\n",
				   tf_dir_2_str(parms->dir),
				   tf_tbl_type_2_str(parms->type), rc);
			return rc;
		}
		/* Clear the counter
		 */
		rc = tf_msg_set_tbl_entry(tfp, parms->dir, hcapi_type,
					  sizeof(data), data, parms->idx,
					  fw_session_id);
		if (rc) {
			netdev_dbg(bp->dev, "%s, Set failed, type:%s, rc:%d\n",
				   tf_dir_2_str(parms->dir),
				   tf_tbl_type_2_str(parms->type), rc);
			return rc;
		}
	}

	/* Free requested element */
	fparms.rm_db = tbl_db->tbl_db[parms->dir];
	fparms.subtype = parms->type;
	fparms.index = parms->idx;
	rc = tf_rm_free(&fparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Free failed, type:%s, index:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), parms->idx);
		return rc;
	}

	return 0;
}

int tf_tbl_set(struct tf *tfp, struct tf_tbl_set_parms *parms)
{
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	struct tbl_rm_db *tbl_db;
	struct tf_dev_info *dev;
	void *tbl_db_ptr = NULL;
	struct tf_session *tfs;
	int allocated = 0;
	u8 fw_session_id;
	struct bnxt *bp;
	u16 hcapi_type;
	int rc;

	if (!tfp || !parms || !parms->data)
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

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to get em_ext_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	/* Verify that the entry has been previously allocated.
	 * for meter drop counter, check the corresponding meter
	 * entry
	 */
	aparms.rm_db = tbl_db->tbl_db[parms->dir];
	if (parms->type != TF_TBL_TYPE_METER_DROP_CNT)
		aparms.subtype = parms->type;
	else
		aparms.subtype = TF_TBL_TYPE_METER_INST;
	aparms.allocated = &allocated;
	aparms.index = parms->idx;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		netdev_dbg(tfp->bp->dev,
			   "%s, Invalid index, type:%s, idx:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), parms->idx);
		return -EINVAL;
	}

	/* Set the entry */
	hparms.rm_db = tbl_db->tbl_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s, Failed type lookup, type:%s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), rc);
		return rc;
	}

	rc = tf_msg_set_tbl_entry(tfp, parms->dir, hcapi_type,
				  parms->data_sz_in_bytes, parms->data,
				  parms->idx, fw_session_id);
	if (rc) {
		netdev_dbg(bp->dev, "%s, Set failed, type:%s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), rc);
		return rc;
	}

	return 0;
}

int tf_tbl_get(struct tf *tfp, struct tf_tbl_get_parms *parms)
{
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	struct tbl_rm_db *tbl_db;
	struct tf_dev_info *dev;
	void *tbl_db_ptr = NULL;
	struct tf_session *tfs;
	int allocated = 0;
	u8 fw_session_id;
	struct bnxt *bp;
	u16 hcapi_type;
	int rc;

	if (!tfp || !parms || !parms->data)
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

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to get em_ext_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	/* Verify that the entry has been previously allocated.
	 * for meter drop counter, check the corresponding meter
	 * entry
	 */
	aparms.rm_db = tbl_db->tbl_db[parms->dir];
	if (parms->type != TF_TBL_TYPE_METER_DROP_CNT)
		aparms.subtype = parms->type;
	else
		aparms.subtype = TF_TBL_TYPE_METER_INST;
	aparms.index = parms->idx;
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		netdev_dbg(tfp->bp->dev,
			   "%s, Invalid index, type:%s, idx:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), parms->idx);
		return -EINVAL;
	}

	/* Set the entry */
	hparms.rm_db = tbl_db->tbl_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s, Failed type lookup, type:%s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), rc);
		return rc;
	}

	/* Get the entry */
	rc = tf_msg_get_tbl_entry(tfp, parms->dir, hcapi_type,
				  parms->data_sz_in_bytes, parms->data,
				  parms->idx, false, fw_session_id);
	if (rc) {
		netdev_dbg(bp->dev, "%s, Get failed, type:%s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), rc);
		return rc;
	}

	return 0;
}

int tf_tbl_bulk_get(struct tf *tfp, struct tf_tbl_get_bulk_parms *parms)
{
	struct tf_rm_check_indexes_in_range_parms cparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	struct bnxt *bp;
	struct tbl_rm_db *tbl_db;
	struct tf_dev_info *dev;
	void *tbl_db_ptr = NULL;
	struct tf_session *tfs;
	u16 hcapi_type;
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

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to get em_ext_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	/* Verify that the entries are in the range of reserved resources. */
	cparms.rm_db = tbl_db->tbl_db[parms->dir];
	cparms.subtype = parms->type;
	cparms.num_entries = parms->num_entries;
	cparms.starting_index = parms->starting_idx;

	rc = tf_rm_check_indexes_in_range(&cparms);
	if (rc) {
		netdev_dbg(bp->dev,
			   "%s, index %d entries: %d not in range, type:%s",
			   tf_dir_2_str(parms->dir), parms->starting_idx,
			   parms->num_entries, tf_tbl_type_2_str(parms->type));
		return rc;
	}

	hparms.rm_db = tbl_db->tbl_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s, Failed type lookup, type:%s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), rc);
		return rc;
	}

	/* Get the entries */
	rc = tf_msg_bulk_get_tbl_entry(tfp, parms->dir, hcapi_type,
				       parms->starting_idx,
				       parms->num_entries,
				       parms->entry_sz_in_bytes,
				       parms->physical_mem_addr, false);
	if (rc) {
		netdev_dbg(bp->dev, "%s, Bulk get failed, type:%s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), rc);
	}

	return rc;
}

int tf_tbl_get_resc_info(struct tf *tfp, struct tf_tbl_resource_info *tbl)
{
	struct tf_rm_get_alloc_info_parms ainfo;
	struct tf_resource_info *dinfo;
	struct tbl_rm_db *tbl_db;
	void *tbl_db_ptr = NULL;
	struct tf_dev_info *dev;
	u16 base = 0, shift = 0;
	struct tf_session *tfs;
	int d, i;
	int rc;

	if (!tfp || !tbl)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc == -ENOMEM)
		return 0; /* db doesn't exist */
	else if (rc)
		return rc; /* error getting db */

	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	for (d = 0; d < TF_DIR_MAX; d++) {
		ainfo.rm_db = tbl_db->tbl_db[d];
		dinfo = tbl[d].info;

		if (!ainfo.rm_db)
			continue;

		ainfo.info = (struct tf_rm_alloc_info *)dinfo;
		ainfo.subtype = 0;
		rc = tf_rm_get_all_info(&ainfo, TF_TBL_TYPE_MAX);
		if (rc)
			return rc;

		if (dev->ops->tf_dev_get_tbl_info) {
			/* Adjust all */
			for (i = 0; i < TF_TBL_TYPE_MAX; i++) {
				/* Only get table info if required for the device */
				rc = dev->ops->tf_dev_get_tbl_info(tfp,
								   tbl_db->tbl_db[d],
								   i,
								   &base,
								   &shift);
				if (rc) {
					netdev_dbg(tfp->bp->dev,
						   "%s: Failed to get table info:%d\n",
						   tf_dir_2_str(d), i);
					return rc;
				}
				if (dinfo[i].stride)
					TF_TBL_RM_TO_PTR(&dinfo[i].start,
							 dinfo[i].start,
							 base,
							 shift);
			}
		}
	}

	return 0;
}
