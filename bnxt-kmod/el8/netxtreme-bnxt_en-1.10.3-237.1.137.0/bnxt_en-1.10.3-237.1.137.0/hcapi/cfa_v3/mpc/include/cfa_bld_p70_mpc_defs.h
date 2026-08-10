/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_BLD_P70_MPC_DEFS_H_
#define _CFA_BLD_P70_MPC_DEFS_H_

/*
 * CFA phase 7.0 Action/Lookup cache option values for various accesses
 * From EAS
 */
#define CACHE_READ_OPTION_NORMAL 0x0
#define CACHE_READ_OPTION_EVICT 0x1
#define CACHE_READ_OPTION_FAST_EVICT 0x2
#define CACHE_READ_OPTION_DEBUG_LINE 0x4
#define CACHE_READ_OPTION_DEBUG_TAG  0x5

/*
 * Cache read and clear command expects the cache option bit 3
 * to be set, failing which the clear is not done.
 */
#define CACHE_READ_CLR_MASK (0x1U << 3)
#define CACHE_READ_CLR_OPTION_NORMAL                                           \
	(CACHE_READ_CLR_MASK | CACHE_READ_OPTION_NORMAL)
#define CACHE_READ_CLR_OPTION_EVICT                                            \
	(CACHE_READ_CLR_MASK | CACHE_READ_OPTION_EVICT)
#define CACHE_READ_CLR_OPTION_FAST_EVICT                                       \
	(CACHE_READ_CLR_MASK | CACHE_READ_OPTION_FAST_EVICT)

#define CACHE_WRITE_OPTION_WRITE_BACK 0x0
#define CACHE_WRITE_OPTION_WRITE_THRU 0x1

#define CACHE_EVICT_OPTION_CLEAN_LINES 0x1
#define CACHE_EVICT_OPTION_CLEAN_FAST_LINES 0x2
#define CACHE_EVICT_OPTION_CLEAN_AND_FAST_LINES 0x3
#define CACHE_EVICT_OPTION_LINE 0x4
#define CACHE_EVICT_OPTION_SCOPE_ADDRESS 0x5

#define CFA_P70_CACHE_LINE_BYTES 32
#define CFA_P70_CACHE_LINE_BITS (CFA_P70_CACHE_LINE_BYTES * BITS_PER_BYTE)

/* EM/action cache access unit size in bytes */
#define MPC_CFA_CACHE_ACCESS_UNIT_SIZE CFA_P70_CACHE_LINE_BYTES

/**
 * READ_CMD: This command reads 1-4 consecutive 32B words from the
 * specified address within a table scope.
 */
#define READ_CMD_OPCODE_READ 0

#define READ_CMD_TABLE_TYPE_ACTION 0
#define READ_CMD_TABLE_TYPE_EM 1

/**
 * WRITE_CMD: This command writes 1-4 consecutive 32B words to the
 * specified address within a table scope.
 */
#define WRITE_CMD_OPCODE_WRITE 1

#define WRITE_CMD_TABLE_TYPE_ACTION 0
#define WRITE_CMD_TABLE_TYPE_EM 1

/**
 * READ_CLR_CMD: This command performs a read-modify-write to the
 * specified 32B address using a 16b mask that specifies up to 16 16b
 * words to clear before writing the data back. It returns the 32B data
 * word read from cache (not the value written after the clear
 * operation).
 */
#define READ_CLR_CMD_OPCODE_READ_CLR 2

#define READ_CLR_CMD_TABLE_TYPE_ACTION 0
#define READ_CLR_CMD_TABLE_TYPE_EM 1

/**
 * INVALIDATE_CMD: This command forces an explicit evict of 1-4
 * consecutive cache lines such that the next time the structure is used
 * it will be re-read from its backing store location.
 */
#define INVALIDATE_CMD_OPCODE_INVALIDATE 5

#define INVALIDATE_CMD_TABLE_TYPE_ACTION 0
#define INVALIDATE_CMD_TABLE_TYPE_EM 1

