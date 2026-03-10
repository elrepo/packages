/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#ifndef _TFO_H_
#define _TFO_H_

#include "cfa_types.h"
#include "cfa_bld_mpcops.h"
#include "tfc.h"
#include "tfc_cpm.h"
#include "bnxt_compat.h"
#include "bnxt.h"

/* Invalid Table Scope ID */
#define INVALID_TSID 0xff

/* Invalid session ID */
#define INVALID_SID 0xffff

/* Maximum number of table scopes */
#define TFC_TBL_SCOPE_MAX 32

/* Backing store/memory page levels */
enum tfc_ts_pg_tbl_lvl {
	TFC_TS_PT_LVL_0 = 0,
	TFC_TS_PT_LVL_1,
	TFC_TS_PT_LVL_2,
	TFC_TS_PT_LVL_MAX
};

/* Backing store/memory page table level config structure */
struct tfc_ts_page_tbl {
	dma_addr_t *pg_pa_tbl;	/* Array of pointers to physical addresses */
	void **pg_va_tbl;	/* Array of pointers to virtual addresses */
	u32 pg_count;		/* Number of pages in this level */
	u32 pg_size;		/* Size of each page in bytes */
};

/* Backing store/memory config structure */
struct tfc_ts_mem_cfg {
	struct tfc_ts_page_tbl pg_tbl[TFC_TS_PT_LVL_MAX]; /* page table configuration */
	u64 num_data_pages;				  /* Total number of pages */
	u64 l0_dma_addr;				  /* Physical base memory address */
	void *l0_addr;					  /* Virtual base memory address */
	int num_lvl;					  /* Number of page levels */
	u32 page_cnt[TFC_TS_PT_LVL_MAX];		  /* Page count per level */
	u32 rec_cnt;					  /* Total number of records in memory */
	u32 lkup_rec_start_offset;			  /* Offset of lkup rec start (in recs) */
	u32 entry_size;					  /* Size of record in bytes */
};

/* TFO APIs */

/**
 * Allocate a TFC object for this DPDK port/function.
 *
 * @tfo: Pointer to TFC object
 * @is_pf: Indicates whether the port is a PF.
 */
void tfo_open(void **tfo, struct bnxt *bp, bool is_pf);

/**
 * Free the TFC object for this DPDK port/function.
 *
 * @tfo: Pointer to TFC object
 */
void tfo_close(void **tfo, struct bnxt *bp);

/**
 * Validate table scope id
 *
 * @tfo: Pointer to TFC object
 * @ts_tsid: Table scope ID
 * @ts_valid True if the table scope is valid
 *
 * Return 0 for tsid within range
 */
int tfo_ts_validate(void *tfo, uint8_t ts_tsid, bool *ts_valid);

/**
 * Set the table scope configuration.
 *
 * @tfo: Pointer to TFC object
 * @ts_tsid: The table scope ID
 * @scope_type: non-shared, shared-app, global
 * @ts_app: Application type TF/AFM
 * @ts_valid: True if the table scope is valid
 * @ts_max_pools: Maximum number of pools.
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_set(void *tfo, u8 ts_tsid, enum cfa_scope_type scope_type,
	       enum cfa_app_type ts_app, bool ts_valid,
	       u16 ts_max_pools);

/**
 * Get the table scope configuration.
 *
 * @tfo: Pointer to TFC object
 * @ts_tsid: The table scope ID
 * @scope_type: non-shared, shared-app, global
 * @ts_app: Application type TF/AFM
 * @ts_valid: True if the table scope is valid
 * @ts_max_pools: Maximum number of pools
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_get(void *tfo, u8 ts_tsid, enum cfa_scope_type *scope_type,
	       enum cfa_app_type *ts_app, bool *ts_valid,
	       u16 *ts_max_pools);

/**
 * Set the table scope memory configuration for this direction.
 *
 * @tfo: Pointer to TFC object
 * @ts_tsid: The table scope ID
 * @dir: The direction (RX/TX)
 * @region: The memory region type (lookup/action)
 * @is_bs_owner: True if the caller is the owner of the backing store
 * @mem_cfg: Backing store/memory config structure
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_set_mem_cfg(void *tfo, u8 ts_tsid, enum cfa_dir dir,
		       enum cfa_region_type region, bool is_bs_owner,
		       struct tfc_ts_mem_cfg *mem_cfg);

/**
 * Get the table scope memory configuration for this direction.
 *
 * @tfo: Pointer to TFC object
 * @ts_tsid: The table scope ID
 * @dir: The direction (RX/TX)
 * @region: The memory region type (lookup/action)
 * @is_bs_owner: True if the caller is the owner of the backing store
 * @mem_cfg: Backing store/memory config structure
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_get_mem_cfg(void *tfo, u8 ts_tsid, enum cfa_dir dir,
		       enum cfa_region_type region, bool *is_bs_owner,
		       struct tfc_ts_mem_cfg *mem_cfg);

/**
 * Get the pool memory configuration for this direction.
 *
 * @tfo: Pointer to TFC object
 * @ts_tsid: The table scope ID
 * @dir: The direction (RX/TX)
 * @ts_pool: Table scope pool info
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_get_pool_info(void *tfo, u8 ts_tsid, enum cfa_dir dir,
			 enum cfa_region_type region, u16 *max_contig_rec,
			 u8 *pool_sz_exp);

/**
 * Set the pool memory configuration for this direction.
 *
 * @tfo: Pointer to TFC object
 * @ts_tsid: The table scope ID
 * @dir: The direction (RX/TX)
 * @ts_pool: Table scope pool info
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_set_pool_info(void *tfo, u8 ts_tsid, enum cfa_dir dir,
			 enum cfa_region_type region, u16 max_contig_rec,
			 u8 pool_sz_exp);

/**
 *  Get the Pool Manager instance
 *
 * @tfo: Pointer to TFC object
 * @ts_tsid: The table scope ID
 * @dir: The direction (RX/TX)
 * @cpm_lkup: Lookup CPM instance
 * @cpm_act: Action CPM instance
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_get_cpm_inst(void *tfo, u8 ts_tsid, u16 fid, enum cfa_dir dir,
			enum cfa_region_type region, struct tfc_cpm **cpm);

/**
 *  Set the Pool Manager instance
 *
 * @tfo: Pointer to TFC object
 * @ts_tsid: The table scope ID
 * @dir: The direction (RX/TX)
 * @cpm_lkup: Lookup CPM instance
 * @cpm_act: Action CPM instance
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_ts_set_cpm_inst(void *tfo, u8 ts_tsid, u16 fid, enum cfa_dir dir,
			enum cfa_region_type region, struct tfc_cpm *cpm);

/**
 * Get the MPC info reference
 *
 * @tfo: Pointer to TFC object
 * @mpc_info: MPC reference
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_mpcinfo_get(void *tfo, struct cfa_bld_mpcinfo **mpc_info);

/**
 * Set the session ID.
 *
 * @tfo: Pointer to TFC object
 * @sid: The session ID
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_sid_set(void *tfo, u16 sid);

/**
 * Get the session ID.
 *
 * @tfo: Pointer to TFC object
 * @sid: The session ID
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_sid_get(void *tfo, u16 *sid);

/**
 * Get the table scope instance manager.
 *
 * @tfo: Pointer to TFC object
 * @tim: Pointer to a pointer to the table scope instance manager
 * @ts_tsid: The table scope ID of interest
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfo_tim_get(void *tfo, void **tim, u8 ts_tsid);

#endif /* _TFO_H_ */
