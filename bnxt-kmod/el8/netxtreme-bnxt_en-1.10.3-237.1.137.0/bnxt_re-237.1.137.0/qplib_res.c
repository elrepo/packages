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
 * Description: QPLib resource manager
 */

#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/inetdevice.h>
#include <linux/dma-mapping.h>
#include <linux/if_vlan.h>
#include <linux/vmalloc.h>

#include <net/ipv6.h>
#include <net/addrconf.h>
#include <rdma/ib_verbs.h>

#include "roce_hsi.h"
#include "qplib_res.h"
#include "qplib_sp.h"
#include "qplib_rcfw.h"
#include "compat.h"
#include "bnxt_re.h"

inline bool _is_alloc_mr_unified(struct bnxt_qplib_dev_attr *dattr)
{
	return dattr->dev_cap_flags &
	       CREQ_QUERY_FUNC_RESP_SB_MR_REGISTER_ALLOC;
}

/* PBL */
static void __free_pbl(struct bnxt_qplib_res *res,
		       struct bnxt_qplib_pbl *pbl, bool is_umem)
{
	struct pci_dev *pdev;
	int i;

	pdev = res->pdev;
	if (is_umem == false) {
		for (i = 0; i < pbl->pg_count; i++) {
			if (pbl->pg_arr[i]) {
				dma_free_coherent(&pdev->dev, pbl->pg_size,
					(void *)((u64)pbl->pg_arr[i] &
						 PAGE_MASK),
					pbl->pg_map_arr[i]);
			}
			else
				dev_warn(&pdev->dev,
					 "QPLIB: PBL free pg_arr[%d] empty?!",
					 i);
			pbl->pg_arr[i] = NULL;
		}
	}

	if (pbl->pg_arr) {
		vfree(pbl->pg_arr);
		pbl->pg_arr = NULL;
	}
	if (pbl->pg_map_arr) {
		vfree(pbl->pg_map_arr);
		pbl->pg_map_arr = NULL;
	}
	pbl->pg_count = 0;
	pbl->pg_size = 0;
}

static dma_addr_t
bnxt_re_trap_and_map_addr(struct bnxt_qplib_pbl *pbl, dma_addr_t addr)
{
	struct bnxt_peer_bar_addr *bar_addr = pbl->res->bar_addr;
	int count = atomic_read(&pbl->res->bar_cnt);
	int i;

	for (i = 0; i < count; i++) {
		struct bnxt_peer_bar_addr bar = bar_addr[i];
		dma_addr_t vm_bar_addr = bar.vm_bar_addr;
		dma_addr_t hv_bar_addr = bar.hv_bar_addr;
		u64 bar_size = bar.bar_size;

		if (vm_bar_addr == hv_bar_addr)
			continue;

		if (addr >= vm_bar_addr && addr < vm_bar_addr + bar_size) {
			addr = hv_bar_addr + (addr & ~vm_bar_addr);
			break;
		}
	}

	return addr;
}

#if !defined(HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK) && !defined(HAVE_FOR_EACH_SG_DMA_PAGE)
struct qplib_sg {
	dma_addr_t 	pg_map_arr;
	u32		size;
};

static int __fill_user_dma_pages(struct bnxt_qplib_pbl *pbl,
				 struct bnxt_qplib_sg_info *sginfo)
{
	int sg_indx, pg_indx, tmp_size, offset;
	struct qplib_sg *tmp_sg = NULL;
	struct scatterlist *sg;
	u64 pmask, addr;

	tmp_sg = vzalloc(sginfo->nmap * sizeof(struct qplib_sg));
	if (!tmp_sg)
		return -ENOMEM;

	pmask = BIT_ULL(sginfo->pgshft) - 1;
	sg_indx = 0;
	for_each_sg(sginfo->sghead, sg, sginfo->nmap, sg_indx) {
		tmp_sg[sg_indx].pg_map_arr = sg_dma_address(sg);
		tmp_sg[sg_indx].size = sg_dma_len(sg);
	}
	pg_indx = 0;
	for (sg_indx = 0; sg_indx < sginfo->nmap; sg_indx++) {
		tmp_size = tmp_sg[sg_indx].size;
		offset = 0;
		while (tmp_size > 0) {
			addr = tmp_sg[sg_indx].pg_map_arr + offset;
			if ((!sg_indx && !pg_indx) || !(addr & pmask)) {
				dma_addr_t dma_addr = (addr & (~pmask));

				pbl->pg_map_arr[pg_indx] = bnxt_re_trap_and_map_addr(pbl, dma_addr);
				pbl->pg_count++;
				pg_indx++;
			}
			offset += sginfo->pgsize;
			tmp_size -= sginfo->pgsize;
		}
	}

	vfree(tmp_sg);
	return 0;
}
#endif

static int bnxt_qplib_fill_user_dma_pages(struct bnxt_qplib_pbl *pbl,
					  struct bnxt_qplib_sg_info *sginfo)
{
	int rc = 0;

#ifdef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	struct ib_block_iter biter;
	int pg_indx = 0;

	rdma_umem_for_each_dma_block(sginfo->umem, &biter, sginfo->pgsize) {
		dma_addr_t dma_addr = rdma_block_iter_dma_address(&biter);

		pbl->pg_map_arr[pg_indx] = bnxt_re_trap_and_map_addr(pbl, dma_addr);
		pbl->pg_arr[pg_indx] = NULL;
		pbl->pg_count++;
		pg_indx++;
	}
#else
#ifdef HAVE_FOR_EACH_SG_DMA_PAGE
	struct sg_dma_page_iter sg_iter;
	int indx = 0, pg_indx = 0;
	u64 pmask, addr;

	/* TODO: Use rdma block iterator. */
	pmask = BIT_ULL(sginfo->pgshft) - 1;
	for_each_sg_dma_page(sginfo->sghead, &sg_iter, sginfo->nmap, 0) {
		addr = sg_page_iter_dma_address(&sg_iter);
		if (!indx || !(addr & pmask)) {
			dma_addr_t dma_addr = (addr & (~pmask));

			pbl->pg_map_arr[pg_indx] = bnxt_re_trap_and_map_addr(pbl, dma_addr);
			pbl->pg_arr[pg_indx] = NULL;
			pbl->pg_count++;
			pg_indx++;
		}
		indx++;
	}
#else
	rc =  __fill_user_dma_pages(pbl, sginfo);
#endif
#endif
	return rc;
}

static int __alloc_pbl(struct bnxt_qplib_res *res, struct bnxt_qplib_pbl *pbl,
		       struct bnxt_qplib_sg_info *sginfo)
{
	struct pci_dev *pdev;
	bool is_umem = false;
	int i;

