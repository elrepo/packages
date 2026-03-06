// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2024 Broadcom
 * All rights reserved.
 */
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/limits.h>
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "tfc.h"
#include "bnxt_nic_flow.h"
#include "ulp_nic_flow.h"
#include "bnxt_vfr.h"
#include "tfc_util.h"
#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)

/* NIC flow database */
struct nic_flow_db {
#define BNXT_NIC_FLOW_DB_SIGNATURE 0x1ABEADED
	u32 signature;
	struct ulp_nic_flows flows[CFA_DIR_MAX];
};

#define NIC_FLOW_DB_VALID(nfdb) \
	((nfdb) && (nfdb)->signature == BNXT_NIC_FLOW_DB_SIGNATURE)


/* This function initializes the NIC Flow feature which allows
 * TF to insert NIC flows into the CFA.
 */
int bnxt_nic_flows_init(struct bnxt *bp)
{
	struct nic_flow_db *nfdb;
	enum cfa_dir dir;
	int rc = 0;

	nfdb = kzalloc(sizeof(*nfdb), GFP_ATOMIC);
	if (!nfdb)
		return -ENOMEM;

	bp->nic_flow_info = nfdb;
	nfdb->signature = BNXT_NIC_FLOW_DB_SIGNATURE;


	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		rc = bnxt_ulp_nic_flows_roce_add(bp,
						 &nfdb->flows[dir],
						 dir);
		if (rc) {
			netdev_dbg(bp->dev, "%s: NIC flow creation error(%d)\n",
				   __func__, rc);
			goto error;
		}
	}
	return rc;
error:
	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		rc = bnxt_ulp_nic_flows_roce_del(bp,
						 &nfdb->flows[dir],
						 dir);
		if (rc) {
			netdev_dbg(bp->dev, "%s: delete nic flows failed rc(%d)\n",
				   __func__, rc);
		}
		memset(&nfdb->flows[dir], 0, sizeof(nfdb->flows[dir]));
	}
	return rc;
}

void bnxt_nic_flows_deinit(struct bnxt *bp)
{
	struct nic_flow_db *nfdb = bp->nic_flow_info;
	enum cfa_dir dir;
	int rc = 0;

	if (!NIC_FLOW_DB_VALID(nfdb))
		return;

	for (dir = 0; dir < CFA_DIR_MAX; dir++) {
		rc = bnxt_ulp_nic_flows_roce_del(bp,
						 &nfdb->flows[dir],
						 dir);
		if (rc) {
			netdev_dbg(bp->dev, "%s: delete nic flows failed rc(%d)\n",
				   __func__, rc);
		}
		memset(&nfdb->flows[dir], 0, sizeof(nfdb->flows[dir]));
	}

	nfdb->signature = 0;
	kfree(bp->nic_flow_info);
	bp->nic_flow_info = NULL;
}


#else /* if defined(CONFIG_BNXT_FLOWER_OFFLOAD) */
int bnxt_nic_flows_init(struct bnxt *bp)
{
	return 0;
}

void bnxt_nic_flows_deinit(struct bnxt *bp)
{
}

#endif /* if defined(CONFIG_BNXT_FLOWER_OFFLOAD) */
