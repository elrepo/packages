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

#include <linux/phy.h>

#include "r8127.h"
#include "r8127_mdio.h"
#include "r8127_macsec.h"

static void rtl8127_set_mdio_dir(struct rtl8127_private *tp, int output)
{
        if (output)
                tp->val_GPIO_OUT_EN |= MDIO_bit;
        else
                tp->val_GPIO_OUT_EN &= ~MDIO_bit;

        rtl8127_mac_ocp_write(tp, GPIO_OUT_EN, tp->val_GPIO_OUT_EN);
}

static void rtl8127_mdc(struct rtl8127_private *tp, int mdc_high)
{
        if (mdc_high)
                tp->val_GPIO_OUT_VAL |= MDC_bit;
        else
                tp->val_GPIO_OUT_VAL &= ~MDC_bit;

        rtl8127_mac_ocp_write(tp, GPIO_OUT_VAL, tp->val_GPIO_OUT_VAL);
}

static void rtl8127_mdc_mdio(struct rtl8127_private *tp, int mdc_high, int mdio_high)
{
        if (mdc_high)
                tp->val_GPIO_OUT_VAL |= MDC_bit;
        else
                tp->val_GPIO_OUT_VAL &= ~MDC_bit;

        if (mdio_high)
                tp->val_GPIO_OUT_VAL |= MDIO_bit;
        else
                tp->val_GPIO_OUT_VAL &= ~MDIO_bit;

        rtl8127_mac_ocp_write(tp, GPIO_OUT_VAL, tp->val_GPIO_OUT_VAL);
}

static void rtl8127_send_mdio_bit(struct rtl8127_private *tp, int level)
{
        int i;

        rtl8127_mdc_mdio(tp, 0, level);

        for (i = 0; i < RTL8127_MDIO_CLK_P; i++) {
                rtl8127_mdc_mdio(tp, 1, level);
        }
}

static u16 rtl8127_get_mdio_bit(struct rtl8127_private *tp)
{
        u16 val;

        rtl8127_mdc(tp, 1);
        val = (rtl8127_mac_ocp_read(tp, GPIO_IN_VAL) >> 2) & 0x1;
        rtl8127_mdc(tp, 0);

        return val;
}

static void rtl8127_mdio_send_preamble(struct rtl8127_private *tp)
{
        int i;

        for (i = 0; i < RTL8127_MDIO_PREAMBLE_BITS; i++)
                rtl8127_send_mdio_bit(tp, 1);
}

static void rtl8127_mdio_send_num(struct rtl8127_private *tp, u16 val, int bits)
{
        int i;

        for (i = bits; i > 0; i--)
                rtl8127_send_mdio_bit(tp, (val >> (i - 1)) & 0x1);
}

static void rtl8127_mdio_cmd(struct rtl8127_private *tp, u16 op, u8 phy_addr, u8 mmd)
{
        rtl8127_mdio_send_preamble(tp);

        rtl8127_mdio_send_num(tp, RTL8127_MDIO_C45_ST, 2);

        rtl8127_mdio_send_num(tp, op, 2);

        rtl8127_mdio_send_num(tp, phy_addr, 5);

        rtl8127_mdio_send_num(tp, mmd, 5);
}

static void rtl8127_mdio_cmdAddr(struct rtl8127_private *tp, u16 op,
                                 int phy_addr, int devad, int reg)
{
        rtl8127_set_mdio_dir(tp, 1);

        rtl8127_mdio_cmd(tp, op, phy_addr, devad);

        rtl8127_mdio_send_num(tp, RTL8127_MDIO_C45_TA_ADDR, 2);

        rtl8127_mdio_send_num(tp, reg, 16);
}

static u16 rtl8127_mdio_read_common(struct rtl8127_private *tp)
{
        u16 data = 0;
        int i;

        rtl8127_mdc(tp, 0);
        rtl8127_mdc(tp, 1);

        rtl8127_mdc_mdio(tp, 1, 0);
        rtl8127_mdc_mdio(tp, 0, 0);

        rtl8127_set_mdio_dir(tp, 0);

        for (i = 0; i < 16; i++) {
                data <<= 1;
                data |= rtl8127_get_mdio_bit(tp);
        }

        rtl8127_mdc_mdio(tp, 0, 1);
        rtl8127_set_mdio_dir(tp, 1);

        return data;
}

static u16 rtl8127_mdio_write_common(struct rtl8127_private *tp, u16 data)
{
        rtl8127_mdio_send_num(tp, RTL8127_MDIO_C45_TA_WRITE, 2);

        rtl8127_mdio_send_num(tp, data, 16);

        rtl8127_mdc_mdio(tp, 0, 1);

        return data;
}

static u16 rtl8127_mdio_read_gpio_c45(struct rtl8127_private *tp, int phy_addr, int devad, int reg)
{
        u16 val;

        rtl8127_mdio_cmdAddr(tp, RTL8127_MDIO_C45_OP_ADDR, phy_addr, devad, reg);

        rtl8127_mdio_cmd(tp, RTL8127_MDIO_C45_OP_READ, phy_addr, devad);
        val = rtl8127_mdio_read_common(tp);

        return val;
}