/**
 * EM_SEARCH_CMD: This command supplies an exact match entry of 1-4 32B
 * words to search for in the exact match table. CFA first computes the
 * hash value of the key in the entry, and determines the static bucket
 * address to search from the hash and the (EM_BUCKETS, EM_SIZE) for
 * TABLE_SCOPE. It then searches that static bucket chain for an entry
 * with a matching key (the LREC in the command entry is ignored). If a
 * matching entry is found, CFA reports OK status in the completion.
 * Otherwise, assuming no errors abort the search before it completes,
 * it reports EM_MISS status.
 */
#define EM_SEARCH_CMD_OPCODE_EM_SEARCH 8

/**
 * EM_INSERT_CMD: This command supplies an exact match entry of 1-4 32B
 * words to insert in the exact match table. CFA first computes the hash
 * value of the key in the entry, and determines the static bucket
 * address to search from the hash and the (EM_BUCKETS, EM_SIZE) for
 * TABLE_SCOPE. It then writes the 1-4 32B words of the exact match
 * entry starting at the TABLE_INDEX location in the command. When the
 * entry write completes, it searches the static bucket chain for an
 * existing entry with a key matching the key in the insert entry (the
 * LREC does not need to match). If a matching entry is found: * If
 * REPLACE=0, the CFA aborts the insert and returns EM_DUPLICATE status.
 * * If REPLACE=1, the CFA overwrites the matching entry with the new
 * entry. REPLACED_ENTRY=1 in the completion in this case to signal that
 * an entry was replaced. The location of the entry is provided in the
 * completion. If no match is found, CFA adds the new entry to the
 * lowest unused entry in the tail bucket. If the current tail bucket is
 * full, this requires adding a new bucket to the tail. Then entry is
 * then inserted at entry number 0. TABLE_INDEX2 provides the address of
 * the new tail bucket, if needed. If set to 0, the insert is aborted
 * and returns EM_ABORT status instead of adding a new bucket to the
 * tail. CHAIN_UPD in the completion indicates whether a new bucket was
 * added (1) or not (0). For locked scopes, if the read of the static
 * bucket gives a locked scope miss error, indicating that the address
 * is not in the cache, the static bucket is assumed empty. In this
 * case, TAI creates a new bucket, setting entry 0 to the new entry
 * fields and initializing all other fields to 0. It writes this new
 * bucket to the static bucket address, which installs it in the cache.
 */
#define EM_INSERT_CMD_OPCODE_EM_INSERT 9

/**
 * EM_DELETE_CMD: This command searches for an exact match entry index
 * in the static bucket chain and deletes it if found. TABLE_INDEX give
 * the entry index to delete and TABLE_INDEX2 gives the static bucket
 * index. If a matching entry is found: * If the matching entry is the
 * last valid entry in the tail bucket, its entry fields (HASH_MSBS and
 * ENTRY_PTR) are set to 0 to delete the entry. * If the matching entry
 * is not the last valid entry in the tail bucket, the entry fields from
 * that last entry are moved to the matching entry, and the fields of
 * that last entry are set to 0. * If any of the previous processing
 * results in the tail bucket not having any valid entries, the tail
 * bucket is the static bucket, the scope is a locked scope, and
 * CHAIN_PTR=0, hardware evicts the static bucket from the cache and the
 * completion signals this case with CHAIN_UPD=1. * If any of the
 * previous processing results in the tail bucket not having any valid
 * entries, and the tail bucket is not the static bucket, the tail
 * bucket is removed from the chain. In this case, the penultimate
 * bucket in the chain becomes the tail bucket. It has CHAIN set to 0 to
 * unlink the tail bucket, and CHAIN_PTR set to that from the original
 * tail bucket to preserve background chaining. The completion signals
 * this case with CHAIN_UPD=1 and returns the index to the bucket
 * removed so that software can de-allocate it. CFA returns OK status if
 * the entry was successfully deleted. Otherwise, it returns EM_MISS
 * status assuming there were no errors that caused processing to be
 * aborted.
 */
