/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2022-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "bnxt_re.h"
#include "bnxt.h"
#include "bnxt_hdbr.h"
#include "hdbr.h"

static void bnxt_re_hdbr_wait_hw_read_complete(struct bnxt_re_dev *rdev, int group)
{
	/*
	 * TODO: We need a deterministic signal/event/operation here to make sure
	 * HW doesn't read host memory of DB copy region. Then we could go ahead
	 * to free the page safely.
	 */
}

static void bnxt_re_hdbr_free_pg_task(struct work_struct *work)
{
	struct bnxt_re_hdbr_free_pg_work *wk =
			container_of(work, struct bnxt_re_hdbr_free_pg_work, work);

	bnxt_re_hdbr_wait_hw_read_complete(wk->rdev, wk->group);

	/*
	 * TODO: As a temporary solution of preventing HW access a freed page, we'll
	 * not free the page. Instead, we put it into reusable free page list.
	 * dma_free_coherent(&wk->rdev->en_dev->pdev->dev, wk->size, wk->pg->kptr,
	 * wk->pg->da);
	 * kfree(wk->pg);
	 */
	mutex_lock(&wk->rdev->hdbr_fpg_lock);
	list_add_tail(&wk->pg->pg_node, &wk->rdev->hdbr_fpgs);
	mutex_unlock(&wk->rdev->hdbr_fpg_lock);

	kfree(wk);
}

static struct hdbr_pg *hdbr_reuse_page(struct bnxt_re_dev *rdev)
{
	struct hdbr_pg *pg = NULL;

	mutex_lock(&rdev->hdbr_fpg_lock);
	if (!list_empty(&rdev->hdbr_fpgs)) {
		pg = list_first_entry(&rdev->hdbr_fpgs, struct hdbr_pg, pg_node);
		list_del(&pg->pg_node);
	}
	mutex_unlock(&rdev->hdbr_fpg_lock);

	return pg;
}

/*
 * This function allocates a 4K page as DB copy app page, and link it to the
 * main kernel table which is managed by L2 driver.
 *
 * Inside RoCE DB copy app page, DBs are grouped by group type.
 *     DBC_GROUP_SQ  : grp_size = 1,
 *		       offset 0: SQ producer index doorbell
 *     DBC_GROUP_RQ  : grp_size = 1,
 *		       offset 0: RQ producer index doorbell
 *     DBC_GROUP_SRQ : grp_size = 3,
 *		       offset 0: SRQ producer index doorbell
 *		       offset 1: SRQ_ARMENA (must before SRQ_ARM)
 *		       offset 2: SRQ_ARM
 *     DBC_GROUP_CQ  : grp_size = 4,
 *		       offset 0: CQ consumer index doorbell
 *		       offset 1: CQ_ARMENA (must before CQ_ARMALL/SE)
 *		       offset 2: CQ_ARMALL/CQ_ARMASE (share slot)
 *		       offset 3: CUTOFF_ACK
 */
static struct hdbr_pg *hdbr_alloc_page(struct bnxt_re_dev *rdev, int group, u16 pi)
{
	struct bnxt_hdbr_ktbl *ktbl;
	struct hdbr_pg *pg;
	int rc;

	ktbl = rdev->en_dev->hdbr_info->ktbl[group];
	if (!ktbl)
		return NULL;
	pg = hdbr_reuse_page(rdev);
	if (pg) {
		u64 *kptr = pg->kptr;
		dma_addr_t da = pg->da;

		memset(pg, 0, sizeof(*pg));
		memset(kptr, 0, SZ_4K);
		pg->kptr = kptr;
		pg->da = da;
	} else {
		pg = kzalloc(sizeof(*pg), GFP_KERNEL);
		if (!pg)
			return NULL;
		pg->kptr = dma_alloc_coherent(&rdev->en_dev->pdev->dev, SZ_4K, &pg->da,
					      GFP_KERNEL | __GFP_ZERO);
		if (!pg->kptr)
			goto alloc_err;
	}
	pg->grp_size = bnxt_re_hdbr_group_size(group);
	pg->blk_size = hdbr_get_block_size(pg->grp_size);
	pg->stride_n_size = hdbr_get_stride_size(pg->grp_size);
	pg->first_avail = 0;
	pg->first_empty = 0;
	pg->size = hdbr_get_entries_per_pg(pg->blk_size);
	pg->blk_avail = pg->size;
	/* Register this page to main kernel table in L2 driver */
	rc = bnxt_hdbr_reg_apg(ktbl, pg->da, &pg->ktbl_idx, pi, pg->stride_n_size);
	if (rc)
		goto reg_page_err;

