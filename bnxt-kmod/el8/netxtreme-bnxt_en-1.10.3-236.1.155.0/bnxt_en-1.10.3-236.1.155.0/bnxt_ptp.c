/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2017-2018 Broadcom Limited
 * Copyright (c) 2018-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#ifdef HAVE_IEEE1588_SUPPORT
#include <linux/ptp_clock_kernel.h>
#include <linux/net_tstamp.h>
#include <linux/timecounter.h>
#include <linux/timekeeping.h>
#include <linux/ptp_classify.h>
#include <linux/clocksource.h>
#endif
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_ptp.h"

#ifdef HAVE_IEEE1588_SUPPORT
static int bnxt_ptp_cfg_settime(struct bnxt *bp, u64 time)
{
	struct hwrm_func_ptp_cfg_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_FUNC_PTP_CFG);
	if (rc)
		return rc;

	req->enables = cpu_to_le16(FUNC_PTP_CFG_REQ_ENABLES_PTP_SET_TIME);
	req->ptp_set_time = time;
	return hwrm_req_send(bp, req);
}

int bnxt_ptp_parse(struct sk_buff *skb, u16 *seq_id, u16 *hdr_off)
{
	unsigned int ptp_class;
	struct ptp_header *hdr;

	ptp_class = ptp_classify_raw(skb);

	switch (ptp_class & PTP_CLASS_VMASK) {
	case PTP_CLASS_V1:
	case PTP_CLASS_V2:
		hdr = ptp_parse_header(skb, ptp_class);
		if (!hdr)
			return -EINVAL;

		if (hdr_off)
			*hdr_off = (u8 *)hdr - skb->data;
		*seq_id	 = ntohs(hdr->sequence_id);
		return 0;
	default:
		return -ERANGE;
	}
}

static int bnxt_ptp_settime(struct ptp_clock_info *ptp_info,
			    const struct timespec64 *ts)
{
	struct bnxt_ptp_cfg *ptp = container_of(ptp_info, struct bnxt_ptp_cfg,
						ptp_info);
	u64 ns = timespec64_to_ns(ts);
	unsigned long flags;

	if (BNXT_PTP_USE_RTC(ptp->bp))
		return bnxt_ptp_cfg_settime(ptp->bp, ns);

	write_seqlock_irqsave(&ptp->ptp_lock, flags);
	timecounter_init(&ptp->tc, &ptp->cc, ns);
	write_sequnlock_irqrestore(&ptp->ptp_lock, flags);
	return 0;
}

/* Caller holds ptp_lock */
static int __bnxt_refclk_read(struct bnxt *bp, struct ptp_system_timestamp *sts,
			      u64 *ns)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	u32 high_before, high_now, low;

	if (test_bit(BNXT_STATE_IN_FW_RESET, &bp->state))
		return -EIO;

	high_before = readl(bp->bar0 + ptp->refclk_mapped_regs[1]);
	ptp_read_system_prets(sts);
	low = readl(bp->bar0 + ptp->refclk_mapped_regs[0]);
	ptp_read_system_postts(sts);
	high_now = readl(bp->bar0 + ptp->refclk_mapped_regs[1]);
	if (high_now != high_before) {
		ptp_read_system_prets(sts);
		low = readl(bp->bar0 + ptp->refclk_mapped_regs[0]);
		ptp_read_system_postts(sts);
	}
	*ns = (((u64)high_now) << 32) | ((u64)low);

	return 0;
}

static int bnxt_refclk_read(struct bnxt *bp, struct ptp_system_timestamp *sts,
			    u64 *ns)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	unsigned long flags;
	int rc;

	/* We have to serialize reg access and FW reset */
	read_seqlock_excl_irqsave(&ptp->ptp_lock, flags);
	rc = __bnxt_refclk_read(bp, sts, ns);
	read_sequnlock_excl_irqrestore(&ptp->ptp_lock, flags);
	return rc;
}

#ifdef HAVE_PTP_GETTIMEX64
static int bnxt_refclk_read_low(struct bnxt *bp, struct ptp_system_timestamp *sts,
				u32 *low)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	unsigned long flags;

	/* We have to serialize reg access and FW reset */
	read_seqlock_excl_irqsave(&ptp->ptp_lock, flags);

	if (test_bit(BNXT_STATE_IN_FW_RESET, &bp->state)) {
		read_sequnlock_excl_irqrestore(&ptp->ptp_lock, flags);
		return -EIO;
	}

	ptp_read_system_prets(sts);
	*low = readl(bp->bar0 + ptp->refclk_mapped_regs[0]);
	ptp_read_system_postts(sts);

	read_sequnlock_excl_irqrestore(&ptp->ptp_lock, flags);
	return 0;
}
#endif

static int bnxt_hwrm_port_ts_query(struct bnxt *bp, u32 flags, u64 *ts,
				   u32 txts_tmo, int slot)
{
	struct hwrm_port_ts_query_output *resp;
	struct hwrm_port_ts_query_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_PORT_TS_QUERY);
	if (rc)
		return rc;

	req->flags = cpu_to_le32(flags);
	if (flags == PORT_TS_QUERY_REQ_FLAGS_PATH_TX) {
		u32 tmo_us = txts_tmo * 1000;

		req->enables = cpu_to_le16(BNXT_PTP_QTS_TX_ENABLES);
		req->ptp_seq_id = cpu_to_le32(bp->ptp_cfg->txts_req[slot].tx_seqid);
		req->ptp_hdr_offset = cpu_to_le16(bp->ptp_cfg->txts_req[slot].tx_hdr_off);
		if (!tmo_us)
			tmo_us = BNXT_PTP_QTS_TIMEOUT(bp);
		tmo_us = min(tmo_us, BNXT_PTP_QTS_MAX_TMO_US);
		req->ts_req_timeout = cpu_to_le16(tmo_us);
	} else if (flags == PORT_TS_QUERY_REQ_FLAGS_PATH_RX) {
		req->ptp_seq_id = cpu_to_le32(bp->ptp_cfg->rx_seqid);
		req->enables = cpu_to_le16(BNXT_PTP_QTS_RX_ENABLES);
	}

	resp = hwrm_req_hold(bp, req);

	rc = hwrm_req_send(bp, req);
	if (rc) {
		hwrm_req_drop(bp, req);
		return rc;
	}
	*ts = le64_to_cpu(resp->ptp_msg_ts);
	hwrm_req_drop(bp, req);
	return 0;
}

static void bnxt_ptp_get_current_time(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	if (!ptp)
		return;
	WRITE_ONCE(ptp->old_time, ptp->current_time >> BNXT_HI_TIMER_SHIFT);
	bnxt_refclk_read(bp, NULL, &ptp->current_time);
}

void bnxt_ptp_get_skb_pre_xmit_ts(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	if (!ptp)
		return;
	bnxt_refclk_read(bp, NULL, &ptp->skb_pre_xmit_ts);
}

#ifdef HAVE_PTP_GETTIMEX64
static int bnxt_ptp_gettimex(struct ptp_clock_info *ptp_info,
			     struct timespec64 *ts,
			     struct ptp_system_timestamp *sts)
{
	struct bnxt_ptp_cfg *ptp = container_of(ptp_info, struct bnxt_ptp_cfg,
						ptp_info);
	u64 ns, cycles;
	u32 low;
	int rc;