	if (sginfo->nopte)
		return 0;

	pdev = res->pdev;
	/* page ptr arrays */
	pbl->pg_arr = vmalloc_array(sginfo->npages, sizeof(void *));
	if (!pbl->pg_arr)
		return -ENOMEM;
	memset(pbl->pg_arr, 0, sginfo->npages * sizeof(void *));

	pbl->pg_map_arr = vmalloc_array(sginfo->npages, sizeof(dma_addr_t));
	if (!pbl->pg_map_arr) {
		vfree(pbl->pg_arr);
		pbl->pg_arr = NULL;
		return -ENOMEM;
	}
	memset(pbl->pg_map_arr, 0, sginfo->npages * sizeof(dma_addr_t));
	pbl->pg_count = 0;
	pbl->pg_size = sginfo->pgsize;
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	if (!sginfo->sghead) {
#else
	if (!sginfo->umem) {
#endif
		for (i = 0; i < sginfo->npages; i++) {
			pbl->pg_arr[i] = dma_zalloc_coherent(&pdev->dev,
							     pbl->pg_size,
							     &pbl->pg_map_arr[i],
							     GFP_KERNEL);
			if (!pbl->pg_arr[i])
				goto fail;
			pbl->pg_count++;
		}
	} else {
		is_umem = true;
		pbl->res = res;
		if (bnxt_qplib_fill_user_dma_pages(pbl, sginfo))
			goto fail;
	}

	return 0;
fail:
	__free_pbl(res, pbl, is_umem);
	return -ENOMEM;
}

/* HWQ */
void bnxt_qplib_free_hwq(struct bnxt_qplib_res *res,
			 struct bnxt_qplib_hwq *hwq)
{
	int i;

	if (!hwq->max_elements)
		return;
	if (hwq->level >= PBL_LVL_MAX)
		return;

	for (i = 0; i < hwq->level + 1; i++) {
		if (i == hwq->level)
			__free_pbl(res, &hwq->pbl[i], hwq->is_user);
		else
			__free_pbl(res, &hwq->pbl[i], false);
	}

	hwq->level = PBL_LVL_MAX;
	hwq->max_elements = 0;
	hwq->element_size = 0;
	hwq->prod = hwq->cons = 0;
	hwq->cp_bit = 0;
}

/* All HWQs are power of 2 in size */
int bnxt_qplib_alloc_init_hwq(struct bnxt_qplib_hwq *hwq,
			      struct bnxt_qplib_hwq_attr *hwq_attr)
{
	u32 npages = 0, depth, stride, aux_pages = 0;
	dma_addr_t *src_phys_ptr, **dst_virt_ptr;
	struct bnxt_qplib_sg_info sginfo = {};
	u32 aux_size = 0, npbl, npde;
	void *umem;
	struct bnxt_qplib_res *res;
	u32 aux_slots, pg_size;
	struct pci_dev *pdev;
	int i, rc, lvl;

	res = hwq_attr->res;
	pdev = res->pdev;
#ifndef HAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
	umem = hwq_attr->sginfo->sghead;
#else
	umem = hwq_attr->sginfo->umem;
#endif
	pg_size = hwq_attr->sginfo->pgsize;
	hwq->level = PBL_LVL_MAX;

	depth = roundup_pow_of_two(hwq_attr->depth);
	stride = roundup_pow_of_two(hwq_attr->stride);
	if (hwq_attr->aux_depth) {
		aux_slots = hwq_attr->aux_depth;
		aux_size = roundup_pow_of_two(hwq_attr->aux_stride);
		aux_pages = (aux_slots * aux_size) / pg_size;
		if ((aux_slots * aux_size) % pg_size)
			aux_pages++;
	}

	if (!umem) {
		hwq->is_user = false;
		npages = (depth * stride) / pg_size + aux_pages;
		if ((depth * stride) % pg_size)
			npages++;
		if (!npages)
			return -EINVAL;
		hwq_attr->sginfo->npages = npages;
	} else {
		hwq->is_user = true;
		npages = hwq_attr->sginfo->npages;
		npages = (npages * (u64)pg_size) /
			  BIT_ULL(hwq_attr->sginfo->pgshft);
		if ((hwq_attr->sginfo->npages * (u64)pg_size) %
		     BIT_ULL(hwq_attr->sginfo->pgshft))
			npages++;
	}
#ifdef ENABLE_DEBUG_SGE
	dev_dbg(&pdev->dev, "QPLIB: Alloc HWQ slots 0x%x size 0x%x pages 0x%x",
		slots, size, pages);
#endif
	if (hwq_attr->type != HWQ_TYPE_QUEUE &&
	    npages == MAX_PBL_LVL_0_PGS && !hwq_attr->sginfo->nopte) {
		/* This request is Level 0, map PTE */
		rc = __alloc_pbl(res, &hwq->pbl[PBL_LVL_0], hwq_attr->sginfo);
		if (rc)
			goto fail;
		hwq->level = PBL_LVL_0;
#ifdef ENABLE_DEBUG_SGE
		dev_dbg(&pdev->dev, "QPLIB: PBL_LVL_0 DMA=0x%llx",
				hwq->pbl[PBL_LVL_0].pg_map_arr[0]);
#endif
		goto done;
	}

