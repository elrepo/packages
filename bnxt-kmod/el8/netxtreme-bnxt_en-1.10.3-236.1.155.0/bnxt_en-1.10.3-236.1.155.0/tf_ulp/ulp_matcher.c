// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "bnxt_compat.h"
#include "ulp_matcher.h"
#include "ulp_utils.h"
#include "ulp_template_debug_proto.h"
#include <linux/vmalloc.h>


#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
static int ulp_matcher_class_list_lookup(struct ulp_tc_parser_params *params,
					 u32 *class_match_idx)
{
	struct bnxt_ulp_class_match_info *class_list = ulp_class_match_list;
	u32 idx = 0;

	while (++idx < BNXT_ULP_CLASS_MATCH_LIST_MAX_SZ) {
		/* iterate the list of class matches to find header match */
		if (class_list[idx].app_id == params->app_id &&
		    !ULP_BITMAP_CMP(&class_list[idx].hdr_bitmap,
				    &params->hdr_bitmap)) {
			/* Found the match */
			*class_match_idx = idx;
			return 0;
		}
	}

	netdev_dbg(params->ulp_ctx->bp->dev, "Did not find any matching protocol hdr\n");
	return -1;
}

static int ulp_matcher_action_list_lookup(struct ulp_tc_parser_params *params,
					  u32 *act_tmpl_idx)
{
	struct bnxt_ulp_act_match_info *act_list = ulp_act_match_list;
	u64 act_bits = params->act_bitmap.bits;
	u32 idx = 0;

	while (++idx < BNXT_ULP_ACT_MATCH_LIST_MAX_SZ) {
		/* iterate the list of action matches to find header match */
		if ((act_bits & act_list[idx].act_bitmap.bits) == act_bits) {
			/* Found the match */
			*act_tmpl_idx = act_list[idx].act_tid;
			/* set the comp field to enable action reject cond */
			ULP_COMP_FLD_IDX_WR(params, BNXT_ULP_CF_IDX_ACT_REJ_COND_EN, 1);
			return 0;
		}
	}
	return -1;
}

static int ulp_matcher_class_hdr_field_validate(struct ulp_tc_parser_params
						*params, u32 idx)
{
	struct bnxt_ulp_class_match_info *info = &ulp_class_match_list[idx];
	u64 bitmap;

	/* manadatory fields should be enabled */
	if ((params->fld_s_bitmap.bits & info->field_man_bitmap) !=
	    info->field_man_bitmap){
		netdev_dbg(params->ulp_ctx->bp->dev,
			   "mismatch in manadatory hdr fields\n");
		return -EINVAL;
	}

	/* optional fields may be enabled or not */
	bitmap = params->fld_s_bitmap.bits & (~info->field_man_bitmap);
	if ((bitmap && (bitmap & info->field_opt_bitmap) != bitmap)) {
		netdev_dbg(params->ulp_ctx->bp->dev,
			   "mismatch in optional hdr fields\n");
		netdev_dbg(params->ulp_ctx->bp->dev,
			   "bitmap:%llx opt_bitmap:%llx man_bitmap:%llx\n",
			   bitmap,
			   info->field_opt_bitmap,
			   info->field_man_bitmap);
		return -EINVAL;
	}

	return 0;
}

static u64 ulp_matcher_class_hdr_field_signature(struct ulp_tc_parser_params
						 *params, u32 idx)
{
	struct bnxt_ulp_class_match_info *info = &ulp_class_match_list[idx];

	/* remove the exclude bits */
	return (params->fld_s_bitmap.bits & ~info->field_exclude_bitmap);
}

static u64
ulp_matcher_class_wc_fld_get(u32 idx)
{
	struct bnxt_ulp_class_match_info *info = &ulp_class_match_list[idx];
	u64 bits;

	bits = info->field_opt_bitmap | info->field_man_bitmap;
	bits &= ~info->field_exclude_bitmap;
	return bits;
}

static struct ulp_matcher_class_db_node *
ulp_matcher_class_hash_lookup(struct bnxt_ulp_matcher_data *mdata,
			      struct ulp_tc_parser_params *params)
{
	struct ulp_matcher_class_db_node *matcher_node;
	struct ulp_matcher_hash_db_key key = {{ 0 }};

