// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: Direct Verbs interpreter
 */

#include <linux/uaccess.h>
#include "bnxt_re.h"
#include "bnxt_re-abi.h"

#define UVERBS_MODULE_NAME bnxt_re

#include <rdma/uverbs_types.h>
#include <rdma/uverbs_std_types.h>
#include <rdma/ib_user_ioctl_cmds.h>
#include <rdma/uverbs_named_ioctl.h>

static struct bnxt_re_cq *bnxt_re_search_for_cq(struct bnxt_re_dev *rdev, u32 cq_id,
						struct bnxt_re_ucontext *uctx)
{
	struct bnxt_re_cq *cq = NULL, *tmp_cq;

	mutex_lock(&uctx->cq_lock);
	hash_for_each_possible(rdev->cq_hash, tmp_cq, hash_entry, cq_id) {
		if (tmp_cq->qplib_cq.id == cq_id) {
			cq = tmp_cq;
			break;
		}
	}
	mutex_unlock(&uctx->cq_lock);
	return cq;
}

static struct bnxt_re_srq *bnxt_re_search_for_srq(struct bnxt_re_dev *rdev, u32 srq_id,
						  struct bnxt_re_ucontext *uctx)
{
	struct bnxt_re_srq *srq = NULL, *tmp_srq;

	mutex_lock(&uctx->srq_lock);
	hash_for_each_possible(rdev->srq_hash, tmp_srq, hash_entry, srq_id) {
		if (tmp_srq->qplib_srq.id == srq_id) {
			srq = tmp_srq;
			break;
		}
	}
	mutex_unlock(&uctx->srq_lock);
	return srq;
}

struct bnxt_re_resolve_cb_context {
	struct completion comp;
	int status;
};

static void bnxt_re_resolve_cb(int status, struct sockaddr *src_addr,
			       struct rdma_dev_addr *addr, void *context)
{
	struct bnxt_re_resolve_cb_context *ctx = context;

	ctx->status = status;
	complete(&ctx->comp);
}

static int bnxt_re_resolve_eth_dmac_by_grh(const struct ib_gid_attr *sgid_attr,
					   const union ib_gid *sgid,
					   const union ib_gid *dgid,
					   u8 *dmac, u8 *smac,
					   int *hoplimit)
{
	struct bnxt_re_resolve_cb_context ctx;
	struct rdma_dev_addr dev_addr;
	union {
		struct sockaddr_in  _sockaddr_in;
		struct sockaddr_in6 _sockaddr_in6;
	} sgid_addr, dgid_addr;
	int rc;

	rdma_gid2ip((struct sockaddr *)&sgid_addr, sgid);
	rdma_gid2ip((struct sockaddr *)&dgid_addr, dgid);

	memset(&dev_addr, 0, sizeof(dev_addr));
	dev_addr.sgid_attr = sgid_attr;
	dev_addr.net = &init_net;

	init_completion(&ctx.comp);
	rc = rdma_resolve_ip((struct sockaddr *)&sgid_addr,
			     (struct sockaddr *)&dgid_addr, &dev_addr, 1000,
			     bnxt_re_resolve_cb, true, &ctx);
	if (rc)
		return rc;

	wait_for_completion(&ctx.comp);

	rc = ctx.status;
	if (rc)
		return rc;

	memcpy(dmac, dev_addr.dst_dev_addr, ETH_ALEN);
	memcpy(smac, dev_addr.src_dev_addr, ETH_ALEN);
	*hoplimit = dev_addr.hoplimit;
	return 0;
}

static int bnxt_re_resolve_gid_dmac(struct ib_device *device,
				    struct rdma_ah_attr *ah_attr)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(device, ibdev);
	const struct ib_gid_attr *sgid_attr;
	struct ib_global_route *grh;
	u8 smac[ETH_ALEN] = {};
	int hop_limit = 0xff;
	int rc = 0;

	grh = rdma_ah_retrieve_grh(ah_attr);
	if (!grh)
		return -EINVAL;

	sgid_attr = grh->sgid_attr;
	if (!sgid_attr)
		return -EINVAL;

	/*  Link local destination and RoCEv1 SGID */
	if (rdma_link_local_addr((struct in6_addr *)grh->dgid.raw) &&
	    sgid_attr->gid_type == IB_GID_TYPE_ROCE) {
		rdma_get_ll_mac((struct in6_addr *)grh->dgid.raw,
				ah_attr->roce.dmac);
		return rc;
	}

	dev_dbg(rdev_to_dev(rdev),
		"%s: netdev: %s sgid: %pI6 dgid: %pI6 gid_type: %d gid_index: %d\n",
		__func__, rdev->netdev ? rdev->netdev->name : "NULL",
		&sgid_attr->gid, &grh->dgid, sgid_attr->gid_type, grh->sgid_index);

	rc = bnxt_re_resolve_eth_dmac_by_grh(sgid_attr, &sgid_attr->gid,
					     &grh->dgid, ah_attr->roce.dmac,
					     smac, &hop_limit);
	if (!rc) {
		grh->hop_limit = hop_limit;
		dev_dbg(rdev_to_dev(rdev), "%s: Resolved: dmac: %pM smac: %pM\n",
			__func__, ah_attr->roce.dmac, smac);
	}
	return rc;
}

static int bnxt_re_resolve_eth_dmac(struct ib_device *device,
				    struct rdma_ah_attr *ah_attr)
{
	int rc = 0;

	/* unicast */
	if (!rdma_is_multicast_addr((struct in6_addr *)ah_attr->grh.dgid.raw)) {
		rc = bnxt_re_resolve_gid_dmac(device, ah_attr);
		if (rc) {
			dev_err(&device->dev, "%s: Failed to resolve gid dmac: %d\n",
				__func__, rc);
		}
		return rc;
	}

	/* multicast */
	if (ipv6_addr_v4mapped((struct in6_addr *)ah_attr->grh.dgid.raw)) {
		__be32 addr = 0;

		memcpy(&addr, ah_attr->grh.dgid.raw + 12, 4);
		       ip_eth_mc_map(addr, (char *)ah_attr->roce.dmac);
	} else {
		ipv6_eth_mc_map((struct in6_addr *)ah_attr->grh.dgid.raw,
				(char *)ah_attr->roce.dmac);
	}
	return rc;
}

static int bnxt_re_copyin_ah_attr(struct ib_device *device,
				  struct rdma_ah_attr *dattr,
				  struct ib_uverbs_ah_attr *sattr)
{
	const struct ib_gid_attr *sgid_attr;
	struct ib_global_route *grh;
	int rc;

	dattr->sl		= sattr->sl;
	dattr->static_rate	= sattr->static_rate;
	dattr->port_num		= sattr->port_num;

	if (!sattr->is_global)
		return 0;

	grh = &dattr->grh;
	if (grh->sgid_attr)
		return 0;

	sgid_attr = rdma_get_gid_attr(device, sattr->port_num,
				      sattr->grh.sgid_index);
	if (IS_ERR(sgid_attr))
		return PTR_ERR(sgid_attr);
	grh->sgid_attr = sgid_attr;

	memcpy(&grh->dgid, sattr->grh.dgid, 16);
	grh->flow_label = sattr->grh.flow_label;
	grh->hop_limit = sattr->grh.hop_limit;
	grh->sgid_index = sattr->grh.sgid_index;
	grh->traffic_class = sattr->grh.traffic_class;

	rc = bnxt_re_resolve_eth_dmac(device, dattr);
	if (rc)
		rdma_put_gid_attr(sgid_attr);
	return rc;
}

static int bnxt_re_dv_copy_qp_attr(struct bnxt_re_dev *rdev,
				   struct ib_qp_attr *dst,
				   struct ib_uverbs_qp_attr *src)
{
	int rc;

	if (src->qp_attr_mask & IB_QP_ALT_PATH)
		return -EINVAL;

	dst->qp_state           = src->qp_state;
	dst->cur_qp_state       = src->cur_qp_state;
	dst->path_mtu           = src->path_mtu;
	dst->path_mig_state     = src->path_mig_state;
	dst->qkey               = src->qkey;
	dst->rq_psn             = src->rq_psn;
	dst->sq_psn             = src->sq_psn;
	dst->dest_qp_num        = src->dest_qp_num;
	dst->qp_access_flags    = src->qp_access_flags;

	dst->cap.max_send_wr        = src->max_send_wr;
	dst->cap.max_recv_wr        = src->max_recv_wr;
	dst->cap.max_send_sge       = src->max_send_sge;
	dst->cap.max_recv_sge       = src->max_recv_sge;
	dst->cap.max_inline_data    = src->max_inline_data;

	if (src->qp_attr_mask & IB_QP_AV) {
		rc = bnxt_re_copyin_ah_attr(&rdev->ibdev, &dst->ah_attr,
					    &src->ah_attr);
		if (rc)
			return rc;
	}

	dst->pkey_index         = src->pkey_index;
	dst->alt_pkey_index     = src->alt_pkey_index;
	dst->en_sqd_async_notify = src->en_sqd_async_notify;
	dst->sq_draining        = src->sq_draining;
	dst->max_rd_atomic      = src->max_rd_atomic;
	dst->max_dest_rd_atomic = src->max_dest_rd_atomic;
	dst->min_rnr_timer      = src->min_rnr_timer;
	dst->port_num           = src->port_num;
	dst->timeout            = src->timeout;
	dst->retry_cnt          = src->retry_cnt;
	dst->rnr_retry          = src->rnr_retry;
	dst->alt_port_num       = src->alt_port_num;
	dst->alt_timeout        = src->alt_timeout;

	return 0;
}

static int bnxt_re_dv_modify_qp_v1(struct ib_uobject *uobj,
				   struct uverbs_attr_bundle *attrs,
				   enum bnxt_re_dv_modify_qp_type type)
{
	struct bnxt_re_qp *qp;
	uint32_t sport;
	int err;

	qp = to_bnxt_re(uobj->object, struct bnxt_re_qp, ib_qp);

	switch (type) {
	case BNXT_RE_DV_MODIFY_QP_UDP_SPORT:
		err = uverbs_copy_from(&sport, attrs, BNXT_RE_DV_MODIFY_QP_VALUE);
		if (err)
			return err;
		err = bnxt_re_modify_udp_sport(qp, sport);
		if (err)
			return err;
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static int bnxt_re_dv_modify_qp_v2(struct ib_uobject *uobj,
				   struct uverbs_attr_bundle *attrs)
{
	struct ib_uverbs_qp_attr qp_u_attr = {};
	struct bnxt_re_ucontext *re_uctx;
	struct ib_qp_attr qp_attr = {};
	struct ib_ucontext *ib_uctx;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_qp *qp;
	int err;

	qp = uobj->object;

	ib_uctx = ib_uverbs_get_ucontext(attrs);
	if (IS_ERR(ib_uctx))
		return PTR_ERR(ib_uctx);

	re_uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	rdev = re_uctx->rdev;

	err = uverbs_copy_from_or_zero(&qp_u_attr, attrs, BNXT_RE_DV_MODIFY_QP_REQ);
	if (err) {
		dev_err(rdev_to_dev(rdev), "%s: uverbs_copy_from() failed: %d\n",
			__func__, err);
		return err;
	}

	err = bnxt_re_dv_copy_qp_attr(rdev, &qp_attr, &qp_u_attr);
	if (err) {
		dev_err(rdev_to_dev(rdev), "%s: Failed to copy qp_u_attr: %d\n",
			__func__, err);
		return err;
	}

	err = bnxt_re_modify_qp(&qp->ib_qp, &qp_attr, qp_u_attr.qp_attr_mask, NULL);
	if (err) {
		dev_err(rdev_to_dev(rdev),
			"%s: Modify QP failed: 0x%llx, handle: 0x%x\n",
			 __func__, (u64)qp, uobj->id);
		if (qp_u_attr.qp_attr_mask & IB_QP_AV)
			rdma_put_gid_attr(qp_attr.ah_attr.grh.sgid_attr);
	} else {
		dev_dbg(rdev_to_dev(rdev),
			"%s: Modified QP: 0x%llx, handle: 0x%x\n",
			__func__, (u64)qp, uobj->id);
		if (qp_u_attr.qp_attr_mask & IB_QP_AV)
			qp->sgid_attr = qp_attr.ah_attr.grh.sgid_attr;
	}
	return err;
}

static int UVERBS_HANDLER(BNXT_RE_METHOD_DV_MODIFY_QP)(struct uverbs_attr_bundle *attrs)
{
	enum bnxt_re_dv_modify_qp_type type;
	struct ib_uobject *uobj;
	int err;

	uobj = uverbs_attr_get_uobject(attrs, BNXT_RE_DV_MODIFY_QP_HANDLE);
	err = uverbs_get_const(&type, attrs, BNXT_RE_DV_MODIFY_QP_TYPE);
	if (!err && type == BNXT_RE_DV_MODIFY_QP_UDP_SPORT)
		return bnxt_re_dv_modify_qp_v1(uobj, attrs, type);
	else
		return bnxt_re_dv_modify_qp_v2(uobj, attrs);
}

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_DV_MODIFY_QP,
			    UVERBS_ATTR_IDR(BNXT_RE_DV_MODIFY_QP_HANDLE,
					    UVERBS_IDR_ANY_OBJECT,
					    UVERBS_ACCESS_READ,
					    UA_MANDATORY),
			    UVERBS_ATTR_CONST_IN(BNXT_RE_DV_MODIFY_QP_TYPE,
						 enum bnxt_re_dv_modify_qp_type,
						 UA_OPTIONAL),
			    UVERBS_ATTR_PTR_IN(BNXT_RE_DV_MODIFY_QP_VALUE,
					       UVERBS_ATTR_TYPE(u32),
					       UA_OPTIONAL),
			    UVERBS_ATTR_PTR_IN(BNXT_RE_DV_MODIFY_QP_REQ,
					       UVERBS_ATTR_STRUCT(struct ib_uverbs_qp_attr,
								  reserved),
					       UA_OPTIONAL));

static int UVERBS_HANDLER(BNXT_RE_METHOD_NOTIFY_DRV)(struct uverbs_attr_bundle *attrs)
{
	struct bnxt_re_ucontext *uctx;

	uctx = container_of(ib_uverbs_get_ucontext(attrs), struct bnxt_re_ucontext, ib_uctx);
	bnxt_re_pacing_alert(uctx->rdev);
	return 0;
}

static int UVERBS_HANDLER(BNXT_RE_METHOD_ALLOC_PAGE)(struct uverbs_attr_bundle *attrs)
{
	struct ib_uobject *uobj = uverbs_attr_get_uobject(attrs, BNXT_RE_ALLOC_PAGE_HANDLE);
	enum bnxt_re_alloc_page_type alloc_type;
	struct bnxt_re_user_mmap_entry *entry;
	enum bnxt_re_mmap_flag mmap_flag;
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_re_ucontext *uctx;
	struct bnxt_re_dev *rdev;
	u64 mmap_offset;
	u32 dpi = 0;
	u32 length;
	u64 addr;
	int err;

	uctx = container_of(ib_uverbs_get_ucontext(attrs), struct bnxt_re_ucontext, ib_uctx);
	if (IS_ERR(uctx))
		return PTR_ERR(uctx);

	err = uverbs_get_const(&alloc_type, attrs, BNXT_RE_ALLOC_PAGE_TYPE);
	if (err)
		return err;

	rdev = uctx->rdev;
	cctx = rdev->chip_ctx;

	switch (alloc_type) {
	case BNXT_RE_ALLOC_WC_PAGE:
		if (cctx->modes.db_push_mode)  {
			length = PAGE_SIZE;
			dpi = uctx->wcdpi.dpi;
			addr = (u64)uctx->wcdpi.umdbr;
			mmap_flag = BNXT_RE_MMAP_WC_DB;
		} else {
			return -EINVAL;
		}
		break;
	case BNXT_RE_ALLOC_DBR_BAR_PAGE:
		length = PAGE_SIZE;
		addr = (u64)rdev->dbr_pacing_bar;
		mmap_flag = BNXT_RE_MMAP_DBR_PACING_BAR;
		break;
	case BNXT_RE_ALLOC_DBR_PAGE:
		length = PAGE_SIZE;
		addr = (u64)rdev->dbr_pacing_page;
		mmap_flag = BNXT_RE_MMAP_DBR_PAGE;
		break;
	default:
		return -EOPNOTSUPP;
	}

	entry = bnxt_re_mmap_entry_insert(uctx, addr, 0, mmap_flag, &mmap_offset);
	if (!entry)
		return -ENOMEM;

	uobj->object = entry;
	uverbs_finalize_uobj_create(attrs, BNXT_RE_ALLOC_PAGE_HANDLE);
	err = uverbs_copy_to(attrs, BNXT_RE_ALLOC_PAGE_MMAP_OFFSET,
			     &mmap_offset, sizeof(mmap_offset));
	if (err)
		return err;

	err = uverbs_copy_to(attrs, BNXT_RE_ALLOC_PAGE_MMAP_LENGTH,
			     &length, sizeof(length));
	if (err)
		return err;

	err = uverbs_copy_to(attrs, BNXT_RE_ALLOC_PAGE_DPI,
			     &dpi, sizeof(length));
	if (err)
		return err;

	return 0;
}

static int alloc_page_obj_cleanup(struct ib_uobject *uobject,
				  enum rdma_remove_reason why,
				  struct uverbs_attr_bundle *attrs)
{
	struct bnxt_re_user_mmap_entry *entry = uobject->object;