	rc = bnxt_refclk_read_low(ptp->bp, sts, &low);
	if (rc)
		return rc;

	cycles = bnxt_extend_cycles_32b_to_48b(ptp, low);
	ns = bnxt_timecounter_cyc2time(ptp, cycles);
	*ts = ns_to_timespec64(ns);

	return 0;
}
#else
static int bnxt_ptp_gettime(struct ptp_clock_info *ptp_info,
			    struct timespec64 *ts)
{
	struct bnxt_ptp_cfg *ptp = container_of(ptp_info, struct bnxt_ptp_cfg,
						ptp_info);
	unsigned long flags;
	u64 ns;

	write_seqlock_irqsave(&ptp->ptp_lock, flags);
	ns = timecounter_read(&ptp->tc);
	write_sequnlock_irqrestore(&ptp->ptp_lock, flags);
	*ts = ns_to_timespec64(ns);
	return 0;
}
#endif

void bnxt_ptp_update_current_time(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	bnxt_refclk_read(bp, NULL, &ptp->current_time);
	WRITE_ONCE(ptp->old_time, ptp->current_time >> BNXT_HI_TIMER_SHIFT);
}

static int bnxt_ptp_adjphc(struct bnxt_ptp_cfg *ptp, s64 delta)
{
	struct hwrm_port_mac_cfg_input *req;
	struct bnxt *bp = ptp->bp;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_PORT_MAC_CFG);
	if (rc)
		return rc;

	req->enables = cpu_to_le32(PORT_MAC_CFG_REQ_ENABLES_PTP_ADJ_PHASE);
	req->ptp_adj_phase = delta;

	rc = hwrm_req_send(bp, req);
	if (rc) {
		netdev_err(bp->dev, "ptp adjphc failed. rc = %x\n", rc);
	} else {
		bnxt_ptp_update_current_time(bp);
	}

	return rc;
}

static int bnxt_ptp_adjtime(struct ptp_clock_info *ptp_info, s64 delta)
{
	struct bnxt_ptp_cfg *ptp = container_of(ptp_info, struct bnxt_ptp_cfg,
						ptp_info);
	unsigned long flags;

	if (BNXT_PTP_USE_RTC(ptp->bp))
		return bnxt_ptp_adjphc(ptp, delta);

	write_seqlock_irqsave(&ptp->ptp_lock, flags);
	timecounter_adjtime(&ptp->tc, delta);
	write_sequnlock_irqrestore(&ptp->ptp_lock, flags);
	return 0;
}

#ifdef HAVE_PTP_ADJPHASE
static int bnxt_ptp_adjphase(struct ptp_clock_info *ptp_info, s32 offset_ns)
{
	struct bnxt_ptp_cfg *ptp = container_of(ptp_info, struct bnxt_ptp_cfg,
						ptp_info);
	struct hwrm_port_mac_cfg_input *req;
	struct bnxt *bp = ptp->bp;
	int rc;

	if (!(bp->flags & BNXT_FLAG_CHIP_P5_PLUS))
		return -EOPNOTSUPP;

	rc = hwrm_req_init(bp, req, HWRM_PORT_MAC_CFG);
	if (rc)
		return rc;

	req->enables = cpu_to_le32(PORT_MAC_CFG_REQ_ENABLES_PTP_ADJ_PHASE);
	req->ptp_adj_phase = cpu_to_le32(offset_ns);

	rc = hwrm_req_send(bp, req);
	if (rc)
		netdev_err(bp->dev, "ptp adjphase failed. rc = %x\n", rc);

	return rc;
}
#endif

static int bnxt_ptp_adjfine_rtc(struct bnxt *bp, s32 ppb)
{
	struct hwrm_port_mac_cfg_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_PORT_MAC_CFG);
	if (rc)
		return rc;

	req->ptp_freq_adj_ppb = cpu_to_le32(ppb);
	req->enables = cpu_to_le32(PORT_MAC_CFG_REQ_ENABLES_PTP_FREQ_ADJ_PPB);
	rc = hwrm_req_send(bp, req);
	if (rc)
		netdev_err(bp->dev,
			   "ptp adjfine failed. rc = %d\n", rc);
	return rc;
}

#ifdef HAVE_SCALED_PPM
static int bnxt_ptp_adjfine(struct ptp_clock_info *ptp_info, long scaled_ppm)
#else
static int bnxt_ptp_adjfreq(struct ptp_clock_info *ptp_info, s32 ppb)
#endif
{
	struct bnxt_ptp_cfg *ptp = container_of(ptp_info, struct bnxt_ptp_cfg,
						ptp_info);
	s32 period, period1, period2, dif, dif1, dif2;
	s32 step, best_step = 0, best_period = 0;
	s32 best_dif = BNXT_MAX_PHC_DRIFT;
	struct bnxt *bp = ptp->bp;
	unsigned long flags;
	u32 drift_sign = 1;
#ifdef HAVE_SCALED_PPM
	s32 ppb = scaled_ppm_to_ppb(scaled_ppm);
#endif

	if (bp->flags & BNXT_FLAG_CHIP_P5_PLUS ||
	    BNXT_CHIP_NUM_5745X(bp->chip_num)) {
#if !defined(HAVE_SCALED_PPM)
		int neg_adj = 0;
		u32 diff;
		u64 adj;
#endif

		if (!BNXT_MH(bp))
			return bnxt_ptp_adjfine_rtc(bp, ppb);

#if !defined(HAVE_SCALED_PPM)
		if (ppb < 0) {
			neg_adj = 1;
			ppb = -ppb;
		}
		adj = ptp->cmult;
		adj *= ppb;
		diff = div_u64(adj, 1000000000ULL);
#endif
		write_seqlock_irqsave(&ptp->ptp_lock, flags);
		timecounter_read(&ptp->tc);
#ifdef HAVE_SCALED_PPM
		ptp->cc.mult = adjust_by_scaled_ppm(ptp->cmult, scaled_ppm);
#else
		ptp->cc.mult = neg_adj ? ptp->cmult - diff : ptp->cmult + diff;
#endif
		write_sequnlock_irqrestore(&ptp->ptp_lock, flags);
		return 0;
	}

	/* Frequency adjustment requires programming 3 values:
	 * 1-bit direction
	 * 5-bit adjustment step in 1 ns unit
	 * 24-bit period in 1 us unit between adjustments
	 */
	if (ppb < 0) {
		ppb = -ppb;
		drift_sign = 0;
	}

	if (ppb == 0) {
		/* no adjustment */
		best_step = 0;
		best_period = 0xFFFFFF;
	} else if (ppb >= BNXT_MAX_PHC_DRIFT) {
		/* max possible adjustment */
		best_step = 31;
		best_period = 1;
	} else {
		/* Find the best possible adjustment step and period */
		for (step = 0; step <= 31; step++) {
			period1 = step * 1000000 / ppb;
			period2 = period1 + 1;
			if (period1 != 0)
				dif1 = ppb - (step * 1000000 / period1);
			else
				dif1 = BNXT_MAX_PHC_DRIFT;
			if (dif1 < 0)
				dif1 = -dif1;
			dif2 = ppb - (step * 1000000 / period2);
			if (dif2 < 0)
				dif2 = -dif2;
			dif = (dif1 < dif2) ? dif1 : dif2;
			period = (dif1 < dif2) ? period1 : period2;
			if (dif < best_dif) {
				best_dif = dif;
				best_step = step;
				best_period = period;
			}
		}
	}
	writel((drift_sign << BNXT_GRCPF_REG_SYNC_TIME_ADJ_SIGN_SFT) |
	       (best_step << BNXT_GRCPF_REG_SYNC_TIME_ADJ_VAL_SFT) |
	       (best_period & BNXT_GRCPF_REG_SYNC_TIME_ADJ_PER_MSK),
	       bp->bar0 + BNXT_GRCPF_REG_SYNC_TIME_ADJ);

	return 0;
}

