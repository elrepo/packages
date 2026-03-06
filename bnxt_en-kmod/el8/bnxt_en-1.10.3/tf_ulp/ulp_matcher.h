/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef ULP_MATCHER_H_
#define ULP_MATCHER_H_

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "bnxt_tf_common.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
struct ulp_matcher_hash_db_key {
	struct ulp_tc_hdr_bitmap	hdr_bitmap;
	u8				app_id;
};

struct ulp_matcher_class_db_node {
	struct ulp_matcher_hash_db_key	key;
	struct rhash_head               node;
	u8				in_use;
	u16				match_info_idx;
	struct rcu_head                 rcu;
};

struct ulp_matcher_act_db_node {
	struct ulp_matcher_hash_db_key	key;
	struct rhash_head               node;
	struct ulp_tc_hdr_bitmap	act_bitmap;
	u16				match_info_idx;
	struct rcu_head                 rcu;
};

struct bnxt_ulp_matcher_data {
	/* hash table to store matcher class info */
	struct rhashtable               class_matcher_db;
	struct rhashtable_params        class_matcher_db_ht_params;
	struct rhashtable               act_matcher_db;
	struct rhashtable_params        act_matcher_db_ht_params;
};

#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */

int ulp_matcher_class_info_add(struct bnxt_ulp_context *ulp_ctx,
			       u16 hash_idx);
int ulp_matcher_class_info_del(struct bnxt_ulp_context *ulp_ctx,
			       u16 hash_idx);

/* Function to handle the matching of RTE Flows and validating
 * the pattern masks against the flow templates.
 */
int
ulp_matcher_pattern_match(struct ulp_tc_parser_params *params,
			  u32 *class_id);

/* Function to handle the matching of RTE Flows and validating
 * the action against the flow templates.
 */
int
ulp_matcher_action_match(struct ulp_tc_parser_params *params,
			 u32 *act_id);

int ulp_matcher_init(struct bnxt_ulp_context *ulp_ctx);
void ulp_matcher_deinit(struct bnxt_ulp_context *ulp_ctx);

#endif /* ULP_MATCHER_H_ */
