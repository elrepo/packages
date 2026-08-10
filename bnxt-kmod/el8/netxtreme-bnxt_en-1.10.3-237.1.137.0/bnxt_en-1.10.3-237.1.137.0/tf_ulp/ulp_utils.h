/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_UTILS_H_
#define _ULP_UTILS_H_

#include "bnxt.h"
#include "ulp_template_db_enum.h"

#define ULP_BUFFER_ALIGN_8_BITS		8
#define ULP_BUFFER_ALIGN_8_BYTE		8
#define ULP_BUFFER_ALIGN_16_BYTE	16
#define ULP_BUFFER_ALIGN_64_BYTE	64
#define ULP_64B_IN_BYTES		8
#define ULP_64B_IN_BITS			64

/* Macros for bitmap sets and gets
 * These macros can be used if the val are power of 2.
 */
#define ULP_BITMAP_SET(bitmap, val)	((bitmap) |= (val))
#define ULP_BITMAP_RESET(bitmap, val)	((bitmap) &= ~(val))
#define ULP_BITMAP_ISSET(bitmap, val)	((bitmap) & (val))
#define ULP_BITMAP_CMP(b1, b2)  memcmp(&(b1)->bits, \
				&(b2)->bits, sizeof((b1)->bits))
/* Macros for bitmap sets and gets
 * These macros can be used if the val are not power of 2 and
 * are simple index values.
 */
#define ULP_INDEX_BITMAP_SIZE	(sizeof(u64) * 8)
#define ULP_INDEX_BITMAP_CSET(i)	(1UL << \
			((ULP_INDEX_BITMAP_SIZE - 1) - \
			((i) % ULP_INDEX_BITMAP_SIZE)))

#define ULP_INDEX_BITMAP_SET(b, i)	((b) |= \
			(1UL << ((ULP_INDEX_BITMAP_SIZE - 1) - \
			((i) % ULP_INDEX_BITMAP_SIZE))))

#define ULP_INDEX_BITMAP_RESET(b, i)	((b) &= \
			(~(1UL << ((ULP_INDEX_BITMAP_SIZE - 1) - \
			((i) % ULP_INDEX_BITMAP_SIZE)))))

#define ULP_INDEX_BITMAP_GET(b, i)		(((b) >> \
			((ULP_INDEX_BITMAP_SIZE - 1) - \
			((i) % ULP_INDEX_BITMAP_SIZE))) & 1)

#define ULP_DEVICE_PARAMS_INDEX(tid, dev_id)	\
	(((tid) << BNXT_ULP_LOG2_MAX_NUM_DEV) | (dev_id))

/* Macro to convert bytes to bits */
#define ULP_BYTE_2_BITS(byte_x)		((byte_x) * 8)
/* Macro to convert bits to bytes */
#define ULP_BITS_2_BYTE(bits_x)		(((bits_x) + 7) / 8)
/* Macro to convert bits to bytes with no round off*/
#define ULP_BITS_2_BYTE_NR(bits_x)	((bits_x) / 8)

/* Macro to round off to next multiple of 8*/
#define ULP_BYTE_ROUND_OFF_8(x)	(((x) + 7) & ~7)

/* Macro to check bits are byte aligned */
#define ULP_BITS_IS_BYTE_NOT_ALIGNED(x)	((x) % 8)

/* Macro for word conversion*/
#define ULP_BITS_TO_4_BYTE_WORD(x) (((x) + 31) / 32)
#define ULP_BITS_TO_32_BYTE_WORD(x) (((x) + 255) / 256)
#define ULP_BITS_TO_4_BYTE_QWORDS(x) (((x) + 127) / 128)
#define ULP_BITS_TO_128B_ALIGNED_BYTES(x) ((((x) + 127) / 128) * 16)

/* Macros to read the computed fields */
#define ULP_COMP_FLD_IDX_RD(params, idx) \
	be64_to_cpu((params)->comp_fld[(idx)])

#define ULP_COMP_FLD_IDX_WR(params, idx, val)	\
	((params)->comp_fld[(idx)] = cpu_to_be64((u64)(val)))

enum bnxt_ulp_resource_type {
	BNXT_ULP_RESOURCE_TYPE_FULL_ACT,
	BNXT_ULP_RESOURCE_TYPE_COMPACT_ACT,
	BNXT_ULP_RESOURCE_TYPE_MCG_ACT,
	BNXT_ULP_RESOURCE_TYPE_MODIFY,
	BNXT_ULP_RESOURCE_TYPE_STAT,
	BNXT_ULP_RESOURCE_TYPE_SRC_PROP,
	BNXT_ULP_RESOURCE_TYPE_ENCAP
};

