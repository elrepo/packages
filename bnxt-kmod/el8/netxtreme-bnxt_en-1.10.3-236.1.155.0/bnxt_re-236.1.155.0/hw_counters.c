/*
 * Copyright (c) 2023-2025, Broadcom. All rights reserved.  The term
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
 * Description: Statistics
 *
 */

#include <rdma/ib_addr.h>
#include <rdma/ib_pma.h>

#include "roce_hsi.h"
#include "qplib_res.h"
#include "qplib_rcfw.h"
#include "bnxt_re.h"
#include "hw_counters.h"

#ifdef HAVE_RDMA_STAT_DESC
static const struct rdma_stat_desc bnxt_re_stat_descs[] = {
	[BNXT_RE_ACTIVE_PD].name		= "active_pds",
	[BNXT_RE_ACTIVE_AH].name		= "active_ahs",
	[BNXT_RE_ACTIVE_QP].name		= "active_qps",
	[BNXT_RE_ACTIVE_RC_QP].name             = "active_rc_qps",
	[BNXT_RE_ACTIVE_UD_QP].name             = "active_ud_qps",
	[BNXT_RE_ACTIVE_SRQ].name		= "active_srqs",
	[BNXT_RE_ACTIVE_CQ].name		= "active_cqs",
	[BNXT_RE_ACTIVE_MR].name		= "active_mrs",
	[BNXT_RE_ACTIVE_MW].name		= "active_mws",
	[BNXT_RE_WATERMARK_PD].name             = "watermark_pds",
	[BNXT_RE_WATERMARK_AH].name             = "watermark_ahs",
	[BNXT_RE_WATERMARK_QP].name             = "watermark_qps",
	[BNXT_RE_WATERMARK_RC_QP].name          = "watermark_rc_qps",
	[BNXT_RE_WATERMARK_UD_QP].name          = "watermark_ud_qps",
	[BNXT_RE_WATERMARK_SRQ].name            = "watermark_srqs",
	[BNXT_RE_WATERMARK_CQ].name             = "watermark_cqs",
	[BNXT_RE_WATERMARK_MR].name             = "watermark_mrs",
	[BNXT_RE_WATERMARK_MW].name             = "watermark_mws",
	[BNXT_RE_RX_PKTS].name			= "rx_pkts",
	[BNXT_RE_RX_BYTES].name			= "rx_bytes",
	[BNXT_RE_TX_PKTS].name			= "tx_pkts",
	[BNXT_RE_TX_BYTES].name			= "tx_bytes",
	[BNXT_RE_RECOVERABLE_ERRORS].name	= "recoverable_errors",
	[BNXT_RE_TX_ERRORS].name		= "tx_roce_errors",
	[BNXT_RE_TX_DISCARDS].name		= "tx_roce_discards",
	[BNXT_RE_RX_ERRORS].name		= "rx_roce_errors",
	[BNXT_RE_RX_DISCARDS].name		= "rx_roce_discards",
	[BNXT_RE_TO_RETRANSMITS].name		= "local_ack_timeout_err",
	[BNXT_RE_SEQ_ERR_NAKS_RCVD].name	= "packet_seq_err",
	[BNXT_RE_MAX_RETRY_EXCEEDED].name	= "max_retry_exceeded",
	[BNXT_RE_RNR_NAKS_RCVD].name		= "rnr_nak_retry_err",
	[BNXT_RE_MISSING_RESP].name		= "implied_nak_seq_err",
	[BNXT_RE_UNRECOVERABLE_ERR].name	= "unrecoverable_err",
	[BNXT_RE_BAD_RESP_ERR].name		= "bad_resp_err",
	[BNXT_RE_LOCAL_QP_OP_ERR].name		= "local_qp_op_err",
	[BNXT_RE_LOCAL_PROTECTION_ERR].name	= "local_protection_err",
	[BNXT_RE_MEM_MGMT_OP_ERR].name		= "mem_mgmt_op_err",
	[BNXT_RE_REMOTE_INVALID_REQ_ERR].name	= "req_remote_invalid_request",
	[BNXT_RE_REMOTE_ACCESS_ERR].name	= "req_remote_access_errors",
	[BNXT_RE_REMOTE_OP_ERR].name		= "remote_op_err",
	[BNXT_RE_DUP_REQ].name			= "duplicate_request",
	[BNXT_RE_RES_EXCEED_MAX].name		= "res_exceed_max",
	[BNXT_RE_RES_LENGTH_MISMATCH].name	= "resp_local_length_error",
	[BNXT_RE_RES_EXCEEDS_WQE].name		= "res_exceeds_wqe",
	[BNXT_RE_RES_OPCODE_ERR].name		= "res_opcode_err",
	[BNXT_RE_RES_RX_INVALID_RKEY].name	= "res_rx_invalid_rkey",
	[BNXT_RE_RES_RX_DOMAIN_ERR].name	= "res_rx_domain_err",
	[BNXT_RE_RES_RX_NO_PERM].name		= "res_rx_no_perm",
	[BNXT_RE_RES_RX_RANGE_ERR].name		= "res_rx_range_err",
	[BNXT_RE_RES_TX_INVALID_RKEY].name	= "res_tx_invalid_rkey",
	[BNXT_RE_RES_TX_DOMAIN_ERR].name	= "res_tx_domain_err",
	[BNXT_RE_RES_TX_NO_PERM].name		= "res_tx_no_perm",
	[BNXT_RE_RES_TX_RANGE_ERR].name		= "res_tx_range_err",
	[BNXT_RE_RES_IRRQ_OFLOW].name		= "res_irrq_oflow",
	[BNXT_RE_RES_UNSUP_OPCODE].name		= "res_unsup_opcode",
	[BNXT_RE_RES_UNALIGNED_ATOMIC].name	= "res_unaligned_atomic",
	[BNXT_RE_RES_REM_INV_ERR].name		= "res_rem_inv_err",
	[BNXT_RE_RES_MEM_ERROR].name		= "res_mem_err",
	[BNXT_RE_RES_SRQ_ERR].name		= "res_srq_err",
	[BNXT_RE_RES_CMP_ERR].name		= "res_cmp_err",
	[BNXT_RE_RES_INVALID_DUP_RKEY].name	= "res_invalid_dup_rkey",
	[BNXT_RE_RES_WQE_FORMAT_ERR].name	= "res_wqe_format_err",
	[BNXT_RE_RES_CQ_LOAD_ERR].name		= "res_cq_load_err",
	[BNXT_RE_RES_SRQ_LOAD_ERR].name		= "res_srq_load_err",
	[BNXT_RE_RES_TX_PCI_ERR].name		= "res_tx_pci_err",
	[BNXT_RE_RES_RX_PCI_ERR].name		= "res_rx_pci_err",
	[BNXT_RE_OUT_OF_SEQ_ERR].name		= "out_of_sequence",
	[BNXT_RE_TX_ATOMIC_REQ].name		= "tx_atomic_req",
	[BNXT_RE_TX_READ_REQ].name		= "tx_read_req",
	[BNXT_RE_TX_READ_RES].name		= "tx_read_resp",
	[BNXT_RE_TX_WRITE_REQ].name		= "tx_write_req",
	[BNXT_RE_TX_SEND_REQ].name		= "tx_send_req",
	[BNXT_RE_RX_ATOMIC_REQ].name		= "rx_atomic_requests",
	[BNXT_RE_RX_READ_REQ].name		= "rx_read_requests",
	[BNXT_RE_RX_READ_RESP].name		= "rx_read_resp",
	[BNXT_RE_RX_WRITE_REQ].name		= "rx_write_requests",
	[BNXT_RE_RX_SEND_REQ].name		= "rx_send_req",
	[BNXT_RE_RX_GOOD_PKTS].name		= "rx_good_pkts",
	[BNXT_RE_RX_GOOD_BYTES].name		= "rx_good_bytes",
	[BNXT_RE_OOB].name			= "out_of_buffer",
	[BNXT_RE_TX_CNP].name			= "np_cnp_sent",
	[BNXT_RE_RX_CNP].name			= "rp_cnp_handled",
	[BNXT_RE_RX_ECN].name			= "np_ecn_marked_roce_packets",
	[BNXT_RE_PACING_RESCHED].name		= "pacing_reschedule",
	[BNXT_RE_PACING_CMPL].name		= "pacing_complete",
	[BNXT_RE_PACING_ALERT].name		= "pacing_alerts",
	[BNXT_RE_DB_FIFO_REG].name		= "db_fifo_register",
	[BNXT_RE_REQ_CQE_ERROR].name            = "req_cqe_error",
	[BNXT_RE_REG_CQE_FLUSH_ERROR].name	= "req_cqe_flush_error",
	[BNXT_RE_RESP_CQE_ERROR].name		= "resp_cqe_error",
	[BNXT_RE_RESP_CQE_FLUSH_ERROR].name	= "resp_cqe_flush_error",
	[BNXT_RE_RESP_REMOTE_ACCESS_ERRS].name	= "resp_remote_access_errors",
	[BNXT_RE_ROCE_ADP_RETRANS].name		= "roce_adp_retrans",
	[BNXT_RE_ROCE_ADP_RETRANS_TO].name	= "roce_adp_retrans_to",
	[BNXT_RE_ROCE_SLOW_RESTART].name	= "roce_slow_restart",
	[BNXT_RE_ROCE_SLOW_RESTART_CNP].name	= "roce_slow_restart_cnps",
	[BNXT_RE_ROCE_SLOW_RESTART_TRANS].name	= "roce_slow_restart_trans",
	[BNXT_RE_RX_CNP_IGNORED].name		= "rp_cnp_ignored",
	[BNXT_RE_RX_ICRC_ENCAPSULATED].name	= "rx_icrc_encapsulated",
};
#else
static const char *const bnxt_re_stat_name[] = {
	[BNXT_RE_ACTIVE_PD]			= "active_pds",
	[BNXT_RE_ACTIVE_AH]			= "active_ahs",
	[BNXT_RE_ACTIVE_QP]			= "active_qps",
	[BNXT_RE_ACTIVE_RC_QP]			= "active_rc_qps",
	[BNXT_RE_ACTIVE_UD_QP]			= "active_ud_qps",
	[BNXT_RE_ACTIVE_SRQ]			= "active_srqs",
	[BNXT_RE_ACTIVE_CQ]			= "active_cqs",
	[BNXT_RE_ACTIVE_MR]			= "active_mrs",
	[BNXT_RE_ACTIVE_MW]			= "active_mws",
	[BNXT_RE_WATERMARK_PD]			= "watermark_pds",
	[BNXT_RE_WATERMARK_AH]			= "watermark_ahs",
	[BNXT_RE_WATERMARK_QP]			= "watermark_qps",
	[BNXT_RE_WATERMARK_RC_QP]		= "watermark_rc_qps",
	[BNXT_RE_WATERMARK_UD_QP]		= "watermark_ud_qps",
	[BNXT_RE_WATERMARK_SRQ]			= "watermark_srqs",
	[BNXT_RE_WATERMARK_CQ]			= "watermark_cqs",
	[BNXT_RE_WATERMARK_MR]			= "watermark_mrs",
	[BNXT_RE_WATERMARK_MW]			= "watermark_mws",
	[BNXT_RE_RX_PKTS]			= "rx_pkts",
	[BNXT_RE_RX_BYTES]			= "rx_bytes",
	[BNXT_RE_TX_PKTS]			= "tx_pkts",
	[BNXT_RE_TX_BYTES]			= "tx_bytes",
	[BNXT_RE_RECOVERABLE_ERRORS]		= "recoverable_errors",
	[BNXT_RE_TX_ERRORS]			= "tx_roce_errors",
	[BNXT_RE_TX_DISCARDS]			= "tx_roce_discards",
	[BNXT_RE_RX_ERRORS]			= "rx_roce_errors",
	[BNXT_RE_RX_DISCARDS]			= "rx_roce_discards",
	[BNXT_RE_TO_RETRANSMITS]		= "local_ack_timeout_err",
	[BNXT_RE_SEQ_ERR_NAKS_RCVD]		= "packet_seq_err",
	[BNXT_RE_MAX_RETRY_EXCEEDED]		= "max_retry_exceeded",
	[BNXT_RE_RNR_NAKS_RCVD]			= "rnr_nak_retry_err",
	[BNXT_RE_MISSING_RESP]			= "implied_nak_seq_err",
	[BNXT_RE_UNRECOVERABLE_ERR]		= "unrecoverable_err",
	[BNXT_RE_BAD_RESP_ERR]			= "bad_resp_err",
	[BNXT_RE_LOCAL_QP_OP_ERR]		= "local_qp_op_err",
	[BNXT_RE_LOCAL_PROTECTION_ERR]		= "local_protection_err",
	[BNXT_RE_MEM_MGMT_OP_ERR]		= "mem_mgmt_op_err",
	[BNXT_RE_REMOTE_INVALID_REQ_ERR]	= "req_remote_invalid_request",
	[BNXT_RE_REMOTE_ACCESS_ERR]		= "req_remote_access_errors",
	[BNXT_RE_REMOTE_OP_ERR]			= "remote_op_err",
	[BNXT_RE_DUP_REQ]			= "duplicate_request",
	[BNXT_RE_RES_EXCEED_MAX]		= "res_exceed_max",
	[BNXT_RE_RES_LENGTH_MISMATCH]		= "resp_local_length_error",
	[BNXT_RE_RES_EXCEEDS_WQE]		= "res_exceeds_wqe",
	[BNXT_RE_RES_OPCODE_ERR]		= "res_opcode_err",
	[BNXT_RE_RES_RX_INVALID_RKEY]		= "res_rx_invalid_rkey",
	[BNXT_RE_RES_RX_DOMAIN_ERR]		= "res_rx_domain_err",
	[BNXT_RE_RES_RX_NO_PERM]		= "res_rx_no_perm",
	[BNXT_RE_RES_RX_RANGE_ERR]		= "res_rx_range_err",
	[BNXT_RE_RES_TX_INVALID_RKEY]		= "res_tx_invalid_rkey",
	[BNXT_RE_RES_TX_DOMAIN_ERR]		= "res_tx_domain_err",
	[BNXT_RE_RES_TX_NO_PERM]		= "res_tx_no_perm",
	[BNXT_RE_RES_TX_RANGE_ERR]		= "res_tx_range_err",
	[BNXT_RE_RES_IRRQ_OFLOW]		= "res_irrq_oflow",
	[BNXT_RE_RES_UNSUP_OPCODE]		= "res_unsup_opcode",
	[BNXT_RE_RES_UNALIGNED_ATOMIC]		= "res_unaligned_atmeomic",
	[BNXT_RE_RES_REM_INV_ERR]		= "res_rem_inv_err",
	[BNXT_RE_RES_MEM_ERROR]			= "res_mem_err",
	[BNXT_RE_RES_SRQ_ERR]			= "res_srq_err",
	[BNXT_RE_RES_CMP_ERR]			= "res_cmp_err",
	[BNXT_RE_RES_INVALID_DUP_RKEY]		= "res_invalid_dup_rkey",
	[BNXT_RE_RES_WQE_FORMAT_ERR]		= "res_wqe_format_err",
	[BNXT_RE_RES_CQ_LOAD_ERR]		= "res_cq_load_err",
	[BNXT_RE_RES_SRQ_LOAD_ERR]		= "res_srq_load_err",
	[BNXT_RE_RES_TX_PCI_ERR]		= "res_tx_pci_err",
	[BNXT_RE_RES_RX_PCI_ERR]		= "res_rx_pci_err",
	[BNXT_RE_OUT_OF_SEQ_ERR]		= "out_of_sequence",
	[BNXT_RE_TX_ATOMIC_REQ]			= "tx_atomic_req",
	[BNXT_RE_TX_READ_REQ]			= "tx_read_req",
	[BNXT_RE_TX_READ_RES]			= "tx_read_resp",
	[BNXT_RE_TX_WRITE_REQ]			= "tx_write_req",
	[BNXT_RE_TX_SEND_REQ]			= "tx_send_req",
	[BNXT_RE_RX_ATOMIC_REQ]			= "rx_atomic_requests",
	[BNXT_RE_RX_READ_REQ]			= "rx_read_requests",
	[BNXT_RE_RX_READ_RESP]			= "rx_read_resp",
	[BNXT_RE_RX_WRITE_REQ]			= "rx_write_requests",
	[BNXT_RE_RX_SEND_REQ]			= "rx_send_req",
	[BNXT_RE_RX_GOOD_PKTS]			= "rx_good_pkts",
	[BNXT_RE_RX_GOOD_BYTES]			= "rx_good_bytes",
	[BNXT_RE_OOB]				= "out_of_buffer",
	[BNXT_RE_TX_CNP]			= "np_cnp_sent",
	[BNXT_RE_RX_CNP]			= "rp_cnp_handled",
	[BNXT_RE_RX_ECN]			= "np_ecn_marked_roce_packets",
	[BNXT_RE_PACING_RESCHED]		= "pacing_reschedule",
	[BNXT_RE_PACING_CMPL]			= "pacing_complete",
	[BNXT_RE_PACING_ALERT]			= "pacing_alerts",
	[BNXT_RE_DB_FIFO_REG]			= "db_fifo_register",
	[BNXT_RE_REQ_CQE_ERROR]			= "req_cqe_error",
	[BNXT_RE_REG_CQE_FLUSH_ERROR]           = "req_cqe_flush_error",
	[BNXT_RE_RESP_CQE_ERROR]		= "resp_cqe_error",
	[BNXT_RE_RESP_CQE_FLUSH_ERROR]		= "resp_cqe_flush_error",
	[BNXT_RE_RESP_REMOTE_ACCESS_ERRS]	= "resp_remote_access_errors",
	[BNXT_RE_ROCE_ADP_RETRANS]		= "roce_adp_retrans",
	[BNXT_RE_ROCE_ADP_RETRANS_TO]		= "roce_adp_retrans_to",
	[BNXT_RE_ROCE_SLOW_RESTART]		= "roce_slow_restart",
	[BNXT_RE_ROCE_SLOW_RESTART_CNP]		= "roce_slow_restart_cnps",
	[BNXT_RE_ROCE_SLOW_RESTART_TRANS]	= "roce_slow_restart_trans",
	[BNXT_RE_RX_CNP_IGNORED]		= "rp_cnp_ignored",
	[BNXT_RE_RX_ICRC_ENCAPSULATED]		= "rx_icrc_encapsulated",
};
#endif /* HAVE_RDMA_STAT_DESC */

