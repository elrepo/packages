/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

/*!
 *   \file
 *   \brief Exported functions for CFA HW programming
 */
#ifndef _HCAPI_CFA_H_
#define _HCAPI_CFA_H_

#include <linux/types.h>
#include "hcapi_cfa_defs.h"

struct hcapi_cfa_devops;

/**
 * CFA device information
 */
struct hcapi_cfa_devinfo {
	/** [out] CFA hw fix formatted layouts */
	const struct hcapi_cfa_layout_tbl *layouts;
	/** [out] CFA device ops function pointer table */
	const struct hcapi_cfa_devops *devops;
};

/**
 *  \defgroup CFA_HCAPI_DEVICE_API
 *  HCAPI used for writing to the hardware
 *  @{
 */

/** CFA device specific function hooks structure
 *
 * The following device hooks can be defined; unless noted otherwise, they are
 * optional and can be filled with a null pointer. The pupose of these hooks
 * to support CFA device operations for different device variants.
 */
struct hcapi_cfa_devops {
	/** calculate a key hash for the provided key_data
	 *
	 * This API computes hash for a key.
	 *
	 * @param[in] key_data
	 *   A pointer of the key data buffer
	 *
	 * @param[in] bitlen
	 *   Number of bits of the key data
	 *
	 * @return
	 *   0 for SUCCESS, negative value for FAILURE
	 */
	u64 (*hcapi_cfa_key_hash)(u8 *key_data, u16 bitlen);
};

/*@}*/

extern const size_t CFA_RM_HANDLE_DATA_SIZE;

#if SUPPORT_CFA_HW_ALL
extern const struct hcapi_cfa_devops cfa_p4_devops;
extern const struct hcapi_cfa_devops cfa_p58_devops;
extern const struct hcapi_cfa_devops cfa_p59_devops;
extern const struct hcapi_cfa_layout_tbl cfa_p59_layout_tbl;

u64 hcapi_cfa_p59_key_hash(u64 *key_data, u16 bitlen);
#elif defined(SUPPORT_CFA_HW_P4) && SUPPORT_CFA_HW_P4
extern const struct hcapi_cfa_devops cfa_p4_devops;
u64 hcapi_cfa_p4_key_hash(u64 *key_data, u16 bitlen);
/* SUPPORT_CFA_HW_P4 */
#elif SUPPORT_CFA_HW_P45
/* Firmware function defines */
/* SUPPORT_CFA_HW_P45 */
#elif defined(SUPPORT_CFA_HW_P58) && SUPPORT_CFA_HW_P58
extern const struct hcapi_cfa_devops cfa_p58_devops;
u64 hcapi_cfa_p58_key_hash(u64 *key_data, u16 bitlen);
/* SUPPORT_CFA_HW_P58 */
#elif defined(SUPPORT_CFA_HW_P59) && SUPPORT_CFA_HW_P59
extern const struct hcapi_cfa_devops cfa_p59_devops;
extern const struct hcapi_cfa_layout_tbl cfa_p59_layout_tbl;
u64 hcapi_cfa_p59_key_hash(u64 *key_data, u16 bitlen);
#ifdef CFA_HW_SUPPORT_HOST_IF
#else
#endif
/* SUPPORT_CFA_HW_P59 */
#endif

#endif /* HCAPI_CFA_H_ */
