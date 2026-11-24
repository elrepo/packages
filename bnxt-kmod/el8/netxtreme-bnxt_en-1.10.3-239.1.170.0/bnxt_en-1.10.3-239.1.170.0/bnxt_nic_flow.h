/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Broadcom
 * All rights reserved.
 */
#ifndef BNXT_NIC_FLOW_H
#define BNXT_NIC_FLOW_H

#define RX_NIC_FLOW_SUPPORTED(bp) \
	((BNXT_PF(bp)) && (BNXT_UDCC_CAP(bp)) && (BNXT_TF_RX_NIC_FLOW_CAP(bp)))

#define TX_NIC_FLOW_SUPPORTED(bp) \
	((BNXT_PF(bp)) && (BNXT_UDCC_CAP(bp)) && (BNXT_TF_TX_NIC_FLOW_CAP(bp)))

#define NIC_FLOW_SUPPORTED(bp) \
	(BNXT_PF(bp) && BNXT_UDCC_CAP(bp) && \
	 ((BNXT_TF_TX_NIC_FLOW_CAP(bp)) || (BNXT_TF_RX_NIC_FLOW_CAP(bp))))

int bnxt_nic_flows_init(struct bnxt *bp);
void bnxt_nic_flows_deinit(struct bnxt *bp);
#endif /* BNXT_NIC_FLOW_H */