	return pg;

reg_page_err:
	dma_free_coherent(&rdev->en_dev->pdev->dev, SZ_4K, pg->kptr, pg->da);

alloc_err:
	kfree(pg);

	return NULL;
}

static void hdbr_dealloc_page(struct bnxt_re_dev *rdev, struct hdbr_pg *pg, int group)
{
	struct bnxt_hdbr_ktbl *ktbl = rdev->en_dev->hdbr_info->ktbl[group];
	struct bnxt_re_hdbr_free_pg_work *wk;

	if (!ktbl) {
		dev_err(rdev_to_dev(rdev), "L2 driver has no support for unreg page!");
		return;
	}
	/* Unregister this page from main kernel table in L2 driver */
	if (!test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags))
		bnxt_hdbr_unreg_apg(ktbl, pg->ktbl_idx);

	/* Free page and structure memory in background */
	wk = kzalloc(sizeof(*wk), GFP_ATOMIC);
	if (!wk)
		return;

	wk->rdev = rdev;
	wk->pg = pg;
	wk->size = SZ_4K;
	wk->group = group;
	INIT_WORK(&wk->work, bnxt_re_hdbr_free_pg_task);
	queue_work(rdev->hdbr_wq, &wk->work);
}

/**
 * hdbr_claim_slot - Claim the hdbr page and insert mmap entry
 * into xarray.
 * @pg: page allocated using dma_alloc_coherent.
 * @cntx: user context of the driver
 * @mmap_key: offset of rdma entry xarray (multiple of page size).
 * @mmap_entry_save: Per object rdma_entry pointer used in mmap_free
 *
 * hdbr_pg should always be a valid pointer.
 * bnxt_re_ucontext is valid if there is user context.
 * For kernel context, rdma entry will not be populated.
 * mmap_key is valid if a reply for user context is requested.
 * For kernel context, it is expected to be NULL.
 *
 * Memory associated with hdbr_pg will be eventually mmap by user
 * (in case of valid user context), it is required to insert
 * rdma mmap xarray entry.
 *
 * Returns NULL, only if rdma mmap xarray is not inserted.
 * Upon successful addition of rdma mmap xarray, it will set
 * mmap_key based on old vs new API. In the case of new API it is
 * xarray index.
 *
 */
static __le64 *hdbr_claim_slot(struct hdbr_pg *pg,
			       struct bnxt_re_ucontext *cntx, __u64 *mmap_key,
			       struct rdma_user_mmap_entry **mmap_entry_save)
{
	bool is_entry = false;
	int i, n, idx;

	if (cntx) {
		is_entry = bnxt_re_mmap_entry_insert_compat(cntx, (u64)pg->kptr, pg->da,
							    BNXT_RE_MMAP_HDBR_BASE, mmap_key,
							    mmap_entry_save);
		if (!is_entry)
			return NULL;
	}

	n = pg->blk_size;
	idx = pg->first_avail * n;
	for (i = 0; i < pg->grp_size; i++)
		pg->kptr[idx + i] = cpu_to_le64(DBC_VALUE_INIT);
	pg->blk_avail--;

	/* Update indice for next */
	if (pg->first_avail == pg->first_empty) {
		pg->first_avail++;
		pg->first_empty++;
		if (pg->first_empty < pg->size)
			pg->kptr[pg->first_empty * n] = cpu_to_le64(DBC_VALUE_LAST);
	} else {
		while (++pg->first_avail < pg->first_empty) {
			if (!pg->kptr[pg->first_avail * n])
				break;
		}
	}
	return pg->kptr + idx;
}

static void hdbr_clear_slot(struct hdbr_pg *pg, int pos)
{
	int i, offset = pos * pg->blk_size;

	for (i = 0; i < pg->grp_size; i++)
		pg->kptr[offset + i] = 0;
	pg->blk_avail++;
	if (pos < pg->first_avail)
		pg->first_avail = pos;
}

