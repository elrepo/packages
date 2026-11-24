/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2021-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */
#include "bnxt_ethtool.c"

#ifndef HAVE_ETHTOOL_SPRINTF
__printf(2, 3) void ethtool_sprintf(u8 **data, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsnprintf(*data, ETH_GSTRING_LEN, fmt, args);
	va_end(args);

	*data += ETH_GSTRING_LEN;
}
#endif /* HAVE_ETHTOOL_SPRINTF */

#ifndef HAVE_ETHTOOL_LINK_KSETTINGS
int bnxt_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct bnxt *bp = netdev_priv(dev);
	struct ethtool_link_ksettings ks;
	int rc;

	memset(&ks, 0, sizeof(ks));
	rc = bnxt_get_link_ksettings(dev, &ks);
	if (rc)
		return rc;

	cmd->supported = ks.link_modes.supported[0];
	cmd->advertising = ks.link_modes.advertising[0];
	cmd->lp_advertising = ks.link_modes.lp_advertising[0];
	ethtool_cmd_speed_set(cmd, ks.base.speed);
	cmd->duplex = ks.base.duplex;
	cmd->autoneg = ks.base.autoneg;
	cmd->port = ks.base.port;
	cmd->phy_address = ks.base.phy_address;
	if (bp->link_info.transceiver ==
	    PORT_PHY_QCFG_RESP_XCVR_PKG_TYPE_XCVR_INTERNAL)
		cmd->transceiver = XCVR_INTERNAL;
	else
		cmd->transceiver = XCVR_EXTERNAL;

	return 0;
}

static void bnxt_fw_to_ethtool_support_spds(struct bnxt_link_info *link_info,
					    struct ethtool_link_ksettings *ks)
{
	u16 fw_speeds = link_info->support_speeds;
	u32 supported;

	supported = _bnxt_fw_to_ethtool_adv_spds(fw_speeds, 0);
	ks->link_modes.supported[0] = supported | SUPPORTED_Pause |
				      SUPPORTED_Asym_Pause;
}

int bnxt_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct bnxt *bp = netdev_priv(dev);
	struct ethtool_link_ksettings ks;

	memset(&ks, 0, sizeof(ks));
	if (cmd->autoneg == AUTONEG_ENABLE) {
		bnxt_fw_to_ethtool_support_spds(&bp->link_info, &ks);

		if (!ks.link_modes.supported) {
			netdev_err(dev, "Autoneg not supported\n");
			return -EINVAL;
		}
		if (cmd->advertising & ~(ks.link_modes.supported[0] |
					 ADVERTISED_Autoneg |
					 ADVERTISED_TP | ADVERTISED_FIBRE)) {
			netdev_err(dev, "Unsupported advertising mask (adv: 0x%x)\n",
				   cmd->advertising);
			return -EINVAL;
		}
	} else {
		/* If received a request for an unknown duplex, assume full*/
		if (cmd->duplex == DUPLEX_UNKNOWN)
			cmd->duplex = DUPLEX_FULL;
	}

	ks.link_modes.advertising[0] = cmd->advertising;
	ks.base.speed = ethtool_cmd_speed(cmd);
	ks.base.duplex = cmd->duplex;
	ks.base.autoneg = cmd->autoneg;
	return bnxt_set_link_ksettings(dev, &ks);
}
#endif

#ifndef HAVE_ETHTOOL_PARAMS_FROM_LINK_MODE
#define ETHTOOL_LINK_MODE(speed, type, duplex)				\
	ETHTOOL_LINK_MODE_ ## speed ## base ## type ## _ ## duplex ## _BIT

#include "bnxt_compat_link_modes.c"

void
ethtool_params_from_link_mode(struct ethtool_link_ksettings *link_ksettings,
			      enum ethtool_link_mode_bit_indices link_mode)
{
	const struct link_mode_info *link_info;

	if (WARN_ON_ONCE(link_mode >= ARRAY_SIZE(link_mode_params)))
		return;

