/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2014-2016 Broadcom Corporation
 * Copyright (c) 2016-2018 Broadcom Limited
 * Copyright (c) 2018-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef BNXT_ETHTOOL_H
#define BNXT_ETHTOOL_H

#include <linux/firmware.h>

struct bnxt_led_cfg {
	u8 led_id;
	u8 led_state;
	u8 led_color;
	u8 unused;
	__le16 led_blink_on;
	__le16 led_blink_off;
	u8 led_group_id;
	u8 rsvd;
};

#define BNXT_LED_DFLT_ENA				\
	(PORT_LED_CFG_REQ_ENABLES_LED0_ID |		\
	 PORT_LED_CFG_REQ_ENABLES_LED0_STATE |		\
	 PORT_LED_CFG_REQ_ENABLES_LED0_BLINK_ON |	\
	 PORT_LED_CFG_REQ_ENABLES_LED0_BLINK_OFF |	\
	 PORT_LED_CFG_REQ_ENABLES_LED0_GROUP_ID)

#define BNXT_LED_DFLT_ENA_SHIFT	6

#define BNXT_LED_DFLT_ENABLES(x)			\
	cpu_to_le32(BNXT_LED_DFLT_ENA << (BNXT_LED_DFLT_ENA_SHIFT * (x)))

#define BNXT_FW_RESET_CRASHDUMP (ETH_RESET_CRASHDUMP << ETH_RESET_SHARED_SHIFT)
#define BNXT_FW_RESET_AP	(ETH_RESET_AP << ETH_RESET_SHARED_SHIFT)
#define BNXT_FW_RESET_CHIP	((ETH_RESET_MGMT | ETH_RESET_IRQ |	\
				  ETH_RESET_DMA | ETH_RESET_FILTER |	\
				  ETH_RESET_OFFLOAD | ETH_RESET_MAC |	\
				  ETH_RESET_PHY | ETH_RESET_RAM)	\
				 << ETH_RESET_SHARED_SHIFT)

#define BNXT_PXP_REG_LEN	0x3110
#define BNXT_MBUF_REG_START	0x3800
#define BNXT_MBUF_REG_LEN	0x12c

#define BNXT_RX_MBUF_REG_OFFSET	BNXT_MBUF_REG_START
#define BNXT_TX_MBUF_REG_OFFSET	(BNXT_RX_MBUF_REG_OFFSET + 0x64 +	\
				 sizeof(struct queue_adptv_qos_rx_info))

#define BNXT_IP_PROTO_FULL_MASK	0xFF
#define BNXT_IP_PROTO_WILDCARD 0x0

extern const struct ethtool_ops bnxt_ethtool_ops;

u32 bnxt_get_rxfh_indir_size(struct net_device *dev);
#ifdef HAVE_ETHTOOL_KEEE
void _bnxt_fw_to_linkmode(unsigned long *mode, u16 fw_speeds);
#else
u32 _bnxt_fw_to_ethtool_adv_spds(u16, u8);
#endif
u32 bnxt_fw_to_ethtool_speed(u16);
#ifdef HAVE_ETHTOOL_KEEE
u16 bnxt_get_fw_auto_link_speeds(const unsigned long *mode);
#else
u16 bnxt_get_fw_auto_link_speeds(u32);
#endif
int bnxt_hwrm_nvm_get_dev_info(struct bnxt *bp,
			       struct hwrm_nvm_get_dev_info_output *nvm_dev_info);
int bnxt_flash_package_from_fw_obj(struct net_device *dev, const struct firmware *fw,
				   u32 install_type, struct netlink_ext_ack *extack);
int bnxt_flash_package_from_file(struct net_device *dev, const char *filename,
				 u32 install_type, struct netlink_ext_ack *extack);
int bnxt_hwrm_firmware_reset(struct net_device *dev, u8 proc_type,
			     u8 self_reset, u8 flags);
int bnxt_get_pkginfo(struct net_device *dev, char *ver, int size);
int bnxt_sync_firmware(struct bnxt *bp);
int bnxt_hwrm_get_fw_sync_status(struct bnxt *bp, u16 *fw_status);
int bnxt_hwrm_fw_sync(struct bnxt *bp, u16 fw_status);
void bnxt_ethtool_init(struct bnxt *bp);
void bnxt_ethtool_free(struct bnxt *bp);
int bnxt_find_nvram_item(struct net_device *dev, u16 type, u16 ordinal,
			 u16 ext, u16 *index, u32 *item_length,
			 u32 *data_length);
int bnxt_find_nvram_item(struct net_device *dev, u16 type, u16 ordinal,
			 u16 ext, u16 *index, u32 *item_length,
			 u32 *data_length);
