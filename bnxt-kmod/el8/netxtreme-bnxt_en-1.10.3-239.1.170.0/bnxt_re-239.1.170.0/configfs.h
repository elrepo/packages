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
 * Description: defines data-structure for configfs interface
 */
#ifndef __CONFIGFS_H__
#define __CONFIGFS_H__

#include <linux/module.h>
#include <linux/configfs.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_addr.h>

#include "compat.h"
#include "bnxt_ulp.h"
#include "roce_hsi.h"
#include "qplib_res.h"
#include "qplib_sp.h"
#include "qplib_fp.h"
#include "qplib_rcfw.h"
#include "bnxt_re.h"

#define BNXT_RE_MAX_CONFIGFS_ENTRIES 4
#define BNXT_DBR_DROP_MIN_TIMEOUT	1  	/* 1 ms */
#define BNXT_DBR_DROP_MAX_TIMEOUT 	1000	/* 1000 ms */

extern struct list_head bnxt_re_dev_list;
extern struct mutex bnxt_re_mutex;

#define BNXT_RE_CONFIGFS_HIDE_ADV_CC_PARAMS	0x0
#define BNXT_RE_CONFIGFS_SHOW_ADV_CC_PARAMS	0x1

enum bnxt_re_configfs_cmd {
	BNXT_RE_MODIFY_CC = 0x01,
};

struct bnxt_re_port_group;
struct bnxt_re_dev_group;

struct bnxt_re_cfg_group
{
	struct bnxt_re_dev *rdev;
	struct bnxt_re_port_group *portgrp;
	struct config_group group;
};

struct bnxt_re_port_group
{
	unsigned int port_num;
	struct bnxt_re_dev_group *devgrp;
	struct bnxt_re_cfg_group *ccgrp;
	struct bnxt_re_cfg_group *tungrp;
#if defined(CONFIGFS_BIN_ATTR)
	struct bnxt_re_cfg_group *udccgrp;
#endif
	struct config_group nportgrp;
#ifndef HAVE_CFGFS_ADD_DEF_GRP
	struct config_group *default_grp[BNXT_RE_MAX_CONFIGFS_ENTRIES];
#endif
};

struct bnxt_re_dev_group
{
	char name[IB_DEVICE_NAME_MAX];
	struct config_group dev_group;
	struct config_group port_group;
#ifndef HAVE_CFGFS_ADD_DEF_GRP
	struct config_group *default_devgrp[2];
	struct config_group **default_portsgrp;
#endif
	struct bnxt_re_port_group *ports;
	int nports;
};

int bnxt_re_get_print_dscp_pri_mapping(struct bnxt_re_dev *rdev,
				       char *buf,
				       struct bnxt_qplib_cc_param *ccparam);
u8 bnxt_re_get_priority_mask(struct bnxt_re_dev *rdev, u8 selector);
int bnxt_re_configfs_init(void);
void bnxt_re_configfs_exit(void);
#endif
