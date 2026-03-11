/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */
#ifndef __CFA_P70_MPC_CMDS_H__
#define __CFA_P70_MPC_CMDS_H__

#include "cfa_p70_mpc_common.h"

/*
 * CFA Table Read Command Record:
 *
 * This command reads 1-4 consecutive 32B words from the specified address
 * within a table scope.
 * Offset  31              0
 * 0x0     cache_option    unused(1)       data_size       unused(3)
 * table_scope     unused(4)       table_type      opcode
 * 0x4     unused(6)       table_index
 * 0x8
 * -
 * 0xf     host_address
 *
 *    opcode (Offset:0x0[7:0], Size: 8)
 *       This value selects the format for the mid-path
 *       command for the CFA.
 *       Value   Enum    Enumeration Description
 *          0       READ
 *          This command reads 1-4 consecutive 32B words
 *          from the specified address within a table scope.
 *
 *    table_type (Offset:0x0[11:8], Size: 4)
 *       This value selects the table type to be acted
 *       upon.
 *       Value   Enum    Enumeration Description
 *          0       ACTION
 *          This command acts on the action table of the
 *          specified scope.
 *          1       EM
 *          This command acts on the exact match table of
 *          the specified scope.
 *
 *    table_scope (Offset:0x0[20:16], Size: 5)
 *       Table scope to access.
 *
 *    data_size (Offset:0x0[26:24], Size: 3)
 *       Number of 32B units in access. If value is outside
 *       the range [1, 4], CFA aborts processing and reports
 *       FMT_ERR status.
 *
 *    cache_option (Offset:0x0[31:28], Size: 4)
 *       Determines setting of OPTION field for all cache
 *       requests while processing any command other than
 *       EM_INSERT, EM_DELETE, or EM_CHAIN. For these latter
 *       commands, CACHE_OPTION sets the OPTION field for
 *       all read requests, and CACHE_OPTION2 sets it for
 *       all write requests.
 *       CFA does not support posted write requests.
 *       Therefore, for WRITE commands, CACHE_OPTION[1] must
 *       be set to 0. And for EM commands that send write
 *       requests (all but EM_SEARCH), CACHE_OPTION2[1] must
 *       be set to 0.
 *
 *    table_index (Offset:0x4[25:0], Size: 26)
 *       A 32B index into the table identified by
 *       (TABLE_TYPE, TABLE_SCOPE):
 *
 *    host_address (Offset:0x8[31:0], Size: 32, Words: 2)
 *       The 64-bit host address to which to write the DMA
 *       data returned in the completion. The data will be
 *       written to the same function as the one that owns
 *       the SQ this command is read from. DATA_SIZE
 *       determines the maximum size of the data written. If
 *       HOST_ADDRESS[1:0] is not 0, CFA aborts processing
 *       and reports FMT_ERR status.
 */
#define TFC_MPC_CMD_OPCODE_READ    0

#define TFC_MPC_CMD_TBL_RD_OPCODE_EB           7
#define TFC_MPC_CMD_TBL_RD_OPCODE_SB           0
#define TFC_MPC_CMD_TBL_RD_OPCODE_OFFS         0x0

#define TFC_MPC_CMD_TBL_RD_SET_OPCODE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_OPCODE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RD_OPCODE_EB, \
		     TFC_MPC_CMD_TBL_RD_OPCODE_SB)
#define TFC_MPC_CMD_TBL_RD_GET_OPCODE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_OPCODE_OFFS), \
		     TFC_MPC_CMD_TBL_RD_OPCODE_EB, \
		     TFC_MPC_CMD_TBL_RD_OPCODE_SB)

#define TFC_MPC_CMD_TBL_RD_TABLE_TYPE_ACTION    0
#define TFC_MPC_CMD_TBL_RD_TABLE_TYPE_EM        1

#define TFC_MPC_CMD_TBL_RD_TABLE_TYPE_EB           11
#define TFC_MPC_CMD_TBL_RD_TABLE_TYPE_SB           8
#define TFC_MPC_CMD_TBL_RD_TABLE_TYPE_OFFS         0x0

#define TFC_MPC_CMD_TBL_RD_SET_TABLE_TYPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_TABLE_TYPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RD_TABLE_TYPE_EB, \
		     TFC_MPC_CMD_TBL_RD_TABLE_TYPE_SB)
#define TFC_MPC_CMD_TBL_RD_GET_TABLE_TYPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_TABLE_TYPE_OFFS), \
		     TFC_MPC_CMD_TBL_RD_TABLE_TYPE_EB, \
		     TFC_MPC_CMD_TBL_RD_TABLE_TYPE_SB)

#define TFC_MPC_CMD_TBL_RD_TABLE_SCOPE_EB           20
#define TFC_MPC_CMD_TBL_RD_TABLE_SCOPE_SB           16
#define TFC_MPC_CMD_TBL_RD_TABLE_SCOPE_OFFS         0x0

#define TFC_MPC_CMD_TBL_RD_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_TABLE_SCOPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RD_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_TBL_RD_TABLE_SCOPE_SB)
#define TFC_MPC_CMD_TBL_RD_GET_TABLE_SCOPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_TABLE_SCOPE_OFFS), \
		     TFC_MPC_CMD_TBL_RD_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_TBL_RD_TABLE_SCOPE_SB)

#define TFC_MPC_CMD_TBL_RD_DATA_SIZE_EB           26
#define TFC_MPC_CMD_TBL_RD_DATA_SIZE_SB           24
#define TFC_MPC_CMD_TBL_RD_DATA_SIZE_OFFS         0x0

#define TFC_MPC_CMD_TBL_RD_SET_DATA_SIZE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_DATA_SIZE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RD_DATA_SIZE_EB, \
		     TFC_MPC_CMD_TBL_RD_DATA_SIZE_SB)
#define TFC_MPC_CMD_TBL_RD_GET_DATA_SIZE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_DATA_SIZE_OFFS), \
		     TFC_MPC_CMD_TBL_RD_DATA_SIZE_EB, \
		     TFC_MPC_CMD_TBL_RD_DATA_SIZE_SB)

#define TFC_MPC_CMD_TBL_RD_CACHE_OPTION_EB           31
#define TFC_MPC_CMD_TBL_RD_CACHE_OPTION_SB           28
#define TFC_MPC_CMD_TBL_RD_CACHE_OPTION_OFFS         0x0

#define TFC_MPC_CMD_TBL_RD_SET_CACHE_OPTION(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_CACHE_OPTION_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RD_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_TBL_RD_CACHE_OPTION_SB)
#define TFC_MPC_CMD_TBL_RD_GET_CACHE_OPTION(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_CACHE_OPTION_OFFS), \
		     TFC_MPC_CMD_TBL_RD_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_TBL_RD_CACHE_OPTION_SB)

#define TFC_MPC_CMD_TBL_RD_TABLE_INDEX_EB           25
#define TFC_MPC_CMD_TBL_RD_TABLE_INDEX_SB           0
#define TFC_MPC_CMD_TBL_RD_TABLE_INDEX_OFFS         0x4

#define TFC_MPC_CMD_TBL_RD_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_TABLE_INDEX_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RD_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_TBL_RD_TABLE_INDEX_SB)
#define TFC_MPC_CMD_TBL_RD_GET_TABLE_INDEX(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_TABLE_INDEX_OFFS), \
		     TFC_MPC_CMD_TBL_RD_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_TBL_RD_TABLE_INDEX_SB)

#define TFC_MPC_CMD_TBL_RD_HOST_ADDRESS_0_OFFS         0x8

#define TFC_MPC_CMD_TBL_RD_SET_HOST_ADDRESS_0(buf, val) \
	SET_FLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_HOST_ADDRESS_0_OFFS), (u32)(val))
#define TFC_MPC_CMD_TBL_RD_GET_HOST_ADDRESS_0(buf) \
	GET_FLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_HOST_ADDRESS_0_OFFS))

#define TFC_MPC_CMD_TBL_RD_HOST_ADDRESS_1_OFFS         0xc

#define TFC_MPC_CMD_TBL_RD_SET_HOST_ADDRESS_1(buf, val) \
	SET_FLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_HOST_ADDRESS_1_OFFS), (u32)(val))
#define TFC_MPC_CMD_TBL_RD_GET_HOST_ADDRESS_1(buf) \
	GET_FLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RD_HOST_ADDRESS_1_OFFS))

#define TFC_MPC_CMD_TBL_RD_SIZE     16

