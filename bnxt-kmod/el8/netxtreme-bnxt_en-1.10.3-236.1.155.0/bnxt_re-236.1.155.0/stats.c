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
 * Description: statistics related functions
 */

#include "bnxt_re.h"
#include "bnxt.h"

int bnxt_re_get_qos_stats(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_ro_counters roce_only_tmp[2] = {{}, {}};
	struct bnxt_re_cnp_counters tmp_counters[2] = {{}, {}};
	struct hwrm_cfa_flow_stats_output resp = {};
	struct hwrm_cfa_flow_stats_input req = {};
	struct bnxt_en_dev *en_dev = rdev->en_dev;
	struct bnxt_fw_msg fw_msg = {};
	struct bnxt_re_cc_stat *cnps;
	struct bnxt_re_rstat *dstat;
	int rc = 0;
	u64 bytes;
	u64 pkts;

	/* FIXME Workaround to avoid system hang when this
	 * thread is competing with device create/destroy
	 * sequence
	 */
	if (!bnxt_re_rtnl_trylock())
		/* Not querying stats. Return older values */
		return 0;

	/* Issue HWRM cmd to read flow counters for CNP tx and rx */
	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_CFA_FLOW_STATS, -1);
	req.num_flows = cpu_to_le16(6);
	req.flow_handle_0 = cpu_to_le16(CFA_FLOW_INFO_REQ_FLOW_HANDLE_CNP_CNT);
	req.flow_handle_1 = cpu_to_le16(CFA_FLOW_INFO_REQ_FLOW_HANDLE_CNP_CNT |
					CFA_FLOW_INFO_REQ_FLOW_HANDLE_DIR_RX);
	req.flow_handle_2 = cpu_to_le16(CFA_FLOW_INFO_REQ_FLOW_HANDLE_ROCEV1_CNT);
	req.flow_handle_3 = cpu_to_le16(CFA_FLOW_INFO_REQ_FLOW_HANDLE_ROCEV1_CNT |
					CFA_FLOW_INFO_REQ_FLOW_HANDLE_DIR_RX);
	req.flow_handle_4 = cpu_to_le16(CFA_FLOW_INFO_REQ_FLOW_HANDLE_ROCEV2_CNT);
	req.flow_handle_5 = cpu_to_le16(CFA_FLOW_INFO_REQ_FLOW_HANDLE_ROCEV2_CNT |
				       CFA_FLOW_INFO_REQ_FLOW_HANDLE_DIR_RX);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
	rc = bnxt_send_msg(en_dev, &fw_msg);
	if (rc) {
		dev_dbg(rdev_to_dev(rdev),
			"Failed to get CFA Flow stats : rc = 0x%x", rc);
		goto done;
	}

	tmp_counters[0].cnp_tx_pkts = le64_to_cpu(resp.packet_0);
	tmp_counters[0].cnp_tx_bytes = le64_to_cpu(resp.byte_0);
	tmp_counters[0].cnp_rx_pkts = le64_to_cpu(resp.packet_1);
	tmp_counters[0].cnp_rx_bytes = le64_to_cpu(resp.byte_1);

	roce_only_tmp[0].tx_pkts = le64_to_cpu(resp.packet_2) +
				   le64_to_cpu(resp.packet_4);
	roce_only_tmp[0].tx_bytes = le64_to_cpu(resp.byte_2) +
				    le64_to_cpu(resp.byte_4);
	roce_only_tmp[0].rx_pkts = le64_to_cpu(resp.packet_3) +
				   le64_to_cpu(resp.packet_5);
	roce_only_tmp[0].rx_bytes = le64_to_cpu(resp.byte_3) +
				    le64_to_cpu(resp.byte_5);

	if (rdev->binfo) {
		memset(&req, 0, sizeof(req));
		memset(&fw_msg, 0, sizeof(fw_msg));
		bnxt_re_init_hwrm_hdr((void *)&req, HWRM_CFA_FLOW_STATS,
				      PCI_FUNC(rdev->binfo->pdev2->devfn) + 1);
		req.num_flows = cpu_to_le16(6);
		req.flow_handle_0 = cpu_to_le16(CFA_FLOW_INFO_REQ_FLOW_HANDLE_CNP_CNT);
		req.flow_handle_1 = cpu_to_le16(CFA_FLOW_INFO_REQ_FLOW_HANDLE_CNP_CNT |
				CFA_FLOW_INFO_REQ_FLOW_HANDLE_DIR_RX);
		req.flow_handle_2 = cpu_to_le16(CFA_FLOW_INFO_REQ_FLOW_HANDLE_ROCEV1_CNT);
		req.flow_handle_3 = cpu_to_le16(CFA_FLOW_INFO_REQ_FLOW_HANDLE_ROCEV1_CNT |
				CFA_FLOW_INFO_REQ_FLOW_HANDLE_DIR_RX);
		req.flow_handle_4 = cpu_to_le16(CFA_FLOW_INFO_REQ_FLOW_HANDLE_ROCEV2_CNT);
		req.flow_handle_5 = cpu_to_le16(CFA_FLOW_INFO_REQ_FLOW_HANDLE_ROCEV2_CNT |
				CFA_FLOW_INFO_REQ_FLOW_HANDLE_DIR_RX);
		bnxt_re_fill_fw_msg(&fw_msg, &req, sizeof(req), &resp,
				    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));
		rc = bnxt_send_msg(en_dev, &fw_msg);
		if (rc) {
			dev_dbg(rdev_to_dev(rdev),
				"Failed to get CFA 2nd port : rc = 0x%x",
				rc);
			/* Workaround for avoiding CFA query problem for bond */
			rc = 0;
			goto done;
		}
		tmp_counters[1].cnp_tx_pkts = le64_to_cpu(resp.packet_0);
		tmp_counters[1].cnp_tx_bytes = le64_to_cpu(resp.byte_0);
		tmp_counters[1].cnp_rx_pkts = le64_to_cpu(resp.packet_1);
		tmp_counters[1].cnp_rx_bytes = le64_to_cpu(resp.byte_1);

		roce_only_tmp[1].tx_pkts = le64_to_cpu(resp.packet_2) +
					   le64_to_cpu(resp.packet_4);
		roce_only_tmp[1].tx_bytes = le64_to_cpu(resp.byte_2) +
					    le64_to_cpu(resp.byte_4);
		roce_only_tmp[1].rx_pkts = le64_to_cpu(resp.packet_3) +
					   le64_to_cpu(resp.packet_5);
		roce_only_tmp[1].rx_bytes = le64_to_cpu(resp.byte_3) +
					    le64_to_cpu(resp.byte_5);
	}

	cnps = &rdev->stats.cnps;
	dstat = &rdev->stats.dstat;
	if (!cnps->is_first) {
		/* First query done.. */
		cnps->is_first = true;
		cnps->prev[0].cnp_tx_pkts = tmp_counters[0].cnp_tx_pkts;
		cnps->prev[0].cnp_tx_bytes = tmp_counters[0].cnp_tx_bytes;
		cnps->prev[0].cnp_rx_pkts = tmp_counters[0].cnp_rx_pkts;
		cnps->prev[0].cnp_rx_bytes = tmp_counters[0].cnp_rx_bytes;

		cnps->prev[1].cnp_tx_pkts = tmp_counters[1].cnp_tx_pkts;
		cnps->prev[1].cnp_tx_bytes = tmp_counters[1].cnp_tx_bytes;
		cnps->prev[1].cnp_rx_pkts = tmp_counters[1].cnp_rx_pkts;
		cnps->prev[1].cnp_rx_bytes = tmp_counters[1].cnp_rx_bytes;

		dstat->prev[0].tx_pkts = roce_only_tmp[0].tx_pkts;
		dstat->prev[0].tx_bytes = roce_only_tmp[0].tx_bytes;
		dstat->prev[0].rx_pkts = roce_only_tmp[0].rx_pkts;
		dstat->prev[0].rx_bytes = roce_only_tmp[0].rx_bytes;

		dstat->prev[1].tx_pkts = roce_only_tmp[1].tx_pkts;
		dstat->prev[1].tx_bytes = roce_only_tmp[1].tx_bytes;
		dstat->prev[1].rx_pkts = roce_only_tmp[1].rx_pkts;
		dstat->prev[1].rx_bytes = roce_only_tmp[1].rx_bytes;
	} else {
		u64 byte_mask, pkts_mask;
		u64 diff;

		byte_mask = bnxt_re_get_cfa_stat_mask(rdev->chip_ctx,
						      BYTE_MASK);
		pkts_mask = bnxt_re_get_cfa_stat_mask(rdev->chip_ctx,
						      PKTS_MASK);
		/*
		 * Calculate the number of cnp packets and use
		 * the value to calculate the CRC bytes.
		 * Multiply pkts with 4 and add it to total bytes
		 */
		pkts = bnxt_re_stat_diff(tmp_counters[0].cnp_tx_pkts,
					 &cnps->prev[0].cnp_tx_pkts,
					 pkts_mask);
		cnps->cur[0].cnp_tx_pkts += pkts;
		diff = bnxt_re_stat_diff(tmp_counters[0].cnp_tx_bytes,
					 &cnps->prev[0].cnp_tx_bytes,
					 byte_mask);
		bytes = diff + pkts * 4;
		cnps->cur[0].cnp_tx_bytes += bytes;
		pkts = bnxt_re_stat_diff(tmp_counters[0].cnp_rx_pkts,
					 &cnps->prev[0].cnp_rx_pkts,
					 pkts_mask);
		cnps->cur[0].cnp_rx_pkts += pkts;
		bytes = bnxt_re_stat_diff(tmp_counters[0].cnp_rx_bytes,
					  &cnps->prev[0].cnp_rx_bytes,
					  byte_mask);
		cnps->cur[0].cnp_rx_bytes += bytes;

		/*
		 * Calculate the number of cnp packets and use
		 * the value to calculate the CRC bytes.
		 * Multiply pkts with 4 and add it to total bytes
		 */
		pkts = bnxt_re_stat_diff(tmp_counters[1].cnp_tx_pkts,
					 &cnps->prev[1].cnp_tx_pkts,
					 pkts_mask);
		cnps->cur[1].cnp_tx_pkts += pkts;
		diff = bnxt_re_stat_diff(tmp_counters[1].cnp_tx_bytes,
					 &cnps->prev[1].cnp_tx_bytes,
					 byte_mask);
		cnps->cur[1].cnp_tx_bytes += diff + pkts * 4;
		pkts = bnxt_re_stat_diff(tmp_counters[1].cnp_rx_pkts,
					 &cnps->prev[1].cnp_rx_pkts,
					 pkts_mask);
		cnps->cur[1].cnp_rx_pkts += pkts;
		bytes = bnxt_re_stat_diff(tmp_counters[1].cnp_rx_bytes,
					  &cnps->prev[1].cnp_rx_bytes,
					  byte_mask);
		cnps->cur[1].cnp_rx_bytes += bytes;

		pkts = bnxt_re_stat_diff(roce_only_tmp[0].tx_pkts,
					 &dstat->prev[0].tx_pkts,
					 pkts_mask);
		dstat->cur[0].tx_pkts += pkts;
		diff = bnxt_re_stat_diff(roce_only_tmp[0].tx_bytes,
					 &dstat->prev[0].tx_bytes,
					 byte_mask);
		dstat->cur[0].tx_bytes += diff + pkts * 4;
		pkts = bnxt_re_stat_diff(roce_only_tmp[0].rx_pkts,
					 &dstat->prev[0].rx_pkts,
					 pkts_mask);
		dstat->cur[0].rx_pkts += pkts;

		bytes = bnxt_re_stat_diff(roce_only_tmp[0].rx_bytes,
					  &dstat->prev[0].rx_bytes,
					  byte_mask);
		dstat->cur[0].rx_bytes += bytes;
		pkts = bnxt_re_stat_diff(roce_only_tmp[1].tx_pkts,
					 &dstat->prev[1].tx_pkts,
					 pkts_mask);
		dstat->cur[1].tx_pkts += pkts;
		diff = bnxt_re_stat_diff(roce_only_tmp[1].tx_bytes,
					 &dstat->prev[1].tx_bytes,
					 byte_mask);
		dstat->cur[1].tx_bytes += diff + pkts * 4;
		pkts = bnxt_re_stat_diff(roce_only_tmp[1].rx_pkts,
					 &dstat->prev[1].rx_pkts,
					 pkts_mask);
		dstat->cur[1].rx_pkts += pkts;
		bytes = bnxt_re_stat_diff(roce_only_tmp[1].rx_bytes,
					  &dstat->prev[1].rx_bytes,
					  byte_mask);
		dstat->cur[1].rx_bytes += bytes;
	}