#define EM_DELETE_CMD_OPCODE_EM_DELETE 10

/**
 * EM_CHAIN_CMD: This command updates CHAIN_PTR in the tail bucket of a
 * static bucket chain, supplying both the static bucket and the new
 * CHAIN_PTR value. TABLE_INDEX is the new CHAIN_PTR value and
 * TABLE_INDEX2[23:0] is the static bucket. This command provides
 * software a means to update background chaining coherently with other
 * bucket updates. The value of CHAIN is unaffected (stays at 0). For
 * locked scopes, if the static bucket is the tail bucket, it is empty
 * (all of its ENTRY_PTR values are 0), and TABLE_INDEX=0 (the CHAIN_PTR
 * is being set to 0), instead of updating the static bucket it is
 * evicted from the cache. In this case, CHAIN_UPD=1 in the completion.
 */
#define EM_CHAIN_CMD_OPCODE_EM_CHAIN 11

/**
 * READ_CMP: When no errors, teturns 1-4 consecutive 32B words from the
 * TABLE_INDEX within the TABLE_SCOPE specified in the command, writing
 * them to HOST_ADDRESS from the command.
 */
#define READ_CMP_TYPE_MID_PATH_SHORT 30

#define READ_CMP_STATUS_OK 0
#define READ_CMP_STATUS_UNSPRT_ERR 1
#define READ_CMP_STATUS_FMT_ERR 2
#define READ_CMP_STATUS_SCOPE_ERR 3
#define READ_CMP_STATUS_ADDR_ERR 4
#define READ_CMP_STATUS_CACHE_ERR 5

#define READ_CMP_MP_CLIENT_TE_CFA 2
#define READ_CMP_MP_CLIENT_RE_CFA 3

#define READ_CMP_OPCODE_READ 0

#define READ_CMP_TABLE_TYPE_ACTION 0
#define READ_CMP_TABLE_TYPE_EM 1

/**
 * WRITE_CMP: Returns status of the write of 1-4 consecutive 32B words
 * starting at TABLE_INDEX in the table specified by (TABLE_TYPE,
 * TABLE_SCOPE).
 */
#define WRITE_CMP_TYPE_MID_PATH_SHORT 30

#define WRITE_CMP_STATUS_OK 0
#define WRITE_CMP_STATUS_UNSPRT_ERR 1
#define WRITE_CMP_STATUS_FMT_ERR 2
#define WRITE_CMP_STATUS_SCOPE_ERR 3
#define WRITE_CMP_STATUS_ADDR_ERR 4
#define WRITE_CMP_STATUS_CACHE_ERR 5

#define WRITE_CMP_MP_CLIENT_TE_CFA 2
#define WRITE_CMP_MP_CLIENT_RE_CFA 3

#define WRITE_CMP_OPCODE_WRITE 1

#define WRITE_CMP_TABLE_TYPE_ACTION 0
#define WRITE_CMP_TABLE_TYPE_EM 1

/**
 * READ_CLR_CMP: When no errors, returns 1 32B word from TABLE_INDEX in
 * the table specified by (TABLE_TYPE, TABLE_SCOPE). The data returned
 * is the value prior to the clear.
 */
#define READ_CLR_CMP_TYPE_MID_PATH_SHORT 30

#define READ_CLR_CMP_STATUS_OK 0
#define READ_CLR_CMP_STATUS_UNSPRT_ERR 1
#define READ_CLR_CMP_STATUS_FMT_ERR 2
#define READ_CLR_CMP_STATUS_SCOPE_ERR 3
#define READ_CLR_CMP_STATUS_ADDR_ERR 4
#define READ_CLR_CMP_STATUS_CACHE_ERR 5

#define READ_CLR_CMP_MP_CLIENT_TE_CFA 2
#define READ_CLR_CMP_MP_CLIENT_RE_CFA 3

#define READ_CLR_CMP_OPCODE_READ_CLR 2

