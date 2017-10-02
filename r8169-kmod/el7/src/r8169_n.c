/*
################################################################################
#
# r8169 is the Linux device driver released for Realtek Gigabit Ethernet
# Controllers with PCI interface.
#
# Copyright(c) 2015 Realtek Semiconductor Corp. All rights reserved.
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

/*
 * This driver is modified from r8169.c in Linux kernel 2.6.18
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/init.h>
#include <linux/rtnetlink.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,37)
#include <linux/prefetch.h>
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
#include <linux/moduleparam.h>
#include <linux/dma-mapping.h>
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include "r8169.h"

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static const int max_interrupt_work = 20;

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
   The RTL chips use a 64 element hash table based on the Ethernet CRC. */
static const int multicast_filter_limit = 32;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8)
#ifdef __CHECKER__
#define __force	__attribute__((force))
#else
#define __force
#endif
#endif	//LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8)

#define _R(NAME,MAC,MASK) \
	{ .name = NAME, .mcfg = MAC, .RxConfigMask = MASK }

static const struct {
        const char *name;
        u8 mcfg;
        u32 RxConfigMask;	/* Clears the bits supported by this chip */
} rtl_chip_info[] = {
        _R("RTL8169", CFG_METHOD_1, 0xff7e1880),
        _R("RTL8169S/8110S", CFG_METHOD_2, 0xff7e1880),
        _R("RTL8169S/8110S", CFG_METHOD_3, 0xff7e1880),
        _R("RTL8169SB/8110SB", CFG_METHOD_4, 0xff7e1880),
        _R("RTL8169SC/8110SC", CFG_METHOD_5, 0xff7e1880),
        _R("RTL8169SC/8110SC", CFG_METHOD_6, 0xff7e1880),
};
#undef _R

enum cfg_version {
        RTL_CFG_0 = 0x00,
        RTL_CFG_1,
        RTL_CFG_2
};

static const struct {
        unsigned int region;
        unsigned int align;
} rtl_cfg_info[] = {
        [RTL_CFG_0] = { 1, 8 },
        [RTL_CFG_1] = { 2, 8 },
        [RTL_CFG_2] = { 2, 8 }
};

static struct pci_device_id rtl8169_pci_tbl[] = {
        { PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8167), 0, 0, RTL_CFG_0 },
        { PCI_DEVICE(PCI_VENDOR_ID_REALTEK,	0x8169), 0, 0, RTL_CFG_0 },
        { PCI_VENDOR_ID_DLINK, 0x4300, PCI_VENDOR_ID_DLINK, 0x4300, 0, 0, RTL_CFG_0 },
        { PCI_VENDOR_ID_DLINK, 0x4302, PCI_VENDOR_ID_DLINK, 0x4302, 0, 0, RTL_CFG_0 },
        { PCI_VENDOR_ID_DLINK, 0x4300, PCI_VENDOR_ID_DLINK, 0x4c00, 0, 0, RTL_CFG_0 },
        {0,},
};

MODULE_DEVICE_TABLE(pci, rtl8169_pci_tbl);

static int rx_copybreak = 200;
static int use_dac;
static struct {
        u32 msg_enable;
} debug = { -1 };

static unsigned short speed = SPEED_1000;
static int duplex = DUPLEX_FULL;
static int autoneg = AUTONEG_ENABLE;

module_param(rx_copybreak, int, 0);
MODULE_PARM_DESC(rx_copybreak, "Copy breakpoint for copy-only-tiny-frames");
module_param(use_dac, int, 0);
MODULE_PARM_DESC(use_dac, "Enable PCI DAC. Unsafe on 32 bit PCI slot.");

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
module_param_named(debug, debug.msg_enable, int, 0);
MODULE_PARM_DESC(debug, "Debug verbosity level (0=none, ..., 16=all)");
#endif//LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)

MODULE_LICENSE("GPL");

#ifndef MODULE_VERSION
#define MODULE_VERSION(_version) MODULE_INFO(version, _version)
#endif
MODULE_VERSION(RTL8169_VERSION);

static void rtl8169_set_tx_config(struct net_device *dev);

static void rtl8169_esd_timer(unsigned long __opaque);
static void rtl8169_tx_clear(struct rtl8169_private *tp);
static void rtl8169_rx_clear(struct rtl8169_private *tp);
static void rtl8169_init_ring_indexes(struct rtl8169_private *tp);

static int rtl8169_open(struct net_device *dev);
static int rtl8169_start_xmit(struct sk_buff *skb, struct net_device *dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t rtl8169_interrupt(int irq, void *dev_instance, struct pt_regs *regs);
#else
static irqreturn_t rtl8169_interrupt(int irq, void *dev_instance);
#endif
static int rtl8169_init_ring(struct net_device *dev);
static void rtl8169_hw_start(struct net_device *dev);
static int rtl8169_close(struct net_device *dev);
static void rtl8169_set_rx_mode(struct net_device *dev);
static void rtl8169_tx_timeout(struct net_device *dev);
static struct net_device_stats *rtl8169_get_stats(struct net_device *dev);
static int rtl8169_rx_interrupt(struct net_device *, struct rtl8169_private *, void __iomem *, u32 budget);
static int rtl8169_change_mtu(struct net_device *dev, int new_mtu);
static void rtl8169_down(struct net_device *dev);
static int rtl8169_set_mac_address(struct net_device *dev, void *p);
void rtl8169_rar_set(struct rtl8169_private *tp, uint8_t *addr);

static void rtl8169_phy_power_up(struct net_device *dev);
static void rtl8169_phy_power_down(struct net_device *dev);

static void rtl8169_hw_reset(struct net_device *dev);

#ifdef CONFIG_R8169_NAPI
static int rtl8169_poll(napi_ptr napi, napi_budget budget);
#endif

static const u16 rtl8169_intr_mask =
        SYSErr | LinkChg | RxOverflow | RxFIFOOver | TxErr | TxOK | RxErr | RxOK;
static const u16 rtl8169_napi_event =
        RxOK | RxOverflow | RxFIFOOver | TxOK | TxErr;
static const unsigned int rtl8169_rx_config =
        (Reserved2_data << Reserved2_shift) | (RX_DMA_BURST << RxCfgDMAShift);

#if (( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,27) ) || \
     (( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) ) && \
      ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3) )))
/* copied from linux kernel 2.6.20 include/linux/netdev.h */
#define	NETDEV_ALIGN		32
#define	NETDEV_ALIGN_CONST	(NETDEV_ALIGN - 1)

static inline void *netdev_priv(struct net_device *dev)
{
        return (char *)dev + ((sizeof(struct net_device)
                               + NETDEV_ALIGN_CONST)
                              & ~NETDEV_ALIGN_CONST);
}
#endif

//#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5)
#ifndef netif_msg_init
#define netif_msg_init _kc_netif_msg_init
/* copied from linux kernel 2.6.20 include/linux/netdevice.h */
static inline u32 _kc_netif_msg_init(int debug_value, int default_msg_enable_bits)
{
        /* use default */
        if (debug_value < 0 || debug_value >= (sizeof(u32) * 8))
                return default_msg_enable_bits;
        if (debug_value == 0)	/* no output */
                return 0;
        /* set low N bits */
        return (1 << debug_value) - 1;
}

#endif	//LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5)


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)
/* copied from linux kernel 2.6.20 /include/linux/time.h */
/* Parameters used to convert the timespec values: */
#define MSEC_PER_SEC	1000L

/* copied from linux kernel 2.6.20 /include/linux/jiffies.h */
/*
 * Change timeval to jiffies, trying to avoid the
 * most obvious overflows..
 *
 * And some not so obvious.
 *
 * Note that we don't want to return MAX_LONG, because
 * for various timeout reasons we often end up having
 * to wait "jiffies+1" in order to guarantee that we wait
 * at _least_ "jiffies" - so "jiffies+1" had better still
 * be positive.
 */
#define MAX_JIFFY_OFFSET ((~0UL >> 1)-1)

/*
 * Convert jiffies to milliseconds and back.
 *
 * Avoid unnecessary multiplications/divisions in the
 * two most common HZ cases:
 */
static inline unsigned int _kc_jiffies_to_msecs(const unsigned long j)
{
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
        return (MSEC_PER_SEC / HZ) * j;
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
        return (j + (HZ / MSEC_PER_SEC) - 1)/(HZ / MSEC_PER_SEC);
#else
        return (j * MSEC_PER_SEC) / HZ;
#endif
}

static inline unsigned long _kc_msecs_to_jiffies(const unsigned int m)
{
        if (m > _kc_jiffies_to_msecs(MAX_JIFFY_OFFSET))
                return MAX_JIFFY_OFFSET;
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
        return (m + (MSEC_PER_SEC / HZ) - 1) / (MSEC_PER_SEC / HZ);
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
        return m * (HZ / MSEC_PER_SEC);
#else
        return (m * HZ + MSEC_PER_SEC - 1) / MSEC_PER_SEC;
#endif
}
#endif	//LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
//for linux kernel 2.6.10 and earlier.

/* copied from linux kernel 2.6.12.6 /include/linux/pm.h */
typedef int __bitwise pci_power_t;

/* copied from linux kernel 2.6.12.6 /include/linux/pci.h */
typedef u32 __bitwise pm_message_t;

#define PCI_D0	((pci_power_t __force) 0)
#define PCI_D1	((pci_power_t __force) 1)
#define PCI_D2	((pci_power_t __force) 2)
#define PCI_D3hot	((pci_power_t __force) 3)
#define PCI_D3cold	((pci_power_t __force) 4)
#define PCI_POWER_ERROR	((pci_power_t __force) -1)

/* copied from linux kernel 2.6.12.6 /drivers/pci/pci.c */
/**
 * pci_choose_state - Choose the power state of a PCI device
 * @dev: PCI device to be suspended
 * @state: target sleep state for the whole system. This is the value
 *	that is passed to suspend() function.
 *
 * Returns PCI power state suitable for given device and given system
 * message.
 */

pci_power_t pci_choose_state(struct pci_dev *dev, pm_message_t state)
{
        if (!pci_find_capability(dev, PCI_CAP_ID_PM))
                return PCI_D0;

        switch (state) {
        case 0:
                return PCI_D0;
        case 3:
                return PCI_D3hot;
        default:
                printk("They asked me for state %d\n", state);
//		BUG();
        }
        return PCI_D0;
}
#endif	//LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)
//porting on 2.6.8.1 and earlier
/**
 * msleep_interruptible - sleep waiting for waitqueue interruptions
 * @msecs: Time in milliseconds to sleep for
 */
#define msleep_interruptible _kc_msleep_interruptible
unsigned long _kc_msleep_interruptible(unsigned int msecs)
{
        unsigned long timeout = _kc_msecs_to_jiffies(msecs);

        while (timeout && !signal_pending(current)) {
                set_current_state(TASK_INTERRUPTIBLE);
                timeout = schedule_timeout(timeout);
        }
        return _kc_jiffies_to_msecs(timeout);
}
#endif	//LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8) )
#define msleep(x)	do { set_current_state(TASK_UNINTERRUPTIBLE); \
				schedule_timeout((x * HZ)/1000 + 2); \
			} while (0)
#endif
/*****************************************************************************/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)
/* copied from linux kernel 2.6.20 include/linux/sched.h */
#ifndef __sched
#define __sched		__attribute__((__section__(".sched.text")))
#endif

/* copied from linux kernel 2.6.20 kernel/timer.c */
signed long __sched schedule_timeout_uninterruptible(signed long timeout)
{
        __set_current_state(TASK_UNINTERRUPTIBLE);
        return schedule_timeout(timeout);
}

/* copied from linux kernel 2.6.20 include/linux/mii.h */
#undef if_mii
#define if_mii _kc_if_mii
static inline struct mii_ioctl_data *if_mii(struct ifreq *rq)
{
        return (struct mii_ioctl_data *) &rq->ifr_ifru;
}
#endif	//LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,22)
static inline void eth_copy_and_sum (struct sk_buff *dest,
                                     const unsigned char *src,
                                     int len, int base)
{
        memcpy (dest->data, src, len);
}
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,22)

static void mdio_write(struct rtl8169_private *tp, int RegAddr, int value)
{
        void __iomem *ioaddr = tp->mmio_addr;
        int i;

        RTL_W32(PHYAR,
                PHYAR_Write |
                (RegAddr & PHYAR_Reg_Mask) << PHYAR_Reg_shift |
                (value & PHYAR_Data_Mask));

        for (i = 0; i < 20; i++) {
                /* Check if the RTL8169 has completed writing to the specified MII register */
                if (!(RTL_R32(PHYAR) & PHYAR_Flag))
                        break;
                udelay(100);
        }

        udelay(20);
}

static int mdio_read(struct rtl8169_private *tp, int RegAddr)
{
        void __iomem *ioaddr = tp->mmio_addr;
        int i, value = -1;

        RTL_W32(PHYAR, PHYAR_Read | (RegAddr & PHYAR_Reg_Mask) << PHYAR_Reg_shift);

        for (i = 0; i < 20; i++) {
                /* Check if the RTL8169 has completed retrieving data from the specified MII register */
                if (RTL_R32(PHYAR) & PHYAR_Flag) {
                        udelay(1);
                        value = (int) (RTL_R32(PHYAR) & PHYAR_Data_Mask);
                        break;
                }
                udelay(100);

        }

        udelay(20);

        return value;
}

static void rtl8169_irq_mask_and_ack(void __iomem *ioaddr)
{
        RTL_W16(IntrMask, 0x0000);

        RTL_W16(IntrStatus, 0xffff);
}

static unsigned int rtl8169_xmii_reset_pending(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        unsigned long flags;
        unsigned int retval;

        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1f, 0x0000);
        retval = mdio_read(tp, MII_BMCR) & BMCR_RESET;
        spin_unlock_irqrestore(&tp->phy_lock, flags);

        return retval;
}

static unsigned int rtl8169_xmii_link_ok(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        unsigned long flags;
        unsigned int retval;

        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1f, 0x0000);
        retval = mdio_read(tp, MII_BMSR) & BMSR_LSTATUS;
        spin_unlock_irqrestore(&tp->phy_lock, flags);

        return retval;
}

static void rtl8169_xmii_reset_enable(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        int i, val = 0;
        unsigned long flags;

        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1f, 0x0000);
        mdio_write(tp, MII_BMCR, BMCR_RESET | BMCR_ANENABLE);

        for (i = 0; i < 2500; i++) {
                val = mdio_read(tp, MII_BMCR) & BMCR_RESET;

                if (!val) {
                        spin_unlock_irqrestore(&tp->phy_lock, flags);
                        return;
                }

                mdelay(1);
        }

        spin_unlock_irqrestore(&tp->phy_lock, flags);

        if (netif_msg_link(tp))
                printk(KERN_ERR "%s: PHY reset failed.\n", dev->name);
}

static void rtl8169_check_link_status(struct net_device *dev, struct rtl8169_private *tp, void __iomem *ioaddr)
{
        u8 status;

        if (tp->link_ok(dev)) {
                if (tp->mcfg == CFG_METHOD_4) {
                        status = RTL_R8(PHYstatus);

                        if ((status & _10bps) && (RTL_R8(Config2) & PCI_Clock_66MHz)) {
                                RTL_W32(TxConfig, RTL_R32(TxConfig) & ~(TX_DMA_BURST << TxDMAShift));
                        }
                } else {
                        RTL_W32(TxConfig, RTL_R32(TxConfig) | (TX_DMA_BURST << TxDMAShift));
                }

                if (netif_msg_ifup(tp))
                        printk(KERN_INFO PFX "%s: link up\n", dev->name);

                rtl8169_set_tx_config(dev);

                netif_carrier_on(dev);

                netif_wake_queue(dev);
        } else {
                if (netif_msg_ifdown(tp))
                        printk(KERN_INFO PFX "%s: link down\n", dev->name);

                netif_stop_queue(dev);

                netif_carrier_off(dev);
        }
}

static void
rtl8169_link_option(int idx, u8 *aut, u16 *spd, u8 *dup)
{
        if ((*spd != SPEED_1000) && (*spd != SPEED_100) && (*spd != SPEED_10))
                *spd = SPEED_1000;

        if ((*dup != DUPLEX_FULL) && (*dup != DUPLEX_HALF))
                *dup = DUPLEX_FULL;

        if ((*aut != AUTONEG_ENABLE) && (*aut != AUTONEG_DISABLE))
                *aut = AUTONEG_ENABLE;
}