/*
 * CFA Table Write Command Record:
 *
 * This command writes 1-4 consecutive 32B words to the specified address
 * within a table scope.
 * Offset  31              0
 * 0x0     cache_option    unused(1)       data_size       unused(3)
 * table_scope     unused(3)       write_through   table_type      opcode
 * 0x4     unused(6)       table_index
 * 0x8
 * -
 * 0xf     unused(64)
 *
 *    opcode (Offset:0x0[7:0], Size: 8)
 *       This value selects the format for the mid-path
 *       command for the CFA.
 *       Value   Enum    Enumeration Description
 *          1       WRITE
 *          This command writes 1-4 consecutive 32B words
 *          to the specified address within a table scope.
 *
 *    table_type (Offset:0x0[11:8], Size: 4)
 *       This value selects the table type to be acted
 *       upon.
 *       Value   Enum    Enumeration Description
 *          0       ACTION
 *          This command acts on the action table of the
 *          specified scope.
 *          1       EM
 *          This command acts on the exact match table of
 *          the specified scope.
 *
 *    write_through (Offset:0x0[12], Size: 1)
 *       Sets the OPTION field on the cache interface to
 *       use write-through for EM entry writes while
 *       processing EM_INSERT commands. For all other cases
 *       (inluding EM_INSERT bucket writes), the OPTION
 *       field is set by the CACHE_OPTION and CACHE_OPTION2
 *       fields.
 *
 *    table_scope (Offset:0x0[20:16], Size: 5)
 *       Table scope to access.
 *
 *    data_size (Offset:0x0[26:24], Size: 3)
 *       Number of 32B units in access. If value is outside
 *       the range [1, 4], CFA aborts processing and reports
 *       FMT_ERR status.
 *
 *    cache_option (Offset:0x0[31:28], Size: 4)
 *       Determines setting of OPTION field for all cache
 *       requests while processing any command other than
 *       EM_INSERT, EM_DELETE, or EM_CHAIN. For these latter
 *       commands, CACHE_OPTION sets the OPTION field for
 *       all read requests, and CACHE_OPTION2 sets it for
 *       all write requests.
 *       CFA does not support posted write requests.
 *       Therefore, for WRITE commands, CACHE_OPTION[1] must
 *       be set to 0. And for EM commands that send write
 *       requests (all but EM_SEARCH), CACHE_OPTION2[1] must
 *       be set to 0.
 *
 *    table_index (Offset:0x4[25:0], Size: 26)
 *       A 32B index into the table identified by
 *       (TABLE_TYPE, TABLE_SCOPE):
 */
#define TFC_MPC_CMD_OPCODE_WRITE    1

#define TFC_MPC_CMD_TBL_WR_OPCODE_EB           7
#define TFC_MPC_CMD_TBL_WR_OPCODE_SB           0
#define TFC_MPC_CMD_TBL_WR_OPCODE_OFFS         0x0

#define TFC_MPC_CMD_TBL_WR_SET_OPCODE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_OPCODE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_WR_OPCODE_EB, \
		     TFC_MPC_CMD_TBL_WR_OPCODE_SB)
#define TFC_MPC_CMD_TBL_WR_GET_OPCODE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_OPCODE_OFFS), \
		     TFC_MPC_CMD_TBL_WR_OPCODE_EB, \
		     TFC_MPC_CMD_TBL_WR_OPCODE_SB)

#define TFC_MPC_CMD_TBL_WR_TABLE_TYPE_ACTION    0
#define TFC_MPC_CMD_TBL_WR_TABLE_TYPE_EM        1

#define TFC_MPC_CMD_TBL_WR_TABLE_TYPE_EB           11
#define TFC_MPC_CMD_TBL_WR_TABLE_TYPE_SB           8
#define TFC_MPC_CMD_TBL_WR_TABLE_TYPE_OFFS         0x0

#define TFC_MPC_CMD_TBL_WR_SET_TABLE_TYPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_TABLE_TYPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_WR_TABLE_TYPE_EB, \
		     TFC_MPC_CMD_TBL_WR_TABLE_TYPE_SB)
#define TFC_MPC_CMD_TBL_WR_GET_TABLE_TYPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_TABLE_TYPE_OFFS), \
		     TFC_MPC_CMD_TBL_WR_TABLE_TYPE_EB, \
		     TFC_MPC_CMD_TBL_WR_TABLE_TYPE_SB)

#define TFC_MPC_CMD_TBL_WR_WRITE_THROUGH_EB           12
#define TFC_MPC_CMD_TBL_WR_WRITE_THROUGH_SB           12
#define TFC_MPC_CMD_TBL_WR_WRITE_THROUGH_OFFS         0x0

#define TFC_MPC_CMD_TBL_WR_SET_WRITE_THROUGH(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_WRITE_THROUGH_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_WR_WRITE_THROUGH_EB, \
		     TFC_MPC_CMD_TBL_WR_WRITE_THROUGH_SB)
#define TFC_MPC_CMD_TBL_WR_GET_WRITE_THROUGH(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_WRITE_THROUGH_OFFS), \
		     TFC_MPC_CMD_TBL_WR_WRITE_THROUGH_EB, \
		     TFC_MPC_CMD_TBL_WR_WRITE_THROUGH_SB)

#define TFC_MPC_CMD_TBL_WR_TABLE_SCOPE_EB           20
#define TFC_MPC_CMD_TBL_WR_TABLE_SCOPE_SB           16
#define TFC_MPC_CMD_TBL_WR_TABLE_SCOPE_OFFS         0x0

#define TFC_MPC_CMD_TBL_WR_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_TABLE_SCOPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_WR_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_TBL_WR_TABLE_SCOPE_SB)
#define TFC_MPC_CMD_TBL_WR_GET_TABLE_SCOPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_TABLE_SCOPE_OFFS), \
		     TFC_MPC_CMD_TBL_WR_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_TBL_WR_TABLE_SCOPE_SB)

#define TFC_MPC_CMD_TBL_WR_DATA_SIZE_EB           26
#define TFC_MPC_CMD_TBL_WR_DATA_SIZE_SB           24
#define TFC_MPC_CMD_TBL_WR_DATA_SIZE_OFFS         0x0

#define TFC_MPC_CMD_TBL_WR_SET_DATA_SIZE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_DATA_SIZE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_WR_DATA_SIZE_EB, \
		     TFC_MPC_CMD_TBL_WR_DATA_SIZE_SB)
#define TFC_MPC_CMD_TBL_WR_GET_DATA_SIZE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_DATA_SIZE_OFFS), \
		     TFC_MPC_CMD_TBL_WR_DATA_SIZE_EB, \
		     TFC_MPC_CMD_TBL_WR_DATA_SIZE_SB)

#define TFC_MPC_CMD_TBL_WR_CACHE_OPTION_EB           31
#define TFC_MPC_CMD_TBL_WR_CACHE_OPTION_SB           28
#define TFC_MPC_CMD_TBL_WR_CACHE_OPTION_OFFS         0x0

#define TFC_MPC_CMD_TBL_WR_SET_CACHE_OPTION(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_CACHE_OPTION_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_WR_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_TBL_WR_CACHE_OPTION_SB)
#define TFC_MPC_CMD_TBL_WR_GET_CACHE_OPTION(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_CACHE_OPTION_OFFS), \
		     TFC_MPC_CMD_TBL_WR_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_TBL_WR_CACHE_OPTION_SB)

#define TFC_MPC_CMD_TBL_WR_TABLE_INDEX_EB           25
#define TFC_MPC_CMD_TBL_WR_TABLE_INDEX_SB           0
#define TFC_MPC_CMD_TBL_WR_TABLE_INDEX_OFFS         0x4

#define TFC_MPC_CMD_TBL_WR_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_TABLE_INDEX_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_WR_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_TBL_WR_TABLE_INDEX_SB)
#define TFC_MPC_CMD_TBL_WR_GET_TABLE_INDEX(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_WR_TABLE_INDEX_OFFS), \
		     TFC_MPC_CMD_TBL_WR_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_TBL_WR_TABLE_INDEX_SB)

#define TFC_MPC_CMD_TBL_WR_SIZE     16

/*
 * CFA Table Read-Clear Command Record:
 *
 * This command performs a read-modify-write to the specified 32B address using
 * a 16b mask that specifies up to 16 16b words to clear before writing the data
 * back. It returns the 32B data word read from cache (not the value written
 * after the clear operation).
 * Offset  31              0
 * 0x0     cache_option    unused(1)       data_size       unused(3)
 * table_scope     unused(4)       table_type      opcode
 * 0x4     unused(6)       table_index
 * 0x8
 * -
 * 0xf     host_address
 * 0x10    unused(16)      clear_mask
 *
 *    opcode (Offset:0x0[7:0], Size: 8)
 *       This value selects the format for the mid-path
 *       command for the CFA.
 *       Value   Enum    Enumeration Description
 *          2       READ_CLR
 *          This command performs a read-modify-write to
 *          the specified 32B address using a 16b mask that
 *          specifies up to 16 16b words to clear. It
 *          returns the 32B data word prior to the clear
 *          operation.
 *
 *    table_type (Offset:0x0[11:8], Size: 4)
 *       This value selects the table type to be acted
 *       upon.
 *       Value   Enum    Enumeration Description
 *          0       ACTION
 *          This command acts on the action table of the
 *          specified scope.
 *          1       EM
 *          This command acts on the exact match table of
 *          the specified scope.
 *
 *    table_scope (Offset:0x0[20:16], Size: 5)
 *       Table scope to access.
 *
 *    data_size (Offset:0x0[26:24], Size: 3)
 *       This field is no longer used. The READ_CLR command
 *       always reads (and does a mask-clear) on a single
 *       cache line.
 *       This field was added for SR2 A0 to avoid an
 *       ADDR_ERR when TABLE_INDEX=0 and TABLE_TYPE=EM (see
 *       CUMULUS-17872). That issue was fixed in SR2 B0.
 *
 *    cache_option (Offset:0x0[31:28], Size: 4)
 *       Determines setting of OPTION field for all cache
 *       requests while processing any command other than
 *       EM_INSERT, EM_DELETE, or EM_CHAIN. For these latter
 *       commands, CACHE_OPTION sets the OPTION field for
 *       all read requests, and CACHE_OPTION2 sets it for
 *       all write requests.
 *       CFA does not support posted write requests.
 *       Therefore, for WRITE commands, CACHE_OPTION[1] must
 *       be set to 0. And for EM commands that send write
 *       requests (all but EM_SEARCH), CACHE_OPTION2[1] must
 *       be set to 0.
 *
 *    table_index (Offset:0x4[25:0], Size: 26)
 *       A 32B index into the table identified by
 *       (TABLE_TYPE, TABLE_SCOPE):
 *
 *    host_address (Offset:0x8[31:0], Size: 32, Words: 2)
 *       The 64-bit host address to which to write the DMA
 *       data returned in the completion. The data will be
 *       written to the same function as the one that owns
 *       the SQ this command is read from. DATA_SIZE
 *       determines the maximum size of the data written. If
 *       HOST_ADDRESS[1:0] is not 0, CFA aborts processing
 *       and reports FMT_ERR status.
 *
 *    clear_mask (Offset:0x10[15:0], Size: 16)
 *       Specifies bits in 32B data word to clear. For
 *       x=0..15, when clear_mask[x]=1, data[x*16+15:x*16]
 *       is set to 0.
 */