#define READ_CLR_CMP_TABLE_TYPE_ACTION 0
#define READ_CLR_CMP_TABLE_TYPE_EM 1

/**
 * INVALIDATE_CMP: Returns status for INVALIDATE commands.
 */
#define INVALIDATE_CMP_TYPE_MID_PATH_SHORT 30

#define INVALIDATE_CMP_STATUS_OK 0
#define INVALIDATE_CMP_STATUS_UNSPRT_ERR 1
#define INVALIDATE_CMP_STATUS_FMT_ERR 2
#define INVALIDATE_CMP_STATUS_SCOPE_ERR 3
#define INVALIDATE_CMP_STATUS_ADDR_ERR 4
#define INVALIDATE_CMP_STATUS_CACHE_ERR 5

#define INVALIDATE_CMP_MP_CLIENT_TE_CFA 2
#define INVALIDATE_CMP_MP_CLIENT_RE_CFA 3

#define INVALIDATE_CMP_OPCODE_INVALIDATE 5

#define INVALIDATE_CMP_TABLE_TYPE_ACTION 0
#define INVALIDATE_CMP_TABLE_TYPE_EM 1

/**
 * EM_SEARCH_CMP: For OK status, returns the index of the matching entry
 * found for the EM key supplied in the command. Returns EM_MISS status
 * if no match was found.
 */
#define EM_SEARCH_CMP_TYPE_MID_PATH_LONG 31

#define EM_SEARCH_CMP_STATUS_OK 0
#define EM_SEARCH_CMP_STATUS_UNSPRT_ERR 1
#define EM_SEARCH_CMP_STATUS_FMT_ERR 2
#define EM_SEARCH_CMP_STATUS_SCOPE_ERR 3
#define EM_SEARCH_CMP_STATUS_ADDR_ERR 4
#define EM_SEARCH_CMP_STATUS_CACHE_ERR 5
#define EM_SEARCH_CMP_STATUS_EM_MISS 6

#define EM_SEARCH_CMP_MP_CLIENT_TE_CFA 2
#define EM_SEARCH_CMP_MP_CLIENT_RE_CFA 3

#define EM_SEARCH_CMP_OPCODE_EM_SEARCH 8

/**
 * EM_INSERT_CMP: OK status indicates that the exact match entry from
 * the command was successfully inserted. EM_DUPLICATE status indicates
 * that the insert was aborted because an entry with the same exact
 * match key was found and REPLACE=0 in the command. EM_ABORT status
 * indicates that no duplicate was found, the tail bucket in the chain
 * was full, and TABLE_INDEX2=0. No changes are made to the database in
 * this case. TABLE_INDEX is the starting address at which to insert the
 * exact match entry (from the command). TABLE_INDEX2 is the address at
 * which to insert a new bucket at the tail of the static bucket chain
 * if needed (from the command). CHAIN_UPD=1 if a new bucket was added
 * at this address. TABLE_INDEX3 is the static bucket address for the
 * chain, determined from hashing the exact match entry. Software needs
 * this address and TABLE_INDEX in order to delete the entry using an
 * EM_DELETE command. TABLE_INDEX4 is the index of an entry found that
 * had a matching exact match key to the command entry key. If no
 * matching entry was found, it is set to 0. There are two cases when
 * there is a matching entry, depending on REPLACE from the command: *
 * REPLACE=0: EM_DUPLICATE status is reported and the insert is aborted.
 * Software can use the static bucket address (TABLE_INDEX3[23:0]) and
 * the matching entry (TABLE_INDEX4) in an EM_DELETE command if it
 * wishes to explicity delete the matching entry. * REPLACE=1:
 * REPLACED_ENTRY=1 to signal that the entry at TABLE_INDEX4 was
 * replaced by the insert entry. REPLACED_ENTRY will only be 1 if
 * reporting OK status in this case. Software can de-allocate the entry
 * at TABLE_INDEX4.
 */
#define EM_INSERT_CMP_TYPE_MID_PATH_LONG 31

