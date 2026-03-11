/*
 * Copyright (c) 2015-2025, Broadcom. All rights reserved.  The term
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
 * Description: DebugFS specifics
 */

#include "bnxt_re.h"
#include "bnxt.h"
#include "debugfs.h"
#include "ib_verbs.h"
#include "hdbr.h"
#include "bnxt_ulp.h"
#include "qplib_res.h"

#ifdef ENABLE_DEBUGFS

#define BNXT_RE_DEBUGFS_NAME_BUF_SIZE	128

static struct dentry *bnxt_re_debugfs_root;
extern unsigned int restrict_stats;

static void bnxt_re_print_ext_stat(struct bnxt_re_dev *rdev,
				   struct seq_file *s);

static const char * const bnxt_re_cc_gen0_name[] = {
	"enable_cc",
	"run_avg_weight_g",
	"num_phase_per_state",
	"init_cr",
	"init_tr",
	"tos_ecn",
	"tos_dscp",
	"alt_vlan_pcp",
	"alt_vlan_dscp",
	"rtt",
	"cc_mode",
	"tcp_cp",
	"tx_queue",
	"inactivity_cp",
};

static const char * const bnxt_re_cc_gen1_name[] = {
	"min_time_bet_cnp_store",
	"init_cp",
	"tr_update_mode",
	"tr_update_cycles",
	"fr_num_rtt",
	"ai_rate_incr",
	"red_rel_rtts_th",
	"act_rel_cr_th",
	"cr_min_th",
	"bw_avg_weight",
	"act_cr_factor",
	"max_cp_cr_th",
	"cp_bias_en",
	"cp_bias",
	"cnp_ecn",
	"rtt_jitter_en",
	"lbytes_per_usec",
	"reset_cc_cr_th",
	"cr_width",
	"min_quota",
	"max_quota",
	"abs_max_quota",
	"tr_lb",
	"cr_prob_fac",
	"tr_prob_fact",
	"fair_cr_th",
	"red_div",
	"cnp_ratio_th",
	"exp_ai_rtts",
	"exp_crcp_ratio",
	"cp_exp_update_th",
	"ai_rtts_th1",
	"ai_rtts_th2",
	"rt_en",
	"link64b_per_rtt",
	"cf_rtt_th",
	"sc_cr_th1",
	"sc_cr_th2",
	"cc_ack_bytes",
	"reduce1_init_en",
	"reduce_cf_rtt_th",
	"rnd_no_red_en",
	"act_cr_sh_cor_en",
	"per_adj_en"
};

static const char * const bnxt_re_cc_gen1_ext1_name[] = {
	"rnd_no_red_mult",
	"no_red_off",
	"reduce_init_cong_free_rtts_th",
	"reduce_init_en",
	"per_adj_cnt",
	"cr_th1",
	"cr_th2"
};

static const char *qp_type_str[] = {
	"IB_QPT_SMI",
	"IB_QPT_GSI",
	"IB_QPT_RC",
	"IB_QPT_UC",
	"IB_QPT_UD",
	"IB_QPT_RAW_IPV6",
	"IB_QPT_RAW_ETHERTYPE",
	"IB_QPT_UNKNOWN",
	"IB_QPT_RAW_PACKET",
	"IB_QPT_XRC_INI",
	"IB_QPT_XRC_TGT",
	"IB_QPT_MAX"
};

static const char *qp_state_str[] = {
	"IB_QPS_RESET",
	"IB_QPS_INIT",
	"IB_QPS_RTR",
	"IB_QPS_RTS",
	"IB_QPS_SQD",
	"IB_QPS_SQE",
	"IB_QPS_ERR"
};

static const char * const bnxt_re_cq_coal_str[] = {
	"buf_maxtime",
	"normal_maxbuf",
	"during_maxbuf",
	"en_ring_idle_mode",
	"enable",
};

static void bnxt_re_fill_qp_info(struct bnxt_re_qp *qp)
{
	struct bnxt_dcqcn_session_entry *ses;
	struct bnxt_re_dev *rdev = qp->rdev;
	struct bnxt_qplib_qp *qplib_qp;
	struct bnxt_re_flow *flow;
	u16 type, state, s_port;
	u8 *cur_ptr;
	int rc;

	cur_ptr = qp->qp_data;
	if (!cur_ptr)
		return;

	qplib_qp = kcalloc(1, sizeof(*qplib_qp), GFP_KERNEL);
	if (!qplib_qp)
		return;

	qplib_qp->id = qp->qplib_qp.id;
	rc = bnxt_qplib_query_qp(&rdev->qplib_res, qplib_qp);
	if (rc)
		goto bail;
	type = __from_hw_to_ib_qp_type(qp->qplib_qp.type);
	cur_ptr += sprintf(cur_ptr, "type \t = %s(%d)\n",
			   (type > IB_QPT_MAX) ?
			   "IB_QPT_UNKNOWN" : qp_type_str[type],
			   type);
	state =  __to_ib_qp_state(qplib_qp->state);
	cur_ptr += sprintf(cur_ptr, "state \t = %s(%d)\n",
			   (state > IB_QPS_ERR) ?
			   "IB_QPS_UNKNOWN" : qp_state_str[state],
			   state);
	cur_ptr += sprintf(cur_ptr, "source qpn \t = %d\n", qplib_qp->id);

	if (type != IB_QPT_UD)
		cur_ptr += sprintf(cur_ptr, "dest qpn \t = %d\n", qplib_qp->dest_qpn);

	if (type != IB_QPT_UD || BNXT_RE_UDP_SP_WQE(qp->qplib_qp.dev_cap_ext_flags2)) {
		if (qplib_qp->udp_sport)
			s_port = qplib_qp->udp_sport;
		else if (qp->qplib_qp.ext_modify_flags &
			 CMDQ_MODIFY_QP_EXT_MODIFY_MASK_UDP_SRC_PORT_VALID)
			s_port = qp->qplib_qp.udp_sport;
		else
			s_port = qp->qp_info_entry.s_port;
		cur_ptr += sprintf(cur_ptr, "source port \t = %d\n", s_port);
	}

	cur_ptr += sprintf(cur_ptr, "dest port \t = %d\n", qp->qp_info_entry.d_port);
	cur_ptr += sprintf(cur_ptr, "port \t = %d\n", qplib_qp->port_id);

	if (type != IB_QPT_UD) {
		if (qp->qplib_qp.nw_type == CMDQ_MODIFY_QP_NETWORK_TYPE_ROCEV2_IPV4) {
			cur_ptr += sprintf(cur_ptr, "source_ipaddr \t = %pI4\n",
				   &qp->qp_info_entry.s_ip.ipv4_addr);
			cur_ptr += sprintf(cur_ptr, "destination_ipaddr \t = %pI4\n",
				   &qp->qp_info_entry.d_ip.ipv4_addr);
		} else {
			cur_ptr += sprintf(cur_ptr, "source_ipaddr \t = %pI6\n",
					   qp->qp_info_entry.s_ip.ipv6_addr);
			cur_ptr += sprintf(cur_ptr, "destination_ipaddr \t = %pI6\n",
					   qp->qp_info_entry.d_ip.ipv6_addr);
		}
	}

	cur_ptr += sprintf(cur_ptr, "timeout \t = %d\n", qplib_qp->timeout);
	if (type != IB_QPT_UD) {
		cur_ptr += sprintf(cur_ptr, "shaper allocated \t = %d\n",
				   qp->qplib_qp.shaper_allocation_status);
		cur_ptr += sprintf(cur_ptr, "rate limit \t = %d kbps\n",
				   qplib_qp->rate_limit);
	}

	if (BNXT_RE_IQM(rdev->dev_attr->dev_cap_flags)) {
		cur_ptr += sprintf(cur_ptr, "max_dest_rd_atomic \t = %d\n",
				   qp->qplib_qp.max_rd_atomic);
		cur_ptr += sprintf(cur_ptr, "max_rd_atomic \t = %d\n",
				   qp->qplib_qp.max_dest_rd_atomic);
	}

	mutex_lock(&qp->flow_lock);
	if (!list_empty(&qp->flow_list))
		cur_ptr += sprintf(cur_ptr, "Flow Info:\n");
	list_for_each_entry(flow, &qp->flow_list, list) {
		ses = flow->session;
		if (ses && ses->type == DCQCN_FLOW_TYPE_ROCE_MONITOR)
			cur_ptr += sprintf(cur_ptr, "\t\tType : RoCE Monitor, l4_proto %d,"
					   " vnic %d ifa_gns %d\n", ses->l4_proto,
					   ses->vnic_id, ses->ifa_gns);
	}
	mutex_unlock(&qp->flow_lock);

bail:
	kfree(qplib_qp);
}

static ssize_t bnxt_re_qp_info_qp_read(struct file *filp, char __user *buffer,
				       size_t usr_buf_len, loff_t *ppos)
{
	struct bnxt_re_qp *qp = filp->private_data;

	if (usr_buf_len < BNXT_RE_DEBUGFS_QP_INFO_MAX_SIZE)
		return -ENOSPC;

	if (!qp->qp_data)
		return -ENOMEM;

	if (*ppos >= BNXT_RE_DEBUGFS_QP_INFO_MAX_SIZE)
		return 0;

	bnxt_re_fill_qp_info(qp);

	return simple_read_from_buffer(buffer, usr_buf_len, ppos,
				       (u8 *)(qp->qp_data),
				       strlen((char *)qp->qp_data));
}

static ssize_t bnxt_re_handle_modify_udp_sport(struct bnxt_re_dev *rdev,
					       struct bnxt_re_qp *qp, char *val_str)
{
	u16 new_sport;
	int rc = 0;

	rc = kstrtou16(val_str, 10, &new_sport);
	if (rc < 0)
		return rc;

	if (!new_sport)
		return -EINVAL;

	qp->qp_info_entry.s_port = new_sport;
	rc = bnxt_re_modify_udp_sport(qp, new_sport);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to update UDP source port\n");
		return rc;
	}
	return rc;
}

static ssize_t bnxt_re_handle_modify_rate_limit(struct bnxt_re_dev *rdev,
						struct bnxt_re_qp *qp, char *val_str)
{
	u32 new_rate_limit;
	int rc = 0;

	rc = kstrtou32(val_str, 10, &new_rate_limit);
	if (rc < 0)
		return rc;

	if (!new_rate_limit)
		return -EINVAL;

	if (new_rate_limit < rdev->dev_attr->rate_limit_min ||
	    new_rate_limit > rdev->dev_attr->rate_limit_max) {
		dev_err(rdev_to_dev(rdev),
			"Invalid value %u. Valid range is %d to %d kbps\n", new_rate_limit,
			rdev->dev_attr->rate_limit_min, rdev->dev_attr->rate_limit_max);
		return -EINVAL;
	}

	qp->qp_info_entry.rate_limit = new_rate_limit;
	rc = bnxt_re_modify_rate_limit(qp, new_rate_limit);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to update qp rate limit\n");
		return rc;
	}
	return rc;
}

static ssize_t bnxt_re_qp_info_qp_write(struct file *filp, const char __user *buffer,
					size_t count, loff_t *ppos)
{
	struct bnxt_re_qp *qp = filp->private_data;
	struct bnxt_re_dev *rdev = qp->rdev;
	char *cmd, *val_str, *buf;
	int rc = -EINVAL;

	if (count > BNXT_RE_DEBUGFS_NAME_BUF_SIZE)
		return -EINVAL;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EFAULT;
	}

	buf[count] = '\0';

	cmd = strsep(&buf, " \t");
	val_str = strsep(&buf, " \t");

	if (!cmd || (strcmp(cmd, "UDP_PORT") != 0 && strcmp(cmd, "RATE_LIMIT") != 0)) {
		dev_err(rdev_to_dev(rdev), "Invalid identifier. Expected 'UDP_PORT or RATE_LIMIT'\n");
		kfree(buf);
		return -EINVAL;
	}

	if (strcmp(cmd, "UDP_PORT") == 0)
		rc = bnxt_re_handle_modify_udp_sport(rdev, qp, val_str);

	if (strcmp(cmd, "RATE_LIMIT") == 0)
		rc = bnxt_re_handle_modify_rate_limit(rdev, qp, val_str);

	kfree(buf);
	return (rc < 0 ? rc : count);
}

static const struct file_operations bnxt_re_qp_info_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = bnxt_re_qp_info_qp_read,
	.write = bnxt_re_qp_info_qp_write,
};

void bnxt_re_qp_info_add_qpinfo(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp)
{
	char qp_name[32];

	qp->qp_data = kzalloc(BNXT_RE_DEBUGFS_QP_INFO_MAX_SIZE, GFP_KERNEL);
	if (!qp->qp_data)
		return;

	sprintf(qp_name, "0x%x", qp->qplib_qp.id);
	qp->qp_info_pdev_dentry = debugfs_create_file(qp_name, 0400,
						      rdev->pdev_qpinfo_dir,
						      qp,
						      &bnxt_re_qp_info_ops);
}

void bnxt_re_qp_info_rem_qpinfo(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp)
{
	debugfs_remove(qp->qp_info_pdev_dentry);
	qp->qp_info_pdev_dentry = NULL;

	kfree(qp->qp_data);
	qp->qp_data = NULL;
}

/* Clear the driver statistics maintained in the info file */
static ssize_t bnxt_re_info_debugfs_clear(struct file *fil, const char __user *u,
					  size_t size, loff_t *off)
{
	struct seq_file *m = fil->private_data;
	struct bnxt_re_dev *rdev = m->private;
	struct bnxt_re_res_cntrs *rsors;

	rsors = &rdev->stats.rsors;

	/* Clear the driver statistics only */
	atomic_set(&rsors->max_qp_count, atomic_read(&rsors->qp_count));
	atomic_set(&rsors->max_rc_qp_count, atomic_read(&rsors->rc_qp_count));
	atomic_set(&rsors->max_ud_qp_count, atomic_read(&rsors->ud_qp_count));
	atomic_set(&rsors->max_srq_count, atomic_read(&rsors->srq_count));
	atomic_set(&rsors->max_cq_count, atomic_read(&rsors->cq_count));
	atomic_set(&rsors->max_mr_count, atomic_read(&rsors->mr_count));
	atomic_set(&rsors->max_mr_dmabuf_count, atomic_read(&rsors->mr_dmabuf_count));
	atomic_set(&rsors->max_peer_mem_mr_count, atomic_read(&rsors->peer_mem_mr_count));
	atomic_set(&rsors->max_mw_count, atomic_read(&rsors->mw_count));
	atomic_set(&rsors->max_ah_count, atomic_read(&rsors->ah_count));
	atomic_set(&rsors->max_pd_count, atomic_read(&rsors->pd_count));
	atomic_set(&rsors->resize_count, 0);

	if (rdev->dbr_sw_stats) {
		rdev->dbr_sw_stats->dbq_int_recv = 0;
		rdev->dbr_sw_stats->dbq_int_en = 0;
		rdev->dbr_sw_stats->dbq_pacing_resched = 0;
		rdev->dbr_sw_stats->dbq_pacing_complete = 0;
		rdev->dbr_sw_stats->dbq_pacing_alerts = 0;

		rdev->dbr_evt_curr_epoch = 0;
		rdev->dbr_sw_stats->dbr_drop_recov_events = 0;
		rdev->dbr_sw_stats->dbr_drop_recov_timeouts = 0;
		rdev->dbr_sw_stats->dbr_drop_recov_timeout_users = 0;
		rdev->dbr_sw_stats->dbr_drop_recov_event_skips = 0;
	}

	return size;
}

/* Clear perf state irrespective value passed.
 * Any value written to debugfs entry will clear the stats
 */
static ssize_t bnxt_re_perf_debugfs_clear(struct file *fil, const char __user *u,
				     size_t size, loff_t *off)
{
	struct seq_file *m = fil->private_data;
	struct bnxt_re_dev *rdev = m->private;
	int i;

	if (!rdev->rcfw.sp_perf_stats_enabled)
		return size;

	for (i = 0; i < RCFW_MAX_STAT_INDEX; i++) {
		rdev->rcfw.qp_create_stats[i] = 0;
		rdev->rcfw.qp_destroy_stats[i] = 0;
		rdev->rcfw.mr_create_stats[i] = 0;
		rdev->rcfw.mr_destroy_stats[i] = 0;
		rdev->rcfw.qp_modify_stats[i] = 0;
	}

	rdev->rcfw.qp_create_stats_id = 0;
	rdev->rcfw.qp_destroy_stats_id = 0;
	rdev->rcfw.mr_create_stats_id = 0;
	rdev->rcfw.mr_destroy_stats_id = 0;
	rdev->rcfw.qp_modify_stats_id = 0;

	for (i = 0; i < RCFW_MAX_LATENCY_MSEC_SLAB_INDEX; i++)
		rdev->rcfw.rcfw_lat_slab_msec[i] = 0;

	return size;
}

