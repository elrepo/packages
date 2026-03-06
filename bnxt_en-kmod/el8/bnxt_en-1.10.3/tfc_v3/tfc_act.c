// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include <linux/netdevice.h>

#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_mpc.h"
#include "bnxt_tfc.h"

#include "tfc.h"
#include "cfa_bld_mpc_field_ids.h"
#include "cfa_bld_mpcops.h"
#include "tfo.h"
#include "tfc_em.h"
#include "tfc_cpm.h"
#include "tfc_msg.h"
#include "tfc_priv.h"
#include "cfa_types.h"
#include "cfa_mm.h"
#include "tfc_action_handle.h"
#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_mpc.h"
#include "bnxt_tfc.h"
#include "sys_util.h"
#include "tfc_util.h"

/* The read/write  granularity is 32B
 */
#define TFC_ACT_RW_GRANULARITY 32

#define TFC_ACT_CACHE_OPT_EN 0

int tfc_act_alloc(struct tfc *tfcp, u8 tsid, struct tfc_cmm_info *cmm_info, u16 num_contig_rec)
{
	struct cfa_mm_alloc_parms aparms;
	struct tfc_cpm *cpm_act = NULL;
	enum cfa_scope_type scope_type;
	struct tfc_ts_mem_cfg mem_cfg;
	struct bnxt *bp = tfcp->bp;
	struct tfc_cmm *cmm;
	u16 max_contig_rec;
	bool is_bs_owner;
	u32 entry_offset;
	u8 pool_sz_exp;
	u16 max_pools;
	u16 pool_id;
	bool valid;
	int rc;

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, &max_pools);
	if (rc) {
		netdev_dbg(bp->dev, "%s: failed to get tsid: %d\n", __func__, rc);
		return -EINVAL;
	}

	if (!valid) {
		netdev_dbg(bp->dev, "%s: tsid(%d) not allocated\n", __func__, tsid);
		return -EINVAL;
	}

	if (!max_pools) {
		netdev_dbg(bp->dev, "%s: tsid(%d) Max pools must be greater than 0 %d\n",
			   __func__, tsid, max_pools);
		return -EINVAL;
	}

	rc = tfo_ts_get_pool_info(tfcp->tfo, tsid, cmm_info->dir,
				  CFA_REGION_TYPE_ACT, &max_contig_rec, &pool_sz_exp);
	if (rc) {
		netdev_dbg(bp->dev,
			   "%s: Failed to get pool info for tsid:%d\n",
			   __func__, tsid);
		return -EINVAL;
	}

	/* Get CPM instance */
	rc = tfo_ts_get_cpm_inst(tfcp->tfo, tsid, bp->pf.fw_fid, cmm_info->dir,
				 CFA_REGION_TYPE_ACT, &cpm_act);
	if (rc) {
		netdev_dbg(bp->dev, "%s: failed to get CPM instance: %d\n",
			   __func__, rc);
		return -EINVAL;
	}

	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid,
				cmm_info->dir,
				CFA_REGION_TYPE_ACT,
				&is_bs_owner,
				&mem_cfg);
	if (rc) {
		netdev_dbg(bp->dev, "%s: tfo_ts_get_mem_cfg() failed: %d\n",
			   __func__, rc);
		return -EINVAL;
	}

	/* if no pool available locally or all pools full */
	rc = tfc_cpm_get_avail_pool(cpm_act, &pool_id);
	if (rc) {
		/* Allocate a pool */
		struct cfa_mm_query_parms qparms;
		struct cfa_mm_open_parms oparms;
		u16 fid;

		/* There is only 1 pool for a non-shared table scope
		 * and it is full.
		 */
		if (scope_type == CFA_SCOPE_TYPE_NON_SHARED) {
			netdev_dbg(bp->dev, "%s: no records remain\n", __func__);
			return -ENOMEM;
		}
		rc = tfc_get_fid(tfcp, &fid);
		if (rc)
			return rc;

		rc = tfc_tbl_scope_pool_alloc(tfcp,
					      fid,
					      tsid,
					      CFA_REGION_TYPE_ACT,
					      cmm_info->dir,
					      NULL,
					      &pool_id);
		if (rc) {
			netdev_dbg(bp->dev, "%s: fid(%d) tsid(%d:%s_action) pool alloc failed: (%d)\n",
				   __func__, fid, tsid, tfc_dir_2_str(cmm_info->dir), rc);
			return -EINVAL;
		}
		netdev_dbg(bp->dev, "fid(%d) tsid(%d) action dir(%s) pool_id(%d) allocated\n",
			   fid, tsid, tfc_dir_2_str(cmm_info->dir), pool_id);
		/* Create pool CMM instance */
		qparms.max_records = mem_cfg.rec_cnt / max_pools;
		qparms.max_contig_records = roundup_pow_of_two(max_contig_rec);
		rc = cfa_mm_query(&qparms);
		if (rc) {
			netdev_dbg(bp->dev, "%s: cfa_mm_query() failed: %d\n",
				   __func__, rc);
			return -EINVAL;
		}

		cmm = vzalloc(qparms.db_size);
		if (!cmm)
			return -ENOMEM;

		oparms.db_mem_size = qparms.db_size;
		oparms.max_contig_records = roundup_pow_of_two(qparms.max_contig_records);
		oparms.max_records = qparms.max_records / max_pools;
		rc = cfa_mm_open(cmm, &oparms);
		if (rc) {
			netdev_dbg(bp->dev, "%s: cfa_mm_open() failed: %d\n",
				   __func__, rc);
			vfree(cmm);
			return -EINVAL;
		}

		/* Store CMM instance in the CPM */
		rc = tfc_cpm_set_cmm_inst(cpm_act, pool_id, cmm);
		if (rc) {
			netdev_dbg(bp->dev, "%s: tfc_cpm_set_cmm_inst() failed: %d\n",
				   __func__, rc);
			vfree(cmm);
			return -EINVAL;
		}

		/* store updated pool info */
		tfo_ts_set_pool_info(tfcp->tfo, tsid, cmm_info->dir, CFA_REGION_TYPE_ACT,
				     max_contig_rec, pool_sz_exp);
	} else {
		/* Get the pool instance and allocate an act rec index from the pool */
		rc = tfc_cpm_get_cmm_inst(cpm_act, pool_id, &cmm);
		if (rc) {
			netdev_dbg(bp->dev, "%s: tfc_cpm_get_cmm_inst() failed: %d\n",
				   __func__, rc);
			vfree(cmm);
			return -EINVAL;
		}
	}

	aparms.num_contig_records = roundup_pow_of_two(num_contig_rec);
	rc = cfa_mm_alloc(cmm, &aparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: cfa_mm_alloc() failed: %d\n",
			   __func__, rc);
		vfree(cmm);
		return -EINVAL;
	}

	/* Update CPM info so it will determine best pool to use next alloc */
	rc = tfc_cpm_set_usage(cpm_act, pool_id, aparms.used_count, aparms.all_used);
	if (rc) {
		netdev_dbg(bp->dev, "%s: EM insert tfc_cpm_set_usage() failed: %d\n",
			   __func__, rc);
	}

	CREATE_OFFSET(&entry_offset, pool_sz_exp, pool_id, aparms.record_offset);

	/* Create Action handle */
	cmm_info->act_handle = tfc_create_action_handle(tsid, num_contig_rec, entry_offset);

	return rc;
}

