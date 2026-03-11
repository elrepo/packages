/*
 * Copyright (c) 2015-2024, Broadcom. All rights reserved.  The term
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
 * Description: Fast Path Operators
 */

#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/if_ether.h>
#include <rdma/ib_mad.h>

#include "roce_hsi.h"

#include "qplib_tlv.h"
#include "qplib_res.h"
#include "qplib_rcfw.h"
#include "qplib_sp.h"
#include "qplib_fp.h"
#include "compat.h"
#include "bnxt_re.h"

static void __clean_cq(struct bnxt_qplib_cq *cq, u64 qp);

/* Flush list */

static void bnxt_re_legacy_cancel_phantom_processing(struct bnxt_qplib_qp *qp)
{
	qp->sq.condition = false;
	qp->sq.legacy_send_phantom = false;
	qp->sq.single = false;
}

static void __bnxt_qplib_add_flush_qp(struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_cq *scq, *rcq;

	scq = qp->scq;
	rcq = qp->rcq;

	if (!qp->sq.flushed) {
		dev_dbg(&scq->hwq.pdev->dev,
			"QPLIB: FP: Adding to SQ Flush list = %p",
			qp);
		bnxt_re_legacy_cancel_phantom_processing(qp);
		list_add_tail(&qp->sq_flush, &scq->sqf_head);
		qp->sq.flushed = true;
	}
	if (!qp->srq) {
		if (!qp->rq.flushed) {
			dev_dbg(&rcq->hwq.pdev->dev,
				"QPLIB: FP: Adding to RQ Flush list = %p",
				qp);
			list_add_tail(&qp->rq_flush, &rcq->rqf_head);
			qp->rq.flushed = true;
		}
	}
}

static void bnxt_qplib_acquire_cq_flush_locks(struct bnxt_qplib_qp *qp)
	__acquires(&qp->scq->flush_lock) __acquires(&qp->rcq->flush_lock)
{
	/* Interrupts are already disabled in calling functions */
	spin_lock(&qp->scq->flush_lock);
	if (qp->scq == qp->rcq)
		__acquire(&qp->rcq->flush_lock);
	else
		spin_lock(&qp->rcq->flush_lock);
}

static void bnxt_qplib_release_cq_flush_locks(struct bnxt_qplib_qp *qp)
	__releases(&qp->scq->flush_lock) __releases(&qp->rcq->flush_lock)
{
	if (qp->scq == qp->rcq)
		__release(&qp->rcq->flush_lock);
	else
		spin_unlock(&qp->rcq->flush_lock);
	spin_unlock(&qp->scq->flush_lock);
}

void bnxt_qplib_add_flush_qp(struct bnxt_qplib_qp *qp)
{

	bnxt_qplib_acquire_cq_flush_locks(qp);
	__bnxt_qplib_add_flush_qp(qp);
	bnxt_qplib_release_cq_flush_locks(qp);
}

static void __bnxt_qplib_del_flush_qp(struct bnxt_qplib_qp *qp)
{
	if (qp->sq.flushed) {
		qp->sq.flushed = false;
		list_del(&qp->sq_flush);
	}
	if (!qp->srq) {
		if (qp->rq.flushed) {
			qp->rq.flushed = false;
			list_del(&qp->rq_flush);
		}
	}
}

void bnxt_qplib_clean_qp(struct bnxt_qplib_qp *qp)
{

	bnxt_qplib_acquire_cq_flush_locks(qp);
	__clean_cq(qp->scq, (u64)(unsigned long)qp);
	qp->sq.hwq.prod = 0;
	qp->sq.hwq.cons = 0;
	qp->sq.swq_start = 0;
	qp->sq.swq_last = 0;
	__clean_cq(qp->rcq, (u64)(unsigned long)qp);
	qp->rq.hwq.prod = 0;
	qp->rq.hwq.cons = 0;
	qp->rq.swq_start = 0;
	qp->rq.swq_last = 0;

	__bnxt_qplib_del_flush_qp(qp);
	bnxt_qplib_release_cq_flush_locks(qp);
}

static void bnxt_qpn_cqn_sched_task(struct work_struct *work)
{
	struct bnxt_qplib_nq_work *nq_work =
			container_of(work, struct bnxt_qplib_nq_work, work);

	struct bnxt_qplib_cq *cq = nq_work->cq;
	struct bnxt_qplib_nq *nq = nq_work->nq;

	if (cq && nq) {
		spin_lock_bh(&cq->compl_lock);
		if (nq->cqn_handler) {
			dev_dbg(&nq->res->pdev->dev,
				"%s:Trigger cq  = %p event nq = %p\n",
				__func__, cq, nq);
			nq->cqn_handler(nq, cq);
		}
		spin_unlock_bh(&cq->compl_lock);
	}
	kfree(nq_work);
}

/* NQ */
static int bnxt_qplib_process_dbqn(struct bnxt_qplib_nq *nq,
				   struct nq_dbq_event *nqe)
{
	u32 db_xid, db_type, db_pfid, db_dpi;

	if ((nqe->event) !=
	    NQ_DBQ_EVENT_EVENT_DBQ_THRESHOLD_EVENT) {
		dev_warn(&nq->res->pdev->dev,
			 "QPLIB: DBQ event 0x%x not handled", nqe->event);
		return -EINVAL;
	}
	db_type = le32_to_cpu(nqe->db_type_db_xid) & NQ_DBQ_EVENT_DB_TYPE_MASK
			      >> NQ_DBQ_EVENT_DB_TYPE_SFT;
	db_xid = le32_to_cpu(nqe->db_type_db_xid) & NQ_DBQ_EVENT_DB_XID_MASK
			     >> NQ_DBQ_EVENT_DB_XID_SFT;
	db_pfid = le16_to_cpu(nqe->db_pfid) & NQ_DBQ_EVENT_DB_DPI_MASK
			      >> NQ_DBQ_EVENT_DB_DPI_SFT;
	db_dpi = le32_to_cpu(nqe->db_dpi) & NQ_DBQ_EVENT_DB_DPI_MASK
			     >> NQ_DBQ_EVENT_DB_DPI_SFT;

	dev_dbg(&nq->res->pdev->dev,
		"QPLIB: DBQ notification xid 0x%x type 0x%x pfid 0x%x dpi 0x%x",
		db_xid, db_type, db_pfid, db_dpi);
	bnxt_re_schedule_dbq_event(nq->res);
	return 0;
}

static void bnxt_qplib_put_hdr_buf(struct pci_dev *pdev,
				   struct bnxt_qplib_hdrbuf *buf)
{
	dma_free_coherent(&pdev->dev, buf->len, buf->va, buf->dma_map);
	kfree(buf);
}

static void *bnxt_qplib_get_hdr_buf(struct pci_dev *pdev,  u32 step, u32 cnt)
{
	struct bnxt_qplib_hdrbuf *hdrbuf;
	u32 len;

	hdrbuf = kmalloc(sizeof(*hdrbuf), GFP_KERNEL);
	if (!hdrbuf)
		return NULL;

	len = ALIGN((step * cnt), PAGE_SIZE);
	hdrbuf->va = dma_alloc_coherent(&pdev->dev, len,
					&hdrbuf->dma_map, GFP_KERNEL);
	if (!hdrbuf->va)
		goto out;

	hdrbuf->len = len;
	hdrbuf->step = step;
	return hdrbuf;
out:
	kfree(hdrbuf);
	return NULL;
}

void bnxt_qplib_free_hdr_buf(struct bnxt_qplib_res *res,
			     struct bnxt_qplib_qp *qp)
{
	if (qp->rq_hdr_buf) {
		bnxt_qplib_put_hdr_buf(res->pdev, qp->rq_hdr_buf);
		qp->rq_hdr_buf = NULL;
	}

	if (qp->sq_hdr_buf) {
		bnxt_qplib_put_hdr_buf(res->pdev, qp->sq_hdr_buf);
		qp->sq_hdr_buf = NULL;
	}
}

int bnxt_qplib_alloc_hdr_buf(struct bnxt_qplib_res *res,
			     struct bnxt_qplib_qp *qp, u32 sstep, u32 rstep)
{
	struct pci_dev *pdev;

	pdev = res->pdev;
	if (sstep) {
		qp->sq_hdr_buf = bnxt_qplib_get_hdr_buf(pdev, sstep,
							qp->sq.max_wqe);
		if (!qp->sq_hdr_buf) {
			dev_err(&pdev->dev, "QPLIB: Failed to get sq_hdr_buf");
			return -ENOMEM;
		}
	}

	if (rstep) {
		qp->rq_hdr_buf = bnxt_qplib_get_hdr_buf(pdev, rstep,
							qp->rq.max_wqe);
		if (!qp->rq_hdr_buf) {
			dev_err(&pdev->dev, "QPLIB: Failed to get rq_hdr_buf");
			goto err_put_sq_hdr_buf;
		}
	}

	return 0;
err_put_sq_hdr_buf:
	bnxt_qplib_put_hdr_buf(res->pdev, qp->sq_hdr_buf);
	qp->sq_hdr_buf = NULL;
	return -ENOMEM;
}

/**
 * clean_nq -	Invalidate cqe from given nq.
 * @cq      -	Completion queue
 *
 * Traverse whole notification queue and invalidate any completion
 * associated cq handler provided by caller.
 * Note - This function traverse the hardware queue but do not update
 * consumer index. Invalidated cqe(marked from this function) will be
 * ignored from actual completion of notification queue.
 */
static void clean_nq(struct bnxt_qplib_cq *cq)
{
	struct bnxt_qplib_hwq *nq_hwq = NULL;
	struct bnxt_qplib_nq *nq = NULL;
	struct nq_base *hw_nqe = NULL;
	struct nq_cn *nqcne = NULL;
	u32 peek_flags, peek_cons;
	u64 q_handle;
	u32 type;
	int i;

	nq = cq->nq;
	nq_hwq = &nq->hwq;

	spin_lock_bh(&nq_hwq->lock);
	peek_flags = nq->nq_db.dbinfo.flags;
	peek_cons = nq_hwq->cons;
	for (i = 0; i < nq_hwq->max_elements; i++) {
		hw_nqe = bnxt_qplib_get_qe(nq_hwq, peek_cons, NULL);
		if (!NQE_CMP_VALID(hw_nqe, peek_flags))
			break;

		/* The valid test of the entry must be done first
		 * before reading any further.
		 */
		dma_rmb();
		type = le16_to_cpu(hw_nqe->info10_type) &
				   NQ_BASE_TYPE_MASK;

		/* Processing only NQ_BASE_TYPE_CQ_NOTIFICATION */
		if (type == NQ_BASE_TYPE_CQ_NOTIFICATION) {
			nqcne = (struct nq_cn *)hw_nqe;

			q_handle = le32_to_cpu(nqcne->cq_handle_low);
			q_handle |= (u64)le32_to_cpu(nqcne->cq_handle_high) << 32;
			if (q_handle == (u64)cq) {
				nqcne->cq_handle_low = 0;
				nqcne->cq_handle_high = 0;
				cq->cnq_events++;
			}
		}
		bnxt_qplib_hwq_incr_cons(nq_hwq->max_elements, &peek_cons,
					 1, &peek_flags);
	}
	spin_unlock_bh(&nq_hwq->lock);
}

/* Wait for receiving all NQEs for this CQ.
 * clean_nq is tried 100 times, each time clean_cq
 * loops up to budget times. budget is based on the
 * number of CQs shared by that NQ. So any NQE from
 * CQ would be already in the NQ.
 */
static void __wait_for_all_nqes(struct bnxt_qplib_cq *cq, u16 cnq_events)
{
	u32 retry_cnt = 100;
	u16 total_events;

	if (!cnq_events) {
		clean_nq(cq);
		return;
	}
	while (retry_cnt--) {
		total_events = cq->cnq_events;

		/* Increment total_events by 1 if any CREQ event received with CQ notification */
		if (cq->is_cq_err_event)
			total_events++;

		if (cnq_events == total_events) {
			dev_dbg(&cq->nq->res->pdev->dev,
				"QPLIB: NQ cleanup - Received all NQ events");
			return;
		}
		msleep(1);
		clean_nq(cq);
	}
}

static void bnxt_qplib_service_nq(
#ifdef HAS_TASKLET_SETUP
		struct tasklet_struct *t
#else
		unsigned long data
#endif
		)
{
#ifdef HAS_TASKLET_SETUP
	struct bnxt_qplib_nq *nq = from_tasklet(nq, t, nq_tasklet);
#else
	struct bnxt_qplib_nq *nq = (struct bnxt_qplib_nq *)data;
#endif
	struct bnxt_qplib_hwq *nq_hwq = &nq->hwq;
	int rc, budget = nq->budget;
	struct bnxt_qplib_res *res;
	struct bnxt_qplib_cq *cq;
	struct pci_dev *pdev;
	struct nq_base *nqe;
	u32 hw_polled = 0;
	u64 q_handle;
	u32 type;

	res = nq->res;
	pdev = res->pdev;

	spin_lock_bh(&nq_hwq->lock);
	/* Service the NQ until empty or budget expired */
	while (budget--) {
		nqe = bnxt_qplib_get_qe(nq_hwq, nq_hwq->cons, NULL);
		if (!NQE_CMP_VALID(nqe, nq->nq_db.dbinfo.flags))
			break;
		/* The valid test of the entry must be done first before
		 * reading any further.
		 */
		dma_rmb();
		type = le16_to_cpu(nqe->info10_type) & NQ_BASE_TYPE_MASK;
		switch (type) {
		case NQ_BASE_TYPE_CQ_NOTIFICATION:
		{
			struct nq_cn *nqcne = (struct nq_cn *)nqe;
			struct bnxt_re_cq *cq_p;

			q_handle = le32_to_cpu(nqcne->cq_handle_low);
			q_handle |= (u64)le32_to_cpu(nqcne->cq_handle_high) << 32;
			cq = (struct bnxt_qplib_cq *)q_handle;
			if (!cq)
				break;
			cq->toggle = (le16_to_cpu(nqe->info10_type) & NQ_CN_TOGGLE_MASK) >> NQ_CN_TOGGLE_SFT;
			cq->dbinfo.toggle = cq->toggle;
			cq_p = container_of(cq, struct bnxt_re_cq, qplib_cq);
			if (cq_p->cq_toggle_page)
				*((u32 *)cq_p->cq_toggle_page) = cq->toggle;
			bnxt_qplib_armen_db(&cq->dbinfo,
					    DBC_DBC_TYPE_CQ_ARMENA);
			spin_lock_bh(&cq->compl_lock);
			atomic_set(&cq->arm_state, 0) ;
			if (!nq->cqn_handler(nq, (cq)))
				nq->stats.num_cqne_processed++;
			else
				dev_warn(&pdev->dev,
					 "QPLIB: cqn - type 0x%x not handled",
					 type);
			cq->cnq_events++;
			spin_unlock_bh(&cq->compl_lock);
			break;
		}
		case NQ_BASE_TYPE_SRQ_EVENT:
		{
			struct bnxt_qplib_srq *srq;
			struct bnxt_re_srq *srq_p;
			struct nq_srq_event *nqsrqe =
						(struct nq_srq_event *)nqe;

			q_handle = le32_to_cpu(nqsrqe->srq_handle_low);
			q_handle |= (u64)le32_to_cpu(nqsrqe->srq_handle_high) << 32;
			srq = (struct bnxt_qplib_srq *)q_handle;
			srq->toggle = (le16_to_cpu(nqe->info10_type) & NQ_CN_TOGGLE_MASK)
				      >> NQ_CN_TOGGLE_SFT;
			srq->dbinfo.toggle = srq->toggle;
			srq_p = container_of(srq, struct bnxt_re_srq,
					     qplib_srq);
			if (srq_p->srq_toggle_page)
				*((u32 *)srq_p->srq_toggle_page) = srq->toggle;

			bnxt_qplib_armen_db(&srq->dbinfo,
					    DBC_DBC_TYPE_SRQ_ARMENA);
			if (!nq->srqn_handler(nq,
					      (struct bnxt_qplib_srq *)q_handle,
					      nqsrqe->event))
				nq->stats.num_srqne_processed++;
			else
				dev_warn(&pdev->dev,
					 "QPLIB: SRQ event 0x%x not handled",
					 nqsrqe->event);
			break;
		}
		case NQ_BASE_TYPE_DBQ_EVENT:
			rc = bnxt_qplib_process_dbqn(nq,
						(struct nq_dbq_event *)nqe);
			nq->stats.num_dbqne_processed++;
			break;
		default:
			dev_warn(&pdev->dev,
				 "QPLIB: nqe with opcode = 0x%x not handled",
				 type);
			break;
		}
		hw_polled++;
		bnxt_qplib_hwq_incr_cons(nq_hwq->max_elements, &nq_hwq->cons,
					 1, &nq->nq_db.dbinfo.flags);
	}
	nqe = bnxt_qplib_get_qe(nq_hwq, nq_hwq->cons, NULL);
	if (!NQE_CMP_VALID(nqe, nq->nq_db.dbinfo.flags)) {
		nq->stats.num_nq_rearm++;
		bnxt_qplib_ring_nq_db(&nq->nq_db.dbinfo, res->cctx, true);
	} else if (nq->requested) {
		/* Update the consumer index only and dont enable arm */
		bnxt_qplib_ring_nq_db(&nq->nq_db.dbinfo, res->cctx, false);
		nq->stats.num_tasklet_resched++;
		tasklet_schedule(&nq->nq_tasklet);
	}
	dev_dbg(&pdev->dev, "QPLIB: cqn/srqn/dbqn ");
	dev_dbg(&pdev->dev,
		"QPLIB: serviced %llu/%llu/%llu budget 0x%x reaped 0x%x",
		nq->stats.num_cqne_processed, nq->stats.num_srqne_processed,
		nq->stats.num_dbqne_processed, budget, hw_polled);
	dev_dbg(&pdev->dev,
		"QPLIB: resched_cnt  = %llu arm_count = %llu\n",
		nq->stats.num_tasklet_resched, nq->stats.num_nq_rearm);
	spin_unlock_bh(&nq_hwq->lock);
}

/* bnxt_re_synchronize_nq - self polling notification queue.
 * @nq      -     notification queue pointer
 *
 * This function will start polling entries of a given notification queue
 * for all pending  entries.
 * This function is useful to synchronize notification entries while resources
 * are going away.
 *
 *
 * Returns: Nothing
 *
 */
void bnxt_re_synchronize_nq(struct bnxt_qplib_nq *nq)
{
	nq->budget = nq->hwq.max_elements;
	bnxt_qplib_service_nq(
#ifdef HAS_TASKLET_SETUP
		(struct tasklet_struct *)&nq->nq_tasklet
#else
		(unsigned long)nq
#endif
		);
}

static irqreturn_t bnxt_qplib_nq_irq(int irq, void *dev_instance)
{
	struct bnxt_qplib_nq *nq = dev_instance;
	struct bnxt_qplib_hwq *nq_hwq = &nq->hwq;
	u32 sw_cons;

	/* Prefetch the NQ element */
	sw_cons = HWQ_CMP(nq_hwq->cons, nq_hwq);
	prefetch(bnxt_qplib_get_qe(nq_hwq, sw_cons, NULL));

	/* Fan out to CPU affinitized kthreads? */
	tasklet_schedule(&nq->nq_tasklet);

	return IRQ_HANDLED;
}

void bnxt_qplib_nq_stop_irq(struct bnxt_qplib_nq *nq, bool kill)
{
	struct bnxt_qplib_res *res;

	if (!nq->requested)
		return;

	nq->requested = false;
	res = nq->res;
	/* Mask h/w interrupt */
	bnxt_qplib_ring_nq_db(&nq->nq_db.dbinfo, res->cctx, false);
	/* Sync with last running IRQ handler */
	synchronize_irq(nq->msix_vec);
	irq_set_affinity_hint(nq->msix_vec, NULL);
	free_irq(nq->msix_vec, nq);
	kfree(nq->name);
	nq->name = NULL;

	/* Cleanup Tasklet */
	if (kill)
		tasklet_kill(&nq->nq_tasklet);
	tasklet_disable(&nq->nq_tasklet);
}

void bnxt_qplib_disable_nq(struct bnxt_qplib_nq *nq)
{
	if (nq->cqn_wq) {
		destroy_workqueue(nq->cqn_wq);
		nq->cqn_wq = NULL;
	}
	/* Make sure the HW is stopped! */
	bnxt_qplib_nq_stop_irq(nq, true);

	nq->nq_db.reg.bar_reg = NULL;
	nq->nq_db.db = NULL;

	nq->cqn_handler = NULL;
	nq->srqn_handler = NULL;
	nq->msix_vec = 0;
}

int bnxt_qplib_nq_start_irq(struct bnxt_qplib_nq *nq, int nq_indx,
			    int msix_vector, bool need_init)
{
	struct bnxt_qplib_res *res;
	int rc;

	res = nq->res;
	if (nq->requested)
		return -EFAULT;

	nq->msix_vec = msix_vector;
	if (need_init)
		compat_tasklet_init(&nq->nq_tasklet, bnxt_qplib_service_nq,
				    (unsigned long)nq);
	else
		tasklet_enable(&nq->nq_tasklet);

	nq->name = kasprintf(GFP_KERNEL, "bnxt_re-nq-%d@pci:%s",
			     nq_indx, pci_name(res->pdev));
	if (!nq->name)
		return -ENOMEM;
	rc = request_irq(nq->msix_vec, bnxt_qplib_nq_irq, 0, nq->name, nq);
	if (rc) {
		kfree(nq->name);
		nq->name = NULL;
		tasklet_disable(&nq->nq_tasklet);
		return rc;
	}

	cpumask_clear(&nq->mask);
	cpumask_set_cpu(nq_indx, &nq->mask);
	rc = irq_set_affinity_hint(nq->msix_vec, &nq->mask);
	if (rc)
		dev_warn(&res->pdev->dev,
			 "QPLIB: set affinity failed; vector: %d nq_idx: %d\n",
			 nq->msix_vec, nq_indx);
        nq->requested = true;
	bnxt_qplib_ring_nq_db(&nq->nq_db.dbinfo, res->cctx, true);

	return rc;
}

