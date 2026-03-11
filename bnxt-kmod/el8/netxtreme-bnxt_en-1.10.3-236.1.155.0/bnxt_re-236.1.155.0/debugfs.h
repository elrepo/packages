/*
 * Copyright (c) 2015-2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: DebugFS header
 */

#ifndef __BNXT_RE_DEBUGFS__
#define __BNXT_RE_DEBUGFS__

#define BNXT_RE_DEBUGFS_QP_INFO_MAX_SIZE	512

extern struct list_head bnxt_re_dev_list;

void bnxt_re_debugfs_init(void);
void bnxt_re_debugfs_remove(void);
void bnxt_re_debugfs_create_udcc_session(struct bnxt_re_dev *rdev, u32 session_id);
void bnxt_re_debugfs_delete_udcc_session(struct bnxt_re_dev *rdev, u32 session_id);

static inline bool bnxt_re_udcc_rtt_supported(struct bnxt_re_udcc_cfg *udcc)
{
	return ((udcc->flags & UDCC_QCAPS_RESP_FLAGS_RTT_DISTRIBUTION_SUPPORTED) ? true : false);
}

static inline char *bnxt_udcc_session_type_str(u8 type)
{
	switch (type) {
	case UDCC_QCAPS_RESP_SESSION_TYPE_PER_DESTINATION:
		return "Per Destination";
	case UDCC_QCAPS_RESP_SESSION_TYPE_PER_QP:
		return "Per QP";
	default:
		return "Invalid";
	}
}

enum bnxt_re_cq_coal_vals {
	BNXT_RE_BUF_MAXTIME,
	BNXT_RE_NORMAL_MAXBUF,
	BNXT_RE_DURING_MAXBUF,
	BNXT_RE_EN_RING_IDLE_MODE,
	BNXT_RE_CQ_COAL_ENABLE,
	BNXT_RE_CQ_COAL_MAX
};

struct bnxt_re_cq_coal_param {
	struct bnxt_re_dev *rdev;
	struct dentry *dentry;
	u32 offset;
};

struct bnxt_re_dbg_cq_coal_cfg_params {
	struct bnxt_re_cq_coal_param  dbgfs_attr[BNXT_RE_CQ_COAL_MAX];
};

#define CC_CONFIG_GEN_EXT(x, y)	(((x) << 16) | (y))
#define CC_CONFIG_GEN0_EXT0  CC_CONFIG_GEN_EXT(0, 0)
#define CC_CONFIG_GEN1_EXT0  CC_CONFIG_GEN_EXT(1, 0)
#define CC_CONFIG_GEN1_EXT1  CC_CONFIG_GEN_EXT(1, 1)

#define BNXT_RE_CC_PARAM_GEN0	14 /* TODO convert it to a variable*/
#define BNXT_RE_CC_PARAM_GEN1	44
#define BNXT_RE_CC_PARAM_GEN1_EXT1	7

struct bnxt_re_cc_param {
	struct bnxt_re_dev *rdev;
	struct dentry *dentry;
	u32 offset;
	u32 cc_gen;
};

struct bnxt_re_dbg_cc_config_params {
	struct bnxt_re_cc_param        gen0_params[BNXT_RE_CC_PARAM_GEN0];
	struct bnxt_re_cc_param        gen1_params[BNXT_RE_CC_PARAM_GEN1];
	struct bnxt_re_cc_param        gen1_ext1_params[BNXT_RE_CC_PARAM_GEN1_EXT1];
};
#endif