static void
rtl8169_powerdown_pll(struct rtl8169_private *tp)
{
        struct net_device *dev = pci_get_drvdata(tp->pci_dev);
        void __iomem *ioaddr = tp->mmio_addr;

        RTL_W16(RxMaxSize, RX_BUF_SIZE);

        if (tp->wol_enabled == WOL_ENABLED) {
                int auto_nego;
                int giga_ctrl;
                u16 val;
                unsigned long flags;
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1F, 0x0000);
                auto_nego = mdio_read(tp, MII_ADVERTISE);
                auto_nego &= ~(ADVERTISE_10HALF | ADVERTISE_10FULL
                               | ADVERTISE_100HALF | ADVERTISE_100FULL);

                val = mdio_read(tp, MII_LPA);

                if (val & (LPA_10HALF | LPA_10FULL))
                        auto_nego |= (ADVERTISE_10HALF | ADVERTISE_10FULL);
                else
                        auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10HALF | ADVERTISE_10FULL);

                giga_ctrl = mdio_read(tp, MII_CTRL1000) & ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL);
                mdio_write(tp, MII_ADVERTISE, auto_nego);
                mdio_write(tp, MII_CTRL1000, giga_ctrl);
                mdio_write(tp, MII_BMCR, BMCR_RESET | BMCR_ANENABLE | BMCR_ANRESTART);
                spin_unlock_irqrestore(&tp->phy_lock, flags);

                return;
        }

        rtl8169_phy_power_down(dev);
}

static void rtl8169_powerup_pll(struct net_device *dev)
{
        rtl8169_phy_power_up(dev);
}

static void rtl8169_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u8 options;

        wol->wolopts = 0;

#define WAKE_ANY (WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_BCAST | WAKE_MCAST)
        wol->supported = WAKE_ANY;

        spin_lock_irq(&tp->lock);

        options = RTL_R8(Config1);
        if (!(options & PMEnable))
                goto out_unlock;

        options = RTL_R8(Config3);
        if (options & LinkUp)
                wol->wolopts |= WAKE_PHY;
        if (options & MagicPacket)
                wol->wolopts |= WAKE_MAGIC;

        options = RTL_R8(Config5);
        if (options & UWF)
                wol->wolopts |= WAKE_UCAST;
        if (options & BWF)
                wol->wolopts |= WAKE_BCAST;
        if (options & MWF)
                wol->wolopts |= WAKE_MCAST;

out_unlock:
        spin_unlock_irq(&tp->lock);
}

static int rtl8169_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i;
        static struct {
                u32 opt;
                u16 reg;
                u8  mask;
        } cfg[] = {
                { WAKE_ANY,   Config1, PMEnable },
                { WAKE_PHY,   Config3, LinkUp },
                { WAKE_MAGIC, Config3, MagicPacket },
                { WAKE_UCAST, Config5, UWF },
                { WAKE_BCAST, Config5, BWF },
                { WAKE_MCAST, Config5, MWF },
                { WAKE_ANY,   Config5, LanWake }
        };

        spin_lock_irq(&tp->lock);

        RTL_W8(Cfg9346, Cfg9346_Unlock);

        for (i = 0; i < ARRAY_SIZE(cfg); i++) {
                u8 options = RTL_R8(cfg[i].reg) & ~cfg[i].mask;
                if (wol->wolopts & cfg[i].opt)
                        options |= cfg[i].mask;
                RTL_W8(cfg[i].reg, options);
        }

        RTL_W8(Cfg9346, Cfg9346_Lock);

        tp->wol_enabled = (wol->wolopts) ? WOL_ENABLED : WOL_DISABLED;

        spin_unlock_irq(&tp->lock);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
        device_set_wakeup_enable(&tp->pci_dev->dev, tp->wol_enabled);
#endif

        return 0;
}

static void rtl8169_get_drvinfo(struct net_device *dev,
                                struct ethtool_drvinfo *info)
{
        struct rtl8169_private *tp = netdev_priv(dev);

        strcpy(info->driver, MODULENAME);
        strcpy(info->version, RTL8169_VERSION);
        strcpy(info->bus_info, pci_name(tp->pci_dev));
        info->regdump_len = R8169_REGS_DUMP_SIZE;
}

static int rtl8169_get_regs_len(struct net_device *dev)
{
        return R8169_REGS_DUMP_SIZE;
}

static void rtl8169_set_tx_config(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u32 tx_config;
        u8 duplex;

        duplex = (RTL_R8(PHYstatus) & FullDup) ? DUPLEX_FULL : DUPLEX_HALF;

        tx_config = RTL_R32(TxConfig) | IFG0 | IFG1;

        if (((tp->mcfg == CFG_METHOD_5) | (tp->mcfg == CFG_METHOD_6)) && duplex == DUPLEX_HALF)
                tx_config &= ~IFG0;

        RTL_W32(TxConfig, tx_config);
}

static int rtl8169_set_speed_xmii(struct net_device *dev,
                                  u8 autoneg, u16 speed, u8 duplex)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        int auto_nego = 0;
        int giga_ctrl = 0;
        int bmcr_true_force = 0;
        unsigned long flags;

        if ((speed != SPEED_1000) &&
            (speed != SPEED_100) &&
            (speed != SPEED_10)) {
                speed = SPEED_1000;
                duplex = DUPLEX_FULL;
        }

        if ((autoneg == AUTONEG_ENABLE) || (speed == SPEED_1000)) {
                /*n-way force*/
                if ((speed == SPEED_10) && (duplex == DUPLEX_HALF)) {
                        auto_nego |= ADVERTISE_10HALF;
                } else if ((speed == SPEED_10) && (duplex == DUPLEX_FULL)) {
                        auto_nego |= ADVERTISE_10HALF |
                                     ADVERTISE_10FULL;
                } else if ((speed == SPEED_100) && (duplex == DUPLEX_HALF)) {
                        auto_nego |= ADVERTISE_100HALF |
                                     ADVERTISE_10HALF |
                                     ADVERTISE_10FULL;
                } else if ((speed == SPEED_100) && (duplex == DUPLEX_FULL)) {
                        auto_nego |= ADVERTISE_100HALF |
                                     ADVERTISE_100FULL |
                                     ADVERTISE_10HALF |
                                     ADVERTISE_10FULL;
                } else if (speed == SPEED_1000) {
                        giga_ctrl |= ADVERTISE_1000HALF |
                                     ADVERTISE_1000FULL;

                        auto_nego |= ADVERTISE_100HALF |
                                     ADVERTISE_100FULL |
                                     ADVERTISE_10HALF |
                                     ADVERTISE_10FULL;
                }

                auto_nego |= ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;

                tp->phy_auto_nego_reg = auto_nego;
                tp->phy_1000_ctrl_reg = giga_ctrl;

                tp->autoneg = autoneg;
                tp->speed = speed;
                tp->duplex = duplex;

                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, MII_ADVERTISE, auto_nego);
                mdio_write(tp, MII_CTRL1000, giga_ctrl);
                mdio_write(tp, MII_BMCR, BMCR_ANENABLE | BMCR_ANRESTART);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
        } else {
                /*true force*/
#ifndef BMCR_SPEED100
#define BMCR_SPEED100	0x0040
#endif

#ifndef BMCR_SPEED10
#define BMCR_SPEED10	0x0000
#endif
                if ((speed == SPEED_10) && (duplex == DUPLEX_HALF)) {
                        bmcr_true_force = BMCR_SPEED10;
                } else if ((speed == SPEED_10) && (duplex == DUPLEX_FULL)) {
                        bmcr_true_force = BMCR_SPEED10 | BMCR_FULLDPLX;
                } else if ((speed == SPEED_100) && (duplex == DUPLEX_HALF)) {
                        bmcr_true_force = BMCR_SPEED100;
                } else if ((speed == SPEED_100) && (duplex == DUPLEX_FULL)) {
                        bmcr_true_force = BMCR_SPEED100 | BMCR_FULLDPLX;
                }

                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, MII_BMCR, bmcr_true_force);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
        }

        if ((tp->mcfg == CFG_METHOD_2) || (tp->mcfg == CFG_METHOD_3)) {
                spin_lock_irqsave(&tp->phy_lock, flags);
                if ((speed == SPEED_100) && (autoneg != AUTONEG_ENABLE)) {
                        mdio_write(tp, 0x17, 0x2138);
                        mdio_write(tp, 0x0e, 0x0260);
                } else {
                        mdio_write(tp, 0x17, 0x2108);
                        mdio_write(tp, 0x0e, 0x0000);
                }
                spin_unlock_irqrestore(&tp->phy_lock, flags);
        }

        tp->autoneg = autoneg;
        tp->speed = speed;
        tp->duplex = duplex;

        return 0;
}

static int rtl8169_set_speed(struct net_device *dev,
                             u8 autoneg, u16 speed, u8 duplex)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        int ret;

        ret = tp->set_speed(dev, autoneg, speed, duplex);

        return ret;
}

static int rtl8169_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        unsigned long flags;
        int ret;

        spin_lock_irqsave(&tp->lock, flags);
        ret = rtl8169_set_speed(dev, cmd->autoneg, cmd->speed, cmd->duplex);
        spin_unlock_irqrestore(&tp->lock, flags);

        return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static u32
rtl8169_get_tx_csum(struct net_device *dev)
{
        return (dev->features & NETIF_F_IP_CSUM) != 0;
}

static u32 rtl8169_get_rx_csum(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);

        return tp->cp_cmd & RxChkSum;
}

static int
rtl8169_set_tx_csum(struct net_device *dev,
                    u32 data)
{
        if (data)
                dev->features |= NETIF_F_IP_CSUM;
        else
                dev->features &= ~NETIF_F_IP_CSUM;

        return 0;
}

static int rtl8169_set_rx_csum(struct net_device *dev, u32 data)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);

        if (data)
                tp->cp_cmd |= RxChkSum;
        else
                tp->cp_cmd &= ~RxChkSum;

        RTL_W16(CPlusCmd, tp->cp_cmd);
        RTL_R16(CPlusCmd);

        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}
#endif

#ifdef CONFIG_R8169_VLAN

static inline u32 rtl8169_tx_vlan_tag(struct rtl8169_private *tp,
                                      struct sk_buff *skb)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        return (tp->vlgrp && vlan_tx_tag_present(skb)) ?
               TxVlanTag | swab16(vlan_tx_tag_get(skb)) : 0x00;
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0)
        return (vlan_tx_tag_present(skb)) ?
               TxVlanTag | swab16(vlan_tx_tag_get(skb)) : 0x00;
#else
        return (skb_vlan_tag_present(skb)) ?
               TxVlanTag | swab16(skb_vlan_tag_get(skb)) : 0x00;
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)

static void rtl8169_vlan_rx_register(struct net_device *dev,
                                     struct vlan_group *grp)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        tp->vlgrp = grp;
        if (tp->vlgrp)
                tp->cp_cmd |= RxVlan;
        else
                tp->cp_cmd &= ~RxVlan;
        RTL_W16(CPlusCmd, tp->cp_cmd);
        RTL_R16(CPlusCmd);
        spin_unlock_irqrestore(&tp->lock, flags);
}

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
static void rtl8169_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
        if (tp->vlgrp)
                tp->vlgrp->vlan_devices[vid] = NULL;
#else
        vlan_group_set_device(tp->vlgrp, vid, NULL);
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
        spin_unlock_irqrestore(&tp->lock, flags);
}
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)

static int rtl8169_rx_vlan_skb(struct rtl8169_private *tp,
                               struct RxDesc *desc,
                               struct sk_buff *skb)
{
        u32 opts2 = le32_to_cpu(desc->opts2);
        int ret = -1;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        if (tp->vlgrp && (opts2 & RxVlanTag)) {
                rtl8169_rx_hwaccel_skb(skb, tp->vlgrp,
                                       swab16(opts2 & 0xffff));
                ret = 0;
        }
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
        if (opts2 & RxVlanTag)
                __vlan_hwaccel_put_tag(skb, swab16(opts2 & 0xffff));
#else
        if (opts2 & RxVlanTag)
                __vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), swab16(opts2 & 0xffff));
#endif

        desc->opts2 = 0;
        return ret;
}

#else /* !CONFIG_R8169_VLAN */

static inline u32 rtl8169_tx_vlan_tag(struct rtl8169_private *tp,
                                      struct sk_buff *skb)
{
        return 0;
}

static int rtl8169_rx_vlan_skb(struct rtl8169_private *tp,
                               struct RxDesc *desc,
                               struct sk_buff *skb)
{
        return -1;
}

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static u32 rtl8169_fix_features(struct net_device *dev, u32 features)
#else
static netdev_features_t rtl8169_fix_features(struct net_device *dev,
                netdev_features_t features)
#endif
{
        if (dev->mtu > MSSMask)
                features &= ~NETIF_F_ALL_TSO;

        return features;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static int rtl8169_hw_set_features(struct net_device *dev, u32 features)
#else
static int rtl8169_hw_set_features(struct net_device *dev,
                                   netdev_features_t features)
#endif
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        if (features & NETIF_F_RXCSUM)
                tp->cp_cmd |= RxChkSum;
        else
                tp->cp_cmd &= ~RxChkSum;

        if (dev->features & NETIF_F_HW_VLAN_RX)
                tp->cp_cmd |= RxVlan;
        else
                tp->cp_cmd &= ~RxVlan;

        RTL_W16(CPlusCmd, tp->cp_cmd);
        RTL_R16(CPlusCmd);

        return 0;
}



#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static int rtl8169_set_features(struct net_device *dev, u32 features)
#else
static int rtl8169_set_features(struct net_device *dev,
                                netdev_features_t features)
#endif
{
        struct rtl8169_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);

        rtl8169_hw_set_features(dev, features);

        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}

#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)

static void rtl8169_gset_xmii(struct net_device *dev, struct ethtool_cmd *cmd)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u8 status;
        unsigned long flags;

        cmd->supported = SUPPORTED_10baseT_Half |
                         SUPPORTED_10baseT_Full |
                         SUPPORTED_100baseT_Half |
                         SUPPORTED_100baseT_Full |
                         SUPPORTED_1000baseT_Full |
                         SUPPORTED_Autoneg |
                         SUPPORTED_TP;

        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1F, 0x0000);
        cmd->autoneg = (mdio_read(tp, MII_BMCR) & BMCR_ANENABLE) ? 1 : 0;
        spin_unlock_irqrestore(&tp->phy_lock, flags);
        cmd->advertising = ADVERTISED_TP | ADVERTISED_Autoneg;

        if (tp->phy_auto_nego_reg & ADVERTISE_10HALF)
                cmd->advertising |= ADVERTISED_10baseT_Half;
        if (tp->phy_auto_nego_reg & ADVERTISE_10FULL)
                cmd->advertising |= ADVERTISED_10baseT_Full;
        if (tp->phy_auto_nego_reg & ADVERTISE_100HALF)
                cmd->advertising |= ADVERTISED_100baseT_Half;
        if (tp->phy_auto_nego_reg & ADVERTISE_100FULL)
                cmd->advertising |= ADVERTISED_100baseT_Full;
        if (tp->phy_1000_ctrl_reg & ADVERTISE_1000FULL)
                cmd->advertising |= ADVERTISED_1000baseT_Full;

        status = RTL_R8(PHYstatus);

        if (status & _1000bpsF)
                cmd->speed = SPEED_1000;
        else if (status & _100bps)
                cmd->speed = SPEED_100;
        else if (status & _10bps)
                cmd->speed = SPEED_10;

        if (status & TxFlowCtrl)
                cmd->advertising |= ADVERTISED_Asym_Pause;

        if (status & RxFlowCtrl)
                cmd->advertising |= ADVERTISED_Pause;

        cmd->duplex = ((status & _1000bpsF) || (status & FullDup)) ?
                      DUPLEX_FULL : DUPLEX_HALF;
}

static int rtl8169_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);

        tp->get_settings(dev, cmd);

        spin_unlock_irqrestore(&tp->lock, flags);
        return 0;
}

static void rtl8169_get_regs(struct net_device *dev, struct ethtool_regs *regs,
                             void *p)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned int i;
        u8 *data = p;
        unsigned long flags;

        if (regs->len < R8169_REGS_DUMP_SIZE)
                return /* -EINVAL */;

        memset(p, 0, regs->len);

        spin_lock_irqsave(&tp->lock, flags);
        for (i = 0; i < R8169_MAC_REGS_SIZE; i++)
                *data++ = readb(ioaddr + i);
        spin_unlock_irqrestore(&tp->lock, flags);
        data = (u8*)p + 256;

        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1F, 0x0000);
        for (i = 0; i < R8169_PHY_REGS_SIZE/2; i++) {
                *(u16*)data = mdio_read(tp, i);
                data += 2;
        }
        spin_unlock_irqrestore(&tp->phy_lock, flags);
}