done:
	rtnl_unlock();
	return rc;
}

static void bnxt_re_copy_ext_stats(struct bnxt_re_dev *rdev,
				   u8 indx, struct bnxt_qplib_ext_stat *s)
{
	struct bnxt_re_ext_roce_stats *e_errs;
	struct bnxt_re_cnp_counters *cnp;
	struct bnxt_re_ext_rstat *ext_d;
	struct bnxt_re_ro_counters *ro;

	cnp = &rdev->stats.cnps.cur[indx];
	ro = &rdev->stats.dstat.cur[indx];
	ext_d = &rdev->stats.dstat.ext_rstat[indx];
	e_errs = &rdev->stats.dstat.e_errs;

	cnp->cnp_tx_pkts = s->tx_cnp;
	cnp->cnp_rx_pkts = s->rx_cnp;
	cnp->ecn_marked = s->rx_ecn_marked;

	/* In bonding mode do not duplicate other stats */
	if (indx)
		return;

	ro->tx_pkts = s->tx_roce_pkts;
	ro->tx_bytes = s->tx_roce_bytes;
	ro->rx_pkts = s->rx_roce_pkts;
	ro->rx_bytes = s->rx_roce_bytes;

	ext_d->tx.atomic_req = s->tx_atomic_req;
	ext_d->tx.read_req = s->tx_read_req;
	ext_d->tx.read_resp = s->tx_read_res;
	ext_d->tx.write_req = s->tx_write_req;
	ext_d->tx.send_req = s->tx_send_req;
	ext_d->rx.atomic_req = s->rx_atomic_req;
	ext_d->rx.read_req = s->rx_read_req;
	ext_d->rx.read_resp = s->rx_read_res;
	ext_d->rx.write_req = s->rx_write_req;
	ext_d->rx.send_req = s->rx_send_req;
	ext_d->grx.rx_pkts = s->rx_roce_good_pkts;
	ext_d->grx.rx_bytes = s->rx_roce_good_bytes;
	ext_d->rx_dcn_payload_cut = s->rx_dcn_payload_cut;
	ext_d->te_bypassed = s->te_bypassed;
	ext_d->tx_dcn_cnp = s->tx_dcn_cnp;
	ext_d->rx_dcn_cnp = s->rx_dcn_cnp;
	ext_d->rx_payload_cut = s->rx_payload_cut;
	ext_d->rx_payload_cut_ignored = s->rx_payload_cut_ignored;
	ext_d->rx_dcn_cnp_ignored = s->rx_dcn_cnp_ignored;
	e_errs->oob = s->rx_out_of_buffer;
	e_errs->oos = s->rx_out_of_sequence;
	e_errs->seq_err_naks_rcvd = s->seq_err_naks_rcvd;
	e_errs->rnr_naks_rcvd = s->rnr_naks_rcvd;
	e_errs->missing_resp = s->missing_resp;
	e_errs->to_retransmits = s->to_retransmits;
	e_errs->dup_req = s->dup_req;
}

