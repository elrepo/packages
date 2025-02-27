// SPDX-License-Identifier: GPL-2.0-only
/*
################################################################################
#
# r8168 is the Linux device driver released for Realtek Gigabit Ethernet
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

#include <linux/version.h>
#include "r8168.h"

enum rtl8168_rss_register_content {
        /* RSS */
        RSS_CTRL_TCP_IPV4_SUPP = (1 << 0),
        RSS_CTRL_IPV4_SUPP  = (1 << 1),
        RSS_CTRL_TCP_IPV6_SUPP  = (1 << 2),
        RSS_CTRL_IPV6_SUPP  = (1 << 3),
        RSS_CTRL_IPV6_EXT_SUPP  = (1 << 4),
        RSS_CTRL_TCP_IPV6_EXT_SUPP  = (1 << 5),
        RSS_HALF_SUPP  = (1 << 7),
        RSS_QUAD_CPU_EN  = (1 << 16),
        RSS_HQ_Q_SUP_R  = (1 << 31),
};

static int rtl8168_get_rss_hash_opts(struct rtl8168_private *tp,
                                     struct ethtool_rxnfc *cmd)
{
        cmd->data = 0;

        /* Report default options for RSS */
        switch (cmd->flow_type) {
        case TCP_V4_FLOW:
                cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
                fallthrough;
        case IPV4_FLOW:
                cmd->data |= RXH_IP_SRC | RXH_IP_DST;
                break;
        case TCP_V6_FLOW:
                cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
                fallthrough;
        case IPV6_FLOW:
                cmd->data |= RXH_IP_SRC | RXH_IP_DST;
                break;
        default:
                return -EINVAL;
        }

        return 0;
}

int rtl8168_get_rxnfc(struct net_device *dev, struct ethtool_rxnfc *cmd,
                      u32 *rule_locs)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        int ret = -EOPNOTSUPP;

        if (!(dev->features & NETIF_F_RXHASH))
                return ret;

        switch (cmd->cmd) {
        case ETHTOOL_GRXRINGS:
                cmd->data = rtl8168_tot_rx_rings(tp);
                ret = 0;
                break;
        case ETHTOOL_GRXFH:
                ret = rtl8168_get_rss_hash_opts(tp, cmd);
                break;
        default:
                break;
        }

        return ret;
}

u32 rtl8168_rss_indir_tbl_entries(struct rtl8168_private *tp)
{
        return tp->HwSuppIndirTblEntries;
}

#define RSS_MASK_BITS_OFFSET (8)
static int _rtl8168_set_rss_hash_opt(struct rtl8168_private *tp)
{
        u32 hash_mask_len;
        u32 rss_ctrl;

        /* Perform hash on these packet types */
        rss_ctrl = RSS_CTRL_TCP_IPV4_SUPP
                   | RSS_CTRL_IPV4_SUPP
                   | RSS_CTRL_IPV6_SUPP
                   | RSS_CTRL_IPV6_EXT_SUPP
                   | RSS_CTRL_TCP_IPV6_SUPP
                   | RSS_CTRL_TCP_IPV6_EXT_SUPP;

        if (R8168_MULTI_RSS_4Q(tp))
                rss_ctrl |= RSS_QUAD_CPU_EN;

        hash_mask_len = ilog2(rtl8168_rss_indir_tbl_entries(tp));
        hash_mask_len &= (BIT_0 | BIT_1 | BIT_2);
        rss_ctrl |= hash_mask_len << RSS_MASK_BITS_OFFSET;

        rtl8168_eri_write(tp, RSS_CTRL_8168, 4, rss_ctrl, ERIAR_ExGMAC);

        return 0;
}

static int rtl8168_set_rss_hash_opt(struct rtl8168_private *tp,
                                    struct ethtool_rxnfc *nfc)
{
        u32 rss_flags = tp->rss_flags;