	if (npages >= MAX_PBL_LVL_0_PGS) {
		if (npages > MAX_PBL_LVL_1_PGS) {
			u32 flag = (hwq_attr->type == HWQ_TYPE_L2_CMPL) ?
				    0 : PTU_PTE_VALID;
			/* 2 levels of indirection */
			npbl = npages >> MAX_PBL_LVL_1_PGS_SHIFT;
			if (npages % BIT(MAX_PBL_LVL_1_PGS_SHIFT))
				npbl++;
			npde = npbl >> MAX_PDL_LVL_SHIFT;
			if(npbl % BIT(MAX_PDL_LVL_SHIFT))
				npde++;
			/* Alloc PDE pages */
			sginfo.pgsize = npde * PAGE_SIZE;
			sginfo.npages = 1;
			rc = __alloc_pbl(res, &hwq->pbl[PBL_LVL_0], &sginfo);
			if (rc)
				goto fail;

			/* Alloc PBL pages */
			sginfo.npages = npbl;
			sginfo.pgsize = PAGE_SIZE;
			rc = __alloc_pbl(res, &hwq->pbl[PBL_LVL_1], &sginfo);
			if (rc)
				goto fail;
			/* Fill PDL with PBL page pointers */
			dst_virt_ptr =
				(dma_addr_t **)hwq->pbl[PBL_LVL_0].pg_arr;
			src_phys_ptr = hwq->pbl[PBL_LVL_1].pg_map_arr;
			for (i = 0; i < hwq->pbl[PBL_LVL_1].pg_count; i++)
				dst_virt_ptr[0][i] = src_phys_ptr[i] | flag;
			/* Alloc or init PTEs */
			rc = __alloc_pbl(res, &hwq->pbl[PBL_LVL_2],
					 hwq_attr->sginfo);
			if (rc)
				goto fail;
			hwq->level = PBL_LVL_2;
			if (hwq_attr->sginfo->nopte)
				goto done;
#ifdef ENABLE_DEBUG_SGE
			dev_dbg(&pdev->dev, "QPLIB: PBL_LVL_1 alloc ");
			dev_dbg(&pdev->dev,
				"QPLIB: PBL_LVL_1.pg_count = 0x%x aux_pages=0x%x",
				hwq->pbl[PBL_LVL_1].pg_count, aux_pages);
#endif
			/* Fill PBLs with PTE pointers */
			dst_virt_ptr =
				(dma_addr_t **)hwq->pbl[PBL_LVL_1].pg_arr;
			src_phys_ptr = hwq->pbl[PBL_LVL_2].pg_map_arr;
			for (i = 0; i < hwq->pbl[PBL_LVL_2].pg_count; i++) {
				dst_virt_ptr[PTR_PG(i)][PTR_IDX(i)] =
					src_phys_ptr[i] | PTU_PTE_VALID;
#ifdef ENABLE_DEBUG_SGE
				dev_dbg(&pdev->dev,
					"QPLIB: dst_virt_ptr[%d] = 0x%llx", i,
					dst_virt_ptr[PTR_PG(i)][PTR_IDX(i)]);
#endif
			}
			if (hwq_attr->type == HWQ_TYPE_QUEUE) {
				/* Find the last pg of the size */
				i = hwq->pbl[PBL_LVL_2].pg_count;
				dst_virt_ptr[PTR_PG(i - 1)][PTR_IDX(i - 1)] |=
								  PTU_PTE_LAST;
				if (i > 1)
					dst_virt_ptr[PTR_PG(i - 2)]
						    [PTR_IDX(i - 2)] |=
						    PTU_PTE_NEXT_TO_LAST;
			}
#ifdef ENABLE_DEBUG_SGE
			dev_dbg(&pdev->dev, "QPLIB: PBL_LVL_2 alloc ");
			dev_dbg(&pdev->dev,
				"QPLIB: PBL_LVL_2.pg_count = 0x%x aux_pages=0x%x",
				hwq->pbl[PBL_LVL_2].pg_count, aux_pages);
			for (i = 0; i < hwq->pbl[PBL_LVL_2].pg_count; i++)
				dev_dbg(&pdev->dev,
					"QPLIB: dst_virt_ptr[%d] = 0x%llx", i,
					dst_virt_ptr[PTR_PG(i)][PTR_IDX(i)]);
#endif
		} else { /* pages < MAX_PBL_LVL_1_PGS npbl = 1, npde = 0 */
			u32 flag = (hwq_attr->type == HWQ_TYPE_L2_CMPL) ?
				    0 : PTU_PTE_VALID;

			/* 1 level of indirection */
			npbl = npages >> MAX_PBL_LVL_1_PGS_SHIFT;
			if (npages % BIT(MAX_PBL_LVL_1_PGS_SHIFT))
				npbl++;
			sginfo.npages = npbl;
			sginfo.pgsize = PAGE_SIZE;
			/* Alloc PBL page */
			rc = __alloc_pbl(res, &hwq->pbl[PBL_LVL_0], &sginfo);
			if (rc)
				goto fail;
			/* Alloc or init  PTEs */
			rc = __alloc_pbl(res, &hwq->pbl[PBL_LVL_1],
					 hwq_attr->sginfo);
			if (rc)
				goto fail;
			hwq->level = PBL_LVL_1;
			if (hwq_attr->sginfo->nopte)
				goto done;
#ifdef ENABLE_DEBUG_SGE
			dev_dbg(&pdev->dev, "QPLIB: PBL_LVL_1 alloc ");
			dev_dbg(&pdev->dev,
				"QPLIB: PBL_LVL_1.pg_count = 0x%x aux_pages=0x%x",
				hwq->pbl[PBL_LVL_1].pg_count, aux_pages);
#endif
			/* Fill PBL with PTE pointers */
			dst_virt_ptr =
				(dma_addr_t **)hwq->pbl[PBL_LVL_0].pg_arr;
			src_phys_ptr = hwq->pbl[PBL_LVL_1].pg_map_arr;
			for (i = 0; i < hwq->pbl[PBL_LVL_1].pg_count; i++)
				dst_virt_ptr[PTR_PG(i)][PTR_IDX(i)] =
					src_phys_ptr[i] | flag;
			if (hwq_attr->type == HWQ_TYPE_QUEUE) {
				/* Find the last pg of the size */
				i = hwq->pbl[PBL_LVL_1].pg_count;
				dst_virt_ptr[PTR_PG(i - 1)][PTR_IDX(i - 1)] |=
								  PTU_PTE_LAST;
				if (i > 1)
					dst_virt_ptr[PTR_PG(i - 2)]
						    [PTR_IDX(i - 2)] |=
						    PTU_PTE_NEXT_TO_LAST;
			}
#ifdef ENABLE_DEBUG_SGE
			for (i = 0; i < hwq->pbl[PBL_LVL_1].pg_count; i++)
				dev_dbg(&pdev->dev,
					"QPLIB: dst_virt_ptr[%d] = 0x%llx",
					i, dst_virt_ptr[PTR_PG(i)][PTR_IDX(i)]);
#endif
		}
	}
done:
	hwq->prod = 0;
	hwq->cons = 0;
	hwq->pdev = pdev;
	hwq->depth = hwq_attr->depth;
	hwq->max_elements = depth;
	hwq->element_size = stride;
	hwq->qe_ppg = (pg_size/stride);

	if (hwq->level >= PBL_LVL_MAX)
		goto fail;
	/* For direct access to the elements */
	lvl = hwq->level;
	if (hwq_attr->sginfo->nopte && hwq->level)
		lvl = hwq->level - 1;
	hwq->pbl_ptr = hwq->pbl[lvl].pg_arr;
	hwq->pbl_dma_ptr = hwq->pbl[lvl].pg_map_arr;
	spin_lock_init(&hwq->lock);