static u32 rtl8169_get_msglevel(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);

        return tp->msg_enable;
}

static void rtl8169_set_msglevel(struct net_device *dev, u32 value)
{
        struct rtl8169_private *tp = netdev_priv(dev);

        tp->msg_enable = value;
}

static const char rtl8169_gstrings[][ETH_GSTRING_LEN] = {
        "tx_packets",
        "rx_packets",
        "tx_errors",
        "rx_errors",
        "rx_missed",
        "align_errors",
        "tx_single_collisions",
        "tx_multi_collisions",
        "unicast",
        "broadcast",
        "multicast",
        "tx_aborted",
        "tx_underrun",
};

struct rtl8169_counters {
        u64	tx_packets;
        u64	rx_packets;
        u64	tx_errors;
        u32	rx_errors;
        u16	rx_missed;
        u16	align_errors;
        u32	tx_one_collision;
        u32	tx_multi_collision;
        u64	rx_unicast;
        u64	rx_broadcast;
        u32	rx_multicast;
        u16	tx_aborted;
        u16	tx_underun;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
static int rtl8169_get_stats_count(struct net_device *dev)
{
        return ARRAY_SIZE(rtl8169_gstrings);
}
#endif

static void rtl8169_get_ethtool_stats(struct net_device *dev,
                                      struct ethtool_stats *stats, u64 *data)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        struct rtl8169_counters *counters;
        dma_addr_t paddr;
        u32 cmd;

        ASSERT_RTNL();

        counters = pci_alloc_consistent(tp->pci_dev, sizeof(*counters), &paddr);
        if (!counters)
                return;

        RTL_W32(CounterAddrHigh, (u64)paddr >> 32);
        cmd = (u64)paddr & DMA_32BIT_MASK;
        RTL_W32(CounterAddrLow, cmd);
        RTL_W32(CounterAddrLow, cmd | CounterDump);

        while (RTL_R32(CounterAddrLow) & CounterDump) {
                if (msleep_interruptible(1))
                        break;
        }

        RTL_W32(CounterAddrLow, 0);
        RTL_W32(CounterAddrHigh, 0);

        data[0] = le64_to_cpu(counters->tx_packets);
        data[1] = le64_to_cpu(counters->rx_packets);
        data[2] = le64_to_cpu(counters->tx_errors);
        data[3] = le32_to_cpu(counters->rx_errors);
        data[4] = le16_to_cpu(counters->rx_missed);
        data[5] = le16_to_cpu(counters->align_errors);
        data[6] = le32_to_cpu(counters->tx_one_collision);
        data[7] = le32_to_cpu(counters->tx_multi_collision);
        data[8] = le64_to_cpu(counters->rx_unicast);
        data[9] = le64_to_cpu(counters->rx_broadcast);
        data[10] = le32_to_cpu(counters->rx_multicast);
        data[11] = le16_to_cpu(counters->tx_aborted);
        data[12] = le16_to_cpu(counters->tx_underun);

        pci_free_consistent(tp->pci_dev, sizeof(*counters), counters, paddr);
}

static void rtl8169_get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
        switch(stringset) {
        case ETH_SS_STATS:
                memcpy(data, *rtl8169_gstrings, sizeof(rtl8169_gstrings));
                break;
        }
}

#undef ethtool_op_get_link
#define ethtool_op_get_link _kc_ethtool_op_get_link
u32 _kc_ethtool_op_get_link(struct net_device *dev)
{
        return netif_carrier_ok(dev) ? 1 : 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
#undef ethtool_op_get_sg
#define ethtool_op_get_sg _kc_ethtool_op_get_sg
u32 _kc_ethtool_op_get_sg(struct net_device *dev)
{
#ifdef NETIF_F_SG
        return (dev->features & NETIF_F_SG) != 0;
#else
        return 0;
#endif
}

#undef ethtool_op_set_sg
#define ethtool_op_set_sg _kc_ethtool_op_set_sg
int _kc_ethtool_op_set_sg(struct net_device *dev, u32 data)
{
#ifdef NETIF_F_SG
        if (data)
                dev->features |= NETIF_F_SG;
        else
                dev->features &= ~NETIF_F_SG;
#endif

        return 0;
}
#endif

static struct ethtool_ops rtl8169_ethtool_ops = {
        .get_drvinfo		= rtl8169_get_drvinfo,
        .get_regs_len		= rtl8169_get_regs_len,
        .get_link		= ethtool_op_get_link,
        .get_settings		= rtl8169_get_settings,
        .set_settings		= rtl8169_set_settings,
        .get_msglevel		= rtl8169_get_msglevel,
        .set_msglevel		= rtl8169_set_msglevel,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
        .get_rx_csum		= rtl8169_get_rx_csum,
        .set_rx_csum		= rtl8169_set_rx_csum,
        .get_tx_csum		= rtl8169_get_tx_csum,
        .set_tx_csum		= rtl8169_set_tx_csum,
        .get_sg			= ethtool_op_get_sg,
        .set_sg			= ethtool_op_set_sg,
#ifdef NETIF_F_TSO
        .get_tso		= ethtool_op_get_tso,
        .set_tso		= ethtool_op_set_tso,
#endif
#endif
        .get_regs		= rtl8169_get_regs,
        .get_wol		= rtl8169_get_wol,
        .set_wol		= rtl8169_set_wol,
        .get_strings		= rtl8169_get_strings,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
        .get_stats_count	= rtl8169_get_stats_count,
#endif
        .get_ethtool_stats	= rtl8169_get_ethtool_stats,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
#ifdef ETHTOOL_GPERMADDR
        .get_perm_addr		= ethtool_op_get_perm_addr,
#endif
#endif//LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
};

static void rtl8169_get_mac_version(struct rtl8169_private *tp, void __iomem *ioaddr)
{
        u32 reg,val32;

        val32 = RTL_R32(TxConfig)  ;
        reg = val32 & 0xFC800000;

        switch(reg) {
        case 0x00000000:
                tp->mcfg = CFG_METHOD_1;
                break;
        case 0x00800000:
                tp->mcfg = CFG_METHOD_2;
                break;
        case 0x04000000:
                tp->mcfg = CFG_METHOD_3;
                break;
        case 0x10000000:
                tp->mcfg = CFG_METHOD_4;
                break;
        case 0x18000000:
                tp->mcfg = CFG_METHOD_5;
                break;
        case 0x98000000:
                tp->mcfg = CFG_METHOD_6;
                break;
        default:
                tp->mcfg = 0xFFFFFFFF;
                printk("unknown chip version (%x)\n",reg);
                break;
        }
}

static void rtl8169_print_mac_version(struct rtl8169_private *tp)
{
        dprintk("mac_version = 0x%02x\n", tp->mcfg);
}

static void rtl8169_get_phy_version(struct rtl8169_private *tp)
{
        const struct {
                u16 mask;
                u16 set;
                int pcfg;
        } phy_info[] = {
                { 0x000f, 0x0002, PCFG_METHOD_5 },
                { 0x000f, 0x0001, PCFG_METHOD_4 },
                { 0x000f, 0x0000, PCFG_METHOD_3 },
                { 0x0000, 0x0000, PCFG_METHOD_2 } /* Catch-all */
        }, *p = phy_info;
        u16 reg;

        mdio_write(tp, 0x1f, 0x0000);
        reg = mdio_read(tp, MII_PHYSID2) & 0xffff;

        while ((reg & p->mask) != p->set)
                p++;
        tp->pcfg = p->pcfg;
}

static void rtl8169_print_phy_version(struct rtl8169_private *tp)
{
        struct {
                int version;
                char *msg;
                u32 reg;
        } phy_print[] = {
                { PCFG_METHOD_5, "PCFG_METHOD_5", 0x0002 },
                { PCFG_METHOD_4, "PCFG_METHOD_4", 0x0001 },
                { PCFG_METHOD_3, "PCFG_METHOD_3", 0x0000 },
                { PCFG_METHOD_2, "PCFG_METHOD_2", 0x0000 },
                { 0, NULL, 0x0000 }
        }, *p;

        for (p = phy_print; p->msg; p++) {
                if (tp->pcfg == p->version) {
                        dprintk("phy_version == %s (%04x)\n", p->msg, p->reg);
                        return;
                }
        }
        dprintk("phy_version == Unknown\n");
}

static void rtl8169_hw_phy_config(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;
        unsigned long flags;
        u16 vendor_id;
        u16 device_id;

        tp->phy_reset_enable(dev);

        spin_lock_irqsave(&tp->phy_lock, flags);
        if (tp->mcfg == CFG_METHOD_6) {
                mdio_write(tp, 0x1f, 0x0001);
                mdio_write(tp, 0x04, 0x0000);
                mdio_write(tp, 0x03, 0x00a1);
                mdio_write(tp, 0x02, 0x0008);
                mdio_write(tp, 0x01, 0x0120);
                mdio_write(tp, 0x00, 0x1000);
                mdio_write(tp, 0x04, 0x0800);
                mdio_write(tp, 0x04, 0x9000);
                mdio_write(tp, 0x03, 0x802f);
                mdio_write(tp, 0x02, 0x4f02);
                mdio_write(tp, 0x01, 0x0409);
                mdio_write(tp, 0x00, 0xf099);
                mdio_write(tp, 0x04, 0x9800);
                mdio_write(tp, 0x04, 0xa000);
                mdio_write(tp, 0x03, 0xdf01);
                mdio_write(tp, 0x02, 0xdf20);
                mdio_write(tp, 0x01, 0xff95);
                mdio_write(tp, 0x00, 0xba00);
                mdio_write(tp, 0x04, 0xa800);
                mdio_write(tp, 0x04, 0xf000);
                mdio_write(tp, 0x03, 0xdf01);
                mdio_write(tp, 0x02, 0xdf20);
                mdio_write(tp, 0x01, 0x101a);
                mdio_write(tp, 0x00, 0xa0ff);
                mdio_write(tp, 0x04, 0xf800);
                mdio_write(tp, 0x04, 0x0000);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1f, 0x0001);
                mdio_write(tp, 0x0b, 0x8480);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1f, 0x0001);
                mdio_write(tp, 0x18, 0x67c7);
                mdio_write(tp, 0x04, 0x2000);
                mdio_write(tp, 0x03, 0x002f);
                mdio_write(tp, 0x02, 0x4360);
                mdio_write(tp, 0x01, 0x0109);
                mdio_write(tp, 0x00, 0x3022);
                mdio_write(tp, 0x04, 0x2800);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1f, 0x0001);
                mdio_write(tp, 0x17, 0x0cc0);
                mdio_write(tp, 0x1f, 0x0000);
        } else if (tp->mcfg == CFG_METHOD_5) {
                mdio_write(tp, 0x1f, 0x0001);
                mdio_write(tp, 0x04, 0x0000);
                mdio_write(tp, 0x03, 0x00a1);
                mdio_write(tp, 0x02, 0x0008);
                mdio_write(tp, 0x01, 0x0120);
                mdio_write(tp, 0x00, 0x1000);
                mdio_write(tp, 0x04, 0x0800);
                mdio_write(tp, 0x04, 0x9000);
                mdio_write(tp, 0x03, 0x802f);
                mdio_write(tp, 0x02, 0x4f02);
                mdio_write(tp, 0x01, 0x0409);
                mdio_write(tp, 0x00, 0xf099);
                mdio_write(tp, 0x04, 0x9800);
                mdio_write(tp, 0x04, 0xa000);
                mdio_write(tp, 0x03, 0xdf01);
                mdio_write(tp, 0x02, 0xdf20);
                mdio_write(tp, 0x01, 0xff95);
                mdio_write(tp, 0x00, 0xba00);
                mdio_write(tp, 0x04, 0xa800);
                mdio_write(tp, 0x04, 0xf000);
                mdio_write(tp, 0x03, 0xdf01);
                mdio_write(tp, 0x02, 0xdf20);
                mdio_write(tp, 0x01, 0x101a);
                mdio_write(tp, 0x00, 0xa0ff);
                mdio_write(tp, 0x04, 0xf800);
                mdio_write(tp, 0x04, 0x0000);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1f, 0x0001);
                mdio_write(tp, 0x10, 0xf41b);
                mdio_write(tp, 0x14, 0xfb54);
                mdio_write(tp, 0x18, 0xf5c7);
                mdio_write(tp, 0x1f, 0x0000);

                mdio_write(tp, 0x1f, 0x0001);
                mdio_write(tp, 0x17, 0x0cc0);
                mdio_write(tp, 0x1f, 0x0000);

                pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, &vendor_id);
                pci_read_config_word(pdev, PCI_SUBSYSTEM_ID, &device_id);
                if ((vendor_id == 0x1458) && (device_id == 0xe000)) {
                        mdio_write(tp, 0x1f, 0x0001);
                        mdio_write(tp, 0x10, 0xf01b);
                        mdio_write(tp, 0x1f, 0x0000);
                }
        } else if (tp->mcfg == CFG_METHOD_4) {
                mdio_write(tp, 0x1f, 0x0002);
                mdio_write(tp, 0x01, 0x90d0);
                mdio_write(tp, 0x1f, 0x0000);
                //mdio_write(tp, 0x1e, 0x8c00); /* PHY link down with some Giga switch */
        } else if ((tp->mcfg == CFG_METHOD_2) || (tp->mcfg == CFG_METHOD_3)) {
                mdio_write(tp, 0x1f, 0x0001);
                mdio_write(tp, 0x06, 0x006e);
                mdio_write(tp, 0x08, 0x0708);
                mdio_write(tp, 0x15, 0x4000);
                mdio_write(tp, 0x18, 0x65c7);

                mdio_write(tp, 0x1f, 0x0001);
                mdio_write(tp, 0x03, 0x00a1);
                mdio_write(tp, 0x02, 0x0008);
                mdio_write(tp, 0x01, 0x0120);
                mdio_write(tp, 0x00, 0x1000);
                mdio_write(tp, 0x04, 0x0800);
                mdio_write(tp, 0x04, 0x0000);

                mdio_write(tp, 0x03, 0xff41);
                mdio_write(tp, 0x02, 0xdf60);
                mdio_write(tp, 0x01, 0x0140);
                mdio_write(tp, 0x00, 0x0077);
                mdio_write(tp, 0x04, 0x7800);
                mdio_write(tp, 0x04, 0x7000);

                mdio_write(tp, 0x03, 0x802f);
                mdio_write(tp, 0x02, 0x4f02);
                mdio_write(tp, 0x01, 0x0409);
                mdio_write(tp, 0x00, 0xf0f9);
                mdio_write(tp, 0x04, 0x9800);
                mdio_write(tp, 0x04, 0x9000);

                mdio_write(tp, 0x03, 0xdf01);
                mdio_write(tp, 0x02, 0xdf20);
                mdio_write(tp, 0x01, 0xff95);
                mdio_write(tp, 0x00, 0xba00);
                mdio_write(tp, 0x04, 0xa800);
                mdio_write(tp, 0x04, 0xa000);

                mdio_write(tp, 0x03, 0xff41);
                mdio_write(tp, 0x02, 0xdf20);
                mdio_write(tp, 0x01, 0x0140);
                mdio_write(tp, 0x00, 0x00bb);
                mdio_write(tp, 0x04, 0xb800);
                mdio_write(tp, 0x04, 0xb000);

                mdio_write(tp, 0x03, 0xdf41);
                mdio_write(tp, 0x02, 0xdc60);
                mdio_write(tp, 0x01, 0x6340);
                mdio_write(tp, 0x00, 0x007d);
                mdio_write(tp, 0x04, 0xd800);
                mdio_write(tp, 0x04, 0xd000);

                mdio_write(tp, 0x03, 0xdf01);
                mdio_write(tp, 0x02, 0xdf20);
                mdio_write(tp, 0x01, 0x100a);
                mdio_write(tp, 0x00, 0xa0ff);
                mdio_write(tp, 0x04, 0xf800);
                mdio_write(tp, 0x04, 0xf000);

                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, 0x0b, 0x0000);
                mdio_write(tp, 0x00, 0x9200);
        }

        mdio_write(tp, 0x1F, 0x0000);

        spin_unlock_irqrestore(&tp->phy_lock, flags);
}

