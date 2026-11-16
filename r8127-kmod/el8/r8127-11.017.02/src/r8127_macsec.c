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
#include "r8127_macsec.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0)
#include <net/macsec.h>

#include "r8127_mdio.h"

static const uint8_t sbox[256] = {
        /*
        | 0  |   1  |   2  |   3  |   4  |   5  |   6  |   7  |   8  |   9  |   a  |   b  |   c  |  d   |  e   |  f   |
        |----|------|------|------|------|------|------|------|------|------|------|------|------|------|------|------|     */
        0x63,  0x7c,  0x77,  0x7b,  0xf2,  0x6b,  0x6f,  0xc5,  0x30,  0x01,  0x67,  0x2b,  0xfe,  0xd7,  0xab,  0x76, /* 0 */
        0xca,  0x82,  0xc9,  0x7d,  0xfa,  0x59,  0x47,  0xf0,  0xad,  0xd4,  0xa2,  0xaf,  0x9c,  0xa4,  0x72,  0xc0, /* 1 */
        0xb7,  0xfd,  0x93,  0x26,  0x36,  0x3f,  0xf7,  0xcc,  0x34,  0xa5,  0xe5,  0xf1,  0x71,  0xd8,  0x31,  0x15, /* 2 */
        0x04,  0xc7,  0x23,  0xc3,  0x18,  0x96,  0x05,  0x9a,  0x07,  0x12,  0x80,  0xe2,  0xeb,  0x27,  0xb2,  0x75, /* 3 */
        0x09,  0x83,  0x2c,  0x1a,  0x1b,  0x6e,  0x5a,  0xa0,  0x52,  0x3b,  0xd6,  0xb3,  0x29,  0xe3,  0x2f,  0x84, /* 4 */
        0x53,  0xd1,  0x00,  0xed,  0x20,  0xfc,  0xb1,  0x5b,  0x6a,  0xcb,  0xbe,  0x39,  0x4a,  0x4c,  0x58,  0xcf, /* 5 */
        0xd0,  0xef,  0xaa,  0xfb,  0x43,  0x4d,  0x33,  0x85,  0x45,  0xf9,  0x02,  0x7f,  0x50,  0x3c,  0x9f,  0xa8, /* 6 */
        0x51,  0xa3,  0x40,  0x8f,  0x92,  0x9d,  0x38,  0xf5,  0xbc,  0xb6,  0xda,  0x21,  0x10,  0xff,  0xf3,  0xd2, /* 7 */
        0xcd,  0x0c,  0x13,  0xec,  0x5f,  0x97,  0x44,  0x17,  0xc4,  0xa7,  0x7e,  0x3d,  0x64,  0x5d,  0x19,  0x73, /* 8 */
        0x60,  0x81,  0x4f,  0xdc,  0x22,  0x2a,  0x90,  0x88,  0x46,  0xee,  0xb8,  0x14,  0xde,  0x5e,  0x0b,  0xdb, /* 9 */
        0xe0,  0x32,  0x3a,  0x0a,  0x49,  0x06,  0x24,  0x5c,  0xc2,  0xd3,  0xac,  0x62,  0x91,  0x95,  0xe4,  0x79, /* a */
        0xe7,  0xc8,  0x37,  0x6d,  0x8d,  0xd5,  0x4e,  0xa9,  0x6c,  0x56,  0xf4,  0xea,  0x65,  0x7a,  0xae,  0x08, /* b */
        0xba,  0x78,  0x25,  0x2e,  0x1c,  0xa6,  0xb4,  0xc6,  0xe8,  0xdd,  0x74,  0x1f,  0x4b,  0xbd,  0x8b,  0x8a, /* c */
        0x70,  0x3e,  0xb5,  0x66,  0x48,  0x03,  0xf6,  0x0e,  0x61,  0x35,  0x57,  0xb9,  0x86,  0xc1,  0x1d,  0x9e, /* d */
        0xe1,  0xf8,  0x98,  0x11,  0x69,  0xd9,  0x8e,  0x94,  0x9b,  0x1e,  0x87,  0xe9,  0xce,  0x55,  0x28,  0xdf, /* e */
        0x8c,  0xa1,  0x89,  0x0d,  0xbf,  0xe6,  0x42,  0x68,  0x41,  0x99,  0x2d,  0x0f,  0xb0,  0x54,  0xbb,  0x16, /* f */
};

#define GET_SBOX_VALUE(num) (sbox[(num)])

static const uint8_t round_constant[11] = {
        0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

static void aes_obj_init(aes_obj_t *const aes_obj, const unsigned int key_len)
{
        memset(aes_obj, 0, sizeof(aes_obj_t));

        if (key_len == 16)
                aes_obj->aes_key_type = RTL_AES128;
        else if (key_len == 24)
                aes_obj->aes_key_type = RTL_AES192;
        else if (key_len == 32)
                aes_obj->aes_key_type = RTL_AES256;

        aes_obj->no_of_key_words = key_len / 4;
        aes_obj->no_of_rounds = key_len / 4 + 6;
        aes_obj->key_len = key_len;
}

static void substitute_word(uint8_t word[])
{
        uint8_t i;

        for (i = 0; i < NB; i++)
                word[i] = GET_SBOX_VALUE(word[i]);
}

static void rotate_word(uint8_t word[], const uint8_t no_of_rotations)
{
        uint8_t i = 0;

        for (i = 0; i < no_of_rotations; i++) {
                swap(word[0], word[1]);
                swap(word[1], word[2]);
                swap(word[2], word[3]);
        }
}

static void aes_substitute_bytes(state_t *const state)
{
        uint8_t i, j;

        for (i = 0; i < 4; i++) {
                for (j = 0; j < NB; j++)
                        (*state)[j][i] = GET_SBOX_VALUE((*state)[j][i]);
        }
}

static void aes_shift_rows(uint8_t *const state)
{
        uint8_t tmp[16];
        int i = 0;

        tmp[0] = state[0];
        tmp[1] = state[5];
        tmp[2] = state[10];
        tmp[3] = state[15];

        tmp[4] = state[4];
        tmp[5] = state[9];
        tmp[6] = state[14];
        tmp[7] = state[3];

        tmp[8] = state[8];
        tmp[9] = state[13];
        tmp[10] = state[2];
        tmp[11] = state[7];

        tmp[12] = state[12];
        tmp[13] = state[1];
        tmp[14] = state[6];
        tmp[15] = state[11];

        for (i = 0; i < 16; i++)
                state[i] = tmp[i];
}

/**
 * Multiplies the argument by 2 in GF(2^8)
 *
 * The multiplications implicit within the MixColumns operation are GF(2^8)
 * multiplication operations
 */
static uint8_t aes_xtime(const uint8_t val)
{
        uint8_t val1 = (uint8_t)(val << 1);
        uint8_t val2 = (uint8_t)(val >> 7);

        return val1 ^ ((val2 & 1) * 0x1B);
}

/* Performs the mix columns diffusion
 *
 * Takes a 16 char byte array and transforms to new 16 byte char array that
 * represents the GF(256) matrix multiplication of a known matrix times the
 * state. The byte array represents a 4x4 matrix where the first 4 entries
 * represents the first column.
 *
 * state: 16 unsigned char byte array to transform
 */
static void aes_mix_columns(state_t *const state)
{
        uint8_t i = 0;

        for (i = 0; i < 4; ++i) {
                uint8_t temp1 = 0, temp2 = 0, temp3 = 0;

                temp3 = (*state)[i][0];
                temp1 = (*state)[i][0] ^ (*state)[i][1] ^ (*state)[i][2] ^ (*state)[i][3];

                temp2 = aes_xtime((*state)[i][0] ^ (*state)[i][1]);

                (*state)[i][0] = (*state)[i][0] ^ (temp2 ^ temp1);

                temp2 = aes_xtime((*state)[i][1] ^ (*state)[i][2]);

                (*state)[i][1] = (*state)[i][1] ^ (temp2 ^ temp1);

                temp2 = aes_xtime((*state)[i][2] ^ (*state)[i][3]);

                (*state)[i][2] = (*state)[i][2] ^ (temp2 ^ temp1);

                temp2 = aes_xtime((*state)[i][3] ^ temp3);

                (*state)[i][3] = (*state)[i][3] ^ (temp2 ^ temp1);
        }
}

static void aes_key_schedule(const aes_obj_t *const aes_obj, uint8_t expanded_key[],
                             const uint8_t *const input_key)
{
        uint16_t i, j;
        uint8_t temp[4];
        uint16_t key_offset = 4 * aes_obj->no_of_key_words;

        /* Input key is copied as the first key of the expanded key */
        for (i = 0; i < key_offset ; i++)
                expanded_key[i] = input_key[i];

        /**
        * Each added word depends on the immediately preceding word, and the
        * word four positions back
        */
        for (i = (uint16_t)(NB * aes_obj->no_of_key_words);
             i < (4 * NB *(aes_obj->no_of_rounds + 1));
             i = (uint16_t)(i + 4)) {
                for (j = 0; j < NB; j++)
                        temp[j] = expanded_key[i - 4 + j];

                if (i / NB % aes_obj->no_of_key_words == 0) {
                        rotate_word(temp, 1);

                        substitute_word(temp);

                        temp[0] = temp[0] ^ round_constant[i / key_offset];
                } else if ((aes_obj->no_of_key_words > 6) &&
                           (i / 4 % aes_obj->no_of_key_words == 4)) {
                        substitute_word(temp);
                }

                for (j = 0; j < NB; j++) {
                        expanded_key[i + j] = expanded_key[i + j - key_offset] ^ temp[j];
                }
        }
}

static void aes_add_round_key(state_t *const state, const uint8_t *const round_key)
{
        uint8_t i, j;

        for (i = 0; i < 4; i++) {
                for (j = 0; j < NB; j++)
                        (*state)[i][j] ^= round_key[(i * NB) + j];
        }
}

static void AES_Encrypt(const uint8_t *const pInput, uint8_t *const pOutput,
                        const uint8_t *const pKey, const unsigned int nKeyLen)
{
        aes_obj_t aes_obj;
        uint8_t expanded_key[AES_MAX_KEY_EXPN_SIZE] = {0};
        uint8_t round = 0;

        if (nKeyLen != 16 && nKeyLen != 24 && nKeyLen != 32)
                return;

        if (NULL == pInput || NULL == pOutput || NULL == pKey)
                return;

        /** Initialise the AES object with the required configuration */
        aes_obj_init(&aes_obj, nKeyLen);

        /** Prepare the round keys */
        aes_key_schedule(&aes_obj, expanded_key, pKey);

        /*
        * Copy the plain data to the output buff. When encryption operation is
        * completed, the output buffer contains the encrypted data
        */
        memcpy(pOutput, pInput, AES_BLOCK_LEN);

        /* Add the First round key to the state before starting the rounds */
        aes_add_round_key((state_t *const)pOutput, expanded_key);

        /* There will be Nr rounds.
        * The first Nr-1 rounds are identical.
        * These Nr-1 rounds are executed in the loop below.
        */
        for (round = 1; round < aes_obj.no_of_rounds; round++) {
                aes_substitute_bytes((state_t *)pOutput);
                aes_shift_rows(pOutput);
                aes_mix_columns((state_t *)pOutput);
                aes_add_round_key((state_t *)pOutput, expanded_key + (round * 16));
        }

        aes_substitute_bytes((state_t *)pOutput);
        aes_shift_rows(pOutput);
        aes_add_round_key((state_t *)pOutput, expanded_key + (round * 16));

        /* The round keys allow recovering pKey, and returning does not clear
         * the stack. Scrub explicitly so the schedule is not left behind for
         * whatever reuses this stack depth next.
         */
        memzero_explicit(expanded_key, sizeof(expanded_key));
}

/**
 * @ data: read 32 bit data
 * return value < 0 -> fail
 * return value == 0 -> success
 */
static int rtl8261d_macsec_comp_read(struct phy_device *phydev, u16 comp_addr,
                                     u32 *data, enum rtl8261d_comp comp)
{
        int i, val, ret;
        u16 ctrl_val;

        ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_REG_ADDR, comp_addr);
        if (ret < 0)
                return ret;

        switch (comp) {
        case INGRESS:
                ctrl_val = 0x0200;
                break;
        case EGRESS:
                ctrl_val = 0x0600;
                break;
        case CONVERTER:
                ctrl_val = 0x0a00;
                break;
        default:
                return -EINVAL;
        }

        ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_REG_CTRL, ctrl_val);
        if (ret < 0)
                return ret;

        for (i = 0; i < RTL8261D_CHANNEL_WAIT_COUNT; i++) {

                udelay(RTL8261D_CHANNEL_WAIT_TIME);

                val = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_REG_CTRL);
                if (val < 0)
                        return val;

                if (!((u32)val & MACSEC_RD_REQ)) {
                        *data = (((u32)val & 0x000000FF) << 24);
                        break;
                }
        }
        if (i == RTL8261D_CHANNEL_WAIT_COUNT)
                return -ETIMEDOUT;

        val = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_REG_DATA_15_0);
        if (val < 0)
                return val;
        *data |= ((u32)val & 0x0000FFFF);

        val = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_REG_DATA_23_16);
        if (val < 0)
                return val;
        *data |= (((u32)val & 0x000000FF) << 16);

        return 0;
}

