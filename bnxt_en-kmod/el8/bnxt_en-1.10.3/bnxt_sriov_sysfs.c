/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2014, Mellanox Technologies inc. All rights reserved.
 * Copyright (c) 2023-2024 Broadcom Inc.
 *
 *  This software is available to you under a choice of one of two
 *  licenses. You may choose to be licensed under the terms of the GNU
 *  General Public License (GPL) Version 2, available from the file
 *  COPYING in the main directory of this source tree, or the
 *  OpenIB.org BSD license below:
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * - Redistributions of source code must retain the above
 *  copyright notice, this list of conditions and the following
 *  disclaimer.
 *
 * - Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following
 *  disclaimer in the documentation and/or other materials
 *  provided with the distribution.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 *  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 *  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 *  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 */

#include <linux/sysfs.h>
#include <linux/module.h>
#include "bnxt_hsi.h"
#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_sriov_sysfs.h"

struct vf_attributes {
	struct attribute attr;
	ssize_t (*show)(struct bnxt_vf_sysfs_obj *vf_so, struct vf_attributes *vfa,
			char *buf);
	ssize_t (*store)(struct bnxt_vf_sysfs_obj *vf_so, struct vf_attributes *vfa,
			 const char *buf, size_t count);
};

static ssize_t vf_attr_show(struct kobject *kobj,
			    struct attribute *attr, char *buf)
{
	struct vf_attributes *ga =
		container_of(attr, struct vf_attributes, attr);
	struct bnxt_vf_sysfs_obj *g = container_of(kobj, struct bnxt_vf_sysfs_obj, kobj);

	if (!ga->show)
		return -EIO;

	return ga->show(g, ga, buf);
}

static ssize_t vf_attr_store(struct kobject *kobj,
			     struct attribute *attr,
			     const char *buf, size_t size)
{
	struct vf_attributes *ga =
		container_of(attr, struct vf_attributes, attr);
	struct bnxt_vf_sysfs_obj *g = container_of(kobj, struct bnxt_vf_sysfs_obj, kobj);

	if (!ga->store)
		return -EIO;

	return ga->store(g, ga, buf, size);
}