static void bnxt_qplib_map_nq_db(struct bnxt_qplib_nq *nq,  u32 reg_offt)
{
	struct bnxt_qplib_reg_desc *dbreg;
	struct bnxt_qplib_nq_db *nq_db;
	struct bnxt_qplib_res *res;

	nq_db = &nq->nq_db;
	res = nq->res;
	dbreg = &res->dpi_tbl.ucreg;

	nq_db->reg.bar_id = dbreg->bar_id;
	nq_db->reg.bar_base = dbreg->bar_base;
	nq_db->reg.bar_reg = dbreg->bar_reg + reg_offt;
	nq_db->reg.len = _is_chip_p5_plus(res->cctx) ? sizeof(u64) :
						      sizeof(u32);

        nq_db->dbinfo.db = nq_db->reg.bar_reg;
        nq_db->dbinfo.hwq = &nq->hwq;
        nq_db->dbinfo.xid = nq->ring_id;
	nq_db->dbinfo.seed = nq->ring_id;
	nq_db->dbinfo.flags = 0;
	spin_lock_init(&nq_db->dbinfo.lock);
	nq_db->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
	nq_db->dbinfo.res = nq->res;

	return;
}

int bnxt_qplib_enable_nq(struct bnxt_qplib_nq *nq, int nq_idx,
			 int msix_vector, int bar_reg_offset,
			 cqn_handler_t cqn_handler,
			 srqn_handler_t srqn_handler)
{
	struct pci_dev *pdev;
	int rc;

	pdev = nq->res->pdev;
	nq->cqn_handler = cqn_handler;
	nq->srqn_handler = srqn_handler;
	nq->load = 0;
	mutex_init(&nq->lock);

	/* Have a task to schedule CQ notifiers in post send case */
	nq->cqn_wq  = create_singlethread_workqueue("bnxt_qplib_nq");
	if (!nq->cqn_wq)
		return -ENOMEM;

	bnxt_qplib_map_nq_db(nq, bar_reg_offset);
	rc = bnxt_qplib_nq_start_irq(nq, nq_idx, msix_vector, true);
	if (rc) {
		dev_err(&pdev->dev,
			"QPLIB: Failed to request irq for nq-idx %d", nq_idx);
		goto fail;
	}
	dev_dbg(&pdev->dev, "QPLIB: NQ max = 0x%x", nq->hwq.max_elements);

	return 0;
fail:
	bnxt_qplib_disable_nq(nq);
	return rc;
}

void bnxt_qplib_free_nq_mem(struct bnxt_qplib_nq *nq)
{
	if (nq->hwq.max_elements) {
		bnxt_qplib_free_hwq(nq->res, &nq->hwq);
		nq->hwq.max_elements = 0;
	}
}

int bnxt_qplib_alloc_nq_mem(struct bnxt_qplib_res *res,
			    struct bnxt_qplib_nq *nq)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_sg_info sginfo = {};

	nq->res = res;
	if (!nq->hwq.max_elements ||
	    nq->hwq.max_elements > BNXT_QPLIB_NQE_MAX_CNT)
		nq->hwq.max_elements = BNXT_QPLIB_NQE_MAX_CNT;

	sginfo.pgsize = PAGE_SIZE;
	sginfo.pgshft = PAGE_SHIFT;
	hwq_attr.res = res;
	hwq_attr.sginfo = &sginfo;
	hwq_attr.depth = nq->hwq.max_elements;
	hwq_attr.stride = sizeof(struct nq_base);
	hwq_attr.type = _get_hwq_type(res);
	if (bnxt_qplib_alloc_init_hwq(&nq->hwq, &hwq_attr)) {
		dev_err(&res->pdev->dev, "QPLIB: FP NQ allocation failed");
		return -ENOMEM;
	}
	nq->budget = 8;
	return 0;
}

/* SRQ */
static int __qplib_destroy_srq(struct bnxt_qplib_rcfw *rcfw,
			       struct bnxt_qplib_srq *srq,
			       u16 sb_resp_size, void *sb_resp_va)
{
	struct creq_destroy_srq_resp resp = {};
	struct bnxt_qplib_rcfw_sbuf sbuf = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_destroy_srq req = {};
	int rc = 0;

	/* Configure the request */
	req.srq_cid = cpu_to_le32(srq->id);
	if (sb_resp_size && sb_resp_va) {
		sbuf.size = sb_resp_size;
		sbuf.sb = dma_zalloc_coherent(&rcfw->pdev->dev, sbuf.size,
					      &sbuf.dma_addr, GFP_KERNEL);
	}
	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_DESTROY_SRQ,
				 sizeof(req));
	req.resp_addr = cpu_to_le64(sbuf.dma_addr);
	req.resp_size = sb_resp_size / BNXT_QPLIB_CMDQE_UNITS;
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);

	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);

	if (sbuf.sb) {
		memcpy(sb_resp_va, sbuf.sb, sb_resp_size);
		dma_free_coherent(&rcfw->pdev->dev, sbuf.size, sbuf.sb, sbuf.dma_addr);
	}


	return rc;
}

int bnxt_qplib_destroy_srq(struct bnxt_qplib_res *res,
			   struct bnxt_qplib_srq *srq,
			   u16 sb_resp_size, void *sb_resp_va)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	int rc;

	rc = __qplib_destroy_srq(rcfw, srq, sb_resp_size, sb_resp_va);
	if (rc)
		return rc;
	bnxt_qplib_free_hwq(res, &srq->hwq);
	kfree(srq->swq);
	return 0;
}

int bnxt_qplib_create_srq(struct bnxt_qplib_res *res,
			  struct bnxt_qplib_srq *srq)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_create_srq_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_create_srq req = {};
	u16 pg_sz_lvl = 0;
	u16 flags = 0;
	u32 fwo = 0;
	u16 srq_size;
	int rc, idx;

	hwq_attr.res = res;
	hwq_attr.sginfo = &srq->sginfo;
	hwq_attr.depth = srq->max_wqe;
	hwq_attr.stride = srq->wqe_size;
	hwq_attr.type = HWQ_TYPE_QUEUE;
	rc = bnxt_qplib_alloc_init_hwq(&srq->hwq, &hwq_attr);
	if (rc)
		goto exit;
	/* Configure the request */
	req.dpi = cpu_to_le32(srq->dpi->dpi);
	req.srq_handle = cpu_to_le64(srq);
	srq_size = min_t(u32, srq->hwq.depth, U16_MAX);
	req.srq_size = cpu_to_le16(srq_size);
	pg_sz_lvl |= (_get_base_pg_size(&srq->hwq) <<
		      CMDQ_CREATE_SRQ_PG_SIZE_SFT);
	pg_sz_lvl |= (srq->hwq.level & CMDQ_CREATE_SRQ_LVL_MASK);
	req.pg_size_lvl = cpu_to_le16(pg_sz_lvl);
	if (bnxt_re_pbl_size_supported(res->dattr->dev_cap_ext_flags_1)) {
		flags |= CMDQ_CREATE_SRQ_FLAGS_PBL_PG_SIZE_VALID;
		req.pbl_pg_size = _get_pbl_page_size(&srq->sginfo);
	}
	req.pbl = cpu_to_le64(_get_base_addr(&srq->hwq));
	req.pd_id = cpu_to_le32(srq->pd->id);
	req.eventq_id = cpu_to_le16(srq->eventq_hw_ring_id);
	if (srq->small_recv_wqe_sup)
		req.srq_fwo = (srq->max_sge << CMDQ_CREATE_SRQ_SRQ_SGE_SFT) &
			       CMDQ_CREATE_SRQ_SRQ_SGE_MASK;
	fwo = hwq_attr.sginfo->fwo_offset;
	if (fwo)
		req.srq_fwo |= cpu_to_le16(((fwo >> BNXT_RE_PBL_4K_FWO)
					   << CMDQ_CREATE_SRQ_SRQ_FWO_SFT) &
					   CMDQ_CREATE_SRQ_SRQ_FWO_MASK);

	if (hwq_attr.sginfo->pgsize != PAGE_SIZE)
		dev_dbg(&res->pdev->dev,
			"SRQ pgsize %d pgshft %d npages %d lvl %d fwo %d shft 0x%x srq_fwo 0x%x\n",
			hwq_attr.sginfo->pgsize, hwq_attr.sginfo->pgshft,
			hwq_attr.sginfo->npages, srq->hwq.level, fwo,
			fwo >> BNXT_RE_PBL_4K_FWO, req.srq_fwo);

	if (res->cctx->modes.st_tag_supported) {
		flags |= CMDQ_CREATE_SRQ_FLAGS_STEERING_TAG_VALID;
		req.steering_tag = cpu_to_le16(BNXT_RE_STEERING_TO_HOST);
	}
	req.flags = cpu_to_le16(flags);

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_CREATE_SRQ,
				 sizeof(req));
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto fail;
	if (!srq->is_user) {
		srq->swq = kcalloc(srq->hwq.depth, sizeof(*srq->swq),
				   GFP_KERNEL);
		if (!srq->swq)
			goto srq_fail;
		srq->start_idx = 0;
		srq->last_idx = srq->hwq.depth - 1;
		for (idx = 0; idx < srq->hwq.depth; idx++)
			srq->swq[idx].next_idx = idx + 1;
		srq->swq[srq->last_idx].next_idx = -1;
	}

	spin_lock_init(&srq->lock);
	srq->id = le32_to_cpu(resp.xid);
	srq->cctx = res->cctx;
	srq->dbinfo.hwq = &srq->hwq;
	srq->dbinfo.xid = srq->id;
	srq->dbinfo.db = srq->dpi->dbr;
	srq->dbinfo.max_slot = 1;
	srq->dbinfo.priv_db = res->dpi_tbl.priv_db;
	srq->dbinfo.flags = 0;
	spin_lock_init(&srq->dbinfo.lock);
	srq->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
	srq->dbinfo.shadow_key_arm_ena = BNXT_QPLIB_DBR_KEY_INVALID;
	srq->dbinfo.res = res;
	srq->dbinfo.seed = srq->id;

	if (rcfw->roce_context_destroy_sb) {
		srq->ctx_size_sb = resp.context_size * BNXT_QPLIB_CMDQE_UNITS;
		if (!rcfw->srq_ctxm_data) {
			rcfw->srq_ctxm_data = vzalloc(CTXM_DATA_INDEX_MAX * srq->ctx_size_sb);
			rcfw->srq_ctxm_size = srq->ctx_size_sb;
		}
	}

	bnxt_qplib_armen_db(&srq->dbinfo, DBC_DBC_TYPE_SRQ_ARMENA);
	return 0;
srq_fail:
	__qplib_destroy_srq(rcfw, srq, 0, NULL);
fail:
	bnxt_qplib_free_hwq(res, &srq->hwq);
exit:
	return rc;
}

void bnxt_qplib_modify_srq(struct bnxt_qplib_res *res,
			   struct bnxt_qplib_srq *srq)
{
	struct bnxt_qplib_hwq *srq_hwq = &srq->hwq;
	unsigned long flags;

	spin_lock_irqsave(&srq_hwq->lock, flags);
	bnxt_qplib_srq_arm_db(&srq->dbinfo, srq->srq_limit_drv);
	spin_unlock_irqrestore(&srq_hwq->lock, flags);
}

int bnxt_qplib_query_srq(struct bnxt_qplib_res *res,
			 struct bnxt_qplib_srq *srq)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_query_srq_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct creq_query_srq_resp_sb *sb;
	struct bnxt_qplib_rcfw_sbuf sbuf;
	struct cmdq_query_srq req = {};
	int rc = 0;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_QUERY_SRQ,
				 sizeof(req));
	sbuf.size = ALIGN(sizeof(*sb), BNXT_QPLIB_CMDQE_UNITS);
	sbuf.sb = dma_zalloc_coherent(&rcfw->pdev->dev, sbuf.size,
				       &sbuf.dma_addr, GFP_KERNEL);
	if (!sbuf.sb)
		return -ENOMEM;
	req.resp_size = sbuf.size / BNXT_QPLIB_CMDQE_UNITS;
	req.srq_cid = cpu_to_le32(srq->id);
	sb = sbuf.sb;
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, &sbuf, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (!rc)
		srq->srq_limit_hw = le16_to_cpu(sb->srq_limit);
	dma_free_coherent(&rcfw->pdev->dev, sbuf.size,
				  sbuf.sb, sbuf.dma_addr);

	return rc;
}

int bnxt_qplib_post_srq_recv(struct bnxt_qplib_srq *srq,
			     struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_qplib_hwq *srq_hwq = &srq->hwq;
	struct sq_sge *hw_sge;
	struct rq_wqe *srqe;
	int i, rc = 0, next;

	spin_lock(&srq_hwq->lock);
	if (srq->start_idx == srq->last_idx) {
		dev_err(&srq_hwq->pdev->dev, "QPLIB: FP: SRQ (0x%x) is full!",
			srq->id);
		rc = -EINVAL;
		spin_unlock(&srq_hwq->lock);
		goto done;
	}
	next = srq->start_idx;
	srq->start_idx = srq->swq[next].next_idx;
	spin_unlock(&srq_hwq->lock);

	srqe = bnxt_qplib_get_qe(srq_hwq, srq_hwq->prod, NULL);
	memset(srqe, 0, srq->wqe_size);
	/* Calculate wqe_size and data_len */
	for (i = 0, hw_sge = (struct sq_sge *)srqe->data;
	     i < wqe->num_sge; i++, hw_sge++) {
		hw_sge->va_or_pa = cpu_to_le64(wqe->sg_list[i].addr);
		hw_sge->l_key = cpu_to_le32(wqe->sg_list[i].lkey);
		hw_sge->size = cpu_to_le32(wqe->sg_list[i].size);
	}
	srqe->wqe_type = wqe->type;
	srqe->flags = wqe->flags;
	srqe->wqe_size = wqe->num_sge +
			((offsetof(typeof(*srqe), data) + 15) >> 4);
	if (!wqe->num_sge)
		srqe->wqe_size++;
	srqe->wr_id[0] = cpu_to_le32((u32)next);
	srq->swq[next].wr_id = wqe->wr_id;
	bnxt_qplib_hwq_incr_prod(&srq->dbinfo, srq_hwq, srq->dbinfo.max_slot);
	/* Ring DB */
	bnxt_qplib_ring_prod_db(&srq->dbinfo, DBC_DBC_TYPE_SRQ);
done:
	return rc;
}

/* QP */
static int __qplib_destroy_qp(struct bnxt_qplib_rcfw *rcfw,
			      struct bnxt_qplib_qp *qp,
			      u16 sb_resp_size, void *sb_resp_va,
			      void *destroy_qp_resp)
{
	struct creq_destroy_qp_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_destroy_qp req = {};
	struct bnxt_qplib_rcfw_sbuf sbuf = {};
	int rc = 0;

	if (sb_resp_size && sb_resp_va) {
		sbuf.size = sb_resp_size;
		sbuf.sb = dma_zalloc_coherent(&rcfw->pdev->dev, sbuf.size,
					      &sbuf.dma_addr, GFP_KERNEL);
	}

	req.qp_cid = cpu_to_le32(qp->id);
	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_DESTROY_QP,
				 sizeof(req));

	/* In some cases resp_addr might be NULL.
	 * Firmware handle this case and it ignore side-band DMA
	 */
	req.resp_addr = cpu_to_le64(sbuf.dma_addr);
	req.resp_size = sb_resp_size / BNXT_QPLIB_CMDQE_UNITS;
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, &sbuf, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);

	if (sbuf.sb) {
		print_hex_dump_bytes("", DUMP_PREFIX_OFFSET, sbuf.sb, sb_resp_size);
		memcpy(sb_resp_va, sbuf.sb, sb_resp_size);
		dma_free_coherent(&rcfw->pdev->dev, sbuf.size, sbuf.sb, sbuf.dma_addr);
	}

	if (destroy_qp_resp)
		memcpy(destroy_qp_resp, &resp, sizeof(resp));

	return rc;
}

static struct bnxt_qplib_swq *bnxt_qplib_get_swqe(struct bnxt_qplib_q *que,
						  u32 *swq_idx)
{
	u32 idx;

	idx = que->swq_start;
	if (swq_idx)
		*swq_idx = idx;
	return &que->swq[idx];
}

static void bnxt_qplib_swq_mod_start(struct bnxt_qplib_q *que, u32 idx)
{
	que->swq_start = que->swq[idx].next_idx;
}

u32 bnxt_qplib_get_stride(void)
{
	return sizeof(struct sq_sge);
}

u32 bnxt_qplib_get_depth(struct bnxt_qplib_q *que, u8 wqe_mode, bool is_sq)
{
	u32 slots;

	/* Queue depth is the number of slots. */
	slots = (que->wqe_size * que->max_wqe) / bnxt_qplib_get_stride();
	/* For variable WQE mode, need to align the slots to 256 */
	if (wqe_mode == BNXT_QPLIB_WQE_MODE_VARIABLE && is_sq)
		slots = ALIGN(slots, BNXT_VAR_MAX_SLOT_ALIGN);
	return slots;
}

u32 bnxt_re_set_sq_size(struct bnxt_qplib_q *que, u8 wqe_mode)
{
	/* For static wqe mode, sq_size is max_wqe.
	 * For variable wqe mode, sq_size is que depth.
	 */
	return (wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC) ?
		que->max_wqe : bnxt_qplib_get_depth(que, wqe_mode, true);
}

u32 _set_sq_max_slot(u8 wqe_mode)
{
	/* for static mode index divisor is 8 */
	return (wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC) ?
		sizeof(struct sq_send) / sizeof(struct sq_sge) : 1;
}

u32 _set_rq_max_slot(struct bnxt_qplib_q *que)
{
	return (que->wqe_size / sizeof(struct sq_sge));
}

static int bnxt_qplib_alloc_init_swq(struct bnxt_qplib_q *que)
{
	int indx;

	que->swq = kcalloc(que->max_sw_wqe, sizeof(*que->swq), GFP_KERNEL);
	if (!que->swq)
		return -ENOMEM;

	que->swq_start = 0;
	que->swq_last = que->max_sw_wqe - 1;
	for (indx = 0; indx < que->max_sw_wqe; indx++)
		que->swq[indx].next_idx = indx + 1;
	que->swq[que->swq_last].next_idx = 0; /* Make it circular */
	que->swq_last = 0;

	return 0;
}

static int bnxt_re_setup_qp_swqs(struct bnxt_qplib_qp *qplqp)
{
	struct bnxt_qplib_q *sq = &qplqp->sq;
	struct bnxt_qplib_q *rq = &qplqp->rq;
	int rc;

	rc = bnxt_qplib_alloc_init_swq(sq);
	if (rc)
		return rc;

	if (!qplqp->srq) {
		rc = bnxt_qplib_alloc_init_swq(rq);
		if (rc)
			goto free_sq_swq;
	}

	return 0;
free_sq_swq:
	kfree(sq->swq);
	sq->swq = NULL;
	return rc;
}

static void bnxt_qp_init_dbinfo(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_q *sq = &qp->sq;
	struct bnxt_qplib_q *rq = &qp->rq;

	sq->dbinfo.hwq = &sq->hwq;
	sq->dbinfo.xid = qp->id;
	sq->dbinfo.db = qp->dpi->dbr;
	sq->dbinfo.max_slot = _set_sq_max_slot(qp->wqe_mode);
	sq->dbinfo.flags = 0;
	spin_lock_init(&sq->dbinfo.lock);
	sq->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
	sq->dbinfo.res = res;
	sq->dbinfo.seed = qp->id;
	if (rq->max_wqe) {
		rq->dbinfo.hwq = &rq->hwq;
		rq->dbinfo.xid = qp->id;
		rq->dbinfo.db = qp->dpi->dbr;
		rq->dbinfo.max_slot = _set_rq_max_slot(rq);
		rq->dbinfo.flags = 0;
		spin_lock_init(&rq->dbinfo.lock);
		rq->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
		rq->dbinfo.res = res;
		rq->dbinfo.seed = qp->id;
	}
}

static void bnxt_qplib_init_psn_ptr(struct bnxt_qplib_qp *qp, int size)
{
	struct bnxt_qplib_hwq *sq_hwq;
	struct bnxt_qplib_q *sq;
	u64 fpsne, psn_pg;
	u16 indx_pad = 0;

	sq = &qp->sq;
	sq_hwq = &sq->hwq;
	/* First psn entry */
	fpsne = (u64)bnxt_qplib_get_qe(sq_hwq, sq_hwq->depth, &psn_pg);
	if (!IS_ALIGNED(fpsne, PAGE_SIZE))
		indx_pad = (fpsne & ~PAGE_MASK) / size;
	sq_hwq->pad_pgofft = indx_pad;
	sq_hwq->pad_pg = (u64 *)psn_pg;
	sq_hwq->pad_stride = size;
}

int bnxt_qplib_create_qp1(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_create_qp1_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct bnxt_qplib_q *sq = &qp->sq;
	struct bnxt_qplib_q *rq = &qp->rq;
	struct cmdq_create_qp1 req = {};
	struct bnxt_qplib_reftbl *tbl;
	unsigned long flag;
	u32 qp_flags = 0;
	int rc;

	/* General */
	req.type = qp->type;
	req.dpi = cpu_to_le32(qp->dpi->dpi);
	req.qp_handle = cpu_to_le64(qp->qp_handle);
	/* SQ */
	sq->max_sw_wqe = bnxt_re_set_sq_size(sq, qp->wqe_mode);
	req.sq_size = cpu_to_le32(sq->max_sw_wqe);
	req.sq_pg_size_sq_lvl = sq->hwq.pg_sz_lvl;
	req.sq_pbl = cpu_to_le64(_get_base_addr(&sq->hwq));
	req.sq_fwo_sq_sge = cpu_to_le16(((0 << CMDQ_CREATE_QP1_SQ_FWO_SFT) &
					 CMDQ_CREATE_QP1_SQ_FWO_MASK) |
					 (sq->max_sge &
					  CMDQ_CREATE_QP1_SQ_SGE_MASK));
	req.scq_cid = cpu_to_le32(qp->scq->id);

	/* RQ */
	if (!qp->srq) {
		req.rq_size = cpu_to_le32(rq->max_wqe);
		req.rq_pbl = cpu_to_le64(_get_base_addr(&rq->hwq));
		req.rq_pg_size_rq_lvl = rq->hwq.pg_sz_lvl;
		req.rq_fwo_rq_sge =
			cpu_to_le16(((0 << CMDQ_CREATE_QP1_RQ_FWO_SFT) &
				     CMDQ_CREATE_QP1_RQ_FWO_MASK) |
				     (rq->max_sge &
				     CMDQ_CREATE_QP1_RQ_SGE_MASK));
	} else {
		/* SRQ */
		qp_flags |= CMDQ_CREATE_QP1_QP_FLAGS_SRQ_USED;
		req.srq_cid = cpu_to_le32(qp->srq->id);
	}
	req.rcq_cid = cpu_to_le32(qp->rcq->id);

	qp_flags |= CMDQ_CREATE_QP1_QP_FLAGS_RESERVED_LKEY_ENABLE;
	req.qp_flags = cpu_to_le32(qp_flags);
	req.pd_id = cpu_to_le32(qp->pd_id);

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_CREATE_QP1,
				 sizeof(req));
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		return rc;

	qp->id = le32_to_cpu(resp.xid);

	qp->cur_qp_state = CMDQ_MODIFY_QP_NEW_STATE_RESET;

	rc = bnxt_re_setup_qp_swqs(qp);
	if (rc)
		goto destroy_qp;
	bnxt_qp_init_dbinfo(res, qp);

	tbl = &res->reftbl.qpref;
	spin_lock_irqsave(&tbl->lock, flag);
	tbl->rec[tbl->max].xid = qp->id;
	tbl->rec[tbl->max].handle = qp;
	spin_unlock_irqrestore(&tbl->lock, flag);

	return 0;
