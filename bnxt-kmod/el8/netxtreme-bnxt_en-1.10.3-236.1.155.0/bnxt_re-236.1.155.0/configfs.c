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
 * Description: Enables configfs interface
 */

#include "configfs.h"
#include "bnxt.h"

#ifdef CONFIG_BNXT_DCB
u8 bnxt_re_get_priority_mask(struct bnxt_re_dev *rdev, u8 selector)
{
	struct net_device *netdev;
	struct dcb_app app;
	u8 prio_map = 0, tmp_map = 0;

	netdev = rdev->en_dev->net;
	memset(&app, 0, sizeof(app));
	if (selector & IEEE_8021QAZ_APP_SEL_ETHERTYPE) {
		app.selector = IEEE_8021QAZ_APP_SEL_ETHERTYPE;
		app.protocol = BNXT_RE_ROCE_V1_ETH_TYPE;
		tmp_map = dcb_ieee_getapp_mask(netdev, &app);
		prio_map = tmp_map;
	}

	if (selector & IEEE_8021QAZ_APP_SEL_DGRAM) {
		app.selector = IEEE_8021QAZ_APP_SEL_DGRAM;
		app.protocol = BNXT_RE_ROCE_V2_PORT_NO;
		tmp_map = dcb_ieee_getapp_mask(netdev, &app);
		prio_map |= tmp_map;
	}

	return prio_map;
}
#else
u8 bnxt_re_get_priority_mask(struct bnxt_re_dev *rdev, u8 selector)
{
	return 0;
}
#endif /* CONFIG_BNXT_DCB */

static const char *mode_name[] = {"DCQCN-D", "TCP", "Invalid"};
static const char *mode_name_p5[] = {"DCQCN-D", "DCQCN-P", "Invalid"};

static const char *_get_mode_str(u8 mode, bool is_p5)
{
	return is_p5 ?  mode_name_p5[mode] : mode_name[mode];
}

static struct bnxt_re_dev *__get_rdev_from_name(const char *name)
{
	struct bnxt_re_dev *rdev;
	u8 found = false;

	mutex_lock(&bnxt_re_mutex);
	list_for_each_entry(rdev, &bnxt_re_dev_list, list) {
		if (!strcmp(name, rdev->ibdev.name)) {
			found = true;
			break;
		}
	}
	mutex_unlock(&bnxt_re_mutex);

	return found ? rdev : ERR_PTR(-ENODEV);
}

static struct bnxt_re_dev *bnxt_re_get_valid_rdev(struct bnxt_re_cfg_group *ccgrp)
{
	struct bnxt_re_dev *rdev = NULL;

	if (ccgrp->portgrp && ccgrp->portgrp->devgrp)
		rdev = __get_rdev_from_name(ccgrp->portgrp->devgrp->name);

	if (!rdev || (PTR_ERR(rdev) == -ENODEV))
	{
		dev_dbg(NULL, "bnxt_re: %s: Invalid rdev received rdev = %p\n",
			__func__, ccgrp->rdev);
		return NULL;
	}


	if (ccgrp->rdev != rdev)
		ccgrp->rdev = rdev;

	return rdev;
}

static int bnxt_re_is_dscp_mapping_set(u32 mask)
{
	return (mask &
		(CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP |
		 CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_TOS_DSCP));
}

static int bnxt_re_init_d2p_map(struct bnxt_re_dev *rdev,
				struct bnxt_re_dscp2pri *d2p)
{
	u32 cc_mask;
	int mapcnt = 0;

	cc_mask = rdev->cc_param.mask;

	if (!bnxt_re_is_dscp_mapping_set(rdev->cc_param.mask))
		goto bail;

	if (cc_mask & CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP ||
	    cc_mask & BNXT_QPLIB_CC_PARAM_MASK_ROCE_PRI) {
		d2p->dscp = rdev->cc_param.tos_dscp;
		d2p->pri = rdev->cc_param.roce_pri;
		d2p->mask = 0x3F;
		mapcnt++;
		d2p++;
	}

	if (cc_mask & CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_TOS_DSCP ||
	    cc_mask & CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_VLAN_PCP) {
		d2p->dscp = rdev->cc_param.alt_tos_dscp;
		d2p->pri = rdev->cc_param.alt_vlan_pcp;
		d2p->mask = 0x3F;
		mapcnt++;
	}
bail:

	return mapcnt;
}

static int __bnxt_re_clear_dscp(struct bnxt_re_dev *rdev, u16 portid)
{
	int rc = 0;
	u16 i;

	/* Get older values to be reset. Set mask to 0 */
	rc = bnxt_re_query_hwrm_dscp2pri(rdev, portid);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to query dscp on pci function %d\n",
			bnxt_re_dev_pcifn_id(rdev));
		goto bail;
	}
	if (!rdev->d2p_count)
		goto bail;

	/* Clear mask of all d2p mapping in HW */
	for (i = 0; i < rdev->d2p_count;  i++)
		rdev->d2p[i].mask = 0;

	rc = bnxt_re_set_hwrm_dscp2pri(rdev, rdev->d2p, rdev->d2p_count, portid);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to clear dscp on pci function %d\n",
			bnxt_re_dev_pcifn_id(rdev));
		goto bail;
	}
bail:
	return rc;
}

int bnxt_re_clear_dscp(struct bnxt_re_dev *rdev)
{
	int rc = 0;
	u16 portid;

	/*
	 * Target ID to be specified.
	 * 0xFFFF - if issued for the same function
	 * function_id - if issued for another function
	 */
	portid = 0xFFFF;
	rc = __bnxt_re_clear_dscp(rdev, portid);
	if (rc)
		goto bail;

	if (rdev->binfo) {
		/* Function id of the second function to be
		 * specified.
		 */
		portid = (PCI_FUNC(rdev->binfo->pdev2->devfn) + 1);
		rc = __bnxt_re_clear_dscp(rdev, portid);
		if (rc)
			goto bail;
	}
bail:
	return rc;
}

static int __bnxt_re_setup_dscp(struct bnxt_re_dev *rdev, u16 portid)
{
	struct bnxt_re_dscp2pri d2p[2] = {};
	int rc = 0, mapcnt = 0;

	/*Init dscp to pri map */
	mapcnt = bnxt_re_init_d2p_map(rdev, d2p);
	if (!mapcnt)
		goto bail;
	rc = bnxt_re_set_hwrm_dscp2pri(rdev, d2p, mapcnt, portid);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to updated dscp on pci function %d\n",
			bnxt_re_dev_pcifn_id(rdev));
		goto bail;
	}
bail:
	return rc;
}

int bnxt_re_setup_dscp(struct bnxt_re_dev *rdev)
{
	int rc = 0;
	u16 portid;

	/*
	 * Target ID to be specified.
	 * 0xFFFF - if issued for the same function
	 * function_id - if issued for another function
	 */
	portid = 0xFFFF;
	rc = __bnxt_re_setup_dscp(rdev, portid);
	if (rc)
		goto bail;

	if (rdev->binfo) {
		/* Function id of the second function to be
		 * specified.
		 */
		portid = (PCI_FUNC(rdev->binfo->pdev2->devfn) + 1);
		rc = __bnxt_re_setup_dscp(rdev, portid);
		if (rc)
			goto bail;
	}
bail:
	return rc;
}

static struct bnxt_re_cfg_group *__get_cc_group(struct config_item *item)
{
	struct config_group *group = container_of(item, struct config_group,
						  cg_item);
	struct bnxt_re_cfg_group *ccgrp =
			container_of(group, struct bnxt_re_cfg_group, group);
	return ccgrp;
}

static bool _is_cc_gen1_plus(struct bnxt_re_dev *rdev)
{
	u16 cc_gen;

	cc_gen = rdev->dev_attr->dev_cap_flags &
		 CREQ_QUERY_FUNC_RESP_SB_CC_GENERATION_MASK;
	return cc_gen >= CREQ_QUERY_FUNC_RESP_SB_CC_GENERATION_CC_GEN1;
}

static int print_cc_gen1_adv(struct bnxt_qplib_cc_param_ext *cc_ext, char *buf)
{
	int bytes = 0;

	bytes += sprintf(buf + bytes,
			 "min_time_bet_cnp=%#x\t\t# minimum time between cnp\t\t: %u usec\n",
			 cc_ext->min_delta_cnp, cc_ext->min_delta_cnp);
	bytes += sprintf(buf + bytes, "init_cp=%#x\t\t\t# initial congestion probability\t: %u\n",
			 cc_ext->init_cp, cc_ext->init_cp);
	bytes += sprintf(buf + bytes, "tr_update_mode=%#x\t\t# target rate update mode\t\t: %u\n",
			 cc_ext->tr_update_mode, cc_ext->tr_update_mode);
	bytes += sprintf(buf + bytes, "tr_update_cyls=%#x\t\t# target rate update cycle\t\t: %u\n",
			 cc_ext->tr_update_cyls, cc_ext->tr_update_cyls);
	bytes += sprintf(buf + bytes, "fr_num_rtts=%#x\t\t\t# fast recovery rtt\t\t\t: %u\n",
			 cc_ext->fr_rtt, cc_ext->fr_rtt);
	bytes += sprintf(buf + bytes, "ai_rate_incr=%#x\t\t# active increase time quanta\t\t: %u\n",
			 cc_ext->ai_rate_incr, cc_ext->ai_rate_incr);
	bytes += sprintf(buf + bytes,
			 "red_rel_rtts_th=%#x\t\t# reduc. relax rtt threshold\t\t: %u rtts\n",
			 cc_ext->rr_rtt_th, cc_ext->rr_rtt_th);
	bytes += sprintf(buf + bytes,
			 "act_rel_cr_th=%#x\t\t# additional relax cr rtt\t\t: %u rtts\t\n",
			 cc_ext->ar_cr_th, cc_ext->ar_cr_th);
	bytes += sprintf(buf + bytes,
			 "cr_min_th=%#x\t\t\t# minimum current rate threshold\t: %u\n",
			 cc_ext->cr_min_th, cc_ext->cr_min_th);
	bytes += sprintf(buf + bytes, "bw_avg_weight=%#x\t\t# bandwidth weight\t\t\t: %u\n",
			 cc_ext->bw_avg_weight, cc_ext->bw_avg_weight);
	bytes += sprintf(buf + bytes, "act_cr_factor=%#x\t\t# actual current rate factor\t\t: %u\n",
			 cc_ext->cr_factor, cc_ext->cr_factor);
	bytes += sprintf(buf + bytes,
			 "max_cp_cr_th=%#x\t\t# current rate level to max cp\t\t: %u\n",
			 cc_ext->cr_th_max_cp, cc_ext->cr_th_max_cp);
	bytes += sprintf(buf + bytes, "cp_bias_en=%#x\t\t\t# cp bias state\t\t\t\t: %s\n",
			 cc_ext->cp_bias_en, cc_ext->cp_bias_en ? "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes, "cp_bias=%#x\t\t\t# log of cr fraction added to cp\t: %u\n",
			 cc_ext->cp_bias, cc_ext->cp_bias);
	bytes += sprintf(buf + bytes, "reset_cc_cr_th=%#x\t\t# cr threshold to reset cc\t\t: %u\n",
			 cc_ext->cc_cr_reset_th, cc_ext->cc_cr_reset_th);
	bytes += sprintf(buf + bytes, "tr_lb=%#x\t\t\t# target rate lower bound\t\t: %u\n",
			 cc_ext->tr_lb, cc_ext->tr_lb);
	bytes += sprintf(buf + bytes,
			 "cr_prob_fac=%#x\t\t\t# current rate probability factor\t: %u\n",
			 cc_ext->cr_prob_fac, cc_ext->cr_prob_fac);
	bytes += sprintf(buf + bytes,
			 "tr_prob_fac=%#x\t\t\t# target rate probability factor\t: %u\n",
			 cc_ext->tr_prob_fac, cc_ext->tr_prob_fac);
	bytes += sprintf(buf + bytes,
			 "fair_cr_th=%#x\t\t\t# current rate fairness threshold\t: %u\n",
			 cc_ext->fair_cr_th, cc_ext->fair_cr_th);
	bytes += sprintf(buf + bytes, "red_div=%#x\t\t\t# reduction divider\t\t\t: %u\n",
			 cc_ext->red_div, cc_ext->red_div);
	bytes += sprintf(buf + bytes,
			 "cnp_ratio_th=%#x\t\t# rate reduction threshold\t\t: %u cnps\n",
			 cc_ext->cnp_ratio_th, cc_ext->cnp_ratio_th);
	bytes += sprintf(buf + bytes,
			 "exp_ai_rtts=%#x\t\t\t# extended no congestion rtts\t\t: %u rtt\n",
			 cc_ext->ai_ext_rtt, cc_ext->ai_ext_rtt);
	bytes += sprintf(buf + bytes, "exp_crcp_ratio=%#x\t\t# log of cp to cr ratio\t\t\t: %u\n",
			 cc_ext->exp_crcp_ratio, cc_ext->exp_crcp_ratio);
	bytes += sprintf(buf + bytes, "rt_en=%#x\t\t\t# use lower rate table entries\t\t: %s\n",
			 cc_ext->low_rate_en, cc_ext->low_rate_en ? "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes,
			 "cp_exp_update_th=%#x\t\t# rtts to start cp track cr\t\t: %u rtt\n",
			 cc_ext->cpcr_update_th, cc_ext->cpcr_update_th);
	bytes += sprintf(buf + bytes,
			 "ai_rtt_th1=%#x\t\t\t# first threshold to rise ai\t\t: %u rtt\n",
			 cc_ext->ai_rtt_th1, cc_ext->ai_rtt_th1);
	bytes += sprintf(buf + bytes,
			 "ai_rtt_th2=%#x\t\t\t# second threshold to rise ai\t\t: %u rtt\n",
			 cc_ext->ai_rtt_th2, cc_ext->ai_rtt_th2);
	bytes += sprintf(buf + bytes,
			 "cf_rtt_th=%#x\t\t\t# actual rate base reduction threshold\t: %u rtt\n",
			 cc_ext->cf_rtt_th, cc_ext->cf_rtt_th);
	bytes += sprintf(buf + bytes,
			 "sc_cr_th1=%#x\t\t\t# first severe cong. cr threshold\t: %u\n",
			 cc_ext->sc_cr_th1, cc_ext->sc_cr_th1);
	bytes += sprintf(buf + bytes,
			 "sc_cr_th2=%#x\t\t\t# second severe cong. cr threshold\t: %u\n",
			 cc_ext->sc_cr_th2, cc_ext->sc_cr_th2);
	bytes += sprintf(buf + bytes, "cc_ack_bytes=%#x\t\t# cc ack bytes\t\t\t\t: %u\n",
			 cc_ext->cc_ack_bytes, cc_ext->cc_ack_bytes);
	bytes += sprintf(buf + bytes,
			 "reduce_cf_rtt_th=%#x\t\t# reduce to init rtts threshold\t\t: %u rtt\n",
			 cc_ext->reduce_cf_rtt_th, cc_ext->reduce_cf_rtt_th);
	bytes += sprintf(buf + bytes, "rnd_no_red_en=%#x\t\t# random no reduction of cr\t\t: %s\n",
			 cc_ext->random_no_red_en,
			 cc_ext->random_no_red_en ? "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes,
			 "act_cr_sh_cor_en=%#x\t\t# actual cr shift correction\t\t: %s\n",
			 cc_ext->actual_cr_shift_correction_en,
			 cc_ext->actual_cr_shift_correction_en ? "Enabled" : "Disabled");

	return bytes;
}