static int bnxt_re_get_ext_stat(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_ext_stat estat[2] = {{}, {}};
	struct bnxt_qplib_query_stats_info sinfo;
	u32 fid;
	int rc;

	fid = PCI_FUNC(rdev->en_dev->pdev->devfn);
	/* Set default values for sinfo */
	sinfo.function_id = 0xFFFFFFFF;
	sinfo.collection_id = 0xFF;
	sinfo.vf_valid  = false;
	rc = bnxt_qplib_qext_stat(&rdev->rcfw, fid, &estat[0], &sinfo);
	if (rc)
		goto done;
	bnxt_re_copy_ext_stats(rdev, 0, &estat[0]);

	if (rdev->binfo) {
		fid = PCI_FUNC(rdev->binfo->pdev2->devfn);
		rc = bnxt_qplib_qext_stat(&rdev->rcfw, fid, &estat[1], &sinfo);
		if (rc)
			goto done;
		bnxt_re_copy_ext_stats(rdev, 1, &estat[1]);
	}
done:
	return rc;
}

static void bnxt_re_add_one_ctr(u64 hw, u64 *sw, u64 mask)
{
	u64 sw_tmp;

	hw &= mask;
	sw_tmp = (*sw & ~mask) | hw;
	if (hw < (*sw & mask))
		sw_tmp += mask + 1;
	WRITE_ONCE(*sw, sw_tmp);
}

