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
	struct bnxt_re_user_mmap_entry *entry;

	entry = bnxt_re_mmap_entry_insert(uctx, cpu_addr, dma_addr, user_mmap_hint, offset);
	if (entry) {
		*rdma_entry_save = &entry->rdma_entry;
		return true;
	}

	return false;
}

void bnxt_re_mmap_entry_remove_compat(struct rdma_user_mmap_entry *entry)
{
	return rdma_user_mmap_entry_remove(entry);
}
