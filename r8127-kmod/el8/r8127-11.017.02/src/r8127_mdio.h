/* SPDX-License-Identifier: GPL-2.0-only */
/*
################################################################################
#
# r8127 is the Linux device driver released for Realtek 10 Gigabit Ethernet
# controllers with PCI-Express interface.
#
# Copyright(c) 2026 Realtek Semiconductor Corp. All rights reserved.
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

#ifndef _LINUX_R8127_MDIO_H
#define _LINUX_R8127_MDIO_H

#ifndef MDIO_CTRL1_SPEED2_5G
#define MDIO_CTRL1_SPEED2_5G (MDIO_CTRL1_SPEEDSELEXT | 0x18)
#endif
#ifndef MDIO_CTRL1_SPEED5G
#define MDIO_CTRL1_SPEED5G (MDIO_CTRL1_SPEEDSELEXT | 0x1c)
#endif
#ifndef MDIO_AN_10GBT_CTRL_ADV2_5G
#define MDIO_AN_10GBT_CTRL_ADV2_5G 0x0080
#endif
#ifndef MDIO_AN_10GBT_CTRL_ADV5G
#define MDIO_AN_10GBT_CTRL_ADV5G 0x0100
#endif
#ifndef MDIO_AN_10GBT_STAT_LP2_5G
#define MDIO_AN_10GBT_STAT_LP2_5G 0x0020
#endif
#ifndef MDIO_AN_10GBT_STAT_LP5G
#define MDIO_AN_10GBT_STAT_LP5G 0x0040
#endif
#ifndef MASTER_SLAVE_STATE_UNSUPPORTED
#define MASTER_SLAVE_STATE_UNSUPPORTED 0
#define MASTER_SLAVE_STATE_UNKNOWN 1
#define MASTER_SLAVE_STATE_MASTER 2
#define MASTER_SLAVE_STATE_SLAVE 3
#define MASTER_SLAVE_STATE_ERR 4
#endif

#define GPIO_OUT_EN 0xDC4C
#define GPIO_OUT_VAL 0xDC52
#define GPIO_IN_VAL 0xDC58

#define MDC_bit (1 << 7)
#define MDIO_bit (1 << 2)

#define RTL8127_MDIO_CLK_P 3
#define RTL8127_MDIO_PREAMBLE_BITS 32

#define RTL8127_MDIO_C45_ST 0b00
#define RTL8127_MDIO_C45_OP_ADDR 0b00
#define RTL8127_MDIO_C45_OP_WRITE 0b01
#define RTL8127_MDIO_C45_OP_READ 0b11
#define RTL8127_MDIO_C45_TA_ADDR 0b10
#define RTL8127_MDIO_C45_TA_WRITE 0b10

#define RTL_PHYID1 0x001C
#define RTL_PHYADDR 0

enum {
        MDIO_PHY_NONE = 0,
        MDIO_PHY_RTL8261D,
        MDIO_PHY_MAX
};

enum rtl8261_phy_registers {
        SPHY_SET_LINK = 0xA400,
        SPHY_AN_GIGA = 0xA412,
        SPHY_LP_GIGA = 0xA414,
        SPHY_LINK_SPEED = 0xA434,
};

enum rtl8261_phy_content {
        // SPHY_LINK_SPEED
        SPHY_LINK_UP = (1 << 2),
};

struct rtl8261_phy_private {
        struct mutex lock;

        struct rtl8261d_macsec_private *macsec;
        int HwIsXgMode;
        u32 HwMacsecSerDesSfpCap;
        u8 ethernetAutoneg;
        u32 ethernetSpeed;
        u8 ethernetDuplex;
        u64 ethernetAdvertising;

        struct rtl8127_private *tp;
};

int rtl8261_read_phy_ocp(struct phy_device *phydev, int mmd, u32 ocp_addr);
int rtl8261_write_phy_ocp(struct phy_device *phydev, int mmd, u32 ocp_addr, u16 val);
int rtl8261_modify_phy_ocp(struct phy_device *phydev, int mmd, u32 ocp_addr,
                           u16 mask, u16 set);

unsigned int rtl8127_mdio_link_ok(struct rtl8127_private *tp);
int rtl8127_mdio_set_sds_speed(struct rtl8127_private *tp, u32 speed);
int rtl8127_mdio_set_eth_speed(struct rtl8127_private *tp, u8 autoneg,
                               u32 speed, u8 duplex, u64 adv);
int rtl8127_mdio_config_eth_speed(struct rtl8127_private *tp);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)
int rtl8127_mdio_get_eth_speed(struct rtl8127_private *tp, struct ethtool_cmd *cmd);
#else
int rtl8127_mdio_get_eth_speed(struct rtl8127_private *tp, struct ethtool_link_ksettings *cmd);
#endif
int rtl8127_mdio_set_wol_eth_speed(struct rtl8127_private *tp);
int rtl8127_mdio_nway_reset(struct rtl8127_private *tp);
int rtl8127_mdio_power_up(struct rtl8127_private *tp, bool enable);

int rtl8127_mdio_init(struct net_device *netdev);
void rtl8127_mdio_remove(struct net_device *netdev);

#endif /* _LINUX_R8127_MDIO_H */
