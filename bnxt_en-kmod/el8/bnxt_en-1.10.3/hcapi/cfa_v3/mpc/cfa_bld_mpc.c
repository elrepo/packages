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
#include "cfa_bld_p70_mpcops.h"

int cfa_bld_mpc_bind(enum cfa_ver hw_ver, struct cfa_bld_mpcinfo *mpcinfo)
{
	if (!mpcinfo)
		return -EINVAL;

	switch (hw_ver) {
	case CFA_P40:
	case CFA_P45:
	case CFA_P58:
	case CFA_P59:
		return -ENOTSUPP;
	case CFA_P70:
		return cfa_bld_p70_mpc_bind(hw_ver, mpcinfo);
	default:
		return -EINVAL;
	}
}