static void __bnxt_re_accumulate_stats(__le64 *hw_stats, u64 *sw_stats,
				       u64 masks, int count, bool ignore_zero)
{
	int i;

	for (i = 0; i < count; i++) {
		u64 hw = le64_to_cpu(READ_ONCE(hw_stats[i]));

		if (ignore_zero && !hw)
			continue;

		if (masks == -1ULL)
			sw_stats[i] = hw;
		else
			bnxt_re_add_one_ctr(hw, &sw_stats[i], masks);
	}
}

static void bnxt_re_copy_ext_stats_v2(struct bnxt_re_dev *rdev,
				      u8 indx, struct bnxt_qplib_ext_stat_v2 *s)
{
	struct bnxt_re_ext_roce_stats *e_errs;
	struct bnxt_re_cnp_counters *cnp;
	struct bnxt_re_ext_rstat *ext_d;
	struct bnxt_re_ro_counters *ro;

	cnp = &rdev->stats.cnps.cur[indx];
	ro = &rdev->stats.dstat.cur[indx];
	ext_d = &rdev->stats.dstat.ext_rstat[indx];
	e_errs = &rdev->stats.dstat.e_errs;
	cnp->cnp_tx_pkts = s->tx_cnp;
	cnp->cnp_rx_pkts = s->rx_cnp;
	cnp->ecn_marked = s->rx_ecn_marked;
	if (indx)
		return;

	ro->tx_pkts = s->tx_roce;
	ro->tx_bytes = s->tx_roce_bytes;
	ro->rx_pkts = s->rx_roce;
	ro->rx_bytes = s->rx_roce_bytes;
	ext_d->tx.atomic_req = s->tx_atomic_req;
	ext_d->tx.read_req = s->tx_read_req;
	ext_d->tx.read_resp = s->tx_read_res;
	ext_d->tx.write_req = s->tx_write_req;
	ext_d->tx.send_req = s->tx_rc_send_req + s->tx_ud_send_req;
	ext_d->tx.send_req_rc = s->tx_rc_send_req;
	ext_d->tx.send_req_ud = s->tx_ud_send_req;
	ext_d->rx.atomic_req = s->rx_atomic_req;
	ext_d->rx.read_req = s->rx_read_req;
	ext_d->rx.read_resp = s->rx_read_res;
	ext_d->rx.write_req = s->rx_write_req;
	ext_d->rx.send_req = s->rx_rc_send + s->rx_ud_send;
	ext_d->rx.send_req_rc = s->rx_rc_send;
	ext_d->rx.send_req_ud = s->rx_ud_send;
	ext_d->grx.rx_pkts = s->rx_roce_good;
	ext_d->grx.rx_bytes = s->rx_roce_good_bytes;
	ext_d->rx_dcn_payload_cut = s->rx_dcn_payload_cut;
	ext_d->te_bypassed = 0;
	e_errs->oob = s->rx_out_of_buffer;
	e_errs->oos = s->rx_out_of_sequence;
	e_errs->seq_err_naks_rcvd = s->seq_err_naks_rcvd;
	e_errs->rnr_naks_rcvd = s->rnr_naks_rcvd;
	e_errs->missing_resp = s->missing_resp;
	e_errs->to_retransmits = s->to_retransmits;
	e_errs->dup_req = s->dup_req;
}

