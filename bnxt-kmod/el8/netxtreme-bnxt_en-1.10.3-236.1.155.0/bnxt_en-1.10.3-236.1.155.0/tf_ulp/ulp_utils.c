// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "ulp_utils.h"

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD) || defined(CONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD)
/**
 * Initialize the regfile structure for writing
 *
 * @regfile: Ptr to a regfile instance
 *
 */
int
ulp_regfile_init(struct ulp_regfile *regfile)
{
	/* validate the arguments */
	if (!regfile)
		return -EINVAL;

	memset(regfile, 0, sizeof(struct ulp_regfile));
	return 0;
}

/**
 * Read a value from the regfile
 *
 * @regfile: The regfile instance. Must be initialized prior to being used
 *
 * @field: The field to be read within the regfile.
 *
 * @data:
 *
 * returns size, zero on failure
 */
int
ulp_regfile_read(struct ulp_regfile *regfile,
		 enum bnxt_ulp_rf_idx field,
		 u64 *data)
{
	/* validate the arguments */
	if (!regfile || field >= BNXT_ULP_RF_IDX_LAST)
		return -EINVAL;

	*data = regfile->entry[field].data;
	return 0;
}

/**
 * Write a value to the regfile
 *
 * @regfile: The regfile instance.  Must be initialized prior to being used
 *
 * @field: The field to be written within the regfile.
 *
 * @data: The value is written into this variable.  It is going to be in the
 * same byte order as it was written.
 *
 * @size: The size in bytes of the value beingritten into this
 * variable.
 *
 * returns 0 on success
 */
int
ulp_regfile_write(struct ulp_regfile *regfile,
		  enum bnxt_ulp_rf_idx field,
		  u64 data)
{
	/* validate the arguments */
	if (!regfile || field >= BNXT_ULP_RF_IDX_LAST)
		return -EINVAL;

	regfile->entry[field].data = data;
	return 0; /* Success */
}

static void
ulp_bs_put_msb(u8 *bs, u16 bitpos, u8 bitlen, u8 val)
{
	u8 bitoffs = bitpos % 8;
	u16 index = bitpos / 8;
	s8 shift;
	u8 mask;
	u8 tmp;

	tmp = bs[index];
	mask = ((u8)-1 >> (8 - bitlen));
	shift = 8 - bitoffs - bitlen;
	val &= mask;

	if (shift >= 0) {
		tmp &= ~(mask << shift);
		tmp |= val << shift;
		bs[index] = tmp;
	} else {
		tmp &= ~((u8)-1 >> bitoffs);
		tmp |= val >> -shift;
		bs[index++] = tmp;

		tmp = bs[index];
		tmp &= ((u8)-1 >> (bitlen - (8 - bitoffs)));
		tmp |= val << (8 + shift);
		bs[index] = tmp;
	}
}

static void
ulp_bs_put_lsb(u8 *bs, u16 bitpos, u8 bitlen, u8 val)
{
	u8 bitoffs = bitpos % 8;
	u16 index = bitpos / 8;
	u8 partial;
	u8 shift;
	u8 mask;
	u8 tmp;

	tmp = bs[index];
	shift = bitoffs;

	if (bitoffs + bitlen <= 8) {
		mask = ((1 << bitlen) - 1) << shift;
		tmp &= ~mask;
		tmp |= ((val << shift) & mask);
		bs[index] = tmp;
	} else {
		partial = 8 - bitoffs;
		mask = ((1 << partial) - 1) << shift;
		tmp &= ~mask;
		tmp |= ((val << shift) & mask);
		bs[index++] = tmp;

		val >>= partial;
		partial = bitlen - partial;
		mask = ((1 << partial) - 1);
		tmp = bs[index];
		tmp &= ~mask;
		tmp |= (val & mask);
		bs[index] = tmp;
	}
}

/**
 * Add data to the byte array in Little endian format.
 *
 * @bs: The byte array where data is pushed
 *
 * @pos: The offset where data is pushed
 *
 * @len: The number of bits to be added to the data array.
 *
 * @val: The data to be added to the data array.
 *
 * returns the number of bits pushed.
 */
u32
ulp_bs_push_lsb(u8 *bs, u16 pos, u8 len, u8 *val)
{
	int cnt = (len) / 8;
	int tlen = len;
	int i;

	if (cnt > 0 && !(len % 8))
		cnt -= 1;

	for (i = 0; i < cnt; i++) {
		ulp_bs_put_lsb(bs, pos, 8, val[cnt - i]);
		pos += 8;
		tlen -= 8;
	}

	/* Handle the remainder bits */
	if (tlen)
		ulp_bs_put_lsb(bs, pos, tlen, val[0]);
	return len;
}