void bnxt_ptp_pps_event(struct bnxt *bp, u32 data1, u32 data2)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	struct ptp_clock_event event;
	u64 ns, pps_ts;

	pps_ts = EVENT_PPS_TS(data2, data1);
	ns = bnxt_timecounter_cyc2time(ptp, pps_ts);

	switch (EVENT_DATA2_PPS_EVENT_TYPE(data2)) {
	case ASYNC_EVENT_CMPL_PPS_TIMESTAMP_EVENT_DATA2_EVENT_TYPE_INTERNAL:
		event.pps_times.ts_real = ns_to_timespec64(ns);
		event.type = PTP_CLOCK_PPSUSR;
		event.index = EVENT_DATA2_PPS_PIN_NUM(data2);
		break;
	case ASYNC_EVENT_CMPL_PPS_TIMESTAMP_EVENT_DATA2_EVENT_TYPE_EXTERNAL:
		event.timestamp = ns;
		event.type = PTP_CLOCK_EXTTS;
		event.index = EVENT_DATA2_PPS_PIN_NUM(data2);
		break;
	}

	ptp_clock_event(ptp->ptp_clock, &event);
}

static int bnxt_ptp_cfg_pin(struct bnxt *bp, int pin, u8 usage)
{
	struct hwrm_func_ptp_pin_cfg_input *req;
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	u8 state = usage != BNXT_PPS_PIN_NONE;
	u8 *pin_state, *pin_usg;
	u32 enables;
	int rc;

	if (!TSIO_PIN_VALID(pin)) {
		netdev_err(bp->dev, "1PPS: Invalid pin. Check pin-function configuration\n");
		return -EOPNOTSUPP;
	}

	rc = hwrm_req_init(bp, req, HWRM_FUNC_PTP_PIN_CFG);
	if (rc)
		return rc;

	enables = (FUNC_PTP_PIN_CFG_REQ_ENABLES_PIN0_STATE |
		   FUNC_PTP_PIN_CFG_REQ_ENABLES_PIN0_USAGE) << (pin * 2);
	req->enables = cpu_to_le32(enables);

	pin_state = &req->pin0_state;
	pin_usg = &req->pin0_usage;

	*(pin_state + (pin * 2)) = state;
	*(pin_usg + (pin * 2)) = usage;

	rc = hwrm_req_send(bp, req);
	if (rc)
		return rc;

	ptp->pps_info.pins[pin].usage = usage;
	ptp->pps_info.pins[pin].state = state;

	return 0;
}

static int bnxt_ptp_cfg_event(struct bnxt *bp, u8 event)
{
	struct hwrm_func_ptp_cfg_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_FUNC_PTP_CFG);
	if (rc)
		return rc;

	req->enables = cpu_to_le16(FUNC_PTP_CFG_REQ_ENABLES_PTP_PPS_EVENT);
	req->ptp_pps_event = event;
	return hwrm_req_send(bp, req);
}

void bnxt_ptp_cfg_tstamp_filters(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	struct hwrm_port_mac_cfg_input *req;

	if (!ptp || !ptp->tstamp_filters)
		return;

	if (hwrm_req_init(bp, req, HWRM_PORT_MAC_CFG))
		goto out;

	if (!(bp->fw_cap & BNXT_FW_CAP_RX_ALL_PKT_TS) && (ptp->tstamp_filters &
	    (PORT_MAC_CFG_REQ_FLAGS_ALL_RX_TS_CAPTURE_ENABLE |
	     PORT_MAC_CFG_REQ_FLAGS_ALL_RX_TS_CAPTURE_DISABLE))) {
		ptp->tstamp_filters &= ~(PORT_MAC_CFG_REQ_FLAGS_ALL_RX_TS_CAPTURE_ENABLE |
					 PORT_MAC_CFG_REQ_FLAGS_ALL_RX_TS_CAPTURE_DISABLE);
		netdev_warn(bp->dev, "Unsupported FW for all RX pkts timestamp filter\n");
	}

	req->flags = cpu_to_le32(ptp->tstamp_filters);
	req->enables = cpu_to_le32(PORT_MAC_CFG_REQ_ENABLES_RX_TS_CAPTURE_PTP_MSG_TYPE);
	req->rx_ts_capture_ptp_msg_type = cpu_to_le16(ptp->rxctl);

	if (!hwrm_req_send(bp, req)) {
		bp->ptp_all_rx_tstamp = !!(ptp->tstamp_filters &
					   PORT_MAC_CFG_REQ_FLAGS_ALL_RX_TS_CAPTURE_ENABLE);
		return;
	}
	ptp->tstamp_filters = 0;
out:
	bp->ptp_all_rx_tstamp = 0;
	netdev_warn(bp->dev, "Failed to configure HW packet timestamp filters\n");
}

void bnxt_ptp_reapply_pps(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	struct bnxt_pps *pps;
	u32 pin = 0;
	int rc;

	if (!ptp || !(bp->fw_cap & BNXT_FW_CAP_PTP_PPS) ||
	    !(ptp->ptp_info.pin_config))
		return;
	pps = &ptp->pps_info;
	for (pin = 0; pin < BNXT_MAX_TSIO_PINS; pin++) {
		if (pps->pins[pin].state) {
			rc = bnxt_ptp_cfg_pin(bp, pin, pps->pins[pin].usage);
			if (!rc && pps->pins[pin].event)
				rc = bnxt_ptp_cfg_event(bp,
							pps->pins[pin].event);
			if (rc)
				netdev_err(bp->dev, "1PPS: Failed to configure pin%d\n",
					   pin);
		}
	}
}

void bnxt_ptp_reapply_phc(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	unsigned long flags;
	u64 current_ns;

	if (!ptp || (bp->flags & BNXT_FLAG_CHIP_P5_PLUS))
		return;

	/* Account for the delta when stored to now */
	write_seqlock_irqsave(&ptp->ptp_lock, flags);
	ptp->current_time += ktime_get_ns() - ptp->save_ts;
	current_ns = ptp->current_time;
	WRITE_ONCE(ptp->old_time, current_ns);
	writel(lower_32_bits(current_ns), bp->bar0 + ptp->refclk_mapped_regs[0]);
	writel(upper_32_bits(current_ns), bp->bar0 + ptp->refclk_mapped_regs[1]);
	write_sequnlock_irqrestore(&ptp->ptp_lock, flags);
}

static int bnxt_ptp_perout_cfg(struct bnxt_ptp_cfg *ptp,
			       struct ptp_clock_request *rq)
{
	struct hwrm_func_ptp_cfg_input *req;
	struct bnxt *bp = ptp->bp;
	struct timespec64 ts;
	u64 target_ns;
	u16 enables;
	int rc;

	ts.tv_sec = rq->perout.start.sec;
	ts.tv_nsec = rq->perout.start.nsec;
	target_ns = timespec64_to_ns(&ts);

