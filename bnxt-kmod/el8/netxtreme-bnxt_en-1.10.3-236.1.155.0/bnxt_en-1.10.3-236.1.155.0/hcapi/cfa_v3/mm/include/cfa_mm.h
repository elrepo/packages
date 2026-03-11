/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_MM_H_
#define _CFA_MM_H_

/**
 *  CFA_MM CFA Memory Manager
 *  A CFA memory manager (Document Control:DCSG00988445) is a object instance
 *  within the CFA service module that is responsible for managing CFA related
 *  memories such as Thor2 CFA backings stores, Thor CFA action SRAM, etc. It
 *  is designed to operate in firmware or as part of the host Truflow stack.
 *  Each manager instance consists of a number of bank databases with each
 *  database managing a pool of CFA memory.
 */

/** CFA Memory Manager database query params structure
 *
 *  Structure of database params
 *  @max_records: [in] Maximum number of CFA records
 *  @max_contig_records: [in] Max contiguous CFA records per Alloc (Must be a power of 2).
 *  @db_size: [out] Memory required for Database
 */
struct cfa_mm_query_parms {
	u32 max_records;
	u32 max_contig_records;
	u32 db_size;
};

/** CFA Memory Manager open parameters
 *
 * Structure to store CFA MM open parameters
 * @db_mem_size: [in] Size of memory allocated for CFA MM database
 * @max_records: [in] Max number of CFA records
 * @max_contig_records: [in] Maximum number of contiguous CFA records
 */
struct cfa_mm_open_parms {
	u32 db_mem_size;
	u32 max_records;
	u16 max_contig_records;
};

/** CFA Memory Manager record alloc  parameters
 *
 * Structure to contain parameters for record alloc
 * @num_contig_records - [in] Number of contiguous CFA records
 * @record_offset: [out] Offset of the first of the records allocated
 * @used_count: [out] Total number of records already allocated
 * @all_used: [out] Flag to indicate if all the records are allocated
 */
struct cfa_mm_alloc_parms {
	u32 num_contig_records;
	u32 record_offset;
	u32 used_count;
	u32 all_used;
};

/** CFA Memory Manager record free  parameters
 *
 * Structure to contain parameters for record free
 * @record_offset: [in] Offset of the first of the records allocated
 * @num_contig_records: [in] Number of contiguous CFA records
 * @used_count: [out] Total number of records already allocated
 */
struct cfa_mm_free_parms {
	u32 record_offset;
	u32 num_contig_records;
	u32 used_count;
};

/** CFA Memory Manager query API
 *
 * This API returns the size of memory required for internal data structures to
 * manage the pool of CFA Records with given parameters.
 *
 * @parms: [in,out] CFA Memory manager query data base parameters.
 *
 * Returns
 *   - (0) if successful.
 *   - (-ERRNO) on failure
 */
int cfa_mm_query(struct cfa_mm_query_parms *parms);

/** CFA Memory Manager open API
 *
 * This API initializes the CFA Memory Manager database
 *
 * @cmm: [in] Pointer to the memory used for the CFA Mmeory Manager Database
 *
 * @parms: [in] CFA Memory manager data base parameters.
 *
 * Returns
 *   - (0) if successful.
 *   - (-ERRNO) on failure
 */
int cfa_mm_open(void *cmm, struct cfa_mm_open_parms *parms);

/** CFA Memory Manager close API
 *
 * This API frees the CFA Memory NManager database
 *
 * @cmm: [in] Pointer to the database memory for the record pool
 *
 * Returns
 *   - (0) if successful.
 *   - (-ERRNO) on failure
 */
int cfa_mm_close(void *cmm);

/** CFA Memory Manager Allocate CFA Records API
 *
 * This API allocates the request number of contiguous CFA Records
 *
 * @cmm: [in] Pointer to the database from which to allocate CFA Records
 *
 * @parms: [in,out] CFA MM alloc records parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-ERRNO) on failure
 */
int cfa_mm_alloc(void *cmm, struct cfa_mm_alloc_parms *parms);

/** CFA MemoryManager Free CFA Records API
 *
 * This API frees the requested number of contiguous CFA Records
 *
 * @cmm: [in] Pointer to the database from which to free CFA Records
 *
 * @parms: [in,out] CFA MM free records parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-ERRNO) on failure
 */
int cfa_mm_free(void *cmm, struct cfa_mm_free_parms *parms);

/** CFA Memory Manager Get Entry Size API
 *
 * This API retrieves the size of an allocated CMM entry.
 *
 * @cmm: [in] Pointer to the database from which to allocate CFA Records
 *
 * @entry_id: [in] Index of the allocated entry.
 *
 * @size: [out] Number of contiguous records in the entry.
 *
 * Returns
 *   - (0) if successful.
 *   - (-ERRNO) on failure
 */
int cfa_mm_entry_size_get(void *cmm, u32 entry_id, u8 *size);

#endif /* _CFA_MM_H_ */
