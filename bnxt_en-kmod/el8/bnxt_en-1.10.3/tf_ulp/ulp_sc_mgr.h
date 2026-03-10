/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_SC_MGR_H_
#define _ULP_SC_MGR_H_

#include "bnxt_ulp.h"
#include "ulp_flow_db.h"
#include "tfc.h"

#define ULP_FLAG_SC_THREAD			BIT(0)

#define ULP_SC_ENTRY_FLAG_VALID BIT(0)

#define ULP_SC_BATCH_SIZE   64
#define ULP_SC_PAGE_SIZE  4096

struct ulp_sc_tfc_stats_cache_entry {
	struct bnxt_ulp_context *ctxt;
	u32 flags;
	u64 timestamp;
	u64 handle;
	u8  dir;
	union {
		struct {
			u64 packet_count;
			u64 byte_count;
			u64 count_fields1;
			u64 count_fields2;
		} count_fields;
		u64 count_data[4];
	};
	bool reset;
};

struct bnxt_ulp_sc_info {
	struct ulp_sc_tfc_stats_cache_entry *stats_cache_tbl;
	void	        *data_va[ULP_SC_BATCH_SIZE]; /* virtual addr */
	dma_addr_t	data_pa[ULP_SC_BATCH_SIZE]; /* physical addr */
	u32		flags;
	u32		num_entries;
	u32		num_counters;
	const struct bnxt_ulp_sc_core_ops *sc_ops;
	struct tfc_mpc_batch_info_t *batch_info;
};

struct bnxt_ulp_sc_core_ops {
	int32_t
	(*ulp_stats_cache_update)(struct tfc *tfcp,
				  int dir,
				  dma_addr_t data_pa,
				  u64 handle,
				  u16 *words,
				  struct tfc_mpc_batch_info_t *batch_info,
				  bool reset);
};

/*
 * Allocate all resources in the stats cache manager for this ulp context
 *
 * ctxt [in] The ulp context for the stats cache manager
 */
int32_t
ulp_sc_mgr_init(struct bnxt_ulp_context *ctxt);

/*
 * Release all resources in the stats cache manager for this ulp context
 *
 * ctxt [in] The ulp context for the stats cache manager
 */
void
ulp_sc_mgr_deinit(struct bnxt_ulp_context *ctxt);

/*
 * Setup the stats cache timer thread that will fetch/accumulate raw counter
 * data from the chip's internal stats caches
 *
 * ctxt [in] The ulp context for the stats cache manager
 */
int
ulp_sc_mgr_thread_start(struct bnxt_ulp_context *ctxt);

/*
 * Alarm handler that will issue the TF-Core API to fetch
 * data from the chip's internal stats caches
 *
 * ctxt [in] The ulp context for the stats cache manager
 */
void
ulp_sc_mgr_alarm_cb(void *arg);

/*
 * Check if the thread that walks through the flows is started
 *
 * ctxt [in] The ulp context for the stats cache manager
 *
 */
bool ulp_sc_mgr_thread_isstarted(struct bnxt_ulp_context *ctxt);

/*
 * Get the current counts for the given flow id
 *
 * ctxt [in] The ulp context for the stats cache manager
 * flow_id [in] The flow identifier
 * count [out] structure in which the updated counts are passed
 * back to the caller.
 *
 */
int ulp_sc_mgr_query_count_get(struct bnxt_ulp_context *ctxt,
			       u32 flow_id,
			       u64 *packets,
			       u64 *bytes,
			       unsigned long *lastused);

/*
 * Allocate a cache entry for flow
 *
 * parms [in] Various fields used to identify the flow
 * counter_handle [in] This is the action table entry identifier.
 * tbl [in] Various fields used to identify the flow
 *
 */
int ulp_sc_mgr_entry_alloc(struct bnxt_ulp_mapper_parms *parms,
			   u64 counter_handle,
			   struct bnxt_ulp_mapper_tbl_info *tbl);

/*
 * Free cache entry
 *
 * ulp [in] The ulp context for the stats cache manager
 * fid [in] The flow identifier
 *
 */
void ulp_sc_mgr_entry_free(struct bnxt_ulp_context *ulp,
			   u32 fid);

extern const struct bnxt_ulp_sc_core_ops ulp_sc_tfc_core_ops;

#endif /* _ULP_SC_MGR_H_ */