/**
 * Add data to the byte array in Big endian format.
 *
 * @bs: The byte array where data is pushed
 *
 * @pos: The offset where data is pushed
 *
 * @len: The number of bits to be added to the data array.
 *
 * @val: The data to be added to the data array.
 *
 * returns the number of bits pushed.
 */
u32
ulp_bs_push_msb(u8 *bs, u16 pos, u8 len, u8 *val)
{
	int cnt = (len + 7) / 8;
	int i;

	/* Handle any remainder bits */
	int tmp = len % 8;

	if (!tmp)
		tmp = 8;

	ulp_bs_put_msb(bs, pos, tmp, val[0]);

	pos += tmp;

	for (i = 1; i < cnt; i++) {
		ulp_bs_put_msb(bs, pos, 8, val[i]);
		pos += 8;
	}

	return len;
}

/**
 * Initializes the blob structure for creating binary blob
 *
 * @blob: The blob to be initialized
 *
 * @bitlen: The bit length of the blob
 *
 * @order: The byte order for the blob.  Currently only supporting
 * big endian.  All fields are packed with this order.
 *
 * Notes - If bitlen is zero then set it to max.
 */
int
ulp_blob_init(struct ulp_blob *blob,
	      u16 bitlen,
	      enum bnxt_ulp_byte_order order)
{
	/* validate the arguments */
	if (!blob || bitlen > (8 * sizeof(blob->data)))
		return -EINVAL;

	if (bitlen)
		blob->bitlen = bitlen;
	else
		blob->bitlen = BNXT_ULP_FLMP_BLOB_SIZE_IN_BITS;
	blob->byte_order = order;
	blob->write_idx = 0;
	memset(blob->data, 0, sizeof(blob->data));
	return 0;
}

/**
 * Add data to the binary blob at the current offset.
 *
 * @blob: The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * @data: A pointer to bytes to be added to the blob.
 *
 * @datalen: The number of bits to be added to the blob.
 *
 * The offset of the data is updated after each push of data.
 * NULL returned on error.
 */
#define ULP_BLOB_BYTE		8
#define ULP_BLOB_BYTE_HEX	0xFF
#define BLOB_MASK_CAL(x)	((0xFF << (x)) & 0xFF)
int
ulp_blob_push(struct ulp_blob *blob,
	      u8 *data,
	      u32 datalen)
{
	u32 rc;

	/* validate the arguments */
	if (!blob || datalen > (u32)(blob->bitlen - blob->write_idx))
		return -EINVAL;

	if (blob->byte_order == BNXT_ULP_BYTE_ORDER_BE)
		rc = ulp_bs_push_msb(blob->data,
				     blob->write_idx,
				     datalen,
				     data);
	else
		rc = ulp_bs_push_lsb(blob->data,
				     blob->write_idx,
				     datalen,
				     data);
	if (!rc)
		return -EINVAL;

	blob->write_idx += datalen;
	return 0;
}

/**
 * Insert data into the binary blob at the given offset.
 *
 * @blob: The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * @offset: The offset where the data needs to be inserted.
 *
 * @data: A pointer to bytes to be added to the blob.
 *
 * @datalen: The number of bits to be added to the blob.
 *
 * The offset of the data is updated after each push of data.
 * NULL returned on error.
 */
int
ulp_blob_insert(struct ulp_blob *blob, u32 offset,
		u8 *data, u32 datalen)
{
	u8 local_data[BNXT_ULP_FLMP_BLOB_SIZE];
	u16 mov_len;
	u32 rc;

	/* validate the arguments */
	if (!blob || datalen > (u32)(blob->bitlen - blob->write_idx) ||
	    offset > blob->write_idx)
		return -EINVAL;

	mov_len = blob->write_idx - offset;
	/* If offset and data len are not 8 bit aligned then return error */
	if (ULP_BITS_IS_BYTE_NOT_ALIGNED(offset) ||
	    ULP_BITS_IS_BYTE_NOT_ALIGNED(datalen))
		return -EINVAL;

	/* copy the data so we can move the data */
	memcpy(local_data, &blob->data[ULP_BITS_2_BYTE_NR(offset)],
	       ULP_BITS_2_BYTE(mov_len));
	blob->write_idx = offset;
	if (blob->byte_order == BNXT_ULP_BYTE_ORDER_BE)
		rc = ulp_bs_push_msb(blob->data,
				     blob->write_idx,
				     datalen,
				     data);
	else
		rc = ulp_bs_push_lsb(blob->data,
				     blob->write_idx,
				     datalen,
				     data);
	if (!rc)
		return -EINVAL;

	/* copy the previously stored data */
	memcpy(&blob->data[ULP_BITS_2_BYTE_NR(offset + datalen)], local_data,
	       ULP_BITS_2_BYTE(mov_len));
	blob->write_idx += (mov_len + datalen);
	return 0;
}

