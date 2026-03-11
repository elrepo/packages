// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include "linux/kernel.h"
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_tfc.h"
#include "ulp_mapper.h"
#include "ulp_flow_db.h"
#include "cfa_resources.h"
#include "tfc_util.h"
#include "bnxt_tf_ulp_p7.h"
#include "tfc_action_handle.h"
#include "ulp_utils.h"
#include "tf_ulp/ulp_template_debug_proto.h"

#ifdef CONFIG_BNXT_FLOWER_OFFLOAD
static uint32_t CFA_RESTYPE_TO_BLKT(uint8_t idx_tbl_restype)
{
	return (idx_tbl_restype > CFA_RSUBTYPE_IDX_TBL_MAX) ?
			CFA_IDX_TBL_BLKTYPE_RXP : CFA_IDX_TBL_BLKTYPE_CFA;
}

/* Internal function to write the tcam entry */
static int
ulp_mapper_tfc_tcam_tbl_entry_write(struct bnxt_ulp_mapper_parms *parms,
				    struct bnxt_ulp_mapper_tbl_info *tbl,
				    struct ulp_blob *key,
				    struct ulp_blob *mask,
				    struct ulp_blob *remap,
				    u16 idx)
{
	u16 key_size = 0, mask_size = 0, remap_size = 0;
	struct tfc_tcam_info tfc_info = {0};
	struct tfc_tcam_data tfc_data = {0};
	struct tfc *tfcp = NULL;
	u16 fw_fid;
	int rc;

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_fid_get(parms->ulp_ctx, &fw_fid);
	if (rc)
		return rc;

	tfc_info.dir = tbl->direction;
	tfc_info.rsubtype = tbl->resource_type;
	tfc_info.id = idx;
	tfc_data.key = ulp_blob_data_get(key, &key_size);
	tfc_data.key_sz_in_bytes = ULP_BITS_2_BYTE(key_size);
	tfc_data.mask = ulp_blob_data_get(mask, &mask_size);
	tfc_data.remap = ulp_blob_data_get(remap, &remap_size);
	remap_size = ULP_BITS_2_BYTE(remap_size);
	tfc_data.remap_sz_in_bytes = remap_size;

	if (tfc_tcam_set(tfcp, fw_fid, &tfc_info, &tfc_data)) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "tcam[%s][%s][%x] write failed.\n",
			   tfc_tcam_2_str(tfc_info.rsubtype),
			   tfc_dir_2_str(tfc_info.dir), tfc_info.id);
		return -EIO;
	}
	netdev_dbg(parms->ulp_ctx->bp->dev, "tcam[%s][%s][%x] write success.\n",
		   tfc_tcam_2_str(tfc_info.rsubtype),
		   tfc_dir_2_str(tfc_info.dir), tfc_info.id);

	/* Mark action */
	rc = ulp_mapper_mark_act_ptr_process(parms, tbl);
	if (rc) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "failed mark action processing\n");
		return rc;
	}

	ulp_mapper_tcam_entry_dump(parms->ulp_ctx, "TCAM", idx, tbl, key, mask, remap);

	return rc;
}

static u32
ulp_mapper_tfc_wc_tcam_post_process(struct bnxt_ulp_context *ulp_ctx,
				    struct bnxt_ulp_device_params *dparms,
				    struct ulp_blob *key,
				    struct ulp_blob *tkey)
{
	u16 tlen, blen, clen, slice_width, num_slices, max_slices, offset;
	u32 cword, i, rc;
	int pad;
	u8 *val;

	slice_width = dparms->wc_slice_width;
	clen = dparms->wc_ctl_size_bits;
	max_slices = dparms->wc_max_slices;
	blen = ulp_blob_data_len_get(key);

	/* Get the length of the key based on number of slices and width */
	num_slices = 1;
	tlen = slice_width;
	while (tlen < blen &&
	       num_slices <= max_slices) {
		num_slices = num_slices << 1;
		tlen = tlen << 1;
	}

	if (num_slices > max_slices) {
		netdev_dbg(ulp_ctx->bp->dev, "Key size (%d) too large for WC\n", blen);
		return -EINVAL;
	}

	/* The key/mask may not be on a natural slice boundary, pad it */
	pad = tlen - blen;
	if (ulp_blob_pad_push(key, pad)) {
		netdev_dbg(ulp_ctx->bp->dev, "Unable to pad key/mask\n");
		return -EINVAL;
	}

	/* The new length accounts for the ctrl word length and num slices */
	tlen = tlen + (clen + 1) * num_slices;
	if (ulp_blob_init(tkey, tlen, key->byte_order)) {
		netdev_dbg(ulp_ctx->bp->dev, "Unable to post process wc tcam entry\n");
		return -EINVAL;
	}

	/* pad any remaining bits to do byte alignment */
	pad = (slice_width + clen) * num_slices;
	pad = ULP_BYTE_ROUND_OFF_8(pad) - pad;
	if (ulp_blob_pad_push(tkey, pad)) {
		netdev_dbg(ulp_ctx->bp->dev, "Unable to pad key/mask\n");
		return -EINVAL;
	}

	/* Build the transformed key/mask */
	cword = dparms->wc_mode_list[num_slices - 1];
	cword = cpu_to_be32(cword);
	offset = 0;
	for (i = 0; i < num_slices; i++) {
		val = ulp_blob_push_32(tkey, &cword, clen);
		if (!val) {
			netdev_dbg(ulp_ctx->bp->dev, "Key ctrl word push failed\n");
			return -EINVAL;
		}
		rc = ulp_blob_append(tkey, key, offset, slice_width);
		if (rc) {
			netdev_dbg(ulp_ctx->bp->dev, "Key blob append failed\n");
			return rc;
		}
		offset += slice_width;
	}
	blen = ulp_blob_data_len_get(tkey);
	/* reverse the blob byte wise in reverse */
	ulp_blob_perform_byte_reverse(tkey, ULP_BITS_2_BYTE(blen));
	return 0;
}

