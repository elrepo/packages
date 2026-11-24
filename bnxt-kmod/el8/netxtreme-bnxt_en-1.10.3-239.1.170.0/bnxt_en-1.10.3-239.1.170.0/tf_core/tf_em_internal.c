// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include "tf_core.h"
#include "tf_util.h"
#include "tf_em.h"
#include "tf_msg.h"
#include "tf_ext_flow_handle.h"
#include "bnxt.h"

#define TF_EM_DB_EM_REC 0
#include "dpool.h"

/**
 * Insert EM internal entry API
 *
 *  returns:
 *     0 - Success
 */
int tf_em_insert_int_entry(struct tf *tfp,
			   struct tf_insert_em_entry_parms *parms)
{
	struct tf_session *tfs;
	u8 num_of_entries = 0;
	struct dpool *pool;
	u16 rptr_index = 0;
	u8 rptr_entry = 0;
	u8 fw_session_id;
	u32 index;
	u32 gfid;
	int rc;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(NULL, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	pool = (struct dpool *)tfs->em_pool[parms->dir];
	index = dpool_alloc(pool, TF_SESSION_EM_ENTRY_SIZE, 0);
	if (index == DP_INVALID_INDEX) {
		netdev_dbg(NULL, "%s, EM entry index allocation failed\n",
			   tf_dir_2_str(parms->dir));
		return -1;
	}

	rptr_index = index;
	rc = tf_msg_insert_em_internal_entry(tfp, parms, fw_session_id,
					     &rptr_index, &rptr_entry,
					     &num_of_entries);
	if (rc) {
		/* Free the allocated index before returning */
		dpool_free(pool, index);
		return -1;
	}
	netdev_dbg(NULL,
		   "%s, Internal index:%d rptr_i:0x%x rptr_e:0x%x num:%d\n",
		   tf_dir_2_str(parms->dir), index, rptr_index, rptr_entry,
		   num_of_entries);
	TF_SET_GFID(gfid,
		    ((rptr_index << TF_EM_INTERNAL_INDEX_SHIFT) | rptr_entry),
		    0); /* N/A for internal table */

	TF_SET_FLOW_ID(parms->flow_id, gfid, TF_GFID_TABLE_INTERNAL,
		       parms->dir);

	TF_SET_FIELDS_IN_FLOW_HANDLE(parms->flow_handle,
				     (u32)num_of_entries,
				     0,
				     TF_FLAGS_FLOW_HANDLE_INTERNAL,
				     rptr_index,
				     rptr_entry,
				     0);
	return 0;
}

/**
 * Delete EM internal entry API
 *
 * returns:
 * 0
 * -EINVAL
 */
int tf_em_delete_int_entry(struct tf *tfp,
			   struct tf_delete_em_entry_parms *parms)
{
	struct tf_session *tfs;
	struct dpool *pool;
	u8 fw_session_id;
	int rc = 0;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(NULL, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	rc = tf_msg_delete_em_entry(tfp, parms, fw_session_id);

	/* Return resource to pool */
	if (rc == 0) {
		pool = (struct dpool *)tfs->em_pool[parms->dir];
		dpool_free(pool, parms->index);
	}

	return rc;
}

static int tf_em_move_callback(void *user_data, u64 entry_data,
			       u32 new_index)
{
	struct tf *tfp = (struct tf *)user_data;
	struct tf_move_em_entry_parms parms = { 0 };
	struct tf_dev_info     *dev;
	struct tf_session      *tfs;
	int rc;

	parms.tbl_scope_id = 0;
	parms.flow_handle  = entry_data;
	parms.new_index    = new_index;
	TF_GET_DIR_FROM_FLOW_ID(entry_data, parms.dir);
	parms.mem          = TF_MEM_INTERNAL;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms.dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms.dir), rc);
		return rc;
	}

	if (!dev->ops->tf_dev_move_int_em_entry)
		rc = dev->ops->tf_dev_move_int_em_entry(tfp, &parms);
	else
		rc = -EOPNOTSUPP;

	return rc;
}