static int print_cc_gen1(struct bnxt_qplib_cc_param_ext *cc_ext, char *buf,
			 u8 show_adv_cc)
{
	int bytes = 0;

	bytes += sprintf(buf + bytes, "rtt_jitter_en=%#x\t\t# rtt jitter\t\t\t\t: %s\n",
			 cc_ext->rtt_jitter_en, cc_ext->rtt_jitter_en ? "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes, "cnp_ecn=%#x\t\t\t# cnp header ecn status\t\t\t: %s\n",
			 cc_ext->cnp_ecn, !cc_ext->cnp_ecn ? "Not-ECT" :
			 (cc_ext->cnp_ecn == 0x01) ? "ECT(1)" : "ECT(0)");
	bytes += sprintf(buf + bytes,
			 "lbytes_per_usec=%#x\t\t# link bytes per usec\t\t\t: %u byte/usec\n",
			 cc_ext->bytes_per_usec, cc_ext->bytes_per_usec);
	bytes += sprintf(buf + bytes, "cr_width=%#x\t\t\t# current rate width\t\t\t: %u bits\n",
			 cc_ext->cr_width, cc_ext->cr_width);
	bytes += sprintf(buf + bytes, "min_quota=%#x\t\t\t# minimum quota period\t\t\t: %u\n",
			 cc_ext->min_quota, cc_ext->min_quota);
	bytes += sprintf(buf + bytes, "max_quota=%#x\t\t\t# maximum quota period\t\t\t: %u\n",
			 cc_ext->max_quota, cc_ext->max_quota);
	bytes += sprintf(buf + bytes,
			 "abs_max_quota=%#x\t\t# absolute maximum quota period\t\t: %u\n",
			 cc_ext->abs_max_quota, cc_ext->abs_max_quota);
	bytes += sprintf(buf + bytes, "l64B_per_rtt=%#x\t\t# 64B transmitted in one rtt\t\t: %u\n",
			 cc_ext->l64B_per_rtt, cc_ext->l64B_per_rtt);
	/* Print advanced parameters */
	if (show_adv_cc)
		bytes += print_cc_gen1_adv(cc_ext, (buf+bytes));
	return bytes;
}

static int print_cc_gen2(struct bnxt_qplib_cc_param_ext2 *cc_ext2, char *buf)
{
	int i, bytes = 0;

	bytes += sprintf(buf + bytes, "dcn_qlevel_tbl_thr\t\t\t\t\t: [");
	for (i = 0; i < 7; i++)
		bytes += sprintf(buf + bytes, "%x,", cc_ext2->dcn_qlevel_tbl_thr[i]);
	bytes += sprintf(buf + bytes, "%x]\n", cc_ext2->dcn_qlevel_tbl_thr[i]);
	bytes += sprintf(buf + bytes, "dcn_qlevel_tbl_act\t\t\t\t\t: [");
	for (i = 0; i < 7; i++)
		bytes += sprintf(buf + bytes, "%x,", cc_ext2->dcn_qlevel_tbl_act[i]);
	bytes += sprintf(buf + bytes, "%x]\n", cc_ext2->dcn_qlevel_tbl_act[i]);

	return bytes;
}

static int print_cc_gen1_ext(struct bnxt_qplib_cc_param_gen1_ext *cc_gen1_ext, char *buf)
{
	int i, bytes = 0;

	bytes += sprintf(buf + bytes,
			"maximum no red probability threshold (rnd_no_red_mult)\t: %#x\n",
			cc_gen1_ext->rnd_no_red_mult);
	bytes += sprintf(buf + bytes,
			"threshold for cong. free rtts (reduce_init_cong_fr_rtts_th): %#x\n",
			cc_gen1_ext->reduce2_init_cong_free_rtts_th);
	bytes += sprintf(buf + bytes, "offset to rtt time adjust (no_red_off)\t\t\t: %#x\n",
			cc_gen1_ext->no_red_offset);
	bytes += sprintf(buf + bytes, "quota period adjust count (per_adj_cnt)\t\t\t: %#x\n",
			cc_gen1_ext->period_adjust_count);
	bytes += sprintf(buf + bytes, "first current rate quota period threshold (cr_th1)\t: %#x\n",
			cc_gen1_ext->current_rate_threshold_1);
	bytes += sprintf(buf + bytes,
			"second current rate quota period threshold (cr_th2)\t: %#x\n",
			cc_gen1_ext->current_rate_threshold_2);
	bytes += sprintf(buf + bytes,
			"init values on cong. free rtts threshold (reduce_init_en): %s\n",
			cc_gen1_ext->reduce2_init_en ? "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes, "rate_table_quota_period\t\t\t\t\t: [");
	for (i = 0; i < 23; i++)
		bytes += sprintf(buf + bytes, "%x,", cc_gen1_ext->rate_table_quota_period[i]);
	bytes += sprintf(buf + bytes, "%x]\n", cc_gen1_ext->rate_table_quota_period[i]);
	bytes += sprintf(buf + bytes, "rate_table_byte_quota\t\t\t\t\t: [");
	for (i = 0; i < 23; i++)
		bytes += sprintf(buf + bytes, "%x,", cc_gen1_ext->rate_table_byte_quota[i]);
	bytes += sprintf(buf + bytes, "%x]\n", cc_gen1_ext->rate_table_byte_quota[i]);

	return bytes;
}

static int print_cc_gen2_ext(struct bnxt_qplib_cc_param_gen2_ext *cc_gen2_ext, char *buf)
{
	int bytes = 0;

	bytes += sprintf(buf + bytes, "link number of 64b per usec (cr2bw_64b_ratio)\t\t: %#x\n",
			cc_gen2_ext->cr2bw_64b_ratio);
	bytes += sprintf(buf + bytes, "retx_cp\t\t\t\t\t\t\t: %#x\n",
			cc_gen2_ext->retx_cp);
	bytes += sprintf(buf + bytes, "retx_cr\t\t\t\t\t\t\t: %#x\n",
			cc_gen2_ext->retx_cr);
	bytes += sprintf(buf + bytes, "retx_tr\t\t\t\t\t\t\t: %#x\n",
			cc_gen2_ext->retx_tr);
	bytes += sprintf(buf + bytes,
			"retransmit reset cc current rate threshold (retx_reset_cc_cr_th): %#x\n",
			cc_gen2_ext->hw_retx_reset_cc_cr_th);
	bytes += sprintf(buf + bytes, "retransmit cc reset enables (retx_cc_reset_en)\t\t: %s\n",
			cc_gen2_ext->hw_retx_cc_reset_en ? "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes, "actual cr computation enhancement (act_cr_cn)\t\t: %s\n",
			cc_gen2_ext->sr2_cc_first_cnp_en ? "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes,
			"No random reduction on transmit (rnd_no_red_on_tx_en)\t: %s\n",
			cc_gen2_ext->sr2_cc_actual_cr_en ? "Enabled" : "Disabled");
	return bytes;
}

/* This function will print GEN2_EXT and GEN1_EXT parameters except Rate table */
static int print_cc_gen1_2_ext(struct bnxt_re_dev *rdev,
			       struct bnxt_qplib_cc_param *ccparam, char *buf)
{
	struct bnxt_qplib_cc_param_gen1_ext *cc_gen1_ext;
	struct bnxt_qplib_cc_param_gen2_ext *cc_gen2_ext;
	int bytes = 0;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		goto print_gen2_param;

	cc_gen1_ext = &ccparam->cc_gen1_ext;
	bytes += sprintf(buf + bytes,
			 "rnd_no_red_mult=%#x\t\t# maximum no red probability threshold\t: %u\n",
			 cc_gen1_ext->rnd_no_red_mult, cc_gen1_ext->rnd_no_red_mult);
	bytes += sprintf(buf + bytes,
			 "reduce_init_cong_fr_rtts_th=%#x\t# threshold for cong. free rtts\t: %u\n",
			 cc_gen1_ext->reduce2_init_cong_free_rtts_th,
			 cc_gen1_ext->reduce2_init_cong_free_rtts_th);
	bytes += sprintf(buf + bytes,
			 "no_red_off=%#x\t\t\t# offset to rtt time adjust\t\t: %u\n",
			 cc_gen1_ext->no_red_offset, cc_gen1_ext->no_red_offset);
	bytes += sprintf(buf + bytes,
			 "per_adj_cnt=%#x\t\t\t# quota period adjust count\t\t: %u\n",
			 cc_gen1_ext->period_adjust_count, cc_gen1_ext->period_adjust_count);
	bytes += sprintf(buf + bytes,
			 "cr_th1=%#x\t\t\t# first current rate quota period threshold\t\t: %u\n",
			 cc_gen1_ext->current_rate_threshold_1,
			 cc_gen1_ext->current_rate_threshold_1);
	bytes += sprintf(buf + bytes,
			 "cr_th2=%#x\t\t\t# second current rate quota period threshold\t\t: %u\n",
			 cc_gen1_ext->current_rate_threshold_2,
			 cc_gen1_ext->current_rate_threshold_2);
	bytes += sprintf(buf + bytes,
			 "reduce_init_en=%#x\t\t# init values on cong. free rtts threshold\t\t: %s\n",
			 cc_gen1_ext->reduce2_init_en,
			 cc_gen1_ext->reduce2_init_en ? "Enabled" : "Disabled");

print_gen2_param:
	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return bytes;

	cc_gen2_ext = &ccparam->cc_gen2_ext;
	bytes += sprintf(buf + bytes,
			 "cr2bw_64b_ratio=%#x\t\t# link number of 64b per usec\t\t\t\t: %u\n",
			 cc_gen2_ext->cr2bw_64b_ratio, cc_gen2_ext->cr2bw_64b_ratio);
	bytes += sprintf(buf + bytes,
			 "retx_cp=%#x\t\t\t# starting value of CP for re-Tx \t\t\t: %u\n",
			 cc_gen2_ext->retx_cp, cc_gen2_ext->retx_cp);
	bytes += sprintf(buf + bytes,
			 "retx_cr=%#x\t\t\t# starting value of current rate for re-Tx \t\t: %u\n",
			 cc_gen2_ext->retx_cr, cc_gen2_ext->retx_cr);
	bytes += sprintf(buf + bytes,
			 "retx_tr=%#x\t\t\t# starting value of target rate for re-Tx \t\t: %u\n",
			 cc_gen2_ext->retx_tr, cc_gen2_ext->retx_tr);
	bytes += sprintf(buf + bytes,
			 "retx_reset_cc_cr_th=%#x\t\t# re-Tx reset cc current rate threshold\t\t\t: %u\n",
			 cc_gen2_ext->hw_retx_reset_cc_cr_th, cc_gen2_ext->hw_retx_reset_cc_cr_th);
	bytes += sprintf(buf + bytes, "retx_cc_reset_en=%#x\t\t# retransmit cc reset enables\t\t\t\t: %s\n",
			 cc_gen2_ext->hw_retx_cc_reset_en, cc_gen2_ext->hw_retx_cc_reset_en ?
			 "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes, "act_cr_cn=%#x\t\t\t# retransmit cc reset enables\t\t\t\t: %s\n",
			 cc_gen2_ext->sr2_cc_first_cnp_en, cc_gen2_ext->sr2_cc_first_cnp_en ?
			 "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes,
			 "rnd_no_red_on_tx_en=%#x\t\t# No random reduction on transmit\t\t\t: %s\n",
			 cc_gen2_ext->sr2_cc_first_cnp_en, cc_gen2_ext->sr2_cc_first_cnp_en ?
			 "Enabled" : "Disabled");
	return bytes;
}

static ssize_t apply_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param ccparam = {0};
	struct bnxt_qplib_drv_modes *drv_mode;
	struct bnxt_re_dev *rdev;
	int rc = 0, bytes = 0;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	drv_mode = &rdev->chip_ctx->modes;
	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &ccparam);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"%s: Failed to query CC parameters\n", __func__);
		bytes = rc;
		goto out;
	}

	bytes += sprintf(buf + bytes, "ecn_enable=%#x\t\t\t# ecn status\t\t\t\t: %s\n",
			 ccparam.enable, ccparam.enable ? "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes, "ecn_marking=%#x\t\t\t# ecn marking\t\t\t\t: %s\n",
			 ccparam.tos_ecn, !ccparam.tos_ecn ? "Not-ECT" :
			 (ccparam.tos_ecn == 0x01) ? "ECT(1)" : "ECT(0)");
	bytes += sprintf(buf + bytes, "cc_mode=%#x\t\t\t# congestion control mode\t\t: %s\n",
			 ccparam.cc_mode, _get_mode_str(ccparam.cc_mode, _is_cc_gen1_plus(rdev)));
	bytes += sprintf(buf + bytes, "g=%#x\t\t\t\t# running avg. weight\t\t\t: %u\n",
			 ccparam.g, ccparam.g);
	bytes += sprintf(buf + bytes, "inact_th=%#x\t\t\t# inactivity threshold\t\t\t: %u usec\n",
			 ccparam.inact_th, ccparam.inact_th);
	bytes += sprintf(buf + bytes, "init_cr=%#x\t\t\t# initial current rate\t\t\t: %u\n",
			 ccparam.init_cr, ccparam.init_cr);
	bytes += sprintf(buf + bytes, "init_tr=%#x\t\t\t# initial target rate\t\t\t: %u\n",
			 ccparam.init_tr, ccparam.init_tr);

	if (drv_mode->cc_pr_mode) {
		bytes += sprintf(buf + bytes, "rtt=%#x\t\t\t# round trip time\t\t\t: %u usec\n",
				 ccparam.rtt, ccparam.rtt);
		if (!_is_chip_p5_plus(rdev->chip_ctx)) {
			bytes += sprintf(buf+bytes,
					 "phases in fast recovery\t\t\t: %u\n",
					 ccparam.nph_per_state);
			bytes += sprintf(buf+bytes,
					 "quanta in recovery phase\t\t: %u\n",
					 ccparam.time_pph);
			bytes += sprintf(buf+bytes,
					 "packets in recovery phase\t\t: %u\n",
					 ccparam.pkts_pph);
		}

		if (ccparam.cc_mode == 1 && !_is_cc_gen1_plus(rdev)) {
			bytes += sprintf(buf+bytes,
					 "tcp congestion probability\t\t: %#x\n",
					 ccparam.tcp_cp);
		}
	}

	if (_is_chip_p5_plus(rdev->chip_ctx)) {
		bytes += print_cc_gen1(&ccparam.cc_ext, (buf + bytes),
				       drv_mode->cc_pr_mode);

		bytes += print_cc_gen1_2_ext(rdev, &ccparam, (buf + bytes));
	}

out:
	return bytes;
}

static int bnxt_re_program_cnp_dcb_values(struct bnxt_re_dev *rdev)
{
	int rc;

	rc = bnxt_re_setup_cnp_cos(rdev, !is_cc_enabled(rdev));
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to setup cnp cos\n");
		goto exit;
	}

	/* Clear the previous dscp table */
	rc = bnxt_re_clear_dscp(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to clear the dscp - pri table\n");
		goto exit;
	}
	if (!is_cc_enabled(rdev)) {
		/*
		 * Reset the CNP pri and dscp masks if
		 * dscp is not programmed
		 */
		rdev->cc_param.mask &=
			(~CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_VLAN_PCP);
		rdev->cc_param.mask &=
			(~CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_TOS_DSCP);
		rdev->cc_param.alt_tos_dscp = 0;
		rdev->cc_param.alt_vlan_pcp = 0;
	}
	/* Setup cnp and roce dscp */
	rc = bnxt_re_setup_dscp(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to setup the dscp - pri table\n");
		goto exit;
	}
	return 0;

exit:
	return rc;
}

static int bnxt_re_program_cc_params(struct bnxt_re_dev *rdev)
{
	int rc = 0;

	if (rdev->cc_param.mask || rdev->cc_param.cc_ext.ext_mask ||
	    rdev->cc_param.cc_ext2.ext2_mask || rdev->cc_param.cc_gen1_ext.gen1_ext_mask ||
	    rdev->cc_param.cc_gen2_ext.gen2_ext_mask) {
		if (!is_qport_service_type_supported(rdev)) {
			rc = bnxt_re_program_cnp_dcb_values(rdev);
			if (rc) {
				dev_err(rdev_to_dev(rdev),
					"Failed to set cnp values\n");
				goto clear_mask;
			}
		}
		rc = bnxt_qplib_modify_cc(&rdev->qplib_res,
					  &rdev->cc_param);
		if (rc)
			dev_err(rdev_to_dev(rdev),
				"Failed to apply cc settings\n");
	}
clear_mask:
	rdev->cc_param.mask = 0;
	return rc;
}

static ssize_t apply_store(struct config_item *item, const char *buf,
			   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val;
	u8 prio_map;
	int rc = 0;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	mutex_lock(&rdev->cc_lock);
	if (val == BNXT_RE_MODIFY_CC) {
		/* Update current priority setting */
		prio_map = bnxt_re_get_priority_mask(rdev,
				(IEEE_8021QAZ_APP_SEL_ETHERTYPE |
				 IEEE_8021QAZ_APP_SEL_DGRAM));
		if (rdev->cur_prio_map != prio_map)
			rdev->cur_prio_map = prio_map;

		rc = bnxt_re_program_cc_params(rdev);
	}
	mutex_unlock(&rdev->cc_lock);
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, apply);