static void rtl8169_phy_timer(unsigned long __opaque)
{
        struct net_device *dev = (struct net_device *)__opaque;
        struct rtl8169_private *tp = netdev_priv(dev);
        struct timer_list *timer = &tp->link_timer;
        unsigned long timeout = RTL8169_PHY_TIMEOUT;

        assert(tp->mcfg > CFG_METHOD_1);
        assert(tp->pcfg < PCFG_METHOD_6);

        if (!(tp->phy_1000_ctrl_reg & ADVERTISE_1000FULL))
                return;

        spin_lock_irq(&tp->lock);

        if (tp->phy_reset_pending(dev)) {
                /*
                 * A busy loop could burn quite a few cycles on nowadays CPU.
                 * Let's delay the execution of the timer for a few ticks.
                 */
                timeout = HZ/10;
                goto out_mod_timer;
        }

        if (tp->link_ok(dev))
                goto out_unlock;

        if (netif_msg_link(tp))
                printk(KERN_WARNING "%s: PHY reset until link up\n", dev->name);

        tp->phy_reset_enable(dev);

out_mod_timer:
        mod_timer(timer, jiffies + timeout);
out_unlock:
        spin_unlock_irq(&tp->lock);
}

static inline void rtl8169_delete_timer(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        struct timer_list *timer = &tp->link_timer;

        if ((tp->mcfg <= CFG_METHOD_1) ||
            (tp->pcfg >= PCFG_METHOD_6))
                return;

        del_timer_sync(timer);
}

static inline void rtl8169_request_timer(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        struct timer_list *timer = &tp->link_timer;

        if ((tp->mcfg <= CFG_METHOD_1) ||
            (tp->pcfg >= PCFG_METHOD_6))
                return;

        init_timer(timer);
        timer->expires = jiffies + RTL8169_PHY_TIMEOUT;
        timer->data = (unsigned long)(dev);
        timer->function = rtl8169_phy_timer;
        add_timer(timer);
}

static inline void rtl8169_delete_esd_timer(struct net_device *dev, struct timer_list *timer)
{
        struct rtl8169_private *tp = netdev_priv(dev);

        spin_lock_irq(&tp->lock);
        del_timer_sync(timer);
        spin_unlock_irq(&tp->lock);
}

static inline void rtl8169_request_esd_timer(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        struct timer_list *timer = &tp->esd_timer;

        init_timer(timer);
        timer->expires = jiffies + RTL8169_ESD_TIMEOUT;
        timer->data = (unsigned long)(dev);
        timer->function = rtl8169_esd_timer;
        add_timer(timer);
}

static void
rtl8169_hw_init(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        if (RTL_R8(Config2) & PCI_Clock_66MHz) {
                if (tp->mcfg == CFG_METHOD_5)
                        RTL_W32(Offset_7Ch, 0x000FFFFF);
                else if (tp->mcfg == CFG_METHOD_6)
                        RTL_W32(Offset_7Ch, 0x003FFFFF);
        } else {
                if (tp->mcfg == CFG_METHOD_5)
                        RTL_W32(Offset_7Ch, 0x000FFF00);
                else if (tp->mcfg == CFG_METHOD_6)
                        RTL_W32(Offset_7Ch, 0x003FFF00);
        }
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Polling 'interrupt' - used by things like netconsole to send skbs
 * without having to re-enable interrupts. It's not called while
 * the interrupt routine is executing.
 */
static void rtl8169_netpoll(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;

        disable_irq(pdev->irq);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
        rtl8169_interrupt(pdev->irq, dev, NULL);
#else
        rtl8169_interrupt(pdev->irq, dev);
#endif
        enable_irq(pdev->irq);
}
#endif

static void rtl8169_release_board(struct pci_dev *pdev, struct net_device *dev,
                                  void __iomem *ioaddr)
{
        struct rtl8169_private *tp = netdev_priv(dev);

        rtl8169_phy_power_down(dev);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
        /* restore the original MAC address */
        rtl8169_rar_set(tp, dev->perm_addr);
#else
        rtl8169_rar_set(tp, tp->org_mac_addr);
#endif

        iounmap(ioaddr);
        pci_release_regions(pdev);
        pci_disable_device(pdev);
        free_netdev(dev);
}

static int
rtl8169_get_mac_address(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i;


        /* Get MAC address.  FIXME: read EEPROM */
        for (i = 0; i < MAC_ADDR_LEN; i++) {
                dev->dev_addr[i] = RTL_R8(MAC0 + i);
                tp->org_mac_addr[i] = dev->dev_addr[i]; /* keep the original MAC address */
        }
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
        memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);
#endif
//  memcpy(dev->dev_addr, dev->dev_addr, dev->addr_len);

        return 0;
}

/**
 * rtl8169_set_mac_address - Change the Ethernet Address of the NIC
 * @dev: network interface device structure
 * @p:   pointer to an address structure
 *
 * Return 0 on success, negative on failure
 **/
static int
rtl8169_set_mac_address(struct net_device *dev,
                        void *p)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        struct sockaddr *addr = p;
        unsigned long flags;

        if (!is_valid_ether_addr(addr->sa_data))
                return -EADDRNOTAVAIL;

        spin_lock_irqsave(&tp->lock, flags);

        memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

        rtl8169_rar_set(tp, dev->dev_addr);

        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}

/******************************************************************************
 * rtl8169_rar_set - Puts an ethernet address into a receive address register.
 *
 * tp - The private data structure for driver
 * addr - Address to put into receive address register
 *****************************************************************************/
void
rtl8169_rar_set(struct rtl8169_private *tp,
                uint8_t *addr)
{
        void __iomem *ioaddr = tp->mmio_addr;
        uint32_t rar_low = 0;
        uint32_t rar_high = 0;

        rar_low = ((uint32_t) addr[0] |
                   ((uint32_t) addr[1] << 8) |
                   ((uint32_t) addr[2] << 16) |
                   ((uint32_t) addr[3] << 24));

        rar_high = ((uint32_t) addr[4] |
                    ((uint32_t) addr[5] << 8));

        RTL_W8(Cfg9346, Cfg9346_Unlock);
        RTL_W32(MAC0, rar_low);
        RTL_W32(MAC4, rar_high);
        RTL_W8(Cfg9346, Cfg9346_Lock);
}