static int
ulp_mapper_tfc_tcam_tbl_process(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_device_params *dparms = parms->device_params;
	struct ulp_blob tkey, tmask; /* transform key and mask */
	u32 alloc_tcam = 0, alloc_ident = 0, write_tcam = 0;
	struct ulp_flow_db_res_params fid_parms = { 0 };
	struct ulp_blob okey, omask, *key, *mask, data;
	u16 key_sz_in_words = 0, key_sz_in_bits = 0;
	enum cfa_track_type tt = tbl->track_type;
	enum bnxt_ulp_byte_order key_byte_order;
	enum bnxt_ulp_byte_order res_byte_order;
	struct bnxt_ulp_mapper_key_info	*kflds;
	struct tfc_tcam_info tfc_inf = {0};
	struct tfc *tfcp = NULL;
	int rc = 0, free_rc = 0;
	u32 num_kflds, i;
	u32 priority;
	u16 fw_fid = 0;

	/* Set the key and mask to the original key and mask. */
	key = &okey;
	mask = &omask;

	switch (tbl->tbl_opcode) {
	case BNXT_ULP_TCAM_TBL_OPC_ALLOC_IDENT:
		alloc_ident = 1;
		break;
	case BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE:
		alloc_ident = 1;
		alloc_tcam = 1;
		write_tcam = 1;
		break;
	case BNXT_ULP_TCAM_TBL_OPC_NOT_USED:
	case BNXT_ULP_TCAM_TBL_OPC_LAST:
	default:
		netdev_dbg(parms->ulp_ctx->bp->dev, "Invalid tcam table opcode %d\n",
			   tbl->tbl_opcode);
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	if (bnxt_ulp_cntxt_fid_get(parms->ulp_ctx, &fw_fid)) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to get func_id\n");
		return -EINVAL;
	}

	/* Allocate the identifiers */
	if (alloc_ident) {
		rc = ulp_mapper_tcam_tbl_ident_alloc(parms, tbl);
		if (rc) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to alloc identifier\n");
			return rc;
		}
	}

	/* If no allocation or write is needed, then just exit */
	if (!alloc_tcam && !write_tcam)
		return rc;

	/* Initialize the blobs for write */
	if (tbl->resource_type == CFA_RSUBTYPE_TCAM_WC)
		key_byte_order = dparms->wc_key_byte_order;
	else
		key_byte_order = dparms->key_byte_order;

	res_byte_order = dparms->result_byte_order;
	if (ulp_blob_init(key, tbl->blob_key_bit_size, key_byte_order) ||
	    ulp_blob_init(mask, tbl->blob_key_bit_size, key_byte_order) ||
	    ulp_blob_init(&data, tbl->result_bit_size, res_byte_order)) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "blob inits failed.\n");
		return -EINVAL;
	}

	/* Get the key fields and update the key blob */
	if (tbl->key_recipe_opcode == BNXT_ULP_KEY_RECIPE_OPC_DYN_KEY)
		kflds = ulp_mapper_key_recipe_fields_get(parms, tbl, &num_kflds);
	else
		kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	if (!kflds || !num_kflds) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to get key fields\n");
		return -EINVAL;
	}

	for (i = 0; i < num_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_spec,
						  key, 1, "TCAM Key");
		if (rc) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "Key field set failed %s\n",
				   kflds[i].field_info_spec.description);
			return rc;
		}

		/* Setup the mask */
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_mask,
						  mask, 0, "TCAM Mask");
		if (rc) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "Mask field set failed %s\n",
				   kflds[i].field_info_mask.description);
			return rc;
		}
	}

	/* For wild card tcam perform the post process to swap the blob */
	if (tbl->resource_type == CFA_RSUBTYPE_TCAM_WC) {
		/* Sets up the slices for writing to the WC TCAM */
		rc = ulp_mapper_tfc_wc_tcam_post_process(parms->ulp_ctx, dparms, key, &tkey);
		if (rc) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to post proc WC key.\n");
			return rc;
		}
		/* Sets up the slices for writing to the WC TCAM */
		rc = ulp_mapper_tfc_wc_tcam_post_process(parms->ulp_ctx, dparms, mask, &tmask);
		if (rc) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to post proc WC mask.\n");
			return rc;
		}
		key = &tkey;
		mask = &tmask;
	}

	ulp_mapper_tcam_entry_dump(parms->ulp_ctx, "TCAM", 0, tbl, key, mask, &data);

	if (alloc_tcam) {
		tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
		if (!tfcp) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to get tfcp pointer\n");
			return -EINVAL;
		}
		/* calculate the entry priority */
		rc = ulp_mapper_priority_opc_process(parms, tbl, &priority);
		if (rc) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "entry priority process failed\n");
			return rc;
		}

		/* allocate the tcam entry, only need the length */
		(void)ulp_blob_data_get(key, &key_sz_in_bits);
		key_sz_in_words = ULP_BITS_2_BYTE(key_sz_in_bits);
		tfc_inf.dir = tbl->direction;
		tfc_inf.rsubtype = tbl->resource_type;

		rc = tfc_tcam_alloc(tfcp, fw_fid, tt, priority, key_sz_in_words, &tfc_inf);
		if (rc) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "TCAM Alloc failed, status:%d\n", rc);
			return rc;
		}

		/* Write the tcam index into the regfile*/
		if (ulp_regfile_write(parms->regfile, tbl->tbl_operand,
				      (u64)cpu_to_be64(tfc_inf.id))) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "Regfile[%d] write failed.\n",
				   tbl->tbl_operand);
			/* Need to free the tcam idx, so goto error */
			goto error;
		}
	}

	if (write_tcam) {
		/* Create the result blob */
		rc = ulp_mapper_tbl_result_build(parms, tbl, &data, "TCAM Result");
		/* write the tcam entry */
		if (!rc)
			rc = ulp_mapper_tfc_tcam_tbl_entry_write(parms,
								 tbl, key,
								 mask, &data,
								 tfc_inf.id);
	}
	if (rc)
		goto error;

	/* Add the tcam index to the flow database */
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_hndl	= tfc_inf.id;
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (rc) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to link resource to flow rc = %d\n",
			   rc);
		/* Need to free the identifier, so goto error */
		goto error;
	}

	return 0;
error:
	free_rc = tfc_tcam_free(tfcp, fw_fid, &tfc_inf);
	if (free_rc)
		netdev_dbg(parms->ulp_ctx->bp->dev, "TCAM free failed on error, status:%d\n",
			   free_rc);
	return rc;
}

static const char * const mpc_error_str[] = {
	"OK",
	"Unsupported Opcode",
	"Bad Format",
	"Invalid Scope",
	"Bad Address",
	"Cache Error",
	"EM Miss",
	"Duplicate Entry",
	"No Events",
	"EM Abort"
};

/**
 * TBD: Temporary swap until a more generic solution is designed
 *
 * @blob: A byte array that is being edited in-place
 * @block_sz: The size of the blocks in bytes to swap
 *
 * The length of the blob is assumed to be a multiple of block_sz
 */
static int
ulp_mapper_blob_block_swap(struct bnxt_ulp_context *ulp_ctx, struct ulp_blob *blob, u32 block_sz)
{
	u16 num_words, data_sz;
	int i, rc = 0;
	u8 *pdata;
	u8 *data; /* size of a block for temp storage */

	/* Shouldn't happen since it is internal function, but check anyway */
	if (!blob || !block_sz) {
		netdev_dbg(ulp_ctx->bp->dev, "Invalid arguments\n");
		return -EINVAL;
	}

	data = vzalloc(block_sz);
	if (!data)
		return -ENOMEM;

	pdata = ulp_blob_data_get(blob, &data_sz);
	data_sz = ULP_BITS_2_BYTE(data_sz);
	if (!data_sz || (data_sz % block_sz) != 0) {
		netdev_dbg(ulp_ctx->bp->dev, "length(%d) not a multiple of %d\n",
			   data_sz, block_sz);
		rc = -EINVAL;
		goto err;
	}

	num_words = data_sz / block_sz;
	for (i = 0; i < num_words / 2; i++) {
		memcpy(data, &pdata[i * block_sz], block_sz);
		memcpy(&pdata[i * block_sz],
		       &pdata[(num_words - 1 - i) * block_sz], block_sz);
		memcpy(&pdata[(num_words - 1 - i) * block_sz],
		       data, block_sz);
	}
err:
	vfree(data);
	return rc;
}

static int
ulp_mapper_tfc_mpc_batch_end(struct bnxt *bp,
			     void *tfcp,
			     struct tfc_mpc_batch_info_t *batch_info)
{
	int rc;
	int i;

	rc = tfc_mpc_batch_end((void *)bp, (struct tfc *)tfcp, batch_info);
	if (unlikely(rc))
		return rc;

	for (i = 0; i < batch_info->count; i++) {
		if (!batch_info->result[i])
			continue;

		switch (batch_info->comp_info[i].type) {
		case TFC_MPC_EM_INSERT:
			batch_info->em_error = batch_info->result[i];
			break;
		default:
			if (batch_info->result[i] && !batch_info->error)
				batch_info->error = batch_info->result[i];
			break;
		}
	}

