// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2021-2022 Broadcom
 * All rights reserved.
 */

#include <linux/netdevice.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

#include "hcapi_cfa_defs.h"
#include "bnxt_hsi.h"
#include "bnxt_compat.h"
#include "bnxt.h"
#include "tf_core.h"
#include "tf_session.h"
#include "tf_util.h"

#include "cfa_tcam_mgr.h"
#include "cfa_tcam_mgr_device.h"
#include "cfa_tcam_mgr_hwop_msg.h"

/* Thor */
#include "cfa_tcam_mgr_p58.h"
/* Wh+, SR */
#include "cfa_tcam_mgr_p4.h"

#define TF_TCAM_SLICE_INVALID (-1)

/* The following macros are for setting the entry status in a row entry.
 * row is (struct cfa_tcam_mgr_table_rows_0 *)
 */
#define ROW_ENTRY_INUSE(row, entry)  ((row)->entry_inuse &   (1U << (entry)))
#define ROW_ENTRY_SET(row, entry)    ((row)->entry_inuse |=  (1U << (entry)))
#define ROW_ENTRY_CLEAR(row, entry)  ((row)->entry_inuse &= ~(1U << (entry)))
#define ROW_INUSE(row)               ((row)->entry_inuse != 0)

static int physical_table_types[CFA_TCAM_MGR_TBL_TYPE_MAX] = {
	[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_APPS] =
		TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	[CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_APPS]  =
		TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	[CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_APPS]	      =
		TF_TCAM_TBL_TYPE_PROF_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_APPS]	      =
		TF_TCAM_TBL_TYPE_WC_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_APPS]	      =
		TF_TCAM_TBL_TYPE_SP_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_APPS]      =
		TF_TCAM_TBL_TYPE_CT_RULE_TCAM,
	[CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_APPS]	      =
		TF_TCAM_TBL_TYPE_VEB_TCAM,
};

int cfa_tcam_mgr_get_phys_table_type(enum cfa_tcam_mgr_tbl_type type)
{
	WARN_ON(type >= CFA_TCAM_MGR_TBL_TYPE_MAX);

	return physical_table_types[type];
}

const char *
cfa_tcam_mgr_tbl_2_str(enum cfa_tcam_mgr_tbl_type tcam_type)
{
	switch (tcam_type) {
	case CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_AFM:
		return "l2_ctxt_tcam_high AFM";
	case CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_HIGH_APPS:
		return "l2_ctxt_tcam_high Apps";
	case CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_AFM:
		return "l2_ctxt_tcam_low AFM";
	case CFA_TCAM_MGR_TBL_TYPE_L2_CTXT_TCAM_LOW_APPS:
		return "l2_ctxt_tcam_low Apps";
	case CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_AFM:
		return "prof_tcam AFM";
	case CFA_TCAM_MGR_TBL_TYPE_PROF_TCAM_APPS:
		return "prof_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_AFM:
		return "wc_tcam AFM";
	case CFA_TCAM_MGR_TBL_TYPE_WC_TCAM_APPS:
		return "wc_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_AFM:
		return "veb_tcam AFM";
	case CFA_TCAM_MGR_TBL_TYPE_VEB_TCAM_APPS:
		return "veb_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_AFM:
		return "sp_tcam AFM";
	case CFA_TCAM_MGR_TBL_TYPE_SP_TCAM_APPS:
		return "sp_tcam Apps";
	case CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_AFM:
		return "ct_rule_tcam AFM";
	case CFA_TCAM_MGR_TBL_TYPE_CT_RULE_TCAM_APPS:
		return "ct_rule_tcam Apps";
	default:
		return "Invalid tcam table type";
	}
}

/* key_size and slice_width are in bytes */
static int cfa_tcam_mgr_get_num_slices(unsigned int key_size, unsigned int slice_width)
{
	int num_slices = 0;

	if (!key_size)
		return -EINVAL;

	num_slices = ((key_size - 1U) / slice_width) + 1U;
	/* Round up to next highest power of 2 */
	/* This is necessary since, for example, 3 slices is not a valid entry
	 * width.
	 */
	num_slices--;
	/* Repeat to maximum number of bits actually used */
	/* This fills in all the bits. */
	num_slices |= num_slices >> 1;
	num_slices |= num_slices >> 2;
	num_slices |= num_slices >> 4;
	/*
	 * If the maximum number of slices that are supported by the HW
	 * increases, then additional shifts are needed.
	 */
	num_slices++;
	return num_slices;
}

static struct cfa_tcam_mgr_entry_data *cfa_tcam_mgr_entry_get(struct
							      cfa_tcam_mgr_data
							      * tcam_mgr_data,
							      u16 id)
{
	if (id > tcam_mgr_data->cfa_tcam_mgr_max_entries)
		return NULL;

	return &tcam_mgr_data->entry_data[id];
}

/* Insert an entry into the entry table */
static int cfa_tcam_mgr_entry_insert(struct cfa_tcam_mgr_data *tcam_mgr_data,
				     struct tf *tfp, u16 id,
				     struct cfa_tcam_mgr_entry_data *entry)
{
	if (id > tcam_mgr_data->cfa_tcam_mgr_max_entries)
		return -EINVAL;

	memcpy(&tcam_mgr_data->entry_data[id], entry,
	       sizeof(tcam_mgr_data->entry_data[id]));

	netdev_dbg(tfp->bp->dev, "Added entry %d to table\n", id);

	return 0;
}

/* Delete an entry from the entry table */
static int cfa_tcam_mgr_entry_delete(struct cfa_tcam_mgr_data *tcam_mgr_data,
				     struct tf *tfp, u16 id)
{
	if (id > tcam_mgr_data->cfa_tcam_mgr_max_entries)
		return -EINVAL;

	memset(&tcam_mgr_data->entry_data[id], 0,
	       sizeof(tcam_mgr_data->entry_data[id]));

	netdev_dbg(tfp->bp->dev,
		   "Deleted entry %d from table\n", id);

	return 0;
}

/* Returns the size of the row structure taking into account how many slices a
 * TCAM supports.
 */
static int cfa_tcam_mgr_row_size_get(struct cfa_tcam_mgr_data *tcam_mgr_data,
				     enum tf_dir dir,
				     enum cfa_tcam_mgr_tbl_type type)
{
	return sizeof(struct cfa_tcam_mgr_table_rows_0) +
		(tcam_mgr_data->cfa_tcam_mgr_tables[dir][type].max_slices *
		 sizeof(((struct cfa_tcam_mgr_table_rows_0 *)0)->entries[0]));
}

static void *cfa_tcam_mgr_row_ptr_get(void *base, int index, int row_size)
{
	return (u8 *)base + (index * row_size);
}

/* Searches a table to find the direction and type of an entry. */
static int cfa_tcam_mgr_entry_find_in_table(struct cfa_tcam_mgr_data
					    *tcam_mgr_data,
					    int id, enum tf_dir dir,
					    enum cfa_tcam_mgr_tbl_type type)
{
	struct cfa_tcam_mgr_table_data *table_data;
	int max_slices, row_idx, row_size, slice;
	struct cfa_tcam_mgr_table_rows_0 *row;

	table_data = &tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];
	if (table_data->max_entries > 0 &&
	    table_data->hcapi_type > 0) {
		max_slices = table_data->max_slices;
		row_size = cfa_tcam_mgr_row_size_get(tcam_mgr_data, dir, type);
		for (row_idx = table_data->start_row;
		     row_idx <= table_data->end_row;
		     row_idx++) {
			row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows,
						       row_idx, row_size);
			if (!ROW_INUSE(row))
				continue;
			for (slice = 0;
			     slice < (max_slices / row->entry_size);
			     slice++) {
				if (!ROW_ENTRY_INUSE(row, slice))
					continue;
				if (row->entries[slice] == id)
					return 0;
			}
		}
	}

	return -ENOENT;
}