int tfc_act_set_response(struct bnxt *bp,
			 struct cfa_bld_mpcinfo *mpc_info,
			 struct bnxt_mpc_mbuf *mpc_msg_out,
			 uint8_t *rx_msg)
{
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_WRITE_CMP_MAX_FLD];
	int rc;
	int i;

	/* Process response */
	for (i = 0; i < CFA_BLD_MPC_WRITE_CMP_MAX_FLD; i++)
		fields_cmp[i].field_id = INVALID_U16;

	fields_cmp[CFA_BLD_MPC_WRITE_CMP_STATUS_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMP_STATUS_FLD;

	rc = mpc_info->mpcops->cfa_bld_mpc_parse_cache_write(rx_msg,
							     mpc_msg_out->msg_size,
							     fields_cmp);

	if (unlikely(rc)) {
		netdev_dbg(bp->dev, "%s: write parse failed: %d\n",
			   __func__, rc);
		rc = -EINVAL;
	}

	if (unlikely(fields_cmp[CFA_BLD_MPC_WRITE_CMP_STATUS_FLD].val != CFA_BLD_MPC_OK)) {
		netdev_dbg(bp->dev, "%s: failed with status code:%d\n",
			   __func__,
			    (uint32_t)fields_cmp[CFA_BLD_MPC_WRITE_CMP_STATUS_FLD].val);
		rc = ((int)fields_cmp[CFA_BLD_MPC_WRITE_CMP_STATUS_FLD].val) * -1;
	}

	return rc;
}

