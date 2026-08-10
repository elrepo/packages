/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2022-2023 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hdbr.h"
#include "bnxt_debugfs.h"

/*
 * Map DB type to DB copy group type
 */
int bnxt_hdbr_get_grp(u64 db_val)
{
	db_val &= DBC_TYPE_MASK;

	switch (db_val) {
	case DBR_TYPE_SQ:
		return DBC_GROUP_SQ;

	case DBR_TYPE_RQ:
		return DBC_GROUP_RQ;

	case DBR_TYPE_SRQ:
	case DBR_TYPE_SRQ_ARM:
	case DBR_TYPE_SRQ_ARMENA:
		return DBC_GROUP_SRQ;

	case DBR_TYPE_CQ:
	case DBR_TYPE_CQ_ARMSE:
	case DBR_TYPE_CQ_ARMALL:
	case DBR_TYPE_CQ_ARMENA:
	case DBR_TYPE_CQ_CUTOFF_ACK:
		return DBC_GROUP_CQ;

	default:
		break;
	}

	return DBC_GROUP_MAX;
}

/*
 * Caller of this function is debugfs knob. It dumps the kernel memory table
 * main structure value to caller.
 * Additionally, dump page content to dmesg. Since we may have many pages, it
 * is too large to output to debugfs.
 */
char *bnxt_hdbr_ktbl_dump(struct bnxt_hdbr_ktbl *ktbl)
{
	struct dbc_drk64 *slot;
	char *buf;
	int i, j;

	if (!ktbl) {
		buf = kasprintf(GFP_KERNEL, "ktbl is NULL\n");
		return buf;
	}

	/* Structure data to debugfs console */
	buf = kasprintf(GFP_KERNEL,
			"group_type    = %d\n"
			"first_avail   = %d\n"
			"first_empty   = %d\n"
			"last_entry    = %d\n"
			"slot_avail    = %d\n"
			"num_4k_pages  = %d\n"
			"daddr         = 0x%016llX\n"
			"link_slot     = 0x%016llX\n",
			ktbl->group_type,
			ktbl->first_avail,
			ktbl->first_empty,
			ktbl->last_entry,
			ktbl->slot_avail,
			ktbl->num_4k_pages,
			ktbl->daddr,
			(u64)ktbl->link_slot);

	/* Page content dump to dmesg console */
	pr_info("====== Dumping ktbl info ======\n%s", buf);
	for (i = 0; i < ktbl->num_4k_pages; i++) {
		slot = ktbl->pages[i];
		pr_info("ktbl->pages[%d]: 0x%016llX\n", i, (u64)slot);
		for (j = 0; j < 256; j++) {
			if (j && j < 255 && !slot[j].flags && !slot[j].memptr)
				continue;
			pr_info("pages[%2d][%3d], 0x%016llX, 0x%016llX\n",
				i, j, le64_to_cpu(slot[j].flags),
				le64_to_cpu(slot[j].memptr));
		}
	}

	return buf;
}

/*
 * This function is called during L2 driver context memory allocation time.
 * It is on the path of nic open.
 * The initialization is allocating the memory for main data structure and
 * setup initial values.
 * pg_ptr and da are pointing to the first page allocated in
 * bnxt_setup_ctxm_pg_tbls()
 */
int bnxt_hdbr_ktbl_init(struct bnxt *bp, int group, void *pg_ptr, dma_addr_t da)
{
	struct bnxt_hdbr_ktbl *ktbl;

	ktbl = kzalloc(sizeof(*ktbl), GFP_KERNEL);
	if (!ktbl)
		return -ENOMEM;

	memset(pg_ptr, 0, SZ_4K);
	ktbl->pdev = bp->pdev;
	mutex_init(&ktbl->hdbr_kmem_lock);
	ktbl->group_type = group;
	ktbl->first_avail = 0;
	ktbl->first_empty = 0;
	ktbl->last_entry = -1; /* There isn't last entry at first */
	ktbl->slot_avail = NSLOT_PER_4K_PAGE;
	ktbl->num_4k_pages = 1;
	ktbl->pages[0] = pg_ptr;
	ktbl->daddr = da;
	ktbl->link_slot = pg_ptr + SZ_4K - DBC_KERNEL_ENTRY_SIZE;

	/* Link to main bnxt structure */
	bp->hdbr_info.ktbl[group] = ktbl;

	return 0;
}