	/* populate the key for the search */
	key.app_id = params->app_id;
	key.hdr_bitmap = params->hdr_bitmap;

	matcher_node = rhashtable_lookup_fast(&mdata->class_matcher_db, &key,
					      mdata->class_matcher_db_ht_params);
	if (!matcher_node)
		return NULL;

	if (!matcher_node->in_use) {
		netdev_dbg(params->ulp_ctx->bp->dev,
			   "Matcher database is corrupt\n");
		return NULL;
	}
	return matcher_node;
}

static struct ulp_matcher_class_db_node *
ulp_matcher_class_hash_add(struct bnxt_ulp_matcher_data *matcher_data,
			   struct ulp_tc_parser_params *params,
			   int class_match_idx)
{
	struct ulp_matcher_class_db_node *matcher_node;
	struct ulp_matcher_hash_db_key key = {{ 0 }};
	int rc;

	/* populate the key for the search */
	key.app_id = params->app_id;
	key.hdr_bitmap = params->hdr_bitmap;

	matcher_node = kzalloc(sizeof(*matcher_node), GFP_KERNEL);
	if (!matcher_node)
		return NULL;

	matcher_node->key = key;
	matcher_node->in_use = 1;
	matcher_node->match_info_idx = class_match_idx;
	rc = rhashtable_insert_fast(&matcher_data->class_matcher_db, &matcher_node->node,
				    matcher_data->class_matcher_db_ht_params);
	if (rc) {
		netdev_dbg(params->ulp_ctx->bp->dev,
			   "unable add the entry to matcher hash: %d\n",
			   class_match_idx);
		kfree_rcu(matcher_node, rcu);
		return NULL;
	}

	netdev_dbg(params->ulp_ctx->bp->dev,
		   "Added entry: %d to matcher hash\n",
		   class_match_idx);
	return matcher_node;
}

/* Function to handle the matching of RTE Flows and validating
 * the pattern masks against the flow templates.
 */
int
ulp_matcher_pattern_match(struct ulp_tc_parser_params *params,
			  u32 *class_id)
{
	struct ulp_matcher_class_db_node *matcher_node;
	struct bnxt_ulp_class_match_info *class_match;
	struct bnxt_ulp_matcher_data *matcher_data;
	u32 class_match_idx = 0;
	u64 bits = 0;

	/* Get the matcher data for hash lookup  */
	matcher_data = (struct bnxt_ulp_matcher_data *)
		bnxt_ulp_cntxt_ptr2_matcher_data_get(params->ulp_ctx);
	if (!matcher_data) {
		netdev_dbg(params->ulp_ctx->bp->dev,
			   "Failed to get the ulp matcher data\n");
		return -EINVAL;
	}

	bits = bnxt_ulp_cntxt_ptr2_default_class_bits_get(params->ulp_ctx);
	params->hdr_bitmap.bits |= bits;

	/* search the matcher hash db for the entry  */
	matcher_node = ulp_matcher_class_hash_lookup(matcher_data, params);
	if (!matcher_node) {
		/* find  the class list entry */
		if (ulp_matcher_class_list_lookup(params, &class_match_idx))
			goto error;

		/* add it to the hash */
		matcher_node = ulp_matcher_class_hash_add(matcher_data, params,
							  class_match_idx);
		if (!matcher_node)
			goto error;
	} else {
		class_match_idx = matcher_node->match_info_idx;
	}

	class_match = &ulp_class_match_list[matcher_node->match_info_idx];

	/* perform the field bitmap validation */
	if (ulp_matcher_class_hdr_field_validate(params,
						 matcher_node->match_info_idx))
		goto error;

	/* Update the fields for further processing */
	*class_id = class_match->class_tid;
	params->class_info_idx = matcher_node->match_info_idx;
	params->flow_sig_id =
		ulp_matcher_class_hdr_field_signature(params, class_match_idx);
	params->flow_pattern_id = class_match->flow_pattern_id;
	params->wc_field_bitmap = ulp_matcher_class_wc_fld_get(class_match_idx);
	params->exclude_field_bitmap = class_match->field_exclude_bitmap;

	netdev_dbg(params->ulp_ctx->bp->dev,
		   "Found matching pattern template %u:%d\n",
		   class_match_idx, class_match->class_tid);
	return BNXT_TF_RC_SUCCESS;

error:
	netdev_err(params->ulp_ctx->bp->dev, "Did not find any matching template\n");
	netdev_err(params->ulp_ctx->bp->dev,
		   "hid:0x%x, Hdr:0x%llx Fld:0x%llx SFld:0x%llx\n",
		   class_match_idx, params->hdr_bitmap.bits,
		   params->fld_bitmap.bits, params->fld_s_bitmap.bits);
	*class_id = 0;
	return BNXT_TF_RC_ERROR;
}