/* Searches all the tables to find the direction and type of an entry. */
static int cfa_tcam_mgr_entry_find(struct cfa_tcam_mgr_data *tcam_mgr_data,
				   int id, enum tf_dir *tbl_dir,
				   enum cfa_tcam_mgr_tbl_type *tbl_type)
{
	enum cfa_tcam_mgr_tbl_type type;
	int rc = -ENOENT;
	enum tf_dir dir;

	for (dir = TF_DIR_RX; dir <
			ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables);
			dir++) {
		for (type = CFA_TCAM_MGR_TBL_TYPE_START;
		     type <
			ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir]);
		     type++) {
			rc = cfa_tcam_mgr_entry_find_in_table(tcam_mgr_data,
							      id, dir, type);
			if (!rc) {
				*tbl_dir  = dir;
				*tbl_type = type;
				return rc;
			}
		}
	}

	return rc;
}

static int cfa_tcam_mgr_row_is_entry_free(struct cfa_tcam_mgr_table_rows_0 *row,
					  int max_slices, int key_slices)
{
	int j;

	if (ROW_INUSE(row) &&
	    row->entry_size == key_slices) {
		for (j = 0; j < (max_slices / row->entry_size); j++) {
			if (!ROW_ENTRY_INUSE(row, j))
				return j;
		}
	}
	return -EINVAL;
}

static int cfa_tcam_mgr_entry_move(struct cfa_tcam_mgr_data *tcam_mgr_data,
				   struct tf *tfp, enum tf_dir dir,
				   enum cfa_tcam_mgr_tbl_type type,
				   int entry_id,
				   struct cfa_tcam_mgr_table_data *table_data,
				   int dest_row_index, int dest_row_slice,
				   struct cfa_tcam_mgr_table_rows_0 *dest_row,
				   int source_row_index,
				   struct cfa_tcam_mgr_table_rows_0 *source_row,
				   bool free_source_entry)
{
	struct cfa_tcam_mgr_get_parms gparms = { 0 };
	struct cfa_tcam_mgr_set_parms sparms = { 0 };
	struct cfa_tcam_mgr_free_parms fparms = { 0 };
	struct cfa_tcam_mgr_entry_data *entry;
	u8  result[CFA_TCAM_MGR_MAX_KEY_SIZE];
	u8  mask[CFA_TCAM_MGR_MAX_KEY_SIZE];
	u8  key[CFA_TCAM_MGR_MAX_KEY_SIZE];
	int j, rc;

	entry = cfa_tcam_mgr_entry_get(tcam_mgr_data, entry_id);
	if (!entry)
		return -EINVAL;

	gparms.dir	   = dir;
	gparms.type	   = type;
	gparms.hcapi_type  = table_data->hcapi_type;
	gparms.key	   = key;
	gparms.mask	   = mask;
	gparms.result	   = result;
	gparms.id	   = source_row->entries[entry->slice];
	gparms.key_size	   = sizeof(key);
	gparms.result_size = sizeof(result);

	rc = cfa_tcam_mgr_entry_get_msg(tcam_mgr_data, tfp, &gparms,
					source_row_index,
					entry->slice * source_row->entry_size,
					table_data->max_slices);
	if (rc)
		return rc;

	sparms.dir	   = dir;
	sparms.type	   = type;
	sparms.hcapi_type  = table_data->hcapi_type;
	sparms.key	   = key;
	sparms.mask	   = mask;
	sparms.result	   = result;
	sparms.id	   = gparms.id;
	sparms.key_size	   = gparms.key_size;
	sparms.result_size = gparms.result_size;

	/* Slice in destination row not specified. Find first free slice. */
	if (dest_row_slice < 0)
		for (j = 0;
		     j < (table_data->max_slices / dest_row->entry_size);
		     j++) {
			if (!ROW_ENTRY_INUSE(dest_row, j)) {
				dest_row_slice = j;
				break;
			}
		}

	/* If no free slice found, return error. */
	if (dest_row_slice < 0)
		return -EPERM;

	rc = cfa_tcam_mgr_entry_set_msg(tcam_mgr_data, tfp, &sparms,
					dest_row_index,
					dest_row_slice * dest_row->entry_size,
					table_data->max_slices);
	if (rc)
		return rc;

	if (free_source_entry) {
		fparms.dir	  = dir;
		fparms.type	  = type;
		fparms.hcapi_type = table_data->hcapi_type;
		rc = cfa_tcam_mgr_entry_free_msg(tcam_mgr_data,
						 tfp, &fparms,
						 source_row_index,
						 entry->slice *
						 dest_row->entry_size,
						 table_data->row_width /
						 table_data->max_slices *
						 source_row->entry_size,
						 table_data->result_size,
						 table_data->max_slices);
		if (rc) {
			netdev_dbg(tfp->bp->dev,
				   "%s: %s Failed to free ID:%d row:%d slice:%d rc:%d\n",
				   tf_dir_2_str(dir),
				   cfa_tcam_mgr_tbl_2_str(type),
				   gparms.id, source_row_index, entry->slice,
				   -rc);
		}
	}

	netdev_dbg(tfp->bp->dev,
		   "Moved entry:%d from row:%d slice:%d to row:%d slice:%d\n",
		   entry_id, source_row_index, entry->slice, dest_row_index,
		   dest_row_slice);

	ROW_ENTRY_SET(dest_row, dest_row_slice);
	dest_row->entries[dest_row_slice] = entry_id;
	ROW_ENTRY_CLEAR(source_row, entry->slice);
	entry->row   = dest_row_index;
	entry->slice = dest_row_slice;

	cfa_tcam_mgr_rows_dump(tfp, dir, type);

	return 0;
}

static int cfa_tcam_mgr_row_move(struct cfa_tcam_mgr_data *tcam_mgr_data,
				 struct tf *tfp, enum tf_dir dir,
				 enum cfa_tcam_mgr_tbl_type type,
				 struct cfa_tcam_mgr_table_data *table_data,
				 int dest_row_index,
				 struct cfa_tcam_mgr_table_rows_0 *dest_row,
				 int source_row_index,
				 struct cfa_tcam_mgr_table_rows_0 *source_row)
{
	struct cfa_tcam_mgr_free_parms fparms = { 0 };
	int j, rc;

	dest_row->priority   = source_row->priority;
	dest_row->entry_size = source_row->entry_size;
	dest_row->entry_inuse = 0;

	fparms.dir	  = dir;
	fparms.type	  = type;
	fparms.hcapi_type = table_data->hcapi_type;

	for (j = 0;
	     j < (table_data->max_slices / source_row->entry_size);
	     j++) {
		if (ROW_ENTRY_INUSE(source_row, j)) {
			cfa_tcam_mgr_entry_move(tcam_mgr_data, tfp,
						dir, type,
						source_row->entries[j],
						table_data,
						dest_row_index, j, dest_row,
						source_row_index, source_row,
						true);
		} else {
			/* Slice not in use, write an empty slice. */
			rc = cfa_tcam_mgr_entry_free_msg(tcam_mgr_data,
							 tfp, &fparms,
							 dest_row_index,
							 j *
							 dest_row->entry_size,
							 table_data->row_width /
							 table_data->max_slices *
							 dest_row->entry_size,
							 table_data->result_size,
							 table_data->max_slices);
			if (rc)
				return rc;
		}
	}

	return 0;
}