	link_info = &link_mode_params[link_mode];
	link_ksettings->base.speed = link_info->speed;
#ifdef HAVE_ETHTOOL_LANES
	link_ksettings->lanes = link_info->lanes;
#endif
	link_ksettings->base.duplex = link_info->duplex;
#ifdef HAVE_ETHTOOL_LINK_MODE
	link_ksettings->link_mode = link_mode;
#endif
}
#endif

#if !defined(HAVE_ETHTOOL_RXFH_PARAM)
#if defined(HAVE_ETH_RXFH_CONTEXT_ALLOC)
int bnxt_set_rxfh_context(struct net_device *dev, const u32 *indir,
			  const u8 *key, const u8 hfunc, u32 *rss_context,
			  bool delete)
{
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_rss_ctx *rss_ctx;
	struct bnxt_vnic_info *vnic;
	bool modify = false;
	int bit_id;
	int rc;

	if (!BNXT_SUPPORTS_MULTI_RSS_CTX(bp))
		return -EOPNOTSUPP;

	if (!netif_running(dev))
		return -EAGAIN;

	if (*rss_context != ETH_RXFH_CONTEXT_ALLOC) {
		rss_ctx = bnxt_get_rss_ctx_from_index(bp, *rss_context);
		if (!rss_ctx)
			return -EINVAL;
		if (delete) {
			bnxt_del_one_rss_ctx(bp, rss_ctx, true, false);
			return 0;
		}
		modify = true;
		vnic = &rss_ctx->vnic;
		goto modify_context;
	}

	if (hfunc && hfunc != ETH_RSS_HASH_TOP)
		return -EOPNOTSUPP;

	if (bp->num_rss_ctx >= BNXT_MAX_ETH_RSS_CTX)
		return -EINVAL;

	if (!__bnxt_rfs_capable(bp, true))
		return -ENOMEM;

	rss_ctx = bnxt_alloc_rss_ctx(bp);
	if (!rss_ctx)
		return -ENOMEM;

	vnic = &rss_ctx->vnic;
	vnic->flags |= BNXT_VNIC_RSSCTX_FLAG;
	vnic->vnic_id = BNXT_VNIC_ID_INVALID;
	rc = bnxt_alloc_vnic_rss_table(bp, vnic);
	if (rc)
		goto out;

	rc = bnxt_alloc_rss_indir_tbl_compat(bp, rss_ctx);
	if (rc)
		goto out;

	bnxt_set_dflt_rss_indir_tbl(bp, rss_ctx);
	memcpy(vnic->rss_hash_key, bp->rss_hash_key, HW_HASH_KEY_SIZE);

	rc = bnxt_hwrm_vnic_alloc(bp, vnic, 0, bp->rx_nr_rings);
	if (rc)
		goto out;

	rc = bnxt_hwrm_vnic_set_tpa(bp, vnic, bp->flags & BNXT_FLAG_TPA);
	if (rc)
		goto out;
modify_context:
	if (indir) {
		u32 i, pad, tbl_size = bnxt_get_rxfh_indir_size(dev);

		for (i = 0; i < tbl_size; i++)
			rss_ctx->rss_indir_tbl[i] = indir[i];
		pad = bp->rss_indir_tbl_entries - tbl_size;
		if (pad)
			memset(&rss_ctx->rss_indir_tbl[i], 0, pad * sizeof(u16));
	}

	if (key)
		memcpy(vnic->rss_hash_key, key, HW_HASH_KEY_SIZE);

	if (modify)
		return bnxt_hwrm_vnic_rss_cfg_p5(bp, vnic);

	rc = __bnxt_setup_vnic_p5(bp, vnic);
	if (rc)
		goto out;

	bit_id = bitmap_find_free_region(bp->rss_ctx_bmap,
					 BNXT_RSS_CTX_BMAP_LEN, 0);
	if (bit_id < 0) {
		rc = -ENOMEM;
		goto out;
	}
	rss_ctx->index = (u16)bit_id;
	*rss_context = rss_ctx->index;

	return 0;
out:
	bnxt_del_one_rss_ctx(bp, rss_ctx, true, false);
	return rc;
}

