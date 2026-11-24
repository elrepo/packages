/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_TF_DEBUG_H_
#define _ULP_TF_DEBUG_H_

#include "bnxt_tf_ulp.h"

struct tf;
struct ulp_interface_info;
struct bnxt_ulp_port_db;

const char *tf_if_tbl_2_str(u32 id_type);
void ulp_port_db_dump(struct bnxt_ulp_context *ulp_ctx,
		      struct bnxt_ulp_port_db *port_db,
		      struct ulp_interface_info *intf,
		      u32 port_id);

#endif
