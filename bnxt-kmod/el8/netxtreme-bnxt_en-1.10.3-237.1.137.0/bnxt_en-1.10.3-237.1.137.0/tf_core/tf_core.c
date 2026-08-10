// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <linux/types.h>
#include <linux/vmalloc.h>
#include "tf_core.h"
#include "tf_util.h"
#include "tf_session.h"
#include "tf_tbl.h"
#include "tf_em.h"
#include "tf_rm.h"
#include "tf_global_cfg.h"
#include "tf_msg.h"
#include "bitalloc.h"
#include "bnxt.h"
#include "tf_ext_flow_handle.h"

int tf_open_session(struct tf *tfp, struct tf_open_session_parms *parms)
{
	struct tf_session_open_session_parms oparms;
	unsigned int domain, bus, slot, device;
	struct bnxt *bp;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = parms->bp;

	/* Filter out any non-supported device types on the Core
	 * side. It is assumed that the Firmware will be supported if
	 * firmware open session succeeds.
	 */
	if (parms->device_type != TF_DEVICE_TYPE_P4 &&
	    parms->device_type != TF_DEVICE_TYPE_P5) {
		netdev_dbg(bp->dev, "Unsupported device type %d\n",
			   parms->device_type);
		return -EOPNOTSUPP;
	}

	/* Verify control channel and build the beginning of session_id */
	rc = sscanf(parms->ctrl_chan_name, "%x:%x:%x.%u", &domain, &bus, &slot,
		    &device);
	if (rc != 4) {
		/* PCI Domain not provided (optional in DPDK), thus we
		 * force domain to 0 and recheck.
		 */
		domain = 0;

		/* Check parsing of bus/slot/device */
		rc = sscanf(parms->ctrl_chan_name, "%x:%x.%u", &bus, &slot,
			    &device);
		if (rc != 3) {
			netdev_dbg(bp->dev,
				   "Failed to scan device ctrl_chan_name\n");
			return -EINVAL;
		}
	}

	parms->session_id.internal.domain = domain;
	parms->session_id.internal.bus = bus;
	parms->session_id.internal.device = device;
	oparms.open_cfg = parms;

	/* Session vs session client is decided in
	 * tf_session_open_session()
	 */
	rc = tf_session_open_session(tfp, &oparms);
	/* Logging handled by tf_session_open_session */
	if (rc)
		return rc;

	netdev_dbg(bp->dev, "%s: domain:%d, bus:%d, device:%u\n", __func__,
		   parms->session_id.internal.domain,
		   parms->session_id.internal.bus,
		   parms->session_id.internal.device);

	return 0;
}

int tf_attach_session(struct tf *tfp, struct tf_attach_session_parms *parms)
{
	struct tf_session_attach_session_parms aparms;
	unsigned int domain, bus, slot, device;
	struct bnxt *bp;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Verify control channel */
	rc = sscanf(parms->ctrl_chan_name, "%x:%x:%x.%u", &domain, &bus, &slot,
		    &device);
	if (rc != 4) {
		netdev_dbg(bp->dev, "Failed to scan device ctrl_chan_name\n");
		return -EINVAL;
	}

	/* Verify 'attach' channel */
	rc = sscanf(parms->attach_chan_name, "%x:%x:%x.%u", &domain, &bus,
		    &slot, &device);
	if (rc != 4) {
		netdev_dbg(bp->dev, "Failed to scan device attach_chan_name\n");
		return -EINVAL;
	}

	/* Prepare return value of session_id, using ctrl_chan_name
	 * device values as it becomes the session id.
	 */
	parms->session_id.internal.domain = domain;
	parms->session_id.internal.bus = bus;
	parms->session_id.internal.device = device;
	aparms.attach_cfg = parms;
	rc = tf_session_attach_session(tfp,
				       &aparms);
	/* Logging handled by dev_bind */
	if (rc)
		return rc;

	netdev_dbg(bp->dev,
		   "%s: sid:%d domain:%d, bus:%d, device:%d, fw_sid:%d\n",
		   __func__, parms->session_id.id,
		   parms->session_id.internal.domain,
		   parms->session_id.internal.bus,
		   parms->session_id.internal.device,
		   parms->session_id.internal.fw_session_id);

	return rc;
}