static ssize_t ext_settings_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param ccparam = {0};
	struct bnxt_re_dev *rdev;
	int rc = 0, bytes = 0;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!(BNXT_RE_DCN_ENABLED(rdev->rcfw.res) ||
	      BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res) ||
	      BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res)))
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &ccparam);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"%s: Failed to query CC parameters\n", __func__);
		bytes = rc;
		goto out;
	}

	if (BNXT_RE_DCN_ENABLED(rdev->rcfw.res))
		bytes += print_cc_gen2(&ccparam.cc_ext2, (buf + bytes));

	if (BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		bytes += print_cc_gen1_ext(&ccparam.cc_gen1_ext, (buf + bytes));

	if (BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		bytes += print_cc_gen2_ext(&ccparam.cc_gen2_ext, (buf + bytes));

out:
	return bytes;
}
CONFIGFS_ATTR_RO(, ext_settings);

static ssize_t advanced_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_drv_modes *drv_mode;
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	drv_mode = &rdev->chip_ctx->modes;
	return sprintf(buf, "%#x\n", drv_mode->cc_pr_mode);
}

static ssize_t advanced_store(struct config_item *item, const char *buf,
				   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_drv_modes *drv_mode;
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	drv_mode = &rdev->chip_ctx->modes;
	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	if (val > 0)
		drv_mode->cc_pr_mode = BNXT_RE_CONFIGFS_SHOW_ADV_CC_PARAMS;
	else
		drv_mode->cc_pr_mode = BNXT_RE_CONFIGFS_HIDE_ADV_CC_PARAMS;
	return strnlen(buf, count);
}
CONFIGFS_ATTR(, advanced);

static int bnxt_modify_one_cc_param(struct bnxt_re_dev *rdev,
				    struct bnxt_qplib_cc_param *cc_param)
{
	int rc;

	mutex_lock(&rdev->cc_lock);
	rc = bnxt_qplib_modify_cc(&rdev->qplib_res, cc_param);
	mutex_unlock(&rdev->cc_lock);

	return rc;
}

static ssize_t cnp_dscp_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.alt_tos_dscp);
}

static ssize_t cnp_dscp_store(struct config_item *item, const char *buf,
				   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_tc_rec *tc_rec;
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	mutex_lock(&rdev->cc_lock);

	tc_rec = &rdev->tc_rec[0];
	if (bnxt_re_get_pri_dscp_settings(rdev, -1, tc_rec))
		goto fail;

	/*
	 * When use_profile_type on qportcfg_output is set (indicates
	 * service_profile will carry either lossy/lossless type),
	 * Validate the DSCP and reject if it is not configured
	 * for CNP Traffic
	 */
	if (is_qport_service_type_supported(rdev) &&
	    (!(tc_rec->cnp_dscp_bv & (1ul << val))))
		goto fail;

	rdev->cc_param.prev_alt_tos_dscp = rdev->cc_param.alt_tos_dscp;
	rdev->cc_param.alt_tos_dscp = val;
	rdev->cc_param.cnp_dscp_user = val;
	rdev->cc_param.cnp_dscp_user |= BNXT_QPLIB_USER_DSCP_VALID;
	rdev->cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_TOS_DSCP;
	mutex_unlock(&rdev->cc_lock);

	return strnlen(buf, count);
fail:
	mutex_unlock(&rdev->cc_lock);
	return -EINVAL;
}

CONFIGFS_ATTR(, cnp_dscp);

static ssize_t cnp_prio_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.alt_vlan_pcp);
}

static ssize_t cnp_prio_store(struct config_item *item, const char *buf,
			      size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (is_qport_service_type_supported(rdev))
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	if (rdev->cc_param.alt_vlan_pcp > 7)
		return -EINVAL;
	rdev->cc_param.prev_alt_vlan_pcp = rdev->cc_param.alt_vlan_pcp;
	rdev->cc_param.alt_vlan_pcp = val;
	rdev->cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ALT_VLAN_PCP;

	return strnlen(buf, count);
}
CONFIGFS_ATTR(, cnp_prio);

static ssize_t cc_mode_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_mode);
}

static ssize_t cc_mode_store(struct config_item *item, const char *buf,
			     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	if (val > 1)
		return -EINVAL;
	cc_param.cc_mode = val;
	cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_CC_MODE;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cc_mode);

static ssize_t dcn_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param_ext2 *cc_ext2;
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	ssize_t cnt = 0;
	int i, rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_DCN_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	cc_ext2 = &cc_param.cc_ext2;
	cnt += sprintf(buf + cnt, "index   ql_thr(kB)   cr     tr cnp_inc upd_imm\n");
	for (i = 7; i >= 0; i--)
		cnt += sprintf(buf + cnt, "%5d %8u   %6lu %6lu %7d %7d\n",
			       i, cc_ext2->dcn_qlevel_tbl_thr[i],
			       DCN_GET_CR(cc_ext2->dcn_qlevel_tbl_act[i]),
			       DCN_GET_TR(cc_ext2->dcn_qlevel_tbl_act[i]),
			       DCN_GET_INC_CNP(cc_ext2->dcn_qlevel_tbl_act[i]),
			       DCN_GET_UPD_IMM(cc_ext2->dcn_qlevel_tbl_act[i]));

	return cnt;
}

static ssize_t dcn_store(struct config_item *item, const char *buf,
			 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	int idx, ql_thr, cr, tr, cnp_inc, upd_imm;
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_DCN_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	if (sscanf(buf, "%d %d %d %d %d %d\n",
		   &idx, &ql_thr, &cr, &tr, &cnp_inc, &upd_imm) != 6)
		return -EINVAL;

	/* Input values range check */
	if (idx < 0 || idx > 7)
		return -EINVAL;
	if (ql_thr < 0 || ql_thr > 0xFFFF)
		return -EINVAL;
	if (cr < 0 || cr > (MODIFY_DCN_QT_ACT_CR_MASK >> MODIFY_DCN_QT_ACT_CR_SFT))
		return -EINVAL;
	if (tr < 0 || tr > (MODIFY_DCN_QT_ACT_TR_MASK >> MODIFY_DCN_QT_ACT_TR_SFT))
		return -EINVAL;
	if (cnp_inc < 0 || cnp_inc > 1)
		return -EINVAL;
	if (upd_imm < 0 || upd_imm > 1)
		return -EINVAL;

	cc_param.cc_ext2.idx = idx;
	cc_param.cc_ext2.ext2_mask = 0;
	cc_param.cc_ext2.thr = ql_thr;
	if (cc_param.cc_ext2.thr != cc_param.cc_ext2.dcn_qlevel_tbl_thr[idx])
		cc_param.cc_ext2.ext2_mask |= MODIFY_MASK_DCN_QLEVEL_TBL_THR;
	cc_param.cc_ext2.cr = cr;
	if (cc_param.cc_ext2.cr != DCN_GET_CR(cc_param.cc_ext2.dcn_qlevel_tbl_act[idx]))
		cc_param.cc_ext2.ext2_mask |= MODIFY_MASK_DCN_QLEVEL_TBL_CR;
	cc_param.cc_ext2.tr = tr;
	if (cc_param.cc_ext2.tr != DCN_GET_TR(cc_param.cc_ext2.dcn_qlevel_tbl_act[idx]))
		cc_param.cc_ext2.ext2_mask |= MODIFY_MASK_DCN_QLEVEL_TBL_TR;
	cc_param.cc_ext2.cnp_inc = cnp_inc;
	if (cc_param.cc_ext2.cnp_inc != DCN_GET_INC_CNP(cc_param.cc_ext2.dcn_qlevel_tbl_act[idx]))
		cc_param.cc_ext2.ext2_mask |= MODIFY_MASK_DCN_QLEVEL_TBL_INC_CNP;
	cc_param.cc_ext2.upd_imm = upd_imm;
	if (cc_param.cc_ext2.upd_imm != DCN_GET_UPD_IMM(cc_param.cc_ext2.dcn_qlevel_tbl_act[idx]))
		cc_param.cc_ext2.ext2_mask |= MODIFY_MASK_DCN_QLEVEL_TBL_UPD_IMM;
	if (cc_param.cc_ext2.ext2_mask)
		cc_param.cc_ext2.ext2_mask |= MODIFY_MASK_DCN_QLEVEL_TBL_IDX;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
		return rc;
	}

	return strnlen(buf, count);
}
CONFIGFS_ATTR(, dcn);

static ssize_t ecn_enable_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.enable);
}

static ssize_t ecn_enable_store(struct config_item *item, const char *buf,
			    size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.enable = val;
	cc_param.admin_enable = cc_param.enable;
	cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_ENABLE_CC;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, ecn_enable);

static ssize_t g_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.g);
}

static ssize_t g_store(struct config_item *item, const char *buf,
		       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.g = val;
	cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_G;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, g);

static ssize_t init_cr_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.init_cr);
}

static ssize_t init_cr_store(struct config_item *item, const char *buf,
			     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.init_cr = val;
	cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_INIT_CR;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, init_cr);

static ssize_t inact_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	u32 inactivity_th;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	inactivity_th = cc_param.inact_th | (cc_param.cc_ext.inact_th_hi << 16);

	return sprintf(buf, "%#x\n", inactivity_th);
}

static ssize_t inact_th_store(struct config_item *item, const char *buf,
			      size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.inact_th = val;
	cc_param.cc_ext.inact_th_hi =  (val >> 16) & 0x3F;
	cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_INACTIVITY_CP;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, inact_th);

static ssize_t init_tr_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.init_tr);
}

static ssize_t init_tr_store(struct config_item *item, const char *buf,
			     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.init_tr = val;
	cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_INIT_TR;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, init_tr);

static ssize_t nph_per_state_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.nph_per_state);
}

static ssize_t nph_per_state_store(struct config_item *item, const char *buf,
			   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.nph_per_state = val;
	cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_NUMPHASEPERSTATE;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, nph_per_state);

static ssize_t time_pph_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.time_pph);
}

static ssize_t time_pph_store(struct config_item *item, const char *buf,
			      size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.time_pph = val;
	cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TIME_PER_PHASE;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, time_pph);

static ssize_t pkts_pph_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.pkts_pph);
}

static ssize_t pkts_pph_store(struct config_item *item, const char *buf,
			      size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.pkts_pph = val;
	cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_PKTS_PER_PHASE;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, pkts_pph);

static ssize_t rtt_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.rtt);
}

static ssize_t rtt_store(struct config_item *item, const char *buf,
			 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.rtt = val;
	cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_RTT;
	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, rtt);

static ssize_t tcp_cp_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.tcp_cp);
}

static ssize_t tcp_cp_store(struct config_item *item, const char *buf,
			    size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.tcp_cp = val;
	cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TCP_CP;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, tcp_cp);

static ssize_t roce_dscp_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.tos_dscp);
}

static ssize_t roce_dscp_store(struct config_item *item, const char *buf,
			      size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_tc_rec *tc_rec;
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	mutex_lock(&rdev->cc_lock);

	tc_rec = &rdev->tc_rec[0];
	if (bnxt_re_get_pri_dscp_settings(rdev, -1, tc_rec))
		goto fail;

	/*
	 * When use_profile_type on qportcfg_output is set (indicates
	 * service_profile will carry either lossy/lossless type),
	 * Validate the DSCP and reject if it is not configured
	 * for RoCE Traffic
	 */
	if (is_qport_service_type_supported(rdev) &&
	    (!(tc_rec->roce_dscp_bv & (1ul << val))))
		goto fail;

	rdev->cc_param.prev_tos_dscp = rdev->cc_param.tos_dscp;
	rdev->cc_param.tos_dscp = val;
	rdev->cc_param.roce_dscp_user = val;
	rdev->cc_param.roce_dscp_user |= BNXT_QPLIB_USER_DSCP_VALID;
	rdev->cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_DSCP;
	mutex_unlock(&rdev->cc_lock);

	return strnlen(buf, count);
fail:
	mutex_unlock(&rdev->cc_lock);
	return -EINVAL;
}
CONFIGFS_ATTR(, roce_dscp);

static ssize_t roce_prio_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.roce_pri);
}

static ssize_t roce_prio_store(struct config_item *item, const char *buf,
			       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (is_qport_service_type_supported(rdev))
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	if (rdev->cc_param.roce_pri > 7)
		return -EINVAL;
	rdev->cc_param.prev_roce_pri = rdev->cc_param.roce_pri;
	rdev->cc_param.roce_pri = val;
	rdev->cc_param.mask |= BNXT_QPLIB_CC_PARAM_MASK_ROCE_PRI;

	return strnlen(buf, count);
}
CONFIGFS_ATTR(, roce_prio);

static ssize_t ecn_marking_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.tos_ecn);
}

static ssize_t ecn_marking_store(struct config_item *item, const char *buf,
			     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.tos_ecn = val;
	cc_param.mask |= CMDQ_MODIFY_ROCE_CC_MODIFY_MASK_TOS_ECN;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, ecn_marking);

static ssize_t disable_prio_vlan_tx_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf,"%#x\n", rdev->cc_param.disable_prio_vlan_tx);
}

static ssize_t disable_prio_vlan_tx_store(struct config_item *item, const char *buf,
				     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	rdev->cc_param.disable_prio_vlan_tx = val & 0x1;

	rc = bnxt_re_prio_vlan_tx_update(rdev);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to configure VLAN tx\n");
		return rc;
	}

	return strnlen(buf, count);
}
CONFIGFS_ATTR(, disable_prio_vlan_tx);

static ssize_t min_time_bet_cnp_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.min_delta_cnp);
}

static ssize_t min_time_bet_cnp_store(struct config_item *item,
				      const char *buf, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.min_delta_cnp = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_MIN_TIME_BETWEEN_CNPS;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, min_time_bet_cnp);

static ssize_t init_cp_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.init_cp);
}

static ssize_t init_cp_store(struct config_item *item,
			     const char *buf, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.init_cp = val;
	cc_param.cc_ext.ext_mask |= CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_INIT_CP;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, init_cp);

static ssize_t tr_update_mode_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.tr_update_mode);
}

static ssize_t tr_update_mode_store(struct config_item *item,
				    const char *buf, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.tr_update_mode = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_TR_UPDATE_MODE;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, tr_update_mode);

static ssize_t tr_update_cyls_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.tr_update_cyls);
}

static ssize_t tr_update_cyls_store(struct config_item *item,
				    const char *buf, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.tr_update_cyls = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_TR_UPDATE_CYCLES;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, tr_update_cyls);

static ssize_t fr_num_rtts_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.fr_rtt);
}

static ssize_t fr_num_rtts_store(struct config_item *item, const char *buf,
				 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.fr_rtt = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_FR_NUM_RTTS;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, fr_num_rtts);

static ssize_t ai_rate_incr_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.ai_rate_incr);
}

static ssize_t ai_rate_incr_store(struct config_item *item, const char *buf,
				  size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.ai_rate_incr = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_AI_RATE_INCREASE;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, ai_rate_incr);

static ssize_t red_rel_rtts_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.rr_rtt_th);
}

static ssize_t red_rel_rtts_th_store(struct config_item *item,
				     const char *buf, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.rr_rtt_th = val;
	cc_param.cc_ext.ext_mask |=
	CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_REDUCTION_RELAX_RTTS_TH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, red_rel_rtts_th);

static ssize_t act_rel_cr_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.ar_cr_th);
}

static ssize_t act_rel_cr_th_store(struct config_item *item, const char *buf,
				   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.ar_cr_th = val;
	cc_param.cc_ext.ext_mask |=
	CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_ADDITIONAL_RELAX_CR_TH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, act_rel_cr_th);

static ssize_t cr_min_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cr_min_th);
}

static ssize_t cr_min_th_store(struct config_item *item, const char *buf,
			       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cr_min_th = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CR_MIN_TH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cr_min_th);

static ssize_t bw_avg_weight_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.bw_avg_weight);
}

static ssize_t bw_avg_weight_store(struct config_item *item, const char *buf,
				   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.bw_avg_weight = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_BW_AVG_WEIGHT;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, bw_avg_weight);

static ssize_t act_cr_factor_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cr_factor);
}

static ssize_t act_cr_factor_store(struct config_item *item, const char *buf,
				   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cr_factor = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_ACTUAL_CR_FACTOR;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, act_cr_factor);

static ssize_t max_cp_cr_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cr_th_max_cp);
}

static ssize_t max_cp_cr_th_store(struct config_item *item, const char *buf,
				  size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cr_th_max_cp = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_MAX_CP_CR_TH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, max_cp_cr_th);

static ssize_t cp_bias_en_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cp_bias_en);
}

static ssize_t cp_bias_en_store(struct config_item *item, const char *buf,
				size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cp_bias_en = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CP_BIAS_EN;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cp_bias_en);

