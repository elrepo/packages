/*
 * Copyright (c) 2024, Broadcom. All rights reserved.  The term
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
 *
 * Description: Compat file for supporting multiple distros
 */

#include <linux/types.h>
#include <linux/version.h>
#include <rdma/ib_verbs.h>

#include "bnxt_re.h"
#include "hdbr.h"
#include "bnxt_re-abi.h"

#ifndef HAVE_RDMA_XARRAY_MMAP_V2
static struct bnxt_re_cq *is_bnxt_re_cq_page(struct bnxt_re_ucontext *uctx,
					     u64 pg_off)
{
	struct bnxt_re_cq *cq = NULL, *tmp_cq;

	if (!_is_chip_p7(uctx->rdev->chip_ctx))
		return NULL;

	mutex_lock(&uctx->cq_lock);
	list_for_each_entry(tmp_cq, &uctx->cq_list, cq_list) {
		if (((u64)tmp_cq->cq_toggle_page >> PAGE_SHIFT) == pg_off) {
			cq = tmp_cq;
			break;
		}
	}
	mutex_unlock(&uctx->cq_lock);
	return cq;
}

static struct bnxt_re_srq *is_bnxt_re_srq_page(struct bnxt_re_ucontext *uctx,
					       u64 pg_off)
{
	struct bnxt_re_srq *srq = NULL, *tmp_srq;

	if (!_is_chip_p7(uctx->rdev->chip_ctx))
		return NULL;

	mutex_lock(&uctx->srq_lock);
	list_for_each_entry(tmp_srq, &uctx->srq_list, srq_list) {
		if (((u64)tmp_srq->srq_toggle_page >> PAGE_SHIFT) == pg_off) {
			srq = tmp_srq;
			break;
		}
	}
	mutex_unlock(&uctx->srq_lock);
	return srq;
}

/* Helper function to mmap the virtual memory from user app */
int bnxt_re_mmap(struct ib_ucontext *ib_uctx, struct vm_area_struct *vma)
{
	struct bnxt_re_ucontext *uctx = to_bnxt_re(ib_uctx,
						   struct bnxt_re_ucontext,
						   ib_uctx);
	struct bnxt_re_dev *rdev = uctx->rdev;
	struct bnxt_re_cq *cq = NULL;
	struct bnxt_re_srq *srq;
	dma_addr_t dma_handle;
	u8 user_mmap_hint;
	void *cpu_addr;
	int rc = 0;
	u64 pfn;

#ifndef HAVE_RDMA_USER_MMAP_IO
	if (vma->vm_end - vma->vm_start != PAGE_SIZE)
		return -EINVAL;
#endif

	user_mmap_hint = (u8)vma->vm_pgoff;
	if (vma->vm_flags & VM_LOCKED) {
		/* This is for mapping DB copy memory page */
		cpu_addr = (void *)(vma->vm_pgoff << PAGE_SHIFT);
		dma_handle = bnxt_re_hdbr_kaddr_to_dma((struct bnxt_re_hdbr_app *)uctx->hdbr_app,
						       (u64)cpu_addr);
		/* Resetting vm_pgoff is required since vm_pgoff must be an actual offset.
		 * dma_mmap_coherent does sanity check.
		 * We are using vm_pgoff for other purpose like sending
		 * kernel virtual address through vm_pgoff.
		 */
		vma->vm_pgoff = 0;
		rc = dma_mmap_coherent(&rdev->en_dev->pdev->dev, vma, cpu_addr, dma_handle,
				       (vma->vm_end - vma->vm_start));
		if (rc) {
			dev_err(rdev_to_dev(rdev),
				"DB copy memory mapping failed!");
			rc = -EAGAIN;
		}
		user_mmap_hint = BNXT_RE_MMAP_HDBR_BASE;
		goto out;
	}

	switch (vma->vm_pgoff) {
	case BNXT_RE_MMAP_UC_DB:
		pfn = (uctx->dpi.umdbr >> PAGE_SHIFT);
		if (!pfn)
			return -EFAULT;
		break;
	case BNXT_RE_MMAP_WC_DB:
		vma->vm_page_prot =
			pgprot_writecombine(vma->vm_page_prot);
		pfn = (uctx->wcdpi.umdbr >> PAGE_SHIFT);
		if (!pfn)
			return -EFAULT;
		break;
	case BNXT_RE_MMAP_DBR_PACING_BAR:
		pfn = (rdev->dbr_pacing_bar >> PAGE_SHIFT);
		if (!pfn)
			return -EFAULT;
		break;
	case BNXT_RE_MMAP_DBR_PAGE:
		/* Driver doesn't expect write access request */
		if (vma->vm_flags & VM_WRITE)
			return -EFAULT;
		pfn = virt_to_phys(rdev->dbr_pacing_page) >> PAGE_SHIFT;
		if (!pfn)
			return -EFAULT;
		break;
	case BNXT_RE_MMAP_DB_RECOVERY_PAGE:
		pfn = virt_to_phys(uctx->dbr_recov_cq_page) >> PAGE_SHIFT;
		if (!pfn)
			return -EFAULT;
		break;
	default:
		cq = is_bnxt_re_cq_page(uctx, vma->vm_pgoff);
		if (cq) {
			pfn = virt_to_phys((void *)cq->cq_toggle_page) >> PAGE_SHIFT;
			rc = remap_pfn_compat(ib_uctx, vma, pfn);
			if (rc) {
				dev_err(rdev_to_dev(rdev),
					"CQ page mapping failed!");
				rc = -EAGAIN;
			}
			user_mmap_hint = BNXT_RE_MMAP_TOGGLE_PAGE;
			goto out;
		}

		srq = is_bnxt_re_srq_page(uctx, vma->vm_pgoff);
		if (srq) {
			pfn = virt_to_phys((void *)srq->srq_toggle_page) >> PAGE_SHIFT;
			rc = remap_pfn_compat(ib_uctx, vma, pfn);
			if (rc) {
				dev_err(rdev_to_dev(rdev),
					"SRQ page mapping failed!");
				rc = -EAGAIN;
			}
			user_mmap_hint = BNXT_RE_MMAP_TOGGLE_PAGE;
			goto out;
		}

		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		pfn = vma->vm_pgoff;
		break;
	}

	rc = remap_pfn_compat(ib_uctx, vma, pfn);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "DPI mapping failed!");
		return -EAGAIN;
	}
	rc = __bnxt_re_set_vma_data(uctx, vma);