	return 0;
fail:
	bnxt_qplib_free_hwq(res, hwq);
	return -ENOMEM;
}

/* Context Tables */
void bnxt_qplib_free_hwctx(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_ctx *hctx;
	int i;

	hctx = res->hctx;
	bnxt_qplib_free_hwq(res, &hctx->qp_ctx.hwq);
	bnxt_qplib_free_hwq(res, &hctx->mrw_ctx.hwq);
	bnxt_qplib_free_hwq(res, &hctx->srq_ctx.hwq);
	bnxt_qplib_free_hwq(res, &hctx->cq_ctx.hwq);
	bnxt_qplib_free_hwq(res, &hctx->tim_ctx.hwq);
	for (i = 0; i < MAX_TQM_ALLOC_REQ; i++)
		bnxt_qplib_free_hwq(res, &hctx->tqm_ctx.qtbl[i]);
	/* restore original pde level before destroy */
	hctx->tqm_ctx.pde.level = hctx->tqm_ctx.pde_level;
	bnxt_qplib_free_hwq(res, &hctx->tqm_ctx.pde);
}

static int bnxt_qplib_alloc_tqm_rings(struct bnxt_qplib_res *res,
				      struct bnxt_qplib_ctx *hctx)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_sg_info sginfo = {};
	struct bnxt_qplib_tqm_ctx *tqmctx;
	int rc = 0;
	int i;

	tqmctx = &hctx->tqm_ctx;

	sginfo.pgsize = PAGE_SIZE;
	sginfo.pgshft = PAGE_SHIFT;
	hwq_attr.sginfo = &sginfo;
	hwq_attr.res = res;
	hwq_attr.type = HWQ_TYPE_CTX;
	hwq_attr.depth = 512;
	hwq_attr.stride = sizeof(u64);
	/* Alloc pdl buffer */
	rc = bnxt_qplib_alloc_init_hwq(&tqmctx->pde, &hwq_attr);
	if (rc)
		return rc;
	/* Save original pdl level */
	tqmctx->pde_level = tqmctx->pde.level;

	hwq_attr.stride = 1;
	for (i = 0; i < MAX_TQM_ALLOC_REQ; i++) {
		if (!tqmctx->qcount[i])
			continue;
		hwq_attr.depth = hctx->qp_ctx.max * tqmctx->qcount[i];
		rc = bnxt_qplib_alloc_init_hwq(&tqmctx->qtbl[i], &hwq_attr);
		if (rc)
			return rc;
	}

	return 0;
}

static void bnxt_qplib_map_tqm_pgtbl(struct bnxt_qplib_tqm_ctx *ctx)
{
	struct bnxt_qplib_hwq *qtbl_hwq;
	dma_addr_t *dma_ptr;
	__le64 **pbl_ptr, *ptr;
	int i, j, k;
	int fnz_idx = -1;
	int pg_count;

	pbl_ptr = (__le64 **)ctx->pde.pbl_ptr;

	for (i = 0, j = 0; i < MAX_TQM_ALLOC_REQ;
	     i++, j += MAX_TQM_ALLOC_BLK_SIZE) {
		qtbl_hwq = &ctx->qtbl[i];
		if (!qtbl_hwq->max_elements)
			continue;
		if (fnz_idx == -1)
			fnz_idx = i; /* first non-zero index */
		switch (qtbl_hwq->level) {
			case PBL_LVL_2:
			pg_count = qtbl_hwq->pbl[PBL_LVL_1].pg_count;
			for (k = 0; k < pg_count; k++) {
				ptr = &pbl_ptr[PTR_PG(j + k)][PTR_IDX(j + k)];
				dma_ptr = &qtbl_hwq->pbl[PBL_LVL_1].pg_map_arr[k];
				*ptr = cpu_to_le64(*dma_ptr | PTU_PTE_VALID);
			}
			break;
			case PBL_LVL_1:
			case PBL_LVL_0:
			default:
			ptr = &pbl_ptr[PTR_PG(j)][PTR_IDX(j)];
			*ptr = cpu_to_le64(qtbl_hwq->pbl[PBL_LVL_0].pg_map_arr[0] |
					   PTU_PTE_VALID);
			break;
		}
	}
	if (fnz_idx == -1)
		fnz_idx = 0;
	/* update pde level as per page table programming */
	ctx->pde.level = (ctx->qtbl[fnz_idx].level == PBL_LVL_2) ? PBL_LVL_2 :
			  ctx->qtbl[fnz_idx].level + 1;
}

static int bnxt_qplib_setup_tqm_rings(struct bnxt_qplib_res *res,
				      struct bnxt_qplib_ctx *hctx)
{
	int rc;

	rc = bnxt_qplib_alloc_tqm_rings(res, hctx);
	if (rc)
		return rc;

	bnxt_qplib_map_tqm_pgtbl(&hctx->tqm_ctx);

	return 0;
}

/*
 * Routine: bnxt_qplib_alloc_hwctx
 * Description:
 *     Context tables are memories which are used by the chip.
 *     The 6 tables defined are:
 *             QPC ctx - holds QP states
 *             MRW ctx - holds memory region and window
 *             SRQ ctx - holds shared RQ states
 *             CQ ctx - holds completion queue states
 *             TQM ctx - holds Tx Queue Manager context
 *             TIM ctx - holds timer context
 *     Depending on the size of the tbl requested, either a 1 Page Buffer List
 *     or a 1-to-2-stage indirection Page Directory List + 1 PBL is used
 *     instead.
 *     Table might be employed as follows:
 *             For 0      < ctx size <= 1 PAGE, 0 level of ind is used
 *             For 1 PAGE < ctx size <= 512 entries size, 1 level of ind is used
 *             For 512    < ctx size <= MAX, 2 levels of ind is used
 * Returns:
 *     0 if success, else -ERRORS
 */