/* Clear the driver debug statistics */
static ssize_t bnxt_re_drv_stats_debugfs_clear(struct file *fil, const char __user *u,
					       size_t size, loff_t *off)
{
	struct seq_file *m = fil->private_data;
	struct bnxt_re_dev *rdev = m->private;


	rdev->dbg_stats->dbq.fifo_occup_slab_1 = 0;
	rdev->dbg_stats->dbq.fifo_occup_slab_2 = 0;
	rdev->dbg_stats->dbq.fifo_occup_slab_3 = 0;
	rdev->dbg_stats->dbq.fifo_occup_slab_4 = 0;
	rdev->dbg_stats->dbq.fifo_occup_water_mark = 0;
	rdev->dbg_stats->dbq.do_pacing_slab_1 = 0;
	rdev->dbg_stats->dbq.do_pacing_slab_2 = 0;
	rdev->dbg_stats->dbq.do_pacing_slab_3 = 0;
	rdev->dbg_stats->dbq.do_pacing_slab_4 = 0;
	rdev->dbg_stats->dbq.do_pacing_slab_5 = 0;
	rdev->dbg_stats->dbq.do_pacing_water_mark = 0;
	rdev->dbg_stats->dbq.do_pacing_retry = 0;

	return size;
}

static void bnxt_re_print_roce_only_counters(struct bnxt_re_dev *rdev,
					     struct seq_file *s)
{
	struct bnxt_re_ro_counters *roce_only = &rdev->stats.dstat.cur[0];

	/* Do not polulate RoCE Only stats for VF from  Thor onwards */
	if (_is_chip_p5_plus(rdev->chip_ctx) && rdev->is_virtfn)
		return;

	seq_printf(s, "\tRoCE Only Rx Pkts: %llu\n", roce_only->rx_pkts);
	seq_printf(s, "\tRoCE Only Rx Bytes: %llu\n", roce_only->rx_bytes);
	seq_printf(s, "\tRoCE Only Tx Pkts: %llu\n", roce_only->tx_pkts);
	seq_printf(s, "\tRoCE Only Tx Bytes: %llu\n", roce_only->tx_bytes);
}

static void bnxt_re_print_normal_total_counters(struct bnxt_re_dev *rdev,
					      struct seq_file *s)
{

	if (_is_chip_p7_plus(rdev->chip_ctx) && rdev->is_virtfn) {
		struct bnxt_re_ro_counters *roce_only;

		roce_only = &rdev->stats.dstat.cur[0];
		seq_printf(s, "\tRx Pkts: %llu\n", roce_only->rx_pkts);
		seq_printf(s, "\tRx Bytes: %llu\n", roce_only->rx_bytes);
		seq_printf(s, "\tTx Pkts: %llu\n", roce_only->tx_pkts);
		seq_printf(s, "\tTx Bytes: %llu\n", roce_only->tx_bytes);
	} else if (_is_chip_gen_p5(rdev->chip_ctx) && rdev->is_virtfn) {
		struct bnxt_re_rdata_counters *rstat = &rdev->stats.dstat.rstat[0];

		/* Only for VF from Thor onwards */
		seq_printf(s, "\tRx Pkts: %llu\n", rstat->rx_ucast_pkts);
		seq_printf(s, "\tRx Bytes: %llu\n", rstat->rx_ucast_bytes);
		seq_printf(s, "\tTx Pkts: %llu\n", rstat->tx_ucast_pkts);
		seq_printf(s, "\tTx Bytes: %llu\n", rstat->tx_ucast_bytes);
	} else {
		struct bnxt_re_ro_counters *roce_only;
		struct bnxt_re_cc_stat *cnps;

		cnps = &rdev->stats.cnps;
		roce_only = &rdev->stats.dstat.cur[0];

		seq_printf(s, "\tRx Pkts: %llu\n", cnps->cur[0].cnp_rx_pkts +
			   roce_only->rx_pkts);
		seq_printf(s, "\tRx Bytes: %llu\n",
			   cnps->cur[0].cnp_rx_bytes + roce_only->rx_bytes);
		seq_printf(s, "\tTx Pkts: %llu\n",
			   cnps->cur[0].cnp_tx_pkts + roce_only->tx_pkts);
		seq_printf(s, "\tTx Bytes: %llu\n",
			   cnps->cur[0].cnp_tx_bytes + roce_only->tx_bytes);
	}
}

static void bnxt_re_print_bond_total_counters(struct bnxt_re_dev *rdev,
					      struct seq_file *s)
{
	struct bnxt_re_ro_counters *roce_only;
	struct bnxt_re_cc_stat *cnps;

	cnps = &rdev->stats.cnps;
	roce_only = &rdev->stats.dstat.cur[0];

	seq_printf(s, "\tRx Pkts: %llu\n",
		   cnps->cur[0].cnp_rx_pkts +
		   cnps->cur[1].cnp_rx_pkts +
		   roce_only[0].rx_pkts +
		   roce_only[1].rx_pkts);

	seq_printf(s, "\tRx Bytes: %llu\n",
		   cnps->cur[0].cnp_rx_bytes +
		   cnps->cur[1].cnp_rx_bytes +
		   roce_only[0].rx_bytes +
		   roce_only[1].rx_bytes);

	seq_printf(s, "\tTx Pkts: %llu\n",
		   cnps->cur[0].cnp_tx_pkts +
		   cnps->cur[1].cnp_tx_pkts +
		   roce_only[0].tx_pkts +
		   roce_only[1].tx_pkts);

	seq_printf(s, "\tTx Bytes: %llu\n",
		   cnps->cur[0].cnp_tx_bytes +
		   cnps->cur[1].cnp_tx_bytes +
		   roce_only[0].tx_bytes +
		   roce_only[1].tx_bytes);

	/* Disable per port stat display for p5 onwards. */
	if (_is_chip_p5_plus(rdev->chip_ctx))
		return;
	seq_printf(s, "\tRx Pkts P0: %llu\n",
		   cnps->cur[0].cnp_rx_pkts + roce_only[0].rx_pkts);
	seq_printf(s, "\tRx Bytes P0: %llu\n",
		   cnps->cur[0].cnp_rx_bytes + roce_only[0].rx_bytes);
	seq_printf(s, "\tTx Pkts P0: %llu\n",
		   cnps->cur[0].cnp_tx_pkts + roce_only[0].tx_pkts);
	seq_printf(s, "\tTx Bytes P0: %llu\n",
		   cnps->cur[0].cnp_tx_bytes + roce_only[0].tx_bytes);

	seq_printf(s, "\tRx Pkts P1: %llu\n",
		   cnps->cur[1].cnp_rx_pkts + roce_only[1].rx_pkts);
	seq_printf(s, "\tRx Bytes P1: %llu\n",
		   cnps->cur[1].cnp_rx_bytes + roce_only[1].rx_bytes);
	seq_printf(s, "\tTx Pkts P1: %llu\n",
		   cnps->cur[1].cnp_tx_pkts + roce_only[1].tx_pkts);
	seq_printf(s, "\tTx Bytes P1: %llu\n",
		   cnps->cur[1].cnp_tx_bytes + roce_only[1].tx_bytes);
}

static void bnxt_re_print_bond_roce_only_counters(struct bnxt_re_dev *rdev,
					     struct seq_file *s)
{
	struct bnxt_re_ro_counters *roce_only;

	roce_only = rdev->stats.dstat.cur;
	seq_printf(s, "\tRoCE Only Rx Pkts: %llu\n" ,roce_only[0].rx_pkts +
			roce_only[1].rx_pkts);
	seq_printf(s, "\tRoCE Only Rx Bytes: %llu\n", roce_only[0].rx_bytes +
			roce_only[1].rx_bytes);
	seq_printf(s, "\tRoCE Only Tx Pkts: %llu\n", roce_only[0].tx_pkts +
			roce_only[1].tx_pkts);
	seq_printf(s, "\tRoCE Only Tx Bytes: %llu\n", roce_only[0].tx_bytes +
			roce_only[1].tx_bytes);

	/* Disable per port stat display for p5 onwards. */
	if (_is_chip_p5_plus(rdev->chip_ctx))
		return;
	seq_printf(s, "\tRoCE Only Rx Pkts P0: %llu\n", roce_only[0].rx_pkts);
	seq_printf(s, "\tRoCE Only Rx Bytes P0: %llu\n", roce_only[0].rx_bytes);
	seq_printf(s, "\tRoCE Only Tx Pkts P0: %llu\n", roce_only[0].tx_pkts);
	seq_printf(s, "\tRoCE Only Tx Bytes P0: %llu\n", roce_only[0].tx_bytes);

	seq_printf(s, "\tRoCE Only Rx Pkts P1: %llu\n", roce_only[1].rx_pkts);
	seq_printf(s, "\tRoCE Only Rx Bytes P1: %llu\n", roce_only[1].rx_bytes);
	seq_printf(s, "\tRoCE Only Tx Pkts P1: %llu\n", roce_only[1].tx_pkts);
	seq_printf(s, "\tRoCE Only Tx Bytes P1: %llu\n", roce_only[1].tx_bytes);
}

static void bnxt_re_print_bond_counters(struct bnxt_re_dev *rdev,
					struct seq_file *s)
{
	struct bnxt_qplib_roce_stats *roce_stats;
	struct bnxt_re_rdata_counters *stats1;
	struct bnxt_re_rdata_counters *stats2;
	struct bnxt_re_cc_stat *cnps;
	long long oob_cnt = 0;
	bool en_disp;

	roce_stats = &rdev->stats.dstat.errs;
	stats1 = &rdev->stats.dstat.rstat[0];
	stats2 = &rdev->stats.dstat.rstat[1];
	cnps = &rdev->stats.cnps;
	en_disp = !_is_chip_p5_plus(rdev->chip_ctx);

	seq_printf(s, "\tActive QPs P0: %lld\n", roce_stats->active_qp_count_p0);
	seq_printf(s, "\tActive QPs P1: %lld\n", roce_stats->active_qp_count_p1);

	bnxt_re_print_bond_total_counters(rdev, s);

	seq_printf(s, "\tCNP Tx Pkts: %llu\n",
		   cnps->cur[0].cnp_tx_pkts + cnps->cur[1].cnp_tx_pkts);
	if (en_disp)
		seq_printf(s, "\tCNP Tx Bytes: %llu\n",
			   cnps->cur[0].cnp_tx_bytes +
			   cnps->cur[1].cnp_tx_bytes);
	seq_printf(s, "\tCNP Rx Pkts: %llu\n",
		   cnps->cur[0].cnp_rx_pkts + cnps->cur[1].cnp_rx_pkts);
	if (en_disp)
		seq_printf(s, "\tCNP Rx Bytes: %llu\n",
			   cnps->cur[0].cnp_rx_bytes +
			   cnps->cur[1].cnp_rx_bytes);

	seq_printf(s, "\tCNP Tx Pkts P0: %llu\n", cnps->cur[0].cnp_tx_pkts);
	if (en_disp)
		seq_printf(s, "\tCNP Tx Bytes P0: %llu\n",
			   cnps->cur[0].cnp_tx_bytes);
	seq_printf(s, "\tCNP Rx Pkts P0: %llu\n", cnps->cur[0].cnp_rx_pkts);
	if (en_disp)
		seq_printf(s, "\tCNP Rx Bytes P0: %llu\n",
			   cnps->cur[0].cnp_rx_bytes);
	seq_printf(s, "\tCNP Tx Pkts P1: %llu\n", cnps->cur[1].cnp_tx_pkts);
	if (en_disp)
		seq_printf(s, "\tCNP Tx Bytes P1: %llu\n",
			   cnps->cur[1].cnp_tx_bytes);
	seq_printf(s, "\tCNP Rx Pkts P1: %llu\n", cnps->cur[1].cnp_rx_pkts);
	if (en_disp)
		seq_printf(s, "\tCNP Rx Bytes P1: %llu\n",
			   cnps->cur[1].cnp_rx_bytes);
	/* Print RoCE only bytes.. CNP counters include RoCE packets also */
	bnxt_re_print_bond_roce_only_counters(rdev, s);


	seq_printf(s, "\trx_roce_error_pkts: %lld\n",
		   (stats1 ? stats1->rx_error_pkts : 0) +
		   (stats2 ? stats2->rx_error_pkts : 0));
	seq_printf(s, "\trx_roce_discard_pkts: %lld\n",
		   (stats1 ? stats1->rx_discard_pkts : 0) +
		   (stats2 ? stats2->rx_discard_pkts : 0));
	if (!en_disp) {
		/* show only for Gen P5 or higher */
		seq_printf(s, "\ttx_roce_error_pkts: %lld\n",
			   (stats1 ? stats1->tx_error_pkts : 0) +
			   (stats2 ? stats2->tx_error_pkts : 0));
		seq_printf(s, "\ttx_roce_discard_pkts: %lld\n",
			   (stats1 ? stats1->tx_discard_pkts : 0) +
			   (stats2 ? stats2->tx_discard_pkts : 0));
	}
	/* No need to sum-up both port stat counts in bond mode */
	if (bnxt_ext_stats_supported(rdev->chip_ctx, rdev->dev_attr->dev_cap_flags,
				     rdev->is_virtfn)) {
		seq_printf(s, "\tres_oob_drop_count: %lld\n",
			   rdev->stats.dstat.e_errs.oob);
		bnxt_re_print_ext_stat(rdev, s);
	} else {
		oob_cnt = (stats1 ? stats1->rx_discard_pkts : 0) +
			(stats2 ? stats2->rx_discard_pkts : 0) -
			rdev->stats.dstat.errs.res_oos_drop_count;

		/*
		 * oob count is calculated from the output of two separate
		 * HWRM commands. To avoid reporting inconsistent values
		 * due to the time delta between two different queries,
		 * report newly calculated value only if it is more than the
		 * previously reported OOB value.
		 */
		if (oob_cnt < rdev->stats.dstat.prev_oob)
			oob_cnt = rdev->stats.dstat.prev_oob;
		seq_printf(s, "\tres_oob_drop_count: %lld\n", oob_cnt);
		rdev->stats.dstat.prev_oob = oob_cnt;
	}
}

static void bnxt_re_print_ext_stat(struct bnxt_re_dev *rdev,
				   struct seq_file *s)
{
	struct bnxt_re_ext_rstat *ext_s;
	struct bnxt_re_cc_stat *cnps;

	ext_s = &rdev->stats.dstat.ext_rstat[0];
	cnps = &rdev->stats.cnps;

	seq_printf(s, "\ttx_atomic_req: %llu\n", ext_s->tx.atomic_req);
	seq_printf(s, "\trx_atomic_req: %llu\n", ext_s->rx.atomic_req);
	seq_printf(s, "\ttx_read_req: %llu\n", ext_s->tx.read_req);
	seq_printf(s, "\ttx_read_resp: %llu\n", ext_s->tx.read_resp);
	seq_printf(s, "\trx_read_req: %llu\n", ext_s->rx.read_req);
	seq_printf(s, "\trx_read_resp: %llu\n", ext_s->rx.read_resp);
	seq_printf(s, "\ttx_write_req: %llu\n", ext_s->tx.write_req);
	seq_printf(s, "\trx_write_req: %llu\n", ext_s->rx.write_req);
	seq_printf(s, "\ttx_send_req: %llu\n", ext_s->tx.send_req);
	seq_printf(s, "\trx_send_req: %llu\n", ext_s->rx.send_req);
	if (bnxt_ext_stats_v2_supported(rdev->dev_attr->dev_cap_ext_flags)) {
		seq_printf(s, "\ttx_send_req_rc: %llu\n", ext_s->tx.send_req_rc);
		seq_printf(s, "\ttx_send_req_ud: %llu\n", ext_s->tx.send_req_ud);
		seq_printf(s, "\trx_send_req_rc: %llu\n", ext_s->rx.send_req_rc);
		seq_printf(s, "\trx_send_req_ud: %llu\n", ext_s->rx.send_req_ud);
	}
	seq_printf(s, "\trx_good_pkts: %llu\n", ext_s->grx.rx_pkts);
	seq_printf(s, "\trx_good_bytes: %llu\n", ext_s->grx.rx_bytes);
	if (_is_chip_p7_plus(rdev->chip_ctx)) {
		seq_printf(s, "\trx_dcn_payload_cut: %llu\n", ext_s->rx_dcn_payload_cut);
		seq_printf(s, "\tte_bypassed: %llu\n", ext_s->te_bypassed);
		if (BNXT_RE_DCN_ENABLED(rdev->rcfw.res)) {
			seq_printf(s, "\ttx_dcn_cnp: %llu\n", ext_s->tx_dcn_cnp);
			seq_printf(s, "\trx_dcn_cnp: %llu\n", ext_s->rx_dcn_cnp);
			seq_printf(s, "\trx_payload_cut: %llu\n", ext_s->rx_payload_cut);
			seq_printf(s, "\trx_payload_cut_ignored: %llu\n",
				   ext_s->rx_payload_cut_ignored);
			seq_printf(s, "\trx_dcn_cnp_ignored: %llu\n",
				   ext_s->rx_dcn_cnp_ignored);
		}
	}

	if (rdev->binfo) {
		seq_printf(s, "\trx_ecn_marked_pkts: %llu\n",
			   cnps->cur[0].ecn_marked + cnps->cur[1].ecn_marked);
		seq_printf(s, "\trx_ecn_marked_pkts P0: %llu\n", cnps->cur[0].ecn_marked);
		seq_printf(s, "\trx_ecn_marked_pkts P1: %llu\n", cnps->cur[1].ecn_marked);
	} else {
		seq_printf(s, "\trx_ecn_marked_pkts: %llu\n", cnps->cur[0].ecn_marked);
	}
}

static void bnxt_re_print_rawqp_stats(struct bnxt_re_dev *rdev,
				      struct seq_file *s)
{
	struct bnxt_re_rdata_counters *stats;
	u64 tot_pkts, tot_bytes;

