/*
 * Copyright (C) 2015 Microchip Technology
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include "microchipphy.h"

#define DRIVER_AUTHOR	"WOOJUNG HUH <woojung.huh@microchip.com>"
#define DRIVER_DESC	"Microchip LAN88XX PHY driver"

/* elrepo backport */
static int phy_read_mmd_indirect(struct mii_bus *bus, int prtad, int devad,
				int addr);

static int genphy_config_init(struct phy_device *phydev);

static inline void mmd_phy_indirect(struct mii_bus *bus, int prtad, int devad,
				    int addr)
{
	/* Write the desired MMD Devad */
	bus->write(bus, addr, MII_MMD_CTRL, devad);

	/* Write the desired MMD register address */
	bus->write(bus, addr, MII_MMD_DATA, prtad);

	/* Select the Function : DATA with no post increment */
	bus->write(bus, addr, MII_MMD_CTRL, (devad | MII_MMD_CTRL_NOINCR));
}

/**
 * phy_read_mmd_indirect - reads data from the MMD registers
 * @bus: the target MII bus
 * @prtad: MMD Address
 * @devad: MMD DEVAD
 * @addr: PHY address on the MII bus
 *
 * Description: it reads data from the MMD registers (clause 22 to access to
 * clause 45) of the specified phy address.
 * To read these register we have:
 * 1) Write reg 13 // DEVAD
 * 2) Write reg 14 // MMD Address
 * 3) Write reg 13 // MMD Data Command for MMD DEVAD
 * 3) Read  reg 14 // Read MMD data
 */
static int phy_read_mmd_indirect(struct mii_bus *bus, int prtad, int devad,
				 int addr)
{
	u32 ret;

	mmd_phy_indirect(bus, prtad, devad, addr);

	/* Read the content of the MMD's selected register */
	ret = bus->read(bus, addr, MII_MMD_DATA);

	return ret;
}

static int genphy_config_init(struct phy_device *phydev)
{
	int val;
	u32 features;

	/* For now, I'll claim that the generic driver supports
	 * all possible port types */
	features = (SUPPORTED_TP | SUPPORTED_MII
			| SUPPORTED_AUI | SUPPORTED_FIBRE |
			SUPPORTED_BNC);

	/* Do we support autonegotiation? */
	val = phy_read(phydev, MII_BMSR);

	if (val < 0)
		return val;

	if (val & BMSR_ANEGCAPABLE)
		features |= SUPPORTED_Autoneg;

	if (val & BMSR_100FULL)
		features |= SUPPORTED_100baseT_Full;
	if (val & BMSR_100HALF)
		features |= SUPPORTED_100baseT_Half;
	if (val & BMSR_10FULL)
		features |= SUPPORTED_10baseT_Full;
	if (val & BMSR_10HALF)
		features |= SUPPORTED_10baseT_Half;

	if (val & BMSR_ESTATEN) {
		val = phy_read(phydev, MII_ESTATUS);

		if (val < 0)
			return val;

		if (val & ESTATUS_1000_TFULL)
			features |= SUPPORTED_1000baseT_Full;
		if (val & ESTATUS_1000_THALF)
			features |= SUPPORTED_1000baseT_Half;
	}

	phydev->supported = features;
	phydev->advertising = features;

	return 0;
}
/* end elrepo backport */

struct lan88xx_priv {
	int	chip_id;
	int	chip_rev;
	__u32	wolopts;
};

static int lan88xx_phy_config_intr(struct phy_device *phydev)
{
	int rc;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED) {
		/* unmask all source and clear them before enable */
		rc = phy_write(phydev, LAN88XX_INT_MASK, 0x7FFF);
		rc = phy_read(phydev, LAN88XX_INT_STS);
		rc = phy_write(phydev, LAN88XX_INT_MASK,
			       LAN88XX_INT_MASK_MDINTPIN_EN_ |
			       LAN88XX_INT_MASK_LINK_CHANGE_);
	} else {
		rc = phy_write(phydev, LAN88XX_INT_MASK, 0);
	}

	return rc < 0 ? rc : 0;
}

static int lan88xx_phy_ack_interrupt(struct phy_device *phydev)
{
	int rc = phy_read(phydev, LAN88XX_INT_STS);

	return rc < 0 ? rc : 0;
}

int lan88xx_suspend(struct phy_device *phydev)
{
	struct lan88xx_priv *priv = phydev->priv;

	/* do not power down PHY when WOL is enabled */
	if (!priv->wolopts)
		genphy_suspend(phydev);

	return 0;
}

static int lan88xx_probe(struct phy_device *phydev)
{

	struct device *dev = &phydev->dev;
	struct lan88xx_priv *priv;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->wolopts = 0;

	/* these values can be used to identify internal PHY */
	priv->chip_id = phy_read_mmd_indirect(phydev->bus,
				LAN88XX_MMD3_CHIP_ID, 3, phydev->addr);
	priv->chip_rev = phy_read_mmd_indirect(phydev->bus,
				LAN88XX_MMD3_CHIP_REV, 3, phydev->addr);

	phydev->priv = priv;

	return 0;
}

static void lan88xx_remove(struct phy_device *phydev)
{

	struct device *dev = &phydev->dev;
	struct lan88xx_priv *priv = phydev->priv;

	if (priv)
		devm_kfree(dev, priv);
}

static int lan88xx_set_wol(struct phy_device *phydev,
			   struct ethtool_wolinfo *wol)
{
	struct lan88xx_priv *priv = phydev->priv;

	priv->wolopts = wol->wolopts;

	return 0;
}

static struct phy_driver microchip_phy_driver[] = {
{
	.phy_id		= 0x0007c130,
	.phy_id_mask	= 0xfffffff0,
	.name		= "Microchip LAN88xx",

	.features	= (PHY_GBIT_FEATURES |
			   SUPPORTED_Pause | SUPPORTED_Asym_Pause),
	.flags		= PHY_HAS_INTERRUPT | PHY_HAS_MAGICANEG,

	.probe		= lan88xx_probe,
	.remove		= lan88xx_remove,

	.config_init	= genphy_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,

	.ack_interrupt	= lan88xx_phy_ack_interrupt,
	.config_intr	= lan88xx_phy_config_intr,

	.suspend	= lan88xx_suspend,
	.resume		= genphy_resume,
	.set_wol	= lan88xx_set_wol,
} };

static int __init microchip_phy_init(void)
{
	return phy_drivers_register(microchip_phy_driver,
					ARRAY_SIZE(microchip_phy_driver));
}

static void __exit microchip_phy_exit(void)
{
	return phy_drivers_unregister(microchip_phy_driver,
					ARRAY_SIZE(microchip_phy_driver));
}

module_init(microchip_phy_init);
module_exit(microchip_phy_exit);

static struct mdio_device_id __maybe_unused microchip_tbl[] = {
	{ 0x0007c130, 0xfffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, microchip_tbl);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
