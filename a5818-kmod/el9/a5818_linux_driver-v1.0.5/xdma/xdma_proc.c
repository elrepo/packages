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

// Partially inspired from https://stackoverflow.com/a/66926881/3287591

#include "xdma_proc.h"

#include <linux/device.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>

#include "version.h"
#include "a5818_regs.h"

static int xdma_proc_show(struct seq_file *m, void *v) {
    
    struct xdma_pci_dev* const xpdev = m->private;
    struct xdma_dev* const xdev = xpdev->xdev;
    char* const user_offset = xdev->bar[xdev->user_bar_idx];
    unsigned int data;
    unsigned int maj, min, build;
    unsigned int die_temp_raw, board_temp_raw;
    long long die_temp_ll, board_temp_ll; // long long (at least 64 bits) to avoid int overflow on conversion

    data = ioread32(user_offset + C_FW_REVISION_OFFSET);
    min = data & 0xFF;
    maj = (data >> 8) & 0xFF;
    data = ioread32(user_offset + C_FW_BUILD_OFFSET);
    build = data;
    data = ioread32(user_offset + DSTAT_OFFSET);
    die_temp_raw = (data >> 20) & 0xFFF;
    board_temp_raw = (data >> 8) & 0xFFF;

    // do conversion in fixed point arithmetics to avoid SSE issues

    // die_temp = die_temp_raw * 501.3743 * 0x1p-12 - 273.6777;
    die_temp_ll = (((die_temp_raw * 5013743LL) >> 12) - 2736777LL) / 10000LL;

    // board_temp = board_temp_raw * 0x1p-4;
    board_temp_ll = board_temp_raw >> 4;

    seq_printf(m, "CAEN A5818 Driver "A5818_DRV_MODULE_VERSION"\n");
    seq_printf(m, "Firmware revision: %u.%u (build %08X)\n", maj, min, build);
    seq_printf(m, "Die temperature: %lld degC\n", die_temp_ll);
    seq_printf(m, "Board temperature: %lld degC\n", board_temp_ll);

    return 0;
}

static int xdma_proc_open(struct inode *inode, struct file *file) {
#ifdef RHEL_RELEASE_VERSION
#if RHEL_RELEASE_VERSION(9, 0) > RHEL_RELEASE_CODE
    return single_open(file, xdma_proc_show, PDE_DATA(inode));
#else
    return single_open(file, xdma_proc_show, pde_data(inode));
#endif
#else
#if KERNEL_VERSION(5, 17, 0) > LINUX_VERSION_CODE
    return single_open(file, xdma_proc_show, PDE_DATA(inode));
#else
    return single_open(file, xdma_proc_show, pde_data(inode));
#endif
#endif
}

#if  LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static struct proc_ops xdma_procdir_fops = {
    .proc_open = xdma_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#else
static struct file_operations xdma_procdir_fops = {
    .owner = THIS_MODULE,
    .open = xdma_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif

struct proc_dir_entry* xdma_register_proc(struct xdma_pci_dev* xpdev) {
    // use control cdev just for simplicity and because always available
    const char* const name = dev_name(xpdev->ctrl_cdev.sys_device);
	return proc_create_data(name, 0666, NULL, &xdma_procdir_fops, xpdev);
}

void xdma_unregister_proc(struct proc_dir_entry* proc_dir) {
    proc_remove(proc_dir);
}
