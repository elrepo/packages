// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include "tf_identifier.h"
#include "tf_rm.h"
#include "tf_util.h"
#include "tf_session.h"
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"

struct tf;

int tf_ident_bind(struct tf *tfp, struct tf_ident_cfg_parms *parms)
{
	struct tf_rm_create_db_parms db_cfg = { 0 };
	int db_rc[TF_DIR_MAX] = { 0 };
	struct ident_rm_db *ident_db;
	struct tf_session *tfs;
	int rc;
	int i;

	if (!tfp || !parms)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	ident_db = vzalloc(sizeof(*ident_db));
	if (!ident_db)
		return -ENOMEM;

	for (i = 0; i < TF_DIR_MAX; i++)
		ident_db->ident_db[i] = NULL;
	tf_session_set_db(tfp, TF_MODULE_TYPE_IDENTIFIER, ident_db);

	db_cfg.module = TF_MODULE_TYPE_IDENTIFIER;
	db_cfg.num_elements = parms->num_elements;
	db_cfg.cfg = parms->cfg;

	for (i = 0; i < TF_DIR_MAX; i++) {
		db_cfg.rm_db = (void *)&ident_db->ident_db[i];
		db_cfg.dir = i;
		db_cfg.alloc_cnt = parms->resources->ident_cnt[i].cnt;
		if (tf_session_is_shared_session(tfs) &&
		    (!tf_session_is_shared_session_creator(tfs)))
			db_rc[i] = tf_rm_create_db_no_reservation(tfp, &db_cfg);
		else
			db_rc[i] = tf_rm_create_db(tfp, &db_cfg);

		if (db_rc[i])
			netdev_dbg(tfp->bp->dev,
				   "%s: No Identifier DB required\n",
				   tf_dir_2_str(i));
	}

	/* No db created */
	if (db_rc[TF_DIR_RX] && db_rc[TF_DIR_TX]) {
		netdev_dbg(tfp->bp->dev, "No Identifier DB created\n");
		return db_rc[TF_DIR_RX];
	}

	netdev_dbg(tfp->bp->dev, "Identifier - initialized\n");

	return 0;
}

int tf_ident_unbind(struct tf *tfp)
{
	struct tf_rm_free_db_parms fparms = { 0 };
	struct ident_rm_db *ident_db;
	void *ident_db_ptr = NULL;
	int rc = 0;
	int i;

	if (!tfp)
		return -EINVAL;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_IDENTIFIER, &ident_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "Ident_db is not initialized\n");
		return 0;
	}
	ident_db = (struct ident_rm_db *)ident_db_ptr;

	for (i = 0; i < TF_DIR_MAX; i++) {
		if (!ident_db->ident_db[i])
			continue;
		fparms.rm_db = ident_db->ident_db[i];
		fparms.dir = i;
		rc = tf_rm_free_db(tfp, &fparms);
		if (rc) {
			netdev_dbg(tfp->bp->dev,
				   "rm free failed on unbind\n");
		}

		ident_db->ident_db[i] = NULL;
	}
	tf_session_set_db(tfp, TF_MODULE_TYPE_IDENTIFIER, NULL);
	vfree(ident_db);
	return 0;
}

int tf_ident_alloc(struct tf *tfp, struct tf_ident_alloc_parms *parms)
{
	struct tf_rm_allocate_parms aparms = { 0 };
	struct ident_rm_db *ident_db;
	void *ident_db_ptr = NULL;
	u32 base_id;
	u32 id;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_IDENTIFIER, &ident_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to get ident_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	ident_db = (struct ident_rm_db *)ident_db_ptr;

	aparms.rm_db = ident_db->ident_db[parms->dir];
	aparms.subtype = parms->type;
	aparms.index = &id;
	aparms.base_index = &base_id;
	rc = tf_rm_allocate(&aparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "%s: Failed allocate, type:%d\n",
			   tf_dir_2_str(parms->dir), parms->type);
		return rc;
	}

	*parms->id = id;

	return 0;
}

int tf_ident_free(struct tf *tfp, struct tf_ident_free_parms *parms)
{
	struct tf_rm_is_allocated_parms aparms = { 0 };
	struct tf_rm_free_parms fparms = { 0 };
	struct ident_rm_db *ident_db;
	void *ident_db_ptr = NULL;
	int allocated = 0;
	u32 base_id;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_IDENTIFIER, &ident_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to get ident_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	ident_db = (struct ident_rm_db *)ident_db_ptr;

	/* Check if element is in use */
	aparms.rm_db = ident_db->ident_db[parms->dir];
	aparms.subtype = parms->type;
	aparms.index = parms->id;
	aparms.base_index = &base_id;
	aparms.allocated = &allocated;
	rc = tf_rm_is_allocated(&aparms);
	if (rc)
		return rc;

	if (allocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
		netdev_dbg(tfp->bp->dev,
			   "%s: Entry already free, type:%d, index:%d\n",
			   tf_dir_2_str(parms->dir), parms->type, parms->id);
		return -EINVAL;
	}

	/* Free requested element */
	fparms.rm_db = ident_db->ident_db[parms->dir];
	fparms.subtype = parms->type;
	fparms.index = parms->id;
	rc = tf_rm_free(&fparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s: Free failed, type:%d, index:%d\n",
			   tf_dir_2_str(parms->dir), parms->type, parms->id);
		return rc;
	}

	return 0;
}

int tf_ident_get_resc_info(struct tf *tfp,
			   struct tf_identifier_resource_info *ident)
{
	struct tf_rm_get_alloc_info_parms ainfo;
	struct tf_resource_info *dinfo;
	struct ident_rm_db *ident_db;
	void *ident_db_ptr = NULL;
	int rc;
	int d;

	if (!tfp || !ident)
		return -EINVAL;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_IDENTIFIER, &ident_db_ptr);
	if (rc == -ENOMEM)
		return 0; /* db doesn't exist */
	else if (rc)
		return rc; /* error getting db */

	ident_db = (struct ident_rm_db *)ident_db_ptr;

	/* check if reserved resource for WC is multiple of num_slices */
	for (d = 0; d < TF_DIR_MAX; d++) {
		ainfo.rm_db = ident_db->ident_db[d];

		if (!ainfo.rm_db)
			continue;

		dinfo = ident[d].info;

		ainfo.info = (struct tf_rm_alloc_info *)dinfo;
		ainfo.subtype = 0;
		rc = tf_rm_get_all_info(&ainfo, TF_IDENT_TYPE_MAX);
		if (rc)
			return rc;
	}

	return 0;
}