/**
 * Add data to the binary blob at the current offset.
 *
 * @blob: The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * @data: 64-bit value to be added to the blob.
 *
 * @datalen: The number of bits to be added to the blob.
 *
 * The offset of the data is updated after each push of data.
 * NULL returned on error, pointer pushed value otherwise.
 */
u8 *
ulp_blob_push_64(struct ulp_blob *blob,
		 u64 *data,
		 u32 datalen)
{
	u8 *val = (u8 *)data;
	int rc;

	int size = (datalen + 7) / 8;

	if (!blob || !data ||
	    datalen > (u32)(blob->bitlen - blob->write_idx))
		return NULL;

	rc = ulp_blob_push(blob, &val[8 - size], datalen);
	if (rc)
		return NULL;

	return &val[8 - size];
}

/**
 * Add data to the binary blob at the current offset.
 *
 * @blob: The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * @data: 32-bit value to be added to the blob.
 *
 * @datalen: The number of bits to be added ot the blob.
 *
 * The offset of the data is updated after each push of data.
 * NULL returned on error, pointer pushed value otherwise.
 */
u8 *
ulp_blob_push_32(struct ulp_blob *blob,
		 u32 *data,
		 u32 datalen)
{
	u8 *val = (u8 *)data;
	u32 rc;
	u32 size = ULP_BITS_2_BYTE(datalen);

	if (!data || size > sizeof(u32))
		return NULL;

	rc = ulp_blob_push(blob, &val[sizeof(u32) - size], datalen);
	if (rc)
		return NULL;

	return &val[sizeof(u32) - size];
}

/**
 * Add encap data to the binary blob at the current offset.
 *
 * @blob: The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * @data: value to be added to the blob.
 *
 * @datalen: The number of bits to be added to the blob.
 *
 * The offset of the data is updated after each push of data.
 * NULL returned on error, pointer pushed value otherwise.
 */
int
ulp_blob_push_encap(struct ulp_blob *blob,
		    u8 *data,
		    u32 datalen)
{
	u32 initial_size, write_size = datalen;
	u8 *val = (u8 *)data;
	u32 size = 0;

	if (!blob || !data ||
	    datalen > (u32)(blob->bitlen - blob->write_idx))
		return -EINVAL;

	initial_size = ULP_BYTE_2_BITS(sizeof(u64)) -
	    (blob->write_idx % ULP_BYTE_2_BITS(sizeof(u64)));
	while (write_size > 0) {
		if (initial_size && write_size > initial_size) {
			size = initial_size;
			initial_size = 0;
		} else if (initial_size && write_size <= initial_size) {
			size = write_size;
			initial_size = 0;
		} else if (write_size > ULP_BYTE_2_BITS(sizeof(u64))) {
			size = ULP_BYTE_2_BITS(sizeof(u64));
		} else {
			size = write_size;
		}
		if (ulp_blob_push(blob, val, size))
			return -EINVAL;

		val += ULP_BITS_2_BYTE(size);
		write_size -= size;
	}
	return 0;
}

/**
 * Adds pad to an initialized blob at the current offset
 *
 * @blob: The blob that data is added to.  The blob must
 * be initialized prior to pushing data.
 *
 * @datalen: The number of bits of pad to add
 *
 * returns the number of pad bits added, -1 on failure
 */
int
ulp_blob_pad_push(struct ulp_blob *blob,
		  u32 datalen)
{
	if (datalen > (u32)(blob->bitlen - blob->write_idx))
		return -EINVAL;

	blob->write_idx += datalen;
	return 0;
}

/**
 * Adds pad to an initialized blob at the current offset based on
 * the alignment.
 *
 * @blob: The blob that needs to be aligned
 *
 * @align: Alignment in bits.
 *
 * returns the number of pad bits added, -1 on failure
 */
