// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "bnxt_tf_ulp.h"
#include "bnxt_ulp.h"
#include "bnxt_tf_common.h"
#include "ulp_sc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "tfc.h"
#include "tfc_debug.h"
#include "tfc_action_handle.h"

#define ULP_COUNTER_SIZE 16

static int
ulp_sc_tfc_stats_cache_update(struct tfc *tfcp, int dir, dma_addr_t data_pa, u64 handle,
			      u16 *words,
			      struct tfc_mpc_batch_info_t *batch_info,
			      bool reset)
{
	struct tfc_cmm_info cmm_info;
	struct tfc_cmm_clr cmm_clr = {0};

	cmm_info.dir = dir;
	cmm_info.rsubtype = CFA_RSUBTYPE_CMM_ACT;
	cmm_info.act_handle = handle;
	cmm_clr.clr = reset;

	if (reset)
		cmm_clr.sz_in_byte = ULP_COUNTER_SIZE;

	return tfc_act_get(tfcp, batch_info, &cmm_info, &cmm_clr,
			   data_pa, words);
}

const struct bnxt_ulp_sc_core_ops ulp_sc_tfc_core_ops = {
	.ulp_stats_cache_update = ulp_sc_tfc_stats_cache_update
};