/* Install entry into in-memory tables, not into TCAM (yet). */
static void cfa_tcam_mgr_row_entry_install(struct tf *tfp,
					   struct cfa_tcam_mgr_table_rows_0
					   *row,
					   struct cfa_tcam_mgr_alloc_parms
					   *parms,
					   struct cfa_tcam_mgr_entry_data
					   *entry,
					   u16 id, int key_slices,
					   int row_index, int slice)
{
	if (slice == TF_TCAM_SLICE_INVALID) {
		slice = 0;
		row->entry_size = key_slices;
		row->priority = parms->priority;
	}

	ROW_ENTRY_SET(row, slice);
	row->entries[slice] = id;
	entry->row = row_index;
	entry->slice = slice;

	netdev_dbg(tfp->bp->dev,
		   "Entry %d installed row:%d slice:%d prio:%d\n",
		   id, row_index, slice, row->priority);
	cfa_tcam_mgr_rows_dump(tfp, parms->dir, parms->type);
}

/* Finds an empty row that can be used and reserve for entry.  If necessary,
 * entries will be shuffled in order to make room.
 */
static struct cfa_tcam_mgr_table_rows_0 *
cfa_tcam_mgr_empty_row_alloc(struct cfa_tcam_mgr_data *tcam_mgr_data,
			     struct tf *tfp,
			     struct cfa_tcam_mgr_alloc_parms *parms,
			     struct cfa_tcam_mgr_entry_data *entry,
			     u16 id, int key_slices)
{
	int to_row_idx, from_row_idx, slice, start_row, end_row;
	struct cfa_tcam_mgr_table_rows_0 *tcam_rows;
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_table_rows_0 *from_row;
	struct cfa_tcam_mgr_table_rows_0 *to_row;
	struct cfa_tcam_mgr_table_rows_0 *row;
	int i, max_slices, row_size;
	int target_row = -1;
	int empty_row = -1;

	table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[parms->dir][parms->type];

	start_row = table_data->start_row;
	end_row = table_data->end_row;
	max_slices = table_data->max_slices;
	tcam_rows = table_data->tcam_rows;

	row_size = cfa_tcam_mgr_row_size_get(tcam_mgr_data, parms->dir,
					     parms->type);
	/* Note: The rows are ordered from highest priority to lowest priority.
	 * That is, the first row in the table will have the highest priority
	 * and the last row in the table will have the lowest priority.
	 */

	netdev_dbg(tfp->bp->dev,
		   "Trying to alloc space for entry with priority %d and width %d slices.\n",
		   parms->priority, key_slices);

	/* First check for partially used entries, but only if the key needs
	 * fewer slices than there are in a row.
	 */
	if (key_slices < max_slices) {
		for (i = start_row; i <= end_row; i++) {
			row = cfa_tcam_mgr_row_ptr_get(tcam_rows, i, row_size);
			if (!ROW_INUSE(row))
				continue;
			if (row->priority < parms->priority)
				break;
			if (row->priority > parms->priority)
				continue;
			slice = cfa_tcam_mgr_row_is_entry_free(row,
							       max_slices,
							       key_slices);
			if (slice >= 0) {
				cfa_tcam_mgr_row_entry_install(tfp,
							       row, parms,
							       entry, id,
							       key_slices, i,
							       slice);
				return row;
			}
		}
	}

	/* No partially used rows available.  Find an empty row, if any. */

	/* All max priority entries are placed in the beginning of the TCAM.  It
	 * should not be necessary to shuffle any of these entries.  All other
	 * priorities are placed from the end of the TCAM and may require
	 * shuffling.
	 */
	if (parms->priority == TF_TCAM_PRIORITY_MAX) {
		/* Handle max priority first. */
		for (i = start_row; i <= end_row; i++) {
			row = cfa_tcam_mgr_row_ptr_get(tcam_rows, i, row_size);
			if (!ROW_INUSE(row)) {
				cfa_tcam_mgr_row_entry_install(tfp,
							       row, parms,
							       entry, id,
							       key_slices, i,
							       TF_TCAM_SLICE_INVALID);
				return row;
			}
			if (row->priority < parms->priority) {
				/* No free entries before priority change, table is full. */
				return NULL;
			}
		}
		/* No free entries found, table is full. */
		return NULL;
	}

	/* Use the highest available entry */
	for (i = end_row; i >= start_row; i--) {
		row = cfa_tcam_mgr_row_ptr_get(tcam_rows, i, row_size);
		if (!ROW_INUSE(row)) {
			empty_row = i;
			break;
		}

		if (row->priority > parms->priority &&
		    target_row < 0)
			target_row = i;
	}

	if (empty_row < 0) {
		/* No free entries found, table is full. */
		return NULL;
	}

	if (target_row < 0) {
		/* Did not find a row with higher priority before unused row so
		 * just install new entry in empty_row.
		 */
		row = cfa_tcam_mgr_row_ptr_get(tcam_rows, empty_row, row_size);
		cfa_tcam_mgr_row_entry_install(tfp, row, parms, entry, id,
					       key_slices, empty_row,
					       TF_TCAM_SLICE_INVALID);
		return row;
	}

	to_row_idx = empty_row;
	to_row = cfa_tcam_mgr_row_ptr_get(tcam_rows, to_row_idx, row_size);
	while (to_row_idx < target_row) {
		from_row_idx = to_row_idx + 1;
		from_row = cfa_tcam_mgr_row_ptr_get(tcam_rows, from_row_idx,
						    row_size);
		/* Find the highest row with the same priority as the initial
		 * source row (from_row).  It's only necessary to copy one row
		 * of each priority.
		 */
		for (i = from_row_idx + 1; i <= target_row; i++) {
			row = cfa_tcam_mgr_row_ptr_get(tcam_rows, i, row_size);
			if (row->priority != from_row->priority)
				break;
			from_row_idx = i;
			from_row = row;
		}
		cfa_tcam_mgr_row_move(tcam_mgr_data, tfp, parms->dir,
				      parms->type,
				      table_data, to_row_idx, to_row,
				      from_row_idx, from_row);
		netdev_dbg(tfp->bp->dev, "Moved row %d to row %d.\n",
			   from_row_idx, to_row_idx);

		to_row = from_row;
		to_row_idx = from_row_idx;
	}
	to_row = cfa_tcam_mgr_row_ptr_get(tcam_rows, target_row, row_size);
	memset(to_row, 0, row_size);
	cfa_tcam_mgr_row_entry_install(tfp, to_row, parms, entry, id,
				       key_slices, target_row,
				       TF_TCAM_SLICE_INVALID);

	return row;
}

/* This function will combine rows when possible to result in the fewest rows
 * used necessary for the entries that are installed.
 */
static void cfa_tcam_mgr_rows_combine(struct cfa_tcam_mgr_data *tcam_mgr_data,
				      struct tf *tfp,
				      struct cfa_tcam_mgr_free_parms *parms,
				      struct cfa_tcam_mgr_table_data
				      *table_data,
				      int changed_row_index)
{
	int to_row_idx, from_row_idx, start_row, end_row, max_slices;
	struct cfa_tcam_mgr_table_rows_0 *from_row = NULL;
	struct cfa_tcam_mgr_table_rows_0 *tcam_rows;
	struct cfa_tcam_mgr_table_rows_0 *to_row;
	bool entry_moved = false;
	int  i, j, row_size;

	start_row  = table_data->start_row;
	end_row	   = table_data->end_row;
	max_slices = table_data->max_slices;
	tcam_rows  = table_data->tcam_rows;

	row_size   = cfa_tcam_mgr_row_size_get(tcam_mgr_data, parms->dir,
					       parms->type);

	from_row_idx = changed_row_index;
	from_row = cfa_tcam_mgr_row_ptr_get(tcam_rows, from_row_idx, row_size);

	if (ROW_INUSE(from_row)) {
		/* Row is still in partial use.  See if remaining entry(s) can
		 * be moved to free up a row.
		 */
		for (i = 0; i < (max_slices / from_row->entry_size); i++) {
			if (!ROW_ENTRY_INUSE(from_row, i))
				continue;
			for (to_row_idx = end_row;
			     to_row_idx >= start_row;
			     to_row_idx--) {
				to_row = cfa_tcam_mgr_row_ptr_get(tcam_rows,
								  to_row_idx,
								  row_size);
				if (!ROW_INUSE(to_row))
					continue;
				if (to_row->priority > from_row->priority)
					break;
				if (to_row->priority != from_row->priority)
					continue;
				if (to_row->entry_size != from_row->entry_size)
					continue;
				if (to_row_idx == changed_row_index)
					continue;
				for (j = 0;
				     j < (max_slices / to_row->entry_size);
				     j++) {
					if (!ROW_ENTRY_INUSE(to_row, j)) {
						cfa_tcam_mgr_entry_move
							(tcam_mgr_data,
							 tfp,
							 parms->dir,
							 parms->type,
							 from_row->entries[i],
							 table_data,
							 to_row_idx,
							 -1, to_row,
							 from_row_idx,
							 from_row,
							 true);
						entry_moved = true;
						break;
					}
				}
				if (entry_moved)
					break;
			}
			if (ROW_INUSE(from_row))
				entry_moved = false;
			else
				break;
		}
	}
}