	if (!rdev->rcfw.roce_mirror)
		return;

	stats = &rdev->stats.dstat.rstat[2];
	tot_pkts = stats->rx_ucast_pkts + stats->rx_mcast_pkts + stats->rx_bcast_pkts;
	tot_bytes = stats->rx_ucast_bytes + stats->rx_mcast_bytes + stats->rx_bcast_bytes;

	seq_printf(s, "\tRoCE Mirror Rx Pkts: %llu\n", tot_pkts);
	seq_printf(s, "\tRoCE Mirror Rx Bytes: %llu\n", tot_bytes);
	seq_printf(s, "\tRoCE Mirror rx_discard_pkts: %llu\n", stats->rx_discard_pkts);
	seq_printf(s, "\tRoCE Mirror rx_error_pkts: %llu\n", stats->rx_error_pkts);
}

static void bnxt_re_print_normal_counters(struct bnxt_re_dev *rdev,
					  struct seq_file *s)
{
	struct bnxt_re_rdata_counters *stats;
	struct bnxt_re_cc_stat *cnps;
	bool en_disp;

	stats = &rdev->stats.dstat.rstat[0];
	cnps = &rdev->stats.cnps;
	en_disp = !_is_chip_p5_plus(rdev->chip_ctx);

	bnxt_re_print_normal_total_counters(rdev, s);
	if (!rdev->is_virtfn) {
		seq_printf(s, "\tCNP Tx Pkts: %llu\n",
			   cnps->cur[0].cnp_tx_pkts);
		if (en_disp)
			seq_printf(s, "\tCNP Tx Bytes: %llu\n",
				   cnps->cur[0].cnp_tx_bytes);
		seq_printf(s, "\tCNP Rx Pkts: %llu\n",
			   cnps->cur[0].cnp_rx_pkts);
		if (en_disp)
			seq_printf(s, "\tCNP Rx Bytes: %llu\n",
				   cnps->cur[0].cnp_rx_bytes);
	}
	/* Print RoCE only bytes.. CNP counters include RoCE packets also */
	bnxt_re_print_roce_only_counters(rdev, s);

	seq_printf(s, "\trx_roce_error_pkts: %lld\n",
		   stats ? stats->rx_error_pkts : 0);
	seq_printf(s, "\trx_roce_discard_pkts: %lld\n",
		   stats ? stats->rx_discard_pkts : 0);
	if (!en_disp) {
		seq_printf(s, "\ttx_roce_error_pkts: %lld\n",
			   stats ? stats->tx_error_pkts : 0);
		seq_printf(s, "\ttx_roce_discards_pkts: %lld\n",
			   stats ? stats->tx_discard_pkts : 0);
	}

	if (bnxt_ext_stats_supported(rdev->chip_ctx,
				     rdev->dev_attr->dev_cap_flags,
				     rdev->is_virtfn) ||
	    bnxt_ext_stats_v2_supported(rdev->dev_attr->dev_cap_ext_flags)) {
		seq_printf(s, "\tres_oob_drop_count: %lld\n",
			   rdev->stats.dstat.e_errs.oob);
		bnxt_re_print_ext_stat(rdev, s);
	}
}

static int bnxt_re_info_debugfs_show(struct seq_file *s, void *unused)
{
	struct bnxt_re_dev *rdev = s->private;
	struct bnxt_re_ext_roce_stats *e_errs;
	struct bnxt_re_rdata_counters *rstat;
	struct bnxt_qplib_roce_stats *errs;
	int sched_msec, i, ext_stats;
	unsigned long tstamp_diff;
	struct pci_dev *pdev;
	int rc = 0;

	seq_printf(s, "bnxt_re debug info:\n");

	if (!bnxt_re_is_rdev_valid(rdev)) {
		rc = -ENODEV;
		goto err;
	}

	pdev = rdev->en_dev->pdev;

	errs = &rdev->stats.dstat.errs;
	rstat = &rdev->stats.dstat.rstat[0];
	e_errs = &rdev->stats.dstat.e_errs;
	sched_msec = BNXT_RE_STATS_CTX_UPDATE_TIMER;
	tstamp_diff = jiffies - rdev->stats.read_tstamp;
	if (test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags)) {
		if (restrict_stats && tstamp_diff <
		    msecs_to_jiffies(sched_msec))
			goto skip_query;
		rc = bnxt_re_get_device_stats(rdev);
		if (rc)
			dev_err(rdev_to_dev(rdev),
				"Failed to query device stats\n");
		rdev->stats.read_tstamp = jiffies;
	}