int
ulp_blob_pad_align(struct ulp_blob *blob,
		   u32 align)
{
	int pad = 0;

	pad = ALIGN(blob->write_idx, align) - blob->write_idx;
	if (pad > (int)(blob->bitlen - blob->write_idx))
		return -EINVAL;

	blob->write_idx += pad;
	return pad;
}

/* Get data from src and put into dst using little-endian format */
static void
ulp_bs_get_lsb(u8 *src, u16 bitpos, u8 bitlen, u8 *dst)
{
	u16 index = ULP_BITS_2_BYTE_NR(bitpos);
	u8 bitoffs = bitpos % ULP_BLOB_BYTE;
	u8 mask, partial, shift;

	shift = bitoffs;
	partial = ULP_BLOB_BYTE - bitoffs;
	if (bitoffs + bitlen <= ULP_BLOB_BYTE) {
		mask = ((1 << bitlen) - 1) << shift;
		*dst = (src[index] & mask) >> shift;
	} else {
		mask = ((1 << partial) - 1) << shift;
		*dst = (src[index] & mask) >> shift;
		index++;
		partial = bitlen - partial;
		mask = ((1 << partial) - 1);
		*dst |= (src[index] & mask) << (ULP_BLOB_BYTE - bitoffs);
	}
}

/**
 * Get data from the byte array in Little endian format.
 *
 * @src: The byte array where data is extracted from
 *
 * @dst: The byte array where data is pulled into
 *
 * @size: The size of dst array in bytes
 *
 * @offset: The offset where data is pulled
 *
 * @len: The number of bits to be extracted from the data array
 *
 * returns None.
 */
void
ulp_bs_pull_lsb(u8 *src, u8 *dst, u32 size,
		u32 offset, u32 len)
{
	u32 cnt = ULP_BITS_2_BYTE_NR(len);
	u32 idx;

	/* iterate bytewise to get data */
	for (idx = 0; idx < cnt; idx++) {
		ulp_bs_get_lsb(src, offset, ULP_BLOB_BYTE,
			       &dst[size - 1 - idx]);
		offset += ULP_BLOB_BYTE;
		len -= ULP_BLOB_BYTE;
	}

	/* Extract the last reminder data that is not 8 byte boundary */
	if (len)
		ulp_bs_get_lsb(src, offset, len, &dst[size - 1 - idx]);
}

/* Get data from src and put into dst using big-endian format */
static void
ulp_bs_get_msb(u8 *src, u16 bitpos, u8 bitlen, u8 *dst)
{
	u16 index = ULP_BITS_2_BYTE_NR(bitpos);
	u8 bitoffs = bitpos % ULP_BLOB_BYTE;
	int shift;
	u8 mask;

	shift = ULP_BLOB_BYTE - bitoffs - bitlen;
	if (shift >= 0) {
		mask = 0xFF >> -bitlen;
		*dst = (src[index] >> shift) & mask;
	} else {
		*dst = (src[index] & (0xFF >> bitoffs)) << -shift;
		*dst |= src[index + 1] >> -shift;
	}
}

/**
 * Get data from the byte array in Big endian format.
 *
 * @src: The byte array where data is extracted from
 *
 * @dst: The byte array where data is pulled into
 *
 * @offset: The offset where data is pulled
 *
 * @len: The number of bits to be extracted from the data array
 *
 * returns None.
 */
void
ulp_bs_pull_msb(u8 *src, u8 *dst,
		u32 offset, u32 len)
{
	u32 cnt = ULP_BITS_2_BYTE_NR(len);
	u32 idx;

	/* iterate bytewise to get data */
	for (idx = 0; idx < cnt; idx++) {
		ulp_bs_get_msb(src, offset, ULP_BLOB_BYTE, &dst[idx]);
		offset += ULP_BLOB_BYTE;
		len -= ULP_BLOB_BYTE;
	}

	/* Extract the last reminder data that is not 8 byte boundary */
	if (len)
		ulp_bs_get_msb(src, offset, len, &dst[idx]);
}

/**
 * Extract data from the binary blob using given offset.
 *
 * @blob: The blob that data is extracted from. The blob must
 * be initialized prior to pulling data.
 *
 * @data: A pointer to put the data.
 * @data_size: size of the data buffer in bytes.
 *@offset: - Offset in the blob to extract the data in bits format.
 * @len: The number of bits to be pulled from the blob.
 *
 * Output: zero on success, -1 on failure
 */