/**
 * @ data: write 32 bit data
 * return value < 0 -> fail
 * return value == 0 -> success
 */
static int rtl8261d_macsec_comp_write(struct phy_device *phydev, u16 comp_addr,
                                      u32 data, enum rtl8261d_comp comp)
{
        int i, val, ret;
        u16 ctrl_val;

        ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_REG_ADDR, comp_addr);
        if (ret < 0)
                return ret;

        ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_REG_DATA_15_0,
                                    data & 0x0000FFFF);
        if (ret < 0)
                return ret;

        ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_REG_DATA_23_16,
                                    (data & 0x00FF0000) >> 16);
        if (ret < 0)
                return ret;

        switch (comp) {
        case INGRESS:
                ctrl_val = 0x0100 | ((data & 0xFF000000) >> 24);
                break;
        case EGRESS:
                ctrl_val = 0x0500 | ((data & 0xFF000000) >> 24);
                break;
        case CONVERTER:
                ctrl_val = 0x0900 | ((data & 0xFF000000) >> 24);
                break;
        default:
                return -EINVAL;
        }

        ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_REG_CTRL, ctrl_val);
        if (ret < 0)
                return ret;

        for (i = 0; i < RTL8261D_CHANNEL_WAIT_COUNT; i++) {

                udelay(RTL8261D_CHANNEL_WAIT_TIME);

                val = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_REG_CTRL);
                if (val < 0)
                        return val;

                if (!((u32)val & MACSEC_WR_REQ))
                        break;
        }
        if (i == RTL8261D_CHANNEL_WAIT_COUNT)
                return -ETIMEDOUT;

        return 0;
}

static int rtl8261d_wr_2x32_to_64(struct phy_device *phydev, u16 addrl, u16 addrh,
                                  u64 data, enum rtl8261d_comp comp)
{
        int ret;

        ret = rtl8261d_macsec_comp_write(phydev, addrl, (u32)data, comp);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, addrh, (u32)(data >> 32), comp);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_rd_2x32_to_64(struct phy_device *phydev, u16 addrl, u16 addrh,
                                  u64 *data, enum rtl8261d_comp comp)
{
        int val;
        u32 data_low, data_high;

        val = rtl8261d_macsec_comp_read(phydev, addrl, &data_low, comp);
        if (val < 0)
                return val;

        val = rtl8261d_macsec_comp_read(phydev, addrh, &data_high, comp);
        if (val < 0)
                return val;

        *data = ((u64)data_high << 32) | (u64)data_low;

        return 0;
}

static int rtl8261d_ifg_to_reg(int ifg)
{
        switch (ifg) {
        case 16:
                return (0 << 13);
        case 24:
                return (1 << 13);
        case 32:
                return (2 << 13);
        default:
                pr_debug(KBUILD_MODNAME ": IFG length doesn't support\n");
                return -EINVAL;
        }
}

static int rtl8261d_set_ifg(struct phy_device *phydev, int ifg)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        int ret, val;

        val = rtl8261d_ifg_to_reg(ifg);
        if (val < 0)
                return val;

        if (ifg <= macsec->cur_ifg)
                return 0;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_IFG_SEL,
                                     MACSEC_IFG_SEL_MASK, (u16)val);
        if (ret < 0)
                return ret;

        macsec->cur_ifg = ifg;

        return 0;
}

static int rtl8261d_upd_ifg(struct phy_device *phydev)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        int ret, val, i, ifg = 0;

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                if (macsec->secy[i].ifg > ifg)
                        ifg = macsec->secy[i].ifg;
        }

        if (ifg == 0) {
                macsec->cur_ifg = 0;
                return 0;
        }

        if (ifg == macsec->cur_ifg)
                return 0;

        val = rtl8261d_ifg_to_reg(ifg);
        if (val < 0)
                return val;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_IFG_SEL,
                                     MACSEC_IFG_SEL_MASK, (u16)val);
        if (ret < 0)
                return ret;

        macsec->cur_ifg = ifg;

        return 0;
}

static int rtl8261d_set_key(struct phy_device *phydev, u16 key_len, u8 key[],
                            u16 base_sa_ram, enum rtl8261d_comp comp)
{
        int i, ret;
        u8 plaintext[16] = {0}, hkey[16] = {0};

        switch (key_len) {
        case MACSEC_AES_GCM_128_SAK_BLEN:
                for (i = 0; i < MACSEC_AES_GCM_128_SAK_BLEN / RTL8261D_CHANNEL_DATA_BLEN; i++) {
                        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + (2 + i) * 4,
                                                         get_unaligned_le32(&key[i * 4]), comp);
                        if (ret < 0)
                                goto out;
                }

                for (i = 4; i < MACSEC_AES_GCM_256_SAK_BLEN / RTL8261D_CHANNEL_DATA_BLEN; i++) {
                        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + (2 + i) * 4,
                                                         0x00000000, comp);
                        if (ret < 0)
                                goto out;
                }

                break;

        case MACSEC_AES_GCM_256_SAK_BLEN:
                for (i = 0; i < MACSEC_AES_GCM_256_SAK_BLEN / RTL8261D_CHANNEL_DATA_BLEN; i++) {
                        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + (2 + i) * 4,
                                                         get_unaligned_le32(&key[i * 4]), comp);

                        if (ret < 0)
                                goto out;
                }

                break;

        default:
                pr_debug(KBUILD_MODNAME ": Key length not support\n");
                ret = -EINVAL;
                goto out;
        }

        AES_Encrypt(plaintext, hkey, key, key_len);

        for (i = 0; i < MACSEC_AES_ECB_SAK_BLEN / RTL8261D_CHANNEL_DATA_BLEN; i++) {
                ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + (10 + i) * 4,
                                                 get_unaligned_le32(&hkey[i * 4]), comp);
                if (ret < 0)
                        goto clear_key_out;
        }

clear_key_out:
        memzero_explicit(hkey, sizeof(hkey));
out:
        return ret;
}