static void rtl8127_mdio_write_gpio_c45(struct rtl8127_private *tp, int phy_addr, int devad,
                                        int reg, u16 data)
{
        rtl8127_mdio_cmdAddr(tp, RTL8127_MDIO_C45_OP_ADDR, phy_addr, devad, reg);
        rtl8127_mdio_cmd(tp, RTL8127_MDIO_C45_OP_WRITE, phy_addr, devad);

        rtl8127_mdio_write_common(tp, data);
}

static void rtl8127_mdio_gpio_init(struct rtl8127_private *tp)
{
        u16 gpio_val;

        gpio_val = rtl8127_mac_ocp_read(tp, GPIO_OUT_EN);
        gpio_val |= (MDC_bit | MDIO_bit);
        tp->val_GPIO_OUT_EN = gpio_val;
        rtl8127_mac_ocp_write(tp, GPIO_OUT_EN, gpio_val);

        gpio_val = rtl8127_mac_ocp_read(tp, GPIO_OUT_VAL);
        gpio_val &= ~MDC_bit;
        gpio_val |= MDIO_bit;
        tp->val_GPIO_OUT_VAL = gpio_val;
        rtl8127_mac_ocp_write(tp, GPIO_OUT_VAL, gpio_val);

        gpio_val = rtl8127_mac_ocp_read(tp, 0xDC5E);
        gpio_val |= (MDC_bit | MDIO_bit);
        rtl8127_mac_ocp_write(tp, 0xDC5E, gpio_val);

        gpio_val = rtl8127_mac_ocp_read(tp, 0xDC46);
        gpio_val |= (MDC_bit | MDIO_bit);
        rtl8127_mac_ocp_write(tp, 0xDC46, gpio_val);
}

static void rtl8127_mdio_gpio_remove(struct rtl8127_private *tp)
{
        u16 gpio_val;

        gpio_val = rtl8127_mac_ocp_read(tp, 0xDC5E);
        gpio_val &= ~(MDC_bit | MDIO_bit);
        rtl8127_mac_ocp_write(tp, 0xDC5E, gpio_val);

        gpio_val = rtl8127_mac_ocp_read(tp, 0xDC46);
        gpio_val &= ~(MDC_bit | MDIO_bit);
        rtl8127_mac_ocp_write(tp, 0xDC46, gpio_val);
}

/**
 * return value < 0 -> fail
 * return value >= 0 -> low 16 bit is data
 */
int rtl8261_read_phy_ocp(struct phy_device *phydev, int mmd, u32 ocp_addr)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8127_private *tp = phytp->tp;
        int val;

        val = rtl8127_mdio_read_gpio_c45(tp, RTL_PHYADDR, mmd, ocp_addr);
        if (val == 0xFFFF)
                return -EIO;

        return val;
}

/**
 * return value < 0 -> fail
 * return value == 0 -> success
 */
int rtl8261_write_phy_ocp(struct phy_device *phydev, int mmd, u32 ocp_addr, u16 val)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8127_private *tp = phytp->tp;

        rtl8127_mdio_write_gpio_c45(tp, RTL_PHYADDR, mmd, ocp_addr, val);

        return 0;
}

/**
 * return value < 0 -> fail
 * return value == 0 -> success
 */
int rtl8261_modify_phy_ocp(struct phy_device *phydev, int mmd,
                           u32 ocp_addr, u16 mask, u16 set)
{
        int ret, val;
        u16 data;

        val = rtl8261_read_phy_ocp(phydev, mmd, ocp_addr);
        if (val < 0)
                return val;

        data = (u16)val;
        data &= ~mask;
        data |= set;

        ret = rtl8261_write_phy_ocp(phydev, mmd, ocp_addr, data);
        if (ret < 0)
                return ret;

        return 0;
}

static void rtl8261_set_link_option(struct phy_device *phydev)
{
        struct rtl8261_phy_private *phytp = phydev->priv;

        phytp->ethernetAutoneg = AUTONEG_ENABLE;
        phytp->ethernetSpeed = SPEED_10000;
        phytp->ethernetDuplex = DUPLEX_FULL;
        phytp->ethernetAdvertising = (ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full |
                                      ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full |
                                      ADVERTISED_1000baseT_Full | ADVERTISED_2500baseX_Full |
                                      RTK_ADVERTISED_5000baseX_Full | ADVERTISED_10000baseT_Full);

        phytp->HwMacsecSerDesSfpCap = SPEED_UNKNOWN;
}

static void rtl8261_set_wol_option(struct phy_device *phydev)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8127_private *tp = phytp->tp;

        tp->HwSuppLinkChgWakeUpVer = 0;
        tp->wol_opts &= ~WAKE_PHY;
        tp->wol_enabled = (tp->wol_opts) ? WOL_ENABLED : WOL_DISABLED;
}

