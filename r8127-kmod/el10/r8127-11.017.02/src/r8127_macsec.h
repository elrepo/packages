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

#ifndef RTL8261D_MACSEC_H
#define RTL8261D_MACSEC_H

#define RTL8261D_CHANNEL_WAIT_COUNT 20000
#define RTL8261D_CHANNEL_WAIT_TIME 1  // 1us

#define RTL8261D_TXCNT_NUM 8
#define RTL8261D_RXCNT_NUM 20

#define RTL8261D_CHANNEL_DATA_BLEN 4
#define RTL8261D_TCAM_KEY_NUM 6
#define RTL8261D_TCAM_MASK_NUM 6

#define RTL8261D_HW_MAX_TCAM 64
#define RTL8261D_HW_MAX_RXSC 32
#define RTL8261D_HW_MAX_SECY 32
#define RTL8261D_HW_MAX_TXSA 64
#define RTL8261D_HW_MAX_RXSA 64

#define RTL8261D_SA_RAM_ENTRY_BSIZE 0x80
#define RTL8261D_SA_RAM_ENTRY_DATA_BSIZE 96

#define MACSEC_AES_GCM_128_SAK_BLEN 16
#define MACSEC_AES_GCM_256_SAK_BLEN 32
#define MACSEC_AES_ECB_SAK_BLEN 16
#define MACSEC_SECTAG_BLEN 16
#define MACSEC_SECTAG_BLEN_NOSCI 8
#define MACSEC_NUM_AN 4

#define NB 4 /* No. of columns in a state which is a constant in AES */
#define AES_BLOCK_LEN 16 /* Block length in AES is 128 bytes only */
#define AES_MAX_KEY_EXPN_SIZE  240 /** Maximum size of the expanded key */

typedef uint8_t state_t[4][4]; /* state - array holding the intermediate results */

typedef enum {
        RTL_AES128,
        RTL_AES192,
        RTL_AES256
} aes_key_types_t;

typedef struct {
        aes_key_types_t aes_key_type;   /** AES key type, Cipher is working on */
        uint32_t key_len;
        uint16_t no_of_rounds;          /** no of rounds */
        uint8_t no_of_key_words;        /** no of (32 bit) words in the key */
} aes_obj_t;

enum rtl8261d_speed_mode {
        legacy_mode = 0,
        xg_mode,
        unknown_mode,
};

enum rtl8261d_comp {
        INGRESS = 0,
        EGRESS,
        CONVERTER
};

enum rtl8261d_macsec_registers {
        MACSEC_EN= 0xDD6A,
        MACSEC_INTR_EN= 0xDE42,
        MACSEC_DATA_PATH = 0xDE08,
        MACSEC_IFG_SEL= 0xDC22,

        MACSEC_REG_ADDR = 0xDE20,
        MACSEC_REG_DATA_15_0 = 0xDE22,
        MACSEC_REG_DATA_23_16 = 0xDE24,
        MACSEC_REG_CTRL = 0xDE26,

        TCAM_KEY_base = 0x2000,
        TCAM_MASK_base = 0x2020,
        TCAM_POLICY_base = 0x3000,
        TCAM_ENTRY_EN_CTRL = 0x6100,

        RXCAM_SCI_LO_base = 0x3400,
        RXCAM_SCI_HI_base = 0x3404,
        RXCAM_CTRL_base = 0x3408,
        RXCAM_ENTRY_EN_CTRL = 0x3700,

        SAM_SC_SA_MAP1_base = 0x3800,
        SAM_SC_SA_MAP2_base = 0x3804,

        FLOW_CRTL_1 = 0x7000,
        FLOW_CRTL_2 = 0x7004,

        SYN_CORE = 0x6104,
};

enum rtl8261d_macsec_content {
        // MACSEC_REG_CTRL
        MACSEC_RD_REQ = (1 << 9),
        MACSEC_WR_REQ = (1 << 8),

        // MACSEC_IFG_SEL
        MACSEC_IFG_SEL_MASK = (1 << 15) | (1 << 14) | (1 << 13),

        // TCAM_ENTRY_EN_CTRL
        TCAM_CLR_EN = (1 << 30),
        TCAM_SET_EN = (1 << 14),

        // RXCAM_ENTRY_EN_CTRL
        RXCAM_CLR_EN = (1 << 30),
        RXCAM_SET_EN = (1 << 14),

        // SAM_SC_SA_MAP
        SA_IN_USE = (1 << 15),
        SA_UPD_TIME = (1 << 14),
        SA_INX_UPD = (1 << 13),

