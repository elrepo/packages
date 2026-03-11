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
 * Description: statistics related data structures
 */

#ifndef __STATS_H__
#define __STATS_H__

#define BNXT_RE_CFA_STAT_BYTES_MASK 0xFFFFFFFFF
#define BNXT_RE_CFA_STAT_PKTS_MASK 0xFFFFFFF

#define BNXT_RE_RDATA_STAT(hw_stat, rdata)	\
		((hw_stat) ? (le64_to_cpu(hw_stat)) : (rdata))

enum{
	BYTE_MASK = 0,
	PKTS_MASK = 1
};

struct bnxt_re_cnp_counters {
	u64	cnp_tx_pkts;
	u64	cnp_tx_bytes;
	u64	cnp_rx_pkts;
	u64	cnp_rx_bytes;
	u64	ecn_marked;
};

struct bnxt_re_ro_counters {
	u64	tx_pkts;
	u64	tx_bytes;
	u64	rx_pkts;
	u64	rx_bytes;
};

struct bnxt_re_flow_counters {
	struct bnxt_re_ro_counters ro_stats;
	struct bnxt_re_cnp_counters cnp_stats;
};

struct bnxt_re_ext_cntr {
	u64	atomic_req;
	u64	read_req;
	u64	read_resp;
	u64	write_req;
	u64	send_req;
	u64	send_req_rc;
	u64	send_req_ud;
};

struct bnxt_re_ext_good {
	u64	rx_pkts;
	u64	rx_bytes;
};

struct bnxt_re_ext_rstat {
	struct bnxt_re_ext_cntr tx;
	struct bnxt_re_ext_cntr rx;
	struct bnxt_re_ext_good	grx;
	u64  rx_dcn_payload_cut;
	u64  te_bypassed;
	u64  tx_dcn_cnp;
	u64  rx_dcn_cnp;
	u64  rx_payload_cut;
	u64  rx_payload_cut_ignored;
	u64  rx_dcn_cnp_ignored;
};

struct bnxt_re_rdata_counters {
	u64  tx_ucast_pkts;
	u64  tx_mcast_pkts;
	u64  tx_bcast_pkts;
	u64  tx_discard_pkts;
	u64  tx_error_pkts;
	u64  tx_ucast_bytes;
	u64  tx_mcast_bytes;
	u64  tx_bcast_bytes;
	u64  rx_ucast_pkts;
	u64  rx_mcast_pkts;
	u64  rx_bcast_pkts;
	u64  rx_discard_pkts;
	u64  rx_error_pkts;
	u64  rx_ucast_bytes;
	u64  rx_mcast_bytes;
	u64  rx_bcast_bytes;
};

struct bnxt_re_cc_stat {
	struct bnxt_re_cnp_counters prev[2];
	struct bnxt_re_cnp_counters cur[2];
	bool is_first;
};

struct bnxt_re_ext_roce_stats {
	u64	oob;
	u64	oos;
	u64	seq_err_naks_rcvd;
	u64	rnr_naks_rcvd;
	u64	missing_resp;
	u64	to_retransmits;
	u64	dup_req;
};

struct bnxt_re_rstat {
	struct bnxt_re_ro_counters	prev[2];
	struct bnxt_re_ro_counters	cur[2];
	struct bnxt_re_rdata_counters	rstat[3];
	struct bnxt_re_ext_rstat	ext_rstat[2];
	struct bnxt_re_ext_roce_stats	e_errs;
	struct bnxt_qplib_roce_stats	errs;
	unsigned long long		prev_oob;
};

