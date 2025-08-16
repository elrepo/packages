/*
 * This file is part of the CAEN A5818 Driver for Linux
 *
 * Copyright (c) 2023-present,  CAEN SpA
 * All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#ifndef __XDMA_PROC_H__
#define __XDMA_PROC_H__

#include <linux/proc_fs.h>

#include "xdma_mod.h"

struct proc_dir_entry* xdma_register_proc(struct xdma_pci_dev* xpdev);
void xdma_unregister_proc(struct proc_dir_entry* proc_dir);

#endif /* #ifndef __XDMA_PROC_H__ */