int tfc_act_set(struct tfc *tfcp, const struct tfc_cmm_info *cmm_info, const u8 *data,
		u16 data_sz_words, struct tfc_mpc_batch_info_t *batch_info)
{
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_WRITE_CMD_MAX_FLD];
	u8 tx_msg[TFC_MPC_MAX_TX_BYTES], rx_msg[TFC_MPC_MAX_RX_BYTES];
	u32 i, buff_len, entry_offset, record_size;
	u32 mpc_opaque = TFC_MPC_OPAQUE_VAL;
	struct bnxt_mpc_mbuf mpc_msg_out;
	struct cfa_bld_mpcinfo *mpc_info;
	struct bnxt_mpc_mbuf mpc_msg_in;
	enum cfa_scope_type scope_type;
	struct bnxt *bp = tfcp->bp;
	bool valid;
	u8 tsid;
	int rc;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	/* Check that MPC APIs are bound */
	if (!mpc_info->mpcops) {
		netdev_dbg(bp->dev, "%s: MPC not initialized\n",
			   __func__);
		return -EINVAL;
	}

	tfc_get_fields_from_action_handle(&cmm_info->act_handle,
					  &tsid,
					  &record_size,
					  &entry_offset);

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (rc) {
		netdev_dbg(bp->dev, "%s: failed to get tsid: rc:%d\n", __func__, rc);
		return -EINVAL;
	}
	if (!valid) {
		netdev_dbg(bp->dev, "%s: tsid not allocated %d\n", __func__, tsid);
		return -EINVAL;
	}

	/* Create MPC EM insert command using builder */
	for (i = 0; i < CFA_BLD_MPC_WRITE_CMD_MAX_FLD; i++)
		fields_cmd[i].field_id = INVALID_U16;

	fields_cmd[CFA_BLD_MPC_WRITE_CMD_OPAQUE_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_OPAQUE_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_OPAQUE_FLD].val = 0xAA;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_TYPE_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_TABLE_TYPE_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_TYPE_FLD].val = CFA_BLD_MPC_HW_TABLE_TYPE_ACTION;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_SCOPE_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_TABLE_SCOPE_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_SCOPE_FLD].val = tsid;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_DATA_SIZE_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_DATA_SIZE_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_DATA_SIZE_FLD].val = data_sz_words;
#if TFC_ACT_CACHE_OPT_EN
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_CACHE_OPTION_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_CACHE_OPTION_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_CACHE_OPTION_FLD].val = 0x01;
#endif
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_INDEX_FLD].field_id =
		CFA_BLD_MPC_WRITE_CMD_TABLE_INDEX_FLD;
	fields_cmd[CFA_BLD_MPC_WRITE_CMD_TABLE_INDEX_FLD].val = entry_offset;

	buff_len = TFC_MPC_MAX_TX_BYTES;

	rc = mpc_info->mpcops->cfa_bld_mpc_build_cache_write(tx_msg,
							     &buff_len,
							     data,
							     fields_cmd);
	if (rc) {
		netdev_dbg(bp->dev, "%s: write build failed: %d\n",
			   __func__, rc);
		goto cleanup;
	}