	switch (entry->mmap_flag) {
	case BNXT_RE_MMAP_WC_DB:
	case BNXT_RE_MMAP_DBR_PACING_BAR:
	case BNXT_RE_MMAP_DBR_PAGE:
		break;
	default:
		goto exit;
	}
	rdma_user_mmap_entry_remove(&entry->rdma_entry);
exit:
	return 0;
}

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_ALLOC_PAGE,
			    UVERBS_ATTR_IDR(BNXT_RE_ALLOC_PAGE_HANDLE,
					    BNXT_RE_OBJECT_ALLOC_PAGE,
					    UVERBS_ACCESS_NEW,
					    UA_MANDATORY),
			    UVERBS_ATTR_CONST_IN(BNXT_RE_ALLOC_PAGE_TYPE,
						 enum bnxt_re_alloc_page_type,
						 UA_MANDATORY),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_ALLOC_PAGE_MMAP_OFFSET,
						UVERBS_ATTR_TYPE(u64),
						UA_MANDATORY),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_ALLOC_PAGE_MMAP_LENGTH,
						UVERBS_ATTR_TYPE(u32),
						UA_MANDATORY),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_ALLOC_PAGE_DPI,
						UVERBS_ATTR_TYPE(u32),
						UA_MANDATORY));

DECLARE_UVERBS_NAMED_METHOD_DESTROY(BNXT_RE_METHOD_DESTROY_PAGE,
				    UVERBS_ATTR_IDR(BNXT_RE_DESTROY_PAGE_HANDLE,
						    BNXT_RE_OBJECT_ALLOC_PAGE,
						    UVERBS_ACCESS_DESTROY,
						    UA_MANDATORY));

DECLARE_UVERBS_NAMED_OBJECT(BNXT_RE_OBJECT_ALLOC_PAGE,
			    UVERBS_TYPE_ALLOC_IDR(alloc_page_obj_cleanup),
			    &UVERBS_METHOD(BNXT_RE_METHOD_ALLOC_PAGE),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DESTROY_PAGE));

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_NOTIFY_DRV);

DECLARE_UVERBS_GLOBAL_METHODS(BNXT_RE_OBJECT_NOTIFY_DRV,
			      &UVERBS_METHOD(BNXT_RE_METHOD_NOTIFY_DRV));

/* Toggle MEM */
static int UVERBS_HANDLER(BNXT_RE_METHOD_GET_TOGGLE_MEM)(struct uverbs_attr_bundle *attrs)
{
	struct ib_uobject *uobj = uverbs_attr_get_uobject(attrs, BNXT_RE_TOGGLE_MEM_HANDLE);
	enum bnxt_re_mmap_flag mmap_flag = BNXT_RE_MMAP_TOGGLE_PAGE;
	enum bnxt_re_get_toggle_mem_type res_type;
	struct bnxt_re_user_mmap_entry *entry;
	struct bnxt_re_ucontext *uctx;
	struct ib_ucontext *ib_uctx;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_srq *srq;
	u32 length = PAGE_SIZE;
	struct bnxt_re_cq *cq;
	u64 mem_offset;
	u32 offset = 0;
	u64 addr = 0;
	u32 res_id;
	int err;

	ib_uctx = ib_uverbs_get_ucontext(attrs);
	if (IS_ERR(ib_uctx))
		return PTR_ERR(ib_uctx);

	err = uverbs_get_const(&res_type, attrs, BNXT_RE_TOGGLE_MEM_TYPE);
	if (err)
		return err;

	uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	rdev = uctx->rdev;
	err = uverbs_copy_from(&res_id, attrs, BNXT_RE_TOGGLE_MEM_RES_ID);
	if (err)
		return err;

	switch (res_type) {
	case BNXT_RE_CQ_TOGGLE_MEM:
		cq = bnxt_re_search_for_cq(rdev, res_id, uctx);
		if (!cq)
			return -EINVAL;

		addr = (u64)cq->cq_toggle_page;
		break;
	case BNXT_RE_SRQ_TOGGLE_MEM:
		srq = bnxt_re_search_for_srq(rdev, res_id, uctx);
		if (!srq)
			return -EINVAL;

		addr = (u64)srq->srq_toggle_page;
		break;
	default:
		return -EOPNOTSUPP;
	}

	entry = bnxt_re_mmap_entry_insert(uctx, addr, 0, mmap_flag, &mem_offset);
	if (!entry)
		return -ENOMEM;

	uobj->object = entry;
	uverbs_finalize_uobj_create(attrs, BNXT_RE_TOGGLE_MEM_HANDLE);
	err = uverbs_copy_to(attrs, BNXT_RE_TOGGLE_MEM_MMAP_PAGE,
			     &mem_offset, sizeof(mem_offset));
	if (err)
		return err;

	err = uverbs_copy_to(attrs, BNXT_RE_TOGGLE_MEM_MMAP_LENGTH,
			     &length, sizeof(length));
	if (err)
		return err;

	err = uverbs_copy_to(attrs, BNXT_RE_TOGGLE_MEM_MMAP_OFFSET,
			     &offset, sizeof(offset));
	if (err)
		return err;

	return 0;
}

static int get_toggle_mem_obj_cleanup(struct ib_uobject *uobject,
				      enum rdma_remove_reason why,
				      struct uverbs_attr_bundle *attrs)
{
	struct  bnxt_re_user_mmap_entry *entry = uobject->object;

	rdma_user_mmap_entry_remove(&entry->rdma_entry);
	return 0;
}

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_GET_TOGGLE_MEM,
			    UVERBS_ATTR_IDR(BNXT_RE_TOGGLE_MEM_HANDLE,
					    BNXT_RE_OBJECT_GET_TOGGLE_MEM,
					    UVERBS_ACCESS_NEW,
					    UA_MANDATORY),
			    UVERBS_ATTR_CONST_IN(BNXT_RE_TOGGLE_MEM_TYPE,
						 enum bnxt_re_get_toggle_mem_type,
						 UA_MANDATORY),
			    UVERBS_ATTR_PTR_IN(BNXT_RE_TOGGLE_MEM_RES_ID,
					       UVERBS_ATTR_TYPE(u32),
					       UA_MANDATORY),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_TOGGLE_MEM_MMAP_PAGE,
						UVERBS_ATTR_TYPE(u64),
						UA_MANDATORY),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_TOGGLE_MEM_MMAP_OFFSET,
						UVERBS_ATTR_TYPE(u32),
						UA_MANDATORY),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_TOGGLE_MEM_MMAP_LENGTH,
						UVERBS_ATTR_TYPE(u32),
						UA_MANDATORY));

DECLARE_UVERBS_NAMED_METHOD_DESTROY(BNXT_RE_METHOD_RELEASE_TOGGLE_MEM,
				    UVERBS_ATTR_IDR(BNXT_RE_RELEASE_TOGGLE_MEM_HANDLE,
						    BNXT_RE_OBJECT_GET_TOGGLE_MEM,
						    UVERBS_ACCESS_DESTROY,
						    UA_MANDATORY));

DECLARE_UVERBS_NAMED_OBJECT(BNXT_RE_OBJECT_GET_TOGGLE_MEM,
			    UVERBS_TYPE_ALLOC_IDR(get_toggle_mem_obj_cleanup),
			    &UVERBS_METHOD(BNXT_RE_METHOD_GET_TOGGLE_MEM),
			    &UVERBS_METHOD(BNXT_RE_METHOD_RELEASE_TOGGLE_MEM));

static int bnxt_re_dv_validate_umem_attr(struct bnxt_re_dev *rdev,
					 struct uverbs_attr_bundle *attrs,
					 struct bnxt_re_dv_umem *obj)
{
	int dmabuf_fd = 0;
	u32 access_flags;
	size_t size;
	u64 addr;
	int err;

	err = uverbs_get_flags32(&access_flags, attrs,
				 BNXT_RE_UMEM_OBJ_REG_ACCESS,
				 IB_ACCESS_LOCAL_WRITE |
				 IB_ACCESS_REMOTE_WRITE |
				 IB_ACCESS_REMOTE_READ);
	if (err)
		return err;

	err = ib_check_mr_access(&rdev->ibdev, access_flags);
	if (err)
		return err;

	if (uverbs_copy_from(&addr, attrs, BNXT_RE_UMEM_OBJ_REG_ADDR) ||
	    uverbs_copy_from(&size, attrs, BNXT_RE_UMEM_OBJ_REG_LEN))
		return -EFAULT;
#ifdef HAVE_UVERBS_GET_RAW_FD
	if (uverbs_attr_is_valid(attrs, BNXT_RE_UMEM_OBJ_REG_DMABUF_FD)) {
		if (uverbs_get_raw_fd(&dmabuf_fd, attrs,
				      BNXT_RE_UMEM_OBJ_REG_DMABUF_FD))
			return -EFAULT;
	}
#endif
	obj->addr = addr;
	obj->size = size;
	obj->access = access_flags;
	obj->dmabuf_fd = dmabuf_fd;

	return 0;
}

static bool bnxt_re_dv_is_valid_umem(struct bnxt_re_dv_umem *umem,
				     u64 offset, u32 size)
{
	return ((offset == ALIGN(offset, PAGE_SIZE)) &&
		(offset + size <= umem->size));
}

static struct bnxt_re_dv_umem *bnxt_re_dv_umem_get(struct bnxt_re_dev *rdev,
						   struct ib_ucontext *ib_uctx,
						   struct bnxt_re_dv_umem *obj,
						   u64 umem_offset, u64 size,
						   struct bnxt_qplib_sg_info *sg,
						   u8 res)
{
	struct uverbs_attr_bundle attr = { .context = ib_uctx };
	struct bnxt_re_dv_umem *dv_umem;
	struct ib_umem *umem;
	int umem_pgs, rc;

	if (!bnxt_re_dv_is_valid_umem(obj, umem_offset, size))
		return ERR_PTR(-EINVAL);

	dv_umem = kzalloc(sizeof(*dv_umem), GFP_KERNEL);
	if (!dv_umem)
		return ERR_PTR(-ENOMEM);

	dv_umem->addr = obj->addr + umem_offset;
	dv_umem->size = size;
	dv_umem->rdev = obj->rdev;
	dv_umem->dmabuf_fd = obj->dmabuf_fd;
	dv_umem->access = obj->access;

	if (obj->dmabuf_fd) {
#ifdef HAVE_IB_UMEM_DMABUF
		struct ib_umem_dmabuf *umem_dmabuf;

		umem_dmabuf = ib_umem_dmabuf_get_pinned(&rdev->ibdev, dv_umem->addr,
							dv_umem->size, dv_umem->dmabuf_fd,
							dv_umem->access);
		if (IS_ERR(umem_dmabuf)) {
			rc = PTR_ERR(umem_dmabuf);
			dev_err(rdev_to_dev(rdev),
				"%s: failed to get umem dmabuf : %d\n",
				__func__, rc);
			goto free_umem;
		}
		umem = &umem_dmabuf->umem;
#else
		rc = -EOPNOTSUPP;
		goto free_umem;
#endif
	} else {
		umem = ib_umem_get_flags_compat(rdev, ib_uctx,
						&attr.driver_udata, dv_umem->addr,
						dv_umem->size, dv_umem->access, 0);
		if (IS_ERR(umem)) {
			rc = PTR_ERR(umem);
			dev_err(rdev_to_dev(rdev),
				"%s: ib_umem_get failed! rc = %d\n",
				__func__, rc);
			goto free_umem;
		}
	}

	dv_umem->umem = umem;

	umem_pgs = ib_umem_num_pages_compat(umem);
	if (!umem_pgs) {
		dev_err(rdev_to_dev(rdev), "%s: umem is invalid!", __func__);
		rc = -EINVAL;
		goto rel_umem;
	}

	bnxt_re_set_sginfo(rdev, umem, dv_umem->addr, sg, res);

	/* Map umem buf ptrs to the PBL */
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	sg->sghead = get_ib_umem_sgl(umem, &sg->nmap);
#else
	sg->umem = umem;
#endif

	dev_dbg(rdev_to_dev(rdev), "%s: umem: 0x%llx\n", __func__, (u64)umem);
	dev_dbg(rdev_to_dev(rdev), "\tva: 0x%llx size: %lu\n",
		dv_umem->addr, dv_umem->size);
	dev_dbg(rdev_to_dev(rdev), "\tpgsize: %d pgshft: %d npages: %d fwo_offset: 0x%x\n",
		sg->pgsize, sg->pgshft, sg->npages, sg->fwo_offset);
	return dv_umem;

rel_umem:
	bnxt_re_peer_mem_release(umem);
free_umem:
	kfree(dv_umem);
	return ERR_PTR(rc);
}

static int bnxt_re_dv_umem_cleanup(struct ib_uobject *uobject,
				   enum rdma_remove_reason why,
				   struct uverbs_attr_bundle *attrs)
{
	struct bnxt_re_dv_umem *obj;

	obj = uobject->object;
	if (obj->map) {
		bnxt_re_peer_mem_release(obj->map->umem);
		kfree(obj->map);
	}

	kfree(obj);
	return 0;
}

static int bnxt_re_dv_umem_map(struct uverbs_attr_bundle *attrs,
			       struct bnxt_re_dv_umem *obj)
{
	struct bnxt_qplib_sg_info sginfo = {};
	struct bnxt_re_ucontext *uctx;
	struct ib_ucontext *ib_uctx;
	struct bnxt_re_dev *rdev;
#ifdef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	struct ib_block_iter biter;
#else
	struct scatterlist *sg;
#endif
	u64 addr, size, *buf;
	int count, i = 0;
	u32 best_pg_sz;

	if (!uverbs_attr_is_valid(attrs, BNXT_RE_UMEM_OBJ_REG_PA_ARR) ||
	    !uverbs_attr_is_valid(attrs, BNXT_RE_UMEM_OBJ_REG_PA_ARR_LEN) ||
	    !uverbs_attr_is_valid(attrs, BNXT_RE_UMEM_OBJ_REG_MAP_PG_SZ))
		return 0;

	ib_uctx = ib_uverbs_get_ucontext(attrs);
	uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	rdev = uctx->rdev;

