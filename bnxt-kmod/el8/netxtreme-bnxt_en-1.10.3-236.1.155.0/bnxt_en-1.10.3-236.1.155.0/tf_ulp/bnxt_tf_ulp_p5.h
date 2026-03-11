/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_ULP_TF_H_
#define _BNXT_ULP_TF_H_

#include <linux/stddef.h>
#include "bnxt.h"

#include "tf_core.h"
#include "ulp_template_db_enum.h"
#include "bnxt_tf_common.h"

struct tf *
bnxt_ulp_bp_tfp_get(struct bnxt *bp, enum bnxt_ulp_session_type type);

struct tf *
bnxt_get_tfp_session(struct bnxt *bp, enum bnxt_session_type type);

/* Function to get the tfp session details from ulp context. */
void *
bnxt_tf_ulp_cntxt_tfp_get(struct bnxt_ulp_context *ulp, enum bnxt_ulp_session_type s_type);
#endif