	rc = hwrm_req_init(bp, req, HWRM_FUNC_PTP_CFG);
	if (rc)
		return rc;

	enables = FUNC_PTP_CFG_REQ_ENABLES_PTP_FREQ_ADJ_EXT_PERIOD |
		  FUNC_PTP_CFG_REQ_ENABLES_PTP_FREQ_ADJ_EXT_UP |
		  FUNC_PTP_CFG_REQ_ENABLES_PTP_FREQ_ADJ_EXT_PHASE;
	req->enables = cpu_to_le16(enables);
	req->ptp_pps_event = 0;
	req->ptp_freq_adj_dll_source = 0;
	req->ptp_freq_adj_dll_phase = 0;
	req->ptp_freq_adj_ext_period = cpu_to_le32(NSEC_PER_SEC);
	req->ptp_freq_adj_ext_up = 0;
	req->ptp_freq_adj_ext_phase_lower = cpu_to_le32(lower_32_bits(target_ns));
	req->ptp_freq_adj_ext_phase_upper = cpu_to_le32(upper_32_bits(target_ns));

	return hwrm_req_send(bp, req);
}

static int bnxt_ptp_enable(struct ptp_clock_info *ptp_info,
			   struct ptp_clock_request *rq, int on)
{
	struct bnxt_ptp_cfg *ptp = container_of(ptp_info, struct bnxt_ptp_cfg,
						ptp_info);
	struct bnxt *bp = ptp->bp;
	int pin_id;
	int rc;

	switch (rq->type) {
	case PTP_CLK_REQ_EXTTS:
		/* Configure an External PPS IN */
		pin_id = ptp_find_pin(ptp->ptp_clock, PTP_PF_EXTTS,
				      rq->extts.index);
		if (!TSIO_PIN_VALID(pin_id))
			return -EOPNOTSUPP;
		if (!on)
			break;
		rc = bnxt_ptp_cfg_pin(bp, pin_id, BNXT_PPS_PIN_PPS_IN);
		if (rc)
			return rc;
		rc = bnxt_ptp_cfg_event(bp, BNXT_PPS_EVENT_EXTERNAL);
		if (!rc)
			ptp->pps_info.pins[pin_id].event = BNXT_PPS_EVENT_EXTERNAL;
		return rc;
	case PTP_CLK_REQ_PEROUT:
		/* Configure a Periodic PPS OUT */
		pin_id = ptp_find_pin(ptp->ptp_clock, PTP_PF_PEROUT,
				      rq->perout.index);
		if (!TSIO_PIN_VALID(pin_id))
			return -EOPNOTSUPP;
		if (!on)
			break;

		rc = bnxt_ptp_cfg_pin(bp, pin_id, BNXT_PPS_PIN_PPS_OUT);
		if (!rc)
			rc = bnxt_ptp_perout_cfg(ptp, rq);

		return rc;
	case PTP_CLK_REQ_PPS:
		/* Configure PHC PPS IN */
		rc = bnxt_ptp_cfg_pin(bp, 0, BNXT_PPS_PIN_PPS_IN);
		if (rc)
			return rc;
		rc = bnxt_ptp_cfg_event(bp, BNXT_PPS_EVENT_INTERNAL);
		if (!rc)
			ptp->pps_info.pins[0].event = BNXT_PPS_EVENT_INTERNAL;
		return rc;
	default:
		netdev_err(bp->dev, "Unrecognized PIN function\n");
		return -EOPNOTSUPP;
	}

	return bnxt_ptp_cfg_pin(bp, pin_id, BNXT_PPS_PIN_NONE);
}

static int bnxt_hwrm_ptp_cfg(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	u32 flags = 0;
	int rc = 0;

	switch (ptp->rx_filter) {
	case HWTSTAMP_FILTER_ALL:
		flags = PORT_MAC_CFG_REQ_FLAGS_ALL_RX_TS_CAPTURE_ENABLE;
		break;
	case HWTSTAMP_FILTER_NONE:
		flags = PORT_MAC_CFG_REQ_FLAGS_PTP_RX_TS_CAPTURE_DISABLE;
		if (bp->fw_cap & BNXT_FW_CAP_RX_ALL_PKT_TS)
			flags |= PORT_MAC_CFG_REQ_FLAGS_ALL_RX_TS_CAPTURE_DISABLE;
		break;
	case HWTSTAMP_FILTER_PTP_V2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
		flags = PORT_MAC_CFG_REQ_FLAGS_PTP_RX_TS_CAPTURE_ENABLE;
		break;
	}

	if (ptp->tx_tstamp_en)
		flags |= PORT_MAC_CFG_REQ_FLAGS_PTP_TX_TS_CAPTURE_ENABLE;
	else
		flags |= PORT_MAC_CFG_REQ_FLAGS_PTP_TX_TS_CAPTURE_DISABLE;

	ptp->tstamp_filters = flags;

	if (netif_running(bp->dev)) {
		if (ptp->rx_filter == HWTSTAMP_FILTER_ALL) {
			bnxt_close_nic(bp, false, false);
			rc = bnxt_open_nic(bp, false, false);
		} else {
			bnxt_ptp_cfg_tstamp_filters(bp);
		}
		if (!rc && !ptp->tstamp_filters)
			rc = -EIO;
	}

	return rc;
}

int bnxt_hwtstamp_set(struct net_device *dev, struct ifreq *ifr)
{
	struct bnxt *bp = netdev_priv(dev);
	struct hwtstamp_config stmpconf;
	struct bnxt_ptp_cfg *ptp;
	u16 old_rxctl;
	int old_rx_filter, rc;
	u8 old_tx_tstamp_en;

	ptp = bp->ptp_cfg;
	if (!ptp)
		return -EOPNOTSUPP;

	if (copy_from_user(&stmpconf, ifr->ifr_data, sizeof(stmpconf)))
		return -EFAULT;

#ifndef HAVE_HWTSTAMP_FLAG_BONDED_PHC_INDEX
	if (stmpconf.flags)
		return -EINVAL;
#endif

	if (stmpconf.tx_type != HWTSTAMP_TX_ON &&
	    stmpconf.tx_type != HWTSTAMP_TX_OFF)
		return -ERANGE;

	old_rx_filter = ptp->rx_filter;
	old_rxctl = ptp->rxctl;
	old_tx_tstamp_en = ptp->tx_tstamp_en;
	switch (stmpconf.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		ptp->rxctl = 0;
		ptp->rx_filter = HWTSTAMP_FILTER_NONE;
		break;
	case HWTSTAMP_FILTER_ALL:
		if (bp->fw_cap & BNXT_FW_CAP_RX_ALL_PKT_TS) {
			ptp->rx_filter = HWTSTAMP_FILTER_ALL;
			break;
		}
		return -EOPNOTSUPP;
	case HWTSTAMP_FILTER_PTP_V2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
		ptp->rxctl = BNXT_PTP_MSG_EVENTS;
		ptp->rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
		break;
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
		ptp->rxctl = BNXT_PTP_MSG_SYNC;
		ptp->rx_filter = HWTSTAMP_FILTER_PTP_V2_SYNC;
		break;
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
		ptp->rxctl = BNXT_PTP_MSG_DELAY_REQ;
		ptp->rx_filter = HWTSTAMP_FILTER_PTP_V2_DELAY_REQ;
		break;
	default:
		return -ERANGE;
	}

	if (stmpconf.tx_type == HWTSTAMP_TX_ON)
		ptp->tx_tstamp_en = 1;
	else
		ptp->tx_tstamp_en = 0;

	rc = bnxt_hwrm_ptp_cfg(bp);
	if (rc)
		goto ts_set_err;

	stmpconf.rx_filter = ptp->rx_filter;
	return copy_to_user(ifr->ifr_data, &stmpconf, sizeof(stmpconf)) ?
		-EFAULT : 0;

ts_set_err:
	ptp->rx_filter = old_rx_filter;
	ptp->rxctl = old_rxctl;
	ptp->tx_tstamp_en = old_tx_tstamp_en;
	return rc;
}