#ifdef TFC_ACT_MSG_DEBUG
	netdev_dbg(bp->dev, "Tx Msg: size:%d\n", buff_len);
	bnxt_tfc_buf_dump(bp, NULL, (uint8_t *)tx_msg, buff_len, 4, 4);
#endif

	/* Send MPC */
	mpc_msg_in.chnl_id = (cmm_info->dir == CFA_DIR_TX ?
			      RING_ALLOC_REQ_MPC_CHNLS_TYPE_TE_CFA :
			      RING_ALLOC_REQ_MPC_CHNLS_TYPE_RE_CFA);
	mpc_msg_in.msg_data = &tx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_in.msg_size = buff_len - TFC_MPC_HEADER_SIZE_BYTES;
	mpc_msg_out.cmp_type = MPC_CMP_TYPE_MID_PATH_SHORT;
	mpc_msg_out.msg_data = &rx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_out.msg_size = TFC_MPC_MAX_RX_BYTES;

	rc = bnxt_mpc_send(tfcp->bp,
			   &mpc_msg_in,
			   &mpc_msg_out,
			   &mpc_opaque,
			   TFC_MPC_TABLE_WRITE,
			   batch_info);
	if (rc) {
		netdev_dbg(bp->dev, "%s: write MPC send failed: %d\n",
			   __func__, rc);
		goto cleanup;
	}

#ifdef TFC_ACT_MSG_DEBUG
	netdev_dbg(bp->dev, "Rx Msg: size:%d\n", mpc_msg_out.msg_size);
	bnxt_tfc_buf_dump(bp, NULL, (uint8_t *)rx_msg, buff_len, 4, 4);
#endif

	if ((batch_info && !batch_info->enabled) || !batch_info)
		rc =  tfc_act_set_response(bp, mpc_info, &mpc_msg_out, rx_msg);
	else
		batch_info->comp_info[batch_info->count - 1].bp = bp;

 cleanup:
	return rc;
}

int tfc_act_get_only_response(struct bnxt *bp,
			      struct cfa_bld_mpcinfo *mpc_info,
			      struct bnxt_mpc_mbuf *mpc_msg_out,
			      uint8_t *rx_msg,
			      uint16_t *data_sz_words)
{
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_READ_CMP_MAX_FLD] = { {0} };
	u8 discard_data[128];
	int rc;
	int i;

	/* Process response */
	for (i = 0; i < CFA_BLD_MPC_READ_CMP_MAX_FLD; i++)
		fields_cmp[i].field_id = INVALID_U16;

	fields_cmp[CFA_BLD_MPC_READ_CMP_STATUS_FLD].field_id =
		CFA_BLD_MPC_READ_CMP_STATUS_FLD;

	rc = mpc_info->mpcops->cfa_bld_mpc_parse_cache_read(rx_msg,
							    mpc_msg_out->msg_size,
							    discard_data,
							    *data_sz_words * TFC_MPC_BYTES_PER_WORD,
							    fields_cmp);

	if (rc) {
		netdev_dbg(bp->dev, "%s: Action read parse failed: %d\n",
			   __func__, rc);
		return -1;
	}

	if (fields_cmp[CFA_BLD_MPC_READ_CMP_STATUS_FLD].val != CFA_BLD_MPC_OK) {
		netdev_dbg(bp->dev, "%s: Action read failed with status code:%d\n",
			   __func__,
			   (uint32_t)fields_cmp[CFA_BLD_MPC_READ_CMP_STATUS_FLD].val);
		rc = ((int)fields_cmp[CFA_BLD_MPC_READ_CMP_STATUS_FLD].val) * -1;
		return rc;
	}

	return 0;
}