#ifdef ETHTOOL_OPS_COMPAT
static int ethtool_get_settings(struct net_device *dev, void *useraddr)
{
        struct ethtool_cmd cmd = { ETHTOOL_GSET };
        int err;

        if (!ethtool_ops->get_settings)
                return -EOPNOTSUPP;

        err = ethtool_ops->get_settings(dev, &cmd);
        if (err < 0)
                return err;

        if (copy_to_user(useraddr, &cmd, sizeof(cmd)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_settings(struct net_device *dev, void *useraddr)
{
        struct ethtool_cmd cmd;

        if (!ethtool_ops->set_settings)
                return -EOPNOTSUPP;

        if (copy_from_user(&cmd, useraddr, sizeof(cmd)))
                return -EFAULT;

        return ethtool_ops->set_settings(dev, &cmd);
}

static int ethtool_get_drvinfo(struct net_device *dev, void *useraddr)
{
        struct ethtool_drvinfo info;
        struct ethtool_ops *ops = ethtool_ops;

        if (!ops->get_drvinfo)
                return -EOPNOTSUPP;

        memset(&info, 0, sizeof(info));
        info.cmd = ETHTOOL_GDRVINFO;
        ops->get_drvinfo(dev, &info);

        if (ops->self_test_count)
                info.testinfo_len = ops->self_test_count(dev);
        if (ops->get_stats_count)
                info.n_stats = ops->get_stats_count(dev);
        if (ops->get_regs_len)
                info.regdump_len = ops->get_regs_len(dev);
        if (ops->get_eeprom_len)
                info.eedump_len = ops->get_eeprom_len(dev);

        if (copy_to_user(useraddr, &info, sizeof(info)))
                return -EFAULT;
        return 0;
}

static int ethtool_get_regs(struct net_device *dev, char *useraddr)
{
        struct ethtool_regs regs;
        struct ethtool_ops *ops = ethtool_ops;
        void *regbuf;
        int reglen, ret;

        if (!ops->get_regs || !ops->get_regs_len)
                return -EOPNOTSUPP;

        if (copy_from_user(&regs, useraddr, sizeof(regs)))
                return -EFAULT;

        reglen = ops->get_regs_len(dev);
        if (regs.len > reglen)
                regs.len = reglen;

        regbuf = kmalloc(reglen, GFP_USER);
        if (!regbuf)
                return -ENOMEM;

        ops->get_regs(dev, &regs, regbuf);

        ret = -EFAULT;
        if (copy_to_user(useraddr, &regs, sizeof(regs)))
                goto out;
        useraddr += offsetof(struct ethtool_regs, data);
        if (copy_to_user(useraddr, regbuf, reglen))
                goto out;
        ret = 0;

out:
        kfree(regbuf);
        return ret;
}

static int ethtool_get_wol(struct net_device *dev, char *useraddr)
{
        struct ethtool_wolinfo wol = { ETHTOOL_GWOL };

        if (!ethtool_ops->get_wol)
                return -EOPNOTSUPP;

        ethtool_ops->get_wol(dev, &wol);

        if (copy_to_user(useraddr, &wol, sizeof(wol)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_wol(struct net_device *dev, char *useraddr)
{
        struct ethtool_wolinfo wol;

        if (!ethtool_ops->set_wol)
                return -EOPNOTSUPP;

        if (copy_from_user(&wol, useraddr, sizeof(wol)))
                return -EFAULT;

        return ethtool_ops->set_wol(dev, &wol);
}

static int ethtool_get_msglevel(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GMSGLVL };

        if (!ethtool_ops->get_msglevel)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_msglevel(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_msglevel(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_msglevel)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        ethtool_ops->set_msglevel(dev, edata.data);
        return 0;
}

static int ethtool_nway_reset(struct net_device *dev)
{
        if (!ethtool_ops->nway_reset)
                return -EOPNOTSUPP;

        return ethtool_ops->nway_reset(dev);
}

static int ethtool_get_link(struct net_device *dev, void *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GLINK };

        if (!ethtool_ops->get_link)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_link(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_get_eeprom(struct net_device *dev, void *useraddr)
{
        struct ethtool_eeprom eeprom;
        struct ethtool_ops *ops = ethtool_ops;
        u8 *data;
        int ret;

        if (!ops->get_eeprom || !ops->get_eeprom_len)
                return -EOPNOTSUPP;

        if (copy_from_user(&eeprom, useraddr, sizeof(eeprom)))
                return -EFAULT;

        /* Check for wrap and zero */
        if (eeprom.offset + eeprom.len <= eeprom.offset)
                return -EINVAL;

        /* Check for exceeding total eeprom len */
        if (eeprom.offset + eeprom.len > ops->get_eeprom_len(dev))
                return -EINVAL;

        data = kmalloc(eeprom.len, GFP_USER);
        if (!data)
                return -ENOMEM;

        ret = -EFAULT;
        if (copy_from_user(data, useraddr + sizeof(eeprom), eeprom.len))
                goto out;

        ret = ops->get_eeprom(dev, &eeprom, data);
        if (ret)
                goto out;

        ret = -EFAULT;
        if (copy_to_user(useraddr, &eeprom, sizeof(eeprom)))
                goto out;
        if (copy_to_user(useraddr + sizeof(eeprom), data, eeprom.len))
                goto out;
        ret = 0;

out:
        kfree(data);
        return ret;
}

static int ethtool_set_eeprom(struct net_device *dev, void *useraddr)
{
        struct ethtool_eeprom eeprom;
        struct ethtool_ops *ops = ethtool_ops;
        u8 *data;
        int ret;

        if (!ops->set_eeprom || !ops->get_eeprom_len)
                return -EOPNOTSUPP;

        if (copy_from_user(&eeprom, useraddr, sizeof(eeprom)))
                return -EFAULT;

        /* Check for wrap and zero */
        if (eeprom.offset + eeprom.len <= eeprom.offset)
                return -EINVAL;

        /* Check for exceeding total eeprom len */
        if (eeprom.offset + eeprom.len > ops->get_eeprom_len(dev))
                return -EINVAL;

        data = kmalloc(eeprom.len, GFP_USER);
        if (!data)
                return -ENOMEM;

        ret = -EFAULT;
        if (copy_from_user(data, useraddr + sizeof(eeprom), eeprom.len))
                goto out;

        ret = ops->set_eeprom(dev, &eeprom, data);
        if (ret)
                goto out;

        if (copy_to_user(useraddr + sizeof(eeprom), data, eeprom.len))
                ret = -EFAULT;

out:
        kfree(data);
        return ret;
}

static int ethtool_get_coalesce(struct net_device *dev, void *useraddr)
{
        struct ethtool_coalesce coalesce = { ETHTOOL_GCOALESCE };

        if (!ethtool_ops->get_coalesce)
                return -EOPNOTSUPP;

        ethtool_ops->get_coalesce(dev, &coalesce);

        if (copy_to_user(useraddr, &coalesce, sizeof(coalesce)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_coalesce(struct net_device *dev, void *useraddr)
{
        struct ethtool_coalesce coalesce;

        if (!ethtool_ops->get_coalesce)
                return -EOPNOTSUPP;

        if (copy_from_user(&coalesce, useraddr, sizeof(coalesce)))
                return -EFAULT;

        return ethtool_ops->set_coalesce(dev, &coalesce);
}

static int ethtool_get_ringparam(struct net_device *dev, void *useraddr)
{
        struct ethtool_ringparam ringparam = { ETHTOOL_GRINGPARAM };

        if (!ethtool_ops->get_ringparam)
                return -EOPNOTSUPP;

        ethtool_ops->get_ringparam(dev, &ringparam);

        if (copy_to_user(useraddr, &ringparam, sizeof(ringparam)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_ringparam(struct net_device *dev, void *useraddr)
{
        struct ethtool_ringparam ringparam;

        if (!ethtool_ops->get_ringparam)
                return -EOPNOTSUPP;

        if (copy_from_user(&ringparam, useraddr, sizeof(ringparam)))
                return -EFAULT;

        return ethtool_ops->set_ringparam(dev, &ringparam);
}

static int ethtool_get_pauseparam(struct net_device *dev, void *useraddr)
{
        struct ethtool_pauseparam pauseparam = { ETHTOOL_GPAUSEPARAM };

        if (!ethtool_ops->get_pauseparam)
                return -EOPNOTSUPP;

        ethtool_ops->get_pauseparam(dev, &pauseparam);

        if (copy_to_user(useraddr, &pauseparam, sizeof(pauseparam)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_pauseparam(struct net_device *dev, void *useraddr)
{
        struct ethtool_pauseparam pauseparam;

        if (!ethtool_ops->get_pauseparam)
                return -EOPNOTSUPP;

        if (copy_from_user(&pauseparam, useraddr, sizeof(pauseparam)))
                return -EFAULT;

        return ethtool_ops->set_pauseparam(dev, &pauseparam);
}

static int ethtool_get_rx_csum(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GRXCSUM };

        if (!ethtool_ops->get_rx_csum)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_rx_csum(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_rx_csum(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_rx_csum)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        ethtool_ops->set_rx_csum(dev, edata.data);
        return 0;
}

static int ethtool_get_tx_csum(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GTXCSUM };

        if (!ethtool_ops->get_tx_csum)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_tx_csum(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_tx_csum(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_tx_csum)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        return ethtool_ops->set_tx_csum(dev, edata.data);
}

static int ethtool_get_sg(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GSG };

        if (!ethtool_ops->get_sg)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_sg(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_sg(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_sg)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        return ethtool_ops->set_sg(dev, edata.data);
}

static int ethtool_get_tso(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GTSO };

        if (!ethtool_ops->get_tso)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_tso(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_tso(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_tso)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        return ethtool_ops->set_tso(dev, edata.data);
}

static int ethtool_self_test(struct net_device *dev, char *useraddr)
{
        struct ethtool_test test;
        struct ethtool_ops *ops = ethtool_ops;
        u64 *data;
        int ret;

        if (!ops->self_test || !ops->self_test_count)
                return -EOPNOTSUPP;

        if (copy_from_user(&test, useraddr, sizeof(test)))
                return -EFAULT;

        test.len = ops->self_test_count(dev);
        data = kmalloc(test.len * sizeof(u64), GFP_USER);
        if (!data)
                return -ENOMEM;

        ops->self_test(dev, &test, data);

        ret = -EFAULT;
        if (copy_to_user(useraddr, &test, sizeof(test)))
                goto out;
        useraddr += sizeof(test);
        if (copy_to_user(useraddr, data, test.len * sizeof(u64)))
                goto out;
        ret = 0;

out:
        kfree(data);
        return ret;
}

static int ethtool_get_strings(struct net_device *dev, void *useraddr)
{
        struct ethtool_gstrings gstrings;
        struct ethtool_ops *ops = ethtool_ops;
        u8 *data;
        int ret;

        if (!ops->get_strings)
                return -EOPNOTSUPP;

        if (copy_from_user(&gstrings, useraddr, sizeof(gstrings)))
                return -EFAULT;

        switch (gstrings.string_set) {
        case ETH_SS_TEST:
                if (!ops->self_test_count)
                        return -EOPNOTSUPP;
                gstrings.len = ops->self_test_count(dev);
                break;
        case ETH_SS_STATS:
                if (!ops->get_stats_count)
                        return -EOPNOTSUPP;
                gstrings.len = ops->get_stats_count(dev);
                break;
        default:
                return -EINVAL;
        }

        data = kmalloc(gstrings.len * ETH_GSTRING_LEN, GFP_USER);
        if (!data)
                return -ENOMEM;

        ops->get_strings(dev, gstrings.string_set, data);

        ret = -EFAULT;
        if (copy_to_user(useraddr, &gstrings, sizeof(gstrings)))
                goto out;
        useraddr += sizeof(gstrings);
        if (copy_to_user(useraddr, data, gstrings.len * ETH_GSTRING_LEN))
                goto out;
        ret = 0;

out:
        kfree(data);
        return ret;
}

static int ethtool_phys_id(struct net_device *dev, void *useraddr)
{
        struct ethtool_value id;

        if (!ethtool_ops->phys_id)
                return -EOPNOTSUPP;

        if (copy_from_user(&id, useraddr, sizeof(id)))
                return -EFAULT;

        return ethtool_ops->phys_id(dev, id.data);
}

static int ethtool_get_stats(struct net_device *dev, void *useraddr)
{
        struct ethtool_stats stats;
        struct ethtool_ops *ops = ethtool_ops;
        u64 *data;
        int ret;

        if (!ops->get_ethtool_stats || !ops->get_stats_count)
                return -EOPNOTSUPP;

        if (copy_from_user(&stats, useraddr, sizeof(stats)))
                return -EFAULT;

        stats.n_stats = ops->get_stats_count(dev);
        data = kmalloc(stats.n_stats * sizeof(u64), GFP_USER);
        if (!data)
                return -ENOMEM;

        ops->get_ethtool_stats(dev, &stats, data);

        ret = -EFAULT;
        if (copy_to_user(useraddr, &stats, sizeof(stats)))
                goto out;
        useraddr += sizeof(stats);
        if (copy_to_user(useraddr, data, stats.n_stats * sizeof(u64)))
                goto out;
        ret = 0;

out:
        kfree(data);
        return ret;
}

static int ethtool_ioctl(struct ifreq *ifr)
{
        struct net_device *dev = __dev_get_by_name(ifr->ifr_name);
        void *useraddr = (void *) ifr->ifr_data;
        u32 ethcmd;

        /*
         * XXX: This can be pushed down into the ethtool_* handlers that
         * need it.  Keep existing behaviour for the moment.
         */
        if (!capable(CAP_NET_ADMIN))
                return -EPERM;

        if (!dev || !netif_device_present(dev))
                return -ENODEV;

        if (copy_from_user(&ethcmd, useraddr, sizeof (ethcmd)))
                return -EFAULT;

        switch (ethcmd) {
        case ETHTOOL_GSET:
                return ethtool_get_settings(dev, useraddr);
        case ETHTOOL_SSET:
                return ethtool_set_settings(dev, useraddr);
        case ETHTOOL_GDRVINFO:
                return ethtool_get_drvinfo(dev, useraddr);
        case ETHTOOL_GREGS:
                return ethtool_get_regs(dev, useraddr);
        case ETHTOOL_GWOL:
                return ethtool_get_wol(dev, useraddr);
        case ETHTOOL_SWOL:
                return ethtool_set_wol(dev, useraddr);
        case ETHTOOL_GMSGLVL:
                return ethtool_get_msglevel(dev, useraddr);
        case ETHTOOL_SMSGLVL:
                return ethtool_set_msglevel(dev, useraddr);
        case ETHTOOL_NWAY_RST:
                return ethtool_nway_reset(dev);
        case ETHTOOL_GLINK:
                return ethtool_get_link(dev, useraddr);
        case ETHTOOL_GEEPROM:
                return ethtool_get_eeprom(dev, useraddr);
        case ETHTOOL_SEEPROM:
                return ethtool_set_eeprom(dev, useraddr);
        case ETHTOOL_GCOALESCE:
                return ethtool_get_coalesce(dev, useraddr);
        case ETHTOOL_SCOALESCE:
                return ethtool_set_coalesce(dev, useraddr);
        case ETHTOOL_GRINGPARAM:
                return ethtool_get_ringparam(dev, useraddr);
        case ETHTOOL_SRINGPARAM:
                return ethtool_set_ringparam(dev, useraddr);
        case ETHTOOL_GPAUSEPARAM:
                return ethtool_get_pauseparam(dev, useraddr);
        case ETHTOOL_SPAUSEPARAM:
                return ethtool_set_pauseparam(dev, useraddr);
        case ETHTOOL_GRXCSUM:
                return ethtool_get_rx_csum(dev, useraddr);
        case ETHTOOL_SRXCSUM:
                return ethtool_set_rx_csum(dev, useraddr);
        case ETHTOOL_GTXCSUM:
                return ethtool_get_tx_csum(dev, useraddr);
        case ETHTOOL_STXCSUM:
                return ethtool_set_tx_csum(dev, useraddr);
        case ETHTOOL_GSG:
                return ethtool_get_sg(dev, useraddr);
        case ETHTOOL_SSG:
                return ethtool_set_sg(dev, useraddr);
        case ETHTOOL_GTSO:
                return ethtool_get_tso(dev, useraddr);
        case ETHTOOL_STSO:
                return ethtool_set_tso(dev, useraddr);
        case ETHTOOL_TEST:
                return ethtool_self_test(dev, useraddr);
        case ETHTOOL_GSTRINGS:
                return ethtool_get_strings(dev, useraddr);
        case ETHTOOL_PHYS_ID:
                return ethtool_phys_id(dev, useraddr);
        case ETHTOOL_GSTATS:
                return ethtool_get_stats(dev, useraddr);
        default:
                return -EOPNOTSUPP;
        }

        return -EOPNOTSUPP;
}
#endif //ETHTOOL_OPS_COMPAT

static int rtl8169_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        struct mii_ioctl_data *data = if_mii(ifr);
        unsigned long flags;

        if (!netif_running(dev))
                return -ENODEV;

        switch (cmd) {
        case SIOCGMIIPHY:
                data->phy_id = 32; /* Internal PHY */
                return 0;

        case SIOCGMIIREG:
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1f, 0x0000);
                data->val_out = mdio_read(tp, data->reg_num & 0x1f);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                return 0;

        case SIOCSMIIREG:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;
                spin_lock_irqsave(&tp->phy_lock, flags);
                mdio_write(tp, 0x1f, 0x0000);
                mdio_write(tp, data->reg_num & 0x1f, data->val_in);
                spin_unlock_irqrestore(&tp->phy_lock, flags);
                return 0;
#ifdef ETHTOOL_OPS_COMPAT
        case SIOCETHTOOL:
                return ethtool_ioctl(ifr);
#endif
        default:
                return -EOPNOTSUPP;
        }
}

static void rtl8169_phy_power_up(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        unsigned long flags;


        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1F, 0x0000);
        mdio_write(tp, 0x0E, 0x0000);
        mdio_write(tp, MII_BMCR, BMCR_ANENABLE);
        spin_unlock_irqrestore(&tp->phy_lock, flags);
}

static void rtl8169_phy_power_down(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->phy_lock, flags);
        mdio_write(tp, 0x1F, 0x0000);
        mdio_write(tp, MII_BMCR, BMCR_PDOWN | BMCR_ANENABLE);
        spin_unlock_irqrestore(&tp->phy_lock, flags);
}

static void
rtl8169_esd_timer(unsigned long __opaque)
{
        struct net_device *dev = (struct net_device *)__opaque;
        struct rtl8169_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;
        struct timer_list *timer = &tp->esd_timer;
        unsigned long timeout = RTL8169_ESD_TIMEOUT;
        u8 cmd;
        u8 cls;
        u16 io_base_l;
        u16 io_base_h;
        u16 mem_base_l;
        u16 mem_base_h;
        u8 ilr;
        u16 resv_0x20_l;
        u16 resv_0x20_h;
        u16 resv_0x24_l;
        u16 resv_0x24_h;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);

        tp->esd_flag = 0;

        pci_read_config_byte(pdev, PCI_COMMAND, &cmd);
        if (cmd != tp->pci_cfg_space.cmd) {
                pci_write_config_byte(pdev, PCI_COMMAND, tp->pci_cfg_space.cmd);
                tp->esd_flag |= BIT_0;
        }

        pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE, &cls);
        if (cls != tp->pci_cfg_space.cls) {
                pci_write_config_byte(pdev, PCI_CACHE_LINE_SIZE, tp->pci_cfg_space.cls);
                tp->esd_flag |= BIT_1;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_0, &io_base_l);
        if (io_base_l != tp->pci_cfg_space.io_base_l) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_0, tp->pci_cfg_space.io_base_l);
                tp->esd_flag |= BIT_2;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_0 + 2, &io_base_h);
        if (io_base_h != tp->pci_cfg_space.io_base_h) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_0 + 2, tp->pci_cfg_space.io_base_h);
                tp->esd_flag |= BIT_3;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_1, &mem_base_l);
        if (mem_base_l != tp->pci_cfg_space.mem_base_l) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_1, tp->pci_cfg_space.mem_base_l);
                tp->esd_flag |= BIT_4;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_1 + 2, &mem_base_h);
        if (mem_base_h != tp->pci_cfg_space.mem_base_h) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_1 + 2, tp->pci_cfg_space.mem_base_h);
                tp->esd_flag |= BIT_5;
        }

        pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &ilr);
        if (ilr != tp->pci_cfg_space.ilr) {
                pci_write_config_byte(pdev, PCI_INTERRUPT_LINE, tp->pci_cfg_space.ilr);
                tp->esd_flag |= BIT_6;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_4, &resv_0x20_l);
        if (resv_0x20_l != tp->pci_cfg_space.resv_0x20_l) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_4, tp->pci_cfg_space.resv_0x20_l);
                tp->esd_flag |= BIT_7;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_4 + 2, &resv_0x20_h);
        if (resv_0x20_h != tp->pci_cfg_space.resv_0x20_h) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_4 + 2, tp->pci_cfg_space.resv_0x20_h);
                tp->esd_flag |= BIT_8;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_5, &resv_0x24_l);
        if (resv_0x24_l != tp->pci_cfg_space.resv_0x24_l) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_5, tp->pci_cfg_space.resv_0x24_l);
                tp->esd_flag |= BIT_9;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_5 + 2, &resv_0x24_h);
        if (resv_0x24_h != tp->pci_cfg_space.resv_0x24_h) {
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_5 + 2, tp->pci_cfg_space.resv_0x24_h);
                tp->esd_flag |= BIT_10;
        }

        if (tp->esd_flag != 0) {
                netif_stop_queue(dev);
                netif_carrier_off(dev);
                rtl8169_hw_reset(dev);
                rtl8169_tx_clear(tp);
                rtl8169_rx_clear(tp);
                rtl8169_init_ring(dev);
                rtl8169_powerup_pll(dev);
                rtl8169_hw_phy_config(dev);
                rtl8169_hw_start(dev);
                rtl8169_set_speed(dev, tp->autoneg, tp->speed, tp->duplex);
                tp->esd_flag = 0;
        }

        spin_unlock_irqrestore(&tp->lock, flags);

        mod_timer(timer, jiffies + timeout);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
static const struct net_device_ops rtl8169_netdev_ops = {
        .ndo_open		= rtl8169_open,
        .ndo_stop		= rtl8169_close,
        .ndo_get_stats		= rtl8169_get_stats,
        .ndo_start_xmit		= rtl8169_start_xmit,
        .ndo_tx_timeout		= rtl8169_tx_timeout,
        .ndo_change_mtu		= rtl8169_change_mtu,
        .ndo_set_mac_address	= rtl8169_set_mac_address,
        .ndo_do_ioctl		= rtl8169_ioctl,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
        .ndo_set_multicast_list	= rtl8169_set_rx_mode,
#else
        .ndo_set_rx_mode	= rtl8169_set_rx_mode,
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
#ifdef CONFIG_R8169_VLAN
        .ndo_vlan_rx_register	= rtl8169_vlan_rx_register,
#endif
#else
        .ndo_fix_features	= rtl8169_fix_features,
        .ndo_set_features	= rtl8169_set_features,
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
        .ndo_poll_controller	= rtl8169_netpoll,
#endif
};
#endif //HAVE_NET_DEVICE_OPS

static int __devinit
rtl8169_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
        const unsigned int region = rtl_cfg_info[ent->driver_data].region;
        struct rtl8169_private *tp;
        struct net_device *dev;
        void __iomem *ioaddr;
        unsigned int pm_cap;
        int i, rc;
        static int board_idx = -1;

        board_idx++;

        if (netif_msg_drv(&debug)) {
                printk(KERN_INFO "%s Gigabit Ethernet driver %s loaded\n",
                       MODULENAME, RTL8169_VERSION);
        }

        dev = alloc_etherdev(sizeof (*tp));
        if (!dev) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_drv(&debug))
                        dev_err(&pdev->dev, "unable to alloc new ethernet\n");
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
                rc = -ENOMEM;
                goto out;
        }

        SET_MODULE_OWNER(dev);
        SET_NETDEV_DEV(dev, &pdev->dev);
        tp = netdev_priv(dev);
        tp->dev = dev;
        tp->msg_enable = netif_msg_init(debug.msg_enable, R8169_MSG_DEFAULT);

        /* enable device (incl. PCI PM wakeup and hotplug setup) */
        rc = pci_enable_device(pdev);
        if (rc < 0) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_err(&pdev->dev, "enable failure\n");
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
                goto err_out_free_dev_1;
        }

        rc = pci_set_mwi(pdev);
        if (rc < 0)
                goto err_out_disable_2;

        /* save power state before pci_enable_device overwrites it */
        pm_cap = pci_find_capability(pdev, PCI_CAP_ID_PM);
        if (pm_cap) {
                u16 pwr_command, acpi_idle_state;

                pci_read_config_word(pdev, pm_cap + PCI_PM_CTRL, &pwr_command);
                acpi_idle_state = pwr_command & PCI_PM_CTRL_STATE_MASK;
        } else {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp)) {
                        dev_err(&pdev->dev, "PowerManagement capability not found.\n");
                }
