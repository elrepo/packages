// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

/* Truflow Table APIs and supporting code */

#include <linux/types.h>
#include "tf_tbl.h"
#include "tf_tbl_sram.h"
#include "tf_sram_mgr.h"
#include "tf_rm.h"
#include "tf_util.h"
#include "tf_msg.h"
#include "tf_session.h"
#include "tf_device.h"
#include "cfa_resource_types.h"

#define TF_TBL_PTR_TO_RM(new_idx, idx, base, shift) {		\
		*(new_idx) = (((idx) >> (shift)) - (base));	\
}

/**
 * tf_sram_tbl_get_info_parms parameter definition
 *
 * @rm_db:	table RM database
 * @dir:	Receive or transmit direction
 * @tbl_type:	the TF index table type
 * @bank_id:	The SRAM bank associated with the type
 * @slice_size:	the slice size for the indicated table type
 *
 */
struct tf_tbl_sram_get_info_parms {
	void			*rm_db;
	enum tf_dir		dir;
	enum tf_tbl_type	tbl_type;
	enum tf_sram_bank_id	bank_id;
	enum tf_sram_slice_size	slice_size;
};

/* Translate HCAPI type to SRAM Manager bank */
const u16 tf_tbl_sram_hcapi_2_bank[CFA_RESOURCE_TYPE_P58_LAST] = {
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_0] = TF_SRAM_BANK_ID_0,
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_1] = TF_SRAM_BANK_ID_1,
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_2] = TF_SRAM_BANK_ID_2,
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_3] = TF_SRAM_BANK_ID_3
};

#define TF_TBL_SRAM_SLICES_MAX  \
	(TF_SRAM_MGR_BLOCK_SZ_BYTES / TF_SRAM_MGR_MIN_SLICE_BYTES)

/* Translate HCAPI type to SRAM Manager bank */
const u8 tf_tbl_sram_slices_2_size[TF_TBL_SRAM_SLICES_MAX + 1] = {
	[0] = TF_SRAM_SLICE_SIZE_64B, /* if 0 slices assume 1 64B block */
	[1] = TF_SRAM_SLICE_SIZE_64B, /* 1 slice  per 64B block */
	[2] = TF_SRAM_SLICE_SIZE_32B, /* 2 slices per 64B block */
	[4] = TF_SRAM_SLICE_SIZE_16B, /* 4 slices per 64B block */
	[8] = TF_SRAM_SLICE_SIZE_8B   /* 8 slices per 64B block */
};