	if (uverbs_copy_from(&addr, attrs, BNXT_RE_UMEM_OBJ_REG_PA_ARR) ||
	    uverbs_copy_from(&size, attrs, BNXT_RE_UMEM_OBJ_REG_PA_ARR_LEN))
		return -EFAULT;

	if (!addr || size < sizeof(u64))
		return -EFAULT;

	size = min_t(u64, size, BNXT_RE_UMEM_MAX_PA_BUFSZ);
	buf = vzalloc(size);
	if (!buf)
		return -ENOMEM;

	count = size / sizeof(u64);
	obj->map = bnxt_re_dv_umem_get(rdev, ib_uctx, obj, 0, obj->size, &sginfo,
				       BNXT_RE_RES_TYPE_UMEM_MAP);
	if (IS_ERR(obj->map))
		goto buf_free;

#ifdef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	/* coverity[DIVIDE_BY_ZERO:FALSE] */
	rdma_umem_for_each_dma_block(obj->map->umem, &biter, sginfo.pgsize) {
		buf[i] = rdma_block_iter_dma_address(&biter);
		i++;
		if (i >= count)
			break;
	}
#else
	for_each_sg(obj->map->umem->sg_head.sgl, sg, obj->map->umem->nmap, i) {
		if (i >= count)
			break;
		buf[i] = sg_dma_address(sg);
	}
#endif

	best_pg_sz = sginfo.pgsize;
	if (copy_to_user((void *)addr, (void *)buf, size) ||
	    uverbs_copy_to(attrs, BNXT_RE_UMEM_OBJ_REG_MAP_PG_SZ,
			   &best_pg_sz, sizeof(best_pg_sz)))
		goto umem_free;

	vfree(buf);
	return 0;
umem_free:
	bnxt_re_peer_mem_release(obj->map->umem);
	kfree(obj->map);
buf_free:
	vfree(buf);
	return -EFAULT;
}

static int UVERBS_HANDLER(BNXT_RE_METHOD_UMEM_REG)(struct uverbs_attr_bundle *attrs)
{
	struct ib_uobject *uobj =
		uverbs_attr_get_uobject(attrs, BNXT_RE_UMEM_OBJ_REG_HANDLE);
	struct bnxt_re_ucontext *uctx;
	struct ib_ucontext *ib_uctx;
	struct bnxt_re_dv_umem *obj;
	struct bnxt_re_dev *rdev;
	int err;

	ib_uctx = ib_uverbs_get_ucontext(attrs);
	if (IS_ERR(ib_uctx))
		return PTR_ERR(ib_uctx);

	uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	rdev = uctx->rdev;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	obj->rdev = rdev;
	err = bnxt_re_dv_validate_umem_attr(rdev, attrs, obj);
	if (err)
		goto free_mem;

	obj->umem = NULL;
	uobj->object = obj;

	err = bnxt_re_dv_umem_map(attrs, obj);
	if (err)
		goto free_mem;

	uverbs_finalize_uobj_create(attrs, BNXT_RE_UMEM_OBJ_REG_HANDLE);

	return 0;
free_mem:
	kfree(obj);
	return err;
}

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_UMEM_REG,
			    UVERBS_ATTR_IDR(BNXT_RE_UMEM_OBJ_REG_HANDLE,
					    BNXT_RE_OBJECT_UMEM,
					    UVERBS_ACCESS_NEW,
					    UA_MANDATORY),
			    UVERBS_ATTR_PTR_IN(BNXT_RE_UMEM_OBJ_REG_ADDR,
					       UVERBS_ATTR_TYPE(u64),
					       UA_MANDATORY),
			    UVERBS_ATTR_PTR_IN(BNXT_RE_UMEM_OBJ_REG_LEN,
					       UVERBS_ATTR_TYPE(u64),
					       UA_MANDATORY),
			    UVERBS_ATTR_FLAGS_IN(BNXT_RE_UMEM_OBJ_REG_ACCESS,
						 enum ib_access_flags),
#ifdef HAVE_UVERBS_GET_RAW_FD
			    UVERBS_ATTR_RAW_FD(BNXT_RE_UMEM_OBJ_REG_DMABUF_FD,
					       UA_OPTIONAL),
#endif
			    UVERBS_ATTR_CONST_IN(BNXT_RE_UMEM_OBJ_REG_PGSZ_BITMAP,
						 u64),
			    UVERBS_ATTR_PTR_IN(BNXT_RE_UMEM_OBJ_REG_PA_ARR,
					       UVERBS_ATTR_TYPE(u64),
					       UA_OPTIONAL),
			    UVERBS_ATTR_PTR_IN(BNXT_RE_UMEM_OBJ_REG_PA_ARR_LEN,
					       UVERBS_ATTR_TYPE(u64),
					       UA_OPTIONAL),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_UMEM_OBJ_REG_MAP_PG_SZ,
						UVERBS_ATTR_TYPE(u32),
						UA_OPTIONAL));

DECLARE_UVERBS_NAMED_METHOD_DESTROY(BNXT_RE_METHOD_UMEM_DEREG,
				    UVERBS_ATTR_IDR(BNXT_RE_UMEM_OBJ_DEREG_HANDLE,
						    BNXT_RE_OBJECT_UMEM,
						    UVERBS_ACCESS_DESTROY,
						    UA_MANDATORY));

DECLARE_UVERBS_NAMED_OBJECT(BNXT_RE_OBJECT_UMEM,
			    UVERBS_TYPE_ALLOC_IDR(bnxt_re_dv_umem_cleanup),
			    &UVERBS_METHOD(BNXT_RE_METHOD_UMEM_REG),
			    &UVERBS_METHOD(BNXT_RE_METHOD_UMEM_DEREG));

static int UVERBS_HANDLER(BNXT_RE_METHOD_DBR_ALLOC)(struct uverbs_attr_bundle *attrs)
{
	struct bnxt_re_dv_db_region_attr dbr = {};
	struct bnxt_re_alloc_dbr_obj *obj;
	struct bnxt_re_ucontext *uctx;
	struct ib_ucontext *ib_uctx;
	struct bnxt_qplib_dpi *dpi;
	struct bnxt_re_dev *rdev;
	struct ib_uobject *uobj;
	u64 mmap_offset;
	int ret;

	ib_uctx = ib_uverbs_get_ucontext(attrs);
	if (IS_ERR(ib_uctx))
		return PTR_ERR(ib_uctx);

	uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	rdev = uctx->rdev;
	uobj = uverbs_attr_get_uobject(attrs, BNXT_RE_DV_ALLOC_DBR_HANDLE);

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	dpi = &obj->dpi;
	ret = bnxt_qplib_alloc_uc_dpi(&rdev->qplib_res, dpi);
	if (ret)
		goto free_mem;

	obj->entry = bnxt_re_mmap_entry_insert(uctx, dpi->umdbr, 0,
					       BNXT_RE_MMAP_UC_DB,
					       &mmap_offset);
	if (!obj->entry) {
		ret = -ENOMEM;
		goto free_dpi;
	}

	obj->rdev = rdev;
	dbr.umdbr = dpi->umdbr;
	dbr.dpi = dpi->dpi;

	ret = uverbs_copy_to_struct_or_zero(attrs, BNXT_RE_DV_ALLOC_DBR_ATTR,
					    &dbr, sizeof(dbr));
	if (ret)
		goto free_entry;

	ret = uverbs_copy_to(attrs, BNXT_RE_DV_ALLOC_DBR_OFFSET,
			     &mmap_offset, sizeof(mmap_offset));
	if (ret)
		goto free_entry;

	uobj->object = obj;
	uverbs_finalize_uobj_create(attrs, BNXT_RE_DV_ALLOC_DBR_HANDLE);
	return 0;
free_entry:
	rdma_user_mmap_entry_remove(&obj->entry->rdma_entry);
free_dpi:
	bnxt_qplib_free_uc_dpi(&rdev->qplib_res, dpi);
free_mem:
	kfree(obj);
	return ret;
}

static int bnxt_re_dv_dbr_cleanup(struct ib_uobject *uobject,
				  enum rdma_remove_reason why,
				  struct uverbs_attr_bundle *attrs)
{
	struct bnxt_re_alloc_dbr_obj *obj = uobject->object;
	struct bnxt_re_dev *rdev = obj->rdev;

	rdma_user_mmap_entry_remove(&obj->entry->rdma_entry);
	bnxt_qplib_free_uc_dpi(&rdev->qplib_res, &obj->dpi);
	kfree(obj);
	return 0;
}

static int UVERBS_HANDLER(BNXT_RE_METHOD_DBR_QUERY)(struct uverbs_attr_bundle *attrs)
{
	struct bnxt_re_dv_db_region_attr dpi = {};
	struct bnxt_re_ucontext *uctx;
	struct ib_ucontext *ib_uctx;
	struct bnxt_re_dev *rdev;
	int ret;

	ib_uctx = ib_uverbs_get_ucontext(attrs);
	if (IS_ERR(ib_uctx))
		return PTR_ERR(ib_uctx);

	uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	rdev = uctx->rdev;

	dpi.umdbr = uctx->dpi.umdbr;
	dpi.dpi = uctx->dpi.dpi;

	ret = uverbs_copy_to_struct_or_zero(attrs, BNXT_RE_DV_QUERY_DBR_ATTR,
					    &dpi, sizeof(dpi));
	if (ret)
		return ret;

	return 0;
}

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_DBR_ALLOC,
			    UVERBS_ATTR_IDR(BNXT_RE_DV_ALLOC_DBR_HANDLE,
					    BNXT_RE_OBJECT_DBR,
					    UVERBS_ACCESS_NEW,
					    UA_MANDATORY),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_DV_ALLOC_DBR_ATTR,
						UVERBS_ATTR_STRUCT(struct bnxt_re_dv_db_region_attr,
								   dbr),
								   UA_MANDATORY),
			    UVERBS_ATTR_PTR_IN(BNXT_RE_DV_ALLOC_DBR_OFFSET,
					       UVERBS_ATTR_TYPE(u64),
					       UA_MANDATORY));

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_DBR_QUERY,
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_DV_QUERY_DBR_ATTR,
						UVERBS_ATTR_STRUCT(struct bnxt_re_dv_db_region_attr,
								   dbr),
						UA_MANDATORY));

DECLARE_UVERBS_NAMED_METHOD_DESTROY(BNXT_RE_METHOD_DBR_FREE,
				    UVERBS_ATTR_IDR(BNXT_RE_DV_FREE_DBR_HANDLE,
						    BNXT_RE_OBJECT_DBR,
						    UVERBS_ACCESS_DESTROY,
						    UA_MANDATORY));

DECLARE_UVERBS_NAMED_OBJECT(BNXT_RE_OBJECT_DBR,
			    UVERBS_TYPE_ALLOC_IDR(bnxt_re_dv_dbr_cleanup),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DBR_ALLOC),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DBR_FREE),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DBR_QUERY));

static int bnxt_re_dv_create_cq_resp(struct bnxt_re_dev *rdev,
				     struct bnxt_re_cq *cq,
				     struct bnxt_re_dv_cq_resp *resp)
{
	struct bnxt_qplib_cq *qplcq = &cq->qplib_cq;

	resp->cqid = qplcq->id;
	resp->tail = qplcq->hwq.cons;
	resp->phase = qplcq->period;

	if (!(rdev->chip_ctx->modes.toggle_bits & BNXT_QPLIB_CQ_TOGGLE_BIT))
		return 0;

	cq->cq_toggle_page = (void *)get_zeroed_page(GFP_KERNEL);
	if (!cq->cq_toggle_page) {
		dev_err(rdev_to_dev(rdev), "CQ page allocation failed!");
		return -ENOMEM;
	}

	hash_add(rdev->cq_hash, &cq->hash_entry, cq->qplib_cq.id);
	BNXT_RE_CQ_PAGE_LIST_ADD(cq->uctx, cq);
	resp->comp_mask |= BNXT_RE_CQ_TOGGLE_PAGE_SUPPORT;

	dev_dbg(rdev_to_dev(rdev),
		"%s: cqid: 0x%x tail: 0x%x phase: 0x%x comp_mask: 0x%llx\n",
		__func__, resp->cqid, resp->tail, resp->phase,
		resp->comp_mask);
	return 0;
}

static struct bnxt_re_cq *
bnxt_re_dv_create_qplib_cq(struct bnxt_re_dev *rdev,
			   struct bnxt_re_ucontext *re_uctx,
			   struct bnxt_re_dv_cq_req *req,
			   struct bnxt_re_dv_umem *umem_handle,
			   u64 umem_offset)
{
	struct bnxt_qplib_dev_attr *dev_attr = rdev->dev_attr;
	struct bnxt_re_dv_umem *dv_umem;
	struct bnxt_qplib_cq *qplcq;
	struct bnxt_re_cq *cq = NULL;
	int cqe = req->ncqe;
	u32 max_active_cqs;
	int rc = 0;

	if (atomic_read(&rdev->stats.rsors.cq_count) >= dev_attr->max_cq) {
		dev_err(rdev_to_dev(rdev),
			"Create CQ failed - max exceeded(CQs)");
		return NULL;
	}

	/* Validate CQ fields */
	if (cqe < 1 || cqe > dev_attr->max_cq_wqes) {
		dev_err(rdev_to_dev(rdev),
			"Create CQ failed - max exceeded(CQ_WQs)");
		return NULL;
	}

	cq = kzalloc(sizeof(*cq), GFP_KERNEL);
	if (!cq)
		return NULL;

	cq->rdev = rdev;
	cq->uctx = re_uctx;
	qplcq = &cq->qplib_cq;
	qplcq->cq_handle = (u64)qplcq;
	dev_dbg(rdev_to_dev(rdev), "%s: umem_va: 0x%llx umem_offset: 0x%llx\n",
		__func__, umem_handle->addr, umem_offset);
	dv_umem = bnxt_re_dv_umem_get(rdev, &re_uctx->ib_uctx, umem_handle,
				      umem_offset, cqe * sizeof(struct cq_base),
				      &qplcq->sginfo, BNXT_RE_RES_TYPE_CQ);
	if (IS_ERR(dv_umem)) {
		rc = PTR_ERR(dv_umem);
		dev_err(rdev_to_dev(rdev), "%s: bnxt_re_dv_umem_get() failed! rc = %d\n",
			__func__, rc);
		goto fail_umem;
	}
	cq->umem = dv_umem->umem;
	cq->umem_handle = dv_umem;
	dev_dbg(rdev_to_dev(rdev), "%s: cq->umem: %llx\n", __func__, (u64)cq->umem);

	qplcq->dpi = &re_uctx->dpi;
	qplcq->max_wqe = cqe;
	qplcq->nq = bnxt_re_get_nq(rdev);
	qplcq->cnq_hw_ring_id = qplcq->nq->ring_id;
	qplcq->coalescing = &rdev->cq_coalescing;
	qplcq->cq_timer_reset = &rdev->cq_timer_cfg;
	qplcq->overflow_telemetry = rdev->enable_queue_overflow_telemetry;
	qplcq->ignore_overrun = 1;

	rc = bnxt_re_setup_cq_hwqs(qplcq);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to setup HW CQ!");
		goto fail_qpl;
	}
	rc = bnxt_qplib_create_cq(&rdev->qplib_res, qplcq);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Create HW CQ failed!");
		goto fail_hwq;
	}

	INIT_LIST_HEAD(&cq->cq_list);
	cq->ib_cq.cqe = cqe;
	cq->cq_period = qplcq->period;

	atomic_inc(&rdev->stats.rsors.cq_count);
	max_active_cqs = atomic_read(&rdev->stats.rsors.cq_count);
	if (max_active_cqs > atomic_read(&rdev->stats.rsors.max_cq_count))
		atomic_set(&rdev->stats.rsors.max_cq_count, max_active_cqs);
	spin_lock_init(&cq->cq_lock);

	return cq;