destroy_qp:
	__qplib_destroy_qp(rcfw, qp, 0, NULL, NULL);
	return rc;
}

int bnxt_qplib_create_qp(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_create_qp_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct bnxt_qplib_q *sq = &qp->sq;
	struct bnxt_qplib_q *rq = &qp->rq;
	struct cmdq_create_qp req = {};
	struct bnxt_qplib_reftbl *tbl;
	bool internal_queue_memory;
	u32 qp_flags = 0, qp_idx;
	unsigned long flag;
	u32 fwo = 0;
	u16 nsge;
	int rc;

	qp->dev_cap_flags = res->dattr->dev_cap_flags;
	qp->dev_cap_ext_flags = res->dattr->dev_cap_ext_flags;
	qp->dev_cap_ext_flags2 = res->dattr->dev_cap_ext_flags2;
	internal_queue_memory = BNXT_RE_IQM(qp->dev_cap_flags);

	if (bnxt_ext_stats_v2_supported(qp->dev_cap_ext_flags) &&
	    (qp->type != CMDQ_CREATE_QP_TYPE_GSI)) {
		req.ext_stats_ctx_id = cpu_to_le32(qp->ext_stats_ctx_id);
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_EXT_STATS_CTX_VALID;
	}

	/* General */
	req.type = qp->type;
	req.dpi = cpu_to_le32(qp->dpi->dpi);
	req.qp_handle = cpu_to_le64(qp->qp_handle);
	req.sq_size = cpu_to_le32(sq->max_sw_wqe);

	if (internal_queue_memory) {
		qp->sq_max_num_wqes = min_t(u16, sq->max_wqe - 1,
					    SQ_MAX_NUM_WQES_DEFAULT);
		req.sq_max_num_wqes = cpu_to_le16(qp->sq_max_num_wqes);
	}

	if (internal_queue_memory && qp->exp_mode) {
		/* express mode and onchip qp metadata, SQ in IQM */
		req.sq_pbl = 0;
		req.sq_pg_size_sq_lvl = 0;
	} else {
		req.sq_pbl = cpu_to_le64(_get_base_addr(&sq->hwq));
		req.sq_pg_size_sq_lvl = sq->hwq.pg_sz_lvl;
		if (bnxt_re_pbl_size_supported(res->dattr->dev_cap_ext_flags_1)) {
			qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_SQ_PBL_PG_SIZE_VALID;
			req.sq_pbl_pg_size = _get_pbl_page_size(&sq->sginfo);
		}
		fwo = sq->sginfo.fwo_offset;
	}
	req.sq_fwo_sq_sge = cpu_to_le16((((fwo >> BNXT_RE_PBL_4K_FWO) <<
					 CMDQ_CREATE_QP_SQ_FWO_SFT) &
					 CMDQ_CREATE_QP_SQ_FWO_MASK) |
					(sq->max_sge & CMDQ_CREATE_QP_SQ_SGE_MASK));
	req.scq_cid = cpu_to_le32(qp->scq->id);

	/* RQ/SRQ */
	fwo = 0;
	if (!qp->srq) {
		if (internal_queue_memory && qp->exp_mode) {
			/* express mode and onchip qp metadata, RQ in IQM */
			req.rq_pbl = 0;
			req.rq_pg_size_rq_lvl = 0;
		} else {
			req.rq_size = cpu_to_le32(rq->max_wqe);
			req.rq_pbl = cpu_to_le64(_get_base_addr(&rq->hwq));
			req.rq_pg_size_rq_lvl = rq->hwq.pg_sz_lvl;
			if (bnxt_re_pbl_size_supported(res->dattr->dev_cap_ext_flags_1)) {
				qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_RQ_PBL_PG_SIZE_VALID;
				req.rq_pbl_pg_size = _get_pbl_page_size(&rq->sginfo);
			}
			fwo = rq->sginfo.fwo_offset;
		}
		nsge = (qp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC) ?
			res->dattr->max_qp_sges : rq->max_sge;
		if (qp->small_recv_wqe_sup)
			nsge = rq->max_sge;
		req.rq_fwo_rq_sge =
			cpu_to_le16((((fwo >> BNXT_RE_PBL_4K_FWO) <<
				     CMDQ_CREATE_QP_RQ_FWO_SFT) &
				     CMDQ_CREATE_QP_RQ_FWO_MASK) |
				    (nsge & CMDQ_CREATE_QP_RQ_SGE_MASK));
	} else {
		/* express mode does not support SRQ */
		if (qp->exp_mode)
			return -EINVAL;
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_SRQ_USED;
		req.srq_cid = cpu_to_le32(qp->srq->id);
	}
	req.rcq_cid = cpu_to_le32(qp->rcq->id);

	qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_RESERVED_LKEY_ENABLE;
	qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_FR_PMR_ENABLED;
	if (qp->sig_type)
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_FORCE_COMPLETION;
	if (qp->wqe_mode == BNXT_QPLIB_WQE_MODE_VARIABLE)
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_VARIABLE_SIZED_WQE_ENABLED;
	if (res->cctx->modes.te_bypass)
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_OPTIMIZED_TRANSMIT_ENABLED;
	if (res->dattr &&
	    bnxt_ext_stats_supported(qp->cctx, res->dattr->dev_cap_flags, res->is_vf))
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_EXT_STATS_ENABLED;

	if (res->cctx->modes.st_tag_supported) {
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_STEERING_TAG_VALID;
		req.steering_tag = cpu_to_le16(BNXT_RE_STEERING_TO_HOST);
	}

	if (qp->exp_mode)
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_EXPRESS_MODE_ENABLED;

	if (_is_chip_p8(res->cctx) && qp->type == CMDQ_CREATE_QP_TYPE_RC)
		/* Always support read/atomic for RC QP now */
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_RDMA_READ_OR_ATOMICS_USED;

	req.qp_flags = cpu_to_le32(qp_flags);

	/* ORRQ and IRRQ */
	if (internal_queue_memory) {
		/* onchip qp metadata, ORRQ/IRRQ in IQM */
		req.orrq_addr = 0;
		req.irrq_addr = 0;
	} else if (qp->psn_sz) {
		req.orrq_addr = cpu_to_le64(_get_base_addr(&qp->orrq));
		req.irrq_addr = cpu_to_le64(_get_base_addr(&qp->irrq));
	}

	req.pd_id = cpu_to_le32(qp->pd_id);
	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_CREATE_QP,
				 sizeof(req));
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		return rc;

	/* Store xid immediately so that we can destroy QP on error path */
	qp->id = le32_to_cpu(resp.xid);

	if (!qp->is_user) {
		rc = bnxt_re_setup_qp_swqs(qp);
		if (rc)
			goto destroy_qp;
		if (qp->psn_sz)
			bnxt_qplib_init_psn_ptr(qp, qp->psn_sz);
	}
	bnxt_qp_init_dbinfo(res, qp);

	if (rcfw->roce_context_destroy_sb) {
		qp->ctx_size_sb = resp.context_size * BNXT_QPLIB_CMDQE_UNITS;
		if (!rcfw->qp_ctxm_data) {
			rcfw->qp_ctxm_data = vzalloc(CTXM_DATA_INDEX_MAX * qp->ctx_size_sb);
			rcfw->qp_ctxm_size = qp->ctx_size_sb;
		}
	}

	qp->cur_qp_state = CMDQ_MODIFY_QP_NEW_STATE_RESET;
	INIT_LIST_HEAD(&qp->sq_flush);
	INIT_LIST_HEAD(&qp->rq_flush);

	tbl = &res->reftbl.qpref;
	qp_idx = map_qp_id_to_tbl_indx(qp->id, tbl);
	spin_lock_irqsave(&tbl->lock, flag);
	tbl->rec[qp_idx].xid = qp->id;
	tbl->rec[qp_idx].handle = qp;
	spin_unlock_irqrestore(&tbl->lock, flag);

	return 0;
destroy_qp:
	__qplib_destroy_qp(rcfw, qp, 0, NULL, NULL);
	return rc;
}

static void bnxt_set_mandatory_attributes(struct bnxt_qplib_res *res,
					  struct bnxt_qplib_qp *qp,
					  struct cmdq_modify_qp *req)
{
	u32 mandatory_flags = 0;

	if (qp->type == CMDQ_MODIFY_QP_QP_TYPE_RC)
		mandatory_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_ACCESS;

	if (qp->cur_qp_state == CMDQ_MODIFY_QP_NEW_STATE_INIT &&
	    qp->state == CMDQ_MODIFY_QP_NEW_STATE_RTR) {
		if (qp->type == CMDQ_MODIFY_QP_QP_TYPE_RC && qp->srq)
			req->flags = CMDQ_MODIFY_QP_FLAGS_SRQ_USED;
		mandatory_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_PKEY;
	}

	if (_is_min_rnr_in_rtr_rts_mandatory(res->dattr->dev_cap_ext_flags2) &&
	    (qp->cur_qp_state == CMDQ_MODIFY_QP_NEW_STATE_RTR &&
	     qp->state == CMDQ_MODIFY_QP_NEW_STATE_RTS)) {
		if (qp->type == CMDQ_MODIFY_QP_QP_TYPE_RC)
			mandatory_flags |=
				CMDQ_MODIFY_QP_MODIFY_MASK_MIN_RNR_TIMER;
	}

	if (qp->type == CMDQ_MODIFY_QP_QP_TYPE_UD ||
	    qp->type == CMDQ_MODIFY_QP_QP_TYPE_GSI)
		mandatory_flags |= CMDQ_MODIFY_QP_MODIFY_MASK_QKEY;

	qp->modify_flags |= mandatory_flags;
	req->qp_type = qp->type;
}

static bool is_optimized_state_transition(struct bnxt_qplib_qp *qp)
{
	if ((qp->cur_qp_state == CMDQ_MODIFY_QP_NEW_STATE_INIT &&
	     qp->state == CMDQ_MODIFY_QP_NEW_STATE_RTR) ||
	    (qp->cur_qp_state == CMDQ_MODIFY_QP_NEW_STATE_RTR &&
	     qp->state == CMDQ_MODIFY_QP_NEW_STATE_RTS))
		return true;

	return false;
}

static void __filter_modify_flags(struct bnxt_qplib_qp *qp)
{
	u32 min_max_dest_rd_atomic, min_max_rd_atomic;

	switch (qp->cur_qp_state) {
	case CMDQ_MODIFY_QP_NEW_STATE_RESET:
		switch (qp->state) {
		case CMDQ_MODIFY_QP_NEW_STATE_INIT:
			break;
		default:
			break;
		}
		break;
	case CMDQ_MODIFY_QP_NEW_STATE_INIT:
		switch (qp->state) {
		case CMDQ_MODIFY_QP_NEW_STATE_RTR:
			/* INIT->RTR, configure the path_mtu to the default
			   2048 if not being requested */
			if (!(qp->modify_flags &
			      CMDQ_MODIFY_QP_MODIFY_MASK_PATH_MTU)) {
				qp->modify_flags |=
					CMDQ_MODIFY_QP_MODIFY_MASK_PATH_MTU;
				qp->path_mtu = CMDQ_MODIFY_QP_PATH_MTU_MTU_2048;
			}
			/* Adjust max_dest_rd_atomic as per fw for thor3cp */
			if (_is_chip_p8(qp->cctx) &&
			    qp->max_dest_rd_atomic > qp->sq_max_num_wqes)
				qp->max_dest_rd_atomic = qp->sq_max_num_wqes;
			/*
			 * FW requires max_dest_rd_atomic to have min value of 3 for
			 * Thor3C/Thor3CP, 1 for other chips
			 */
			min_max_dest_rd_atomic = _is_chip_p8(qp->cctx) ? 3 : 1;
			if (qp->max_dest_rd_atomic < min_max_dest_rd_atomic)
				qp->max_dest_rd_atomic = min_max_dest_rd_atomic;

			/* TODO: Bono FW 0.0.12.0+ does not allow SRC_MAC
			   modification */
			qp->modify_flags &= ~CMDQ_MODIFY_QP_MODIFY_MASK_SRC_MAC;
			/* Bono FW 20.6.5 requires SGID_INDEX to be configured */
			if (!(qp->modify_flags &
			      CMDQ_MODIFY_QP_MODIFY_MASK_SGID_INDEX)) {
				qp->modify_flags |=
					CMDQ_MODIFY_QP_MODIFY_MASK_SGID_INDEX;
				qp->ah.sgid_index = 0;
			}
			break;
		default:
			break;
		}
		break;
	case CMDQ_MODIFY_QP_NEW_STATE_RTR:
		switch (qp->state) {
		case CMDQ_MODIFY_QP_NEW_STATE_RTS:
			/* Adjust max_rd_atomic as per fw for thor3cp */
			if (_is_chip_p8(qp->cctx) &&
			    qp->max_rd_atomic >= qp->sq_max_num_wqes)
				qp->max_rd_atomic = qp->sq_max_num_wqes - 1;
			/*
			 * FW requires max_rd_atomic to have min value of 2 for
			 * Thor3C/Thor3CP, and 1 for other chips
			 */
			min_max_rd_atomic = _is_chip_p8(qp->cctx) ? 2 : 1;
			if (qp->max_rd_atomic < min_max_rd_atomic)
				qp->max_rd_atomic = min_max_rd_atomic;

			/* TODO: Bono FW 0.0.12.0+ does not allow PKEY_INDEX,
			   DGID, FLOW_LABEL, SGID_INDEX, HOP_LIMIT,
			   TRAFFIC_CLASS, DEST_MAC, PATH_MTU, RQ_PSN,
			   MIN_RNR_TIMER, MAX_DEST_RD_ATOMIC, DEST_QP_ID
			   modification */
			qp->modify_flags &=
				~(CMDQ_MODIFY_QP_MODIFY_MASK_PKEY |
				  CMDQ_MODIFY_QP_MODIFY_MASK_DGID |
				  CMDQ_MODIFY_QP_MODIFY_MASK_FLOW_LABEL |
				  CMDQ_MODIFY_QP_MODIFY_MASK_SGID_INDEX |
				  CMDQ_MODIFY_QP_MODIFY_MASK_HOP_LIMIT |
				  CMDQ_MODIFY_QP_MODIFY_MASK_TRAFFIC_CLASS |
				  CMDQ_MODIFY_QP_MODIFY_MASK_DEST_MAC |
				  CMDQ_MODIFY_QP_MODIFY_MASK_PATH_MTU |
				  CMDQ_MODIFY_QP_MODIFY_MASK_RQ_PSN |
				  CMDQ_MODIFY_QP_MODIFY_MASK_MIN_RNR_TIMER |
				  CMDQ_MODIFY_QP_MODIFY_MASK_MAX_DEST_RD_ATOMIC |
				  CMDQ_MODIFY_QP_MODIFY_MASK_DEST_QP_ID);
			break;
		default:
			break;
		}
		break;
	case CMDQ_MODIFY_QP_NEW_STATE_RTS:
		break;
	case CMDQ_MODIFY_QP_NEW_STATE_SQD:
		break;
	case CMDQ_MODIFY_QP_NEW_STATE_SQE:
		break;
	case CMDQ_MODIFY_QP_NEW_STATE_ERR:
		break;
	default:
		break;
	}
}

static void bnxt_qplib_create_roceflow(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp)
{
	struct bnxt_dcqcn_session_entry *ses;
	int rc;

	qp->is_ses_eligible = true;

	if (!bnxt_qplib_gen_udp_src_supported(res->cctx) ||
	    qp->cur_qp_state != CMDQ_MODIFY_QP_NEW_STATE_INIT ||
	    qp->state != CMDQ_MODIFY_QP_NEW_STATE_RTR ||
	    qp->ses)
		return;

	ses = kzalloc(sizeof(*ses), GFP_KERNEL);
	if (!ses)
		return;

	ses->type = DCQCN_FLOW_TYPE_ROCE;
	ses->session_id = qp->id;
	ses->src_qp_num = qp->id;
	memcpy(ses->src_mac, qp->smac, ETH_ALEN);
	memcpy(ses->dest_mac, qp->ah.dmac, ETH_ALEN);
	ses->dest_qp_num = qp->dest_qpn;
	memcpy(&ses->dst_ip, qp->ah.dgid.data, sizeof(struct bnxt_qplib_gid));

	rc = bnxt_en_ulp_dcqcn_flow_create(res->en_dev, ses);
	if (rc) {
		kfree(ses);
		dev_dbg(&res->pdev->dev, "%s: failed to install roce flow. rc = %d\n",
			__func__, rc);
		return;
	}

	qp->ses = ses;
}

static void bnxt_qplib_del_roceflow(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp)
{
	int rc;

	if (!qp->ses)
		return;

	rc = bnxt_en_ulp_dcqcn_flow_delete(res->en_dev, qp->ses);
	if (rc)
		dev_dbg(&res->pdev->dev, "%s: failed to delete roce flow rc = %d\n",
			__func__, rc);
	kfree(qp->ses);
	qp->ses = NULL;
}