/**
 * Get SRAM Table Information for a given index table type
 *
 * @sram_handle:	Pointer to SRAM handle
 * @parms:		Pointer to the SRAM get info parameters
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 */
static int tf_tbl_sram_get_info(struct tf_tbl_sram_get_info_parms *parms)
{
	struct tf_rm_get_slices_parms sparms;
	struct tf_rm_get_hcapi_parms hparms;
	u16 hcapi_type;
	u16 slices;
	int rc = 0;

	hparms.rm_db = parms->rm_db;
	hparms.subtype = parms->tbl_type;
	hparms.hcapi_type = &hcapi_type;

	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		netdev_dbg(NULL, "%s: Failed to get hcapi_type %s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->tbl_type), rc);
		return rc;
	}
	parms->bank_id = tf_tbl_sram_hcapi_2_bank[hcapi_type];

	sparms.rm_db = parms->rm_db;
	sparms.subtype = parms->tbl_type;
	sparms.slices = &slices;

	rc = tf_rm_get_slices(&sparms);
	if (rc) {
		netdev_dbg(NULL, "%s: Failed to get slice cnt %s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->tbl_type), rc);
		return rc;
	}
	if (slices)
		parms->slice_size = tf_tbl_sram_slices_2_size[slices];

	netdev_dbg(NULL, "(%s) bank(%s) slice_size(%s)\n",
		   tf_tbl_type_2_str(parms->tbl_type),
		   tf_sram_bank_2_str(parms->bank_id),
		   tf_sram_slice_2_str(parms->slice_size));
	return rc;
}

int tf_tbl_sram_bind(struct tf *tfp)
{
	void *sram_handle = NULL;
	int rc = 0;

	if (!tfp)
		return -EINVAL;

	rc = tf_sram_mgr_bind(&sram_handle);

	tf_session_set_sram_db(tfp, sram_handle);

	netdev_dbg(tfp->bp->dev, "SRAM Table - initialized\n");

	return rc;
}

int tf_tbl_sram_unbind(struct tf *tfp)
{
	void *sram_handle = NULL;
	int rc = 0;

	if (!tfp)
		return -EINVAL;

	rc = tf_session_get_sram_db(tfp, &sram_handle);
	if (rc) {
		netdev_dbg(NULL,
			   "Failed to get sram_handle from session, rc:%d\n",
			   rc);
		return rc;
	}
	tf_session_set_sram_db(tfp, NULL);

	if (sram_handle)
		rc = tf_sram_mgr_unbind(sram_handle);

	netdev_dbg(tfp->bp->dev, "SRAM Table - deinitialized\n");
	return rc;
}

int tf_tbl_sram_alloc(struct tf *tfp, struct tf_tbl_alloc_parms *parms)
{
	struct tf_tbl_sram_get_info_parms iparms = { 0 };
	struct tf_sram_mgr_alloc_parms aparms = { 0 };
	void *sram_handle = NULL;
	struct tbl_rm_db *tbl_db;
	void *tbl_db_ptr = NULL;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	u16 idx;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get(tfp, &tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to get tbl_db from session, rc:%d\n",
			   rc);
		return rc;
	}

	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	rc = tf_session_get_sram_db(tfp, &sram_handle);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to get sram_handle from session, rc:%d\n",
			   rc);
		return rc;
	}

	iparms.rm_db = tbl_db->tbl_db[parms->dir];
	iparms.dir = parms->dir;
	iparms.tbl_type = parms->type;

	rc = tf_tbl_sram_get_info(&iparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s: Failed to get SRAM info %s\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type));
		return rc;
	}

	aparms.dir = parms->dir;
	aparms.bank_id = iparms.bank_id;
	aparms.slice_size = iparms.slice_size;
	aparms.sram_offset = &idx;
	aparms.tbl_type = parms->type;
	aparms.rm_db = tbl_db->tbl_db[parms->dir];

	rc = tf_sram_mgr_alloc(sram_handle, &aparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s: Failed to allocate SRAM table:%s\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type));
		return rc;
	}
	*parms->idx = idx;

	return rc;
}

int tf_tbl_sram_free(struct tf *tfp, struct tf_tbl_free_parms *parms)
{
	struct tf_sram_mgr_is_allocated_parms aparms = { 0 };
	struct tf_tbl_sram_get_info_parms iparms = { 0 };
	struct tf_sram_mgr_free_parms fparms = { 0 };
	struct tbl_rm_db *tbl_db;
	void *sram_handle = NULL;
	struct tf_dev_info *dev;
	void *tbl_db_ptr = NULL;
	struct tf_session *tfs;
	bool allocated = false;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get(tfp, &tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to get em_ext_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	rc = tf_session_get_sram_db(tfp, &sram_handle);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to get sram_handle from session, rc:%d\n",
			   rc);
		return rc;
	}

	iparms.rm_db = tbl_db->tbl_db[parms->dir];
	iparms.dir = parms->dir;
	iparms.tbl_type = parms->type;

	rc = tf_tbl_sram_get_info(&iparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "%s: Failed to get table info:%s\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type));
		return rc;
	}

	aparms.sram_offset = parms->idx;
	aparms.slice_size = iparms.slice_size;
	aparms.bank_id = iparms.bank_id;
	aparms.dir = parms->dir;
	aparms.is_allocated = &allocated;

	rc = tf_sram_mgr_is_allocated(sram_handle, &aparms);
	if (rc || !allocated) {
		netdev_dbg(tfp->bp->dev,
			   "%s: Free of invalid entry:%s idx(%d):(%d)\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), parms->idx, rc);
		rc = -ENOMEM;
		return rc;
	}

	fparms.rm_db = tbl_db->tbl_db[parms->dir];
	fparms.tbl_type = parms->type;
	fparms.sram_offset = parms->idx;
	fparms.slice_size = iparms.slice_size;
	fparms.bank_id = iparms.bank_id;
	fparms.dir = parms->dir;
	rc = tf_sram_mgr_free(sram_handle, &fparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s: Failed to free entry:%s idx(%d)\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), parms->idx);
		return rc;
	}

	return rc;
}