skip_query:
	seq_printf(s, "=====[ IBDEV %s ]=============================\n",
		   rdev->ibdev.name);
	if (rdev->netdev)
		seq_printf(s, "\tlink state: %s\n",
			   bnxt_re_link_state_str(rdev));
	seq_printf(s, "\tMax QP:\t\t%d\n", rdev->dev_attr->max_qp);
	seq_printf(s, "\tMax SRQ:\t%d\n", rdev->dev_attr->max_srq);
	seq_printf(s, "\tMax CQ:\t\t%d\n", rdev->dev_attr->max_cq);
	seq_printf(s, "\tMax MR:\t\t%d\n", rdev->dev_attr->max_mr);
	seq_printf(s, "\tMax MW:\t\t%d\n", rdev->dev_attr->max_mw);
	seq_printf(s, "\tMax AH:\t\t%d\n", rdev->dev_attr->max_ah);
	seq_printf(s, "\tMax PD:\t\t%d\n", rdev->dev_attr->max_pd);
	seq_printf(s, "\tActive QP:\t%d\n",
		   atomic_read(&rdev->stats.rsors.qp_count));
	seq_printf(s, "\tActive RC QP:\t%d\n",
		   atomic_read(&rdev->stats.rsors.rc_qp_count));
	seq_printf(s, "\tActive UD QP:\t%d\n",
		   atomic_read(&rdev->stats.rsors.ud_qp_count));
	seq_printf(s, "\tActive SRQ:\t%d\n",
		   atomic_read(&rdev->stats.rsors.srq_count));
	seq_printf(s, "\tActive CQ:\t%d\n",
		   atomic_read(&rdev->stats.rsors.cq_count));
	seq_printf(s, "\tActive MR:\t%d\n",
		   atomic_read(&rdev->stats.rsors.mr_count));
	seq_printf(s, "\tActive DMABUF MR: %d\n",
		   atomic_read(&rdev->stats.rsors.mr_dmabuf_count));
	seq_printf(s, "\tActive PEERMEM MR: %d\n",
		   atomic_read(&rdev->stats.rsors.peer_mem_mr_count));
	seq_printf(s, "\tActive MW:\t%d\n",
		   atomic_read(&rdev->stats.rsors.mw_count));
	seq_printf(s, "\tActive AH:\t%d\n",
		   atomic_read(&rdev->stats.rsors.ah_count));
	seq_printf(s, "\tActive HW AH:\t%d\n",
		   atomic_read(&rdev->stats.rsors.ah_hw_count));
	seq_printf(s, "\tActive PD:\t%d\n",
		   atomic_read(&rdev->stats.rsors.pd_count));
	seq_printf(s, "\tQP Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_qp_count));
	seq_printf(s, "\tRC QP Watermark: %d\n",
		   atomic_read(&rdev->stats.rsors.max_rc_qp_count));
	seq_printf(s, "\tUD QP Watermark: %d\n",
		   atomic_read(&rdev->stats.rsors.max_ud_qp_count));
	seq_printf(s, "\tSRQ Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_srq_count));
	seq_printf(s, "\tCQ Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_cq_count));
	seq_printf(s, "\tMR Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_mr_count));
	seq_printf(s, "\tDMABUF MR Watermark: %d\n",
		   atomic_read(&rdev->stats.rsors.max_mr_dmabuf_count));
	seq_printf(s, "\tPEERMEM MR Watermark: %d\n",
		   atomic_read(&rdev->stats.rsors.max_peer_mem_mr_count));
	seq_printf(s, "\tMW Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_mw_count));
	seq_printf(s, "\tAH Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_ah_count));
	seq_printf(s, "\tAH HW Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_ah_hw_count));
	seq_printf(s, "\tPD Watermark:\t%d\n",
		   atomic_read(&rdev->stats.rsors.max_pd_count));
	seq_printf(s, "\tResize CQ count: %d\n",
		   atomic_read(&rdev->stats.rsors.resize_count));
	seq_printf(s, "\tRecoverable Errors: %lld\n",
		   rstat ? rstat->tx_bcast_pkts : 0);

	if (rdev->binfo)
		bnxt_re_print_bond_counters(rdev, s);
	else
		bnxt_re_print_normal_counters(rdev, s);

	if (_is_modify_qp_rate_limit_supported(rdev->dev_attr->dev_cap_ext_flags2)) {
		seq_printf(s, "\tRate limit min(kbps): %d\n", rdev->dev_attr->rate_limit_min);
		seq_printf(s, "\tRate limit max(kbps): %d\n", rdev->dev_attr->rate_limit_max);
	}
	seq_printf(s, "\tmax_retry_exceeded: %llu\n", errs->max_retry_exceeded);
	/* For HW req retx pick from extended stats */
	ext_stats = bnxt_ext_stats_supported(rdev->chip_ctx, rdev->dev_attr->dev_cap_flags,
					     rdev->is_virtfn);
	if (_is_hw_req_retx_supported(rdev->dev_attr->dev_cap_flags) && ext_stats) {
		seq_printf(s, "\tto_retransmits: %llu\n", e_errs->to_retransmits);
		seq_printf(s, "\tseq_err_naks_rcvd: %llu\n", e_errs->seq_err_naks_rcvd);
		seq_printf(s, "\trnr_naks_rcvd: %llu\n", e_errs->rnr_naks_rcvd);
		seq_printf(s, "\tmissing_resp: %llu\n", e_errs->missing_resp);
	} else {
		seq_printf(s, "\tto_retransmits: %llu\n", errs->to_retransmits);
		seq_printf(s, "\tseq_err_naks_rcvd: %llu\n", errs->seq_err_naks_rcvd);
		seq_printf(s, "\trnr_naks_rcvd: %llu\n", errs->rnr_naks_rcvd);
		seq_printf(s, "\tmissing_resp: %llu\n", errs->missing_resp);
	}

	/* For HW res retx pick from extended stats */
	if (_is_hw_resp_retx_supported(rdev->dev_attr->dev_cap_flags) && ext_stats)
		seq_printf(s, "\tdup_req: %llu\n", e_errs->dup_req);
	else
		seq_printf(s, "\tdup_req: %llu\n", errs->dup_req);

	seq_printf(s, "\tunrecoverable_err: %llu\n", errs->unrecoverable_err);
	seq_printf(s, "\tbad_resp_err: %llu\n", errs->bad_resp_err);
	seq_printf(s, "\tlocal_qp_op_err: %llu\n", errs->local_qp_op_err);
	seq_printf(s, "\tlocal_protection_err: %llu\n", errs->local_protection_err);
	seq_printf(s, "\tmem_mgmt_op_err: %llu\n", errs->mem_mgmt_op_err);
	seq_printf(s, "\tremote_invalid_req_err: %llu\n", errs->remote_invalid_req_err);
	seq_printf(s, "\tremote_access_err: %llu\n", errs->remote_access_err);
	seq_printf(s, "\tremote_op_err: %llu\n", errs->remote_op_err);
	seq_printf(s, "\tres_exceed_max: %llu\n", errs->res_exceed_max);
	seq_printf(s, "\tres_length_mismatch: %llu\n", errs->res_length_mismatch);
	seq_printf(s, "\tres_exceeds_wqe: %llu\n", errs->res_exceeds_wqe);
	seq_printf(s, "\tres_opcode_err: %llu\n", errs->res_opcode_err);
	seq_printf(s, "\tres_rx_invalid_rkey: %llu\n", errs->res_rx_invalid_rkey);
	seq_printf(s, "\tres_rx_domain_err: %llu\n", errs->res_rx_domain_err);
	seq_printf(s, "\tres_rx_no_perm: %llu\n", errs->res_rx_no_perm);
	seq_printf(s, "\tres_rx_range_err: %llu\n", errs->res_rx_range_err);
	seq_printf(s, "\tres_tx_invalid_rkey: %llu\n", errs->res_tx_invalid_rkey);
	seq_printf(s, "\tres_tx_domain_err: %llu\n", errs->res_tx_domain_err);
	seq_printf(s, "\tres_tx_no_perm: %llu\n", errs->res_tx_no_perm);
	seq_printf(s, "\tres_tx_range_err: %llu\n", errs->res_tx_range_err);
	seq_printf(s, "\tres_irrq_oflow: %llu\n", errs->res_irrq_oflow);
	seq_printf(s, "\tres_unsup_opcode: %llu\n", errs->res_unsup_opcode);
	seq_printf(s, "\tres_unaligned_atomic: %llu\n", errs->res_unaligned_atomic);
	seq_printf(s, "\tres_rem_inv_err: %llu\n", errs->res_rem_inv_err);
	seq_printf(s, "\tres_mem_error64: %llu\n", errs->res_mem_error);
	seq_printf(s, "\tres_srq_err: %llu\n", errs->res_srq_err);
	seq_printf(s, "\tres_cmp_err: %llu\n", errs->res_cmp_err);
	seq_printf(s, "\tres_invalid_dup_rkey: %llu\n", errs->res_invalid_dup_rkey);
	seq_printf(s, "\tres_wqe_format_err: %llu\n", errs->res_wqe_format_err);
	seq_printf(s, "\tres_cq_load_err: %llu\n", errs->res_cq_load_err);
	seq_printf(s, "\tres_srq_load_err: %llu\n", errs->res_srq_load_err);
	seq_printf(s, "\tres_tx_pci_err: %llu\n", errs->res_tx_pci_err);
	seq_printf(s, "\tres_rx_pci_err: %llu\n", errs->res_rx_pci_err);
	if (bnxt_ext_stats_supported(rdev->chip_ctx, rdev->dev_attr->dev_cap_flags,
				     rdev->is_virtfn)) {
		seq_printf(s, "\tres_oos_drop_count: %llu\n",
			   e_errs->oos);
	} else {
		/* Display on function 0 as OOS counters are chip-wide */
		if (PCI_FUNC(pdev->devfn) == 0)
			seq_printf(s, "\tres_oos_drop_count: %llu\n",
					errs->res_oos_drop_count);
	}

	seq_printf(s, "\tnum_irq_started : %u\n", rdev->rcfw.num_irq_started);
	seq_printf(s, "\tnum_irq_stopped : %u\n", rdev->rcfw.num_irq_stopped);
	seq_printf(s, "\tpoll_in_intr_en : %u\n", rdev->rcfw.poll_in_intr_en);
	seq_printf(s, "\tpoll_in_intr_dis : %u\n", rdev->rcfw.poll_in_intr_dis);
	seq_printf(s, "\tcmdq_full_dbg_cnt : %u\n", rdev->rcfw.cmdq_full_dbg);
	if (!rdev->is_virtfn)
		seq_printf(s, "\tfw_service_prof_type_sup : %u\n",
			   is_qport_service_type_supported(rdev));
	if (rdev->dbr_pacing) {
		seq_printf(s, "\tdbq_int_recv: %llu\n", rdev->dbr_sw_stats->dbq_int_recv);
		if (!_is_chip_p7_plus(rdev->chip_ctx))
			seq_printf(s, "\tdbq_int_en: %llu\n", rdev->dbr_sw_stats->dbq_int_en);
		seq_printf(s, "\tdbq_pacing_resched: %llu\n",
			   rdev->dbr_sw_stats->dbq_pacing_resched);
		seq_printf(s, "\tdbq_pacing_complete: %llu\n",
			   rdev->dbr_sw_stats->dbq_pacing_complete);
		seq_printf(s, "\tdbq_pacing_alerts: %llu\n",
			   rdev->dbr_sw_stats->dbq_pacing_alerts);
		seq_printf(s, "\tdbq_dbr_fifo_reg: 0x%x\n",
			   readl(rdev->en_dev->bar0 + rdev->dbr_db_fifo_reg_off));
	}

	if (rdev->dbr_drop_recov) {
		seq_printf(s, "\tdbr_drop_recov_epoch: %d\n",
			   rdev->dbr_evt_curr_epoch);
		seq_printf(s, "\tdbr_drop_recov_events: %lld\n",
			   rdev->dbr_sw_stats->dbr_drop_recov_events);
		seq_printf(s, "\tdbr_drop_recov_timeouts: %lld\n",
			   rdev->dbr_sw_stats->dbr_drop_recov_timeouts);
		seq_printf(s, "\tdbr_drop_recov_timeout_users: %lld\n",
			   rdev->dbr_sw_stats->dbr_drop_recov_timeout_users);
		seq_printf(s, "\tdbr_drop_recov_event_skips: %lld\n",
			   rdev->dbr_sw_stats->dbr_drop_recov_event_skips);
	}

	if (BNXT_RE_PPP_ENABLED(rdev->chip_ctx)) {
		seq_printf(s, "\tppp_enabled_contexts: %d\n",
			   rdev->ppp_stats.ppp_enabled_ctxs);
		seq_printf(s, "\tppp_enabled_qps: %d\n",
			   rdev->ppp_stats.ppp_enabled_qps);
	}

	for (i = 0; i < RCFW_MAX_LATENCY_SEC_SLAB_INDEX; i++) {
		if (rdev->rcfw.rcfw_lat_slab_sec[i])
			seq_printf(s, "\tlatency_slab [%d - %d] sec = %d\n",
				i, i + 1, rdev->rcfw.rcfw_lat_slab_sec[i]);
	}

	seq_printf(s, "\n");
err:
	return rc;
}

static int bnxt_re_perf_debugfs_show(struct seq_file *s, void *unused)
{
	u64 qp_create_total_msec = 0, qp_destroy_total_msec = 0;
	u64 mr_create_total_msec = 0, mr_destroy_total_msec = 0;
	int qp_create_total = 0, qp_destroy_total = 0;
	int mr_create_total = 0, mr_destroy_total = 0;
	u64 qp_modify_err_total_msec = 0;
	int qp_modify_err_total = 0;
	struct bnxt_re_dev *rdev;
	bool add_entry = false;
	int i;

	rdev = s->private;
	seq_printf(s, "bnxt_re perf stats: %s shadow qd %d Driver Version - %s\n",
		   rdev->rcfw.sp_perf_stats_enabled ? "Enabled" : "Disabled",
		   rdev->rcfw.curr_shadow_qd,
		   ROCE_DRV_MODULE_VERSION);

	if (!rdev->rcfw.sp_perf_stats_enabled)
		return -ENOMEM;

	for (i = 0; i < RCFW_MAX_LATENCY_MSEC_SLAB_INDEX; i++) {
		if (rdev->rcfw.rcfw_lat_slab_msec[i])
			seq_printf(s, "\tlatency_slab [%d - %d] msec = %d\n",
				i, i + 1, rdev->rcfw.rcfw_lat_slab_msec[i]);
	}

	if (!bnxt_re_is_rdev_valid(rdev))
		return -ENODEV;

	for (i = 0; i < RCFW_MAX_STAT_INDEX; i++) {
		if (rdev->rcfw.qp_create_stats[i] > 0) {
			qp_create_total++;
			qp_create_total_msec += rdev->rcfw.qp_create_stats[i];
			add_entry = true;
		}
		if (rdev->rcfw.qp_destroy_stats[i] > 0) {
			qp_destroy_total++;
			qp_destroy_total_msec += rdev->rcfw.qp_destroy_stats[i];
			add_entry = true;
		}
		if (rdev->rcfw.mr_create_stats[i] > 0) {
			mr_create_total++;
			mr_create_total_msec += rdev->rcfw.mr_create_stats[i];
			add_entry = true;
		}
		if (rdev->rcfw.mr_destroy_stats[i] > 0) {
			mr_destroy_total++;
			mr_destroy_total_msec += rdev->rcfw.mr_destroy_stats[i];
			add_entry = true;
		}
		if (rdev->rcfw.qp_modify_stats[i] > 0) {
			qp_modify_err_total++;
			qp_modify_err_total_msec += rdev->rcfw.qp_modify_stats[i];
			add_entry = true;
		}

		if (add_entry)
			seq_printf(s, "<qp_create> %lld <qp_destroy> %lld <mr_create> %lld "
				      "<mr_destroy> %lld <qp_modify_to_err> %lld\n",
				      rdev->rcfw.qp_create_stats[i],
				      rdev->rcfw.qp_destroy_stats[i],
				      rdev->rcfw.mr_create_stats[i],
				      rdev->rcfw.mr_destroy_stats[i],
				      rdev->rcfw.qp_modify_stats[i]);

		add_entry = false;
	}

	seq_printf(s, "Total qp_create %d in msec %lld\n",
		   qp_create_total, qp_create_total_msec);
	seq_printf(s, "Total qp_destroy %d in msec %lld\n",
		   qp_destroy_total, qp_destroy_total_msec);
	seq_printf(s, "Total mr_create %d in msec %lld\n",
		   mr_create_total, mr_create_total_msec);
	seq_printf(s, "Total mr_destroy %d in msec %lld\n",
		   mr_destroy_total, mr_destroy_total_msec);
	seq_printf(s, "Total qp_modify_err_total %d in msec %lld\n",
		   qp_modify_err_total, qp_modify_err_total_msec);
	seq_puts(s, "\n");

	return 0;
}

static int bnxt_re_drv_stats_debugfs_show(struct seq_file *s, void *unused)
{
	struct bnxt_re_dev *rdev = s->private;
	int rc = 0;

	seq_puts(s, "bnxt_re debug stats:\n");


	seq_printf(s, "=====[ IBDEV %s ]=============================\n",
		   rdev->ibdev.name);
	if (rdev->dbr_pacing) {
		seq_printf(s, "\tdbq_fifo_occup_slab_1: %llu\n",
			   rdev->dbg_stats->dbq.fifo_occup_slab_1);
		seq_printf(s, "\tdbq_fifo_occup_slab_2: %llu\n",
			   rdev->dbg_stats->dbq.fifo_occup_slab_2);
		seq_printf(s, "\tdbq_fifo_occup_slab_3: %llu\n",
			   rdev->dbg_stats->dbq.fifo_occup_slab_3);
		seq_printf(s, "\tdbq_fifo_occup_slab_4: %llu\n",
			   rdev->dbg_stats->dbq.fifo_occup_slab_4);
		seq_printf(s, "\tdbq_fifo_occup_water_mark: %llu\n",
			   rdev->dbg_stats->dbq.fifo_occup_water_mark);
		seq_printf(s, "\tdbq_do_pacing_slab_1: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_slab_1);
		seq_printf(s, "\tdbq_do_pacing_slab_2: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_slab_2);
		seq_printf(s, "\tdbq_do_pacing_slab_3: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_slab_3);
		seq_printf(s, "\tdbq_do_pacing_slab_4: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_slab_4);
		seq_printf(s, "\tdbq_do_pacing_slab_5: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_slab_5);
		seq_printf(s, "\tdbq_do_pacing_water_mark: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_water_mark);
		seq_printf(s, "\tdbq_do_pacing_retry: %llu\n",
			   rdev->dbg_stats->dbq.do_pacing_retry);
		seq_printf(s, "\tmad_consumed: %llu\n",
			   rdev->dbg_stats->mad.mad_consumed);
		seq_printf(s, "\tmad_processed: %llu\n",
			   rdev->dbg_stats->mad.mad_processed);
	}
	seq_printf(s, "\treq_retransmission: %s\n",
		   BNXT_RE_HW_REQ_RETX(rdev->dev_attr->dev_cap_flags) ?
		   "Hardware" : "Firmware");
	seq_printf(s, "\tresp_retransmission: %s\n",
		   BNXT_RE_HW_RESP_RETX(rdev->dev_attr->dev_cap_flags) ?
		   "Hardware" : "Firmware");
	/* show wqe mode */
	seq_printf(s, "\tsq_wqe_mode: %d\n", rdev->chip_ctx->modes.wqe_mode);
	seq_printf(s, "\tsnapdump_context_saved: %d\n",
		   atomic_read(&rdev->dbg_stats->snapdump.context_saved_count));
	seq_puts(s, "\n");

	return rc;
}

static int bnxt_re_info_debugfs_open(struct inode *inode, struct file *file)
{
	struct bnxt_re_dev *rdev = inode->i_private;

	return single_open(file, bnxt_re_info_debugfs_show, rdev);
}

static int bnxt_re_perf_debugfs_open(struct inode *inode, struct file *file)
{
	struct bnxt_re_dev *rdev = inode->i_private;

	return single_open(file, bnxt_re_perf_debugfs_show, rdev);
}

static int bnxt_re_drv_stats_debugfs_open(struct inode *inode, struct file *file)
{
	struct bnxt_re_dev *rdev = inode->i_private;

	return single_open(file, bnxt_re_drv_stats_debugfs_show, rdev);
}

static int bnxt_re_debugfs_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

static const struct file_operations bnxt_re_info_dbg_ops = {
	.owner		= THIS_MODULE,
	.open		= bnxt_re_info_debugfs_open,
	.read		= seq_read,
	.write		= bnxt_re_info_debugfs_clear,
	.llseek		= seq_lseek,
	.release	= bnxt_re_debugfs_release,
};

static const struct file_operations bnxt_re_perf_dbg_ops = {
	.owner		= THIS_MODULE,
	.open		= bnxt_re_perf_debugfs_open,
	.read		= seq_read,
	.write		= bnxt_re_perf_debugfs_clear,
	.llseek		= seq_lseek,
	.release	= bnxt_re_debugfs_release,
};

static const struct file_operations bnxt_re_drv_stats_dbg_ops = {
	.owner		= THIS_MODULE,
	.open		= bnxt_re_drv_stats_debugfs_open,
	.read		= seq_read,
	.write		= bnxt_re_drv_stats_debugfs_clear,
	.llseek		= seq_lseek,
	.release	= bnxt_re_debugfs_release,
};

/*
 * bnxt_re_hex_dump -   This function will print hexdump of given buffer
 *                      It will also attempt pretty format if row data has all ZERO.
 * @s:                  Sequential file pointer
 * @buf:                Buffer to be dumped
 * @sz:                 Size in bytes
 * @format:             Different formats of dumping e.g. format=n will
 *                      cause only 'n' 32 bit words to be dumped in a
 *                      single line.
 * @abs_offset:         absolute offset.
 */
inline void bnxt_re_hex_dump(struct seq_file *s, void *buf, int sz,
			     int format, int abs_offset)
{
	int i, j, non_zero_data = 0, dump_this_row = 0, loop;
	u32 *buf_loc = (u32 *)buf;

	loop = (sz / sizeof(u32));

	for (i = 0; i < loop; i++) {
		if ((i % format) == 0) {
			if (dump_this_row)
				seq_puts(s, "\n");

			non_zero_data = 0;
			dump_this_row = 0;
			/*
			 * Below loop will check possible all zero data
			 * in single raw. It is added to make smaller data
			 * dump for better debugging.
			 */
			for (j = 0; j < format; j++) {
				if (buf_loc[i + j])
					non_zero_data++;
			}

			/* Force raw data dump at some intervals. */
			if (non_zero_data || (i == 0) || (i == loop / 4) ||
			    (i == loop / 2) || (i == (3 * loop / 4)))
				dump_this_row = 1;

			if (dump_this_row) {
				if (non_zero_data)
					seq_printf(s, "%08x: ", (i * 4) + abs_offset);
				else
					seq_printf(s, "\n%08x: ", (i * 4) + abs_offset);
			} else {
				seq_puts(s, ".");
			}
		}
		if (dump_this_row)
			seq_printf(s, "%08x ", buf_loc[i]);
	}
	seq_puts(s, "\n");
}

static void bnxt_re_print_pte_pbl(struct seq_file *s, struct bnxt_qplib_hwq *hwq)
{
	int i, j;

	for (i = 0; i <= hwq->level; i++) {
		struct bnxt_qplib_pbl *pbl = &hwq->pbl[i];

		seq_printf(s, "[level %d] pbl 0x%lx\n", i, (unsigned long)pbl);
		for (j = 0; j < pbl->pg_count; j++) {
			seq_printf(s, "\t[%2d] va: 0x%llx pa: 0x%llx\n",
				   j, (u64)hwq->pbl[i].pg_arr[j],
				   hwq->pbl[i].pg_map_arr[j]);
		}
	}
}

/*
 * bnxt_re_hwq_kernel_hex_dump - This function will print hexdump of kernel buffer.
 * @s:                  Sequential file pointer
 * @hwq:                hardware queue of rdma resources
 *
 * This function care about actual hwq page size.
 */
static void bnxt_re_hwq_kernel_hex_dump(struct seq_file *s,
					struct bnxt_qplib_hwq *hwq)
{
	u32 npages, i, page_size;

	npages = (hwq->max_elements / hwq->qe_ppg);

	if (hwq->max_elements % hwq->qe_ppg)
		npages++;

	page_size = hwq->qe_ppg * hwq->element_size;
	for (i = 0; i < npages; i++) {
		seq_puts(s, "\n");
		bnxt_re_hex_dump(s, hwq->pbl_ptr[i], page_size, 8, page_size * i);
	}
}

/*
 * bnxt_re_hwq_user_hex_dump - This function will print hexdump of given umem buffer.
 * @s:                  Sequential file pointer
 * @umem:               user memory from ib
 *
 * This function don't care about actual hwq page size.
 * It use PAGE_SIZE and loop using offset field of ib_umem_copy_from
 * just to avoid large vmalloc buffer.
 */
static void bnxt_re_hwq_user_hex_dump(struct seq_file *s,
				      struct ib_umem *umem)
{
	size_t	length_remain, length;
	u32 npages, i;
	void *buf;
	unsigned long   start_jiffies;

	buf = vzalloc(umem->length);
	if (!buf)
		return;

	npages = (umem->length / PAGE_SIZE);

	if (umem->length % PAGE_SIZE)
		npages++;

	ib_umem_copy_from(buf, umem, 0, umem->length);
	start_jiffies = jiffies;
	length_remain = umem->length;
	for (i = 0; i < npages; i++) {
		seq_puts(s, "\n");
		length = min_t(int, PAGE_SIZE, length_remain);
		length_remain -= length;
		bnxt_re_hex_dump(s, buf, length, 8, PAGE_SIZE * i);
		if (time_after(jiffies, start_jiffies + (BNXT_RE_TIMEOUT_HZ)))
			break;
	}
	vfree(buf);
}

static int bnxt_re_dump_pte_pbl_show(struct seq_file *s, void *unused)
{
	struct bnxt_re_dev *rdev = s->private;
	struct bnxt_re_res_list *res_list;
	unsigned long   start_jiffies;
	struct bnxt_re_srq *srq;
	struct bnxt_re_mr *mr;
	struct bnxt_re_qp *qp;
	struct bnxt_re_cq *cq;

	if (!rdev)
		return -ENODEV;

	/* This debugfs is open only for P5 onwards chips */
	if (!bnxt_re_is_rdev_valid(rdev) ||
	    !_is_chip_p5_plus(rdev->chip_ctx))
		return -ENODEV;

	seq_puts(s, "List of active PTEs and PBLs:\n");
	seq_puts(s, "=============================\n");

	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_QP];
	spin_lock(&res_list->lock);
	start_jiffies = jiffies;
	list_for_each_entry(qp, &res_list->head, res_list) {
		seq_printf(s, "%s [xid 0x%x] %s\n", "qp_sq",
			   qp->qplib_qp.id,
			   qp->qplib_qp.is_user ? "user" : "kernel");
		bnxt_re_print_pte_pbl(s, &qp->qplib_qp.sq.hwq);
		seq_printf(s, "%s [xid 0x%x] %s\n", "qp_rcq",
			   qp->qplib_qp.id,
			   qp->qplib_qp.is_user ? "user" : "kernel");
		bnxt_re_print_pte_pbl(s, &qp->qplib_qp.rq.hwq);
		if (time_after(jiffies, start_jiffies + BNXT_RE_TIMEOUT_HZ))
			goto qp_done;
	}
qp_done:
	spin_unlock(&res_list->lock);

	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_CQ];
	spin_lock(&res_list->lock);
	start_jiffies = jiffies;
	list_for_each_entry(cq, &res_list->head, res_list) {
		seq_printf(s, "%s [xid 0x%x] %s\n", "cq",
			   cq->qplib_cq.id,
			   cq->qplib_cq.hwq.is_user ? "user" : "kernel");
		bnxt_re_print_pte_pbl(s, &cq->qplib_cq.hwq);
		if (time_after(jiffies, start_jiffies + BNXT_RE_TIMEOUT_HZ))
			goto cq_done;
	}
cq_done:
	spin_unlock(&res_list->lock);

	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_MR];
	spin_lock(&res_list->lock);
	start_jiffies = jiffies;
	list_for_each_entry(mr, &res_list->head, res_list) {
		seq_printf(s, "%s [lkey 0x%x 0x%x] %s bnxt_re_mr 0x%lx\n", "mr",
			   mr->qplib_mr.lkey,
			   mr->ib_mr.lkey,
			   mr->qplib_mr.hwq.is_user ? "user" : "kernel",
			   (unsigned long)mr);
		bnxt_re_print_pte_pbl(s, &mr->qplib_mr.hwq);
		bnxt_re_print_pte_pbl(s, &mr->qplib_frpl.hwq);
		if (time_after(jiffies, start_jiffies + BNXT_RE_TIMEOUT_HZ))
			goto mr_done;
	}
mr_done:
	spin_unlock(&res_list->lock);

	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_SRQ];
	spin_lock(&res_list->lock);
	start_jiffies = jiffies;
	list_for_each_entry(srq, &res_list->head, res_list) {
		seq_printf(s, "%s [xid 0x%x] %s\n", "srq",
			   srq->qplib_srq.id,
			   srq->qplib_srq.is_user ? "user" : "kernel");
		bnxt_re_print_pte_pbl(s, &srq->qplib_srq.hwq);
		if (time_after(jiffies, start_jiffies + BNXT_RE_TIMEOUT_HZ))
			goto srq_done;
	}
srq_done:
	spin_unlock(&res_list->lock);

	seq_puts(s, "\n");
	return 0;
}

static int bnxt_re_active_res_dump_show(struct seq_file *s, void *unused)
{
	struct bnxt_re_dev *rdev = s->private;
	struct bnxt_re_res_list *res_list;
	unsigned long   start_jiffies;
	struct bnxt_re_srq *srq;
	struct bnxt_re_qp *qp;
	struct bnxt_re_cq *cq;

	if (!rdev)
		return -ENODEV;

	/* This debugfs is open only for P5 onwards chips */
	if (!bnxt_re_is_rdev_valid(rdev) ||
	    !_is_chip_p5_plus(rdev->chip_ctx))
		return -ENODEV;

	seq_puts(s, "=============================\n");

	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_QP];
	spin_lock(&res_list->lock);
	start_jiffies = jiffies;
	list_for_each_entry(qp, &res_list->head, res_list) {
		seq_printf(s, "%s [xid 0x%x] %s\n", "qp_sq",
			   qp->qplib_qp.id,
			   qp->qplib_qp.is_user ? "user" : "kernel");
		if (!qp->qplib_qp.is_user)
			bnxt_re_hwq_kernel_hex_dump(s, &qp->qplib_qp.sq.hwq);
		else
			bnxt_re_hwq_user_hex_dump(s, qp->sumem);
		seq_printf(s, "%s [xid 0x%x] %s\n", "qp_rq",
			   qp->qplib_qp.id,
			   qp->qplib_qp.is_user ? "user" : "kernel");
		if (!qp->qplib_qp.is_user)
			bnxt_re_hwq_kernel_hex_dump(s, &qp->qplib_qp.rq.hwq);
		else
			bnxt_re_hwq_user_hex_dump(s, qp->rumem);

		if (time_after(jiffies, start_jiffies + BNXT_RE_TIMEOUT_HZ))
			goto qp_done;
	}
qp_done:
	spin_unlock(&res_list->lock);

	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_CQ];
	spin_lock(&res_list->lock);
	start_jiffies = jiffies;
	list_for_each_entry(cq, &res_list->head, res_list) {
		seq_printf(s, "%s [xid 0x%x] %s\n", "cq",
			   cq->qplib_cq.id,
			   cq->qplib_cq.hwq.is_user ? "user" : "kernel");
		if (!cq->qplib_cq.hwq.is_user)
			bnxt_re_hwq_kernel_hex_dump(s, &cq->qplib_cq.hwq);
		else
			bnxt_re_hwq_user_hex_dump(s, cq->umem);
		if (time_after(jiffies, start_jiffies + BNXT_RE_TIMEOUT_HZ))
			goto cq_done;
	}
cq_done:
	spin_unlock(&res_list->lock);

	res_list = &rdev->res_list[BNXT_RE_RES_TYPE_SRQ];
	spin_lock(&res_list->lock);
	start_jiffies = jiffies;
	list_for_each_entry(srq, &res_list->head, res_list) {
		seq_printf(s, "%s [xid 0x%x] %s\n", "srq",
			   srq->qplib_srq.id,
			   srq->qplib_srq.is_user ? "user" : "kernel");
		if (!srq->qplib_srq.is_user)
			bnxt_re_hwq_kernel_hex_dump(s, &srq->qplib_srq.hwq);
		else
			bnxt_re_hwq_user_hex_dump(s, srq->umem);
		if (time_after(jiffies, start_jiffies + BNXT_RE_TIMEOUT_HZ))
			goto srq_done;
	}
srq_done:
	spin_unlock(&res_list->lock);

	seq_puts(s, "\n");
	return 0;
}

static int bnxt_re_dump_pte_pbl_open(struct inode *inode, struct file *file)
{
	struct bnxt_re_dev *rdev = inode->i_private;

	return single_open(file, bnxt_re_dump_pte_pbl_show, rdev);
}

static const struct file_operations bnxt_re_bump_pte_pbl_dbg_ops = {
	.owner		= THIS_MODULE,
	.open		= bnxt_re_dump_pte_pbl_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= bnxt_re_debugfs_release,
};

static int bnxt_re_active_res_dump_open(struct inode *inode, struct file *file)
{
	struct bnxt_re_dev *rdev = inode->i_private;

	return single_open(file, bnxt_re_active_res_dump_show, rdev);
}

static const struct file_operations bnxt_re_active_res_dump_ops = {
	.owner		= THIS_MODULE,
	.open		= bnxt_re_active_res_dump_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= bnxt_re_debugfs_release,
};

static int bnxt_re_qp_stats_show(struct seq_file *s, void *unused)
{
	struct bnxt_re_dev *rdev = s->private;

	if (!bnxt_re_is_rdev_valid(rdev))
		return -ENODEV;

	bnxt_re_print_rawqp_stats(rdev, s);

	return 0;
}

static int bnxt_re_qp_stats_open(struct inode *inode, struct file *file)
{
	struct bnxt_re_dev *rdev = inode->i_private;

	return single_open(file, bnxt_re_qp_stats_show, rdev);
}

static const struct file_operations bnxt_re_qp_stats_ops = {
	.owner		= THIS_MODULE,
	.open		= bnxt_re_qp_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t bnxt_re_qp_dump_read(struct file *filp, char *buffer,
				    size_t usr_buf_len, loff_t *ppos)
{
	struct bnxt_re_dev *rdev = filp->private_data;
	ssize_t sz;

	if (!rdev->qp_dump_buf)
		return -EFAULT;

	sz = simple_read_from_buffer(buffer, usr_buf_len, ppos,
				     (u8 *)(rdev->qp_dump_buf),
				      strlen((char *)rdev->qp_dump_buf));
	if (!sz)
		memset(rdev->qp_dump_buf, 0, BNXT_RE_LIVE_QPDUMP_SZ);

	return sz;
}

static ssize_t bnxt_re_qp_dump_write(struct file *filp, const char *buffer,
				     size_t count, loff_t *ppos)
{
	struct bnxt_re_dev *rdev = filp->private_data;
	struct bnxt_re_qp *qp;
	char *buf;
	u32 qp_id;
	int rc;

	if (!rdev->qp_dump_buf)
		return -ENOMEM;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		rc = -EFAULT;
		goto out;
	}

	buf[count] = '\0';

	rc = kstrtouint(buf, 0, &qp_id);
	if (rc)
		goto out;

	rc = -EINVAL;
	mutex_lock(&rdev->qp_lock);
	list_for_each_entry(qp, &rdev->qp_list, list) {
		if (qp_id == qp->qplib_qp.id) {
			rc = bnxt_re_capture_qpdump(qp, rdev->qp_dump_buf,
						    BNXT_RE_LIVE_QPDUMP_SZ, true);
			break;
		}
	}
	mutex_unlock(&rdev->qp_lock);
out:
	kfree(buf);
	return rc ? rc : count;
}

static const struct file_operations bnxt_re_qp_dump_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = bnxt_re_qp_dump_read,
	.write = bnxt_re_qp_dump_write,
};

void bnxt_re_add_dbg_files(struct bnxt_re_dev *rdev)
{
	rdev->pdev_qpinfo_dir = debugfs_create_dir("qp_info",
						   rdev->pdev_debug_dir);
	rdev->pdev_debug_dump_dir = debugfs_create_dir("debug_dump",
						       rdev->pdev_debug_dir);
	rdev->pte_pbl_info = debugfs_create_file("dump_pte_pbl", 0400,
						 rdev->pdev_debug_dump_dir, rdev,
						 &bnxt_re_bump_pte_pbl_dbg_ops);
	rdev->pte_pbl_info = debugfs_create_file("active_res_dump", 0400,
						 rdev->pdev_debug_dump_dir, rdev,
						 &bnxt_re_active_res_dump_ops);
	(void)debugfs_create_file("qp_dump", 0400, rdev->pdev_debug_dump_dir,
				  rdev, &bnxt_re_qp_dump_ops);
	(void)debugfs_create_file("qp_stats", 0400, rdev->pdev_debug_dump_dir,
				   rdev, &bnxt_re_qp_stats_ops);
}

static ssize_t bnxt_re_hdbr_dfs_read(struct file *filp, char __user *buffer,
				     size_t usr_buf_len, loff_t *ppos)
{
	struct bnxt_re_hdbr_dbgfs_file_data *data = filp->private_data;
	size_t len;
	char *buf;

	if (*ppos)
		return 0;
	if (!data)
		return -ENODEV;

	buf = bnxt_re_hdbr_dump(data->rdev, data->group, data->user);
	if (!buf)
		return -ENOMEM;
	len = strlen(buf);
	if (usr_buf_len < len) {
		kfree(buf);
		return -ENOSPC;
	}
	len = simple_read_from_buffer(buffer, usr_buf_len, ppos, buf, len);
	kfree(buf);
	return len;
}

static const struct file_operations bnxt_re_hdbr_dfs_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = bnxt_re_hdbr_dfs_read,
};

#define HDBR_DEBUGFS_SUB_TYPES 2
static void bnxt_re_add_hdbr_knobs(struct bnxt_re_dev *rdev)
{
	char *dirs[HDBR_DEBUGFS_SUB_TYPES] = {"driver", "apps"};
	char *names[DBC_GROUP_MAX] = {"sq", "rq", "srq", "cq"};
	struct bnxt_re_hdbr_dfs_data *data = rdev->hdbr_dbgfs;
	struct dentry *sub_dir, *f;
	int i, j;

	if (!rdev->hdbr_enabled)
		return;

	if (data)
		return;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return;
	data->hdbr_dir = debugfs_create_dir("hdbr", rdev->pdev_debug_dir);
	if (IS_ERR_OR_NULL(data->hdbr_dir)) {
		dev_dbg(rdev_to_dev(rdev), "Unable to create debugfs hdbr");
		kfree(data);
		return;
	}
	rdev->hdbr_dbgfs = data;
	for (i = 0; i < HDBR_DEBUGFS_SUB_TYPES; i++) {
		sub_dir = debugfs_create_dir(dirs[i], data->hdbr_dir);
		if (IS_ERR_OR_NULL(sub_dir)) {
			dev_dbg(rdev_to_dev(rdev), "Unable to create debugfs %s", dirs[i]);
			return;
		}
		for (j = 0; j < DBC_GROUP_MAX; j++) {
			data->file_data[i][j].rdev = rdev;
			data->file_data[i][j].group = j;
			data->file_data[i][j].user = !!i;
			f = debugfs_create_file(names[j], 0600, sub_dir, &data->file_data[i][j],
						&bnxt_re_hdbr_dfs_ops);
			if (IS_ERR_OR_NULL(f)) {
				dev_dbg(rdev_to_dev(rdev), "Unable to create hdbr debugfs file");
				return;
			}
		}
	}
}

static void bnxt_re_rem_hdbr_knobs(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_hdbr_dfs_data *data = rdev->hdbr_dbgfs;

	if (!data)
		return;
	debugfs_remove_recursive(data->hdbr_dir);
	kfree(data);
	rdev->hdbr_dbgfs = NULL;
}

static int bnxt_re_get_rtt_counts(struct bnxt_re_dev *rdev, char *buf,
				  int size, u16 session_id)
{
	struct hwrm_fw_get_structured_data_output getresp = {};
	struct hwrm_struct_data_udcc_rtt_bucket_count *bucket;
	struct hwrm_fw_get_structured_data_input getreq = {};
	struct bnxt_re_udcc_cfg *udcc = &rdev->udcc_cfg;
	struct hwrm_struct_hdr *cmdhdr;
	struct bnxt_fw_msg fw_msg = {};
	dma_addr_t dma_handle;
	u32 bucket_len, i;
	u32 len = 0;
	int rc;

	bucket_len = sizeof(struct hwrm_struct_hdr) +
		     (sizeof(struct hwrm_struct_data_udcc_rtt_bucket_count) *
		     udcc->rtt_num_of_buckets);

	cmdhdr = dma_zalloc_coherent(&rdev->en_dev->pdev->dev, bucket_len,
				     &dma_handle, GFP_KERNEL);
	if (!cmdhdr)
		return -ENOMEM;

	bnxt_re_init_hwrm_hdr((void *)&getreq, HWRM_FW_GET_STRUCTURED_DATA, -1);
	getreq.structure_id = cpu_to_le16(STRUCT_HDR_STRUCT_ID_UDCC_RTT_BUCKET_COUNT);
	getreq.subtype = cpu_to_le16(session_id);
	getreq.dest_data_addr = cpu_to_le64(dma_handle);
	getreq.count = udcc->rtt_num_of_buckets;
	getreq.data_len = sizeof(struct hwrm_struct_data_udcc_rtt_bucket_count);

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&getreq, sizeof(getreq), (void *)&getresp,
			    sizeof(getresp), DFLT_HWRM_CMD_TIMEOUT);
	rc = bnxt_send_msg(rdev->en_dev, &fw_msg);
	if (rc)
		goto out;

	bucket = ((void *)cmdhdr + sizeof(*cmdhdr));
	for (i = 0; i < udcc->rtt_num_of_buckets; i++) {
		len += snprintf(buf + len, size - len, "RTT bucket[%d] Count :\t%u\n",
				bucket[i].bucket_idx, bucket[i].count);
	}

out:
	dma_free_coherent(&rdev->en_dev->pdev->dev, bucket_len, cmdhdr, dma_handle);

	return rc;
}

static int bnxt_re_get_rtt_cfg(struct bnxt_re_dev *rdev, char *buf, int size)
{
	struct hwrm_fw_get_structured_data_output getresp = {};
	struct hwrm_struct_data_udcc_rtt_bucket_bound *bucket;
	struct hwrm_fw_get_structured_data_input getreq = {};
	struct bnxt_re_udcc_cfg *udcc = &rdev->udcc_cfg;
	struct hwrm_struct_hdr *cmdhdr;
	struct bnxt_fw_msg fw_msg = {};
	dma_addr_t dma_handle;
	u32 bucket_len, i;
	u32 len = 0;
	int rc;

	bucket_len = sizeof(struct hwrm_struct_hdr) +
		     (sizeof(struct hwrm_struct_data_udcc_rtt_bucket_bound) *
		     udcc->rtt_num_of_buckets);

	cmdhdr = dma_zalloc_coherent(&rdev->en_dev->pdev->dev, bucket_len,
				     &dma_handle, GFP_KERNEL);
	if (!cmdhdr)
		return -ENOMEM;

	bnxt_re_init_hwrm_hdr((void *)&getreq, HWRM_FW_GET_STRUCTURED_DATA, -1);
	getreq.structure_id = cpu_to_le16(STRUCT_HDR_STRUCT_ID_UDCC_RTT_BUCKET_BOUND);
	getreq.dest_data_addr = cpu_to_le64(dma_handle);
	getreq.count = udcc->rtt_num_of_buckets;
	getreq.data_len = sizeof(struct hwrm_struct_data_udcc_rtt_bucket_bound);

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&getreq, sizeof(getreq), (void *)&getresp,
			    sizeof(getresp), DFLT_HWRM_CMD_TIMEOUT);
	rc = bnxt_send_msg(rdev->en_dev, &fw_msg);
	if (rc)
		goto out;

	bucket = ((void *)cmdhdr + sizeof(*cmdhdr));
	for (i = 0; i < udcc->rtt_num_of_buckets; i++) {
		len += snprintf(buf + len, size - len, "RTT bucket[%d] Value(ns) :\t%u\n",
				bucket[i].bucket_idx, bucket[i].bound);
	}

out:
	dma_free_coherent(&rdev->en_dev->pdev->dev, bucket_len, cmdhdr, dma_handle);

	return rc;
}

static ssize_t bnxt_re_udcc_cfg_read(struct file *filp, char *buffer,
				     size_t count, loff_t *ppos)
{
	struct bnxt_re_dev *rdev = filp->private_data;
	struct bnxt_re_udcc_cfg *udcc;
	u32 len = 0, size = PAGE_SIZE;
	char *buf;
	int rc;

	buf = vzalloc(size);
	if (!buf)
		return -ENOMEM;

	udcc = &rdev->udcc_cfg;
	len = snprintf(buf, size, "UDCC Session_type :\t%s\n",
		       bnxt_udcc_session_type_str(udcc->session_type));
	len += snprintf(buf + len, size - len, "RTT no of buckets :\t%d\n",
			udcc->rtt_num_of_buckets);
	len += snprintf(buf + len, size - len, "max_sessions :\t\t%d\n",
			udcc->max_sessions);
	len += snprintf(buf + len, size - len, "min_sessions :\t\t%d\n",
			udcc->min_sessions);
	len += snprintf(buf + len, size - len, "RTT Distribution :\t%s\n",
			(bnxt_re_udcc_rtt_supported(udcc) ?
			 "Supported" : "Unsupported"));

	rc = bnxt_re_get_rtt_cfg(rdev, buf + len, size - len);
	if (rc)
		goto out;

	if (count < strlen(buf)) {
		rc = -ENOSPC;
		goto out;
	}

	len = simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
out:
	vfree(buf);

	return (rc) ? rc : len;
}

static int bnxt_re_put_rtt_cfg(struct bnxt_re_dev *rdev, char *buf)
{
	struct hwrm_fw_set_structured_data_output setresp = {};
	struct hwrm_struct_data_udcc_rtt_bucket_bound *bucket;
	struct hwrm_fw_set_structured_data_input setreq = {};
	struct bnxt_re_udcc_cfg *udcc = &rdev->udcc_cfg;
	u32 bucket_len, i, bucket_val, prev_val = 0;
	struct hwrm_struct_hdr *cmdhdr;
	struct bnxt_fw_msg fw_msg = {};
	dma_addr_t dma_handle;
	char *bucket_str;
	int rc;

	bucket_len = sizeof(*cmdhdr) +
		     (sizeof(struct hwrm_struct_data_udcc_rtt_bucket_bound) *
		      udcc->rtt_num_of_buckets);

	cmdhdr = dma_zalloc_coherent(&rdev->en_dev->pdev->dev, bucket_len,
				     &dma_handle, GFP_KERNEL);
	if (!cmdhdr)
		return -ENOMEM;

	bucket = ((void *)cmdhdr + sizeof(*cmdhdr));
	for (i = 0; i < udcc->rtt_num_of_buckets; i++) {
		bucket_str = strsep(&buf, ",");
		if (!bucket_str)
			break;

		rc = kstrtou32(bucket_str, 10, &bucket_val);
		if (rc < 0) {
			rc = -EINVAL;
			break;
		}

		if (bucket_val <= prev_val)
			break;

		prev_val = bucket_val;
		bucket[i].bucket_idx = i;
		bucket[i].bound = bucket_val;
	}

	if (i < udcc->rtt_num_of_buckets) {
		rc = -EINVAL;
		goto out;
	}

	bnxt_re_init_hwrm_hdr((void *)&setreq, HWRM_FW_SET_STRUCTURED_DATA, -1);

	cmdhdr->struct_id = cpu_to_le16(STRUCT_HDR_STRUCT_ID_UDCC_RTT_BUCKET_BOUND);
	cmdhdr->len = sizeof(struct hwrm_struct_data_udcc_rtt_bucket_bound);
	cmdhdr->count = udcc->rtt_num_of_buckets;
	cmdhdr->version = 1;

	setreq.src_data_addr = cpu_to_le64(dma_handle);
	setreq.data_len = bucket_len;
	setreq.hdr_cnt = 1;

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&setreq, sizeof(setreq), (void *)&setresp,
			    sizeof(setresp), DFLT_HWRM_CMD_TIMEOUT);
	rc = bnxt_send_msg(rdev->en_dev, &fw_msg);

out:
	dma_free_coherent(&rdev->en_dev->pdev->dev, bucket_len, cmdhdr, dma_handle);

	return rc;
}

static ssize_t bnxt_re_udcc_cfg_write(struct file *filp, const char *buffer,
				      size_t len, loff_t *ppos)
{
	struct bnxt_re_dev *rdev = filp->private_data;
	char *cmd, *cmdbuf, *bufptr;
	int rc = -EINVAL;

	cmdbuf = vzalloc(len + 1);
	if (!cmdbuf)
		return -ENOMEM;

	bufptr = cmdbuf;
	if (copy_from_user(cmdbuf, buffer, len)) {
		rc = -EFAULT;
		goto out;
	}

	cmdbuf[len] = '\0';
	cmd = strsep(&cmdbuf, ",");
	if (!cmd) {
		rc = -EINVAL;
		goto out;
	}

	if (strcmp(cmd, "RTT_CFG") == 0)
		rc = bnxt_re_put_rtt_cfg(rdev, cmdbuf);

out:
	vfree(bufptr);

	return (rc) ? rc : len;
}

static const struct file_operations bnxt_re_udcc_cfg_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = bnxt_re_udcc_cfg_read,
	.write = bnxt_re_udcc_cfg_write,
};