static int tfc_act_get_only(struct tfc *tfcp,
			    struct tfc_mpc_batch_info_t *batch_info,
			    const struct tfc_cmm_info *cmm_info,
			    u64 data_pa,
			    u16 *data_sz_words)
{
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_READ_CMD_MAX_FLD] = { {0} };
	u8 tx_msg[TFC_MPC_MAX_TX_BYTES] = { 0 };
	u8 rx_msg[TFC_MPC_MAX_RX_BYTES] = { 0 };
	u32 entry_offset, record_size, buff_len;
	u32 mpc_opaque = TFC_MPC_OPAQUE_VAL;
	struct cfa_bld_mpcinfo *mpc_info;
	struct bnxt_mpc_mbuf mpc_msg_out;
	struct bnxt_mpc_mbuf mpc_msg_in;
	enum cfa_scope_type scope_type;
	struct bnxt *bp = tfcp->bp;
	bool valid;
	int i, rc;
	u8 tsid;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	tfc_get_fields_from_action_handle(&cmm_info->act_handle, &tsid,
					  &record_size, &entry_offset);

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (rc) {
		netdev_dbg(bp->dev, "%s: failed to get tsid: rc:%d\n", __func__, rc);
		return -EINVAL;
	}

	if (!valid) {
		netdev_dbg(bp->dev, "%s: tsid not allocated %d\n", __func__, tsid);
		return -EINVAL;
	}

	/* Check that MPC APIs are bound */
	if (!mpc_info->mpcops) {
		netdev_dbg(bp->dev, "%s: MPC not initialized\n",
			   __func__);
		return -EINVAL;
	}

	/* Create MPC EM insert command using builder */
	for (i = 0; i < CFA_BLD_MPC_READ_CMD_MAX_FLD; i++)
		fields_cmd[i].field_id = INVALID_U16;

	fields_cmd[CFA_BLD_MPC_READ_CMD_OPAQUE_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_OPAQUE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_OPAQUE_FLD].val = 0xAA;

	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_TYPE_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_TABLE_TYPE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_TYPE_FLD].val =
		CFA_BLD_MPC_HW_TABLE_TYPE_ACTION;

	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_SCOPE_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_TABLE_SCOPE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_SCOPE_FLD].val = tsid;

	fields_cmd[CFA_BLD_MPC_READ_CMD_DATA_SIZE_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_DATA_SIZE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_DATA_SIZE_FLD].val = *data_sz_words;

#if TFC_ACT_CACHE_OPT_EN
	fields_cmd[CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_CACHE_OPTION_FLD].val = 0x0;
#endif
	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_INDEX_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_TABLE_INDEX_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_TABLE_INDEX_FLD].val = entry_offset;

	fields_cmd[CFA_BLD_MPC_READ_CMD_HOST_ADDRESS_FLD].field_id =
		CFA_BLD_MPC_READ_CMD_HOST_ADDRESS_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CMD_HOST_ADDRESS_FLD].val = data_pa;

	buff_len = TFC_MPC_MAX_TX_BYTES;

	rc = mpc_info->mpcops->cfa_bld_mpc_build_cache_read(tx_msg,
							    &buff_len,
							    fields_cmd);
	if (rc) {
		netdev_dbg(bp->dev, "%s: read build failed: %d\n",
			   __func__, rc);
		goto cleanup;
	}

	/* Send MPC */
	mpc_msg_in.chnl_id = (cmm_info->dir == CFA_DIR_TX ?
			      RING_ALLOC_REQ_MPC_CHNLS_TYPE_TE_CFA :
			      RING_ALLOC_REQ_MPC_CHNLS_TYPE_RE_CFA);
	mpc_msg_in.msg_data = &tx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_in.msg_size = buff_len - TFC_MPC_HEADER_SIZE_BYTES;
	mpc_msg_out.cmp_type = MPC_CMP_TYPE_MID_PATH_SHORT;
	mpc_msg_out.msg_data = &rx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_out.msg_size = TFC_MPC_MAX_RX_BYTES;

	rc = bnxt_mpc_send(tfcp->bp,
			   &mpc_msg_in,
			   &mpc_msg_out,
			   &mpc_opaque,
			   TFC_MPC_TABLE_READ,
			   batch_info);
	if (rc) {
		netdev_dbg(bp->dev, "%s: read MPC send failed: %d\n",
			   __func__, rc);
		goto cleanup;
	}

	/* Process response */
	if ((batch_info && !batch_info->enabled) || !batch_info) {
		rc = tfc_act_get_only_response(bp,
					       mpc_info,
					       &mpc_msg_out,
					       rx_msg,
					       data_sz_words);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Action response failed: %d\n",
				   __func__, rc);
			goto cleanup;
		}
	} else {
		batch_info->comp_info[batch_info->count - 1].read_words = *data_sz_words;
		batch_info->comp_info[batch_info->count - 1].bp = bp;
	}

	return 0;

