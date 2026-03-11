/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_PRIV_H_
#define _TFC_PRIV_H_

#include "tfc.h"

/**
 * Get the FID for this DPDK port/function.
 *
 * @tfcp: Pointer to TFC handle
 * @fw_fid: The function ID
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_get_fid(struct tfc *tfcp, u16 *fw_fid);

/**
 * Get the PFID for this DPDK port/function.
 *
 * @tfcp: Pointer to TFC handle
 * @pfid: The Physical Function ID for this port/function
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_get_pfid(struct tfc *tfcp, u16 *pfid);

/**
 * Is this DPDK port/function a PF?
 *
 * @tfcp: Pointer to TFC handle
 * @is_pf: If true, the DPDK port is a PF (as opposed to a VF)
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_bp_is_pf(struct tfc *tfcp, bool *is_pf);

/**
 * Get the maximum VF for the PF
 *
 * @tfcp: Pointer to TFC handle
 * @max_vf: The maximum VF for the PF (only valid on a PF)
 *
 * Return
 *   0 for SUCCESS, negative error value for FAILURE (errno.h)
 */
int tfc_bp_vf_max(struct tfc *tfcp, u16 *max_vf);

#endif  /* _TFC_PRIV_H_ */