static void bnxt_re_add_udcc_dbg(struct bnxt_re_dev *rdev)
{
	if (!bnxt_qplib_udcc_supported(rdev->chip_ctx))
		return;
	rdev->udcc_dbgfs_dir = debugfs_create_dir("udcc", rdev->pdev_debug_dir);
	if (!rdev->udcc_dbgfs_dir)
		return;

	rdev->udcc_dbg_info = kcalloc(BNXT_RE_UDCC_MAX_SESSIONS,
				      sizeof(*rdev->udcc_dbg_info),
				      GFP_KERNEL);
	if (!rdev->udcc_dbg_info) {
		debugfs_remove_recursive(rdev->udcc_dbgfs_dir);
		rdev->udcc_dbgfs_dir = NULL;
	}

	if (!bnxt_re_udcc_rtt_supported(&rdev->udcc_cfg))
		return;

	rdev->udcc_cfg.cfg_entry = debugfs_create_file("config", 0400,
						       rdev->udcc_dbgfs_dir, rdev,
						       &bnxt_re_udcc_cfg_ops);
}

static void bnxt_re_rem_udcc_dbg(struct bnxt_re_dev *rdev)
{
	if (rdev->udcc_dbgfs_dir) {
		kfree(rdev->udcc_dbg_info);
		rdev->udcc_dbg_info = NULL;
		debugfs_remove_recursive(rdev->udcc_dbgfs_dir);
		rdev->udcc_dbgfs_dir = NULL;
	}
}