int tf_close_session(struct tf *tfp)
{
	struct tf_session_close_session_parms cparms = { 0 };
	union tf_session_id session_id = { 0 };
	u8 ref_count;
	int rc;

	if (!tfp)
		return -EINVAL;

	cparms.ref_count = &ref_count;
	cparms.session_id = &session_id;
	/* Session vs session client is decided in
	 * tf_session_close_session()
	 */
	rc = tf_session_close_session(tfp,
				      &cparms);
	/* Logging handled by tf_session_close_session */
	if (rc)
		return rc;

	netdev_dbg(tfp->bp->dev, "%s: domain:%d, bus:%d, device:%d\n",
		   __func__, cparms.session_id->internal.domain,
		   cparms.session_id->internal.bus,
		   cparms.session_id->internal.device);

	return rc;
}

/* insert EM hash entry API
 *
 *    returns:
 *    0       - Success
 *    -EINVAL - Error
 */
int tf_insert_em_entry(struct tf *tfp, struct tf_insert_em_entry_parms *parms)
{
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	if (parms->mem == TF_MEM_EXTERNAL &&
	    dev->ops->tf_dev_insert_ext_em_entry)
		rc = dev->ops->tf_dev_insert_ext_em_entry(tfp, parms);
	else if (parms->mem == TF_MEM_INTERNAL &&
		 dev->ops->tf_dev_insert_int_em_entry)
		rc = dev->ops->tf_dev_insert_int_em_entry(tfp, parms);
	else
		return -EINVAL;

	if (rc) {
		netdev_err(bp->dev, "%s: EM insert failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	return 0;
}

/* Delete EM hash entry API
 *
 *    returns:
 *    0       - Success
 *    -EINVAL - Error
 */
int tf_delete_em_entry(struct tf *tfp, struct tf_delete_em_entry_parms *parms)
{
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	unsigned int flag = 0;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	TF_GET_FLAG_FROM_FLOW_HANDLE(parms->flow_handle, flag);
	if ((flag & TF_FLAGS_FLOW_HANDLE_INTERNAL))
		rc = dev->ops->tf_dev_delete_int_em_entry(tfp, parms);
	else
		rc = dev->ops->tf_dev_delete_ext_em_entry(tfp, parms);

	if (rc) {
		netdev_dbg(bp->dev, "%s: EM delete failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	return rc;
}

/* Get global configuration API
 *
 *    returns:
 *    0       - Success
 *    -EINVAL - Error
 */
int tf_get_global_cfg(struct tf *tfp, struct tf_global_cfg_parms *parms)
{
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc = 0;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	if (!parms->config || parms->config_sz_in_bytes == 0) {
		netdev_dbg(bp->dev, "Invalid Argument(s)\n");
		return -EINVAL;
	}

	if (!dev->ops->tf_dev_get_global_cfg) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return -EOPNOTSUPP;
	}

	rc = dev->ops->tf_dev_get_global_cfg(tfp, parms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Global Cfg get failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	return rc;
}

/* Set global configuration API
 *
 *    returns:
 *    0       - Success
 *    -EINVAL - Error
 */
int tf_set_global_cfg(struct tf *tfp, struct tf_global_cfg_parms *parms)
{
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc = 0;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	if (!parms->config || parms->config_sz_in_bytes == 0) {
		netdev_dbg(bp->dev, "Invalid Argument(s)\n");
		return -EINVAL;
	}

	if (!dev->ops->tf_dev_set_global_cfg) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return -EOPNOTSUPP;
	}

	rc = dev->ops->tf_dev_set_global_cfg(tfp, parms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Global Cfg set failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	return rc;
}

int tf_alloc_identifier(struct tf *tfp,
			struct tf_alloc_identifier_parms *parms)
{
	struct tf_ident_alloc_parms aparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;
	u16 id;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	if (!dev->ops->tf_dev_alloc_ident) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return -EOPNOTSUPP;
	}

	aparms.dir = parms->dir;
	aparms.type = parms->ident_type;
	aparms.id = &id;
	rc = dev->ops->tf_dev_alloc_ident(tfp, &aparms);
	if (rc) {
		netdev_dbg(bp->dev,
			   "%s: Identifier allocation failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	parms->id = id;

	return 0;
}

int tf_free_identifier(struct tf *tfp, struct tf_free_identifier_parms *parms)
{
	struct tf_ident_free_parms fparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	if (!dev->ops->tf_dev_free_ident) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return -EOPNOTSUPP;
	}

	fparms.dir = parms->dir;
	fparms.type = parms->ident_type;
	fparms.id = parms->id;
	fparms.ref_cnt = &parms->ref_cnt;
	rc = dev->ops->tf_dev_free_ident(tfp, &fparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Identifier free failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	return 0;
}

int tf_alloc_tcam_entry(struct tf *tfp,
			struct tf_alloc_tcam_entry_parms *parms)
{
	struct tf_tcam_alloc_parms aparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	if (!dev->ops->tf_dev_alloc_tcam) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	aparms.dir = parms->dir;
	aparms.type = parms->tcam_tbl_type;
	aparms.key_size = TF_BITS2BYTES_WORD_ALIGN(parms->key_sz_in_bits);
	aparms.priority = parms->priority;
	rc = dev->ops->tf_dev_alloc_tcam(tfp, &aparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: TCAM allocation failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	parms->idx = aparms.idx;

	return 0;
}

int tf_set_tcam_entry(struct tf *tfp, struct tf_set_tcam_entry_parms *parms)
{
	struct tf_tcam_set_parms sparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	if (!dev->ops->tf_dev_set_tcam || !dev->ops->tf_dev_word_align) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	sparms.dir = parms->dir;
	sparms.type = parms->tcam_tbl_type;
	sparms.idx = parms->idx;
	sparms.key = parms->key;
	sparms.mask = parms->mask;
	sparms.key_size = dev->ops->tf_dev_word_align(parms->key_sz_in_bits);
	sparms.result = parms->result;
	sparms.result_size = TF_BITS2BYTES_WORD_ALIGN(parms->result_sz_in_bits);

	rc = dev->ops->tf_dev_set_tcam(tfp, &sparms);
	if (rc) {
		netdev_err(bp->dev, "%s: TCAM set failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	return 0;
}

int tf_get_tcam_entry(struct tf *tfp, struct tf_get_tcam_entry_parms *parms)
{
	struct tf_tcam_get_parms gparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	if (!dev->ops->tf_dev_get_tcam) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	gparms.dir = parms->dir;
	gparms.type = parms->tcam_tbl_type;
	gparms.idx = parms->idx;
	gparms.key = parms->key;
	gparms.key_size = dev->ops->tf_dev_word_align(parms->key_sz_in_bits);
	gparms.mask = parms->mask;
	gparms.result = parms->result;
	gparms.result_size = TF_BITS2BYTES_WORD_ALIGN(parms->result_sz_in_bits);

	rc = dev->ops->tf_dev_get_tcam(tfp, &gparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: TCAM get failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}
	parms->key_sz_in_bits = gparms.key_size * 8;
	parms->result_sz_in_bits = gparms.result_size * 8;

	return 0;
}

int tf_free_tcam_entry(struct tf *tfp, struct tf_free_tcam_entry_parms *parms)
{
	struct tf_tcam_free_parms fparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	if (!dev->ops->tf_dev_free_tcam) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	fparms.dir = parms->dir;
	fparms.type = parms->tcam_tbl_type;
	fparms.idx = parms->idx;
	rc = dev->ops->tf_dev_free_tcam(tfp, &fparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: TCAM free failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	return 0;
}

int tf_alloc_tbl_entry(struct tf *tfp, struct tf_alloc_tbl_entry_parms *parms)
{
	struct tf_tbl_alloc_parms aparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;
	u32 idx;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	aparms.dir = parms->dir;
	aparms.type = parms->type;
	aparms.idx = &idx;
	aparms.tbl_scope_id = parms->tbl_scope_id;

	if (parms->type == TF_TBL_TYPE_EXT) {
		if (!dev->ops->tf_dev_alloc_ext_tbl) {
			rc = -EOPNOTSUPP;
			netdev_dbg(bp->dev,
				   "%s: Operation not supported, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return -EOPNOTSUPP;
		}

		rc = dev->ops->tf_dev_alloc_ext_tbl(tfp, &aparms);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: External table allocation failed, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return rc;
		}
	} else if (dev->ops->tf_dev_is_sram_managed(tfp, parms->type)) {
		rc = dev->ops->tf_dev_alloc_sram_tbl(tfp, &aparms);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: SRAM table allocation failed, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return rc;
		}
	} else {
		rc = dev->ops->tf_dev_alloc_tbl(tfp, &aparms);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: Table allocation failed, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return rc;
		}
	}

	parms->idx = idx;

	return 0;
}

int tf_free_tbl_entry(struct tf *tfp, struct tf_free_tbl_entry_parms *parms)
{
	struct tf_tbl_free_parms fparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	fparms.dir = parms->dir;
	fparms.type = parms->type;
	fparms.idx = parms->idx;
	fparms.tbl_scope_id = parms->tbl_scope_id;

	if (parms->type == TF_TBL_TYPE_EXT) {
		if (!dev->ops->tf_dev_free_ext_tbl) {
			rc = -EOPNOTSUPP;
			netdev_dbg(bp->dev,
				   "%s: Operation not supported, rc:%d\n",
				   tf_dir_2_str(parms->dir),
				   rc);
			return -EOPNOTSUPP;
		}

		rc = dev->ops->tf_dev_free_ext_tbl(tfp, &fparms);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Table free failed, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return rc;
		}
	} else if (dev->ops->tf_dev_is_sram_managed(tfp, parms->type)) {
		rc = dev->ops->tf_dev_free_sram_tbl(tfp, &fparms);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: SRAM table free failed, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return rc;
		}
	} else {
		rc = dev->ops->tf_dev_free_tbl(tfp, &fparms);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Table free failed, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return rc;
		}
	}
	return 0;
}

int tf_set_tbl_entry(struct tf *tfp, struct tf_set_tbl_entry_parms *parms)
{
	struct tf_tbl_set_parms sparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc = 0;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	sparms.dir = parms->dir;
	sparms.type = parms->type;
	sparms.data = parms->data;
	sparms.data_sz_in_bytes = parms->data_sz_in_bytes;
	sparms.idx = parms->idx;
	sparms.tbl_scope_id = parms->tbl_scope_id;

	if (parms->type == TF_TBL_TYPE_EXT) {
		if (!dev->ops->tf_dev_set_ext_tbl) {
			rc = -EOPNOTSUPP;
			netdev_dbg(bp->dev,
				   "%s: Operation not supported, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return -EOPNOTSUPP;
		}

		rc = dev->ops->tf_dev_set_ext_tbl(tfp, &sparms);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Table set failed, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return rc;
		}
	}  else if (dev->ops->tf_dev_is_sram_managed(tfp, parms->type)) {
		rc = dev->ops->tf_dev_set_sram_tbl(tfp, &sparms);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: SRAM table set failed, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return rc;
		}
	} else {
		if (!dev->ops->tf_dev_set_tbl) {
			rc = -EOPNOTSUPP;
			netdev_dbg(bp->dev,
				   "%s: Operation not supported, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return -EOPNOTSUPP;
		}

		rc = dev->ops->tf_dev_set_tbl(tfp, &sparms);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: Table set failed, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return rc;
		}
	}

	return rc;
}

int tf_get_tbl_entry(struct tf *tfp, struct tf_get_tbl_entry_parms *parms)
{
	struct tf_tbl_get_parms gparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc = 0;

	if (!tfp || !parms || !parms->data)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	gparms.dir = parms->dir;
	gparms.type = parms->type;
	gparms.data = parms->data;
	gparms.data_sz_in_bytes = parms->data_sz_in_bytes;
	gparms.idx = parms->idx;

	if (dev->ops->tf_dev_is_sram_managed(tfp, parms->type)) {
		rc = dev->ops->tf_dev_get_sram_tbl(tfp, &gparms);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: SRAM table get failed, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return rc;
		}
	} else {
		if (!dev->ops->tf_dev_get_tbl) {
			rc = -EOPNOTSUPP;
			netdev_dbg(bp->dev,
				   "%s: Operation not supported, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return -EOPNOTSUPP;
		}

		rc = dev->ops->tf_dev_get_tbl(tfp, &gparms);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Table get failed, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
			return rc;
		}
	}

	return rc;
}

int tf_bulk_get_tbl_entry(struct tf *tfp,
			  struct tf_bulk_get_tbl_entry_parms *parms)
{
	struct tf_tbl_get_bulk_parms bparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc = 0;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	bparms.dir = parms->dir;
	bparms.type = parms->type;
	bparms.starting_idx = parms->starting_idx;
	bparms.num_entries = parms->num_entries;
	bparms.entry_sz_in_bytes = parms->entry_sz_in_bytes;
	bparms.physical_mem_addr = parms->physical_mem_addr;

	if (parms->type == TF_TBL_TYPE_EXT) {
		/* Not supported, yet */
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev,
			   "%s, External table type not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);

		return rc;
	} else if (dev->ops->tf_dev_is_sram_managed(tfp, parms->type)) {
		rc = dev->ops->tf_dev_get_bulk_sram_tbl(tfp, &bparms);
		if (rc) {
			netdev_dbg(bp->dev,
				   "%s: SRAM table bulk get failed, rc:%d\n",
				   tf_dir_2_str(parms->dir), rc);
		}
		return rc;
	}

	if (!dev->ops->tf_dev_get_bulk_tbl) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return -EOPNOTSUPP;
	}

	rc = dev->ops->tf_dev_get_bulk_tbl(tfp, &bparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Table get bulk failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}
	return rc;
}

int tf_get_shared_tbl_increment(struct tf *tfp,
				struct tf_get_shared_tbl_increment_parms
				*parms)
{
	struct tf_session *tfs;
	struct tf_dev_info *dev;
	struct bnxt *bp;
	int rc = 0;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Internal table type processing */

	if (!dev->ops->tf_dev_get_shared_tbl_increment) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), -rc);
		return rc;
	}

	rc = dev->ops->tf_dev_get_shared_tbl_increment(tfp, parms);
	if (rc) {
		netdev_dbg(bp->dev,
			   "%s: Get table increment not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	return rc;
}

int
tf_alloc_tbl_scope(struct tf *tfp,
		   struct tf_alloc_tbl_scope_parms *parms)
{
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	struct bnxt *bp;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to lookup session, rc:%d\n",
			   rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to lookup device, rc:%d\n",
			   rc);
		return rc;
	}

	if (dev->ops->tf_dev_alloc_tbl_scope) {
		rc = dev->ops->tf_dev_alloc_tbl_scope(tfp, parms);
	} else {
		netdev_dbg(bp->dev,
			   "Alloc table scope not supported by device\n");
		return -EINVAL;
	}

	return rc;
}