/* Making the blob statically sized to 128 bytes for now.
 * The blob must be initialized with ulp_blob_init prior to using.
 */
#define BNXT_ULP_FLMP_BLOB_SIZE	(128)
#define BNXT_ULP_FLMP_BLOB_SIZE_IN_BITS	ULP_BYTE_2_BITS(BNXT_ULP_FLMP_BLOB_SIZE)
struct ulp_blob {
	enum bnxt_ulp_byte_order	byte_order;
	u16				write_idx;
	u16				bitlen;
	u8				data[BNXT_ULP_FLMP_BLOB_SIZE];
	u16				encap_swap_idx;
};

/* The data can likely be only 32 bits for now.  Just size check
 * the data when being written.
 */
#define ULP_REGFILE_ENTRY_SIZE	(sizeof(u32))
struct ulp_regfile_entry {
	u64	data;
	u32	size;
};

struct ulp_regfile {
	struct ulp_regfile_entry entry[BNXT_ULP_RF_IDX_LAST];
};

int
ulp_regfile_init(struct ulp_regfile *regfile);

int
ulp_regfile_read(struct ulp_regfile *regfile,
		 enum bnxt_ulp_rf_idx field,
		 u64 *data);

int
ulp_regfile_write(struct ulp_regfile *regfile,
		  enum bnxt_ulp_rf_idx field,
		  u64 data);

u32
ulp_bs_push_lsb(u8 *bs, u16 pos, u8 len, u8 *val);

u32
ulp_bs_push_msb(u8 *bs, u16 pos, u8 len, u8 *val);

int
ulp_blob_init(struct ulp_blob *blob,
	      u16 bitlen,
	      enum bnxt_ulp_byte_order order);

int
ulp_blob_push(struct ulp_blob *blob,
	      u8 *data,
	      u32 datalen);

int
ulp_blob_insert(struct ulp_blob *blob, u32 offset,
		u8 *data, u32 datalen);

u8 *
ulp_blob_push_64(struct ulp_blob *blob,
		 u64 *data,
		 u32 datalen);

u8 *
ulp_blob_push_32(struct ulp_blob *blob,
		 u32 *data,
		 u32 datalen);

int
ulp_blob_push_encap(struct ulp_blob *blob,
		    u8 *data,
		    u32 datalen);

u8 *
ulp_blob_data_get(struct ulp_blob *blob,
		  u16 *datalen);

int
ulp_blob_data_len_get(struct ulp_blob *blob);

void
ulp_bs_pull_lsb(u8 *src, u8 *dst, u32 size,
		u32 offset, u32 len);

void
ulp_bs_pull_msb(u8 *src, u8 *dst,
		u32 offset, u32 len);

int
ulp_blob_pull(struct ulp_blob *blob, u8 *data, u32 data_size,
	      u16 offset, u16 len);

int
ulp_blob_pad_push(struct ulp_blob *blob,
		  u32 datalen);

int
ulp_blob_pad_align(struct ulp_blob *blob,
		   u32 align);

void
ulp_blob_encap_swap_idx_set(struct ulp_blob *blob);

void
ulp_blob_perform_encap_swap(struct ulp_blob *blob);

void
ulp_blob_perform_byte_reverse(struct ulp_blob *blob,
			      u32 chunk_size);

void
ulp_blob_perform_64B_word_swap(struct ulp_blob *blob);

void
ulp_blob_perform_64B_byte_swap(struct ulp_blob *blob);

int
ulp_blob_block_merge(struct ulp_blob *dst, struct ulp_blob *src,
		     u32 block_size, u32 pad);

int
ulp_blob_append(struct ulp_blob *dst, struct ulp_blob *src,
		u16 src_offset, u16 src_len);

int
ulp_blob_buffer_copy(struct ulp_blob *dst, struct ulp_blob *src);

int
ulp_operand_read(u8 *operand,
		 u8 *val,
		 u16 bitlen);

int ulp_buffer_is_empty(const u8 *buf, u32 size);

/* Function to check if bitmap is zero.Return 1 on success */
u32 ulp_bitmap_is_zero(u8 *bitmap, int size);

/* Function to check if bitmap is ones. Return 1 on success */
u32 ulp_bitmap_is_ones(u8 *bitmap, int size);

/* Function to check if bitmap is not zero. Return 1 on success */
u32 ulp_bitmap_notzero(const u8 *bitmap, int size);

/* returns 0 if input is power of 2 */
int ulp_util_is_power_of_2(u64 x);

#endif /* _ULP_UTILS_H_ */