#define TFC_MPC_CMD_OPCODE_READ_CLR    2

#define TFC_MPC_CMD_TBL_RDCLR_OPCODE_EB           7
#define TFC_MPC_CMD_TBL_RDCLR_OPCODE_SB           0
#define TFC_MPC_CMD_TBL_RDCLR_OPCODE_OFFS         0x0

#define TFC_MPC_CMD_TBL_RDCLR_SET_OPCODE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_OPCODE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RDCLR_OPCODE_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_OPCODE_SB)
#define TFC_MPC_CMD_TBL_RDCLR_GET_OPCODE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_OPCODE_OFFS), \
		     TFC_MPC_CMD_TBL_RDCLR_OPCODE_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_OPCODE_SB)

#define TFC_MPC_CMD_TBL_RDCLR_TABLE_TYPE_ACTION    0
#define TFC_MPC_CMD_TBL_RDCLR_TABLE_TYPE_EM        1

#define TFC_MPC_CMD_TBL_RDCLR_TABLE_TYPE_EB           11
#define TFC_MPC_CMD_TBL_RDCLR_TABLE_TYPE_SB           8
#define TFC_MPC_CMD_TBL_RDCLR_TABLE_TYPE_OFFS         0x0

#define TFC_MPC_CMD_TBL_RDCLR_SET_TABLE_TYPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_TABLE_TYPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RDCLR_TABLE_TYPE_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_TABLE_TYPE_SB)
#define TFC_MPC_CMD_TBL_RDCLR_GET_TABLE_TYPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_TABLE_TYPE_OFFS), \
		     TFC_MPC_CMD_TBL_RDCLR_TABLE_TYPE_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_TABLE_TYPE_SB)

#define TFC_MPC_CMD_TBL_RDCLR_TABLE_SCOPE_EB           20
#define TFC_MPC_CMD_TBL_RDCLR_TABLE_SCOPE_SB           16
#define TFC_MPC_CMD_TBL_RDCLR_TABLE_SCOPE_OFFS         0x0

#define TFC_MPC_CMD_TBL_RDCLR_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_TABLE_SCOPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RDCLR_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_TABLE_SCOPE_SB)
#define TFC_MPC_CMD_TBL_RDCLR_GET_TABLE_SCOPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_TABLE_SCOPE_OFFS), \
		     TFC_MPC_CMD_TBL_RDCLR_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_TABLE_SCOPE_SB)

#define TFC_MPC_CMD_TBL_RDCLR_DATA_SIZE_EB           26
#define TFC_MPC_CMD_TBL_RDCLR_DATA_SIZE_SB           24
#define TFC_MPC_CMD_TBL_RDCLR_DATA_SIZE_OFFS         0x0

#define TFC_MPC_CMD_TBL_RDCLR_SET_DATA_SIZE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_DATA_SIZE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RDCLR_DATA_SIZE_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_DATA_SIZE_SB)
#define TFC_MPC_CMD_TBL_RDCLR_GET_DATA_SIZE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_DATA_SIZE_OFFS), \
		     TFC_MPC_CMD_TBL_RDCLR_DATA_SIZE_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_DATA_SIZE_SB)

#define TFC_MPC_CMD_TBL_RDCLR_CACHE_OPTION_EB           31
#define TFC_MPC_CMD_TBL_RDCLR_CACHE_OPTION_SB           28
#define TFC_MPC_CMD_TBL_RDCLR_CACHE_OPTION_OFFS         0x0

#define TFC_MPC_CMD_TBL_RDCLR_SET_CACHE_OPTION(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_CACHE_OPTION_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RDCLR_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_CACHE_OPTION_SB)
#define TFC_MPC_CMD_TBL_RDCLR_GET_CACHE_OPTION(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_CACHE_OPTION_OFFS), \
		     TFC_MPC_CMD_TBL_RDCLR_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_CACHE_OPTION_SB)

#define TFC_MPC_CMD_TBL_RDCLR_TABLE_INDEX_EB           25
#define TFC_MPC_CMD_TBL_RDCLR_TABLE_INDEX_SB           0
#define TFC_MPC_CMD_TBL_RDCLR_TABLE_INDEX_OFFS         0x4

#define TFC_MPC_CMD_TBL_RDCLR_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_TABLE_INDEX_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RDCLR_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_TABLE_INDEX_SB)
#define TFC_MPC_CMD_TBL_RDCLR_GET_TABLE_INDEX(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_TABLE_INDEX_OFFS), \
		     TFC_MPC_CMD_TBL_RDCLR_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_TABLE_INDEX_SB)

#define TFC_MPC_CMD_TBL_RDCLR_HOST_ADDRESS_0_OFFS         0x8

#define TFC_MPC_CMD_TBL_RDCLR_SET_HOST_ADDRESS_0(buf, val) \
	SET_FLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_HOST_ADDRESS_0_OFFS), (u32)(val))
#define TFC_MPC_CMD_TBL_RDCLR_GET_HOST_ADDRESS_0(buf) \
	GET_FLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_HOST_ADDRESS_0_OFFS))

#define TFC_MPC_CMD_TBL_RDCLR_HOST_ADDRESS_1_OFFS         0xc

#define TFC_MPC_CMD_TBL_RDCLR_SET_HOST_ADDRESS_1(buf, val) \
	SET_FLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_HOST_ADDRESS_1_OFFS), (u32)(val))
#define TFC_MPC_CMD_TBL_RDCLR_GET_HOST_ADDRESS_1(buf) \
	GET_FLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_HOST_ADDRESS_1_OFFS))

#define TFC_MPC_CMD_TBL_RDCLR_CLEAR_MASK_EB           15
#define TFC_MPC_CMD_TBL_RDCLR_CLEAR_MASK_SB           0
#define TFC_MPC_CMD_TBL_RDCLR_CLEAR_MASK_OFFS         0x10

#define TFC_MPC_CMD_TBL_RDCLR_SET_CLEAR_MASK(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_CLEAR_MASK_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_RDCLR_CLEAR_MASK_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_CLEAR_MASK_SB)
#define TFC_MPC_CMD_TBL_RDCLR_GET_CLEAR_MASK(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_RDCLR_CLEAR_MASK_OFFS), \
		     TFC_MPC_CMD_TBL_RDCLR_CLEAR_MASK_EB, \
		     TFC_MPC_CMD_TBL_RDCLR_CLEAR_MASK_SB)

#define TFC_MPC_CMD_TBL_RDCLR_SIZE     20

/*
 * CFA Invalidate Command Record:
 *
 * This command forces an explicit evict of 1-4 consecutive cache lines such
 * that the next time the structure is used it will be re-read from its backing
 * store location.
 * Offset  31              0
 * 0x0     cache_option    unused(1)       data_size       unused(3)
 * table_scope     unused(4)       table_type      opcode
 * 0x4     unused(6)       table_index
 *
 *    opcode (Offset:0x0[7:0], Size: 8)
 *       This value selects the format for the mid-path
 *       command for the CFA.
 *       Value   Enum    Enumeration Description
 *          5       INVALIDATE
 *          This command invalidates 1-4 consecutively-
 *          addressed 32B words in the cache.
 *
 *    table_type (Offset:0x0[11:8], Size: 4)
 *       This value selects the table type to be acted
 *       upon.
 *       Value   Enum    Enumeration Description
 *          0       ACTION
 *          This command acts on the action table of the
 *          specified scope.
 *          1       EM
 *          This command acts on the exact match table of
 *          the specified scope.
 *
 *    table_scope (Offset:0x0[20:16], Size: 5)
 *       Table scope to access.
 *
 *    data_size (Offset:0x0[26:24], Size: 3)
 *       This value identifies the number of cache lines to
 *       invalidate. A FMT_ERR is reported if the value is
 *       not in the range of [1, 4].
 *
 *    cache_option (Offset:0x0[31:28], Size: 4)
 *       Determines setting of OPTION field for all cache
 *       requests while processing any command other than
 *       EM_INSERT, EM_DELETE, or EM_CHAIN. For these latter
 *       commands, CACHE_OPTION sets the OPTION field for
 *       all read requests, and CACHE_OPTION2 sets it for
 *       all write requests.
 *       CFA does not support posted write requests.
 *       Therefore, for WRITE commands, CACHE_OPTION[1] must
 *       be set to 0. And for EM commands that send write
 *       requests (all but EM_SEARCH), CACHE_OPTION2[1] must
 *       be set to 0.
 *
 *    table_index (Offset:0x4[25:0], Size: 26)
 *       A 32B index into the table identified by
 *       (TABLE_TYPE, TABLE_SCOPE):
 */