static void rtl8261_init_software_variables(struct phy_device *phydev)
{
        rtl8261_set_link_option(phydev);
        rtl8261_set_wol_option(phydev);
}

static int rtl8261_link_ok(struct phy_device *phydev)
{
        int status;

        status = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, SPHY_LINK_SPEED);
        if (status < 0)
                return status;

        return (status & SPHY_LINK_UP) ? 1 : 0;
}

static int rtl8261_set_sds_speed(struct phy_device *phydev, u32 speed)
{
        u16 val;
        int ret;

        switch(speed) {
        case SPEED_10000:
                val = (2 << 8) | 2;
                break;
        case SPEED_1000:
                val = (8 << 8) | 8;
                break;
        default:
                return -EINVAL;
        }

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6972, 0x0F0F, val);
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6973, 0x0F0F, val);
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6974, 0x0F0F, val);
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6975, 0x0F0F, val);
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6976, 0x0F0F, val);
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6977, 0x0F0F, val);
        if (ret < 0)
                return ret;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_AN, MDIO_CTRL1,
                                     MDIO_AN_CTRL1_ENABLE | MDIO_AN_CTRL1_RESTART,
                                     MDIO_AN_CTRL1_ENABLE | MDIO_AN_CTRL1_RESTART);
        if (ret < 0)
                return ret;

        return 0;
}

static bool rtl8261_eth_is_speed_mode_valid(u32 speed)
{
        switch(speed) {
        case SPEED_10000:
        case SPEED_5000:
        case SPEED_2500:
        case SPEED_1000:
        case SPEED_100:
        case SPEED_10:
                return true;
        default:
                return false;
        }
}

static int rtl8261_set_eth_speed(struct phy_device *phydev, u8 autoneg,
                                 u32 speed, u8 duplex, u64 adv)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8127_private *tp = phytp->tp;
        int auto_nego;
        int giga_ctrl;
        int ctrl_2500;
        u16 speed_force;
        int val, ret = 0;

        if (!rtl8261_eth_is_speed_mode_valid(speed)) {
                speed = SPEED_10000;
                duplex = DUPLEX_FULL;
                adv |= phytp->ethernetAdvertising;
        }

        if (autoneg == AUTONEG_ENABLE) {
                /* n-way force */
                val = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, SPHY_AN_GIGA);
                if (val < 0)
                        return val;
                giga_ctrl = (u32)val & ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL);

                val = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL);
                if (val < 0)
                        return val;
                ctrl_2500 = (u32)val & ~(MDIO_AN_10GBT_CTRL_ADV2_5G | MDIO_AN_10GBT_CTRL_ADV5G |
                                         MDIO_AN_10GBT_CTRL_ADV10G);
                val = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
                if (val < 0)
                        return val;
                auto_nego = (u32)val & ~(ADVERTISE_10HALF | ADVERTISE_10FULL |
                                         ADVERTISE_100HALF | ADVERTISE_100FULL |
                                         ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);

                if (adv & ADVERTISED_10baseT_Half)
                        auto_nego |= ADVERTISE_10HALF;
                if (adv & ADVERTISED_10baseT_Full)
                        auto_nego |= ADVERTISE_10FULL;
                if (adv & ADVERTISED_100baseT_Half)
                        auto_nego |= ADVERTISE_100HALF;
                if (adv & ADVERTISED_100baseT_Full)
                        auto_nego |= ADVERTISE_100FULL;

                if (adv & ADVERTISED_1000baseT_Full)
                        giga_ctrl |= ADVERTISE_1000FULL;

                if (adv & ADVERTISED_2500baseX_Full)
                        ctrl_2500 |= MDIO_AN_10GBT_CTRL_ADV2_5G;
                if (adv & RTK_ADVERTISED_5000baseX_Full)
                        ctrl_2500 |= MDIO_AN_10GBT_CTRL_ADV5G;
                if (adv & ADVERTISED_10000baseT_Full)
                        ctrl_2500 |= MDIO_AN_10GBT_CTRL_ADV10G;

                if (tp->fcpause == rtl8127_fc_full)
                        auto_nego |= ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;

                ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE, auto_nego);
                if (ret < 0)
                        return ret;
                ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_VEND2, SPHY_AN_GIGA, giga_ctrl);
                if (ret < 0)
                        return ret;
                ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL, ctrl_2500);
                if (ret < 0)
                        return ret;

                ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_AN, MDIO_CTRL1,
                                             MDIO_AN_CTRL1_ENABLE | MDIO_AN_CTRL1_RESTART,
                                             MDIO_AN_CTRL1_ENABLE | MDIO_AN_CTRL1_RESTART);
                if (ret < 0)
                        return ret;
        } else {
                /* true force */
                if (speed >= SPEED_1000 && duplex == DUPLEX_HALF) {
                        pr_err(KBUILD_MODNAME " Failed to set force speed %u duplex %u\n",
                               speed, duplex);
                        return -EINVAL;
                }

                switch (speed) {
                case SPEED_10:
                        speed_force = 0;
                        break;
                case SPEED_100:
                        speed_force = MDIO_PMA_CTRL1_SPEED100;
                        break;
                case SPEED_1000:
                        speed_force = MDIO_PMA_CTRL1_SPEED1000;
                        break;
                case SPEED_2500:
                        speed_force = MDIO_CTRL1_SPEED2_5G;
                        break;
                case SPEED_5000:
                        speed_force = MDIO_CTRL1_SPEED5G;
                        break;
                case SPEED_10000:
                        speed_force = MDIO_CTRL1_SPEED10G;
                        break;
                default:
                        pr_err(KBUILD_MODNAME ": Failed to set force speed %u\n", speed);
                        return -EINVAL;
                }

                ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND2, 0xA5B4, BIT_15, BIT_15);
                if (ret < 0)
                        return ret;

                ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1,
                                             MDIO_CTRL1_SPEEDSEL, speed_force);
                if (ret < 0)
                        return ret;

                ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND2, SPHY_SET_LINK,
                                             MDIO_CTRL1_FULLDPLX,
                                             (duplex == DUPLEX_FULL) ? MDIO_CTRL1_FULLDPLX : 0);
                if (ret < 0)
                        return ret;

                ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_AN, MDIO_CTRL1,
                                             MDIO_AN_CTRL1_ENABLE, 0);
                if (ret < 0)
                        return ret;
                ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_AN, MDIO_CTRL1,
                                             MDIO_AN_CTRL1_RESTART, MDIO_AN_CTRL1_RESTART);
                if (ret < 0)
                        return ret;
        }

        phytp->ethernetAutoneg = autoneg;
        phytp->ethernetSpeed = speed;
        phytp->ethernetDuplex = duplex;
        phytp->ethernetAdvertising = adv;

        return 0;
}