int bnxt_qplib_alloc_hwctx(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_sg_info sginfo = {};
	struct bnxt_qplib_ctx *hctx;
	struct bnxt_qplib_hwq *hwq;
	int rc = 0;

	hctx = res->hctx;
	/* QPC Tables */
	sginfo.pgsize = PAGE_SIZE;
	sginfo.pgshft = PAGE_SHIFT;
	hwq_attr.sginfo = &sginfo;

	hwq_attr.res = res;
	hwq_attr.depth = hctx->qp_ctx.max;
	hwq_attr.stride = BNXT_QPLIB_MAX_QP_CTX_ENTRY_SIZE;
	hwq_attr.type = HWQ_TYPE_CTX;
	hwq = &hctx->qp_ctx.hwq;
	rc = bnxt_qplib_alloc_init_hwq(hwq, &hwq_attr);
	if (rc)
		goto fail;

	/* MRW Tables */
	hwq_attr.depth = hctx->mrw_ctx.max;
	hwq_attr.stride = BNXT_QPLIB_MAX_MRW_CTX_ENTRY_SIZE;
	hwq = &hctx->mrw_ctx.hwq;
	rc = bnxt_qplib_alloc_init_hwq(hwq, &hwq_attr);
	if (rc)
		goto fail;

	/* SRQ Tables */
	hwq_attr.depth = hctx->srq_ctx.max;
	hwq_attr.stride = BNXT_QPLIB_MAX_SRQ_CTX_ENTRY_SIZE;
	hwq = &hctx->srq_ctx.hwq;
	rc = bnxt_qplib_alloc_init_hwq(hwq, &hwq_attr);
	if (rc)
		goto fail;

	/* CQ Tables */
	hwq_attr.depth = hctx->cq_ctx.max;
	hwq_attr.stride = BNXT_QPLIB_MAX_CQ_CTX_ENTRY_SIZE;
	hwq = &hctx->cq_ctx.hwq;
	rc = bnxt_qplib_alloc_init_hwq(hwq, &hwq_attr);
	if (rc)
		goto fail;

	/* TQM Buffer */
	rc = bnxt_qplib_setup_tqm_rings(res, hctx);
	if (rc)
		goto fail;
	/* TIM Buffer */
	hwq_attr.depth = hctx->qp_ctx.max * 16;
	hwq_attr.stride = 1;
	hwq = &hctx->tim_ctx.hwq;
	rc = bnxt_qplib_alloc_init_hwq(hwq, &hwq_attr);
	if (rc)
		goto fail;

	return 0;
fail:
	bnxt_qplib_free_hwctx(res);
	return rc;
}

static void bnxt_qplib_free_sgid_tbl(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_sgid_tbl *sgid_tbl;

	sgid_tbl = &res->sgid_tbl;

	kfree(sgid_tbl->tbl);
	sgid_tbl->tbl = NULL;
	kfree(sgid_tbl->hw_id);
	sgid_tbl->hw_id = NULL;
	kfree(sgid_tbl->ctx);
	sgid_tbl->ctx = NULL;
	kfree(sgid_tbl->vlan);
	sgid_tbl->vlan = NULL;
	sgid_tbl->max = 0;
	sgid_tbl->active = 0;
}

static void bnxt_qplib_free_reftbls(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_reftbl *tbl;

	tbl = &res->reftbl.srqref;
	vfree(tbl->rec);

	tbl = &res->reftbl.cqref;
	vfree(tbl->rec);

	tbl = &res->reftbl.qpref;
	vfree(tbl->rec);
}

static int bnxt_qplib_alloc_reftbl(struct bnxt_qplib_reftbl *tbl, u32 max)
{
	tbl->max = max;
	tbl->rec = vzalloc(sizeof(*tbl->rec) * max);
	if (!tbl->rec)
		return -ENOMEM;
	spin_lock_init(&tbl->lock);
	return 0;
}

static int bnxt_qplib_alloc_reftbls(struct bnxt_qplib_res *res,
				    struct bnxt_qplib_dev_attr *dattr)
{
	u32 max_cq = BNXT_QPLIB_MAX_CQ_COUNT;
	struct bnxt_qplib_reftbl *tbl;
	u32 res_cnt;
	int rc;

	/*
	 * Allocating one extra entry  to hold QP1 info.
	 * Store QP1 info at the last entry of the table.
	 * Decrement the tbl->max by one so that modulo
	 * operation to get the qp table index from qp id
	 * returns any value between 0 and max_qp-1
	 */
	res_cnt = max_t(u32, BNXT_QPLIB_MAX_QPC_COUNT + 1, dattr->max_qp);
	tbl = &res->reftbl.qpref;
	rc = bnxt_qplib_alloc_reftbl(tbl, res_cnt);
	if (rc)
		return rc;
	tbl->max--;

	if (_is_chip_gen_p5_p7(res->cctx))
		max_cq = BNXT_QPLIB_MAX_CQ_COUNT_P5;
	else if (_is_chip_p8(res->cctx))
		max_cq = dattr->max_cq;
	res_cnt = max_t(u32, max_cq, dattr->max_cq);
	tbl = &res->reftbl.cqref;
	rc = bnxt_qplib_alloc_reftbl(tbl, res_cnt);
	if (rc)
		goto free_qpref_tbl;

	res_cnt = max_t(u32, BNXT_QPLIB_MAX_SRQC_COUNT, dattr->max_cq);
	tbl = &res->reftbl.srqref;
	rc = bnxt_qplib_alloc_reftbl(tbl, BNXT_QPLIB_MAX_SRQC_COUNT);
	if (rc)
		goto free_cqref_tbl;

	return 0;
free_cqref_tbl:
	tbl = &res->reftbl.cqref;
	vfree(tbl->rec);
free_qpref_tbl:
	tbl = &res->reftbl.qpref;
	vfree(tbl->rec);
	return rc;
}

static int bnxt_qplib_alloc_sgid_tbl(struct bnxt_qplib_res *res, u16 max)
{
	struct bnxt_qplib_sgid_tbl *sgid_tbl;
	u32 i;

	sgid_tbl = &res->sgid_tbl;

	sgid_tbl->tbl = kcalloc(max, sizeof(*sgid_tbl->tbl), GFP_KERNEL);
	if (!sgid_tbl->tbl)
		return -ENOMEM;

	sgid_tbl->hw_id = kcalloc(max, sizeof(u32), GFP_KERNEL);
	if (!sgid_tbl->hw_id)
		goto free_tbl;

	sgid_tbl->ctx = kcalloc(max, sizeof(void *), GFP_KERNEL);
	if (!sgid_tbl->ctx)
		goto free_hw_id;

	sgid_tbl->vlan = kcalloc(max, sizeof(u8), GFP_KERNEL);
	if (!sgid_tbl->vlan)
		goto free_ctx;

	sgid_tbl->max = max;

	for (i = 0; i < sgid_tbl->max; i++)
		sgid_tbl->tbl[i].vlan_id = 0xffff;
	memset(sgid_tbl->hw_id, -1, sizeof(u16) * sgid_tbl->max);
	return 0;
free_ctx:
	kfree(sgid_tbl->ctx);
free_hw_id:
	kfree(sgid_tbl->hw_id);
free_tbl:
	kfree(sgid_tbl->tbl);
	return -ENOMEM;
};

