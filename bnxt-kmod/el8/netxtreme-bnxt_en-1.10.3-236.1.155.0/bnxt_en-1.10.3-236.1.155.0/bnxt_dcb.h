/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2014-2016 Broadcom Corporation
 * Copyright (c) 2016-2018 Broadcom Limited
 * Copyright (c) 2018-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BNXT_DCB_H
#define BNXT_DCB_H

#include <net/dcbnl.h>

struct bnxt_dcb {
	u8			max_tc;
	struct ieee_pfc		*ieee_pfc;
	struct ieee_ets		*ieee_ets;
	u8			dcbx_cap;
	u8			default_pri;
};

struct bnxt_cos2bw_cfg {
	u8			pad[3];
	struct_group_attr(cfg, __packed,
		u8			queue_id;
		__le32			min_bw;
		__le32			max_bw;
		u8			tsa;
		u8			pri_lvl;
		u8			bw_weight;
	);
/* for min_bw / max_bw */
#define BW_VALUE_UNIT_PERCENT1_100		(0x1UL << 29)
	u8			unused;
};

struct bnxt_dscp2pri_entry {
	u8	dscp;
	u8	mask;
	u8	pri;
};

#define BNXT_LLQ(q_profile)	\
	((q_profile) == QUEUE_QPORTCFG_RESP_QUEUE_ID0_SERVICE_PROFILE_LOSSLESS_ROCE ||	\
	 (q_profile) == QUEUE_QPORTCFG_RESP_QUEUE_ID0_SERVICE_PROFILE_LOSSLESS_NIC)
#define BNXT_CNPQ(q_profile)	\
	((q_profile) == QUEUE_QPORTCFG_RESP_QUEUE_ID0_SERVICE_PROFILE_LOSSY_ROCE_CNP)

#define HWRM_STRUCT_DATA_SUBTYPE_HOST_OPERATIONAL	0x0300

/* bnxt_queue_cos2bw_qcfg_output (size:896b/112B)
 * This structure is identical in memory layout to
 * struct hwrm_queue_cos2bw_qcfg_output in bnxt_hsi.h.
 * Using the structure prevents fortify memcpy warnings.
 */
struct bnxt_queue_cos2bw_qcfg_output {
	__le16	error_code;
	__le16	req_type;
	__le16	seq_id;
	__le16	resp_len;
	u8	queue_id0;
	u8	unused_0;
	__le16	unused_1;
	__le32	queue_id0_min_bw;
	__le32	queue_id0_max_bw;
	u8	queue_id0_tsa_assign;
	u8	queue_id0_pri_lvl;
	u8	queue_id0_bw_weight;
	struct {
		u8	queue_id;
		__le32	queue_id_min_bw;
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_BW_VALUE_MASK             0xfffffffUL
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_BW_VALUE_SFT              0
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_SCALE                     0x10000000UL
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_SCALE_BITS                  (0x0UL << 28)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_SCALE_BYTES                 (0x1UL << 28)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_SCALE_LAST                 QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_SCALE_BYTES
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_MASK        0xe0000000UL
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_SFT         29
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_MEGA          (0x0UL << 29)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_KILO          (0x2UL << 29)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_BASE          (0x4UL << 29)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_GIGA          (0x6UL << 29)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_PERCENT1_100  (0x1UL << 29)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_INVALID       (0x7UL << 29)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_LAST         QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_INVALID
		__le32	queue_id_max_bw;
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_BW_VALUE_MASK             0xfffffffUL
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_BW_VALUE_SFT              0
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_SCALE                     0x10000000UL
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_SCALE_BITS                  (0x0UL << 28)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_SCALE_BYTES                 (0x1UL << 28)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_SCALE_LAST                 QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_SCALE_BYTES
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_MASK        0xe0000000UL
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_SFT         29
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_MEGA          (0x0UL << 29)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_KILO          (0x2UL << 29)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_BASE          (0x4UL << 29)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_GIGA          (0x6UL << 29)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_PERCENT1_100  (0x1UL << 29)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_INVALID       (0x7UL << 29)
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_LAST         QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_INVALID
		u8	queue_id_tsa_assign;
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_TSA_ASSIGN_SP             0x0UL
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_TSA_ASSIGN_ETS            0x1UL
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_TSA_ASSIGN_RESERVED_FIRST 0x2UL
	#define QUEUE_COS2BW_QCFG_RESP_QUEUE_ID_TSA_ASSIGN_RESERVED_LAST  0xffUL
		u8	queue_id_pri_lvl;
		u8	queue_id_bw_weight;
	} __packed cfg[7];
	u8	unused_2[4];
	u8	valid;
};