static u32 rtl8261_convert_link_speed(u32 status)
{
        u32 speed = SPEED_UNKNOWN;
        u32 speed_h, speed_l;

        speed_h = (status & ((1 << 10) | (1 << 9)));
        speed_l = (status & ((1 << 5) | (1 << 4)));

        switch (speed_h) {
        case 0x0000:
                switch (speed_l) {
                case 0x0000:
                        speed = SPEED_10;
                        break;
                case 0x0010:
                        speed = SPEED_100;
                        break;
                case 0x0020:
                        speed = SPEED_1000;
                        break;
                }
                break;
        case 0x0200:
                switch (speed_l) {
                case 0x0000:
                        speed = SPEED_10000;
                        break;
                case 0x0010:
                        speed = SPEED_2500;
                        break;
                case 0x0020:
                        speed = SPEED_5000;
                        break;
                case 0x0030:
                        speed = SPEED_1000;
                        break;
                }
                break;
        case 0x0400:
                switch (speed_l) {
                case 0x0000:
                        speed = SPEED_2500;
                        break;
                case 0x0010:
                        speed = SPEED_5000;
                        break;
                }
                break;
        }

        return speed;
}

static u8 rtl8261_get_mdi_status(struct phy_device *phydev)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8127_private *tp = phytp->tp;
        int link_status;

        if (!(tp->link_ok(tp->dev) && rtl8261_link_ok(phydev)))
                return ETH_TP_MDI_INVALID;

        link_status = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, 0xA434);
        if (link_status < 0)
                return ETH_TP_MDI_INVALID;

        return (link_status & BIT_1) ? ETH_TP_MDI : ETH_TP_MDI_X;
}

static int rtl8261_get_eth_speed(struct phy_device *phydev,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)
                                 struct ethtool_cmd *cmd
#else
                                 struct ethtool_link_ksettings *cmd