/* This function will ensure that all rows, except those of the highest
 * priority, at the end of the table.  When this function is finished, all the
 * empty rows should be between the highest priority rows at the beginning of
 * the table and the rest of the rows with lower priorities.
 *
 * Will need to free the row left newly empty as a result of moving.
 * Return row to free to caller.  If new_row_to_free < 0, then no new row to
 * free.
 */
static void cfa_tcam_mgr_rows_compact(struct cfa_tcam_mgr_data *tcam_mgr_data,
				      struct tf *tfp,
				      struct cfa_tcam_mgr_free_parms *parms,
				      struct cfa_tcam_mgr_table_data
					*table_data,
				      int *new_row_to_free,
				      int changed_row_index)
{
	int  to_row_idx = 0, from_row_idx = 0, start_row = 0, end_row = 0;
	struct cfa_tcam_mgr_table_rows_0 *from_row = NULL;
	struct cfa_tcam_mgr_table_rows_0 *tcam_rows;
	struct cfa_tcam_mgr_table_rows_0 *to_row;
	struct cfa_tcam_mgr_table_rows_0 *row;
	int  i, row_size, priority;

	*new_row_to_free = -1;

	start_row  = table_data->start_row;
	end_row	   = table_data->end_row;
	tcam_rows  = table_data->tcam_rows;

	row_size   = cfa_tcam_mgr_row_size_get(tcam_mgr_data, parms->dir,
					       parms->type);

	/* The row is no longer in use, so see if rows need to be moved in order
	 * to not leave any gaps.
	 */
	to_row_idx = changed_row_index;
	to_row = cfa_tcam_mgr_row_ptr_get(tcam_rows, to_row_idx, row_size);

	priority = to_row->priority;
	if (priority == TF_TCAM_PRIORITY_MAX) {
		if (changed_row_index == end_row)
			/* Nothing to move - the last row in the TCAM is being deleted. */
			return;
		for (i = changed_row_index + 1; i <= end_row; i++) {
			row = cfa_tcam_mgr_row_ptr_get(tcam_rows, i, row_size);
			if (!ROW_INUSE(row))
				break;

			if (row->priority < priority)
				break;

			from_row = row;
			from_row_idx = i;
		}
	} else {
		if (changed_row_index == start_row)
			/* Nothing to move - the first row in the TCAM is being deleted. */
			return;
		for (i = changed_row_index - 1; i >= start_row; i--) {
			row = cfa_tcam_mgr_row_ptr_get(tcam_rows, i, row_size);
			if (!ROW_INUSE(row))
				break;

			if (row->priority > priority) {
				/* Don't move the highest priority rows. */
				if (row->priority == TF_TCAM_PRIORITY_MAX)
					break;
				/* If from_row is NULL, that means that there
				 * were no rows of the deleted priority.
				 * Nothing to move yet.
				 *
				 * If from_row is not NULL, then it is the last
				 * row with the same priority and must be moved
				 * to fill the newly empty (by free or by move)
				 * row.
				 */
				if (from_row) {
					cfa_tcam_mgr_row_move(tcam_mgr_data,
							      tfp,
							      parms->dir,
							      parms->type,
							      table_data,
							      to_row_idx,
							      to_row,
							      from_row_idx,
							      from_row);
					netdev_dbg(tfp->bp->dev,
						   "Moved row %d to row %d.\n", from_row_idx,
						   to_row_idx);
					*new_row_to_free = from_row_idx;
					to_row	   = from_row;
					to_row_idx = from_row_idx;
				}

				priority = row->priority;
			}
			from_row = row;
			from_row_idx = i;
		}
	}

	if (from_row) {
		cfa_tcam_mgr_row_move(tcam_mgr_data, tfp, parms->dir,
				      parms->type, table_data, to_row_idx,
				      to_row, from_row_idx, from_row);
		netdev_dbg(tfp->bp->dev, "Moved row %d to row %d.\n",
			   from_row_idx, to_row_idx);
		*new_row_to_free = from_row_idx;
	}
}

/* This function is to set table limits for the logical TCAM tables. */
static int cfa_tcam_mgr_table_limits_set(struct cfa_tcam_mgr_data
						*tcam_mgr_data, struct tf *tfp,
					 struct cfa_tcam_mgr_init_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	unsigned int dir, type;
	int start, stride;

	if (!parms)
		return 0;

	for (dir = 0; dir < ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables);
			dir++)
		for (type = 0;
		     type <
			ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir]);
		     type++) {
			table_data =
				&tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];
			/* If num_rows is zero, then TCAM Manager did not
			 * allocate any row storage for that table so cannot
			 * manage it.
			 */
			if (!table_data->num_rows)
				continue;
			start  = parms->resc[dir][type].start;
			stride = parms->resc[dir][type].stride;
			if (start % table_data->max_slices > 0) {
				netdev_dbg(tfp->bp->dev,
					   "%s: %s Resrces(%d) not on row boundary\n",
					   tf_dir_2_str(dir),
					   cfa_tcam_mgr_tbl_2_str(type),
					   start);
				netdev_dbg(tfp->bp->dev,
					   "%s: Start:%d, num slices:%d\n",
					   tf_dir_2_str(dir), start,
					   table_data->max_slices);
				return -EINVAL;
			}
			if (stride % table_data->max_slices > 0) {
				netdev_dbg(tfp->bp->dev,
					   "%s: %s Resrces(%d) not on row boundary.\n",
					   tf_dir_2_str(dir),
					   cfa_tcam_mgr_tbl_2_str(type),
					   stride);
				netdev_dbg(tfp->bp->dev,
					   "%s: Stride:%d, num slices:%d\n",
					   tf_dir_2_str(dir), stride,
					   table_data->max_slices);
				return -EINVAL;
			}
			if (!stride) {
				table_data->start_row	= 0;
				table_data->end_row	= 0;
				table_data->max_entries = 0;
			} else {
				table_data->start_row = start /
					table_data->max_slices;
				table_data->end_row = table_data->start_row +
					(stride / table_data->max_slices) - 1;
				table_data->max_entries =
					table_data->max_slices *
					(table_data->end_row -
					 table_data->start_row + 1);
			}
		}

	return 0;
}

static int cfa_tcam_mgr_bitmap_alloc(struct tf *tfp, struct cfa_tcam_mgr_data *tcam_mgr_data)
{
	unsigned long logical_id_bmp_size;
	unsigned long *logical_id_bmp;
	int max_entries;

	if (!tcam_mgr_data->cfa_tcam_mgr_max_entries)
		return -EINVAL;

	max_entries = tcam_mgr_data->cfa_tcam_mgr_max_entries;

	logical_id_bmp_size = (sizeof(unsigned long) *
				(((max_entries - 1) / sizeof(unsigned long)) + 1));
	logical_id_bmp = vzalloc(logical_id_bmp_size);
	if (!logical_id_bmp)
		return -ENOMEM;

	tcam_mgr_data->logical_id_bmp = logical_id_bmp;
	tcam_mgr_data->logical_id_bmp_size = max_entries;

	netdev_dbg(tfp->bp->dev, "session bitmap size is %lu\n",
		   tcam_mgr_data->logical_id_bmp_size);

	return 0;
}

