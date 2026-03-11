// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include "bnxt_compat.h"
#include "sys_util.h"
#include "cfa_types.h"
#include "cfa_bld_p70_mpc.h"
#include "cfa_bld_p70_mpc_defs.h"
#include "cfa_p70_mpc_cmds.h"
#include "cfa_p70_mpc_cmpls.h"

/* CFA MPC client ids */
#define MP_CLIENT_TE_CFA READ_CMP_MP_CLIENT_TE_CFA
#define MP_CLIENT_RE_CFA READ_CMP_MP_CLIENT_RE_CFA

/* MPC Client id check in CFA completion messages */
#define ASSERT_CFA_MPC_CLIENT_ID(MPCID)                                            \
	do {                                                                       \
		if ((MPCID) != MP_CLIENT_TE_CFA &&                                 \
		    (MPCID) != MP_CLIENT_RE_CFA) {                                 \
			netdev_warn(NULL,                                          \
				    "Unexpected MPC client id in response: %d\n",  \
				    (MPCID));                                      \
		}                                                                  \
	} while (0)

/** Add MPC header information to MPC command message */
static int fill_mpc_header(u8 *cmd, u32 size, u32 opaque_val)
{
	struct mpc_header hdr = {
		.opaque = opaque_val,
	};

	if (size < sizeof(struct mpc_header)) {
		netdev_dbg(NULL, "%s: invalid parameter: size:%d too small\n", __func__, size);
		ASSERT_RTNL();
		return -EINVAL;
	}

	memcpy(cmd, &hdr, sizeof(hdr));

	return 0;
}

/** Compose Table read-clear message */
static int compose_mpc_read_clr_msg(u8 *cmd_buff, u32 *cmd_buff_len,
				    struct cfa_mpc_cache_axs_params *parms)
{
	u32 cmd_size = sizeof(struct mpc_header) + TFC_MPC_CMD_TBL_RDCLR_SIZE;
	struct cfa_mpc_cache_read_params *rd_parms = &parms->read;
	u8 *cmd;

