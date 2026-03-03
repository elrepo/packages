/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_MARK_MGR_H_
#define _ULP_MARK_MGR_H_

#include "bnxt_tf_ulp.h"

#define BNXT_ULP_MARK_VALID   0x1
#define BNXT_ULP_MARK_VFR_ID  0x2
#define BNXT_ULP_MARK_GLOBAL_HW_FID 0x4
#define BNXT_ULP_MARK_LOCAL_HW_FID 0x8

struct bnxt_lfid_mark_info {
	u16	mark_id;
	u16	flags;
};

struct bnxt_gfid_mark_info {
	u32	mark_id;
	u16	flags;
};

struct bnxt_ulp_mark_tbl {
	struct bnxt_lfid_mark_info	*lfid_tbl;
	struct bnxt_gfid_mark_info	*gfid_tbl;
	u32			lfid_num_entries;
	u32			gfid_num_entries;
	u32			gfid_mask;
	u32			gfid_type_bit;
};

/**
 * Allocate and Initialize all Mark Manager resources for this ulp context.
 *
 * Initialize MARK database for GFID & LFID tables
 * GFID: Global flow id which is based on EEM hash id.
 * LFID: Local flow id which is the CFA action pointer.
 * GFID is used for EEM flows, LFID is used for EM flows.
 *
 * Flow mapper modules adds mark_id in the MARK database.
 *
 * BNXT PMD receive handler extracts the hardware flow id from the
 * received completion record. Fetches mark_id from the MARK
 * database using the flow id. Injects mark_id into the packet's mbuf.
 *
 * @ctxt: The ulp context for the mark manager.
 */
int
ulp_mark_db_init(struct bnxt_ulp_context *ctxt);

/**
 * Adds a Mark to the Mark Manager
 *
 * @ctxt: The ulp context for the mark manager
 * @mark_flag: mark flags.
 * @fid: The flow id that is returned by HW in BD
 * @mark: The mark to be associated with the FID
 */
int
ulp_mark_db_deinit(struct bnxt_ulp_context *ctxt);

/**
 * Get a Mark from the Mark Manager
 *
 * @ctxt: The ulp context for the mark manager
 * @is_gfid: The type of fid (GFID or LFID)
 * @fid: The flow id that is returned by HW in BD
 * @vfr_flag: It indicates if mark is vfr_id or mark id
 * @mark: The mark that is associated with the FID
 */
int
ulp_mark_db_mark_get(struct bnxt_ulp_context *ctxt,
		     bool is_gfid,
		     u32 fid,
		     u32 *vfr_flag,
		     u32 *mark);

/**
 * Adds a Mark to the Mark Manager
 *
 * @ctxt: The ulp context for the mark manager
 * @mark_flag: mark flags.
 * @fid: The flow id that is returned by HW in BD
 * @mark: The mark to be associated with the FID
 */
int
ulp_mark_db_mark_add(struct bnxt_ulp_context *ctxt,
		     u32 mark_flag,
		     u32 gfid,
		     u32 mark);

/**
 * Removes a Mark from the Mark Manager
 *
 * @ctxt: The ulp context for the mark manager
 * @mark_flag: mark flags
 * @fid: The flow id that is returned by HW in BD
 */
int
ulp_mark_db_mark_del(struct bnxt_ulp_context *ctxt,
		     u32 mark_flag,
		     u32 gfid);

#endif /* _ULP_MARK_MGR_H_ */