int bnxt_qplib_modify_qp(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_modify_qp_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_modify_qp req = {};
	bool ppp_requested = false;
	u32 bmask, bmask_ext;
	u32 temp32[4];
	int rc;

	bmask_ext = qp->ext_modify_flags;
	if (bmask_ext & CMDQ_MODIFY_QP_EXT_MODIFY_MASK_UDP_SRC_PORT_VALID) {
		req.udp_src_port = cpu_to_le16(qp->udp_sport);
		req.ext_modify_mask = cpu_to_le32(qp->ext_modify_flags);
	}

	if (bmask_ext & CMDQ_MODIFY_QP_EXT_MODIFY_MASK_RATE_LIMIT_VALID) {
		req.rate_limit = cpu_to_le32(qp->rate_limit);
		req.ext_modify_mask |= cpu_to_le32(qp->ext_modify_flags);
	}

	if (qp->modify_flags & CMDQ_MODIFY_QP_MODIFY_MASK_STATE) {
		/* Filter out the qp_attr_mask based on the state->new transition */
		__filter_modify_flags(qp);
		/* Set mandatory attributes for INIT -> RTR and RTR -> RTS transition */
		if (_is_optimize_modify_qp_supported(res->dattr->dev_cap_ext_flags2) &&
		    is_optimized_state_transition(qp))
			bnxt_set_mandatory_attributes(res, qp, &req);
	}
	if (qp->udcc_exclude)
		req.flags |= CMDQ_MODIFY_QP_FLAGS_EXCLUDE_QP_UDCC;
	bmask = qp->modify_flags;
	req.modify_mask = cpu_to_le32(qp->modify_flags);
	req.qp_cid = cpu_to_le32(qp->id);
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_STATE) {
		req.network_type_en_sqd_async_notify_new_state =
				(qp->state & CMDQ_MODIFY_QP_NEW_STATE_MASK) |
				(qp->en_sqd_async_notify == true ?
					CMDQ_MODIFY_QP_EN_SQD_ASYNC_NOTIFY : 0);
		if (__can_request_ppp(qp)) {
			req.path_mtu_pingpong_push_enable =
				CMDQ_MODIFY_QP_PINGPONG_PUSH_ENABLE;
			req.pingpong_push_dpi = qp->ppp.dpi;
			ppp_requested = true;
		}
	}
	req.network_type_en_sqd_async_notify_new_state |= qp->nw_type;

	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_ACCESS) {
		if (!rcfw->res->dattr->is_atomic)
			qp->access &= ~CMDQ_MODIFY_QP_ACCESS_REMOTE_ATOMIC;
		req.access = qp->access;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_PKEY)
		req.pkey = cpu_to_le16(IB_DEFAULT_PKEY_FULL);

	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_QKEY) {
		req.qkey = cpu_to_le32(qp->qkey);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_DGID) {
		memcpy(temp32, qp->ah.dgid.data, sizeof(struct bnxt_qplib_gid));
		req.dgid[0] = cpu_to_le32(temp32[0]);
		req.dgid[1] = cpu_to_le32(temp32[1]);
		req.dgid[2] = cpu_to_le32(temp32[2]);
		req.dgid[3] = cpu_to_le32(temp32[3]);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_FLOW_LABEL) {
		req.flow_label = cpu_to_le32(qp->ah.flow_label);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_SGID_INDEX) {
		if (qp->is_roce_mirror_qp)
			req.sgid_index = cpu_to_le16(res->sgid_tbl.hw_id[qp->ugid_index]);
		else
			req.sgid_index = cpu_to_le16(res->sgid_tbl.hw_id[qp->ah.sgid_index]);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_HOP_LIMIT) {
		req.hop_limit = qp->ah.hop_limit;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_TRAFFIC_CLASS) {
		req.traffic_class = qp->ah.traffic_class;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_DEST_MAC) {
		memcpy(req.dest_mac, qp->ah.dmac, 6);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_PATH_MTU) {
		req.path_mtu_pingpong_push_enable = qp->path_mtu;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_TIMEOUT) {
		req.timeout = qp->timeout;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_RETRY_CNT) {
		req.retry_cnt = qp->retry_cnt;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_RNR_RETRY) {
		req.rnr_retry = qp->rnr_retry;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_MIN_RNR_TIMER) {
		req.min_rnr_timer = qp->min_rnr_timer;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_RQ_PSN) {
		req.rq_psn = cpu_to_le32(qp->rq.psn);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_SQ_PSN) {
		req.sq_psn = cpu_to_le32(qp->sq.psn);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_MAX_RD_ATOMIC) {
		if (BNXT_RE_IQM(qp->dev_cap_flags))
			req.max_rd_atomic = qp->max_rd_atomic;
		else
			req.max_rd_atomic =
				(u8)min_t(u32, BNXT_QPLIB_MAX_OUT_RD_ATOM,
					  ORD_LIMIT_TO_ORRQ_SLOTS(qp->max_rd_atomic));
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_MAX_DEST_RD_ATOMIC) {
		if (BNXT_RE_IQM(qp->dev_cap_flags))
			req.max_dest_rd_atomic = qp->max_dest_rd_atomic;
		else
			req.max_dest_rd_atomic =
				(u8)min_t(u32, BNXT_QPLIB_MAX_IN_RD_ATOM,
					  IRD_LIMIT_TO_IRRQ_SLOTS(qp->max_dest_rd_atomic));
	}

	req.sq_size = cpu_to_le32(qp->sq.hwq.depth);
	req.rq_size = cpu_to_le32(qp->rq.hwq.depth);
	req.sq_sge = cpu_to_le16(qp->sq.max_sge);
	req.rq_sge = cpu_to_le16(qp->rq.max_sge);
	req.max_inline_data = cpu_to_le32(qp->max_inline_data);
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_DEST_QP_ID)
		req.dest_qp_id = cpu_to_le32(qp->dest_qpn);
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_ENABLE_CC)
		req.enable_cc = cpu_to_le16(CMDQ_MODIFY_QP_ENABLE_CC);
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_TOS_ECN)
		req.tos_dscp_tos_ecn =
			((qp->tos_ecn << CMDQ_MODIFY_QP_TOS_ECN_SFT) &
			 CMDQ_MODIFY_QP_TOS_ECN_MASK);
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_TOS_DSCP)
		req.tos_dscp_tos_ecn |=
			((qp->tos_dscp << CMDQ_MODIFY_QP_TOS_DSCP_SFT) &
			 CMDQ_MODIFY_QP_TOS_DSCP_MASK);
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_VLAN_ID) {
		req.vlan_pcp_vlan_dei_vlan_id =
			((res->sgid_tbl.tbl[qp->ah.sgid_index].vlan_id <<
			 CMDQ_MODIFY_QP_VLAN_ID_SFT) &
			 CMDQ_MODIFY_QP_VLAN_ID_MASK);
		req.vlan_pcp_vlan_dei_vlan_id |=
			((qp->ah.sl << CMDQ_MODIFY_QP_VLAN_PCP_SFT) &
			 CMDQ_MODIFY_QP_VLAN_PCP_MASK);
		req.vlan_pcp_vlan_dei_vlan_id = cpu_to_le16(req.vlan_pcp_vlan_dei_vlan_id);
	}
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	msg.qp_state = qp->state;
	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_MODIFY_QP,
				 sizeof(req));
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc == -ETIMEDOUT && (qp->state == CMDQ_MODIFY_QP_NEW_STATE_ERR)) {
		qp->cur_qp_state = qp->state;
		return 0;
	} else if (rc) {
		return rc;
	}

	if (resp.flags & CREQ_MODIFY_QP_RESP_SESSION_ELIGIBLE)
		bnxt_qplib_create_roceflow(res, qp);

	if (ppp_requested)
		qp->ppp.st_idx_en = resp.pingpong_push_state_index_enabled;

	if (bmask_ext & CMDQ_MODIFY_QP_EXT_MODIFY_MASK_RATE_LIMIT_VALID)
		qp->shaper_allocation_status = resp.shaper_allocation_status;

	qp->cur_qp_state = qp->state;
	return 0;
}

int bnxt_qplib_query_qp(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_query_qp_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct bnxt_qplib_rcfw_sbuf sbuf;
	struct creq_query_qp_resp_sb *sb;
	struct cmdq_query_qp req = {};
	u32 temp32[4];
	int i, rc;

	sbuf.size = ALIGN(sizeof(*sb), BNXT_QPLIB_CMDQE_UNITS);
	sbuf.sb = dma_zalloc_coherent(&rcfw->pdev->dev, sbuf.size,
				       &sbuf.dma_addr, GFP_KERNEL);
	if (!sbuf.sb)
		return -ENOMEM;
	sb = sbuf.sb;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_QUERY_QP,
				 sizeof(req));
	req.qp_cid = cpu_to_le32(qp->id);
	req.resp_size = sbuf.size / BNXT_QPLIB_CMDQE_UNITS;
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, &sbuf, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto bail;

	/* Extract the context from the side buffer */
	qp->state = sb->en_sqd_async_notify_state &
			CREQ_QUERY_QP_RESP_SB_STATE_MASK;
	qp->cur_qp_state = qp->state;
	qp->udp_sport = le16_to_cpu(sb->udp_src_port);
	qp->en_sqd_async_notify = sb->en_sqd_async_notify_state &
				  CREQ_QUERY_QP_RESP_SB_EN_SQD_ASYNC_NOTIFY ?
				  true : false;
	qp->access = sb->access;
	qp->pkey_index = le16_to_cpu(sb->pkey);
	qp->qkey = le32_to_cpu(sb->qkey);

	temp32[0] = le32_to_cpu(sb->dgid[0]);
	temp32[1] = le32_to_cpu(sb->dgid[1]);
	temp32[2] = le32_to_cpu(sb->dgid[2]);
	temp32[3] = le32_to_cpu(sb->dgid[3]);
	memcpy(qp->ah.dgid.data, temp32, sizeof(qp->ah.dgid.data));

	qp->ah.flow_label = le32_to_cpu(sb->flow_label);

	qp->ah.sgid_index = 0;
	for (i = 0; i < res->sgid_tbl.max; i++) {
		if (res->sgid_tbl.hw_id[i] == le16_to_cpu(sb->sgid_index)) {
			qp->ah.sgid_index = i;
			break;
		}
	}
	if (i == res->sgid_tbl.max)
		dev_dbg(&res->pdev->dev,
			"QPLIB: SGID not found qp->id = 0x%x sgid_index = 0x%x\n",
			qp->id, le16_to_cpu(sb->sgid_index));

	qp->ah.hop_limit = sb->hop_limit;
	qp->ah.traffic_class = sb->traffic_class;
	memcpy(qp->ah.dmac, sb->dest_mac, ETH_ALEN);
	qp->ah.vlan_id = le16_to_cpu(sb->path_mtu_dest_vlan_id) &
				CREQ_QUERY_QP_RESP_SB_VLAN_ID_MASK >>
				CREQ_QUERY_QP_RESP_SB_VLAN_ID_SFT;
	qp->path_mtu = le16_to_cpu(sb->path_mtu_dest_vlan_id) &
				    CREQ_QUERY_QP_RESP_SB_PATH_MTU_MASK;
	qp->timeout = sb->timeout;
	qp->retry_cnt = sb->retry_cnt;
	qp->rnr_retry = sb->rnr_retry;
	qp->min_rnr_timer = sb->min_rnr_timer;
	qp->rq.psn = le32_to_cpu(sb->rq_psn);
	qp->max_rd_atomic = BNXT_RE_IQM(res->dattr->dev_cap_flags) ?
		(sb->max_rd_atomic) :
		ORRQ_SLOTS_TO_ORD_LIMIT(sb->max_rd_atomic);
	qp->sq.psn = le32_to_cpu(sb->sq_psn);
	qp->max_dest_rd_atomic = BNXT_RE_IQM(res->dattr->dev_cap_flags) ?
		sb->max_dest_rd_atomic :
		IRRQ_SLOTS_TO_IRD_LIMIT(sb->max_dest_rd_atomic);
	qp->sq.max_wqe = qp->sq.hwq.max_elements;
	qp->rq.max_wqe = qp->rq.hwq.max_elements;
	qp->sq.max_sge = le16_to_cpu(sb->sq_sge);
	qp->rq.max_sge = le16_to_cpu(sb->rq_sge);
	qp->max_inline_data = le32_to_cpu(sb->max_inline_data);
	if (_is_chip_p7_plus(res->cctx))
		qp->max_inline_data = qp->sq.max_sge * sizeof(struct sq_sge);
	qp->dest_qpn = le32_to_cpu(sb->dest_qp_id);
	memcpy(qp->smac, sb->src_mac, ETH_ALEN);
	qp->vlan_id = le16_to_cpu(sb->vlan_pcp_vlan_dei_vlan_id) &
				CREQ_QUERY_QP_RESP_SB_VLAN_ID_MASK >>
				CREQ_QUERY_QP_RESP_SB_VLAN_ID_SFT;
	qp->ah.sl = le16_to_cpu(sb->vlan_pcp_vlan_dei_vlan_id) &
				CREQ_QUERY_QP_RESP_SB_VLAN_PCP_MASK >>
				CREQ_QUERY_QP_RESP_SB_VLAN_PCP_SFT;
	qp->port_id = le16_to_cpu(sb->port_id);
	qp->rate_limit = le32_to_cpu(sb->rate_limit);
bail:
	dma_free_coherent(&rcfw->pdev->dev, sbuf.size,
				  sbuf.sb, sbuf.dma_addr);
	return rc;
}

static void __clean_cq(struct bnxt_qplib_cq *cq, u64 qp)
{
	struct bnxt_qplib_hwq *cq_hwq = &cq->hwq;
	struct bnxt_qplib_chip_ctx *chip_ctx;
	u32 peek_flags, peek_cons;
	struct cq_base *hw_cqe;
	int i;

	peek_flags = cq->dbinfo.flags;
	peek_cons = cq_hwq->cons;
	chip_ctx = cq->cctx;

	for (i = 0; i < cq_hwq->depth; i++) {
		hw_cqe = bnxt_qplib_get_qe(cq_hwq, peek_cons, NULL);
		if (CQE_CMP_VALID(hw_cqe, peek_flags)) {
			dma_rmb();
			switch (hw_cqe->cqe_type_toggle & CQ_BASE_CQE_TYPE_MASK) {
			case CQ_BASE_CQE_TYPE_REQ:
			case CQ_BASE_CQE_TYPE_REQ_V3:
			case CQ_BASE_CQE_TYPE_TERMINAL:
			{
				if (_is_hsi_v3(chip_ctx)) {
					struct cq_req_v3 *cqe = (struct cq_req_v3 *)hw_cqe;

					if (qp == le64_to_cpu(cqe->qp_handle))
						cqe->qp_handle = 0;
				} else {
					struct cq_req *cqe = (struct cq_req *)hw_cqe;

					if (qp == le64_to_cpu(cqe->qp_handle))
						cqe->qp_handle = 0;
				}
				break;
			}
			case CQ_BASE_CQE_TYPE_RES_RC:
			case CQ_BASE_CQE_TYPE_RES_UD:
			case CQ_BASE_CQE_TYPE_RES_RAWETH_QP1:
			case CQ_BASE_CQE_TYPE_RES_UD_V3:
			{
				if (_is_hsi_v3(chip_ctx)) {
					struct cq_res_rc_v3 *cqe = (struct cq_res_rc_v3 *)hw_cqe;

					if (qp == le64_to_cpu(cqe->qp_handle))
						cqe->qp_handle = 0;
				} else {
					struct cq_res_rc *cqe = (struct cq_res_rc *)hw_cqe;

					if (qp == le64_to_cpu(cqe->qp_handle))
						cqe->qp_handle = 0;
				}
				break;
			}
			default:
				break;
			}
		}
		bnxt_qplib_hwq_incr_cons(cq_hwq->depth, &peek_cons,
					 1, &peek_flags);
	}
}

#ifdef ENABLE_FP_SPINLOCK
static unsigned long bnxt_qplib_lock_cqs(struct bnxt_qplib_qp *qp)
{
	unsigned long flags;

	spin_lock_irqsave(&qp->scq->hwq.lock, flags);
	if (qp->rcq && qp->rcq != qp->scq)
		spin_lock(&qp->rcq->hwq.lock);

	return flags;
}

static void bnxt_qplib_unlock_cqs(struct bnxt_qplib_qp *qp,
				  unsigned long flags)
{
	if (qp->rcq && qp->rcq != qp->scq)
		spin_unlock(&qp->rcq->hwq.lock);
	spin_unlock_irqrestore(&qp->scq->hwq.lock, flags);
}
#endif

int bnxt_qplib_destroy_qp(struct bnxt_qplib_res *res,
			  struct bnxt_qplib_qp *qp,
			  u16 ctx_size, void *ctx_sb_data, void *resp)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct bnxt_qplib_reftbl *tbl;
	unsigned long flags;
	u32 qp_idx;

	tbl = &res->reftbl.qpref;
	qp_idx = map_qp_id_to_tbl_indx(qp->id, tbl);
	spin_lock_irqsave(&tbl->lock, flags);
	tbl->rec[qp_idx].xid = BNXT_QPLIB_QP_ID_INVALID;
	tbl->rec[qp_idx].handle = NULL;
	spin_unlock_irqrestore(&tbl->lock, flags);

	bnxt_qplib_del_roceflow(res, qp);

	return __qplib_destroy_qp(rcfw, qp, ctx_size, ctx_sb_data, resp);
}

void bnxt_qplib_free_qp_res(struct bnxt_qplib_res *res,
			    struct bnxt_qplib_qp *qp)
{
	if (qp->irrq.max_elements)
		bnxt_qplib_free_hwq(res, &qp->irrq);
	if (qp->orrq.max_elements)
		bnxt_qplib_free_hwq(res, &qp->orrq);

	if (!qp->is_user)
		kfree(qp->rq.swq);
	bnxt_qplib_free_hwq(res, &qp->rq.hwq);

	if (!qp->is_user)
		kfree(qp->sq.swq);
	bnxt_qplib_free_hwq(res, &qp->sq.hwq);
}

void *bnxt_qplib_get_qp1_sq_buf(struct bnxt_qplib_qp *qp,
				struct bnxt_qplib_sge *sge)
{
	struct bnxt_qplib_q *sq = &qp->sq;
	struct bnxt_qplib_hdrbuf *buf;
	u32 sw_prod;

	memset(sge, 0, sizeof(*sge));

	buf = qp->sq_hdr_buf;
	if (buf) {
		sw_prod = sq->swq_start;
		sge->addr = (dma_addr_t)(buf->dma_map + sw_prod * buf->step);
		sge->lkey = 0xFFFFFFFF;
		sge->size = buf->step;
		return buf->va + sw_prod * sge->size;
	}
	return NULL;
}

u32 bnxt_qplib_get_rq_prod_index(struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_q *rq = &qp->rq;

	return rq->swq_start;
}

void *bnxt_qplib_get_qp1_rq_buf(struct bnxt_qplib_qp *qp,
				struct bnxt_qplib_sge *sge)
{
	struct bnxt_qplib_q *rq = &qp->rq;
	struct bnxt_qplib_hdrbuf *buf;
	u32 sw_prod;

	memset(sge, 0, sizeof(*sge));

	buf = qp->rq_hdr_buf;
	if (buf) {
		sw_prod = rq->swq_start;
		sge->addr = (dma_addr_t)(buf->dma_map + sw_prod * buf->step);
		sge->lkey = 0xFFFFFFFF;
		sge->size = buf->step;
		return buf->va + sw_prod * sge->size;
	}
	return NULL;
}

/* Fil the MSN table into the next psn row */
static void bnxt_qplib_fill_msn_search(struct bnxt_qplib_qp *qp,
				       struct bnxt_qplib_swqe *wqe,
				       struct bnxt_qplib_swq *swq)
{
	struct sq_msn_search *msns;
	u32 start_psn, next_psn;
	u16 start_idx;

	msns = (struct sq_msn_search *)swq->psn_search;
	msns->start_idx_next_psn_start_psn = 0;

	start_psn = swq->start_psn;
	next_psn = swq->next_psn;
	start_idx = swq->slot_idx;
	msns->start_idx_next_psn_start_psn |=
		bnxt_re_update_msn_tbl(start_idx, next_psn, start_psn);
	pr_debug("QP_LIB MSN %d START_IDX %u NEXT_PSN %u START_PSN %u",
		 qp->msn,
		 (u16)
		 cpu_to_le16(BNXT_RE_MSN_IDX(msns->start_idx_next_psn_start_psn)),
		 (u32)
		 cpu_to_le32(BNXT_RE_MSN_NPSN(msns->start_idx_next_psn_start_psn)),
		 (u32)
		 cpu_to_le32(BNXT_RE_MSN_SPSN(msns->start_idx_next_psn_start_psn)));
	qp->msn++;
	qp->msn %= qp->msn_tbl_sz;
}

static void bnxt_qplib_fill_psn_search(struct bnxt_qplib_qp *qp,
				       struct bnxt_qplib_swqe *wqe,
				       struct bnxt_qplib_swq *swq)
{
	struct sq_psn_search_ext *psns_ext;
	struct sq_psn_search *psns;
	u32 flg_npsn;
	u32 op_spsn;

	if (!swq->psn_search)
		return;

	/* Handle MSN differently on cap flags  */
	if (qp->is_host_msn_tbl) {
		bnxt_qplib_fill_msn_search(qp, wqe, swq);
		return;
	}
	psns = (struct sq_psn_search *)swq->psn_search;
	psns_ext = (struct sq_psn_search_ext *)swq->psn_search;

	op_spsn = ((swq->start_psn << SQ_PSN_SEARCH_START_PSN_SFT) &
		   SQ_PSN_SEARCH_START_PSN_MASK);
	op_spsn |= ((wqe->type << SQ_PSN_SEARCH_OPCODE_SFT) &
		    SQ_PSN_SEARCH_OPCODE_MASK);
	flg_npsn = ((swq->next_psn << SQ_PSN_SEARCH_NEXT_PSN_SFT) &
		    SQ_PSN_SEARCH_NEXT_PSN_MASK);

	if (_is_chip_gen_p5_p7(qp->cctx)) {
		psns_ext->opcode_start_psn = cpu_to_le32(op_spsn);
		psns_ext->flags_next_psn = cpu_to_le32(flg_npsn);
		psns_ext->start_slot_idx = cpu_to_le16(swq->slot_idx);
	} else {
		psns->opcode_start_psn = cpu_to_le32(op_spsn);
		psns->flags_next_psn = cpu_to_le32(flg_npsn);
	}
}

static u16 _calc_ilsize(struct bnxt_qplib_swqe *wqe)
{
	u16 size = 0;
	int indx;

	for (indx = 0; indx < wqe->num_sge; indx++)
		size += wqe->sg_list[indx].size;
	return size;
}

static unsigned int bnxt_qplib_put_inline(struct bnxt_qplib_qp *qp,
					  struct bnxt_qplib_swqe *wqe,
					  u32 *sw_prod)
{
	struct bnxt_qplib_hwq *sq_hwq;
	int len, t_len, offt = 0;
	int t_cplen = 0, cplen;
	bool pull_dst = true;
	void *il_dst = NULL;
	void *il_src = NULL;
	int indx;

	sq_hwq = &qp->sq.hwq;
	t_len = 0;
	for (indx = 0; indx < wqe->num_sge; indx++) {
		len = wqe->sg_list[indx].size;
		il_src = (void *)wqe->sg_list[indx].addr;
		t_len += len;
		if (t_len > qp->max_inline_data)
			return BNXT_RE_INVAL_MSG_SIZE;
		while (len) {
			if (pull_dst) {
				pull_dst = false;
				il_dst = bnxt_qplib_get_qe(sq_hwq, ((*sw_prod) %
							   sq_hwq->depth), NULL);
				(*sw_prod)++;
				t_cplen = 0;
				offt = 0;
			}
			cplen = min_t(int, len, sizeof(struct sq_sge));
			cplen = min_t(int, cplen,
				      (sizeof(struct sq_sge) - offt));
			memcpy(il_dst, il_src, cplen);
			t_cplen += cplen;
			il_src += cplen;
			il_dst += cplen;
			offt += cplen;
			len -= cplen;
			if (t_cplen == sizeof(struct sq_sge))
				pull_dst = true;
		}
	}

	return t_len;
}

static unsigned int bnxt_qplib_put_sges(struct bnxt_qplib_hwq *sq_hwq,
					struct bnxt_qplib_sge *ssge,
					u32 nsge, u32 *sw_prod)
{
	struct sq_sge *dsge;
	int indx, len = 0;

	for (indx = 0; indx < nsge; indx++, (*sw_prod)++) {
		dsge = bnxt_qplib_get_qe(sq_hwq, ((*sw_prod) % sq_hwq->depth), NULL);
		dsge->va_or_pa = cpu_to_le64(ssge[indx].addr);
		dsge->l_key = cpu_to_le32(ssge[indx].lkey);
		dsge->size = cpu_to_le32(ssge[indx].size);
		len += ssge[indx].size;
#ifdef ENABLE_DEBUG_SGE
		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: va/pa=0x%llx lkey=0x%x size=0x%x",
			dsge->va_or_pa, dsge->l_key, dsge->size);
#endif

	}
	return len;
}

static u16 _calculate_wqe_byte(struct bnxt_qplib_qp *qp,
			       struct bnxt_qplib_swqe *wqe, u16 *wqe_byte,
			       bool compact, bool is_sq)
{
	u16 hdr_size;
	u16 wqe_size;
	u32 ilsize;
	u16 nsge;

	if (_is_hsi_v3(qp->cctx)) {
		if (is_sq)
			hdr_size = (compact) ? sizeof(struct sq_send_hdr_v3) :
					       sizeof(struct sq_udsend_hdr_v3);
		else
			hdr_size = sizeof(struct rq_wqe_hdr_v3);
	} else {
		if (is_sq)
			hdr_size = sizeof(struct sq_send_hdr);
		else
			hdr_size = sizeof(struct rq_wqe_hdr);
	}

	nsge = wqe->num_sge;

	if (wqe->flags & BNXT_QPLIB_SWQE_FLAGS_INLINE) {
		ilsize = _calc_ilsize(wqe);
		wqe_size = (ilsize > qp->max_inline_data) ?
			    qp->max_inline_data : ilsize;
		wqe_size = ALIGN(wqe_size, sizeof(struct sq_sge));
	} else {
		wqe_size = nsge * sizeof(struct sq_sge);
	}

	/* Adding sq_send_hdr is a misnomer, for rq also hdr size is same. */
	wqe_size += hdr_size;
	if (wqe_byte)
		*wqe_byte = wqe_size;
	return wqe_size / sizeof(struct sq_sge);
}