fail_hwq:
	bnxt_qplib_free_hwq(&rdev->qplib_res, &qplcq->hwq);
fail_qpl:
	bnxt_re_peer_mem_release(cq->umem);
	kfree(cq->umem_handle);
fail_umem:
	kfree(cq);
	return NULL;
}

static int bnxt_re_dv_uverbs_copy_to(struct bnxt_re_dev *rdev,
				     struct uverbs_attr_bundle *attrs,
				     int attr, void *from, size_t size)
{
	int ret;

	ret = uverbs_copy_to_struct_or_zero(attrs, attr, from, size);
	if (ret) {
		dev_err(rdev_to_dev(rdev), "%s: uverbs_copy_to() failed: %d\n",
			__func__, ret);
		return ret;
	}

	dev_dbg(rdev_to_dev(rdev),
		"%s: Copied to user from: 0x%llx size: 0x%lx\n",
		__func__, (u64)from, size);
	return ret;
}

static void bnxt_re_dv_finalize_uobj(struct ib_uobject *uobj, void *priv_obj,
				     struct uverbs_attr_bundle *attrs, int attr)
{
	uobj->object = priv_obj;
	uverbs_finalize_uobj_create(attrs, attr);
}

static void bnxt_re_dv_init_ib_cq(struct bnxt_re_dev *rdev,
				  struct ib_cq *ib_cq,
				  struct bnxt_re_cq *re_cq)
{
	ib_cq = &re_cq->ib_cq;
	ib_cq->device = &rdev->ibdev;
	ib_cq->uobject = NULL;
	ib_cq->comp_handler  = NULL;
	ib_cq->event_handler = NULL;
	atomic_set(&ib_cq->usecnt, 0);
}

static int UVERBS_HANDLER(BNXT_RE_METHOD_DV_CREATE_CQ)(struct uverbs_attr_bundle *attrs)
{
	struct ib_uobject *uobj =
		uverbs_attr_get_uobject(attrs, BNXT_RE_DV_CREATE_CQ_HANDLE);
	struct bnxt_re_dv_umem *umem_handle = NULL;
	struct bnxt_re_dv_cq_resp resp = {};
	struct bnxt_re_dv_cq_req req = {};
	struct bnxt_re_ucontext *re_uctx;
	struct ib_ucontext *ib_uctx;
	struct ib_cq *ib_cq = NULL;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_cq *re_cq;
	u64 offset;
	int ret;

	ib_uctx = ib_uverbs_get_ucontext(attrs);
	if (IS_ERR(ib_uctx))
		return PTR_ERR(ib_uctx);

	re_uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	rdev = re_uctx->rdev;

	ret = uverbs_copy_from_or_zero(&req, attrs, BNXT_RE_DV_CREATE_CQ_REQ);
	if (ret) {
		dev_err(rdev_to_dev(rdev), "%s: Failed to copy request: %d\n",
			__func__, ret);
		return ret;
	}

	umem_handle = uverbs_attr_get_obj(attrs, BNXT_RE_DV_CREATE_CQ_UMEM_HANDLE);
	if (IS_ERR(umem_handle)) {
		dev_err(rdev_to_dev(rdev),
			"%s: BNXT_RE_DV_CREATE_CQ_UMEM_HANDLE is not valid\n",
			__func__);
		return PTR_ERR(umem_handle);
	}

	ret = uverbs_copy_from(&offset, attrs, BNXT_RE_DV_CREATE_CQ_UMEM_OFFSET);
	if (ret) {
		dev_err(rdev_to_dev(rdev), "%s: Failed to copy umem offset: %d\n",
			__func__, ret);
		return ret;
	}

	re_cq = bnxt_re_dv_create_qplib_cq(rdev, re_uctx, &req, umem_handle, offset);
	if (!re_cq) {
		dev_err(rdev_to_dev(rdev), "%s: Failed to create qplib cq\n",
			__func__);
		return -EIO;
	}

	ret = bnxt_re_dv_create_cq_resp(rdev, re_cq, &resp);
	if (ret) {
		dev_err(rdev_to_dev(rdev),
			"%s: Failed to create cq response\n", __func__);
		goto fail_resp;
	}

	ret = bnxt_re_dv_uverbs_copy_to(rdev, attrs, BNXT_RE_DV_CREATE_CQ_RESP,
					&resp, sizeof(resp));
	if (ret) {
		dev_err(rdev_to_dev(rdev),
			"%s: Failed to copy cq response: %d\n", __func__, ret);
		goto fail_copy;
	}

	bnxt_re_dv_finalize_uobj(uobj, re_cq, attrs, BNXT_RE_DV_CREATE_CQ_HANDLE);
	bnxt_re_dv_init_ib_cq(rdev, ib_cq, re_cq);
	BNXT_RE_RES_LIST_ADD(rdev, re_cq, BNXT_RE_RES_TYPE_CQ);
	re_cq->is_dv_cq = true;
	atomic_inc(&rdev->dv_cq_count);

	dev_dbg(rdev_to_dev(rdev), "%s: Created CQ: 0x%llx, handle: 0x%x\n",
		__func__, (u64)re_cq, uobj->id);

	return 0;

fail_copy:
	if (re_cq->cq_toggle_page)
		bnxt_re_cq_page_del(re_cq);
fail_resp:
	bnxt_qplib_destroy_cq(&rdev->qplib_res, &re_cq->qplib_cq, 0, NULL);
	bnxt_re_put_nq(rdev, re_cq->qplib_cq.nq);
	if (re_cq->umem_handle) {
		bnxt_re_peer_mem_release(re_cq->umem);
		kfree(re_cq->umem_handle);
	}
	kfree(re_cq);
	return ret;
};

void bnxt_re_dv_destroy_cq(struct bnxt_re_dev *rdev, struct bnxt_re_cq *cq)
{
	int rc;

	if (cq->cq_toggle_page)
		bnxt_re_cq_page_del(cq);

	rc = bnxt_qplib_destroy_cq(&rdev->qplib_res, &cq->qplib_cq,
				   0, NULL);
	if (rc)
		dev_err_ratelimited(rdev_to_dev(rdev),
				    "%s id = %d failed rc = %d",
				    __func__, cq->qplib_cq.id, rc);

	bnxt_re_put_nq(rdev, cq->qplib_cq.nq);
	if (cq->umem_handle) {
		bnxt_re_peer_mem_release(cq->umem);
		kfree(cq->umem_handle);
	}
	atomic_dec(&rdev->stats.rsors.cq_count);
	atomic_dec(&rdev->dv_cq_count);
	kfree(cq);
}

static int bnxt_re_dv_free_cq(struct ib_uobject *uobj,
			      enum rdma_remove_reason why,
			      struct uverbs_attr_bundle *attrs)
{
	struct bnxt_re_cq *cq = uobj->object;
	struct bnxt_re_dev *rdev = cq->rdev;

	dev_dbg(rdev_to_dev(rdev), "%s: Destroy CQ: 0x%llx, handle: 0x%x\n",
		__func__, (u64)cq, uobj->id);
	BNXT_RE_RES_LIST_DEL(rdev, cq, BNXT_RE_RES_TYPE_CQ);
	bnxt_re_dv_destroy_cq(rdev, cq);
	uobj->object = NULL;
	return 0;
}

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_DV_CREATE_CQ,
			    UVERBS_ATTR_IDR(BNXT_RE_DV_CREATE_CQ_HANDLE,
					    BNXT_RE_OBJECT_DV_CQ,
					    UVERBS_ACCESS_NEW,
					    UA_MANDATORY),
			    UVERBS_ATTR_PTR_IN(BNXT_RE_DV_CREATE_CQ_REQ,
					       UVERBS_ATTR_STRUCT(struct bnxt_re_dv_cq_req,
								  comp_mask),
								  UA_MANDATORY),
			    UVERBS_ATTR_IDR(BNXT_RE_DV_CREATE_CQ_UMEM_HANDLE,
					    BNXT_RE_OBJECT_UMEM,
					    UVERBS_ACCESS_READ,
					    UA_MANDATORY),
			    UVERBS_ATTR_PTR_IN(BNXT_RE_DV_CREATE_CQ_UMEM_OFFSET,
					       UVERBS_ATTR_TYPE(u64),
					       UA_MANDATORY),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_DV_CREATE_CQ_RESP,
						UVERBS_ATTR_STRUCT(struct bnxt_re_dv_cq_resp,
								   comp_mask),
								   UA_MANDATORY));

DECLARE_UVERBS_NAMED_METHOD_DESTROY(BNXT_RE_METHOD_DV_DESTROY_CQ,
				    UVERBS_ATTR_IDR(BNXT_RE_DV_DESTROY_CQ_HANDLE,
						    BNXT_RE_OBJECT_DV_CQ,
						    UVERBS_ACCESS_DESTROY,
						    UA_MANDATORY));

DECLARE_UVERBS_NAMED_OBJECT(BNXT_RE_OBJECT_DV_CQ,
			    UVERBS_TYPE_ALLOC_IDR(bnxt_re_dv_free_cq),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DV_CREATE_CQ),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DV_DESTROY_CQ));

static void
bnxt_re_print_dv_qp_attr(struct bnxt_re_dev *rdev,
			 struct bnxt_re_cq *send_cq,
			 struct bnxt_re_cq *recv_cq,
			 struct  bnxt_re_dv_create_qp_req *req)
{
	dev_dbg(rdev_to_dev(rdev), "DV_QP_ATTR:\n");
	dev_dbg(rdev_to_dev(rdev),
		"\t qp_type: 0x%x pdid: 0x%x qp_handle: 0x%llx\n",
		req->qp_type, req->pd_id, req->qp_handle);

	dev_dbg(rdev_to_dev(rdev), "\t SQ ATTR:\n");
	dev_dbg(rdev_to_dev(rdev),
		"\t\t max_send_wr: 0x%x max_send_sge: 0x%x\n",
		req->max_send_wr, req->max_send_sge);
	dev_dbg(rdev_to_dev(rdev),
		"\t\t va: 0x%llx len: 0x%x slots: 0x%x wqe_sz: 0x%x\n",
		req->sq_va, req->sq_len, req->sq_slots, req->sq_wqe_sz);
	dev_dbg(rdev_to_dev(rdev), "\t\t psn_sz: 0x%x npsn: 0x%x\n",
		req->sq_psn_sz, req->sq_npsn);
	dev_dbg(rdev_to_dev(rdev),
		"\t\t send_cq_id: 0x%x\n", send_cq->qplib_cq.id);

	dev_dbg(rdev_to_dev(rdev), "\t RQ ATTR:\n");
	dev_dbg(rdev_to_dev(rdev),
		"\t\t max_recv_wr: 0x%x max_recv_sge: 0x%x\n",
		req->max_recv_wr, req->max_recv_sge);
	dev_dbg(rdev_to_dev(rdev),
		"\t\t va: 0x%llx len: 0x%x slots: 0x%x wqe_sz: 0x%x\n",
		req->rq_va, req->rq_len, req->rq_slots, req->rq_wqe_sz);
	dev_dbg(rdev_to_dev(rdev),
		"\t\t recv_cq_id: 0x%x\n", recv_cq->qplib_cq.id);
}

static int bnxt_re_dv_init_qp_attr(struct bnxt_re_qp *qp,
				   struct ib_ucontext *context,
				   struct bnxt_re_cq *send_cq,
				   struct bnxt_re_cq *recv_cq,
				   struct bnxt_re_srq *srq,
				   struct bnxt_re_alloc_dbr_obj *dbr_obj,
				   struct bnxt_re_dv_create_qp_req *init_attr)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	struct bnxt_re_ucontext *cntx = NULL;
	struct bnxt_qplib_qp *qplqp;
	struct bnxt_re_dev *rdev;
	struct bnxt_qplib_q *rq;
	struct bnxt_qplib_q *sq;
	u32 slot_size;
	int qptype;

	rdev = qp->rdev;
	qplqp = &qp->qplib_qp;
	dev_attr = rdev->dev_attr;
	cntx = to_bnxt_re(context, struct bnxt_re_ucontext, ib_uctx);

	/* Setup misc params */
	qplqp->is_user = true;
	qplqp->pd_id = init_attr->pd_id;
	qplqp->qp_handle = (u64)qplqp;
	qplqp->sig_type = false;
	qptype = __from_ib_qp_type(init_attr->qp_type);
	if (qptype < 0)
		return qptype;
	qplqp->type = (u8)qptype;
	qplqp->wqe_mode = rdev->chip_ctx->modes.wqe_mode;
	ether_addr_copy(qplqp->smac, rdev->netdev->dev_addr);
	qplqp->dev_cap_flags = dev_attr->dev_cap_flags;
	qplqp->cctx = rdev->chip_ctx;

	if (init_attr->qp_type == IB_QPT_RC) {
		qplqp->max_rd_atomic = dev_attr->max_qp_rd_atom;
		qplqp->max_dest_rd_atomic = dev_attr->max_qp_init_rd_atom;
	}
	qplqp->mtu = ib_mtu_enum_to_int(iboe_get_mtu(rdev->netdev->mtu));
	if (dbr_obj)
		qplqp->dpi = &dbr_obj->dpi;
	else
		qplqp->dpi = &cntx->dpi;

	/* Setup CQs */
	if (!send_cq) {
		dev_err(rdev_to_dev(rdev), "Send CQ not found");
		return -EINVAL;
	}
	qplqp->scq = &send_cq->qplib_cq;
	qp->scq = send_cq;

	if (!recv_cq) {
		dev_err(rdev_to_dev(rdev), "Receive CQ not found");
		return -EINVAL;
	}
	qplqp->rcq = &recv_cq->qplib_cq;
	qp->rcq = recv_cq;

	qplqp->small_recv_wqe_sup = cntx->small_recv_wqe_sup;

	if (!srq) {
		if (!init_attr->rq_wqe_sz) {
			dev_err(rdev_to_dev(rdev), "Invalid rq_wqe_sz");
			return -EINVAL;
		}

		/* Setup RQ */
		slot_size = bnxt_qplib_get_stride();
		rq = &qplqp->rq;
		rq->max_sge = init_attr->max_recv_sge;
		rq->wqe_size = init_attr->rq_wqe_sz;
		rq->max_wqe = (init_attr->rq_slots * slot_size) /
				init_attr->rq_wqe_sz;
		rq->max_sw_wqe = rq->max_wqe;
		rq->q_full_delta = 0;
		rq->sginfo.pgsize = PAGE_SIZE;
		rq->sginfo.pgshft = PAGE_SHIFT;
	}

	/* Setup SQ */
	sq = &qplqp->sq;
	sq->max_sge = init_attr->max_send_sge;
	sq->wqe_size = init_attr->sq_wqe_sz;
	sq->max_wqe = init_attr->sq_slots; /* SQ in var-wqe mode */
	sq->max_sw_wqe = sq->max_wqe;
	sq->q_full_delta = 0;
	sq->sginfo.pgsize = PAGE_SIZE;
	sq->sginfo.pgshft = PAGE_SHIFT;

	return 0;
}

static int bnxt_re_dv_init_user_qp(struct bnxt_re_dev *rdev,
				   struct ib_ucontext *context,
				   struct bnxt_re_qp *qp,
				   struct bnxt_re_srq *srq,
				   struct bnxt_re_dv_create_qp_req *init_attr,
				   struct bnxt_re_dv_umem *sq_umem,
				   struct bnxt_re_dv_umem *rq_umem)
{
	struct bnxt_qplib_sg_info *sginfo;
	struct bnxt_re_dv_umem *dv_umem;
	struct bnxt_qplib_qp *qplib_qp;
	struct bnxt_re_ucontext *cntx;
	struct ib_umem *umem;
	int bytes = 0, rc;