static void bnxt_re_copy_normal_total_stats(struct bnxt_re_dev *rdev,
					    struct rdma_hw_stats *stats)
{

	if (_is_chip_gen_p5_p7(rdev->chip_ctx) && rdev->is_virtfn) {
		struct bnxt_re_rdata_counters *rstat = &rdev->stats.dstat.rstat[0];

		/* Only for VF from Thor onwards */
		stats->value[BNXT_RE_RX_PKTS] = rstat->rx_ucast_pkts;
		stats->value[BNXT_RE_RX_BYTES] = rstat->rx_ucast_bytes;
		stats->value[BNXT_RE_TX_PKTS] = rstat->tx_ucast_pkts;
		stats->value[BNXT_RE_TX_BYTES] = rstat->tx_ucast_bytes;
	} else {
		struct bnxt_re_ro_counters *roce_only;
		struct bnxt_re_cc_stat *cnps;

		cnps = &rdev->stats.cnps;
		roce_only = &rdev->stats.dstat.cur[0];

		stats->value[BNXT_RE_RX_PKTS] = cnps->cur[0].cnp_rx_pkts + roce_only->rx_pkts;
		stats->value[BNXT_RE_RX_BYTES] = cnps->cur[0].cnp_rx_bytes + roce_only->rx_bytes;
		stats->value[BNXT_RE_TX_PKTS] = cnps->cur[0].cnp_tx_pkts + roce_only->tx_pkts;
		stats->value[BNXT_RE_TX_BYTES] = cnps->cur[0].cnp_tx_bytes + roce_only->tx_bytes;

		stats->value[BNXT_RE_RX_CNP] = cnps->cur[0].cnp_rx_pkts;
		stats->value[BNXT_RE_TX_CNP] = cnps->cur[0].cnp_tx_pkts;
		stats->value[BNXT_RE_RX_ECN] = cnps->cur[0].ecn_marked;
	}
}