static ssize_t cp_bias_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cp_bias);
}

static ssize_t cp_bias_store(struct config_item *item, const char *buf,
			     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cp_bias = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CP_BIAS;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cp_bias);

static ssize_t cnp_ecn_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cnp_ecn);
}

static ssize_t cnp_ecn_store(struct config_item *item, const char *buf,
			     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cnp_ecn = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CNP_ECN;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cnp_ecn);

static ssize_t rtt_jitter_en_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.rtt_jitter_en);
}

static ssize_t rtt_jitter_en_store(struct config_item *item, const char *buf,
				   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.rtt_jitter_en = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_RTT_JITTER_EN;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, rtt_jitter_en);

static ssize_t lbytes_per_usec_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.bytes_per_usec);
}

static ssize_t lbytes_per_usec_store(struct config_item *item, const char *buf,
				     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.bytes_per_usec = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_LINK_BYTES_PER_USEC;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}

CONFIGFS_ATTR(, lbytes_per_usec);

static ssize_t reset_cc_cr_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cc_cr_reset_th);
}

static ssize_t reset_cc_cr_th_store(struct config_item *item, const char *buf,
				    size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cc_cr_reset_th = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_RESET_CC_CR_TH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, reset_cc_cr_th);

static ssize_t cr_width_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cr_width);
}

static ssize_t cr_width_store(struct config_item *item, const char *buf,
			      size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cr_width = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CR_WIDTH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cr_width);

static ssize_t min_quota_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.min_quota);
}

static ssize_t min_quota_store(struct config_item *item, const char *buf,
			       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.min_quota = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_QUOTA_PERIOD_MIN;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, min_quota);

static ssize_t max_quota_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.max_quota);
}

static ssize_t max_quota_store(struct config_item *item, const char *buf,
			       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.max_quota = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_QUOTA_PERIOD_MAX;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, max_quota);

static ssize_t abs_max_quota_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.abs_max_quota);
}

static ssize_t abs_max_quota_store(struct config_item *item, const char *buf,
				   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.abs_max_quota = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_QUOTA_PERIOD_ABS_MAX;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, abs_max_quota);

static ssize_t tr_lb_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.tr_lb);
}

static ssize_t tr_lb_store(struct config_item *item, const char *buf,
			   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.tr_lb = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_TR_LOWER_BOUND;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, tr_lb);

static ssize_t cr_prob_fac_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cr_prob_fac);
}

static ssize_t cr_prob_fac_store(struct config_item *item, const char *buf,
				 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cr_prob_fac = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CR_PROB_FACTOR;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cr_prob_fac);

static ssize_t tr_prob_fac_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.tr_prob_fac);
}

static ssize_t tr_prob_fac_store(struct config_item *item, const char *buf,
				 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.tr_prob_fac = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_TR_PROB_FACTOR;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, tr_prob_fac);

static ssize_t fair_cr_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.fair_cr_th);
}

static ssize_t fair_cr_th_store(struct config_item *item, const char *buf,
				size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.fair_cr_th = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_FAIRNESS_CR_TH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, fair_cr_th);

static ssize_t red_div_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.red_div);
}

static ssize_t red_div_store(struct config_item *item, const char *buf,
			     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.red_div = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_RED_DIV;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, red_div);

static ssize_t cnp_ratio_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cnp_ratio_th);
}

static ssize_t cnp_ratio_th_store(struct config_item *item, const char *buf,
				  size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cnp_ratio_th = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CNP_RATIO_TH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cnp_ratio_th);

static ssize_t exp_ai_rtts_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.ai_ext_rtt);
}

static ssize_t exp_ai_rtts_store(struct config_item *item, const char *buf,
				 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.ai_ext_rtt = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_EXP_AI_RTTS;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, exp_ai_rtts);

static ssize_t exp_crcp_ratio_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.exp_crcp_ratio);
}

static ssize_t exp_crcp_ratio_store(struct config_item *item, const char *buf,
				    size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.exp_crcp_ratio = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_EXP_AI_CR_CP_RATIO;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, exp_crcp_ratio);

static ssize_t rt_en_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.low_rate_en);
}

static ssize_t rt_en_store(struct config_item *item, const char *buf,
			   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.low_rate_en = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_USE_RATE_TABLE;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, rt_en);

static ssize_t cp_exp_update_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cpcr_update_th);
}

static ssize_t cp_exp_update_th_store(struct config_item *item,
				      const char *buf, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cpcr_update_th = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CP_EXP_UPDATE_TH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cp_exp_update_th);

static ssize_t ai_rtt_th1_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.ai_rtt_th1);
}

static ssize_t ai_rtt_th1_store(struct config_item *item, const char *buf,
				size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.ai_rtt_th1 = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_HIGH_EXP_AI_RTTS_TH1;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, ai_rtt_th1);

static ssize_t ai_rtt_th2_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.ai_rtt_th2);
}

static ssize_t ai_rtt_th2_store(struct config_item *item, const char *buf,
				size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.ai_rtt_th2 = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_HIGH_EXP_AI_RTTS_TH2;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, ai_rtt_th2);

static ssize_t cf_rtt_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cf_rtt_th);
}

static ssize_t cf_rtt_th_store(struct config_item *item, const char *buf,
			       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cf_rtt_th = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_ACTUAL_CR_CONG_FREE_RTTS_TH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cf_rtt_th);

static ssize_t reduce_cf_rtt_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.reduce_cf_rtt_th);
}

static ssize_t reduce_cf_rtt_th_store(struct config_item *item, const char *buf,
			       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.reduce_cf_rtt_th = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_REDUCE_INIT_CONG_FREE_RTTS_TH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, reduce_cf_rtt_th);

static ssize_t rnd_no_red_en_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_ext.random_no_red_en);
}

static ssize_t rnd_no_red_en_store(struct config_item *item, const char *buf,
				   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.random_no_red_en = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_RANDOM_NO_RED_EN;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, rnd_no_red_en);

static ssize_t act_cr_sh_cor_en_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_ext.actual_cr_shift_correction_en);
}

static ssize_t act_cr_sh_cor_en_store(struct config_item *item, const char *buf,
				      size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_ext.actual_cr_shift_correction_en = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_ACTUAL_CR_SHIFT_CORRECTION_EN;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, act_cr_sh_cor_en);

static ssize_t sc_cr_th1_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.sc_cr_th1);
}

static ssize_t sc_cr_th1_store(struct config_item *item, const char *buf,
			       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.sc_cr_th1 = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_SEVERE_CONG_CR_TH1;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, sc_cr_th1);

static ssize_t sc_cr_th2_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.sc_cr_th2);
}

static ssize_t sc_cr_th2_store(struct config_item *item, const char *buf,
			       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.sc_cr_th2 = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_SEVERE_CONG_CR_TH2;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, sc_cr_th2);

static ssize_t l64B_per_rtt_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.l64B_per_rtt);
}

static ssize_t l64B_per_rtt_store(struct config_item *item, const char *buf,
				  size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.l64B_per_rtt = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_LINK64B_PER_RTT;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, l64B_per_rtt);

static ssize_t cc_ack_bytes_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_ext.cc_ack_bytes);
}

static ssize_t cc_ack_bytes_store(struct config_item *item, const char *buf,
				  size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;
	cc_param.cc_ext.cc_ack_bytes = val;
	cc_param.cc_ext.ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_TLV_MODIFY_MASK_CC_ACK_BYTES;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cc_ack_bytes);

static ssize_t cc_rate_quota_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	ssize_t cnt = 0;
	int rc, i;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	cnt += sprintf(buf + cnt, "index   rate_table_quota_period   rate_table_byte_quota\n");
	for (i = 0; i < 24; i++)
		cnt += sprintf(buf + cnt, "%3d   %13u    %23u\n",
			       i, cc_param.cc_gen1_ext.rate_table_quota_period[i],
			       cc_param.cc_gen1_ext.rate_table_byte_quota[i]);
	return cnt;
}

static ssize_t cc_rate_quota_store(struct config_item *item, const char *buf,
				   size_t count)
{
	unsigned int rate_table_quota_period, rate_table_byte_quota, idx;
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	if (sscanf(buf, "%x %x %x\n", &idx, &rate_table_quota_period, &rate_table_byte_quota) != 3)
		return -EINVAL;

	cc_param.cc_gen1_ext.rate_table_idx = idx;
	cc_param.cc_gen1_ext.rate_table_quota_period[idx] = rate_table_quota_period;
	cc_param.cc_gen1_ext.rate_table_byte_quota[idx] = rate_table_byte_quota;
	cc_param.cc_gen1_ext.gen1_ext_mask |=
		(CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_RATE_TABLE_BYTE_QUOTA |
		 CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_RATE_TABLE_IDX |
		 CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_RATE_TABLE_QUOTA_PERIOD);

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cc_rate_quota);

static ssize_t reduce_init_cong_fr_rtts_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen1_ext.reduce2_init_cong_free_rtts_th);
}

static ssize_t reduce_init_cong_fr_rtts_th_store(struct config_item *item, const char *buf,
						 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen1_ext.reduce2_init_cong_free_rtts_th = val;
	cc_param.cc_gen1_ext.gen1_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_REDUCE2_INIT_CONG_FREE_RTTS_TH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, reduce_init_cong_fr_rtts_th);

static ssize_t rnd_no_red_mult_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen1_ext.rnd_no_red_mult);
}

static ssize_t rnd_no_red_mult_store(struct config_item *item, const char *buf,
				     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen1_ext.rnd_no_red_mult = val;
	cc_param.cc_gen1_ext.gen1_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_RND_NO_RED_MULT;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, rnd_no_red_mult);

static ssize_t no_red_off_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen1_ext.no_red_offset);
}

static ssize_t no_red_off_store(struct config_item *item, const char *buf,
				size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen1_ext.no_red_offset = val;
	cc_param.cc_gen1_ext.gen1_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_NO_RED_OFFSET;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, no_red_off);

static ssize_t reduce_init_en_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen1_ext.reduce2_init_en);
}

static ssize_t reduce_init_en_store(struct config_item *item, const char *buf,
				    size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen1_ext.reduce2_init_en = val;
	cc_param.cc_gen1_ext.gen1_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_REDUCE2_INIT_EN;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, reduce_init_en);

static ssize_t per_adj_cnt_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen1_ext.period_adjust_count);
}

static ssize_t per_adj_cnt_store(struct config_item *item, const char *buf,
				 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen1_ext.period_adjust_count = val;
	cc_param.cc_gen1_ext.gen1_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_PERIOD_ADJUST_COUNT;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, per_adj_cnt);

static ssize_t cr_th2_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}
	return sprintf(buf, "%#x\n", cc_param.cc_gen1_ext.current_rate_threshold_2);
}

static ssize_t cr_th2_store(struct config_item *item, const char *buf,
			    size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen1_ext.current_rate_threshold_2 = val;
	cc_param.cc_gen1_ext.gen1_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_CURRENT_RATE_THRESHOLD_2;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cr_th2);

static ssize_t cr_th1_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen1_ext.current_rate_threshold_1);
}

static ssize_t cr_th1_store(struct config_item *item, const char *buf,
			    size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen1_ext.current_rate_threshold_1 = val;
	cc_param.cc_gen1_ext.gen1_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN1_EXT_TLV_MODIFY_MASK_CURRENT_RATE_THRESHOLD_1;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cr_th1);

static ssize_t cr2bw_64b_ratio_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen2_ext.cr2bw_64b_ratio);
}

static ssize_t cr2bw_64b_ratio_store(struct config_item *item, const char *buf,
				     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen2_ext.cr2bw_64b_ratio = val;
	cc_param.cc_gen2_ext.gen2_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN2_EXT_TLV_MODIFY_MASK_CR2BW_64B_RATIO;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, cr2bw_64b_ratio);

static ssize_t act_cr_cn_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen2_ext.sr2_cc_first_cnp_en);
}

static ssize_t act_cr_cn_store(struct config_item *item, const char *buf,
			       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen2_ext.sr2_cc_first_cnp_en = val;
	cc_param.cc_gen2_ext.gen2_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN2_EXT_TLV_MODIFY_MASK_SR2_CC_FIRST_CNP_EN;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, act_cr_cn);

static ssize_t rnd_no_red_on_tx_en_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen2_ext.sr2_cc_actual_cr_en);
}

static ssize_t rnd_no_red_on_tx_en_store(struct config_item *item, const char *buf,
					 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen2_ext.sr2_cc_actual_cr_en = val;
	cc_param.cc_gen2_ext.gen2_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN2_EXT_TLV_MODIFY_MASK_SR2_CC_ACTUAL_CR_EN;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, rnd_no_red_on_tx_en);

static ssize_t retx_cc_reset_en_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen2_ext.hw_retx_cc_reset_en);
}

static ssize_t retx_cc_reset_en_store(struct config_item *item, const char *buf,
				      size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen2_ext.hw_retx_cc_reset_en = val;
	cc_param.cc_gen2_ext.gen2_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN2_EXT_TLV_MODIFY_MASK_HW_RETX_CC_RESET_EN;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, retx_cc_reset_en);

static ssize_t retx_reset_cc_cr_th_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen2_ext.hw_retx_reset_cc_cr_th);
}

static ssize_t retx_reset_cc_cr_th_store(struct config_item *item, const char *buf,
					 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen2_ext.hw_retx_reset_cc_cr_th = val;
	cc_param.cc_gen2_ext.gen2_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN2_EXT_TLV_MODIFY_MASK_HW_RETX_RESET_CC_CR_TH;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, retx_reset_cc_cr_th);

static ssize_t retx_cr_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen2_ext.retx_cr);
}

static ssize_t retx_cr_store(struct config_item *item, const char *buf,
			     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen2_ext.retx_cr = val;
	cc_param.cc_gen2_ext.gen2_ext_mask |=
		CMDQ_MODIFY_ROCE_CC_GEN2_EXT_TLV_MODIFY_MASK_RETX_CR;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, retx_cr);

static ssize_t retx_tr_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen2_ext.retx_tr);
}

static ssize_t retx_tr_store(struct config_item *item, const char *buf,
			     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen2_ext.retx_tr = val;
	cc_param.cc_gen2_ext.gen2_ext_mask |=
		 CMDQ_MODIFY_ROCE_CC_GEN2_EXT_TLV_MODIFY_MASK_RETX_TR;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, retx_tr);

static ssize_t retx_cp_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = bnxt_qplib_query_cc_param(&rdev->qplib_res, &cc_param);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to query cc settings\n");
		return rc;
	}

	return sprintf(buf, "%#x\n", cc_param.cc_gen2_ext.retx_cp);
}

static ssize_t retx_cp_store(struct config_item *item, const char *buf,
			     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_cc_param cc_param = {};
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
		return -EOPNOTSUPP;

	rc = kstrtouint(buf, 0, &val);
	if (rc)
		return rc;

	cc_param.cc_gen2_ext.retx_cp = val;
	cc_param.cc_gen2_ext.gen2_ext_mask |=
		 CMDQ_MODIFY_ROCE_CC_GEN2_EXT_TLV_MODIFY_MASK_RETX_CP;

	rc = bnxt_modify_one_cc_param(rdev, &cc_param);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to apply cc settings\n");
	return rc ? -EINVAL : strnlen(buf, count);
}
CONFIGFS_ATTR(, retx_cp);

static struct configfs_attribute *bnxt_re_cc_attrs[] = {
	CONFIGFS_ATTR_ADD(attr_advanced),
	CONFIGFS_ATTR_ADD(attr_apply),
	CONFIGFS_ATTR_ADD(attr_cnp_dscp),
	CONFIGFS_ATTR_ADD(attr_cnp_prio),
	CONFIGFS_ATTR_ADD(attr_cc_mode),
	CONFIGFS_ATTR_ADD(attr_ecn_enable),
	CONFIGFS_ATTR_ADD(attr_g),
	CONFIGFS_ATTR_ADD(attr_init_cr),
	CONFIGFS_ATTR_ADD(attr_inact_th),
	CONFIGFS_ATTR_ADD(attr_init_tr),
	CONFIGFS_ATTR_ADD(attr_nph_per_state),
	CONFIGFS_ATTR_ADD(attr_time_pph),
	CONFIGFS_ATTR_ADD(attr_pkts_pph),
	CONFIGFS_ATTR_ADD(attr_rtt),
	CONFIGFS_ATTR_ADD(attr_tcp_cp),
	CONFIGFS_ATTR_ADD(attr_roce_dscp),
	CONFIGFS_ATTR_ADD(attr_roce_prio),
	CONFIGFS_ATTR_ADD(attr_ecn_marking),
	CONFIGFS_ATTR_ADD(attr_disable_prio_vlan_tx),
	NULL,
};