#else
                printk("PowerManagement capability not found.\n");
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
        }

        /* make sure PCI base addr 1 is MMIO */
        if (!(pci_resource_flags(pdev, region) & IORESOURCE_MEM)) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp)) {
                        dev_err(&pdev->dev, "region #%d not an MMIO resource, aborting\n", region);
                }
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
                rc = -ENODEV;
                goto err_out_mwi_3;
        }

        /* check for weird/broken PCI region reporting */
        if (pci_resource_len(pdev, region) < R8169_REGS_SIZE) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp)) {
                        dev_err(&pdev->dev, "Invalid PCI region size(s), aborting\n");
                }
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
                rc = -ENODEV;
                goto err_out_mwi_3;
        }

        rc = pci_request_regions(pdev, MODULENAME);
        if (rc < 0) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_err(&pdev->dev, "could not request regions.\n");
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
                goto err_out_mwi_3;
        }

        tp->cp_cmd = PCIMulRW | RxChkSum;

        if ((sizeof(dma_addr_t) > 4) &&
            !pci_set_dma_mask(pdev, DMA_64BIT_MASK) && use_dac) {
                tp->cp_cmd |= PCIDAC;
                dev->features |= NETIF_F_HIGHDMA;
        } else {
                rc = pci_set_dma_mask(pdev, DMA_32BIT_MASK);
                if (rc < 0) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                        if (netif_msg_probe(tp)) {
                                dev_err(&pdev->dev, "DMA configuration failed.\n");
                        }
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
                        goto err_out_free_res_4;
                }
        }

        pci_set_master(pdev);

        /* ioremap MMIO region */
        ioaddr = ioremap(pci_resource_start(pdev, region), R8169_REGS_SIZE);
        if (!ioaddr) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_err(&pdev->dev, "cannot remap MMIO, aborting\n");
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
                rc = -EIO;
                goto err_out_free_res_4;
        }

        /* Identify chip attached to board */
        rtl8169_get_mac_version(tp, ioaddr);

        rtl8169_print_mac_version(tp);

        for (i = ARRAY_SIZE(rtl_chip_info) - 1; i >= 0; i--) {
                if (tp->mcfg == rtl_chip_info[i].mcfg)
                        break;
        }
        if (i < 0) {
                /* Unknown chip: assume array element #0, original RTL-8169 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp)) {
                        dev_printk(KERN_DEBUG, &pdev->dev, "unknown chip version, assuming %s\n", rtl_chip_info[0].name);
                }
#else
                printk("Realtek unknown chip version, assuming %s\n", rtl_chip_info[0].name);
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
                i++;
        }
        tp->chipset = i;

        RTL_W8(Cfg9346, Cfg9346_Unlock);
        RTL_W8(Config1, RTL_R8(Config1) | PMEnable);
        RTL_W8(Config5, RTL_R8(Config5) & PMEStatus);
        RTL_W8(Cfg9346, Cfg9346_Lock);

        tp->set_speed = rtl8169_set_speed_xmii;
        tp->get_settings = rtl8169_gset_xmii;
        tp->phy_reset_enable = rtl8169_xmii_reset_enable;
        tp->phy_reset_pending = rtl8169_xmii_reset_pending;
        tp->link_ok = rtl8169_xmii_link_ok;

        RTL_W8(Cfg9346, Cfg9346_Unlock);
        RTL_W8(Cfg9346, Cfg9346_Lock);

        RTL_NET_DEVICE_OPS(rtl8169_netdev_ops);

        SET_ETHTOOL_OPS(dev, &rtl8169_ethtool_ops);

        dev->watchdog_timeo = RTL8169_TX_TIMEOUT;
        dev->irq = pdev->irq;
        dev->base_addr = (unsigned long) ioaddr;

#ifdef CONFIG_R8169_NAPI
        RTL_NAPI_CONFIG(dev, tp, rtl8169_poll, R8169_NAPI_WEIGHT);
#endif

#ifdef CONFIG_R8169_VLAN
        dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
        dev->vlan_rx_kill_vid = rtl8169_vlan_rx_kill_vid;
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
#endif //CONFIG_R8169_VLAN

        dev->features |= NETIF_F_IP_CSUM;
        tp->cp_cmd |= RTL_R16(CPlusCmd);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        tp->cp_cmd |= RxChkSum;
#else
        dev->features |= NETIF_F_RXCSUM;
        dev->hw_features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_TSO |
                           NETIF_F_RXCSUM | NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
        dev->vlan_features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_TSO |
                             NETIF_F_HIGHDMA;
#endif

        tp->intr_mask = rtl8169_intr_mask;
        tp->pci_dev = pdev;
        tp->mmio_addr = ioaddr;
        tp->align = rtl_cfg_info[ent->driver_data].align;

        spin_lock_init(&tp->lock);
        spin_lock_init(&tp->phy_lock);

        rtl8169_hw_init(dev);

        rtl8169_hw_reset(dev);

        rtl8169_get_mac_address(dev);

        rtl8169_get_phy_version(tp);

        rtl8169_print_phy_version(tp);

        pci_set_drvdata(pdev, dev);

        rc = register_netdev(dev);
        if (rc < 0)
                goto err_out_unmap_5;

        printk(KERN_INFO "%s: This product is covered by one or more of the following patents: US6,570,884, US6,115,776, and US6,327,625.\n", MODULENAME);

        if (netif_msg_probe(tp)) {
                printk(KERN_INFO "%s: %s at 0x%lx, "
                       "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, "
                       "IRQ %d\n",
                       dev->name,
                       rtl_chip_info[tp->chipset].name,
                       dev->base_addr,
                       dev->dev_addr[0], dev->dev_addr[1],
                       dev->dev_addr[2], dev->dev_addr[3],
                       dev->dev_addr[4], dev->dev_addr[5], dev->irq);
        }

        rtl8169_link_option(board_idx, (u8*)&autoneg, (u16*)&speed, (u8*)&duplex);

        tp->autoneg = autoneg;
        tp->speed = speed;
        tp->duplex = duplex;

out:
        return rc;

err_out_unmap_5:
#ifdef  CONFIG_R8169_NAPI
        RTL_NAPI_DEL(tp);
#endif
        iounmap(ioaddr);
err_out_free_res_4:
        pci_release_regions(pdev);
err_out_mwi_3:
        pci_clear_mwi(pdev);
err_out_disable_2:
        pci_disable_device(pdev);
err_out_free_dev_1:
        free_netdev(dev);
        goto out;
}

static void __devexit
rtl8169_remove_one(struct pci_dev *pdev)
{
        struct net_device *dev = pci_get_drvdata(pdev);
        struct rtl8169_private *tp = netdev_priv(dev);

        assert(dev != NULL);
        assert(tp != NULL);

#ifdef  CONFIG_R8169_NAPI
        RTL_NAPI_DEL(tp);
#endif

        unregister_netdev(dev);
        rtl8169_release_board(pdev, dev, tp->mmio_addr);
        pci_set_drvdata(pdev, NULL);
}

static void rtl8169_set_rxbufsize(struct rtl8169_private *tp,
                                  struct net_device *dev)
{
        unsigned int mtu = dev->mtu;

        tp->rx_buf_sz = (mtu > ETH_DATA_LEN) ? mtu + ETH_HLEN + 8 : RX_BUF_SIZE;
}

static int rtl8169_open(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;
        int retval;

        rtl8169_set_rxbufsize(tp, dev);

        retval = -ENOMEM;

        /*
         * Rx and Tx descriptors needs 256 bytes alignment.
         * pci_alloc_consistent provides more.
         */
        tp->TxDescArray = pci_alloc_consistent(pdev, R8169_TX_RING_BYTES,
                                               &tp->TxPhyAddr);
        if (!tp->TxDescArray)
                goto err_free_irq;

        tp->RxDescArray = pci_alloc_consistent(pdev, R8169_RX_RING_BYTES,
                                               &tp->RxPhyAddr);
        if (!tp->RxDescArray)
                goto err_free_tx;

        retval = rtl8169_init_ring(dev);
        if (retval < 0)
                goto err_free_rx;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
        INIT_WORK(&tp->task, NULL, dev);
#else
        INIT_DELAYED_WORK(&tp->task, NULL);
#endif

#ifdef	CONFIG_R8169_NAPI
        RTL_NAPI_ENABLE(dev, &tp->napi);
#endif

        rtl8169_hw_init(dev);

        rtl8169_hw_reset(dev);

        rtl8169_phy_power_up(dev);

        rtl8169_hw_phy_config(dev);

        rtl8169_hw_start(dev);

        if (tp->esd_flag == 0) {
                rtl8169_request_timer(dev);
                rtl8169_request_esd_timer(dev);
        }

        rtl8169_check_link_status(dev, tp, tp->mmio_addr);

        rtl8169_set_speed(dev, tp->autoneg, tp->speed, tp->duplex);

        retval = request_irq(dev->irq, rtl8169_interrupt, SA_SHIRQ, dev->name, dev);

        if (retval < 0)
                goto err_free_rx;

out:
        return retval;

err_free_rx:
        pci_free_consistent(pdev, R8169_RX_RING_BYTES, tp->RxDescArray,
                            tp->RxPhyAddr);
err_free_tx:
        pci_free_consistent(pdev, R8169_TX_RING_BYTES, tp->TxDescArray,
                            tp->TxPhyAddr);
err_free_irq:
        free_irq(dev->irq, dev);
        goto out;
}

static void rtl8169_nic_reset(void __iomem *ioaddr)
{
        int i;

        RTL_W32(RxConfig, (RX_DMA_BURST << RxCfgDMAShift));

        mdelay(1);

        /* Soft reset the chip. */
        RTL_W8(ChipCmd, CmdReset);

        /* Check that the chip has finished the reset. */
        for (i = 1000; i > 0; i--) {
                if ((RTL_R8(ChipCmd) & CmdReset) == 0)
                        break;

                udelay(100);
        }
}

static void
rtl8169_hw_clear_timer_int(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        RTL_W32(TimeInt0, 0x0000);
}

static void rtl8169_hw_reset(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        /* Disable interrupts */
        rtl8169_irq_mask_and_ack(ioaddr);

        rtl8169_hw_clear_timer_int(dev);

        /* Reset the chipset */
        rtl8169_nic_reset(ioaddr);
}

static void
rtl8169_hw_set_rx_packet_filter(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u32 mc_filter[2];	/* Multicast hash filter */
        int rx_mode;
        u32 tmp = 0;

        if (dev->flags & IFF_PROMISC) {
                /* Unconditionally log net taps. */

                if (netif_msg_link(tp)) {
                        printk(KERN_NOTICE "%s: Promiscuous mode enabled.\n",
                               dev->name);
                }
                rx_mode =
                        AcceptBroadcast | AcceptMulticast | AcceptMyPhys |
                        AcceptAllPhys;
                mc_filter[1] = mc_filter[0] = 0xffffffff;
        } else if ((netdev_mc_count(dev) > multicast_filter_limit)
                   || (dev->flags & IFF_ALLMULTI)) {
                /* Too many to filter perfectly -- accept all multicasts. */

                rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
                mc_filter[1] = mc_filter[0] = 0xffffffff;
        } else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
                struct dev_mc_list *mclist;
                int i;

                rx_mode = AcceptBroadcast | AcceptMyPhys;
                mc_filter[1] = mc_filter[0] = 0;
                for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
                     i++, mclist = mclist->next) {
                        int bit_nr = ether_crc(ETH_ALEN, mclist->dmi_addr) >> 26;
                        mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
                        rx_mode |= AcceptMulticast;
                }
#else
                struct netdev_hw_addr *ha;

                rx_mode = AcceptBroadcast | AcceptMyPhys;
                mc_filter[1] = mc_filter[0] = 0;
                netdev_for_each_mc_addr(ha, dev) {
                        int bit_nr = ether_crc(ETH_ALEN, ha->addr) >> 26;
                        mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
                        rx_mode |= AcceptMulticast;
                }
#endif
        }

        tmp = rtl8169_rx_config | rx_mode |
              (RTL_R32(RxConfig) & rtl_chip_info[tp->chipset].RxConfigMask);

        RTL_W32(RxConfig, tmp);
        RTL_W32(MAR0 + 0, mc_filter[0]);
        RTL_W32(MAR0 + 4, mc_filter[1]);
}

static void
rtl8169_set_rx_mode(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);

        rtl8169_hw_set_rx_packet_filter(dev);

        spin_unlock_irqrestore(&tp->lock, flags);
}

/**
 *  rtl8169_get_stats - Get rtl8169 read/write statistics
 *  @dev: The Ethernet Device to get statistics for
 *
 *  Get TX/RX statistics for rtl8169
 */
static struct net_device_stats *rtl8169_get_stats(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        if (netif_running(dev)) {
                spin_lock_irqsave(&tp->lock, flags);
                tp->stats.rx_missed_errors += RTL_R32(RxMissed);
                RTL_W32(RxMissed, 0);
                spin_unlock_irqrestore(&tp->lock, flags);
        }

        return &RTLDEV->stats;
}

static void rtl8169_hw_start(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        struct pci_dev *pdev = tp->pci_dev;
        u8 options1, options2;

        RTL_W32(RxConfig, (RX_DMA_BURST << RxCfgDMAShift));

        rtl8169_hw_reset(dev);

        dprintk("Set PCI Latency=0x40\n");
        pci_write_config_byte(tp->pci_dev, PCI_LATENCY_TIMER, 0x40);

        RTL_W8(Cfg9346, Cfg9346_Unlock);

        RTL_W8(Reserved1, Reserved1_data);

        tp->cp_cmd |= PCIMulRW;
        RTL_W16(CPlusCmd, tp->cp_cmd);
        pci_write_config_byte(pdev, PCI_CACHE_LINE_SIZE, 0x08);

        if (tp->mcfg == CFG_METHOD_2)
                tp->cp_cmd |= EnAnaPLL;
        else
                tp->cp_cmd &= ~EnAnaPLL;

        pci_write_config_byte(pdev, PCI_LATENCY_TIMER, 0x40);

        /*
         * Undocumented corner. Supposedly:
         */
        RTL_W16(IntrMitigate, 0x0000);

        /*
         * Magic spell: some iop3xx ARM board needs the TxDescAddrHigh
         * register to be written before TxDescAddrLow to work.
         * Switching from MMIO to I/O access fixes the issue as well.
         */
        RTL_W32(TxDescStartAddrLow, ((u64) tp->TxPhyAddr & DMA_32BIT_MASK));
        RTL_W32(RxDescAddrLow, ((u64) tp->RxPhyAddr & DMA_32BIT_MASK));
        RTL_W32(TxDescStartAddrHigh, ((u64) tp->TxPhyAddr >> 32));
        RTL_W32(RxDescAddrHigh, ((u64) tp->RxPhyAddr >> 32));

        RTL_W32(RxMissed, 0);

        /* no early-rx interrupts */
        RTL_W16(MultiIntr, RTL_R16(MultiIntr) & 0xF000);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        RTL_W16(CPlusCmd, tp->cp_cmd);
#else
        rtl8169_hw_set_features(dev, dev->features);
#endif

        RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

        RTL_W16(RxMaxSize, tp->rx_buf_sz);

        /* Set Rx packet filter */
        rtl8169_hw_set_rx_packet_filter(dev);

        /* Set DMA burst size and Interframe Gap Time */
        rtl8169_set_tx_config(dev);

        RTL_W8(Cfg9346, Cfg9346_Lock);

        rtl8169_hw_clear_timer_int(dev);

        /* Clear the interrupt status. */
        RTL_W16(IntrStatus, 0xFFFF);

        /* Enable all known interrupts by setting the interrupt mask. */
        RTL_W16(IntrMask, rtl8169_intr_mask);

        if (tp->link_ok(dev))
                netif_wake_queue(dev);
        else
                netif_stop_queue(dev);

        if (!tp->pci_cfg_is_read) {
                pci_read_config_byte(pdev, PCI_COMMAND, &tp->pci_cfg_space.cmd);
                pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE, &tp->pci_cfg_space.cls);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_0, &tp->pci_cfg_space.io_base_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_0 + 2, &tp->pci_cfg_space.io_base_h);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_1, &tp->pci_cfg_space.mem_base_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_1 + 2, &tp->pci_cfg_space.mem_base_h);
                pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &tp->pci_cfg_space.ilr);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_4, &tp->pci_cfg_space.resv_0x20_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_4 + 2, &tp->pci_cfg_space.resv_0x20_h);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_5, &tp->pci_cfg_space.resv_0x24_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_5 + 2, &tp->pci_cfg_space.resv_0x24_h);

                tp->pci_cfg_is_read = 1;
        }

        options1 = RTL_R8(Config3);
        options2 = RTL_R8(Config5);

        if ((options1 & LinkUp) || (options1 & MagicPacket) || (options2 & UWF) || (options2 & BWF) || (options2 & MWF))
                tp->wol_enabled = WOL_ENABLED;
        else
                tp->wol_enabled = WOL_DISABLED;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
        device_set_wakeup_enable(&tp->pci_dev->dev, tp->wol_enabled);
#endif

        udelay(10);
}

static int rtl8169_change_mtu(struct net_device *dev, int new_mtu)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        int max_mtu = Jumbo_Frame_7k - ETH_HLEN - 8;
        int ret = 0;
        unsigned long flags;

        if (new_mtu < ETH_ZLEN)
                return -EINVAL;
        else if (new_mtu > max_mtu)
                new_mtu = max_mtu;

        if (!netif_running(dev))
                goto out;

        rtl8169_down(dev);

        spin_lock_irqsave(&tp->lock, flags);

        dev->mtu = new_mtu;

        rtl8169_set_rxbufsize(tp, dev);

        ret = rtl8169_init_ring(dev);

        if (ret < 0) {
                spin_unlock_irqrestore(&tp->lock, flags);
                goto out;
        }

#ifdef CONFIG_R8169_NAPI
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        RTL_NAPI_ENABLE(dev, &tp->napi);
#endif
#endif//CONFIG_R8169_NAPI

        rtl8169_hw_start(dev);
        spin_unlock_irqrestore(&tp->lock, flags);
        rtl8169_set_speed(dev, tp->autoneg, tp->speed, tp->duplex);

        mod_timer(&tp->link_timer, jiffies + RTL8169_PHY_TIMEOUT);
        mod_timer(&tp->esd_timer, jiffies + RTL8169_ESD_TIMEOUT);

out:
        return ret;
}

static inline void rtl8169_make_unusable_by_asic(struct RxDesc *desc)
{
        desc->addr = 0x0badbadbadbadbadull;
        desc->opts1 &= ~cpu_to_le32(DescOwn | RsvdMask);
}

static void rtl8169_free_rx_skb(struct rtl8169_private *tp,
                                struct sk_buff **sk_buff, struct RxDesc *desc)
{
        struct pci_dev *pdev = tp->pci_dev;