static struct ulp_matcher_act_db_node *
ulp_matcher_action_hash_lookup(struct bnxt_ulp_matcher_data *mdata,
			       struct ulp_tc_parser_params *params)
{
	struct ulp_matcher_act_db_node *matcher_node;
	struct ulp_matcher_hash_db_key key = {{ 0 }};

	/* populate the key for the search */
	key.hdr_bitmap = params->act_bitmap;

	matcher_node = rhashtable_lookup_fast(&mdata->act_matcher_db, &key,
					      mdata->act_matcher_db_ht_params);
	if (!matcher_node)
		return NULL;

	return matcher_node;
}

static struct ulp_matcher_act_db_node *
ulp_matcher_action_hash_add(struct bnxt_ulp_matcher_data *matcher_data,
			    struct ulp_tc_parser_params *params,
			    int class_match_idx)
{
	struct ulp_matcher_act_db_node *matcher_node;
	struct ulp_matcher_hash_db_key key = {{ 0 }};
	int rc;

	/* populate the key for the search */
	key.hdr_bitmap = params->act_bitmap;

	matcher_node = kzalloc(sizeof(*matcher_node), GFP_KERNEL);
	if (!matcher_node)
		return NULL;

	matcher_node->key = key;
	matcher_node->match_info_idx = class_match_idx;
	rc = rhashtable_insert_fast(&matcher_data->act_matcher_db, &matcher_node->node,
				    matcher_data->act_matcher_db_ht_params);
	if (rc) {
		netdev_dbg(params->ulp_ctx->bp->dev,
			   "unable add the entry to matcher hash: %d\n",
			   class_match_idx);
		kfree_rcu(matcher_node, rcu);
		return NULL;
	}

	netdev_dbg(params->ulp_ctx->bp->dev,
		   "Added entry: %d to action hash\n",
		   class_match_idx);
	return matcher_node;
}

/* Function to handle the matching of TC Flows and validating
 * the action against the flow templates.
 */
int
ulp_matcher_action_match(struct ulp_tc_parser_params *params,
			 u32 *act_id)
{
	struct ulp_matcher_act_db_node *matcher_node;
	struct bnxt_ulp_act_match_info *action_match;
	struct bnxt_ulp_matcher_data *matcher_data;
	u32 act_match_idx = 0;
	u64 bits = 0;

	/* Get the matcher data for hash lookup */
	matcher_data = (struct bnxt_ulp_matcher_data *)
		bnxt_ulp_cntxt_ptr2_matcher_data_get(params->ulp_ctx);
	if (!matcher_data) {
		netdev_dbg(params->ulp_ctx->bp->dev, "Failed to get the ulp matcher data\n");
		return -EINVAL;
	}

	bits = bnxt_ulp_cntxt_ptr2_default_act_bits_get(params->ulp_ctx);
	params->act_bitmap.bits |= bits;

	/* search the matcher hash db for the entry */
	matcher_node = ulp_matcher_action_hash_lookup(matcher_data, params);
	if (!matcher_node) {
	       /* find the action list entry */
		if (ulp_matcher_action_list_lookup(params, &act_match_idx))
			goto error;

		/* add it to the hash */
		matcher_node = ulp_matcher_action_hash_add(matcher_data, params,
							   act_match_idx);
		if (!matcher_node)
			goto error;
	} else {
		act_match_idx = matcher_node->match_info_idx;
	}

	action_match = &ulp_act_match_list[matcher_node->match_info_idx];

	/* Update the fields for further processing */
	*act_id = action_match->act_tid;
	params->act_info_idx = matcher_node->match_info_idx;

	netdev_dbg(params->ulp_ctx->bp->dev, "Found matching action templ %u\n", act_match_idx);
	*act_id = act_match_idx;
	return BNXT_TF_RC_SUCCESS;

error:
	netdev_err(params->ulp_ctx->bp->dev, "Did not find any matching action template\n");
	netdev_err(params->ulp_ctx->bp->dev, "Hdr:%llx\n", params->act_bitmap.bits);
	*act_id = 0;
	return BNXT_TF_RC_ERROR;
}