static struct configfs_attribute *bnxt_re_cc_attrs_ext[] = {
	CONFIGFS_ATTR_ADD(attr_advanced),
	CONFIGFS_ATTR_ADD(attr_apply),
	CONFIGFS_ATTR_ADD(attr_cnp_dscp),
	CONFIGFS_ATTR_ADD(attr_cnp_prio),
	CONFIGFS_ATTR_ADD(attr_cc_mode),
	CONFIGFS_ATTR_ADD(attr_ecn_enable),
	CONFIGFS_ATTR_ADD(attr_g),
	CONFIGFS_ATTR_ADD(attr_init_cr),
	CONFIGFS_ATTR_ADD(attr_inact_th),
	CONFIGFS_ATTR_ADD(attr_init_tr),
	CONFIGFS_ATTR_ADD(attr_rtt),
	CONFIGFS_ATTR_ADD(attr_roce_dscp),
	CONFIGFS_ATTR_ADD(attr_roce_prio),
	CONFIGFS_ATTR_ADD(attr_ecn_marking),
	CONFIGFS_ATTR_ADD(attr_disable_prio_vlan_tx),
	CONFIGFS_ATTR_ADD(attr_min_time_bet_cnp),
	CONFIGFS_ATTR_ADD(attr_init_cp),
	CONFIGFS_ATTR_ADD(attr_tr_update_mode),
	CONFIGFS_ATTR_ADD(attr_tr_update_cyls),
	CONFIGFS_ATTR_ADD(attr_fr_num_rtts),
	CONFIGFS_ATTR_ADD(attr_ai_rate_incr),
	CONFIGFS_ATTR_ADD(attr_red_rel_rtts_th),
	CONFIGFS_ATTR_ADD(attr_act_rel_cr_th),
	CONFIGFS_ATTR_ADD(attr_cr_min_th),
	CONFIGFS_ATTR_ADD(attr_bw_avg_weight),
	CONFIGFS_ATTR_ADD(attr_act_cr_factor),
	CONFIGFS_ATTR_ADD(attr_max_cp_cr_th),
	CONFIGFS_ATTR_ADD(attr_cp_bias_en),
	CONFIGFS_ATTR_ADD(attr_cp_bias),
	CONFIGFS_ATTR_ADD(attr_cnp_ecn),
	CONFIGFS_ATTR_ADD(attr_rtt_jitter_en),
	CONFIGFS_ATTR_ADD(attr_lbytes_per_usec),
	CONFIGFS_ATTR_ADD(attr_reset_cc_cr_th),
	CONFIGFS_ATTR_ADD(attr_cr_width),
	CONFIGFS_ATTR_ADD(attr_min_quota),
	CONFIGFS_ATTR_ADD(attr_max_quota),
	CONFIGFS_ATTR_ADD(attr_abs_max_quota),
	CONFIGFS_ATTR_ADD(attr_tr_lb),
	CONFIGFS_ATTR_ADD(attr_cr_prob_fac),
	CONFIGFS_ATTR_ADD(attr_tr_prob_fac),
	CONFIGFS_ATTR_ADD(attr_fair_cr_th),
	CONFIGFS_ATTR_ADD(attr_red_div),
	CONFIGFS_ATTR_ADD(attr_cnp_ratio_th),
	CONFIGFS_ATTR_ADD(attr_exp_ai_rtts),
	CONFIGFS_ATTR_ADD(attr_exp_crcp_ratio),
	CONFIGFS_ATTR_ADD(attr_rt_en),
	CONFIGFS_ATTR_ADD(attr_cp_exp_update_th),
	CONFIGFS_ATTR_ADD(attr_ai_rtt_th1),
	CONFIGFS_ATTR_ADD(attr_ai_rtt_th2),
	CONFIGFS_ATTR_ADD(attr_cf_rtt_th),
	CONFIGFS_ATTR_ADD(attr_sc_cr_th1),
	CONFIGFS_ATTR_ADD(attr_sc_cr_th2),
	CONFIGFS_ATTR_ADD(attr_l64B_per_rtt),
	CONFIGFS_ATTR_ADD(attr_cc_ack_bytes),
	CONFIGFS_ATTR_ADD(attr_reduce_cf_rtt_th),
	CONFIGFS_ATTR_ADD(attr_rnd_no_red_en),
	CONFIGFS_ATTR_ADD(attr_act_cr_sh_cor_en),
	NULL,
};

static struct configfs_attribute *bnxt_re_cc_attrs_ext2[] = {
	CONFIGFS_ATTR_ADD(attr_advanced),
	CONFIGFS_ATTR_ADD(attr_apply),
	CONFIGFS_ATTR_ADD(attr_cnp_dscp),
	CONFIGFS_ATTR_ADD(attr_cnp_prio),
	CONFIGFS_ATTR_ADD(attr_cc_mode),
	CONFIGFS_ATTR_ADD(attr_dcn),
	CONFIGFS_ATTR_ADD(attr_ecn_enable),
	CONFIGFS_ATTR_ADD(attr_g),
	CONFIGFS_ATTR_ADD(attr_init_cr),
	CONFIGFS_ATTR_ADD(attr_inact_th),
	CONFIGFS_ATTR_ADD(attr_init_tr),
	CONFIGFS_ATTR_ADD(attr_rtt),
	CONFIGFS_ATTR_ADD(attr_roce_dscp),
	CONFIGFS_ATTR_ADD(attr_roce_prio),
	CONFIGFS_ATTR_ADD(attr_ecn_marking),
	CONFIGFS_ATTR_ADD(attr_disable_prio_vlan_tx),
	CONFIGFS_ATTR_ADD(attr_min_time_bet_cnp),
	CONFIGFS_ATTR_ADD(attr_init_cp),
	CONFIGFS_ATTR_ADD(attr_tr_update_mode),
	CONFIGFS_ATTR_ADD(attr_tr_update_cyls),
	CONFIGFS_ATTR_ADD(attr_fr_num_rtts),
	CONFIGFS_ATTR_ADD(attr_ai_rate_incr),
	CONFIGFS_ATTR_ADD(attr_red_rel_rtts_th),
	CONFIGFS_ATTR_ADD(attr_act_rel_cr_th),
	CONFIGFS_ATTR_ADD(attr_cr_min_th),
	CONFIGFS_ATTR_ADD(attr_bw_avg_weight),
	CONFIGFS_ATTR_ADD(attr_act_cr_factor),
	CONFIGFS_ATTR_ADD(attr_max_cp_cr_th),
	CONFIGFS_ATTR_ADD(attr_cp_bias_en),
	CONFIGFS_ATTR_ADD(attr_cp_bias),
	CONFIGFS_ATTR_ADD(attr_cnp_ecn),
	CONFIGFS_ATTR_ADD(attr_rtt_jitter_en),
	CONFIGFS_ATTR_ADD(attr_lbytes_per_usec),
	CONFIGFS_ATTR_ADD(attr_reset_cc_cr_th),
	CONFIGFS_ATTR_ADD(attr_cr_width),
	CONFIGFS_ATTR_ADD(attr_min_quota),
	CONFIGFS_ATTR_ADD(attr_max_quota),
	CONFIGFS_ATTR_ADD(attr_abs_max_quota),
	CONFIGFS_ATTR_ADD(attr_tr_lb),
	CONFIGFS_ATTR_ADD(attr_cr_prob_fac),
	CONFIGFS_ATTR_ADD(attr_tr_prob_fac),
	CONFIGFS_ATTR_ADD(attr_fair_cr_th),
	CONFIGFS_ATTR_ADD(attr_red_div),
	CONFIGFS_ATTR_ADD(attr_cnp_ratio_th),
	CONFIGFS_ATTR_ADD(attr_exp_ai_rtts),
	CONFIGFS_ATTR_ADD(attr_exp_crcp_ratio),
	CONFIGFS_ATTR_ADD(attr_rt_en),
	CONFIGFS_ATTR_ADD(attr_cp_exp_update_th),
	CONFIGFS_ATTR_ADD(attr_ai_rtt_th1),
	CONFIGFS_ATTR_ADD(attr_ai_rtt_th2),
	CONFIGFS_ATTR_ADD(attr_cf_rtt_th),
	CONFIGFS_ATTR_ADD(attr_sc_cr_th1),
	CONFIGFS_ATTR_ADD(attr_sc_cr_th2),
	CONFIGFS_ATTR_ADD(attr_l64B_per_rtt),
	CONFIGFS_ATTR_ADD(attr_cc_ack_bytes),
	CONFIGFS_ATTR_ADD(attr_reduce_cf_rtt_th),
	CONFIGFS_ATTR_ADD(attr_rnd_no_red_en),
	CONFIGFS_ATTR_ADD(attr_act_cr_sh_cor_en),
	NULL,
};

static struct configfs_attribute *bnxt_re_cc_attrs_gen1_gen2_ext[] = {
	CONFIGFS_ATTR_ADD(attr_advanced),
	CONFIGFS_ATTR_ADD(attr_apply),
	CONFIGFS_ATTR_ADD(attr_cnp_dscp),
	CONFIGFS_ATTR_ADD(attr_cnp_prio),
	CONFIGFS_ATTR_ADD(attr_cc_mode),
	CONFIGFS_ATTR_ADD(attr_dcn),
	CONFIGFS_ATTR_ADD(attr_ecn_enable),
	CONFIGFS_ATTR_ADD(attr_ext_settings),
	CONFIGFS_ATTR_ADD(attr_g),
	CONFIGFS_ATTR_ADD(attr_init_cr),
	CONFIGFS_ATTR_ADD(attr_inact_th),
	CONFIGFS_ATTR_ADD(attr_init_tr),
	CONFIGFS_ATTR_ADD(attr_rtt),
	CONFIGFS_ATTR_ADD(attr_roce_dscp),
	CONFIGFS_ATTR_ADD(attr_roce_prio),
	CONFIGFS_ATTR_ADD(attr_ecn_marking),
	CONFIGFS_ATTR_ADD(attr_disable_prio_vlan_tx),
	CONFIGFS_ATTR_ADD(attr_min_time_bet_cnp),
	CONFIGFS_ATTR_ADD(attr_init_cp),
	CONFIGFS_ATTR_ADD(attr_tr_update_mode),
	CONFIGFS_ATTR_ADD(attr_tr_update_cyls),
	CONFIGFS_ATTR_ADD(attr_fr_num_rtts),
	CONFIGFS_ATTR_ADD(attr_ai_rate_incr),
	CONFIGFS_ATTR_ADD(attr_red_rel_rtts_th),
	CONFIGFS_ATTR_ADD(attr_act_rel_cr_th),
	CONFIGFS_ATTR_ADD(attr_cr_min_th),
	CONFIGFS_ATTR_ADD(attr_bw_avg_weight),
	CONFIGFS_ATTR_ADD(attr_act_cr_factor),
	CONFIGFS_ATTR_ADD(attr_max_cp_cr_th),
	CONFIGFS_ATTR_ADD(attr_cp_bias_en),
	CONFIGFS_ATTR_ADD(attr_cp_bias),
	CONFIGFS_ATTR_ADD(attr_cnp_ecn),
	CONFIGFS_ATTR_ADD(attr_rtt_jitter_en),
	CONFIGFS_ATTR_ADD(attr_lbytes_per_usec),
	CONFIGFS_ATTR_ADD(attr_reset_cc_cr_th),
	CONFIGFS_ATTR_ADD(attr_cr_width),
	CONFIGFS_ATTR_ADD(attr_min_quota),
	CONFIGFS_ATTR_ADD(attr_max_quota),
	CONFIGFS_ATTR_ADD(attr_abs_max_quota),
	CONFIGFS_ATTR_ADD(attr_tr_lb),
	CONFIGFS_ATTR_ADD(attr_cr_prob_fac),
	CONFIGFS_ATTR_ADD(attr_tr_prob_fac),
	CONFIGFS_ATTR_ADD(attr_fair_cr_th),
	CONFIGFS_ATTR_ADD(attr_red_div),
	CONFIGFS_ATTR_ADD(attr_cnp_ratio_th),
	CONFIGFS_ATTR_ADD(attr_exp_ai_rtts),
	CONFIGFS_ATTR_ADD(attr_exp_crcp_ratio),
	CONFIGFS_ATTR_ADD(attr_rt_en),
	CONFIGFS_ATTR_ADD(attr_cp_exp_update_th),
	CONFIGFS_ATTR_ADD(attr_ai_rtt_th1),
	CONFIGFS_ATTR_ADD(attr_ai_rtt_th2),
	CONFIGFS_ATTR_ADD(attr_cf_rtt_th),
	CONFIGFS_ATTR_ADD(attr_sc_cr_th1),
	CONFIGFS_ATTR_ADD(attr_sc_cr_th2),
	CONFIGFS_ATTR_ADD(attr_l64B_per_rtt),
	CONFIGFS_ATTR_ADD(attr_cc_ack_bytes),
	CONFIGFS_ATTR_ADD(attr_reduce_cf_rtt_th),
	CONFIGFS_ATTR_ADD(attr_rnd_no_red_en),
	CONFIGFS_ATTR_ADD(attr_act_cr_sh_cor_en),
	CONFIGFS_ATTR_ADD(attr_no_red_off),
	CONFIGFS_ATTR_ADD(attr_rnd_no_red_mult),
	CONFIGFS_ATTR_ADD(attr_reduce_init_cong_fr_rtts_th),
	CONFIGFS_ATTR_ADD(attr_reduce_init_en),
	CONFIGFS_ATTR_ADD(attr_per_adj_cnt),
	CONFIGFS_ATTR_ADD(attr_cr_th2),
	CONFIGFS_ATTR_ADD(attr_cr_th1),
	CONFIGFS_ATTR_ADD(attr_cc_rate_quota),
	CONFIGFS_ATTR_ADD(attr_cr2bw_64b_ratio),
	CONFIGFS_ATTR_ADD(attr_act_cr_cn),
	CONFIGFS_ATTR_ADD(attr_rnd_no_red_on_tx_en),
	CONFIGFS_ATTR_ADD(attr_retx_cc_reset_en),
	CONFIGFS_ATTR_ADD(attr_retx_reset_cc_cr_th),
	CONFIGFS_ATTR_ADD(attr_retx_cr),
	CONFIGFS_ATTR_ADD(attr_retx_tr),
	CONFIGFS_ATTR_ADD(attr_retx_cp),
	NULL,
};

static struct bnxt_re_dev *cfgfs_update_auxbus_re(struct bnxt_re_dev *rdev,
						  u32 gsi_mode, u8 wqe_mode)
{
	struct bnxt_re_dev *new_rdev = NULL;
	struct net_device *netdev;
	struct bnxt_en_dev *en_dev;
	struct auxiliary_device *adev;
	int rc = 0;

	/* check again if context is changed by roce driver */
	if (!rdev)
		return NULL;

	mutex_lock(&bnxt_re_mutex);
	en_dev = rdev->en_dev;
	netdev = en_dev->net;
	adev   = rdev->adev;

	/* Remove and add the device.
	 * Before removing unregister with IB.
	 */
	bnxt_re_ib_uninit(rdev);
	bnxt_re_remove_device(rdev, BNXT_RE_COMPLETE_REMOVE, adev);
	rc = bnxt_re_add_device(&new_rdev, netdev, NULL, gsi_mode,
				BNXT_RE_COMPLETE_INIT, wqe_mode,
				adev, true);
	if (rc)
		goto clean_dev;
	_bnxt_re_ib_init(new_rdev);
	_bnxt_re_ib_init2(new_rdev);

	/* update the auxdev container */
	rdev = new_rdev;

	/* Don't crash for usermodes.
	 * Return gracefully they will retry
	 */
	if (rtnl_trylock()) {
		bnxt_re_get_link_speed(rdev);
		rtnl_unlock();
	} else {
		pr_err("Setting link speed failed, retry config again");
		goto clean_dev;
	}
	mutex_unlock(&bnxt_re_mutex);
	return rdev;
clean_dev:
	mutex_unlock(&bnxt_re_mutex);
	if (new_rdev) {
		bnxt_re_ib_uninit(rdev);
		bnxt_re_remove_device(rdev, BNXT_RE_COMPLETE_REMOVE, adev);
	}
	return NULL;
}