static void bnxt_re_hdbr_db_unreg(struct bnxt_re_dev *rdev, int group,
				  struct bnxt_qplib_db_info *dbinfo)
{
	struct bnxt_re_hdbr_app *app;
	struct hdbr_pg_lst *plst;
	struct hdbr_pg *pg;
	bool found = false;
	int ktbl_idx;
	__le64 *dbc;

	if (group >= DBC_GROUP_MAX)
		return;
	app = dbinfo->app;
	ktbl_idx = dbinfo->ktbl_idx;
	dbc = dbinfo->dbc;
	if (!app || !dbc) {
		dev_err(rdev_to_dev(rdev),
			"Invalid unreg db params, app=0x%p, ktbl_idx=%d, dbc=0x%p\n",
			app, ktbl_idx, dbc);
		return;
	}

	plst = &app->pg_lst[group];
	mutex_lock(&plst->lst_lock);
	list_for_each_entry(pg, &plst->pg_head, pg_node) {
		if (pg->ktbl_idx == ktbl_idx) {
			int pos;

			pos = ((u64)dbc - (u64)pg->kptr) / HDBR_DB_SIZE / pg->blk_size;
			hdbr_clear_slot(pg, pos);
			plst->blk_avail++;
			found = true;
			break;
		}
	}

	/* Additionally, free the page if it is empty. */
	if (found && pg->blk_avail == pg->size) {
		plst->blk_avail -= pg->blk_avail;
		list_del(&pg->pg_node);
		hdbr_dealloc_page(rdev, pg, group);
	}

	mutex_unlock(&plst->lst_lock);

	dbinfo->app = NULL;
	dbinfo->ktbl_idx = 0;
	dbinfo->dbc = NULL;

	if (!found)
		dev_err(rdev_to_dev(rdev), "Fatal: DB copy not found\n");
}

void bnxt_re_hdbr_db_unreg_srq(struct bnxt_re_dev *rdev, struct bnxt_re_srq *srq)
{
	struct bnxt_qplib_db_info *dbinfo = &srq->qplib_srq.dbinfo;

	bnxt_re_hdbr_db_unreg(rdev, DBC_GROUP_SRQ, dbinfo);
	bnxt_re_mmap_entry_remove_compat(srq->srq_hdbr_mmap);
}

void bnxt_re_hdbr_db_unreg_qp(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp)
{
	struct bnxt_qplib_db_info *dbinfo;

	dbinfo = &qp->qplib_qp.sq.dbinfo;
	bnxt_re_hdbr_db_unreg(rdev, DBC_GROUP_SQ, dbinfo);
	bnxt_re_mmap_entry_remove_compat(qp->sq_hdbr_mmap);
	if (qp->qplib_qp.srq)
		return;
	dbinfo = &qp->qplib_qp.rq.dbinfo;
	bnxt_re_hdbr_db_unreg(rdev, DBC_GROUP_RQ, dbinfo);
	bnxt_re_mmap_entry_remove_compat(qp->rq_hdbr_mmap);
}

void bnxt_re_hdbr_db_unreg_cq(struct bnxt_re_dev *rdev, struct bnxt_re_cq *cq)
{
	struct bnxt_qplib_db_info *dbinfo = &cq->qplib_cq.dbinfo;

	bnxt_re_hdbr_db_unreg(rdev, DBC_GROUP_CQ, dbinfo);
	bnxt_re_mmap_entry_remove_compat(cq->cq_hdbr_mmap);
}

static __le64 *bnxt_re_hdbr_db_reg(struct bnxt_re_dev *rdev, struct bnxt_re_hdbr_app *app,
				   int group, u16 pi, struct bnxt_qplib_db_info *dbinfo,
				   struct bnxt_re_ucontext *cntx, __u64 *mmap_key,
				   struct rdma_user_mmap_entry **mmap_entry_save)
{
	struct hdbr_pg_lst *plst;
	struct hdbr_pg *pg;
	__le64 *dbc = NULL;

	if (group >= DBC_GROUP_MAX)
		return NULL;