	qplib_qp = &qp->qplib_qp;
	cntx = to_bnxt_re(context, struct bnxt_re_ucontext, ib_uctx);
	qplib_qp->qp_handle = init_attr->qp_handle;
	sginfo = &qplib_qp->sq.sginfo;
	if (sq_umem) {
		dv_umem = bnxt_re_dv_umem_get(rdev, context, sq_umem,
					      init_attr->sq_umem_offset,
					      init_attr->sq_len, sginfo,
					      BNXT_RE_RES_TYPE_QP);
		if (IS_ERR(dv_umem)) {
			rc = PTR_ERR(dv_umem);
			dev_err(rdev_to_dev(rdev), "%s: bnxt_re_dv_umem_get() failed! rc = %d\n",
				__func__, rc);
			return -rc;
		}
		/* save the dv_umem we just created */
		qp->sq_umem = dv_umem;
		qp->sumem = dv_umem->umem;
	} else {
		bytes = init_attr->sq_len;
		umem = ib_umem_get_compat(rdev, context, NULL,
					  (unsigned long)init_attr->sq_va,
					  bytes, IB_ACCESS_LOCAL_WRITE, 1);
		if (IS_ERR(umem)) {
			dev_err(rdev_to_dev(rdev), "%s: ib_umem_get failed with %ld\n",
				__func__, PTR_ERR(umem));
			return PTR_ERR(umem);
		}
		dev_dbg(rdev_to_dev(rdev),
			"%s: mapped umem: sq_va: 0x%llx len: 0x%x\n",
			__func__, init_attr->sq_va, bytes);

		qp->sumem = umem;
		sginfo->npages = ib_umem_num_pages_compat(umem);
		/* pgsize and pgshft were initialize already. */
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
		sginfo->sghead = get_ib_umem_sgl(umem, &sginfo->nmap);
#else
		sginfo->umem = umem;
#endif
	}

	if (srq) {
		qplib_qp->srq = &srq->qplib_srq;
		goto done;
	}

	sginfo = &qplib_qp->rq.sginfo;
	if (rq_umem) {
		dv_umem = bnxt_re_dv_umem_get(rdev, context, rq_umem,
					      init_attr->rq_umem_offset,
					      init_attr->rq_len, sginfo,
					      BNXT_RE_RES_TYPE_QP);
		if (IS_ERR(dv_umem)) {
			rc = PTR_ERR(dv_umem);
			dev_err(rdev_to_dev(rdev), "%s: bnxt_re_dv_umem_get() failed! rc = %d\n",
				__func__, rc);
			goto rqfail;
		}
		/* save the dv_umem we just created */
		qp->rq_umem = dv_umem;
		qp->rumem = dv_umem->umem;
	} else {
		bytes = init_attr->rq_len;
		umem = ib_umem_get_compat(rdev, context, NULL,
					  (unsigned long)init_attr->rq_va,
					  bytes, IB_ACCESS_LOCAL_WRITE, 1);
		if (IS_ERR(umem)) {
			rc = PTR_ERR(umem);
			dev_err(rdev_to_dev(rdev),
				"%s: ib_umem_get failed rc =%d\n", __func__, rc);
			goto rqfail;
		}
		dev_dbg(rdev_to_dev(rdev),
			"%s: mapped umem: rq_va: 0x%llx len: 0x%x\n",
			__func__, init_attr->rq_va, bytes);

		qp->rumem = umem;
		sginfo->npages = ib_umem_num_pages_compat(umem);
		/* pgsize and pgshft were initialize already. */
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
		sginfo->sghead = get_ib_umem_sgl(umem, &sginfo->nmap);
#else
		sginfo->umem = umem;
#endif
	}

done:
	qplib_qp->is_user = true;
	return 0;

rqfail:
	bnxt_re_peer_mem_release(qp->sumem);
	kfree(qp->sq_umem);
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	qplib_qp->sq.sginfo.sghead = NULL;
	qplib_qp->sq.sginfo.nmap = 0;
#else
	qplib_qp->sq.sginfo.umem = NULL;
#endif

	return rc;
}

static void
bnxt_re_dv_qp_init_msn(struct bnxt_re_qp *qp,
		       struct bnxt_re_dv_create_qp_req *req)
{
	struct bnxt_qplib_qp *qplib_qp = &qp->qplib_qp;

	qplib_qp->is_host_msn_tbl = true;
	qplib_qp->msn = 0;
	qplib_qp->psn_sz = req->sq_psn_sz;
	qplib_qp->msn_tbl_sz = req->sq_psn_sz * req->sq_npsn;
}

/* Init some members of ib_qp for now; this may
 * not be needed in the final DV implementation.
 * Reference code: core/verbs.c::create_qp()
 */
static void bnxt_re_dv_init_ib_qp(struct bnxt_re_dev *rdev,
				  struct bnxt_re_qp *re_qp)
{
	struct bnxt_qplib_qp *qplib_qp = &re_qp->qplib_qp;
	struct ib_qp *ib_qp = &re_qp->ib_qp;

	ib_qp->device = &rdev->ibdev;
	ib_qp->qp_num = qplib_qp->id;
	ib_qp->real_qp = ib_qp;
	ib_qp->qp_type = IB_QPT_RC;
	ib_qp->send_cq = &re_qp->scq->ib_cq;
	ib_qp->recv_cq = &re_qp->rcq->ib_cq;
}

static void bnxt_re_dv_init_qp(struct bnxt_re_dev *rdev,
			       struct bnxt_re_qp *qp)
{
	u32 active_qps, tmp_qps;

	spin_lock_init(&qp->sq_lock);
	spin_lock_init(&qp->rq_lock);
	INIT_LIST_HEAD(&qp->list);
	INIT_LIST_HEAD(&qp->flow_list);
	mutex_init(&qp->flow_lock);
	mutex_lock(&rdev->qp_lock);
	list_add_tail(&qp->list, &rdev->qp_list);
	mutex_unlock(&rdev->qp_lock);
	atomic_inc(&rdev->stats.rsors.qp_count);
	active_qps = atomic_read(&rdev->stats.rsors.qp_count);
	if (active_qps > atomic_read(&rdev->stats.rsors.max_qp_count))
		atomic_set(&rdev->stats.rsors.max_qp_count, active_qps);
	bnxt_re_qp_info_add_qpinfo(rdev, qp);
	BNXT_RE_RES_LIST_ADD(rdev, qp, BNXT_RE_RES_TYPE_QP);

	/* Get the counters for RC QPs and UD QPs */
	tmp_qps = atomic_inc_return(&rdev->stats.rsors.rc_qp_count);
	if (tmp_qps > atomic_read(&rdev->stats.rsors.max_rc_qp_count))
		atomic_set(&rdev->stats.rsors.max_rc_qp_count, tmp_qps);

	bnxt_re_dv_init_ib_qp(rdev, qp);
}

static int UVERBS_HANDLER(BNXT_RE_METHOD_DV_CREATE_QP)(struct uverbs_attr_bundle *attrs)
{
	struct ib_uobject *uobj =
		uverbs_attr_get_uobject(attrs, BNXT_RE_DV_CREATE_QP_HANDLE);
	struct bnxt_re_alloc_dbr_obj *dbr_obj = NULL;
	struct bnxt_re_dv_create_qp_resp resp = {};
	struct bnxt_re_dv_create_qp_req req = {};
	struct bnxt_re_dv_umem *sq_umem = NULL;
	struct bnxt_re_dv_umem *rq_umem = NULL;
	struct bnxt_re_ucontext *re_uctx;
	struct bnxt_re_srq *srq = NULL;
	struct bnxt_re_dv_umem *umem;
	struct ib_ucontext *ib_uctx;
	struct bnxt_re_cq *send_cq;
	struct bnxt_re_cq *recv_cq;
	struct bnxt_re_dev *rdev;
	struct bnxt_re_qp *re_qp;
	struct ib_srq *ib_srq;
	int ret;

	if (IS_ERR(uobj))
		return PTR_ERR(uobj);

	ib_uctx = ib_uverbs_get_ucontext(attrs);
	if (IS_ERR(ib_uctx))
		return PTR_ERR(ib_uctx);

	re_uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	rdev = re_uctx->rdev;

	ret = uverbs_copy_from_or_zero(&req, attrs, BNXT_RE_DV_CREATE_QP_REQ);
	if (ret) {
		dev_err(rdev_to_dev(rdev), "%s: uverbs_copy_to() failed: %d\n",
			__func__, ret);
		return ret;
	}

	send_cq = uverbs_attr_get_obj(attrs,
				      BNXT_RE_DV_CREATE_QP_SEND_CQ_HANDLE);
	if (IS_ERR(send_cq))
		return PTR_ERR(send_cq);

	recv_cq = uverbs_attr_get_obj(attrs,
				      BNXT_RE_DV_CREATE_QP_RECV_CQ_HANDLE);
	if (IS_ERR(recv_cq))
		return PTR_ERR(recv_cq);

	bnxt_re_print_dv_qp_attr(rdev, send_cq, recv_cq, &req);

	re_qp = kzalloc(sizeof(*re_qp), GFP_KERNEL);
	if (!re_qp)
		return -ENOMEM;

	re_qp->rdev = rdev;
	umem = uverbs_attr_get_obj(attrs, BNXT_RE_DV_CREATE_QP_SQ_UMEM_HANDLE);
	if (!IS_ERR(umem))
		sq_umem = umem;

	umem = uverbs_attr_get_obj(attrs, BNXT_RE_DV_CREATE_QP_RQ_UMEM_HANDLE);
	if (!IS_ERR(umem))
		rq_umem = umem;

	ib_srq = uverbs_attr_get_obj(attrs, BNXT_RE_DV_CREATE_QP_SRQ_HANDLE);
	if (!IS_ERR(ib_srq))
		srq = to_bnxt_re(ib_srq, struct bnxt_re_srq, ib_srq);

	if (uverbs_attr_is_valid(attrs, BNXT_RE_DV_CREATE_QP_DBR_HANDLE))
		dbr_obj = uverbs_attr_get_obj(attrs, BNXT_RE_DV_CREATE_QP_DBR_HANDLE);

	ret = bnxt_re_dv_init_qp_attr(re_qp, ib_uctx, send_cq, recv_cq, srq,
				      dbr_obj, &req);
	if (ret) {
		dev_err(rdev_to_dev(rdev), "Failed to initialize qp attr");
		goto free_qp;
	}

	ret = bnxt_re_dv_init_user_qp(rdev, ib_uctx, re_qp, srq, &req, sq_umem, rq_umem);
	if (ret)
		goto free_qp;

	bnxt_re_dv_qp_init_msn(re_qp, &req);

	ret = bnxt_re_setup_qp_hwqs(re_qp, true);
	if (ret)
		goto free_umem;

	ret = bnxt_qplib_create_qp(&rdev->qplib_res, &re_qp->qplib_qp);
	if (ret) {
		dev_err(rdev_to_dev(rdev), "create HW QP failed!");
		goto free_hwq;
	}

	resp.qpid = re_qp->qplib_qp.id;
	ret = bnxt_re_dv_uverbs_copy_to(rdev, attrs, BNXT_RE_DV_CREATE_QP_RESP,
					&resp, sizeof(resp));
	if (ret) {
		dev_err(rdev_to_dev(rdev),
			"%s: Failed to copy cq response: %d\n", __func__, ret);
		goto free_qplib;
	}

	bnxt_re_dv_finalize_uobj(uobj, re_qp, attrs, BNXT_RE_DV_CREATE_QP_HANDLE);
	bnxt_re_dv_init_qp(rdev, re_qp);
	re_qp->is_dv_qp = true;
	atomic_inc(&rdev->dv_qp_count);
	dev_dbg(rdev_to_dev(rdev), "%s: Created QP: 0x%llx, handle: 0x%x\n",
		__func__, (u64)re_qp, uobj->id);
	if (dbr_obj)
		dev_dbg(rdev_to_dev(rdev), "%s: QP DPI index: 0x%x\n",
			__func__, re_qp->qplib_qp.dpi->dpi);

	return 0;

free_qplib:
	bnxt_qplib_destroy_qp(&rdev->qplib_res, &re_qp->qplib_qp, 0, NULL, NULL);
free_hwq:
	bnxt_qplib_free_qp_res(&rdev->qplib_res, &re_qp->qplib_qp);
free_umem:
	bnxt_re_qp_free_umem(re_qp);
free_qp:
	kfree(re_qp);
	return ret;
}

void bnxt_re_dv_destroy_qp(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp)
{
	struct bnxt_qplib_qp *qplib_qp = &qp->qplib_qp;
	struct creq_destroy_qp_resp resp = {};
	struct bnxt_qplib_nq *scq_nq = NULL;
	struct bnxt_qplib_nq *rcq_nq = NULL;
	void  *ctx_sb_data = NULL;
	u32 active_qps;
	u16 ctx_size;
	int rc;

	mutex_lock(&rdev->qp_lock);
	list_del(&qp->list);
	active_qps = atomic_dec_return(&rdev->stats.rsors.qp_count);
	if (qplib_qp->type == CMDQ_CREATE_QP_TYPE_RC)
		atomic_dec(&rdev->stats.rsors.rc_qp_count);
	mutex_unlock(&rdev->qp_lock);

	bnxt_re_qp_info_rem_qpinfo(rdev, qp);

	ctx_size = (qplib_qp->ctx_size_sb + rdev->rcfw.udcc_session_sb_sz);
	if (ctx_size)
		ctx_sb_data = vzalloc(ctx_size);

	rc = bnxt_qplib_destroy_qp(&rdev->qplib_res, qplib_qp,
				   ctx_size, ctx_sb_data,
				   (rdev->rcfw.roce_udcc_session_destroy_sb ?
				    (void *)&resp : NULL));

	if (rc)
		dev_err_ratelimited(rdev_to_dev(rdev), "%s id = %d failed rc = %d",
				    __func__, qplib_qp->id, rc);

	vfree(ctx_sb_data);
	bnxt_qplib_free_qp_res(&rdev->qplib_res, qplib_qp);
	bnxt_re_qp_free_umem(qp);

	/* Flush all the entries of notification queue associated with
	 * given qp.
	 */
	scq_nq = qplib_qp->scq->nq;
	rcq_nq = qplib_qp->rcq->nq;
	bnxt_re_synchronize_nq(scq_nq);
	if (scq_nq != rcq_nq)
		bnxt_re_synchronize_nq(rcq_nq);

	if (qp->sgid_attr)
		rdma_put_gid_attr(qp->sgid_attr);
	atomic_dec(&rdev->dv_qp_count);
	kfree(qp);
}

static int bnxt_re_dv_clean_fw_msg(struct ib_uobject *uobj,
				   enum rdma_remove_reason why,
				   struct uverbs_attr_bundle *attrs)
{
	netdev_err(NULL, "$ACK:%s:%d\n", __func__, __LINE__);
	return 0;
}

static int bnxt_re_dv_destroy_obj(struct ib_uobject *uobj,
				  enum rdma_remove_reason why,
				  struct uverbs_attr_bundle *attrs)
{
	struct bnxt_re_dv_create_obj_attr *obj_attr;
	struct bnxt_re_dev *rdev = NULL;
	struct bnxt_re_ucontext *re_uctx;

	if (!uobj)
		return 0;

	obj_attr = uobj->object;
	if (!obj_attr)
		goto free_mem;

	if (attrs && attrs->context) {
		re_uctx = container_of(attrs->context, struct bnxt_re_ucontext, ib_uctx);
		rdev = re_uctx->rdev;
	}

	if (!rdev)
		goto free_mem;