static ssize_t bnxt_re_cq_coal_cfg_write(struct file *filp, const char *buffer,
					 size_t len, loff_t *ppos)
{
	struct bnxt_re_cq_coal_param *cq_coal_info = filp->private_data;
	struct bnxt_re_dev *rdev = cq_coal_info->rdev;
	int option = cq_coal_info->offset;
	char buf[16];
	u32 val;

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	switch (option) {
	case BNXT_RE_BUF_MAXTIME:
		if (val < 1 || val > BNXT_QPLIB_CQ_COAL_MAX_BUF_MAXTIME)
			return -EINVAL;
		rdev->cq_coalescing.buf_maxtime = val;
		break;
	case BNXT_RE_NORMAL_MAXBUF:
		if (val < 1 || val > BNXT_QPLIB_CQ_COAL_MAX_NORMAL_MAXBUF)
			return -EINVAL;
		rdev->cq_coalescing.normal_maxbuf = val;
		break;
	case BNXT_RE_DURING_MAXBUF:
		if (val < 1 || val > BNXT_QPLIB_CQ_COAL_MAX_DURING_MAXBUF)
			return -EINVAL;
		rdev->cq_coalescing.during_maxbuf = val;
		break;
	case BNXT_RE_EN_RING_IDLE_MODE:
		if (val > BNXT_QPLIB_CQ_COAL_MAX_EN_RING_IDLE_MODE)
			return -EINVAL;
		rdev->cq_coalescing.en_ring_idle_mode = val;
		break;
	case BNXT_RE_CQ_COAL_ENABLE:
		if (val > 1) {
			dev_err(rdev_to_dev(rdev),
				"Invalid value %u. 1 to re-enable and 0 to disable", val);
			return -EINVAL;
		}
		rdev->cq_coalescing.enable = val;
		break;
	default:
		dev_err(rdev_to_dev(rdev),
			"Invalid option %d", option);
		break;
	}
	return  len;
}

static ssize_t bnxt_re_cq_coal_cfg_read(struct file *filp, char *buffer,
					size_t count, loff_t *ppos)
{
	struct bnxt_re_cq_coal_param *cq_coal_info = filp->private_data;
	struct bnxt_re_dev *rdev = cq_coal_info->rdev;
	int option = cq_coal_info->offset;
	char buf[16];
	u32 val;
	int rc;

	switch (option) {
	case BNXT_RE_BUF_MAXTIME:
		val = rdev->cq_coalescing.buf_maxtime;
		break;
	case BNXT_RE_NORMAL_MAXBUF:
		val = rdev->cq_coalescing.normal_maxbuf;
		break;
	case BNXT_RE_DURING_MAXBUF:
		val = rdev->cq_coalescing.during_maxbuf;
		break;
	case BNXT_RE_EN_RING_IDLE_MODE:
		val = rdev->cq_coalescing.en_ring_idle_mode;
		break;
	case BNXT_RE_CQ_COAL_ENABLE:
		val = rdev->cq_coalescing.enable;
		break;
	default:
		dev_err(rdev_to_dev(rdev),
			"Invalid option %d", option);
		return -EINVAL;
	}

	rc = snprintf(buf, sizeof(buf), "%d\n", val);
	if (rc < 0)
		return rc;

	return simple_read_from_buffer(buffer, count, ppos, (u8 *)(buf), rc);
}

static const struct file_operations bnxt_re_cq_coal_cfg_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = bnxt_re_cq_coal_cfg_read,
	.write = bnxt_re_cq_coal_cfg_write,
};

static void bnxt_re_add_cq_coal_knobs(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_dbg_cq_coal_cfg_params *cq_coal_params;
	int i;

	rdev->cq_coal_cfg_dir = debugfs_create_dir("cq_coal_cfg", rdev->pdev_debug_dir);
	if (IS_ERR_OR_NULL(rdev->cq_coal_cfg_dir)) {
		dev_dbg(rdev_to_dev(rdev), "Unable to create cq_coal debugfs folder");
		return;
	}
	rdev->cq_coal_cfg_params = kzalloc(sizeof(*cq_coal_params), GFP_KERNEL);

	for (i = 0; i < BNXT_RE_CQ_COAL_MAX; i++) {
		struct bnxt_re_cq_coal_param *tmp_params =
			&rdev->cq_coal_cfg_params->dbgfs_attr[i];

		tmp_params->rdev = rdev;
		tmp_params->offset = i;
		tmp_params->dentry = debugfs_create_file(bnxt_re_cq_coal_str[i], 0644,
							 rdev->cq_coal_cfg_dir, tmp_params,
							 &bnxt_re_cq_coal_cfg_ops);
		if (IS_ERR_OR_NULL(tmp_params->dentry)) {
			dev_dbg(rdev_to_dev(rdev), "Unable to create debugfs file");
			kfree(rdev->cq_coal_cfg_params);
			rdev->cq_coal_cfg_params = NULL;
			return;
		}
	}
}

static void bnxt_re_rem_cq_coal_dbg(struct bnxt_re_dev *rdev)
{
	if (rdev->cq_coal_cfg_dir) {
		kfree(rdev->cq_coal_cfg_params);
		rdev->cq_coal_cfg_params = NULL;
		debugfs_remove_recursive(rdev->cq_coal_cfg_dir);
		rdev->cq_coal_cfg_dir = NULL;
	}
}

static ssize_t bnxt_re_udcc_session_query_read(struct file *filep, char __user *buffer,
					       size_t count, loff_t *ppos)
{
	struct bnxt_re_udcc_dbg_info *udcc_info = filep->private_data;
	struct bnxt_re_dev *rdev = udcc_info->rdev;
	struct hwrm_udcc_session_query_output resp;
	struct hwrm_udcc_session_query_input req;
	struct bnxt_fw_msg fw_msg = {};
	int len = 0, size = 4096;
	char *buf;
	int rc;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_UDCC_SESSION_QUERY, -1);
	req.session_id = cpu_to_le16(udcc_info->session_id);
	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);
	rc = bnxt_send_msg(udcc_info->rdev->en_dev, &fw_msg);
	if (rc)
		return rc;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = scnprintf(buf, size, "min_rtt_ns = %u\n",
			le32_to_cpu(resp.min_rtt_ns));
	len += scnprintf(buf + len, size - len, "max_rtt_ns = %u\n",
			le32_to_cpu(resp.max_rtt_ns));
	len += scnprintf(buf + len, size - len, "cur_rate_mbps = %u\n",
			le32_to_cpu(resp.cur_rate_mbps));
	len += scnprintf(buf + len, size - len, "tx_event_count = %u\n",
			le32_to_cpu(resp.tx_event_count));
	len += scnprintf(buf + len, size - len, "cnp_rx_event_count = %u\n",
			le32_to_cpu(resp.cnp_rx_event_count));
	len += scnprintf(buf + len, size - len, "rtt_req_count = %u\n",
			le32_to_cpu(resp.rtt_req_count));
	len += scnprintf(buf + len, size - len, "rtt_resp_count = %u\n",
			le32_to_cpu(resp.rtt_resp_count));
	len += scnprintf(buf + len, size - len, "tx_bytes_sent = %u\n",
			le32_to_cpu(resp.tx_bytes_count));
	len += scnprintf(buf + len, size - len, "tx_pkts_sent = %u\n",
			le32_to_cpu(resp.tx_packets_count));
	len += scnprintf(buf + len, size - len, "init_probes_sent = %u\n",
			le32_to_cpu(resp.init_probes_sent));
	len += scnprintf(buf + len, size - len, "term_probes_recv = %u\n",
			le32_to_cpu(resp.term_probes_recv));
	len += scnprintf(buf + len, size - len, "cnp_packets_recv = %u\n",
			le32_to_cpu(resp.cnp_packets_recv));
	len += scnprintf(buf + len, size - len, "rto_event_recv = %u\n",
			le32_to_cpu(resp.rto_event_recv));
	len += scnprintf(buf + len, size - len, "seq_err_nak_recv = %u\n",
			le32_to_cpu(resp.seq_err_nak_recv));
	len += scnprintf(buf + len, size - len, "qp_count = %u\n",
			le32_to_cpu(resp.qp_count));
	len += scnprintf(buf + len, size - len, "tx_event_detect_count = %u\n",
			le32_to_cpu(resp.tx_event_detect_count));

	if (bnxt_re_udcc_rtt_supported(&rdev->udcc_cfg)) {
		rc = bnxt_re_get_rtt_counts(rdev, buf + len, size - len, udcc_info->session_id);
		if (rc)
			goto out;
	}

	if (count < strlen(buf)) {
		rc = -ENOSPC;
		goto out;
	}

	len = simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
out:
	kfree(buf);
	return (rc) ? rc : len;
}

static int bnxt_re_udcc_session_reset_rtt(struct bnxt_re_udcc_dbg_info *udcc_info)
{
	struct hwrm_udcc_session_cfg_output resp = {};
	struct hwrm_udcc_session_cfg_input req = {};
	struct bnxt_re_dev *rdev = udcc_info->rdev;
	struct bnxt_fw_msg fw_msg = {};

	if (!bnxt_re_udcc_rtt_supported(&rdev->udcc_cfg))
		return 0;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_UDCC_SESSION_CFG, -1);

	req.session_id = cpu_to_le16(udcc_info->session_id);
	req.flags = cpu_to_le16(UDCC_SESSION_CFG_REQ_FLAGS_RESET_RTT_COUNTS);
	req.enables = cpu_to_le32(UDCC_SESSION_CFG_REQ_ENABLES_SESSION_STATE);
	req.session_state = UDCC_SESSION_CFG_REQ_SESSION_STATE_ENABLED;

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);

	return bnxt_send_msg(rdev->en_dev, &fw_msg);
}

static ssize_t bnxt_re_udcc_session_write(struct file *filp, const char *buffer,
					  size_t len, loff_t *ppos)
{
	struct bnxt_re_udcc_dbg_info *udcc_info = filp->private_data;
	char *cmd;
	int rc = -EINVAL;

	cmd = vzalloc(len);
	if (!cmd)
		return -ENOMEM;

	if (copy_from_user(cmd, buffer, len)) {
		rc = -EFAULT;
		goto out;
	}

	cmd[len - 1] = '\0';
	if (strcmp(cmd, "RESET_RTT") == 0)
		rc = bnxt_re_udcc_session_reset_rtt(udcc_info);

out:
	vfree(cmd);

	return (rc) ? rc : len;
}

static const struct file_operations bnxt_re_udcc_session_query_ops = {
	.owner  = THIS_MODULE,
	.open   = simple_open,
	.read   = bnxt_re_udcc_session_query_read,
	.write  = bnxt_re_udcc_session_write,
};

void bnxt_re_debugfs_create_udcc_session(struct bnxt_re_dev *rdev, u32 session_id)
{
	struct bnxt_re_udcc_dbg_info *udcc_info;
	char sname[16];

	if (!rdev->udcc_dbgfs_dir)
		return;
	udcc_info = &rdev->udcc_dbg_info[session_id];
	if (udcc_info->session_dbgfs_dir)
		return;
	snprintf(sname, 10, "%d", session_id);
	udcc_info->session_dbgfs_dir = debugfs_create_dir(sname, rdev->udcc_dbgfs_dir);
	udcc_info->rdev = rdev;
	udcc_info->session_id = session_id;
	debugfs_create_file("session_query", 0644, udcc_info->session_dbgfs_dir,
			    udcc_info, &bnxt_re_udcc_session_query_ops);
}

void bnxt_re_debugfs_delete_udcc_session(struct bnxt_re_dev *rdev, u32 session_id)
{
	struct bnxt_re_udcc_dbg_info *udcc_info = &rdev->udcc_dbg_info[session_id];

	debugfs_remove_recursive(udcc_info->session_dbgfs_dir);
	udcc_info->session_dbgfs_dir = NULL;
}

static ssize_t bnxt_re_read_force_mirror(struct file *file, char *buffer,
					 size_t len, loff_t *ppos)
{
	struct bnxt_re_dev *rdev = file->private_data;
	bool force_mirror_en, tf_en;
	u32 feat_cfg_cur;
	int count, rc;
	char *buf;

	bnxt_force_mirror_en_get(rdev->en_dev, &tf_en, &force_mirror_en);

	if (tf_en) {
		buf = vzalloc(len + 1);
		if (!buf)
			return -ENOMEM;
		buf[len] = '\0';
		count = scnprintf(buf, len, "tf force mirroring : %s\n",
				  (force_mirror_en) ?
				  "enabled" : "disabled");
		count = simple_read_from_buffer(buffer, len, ppos, buf, count);
		vfree(buf);
	} else {

		rc = bnxt_qplib_roce_cfg(&rdev->rcfw, 0, 0, &feat_cfg_cur);
		if (rc)
			return rc;
		buf = vzalloc(len + 1);
		if (!buf)
			return -ENOMEM;
		buf[len] = '\0';
		count = scnprintf(buf, len, "force mirroring : %s\n",
				  (feat_cfg_cur & CREQ_ROCE_CFG_RESP_FORCE_MIRROR_ENABLE) ?
				  "enabled" : "disabled");
		count = simple_read_from_buffer(buffer, len, ppos, buf, count);
		vfree(buf);
	}
	return count;
}