static void bnxt_re_copy_ext_stats(struct bnxt_re_dev *rdev,
				   struct rdma_hw_stats *stats)
{
	struct bnxt_re_ext_rstat *ext_s;

	ext_s = &rdev->stats.dstat.ext_rstat[0];

	stats->value[BNXT_RE_TX_ATOMIC_REQ] = ext_s->tx.atomic_req;
	stats->value[BNXT_RE_TX_READ_REQ]   = ext_s->tx.read_req;
	stats->value[BNXT_RE_TX_READ_RES]   = ext_s->tx.read_resp;
	stats->value[BNXT_RE_TX_WRITE_REQ]  = ext_s->tx.write_req;
	stats->value[BNXT_RE_TX_SEND_REQ]   = ext_s->tx.send_req;
	stats->value[BNXT_RE_RX_ATOMIC_REQ] = ext_s->rx.atomic_req;
	stats->value[BNXT_RE_RX_READ_REQ]   = ext_s->rx.read_req;
	stats->value[BNXT_RE_RX_READ_RESP]  = ext_s->rx.read_resp;
	stats->value[BNXT_RE_RX_WRITE_REQ]  = ext_s->rx.write_req;
	stats->value[BNXT_RE_RX_SEND_REQ]   = ext_s->rx.send_req;
	stats->value[BNXT_RE_RX_GOOD_PKTS]  = ext_s->grx.rx_pkts;
	stats->value[BNXT_RE_RX_GOOD_BYTES] = ext_s->grx.rx_bytes;
	stats->value[BNXT_RE_OOB] = rdev->stats.dstat.e_errs.oob;
	stats->value[BNXT_RE_OUT_OF_SEQ_ERR] = rdev->stats.dstat.e_errs.oos;
}