cleanup:
	return rc;
}

int tfc_act_get_clear_response(struct bnxt *bp,
			       struct cfa_bld_mpcinfo *mpc_info,
			       struct bnxt_mpc_mbuf *mpc_msg_out,
			       uint8_t *rx_msg,
			       uint16_t *data_sz_words)
{
	struct cfa_mpc_data_obj fields_cmp[CFA_BLD_MPC_READ_CLR_CMP_MAX_FLD] = { {0} };
	uint8_t discard_data[128];
	int rc;
	int i;

	/* Process response */
	for (i = 0; i < CFA_BLD_MPC_READ_CLR_CMP_MAX_FLD; i++)
		fields_cmp[i].field_id = INVALID_U16;

	fields_cmp[CFA_BLD_MPC_READ_CLR_CMP_STATUS_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMP_STATUS_FLD;

	rc = mpc_info->mpcops->cfa_bld_mpc_parse_cache_read_clr(rx_msg,
								mpc_msg_out->msg_size,
								discard_data,
								*data_sz_words *
								TFC_MPC_BYTES_PER_WORD,
								fields_cmp);

	if (rc) {
		netdev_dbg(bp->dev, "%s: Action read clear parse failed: %d\n",
			   __func__, rc);
		return -1;
	}

	if (fields_cmp[CFA_BLD_MPC_READ_CLR_CMP_STATUS_FLD].val != CFA_BLD_MPC_OK) {
		netdev_dbg(bp->dev, "%s: Action read clear failed with status code:%d\n",
			   __func__,
			   (uint32_t)fields_cmp[CFA_BLD_MPC_READ_CLR_CMP_STATUS_FLD].val);
		rc = ((int)fields_cmp[CFA_BLD_MPC_READ_CLR_CMP_STATUS_FLD].val) * -1;
		return rc;
	}

	return 0;
}

static int tfc_act_get_clear(struct tfc *tfcp,
			     struct tfc_mpc_batch_info_t *batch_info,
			     const struct tfc_cmm_info *cmm_info,
			     u64 data_pa,
			     u16 *data_sz_words,
			     u8 clr_offset,
			     u8 clr_size)
{
	struct cfa_mpc_data_obj fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_MAX_FLD] = { {0} };
	u8 tx_msg[TFC_MPC_MAX_TX_BYTES] = { 0 };
	u8 rx_msg[TFC_MPC_MAX_RX_BYTES] = { 0 };
	u32 msg_count = BNXT_MPC_COMP_MSG_COUNT;
	struct cfa_bld_mpcinfo *mpc_info;
	struct bnxt_mpc_mbuf mpc_msg_in;
	struct bnxt_mpc_mbuf mpc_msg_out;
	enum cfa_scope_type scope_type;
	struct bnxt *bp = tfcp->bp;
	u32 entry_offset;
	u32 record_size;
	u32 buff_len;
	u16 mask = 0;
	int rc = 0;
	bool valid;
	u8 tsid;
	int i;

	tfo_mpcinfo_get(tfcp->tfo, &mpc_info);

	tfc_get_fields_from_action_handle(&cmm_info->act_handle,
					  &tsid,
					  &record_size,
					  &entry_offset);

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (rc != 0) {
		netdev_dbg(bp->dev, "%s: failed to get tsid: %d\n",
			   __func__, rc);
		return -EINVAL;
	}
	if (!valid) {
		netdev_dbg(bp->dev, "%s: tsid not allocated %d\n",
			   __func__, tsid);
		return -EINVAL;
	}

	/* Check that MPC APIs are bound */
	if (!mpc_info->mpcops) {
		netdev_dbg(bp->dev, "%s: MPC not initialized\n",
			   __func__);
		return -EINVAL;
	}

	/* Create MPC EM insert command using builder */
	for (i = 0; i < CFA_BLD_MPC_READ_CLR_CMD_MAX_FLD; i++)
		fields_cmd[i].field_id = INVALID_U16;

	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_OPAQUE_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_OPAQUE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_OPAQUE_FLD].val = 0xAA;

	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_TABLE_TYPE_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_TABLE_TYPE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_TABLE_TYPE_FLD].val =
		CFA_BLD_MPC_HW_TABLE_TYPE_ACTION;

	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_TABLE_SCOPE_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_TABLE_SCOPE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_TABLE_SCOPE_FLD].val = tsid;

	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_DATA_SIZE_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_DATA_SIZE_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_DATA_SIZE_FLD].val = *data_sz_words;

