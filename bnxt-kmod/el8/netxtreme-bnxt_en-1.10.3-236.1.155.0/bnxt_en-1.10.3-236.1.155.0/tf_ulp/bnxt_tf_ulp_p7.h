/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_ULP_TFC_H_
#define _BNXT_ULP_TFC_H_

#include <linux/stddef.h>
#include "bnxt.h"
#include "tfc.h"

bool
bnxt_ulp_cntxt_shared_tbl_scope_enabled(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_tfcp_set(struct bnxt_ulp_context *ulp, struct tfc *tfcp);

void *
bnxt_ulp_cntxt_tfcp_get(struct bnxt_ulp_context *ulp, enum bnxt_ulp_session_type s_type);

u32
bnxt_ulp_cntxt_tbl_scope_max_pools_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_tbl_scope_max_pools_set(struct bnxt_ulp_context *ulp_ctx,
				       u32 max);
enum tfc_tbl_scope_bucket_factor
bnxt_ulp_cntxt_em_mulitplier_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_em_mulitplier_set(struct bnxt_ulp_context *ulp_ctx,
				 enum tfc_tbl_scope_bucket_factor factor);

u32
bnxt_ulp_cntxt_num_rx_flows_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_num_rx_flows_set(struct bnxt_ulp_context *ulp_ctx, u32 num);

u32
bnxt_ulp_cntxt_num_tx_flows_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_num_tx_flows_set(struct bnxt_ulp_context *ulp_ctx, u32 num);

u16
bnxt_ulp_cntxt_em_rx_key_max_sz_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_em_rx_key_max_sz_set(struct bnxt_ulp_context *ulp_ctx,
				    u16 max);

u16
bnxt_ulp_cntxt_em_tx_key_max_sz_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_em_tx_key_max_sz_set(struct bnxt_ulp_context *ulp_ctx,
				    u16 max);

u16
bnxt_ulp_cntxt_act_rec_rx_max_sz_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_act_rec_rx_max_sz_set(struct bnxt_ulp_context *ulp_ctx,
				     int16_t max);

u16
bnxt_ulp_cntxt_act_rec_tx_max_sz_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_act_rec_tx_max_sz_set(struct bnxt_ulp_context *ulp_ctx,
				     int16_t max);
u32
bnxt_ulp_cntxt_page_sz_get(struct bnxt_ulp_context *ulp_ctxt);

int
bnxt_ulp_cntxt_page_sz_set(struct bnxt_ulp_context *ulp_ctxt,
			   u32 page_sz);

/* Function to get the tfp session details from ulp context. */
void *
bnxt_tfc_ulp_cntxt_tfp_get(struct bnxt_ulp_context *ulp,
			   enum bnxt_ulp_session_type s_type);
#endif