static void bnxt_re_copy_err_stats(struct bnxt_re_dev *rdev,
				   struct rdma_hw_stats *stats,
				   struct bnxt_qplib_roce_stats *err_s)
{
	stats->value[BNXT_RE_TO_RETRANSMITS] =
				err_s->to_retransmits;
	stats->value[BNXT_RE_SEQ_ERR_NAKS_RCVD] =
				err_s->seq_err_naks_rcvd;
	stats->value[BNXT_RE_MAX_RETRY_EXCEEDED] =
				err_s->max_retry_exceeded;
	stats->value[BNXT_RE_RNR_NAKS_RCVD] =
				err_s->rnr_naks_rcvd;
	stats->value[BNXT_RE_MISSING_RESP] =
				err_s->missing_resp;
	stats->value[BNXT_RE_UNRECOVERABLE_ERR] =
				err_s->unrecoverable_err;
	stats->value[BNXT_RE_BAD_RESP_ERR] =
				err_s->bad_resp_err;
	stats->value[BNXT_RE_LOCAL_QP_OP_ERR]	=
				err_s->local_qp_op_err;
	stats->value[BNXT_RE_LOCAL_PROTECTION_ERR] =
				err_s->local_protection_err;
	stats->value[BNXT_RE_MEM_MGMT_OP_ERR] =
				err_s->mem_mgmt_op_err;
	stats->value[BNXT_RE_REMOTE_INVALID_REQ_ERR] =
				err_s->remote_invalid_req_err;
	stats->value[BNXT_RE_REMOTE_ACCESS_ERR] =
				err_s->remote_access_err;
	stats->value[BNXT_RE_REMOTE_OP_ERR] =
				err_s->remote_op_err;
	stats->value[BNXT_RE_DUP_REQ] =
				err_s->dup_req;
	stats->value[BNXT_RE_RES_EXCEED_MAX] =
				err_s->res_exceed_max;
	stats->value[BNXT_RE_RES_LENGTH_MISMATCH] =
				err_s->res_length_mismatch;
	stats->value[BNXT_RE_RES_EXCEEDS_WQE] =
				err_s->res_exceeds_wqe;
	stats->value[BNXT_RE_RES_OPCODE_ERR] =
				err_s->res_opcode_err;
	stats->value[BNXT_RE_RES_RX_INVALID_RKEY] =
				err_s->res_rx_invalid_rkey;
	stats->value[BNXT_RE_RES_RX_DOMAIN_ERR] =
				err_s->res_rx_domain_err;
	stats->value[BNXT_RE_RES_RX_NO_PERM] =
				err_s->res_rx_no_perm;
	stats->value[BNXT_RE_RES_RX_RANGE_ERR]  =
				err_s->res_rx_range_err;
	stats->value[BNXT_RE_RES_TX_INVALID_RKEY] =
				err_s->res_tx_invalid_rkey;
	stats->value[BNXT_RE_RES_TX_DOMAIN_ERR] =
				err_s->res_tx_domain_err;
	stats->value[BNXT_RE_RES_TX_NO_PERM] =
				err_s->res_tx_no_perm;
	stats->value[BNXT_RE_RES_TX_RANGE_ERR]  =
				err_s->res_tx_range_err;
	stats->value[BNXT_RE_RES_IRRQ_OFLOW] =
				err_s->res_irrq_oflow;
	stats->value[BNXT_RE_RES_UNSUP_OPCODE]  =
				err_s->res_unsup_opcode;
	stats->value[BNXT_RE_RES_UNALIGNED_ATOMIC] =
				err_s->res_unaligned_atomic;
	stats->value[BNXT_RE_RES_REM_INV_ERR]   =
				err_s->res_rem_inv_err;
	stats->value[BNXT_RE_RES_MEM_ERROR] =
				err_s->res_mem_error;
	stats->value[BNXT_RE_RES_SRQ_ERR] =
				err_s->res_srq_err;
	stats->value[BNXT_RE_RES_CMP_ERR] =
				err_s->res_cmp_err;
	stats->value[BNXT_RE_RES_INVALID_DUP_RKEY] =
				err_s->res_invalid_dup_rkey;
	stats->value[BNXT_RE_RES_WQE_FORMAT_ERR] =
				err_s->res_wqe_format_err;
	stats->value[BNXT_RE_RES_CQ_LOAD_ERR]   =
				err_s->res_cq_load_err;
	stats->value[BNXT_RE_RES_SRQ_LOAD_ERR]  =
				err_s->res_srq_load_err;
	stats->value[BNXT_RE_RES_TX_PCI_ERR]    =
				err_s->res_tx_pci_err;
	stats->value[BNXT_RE_RES_RX_PCI_ERR]    =
				err_s->res_rx_pci_err;
	stats->value[BNXT_RE_OUT_OF_SEQ_ERR]    =
				err_s->res_oos_drop_count;
	stats->value[BNXT_RE_REQ_CQE_ERROR]     =
				err_s->bad_resp_err +
				err_s->local_qp_op_err +
				err_s->local_protection_err +
				err_s->mem_mgmt_op_err +
				err_s->remote_invalid_req_err +
				err_s->remote_access_err +
				err_s->remote_op_err;
	stats->value[BNXT_RE_RESP_CQE_ERROR]   =
				err_s->res_cmp_err +
				err_s->res_cq_load_err;
	stats->value[BNXT_RE_RESP_REMOTE_ACCESS_ERRS]   =
				err_s->res_rx_no_perm +
				err_s->res_tx_no_perm;
}