static void cfa_tcam_mgr_uninit(struct tf *tfp,
				enum cfa_tcam_mgr_device_type type)
{
	switch (type) {
	case CFA_TCAM_MGR_DEVICE_TYPE_WH:
	case CFA_TCAM_MGR_DEVICE_TYPE_SR:
		cfa_tcam_mgr_uninit_p4(tfp);
		break;
	case CFA_TCAM_MGR_DEVICE_TYPE_THOR:
		cfa_tcam_mgr_uninit_p58(tfp);
		break;
	default:
		netdev_dbg(tfp->bp->dev, "No such device %d\n", type);
		return;
	}
}

int cfa_tcam_mgr_init(struct tf *tfp, enum cfa_tcam_mgr_device_type type,
		      struct cfa_tcam_mgr_init_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	unsigned int dir, tbl_type;
	struct tf_session *tfs;
	int rc;

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	switch (type) {
	case CFA_TCAM_MGR_DEVICE_TYPE_WH:
	case CFA_TCAM_MGR_DEVICE_TYPE_SR:
		rc = cfa_tcam_mgr_init_p4(tfp);
		break;
	case CFA_TCAM_MGR_DEVICE_TYPE_THOR:
		rc = cfa_tcam_mgr_init_p58(tfp);
		break;
	default:
		netdev_dbg(tfp->bp->dev, "No such device %d\n", type);
		return -ENODEV;
	}
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	rc = cfa_tcam_mgr_table_limits_set(tcam_mgr_data, tfp, parms);
	if (rc)
		return rc;

	/* Now calculate the max entries per table and global max entries based
	 * on the updated table limits.
	 */
	tcam_mgr_data->cfa_tcam_mgr_max_entries = 0;
	for (dir = 0; dir < ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables);
			dir++)
		for (tbl_type = 0;
		     tbl_type <
			ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir]);
		     tbl_type++) {
			table_data =
				&tcam_mgr_data->cfa_tcam_mgr_tables[dir]
								[tbl_type];
			/* If num_rows is zero, then TCAM Manager did not
			 * allocate any row storage for that table so cannot
			 * manage it.
			 */
			if (!table_data->num_rows) {
				table_data->start_row = 0;
				table_data->end_row = 0;
				table_data->max_entries = 0;
			} else if (table_data->end_row >=
				   table_data->num_rows) {
				netdev_dbg(tfp->bp->dev,
					   "%s: %s End row is OOR(%d >= %d)\n",
					   tf_dir_2_str(dir),
					   cfa_tcam_mgr_tbl_2_str((enum cfa_tcam_mgr_tbl_type)type),
					   table_data->end_row,
					   table_data->num_rows);
				return -EFAULT;
			} else if (!table_data->max_entries &&
				   !table_data->start_row &&
				   !table_data->end_row) {
				/* Nothing to do */
			} else {
				table_data->max_entries =
					table_data->max_slices *
					(table_data->end_row -
					 table_data->start_row + 1);
			}
			tcam_mgr_data->cfa_tcam_mgr_max_entries +=
				table_data->max_entries;
		}

	rc = cfa_tcam_mgr_bitmap_alloc(tfp, tcam_mgr_data);
	if (rc)
		return rc;

	rc = cfa_tcam_mgr_hwops_init(tcam_mgr_data, type);
	if (rc)
		return rc;

	if (parms)
		parms->max_entries = tcam_mgr_data->cfa_tcam_mgr_max_entries;

	netdev_dbg(tfp->bp->dev, "Global TCAM table initialized\n");

	return 0;
}

int cfa_tcam_mgr_qcaps(struct tf *tfp, struct cfa_tcam_mgr_qcaps_parms *parms)
{
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_session *tfs;
	unsigned int type;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		netdev_dbg(tfp->bp->dev,
			   "No TCAM data created for session\n");
		return -EPERM;
	}

	/* This code will indicate if TCAM Manager is managing a logical TCAM
	 * table or not.  If not, then the physical TCAM will have to be
	 * accessed using the traditional methods.
	 */
	parms->rx_tcam_supported = 0;
	parms->tx_tcam_supported = 0;
	for (type = 0; type < CFA_TCAM_MGR_TBL_TYPE_MAX; type++) {
		if (tcam_mgr_data->cfa_tcam_mgr_tables[TF_DIR_RX]
				[type].max_entries > 0 &&
		    tcam_mgr_data->cfa_tcam_mgr_tables[TF_DIR_RX]
				[type].hcapi_type > 0)
			parms->rx_tcam_supported |=
				1 << cfa_tcam_mgr_get_phys_table_type(type);
		if (tcam_mgr_data->cfa_tcam_mgr_tables[TF_DIR_TX]
				[type].max_entries > 0 &&
		    tcam_mgr_data->cfa_tcam_mgr_tables[TF_DIR_TX]
				[type].hcapi_type > 0)
			parms->tx_tcam_supported |=
				1 << cfa_tcam_mgr_get_phys_table_type(type);
	}

	return 0;
}

static int cfa_tcam_mgr_validate_tcam_cnt(struct tf *tfp,
					  struct cfa_tcam_mgr_data
						*tcam_mgr_data,
					  u16 tcam_cnt[]
						[CFA_TCAM_MGR_TBL_TYPE_MAX])
{
	struct cfa_tcam_mgr_table_data *table_data;
	unsigned int dir, type;
	u16 requested_cnt;

	/* Validate session request */
	for (dir = 0; dir < ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables);
			dir++) {
		for (type = 0;
		     type < ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir]);
		     type++) {
			table_data =
				&tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];
			requested_cnt = tcam_cnt[dir][type];
			/* Only check if table supported (max_entries > 0). */
			if (table_data->max_entries > 0 &&
			    requested_cnt > table_data->max_entries) {
				netdev_err(tfp->bp->dev,
					   "%s: %s Requested %d, available %d\n",
					   tf_dir_2_str(dir),
					   cfa_tcam_mgr_tbl_2_str(type),
					   requested_cnt,
					   table_data->max_entries);
				return -ENOSPC;
			}
		}
	}

	return 0;
}

static int cfa_tcam_mgr_free_entries(struct tf *tfp)
{
	struct cfa_tcam_mgr_free_parms free_parms;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_session *tfs;
	int entry_id;
	int rc;

	netdev_dbg(tfp->bp->dev, "Unbinding session\n");

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	memset(&free_parms, 0, sizeof(free_parms));
	/* Since we are freeing all pending TCAM entries (which is typically
	 * done during tcam_unbind), we don't know the type of each entry.
	 * So we set the type to MAX as a hint to cfa_tcam_mgr_free() to
	 * figure out the actual type. We need to set it through each
	 * iteration in the loop below; otherwise, the type determined for
	 * the first entry would be used for subsequent entries that may or
	 * may not be of the same type, resulting in errors.
	 */
	for (entry_id = 0; entry_id < tcam_mgr_data->cfa_tcam_mgr_max_entries;
	     entry_id++) {
		if (test_bit(entry_id, tcam_mgr_data->logical_id_bmp)) {
			clear_bit(entry_id, tcam_mgr_data->logical_id_bmp);

			free_parms.id = entry_id;
			free_parms.type = CFA_TCAM_MGR_TBL_TYPE_MAX;
			cfa_tcam_mgr_free(tfp, &free_parms);
		}
	}

	return 0;
}