int
ulp_blob_pull(struct ulp_blob *blob, u8 *data, u32 data_size,
	      u16 offset, u16 len)
{
	/* validate the arguments */
	if (!blob || (offset + len) > blob->bitlen ||
	    ULP_BYTE_2_BITS(data_size) < len)
		return -EINVAL;

	if (blob->byte_order == BNXT_ULP_BYTE_ORDER_BE)
		ulp_bs_pull_msb(blob->data, data, offset, len);
	else
		ulp_bs_pull_lsb(blob->data, data, data_size, offset, len);
	return 0;
}

/**
 * Get the data portion of the binary blob.
 *
 * @blob: The blob's data to be retrieved. The blob must be
 * initialized prior to pushing data.
 *
 * @datalen: The number of bits to that are filled.
 *
 * returns a byte array of the blob data.  Returns NULL on error.
 */
u8 *
ulp_blob_data_get(struct ulp_blob *blob,
		  u16 *datalen)
{
	/* validate the arguments */
	if (!blob)
		return NULL; /* failure */

	*datalen = blob->write_idx;
	return blob->data;
}

/**
 * Get the data length of the binary blob.
 *
 * @blob: The blob's data len to be retrieved.
 *
 * returns length of the binary blob
 */
int
ulp_blob_data_len_get(struct ulp_blob *blob)
{
	/* validate the arguments */
	if (!blob)
		return -EINVAL;

	return blob->write_idx;
}

/**
 * Set the encap swap start index of the binary blob.
 *
 * @blob: The blob's data to be retrieved. The blob must be
 * initialized prior to pushing data.
 *
 * returns void.
 */
void
ulp_blob_encap_swap_idx_set(struct ulp_blob *blob)
{
	/* validate the arguments */
	if (!blob)
		return; /* failure */

	blob->encap_swap_idx = blob->write_idx;
}

/**
 * Perform the encap buffer swap to 64 bit reversal.
 *
 * @blob: The blob's data to be used for swap.
 *
 * returns void.
 */
void
ulp_blob_perform_encap_swap(struct ulp_blob *blob)
{
	u32 i, idx = 0, end_idx = 0, roundoff;
	u8 temp_val_1, temp_val_2;

	/* validate the arguments */
	if (!blob)
		return; /* failure */

	idx = ULP_BITS_2_BYTE_NR(blob->encap_swap_idx);
	end_idx = ULP_BITS_2_BYTE(blob->write_idx);
	roundoff = ULP_BYTE_2_BITS(ULP_BITS_2_BYTE(end_idx));
	if (roundoff > end_idx) {
		blob->write_idx += ULP_BYTE_2_BITS(roundoff - end_idx);
		end_idx = roundoff;
	}
	while (idx <= end_idx) {
		for (i = 0; i < 4; i = i + 2) {
			temp_val_1 = blob->data[idx + i];
			temp_val_2 = blob->data[idx + i + 1];
			blob->data[idx + i] = blob->data[idx + 6 - i];
			blob->data[idx + i + 1] = blob->data[idx + 7 - i];
			blob->data[idx + 7 - i] = temp_val_2;
			blob->data[idx + 6 - i] = temp_val_1;
		}
		idx += 8;
	}
}

/**
 * Perform the blob buffer reversal byte wise.
 * This api makes the first byte the last and
 * vice-versa.
 *
 * @blob: The blob's data to be used for swap.
 * @chunk_size:the swap is done within the chunk in bytes
 *
 * returns void.
 */
void
ulp_blob_perform_byte_reverse(struct ulp_blob *blob,
			      u32 chunk_size)
{
	u32 idx = 0, jdx = 0, num = 0;
	u8 xchar;
	u8 *buff;

	/* validate the arguments */
	if (!blob)
		return; /* failure */

	buff = blob->data;
	num = ULP_BITS_2_BYTE(blob->write_idx) / chunk_size;
	for (idx = 0; idx < num; idx++) {
		for (jdx = 0; jdx < chunk_size / 2; jdx++) {
			xchar = buff[jdx];
			buff[jdx] = buff[(chunk_size - 1) - jdx];
			buff[(chunk_size - 1) - jdx] = xchar;
		}
		buff += chunk_size;
	}
}