	if (parms->data_size != 1) {
		netdev_dbg(NULL, "%s: invalid parameter: data_size:%d\n",
			   __func__, parms->data_size);
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (parms->tbl_type >= CFA_HW_TABLE_MAX) {
		netdev_dbg(NULL, "%s: invalid parameter: tbl_typed: %d out of range\n",
			   __func__, parms->tbl_type);
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (*cmd_buff_len < cmd_size) {
		netdev_dbg(NULL, "%s: invalid parameter: cmd_buff_len too small\n", __func__);
		ASSERT_RTNL();
		return -EINVAL;
	}

	cmd = cmd_buff + sizeof(struct mpc_header);

	/* Populate CFA MPC command header */
	memset(cmd, 0, TFC_MPC_CMD_TBL_RDCLR_SIZE);
	TFC_MPC_CMD_TBL_RDCLR_SET_OPCODE(cmd, TFC_MPC_CMD_OPCODE_READ_CLR);
	TFC_MPC_CMD_TBL_RDCLR_SET_TABLE_TYPE(cmd, parms->tbl_type);
	TFC_MPC_CMD_TBL_RDCLR_SET_TABLE_SCOPE(cmd, parms->tbl_scope);
	TFC_MPC_CMD_TBL_RDCLR_SET_DATA_SIZE(cmd, parms->data_size);
	TFC_MPC_CMD_TBL_RDCLR_SET_TABLE_INDEX(cmd, parms->tbl_index);
	TFC_MPC_CMD_TBL_RDCLR_SET_HOST_ADDRESS_0(cmd, (u32)rd_parms->host_address);
	TFC_MPC_CMD_TBL_RDCLR_SET_HOST_ADDRESS_1(cmd, (u32)(rd_parms->host_address >> 32));
	switch (rd_parms->mode) {
	case CFA_MPC_RD_EVICT:
		TFC_MPC_CMD_TBL_RDCLR_SET_CACHE_OPTION(cmd, CACHE_READ_CLR_OPTION_EVICT);
		break;
	case CFA_MPC_RD_NORMAL:
	default:
		TFC_MPC_CMD_TBL_RDCLR_SET_CACHE_OPTION(cmd, CACHE_READ_CLR_OPTION_NORMAL);
		break;
	}
	TFC_MPC_CMD_TBL_RDCLR_SET_CLEAR_MASK(cmd, rd_parms->clear_mask);

	*cmd_buff_len = cmd_size;

	return 0;
}

/** Compose Table read message */
static int compose_mpc_read_msg(u8 *cmd_buff, u32 *cmd_buff_len,
				struct cfa_mpc_cache_axs_params *parms)
{
	u32 cmd_size = sizeof(struct mpc_header) + TFC_MPC_CMD_TBL_RD_SIZE;
	struct cfa_mpc_cache_read_params *rd_parms = &parms->read;
	u8 *cmd;

	if (parms->data_size < 1 || parms->data_size > 4) {
		netdev_dbg(NULL, "%s: invalid parameter: data_size:%d out of range\n",
			   __func__, parms->data_size);
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (parms->tbl_type >= CFA_HW_TABLE_MAX) {
		netdev_dbg(NULL, "%s: invalid parameter: tbl_typed: %d out of range\n",
			   __func__, parms->tbl_type);
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (*cmd_buff_len < cmd_size) {
		netdev_dbg(NULL, "%s: invalid parameter: cmd_buff_len too small\n", __func__);
		ASSERT_RTNL();
		return -EINVAL;
	}

	cmd = (cmd_buff + sizeof(struct mpc_header));

	/* Populate CFA MPC command header */
	memset(cmd, 0, TFC_MPC_CMD_TBL_RD_SIZE);
	TFC_MPC_CMD_TBL_RD_SET_OPCODE(cmd, TFC_MPC_CMD_OPCODE_READ);
	TFC_MPC_CMD_TBL_RD_SET_TABLE_TYPE(cmd, parms->tbl_type);
	TFC_MPC_CMD_TBL_RD_SET_TABLE_SCOPE(cmd, parms->tbl_scope);
	TFC_MPC_CMD_TBL_RD_SET_DATA_SIZE(cmd, parms->data_size);
	TFC_MPC_CMD_TBL_RD_SET_TABLE_INDEX(cmd, parms->tbl_index);
	TFC_MPC_CMD_TBL_RD_SET_HOST_ADDRESS_0(cmd, (u32)rd_parms->host_address);
	TFC_MPC_CMD_TBL_RD_SET_HOST_ADDRESS_1(cmd, (u32)(rd_parms->host_address >> 32));
	switch (rd_parms->mode) {
	case CFA_MPC_RD_EVICT:
		TFC_MPC_CMD_TBL_RD_SET_CACHE_OPTION(cmd, CACHE_READ_OPTION_EVICT);
		break;
	case CFA_MPC_RD_DEBUG_LINE:
		TFC_MPC_CMD_TBL_RD_SET_CACHE_OPTION(cmd, CACHE_READ_OPTION_DEBUG_LINE);
		break;
	case CFA_MPC_RD_DEBUG_TAG:
		TFC_MPC_CMD_TBL_RD_SET_CACHE_OPTION(cmd, CACHE_READ_OPTION_DEBUG_TAG);
		break;
	case CFA_MPC_RD_NORMAL:
	default:
		TFC_MPC_CMD_TBL_RD_SET_CACHE_OPTION(cmd, CACHE_READ_OPTION_NORMAL);
		break;
	}

	*cmd_buff_len = cmd_size;

	return 0;
}

/** Compose Table write message */
static int compose_mpc_write_msg(u8 *cmd_buff, u32 *cmd_buff_len,
				 struct cfa_mpc_cache_axs_params *parms)
{
	u32 cmd_size = sizeof(struct mpc_header) + TFC_MPC_CMD_TBL_WR_SIZE +
			    parms->data_size * MPC_CFA_CACHE_ACCESS_UNIT_SIZE;
	struct cfa_mpc_cache_write_params *wr_parms = &parms->write;
	u8 *cmd;

	if (parms->data_size < 1 || parms->data_size > 4) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (parms->tbl_type >= CFA_HW_TABLE_MAX) {
		netdev_dbg(NULL, "%s: invalid parameter: tbl_typed: %d out of range\n",
			   __func__, parms->tbl_type);
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (!parms->write.data_ptr) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (*cmd_buff_len < cmd_size) {
		netdev_dbg(NULL, "%s: invalid parameter: cmd_buff_len too small\n", __func__);
		ASSERT_RTNL();
		return -EINVAL;
	}

	cmd = (cmd_buff + sizeof(struct mpc_header));

	/* Populate CFA MPC command header */
	memset(cmd, 0, TFC_MPC_CMD_TBL_WR_SIZE);
	TFC_MPC_CMD_TBL_WR_SET_OPCODE(cmd, TFC_MPC_CMD_OPCODE_WRITE);
	TFC_MPC_CMD_TBL_WR_SET_TABLE_TYPE(cmd, parms->tbl_type);
	TFC_MPC_CMD_TBL_WR_SET_TABLE_SCOPE(cmd, parms->tbl_scope);
	TFC_MPC_CMD_TBL_WR_SET_DATA_SIZE(cmd, parms->data_size);
	TFC_MPC_CMD_TBL_WR_SET_TABLE_INDEX(cmd, parms->tbl_index);
	switch (wr_parms->mode) {
	case CFA_MPC_WR_WRITE_THRU:
		TFC_MPC_CMD_TBL_WR_SET_CACHE_OPTION(cmd, CACHE_WRITE_OPTION_WRITE_THRU);
		break;
	case CFA_MPC_WR_WRITE_BACK:
	default:
		TFC_MPC_CMD_TBL_WR_SET_CACHE_OPTION(cmd, CACHE_WRITE_OPTION_WRITE_BACK);
		break;
	}

	/* Populate CFA MPC command payload following the header */
	memcpy(cmd + TFC_MPC_CMD_TBL_WR_SIZE, wr_parms->data_ptr,
	       parms->data_size * MPC_CFA_CACHE_ACCESS_UNIT_SIZE);

	*cmd_buff_len = cmd_size;

	return 0;
}

/** Compose Invalidate message */
static int compose_mpc_evict_msg(u8 *cmd_buff, u32 *cmd_buff_len,
				 struct cfa_mpc_cache_axs_params *parms)
{
	u32 cmd_size = sizeof(struct mpc_header) + TFC_MPC_CMD_TBL_INV_SIZE;
	struct cfa_mpc_cache_evict_params *ev_parms = &parms->evict;
	u8 *cmd;

	if (parms->data_size < 1 || parms->data_size > 4) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (parms->tbl_type >= CFA_HW_TABLE_MAX) {
		netdev_dbg(NULL, "%s: invalid parameter: tbl_typed: %d out of range\n",
			   __func__, parms->tbl_type);
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (*cmd_buff_len < cmd_size) {
		netdev_dbg(NULL, "%s: invalid parameter: cmd_buff_len too small\n", __func__);
		ASSERT_RTNL();
		return -EINVAL;
	}

	cmd = cmd_buff + sizeof(struct mpc_header);

	/* Populate CFA MPC command header */
	memset(cmd, 0, TFC_MPC_CMD_TBL_INV_SIZE);
	TFC_MPC_CMD_TBL_INV_SET_OPCODE(cmd, TFC_MPC_CMD_OPCODE_INVALIDATE);
	TFC_MPC_CMD_TBL_INV_SET_TABLE_TYPE(cmd, parms->tbl_type);
	TFC_MPC_CMD_TBL_INV_SET_TABLE_SCOPE(cmd, parms->tbl_scope);
	TFC_MPC_CMD_TBL_INV_SET_DATA_SIZE(cmd, parms->data_size);
	TFC_MPC_CMD_TBL_INV_SET_TABLE_INDEX(cmd, parms->tbl_index);

	switch (ev_parms->mode) {
	case CFA_MPC_EV_EVICT_LINE:
		TFC_MPC_CMD_TBL_INV_SET_CACHE_OPTION(cmd, CACHE_EVICT_OPTION_LINE);
		break;
	case CFA_MPC_EV_EVICT_CLEAN_LINES:
		TFC_MPC_CMD_TBL_INV_SET_CACHE_OPTION(cmd, CACHE_EVICT_OPTION_CLEAN_LINES);
		break;
	case CFA_MPC_EV_EVICT_CLEAN_FAST_EVICT_LINES:
		TFC_MPC_CMD_TBL_INV_SET_CACHE_OPTION(cmd, CACHE_EVICT_OPTION_CLEAN_FAST_LINES);
		break;
	case CFA_MPC_EV_EVICT_CLEAN_AND_CLEAN_FAST_EVICT_LINES:
		TFC_MPC_CMD_TBL_INV_SET_CACHE_OPTION(cmd,
						     CACHE_EVICT_OPTION_CLEAN_AND_FAST_LINES);
		break;
	case CFA_MPC_EV_EVICT_TABLE_SCOPE:
		/* Not supported */
		ASSERT_RTNL();
		return -EOPNOTSUPP;
	case CFA_MPC_EV_EVICT_SCOPE_ADDRESS:
	default:
		TFC_MPC_CMD_TBL_INV_SET_CACHE_OPTION(cmd, CACHE_EVICT_OPTION_SCOPE_ADDRESS);
		break;
	}

	*cmd_buff_len = cmd_size;

	return 0;
}

/**
 * Build MPC CFA Cache access command
 *
 * @param [in] opc MPC opcode
 *
 * @param [out] cmd_buff Command data buffer to write the command to
 *
 * @param [in/out] cmd_buff_len Pointer to command buffer size param
 *        Set by caller to indicate the input cmd_buff size.
 *        Set to the actual size of the command generated by the api.
 *
 * @param [in] parms Pointer to MPC cache access command parameters
 *
 * @return 0 on Success, negative errno on failure
 */
int cfa_mpc_build_cache_axs_cmd(enum cfa_mpc_opcode opc, u8 *cmd_buff,
				u32 *cmd_buff_len,
				struct cfa_mpc_cache_axs_params *parms)
{
	int rc;

	if (!cmd_buff || !cmd_buff_len || *cmd_buff_len == 0 || !parms) {
		netdev_dbg(NULL, "%s: invalid parameter: cmd_buff_len too small\n", __func__);
		ASSERT_RTNL();
		return -EINVAL;
	}

	rc = fill_mpc_header(cmd_buff, *cmd_buff_len, parms->opaque);
	if (rc)
		return rc;

	switch (opc) {
	case CFA_MPC_READ_CLR:
		return compose_mpc_read_clr_msg(cmd_buff, cmd_buff_len, parms);
	case CFA_MPC_READ:
		return compose_mpc_read_msg(cmd_buff, cmd_buff_len, parms);
	case CFA_MPC_WRITE:
		return compose_mpc_write_msg(cmd_buff, cmd_buff_len, parms);
	case CFA_MPC_INVALIDATE:
		return compose_mpc_evict_msg(cmd_buff, cmd_buff_len, parms);
	default:
		ASSERT_RTNL();
		return -EOPNOTSUPP;
	}
}

/** Compose EM Search message */
static int compose_mpc_em_search_msg(u8 *cmd_buff, u32 *cmd_buff_len,
				     struct cfa_mpc_em_op_params *parms)
{
	struct cfa_mpc_em_search_params *e = &parms->search;
	u8 *cmd;
	u32 cmd_size = 0;

	cmd_size = sizeof(struct mpc_header) + TFC_MPC_CMD_EM_SEARCH_SIZE +
		   e->data_size * MPC_CFA_CACHE_ACCESS_UNIT_SIZE;

	if (e->data_size < 1 || e->data_size > 4) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (*cmd_buff_len < cmd_size) {
		netdev_dbg(NULL, "%s: invalid parameter: cmd_buff_len too small\n", __func__);
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (!e->em_entry) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	cmd = cmd_buff + sizeof(struct mpc_header);

	/* Populate CFA MPC command header */
	memset(cmd, 0, TFC_MPC_CMD_EM_SEARCH_SIZE);
	TFC_MPC_CMD_EM_SEARCH_SET_OPCODE(cmd, TFC_MPC_CMD_OPCODE_EM_SEARCH);
	TFC_MPC_CMD_EM_SEARCH_SET_TABLE_SCOPE(cmd, parms->tbl_scope);
	TFC_MPC_CMD_EM_SEARCH_SET_DATA_SIZE(cmd, e->data_size);
	/* Default to normal read cache option for EM search */
	TFC_MPC_CMD_EM_SEARCH_SET_CACHE_OPTION(cmd, CACHE_READ_OPTION_NORMAL);

	/* Populate CFA MPC command payload following the header */
	memcpy(cmd + TFC_MPC_CMD_EM_SEARCH_SIZE, e->em_entry,
	       e->data_size * MPC_CFA_CACHE_ACCESS_UNIT_SIZE);

	*cmd_buff_len = cmd_size;

	return 0;
}

/** Compose EM Insert message */
static int compose_mpc_em_insert_msg(u8 *cmd_buff, u32 *cmd_buff_len,
				     struct cfa_mpc_em_op_params *parms)
{
	struct cfa_mpc_em_insert_params *e = &parms->insert;
	u8 *cmd;
	u32 cmd_size = 0;

	cmd_size = sizeof(struct mpc_header) + TFC_MPC_CMD_EM_INSERT_SIZE +
		   e->data_size * MPC_CFA_CACHE_ACCESS_UNIT_SIZE;

	if (e->data_size < 1 || e->data_size > 4) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (*cmd_buff_len < cmd_size) {
		netdev_dbg(NULL, "%s: invalid parameter: cmd_buff_len too small\n", __func__);
		ASSERT_RTNL();
		return -EINVAL;
	}

	if (!e->em_entry) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	cmd = (cmd_buff + sizeof(struct mpc_header));

	/* Populate CFA MPC command header */
	memset(cmd, 0, TFC_MPC_CMD_EM_INSERT_SIZE);
	TFC_MPC_CMD_EM_INSERT_SET_OPCODE(cmd, TFC_MPC_CMD_OPCODE_EM_INSERT);
	TFC_MPC_CMD_EM_INSERT_SET_WRITE_THROUGH(cmd, 1);
	TFC_MPC_CMD_EM_INSERT_SET_TABLE_SCOPE(cmd, parms->tbl_scope);
	TFC_MPC_CMD_EM_INSERT_SET_DATA_SIZE(cmd, e->data_size);
	TFC_MPC_CMD_EM_INSERT_SET_REPLACE(cmd, e->replace);
	TFC_MPC_CMD_EM_INSERT_SET_TABLE_INDEX(cmd, e->entry_idx);
	TFC_MPC_CMD_EM_INSERT_SET_TABLE_INDEX2(cmd, e->bucket_idx);
	/* Default to normal read cache option for EM insert */
	TFC_MPC_CMD_EM_INSERT_SET_CACHE_OPTION(cmd, CACHE_READ_OPTION_NORMAL);
	/* Default to write through cache write option for EM insert */
	TFC_MPC_CMD_EM_INSERT_SET_CACHE_OPTION2(cmd, CACHE_WRITE_OPTION_WRITE_THRU);

	/* Populate CFA MPC command payload following the header */
	memcpy(cmd + TFC_MPC_CMD_EM_INSERT_SIZE, e->em_entry,
	       e->data_size * MPC_CFA_CACHE_ACCESS_UNIT_SIZE);

	*cmd_buff_len = cmd_size;

	return 0;
}

/** Compose EM Delete message */
static int compose_mpc_em_delete_msg(u8 *cmd_buff, u32 *cmd_buff_len,
				     struct cfa_mpc_em_op_params *parms)
{
	u32 cmd_size = sizeof(struct mpc_header) + TFC_MPC_CMD_EM_DELETE_SIZE;
	struct cfa_mpc_em_delete_params *e = &parms->del;
	u8 *cmd;

	if (*cmd_buff_len < cmd_size) {
		netdev_dbg(NULL, "%s: invalid parameter: cmd_buff_len too small\n", __func__);
		ASSERT_RTNL();
		return -EINVAL;
	}

	/* Populate CFA MPC command header */
	cmd = cmd_buff + sizeof(struct mpc_header);
	memset(cmd, 0, TFC_MPC_CMD_EM_DELETE_SIZE);
	TFC_MPC_CMD_EM_DELETE_SET_OPCODE(cmd, TFC_MPC_CMD_OPCODE_EM_DELETE);
	TFC_MPC_CMD_EM_DELETE_SET_TABLE_SCOPE(cmd, parms->tbl_scope);
	TFC_MPC_CMD_EM_DELETE_SET_TABLE_INDEX(cmd, e->entry_idx);
	TFC_MPC_CMD_EM_DELETE_SET_TABLE_INDEX2(cmd, e->bucket_idx);
	/* Default to normal read cache option for EM delete */
	TFC_MPC_CMD_EM_DELETE_SET_CACHE_OPTION(cmd, CACHE_READ_OPTION_NORMAL);
	/* Default to write through cache write option for EM delete */
	TFC_MPC_CMD_EM_DELETE_SET_CACHE_OPTION2(cmd, CACHE_WRITE_OPTION_WRITE_THRU);

	*cmd_buff_len = cmd_size;

	return 0;
}

/** Compose EM Chain message */
static int compose_mpc_em_chain_msg(u8 *cmd_buff, u32 *cmd_buff_len,
				    struct cfa_mpc_em_op_params *parms)
{
	u32 cmd_size = sizeof(struct mpc_header) + TFC_MPC_CMD_EM_MATCH_CHAIN_SIZE;
	struct cfa_mpc_em_chain_params *e = &parms->chain;
	u8 *cmd;

	if (*cmd_buff_len < cmd_size) {
		netdev_dbg(NULL, "%s: invalid parameter: cmd_buff_len too small\n", __func__);
		ASSERT_RTNL();
		return -EINVAL;
	}

	/* Populate CFA MPC command header */
	cmd = cmd_buff + TFC_MPC_CMD_EM_MATCH_CHAIN_SIZE;
	memset(cmd, 0, TFC_MPC_CMD_EM_MATCH_CHAIN_SIZE);
	TFC_MPC_CMD_EM_MATCH_CHAIN_SET_OPCODE(cmd, TFC_MPC_CMD_OPCODE_EM_CHAIN);
	TFC_MPC_CMD_EM_MATCH_CHAIN_SET_TABLE_SCOPE(cmd, parms->tbl_scope);
	TFC_MPC_CMD_EM_MATCH_CHAIN_SET_TABLE_INDEX(cmd, e->entry_idx);
	TFC_MPC_CMD_EM_MATCH_CHAIN_SET_TABLE_INDEX2(cmd, e->bucket_idx);
	/* Default to normal read cache option for EM delete */
	TFC_MPC_CMD_EM_MATCH_CHAIN_SET_CACHE_OPTION(cmd, CACHE_READ_OPTION_NORMAL);
	/* Default to write through cache write option for EM delete */
	TFC_MPC_CMD_EM_MATCH_CHAIN_SET_CACHE_OPTION2(cmd, CACHE_WRITE_OPTION_WRITE_THRU);

	*cmd_buff_len = cmd_size;

	return 0;
}

/**
 * Build MPC CFA EM operation command
 *
 * @param [in] opc MPC EM opcode
 *
 * @param [in] cmd_buff Command data buffer to write the command to
 *
 * @param [in/out] cmd_buff_len Pointer to command buffer size param
 *        Set by caller to indicate the input cmd_buff size.
 *        Set to the actual size of the command generated by the api.
 *
 * @param [in] parms Pointer to MPC cache access command parameters
 *
 * @return 0 on Success, negative errno on failure
 */
int cfa_mpc_build_em_op_cmd(enum cfa_mpc_opcode opc, u8 *cmd_buff, u32 *cmd_buff_len,
			    struct cfa_mpc_em_op_params *parms)
{
	int rc;

	if (!cmd_buff || !cmd_buff_len || *cmd_buff_len == 0 || !parms) {
		netdev_dbg(NULL, "%s: invalid parameter: cmd_buff_len too small\n", __func__);
		ASSERT_RTNL();
		return -EINVAL;
	}

	rc = fill_mpc_header(cmd_buff, *cmd_buff_len, parms->opaque);
	if (rc)
		return rc;

	switch (opc) {
	case CFA_MPC_EM_SEARCH:
		return compose_mpc_em_search_msg(cmd_buff, cmd_buff_len, parms);
	case CFA_MPC_EM_INSERT:
		return compose_mpc_em_insert_msg(cmd_buff, cmd_buff_len, parms);
	case CFA_MPC_EM_DELETE:
		return compose_mpc_em_delete_msg(cmd_buff, cmd_buff_len, parms);
	case CFA_MPC_EM_CHAIN:
		return compose_mpc_em_chain_msg(cmd_buff, cmd_buff_len, parms);
	default:
		ASSERT_RTNL();
		return -EOPNOTSUPP;
	}

	return 0;
}

/** Parse MPC read clear completion */
static int parse_mpc_read_clr_result(u8 *resp_buff, u32 resp_buff_len,
				     struct cfa_mpc_cache_axs_result *result)
{
	u8 *cmp;
	u32 resp_size, rd_size;
	u8 *rd_data;

	/* Minimum data size = 1 32B unit */
	rd_size = MPC_CFA_CACHE_ACCESS_UNIT_SIZE;
	resp_size = sizeof(struct mpc_header) +
		    TFC_MPC_TBL_RDCLR_CMPL_SIZE +
		    sizeof(struct mpc_cr_short_dma_data) + rd_size;
	cmp = resp_buff + sizeof(struct mpc_header);

	if (resp_buff_len < resp_size ||
	    result->data_len < rd_size ||
	    !result->rd_data) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID((int)TFC_MPC_TBL_RDCLR_CMPL_GET_MP_CLIENT(cmp));

	result->status = TFC_MPC_TBL_RDCLR_CMPL_GET_STATUS(cmp);
	result->error_data = TFC_MPC_TBL_RDCLR_CMPL_GET_HASH_MSB(cmp);
	result->opaque = TFC_MPC_TBL_RDCLR_CMPL_GET_OPAQUE(cmp);

	/* No data to copy if there was an error, return early */
	if (result->status != TFC_MPC_TBL_RDCLR_CMPL_STATUS_OK)
		return 0;

	/* Copy the read data - starting at the end of the completion header including dma data */
	rd_data = resp_buff + sizeof(struct mpc_header) +
		  TFC_MPC_TBL_RDCLR_CMPL_SIZE +
		  sizeof(struct mpc_cr_short_dma_data);

	memcpy(result->rd_data, rd_data, rd_size);

	return 0;
}

/** Parse MPC table read completion */
static int parse_mpc_read_result(u8 *resp_buff, u32 resp_buff_len,
				 struct cfa_mpc_cache_axs_result *result)
{
	u8 *cmp;
	u32 resp_size, rd_size;
	u8 *rd_data;

	/* Minimum data size = 1 32B unit */
	rd_size = MPC_CFA_CACHE_ACCESS_UNIT_SIZE;
	resp_size = sizeof(struct mpc_header) +
		    TFC_MPC_TBL_RD_CMPL_SIZE +
		    sizeof(struct mpc_cr_short_dma_data) + rd_size;
	cmp = (resp_buff + sizeof(struct mpc_header));

	if (resp_buff_len < resp_size ||
	    result->data_len < rd_size ||
	    !result->rd_data) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID((int)TFC_MPC_TBL_RD_CMPL_GET_MP_CLIENT(cmp));

	result->status = TFC_MPC_TBL_RD_CMPL_GET_STATUS(cmp);
	result->error_data = TFC_MPC_TBL_RD_CMPL_GET_HASH_MSB(cmp);
	result->opaque = TFC_MPC_TBL_RD_CMPL_GET_OPAQUE(cmp);

	/* No data to copy if there was an error, return early */
	if (result->status != TFC_MPC_TBL_RD_CMPL_STATUS_OK)
		return 0;

	/* Copy max of 4 32B words that can fit into the return buffer */
	rd_size = MIN(4 * MPC_CFA_CACHE_ACCESS_UNIT_SIZE, result->data_len);

	/* Copy the read data - starting at the end of the completion header */
	rd_data = resp_buff + sizeof(struct mpc_header) +
		  TFC_MPC_TBL_RD_CMPL_SIZE +
		  sizeof(struct mpc_cr_short_dma_data);

	memcpy(result->rd_data, rd_data, rd_size);

	return 0;
}

/** Parse MPC table write completion */
static int parse_mpc_write_result(u8 *resp_buff, u32 resp_buff_len,
				  struct cfa_mpc_cache_axs_result *result)
{
	u32 resp_size;
	u8 *cmp;

	resp_size = sizeof(struct mpc_header) + TFC_MPC_TBL_WR_CMPL_SIZE;
	cmp = (resp_buff + sizeof(struct mpc_header));

	if (resp_buff_len < resp_size) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID((int)TFC_MPC_TBL_WR_CMPL_GET_MP_CLIENT(cmp));

	result->status = TFC_MPC_TBL_WR_CMPL_GET_STATUS(cmp);
	result->error_data = TFC_MPC_TBL_WR_CMPL_GET_HASH_MSB(cmp);
	result->opaque = TFC_MPC_TBL_WR_CMPL_GET_OPAQUE(cmp);

	return 0;
}

/** Parse MPC table evict completion */
static int parse_mpc_evict_result(u8 *resp_buff, u32 resp_buff_len,
				  struct cfa_mpc_cache_axs_result *result)
{
	u8 *cmp;
	u32 resp_size;

	resp_size = sizeof(struct mpc_header) +
		    TFC_MPC_TBL_INV_CMPL_SIZE;
	cmp = resp_buff + sizeof(struct mpc_header);

	if (resp_buff_len < resp_size) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID((int)TFC_MPC_TBL_INV_CMPL_GET_MP_CLIENT(cmp));

	result->status = TFC_MPC_TBL_INV_CMPL_GET_STATUS(cmp);
	result->error_data = TFC_MPC_TBL_INV_CMPL_GET_HASH_MSB(cmp);
	result->opaque = TFC_MPC_TBL_INV_CMPL_GET_OPAQUE(cmp);

	return 0;
}

/**
 * Parse MPC CFA Cache access command completion result
 *
 * @param [in] opc MPC cache access opcode
 *
 * @param [in] resp_buff Data buffer containing the response to parse
 *
 * @param [in] resp_buff_len Response buffer size
 *
 * @param [out] result Pointer to MPC cache access result object. This
 *        object will contain the fields parsed and extracted from the
 *        response buffer.
 *
 * @return 0 on Success, negative errno on failure
 */
int cfa_mpc_parse_cache_axs_resp(enum cfa_mpc_opcode opc, u8 *resp_buff,
				 u32 resp_buff_len,
				 struct cfa_mpc_cache_axs_result *result)
{
	if (!resp_buff || resp_buff_len == 0 || !result) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	switch (opc) {
	case CFA_MPC_READ_CLR:
		return parse_mpc_read_clr_result(resp_buff, resp_buff_len,
						 result);
	case CFA_MPC_READ:
		return parse_mpc_read_result(resp_buff, resp_buff_len, result);
	case CFA_MPC_WRITE:
		return parse_mpc_write_result(resp_buff, resp_buff_len, result);
	case CFA_MPC_INVALIDATE:
		return parse_mpc_evict_result(resp_buff, resp_buff_len, result);
	default:
		ASSERT_RTNL();
		return -EOPNOTSUPP;
	}
}

/** Parse MPC EM Search completion */
static int parse_mpc_em_search_result(u8 *resp_buff,
				      u32 resp_buff_len,
				      struct cfa_mpc_em_op_result *result)
{
	u8 *cmp;
	u32 resp_size;

	cmp = resp_buff + sizeof(struct mpc_header);
	resp_size = sizeof(struct mpc_header) +
		    TFC_MPC_TBL_EM_SEARCH_CMPL_SIZE;

	if (resp_buff_len < resp_size) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID((int)TFC_MPC_TBL_EM_SEARCH_CMPL_GET_MP_CLIENT(cmp));

	result->status = TFC_MPC_TBL_EM_SEARCH_CMPL_GET_STATUS(cmp);
	result->error_data = result->status != CFA_MPC_OK ?
					       TFC_MPC_TBL_EM_SEARCH_CMPL_GET_HASH_MSB(cmp) : 0;
	result->opaque = TFC_MPC_TBL_EM_SEARCH_CMPL_GET_OPAQUE(cmp);
	result->search.bucket_num = TFC_MPC_TBL_EM_SEARCH_CMPL_GET_BKT_NUM(cmp);
	result->search.num_entries = TFC_MPC_TBL_EM_SEARCH_CMPL_GET_NUM_ENTRIES(cmp);
	result->search.hash_msb = TFC_MPC_TBL_EM_SEARCH_CMPL_GET_HASH_MSB(cmp);
	result->search.match_idx = TFC_MPC_TBL_EM_SEARCH_CMPL_GET_TABLE_INDEX(cmp);
	result->search.bucket_idx = TFC_MPC_TBL_EM_SEARCH_CMPL_GET_TABLE_INDEX2(cmp);

	return 0;
}

/** Parse MPC EM Insert completion */
static int parse_mpc_em_insert_result(u8 *resp_buff,
				      u32 resp_buff_len,
				      struct cfa_mpc_em_op_result *result)
{
	u8 *cmp;
	u32 resp_size;

	cmp = resp_buff + sizeof(struct mpc_header);
	resp_size = sizeof(struct mpc_header) + TFC_MPC_TBL_EM_INSERT_CMPL_SIZE;

	if (resp_buff_len < resp_size) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID((int)TFC_MPC_TBL_EM_INSERT_CMPL_GET_MP_CLIENT(cmp));

	result->status = TFC_MPC_TBL_EM_INSERT_CMPL_GET_STATUS(cmp);
	result->error_data = (result->status != TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_OK) ?
			     (u32)TFC_MPC_TBL_EM_INSERT_CMPL_GET_HASH_MSB(cmp) : 0UL;
	result->opaque = TFC_MPC_TBL_EM_INSERT_CMPL_GET_OPAQUE(cmp);
	result->insert.bucket_num = TFC_MPC_TBL_EM_INSERT_CMPL_GET_BKT_NUM(cmp);
	result->insert.num_entries = TFC_MPC_TBL_EM_INSERT_CMPL_GET_NUM_ENTRIES(cmp);
	result->insert.hash_msb = TFC_MPC_TBL_EM_DELETE_CMPL_GET_HASH_MSB(cmp);
	result->insert.match_idx = TFC_MPC_TBL_EM_INSERT_CMPL_GET_TABLE_INDEX4(cmp);
	result->insert.bucket_idx = TFC_MPC_TBL_EM_INSERT_CMPL_GET_TABLE_INDEX3(cmp);
	result->insert.replaced = TFC_MPC_TBL_EM_INSERT_CMPL_GET_REPLACED_ENTRY(cmp);
	result->insert.chain_update = TFC_MPC_TBL_EM_INSERT_CMPL_GET_CHAIN_UPD(cmp);

	return 0;
}

/** Parse MPC EM Delete completion */
static int parse_mpc_em_delete_result(u8 *resp_buff,
				      u32 resp_buff_len,
				      struct cfa_mpc_em_op_result *result)
{
	u8 *cmp;
	u32 resp_size;

	cmp = resp_buff + sizeof(struct mpc_header);
	resp_size = sizeof(struct mpc_header) +
		    TFC_MPC_TBL_EM_DELETE_CMPL_SIZE;

	if (resp_buff_len < resp_size) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID((int)TFC_MPC_TBL_EM_DELETE_CMPL_GET_MP_CLIENT(cmp));

	result->status = TFC_MPC_TBL_EM_DELETE_CMPL_GET_STATUS(cmp);
	result->error_data = TFC_MPC_TBL_EM_DELETE_CMPL_GET_HASH_MSB(cmp);
	result->opaque = TFC_MPC_TBL_EM_DELETE_CMPL_GET_OPAQUE(cmp);
	result->del.bucket_num = TFC_MPC_TBL_EM_DELETE_CMPL_GET_BKT_NUM(cmp);
	result->del.num_entries = TFC_MPC_TBL_EM_DELETE_CMPL_GET_NUM_ENTRIES(cmp);
	result->del.prev_tail = TFC_MPC_TBL_EM_DELETE_CMPL_GET_TABLE_INDEX3(cmp);
	result->del.new_tail = TFC_MPC_TBL_EM_DELETE_CMPL_GET_TABLE_INDEX4(cmp);
	result->del.chain_update = TFC_MPC_TBL_EM_DELETE_CMPL_GET_CHAIN_UPD(cmp);

	return 0;
}

/** Parse MPC EM Chain completion */
static int parse_mpc_em_chain_result(u8 *resp_buff, u32 resp_buff_len,
				     struct cfa_mpc_em_op_result *result)
{
	u8 *cmp;
	u32 resp_size;

	cmp = resp_buff + sizeof(struct mpc_header);
	resp_size =
		sizeof(struct mpc_header) + TFC_MPC_TBL_EM_CHAIN_CMPL_SIZE;

	if (resp_buff_len < resp_size) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	ASSERT_CFA_MPC_CLIENT_ID((int)TFC_MPC_TBL_EM_CHAIN_CMPL_GET_MP_CLIENT(cmp));

	result->status = TFC_MPC_TBL_EM_CHAIN_CMPL_GET_STATUS(cmp);
	result->error_data = TFC_MPC_TBL_EM_CHAIN_CMPL_GET_HASH_MSB(cmp);
	result->opaque = TFC_MPC_TBL_EM_CHAIN_CMPL_GET_OPAQUE(cmp);
	result->chain.bucket_num = TFC_MPC_TBL_EM_CHAIN_CMPL_GET_BKT_NUM(cmp);
	result->chain.num_entries = TFC_MPC_TBL_EM_CHAIN_CMPL_GET_NUM_ENTRIES(cmp);

	return 0;
}

/**
 * Parse MPC CFA EM operation command completion result
 *
 * @param [in] opc MPC cache access opcode
 *
 * @param [in] resp_buff Data buffer containing the response to parse
 *
 * @param [in] resp_buff_len Response buffer size
 *
 * @param [out] result Pointer to MPC EM operation result object. This
 *        object will contain the fields parsed and extracted from the
 *        response buffer.
 *
 * @return 0 on Success, negative errno on failure
 */
int cfa_mpc_parse_em_op_resp(enum cfa_mpc_opcode opc, u8 *resp_buff,
			     u32 resp_buff_len,
			     struct cfa_mpc_em_op_result *result)
{
	if (!resp_buff || resp_buff_len == 0 || !result) {
		ASSERT_RTNL();
		return -EINVAL;
	}

	switch (opc) {
	case CFA_MPC_EM_SEARCH:
		return parse_mpc_em_search_result(resp_buff, resp_buff_len,
						  result);
	case CFA_MPC_EM_INSERT:
		return parse_mpc_em_insert_result(resp_buff, resp_buff_len,
						  result);
	case CFA_MPC_EM_DELETE:
		return parse_mpc_em_delete_result(resp_buff, resp_buff_len,
						  result);
	case CFA_MPC_EM_CHAIN:
		return parse_mpc_em_chain_result(resp_buff, resp_buff_len,
						 result);
	default:
		ASSERT_RTNL();
		return -EOPNOTSUPP;
	}
}
