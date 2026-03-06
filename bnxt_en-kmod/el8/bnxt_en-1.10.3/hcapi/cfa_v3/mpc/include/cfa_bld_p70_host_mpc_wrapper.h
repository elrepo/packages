/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_BLD_P70_HOST_MPC_WRAPPER_H_
#define _CFA_BLD_P70_HOST_MPC_WRAPPER_H_

#include "cfa_bld_mpcops.h"
/**
 * MPC Cache operation command build apis
 */
int cfa_bld_p70_mpc_build_cache_read(u8 *cmd, u32 *cmd_buff_len,
				     struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_build_cache_write(u8 *cmd, u32 *cmd_buff_len,
				      const u8 *data,
				      struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_build_cache_evict(u8 *cmd, u32 *cmd_buff_len,
				      struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_build_cache_rdclr(u8 *cmd, u32 *cmd_buff_len,
				      struct cfa_mpc_data_obj *fields);

/**
 * MPC EM operation command build apis
 */
int cfa_bld_p70_mpc_build_em_search(u8 *cmd, u32 *cmd_buff_len,
				    u8 *em_entry,
				    struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_build_em_insert(u8 *cmd, u32 *cmd_buff_len,
				    const u8 *em_entry,
				    struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_build_em_delete(u8 *cmd, u32 *cmd_buff_len,
				    struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_build_em_chain(u8 *cmd, u32 *cmd_buff_len,
				   struct cfa_mpc_data_obj *fields);

/**
 * MPC Cache operation completion parse apis
 */
int cfa_bld_p70_mpc_parse_cache_read(u8 *resp, u32 resp_buff_len,
				     u8 *rd_data, u32 rd_data_len,
				     struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_parse_cache_write(u8 *resp, u32 resp_buff_len,
				      struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_parse_cache_evict(u8 *resp, u32 resp_buff_len,
				      struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_parse_cache_rdclr(u8 *resp, u32 resp_buff_len,
				      u8 *rd_data, u32 rd_data_len,
				      struct cfa_mpc_data_obj *fields);

/**
 * MPC EM operation completion parse apis
 */
int cfa_bld_p70_mpc_parse_em_search(u8 *resp, u32 resp_buff_len,
				    struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_parse_em_insert(u8 *resp, u32 resp_buff_len,
				    struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_parse_em_delete(u8 *resp, u32 resp_buff_len,
				    struct cfa_mpc_data_obj *fields);

int cfa_bld_p70_mpc_parse_em_chain(u8 *resp, u32 resp_buff_len,
				   struct cfa_mpc_data_obj *fields);

#endif /* _CFA_BLD_P70_HOST_MPC_WRAPPER_H_ */