int bnxt_hwtstamp_get(struct net_device *dev, struct ifreq *ifr)
{
	struct bnxt *bp = netdev_priv(dev);
	struct hwtstamp_config stmpconf;
	struct bnxt_ptp_cfg *ptp;

	ptp = bp->ptp_cfg;
	if (!ptp)
		return -EOPNOTSUPP;

	stmpconf.flags = 0;
	stmpconf.tx_type = ptp->tx_tstamp_en ? HWTSTAMP_TX_ON : HWTSTAMP_TX_OFF;

	stmpconf.rx_filter = ptp->rx_filter;
	return copy_to_user(ifr->ifr_data, &stmpconf, sizeof(stmpconf)) ?
		-EFAULT : 0;
}

static int bnxt_map_regs(struct bnxt *bp, u32 *reg_arr, int count, int reg_win)
{
	u32 reg_base = *reg_arr & BNXT_GRC_BASE_MASK;
	u32 win_off;
	int i;

	for (i = 0; i < count; i++) {
		if ((reg_arr[i] & BNXT_GRC_BASE_MASK) != reg_base)
			return -ERANGE;
	}
	win_off = BNXT_GRCPF_REG_WINDOW_BASE_OUT + (reg_win - 1) * 4;
	writel(reg_base, bp->bar0 + win_off);
	return 0;
}

static int bnxt_map_ptp_regs(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	u32 *reg_arr;
	int rc, i;

	reg_arr = ptp->refclk_regs;
	if (BNXT_CHIP_P5(bp)) {
		u32 base = BNXT_PTP_GRC_WIN_BASE;
		int win = BNXT_PTP_GRC_WIN;

		if (BNXT_VF(bp)) {
			base = BNXT_PTP_GRC_WIN_BASE_VF;
			win = BNXT_PTP_GRC_WIN_VF;
		}
		rc = bnxt_map_regs(bp, reg_arr, 2, win);
		if (rc)
			return rc;
		for (i = 0; i < 2; i++)
			ptp->refclk_mapped_regs[i] = base +
				(ptp->refclk_regs[i] & BNXT_GRC_OFFSET_MASK);
		return 0;
	}
	for (i = 0; i < 2; i++) {
		if (reg_arr[i] & BNXT_GRC_BASE_MASK)
			return -EINVAL;
		ptp->refclk_mapped_regs[i] = ptp->refclk_regs[i];
	}

	return 0;
}

static void bnxt_unmap_ptp_regs(struct bnxt *bp)
{
	writel(0, bp->bar0 + BNXT_GRCPF_REG_WINDOW_BASE_OUT +
		  (BNXT_PTP_GRC_WIN - 1) * 4);
	if (!(bp->flags & BNXT_FLAG_CHIP_P5_PLUS))
		writel(0, bp->bar0 + BNXT_GRCPF_REG_WINDOW_BASE_OUT + 16);
}

static u64 bnxt_cc_read(const struct cyclecounter *cc)
{
	struct bnxt_ptp_cfg *ptp = container_of(cc, struct bnxt_ptp_cfg, cc);
	u64 ns = 0;

	__bnxt_refclk_read(ptp->bp, NULL, &ns);
	return ns;
}

int bnxt_get_rx_ts(struct bnxt *bp, struct bnxt_napi *bnapi,
		   u32 vlan, struct sk_buff *skb)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	if (ptp->rx_skb) {
		netdev_err(bp->dev, "deferring skb:one SKB is still outstanding\n");
		return -EBUSY;
	}

	ptp->rx_skb = skb;
	ptp->bnapi = bnapi;
	ptp->vlan = vlan;
#if !defined HAVE_PTP_DO_AUX_WORK
	schedule_work(&ptp->ptp_ts_task);
#else
	ptp_schedule_worker(ptp->ptp_clock, 0);
#endif
	return 0;
}

static int bnxt_stamp_tx_skb(struct bnxt *bp, int slot)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	struct skb_shared_hwtstamps timestamp;
	struct bnxt_ptp_tx_req *txts_req;
	unsigned long now = jiffies;
	u64 ts = 0, ns = 0;
	u32 tmo = 0;
	int rc;

	txts_req = &ptp->txts_req[slot];
	/* make sure bnxt_get_tx_ts() has finished updating */
	smp_rmb();
	if (!time_after_eq(now, txts_req->abs_txts_tmo))
		tmo = jiffies_to_msecs(txts_req->abs_txts_tmo - now);
	rc = bnxt_hwrm_port_ts_query(bp, PORT_TS_QUERY_REQ_FLAGS_PATH_TX, &ts, tmo, slot);
	if (!rc) {
		if (!(bp->flags & BNXT_FLAG_CHIP_P5_PLUS) &&
		    (ts < ptp->skb_pre_xmit_ts))
			goto retry_ts;

		memset(&timestamp, 0, sizeof(timestamp));
		ns = bnxt_timecounter_cyc2time(ptp, ts);
		timestamp.hwtstamp = ns_to_ktime(ns);
		skb_tstamp_tx(txts_req->tx_skb, &timestamp);
		ptp->stats.ts_pkts++;
	} else {
retry_ts:
		if (!time_after_eq(jiffies, txts_req->abs_txts_tmo))
			return -EAGAIN;

		ptp->stats.ts_lost++;
		netdev_warn_once(bp->dev, "TS query for TX timer failed rc = %x\n",
				 rc);
	}

	dev_kfree_skb_any(txts_req->tx_skb);
	txts_req->tx_skb = NULL;

	return 0;
}

static void bnxt_stamp_rx_skb(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	unsigned long flags;
	u64 ts = 0, ns = 0;
	int rc;

	__skb_push(ptp->rx_skb, ETH_HLEN);
	/* On BCM57414 chips, hdr_offset is not supported, only seqid */
	bnxt_ptp_parse(ptp->rx_skb, &ptp->rx_seqid, NULL);
	__skb_pull(ptp->rx_skb, ETH_HLEN);

	rc = bnxt_hwrm_port_ts_query(bp, PORT_TS_QUERY_REQ_FLAGS_PATH_RX, &ts,
				     0, 0);

	if (!rc) {
		write_seqlock_irqsave(&ptp->ptp_lock, flags);
		ns = timecounter_cyc2time(&ptp->tc, ts);
		write_sequnlock_irqrestore(&ptp->ptp_lock, flags);
		memset(skb_hwtstamps(ptp->rx_skb), 0, sizeof(*skb_hwtstamps(ptp->rx_skb)));
		skb_hwtstamps(ptp->rx_skb)->hwtstamp = ns_to_ktime(ns);
	} else {
		netdev_err(bp->dev, "TS query for RX timer failed rc = %x\n", rc);
	}
	bnxt_deliver_skb(bp, ptp->bnapi, ptp->vlan, ptp->rx_skb);
	ptp->rx_skb = NULL;
}