static void bnxt_re_copy_db_pacing_stats(struct bnxt_re_dev *rdev,
					 struct rdma_hw_stats *stats)
{
	struct bnxt_re_dbr_sw_stats *dbr_sw_stats = rdev->dbr_sw_stats;

	stats->value[BNXT_RE_PACING_RESCHED] = dbr_sw_stats->dbq_pacing_resched;
	stats->value[BNXT_RE_PACING_CMPL] = dbr_sw_stats->dbq_pacing_complete;
	stats->value[BNXT_RE_PACING_ALERT] = dbr_sw_stats->dbq_pacing_alerts;
	stats->value[BNXT_RE_DB_FIFO_REG] =
		readl(rdev->en_dev->bar0 + rdev->dbr_db_fifo_reg_off);
}

int bnxt_re_assign_pma_port_ext_counters(struct bnxt_re_dev *rdev, struct ib_mad *out_mad)
{
	struct ib_pma_portcounters_ext *pma_cnt_ext;
	struct bnxt_re_rdata_counters *rstat;
	int rc;

	rc = bnxt_re_get_device_stats(rdev);
	if (rc)
		return rc;

	rstat = &rdev->stats.dstat.rstat[0];
	pma_cnt_ext = (void *)(out_mad->data + 40);
	if (_is_chip_gen_p5_p7(rdev->chip_ctx) && rdev->is_virtfn) {
		pma_cnt_ext->port_xmit_data = cpu_to_be64((rstat->tx_ucast_bytes / 4));
		pma_cnt_ext->port_rcv_data = cpu_to_be64((rstat->rx_ucast_bytes / 4));
		pma_cnt_ext->port_xmit_packets = cpu_to_be64(rstat->tx_ucast_pkts);
		pma_cnt_ext->port_rcv_packets = cpu_to_be64(rstat->rx_ucast_pkts);
		pma_cnt_ext->port_unicast_rcv_packets = cpu_to_be64(rstat->rx_ucast_pkts);
		pma_cnt_ext->port_unicast_xmit_packets = cpu_to_be64(rstat->tx_ucast_pkts);

	} else {
		struct bnxt_re_ro_counters *roce_only;

		roce_only = &rdev->stats.dstat.cur[0];
		pma_cnt_ext->port_rcv_packets = cpu_to_be64(roce_only->rx_pkts);
		pma_cnt_ext->port_rcv_data = cpu_to_be64((roce_only->rx_bytes / 4));
		pma_cnt_ext->port_xmit_packets = cpu_to_be64(roce_only->tx_pkts);
		pma_cnt_ext->port_xmit_data = cpu_to_be64((roce_only->tx_bytes / 4));
		pma_cnt_ext->port_unicast_rcv_packets = cpu_to_be64(roce_only->rx_pkts);
		pma_cnt_ext->port_unicast_xmit_packets = cpu_to_be64(roce_only->tx_pkts);
	}
	return 0;
}

