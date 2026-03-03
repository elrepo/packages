// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#include <linux/vmalloc.h>
#include <linux/if_ether.h>
#include <linux/atomic.h>
#include <linux/ipv6.h>
#include <linux/in6.h>
#include <linux/err.h>

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_vfr.h"
#include "bnxt_tf_ulp.h"
#include "bnxt_udcc.h"
#include "ulp_udcc.h"
#include "bitalloc.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)

static inline int bnxt_ulp_udcc_v6_subnet_delete(struct bnxt *bp,
						 struct bnxt_ulp_udcc_v6_subnet_node *node)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	int rc = 0;

	if (!node)
		return -EINVAL;

	netdev_dbg(bp->dev, "DEL: fid %d dst %pI6/%pI6 subnet_hndl %d ref %u\n",
		   node->key.src_fid, &node->key.dst, &node->key.dmsk,
		   node->data.subnet_hndl,
		   atomic_read(&node->ref.refs));

	if (refcount_dec_and_test(&node->ref)) {
		rc = bnxt_ba_free(&tc_info->v6_subnet_pool,
				  node->data.subnet_hndl);
		if (rc)
			netdev_err(bp->dev, "UDCC: BA free failed, rc=%d\n", rc);

		rc = rhashtable_remove_fast(&tc_info->v6_subnet_table,
					    &node->node,
					    tc_info->v6_subnet_ht_params);
		if (rc)
			netdev_err(bp->dev, "UDCC: rhash remove failed, rc=%d\n", rc);

		netdev_dbg(bp->dev, "DEL:Y suspend fid %d dst %pI6/%pI6\n",
			   node->key.src_fid, &node->key.dst, &node->key.dmsk);

		kfree(node);
		/* Update the sessions and delete its flows */
		bnxt_udcc_update_session(bp, true);
	}

	return rc;
}

/* Utility to ensure prefix is addr & mask, so if
 * user sends different host but same network addr
 * we are ablel to normalize all of them to network
 *
 * @addr[in]: Subnet IPv6 address
 * @mask[in]: Subnet IPv6 mask
 * @pfx[out]: Returns the prefix
 */
static inline void bnxt_ulp_udcc_v6_addr_prefix(struct in6_addr *pfx,
						struct in6_addr *msk,
						const u8 *addr,
						const u8 *mask)
{
	int i;

	/* Copy the key, AND it with the mask */
	for (i = 0; i < sizeof(pfx->s6_addr); i++)
		pfx->s6_addr[i] = addr[i] & mask[i];
	/* Copy the mask */
	memcpy(msk->s6_addr, mask, sizeof(struct in6_addr));
}

int bnxt_ulp_udcc_v6_subnet_add(struct bnxt *bp,
				u16 *src_fid, u8 *v6dst, u8 *v6msk,
				u8 *dmac, u8 *smac,
				u16 *subnet_hndl)
{
	struct bnxt_ulp_udcc_v6_subnet_node *new_node, *old_node;
	struct bnxt_tc_info *tc_info = bp->tc_info;
	int hndl = -1;
	int rc = 0;

	if (!src_fid || !v6dst || !v6msk || !dmac || !smac || !subnet_hndl)
		return -EINVAL;

	netdev_dbg(bp->dev, "ADD: fid %d dst %pI6/%pI6\n",
		   be16_to_cpu(*src_fid), v6dst, v6msk);
	netdev_dbg(bp->dev, "ADD: dmac %pM smac %pM\n",
		   dmac, smac);

	/* Allocate memory for the new node */
	new_node = kzalloc(sizeof(*new_node), GFP_KERNEL);
	if (!new_node)
		return -ENOMEM;

	/* Setup the KEY */
	bnxt_ulp_udcc_v6_addr_prefix(&new_node->key.dst, &new_node->key.dmsk,
				     v6dst, v6msk);
	/* Ideally we want it to be this be16_to_cpu(*src_fid),
	 * but this application is per PF so use the PF fid
	 */
	new_node->key.src_fid = bp->pf.fw_fid;
	/* setup an invalid handle */
	*subnet_hndl = -1;

	/* This function returns the object if it exists,
	 * NULL if it did not and the insertion was successful,
	 * and an ERR_PTR otherwise.
	 */
	old_node = rhashtable_lookup_get_insert_fast(&tc_info->v6_subnet_table,
						     &new_node->node,
						     tc_info->v6_subnet_ht_params);
	if (IS_ERR(old_node)) {
		rc = PTR_ERR(old_node);
		goto node_free;
	}

	if (old_node) {
		/* if the subnet already exists, but DMAC/SMAC changed */
		if ((!ether_addr_equal(old_node->data.dmac, dmac)) ||
		    (!ether_addr_equal(old_node->data.smac, smac))) {
			netdev_dbg(bp->dev, "OLD dmac %pM smac %pM\n",
				   old_node->data.dmac, old_node->data.smac);
			ether_addr_copy(old_node->data.dmac, dmac);
			ether_addr_copy(old_node->data.smac, smac);
			/* Update the sessions and modify its flows */
			bnxt_udcc_update_session(bp, true);
		}

		/* Increment Refcount and free the new node */
		if (!refcount_inc_not_zero(&old_node->ref))
			netdev_err(bp->dev, "UDCC: incr refcount failed for %pI6\n",
				   v6dst);

		netdev_dbg(bp->dev, "ADD: already exist, inc ref count %u\n",
			   atomic_read(&old_node->ref.refs));
		*subnet_hndl = old_node->data.subnet_hndl;
		goto node_free;

	} else {
		/* Set Refcount to 1 and fill up the data in the new node*/
		refcount_set(&new_node->ref, 1);
		hndl = bnxt_ba_alloc(&tc_info->v6_subnet_pool);
		if (hndl < 0) {
			rc = -ENOMEM;
			netdev_err(bp->dev, "UDCC: BA allocation failed, rc:%d\n", rc);
			goto node_delete_free;
		}
		*subnet_hndl = (u16)hndl;
		new_node->data.subnet_hndl = *subnet_hndl;
		ether_addr_copy(new_node->data.dmac, dmac);
		ether_addr_copy(new_node->data.smac, smac);
		netdev_dbg(bp->dev, "ADD:Y unsuspend key_fid %d, entry subnet_hndl %d, ref count %u\n",
			   new_node->key.src_fid,
			   new_node->data.subnet_hndl,
			   atomic_read(&new_node->ref.refs));

		/* Update the sessions and modify its flows */
		bnxt_udcc_update_session(bp, false);
	}
	return rc;

node_delete_free:
	bnxt_ulp_udcc_v6_subnet_delete(bp, new_node);
	return rc;
node_free:
	kfree(new_node);
	return rc;
}