int bnxt_get_rxfh_context(struct net_device *dev, u32 *indir, u8 *key,
			  u8 *hfunc, u32 rss_context)
{
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_rss_ctx *rss_ctx;
	struct bnxt_vnic_info *vnic;
	int i;

	rss_ctx = bnxt_get_rss_ctx_from_index(bp, rss_context);
	if (!rss_ctx)
		return -EINVAL;

	vnic = &rss_ctx->vnic;
	if (hfunc)
		*hfunc = ETH_RSS_HASH_TOP;
	if (indir)
		for (i = 0; i < bnxt_get_rxfh_indir_size(bp->dev); i++)
			indir[i] = rss_ctx->rss_indir_tbl[i];
	if (key)
		memcpy(key, vnic->rss_hash_key, HW_HASH_KEY_SIZE);
	return 0;
}
#endif /* HAVE_ETH_RXFH_CONTEXT_ALLOC */

int bnxt_get_rxfh(struct net_device *dev, u32 *indir, u8 *key, u8 *hfunc)
{
	struct bnxt *bp = netdev_priv(dev);
	struct bnxt_vnic_info *vnic;
	u32 i, tbl_size;

	/* WIP: Return HWRM_VNIC_RSS_QCFG response, instead of driver cache */
	if (hfunc)
		*hfunc = bp->rss_hfunc;

	if (!bp->vnic_info)
		return 0;

	vnic = &bp->vnic_info[BNXT_VNIC_DEFAULT];
	if (indir && bp->rss_indir_tbl) {
		tbl_size = bnxt_get_rxfh_indir_size(dev);
		for (i = 0; i < tbl_size; i++)
			indir[i] = bp->rss_indir_tbl[i];
	}

	if (key && vnic->rss_hash_key)
		memcpy(key, vnic->rss_hash_key, HW_HASH_KEY_SIZE);

	return 0;
}

int bnxt_set_rxfh(struct net_device *dev, const u32 *indir, const u8 *key,
		  const u8 hfunc)
{
	struct bnxt *bp = netdev_priv(dev);
	bool skip_key = false;
	int rc = 0;

	/* Check HW cap and cache hash func details */
	switch (hfunc) {
	case ETH_RSS_HASH_XOR:
		if (!(bp->rss_cap & BNXT_RSS_CAP_XOR_CAP))
			return -EOPNOTSUPP;
		/* hkey not needed in XOR mode */
		skip_key = true;
		break;
	case ETH_RSS_HASH_TOP:
		if (!(bp->rss_cap & BNXT_RSS_CAP_TOEPLITZ_CAP))
			return -EOPNOTSUPP;
		break;
	case ETH_RSS_HASH_CRC32:
		/* default keys/indir */
		if (!(bp->rss_cap & BNXT_RSS_CAP_TOEPLITZ_CHKSM_CAP))
			return -EOPNOTSUPP;
		skip_key = true;
		break;
	case ETH_RSS_HASH_NO_CHANGE:
		break;
	default:
		return -EOPNOTSUPP;
	}

	/* Repeat of same hfunc with no key or weight */
	if (bp->rss_hfunc == hfunc && !key && !indir)
		return -EINVAL;

	/* for xor and crc32 block hkey config */
	if (key && skip_key)
		return -EINVAL;

	if (key) {
		memcpy(bp->rss_hash_key, key, HW_HASH_KEY_SIZE);
		bp->rss_hash_key_updated = true;
	}

	bp->rss_hfunc = hfunc;
	if (indir) {
		u32 i, pad, tbl_size = bnxt_get_rxfh_indir_size(dev);

		for (i = 0; i < tbl_size; i++)
			bp->rss_indir_tbl[i] = indir[i];
		pad = bp->rss_indir_tbl_entries - tbl_size;
		if (pad)
			memset(&bp->rss_indir_tbl[i], 0, pad * sizeof(u16));
	}
	bnxt_clear_usr_fltrs(bp, false);
	if (netif_running(bp->dev)) {
		bnxt_close_nic(bp, false, false);
		rc = bnxt_open_nic(bp, false, false);
	}
	return rc;
}
#endif /* !HAVE_ETHTOOL_RXFH_PARAM */