int tf_tbl_sram_set(struct tf *tfp, struct tf_tbl_set_parms *parms)
{
	struct tf_sram_mgr_is_allocated_parms aparms = { 0 };
	struct tf_tbl_sram_get_info_parms iparms = { 0 };
	struct tf_rm_is_allocated_parms raparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	struct tbl_rm_db *tbl_db;
	void *sram_handle = NULL;
	u16 base = 0, shift = 0;
	struct tf_dev_info *dev;
	void *tbl_db_ptr = NULL;
	struct tf_session *tfs;
	bool allocated = 0;
	int rallocated = 0;
	u8 fw_session_id;
	u16 hcapi_type;
	int rc;

	if (!tfp || !parms || !parms->data)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get(tfp, &tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to get em_ext_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	rc = tf_session_get_sram_db(tfp, &sram_handle);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to get sram_handle from session, rc:%d\n",
			   rc);
		return rc;
	}

	iparms.rm_db = tbl_db->tbl_db[parms->dir];
	iparms.dir = parms->dir;
	iparms.tbl_type = parms->type;

	rc = tf_tbl_sram_get_info(&iparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "%s: Failed to get table info:%s\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type));
		return rc;
	}

	if (tf_session_is_shared_session(tfs)) {
		/* Only get table info if required for the device */
		if (dev->ops->tf_dev_get_tbl_info) {
			rc = dev->ops->tf_dev_get_tbl_info(tfp,
							   tbl_db->tbl_db[parms->dir],
							   parms->type,
							   &base,
							   &shift);
			if (rc) {
				netdev_dbg(tfp->bp->dev,
					   "%s: Failed to get table info:%d\n",
					   tf_dir_2_str(parms->dir),
					   parms->type);
				return rc;
			}
		}
		TF_TBL_PTR_TO_RM(&raparms.index, parms->idx, base, shift);

		raparms.rm_db = tbl_db->tbl_db[parms->dir];
		raparms.subtype = parms->type;
		raparms.allocated = &rallocated;
		rc = tf_rm_is_allocated(&raparms);
		if (rc)
			return rc;

		if (rallocated != TF_RM_ALLOCATED_ENTRY_IN_USE) {
			netdev_dbg(tfp->bp->dev,
				   "%s, Invalid index, type:%s, idx:%d\n",
				   tf_dir_2_str(parms->dir),
				   tf_tbl_type_2_str(parms->type),
				   parms->idx);
			return -EINVAL;
		}
	} else {
		aparms.sram_offset = parms->idx;
		aparms.slice_size = iparms.slice_size;
		aparms.bank_id = iparms.bank_id;
		aparms.dir = parms->dir;
		aparms.is_allocated = &allocated;
		rc = tf_sram_mgr_is_allocated(sram_handle, &aparms);
		if (rc || !allocated) {
			netdev_dbg(tfp->bp->dev,
				   "%s: Entry not allocated:%s idx(%d):(%d)\n",
				   tf_dir_2_str(parms->dir),
				   tf_tbl_type_2_str(parms->type),
				   parms->idx, rc);
			rc = -ENOMEM;
			return rc;
		}
	}

	/* Set the entry */
	hparms.rm_db = tbl_db->tbl_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s, Failed type lookup, type:%s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), rc);
		return rc;
	}

	rc = tf_msg_set_tbl_entry(tfp, parms->dir, hcapi_type,
				  parms->data_sz_in_bytes, parms->data,
				  parms->idx, fw_session_id);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "%s, Set failed, type:%s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), rc);
		return rc;
	}
	return rc;
}

int tf_tbl_sram_get(struct tf *tfp, struct tf_tbl_get_parms *parms)
{
	struct tf_sram_mgr_is_allocated_parms aparms = { 0 };
	struct tf_tbl_sram_get_info_parms iparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	bool clear_on_read = false;
	struct tbl_rm_db *tbl_db;
	void *sram_handle = NULL;
	struct tf_dev_info *dev;
	void *tbl_db_ptr = NULL;
	struct tf_session *tfs;
	bool allocated = 0;
	u8 fw_session_id;
	u16 hcapi_type;
	int rc;

	if (!tfp || !parms || !parms->data)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get(tfp, &tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to get em_ext_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	rc = tf_session_get_sram_db(tfp, &sram_handle);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to get sram_handle from session, rc:%d\n",
			   rc);
		return rc;
	}

	iparms.rm_db = tbl_db->tbl_db[parms->dir];
	iparms.dir = parms->dir;
	iparms.tbl_type = parms->type;

	rc = tf_tbl_sram_get_info(&iparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "%s: Failed to get table info:%s\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type));
		return rc;
	}

	aparms.sram_offset = parms->idx;
	aparms.slice_size = iparms.slice_size;
	aparms.bank_id = iparms.bank_id;
	aparms.dir = parms->dir;
	aparms.is_allocated = &allocated;

	rc = tf_sram_mgr_is_allocated(sram_handle, &aparms);
	if (rc || !allocated) {
		netdev_dbg(tfp->bp->dev,
			   "%s: Entry not allocated:%s idx(%d):(%d)\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), parms->idx, rc);
		rc = -ENOMEM;
		return rc;
	}

	/* Get the entry */
	hparms.rm_db = tbl_db->tbl_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s, Failed type lookup, type:%s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), rc);
		return rc;
	}

	/* Get the entry */
	rc = tf_msg_get_tbl_entry(tfp, parms->dir, hcapi_type,
				  parms->data_sz_in_bytes, parms->data,
				  parms->idx, clear_on_read, fw_session_id);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "%s, Get failed, type:%s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), rc);
		return rc;
	}
	return rc;
}

