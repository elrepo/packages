/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_DEBUG_H_
#define _TFC_DEBUG_H_

/* #define EM_DEBUG */
/* #define WC_DEBUG */
/* #define ACT_DEBUG */

int tfc_mpc_table_write_zero(struct tfc *tfcp, u8 tsid, enum cfa_dir dir,
			     u32 type, u32 offset, u8 words, u8 *data);
const char *get_lrec_opcode_str(u8 opcode);
int tfc_em_show(struct seq_file *m, struct tfc *tfcp, u8 tsid, enum cfa_dir dir);
int tfc_wc_show(struct seq_file *m, struct tfc *tfcp, u8 tsid, enum cfa_dir dir);
int tfc_mpc_table_invalidate(struct tfc *tfcp, u8 tsid, enum cfa_dir dir,
			     u32 type, u32 offset, u32 words);
#endif
