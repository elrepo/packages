/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_EM_H_
#define _TF_EM_H_

#include "tf_core.h"
#include "tf_session.h"
#include "hcapi_cfa_defs.h"

#define TF_EM_MIN_ENTRIES     BIT(15) /* 32K */
#define TF_EM_MAX_ENTRIES     BIT(27) /* 128M */

#define TF_P4_HW_EM_KEY_MAX_SIZE 52
#define TF_P4_EM_KEY_RECORD_SIZE 64

#define TF_P58_HW_EM_KEY_MAX_SIZE 80

#define TF_EM_MAX_MASK 0x7FFF
#define TF_EM_MAX_ENTRY (128 * 1024 * 1024)

/**
 * Hardware Page sizes supported for EEM:
 *   4K, 8K, 64K, 256K, 1M, 2M, 4M, 1G.
 *
 * Round-down other page sizes to the lower hardware page
 * size supported.
 */
#define TF_EM_PAGE_SIZE_4K 12
#define TF_EM_PAGE_SIZE_8K 13
#define TF_EM_PAGE_SIZE_64K 16
#define TF_EM_PAGE_SIZE_256K 18
#define TF_EM_PAGE_SIZE_1M 20
#define TF_EM_PAGE_SIZE_2M 21
#define TF_EM_PAGE_SIZE_4M 22
#define TF_EM_PAGE_SIZE_1G 30

/* Set page size */
#define BNXT_TF_PAGE_SIZE TF_EM_PAGE_SIZE_2M

#if (BNXT_TF_PAGE_SIZE == TF_EM_PAGE_SIZE_4K)	/** 4K */
#define TF_EM_PAGE_SHIFT TF_EM_PAGE_SIZE_4K
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_4K
#elif (BNXT_TF_PAGE_SIZE == TF_EM_PAGE_SIZE_8K)	/** 8K */
#define TF_EM_PAGE_SHIFT TF_EM_PAGE_SIZE_8K
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_8K
#elif (BNXT_TF_PAGE_SIZE == TF_EM_PAGE_SIZE_64K)	/** 64K */
#define TF_EM_PAGE_SHIFT TF_EM_PAGE_SIZE_64K
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_64K
#elif (BNXT_TF_PAGE_SIZE == TF_EM_PAGE_SIZE_256K)	/** 256K */
#define TF_EM_PAGE_SHIFT TF_EM_PAGE_SIZE_256K
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_256K
#elif (BNXT_TF_PAGE_SIZE == TF_EM_PAGE_SIZE_1M)	/** 1M */
#define TF_EM_PAGE_SHIFT TF_EM_PAGE_SIZE_1M
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_1M
#elif (BNXT_TF_PAGE_SIZE == TF_EM_PAGE_SIZE_2M)	/** 2M */
#define TF_EM_PAGE_SHIFT TF_EM_PAGE_SIZE_2M
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_2M
#elif (BNXT_TF_PAGE_SIZE == TF_EM_PAGE_SIZE_4M)	/** 4M */
#define TF_EM_PAGE_SHIFT TF_EM_PAGE_SIZE_4M
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_4M
#elif (BNXT_TF_PAGE_SIZE == TF_EM_PAGE_SIZE_1G)	/** 1G */
#define TF_EM_PAGE_SHIFT TF_EM_PAGE_SIZE_1G
#define TF_EM_PAGE_SIZE_ENUM HWRM_TF_CTXT_MEM_RGTR_INPUT_PAGE_SIZE_1G
#else
#error "Invalid Page Size specified. Please use a TF_EM_PAGE_SIZE_n define"
#endif

/* System memory always uses 4K pages */
#ifdef TF_USE_SYSTEM_MEM
#define TF_EM_PAGE_SIZE BIT(TF_EM_PAGE_SIZE_4K)
#define TF_EM_PAGE_ALIGNMENT BIT(TF_EM_PAGE_SIZE_4K)
#else
#define TF_EM_PAGE_SIZE	BIT(TF_EM_PAGE_SHIFT)
#define TF_EM_PAGE_ALIGNMENT BIT(TF_EM_PAGE_SHIFT)
#endif

/* Used to build GFID:
 *
 *   15           2  0
 *  +--------------+--+
 *  |   Index      |E |
 *  +--------------+--+
 *
 * E = Entry (bucket index)
 */
#define TF_EM_INTERNAL_INDEX_SHIFT 2
#define TF_EM_INTERNAL_INDEX_MASK 0xFFFC
#define TF_EM_INTERNAL_ENTRY_MASK  0x3

/**
 * EM Entry
 * Each EM entry is 512-bit (64-bytes) but ordered differently to EEM.
 *
 * @hdr:	Header is 8 bytes long
 * @key:	Key is 448 bits - 56 bytes
 */
struct tf_em_64b_entry {
	struct cfa_p4_eem_entry_hdr hdr;
	u8 key[TF_P4_EM_KEY_RECORD_SIZE - sizeof(struct cfa_p4_eem_entry_hdr)];
};

/* EEM Memory Type */
enum tf_mem_type {
	TF_EEM_MEM_TYPE_INVALID,
	TF_EEM_MEM_TYPE_HOST,
	TF_EEM_MEM_TYPE_SYSTEM
};

/**
 * tf_em_cfg_parms definition
 *
 * @num_elements:	Num entries in resource config
 * @cfg:		Resource config
 * @resources:		Session resource allocations
 * @mem_type:		Memory type
 */
struct tf_em_cfg_parms {
	u16				num_elements;
	struct tf_rm_element_cfg	*cfg;
	struct tf_session_resources	*resources;
	enum tf_mem_type		mem_type;
};

/* EM RM database */
struct em_rm_db {
	struct rm_db *em_db[TF_DIR_MAX];
};

/**
 * Insert record in to internal EM table
 *
 * @tfp:	Pointer to TruFlow handle
 * @parms:	Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
int tf_em_insert_int_entry(struct tf *tfp,
			   struct tf_insert_em_entry_parms *parms);

/**
 * Delete record from internal EM table
 *
 * @tfp:	Pointer to TruFlow handle
 * @parms:	Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
int tf_em_delete_int_entry(struct tf *tfp,
			   struct tf_delete_em_entry_parms *parms);

/**
 * Insert record in to internal EM table
 *
 * @tfp:	Pointer to TruFlow handle
 * @parms:	Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
int tf_em_hash_insert_int_entry(struct tf *tfp,
				struct tf_insert_em_entry_parms *parms);

/**
 * Delete record from internal EM table
 *
 * @tfp:	Pointer to TruFlow handle
 * @parms:	Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
int tf_em_hash_delete_int_entry(struct tf *tfp,
				struct tf_delete_em_entry_parms *parms);

/**
 * Move record from internal EM table
 *
 * @tfp:	Pointer to TruFlow handle
 * @parms:	Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
int tf_em_move_int_entry(struct tf *tfp,
			 struct tf_move_em_entry_parms *parms);

/**
 * Bind internal EM device interface
 *
 * @tfp:	Pointer to TruFlow handle
 * @parms:	Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
int tf_em_int_bind(struct tf *tfp, struct tf_em_cfg_parms *parms);

/**
 * Unbind internal EM device interface
 *
 * @tfp:	Pointer to TruFlow handle
 * @parms:	Pointer to input parameters
 *
 * Returns:
 *   0       - Success
 *   -EINVAL - Parameter error
 */
int tf_em_int_unbind(struct tf *tfp);

/**
 * Retrieves the allocated resource info
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_em_get_resc_info(struct tf *tfp, struct tf_em_resource_info *em);

#endif /* _TF_EM_H_ */