int bnxt_re_assign_pma_port_counters(struct bnxt_re_dev *rdev, struct ib_mad *out_mad)
{
	struct bnxt_en_ulp_stats counters = {0};
	struct ib_pma_portcounters *pma_cnt;
	struct bnxt_re_rdata_counters *rstat;
	int rc;

	rc = bnxt_re_get_device_stats(rdev);
	if (rc)
		return rc;

	rstat = &rdev->stats.dstat.rstat[0];
	pma_cnt = (void *)(out_mad->data + 40);
	if (_is_chip_gen_p5_p7(rdev->chip_ctx) && rdev->is_virtfn) {

		pma_cnt->port_rcv_packets = cpu_to_be32(rstat->rx_ucast_pkts);
		pma_cnt->port_rcv_data = cpu_to_be32((rstat->rx_ucast_bytes / 4));
		pma_cnt->port_xmit_packets = cpu_to_be32(rstat->tx_ucast_pkts);
		pma_cnt->port_xmit_data = cpu_to_be32((rstat->tx_ucast_bytes / 4));
	} else {
		struct bnxt_re_ro_counters *roce_only;

		roce_only = &rdev->stats.dstat.cur[0];
		pma_cnt->port_rcv_packets = cpu_to_be32(roce_only->rx_pkts);
		pma_cnt->port_rcv_data = cpu_to_be32((roce_only->rx_bytes / 4));
		pma_cnt->port_xmit_packets = cpu_to_be32(roce_only->tx_pkts);
		pma_cnt->port_xmit_data = cpu_to_be32((roce_only->tx_bytes / 4));
	}
	pma_cnt->port_rcv_constraint_errors = rstat->rx_discard_pkts;
	pma_cnt->port_rcv_errors = cpu_to_be16(rstat->rx_error_pkts);
	pma_cnt->port_xmit_constraint_errors = rstat->tx_error_pkts;
	pma_cnt->port_xmit_discards = cpu_to_be16(rstat->tx_discard_pkts);
	bnxt_ulp_get_stats(rdev->en_dev, &counters);
	pma_cnt->link_downed_counter = (u8)counters.link_down_events;
	pma_cnt->symbol_error_counter = (u16)counters.rx_pcs_symbol_err;

	return 0;
}