static const struct rhashtable_params ulp_matcher_class_ht_params = {
	.head_offset = offsetof(struct ulp_matcher_class_db_node, node),
	.key_offset = offsetof(struct ulp_matcher_class_db_node, key),
	.key_len = sizeof(struct ulp_matcher_hash_db_key),
	.automatic_shrinking = true
};

static const struct rhashtable_params ulp_matcher_act_ht_params = {
	.head_offset = offsetof(struct ulp_matcher_act_db_node, node),
	.key_offset = offsetof(struct ulp_matcher_act_db_node, key),
	.key_len = sizeof(struct ulp_matcher_hash_db_key),
	.automatic_shrinking = true
};

int ulp_matcher_init(struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_matcher_data *data;
	int rc;

	data = vzalloc(sizeof(*data));
	if (!data)
		return -ENOMEM;

	if (bnxt_ulp_cntxt_ptr2_matcher_data_set(ulp_ctx, data)) {
		netdev_dbg(ulp_ctx->bp->dev,
			   "Failed to set matcher data in context\n");
		goto free_matcher_data;
	}

	data->class_matcher_db_ht_params = ulp_matcher_class_ht_params;
	rc = rhashtable_init(&data->class_matcher_db,
			     &data->class_matcher_db_ht_params);
	if (rc) {
		netdev_dbg(ulp_ctx->bp->dev,
			   "Failed to create matcher hash table\n");
		goto clear_matcher_data;
	}

	if (rc) {
		netdev_dbg(ulp_ctx->bp->dev,
			   "Failed to create class matcher hash table\n");
		goto clear_matcher_data;
	}

	data->act_matcher_db_ht_params = ulp_matcher_act_ht_params;
	rc = rhashtable_init(&data->act_matcher_db,
			     &data->act_matcher_db_ht_params);
	if (rc) {
		netdev_dbg(ulp_ctx->bp->dev,
			   "Failed to create action matcher hash table\n");
		goto clear_matcher_data;
	}

	return 0;

clear_matcher_data:
	bnxt_ulp_cntxt_ptr2_matcher_data_set(ulp_ctx, NULL);
free_matcher_data:
	vfree(data);
	return -ENOMEM;
}

static void ulp_matcher_class_hash_deinit(struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_class_match_info *class_list = ulp_class_match_list;
	struct ulp_matcher_class_db_node *matcher_node = NULL;
	struct ulp_matcher_hash_db_key key = {{ 0 }};
	struct bnxt_ulp_matcher_data *mdata;
	struct rhashtable_iter iter;
	u32 idx = 0;
	int rc;

	/* Get the matcher data for hash lookup  */
	mdata = (struct bnxt_ulp_matcher_data *)
		bnxt_ulp_cntxt_ptr2_matcher_data_get(ulp_ctx);
	if (!mdata) {
		netdev_dbg(ulp_ctx->bp->dev,
			   "Failed to get the ulp matcher data\n");
		return;
	}

	while (++idx < BNXT_ULP_CLASS_MATCH_LIST_MAX_SZ) {
		/* iterate the list of class matches to find header match */
		key.app_id = class_list[idx].app_id;
		key.hdr_bitmap.bits = class_list[idx].hdr_bitmap.bits;

		matcher_node = rhashtable_lookup_fast(&mdata->class_matcher_db, &key,
						      mdata->class_matcher_db_ht_params);
		if (!matcher_node)
			continue;
		rc = rhashtable_remove_fast(&mdata->class_matcher_db,
					    &matcher_node->node,
					    mdata->class_matcher_db_ht_params);
		if (rc) {
			netdev_dbg(ulp_ctx->bp->dev,
				   "Failed to remove: %d from class matcher hash\n",
				   idx);
			continue;
		}
		netdev_dbg(ulp_ctx->bp->dev,
			   "Removed entry: %d from matcher hash\n",
			   idx);
		kfree(matcher_node);
	}

	/* Clean up any remaining nodes that weren't matched by the static list */
	rhashtable_walk_enter(&mdata->class_matcher_db, &iter);
	rhashtable_walk_start(&iter);
	while ((matcher_node = rhashtable_walk_next(&iter)) != NULL) {
		if (IS_ERR(matcher_node))
			continue;
		rc = rhashtable_remove_fast(&mdata->class_matcher_db,
					    &matcher_node->node,
					    mdata->class_matcher_db_ht_params);
		if (rc) {
			netdev_dbg(ulp_ctx->bp->dev,
				   "Failed to remove from class matcher hash\n");
			continue;
		}
		netdev_dbg(ulp_ctx->bp->dev,
			   "Removed remaining entry from class matcher hash\n");
		kfree(matcher_node);
	}
	rhashtable_walk_stop(&iter);
	rhashtable_walk_exit(&iter);
}