int bnxt_ulp_udcc_v6_subnet_del(struct bnxt *bp, u16 subnet_hndl)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_ulp_udcc_v6_subnet_node *node;
	struct rhashtable_iter iter;
	int rc = 0;

	if (subnet_hndl > BNXT_ULP_MAX_V6_SUBNETS)
		return -EINVAL;

	netdev_dbg(bp->dev, "DEL HNDL: subnet_hndl %u\n", subnet_hndl);
	rhashtable_walk_enter(&tc_info->v6_subnet_table, &iter);
	rhashtable_walk_start(&iter);
	while ((node = rhashtable_walk_next(&iter)) != NULL) {
		if (IS_ERR(node))
			continue;
		if (node->data.subnet_hndl == subnet_hndl) {
			/* Found a subnet that matches the handle*/
			rc = bnxt_ulp_udcc_v6_subnet_delete(bp, node);
			break;
		}
		rc = -ENOENT;
	}
	rhashtable_walk_stop(&iter);
	rhashtable_walk_exit(&iter);

	return rc;
}

static inline bool bnxt_ulp_udcc_v6_subnet_compare(struct bnxt *bp,
						   u16 src_fid,
						   const struct in6_addr *dst,
						   struct bnxt_ulp_udcc_v6_subnet_key *key)
{
	bool found = false;

	if (src_fid != key->src_fid)
		return found;

	found = !ipv6_masked_addr_cmp(&key->dst, &key->dmsk, dst);
	netdev_dbg(bp->dev, "CMP:%s fid %d/%d subnet %pI6/%pI6\n",
		   found ? "Y" : "N",
		   src_fid, key->src_fid, &key->dst, &key->dmsk);

	return found;
}

int bnxt_ulp_udcc_v6_subnet_check(struct bnxt *bp,
				  u16 src_fid,
				  const struct in6_addr *dst,
				  u8 *dmac, u8 *smac)
{
	struct bnxt_tc_info *tc_info = bp->tc_info;
	struct bnxt_ulp_context *ulp_ctx = bp->ulp_ctx;
	struct bnxt_ulp_udcc_v6_subnet_node *node;
	struct rhashtable_iter iter;
	int rc = -ENOENT;

	/* Truflow is not initialized; we should not create session */
	if (!(bp->tf_flags & BNXT_TF_FLAG_INITIALIZED)) {
		netdev_dbg(bp->dev, "Truflow is not initialized\n");
		return -EPERM;
	}

	/* subnets cannot be added in non-switchdev mode so return -ENOENT */
	if (!bnxt_tc_is_switchdev_mode(bp))
		return -ENOENT;

	if (!dst || !dmac || !smac)
		return -EINVAL;

	netdev_dbg(bp->dev, "CHK: fid %d dst %pI6\n", src_fid, dst);
	mutex_lock(&ulp_ctx->cfg_data->flow_db_lock);
	rhashtable_walk_enter(&tc_info->v6_subnet_table, &iter);
	rhashtable_walk_start(&iter);
	while ((node = rhashtable_walk_next(&iter)) != NULL) {
		if (IS_ERR(node))
			continue;
		if (bnxt_ulp_udcc_v6_subnet_compare(bp, src_fid, dst, &node->key)) {
			/* Found a subnet that matches the DIP */
			if (is_valid_ether_addr(node->data.dmac) &&
			    is_valid_ether_addr(node->data.smac)) {
				ether_addr_copy(dmac, node->data.dmac);
				ether_addr_copy(smac, node->data.smac);
				rc = 0;
			} else {
				/* VF to VF case the SMAC/DMAC will be invalid */
				rc = -EPERM;
			}
			break;
		}
	}
	rhashtable_walk_stop(&iter);
	rhashtable_walk_exit(&iter);

	mutex_unlock(&ulp_ctx->cfg_data->flow_db_lock);
	return rc;
}

#endif /* if defined(CONFIG_BNXT_FLOWER_OFFLOAD) */