#define TFC_MPC_CMD_OPCODE_INVALIDATE    5

#define TFC_MPC_CMD_TBL_INV_OPCODE_EB           7
#define TFC_MPC_CMD_TBL_INV_OPCODE_SB           0
#define TFC_MPC_CMD_TBL_INV_OPCODE_OFFS         0x0

#define TFC_MPC_CMD_TBL_INV_SET_OPCODE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_INV_OPCODE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_INV_OPCODE_EB, \
		     TFC_MPC_CMD_TBL_INV_OPCODE_SB)
#define TFC_MPC_CMD_TBL_INV_GET_OPCODE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_INV_OPCODE_OFFS), \
		     TFC_MPC_CMD_TBL_INV_OPCODE_EB, \
		     TFC_MPC_CMD_TBL_INV_OPCODE_SB)

#define TFC_MPC_CMD_TBL_INV_TABLE_TYPE_ACTION    0
#define TFC_MPC_CMD_TBL_INV_TABLE_TYPE_EM        1

#define TFC_MPC_CMD_TBL_INV_TABLE_TYPE_EB           11
#define TFC_MPC_CMD_TBL_INV_TABLE_TYPE_SB           8
#define TFC_MPC_CMD_TBL_INV_TABLE_TYPE_OFFS         0x0

#define TFC_MPC_CMD_TBL_INV_SET_TABLE_TYPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_INV_TABLE_TYPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_INV_TABLE_TYPE_EB, \
		     TFC_MPC_CMD_TBL_INV_TABLE_TYPE_SB)
#define TFC_MPC_CMD_TBL_INV_GET_TABLE_TYPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_INV_TABLE_TYPE_OFFS), \
		     TFC_MPC_CMD_TBL_INV_TABLE_TYPE_EB, \
		     TFC_MPC_CMD_TBL_INV_TABLE_TYPE_SB)

#define TFC_MPC_CMD_TBL_INV_TABLE_SCOPE_EB           20
#define TFC_MPC_CMD_TBL_INV_TABLE_SCOPE_SB           16
#define TFC_MPC_CMD_TBL_INV_TABLE_SCOPE_OFFS         0x0

#define TFC_MPC_CMD_TBL_INV_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_INV_TABLE_SCOPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_INV_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_TBL_INV_TABLE_SCOPE_SB)
#define TFC_MPC_CMD_TBL_INV_GET_TABLE_SCOPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_INV_TABLE_SCOPE_OFFS), \
		     TFC_MPC_CMD_TBL_INV_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_TBL_INV_TABLE_SCOPE_SB)

#define TFC_MPC_CMD_TBL_INV_DATA_SIZE_EB           26
#define TFC_MPC_CMD_TBL_INV_DATA_SIZE_SB           24
#define TFC_MPC_CMD_TBL_INV_DATA_SIZE_OFFS         0x0

#define TFC_MPC_CMD_TBL_INV_SET_DATA_SIZE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_INV_DATA_SIZE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_INV_DATA_SIZE_EB, \
		     TFC_MPC_CMD_TBL_INV_DATA_SIZE_SB)
#define TFC_MPC_CMD_TBL_INV_GET_DATA_SIZE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_INV_DATA_SIZE_OFFS), \
		     TFC_MPC_CMD_TBL_INV_DATA_SIZE_EB, \
		     TFC_MPC_CMD_TBL_INV_DATA_SIZE_SB)

#define TFC_MPC_CMD_TBL_INV_CACHE_OPTION_EB           31
#define TFC_MPC_CMD_TBL_INV_CACHE_OPTION_SB           28
#define TFC_MPC_CMD_TBL_INV_CACHE_OPTION_OFFS         0x0

#define TFC_MPC_CMD_TBL_INV_SET_CACHE_OPTION(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_INV_CACHE_OPTION_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_INV_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_TBL_INV_CACHE_OPTION_SB)
#define TFC_MPC_CMD_TBL_INV_GET_CACHE_OPTION(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_INV_CACHE_OPTION_OFFS), \
		     TFC_MPC_CMD_TBL_INV_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_TBL_INV_CACHE_OPTION_SB)

#define TFC_MPC_CMD_TBL_INV_TABLE_INDEX_EB           25
#define TFC_MPC_CMD_TBL_INV_TABLE_INDEX_SB           0
#define TFC_MPC_CMD_TBL_INV_TABLE_INDEX_OFFS         0x4

#define TFC_MPC_CMD_TBL_INV_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_INV_TABLE_INDEX_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_TBL_INV_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_TBL_INV_TABLE_INDEX_SB)
#define TFC_MPC_CMD_TBL_INV_GET_TABLE_INDEX(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_TBL_INV_TABLE_INDEX_OFFS), \
		     TFC_MPC_CMD_TBL_INV_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_TBL_INV_TABLE_INDEX_SB)

#define TFC_MPC_CMD_TBL_INV_SIZE     8

/*
 * CFA Event Collection Command Record:
 *
 * This command is used to read notification messages from the Host
 * Notification Queue for TABLE_SCOPE in the command.
 * Offset  31              0
 * 0x0     unused(5)       data_size       unused(3)       table_scope
 * unused(8)       opcode
 * 0x4     unused(32)
 * 0x8
 * -
 * 0xf     host_address
 *
 *    opcode (Offset:0x0[7:0], Size: 8)
 *       This value selects the format for the mid-path
 *       command for the CFA.
 *       Value   Enum    Enumeration Description
 *          6       EVENT_COLLECTION
 *          This command reads host notification messages
 *          from the lookup block connection tracking for a
 *          specified table scope. The command can specify
 *          the maximum number of messages returned: 4, 8,
 *          12, or 16. The actual number returned may be
 *          fewer than the maximum depending on the number
 *          queued.
 *
 *    table_scope (Offset:0x0[20:16], Size: 5)
 *       Table scope to access.
 *
 *    data_size (Offset:0x0[26:24], Size: 3)
 *       This value identifies the maximum number of host
 *       notification messages that will be returned:
 *       1 = 32B = up to 4 messages
 *       2 = 64B = up to 8 messages
 *       3 = 96B = up to 12 messages
 *       4 = 128B = up to 16 messages
 *
 *    host_address (Offset:0x8[31:0], Size: 32, Words: 2)
 *       The 64-bit host address to which to write the DMA
 *       data returned in the completion. The data will be
 *       written to the same function as the one that owns
 *       the SQ this command is read from. DATA_SIZE
 *       determines the maximum size of the data written. If
 *       HOST_ADDRESS[1:0] is not 0, CFA aborts processing
 *       and reports FMT_ERR status.
 */
#define TFC_MPC_CMD_OPCODE_EVENT_COLLECTION    6

#define TFC_MPC_CMD_EVT_COLL_OPCODE_EB           7
#define TFC_MPC_CMD_EVT_COLL_OPCODE_SB           0
#define TFC_MPC_CMD_EVT_COLL_OPCODE_OFFS         0x0

#define TFC_MPC_CMD_EVT_COLL_SET_OPCODE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EVT_COLL_OPCODE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EVT_COLL_OPCODE_EB, \
		     TFC_MPC_CMD_EVT_COLL_OPCODE_SB)
#define TFC_MPC_CMD_EVT_COLL_GET_OPCODE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EVT_COLL_OPCODE_OFFS), \
		     TFC_MPC_CMD_EVT_COLL_OPCODE_EB, \
		     TFC_MPC_CMD_EVT_COLL_OPCODE_SB)

#define TFC_MPC_CMD_EVT_COLL_TABLE_SCOPE_EB           20
#define TFC_MPC_CMD_EVT_COLL_TABLE_SCOPE_SB           16
#define TFC_MPC_CMD_EVT_COLL_TABLE_SCOPE_OFFS         0x0

#define TFC_MPC_CMD_EVT_COLL_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EVT_COLL_TABLE_SCOPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EVT_COLL_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_EVT_COLL_TABLE_SCOPE_SB)
#define TFC_MPC_CMD_EVT_COLL_GET_TABLE_SCOPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EVT_COLL_TABLE_SCOPE_OFFS), \
		     TFC_MPC_CMD_EVT_COLL_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_EVT_COLL_TABLE_SCOPE_SB)

#define TFC_MPC_CMD_EVT_COLL_DATA_SIZE_EB           26
#define TFC_MPC_CMD_EVT_COLL_DATA_SIZE_SB           24
#define TFC_MPC_CMD_EVT_COLL_DATA_SIZE_OFFS         0x0