static ssize_t bnxt_re_force_mirror(struct file *file, const char *buffer,
				    size_t len, loff_t *ppos)
{
	u32 val, feat_cfg = 0, feat_enables, feat_cfg_cur;
	struct bnxt_re_dev *rdev = file->private_data;
	char *buf;
	int rc;

	buf = vzalloc(len + 1);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, len)) {
		rc = -EFAULT;
		goto out;
	}

	buf[len] = '\0';
	rc = kstrtou32(buf, 10, &val);
	if (rc < 0) {
		rc = -EINVAL;
		goto out;
	}

	feat_enables = CMDQ_ROCE_CFG_FEAT_ENABLES_FORCE_MIRROR_ENABLE;
	switch (val) {
	case 1:
		feat_cfg = CMDQ_ROCE_CFG_FEAT_CFG_FORCE_MIRROR_ENABLE;
		dev_warn(rdev_to_dev(rdev), "Enabling forced mirroring\n");
		break;
	case 0:
		feat_cfg &= ~CMDQ_ROCE_CFG_FEAT_CFG_FORCE_MIRROR_ENABLE;
		dev_info(rdev_to_dev(rdev), "Disabling forced mirroring\n");
		break;
	default:
		rc = -EINVAL;
		goto out;
	}

	/* notify kernel driver of force mirror config change */
	bnxt_force_mirror_en_cfg(rdev->en_dev, val);

	rc = bnxt_qplib_roce_cfg(&rdev->rcfw, feat_cfg, feat_enables, &feat_cfg_cur);
out:
	vfree(buf);
	return rc ? rc : len;
}

static const struct file_operations bnxt_re_force_mir_cfg_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = bnxt_re_read_force_mirror,
	.write = bnxt_re_force_mirror,
};

static void bnxt_re_add_force_mirroring_cfg(struct bnxt_re_dev *rdev)
{
	if (!is_force_mirroring_supported(rdev->dev_attr->dev_cap_ext_flags2))
		return;

	(void)debugfs_create_file("force_mirroring", 0400, rdev->config_debugfs_dir,
				  rdev, &bnxt_re_force_mir_cfg_ops);
}

void bnxt_re_rename_debugfs_entry(struct bnxt_re_dev *rdev)
{
#ifndef HAVE_DEBUGFS_CHANGE_NAME
	struct dentry *port_debug_dir;
#endif
	int rc = 0;

	if (!test_bit(BNXT_RE_FLAG_PER_PORT_DEBUG_INFO, &rdev->flags)) {
		strncpy(rdev->dev_name, dev_name(&rdev->ibdev.dev), IB_DEVICE_NAME_MAX);
		bnxt_re_debugfs_add_port(rdev, rdev->dev_name);
		set_bit(BNXT_RE_FLAG_PER_PORT_DEBUG_INFO, &rdev->flags);
		dev_info(rdev_to_dev(rdev), "Device %s registered successfully",
			 rdev->dev_name);
	} else if (strncmp(rdev->dev_name, dev_name(&rdev->ibdev.dev), IB_DEVICE_NAME_MAX)) {
		if (IS_ERR_OR_NULL(rdev->port_debug_dir))
			return;
		strncpy(rdev->dev_name, dev_name(&rdev->ibdev.dev), IB_DEVICE_NAME_MAX);
#ifdef HAVE_DEBUGFS_CHANGE_NAME
		rc = debugfs_change_name(rdev->port_debug_dir, "%s", rdev->dev_name);
#else
		port_debug_dir = debugfs_rename(bnxt_re_debugfs_root,
						rdev->port_debug_dir,
						bnxt_re_debugfs_root,
						rdev->dev_name);
		if (IS_ERR(port_debug_dir))
			rc = PTR_ERR(port_debug_dir);
		else
			rdev->port_debug_dir = port_debug_dir;
#endif
		if (rc) {
			dev_warn(rdev_to_dev(rdev), "Unable to rename debugfs %s",
				 rdev->dev_name);
			return;
		}
		dev_info(rdev_to_dev(rdev), "Device renamed to %s successfully",
			 rdev->dev_name);
	}
}

static int map_cc_config_offset_gen0_ext0(u32 offset, struct bnxt_qplib_cc_param *ccparam, u32 *val)
{
	u64 map_offset;

	map_offset = BIT(offset);

	switch (map_offset) {
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ENABLE_CC:
		*val = ccparam->enable;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_G:
		*val = ccparam->g;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_NUMPHASEPERSTATE:
		*val = ccparam->nph_per_state;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_INIT_CR:
		*val = ccparam->init_cr;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_INIT_TR:
		*val = ccparam->init_tr;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_ECN:
		*val = ccparam->tos_ecn;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP:
		*val =  ccparam->tos_dscp;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_VLAN_PCP:
		*val = ccparam->alt_vlan_pcp;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_TOS_DSCP:
		*val = ccparam->alt_tos_dscp;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_RTT:
	       *val = ccparam->rtt;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_CC_MODE:
		*val = ccparam->cc_mode;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TCP_CP:
		*val =  ccparam->tcp_cp;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_INACTIVITY_CP:
		*val =  ccparam->inact_th;
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int map_cc_config_offset_gen1_ext0(u32 offset,
					  struct bnxt_qplib_cc_param *cc_param,
					  u32 *val)
{
	u64 map_offset;

	map_offset = BIT(offset);

	switch (map_offset) {
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_MIN_TIME_BETWEEN_CNPS:
		*val = cc_param->cc_ext.min_delta_cnp;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_INIT_CP:
		*val = cc_param->cc_ext.init_cp;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_TR_UPDATE_MODE:
		*val = cc_param->cc_ext.tr_update_mode;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_TR_UPDATE_CYCLES:
		*val = cc_param->cc_ext.tr_update_cyls;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_FR_NUM_RTTS:
		*val = cc_param->cc_ext.fr_rtt;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_AI_RATE_INCREASE:
		*val = cc_param->cc_ext.ai_rate_incr;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_REDUCTION_RELAX_RTTS_TH:
		*val =  cc_param->cc_ext.rr_rtt_th;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_ADDITIONAL_RELAX_CR_TH:
		*val = cc_param->cc_ext.ar_cr_th;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CR_MIN_TH:
		*val = cc_param->cc_ext.cr_min_th;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_BW_AVG_WEIGHT:
		*val = cc_param->cc_ext.bw_avg_weight;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_ACTUAL_CR_FACTOR:
		*val = cc_param->cc_ext.cr_factor;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_MAX_CP_CR_TH:
		*val = cc_param->cc_ext.cr_th_max_cp;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CP_BIAS_EN:
		*val = cc_param->cc_ext.cp_bias_en;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CP_BIAS:
		*val = cc_param->cc_ext.cp_bias;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CNP_ECN:
		*val =  cc_param->cc_ext.cnp_ecn;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_RTT_JITTER_EN:
		*val = cc_param->cc_ext.rtt_jitter_en;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_LINK_BYTES_PER_USEC:
		*val = cc_param->cc_ext.bytes_per_usec;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_RESET_CC_CR_TH:
		*val = cc_param->cc_ext.cc_cr_reset_th;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CR_WIDTH:
		*val = cc_param->cc_ext.cr_width;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_QUOTA_PERIOD_MIN:
		*val = cc_param->cc_ext.min_quota;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_QUOTA_PERIOD_MAX:
		*val = cc_param->cc_ext.max_quota;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_QUOTA_PERIOD_ABS_MAX:
		*val = cc_param->cc_ext.abs_max_quota;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_TR_LOWER_BOUND:
		*val =  cc_param->cc_ext.tr_lb;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CR_PROB_FACTOR:
		*val = cc_param->cc_ext.cr_prob_fac;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_TR_PROB_FACTOR:
		*val = cc_param->cc_ext.tr_prob_fac;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_FAIRNESS_CR_TH:
		*val = cc_param->cc_ext.fair_cr_th;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_RED_DIV:
		*val = cc_param->cc_ext.red_div;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CNP_RATIO_TH:
		*val = cc_param->cc_ext.cnp_ratio_th;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_EXP_AI_RTTS:
		*val = cc_param->cc_ext.ai_ext_rtt;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_EXP_AI_CR_CP_RATIO:
		*val = cc_param->cc_ext.exp_crcp_ratio;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CP_EXP_UPDATE_TH:
		*val = cc_param->cc_ext.cpcr_update_th;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_HIGH_EXP_AI_RTTS_TH1:
		*val = cc_param->cc_ext.ai_rtt_th1;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_HIGH_EXP_AI_RTTS_TH2:
		*val = cc_param->cc_ext.ai_rtt_th2;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_USE_RATE_TABLE:
		*val =  cc_param->cc_ext.low_rate_en;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_LINK64B_PER_RTT:
		*val = cc_param->cc_ext.l64B_per_rtt;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_ACTUAL_CR_CONG_FREE_RTTS_TH:
		*val = cc_param->cc_ext.cf_rtt_th;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_REDUCE_INIT_CONG_FREE_RTTS_TH:
		*val = cc_param->cc_ext.reduce_cf_rtt_th;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_RANDOM_NO_RED_EN:
		*val = cc_param->cc_ext.random_no_red_en;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_SEVERE_CONG_CR_TH1:
		*val =  cc_param->cc_ext.sc_cr_th1;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_SEVERE_CONG_CR_TH2:
		*val = cc_param->cc_ext.sc_cr_th2;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CC_ACK_BYTES:
		*val = cc_param->cc_ext.cc_ack_bytes;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_ACTUAL_CR_SHIFT_CORRECTION_EN:
		*val = cc_param->cc_ext.actual_cr_shift_correction_en;
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static int map_cc_config_offset_gen1_ext1(u32 offset, struct bnxt_qplib_cc_param *cc_param,
					  u32 *val)
{
	u64 map_offset;

	map_offset = BIT(offset);

	switch (map_offset) {
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_RND_NO_RED_MULT:
		*val = cc_param->cc_gen1_ext.rnd_no_red_mult;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_NO_RED_OFFSET:
		*val = cc_param->cc_gen1_ext.no_red_offset;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_REDUCE2_INIT_CONG_FREE_RTTS_TH:
		*val = cc_param->cc_gen1_ext.reduce2_init_cong_free_rtts_th;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_REDUCE2_INIT_EN:
		*val = cc_param->cc_gen1_ext.reduce2_init_en;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_PERIOD_ADJUST_COUNT:
		*val = cc_param->cc_gen1_ext.period_adjust_count;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_CURRENT_RATE_THRESHOLD_1:
		*val = cc_param->cc_gen1_ext.current_rate_threshold_1;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_CURRENT_RATE_THRESHOLD_2:
		*val = cc_param->cc_gen1_ext.current_rate_threshold_2;
		break;
	default:
		return -EOPNOTSUPP;
}

	return 0;
}

static ssize_t bnxt_re_cc_config_get(struct file *filp, char __user *buffer,
				     size_t usr_buf_len, loff_t *ppos)
{
	struct bnxt_re_cc_param *dbg_cc_param = filp->private_data;
	struct bnxt_re_dev *rdev = dbg_cc_param->rdev;
	struct bnxt_qplib_cc_param ccparam = {};
	u32 offset = dbg_cc_param->offset;
	u32 cc_gen = dbg_cc_param->cc_gen;
	char buf[16];
	u32 val;
	int rc;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &ccparam);
	if (rc)
		return rc;

	switch (cc_gen) {
	case CC_CONFIG_GEN0_EXT0:
		rc = map_cc_config_offset_gen0_ext0(offset, &ccparam, &val);
		break;
	case CC_CONFIG_GEN1_EXT0:
		rc = map_cc_config_offset_gen1_ext0(offset, &ccparam, &val);
		break;
	case CC_CONFIG_GEN1_EXT1:
		rc = map_cc_config_offset_gen1_ext1(offset, &ccparam, &val);
		break;
	default:
		return -EINVAL;
	}
	if (rc)
		return rc;

	rc = snprintf(buf, sizeof(buf), "%d\n", val);
	if (rc < 0)
		return rc;

	return simple_read_from_buffer(buffer, usr_buf_len, ppos, (u8 *)(buf), rc);
}

static int bnxt_re_fill_gen0_ext0(struct bnxt_qplib_cc_param *ccparam, u32 offset, u32 val)
{
	u32 modify_mask;

	modify_mask = BIT(offset);

	switch (modify_mask) {
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ENABLE_CC:
		ccparam->enable = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_G:
		ccparam->g = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_NUMPHASEPERSTATE:
		ccparam->nph_per_state = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_INIT_CR:
		ccparam->init_cr = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_INIT_TR:
		ccparam->init_tr = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_ECN:
		ccparam->tos_ecn = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP:
		ccparam->tos_dscp = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_VLAN_PCP:
		ccparam->alt_vlan_pcp = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_TOS_DSCP:
		ccparam->alt_tos_dscp = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_RTT:
		ccparam->rtt = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_CC_MODE:
		ccparam->cc_mode = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TCP_CP:
		ccparam->tcp_cp = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TX_QUEUE:
		return -EOPNOTSUPP;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_INACTIVITY_CP:
		ccparam->inact_th = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TIME_PER_PHASE:
		ccparam->time_pph = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_PKTS_PER_PHASE:
		ccparam->pkts_pph = val;
		break;
	}

	ccparam->mask = modify_mask;
	return 0;
}

static int bnxt_re_fill_gen1_ext0(struct bnxt_qplib_cc_param *ccparam, u32 offset, u32 val)
{
	u64 modify_mask;

	modify_mask = BIT(offset);

	switch (modify_mask) {
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_MIN_TIME_BETWEEN_CNPS:
		ccparam->cc_ext.min_delta_cnp = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_INIT_CP:
		ccparam->cc_ext.init_cp = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_TR_UPDATE_MODE:
		ccparam->cc_ext.tr_update_mode = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_TR_UPDATE_CYCLES:
		ccparam->cc_ext.tr_update_cyls = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_FR_NUM_RTTS:
		ccparam->cc_ext.fr_rtt = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_AI_RATE_INCREASE:
		ccparam->cc_ext.ai_rate_incr = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_REDUCTION_RELAX_RTTS_TH:
		ccparam->cc_ext.rr_rtt_th = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_ADDITIONAL_RELAX_CR_TH:
		ccparam->cc_ext.ar_cr_th = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CR_MIN_TH:
		ccparam->cc_ext.cr_min_th = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_BW_AVG_WEIGHT:
		ccparam->cc_ext.bw_avg_weight = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_ACTUAL_CR_FACTOR:
		ccparam->cc_ext.cr_factor = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_MAX_CP_CR_TH:
		ccparam->cc_ext.cr_th_max_cp = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CP_BIAS_EN:
		ccparam->cc_ext.cp_bias_en = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CP_BIAS:
		ccparam->cc_ext.cp_bias = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CNP_ECN:
		ccparam->cc_ext.cnp_ecn = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_RTT_JITTER_EN:
		ccparam->cc_ext.rtt_jitter_en = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_LINK_BYTES_PER_USEC:
		ccparam->cc_ext.bytes_per_usec = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_RESET_CC_CR_TH:
		ccparam->cc_ext.cc_cr_reset_th = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CR_WIDTH:
		ccparam->cc_ext.cr_width = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_QUOTA_PERIOD_MIN:
		ccparam->cc_ext.min_quota = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_QUOTA_PERIOD_MAX:
		ccparam->cc_ext.max_quota = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_QUOTA_PERIOD_ABS_MAX:
		ccparam->cc_ext.abs_max_quota = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_TR_LOWER_BOUND:
		ccparam->cc_ext.tr_lb = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CR_PROB_FACTOR:
		ccparam->cc_ext.cr_prob_fac = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_TR_PROB_FACTOR:
		ccparam->cc_ext.tr_prob_fac = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_FAIRNESS_CR_TH:
		ccparam->cc_ext.fair_cr_th = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_RED_DIV:
		ccparam->cc_ext.red_div = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CNP_RATIO_TH:
		ccparam->cc_ext.cnp_ratio_th = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_EXP_AI_RTTS:
		ccparam->cc_ext.ai_ext_rtt = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_EXP_AI_CR_CP_RATIO:
		ccparam->cc_ext.exp_crcp_ratio = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_USE_RATE_TABLE:
		ccparam->cc_ext.low_rate_en = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CP_EXP_UPDATE_TH:
		ccparam->cc_ext.cpcr_update_th = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_HIGH_EXP_AI_RTTS_TH1:
		ccparam->cc_ext.ai_rtt_th1 = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_HIGH_EXP_AI_RTTS_TH2:
		ccparam->cc_ext.ai_rtt_th2 = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_ACTUAL_CR_CONG_FREE_RTTS_TH:
		ccparam->cc_ext.cf_rtt_th = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_REDUCE_INIT_CONG_FREE_RTTS_TH:
		ccparam->cc_ext.reduce_cf_rtt_th = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_RANDOM_NO_RED_EN:
		ccparam->cc_ext.random_no_red_en = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_SEVERE_CONG_CR_TH1:
		ccparam->cc_ext.sc_cr_th1 = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_SEVERE_CONG_CR_TH2:
		ccparam->cc_ext.sc_cr_th2 = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_LINK64B_PER_RTT:
		ccparam->cc_ext.l64B_per_rtt = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CC_ACK_BYTES:
		ccparam->cc_ext.cc_ack_bytes = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_ACTUAL_CR_SHIFT_CORRECTION_EN:
		ccparam->cc_ext.actual_cr_shift_correction_en = val;
		break;
	default:
		return -EOPNOTSUPP;
	}

	ccparam->cc_ext.ext_mask = modify_mask;
	return 0;
}

static int bnxt_re_fill_gen1_ext1(struct bnxt_qplib_cc_param *cc_param, u32 offset, u32 val)
{
	u64 modify_mask;

	modify_mask = BIT(offset);

	switch (modify_mask) {
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_RND_NO_RED_MULT:
		cc_param->cc_gen1_ext.rnd_no_red_mult = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_NO_RED_OFFSET:
		cc_param->cc_gen1_ext.no_red_offset = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_REDUCE2_INIT_CONG_FREE_RTTS_TH:
		cc_param->cc_gen1_ext.reduce2_init_cong_free_rtts_th = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_REDUCE2_INIT_EN:
		cc_param->cc_gen1_ext.reduce2_init_en = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_PERIOD_ADJUST_COUNT:
		cc_param->cc_gen1_ext.period_adjust_count = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_CURRENT_RATE_THRESHOLD_1:
		cc_param->cc_gen1_ext.current_rate_threshold_1 = val;
		break;
	case CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_CURRENT_RATE_THRESHOLD_2:
		cc_param->cc_gen1_ext.current_rate_threshold_2 = val;
		break;
	default:
		return -EOPNOTSUPP;
	}

	cc_param->cc_gen1_ext.gen1_ext_mask = modify_mask;
	return 0;
}

static int bnxt_re_configure_cc(struct bnxt_re_dev *rdev, u32 gen_ext, u32 offset, u32 val)
{
	struct bnxt_qplib_cc_param ccparam = { };
	int rc;

	switch (gen_ext) {
	case CC_CONFIG_GEN0_EXT0:
		rc = bnxt_re_fill_gen0_ext0(&ccparam, offset, val);
		break;
	case CC_CONFIG_GEN1_EXT0:
		rc = bnxt_re_fill_gen1_ext0(&ccparam, offset, val);
		break;
	case CC_CONFIG_GEN1_EXT1:
		rc = bnxt_re_fill_gen1_ext1(&ccparam, offset, val);
		break;
	default:
		return -EINVAL;
	}
	if (rc)
		return rc;

	return bnxt_qplib_modify_cc(&rdev->qplib_res, &ccparam);
}

static ssize_t bnxt_re_cc_config_set(struct file *filp, const char __user *buffer,
				     size_t count, loff_t *ppos)
{
	struct bnxt_re_cc_param *dbg_cc_param = filp->private_data;
	struct bnxt_re_dev *rdev = dbg_cc_param->rdev;
	u32 offset = dbg_cc_param->offset;
	u32 cc_gen = dbg_cc_param->cc_gen;
	char buf[16];
	u32 val;
	int rc;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[count] = '\0';
	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	rc = bnxt_re_configure_cc(rdev, cc_gen, offset, val);
	return rc ? rc : count;
}

static const struct file_operations bnxt_re_cc_config_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = bnxt_re_cc_config_get,
	.write = bnxt_re_cc_config_set,
};

static void bnxt_re_add_cc_config(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_dbg_cc_config_params *cc_params;
	int i;

	rdev->cc_config = debugfs_create_dir("cc_config", rdev->pdev_debug_dir);
	rdev->cc_config_params = kzalloc(sizeof(*cc_params), GFP_KERNEL);
	for (i = 0; i < BNXT_RE_CC_PARAM_GEN0; i++) {
		struct bnxt_re_cc_param *tmp_params = &rdev->cc_config_params->gen0_params[i];

		tmp_params->rdev = rdev;
		tmp_params->offset = i;
		tmp_params->cc_gen = CC_CONFIG_GEN0_EXT0;
		tmp_params->dentry = debugfs_create_file(bnxt_re_cc_gen0_name[i], 0400,
							 rdev->cc_config, tmp_params,
							 &bnxt_re_cc_config_ops);
	}
	for (i = 0; i < BNXT_RE_CC_PARAM_GEN1; i++) {
		struct bnxt_re_cc_param *tmp_params = &rdev->cc_config_params->gen1_params[i];

		tmp_params->rdev = rdev;
		tmp_params->offset = i;
		tmp_params->cc_gen = CC_CONFIG_GEN1_EXT0;
		tmp_params->dentry = debugfs_create_file(bnxt_re_cc_gen1_name[i], 0400,
							 rdev->cc_config, tmp_params,
							 &bnxt_re_cc_config_ops);
	}
	for (i = 0; i < BNXT_RE_CC_PARAM_GEN1_EXT1; i++) {
		struct bnxt_re_cc_param *tmp_params = &rdev->cc_config_params->gen1_ext1_params[i];

		tmp_params->rdev = rdev;
		tmp_params->offset = i;
		tmp_params->cc_gen = CC_CONFIG_GEN1_EXT1;
		tmp_params->dentry = debugfs_create_file(bnxt_re_cc_gen1_ext1_name[i], 0400,
							 rdev->cc_config, tmp_params,
							 &bnxt_re_cc_config_ops);
	}
}

static ssize_t bnxt_re_read_icrc_cfg(struct file *file, char __user *buffer,
				     size_t len, loff_t *ppos)
{
	struct bnxt_re_dev *rdev = file->private_data;
	u32 feat_cfg_cur;
	int count, rc;
	char *buf;

	buf = vzalloc(len + 1);
	if (!buf)
		return -ENOMEM;

	buf[len] = '\0';
	rc = bnxt_qplib_roce_cfg(&rdev->rcfw, 0, 0, &feat_cfg_cur);
	if (rc)
		goto out;

	count = scnprintf(buf, len, "disable_icrc : %s\n",
			  (feat_cfg_cur & CREQ_ROCE_CFG_RESP_ICRC_CHECK_DISABLED) ?
			   "true" : "false");
	count = simple_read_from_buffer(buffer, len, ppos, buf, count);
out:
	vfree(buf);
	return (rc) ? rc : count;
}

static ssize_t bnxt_re_disable_icrc(struct file *file, const char *buffer,
				    size_t len, loff_t *ppos)
{
	u32 val, feat_cfg = 0, feat_enables, feat_cfg_cur;
	struct bnxt_re_dev *rdev = file->private_data;
	char *buf;
	int rc;

	buf = vzalloc(len + 1);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, len)) {
		rc = -EFAULT;
		goto out;
	}

	buf[len] = '\0';
	rc = kstrtou32(buf, 10, &val);
	if (rc < 0) {
		rc = -EINVAL;
		goto out;
	}

	feat_enables = CMDQ_ROCE_CFG_FEAT_ENABLES_ICRC_CHECK_DISABLE;
	switch (val) {
	case 1:
		feat_cfg = CMDQ_ROCE_CFG_FEAT_CFG_ICRC_CHECK_DISABLE;
		break;
	case 0:
		feat_cfg &= ~CMDQ_ROCE_CFG_FEAT_CFG_ICRC_CHECK_DISABLE;
		break;
	default:
		rc = -EINVAL;
		goto out;
	}

	rc = bnxt_qplib_roce_cfg(&rdev->rcfw, feat_cfg, feat_enables, &feat_cfg_cur);
out:
	vfree(buf);
	return (rc) ? rc : len;
}