int tf_tbl_sram_bulk_get(struct tf *tfp, struct tf_tbl_get_bulk_parms *parms)
{
	struct tf_sram_mgr_is_allocated_parms aparms = { 0 };
	struct tf_tbl_sram_get_info_parms iparms = { 0 };
	struct tf_rm_get_hcapi_parms hparms = { 0 };
	bool clear_on_read = false;
	struct tbl_rm_db *tbl_db;
	void *sram_handle = NULL;
	void *tbl_db_ptr = NULL;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	bool allocated = false;
	u16 hcapi_type;
	u16 idx;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get(tfp, &tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_db(tfp, TF_MODULE_TYPE_TABLE, &tbl_db_ptr);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to get em_ext_db from session, rc:%d\n",
			   rc);
		return rc;
	}
	tbl_db = (struct tbl_rm_db *)tbl_db_ptr;

	rc = tf_session_get_sram_db(tfp, &sram_handle);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to get sram_handle from session, rc:%d\n",
			   rc);
		return rc;
	}

	iparms.rm_db = tbl_db->tbl_db[parms->dir];
	iparms.dir = parms->dir;
	iparms.tbl_type = parms->type;

	rc = tf_tbl_sram_get_info(&iparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "%s: Failed to get table info:%s\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type));
		return rc;
	}

	/* Validate the start offset and the end offset is allocated
	 * This API is only used for statistics.  8 Byte entry allocation
	 * is used to verify
	 */
	aparms.sram_offset = parms->starting_idx;
	aparms.slice_size = iparms.slice_size;
	aparms.bank_id = iparms.bank_id;
	aparms.dir = parms->dir;
	aparms.is_allocated = &allocated;
	rc = tf_sram_mgr_is_allocated(sram_handle, &aparms);
	if (rc || !allocated) {
		netdev_dbg(tfp->bp->dev,
			   "%s: Entry not allocated:%s start_idx(%d):(%d)\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), parms->starting_idx,
			   rc);
		rc = -ENOMEM;
		return rc;
	}
	idx = parms->starting_idx + parms->num_entries - 1;
	aparms.sram_offset = idx;
	rc = tf_sram_mgr_is_allocated(sram_handle, &aparms);
	if (rc || !allocated) {
		netdev_dbg(tfp->bp->dev,
			   "%s: Entry not allocated:%s last_idx(%d):(%d)\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), idx, rc);
		rc = -ENOMEM;
		return rc;
	}

	hparms.rm_db = tbl_db->tbl_db[parms->dir];
	hparms.subtype = parms->type;
	hparms.hcapi_type = &hcapi_type;
	rc = tf_rm_get_hcapi_type(&hparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s, Failed type lookup, type:%s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), rc);
		return rc;
	}

	if (parms->type == TF_TBL_TYPE_ACT_STATS_64)
		clear_on_read = true;

	/* Get the entries */
	rc = tf_msg_bulk_get_tbl_entry(tfp,
				       parms->dir,
				       hcapi_type,
				       parms->starting_idx,
				       parms->num_entries,
				       parms->entry_sz_in_bytes,
				       parms->physical_mem_addr,
				       clear_on_read);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "%s, Bulk get failed, type:%s, rc:%d\n",
			   tf_dir_2_str(parms->dir),
			   tf_tbl_type_2_str(parms->type), rc);
	}
	return rc;
}
