// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2026 Broadcom
 * All rights reserved.
 */
#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_sriov.h"
#include <bnxt_tfc.h>

#include "cfa_types.h"
#include "cfa_mm.h"
#include "cfa_bld_mpc_field_ids.h"
#include "cfa_bld_mpcops.h"
#include "tfc.h"
#include "tfo.h"
#include "tfc_em.h"
#include "tfc_util.h"
#include "tfc_debug.h"
#include "tfc_debug.h"

/* Helper function to decode bucket and format as string */
int tfc_em_format_bucket_decode(char *buf, int bufsize, u32 *bucket_ptr)
{
	int i;
	int len = 0;
	int offset = 0;
	u32 chain, chain_ptr;
	u32 entry_ptr, hash_msb;

	chain = tfc_getbits(bucket_ptr, 254, 1);
	chain_ptr = tfc_getbits(bucket_ptr, 228, 26);

	len += scnprintf(buf + len, bufsize - len,
			 "Bucket Decode:\n"
			 "Chain: %u\n"
			 "Chain Ptr: 0x%07x\n"
			 "Entries:\n",
			 chain, chain_ptr);

	for (i = 0; i < TFC_BUCKET_ENTRIES; i++) {
		entry_ptr = tfc_getbits(bucket_ptr, offset, 26);
		offset += 26;
		hash_msb = tfc_getbits(bucket_ptr, offset, 12);
		offset += 12;

		len += scnprintf(buf + len, bufsize - len,
				 "  Entry %d: hash_msb=0x%03x entry_ptr=0x%07x\n",
				 i, hash_msb, entry_ptr);
	}

	return len;
}

/* Helper function to decode EM using actual em_decode and em_show functions */
int tfc_em_format_em_decode(char *buf, int bufsize, u32 *em_ptr,
			    struct tfc *tfcp, u8 tsid, enum cfa_dir dir)
{
	int i;
	int len = 0;
	char *line1 = NULL;
	char *line2 = NULL;
	char *line3 = NULL;
	char *line4 = NULL;
	struct em_info_t *em_info = NULL;
	char tmp1[64], tmp2[64], tmp3[64], tmp4[64];

	em_info = kmalloc(sizeof(*em_info), GFP_KERNEL);
	line1 = kmalloc(256, GFP_KERNEL);
	line2 = kmalloc(256, GFP_KERNEL);
	line3 = kmalloc(256, GFP_KERNEL);
	line4 = kmalloc(256, GFP_KERNEL);
	if (!em_info || !line1 || !line2 || !line3 || !line4) {
		kfree((char *)em_info);
		kfree(line1);
		kfree(line2);
		kfree(line3);
		kfree(line4);
		len += scnprintf(buf + len, bufsize - len,
				 "%s: Failed to allocate temp buffer\n",
				 __func__);
		return len;
	}

	memset(em_info, 0, sizeof(*em_info));
	em_info->key = (uint8_t *)em_ptr;
	em_ptr += (128 / 8) / 4; /* For EM records the LREC follows 128 bits of key */
	em_info->valid = tfc_getbits(em_ptr, 127, 1);
	em_info->rec_size = tfc_getbits(em_ptr, 125, 2);
	em_info->epoch0 = tfc_getbits(em_ptr, 113, 12);
	em_info->epoch1 = tfc_getbits(em_ptr, 107, 6);
	em_info->opcode = tfc_getbits(em_ptr, 103, 4);
	em_info->strength = tfc_getbits(em_ptr, 101, 2);
	em_info->act_hint = tfc_getbits(em_ptr, 99, 2);

	if (em_info->opcode != 2 && em_info->opcode != 3) {
		/* All but FAST */
		em_info->act_rec_ptr = tfc_getbits(em_ptr, 73, 26);
	} else {
		/* Just FAST */
		em_info->destination = tfc_getbits(em_ptr, 73, 17);
	}

	if (em_info->opcode == 4 || em_info->opcode == 6) {
		/* CT only */
		em_info->tcp_direction = tfc_getbits(em_ptr, 72, 1);
		em_info->tcp_update_en = tfc_getbits(em_ptr, 71, 1);
		em_info->tcp_win = tfc_getbits(em_ptr, 66, 5);
		em_info->tcp_msb_loc = tfc_getbits(em_ptr, 48, 18);
		em_info->tcp_msb_opp = tfc_getbits(em_ptr, 30, 18);
		em_info->tcp_msb_opp_init = tfc_getbits(em_ptr, 29, 1);
		em_info->state = tfc_getbits(em_ptr, 24, 5);
		em_info->timer_value  = tfc_getbits(em_ptr, 20, 4);
	} else if (em_info->opcode != 8) {
		/* Not CT and nor RECYCLE */
		em_info->ring_table_idx = tfc_getbits(em_ptr, 64, 9);
		em_info->act_rec_size = tfc_getbits(em_ptr, 59, 5);
		em_info->paths_m1 = tfc_getbits(em_ptr, 55, 4);
		em_info->fc_op  = tfc_getbits(em_ptr, 54, 1);
		em_info->fc_type = tfc_getbits(em_ptr, 52, 2);
		em_info->fc_ptr = tfc_getbits(em_ptr, 24, 28);
	} else {
		em_info->recycle_dest = tfc_getbits(em_ptr, 72, 1); /* Just Recycle */
		em_info->prof_func = tfc_getbits(em_ptr, 64, 8);
		em_info->meta_prof = tfc_getbits(em_ptr, 61, 3);
		em_info->metadata = tfc_getbits(em_ptr, 29, 32);
	}

