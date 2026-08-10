/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2021 Broadcom
 * All rights reserved.
 */

#ifndef CFA_TCAM_MGR_DEVICE_H
#define CFA_TCAM_MGR_DEVICE_H

#include "cfa_tcam_mgr.h"

struct cfa_tcam_mgr_data;

/* HW OP definitions */
typedef int (*cfa_tcam_mgr_hwop_set_func_t)(struct cfa_tcam_mgr_data
					    *tcam_mgr_data,
					    struct cfa_tcam_mgr_set_parms
					    *parms, int row, int slice,
					    int max_slices);
typedef int (*cfa_tcam_mgr_hwop_get_func_t)(struct cfa_tcam_mgr_data
					    *tcam_mgr_data,
					    struct cfa_tcam_mgr_get_parms
					    *parms, int row, int slice,
					    int max_slices);
typedef int (*cfa_tcam_mgr_hwop_free_func_t)(struct cfa_tcam_mgr_data
					     *tcam_mgr_data,
					     struct cfa_tcam_mgr_free_parms
					     *parms, int row, int slice,
					     int max_slices);

struct cfa_tcam_mgr_hwops_funcs {
	cfa_tcam_mgr_hwop_set_func_t set;
	cfa_tcam_mgr_hwop_get_func_t get;
	cfa_tcam_mgr_hwop_free_func_t free;
};

/* End: HW OP definitions */

/* This identifier is to be used for one-off variable sizes.
 * Do not use it for sizing keys in an array.
 */
#define CFA_TCAM_MGR_MAX_KEY_SIZE 96

/* Note that this macro's arguments are not macro expanded due to
 * concatenation.
 */
#define TF_TCAM_TABLE_ROWS_DEF(_slices)					\
	struct cfa_tcam_mgr_table_rows_ ## _slices {			\
		u16 priority;					\
		u8 entry_size;		/* Slices per entry */	\
		u8 entry_inuse;	        /* bit[entry] set if in use */ \
		u16 entries[_slices];				\
	}

/* Have to explicitly declare this struct since some compilers don't
 * accept the GNU C extension of zero length arrays.
 */
struct cfa_tcam_mgr_table_rows_0 {
	u16 priority;
	u8 entry_size;		/* Slices per entry */
	u8 entry_inuse;	        /* bit[entry] set if in use */
	u16 entries[];
};

TF_TCAM_TABLE_ROWS_DEF(1);
TF_TCAM_TABLE_ROWS_DEF(2);
TF_TCAM_TABLE_ROWS_DEF(4);
TF_TCAM_TABLE_ROWS_DEF(8);

#define TF_TCAM_MAX_ENTRIES (L2_CTXT_TCAM_RX_MAX_ENTRIES +	\
			     L2_CTXT_TCAM_TX_MAX_ENTRIES +	\
			     PROF_TCAM_RX_MAX_ENTRIES +		\
			     PROF_TCAM_TX_MAX_ENTRIES +		\
			     WC_TCAM_RX_MAX_ENTRIES +		\
			     WC_TCAM_TX_MAX_ENTRIES +		\
			     SP_TCAM_RX_MAX_ENTRIES +		\
			     SP_TCAM_TX_MAX_ENTRIES +		\
			     CT_RULE_TCAM_RX_MAX_ENTRIES +	\
			     CT_RULE_TCAM_TX_MAX_ENTRIES +	\
			     VEB_TCAM_RX_MAX_ENTRIES +		\
			     VEB_TCAM_TX_MAX_ENTRIES)

#define	TCAM_SET_END_ROW(n) ((n) ? (n) - 1 : 0)

#define L2_CTXT_TCAM_RX_APP_LO_START	(L2_CTXT_TCAM_RX_NUM_ROWS / 2)
#define L2_CTXT_TCAM_RX_APP_HI_END	(L2_CTXT_TCAM_RX_APP_LO_START - 1)
#define L2_CTXT_TCAM_TX_APP_LO_START	(L2_CTXT_TCAM_TX_NUM_ROWS / 2)
#define L2_CTXT_TCAM_TX_APP_HI_END	(L2_CTXT_TCAM_TX_APP_LO_START - 1)

struct cfa_tcam_mgr_entry_data {
	u16 row;
	u8 slice;
	u8 ref_cnt;
};

struct cfa_tcam_mgr_table_data {
	struct cfa_tcam_mgr_table_rows_0 *tcam_rows;
	u16 hcapi_type;
	u16 num_rows;		/* Rows in physical TCAM */
	u16 start_row;		/* Where the logical TCAM starts */
	u16 end_row;		/* Where the logical TCAM ends */
	u16 max_entries;
	u16 used_entries;
	u8  row_width;		/* bytes */
	u8  result_size;	/* bytes */
	u8  max_slices;
};

struct cfa_tcam_mgr_data {
	int cfa_tcam_mgr_max_entries;
	struct cfa_tcam_mgr_table_data
		cfa_tcam_mgr_tables[TF_DIR_MAX][CFA_TCAM_MGR_TBL_TYPE_MAX];
	void *table_rows;
	struct cfa_tcam_mgr_entry_data *entry_data;
	/* Logical ids assigned for all TCAM types */
	unsigned long *logical_id_bmp;
	/* Sum of all TCAM entries allocated  */
	unsigned long logical_id_bmp_size;
	void *row_tables[TF_DIR_MAX][TF_TCAM_TBL_TYPE_MAX];
	void *rx_row_data;
	void *tx_row_data;
	struct cfa_tcam_mgr_hwops_funcs hwop_funcs;
};

#endif /* CFA_TCAM_MGR_DEVICE_H */
