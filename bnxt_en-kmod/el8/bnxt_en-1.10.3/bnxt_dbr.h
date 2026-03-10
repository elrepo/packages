/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2014-2016 Broadcom Corporation
 * Copyright (c) 2016-2018 Broadcom Limited
 * Copyright (c) 2018-2022 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BNXT_DBR_H
#define BNXT_DBR_H

#include <linux/delay.h>

/* 32-bit XORSHIFT generator.  Seed must not be zero. */
static inline u32 xorshift(u32 *state)
{
	u32 seed = *state;

	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;

	*state = seed;
	return seed;
}

static inline u16 rnd(u32 *state, uint16_t range)
{
	/* range must be a power of 2 - 1 */
	return (xorshift(state) & range);
}

#define BNXT_DB_FIFO_ROOM_MASK      0x1fff8000
#define BNXT_DB_FIFO_ROOM_SHIFT     15
#define BNXT_MAX_FIFO_DEPTH         0x2c00

#define BNXT_DB_PACING_ALGO_THRESHOLD	250
#define BNXT_DEFAULT_PACING_PROBABILITY 0xFFFF

#define BNXT_DBR_PACING_WIN_BASE	0x2000
#define BNXT_DBR_PACING_WIN_MAP_OFF	4
#define BNXT_DBR_PACING_WIN_OFF(reg)	(BNXT_DBR_PACING_WIN_BASE +	\
					 ((reg) & BNXT_GRC_OFFSET_MASK))

struct bnxt_dbr_sw_stats {
	u32			nr_dbr;
	u64			total_dbr_us;
	u64			avg_dbr_us;
	u64			max_dbr_us;
	u64			min_dbr_us;
};

struct bnxt_dbr_debug {
	u32			recover_interval_ms;
	u32			drop_ratio;
	u32			drop_cnt;
	u8			recover_enable;
	u8			drop_enable;
};

struct bnxt_dbr {
	u8			enable;
	u8			pacing_enable;
	atomic_t		event_cnt;

	/* dedicated workqueue for DB recovery DRA */
	struct workqueue_struct	*wq;
	struct delayed_work	dwork;
	struct mutex		lock; /* protect this data struct */

	u32			curr_epoch;
	u32			last_l2_epoch;
	u32			last_roce_epoch;
	u32			last_completed_epoch;

	u32			stat_db_fifo_reg;
	u32			db_fifo_reg_off;

	struct bnxt_dbr_sw_stats sw_stats;
	struct bnxt_dbr_debug debug;
};

static inline int __get_fifo_occupancy(void __iomem *bar0, u32 db_fifo_reg_off)
{
	u32 val;

	val = readl(bar0 + db_fifo_reg_off);
	return BNXT_MAX_FIFO_DEPTH -
		((val & BNXT_DB_FIFO_ROOM_MASK) >>
		 BNXT_DB_FIFO_ROOM_SHIFT);
}

/* Caller make sure that the pacing is enabled or not */
static inline void bnxt_do_pacing(void __iomem *bar0, struct bnxt_dbr *dbr,
				  u32 *seed, u32 pacing_th, u32 pacing_prob)
{
	u32 pace_time = 1;
	u32 retry = 10;

	if (!dbr->pacing_enable)
		return;

	if (rnd(seed, 0xFFFF) < pacing_prob) {
		while (__get_fifo_occupancy(bar0, dbr->db_fifo_reg_off) > pacing_th &&
		       retry--) {
			u32 us_delay;

			us_delay = rnd(seed, pace_time - 1);
			if (us_delay)
				udelay(us_delay);
			/* pacing delay time capped at 128 us */
			pace_time = min_t(u16, pace_time * 2, 128);
		}
	}
}
#endif