int tf_em_int_bind(struct tf *tfp, struct tf_em_cfg_parms *parms)
{
	struct tf_rm_create_db_parms db_cfg = { 0 };
	struct tf_rm_get_alloc_info_parms iparms;
	int db_rc[TF_DIR_MAX] = { 0 };
	struct tf_rm_alloc_info info;
	struct tf_session *tfs;
	struct em_rm_db *em_db;
	struct dpool *mem_va;
	int rc;
	int i;

	if (!tfp || !parms)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	em_db = vzalloc(sizeof(*em_db));
	if (!em_db)
		return -ENOMEM;

	for (i = 0; i < TF_DIR_MAX; i++)
		em_db->em_db[i] = NULL;
	tf_session_set_db(tfp, TF_MODULE_TYPE_EM, em_db);

	db_cfg.module = TF_MODULE_TYPE_EM;
	db_cfg.num_elements = parms->num_elements;
	db_cfg.cfg = parms->cfg;

	for (i = 0; i < TF_DIR_MAX; i++) {
		db_cfg.dir = i;
		db_cfg.alloc_cnt = parms->resources->em_cnt[i].cnt;

		/* Check if we got any request to support EEM, if so
		 * we build an EM Int DB holding Table Scopes.
		 */
		if (db_cfg.alloc_cnt[TF_EM_TBL_TYPE_EM_RECORD] == 0)
			continue;

		if (db_cfg.alloc_cnt[TF_EM_TBL_TYPE_EM_RECORD] %
		    TF_SESSION_EM_ENTRY_SIZE != 0) {
			rc = -ENOMEM;
			netdev_dbg(tfp->bp->dev,
				   "%s, EM must be in blocks of %d, rc %d\n",
				   tf_dir_2_str(i), TF_SESSION_EM_ENTRY_SIZE,
				   rc);

			return rc;
		}

		db_cfg.rm_db = (void *)&em_db->em_db[i];
		if (tf_session_is_shared_session(tfs) &&
		    (!tf_session_is_shared_session_creator(tfs)))
			db_rc[i] = tf_rm_create_db_no_reservation(tfp,
								  &db_cfg);
		else
			db_rc[i] = tf_rm_create_db(tfp, &db_cfg);
		if (db_rc[i]) {
			netdev_dbg(tfp->bp->dev,
				   "%s: EM Int DB creation failed\n",
				   tf_dir_2_str(i));
		}
	}

	/* No db created */
	if (db_rc[TF_DIR_RX] && db_rc[TF_DIR_TX]) {
		netdev_dbg(tfp->bp->dev, "EM Int DB creation failed\n");
		return db_rc[TF_DIR_RX];
	}

	if (tf_session_is_shared_session(tfs))
		return 0;

	for (i = 0; i < TF_DIR_MAX; i++) {
		iparms.rm_db = em_db->em_db[i];
		iparms.subtype = TF_EM_DB_EM_REC;
		iparms.info = &info;

		rc = tf_rm_get_info(&iparms);
		if (rc) {
			netdev_dbg(tfp->bp->dev,
				   "%s: EM DB get info failed\n",
				   tf_dir_2_str(i));
			return rc;
		}
		/* Allocate stack pool */
		mem_va = vzalloc(sizeof(*mem_va));
		if (!mem_va) {
			rc = -ENOMEM;
			return rc;
		}

		tfs->em_pool[i] = mem_va;

		rc = dpool_init(tfs->em_pool[i],
				iparms.info->entry.start,
				iparms.info->entry.stride,
				7,
				(void *)tfp,
				tf_em_move_callback);
		/* Logging handled in tf_create_em_pool */
		if (rc)
			return rc;
	}

	return 0;
}

int tf_em_int_unbind(struct tf *tfp)
{
	struct tf_rm_free_db_parms fparms = { 0 };
	struct em_rm_db *em_db;
	void *em_db_ptr = NULL;
	struct tf_session *tfs;
	int rc;
	int i;

	if (!tfp)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	if (!tf_session_is_shared_session(tfs)) {
		for (i = 0; i < TF_DIR_MAX; i++) {
			if (!tfs->em_pool[i])
				continue;
			dpool_free_all(tfs->em_pool[i]);
		}
	}

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_EM, &em_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "Em_db is not initialized\n");
		return 0;
	}
	em_db = (struct em_rm_db *)em_db_ptr;

	for (i = 0; i < TF_DIR_MAX; i++) {
		if (!em_db->em_db[i])
			continue;
		fparms.dir = i;
		fparms.rm_db = em_db->em_db[i];
		rc = tf_rm_free_db(tfp, &fparms);
		if (rc)
			return rc;

		em_db->em_db[i] = NULL;
	}

	/* Free EM database pointer */
	tf_session_set_db(tfp, TF_MODULE_TYPE_EM, NULL);
	vfree(em_db);

	return 0;
}

int tf_em_get_resc_info(struct tf *tfp, struct tf_em_resource_info *em)
{
	struct tf_rm_get_alloc_info_parms ainfo;
	struct tf_resource_info *dinfo;
	void *em_db_ptr = NULL;
	struct em_rm_db *em_db;
	int rc;
	int d;

	if (!tfp || !em)
		return -EINVAL;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_EM, &em_db_ptr);
	if (rc == -ENOMEM)
		return 0;  /* db does not exist */
	else if (rc)
		return rc; /* db error */

	em_db = (struct em_rm_db *)em_db_ptr;

	/* check if reserved resource for EM is multiple of num_slices */
	for (d = 0; d < TF_DIR_MAX; d++) {
		ainfo.rm_db = em_db->em_db[d];
		dinfo = em[d].info;

		if (!ainfo.rm_db)
			continue;

		ainfo.info = (struct tf_rm_alloc_info *)dinfo;
		ainfo.subtype = 0;
		rc = tf_rm_get_all_info(&ainfo, TF_EM_TBL_TYPE_MAX);
		if (rc && rc != -EOPNOTSUPP)
			return rc;
	}

	return 0;
}