out:
	dev_dbg(NULL, ">> %s vm_pgoff 0x%lx %s %d uctx->ib_uctx 0x%lx\n",
		bnxt_re_mmap_str(user_mmap_hint), vma->vm_pgoff,
		__func__, __LINE__, (unsigned long)&uctx->ib_uctx);
	return rc;
}
#endif

/**
 * bnxt_re_mmap_entry_insert_compat - Insert rdma entry into
 * xarray.
 * @uctx: user context of the driver
 * @cpu_addr: cpu address of kernel memory.
 * @dma_addr: dma address of kernel memory.
 * @offset: offset of rdma entry xarray (multiple of page size).
 * @rdma_entry_save: rdma_entry pointer used in mmap_free.
 *
 * Return true if the rdma entry is inserted into an xarray.
 *
 * This function is for compatibility between old api vs new api.
 * In case of old API, this function returns cpu_address into
 * offset and it always returns true.
 *
 * Note - offset is useful if library is passing the same value to
 * the driver at the time of mmap.
 * In some of the cases, like SH_PAGE, library pass FIXED mmap_hint,
 * and whatever offset driver passes to the library doesn't care.
 *
 * Currently below four are fixed mmap_hint which we want to continue
 * to keep library-driver interoperability intact.
 *
 * BNXT_RE_MMAP_WC_DB
 * BNXT_RE_MMAP_DBR_PAGE
 * BNXT_RE_MMAP_DB_RECOVERY_PAGE
 */
bool bnxt_re_mmap_entry_insert_compat(struct bnxt_re_ucontext *uctx, u64 cpu_addr,
				      dma_addr_t dma_addr, u8 user_mmap_hint, u64 *offset,
				      struct rdma_user_mmap_entry **rdma_entry_save)
{
#ifdef HAVE_RDMA_XARRAY_MMAP_V2
	struct bnxt_re_user_mmap_entry *entry;

	entry = bnxt_re_mmap_entry_insert(uctx, cpu_addr, dma_addr, user_mmap_hint, offset);
	if (entry) {
		*rdma_entry_save = &entry->rdma_entry;
		return true;
	}

	return false;
#else
	if (offset) {
		if (user_mmap_hint >= BNXT_RE_MMAP_TOGGLE_PAGE)
			*offset = cpu_addr;
		else
			*offset = user_mmap_hint * PAGE_SIZE;

		dev_dbg(NULL,
			"<> %s %s %d uctx->ib_uctx 0x%lx cpu 0x%lx dma offset 0x%lx\n",
			bnxt_re_mmap_str(user_mmap_hint),
			__func__, __LINE__, (unsigned long)&uctx->ib_uctx,
			(unsigned long)cpu_addr, (unsigned long)*offset);
	}


	return true;
#endif
}

void bnxt_re_mmap_entry_remove_compat(struct rdma_user_mmap_entry *entry)
{
#ifdef HAVE_RDMA_XARRAY_MMAP_V2
	return rdma_user_mmap_entry_remove(entry);
#else
	return;
#endif
}