        /*
         * RSS does not support anything other than hashing
         * to queues on src and dst IPs and ports
         */
        if (nfc->data & ~(RXH_IP_SRC | RXH_IP_DST |
                          RXH_L4_B_0_1 | RXH_L4_B_2_3))
                return -EINVAL;

        switch (nfc->flow_type) {
        case TCP_V4_FLOW:
        case TCP_V6_FLOW:
                if (!(nfc->data & RXH_IP_SRC) ||
                    !(nfc->data & RXH_IP_DST) ||
                    !(nfc->data & RXH_L4_B_0_1) ||
                    !(nfc->data & RXH_L4_B_2_3))
                        return -EINVAL;
                break;
        case SCTP_V4_FLOW:
        case AH_ESP_V4_FLOW:
        case AH_V4_FLOW:
        case ESP_V4_FLOW:
        case SCTP_V6_FLOW:
        case AH_ESP_V6_FLOW:
        case AH_V6_FLOW:
        case ESP_V6_FLOW:
        case IP_USER_FLOW:
        case ETHER_FLOW:
                /* RSS is not supported for these protocols */
                if (nfc->data) {
                        netif_err(tp, drv, tp->dev, "Command parameters not supported\n");
                        return -EINVAL;
                }
                return 0;
        default:
                return -EINVAL;
        }

        /* if we changed something we need to update flags */
        if (rss_flags != tp->rss_flags) {
                u32 rss_ctrl = rtl8168_eri_read(tp, RSS_CTRL_8168, 4, ERIAR_ExGMAC);

                tp->rss_flags = rss_flags;

                /* Perform hash on these packet types */
                rss_ctrl |= RSS_CTRL_TCP_IPV4_SUPP
                            | RSS_CTRL_IPV4_SUPP
                            | RSS_CTRL_IPV6_SUPP
                            | RSS_CTRL_IPV6_EXT_SUPP
                            | RSS_CTRL_TCP_IPV6_SUPP
                            | RSS_CTRL_TCP_IPV6_EXT_SUPP;

                if (R8168_MULTI_RSS_4Q(tp))
                        rss_ctrl |= RSS_QUAD_CPU_EN;
                else
                        rss_ctrl &= ~RSS_QUAD_CPU_EN;

                rtl8168_eri_write(tp, RSS_CTRL_8168, 4, rss_ctrl, ERIAR_ExGMAC);
        }

        return 0;
}

int rtl8168_set_rxnfc(struct net_device *dev, struct ethtool_rxnfc *cmd)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        int ret = -EOPNOTSUPP;

        if (!(dev->features & NETIF_F_RXHASH))
                return ret;

        switch (cmd->cmd) {
        case ETHTOOL_SRXFH:
                ret = rtl8168_set_rss_hash_opt(tp, cmd);
                break;
        default:
                break;
        }

        return ret;
}

static u32 _rtl8168_get_rxfh_key_size(struct rtl8168_private *tp)
{
        return sizeof(tp->rss_key);
}

u32 rtl8168_get_rxfh_key_size(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        if (!(dev->features & NETIF_F_RXHASH))
                return 0;

        return _rtl8168_get_rxfh_key_size(tp);
}

u32 rtl8168_rss_indir_size(struct net_device *dev)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        if (!(dev->features & NETIF_F_RXHASH))
                return 0;

        return rtl8168_rss_indir_tbl_entries(tp);
}

static void rtl8168_get_reta(struct rtl8168_private *tp, u32 *indir)
{
        int i, reta_size = rtl8168_rss_indir_tbl_entries(tp);

        for (i = 0; i < reta_size; i++)
                indir[i] = tp->rss_indir_tbl[i];
}

static u32 rtl8168_rss_key_reg(struct rtl8168_private *tp)
{
        return RSS_KEY_8168;
}

static u32 rtl8168_rss_indir_tbl_reg(struct rtl8168_private *tp)
{
        return Rss_indir_tbl;
}