#if !defined(HAVE_ETHTOOL_KEEE) || !defined(HAVE_ETHTOOL_LINK_KSETTINGS)
u32 _bnxt_fw_to_ethtool_adv_spds(u16 fw_speeds, u8 fw_pause)
{
	u32 speed_mask = 0;

	/* TODO: support 25GB, 40GB, 50GB with different cable type */
	/* set the advertised speeds */
	if (fw_speeds & BNXT_LINK_SPEED_MSK_100MB)
		speed_mask |= ADVERTISED_100baseT_Full;
	if (fw_speeds & BNXT_LINK_SPEED_MSK_1GB)
		speed_mask |= ADVERTISED_1000baseT_Full;
	if (fw_speeds & BNXT_LINK_SPEED_MSK_2_5GB)
		speed_mask |= ADVERTISED_2500baseX_Full;
	if (fw_speeds & BNXT_LINK_SPEED_MSK_10GB)
		speed_mask |= ADVERTISED_10000baseT_Full;
	if (fw_speeds & BNXT_LINK_SPEED_MSK_40GB)
		speed_mask |= ADVERTISED_40000baseCR4_Full;

	if ((fw_pause & BNXT_LINK_PAUSE_BOTH) == BNXT_LINK_PAUSE_BOTH)
		speed_mask |= ADVERTISED_Pause;
	else if (fw_pause & BNXT_LINK_PAUSE_TX)
		speed_mask |= ADVERTISED_Asym_Pause;
	else if (fw_pause & BNXT_LINK_PAUSE_RX)
		speed_mask |= ADVERTISED_Pause | ADVERTISED_Asym_Pause;

	return speed_mask;
}

u16 bnxt_get_fw_auto_link_speeds(u32 advertising)
{
	u16 fw_speed_mask = 0;

	/* only support autoneg at speed 100, 1000, and 10000 */
	if (advertising & (ADVERTISED_100baseT_Full |
			   ADVERTISED_100baseT_Half)) {
		fw_speed_mask |= BNXT_LINK_SPEED_MSK_100MB;
	}
	if (advertising & (ADVERTISED_1000baseT_Full |
			   ADVERTISED_1000baseT_Half)) {
		fw_speed_mask |= BNXT_LINK_SPEED_MSK_1GB;
	}
	if (advertising & ADVERTISED_10000baseT_Full)
		fw_speed_mask |= BNXT_LINK_SPEED_MSK_10GB;

	if (advertising & ADVERTISED_40000baseCR4_Full)
		fw_speed_mask |= BNXT_LINK_SPEED_MSK_40GB;

	return fw_speed_mask;
}
#endif /* !HAVE_ETHTOOL_KEEE || !HAVE_ETHTOOL_LINK_KSETTINGS */