static void bnxt_qplib_cleanup_sgid_tbl(struct bnxt_qplib_res *res,
					struct bnxt_qplib_sgid_tbl *sgid_tbl)
{
	int i;

	for (i = 0; i < sgid_tbl->max; i++) {
		if (memcmp(&sgid_tbl->tbl[i], &bnxt_qplib_gid_zero,
			   sizeof(bnxt_qplib_gid_zero)))
			bnxt_qplib_del_sgid(sgid_tbl, &sgid_tbl->tbl[i].gid,
					    sgid_tbl->tbl[i].vlan_id, true);
	}
	memset(sgid_tbl->tbl, 0, sizeof(*sgid_tbl->tbl) * sgid_tbl->max);
	memset(sgid_tbl->hw_id, -1, sizeof(u16) * sgid_tbl->max);
	memset(sgid_tbl->vlan, 0, sizeof(u8) * sgid_tbl->max);
	sgid_tbl->active = 0;
}

/* PDs */
int bnxt_qplib_alloc_pd(struct bnxt_qplib_res *res, struct bnxt_qplib_pd *pd)
{
	u32 bit_num;
	struct bnxt_qplib_pd_tbl *pdt = &res->pd_tbl;

	mutex_lock(&res->pd_tbl_lock);
	bit_num = find_first_bit(pdt->tbl, pdt->max);
	if (bit_num == pdt->max - 1) {/* Last bit is reserved */
		mutex_unlock(&res->pd_tbl_lock);
		return -ENOMEM;
	}

	/* Found unused PD */
	clear_bit(bit_num, pdt->tbl);
	pd->id = bit_num;

	mutex_unlock(&res->pd_tbl_lock);
	return 0;
}

int bnxt_qplib_dealloc_pd(struct bnxt_qplib_res *res,
			  struct bnxt_qplib_pd_tbl *pdt,
			  struct bnxt_qplib_pd *pd)
{
	mutex_lock(&res->pd_tbl_lock);
	if (test_and_set_bit(pd->id, pdt->tbl)) {
		dev_warn(&res->pdev->dev, "Freeing an unused PD? pdn = %d",
			 pd->id);
		mutex_unlock(&res->pd_tbl_lock);
		return -EINVAL;
	}
	/* Reset to reserved pdid. */
	pd->id = pdt->max - 1;

	mutex_unlock(&res->pd_tbl_lock);
	return 0;
}

static void bnxt_qplib_free_pd_tbl(struct bnxt_qplib_pd_tbl *pdt)
{
	kfree(pdt->tbl);
	pdt->tbl = NULL;
	pdt->max = 0;
}

static int bnxt_qplib_alloc_pd_tbl(struct bnxt_qplib_res *res, u32 max)
{
	struct bnxt_qplib_pd_tbl *pdt;
	u32 bytes;

	pdt = &res->pd_tbl;

	max++; /* One extra for reserved pdid. */
	bytes = DIV_ROUND_UP(max, 8);

	if (!bytes)
		bytes = 1;
	pdt->tbl = kmalloc(bytes, GFP_KERNEL);
	if (!pdt->tbl) {
		dev_err(&res->pdev->dev,
			"QPLIB: PD tbl allocation failed for size = %d", bytes);
		return -ENOMEM;
	}
	pdt->max = max;
	memset((u8 *)pdt->tbl, 0xFF, bytes);
	mutex_init(&res->pd_tbl_lock);

	return 0;
}

void bnxt_qplib_alloc_kernel_dpi(struct bnxt_qplib_res *res,
				 struct bnxt_qplib_dpi *dpi)
{
	struct bnxt_qplib_dpi_tbl *dpit = &res->dpi_tbl;

	/*
	 * Privileged dbr was already mapped at bar base off
	 * ucreg.offset. It is sharing the same normal DB page
	 * with L2 driver. Here we only need to initialize it.
	 */
	dpi->umdbr = dpit->ucreg.bar_base + dpit->ucreg.offset;
	dpi->dbr = dpit->priv_db;
	dpi->dpi = dpit->ucreg.offset / PAGE_SIZE;
	dpi->type = BNXT_QPLIB_DPI_TYPE_KERNEL;
}

void bnxt_qplib_dealloc_kernel_dpi(struct bnxt_qplib_dpi *dpi)
{
	memset(dpi, 0, sizeof(*dpi));
}

/* DPIs */
int bnxt_qplib_alloc_uc_dpi(struct bnxt_qplib_res *res, struct bnxt_qplib_dpi *dpi)
{
	struct bnxt_qplib_dpi_tbl *dpit = &res->dpi_tbl;
	struct bnxt_qplib_reg_desc *reg;
	u32 bit_num;
	int rc = 0;

	reg = &dpit->wcreg;
	mutex_lock(&res->dpi_tbl_lock);
	bit_num = find_first_bit(dpit->tbl, dpit->max);
	if (bit_num >= dpit->max) {
		rc = -ENOMEM;
		goto unlock;
	}
	/* Found unused DPI */
	clear_bit(bit_num, dpit->tbl);
	dpi->bit = bit_num;
	dpi->dpi = bit_num + (reg->offset - dpit->ucreg.offset) / PAGE_SIZE;
	dpi->umdbr = reg->bar_base + reg->offset + bit_num * PAGE_SIZE;
unlock:
	mutex_unlock(&res->dpi_tbl_lock);
	return rc;
}

int bnxt_qplib_free_uc_dpi(struct bnxt_qplib_res *res, struct bnxt_qplib_dpi *dpi)
{
	struct bnxt_qplib_dpi_tbl *dpit = &res->dpi_tbl;
	int rc = 0;

	mutex_lock(&res->dpi_tbl_lock);
	if (dpi->bit >= dpit->max) {
		rc = -EINVAL;
		goto unlock;
	}

	if (test_and_set_bit(dpi->bit, dpit->tbl))
		rc = -EINVAL;
	memset(dpi, 0, sizeof(*dpi));
unlock:
	mutex_unlock(&res->dpi_tbl_lock);
	return rc;
}