#define TFC_MPC_CMD_EVT_COLL_SET_DATA_SIZE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EVT_COLL_DATA_SIZE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EVT_COLL_DATA_SIZE_EB, \
		     TFC_MPC_CMD_EVT_COLL_DATA_SIZE_SB)
#define TFC_MPC_CMD_EVT_COLL_GET_DATA_SIZE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EVT_COLL_DATA_SIZE_OFFS), \
		     TFC_MPC_CMD_EVT_COLL_DATA_SIZE_EB, \
		     TFC_MPC_CMD_EVT_COLL_DATA_SIZE_SB)

#define TFC_MPC_CMD_EVT_COLL_HOST_ADDRESS_0_OFFS         0x8

#define TFC_MPC_CMD_EVT_COLL_SET_HOST_ADDRESS_0(buf, val) \
	SET_FLD32(TO_P32((buf), TFC_MPC_CMD_EVT_COLL_HOST_ADDRESS_0_OFFS), (u32)(val))
#define TFC_MPC_CMD_EVT_COLL_GET_HOST_ADDRESS_0(buf) \
	GET_FLD32(TO_P32((buf), TFC_MPC_CMD_EVT_COLL_HOST_ADDRESS_0_OFFS))

#define TFC_MPC_CMD_EVT_COLL_HOST_ADDRESS_1_OFFS         0xc

#define TFC_MPC_CMD_EVT_COLL_SET_HOST_ADDRESS_1(buf, val) \
	SET_FLD32(TO_P32((buf), TFC_MPC_CMD_EVT_COLL_HOST_ADDRESS_1_OFFS), (u32)(val))
#define TFC_MPC_CMD_EVT_COLL_GET_HOST_ADDRESS_1(buf) \
	GET_FLD32(TO_P32((buf), TFC_MPC_CMD_EVT_COLL_HOST_ADDRESS_1_OFFS))

#define TFC_MPC_CMD_EVT_COLL_SIZE     16

/*
 * CFA Exact Match Search Command Record:
 *
 * This command supplies an exact match entry of 1-4 32B words to search for in
 * the exact match table. CFA first computes the hash value of the key in the
 * entry, and determines the static bucket address to search from the hash and
 * the (EM_BUCKETS, EM_SIZE) for TABLE_SCOPE.
 * It then searches that static bucket chain for an entry with a matching key
 * (the LREC in the command entry is ignored).
 * If a matching entry is found, CFA reports OK status in the completion.
 * Otherwise, assuming no errors abort the search before it completes, it
 * reports EM_MISS status.
 * Offset  31              0
 * 0x0     cache_option    unused(1)       data_size       unused(3)
 * table_scope     unused(8)       opcode
 * 0x4
 * -
 * 0xf     unused(96)
 *
 *    opcode (Offset:0x0[7:0], Size: 8)
 *       This value selects the format for the mid-path
 *       command for the CFA.
 *       Value   Enum    Enumeration Description
 *          8       EM_SEARCH
 *          This command supplies an exact match entry of
 *          1-4 32B words to search for in the exact match
 *          table.
 *
 *    table_scope (Offset:0x0[20:16], Size: 5)
 *       Table scope to access.
 *
 *    data_size (Offset:0x0[26:24], Size: 3)
 *       Number of 32B units in access. If value is outside
 *       the range [1, 4], CFA aborts processing and reports
 *       FMT_ERR status.
 *
 *    cache_option (Offset:0x0[31:28], Size: 4)
 *       Determines setting of OPTION field for all cache
 *       requests while processing any command other than
 *       EM_INSERT, EM_DELETE, or EM_CHAIN. For these latter
 *       commands, CACHE_OPTION sets the OPTION field for
 *       all read requests, and CACHE_OPTION2 sets it for
 *       all write requests.
 *       CFA does not support posted write requests.
 *       Therefore, for WRITE commands, CACHE_OPTION[1] must
 *       be set to 0. And for EM commands that send write
 *       requests (all but EM_SEARCH), CACHE_OPTION2[1] must
 *       be set to 0.
 */
#define TFC_MPC_CMD_OPCODE_EM_SEARCH    8

#define TFC_MPC_CMD_EM_SEARCH_OPCODE_EB           7
#define TFC_MPC_CMD_EM_SEARCH_OPCODE_SB           0
#define TFC_MPC_CMD_EM_SEARCH_OPCODE_OFFS         0x0

#define TFC_MPC_CMD_EM_SEARCH_SET_OPCODE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_SEARCH_OPCODE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_SEARCH_OPCODE_EB, \
		     TFC_MPC_CMD_EM_SEARCH_OPCODE_SB)
#define TFC_MPC_CMD_EM_SEARCH_GET_OPCODE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_SEARCH_OPCODE_OFFS), \
		     TFC_MPC_CMD_EM_SEARCH_OPCODE_EB, \
		     TFC_MPC_CMD_EM_SEARCH_OPCODE_SB)

#define TFC_MPC_CMD_EM_SEARCH_TABLE_SCOPE_EB           20
#define TFC_MPC_CMD_EM_SEARCH_TABLE_SCOPE_SB           16
#define TFC_MPC_CMD_EM_SEARCH_TABLE_SCOPE_OFFS         0x0

#define TFC_MPC_CMD_EM_SEARCH_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_SEARCH_TABLE_SCOPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_SEARCH_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_EM_SEARCH_TABLE_SCOPE_SB)
#define TFC_MPC_CMD_EM_SEARCH_GET_TABLE_SCOPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_SEARCH_TABLE_SCOPE_OFFS), \
		     TFC_MPC_CMD_EM_SEARCH_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_EM_SEARCH_TABLE_SCOPE_SB)

#define TFC_MPC_CMD_EM_SEARCH_DATA_SIZE_EB           26
#define TFC_MPC_CMD_EM_SEARCH_DATA_SIZE_SB           24
#define TFC_MPC_CMD_EM_SEARCH_DATA_SIZE_OFFS         0x0

#define TFC_MPC_CMD_EM_SEARCH_SET_DATA_SIZE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_SEARCH_DATA_SIZE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_SEARCH_DATA_SIZE_EB, \
		     TFC_MPC_CMD_EM_SEARCH_DATA_SIZE_SB)
#define TFC_MPC_CMD_EM_SEARCH_GET_DATA_SIZE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_SEARCH_DATA_SIZE_OFFS), \
		     TFC_MPC_CMD_EM_SEARCH_DATA_SIZE_EB, \
		     TFC_MPC_CMD_EM_SEARCH_DATA_SIZE_SB)

#define TFC_MPC_CMD_EM_SEARCH_CACHE_OPTION_EB           31
#define TFC_MPC_CMD_EM_SEARCH_CACHE_OPTION_SB           28
#define TFC_MPC_CMD_EM_SEARCH_CACHE_OPTION_OFFS         0x0

#define TFC_MPC_CMD_EM_SEARCH_SET_CACHE_OPTION(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_SEARCH_CACHE_OPTION_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_SEARCH_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_EM_SEARCH_CACHE_OPTION_SB)
#define TFC_MPC_CMD_EM_SEARCH_GET_CACHE_OPTION(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_SEARCH_CACHE_OPTION_OFFS), \
		     TFC_MPC_CMD_EM_SEARCH_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_EM_SEARCH_CACHE_OPTION_SB)

#define TFC_MPC_CMD_EM_SEARCH_SIZE     16