int bnxt_flash_nvram(struct net_device *dev, u16 dir_type,
		     u16 dir_ordinal, u16 dir_ext, u16 dir_attr,
		     u32 dir_item_len, const u8 *data,
		     size_t data_len);
int bnxt_get_nvram_item(struct net_device *dev, u32 index, u32 offset,
			u32 length, u8 *data);
int bnxt_firmware_reset_chip(struct net_device *dev);
int bnxt_firmware_reset_ap(struct net_device *dev);

/* CMIS (page 0) offsets */
#define CMIS_PG0_IDENTIFIER			0x00
#define CMIS_PG0_MODULE_STATE		0x03
#define CMIS_PG0_MODULE_STATE_READY	(0x03u << 1)
#define CMIS_PG0_MODULE_STATE_FAULT	(0x05u << 1)
#define CMIS_PG0_FAULT_CAUSE			0x29
#define CMIS_PG0_MODULE_FLAGS			0x09
#define CMIS_PG0_MODULE_FLAGS_TEMP_HI_ALARM	BIT(0)
#define CMIS_PG0_MODULE_FLAGS_TEMP_LO_ALARM	BIT(1)
#define CMIS_PG0_MODULE_FLAGS_TEMP_HI_WARN	BIT(2)
#define CMIS_PG0_MODULE_FLAGS_TEMP_LO_WARN	BIT(3)
#define CMIS_PG0_MODULE_FLAGS_VCC_HI_ALARM	BIT(4)
#define CMIS_PG0_MODULE_FLAGS_VCC_LO_ALARM	BIT(5)
#define CMIS_PG0_MODULE_FLAGS_VCC_HI_WARN	BIT(6)
#define CMIS_PG0_MODULE_FLAGS_VCC_LO_WARN	BIT(7)
#define CMIS_PG0_TEMP			0x0E
#define CMIS_PG0_VOLT			0x10
#define CMIS_PG0_CONN_TYPE			0xCB

/* CMIS (page 2) offsets */
#define CMIS_P02_HTEMP_ALARM_THRES		0x80
#define CMIS_P02_HTEMP_WARN_THRES		0x84

/* CMIS (page 11) offsets */
#define CMIS_P11_TX_FAULT			0x87
#define CMIS_P11_TX_LOS		0x88
#define CMIS_P11_TX_LOL		0x89
#define CMIS_P11_TX_EQ_FAIL		0x8A
#define CMIS_P11_TX_PWR_ALARMS		0x8B
#define CMIS_P11_TX_BIAS_ALARMS		0x8F
#define CMIS_P11_RX_LOS			0x93
#define CMIS_P11_RX_LOL		0x94
#define CMIS_P11_RX_PWR_ALARMS		0x95
#define CMIS_P11_TX_PWR			0x9A
#define CMIS_P11_TX_BIAS			0xAA
#define CMIS_P11_RX_PWR			0xBA

/* Copy a single byte into buf at offset eoff. */
#define BNXT_EEPROM_PUT_BYTE(buf, base, len, eoff, val)			\
	do {								\
		u16 _base = (u16)(base);				\
		u16 _eoff = (u16)(eoff);				\
		if (_eoff >= _base && _eoff < _base + (u16)(len))	\
			(buf)[_eoff - _base] = (u8)(val);		\
	} while (0)

/* Copy a 16-bit value into buf at offset eoff. */
#define BNXT_EEPROM_PUT_BE16(buf, base, len, eoff, val)			\
	do {								\
		u8  *_buf  = (buf);					\
		u16  _base = (u16)(base);				\
		u16  _len  = (u16)(len);				\
		u16  _eoff = (u16)(eoff);				\
		u16  _val  = (u16)(val);				\
		if (_eoff >= _base && _eoff < _base + _len)		\
			_buf[_eoff - _base] = _val >> 8;		\
		if (_eoff + 1 >= _base && _eoff + 1 < _base + _len)	\
			_buf[_eoff + 1 - _base] = _val & 0xFF;		\
	} while (0)

/*
 * Copy n bytes from src into buf. Only the intersection of the two ranges
 * is written.
 */
#define BNXT_EEPROM_PUT_BYTES(buf, base, len, eoff, src, n)		\
	do {								\
		u16 _base  = (u16)(base);				\
		u16 _eoff  = (u16)(eoff);				\
		u16 _start = max_t(u16, _eoff, _base);			\
		u16 _end   = min_t(u16, _eoff + (u16)(n),		\
				   _base + (u16)(len));			\
		if (_start < _end)					\
			memcpy((buf) + _start - _base,			\
			       (src) + _start - _eoff,			\
			       _end - _start);				\
	} while (0)

#endif
