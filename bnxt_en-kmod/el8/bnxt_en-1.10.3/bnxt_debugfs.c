/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2017-2018 Broadcom Limited
 * Copyright (c) 2018-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/pci.h>
#include "bnxt_hsi.h"
#include "bnxt_compat.h"
#ifdef HAVE_NETDEV_RX_Q_RESTART
#include <net/netdev_rx_queue.h>
#endif
#ifdef HAVE_DIM
#include <linux/dim.h>
#else
#include "bnxt_dim.h"
#endif
#include "bnxt.h"
#include "bnxt_debugfs.h"
#include "bnxt_hdbr.h"
#include "bnxt_udcc.h"
#include "cfa_types.h"
#include "bnxt_vfr.h"
#include "bnxt_coredump.h"

#ifdef CONFIG_DEBUG_FS

static struct dentry *bnxt_debug_mnt;
static struct dentry *bnxt_debug_tf;

static ssize_t debugfs_dim_read(struct file *filep,
				char __user *buffer,
				size_t count, loff_t *ppos)
{
	struct dim *dim = filep->private_data;
	int len;
	char *buf;

	if (*ppos)
		return 0;
	if (!dim)
		return -ENODEV;
	buf = kasprintf(GFP_KERNEL,
			"state = %d\n" \
			"profile_ix = %d\n" \
			"mode = %d\n" \
			"tune_state = %d\n" \
			"steps_right = %d\n" \
			"steps_left = %d\n" \
			"tired = %d\n",
			dim->state,
			dim->profile_ix,
			dim->mode,
			dim->tune_state,
			dim->steps_right,
			dim->steps_left,
			dim->tired);
	if (!buf)
		return -ENOMEM;
	if (count < strlen(buf)) {
		kfree(buf);
		return -ENOSPC;
	}
	len = simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
	kfree(buf);
	return len;
}

static const struct file_operations debugfs_dim_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = debugfs_dim_read,
};

#if defined(HAVE_NETDEV_QMGMT_OPS) && defined(HAVE_QMGMT_OPS_IN_NETDEV_H)
static ssize_t debugfs_reset_rx_write(struct file *filep,
				      const char __user *buffer,
				      size_t count, loff_t *ppos)
{
	unsigned int ring_nr;
	struct bnxt *bp;
	char buf[10];
	ssize_t ret;
	int rc;

	if (*ppos != 0)
		return 0;

	if (count >= sizeof(buf))
		return -ENOSPC;

	ret = copy_from_user(buf, buffer, count);
	if (ret)
		return -EFAULT;
	buf[count] = '\0';

	sscanf(buf, "%u", &ring_nr);

	bp = filep->private_data;
	if (ring_nr > bp->rx_nr_rings)
		return -EINVAL;

	rtnl_lock();
	rc = netdev_rx_queue_restart(bp->dev, ring_nr);
	rtnl_unlock();
	if (rc)
		return rc;

	return count;
}

static const struct file_operations debugfs_reset_rx_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = debugfs_reset_rx_write,
};
#endif

static void debugfs_dim_ring_init(struct dim *dim, int ring_idx,
				  struct dentry *dd)
{
	static char qname[12];

	snprintf(qname, sizeof(qname), "%d", ring_idx);
	debugfs_create_file(qname, 0600, dd, dim, &debugfs_dim_fops);
}

static int dbr_enable_get(void *data, u64 *val)
{
	struct bnxt *bp = data;

	*val = bp->dbr.enable;
	return 0;
}

static int dbr_enable_set(void *data, u64 val)
{
	struct bnxt *bp = data;
	struct bnxt_dbr *dbr;
	int rc;

	dbr = &bp->dbr;

	if (val) {
		dbr->enable = 1;
		rc = bnxt_dbr_init(bp);
		if (rc) {
			netdev_err(bp->dev,
				   "Failed to initialize DB recovery\n");
			dbr->enable = 0;
			return rc;
		}
	} else {
		dbr->enable = 0;
		bnxt_dbr_exit(bp);
	}

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(dbr_enable_fops, dbr_enable_get, dbr_enable_set,
			 "%llu\n");

static ssize_t dbr_stats_read(struct file *filep, char __user *buffer,
size_t count, loff_t *ppos)
{
	struct bnxt_dbr_sw_stats *stat = filep->private_data;
	char *buf;
	int len;

	if (*ppos)
		return 0;

	buf = kasprintf(GFP_KERNEL, "nr_dbr = %u\n" "avg_dbr_us = %llu\n" \
			"max_dbr_us = %llu\n" "min_dbr_us = %llu\n",
			stat->nr_dbr, stat->avg_dbr_us, stat->max_dbr_us,
			stat->min_dbr_us);
	if (!buf)
		return -ENOMEM;

	if (count < strlen(buf)) {
		kfree(buf);
		return -ENOSPC;
	}

	len = simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
	kfree(buf);

	return len;
}

static const struct file_operations dbr_stats_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = dbr_stats_read,
};