struct bnxt_re_res_cntrs {
	atomic_t qp_count;
	atomic_t rc_qp_count;
	atomic_t ud_qp_count;
	atomic_t cq_count;
	atomic_t srq_count;
	atomic_t mr_count;
	atomic_t mr_dmabuf_count;
	atomic_t peer_mem_mr_count;
	atomic_t mw_count;
	atomic_t ah_count;
	atomic_t ah_hw_count;
	atomic_t pd_count;
	atomic_t resize_count;
	atomic_t max_qp_count;
	atomic_t max_rc_qp_count;
	atomic_t max_ud_qp_count;
	atomic_t max_cq_count;
	atomic_t max_srq_count;
	atomic_t max_mr_count;
	atomic_t max_mr_dmabuf_count;
	atomic_t max_peer_mem_mr_count;
	atomic_t max_mw_count;
	atomic_t max_ah_count;
	atomic_t max_ah_hw_count;
	atomic_t max_pd_count;
};

struct bnxt_re_device_stats {
	struct bnxt_re_rstat            dstat;
	struct bnxt_re_res_cntrs        rsors;
	struct bnxt_re_cc_stat          cnps;
	unsigned long                   read_tstamp;
	/* To be used in case to disable stats query from worker or change
	 * query interval. 0 means stats_query disabled.
	 */
	u32				stats_query_sec;
	/* A free running counter to be used along with stats_query_sec to
	 * decide whether to issue the command to FW.
	 */
	u32				stats_query_counter;
};

static inline u64 bnxt_re_get_cfa_stat_mask(struct bnxt_qplib_chip_ctx *cctx,
					    bool type)
{
	u64 mask;

	if (type == BYTE_MASK) {
		mask = BNXT_RE_CFA_STAT_BYTES_MASK; /* 36 bits */
		if (_is_chip_p5_plus(cctx))
			mask >>= 0x01; /* 35 bits */
	} else {
		mask = BNXT_RE_CFA_STAT_PKTS_MASK; /* 28 bits */
		if (_is_chip_p5_plus(cctx))
			mask |= (0x10000000ULL); /* 29 bits */
	}

	return mask;
}

static inline u64 bnxt_re_stat_diff(u64 cur, u64 *prev, u64 mask)
{
	u64 diff;

	if (!cur)
		return 0;
	diff = (cur - *prev) & mask;
	if (diff)
		*prev = cur;
	return diff;
}

static inline void bnxt_re_clear_rsors_stat(struct bnxt_re_res_cntrs *rsors)
{
	atomic_set(&rsors->qp_count, 0);
	atomic_set(&rsors->cq_count, 0);
	atomic_set(&rsors->srq_count, 0);
	atomic_set(&rsors->mr_count, 0);
	atomic_set(&rsors->mr_dmabuf_count, 0);
	atomic_set(&rsors->peer_mem_mr_count, 0);
	atomic_set(&rsors->mw_count, 0);
	atomic_set(&rsors->ah_count, 0);
	atomic_set(&rsors->ah_hw_count, 0);
	atomic_set(&rsors->pd_count, 0);
	atomic_set(&rsors->resize_count, 0);
	atomic_set(&rsors->max_qp_count, 0);
	atomic_set(&rsors->max_cq_count, 0);
	atomic_set(&rsors->max_srq_count, 0);
	atomic_set(&rsors->max_mr_count, 0);
	atomic_set(&rsors->max_mr_dmabuf_count, 0);
	atomic_set(&rsors->max_peer_mem_mr_count, 0);
	atomic_set(&rsors->max_mw_count, 0);
	atomic_set(&rsors->max_ah_count, 0);
	atomic_set(&rsors->max_ah_hw_count, 0);
	atomic_set(&rsors->max_pd_count, 0);
	atomic_set(&rsors->max_rc_qp_count, 0);
	atomic_set(&rsors->max_ud_qp_count, 0);
}

int bnxt_re_get_device_stats(struct bnxt_re_dev *rdev);
int bnxt_re_get_qos_stats(struct bnxt_re_dev *rdev);
void bnxt_re_get_roce_data_stats(struct bnxt_re_dev *rdev);
void bnxt_re_copy_roce_stats_ext_ctx(struct bnxt_qplib_ext_stat_v2 *d,
				     struct roce_stats_ext_ctx *s);
#endif /* __STATS_H__ */