/*
 * CFA Exact Match Insert Command Record:
 *
 * This command supplies an exact match entry of 1-4 32B words to insert in the
 * exact match table. CFA first computes the hash value of the key in the entry,
 * and determines the static bucket address to search from the hash and the
 * (EM_BUCKETS, EM_SIZE) for TABLE_SCOPE.
 * It then writes the 1-4 32B words of the exact match entry starting at the
 * TABLE_INDEX location in the command.
 * When the entry write completes, it searches the static bucket chain for an
 * existing entry with a key matching the key in the insert entry (the LREC does
 * not need to match).
 * If a matching entry is found:
 * If REPLACE=0, the CFA aborts the insert and returns EM_DUPLICATE status.
 * If REPLACE=1, the CFA overwrites the matching entry with the new entry.
 * REPLACED_ENTRY=1 in the completion in this case to signal that an entry was
 * replaced. The location of the entry is provided in the completion.
 * If no match is found, CFA adds the new entry to the lowest unused entry in
 * the tail bucket. If the current tail bucket is full, this requires adding a
 * new bucket to the tail. Then entry is then inserted at entry number 0.
 * TABLE_INDEX2 provides the address of the new tail bucket, if needed. If set
 * to 0, the insert is aborted and returns EM_ABORT status instead of adding a
 * new bucket to the tail.
 * CHAIN_UPD in the completion indicates whether a new bucket was added (1) or
 * not (0).
 * For locked scopes, if the read of the static bucket gives a locked scope
 * miss error, indicating that the address is not in the cache, the static
 * bucket is assumed empty. In this case, TAI creates a new bucket, setting
 * entry 0 to the new entry fields and initializing all other fields to 0. It
 * writes this new bucket to the static bucket address, which installs it in the
 * cache.
 * Offset  31              0
 * 0x0     cache_option    unused(1)       data_size       unused(3)
 * table_scope     unused(3)       write_through   unused(4)       opcode
 * 0x4     cache_option2   unused(2)       table_index
 * 0x8     replace unused(5)       table_index2
 * 0xc     unused(32)
 *
 *    opcode (Offset:0x0[7:0], Size: 8)
 *       This value selects the format for the mid-path
 *       command for the CFA.
 *       Value   Enum    Enumeration Description
 *          9       EM_INSERT
 *          This command supplies an exact match entry of
 *          1-4 32B words to be inserted into the exact
 *          match table.
 *
 *    write_through (Offset:0x0[12], Size: 1)
 *       Sets the OPTION field on the cache interface to
 *       use write-through for EM entry writes while
 *       processing EM_INSERT commands. For all other cases
 *       (inluding EM_INSERT bucket writes), the OPTION
 *       field is set by the CACHE_OPTION and CACHE_OPTION2
 *       fields.
 *
 *    table_scope (Offset:0x0[20:16], Size: 5)
 *       Table scope to access.
 *
 *    data_size (Offset:0x0[26:24], Size: 3)
 *       Number of 32B units in access. If value is outside
 *       the range [1, 4], CFA aborts processing and reports
 *       FMT_ERR status.
 *
 *    cache_option (Offset:0x0[31:28], Size: 4)
 *       Determines setting of OPTION field for all cache
 *       requests while processing any command other than
 *       EM_INSERT, EM_DELETE, or EM_CHAIN. For these latter
 *       commands, CACHE_OPTION sets the OPTION field for
 *       all read requests, and CACHE_OPTION2 sets it for
 *       all write requests.
 *       CFA does not support posted write requests.
 *       Therefore, for WRITE commands, CACHE_OPTION[1] must
 *       be set to 0. And for EM commands that send write
 *       requests (all but EM_SEARCH), CACHE_OPTION2[1] must
 *       be set to 0.
 *
 *    table_index (Offset:0x4[25:0], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       Starting address to write exact match entry being
 *       inserted.
 *
 *    cache_option2 (Offset:0x4[31:28], Size: 4)
 *       Determines setting of OPTION field for all cache
 *       write requests for EM_INSERT, EM_DELETE, and
 *       EM_CHAIN commands.
 *       CFA does not support posted write requests.
 *       Therefore, CACHE_OPTION2[1] must be set to 0.
 *
 *    table_index2 (Offset:0x8[25:0], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       Only used when no duplicate entry is found and the
 *       tail bucket in the chain searched has no unused
 *       entries. In this case, TABLE_INDEX2 provides the
 *       index to the 32B dynamic bucket to add to the tail
 *       of the chain (it is the new tail bucket).
 *       In this case, the CFA first writes TABLE_INDEX2
 *       with a new bucket:
 *       Entry 0 of the bucket sets the HASH_MSBS computed
 *       from the hash and ENTRY_PTR to TABLE_INDEX.
 *       Entries 1-5 of the bucket set HASH_MSBS and
 *       ENTRY_PTR to 0.
 *       CHAIN=0 and CHAIN_PTR is set to CHAIN_PTR from to
 *       original tail bucket to maintain the background
 *       chaining.
 *       CFA then sets CHAIN=1 and CHAIN_PTR=TABLE_INDEX2
 *       in the original tail bucket to link the new bucket
 *       to the chain.
 *       CHAIN_UPD=1 in the completion to signal that the
 *       new bucket at TABLE_INDEX2 was added to the tail of
 *       the chain.
 *
 *    replace (Offset:0x8[31], Size: 1)
 *       Only used if an entry is found whose key matches
 *       the exact match entry key in the command:
 *       REPLACE=0: The insert is aborted and EM_DUPLICATE
 *       status is returned, signaling that the insert
 *       failed. The index of the matching entry that
 *       blocked the insertion is returned in the
 *       completion.
 *       REPLACE=1: The matching entry is replaced with
 *       that from the command (ENTRY_PTR in the bucket is
 *       overwritten with TABLE_INDEX from the command).
 *       HASH_MSBS for the entry number never changes in
 *       this case since it had to match the new entry key
 *       HASH_MSBS to match.
 *       When an entry is replaced, REPLACED_ENTRY=1 in the
 *       completion and the index of the matching entry is
 *       returned in the completion so that software can de-
 *       allocate the entry.
 */
#define TFC_MPC_CMD_OPCODE_EM_INSERT    9

#define TFC_MPC_CMD_EM_INSERT_OPCODE_EB           7
#define TFC_MPC_CMD_EM_INSERT_OPCODE_SB           0
#define TFC_MPC_CMD_EM_INSERT_OPCODE_OFFS         0x0

#define TFC_MPC_CMD_EM_INSERT_SET_OPCODE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_OPCODE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_INSERT_OPCODE_EB, \
		     TFC_MPC_CMD_EM_INSERT_OPCODE_SB)
#define TFC_MPC_CMD_EM_INSERT_GET_OPCODE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_OPCODE_OFFS), \
		     TFC_MPC_CMD_EM_INSERT_OPCODE_EB, \
		     TFC_MPC_CMD_EM_INSERT_OPCODE_SB)

#define TFC_MPC_CMD_EM_INSERT_WRITE_THROUGH_EB           12
#define TFC_MPC_CMD_EM_INSERT_WRITE_THROUGH_SB           12
#define TFC_MPC_CMD_EM_INSERT_WRITE_THROUGH_OFFS         0x0

#define TFC_MPC_CMD_EM_INSERT_SET_WRITE_THROUGH(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_WRITE_THROUGH_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_INSERT_WRITE_THROUGH_EB, \
		     TFC_MPC_CMD_EM_INSERT_WRITE_THROUGH_SB)
#define TFC_MPC_CMD_EM_INSERT_GET_WRITE_THROUGH(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_WRITE_THROUGH_OFFS), \
		     TFC_MPC_CMD_EM_INSERT_WRITE_THROUGH_EB, \
		     TFC_MPC_CMD_EM_INSERT_WRITE_THROUGH_SB)

#define TFC_MPC_CMD_EM_INSERT_TABLE_SCOPE_EB           20
#define TFC_MPC_CMD_EM_INSERT_TABLE_SCOPE_SB           16
#define TFC_MPC_CMD_EM_INSERT_TABLE_SCOPE_OFFS         0x0

#define TFC_MPC_CMD_EM_INSERT_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_TABLE_SCOPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_INSERT_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_EM_INSERT_TABLE_SCOPE_SB)
#define TFC_MPC_CMD_EM_INSERT_GET_TABLE_SCOPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_TABLE_SCOPE_OFFS), \
		     TFC_MPC_CMD_EM_INSERT_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_EM_INSERT_TABLE_SCOPE_SB)

#define TFC_MPC_CMD_EM_INSERT_DATA_SIZE_EB           26
#define TFC_MPC_CMD_EM_INSERT_DATA_SIZE_SB           24
#define TFC_MPC_CMD_EM_INSERT_DATA_SIZE_OFFS         0x0

#define TFC_MPC_CMD_EM_INSERT_SET_DATA_SIZE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_DATA_SIZE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_INSERT_DATA_SIZE_EB, \
		     TFC_MPC_CMD_EM_INSERT_DATA_SIZE_SB)
#define TFC_MPC_CMD_EM_INSERT_GET_DATA_SIZE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_DATA_SIZE_OFFS), \
		     TFC_MPC_CMD_EM_INSERT_DATA_SIZE_EB, \
		     TFC_MPC_CMD_EM_INSERT_DATA_SIZE_SB)

#define TFC_MPC_CMD_EM_INSERT_CACHE_OPTION_EB           31
#define TFC_MPC_CMD_EM_INSERT_CACHE_OPTION_SB           28
#define TFC_MPC_CMD_EM_INSERT_CACHE_OPTION_OFFS         0x0

#define TFC_MPC_CMD_EM_INSERT_SET_CACHE_OPTION(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_CACHE_OPTION_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_INSERT_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_EM_INSERT_CACHE_OPTION_SB)
#define TFC_MPC_CMD_EM_INSERT_GET_CACHE_OPTION(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_CACHE_OPTION_OFFS), \
		     TFC_MPC_CMD_EM_INSERT_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_EM_INSERT_CACHE_OPTION_SB)

#define TFC_MPC_CMD_EM_INSERT_TABLE_INDEX_EB           25
#define TFC_MPC_CMD_EM_INSERT_TABLE_INDEX_SB           0
#define TFC_MPC_CMD_EM_INSERT_TABLE_INDEX_OFFS         0x4

#define TFC_MPC_CMD_EM_INSERT_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_TABLE_INDEX_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_INSERT_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_EM_INSERT_TABLE_INDEX_SB)
#define TFC_MPC_CMD_EM_INSERT_GET_TABLE_INDEX(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_TABLE_INDEX_OFFS), \
		     TFC_MPC_CMD_EM_INSERT_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_EM_INSERT_TABLE_INDEX_SB)

#define TFC_MPC_CMD_EM_INSERT_CACHE_OPTION2_EB           31
#define TFC_MPC_CMD_EM_INSERT_CACHE_OPTION2_SB           28
#define TFC_MPC_CMD_EM_INSERT_CACHE_OPTION2_OFFS         0x4

#define TFC_MPC_CMD_EM_INSERT_SET_CACHE_OPTION2(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_CACHE_OPTION2_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_INSERT_CACHE_OPTION2_EB, \
		     TFC_MPC_CMD_EM_INSERT_CACHE_OPTION2_SB)