static void rtl8168_store_reta(struct rtl8168_private *tp)
{
        u32 reta_entries = rtl8168_rss_indir_tbl_entries(tp);
        u16 indir_tbl_reg = rtl8168_rss_indir_tbl_reg(tp);
        u32 hw_indir[RTL8168_RSS_INDIR_TBL_SIZE] = {0};
        u8 *indir = tp->rss_indir_tbl;
        u32 bit_on_cnt = 0x00000001;
        u32 i, j;

        /* Mapping redirection table to HW */
        for (i = 0, j = 0; i < reta_entries; i++) {
                if ((indir[i] & 2) && R8168_MULTI_RSS_4Q(tp))
                        hw_indir[j + 4] |= bit_on_cnt;
                if (indir[i] & 1)
                        hw_indir[j] |= bit_on_cnt;

                if (bit_on_cnt == 0x80000000) {
                        bit_on_cnt = 0x00000001;
                        j++;
                        continue;
                }
                bit_on_cnt <<= 1;
        }

        /* Write redirection table to HW */
        for (i = 0; i < RTL8168_RSS_INDIR_TBL_SIZE; i++)
                RTL_W32(tp, indir_tbl_reg + i*4, hw_indir[i]);
}

static void rtl8168_store_rss_key(struct rtl8168_private *tp)
{
        const u16 rss_key_reg = rtl8168_rss_key_reg(tp);
        u32 i, rss_key_size = _rtl8168_get_rxfh_key_size(tp);
        u32 *rss_key = (u32*)tp->rss_key;

        /* Write redirection table to HW */
        for (i = 0; i < rss_key_size; i+=4)
                rtl8168_eri_write(tp, rss_key_reg + i, 4, *rss_key++, ERIAR_ExGMAC);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0)
int rtl8168_get_rxfh(struct net_device *dev, struct ethtool_rxfh_param *rxfh)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        if (!(dev->features & NETIF_F_RXHASH))
                return -EOPNOTSUPP;

        rxfh->hfunc = ETH_RSS_HASH_TOP;

        if (rxfh->indir)
                rtl8168_get_reta(tp, rxfh->indir);

        if (rxfh->key)
                memcpy(rxfh->key, tp->rss_key, RTL8168_RSS_KEY_SIZE);

        return 0;
}

int rtl8168_set_rxfh(struct net_device *dev, struct ethtool_rxfh_param *rxfh,
                     struct netlink_ext_ack *extack)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        u32 reta_entries = rtl8168_rss_indir_tbl_entries(tp);
        int i;

        /* We require at least one supported parameter to be changed and no
         * change in any of the unsupported parameters
         */
        if (rxfh->hfunc != ETH_RSS_HASH_NO_CHANGE && rxfh->hfunc != ETH_RSS_HASH_TOP)
                return -EOPNOTSUPP;

        /* Fill out the redirection table */
        if (rxfh->indir) {
                int max_queues = tp->num_rx_rings;

                /* Verify user input. */
                for (i = 0; i < reta_entries; i++)
                        if (rxfh->indir[i] >= max_queues)
                                return -EINVAL;

                for (i = 0; i < reta_entries; i++)
                        tp->rss_indir_tbl[i] = rxfh->indir[i];
        }

        /* Fill out the rss hash key */
        if (rxfh->key)
                memcpy(tp->rss_key, rxfh->key, RTL8168_RSS_KEY_SIZE);

        rtl8168_store_reta(tp);

        rtl8168_store_rss_key(tp);

        return 0;
}
#else
int rtl8168_get_rxfh(struct net_device *dev, u32 *indir, u8 *key,
                     u8 *hfunc)
{
        struct rtl8168_private *tp = netdev_priv(dev);

        if (!(dev->features & NETIF_F_RXHASH))
                return -EOPNOTSUPP;

        if (hfunc)
                *hfunc = ETH_RSS_HASH_TOP;

        if (indir)
                rtl8168_get_reta(tp, indir);

        if (key)
                memcpy(key, tp->rss_key, RTL8168_RSS_KEY_SIZE);

        return 0;
}