#endif
                                )
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8127_private *tp = phytp->tp;
        u32 autoneg, speed, duplex;
        u64 supported, advertising;
        u64 lpa_adv = 0;
        u8 report_lpa = 0;
        u8 master_slave_state;
        int link_status;
        int ctrl1, auto_nego, ansr, anlpar, gbsr, status_2500;

        supported = SUPPORTED_10baseT_Half |
                    SUPPORTED_10baseT_Full |
                    SUPPORTED_100baseT_Half |
                    SUPPORTED_100baseT_Full |
                    SUPPORTED_1000baseT_Full |
                    SUPPORTED_2500baseX_Full |
                    RTK_SUPPORTED_5000baseX_Full |
                    SUPPORTED_10000baseT_Full |
                    SUPPORTED_Autoneg |
                    SUPPORTED_TP |
                    SUPPORTED_Pause |
                    SUPPORTED_Asym_Pause;

        advertising = phytp->ethernetAdvertising;

        ctrl1 = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN, MDIO_CTRL1);
        if (ctrl1 < 0)
                return ctrl1;

        if ((supported & SUPPORTED_Autoneg) && (ctrl1 & MDIO_AN_CTRL1_ENABLE))
                autoneg = AUTONEG_ENABLE;
        else
                autoneg = AUTONEG_DISABLE;

        if (autoneg == AUTONEG_ENABLE)
                advertising |= ADVERTISED_Autoneg;

        advertising |= ADVERTISED_TP;

        if (netif_running(tp->dev) && (tp->link_ok(tp->dev) && rtl8261_link_ok(phydev)))
                report_lpa = 1;

        if (report_lpa) {
                /* link on */
                link_status = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, SPHY_LINK_SPEED);
                if (link_status < 0)
                        return link_status;

                speed = rtl8261_convert_link_speed(link_status);

                duplex = (link_status & (1 << 3)) ? DUPLEX_FULL : DUPLEX_HALF;

                auto_nego = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
                if (auto_nego < 0)
                        return auto_nego;

                if (auto_nego & ADVERTISE_PAUSE_ASYM)
                        advertising |= ADVERTISED_Asym_Pause;

                if (auto_nego & ADVERTISE_PAUSE_CAP)
                        advertising |= ADVERTISED_Pause;

                anlpar = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN, MDIO_AN_LPA);
                if (anlpar < 0)
                        return anlpar;
                ansr = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN, MDIO_STAT1);
                if (ansr < 0)
                        return ansr;
                gbsr = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, SPHY_LP_GIGA);
                if (gbsr < 0)
                        return gbsr;
                status_2500 = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_STAT);
                if (status_2500 < 0)
                        return status_2500;

                if (ansr & MDIO_AN_STAT1_LPABLE)
                        lpa_adv |= ADVERTISED_Autoneg;

                if (anlpar & LPA_10HALF)
                        lpa_adv |= ADVERTISED_10baseT_Half;
                if (anlpar & LPA_10FULL)
                        lpa_adv |= ADVERTISED_10baseT_Full;
                if (anlpar & LPA_100HALF)
                        lpa_adv |= ADVERTISED_100baseT_Half;
                if (anlpar & LPA_100FULL)
                        lpa_adv |= ADVERTISED_100baseT_Full;
                if (anlpar & LPA_PAUSE_CAP)
                        lpa_adv |= ADVERTISED_Pause;
                if (anlpar & LPA_PAUSE_ASYM)
                        lpa_adv |= ADVERTISED_Asym_Pause;

                if (gbsr & LPA_1000HALF)
                        lpa_adv |= ADVERTISED_1000baseT_Half;
                if (gbsr & LPA_1000FULL)
                        lpa_adv |= ADVERTISED_1000baseT_Full;

                if (status_2500 & MDIO_AN_10GBT_STAT_LP2_5G)
                        lpa_adv |= ADVERTISED_2500baseX_Full;
                if (status_2500 & MDIO_AN_10GBT_STAT_LP5G)
                        lpa_adv |= RTK_ADVERTISED_5000baseX_Full;
                if (status_2500 & MDIO_AN_10GBT_STAT_LP10G)
                        lpa_adv |= ADVERTISED_10000baseT_Full;

                if (status_2500 & MDIO_AN_10GBT_STAT_MSFLT)
                        master_slave_state = MASTER_SLAVE_STATE_ERR;
                else if (status_2500 & MDIO_AN_10GBT_STAT_MS)
                        master_slave_state =  MASTER_SLAVE_STATE_MASTER;
                else
                        master_slave_state =  MASTER_SLAVE_STATE_SLAVE;
        } else {
                /*link down*/
                speed = SPEED_UNKNOWN;
                duplex = DUPLEX_UNKNOWN;
                lpa_adv = 0;
                master_slave_state = MASTER_SLAVE_STATE_UNKNOWN;
        }

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)
        cmd->supported = (u32)supported;
        cmd->advertising = (u32)advertising;
        cmd->autoneg = autoneg;
        cmd->speed = speed;
        cmd->duplex = duplex;
        cmd->port = PORT_TP;
        cmd->lp_advertising = (u32)lpa_adv;
        cmd->eth_tp_mdix = rtl8261_get_mdi_status(phydev);
        cmd->transceiver = XCVR_EXTERNAL;
        cmd->phy_address = RTL_PHYADDR;
        // cmd->mdio_support = ETH_MDIO_SUPPORTS_C45;
        // cmd->rate_matching = RATE_MATCH_NONE;