/* bnxt_queue_cos2bw_cfg_input (size:1024b/128B)
 * This structure is identical in memory layout to
 * struct hwrm_queue_cos2bw_cfg_input in bnxt_hsi.h.
 * Using the structure prevents fortify memcpy warnings.
 */
struct bnxt_queue_cos2bw_cfg_input {
	__le16	req_type;
	__le16	cmpl_ring;
	__le16	seq_id;
	__le16	target_id;
	__le64	resp_addr;
	__le32	flags;
	__le32	enables;
	__le16	port_id;
	u8	queue_id0;
	u8	unused_0;
	__le32	queue_id0_min_bw;
	__le32	queue_id0_max_bw;
	u8	queue_id0_tsa_assign;
	u8	queue_id0_pri_lvl;
	u8	queue_id0_bw_weight;
	struct {
		u8	queue_id;
		__le32	queue_id_min_bw;
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_BW_VALUE_MASK             0xfffffffUL
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_BW_VALUE_SFT              0
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_SCALE                     0x10000000UL
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_SCALE_BITS                  (0x0UL << 28)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_SCALE_BYTES                 (0x1UL << 28)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_SCALE_LAST                 QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_SCALE_BYTES
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_MASK        0xe0000000UL
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_SFT         29
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_MEGA          (0x0UL << 29)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_KILO          (0x2UL << 29)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_BASE          (0x4UL << 29)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_GIGA          (0x6UL << 29)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_PERCENT1_100  (0x1UL << 29)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_INVALID       (0x7UL << 29)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_LAST         QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MIN_BW_BW_VALUE_UNIT_INVALID
		__le32	queue_id_max_bw;
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_BW_VALUE_MASK             0xfffffffUL
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_BW_VALUE_SFT              0
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_SCALE                     0x10000000UL
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_SCALE_BITS                  (0x0UL << 28)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_SCALE_BYTES                 (0x1UL << 28)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_SCALE_LAST                 QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_SCALE_BYTES
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_MASK        0xe0000000UL
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_SFT         29
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_MEGA          (0x0UL << 29)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_KILO          (0x2UL << 29)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_BASE          (0x4UL << 29)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_GIGA          (0x6UL << 29)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_PERCENT1_100  (0x1UL << 29)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_INVALID       (0x7UL << 29)
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_LAST         QUEUE_COS2BW_CFG_REQ_QUEUE_ID_MAX_BW_BW_VALUE_UNIT_INVALID
		u8	queue_id_tsa_assign;
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_TSA_ASSIGN_SP             0x0UL
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_TSA_ASSIGN_ETS            0x1UL
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_TSA_ASSIGN_RESERVED_FIRST 0x2UL
	#define QUEUE_COS2BW_CFG_REQ_QUEUE_ID_TSA_ASSIGN_RESERVED_LAST  0xffUL
		u8	queue_id_pri_lvl;
		u8	queue_id_bw_weight;
	} __packed cfg[7];
	u8	unused_1[5];
};

void bnxt_dcb_init(struct bnxt *bp);
void bnxt_dcb_free(struct bnxt *bp, bool reset);
#endif
