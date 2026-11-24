/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2023-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BNXT_SRIOV_SYSFS_H
#define BNXT_SRIOV_SYSFS_H

#include "bnxt_hsi.h"
#include "bnxt.h"

int bnxt_sriov_sysfs_init(struct bnxt *bp);
void bnxt_sriov_sysfs_exit(struct bnxt *bp);
int bnxt_create_vfs_sysfs(struct bnxt *bp);
void bnxt_destroy_vfs_sysfs(struct bnxt *bp);
#endif