	em_info->range_profile = tfc_getbits(em_ptr, 16, 4);
	em_info->range_index = tfc_getbits(em_ptr, 0, 16);

	len += scnprintf(buf + len, bufsize - len, ":LREC: opcode:%s\n",
			 get_lrec_opcode_str(em_info->opcode));

	snprintf(line1, 256, "+-+--+-Epoch-+--+--+--+");
	snprintf(line2, 256, " V|rs|  0  1 |Op|St|ah|");
	snprintf(line3, 256, "+-+--+----+--+--+--+--+");
	snprintf(line4, 256, " %1d %2d %4d %2d %2d %2d %2d ",
		 em_info->valid,
		 em_info->rec_size,
		 em_info->epoch0,
		 em_info->epoch1,
		 em_info->opcode,
		 em_info->strength,
		 em_info->act_hint);

	if (em_info->opcode != 2 && em_info->opcode != 3) {
		/* All but FAST */
		snprintf(tmp1, 64, "-Act Rec--+");
		snprintf(tmp2, 64, " Ptr      |");
		snprintf(tmp3, 64, "----------+");
		snprintf(tmp4, 64, "0x%08x ",
			 em_info->act_rec_ptr);
	} else {
		/* Just FAST */
		snprintf(tmp1, 64, "-------+");
		snprintf(tmp2, 64, " Dest  |");
		snprintf(tmp3, 64, "-------+");
		snprintf(tmp4, 64, "0x05%x ",
			 em_info->destination);
	}

	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);

	if (em_info->opcode == 4 || em_info->opcode == 6) {
		/* CT only */
		snprintf(tmp1, 64, "--+--+-------------TCP-------+--+---+");
		snprintf(tmp2, 64, "Dr|ue| Win|   lc  |   op  |oi|st|tmr|");
		snprintf(tmp3, 64, "--+--+----+-------+-------+--+--+---+");
		snprintf(tmp4, 64, "%2d %2d %4d 0x%5x 0x%5x %2d %2d %3d ",
			 em_info->tcp_direction,
			 em_info->tcp_update_en,
			 em_info->tcp_win,
			 em_info->tcp_msb_loc,
			 em_info->tcp_msb_opp,
			 em_info->tcp_msb_opp_init,
			 em_info->state,
			 em_info->timer_value);
	} else if (em_info->opcode != 8) {
		/* Not CT and nor RECYCLE */
		snprintf(tmp1, 64, "--+--+--+-------FC-------+");
		snprintf(tmp2, 64, "RI|as|pm|op|tp|     Ptr  |");
		snprintf(tmp3, 64, "--+--+--+--+--+----------+");
		snprintf(tmp4, 64, "%2d %2d %2d %2d %2d 0x%08x ",
			 em_info->ring_table_idx,
			 em_info->act_rec_size,
			 em_info->paths_m1,
			 em_info->fc_op,
			 em_info->fc_type,
			 em_info->fc_ptr);
	} else {
		snprintf(tmp1, 64, "--+--+--+---------+");
		snprintf(tmp2, 64, "RD|pf|mp| cMData  |");
		snprintf(tmp3, 64, "--+--+--+---------+");
		snprintf(tmp4, 64, "%2d 0x%2x %2d %08x ",
			 em_info->recycle_dest,
			 em_info->prof_func,
			 em_info->meta_prof,
			 em_info->metadata);
	}

	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);

	snprintf(tmp1, 64, "-----Range-+\n");
	snprintf(tmp2, 64, "Prof|  Idx |\n");
	snprintf(tmp3, 64, "----+------+\n");
	snprintf(tmp4, 64, "0x%02x 0x%04x\n",
		 em_info->range_profile,
		 em_info->range_index);

	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);

	len += scnprintf(buf + len, bufsize - len, "%s%s%s%s",
			 line1,
			 line2,
			 line3,
			 line4);

	len += scnprintf(buf + len, bufsize - len, "Key:");
	for (i = 0; i < ((em_info->rec_size + 1) * 32); i++) {
		if (i % 32 == 0)
			len += scnprintf(buf + len, bufsize - len, "\n%04d:  ", i);
		len += scnprintf(buf + len, bufsize - len, "%02x", em_info->key[i]);
	}
	i = ((em_info->rec_size + 1) * 32);
	len += scnprintf(buf + len, bufsize - len, "\nKey Reversed:\n%04d:  ", i - 32);
	do {
		i--;
		len += scnprintf(buf + len, bufsize - len, "%02x", em_info->key[i]);
		if (i != 0 && i % 32 == 0)
			len += scnprintf(buf + len, bufsize - len, "\n%04d:  ", i - 32);
	} while (i > 0);
	len += scnprintf(buf + len, bufsize - len, "\n");

	kfree((char *)em_info);
	kfree(line1);
	kfree(line2);
	kfree(line3);
	kfree(line4);

	/* Return the amount written */
	return len;
}