void bnxt_re_copy_roce_stats_ext_ctx(struct bnxt_qplib_ext_stat_v2 *d,
				     struct roce_stats_ext_ctx *s)
{
	d->tx_atomic_req = le64_to_cpu(s->tx_atomic_req_pkts);
	d->tx_read_req = le64_to_cpu(s->tx_read_req_pkts);
	d->tx_read_res = le64_to_cpu(s->tx_read_res_pkts);
	d->tx_write_req = le64_to_cpu(s->tx_write_req_pkts);
	d->tx_rc_send_req = le64_to_cpu(s->tx_rc_send_req_pkts);
	d->tx_ud_send_req = le64_to_cpu(s->tx_ud_send_req_pkts);
	d->tx_cnp = le64_to_cpu(s->tx_cnp_pkts);
	d->tx_roce = le64_to_cpu(s->tx_roce_pkts);
	d->tx_roce_bytes = le64_to_cpu(s->tx_roce_bytes);
	d->rx_out_of_buffer = le64_to_cpu(s->rx_out_of_buffer_pkts);
	d->rx_out_of_sequence = le64_to_cpu(s->rx_out_of_sequence_pkts);
	d->dup_req = le64_to_cpu(s->dup_req);
	d->missing_resp = le64_to_cpu(s->missing_resp);
	d->seq_err_naks_rcvd = le64_to_cpu(s->seq_err_naks_rcvd);
	d->rnr_naks_rcvd = le64_to_cpu(s->rnr_naks_rcvd);
	d->to_retransmits = le64_to_cpu(s->to_retransmits);
	d->rx_atomic_req = le64_to_cpu(s->rx_atomic_req_pkts);
	d->rx_read_req = le64_to_cpu(s->rx_read_req_pkts);
	d->rx_read_res = le64_to_cpu(s->rx_read_res_pkts);
	d->rx_write_req = le64_to_cpu(s->rx_write_req_pkts);
	d->rx_rc_send = le64_to_cpu(s->rx_rc_send_pkts);
	d->rx_ud_send = le64_to_cpu(s->rx_ud_send_pkts);
	d->rx_dcn_payload_cut = le64_to_cpu(s->rx_dcn_payload_cut);
	d->rx_ecn_marked = le64_to_cpu(s->rx_ecn_marked_pkts);
	d->rx_cnp = le64_to_cpu(s->rx_cnp_pkts);
	d->rx_roce = le64_to_cpu(s->rx_roce_pkts);
	d->rx_roce_bytes = le64_to_cpu(s->rx_roce_bytes);
	d->rx_roce_good = le64_to_cpu(s->rx_roce_good_pkts);
}