static u16 _translate_q_full_delta(struct bnxt_qplib_q *que, u16 wqe_bytes)
{
	/* For Cu/Wh delta = 128, stride = 16, wqe_bytes = 128
	 * For Gen-p5 B/C mode delta = 0, stride = 16, wqe_bytes = 128.
	 * For Gen-p5 delta = 0, stride = 16, 32 <= wqe_bytes <= 512.
	 * when 8916 is disabled.
	 */
	return (que->q_full_delta * wqe_bytes) / que->hwq.element_size;
}

static void bnxt_qplib_pull_psn_buff(struct bnxt_qplib_qp *qp, struct bnxt_qplib_q *sq,
				     struct bnxt_qplib_swq *swq, bool is_host_msn_tbl)
{
	struct bnxt_qplib_hwq *sq_hwq;
	u32 pg_num, pg_indx;
	void *buff;
	u32 tail;

	sq_hwq = &sq->hwq;
	if (!sq_hwq->pad_pg)
		return;

	tail = swq->slot_idx / sq->dbinfo.max_slot;
	if (is_host_msn_tbl) {
		/* For HW retx use qp msn index */
		tail = qp->msn;
		tail %= qp->msn_tbl_sz;
	}
	pg_num = (tail + sq_hwq->pad_pgofft) / (PAGE_SIZE / sq_hwq->pad_stride);
	pg_indx = (tail + sq_hwq->pad_pgofft) % (PAGE_SIZE / sq_hwq->pad_stride);
	buff = (void *)(sq_hwq->pad_pg[pg_num] + pg_indx * sq_hwq->pad_stride);
	/* the start ptr for buff is same ie after the SQ */
	swq->psn_search = buff;
}

void bnxt_qplib_post_send_db(struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_q *sq = &qp->sq;

	bnxt_qplib_ring_prod_db(&sq->dbinfo, DBC_DBC_TYPE_SQ);
}

int bnxt_qplib_post_send(struct bnxt_qplib_qp *qp,
			 struct bnxt_qplib_swqe *wqe,
			 void *ah_in)
{
	struct bnxt_qplib_nq_work *nq_work = NULL;
	int i, rc = 0, data_len = 0, pkt_num = 0;
	struct bnxt_qplib_q *sq = &qp->sq;
	struct bnxt_qplib_hwq *sq_hwq;
	struct bnxt_qplib_swq *swq;
	bool sch_handler = false;
	struct bnxt_re_ah *ah;
#ifdef ENABLE_FP_SPINLOCK
	unsigned long flags;
#endif
	u16 slots_needed;
	void *base_hdr;
	bool msn_update;
	void *ext_hdr;
	__le32 temp32;
	u16 qfd_slots;
	u8 wqe_slots;
	u16 wqe_size;
	u32 sw_prod;
	u32 wqe_idx;

	sq_hwq = &sq->hwq;
#ifdef ENABLE_FP_SPINLOCK
	spin_lock_irqsave(&sq_hwq->lock, flags);
#endif

	if (qp->state != CMDQ_MODIFY_QP_NEW_STATE_RTS &&
	    qp->state != CMDQ_MODIFY_QP_NEW_STATE_ERR) {
		dev_err(&sq_hwq->pdev->dev,
			"QPLIB: FP: QP (0x%x) is in the 0x%x state",
			qp->id, qp->state);
		rc = -EINVAL;
		goto done;
	}

	wqe_slots = _calculate_wqe_byte(qp, wqe, &wqe_size, false, true);
	slots_needed = (qp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC) ?
			sq->dbinfo.max_slot : wqe_slots;
	qfd_slots = _translate_q_full_delta(sq, wqe_size);
	if (bnxt_qplib_queue_full(sq_hwq, (slots_needed + qfd_slots))) {
		dev_err(&sq_hwq->pdev->dev,
			"QPLIB: FP: QP (0x%x) SQ is full!", qp->id);
		dev_err(&sq_hwq->pdev->dev,
			"QPLIB: prod = %#x cons = %#x qdepth = %#x delta = %#x slots = %#x",
			HWQ_CMP(sq_hwq->prod, sq_hwq),
			HWQ_CMP(sq_hwq->cons, sq_hwq),
			sq_hwq->max_elements, qfd_slots, slots_needed);
		dev_err(&sq_hwq->pdev->dev,
			"QPLIB: phantom_wqe_cnt: %d phantom_cqe_cnt: %d\n",
			sq->phantom_wqe_cnt, sq->phantom_cqe_cnt);
		rc = -ENOMEM;
		goto done;
	}

	sw_prod = sq_hwq->prod;
	swq = bnxt_qplib_get_swqe(sq, &wqe_idx);
	swq->slot_idx = sw_prod;
	bnxt_qplib_pull_psn_buff(qp, sq, swq, qp->is_host_msn_tbl);

	swq->wr_id = wqe->wr_id;
	swq->type = wqe->type;
	swq->flags = wqe->flags;
	swq->slots = slots_needed;
	swq->start_psn = sq->psn & BTH_PSN_MASK;
	if (qp->sig_type || wqe->flags & BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP)
		swq->flags |= SQ_SEND_FLAGS_SIGNAL_COMP;

	dev_dbg(&sq_hwq->pdev->dev,
		"QPLIB: FP: QP(0x%x) post SQ wr_id[%d] = 0x%llx",
		qp->id, wqe_idx, swq->wr_id);
	if (qp->cur_qp_state == CMDQ_MODIFY_QP_NEW_STATE_ERR) {
		sch_handler = true;
		dev_dbg(&sq_hwq->pdev->dev,
			"%s Error QP. Scheduling for poll_cq\n", __func__);
		goto queue_err;
	}

	base_hdr = bnxt_qplib_get_qe(sq_hwq, sw_prod, NULL);
	sw_prod++;
	ext_hdr = bnxt_qplib_get_qe(sq_hwq, (sw_prod % sq_hwq->depth), NULL);
	sw_prod++;
	memset(base_hdr, 0, sizeof(struct sq_sge));
	memset(ext_hdr, 0, sizeof(struct sq_sge));

	if (wqe->flags & BNXT_QPLIB_SWQE_FLAGS_INLINE)
		data_len = bnxt_qplib_put_inline(qp, wqe, &sw_prod);
	else
		data_len = bnxt_qplib_put_sges(sq_hwq, wqe->sg_list,
					       wqe->num_sge, &sw_prod);
	if (data_len > BNXT_RE_MAX_MSG_SIZE) {
		rc = -EINVAL;
		goto done;
	}
	/* Make sure we update MSN table only for wired wqes */
	msn_update = true;
	/* Specifics */
	switch (wqe->type) {
	case BNXT_QPLIB_SWQE_TYPE_SEND:
		if (qp->type == CMDQ_CREATE_QP_TYPE_RAW_ETHERTYPE ||
		    qp->type == CMDQ_CREATE_QP1_TYPE_GSI) {
			/* Assemble info for Raw Ethertype QPs */
			struct sq_send_raweth_qp1_hdr *sqe = base_hdr;
			struct sq_raw_ext_hdr *ext_sqe = ext_hdr;

			sqe->wqe_type = wqe->type;
			sqe->flags = wqe->flags;
			sqe->wqe_size = wqe_slots;
			sqe->cfa_action = cpu_to_le16(wqe->rawqp1.cfa_action);
			sqe->lflags = cpu_to_le16(wqe->rawqp1.lflags);
			sqe->length = cpu_to_le32(data_len);
			ext_sqe->cfa_meta = cpu_to_le32((wqe->rawqp1.cfa_meta &
				SQ_SEND_RAWETH_QP1_CFA_META_VLAN_VID_MASK) <<
				SQ_SEND_RAWETH_QP1_CFA_META_VLAN_VID_SFT);

			dev_dbg(&sq_hwq->pdev->dev,
				"QPLIB: FP: RAW/QP1 Send WQE:\n"
				"\twqe_type = 0x%x\n"
				"\tflags = 0x%x\n"
				"\twqe_size = 0x%x\n"
				"\tlflags = 0x%x\n"
				"\tcfa_action = 0x%x\n"
				"\tlength = 0x%x\n"
				"\tcfa_meta = 0x%x",
				sqe->wqe_type, sqe->flags, sqe->wqe_size,
				sqe->lflags, sqe->cfa_action,
				sqe->length, ext_sqe->cfa_meta);
			break;
		}
		fallthrough;
	case BNXT_QPLIB_SWQE_TYPE_SEND_WITH_IMM:
		fallthrough;
	case BNXT_QPLIB_SWQE_TYPE_SEND_WITH_INV:
	{
		struct sq_send_hdr *sqe = base_hdr;
		struct sq_ud_ext_hdr *ext_sqe = ext_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->wqe_size = wqe_slots;
		sqe->inv_key_or_imm_data = cpu_to_le32(wqe->send.inv_key);
		if (qp->type == CMDQ_CREATE_QP_TYPE_UD ||
		    qp->type == CMDQ_CREATE_QP_TYPE_GSI) {
			sqe->q_key = cpu_to_le32(wqe->send.q_key);
			sqe->length = cpu_to_le32(data_len);
			ext_sqe->dst_qp = cpu_to_le32(
					wqe->send.dst_qp & SQ_SEND_DST_QP_MASK);
			ext_sqe->avid = cpu_to_le32(wqe->send.avid &
						SQ_SEND_AVID_MASK);
			sq->psn = (sq->psn + 1) & BTH_PSN_MASK;
			msn_update = false;
		} else {
			sqe->length = cpu_to_le32(data_len);
			if (qp->mtu)
				pkt_num = (data_len + qp->mtu - 1) / qp->mtu;
			if (!pkt_num)
				pkt_num = 1;
			sq->psn = (sq->psn + pkt_num) & BTH_PSN_MASK;
		}

		if (ah_in) {
			ah = (struct bnxt_re_ah *)ah_in;
			refcount_inc(ah->ref_cnt);
			swq->ah_ref_cnt = ah->ref_cnt;
		}

		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: Send WQE:\n"
			"\twqe_type = 0x%x\n"
			"\tflags = 0x%x\n"
			"\twqe_size = 0x%x\n"
			"\tinv_key/immdata = 0x%x\n"
			"\tq_key = 0x%x\n"
			"\tdst_qp = 0x%x\n"
			"\tlength = 0x%x\n"
			"\tavid = 0x%x",
			sqe->wqe_type, sqe->flags, sqe->wqe_size,
			sqe->inv_key_or_imm_data, sqe->q_key, ext_sqe->dst_qp,
			sqe->length, ext_sqe->avid);
		break;
	}
	case BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE:
		/* fall-thru */
	case BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE_WITH_IMM:
		/* fall-thru */
	case BNXT_QPLIB_SWQE_TYPE_RDMA_READ:
	{
		struct sq_rdma_hdr *sqe = base_hdr;
		struct sq_rdma_ext_hdr *ext_sqe = ext_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->wqe_size = wqe_slots;
		sqe->imm_data = cpu_to_le32(wqe->rdma.inv_key);
		sqe->length = cpu_to_le32((u32)data_len);
		ext_sqe->remote_va = cpu_to_le64(wqe->rdma.remote_va);
		ext_sqe->remote_key = cpu_to_le32(wqe->rdma.r_key);
		if (qp->mtu)
			pkt_num = (data_len + qp->mtu - 1) / qp->mtu;
		if (!pkt_num)
			pkt_num = 1;
		sq->psn = (sq->psn + pkt_num) & BTH_PSN_MASK;

		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: RDMA WQE:\n"
			"\twqe_type = 0x%x\n"
			"\tflags = 0x%x\n"
			"\twqe_size = 0x%x\n"
			"\timmdata = 0x%x\n"
			"\tlength = 0x%x\n"
			"\tremote_va = 0x%llx\n"
			"\tremote_key = 0x%x",
			sqe->wqe_type, sqe->flags, sqe->wqe_size,
			sqe->imm_data, sqe->length, ext_sqe->remote_va,
			ext_sqe->remote_key);
		break;
	}
	case BNXT_QPLIB_SWQE_TYPE_ATOMIC_CMP_AND_SWP:
		/* fall-thru */
	case BNXT_QPLIB_SWQE_TYPE_ATOMIC_FETCH_AND_ADD:
	{
		struct sq_atomic_hdr *sqe = base_hdr;
		struct sq_atomic_ext_hdr *ext_sqe = ext_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->remote_key = cpu_to_le32(wqe->atomic.r_key);
		sqe->remote_va = cpu_to_le64(wqe->atomic.remote_va);
		ext_sqe->swap_data = cpu_to_le64(wqe->atomic.swap_data);
		ext_sqe->cmp_data = cpu_to_le64(wqe->atomic.cmp_data);
		if (qp->mtu)
			pkt_num = (data_len + qp->mtu - 1) / qp->mtu;
		if (!pkt_num)
			pkt_num = 1;
		sq->psn = (sq->psn + pkt_num) & BTH_PSN_MASK;
		break;
	}
	case BNXT_QPLIB_SWQE_TYPE_LOCAL_INV:
	{
		struct sq_localinvalidate_hdr *sqe = base_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->inv_l_key = cpu_to_le32(wqe->local_inv.inv_l_key);

		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: LOCAL INV WQE:\n"
			"\twqe_type = 0x%x\n"
			"\tflags = 0x%x\n"
			"\tinv_l_key = 0x%x",
			sqe->wqe_type, sqe->flags, sqe->inv_l_key);
		msn_update = false;
		break;
	}
	case BNXT_QPLIB_SWQE_TYPE_FAST_REG_MR:
	{
		struct sq_fr_pmr_hdr *sqe = base_hdr;
		struct sq_fr_pmr_ext_hdr *ext_sqe = ext_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->access_cntl = wqe->frmr.access_cntl |
				   SQ_FR_PMR_ACCESS_CNTL_LOCAL_WRITE;
		sqe->zero_based_page_size_log =
			(wqe->frmr.pg_sz_log & SQ_FR_PMR_PAGE_SIZE_LOG_MASK) <<
			SQ_FR_PMR_PAGE_SIZE_LOG_SFT |
			(wqe->frmr.zero_based == true ? SQ_FR_PMR_ZERO_BASED : 0);
		sqe->l_key = cpu_to_le32(wqe->frmr.l_key);
		/* TODO: OFED only provides length of MR up to 32-bits for FRMR */
		temp32 = cpu_to_le32(wqe->frmr.length);
		memcpy(sqe->length, &temp32, sizeof(wqe->frmr.length));
		sqe->numlevels_pbl_page_size_log =
			((wqe->frmr.pbl_pg_sz_log <<
					SQ_FR_PMR_PBL_PAGE_SIZE_LOG_SFT) &
					SQ_FR_PMR_PBL_PAGE_SIZE_LOG_MASK) |
			((wqe->frmr.levels << SQ_FR_PMR_NUMLEVELS_SFT) &
					SQ_FR_PMR_NUMLEVELS_MASK);
		if (!wqe->frmr.levels && !wqe->frmr.pbl_ptr) {
			ext_sqe->pblptr = cpu_to_le64(wqe->frmr.page_list[0]);
		} else {
			for (i = 0; i < wqe->frmr.page_list_len; i++)
				wqe->frmr.pbl_ptr[i] = cpu_to_le64(
						wqe->frmr.page_list[i] |
						PTU_PTE_VALID);
			ext_sqe->pblptr = cpu_to_le64(wqe->frmr.pbl_dma_ptr);
		}
		ext_sqe->va = cpu_to_le64(wqe->frmr.va);
		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: FRMR WQE:\n"
			"\twqe_type = 0x%x\n"
			"\tflags = 0x%x\n"
			"\taccess_cntl = 0x%x\n"
			"\tzero_based_page_size_log = 0x%x\n"
			"\tl_key = 0x%x\n"
			"\tlength = 0x%x\n"
			"\tnumlevels_pbl_page_size_log = 0x%x\n"
			"\tpblptr = 0x%llx\n"
			"\tva = 0x%llx",
			sqe->wqe_type, sqe->flags, sqe->access_cntl,
			sqe->zero_based_page_size_log, sqe->l_key,
			*(u32 *)sqe->length, sqe->numlevels_pbl_page_size_log,
			ext_sqe->pblptr, ext_sqe->va);
		msn_update = false;
		break;
	}
	case BNXT_QPLIB_SWQE_TYPE_BIND_MW:
	{
		struct sq_bind_hdr *sqe = base_hdr;
		struct sq_bind_ext_hdr *ext_sqe = ext_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->access_cntl = wqe->bind.access_cntl;
		sqe->mw_type_zero_based = wqe->bind.mw_type |
			(wqe->bind.zero_based == true ? SQ_BIND_ZERO_BASED : 0);
		sqe->parent_l_key = cpu_to_le32(wqe->bind.parent_l_key);
		sqe->l_key = cpu_to_le32(wqe->bind.r_key);
		ext_sqe->va = cpu_to_le64(wqe->bind.va);
		ext_sqe->length_lo = cpu_to_le32(wqe->bind.length);
		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: BIND WQE:\n"
			"\twqe_type = 0x%x\n"
			"\tflags = 0x%x\n"
			"\taccess_cntl = 0x%x\n"
			"\tmw_type_zero_based = 0x%x\n"
			"\tparent_l_key = 0x%x\n"
			"\tl_key = 0x%x\n"
			"\tva = 0x%llx\n"
			"\tlength = 0x%x",
			sqe->wqe_type, sqe->flags, sqe->access_cntl,
			sqe->mw_type_zero_based, sqe->parent_l_key,
			sqe->l_key, sqe->va, ext_sqe->length_lo);
		msn_update = false;
		break;
	}
	case BNXT_QPLIB_SWQE_TYPE_SEND_UD_V3:
	{
		struct sq_udsend_hdr_v3 *sqe = base_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->wqe_size = wqe_slots & SQ_UDSEND_V3_WQE_SIZE_MASK;
		if (sqe->flags & SQ_UDSEND_HDR_V3_FLAGS_INLINE)
			/*
			 * When inline flag is '1', this field determines the number of bytes
			 * that are valid in the last 16B unit of the inline WQE.
			 * Zero means all 16 bytes are valid.
			 * One means only bits 7:0 of the last 16B unit are valid.
			 */
			sqe->inline_length = (data_len % 16);

		sqe->opaque = cpu_to_le32(wqe_idx);

		if (wqe->type == BNXT_QPLIB_SWQE_TYPE_SEND_UD_WITH_IMM_V3)
			sqe->imm_data = cpu_to_le32(wqe->udsend_v3.imm_data);

		sqe->q_key = cpu_to_le32(wqe->udsend_v3.q_key);
		sqe->dst_qp = cpu_to_le32(wqe->udsend_v3.dst_qp & SQ_UDSEND_V3_DST_QP_MASK);
		sqe->avid = cpu_to_le32(wqe->udsend_v3.avid & SQ_UDSEND_V3_AVID_MASK);

		if (sqe->flags & SQ_UDSEND_HDR_V3_FLAGS_WQE_TS_EN)
			sqe->timestamp = sq->psn & BTH_PSN_MASK;
		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: UDSend V3 WQE:\n"
			"\twqe_type = 0x%x\n"
			"\tflags = 0x%x\n"
			"\twqe_size = 0x%x\n"
			"\tinline_len = 0x%x\n"
			"\topaque = 0x%x\n"
			"\timm_data = 0x%x\n"
			"\tq_key = 0x%x\n"
			"\tdst_qp = 0x%x\n"
			"\tavid = 0x%x",
			sqe->wqe_type, sqe->flags, sqe->wqe_size, sqe->inline_length,
			sqe->opaque, sqe->imm_data, sqe->q_key, sqe->dst_qp, sqe->avid);
		msn_update = false;
		break;
	}
	default:
		/* Bad wqe, return error */
		rc = -EINVAL;
		goto done;
	}
	/*
	 * Ensure we update MSN table only for wired WQEs only.
	 * Free entry for all other NICS psn, psn_ext
	 */
	if (!qp->is_host_msn_tbl || msn_update) {
		swq->next_psn = sq->psn & BTH_PSN_MASK;
		bnxt_qplib_fill_psn_search(qp, wqe, swq);
	}

#ifdef ENABLE_DEBUG_SGE
	for (i = 0, hw_sge = (struct sq_sge *)hw_sq_send_hdr->data;
	     i < wqe->num_sge; i++, hw_sge++)
		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: va/pa=0x%llx lkey=0x%x size=0x%x",
			hw_sge->va_or_pa, hw_sge->l_key, hw_sge->size);
#endif
queue_err:
	bnxt_qplib_swq_mod_start(sq, wqe_idx);
	bnxt_qplib_hwq_incr_prod(&sq->dbinfo, sq_hwq, swq->slots);
	qp->wqe_cnt++;
done:
#ifdef ENABLE_FP_SPINLOCK
	spin_unlock_irqrestore(&sq_hwq->lock, flags);
#endif

	if (sch_handler) {
		nq_work = kzalloc(sizeof(*nq_work), GFP_ATOMIC);
		if (nq_work) {
			nq_work->cq = qp->scq;
			nq_work->nq = qp->scq->nq;
			INIT_WORK(&nq_work->work, bnxt_qpn_cqn_sched_task);
			queue_work(qp->scq->nq->cqn_wq, &nq_work->work);
		} else {
			dev_err(&sq->hwq.pdev->dev,
				"QPLIB: FP: Failed to allocate SQ nq_work!");
			rc = -ENOMEM;
		}
	}
	return rc;
}

void bnxt_qplib_post_recv_db(struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_q *rq = &qp->rq;

	if (unlikely(qp->cur_qp_state != CMDQ_MODIFY_QP_NEW_STATE_INIT))
		bnxt_qplib_ring_prod_db(&rq->dbinfo, DBC_DBC_TYPE_RQ);
}