/*
 * This function is called during L2 driver context memory free time. It is on
 * the path of nic close.
 */
void bnxt_hdbr_ktbl_uninit(struct bnxt *bp, int group)
{
	struct bnxt_hdbr_ktbl *ktbl;
	struct dbc_drk64 *slot;
	dma_addr_t da;
	void *ptr;
	int i;

	/* Tear off from bp structure first */
	ktbl = bp->hdbr_info.ktbl[group];
	bp->hdbr_info.ktbl[group] = NULL;
	if (!ktbl)
		return;

	/* Free attached pages(first page will be freed by bnxt_free_ctx_pg_tbls() */
	for (i = ktbl->num_4k_pages - 1; i >= 1; i--) {
		ptr = ktbl->pages[i];
		slot = ktbl->pages[i - 1] + SZ_4K - DBC_KERNEL_ENTRY_SIZE;
		da = (dma_addr_t)le64_to_cpu(slot->memptr);
		dma_free_coherent(&bp->pdev->dev, SZ_4K, ptr, da);
	}

	/* Free the control structure at last */
	kfree(ktbl);
}

/*
 * This function is called when dbnxt_hdbr_reg_apg() run out of memory slots.
 * hdbr_kmem_lock is held in caller, so it is safe to alter the kernel page
 * chain.
 */
static int bnxt_hdbr_alloc_ktbl_pg(struct bnxt_hdbr_ktbl *ktbl)
{
	dma_addr_t da;
	void *ptr;

	/* Development stage guard */
	if (ktbl->num_4k_pages >= MAX_KMEM_4K_PAGES) {
		pr_err("Must fix: need more than MAX_KMEM_4K_PAGES\n");
		return -ENOMEM;
	}

	/* Alloc one page */
	ptr = dma_alloc_coherent(&ktbl->pdev->dev, SZ_4K, &da, GFP_KERNEL | __GFP_ZERO);
	if (!ptr)
		return -ENOMEM;

	/* Chain up with existing pages */
	ktbl->pages[ktbl->num_4k_pages] = ptr;
	bnxt_hdbr_set_link(ktbl->link_slot, da);
	ktbl->link_slot = ptr + SZ_4K - DBC_KERNEL_ENTRY_SIZE;
	ktbl->num_4k_pages += 1;
	ktbl->slot_avail += NSLOT_PER_4K_PAGE;

	return 0;
}

/*
 * This function is called when L2 driver, RoCE driver or RoCE driver on
 * behalf of rocelib need to register its application memory page.
 * Each application memory page is linked in kernel memory table with a
 * 16 bytes memory slot.
 */
int bnxt_hdbr_reg_apg(struct bnxt_hdbr_ktbl *ktbl, dma_addr_t ap_da, int *idx,
		      u16 pi, u64 stride_n_size)
{
	struct dbc_drk64 *slot;
	int rc = 0;

	mutex_lock(&ktbl->hdbr_kmem_lock);

	/* Add into kernel table */
	if (ktbl->slot_avail == 0) {
		rc = bnxt_hdbr_alloc_ktbl_pg(ktbl);
		if (rc)
			goto exit;
	}

	/* Fill up the new entry */
	slot = get_slot(ktbl, ktbl->first_avail);
	bnxt_hdbr_set_slot(slot, ap_da, pi, stride_n_size,
			   ktbl->first_avail == ktbl->first_empty);
	*idx = ktbl->first_avail;
	ktbl->slot_avail--;

	/* Clear last flag of previous and advance first_avail index */
	if (ktbl->first_avail == ktbl->first_empty) {
		if (ktbl->last_entry >= 0) {
			slot = get_slot(ktbl, ktbl->last_entry);
			slot->flags &= cpu_to_le64(~DBC_DRK64_LAST);
		}
		ktbl->last_entry = ktbl->first_avail;
		ktbl->first_avail++;
		ktbl->first_empty++;
	} else {
		while (++ktbl->first_avail < ktbl->first_empty) {
			slot = get_slot(ktbl, ktbl->first_avail);
			if (slot->flags & cpu_to_le64(DBC_DRK64_VALID))
				continue;
			break;
		}
	}

exit:
	mutex_unlock(&ktbl->hdbr_kmem_lock);
	return rc;
}
EXPORT_SYMBOL(bnxt_hdbr_reg_apg);