int
tf_map_tbl_scope(struct tf *tfp,
		 struct tf_map_tbl_scope_parms *parms)
{
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	struct bnxt *bp;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to lookup session, rc:%d\n",
			   rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to lookup device, rc:%d\n",
			   rc);
		return rc;
	}

	if (dev->ops->tf_dev_map_tbl_scope) {
		rc = dev->ops->tf_dev_map_tbl_scope(tfp, parms);
	} else {
		netdev_dbg(bp->dev,
			   "Map table scope not supported by device\n");
		return -EINVAL;
	}

	return rc;
}

int
tf_free_tbl_scope(struct tf *tfp,
		  struct tf_free_tbl_scope_parms *parms)
{
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	struct bnxt *bp;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to lookup session, rc:%d\n",
			   rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev,
			   "Failed to lookup device, rc:%d\n",
			   rc);
		return rc;
	}

	if (dev->ops->tf_dev_free_tbl_scope) {
		rc = dev->ops->tf_dev_free_tbl_scope(tfp, parms);
	} else {
		netdev_dbg(bp->dev,
			   "Free table scope not supported by device\n");
		return -EINVAL;
	}

	return rc;
}

int tf_set_if_tbl_entry(struct tf *tfp,
			struct tf_set_if_tbl_entry_parms *parms)
{
	struct tf_if_tbl_set_parms sparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	if (!dev->ops->tf_dev_set_if_tbl) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	sparms.dir = parms->dir;
	sparms.type = parms->type;
	sparms.idx = parms->idx;
	sparms.data_sz_in_bytes = parms->data_sz_in_bytes;
	sparms.data = parms->data;