void bnxt_re_handle_cqn(struct bnxt_qplib_cq *cq)
{
	struct bnxt_qplib_nq *nq;

	if (!(cq && cq->nq))
		return;

	nq = cq->nq;
	spin_lock_bh(&cq->compl_lock);
	if (nq->cqn_handler) {
		dev_dbg(&nq->res->pdev->dev,
			"%s:Trigger cq  = %p event nq = %p\n",
			__func__, cq, nq);
		nq->cqn_handler(nq, cq);
	}
	spin_unlock_bh(&cq->compl_lock);
}

int bnxt_qplib_post_recv(struct bnxt_qplib_qp *qp,
			 struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_qplib_nq_work *nq_work = NULL;
	struct bnxt_qplib_q *rq = &qp->rq;
	struct rq_wqe_hdr_v3 *base_hdr_v3;
	struct bnxt_qplib_hwq *rq_hwq;
	struct bnxt_qplib_swq *swq;
	bool sch_handler = false;
	struct rq_wqe_hdr *base_hdr;
	struct rq_ext_hdr *ext_hdr;
	struct sq_sge *dsge;
#ifdef ENABLE_FP_SPINLOCK
	unsigned long flags;
#endif
	u8 wqe_slots;
	u32 wqe_idx;
	u32 sw_prod;
	int rc = 0;
	bool is_v3;

	is_v3 = _is_hsi_v3(qp->cctx);
	rq_hwq = &rq->hwq;
#ifdef ENABLE_FP_SPINLOCK
	spin_lock_irqsave(&rq_hwq->lock, flags);
#endif
	if (qp->state == CMDQ_MODIFY_QP_NEW_STATE_RESET) {
		dev_err(&rq_hwq->pdev->dev,
			"QPLIB: FP: QP (0x%x) is in the 0x%x state",
			qp->id, qp->state);
		rc = -EINVAL;
		goto done;
	}

	wqe_slots = _calculate_wqe_byte(qp, wqe, NULL, false, false);
	if (bnxt_qplib_queue_full(rq_hwq, rq->dbinfo.max_slot)) {
		dev_err(&rq_hwq->pdev->dev,
			"QPLIB: FP: QP (0x%x) RQ is full!", qp->id);
		rc = -EINVAL;
		goto done;
	}

	swq = bnxt_qplib_get_swqe(rq, &wqe_idx);
	swq->wr_id = wqe->wr_id;
	swq->slots = rq->dbinfo.max_slot;
	dev_dbg(&rq_hwq->pdev->dev,
		"QPLIB: FP: post RQ wr_id[%d] = 0x%llx",
		wqe_idx, swq->wr_id);
	if (qp->cur_qp_state == CMDQ_MODIFY_QP_NEW_STATE_ERR) {
		sch_handler = true;
		dev_dbg(&rq_hwq->pdev->dev, "%s Error QP. Sched a flushed cmpl\n",
			__func__);
		goto queue_err;
	}

	sw_prod = rq_hwq->prod;
	if (is_v3) {
		base_hdr_v3 = bnxt_qplib_get_qe(rq_hwq, sw_prod, NULL);
		sw_prod++;
		memset(base_hdr_v3, 0, sizeof(struct rq_wqe_hdr_v3));
	} else {
		base_hdr = bnxt_qplib_get_qe(rq_hwq, sw_prod, NULL);
		sw_prod++;
		ext_hdr = bnxt_qplib_get_qe(rq_hwq, (sw_prod % rq_hwq->depth), NULL);
		sw_prod++;
		memset(base_hdr, 0, sizeof(struct sq_sge));
		memset(ext_hdr, 0, sizeof(struct sq_sge));
	}

	if (!wqe->num_sge) {
		dsge = bnxt_qplib_get_qe(rq_hwq, (sw_prod % rq_hwq->depth), NULL);
		dsge->size = 0;
		wqe_slots++;
	} else {
		bnxt_qplib_put_sges(rq_hwq, wqe->sg_list, wqe->num_sge, &sw_prod);
	}

	if (is_v3) {
		base_hdr_v3->wqe_type = wqe->type;
		base_hdr_v3->flags = 0;
		base_hdr_v3->wqe_size = wqe_slots;
		base_hdr_v3->opaque = cpu_to_le32(wqe_idx);
	} else {
		base_hdr->wqe_type = wqe->type;
		base_hdr->flags = wqe->flags;
		base_hdr->wqe_size = wqe_slots;
		base_hdr->wr_id[0] = cpu_to_le32(wqe_idx);
	}
queue_err:
	bnxt_qplib_swq_mod_start(rq, wqe_idx);
	bnxt_qplib_hwq_incr_prod(&rq->dbinfo, &rq->hwq, swq->slots);
done:
#ifdef ENABLE_FP_SPINLOCK
	spin_unlock_irqrestore(&rq->hwq.lock, flags);
#endif
	if (sch_handler) {
		nq_work = kzalloc(sizeof(*nq_work), GFP_ATOMIC);
		if (nq_work) {
			nq_work->cq = qp->rcq;
			nq_work->nq = qp->rcq->nq;
			INIT_WORK(&nq_work->work, bnxt_qpn_cqn_sched_task);
			queue_work(qp->rcq->nq->cqn_wq, &nq_work->work);
		} else {
			dev_err(&rq->hwq.pdev->dev,
				"QPLIB: FP: Failed to allocate RQ nq_work!");
			rc = -ENOMEM;
		}
	}
	return rc;
}

/* CQ */
int bnxt_qplib_create_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_create_cq_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_create_cq req = {};
	struct bnxt_qplib_reftbl *tbl;
	unsigned long flag;
	u32 coalescing = 0;
	u32 pg_sz_lvl = 0;
	u32 fwo = 0;
	int rc;

	if (!cq->dpi) {
		dev_err(&rcfw->pdev->dev,
			"QPLIB: FP: CREATE_CQ failed due to NULL DPI");
		return -EINVAL;
	}

	hwq_attr.res = res;
	hwq_attr.depth = cq->max_wqe;
	hwq_attr.stride = sizeof(struct cq_base);
	hwq_attr.type = HWQ_TYPE_QUEUE;
	hwq_attr.sginfo = &cq->sginfo;
	rc = bnxt_qplib_alloc_init_hwq(&cq->hwq, &hwq_attr);
	if (rc)
		return rc;

	if (res->cctx->modes.st_tag_supported) {
		req.flags |= cpu_to_le16(CMDQ_CREATE_CQ_FLAGS_STEERING_TAG_VALID);
		req.steering_tag = cpu_to_le16(BNXT_RE_STEERING_TO_HOST);
	}

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_CREATE_CQ,
				 sizeof(req));
	req.dpi = cpu_to_le32(cq->dpi->dpi);
	req.cq_handle = cpu_to_le64(cq->cq_handle);
	if (res->cctx->modes.hdbr_enabled &&
	    (!cq->overflow_telemetry &&
	     !_is_cq_overflow_detection_enabled(res->dattr->dev_cap_ext_flags2)) &&
	    cq->ignore_overrun)
		req.flags |=
			cpu_to_le16(CMDQ_CREATE_CQ_FLAGS_DISABLE_CQ_OVERFLOW_DETECTION);
	if (_is_cq_coalescing_supported(res->dattr->dev_cap_ext_flags2)) {
		req.flags |= cpu_to_le16(CMDQ_CREATE_CQ_FLAGS_COALESCING_VALID);
		coalescing |= ((cq->coalescing->buf_maxtime <<
				CMDQ_CREATE_CQ_BUF_MAXTIME_SFT) &
			       CMDQ_CREATE_CQ_BUF_MAXTIME_MASK);
		if (cq->coalescing->enable) {
			coalescing |= ((cq->coalescing->normal_maxbuf <<
				CMDQ_CREATE_CQ_NORMAL_MAXBUF_SFT) &
			       CMDQ_CREATE_CQ_NORMAL_MAXBUF_MASK);
			coalescing |= ((cq->coalescing->during_maxbuf <<
				CMDQ_CREATE_CQ_DURING_MAXBUF_SFT) &
			       CMDQ_CREATE_CQ_DURING_MAXBUF_MASK);
		}
		if (cq->coalescing->en_ring_idle_mode)
			coalescing |= CMDQ_CREATE_CQ_ENABLE_RING_IDLE_MODE;
		else
			coalescing &= ~CMDQ_CREATE_CQ_ENABLE_RING_IDLE_MODE;
		req.coalescing = cpu_to_le32(coalescing);
	}

	req.cq_size = cpu_to_le32(cq->max_wqe);
	req.pbl = cpu_to_le64(_get_base_addr(&cq->hwq));
	pg_sz_lvl = _get_base_pg_size(&cq->hwq) << CMDQ_CREATE_CQ_PG_SIZE_SFT;
	pg_sz_lvl |= ((cq->hwq.level & CMDQ_CREATE_CQ_LVL_MASK) <<
		       CMDQ_CREATE_CQ_LVL_SFT);
	req.pg_size_lvl = cpu_to_le32(pg_sz_lvl);
	if (bnxt_re_pbl_size_supported(res->dattr->dev_cap_ext_flags_1)) {
		req.flags |= cpu_to_le16(CMDQ_CREATE_CQ_FLAGS_PBL_PG_SIZE_VALID);
		req.pbl_pg_size = _get_pbl_page_size(&cq->sginfo);
	}

	fwo = hwq_attr.sginfo->fwo_offset;
	req.cq_fco_cnq_id = cpu_to_le32(
			((cq->cnq_hw_ring_id & CMDQ_CREATE_CQ_CNQ_ID_MASK) <<
			 CMDQ_CREATE_CQ_CNQ_ID_SFT) |
			(((fwo >> 5) << CMDQ_CREATE_CQ_CQ_FCO_SFT) & CMDQ_CREATE_CQ_CQ_FCO_MASK));

	if (hwq_attr.sginfo->pgsize != PAGE_SIZE)
		dev_dbg(&rcfw->pdev->dev,
			"CQ pgsize %d pgshft %d npages %d fwo %d shft 0x%x cq_fco_cnq_id 0x%x\n",
			hwq_attr.sginfo->pgsize, hwq_attr.sginfo->pgshft,
			hwq_attr.sginfo->npages, fwo, fwo >> 7, req.cq_fco_cnq_id);

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto fail;
	cq->id = le32_to_cpu(resp.xid);

	if (rcfw->roce_context_destroy_sb) {
		cq->ctx_size_sb = resp.context_size * BNXT_QPLIB_CMDQE_UNITS;
		if (!rcfw->cq_ctxm_data) {
			rcfw->cq_ctxm_data = vzalloc(CTXM_DATA_INDEX_MAX * cq->ctx_size_sb);
			rcfw->cq_ctxm_size = cq->ctx_size_sb;
		}
	}

	cq->period = BNXT_QPLIB_QUEUE_START_PERIOD;
	init_waitqueue_head(&cq->waitq);
	INIT_LIST_HEAD(&cq->sqf_head);
	INIT_LIST_HEAD(&cq->rqf_head);
	spin_lock_init(&cq->flush_lock);
	spin_lock_init(&cq->compl_lock);

	/* init dbinfo */
	cq->cctx = res->cctx;
	cq->dbinfo.hwq = &cq->hwq;
	cq->dbinfo.xid = cq->id;
	cq->dbinfo.db = cq->dpi->dbr;
	cq->dbinfo.priv_db = res->dpi_tbl.priv_db;
	cq->dbinfo.flags = 0;
	cq->dbinfo.toggle = 0;
	cq->dbinfo.res = res;
	cq->dbinfo.seed = cq->id;
	spin_lock_init(&cq->dbinfo.lock);
	cq->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
	cq->dbinfo.shadow_key_arm_ena = BNXT_QPLIB_DBR_KEY_INVALID;

	tbl = &res->reftbl.cqref;
	spin_lock_irqsave(&tbl->lock, flag);
	tbl->rec[GET_TBL_INDEX(cq->id, tbl)].xid = cq->id;
	tbl->rec[GET_TBL_INDEX(cq->id, tbl)].handle = cq;
	spin_unlock_irqrestore(&tbl->lock, flag);

	bnxt_qplib_armen_db(&cq->dbinfo, DBC_DBC_TYPE_CQ_ARMENA);
	return 0;

fail:
	bnxt_qplib_free_hwq(res, &cq->hwq);
	return rc;
}

int bnxt_qplib_modify_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq)
{
	/* TODO: Modify CQ threshold are passed to the HW via DBR */
	return 0;
}

void bnxt_qplib_resize_cq_complete(struct bnxt_qplib_res *res,
				   struct bnxt_qplib_cq *cq)
{
	bnxt_qplib_free_hwq(res, &cq->hwq);
	memcpy(&cq->hwq, &cq->resize_hwq, sizeof(cq->hwq));
	/* Reset only the cons bit in the flags */
	cq->dbinfo.flags &= ~(1UL << BNXT_QPLIB_FLAG_EPOCH_CONS_SHIFT);

	/* Tell HW to switch over to the new CQ */
	if (!cq->resize_hwq.is_user)
		bnxt_qplib_cq_coffack_db(&cq->dbinfo);
}

int bnxt_qplib_resize_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq,
			 int new_cqes)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_resize_cq_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_resize_cq req = {};
	u32 pgsz = 0, lvl = 0, nsz = 0;
	struct bnxt_qplib_pbl *pbl;
	u16 count = -1;
	u32 fco = 0;
	int rc;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_RESIZE_CQ,
				 sizeof(req));

	hwq_attr.sginfo = &cq->sginfo;
	hwq_attr.res = res;
	hwq_attr.depth = new_cqes;
	hwq_attr.stride = sizeof(struct cq_base);
	hwq_attr.type = HWQ_TYPE_QUEUE;
	rc = bnxt_qplib_alloc_init_hwq(&cq->resize_hwq, &hwq_attr);
	if (rc)
		return rc;

	dev_dbg(&rcfw->pdev->dev, "QPLIB: FP: %s: pbl_lvl: %d\n", __func__,
		cq->resize_hwq.level);
	req.cq_cid = cpu_to_le32(cq->id);
	pbl = &cq->resize_hwq.pbl[PBL_LVL_0];
	pgsz = _get_base_pg_size(&cq->resize_hwq) << CMDQ_RESIZE_CQ_PG_SIZE_SFT;
	lvl = (cq->resize_hwq.level << CMDQ_RESIZE_CQ_LVL_SFT) &
				       CMDQ_RESIZE_CQ_LVL_MASK;
	nsz = (new_cqes << CMDQ_RESIZE_CQ_NEW_CQ_SIZE_SFT) &
	       CMDQ_RESIZE_CQ_NEW_CQ_SIZE_MASK;
	req.new_cq_size_pg_size_lvl = cpu_to_le32(nsz|pgsz|lvl);
	req.new_pbl = cpu_to_le64(pbl->pg_map_arr[0]);

	if (bnxt_re_pbl_size_supported(res->dattr->dev_cap_ext_flags_1)) {
		req.flags |= cpu_to_le16(CMDQ_RESIZE_CQ_FLAGS_PBL_PG_SIZE_VALID);
		req.pbl_pg_size = _get_pbl_page_size(&cq->sginfo);
	}

	fco = hwq_attr.sginfo->fwo_offset;
	req.new_cq_fco = cpu_to_le32(((fco >> 5) << CMDQ_RESIZE_CQ_CQ_FCO_SFT) &
				     CMDQ_RESIZE_CQ_CQ_FCO_MASK);
	if (!cq->resize_hwq.is_user)
		set_bit(CQ_FLAGS_RESIZE_IN_PROG, &cq->flags);

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto fail;

	if (!cq->resize_hwq.is_user) {
wait:
		/* Wait here for the HW to switch the CQ over */
		if (wait_event_interruptible_timeout(cq->waitq,
		    !test_bit(CQ_FLAGS_RESIZE_IN_PROG, &cq->flags),
		    msecs_to_jiffies(CQ_RESIZE_WAIT_TIME_MS)) ==
		    -ERESTARTSYS && count--)
			goto wait;

		if (test_bit(CQ_FLAGS_RESIZE_IN_PROG, &cq->flags)) {
			dev_err(&rcfw->pdev->dev,
				"QPLIB: FP: RESIZE_CQ timed out");
			rc = -ETIMEDOUT;
			goto fail;
		}

		bnxt_qplib_resize_cq_complete(res, cq);
	}

	return 0;
fail:
	if (!cq->resize_hwq.is_user) {
		bnxt_qplib_free_hwq(res, &cq->resize_hwq);
		clear_bit(CQ_FLAGS_RESIZE_IN_PROG, &cq->flags);
	}
	return rc;
}

void bnxt_qplib_free_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq)
{
	bnxt_qplib_free_hwq(res, &cq->hwq);
}

static void bnxt_qplib_sync_cq(struct bnxt_qplib_cq *cq)
{
	struct bnxt_qplib_nq *nq = cq->nq;
	/* Flush any pending work and synchronize irq */
	flush_workqueue(cq->nq->cqn_wq);
	mutex_lock(&nq->lock);
	if (nq->requested)
		synchronize_irq(nq->msix_vec);
	mutex_unlock(&nq->lock);
}

int bnxt_qplib_destroy_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq,
			  u16 sb_resp_size, void *sb_resp_va)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_destroy_cq_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_destroy_cq req = {};
	struct bnxt_qplib_reftbl *tbl;
	u16 total_cnq_events;
	unsigned long flag;
	int rc;
	struct bnxt_qplib_rcfw_sbuf sbuf = {};

	if (sb_resp_size && sb_resp_va) {
		sbuf.size = sb_resp_size;
		sbuf.sb = dma_zalloc_coherent(&rcfw->pdev->dev, sbuf.size,
					      &sbuf.dma_addr, GFP_KERNEL);
	}

	tbl = &res->reftbl.cqref;
	spin_lock_irqsave(&tbl->lock, flag);
	tbl->rec[GET_TBL_INDEX(cq->id, tbl)].handle = NULL;
	tbl->rec[GET_TBL_INDEX(cq->id, tbl)].xid = 0;
	spin_unlock_irqrestore(&tbl->lock, flag);

	req.resp_addr = cpu_to_le64(sbuf.dma_addr);
	req.resp_size = sb_resp_size / BNXT_QPLIB_CMDQE_UNITS;
	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_DESTROY_CQ,
				 sizeof(req));

	req.cq_cid = cpu_to_le32(cq->id);
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, &sbuf, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		return rc;

	if (sbuf.sb) {
		memcpy(sb_resp_va, sbuf.sb, sb_resp_size);
		dma_free_coherent(&rcfw->pdev->dev, sbuf.size, sbuf.sb, sbuf.dma_addr);
	}

	total_cnq_events = le16_to_cpu(resp.total_cnq_events);
	dev_dbg(&rcfw->pdev->dev,
		"%s: cq_id = 0x%x cq = 0x%p resp.total_cnq_events = 0x%x\n",
		__func__, cq->id, cq, total_cnq_events);
	__wait_for_all_nqes(cq, total_cnq_events);
	bnxt_qplib_sync_cq(cq);
	bnxt_qplib_free_hwq(res, &cq->hwq);
	return 0;
}

static int __flush_sq(struct bnxt_qplib_q *sq, struct bnxt_qplib_qp *qp,
		      struct bnxt_qplib_cqe **pcqe, int *budget)
{
	struct bnxt_qplib_cqe *cqe;
	u32 start, last;
	int rc = 0;

	/* Now complete all outstanding SQEs with FLUSHED_ERR */
	start = sq->swq_start;
	cqe = *pcqe;
	while (*budget) {
		last = sq->swq_last;
		if (start == last) {
			break;
		}
		/* Skip the FENCE WQE completions */
		if (sq->swq[last].wr_id == BNXT_QPLIB_FENCE_WRID) {
			bnxt_re_legacy_cancel_phantom_processing(qp);
			goto skip_compl;
		}

		memset(cqe, 0, sizeof(*cqe));
		cqe->status = CQ_REQ_STATUS_WORK_REQUEST_FLUSHED_ERR;
		cqe->opcode = CQ_BASE_CQE_TYPE_REQ;
		cqe->qp_handle = (u64)qp;
		cqe->wr_id = sq->swq[last].wr_id;
		cqe->src_qp = qp->id;
		cqe->type = sq->swq[last].type;
		dev_dbg(&sq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed terminal Req ");
		dev_dbg(&sq->hwq.pdev->dev,
			"QPLIB: wr_id[%d] = 0x%llx with status 0x%x",
			last, cqe->wr_id, cqe->status);
		cqe++;
		(*budget)--;
skip_compl:
		bnxt_qplib_hwq_incr_cons(sq->hwq.depth,
					 &sq->hwq.cons,
					 sq->swq[last].slots,
					 &sq->dbinfo.flags);
		sq->swq_last = sq->swq[last].next_idx;
	}
	*pcqe = cqe;
	if (!*budget && sq->swq_last != start)
		/* Out of budget */
		rc = -EAGAIN;
	dev_dbg(&sq->hwq.pdev->dev, "QPLIB: FP: Flush SQ rc = 0x%x", rc);

	return rc;
}

static int __flush_rq(struct bnxt_qplib_q *rq, struct bnxt_qplib_qp *qp,
		      struct bnxt_qplib_cqe **pcqe, int *budget)
{
	u8  status = CQ_RES_RC_STATUS_WORK_REQUEST_FLUSHED_ERR;
	int opcode = CQ_BASE_CQE_TYPE_RES_RC;
	struct bnxt_qplib_cqe *cqe;
	u32 start, last;
	int rc = 0;

	switch (qp->type) {
	case CMDQ_CREATE_QP1_TYPE_GSI:
		opcode = CQ_BASE_CQE_TYPE_RES_RAWETH_QP1;
		status = CQ_BASE_STATUS_WORK_REQUEST_FLUSHED_ERR;
		break;
	case CMDQ_CREATE_QP_TYPE_RC:
		opcode = CQ_BASE_CQE_TYPE_RES_RC;
		status = CQ_RES_RC_STATUS_WORK_REQUEST_FLUSHED_ERR;
		break;
	case CMDQ_CREATE_QP_TYPE_UD:
	case CMDQ_CREATE_QP_TYPE_GSI:
		opcode = CQ_BASE_CQE_TYPE_RES_UD;
		status = CQ_RES_UD_STATUS_WORK_REQUEST_FLUSHED_ERR;
		break;
	}