#else
        ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.supported,
                                                supported);
        ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.advertising,
                                                advertising);
        ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.lp_advertising,
                                                lpa_adv);

        if (supported & SUPPORTED_2500baseX_Full) {
                linkmode_mod_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
                                 cmd->link_modes.supported, 1);
        }
        if (advertising & ADVERTISED_2500baseX_Full) {
                linkmode_mod_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
                                 cmd->link_modes.advertising, 1);
        }
        if (supported & RTK_SUPPORTED_5000baseX_Full) {
                linkmode_mod_bit(ETHTOOL_LINK_MODE_5000baseT_Full_BIT,
                                 cmd->link_modes.supported, 1);
        }
        if (advertising & RTK_ADVERTISED_5000baseX_Full) {
                linkmode_mod_bit(ETHTOOL_LINK_MODE_5000baseT_Full_BIT,
                                 cmd->link_modes.advertising, 1);
        }
        if (supported & SUPPORTED_10000baseT_Full) {
                linkmode_mod_bit(ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
                                 cmd->link_modes.supported, 1);
        }
        if (advertising & ADVERTISED_10000baseT_Full) {
                linkmode_mod_bit(ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
                                 cmd->link_modes.advertising, 1);
        }
        if (report_lpa) {
                if (lpa_adv & ADVERTISED_2500baseX_Full) {
                        linkmode_mod_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
                                         cmd->link_modes.lp_advertising, 1);
                }
                if (lpa_adv & RTK_ADVERTISED_5000baseX_Full)
                        linkmode_mod_bit(ETHTOOL_LINK_MODE_5000baseT_Full_BIT,
                                         cmd->link_modes.lp_advertising, 1);
                if (lpa_adv & ADVERTISED_10000baseT_Full)
                        linkmode_mod_bit(ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
                                         cmd->link_modes.lp_advertising, 1);
        }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
        /* Use ETHTOOL_LINK_MODE_2500baseT_Full_BIT instead of
           ETHTOOL_LINK_MODE_2500baseX_Full_BIT. */
        linkmode_mod_bit(ETHTOOL_LINK_MODE_2500baseX_Full_BIT,
                         cmd->link_modes.supported, 0);

        linkmode_mod_bit(ETHTOOL_LINK_MODE_2500baseX_Full_BIT,
                         cmd->link_modes.advertising, 0);

        linkmode_mod_bit(ETHTOOL_LINK_MODE_2500baseX_Full_BIT,
                         cmd->link_modes.lp_advertising, 0);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0) */

        cmd->base.autoneg = autoneg;
        cmd->base.speed = speed;
        cmd->base.duplex = duplex;
        cmd->base.port = PORT_TP;
        cmd->base.eth_tp_mdix = rtl8261_get_mdi_status(phydev);
        cmd->base.transceiver = XCVR_EXTERNAL;
        cmd->base.phy_address = RTL_PHYADDR;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0)
        cmd->base.master_slave_state = master_slave_state;
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0) */
        // cmd->base.mdio_support = ETH_MDIO_SUPPORTS_C45;
        // cmd->base.rate_matching = RATE_MATCH_NONE;
#endif

        return 0;
}

static int rtl8261_set_wol_eth_speed(struct phy_device *phydev)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        int auto_nego, giga_ctrl, ctrl_2500;
        int ansr = 0, anlpar = 0, gbsr = 0, status_2500 = 0;
        u64 adv;
        int val, ret;

        if (phytp->ethernetAutoneg != AUTONEG_ENABLE)
                return 0;

        val = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
        if (val < 0)
                return val;
        auto_nego = (u32)val & ~(ADVERTISE_10HALF | ADVERTISE_10FULL |
                                 ADVERTISE_100HALF | ADVERTISE_100FULL);

        val = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, SPHY_AN_GIGA);
        if (val < 0)
                return val;
        giga_ctrl = (u32)val & ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL);

        val = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL);
        if (val < 0)
                return val;
        ctrl_2500 = (u32)val & ~(MDIO_AN_10GBT_CTRL_ADV2_5G | MDIO_AN_10GBT_CTRL_ADV5G |
                                 MDIO_AN_10GBT_CTRL_ADV10G);

        if (rtl8261_link_ok(phydev)) {
                ansr = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN, MDIO_STAT1);
                if (ansr < 0)
                        return ansr;
                anlpar = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN, MDIO_AN_LPA);
                if (anlpar < 0)
                        return anlpar;
                gbsr = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, SPHY_LP_GIGA);
                if (gbsr < 0)
                        return gbsr;
                status_2500 = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN,
                                                   MDIO_AN_10GBT_STAT);
                if (status_2500 < 0)
                        return status_2500;
        }

        adv = phytp->ethernetAdvertising;
        if ((ansr | anlpar | gbsr | status_2500) == 0) {
                int auto_nego_tmp = 0;
                if (adv & ADVERTISED_10baseT_Half)
                        auto_nego_tmp |= ADVERTISE_10HALF;
                if (adv & ADVERTISED_10baseT_Full)
                        auto_nego_tmp |= ADVERTISE_10FULL;
                if (adv & ADVERTISED_100baseT_Half)
                        auto_nego_tmp |= ADVERTISE_100HALF;
                if (adv & ADVERTISED_100baseT_Full)
                        auto_nego_tmp |= ADVERTISE_100FULL;

                if (auto_nego_tmp == 0)
                        return 0;

                auto_nego |= auto_nego_tmp;
                goto skip_check_lpa;
        }

        if (!(ansr & MDIO_AN_STAT1_LPABLE))
                return 0;

        if ((adv & ADVERTISED_10baseT_Half) && (anlpar & LPA_10HALF))
                auto_nego |= ADVERTISE_10HALF;
        else if ((adv & ADVERTISED_10baseT_Full) && (anlpar & LPA_10FULL))
                auto_nego |= ADVERTISE_10FULL;
        else if ((adv & ADVERTISED_100baseT_Half) && (anlpar & LPA_100HALF))
                auto_nego |= ADVERTISE_100HALF;
        else if ((adv & ADVERTISED_100baseT_Full) && (anlpar & LPA_100FULL))
                auto_nego |= ADVERTISE_100FULL;
        else if ((adv & ADVERTISED_1000baseT_Half) && (gbsr & LPA_1000HALF))
                giga_ctrl |= ADVERTISE_1000HALF;
        else if ((adv & ADVERTISED_1000baseT_Full) && (gbsr & LPA_1000FULL))
                giga_ctrl |= ADVERTISE_1000FULL;
        else if ((adv & ADVERTISED_2500baseX_Full) && (status_2500 & MDIO_AN_10GBT_STAT_LP2_5G))
                ctrl_2500 |= MDIO_AN_10GBT_CTRL_ADV2_5G;
        else if ((adv & RTK_ADVERTISED_5000baseX_Full) && (status_2500 & MDIO_AN_10GBT_STAT_LP5G))
                ctrl_2500 |= MDIO_AN_10GBT_CTRL_ADV5G;
        else if ((adv & ADVERTISED_10000baseT_Full) && (status_2500 & MDIO_AN_10GBT_STAT_LP10G))
                ctrl_2500 |= MDIO_AN_10GBT_CTRL_ADV10G;
        else
                return 0;