        // SAM_FLOW_CTRL1
        FLOW_ALLOW_DATA_PKTS = (1 << 21),
        FLOW_TX_SCB = (1 << 20),
        FLOW_TX_ES = (1 << 19),
        FLOW_TX_INCLIUDE_SCI = (1 << 18),
        FLOW_RX_ALLOW_UNTAG_DATA = (1 << 18),
        FLOW_TX_CONF_PROTECT = (1 << 17),
        FLOW_RX_ALLOW_TAG_DATA = (1 << 17),
        FLOW_TX_PROTECT_FRAME = (1 << 16),
        FLOW_RX_REPLAY_PROTECT = (1 << 16),
        FLOW_TYPE_EGRESS = 0x3,
        FLOW_TYPE_INGRESS = 0x2,

        // Control word
        CtrlW_AUTH = (4 << 23),
        CtrlW_AN_MASK = (1 << 27) | (1 << 26),
        CtrlW_SEQ = (1 << 30),

        // SA Control word
        SACTRL_HWUPD_EN = (1 << 31),
        SACTRL_IDX_VAILD = (1 << 15),
        SACTRL_EXPIRED_IRQ = (1 << 14),

        // counter
        CNT_READ_CLR = (1 << 3),
        CNT_READ_OVERSTOP = (1 << 2),

        // SYN_CORE
        SYN_LOAD_SAFE = (1 << 31),
        SYN_FINISH_MASK = 0x3F,
};

struct rtl8261d_txsa_stats {
        u64 OutPktsProtected;
        u64 OutPktsEncrypted;
};

struct rtl8261d_rxsa_stats {
        u64 InPktsOK;
        u64 InPktsInvalid;
        u64 InPktsNotValid;
        u64 InPktsNotUsingSA;
        u64 InPktsUnusedSA;
};

struct rtl8261d_txsc_stats {
        u64 OutPktsProtected;
        u64 OutPktsEncrypted;
        u64 OutOctetsProtected;
        u64 OutOctetsEncrypted;
};

struct rtl8261d_rxsc_stats {
        u64 InOctetsValidated;
        u64 InOctetsDecrypted;
        u64 InPktsUnchecked;
        u64 InPktsDelayed;
        u64 InPktsLate;
        u64 InPktsOK;
        u64 InPktsInvalid;
        u64 InPktsNotValid;
        u64 InPktsNotUsingSA;
        u64 InPktsUnusedSA;
};

struct rtl8261d_secy_stats {
        u64 OutPktsUntagged;
        u64 InPktsUntagged;
        u64 OutPktsTooLong;
        u64 InPktsNoTag;
        u64 InPktsBadTag;
        u64 InPktsNoSCI;
        u64 InPktsUnknownSCI;
};

struct rtl8261d_macsec_txsa {
        bool inuse;
        bool hw_added;
        bool active;

        struct rtl8261d_txsa_stats stats;
};

struct rtl8261d_macsec_rxsa {
        bool inuse;
        bool hw_added;
        bool active;

        struct rtl8261d_rxsa_stats stats;
};

struct rtl8261d_macsec_rxsc {
        const struct macsec_rx_sc *kernel_rxsc;
        bool inuse;
        bool active;
        int rx_sa_idxs[MACSEC_NUM_AN];

        struct rtl8261d_rxsc_stats stats;
};

struct rtl8261d_macsec_secy {
        const struct macsec_secy *kernel_secy;
        bool inuse;
        int ifg;

        struct rtl8261d_secy_stats secy_stats;

        // tx_sc
        bool encrypt;
        int encoding_sa;
        int tx_sa_idxs[MACSEC_NUM_AN];
        struct rtl8261d_txsc_stats stats;

        // rx_sc
        int rx_sc_idxs[RTL8261D_HW_MAX_RXSC];
        int rx_sc_count;
        // rx_sa
        u32 replay_window;
};

struct rtl8261d_macsec_tcam {
        bool inuse;

        int secy_idx;
};

struct rtl8261d_macsec_private {
        int cur_ifg;

        struct rtl8261d_macsec_tcam tcam[RTL8261D_HW_MAX_TCAM];
        struct rtl8261d_macsec_secy secy[RTL8261D_HW_MAX_SECY];
        struct rtl8261d_macsec_rxsc rxsc[RTL8261D_HW_MAX_RXSC];
        struct rtl8261d_macsec_txsa txsa[RTL8261D_HW_MAX_TXSA];
        struct rtl8261d_macsec_rxsa rxsa[RTL8261D_HW_MAX_RXSA];
};

int rtl8127_macsec_set_fix_latency(struct rtl8127_private *tp);

int rtl8127_macsec_init(struct net_device *netdev);
void rtl8127_macsec_remove(struct net_device *netdev);

#endif /* RTL8261D_MACSEC_H */