	/* Flush the rest of the RQ */
	start = rq->swq_start;
	cqe = *pcqe;
	while (*budget) {
		last = rq->swq_last;
		if (last == start)
			break;
		memset(cqe, 0, sizeof(*cqe));
		cqe->status = status;
		cqe->opcode = opcode;
		cqe->qp_handle = (u64)qp;
		cqe->wr_id = rq->swq[last].wr_id;
		dev_dbg(&rq->hwq.pdev->dev, "QPLIB: FP: CQ Processed Res RC ");
		dev_dbg(&rq->hwq.pdev->dev,
			"QPLIB: rq[%d] = 0x%llx with status 0x%x qp->type 0x%x",
			last, cqe->wr_id, cqe->status, qp->type);
		cqe++;
		(*budget)--;
		bnxt_qplib_hwq_incr_cons(rq->hwq.depth,
					 &rq->hwq.cons,
					 rq->swq[last].slots,
					 &rq->dbinfo.flags);
		rq->swq_last = rq->swq[last].next_idx;
	}
	*pcqe = cqe;
	if (!*budget && rq->swq_last != start)
		/* Out of budget */
		rc = -EAGAIN;

	dev_dbg(&rq->hwq.pdev->dev, "QPLIB: FP: Flush RQ rc = 0x%x", rc);
	return rc;
}

void bnxt_qplib_mark_qp_error(void *qp_handle)
{
	struct bnxt_qplib_qp *qp = qp_handle;

	if (!qp)
		return;

	/* Must block new posting of SQ and RQ */
	qp->cur_qp_state = CMDQ_MODIFY_QP_NEW_STATE_ERR;
	qp->state = qp->cur_qp_state;

	set_bit(QP_FLAGS_CAPTURE_SNAPDUMP, &qp->flags);
	set_bit(CQ_FLAGS_CAPTURE_SNAPDUMP, &qp->scq->flags);
	set_bit(CQ_FLAGS_CAPTURE_SNAPDUMP, &qp->rcq->flags);
	if (qp->srq)
		set_bit(SRQ_FLAGS_CAPTURE_SNAPDUMP, &qp->srq->flags);

	/* Add qp to flush list of the CQ */
	if (!qp->is_user)
		bnxt_qplib_add_flush_qp(qp);
}

/* Note: SQE is valid from sw_sq_cons up to cqe_sq_cons (exclusive)
 *       CQE is track from sw_cq_cons to max_element but valid only if VALID=1
 */
static int bnxt_re_legacy_do_wa9060(struct bnxt_qplib_qp *qp,
				 struct bnxt_qplib_cq *cq,
				 u32 cq_cons, u32 swq_last,
				 u32 cqe_sq_cons)
{
	struct bnxt_qplib_q *sq = &qp->sq;
	struct bnxt_qplib_swq *swq;
	u32 peek_sw_cq_cons, peek_sq_cons_idx, peek_flags;
	struct cq_terminal *peek_term_hwcqe;
	struct cq_req *peek_req_hwcqe;
	struct bnxt_qplib_qp *peek_qp;
	struct bnxt_qplib_q *peek_sq;
	struct cq_base *peek_hwcqe;
	int i, rc = 0;

	/* Check for the psn_search marking before completing */
	swq = &sq->swq[swq_last];
	if (swq->psn_search &&
	    le32_to_cpu(swq->psn_search->flags_next_psn) & 0x80000000) {
		/* Unmark */
		swq->psn_search->flags_next_psn = cpu_to_le32
				(le32_to_cpu(swq->psn_search->flags_next_psn)
				 & ~0x80000000);
		dev_dbg(&cq->hwq.pdev->dev,
			"FP: Process Req cq_cons=0x%x qp=0x%x sq cons sw=0x%x cqe=0x%x marked!\n",
			cq_cons, qp->id, swq_last, cqe_sq_cons);
		sq->condition = true;
		sq->legacy_send_phantom = true;

		/* TODO: Only ARM if the previous SQE is ARMALL */
		bnxt_qplib_ring_db(&cq->dbinfo, DBC_DBC_TYPE_CQ_ARMALL);

		rc = -EAGAIN;
		goto out;
	}
	if (sq->condition == true) {
		/* Peek at the completions */
		peek_flags = cq->dbinfo.flags;
		peek_sw_cq_cons = cq_cons;
		i = cq->hwq.depth;
		while (i--) {
			peek_hwcqe = bnxt_qplib_get_qe(&cq->hwq,
						       peek_sw_cq_cons, NULL);
			/* If the next hwcqe is VALID */
			if (CQE_CMP_VALID(peek_hwcqe, peek_flags)) {
				/* If the next hwcqe is a REQ */
				dma_rmb();
				switch (peek_hwcqe->cqe_type_toggle &
					CQ_BASE_CQE_TYPE_MASK) {
				case CQ_BASE_CQE_TYPE_REQ:
					peek_req_hwcqe = (struct cq_req *)
							 peek_hwcqe;
					peek_qp = (struct bnxt_qplib_qp *)
						le64_to_cpu(
						peek_req_hwcqe->qp_handle);
					peek_sq = &peek_qp->sq;
					peek_sq_cons_idx =
						((le16_to_cpu(
						  peek_req_hwcqe->sq_cons_idx)
						  - 1) % sq->max_wqe);
					/* If the hwcqe's sq's wr_id matches */
					if (peek_sq == sq &&
					    sq->swq[peek_sq_cons_idx].wr_id ==
					    BNXT_QPLIB_FENCE_WRID) {
						/* Unbreak only if the phantom
						   comes back */
						dev_dbg(&cq->hwq.pdev->dev,
							"FP: Process Req qp=0x%x current sq cons sw=0x%x cqe=0x%x",
							qp->id, swq_last,
							cqe_sq_cons);
						sq->condition = false;
						sq->single = true;
						sq->phantom_cqe_cnt++;
						dev_dbg(&cq->hwq.pdev->dev,
							"qp %#x condition restored at peek cq_cons=%#x sq_cons_idx %#x, phantom_cqe_cnt: %d unmark\n",
							peek_qp->id,
							peek_sw_cq_cons,
							peek_sq_cons_idx,
							sq->phantom_cqe_cnt);
						rc = 0;
						goto out;
					}
					break;

				case CQ_BASE_CQE_TYPE_TERMINAL:
					/* In case the QP has gone into the
					   error state */
					peek_term_hwcqe = (struct cq_terminal *)
							  peek_hwcqe;
					peek_qp = (struct bnxt_qplib_qp *)
						le64_to_cpu(
						peek_term_hwcqe->qp_handle);
					if (peek_qp == qp) {
						sq->condition = false;
						rc = 0;
						goto out;
					}
					break;
				default:
					break;
				}
				/* Valid but not the phantom, so keep looping */
			} else {
				/* Not valid yet, just exit and wait */
				rc = -EINVAL;
				goto out;
			}
			bnxt_qplib_hwq_incr_cons(cq->hwq.depth,
						 &peek_sw_cq_cons,
						 1, &peek_flags);
		}
		dev_err(&cq->hwq.pdev->dev,
			"Should not have come here! cq_cons=0x%x qp=0x%x sq cons sw=0x%x hw=0x%x",
			cq_cons, qp->id, swq_last, cqe_sq_cons);
		rc = -EINVAL;
	}
out:
	return rc;
}

static int bnxt_qplib_get_cqe_sq_cons(struct bnxt_qplib_q *sq, u32 cqe_slot)
{
	struct bnxt_qplib_hwq *sq_hwq;
	struct bnxt_qplib_swq *swq;
	int cqe_sq_cons = -1;
	u32 start, last;

	sq_hwq = &sq->hwq;

	start = sq->swq_start;
	last = sq->swq_last;

	while (last != start) {
		swq = &sq->swq[last];
		if (swq->slot_idx  == cqe_slot) {
			cqe_sq_cons = swq->next_idx;
			dev_err(&sq_hwq->pdev->dev, "%s: Found cons wqe = %d slot = %d\n",
				__func__, cqe_sq_cons, cqe_slot);
			break;
		}

		last = swq->next_idx;
	}
	return cqe_sq_cons;
}

static int bnxt_qplib_cq_process_req(struct bnxt_qplib_cq *cq,
				     struct cq_req *hwcqe,
				     struct bnxt_qplib_cqe **pcqe, int *budget,
				     u32 cq_cons, struct bnxt_qplib_qp **lib_qp)
{
	bool is_v3 = _is_hsi_v3(cq->cctx);
	struct cq_req_v3 *hwcqe_v3;
	struct bnxt_qplib_cqe *cqe;
	struct bnxt_qplib_swq *swq;
	struct bnxt_qplib_qp *qp;
	struct bnxt_qplib_q *sq;
	u32 next_cqe_sq_cons;
#ifdef ENABLE_FP_SPINLOCK
	unsigned long flags;
#endif
	bool out_of_budget;
	u32 cqe_sq_cons;
	int cqe_cons;
	int rc = 0;

	if (is_v3) {
		hwcqe_v3 = (struct cq_req_v3 *)hwcqe;
		qp = (struct bnxt_qplib_qp *)le64_to_cpu(hwcqe_v3->qp_handle);
	} else {
		qp = (struct bnxt_qplib_qp *)le64_to_cpu(hwcqe->qp_handle);
	}
	dev_dbg(&cq->hwq.pdev->dev, "FP: Process Req qp=0x%p", qp);
	if (!qp) {
		dev_err(&cq->hwq.pdev->dev,
			"QPLIB: FP: Process Req qp is NULL");
		return -EINVAL;
	}
	sq = &qp->sq;
	cqe_sq_cons = is_v3 ? le16_to_cpu(hwcqe_v3->sq_cons_idx) % sq->hwq.depth :
			      le16_to_cpu(hwcqe->sq_cons_idx) % sq->max_sw_wqe;
#ifdef ENABLE_FP_SPINLOCK
	spin_lock_irqsave(&sq->hwq.lock, flags);
#endif
	if (qp->sq.flushed) {
		dev_dbg(&cq->hwq.pdev->dev,
			"%s: QPLIB: QP in Flush QP = %p\n", __func__, qp);
		goto done;
	}
	if (!is_v3 && __is_err_cqe_for_var_wqe(qp, hwcqe->status)) {
		cqe_cons = bnxt_qplib_get_cqe_sq_cons(sq, hwcqe->sq_cons_idx);
		if (WARN_ONCE(cqe_cons < 0, "Potential slot index mapping bug")) {
			dev_err(&cq->hwq.pdev->dev, "%s: Wrong SQ cons cqe_slot_indx = %d\n",
				__func__, hwcqe->sq_cons_idx);
			goto done;
		}
		cqe_sq_cons = cqe_cons;
		dev_info(&cq->hwq.pdev->dev, "%s: cqe_sq_cons = %d swq_last = %d swq_start = %d\n",
			__func__, cqe_sq_cons, sq->swq_last, sq->swq_start);
	}

	/* Require to walk the sq's swq to fabricate CQEs for all previously
	 * signaled SWQEs due to CQE aggregation from the current sq cons
	 * to the cqe_sq_cons
	 */
	cqe = *pcqe;
	while (*budget) {
		bool last_cqe_and_err;
		bool sig_comp;
		bool done;

		done = is_v3 ? (sq->hwq.cons == cqe_sq_cons) :
			       (sq->swq_last == cqe_sq_cons);
		if (done)
			break;

		swq = &sq->swq[sq->swq_last];
		memset(cqe, 0, sizeof(*cqe));
		cqe->opcode = is_v3 ? (hwcqe_v3->cqe_type_toggle & CQ_BASE_CQE_TYPE_MASK) :
				      CQ_BASE_CQE_TYPE_REQ;
		cqe->qp_handle = (u64)qp;
		cqe->src_qp = qp->id;
		cqe->wr_id = swq->wr_id;

		if (cqe->wr_id == BNXT_QPLIB_FENCE_WRID)
			goto skip;

		cqe->type = swq->type;

		/* For the last CQE, check for status.  For errors, regardless
		 * of the request being signaled or not, it must complete with
		 * the hwcqe error status
		 */
		if (is_v3) {
			next_cqe_sq_cons = sq->hwq.cons;
			bnxt_qplib_hwq_incr_cons(sq->hwq.depth, &next_cqe_sq_cons,
						 swq->slots, &sq->dbinfo.flags);
			last_cqe_and_err = (next_cqe_sq_cons == cqe_sq_cons) &&
					   (hwcqe_v3->status != CQ_REQ_V3_STATUS_OK);
		} else {
			last_cqe_and_err = (swq->next_idx == cqe_sq_cons) &&
					   (hwcqe->status != CQ_REQ_STATUS_OK);
		}
		if (last_cqe_and_err) {
			cqe->status = is_v3 ? hwcqe_v3->status : hwcqe->status;
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Processed Req ");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: QP 0x%x wr_id[%d] = 0x%llx vendor type 0x%x with vendor status 0x%x",
				cqe->src_qp, sq->swq_last, cqe->wr_id, cqe->type, cqe->status);
			print_hex_dump(KERN_WARNING, "hwcqe: ", DUMP_PREFIX_OFFSET,
				       16, 2, hwcqe, sizeof(*hwcqe), false);
			cqe++;
			(*budget)--;
			bnxt_qplib_mark_qp_error(qp);
		} else {
			/* Before we complete, do WA 9060 */
			if (!_is_chip_p5_plus(qp->cctx)) {
				if (bnxt_re_legacy_do_wa9060(qp, cq, cq_cons,
					      sq->swq_last,
					      cqe_sq_cons)) {
					*lib_qp = qp;
					goto out;
				}
			}
			sig_comp = is_v3 ? !!(swq->flags & SQ_SEND_V3_FLAGS_SIGNAL_COMP) :
					   !!(swq->flags & SQ_SEND_FLAGS_SIGNAL_COMP);
			if (sig_comp) {
				dev_dbg(&cq->hwq.pdev->dev,
					"QPLIB: FP: CQ Processed Req ");
				dev_dbg(&cq->hwq.pdev->dev,
					"QPLIB: wr_id[%d] = 0x%llx ",
					sq->swq_last, cqe->wr_id);
				dev_dbg(&cq->hwq.pdev->dev,
					"QPLIB: with status 0x%x", cqe->status);
				cqe->status = is_v3 ? CQ_REQ_V3_STATUS_OK : CQ_REQ_STATUS_OK;
				cqe++;
				(*budget)--;
			}
		}
skip:
		if (swq->ah_ref_cnt) {
			refcount_dec(swq->ah_ref_cnt);
			swq->ah_ref_cnt = NULL;
		}

		bnxt_qplib_hwq_incr_cons(sq->hwq.depth, &sq->hwq.cons,
					 swq->slots, &sq->dbinfo.flags);
		sq->swq_last = swq->next_idx;
		if (sq->single == true)
			break;
	}
out:
	*pcqe = cqe;
	out_of_budget = is_v3 ? (sq->hwq.prod != sq->hwq.cons && !*budget) :
				(!*budget && sq->swq_last != cqe_sq_cons);
	if (out_of_budget) {
		rc = -EAGAIN;
		goto done;
	}
	/* Back to normal completion mode only after it has completed all of
	   the WC for this CQE */
	sq->single = false;
done:
#ifdef ENABLE_FP_SPINLOCK
	spin_unlock_irqrestore(&sq->hwq.lock, flags);
#endif
	return rc;
}

static void bnxt_qplib_release_srqe(struct bnxt_qplib_srq *srq, u32 tag)
{
	spin_lock(&srq->hwq.lock);
	srq->swq[srq->last_idx].next_idx = (int)tag;
	srq->last_idx = (int)tag;
	srq->swq[srq->last_idx].next_idx = -1;
	bnxt_qplib_hwq_incr_cons(srq->hwq.depth, &srq->hwq.cons,
				 srq->dbinfo.max_slot, &srq->dbinfo.flags);
	spin_unlock(&srq->hwq.lock);
}

static int bnxt_qplib_cq_process_res_rc(struct bnxt_qplib_cq *cq,
					struct cq_res_rc *hwcqe,
					struct bnxt_qplib_cqe **pcqe,
					int *budget)
{
	struct bnxt_qplib_srq *srq;
	struct bnxt_qplib_cqe *cqe;
	struct bnxt_qplib_qp *qp;
	struct bnxt_qplib_q *rq;
#ifdef ENABLE_FP_SPINLOCK
	unsigned long flags;
#endif
	u32 wr_id_idx;
	int rc = 0;

	qp = (struct bnxt_qplib_qp *)le64_to_cpu(hwcqe->qp_handle);
	if (!qp) {
		dev_err(&cq->hwq.pdev->dev, "QPLIB: process_cq RC qp is NULL");
		return -EINVAL;
	}
	if (qp->rq.flushed) {
		dev_dbg(&cq->hwq.pdev->dev,
			"%s: QPLIB: QP in Flush QP = %p\n", __func__, qp);
		goto done;
	}

	cqe = *pcqe;
	cqe->opcode = hwcqe->cqe_type_toggle & CQ_BASE_CQE_TYPE_MASK;
	cqe->length = le32_to_cpu(hwcqe->length);
	cqe->invrkey = le32_to_cpu(hwcqe->imm_data_or_inv_r_key);
	cqe->mr_handle = le64_to_cpu(hwcqe->mr_handle);
	cqe->flags = le16_to_cpu(hwcqe->flags);
	cqe->status = hwcqe->status;
	cqe->qp_handle = (u64)(unsigned long)qp;

	wr_id_idx = le32_to_cpu(hwcqe->srq_or_rq_wr_id) &
				CQ_RES_RC_SRQ_OR_RQ_WR_ID_MASK;
	if (cqe->flags & CQ_RES_RC_FLAGS_SRQ_SRQ) {
		srq = qp->srq;
		if (!srq) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: SRQ used but not defined??");
			return -EINVAL;
		}
		if (wr_id_idx > srq->hwq.depth - 1) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Process RC ");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: wr_id idx 0x%x exceeded SRQ max 0x%x",
				wr_id_idx, srq->hwq.depth);
			print_hex_dump(KERN_WARNING, "hwcqe: ", DUMP_PREFIX_OFFSET,
				       16, 2, hwcqe, sizeof(*hwcqe), false);
			return -EINVAL;
		}
		cqe->wr_id = srq->swq[wr_id_idx].wr_id;
		bnxt_qplib_release_srqe(srq, wr_id_idx);
		dev_dbg(&srq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed RC SRQ wr_id[%d] = 0x%llx",
			wr_id_idx, cqe->wr_id);
		cqe++;
		(*budget)--;
		*pcqe = cqe;
	} else {
		rq = &qp->rq;
		if (wr_id_idx > (rq->max_wqe - 1)) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Process RC ");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: wr_id idx 0x%x exceeded RQ max 0x%x",
				wr_id_idx, rq->hwq.depth);
			print_hex_dump(KERN_WARNING, "hwcqe: ", DUMP_PREFIX_OFFSET,
				       16, 2, hwcqe, sizeof(*hwcqe), false);
			return -EINVAL;
		}
		if (wr_id_idx != rq->swq_last)
			return -EINVAL;
#ifdef ENABLE_FP_SPINLOCK
		spin_lock_irqsave(&rq->hwq.lock, flags);
#endif

		cqe->wr_id = rq->swq[rq->swq_last].wr_id;
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed RC RQ wr_id[%d] = 0x%llx",
			rq->swq_last, cqe->wr_id);
		cqe++;
		(*budget)--;

		if (hwcqe->status != CQ_RES_RC_STATUS_OK) {
			print_hex_dump(KERN_WARNING, "hwcqe: ", DUMP_PREFIX_OFFSET,
				       16, 2, hwcqe, sizeof(*hwcqe), false);
			bnxt_qplib_mark_qp_error(qp);
		}

		bnxt_qplib_hwq_incr_cons(rq->hwq.depth, &rq->hwq.cons,
					 rq->swq[rq->swq_last].slots,
					 &rq->dbinfo.flags);
		rq->swq_last = rq->swq[rq->swq_last].next_idx;
		*pcqe = cqe;


#ifdef ENABLE_FP_SPINLOCK
		spin_unlock_irqrestore(&rq->hwq.lock, flags);
#endif
	}
done:
	return rc;
}

