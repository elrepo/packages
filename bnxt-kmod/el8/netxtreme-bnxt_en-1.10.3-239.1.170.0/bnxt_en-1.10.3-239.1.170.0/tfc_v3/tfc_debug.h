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
#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)
int tfc_wc_show(struct seq_file *m, struct tfc *tfcp, u8 tsid, enum cfa_dir dir);
#else
static inline int tfc_wc_show(struct seq_file *m, struct tfc *tfcp,
			      u8 tsid, enum cfa_dir dir) { return 0; }
#endif
int tfc_mpc_table_invalidate(struct tfc *tfcp, u8 tsid, enum cfa_dir dir,
			     u32 type, u32 offset, u32 words);
int tfc_mpc_table_read(struct tfc *tfcp, u8 tsid, enum cfa_dir dir,
		       u32 type, u32 offset, u8 words, dma_addr_t data_pa, u8 debug);
int tfc_em_format_bucket_decode(char *buf, int bufsize, u32 *bucket_ptr);
int tfc_em_format_em_decode(char *buf, int bufsize, u32 *em_ptr,
			    struct tfc *tfcp, u8 tsid, enum cfa_dir dir);
#endif