static const struct file_operations bnxt_re_icrc_cfg_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = bnxt_re_read_icrc_cfg,
	.write = bnxt_re_disable_icrc,
};

static void bnxt_re_add_disable_icrc_cfg(struct bnxt_re_dev *rdev)
{
	if (!is_disable_icrc_supported(rdev->dev_attr->dev_cap_ext_flags2))
		return;

	(void)debugfs_create_file("disable_icrc", 0400, rdev->config_debugfs_dir,
				  rdev, &bnxt_re_icrc_cfg_ops);
}

static ssize_t enable_deferred_db_read(struct file *file, char __user *buf,
				       size_t count, loff_t *ppos)
{
	struct bnxt_re_dev *rdev = file->private_data;
	char tmp[4];

	snprintf(tmp, sizeof(tmp), "%d\n", rdev->deferred_db_enabled ? 1 : 0);
	return simple_read_from_buffer(buf, count, ppos, tmp, strlen(tmp));
}

static ssize_t enable_deferred_db_write(struct file *file, const char __user *buf,
					size_t count, loff_t *ppos)
{
	struct bnxt_re_dev *rdev = file->private_data;
	bool val;
	int ret;

	ret = kstrtobool_from_user(buf, count, &val);
	if (ret)
		return ret;

	rdev->deferred_db_enabled = val;
	return count;
}

static const struct file_operations enable_deferred_db_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = enable_deferred_db_read,
	.write = enable_deferred_db_write,
};

void bnxt_re_debugfs_add_pdev(struct bnxt_re_dev *rdev)
{
	const char *pdev_name;

	pdev_name = pci_name(rdev->en_dev->pdev);
	rdev->pdev_debug_dir = debugfs_create_dir(pdev_name,
						  bnxt_re_debugfs_root);
	if (IS_ERR_OR_NULL(rdev->pdev_debug_dir)) {
		dev_dbg(rdev_to_dev(rdev), "Unable to create debugfs %s",
			pdev_name);
		return;
	}

	rdev->deferred_db_enabled = 1;
	rdev->en_qp_dbg = 1;
	rdev->config_debugfs_dir = debugfs_create_dir("config",
						      rdev->pdev_debug_dir);
	if (IS_ERR_OR_NULL(rdev->config_debugfs_dir)) {
		dev_dbg(rdev_to_dev(rdev), "Unable to create config dir under debugfs\n");
		return;
	}

	debugfs_create_file("enable_deferred_db", 0600,
			    rdev->config_debugfs_dir, rdev, &enable_deferred_db_fops);

	bnxt_re_add_disable_icrc_cfg(rdev);
	bnxt_re_add_force_mirroring_cfg(rdev);
	bnxt_re_add_dbg_files(rdev);
	bnxt_re_add_hdbr_knobs(rdev);
	bnxt_re_add_udcc_dbg(rdev);
	bnxt_re_add_cq_coal_knobs(rdev);
	bnxt_re_add_cc_config(rdev);
}

void bnxt_re_debugfs_rem_pdev(struct bnxt_re_dev *rdev)
{
	debugfs_remove_recursive(rdev->cc_config);
	kfree(rdev->cc_config_params);
	bnxt_re_rem_hdbr_knobs(rdev);
	bnxt_re_rem_udcc_dbg(rdev);
	bnxt_re_rem_cq_coal_dbg(rdev);
	debugfs_remove_recursive(rdev->pdev_debug_dir);
	rdev->pdev_debug_dir = NULL;
}

static int bnxt_re_peer_mmap_info_show(struct seq_file *s, void *unused)
{
	struct bnxt_re_dev *rdev = s->private;
	struct bnxt_en_dev *en_dev;
	int i;

	if (!bnxt_re_is_rdev_valid(rdev))
		return -ENODEV;

	bnxt_ulp_get_peer_bar_maps(rdev->en_dev);
	if (!rdev->en_dev->bar_cnt) {
		seq_puts(s, "No peer mmap data available with bnxt_re\n");
		goto no_data;
	}
	en_dev = rdev->en_dev;
	seq_printf(s, "\tds_port     : %d\n", en_dev->ds_port);
	seq_printf(s, "\tauth_status : %d\n", en_dev->auth_status);
	seq_puts(s, "bnxt_re peer mmap info:\n");

	for (i = 0; i < rdev->en_dev->bar_cnt; i++) {
		seq_printf(s, "\tHPA[%d]:\t\t0x%llx\n", i, en_dev->bar_addr[i].hv_bar_addr);
		seq_printf(s, "\tGPA[%d]:\t\t0x%llx\n", i, en_dev->bar_addr[i].vm_bar_addr);
		seq_printf(s, "\tSize[%d]:\t0x%llx\n", i, en_dev->bar_addr[i].bar_size);
		seq_printf(s, "\tstatus[%d]:\t0x%x\n", i, en_dev->status[i]);
	}

no_data:
	seq_puts(s, "\n");
	return 0;
}

static int bnxt_re_peer_mmap_info_open(struct inode *inode, struct file *file)
{
	struct bnxt_re_dev *rdev = inode->i_private;

	return single_open(file, bnxt_re_peer_mmap_info_show, rdev);
}

static const struct file_operations bnxt_re_peer_mmap_info_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.open = bnxt_re_peer_mmap_info_open,
	.read = seq_read,
};

static ssize_t bnxt_re_rate_limit_cfg_write(struct file *fil, const char *buffer,
					    size_t len, loff_t *ppos)
{
	struct seq_file *m = fil->private_data;
	struct bnxt_re_dev *rdev = m->private;
	char *val_str, *buf;
	int rc = -EINVAL;
	u32 val;

	if (!_is_modify_qp_rate_limit_supported(rdev->dev_attr->dev_cap_ext_flags2))
		return -EOPNOTSUPP;

	buf = vzalloc(len + 1);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, len)) {
		rc = -EFAULT;
		goto out;
	}

	buf[len] = '\0';
	val_str = strsep(&buf, " \t");
	rc = kstrtou32(val_str, 10, &val);
	if (rc < 0) {
		rc = -EINVAL;
		goto out;
	}

	/* user input in kbps. 0 to reset */
	if (val && (val > rdev->dev_attr->rate_limit_max ||
		    val < rdev->dev_attr->rate_limit_min)) {
		dev_err(rdev_to_dev(rdev), "User provided kbps rate_limit not "
			"within range 0 (reset) %d min and %d max kbps\n",
			rdev->dev_attr->rate_limit_min, rdev->dev_attr->rate_limit_max);
		rc = -EINVAL;
		goto out;
	}
	if (!val)
		dev_info(rdev_to_dev(rdev), "User rate limit cfg turned off\n");
	rdev->qp_rate_limit_cfg = val;

out:
	vfree(buf);
	return (rc) ? rc : len;
}

static int bnxt_re_rate_limit_cfg_show(struct seq_file *s, void *unused)
{
	struct bnxt_re_dev *rdev = s->private;

	if (!bnxt_re_is_rdev_valid(rdev))
		return -ENODEV;

	seq_printf(s, "\tConfigured User rate limit: %d kbps\n", rdev->qp_rate_limit_cfg);

	seq_puts(s, "\n");
	return 0;
}

static int bnxt_re_rate_limit_cfg_open(struct inode *inode, struct file *file)
{
	struct bnxt_re_dev *rdev = inode->i_private;

	return single_open(file, bnxt_re_rate_limit_cfg_show, rdev);
}

static const struct file_operations bnxt_re_rate_limit_cfg_ops = {
	.owner = THIS_MODULE,
	.open = bnxt_re_rate_limit_cfg_open,
	.write = bnxt_re_rate_limit_cfg_write,
	.read = seq_read,
};

void bnxt_re_debugfs_add_port(struct bnxt_re_dev *rdev, char *dev_name)
{
	if (!rdev->en_dev)
		return;

	rdev->port_debug_dir = debugfs_create_dir(dev_name,
						  bnxt_re_debugfs_root);
	rdev->info = debugfs_create_file("info", 0400,
					 rdev->port_debug_dir, rdev,
					 &bnxt_re_info_dbg_ops);
	rdev->sp_perf_stats = debugfs_create_file("sp_perf_stats", 0644,
						  rdev->port_debug_dir, rdev,
						  &bnxt_re_perf_dbg_ops);
	rdev->drv_dbg_stats = debugfs_create_file("drv_dbg_stats", 0644,
						  rdev->port_debug_dir, rdev,
						  &bnxt_re_drv_stats_dbg_ops);
	rdev->peer_mmap = debugfs_create_file("peer_mmap", 0400,
					      rdev->port_debug_dir, rdev,
					      &bnxt_re_peer_mmap_info_ops);
	rdev->rate_limit_cfg = debugfs_create_file("rate_limit_cfg", 0644,
						   rdev->port_debug_dir, rdev,
						   &bnxt_re_rate_limit_cfg_ops);
}

void bnxt_re_rem_dbg_files(struct bnxt_re_dev *rdev)
{
	debugfs_remove_recursive(rdev->pdev_qpinfo_dir);
	rdev->pdev_qpinfo_dir = NULL;
	debugfs_remove_recursive(rdev->pdev_debug_dump_dir);
	rdev->pdev_debug_dump_dir = NULL;
}

void bnxt_re_debugfs_rem_port(struct bnxt_re_dev *rdev)
{
	debugfs_remove_recursive(rdev->port_debug_dir);
	rdev->port_debug_dir = NULL;
	rdev->info = NULL;
}

void bnxt_re_debugfs_remove(void)
{
	debugfs_remove_recursive(bnxt_re_debugfs_root);
	bnxt_re_debugfs_root = NULL;
}

void bnxt_re_debugfs_init(void)
{
	bnxt_re_debugfs_root = debugfs_create_dir(ROCE_DRV_MODULE_NAME, NULL);
	if (IS_ERR_OR_NULL(bnxt_re_debugfs_root)) {
		dev_dbg(NULL, "%s: Unable to create debugfs root directory ",
			ROCE_DRV_MODULE_NAME);
		dev_dbg(NULL, "with err 0x%lx", PTR_ERR(bnxt_re_debugfs_root));
		return;
	}
}
#endif