#ifdef HAVE_OLD_CONFIGFS_API
static ssize_t bnxt_re_cfg_grp_attr_show(struct config_item *item,
					 struct configfs_attribute *attr,
					 char *page)
{
	struct configfs_attr *grp_attr =
			container_of(attr, struct configfs_attr, attr);
	ssize_t rc = -EINVAL;

	if (!grp_attr)
		goto out;

	if (grp_attr->show)
		rc = grp_attr->show(item, page);
out:
	return rc;
}

static ssize_t bnxt_re_cfg_grp_attr_store(struct config_item *item,
					  struct configfs_attribute *attr,
					  const char *page, size_t count)
{
	struct configfs_attr *grp_attr =
			container_of(attr, struct configfs_attr, attr);
	ssize_t rc = -EINVAL;

	if (!grp_attr)
		goto out;
	if (grp_attr->store)
		rc = grp_attr->store(item, page, count);
out:
	return rc;
}

static struct configfs_item_operations bnxt_re_grp_ops = {
	.show_attribute		= bnxt_re_cfg_grp_attr_show,
	.store_attribute	= bnxt_re_cfg_grp_attr_store,
};

#else
static struct configfs_item_operations bnxt_re_grp_ops = {
};
#endif

static struct config_item_type bnxt_re_ccgrp_type = {
	.ct_attrs = bnxt_re_cc_attrs,
	.ct_item_ops = &bnxt_re_grp_ops,
	.ct_owner = THIS_MODULE,
};

static struct config_item_type bnxt_re_ccgrp_type_ext = {
	.ct_attrs = bnxt_re_cc_attrs_ext,
	.ct_item_ops = &bnxt_re_grp_ops,
	.ct_owner = THIS_MODULE,
};

static struct config_item_type bnxt_re_ccgrp_type_ext2 = {
	.ct_attrs = bnxt_re_cc_attrs_ext2,
	.ct_item_ops = &bnxt_re_grp_ops,
	.ct_owner = THIS_MODULE,
};

static struct config_item_type bnxt_re_ccgrp_type_gen1_gen2_ext = {
	.ct_attrs = bnxt_re_cc_attrs_gen1_gen2_ext,
	.ct_item_ops = &bnxt_re_grp_ops,
	.ct_owner = THIS_MODULE,
};

static int make_bnxt_re_cc(struct bnxt_re_port_group *portgrp,
			   struct bnxt_re_dev *rdev, u32 gidx)
{
	struct config_item_type *grp_type;
	struct bnxt_re_cfg_group *ccgrp;
	int rc;

	/*
	 * TODO: If there is confirmed use case that users need to read cc
	 * params from VF instance, we would enable cc node for VF with
	 * selected params.
	 */
	if (rdev->is_virtfn)
		return 0;

	ccgrp = kzalloc(sizeof(*ccgrp), GFP_KERNEL);
	if (!ccgrp) {
		rc = -ENOMEM;
		goto out;
	}

	ccgrp->rdev = rdev;
	grp_type = &bnxt_re_ccgrp_type;
	if (_is_chip_p5_plus(rdev->chip_ctx)) {
		if (BNXT_RE_CC_GEN1_EXT_ENABLED(rdev->rcfw.res) ||
		    BNXT_RE_CC_GEN2_EXT_ENABLED(rdev->rcfw.res))
			grp_type = &bnxt_re_ccgrp_type_gen1_gen2_ext;
		else if (BNXT_RE_DCN_ENABLED(rdev->rcfw.res))
			grp_type = &bnxt_re_ccgrp_type_ext2;
		else
			grp_type = &bnxt_re_ccgrp_type_ext;
	}

	config_group_init_type_name(&ccgrp->group, "cc", grp_type);
#ifndef HAVE_CFGFS_ADD_DEF_GRP
	portgrp->nportgrp.default_groups = portgrp->default_grp;
	portgrp->default_grp[gidx] = &ccgrp->group;
	portgrp->default_grp[gidx + 1] = NULL;
#else
	configfs_add_default_group(&ccgrp->group, &portgrp->nportgrp);
#endif
	portgrp->ccgrp = ccgrp;
	ccgrp->portgrp = portgrp;

	return 0;
out:
	kfree(ccgrp);
	return rc;
}

static ssize_t min_tx_depth_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%u\n", rdev->min_tx_depth);
}

static ssize_t min_tx_depth_store(struct config_item *item, const char *buf,
				  size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val;
	int rc;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	rc = sscanf(buf, "%u\n", &val);
	if (val > rdev->dev_attr->max_sq_wqes || rc <= 0) {
		dev_err(rdev_to_dev(rdev),
			"min_tx_depth %u cannot be greater than max_sq_wqes %u",
			val, rdev->dev_attr->max_sq_wqes);
		return -EINVAL;
	}

	rdev->min_tx_depth = val;

	return strnlen(buf, count);
}

CONFIGFS_ATTR(, min_tx_depth);

static ssize_t stats_query_sec_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->stats.stats_query_sec);
}

static ssize_t stats_query_sec_store(struct config_item *item, const char *buf,
				     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (sscanf(buf, "%x\n", &val)  != 1)
		return -EINVAL;
	/* Valid values are 0 - 8 now. default value is 1
	 * 0 means disable periodic query.
	 * 1 means bnxt_re_worker queries every sec, 2 - every 2 sec and so on
	 */

	if (val > 8)
		return -EINVAL;

	rdev->stats.stats_query_sec = val;

	return strnlen(buf, count);
}

CONFIGFS_ATTR(, stats_query_sec);

static ssize_t gsi_qp_mode_store(struct config_item *item,
				 const char *buf, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev, *new_rdev = NULL;
	struct mutex *mutexp; /* Acquire subsys mutex */
	u32 gsi_mode;
	u8 wqe_mode;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	/* Hold the subsystem lock to serialize */
	mutexp = &item->ci_group->cg_subsys->su_mutex;
	mutex_lock(mutexp);
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		goto ret_err;
	if (_is_chip_p5_plus(rdev->chip_ctx))
		goto ret_err;
	sscanf(buf, "%x\n", (unsigned int *)&gsi_mode);
	if (!gsi_mode || gsi_mode > BNXT_RE_GSI_MODE_ROCE_V2_IPV6)
		goto ret_err;
	if (gsi_mode == rdev->gsi_ctx.gsi_qp_mode)
		goto done;
	wqe_mode = rdev->chip_ctx->modes.wqe_mode;

	if (rdev->binfo) {
		struct bnxt_re_bond_info binfo;
		struct netdev_bonding_info *nbinfo;

		memcpy(&binfo, rdev->binfo, sizeof(*(rdev->binfo)));
		nbinfo = &binfo.nbinfo;
		bnxt_re_destroy_lag(&rdev);
		bnxt_re_create_base_interface(&binfo, true);
		bnxt_re_create_base_interface(&binfo, false);

		/* TODO: Wait for sched count to become 0 on both rdevs */
		msleep(10000);

		/* Recreate lag. */
		rc = bnxt_re_create_lag(&nbinfo->master, &nbinfo->slave,
					nbinfo, binfo.slave2, &new_rdev,
					gsi_mode, wqe_mode);
		if (rc)
			dev_warn(rdev_to_dev(rdev), "%s: failed to create lag %d\n",
				 __func__, rc);
	} else {
		/* driver functions takes care of locking */
		new_rdev = cfgfs_update_auxbus_re(rdev, gsi_mode, wqe_mode);
		if (!new_rdev)
			goto ret_err;
	}

	if (new_rdev)
		ccgrp->rdev = new_rdev;
done:
	mutex_unlock(mutexp);
	return strnlen(buf, count);
ret_err:
	mutex_unlock(mutexp);
	return -EINVAL;
}

static const char *bnxt_re_mode_to_str [] = {
	"GSI Mode Invalid",
	"GSI Mode All",
	"GSI Mode RoCE_v1 Only",
	"GSI Mode RoCE_v2 IPv4 Only",
	"GSI Mode RoCE_v2 IPv6 Only",
	"GSI Mode UD"
};

static inline const char * mode_to_str(u8 gsi_mode)
{
	return bnxt_re_mode_to_str[gsi_mode];
}

static ssize_t gsi_qp_mode_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	int bytes = 0;
	u8 gsi_mode;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	/* Little too much? */
	gsi_mode = rdev->gsi_ctx.gsi_qp_mode;
	bytes += sprintf(buf + bytes, "%s (%#x): %s\n",
			 mode_to_str(BNXT_RE_GSI_MODE_ALL),
			 (int)BNXT_RE_GSI_MODE_ALL,
			 gsi_mode == BNXT_RE_GSI_MODE_ALL ?
			 "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes, "%s (%#x): %s\n",
			 mode_to_str(BNXT_RE_GSI_MODE_ROCE_V1),
			 (int)BNXT_RE_GSI_MODE_ROCE_V1,
			 gsi_mode == BNXT_RE_GSI_MODE_ROCE_V1 ?
			 "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes, "%s (%#x): %s\n",
			 mode_to_str(BNXT_RE_GSI_MODE_ROCE_V2_IPV4),
			 (int)BNXT_RE_GSI_MODE_ROCE_V2_IPV4,
			 gsi_mode == BNXT_RE_GSI_MODE_ROCE_V2_IPV4 ?
			 "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes, "%s (%#x): %s\n",
			 mode_to_str(BNXT_RE_GSI_MODE_ROCE_V2_IPV6),
			 (int)BNXT_RE_GSI_MODE_ROCE_V2_IPV6,
			 gsi_mode == BNXT_RE_GSI_MODE_ROCE_V2_IPV6 ?
			 "Enabled" : "Disabled");
	bytes += sprintf(buf + bytes, "%s (%#x): %s\n",
			 mode_to_str(BNXT_RE_GSI_MODE_UD),
			 (int)BNXT_RE_GSI_MODE_UD,
			 gsi_mode == BNXT_RE_GSI_MODE_UD ?
			 "Enabled" : "Disabled");
	return bytes;
}
CONFIGFS_ATTR(, gsi_qp_mode);

static const char *bnxt_re_wqe_mode_to_str [] = {
	"STATIC", "VARIABLE"
};

static ssize_t wqe_mode_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_drv_modes *drv_mode;
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	drv_mode = &rdev->chip_ctx->modes;
	return sprintf(buf, "sq wqe mode: %s (%#x)\n",
		       bnxt_re_wqe_mode_to_str[drv_mode->wqe_mode],
		       drv_mode->wqe_mode);
}

static ssize_t wqe_mode_store(struct config_item *item, const char *buf,
			      size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev, *new_rdev = NULL;
	struct bnxt_qplib_drv_modes *drv_mode;
	struct mutex *mutexp; /* subsys lock */
	int mode, rc;
	u8 gsi_mode;

	if (!ccgrp)
		return -EINVAL;
	/* Hold the subsys lock */
	mutexp = &item->ci_group->cg_subsys->su_mutex;
	mutex_lock(mutexp);
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		goto ret_err;
	rc = sscanf(buf, "%d\n", &mode);
	if (mode < 0 || mode > BNXT_QPLIB_WQE_MODE_VARIABLE || rc <= 0)
		goto ret_err;
	if (mode == BNXT_QPLIB_WQE_MODE_VARIABLE &&
	    !_is_chip_p5_plus(rdev->chip_ctx))
		goto ret_err;

	drv_mode = &rdev->chip_ctx->modes;
	if (drv_mode->wqe_mode == mode)
		goto done;

	gsi_mode = rdev->gsi_ctx.gsi_qp_mode;
	new_rdev = cfgfs_update_auxbus_re(rdev, gsi_mode, mode);
	if (!new_rdev)
		goto ret_err;
	ccgrp->rdev = new_rdev;
done:
	mutex_unlock(mutexp);
	return strnlen(buf, count);
ret_err:
	mutex_unlock(mutexp);
	return -EINVAL;
}
CONFIGFS_ATTR(, wqe_mode);

static ssize_t acc_tx_path_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_qplib_drv_modes *drv_mode;
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	drv_mode = &rdev->chip_ctx->modes;
	return sprintf(buf, "Accelerated transmit path: %s\n",
		       drv_mode->te_bypass ? "Enabled" : "Disabled");
}

static ssize_t acc_tx_path_store(struct config_item *item, const char *buf,
				 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_en_dev_info *en_info;
	struct bnxt_qplib_drv_modes *drv_mode;
	struct bnxt_re_dev *rdev;
	unsigned int mode;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	sscanf(buf, "%x\n", &mode);
	if (mode >= 2)
		return -EINVAL;
	if (mode) {
		if (!_is_chip_p5_plus(rdev->chip_ctx))
			return -EINVAL;

		if (_is_chip_gen_p5_p7(rdev->chip_ctx) && BNXT_EN_HW_LAG(rdev->en_dev))
			return -EINVAL;
	}

	drv_mode = &rdev->chip_ctx->modes;
	drv_mode->te_bypass = mode;

	/* Update the container */
	en_info = auxiliary_get_drvdata(rdev->adev);
	if (en_info)
		en_info->te_bypass = (mode == 0x1);

	return strnlen(buf, count);
}
CONFIGFS_ATTR(, acc_tx_path);

static ssize_t en_qp_dbg_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->en_qp_dbg);
}

static ssize_t en_qp_dbg_store(struct config_item *item, const char *buf,
			       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val)  != 1)
		return -EINVAL;
	if (val > 1)
		return -EINVAL;

	if (rdev->en_qp_dbg && val == 0)
		bnxt_re_rem_dbg_files(rdev);
	else if (!rdev->en_qp_dbg && val)
		bnxt_re_add_dbg_files(rdev);

	rdev->en_qp_dbg = val;

	return strnlen(buf, count);
}

CONFIGFS_ATTR(, en_qp_dbg);

static ssize_t user_dbr_drop_recov_timeout_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%d\n", rdev->user_dbr_drop_recov_timeout);
}

static ssize_t user_dbr_drop_recov_timeout_store(struct config_item *item, const char *buf,
						 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%d\n", &val)  != 1)
		return -EINVAL;
	if ((val < BNXT_DBR_DROP_MIN_TIMEOUT) || (val > BNXT_DBR_DROP_MAX_TIMEOUT))
		return -EINVAL;

	rdev->user_dbr_drop_recov_timeout = val;

	return strnlen(buf, count);
}

CONFIGFS_ATTR(, user_dbr_drop_recov_timeout);

static ssize_t user_dbr_drop_recov_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->user_dbr_drop_recov);
}

static ssize_t user_dbr_drop_recov_store(struct config_item *item, const char *buf, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;
	int rc = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val)  != 1)
		return -EINVAL;
	if (val > 1)
		return -EINVAL;

	if (!val) {
		/* Disable DBR drop recovery */
		if (!rdev->user_dbr_drop_recov) {
			dev_info(rdev_to_dev(rdev),
				 "User DBR drop recovery already disabled. Returning\n");
			goto exit;
		}
		rdev->user_dbr_drop_recov = false;
	} else {
		if (rdev->user_dbr_drop_recov) {
			dev_info(rdev_to_dev(rdev),
				 "User DBR drop recovery already enabled. Returning\n");
			goto exit;
		}

		if (!rdev->dbr_drop_recov) {
			dev_info(rdev_to_dev(rdev),
				 "Can not enable User DBR drop recovery as FW doesn't support\n");
			rdev->user_dbr_drop_recov = false;
			rc = -EINVAL;
			goto exit;
		}

		rdev->user_dbr_drop_recov = true;
	}
exit:
	return (rc ? -EINVAL : strnlen(buf, count));
}

CONFIGFS_ATTR(, user_dbr_drop_recov);

static ssize_t dbr_pacing_enable_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (!rdev->dbr_pacing_bar)
		dev_info(rdev_to_dev(rdev),
			 "DBR pacing is not supported on this device\n");
	return sprintf(buf, "%#x\n", rdev->dbr_pacing);
}

static ssize_t dbr_pacing_enable_store(struct config_item *item, const char *buf,
				       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	struct bnxt_qplib_nq *nq;
	unsigned int val = 0;
	int rc = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val)  != 1)
		return -EINVAL;
	if (val > 1)
		return -EINVAL;

	if (!rdev->dbr_pacing_bar) {
		dev_info(rdev_to_dev(rdev),
			 "DBR pacing is not supported on this device\n");
		return -EINVAL;
	}

	nq = &rdev->nqr->nq[0];
	if (!val) {
		/* Disable DBR Pacing */
		if (!rdev->dbr_pacing) {
			dev_info(rdev_to_dev(rdev),
				 "DBR pacing already disabled. Returning\n");
			goto exit;
		}
		if (!bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx))
			rc = bnxt_re_disable_dbr_pacing(rdev);
	} else {
		if (rdev->dbr_pacing) {
			dev_info(rdev_to_dev(rdev),
				 "DBR pacing already enabled. Returning\n");
			goto exit;
		}
		if (!bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx))
			rc = bnxt_re_enable_dbr_pacing(rdev);
		else
			bnxt_re_set_dbq_throttling_reg(rdev, nq->ring_id,
						       rdev->dbq_watermark);
	}
	rdev->dbr_pacing = !!val;