#define TFC_MPC_CMD_EM_INSERT_GET_CACHE_OPTION2(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_CACHE_OPTION2_OFFS), \
		     TFC_MPC_CMD_EM_INSERT_CACHE_OPTION2_EB, \
		     TFC_MPC_CMD_EM_INSERT_CACHE_OPTION2_SB)

#define TFC_MPC_CMD_EM_INSERT_TABLE_INDEX2_EB           25
#define TFC_MPC_CMD_EM_INSERT_TABLE_INDEX2_SB           0
#define TFC_MPC_CMD_EM_INSERT_TABLE_INDEX2_OFFS         0x8

#define TFC_MPC_CMD_EM_INSERT_SET_TABLE_INDEX2(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_TABLE_INDEX2_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_INSERT_TABLE_INDEX2_EB, \
		     TFC_MPC_CMD_EM_INSERT_TABLE_INDEX2_SB)
#define TFC_MPC_CMD_EM_INSERT_GET_TABLE_INDEX2(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_TABLE_INDEX2_OFFS), \
		     TFC_MPC_CMD_EM_INSERT_TABLE_INDEX2_EB, \
		     TFC_MPC_CMD_EM_INSERT_TABLE_INDEX2_SB)

#define TFC_MPC_CMD_EM_INSERT_REPLACE_EB           31
#define TFC_MPC_CMD_EM_INSERT_REPLACE_SB           31
#define TFC_MPC_CMD_EM_INSERT_REPLACE_OFFS         0x8

#define TFC_MPC_CMD_EM_INSERT_SET_REPLACE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_REPLACE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_INSERT_REPLACE_EB, \
		     TFC_MPC_CMD_EM_INSERT_REPLACE_SB)
#define TFC_MPC_CMD_EM_INSERT_GET_REPLACE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_INSERT_REPLACE_OFFS), \
		     TFC_MPC_CMD_EM_INSERT_REPLACE_EB, \
		     TFC_MPC_CMD_EM_INSERT_REPLACE_SB)

#define TFC_MPC_CMD_EM_INSERT_SIZE     16

/*
 * CFA Exact Match Delete Command Record:
 *
 * This command searches for an exact match entry index in the static bucket
 * chain and deletes it if found. TABLE_INDEX give the entry index to delete and
 * TABLE_INDEX2 gives the static bucket index. If a matching entry is found:
 * If the matching entry is the last valid entry in the tail bucket, its entry
 * fields (HASH_MSBS and ENTRY_PTR) are set to 0 to delete the entry.
 * If the matching entry is not the last valid entry in the tail bucket, the
 * entry fields from that last entry are moved to the matching entry, and the
 * fields of that last entry are set to 0.
 * If any of the previous processing results in the tail bucket not having any
 * valid entries, the tail bucket is the static bucket, the scope is a locked
 * scope, and CHAIN_PTR=0, hardware evicts the static bucket from the cache and
 * the completion signals this case with CHAIN_UPD=1.
 * If any of the previous processing results in the tail bucket not having any
 * valid entries, and the tail bucket is not the static bucket, the tail bucket
 * is removed from the chain. In this case, the penultimate bucket in the chain
 * becomes the tail bucket. It has CHAIN set to 0 to unlink the tail bucket, and
 * CHAIN_PTR set to that from the original tail bucket to preserve background
 * chaining. The completion signals this case with CHAIN_UPD=1 and returns the
 * index to the bucket removed so that software can de-allocate it.
 * CFA returns OK status if the entry was successfully deleted. Otherwise, it
 * returns EM_MISS status assuming there were no errors that caused processing
 * to be aborted.
 * Offset  31              0
 * 0x0     cache_option    unused(7)       table_scope     unused(3)
 * write_through   unused(4)       opcode
 * 0x4     cache_option2   unused(2)       table_index
 * 0x8     unused(6)       table_index2
 *
 *    opcode (Offset:0x0[7:0], Size: 8)
 *       This value selects the format for the mid-path
 *       command for the CFA.
 *       Value   Enum    Enumeration Description
 *          10      EM_DELETE
 *          This command deletes an entry from the exact
 *          match table. CFA searches for the specified
 *          entry address in the bucket chain at the static
 *          bucket address given.
 *
 *    write_through (Offset:0x0[12], Size: 1)
 *       Sets the OPTION field on the cache interface to
 *       use write-through for EM entry writes while
 *       processing EM_INSERT commands. For all other cases
 *       (inluding EM_INSERT bucket writes), the OPTION
 *       field is set by the CACHE_OPTION and CACHE_OPTION2
 *       fields.
 *
 *    table_scope (Offset:0x0[20:16], Size: 5)
 *       Table scope to access.
 *
 *    cache_option (Offset:0x0[31:28], Size: 4)
 *       Determines setting of OPTION field for all cache
 *       requests while processing any command other than
 *       EM_INSERT, EM_DELETE, or EM_CHAIN. For these latter
 *       commands, CACHE_OPTION sets the OPTION field for
 *       all read requests, and CACHE_OPTION2 sets it for
 *       all write requests.
 *       CFA does not support posted write requests.
 *       Therefore, for WRITE commands, CACHE_OPTION[1] must
 *       be set to 0. And for EM commands that send write
 *       requests (all but EM_SEARCH), CACHE_OPTION2[1] must
 *       be set to 0.
 *
 *    table_index (Offset:0x4[25:0], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       Entry index to delete.
 *
 *    cache_option2 (Offset:0x4[31:28], Size: 4)
 *       Determines setting of OPTION field for all cache
 *       write requests for EM_INSERT, EM_DELETE, and
 *       EM_CHAIN commands.
 *       CFA does not support posted write requests.
 *       Therefore, CACHE_OPTION2[1] must be set to 0.
 *
 *    table_index2 (Offset:0x8[25:0], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       Static bucket address for bucket chain.
 */
#define TFC_MPC_CMD_OPCODE_EM_DELETE    10

#define TFC_MPC_CMD_EM_DELETE_OPCODE_EB           7
#define TFC_MPC_CMD_EM_DELETE_OPCODE_SB           0
#define TFC_MPC_CMD_EM_DELETE_OPCODE_OFFS         0x0

#define TFC_MPC_CMD_EM_DELETE_SET_OPCODE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_OPCODE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_DELETE_OPCODE_EB, \
		     TFC_MPC_CMD_EM_DELETE_OPCODE_SB)
#define TFC_MPC_CMD_EM_DELETE_GET_OPCODE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_OPCODE_OFFS), \
		     TFC_MPC_CMD_EM_DELETE_OPCODE_EB, \
		     TFC_MPC_CMD_EM_DELETE_OPCODE_SB)

#define TFC_MPC_CMD_EM_DELETE_WRITE_THROUGH_EB           12
#define TFC_MPC_CMD_EM_DELETE_WRITE_THROUGH_SB           12
#define TFC_MPC_CMD_EM_DELETE_WRITE_THROUGH_OFFS         0x0

#define TFC_MPC_CMD_EM_DELETE_SET_WRITE_THROUGH(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_WRITE_THROUGH_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_DELETE_WRITE_THROUGH_EB, \
		     TFC_MPC_CMD_EM_DELETE_WRITE_THROUGH_SB)
#define TFC_MPC_CMD_EM_DELETE_GET_WRITE_THROUGH(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_WRITE_THROUGH_OFFS), \
		     TFC_MPC_CMD_EM_DELETE_WRITE_THROUGH_EB, \
		     TFC_MPC_CMD_EM_DELETE_WRITE_THROUGH_SB)

#define TFC_MPC_CMD_EM_DELETE_TABLE_SCOPE_EB           20
#define TFC_MPC_CMD_EM_DELETE_TABLE_SCOPE_SB           16
#define TFC_MPC_CMD_EM_DELETE_TABLE_SCOPE_OFFS         0x0

#define TFC_MPC_CMD_EM_DELETE_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_TABLE_SCOPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_DELETE_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_EM_DELETE_TABLE_SCOPE_SB)
#define TFC_MPC_CMD_EM_DELETE_GET_TABLE_SCOPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_TABLE_SCOPE_OFFS), \
		     TFC_MPC_CMD_EM_DELETE_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_EM_DELETE_TABLE_SCOPE_SB)

#define TFC_MPC_CMD_EM_DELETE_CACHE_OPTION_EB           31
#define TFC_MPC_CMD_EM_DELETE_CACHE_OPTION_SB           28
#define TFC_MPC_CMD_EM_DELETE_CACHE_OPTION_OFFS         0x0

#define TFC_MPC_CMD_EM_DELETE_SET_CACHE_OPTION(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_CACHE_OPTION_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_DELETE_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_EM_DELETE_CACHE_OPTION_SB)
#define TFC_MPC_CMD_EM_DELETE_GET_CACHE_OPTION(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_CACHE_OPTION_OFFS), \
		     TFC_MPC_CMD_EM_DELETE_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_EM_DELETE_CACHE_OPTION_SB)

#define TFC_MPC_CMD_EM_DELETE_TABLE_INDEX_EB           25
#define TFC_MPC_CMD_EM_DELETE_TABLE_INDEX_SB           0
#define TFC_MPC_CMD_EM_DELETE_TABLE_INDEX_OFFS         0x4