int rtl8168_set_rxfh(struct net_device *dev, const u32 *indir,
                     const u8 *key, const u8 hfunc)
{
        struct rtl8168_private *tp = netdev_priv(dev);
        u32 reta_entries = rtl8168_rss_indir_tbl_entries(tp);
        int i;

        /* We require at least one supported parameter to be changed and no
         * change in any of the unsupported parameters
         */
        if (hfunc != ETH_RSS_HASH_NO_CHANGE && hfunc != ETH_RSS_HASH_TOP)
                return -EOPNOTSUPP;

        /* Fill out the redirection table */
        if (indir) {
                int max_queues = tp->num_rx_rings;

                /* Verify user input. */
                for (i = 0; i < reta_entries; i++)
                        if (indir[i] >= max_queues)
                                return -EINVAL;

                for (i = 0; i < reta_entries; i++)
                        tp->rss_indir_tbl[i] = indir[i];
        }

        /* Fill out the rss hash key */
        if (key)
                memcpy(tp->rss_key, key, RTL8168_RSS_KEY_SIZE);

        rtl8168_store_reta(tp);

        rtl8168_store_rss_key(tp);

        return 0;
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,8,0) */

static u32 rtl8168_get_rx_desc_hash(struct rtl8168_private *tp,
                                    struct RxDescV2 *desc)
{
        if (!desc->RSSResult)
                fsleep(1);
        return le32_to_cpu(desc->RSSResult);
}

#define RXS_8168_RSS_IPV4 BIT(17)
#define RXS_8168_RSS_IPV6 BIT(18)
#define RXS_8168_RSS_TCP BIT(19)
#define RTL8168_RXS_RSS_L3_TYPE_MASK (RXS_8168_RSS_IPV4 | RXS_8168_RSS_IPV6)
#define RTL8168_RXS_RSS_L4_TYPE_MASK (RXS_8168_RSS_TCP)
void rtl8168_rx_hash(struct rtl8168_private *tp,
                     struct RxDescV2 *desc,
                     struct sk_buff *skb)
{
        u32 rss_header_info;

        if (!(tp->dev->features & NETIF_F_RXHASH))
                return;

        rss_header_info = le32_to_cpu(desc->opts2);

        if (!(rss_header_info & RTL8168_RXS_RSS_L3_TYPE_MASK))
                return;

        skb_set_hash(skb, rtl8168_get_rx_desc_hash(tp, desc),
                     (RTL8168_RXS_RSS_L4_TYPE_MASK & rss_header_info) ?
                     PKT_HASH_TYPE_L4 : PKT_HASH_TYPE_L3);
}

void rtl8168_disable_rss(struct rtl8168_private *tp)
{
        rtl8168_eri_write(tp, RSS_CTRL_8168, 4, 0x00000000, ERIAR_ExGMAC);
}

void _rtl8168_config_rss(struct rtl8168_private *tp)
{
        _rtl8168_set_rss_hash_opt(tp);

        rtl8168_store_reta(tp);

        rtl8168_store_rss_key(tp);
}

void rtl8168_config_rss(struct rtl8168_private *tp)
{
        if (!HW_RSS_SUPPORT_RSS(tp))
                return;

        if (!tp->EnableRss) {
                rtl8168_disable_rss(tp);
                return;
        }

        _rtl8168_config_rss(tp);
}

void rtl8168_init_rss(struct rtl8168_private *tp)
{
        int i;

        for (i = 0; i < rtl8168_rss_indir_tbl_entries(tp); i++)
                tp->rss_indir_tbl[i] = ethtool_rxfh_indir_default(i, tp->num_rx_rings);

        netdev_rss_key_fill(tp->rss_key, RTL8168_RSS_KEY_SIZE);
}