exit:
	return (rc ? -EINVAL : strnlen(buf, count));
}

CONFIGFS_ATTR(, dbr_pacing_enable);

static ssize_t dbr_pacing_dbq_watermark_show(struct config_item *item,
					     char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->dbq_watermark);
}

static ssize_t dbr_pacing_dbq_watermark_store(struct config_item *item,
					      const char *buf, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	struct bnxt_qplib_nq *nq;
	unsigned int val = 0;
	int rc = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val)  != 1)
		return -EINVAL;
	if (val > BNXT_RE_PACING_DBQ_HIGH_WATERMARK)
		return -EINVAL;

	rdev->dbq_watermark = val;

	if (bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx)) {
		nq = &rdev->nqr->nq[0];
		bnxt_re_set_dbq_throttling_reg(rdev, nq->ring_id, rdev->dbq_watermark);
	} else {
		if (bnxt_re_enable_dbr_pacing(rdev)) {
			dev_err(rdev_to_dev(rdev),
				"Failed to set dbr pacing config\n");
			rc = -EIO;
		}
	}
	return rc ? rc : strnlen(buf, count);
}

CONFIGFS_ATTR(, dbr_pacing_dbq_watermark);

static ssize_t dbr_pacing_time_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->dbq_pacing_time);
}

static ssize_t dbr_pacing_time_store(struct config_item *item, const char *buf,
				     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val)  != 1)
		return -EINVAL;

	rdev->dbq_pacing_time = val;
	return strnlen(buf, count);
}

CONFIGFS_ATTR(, dbr_pacing_time);

static ssize_t dbr_pacing_primary_fn_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n",
		      bnxt_qplib_dbr_pacing_is_primary_pf(rdev->chip_ctx));
}

static ssize_t dbr_pacing_primary_fn_store(struct config_item *item, const char *buf,
					   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;
	int rc = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val)  != 1)
		return -EINVAL;

	if (bnxt_qplib_dbr_pacing_ext_en(rdev->chip_ctx)) {
		dev_info(rdev_to_dev(rdev),
			 "FW is responsible for picking the primary function\n");
		return -EOPNOTSUPP;
	}
	if (!val) {
		if (bnxt_qplib_dbr_pacing_is_primary_pf(rdev->chip_ctx)) {
			rc = bnxt_re_disable_dbr_pacing(rdev);
			bnxt_qplib_dbr_pacing_set_primary_pf(rdev->chip_ctx, 0);
		}
	} else {
		rc = bnxt_re_enable_dbr_pacing(rdev);
		bnxt_qplib_dbr_pacing_set_primary_pf(rdev->chip_ctx, 1);
	}
	return rc ? rc : strnlen(buf, count);
}

CONFIGFS_ATTR(, dbr_pacing_primary_fn);

static ssize_t dbr_pacing_algo_threshold_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->pacing_algo_th);
}

static ssize_t dbr_pacing_algo_threshold_store(struct config_item *item,
					       const char *buf, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val)  != 1)
		return -EINVAL;
	if (val > rdev->qplib_res.pacing_data->fifo_max_depth)
		return -EINVAL;

	rdev->pacing_algo_th = val;
	bnxt_re_set_def_pacing_threshold(rdev);

	return strnlen(buf, count);
}

CONFIGFS_ATTR(, dbr_pacing_algo_threshold);

static ssize_t dbr_pacing_en_int_threshold_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->pacing_en_int_th);
}

static ssize_t dbr_pacing_en_int_threshold_store(struct config_item *item,
						 const char *buf, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val)  != 1)
		return -EINVAL;
	if (val > rdev->qplib_res.pacing_data->fifo_max_depth)
		return -EINVAL;

	rdev->pacing_en_int_th = val;

	return strnlen(buf, count);
}

CONFIGFS_ATTR(, dbr_pacing_en_int_threshold);

static ssize_t dbr_def_do_pacing_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->dbr_def_do_pacing);
}

static ssize_t dbr_def_do_pacing_store(struct config_item *item, const char *buf,
				       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val) != 1)
		return -EINVAL;
	if (val > BNXT_RE_MAX_DBR_DO_PACING)
		return -EINVAL;
	rdev->dbr_def_do_pacing = val;
	bnxt_re_set_def_do_pacing(rdev);
	return strnlen(buf, count);
}

CONFIGFS_ATTR(, dbr_def_do_pacing);

static ssize_t cq_coal_buf_maxtime_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->cq_coalescing.buf_maxtime);
}

static ssize_t cq_coal_buf_maxtime_store(struct config_item *item, const char *buf,
					 size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val) != 1)
		return -EINVAL;
	if (val < 1 || val > BNXT_QPLIB_CQ_COAL_MAX_BUF_MAXTIME)
		return -EINVAL;
	rdev->cq_coalescing.buf_maxtime = val;
	return strnlen(buf, count);
}

CONFIGFS_ATTR(, cq_coal_buf_maxtime);

static ssize_t cq_coal_normal_maxbuf_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->cq_coalescing.normal_maxbuf);
}

static ssize_t cq_coal_normal_maxbuf_store(struct config_item *item, const char *buf,
					   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val) != 1)
		return -EINVAL;
	if (val < 1 || val > BNXT_QPLIB_CQ_COAL_MAX_NORMAL_MAXBUF)
		return -EINVAL;
	rdev->cq_coalescing.normal_maxbuf = val;
	return strnlen(buf, count);
}

CONFIGFS_ATTR(, cq_coal_normal_maxbuf);

static ssize_t cq_coal_during_maxbuf_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->cq_coalescing.during_maxbuf);
}

static ssize_t cq_coal_during_maxbuf_store(struct config_item *item, const char *buf,
					   size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val) != 1)
		return -EINVAL;
	if (val < 1 || val > BNXT_QPLIB_CQ_COAL_MAX_DURING_MAXBUF)
		return -EINVAL;
	rdev->cq_coalescing.during_maxbuf = val;
	return strnlen(buf, count);
}

CONFIGFS_ATTR(, cq_coal_during_maxbuf);

static ssize_t cq_coal_en_ring_idle_mode_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->cq_coalescing.en_ring_idle_mode);
}

static ssize_t cq_coal_en_ring_idle_mode_store(struct config_item *item, const char *buf,
					       size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val) != 1)
		return -EINVAL;
	if (val > BNXT_QPLIB_CQ_COAL_MAX_EN_RING_IDLE_MODE)
		return -EINVAL;
	rdev->cq_coalescing.en_ring_idle_mode = val;
	return strnlen(buf, count);
}

CONFIGFS_ATTR(, cq_coal_en_ring_idle_mode);

static ssize_t snapdump_dbg_lvl_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%#x\n", rdev->snapdump_dbg_lvl);
}

static ssize_t snapdump_dbg_lvl_store(struct config_item *item,
				      const char *buf, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val = 0;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	if (sscanf(buf, "%x\n", &val) != 1)
		return -EINVAL;
	if (val > BNXT_RE_SNAPDUMP_ALL)
		return -EINVAL;
	rdev->snapdump_dbg_lvl = val;
	return strnlen(buf, count);
}

CONFIGFS_ATTR(, snapdump_dbg_lvl);

static ssize_t enable_queue_overflow_telemetry_show(struct config_item *item, char *buf)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;
	return sprintf(buf, "%u\n", rdev->enable_queue_overflow_telemetry);
}

static ssize_t enable_queue_overflow_telemetry_store(struct config_item *item, const char *buf,
						     size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_dev *rdev;
	unsigned int val;

	if (!ccgrp)
		return -EINVAL;
	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	if (sscanf(buf, "%x\n", &val)  != 1)
		return -EINVAL;

	if (val > 1) {
		dev_err(rdev_to_dev(rdev),
			"Invalid value %u. 1 to disable and 0 to enable", val);
		return -EINVAL;
	}

	rdev->enable_queue_overflow_telemetry = val;

	return strnlen(buf, count);
}

CONFIGFS_ATTR(, enable_queue_overflow_telemetry);

#if defined(CONFIGFS_BIN_ATTR)
static ssize_t
config_read(struct config_item *item, void *data, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct hwrm_udcc_comp_qcfg_input req = {};
	struct hwrm_udcc_comp_qcfg_output resp;
	struct bnxt_fw_msg fw_msg = {};
	struct bnxt_re_udcc_cfg *udcc;
	struct bnxt_en_dev *en_dev;
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	udcc = &rdev->udcc_cfg;
	en_dev = rdev->en_dev;

	if (!udcc->max_comp_cfg_xfer) {
		dev_err(rdev_to_dev(rdev),
			"udcc: can not proceed with %s, max: %d\n",
			__func__, udcc->max_comp_cfg_xfer);
		return -EINVAL;
	}

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_UDCC_COMP_QCFG, -1);

	req.arg_len = cpu_to_le32(udcc->cfg_arg_len);
	memcpy(&req.arg_buf, &udcc->cfg_arg, udcc->cfg_arg_len);

	udcc->cfg = dma_alloc_coherent(&en_dev->pdev->dev, udcc->max_comp_cfg_xfer,
				       &udcc->cfg_map, GFP_KERNEL);
	if (!udcc->cfg)
		return -ENOMEM;

	req.cfg_host_buf_size = cpu_to_le32(udcc->max_comp_cfg_xfer);
	req.cfg_host_addr[1] = cpu_to_le32(upper_32_bits(udcc->cfg_map));
	req.cfg_host_addr[0] = cpu_to_le32(lower_32_bits(udcc->cfg_map));

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));

	rc = bnxt_send_msg(rdev->en_dev, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to do udcc comp cfg, rc = %#x", rc);
		goto exit;
	}

	udcc->cfg_len = le32_to_cpu(resp.cfg_len);

	if (!udcc->cfg_len)
		dev_err(rdev_to_dev(rdev), "udcc: %s cfg_len is zero\n", __func__);
	else if (data)
		memcpy(data, udcc->cfg, udcc->cfg_len);
exit:
	dma_free_coherent(&en_dev->pdev->dev, udcc->max_comp_cfg_xfer, udcc->cfg,
			  udcc->cfg_map);
	return rc ? : udcc->cfg_len;
}

static ssize_t
config_write(struct config_item *item, const void *data, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct hwrm_udcc_comp_cfg_input req = {};
	struct hwrm_udcc_comp_cfg_output resp;
	struct bnxt_fw_msg fw_msg = {};
	struct bnxt_re_udcc_cfg *udcc;
	struct bnxt_en_dev *en_dev;
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	udcc = &rdev->udcc_cfg;
	en_dev = rdev->en_dev;

	if (!udcc->max_comp_cfg_xfer || count > udcc->max_comp_cfg_xfer) {
		dev_err(rdev_to_dev(rdev),
			"udcc: can not proceed with %s, requested: %lu, max: %d\n",
			__func__, count, udcc->max_comp_cfg_xfer);
		return -EINVAL;
	}

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_UDCC_COMP_CFG, -1);

	req.arg_len = cpu_to_le32(udcc->cfg_arg_len);
	memcpy(&req.arg_buf, &udcc->cfg_arg, udcc->cfg_arg_len);

	udcc->cfg = dma_alloc_coherent(&en_dev->pdev->dev, udcc->max_comp_cfg_xfer,
				       &udcc->cfg_map, GFP_KERNEL);
	if (!udcc->cfg)
		return -ENOMEM;

	memcpy(udcc->cfg, data, count);

	req.cfg_len = cpu_to_le32(count);
	req.cfg_host_addr[1] = cpu_to_le32(upper_32_bits(udcc->cfg_map));
	req.cfg_host_addr[0] = cpu_to_le32(lower_32_bits(udcc->cfg_map));

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));

	rc = bnxt_send_msg(rdev->en_dev, &fw_msg);
	if (rc)
		dev_err(rdev_to_dev(rdev), "Failed to do udcc cfg, rc = %#x", rc);

	dma_free_coherent(&en_dev->pdev->dev, udcc->max_comp_cfg_xfer, udcc->cfg,
			  udcc->cfg_map);
	return rc ? : udcc->cfg_len;
}

CONFIGFS_BIN_ATTR(, config, NULL, 4096);

static ssize_t
config_args_write(struct config_item *item, const void *data, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_udcc_cfg *udcc;
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	udcc = &rdev->udcc_cfg;

	if (!count || count > sizeof(udcc->cfg_arg)) {
		dev_err(rdev_to_dev(rdev),
			"udcc: can not proceed with %s, count: %lu, max: %ld\n",
			__func__, count, sizeof(udcc->cfg_arg));
		return -EINVAL;
	}

	udcc->cfg_arg_len = count;
	memcpy(&udcc->cfg_arg, data, count);

	return count;
}

CONFIGFS_BIN_ATTR_WO(, config_args, NULL, 4096);

static ssize_t
data_read(struct config_item *item, void *data, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct hwrm_udcc_comp_query_input req = {};
	struct hwrm_udcc_comp_query_output resp;
	struct bnxt_fw_msg fw_msg = {};
	struct bnxt_re_udcc_cfg *udcc;
	struct bnxt_en_dev *en_dev;
	struct bnxt_re_dev *rdev;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	udcc = &rdev->udcc_cfg;
	en_dev = rdev->en_dev;

	if (!udcc->max_comp_data_xfer) {
		dev_err(rdev_to_dev(rdev),
			"udcc: can not proceed with %s, max_comp_data_xfer: %d\n",
			__func__, udcc->max_comp_data_xfer);
		return -EINVAL;
	}

	udcc->data = dma_alloc_coherent(&en_dev->pdev->dev, udcc->max_comp_data_xfer,
					&udcc->data_map, GFP_KERNEL);
	if (!udcc->data)
		return -ENOMEM;

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_UDCC_COMP_QUERY, -1);

	req.arg_len = cpu_to_le32(udcc->data_arg_len);
	memcpy(&req.arg_buf, &udcc->data_arg, udcc->data_arg_len);
	req.data_host_buf_size = cpu_to_le32(udcc->max_comp_data_xfer);
	req.data_host_addr[1] = cpu_to_le32(upper_32_bits(udcc->data_map));
	req.data_host_addr[0] = cpu_to_le32(lower_32_bits(udcc->data_map));

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));

	rc = bnxt_send_msg(rdev->en_dev, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev),
			"Failed to do udcc comp query, rc = %#x", rc);
		goto exit;
	}

	udcc->data_len = le32_to_cpu(resp.data_len);
	if (!udcc->data_len)
		dev_err(rdev_to_dev(rdev), "udcc: %s cfg_len is zero\n", __func__);
	else if (data)
		memcpy(data, udcc->data, udcc->data_len);
exit:
	dma_free_coherent(&en_dev->pdev->dev, udcc->max_comp_data_xfer, udcc->data,
			  udcc->data_map);
	return rc ? : udcc->data_len;
}

CONFIGFS_BIN_ATTR_RO(, data, NULL, 4096);

static ssize_t
data_args_write(struct config_item *item, const void *data, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct bnxt_re_udcc_cfg *udcc;
	struct bnxt_re_dev *rdev;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	udcc = &rdev->udcc_cfg;

	if (!count || count > sizeof(udcc->cfg_arg)) {
		dev_err(rdev_to_dev(rdev),
			"udcc: can not proceed with %s, count: %lu, max: %ld\n",
			__func__, count, sizeof(udcc->cfg_arg));
		return -EINVAL;
	}

	udcc->data_arg_len = count;
	memcpy(&udcc->data_arg, data, count);

	return count;
}

CONFIGFS_BIN_ATTR_WO(, data_args, NULL, 4096);