static void ulp_matcher_act_hash_deinit(struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_act_match_info *act_list = ulp_act_match_list;
	struct ulp_matcher_act_db_node *matcher_node = NULL;
	struct ulp_matcher_hash_db_key key = {{ 0 }};
	struct bnxt_ulp_matcher_data *mdata;
	struct rhashtable_iter iter;
	u32 idx = 0;
	int rc;

	/* Get the matcher data for hash lookup  */
	mdata = (struct bnxt_ulp_matcher_data *)
		bnxt_ulp_cntxt_ptr2_matcher_data_get(ulp_ctx);
	if (!mdata) {
		netdev_dbg(ulp_ctx->bp->dev,
			   "Failed to get the ulp matcher data\n");
		return;
	}

	while (++idx < BNXT_ULP_ACT_MATCH_LIST_MAX_SZ) {
		/* iterate the list of act matches to find header match */
		key.app_id = act_list[idx].act_tid;
		key.hdr_bitmap.bits = act_list[idx].act_bitmap.bits;

		matcher_node = rhashtable_lookup_fast(&mdata->act_matcher_db, &key,
						      mdata->act_matcher_db_ht_params);
		if (!matcher_node)
			continue;
		rc = rhashtable_remove_fast(&mdata->act_matcher_db,
					    &matcher_node->node,
					    mdata->act_matcher_db_ht_params);
		if (rc) {
			netdev_dbg(ulp_ctx->bp->dev,
				   "Failed to remove: %d from action matcher hash\n",
				   idx);
			continue;
		}
		netdev_dbg(ulp_ctx->bp->dev,
			   "Removed entry: %d from action matcher hash\n",
			   idx);
		kfree(matcher_node);
	}

	/* Clean up any remaining nodes that weren't matched by the static list */
	rhashtable_walk_enter(&mdata->act_matcher_db, &iter);
	rhashtable_walk_start(&iter);
	while ((matcher_node = rhashtable_walk_next(&iter)) != NULL) {
		if (IS_ERR(matcher_node))
			continue;
		rc = rhashtable_remove_fast(&mdata->act_matcher_db,
					    &matcher_node->node,
					    mdata->act_matcher_db_ht_params);
		if (rc) {
			netdev_dbg(ulp_ctx->bp->dev,
				   "Failed to remove from action matcher hash\n");
			continue;
		}
		netdev_dbg(ulp_ctx->bp->dev,
			   "Removed remaining entry from action matcher hash\n");
		kfree(matcher_node);
	}
	rhashtable_walk_stop(&iter);
	rhashtable_walk_exit(&iter);
}

void ulp_matcher_deinit(struct bnxt_ulp_context *ulp_ctx)
{
	struct bnxt_ulp_matcher_data *data;

	if (!ulp_ctx)
		return;

	data = (struct bnxt_ulp_matcher_data *)
		bnxt_ulp_cntxt_ptr2_matcher_data_get(ulp_ctx);
	if (!data)
		return;

	ulp_matcher_class_hash_deinit(ulp_ctx);
	ulp_matcher_act_hash_deinit(ulp_ctx);
	rhashtable_destroy(&data->class_matcher_db);
	rhashtable_destroy(&data->act_matcher_db);
	bnxt_ulp_cntxt_ptr2_matcher_data_set(ulp_ctx, NULL);
	vfree(data);
}

#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