/**
 * Perform the blob buffer 64 bit word swap.
 * This api makes the first 4 bytes the last in
 * a given 64 bit value and vice-versa.
 *
 * @blob: The blob's data to be used for swap.
 *
 * returns void.
 */
void
ulp_blob_perform_64B_word_swap(struct ulp_blob *blob)
{
	u32 word_size = ULP_64B_IN_BYTES / 2;
	u32 i, j, num;
	u8 xchar;

	/* validate the arguments */
	if (!blob)
		return; /* failure */

	num = ULP_BITS_2_BYTE(blob->write_idx);
	for (i = 0; i < num; i = i + ULP_64B_IN_BYTES) {
		for (j = 0; j < word_size; j++) {
			xchar = blob->data[i + j];
			blob->data[i + j] = blob->data[i + j + word_size];
			blob->data[i + j + word_size] = xchar;
		}
	}
}

/**
 * Perform the blob buffer 64 bit byte swap.
 * This api makes the first byte the last in
 * a given 64 bit value and vice-versa.
 *
 * @blob: The blob's data to be used for swap.
 *
 * returns void.
 */
void
ulp_blob_perform_64B_byte_swap(struct ulp_blob *blob)
{
	u32 offset = ULP_64B_IN_BYTES - 1;
	u32 i, j, num;
	u8 xchar;

	/* validate the arguments */
	if (!blob)
		return; /* failure */

	num = ULP_BITS_2_BYTE(blob->write_idx);
	for (i = 0; i < num; i = i + ULP_64B_IN_BYTES) {
		for (j = 0; j < (ULP_64B_IN_BYTES / 2); j++) {
			xchar = blob->data[i + j];
			blob->data[i + j] = blob->data[i + offset - j];
			blob->data[i + offset - j] = xchar;
		}
	}
}

static int
ulp_blob_msb_block_merge(struct ulp_blob *dst, struct ulp_blob *src,
			 u32 block_size, u32 pad)
{
	u32 i, k, write_bytes, remaining;
	u8 *src_buf;
	u16 num = 0;
	u8 bluff;

	src_buf = ulp_blob_data_get(src, &num);

	for (i = 0; i < num;) {
		if (((dst->write_idx % block_size)  + (num - i)) > block_size)
			write_bytes = block_size -
				(dst->write_idx % block_size);
		else
			write_bytes = num - i;
		for (k = 0; k < ULP_BITS_2_BYTE_NR(write_bytes); k++) {
			ulp_bs_put_msb(dst->data, dst->write_idx, ULP_BLOB_BYTE,
				       *src_buf);
			dst->write_idx += ULP_BLOB_BYTE;
			src_buf++;
		}
		remaining = write_bytes % ULP_BLOB_BYTE;
		if (remaining) {
			bluff = (*src_buf) & ((u8)-1 <<
					      (ULP_BLOB_BYTE - remaining));
			ulp_bs_put_msb(dst->data, dst->write_idx,
				       ULP_BLOB_BYTE, bluff);
			dst->write_idx += remaining;
		}
		if (write_bytes != (num - i)) {
			/* add the padding */
			ulp_blob_pad_push(dst, pad);
			if (remaining) {
				ulp_bs_put_msb(dst->data, dst->write_idx,
					       ULP_BLOB_BYTE - remaining,
					       *src_buf);
				dst->write_idx += ULP_BLOB_BYTE - remaining;
				src_buf++;
			}
		}
		i += write_bytes;
	}
	return 0;
}

/**
 * Perform the blob buffer merge.
 * This api makes the src blob merged to the dst blob.
 * The block size and pad size help in padding the dst blob
 *
 * @dst: The destination blob, the blob to be merged.
 * @src: The src blob.
 * @block_size: The size of the block after which padding gets applied.
 * @pad: The size of the pad to be applied.
 *
 * returns 0 on success.
 */
int
ulp_blob_block_merge(struct ulp_blob *dst, struct ulp_blob *src,
		     u32 block_size, u32 pad)
{
	if (dst->byte_order == BNXT_ULP_BYTE_ORDER_BE &&
	    src->byte_order == BNXT_ULP_BYTE_ORDER_BE)
		return ulp_blob_msb_block_merge(dst, src, block_size, pad);

	return -EINVAL;
}