int cfa_tcam_mgr_bind(struct tf *tfp, struct cfa_tcam_mgr_cfg_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	enum cfa_tcam_mgr_device_type device_type;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int prev_max_entries;
	unsigned int type;
	int start, stride;
	unsigned int dir;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	switch (dev->type) {
	case TF_DEVICE_TYPE_P4:
		device_type = CFA_TCAM_MGR_DEVICE_TYPE_WH;
		break;
	case TF_DEVICE_TYPE_P5:
		device_type = CFA_TCAM_MGR_DEVICE_TYPE_THOR;
		break;
	default:
		netdev_dbg(tfp->bp->dev, "No such device %d\n", dev->type);
		return -ENODEV;
	}

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		rc = cfa_tcam_mgr_init(tfp, device_type, NULL);
		if (rc)
			return rc;
		tcam_mgr_data = tfs->tcam_mgr_handle;
	}

	if (parms->num_elements !=
		ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir])) {
		netdev_dbg(tfp->bp->dev,
			   "Element count:%d != table count:%zu\n",
			   parms->num_elements,
			   ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir]));
		return -EINVAL;
	}

	/* Only managing one session. resv_res contains the resources allocated
	 * to this session by the resource manager.  Update the limits on TCAMs.
	 */
	for (dir = 0; dir < ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables);
		dir++) {
		for (type = 0;
		     type <
			ARRAY_SIZE(tcam_mgr_data->cfa_tcam_mgr_tables[dir]);
		     type++) {
			table_data =
				&tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];
			prev_max_entries = table_data->max_entries;
			/* In AFM logical tables, max_entries is initialized to
			 * zero.  These logical tables are not used when TCAM
			 * Manager is in the core so skip.
			 */
			if (!prev_max_entries)
				continue;
			start  = parms->resv_res[dir][type].start;
			stride = parms->resv_res[dir][type].stride;
			if (start % table_data->max_slices > 0) {
				netdev_dbg(tfp->bp->dev,
					   "%s: %s Resource:%d not on row boundary\n",
					   tf_dir_2_str(dir),
					   cfa_tcam_mgr_tbl_2_str(type),
					   start);
				netdev_dbg(tfp->bp->dev,
					   "%s: Start:%d, num slices:%d\n",
					   tf_dir_2_str(dir), start,
					   table_data->max_slices);
				cfa_tcam_mgr_free_entries(tfp);
				return -EINVAL;
			}
			if (stride % table_data->max_slices > 0) {
				netdev_dbg(tfp->bp->dev,
					   "%s: %s Resource:%d not on row boundary\n",
					   tf_dir_2_str(dir),
					   cfa_tcam_mgr_tbl_2_str(type),
					   stride);
				netdev_dbg(tfp->bp->dev,
					   "%s: Stride:%d num slices:%d\n",
					   tf_dir_2_str(dir), stride,
					   table_data->max_slices);
				cfa_tcam_mgr_free_entries(tfp);
				return -EINVAL;
			}
			if (!stride) {
				table_data->start_row	= 0;
				table_data->end_row	= 0;
				table_data->max_entries = 0;
			} else {
				table_data->start_row = start /
					table_data->max_slices;
				table_data->end_row = table_data->start_row +
					(stride / table_data->max_slices) - 1;
				table_data->max_entries =
					table_data->max_slices *
					(table_data->end_row -
					 table_data->start_row + 1);
			}
			tcam_mgr_data->cfa_tcam_mgr_max_entries +=
				(table_data->max_entries - prev_max_entries);
		}
	}

	rc = cfa_tcam_mgr_validate_tcam_cnt(tfp, tcam_mgr_data,
					    parms->tcam_cnt);
	if (rc) {
		cfa_tcam_mgr_free_entries(tfp);
		return rc;
	}

	cfa_tcam_mgr_tables_dump(tfp, TF_DIR_MAX, CFA_TCAM_MGR_TBL_TYPE_MAX);
	return 0;
}

int cfa_tcam_mgr_unbind(struct tf *tfp)
{
	enum cfa_tcam_mgr_device_type device_type;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	int rc;

	if (!tfp)
		return -EINVAL;

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve the device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	switch (dev->type) {
	case TF_DEVICE_TYPE_P4:
		device_type = CFA_TCAM_MGR_DEVICE_TYPE_WH;
		break;
	case TF_DEVICE_TYPE_P5:
		device_type = CFA_TCAM_MGR_DEVICE_TYPE_THOR;
		break;
	default:
		netdev_dbg(tfp->bp->dev, "No such device %d\n", dev->type);
		return -ENODEV;
	}

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		netdev_dbg(tfp->bp->dev, "No TCAM data created for session\n");
		return -EPERM;
	}

	cfa_tcam_mgr_free_entries(tfp);
	cfa_tcam_mgr_uninit(tfp, device_type);

	return 0;
}

static int cfa_tcam_mgr_alloc_entry(struct tf *tfp,
				    struct cfa_tcam_mgr_data *tcam_mgr_data,
				    enum tf_dir dir,
				    enum cfa_tcam_mgr_tbl_type type)
{
	u32 free_idx;

	free_idx = find_first_zero_bit(tcam_mgr_data->logical_id_bmp,
				       tcam_mgr_data->logical_id_bmp_size);
	if (free_idx == tcam_mgr_data->logical_id_bmp_size) {
		netdev_dbg(tfp->bp->dev, "Table full (session)\n");
		return -ENOSPC;
	}

	/* Set the bit in the bitmap. set_bit */
	set_bit(free_idx, tcam_mgr_data->logical_id_bmp);

	return free_idx;
}

static int cfa_tcam_mgr_free_entry(struct tf *tfp,
				   struct cfa_tcam_mgr_data *tcam_mgr_data,
				   unsigned int entry_id, enum tf_dir dir,
				   enum cfa_tcam_mgr_tbl_type type)
{
	if (entry_id >= tcam_mgr_data->logical_id_bmp_size)
		return -EINVAL;

	clear_bit(entry_id, tcam_mgr_data->logical_id_bmp);
	netdev_dbg(tfp->bp->dev, "Removed logical id from entry %d\n", entry_id);

	return 0;
}