	return rc;
}

static bool
ulp_mapper_tfc_mpc_batch_started(struct tfc_mpc_batch_info_t *batch_info)
{
	return tfc_mpc_batch_started(batch_info);
}

static int
ulp_mapper_tfc_em_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			      struct bnxt_ulp_mapper_tbl_info *tbl,
			      void *error)
{
	struct bnxt_ulp_device_params *dparms = parms->device_params;
	struct ulp_flow_db_res_params fid_parms = { 0 };
	struct tfc_em_delete_parms free_parms = { 0 };
	struct tfc_em_insert_parms iparms = { 0 };
	struct bnxt_ulp_mapper_key_info	*kflds;
	u16 tmplen, key_len, align_len_bits;
	enum bnxt_ulp_byte_order byte_order;
	struct ulp_blob key, data;
	struct tfc *tfcp = NULL;
	int rc = 0, trc = 0;
	u32 i, num_kflds;
	u64 handle = 0;
	u8 tsid = 0;

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	if (tbl->key_recipe_opcode == BNXT_ULP_KEY_RECIPE_OPC_DYN_KEY)
		kflds = ulp_mapper_key_recipe_fields_get(parms, tbl, &num_kflds);
	else
		kflds = ulp_mapper_key_fields_get(parms, tbl, &num_kflds);
	if (!kflds || !num_kflds) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to get key fields\n");
		return -EINVAL;
	}

	byte_order = dparms->em_byte_order;
	/* Initialize the key/result blobs */
	if (ulp_blob_init(&key, tbl->blob_key_bit_size, byte_order) ||
	    ulp_blob_init(&data, tbl->result_bit_size, byte_order)) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "blob inits failed.\n");
		return -EINVAL;
	}

	/* create the key */
	for (i = 0; i < num_kflds; i++) {
		/* Setup the key */
		rc = ulp_mapper_field_opc_process(parms, tbl->direction,
						  &kflds[i].field_info_spec,
						  &key, 1, "EM Key");
		if (rc) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "Key field set failed.\n");
			return rc;
		}
	}
	/* add padding to make sure key is at record boundary */
	key_len = ulp_blob_data_len_get(&key);
	if (key_len > dparms->em_blk_align_bits) {
		key_len = key_len - dparms->em_blk_align_bits;
		align_len_bits = dparms->em_blk_size_bits -
			(key_len % dparms->em_blk_size_bits);
	} else {
		align_len_bits = dparms->em_blk_align_bits - key_len;
	}

	ulp_blob_pad_push(&key, align_len_bits);
	key_len = ULP_BITS_2_BYTE(ulp_blob_data_len_get(&key));
	ulp_blob_perform_byte_reverse(&key, key_len);

	ulp_mapper_result_dump(parms->ulp_ctx, "EM Key", tbl, &key);

	/* Create the result data blob */
	rc = ulp_mapper_tbl_result_build(parms, tbl, &data, "EM Result");
	if (rc) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to build the result blob\n");
		return rc;
	}
	ulp_blob_pad_align(&data, dparms->em_blk_align_bits);
	key_len = ULP_BITS_2_BYTE(ulp_blob_data_len_get(&data));
	ulp_blob_perform_byte_reverse(&data, key_len);

	ulp_mapper_result_dump(parms->ulp_ctx, "EM Result", tbl, &data);

	/* merge the result into the key blob */
	rc = ulp_blob_append(&key, &data, 0, dparms->em_blk_align_bits);
	if (rc) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "EM Failed to append the result to key(%d)",
			   rc);
		return rc;
	}
	/* TBD: Need to come up with a more generic way to know when to swap,
	 * this is fine for now as this driver only supports this device.
	 */
	rc = ulp_mapper_blob_block_swap(parms->ulp_ctx, &key,
					ULP_BITS_2_BYTE(dparms->em_blk_size_bits));
	/* Error printed within function, just return on error */
	if (rc)
		return rc;

	ulp_mapper_result_dump(parms->ulp_ctx, "EM Merged Result", tbl, &key);

	iparms.dir		 = tbl->direction;
	iparms.lkup_key_data	 = ulp_blob_data_get(&key, &tmplen);
	iparms.lkup_key_sz_words = ULP_BITS_TO_32_BYTE_WORD(tmplen);
	iparms.key_data		 = NULL;
	iparms.key_sz_bits	 = 0;
	iparms.flow_handle	 = &handle;
	iparms.batch_info	 = parms->batch_info;

	rc = bnxt_ulp_cntxt_tsid_get(parms->ulp_ctx, &tsid);
	if (rc) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to get the table scope\n");
		return rc;
	}
	rc = tfc_em_insert(tfcp, tsid, &iparms);

	if (tfc_mpc_batch_started(parms->batch_info)) {
		int em_index = parms->batch_info->count - 1;
		int trc;

		parms->batch_info->em_hdl[em_index] = *iparms.flow_handle;

		trc = ulp_mapper_tfc_mpc_batch_end(parms->ulp_ctx->bp,
						   (void *)tfcp, parms->batch_info);
		if (unlikely(trc))
			return trc;

		*iparms.flow_handle = parms->batch_info->em_hdl[em_index];

		/* Has there been an error? */
		if (parms->batch_info->error) {
			/* If there's not an EM error the entry will need to
			 * be deleted
			 */
			if (!parms->batch_info->em_error) {
				rc = parms->batch_info->error;
				goto error;
			}
		}

		rc = parms->batch_info->em_error;
	}

	if (rc) {
		/* Set the error flag in reg file */
		if (tbl->tbl_opcode == BNXT_ULP_EM_TBL_OPC_WR_REGFILE) {
			uint64_t val = 0;
			int tmp_rc = 0;

			/* hash collision */
			if (rc == -E2BIG)
				netdev_dbg(parms->ulp_ctx->bp->dev, "Dulicate EM entry\n");

			/* over max flows */
			if (rc == -ENOMEM) {
				val = 1;
				rc = 0;
				netdev_dbg(parms->ulp_ctx->bp->dev,
					   "Fail to insert EM, shall add to wc\n");
			}
			tmp_rc = ulp_regfile_write(parms->regfile, tbl->tbl_operand,
						   cpu_to_be64(val));
			if (!tmp_rc)
				netdev_dbg(parms->ulp_ctx->bp->dev, "regwrite failed\n");
		}
		if (rc && rc != -E2BIG)
			netdev_err(parms->ulp_ctx->bp->dev,
				   "Failed to insert em entry rc=%d.\n", rc);
		return rc;
	}

	ulp_mapper_tfc_em_dump(parms->ulp_ctx, "EM", &key, &iparms);

	/* Mark action process */
	rc = ulp_mapper_mark_gfid_process(parms, tbl, *iparms.flow_handle);
	if (rc) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to add mark to flow\n");
		goto error;
	}

	/* Link the EM resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction = tbl->direction;
	fid_parms.resource_func = tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.critical_resource = tbl->critical_resource;
	fid_parms.resource_hndl = *iparms.flow_handle;

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (rc) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Fail to link res to flow rc = %d\n", rc);
		/* Need to free the identifier, so goto error */
		goto error;
	}

	return 0;
error:
	free_parms.dir = iparms.dir;
	free_parms.flow_handle = *iparms.flow_handle;

	trc = tfc_em_delete(tfcp, &free_parms);
	if (trc)
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to delete EM entry on failed add\n");

	return rc;
}

