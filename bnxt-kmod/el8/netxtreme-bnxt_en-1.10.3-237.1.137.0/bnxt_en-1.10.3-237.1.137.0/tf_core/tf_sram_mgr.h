/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_SRAM_MGR_H_
#define _TF_SRAM_MGR_H_

#include <linux/types.h>
#include "tf_core.h"
#include "tf_rm.h"

#define TF_SRAM_MGR_BLOCK_SZ_BYTES 64
#define TF_SRAM_MGR_MIN_SLICE_BYTES 8

/**
 * TF slice size.
 *
 * A slice is part of a 64B row
 * Each slice is a multiple of 8B
 */
enum tf_sram_slice_size {
	TF_SRAM_SLICE_SIZE_8B,	/* 8 byte SRAM slice */
	TF_SRAM_SLICE_SIZE_16B,	/* 16 byte SRAM slice */
	TF_SRAM_SLICE_SIZE_32B,	/* 32 byte SRAM slice */
	TF_SRAM_SLICE_SIZE_64B,	/* 64 byte SRAM slice */
	TF_SRAM_SLICE_SIZE_MAX  /* slice limit */
};

/** Initialize the SRAM slice manager
 *
 *  The SRAM slice manager manages slices within 64B rows. Slices are of size
 *  tf_sram_slice_size.  This function provides a handle to the SRAM manager
 *  data.
 *
 *  SRAM manager data may dynamically allocate data upon initialization if
 *  running on the host.
 *
 * @sram_handle:	Pointer to SRAM handle
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 * Returns the handle for the SRAM slice manager
 */
int tf_sram_mgr_bind(void **sram_handle);

/** Uninitialize the SRAM slice manager
 *
 * Frees any dynamically allocated data structures for SRAM slice management.
 *
 * @sram_handle:	Pointer to SRAM handle
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 */
int tf_sram_mgr_unbind(void *sram_handle);

/**
 * tf_sram_mgr_alloc_parms parameter definition
 *
 * @dir:	direction
 * @bank_id:	the SRAM bank to allocate from
 * @slice_size:	the slice size to allocate
 * @sram_slice:	A pointer to be filled with an 8B sram slice offset
 * @rm_db:	RM DB Handle required for RM allocation
 * @tbl_type:	tf table type
 */
struct tf_sram_mgr_alloc_parms {
	enum tf_dir		dir;
	enum tf_sram_bank_id	bank_id;
	enum tf_sram_slice_size	slice_size;
	u16			*sram_offset;
	void			*rm_db;
	enum tf_tbl_type	tbl_type;
};

/**
 * Allocate an SRAM Slice
 *
 * Allocate an SRAM slice from the indicated bank.  If successful an 8B SRAM
 * offset will be returned.  Slices are variable sized.  This may result in
 * a row being allocated from the RM SRAM bank pool if required.
 *
 * @sram_handle:	Pointer to SRAM handle
 * @parms:		Pointer to the SRAM alloc parameters
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 */
int tf_sram_mgr_alloc(void *sram_handle,
		      struct tf_sram_mgr_alloc_parms *parms);

/**
 * tf_sram_mgr_free_parms parameter definition
 *
 * @dir:		direction
 * @bank_id:		the SRAM bank to free to
 * @slice_size:		the slice size to be returned
 * @sram_offset:	the SRAM slice offset (8B) to be returned
 * @rm_db:		RM DB Handle required for RM free
 * @tbl_type:		tf table type
 * @tfp:		A pointer to the tf handle
 */
struct tf_sram_mgr_free_parms {
	enum tf_dir		dir;
	enum tf_sram_bank_id	bank_id;
	enum tf_sram_slice_size	slice_size;
	u16			sram_offset;
	void			*rm_db;
	enum tf_tbl_type	tbl_type;
};

/**
 * Free an SRAM Slice
 *
 * Free an SRAM slice to the indicated bank.  This may result in a 64B row
 * being returned to the RM SRAM bank pool.
 *
 * @sram_handle:	Pointer to SRAM handle
 * @parms:		Pointer to the SRAM free parameters
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 */
int tf_sram_mgr_free(void *sram_handle, struct tf_sram_mgr_free_parms *parms);

/**
 * tf_sram_mgr_dump_parms parameter definition
 *
 * @dir:		direction
 * @bank_id:		the SRAM bank to dump
 * @slice_size:		the slice size to be dumped
 */
struct tf_sram_mgr_dump_parms {
	enum tf_dir		dir;
	enum tf_sram_bank_id	bank_id;
	enum tf_sram_slice_size	slice_size;
};

/**
 * Dump a slice list
 *
 * Dump the slice list given the SRAM bank and the slice size
 *
 * @sram_handle:	Pointer to SRAM handle
 * @parms:		Pointer to the SRAM free parameters
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 */
int tf_sram_mgr_dump(void *sram_handle, struct tf_sram_mgr_dump_parms *parms);

/**
 * tf_sram_mgr_is_allocated_parms parameter definition
 *
 * @dir:		direction
 * @bank_id:		the SRAM bank allocated from
 * @slice_size:		the slice size which was allocated
 * @sram_offset:	the SRAM slice offset to validate
 * @is_allocated:	indication of allocation
 */
struct tf_sram_mgr_is_allocated_parms {
	enum tf_dir		dir;
	enum tf_sram_bank_id	bank_id;
	enum tf_sram_slice_size	slice_size;
	u16			sram_offset;
	bool			*is_allocated;
};

/**
 * Validate an SRAM Slice is allocated
 *
 * Validate whether the SRAM slice is allocated
 *
 * @sram_handle:	Pointer to SRAM handle
 * @parms:		Pointer to the SRAM alloc parameters
 *
 * Returns
 *   - (0) if successful
 *   - (-EINVAL) on failure
 *
 */
int tf_sram_mgr_is_allocated(void *sram_handle,
			     struct tf_sram_mgr_is_allocated_parms *parms);

/* Given the slice size, return a char string */
const char *tf_sram_slice_2_str(enum tf_sram_slice_size slice_size);

/* Given the bank_id, return a char string */
const char *tf_sram_bank_2_str(enum tf_sram_bank_id bank_id);

#endif /* _TF_SRAM_MGR_H_ */