        pci_unmap_single(pdev, le64_to_cpu(desc->addr), tp->rx_buf_sz,
                         PCI_DMA_FROMDEVICE);
        dev_kfree_skb(*sk_buff);
        *sk_buff = NULL;
        rtl8169_make_unusable_by_asic(desc);
}

static inline void rtl8169_mark_to_asic(struct RxDesc *desc, u32 rx_buf_sz)
{
        u32 eor = le32_to_cpu(desc->opts1) & RingEnd;

        desc->opts1 = cpu_to_le32(DescOwn | eor | rx_buf_sz);
}

static inline void rtl8169_map_to_asic(struct RxDesc *desc, dma_addr_t mapping,
                                       u32 rx_buf_sz)
{
        desc->addr = cpu_to_le64(mapping);
        wmb();
        rtl8169_mark_to_asic(desc, rx_buf_sz);
}

static int rtl8169_alloc_rx_skb(struct pci_dev *pdev, struct sk_buff **sk_buff,
                                struct RxDesc *desc, int rx_buf_sz,
                                unsigned int align)
{
        struct sk_buff *skb;
        dma_addr_t mapping;
        int ret = 0;

        skb = dev_alloc_skb(rx_buf_sz + align);
        if (!skb)
                goto err_out;

        skb_reserve(skb, align - ((align - 1) & (uintptr_t)skb->data));
        *sk_buff = skb;

        mapping = pci_map_single(pdev, skb->data, rx_buf_sz,
                                 PCI_DMA_FROMDEVICE);

        rtl8169_map_to_asic(desc, mapping, rx_buf_sz);

out:
        return ret;

err_out:
        ret = -ENOMEM;
        rtl8169_make_unusable_by_asic(desc);
        goto out;
}

static void rtl8169_rx_clear(struct rtl8169_private *tp)
{
        int i;

        for (i = 0; i < NUM_RX_DESC; i++) {
                if (tp->Rx_skbuff[i]) {
                        rtl8169_free_rx_skb(tp, tp->Rx_skbuff + i,
                                            tp->RxDescArray + i);
                }
        }
}

static u32 rtl8169_rx_fill(struct rtl8169_private *tp, struct net_device *dev,
                           u32 start, u32 end)
{
        u32 cur;

        for (cur = start; end - cur > 0; cur++) {
                int ret, i = cur % NUM_RX_DESC;

                if (tp->Rx_skbuff[i])
                        continue;

                ret = rtl8169_alloc_rx_skb(tp->pci_dev, tp->Rx_skbuff + i,
                                           tp->RxDescArray + i, tp->rx_buf_sz, tp->align);
                if (ret < 0)
                        break;
        }
        return cur - start;
}

static inline void rtl8169_mark_as_last_descriptor(struct RxDesc *desc)
{
        desc->opts1 |= cpu_to_le32(RingEnd);
}

static void rtl8169_init_ring_indexes(struct rtl8169_private *tp)
{
        tp->dirty_tx = tp->dirty_rx = tp->cur_tx = tp->cur_rx = 0;
}

static void
rtl8169_tx_desc_init(struct rtl8169_private *tp)
{
        int i = 0;

        memset(tp->TxDescArray, 0x0, NUM_TX_DESC * sizeof(struct TxDesc));

        for (i = 0; i < NUM_TX_DESC; i++) {
                if (i == (NUM_TX_DESC - 1))
                        tp->TxDescArray[i].opts1 = cpu_to_le32(RingEnd);
        }
}

static void
rtl8169_rx_desc_init(struct rtl8169_private *tp)
{
        memset(tp->RxDescArray, 0x0, NUM_RX_DESC * sizeof(struct RxDesc));
}

static int rtl8169_init_ring(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);

        rtl8169_init_ring_indexes(tp);

        memset(tp->tx_skb, 0x0, NUM_TX_DESC * sizeof(struct ring_info));
        memset(tp->Rx_skbuff, 0x0, NUM_RX_DESC * sizeof(struct sk_buff *));

        rtl8169_tx_desc_init(tp);
        rtl8169_rx_desc_init(tp);

        if (rtl8169_rx_fill(tp, dev, 0, NUM_RX_DESC) != NUM_RX_DESC)
                goto err_out;

        rtl8169_mark_as_last_descriptor(tp->RxDescArray + NUM_RX_DESC - 1);

        return 0;

err_out:
        rtl8169_rx_clear(tp);
        return -ENOMEM;
}

static void rtl8169_unmap_tx_skb(struct pci_dev *pdev, struct ring_info *tx_skb,
                                 struct TxDesc *desc)
{
        unsigned int len = tx_skb->len;

        pci_unmap_single(pdev, le64_to_cpu(desc->addr), len, PCI_DMA_TODEVICE);
        desc->opts1 = 0x00;
        desc->opts2 = 0x00;
        desc->addr = 0x00;
        tx_skb->len = 0;
}

static void rtl8169_tx_clear(struct rtl8169_private *tp)
{
        unsigned int i;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
        struct net_device *dev = tp->dev;
#endif

        for (i = tp->dirty_tx; i < tp->dirty_tx + NUM_TX_DESC; i++) {
                unsigned int entry = i % NUM_TX_DESC;
                struct ring_info *tx_skb = tp->tx_skb + entry;
                unsigned int len = tx_skb->len;

                if (len) {
                        struct sk_buff *skb = tx_skb->skb;

                        rtl8169_unmap_tx_skb(tp->pci_dev, tx_skb,
                                             tp->TxDescArray + entry);
                        if (skb) {
                                dev_kfree_skb(skb);
                                tx_skb->skb = NULL;
                        }
                        RTLDEV->stats.tx_dropped++;
                }
        }
        tp->cur_tx = tp->dirty_tx = 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void rtl8169_schedule_work(struct net_device *dev, void (*task)(void *))
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        struct rtl8169_private *tp = netdev_priv(dev);

        INIT_WORK(&tp->task, task, dev);
        schedule_delayed_work(&tp->task, 4);
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
}

static void rtl8169_cancel_schedule_work(struct net_device *dev)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        struct rtl8169_private *tp = netdev_priv(dev);

        cancel_work_sync(&tp->task);
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
}
#else
static void rtl8169_schedule_work(struct net_device *dev, work_func_t task)
{
        struct rtl8169_private *tp = netdev_priv(dev);

        INIT_DELAYED_WORK(&tp->task, task);
        schedule_delayed_work(&tp->task, 4);
}

static void rtl8169_cancel_schedule_work(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);

        cancel_delayed_work_sync(&tp->task);
}
#endif

static void rtl8169_wait_for_quiescence(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        synchronize_irq(dev->irq);

        /* Wait for any pending NAPI task to complete */
#ifdef CONFIG_R8169_NAPI
        RTL_NAPI_DISABLE(dev, &tp->napi);
#endif

        rtl8169_irq_mask_and_ack(ioaddr);

#ifdef CONFIG_R8169_NAPI
        RTL_NAPI_ENABLE(dev, &tp->napi);
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void rtl8169_reinit_task(void *_data)
#else
static void rtl8169_reinit_task(struct work_struct *work)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
        struct net_device *dev = _data;
#else
        struct rtl8169_private *tp =
                container_of(work, struct rtl8169_private, task.work);
        struct net_device *dev = tp->dev;
#endif
        int ret;

        if (netif_running(dev)) {
                rtl8169_wait_for_quiescence(dev);
                rtl8169_close(dev);
        }

        ret = rtl8169_open(dev);
        if (unlikely(ret < 0)) {
                if (net_ratelimit()) {
                        struct rtl8169_private *tp = netdev_priv(dev);

                        if (netif_msg_drv(tp)) {
                                printk(PFX KERN_ERR
                                       "%s: reinit failure (status = %d)."
                                       " Rescheduling.\n", dev->name, ret);
                        }
                }
                rtl8169_schedule_work(dev, rtl8169_reinit_task);
        }
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void rtl8169_reset_task(void *_data)
{
        struct net_device *dev = _data;
        struct rtl8169_private *tp = netdev_priv(dev);
#else
static void rtl8169_reset_task(struct work_struct *work)
{
        struct rtl8169_private *tp =
                container_of(work, struct rtl8169_private, task.work);
        struct net_device *dev = tp->dev;
#endif
        unsigned long flags;

        if (!netif_running(dev))
                return;

        rtl8169_wait_for_quiescence(dev);

        rtl8169_rx_interrupt(dev, tp, tp->mmio_addr, ~(u32)0);

        spin_lock_irqsave(&tp->lock, flags);

        rtl8169_tx_clear(tp);

        if (tp->dirty_rx == tp->cur_rx) {
                rtl8169_rx_clear(tp);
                rtl8169_init_ring(dev);
                rtl8169_hw_start(dev);
                netif_wake_queue(dev);
                rtl8169_set_speed(dev, tp->autoneg, tp->speed, tp->duplex);
                spin_unlock_irqrestore(&tp->lock, flags);
        } else {
                spin_unlock_irqrestore(&tp->lock, flags);
                if (net_ratelimit()) {
                        struct rtl8169_private *tp = netdev_priv(dev);

                        if (netif_msg_intr(tp)) {
                                printk(PFX KERN_EMERG
                                       "%s: Rx buffers shortage\n", dev->name);
                        }
                }
                rtl8169_schedule_work(dev, rtl8169_reset_task);
        }
}

static void rtl8169_tx_timeout(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        netif_stop_queue(dev);
        rtl8169_hw_reset(dev);
        spin_unlock_irqrestore(&tp->lock, flags);

        /* Let's wait a bit while any (async) irq lands on */
        rtl8169_schedule_work(dev, rtl8169_reset_task);
}

static int rtl8169_xmit_frags(struct rtl8169_private *tp, struct sk_buff *skb,
                              u32 opts1)
{
        struct skb_shared_info *info = skb_shinfo(skb);
        unsigned int cur_frag, entry;
        struct TxDesc *txd = NULL;

        entry = tp->cur_tx;
        for (cur_frag = 0; cur_frag < info->nr_frags; cur_frag++) {
                skb_frag_t *frag = info->frags + cur_frag;
                dma_addr_t mapping;
                u32 status, len;
                void *addr;

                entry = (entry + 1) % NUM_TX_DESC;

                txd = tp->TxDescArray + entry;
                len = frag->size;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
                addr = ((void *) page_address(frag->page)) + frag->page_offset;
#else
                addr = ((void *) page_address(frag->page.p)) + frag->page_offset;
#endif
                mapping = pci_map_single(tp->pci_dev, addr, len, PCI_DMA_TODEVICE);

                /* anti gcc 2.95.3 bugware (sic) */
                status = opts1 | len | (RingEnd * !((entry + 1) % NUM_TX_DESC));

                txd->opts1 = cpu_to_le32(status);
                txd->addr = cpu_to_le64(mapping);

                tp->tx_skb[entry].len = len;
        }

        if (cur_frag) {
                tp->tx_skb[entry].skb = skb;
                txd->opts1 |= cpu_to_le32(LastFrag);
        }

        return cur_frag;
}

static inline u32 rtl8169_tso(struct sk_buff *skb, struct net_device *dev)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        if (dev->features & NETIF_F_TSO) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
                u32 mss = skb_shinfo(skb)->tso_size;
#else
                u32 mss = skb_shinfo(skb)->gso_size;
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)

                if (mss)
                        return LargeSend | ((mss & MSSMask) << MSSShift);
        }
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)

        return 0;
}

static inline u32 rtl8169_tx_csum(struct sk_buff *skb, struct net_device *dev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
        const struct iphdr *ip = skb->nh.iph;
#else
        const struct iphdr *ip = ip_hdr(skb);
#endif

        if (skb->ip_summed == CHECKSUM_PARTIAL) {
                if (ip->protocol == IPPROTO_TCP)
                        return IPCS | TCPCS;
                else if (ip->protocol == IPPROTO_UDP)
                        return IPCS | UDPCS;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                WARN_ON(1);	/* we need a WARN() */
#endif
        }

        return 0;
}

static int rtl8169_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        unsigned int frags, entry;
        struct TxDesc *txd;
        void __iomem *ioaddr = tp->mmio_addr;
        dma_addr_t mapping;
        u32 status, len;
        u32 opts1;
        int ret = NETDEV_TX_OK;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);

        if (unlikely(TX_BUFFS_AVAIL(tp) < skb_shinfo(skb)->nr_frags)) {
                if (netif_msg_drv(tp)) {
                        printk(KERN_ERR
                               "%s: BUG! Tx Ring full when queue awake!\n",
                               dev->name);
                }
                goto err_stop;
        }

        entry = tp->cur_tx % NUM_TX_DESC;
        txd = tp->TxDescArray + entry;

        if (unlikely(le32_to_cpu(txd->opts1) & DescOwn))
                goto err_stop;

        opts1 = DescOwn | rtl8169_tso(skb, dev);
        opts1 |= rtl8169_tx_csum(skb, dev);

        frags = rtl8169_xmit_frags(tp, skb, opts1);
        if (frags) {
                len = skb_headlen(skb);
                opts1 |= FirstFrag;
        } else {
                len = skb->len;

                opts1 |= FirstFrag | LastFrag;
                tp->tx_skb[entry].skb = skb;
        }

        mapping = pci_map_single(tp->pci_dev, skb->data, len, PCI_DMA_TODEVICE);

        tp->tx_skb[entry].len = len;
        txd->addr = cpu_to_le64(mapping);
        txd->opts2 = cpu_to_le32(rtl8169_tx_vlan_tag(tp, skb));

        wmb();

        /* anti gcc 2.95.3 bugware (sic) */
        status = opts1 | len | (RingEnd * !((entry + 1) % NUM_TX_DESC));
        txd->opts1 = cpu_to_le32(status&~DescOwn);
        wmb();
        txd->opts1 = cpu_to_le32(status);

        netif_trans_update(dev);

        tp->cur_tx += frags + 1;

        rtl_wmb();

        RTL_W8(TxPoll, NPQ);	/* set polling bit */

        if (TX_BUFFS_AVAIL(tp) < MAX_SKB_FRAGS) {
                netif_stop_queue(dev);
                rtl_rmb();
                if (TX_BUFFS_AVAIL(tp) >= MAX_SKB_FRAGS)
                        netif_wake_queue(dev);
        }

        spin_unlock_irqrestore(&tp->lock, flags);

out:
        return ret;

err_stop:
        netif_stop_queue(dev);
        ret = NETDEV_TX_BUSY;
        RTLDEV->stats.tx_dropped++;

        spin_unlock_irqrestore(&tp->lock, flags);
        goto out;
}

static void rtl8169_pcierr_interrupt(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;
        void __iomem *ioaddr = tp->mmio_addr;
        u16 pci_status, pci_cmd;

        pci_read_config_word(pdev, PCI_COMMAND, &pci_cmd);
        pci_read_config_word(pdev, PCI_STATUS, &pci_status);

        if (netif_msg_intr(tp)) {
                printk(KERN_ERR
                       "%s: PCI error (cmd = 0x%04x, status = 0x%04x).\n",
                       dev->name, pci_cmd, pci_status);
        }

        /*
         * The recovery sequence below admits a very elaborated explanation:
         * - it seems to work;
         * - I did not see what else could be done;
         * - it makes iop3xx happy.
         *
         * Feel free to adjust to your needs.
         */

        pci_write_config_word(pdev, PCI_COMMAND, pci_cmd | PCI_COMMAND_SERR | PCI_COMMAND_PARITY);

        pci_write_config_word(pdev, PCI_STATUS,
                              pci_status & (PCI_STATUS_DETECTED_PARITY |
                                            PCI_STATUS_SIG_SYSTEM_ERROR | PCI_STATUS_REC_MASTER_ABORT |
                                            PCI_STATUS_REC_TARGET_ABORT | PCI_STATUS_SIG_TARGET_ABORT));

        /* The infamous DAC f*ckup only happens at boot time */
        if ((tp->cp_cmd & PCIDAC) && !tp->dirty_rx && !tp->cur_rx) {
                if (netif_msg_intr(tp))
                        printk(KERN_INFO "%s: disabling PCI DAC.\n", dev->name);
                tp->cp_cmd &= ~PCIDAC;
                RTL_W16(CPlusCmd, tp->cp_cmd);
                dev->features &= ~NETIF_F_HIGHDMA;
        }

        rtl8169_hw_reset(dev);

        rtl8169_schedule_work(dev, rtl8169_reinit_task);
}