static int rtl8261d_set_replay_mask(struct phy_device *phydev,
                                    u16 base_sa_ram, u32 replay_windows)
{
        int ret;

        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 16 * 4,
                                         replay_windows, INGRESS);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_set_pn(struct phy_device *phydev, u16 base_sa_ram,
                           u64 pn, enum rtl8261d_comp comp)
{
        int ret;

        pn--;

        ret = rtl8261d_wr_2x32_to_64(phydev, base_sa_ram + 14 * 4,
                                     base_sa_ram + 15 * 4, pn, comp);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_set_txsa_kernel_pn(struct phy_device *phydev, u16 base_sa_ram,
                                       struct macsec_tx_sa *ctx_tx_sa)
{
        int ret;
        u64 pn;

        ret = rtl8261d_rd_2x32_to_64(phydev, base_sa_ram + 14 * 4, base_sa_ram + 15 * 4,
                                     &pn, EGRESS);
        if (ret < 0)
                return ret;

        ctx_tx_sa->next_pn = pn + 1;

        return 0;
}

static int rtl8261d_set_rxsa_kernel_pn(struct phy_device *phydev, u16 base_sa_ram,
                                       struct macsec_rx_sa *ctx_rx_sa)
{
        int ret;
        u64 pn;

        ret = rtl8261d_rd_2x32_to_64(phydev, base_sa_ram + 14 * 4, base_sa_ram + 15 * 4,
                                     &pn, INGRESS);
        if (ret < 0)
                return ret;

        ctx_rx_sa->next_pn = pn + 1;

        return 0;
}

static int rtl8261d_add_flowctrl1_entry(struct phy_device *phydev, int secy_idx,
                                        const struct macsec_secy *ctx_secy)
{
        int ret;
        u32 channel_data;

        channel_data = ((1 << 31) | FLOW_ALLOW_DATA_PKTS |
                        (ctx_secy->tx_sc.scb ? FLOW_TX_SCB : 0) |
                        (ctx_secy->tx_sc.end_station ? FLOW_TX_ES : 0) |
                        (ctx_secy->tx_sc.send_sci ? FLOW_TX_INCLIUDE_SCI : 0) |
                        (ctx_secy->tx_sc.encrypt ? FLOW_TX_CONF_PROTECT : 0) |
                        (ctx_secy->protect_frames ? FLOW_TX_PROTECT_FRAME : 0) |
                        FLOW_TYPE_EGRESS);
        ret = rtl8261d_macsec_comp_write(phydev, FLOW_CRTL_1 + secy_idx * 8,
                                         channel_data, EGRESS);
        if (ret < 0)
                return ret;

        channel_data = ((ctx_secy->validate_frames << 19) |
                        FLOW_RX_ALLOW_UNTAG_DATA | FLOW_RX_ALLOW_TAG_DATA |
                        (ctx_secy->replay_protect ? FLOW_RX_REPLAY_PROTECT : 0) |
                        FLOW_TYPE_INGRESS);
        ret = rtl8261d_macsec_comp_write(phydev, FLOW_CRTL_1 + secy_idx * 8,
                                         channel_data, INGRESS);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_add_flowctrl2_entry(struct phy_device *phydev, int secy_idx)
{
        int ret;

        ret = rtl8261d_macsec_comp_write(phydev, FLOW_CRTL_2 + secy_idx * 8,
                                         0x000c0c00, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, FLOW_CRTL_2 + secy_idx * 8,
                                         0x000c0c00, INGRESS);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_clear_flowctl_entry(struct phy_device *phydev, int secy_idx)
{
        int ret;

        ret = rtl8261d_macsec_comp_write(phydev, FLOW_CRTL_1 + secy_idx * 8,
                                         0x00000000, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, FLOW_CRTL_1 + secy_idx * 8,
                                         0x00000000, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, FLOW_CRTL_2 + secy_idx * 8,
                                         0x00000000, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, FLOW_CRTL_2 + secy_idx * 8,
                                         0x00000000, INGRESS);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_enable_tcam(struct phy_device *phydev, int tcam_idx,
                                bool enable, enum rtl8261d_comp comp)
{
        int ret;
        u32 channel_data;

        if (enable)
                channel_data = (TCAM_SET_EN | (u32)tcam_idx);
        else
                channel_data = (TCAM_CLR_EN | ((u32)tcam_idx << 16));

        ret = rtl8261d_macsec_comp_write(phydev, TCAM_ENTRY_EN_CTRL,
                                         channel_data, comp);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_del_tcam_entry(struct phy_device *phydev,
                                   struct rtl8261d_macsec_tcam *tcam, int secy_idx)
{
        int i, ret, tcam_idx = -1;

        for (i = 0; i < RTL8261D_HW_MAX_TCAM; i++) {
                if (!tcam[i].inuse || (tcam[i].secy_idx != secy_idx))
                        continue;

                tcam_idx = i;

                ret = rtl8261d_enable_tcam(phydev, tcam_idx, false, EGRESS);
                if (ret < 0)
                        return ret;

                ret = rtl8261d_enable_tcam(phydev, tcam_idx, false, INGRESS);
                if (ret < 0)
                        return ret;

                tcam[tcam_idx].inuse = false;
                tcam[tcam_idx].secy_idx = -1;
        }
        if (tcam_idx == -1) {
                pr_debug(KBUILD_MODNAME ": No tcam in the hw space, impossible\n");
                return -EINVAL;
        }

        return 0;
}

static int rtl8261d_add_tcam_entry(struct phy_device *phydev, int tcam_idx,
                                   int secy_idx, enum rtl8261d_comp comp)
{
        int val, ret, i;
        u32 channel_data, priority = 0;

        val = rtl8261d_macsec_comp_read(phydev, TCAM_POLICY_base + tcam_idx * 0x4,
                                        &channel_data, comp);
        if (val < 0)
                return val;
        channel_data &= ~(((1 << 29) | (1 << 28) | (1 << 27)) | (0xFF));
        channel_data |= ((priority << 27) | (u32)secy_idx);
        ret = rtl8261d_macsec_comp_write(phydev, TCAM_POLICY_base + tcam_idx * 0x4,
                                         channel_data, comp);
        if (ret < 0)
                return ret;

        for (i = 0; i < RTL8261D_TCAM_KEY_NUM; i++) {
                ret = rtl8261d_macsec_comp_write(phydev,
                                                 (TCAM_KEY_base + i * 4) + tcam_idx * 0x40,
                                                 0x00000000, comp);
                if (ret < 0)
                        return ret;
        }

        for (i = 0; i < RTL8261D_TCAM_MASK_NUM; i++) {
                ret = rtl8261d_macsec_comp_write(phydev,
                                                 (TCAM_MASK_base + i * 4) + tcam_idx * 0x40,
                                                 0x00000000, comp);
                if (ret < 0)
                        return ret;
        }

        return 0;
}

static int rtl8261d_enable_rxcam(struct phy_device *phydev, int rxsc_idx, bool enable)
{
        int ret;
        u32 channel_data;

        if (enable)
                channel_data = (RXCAM_SET_EN | (u32)rxsc_idx);
        else
                channel_data = (RXCAM_CLR_EN | ((u32)rxsc_idx << 16));

        ret = rtl8261d_macsec_comp_write(phydev, RXCAM_ENTRY_EN_CTRL,
                                         channel_data, INGRESS);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_add_rxcam_entry(struct phy_device *phydev, int rxsc_idx,
                                    int parent_secy_idx, sci_t ctx_sci)
{
        int ret;
        u64 sci;

        sci = cpu_to_le64((__force u64)ctx_sci);

        ret = rtl8261d_wr_2x32_to_64(phydev, RXCAM_SCI_LO_base + rxsc_idx * 16,
                                     RXCAM_SCI_HI_base + rxsc_idx * 16, sci, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, RXCAM_CTRL_base + rxsc_idx * 16,
                                         (u32)parent_secy_idx, INGRESS);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_add_scsa_mapping_entry(struct phy_device *phydev, u16 sam_sc_sa_map_reg,
                u32 shift, bool *hw_added, int sa_idx,
                bool active, enum rtl8261d_comp comp)
{
        int ret, val;
        u32 channel_data;

        val = rtl8261d_macsec_comp_read(phydev, sam_sc_sa_map_reg, &channel_data, comp);
        if (val < 0)
                return val;

        channel_data &= ~(0xFFFF << shift);
        channel_data |= ((u32)(sa_idx) << shift);

        if (!(*hw_added))
                channel_data |= (SA_UPD_TIME << shift);

        if (active)
                channel_data |= (SA_IN_USE << shift);

        ret = rtl8261d_macsec_comp_write(phydev, sam_sc_sa_map_reg, channel_data, comp);
        if (ret < 0)
                return ret;

        *hw_added = true;

        return 0;
}

static int rtl8261d_seamless_add_scsa_mapping_entry(struct phy_device *phydev,
                u16 sam_sc_sa_map_reg, u32 shift,
                bool *hw_added, int sa_idx,
                enum rtl8261d_comp comp)
{
        int ret, val;
        u32 channel_data;

        val = rtl8261d_macsec_comp_read(phydev, sam_sc_sa_map_reg, &channel_data, comp);
        if (val < 0)
                return val;

        channel_data &= ~(0xFFFF << shift);

        channel_data |= ((SA_INX_UPD | sa_idx) << shift);

        if (!*(hw_added))
                channel_data |= (SA_UPD_TIME << shift);

        ret = rtl8261d_macsec_comp_write(phydev, sam_sc_sa_map_reg, channel_data, comp);
        if (ret < 0)
                return ret;

        *hw_added = true;

        return 0;
}

static int rtl8261d_check_sa_idx(struct phy_device *phydev, u16 sam_sc_sa_map_reg, u32 shift,
                                 int sa_idx, u32 *channel_data, enum rtl8261d_comp comp)
{
        int val;

        val = rtl8261d_macsec_comp_read(phydev, sam_sc_sa_map_reg, channel_data, comp);
        if (val < 0)
                return val;

        if ((((*channel_data >> shift) ^ sa_idx) & 0x3FF)) {
                pr_err(KBUILD_MODNAME ": Specified sa doesn't exist\n");
                return -EINVAL;
        }

        return 0;
}

static int rtl8261d_udp_scsa_mapping_entry(struct phy_device *phydev, u16 sam_sc_sa_map_reg,
                u32 shift, int sa_idx, bool active,
                enum rtl8261d_comp comp)
{
        int ret;
        u32 channel_data;

        ret = rtl8261d_check_sa_idx(phydev, sam_sc_sa_map_reg, shift, sa_idx, &channel_data, comp);
        if (ret < 0)
                return ret;

        channel_data &= ~((SA_IN_USE | SA_UPD_TIME | SA_INX_UPD) << shift);

        if (active)
                channel_data |= (SA_IN_USE << shift);

        ret = rtl8261d_macsec_comp_write(phydev, sam_sc_sa_map_reg, channel_data, comp);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_del_scsa_mapping_entry(struct phy_device *phydev, u16 sam_sc_sa_map_reg,
                u32 shift, int sa_idx, enum rtl8261d_comp comp)
{
        int ret;
        u32 channel_data;

        ret = rtl8261d_check_sa_idx(phydev, sam_sc_sa_map_reg, shift, sa_idx, &channel_data, comp);
        if (ret < 0)
                return ret;

        channel_data &= ~(0xFFFF << shift);

        ret = rtl8261d_macsec_comp_write(phydev, sam_sc_sa_map_reg, channel_data, comp);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_clear_scsa_mapping(struct phy_device *phydev, int sc_idx,
                                       enum rtl8261d_comp comp)
{
        int ret;

        ret = rtl8261d_macsec_comp_write(phydev, SAM_SC_SA_MAP1_base + sc_idx * 8,
                                         0x00000000, comp);
        if (ret < 0)
                return ret;

        if (comp == INGRESS) {
                ret = rtl8261d_macsec_comp_write(phydev, SAM_SC_SA_MAP2_base + sc_idx * 8,
                                                 0x00000000, INGRESS);
                if (ret < 0)
                        return ret;
        }

        return 0;
}

static int rtl8261d_add_tx_sa_ram(struct phy_device *phydev, int txsa_idx,
                                  int parent_secy_idx, struct macsec_context *ctx)
{
        const struct macsec_tx_sa *ctx_tx_sa = ctx->sa.tx_sa;
        const struct macsec_secy *ctx_secy = ctx->secy;
        int ret;
        u16 base_sa_ram;
        u32 channel_data;

        base_sa_ram = RTL8261D_SA_RAM_ENTRY_BSIZE * txsa_idx;

        channel_data = 0x0000A006;

        switch (ctx_secy->key_len) {
        case MACSEC_AES_GCM_128_SAK_BLEN:
                channel_data |= (5 << 17);
                break;
        case MACSEC_AES_GCM_256_SAK_BLEN:
                channel_data |= (7 << 17);
                break;
        default:
                pr_debug(KBUILD_MODNAME ": Key length not support\n");
                return -EINVAL;
        }

        channel_data |= CtrlW_AUTH;
        channel_data |= (ctx->sa.assoc_num << 26);

        if (!ctx_secy->xpn)
                channel_data |= (1 << 28);
        else
                channel_data |= (2 << 28);

        channel_data &= ~CtrlW_SEQ;

        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 0 * 4,
                                         channel_data, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 1 * 4,
                                         0x00000000, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_set_key(phydev, ctx_secy->key_len, ctx->sa.key, base_sa_ram, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_set_pn(phydev, base_sa_ram, ctx_tx_sa->next_pn, EGRESS);
        if (ret < 0)
                return ret;

        channel_data = (SACTRL_HWUPD_EN | (u32)(parent_secy_idx << 16) | SACTRL_EXPIRED_IRQ);

        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 16 * 4,
                                         channel_data, EGRESS);
        if (ret < 0)
                return ret;

        if (!ctx_secy->xpn) {
                ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 17 * 4,
                                                 0x00000000, EGRESS);
                if (ret < 0)
                        return ret;

                ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 18 * 4,
                                                 0x00000000, EGRESS);
                if (ret < 0)
                        return ret;

                ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 19 * 4,
                                                 0x00000000, EGRESS);
                if (ret < 0)
                        return ret;
        } else {
                channel_data = cpu_to_le32((__force u32)(ctx_tx_sa->ssci ^
                                           ctx_tx_sa->key.salt.ssci));
                ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 17 * 4,
                                                 channel_data, EGRESS);
                if (ret < 0)
                        return ret;

                ret = rtl8261d_wr_2x32_to_64(phydev, base_sa_ram + 18 * 4,
                                             base_sa_ram + 19 * 4,
                                             cpu_to_le64((__force u64)ctx_tx_sa->key.salt.pn),
                                             EGRESS);
                if (ret < 0)
                        return ret;
        }

        ret = rtl8261d_wr_2x32_to_64(phydev, base_sa_ram + 20 * 4, base_sa_ram + 21 * 4,
                                     cpu_to_le64((__force u64)ctx_secy->sci), EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 22 * 4,
                                         0x00000000, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 23 * 4,
                                         0x00000000, EGRESS);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_add_rx_sa_ram(struct phy_device *phydev, int rxsa_idx,
                                  int parent_rxsc_idx, struct macsec_context *ctx)
{
        const struct macsec_rx_sa *ctx_rx_sa = ctx->sa.rx_sa;
        const struct macsec_rx_sc *ctx_rx_sc = ctx_rx_sa->sc;
        const struct macsec_secy *ctx_secy = ctx->secy;
        int ret;
        u16 base_sa_ram;
        u32 channel_data;

        base_sa_ram = RTL8261D_SA_RAM_ENTRY_BSIZE * rxsa_idx;

        channel_data = 0x0000A00F;

        switch (ctx_secy->key_len) {
        case MACSEC_AES_GCM_128_SAK_BLEN:
                channel_data |= (5 << 17);
                break;
        case MACSEC_AES_GCM_256_SAK_BLEN:
                channel_data |= (7 << 17);
                break;
        default:
                pr_debug(KBUILD_MODNAME ": Key length not support\n");
                return -EINVAL;
        }

        channel_data |= CtrlW_AUTH;
        channel_data &= ~CtrlW_AN_MASK;

        if (!ctx_secy->xpn)
                channel_data |= (1 << 28);
        else
                channel_data |= (2 << 28);

        channel_data |= CtrlW_SEQ;

        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 0 * 4,
                                         channel_data, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 1 * 4,
                                         0x00000000, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_set_key(phydev, ctx_secy->key_len, ctx->sa.key, base_sa_ram, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_set_pn(phydev, base_sa_ram, ctx_rx_sa->next_pn, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_set_replay_mask(phydev, base_sa_ram, ctx_secy->replay_window);
        if (ret < 0)
                return ret;

        if (!ctx_secy->xpn) {
                ret = rtl8261d_wr_2x32_to_64(phydev, base_sa_ram + 17 * 4, base_sa_ram + 18 * 4,
                                             cpu_to_le64((__force u64)ctx_rx_sc->sci), INGRESS);
                if (ret < 0)
                        return ret;

                ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 19 * 4,
                                                 0x00000000, INGRESS);
                if (ret < 0)
                        return ret;
        } else {
                channel_data = cpu_to_le32((__force u32)(ctx_rx_sa->ssci ^
                                           ctx_rx_sa->key.salt.ssci));
                ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 17 * 4,
                                                 channel_data, INGRESS);
                if (ret < 0)
                        return ret;

                ret = rtl8261d_wr_2x32_to_64(phydev, base_sa_ram + 18 * 4, base_sa_ram + 19 * 4,
                                             cpu_to_le64((__force u64)ctx_rx_sa->key.salt.pn),
                                             INGRESS);
                if (ret < 0)
                        return ret;
        }

        channel_data = (SACTRL_HWUPD_EN | (ctx->sa.assoc_num << 29) |
                        ((u32)(parent_rxsc_idx) << 16) | SACTRL_EXPIRED_IRQ);

        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 20 * 4,
                                         channel_data, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 21 * 4,
                                         0x00000000, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 22 * 4,
                                         0x00000000, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, base_sa_ram + 23 * 4,
                                         0x00000000, INGRESS);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_rxsc_clear_rxsa(struct phy_device *phydev,
                                    struct rtl8261d_macsec_private *macsec, int rxsc_idx)
{
        struct rtl8261d_macsec_rxsc *rxsc = &macsec->rxsc[rxsc_idx];
        struct rtl8261d_macsec_rxsa *rxsa;
        int ret, i, rxsa_idx;

        for (i = 0; i < MACSEC_NUM_AN; i++) {
                rxsa_idx = rxsc->rx_sa_idxs[i];
                if (rxsa_idx == -1 || !macsec->rxsa[rxsa_idx].inuse)
                        continue;

                rxsa = &macsec->rxsa[rxsa_idx];

                memset(&rxsa->stats, 0, sizeof(rxsa->stats));

                rxsa->inuse = 0;
                rxsa->hw_added = 0;
                rxsa->active = false;
                rxsc->rx_sa_idxs[i] = -1;
        }

        ret = rtl8261d_clear_scsa_mapping(phydev, rxsc_idx, INGRESS);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_secy_clear_rxsc(struct phy_device *phydev,
                                    struct rtl8261d_macsec_private *macsec, int secy_idx)
{
        struct rtl8261d_macsec_secy *secy = &macsec->secy[secy_idx];
        struct rtl8261d_macsec_rxsc *rxsc;
        int ret, i, rxsc_idx;

        for (i = 0; i < secy->rx_sc_count; i++) {
                rxsc_idx = secy->rx_sc_idxs[i];
                if (!macsec->rxsc[rxsc_idx].inuse) {
                        pr_debug(KBUILD_MODNAME ": A child rxsc is not inuse in range, impossible\n");
                        return -EINVAL;
                }

                rxsc = &macsec->rxsc[rxsc_idx];

                ret = rtl8261d_enable_rxcam(phydev, rxsc_idx, false);
                if (ret < 0)
                        return ret;

                ret = rtl8261d_rxsc_clear_rxsa(phydev, macsec, rxsc_idx);
                if (ret < 0)
                        return ret;

                memset(&rxsc->stats, 0, sizeof(rxsc->stats));

                rxsc->inuse = 0;
                rxsc->active = false;
                rxsc->kernel_rxsc = NULL;
                secy->rx_sc_idxs[i] = -1;
        }

        secy->rx_sc_count = 0;

        return 0;
}

static int rtl8261d_secy_clear_txsa(struct phy_device *phydev,
                                    struct rtl8261d_macsec_private *macsec, int secy_idx)
{
        struct rtl8261d_macsec_secy *secy = &macsec->secy[secy_idx];
        struct rtl8261d_macsec_txsa *txsa;
        int ret, i, txsa_idx;

        for (i = 0; i < MACSEC_NUM_AN; i++) {
                txsa_idx = secy->tx_sa_idxs[i];
                if (txsa_idx == -1 || !macsec->txsa[txsa_idx].inuse)
                        continue;

                txsa = &macsec->txsa[txsa_idx];

                memset(&txsa->stats, 0, sizeof(txsa->stats));

                txsa->inuse = 0;
                txsa->hw_added = 0;
                txsa->active = false;
                secy->tx_sa_idxs[i] = -1;
        }

        ret = rtl8261d_clear_scsa_mapping(phydev, secy_idx, EGRESS);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_syn_hw(struct phy_device *phydev, enum rtl8261d_comp comp)
{
        int val, ret, i;
        u32 channel_data;

        val = rtl8261d_macsec_comp_read(phydev, SYN_CORE, &channel_data, comp);
        if (val < 0)
                return val;

        channel_data |= SYN_LOAD_SAFE;
        ret = rtl8261d_macsec_comp_write(phydev, SYN_CORE,
                                         channel_data, comp);
        if (ret < 0)
                return ret;

        for (i = 0; i < RTL8261D_CHANNEL_WAIT_COUNT; i++) {

                udelay(RTL8261D_CHANNEL_WAIT_TIME);

                val = rtl8261d_macsec_comp_read(phydev, SYN_CORE, &channel_data, comp);
                if (val < 0)
                        return val;

                if (!((u32)channel_data & SYN_FINISH_MASK))
                        break;
        }
        if (i == RTL8261D_CHANNEL_WAIT_COUNT)
                return -ETIMEDOUT;

        return 0;
}

static int rtl8261d_update_txsa_txsacnt(struct phy_device *phydev, bool encrypt, int txsa_idx,
                                        struct rtl8261d_txsc_stats *txsc_stats,
                                        struct rtl8261d_txsa_stats *txsa_stats)
{
        int ret;
        u64 cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8008 + txsa_idx * 128,
                                     (0x8008 + 4) + txsa_idx * 128, &cnt_data, EGRESS);
        if (ret < 0)
                return ret;

        if (encrypt) {
                txsa_stats->OutPktsEncrypted += cnt_data;
                txsc_stats->OutPktsEncrypted += cnt_data;
        } else {
                txsa_stats->OutPktsProtected += cnt_data;
                txsc_stats->OutPktsProtected += cnt_data;
        }

        return 0;
}

static int rtl8261d_update_txsa_txsccnt(struct phy_device *phydev, bool encrypt,
                                        int txsa_idx, struct rtl8261d_txsc_stats *txsc_stats)
{
        int ret;
        u64 cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8000 + txsa_idx * 128,
                                     (0x8000 + 4) + txsa_idx * 128, &cnt_data, EGRESS);
        if (ret < 0)
                return ret;

        if (encrypt)
                txsc_stats->OutOctetsEncrypted += cnt_data;
        else
                txsc_stats->OutOctetsProtected += cnt_data;

        return 0;
}

static int rtl8261d_update_txsa_secycnt(struct phy_device *phydev, int txsa_idx,
                                        struct rtl8261d_secy_stats *secy_stats)
{
        int ret;
        u64 cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8010 + txsa_idx * 128,
                                     (0x8010 + 4) + txsa_idx * 128, &cnt_data, EGRESS);
        if (ret < 0)
                return ret;

        secy_stats->OutPktsTooLong += cnt_data;

        return 0;
}

static int rtl8261d_final_txsa_cnt(struct phy_device *phydev, bool encrypt, int txsa_idx,
                                   struct rtl8261d_secy_stats *secy_stats,
                                   struct rtl8261d_txsc_stats *txsc_stats,
                                   struct rtl8261d_txsa_stats *txsa_stats)
{
        int ret;

        ret = rtl8261d_update_txsa_txsacnt(phydev, encrypt, txsa_idx, txsc_stats, txsa_stats);
        if (ret < 0)
                return ret;

        ret = rtl8261d_update_txsa_txsccnt(phydev, encrypt, txsa_idx, txsc_stats);
        if (ret < 0)
                return ret;

        ret = rtl8261d_update_txsa_secycnt(phydev, txsa_idx, secy_stats);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_update_rxsa_rxsacnt(struct phy_device *phydev, int rxsa_idx,
                                        struct rtl8261d_rxsc_stats *rxsc_stats,
                                        struct rtl8261d_rxsa_stats *rxsa_stats)
{
        int ret;
        u64 cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8028 + rxsa_idx * 128,
                                     (0x8028 + 4) + rxsa_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        rxsa_stats->InPktsOK += cnt_data;
        rxsc_stats->InPktsOK += cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8030 + rxsa_idx * 128,
                                     (0x8030 + 4) + rxsa_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        rxsa_stats->InPktsInvalid += cnt_data;
        rxsc_stats->InPktsInvalid += cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8038 + rxsa_idx * 128,
                                     (0x8038 + 4) + rxsa_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        rxsa_stats->InPktsNotValid += cnt_data;
        rxsc_stats->InPktsNotValid += cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8040 + rxsa_idx * 128,
                                     (0x8040 + 4) + rxsa_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        rxsa_stats->InPktsNotUsingSA += cnt_data;
        rxsc_stats->InPktsNotUsingSA += cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8048 + rxsa_idx * 128,
                                     (0x8048 + 4) + rxsa_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        rxsa_stats->InPktsUnusedSA += cnt_data;
        rxsc_stats->InPktsUnusedSA += cnt_data;

        return 0;
}

static int rtl8261d_update_rxsa_rxsccnt(struct phy_device *phydev, int rxsa_idx,
                                        struct rtl8261d_rxsc_stats *rxsc_stats)
{
        int ret;
        u64 cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8008 + rxsa_idx * 128,
                                     (0x8008 + 4) + rxsa_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        rxsc_stats->InOctetsValidated += cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8000 + rxsa_idx * 128,
                                     (0x8000 + 4) + rxsa_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        rxsc_stats->InOctetsDecrypted += cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8010 + rxsa_idx * 128,
                                     (0x8010 + 4) + rxsa_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        rxsc_stats->InPktsUnchecked += cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8018 + rxsa_idx * 128,
                                     (0x8018 + 4) + rxsa_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        rxsc_stats->InPktsDelayed += cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0x8020 + rxsa_idx * 128,
                                     (0x8020 + 4) + rxsa_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        rxsc_stats->InPktsLate += cnt_data;

        return 0;
}

static int rtl8261d_final_rxsa_cnt(struct phy_device *phydev, int rxsa_idx,
                                   struct rtl8261d_rxsc_stats *rxsc_stats,
                                   struct rtl8261d_rxsa_stats *rxsa_stats)
{
        int ret;

        ret = rtl8261d_update_rxsa_rxsacnt(phydev, rxsa_idx, rxsc_stats, rxsa_stats);
        if (ret < 0)
                return ret;

        ret = rtl8261d_update_rxsa_rxsccnt(phydev, rxsa_idx, rxsc_stats);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_update_txsc_cnt(struct phy_device *phydev, bool encrypt, int secy_idx,
                                    struct rtl8261d_macsec_private *macsec)
{
        struct rtl8261d_txsc_stats *txsc_stats;
        struct rtl8261d_txsa_stats *txsa_stats;
        int i, ret, txsa_idx;

        txsc_stats = &macsec->secy[secy_idx].stats;

        for (i = 0; i < MACSEC_NUM_AN; i++) {
                txsa_idx = macsec->secy[secy_idx].tx_sa_idxs[i];

                if (txsa_idx == -1 || !macsec->txsa[txsa_idx].inuse)
                        continue;

                txsa_stats = &macsec->txsa[txsa_idx].stats;

                ret = rtl8261d_update_txsa_txsacnt(phydev, encrypt, txsa_idx,
                                                   txsc_stats, txsa_stats);
                if (ret < 0)
                        return ret;

                ret = rtl8261d_update_txsa_txsccnt(phydev, encrypt, txsa_idx, txsc_stats);
                if (ret < 0)
                        return ret;
        }

        return 0;
}

static int rtl8261d_update_rxsc_cnt(struct phy_device *phydev, int rxsc_idx,
                                    struct rtl8261d_macsec_private *macsec)
{
        struct rtl8261d_rxsc_stats *rxsc_stats;
        struct rtl8261d_rxsa_stats *rxsa_stats;
        int i, ret, rxsa_idx;

        rxsc_stats = &macsec->rxsc[rxsc_idx].stats;

        for (i = 0; i < MACSEC_NUM_AN; i++) {
                rxsa_idx = macsec->rxsc[rxsc_idx].rx_sa_idxs[i];

                if (rxsa_idx == -1 || !macsec->rxsa[rxsa_idx].inuse)
                        continue;

                rxsa_stats = &macsec->rxsa[rxsa_idx].stats;

                ret = rtl8261d_final_rxsa_cnt(phydev, rxsa_idx, rxsc_stats, rxsa_stats);
                if (ret < 0)
                        return ret;
        }

        return 0;
}

static int rtl8261d_update_secy_cnt(struct phy_device *phydev, int secy_idx,
                                    struct rtl8261d_macsec_private *macsec)
{
        struct rtl8261d_secy_stats *secy_stats;
        int i, ret, child_txsa_idx;
        u64 cnt_data;

        secy_stats = &macsec->secy[secy_idx].secy_stats;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0xA010 + secy_idx * 128,
                                     (0xA010 + 4) + secy_idx * 128, &cnt_data, EGRESS);
        if (ret < 0)
                return ret;

        secy_stats->OutPktsUntagged += cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0xA010 + secy_idx * 128,
                                     (0xA010 + 4) + secy_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        secy_stats->InPktsUntagged += cnt_data;

        for (i = 0; i < MACSEC_NUM_AN; i++) {
                child_txsa_idx = macsec->secy[secy_idx].tx_sa_idxs[i];

                if (child_txsa_idx != -1 && macsec->txsa[child_txsa_idx].inuse) {
                        ret = rtl8261d_update_txsa_secycnt(phydev, child_txsa_idx, secy_stats);
                        if (ret < 0)
                                return ret;
                }
        }

        ret = rtl8261d_rd_2x32_to_64(phydev, 0xA018 + secy_idx * 128,
                                     (0xA018 + 4) + secy_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        secy_stats->InPktsNoTag += cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0xA020 + secy_idx * 128,
                                     (0xA020 + 4) + secy_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        secy_stats->InPktsBadTag += cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0xA028 + secy_idx * 128,
                                     (0xA028 + 4) + secy_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        secy_stats->InPktsNoSCI += cnt_data;

        ret = rtl8261d_rd_2x32_to_64(phydev, 0xA030 + secy_idx * 128,
                                     (0xA030 + 4) + secy_idx * 128, &cnt_data, INGRESS);
        if (ret < 0)
                return ret;

        secy_stats->InPktsUnknownSCI += cnt_data;

        return 0;
}

static int rtl8261d_add_secy(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_secy *ctx_secy = ctx->secy;
        struct rtl8261d_macsec_secy *secy;
        struct rtl8261d_macsec_tcam *tcam_tx, *tcam_rx;
        int i, ret = 0, secy_idx = -1, tcam_tx_idx = -1, tcam_rx_idx = -1, sectag_len, ifg;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                if (!macsec->secy[i].inuse) {
                        secy_idx = i;
                        secy = &macsec->secy[secy_idx];
                        break;
                }
        }
        if (secy_idx == -1) {
                pr_debug(KBUILD_MODNAME ": Out of secy hw space\n");
                ret = -ENOSPC;
                goto out;
        }

        for (i = 0; i < RTL8261D_HW_MAX_TCAM; i++) {
                if (!macsec->tcam[i].inuse) {
                        if (tcam_tx_idx == -1) {
                                tcam_tx_idx = i;
                                tcam_tx = &macsec->tcam[tcam_tx_idx];
                        } else {
                                tcam_rx_idx = i;
                                tcam_rx = &macsec->tcam[tcam_rx_idx];
                                break;
                        }
                }
        }
        if (tcam_tx_idx == -1 || tcam_rx_idx == -1) {
                pr_debug(KBUILD_MODNAME ": Out of tcam hw space\n");
                ret = -ENOSPC;
                goto out;
        }

        sectag_len = ctx_secy->tx_sc.send_sci ? MACSEC_SECTAG_BLEN : MACSEC_SECTAG_BLEN_NOSCI;
        ifg = ctx_secy->icv_len + sectag_len;
        ret = rtl8261d_set_ifg(phydev, ifg);
        if (ret < 0)
                goto out;

        ret = rtl8261d_add_flowctrl1_entry(phydev, secy_idx, ctx_secy);
        if (ret < 0)
                goto out;

        ret = rtl8261d_add_flowctrl2_entry(phydev, secy_idx);
        if (ret < 0)
                goto out;

        ret = rtl8261d_macsec_comp_write(phydev, 0xF200 + secy_idx * 4,
                                         0x00007FFF, EGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_add_tcam_entry(phydev, tcam_tx_idx, secy_idx, EGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_enable_tcam(phydev, tcam_tx_idx, true, EGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_add_tcam_entry(phydev, tcam_rx_idx, secy_idx, INGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_enable_tcam(phydev, tcam_rx_idx, true, INGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_syn_hw(phydev, EGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_syn_hw(phydev, INGRESS);
        if (ret < 0)
                goto out;

        secy->inuse = true;
        secy->encrypt = ctx_secy->tx_sc.encrypt;
        secy->encoding_sa = ctx_secy->tx_sc.encoding_sa;
        secy->replay_window = ctx_secy->replay_window;
        secy->kernel_secy = ctx_secy;
        secy->ifg = ifg;
        tcam_tx->inuse = true;
        tcam_tx->secy_idx = secy_idx;
        tcam_rx->inuse = true;
        tcam_rx->secy_idx = secy_idx;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_upd_secy(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_secy *ctx_secy = ctx->secy;
        struct rtl8261d_macsec_secy *secy;
        struct rtl8261d_macsec_rxsc *rxsc;
        struct rtl8261d_macsec_txsa *txsa_old = NULL, *txsa_new = NULL;
        int i, j, ret, secy_idx = -1, rxsc_idx, rxsa_idx, sectag_len, ifg;
        int old_encodingsa_idx, new_encodingsa_idx;
        u16 addr;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                if (macsec->secy[i].inuse && macsec->secy[i].kernel_secy == ctx_secy) {
                        secy_idx = i;
                        secy = &macsec->secy[secy_idx];
                        break;
                }
        }
        if (secy_idx == -1) {
                pr_err(KBUILD_MODNAME ": Specified secy doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        if (ctx_secy->tx_sc.encrypt != secy->encrypt) {
                ret = rtl8261d_syn_hw(phydev, EGRESS);
                if (ret < 0)
                        goto out;

                ret = rtl8261d_update_txsc_cnt(phydev, secy->encrypt, secy_idx, macsec);
                if (ret < 0)
                        goto out;
        }

        sectag_len = ctx_secy->tx_sc.send_sci ? MACSEC_SECTAG_BLEN : MACSEC_SECTAG_BLEN_NOSCI;
        ifg = ctx_secy->icv_len + sectag_len;
        ret = rtl8261d_set_ifg(phydev, ifg);
        if (ret < 0)
                goto out;

        ret = rtl8261d_add_flowctrl1_entry(phydev, secy_idx, ctx_secy);
        if (ret < 0)
                goto out;

        if (secy->replay_window != ctx_secy->replay_window) {
                for (i = 0; i < secy->rx_sc_count; i++) {
                        rxsc_idx = secy->rx_sc_idxs[i];
                        rxsc = &macsec->rxsc[rxsc_idx];
                        if (!rxsc->inuse) {
                                pr_debug(KBUILD_MODNAME ": A child rxsc is not inuse in range, impossible\n");
                                ret = -EINVAL;
                                goto out;
                        }

                        for (j = 0; j < MACSEC_NUM_AN; j++) {
                                rxsa_idx = rxsc->rx_sa_idxs[j];
                                if (rxsa_idx == -1 || !macsec->rxsa[rxsa_idx].inuse)
                                        continue;

                                addr = RTL8261D_SA_RAM_ENTRY_BSIZE * rxsa_idx;
                                ret = rtl8261d_set_replay_mask(phydev, addr,
                                                               ctx_secy->replay_window);
                                if (ret < 0)
                                        goto out;
                        }
                }
        }

        if (secy->encoding_sa != ctx_secy->tx_sc.encoding_sa) {
                old_encodingsa_idx = secy->tx_sa_idxs[secy->encoding_sa];
                new_encodingsa_idx = secy->tx_sa_idxs[ctx_secy->tx_sc.encoding_sa];
                if (old_encodingsa_idx != -1)
                        txsa_old = &macsec->txsa[old_encodingsa_idx];
                if (new_encodingsa_idx != -1)
                        txsa_new = &macsec->txsa[new_encodingsa_idx];
                addr = SAM_SC_SA_MAP1_base + secy_idx * 8;

                if (new_encodingsa_idx == -1 || !txsa_new->inuse) {
                        if (old_encodingsa_idx != -1) {
                                ret = rtl8261d_del_scsa_mapping_entry(phydev, addr, 0,
                                                                      old_encodingsa_idx, EGRESS);
                                if (ret < 0)
                                        goto out;
                        }
                } else if (txsa_new->active) {
                        if (old_encodingsa_idx != -1 && txsa_old->inuse && txsa_old->active) {
                                ret = rtl8261d_seamless_add_scsa_mapping_entry(phydev, addr, 0,
                                        &txsa_new->hw_added,
                                        new_encodingsa_idx,
                                        EGRESS);
                                if (ret < 0)
                                        goto out;
                        } else {
                                ret = rtl8261d_add_scsa_mapping_entry(phydev, addr, 0,
                                                                      &txsa_new->hw_added,
                                                                      new_encodingsa_idx, true,
                                                                      EGRESS);
                                if (ret < 0)
                                        goto out;
                        }
                } else {
                        ret = rtl8261d_add_scsa_mapping_entry(phydev, addr, 0, &txsa_new->hw_added,
                                                              new_encodingsa_idx, false, EGRESS);
                        if (ret < 0)
                                goto out;
                }
        }

        ret = rtl8261d_syn_hw(phydev, EGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_syn_hw(phydev, INGRESS);
        if (ret < 0)
                goto out;

        secy->encrypt = ctx_secy->tx_sc.encrypt;
        secy->encoding_sa = ctx_secy->tx_sc.encoding_sa;
        secy->replay_window = ctx_secy->replay_window;
        secy->ifg = ifg;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_del_secy(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_secy *ctx_secy = ctx->secy;
        struct rtl8261d_macsec_secy *secy;
        int i, ret, secy_idx = -1;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                if (macsec->secy[i].inuse && (macsec->secy[i].kernel_secy == ctx_secy)) {
                        secy_idx = i;
                        secy = &macsec->secy[secy_idx];
                        break;
                }
        }
        if (secy_idx == -1) {
                pr_err(KBUILD_MODNAME ": Parent secy doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        ret = rtl8261d_del_tcam_entry(phydev, macsec->tcam, secy_idx);
        if (ret < 0)
                goto out;

        secy->ifg = 0;
        ret = rtl8261d_upd_ifg(phydev);
        if (ret < 0)
                goto out;

        ret = rtl8261d_syn_hw(phydev, EGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_syn_hw(phydev, INGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_secy_clear_rxsc(phydev, macsec, secy_idx);
        if (ret < 0)
                goto out;

        ret = rtl8261d_secy_clear_txsa(phydev, macsec, secy_idx);
        if (ret < 0)
                goto out;

        ret = rtl8261d_clear_flowctl_entry(phydev, secy_idx);
        if (ret < 0)
                goto out;

        memset(&secy->stats, 0, sizeof(secy->stats));
        memset(&secy->secy_stats, 0, sizeof(secy->secy_stats));

        secy->inuse = false;
        secy->encrypt = false;
        secy->encoding_sa = 0;
        secy->replay_window = 0x80;
        secy->kernel_secy = NULL;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_add_rxsc(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_secy *ctx_secy = ctx->secy;
        const struct macsec_rx_sc *ctx_rx_sc = ctx->rx_sc;
        struct rtl8261d_macsec_secy *secy;
        struct rtl8261d_macsec_rxsc *rxsc;
        int ret = 0, i, rxsc_idx = -1, secy_idx = -1;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                if (macsec->secy[i].inuse && macsec->secy[i].kernel_secy == ctx_secy) {
                        secy_idx = i;
                        secy = &macsec->secy[secy_idx];
                        break;
                }
        }
        if (secy_idx == -1) {
                pr_err(KBUILD_MODNAME ": Parent secy doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        for (i = 0; i < RTL8261D_HW_MAX_RXSC; i++) {
                if (!macsec->rxsc[i].inuse) {
                        rxsc_idx = i;
                        rxsc = &macsec->rxsc[rxsc_idx];
                        break;
                }
        }
        if (rxsc_idx == -1) {
                pr_debug(KBUILD_MODNAME ": Out of rxsc hw space\n");
                ret = -ENOSPC;
                goto out;
        }

        ret = rtl8261d_add_rxcam_entry(phydev, rxsc_idx, secy_idx, ctx_rx_sc->sci);
        if (ret < 0)
                goto out;

        ret = rtl8261d_enable_rxcam(phydev, rxsc_idx, ctx_rx_sc->active);
        if (ret < 0)
                goto out;

        rxsc->inuse = 1;
        rxsc->active = ctx_rx_sc->active;
        rxsc->kernel_rxsc = ctx_rx_sc;

        secy->rx_sc_idxs[secy->rx_sc_count] = rxsc_idx;
        secy->rx_sc_count++;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_upd_rxsc(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_rx_sc *ctx_rx_sc = ctx->rx_sc;
        struct rtl8261d_macsec_rxsc *rxsc;
        int ret = 0, i, rxsc_idx = -1;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_RXSC; i++) {
                if (macsec->rxsc[i].inuse && macsec->rxsc[i].kernel_rxsc == ctx_rx_sc) {
                        rxsc_idx = i;
                        rxsc = &macsec->rxsc[rxsc_idx];
                        break;
                }
        }
        if (rxsc_idx == -1) {
                pr_err(KBUILD_MODNAME ": Specified rxsc doen't exist\n");
                ret = -EINVAL;
                goto out;
        }

        if (ctx_rx_sc->active == rxsc->active) {
                ret = 0;
                goto out;
        }

        ret = rtl8261d_enable_rxcam(phydev, rxsc_idx, ctx_rx_sc->active);
        if (ret < 0)
                goto out;

        if (!ctx_rx_sc->active) {
                ret = rtl8261d_syn_hw(phydev, INGRESS);
                if (ret < 0)
                        goto out;
        }

        rxsc->active = ctx_rx_sc->active;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_del_rxsc(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_secy *ctx_secy = ctx->secy;
        const struct macsec_rx_sc *ctx_rx_sc = ctx->rx_sc;
        struct rtl8261d_macsec_secy *secy;
        struct rtl8261d_macsec_rxsc *rxsc;
        int ret = 0, i, rxsc_idx = -1, secy_idx = -1, secy_rxsc_idx;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                if (macsec->secy[i].inuse && macsec->secy[i].kernel_secy == ctx_secy) {
                        secy_idx = i;
                        secy = &macsec->secy[secy_idx];
                        break;
                }
        }
        if (secy_idx == -1) {
                pr_err(KBUILD_MODNAME ": Parent secy doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        for (secy_rxsc_idx = 0; secy_rxsc_idx < secy->rx_sc_count; secy_rxsc_idx++) {
                i = secy->rx_sc_idxs[secy_rxsc_idx];
                if (macsec->rxsc[i].inuse && macsec->rxsc[i].kernel_rxsc == ctx_rx_sc) {
                        rxsc_idx = i;
                        rxsc = &macsec->rxsc[rxsc_idx];
                        break;
                }
        }
        if (rxsc_idx == -1) {
                pr_err(KBUILD_MODNAME ": Specified rxsc doen't exist\n");
                ret = -EINVAL;
                goto out;
        }

        ret = rtl8261d_enable_rxcam(phydev, rxsc_idx, false);
        if (ret < 0)
                goto out;

        ret = rtl8261d_syn_hw(phydev, INGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_rxsc_clear_rxsa(phydev, macsec, rxsc_idx);
        if (ret < 0)
                goto out;

        memset(&rxsc->stats, 0, sizeof(rxsc->stats));

        rxsc->inuse = 0;
        rxsc->active = false;
        rxsc->kernel_rxsc = NULL;
        secy->rx_sc_count--;
        secy->rx_sc_idxs[secy_rxsc_idx] = secy->rx_sc_idxs[secy->rx_sc_count];
        secy->rx_sc_idxs[secy->rx_sc_count] = -1;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_add_txsa(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_secy *ctx_secy = ctx->secy;
        const struct macsec_tx_sa *ctx_tx_sa = ctx->sa.tx_sa;
        struct rtl8261d_macsec_secy *secy;
        struct rtl8261d_macsec_txsa *txsa;
        int i, ret = 0, txsa_idx = -1, secy_idx = -1;
        u32 channel_data;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                if (macsec->secy[i].inuse && macsec->secy[i].kernel_secy == ctx_secy) {
                        secy_idx = i;
                        secy = &macsec->secy[secy_idx];
                        break;
                }
        }
        if (secy_idx == -1) {
                pr_err(KBUILD_MODNAME ": Parent secy doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        for (i = 0; i < RTL8261D_HW_MAX_TXSA; i++) {
                if (!macsec->txsa[i].inuse) {
                        txsa_idx = i;
                        txsa = &macsec->txsa[txsa_idx];
                        break;
                }
        }
        if (txsa_idx == -1) {
                pr_debug(KBUILD_MODNAME ": Out of txsa hw space\n");
                ret = -ENOSPC;
                goto out;
        }

        ret = rtl8261d_add_tx_sa_ram(phydev, txsa_idx, secy_idx, ctx);
        if (ret < 0)
                goto out;

        for (i = 0; i < RTL8261D_TXCNT_NUM; i++) {
                ret = rtl8261d_macsec_comp_read(phydev, (0x8000 + i * 4) + txsa_idx * 128,
                                                &channel_data, EGRESS);
                if (ret < 0)
                        goto out;
        }

        if (ctx->sa.assoc_num == ctx_secy->tx_sc.encoding_sa) {
                ret = rtl8261d_add_scsa_mapping_entry(phydev,
                                                      SAM_SC_SA_MAP1_base + secy_idx * 8,
                                                      0, &txsa->hw_added, txsa_idx,
                                                      ctx_tx_sa->active, EGRESS);
                if (ret < 0)
                        goto out;
        }

        ret = rtl8261d_syn_hw(phydev, EGRESS);
        if (ret < 0)
                goto out;

        txsa->inuse = true;
        txsa->active = ctx_tx_sa->active;
        secy->tx_sa_idxs[ctx->sa.assoc_num] = txsa_idx;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_upd_txsa(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_secy *ctx_secy = ctx->secy;
        const struct macsec_tx_sa *ctx_tx_sa = ctx->sa.tx_sa;
        struct rtl8261d_macsec_secy *secy;
        struct rtl8261d_macsec_txsa *txsa;
        int i, ret = 0, txsa_idx, secy_idx = -1;
        u16 addr;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                if (macsec->secy[i].inuse && macsec->secy[i].kernel_secy == ctx_secy) {
                        secy_idx = i;
                        secy = &macsec->secy[secy_idx];
                        break;
                }
        }
        if (secy_idx == -1) {
                pr_err(KBUILD_MODNAME ": Parent secy doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        txsa_idx = secy->tx_sa_idxs[ctx->sa.assoc_num];
        if (txsa_idx == -1 || !macsec->txsa[txsa_idx].inuse) {
                pr_err(KBUILD_MODNAME ": Specified txsa doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }
        txsa = &macsec->txsa[txsa_idx];

        if (!ctx->sa.update_pn && txsa->active == ctx_tx_sa->active) {
                ret = 0;
                goto out;
        }

        addr = SAM_SC_SA_MAP1_base + secy_idx * 8;

        if (ctx->sa.assoc_num == ctx_secy->tx_sc.encoding_sa) {
                if (txsa->active) {
                        ret = rtl8261d_udp_scsa_mapping_entry(phydev, addr, 0,
                                                              txsa_idx, false, EGRESS);
                        if (ret < 0)
                                goto out;

                        ret = rtl8261d_syn_hw(phydev, EGRESS);
                        if (ret < 0)
                                goto out;
                }
        }

        if (ctx->sa.update_pn) {
                ret = rtl8261d_set_pn(phydev, RTL8261D_SA_RAM_ENTRY_BSIZE * txsa_idx,
                                      ctx_tx_sa->next_pn, EGRESS);
                if (ret < 0)
                        goto out;
        }

        if (ctx->sa.assoc_num == ctx_secy->tx_sc.encoding_sa) {
                if (ctx_tx_sa->active) {
                        ret = rtl8261d_udp_scsa_mapping_entry(phydev, addr, 0,
                                                              txsa_idx, true, EGRESS);
                        if (ret < 0)
                                goto out;
                }
        }

        txsa->active = ctx_tx_sa->active;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_del_txsa(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_secy *ctx_secy = ctx->secy;
        struct rtl8261d_macsec_secy *secy;
        struct rtl8261d_macsec_txsa *txsa;
        int i, ret = 0, txsa_idx, secy_idx = -1;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                if (macsec->secy[i].inuse && macsec->secy[i].kernel_secy == ctx_secy) {
                        secy_idx = i;
                        secy = &macsec->secy[secy_idx];
                        break;
                }
        }
        if (secy_idx == -1) {
                pr_err(KBUILD_MODNAME ": Parent secy doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        txsa_idx = secy->tx_sa_idxs[ctx->sa.assoc_num];
        if (txsa_idx == -1 || !macsec->txsa[txsa_idx].inuse) {
                pr_err(KBUILD_MODNAME ": Specified txsa doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }
        txsa = &macsec->txsa[txsa_idx];

        if (ctx->sa.assoc_num == ctx_secy->tx_sc.encoding_sa) {
                ret = rtl8261d_del_scsa_mapping_entry(phydev, SAM_SC_SA_MAP1_base + secy_idx * 8,
                                                      0, txsa_idx, EGRESS);
                if (ret < 0)
                        goto out;
        }

        ret = rtl8261d_syn_hw(phydev, EGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_final_txsa_cnt(phydev, ctx_secy->tx_sc.encrypt, txsa_idx, &secy->secy_stats,
                                      &secy->stats, &txsa->stats);
        if (ret < 0)
                goto out;

        memset(&txsa->stats, 0, sizeof(txsa->stats));

        txsa->inuse = false;
        txsa->hw_added = false;
        txsa->active = false;
        secy->tx_sa_idxs[ctx->sa.assoc_num] = -1;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_add_rxsa(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_rx_sc *ctx_rx_sc = ctx->sa.rx_sa->sc;
        const struct macsec_rx_sa *ctx_rx_sa = ctx->sa.rx_sa;
        struct rtl8261d_macsec_rxsc *rxsc;
        struct rtl8261d_macsec_rxsa *rxsa;
        int i, ret = 0, rxsa_idx = -1, rxsc_idx = -1;
        u16 sc_sa_addr;
        u32 channel_data, sc_sa_shift;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_RXSC; i++) {
                if (macsec->rxsc[i].inuse && macsec->rxsc[i].kernel_rxsc == ctx_rx_sc) {
                        rxsc_idx = i;
                        rxsc = &macsec->rxsc[rxsc_idx];
                        break;
                }
        }
        if (rxsc_idx == -1) {
                pr_err(KBUILD_MODNAME ": Parent rxsc doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        for (i = 0; i < RTL8261D_HW_MAX_RXSA; i++) {
                if (!macsec->rxsa[i].inuse) {
                        rxsa_idx = i;
                        rxsa = &macsec->rxsa[rxsa_idx];
                        break;
                }
        }
        if (rxsa_idx == -1) {
                pr_debug(KBUILD_MODNAME ": Out of rxsa hw space\n");
                ret = -ENOSPC;
                goto out;
        }

        ret = rtl8261d_add_rx_sa_ram(phydev, rxsa_idx, rxsc_idx, ctx);
        if (ret < 0)
                goto out;

        for (i = 0; i < RTL8261D_RXCNT_NUM; i++) {
                ret = rtl8261d_macsec_comp_read(phydev, (0x8000 + i * 4) + rxsa_idx * 128,
                                                &channel_data, INGRESS);
                if (ret < 0)
                        goto out;
        }

        sc_sa_addr = ((ctx->sa.assoc_num < 2) ? SAM_SC_SA_MAP1_base : SAM_SC_SA_MAP2_base) +
                     rxsc_idx * 8;
        sc_sa_shift = (ctx->sa.assoc_num % 2 * 16);

        ret = rtl8261d_add_scsa_mapping_entry(phydev, sc_sa_addr, sc_sa_shift, &rxsa->hw_added,
                                              rxsa_idx, ctx_rx_sa->active, INGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_syn_hw(phydev, INGRESS);
        if (ret < 0)
                goto out;

        rxsa->inuse = true;
        rxsa->active = ctx_rx_sa->active;
        rxsc->rx_sa_idxs[ctx->sa.assoc_num] = rxsa_idx;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_upd_rxsa(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_rx_sc *ctx_rx_sc = ctx->sa.rx_sa->sc;
        const struct macsec_rx_sa *ctx_rx_sa = ctx->sa.rx_sa;
        struct rtl8261d_macsec_rxsc *rxsc;
        struct rtl8261d_macsec_rxsa *rxsa;
        int i, ret = 0, rxsa_idx, rxsc_idx = -1;
        u16 sc_sa_addr;
        u32 sc_sa_shift;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_RXSC; i++) {
                if (macsec->rxsc[i].inuse && macsec->rxsc[i].kernel_rxsc == ctx_rx_sc) {
                        rxsc_idx = i;
                        rxsc = &macsec->rxsc[rxsc_idx];
                        break;
                }
        }
        if (rxsc_idx == -1) {
                pr_err(KBUILD_MODNAME ": Parent rxsc doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        rxsa_idx = rxsc->rx_sa_idxs[ctx->sa.assoc_num];
        if (rxsa_idx == -1 || !macsec->rxsa[rxsa_idx].inuse) {
                pr_err(KBUILD_MODNAME ": Specified rxsa doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }
        rxsa = &macsec->rxsa[rxsa_idx];

        if (!ctx->sa.update_pn && rxsa->active == ctx_rx_sa->active) {
                ret = 0;
                goto out;
        }

        sc_sa_addr = ((ctx->sa.assoc_num < 2) ? SAM_SC_SA_MAP1_base : SAM_SC_SA_MAP2_base) +
                     rxsc_idx * 8;
        sc_sa_shift = (ctx->sa.assoc_num % 2 * 16);

        if (rxsa->active) {
                ret = rtl8261d_udp_scsa_mapping_entry(phydev, sc_sa_addr, sc_sa_shift,
                                                      rxsa_idx, false, INGRESS);
                if (ret < 0)
                        goto out;

                ret = rtl8261d_syn_hw(phydev, INGRESS);
                if (ret < 0)
                        goto out;
        }

        if (ctx->sa.update_pn) {
                ret = rtl8261d_set_pn(phydev, RTL8261D_SA_RAM_ENTRY_BSIZE * rxsa_idx,
                                      ctx_rx_sa->next_pn, INGRESS);
                if (ret < 0)
                        goto out;

        }

        if (ctx_rx_sa->active) {
                ret = rtl8261d_udp_scsa_mapping_entry(phydev, sc_sa_addr, sc_sa_shift,
                                                      rxsa_idx, true, INGRESS);
                if (ret < 0)
                        goto out;
        }

        rxsa->active = ctx_rx_sa->active;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_del_rxsa(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_rx_sa *ctx_rx_sa = ctx->sa.rx_sa;
        const struct macsec_rx_sc *ctx_rx_sc = ctx_rx_sa->sc;
        struct rtl8261d_macsec_rxsc *rxsc;
        struct rtl8261d_macsec_rxsa *rxsa;
        int i, ret = 0, rxsa_idx, rxsc_idx = -1;
        u16 sc_sa_addr;
        u32 sc_sa_shift;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_RXSC; i++) {
                if (macsec->rxsc[i].inuse && macsec->rxsc[i].kernel_rxsc == ctx_rx_sc) {
                        rxsc_idx = i;
                        rxsc = &macsec->rxsc[rxsc_idx];
                        break;
                }
        }
        if (rxsc_idx == -1) {
                ret = -EINVAL;
                pr_err(KBUILD_MODNAME ": Parent rxsc doesn't exist\n");
                goto out;
        }

        rxsa_idx = rxsc->rx_sa_idxs[ctx->sa.assoc_num];
        if (rxsa_idx == -1 || !macsec->rxsa[rxsa_idx].inuse) {
                pr_err(KBUILD_MODNAME ": Specified rxsa doen't exist\n");
                ret = -EINVAL;
                goto out;
        }
        rxsa = &macsec->rxsa[rxsa_idx];

        sc_sa_addr = ((ctx->sa.assoc_num < 2) ? SAM_SC_SA_MAP1_base : SAM_SC_SA_MAP2_base) +
                     rxsc_idx * 8;
        sc_sa_shift = (ctx->sa.assoc_num % 2 * 16);
        ret = rtl8261d_del_scsa_mapping_entry(phydev, sc_sa_addr, sc_sa_shift, rxsa_idx, INGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_syn_hw(phydev, INGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_final_rxsa_cnt(phydev, rxsa_idx, &rxsc->stats, &rxsa->stats);
        if (ret < 0)
                goto out;

        memset(&rxsa->stats, 0, sizeof(rxsa->stats));

        rxsa->inuse = false;
        rxsa->hw_added = false;
        rxsa->active = false;
        rxsc->rx_sa_idxs[ctx->sa.assoc_num] = -1;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_get_tx_sa_stats(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_secy *ctx_secy = ctx->secy;
        struct macsec_tx_sa *ctx_tx_sa = ctx->sa.tx_sa;
        struct rtl8261d_macsec_secy *secy;
        struct rtl8261d_macsec_txsa *txsa;
        int i, ret = 0, txsa_idx, secy_idx = -1;
        u16 base_sa_ram;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                if (macsec->secy[i].inuse && macsec->secy[i].kernel_secy == ctx_secy) {
                        secy_idx = i;
                        secy = &macsec->secy[secy_idx];
                        break;
                }
        }
        if (secy_idx == -1) {
                pr_err(KBUILD_MODNAME ": Parent secy doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        txsa_idx = secy->tx_sa_idxs[ctx->sa.assoc_num];
        if (txsa_idx == -1 || !macsec->txsa[txsa_idx].inuse) {
                pr_err(KBUILD_MODNAME ": Specified txsa doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }
        txsa = &macsec->txsa[txsa_idx];

        ret = rtl8261d_syn_hw(phydev, EGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_update_txsa_txsacnt(phydev, ctx_secy->tx_sc.encrypt, txsa_idx,
                                           &secy->stats, &txsa->stats);
        if (ret < 0)
                goto out;

        ctx->stats.tx_sa_stats->OutPktsEncrypted = (u32)txsa->stats.OutPktsEncrypted;
        ctx->stats.tx_sa_stats->OutPktsProtected = (u32)txsa->stats.OutPktsProtected;

        base_sa_ram = RTL8261D_SA_RAM_ENTRY_BSIZE * txsa_idx;

        ret = rtl8261d_set_txsa_kernel_pn(phydev, base_sa_ram, ctx_tx_sa);
        if (ret < 0)
                goto out;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_get_rx_sa_stats(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_rx_sc *ctx_rx_sc = ctx->sa.rx_sa->sc;
        struct macsec_rx_sa *ctx_rx_sa = ctx->sa.rx_sa;
        struct rtl8261d_macsec_rxsc *rxsc;
        struct rtl8261d_macsec_rxsa *rxsa;
        int i, ret = 0, rxsa_idx, rxsc_idx = -1;
        u16 base_sa_ram;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_RXSC; i++) {
                if (macsec->rxsc[i].inuse && macsec->rxsc[i].kernel_rxsc == ctx_rx_sc) {
                        rxsc_idx = i;
                        rxsc = &macsec->rxsc[rxsc_idx];
                        break;
                }
        }
        if (rxsc_idx == -1) {
                pr_err(KBUILD_MODNAME ": Parent rxsc doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        rxsa_idx = rxsc->rx_sa_idxs[ctx->sa.assoc_num];
        if (rxsa_idx == -1 || !macsec->rxsa[rxsa_idx].inuse) {
                pr_err(KBUILD_MODNAME ": Specified rxsa doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }
        rxsa = &macsec->rxsa[rxsa_idx];

        ret = rtl8261d_syn_hw(phydev, INGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_update_rxsa_rxsacnt(phydev, rxsa_idx, &rxsc->stats,
                                           &rxsa->stats);
        if (ret < 0)
                goto out;

        ctx->stats.rx_sa_stats->InPktsOK = (u32)rxsa->stats.InPktsOK;
        ctx->stats.rx_sa_stats->InPktsInvalid = (u32)rxsa->stats.InPktsInvalid;
        ctx->stats.rx_sa_stats->InPktsNotValid = (u32)rxsa->stats.InPktsNotValid;
        ctx->stats.rx_sa_stats->InPktsNotUsingSA = (u32)rxsa->stats.InPktsNotUsingSA;
        ctx->stats.rx_sa_stats->InPktsUnusedSA = (u32)rxsa->stats.InPktsUnusedSA;

        base_sa_ram = RTL8261D_SA_RAM_ENTRY_BSIZE * rxsa_idx;

        ret = rtl8261d_set_rxsa_kernel_pn(phydev, base_sa_ram, ctx_rx_sa);
        if (ret < 0)
                goto out;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_get_tx_sc_stats(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_secy *ctx_secy = ctx->secy;
        struct rtl8261d_macsec_secy *secy;
        int i, ret = 0, secy_idx = -1;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                if (macsec->secy[i].inuse && macsec->secy[i].kernel_secy == ctx_secy) {
                        secy_idx = i;
                        secy = &macsec->secy[secy_idx];
                        break;
                }
        }
        if (secy_idx == -1) {
                pr_err(KBUILD_MODNAME ": Parent secy doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        ret = rtl8261d_syn_hw(phydev, EGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_update_txsc_cnt(phydev, ctx_secy->tx_sc.encrypt, secy_idx, macsec);
        if (ret < 0)
                goto out;

        ctx->stats.tx_sc_stats->OutOctetsEncrypted = secy->stats.OutOctetsEncrypted;
        ctx->stats.tx_sc_stats->OutOctetsProtected = secy->stats.OutOctetsProtected;
        ctx->stats.tx_sc_stats->OutPktsEncrypted = secy->stats.OutPktsEncrypted;
        ctx->stats.tx_sc_stats->OutPktsProtected = secy->stats.OutPktsProtected;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_get_rx_sc_stats(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_rx_sc *ctx_rx_sc = ctx->rx_sc;
        struct rtl8261d_macsec_rxsc *rxsc;
        int ret = 0, i, rxsc_idx = -1;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_RXSC; i++) {
                if (macsec->rxsc[i].inuse && macsec->rxsc[i].kernel_rxsc == ctx_rx_sc) {
                        rxsc_idx = i;
                        rxsc = &macsec->rxsc[rxsc_idx];
                        break;
                }
        }
        if (rxsc_idx == -1) {
                pr_err(KBUILD_MODNAME ": Specified rxsc doen't exist\n");
                ret = -EINVAL;
                goto out;
        }

        ret = rtl8261d_syn_hw(phydev, INGRESS);
        if (ret < 0)
                goto out;

        ret = rtl8261d_update_rxsc_cnt(phydev, rxsc_idx, macsec);
        if (ret < 0)
                goto out;

        ctx->stats.rx_sc_stats->InOctetsValidated = rxsc->stats.InOctetsValidated;
        ctx->stats.rx_sc_stats->InOctetsDecrypted = rxsc->stats.InOctetsDecrypted;
        ctx->stats.rx_sc_stats->InPktsUnchecked = rxsc->stats.InPktsUnchecked;
        ctx->stats.rx_sc_stats->InPktsDelayed = rxsc->stats.InPktsDelayed;
        ctx->stats.rx_sc_stats->InPktsLate = rxsc->stats.InPktsLate;
        ctx->stats.rx_sc_stats->InPktsOK = rxsc->stats.InPktsOK;
        ctx->stats.rx_sc_stats->InPktsInvalid = rxsc->stats.InPktsInvalid;
        ctx->stats.rx_sc_stats->InPktsNotValid = rxsc->stats.InPktsNotValid;
        ctx->stats.rx_sc_stats->InPktsNotUsingSA = rxsc->stats.InPktsNotUsingSA;
        ctx->stats.rx_sc_stats->InPktsUnusedSA = rxsc->stats.InPktsUnusedSA;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_get_dev_stats(struct macsec_context *ctx)
{
        struct rtl8127_private *tp = macsec_netdev_priv(ctx->netdev);
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;
        const struct macsec_secy *ctx_secy = ctx->secy;
        struct rtl8261d_macsec_secy *secy;
        int i, ret = 0, secy_idx = -1;

        mutex_lock(&phytp->lock);

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                if (macsec->secy[i].inuse && macsec->secy[i].kernel_secy == ctx_secy) {
                        secy_idx = i;
                        secy = &macsec->secy[secy_idx];
                        break;
                }
        }
        if (secy_idx == -1) {
                pr_err(KBUILD_MODNAME ": Parent secy doesn't exist\n");
                ret = -EINVAL;
                goto out;
        }

        ret = rtl8261d_syn_hw(phydev, EGRESS);
        if (ret < 0)
                goto out;

        ret  = rtl8261d_update_secy_cnt(phydev, secy_idx, macsec);
        if (ret < 0)
                goto out;

        ctx->stats.dev_stats->OutPktsUntagged = secy->secy_stats.OutPktsUntagged;
        ctx->stats.dev_stats->InPktsUntagged = secy->secy_stats.InPktsUntagged;
        ctx->stats.dev_stats->OutPktsTooLong = secy->secy_stats.OutPktsTooLong;
        ctx->stats.dev_stats->InPktsNoTag = secy->secy_stats.InPktsNoTag;
        ctx->stats.dev_stats->InPktsBadTag = secy->secy_stats.InPktsBadTag;
        ctx->stats.dev_stats->InPktsUnknownSCI = secy->secy_stats.InPktsUnknownSCI;
        ctx->stats.dev_stats->InPktsNoSCI = secy->secy_stats.InPktsNoSCI;
        ctx->stats.dev_stats->InPktsOverrun = 0;

out:
        mutex_unlock(&phytp->lock);
        return ret;
}

static int rtl8261d_set_fix_latency(struct phy_device *phydev)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        int ret, link_status;
        u16 is_xg_mode;

        link_status = rtl8261_read_phy_ocp(phydev, MDIO_MMD_VEND2, SPHY_LINK_SPEED);
        if (link_status < 0)
                return link_status;

        if (((u32)link_status & 0x0600) && (((u32)link_status & 0x0030) != 0x0030))
                is_xg_mode = xg_mode;
        else
                is_xg_mode = legacy_mode;

        if (is_xg_mode == phytp->HwIsXgMode)
                return 0;

        if (is_xg_mode) {
                ret = rtl8261d_macsec_comp_write(phydev, 0x797C, 0x3B, EGRESS);
                if (ret < 0)
                        return ret;

                ret = rtl8261d_macsec_comp_write(phydev, 0x797C, 0x44, INGRESS);
                if (ret < 0)
                        return ret;
        } else {
                ret = rtl8261d_macsec_comp_write(phydev, 0x797C, 0x50, EGRESS);
                if (ret < 0)
                        return ret;

                ret = rtl8261d_macsec_comp_write(phydev, 0x797C, 0x58, INGRESS);
                if (ret < 0)
                        return ret;
        }

        ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_DATA_PATH, 0x00 | (is_xg_mode << 2));
        if (ret < 0)
                return ret;

        phytp->HwIsXgMode = is_xg_mode;

        return 0;
}

static int rtl8261d_set_uption(struct phy_device *phydev)
{
        int ret;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6972, 0xF0F0, (7 << 12) | (7 << 4));
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6973, 0xF0F0, (7 << 12) | (7 << 4));
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6974, 0xF0F0, (7 << 12) | (7 << 4));
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6975, 0xF0F0, (7 << 12) | (7 << 4));
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6976, 0xF0F0, (7 << 12) | (7 << 4));
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6977, 0xF0F0, (7 << 12) | (7 << 4));
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_clear_uption(struct phy_device *phydev)
{
        int ret;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6972, 0xF0F0, (0 << 12) | (0 << 4));
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6973, 0xF0F0, (0 << 12) | (0 << 4));
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6974, 0xF0F0, (0 << 12) | (0 << 4));
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6975, 0xF0F0, (0 << 12) | (0 << 4));
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6976, 0xF0F0, (0 << 12) | (0 << 4));
        if (ret < 0)
                return ret;
        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND1, 0x6977, 0xF0F0, (0 << 12) | (0 << 4));
        if (ret < 0)
                return ret;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_AN, MDIO_CTRL1,
                                     MDIO_AN_CTRL1_ENABLE | MDIO_AN_CTRL1_RESTART,
                                     MDIO_AN_CTRL1_ENABLE | MDIO_AN_CTRL1_RESTART);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_macsec_swinit(struct phy_device *phydev)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec;
        int i, j;

        macsec = kzalloc(sizeof(*macsec), GFP_KERNEL);
        if (!macsec) {
                return -ENOMEM;
        }

        phytp->macsec = macsec;

        for (i = 0; i < RTL8261D_HW_MAX_SECY; i++) {
                macsec->secy[i].replay_window = 0x80;

                for (j = 0; j < MACSEC_NUM_AN; j++)
                        macsec->secy[i].tx_sa_idxs[j] = -1;

                for (j = 0; j < RTL8261D_HW_MAX_RXSC; j++)
                        macsec->secy[i].rx_sc_idxs[j] = -1;
        }

        for (i = 0; i < RTL8261D_HW_MAX_RXSC; i++) {
                for (j = 0; j < MACSEC_NUM_AN; j++)
                        macsec->rxsc[i].rx_sa_idxs[j] = -1;
        }

        for (i = 0; i < RTL8261D_HW_MAX_TCAM; i++)
                macsec->tcam[i].secy_idx = -1;

        phytp->HwIsXgMode = unknown_mode;

        return 0;
}

static int rtl8261d_macsec_sysinit(struct phy_device *phydev)
{
        int ret;

        ret = rtl8261d_set_uption(phydev);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_macsec_topinit(struct phy_device *phydev)
{
        int ret;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_EN, (1 << 11) | (1 << 10),
                                     (1 << 11) | (1 << 10));
        if (ret < 0)
                return ret;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND2, 0xDE0A, (1 << 0), 0x0);
        if (ret < 0)
                return ret;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_IFG_SEL,
                                     MACSEC_IFG_SEL_MASK, (2 << 13));
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_macsec_cvtrinit(struct phy_device *phydev)
{
        int ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0x0400, 0x8, CONVERTER);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0x0000, 0x00000001, CONVERTER);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_macsec_coreinit(struct phy_device *phydev)
{
        u32 channel_data;
        int ret, val;

        ret = rtl8261d_macsec_comp_write(phydev, 0xF148, 0x8000003c, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0xE210,
                                         CNT_READ_CLR | CNT_READ_OVERSTOP, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0xE410,
                                         CNT_READ_CLR | CNT_READ_OVERSTOP, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0x4410,
                                         CNT_READ_CLR | CNT_READ_OVERSTOP, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0xE210,
                                         CNT_READ_CLR | CNT_READ_OVERSTOP, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0xE410,
                                         CNT_READ_CLR | CNT_READ_OVERSTOP, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0x4410,
                                         CNT_READ_CLR | CNT_READ_OVERSTOP, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0x5410,
                                         CNT_READ_CLR | CNT_READ_OVERSTOP, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0xF408, (0xE588 << 16) | (1 << 10), EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0xF408, (0xE588 << 16) | (1 << 10), INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0xF430, 0x3, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0xF430, 0x3, INGRESS);
        if (ret < 0)
                return ret;

        val = rtl8261d_macsec_comp_read(phydev, 0x797C, &channel_data, EGRESS);
        if (val < 0)
                return val;
        channel_data &= ~((1 << 11) | (1 << 10) | (1 << 9));
        channel_data |= ((2 << 10) | (1 << 9));

        ret = rtl8261d_macsec_comp_write(phydev, 0x797C, channel_data, EGRESS);
        if (ret < 0)
                return ret;

        val = rtl8261d_macsec_comp_read(phydev, 0x797C, &channel_data, INGRESS);
        if (val < 0)
                return val;
        channel_data &= ~((1 << 11) | (1 << 10) | (1 << 9));
        channel_data |= ((2 << 10) | (1 << 9));
        ret = rtl8261d_macsec_comp_write(phydev, 0x797C, channel_data, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0x78F0, 0x8000000F, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0x78F0, 0x8000000F, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0x7804, 0x08880000, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0x7804, 0x08880000, INGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0x78FC, 0x80180100, EGRESS);
        if (ret < 0)
                return ret;

        ret = rtl8261d_macsec_comp_write(phydev, 0x78FC, 0x80180100, INGRESS);
        if (ret < 0)
                return ret;

        return 0;
}

static int rtl8261d_macsec_init(struct phy_device *phydev)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        int ret = 0;

        ret = rtl8261d_macsec_swinit(phydev);
        if (ret < 0)
                goto out;

        ret = rtl8261d_macsec_sysinit(phydev);
        if (ret < 0)
                goto free_macsec_out;

        ret = rtl8261d_macsec_topinit(phydev);
        if (ret < 0)
                goto free_macsec_out;

        ret = rtl8261d_macsec_cvtrinit(phydev);
        if (ret < 0)
                goto free_macsec_out;

        ret = rtl8261d_macsec_coreinit(phydev);
        if (ret < 0)
                goto free_macsec_out;

out:
        return ret;

free_macsec_out:
        kfree(phytp->macsec);
        phytp->macsec = NULL;
        goto out;
}

static void rtl8261d_macsec_swremove(struct phy_device *phydev)
{
        struct rtl8261_phy_private *phytp = phydev->priv;
        struct rtl8261d_macsec_private *macsec = phytp->macsec;

        kfree(macsec);

        phytp->macsec = NULL;
}

static int rtl8261d_sysremove(struct phy_device *phydev)
{
        int ret, rc = 0;

        ret = rtl8261d_clear_uption(phydev);
        if (ret < 0)
                return ret;

        return rc;
}

static int rtl8261d_topremove(struct phy_device *phydev)
{
        int ret, rc = 0;

        ret = rtl8261_write_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_DATA_PATH, 0x30);
        if (ret < 0)
                rc = ret;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND2, 0xDC0E, (1 << 15), (0 << 15));
        if (ret < 0)
                rc = ret;

        ret = rtl8261_modify_phy_ocp(phydev, MDIO_MMD_VEND2, MACSEC_EN, (1 << 10), (0 << 10));
        if (ret < 0)
                rc = ret;

        return rc;
}

static void rtl8261d_macsec_remove(struct phy_device *phydev)
{
        int ret;

        ret = rtl8261d_sysremove(phydev);
        if (ret < 0)
                pr_err(KBUILD_MODNAME ": System remove error\n");

        ret = rtl8261d_topremove(phydev);
        if (ret < 0)
                pr_err(KBUILD_MODNAME ": Top remove error\n");

        rtl8261d_macsec_swremove(phydev);
}

int rtl8127_macsec_set_fix_latency(struct rtl8127_private *tp)
{
        struct phy_device *phydev = tp->phydev;
        struct rtl8261_phy_private *phytp;
        int ret;

        if (!HW_SUPPORT_MDIO(tp))
                return 0;

        phytp = phydev->priv;

        mutex_lock(&phytp->lock);

        ret = rtl8261d_set_fix_latency(phydev);

        mutex_unlock(&phytp->lock);

        return ret;
}

static const struct macsec_ops rtl8261d_macsec_ops = {
        .mdo_add_secy = rtl8261d_add_secy,
        .mdo_upd_secy = rtl8261d_upd_secy,
        .mdo_del_secy = rtl8261d_del_secy,
        .mdo_add_rxsc = rtl8261d_add_rxsc,
        .mdo_upd_rxsc = rtl8261d_upd_rxsc,
        .mdo_del_rxsc = rtl8261d_del_rxsc,
        .mdo_add_txsa = rtl8261d_add_txsa,
        .mdo_upd_txsa = rtl8261d_upd_txsa,
        .mdo_del_txsa = rtl8261d_del_txsa,
        .mdo_add_rxsa = rtl8261d_add_rxsa,
        .mdo_upd_rxsa = rtl8261d_upd_rxsa,
        .mdo_del_rxsa = rtl8261d_del_rxsa,
        .mdo_get_dev_stats = rtl8261d_get_dev_stats,
        .mdo_get_tx_sc_stats = rtl8261d_get_tx_sc_stats,
        .mdo_get_tx_sa_stats = rtl8261d_get_tx_sa_stats,
        .mdo_get_rx_sc_stats = rtl8261d_get_rx_sc_stats,
        .mdo_get_rx_sa_stats = rtl8261d_get_rx_sa_stats,
};

int rtl8127_macsec_init(struct net_device *netdev)
{
        struct rtl8127_private *tp = netdev_priv(netdev);
        int ret;

        if (!HW_SUPPORT_MDIO(tp))
                return 0;

        ret = rtl8261d_macsec_init(tp->phydev);
        if (ret < 0)
                return ret;

        netdev->macsec_ops = &rtl8261d_macsec_ops;
        netdev->features |= NETIF_F_HW_MACSEC;
        // netif_keep_dst(netdev);

        return 0;
}

void rtl8127_macsec_remove(struct net_device *netdev)
{
        struct rtl8127_private *tp = netdev_priv(netdev);

        if (!HW_SUPPORT_MDIO(tp))
                return;

        rtl8261d_macsec_remove(tp->phydev);
}
#else
int rtl8127_macsec_set_fix_latency(struct rtl8127_private *tp)
{
        return 0;
}

int rtl8127_macsec_init(struct net_device *netdev)
{
        return 0;
}

void rtl8127_macsec_remove(struct net_device *netdev)
{
        return;
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0) */