int bnxt_qplib_alloc_dpi(struct bnxt_qplib_res	*res,
			 struct bnxt_qplib_dpi	*dpi,
			 void *app, enum bnxt_qplib_dpi_type type)
{
	struct bnxt_qplib_dpi_tbl *dpit = &res->dpi_tbl;
	struct bnxt_qplib_reg_desc *reg;
	u32 bit_num;
	u64 umaddr;
	int rc = 0;

	reg = &dpit->wcreg;
	mutex_lock(&res->dpi_tbl_lock);
	if (!dpit->tbl ||
	    (type == BNXT_QPLIB_DPI_TYPE_WC &&
	     BNXT_RE_PPP_ENABLED(res->cctx) && !dpit->avail_ppp)) {
		rc = -ENOMEM;
		goto exit;
	}
	bit_num = find_first_bit(dpit->tbl, dpit->max);
	if (bit_num >= dpit->max) {
		rc = -ENOMEM;
		goto exit;
	}
	/* Found unused DPI */
	clear_bit(bit_num, dpit->tbl);
	dpit->app_tbl[bit_num] = app;
	dpi->bit = bit_num;
	dpi->dpi = bit_num + (reg->offset / PAGE_SIZE);
	umaddr = reg->bar_base + dpit->ucreg.offset + (dpi->dpi * PAGE_SIZE);

	dpi->umdbr = umaddr;
	if (type == BNXT_QPLIB_DPI_TYPE_WC) {
		dpi->dbr = ioremap_wc(umaddr, PAGE_SIZE);
		if (BNXT_RE_PPP_ENABLED(res->cctx) && dpi->dbr)
			dpit->avail_ppp--;
	} else {
		dpi->dbr = ioremap(umaddr, PAGE_SIZE);
	}
	if (!dpi->dbr) {
		dev_err(&res->pdev->dev, "QPLIB: DB remap failed, type = %d\n",
			type);
		rc = -ENOMEM;
		/* Cleanup the dpi->tbl on failure */
		set_bit(bit_num, dpit->tbl);
		dpit->app_tbl[bit_num] = NULL;
		goto exit;
	}
	dpi->type = type;
exit:
	mutex_unlock(&res->dpi_tbl_lock);
	return rc;
}

int bnxt_qplib_dealloc_dpi(struct bnxt_qplib_res *res,
			   struct bnxt_qplib_dpi *dpi)
{
	struct bnxt_qplib_dpi_tbl *dpit = &res->dpi_tbl;
	int rc = 0;

	mutex_lock(&res->dpi_tbl_lock);
	if (dpi->bit >= dpit->max) {
		dev_warn(&res->pdev->dev,
			 "Invalid DPI? dpi = %d, bit = %d\n",
			 dpi->dpi, dpi->bit);
		rc = -EINVAL;
		goto fail;
	}

	if (dpi->dpi) {
		if (dpi->type == BNXT_QPLIB_DPI_TYPE_WC &&
		    BNXT_RE_PPP_ENABLED(res->cctx) && dpi->dbr)
			dpit->avail_ppp++;
		pci_iounmap(res->pdev, dpi->dbr);
	}

	if (test_and_set_bit(dpi->bit, dpit->tbl)) {
		dev_warn(&res->pdev->dev,
			 "Freeing an unused DPI? dpi = %d, bit = %d\n",
			 dpi->dpi, dpi->bit);
		rc = -EINVAL;
		goto fail;
	}
	if (dpit->app_tbl)
		dpit->app_tbl[dpi->bit] = NULL;
	memset(dpi, 0, sizeof(*dpi));
fail:
	mutex_unlock(&res->dpi_tbl_lock);
	return rc;
}

static void bnxt_qplib_free_dpi_tbl(struct bnxt_qplib_dpi_tbl *dpit)
{
	kfree(dpit->tbl);
	kfree(dpit->app_tbl);
	dpit->tbl = NULL;
	dpit->app_tbl = NULL;
	dpit->max = 0;
}

static int bnxt_qplib_alloc_dpi_tbl(struct bnxt_qplib_res *res,
				    struct bnxt_qplib_dev_attr *dev_attr)
{
	struct bnxt_qplib_dpi_tbl *dpit;
	struct bnxt_qplib_reg_desc *reg;
	unsigned long bar_len;
	u32 dbr_offset;
	u32 bytes;

	dpit = &res->dpi_tbl;
	reg = &dpit->wcreg;

	if (!_is_chip_p5_plus(res->cctx)) {
		/* Offset should come from L2 driver */
		dbr_offset = dev_attr->l2_db_size;
		dpit->ucreg.offset = dbr_offset;
		dpit->wcreg.offset = dbr_offset;
	}

	bar_len = pci_resource_len(res->pdev, reg->bar_id);
	if (reg->offset >= bar_len) {
		dev_warn(&res->pdev->dev, "No PCI resource reserved for RoCE apps.\n");
		dpit->app_tbl = NULL;
		dpit->tbl = NULL;
		dpit->max = 0;
		dpit->avail_ppp = 0;
		goto done;
	}

	dpit->max = (bar_len - reg->offset) / PAGE_SIZE;
	if (dev_attr->max_dpi)
		dpit->max = min_t(u32, dpit->max, dev_attr->max_dpi);

	dpit->app_tbl = kzalloc(dpit->max * sizeof(void*), GFP_KERNEL);
	if (!dpit->app_tbl) {
		dev_err(&res->pdev->dev,
			"QPLIB: DPI app tbl allocation failed");
		return -ENOMEM;
	}

	bytes = (dpit->max + 7) >> 3;
	dpit->tbl = kmalloc(bytes, GFP_KERNEL);
	if (!dpit->tbl) {
		kfree(dpit->app_tbl);
		dev_err(&res->pdev->dev,
			"QPLIB: DPI tbl allocation failed for size = %d",
			bytes);
		return -ENOMEM;
	}

	memset((u8 *)dpit->tbl, 0xFF, bytes);
	/*
	 * Allocating the 512 extended PPP pages is based on first
	 * come, first served policy. Any function could use number
	 * of pages from 0 to all.
	 */
	if (BNXT_RE_PPP_ENABLED(res->cctx))
		dpit->avail_ppp = BNXT_QPLIB_MAX_EXTENDED_PPP_PAGES;

done:
	mutex_init(&res->dpi_tbl_lock);
	dpit->priv_db = dpit->ucreg.bar_reg + dpit->ucreg.offset;

	return 0;
}

/* Stats */
void bnxt_qplib_free_stat_mem(struct bnxt_qplib_res *res,
			      struct bnxt_qplib_stats *stats)
{
	struct pci_dev *pdev;

	pdev = res->pdev;
	if (stats->cpu_addr)
		dma_free_coherent(&pdev->dev, stats->size,
				  stats->cpu_addr, stats->dma_handle);
	kfree(stats->sw_stats);

	memset(stats, 0, sizeof(*stats));
	stats->fw_id = -1;
}