#if defined(ETHTOOL_GEEE) && !defined(GET_ETHTOOL_OP_EXT) && !defined(HAVE_ETHTOOL_KEEE)
int bnxt_set_eee(struct net_device *dev, struct ethtool_eee *edata)
{
	struct bnxt *bp = netdev_priv(dev);
	struct ethtool_eee *eee = (struct ethtool_eee *)&bp->eee;
	struct bnxt_link_info *link_info = &bp->link_info;
	u32 advertising;
	int rc = 0;

	if (!BNXT_PHY_CFG_ABLE(bp))
		return -EOPNOTSUPP;

	if (!(bp->phy_flags & BNXT_PHY_FL_EEE_CAP))
		return -EOPNOTSUPP;

	mutex_lock(&bp->link_lock);
	advertising = _bnxt_fw_to_ethtool_adv_spds(link_info->advertising, 0);
	if (!edata->eee_enabled)
		goto eee_ok;

	if (!(link_info->autoneg & BNXT_AUTONEG_SPEED)) {
		netdev_warn(dev, "EEE requires autoneg\n");
		rc = -EINVAL;
		goto eee_exit;
	}
	if (edata->tx_lpi_enabled) {
		if (bp->lpi_tmr_hi && (edata->tx_lpi_timer > bp->lpi_tmr_hi ||
				       edata->tx_lpi_timer < bp->lpi_tmr_lo)) {
			netdev_warn(dev, "Valid LPI timer range is %d and %d microsecs\n",
				    bp->lpi_tmr_lo, bp->lpi_tmr_hi);
			rc = -EINVAL;
			goto eee_exit;
		} else if (!bp->lpi_tmr_hi) {
			edata->tx_lpi_timer = eee->tx_lpi_timer;
		}
	}
	if (!edata->advertised) {
		edata->advertised = advertising & eee->supported;
	} else if (edata->advertised & ~advertising) {
		netdev_warn(dev, "EEE advertised %x must be a subset of autoneg advertised speeds %x\n",
			    edata->advertised, advertising);
		rc = -EINVAL;
		goto eee_exit;
	}

	eee->advertised = edata->advertised;
	eee->tx_lpi_enabled = edata->tx_lpi_enabled;
	eee->tx_lpi_timer = edata->tx_lpi_timer;
eee_ok:
	eee->eee_enabled = edata->eee_enabled;

	if (netif_running(dev))
		rc = bnxt_hwrm_set_link_setting(bp, false, true);

eee_exit:
	mutex_unlock(&bp->link_lock);
	return rc;
}

int bnxt_get_eee(struct net_device *dev, struct ethtool_eee *edata)
{
	struct bnxt *bp = netdev_priv(dev);

	if (!(bp->phy_flags & BNXT_PHY_FL_EEE_CAP))
		return -EOPNOTSUPP;

	memcpy(edata, &bp->eee, sizeof(*edata));
	if (!bp->eee.eee_enabled) {
		/* Preserve tx_lpi_timer so that the last value will be used
		 * by default when it is re-enabled.
		 */
		edata->advertised = 0;
		edata->tx_lpi_enabled = 0;
	}

	if (!bp->eee.eee_active)
		edata->lp_advertised = 0;

	return 0;
}
#endif

#ifndef HAVE_SET_RXFH_FIELDS
int bnxt_grxfh(struct bnxt *bp, struct ethtool_rxnfc *cmd)
{
	cmd->data = 0;
	switch (cmd->flow_type) {
	case TCP_V4_FLOW:
		if (bp->rss_hash_cfg & VNIC_RSS_CFG_REQ_HASH_TYPE_TCP_IPV4)
			cmd->data |= RXH_IP_SRC | RXH_IP_DST |
				     RXH_L4_B_0_1 | RXH_L4_B_2_3;
		cmd->data |= get_ethtool_ipv4_rss(bp);
		break;
	case UDP_V4_FLOW:
		if (bp->rss_hash_cfg & VNIC_RSS_CFG_REQ_HASH_TYPE_UDP_IPV4)
			cmd->data |= RXH_IP_SRC | RXH_IP_DST |
				     RXH_L4_B_0_1 | RXH_L4_B_2_3;
		fallthrough;
	case AH_ESP_V4_FLOW:
		if (bp->rss_hash_cfg &
		    (VNIC_RSS_CFG_REQ_HASH_TYPE_AH_SPI_IPV4 |
		     VNIC_RSS_CFG_REQ_HASH_TYPE_ESP_SPI_IPV4))
			cmd->data |= RXH_IP_SRC | RXH_IP_DST |
				     RXH_L4_B_0_1 | RXH_L4_B_2_3;
		fallthrough;
	case SCTP_V4_FLOW:
	case AH_V4_FLOW:
	case ESP_V4_FLOW:
	case IPV4_FLOW:
		cmd->data |= get_ethtool_ipv4_rss(bp);
		break;

	case TCP_V6_FLOW:
		if (bp->rss_hash_cfg & VNIC_RSS_CFG_REQ_HASH_TYPE_TCP_IPV6)
			cmd->data |= RXH_IP_SRC | RXH_IP_DST |
				     RXH_L4_B_0_1 | RXH_L4_B_2_3;
		cmd->data |= get_ethtool_ipv6_rss(bp);
		break;
	case UDP_V6_FLOW:
		if (bp->rss_hash_cfg & VNIC_RSS_CFG_REQ_HASH_TYPE_UDP_IPV6)
			cmd->data |= RXH_IP_SRC | RXH_IP_DST |
				     RXH_L4_B_0_1 | RXH_L4_B_2_3;
		fallthrough;
	case AH_ESP_V6_FLOW:
		if (bp->rss_hash_cfg &
		    (VNIC_RSS_CFG_REQ_HASH_TYPE_AH_SPI_IPV6 |
		     VNIC_RSS_CFG_REQ_HASH_TYPE_ESP_SPI_IPV6))
			cmd->data |= RXH_IP_SRC | RXH_IP_DST |
				     RXH_L4_B_0_1 | RXH_L4_B_2_3;
		fallthrough;
	case SCTP_V6_FLOW:
	case AH_V6_FLOW:
	case ESP_V6_FLOW:
	case IPV6_FLOW:
		cmd->data |= get_ethtool_ipv6_rss(bp);
		break;
	}
	return 0;
}