static int bnxt_qplib_cq_process_res_ud(struct bnxt_qplib_cq *cq,
					struct cq_base *hwcqe,
					struct bnxt_qplib_cqe **pcqe,
					int *budget)
{
	bool is_v3 = _is_hsi_v3(cq->cctx);
	struct cq_res_ud_v3 *hwcqe_v3;
	struct cq_res_ud_v2 *hwcqe_v2;
	struct bnxt_qplib_srq *srq;
	struct bnxt_qplib_cqe *cqe;
	struct bnxt_qplib_qp *qp;
	struct bnxt_qplib_q *rq;
#ifdef ENABLE_FP_SPINLOCK
	unsigned long flags;
#endif
	u32 wr_id_idx;
	int rc = 0;
	u16 *smac;

	if (is_v3) {
		hwcqe_v3 = (struct cq_res_ud_v3 *)hwcqe;
		qp = (struct bnxt_qplib_qp *)le64_to_cpu(hwcqe_v3->qp_handle);
	} else {
		hwcqe_v2 = (struct cq_res_ud_v2 *)hwcqe;
		qp = (struct bnxt_qplib_qp *)le64_to_cpu(hwcqe_v2->qp_handle);
	}
	if (!qp) {
		dev_err(&cq->hwq.pdev->dev, "QPLIB: process_cq UD qp is NULL");
		return -EINVAL;
	}
	if (qp->rq.flushed) {
		dev_dbg(&cq->hwq.pdev->dev,
			"%s: QPLIB: QP in Flush QP = %p\n", __func__, qp);
		goto done;
	}
	cqe = *pcqe;
	if (is_v3) {
		cqe->opcode = hwcqe_v3->cqe_type_toggle & CQ_RES_UD_V3_CQE_TYPE_MASK;
		cqe->length = le16_to_cpu(hwcqe_v3->length) & CQ_RES_UD_V3_LENGTH_MASK;
		cqe->invrkey = le32_to_cpu(hwcqe_v3->imm_data);
		cqe->flags = le16_to_cpu(hwcqe_v3->flags);
		cqe->status = hwcqe_v3->status;
		cqe->qp_handle = (u64)(unsigned long)qp;
		smac = (u16 *)cqe->smac;
		smac[2] = ntohs(le16_to_cpu(hwcqe_v3->src_mac[0]));
		smac[1] = ntohs(le16_to_cpu(hwcqe_v3->src_mac[1]));
		smac[0] = ntohs(le16_to_cpu(hwcqe_v3->src_mac[2]));
		wr_id_idx = le32_to_cpu(hwcqe_v3->opaque);
		cqe->src_qp = (hwcqe_v3->src_qp_high << 16) | le16_to_cpu(hwcqe_v3->src_qp_low);
	} else {
		cqe->opcode = hwcqe_v2->cqe_type_toggle & CQ_RES_UD_V2_CQE_TYPE_MASK;
		cqe->length = le32_to_cpu((hwcqe_v2->length & CQ_RES_UD_V2_LENGTH_MASK));
		cqe->cfa_meta = le16_to_cpu(hwcqe_v2->cfa_metadata0);
		/* V2 format has metadata1 */
		cqe->cfa_meta |= (((le32_to_cpu(hwcqe_v2->src_qp_high_srq_or_rq_wr_id) &
				   CQ_RES_UD_V2_CFA_METADATA1_MASK) >>
				  CQ_RES_UD_V2_CFA_METADATA1_SFT) <<
				 BNXT_QPLIB_META1_SHIFT);
		cqe->invrkey = le32_to_cpu(hwcqe_v2->imm_data);
		cqe->flags = le16_to_cpu(hwcqe_v2->flags);
		cqe->status = hwcqe_v2->status;
		cqe->qp_handle = (u64)(unsigned long)qp;
		smac = (u16 *)cqe->smac;
		smac[2] = ntohs(le16_to_cpu(hwcqe_v2->src_mac[0]));
		smac[1] = ntohs(le16_to_cpu(hwcqe_v2->src_mac[1]));
		smac[0] = ntohs(le16_to_cpu(hwcqe_v2->src_mac[2]));
		wr_id_idx = le32_to_cpu(hwcqe_v2->src_qp_high_srq_or_rq_wr_id) &
			    CQ_RES_UD_V2_SRQ_OR_RQ_WR_ID_MASK;
		cqe->src_qp = le16_to_cpu(hwcqe_v2->src_qp_low) |
			      ((le32_to_cpu(hwcqe_v2->src_qp_high_srq_or_rq_wr_id) &
			       CQ_RES_UD_V2_SRC_QP_HIGH_MASK) >> 8);
	}

	if ((is_v3 && (cqe->flags & CQ_RES_UD_V3_FLAGS_SRQ)) ||
	    (!is_v3 && (cqe->flags & CQ_RES_UD_V2_FLAGS_SRQ))) {
		srq = qp->srq;
		if (!srq) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: SRQ used but not defined??");
			return -EINVAL;
		}
		if (wr_id_idx > srq->hwq.depth - 1) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Process UD ");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: wr_id idx 0x%x exceeded SRQ max 0x%x",
				wr_id_idx, srq->hwq.depth);
			print_hex_dump(KERN_WARNING, "hwcqe: ", DUMP_PREFIX_OFFSET,
				       16, 2, hwcqe, sizeof(*hwcqe), false);
			return -EINVAL;
		}
		cqe->wr_id = srq->swq[wr_id_idx].wr_id;
		bnxt_qplib_release_srqe(srq, wr_id_idx);
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed UD SRQ wr_id[%d] = 0x%llx",
			wr_id_idx, cqe->wr_id);
		cqe++;
		(*budget)--;
		*pcqe = cqe;
	} else {
		rq = &qp->rq;
		if (wr_id_idx > (rq->max_wqe - 1)) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Process UD ");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: wr_id idx 0x%x exceeded RQ max 0x%x",
				wr_id_idx, rq->hwq.depth);
			print_hex_dump(KERN_WARNING, "hwcqe: ", DUMP_PREFIX_OFFSET,
				       16, 2, hwcqe, sizeof(*hwcqe), false);
			return -EINVAL;
		}
		if (rq->swq_last != wr_id_idx)
			return -EINVAL;

#ifdef ENABLE_FP_SPINLOCK
		spin_lock_irqsave(&rq->hwq.lock, flags);
#endif
		cqe->wr_id = rq->swq[rq->swq_last].wr_id;
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed UD RQ wr_id[%d] = 0x%llx",
			 rq->swq_last, cqe->wr_id);
		cqe++;
		(*budget)--;

		if ((is_v3 && (hwcqe_v3->status != CQ_RES_UD_V3_STATUS_OK)) ||
		    (!is_v3 && (hwcqe_v2->status != CQ_RES_UD_V2_STATUS_OK))) {
			print_hex_dump(KERN_WARNING, "hwcqe: ", DUMP_PREFIX_OFFSET,
				       16, 2, hwcqe, sizeof(*hwcqe), false);
			bnxt_qplib_mark_qp_error(qp);
		}

		bnxt_qplib_hwq_incr_cons(rq->hwq.depth, &rq->hwq.cons,
					 rq->swq[rq->swq_last].slots,
					 &rq->dbinfo.flags);
		rq->swq_last = rq->swq[rq->swq_last].next_idx;
		*pcqe = cqe;


#ifdef ENABLE_FP_SPINLOCK
		spin_unlock_irqrestore(&rq->hwq.lock, flags);
#endif
	}
done:
	return rc;
}

bool bnxt_qplib_is_cq_empty(struct bnxt_qplib_cq *cq)
{

	struct cq_base *hw_cqe;
	unsigned long flags;
	bool rc = true;

	spin_lock_irqsave(&cq->hwq.lock, flags);
	hw_cqe = bnxt_qplib_get_qe(&cq->hwq, cq->hwq.cons, NULL);

	 /* Check for Valid bit. If the CQE is valid, return false */
	rc = !CQE_CMP_VALID(hw_cqe, cq->dbinfo.flags);
	spin_unlock_irqrestore(&cq->hwq.lock, flags);
	return rc;
}

static int bnxt_qplib_cq_process_res_raweth_qp1(struct bnxt_qplib_cq *cq,
						struct cq_res_raweth_qp1 *hwcqe,
						struct bnxt_qplib_cqe **pcqe,
						int *budget)
{
	struct bnxt_qplib_qp *qp;
	struct bnxt_qplib_q *rq;
	struct bnxt_qplib_srq *srq;
	struct bnxt_qplib_cqe *cqe;
	u32 wr_id_idx;
#ifdef ENABLE_FP_SPINLOCK
	unsigned long flags;
#endif
	int rc = 0;

	qp = (struct bnxt_qplib_qp *)le64_to_cpu(hwcqe->qp_handle);
	if (!qp) {
		dev_err(&cq->hwq.pdev->dev,
			"QPLIB: process_cq Raw/QP1 qp is NULL");
		return -EINVAL;
	}
	if (qp->rq.flushed) {
		dev_dbg(&cq->hwq.pdev->dev,
			"%s: QPLIB: QP in Flush QP = %p\n", __func__, qp);
		goto done;
	}
	cqe = *pcqe;
	cqe->opcode = hwcqe->cqe_type_toggle & CQ_BASE_CQE_TYPE_MASK;
	cqe->flags = le16_to_cpu(hwcqe->flags);
	cqe->qp_handle = (u64)(unsigned long)qp;

	wr_id_idx = le32_to_cpu(hwcqe->raweth_qp1_payload_offset_srq_or_rq_wr_id)
				& CQ_RES_RAWETH_QP1_SRQ_OR_RQ_WR_ID_MASK;
	cqe->src_qp = qp->id;
	if (qp->id == 1 && !cqe->length) {
		/* Add workaround for the length misdetection */
		cqe->length = 296;
	} else {
		cqe->length = le16_to_cpu(hwcqe->length);
	}
	cqe->pkey_index = qp->pkey_index;
	memcpy(cqe->smac, qp->smac, 6);

	cqe->raweth_qp1_flags = le16_to_cpu(hwcqe->raweth_qp1_flags);
	cqe->raweth_qp1_flags2 = le32_to_cpu(hwcqe->raweth_qp1_flags2);
	cqe->raweth_qp1_metadata = le32_to_cpu(hwcqe->raweth_qp1_metadata);

	dev_dbg(&cq->hwq.pdev->dev,
		 "QPLIB: raweth_qp1_flags = 0x%x raweth_qp1_flags2 = 0x%x\n",
		 cqe->raweth_qp1_flags, cqe->raweth_qp1_flags2);

	if (cqe->flags & CQ_RES_RAWETH_QP1_FLAGS_SRQ_SRQ) {
		srq = qp->srq;
		if (!srq) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: SRQ used but not defined??");
			return -EINVAL;
		}
		if (wr_id_idx > srq->hwq.depth - 1) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Process Raw/QP1 ");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: wr_id idx 0x%x exceeded SRQ max 0x%x",
				wr_id_idx, srq->hwq.depth);
			print_hex_dump(KERN_WARNING, "hwcqe: ", DUMP_PREFIX_OFFSET,
				       16, 2, hwcqe, sizeof(*hwcqe), false);
			return -EINVAL;
		}
#ifdef ENABLE_FP_SPINLOCK
		spin_lock_irqsave(&srq->hwq.lock, flags);
#endif
		cqe->wr_id = srq->swq[wr_id_idx].wr_id;
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed Raw/QP1 SRQ ");
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: wr_id[%d] = 0x%llx with status = 0x%x",
			wr_id_idx, cqe->wr_id, hwcqe->status);
		cqe++;
		(*budget)--;
		srq->hwq.cons++;
		*pcqe = cqe;
#ifdef ENABLE_FP_SPINLOCK
		spin_unlock_irqrestore(&srq->hwq.lock, flags);
#endif
	} else {
		rq = &qp->rq;
		if (wr_id_idx > (rq->max_wqe - 1)) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Process Raw/QP1 RQ wr_id ");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: ix 0x%x exceeded RQ max 0x%x",
				wr_id_idx, rq->max_wqe);
			print_hex_dump(KERN_WARNING, "hwcqe: ", DUMP_PREFIX_OFFSET,
				       16, 2, hwcqe, sizeof(*hwcqe), false);
			return -EINVAL;
		}
		if (wr_id_idx != rq->swq_last)
			return -EINVAL;
#ifdef ENABLE_FP_SPINLOCK
		spin_lock_irqsave(&rq->hwq.lock, flags);
#endif
		cqe->wr_id = rq->swq[rq->swq_last].wr_id;
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed Raw/QP1 RQ ");
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: wr_id[%d] = 0x%llx with status = 0x%x",
			wr_id_idx, cqe->wr_id, hwcqe->status);
		cqe++;
		(*budget)--;

		if (hwcqe->status != CQ_RES_RC_STATUS_OK) {
			print_hex_dump(KERN_WARNING, "hwcqe: ", DUMP_PREFIX_OFFSET,
				       16, 2, hwcqe, sizeof(*hwcqe), false);
			bnxt_qplib_mark_qp_error(qp);
		}

		bnxt_qplib_hwq_incr_cons(rq->hwq.depth, &rq->hwq.cons,
					 rq->swq[wr_id_idx].slots,
					 &rq->dbinfo.flags);
		rq->swq_last = rq->swq[rq->swq_last].next_idx;
		*pcqe = cqe;


#ifdef ENABLE_FP_SPINLOCK
		spin_unlock_irqrestore(&rq->hwq.lock, flags);
#endif
	}
done:
	return rc;
}

static int bnxt_qplib_cq_process_terminal(struct bnxt_qplib_cq *cq,
					  struct cq_terminal *hwcqe,
					  struct bnxt_qplib_cqe **pcqe,
					  int *budget)
{
	struct bnxt_qplib_q *sq, *rq;
	struct bnxt_qplib_cqe *cqe;
	struct bnxt_qplib_qp *qp;
#ifdef ENABLE_FP_SPINLOCK
	unsigned long flags;
#endif
	u32 cqe_cons;
	int rc = 0;

	/* Check the Status */
	if (hwcqe->status != CQ_TERMINAL_STATUS_OK)
		dev_warn(&cq->hwq.pdev->dev,
			 "QPLIB: FP: CQ Process Terminal Error status = 0x%x",
			 hwcqe->status);

	qp = (struct bnxt_qplib_qp *)le64_to_cpu(hwcqe->qp_handle);
	if (!qp)
		return -EINVAL;
	dev_dbg(&cq->hwq.pdev->dev,
		"QPLIB: FP: CQ Process terminal for qp (0x%x)", qp->id);

	/* Terminal CQE requires all posted RQEs to complete with FLUSHED_ERR
	 * from the current rq->cons to the rq->prod regardless what the
	 * rq->cons the terminal CQE indicates.
	 */
	bnxt_qplib_mark_qp_error(qp);

	sq = &qp->sq;
	rq = &qp->rq;

	cqe_cons = le16_to_cpu(hwcqe->sq_cons_idx);
	if (cqe_cons == 0xFFFF)
		goto do_rq;

	cqe_cons %= sq->max_wqe;
#ifdef ENABLE_FP_SPINLOCK
	spin_lock_irqsave(&sq->hwq.lock, flags);
#endif
	if (qp->sq.flushed) {
		dev_dbg(&cq->hwq.pdev->dev,
			"%s: QPLIB: QP in Flush QP = %p\n", __func__, qp);
		goto sq_done;
	}

	/* Terminal CQE can also include aggregated successful CQEs prior.
	   So we must complete all CQEs from the current sq's cons to the
	   cq_cons with status OK */
	cqe = *pcqe;
	while (*budget) {
		/*sw_cons = HWQ_CMP(sq->hwq.cons, &sq->hwq);*/
		if (sq->swq_last == cqe_cons)
			break;
		if (sq->swq[sq->swq_last].flags & SQ_SEND_FLAGS_SIGNAL_COMP) {
			memset(cqe, 0, sizeof(*cqe));
			cqe->status = CQ_REQ_STATUS_OK;
			cqe->opcode = CQ_BASE_CQE_TYPE_REQ;
			cqe->qp_handle = (u64)qp;
			cqe->src_qp = qp->id;
			cqe->wr_id = sq->swq[sq->swq_last].wr_id;
			cqe->type = sq->swq[sq->swq_last].type;
			dev_dbg(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Processed terminal Req ");
			dev_dbg(&cq->hwq.pdev->dev,
				"QPLIB: wr_id[%d] = 0x%llx with status 0x%x",
				sq->swq_last, cqe->wr_id, cqe->status);
			print_hex_dump(KERN_WARNING, "hwcqe: ", DUMP_PREFIX_OFFSET,
				       16, 2, hwcqe, sizeof(*hwcqe), false);
			cqe++;
			(*budget)--;
		}
		bnxt_qplib_hwq_incr_cons(sq->hwq.depth, &sq->hwq.cons,
					 sq->swq[sq->swq_last].slots,
					 &sq->dbinfo.flags);
		sq->swq_last = sq->swq[sq->swq_last].next_idx;
	}
	*pcqe = cqe;
	if (!*budget && sq->swq_last != cqe_cons) {
		/* Out of budget */
		rc = -EAGAIN;
		goto sq_done;
	}
sq_done:
#ifdef ENABLE_FP_SPINLOCK
	spin_unlock_irqrestore(&sq->hwq.lock, flags);
#endif
	if (rc)
		return rc;
do_rq:
	cqe_cons = le16_to_cpu(hwcqe->rq_cons_idx);
	if (cqe_cons == 0xFFFF) {
		goto done;
	} else if (cqe_cons > (rq->max_wqe - 1)) {
		dev_err(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed terminal ");
		dev_err(&cq->hwq.pdev->dev,
			"QPLIB: reported rq_cons_idx 0x%x exceeds max 0x%x",
			cqe_cons, rq->hwq.depth);
		goto done;
	}
#ifdef ENABLE_FP_SPINLOCK
	spin_lock_irqsave(&rq->hwq.lock, flags);
#endif
	if (qp->rq.flushed) {
		dev_dbg(&cq->hwq.pdev->dev,
			"%s: QPLIB: QP in Flush QP = %p\n", __func__, qp);
		rc = 0;
		goto rq_done;
	}

rq_done:
#ifdef ENABLE_FP_SPINLOCK
	spin_unlock_irqrestore(&rq->hwq.lock, flags);
#endif
done:
	return rc;
}

static int bnxt_qplib_cq_process_cutoff(struct bnxt_qplib_cq *cq,
					struct cq_cutoff *hwcqe)
{
	/* Check the Status */
	if (hwcqe->status != CQ_CUTOFF_STATUS_OK) {
		dev_err(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Process Cutoff Error status = 0x%x",
			hwcqe->status);
		return -EINVAL;
	}
	clear_bit(CQ_FLAGS_RESIZE_IN_PROG, &cq->flags);
	wake_up_interruptible(&cq->waitq);

	dev_dbg(&cq->hwq.pdev->dev, "QPLIB: FP: CQ Processed Cutoff");
	return 0;
}

int bnxt_qplib_process_flush_list(struct bnxt_qplib_cq *cq,
				struct bnxt_qplib_cqe *cqe,
				int num_cqes)
{
	struct bnxt_qplib_qp *qp = NULL;
	u32 budget = num_cqes;
	unsigned long flags;

	spin_lock_irqsave(&cq->flush_lock, flags);
	list_for_each_entry(qp, &cq->sqf_head, sq_flush) {
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: Flushing SQ QP= %p",
			qp);
		__flush_sq(&qp->sq, qp, &cqe, &budget);
	}

	list_for_each_entry(qp, &cq->rqf_head, rq_flush) {
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: Flushing RQ QP= %p",
			qp);
		__flush_rq(&qp->rq, qp, &cqe, &budget);
	}
	spin_unlock_irqrestore(&cq->flush_lock, flags);

	return num_cqes - budget;
}

int bnxt_qplib_poll_cq(struct bnxt_qplib_cq *cq, struct bnxt_qplib_cqe *cqe,
		       int num_cqes, struct bnxt_qplib_qp **lib_qp)
{
	struct cq_base *hw_cqe;
	u32 hw_polled = 0;
	int budget, rc = 0;
	u8 type;

#ifdef ENABLE_FP_SPINLOCK
	unsigned long flags;
	spin_lock_irqsave(&cq->hwq.lock, flags);
#endif
	budget = num_cqes;

	while (budget) {
		hw_cqe = bnxt_qplib_get_qe(&cq->hwq, cq->hwq.cons, NULL);

		/* Check for Valid bit */
		if (!CQE_CMP_VALID(hw_cqe, cq->dbinfo.flags))
			break;

		/* The valid test of the entry must be done first before
		 * reading any further.
		 */
		dma_rmb();
		/* From the device's respective CQE format to qplib_wc*/
		type = hw_cqe->cqe_type_toggle & CQ_BASE_CQE_TYPE_MASK;
		switch (type) {
		case CQ_BASE_CQE_TYPE_REQ:
		case CQ_BASE_CQE_TYPE_REQ_V3:
			rc = bnxt_qplib_cq_process_req(cq,
					(struct cq_req *)hw_cqe, &cqe, &budget,
					cq->hwq.cons, lib_qp);
			break;
		case CQ_BASE_CQE_TYPE_RES_RC:
			rc = bnxt_qplib_cq_process_res_rc(cq,
						(struct cq_res_rc *)hw_cqe, &cqe,
						&budget);
			break;
		case CQ_BASE_CQE_TYPE_RES_UD:
		case CQ_BASE_CQE_TYPE_RES_UD_V3:
			rc = bnxt_qplib_cq_process_res_ud(cq, hw_cqe, &cqe, &budget);
			break;
		case CQ_BASE_CQE_TYPE_RES_RAWETH_QP1:
			rc = bnxt_qplib_cq_process_res_raweth_qp1(cq,
						(struct cq_res_raweth_qp1 *)
						hw_cqe, &cqe, &budget);
			break;
		case CQ_BASE_CQE_TYPE_TERMINAL:
			rc = bnxt_qplib_cq_process_terminal(cq,
						(struct cq_terminal *)hw_cqe,
						&cqe, &budget);
			break;
		case CQ_BASE_CQE_TYPE_CUT_OFF:
			bnxt_qplib_cq_process_cutoff(cq,
						(struct cq_cutoff *)hw_cqe);
			/* Done processing this CQ */
			goto exit;
		default:
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: process_cq unknown type 0x%lx",
				hw_cqe->cqe_type_toggle &
				CQ_BASE_CQE_TYPE_MASK);
			rc = -EINVAL;
			break;
		}
		if (rc < 0) {
			dev_dbg(&cq->hwq.pdev->dev,
				"QPLIB: process_cqe rc = 0x%x", rc);
			if (rc == -EAGAIN)
				break;
			/* Error while processing the CQE, just skip to the
			   next one */
			if (type != CQ_BASE_CQE_TYPE_TERMINAL)
				dev_err(&cq->hwq.pdev->dev,
					"QPLIB: process_cqe error rc = 0x%x",
					rc);
		}
		hw_polled++;
		bnxt_qplib_hwq_incr_cons(cq->hwq.depth, &cq->hwq.cons,
					 1, &cq->dbinfo.flags);
	}
	if (hw_polled)
		bnxt_qplib_ring_db(&cq->dbinfo, DBC_DBC_TYPE_CQ);
exit:
#ifdef ENABLE_FP_SPINLOCK
	spin_unlock_irqrestore(&cq->hwq.lock, flags);
#endif
	return num_cqes - budget;
}

void bnxt_qplib_req_notify_cq(struct bnxt_qplib_cq *cq, u32 arm_type)
{
#ifdef ENABLE_FP_SPINLOCK
	unsigned long flags;

	spin_lock_irqsave(&cq->hwq.lock, flags);
#endif
	cq->dbinfo.toggle = cq->toggle;
	if (arm_type)
		bnxt_qplib_ring_db(&cq->dbinfo, arm_type);
	/* Using cq->arm_state variable to track whether to issue cq handler */
	atomic_set(&cq->arm_state, 1);
#ifdef ENABLE_FP_SPINLOCK
	spin_unlock_irqrestore(&cq->hwq.lock, flags);
#endif
}

void bnxt_qplib_flush_cqn_wq(struct bnxt_qplib_qp *qp)
{
	flush_workqueue(qp->scq->nq->cqn_wq);
	if (qp->scq != qp->rcq)
		flush_workqueue(qp->rcq->nq->cqn_wq);
}