static int bnxt_re_get_ext_stat_v2(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_stats_ext_ctx *ctx;
	struct bnxt_qplib_ext_stat_v2 estat_v2;

	memset(&estat_v2, 0, sizeof(struct bnxt_qplib_ext_stat_v2));
	ctx = &rdev->qplib_res.hctx->stats_ext_ctx;
	bnxt_re_copy_roce_stats_ext_ctx(&estat_v2, ctx->cpu_addr);
	bnxt_re_copy_ext_stats_v2(rdev, 0, &estat_v2);

	return 0;
}

static void bnxt_re_copy_rstat(struct bnxt_re_dev *rdev,
			       struct bnxt_re_rdata_counters *d,
			       struct bnxt_qplib_stats *stats_mem)
{
	struct ctx_hw_stats_ext *hw_stats;
	struct ctx_hw_stats_ext *s;
	bool ignore_zero = false;
	u64 *sw_stats;
	u64 mask;

	if (_is_chip_gen_p5_p7(rdev->chip_ctx))
		mask = (1ULL << 48) - 1;
	else
		mask = -1ULL;

	/* P5 Chip bug.  Counter intermittently becomes 0. */
	if (_is_chip_gen_p5(rdev->chip_ctx))
		ignore_zero = true;

	hw_stats = stats_mem->cpu_addr;
	sw_stats = stats_mem->sw_stats;

	__bnxt_re_accumulate_stats((__le64 *)hw_stats, sw_stats,
				   mask, stats_mem->size / 8, ignore_zero);

	s = (struct ctx_hw_stats_ext *)sw_stats;