int bnxt_qplib_alloc_stat_mem(struct pci_dev *pdev,
			      struct bnxt_qplib_chip_ctx *cctx,
			      struct bnxt_qplib_stats *stats)
{
	memset(stats, 0, sizeof(*stats));
	stats->fw_id = -1;
	stats->size = cctx->hw_stats_size;
	stats->cpu_addr = dma_alloc_coherent(&pdev->dev, stats->size,
					     &stats->dma_handle, GFP_KERNEL);
	if (!stats->cpu_addr) {
		dev_err(&pdev->dev, "QPLIB: Stats DMA allocation failed");
		return -ENOMEM;
	}

	stats->sw_stats = kzalloc(stats->size, GFP_KERNEL);
	if (!stats->sw_stats)
		return -ENOMEM;

	return 0;
}

void bnxt_qplib_init_stats_ext_ctx(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_stats_ext_ctx *ctx;
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct bnxt_qplib_ctx *hctx;

	hctx = res->hctx;
	ctx = &hctx->stats_ext_ctx;
	if (ctx->cpu_addr)
		bnxt_qplib_free_stats_ext_ctx_mem(rcfw, ctx);
	memset(ctx, 0, sizeof(struct bnxt_qplib_stats_ext_ctx));
	ctx->update_period_ms = BNXT_QPLIB_UPDATE_PERIOD_MS;
	ctx->xid = INVALID_STATS_EXT_CTX_ID;
	ctx->steering_tag = BNXT_RE_STEERING_TO_HOST;
}

int bnxt_qplib_get_stats_ext_ctx_id(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_stats_ext_ctx *ctx;
	struct bnxt_qplib_ctx *hctx;

	hctx = res->hctx;
	ctx = &hctx->stats_ext_ctx;
	return ctx->xid;
}

int bnxt_qplib_alloc_stats_ext_ctx(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_stats_ext_ctx *ctx;
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct bnxt_qplib_ctx *hctx;
	int rc;

	hctx = res->hctx;

	ctx = &hctx->stats_ext_ctx;
	rc = bnxt_qplib_alloc_stats_ext_ctx_res(rcfw, ctx);
	if (rc)
		goto err;
	return rc;
err:
	ctx->xid = INVALID_STATS_EXT_CTX_ID;
	return rc;
}

int bnxt_qplib_dealloc_stats_ext_ctx(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_stats_ext_ctx *ctx;
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct bnxt_qplib_ctx *hctx;

	hctx = res->hctx;

	ctx = &hctx->stats_ext_ctx;
	return bnxt_qplib_dealloc_stats_ext_ctx_res(rcfw, ctx);
}

/* Resource */
int bnxt_qplib_stop_res(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_stop_func_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_stop_func req = {};
	int rc;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_STOP_FUNC,
				 sizeof(req));
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	return rc;
}

void bnxt_qplib_clear_tbls(struct bnxt_qplib_res *res)
{
	bnxt_qplib_cleanup_sgid_tbl(res, &res->sgid_tbl);
}

void bnxt_qplib_free_tbls(struct bnxt_qplib_res *res)
{
	bnxt_qplib_free_sgid_tbl(res);
	bnxt_qplib_free_pd_tbl(&res->pd_tbl);
	bnxt_qplib_free_dpi_tbl(&res->dpi_tbl);
	bnxt_qplib_free_reftbls(res);
}

int bnxt_qplib_alloc_tbls(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_dev_attr *dev_attr;
	int rc = 0;

	dev_attr = res->dattr;

	rc = bnxt_qplib_alloc_reftbls(res, dev_attr);
	if (rc)
		return rc;

	rc = bnxt_qplib_alloc_sgid_tbl(res, dev_attr->max_sgid);
	if (rc)
		goto free_reftbls;

	rc = bnxt_qplib_alloc_pd_tbl(res, dev_attr->max_pd);
	if (rc)
		goto free_sgidtbl;

	rc = bnxt_qplib_alloc_dpi_tbl(res, dev_attr);
	if (rc)
		goto free_pdtbl;

	return 0;
free_pdtbl:
	bnxt_qplib_free_pd_tbl(&res->pd_tbl);
free_sgidtbl:
	bnxt_qplib_free_sgid_tbl(res);
free_reftbls:
	bnxt_qplib_free_reftbls(res);
	return rc;
}

void bnxt_qplib_unmap_db_bar(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_reg_desc *reg;

	reg = &res->dpi_tbl.ucreg;
	if (reg->bar_reg)
		pci_iounmap(res->pdev, reg->bar_reg);
	reg->bar_reg = NULL;
	reg->bar_base = 0;
	reg->len = 0;
	reg->bar_id = 0; /* Zero? or ff */
}

int bnxt_qplib_map_db_bar(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_reg_desc *ucreg;
	struct bnxt_qplib_reg_desc *wcreg;

	wcreg = &res->dpi_tbl.wcreg;
	wcreg->bar_id = RCFW_DBR_PCI_BAR_REGION;
	wcreg->bar_base = pci_resource_start(res->pdev, wcreg->bar_id);
	/* No need to set the wcreg->len here */

	ucreg = &res->dpi_tbl.ucreg;
	ucreg->bar_id = RCFW_DBR_PCI_BAR_REGION;
	ucreg->bar_base = pci_resource_start(res->pdev, ucreg->bar_id);
	ucreg->len = ucreg->offset + PAGE_SIZE;
	if (!ucreg->len) {
		dev_err(&res->pdev->dev, "QPLIB: invalid dbr length %d",
			(int)ucreg->len);
		return -EINVAL;
	}
	ucreg->bar_reg = ioremap(ucreg->bar_base, ucreg->len);
	if (!ucreg->bar_reg) {
		dev_err(&res->pdev->dev, "privileged dpi map failed!");
		return -ENOMEM;
	}

	return 0;
}

int bnxt_qplib_enable_atomic_ops_to_root(struct bnxt_en_dev *en_dev, bool is_virtfn)
{
	u16 ctl2;
	struct pci_dev *dev = en_dev->pdev;

	if (_is_chip_num_p8(en_dev->chip_num))
		return -EOPNOTSUPP;

	if (is_virtfn)
		return -EOPNOTSUPP;

	if(pci_enable_atomic_ops_to_root(dev, PCI_EXP_DEVCAP2_ATOMIC_COMP32) &&
	   pci_enable_atomic_ops_to_root(dev, PCI_EXP_DEVCAP2_ATOMIC_COMP64))
		return -EOPNOTSUPP;

	pcie_capability_read_word(dev, PCI_EXP_DEVCTL2, &ctl2);
	return !(ctl2 & PCI_EXP_DEVCTL2_ATOMIC_REQ);
}