int cfa_tcam_mgr_alloc(struct tf *tfp, struct cfa_tcam_mgr_alloc_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct cfa_tcam_mgr_table_rows_0 *row;
	struct cfa_tcam_mgr_entry_data entry;
	struct tf_session *tfs;
	int key_slices, rc;
	int dir, tbl_type;
	int new_entry_id;

	if (!tfp || !parms)
		return -EINVAL;

	dir = parms->dir;
	tbl_type = parms->type;

	if (dir >= TF_DIR_MAX) {
		netdev_dbg(tfp->bp->dev, "Invalid direction: %d.\n", dir);
		return -EINVAL;
	}

	if (tbl_type >= CFA_TCAM_MGR_TBL_TYPE_MAX) {
		netdev_dbg(tfp->bp->dev, "%s: Invalid table type: %d.\n",
			   tf_dir_2_str(dir), tbl_type);
		return -EINVAL;
	}

	if (parms->priority > TF_TCAM_PRIORITY_MAX) {
		netdev_dbg(tfp->bp->dev, "%s: Priority (%u) out of range (%u -%u).\n",
			   tf_dir_2_str(dir), parms->priority,
			   TF_TCAM_PRIORITY_MIN,
			   TF_TCAM_PRIORITY_MAX);
	}

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		netdev_dbg(tfp->bp->dev, "No TCAM data created for session\n");
		return -EPERM;
	}

	table_data = &tcam_mgr_data->cfa_tcam_mgr_tables[dir][tbl_type];

	if (!parms->key_size ||
	    parms->key_size > table_data->row_width) {
		netdev_dbg(tfp->bp->dev,
			   "%s: Invalid key size:%d (range 1-%d)\n",
			   tf_dir_2_str(dir), parms->key_size,
			   table_data->row_width);
		return -EINVAL;
	}

	/* Check global limits */
	if (table_data->used_entries >=
	    table_data->max_entries) {
		netdev_dbg(tfp->bp->dev, "%s: %s Table full\n",
			   tf_dir_2_str(parms->dir),
			   cfa_tcam_mgr_tbl_2_str(parms->type));
		return -ENOSPC;
	}

	/* There is room, now increment counts and allocate an entry. */
	new_entry_id = cfa_tcam_mgr_alloc_entry(tfp, tcam_mgr_data, parms->dir,
						parms->type);
	if (new_entry_id < 0)
		return new_entry_id;

	memset(&entry, 0, sizeof(entry));
	entry.ref_cnt++;

	netdev_dbg(tfp->bp->dev, "Allocated entry ID %d.\n", new_entry_id);

	key_slices = cfa_tcam_mgr_get_num_slices(parms->key_size,
						 (table_data->row_width /
						  table_data->max_slices));

	row = cfa_tcam_mgr_empty_row_alloc(tcam_mgr_data, tfp, parms, &entry,
					   new_entry_id, key_slices);
	if (!row) {
		netdev_dbg(tfp->bp->dev, "%s: %s Table full (HW)\n",
			   tf_dir_2_str(parms->dir),
			   cfa_tcam_mgr_tbl_2_str(parms->type));
		cfa_tcam_mgr_free_entry(tfp, tcam_mgr_data, new_entry_id,
					parms->dir, parms->type);
		return -ENOSPC;
	}

	memcpy(&tcam_mgr_data->entry_data[new_entry_id],
	       &entry,
	       sizeof(tcam_mgr_data->entry_data[new_entry_id]));
	table_data->used_entries += 1;

	cfa_tcam_mgr_entry_insert(tcam_mgr_data, tfp, new_entry_id, &entry);

	parms->id = new_entry_id;

	return 0;
}

int cfa_tcam_mgr_free(struct tf *tfp, struct cfa_tcam_mgr_free_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct cfa_tcam_mgr_entry_data *entry;
	struct cfa_tcam_mgr_table_rows_0 *row;
	int row_size, rc, new_row_to_free;
	struct tf_session *tfs;
	u16 id;

	if (!tfp || !parms)
		return -EINVAL;

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		netdev_dbg(tfp->bp->dev, "No TCAM data created for session\n");
		return -EPERM;
	}

	id = parms->id;
	entry = cfa_tcam_mgr_entry_get(tcam_mgr_data, id);
	if (!entry) {
		netdev_dbg(tfp->bp->dev, "Entry %d not found\n", id);
		return -EINVAL;
	}

	if (!entry->ref_cnt) {
		netdev_dbg(tfp->bp->dev, "Entry %d not in use\n", id);
		return -EINVAL;
	}

	/* If the TCAM type is CFA_TCAM_MGR_TBL_TYPE_MAX, that implies that the
	 * caller does not know the table or direction of the entry and TCAM
	 * Manager must search the tables to find out which table has the entry
	 * installed.
	 *
	 * This would be the case if RM has informed TCAM Mgr that an entry must
	 * be freed.  Clients (sessions, AFM) should always know the type and
	 * direction of the table where an entry is installed.
	 */
	if (parms->type == CFA_TCAM_MGR_TBL_TYPE_MAX) {
		/* Need to search for the entry in the tables */
		rc = cfa_tcam_mgr_entry_find(tcam_mgr_data, id, &parms->dir,
					     &parms->type);
		if (rc) {
			netdev_dbg(tfp->bp->dev,
				   "Entry %d not in tables\n", id);
			return rc;
		}
		netdev_dbg(tfp->bp->dev, "%s: id: %d dir: 0x%x type: 0x%x\n",
			   __func__, id, parms->dir, parms->type);
	}

	table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[parms->dir][parms->type];
	parms->hcapi_type = table_data->hcapi_type;

	row_size = cfa_tcam_mgr_row_size_get(tcam_mgr_data, parms->dir,
					     parms->type);

	row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows, entry->row,
				       row_size);

	entry->ref_cnt--;

	cfa_tcam_mgr_free_entry(tfp, tcam_mgr_data, id, parms->dir,
				parms->type);

	if (!entry->ref_cnt) {
		netdev_dbg(tfp->bp->dev, "Freeing entry %d, row %d, slice %d.\n",
			   id, entry->row, entry->slice);
		cfa_tcam_mgr_entry_free_msg(tcam_mgr_data, tfp,
					    parms, entry->row,
					    entry->slice * row->entry_size,
					    table_data->row_width /
					    table_data->max_slices *
					    row->entry_size,
					    table_data->result_size,
					    table_data->max_slices);
		ROW_ENTRY_CLEAR(row, entry->slice);

		new_row_to_free = entry->row;
		cfa_tcam_mgr_rows_combine(tcam_mgr_data, tfp, parms,
					  table_data, new_row_to_free);

		if (!ROW_INUSE(row)) {
			cfa_tcam_mgr_rows_compact(tcam_mgr_data, tfp,
						  parms, table_data,
						  &new_row_to_free,
						  new_row_to_free);
			if (new_row_to_free >= 0)
				cfa_tcam_mgr_entry_free_msg(tcam_mgr_data,
							    tfp, parms,
							    new_row_to_free, 0,
							    table_data->row_width,
							    table_data->result_size,
							    table_data->max_slices);
		}

		cfa_tcam_mgr_entry_delete(tcam_mgr_data, tfp, id);
		table_data->used_entries -= 1;
		netdev_dbg(tfp->bp->dev, "Freed entry %d.\n", id);
	} else {
		netdev_dbg(tfp->bp->dev, "Entry %d ref cnt = %d.\n", id, entry->ref_cnt);
	}

	return 0;
}

int cfa_tcam_mgr_set(struct tf *tfp, struct cfa_tcam_mgr_set_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct cfa_tcam_mgr_entry_data *entry;
	struct cfa_tcam_mgr_table_rows_0 *row;
	int entry_size_in_bytes;
	struct tf_session *tfs;
	int row_size;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		netdev_dbg(tfp->bp->dev, "No TCAM data created for session\n");
		return -EPERM;
	}

	entry = cfa_tcam_mgr_entry_get(tcam_mgr_data, parms->id);
	if (!entry) {
		netdev_dbg(tfp->bp->dev, "Entry %d not found\n", parms->id);
		return -EINVAL;
	}

	table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[parms->dir][parms->type];
	parms->hcapi_type = table_data->hcapi_type;

	row_size = cfa_tcam_mgr_row_size_get(tcam_mgr_data, parms->dir,
					     parms->type);
	row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows, entry->row,
				       row_size);

	entry_size_in_bytes = table_data->row_width /
			      table_data->max_slices *
			      row->entry_size;
	if (parms->key_size != entry_size_in_bytes) {
		netdev_dbg(tfp->bp->dev,
			   "Key size(%d) is different from entry size(%d).\n",
			   parms->key_size, entry_size_in_bytes);
		return -EINVAL;
	}

	rc = cfa_tcam_mgr_entry_set_msg(tcam_mgr_data, tfp, parms,
					entry->row,
					entry->slice * row->entry_size,
					table_data->max_slices);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "Failed to set TCAM data.\n");
		return rc;
	}

	netdev_dbg(tfp->bp->dev, "Set data for entry %d\n", parms->id);

	return 0;
}