#define _sprintf(p, buf, format, arg...)                                \
	((PAGE_SIZE - (int)((p) - (buf))) <= 0 ? 0 :                        \
	scnprintf((p), PAGE_SIZE - (int)((p) - (buf)), format, ## arg))

static ssize_t stats_show(struct bnxt_vf_sysfs_obj *g, struct vf_attributes *oa,
			  char *buf)
{
	struct bnxt_stats_mem *stats = &g->stats;
	struct bnxt *bp = g->parent_pf_bp;
	struct ctx_hw_stats *hw_stats;
	u64 rx_dropped, tx_dropped;
	u64 rx_packets, rx_bytes;
	u64 tx_packets, tx_bytes;
	char *p = buf;
	int rc;

	memset(stats->hw_stats, 0, stats->len);

	mutex_lock(&bp->sriov_lock);
	rc = bnxt_hwrm_func_qstats(bp, stats,
				   cpu_to_le16(g->fw_fid), 0);

	if (rc) {
		mutex_unlock(&bp->sriov_lock);
		return rc;
	}

	hw_stats = stats->hw_stats;

	rx_packets = hw_stats->rx_ucast_pkts + hw_stats->rx_mcast_pkts + hw_stats->rx_bcast_pkts;
	rx_bytes = hw_stats->rx_ucast_bytes + hw_stats->rx_mcast_bytes + hw_stats->rx_bcast_bytes;

	tx_packets = hw_stats->tx_ucast_pkts + hw_stats->tx_mcast_pkts + hw_stats->tx_bcast_pkts;
	tx_bytes = hw_stats->tx_ucast_bytes + hw_stats->tx_mcast_bytes + hw_stats->tx_bcast_bytes;

	rx_dropped = hw_stats->rx_error_pkts;
	tx_dropped = hw_stats->tx_error_pkts;

	p += _sprintf(p, buf, "tx_packets    : %llu\n", tx_packets);
	p += _sprintf(p, buf, "tx_bytes      : %llu\n", tx_bytes);
	p += _sprintf(p, buf, "tx_dropped    : %llu\n", tx_dropped);
	p += _sprintf(p, buf, "rx_packets    : %llu\n", rx_packets);
	p += _sprintf(p, buf, "rx_bytes      : %llu\n", rx_bytes);
	p += _sprintf(p, buf, "rx_dropped    : %llu\n", rx_dropped);
	p += _sprintf(p, buf, "rx_multicast  : %llu\n", hw_stats->rx_mcast_pkts);
	p += _sprintf(p, buf, "rx_broadcast  : %llu\n", hw_stats->rx_bcast_pkts);
	p += _sprintf(p, buf, "tx_broadcast  : %llu\n", hw_stats->tx_bcast_pkts);
	p += _sprintf(p, buf, "tx_multicast  : %llu\n", hw_stats->tx_mcast_pkts);

	mutex_unlock(&bp->sriov_lock);
	return (ssize_t)(p - buf);
}

#define VF_ATTR(_name) struct vf_attributes vf_attr_##_name = \
	__ATTR(_name, 0644, _name##_show, NULL)

VF_ATTR(stats);

static struct attribute *vf_eth_attrs[] = {
	&vf_attr_stats.attr,
	NULL
};

#ifdef HAVE_KOBJ_DEFAULT_GROUPS
ATTRIBUTE_GROUPS(vf_eth);
#endif

static const struct sysfs_ops vf_sysfs_ops = {
	.show = vf_attr_show,
	.store = vf_attr_store,
};

static struct kobj_type vf_type_eth = {
	.sysfs_ops     = &vf_sysfs_ops,
#ifdef HAVE_KOBJ_DEFAULT_GROUPS
	.default_groups = vf_eth_groups
#else
	.default_attrs = vf_eth_attrs
#endif
};

int bnxt_sriov_sysfs_init(struct bnxt *bp)
{
	struct device *dev = &bp->pdev->dev;

	bp->sriov_sysfs_config = kobject_create_and_add("sriov", &dev->kobj);
	if (!bp->sriov_sysfs_config)
		return -ENOMEM;

	return 0;
}

void bnxt_sriov_sysfs_exit(struct bnxt *bp)
{
	kobject_put(bp->sriov_sysfs_config);
	bp->sriov_sysfs_config = NULL;
}

int bnxt_create_vfs_sysfs(struct bnxt *bp)
{
	struct bnxt_vf_sysfs_obj *vf_obj;
	static struct kobj_type *sysfs;
	struct bnxt_vf_info *vfs, *tmp;
	struct bnxt_stats_mem *stats;
	int err;
	int vf;

	sysfs = &vf_type_eth;

	bp->vf_sysfs_objs = kcalloc(bp->pf.active_vfs, sizeof(struct bnxt_vf_sysfs_obj),
				    GFP_KERNEL);
	if (!bp->vf_sysfs_objs)
		return -ENOMEM;

	mutex_lock(&bp->sriov_lock);
	vfs = rcu_dereference_protected(bp->pf.vf,
					lockdep_is_held(&bp->sriov_lock));

	for (vf = 0; vf < bp->pf.active_vfs; vf++) {
		tmp = &vfs[vf];
		if (!tmp) {
			netdev_warn(bp->dev, "create_vfs_syfs vfs[%d] is NULL\n", vf);
			continue;
		}

		vf_obj = &bp->vf_sysfs_objs[vf];

		vf_obj->parent_pf_bp = bp;
		vf_obj->fw_fid = tmp->fw_fid;

		stats = &vf_obj->stats;

		stats->len = bp->hw_ring_stats_size;
		stats->hw_stats = dma_alloc_coherent(&bp->pdev->dev, stats->len,
						     &stats->hw_stats_map, GFP_KERNEL);

		if (!stats->hw_stats)
			goto err_vf_obj;

		err = kobject_init_and_add(&vf_obj->kobj, sysfs, bp->sriov_sysfs_config,
					   "%d", vf);
		if (err)
			goto err_vf_obj;

		kobject_uevent(&vf_obj->kobj, KOBJ_ADD);
	}
	mutex_unlock(&bp->sriov_lock);
	return 0;

err_vf_obj:
	for (; vf >= 0; vf--) {
		vf_obj = &bp->vf_sysfs_objs[vf];
		stats = &vf_obj->stats;

		if (stats->hw_stats)
			dma_free_coherent(&bp->pdev->dev, stats->len, stats->hw_stats,
					  stats->hw_stats_map);
		stats->hw_stats = NULL;
		if (vf_obj->kobj.state_initialized)
			kobject_put(&vf_obj->kobj);
	}
	kfree(bp->vf_sysfs_objs);
	bp->vf_sysfs_objs = NULL;
	mutex_unlock(&bp->sriov_lock);

	return -ENOMEM;
}

void bnxt_destroy_vfs_sysfs(struct bnxt *bp)
{
	struct bnxt_vf_sysfs_obj *vf_obj;
	struct bnxt_stats_mem *stats;
	int vf;

	mutex_lock(&bp->sriov_lock);

	if (!bp->vf_sysfs_objs)
		goto destroy_exit;

	for (vf = 0; vf < bp->pf.active_vfs; vf++) {
		vf_obj = &bp->vf_sysfs_objs[vf];
		stats = &vf_obj->stats;

		if (stats->hw_stats)
			dma_free_coherent(&bp->pdev->dev, stats->len, stats->hw_stats,
					  stats->hw_stats_map);
		stats->hw_stats = NULL;
		kobject_put(&vf_obj->kobj);
	}

	kfree(bp->vf_sysfs_objs);
	bp->vf_sysfs_objs = NULL;

destroy_exit:
	mutex_unlock(&bp->sriov_lock);
}