/*
 * This function is called when L2 driver, RoCE driver or RoCE driver on
 * behalf of rocelib need to unregister its application memory page.
 * The corresponding memory slot need to be cleared.
 * Kernel memory table will reuse that slot for later application page.
 */
void bnxt_hdbr_unreg_apg(struct bnxt_hdbr_ktbl *ktbl, int idx)
{
	struct dbc_drk64 *slot;

	mutex_lock(&ktbl->hdbr_kmem_lock);
	if (idx == ktbl->last_entry) {
		/* Find the new last_entry index, and mark last */
		while (--ktbl->last_entry >= 0) {
			slot = get_slot(ktbl, ktbl->last_entry);
			if (slot->flags & cpu_to_le64(DBC_DRK64_VALID))
				break;
		}
		if (ktbl->last_entry >= 0) {
			slot = get_slot(ktbl, ktbl->last_entry);
			slot->flags |= cpu_to_le64(DBC_DRK64_LAST);
		}
		ktbl->first_empty = ktbl->last_entry + 1;
	}

	/* unregister app page entry */
	bnxt_hdbr_clear_slot(get_slot(ktbl, idx));

	/* update first_avail index to lower possible */
	if (idx < ktbl->first_avail)
		ktbl->first_avail = idx;
	ktbl->slot_avail++;
	mutex_unlock(&ktbl->hdbr_kmem_lock);
}
EXPORT_SYMBOL(bnxt_hdbr_unreg_apg);

/*
 * Map L2 ring type to DB copy group type
 */
int bnxt_hdbr_r2g(u32 ring_type)
{
	switch (ring_type) {
	case HWRM_RING_ALLOC_TX:
		return DBC_GROUP_SQ;

	case HWRM_RING_ALLOC_RX:
	case HWRM_RING_ALLOC_AGG:
		return DBC_GROUP_SRQ;

	case HWRM_RING_ALLOC_CMPL:
		return DBC_GROUP_CQ;

	default:
		break;
	}

	return DBC_GROUP_MAX;
}

/*
 * Allocate a 4K page for L2 DB copies. This is called when running out of
 * available DB copy blocks during DB registering.
 */