int bnxt_re_get_hw_stats(struct ib_device *ibdev,
			 struct rdma_hw_stats *stats,
			 PORT_NUM port, int index)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	struct bnxt_re_res_cntrs *res_s = &rdev->stats.rsors;
	struct bnxt_qplib_roce_stats *err_s = NULL;
	struct bnxt_re_rdata_counters *hw_stats;
	int rc;

	if (!port || !stats)
		return -EINVAL;

	err_s = &rdev->stats.dstat.errs;
	hw_stats = &rdev->stats.dstat.rstat[0];

	rc = bnxt_re_get_device_stats(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query device stats\n");
		return rc;
	}

	stats->value[BNXT_RE_ACTIVE_QP] = atomic_read(&res_s->qp_count);
	stats->value[BNXT_RE_ACTIVE_RC_QP] = atomic_read(&res_s->rc_qp_count);
	stats->value[BNXT_RE_ACTIVE_UD_QP] = atomic_read(&res_s->ud_qp_count);
	stats->value[BNXT_RE_ACTIVE_SRQ] = atomic_read(&res_s->srq_count);
	stats->value[BNXT_RE_ACTIVE_CQ] = atomic_read(&res_s->cq_count);
	stats->value[BNXT_RE_ACTIVE_MR] = atomic_read(&res_s->mr_count);
	stats->value[BNXT_RE_ACTIVE_MW] = atomic_read(&res_s->mw_count);
	stats->value[BNXT_RE_ACTIVE_PD] = atomic_read(&res_s->pd_count);
	stats->value[BNXT_RE_ACTIVE_AH] = atomic_read(&res_s->ah_count);
	stats->value[BNXT_RE_WATERMARK_QP] = atomic_read(&res_s->max_qp_count);
	stats->value[BNXT_RE_WATERMARK_RC_QP] = atomic_read(&res_s->max_rc_qp_count);
	stats->value[BNXT_RE_WATERMARK_UD_QP] = atomic_read(&res_s->max_ud_qp_count);
	stats->value[BNXT_RE_WATERMARK_SRQ] = atomic_read(&res_s->max_srq_count);
	stats->value[BNXT_RE_WATERMARK_CQ] = atomic_read(&res_s->max_cq_count);
	stats->value[BNXT_RE_WATERMARK_MR] = atomic_read(&res_s->max_mr_count);
	stats->value[BNXT_RE_WATERMARK_MW] = atomic_read(&res_s->max_mw_count);
	stats->value[BNXT_RE_WATERMARK_PD] = atomic_read(&res_s->max_pd_count);
	stats->value[BNXT_RE_WATERMARK_AH] = atomic_read(&res_s->max_ah_count);
	stats->value[BNXT_RE_RECOVERABLE_ERRORS] = hw_stats->tx_bcast_pkts;

	stats->value[BNXT_RE_TX_DISCARDS] = hw_stats->tx_discard_pkts;
	stats->value[BNXT_RE_TX_ERRORS] = hw_stats->tx_error_pkts;
	stats->value[BNXT_RE_RX_ERRORS] = hw_stats->rx_error_pkts;
	stats->value[BNXT_RE_RX_DISCARDS] = hw_stats->rx_discard_pkts;

	bnxt_re_copy_normal_total_stats(rdev, stats);

	bnxt_re_copy_err_stats(rdev, stats, err_s);

	if (_is_chip_gen_p5_p7(rdev->chip_ctx) && rdev->dbr_pacing)
		bnxt_re_copy_db_pacing_stats(rdev, stats);

	if (bnxt_ext_stats_supported(rdev->chip_ctx, rdev->dev_attr->dev_cap_flags,
				     rdev->is_virtfn))
		bnxt_re_copy_ext_stats(rdev, stats);

	return _is_chip_gen_p5_p7(rdev->chip_ctx) ?
		BNXT_RE_NUM_EXT_COUNTERS : BNXT_RE_NUM_STD_COUNTERS;
}