void bnxt_ptp_free_txts_skbs(struct bnxt_ptp_cfg *ptp)
{
	struct bnxt_ptp_tx_req *txts_req;
	u16 cons = ptp->txts_cons;

	/* make sure ptp aux worker finished with
	 * possible BNXT_STATE_OPEN set
	 */
#if defined(HAVE_PTP_DO_AUX_WORK) && defined(HAVE_PTP_CANCEL_WORKER_SYNC)
	ptp_cancel_worker_sync(ptp->ptp_clock);
#else
	bnxt_ptp_cancel_worker_sync(ptp);
#endif

	ptp->tx_avail = BNXT_MAX_TX_TS;
	while (cons != ptp->txts_prod) {
		txts_req = &ptp->txts_req[cons];
		if (!IS_ERR_OR_NULL(txts_req->tx_skb))
			dev_kfree_skb_any(txts_req->tx_skb);
		cons = NEXT_TXTS(cons);
	}
	ptp->txts_cons = cons;
#if defined(HAVE_PTP_DO_AUX_WORK) && defined(HAVE_PTP_CANCEL_WORKER_SYNC)
	ptp_schedule_worker(ptp->ptp_clock, 0);
#else
	bnxt_ptp_schedule_worker(ptp);
#endif
}

int bnxt_ptp_get_txts_prod(struct bnxt_ptp_cfg *ptp, u16 *prod)
{
	spin_lock_bh(&ptp->ptp_tx_lock);
	if (ptp->tx_avail) {
		*prod = ptp->txts_prod;
		ptp->txts_prod = NEXT_TXTS(*prod);
		ptp->tx_avail--;
		spin_unlock_bh(&ptp->ptp_tx_lock);
		return 0;
	}
	spin_unlock_bh(&ptp->ptp_tx_lock);
	atomic64_inc(&ptp->stats.ts_err);
	return -ENOSPC;
}

#if defined HAVE_PTP_DO_AUX_WORK
static long bnxt_ptp_ts_aux_work(struct ptp_clock_info *ptp_info)
{
	struct bnxt_ptp_cfg *ptp = container_of(ptp_info, struct bnxt_ptp_cfg,
						ptp_info);
	unsigned long now = jiffies;
	struct bnxt *bp = ptp->bp;
	u16 cons = ptp->txts_cons;
	unsigned long flags;
	u8 num_requests;
	int rc = 0;

	if (ptp->shutdown)
		return -1;
	num_requests = BNXT_MAX_TX_TS - READ_ONCE(ptp->tx_avail);
	while (num_requests--) {
		if (IS_ERR(ptp->txts_req[cons].tx_skb)) {
			atomic64_inc(&ptp->stats.ts_err);
			goto next_slot;
		}
		if (!ptp->txts_req[cons].tx_skb)
			break;
		rc = bnxt_stamp_tx_skb(bp, cons);
		if (rc == -EAGAIN)
			break;
next_slot:
		BNXT_PTP_INC_TX_AVAIL(ptp);
		cons = NEXT_TXTS(cons);
	}
	ptp->txts_cons = cons;

	if (ptp->rx_skb)
		bnxt_stamp_rx_skb(bp);

	if (!time_after_eq(now, ptp->next_period)) {
		if (rc == -EAGAIN)
			return 0;
		return ptp->next_period - now;
	}

	bnxt_ptp_get_current_time(bp);
	ptp->next_period = now + HZ;
	if (time_after_eq(now, ptp->next_overflow_check)) {
		write_seqlock_irqsave(&ptp->ptp_lock, flags);
		timecounter_read(&ptp->tc);
		write_sequnlock_irqrestore(&ptp->ptp_lock, flags);
		ptp->next_overflow_check = now + BNXT_PHC_OVERFLOW_PERIOD;
	}
	if (rc == -EAGAIN)
		return 0;
	return HZ;
}
#else
void bnxt_ptp_timer(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	unsigned long flags;

	if (!ptp)
		return;

	bnxt_ptp_get_current_time(bp);
	if (time_after_eq(jiffies, ptp->next_overflow_check)) {
		write_seqlock_irqsave(&ptp->ptp_lock, flags);
		timecounter_read(&ptp->tc);
		write_sequnlock_irqrestore(&ptp->ptp_lock, flags);
		ptp->next_overflow_check = jiffies + BNXT_PHC_OVERFLOW_PERIOD;
	}
}

static void bnxt_ptp_ts_task(struct work_struct *work)
{
	struct bnxt_ptp_cfg *ptp = container_of(work, struct bnxt_ptp_cfg,
						ptp_ts_task);
	struct bnxt *bp = ptp->bp;
	u16 cons = ptp->txts_cons;
	u8 num_requests;
	int rc = 0;

	num_requests = BNXT_MAX_TX_TS - READ_ONCE(ptp->tx_avail);
	while (num_requests--) {
		if (IS_ERR(ptp->txts_req[cons].tx_skb)) {
			atomic64_inc(&ptp->stats.ts_err);
			goto next_slot;
		}
		if (!ptp->txts_req[cons].tx_skb)
			break;
		rc = bnxt_stamp_tx_skb(bp, cons);
		if (rc == -EAGAIN)
			break;
next_slot:
		BNXT_PTP_INC_TX_AVAIL(ptp);
		cons = NEXT_TXTS(cons);
	}
	ptp->txts_cons = cons;
	if (ptp->rx_skb)
		bnxt_stamp_rx_skb(bp);
	if (rc == -EAGAIN && ptp->ptp_clock)
		schedule_work(&ptp->ptp_ts_task);
}
#endif
int bnxt_get_tx_ts(struct bnxt *bp, struct sk_buff *skb, u16 prod)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	struct bnxt_ptp_tx_req *txts_req;

	txts_req = &ptp->txts_req[prod];
	txts_req->abs_txts_tmo = jiffies + msecs_to_jiffies(ptp->txts_tmo);
	/* make sure bnxt_stamp_tx_skb() is in sync */
	smp_wmb();
	txts_req->tx_skb = skb;
#if !defined HAVE_PTP_DO_AUX_WORK
	schedule_work(&ptp->ptp_ts_task);
#else
	ptp_schedule_worker(ptp->ptp_clock, 0);
#endif
	return 0;
}

int bnxt_get_rx_ts_p5(struct bnxt *bp, u64 *ts, u32 pkt_ts)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	*ts = bnxt_extend_cycles_32b_to_48b(ptp, pkt_ts);

	return 0;
}

void bnxt_tx_ts_cmp(struct bnxt *bp, struct bnxt_napi *bnapi,
		    struct tx_ts_cmp *tscmp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	struct skb_shared_hwtstamps timestamp;
	u32 opaque = tscmp->tx_ts_cmp_opaque;
	struct bnxt_tx_ring_info *txr;
	struct bnxt_sw_tx_bd *tx_buf;
	u64 ts, ns;
	u16 cons;

	txr = bnapi->tx_ring[TX_OPAQUE_RING(opaque)];
	ts = BNXT_GET_TX_TS_48B_NS(tscmp);
	cons = TX_OPAQUE_IDX(opaque);
	tx_buf = &txr->tx_buf_ring[RING_TX(bp, cons)];
	if (tx_buf->is_ts_pkt) {
		if (BNXT_TX_TS_ERR(tscmp)) {
			netdev_err(bp->dev,
				   "timestamp completion error 0x%x 0x%x\n",
				   le32_to_cpu(tscmp->tx_ts_cmp_flags_type),
				   le32_to_cpu(tscmp->tx_ts_cmp_errors_v));
		} else {
			ns = bnxt_timecounter_cyc2time(ptp, ts);
			memset(&timestamp, 0, sizeof(timestamp));
			timestamp.hwtstamp = ns_to_ktime(ns);
			skb_tstamp_tx(tx_buf->skb, &timestamp);
		}
		tx_buf->is_ts_pkt = 0;
	}
}