static int
ulp_mapper_tfc_em_entry_free(struct bnxt_ulp_context *ulp,
			     struct ulp_flow_db_res_params *res,
			     void *error)
{
	struct tfc_em_delete_parms free_parms = { 0 };
	struct tfc *tfcp = NULL;
	u16 fw_fid = 0;
	int rc = 0;

	if (bnxt_ulp_cntxt_fid_get(ulp, &fw_fid)) {
		netdev_dbg(ulp->bp->dev, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(ulp->bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	free_parms.dir = (enum cfa_dir)res->direction;
	free_parms.flow_handle = res->resource_hndl;

	rc = tfc_em_delete(tfcp, &free_parms);
	if (rc) {
		netdev_dbg(ulp->bp->dev, "Failed to delete EM entry, res_hndl = %llx\n",
			   res->resource_hndl);
	} else {
		netdev_dbg(ulp->bp->dev, "Deleted EM entry, res = %llu\n",
			   res->resource_hndl);
	}

	return rc;
}

static bool ulp_mapper_tfc_use_tbl_result_size(struct bnxt_ulp_mapper_tbl_info *tbl)
{
	return (tbl->resource_func == BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE &&
		tbl->resource_type == CFA_RSUBTYPE_IDX_TBL_MIRROR);
}

static u16
ulp_mapper_tfc_dyn_blob_size_get(struct bnxt_ulp_mapper_parms *mparms,
				 struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct bnxt_ulp_device_params *d_params = mparms->device_params;
	enum bnxt_ulp_resource_type rtype = tbl->resource_type;

	if (ulp_mapper_tfc_use_tbl_result_size(tbl)) {
		return tbl->result_bit_size;
	} else if (d_params->dynamic_sram_en) {
		switch (rtype) {
		/* TBD: add more types here */
		case BNXT_ULP_RESOURCE_TYPE_STAT:
		case BNXT_ULP_RESOURCE_TYPE_ENCAP:
		case BNXT_ULP_RESOURCE_TYPE_MODIFY:
			/* return max size */
			return BNXT_ULP_FLMP_BLOB_SIZE_IN_BITS;
		default:
			break;
		}
	} else if (tbl->encap_num_fields) {
		return BNXT_ULP_FLMP_BLOB_SIZE_IN_BITS;
	}
	return tbl->result_bit_size;
}

static int
ulp_mapper_tfc_index_tbl_process(struct bnxt_ulp_mapper_parms *parms,
				 struct bnxt_ulp_mapper_tbl_info *tbl)
{
	bool alloc = false, write = false, global = false, regfile = false;
	struct bnxt_ulp_glb_resource_info glb_res = { 0 };
	struct ulp_flow_db_res_params fid_parms;
	enum cfa_track_type tt = tbl->track_type;
	struct tfc_idx_tbl_info tbl_info = { 0 };
	u16 bit_size, wordlen = 0, tmplen = 0;
	struct bnxt *bp = parms->ulp_ctx->bp;
	struct tfc *tfcp = NULL;
	unsigned char *data_p;
	struct ulp_blob	data;
	bool shared = false;
	u64 regval = 0;
	u16 fw_fid = 0;
	u32 index = 0;
	int rc = 0;

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	if (bnxt_ulp_cntxt_fid_get(parms->ulp_ctx, &fw_fid)) {
		netdev_dbg(bp->dev, "Failed to get func id\n");
		return -EINVAL;
	}

	/* compute the blob size */
	bit_size = ulp_mapper_tfc_dyn_blob_size_get(parms, tbl);

	/* Initialize the blob data */
	if (ulp_blob_init(&data, bit_size,
			  parms->device_params->result_byte_order)) {
		netdev_dbg(bp->dev, "Failed to initialize index table blob\n");
		return -EINVAL;
	}

	switch (tbl->tbl_opcode) {
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE:
		alloc = true;
		regfile = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE:
		/* Build the entry, alloc an index, write the table, and store
		 * the data in the regfile.
		 */
		alloc = true;
		write = true;
		regfile = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE:
		/* get the index to write to from the regfile and then write
		 * the table entry.
		 */
		regfile = true;
		write = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_GLB_REGFILE:
		/* Build the entry, alloc an index, write the table, and store
		 * the data in the global regfile.
		 */
		alloc = true;
		global = true;
		write = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE:
		if (tbl->fdb_opcode != BNXT_ULP_FDB_OPC_NOP) {
			netdev_dbg(bp->dev, "Template error, wrong fdb opcode\n");
			return -EINVAL;
		}
		/* get the index to write to from the global regfile and then
		 * write the table.
		 */
		if (ulp_mapper_glb_resource_read(parms->mapper_data,
						 tbl->direction,
						 tbl->tbl_operand,
						 &regval, &shared)) {
			netdev_dbg(bp->dev, "Failed to get tbl idx from Glb RF[%d].\n",
				   tbl->tbl_operand);
			return -EINVAL;
		}
		index = be64_to_cpu(regval);
		/* check to see if any scope id changes needs to be done*/
		write = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_RD_REGFILE:
		/* The read is different from the rest and can be handled here
		 * instead of trying to use common code.  Simply read the table
		 * with the index from the regfile, scan and store the
		 * identifiers, and return.
		 */
		if (ulp_regfile_read(parms->regfile,
				     tbl->tbl_operand, &regval)) {
			netdev_dbg(bp->dev, "Failed to get tbl idx from regfile[%d]\n",
				   tbl->tbl_operand);
			return -EINVAL;
		}
		index = be64_to_cpu(regval);
		tbl_info.dir = tbl->direction;
		tbl_info.rsubtype = tbl->resource_type;
		tbl_info.blktype = CFA_RESTYPE_TO_BLKT(tbl->resource_type);
		tbl_info.id = index;
		/* Nothing has been pushed to blob, so push bit_size */
		ulp_blob_pad_push(&data, bit_size);
		data_p = ulp_blob_data_get(&data, &tmplen);
		wordlen = ULP_BITS_2_BYTE(tmplen);

		rc = tfc_idx_tbl_get(tfcp, fw_fid, &tbl_info, (u32 *)data_p,
				     (u8 *)&wordlen);
		if (rc) {
			netdev_dbg(bp->dev, "Failed to read the tbl entry %d:%d\n",
				   tbl->resource_type, index);
			return rc;
		}

		/* Scan the fields in the entry and push them into the regfile*/
		rc = ulp_mapper_tbl_ident_scan_ext(parms, tbl, data_p,
						   wordlen, data.byte_order);
		if (rc) {
			netdev_dbg(bp->dev, "Failed to get flds on tbl read rc=%d\n", rc);
			return rc;
		}
		return 0;
	case BNXT_ULP_INDEX_TBL_OPC_NOP_REGFILE:
		/* Special case, where hw table processing is not being done */
		/* but only for writing the regfile into the flow database */
		regfile = true;
		break;
	default:
		netdev_dbg(bp->dev, "Invalid index table opcode %d\n", tbl->tbl_opcode);
		return -EINVAL;
	}

	/* read the CMM identifier from the regfile, it is not allocated */
	if (!alloc && regfile) {
		if (ulp_regfile_read(parms->regfile,
				     tbl->tbl_operand,
				     &regval)) {
			netdev_dbg(bp->dev, "Failed to get tbl idx from regfile[%d].\n",
				   tbl->tbl_operand);
			return -EINVAL;
		}
		index = be64_to_cpu(regval);
	}

	/* Allocate the Action CMM identifier */
	if (alloc) {
		tbl_info.dir = tbl->direction;
		tbl_info.rsubtype = tbl->resource_type;
		tbl_info.blktype = CFA_RESTYPE_TO_BLKT(tbl->resource_type);
		/*
		 * Read back the operand and pass it into the
		 * alloc command if its a Dyn UPAR table.
		 */
		if (tbl_info.blktype == CFA_IDX_TBL_BLKTYPE_RXP) {
			if (ulp_regfile_read(parms->regfile,
					     tbl->tbl_operand, &regval)) {
				netdev_dbg(bp->dev,
					   "Failed to get tbl idx from regfile[%d]\n",
					   tbl->tbl_operand);
				return -EINVAL;
			}
			tbl_info.rsubtype = be64_to_cpu(regval);
		}
		rc =  tfc_idx_tbl_alloc(tfcp, fw_fid, tt, &tbl_info);
		if (rc) {
			netdev_dbg(bp->dev, "Alloc table[%s][%s] failed rc=%d\n",
				   tfc_idx_tbl_2_str(tbl_info.rsubtype),
				   tfc_dir_2_str(tbl->direction), rc);
			return rc;
		}
		index = tbl_info.id;
	}

	/* update the global register value */
	if (alloc && global) {
		glb_res.direction = tbl->direction;
		glb_res.resource_func = tbl->resource_func;
		glb_res.resource_type = tbl->resource_type;
		glb_res.glb_regfile_index = tbl->tbl_operand;
		regval = cpu_to_be64(index);

		/* Shared resources are never allocated through this
		 * method, so the shared flag is always false.
		 */
		rc = ulp_mapper_glb_resource_write(parms->mapper_data,
						   &glb_res, regval,
						   false);
		if (rc) {
			netdev_dbg(bp->dev, "Failed to write %s regfile[%d] rc=%d\n",
				   (global) ? "global" : "reg",
				   tbl->tbl_operand, rc);
			goto error;
		}
	}

	/* update the local register value */
	if (alloc && regfile) {
		regval = cpu_to_be64(index);
		rc = ulp_regfile_write(parms->regfile,
				       tbl->tbl_operand, regval);
		if (rc) {
			netdev_dbg(bp->dev, "Failed to write %s regfile[%d] rc=%d\n",
				   (global) ? "global" : "reg",
				   tbl->tbl_operand, rc);
			goto error;
		}
	}

	if (write) {
		/* Get the result fields list */
		rc = ulp_mapper_tbl_result_build(parms,
						 tbl,
						 &data,
						 "Indexed Result");
		if (rc) {
			netdev_dbg(bp->dev, "Failed to build the result blob\n");
			return rc;
		}
		data_p = ulp_blob_data_get(&data, &tmplen);
		tbl_info.dir = tbl->direction;
		tbl_info.rsubtype = tbl->resource_type;
		tbl_info.blktype = CFA_RESTYPE_TO_BLKT(tbl->resource_type);
		tbl_info.id = index;
		wordlen = ULP_BITS_2_BYTE(tmplen);
		rc = tfc_idx_tbl_set(tfcp, fw_fid, &tbl_info,
				     (u32 *)data_p, wordlen);
		if (rc) {
			netdev_dbg(bp->dev, "Index table[%s][%s][%x] write fail %d\n",
				   tfc_idx_tbl_2_str(tbl_info.rsubtype),
				   tfc_dir_2_str(tbl_info.dir),
				   tbl_info.id, rc);
			goto error;
		}
		netdev_dbg(bp->dev, "Index table[%s][%d][%x] write successful\n",
			   tfc_idx_tbl_2_str(tbl_info.rsubtype),
			   tbl_info.dir, tbl_info.id);
	}
	/* Link the resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction	= tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.resource_sub_type = tbl->resource_sub_type;
	fid_parms.resource_hndl	= index;
	fid_parms.critical_resource = tbl->critical_resource;
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to link resource to flow rc = %d\n", rc);
		goto error;
	}

	/* Perform the VF rep action */
	rc = ulp_mapper_mark_vfr_idx_process(parms, tbl);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to add vfr mark rc = %d\n", rc);
		goto error;
	}
	return rc;
error:
	/* Shared resources are not freed */
	if (shared)
		return rc;
	/* Free the allocated resource since we failed to either
	 * write to the entry or link the flow
	 */

	tbl_info.blktype = CFA_RESTYPE_TO_BLKT(tbl->resource_type);
	if (tfc_idx_tbl_free(tfcp, fw_fid, &tbl_info))
		netdev_dbg(bp->dev, "Failed to free index entry on failure\n");
	return rc;
}

static inline int
ulp_mapper_tfc_index_entry_free(struct bnxt_ulp_context *ulp_ctx,
				struct ulp_flow_db_res_params *res)
{
	struct tfc_idx_tbl_info tbl_info = { 0 };
	struct bnxt *bp = ulp_ctx->bp;
	struct tfc *tfcp = NULL;
	u16 fw_fid = 0;
	int rc;

	if (bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid)) {
		netdev_dbg(bp->dev, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	tbl_info.dir = (enum cfa_dir)res->direction;
	tbl_info.rsubtype = res->resource_type;
	tbl_info.blktype = CFA_RESTYPE_TO_BLKT(res->resource_type);
	tbl_info.id = (u16)res->resource_hndl;

	/* TBD: check to see if the memory needs to be cleaned as well*/
	rc = tfc_idx_tbl_free(tfcp, fw_fid, &tbl_info);
	if (!rc)
		netdev_dbg(bp->dev, "Freed Index [%d]:[%d] = 0x%X\n",
			   tbl_info.dir, tbl_info.rsubtype, tbl_info.id);

	return rc;
}

static int
ulp_mapper_tfc_cmm_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			       struct bnxt_ulp_mapper_tbl_info *tbl,
			       void *error)
{
	bool alloc = false, write = false, global = false, regfile = false;
	u16 bit_size, wordlen = 0, act_wordlen = 0, tmplen = 0;
	struct bnxt_ulp_glb_resource_info glb_res = { 0 };
	struct ulp_flow_db_res_params fid_parms;
	struct bnxt *bp = parms->ulp_ctx->bp;
	struct tfc_cmm_info cmm_info = { 0 };
	struct tfc_cmm_clr cmm_clr = {0 };
	bool shared = false, read = false;
	struct tfc *tfcp = NULL;
	unsigned char *data_p;
	struct ulp_blob	data;
	u64 act_rec_size = 0;
	const u8 *act_data;
	dma_addr_t pa_addr;
	u64 regval = 0;
	u64 handle = 0;
	void *va_addr;
	u8 tsid = 0;
	int rc = 0;

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	/* compute the blob size */
	bit_size = ulp_mapper_tfc_dyn_blob_size_get(parms, tbl);

	/* Initialize the blob data */
	if (ulp_blob_init(&data, bit_size,
			  parms->device_params->result_byte_order)) {
		netdev_dbg(bp->dev, "Failed to initialize cmm table blob\n");
		return -EINVAL;
	}

	switch (tbl->tbl_opcode) {
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE:
		regfile = true;
		alloc = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE:
		/* Build the entry, alloc an index, write the table, and store
		 * the data in the regfile.
		 */
		alloc = true;
		write = true;
		regfile = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE:
		/* get the index to write to from the regfile and then write
		 * the table entry.
		 */
		regfile = true;
		write = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_GLB_REGFILE:
		/* Build the entry, alloc an index, write the table, and store
		 * the data in the global regfile.
		 */
		alloc = true;
		global = true;
		write = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE:
		if (tbl->fdb_opcode != BNXT_ULP_FDB_OPC_NOP) {
			netdev_dbg(bp->dev, "Template error, wrong fdb opcode\n");
			return -EINVAL;
		}
		/* get the index to write to from the global regfile and then
		 * write the table.
		 */
		if (ulp_mapper_glb_resource_read(parms->mapper_data,
						 tbl->direction,
						 tbl->tbl_operand,
						 &regval, &shared)) {
			netdev_dbg(bp->dev, "Failed to get tbl idx from Glb RF[%d].\n",
				   tbl->tbl_operand);
			return -EINVAL;
		}
		handle = be64_to_cpu(regval);
		/* check to see if any scope id changes needs to be done*/
		write = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_RD_REGFILE:
		if (ulp_regfile_read(parms->regfile,
				     tbl->tbl_operand, &regval)) {
			netdev_dbg(bp->dev, "Failed to get tbl idx from regfile[%d]\n",
				   tbl->tbl_operand);
			return -EINVAL;
		}
		handle = be64_to_cpu(regval);
		read = true;
		break;
	case BNXT_ULP_INDEX_TBL_OPC_NOP_REGFILE:
		regfile = true;
		alloc = false;
		break;
	default:
		netdev_dbg(bp->dev, "Invalid cmm table opcode %d\n", tbl->tbl_opcode);
		return -EINVAL;
	}

	if (read) {
		/* Read the table with the index from the regfile, scan and
		 * store the identifiers, and return.
		 */
		va_addr = dma_alloc_coherent(&bp->pdev->dev,
					     ULP_BITS_2_BYTE(bit_size),
					     &pa_addr, GFP_KERNEL);
		if (!va_addr)
			return -EINVAL;

		cmm_info.dir = tbl->direction;
		cmm_info.rsubtype = tbl->resource_type;
		cmm_info.act_handle = handle;

		/* Nothing has been pushed to blob, so push bit_size */
		ulp_blob_pad_push(&data, bit_size);
		data_p = ulp_blob_data_get(&data, &tmplen);
		act_wordlen = ULP_BITS_TO_32_BYTE_WORD(tmplen);
		wordlen = ULP_BITS_2_BYTE(tmplen);

		rc = tfc_act_get(tfcp,
				 NULL,
				 &cmm_info,
				 &cmm_clr,
				 pa_addr,
				 &act_wordlen);
		if (rc) {
			netdev_dbg(bp->dev, "CMM table[%d][%s][%llu] read fail %d\n",
				   cmm_info.rsubtype,
				   tfc_dir_2_str(cmm_info.dir),
				   handle, rc);
		} else {
			/* Scan the fields in the entry and push them into the regfile*/
			rc = ulp_mapper_tbl_ident_scan_ext(parms, tbl, (u8 *)va_addr,
							   wordlen, data.byte_order);
			if (rc)
				netdev_dbg(bp->dev, "Failed to get flds on tbl read rc=%d\n", rc);
		}

		dma_free_coherent(&bp->pdev->dev, ULP_BITS_2_BYTE(bit_size),
				  va_addr, pa_addr);

		return rc;
	}

	/* read the CMM handle from the regfile, it is not allocated */
	if (!alloc && regfile) {
		if (ulp_regfile_read(parms->regfile,
				     tbl->tbl_operand,
				     &regval)) {
			netdev_dbg(bp->dev, "Failed to get tbl idx from regfile[%d].\n",
				   tbl->tbl_operand);
			return -EINVAL;
		}
		handle = be64_to_cpu(regval);
	}

	/* Get the result fields list */
	rc = ulp_mapper_tbl_result_build(parms,
					 tbl,
					 &data,
					 "Indexed Result");
	if (rc) {
		netdev_dbg(bp->dev, "Failed to build the result blob\n");
		return rc;
	}

	/* Allocate the Action CMM identifier */
	if (alloc) {
		cmm_info.dir = tbl->direction;
		cmm_info.rsubtype = tbl->resource_type;
		/* Only need the length for alloc, ignore the returned data */
		act_data = ulp_blob_data_get(&data, &tmplen);
		act_wordlen = ULP_BITS_TO_32_BYTE_WORD(tmplen);

		rc = bnxt_ulp_cntxt_tsid_get(parms->ulp_ctx, &tsid);
		if (rc) {
			netdev_dbg(bp->dev, "Failed to get the table scope\n");
			return rc;
		}
		/* All failures after the alloc succeeds require a free */
		rc =  tfc_act_alloc(tfcp, tsid, &cmm_info, act_wordlen);
		if (rc) {
			netdev_dbg(bp->dev, "Alloc CMM [%d][%s] failed rc=%d\n",
				   cmm_info.rsubtype, tfc_dir_2_str(cmm_info.dir), rc);
			return rc;
		}
		handle = cmm_info.act_handle;

		/* Counters need to be reset when allocated to ensure counter is
		 * zero
		 */
		if (tbl->resource_func == BNXT_ULP_RESOURCE_FUNC_CMM_STAT) {
			rc = tfc_act_set(tfcp,
					 &cmm_info,
					 act_data,
					 act_wordlen,
					 parms->batch_info);
			if (rc) {
				netdev_dbg(bp->dev, "Stat alloc/clear[%d][%s][%llu] failed rc=%d\n",
					   cmm_info.rsubtype,
					   tfc_dir_2_str(cmm_info.dir),
					   cmm_info.act_handle, rc);
				goto error;
			}
		}
	}

	/* update the global register value */
	if (alloc && global) {
		glb_res.direction = tbl->direction;
		glb_res.resource_func = tbl->resource_func;
		glb_res.resource_type = tbl->resource_type;
		glb_res.glb_regfile_index = tbl->tbl_operand;
		regval = cpu_to_be64(handle);

		/* Shared resources are never allocated through this
		 * method, so the shared flag is always false.
		 */
		rc = ulp_mapper_glb_resource_write(parms->mapper_data,
						   &glb_res, regval,
						   false);
		if (rc) {
			netdev_dbg(bp->dev, "Failed to write %s regfile[%d] rc=%d\n",
				   (global) ? "global" : "reg",
				   tbl->tbl_operand, rc);
			goto error;
		}
	}

	/* update the local register value */
	if (alloc && regfile) {
		regval = cpu_to_be64(handle);
		rc = ulp_regfile_write(parms->regfile,
				       tbl->tbl_operand, regval);
		if (rc) {
			netdev_dbg(bp->dev, "Failed to write %s regfile[%d] rc=%d\n",
				   (global) ? "global" : "reg",
				   tbl->tbl_operand, rc);
			goto error;
		}
	}

	if (write) {
		act_data = ulp_blob_data_get(&data, &tmplen);
		cmm_info.dir = tbl->direction;
		cmm_info.rsubtype = tbl->resource_type;
		cmm_info.act_handle = handle;
		act_wordlen = ULP_BITS_TO_32_BYTE_WORD(tmplen);
		rc = tfc_act_set(tfcp,
				 &cmm_info,
				 act_data,
				 act_wordlen,
				 parms->batch_info);
		if (rc) {
			netdev_dbg(bp->dev, "CMM table[%d][%s][%llu] write fail %d\n",
				   cmm_info.rsubtype,
				   tfc_dir_2_str(cmm_info.dir),
				   handle, rc);
			goto error;
		}
		netdev_dbg(bp->dev, "CMM table[%d][%s][0x%016llx] write successful\n",
			   cmm_info.rsubtype, tfc_dir_2_str(cmm_info.dir), handle);

		/* Calculate action record size */
		if (tbl->resource_type == CFA_RSUBTYPE_CMM_ACT) {
			act_rec_size = (ULP_BITS_2_BYTE_NR(tmplen) + 15) / 16;
			act_rec_size--;
			if (ulp_regfile_write(parms->regfile,
					      BNXT_ULP_RF_IDX_ACTION_REC_SIZE,
					      cpu_to_be64(act_rec_size)))
				netdev_dbg(bp->dev, "Failed write the act rec size\n");
		}
	}
	/* Link the resource to the flow in the flow db */
	memset(&fid_parms, 0, sizeof(fid_parms));
	fid_parms.direction	= tbl->direction;
	fid_parms.resource_func	= tbl->resource_func;
	fid_parms.resource_type	= tbl->resource_type;
	fid_parms.resource_sub_type = tbl->resource_sub_type;
	fid_parms.resource_hndl	= handle;
	fid_parms.critical_resource = tbl->critical_resource;
	ulp_flow_db_shared_session_set(&fid_parms, tbl->session_type);

	rc = ulp_mapper_fdb_opc_process(parms, tbl, &fid_parms);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to link resource to flow rc = %d\n", rc);
		goto error;
	}

	/* Perform the VF rep action */
	rc = ulp_mapper_mark_vfr_idx_process(parms, tbl);
	if (rc) {
		netdev_dbg(bp->dev, "Failed to add vfr mark rc = %d\n", rc);
		goto error;
	}
	return rc;
error:
	/* Shared resources are not freed */
	if (shared)
		return rc;
	/* Free the allocated resource since we failed to either
	 * write to the entry or link the flow
	 */

	if (tfc_act_free(tfcp, &cmm_info))
		netdev_dbg(bp->dev, "Failed to free cmm entry on failure\n");

	return rc;
}

static int
ulp_mapper_tfc_cmm_entry_free(struct bnxt_ulp_context *ulp_ctx,
			      struct ulp_flow_db_res_params *res,
			      void *error)
{
	struct tfc_cmm_info cmm_info = { 0 };
	struct tfc *tfcp = NULL;
	u16 fw_fid = 0;
	int rc = 0;

	/* skip cmm processing if reserve flag is enabled */
	if (res->reserve_flag)
		return 0;

	if (bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid)) {
		netdev_dbg(ulp_ctx->bp->dev, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(ulp_ctx->bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	cmm_info.dir = (enum cfa_dir)res->direction;
	cmm_info.rsubtype = res->resource_type;
	cmm_info.act_handle = res->resource_hndl;

	/* TBD: check to see if the memory needs to be cleaned as well */
	rc = tfc_act_free(tfcp, &cmm_info);
	if (rc) {
		netdev_dbg(ulp_ctx->bp->dev, "Failed to delete CMM entry,res = 0x%llX\n",
			   res->resource_hndl);
	} else {
		netdev_dbg(ulp_ctx->bp->dev, "Deleted CMM entry,res = %llX\n", res->resource_hndl);
	}
	return rc;
}

static int
ulp_mapper_tfc_if_tbl_process(struct bnxt_ulp_mapper_parms *parms,
			      struct bnxt_ulp_mapper_tbl_info *tbl)
{
	enum bnxt_ulp_if_tbl_opc if_opc = tbl->tbl_opcode;
	struct tfc_if_tbl_info tbl_info = { 0 };
	struct ulp_blob	data, res_blob;
	unsigned char *data_p;
	struct tfc *tfcp;
	u16 fw_fid = 0;
	u8 data_size;
	u32 res_size;
	u16 tmplen;
	int rc = 0;
	u64 idx;

	if (bnxt_ulp_cntxt_fid_get(parms->ulp_ctx, &fw_fid)) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(parms->ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	/* Initialize the blob data */
	if (ulp_blob_init(&data, tbl->result_bit_size,
			  parms->device_params->result_byte_order)) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed initial index table blob\n");
		return -EINVAL;
	}

	/* create the result blob */
	rc = ulp_mapper_tbl_result_build(parms, tbl, &data, "IFtable Result");
	if (rc) {
		netdev_dbg(parms->ulp_ctx->bp->dev, "Failed to build the result blob\n");
		return rc;
	}

	/* Get the index details */
	switch (if_opc) {
	case BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD:
		idx = ULP_COMP_FLD_IDX_RD(parms, tbl->tbl_operand);
		break;
	case BNXT_ULP_IF_TBL_OPC_WR_REGFILE:
		if (ulp_regfile_read(parms->regfile, tbl->tbl_operand, &idx)) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "regfile[%d] read oob\n",
				   tbl->tbl_operand);
			return -EINVAL;
		}
		idx = be64_to_cpu(idx);
		break;
	case BNXT_ULP_IF_TBL_OPC_WR_CONST:
		idx = tbl->tbl_operand;
		break;
	case BNXT_ULP_IF_TBL_OPC_RD_COMP_FIELD:
		/* Initialize the result blob */
		if (ulp_blob_init(&res_blob, tbl->result_bit_size,
				  parms->device_params->result_byte_order)) {
			netdev_dbg(parms->ulp_ctx->bp->dev, "Failed initial result blob\n");
			return -EINVAL;
		}

		/* read the interface table */
		idx = ULP_COMP_FLD_IDX_RD(parms, tbl->tbl_operand);
		res_size = ULP_BITS_2_BYTE(tbl->result_bit_size);
		rc = ulp_mapper_tbl_ident_scan_ext(parms, tbl,
						   res_blob.data,
						   res_size,
						   res_blob.byte_order);
		if (rc)
			netdev_dbg(parms->ulp_ctx->bp->dev, "Scan and extract failed rc=%d\n", rc);
		return rc;
	case BNXT_ULP_IF_TBL_OPC_NOT_USED:
		return rc; /* skip it */
	default:
		netdev_dbg(parms->ulp_ctx->bp->dev, "Invalid tbl index opcode\n");
		return -EINVAL;
	}

	tbl_info.dir = tbl->direction;
	tbl_info.rsubtype = tbl->resource_type;
	tbl_info.id = (uint32_t)idx;
	data_p = ulp_blob_data_get(&data, &tmplen);
	data_size = ULP_BITS_2_BYTE(tmplen);

	rc = tfc_if_tbl_set(tfcp, fw_fid, &tbl_info, (uint8_t *)data_p,
			    data_size);
	if (rc) {
		netdev_dbg(parms->ulp_ctx->bp->dev,
			   "Failed to write the if tbl entry %d:%d\n",
			   tbl->resource_type, (uint32_t)idx);
		return rc;
	}

	return rc;
}

static int
ulp_mapper_tfc_ident_alloc(struct bnxt_ulp_context *ulp_ctx,
			   u32 session_type,
			   u16 ident_type,
			   u8 direction,
			   enum cfa_track_type tt,
			   u64 *identifier_id)
{
	struct tfc_identifier_info ident_info = { 0 };
	struct tfc *tfcp = NULL;
	u16 fw_fid = 0;
	int rc = 0;

	if (bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid)) {
		netdev_dbg(ulp_ctx->bp->dev, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(ulp_ctx->bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	ident_info.dir = direction;
	ident_info.rsubtype = ident_type;

	rc = tfc_identifier_alloc(tfcp, fw_fid, tt, &ident_info);
	if (rc != 0) {
		netdev_dbg(ulp_ctx->bp->dev, "alloc failed %d\n", rc);
		return rc;
	}
	*identifier_id = ident_info.id;
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG
#ifdef RTE_LIBRTE_BNXT_TRUFLOW_DEBUG_MAPPER
	netdev_dbg(ulp_ctx->bp->dev, "Allocated Identifier [%s]:[%s] = 0x%X\n",
		   tfc_dir_2_str(direction),
		   tfc_ident_2_str(ident_info.rsubtype), ident_info.id);
#endif
#endif

	return rc;
}

static int
ulp_mapper_tfc_ident_free(struct bnxt_ulp_context *ulp_ctx,
			  struct ulp_flow_db_res_params *res)
{
	struct tfc_identifier_info ident_info = { 0 };
	struct tfc *tfcp = NULL;
	u16 fw_fid = 0;
	int rc = 0;

	if (bnxt_ulp_cntxt_fid_get(ulp_ctx, &fw_fid)) {
		netdev_dbg(ulp_ctx->bp->dev, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctx, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(ulp_ctx->bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	ident_info.dir = (enum cfa_dir)res->direction;
	ident_info.rsubtype = res->resource_type;
	ident_info.id = res->resource_hndl;

	rc = tfc_identifier_free(tfcp, fw_fid, &ident_info);
	if (rc != 0) {
		netdev_dbg(ulp_ctx->bp->dev, "free failed %d\n", rc);
		return rc;
	}

	netdev_dbg(ulp_ctx->bp->dev, "Freed Identifier [%s]:[%s] = 0x%X\n",
		   tfc_dir_2_str(ident_info.dir),
		   tfc_ident_2_str(ident_info.rsubtype), ident_info.id);

	return rc;
}

static inline int
ulp_mapper_tfc_tcam_entry_free(struct bnxt_ulp_context *ulp,
			       struct ulp_flow_db_res_params *res)
{
	struct tfc_tcam_info tcam_info = { 0 };
	struct tfc *tfcp = NULL;
	u16 fw_fid = 0;

	if (bnxt_ulp_cntxt_fid_get(ulp, &fw_fid)) {
		netdev_dbg(ulp->bp->dev, "Failed to get func_id\n");
		return -EINVAL;
	}

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(ulp->bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}
	tcam_info.dir = (enum cfa_dir)res->direction;
	tcam_info.rsubtype = res->resource_type;
	tcam_info.id = (u16)res->resource_hndl;

	if (!tfcp || tfc_tcam_free(tfcp, fw_fid, &tcam_info)) {
		netdev_dbg(ulp->bp->dev, "Unable to free tcam resource %u\n", tcam_info.id);
		return -EINVAL;
	}

	netdev_dbg(ulp->bp->dev, "Freed TCAM [%d]:[%d] = 0x%X\n",
		   tcam_info.dir, tcam_info.dir, tcam_info.id);
	return 0;
}

static u32
ulp_mapper_tfc_dyn_tbl_type_get(struct bnxt_ulp_mapper_parms *parms,
				struct bnxt_ulp_mapper_tbl_info *tbl,
				u16 blob_len,
				u16 *out_len)
{
	switch (tbl->resource_type) {
	case CFA_RSUBTYPE_CMM_ACT:
		*out_len = ULP_BITS_TO_32_BYTE_WORD(blob_len);
		*out_len = *out_len * 256;
		break;
	default:
		netdev_dbg(parms->ulp_ctx->bp->dev, "Not a dynamic table %d\n", tbl->resource_type);
		*out_len = blob_len;
		break;
	}

	return tbl->resource_type;
}

static int
ulp_mapper_tfc_index_tbl_alloc_process(struct bnxt_ulp_context *ulp,
				       u32 session_type,
				       u16 table_type,
				       u8 direction,
				       u64 *index)
{
	struct tfc_idx_tbl_info tbl_info = { 0 };
	struct tfc *tfcp = NULL;
	u16 fw_fid = 0;
	int rc = 0;

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(ulp->bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	if (bnxt_ulp_cntxt_fid_get(ulp, &fw_fid)) {
		netdev_dbg(ulp->bp->dev, "Failed to get func id\n");
		return -EINVAL;
	}

	tbl_info.rsubtype = table_type;
	tbl_info.blktype = CFA_RESTYPE_TO_BLKT(table_type);
	tbl_info.dir = direction;
	rc = tfc_idx_tbl_alloc(tfcp, fw_fid, CFA_TRACK_TYPE_SID, &tbl_info);
	if (rc) {
		netdev_dbg(ulp->bp->dev, "Alloc table[%s][%s] failed rc=%d\n",
			   tfc_idx_tbl_2_str(tbl_info.rsubtype),
			   tfc_dir_2_str(direction), rc);
		return rc;
	}

	*index = tbl_info.id;

	netdev_dbg(ulp->bp->dev, "Allocated Table Index [%s][%s] = 0x%04x\n",
		   tfc_idx_tbl_2_str(table_type), tfc_dir_2_str(direction),
		   tbl_info.id);

	return rc;
}

static int
ulp_mapper_tfc_app_glb_resource_info_init(struct bnxt_ulp_context
					  *ulp_ctx,
					  struct bnxt_ulp_mapper_data
					  *mapper_data)
{
	/* Not supported Shared Apps yet on TFC API */
	return 0;
}

static int
ulp_mapper_tfc_handle_to_offset(struct bnxt_ulp_mapper_parms *parms,
				u64 handle,
				u32 offset,
				u64 *result)
{
	u32 val = 0;
	int rc = 0;

	TFC_GET_32B_OFFSET_ACT_HANDLE(val, &handle);

	switch (offset) {
	case 0:
		val = val << 5;
		break;
	case 4:
		val = val << 3;
		break;
	case 8:
		val = val << 2;
		break;
	case 16:
		val = val << 1;
		break;
	case 32:
		break;
	default:
		return -EINVAL;
	}

	*result = val;
	return rc;
}

static int
ulp_mapper_tfc_mpc_batch_start(struct tfc_mpc_batch_info_t *batch_info)
{
	return tfc_mpc_batch_start(batch_info);
}

const struct ulp_mapper_core_ops ulp_mapper_tfc_core_ops = {
	.ulp_mapper_core_tcam_tbl_process = ulp_mapper_tfc_tcam_tbl_process,
	.ulp_mapper_core_tcam_entry_free = ulp_mapper_tfc_tcam_entry_free,
	.ulp_mapper_core_em_tbl_process = ulp_mapper_tfc_em_tbl_process,
	.ulp_mapper_core_em_entry_free = ulp_mapper_tfc_em_entry_free,
	.ulp_mapper_core_index_tbl_process = ulp_mapper_tfc_index_tbl_process,
	.ulp_mapper_core_index_entry_free = ulp_mapper_tfc_index_entry_free,
	.ulp_mapper_core_cmm_tbl_process = ulp_mapper_tfc_cmm_tbl_process,
	.ulp_mapper_core_cmm_entry_free = ulp_mapper_tfc_cmm_entry_free,
	.ulp_mapper_core_if_tbl_process = ulp_mapper_tfc_if_tbl_process,
	.ulp_mapper_core_ident_alloc_process = ulp_mapper_tfc_ident_alloc,
	.ulp_mapper_core_ident_free = ulp_mapper_tfc_ident_free,
	.ulp_mapper_core_dyn_tbl_type_get = ulp_mapper_tfc_dyn_tbl_type_get,
	.ulp_mapper_core_index_tbl_alloc_process = ulp_mapper_tfc_index_tbl_alloc_process,
	.ulp_mapper_core_app_glb_res_info_init = ulp_mapper_tfc_app_glb_resource_info_init,
	.ulp_mapper_core_handle_to_offset = ulp_mapper_tfc_handle_to_offset,
	.ulp_mapper_mpc_batch_start = ulp_mapper_tfc_mpc_batch_start,
	.ulp_mapper_mpc_batch_started = ulp_mapper_tfc_mpc_batch_started,
	.ulp_mapper_mpc_batch_end = ulp_mapper_tfc_mpc_batch_end
};
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