	plst = &app->pg_lst[group];
	mutex_lock(&plst->lst_lock);
	if (plst->blk_avail == 0) {
		pg = hdbr_alloc_page(rdev, group, pi);
		if (!pg)
			goto exit;
		list_add(&pg->pg_node, &plst->pg_head);
		plst->blk_avail += pg->blk_avail;
	}
	list_for_each_entry(pg, &plst->pg_head, pg_node) {
		if (pg->blk_avail > 0) {
			dbc = hdbr_claim_slot(pg, cntx, mmap_key, mmap_entry_save);
			if (!dbc)
				goto exit;
			dbinfo->ktbl_idx = pg->ktbl_idx;
			plst->blk_avail--;
			break;
		}
	}

exit:
	mutex_unlock(&plst->lst_lock);
	return dbc;
}

int bnxt_re_hdbr_db_reg_srq(struct bnxt_re_dev *rdev, struct bnxt_re_srq *srq,
			    struct bnxt_re_ucontext *cntx, struct bnxt_re_srq_resp *resp)
{
	struct bnxt_qplib_db_info *dbinfo;
	struct bnxt_re_hdbr_app *app;
	__u64 *mmap_key = NULL;
	u16 pi = 0;

	dbinfo = &srq->qplib_srq.dbinfo;
	if (cntx) {
		app = cntx->hdbr_app;
		pi = (u16)cntx->dpi.dpi;
	} else {
		app = container_of(rdev->hdbr_privileged, struct bnxt_re_hdbr_app, lst);
	}
	if (resp)
		mmap_key = &resp->hdbr_srq_mmap_key;

	dbinfo->dbc = bnxt_re_hdbr_db_reg(rdev, app, DBC_GROUP_SRQ,
					  pi, dbinfo, cntx,
					  mmap_key, &srq->srq_hdbr_mmap);
	if (!dbinfo->dbc)
		return -ENOMEM;
	dbinfo->app = app;
	dbinfo->dbc_dt = 0;
	return 0;
}

int bnxt_re_hdbr_db_reg_qp(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp,
			   struct bnxt_re_pd *pd, struct bnxt_re_qp_resp *resp)
{
	struct bnxt_re_ucontext *cntx = NULL;
	struct ib_ucontext *context = NULL;
	struct bnxt_qplib_db_info *dbinfo;
	struct bnxt_re_hdbr_app *app;
	__u64 *mmap_key = NULL;
	u16 pi = 0;

	if (pd) {
		context = pd->ib_pd.uobject->context;
		cntx = to_bnxt_re(context, struct bnxt_re_ucontext, ib_uctx);
	}
	if (cntx) {
		app = cntx->hdbr_app;
		pi = (u16)cntx->dpi.dpi;
	} else {
		app = container_of(rdev->hdbr_privileged, struct bnxt_re_hdbr_app, lst);
	}

	/* sq */
	if (resp)
		mmap_key = &resp->hdbr_sq_mmap_key;
	dbinfo = &qp->qplib_qp.sq.dbinfo;
	dbinfo->dbc = bnxt_re_hdbr_db_reg(rdev, app, DBC_GROUP_SQ,
					  pi, dbinfo, cntx,
					  mmap_key, &qp->sq_hdbr_mmap);
	if (!dbinfo->dbc)
		return -ENOMEM;
	dbinfo->app = app;
	if (*rdev->hdbr_dt)
		dbinfo->dbc_dt = 1;
	else
		dbinfo->dbc_dt = 0;
	if (resp) {
		/* TBD - Do we need rdma entry for this ? */
		resp->hdbr_dt = (__u32)dbinfo->dbc_dt;
	}

	if (qp->qplib_qp.srq)
		return 0;

	/* rq */
	if (resp)
		mmap_key = &resp->hdbr_rq_mmap_key;
	dbinfo = &qp->qplib_qp.rq.dbinfo;
	dbinfo->dbc = bnxt_re_hdbr_db_reg(rdev, app, DBC_GROUP_RQ,
					  pi, dbinfo, cntx,
					  mmap_key, &qp->rq_hdbr_mmap);
	if (!dbinfo->dbc) {
		bnxt_re_hdbr_db_unreg_qp(rdev, qp);
		return -ENOMEM;
	}
	dbinfo->app = app;
	dbinfo->dbc_dt = 0;

	return 0;
}