skip_check_lpa:
#ifdef CONFIG_DOWN_SPEED_100
        auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10HALF |
                      ADVERTISE_10FULL);
#endif
        ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE, auto_nego);
        if (ret < 0)
                return ret;
        ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_VEND2, SPHY_AN_GIGA, giga_ctrl);
        if (ret < 0)
                return ret;
        ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL, ctrl_2500);
        if (ret < 0)
                return ret;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_AN, MDIO_CTRL1,
                                     MDIO_AN_CTRL1_ENABLE | MDIO_AN_CTRL1_RESTART,
                                     MDIO_AN_CTRL1_ENABLE | MDIO_AN_CTRL1_RESTART);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261_nway_reset(struct phy_device *phydev)
{
        int val, ret;

        val = rtl8261_read_phy_ocp(phydev, MDIO_MMD_AN, MDIO_CTRL1);
        if (val < 0)
                return val;

        /* if autoneg is off, it's an error */
        if (!((u32)val & MDIO_AN_CTRL1_ENABLE))
                return -EINVAL;

        ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_AN, MDIO_CTRL1,
                                    (u32)val | MDIO_AN_CTRL1_RESTART);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261_ldps_enable(struct phy_device *phydev, bool enable)
{
        int ret;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND2, 0xA430,
                                     BIT_2, enable ? BIT_2 : 0);
        return ret;
}

static int rtl8261_phy_reset(struct phy_device *phydev)
{
        int val;
        int i;

        val = rtl8261_write_phy_ocp(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1,
                                    MDIO_CTRL1_RESET);
        if (val < 0)
                return val;

        for (i = 0; i < 500; i++) {
                val = rtl8261_read_phy_ocp(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1);
                if (val < 0)
                        return val;

                if (!((u16)val & MDIO_CTRL1_RESET))
                        break;

                udelay(20);
        }

        if (i == 500)
                return -ETIMEDOUT;

        return 0;
}

static int rtl8261_power_up(struct phy_device *phydev, bool enable)
{
        int ret;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1,
                                     BIT_11, enable ? 0 : BIT_11);
        return ret;
}

unsigned int rtl8127_mdio_link_ok(struct rtl8127_private *tp)
{
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp;
        int status;

        if (!HW_SUPPORT_MDIO(tp))
                return 1;

        phytp = phydev->priv;

        mutex_lock(&phytp->lock);

        status = rtl8261_link_ok(phydev);
        if (status < 0)
                status = 0;

        mutex_unlock(&phytp->lock);

        return (unsigned int)status;
}

int rtl8127_mdio_set_sds_speed(struct rtl8127_private *tp, u32 speed)
{
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp;
        int ret;

        if (!HW_SUPPORT_MDIO(tp))
                return 0;

        phytp = phydev->priv;

        mutex_lock(&phytp->lock);

        if (phytp->HwMacsecSerDesSfpCap == speed) {
                ret = 0;
                goto out;
        }

        ret = rtl8261_set_sds_speed(phydev, speed);
        if (ret < 0)
                goto out;

        phytp->HwMacsecSerDesSfpCap = speed;

out:
        mutex_unlock(&phytp->lock);

        return ret;
}