	rc = dev->ops->tf_dev_set_if_tbl(tfp, &sparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: If_tbl set failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	return 0;
}

int tf_get_if_tbl_entry(struct tf *tfp,
			struct tf_get_if_tbl_entry_parms *parms)
{
	struct tf_if_tbl_get_parms gparms = { 0 };
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	if (!dev->ops->tf_dev_get_if_tbl) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	gparms.dir = parms->dir;
	gparms.type = parms->type;
	gparms.idx = parms->idx;
	gparms.data_sz_in_bytes = parms->data_sz_in_bytes;
	gparms.data = parms->data;

	rc = dev->ops->tf_dev_get_if_tbl(tfp, &gparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: If_tbl get failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), rc);
		return rc;
	}

	return 0;
}

int tf_get_session_info(struct tf *tfp,
			struct tf_get_session_info_parms *parms)
{
	struct bnxt *bp;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup session, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Failed to lookup device, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	if (!dev->ops->tf_dev_get_ident_resc_info) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev,
			   "%s: get_ident_resc_info unsupported, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = dev->ops->tf_dev_get_ident_resc_info(tfp,
						  parms->session_info.ident);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Ident get resc info failed, rc:%d\n",
			   __func__, rc);
	}

	if (!dev->ops->tf_dev_get_tbl_resc_info) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev,
			   "%s: get_tbl_resc_info unsupported, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = dev->ops->tf_dev_get_tbl_resc_info(tfp, parms->session_info.tbl);
	if (rc) {
		netdev_dbg(bp->dev, "%s: Tbl get resc info failed, rc:%d\n",
			   __func__, rc);
	}

	if (!dev->ops->tf_dev_get_tcam_resc_info) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev,
			   "%s: get_tcam_resc_info unsupported, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = dev->ops->tf_dev_get_tcam_resc_info(tfp,
						 parms->session_info.tcam);
	if (rc) {
		netdev_dbg(bp->dev, "%s: TCAM get resc info failed, rc:%d\n",
			   __func__, rc);
	}

	if (!dev->ops->tf_dev_get_em_resc_info) {
		rc = -EOPNOTSUPP;
		netdev_dbg(bp->dev,
			   "%s: get_em_resc_info unsupported, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	rc = dev->ops->tf_dev_get_em_resc_info(tfp, parms->session_info.em);
	if (rc) {
		netdev_dbg(bp->dev, "%s: EM get resc info failed, rc:%d\n",
			   __func__, rc);
	}

	return 0;
}

int tf_get_version(struct tf *tfp,
		   struct tf_get_version_parms *parms)
{
	struct tf_dev_info dev;
	struct bnxt *bp;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* This function can be called before open session, filter
	 * out any non-supported device types on the Core side.
	 */
	if (parms->device_type != TF_DEVICE_TYPE_P4 &&
	    parms->device_type != TF_DEVICE_TYPE_P5) {
		netdev_dbg(bp->dev,
			   "Unsupported device type %d\n",
			   parms->device_type);
		return -EOPNOTSUPP;
	}

	tf_dev_bind_ops(parms->device_type, &dev);

	rc = tf_msg_get_version(parms->bp, &dev, parms);
	if (rc)
		return rc;

	return 0;
}

int tf_query_sram_resources(struct tf *tfp,
			    struct tf_query_sram_resources_parms *parms)
{
	enum tf_rm_resc_resv_strategy resv_strategy;
	struct tf_rm_resc_req_entry *query;
	struct bnxt *bp;
	struct tf_dev_info dev;
	u16 max_types;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* This function can be called before open session, filter
	 * out any non-supported device types on the Core side.
	 */
	if (parms->device_type != TF_DEVICE_TYPE_P5) {
		netdev_dbg(bp->dev, "Unsupported device type %d\n",
			   parms->device_type);
		return -EINVAL;
	}

	tf_dev_bind_ops(parms->device_type, &dev);

	if (!dev.ops->tf_dev_get_max_types) {
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), EOPNOTSUPP);
		return -EOPNOTSUPP;
	}

	/* Need device max number of elements for the RM QCAPS */
	rc = dev.ops->tf_dev_get_max_types(tfp, &max_types);
	if (rc) {
		netdev_dbg(bp->dev, "Get SRAM resc info failed, rc:%d\n", rc);
		return rc;
	}

	/* Allocate memory for RM QCAPS request */
	query = vzalloc(max_types * sizeof(*query));
	tfp->bp = parms->bp;

	/* Get Firmware Capabilities */
	rc = tf_msg_session_resc_qcaps(tfp, parms->dir, max_types, query,
				       &resv_strategy, &parms->sram_profile);
	if (rc)
		goto end;

	if (!dev.ops->tf_dev_get_sram_resources) {
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), EOPNOTSUPP);
		rc = -EOPNOTSUPP;
		goto end;
	}

	rc = dev.ops->tf_dev_get_sram_resources((void *)query,
						parms->bank_resc_count,
						&parms->dynamic_sram_capable);
	if (rc)
		netdev_dbg(bp->dev, "Get SRAM resc info failed, rc:%d\n", rc);

 end:
	vfree(query);
	return rc;
}