#ifdef HAVE_ARTNS_TO_TSC
static int bnxt_phc_get_syncdevicetime(ktime_t *device,
				       struct system_counterval_t *system,
				       void *ctx)
{
	struct bnxt_ptp_cfg *ptp = (struct bnxt_ptp_cfg *)ctx;
	struct hwrm_func_ptp_ts_query_output *resp;
	struct hwrm_func_ptp_ts_query_input *req;
	struct bnxt *bp = ptp->bp;
	unsigned long flags;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_FUNC_PTP_TS_QUERY);
	if (rc)
		return rc;
	req->flags = cpu_to_le32(FUNC_PTP_TS_QUERY_REQ_FLAGS_PTM_TIME);
	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (rc) {
		hwrm_req_drop(bp, req);
		return rc;
	}
	write_seqlock_irqsave(&ptp->ptp_lock, flags);
	*device = ns_to_ktime(timecounter_cyc2time(&ptp->tc, le64_to_cpu(resp->ptm_local_ts)));
	write_sequnlock_irqrestore(&ptp->ptp_lock, flags);
	*system = convert_art_ns_to_tsc(le64_to_cpu(resp->ptm_system_ts));
	hwrm_req_drop(bp, req);

	return 0;
}

static int bnxt_ptp_getcrosststamp(struct ptp_clock_info *ptp_info,
				   struct system_device_crosststamp *xtstamp)
{
	struct bnxt_ptp_cfg *ptp = container_of(ptp_info, struct bnxt_ptp_cfg,
						ptp_info);

	if (!(ptp->bp->fw_cap & BNXT_FW_CAP_PTP_PTM))
		return -EOPNOTSUPP;
	return get_device_system_crosststamp(bnxt_phc_get_syncdevicetime,
					     ptp, NULL, xtstamp);
}
#endif

static const struct ptp_clock_info bnxt_ptp_caps = {
	.owner		= THIS_MODULE,
	.name		= "bnxt clock",
	.max_adj	= BNXT_MAX_PHC_DRIFT,
	.n_alarm	= 0,
	.n_ext_ts	= 0,
	.n_per_out	= 0,
	.n_pins		= 0,
	.pps		= 0,
#ifdef HAVE_SCALED_PPM
	.adjfine	= bnxt_ptp_adjfine,
#else
	.adjfreq	= bnxt_ptp_adjfreq,
#endif /* HAVE_SCALED_PPM */
#ifdef HAVE_PTP_ADJPHASE
	.adjphase	= bnxt_ptp_adjphase,
#endif
	.adjtime	= bnxt_ptp_adjtime,
#ifdef HAVE_PTP_DO_AUX_WORK
	.do_aux_work	= bnxt_ptp_ts_aux_work,
#endif
#ifdef HAVE_PTP_GETTIMEX64
	.gettimex64	= bnxt_ptp_gettimex,
#else
	.gettime64	= bnxt_ptp_gettime,
#endif
	.settime64	= bnxt_ptp_settime,
	.enable		= bnxt_ptp_enable,
#ifdef HAVE_ARTNS_TO_TSC
	.getcrosststamp = bnxt_ptp_getcrosststamp,
#endif
};

static int bnxt_ptp_verify(struct ptp_clock_info *ptp_info, unsigned int pin,
			   enum ptp_pin_function func, unsigned int chan)
{
	struct bnxt_ptp_cfg *ptp = container_of(ptp_info, struct bnxt_ptp_cfg,
						ptp_info);
	/* Allow only PPS pin function configuration */
	if (ptp->pps_info.pins[pin].usage <= BNXT_PPS_PIN_PPS_OUT &&
	    func != PTP_PF_PHYSYNC)
		return 0;
	else
		return -EOPNOTSUPP;
}

static int bnxt_ptp_pps_init(struct bnxt *bp)
{
	struct hwrm_func_ptp_pin_qcfg_output *resp;
	struct hwrm_func_ptp_pin_qcfg_input *req;
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	struct ptp_clock_info *ptp_info;
	struct bnxt_pps *pps_info;
	u8 *pin_usg;
	u32 i, rc;

	/* Query current/default PIN CFG */
	rc = hwrm_req_init(bp, req, HWRM_FUNC_PTP_PIN_QCFG);
	if (rc)
		return rc;

	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (rc || !resp->num_pins) {
		hwrm_req_drop(bp, req);
		return -EOPNOTSUPP;
	}

	ptp_info = &ptp->ptp_info;
	pps_info = &ptp->pps_info;
	pps_info->num_pins = resp->num_pins;
	ptp_info->n_pins = pps_info->num_pins;
	ptp_info->pin_config = kcalloc(ptp_info->n_pins,
				       sizeof(*ptp_info->pin_config),
				       GFP_KERNEL);
	if (!ptp_info->pin_config) {
		hwrm_req_drop(bp, req);
		return -ENOMEM;
	}

	/* Report the TSIO capability to kernel */
	pin_usg = &resp->pin0_usage;
	for (i = 0; i < pps_info->num_pins; i++, pin_usg++) {
		snprintf(ptp_info->pin_config[i].name,
			 sizeof(ptp_info->pin_config[i].name), "bnxt_pps%d", i);
		ptp_info->pin_config[i].index = i;
		ptp_info->pin_config[i].chan = i;
		if (*pin_usg == BNXT_PPS_PIN_PPS_IN)
			ptp_info->pin_config[i].func = PTP_PF_EXTTS;
		else if (*pin_usg == BNXT_PPS_PIN_PPS_OUT)
			ptp_info->pin_config[i].func = PTP_PF_PEROUT;
		else
			ptp_info->pin_config[i].func = PTP_PF_NONE;

		pps_info->pins[i].usage = *pin_usg;
	}
	hwrm_req_drop(bp, req);

	/* Only 1 each of ext_ts and per_out pins is available in HW */
	ptp_info->n_ext_ts = 1;
	ptp_info->n_per_out = 1;
	ptp_info->pps = 1;
	ptp_info->verify = bnxt_ptp_verify;

	return 0;
}

static bool bnxt_pps_config_ok(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	return !(bp->fw_cap & BNXT_FW_CAP_PTP_PPS) == !ptp->ptp_info.pin_config;
}