int bnxt_re_hdbr_db_reg_cq(struct bnxt_re_dev *rdev, struct bnxt_re_cq *cq,
			   struct bnxt_re_ucontext *cntx, struct bnxt_re_cq_resp *resp,
			   struct bnxt_re_cq_req *ureq)
{
	struct bnxt_qplib_db_info *dbinfo;
	struct bnxt_re_hdbr_app *app;
	__u64 *mmap_key = NULL;
	u16 pi = 0;

	dbinfo = &cq->qplib_cq.dbinfo;
	if (cntx) {
		app = cntx->hdbr_app;
		pi = (u16)cntx->dpi.dpi;
	} else {
		app = container_of(rdev->hdbr_privileged, struct bnxt_re_hdbr_app, lst);
	}

	if (resp && ureq && ureq->comp_mask & BNXT_RE_COMP_MASK_CQ_REQ_HAS_HDBR_KADDR)
		mmap_key = &resp->hdbr_cq_mmap_key;
	dbinfo->dbc = bnxt_re_hdbr_db_reg(rdev, app, DBC_GROUP_CQ,
					  pi, dbinfo, cntx,
					  mmap_key, &cq->cq_hdbr_mmap);
	if (!dbinfo->dbc)
		return -ENOMEM;
	dbinfo->app = app;
	dbinfo->dbc_dt = 0;
	if (mmap_key)
		resp->comp_mask |= BNXT_RE_CQ_HDBR_KADDR_SUPPORT;

	return 0;
}

struct bnxt_re_hdbr_app *bnxt_re_hdbr_alloc_app(struct bnxt_re_dev *rdev, bool user)
{
	struct bnxt_re_hdbr_app *app;
	int group;

	app = kzalloc(sizeof(*app), GFP_KERNEL);
	if (!app)
		return NULL;

	INIT_LIST_HEAD(&app->lst);
	for (group = DBC_GROUP_SQ; group < DBC_GROUP_MAX; group++) {
		app->pg_lst[group].group = group;
		INIT_LIST_HEAD(&app->pg_lst[group].pg_head);
		app->pg_lst[group].blk_avail = 0;
		mutex_init(&app->pg_lst[group].lst_lock);
	}

	if (user) {
		mutex_lock(&rdev->hdbr_lock);
		list_add(&app->lst, &rdev->hdbr_apps);
		mutex_unlock(&rdev->hdbr_lock);
	}

	return app;
}

void bnxt_re_hdbr_dealloc_app(struct bnxt_re_dev *rdev, struct bnxt_re_hdbr_app *app)
{
	struct list_head *head;
	struct hdbr_pg *pg;
	int group;

	for (group = DBC_GROUP_SQ; group < DBC_GROUP_MAX; group++) {
		head = &app->pg_lst[group].pg_head;
		while (!list_empty(head)) {
			pg = list_first_entry(head, struct hdbr_pg, pg_node);
			list_del(&pg->pg_node);
			hdbr_dealloc_page(rdev, pg, group);
		}
	}

	kfree(app);
}

int bnxt_re_hdbr_init(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_hdbr_app *drv;

	/* HDBR init for normal apps */
	INIT_LIST_HEAD(&rdev->hdbr_apps);

	if (rdev->en_dev->hdbr_info->hdbr_enabled) {
		rdev->hdbr_enabled = true;
		rdev->chip_ctx->modes.hdbr_enabled = true;
	} else {
		rdev->hdbr_enabled = false;
		return 0;
	}

	/* Init free page list */
	mutex_init(&rdev->hdbr_fpg_lock);
	INIT_LIST_HEAD(&rdev->hdbr_fpgs);

	rdev->hdbr_wq = create_singlethread_workqueue("bnxt_re_hdbr_wq");
	if (!rdev->hdbr_wq)
		return -ENOMEM;

	mutex_init(&rdev->hdbr_lock);
	rdev->hdbr_dt = &rdev->en_dev->hdbr_info->debug_trace;

	/* HDBR init for driver app */
	drv = bnxt_re_hdbr_alloc_app(rdev, false);
	if (!drv) {
		destroy_workqueue(rdev->hdbr_wq);
		rdev->hdbr_wq = NULL;
		return -ENOMEM;
	}
	rdev->hdbr_privileged = &drv->lst;

	return 0;
}