#define EM_INSERT_CMP_STATUS_OK 0
#define EM_INSERT_CMP_STATUS_UNSPRT_ERR 1
#define EM_INSERT_CMP_STATUS_FMT_ERR 2
#define EM_INSERT_CMP_STATUS_SCOPE_ERR 3
#define EM_INSERT_CMP_STATUS_ADDR_ERR 4
#define EM_INSERT_CMP_STATUS_CACHE_ERR 5
#define EM_INSERT_CMP_STATUS_EM_DUPLICATE 7
#define EM_INSERT_CMP_STATUS_EM_ABORT 9

#define EM_INSERT_CMP_MP_CLIENT_TE_CFA 2
#define EM_INSERT_CMP_MP_CLIENT_RE_CFA 3

#define EM_INSERT_CMP_OPCODE_EM_INSERT 9

/**
 * EM_DELETE_CMP: OK status indicates that an ENTRY_PTR matching
 * TABLE_INDEX was found in the static bucket chain specified and was
 * therefore deleted. EM_MISS status indicates that no match was found.
 * TABLE_INDEX is from the command. It is the index of the entry to
 * delete. TABLE_INDEX2 is from the command. It is the static bucket
 * address. TABLE_INDEX3 is the index of the tail bucket of the static
 * bucket chain prior to processing the command. TABLE_INDEX4 is the
 * index of the tail bucket of the static bucket chain after processing
 * the command. If CHAIN_UPD=1 and TABLE_INDEX4==TABLE_INDEX2, the
 * static bucket was the tail bucket, it became empty after the delete,
 * the scope is a locked scope, and CHAIN_PTR was 0. In this case, the
 * static bucket has been evicted from the cache. Otherwise, if
 * CHAIN_UPD=1, the original tail bucket given by TABLE_INDEX3 was
 * removed from the chain because it went empty. It can therefore be de-
 * allocated.
 */
#define EM_DELETE_CMP_TYPE_MID_PATH_LONG 31

#define EM_DELETE_CMP_STATUS_OK 0
#define EM_DELETE_CMP_STATUS_UNSPRT_ERR 1
#define EM_DELETE_CMP_STATUS_FMT_ERR 2
#define EM_DELETE_CMP_STATUS_SCOPE_ERR 3
#define EM_DELETE_CMP_STATUS_ADDR_ERR 4
#define EM_DELETE_CMP_STATUS_CACHE_ERR 5
#define EM_DELETE_CMP_STATUS_EM_MISS 6

#define EM_DELETE_CMP_MP_CLIENT_TE_CFA 2
#define EM_DELETE_CMP_MP_CLIENT_RE_CFA 3

#define EM_DELETE_CMP_OPCODE_EM_DELETE 10

/**
 * EM_CHAIN_CMP: OK status indicates that the CHAIN_PTR of the tail
 * bucket was successfully updated. TABLE_INDEX is from the command. It
 * is the value of the new CHAIN_PTR. TABLE_INDEX2 is from the command.
 * TABLE_INDEX3 is the index of the tail bucket of the static bucket
 * chain.
 */
#define EM_CHAIN_CMP_TYPE_MID_PATH_LONG 31

#define EM_CHAIN_CMP_STATUS_OK 0
#define EM_CHAIN_CMP_STATUS_UNSPRT_ERR 1
#define EM_CHAIN_CMP_STATUS_FMT_ERR 2
#define EM_CHAIN_CMP_STATUS_SCOPE_ERR 3
#define EM_CHAIN_CMP_STATUS_ADDR_ERR 4
#define EM_CHAIN_CMP_STATUS_CACHE_ERR 5

#define EM_CHAIN_CMP_MP_CLIENT_TE_CFA 2
#define EM_CHAIN_CMP_MP_CLIENT_RE_CFA 3

#define EM_CHAIN_CMP_OPCODE_EM_CHAIN 11

#endif /* _CFA_BLD_P70_MPC_DEFS_H_ */