#if TFC_ACT_CACHE_OPT_EN
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_CACHE_OPTION_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_CACHE_OPTION_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_CACHE_OPTION_FLD].val = 0x0;
#endif
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_TABLE_INDEX_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_TABLE_INDEX_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_TABLE_INDEX_FLD].val = entry_offset;

	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_HOST_ADDRESS_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_HOST_ADDRESS_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_HOST_ADDRESS_FLD].val = data_pa;

	for (i = clr_offset; i < clr_size; i++)
		mask |= (1 << i);

	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_CLEAR_MASK_FLD].field_id =
		CFA_BLD_MPC_READ_CLR_CMD_CLEAR_MASK_FLD;
	fields_cmd[CFA_BLD_MPC_READ_CLR_CMD_CLEAR_MASK_FLD].val = mask;

	buff_len = TFC_MPC_MAX_TX_BYTES;

	rc = mpc_info->mpcops->cfa_bld_mpc_build_cache_read_clr(tx_msg,
								&buff_len,
								fields_cmd);

	if (rc) {
		netdev_dbg(bp->dev, "%s: read clear build failed: %d\n",
			   __func__, rc);
		goto cleanup;
	}

	/* Send MPC */
	mpc_msg_in.chnl_id = (cmm_info->dir == CFA_DIR_TX ?
			      RING_ALLOC_REQ_MPC_CHNLS_TYPE_TE_CFA :
			      RING_ALLOC_REQ_MPC_CHNLS_TYPE_RE_CFA);
	mpc_msg_in.msg_data = &tx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_in.msg_size = buff_len - TFC_MPC_HEADER_SIZE_BYTES;
	mpc_msg_out.cmp_type = MPC_CMP_TYPE_MID_PATH_SHORT;
	mpc_msg_out.msg_data = &rx_msg[TFC_MPC_HEADER_SIZE_BYTES];
	mpc_msg_out.msg_size = TFC_MPC_MAX_RX_BYTES;

	rc = bnxt_mpc_send(tfcp->bp,
			   &mpc_msg_in,
			   &mpc_msg_out,
			   &msg_count,
			   TFC_MPC_TABLE_READ_CLEAR,
			   batch_info);

	if (rc) {
		netdev_dbg(bp->dev, "%s: read clear MPC send failed: %d\n",
			   __func__, rc);
		goto cleanup;
	}

	if ((batch_info && !batch_info->enabled) || !batch_info) {
		rc = tfc_act_get_clear_response(bp,
						mpc_info,
						&mpc_msg_out,
						rx_msg,
						data_sz_words);
		if (rc) {
			netdev_dbg(bp->dev, "%s: Action response failed: %d\n",
				   __func__, rc);
			goto cleanup;
		}
	} else {
		batch_info->comp_info[batch_info->count - 1].read_words = *data_sz_words;
		batch_info->comp_info[batch_info->count - 1].bp = bp;
	}

	return 0;

cleanup:

	return rc;
}