void bnxt_re_hdbr_uninit(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_hdbr_app *app;
	struct list_head *head;
	struct hdbr_pg *pg;

	if (!rdev->hdbr_enabled)
		return;

	/* Uninitialize normal apps */
	mutex_lock(&rdev->hdbr_lock);
	head = &rdev->hdbr_apps;
	while (!list_empty(head)) {
		app = list_first_entry(head, struct bnxt_re_hdbr_app, lst);
		list_del(&app->lst);
		bnxt_re_hdbr_dealloc_app(rdev, app);
	}
	mutex_unlock(&rdev->hdbr_lock);

	/* Uninitialize driver app */
	if (rdev->hdbr_privileged) {
		app = container_of(rdev->hdbr_privileged, struct bnxt_re_hdbr_app, lst);
		bnxt_re_hdbr_dealloc_app(rdev, app);
		rdev->hdbr_privileged = NULL;
	}

	if (rdev->hdbr_wq) {
		flush_workqueue(rdev->hdbr_wq);
		destroy_workqueue(rdev->hdbr_wq);
		rdev->hdbr_wq = NULL;
	}

	/*
	 * At this point, all app pages are flushed into free page list.
	 * Dealloc all free pages.
	 */
	mutex_lock(&rdev->hdbr_fpg_lock);
	head = &rdev->hdbr_fpgs;
	while (!list_empty(head)) {
		pg = list_first_entry(head, struct hdbr_pg, pg_node);
		list_del(&pg->pg_node);
		dma_free_coherent(&rdev->en_dev->pdev->dev, SZ_4K, pg->kptr, pg->da);
		kfree(pg);
	}
	mutex_unlock(&rdev->hdbr_fpg_lock);
}

static void bnxt_re_hdbr_pages_dump(struct hdbr_pg_lst *plst)
{
	struct hdbr_pg *pg;
	__le64 *dbc_ptr;
	int stride, ss;
	u64  dbc_val;
	int i, cnt = 0;

	mutex_lock(&plst->lst_lock);
	list_for_each_entry(pg, &plst->pg_head, pg_node) {
		stride = (int)((pg->stride_n_size & DBC_DRK64_STRIDE_MASK) >>
			       DBC_DRK64_STRIDE_SFT);
		ss = (int)((pg->stride_n_size & DBC_DRK64_SIZE_MASK) >>
			   DBC_DRK64_SIZE_SFT);
		ss = ss ? ss : 4;
		pr_info("page cnt    = %d\n", cnt);
		pr_info("kptr        = 0x%016llX\n", (u64)pg->kptr);
		pr_info("dma         = 0x%016llX\n", pg->da);
		pr_info("grp_size    = %d\n", pg->grp_size);
		pr_info("blk_size    = %d DBs\n", pg->blk_size);
		pr_info("stride      = %d\n", stride);
		pr_info("stride size = %d\n", ss);
		pr_info("first_avail = %d\n", pg->first_avail);
		pr_info("first_empty = %d\n", pg->first_empty);
		pr_info("blk_avail   = %d\n", pg->blk_avail);
		pr_info("ktbl_idx    = %d\n", pg->ktbl_idx);
		dbc_ptr = pg->kptr;
		if (!dbc_ptr) {
			pr_info("Page content not available\n");
			break;
		}
		for (i = 0; i < 512; i++) {
			if (i > 0 && i < 511 && !dbc_ptr[i])
				continue;
			dbc_val = le64_to_cpu(dbc_ptr[i]);
			pr_info("page[%d][%3d] 0x%016llX : type=%llx "
				"debug_trace=%d valid=%d path=%llx xID=0x%05llx "
				"toggle=%llx epoch=%d index=0x%06llx\n",
				cnt, i, dbc_val,
				(dbc_val & DBC_DBC64_TYPE_MASK) >> DBC_DBC64_TYPE_SFT,
				(dbc_val & DBC_DBC64_DEBUG_TRACE) ? 1 : 0,
				(dbc_val & DBC_DBC64_VALID) ? 1 : 0,
				(dbc_val & DBC_DBC64_PATH_MASK) >> DBC_DBC64_PATH_SFT,
				(dbc_val & DBC_DBC64_XID_MASK) >> DBC_DBC64_XID_SFT,
				(dbc_val & DBC_DBC64_TOGGLE_MASK) >> DBC_DBC64_TOGGLE_SFT,
				(dbc_val & DBC_DBC64_EPOCH) ? 1 : 0,
				(dbc_val & DBC_DBC64_INDEX_MASK));
		}
		cnt++;
	}
	mutex_unlock(&plst->lst_lock);
}