	switch (obj_attr->obj_type) {
	case BNXT_RE_OBJ_TYPE_TX:
	case BNXT_RE_OBJ_TYPE_RX:
	case BNXT_RE_OBJ_TYPE_RX_AGGR:
	case BNXT_RE_OBJ_TYPE_MPC:
	{
		u16 ring_id = obj_attr->resp_data.fw_ring_id;
		u32 ring_type;

		if (obj_attr->obj_type == BNXT_RE_OBJ_TYPE_TX ||
		    obj_attr->obj_type == BNXT_RE_OBJ_TYPE_MPC)
			ring_type = RING_FREE_REQ_RING_TYPE_TX;
		else if (obj_attr->obj_type == BNXT_RE_OBJ_TYPE_RX)
			ring_type = RING_FREE_REQ_RING_TYPE_RX;
		else
			ring_type = RING_FREE_REQ_RING_TYPE_RX_AGG;

		if (ring_id != INVALID_HW_RING_ID && ring_id != 0)
			bnxt_re_net_ring_free(rdev, ring_type, ring_id);
		/* Free hwq if it was allocated */
		if (obj_attr->hwq.max_elements)
			bnxt_qplib_free_hwq(&rdev->qplib_res, &obj_attr->hwq);

		/* Free pinned umem if it exists */
		if (obj_attr->umem_handle && obj_attr->umem_handle->umem) {
			bnxt_re_peer_mem_release(obj_attr->umem_handle->umem);
			kfree(obj_attr->umem_handle);
		}

		/* Release quota after successful ring free */
		if (obj_attr->obj_type == BNXT_RE_OBJ_TYPE_TX)
			bnxt_re_release_resource_quota(rdev, BNXT_ULP_RES_TX);
		else if ((obj_attr->obj_type == BNXT_RE_OBJ_TYPE_RX) ||
			 (obj_attr->obj_type == BNXT_RE_OBJ_TYPE_RX_AGGR))
			bnxt_re_release_resource_quota(rdev, BNXT_ULP_RES_RX);
	}
	break;
	case BNXT_RE_OBJ_TYPE_STATS:
		bnxt_re_net_stats_ctx_free(rdev, obj_attr->resp_data.fw_stats_ctx_id, 0xffff);
		/* Release pinned umem if it exists (for stats context) */
		if (obj_attr->umem_handle && obj_attr->umem_handle->umem) {
			bnxt_re_peer_mem_release(obj_attr->umem_handle->umem);
			kfree(obj_attr->umem_handle);
		}
		bnxt_re_release_resource_quota(rdev, BNXT_ULP_RES_STAT_CTX);
		break;
	case BNXT_RE_OBJ_TYPE_VNIC:
		bnxt_re_hwrm_free_vnic(rdev, obj_attr->resp_data.fw_vnic_id);
		bnxt_re_release_resource_quota(rdev, BNXT_ULP_RES_VNIC);
		break;

	case BNXT_RE_OBJ_TYPE_BFID:
		mutex_lock(&rdev->bfid_lock);
		bnxt_ulp_tfc_bfid_reset(rdev->en_dev,
					obj_attr->resp_data.bfid);
		mutex_unlock(&rdev->bfid_lock);
		bnxt_re_hwrm_free_bfid(rdev, obj_attr->resp_data.bfid);
		break;

	case BNXT_RE_OBJ_TYPE_RSS:
		bnxt_re_hwrm_free_rss_ctx(rdev, obj_attr->resp_data.fw_rss_ctx_id);
		bnxt_re_release_resource_quota(rdev, BNXT_ULP_RES_RSS_CTX);
		break;

	default:
		break;
	}

free_mem:
	kfree(obj_attr);
	uobj->object = NULL;
	return 0;
}

static int bnxt_re_dv_free_qp(struct ib_uobject *uobj,
			      enum rdma_remove_reason why,
			      struct uverbs_attr_bundle *attrs)
{
	struct bnxt_re_qp *qp = uobj->object;
	struct bnxt_re_dev *rdev = qp->rdev;

	dev_dbg(rdev_to_dev(rdev), "%s: Destroy QP: 0x%llx, handle: 0x%x\n",
		__func__, (u64)qp, uobj->id);
	BNXT_RE_RES_LIST_DEL(rdev, qp, BNXT_RE_RES_TYPE_QP);
	bnxt_re_dv_destroy_qp(rdev, qp);
	uobj->object = NULL;
	return 0;
}

static void bnxt_re_copyout_ah_attr(struct ib_uverbs_ah_attr *dattr,
				    struct rdma_ah_attr *sattr)
{
	dattr->sl               = sattr->sl;
	memcpy(dattr->grh.dgid, &sattr->grh.dgid, 16);
	dattr->grh.flow_label = sattr->grh.flow_label;
	dattr->grh.hop_limit = sattr->grh.hop_limit;
	dattr->grh.sgid_index = sattr->grh.sgid_index;
	dattr->grh.traffic_class = sattr->grh.traffic_class;
}

static void bnxt_re_dv_copy_qp_attr_out(struct bnxt_re_dev *rdev,
					struct ib_uverbs_qp_attr *out,
					struct ib_qp_attr *qp_attr,
					struct ib_qp_init_attr *qp_init_attr)
{
	out->qp_state = qp_attr->qp_state;
	out->cur_qp_state = qp_attr->cur_qp_state;
	out->path_mtu = qp_attr->path_mtu;
	out->path_mig_state = qp_attr->path_mig_state;
	out->qkey = qp_attr->qkey;
	out->rq_psn = qp_attr->rq_psn;
	out->sq_psn = qp_attr->sq_psn;
	out->dest_qp_num = qp_attr->dest_qp_num;
	out->qp_access_flags = qp_attr->qp_access_flags;
	out->max_send_wr = qp_attr->cap.max_send_wr;
	out->max_recv_wr = qp_attr->cap.max_recv_wr;
	out->max_send_sge = qp_attr->cap.max_send_sge;
	out->max_recv_sge = qp_attr->cap.max_recv_sge;
	out->max_inline_data = qp_attr->cap.max_inline_data;
	out->pkey_index = qp_attr->pkey_index;
	out->alt_pkey_index = qp_attr->alt_pkey_index;
	out->en_sqd_async_notify = qp_attr->en_sqd_async_notify;
	out->sq_draining = qp_attr->sq_draining;
	out->max_rd_atomic = qp_attr->max_rd_atomic;
	out->max_dest_rd_atomic = qp_attr->max_dest_rd_atomic;
	out->min_rnr_timer = qp_attr->min_rnr_timer;
	out->port_num = qp_attr->port_num;
	out->timeout = qp_attr->timeout;
	out->retry_cnt = qp_attr->retry_cnt;
	out->rnr_retry = qp_attr->rnr_retry;
	out->alt_port_num = qp_attr->alt_port_num;
	out->alt_timeout = qp_attr->alt_timeout;

	bnxt_re_copyout_ah_attr(&out->ah_attr, &qp_attr->ah_attr);
	bnxt_re_copyout_ah_attr(&out->alt_ah_attr, &qp_attr->alt_ah_attr);
}

static int UVERBS_HANDLER(BNXT_RE_METHOD_DV_QUERY_QP)(struct uverbs_attr_bundle *attrs)
{
	struct ib_qp_init_attr qp_init_attr = {};
	struct ib_uverbs_qp_attr attr = {};
	struct bnxt_re_ucontext *re_uctx;
	struct ib_qp_attr qp_attr = {};
	struct ib_ucontext *ib_uctx;
	struct bnxt_re_dev *rdev;
	struct ib_uobject *uobj;
	struct bnxt_re_qp *qp;
	int ret;

	uobj = uverbs_attr_get_uobject(attrs, BNXT_RE_DV_QUERY_QP_HANDLE);
	qp = uobj->object;

	ib_uctx = ib_uverbs_get_ucontext(attrs);
	if (IS_ERR(ib_uctx))
		return PTR_ERR(ib_uctx);

	re_uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	rdev = re_uctx->rdev;

	ret = bnxt_re_query_qp_attr(qp, &qp_attr, 0, &qp_init_attr);
	if (ret)
		return ret;

	bnxt_re_dv_copy_qp_attr_out(rdev, &attr, &qp_attr, &qp_init_attr);

	ret = uverbs_copy_to(attrs, BNXT_RE_DV_QUERY_QP_ATTR, &attr,
			     sizeof(attr));
	if (ret)
		return ret;

	return 0;
}

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_DV_CREATE_QP,
			    UVERBS_ATTR_IDR(BNXT_RE_DV_CREATE_QP_HANDLE,
					    BNXT_RE_OBJECT_DV_QP,
					    UVERBS_ACCESS_NEW,
					    UA_MANDATORY),
			    UVERBS_ATTR_PTR_IN(BNXT_RE_DV_CREATE_QP_REQ,
					       UVERBS_ATTR_STRUCT(struct bnxt_re_dv_create_qp_req,
								  rq_slots),
					       UA_MANDATORY),
			    UVERBS_ATTR_IDR(BNXT_RE_DV_CREATE_QP_SEND_CQ_HANDLE,
					    BNXT_RE_OBJECT_DV_CQ,
					    UVERBS_ACCESS_READ,
					    UA_MANDATORY),
			    UVERBS_ATTR_IDR(BNXT_RE_DV_CREATE_QP_RECV_CQ_HANDLE,
					    BNXT_RE_OBJECT_DV_CQ,
					    UVERBS_ACCESS_READ,
					    UA_MANDATORY),
			    UVERBS_ATTR_IDR(BNXT_RE_DV_CREATE_QP_SQ_UMEM_HANDLE,
					    BNXT_RE_OBJECT_UMEM,
					    UVERBS_ACCESS_READ,
					    UA_OPTIONAL),
			    UVERBS_ATTR_IDR(BNXT_RE_DV_CREATE_QP_RQ_UMEM_HANDLE,
					    BNXT_RE_OBJECT_UMEM,
					    UVERBS_ACCESS_READ,
					    UA_OPTIONAL),
			    UVERBS_ATTR_IDR(BNXT_RE_DV_CREATE_QP_SRQ_HANDLE,
					    UVERBS_OBJECT_SRQ,
					    UVERBS_ACCESS_READ,
					    UA_OPTIONAL),
			    UVERBS_ATTR_IDR(BNXT_RE_DV_CREATE_QP_DBR_HANDLE,
					    BNXT_RE_OBJECT_DBR,
					    UVERBS_ACCESS_READ,
					    UA_OPTIONAL),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_DV_CREATE_QP_RESP,
						UVERBS_ATTR_STRUCT(struct bnxt_re_dv_create_qp_resp,
								   qpid),
						UA_MANDATORY));

DECLARE_UVERBS_NAMED_METHOD_DESTROY(BNXT_RE_METHOD_DV_DESTROY_QP,
				    UVERBS_ATTR_IDR(BNXT_RE_DV_DESTROY_QP_HANDLE,
						    BNXT_RE_OBJECT_DV_QP,
						    UVERBS_ACCESS_DESTROY,
						    UA_MANDATORY));

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_DV_QUERY_QP,
			    UVERBS_ATTR_IDR(BNXT_RE_DV_QUERY_QP_HANDLE,
					    BNXT_RE_OBJECT_DV_QP,
					    UVERBS_ACCESS_READ,
					    UA_MANDATORY),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_DV_QUERY_QP_ATTR,
						UVERBS_ATTR_STRUCT(struct ib_uverbs_qp_attr,
								   reserved),
						UA_MANDATORY));

DECLARE_UVERBS_NAMED_OBJECT(BNXT_RE_OBJECT_DV_QP,
			    UVERBS_TYPE_ALLOC_IDR(bnxt_re_dv_free_qp),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DV_CREATE_QP),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DV_DESTROY_QP),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DV_MODIFY_QP),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DV_QUERY_QP),
);