#define RXH_4TUPLE (RXH_IP_SRC | RXH_IP_DST | RXH_L4_B_0_1 | RXH_L4_B_2_3)
#define RXH_2TUPLE (RXH_IP_SRC | RXH_IP_DST)

int bnxt_srxfh(struct bnxt *bp, struct ethtool_rxnfc *cmd)
{
	u32 rss_hash_cfg = bp->rss_hash_cfg;
	int tuple, rc = 0;

	if (cmd->data == RXH_4TUPLE)
		tuple = 4;
	else if (cmd->data == RXH_2TUPLE)
		tuple = 2;
	else if (!cmd->data)
		tuple = 0;
	else
		return -EINVAL;

	if (cmd->flow_type == TCP_V4_FLOW) {
		rss_hash_cfg &= ~VNIC_RSS_CFG_REQ_HASH_TYPE_TCP_IPV4;
		if (tuple == 4)
			rss_hash_cfg |= VNIC_RSS_CFG_REQ_HASH_TYPE_TCP_IPV4;
	} else if (cmd->flow_type == UDP_V4_FLOW) {
		if (tuple == 4 && !(bp->rss_cap & BNXT_RSS_CAP_UDP_RSS_CAP))
			return -EINVAL;
		rss_hash_cfg &= ~VNIC_RSS_CFG_REQ_HASH_TYPE_UDP_IPV4;
		if (tuple == 4)
			rss_hash_cfg |= VNIC_RSS_CFG_REQ_HASH_TYPE_UDP_IPV4;
	} else if (cmd->flow_type == TCP_V6_FLOW) {
		rss_hash_cfg &= ~VNIC_RSS_CFG_REQ_HASH_TYPE_TCP_IPV6;
		if (tuple == 4)
			rss_hash_cfg |= VNIC_RSS_CFG_REQ_HASH_TYPE_TCP_IPV6;
	} else if (cmd->flow_type == UDP_V6_FLOW) {
		if (tuple == 4 && !(bp->rss_cap & BNXT_RSS_CAP_UDP_RSS_CAP))
			return -EINVAL;
		rss_hash_cfg &= ~VNIC_RSS_CFG_REQ_HASH_TYPE_UDP_IPV6;
		if (tuple == 4)
			rss_hash_cfg |= VNIC_RSS_CFG_REQ_HASH_TYPE_UDP_IPV6;
	} else if (cmd->flow_type == AH_ESP_V4_FLOW) {
		if (tuple == 4 && (!(bp->rss_cap & BNXT_RSS_CAP_AH_V4_RSS_CAP) ||
				   !(bp->rss_cap & BNXT_RSS_CAP_ESP_V4_RSS_CAP)))
			return -EINVAL;
		rss_hash_cfg &= ~(VNIC_RSS_CFG_REQ_HASH_TYPE_AH_SPI_IPV4 |
				  VNIC_RSS_CFG_REQ_HASH_TYPE_ESP_SPI_IPV4);
		if (tuple == 4)
			rss_hash_cfg |= VNIC_RSS_CFG_REQ_HASH_TYPE_AH_SPI_IPV4 |
					VNIC_RSS_CFG_REQ_HASH_TYPE_ESP_SPI_IPV4;
	} else if (cmd->flow_type == AH_ESP_V6_FLOW) {
		if (tuple == 4 && (!(bp->rss_cap & BNXT_RSS_CAP_AH_V6_RSS_CAP) ||
				   !(bp->rss_cap & BNXT_RSS_CAP_ESP_V6_RSS_CAP)))
			return -EINVAL;
		rss_hash_cfg &= ~(VNIC_RSS_CFG_REQ_HASH_TYPE_AH_SPI_IPV6 |
				  VNIC_RSS_CFG_REQ_HASH_TYPE_ESP_SPI_IPV6);
		if (tuple == 4)
			rss_hash_cfg |= VNIC_RSS_CFG_REQ_HASH_TYPE_AH_SPI_IPV6 |
					VNIC_RSS_CFG_REQ_HASH_TYPE_ESP_SPI_IPV6;
	} else if (tuple == 4) {
		return -EINVAL;
	}

	switch (cmd->flow_type) {
	case TCP_V4_FLOW:
	case UDP_V4_FLOW:
	case SCTP_V4_FLOW:
	case AH_ESP_V4_FLOW:
	case AH_V4_FLOW:
	case ESP_V4_FLOW:
	case IPV4_FLOW:
		if (tuple == 2)
			rss_hash_cfg |= VNIC_RSS_CFG_REQ_HASH_TYPE_IPV4;
		else if (!tuple)
			rss_hash_cfg &= ~VNIC_RSS_CFG_REQ_HASH_TYPE_IPV4;
		break;

	case TCP_V6_FLOW:
	case UDP_V6_FLOW:
	case SCTP_V6_FLOW:
	case AH_ESP_V6_FLOW:
	case AH_V6_FLOW:
	case ESP_V6_FLOW:
	case IPV6_FLOW:
		if (tuple == 2) {
			if (bp->ipv6_flow_lbl_rss_en) {
				/* Hash type ipv6 and ipv6_flow_label are mutually
				 * exclusive. HW does not include the flow_label
				 * in hash calculation for the packets that are
				 * matching tcp_ipv6 and udp_ipv6 hash types
				 */
				rss_hash_cfg |= VNIC_RSS_CFG_REQ_HASH_TYPE_IPV6_FLOW_LABEL;
				rss_hash_cfg &= ~VNIC_RSS_CFG_REQ_HASH_TYPE_IPV6;
			} else {
				/* Negate flow label if priv flag not set */
				rss_hash_cfg |= VNIC_RSS_CFG_REQ_HASH_TYPE_IPV6;
				rss_hash_cfg &= ~VNIC_RSS_CFG_REQ_HASH_TYPE_IPV6_FLOW_LABEL;
			}
		} else if (!tuple) {
			rss_hash_cfg &= ~VNIC_RSS_CFG_REQ_HASH_TYPE_IPV6;
		}
		break;
	}

	if (bp->rss_hash_cfg == rss_hash_cfg)
		return 0;

	if (bp->rss_cap & BNXT_RSS_CAP_RSS_HASH_TYPE_DELTA)
		bp->rss_hash_delta = bp->rss_hash_cfg ^ rss_hash_cfg;
	bp->rss_hash_cfg = rss_hash_cfg;
	if (netif_running(bp->dev)) {
		bnxt_close_nic(bp, false, false);
		rc = bnxt_open_nic(bp, false, false);
	}
	return rc;
}
#endif	/* HAVE_SET_RXFH_FIELDS */