int tfc_act_get(struct tfc *tfcp,
		struct tfc_mpc_batch_info_t *batch_info,
		const struct tfc_cmm_info *cmm_info,
		struct tfc_cmm_clr *clr,
		u64 data_pa, u16 *data_sz_words)
{
	struct bnxt *bp = tfcp->bp;

	/* It's not an error to pass clr as a Null pointer, just means that read
	 * and clear is not being requested.  Also allow the user to manage
	 * clear via the clr flag.
	 */
	if (clr && clr->clr) {
		/* Clear offset and size have to be two bytes aligned */
		if (clr->offset_in_byte % 2 || clr->sz_in_byte % 2) {
			netdev_dbg(bp->dev, "%s: clr offset(%d) or size(%d) is not two bytes aligned.\n",
				   __func__, clr->offset_in_byte, clr->sz_in_byte);
			return -EINVAL;
		}

		return tfc_act_get_clear(tfcp, batch_info, cmm_info,
					 data_pa, data_sz_words,
					 clr->offset_in_byte / 2,
					 clr->sz_in_byte / 2);
	} else {
		return tfc_act_get_only(tfcp, batch_info, cmm_info,
					data_pa, data_sz_words);
	}
}

int tfc_act_free(struct tfc *tfcp,
		 const struct tfc_cmm_info *cmm_info)
{
	u32 pool_id = 0, record_size, record_offset;
	struct cfa_mm_free_parms fparms;
	struct tfc_cpm *cpm_act = NULL;
	enum cfa_scope_type scope_type;
	struct tfc_ts_mem_cfg mem_cfg;
	struct bnxt *bp = tfcp->bp;
	struct tfc_cmm *cmm;
	u16 max_contig_rec;
	bool is_bs_owner;
	u8 pool_sz_exp;
	bool valid;
	u8 tsid;
	int rc;

	/* Get fields from MPC Action handle */
	tfc_get_fields_from_action_handle(&cmm_info->act_handle, &tsid,
					  &record_size, &record_offset);

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (rc) {
		netdev_dbg(bp->dev, "%s: failed to get tsid: rc:%d\n", __func__, rc);
		return -EINVAL;
	}

	if (!valid) {
		netdev_dbg(bp->dev, "%s: tsid not allocated %d\n", __func__, tsid);
		return -EINVAL;
	}

	rc = tfo_ts_get_pool_info(tfcp->tfo, tsid, cmm_info->dir, CFA_REGION_TYPE_ACT,
				  &max_contig_rec, &pool_sz_exp);
	if (rc) {
		netdev_dbg(bp->dev,
			   "%s: Failed to get pool info for tsid:%d\n",
			   __func__, tsid);
		return -EINVAL;
	}

	pool_id = TFC_ACTION_GET_POOL_ID(record_offset, pool_sz_exp);

	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid,
				cmm_info->dir,
				CFA_REGION_TYPE_ACT,
				&is_bs_owner,
				&mem_cfg);
	if (rc) {
		netdev_dbg(bp->dev, "%s: tfo_ts_get_mem_cfg() failed: %d\n",
			   __func__, rc);
		return -EINVAL;
	}
	/* Get CPM instance for this table scope */
	rc = tfo_ts_get_cpm_inst(tfcp->tfo, tsid, bp->pf.fw_fid, cmm_info->dir,
				 CFA_REGION_TYPE_ACT, &cpm_act);
	if (rc) {
		netdev_dbg(bp->dev, "%s: failed to get CPM instance: %d\n",
			   __func__, rc);
		return -EINVAL;
	}

	rc = tfc_cpm_get_cmm_inst(cpm_act, pool_id, &cmm);
	if (rc) {
		netdev_dbg(bp->dev, "%s: failed to get record: %d\n", __func__, rc);
		return -EINVAL;
	}

	fparms.record_offset = REMOVE_POOL_FROM_OFFSET(pool_sz_exp, record_offset);
	fparms.num_contig_records = roundup_pow_of_two(record_size);
	rc = cfa_mm_free(cmm, &fparms);
	if (rc) {
		netdev_dbg(bp->dev, "%s: failed to free record offset(0x%x/%d) num(%d): %d\n",
			   __func__, record_offset, record_offset, fparms.num_contig_records, rc);
		return -EINVAL;
	}

	rc = tfc_cpm_set_usage(cpm_act, pool_id, 0, false);
	if (rc)
		netdev_dbg(bp->dev, "%s: failed to set usage: %d\n", __func__, rc);

	return rc;
}