static int bnxt_re_send_direct_cmd(struct bnxt_re_dev *rdev, void *req, int req_len,
				   void *resp, int resp_len)
{
	struct bnxt_fw_msg fw_msg = {};

	bnxt_re_fill_fw_msg(&fw_msg, req, req_len, resp,
			    resp_len, BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	return bnxt_send_msg(rdev->en_dev, &fw_msg);
}

static int UVERBS_HANDLER(BNXT_RE_METHOD_DV_SEND_FW_MSG)(struct uverbs_attr_bundle *attrs)
{
	struct bnxt_re_ucontext *uctx;
	struct ib_ucontext *ib_uctx;
	struct bnxt_re_dev *rdev;
	void *msg_in, *resp;
	u32 in_len, out_len;
	int err;

	ib_uctx = ib_uverbs_get_ucontext(attrs);
	if (IS_ERR(ib_uctx))
		return PTR_ERR(ib_uctx);
	uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	rdev = uctx->rdev;

	msg_in = uverbs_attr_get_alloced_ptr(attrs, BNXT_RE_SEND_FW_MSG_IN);
	in_len = uverbs_attr_get_len(attrs, BNXT_RE_SEND_FW_MSG_IN);
	out_len = uverbs_attr_get_len(attrs, BNXT_RE_SEND_FW_MSG_OUT);

	resp = uverbs_zalloc(attrs, out_len);
	if (IS_ERR(resp))
		return PTR_ERR(resp);

	err = bnxt_re_send_direct_cmd(rdev, msg_in, in_len,
				      resp, out_len);
	if (err) {
		netdev_err(rdev->netdev, "$ACK:%s:%d: in_len = %x out_len = %x, ERR = %x\n",
			   __func__, __LINE__, in_len, out_len, err);
		return err;
	}
	err = uverbs_copy_to(attrs, BNXT_RE_SEND_FW_MSG_OUT, resp, out_len);
	return err;
}

static void bnxt_re_print_ring_attr(struct bnxt_re_dev *rdev,
				    struct bnxt_re_ring_attr *ring_attr,
				    const char *func)
{
	if (!ring_attr) {
		dev_dbg(rdev_to_dev(rdev), "%s: ring_attr is NULL\n", func);
		return;
	}

	dev_dbg(rdev_to_dev(rdev),
		"%s: bnxt_re_ring_attr:\n"
		"  dma_arr:        %p\n"
		"  pages:          %d\n"
		"  type:           0x%08x (%u) [%s]\n"
		"  depth:          0x%08x (%u)\n"
		"  lrid:           0x%08x (%u)\n"
		"  flags:          0x%04x (%u)\n"
		"  nq_ring_id:     0x%04x (%u)\n"
		"  cmpl_ring_id:   0x%04x (%u)\n"
		"  rx_ring_id:     0x%04x (%u)\n"
		"  stat_ctx_id:    0x%08x (%u)\n"
		"  dpi:            0x%08x (%u)\n"
		"  mtu:            0x%08x (%u)\n"
		"  mpc_chnl_type:  0x%02x (%u)\n",
		func,
		ring_attr->dma_arr,
		ring_attr->pages,
		ring_attr->type, ring_attr->type,
		(ring_attr->type == RING_TYPE_L2_CMPL) ? "L2_CMPL" :
		(ring_attr->type == RING_TYPE_TX) ? "TX" :
		(ring_attr->type == RING_TYPE_RX) ? "RX" :
		(ring_attr->type == RING_TYPE_ROCE_CMPL) ? "ROCE_CMPL" :
		(ring_attr->type == RING_TYPE_RX_AGG) ? "RX_AGG" :
		(ring_attr->type == RING_TYPE_NQ) ? "NQ" :
		(ring_attr->type == RING_TYPE_MPC) ? "MPC" : "UNKNOWN",
		ring_attr->depth, ring_attr->depth,
		ring_attr->lrid, ring_attr->lrid,
		ring_attr->flags, ring_attr->flags,
		ring_attr->nq_ring_id, ring_attr->nq_ring_id,
		ring_attr->cmpl_ring_id, ring_attr->cmpl_ring_id,
		ring_attr->rx_ring_id, ring_attr->rx_ring_id,
		ring_attr->stat_ctx_id, ring_attr->stat_ctx_id,
		ring_attr->dpi, ring_attr->dpi,
		ring_attr->mtu, ring_attr->mtu,
		ring_attr->mpc_chnl_type, ring_attr->mpc_chnl_type);
}

/* Handle TX ring allocation */
static int bnxt_re_dv_obj_create_ring(struct bnxt_re_dev *rdev, struct bnxt_en_dev *en_dev,
				      u8 obj_type, struct ib_ucontext *ib_uctx,
				      struct bnxt_re_dv_umem *umem_handle, u64 offset,
				      struct bnxt_re_dv_ring_attr *ring_attr,
				      struct bnxt_re_cq *com_cq,
				      struct bnxt_re_dv_ring_resp *resp,
				      struct bnxt_re_dv_create_obj_attr *obj_attr,
				      u16 stats_ctx_id, u16 rx_ring_id)
{
	struct bnxt_re_ring_attr rattr = {};
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_sg_info sginfo = {};
	struct bnxt_re_ucontext *re_uctx;
	struct bnxt_re_dv_umem *dv_umem;
	u16 fw_ring_id;
	u32 stride;
	u64 ring_size;
	int rc = -EINVAL;

	re_uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	if (!com_cq) {
		dev_err(rdev_to_dev(rdev), "%s: com_cq is NULL\n", __func__);
		return rc;
	}
	if (!umem_handle) {
		dev_err(rdev_to_dev(rdev), "%s: Invalid umem_handle\n", __func__);
		return rc;
	}

	stride = bnxt_qplib_get_stride();
	ring_size = (u64)ring_attr->depth * stride;
	dv_umem = bnxt_re_dv_umem_get(rdev, ib_uctx, umem_handle,
				      offset, ring_size, &sginfo,
				      BNXT_RE_RES_TYPE_QP);
	if (IS_ERR(dv_umem)) {
		rc = PTR_ERR(dv_umem);
		dev_err(rdev_to_dev(rdev), "%s: bnxt_re_dv_umem_get() failed! rc = %d\n",
			__func__, rc);
		return rc;
	}

	hwq_attr.res = &rdev->qplib_res;
	hwq_attr.sginfo = &sginfo;
	hwq_attr.depth = ring_attr->depth;
	hwq_attr.stride = stride;
	hwq_attr.aux_stride = 0;
	hwq_attr.aux_depth = 0;
	hwq_attr.type = HWQ_TYPE_QUEUE;

	obj_attr->umem_handle = dv_umem;
	memset(&obj_attr->hwq, 0, sizeof(obj_attr->hwq));
	rc = bnxt_qplib_alloc_init_hwq(&obj_attr->hwq, &hwq_attr);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"%s: bnxt_qplib_alloc_init_hwq() failed: %d\n",
			__func__, rc);
		goto free_umem;
	}

	if (obj_type == BNXT_RE_OBJ_TYPE_TX)
		rattr.type = RING_TYPE_TX;
	else if (obj_type == BNXT_RE_OBJ_TYPE_RX_AGGR)
		rattr.type = RING_TYPE_RX_AGG;
	else
		rattr.type = RING_TYPE_RX;

	rattr.dma_arr = obj_attr->hwq.pbl[PBL_LVL_0].pg_map_arr;
	rattr.pages = obj_attr->hwq.pbl[obj_attr->hwq.level].pg_count;

	if (obj_type == BNXT_RE_OBJ_TYPE_MPC) {
		rattr.type = RING_TYPE_MPC;
		rattr.mpc_chnl_type = ring_attr->mpc_chnl_type;
	}

	rattr.depth = obj_attr->hwq.max_elements;
	rattr.lrid = rdev->nqr->msix_entries[1].ring_idx;
	rattr.pbl_lvl = obj_attr->hwq.level;
	rattr.flags = RING_ALLOC_REQ_FLAGS_DPI_ROCE_MANAGED;
	rattr.rx_buf_sz = ring_attr->rx_buf_size;
	rattr.cmpl_ring_id = com_cq->qplib_cq.id;
	rattr.fbo = sginfo.fwo_offset;
	rattr.pgsz = sginfo.pgsize;

	if (ring_attr->traffic_class < rdev->tc_rec[0].max_tc)
		rattr.queue_id = rdev->tc2cos[ring_attr->traffic_class];
	else
		rattr.queue_id = rdev->tc_rec[0].cos_id_def;

	/* RX_AGGR doesn't use NQ, skip nq_ring_id setup */
	if (obj_type != BNXT_RE_OBJ_TYPE_RX_AGGR) {
		if (com_cq->qplib_cq.nq) {
			rattr.nq_ring_id = com_cq->qplib_cq.nq->ring_id;
		} else {
			dev_err(rdev_to_dev(rdev), "%s: com_cq->qplib_cq.nq is NULL\n",
				__func__);
			rc = -EINVAL;
			goto free_hwq;
		}
	}
	rattr.stat_ctx_id = stats_ctx_id;
	rattr.dpi = re_uctx->dpi.dpi;
	/* For RX_AGGR, set rx_ring_id and mtu */
	if (obj_type == BNXT_RE_OBJ_TYPE_RX_AGGR) {
		rattr.rx_ring_id = rx_ring_id;
		rattr.mtu = ib_mtu_enum_to_int(iboe_get_mtu(rdev->netdev->mtu));
	}
	bnxt_re_print_ring_attr(rdev, &rattr, __func__);

	rc = bnxt_re_net_ring_alloc(rdev, &rattr, &fw_ring_id);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"%s: bnxt_re_net_ring_alloc() failed: %d\n",
			__func__, rc);
		goto free_hwq;
	} else {
		dev_dbg(rdev_to_dev(rdev), "%s: fw_ring_id = %x\n", __func__, fw_ring_id);
	}

	/* Populate response */
	resp->ring_id = fw_ring_id;
	resp->comp_mask = 0;

	/* Store ring_id in obj_attr for cleanup */
	obj_attr->resp_data.fw_ring_id = fw_ring_id;

	/* Note: hwq is now stored in obj_attr and will be freed in cleanup path */
	/* The umem needs to remain pinned while the ring is active */
	return 0;

free_hwq:
	bnxt_qplib_free_hwq(&rdev->qplib_res, &obj_attr->hwq);
free_umem:
	/* Release the pinned umem */
	if (dv_umem && dv_umem->umem) {
		bnxt_re_peer_mem_release(dv_umem->umem);
		kfree(dv_umem);
	}
	return rc;
}

/* Handle Stats context allocation */
static struct bnxt_re_dv_umem *bnxt_re_dv_create_stats_ctx(struct bnxt_re_dev *rdev,
							   struct bnxt_en_dev *en_dev,
							   struct bnxt_re_dv_stats_ctx_attr *attrs,
							   struct bnxt_re_dv_umem *umem_handle,
							   u64 offset,
							   struct uverbs_attr_bundle *attrs_bundle,
							   struct bnxt_re_dv_stats_ctx_resp *resp)
{
	struct hwrm_stat_ctx_alloc_output hwrm_resp = {};
	struct hwrm_stat_ctx_alloc_input req = {};
	struct bnxt_fw_msg fw_msg = {};
#ifdef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	struct ib_block_iter biter;
	bool first = true;
#endif
	struct bnxt_re_dv_umem *pinned_umem = NULL;
	struct ib_umem *umem = NULL;
	struct ib_ucontext *ib_uctx;
	/* coverity[UNUSED_VALUE:SUPPRESS] */
	struct uverbs_attr_bundle attr = {};
	dma_addr_t dma_addr = 0;
	u64 stats_addr;
	size_t stats_size;
	int ret;

	/* Validate umem_handle */
	if (!umem_handle) {
		dev_err(rdev_to_dev(rdev), "%s: Invalid umem_handle\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	/* Get ucontext from attrs */
	ib_uctx = ib_uverbs_get_ucontext(attrs_bundle);
	if (IS_ERR(ib_uctx)) {
		dev_err(rdev_to_dev(rdev), "%s: Failed to get ucontext: %ld\n",
			__func__, PTR_ERR(ib_uctx));
		return ERR_PTR(PTR_ERR(ib_uctx));
	}

	/* Allocate new bnxt_re_dv_umem structure */
	pinned_umem = kzalloc(sizeof(*pinned_umem), GFP_KERNEL);
	if (!pinned_umem)
		return ERR_PTR(-ENOMEM);

	/* Calculate the address and size for stats buffer */
	stats_addr = umem_handle->addr + offset;
	stats_size = attrs->stats_size;

	if (!bnxt_re_dv_is_valid_umem(umem_handle, offset, stats_size)) {
		dev_err(rdev_to_dev(rdev),
			"%s: Invalid umem offset/size: offset=0x%llx size=0x%zx umem_size=0x%lx\n",
			__func__, offset, stats_size, umem_handle->size);
		ret = -EINVAL;
		goto free_umem;
	}

	if (attrs->stats_dma_length > stats_size) {
		dev_err(rdev_to_dev(rdev),
			"Invalid stats_dma_length: dma_length=0x%x size=0x%zx\n",
			attrs->stats_dma_length, stats_size);
		ret = -EINVAL;
		goto free_umem;
	}

	/* Pin the memory using ib_umem_get_flags_compat to get DMA address */
	attr.context = ib_uctx;
	umem = ib_umem_get_flags_compat(rdev, ib_uctx,
					&attr.driver_udata, stats_addr,
					stats_size, umem_handle->access, 0);
	if (IS_ERR(umem)) {
		ret = PTR_ERR(umem);
		dev_err(rdev_to_dev(rdev),
			"%s: ib_umem_get_flags_compat failed! rc = %d\n",
			__func__, ret);
		goto free_umem;
	}

	/* Initialize the pinned_umem structure */
	pinned_umem->rdev = rdev;
	pinned_umem->umem = umem;
	pinned_umem->addr = stats_addr;
	pinned_umem->size = stats_size;
	pinned_umem->access = umem_handle->access;
	pinned_umem->dmabuf_fd = umem_handle->dmabuf_fd;

	/* Get DMA address from the pinned umem - assume single physical address */
#ifdef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	rdma_umem_for_each_dma_block(umem, &biter, PAGE_SIZE) {
		if (first) {
			dma_addr = rdma_block_iter_dma_address(&biter);
			first = false;
			break; /* Only need first address */
		}
	}
#else
	struct scatterlist *sg;
	u32 nmap;

	sg = get_ib_umem_sgl(umem, &nmap);
	if (sg && nmap > 0)
		dma_addr = sg_dma_address(sg);
	netdev_err(rdev->netdev, "$ACK:%s:%d, dma_addr =%p\n", __func__, __LINE__, dma_addr);

#endif
	if (!dma_addr) {
		dev_err(rdev_to_dev(rdev), "%s: Failed to get DMA address\n", __func__);
		ret = -EINVAL;
		goto release_umem;
	}

	/* Initialize HWRM request */
	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_STAT_CTX_ALLOC, 0xffff);
	req.update_period_ms = cpu_to_le32(attrs->update_period_ms);
	req.stats_dma_length = attrs->stats_dma_length;
	req.stats_dma_addr = cpu_to_le64(dma_addr);
	req.stat_ctx_flags = STAT_CTX_ALLOC_REQ_STAT_CTX_FLAGS_ROCE;

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&hwrm_resp,
			    sizeof(hwrm_resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	ret = bnxt_send_msg(en_dev, &fw_msg);
	if (ret) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to allocate HW stats ctx, rc = 0x%x", ret);
		goto release_umem;
	}

	/* Check for HWRM error */
	if (hwrm_resp.error_code) {
		dev_err(rdev_to_dev(rdev), "%s: HWRM stats ctx alloc error: %d\n",
			__func__, le16_to_cpu(hwrm_resp.error_code));
		ret = -EIO;
		goto release_umem;
	}

	/* Populate response structure */
	resp->stat_ctx_id = le32_to_cpu(hwrm_resp.stat_ctx_id);
	resp->comp_mask = 0;

	/* Return the pinned umem structure - it will be stored in obj_attr */
	/* The umem is pinned and should remain pinned for the lifetime of the stats context */
	return pinned_umem;

release_umem:
	if (umem)
		bnxt_re_peer_mem_release(umem);
free_umem:
	kfree(pinned_umem);
	return ERR_PTR(ret);
}

/*
 * Per-type create handlers. Each acquires resources (e.g. quota), performs
 * the create, and on failure releases resources before returning.
 */
static int bnxt_re_dv_create_obj_stats(struct bnxt_re_dev *rdev,
				       struct bnxt_en_dev *en_dev,
				       struct uverbs_attr_bundle *attrs,
				       struct bnxt_re_dv_create_obj_init_attr *init_attr,
				       struct bnxt_re_dv_umem *umem_handle,
				       struct bnxt_re_dv_create_obj_attr *obj_attr,
				       struct bnxt_re_dv_create_obj_resp *resp)
{
	struct bnxt_re_dv_umem *pinned_umem;
	int ret;

	ret = bnxt_re_acquire_resource_quota(rdev, BNXT_ULP_RES_STAT_CTX);
	if (ret)
		return ret;

	pinned_umem = bnxt_re_dv_create_stats_ctx(rdev, en_dev, &init_attr->stats_attr,
						  umem_handle, init_attr->offset,
						  attrs, &resp->stats_resp);
	if (IS_ERR(pinned_umem)) {
		ret = PTR_ERR(pinned_umem);
		bnxt_re_release_resource_quota(rdev, BNXT_ULP_RES_STAT_CTX);
		return ret;
	}

	obj_attr->umem_handle = pinned_umem;
	obj_attr->resp_data.fw_stats_ctx_id = resp->stats_resp.stat_ctx_id;
	return 0;
}

static int bnxt_re_dv_create_obj_ring(struct bnxt_re_dev *rdev,
				      struct bnxt_en_dev *en_dev,
				      struct ib_ucontext *ib_uctx,
				      struct uverbs_attr_bundle *attrs,
				      struct bnxt_re_dv_create_obj_init_attr *init_attr,
				      struct bnxt_re_dv_umem *umem_handle,
				      struct bnxt_re_dv_create_obj_attr *obj_attr,
				      struct bnxt_re_dv_create_obj_resp *resp)
{
	struct bnxt_re_dv_create_obj_attr *stats_obj;
	struct bnxt_re_dv_create_obj_attr *rx_obj;
	struct bnxt_re_cq *com_cq;
	u8 obj_type = init_attr->obj_type;
	u16 stats_ctx_id = 0;
	u16 rx_ring_id = 0;
	int ret;

	if (obj_type == BNXT_RE_OBJ_TYPE_RX_AGGR) {
		rx_obj = uverbs_attr_get_obj(attrs, BNXT_RE_DV_CREATE_OBJ_RX_HANDLE);
		if (IS_ERR(rx_obj) || rx_obj->obj_type != BNXT_RE_OBJ_TYPE_RX) {
			dev_err(rdev_to_dev(rdev), "Invalid or wrong type for RX handle\n");
			return -EINVAL;
		}
		rx_ring_id = rx_obj->resp_data.fw_ring_id;
	}

	if (!uverbs_attr_is_valid(attrs, BNXT_RE_DV_CREATE_OBJ_CQ_HANDLE)) {
		dev_err(rdev_to_dev(rdev), "CQ handle required for ring object creation\n");
		return -EINVAL;
	}
	com_cq = uverbs_attr_get_obj(attrs, BNXT_RE_DV_CREATE_OBJ_CQ_HANDLE);
	if (IS_ERR(com_cq))
		return PTR_ERR(com_cq);

	stats_obj = uverbs_attr_get_obj(attrs, BNXT_RE_DV_CREATE_OBJ_STATS_HANDLE);
	if (!IS_ERR(stats_obj) && stats_obj->obj_type == BNXT_RE_OBJ_TYPE_STATS)
		stats_ctx_id = stats_obj->resp_data.fw_stats_ctx_id;