/* bnxt_re_hdbr_kaddr_to_dma -	get dma address for given kernel virtual address
 * @app:         hdbr app instance
 * @kaddr:       kernel virtual address
 *
 * This function will search the page list which was allocated for the given
 * hdbr app instance.
 * hdbr_app instance must be a valid instance.
 * If any page instance in any of the group is matching kaddr,
 * return associated dma_addr_t.
 *
 * Returns: dma address if found or ZERO if not found
 *
 */
dma_addr_t bnxt_re_hdbr_kaddr_to_dma(struct bnxt_re_hdbr_app *app, u64 kaddr)
{
	dma_addr_t dma_handle = 0;
	struct hdbr_pg_lst *plst;
	struct hdbr_pg *pg;
	int i;

	for (i = 0; i < DBC_GROUP_MAX; i++) {
		plst = &app->pg_lst[i];

		mutex_lock(&plst->lst_lock);
		list_for_each_entry(pg, &plst->pg_head, pg_node) {
			if (kaddr == (u64)pg->kptr) {
				pr_debug("kptr        = 0x%016llX\n", (u64)pg->kptr);
				pr_debug("dma         = 0x%016llX\n", pg->da);
				dma_handle = pg->da;
				break;
			}
		}
		mutex_unlock(&plst->lst_lock);

		if (dma_handle)
			goto exit;
	}

exit:
	return dma_handle;
}

static char *bnxt_re_hdbr_user_dump(struct bnxt_re_dev *rdev, int group)
{
	struct list_head *head = &rdev->hdbr_apps;
	struct bnxt_re_hdbr_app *app;
	int cnt = 0;
	char *buf;

	mutex_lock(&rdev->hdbr_lock);
	list_for_each_entry(app, head, lst)
		cnt++;
	buf = kasprintf(GFP_KERNEL, "Total apps    = %d\n", cnt);

	/* Page content dump to dmesg console */
	pr_info("====== Dumping %s user apps DB copy page info ======\n%s", rdev->dev_name, buf);
	cnt = 0;
	list_for_each_entry(app, head, lst) {
		struct hdbr_pg_lst *plst;

		plst = &app->pg_lst[group];
		pr_info("App cnt       = %d\n", cnt);
		pr_info("group         = %d\n", plst->group);
		pr_info("blk_avail     = %d\n",	plst->blk_avail);
		bnxt_re_hdbr_pages_dump(plst);
		cnt++;
	}
	mutex_unlock(&rdev->hdbr_lock);

	return buf;
}

static char *bnxt_re_hdbr_driver_dump(char *dev_name, struct list_head *head, int group)
{
	struct bnxt_re_hdbr_app *app;
	struct hdbr_pg_lst *plst;
	char *buf;

	app = container_of(head, struct bnxt_re_hdbr_app, lst);
	plst = &app->pg_lst[group];

	/* Structure data to debugfs console */
	buf = kasprintf(GFP_KERNEL,
			"group         = %d\n"
			"blk_avail     = %d\n",
			plst->group,
			plst->blk_avail);

	/* Page content dump to dmesg console */
	pr_info("====== Dumping %s driver DB copy page info ======\n%s", dev_name, buf);
	bnxt_re_hdbr_pages_dump(plst);

	return buf;
}

char *bnxt_re_hdbr_dump(struct bnxt_re_dev *rdev, int group, bool user)
{
	struct list_head *lst;

	if (user) {
		lst = &rdev->hdbr_apps;
		if (list_empty(lst))
			goto no_data;
		return bnxt_re_hdbr_user_dump(rdev, group);
	}

	lst = rdev->hdbr_privileged;
	if (!lst)
		goto no_data;
	return bnxt_re_hdbr_driver_dump(rdev->dev_name, lst, group);

no_data:
	return kasprintf(GFP_KERNEL, "No data available!\n");
}