int tf_set_sram_policy(struct tf *tfp, struct tf_set_sram_policy_parms *parms)
{
	struct bnxt *bp;
	struct tf_dev_info dev;
	int rc = 0;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* This function can be called before open session, filter
	 * out any non-supported device types on the Core side.
	 */
	if (parms->device_type != TF_DEVICE_TYPE_P5) {
		netdev_dbg(bp->dev, "%s: Unsupported device type %d\n",
			   __func__, parms->device_type);
		return -EINVAL;
	}

	tf_dev_bind_ops(parms->device_type, &dev);

	if (!dev.ops->tf_dev_set_sram_policy) {
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), EOPNOTSUPP);
		return -EOPNOTSUPP;
	}

	rc = dev.ops->tf_dev_set_sram_policy(parms->dir, parms->bank_id);
	if (rc) {
		netdev_dbg(bp->dev, "%s: SRAM policy set failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), -rc);
		return rc;
	}

	return rc;
}

int tf_get_sram_policy(struct tf *tfp, struct tf_get_sram_policy_parms *parms)
{
	struct bnxt *bp;
	struct tf_dev_info dev;
	int rc = 0;

	if (!tfp || !parms)
		return -EINVAL;

	bp = tfp->bp;

	/* This function can be called before open session, filter
	 * out any non-supported device types on the Core side.
	 */
	if (parms->device_type != TF_DEVICE_TYPE_P5) {
		netdev_dbg(bp->dev, "%s: Unsupported device type %d\n",
			   __func__, parms->device_type);
		return -EINVAL;
	}

	tf_dev_bind_ops(parms->device_type, &dev);

	if (!dev.ops->tf_dev_get_sram_policy) {
		netdev_dbg(bp->dev, "%s: Operation not supported, rc:%d\n",
			   tf_dir_2_str(parms->dir), EOPNOTSUPP);
		return -EOPNOTSUPP;
	}

	rc = dev.ops->tf_dev_get_sram_policy(parms->dir, parms->bank_id);
	if (rc) {
		netdev_dbg(bp->dev, "%s: SRAM policy get failed, rc:%d\n",
			   tf_dir_2_str(parms->dir), -rc);
		return rc;
	}

	return rc;
}