static void bnxt_ptp_timecounter_init(struct bnxt *bp, bool init_tc)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	unsigned long flags;

	if (!ptp->ptp_clock) {
		memset(&ptp->cc, 0, sizeof(ptp->cc));
		ptp->cc.read = bnxt_cc_read;
		if (bp->flags & BNXT_FLAG_CHIP_P5_PLUS)
			ptp->cc.mask = CYCLECOUNTER_MASK(48);
		else
			ptp->cc.mask = CYCLECOUNTER_MASK(64);
		if (BNXT_MH(bp)) {
			/* Use timecounter based non-real time mode */
			ptp->cc.shift = BNXT_CYCLES_SHIFT;
			ptp->cc.mult = clocksource_khz2mult(BNXT_DEVCLK_FREQ, ptp->cc.shift);
			ptp->cmult = ptp->cc.mult;
		} else {
			ptp->cc.shift = 0;
			ptp->cc.mult = 1;
		}
		ptp->next_overflow_check = jiffies + BNXT_PHC_OVERFLOW_PERIOD;
	}
	if (init_tc) {
		write_seqlock_irqsave(&ptp->ptp_lock, flags);
		timecounter_init(&ptp->tc, &ptp->cc, ktime_to_ns(ktime_get_real()));
		write_sequnlock_irqrestore(&ptp->ptp_lock, flags);
	}
}

/* Caller holds ptp_lock */
void bnxt_ptp_rtc_timecounter_init(struct bnxt_ptp_cfg *ptp, u64 ns)
{
	timecounter_init(&ptp->tc, &ptp->cc, ns);
	/* For RTC, cycle_last must be in sync with the timecounter value. */
	ptp->tc.cycle_last = ns & ptp->cc.mask;
}

int bnxt_ptp_init_rtc(struct bnxt *bp, bool phc_cfg)
{
	struct timespec64 tsp;
	unsigned long flags;
	u64 ns;
	int rc;

	if (!bp->ptp_cfg || !BNXT_PTP_USE_RTC(bp))
		return -ENODEV;

	if (!phc_cfg) {
		ktime_get_real_ts64(&tsp);
		ns = timespec64_to_ns(&tsp);
		rc = bnxt_ptp_cfg_settime(bp, ns);
		if (rc)
			return rc;
	} else {
		rc = bnxt_hwrm_port_ts_query(bp, PORT_TS_QUERY_REQ_FLAGS_CURRENT_TIME,
					     &ns, 0, 0);
		if (rc)
			return rc;
	}
	write_seqlock_irqsave(&bp->ptp_cfg->ptp_lock, flags);
	bnxt_ptp_rtc_timecounter_init(bp->ptp_cfg, ns);
	write_sequnlock_irqrestore(&bp->ptp_cfg->ptp_lock, flags);

	return 0;
}

static void bnxt_ptp_free(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	if (ptp->ptp_clock) {
		ptp->shutdown = 1;
		ptp_clock_unregister(ptp->ptp_clock);
		ptp->ptp_clock = NULL;
	}
	kfree(ptp->ptp_info.pin_config);
	ptp->ptp_info.pin_config = NULL;
}

int bnxt_ptp_init(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	int rc;

	if (!ptp)
		return 0;

	rc = bnxt_map_ptp_regs(bp);
	if (rc)
		return rc;
	if (!(bp->flags & BNXT_FLAG_CHIP_P5_PLUS)) {
		/* Initialize freq adj GRC space to 0 so that stratus
		 * can ignore GRC and use external TS block
		 */
		writel(0, bp->bar0 + BNXT_GRCPF_REG_SYNC_TIME_ADJ);
	}

	if (ptp->ptp_clock && bnxt_pps_config_ok(bp))
		return 0;

	bnxt_ptp_free(bp);

	WRITE_ONCE(ptp->tx_avail, BNXT_MAX_TX_TS);
	seqlock_init(&ptp->ptp_lock);
	spin_lock_init(&ptp->ptp_tx_lock);

	if (BNXT_PTP_USE_RTC(bp)) {
		bnxt_ptp_timecounter_init(bp, false);
		rc = bnxt_ptp_init_rtc(bp, ptp->rtc_configured);
		if (rc)
			goto out;
	} else {
		bnxt_ptp_timecounter_init(bp, true);
		if (BNXT_MH(bp))
			bnxt_ptp_adjfine_rtc(bp, 0);
	}

	ptp->ptp_info = bnxt_ptp_caps;
	if ((bp->fw_cap & BNXT_FW_CAP_PTP_PPS)) {
		if (bnxt_ptp_pps_init(bp))
			netdev_warn(bp->dev, "1pps not initialized, continuing without 1pps support\n");
	}
	ptp->ptp_clock = ptp_clock_register(&ptp->ptp_info, &bp->pdev->dev);
	if (IS_ERR(ptp->ptp_clock)) {
		rc = PTR_ERR(ptp->ptp_clock);
		ptp->ptp_clock = NULL;
		goto out;
	}
	bnxt_hwrm_func_drv_rgtr(bp, NULL, 0, true);

#if !defined HAVE_PTP_DO_AUX_WORK
	INIT_WORK(&ptp->ptp_ts_task, bnxt_ptp_ts_task);
#endif
	ptp->stats.ts_pkts = 0;
	ptp->stats.ts_lost = 0;
	atomic64_set(&ptp->stats.ts_err, 0);

	bnxt_refclk_read(bp, NULL, &ptp->current_time);
	WRITE_ONCE(ptp->old_time, ptp->current_time >> BNXT_HI_TIMER_SHIFT);
#ifdef HAVE_PTP_DO_AUX_WORK
	ptp_schedule_worker(ptp->ptp_clock, 0);
#endif
	ptp->txts_tmo = BNXT_PTP_DFLT_TX_TMO;
	return 0;

out:
	bnxt_ptp_free(bp);
	bnxt_unmap_ptp_regs(bp);
	return rc;
}

void bnxt_ptp_clear(struct bnxt *bp)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	if (!ptp)
		return;

#if !defined HAVE_PTP_DO_AUX_WORK
	cancel_work_sync(&ptp->ptp_ts_task);
#endif

	bnxt_ptp_free(bp);

	if (ptp->rx_skb) {
		dev_kfree_skb_any(ptp->rx_skb);
		ptp->rx_skb = NULL;
	}

	bnxt_unmap_ptp_regs(bp);
}

void bnxt_save_pre_reset_ts(struct bnxt *bp)
{
	if (BNXT_CHIP_P5_PLUS(bp))
		return;

	bnxt_ptp_get_current_time(bp);
	bp->ptp_cfg->save_ts = ktime_get_ns();
}

#else

void bnxt_ptp_timer(struct bnxt *bp)
{
}

int bnxt_hwtstamp_set(struct net_device *dev, struct ifreq *ifr)
{
	return -EOPNOTSUPP;
}

int bnxt_hwtstamp_get(struct net_device *dev, struct ifreq *ifr)
{
	return -EOPNOTSUPP;
}

int bnxt_ptp_init(struct bnxt *bp)
{
	return 0;
}

void bnxt_ptp_clear(struct bnxt *bp)
{
}

void bnxt_ptp_reapply_pps(struct bnxt *bp)
{
}

void bnxt_ptp_cfg_tstamp_filters(struct bnxt *bp)
{
}

void bnxt_ptp_pps_event(struct bnxt *bp, u32 data1, u32 data2)
{
}

int bnxt_ptp_init_rtc(struct bnxt *bp, bool phc_cfg)
{
	return 0;
}

void bnxt_ptp_reapply_phc(struct bnxt *bp)
{
}

void bnxt_save_pre_reset_ts(struct bnxt *bp)
{
}

void bnxt_tx_ts_cmp(struct bnxt *bp, struct bnxt_napi *bnapi,
		    struct tx_ts_cmp *tscmp)
{
}
#endif
