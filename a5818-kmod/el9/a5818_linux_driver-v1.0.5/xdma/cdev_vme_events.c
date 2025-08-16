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

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s: " fmt, __func__

#include "xdma_cdev.h"

struct ioctl_irq_wait_data {
	u32 mask;
	u32 timeout;
};

static long ioctl_wait(struct xdma_cdev *xcdev, unsigned long arg)
{
	int rv = 0;
	struct xdma_vme_user_irq * vme_user_irq;

	struct ioctl_irq_wait_data data;

	vme_user_irq = xcdev->vme_user_irq;
	if (!vme_user_irq) {
		pr_info("xcdev 0x%p, vme_user_irq NULL.\n", xcdev);
		return -EINVAL;
	}

	rv = copy_from_user(&data, (struct ioctl_irq_wait_data __user *)arg, sizeof(data));
	if (rv < 0) {
		dbg_perf("Failed to copy from user space 0x%lx\n", arg);
		return -EINVAL;
	}

	/*
     * The usage of mb here and on vme_user_irq_service, with
     * swapped order of R/W access to user_mask and events_irq is to prevent
     * the case where here we read events_irq == 0, and on vme_user_irq_service
     * events_irq is set few instants later but no signal is woken because
     * vme_user_irq_service read user_mask == 0.
     *
     * A more intuitive but probably slower alternative would require
     * the usage of spin locks on this critical section.
     */

	// 1. set user_mask, to be checked in HandleVmeUserEvent
	vme_user_irq->user_mask = data.mask;

	// 2. ensure user_mask is set before checking and resetting last_irq
    mb();

	// 3. check if already signalled and, in case, sleep
	rv = wait_event_interruptible_timeout(vme_user_irq->events_wq,
			(atomic_xchg(&vme_user_irq->last_irq, 0) & data.mask) != 0, msecs_to_jiffies(data.timeout));

	switch (rv) {
	case -ERESTARTSYS:
		return -ERESTARTSYS;
	case 0:
		rv = -ETIMEDOUT;
		break;
	default:
		rv = 0;
	}

	// 4. ensure user_mask is set at the end
    mb();

	// 5. clear user_mask
	vme_user_irq->user_mask = 0;

	return rv;
}

#define IOCTL_XDMA_VME_WAIT 	_IOR('q', 0, struct ioctl_irq_wait_data)

static long char_events_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct xdma_cdev *xcdev = (struct xdma_cdev *)file->private_data;

	int rv = 0;

	rv = xcdev_check(__func__, xcdev, 0);
	if (rv < 0)
		return rv;

	switch (cmd) {
	case IOCTL_XDMA_VME_WAIT:
		rv = ioctl_wait(xcdev, arg);
		break;
	default:
		dbg_perf("Unsupported operation\n");
		rv = -EINVAL;
		break;
	}

	return rv;
}

/*
 * character device file operations for the irq events
 */
static const struct file_operations events_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_close,
	.unlocked_ioctl = char_events_ioctl,
};

void cdev_vme_event_init(struct xdma_cdev *xcdev)
{
	xcdev->vme_user_irq = &(xcdev->xdev->vme_user_irq[xcdev->bar]);
	cdev_init(&xcdev->cdev, &events_fops);
}
