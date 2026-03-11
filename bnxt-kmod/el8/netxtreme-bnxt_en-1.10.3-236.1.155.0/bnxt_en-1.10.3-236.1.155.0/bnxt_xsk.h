/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2024 Broadcom Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef __BNXT_XSK_H__
#define __BNXT_XSK_H__

#ifdef HAVE_XSK_SUPPORT
#include <net/xdp_sock_drv.h>
#endif

int bnxt_xsk_wakeup(struct net_device *dev, u32 queue_id, u32 flags);
int bnxt_xdp_setup_pool(struct bnxt *bp, struct xsk_buff_pool *pool,
			u16 queue_id);
bool bnxt_xsk_xmit(struct bnxt *bp, struct bnxt_napi *bnapi, int budget);
#endif