static void
rtl8169_tx_interrupt(struct net_device *dev, struct rtl8169_private *tp,
                     void __iomem *ioaddr)
{
        unsigned int dirty_tx, tx_left;

        assert(dev != NULL);
        assert(tp != NULL);
        assert(ioaddr != NULL);

        dirty_tx = tp->dirty_tx;
        rtl_rmb();
        tx_left = tp->cur_tx - dirty_tx;

        while (tx_left > 0) {
                unsigned int entry = dirty_tx % NUM_TX_DESC;
                struct ring_info *tx_skb = tp->tx_skb + entry;
                u32 len = tx_skb->len;
                u32 status;

                rmb();
                status = le32_to_cpu(tp->TxDescArray[entry].opts1);
                if (status & DescOwn)
                        break;

                RTLDEV->stats.tx_bytes += len;
                RTLDEV->stats.tx_packets++;

                rtl8169_unmap_tx_skb(tp->pci_dev, tx_skb, tp->TxDescArray + entry);

                if (status & LastFrag) {
                        dev_kfree_skb_irq(tx_skb->skb);
                        tx_skb->skb = NULL;
                }
                dirty_tx++;
                tx_left--;
        }

        if (tp->dirty_tx != dirty_tx) {
                tp->dirty_tx = dirty_tx;
                rtl_wmb();
                if (netif_queue_stopped(dev) &&
                    (TX_BUFFS_AVAIL(tp) >= MAX_SKB_FRAGS)) {
                        netif_wake_queue(dev);
                }
        }
}

static inline int rtl8169_fragmented_frame(u32 status)
{
        return (status & (FirstFrag | LastFrag)) != (FirstFrag | LastFrag);
}

static inline void rtl8169_rx_csum(struct sk_buff *skb, struct RxDesc *desc)
{
        u32 opts1 = le32_to_cpu(desc->opts1);
        u32 status = opts1 & RxProtoMask;

        if (((status == RxProtoTCP) && !(opts1 & TCPFail)) ||
            ((status == RxProtoUDP) && !(opts1 & UDPFail)) ||
            ((status == RxProtoIP) && !(opts1 & IPFail)))
                skb->ip_summed = CHECKSUM_UNNECESSARY;
        else
                skb->ip_summed = CHECKSUM_NONE;
}

static inline int rtl8169_try_rx_copy(struct sk_buff **sk_buff, int pkt_size,
                                      struct RxDesc *desc, int rx_buf_sz,
                                      unsigned int align)
{
        int ret = -1;

        if (pkt_size < rx_copybreak) {
                struct sk_buff *skb;

                skb = dev_alloc_skb(pkt_size + NET_IP_ALIGN);
                if (skb) {
                        u8 *data;

                        data = sk_buff[0]->data;
                        skb_reserve(skb, NET_IP_ALIGN);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,37)
                        prefetch(data - NET_IP_ALIGN);
#endif
                        eth_copy_and_sum(skb, data, pkt_size, 0);
                        *sk_buff = skb;
                        ret = 0;
                }
        }
        return ret;
}

static int
rtl8169_rx_interrupt(struct net_device *dev,
                     struct rtl8169_private *tp,
                     void __iomem *ioaddr, u32 budget)
{
        unsigned int cur_rx, rx_left;
        unsigned int delta, count = 0;
        unsigned int entry;
        struct RxDesc *desc;
        u32 status;
        u32 rx_quota = RTL_RX_QUOTA(dev, budget);

        assert(dev != NULL);
        assert(tp != NULL);
        assert(ioaddr != NULL);

        if ((tp->RxDescArray == NULL) || (tp->Rx_skbuff == NULL))
                goto rx_out;

        cur_rx = tp->cur_rx;
        entry = cur_rx % NUM_RX_DESC;
        desc = tp->RxDescArray + entry;
        rx_left = NUM_RX_DESC + tp->dirty_rx - cur_rx;
        rx_left = rtl8169_rx_quota(rx_left, (u32) rx_quota);

        for (; rx_left > 0; rx_left--) {
                rmb();
                status = le32_to_cpu(desc->opts1);
                if (status & DescOwn)
                        break;

                if (unlikely(status & RxRES)) {
                        if (netif_msg_rx_err(tp)) {
                                printk(KERN_INFO
                                       "%s: Rx ERROR. status = %08x\n",
                                       dev->name, status);
                        }
                        RTLDEV->stats.rx_errors++;

                        if (status & (RxRWT | RxRUNT))
                                RTLDEV->stats.rx_length_errors++;
                        if (status & RxCRC)
                                RTLDEV->stats.rx_crc_errors++;
                        rtl8169_mark_to_asic(desc, tp->rx_buf_sz);
                } else {
                        struct sk_buff *skb = tp->Rx_skbuff[entry];
                        int pkt_size = (status & 0x00003FFF) - 4;

                        /*
                         * The driver does not support incoming fragmented
                         * frames. They are seen as a symptom of over-mtu
                         * sized frames.
                         */
                        if (unlikely(rtl8169_fragmented_frame(status))) {
                                RTLDEV->stats.rx_dropped++;
                                RTLDEV->stats.rx_length_errors++;
                                rtl8169_mark_to_asic(desc, tp->rx_buf_sz);
                                continue;
                        }

                        rtl8169_rx_csum(skb, desc);

                        pci_unmap_single(tp->pci_dev,
                                         le64_to_cpu(desc->addr), tp->rx_buf_sz,
                                         PCI_DMA_FROMDEVICE);

                        if (rtl8169_try_rx_copy(&skb, pkt_size, desc,
                                                tp->rx_buf_sz, tp->align)) {
                                tp->Rx_skbuff[entry] = NULL;
                        } else {
                                dma_addr_t mapping;
                                mapping = pci_map_single(tp->pci_dev, tp->Rx_skbuff[entry]->data, tp->rx_buf_sz, PCI_DMA_FROMDEVICE);
                                rtl8169_map_to_asic(desc, mapping, tp->rx_buf_sz);
                        }

                        skb->dev = dev;
                        skb_put(skb, pkt_size);
                        skb->protocol = eth_type_trans(skb, dev);

                        if (rtl8169_rx_vlan_skb(tp, desc, skb) < 0)
                                rtl8169_rx_skb(skb);

                        dev->last_rx = jiffies;
                        RTLDEV->stats.rx_bytes += pkt_size;
                        RTLDEV->stats.rx_packets++;
                }

                cur_rx++;
                entry = cur_rx % NUM_RX_DESC;
                desc = tp->RxDescArray + entry;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,37)
                prefetch(desc);
#endif
        }

        count = cur_rx - tp->cur_rx;
        tp->cur_rx = cur_rx;

        delta = rtl8169_rx_fill(tp, dev, tp->dirty_rx, tp->cur_rx);
        if (!delta && count && netif_msg_intr(tp))
                printk(KERN_INFO "%s: no Rx buffer allocated\n", dev->name);
        tp->dirty_rx += delta;

        /*
         * FIXME: until there is periodic timer to try and refill the ring,
         * a temporary shortage may definitely kill the Rx process.
         * - disable the asic to try and avoid an overflow and kick it again
         *   after refill ?
         * - how do others driver handle this condition (Uh oh...).
         */
        if ((tp->dirty_rx + NUM_RX_DESC == tp->cur_rx) && netif_msg_intr(tp))
                printk(KERN_EMERG "%s: Rx buffers exhausted\n", dev->name);

rx_out:
        return count;
}

/* The interrupt handler does all of the Rx thread work and cleans up after the Tx thread. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t rtl8169_interrupt(int irq, void *dev_instance, struct pt_regs *regs)
#else
static irqreturn_t rtl8169_interrupt(int irq, void *dev_instance)
#endif
{
        struct net_device *dev = (struct net_device *) dev_instance;
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int status;
        int handled = IRQ_NONE;

        do {
                status = RTL_R16(IntrStatus);

                /* hotplug/major error/no more work/shared irq */
                if ((status == 0xFFFF) || !status)
                        break;

                if (unlikely(!netif_running(dev))) {
                        break;
                }

                status &= tp->intr_mask;

                if (!(status & rtl8169_intr_mask))
                        break;

                handled = 1;

                RTL_W16(IntrStatus,
                        (status & RxFIFOOver) ? (status | RxOverflow) : status);

                if (unlikely(status & SYSErr)) {
                        rtl8169_pcierr_interrupt(dev);
                        break;
                }

                if (status & LinkChg)
                        rtl8169_check_link_status(dev, tp, ioaddr);

#ifdef CONFIG_R8169_NAPI
                if (status & rtl8169_napi_event) {
                        RTL_W16(IntrMask, rtl8169_intr_mask & ~rtl8169_napi_event);
                        tp->intr_mask = rtl8169_intr_mask & ~rtl8169_napi_event;

                        if (likely(RTL_NETIF_RX_SCHEDULE_PREP(dev, &tp->napi))) {
                                __RTL_NETIF_RX_SCHEDULE(dev, &tp->napi);
                        } else if (netif_msg_intr(tp)) {
                                printk(KERN_INFO "%s: interrupt %04x in poll\n",
                                       dev->name, status);
                        }
                }
                break;
#else
                /* Rx interrupt */
                rtl8169_rx_interrupt(dev, tp, tp->mmio_addr, ~(u32)0);

                /* Tx interrupt */
                rtl8169_tx_interrupt(dev, tp, ioaddr);

                RTL_W16(IntrMask, rtl8169_intr_mask);
#endif
        } while (false);

        return IRQ_RETVAL(handled);
}

#ifdef CONFIG_R8169_NAPI
static int rtl8169_poll(napi_ptr napi, napi_budget budget)
{
        struct rtl8169_private *tp = RTL_GET_PRIV(napi, struct rtl8169_private);
        void __iomem *ioaddr = tp->mmio_addr;
        RTL_GET_NETDEV(tp)
        unsigned int work_to_do = RTL_NAPI_QUOTA(budget, dev);
        unsigned int work_done;
        unsigned long flags;

        work_done = rtl8169_rx_interrupt(dev, tp, ioaddr, (u32) budget);

        spin_lock_irqsave(&tp->lock, flags);
        rtl8169_tx_interrupt(dev, tp, ioaddr);
        spin_unlock_irqrestore(&tp->lock, flags);

        RTL_NAPI_QUOTA_UPDATE(dev, work_done, budget);

        if (work_done < work_to_do) {
                RTL_NETIF_RX_COMPLETE(dev, napi);
                tp->intr_mask = rtl8169_intr_mask;
                /*
                 * 20040426: the barrier is not strictly required but the
                 * behavior of the irq handler could be less predictable
                 * without it. Btw, the lack of flush for the posted pci
                 * write is safe - FR
                 */
                smp_wmb();
                RTL_W16(IntrMask, rtl8169_intr_mask);
        }

        return RTL_NAPI_RETURN_VALUE;
}
#endif//CONFIG_R8169_NAPI

static void rtl8169_down(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,23)) && (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))
        unsigned int poll_locked = 0;
#endif


        rtl8169_delete_timer(dev);
        rtl8169_delete_esd_timer(dev, &tp->esd_timer);

        netif_stop_queue(dev);

#ifdef CONFIG_R8169_NAPI
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23)
        napi_disable(&tp->napi);
#endif
#endif
        spin_lock_irqsave(&tp->lock, flags);

        rtl8169_hw_reset(dev);

        /* Update the error counts. */
        tp->stats.rx_missed_errors += RTL_R32(RxMissed);
        RTL_W32(RxMissed, 0);

        spin_unlock_irqrestore(&tp->lock, flags);

        synchronize_irq(dev->irq);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,23)) && (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))
        if (!poll_locked) {
#ifdef CONFIG_R8169_NAPI
                netif_poll_disable(dev);
#endif
                poll_locked++;
        }
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
        /* Give a racing hard_start_xmit a few cycles to complete. */
        synchronize_sched();  /* FIXME: should this be synchronize_irq()? */
#endif

        /*
         * And now for the 50k$ question: are IRQ disabled or not ?
         *
         * Two paths lead here:
         * 1) dev->close
         *    -> netif_running() is available to sync the current code and the
         *       IRQ handler. See rtl8169_interrupt for details.
         * 2) dev->change_mtu
         *    -> rtl8169_poll can not be issued again and re-enable the
         *       interruptions. Let's simply issue the IRQ down sequence again.
         *
         * No loop if hotpluged or major error (0xffff).
         */

        spin_lock_irqsave(&tp->lock, flags);

        rtl8169_tx_clear(tp);

        rtl8169_rx_clear(tp);

        rtl8169_powerdown_pll(tp);

        spin_unlock_irqrestore(&tp->lock, flags);
}

static int rtl8169_close(struct net_device *dev)
{
        struct rtl8169_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;

        if(tp->TxDescArray!=NULL && tp->RxDescArray!=NULL) {
                rtl8169_cancel_schedule_work(dev);

                rtl8169_down(dev);

                free_irq(dev->irq, dev);

                pci_free_consistent(pdev, R8169_RX_RING_BYTES, tp->RxDescArray,
                                    tp->RxPhyAddr);
                pci_free_consistent(pdev, R8169_TX_RING_BYTES, tp->TxDescArray,
                                    tp->TxPhyAddr);
                tp->TxDescArray = NULL;
                tp->RxDescArray = NULL;
        }

        return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
static void rtl8169_shutdown(struct pci_dev *pdev)
{
        struct net_device *dev = pci_get_drvdata(pdev);
        struct rtl8169_private *tp = netdev_priv(dev);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
        /* restore the original MAC address */
        rtl8169_rar_set(tp, dev->perm_addr);
#else
        rtl8169_rar_set(tp, tp->org_mac_addr);
#endif

        rtl8169_close(dev);
}
#endif

#ifdef CONFIG_PM

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
static int
rtl8169_suspend(struct pci_dev *pdev,
                u32 state)
#else
static int
rtl8169_suspend(struct pci_dev *pdev,
                pm_message_t state)
#endif
{
        struct net_device *dev = pci_get_drvdata(pdev);
        struct rtl8169_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
        u32 pci_pm_state = pci_choose_state(pdev, state);
#endif
        unsigned long flags;

        if (!netif_running(dev))
                goto out;

        rtl8169_cancel_schedule_work(dev);

        netif_device_detach(dev);
        netif_stop_queue(dev);

        rtl8169_delete_timer(dev);
        rtl8169_delete_esd_timer(dev, &tp->esd_timer);

        spin_lock_irqsave(&tp->lock, flags);

        rtl8169_hw_reset(dev);

        tp->stats.rx_missed_errors += RTL_R32(RxMissed);
        RTL_W32(RxMissed, 0);

        rtl8169_powerdown_pll(tp);

        spin_unlock_irqrestore(&tp->lock, flags);

out:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
        pci_save_state(pdev, &pci_pm_state);
#else
        pci_save_state(pdev);
#endif
        pci_enable_wake(pdev, pci_choose_state(pdev, state), tp->wol_enabled);
//	pci_set_power_state(pdev, pci_choose_state(pdev, state));

        return 0;
}

static int rtl8169_resume(struct pci_dev *pdev)
{
        struct net_device *dev = pci_get_drvdata(pdev);
        struct rtl8169_private *tp = netdev_priv(dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
        u32 pci_pm_state = PCI_D0;
#endif

        pci_set_power_state(pdev, PCI_D0);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
        pci_restore_state(pdev, &pci_pm_state);
#else
        pci_restore_state(pdev);
#endif
        pci_enable_wake(pdev, PCI_D0, 0);

        /* restore last modified mac address */
        rtl8169_rar_set(tp, dev->dev_addr);

        if (!netif_running(dev))
                goto out;

        rtl8169_hw_init(dev);

        rtl8169_phy_power_up(dev);

        rtl8169_hw_phy_config(dev);

        rtl8169_schedule_work(dev, rtl8169_reset_task);

        netif_device_attach(dev);

        mod_timer(&tp->link_timer, jiffies + RTL8169_PHY_TIMEOUT);
        mod_timer(&tp->esd_timer, jiffies + RTL8169_ESD_TIMEOUT);
out:
        return 0;
}

#endif /* CONFIG_PM */

static struct pci_driver rtl8169_pci_driver = {
        .name		= MODULENAME,
        .id_table	= rtl8169_pci_tbl,
        .probe		= rtl8169_init_one,
        .remove		= __devexit_p(rtl8169_remove_one),
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
        .shutdown	= rtl8169_shutdown,
#endif
#ifdef CONFIG_PM
        .suspend	= rtl8169_suspend,
        .resume		= rtl8169_resume,
#endif
};

static int __init
rtl8169_init_module(void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        return pci_register_driver(&rtl8169_pci_driver);
#else
        return pci_module_init(&rtl8169_pci_driver);
#endif
}

static void __exit
rtl8169_cleanup_module(void)
{
        pci_unregister_driver(&rtl8169_pci_driver);
}

module_init(rtl8169_init_module);
module_exit(rtl8169_cleanup_module);