	if (obj_type == BNXT_RE_OBJ_TYPE_TX) {
		ret = bnxt_re_acquire_resource_quota(rdev, BNXT_ULP_RES_TX);
		if (ret)
			return ret;
	} else if (obj_type == BNXT_RE_OBJ_TYPE_RX ||
		   obj_type == BNXT_RE_OBJ_TYPE_RX_AGGR) {
		ret = bnxt_re_acquire_resource_quota(rdev, BNXT_ULP_RES_RX);
		if (ret)
			return ret;
	}

	ret = bnxt_re_dv_obj_create_ring(rdev, en_dev, obj_type, ib_uctx,
					 umem_handle, init_attr->offset,
					 &init_attr->ring_attr, com_cq, &resp->ring_resp,
					 obj_attr, stats_ctx_id, rx_ring_id);
	if (ret) {
		if (obj_type == BNXT_RE_OBJ_TYPE_TX)
			bnxt_re_release_resource_quota(rdev, BNXT_ULP_RES_TX);
		else if (obj_type == BNXT_RE_OBJ_TYPE_RX ||
			 obj_type == BNXT_RE_OBJ_TYPE_RX_AGGR)
			bnxt_re_release_resource_quota(rdev, BNXT_ULP_RES_RX);
		return ret;
	}
	return 0;
}

static int bnxt_re_dv_create_obj_vnic(struct bnxt_re_dev *rdev,
				      struct bnxt_re_dv_create_obj_init_attr *init_attr,
				      struct bnxt_re_dv_create_obj_attr *obj_attr,
				      struct bnxt_re_dv_create_obj_resp *resp)
{
	int ret;

	ret = bnxt_re_acquire_resource_quota(rdev, BNXT_ULP_RES_VNIC);
	if (ret)
		return ret;

	ret = bnxt_re_hwrm_alloc_vnic(rdev, &init_attr->vnic_attr.vnic_id,
				      init_attr->vnic_attr.virtio_net_fid,
				      init_attr->vnic_attr.flags);
	if (ret) {
		bnxt_re_release_resource_quota(rdev, BNXT_ULP_RES_VNIC);
		return ret;
	}

	resp->vnic_resp.vnic_id = init_attr->vnic_attr.vnic_id;
	resp->vnic_resp.comp_mask = 0;
	obj_attr->resp_data.fw_vnic_id = init_attr->vnic_attr.vnic_id;
	return 0;
}

static int bnxt_re_dv_create_obj_bfid(struct bnxt_re_dev *rdev,
				      struct bnxt_re_dv_create_obj_init_attr *init_attr,
				      struct bnxt_re_dv_create_obj_attr *obj_attr,
				      struct bnxt_re_dv_create_obj_resp *resp)
{
	int ret;

	ret = bnxt_re_hwrm_alloc_bfid(rdev, &init_attr->bfid_attr.bfid);
	if (ret)
		return ret;

	resp->bfid_resp.bfid = init_attr->bfid_attr.bfid;
	resp->bfid_resp.comp_mask = 0;
	obj_attr->resp_data.bfid = init_attr->bfid_attr.bfid;
	return 0;
}

static int bnxt_re_dv_create_obj_rss(struct bnxt_re_dev *rdev,
				     struct bnxt_re_dv_create_obj_attr *obj_attr,
				     struct bnxt_re_dv_create_obj_resp *resp)
{
	int ret;

	ret = bnxt_re_acquire_resource_quota(rdev, BNXT_ULP_RES_RSS_CTX);
	if (ret)
		return ret;

	ret = bnxt_re_hwrm_alloc_rss_ctx(rdev, &resp->rss_resp.rss_ctx_idx);
	if (ret) {
		bnxt_re_release_resource_quota(rdev, BNXT_ULP_RES_RSS_CTX);
		return ret;
	}

	obj_attr->resp_data.fw_rss_ctx_id = resp->rss_resp.rss_ctx_idx;
	return 0;
}

/* Main handler - routes to per-type create helpers based on obj_type */
static int UVERBS_HANDLER(BNXT_RE_METHOD_DV_CREATE_OBJ)(struct uverbs_attr_bundle *attrs)
{
	struct bnxt_re_dv_create_obj_init_attr init_attr = {};
	struct bnxt_re_dv_create_obj_resp resp = {};
	struct bnxt_re_dv_create_obj_attr *obj_attr;
	struct bnxt_re_dv_umem *umem_handle = NULL;
	struct bnxt_re_ucontext *re_uctx;
	struct ib_uobject *uobj;
	struct ib_ucontext *ib_uctx;
	struct bnxt_en_dev *en_dev;
	struct bnxt_re_dev *rdev;
	u8 obj_type;
	int ret;

	ib_uctx = ib_uverbs_get_ucontext(attrs);
	if (IS_ERR(ib_uctx))
		return PTR_ERR(ib_uctx);

	re_uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	rdev = re_uctx->rdev;
	en_dev = rdev->en_dev;

	uobj = uverbs_attr_get_uobject(attrs, BNXT_RE_CREATE_OBJ_HANDLE);
	if (IS_ERR(uobj)) {
		dev_err(rdev_to_dev(rdev), "%s: Failed to get uobject: %ld\n",
			__func__, PTR_ERR(uobj));
		return PTR_ERR(uobj);
	}

	ret = uverbs_copy_from_or_zero(&init_attr, attrs, BNXT_RE_CREATE_OBJ_ATTR_PTR);
	if (ret)
		return -EINVAL;

	obj_attr = kzalloc(sizeof(*obj_attr), GFP_KERNEL);
	if (!obj_attr)
		return -ENOMEM;

	obj_type = init_attr.obj_type;
	obj_attr->obj_type = init_attr.obj_type;
	obj_attr->pg_size = init_attr.pg_size;
	obj_attr->offset = init_attr.offset;
	resp.obj_type = init_attr.obj_type;

	if (uverbs_attr_is_valid(attrs, BNXT_RE_CREATE_OBJ_UMEM_HANDLE)) {
		umem_handle = uverbs_attr_get_obj(attrs, BNXT_RE_CREATE_OBJ_UMEM_HANDLE);
		if (IS_ERR(umem_handle)) {
			dev_err(rdev_to_dev(rdev), "%s: UMEM_HANDLE is not valid\n", __func__);
			ret = PTR_ERR(umem_handle);
			goto free_obj;
		}
	}

	switch (obj_type) {
	case BNXT_RE_OBJ_TYPE_STATS:
		ret = bnxt_re_dv_create_obj_stats(rdev, en_dev, attrs, &init_attr,
						  umem_handle, obj_attr, &resp);
		break;
	case BNXT_RE_OBJ_TYPE_TX:
	case BNXT_RE_OBJ_TYPE_RX:
	case BNXT_RE_OBJ_TYPE_MPC:
	case BNXT_RE_OBJ_TYPE_RX_AGGR:
		ret = bnxt_re_dv_create_obj_ring(rdev, en_dev, ib_uctx, attrs,
						 &init_attr, umem_handle, obj_attr, &resp);
		break;
	case BNXT_RE_OBJ_TYPE_VNIC:
		ret = bnxt_re_dv_create_obj_vnic(rdev, &init_attr, obj_attr, &resp);
		break;
	case BNXT_RE_OBJ_TYPE_BFID:
		ret = bnxt_re_dv_create_obj_bfid(rdev, &init_attr, obj_attr, &resp);
		break;
	case BNXT_RE_OBJ_TYPE_RSS:
		ret = bnxt_re_dv_create_obj_rss(rdev, obj_attr, &resp);
		break;
	default:
		dev_err(rdev_to_dev(rdev), "Invalid obj_type: %u\n", obj_type);
		ret = -EINVAL;
		break;
	}

	if (ret)
		goto free_obj;

	ret = bnxt_re_dv_uverbs_copy_to(rdev, attrs, BNXT_RE_CREATE_OBJ_RESP,
					&resp, sizeof(resp));
	if (ret) {
		dev_err(rdev_to_dev(rdev), "%s: Failed to copy response: %d\n",
			__func__, ret);
		goto free_obj;
	}

	bnxt_re_dv_finalize_uobj(uobj, obj_attr, attrs, BNXT_RE_CREATE_OBJ_HANDLE);
	return 0;

free_obj:
	kfree(obj_attr);
	return ret;
}

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_DV_SEND_FW_MSG,
			    UVERBS_ATTR_PTR_IN(BNXT_RE_SEND_FW_MSG_IN,
					       UVERBS_ATTR_MIN_SIZE(sizeof(struct input)),
					       UA_MANDATORY, UA_ALLOC_AND_COPY),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_SEND_FW_MSG_OUT,
						UVERBS_ATTR_MIN_SIZE(sizeof(struct output)),
						UA_MANDATORY));

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_DV_CREATE_OBJ,
			    UVERBS_ATTR_IDR(BNXT_RE_CREATE_OBJ_HANDLE,
					    BNXT_RE_OBJECT_DV_CREATE_DESTROY_OBJ,
					    UVERBS_ACCESS_NEW, UA_MANDATORY),
			    UVERBS_ATTR_PTR_OUT(BNXT_RE_CREATE_OBJ_RESP,
						UVERBS_ATTR_STRUCT(struct
								   bnxt_re_dv_create_obj_resp,
								   obj_type), UA_MANDATORY),
			    UVERBS_ATTR_PTR_IN(BNXT_RE_CREATE_OBJ_ATTR_PTR,
					       UVERBS_ATTR_STRUCT(struct
								  bnxt_re_dv_create_obj_init_attr,
								  comp_mask), UA_MANDATORY),
			    UVERBS_ATTR_IDR(BNXT_RE_DV_CREATE_OBJ_STATS_HANDLE,
					    BNXT_RE_OBJECT_DV_CREATE_DESTROY_OBJ,
					    UVERBS_ACCESS_READ,
					    UA_OPTIONAL),
			    UVERBS_ATTR_IDR(BNXT_RE_DV_CREATE_OBJ_RX_HANDLE,
					    BNXT_RE_OBJECT_DV_CREATE_DESTROY_OBJ,
					    UVERBS_ACCESS_READ, UA_OPTIONAL),
			    UVERBS_ATTR_IDR(BNXT_RE_DV_CREATE_OBJ_CQ_HANDLE,
					    UVERBS_IDR_ANY_OBJECT,
					    UVERBS_ACCESS_READ, UA_OPTIONAL),
			    UVERBS_ATTR_IDR(BNXT_RE_CREATE_OBJ_UMEM_HANDLE,
					    BNXT_RE_OBJECT_UMEM,
					    UVERBS_ACCESS_READ, UA_OPTIONAL));

DECLARE_UVERBS_NAMED_METHOD_DESTROY(BNXT_RE_METHOD_DV_DESTROY_OBJ,
				    UVERBS_ATTR_IDR(BNXT_RE_DV_DESTROY_OBJ_HANDLE,
						    BNXT_RE_OBJECT_DV_CREATE_DESTROY_OBJ,
						    UVERBS_ACCESS_DESTROY,
						    UA_MANDATORY));

DECLARE_UVERBS_NAMED_OBJECT(BNXT_RE_OBJECT_DV_SEND_FW_MSG,
			    UVERBS_TYPE_ALLOC_IDR(bnxt_re_dv_clean_fw_msg),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DV_SEND_FW_MSG));

DECLARE_UVERBS_NAMED_OBJECT(BNXT_RE_OBJECT_DV_CREATE_DESTROY_OBJ,
			    UVERBS_TYPE_ALLOC_IDR(bnxt_re_dv_destroy_obj),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DV_CREATE_OBJ),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DV_DESTROY_OBJ));

static int bnxt_re_dv_clean_pt_msg(struct ib_uobject *uobj,
				   enum rdma_remove_reason why,
				   struct uverbs_attr_bundle *attrs)
{
	dev_dbg(NULL,  "$ACK:%s:%d\n", __func__, __LINE__);
	return 0;
}

static int bnxt_re_dv_pt_msg(struct bnxt_re_dev *rdev, struct bnxt_re_dv_pt_msg_input *ptreq)
{
	void *req = NULL, *resp = NULL;
	int rc;

	switch (ptreq->pt_type) {
	case BNXT_RE_DV_PT_TYPE_TF:
		req = vzalloc(ptreq->req_len);
		if (!req) {
			rc = -ENOMEM;
			goto err;
		}

		resp = vzalloc(ptreq->resp_len);
		if (!resp) {
			rc = -ENOMEM;
			goto err;
		}

		if (copy_from_user(req, (void *)ptreq->req, ptreq->req_len)) {
			rc = -EFAULT;
			goto err;
		}

		rc = bnxt_tfc_oem_process(rdev->en_dev, req, resp, &ptreq->resp_len);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "DV TrueFlow PT command failed: %d\n", rc);
			goto err;
		}

		if (copy_to_user((void *)ptreq->resp, resp, ptreq->resp_len)) {
			rc = -EFAULT;
			goto err;
		}
		break;
	default:
		rc = -EFAULT;
		break;
	}

err:
	vfree(req);
	vfree(resp);
	return rc;
}

static int UVERBS_HANDLER(BNXT_RE_METHOD_DV_SEND_PT_MSG)(struct uverbs_attr_bundle *attrs)
{
	struct bnxt_re_dv_pt_msg_input *ptreq;
	struct bnxt_re_ucontext *uctx;
	struct ib_ucontext *ib_uctx;
	struct bnxt_re_dev *rdev;

	ib_uctx = ib_uverbs_get_ucontext(attrs);
	if (IS_ERR(ib_uctx))
		return PTR_ERR(ib_uctx);
	uctx = container_of(ib_uctx, struct bnxt_re_ucontext, ib_uctx);
	rdev = uctx->rdev;

	ptreq = uverbs_attr_get_alloced_ptr(attrs, BNXT_RE_SEND_PT_MSG_IN);

	return bnxt_re_dv_pt_msg(rdev, ptreq);
}

DECLARE_UVERBS_NAMED_METHOD(BNXT_RE_METHOD_DV_SEND_PT_MSG,
			    UVERBS_ATTR_PTR_IN(BNXT_RE_SEND_PT_MSG_IN,
			    UVERBS_ATTR_MIN_SIZE(sizeof(struct bnxt_re_dv_pt_msg_input)),
						 UA_MANDATORY, UA_ALLOC_AND_COPY));

DECLARE_UVERBS_NAMED_OBJECT(BNXT_RE_OBJECT_DV_SEND_PT_MSG,
			    UVERBS_TYPE_ALLOC_IDR(bnxt_re_dv_clean_pt_msg),
			    &UVERBS_METHOD(BNXT_RE_METHOD_DV_SEND_PT_MSG));

const struct uapi_definition bnxt_re_uapi_defs[] = {
	UAPI_DEF_CHAIN_OBJ_TREE_NAMED(BNXT_RE_OBJECT_ALLOC_PAGE),
	UAPI_DEF_CHAIN_OBJ_TREE_NAMED(BNXT_RE_OBJECT_NOTIFY_DRV),
	UAPI_DEF_CHAIN_OBJ_TREE_NAMED(BNXT_RE_OBJECT_GET_TOGGLE_MEM),
	UAPI_DEF_CHAIN_OBJ_TREE_NAMED(BNXT_RE_OBJECT_DBR),
	UAPI_DEF_CHAIN_OBJ_TREE_NAMED(BNXT_RE_OBJECT_UMEM),
	UAPI_DEF_CHAIN_OBJ_TREE_NAMED(BNXT_RE_OBJECT_DV_CQ),
	UAPI_DEF_CHAIN_OBJ_TREE_NAMED(BNXT_RE_OBJECT_DV_QP),
	UAPI_DEF_CHAIN_OBJ_TREE_NAMED(BNXT_RE_OBJECT_DV_SEND_FW_MSG),
	UAPI_DEF_CHAIN_OBJ_TREE_NAMED(BNXT_RE_OBJECT_DV_CREATE_DESTROY_OBJ),
	UAPI_DEF_CHAIN_OBJ_TREE_NAMED(BNXT_RE_OBJECT_DV_SEND_PT_MSG),
	{}
};