	d->tx_ucast_pkts = BNXT_RE_RDATA_STAT(s->tx_ucast_pkts, d->tx_ucast_pkts);
	d->tx_mcast_pkts = BNXT_RE_RDATA_STAT(s->tx_mcast_pkts, d->tx_mcast_pkts);
	d->tx_bcast_pkts = BNXT_RE_RDATA_STAT(s->tx_bcast_pkts, d->tx_bcast_pkts);
	d->tx_discard_pkts = BNXT_RE_RDATA_STAT(s->tx_discard_pkts, d->tx_discard_pkts);
	d->tx_error_pkts = BNXT_RE_RDATA_STAT(s->tx_error_pkts, d->tx_error_pkts);
	d->tx_ucast_bytes = BNXT_RE_RDATA_STAT(s->tx_ucast_bytes, d->tx_ucast_bytes);
	/* Add four bytes of CRC bytes per packet */
	d->tx_ucast_bytes +=  d->tx_ucast_pkts * 4;
	d->tx_mcast_bytes = BNXT_RE_RDATA_STAT(s->tx_mcast_bytes, d->tx_mcast_bytes);
	d->tx_bcast_bytes = BNXT_RE_RDATA_STAT(s->tx_bcast_bytes, d->tx_bcast_bytes);
	d->rx_ucast_pkts = BNXT_RE_RDATA_STAT(s->rx_ucast_pkts, d->rx_ucast_pkts);
	d->rx_mcast_pkts = BNXT_RE_RDATA_STAT(s->rx_mcast_pkts, d->rx_mcast_pkts);
	d->rx_bcast_pkts = BNXT_RE_RDATA_STAT(s->rx_bcast_pkts, d->rx_bcast_pkts);
	d->rx_discard_pkts = BNXT_RE_RDATA_STAT(s->rx_discard_pkts, d->rx_discard_pkts);
	d->rx_error_pkts = BNXT_RE_RDATA_STAT(s->rx_error_pkts, d->rx_error_pkts);
	d->rx_ucast_bytes = BNXT_RE_RDATA_STAT(s->rx_ucast_bytes, d->rx_ucast_bytes);
	d->rx_mcast_bytes = BNXT_RE_RDATA_STAT(s->rx_mcast_bytes, d->rx_mcast_bytes);
	d->rx_bcast_bytes = BNXT_RE_RDATA_STAT(s->rx_bcast_bytes, d->rx_bcast_bytes);

}

void bnxt_re_get_roce_data_stats(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_rdata_counters *rstat;

	rstat = &rdev->stats.dstat.rstat[0];
	bnxt_re_copy_rstat(rdev, rstat, &rdev->qplib_res.hctx->stats);

	if (rdev->rcfw.roce_mirror) {
		rstat = &rdev->stats.dstat.rstat[2];
		bnxt_re_copy_rstat(rdev, rstat, &rdev->qplib_res.hctx->stats3);
	}

	/* Query second port if LAG is enabled */
	if (rdev->binfo) {
		rstat = &rdev->stats.dstat.rstat[1];
		bnxt_re_copy_rstat(rdev, rstat, &rdev->qplib_res.hctx->stats2);
	}
}

int bnxt_re_get_device_stats(struct bnxt_re_dev *rdev)
{
	struct bnxt_qplib_query_stats_info sinfo;
	int rc = 0;

	/* Stats are in 1s cadence */
	if (test_bit(BNXT_RE_FLAG_ISSUE_CFA_FLOW_STATS, &rdev->flags)) {
		if (bnxt_ext_stats_v2_supported(rdev->dev_attr->dev_cap_ext_flags))
			rc = bnxt_re_get_ext_stat_v2(rdev);
		else if (bnxt_ext_stats_supported(rdev->chip_ctx,
						  rdev->dev_attr->dev_cap_flags,
						  rdev->is_virtfn))
			rc = bnxt_re_get_ext_stat(rdev);
		else
			rc = bnxt_re_get_qos_stats(rdev);

		if (rc && rc != -ENOMEM)
			clear_bit(BNXT_RE_FLAG_ISSUE_CFA_FLOW_STATS,
				  &rdev->flags);
	}

	if (test_bit(BNXT_RE_FLAG_ISSUE_ROCE_STATS, &rdev->flags)) {
		bnxt_re_get_roce_data_stats(rdev);

		/* Set default values for sinfo */
		sinfo.function_id = 0xFFFFFFFF;
		sinfo.collection_id = 0xFF;
		sinfo.vf_valid  = false;
		rc = bnxt_qplib_get_roce_error_stats(&rdev->rcfw,
						     &rdev->stats.dstat.errs,
						     &sinfo);
		if (rc && rc != -ENOMEM)
			clear_bit(BNXT_RE_FLAG_ISSUE_ROCE_STATS,
				  &rdev->flags);
	}

	return rc;
}
