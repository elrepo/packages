/* SPDX-License-Identifier: GPL-2.0-only */
/*
################################################################################
#
# r8126 is the Linux device driver released for Realtek 5 Gigabit Ethernet
# controllers with PCI-Express interface.
#
# Copyright(c) 2024 Realtek Semiconductor Corp. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.
#
# Author:
# Realtek NIC software team <nicfae@realtek.com>
# No. 2, Innovation Road II, Hsinchu Science Park, Hsinchu 300, Taiwan
#
################################################################################
*/

/************************************************************************************
 *  This product is covered by one or more of the following patents:
 *  US6,570,884, US6,115,776, and US6,327,625.
 ***********************************************************************************/

#ifndef _LINUX_R8126_PTP_H
#define _LINUX_R8126_PTP_H

#include <linux/ktime.h>
#include <linux/timecounter.h>
#include <linux/net_tstamp.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/ptp_classify.h>

#ifndef PTP_CLASS_NONE
#define PTP_CLASS_NONE  0x00
#endif

#ifndef PTP_MSGTYPE_SYNC
#define PTP_MSGTYPE_SYNC        0x0
#endif
#ifndef PTP_MSGTYPE_DELAY_REQ
#define PTP_MSGTYPE_DELAY_REQ   0x1
#endif
#ifndef PTP_MSGTYPE_PDELAY_REQ
#define PTP_MSGTYPE_PDELAY_REQ  0x2
#endif
#ifndef PTP_MSGTYPE_PDELAY_RESP
#define PTP_MSGTYPE_PDELAY_RESP 0x3
#endif

struct rtl8126_ptp_info {
        s64 time_sec;
        u32 time_ns;
        u16 ts_info;
};

#ifndef _STRUCT_TIMESPEC
#define _STRUCT_TIMESPEC
struct timespec {
        __kernel_old_time_t tv_sec;     /* seconds */
        long            tv_nsec;    /* nanoseconds */
};
#endif

enum PTP_CMD_TYPE {
        PTP_CMD_SET_LOCAL_TIME = 0,
        PTP_CMD_DRIFT_LOCAL_TIME,
        PTP_CMD_LATCHED_LOCAL_TIME,
};

enum PTP_CLKADJ_MOD_TYPE {
        NO_FUNCTION     = 0,
        CLKADJ_MODE_SET = 1,
        RESERVED        = 2,
        DIRECT_READ     = 4,
        DIRECT_WRITE    = 6,
        INCREMENT_STEP  = 8,
        DECREMENT_STEP  = 10,
        RATE_READ       = 12,
        RATE_WRITE      = 14,
};

enum PTP_INSR_TYPE {
        EVENT_CAP_INTR   = (1 << 0),
        TRIG_GEN_INTR    = (1 << 1),
        RX_TS_INTR       = (1 << 2),
        TX_TX_INTR       = (1 << 3),
};

enum PTP_TRX_TS_STA_REG {
        TRX_TS_RD               = (1 << 0),
        TRXTS_SEL               = (1 << 1),
        RX_TS_PDLYRSP_RDY       = (1 << 8),
        RX_TS_PDLYREQ_RDY       = (1 << 9),
        RX_TS_DLYREQ_RDY        = (1 << 10),
        RX_TS_SYNC_RDY          = (1 << 11),
        TX_TS_PDLYRSP_RDY       = (1 << 12),
        TX_TS_PDLYREQ_RDY       = (1 << 13),
        TX_TS_DLYREQ_RDY        = (1 << 14),
        TX_TS_SYNC_RDY          = (1 << 15),
};

#define PTP_CTL_TYPE_0 (0xF3F)
#define PTP_CTL_TYPE_1 (0x2FF)
#define PTP_CTL_TYPE_2 (0x0FF)
#define PTP_CTL_TYPE_3 (0x03F)

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
struct clock_identity {
        u8 id[8];
} __packed;

struct port_identity {
        struct clock_identity	clock_identity;
        __be16			port_number;
} __packed;

struct ptp_header {
        u8			tsmt;  /* transportSpecific | messageType */
        u8			ver;   /* reserved          | versionPTP  */
        __be16			message_length;
        u8			domain_number;
        u8			reserved1;
        u8			flag_field[2];
        __be64			correction;
        __be32			reserved2;
        struct port_identity	source_port_identity;
        __be16			sequence_id;
        u8			control;
        u8			log_message_interval;
} __packed;

/**
 * ptp_parse_header - Get pointer to the PTP v2 header
 * @skb: packet buffer
 * @type: type of the packet (see ptp_classify_raw())
 *
 * This function takes care of the VLAN, UDP, IPv4 and IPv6 headers. The length
 * is checked.
 *
 * Note, internally skb_mac_header() is used. Make sure that the @skb is
 * initialized accordingly.
 *
 * Return: Pointer to the ptp v2 header or NULL if not found
 */
struct ptp_header *ptp_parse_header(struct sk_buff *skb, unsigned int type);

/**
 * ptp_get_msgtype - Extract ptp message type from given header
 * @hdr: ptp header
 * @type: type of the packet (see ptp_classify_raw())
 *
 * This function returns the message type for a given ptp header. It takes care
 * of the different ptp header versions (v1 or v2).
 *
 * Return: The message type
 */
static inline u8 ptp_get_msgtype(const struct ptp_header *hdr,
                                 unsigned int type)
{
        u8 msgtype;

        if (unlikely(type & PTP_CLASS_V1)) {
                /* msg type is located at the control field for ptp v1 */
                msgtype = hdr->control;
        } else {
                msgtype = hdr->tsmt & 0x0f;
        }

        return msgtype;
}

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0) */

struct rtl8126_private;
struct RxDescV3;

int rtl8126_get_ts_info(struct net_device *netdev,
                        struct ethtool_ts_info *info);

void rtl8126_ptp_reset(struct rtl8126_private *tp);
void rtl8126_ptp_init(struct rtl8126_private *tp);
void rtl8126_ptp_suspend(struct rtl8126_private *tp);
void rtl8126_ptp_stop(struct rtl8126_private *tp);

int rtl8126_ptp_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd);

void rtl8126_rx_ptp_timestamp(struct rtl8126_private *tp, struct sk_buff *skb);

void rtl8126_set_local_time(struct rtl8126_private *tp);

#endif /* _LINUX_R8126_PTP_H */