#ifdef HAVE_ALLOC_HW_PORT_STATS
struct rdma_hw_stats *bnxt_re_alloc_hw_port_stats(struct ib_device *ibdev,
						  PORT_NUM port_num)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	int num_counters;

	if (_is_chip_gen_p5_p7(rdev->chip_ctx))
		num_counters = BNXT_RE_NUM_EXT_COUNTERS;
	else
		num_counters = BNXT_RE_NUM_STD_COUNTERS;

#ifdef HAVE_RDMA_STAT_DESC
	return rdma_alloc_hw_stats_struct(bnxt_re_stat_descs, num_counters,
					  RDMA_HW_STATS_DEFAULT_LIFESPAN);
#else
	return rdma_alloc_hw_stats_struct(bnxt_re_stat_name, num_counters,
					  RDMA_HW_STATS_DEFAULT_LIFESPAN);
#endif
}
#else
struct rdma_hw_stats *bnxt_re_alloc_hw_stats(struct ib_device *ibdev,
					     u8 port_num)
{
	struct bnxt_re_dev *rdev = to_bnxt_re_dev(ibdev, ibdev);
	int num_counters;

	/* We support only per port stats */
	if (!port_num)
		return NULL;

	if (_is_chip_gen_p5_p7(rdev->chip_ctx))
		num_counters = BNXT_RE_NUM_EXT_COUNTERS;
	else
		num_counters = BNXT_RE_NUM_STD_COUNTERS;

	return rdma_alloc_hw_stats_struct(bnxt_re_stat_name, num_counters,
					  RDMA_HW_STATS_DEFAULT_LIFESPAN);
}
#endif /* HAVE_ALLOC_HW_PORT_STATS */