#define TFC_MPC_CMD_EM_DELETE_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_TABLE_INDEX_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_DELETE_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_EM_DELETE_TABLE_INDEX_SB)
#define TFC_MPC_CMD_EM_DELETE_GET_TABLE_INDEX(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_TABLE_INDEX_OFFS), \
		     TFC_MPC_CMD_EM_DELETE_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_EM_DELETE_TABLE_INDEX_SB)

#define TFC_MPC_CMD_EM_DELETE_CACHE_OPTION2_EB           31
#define TFC_MPC_CMD_EM_DELETE_CACHE_OPTION2_SB           28
#define TFC_MPC_CMD_EM_DELETE_CACHE_OPTION2_OFFS         0x4

#define TFC_MPC_CMD_EM_DELETE_SET_CACHE_OPTION2(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_CACHE_OPTION2_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_DELETE_CACHE_OPTION2_EB, \
		     TFC_MPC_CMD_EM_DELETE_CACHE_OPTION2_SB)
#define TFC_MPC_CMD_EM_DELETE_GET_CACHE_OPTION2(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_CACHE_OPTION2_OFFS), \
		     TFC_MPC_CMD_EM_DELETE_CACHE_OPTION2_EB, \
		     TFC_MPC_CMD_EM_DELETE_CACHE_OPTION2_SB)

#define TFC_MPC_CMD_EM_DELETE_TABLE_INDEX2_EB           25
#define TFC_MPC_CMD_EM_DELETE_TABLE_INDEX2_SB           0
#define TFC_MPC_CMD_EM_DELETE_TABLE_INDEX2_OFFS         0x8

#define TFC_MPC_CMD_EM_DELETE_SET_TABLE_INDEX2(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_TABLE_INDEX2_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_DELETE_TABLE_INDEX2_EB, \
		     TFC_MPC_CMD_EM_DELETE_TABLE_INDEX2_SB)
#define TFC_MPC_CMD_EM_DELETE_GET_TABLE_INDEX2(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_DELETE_TABLE_INDEX2_OFFS), \
		     TFC_MPC_CMD_EM_DELETE_TABLE_INDEX2_EB, \
		     TFC_MPC_CMD_EM_DELETE_TABLE_INDEX2_SB)

#define TFC_MPC_CMD_EM_DELETE_SIZE     12

/*
 * CFA Exact Match Chain Command Record:
 *
 * This command updates CHAIN_PTR in the tail bucket of a static bucket chain,
 * supplying both the static bucket and the new CHAIN_PTR value. TABLE_INDEX is
 * the new CHAIN_PTR value and TABLE_INDEX2[23:0] is the static bucket.
 * This command provides software a means to update background chaining
 * coherently with other bucket updates. The value of CHAIN is unaffected (stays
 * at 0).
 * For locked scopes, if the static bucket is the tail bucket, it is empty (all
 * of its ENTRY_PTR values are 0), and TABLE_INDEX=0 (the CHAIN_PTR is being set
 * to 0), instead of updating the static bucket it is evicted from the cache. In
 * this case, CHAIN_UPD=1 in the completion.
 * Offset  31              0
 * 0x0     cache_option    unused(7)       table_scope     unused(3)
 * write_through   unused(4)       opcode
 * 0x4     cache_option2   unused(2)       table_index
 * 0x8     unused(6)       table_index2
 *
 *    opcode (Offset:0x0[7:0], Size: 8)
 *       This value selects the format for the mid-path
 *       command for the CFA.
 *       Value   Enum    Enumeration Description
 *          11      EM_CHAIN
 *          This command updates CHAIN_PTR in the tail
 *          bucket of a static bucket chain, supplying both
 *          the static bucket and the new CHAIN_PTR value.
 *
 *    write_through (Offset:0x0[12], Size: 1)
 *       Sets the OPTION field on the cache interface to
 *       use write-through for EM entry writes while
 *       processing EM_INSERT commands. For all other cases
 *       (inluding EM_INSERT bucket writes), the OPTION
 *       field is set by the CACHE_OPTION and CACHE_OPTION2
 *       fields.
 *
 *    table_scope (Offset:0x0[20:16], Size: 5)
 *       Table scope to access.
 *
 *    cache_option (Offset:0x0[31:28], Size: 4)
 *       Determines setting of OPTION field for all cache
 *       requests while processing any command other than
 *       EM_INSERT, EM_DELETE, or EM_CHAIN. For these latter
 *       commands, CACHE_OPTION sets the OPTION field for
 *       all read requests, and CACHE_OPTION2 sets it for
 *       all write requests.
 *       CFA does not support posted write requests.
 *       Therefore, for WRITE commands, CACHE_OPTION[1] must
 *       be set to 0. And for EM commands that send write
 *       requests (all but EM_SEARCH), CACHE_OPTION2[1] must
 *       be set to 0.
 *
 *    table_index (Offset:0x4[25:0], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       New CHAIN_PTR to write to tail bucket.
 *
 *    cache_option2 (Offset:0x4[31:28], Size: 4)
 *       Determines setting of OPTION field for all cache
 *       write requests for EM_INSERT, EM_DELETE, and
 *       EM_CHAIN commands.
 *       CFA does not support posted write requests.
 *       Therefore, CACHE_OPTION2[1] must be set to 0.
 *
 *    table_index2 (Offset:0x8[25:0], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       Static bucket address for bucket chain.
 */
#define TFC_MPC_CMD_OPCODE_EM_CHAIN    11

#define TFC_MPC_CMD_EM_MATCH_CHAIN_OPCODE_EB           7
#define TFC_MPC_CMD_EM_MATCH_CHAIN_OPCODE_SB           0
#define TFC_MPC_CMD_EM_MATCH_CHAIN_OPCODE_OFFS         0x0

#define TFC_MPC_CMD_EM_MATCH_CHAIN_SET_OPCODE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_OPCODE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_OPCODE_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_OPCODE_SB)
#define TFC_MPC_CMD_EM_MATCH_CHAIN_GET_OPCODE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_OPCODE_OFFS), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_OPCODE_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_OPCODE_SB)

#define TFC_MPC_CMD_EM_MATCH_CHAIN_WRITE_THROUGH_EB           12
#define TFC_MPC_CMD_EM_MATCH_CHAIN_WRITE_THROUGH_SB           12
#define TFC_MPC_CMD_EM_MATCH_CHAIN_WRITE_THROUGH_OFFS         0x0

#define TFC_MPC_CMD_EM_MATCH_CHAIN_SET_WRITE_THROUGH(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_WRITE_THROUGH_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_WRITE_THROUGH_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_WRITE_THROUGH_SB)
#define TFC_MPC_CMD_EM_MATCH_CHAIN_GET_WRITE_THROUGH(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_WRITE_THROUGH_OFFS), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_WRITE_THROUGH_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_WRITE_THROUGH_SB)

#define TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_SCOPE_EB           20
#define TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_SCOPE_SB           16
#define TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_SCOPE_OFFS         0x0

#define TFC_MPC_CMD_EM_MATCH_CHAIN_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_SCOPE_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_SCOPE_SB)
#define TFC_MPC_CMD_EM_MATCH_CHAIN_GET_TABLE_SCOPE(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_SCOPE_OFFS), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_SCOPE_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_SCOPE_SB)

#define TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION_EB           31
#define TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION_SB           28
#define TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION_OFFS         0x0

#define TFC_MPC_CMD_EM_MATCH_CHAIN_SET_CACHE_OPTION(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION_SB)
#define TFC_MPC_CMD_EM_MATCH_CHAIN_GET_CACHE_OPTION(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION_OFFS), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION_SB)

#define TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX_EB           25
#define TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX_SB           0
#define TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX_OFFS         0x4

#define TFC_MPC_CMD_EM_MATCH_CHAIN_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX_SB)
#define TFC_MPC_CMD_EM_MATCH_CHAIN_GET_TABLE_INDEX(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX_OFFS), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX_SB)

#define TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION2_EB           31
#define TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION2_SB           28
#define TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION2_OFFS         0x4

#define TFC_MPC_CMD_EM_MATCH_CHAIN_SET_CACHE_OPTION2(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION2_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION2_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION2_SB)
#define TFC_MPC_CMD_EM_MATCH_CHAIN_GET_CACHE_OPTION2(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION2_OFFS), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION2_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_CACHE_OPTION2_SB)

#define TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX2_EB           25
#define TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX2_SB           0
#define TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX2_OFFS         0x8

#define TFC_MPC_CMD_EM_MATCH_CHAIN_SET_TABLE_INDEX2(buf, val) \
	SET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX2_OFFS), \
		     (u32)(val), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX2_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX2_SB)
#define TFC_MPC_CMD_EM_MATCH_CHAIN_GET_TABLE_INDEX2(buf) \
	GET_BITFLD32(TO_P32((buf), TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX2_OFFS), \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX2_EB, \
		     TFC_MPC_CMD_EM_MATCH_CHAIN_TABLE_INDEX2_SB)

#define TFC_MPC_CMD_EM_MATCH_CHAIN_SIZE     12

#endif /* __CFA_P70_MPC_CMDS_H__ */
