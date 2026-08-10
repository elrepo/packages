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
 *
 * Return
 *   fid
 */
u16 tfc_get_fid(struct tfc *tfcp);

/**
 * Is this DPDK port/function a PF?
 *
 * @tfcp: Pointer to TFC handle
 *
 * Return
 *   true if pf, false if vf
 */
bool tfc_bp_is_pf(struct tfc *tfcp);

/**
 * Get the maximum VF for the PF
 *
 * @tfcp: Pointer to TFC handle
 *
 * Return
 * max_vfs
 */
u16 tfc_bp_vf_max(struct tfc *tfcp);

#endif  /* _TFC_PRIV_H_ */
