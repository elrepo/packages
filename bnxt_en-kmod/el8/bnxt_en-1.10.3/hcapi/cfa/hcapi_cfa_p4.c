// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include "bnxt_compat.h"
#include <linux/jhash.h>

#include <linux/crc32.h>
#include "rand.h"

#include "hcapi_cfa.h"
#include "hcapi_cfa_defs.h"

static u32 hcapi_cfa_lkup_lkup3_init_cfg;
static u32 hcapi_cfa_lkup_em_seed_mem[HCAPI_CFA_LKUP_SEED_MEM_SIZE];
static bool hcapi_cfa_lkup_init;

static void hcapi_cfa_seeds_init(void)
{
	int i;
	u32 r;

	if (hcapi_cfa_lkup_init)
		return;

	hcapi_cfa_lkup_init = true;

	/* Initialize the lfsr */
	rand_init();

	/* RX and TX use the same seed values */
	hcapi_cfa_lkup_lkup3_init_cfg = swahb32(rand32());

	for (i = 0; i < HCAPI_CFA_LKUP_SEED_MEM_SIZE / 2; i++) {
		r = swahb32(rand32());
		hcapi_cfa_lkup_em_seed_mem[i * 2] = r;
		r = swahb32(rand32());
		hcapi_cfa_lkup_em_seed_mem[i * 2 + 1] = (r & 0x1);
	}
}

static u32 hcapi_cfa_crc32_hash(u8 *key)
{
	u8 *kptr = key;
	u32 val1, val2;
	u8 temp[4];
	u32 index;
	int i;

	/* Do byte-wise XOR of the 52-byte HASH key first. */
	index = *key;
	kptr++;

	for (i = 0; i < (CFA_P58_EEM_KEY_MAX_SIZE - 1); i++) {
		index = index ^ *kptr;
		kptr++;
	}

	/* Get seeds */
	val1 = hcapi_cfa_lkup_em_seed_mem[index * 2];
	val2 = hcapi_cfa_lkup_em_seed_mem[index * 2 + 1];

	temp[0] = (u8)(val1 >> 24);
	temp[1] = (u8)(val1 >> 16);
	temp[2] = (u8)(val1 >> 8);
	temp[3] = (u8)(val1 & 0xff);
	val1 = 0;

	/* Start with seed */
	if (!(val2 & 0x1))
		val1 = ~(crc32(~val1, temp, 4));

	val1 = ~(crc32(~val1,
		       key,
		       CFA_P58_EEM_KEY_MAX_SIZE));

	/* End with seed */
	if (val2 & 0x1)
		val1 = ~(crc32(~val1, temp, 4));

	return val1;
}

static u32 hcapi_cfa_lookup3_hash(u8 *in_key)
{
	u32 val1;

	val1 = jhash2(((u32 *)in_key),
		      CFA_P4_EEM_KEY_MAX_SIZE / (sizeof(u32)),
		      hcapi_cfa_lkup_lkup3_init_cfg);

	return val1;
}

u64 hcapi_get_table_page(struct hcapi_cfa_em_table *mem, u32 page)
{
	int level = 0;
	u64 addr;

	if (!mem)
		return 0;

	/* Use the level according to the num_level of page table */
	level = mem->num_lvl - 1;
	addr = (u64)mem->pg_tbl[level].pg_va_tbl[page];

	return addr;
}

/* Approximation of HCAPI hcapi_cfa_key_hash() */
u64 hcapi_cfa_p4_key_hash(u8 *key_data, u16 bitlen)
{
	u32 key0_hash;
	u32 key1_hash;
	u32 *key_word = (u32 *)key_data;
	u32 lk3_key[CFA_P4_EEM_KEY_MAX_SIZE / sizeof(u32)];
	u32 i;

	/* Init the seeds if needed */
	if (!hcapi_cfa_lkup_init)
		hcapi_cfa_seeds_init();

	key0_hash = hcapi_cfa_crc32_hash(key_data);

	for (i = 0; i < (bitlen / 8) / sizeof(uint32_t); i++)
		lk3_key[i] = swab32(key_word[i]);

	key1_hash = hcapi_cfa_lookup3_hash((u8 *)lk3_key);

	return ((u64)key0_hash) << 32 | (u64)key1_hash;
}

const struct hcapi_cfa_devops cfa_p4_devops = {
	.hcapi_cfa_key_hash = hcapi_cfa_p4_key_hash,
};