static int bnxt_hdbr_l2_alloc_page(struct bnxt *bp, int group, int dpi)
{
	struct bnxt_hdbr_l2_pgs *app_pgs;
	dma_addr_t da = 0;
	int ktbl_idx;
	__le64 *ptr;
	int rc;

	app_pgs = bp->hdbr_pgs[group][dpi];
	if (app_pgs->alloced_pages >= app_pgs->max_pages) {
		dev_err(&bp->pdev->dev, "Max reserved HDBR pages exceeded\n");
		return -EINVAL;
	}
	ptr = dma_zalloc_coherent(&bp->pdev->dev, SZ_4K, &da, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;
	ptr[0] = cpu_to_le64(DBC_VALUE_LAST);
	wmb();	/* Make sure HW see this slot when page linked in */
	/* Register to kernel table */
	rc = bnxt_hdbr_reg_apg(bp->hdbr_info.ktbl[group], da, &ktbl_idx, dpi,
			       app_pgs->stride_n_size);
	if (rc) {
		dma_free_coherent(&bp->pdev->dev, SZ_4K, ptr, da);
		return rc;
	}
	app_pgs->pages[app_pgs->alloced_pages].ptr = ptr;
	app_pgs->pages[app_pgs->alloced_pages].da = da;
	app_pgs->pages[app_pgs->alloced_pages].ktbl_idx = ktbl_idx;
	app_pgs->alloced_pages++;
	return 0;
}

/*
 * The l2 init function is called after L2 driver configured backing store
 * context memory and bnxt_hwrm_func_resc_qcaps.
 * The initialization is allocating the management structure and initialize
 * it with the proper values.
 *
 * Inside L2 DB copy app page, DBs are grouped by group type.
 *     DBC_GROUP_SQ  : grp_size = 1,
 *		       offset 0: SQ producer index doorbell
 *     DBC_GROUP_SRQ : grp_size = 1,
 *		       offset 0: SRQ producer index doorbell
 *     DBC_GROUP_CQ  : grp_size = 3,
 *		       offset 0: CQ consumer index doorbell
 *		       offset 1: CQ_ARMALL/CQ_ARMASE (share slot)
 *		       offset 2: CUTOFF_ACK
 */
static int bnxt_hdbr_l2_init_group(struct bnxt *bp, int group)
{
	struct bnxt_hdbr_ktbl *ktbl = bp->hdbr_info.ktbl[group];
	struct bnxt_hdbr_l2_pgs *app_pgs = NULL;
	int grp_size, epp, entries, max_pgs;
	int blk_size, num_dpi = 1, i;

	switch (group) {
	case DBC_GROUP_SQ:
		grp_size = HDBR_L2_SQ_BLK_SIZE;
		entries = bp->hw_resc.max_tx_rings;
		if (bp->db_size_mp)
			num_dpi = (bp->db_size_mp >> PAGE_SHIFT) +
				  bp->base_dpi_mp;
		break;
	case DBC_GROUP_SRQ:
		grp_size = HDBR_L2_SRQ_BLK_SIZE;
		entries = bp->hw_resc.max_rx_rings;
		break;
	case DBC_GROUP_CQ:
		grp_size = HDBR_L2_CQ_BLK_SIZE;
		entries = bp->hw_resc.max_cp_rings;
		break;
	default:
		/* Other group/DB types are not needed */
		return 0;
	}
	bp->hdbr_pgs[group] = kcalloc(num_dpi, sizeof(app_pgs), GFP_KERNEL);
	if (!bp->hdbr_pgs[group])
		return -ENOMEM;
	ktbl->max_dpi = num_dpi - 1;
	blk_size = hdbr_get_block_size(grp_size);
	epp = hdbr_get_entries_per_pg(blk_size);
	max_pgs = DIV_ROUND_UP(entries, epp);

	for (i = 0; i <= ktbl->max_dpi; i++) {
		if (i && i < bp->base_dpi_mp)
			continue;
		app_pgs = kzalloc(struct_size(app_pgs, pages, max_pgs),
				  GFP_KERNEL);
		if (!app_pgs)
			return -ENOMEM;

		app_pgs->max_pages = max_pgs;
		app_pgs->grp_size = grp_size;
		app_pgs->blk_size = blk_size;
		app_pgs->stride_n_size = hdbr_get_stride_size(grp_size);
		app_pgs->entries_per_pg = epp;
		/* Link to main bnxt structure */
		bp->hdbr_pgs[group][i] = app_pgs;
	}
	return 0;
}

int bnxt_hdbr_l2_init(struct bnxt *bp)
{
	int rc, group;

	if (!bp->hdbr_info.hdbr_enabled)
		return 0;

	if (bp->hdbr_pgs[DBC_GROUP_SQ]) {
		netdev_warn(bp->dev, "Attempt to initialize HDBR again!");
		return 0;
	}

	for (group = DBC_GROUP_SQ; group < DBC_GROUP_MAX; group++) {
		rc = bnxt_hdbr_l2_init_group(bp, group);
		if (rc)
			return rc;
	}

	bnxt_debugfs_hdbr_init(bp);

	return 0;
}

/*
 * This function is called during L2 driver context memory free time. It is on
 * the path of nic close.
 */
void bnxt_hdbr_l2_uninit(struct bnxt *bp, int group)
{
	struct bnxt_hdbr_ktbl *ktbl = bp->hdbr_info.ktbl[group];
	struct bnxt_hdbr_l2_pgs *pgs;
	struct hdbr_l2_pg *p;
	int i, j;

	if (!ktbl || !bp->hdbr_pgs[group])
		return;
	for (i = 0; i <= ktbl->max_dpi; i++) {
		pgs = bp->hdbr_pgs[group][i];
		if (!pgs)
			continue;
		for (j = 0; j < pgs->alloced_pages; j++) {
			p = &pgs->pages[j];
			/* Unregister from kernel table */
			bnxt_hdbr_unreg_apg(ktbl, p->ktbl_idx);
			/* Free memory up */
			dma_free_coherent(&bp->pdev->dev, SZ_4K, p->ptr, p->da);
		}
		kfree(pgs);
		bp->hdbr_pgs[group][i] = NULL;
	}
	kfree(bp->hdbr_pgs[group]);
	bp->hdbr_pgs[group] = NULL;
}

/*
 * This function is called when a new db is created.
 * It finds a memory slot in the DB copy application page, and return the
 * address.
 * Not all DB type need a copy, for those DB types don't need a copy, we
 * simply return NULL.
 */
__le64 *bnxt_hdbr_reg_db(struct bnxt *bp, int group, int dpi)
{
	struct bnxt_hdbr_l2_pgs *pgs;
	struct bnxt_hdbr_ktbl *ktbl;
	struct hdbr_l2_pg *p;
	int rc, i, n, idx;

	if (group >= DBC_GROUP_MAX)
		return NULL;

	ktbl = bp->hdbr_info.ktbl[group];
	if (!ktbl || dpi > ktbl->max_dpi)
		return NULL;

	pgs = bp->hdbr_pgs[group][dpi];
	if (!pgs)
		return NULL;

	if (pgs->next_page == pgs->alloced_pages) {
		rc = bnxt_hdbr_l2_alloc_page(bp, group, dpi);
		if (rc)
			return NULL;
	}

	n = pgs->blk_size;
	p = &pgs->pages[pgs->next_page];
	idx = pgs->next_entry * n; /* This is what we'll return */
	for (i = 0; i < pgs->grp_size; i++)
		p->ptr[idx + i] = cpu_to_le64(DBC_VALUE_INIT);
	pgs->next_entry++;
	if (pgs->next_entry == pgs->entries_per_pg) {
		pgs->next_page++;
		pgs->next_entry = 0;
	} else {
		p->ptr[pgs->next_entry * n] = cpu_to_le64(DBC_VALUE_LAST);
	}

	return &p->ptr[idx];
}

/*
 * This function is called when all L2 rings are freed.
 * Driver is still running, but rings are freed, so that all DB copy slots should be
 * reclaimed for later newly created rings' DB.
 */
void bnxt_hdbr_reset_l2pgs(struct bnxt *bp)
{
	struct bnxt_hdbr_l2_pgs *pgs;
	struct bnxt_hdbr_ktbl *ktbl;
	struct hdbr_l2_pg *p;
	int group, i, dpi;

	for (group = DBC_GROUP_SQ; group < DBC_GROUP_MAX; group++) {
		ktbl = bp->hdbr_info.ktbl[group];
		if (!ktbl || !bp->hdbr_pgs[group])
			continue;

		for (dpi = 0; dpi <= ktbl->max_dpi; dpi++) {
			pgs = bp->hdbr_pgs[group][dpi];
			if (!pgs)
				continue;

			for (i = 0; i < pgs->alloced_pages; i++) {
				p = &pgs->pages[i];
				memset(p->ptr, 0, SZ_4K);
				p->ptr[0] = cpu_to_le64(DBC_VALUE_LAST);
			}
			pgs->next_page = 0;
			pgs->next_entry = 0;
		}
	}
}

void bnxt_hdbr_uninit_all(struct bnxt *bp)
{
	u16 type;

	bnxt_debugfs_hdbr_delete(bp);
	for (type = BNXT_CTX_SQDBS; type <= BNXT_CTX_CQDBS; type++) {
		bnxt_hdbr_l2_uninit(bp, type - BNXT_CTX_SQDBS);
		bnxt_hdbr_ktbl_uninit(bp, type - BNXT_CTX_SQDBS);
	}
}

/*
 * Caller of this function is debugfs knob. It dumps the main structure value
 * of L2 driver DB copy region to caller.
 * Additionally, dump page content to dmesg. Since we may have many pages, it
 * is too large to output to debugfs.
 */
char *bnxt_hdbr_l2pg_dump(struct bnxt_hdbr_l2_pgs *app_pgs)
{
	struct hdbr_l2_pg *p;
	int used_entries = 0;
	int stride, ss;
	u64  dbc_val;
	char *buf;
	int pi, i;

	if (!app_pgs) {
		buf = kasprintf(GFP_KERNEL, "No data available!\n");
		return buf;
	}

	if (app_pgs->alloced_pages)
		used_entries = app_pgs->next_page * app_pgs->entries_per_pg + app_pgs->next_entry;
	stride = (int)((app_pgs->stride_n_size & DBC_DRK64_STRIDE_MASK) >> DBC_DRK64_STRIDE_SFT);
	ss = (int)((app_pgs->stride_n_size & DBC_DRK64_SIZE_MASK) >> DBC_DRK64_SIZE_SFT);
	ss = ss ? ss : 4;
	/* Structure data to debugfs console */
	buf = kasprintf(GFP_KERNEL,
			"max_pages      = %d\n"
			"alloced_pages  = %d\n"
			"group_size     = %d\n"
			"block_size     = %d (DBs)\n"
			"stride         = %d\n"
			"stride size    = %d\n"
			"entries_per_pg = %d\n"
			"used entries   = %d\n"
			"used db slots  = %d\n",
			app_pgs->max_pages,
			app_pgs->alloced_pages,
			app_pgs->grp_size,
			app_pgs->blk_size,
			stride,
			ss,
			app_pgs->entries_per_pg,
			used_entries,
			used_entries * app_pgs->grp_size);

	pr_info("====== Dumping pages info ======\n%s", buf);
	for (pi = 0; pi < app_pgs->alloced_pages; pi++) {
		p = &app_pgs->pages[pi];
		/* Page content dump to dmesg console */
		pr_info("page[%d].kernel addr   = 0x%016llX\n"
			"page[%d].dma addr      = 0x%016llX\n"
			"page[%d].Kernel index  = %d\n",
			pi, (u64)p->ptr,
			pi, p->da,
			pi, p->ktbl_idx);
		for (i = 0; i < 512; i++) {
			if (i && i < 511 && !p->ptr[i])
				continue;
			dbc_val = le64_to_cpu(p->ptr[i]);
			pr_info("page[%d][%3d] 0x%016llX : type=%llx "
				"debug_trace=%d valid=%d path=%llx xID=0x%05llx "
				"toggle=%llx epoch=%d index=0x%06llx\n",
				pi, i, dbc_val,
				(dbc_val & DBC_DBC64_TYPE_MASK) >> DBC_DBC64_TYPE_SFT,
				(dbc_val & DBC_DBC64_DEBUG_TRACE) ? 1 : 0,
				(dbc_val & DBC_DBC64_VALID) ? 1 : 0,
				(dbc_val & DBC_DBC64_PATH_MASK) >> DBC_DBC64_PATH_SFT,
				(dbc_val & DBC_DBC64_XID_MASK) >> DBC_DBC64_XID_SFT,
				(dbc_val & DBC_DBC64_TOGGLE_MASK) >> DBC_DBC64_TOGGLE_SFT,
				(dbc_val & DBC_DBC64_EPOCH) ? 1 : 0,
				(dbc_val & DBC_DBC64_INDEX_MASK));
		}
	}

	return buf;
}