int cfa_tcam_mgr_get(struct tf *tfp, struct cfa_tcam_mgr_get_parms *parms)
{
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct cfa_tcam_mgr_entry_data *entry;
	struct cfa_tcam_mgr_table_rows_0 *row;
	struct tf_session *tfs;
	int row_size;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		netdev_dbg(tfp->bp->dev, "No TCAM data created for session\n");
		return -EPERM;
	}

	entry = cfa_tcam_mgr_entry_get(tcam_mgr_data, parms->id);
	if (!entry) {
		netdev_dbg(tfp->bp->dev, "Entry %d not found.\n", parms->id);
		return -EINVAL;
	}

	table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[parms->dir][parms->type];
	parms->hcapi_type = table_data->hcapi_type;

	row_size = cfa_tcam_mgr_row_size_get(tcam_mgr_data, parms->dir,
					     parms->type);
	row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows, entry->row,
				       row_size);

	rc = cfa_tcam_mgr_entry_get_msg(tcam_mgr_data, tfp, parms,
					entry->row,
					entry->slice * row->entry_size,
					table_data->max_slices);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "Failed to read from TCAM.\n");
		return rc;
	}

	return 0;
}

void cfa_tcam_mgr_rows_dump(struct tf *tfp, enum tf_dir dir,
			    enum cfa_tcam_mgr_tbl_type type)
{
	struct cfa_tcam_mgr_table_rows_0 *table_row;
	struct cfa_tcam_mgr_table_data *table_data;
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_session *tfs;
	bool row_found = false;
	bool empty_row = false;
	int i, row, row_size;
	int rc;

	if (dir >= TF_DIR_MAX) {
		netdev_dbg(tfp->bp->dev, "Must specify a valid direction (0-%d).\n",
			   TF_DIR_MAX - 1);
		return;
	}
	if (type >= CFA_TCAM_MGR_TBL_TYPE_MAX) {
		netdev_dbg(tfp->bp->dev, "Must specify a valid type (0-%d).\n",
			   CFA_TCAM_MGR_TBL_TYPE_MAX - 1);
		return;
	}

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		netdev_dbg(tfp->bp->dev, "No TCAM data created for session\n");
		return;
	}

	table_data = &tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];
	row_size = cfa_tcam_mgr_row_size_get(tcam_mgr_data, dir, type);

	netdev_dbg(tfp->bp->dev, "\nTCAM Rows:\n");
	netdev_dbg(tfp->bp->dev,
		   "Rows for direction %s, Logical table type %s\n",
		   tf_dir_2_str(dir), cfa_tcam_mgr_tbl_2_str(type));
	netdev_dbg(tfp->bp->dev, "Managed rows %d-%d\n",
		   table_data->start_row, table_data->end_row);

	netdev_dbg(tfp->bp->dev, "Index Pri   Size  Entry IDs\n");
	netdev_dbg(tfp->bp->dev, "                  Sl 0");
	for (i = 1; i < table_data->max_slices; i++)
		netdev_dbg(tfp->bp->dev, "  Sl %d", i);
	netdev_dbg(tfp->bp->dev, "\n");
	for (row = table_data->start_row; row <= table_data->end_row; row++) {
		table_row = cfa_tcam_mgr_row_ptr_get(table_data->tcam_rows, row,
						     row_size);
		if (ROW_INUSE(table_row)) {
			empty_row = false;
			netdev_dbg(tfp->bp->dev, "%5u %5u %4u", row,
				   TF_TCAM_PRIORITY_MAX - table_row->priority - 1,
				   table_row->entry_size);
			for (i = 0;
			     i < table_data->max_slices / table_row->entry_size;
			     i++) {
				if (ROW_ENTRY_INUSE(table_row, i))
					netdev_dbg(tfp->bp->dev, " %5u",
						   table_row->entries[i]);
				else
					netdev_dbg(tfp->bp->dev, "     x");
			}
			netdev_dbg(tfp->bp->dev, "\n");
			row_found = true;
		} else if (!empty_row) {
			empty_row = true;
			netdev_dbg(tfp->bp->dev, "\n");
		}
	}

	if (!row_found)
		netdev_dbg(tfp->bp->dev, "No rows in use.\n");
}

static void cfa_tcam_mgr_table_dump(struct cfa_tcam_mgr_data *tcam_mgr_data,
				    struct tf *tfp, enum tf_dir dir,
				    enum cfa_tcam_mgr_tbl_type type)
{
	struct cfa_tcam_mgr_table_data *table_data =
		&tcam_mgr_data->cfa_tcam_mgr_tables[dir][type];

	netdev_dbg(tfp->bp->dev, "%3s %-22s %5u %5u %5u %5u %6u %7u %2u\n",
		   tf_dir_2_str(dir),
		   cfa_tcam_mgr_tbl_2_str(type), table_data->row_width,
		   table_data->num_rows, table_data->start_row,
		   table_data->end_row, table_data->max_entries,
		   table_data->used_entries, table_data->max_slices);
}

#define TABLE_DUMP_HEADER \
	"Dir Table                  Width  Rows Start   End " \
	"MaxEnt UsedEnt Slices\n"

void cfa_tcam_mgr_tables_dump(struct tf *tfp, enum tf_dir dir,
			      enum cfa_tcam_mgr_tbl_type type)
{
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct tf_session *tfs;
	int rc;

	netdev_dbg(tfp->bp->dev, "\nTCAM Table(s)\n");
	netdev_dbg(tfp->bp->dev, TABLE_DUMP_HEADER);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		netdev_dbg(tfp->bp->dev, "No TCAM data created for session\n");
		return;
	}

	if (dir >= TF_DIR_MAX) {
		/* Iterate over all directions */
		for (dir = 0; dir < TF_DIR_MAX; dir++) {
			if (type >= CFA_TCAM_MGR_TBL_TYPE_MAX) {
				/* Iterate over all types */
				for (type = 0;
				     type < CFA_TCAM_MGR_TBL_TYPE_MAX;
				     type++) {
					cfa_tcam_mgr_table_dump(tcam_mgr_data,
								tfp, dir, type);
				}
			} else {
				/* Display a specific type */
				cfa_tcam_mgr_table_dump(tcam_mgr_data, tfp,
							dir, type);
			}
		}
	} else if (type >= CFA_TCAM_MGR_TBL_TYPE_MAX) {
		/* Iterate over all types for a direction */
		for (type = 0; type < CFA_TCAM_MGR_TBL_TYPE_MAX; type++)
			cfa_tcam_mgr_table_dump(tcam_mgr_data, tfp, dir, type);
	} else {
		/* Display a specific direction and type */
		cfa_tcam_mgr_table_dump(tcam_mgr_data, tfp, dir, type);
	}
}

#define ENTRY_DUMP_HEADER "Entry RefCnt  Row Slice\n"

void cfa_tcam_mgr_entries_dump(struct tf *tfp)
{
	struct cfa_tcam_mgr_data *tcam_mgr_data;
	struct cfa_tcam_mgr_entry_data *entry;
	bool entry_found = false;
	struct tf_session *tfs;
	u16 id;
	int rc;

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return;

	tcam_mgr_data = tfs->tcam_mgr_handle;
	if (!tcam_mgr_data) {
		netdev_dbg(tfp->bp->dev, "No TCAM data created for session\n");
		return;
	}

	netdev_dbg(tfp->bp->dev, "\nGlobal Maximum Entries: %d\n\n",
		   tcam_mgr_data->cfa_tcam_mgr_max_entries);
	netdev_dbg(tfp->bp->dev, "TCAM Entry Table:\n");
	for (id = 0; id < tcam_mgr_data->cfa_tcam_mgr_max_entries; id++) {
		if (tcam_mgr_data->entry_data[id].ref_cnt > 0) {
			entry = &tcam_mgr_data->entry_data[id];
			if (!entry_found)
				netdev_dbg(tfp->bp->dev, ENTRY_DUMP_HEADER);
			netdev_dbg(tfp->bp->dev, "%5u %5u %5u %5u", id, entry->ref_cnt,
				   entry->row, entry->slice);
			netdev_dbg(tfp->bp->dev, "\n");
			entry_found = true;
		}
	}

	if (!entry_found)
		netdev_dbg(tfp->bp->dev, "No entries found.\n");
}