static int dbr_test_recover_enable_get(void *data, u64 *val)
{
	struct bnxt *bp = data;

	*val = bp->dbr.debug.recover_enable;
	return 0;
}

static int dbr_test_recover_enable_set(void *data, u64 val)
{
	struct bnxt_dbr_debug *debug;
	struct bnxt *bp = data;
	struct bnxt_dbr *dbr;

	dbr = &bp->dbr;
	debug = &dbr->debug;

	if (!dbr->enable && val) {
		netdev_err(bp->dev,
			   "Unable to run DB recovery test when DBR is disabled\n");
		return -EINVAL;
	}

	if (val) {
		debug->recover_enable = 1;
		if (dbr->wq)
			/* kick start the recovery work */
			if (queue_delayed_work(dbr->wq, &dbr->dwork,
					       msecs_to_jiffies(debug->recover_interval_ms)))
			    atomic_inc(&dbr->event_cnt);
	} else {
		debug->recover_enable = 0;
	}

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(dbr_test_recover_enable_fops,
			 dbr_test_recover_enable_get,
			 dbr_test_recover_enable_set,
			 "%llu\n");

static ssize_t hdbr_debug_trace_read(struct file *filep, char __user *buffer,
				     size_t count, loff_t *ppos)
{
	struct bnxt *bp = filep->private_data;
	int len = 2;
	char buf[2];

	if (*ppos)
		return 0;
	if (!bp)
		return -ENODEV;
	if (count < len)
		return -ENOSPC;

	if (bp->hdbr_info.debug_trace)
		buf[0] = '1';
	else
		buf[0] = '0';
	buf[1] = '\n';

	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

static ssize_t hdbr_debug_trace_write(struct file *file, const char __user *u,
				      size_t size, loff_t *off)
{
	struct bnxt *bp = file->private_data;
	char u_in[2];
	size_t n;

	if (!bp)
		return -ENODEV;
	if (*off || !size || size > 2)
		return -EFAULT;

	n = simple_write_to_buffer(u_in, size, off, u, 2);
	if (n != size)
		return -EFAULT;

	if (u_in[0] == '0')
		bp->hdbr_info.debug_trace = 0;
	else
		bp->hdbr_info.debug_trace = 1;

	return size;
}

static const struct file_operations hdbr_debug_trace_fops = {
	.owner	= THIS_MODULE,
	.open	= simple_open,
	.read	= hdbr_debug_trace_read,
	.write	= hdbr_debug_trace_write,
};

static ssize_t debugfs_hdbr_kdmp_read(struct file *filep, char __user *buffer,
				      size_t count, loff_t *ppos)
{
	struct bnxt_hdbr_ktbl *ktbl = *((void **)filep->private_data);
	size_t len;
	char *buf;

	if (*ppos)
		return 0;
	if (!ktbl)
		return -ENODEV;

	buf = bnxt_hdbr_ktbl_dump(ktbl);
	if (!buf)
		return -ENOMEM;
	len = strlen(buf);
	if (count < len) {
		kfree(buf);
		return -ENOSPC;
	}
	len = simple_read_from_buffer(buffer, count, ppos, buf, len);
	kfree(buf);
	return len;
}

static const struct file_operations hdbr_kdmp_fops = {
	.owner	= THIS_MODULE,
	.open	= simple_open,
	.read	= debugfs_hdbr_kdmp_read,
};

static ssize_t debugfs_hdbr_l2dmp_read(struct file *filep, char __user *buffer,
				       size_t count, loff_t *ppos)
{
	struct bnxt_hdbr_l2_pgs *l2pgs = *((void **)filep->private_data);
	size_t len;
	char *buf;

	if (*ppos)
		return 0;
	if (!l2pgs)
		return -ENODEV;

	buf = bnxt_hdbr_l2pg_dump(l2pgs);
	if (!buf)
		return -ENOMEM;
	len = strlen(buf);
	if (count < len) {
		kfree(buf);
		return -ENOSPC;
	}
	len = simple_read_from_buffer(buffer, count, ppos, buf, len);
	kfree(buf);
	return len;
}

static const struct file_operations hdbr_l2dmp_fops = {
	.owner	= THIS_MODULE,
	.open	= simple_open,
	.read	= debugfs_hdbr_l2dmp_read,
};

void bnxt_debugfs_hdbr_init(struct bnxt *bp)
{
	char *names[4] = {"sq", "rq", "srq", "cq"};
	struct dentry *pdevf, *pktbl, *pl2pgs;
	int i;

	if (!bp->hdbr_info.hdbr_enabled)
		return;

	if (!bp->debugfs_pdev)
		return;

	/* Create top dir */
	bp->debugfs_hdbr = debugfs_create_dir("hdbr", bp->debugfs_pdev);

	/* Create debug_trace knob */
	pdevf = debugfs_create_file("debug_trace", 0644, bp->debugfs_hdbr, bp,
				    &hdbr_debug_trace_fops);

	/* Create ktbl dir */
	pktbl = debugfs_create_dir("ktbl", bp->debugfs_hdbr);

	/* Create l2pgs dir */
	pl2pgs = debugfs_create_dir("l2pgs", bp->debugfs_hdbr);

	/* Create hdbr kernel page and L2 page dumping knobs */
	for (i = 0; i < DBC_GROUP_MAX; i++) {
		pdevf = debugfs_create_file(names[i], 0644, pktbl, &bp->hdbr_info.ktbl[i],
					    &hdbr_kdmp_fops);
		if (i == DBC_GROUP_RQ)
			continue;
		pdevf = debugfs_create_file(names[i], 0644, pl2pgs, &bp->hdbr_pgs[i][0],
					    &hdbr_l2dmp_fops);
	}
}

void bnxt_debugfs_hdbr_delete(struct bnxt *bp)
{
	debugfs_remove_recursive(bp->debugfs_hdbr);
	bp->debugfs_hdbr = NULL;
}

static ssize_t debugfs_bs_trace_read(struct file *filep, char __user *buffer,
				     size_t count, loff_t *ppos)
{
	struct bnxt_bs_trace_info *bs_trace;
	size_t len = 0;

	bs_trace = (struct bnxt_bs_trace_info *)filep->private_data;
	if (*ppos == 0)
		bnxt_bs_trace_dbgfs_copy(bs_trace);
	if (bs_trace->dbgfs_trace)
		len = simple_read_from_buffer(buffer, count, ppos,
					      bs_trace->dbgfs_trace,
					      bs_trace->dbgfs_trace_size);
	return len;
}

static const struct file_operations bs_trace_fops = {
	.owner  = THIS_MODULE,
	.open   = simple_open,
	.read   = debugfs_bs_trace_read,
};

static void bnxt_debugfs_bs_trace_init(struct bnxt *bp)
{
	struct dentry *bs_trace, *bs_trace_entry;
	struct bnxt_ctx_mem_info *ctx = bp->ctx;
	const char *pname = pci_name(bp->pdev);
	struct bnxt_ctx_mem_type *ctxm;
	char *trace;
	int type;

	bs_trace = debugfs_create_dir("bs_trace", bp->debugfs_pdev);
	if (!bs_trace) {
		pr_err("Failed to create debugfs entry bs_trace %s/\n", pname);
		return;
	}

	for (type = BNXT_CTX_SRT_TRACE; type <= BNXT_CTX_ERR_QPC_TRACE; type++) {
		u16 trace_type;

		ctxm = &ctx->ctx_arr[type];
		if (!(ctxm->flags & BNXT_CTX_MEM_TYPE_VALID) || !ctxm->mem_valid ||
		    !(ctxm->flags & FUNC_BACKING_STORE_QCAPS_V2_RESP_FLAGS_FW_DBG_TRACE))
			continue;
		trace_type = bnxt_bstore_to_trace[type];
		trace = (char *)bnxt_trace_to_dbgfs_file[trace_type];
		bs_trace_entry = debugfs_create_file(trace, 0644, bs_trace,
						     &bp->bs_trace[trace_type], &bs_trace_fops);
		if (!bs_trace_entry) {
			pr_err("Failed to create debugfs entry %s/bs_trace\n",
			       trace);
			return;
		}
	}
}

#define BNXT_DEBUGFS_TRUFLOW "truflow"

int bnxt_debug_tf_create(struct bnxt *bp, u8 tsid)
{
	char name[32];
	struct dentry *port_dir;

	bnxt_debug_tf = debugfs_lookup(BNXT_DEBUGFS_TRUFLOW, bnxt_debug_mnt);

	if (!bnxt_debug_tf)
		return -ENODEV;

	/* If not there create the port # directory */
	sprintf(name, "%d", bp->pf.port_id);
	port_dir = debugfs_lookup(name, bnxt_debug_tf);

	if (!port_dir) {
		port_dir = debugfs_create_dir(name, bnxt_debug_tf);
		if (!port_dir) {
			pr_debug("Failed to create TF debugfs port %d directory.\n",
				 bp->pf.port_id);
			return -ENODEV;
		}
	}
	/* Call TF function to create the table scope debugfs seq files */
	bnxt_tf_debugfs_create_files(bp, tsid, port_dir);

	return 0;
}

void bnxt_debug_tf_delete(struct bnxt *bp)
{
	char name[32];
	struct dentry *port_dir;

	if (!bnxt_debug_tf)
		return;

	sprintf(name, "%d", bp->pf.port_id);
	port_dir = debugfs_lookup(name, bnxt_debug_tf);
	if (port_dir)
		debugfs_remove_recursive(port_dir);
}

void bnxt_create_debug_dim_dbr(struct bnxt *bp)
{
	struct bnxt_dbr_debug *debug;
	struct bnxt_dbr *dbr;
	int i;

	bp->debugfs_dim = debugfs_create_dir("dim", bp->debugfs_pdev);

	/* Create files for each rx ring */
	for (i = 0; i < bp->cp_nr_rings; i++) {
		struct bnxt_cp_ring_info *cpr = &bp->bnapi[i]->cp_ring;

		if (cpr && bp->bnapi[i]->rx_ring)
			debugfs_dim_ring_init(&cpr->dim, i, bp->debugfs_dim);
	}

#define DBR_TEST_RECOVER_INTERVAL_MS 1000
#define DBR_TEST_DROP_RATIO          10
	dbr = &bp->dbr;
	debug = &bp->dbr.debug;

	debug->recover_interval_ms = DBR_TEST_RECOVER_INTERVAL_MS;
	debug->drop_ratio = DBR_TEST_DROP_RATIO;

	bp->debugfs_dbr = debugfs_create_dir("dbr", bp->debugfs_pdev);
	debugfs_create_file("dbr_enable", 0644, bp->debugfs_dbr, bp, &dbr_enable_fops);
	debugfs_create_file("dbr_stats", 0444, bp->debugfs_dbr, &dbr->sw_stats,
			    &dbr_stats_fops);
#ifdef DBR_DBG_DROP_ENABLE
	debugfs_create_u8("dbr_test_drop_enable", 0644, bp->debugfs_dbr,
			  &debug->drop_enable);
	debugfs_create_u32("dbr_test_drop_ratio", 0644, bp->debugfs_dbr,
			   &debug->drop_ratio);
#endif
	debugfs_create_file("dbr_test_recover_enable", 0644, bp->debugfs_dbr, bp,
			    &dbr_test_recover_enable_fops);
	debugfs_create_u32("dbr_test_recover_interval_ms", 0644, bp->debugfs_dbr,
			   &debug->recover_interval_ms);
}

void bnxt_debug_dev_init(struct bnxt *bp)
{
	const char *pname = pci_name(bp->pdev);

	bp->debugfs_pdev = debugfs_create_dir(pname, bnxt_debug_mnt);

	bnxt_debugfs_hdbr_init(bp);

	if (bnxt_bs_trace_dbgfs_available(bp))
		bnxt_debugfs_bs_trace_init(bp);

#if defined(HAVE_NETDEV_QMGMT_OPS) && defined(HAVE_QMGMT_OPS_IN_NETDEV_H)
	debugfs_create_file("reset_rx", 0600, bp->debugfs_pdev, bp, &debugfs_reset_rx_fops);
#endif
}

void bnxt_delete_debug_dim_dbr(struct bnxt *bp)
{
	struct bnxt_dbr_debug *debug = &bp->dbr.debug;

	if (!bp)
		return;

	memset(debug, 0, sizeof(*debug));
	debugfs_remove_recursive(bp->debugfs_dbr);
	bp->debugfs_dbr = NULL;

	debugfs_remove_recursive(bp->debugfs_dim);
	bp->debugfs_dim = NULL;
}

void bnxt_debug_dev_exit(struct bnxt *bp)
{
	debugfs_remove_recursive(bp->debugfs_pdev);
	bp->debugfs_pdev = NULL;
	bp->debugfs_dim = NULL;
	bp->debugfs_dbr = NULL;
	bp->debugfs_hdbr = NULL;
}

void bnxt_debug_init(void)
{
	bnxt_debug_mnt = debugfs_create_dir("bnxt_en", NULL);
	if (!bnxt_debug_mnt) {
		pr_err("failed to init bnxt_en debugfs\n");
		return;
	}

	bnxt_debug_tf = debugfs_create_dir(BNXT_DEBUGFS_TRUFLOW,
					   bnxt_debug_mnt);

	if (!bnxt_debug_tf)
		pr_err("Failed to create TF debugfs backingstore directory.\n");
}

void bnxt_debug_exit(void)
{
	/* Remove subdirectories.  Older kernels have bug in remove for 2 level
	 * directories.
	 */
	debugfs_remove_recursive(bnxt_debug_tf);
	debugfs_remove_recursive(bnxt_debug_mnt);
}

#endif /* CONFIG_DEBUG_FS */