static ssize_t
probe_pad_cnt_cfg_read(struct config_item *item, void *data, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct hwrm_udcc_qcfg_input req = {};
	struct hwrm_udcc_qcfg_output resp;
	struct bnxt_fw_msg fw_msg = {};
	struct bnxt_re_udcc_cfg *udcc;
	struct bnxt_re_dev *rdev;
	char buf[4];
	size_t len;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	udcc = &rdev->udcc_cfg;

	if (!udcc->probe_pad_cnt_updated) {
		bnxt_re_init_hwrm_hdr((void *)&req, HWRM_UDCC_QCFG, -1);

		bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
				    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));

		rc = bnxt_send_msg(rdev->en_dev, &fw_msg);
		if (rc) {
			dev_err(rdev_to_dev(rdev), "Failed to read probe_pad_cnt_cfg, rc: %#x", rc);
			return rc;
		}
		udcc->probe_pad_cnt_cfg = resp.probe_pad_cnt_cfg;
		udcc->probe_pad_cnt_updated = true;
	}

	len = snprintf(buf, sizeof(buf), "%u\n", udcc->probe_pad_cnt_cfg);

	if (!count)
		return len;

	if (count < len)
		len = count;

	memcpy(data, buf, len);
	return len;
}

static ssize_t
probe_pad_cnt_cfg_write(struct config_item *item, const void *data, size_t count)
{
	struct bnxt_re_cfg_group *ccgrp = __get_cc_group(item);
	struct hwrm_udcc_cfg_input req = {};
	struct hwrm_udcc_cfg_output resp;
	struct bnxt_fw_msg fw_msg = {};
	struct bnxt_re_udcc_cfg *udcc;
	struct bnxt_re_dev *rdev;
	char buf[4];
	u8 value;
	int rc;

	if (!ccgrp)
		return -EINVAL;

	rdev = bnxt_re_get_valid_rdev(ccgrp);
	if (!rdev)
		return -EINVAL;

	udcc = &rdev->udcc_cfg;

	if (count >= sizeof(buf)) {
		dev_err(rdev_to_dev(rdev),
			"Invalid probe_pad_cnt_cfg size %zu, expected %zu bytes\n",
			count, sizeof(u8));
		return -EINVAL;
	}

	memcpy(buf, data, count);
	buf[count] = '\0';

	rc = kstrtou8(buf, 10, &value);
	if (rc < 0) {
		dev_err(rdev_to_dev(rdev), "kstrtou8 failed with rc=%#x\n", rc);
		return rc;
	}

	if (value > UDCC_CFG_REQ_PROBE_PAD_CNT_CFG_LAST) {
		dev_err(rdev_to_dev(rdev), "Invalid probe_pad_cnt_cfg value: %u\n", value);
		return -EINVAL;
	}

	bnxt_re_init_hwrm_hdr((void *)&req, HWRM_UDCC_CFG, -1);

	req.enables |= cpu_to_le32(UDCC_CFG_REQ_ENABLES_PROBE_PAD_CNT_CFG);
	req.probe_pad_cnt_cfg = value;

	bnxt_re_fill_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			    sizeof(resp), BNXT_RE_HWRM_CMD_TIMEOUT(rdev));

	rc = bnxt_send_msg(rdev->en_dev, &fw_msg);
	if (rc) {
		dev_err(rdev_to_dev(rdev), "Failed to write probe_pad_cnt_cfg, rc = %#x", rc);
		return rc;
	}

	udcc->probe_pad_cnt_cfg = value;
	udcc->probe_pad_cnt_updated = true;

	return count;
}
CONFIGFS_BIN_ATTR(, probe_pad_cnt_cfg, NULL, 4096);

static struct configfs_bin_attribute *bnxt_re_udcc_bin_attrs[] = {
	CONFIGFS_ATTR_ADD(attr_config),
	CONFIGFS_ATTR_ADD(attr_config_args),
	CONFIGFS_ATTR_ADD(attr_data),
	CONFIGFS_ATTR_ADD(attr_data_args),
	CONFIGFS_ATTR_ADD(attr_probe_pad_cnt_cfg),
	NULL,
};

static struct config_item_type bnxt_re_udccgrp_type = {
	.ct_bin_attrs = bnxt_re_udcc_bin_attrs,
	.ct_item_ops = &bnxt_re_grp_ops,
	.ct_owner = THIS_MODULE,
};
#endif

static struct configfs_attribute *bnxt_re_tun_attrs[] = {
	CONFIGFS_ATTR_ADD(attr_min_tx_depth),
	CONFIGFS_ATTR_ADD(attr_stats_query_sec),
	CONFIGFS_ATTR_ADD(attr_gsi_qp_mode),
	CONFIGFS_ATTR_ADD(attr_wqe_mode),
	CONFIGFS_ATTR_ADD(attr_acc_tx_path),
	CONFIGFS_ATTR_ADD(attr_en_qp_dbg),
	CONFIGFS_ATTR_ADD(attr_dbr_pacing_enable),
	CONFIGFS_ATTR_ADD(attr_dbr_pacing_dbq_watermark),
	CONFIGFS_ATTR_ADD(attr_dbr_pacing_time),
	CONFIGFS_ATTR_ADD(attr_dbr_pacing_primary_fn),
	CONFIGFS_ATTR_ADD(attr_dbr_pacing_algo_threshold),
	CONFIGFS_ATTR_ADD(attr_dbr_pacing_en_int_threshold),
	CONFIGFS_ATTR_ADD(attr_dbr_def_do_pacing),
	CONFIGFS_ATTR_ADD(attr_user_dbr_drop_recov),
	CONFIGFS_ATTR_ADD(attr_user_dbr_drop_recov_timeout),
	CONFIGFS_ATTR_ADD(attr_cq_coal_buf_maxtime),
	CONFIGFS_ATTR_ADD(attr_cq_coal_normal_maxbuf),
	CONFIGFS_ATTR_ADD(attr_cq_coal_during_maxbuf),
	CONFIGFS_ATTR_ADD(attr_cq_coal_en_ring_idle_mode),
	CONFIGFS_ATTR_ADD(attr_snapdump_dbg_lvl),
	NULL,
};

static struct configfs_attribute *bnxt_re_p7_tun_attrs[] = {
	CONFIGFS_ATTR_ADD(attr_min_tx_depth),
	CONFIGFS_ATTR_ADD(attr_stats_query_sec),
	CONFIGFS_ATTR_ADD(attr_gsi_qp_mode),
	CONFIGFS_ATTR_ADD(attr_acc_tx_path),
	CONFIGFS_ATTR_ADD(attr_en_qp_dbg),
	CONFIGFS_ATTR_ADD(attr_dbr_pacing_enable),
	CONFIGFS_ATTR_ADD(attr_dbr_pacing_time),
	CONFIGFS_ATTR_ADD(attr_dbr_pacing_algo_threshold),
	CONFIGFS_ATTR_ADD(attr_dbr_def_do_pacing),
	CONFIGFS_ATTR_ADD(attr_user_dbr_drop_recov),
	CONFIGFS_ATTR_ADD(attr_user_dbr_drop_recov_timeout),
	CONFIGFS_ATTR_ADD(attr_cq_coal_buf_maxtime),
	CONFIGFS_ATTR_ADD(attr_cq_coal_normal_maxbuf),
	CONFIGFS_ATTR_ADD(attr_cq_coal_during_maxbuf),
	CONFIGFS_ATTR_ADD(attr_cq_coal_en_ring_idle_mode),
	CONFIGFS_ATTR_ADD(attr_snapdump_dbg_lvl),
	CONFIGFS_ATTR_ADD(attr_enable_queue_overflow_telemetry),
	NULL,
};

static struct config_item_type bnxt_re_tungrp_type = {
	.ct_attrs = bnxt_re_tun_attrs,
	.ct_item_ops = &bnxt_re_grp_ops,
	.ct_owner = THIS_MODULE,
};

static struct config_item_type bnxt_re_p7_tungrp_type = {
	.ct_attrs = bnxt_re_p7_tun_attrs,
	.ct_item_ops = &bnxt_re_grp_ops,
	.ct_owner = THIS_MODULE,
};

static int make_bnxt_re_tunables(struct bnxt_re_port_group *portgrp,
				 struct bnxt_re_dev *rdev, u32 gidx)
{
	struct bnxt_re_cfg_group *tungrp;

	tungrp = kzalloc(sizeof(*tungrp), GFP_KERNEL);
	if (!tungrp)
		return -ENOMEM;

	tungrp->rdev = rdev;
	if (_is_chip_p7_plus(rdev->chip_ctx))
		config_group_init_type_name(&tungrp->group, "tunables",
					    &bnxt_re_p7_tungrp_type);
	else
		config_group_init_type_name(&tungrp->group, "tunables",
					    &bnxt_re_tungrp_type);
#ifndef HAVE_CFGFS_ADD_DEF_GRP
	portgrp->nportgrp.default_groups = portgrp->default_grp;
	portgrp->default_grp[gidx] = &tungrp->group;
	portgrp->default_grp[gidx + 1] = NULL;
#else
	configfs_add_default_group(&tungrp->group, &portgrp->nportgrp);
#endif
	portgrp->tungrp = tungrp;
	tungrp->portgrp = portgrp;

	return 0;
}

#if defined(CONFIGFS_BIN_ATTR)
static int make_bnxt_re_udcc(struct bnxt_re_port_group *portgrp,
			     struct bnxt_re_dev *rdev, u32 gidx)
{
	struct bnxt_re_cfg_group *udccgrp;

	udccgrp = kzalloc(sizeof(*udccgrp), GFP_KERNEL);
	if (!udccgrp)
		return -ENOMEM;

	udccgrp->rdev = rdev;
	config_group_init_type_name(&udccgrp->group, "udcc",
				    &bnxt_re_udccgrp_type);
#ifndef HAVE_CFGFS_ADD_DEF_GRP
	portgrp->nportgrp.default_groups = portgrp->default_grp;
	portgrp->default_grp[gidx] = &udccgrp->group;
	portgrp->default_grp[gidx + 1] = NULL;
#else
	configfs_add_default_group(&udccgrp->group, &portgrp->nportgrp);
#endif
	portgrp->udccgrp = udccgrp;
	udccgrp->portgrp = portgrp;

	return 0;
}
#endif

static void bnxt_re_release_nport_group(struct bnxt_re_port_group *portgrp)
{
	kfree(portgrp->ccgrp);
	kfree(portgrp->tungrp);
#if defined(CONFIGFS_BIN_ATTR)
	kfree(portgrp->udccgrp);
#endif
}

static struct config_item_type bnxt_re_nportgrp_type = {
        .ct_owner = THIS_MODULE,
};

static int make_bnxt_re_ports(struct bnxt_re_dev_group *devgrp,
			      struct bnxt_re_dev *rdev)
{
#ifndef HAVE_CFGFS_ADD_DEF_GRP
	struct config_group **portsgrp = NULL;
#endif
	struct bnxt_re_port_group *ports;
	struct ib_device *ibdev;
	int nports, rc, indx;

	if (!rdev)
		return -ENODEV;
	ibdev = &rdev->ibdev;
	devgrp->nports = ibdev->phys_port_cnt;
	nports = devgrp->nports;
	ports = kcalloc(nports, sizeof(*ports), GFP_KERNEL);
	if (!ports) {
		rc = -ENOMEM;
		goto out;
	}

#ifndef HAVE_CFGFS_ADD_DEF_GRP
	portsgrp = kcalloc(nports + 1, sizeof(*portsgrp), GFP_KERNEL);
	if (!portsgrp) {
		rc = -ENOMEM;
		goto out;
	}
#endif
	for (indx = 0; indx < nports; indx++) {
		char port_name[10];
		ports[indx].port_num = indx + 1;
		snprintf(port_name, sizeof(port_name), "%u", indx + 1);
		ports[indx].devgrp = devgrp;
		config_group_init_type_name(&ports[indx].nportgrp,
					    port_name, &bnxt_re_nportgrp_type);
		rc = make_bnxt_re_cc(&ports[indx], rdev, 0);
		if (rc)
			goto out;
		rc = make_bnxt_re_tunables(&ports[indx], rdev, 1);
		if (rc)
			goto out;

#if defined(CONFIGFS_BIN_ATTR)
		if (!rdev->is_virtfn && bnxt_qplib_udcc_supported(rdev->chip_ctx)) {
			rc = make_bnxt_re_udcc(&ports[indx], rdev, 2);
			if (rc)
				goto out;
		}
#endif

#ifndef HAVE_CFGFS_ADD_DEF_GRP
		portsgrp[indx] = &ports[indx].nportgrp;
#else
		configfs_add_default_group(&ports[indx].nportgrp,
					   &devgrp->port_group);
#endif
	}

#ifndef HAVE_CFGFS_ADD_DEF_GRP
	portsgrp[indx] = NULL;
	devgrp->default_portsgrp = portsgrp;
#endif
	devgrp->ports = ports;

	return 0;
out:
#ifndef HAVE_CFGFS_ADD_DEF_GRP
	kfree(portsgrp);
#endif
	kfree(ports);
	return rc;
}

static void bnxt_re_release_ports_group(struct bnxt_re_dev_group *devgrp)
{
	int i;

	/*
	 * nport group is dynamically created along with ports creation, so
	 * that it should also be released along with ports group release.
	 */
	for (i = 0; i < devgrp->nports; i++)
		bnxt_re_release_nport_group(&devgrp->ports[i]);

#ifndef HAVE_CFGFS_ADD_DEF_GRP
	kfree(devgrp->default_portsgrp);
	devgrp->default_portsgrp = NULL;
#endif
	kfree(devgrp->ports);
	devgrp->ports = NULL;
}

static void bnxt_re_release_device_group(struct config_item *item)
{
	struct config_group *group = container_of(item, struct config_group,
						  cg_item);
	struct bnxt_re_dev_group *devgrp =
				container_of(group, struct bnxt_re_dev_group,
					     dev_group);

	/*
	 * ports group is dynamically created along dev group creation, so that
	 * it should also be released along with dev group release.
	 */
	bnxt_re_release_ports_group(devgrp);

	kfree(devgrp);
}

static struct config_item_type bnxt_re_ports_group_type = {
	.ct_owner = THIS_MODULE,
};

static struct configfs_item_operations bnxt_re_dev_item_ops = {
	.release = bnxt_re_release_device_group
};

static struct config_item_type bnxt_re_dev_group_type = {
	.ct_item_ops = &bnxt_re_dev_item_ops,
	.ct_owner = THIS_MODULE,
};

static struct config_group *make_bnxt_re_dev(struct config_group *group,
					     const char *name)
{
	struct bnxt_re_dev_group *devgrp = NULL;
	struct bnxt_re_dev *rdev;
	int rc = -ENODEV;

	rdev = __get_rdev_from_name(name);
	if (PTR_ERR(rdev) == -ENODEV)
		goto out;

	devgrp = kzalloc(sizeof(*devgrp), GFP_KERNEL);
	if (!devgrp) {
		rc = -ENOMEM;
		goto out;
	}

	if (strlen(name) >= sizeof(devgrp->name)) {
		rc = -EINVAL;
		goto out;
	}
	strcpy(devgrp->name, name);
	config_group_init_type_name(&devgrp->port_group, "ports",
                                    &bnxt_re_ports_group_type);
	rc = make_bnxt_re_ports(devgrp, rdev);
	if (rc)
		goto out;
	config_group_init_type_name(&devgrp->dev_group, name,
                                    &bnxt_re_dev_group_type);
#ifndef HAVE_CFGFS_ADD_DEF_GRP
	devgrp->port_group.default_groups = devgrp->default_portsgrp;
	devgrp->dev_group.default_groups = devgrp->default_devgrp;
	devgrp->default_devgrp[0] = &devgrp->port_group;
	devgrp->default_devgrp[1] = NULL;
#else
	configfs_add_default_group(&devgrp->port_group,
				   &devgrp->dev_group);
#endif

	return &devgrp->dev_group;
out:
	kfree(devgrp);
	return ERR_PTR(rc);
}

static void drop_bnxt_re_dev(struct config_group *group, struct config_item *item)
{
	config_item_put(item);
}

static struct configfs_group_operations bnxt_re_group_ops = {
	.make_group = &make_bnxt_re_dev,
	.drop_item = &drop_bnxt_re_dev
};

static struct config_item_type bnxt_re_subsys_type = {
	.ct_group_ops	= &bnxt_re_group_ops,
	.ct_owner	= THIS_MODULE,
};

static struct configfs_subsystem bnxt_re_subsys = {
	.su_group	= {
		.cg_item	= {
			.ci_namebuf	= "bnxt_re",
			.ci_type	= &bnxt_re_subsys_type,
		},
	},
};

int bnxt_re_configfs_init(void)
{
	config_group_init(&bnxt_re_subsys.su_group);
	mutex_init(&bnxt_re_subsys.su_mutex);
	return configfs_register_subsystem(&bnxt_re_subsys);
}

void bnxt_re_configfs_exit(void)
{
	configfs_unregister_subsystem(&bnxt_re_subsys);
}