int rtl8127_mdio_set_eth_speed(struct rtl8127_private *tp, u8 autoneg,
                               u32 speed, u8 duplex, u64 adv)
{
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp;
        int ret = 0;

        if (!HW_SUPPORT_MDIO(tp))
                return 0;

        phytp = phydev->priv;

        mutex_lock(&phytp->lock);

        ret = rtl8261_set_eth_speed(phydev, autoneg, speed, duplex, adv);

        mutex_unlock(&phytp->lock);

        return ret;
}

int rtl8127_mdio_config_eth_speed(struct rtl8127_private *tp)
{
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp;
        int ret;

        if (!HW_SUPPORT_MDIO(tp))
                return 0;

        phytp = phydev->priv;

        ret = rtl8127_mdio_set_eth_speed(tp, phytp->ethernetAutoneg,
                                         phytp->ethernetSpeed,
                                         phytp->ethernetDuplex,
                                         phytp->ethernetAdvertising);

        return ret;
}

int rtl8127_mdio_get_eth_speed(struct rtl8127_private *tp,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)
                               struct ethtool_cmd *cmd
#else
                               struct ethtool_link_ksettings *cmd
#endif
                              )
{
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp;
        int ret = 0;

        if (!HW_SUPPORT_MDIO(tp))
                return 0;

        phytp = phydev->priv;

        mutex_lock(&phytp->lock);

        ret = rtl8261_get_eth_speed(phydev, cmd);

        mutex_unlock(&phytp->lock);

        return ret;
}

int rtl8127_mdio_set_wol_eth_speed(struct rtl8127_private *tp)
{
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp;
        int ret = 0;

        if (!HW_SUPPORT_MDIO(tp))
                return 0;

        phytp = phydev->priv;

        mutex_lock(&phytp->lock);

        ret = rtl8261_set_wol_eth_speed(phydev);

        mutex_unlock(&phytp->lock);

        return ret;
}

int rtl8127_mdio_nway_reset(struct rtl8127_private *tp)
{
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp;
        int ret;

        if (!HW_SUPPORT_MDIO(tp))
                return 0;

        phytp = phydev->priv;

        mutex_lock(&phytp->lock);

        ret = rtl8261_nway_reset(phydev);

        mutex_unlock(&phytp->lock);

        return ret;
}

int rtl8127_mdio_power_up(struct rtl8127_private *tp, bool enable)
{
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp;
        int ret;

        if (!HW_SUPPORT_MDIO(tp))
                return 0;

        phytp = phydev->priv;

        mutex_lock(&phytp->lock);

        ret = rtl8261_power_up(phydev, enable);

        mutex_unlock(&phytp->lock);

        return ret;
}

static int rtl8261_init_phy(struct phy_device *phydev)
{
        int ret;

        ret = rtl8261_phy_reset(phydev);
        if (ret < 0)
                return ret;

        ret = rtl8261_ldps_enable(phydev, true);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261_detect(struct phy_device *phydev)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8127_private *tp = phytp->tp;
        int id;

        id = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, MII_PHYSID1);
        if (id < 0 || id != RTL_PHYID1)
                return -ENODEV;

        tp->HwMdioVer = MDIO_PHY_RTL8261D;

        return 0;
}

int rtl8127_mdio_init(struct net_device *netdev)
{
        struct rtl8127_private *tp = netdev_priv(netdev);
        struct phy_device *phydev;
        struct rtl8261_phy_private *phytp;
        int ret;

        phydev = kzalloc(sizeof(*phydev), GFP_KERNEL);
        if (!phydev)
                return -ENOMEM;

        phytp = kzalloc(sizeof(*phytp), GFP_KERNEL);
        if (!phytp) {
                ret = -ENOMEM;
                goto free_phydev;
        }

        phytp->tp = tp;
        phydev->priv = phytp;
        tp->phydev = phydev;

        mutex_init(&phytp->lock);

        rtl8127_mdio_gpio_init(tp);

        ret = rtl8261_detect(phydev);
        if (ret < 0)
                goto free_phytp;

        rtl8261_init_software_variables(phydev);

        ret = rtl8261_init_phy(phydev);
        if (ret < 0)
                goto clear_version;

        return 0;

clear_version:
        tp->HwMdioVer = MDIO_PHY_NONE;
free_phytp:
        rtl8127_mdio_gpio_remove(tp);
        mutex_destroy(&phytp->lock);
        kfree(phytp);
free_phydev:
        kfree(phydev);
        tp->phydev = NULL;

        return ret;
}

void rtl8127_mdio_remove(struct net_device *netdev)
{
        struct rtl8127_private *tp = netdev_priv(netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp;

        if (!HW_SUPPORT_MDIO(tp))
                return;

        phytp = phydev->priv;

        mutex_destroy(&phytp->lock);
        kfree(phytp);
        kfree(phydev);
        tp->phydev = NULL;

        rtl8127_mdio_gpio_remove(tp);
}