int
ulp_blob_append(struct ulp_blob *dst, struct ulp_blob *src,
		u16 src_offset, u16 src_len)
{
	u32 k, remaining;
	u8 *src_buf;
	u16 num = 0;
	u8 bluff;

	src_buf = ulp_blob_data_get(src, &num);

	if ((src_offset + src_len) > num)
		return -EINVAL;

	/* Only supporting BE for now */
	if (src->byte_order != BNXT_ULP_BYTE_ORDER_BE ||
	    dst->byte_order != BNXT_ULP_BYTE_ORDER_BE)
		return -EINVAL;

	/* Handle if the source offset is not on a byte boundary */
	remaining = src_offset % ULP_BLOB_BYTE;
	if (remaining) {
		bluff = src_buf[src_offset / ULP_BLOB_BYTE] & ((u8)-1 >>
				      (ULP_BLOB_BYTE - remaining));
		ulp_bs_put_msb(dst->data, dst->write_idx,
			       remaining, bluff);
		dst->write_idx += remaining;
		src_offset += remaining;
	}

	src_buf += ULP_BITS_2_BYTE_NR(src_offset);

	/* Push the byte aligned pieces */
	for (k = 0; k < ULP_BITS_2_BYTE_NR(src_len); k++) {
		ulp_bs_put_msb(dst->data, dst->write_idx, ULP_BLOB_BYTE,
			       *src_buf);
		dst->write_idx += ULP_BLOB_BYTE;
		src_buf++;
	}

	/* Handle the remaining if length is not a byte boundary */
	if (src_len > remaining)
		remaining = (src_len - remaining) % ULP_BLOB_BYTE;
	else
		remaining = 0;
	if (remaining) {
		bluff = (*src_buf) & ((u8)-1 <<
				      (ULP_BLOB_BYTE - remaining));
		ulp_bs_put_msb(dst->data, dst->write_idx,
			       ULP_BLOB_BYTE, bluff);
		dst->write_idx += remaining;
	}

	return 0;
}

/**
 * Perform the blob buffer copy.
 * This api makes the src blob merged to the dst blob.
 *
 * @dst: The destination blob, the blob to be merged.
 * @src: The src blob.
 *
 * returns 0 on success.
 */
int
ulp_blob_buffer_copy(struct ulp_blob *dst, struct ulp_blob *src)
{
	if ((dst->write_idx + src->write_idx) > dst->bitlen)
		return -EINVAL;
	if (ULP_BITS_IS_BYTE_NOT_ALIGNED(dst->write_idx) ||
	    ULP_BITS_IS_BYTE_NOT_ALIGNED(src->write_idx))
		return -EINVAL;
	memcpy(&dst->data[ULP_BITS_2_BYTE_NR(dst->write_idx)],
	       src->data, ULP_BITS_2_BYTE_NR(src->write_idx));
	dst->write_idx += src->write_idx;
	return 0;
}

/**
 * Read data from the operand
 *
 * @operand: A pointer to a 16 Byte operand
 *
 * @val: The variable to copy the operand to
 *
 * @bytes: The number of bytes to read into val
 *
 * returns number of bits read, zero on error
 */
int
ulp_operand_read(u8 *operand,
		 u8 *val,
		 u16 bytes)
{
	/* validate the arguments */
	if (!operand || !val)
		return -EINVAL;

	memcpy(val, operand, bytes);
	return 0;
}

/**
 * Check the buffer is empty
 *
 * @buf: The buffer
 * @size: The size of the buffer
 *
 */
int ulp_buffer_is_empty(const u8 *buf, u32 size)
{
	return buf[0] == 0 && !memcmp(buf, buf + 1, size - 1);
}

/* Function to check if bitmap is zero. */
u32 ulp_bitmap_is_zero(u8 *bitmap, int size)
{
	while (size-- > 0) {
		if (*bitmap != 0)
			return false;
		bitmap++;
	}
	return true;
}

/* Function to check if bitmap is ones. */
u32 ulp_bitmap_is_ones(u8 *bitmap, int size)
{
	while (size-- > 0) {
		if (*bitmap != 0xFF)
			return false;
		bitmap++;
	}
	return true;
}

/* Function to check if bitmap is not zero. */
u32 ulp_bitmap_notzero(const u8 *bitmap, int size)
{
	while (size-- > 0) {
		if (*bitmap != 0)
			return true;
		bitmap++;
	}
	return false;
}

/* returns 0 if input is power of 2 */
int ulp_util_is_power_of_2(u64 x)
{
	if (((x - 1) & x))
		return -1;
	return 0;
}
#endif /* CONFIG_BNXT_FLOWER_OFFLOAD */
