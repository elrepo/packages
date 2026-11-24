/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */
#ifndef __CFA_P70_MPC_CMPLS_H__
#define __CFA_P70_MPC_CMPLS_H__

#include "cfa_p70_mpc_common.h"

/*
 * CFA Table Read Completion Record:
 *
 * When no errors, teturns 1-4 consecutive 32B words from the TABLE_INDEX
 * within the TABLE_SCOPE specified in the command, writing them to HOST_ADDRESS
 * from the command.
 * Offset  63              0
 * 0x0     opaque  dma_length      opcode  mp_client       status  unused(2)
 * type
 * 0x8     unused(6)       table_index     unused(3)       table_scope
 * table_type      unused(4)       hash_msb        unused(3)       v
 *
 *    type (Offset:0x0[5:0], Size: 6)
 *       This field indicates the exact type of the
 *       completion. By convention, the LSB identifies the
 *       length of the record in 16B units. Even values
 *       indicate 16B records. Odd values indicate 32B
 *       records **(EXCEPT no_op!!!!)** .
 *       Value   Enum    Enumeration Description
 *          30      mid_path_short
 *          Mid Path Short Completion : Completion of a Mid
 *          Path Command. Length = 16B
 *
 *    status (Offset:0x0[11:8], Size: 4)
 *       The command processing status.
 *       Value   Enum    Enumeration Description
 *          0       OK
 *          Completed without error.
 *          1       UNSPRT_ERR
 *          The CFA OPCODE is an unsupported value.
 *          2       FMT_ERR
 *          Indicates a CFA command formatting error. This
 *          error can occur on any of the supported CFA
 *          commands.
 *          Error conditions:
 *          DATA_SIZE[2:0] outside range of [1, 4]. (Does
 *          not apply to READ_CLR, EM_DELETE, or EM_CHAIN
 *          commands as they do not have a DATA_SIZE field)
 *          HOST_ADDRESS[1:0] != 0 (Only applies to READ,
 *          READ_CLR, and EVENT_COLLECTION as other commands
 *          do not have a HOST_ADDRESS field.
 *          3       SCOPE_ERR
 *          Access to TABLE_SCOPE is disabled for the SVIF.
 *          Indates that the bit indexed by (SVIF,
 *          TABLE_SCOPE) in the TAI_SVIF_SCOPE memory is set
 *          to 0.
 *          4       ADDR_ERR
 *          This error can only occur for commands having
 *          TABLE_TYPE present and set to EM and not having
 *          any of the previous errors, or for any of the
 *          EM* commands, for which a TABLE_TYPE of EM is
 *          implied.
 *          It indicates that an EM address (TABLE_INDEX*)
 *          in the command is invalid based on (EM_BUCKETS,
 *          EM_SIZE) parameters configured for TABLE_SCOPE.
 *          All addresses must be in the range [0,
 *          EM_SIZE). Static bucket addresses must be within
 *          the range determined by EM_BUCKETS. Dynamic
 *          bucket addresses and entries must be outside of
 *          the static bucket range.
 *          5       CACHE_ERR
 *          One of more cache responses signaled an error
 *          while processing the command.
 *
 *    mp_client (Offset:0x0[15:12], Size: 4)
 *       This field represents the Mid-Path client that
 *       generated the completion.
 *       Value   Enum    Enumeration Description
 *          2       TE_CFA
 *          TE-CFA
 *          3       RE_CFA
 *          RE-CFA
 *
 *    opcode (Offset:0x0[23:16], Size: 8)
 *       OPCODE from the command.
 *       Value   Enum    Enumeration Description
 *          0       READ
 *          This command reads 1-4 consecutive 32B words
 *          from the specified address within a table scope.
 *
 *    dma_length (Offset:0x0[31:24], Size: 8)
 *       The length of the DMA that accompanies the
 *       completion in units of DWORDs (32b). Valid values
 *       are [0, 128]. A value of zero indicates that there
 *       is no DMA that accompanies the completion.
 *
 *    opaque (Offset:0x0[63:32], Size: 32)
 *       This is a copy of the opaque field from the mid
 *       path BD of this command.
 *
 *    v (Offset:0x8[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    hash_msb (Offset:0x8[15:4], Size: 12)
 *       For EM_SEARCH and EM_INSERT commands without
 *       errors that abort the command processing prior to
 *       the hash computation, set to HASH[35:24] of the
 *       hash computed from the exact match entry key in the
 *       command.
 *       For all other cases, set to 0 except for the
 *       following error conditions, which carry debug
 *       information in this field as shown by error status
 *       below:
 *       FMT_ERR:
 *       Set to {7'd0, HOST_ADDRESS[1:0],
 *       DATA_SIZE[2:0]}.
 *       If HOST_ADDRESS or DATA_SIZE field not
 *       present they are set to 0.
 *       SCOPE_ERR:
 *       Set to {1'b0, SVIF[10:0]}.
 *       ADDR_ERR:
 *       Only possible when TABLE_TYPE=EM or for EM*
 *       commands
 *       Set to {1'b0, TABLE_INDEX[2:0], 5'd0,
 *       DATA_SIZE[2:0]}
 *       TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error
 *       TABLE_INDEX[1]=1 if TABLE_INDEX2 had an error
 *       TABLE_INDEX[0]=1 if TABLE_INDEX had an error
 *       TABLE_INDEX[n]=0 if the completion does not
 *       have the corresponding TABLE_INDEX field above.
 *       CACHE_ERR:
 *       Set to {9'd0, DATA_SIZE[2:0]}
 *
 *    table_type (Offset:0x8[23:20], Size: 4)
 *       TABLE_TYPE from the command.
 *       Value   Enum    Enumeration Description
 *          0       ACTION
 *          This command acts on the action table of the
 *          specified scope.
 *          1       EM
 *          This command acts on the exact match table of
 *          the specified scope.
 *
 *    table_scope (Offset:0x8[28:24], Size: 5)
 *       TABLE_SCOPE from the command.
 *
 *    table_index (Offset:0x8[57:32], Size: 26)
 *       TABLE_INDEX from the command.
 *       This structure is used to inform the host of an
 *       event within the NIC.
 */
#define TFC_MPC_TBL_RD_CMPL_TYPE_MID_PATH_SHORT    30

#define TFC_MPC_TBL_RD_CMPL_TYPE_EB           5
#define TFC_MPC_TBL_RD_CMPL_TYPE_SB           0
#define TFC_MPC_TBL_RD_CMPL_TYPE_OFFS         0x0

#define TFC_MPC_TBL_RD_CMPL_SET_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RD_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_RD_CMPL_TYPE_SB)
#define TFC_MPC_TBL_RD_CMPL_GET_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_TYPE_OFFS), \
		     TFC_MPC_TBL_RD_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_RD_CMPL_TYPE_SB)

#define TFC_MPC_TBL_RD_CMPL_STATUS_OK            0
#define TFC_MPC_TBL_RD_CMPL_STATUS_UNSPRT_ERR    1
#define TFC_MPC_TBL_RD_CMPL_STATUS_FMT_ERR       2
#define TFC_MPC_TBL_RD_CMPL_STATUS_SCOPE_ERR     3
#define TFC_MPC_TBL_RD_CMPL_STATUS_ADDR_ERR      4
#define TFC_MPC_TBL_RD_CMPL_STATUS_CACHE_ERR     5

#define TFC_MPC_TBL_RD_CMPL_STATUS_EB           11
#define TFC_MPC_TBL_RD_CMPL_STATUS_SB           8
#define TFC_MPC_TBL_RD_CMPL_STATUS_OFFS         0x0

#define TFC_MPC_TBL_RD_CMPL_SET_STATUS(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_STATUS_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RD_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_RD_CMPL_STATUS_SB)
#define TFC_MPC_TBL_RD_CMPL_GET_STATUS(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_STATUS_OFFS), \
		     TFC_MPC_TBL_RD_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_RD_CMPL_STATUS_SB)

#define TFC_MPC_TBL_RD_CMPL_MP_CLIENT_TE_CFA    2
#define TFC_MPC_TBL_RD_CMPL_MP_CLIENT_RE_CFA    3

#define TFC_MPC_TBL_RD_CMPL_MP_CLIENT_EB           15
#define TFC_MPC_TBL_RD_CMPL_MP_CLIENT_SB           12
#define TFC_MPC_TBL_RD_CMPL_MP_CLIENT_OFFS         0x0

#define TFC_MPC_TBL_RD_CMPL_SET_MP_CLIENT(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_MP_CLIENT_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RD_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_RD_CMPL_MP_CLIENT_SB)
#define TFC_MPC_TBL_RD_CMPL_GET_MP_CLIENT(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_MP_CLIENT_OFFS), \
		     TFC_MPC_TBL_RD_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_RD_CMPL_MP_CLIENT_SB)

#define TFC_MPC_CMD_OPCODE_READ    0

#define TFC_MPC_TBL_RD_CMPL_OPCODE_EB           23
#define TFC_MPC_TBL_RD_CMPL_OPCODE_SB           16
#define TFC_MPC_TBL_RD_CMPL_OPCODE_OFFS         0x0

#define TFC_MPC_TBL_RD_CMPL_SET_OPCODE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_OPCODE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RD_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_RD_CMPL_OPCODE_SB)
#define TFC_MPC_TBL_RD_CMPL_GET_OPCODE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_OPCODE_OFFS), \
		     TFC_MPC_TBL_RD_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_RD_CMPL_OPCODE_SB)

#define TFC_MPC_TBL_RD_CMPL_DMA_LENGTH_EB           31
#define TFC_MPC_TBL_RD_CMPL_DMA_LENGTH_SB           24
#define TFC_MPC_TBL_RD_CMPL_DMA_LENGTH_OFFS         0x0

#define TFC_MPC_TBL_RD_CMPL_SET_DMA_LENGTH(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_DMA_LENGTH_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RD_CMPL_DMA_LENGTH_EB, \
		     TFC_MPC_TBL_RD_CMPL_DMA_LENGTH_SB)
#define TFC_MPC_TBL_RD_CMPL_GET_DMA_LENGTH(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_DMA_LENGTH_OFFS), \
		     TFC_MPC_TBL_RD_CMPL_DMA_LENGTH_EB, \
		     TFC_MPC_TBL_RD_CMPL_DMA_LENGTH_SB)

#define TFC_MPC_TBL_RD_CMPL_OPAQUE_EB           63
#define TFC_MPC_TBL_RD_CMPL_OPAQUE_SB           32
#define TFC_MPC_TBL_RD_CMPL_OPAQUE_OFFS         0x0

#define TFC_MPC_TBL_RD_CMPL_SET_OPAQUE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_OPAQUE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RD_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_RD_CMPL_OPAQUE_SB)
#define TFC_MPC_TBL_RD_CMPL_GET_OPAQUE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_OPAQUE_OFFS), \
		     TFC_MPC_TBL_RD_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_RD_CMPL_OPAQUE_SB)

#define TFC_MPC_TBL_RD_CMPL_V_EB           0
#define TFC_MPC_TBL_RD_CMPL_V_SB           0
#define TFC_MPC_TBL_RD_CMPL_V_OFFS         0x8

#define TFC_MPC_TBL_RD_CMPL_SET_V(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_V_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RD_CMPL_V_EB, \
		     TFC_MPC_TBL_RD_CMPL_V_SB)
#define TFC_MPC_TBL_RD_CMPL_GET_V(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_V_OFFS), \
		     TFC_MPC_TBL_RD_CMPL_V_EB, \
		     TFC_MPC_TBL_RD_CMPL_V_SB)

#define TFC_MPC_TBL_RD_CMPL_HASH_MSB_EB           15
#define TFC_MPC_TBL_RD_CMPL_HASH_MSB_SB           4
#define TFC_MPC_TBL_RD_CMPL_HASH_MSB_OFFS         0x8

#define TFC_MPC_TBL_RD_CMPL_SET_HASH_MSB(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_HASH_MSB_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RD_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_RD_CMPL_HASH_MSB_SB)
#define TFC_MPC_TBL_RD_CMPL_GET_HASH_MSB(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_HASH_MSB_OFFS), \
		     TFC_MPC_TBL_RD_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_RD_CMPL_HASH_MSB_SB)

#define TFC_MPC_TBL_RD_CMPL_TABLE_TYPE_ACTION    0
#define TFC_MPC_TBL_RD_CMPL_TABLE_TYPE_EM        1

#define TFC_MPC_TBL_RD_CMPL_TABLE_TYPE_EB           23
#define TFC_MPC_TBL_RD_CMPL_TABLE_TYPE_SB           20
#define TFC_MPC_TBL_RD_CMPL_TABLE_TYPE_OFFS         0x8

#define TFC_MPC_TBL_RD_CMPL_SET_TABLE_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_TABLE_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RD_CMPL_TABLE_TYPE_EB, \
		     TFC_MPC_TBL_RD_CMPL_TABLE_TYPE_SB)
#define TFC_MPC_TBL_RD_CMPL_GET_TABLE_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_TABLE_TYPE_OFFS), \
		     TFC_MPC_TBL_RD_CMPL_TABLE_TYPE_EB, \
		     TFC_MPC_TBL_RD_CMPL_TABLE_TYPE_SB)

#define TFC_MPC_TBL_RD_CMPL_TABLE_SCOPE_EB           28
#define TFC_MPC_TBL_RD_CMPL_TABLE_SCOPE_SB           24
#define TFC_MPC_TBL_RD_CMPL_TABLE_SCOPE_OFFS         0x8

#define TFC_MPC_TBL_RD_CMPL_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_TABLE_SCOPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RD_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_RD_CMPL_TABLE_SCOPE_SB)
#define TFC_MPC_TBL_RD_CMPL_GET_TABLE_SCOPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_TABLE_SCOPE_OFFS), \
		     TFC_MPC_TBL_RD_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_RD_CMPL_TABLE_SCOPE_SB)

#define TFC_MPC_TBL_RD_CMPL_TABLE_INDEX_EB           57
#define TFC_MPC_TBL_RD_CMPL_TABLE_INDEX_SB           32
#define TFC_MPC_TBL_RD_CMPL_TABLE_INDEX_OFFS         0x8

#define TFC_MPC_TBL_RD_CMPL_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_TABLE_INDEX_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RD_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_RD_CMPL_TABLE_INDEX_SB)
#define TFC_MPC_TBL_RD_CMPL_GET_TABLE_INDEX(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RD_CMPL_TABLE_INDEX_OFFS), \
		     TFC_MPC_TBL_RD_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_RD_CMPL_TABLE_INDEX_SB)

#define TFC_MPC_TBL_RD_CMPL_SIZE     16

/*
 * CFA Table Write Completion Record:
 *
 * Returns status of the write of 1-4 consecutive 32B words starting at
 * TABLE_INDEX in the table specified by (TABLE_TYPE, TABLE_SCOPE).
 * Offset  63              0
 * 0x0     opaque  unused(8)       opcode  mp_client       status  unused(2)
 * type
 * 0x8     unused(6)       table_index     unused(3)       table_scope
 * table_type      unused(4)       hash_msb        unused(3)       v
 *
 *    type (Offset:0x0[5:0], Size: 6)
 *       This field indicates the exact type of the
 *       completion. By convention, the LSB identifies the
 *       length of the record in 16B units. Even values
 *       indicate 16B records. Odd values indicate 32B
 *       records **(EXCEPT no_op!!!!)** .
 *       Value   Enum    Enumeration Description
 *          30      mid_path_short
 *          Mid Path Short Completion : Completion of a Mid
 *          Path Command. Length = 16B
 *
 *    status (Offset:0x0[11:8], Size: 4)
 *       The command processing status.
 *       Value   Enum    Enumeration Description
 *          0       OK
 *          Completed without error.
 *          1       UNSPRT_ERR
 *          The CFA OPCODE is an unsupported value.
 *          2       FMT_ERR
 *          Indicates a CFA command formatting error. This
 *          error can occur on any of the supported CFA
 *          commands.
 *          Error conditions:
 *          DATA_SIZE[2:0] outside range of [1, 4]. (Does
 *          not apply to READ_CLR, EM_DELETE, or EM_CHAIN
 *          commands as they do not have a DATA_SIZE field)
 *          HOST_ADDRESS[1:0] != 0 (Only applies to READ,
 *          READ_CLR, and EVENT_COLLECTION as other commands
 *          do not have a HOST_ADDRESS field.
 *          3       SCOPE_ERR
 *          Access to TABLE_SCOPE is disabled for the SVIF.
 *          Indates that the bit indexed by (SVIF,
 *          TABLE_SCOPE) in the TAI_SVIF_SCOPE memory is set
 *          to 0.
 *          4       ADDR_ERR
 *          This error can only occur for commands having
 *          TABLE_TYPE present and set to EM and not having
 *          any of the previous errors, or for any of the
 *          EM* commands, for which a TABLE_TYPE of EM is
 *          implied.
 *          It indicates that an EM address (TABLE_INDEX*)
 *          in the command is invalid based on (EM_BUCKETS,
 *          EM_SIZE) parameters configured for TABLE_SCOPE.
 *          All addresses must be in the range [0,
 *          EM_SIZE). Static bucket addresses must be within
 *          the range determined by EM_BUCKETS. Dynamic
 *          bucket addresses and entries must be outside of
 *          the static bucket range.
 *          5       CACHE_ERR
 *          One of more cache responses signaled an error
 *          while processing the command.
 *
 *    mp_client (Offset:0x0[15:12], Size: 4)
 *       This field represents the Mid-Path client that
 *       generated the completion.
 *       Value   Enum    Enumeration Description
 *          2       TE_CFA
 *          TE-CFA
 *          3       RE_CFA
 *          RE-CFA
 *
 *    opcode (Offset:0x0[23:16], Size: 8)
 *       OPCODE from the command.
 *       Value   Enum    Enumeration Description
 *          1       WRITE
 *          This command writes 1-4 consecutive 32B words
 *          to the specified address within a table scope.
 *
 *    opaque (Offset:0x0[63:32], Size: 32)
 *       This is a copy of the opaque field from the mid
 *       path BD of this command.
 *
 *    v (Offset:0x8[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    hash_msb (Offset:0x8[15:4], Size: 12)
 *       For EM_SEARCH and EM_INSERT commands without
 *       errors that abort the command processing prior to
 *       the hash computation, set to HASH[35:24] of the
 *       hash computed from the exact match entry key in the
 *       command.
 *       For all other cases, set to 0 except for the
 *       following error conditions, which carry debug
 *       information in this field as shown by error status
 *       below:
 *       FMT_ERR:
 *       Set to {7'd0, HOST_ADDRESS[1:0],
 *       DATA_SIZE[2:0]}.
 *       If HOST_ADDRESS or DATA_SIZE field not
 *       present they are set to 0.
 *       SCOPE_ERR:
 *       Set to {1'b0, SVIF[10:0]}.
 *       ADDR_ERR:
 *       Only possible when TABLE_TYPE=EM or for EM*
 *       commands
 *       Set to {1'b0, TABLE_INDEX[2:0], 5'd0,
 *       DATA_SIZE[2:0]}
 *       TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error
 *       TABLE_INDEX[1]=1 if TABLE_INDEX2 had an error
 *       TABLE_INDEX[0]=1 if TABLE_INDEX had an error
 *       TABLE_INDEX[n]=0 if the completion does not
 *       have the corresponding TABLE_INDEX field above.
 *       CACHE_ERR:
 *       Set to {9'd0, DATA_SIZE[2:0]}
 *
 *    table_type (Offset:0x8[23:20], Size: 4)
 *       TABLE_TYPE from the command.
 *       Value   Enum    Enumeration Description
 *          0       ACTION
 *          This command acts on the action table of the
 *          specified scope.
 *          1       EM
 *          This command acts on the exact match table of
 *          the specified scope.
 *
 *    table_scope (Offset:0x8[28:24], Size: 5)
 *       TABLE_SCOPE from the command.
 *
 *    table_index (Offset:0x8[57:32], Size: 26)
 *       TABLE_INDEX from the command.
 *       This structure is used to inform the host of an
 *       event within the NIC.
 */
#define TFC_MPC_TBL_WR_CMPL_TYPE_MID_PATH_SHORT    30

#define TFC_MPC_TBL_WR_CMPL_TYPE_EB           5
#define TFC_MPC_TBL_WR_CMPL_TYPE_SB           0
#define TFC_MPC_TBL_WR_CMPL_TYPE_OFFS         0x0

#define TFC_MPC_TBL_WR_CMPL_SET_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_WR_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_WR_CMPL_TYPE_SB)
#define TFC_MPC_TBL_WR_CMPL_GET_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_TYPE_OFFS), \
		     TFC_MPC_TBL_WR_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_WR_CMPL_TYPE_SB)

#define TFC_MPC_TBL_WR_CMPL_STATUS_OK            0
#define TFC_MPC_TBL_WR_CMPL_STATUS_UNSPRT_ERR    1
#define TFC_MPC_TBL_WR_CMPL_STATUS_FMT_ERR       2
#define TFC_MPC_TBL_WR_CMPL_STATUS_SCOPE_ERR     3
#define TFC_MPC_TBL_WR_CMPL_STATUS_ADDR_ERR      4
#define TFC_MPC_TBL_WR_CMPL_STATUS_CACHE_ERR     5

#define TFC_MPC_TBL_WR_CMPL_STATUS_EB           11
#define TFC_MPC_TBL_WR_CMPL_STATUS_SB           8
#define TFC_MPC_TBL_WR_CMPL_STATUS_OFFS         0x0

#define TFC_MPC_TBL_WR_CMPL_SET_STATUS(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_STATUS_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_WR_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_WR_CMPL_STATUS_SB)
#define TFC_MPC_TBL_WR_CMPL_GET_STATUS(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_STATUS_OFFS), \
		     TFC_MPC_TBL_WR_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_WR_CMPL_STATUS_SB)

#define TFC_MPC_TBL_WR_CMPL_MP_CLIENT_TE_CFA    2
#define TFC_MPC_TBL_WR_CMPL_MP_CLIENT_RE_CFA    3

#define TFC_MPC_TBL_WR_CMPL_MP_CLIENT_EB           15
#define TFC_MPC_TBL_WR_CMPL_MP_CLIENT_SB           12
#define TFC_MPC_TBL_WR_CMPL_MP_CLIENT_OFFS         0x0

#define TFC_MPC_TBL_WR_CMPL_SET_MP_CLIENT(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_MP_CLIENT_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_WR_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_WR_CMPL_MP_CLIENT_SB)
#define TFC_MPC_TBL_WR_CMPL_GET_MP_CLIENT(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_MP_CLIENT_OFFS), \
		     TFC_MPC_TBL_WR_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_WR_CMPL_MP_CLIENT_SB)

#define TFC_MPC_CMD_OPCODE_WRITE    1

#define TFC_MPC_TBL_WR_CMPL_OPCODE_EB           23
#define TFC_MPC_TBL_WR_CMPL_OPCODE_SB           16
#define TFC_MPC_TBL_WR_CMPL_OPCODE_OFFS         0x0

#define TFC_MPC_TBL_WR_CMPL_SET_OPCODE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_OPCODE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_WR_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_WR_CMPL_OPCODE_SB)
#define TFC_MPC_TBL_WR_CMPL_GET_OPCODE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_OPCODE_OFFS), \
		     TFC_MPC_TBL_WR_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_WR_CMPL_OPCODE_SB)

#define TFC_MPC_TBL_WR_CMPL_OPAQUE_EB           63
#define TFC_MPC_TBL_WR_CMPL_OPAQUE_SB           32
#define TFC_MPC_TBL_WR_CMPL_OPAQUE_OFFS         0x0

#define TFC_MPC_TBL_WR_CMPL_SET_OPAQUE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_OPAQUE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_WR_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_WR_CMPL_OPAQUE_SB)
#define TFC_MPC_TBL_WR_CMPL_GET_OPAQUE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_OPAQUE_OFFS), \
		     TFC_MPC_TBL_WR_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_WR_CMPL_OPAQUE_SB)

#define TFC_MPC_TBL_WR_CMPL_V_EB           0
#define TFC_MPC_TBL_WR_CMPL_V_SB           0
#define TFC_MPC_TBL_WR_CMPL_V_OFFS         0x8

#define TFC_MPC_TBL_WR_CMPL_SET_V(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_V_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_WR_CMPL_V_EB, \
		     TFC_MPC_TBL_WR_CMPL_V_SB)
#define TFC_MPC_TBL_WR_CMPL_GET_V(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_V_OFFS), \
		     TFC_MPC_TBL_WR_CMPL_V_EB, \
		     TFC_MPC_TBL_WR_CMPL_V_SB)

#define TFC_MPC_TBL_WR_CMPL_HASH_MSB_EB           15
#define TFC_MPC_TBL_WR_CMPL_HASH_MSB_SB           4
#define TFC_MPC_TBL_WR_CMPL_HASH_MSB_OFFS         0x8

#define TFC_MPC_TBL_WR_CMPL_SET_HASH_MSB(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_HASH_MSB_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_WR_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_WR_CMPL_HASH_MSB_SB)
#define TFC_MPC_TBL_WR_CMPL_GET_HASH_MSB(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_HASH_MSB_OFFS), \
		     TFC_MPC_TBL_WR_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_WR_CMPL_HASH_MSB_SB)

#define TFC_MPC_TBL_WR_CMPL_TABLE_TYPE_ACTION    0
#define TFC_MPC_TBL_WR_CMPL_TABLE_TYPE_EM        1

#define TFC_MPC_TBL_WR_CMPL_TABLE_TYPE_EB           23
#define TFC_MPC_TBL_WR_CMPL_TABLE_TYPE_SB           20
#define TFC_MPC_TBL_WR_CMPL_TABLE_TYPE_OFFS         0x8

#define TFC_MPC_TBL_WR_CMPL_SET_TABLE_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_TABLE_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_WR_CMPL_TABLE_TYPE_EB, \
		     TFC_MPC_TBL_WR_CMPL_TABLE_TYPE_SB)
#define TFC_MPC_TBL_WR_CMPL_GET_TABLE_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_TABLE_TYPE_OFFS), \
		     TFC_MPC_TBL_WR_CMPL_TABLE_TYPE_EB, \
		     TFC_MPC_TBL_WR_CMPL_TABLE_TYPE_SB)

#define TFC_MPC_TBL_WR_CMPL_TABLE_SCOPE_EB           28
#define TFC_MPC_TBL_WR_CMPL_TABLE_SCOPE_SB           24
#define TFC_MPC_TBL_WR_CMPL_TABLE_SCOPE_OFFS         0x8

#define TFC_MPC_TBL_WR_CMPL_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_TABLE_SCOPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_WR_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_WR_CMPL_TABLE_SCOPE_SB)
#define TFC_MPC_TBL_WR_CMPL_GET_TABLE_SCOPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_TABLE_SCOPE_OFFS), \
		     TFC_MPC_TBL_WR_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_WR_CMPL_TABLE_SCOPE_SB)

#define TFC_MPC_TBL_WR_CMPL_TABLE_INDEX_EB           57
#define TFC_MPC_TBL_WR_CMPL_TABLE_INDEX_SB           32
#define TFC_MPC_TBL_WR_CMPL_TABLE_INDEX_OFFS         0x8

#define TFC_MPC_TBL_WR_CMPL_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_TABLE_INDEX_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_WR_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_WR_CMPL_TABLE_INDEX_SB)
#define TFC_MPC_TBL_WR_CMPL_GET_TABLE_INDEX(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_WR_CMPL_TABLE_INDEX_OFFS), \
		     TFC_MPC_TBL_WR_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_WR_CMPL_TABLE_INDEX_SB)

#define TFC_MPC_TBL_WR_CMPL_SIZE     16

/*
 * CFA Table Read-Clear Completion Record:
 *
 * When no errors, returns 1 32B word from TABLE_INDEX in the table specified
 * by (TABLE_TYPE, TABLE_SCOPE). The data returned is the value prior to the
 * clear.
 * Offset  63              0
 * 0x0     opaque  dma_length      opcode  mp_client       status  unused(2)
 * type
 * 0x8     unused(6)       table_index     unused(3)       table_scope
 * table_type      unused(4)       hash_msb        unused(3)       v
 *
 *    type (Offset:0x0[5:0], Size: 6)
 *       This field indicates the exact type of the
 *       completion. By convention, the LSB identifies the
 *       length of the record in 16B units. Even values
 *       indicate 16B records. Odd values indicate 32B
 *       records **(EXCEPT no_op!!!!)** .
 *       Value   Enum    Enumeration Description
 *          30      mid_path_short
 *          Mid Path Short Completion : Completion of a Mid
 *          Path Command. Length = 16B
 *
 *    status (Offset:0x0[11:8], Size: 4)
 *       The command processing status.
 *       Value   Enum    Enumeration Description
 *          0       OK
 *          Completed without error.
 *          1       UNSPRT_ERR
 *          The CFA OPCODE is an unsupported value.
 *          2       FMT_ERR
 *          Indicates a CFA command formatting error. This
 *          error can occur on any of the supported CFA
 *          commands.
 *          Error conditions:
 *          DATA_SIZE[2:0] outside range of [1, 4]. (Does
 *          not apply to READ_CLR, EM_DELETE, or EM_CHAIN
 *          commands as they do not have a DATA_SIZE field)
 *          HOST_ADDRESS[1:0] != 0 (Only applies to READ,
 *          READ_CLR, and EVENT_COLLECTION as other commands
 *          do not have a HOST_ADDRESS field.
 *          3       SCOPE_ERR
 *          Access to TABLE_SCOPE is disabled for the SVIF.
 *          Indates that the bit indexed by (SVIF,
 *          TABLE_SCOPE) in the TAI_SVIF_SCOPE memory is set
 *          to 0.
 *          4       ADDR_ERR
 *          This error can only occur for commands having
 *          TABLE_TYPE present and set to EM and not having
 *          any of the previous errors, or for any of the
 *          EM* commands, for which a TABLE_TYPE of EM is
 *          implied.
 *          It indicates that an EM address (TABLE_INDEX*)
 *          in the command is invalid based on (EM_BUCKETS,
 *          EM_SIZE) parameters configured for TABLE_SCOPE.
 *          All addresses must be in the range [0,
 *          EM_SIZE). Static bucket addresses must be within
 *          the range determined by EM_BUCKETS. Dynamic
 *          bucket addresses and entries must be outside of
 *          the static bucket range.
 *          5       CACHE_ERR
 *          One of more cache responses signaled an error
 *          while processing the command.
 *
 *    mp_client (Offset:0x0[15:12], Size: 4)
 *       This field represents the Mid-Path client that
 *       generated the completion.
 *       Value   Enum    Enumeration Description
 *          2       TE_CFA
 *          TE-CFA
 *          3       RE_CFA
 *          RE-CFA
 *
 *    opcode (Offset:0x0[23:16], Size: 8)
 *       OPCODE from the command.
 *       Value   Enum    Enumeration Description
 *          2       READ_CLR
 *          This command performs a read-modify-write to
 *          the specified 32B address using a 16b mask that
 *          specifies up to 16 16b words to clear. It
 *          returns the 32B data word prior to the clear
 *          operation.
 *
 *    dma_length (Offset:0x0[31:24], Size: 8)
 *       The length of the DMA that accompanies the
 *       completion in units of DWORDs (32b). Valid values
 *       are [0, 128]. A value of zero indicates that there
 *       is no DMA that accompanies the completion.
 *
 *    opaque (Offset:0x0[63:32], Size: 32)
 *       This is a copy of the opaque field from the mid
 *       path BD of this command.
 *
 *    v (Offset:0x8[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    hash_msb (Offset:0x8[15:4], Size: 12)
 *       For EM_SEARCH and EM_INSERT commands without
 *       errors that abort the command processing prior to
 *       the hash computation, set to HASH[35:24] of the
 *       hash computed from the exact match entry key in the
 *       command.
 *       For all other cases, set to 0 except for the
 *       following error conditions, which carry debug
 *       information in this field as shown by error status
 *       below:
 *       FMT_ERR:
 *       Set to {7'd0, HOST_ADDRESS[1:0],
 *       DATA_SIZE[2:0]}.
 *       If HOST_ADDRESS or DATA_SIZE field not
 *       present they are set to 0.
 *       SCOPE_ERR:
 *       Set to {1'b0, SVIF[10:0]}.
 *       ADDR_ERR:
 *       Only possible when TABLE_TYPE=EM or for EM*
 *       commands
 *       Set to {1'b0, TABLE_INDEX[2:0], 5'd0,
 *       DATA_SIZE[2:0]}
 *       TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error
 *       TABLE_INDEX[1]=1 if TABLE_INDEX2 had an error
 *       TABLE_INDEX[0]=1 if TABLE_INDEX had an error
 *       TABLE_INDEX[n]=0 if the completion does not
 *       have the corresponding TABLE_INDEX field above.
 *       CACHE_ERR:
 *       Set to {9'd0, DATA_SIZE[2:0]}
 *
 *    table_type (Offset:0x8[23:20], Size: 4)
 *       TABLE_TYPE from the command.
 *       Value   Enum    Enumeration Description
 *          0       ACTION
 *          This command acts on the action table of the
 *          specified scope.
 *          1       EM
 *          This command acts on the exact match table of
 *          the specified scope.
 *
 *    table_scope (Offset:0x8[28:24], Size: 5)
 *       TABLE_SCOPE from the command.
 *
 *    table_index (Offset:0x8[57:32], Size: 26)
 *       TABLE_INDEX from the command.
 *       This structure is used to inform the host of an
 *       event within the NIC.
 */
#define TFC_MPC_TBL_RDCLR_CMPL_TYPE_MID_PATH_SHORT    30

#define TFC_MPC_TBL_RDCLR_CMPL_TYPE_EB           5
#define TFC_MPC_TBL_RDCLR_CMPL_TYPE_SB           0
#define TFC_MPC_TBL_RDCLR_CMPL_TYPE_OFFS         0x0

#define TFC_MPC_TBL_RDCLR_CMPL_SET_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RDCLR_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_TYPE_SB)
#define TFC_MPC_TBL_RDCLR_CMPL_GET_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_TYPE_OFFS), \
		     TFC_MPC_TBL_RDCLR_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_TYPE_SB)

#define TFC_MPC_TBL_RDCLR_CMPL_STATUS_OK            0
#define TFC_MPC_TBL_RDCLR_CMPL_STATUS_UNSPRT_ERR    1
#define TFC_MPC_TBL_RDCLR_CMPL_STATUS_FMT_ERR       2
#define TFC_MPC_TBL_RDCLR_CMPL_STATUS_SCOPE_ERR     3
#define TFC_MPC_TBL_RDCLR_CMPL_STATUS_ADDR_ERR      4
#define TFC_MPC_TBL_RDCLR_CMPL_STATUS_CACHE_ERR     5

#define TFC_MPC_TBL_RDCLR_CMPL_STATUS_EB           11
#define TFC_MPC_TBL_RDCLR_CMPL_STATUS_SB           8
#define TFC_MPC_TBL_RDCLR_CMPL_STATUS_OFFS         0x0

#define TFC_MPC_TBL_RDCLR_CMPL_SET_STATUS(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_STATUS_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RDCLR_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_STATUS_SB)
#define TFC_MPC_TBL_RDCLR_CMPL_GET_STATUS(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_STATUS_OFFS), \
		     TFC_MPC_TBL_RDCLR_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_STATUS_SB)

#define TFC_MPC_TBL_RDCLR_CMPL_MP_CLIENT_TE_CFA    2
#define TFC_MPC_TBL_RDCLR_CMPL_MP_CLIENT_RE_CFA    3

#define TFC_MPC_TBL_RDCLR_CMPL_MP_CLIENT_EB           15
#define TFC_MPC_TBL_RDCLR_CMPL_MP_CLIENT_SB           12
#define TFC_MPC_TBL_RDCLR_CMPL_MP_CLIENT_OFFS         0x0

#define TFC_MPC_TBL_RDCLR_CMPL_SET_MP_CLIENT(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_MP_CLIENT_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RDCLR_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_MP_CLIENT_SB)
#define TFC_MPC_TBL_RDCLR_CMPL_GET_MP_CLIENT(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_MP_CLIENT_OFFS), \
		     TFC_MPC_TBL_RDCLR_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_MP_CLIENT_SB)

#define TFC_MPC_CMD_OPCODE_READ_CLR    2

#define TFC_MPC_TBL_RDCLR_CMPL_OPCODE_EB           23
#define TFC_MPC_TBL_RDCLR_CMPL_OPCODE_SB           16
#define TFC_MPC_TBL_RDCLR_CMPL_OPCODE_OFFS         0x0

#define TFC_MPC_TBL_RDCLR_CMPL_SET_OPCODE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_OPCODE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RDCLR_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_OPCODE_SB)
#define TFC_MPC_TBL_RDCLR_CMPL_GET_OPCODE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_OPCODE_OFFS), \
		     TFC_MPC_TBL_RDCLR_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_OPCODE_SB)

#define TFC_MPC_TBL_RDCLR_CMPL_DMA_LENGTH_EB           31
#define TFC_MPC_TBL_RDCLR_CMPL_DMA_LENGTH_SB           24
#define TFC_MPC_TBL_RDCLR_CMPL_DMA_LENGTH_OFFS         0x0

#define TFC_MPC_TBL_RDCLR_CMPL_SET_DMA_LENGTH(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_DMA_LENGTH_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RDCLR_CMPL_DMA_LENGTH_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_DMA_LENGTH_SB)
#define TFC_MPC_TBL_RDCLR_CMPL_GET_DMA_LENGTH(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_DMA_LENGTH_OFFS), \
		     TFC_MPC_TBL_RDCLR_CMPL_DMA_LENGTH_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_DMA_LENGTH_SB)

#define TFC_MPC_TBL_RDCLR_CMPL_OPAQUE_EB           63
#define TFC_MPC_TBL_RDCLR_CMPL_OPAQUE_SB           32
#define TFC_MPC_TBL_RDCLR_CMPL_OPAQUE_OFFS         0x0

#define TFC_MPC_TBL_RDCLR_CMPL_SET_OPAQUE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_OPAQUE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RDCLR_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_OPAQUE_SB)
#define TFC_MPC_TBL_RDCLR_CMPL_GET_OPAQUE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_OPAQUE_OFFS), \
		     TFC_MPC_TBL_RDCLR_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_OPAQUE_SB)

#define TFC_MPC_TBL_RDCLR_CMPL_V_EB           0
#define TFC_MPC_TBL_RDCLR_CMPL_V_SB           0
#define TFC_MPC_TBL_RDCLR_CMPL_V_OFFS         0x8

#define TFC_MPC_TBL_RDCLR_CMPL_SET_V(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_V_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RDCLR_CMPL_V_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_V_SB)
#define TFC_MPC_TBL_RDCLR_CMPL_GET_V(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_V_OFFS), \
		     TFC_MPC_TBL_RDCLR_CMPL_V_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_V_SB)

#define TFC_MPC_TBL_RDCLR_CMPL_HASH_MSB_EB           15
#define TFC_MPC_TBL_RDCLR_CMPL_HASH_MSB_SB           4
#define TFC_MPC_TBL_RDCLR_CMPL_HASH_MSB_OFFS         0x8

#define TFC_MPC_TBL_RDCLR_CMPL_SET_HASH_MSB(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_HASH_MSB_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RDCLR_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_HASH_MSB_SB)
#define TFC_MPC_TBL_RDCLR_CMPL_GET_HASH_MSB(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_HASH_MSB_OFFS), \
		     TFC_MPC_TBL_RDCLR_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_HASH_MSB_SB)

#define TFC_MPC_TBL_RDCLR_CMPL_TABLE_TYPE_ACTION    0
#define TFC_MPC_TBL_RDCLR_CMPL_TABLE_TYPE_EM        1

#define TFC_MPC_TBL_RDCLR_CMPL_TABLE_TYPE_EB           23
#define TFC_MPC_TBL_RDCLR_CMPL_TABLE_TYPE_SB           20
#define TFC_MPC_TBL_RDCLR_CMPL_TABLE_TYPE_OFFS         0x8

#define TFC_MPC_TBL_RDCLR_CMPL_SET_TABLE_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_TABLE_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RDCLR_CMPL_TABLE_TYPE_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_TABLE_TYPE_SB)
#define TFC_MPC_TBL_RDCLR_CMPL_GET_TABLE_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_TABLE_TYPE_OFFS), \
		     TFC_MPC_TBL_RDCLR_CMPL_TABLE_TYPE_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_TABLE_TYPE_SB)

#define TFC_MPC_TBL_RDCLR_CMPL_TABLE_SCOPE_EB           28
#define TFC_MPC_TBL_RDCLR_CMPL_TABLE_SCOPE_SB           24
#define TFC_MPC_TBL_RDCLR_CMPL_TABLE_SCOPE_OFFS         0x8

#define TFC_MPC_TBL_RDCLR_CMPL_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_TABLE_SCOPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RDCLR_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_TABLE_SCOPE_SB)
#define TFC_MPC_TBL_RDCLR_CMPL_GET_TABLE_SCOPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_TABLE_SCOPE_OFFS), \
		     TFC_MPC_TBL_RDCLR_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_TABLE_SCOPE_SB)

#define TFC_MPC_TBL_RDCLR_CMPL_TABLE_INDEX_EB           57
#define TFC_MPC_TBL_RDCLR_CMPL_TABLE_INDEX_SB           32
#define TFC_MPC_TBL_RDCLR_CMPL_TABLE_INDEX_OFFS         0x8

#define TFC_MPC_TBL_RDCLR_CMPL_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_TABLE_INDEX_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_RDCLR_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_TABLE_INDEX_SB)
#define TFC_MPC_TBL_RDCLR_CMPL_GET_TABLE_INDEX(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_RDCLR_CMPL_TABLE_INDEX_OFFS), \
		     TFC_MPC_TBL_RDCLR_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_RDCLR_CMPL_TABLE_INDEX_SB)

#define TFC_MPC_TBL_RDCLR_CMPL_SIZE     16

/*
 * CFA Table Invalidate Completion Record:
 *
 * Returns status for INVALIDATE commands.
 * Offset  63              0
 * 0x0     opaque  unused(8)       opcode  mp_client       status  unused(2)
 * type
 * 0x8     unused(6)       table_index     unused(3)       table_scope
 * table_type      unused(4)       hash_msb        unused(3)       v
 *
 *    type (Offset:0x0[5:0], Size: 6)
 *       This field indicates the exact type of the
 *       completion. By convention, the LSB identifies the
 *       length of the record in 16B units. Even values
 *       indicate 16B records. Odd values indicate 32B
 *       records **(EXCEPT no_op!!!!)** .
 *       Value   Enum    Enumeration Description
 *          30      mid_path_short
 *          Mid Path Short Completion : Completion of a Mid
 *          Path Command. Length = 16B
 *
 *    status (Offset:0x0[11:8], Size: 4)
 *       The command processing status.
 *       Value   Enum    Enumeration Description
 *          0       OK
 *          Completed without error.
 *          1       UNSPRT_ERR
 *          The CFA OPCODE is an unsupported value.
 *          2       FMT_ERR
 *          Indicates a CFA command formatting error. This
 *          error can occur on any of the supported CFA
 *          commands.
 *          Error conditions:
 *          DATA_SIZE[2:0] outside range of [1, 4]. (Does
 *          not apply to READ_CLR, EM_DELETE, or EM_CHAIN
 *          commands as they do not have a DATA_SIZE field)
 *          HOST_ADDRESS[1:0] != 0 (Only applies to READ,
 *          READ_CLR, and EVENT_COLLECTION as other commands
 *          do not have a HOST_ADDRESS field.
 *          3       SCOPE_ERR
 *          Access to TABLE_SCOPE is disabled for the SVIF.
 *          Indates that the bit indexed by (SVIF,
 *          TABLE_SCOPE) in the TAI_SVIF_SCOPE memory is set
 *          to 0.
 *          4       ADDR_ERR
 *          This error can only occur for commands having
 *          TABLE_TYPE present and set to EM and not having
 *          any of the previous errors, or for any of the
 *          EM* commands, for which a TABLE_TYPE of EM is
 *          implied.
 *          It indicates that an EM address (TABLE_INDEX*)
 *          in the command is invalid based on (EM_BUCKETS,
 *          EM_SIZE) parameters configured for TABLE_SCOPE.
 *          All addresses must be in the range [0,
 *          EM_SIZE). Static bucket addresses must be within
 *          the range determined by EM_BUCKETS. Dynamic
 *          bucket addresses and entries must be outside of
 *          the static bucket range.
 *          5       CACHE_ERR
 *          One of more cache responses signaled an error
 *          while processing the command.
 *
 *    mp_client (Offset:0x0[15:12], Size: 4)
 *       This field represents the Mid-Path client that
 *       generated the completion.
 *       Value   Enum    Enumeration Description
 *          2       TE_CFA
 *          TE-CFA
 *          3       RE_CFA
 *          RE-CFA
 *
 *    opcode (Offset:0x0[23:16], Size: 8)
 *       OPCODE from the command.
 *       Value   Enum    Enumeration Description
 *          5       INVALIDATE
 *          This command invalidates 1-4 consecutively-
 *          addressed 32B words in the cache.
 *
 *    opaque (Offset:0x0[63:32], Size: 32)
 *       This is a copy of the opaque field from the mid
 *       path BD of this command.
 *
 *    v (Offset:0x8[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    hash_msb (Offset:0x8[15:4], Size: 12)
 *       For EM_SEARCH and EM_INSERT commands without
 *       errors that abort the command processing prior to
 *       the hash computation, set to HASH[35:24] of the
 *       hash computed from the exact match entry key in the
 *       command.
 *       For all other cases, set to 0 except for the
 *       following error conditions, which carry debug
 *       information in this field as shown by error status
 *       below:
 *       FMT_ERR:
 *       Set to {7'd0, HOST_ADDRESS[1:0],
 *       DATA_SIZE[2:0]}.
 *       If HOST_ADDRESS or DATA_SIZE field not
 *       present they are set to 0.
 *       SCOPE_ERR:
 *       Set to {1'b0, SVIF[10:0]}.
 *       ADDR_ERR:
 *       Only possible when TABLE_TYPE=EM or for EM*
 *       commands
 *       Set to {1'b0, TABLE_INDEX[2:0], 5'd0,
 *       DATA_SIZE[2:0]}
 *       TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error
 *       TABLE_INDEX[1]=1 if TABLE_INDEX2 had an error
 *       TABLE_INDEX[0]=1 if TABLE_INDEX had an error
 *       TABLE_INDEX[n]=0 if the completion does not
 *       have the corresponding TABLE_INDEX field above.
 *       CACHE_ERR:
 *       Set to {9'd0, DATA_SIZE[2:0]}
 *
 *    table_type (Offset:0x8[23:20], Size: 4)
 *       TABLE_TYPE from the command.
 *       Value   Enum    Enumeration Description
 *          0       ACTION
 *          This command acts on the action table of the
 *          specified scope.
 *          1       EM
 *          This command acts on the exact match table of
 *          the specified scope.
 *
 *    table_scope (Offset:0x8[28:24], Size: 5)
 *       TABLE_SCOPE from the command.
 *
 *    table_index (Offset:0x8[57:32], Size: 26)
 *       TABLE_INDEX from the command.
 *       This structure is used to inform the host of an
 *       event within the NIC.
 */
#define TFC_MPC_TBL_INV_CMPL_TYPE_MID_PATH_SHORT    30

#define TFC_MPC_TBL_INV_CMPL_TYPE_EB           5
#define TFC_MPC_TBL_INV_CMPL_TYPE_SB           0
#define TFC_MPC_TBL_INV_CMPL_TYPE_OFFS         0x0

#define TFC_MPC_TBL_INV_CMPL_SET_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_INV_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_INV_CMPL_TYPE_SB)
#define TFC_MPC_TBL_INV_CMPL_GET_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_TYPE_OFFS), \
		     TFC_MPC_TBL_INV_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_INV_CMPL_TYPE_SB)

#define TFC_MPC_TBL_INV_CMPL_STATUS_OK            0
#define TFC_MPC_TBL_INV_CMPL_STATUS_UNSPRT_ERR    1
#define TFC_MPC_TBL_INV_CMPL_STATUS_FMT_ERR       2
#define TFC_MPC_TBL_INV_CMPL_STATUS_SCOPE_ERR     3
#define TFC_MPC_TBL_INV_CMPL_STATUS_ADDR_ERR      4
#define TFC_MPC_TBL_INV_CMPL_STATUS_CACHE_ERR     5

#define TFC_MPC_TBL_INV_CMPL_STATUS_EB           11
#define TFC_MPC_TBL_INV_CMPL_STATUS_SB           8
#define TFC_MPC_TBL_INV_CMPL_STATUS_OFFS         0x0

#define TFC_MPC_TBL_INV_CMPL_SET_STATUS(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_STATUS_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_INV_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_INV_CMPL_STATUS_SB)
#define TFC_MPC_TBL_INV_CMPL_GET_STATUS(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_STATUS_OFFS), \
		     TFC_MPC_TBL_INV_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_INV_CMPL_STATUS_SB)

#define TFC_MPC_TBL_INV_CMPL_MP_CLIENT_TE_CFA    2
#define TFC_MPC_TBL_INV_CMPL_MP_CLIENT_RE_CFA    3

#define TFC_MPC_TBL_INV_CMPL_MP_CLIENT_EB           15
#define TFC_MPC_TBL_INV_CMPL_MP_CLIENT_SB           12
#define TFC_MPC_TBL_INV_CMPL_MP_CLIENT_OFFS         0x0

#define TFC_MPC_TBL_INV_CMPL_SET_MP_CLIENT(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_MP_CLIENT_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_INV_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_INV_CMPL_MP_CLIENT_SB)
#define TFC_MPC_TBL_INV_CMPL_GET_MP_CLIENT(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_MP_CLIENT_OFFS), \
		     TFC_MPC_TBL_INV_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_INV_CMPL_MP_CLIENT_SB)

#define TFC_MPC_CMD_OPCODE_INVALIDATE    5

#define TFC_MPC_TBL_INV_CMPL_OPCODE_EB           23
#define TFC_MPC_TBL_INV_CMPL_OPCODE_SB           16
#define TFC_MPC_TBL_INV_CMPL_OPCODE_OFFS         0x0

#define TFC_MPC_TBL_INV_CMPL_SET_OPCODE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_OPCODE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_INV_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_INV_CMPL_OPCODE_SB)
#define TFC_MPC_TBL_INV_CMPL_GET_OPCODE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_OPCODE_OFFS), \
		     TFC_MPC_TBL_INV_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_INV_CMPL_OPCODE_SB)

#define TFC_MPC_TBL_INV_CMPL_OPAQUE_EB           63
#define TFC_MPC_TBL_INV_CMPL_OPAQUE_SB           32
#define TFC_MPC_TBL_INV_CMPL_OPAQUE_OFFS         0x0

#define TFC_MPC_TBL_INV_CMPL_SET_OPAQUE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_OPAQUE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_INV_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_INV_CMPL_OPAQUE_SB)
#define TFC_MPC_TBL_INV_CMPL_GET_OPAQUE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_OPAQUE_OFFS), \
		     TFC_MPC_TBL_INV_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_INV_CMPL_OPAQUE_SB)

#define TFC_MPC_TBL_INV_CMPL_V_EB           0
#define TFC_MPC_TBL_INV_CMPL_V_SB           0
#define TFC_MPC_TBL_INV_CMPL_V_OFFS         0x8

#define TFC_MPC_TBL_INV_CMPL_SET_V(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_V_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_INV_CMPL_V_EB, \
		     TFC_MPC_TBL_INV_CMPL_V_SB)
#define TFC_MPC_TBL_INV_CMPL_GET_V(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_V_OFFS), \
		     TFC_MPC_TBL_INV_CMPL_V_EB, \
		     TFC_MPC_TBL_INV_CMPL_V_SB)

#define TFC_MPC_TBL_INV_CMPL_HASH_MSB_EB           15
#define TFC_MPC_TBL_INV_CMPL_HASH_MSB_SB           4
#define TFC_MPC_TBL_INV_CMPL_HASH_MSB_OFFS         0x8

#define TFC_MPC_TBL_INV_CMPL_SET_HASH_MSB(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_HASH_MSB_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_INV_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_INV_CMPL_HASH_MSB_SB)
#define TFC_MPC_TBL_INV_CMPL_GET_HASH_MSB(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_HASH_MSB_OFFS), \
		     TFC_MPC_TBL_INV_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_INV_CMPL_HASH_MSB_SB)

#define TFC_MPC_TBL_INV_CMPL_TABLE_TYPE_ACTION    0
#define TFC_MPC_TBL_INV_CMPL_TABLE_TYPE_EM        1

#define TFC_MPC_TBL_INV_CMPL_TABLE_TYPE_EB           23
#define TFC_MPC_TBL_INV_CMPL_TABLE_TYPE_SB           20
#define TFC_MPC_TBL_INV_CMPL_TABLE_TYPE_OFFS         0x8

#define TFC_MPC_TBL_INV_CMPL_SET_TABLE_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_TABLE_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_INV_CMPL_TABLE_TYPE_EB, \
		     TFC_MPC_TBL_INV_CMPL_TABLE_TYPE_SB)
#define TFC_MPC_TBL_INV_CMPL_GET_TABLE_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_TABLE_TYPE_OFFS), \
		     TFC_MPC_TBL_INV_CMPL_TABLE_TYPE_EB, \
		     TFC_MPC_TBL_INV_CMPL_TABLE_TYPE_SB)

#define TFC_MPC_TBL_INV_CMPL_TABLE_SCOPE_EB           28
#define TFC_MPC_TBL_INV_CMPL_TABLE_SCOPE_SB           24
#define TFC_MPC_TBL_INV_CMPL_TABLE_SCOPE_OFFS         0x8

#define TFC_MPC_TBL_INV_CMPL_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_TABLE_SCOPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_INV_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_INV_CMPL_TABLE_SCOPE_SB)
#define TFC_MPC_TBL_INV_CMPL_GET_TABLE_SCOPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_TABLE_SCOPE_OFFS), \
		     TFC_MPC_TBL_INV_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_INV_CMPL_TABLE_SCOPE_SB)

#define TFC_MPC_TBL_INV_CMPL_TABLE_INDEX_EB           57
#define TFC_MPC_TBL_INV_CMPL_TABLE_INDEX_SB           32
#define TFC_MPC_TBL_INV_CMPL_TABLE_INDEX_OFFS         0x8

#define TFC_MPC_TBL_INV_CMPL_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_TABLE_INDEX_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_INV_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_INV_CMPL_TABLE_INDEX_SB)
#define TFC_MPC_TBL_INV_CMPL_GET_TABLE_INDEX(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_INV_CMPL_TABLE_INDEX_OFFS), \
		     TFC_MPC_TBL_INV_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_INV_CMPL_TABLE_INDEX_SB)

#define TFC_MPC_TBL_INV_CMPL_SIZE     16

/*
 * CFA Table Event Collection Completion Record:
 *
 * For OK status, returns 1-16 8B Host Notification Record for TABLE_SCOPE,
 * where the maximum number is limited by DATA_SIZE from the command (see
 * command for details). Returns EVENT_COLLECTION_FAIL status and no DMA data
 * when there are no messages available.
 * Offset  63              0
 * 0x0     opaque  dma_length      opcode  mp_client       status  unused(2)
 * type
 * 0x8     unused(35)      table_scope     unused(8)       hash_msb
 * unused(3)       v
 *
 *    type (Offset:0x0[5:0], Size: 6)
 *       This field indicates the exact type of the
 *       completion. By convention, the LSB identifies the
 *       length of the record in 16B units. Even values
 *       indicate 16B records. Odd values indicate 32B
 *       records **(EXCEPT no_op!!!!)** .
 *       Value   Enum    Enumeration Description
 *          30      mid_path_short
 *          Mid Path Short Completion : Completion of a Mid
 *          Path Command. Length = 16B
 *
 *    status (Offset:0x0[11:8], Size: 4)
 *       The command processing status.
 *       Value   Enum    Enumeration Description
 *          0       OK
 *          Completed without error.
 *          1       UNSPRT_ERR
 *          The CFA OPCODE is an unsupported value.
 *          2       FMT_ERR
 *          Indicates a CFA command formatting error. This
 *          error can occur on any of the supported CFA
 *          commands.
 *          Error conditions:
 *          DATA_SIZE[2:0] outside range of [1, 4]. (Does
 *          not apply to READ_CLR, EM_DELETE, or EM_CHAIN
 *          commands as they do not have a DATA_SIZE field)
 *          HOST_ADDRESS[1:0] != 0 (Only applies to READ,
 *          READ_CLR, and EVENT_COLLECTION as other commands
 *          do not have a HOST_ADDRESS field.
 *          3       SCOPE_ERR
 *          Access to TABLE_SCOPE is disabled for the SVIF.
 *          Indates that the bit indexed by (SVIF,
 *          TABLE_SCOPE) in the TAI_SVIF_SCOPE memory is set
 *          to 0.
 *          8       EVENT_COLLECTION_FAIL
 *          The TABLE_SCOPE had no host notification
 *          messages to return.
 *
 *    mp_client (Offset:0x0[15:12], Size: 4)
 *       This field represents the Mid-Path client that
 *       generated the completion.
 *       Value   Enum    Enumeration Description
 *          2       TE_CFA
 *          TE-CFA
 *          3       RE_CFA
 *          RE-CFA
 *
 *    opcode (Offset:0x0[23:16], Size: 8)
 *       OPCODE from the command.
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
 *    dma_length (Offset:0x0[31:24], Size: 8)
 *       The length of the DMA that accompanies the
 *       completion in units of DWORDs (32b). Valid values
 *       are [0, 128]. A value of zero indicates that there
 *       is no DMA that accompanies the completion.
 *
 *    opaque (Offset:0x0[63:32], Size: 32)
 *       This is a copy of the opaque field from the mid
 *       path BD of this command.
 *
 *    v (Offset:0x8[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    hash_msb (Offset:0x8[15:4], Size: 12)
 *       For EM_SEARCH and EM_INSERT commands without
 *       errors that abort the command processing prior to
 *       the hash computation, set to HASH[35:24] of the
 *       hash computed from the exact match entry key in the
 *       command.
 *       For all other cases, set to 0 except for the
 *       following error conditions, which carry debug
 *       information in this field as shown by error status
 *       below:
 *       FMT_ERR:
 *       Set to {7'd0, HOST_ADDRESS[1:0],
 *       DATA_SIZE[2:0]}.
 *       If HOST_ADDRESS or DATA_SIZE field not
 *       present they are set to 0.
 *       SCOPE_ERR:
 *       Set to {1'b0, SVIF[10:0]}.
 *       ADDR_ERR:
 *       Only possible when TABLE_TYPE=EM or for EM*
 *       commands
 *       Set to {1'b0, TABLE_INDEX[2:0], 5'd0,
 *       DATA_SIZE[2:0]}
 *       TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error
 *       TABLE_INDEX[1]=1 if TABLE_INDEX2 had an error
 *       TABLE_INDEX[0]=1 if TABLE_INDEX had an error
 *       TABLE_INDEX[n]=0 if the completion does not
 *       have the corresponding TABLE_INDEX field above.
 *       CACHE_ERR:
 *       Set to {9'd0, DATA_SIZE[2:0]}
 *
 *    table_scope (Offset:0x8[28:24], Size: 5)
 *       TABLE_SCOPE from the command.
 *       This structure is used to inform the host of an
 *       event within the NIC.
 */
#define TFC_MPC_TBL_EVENT_COLL_CMPL_TYPE_MID_PATH_SHORT    30

#define TFC_MPC_TBL_EVENT_COLL_CMPL_TYPE_EB           5
#define TFC_MPC_TBL_EVENT_COLL_CMPL_TYPE_SB           0
#define TFC_MPC_TBL_EVENT_COLL_CMPL_TYPE_OFFS         0x0

#define TFC_MPC_TBL_EVENT_COLL_CMPL_SET_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_TYPE_SB)
#define TFC_MPC_TBL_EVENT_COLL_CMPL_GET_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_TYPE_OFFS), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_TYPE_SB)

#define TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_OK                       0
#define TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_UNSPRT_ERR               1
#define TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_FMT_ERR                  2
#define TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_SCOPE_ERR                3
#define TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_EVENT_COLLECTION_FAIL    8

#define TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_EB           11
#define TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_SB           8
#define TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_OFFS         0x0

#define TFC_MPC_TBL_EVENT_COLL_CMPL_SET_STATUS(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_SB)
#define TFC_MPC_TBL_EVENT_COLL_CMPL_GET_STATUS(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_OFFS), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_STATUS_SB)

#define TFC_MPC_TBL_EVENT_COLL_CMPL_MP_CLIENT_TE_CFA    2
#define TFC_MPC_TBL_EVENT_COLL_CMPL_MP_CLIENT_RE_CFA    3

#define TFC_MPC_TBL_EVENT_COLL_CMPL_MP_CLIENT_EB           15
#define TFC_MPC_TBL_EVENT_COLL_CMPL_MP_CLIENT_SB           12
#define TFC_MPC_TBL_EVENT_COLL_CMPL_MP_CLIENT_OFFS         0x0

#define TFC_MPC_TBL_EVENT_COLL_CMPL_SET_MP_CLIENT(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_MP_CLIENT_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_MP_CLIENT_SB)
#define TFC_MPC_TBL_EVENT_COLL_CMPL_GET_MP_CLIENT(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_MP_CLIENT_OFFS), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_MP_CLIENT_SB)

#define TFC_MPC_CMD_OPCODE_EVENT_COLLECTION    6

#define TFC_MPC_TBL_EVENT_COLL_CMPL_OPCODE_EB           23
#define TFC_MPC_TBL_EVENT_COLL_CMPL_OPCODE_SB           16
#define TFC_MPC_TBL_EVENT_COLL_CMPL_OPCODE_OFFS         0x0

#define TFC_MPC_TBL_EVENT_COLL_CMPL_SET_OPCODE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_OPCODE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_OPCODE_SB)
#define TFC_MPC_TBL_EVENT_COLL_CMPL_GET_OPCODE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_OPCODE_OFFS), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_OPCODE_SB)

#define TFC_MPC_TBL_EVENT_COLL_CMPL_DMA_LENGTH_EB           31
#define TFC_MPC_TBL_EVENT_COLL_CMPL_DMA_LENGTH_SB           24
#define TFC_MPC_TBL_EVENT_COLL_CMPL_DMA_LENGTH_OFFS         0x0

#define TFC_MPC_TBL_EVENT_COLL_CMPL_SET_DMA_LENGTH(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_DMA_LENGTH_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_DMA_LENGTH_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_DMA_LENGTH_SB)
#define TFC_MPC_TBL_EVENT_COLL_CMPL_GET_DMA_LENGTH(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_DMA_LENGTH_OFFS), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_DMA_LENGTH_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_DMA_LENGTH_SB)

#define TFC_MPC_TBL_EVENT_COLL_CMPL_OPAQUE_EB           63
#define TFC_MPC_TBL_EVENT_COLL_CMPL_OPAQUE_SB           32
#define TFC_MPC_TBL_EVENT_COLL_CMPL_OPAQUE_OFFS         0x0

#define TFC_MPC_TBL_EVENT_COLL_CMPL_SET_OPAQUE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_OPAQUE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_OPAQUE_SB)
#define TFC_MPC_TBL_EVENT_COLL_CMPL_GET_OPAQUE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_OPAQUE_OFFS), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_OPAQUE_SB)

#define TFC_MPC_TBL_EVENT_COLL_CMPL_V_EB           0
#define TFC_MPC_TBL_EVENT_COLL_CMPL_V_SB           0
#define TFC_MPC_TBL_EVENT_COLL_CMPL_V_OFFS         0x8

#define TFC_MPC_TBL_EVENT_COLL_CMPL_SET_V(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_V_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_V_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_V_SB)
#define TFC_MPC_TBL_EVENT_COLL_CMPL_GET_V(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_V_OFFS), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_V_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_V_SB)

#define TFC_MPC_TBL_EVENT_COLL_CMPL_HASH_MSB_EB           15
#define TFC_MPC_TBL_EVENT_COLL_CMPL_HASH_MSB_SB           4
#define TFC_MPC_TBL_EVENT_COLL_CMPL_HASH_MSB_OFFS         0x8

#define TFC_MPC_TBL_EVENT_COLL_CMPL_SET_HASH_MSB(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_HASH_MSB_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_HASH_MSB_SB)
#define TFC_MPC_TBL_EVENT_COLL_CMPL_GET_HASH_MSB(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_HASH_MSB_OFFS), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_HASH_MSB_SB)

#define TFC_MPC_TBL_EVENT_COLL_CMPL_TABLE_SCOPE_EB           28
#define TFC_MPC_TBL_EVENT_COLL_CMPL_TABLE_SCOPE_SB           24
#define TFC_MPC_TBL_EVENT_COLL_CMPL_TABLE_SCOPE_OFFS         0x8

#define TFC_MPC_TBL_EVENT_COLL_CMPL_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_TABLE_SCOPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_TABLE_SCOPE_SB)
#define TFC_MPC_TBL_EVENT_COLL_CMPL_GET_TABLE_SCOPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EVENT_COLL_CMPL_TABLE_SCOPE_OFFS), \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_EVENT_COLL_CMPL_TABLE_SCOPE_SB)

#define TFC_MPC_TBL_EVENT_COLL_CMPL_SIZE     16

/*
 * CFA Table EM Search Completion Record:
 *
 * For OK status, returns the index of the matching entry found for the EM key
 * supplied in the command. Returns EM_MISS status if no match was found.
 * Offset  63              0
 * 0x0     opaque  unused(8)       opcode  mp_client       status  unused(2)
 * type
 * 0x8     unused(6)       table_index     unused(3)       table_scope
 * unused(8)       hash_msb        unused(3)       v1
 * 0x10    unused(38)      table_index2
 * 0x18    unused(21)      num_entries     bkt_num unused(31)      v2
 *
 *    type (Offset:0x0[5:0], Size: 6)
 *       This field indicates the exact type of the
 *       completion. By convention, the LSB identifies the
 *       length of the record in 16B units. Even values
 *       indicate 16B records. Odd values indicate 32B
 *       records **(EXCEPT no_op!!!!)** .
 *       Value   Enum    Enumeration Description
 *          31      mid_path_long
 *          Mid Path Long Completion : Completion of a Mid
 *          Path Command. Length = 32B
 *
 *    status (Offset:0x0[11:8], Size: 4)
 *       The command processing status.
 *       Value   Enum    Enumeration Description
 *          0       OK
 *          Completed without error.
 *          1       UNSPRT_ERR
 *          The CFA OPCODE is an unsupported value.
 *          2       FMT_ERR
 *          Indicates a CFA command formatting error. This
 *          error can occur on any of the supported CFA
 *          commands.
 *          Error conditions:
 *          DATA_SIZE[2:0] outside range of [1, 4]. (Does
 *          not apply to READ_CLR, EM_DELETE, or EM_CHAIN
 *          commands as they do not have a DATA_SIZE field)
 *          HOST_ADDRESS[1:0] != 0 (Only applies to READ,
 *          READ_CLR, and EVENT_COLLECTION as other commands
 *          do not have a HOST_ADDRESS field.
 *          3       SCOPE_ERR
 *          Access to TABLE_SCOPE is disabled for the SVIF.
 *          Indates that the bit indexed by (SVIF,
 *          TABLE_SCOPE) in the TAI_SVIF_SCOPE memory is set
 *          to 0.
 *          4       ADDR_ERR
 *          This error can only occur for commands having
 *          TABLE_TYPE present and set to EM and not having
 *          any of the previous errors, or for any of the
 *          EM* commands, for which a TABLE_TYPE of EM is
 *          implied.
 *          It indicates that an EM address (TABLE_INDEX*)
 *          in the command is invalid based on (EM_BUCKETS,
 *          EM_SIZE) parameters configured for TABLE_SCOPE.
 *          All addresses must be in the range [0,
 *          EM_SIZE). Static bucket addresses must be within
 *          the range determined by EM_BUCKETS. Dynamic
 *          bucket addresses and entries must be outside of
 *          the static bucket range.
 *          5       CACHE_ERR
 *          One of more cache responses signaled an error
 *          while processing the command.
 *          6       EM_MISS
 *          No matching entry found.
 *
 *    mp_client (Offset:0x0[15:12], Size: 4)
 *       This field represents the Mid-Path client that
 *       generated the completion.
 *       Value   Enum    Enumeration Description
 *          2       TE_CFA
 *          TE-CFA
 *          3       RE_CFA
 *          RE-CFA
 *
 *    opcode (Offset:0x0[23:16], Size: 8)
 *       OPCODE from the command.
 *       Value   Enum    Enumeration Description
 *          8       EM_SEARCH
 *          This command supplies an exact match entry of
 *          1-4 32B words to search for in the exact match
 *          table.
 *
 *    opaque (Offset:0x0[63:32], Size: 32)
 *       This is a copy of the opaque field from the mid
 *       path BD of this command.
 *
 *    v1 (Offset:0x8[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    hash_msb (Offset:0x8[15:4], Size: 12)
 *       For EM_SEARCH and EM_INSERT commands without
 *       errors that abort the command processing prior to
 *       the hash computation, set to HASH[35:24] of the
 *       hash computed from the exact match entry key in the
 *       command.
 *       For all other cases, set to 0 except for the
 *       following error conditions, which carry debug
 *       information in this field as shown by error status
 *       below:
 *       FMT_ERR:
 *       Set to {7'd0, HOST_ADDRESS[1:0],
 *       DATA_SIZE[2:0]}.
 *       If HOST_ADDRESS or DATA_SIZE field not
 *       present they are set to 0.
 *       SCOPE_ERR:
 *       Set to {1'b0, SVIF[10:0]}.
 *       ADDR_ERR:
 *       Only possible when TABLE_TYPE=EM or for EM*
 *       commands
 *       Set to {1'b0, TABLE_INDEX[2:0], 5'd0,
 *       DATA_SIZE[2:0]}
 *       TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error
 *       TABLE_INDEX[1]=1 if TABLE_INDEX2 had an error
 *       TABLE_INDEX[0]=1 if TABLE_INDEX had an error
 *       TABLE_INDEX[n]=0 if the completion does not
 *       have the corresponding TABLE_INDEX field above.
 *       CACHE_ERR:
 *       Set to {9'd0, DATA_SIZE[2:0]}
 *
 *    table_scope (Offset:0x8[28:24], Size: 5)
 *       TABLE_SCOPE from the command.
 *
 *    table_index (Offset:0x8[57:32], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       For OK status, gives ENTRY_PTR[25:0] of the
 *       matching entry found. Otherwise, set to 0.
 *
 *    table_index2 (Offset:0x10[25:0], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       If the hash is computed (no errors during initial
 *       processing of the command), TABLE_INDEX2[23:0] is
 *       the static bucket address determined from the hash
 *       of the exact match entry key in the command and the
 *       (EM_SIZE, EM_BUCKETS) configuration for TABLE_SCOPE
 *       of the command. Bits 25:24 in this case are set to
 *       0. For any other status, it is always 0.
 *
 *    v2 (Offset:0x18[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    bkt_num (Offset:0x18[39:32], Size: 8)
 *       BKT_NUM is the bucket number in chain of the tail
 *       bucket after finishing processing the command,
 *       except when the command stops processing before the
 *       tail bucket. NUM_ENTRIES is the number of valid
 *       entries in the BKT_NUM bucket. The following
 *       describes the cases where BKT_NUM and NUM_ENTRIES
 *       are not for the tail bucket after finishing
 *       processing of the command:
 *       For UNSPRT_ERR, FMT_ERR, SCOPE_ERR, or ADDR_ERR
 *       completion status, BKT_NUM will be set to 0.
 *       For CACHE_ERR completion status, BKT_NUM will be
 *       set to the bucket number that was last read without
 *       error. If ERR=1 in the response to the static
 *       bucket read, BKT_NUM and NUM_ENTRIES are set to 0.
 *       The static bucket is number 0, BKT_NUM increments
 *       for each new bucket in the chain, and saturates at
 *       255. Therefore, if the value is 255, BKT_NUM may or
 *       may not be accurate. In this case, though,
 *       NUM_ENTRIES will still be the correct value as
 *       described above for the bucket.
 *       For OK status, which indicates a matching entry
 *       was found, BKT_NUM and NUM_ENTRIES are for the
 *       bucket containing the match entry, which may or may
 *       not be the tail bucket. For EM_MISS status, the
 *       values are always for the tail bucket.
 *
 *    num_entries (Offset:0x18[42:40], Size: 3)
 *       See BKT_NUM description.
 *       This structure is used to inform the host of an
 *       event within the NIC.
 */
#define TFC_MPC_TBL_EM_SEARCH_CMPL_TYPE_MID_PATH_LONG    31

#define TFC_MPC_TBL_EM_SEARCH_CMPL_TYPE_EB           5
#define TFC_MPC_TBL_EM_SEARCH_CMPL_TYPE_SB           0
#define TFC_MPC_TBL_EM_SEARCH_CMPL_TYPE_OFFS         0x0

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TYPE_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_TYPE_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TYPE_SB)

#define TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_OK            0
#define TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_UNSPRT_ERR    1
#define TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_FMT_ERR       2
#define TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_SCOPE_ERR     3
#define TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_ADDR_ERR      4
#define TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_CACHE_ERR     5
#define TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_EM_MISS       6

#define TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_EB           11
#define TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_SB           8
#define TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_OFFS         0x0

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_STATUS(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_STATUS(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_STATUS_SB)

#define TFC_MPC_TBL_EM_SEARCH_CMPL_MP_CLIENT_TE_CFA    2
#define TFC_MPC_TBL_EM_SEARCH_CMPL_MP_CLIENT_RE_CFA    3

#define TFC_MPC_TBL_EM_SEARCH_CMPL_MP_CLIENT_EB           15
#define TFC_MPC_TBL_EM_SEARCH_CMPL_MP_CLIENT_SB           12
#define TFC_MPC_TBL_EM_SEARCH_CMPL_MP_CLIENT_OFFS         0x0

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_MP_CLIENT(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_MP_CLIENT_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_MP_CLIENT_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_MP_CLIENT(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_MP_CLIENT_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_MP_CLIENT_SB)

#define TFC_MPC_CMD_OPCODE_EM_SEARCH    8

#define TFC_MPC_TBL_EM_SEARCH_CMPL_OPCODE_EB           23
#define TFC_MPC_TBL_EM_SEARCH_CMPL_OPCODE_SB           16
#define TFC_MPC_TBL_EM_SEARCH_CMPL_OPCODE_OFFS         0x0

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_OPCODE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_OPCODE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_OPCODE_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_OPCODE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_OPCODE_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_OPCODE_SB)

#define TFC_MPC_TBL_EM_SEARCH_CMPL_OPAQUE_EB           63
#define TFC_MPC_TBL_EM_SEARCH_CMPL_OPAQUE_SB           32
#define TFC_MPC_TBL_EM_SEARCH_CMPL_OPAQUE_OFFS         0x0

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_OPAQUE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_OPAQUE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_OPAQUE_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_OPAQUE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_OPAQUE_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_OPAQUE_SB)

#define TFC_MPC_TBL_EM_SEARCH_CMPL_V1_EB           0
#define TFC_MPC_TBL_EM_SEARCH_CMPL_V1_SB           0
#define TFC_MPC_TBL_EM_SEARCH_CMPL_V1_OFFS         0x8

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_V1(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_V1_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_V1_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_V1_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_V1(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_V1_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_V1_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_V1_SB)

#define TFC_MPC_TBL_EM_SEARCH_CMPL_HASH_MSB_EB           15
#define TFC_MPC_TBL_EM_SEARCH_CMPL_HASH_MSB_SB           4
#define TFC_MPC_TBL_EM_SEARCH_CMPL_HASH_MSB_OFFS         0x8

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_HASH_MSB(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_HASH_MSB_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_HASH_MSB_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_HASH_MSB(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_HASH_MSB_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_HASH_MSB_SB)

#define TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_SCOPE_EB           28
#define TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_SCOPE_SB           24
#define TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_SCOPE_OFFS         0x8

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_SCOPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_SCOPE_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_TABLE_SCOPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_SCOPE_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_SCOPE_SB)

#define TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX_EB           57
#define TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX_SB           32
#define TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX_OFFS         0x8

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_TABLE_INDEX(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX_SB)

#define TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX2_EB           25
#define TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX2_SB           0
#define TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX2_OFFS         0x10

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_TABLE_INDEX2(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX2_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX2_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX2_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_TABLE_INDEX2(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX2_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX2_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_TABLE_INDEX2_SB)

#define TFC_MPC_TBL_EM_SEARCH_CMPL_V2_EB           0
#define TFC_MPC_TBL_EM_SEARCH_CMPL_V2_SB           0
#define TFC_MPC_TBL_EM_SEARCH_CMPL_V2_OFFS         0x18

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_V2(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_V2_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_V2_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_V2_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_V2(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_V2_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_V2_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_V2_SB)

#define TFC_MPC_TBL_EM_SEARCH_CMPL_BKT_NUM_EB           39
#define TFC_MPC_TBL_EM_SEARCH_CMPL_BKT_NUM_SB           32
#define TFC_MPC_TBL_EM_SEARCH_CMPL_BKT_NUM_OFFS         0x18

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_BKT_NUM(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_BKT_NUM_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_BKT_NUM_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_BKT_NUM_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_BKT_NUM(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_BKT_NUM_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_BKT_NUM_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_BKT_NUM_SB)

#define TFC_MPC_TBL_EM_SEARCH_CMPL_NUM_ENTRIES_EB           42
#define TFC_MPC_TBL_EM_SEARCH_CMPL_NUM_ENTRIES_SB           40
#define TFC_MPC_TBL_EM_SEARCH_CMPL_NUM_ENTRIES_OFFS         0x18

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SET_NUM_ENTRIES(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_NUM_ENTRIES_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_NUM_ENTRIES_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_NUM_ENTRIES_SB)
#define TFC_MPC_TBL_EM_SEARCH_CMPL_GET_NUM_ENTRIES(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_SEARCH_CMPL_NUM_ENTRIES_OFFS), \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_NUM_ENTRIES_EB, \
		     TFC_MPC_TBL_EM_SEARCH_CMPL_NUM_ENTRIES_SB)

#define TFC_MPC_TBL_EM_SEARCH_CMPL_SIZE     32

/*
 * CFA Table EM Insert Completion Record:
 *
 * OK status indicates that the exact match entry from the command was
 * successfully inserted.
 * EM_DUPLICATE status indicates that the insert was aborted because an entry
 * with the same exact match key was found and REPLACE=0 in the command.
 * EM_ABORT status indicates that no duplicate was found, the tail bucket in
 * the chain was full, and TABLE_INDEX2=0. No changes are made to the database
 * in this case.
 * TABLE_INDEX is the starting address at which to insert the exact match entry
 * (from the command).
 * TABLE_INDEX2 is the address at which to insert a new bucket at the tail of
 * the static bucket chain if needed (from the command). CHAIN_UPD=1 if a new
 * bucket was added at this address.
 * TABLE_INDEX3 is the static bucket address for the chain, determined from
 * hashing the exact match entry. Software needs this address and TABLE_INDEX in
 * order to delete the entry using an EM_DELETE command.
 * TABLE_INDEX4 is the index of an entry found that had a matching exact match
 * key to the command entry key. If no matching entry was found, it is set to 0.
 * There are two cases when there is a matching entry, depending on REPLACE from
 * the command:
 * REPLACE=0: EM_DUPLICATE status is reported and the insert is aborted.
 * Software can use the static bucket address (TABLE_INDEX3[23:0]) and the
 * matching entry (TABLE_INDEX4) in an EM_DELETE command if it wishes to
 * explicity delete the matching entry.
 * REPLACE=1: REPLACED_ENTRY=1 to signal that the entry at TABLE_INDEX4 was
 * replaced by the insert entry. REPLACED_ENTRY will only be 1 if reporting OK
 * status in this case. Software can de-allocate the entry at TABLE_INDEX4.
 * Offset  63              0
 * 0x0     opaque  unused(8)       opcode  mp_client       status  unused(2)
 * type
 * 0x8     unused(6)       table_index     unused(3)       table_scope
 * unused(8)       hash_msb        unused(3)       v1
 * 0x10    unused(6)       table_index3    unused(6)       table_index2
 * 0x18    unused(19)      replaced_entry  chain_upd       num_entries
 * bkt_num unused(5)       table_index4    v2
 *
 *    type (Offset:0x0[5:0], Size: 6)
 *       This field indicates the exact type of the
 *       completion. By convention, the LSB identifies the
 *       length of the record in 16B units. Even values
 *       indicate 16B records. Odd values indicate 32B
 *       records **(EXCEPT no_op!!!!)** .
 *       Value   Enum    Enumeration Description
 *          31      mid_path_long
 *          Mid Path Long Completion : Completion of a Mid
 *          Path Command. Length = 32B
 *
 *    status (Offset:0x0[11:8], Size: 4)
 *       The command processing status.
 *       Value   Enum    Enumeration Description
 *          0       OK
 *          Completed without error.
 *          1       UNSPRT_ERR
 *          The CFA OPCODE is an unsupported value.
 *          2       FMT_ERR
 *          Indicates a CFA command formatting error. This
 *          error can occur on any of the supported CFA
 *          commands.
 *          Error conditions:
 *          DATA_SIZE[2:0] outside range of [1, 4]. (Does
 *          not apply to READ_CLR, EM_DELETE, or EM_CHAIN
 *          commands as they do not have a DATA_SIZE field)
 *          HOST_ADDRESS[1:0] != 0 (Only applies to READ,
 *          READ_CLR, and EVENT_COLLECTION as other commands
 *          do not have a HOST_ADDRESS field.
 *          3       SCOPE_ERR
 *          Access to TABLE_SCOPE is disabled for the SVIF.
 *          Indates that the bit indexed by (SVIF,
 *          TABLE_SCOPE) in the TAI_SVIF_SCOPE memory is set
 *          to 0.
 *          4       ADDR_ERR
 *          This error can only occur for commands having
 *          TABLE_TYPE present and set to EM and not having
 *          any of the previous errors, or for any of the
 *          EM* commands, for which a TABLE_TYPE of EM is
 *          implied.
 *          It indicates that an EM address (TABLE_INDEX*)
 *          in the command is invalid based on (EM_BUCKETS,
 *          EM_SIZE) parameters configured for TABLE_SCOPE.
 *          All addresses must be in the range [0,
 *          EM_SIZE). Static bucket addresses must be within
 *          the range determined by EM_BUCKETS. Dynamic
 *          bucket addresses and entries must be outside of
 *          the static bucket range.
 *          5       CACHE_ERR
 *          One of more cache responses signaled an error
 *          while processing the command.
 *          7       EM_DUPLICATE
 *          Found an entry with a key that matches the
 *          entry to insert and the command has REPLACE=0.
 *          The new entry was not inserted.
 *          9       EM_ABORT
 *          For insert commands, TABLE_INDEX2 provides the
 *          address at which to add a new bucket if the tail
 *          bucket of the chain is full and no duplicate was
 *          found. If TABLE_INDEX2=0, the insert is aborted
 *          (no changes are made to the database) and this
 *          status is returned.
 *
 *    mp_client (Offset:0x0[15:12], Size: 4)
 *       This field represents the Mid-Path client that
 *       generated the completion.
 *       Value   Enum    Enumeration Description
 *          2       TE_CFA
 *          TE-CFA
 *          3       RE_CFA
 *          RE-CFA
 *
 *    opcode (Offset:0x0[23:16], Size: 8)
 *       OPCODE from the command.
 *       Value   Enum    Enumeration Description
 *          9       EM_INSERT
 *          This command supplies an exact match entry of
 *          1-4 32B words to be inserted into the exact
 *          match table.
 *
 *    opaque (Offset:0x0[63:32], Size: 32)
 *       This is a copy of the opaque field from the mid
 *       path BD of this command.
 *
 *    v1 (Offset:0x8[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    hash_msb (Offset:0x8[15:4], Size: 12)
 *       For EM_SEARCH and EM_INSERT commands without
 *       errors that abort the command processing prior to
 *       the hash computation, set to HASH[35:24] of the
 *       hash computed from the exact match entry key in the
 *       command.
 *       For all other cases, set to 0 except for the
 *       following error conditions, which carry debug
 *       information in this field as shown by error status
 *       below:
 *       FMT_ERR:
 *       Set to {7'd0, HOST_ADDRESS[1:0],
 *       DATA_SIZE[2:0]}.
 *       If HOST_ADDRESS or DATA_SIZE field not
 *       present they are set to 0.
 *       SCOPE_ERR:
 *       Set to {1'b0, SVIF[10:0]}.
 *       ADDR_ERR:
 *       Only possible when TABLE_TYPE=EM or for EM*
 *       commands
 *       Set to {1'b0, TABLE_INDEX[2:0], 5'd0,
 *       DATA_SIZE[2:0]}
 *       TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error
 *       TABLE_INDEX[1]=1 if TABLE_INDEX2 had an error
 *       TABLE_INDEX[0]=1 if TABLE_INDEX had an error
 *       TABLE_INDEX[n]=0 if the completion does not
 *       have the corresponding TABLE_INDEX field above.
 *       CACHE_ERR:
 *       Set to {9'd0, DATA_SIZE[2:0]}
 *
 *    table_scope (Offset:0x8[28:24], Size: 5)
 *       TABLE_SCOPE from the command.
 *
 *    table_index (Offset:0x8[57:32], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       TABLE_INDEX from the command, which is the
 *       starting address at which to insert the exact match
 *       entry.
 *
 *    table_index2 (Offset:0x10[25:0], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       TABLE_INDEX2 from the command, which is the index
 *       for the new tail bucket to add if needed
 *       (CHAIN_UPD=1 if it was used).
 *
 *    table_index3 (Offset:0x10[57:32], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       If the hash is computed (no errors during initial
 *       processing of the command), TABLE_INDEX2[23:0] is
 *       the static bucket address determined from the hash
 *       of the exact match entry key in the command and the
 *       (EM_SIZE, EM_BUCKETS) configuration for TABLE_SCOPE
 *       of the command. Bits 25:24 in this case are set to
 *       0.
 *       For any other status, it is always 0.
 *
 *    v2 (Offset:0x18[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    table_index4 (Offset:0x18[26:1], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       ENTRY_PTR of matching entry found. Set to 0 if no
 *       matching entry found. If REPLACED_ENTRY=1, that
 *       indicates a matching entry was found and REPLACE=1
 *       in the command. In this case, the matching entry
 *       was replaced by the new entry in the command and
 *       this index can therefore by de-allocated.
 *
 *    bkt_num (Offset:0x18[39:32], Size: 8)
 *       BKT_NUM is the bucket number in chain of the tail
 *       bucket after finishing processing the command,
 *       except when the command stops processing before the
 *       tail bucket. NUM_ENTRIES is the number of valid
 *       entries in the BKT_NUM bucket. The following
 *       describes the cases where BKT_NUM and NUM_ENTRIES
 *       are not for the tail bucket after finishing
 *       processing of the command:
 *       For UNSPRT_ERR, FMT_ERR, SCOPE_ERR, or ADDR_ERR
 *       completion status, BKT_NUM will be set to 0.
 *       For CACHE_ERR completion status, BKT_NUM will be
 *       set to the bucket number that was last read without
 *       error. If ERR=1 in the response to the static
 *       bucket read, BKT_NUM and NUM_ENTRIES are set to 0.
 *       The static bucket is number 0, BKT_NUM increments
 *       for each new bucket in the chain, and saturates at
 *       255. Therefore, if the value is 255, BKT_NUM may or
 *       may not be accurate. In this case, though,
 *       NUM_ENTRIES will still be the correct value as
 *       described above for the bucket.
 *       For EM_DUPLICATE status, which indicates a
 *       matching entry was found and prevented the insert,
 *       BKT_NUM and NUM_ENTRIES are for the bucket
 *       containing the match entry, which may or may not be
 *       the tail bucket. For OK and EM_ABORT status, the
 *       values are always for the tail bucket. For
 *       EM_ABORT, NUM_ENTRIES will always be 6 since the
 *       tail bucket is full.
 *
 *    num_entries (Offset:0x18[42:40], Size: 3)
 *       See BKT_NUM description.
 *
 *    chain_upd (Offset:0x18[43], Size: 1)
 *       Specifies if the chain was updated while
 *       processing the command:
 *       Set to 1 when a new bucket is added to the tail of
 *       the static bucket chain at TABLE_INDEX2. This
 *       occurs if and only if the insert requires adding a
 *       new entry and the tail bucket is full. If set to 0,
 *       TABLE_INDEX2 was not used and is therefore still
 *       free.
 *       When the CFA updates the static bucket chain by
 *       adding a bucket during inserts or removing one
 *       during deletes, it always sets CHAIN=0 in the new
 *       tail bucket and sets CHAIN_PTR to that of the
 *       original tail bucket. This is done to preserve the
 *       background chaining. EM_CHAIN provides a means to
 *       coherently update the CHAIN_PTR in the tail bucket
 *       separately if desired.
 *
 *    replaced_entry (Offset:0x18[44], Size: 1)
 *       Set to 1 if a matching entry was found and
 *       REPLACE=1 in command. In the case, the entry
 *       starting at TABLE_INDEX4 was replaced and can
 *       therefore be de-allocated. Otherwise, this flag is
 *       set to 0.
 *       This structure is used to inform the host of an
 *       event within the NIC.
 */
#define TFC_MPC_TBL_EM_INSERT_CMPL_TYPE_MID_PATH_LONG    31

#define TFC_MPC_TBL_EM_INSERT_CMPL_TYPE_EB           5
#define TFC_MPC_TBL_EM_INSERT_CMPL_TYPE_SB           0
#define TFC_MPC_TBL_EM_INSERT_CMPL_TYPE_OFFS         0x0

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TYPE_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_TYPE_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TYPE_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_OK              0
#define TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_UNSPRT_ERR      1
#define TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_FMT_ERR         2
#define TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_SCOPE_ERR       3
#define TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_ADDR_ERR        4
#define TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_CACHE_ERR       5
#define TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_EM_DUPLICATE    7
#define TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_EM_ABORT        9

#define TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_EB           11
#define TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_SB           8
#define TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_OFFS         0x0

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_STATUS(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_STATUS(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_STATUS_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_MP_CLIENT_TE_CFA    2
#define TFC_MPC_TBL_EM_INSERT_CMPL_MP_CLIENT_RE_CFA    3

#define TFC_MPC_TBL_EM_INSERT_CMPL_MP_CLIENT_EB           15
#define TFC_MPC_TBL_EM_INSERT_CMPL_MP_CLIENT_SB           12
#define TFC_MPC_TBL_EM_INSERT_CMPL_MP_CLIENT_OFFS         0x0

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_MP_CLIENT(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_MP_CLIENT_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_MP_CLIENT_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_MP_CLIENT(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_MP_CLIENT_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_MP_CLIENT_SB)

#define TFC_MPC_CMD_OPCODE_EM_INSERT    9

#define TFC_MPC_TBL_EM_INSERT_CMPL_OPCODE_EB           23
#define TFC_MPC_TBL_EM_INSERT_CMPL_OPCODE_SB           16
#define TFC_MPC_TBL_EM_INSERT_CMPL_OPCODE_OFFS         0x0

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_OPCODE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_OPCODE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_OPCODE_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_OPCODE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_OPCODE_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_OPCODE_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_OPAQUE_EB           63
#define TFC_MPC_TBL_EM_INSERT_CMPL_OPAQUE_SB           32
#define TFC_MPC_TBL_EM_INSERT_CMPL_OPAQUE_OFFS         0x0

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_OPAQUE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_OPAQUE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_OPAQUE_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_OPAQUE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_OPAQUE_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_OPAQUE_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_V1_EB           0
#define TFC_MPC_TBL_EM_INSERT_CMPL_V1_SB           0
#define TFC_MPC_TBL_EM_INSERT_CMPL_V1_OFFS         0x8

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_V1(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_V1_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_V1_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_V1_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_V1(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_V1_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_V1_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_V1_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_HASH_MSB_EB           15
#define TFC_MPC_TBL_EM_INSERT_CMPL_HASH_MSB_SB           4
#define TFC_MPC_TBL_EM_INSERT_CMPL_HASH_MSB_OFFS         0x8

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_HASH_MSB(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_HASH_MSB_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_HASH_MSB_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_HASH_MSB(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_HASH_MSB_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_HASH_MSB_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_SCOPE_EB           28
#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_SCOPE_SB           24
#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_SCOPE_OFFS         0x8

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_SCOPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_SCOPE_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_TABLE_SCOPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_SCOPE_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_SCOPE_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX_EB           57
#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX_SB           32
#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX_OFFS         0x8

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_TABLE_INDEX(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX2_EB           25
#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX2_SB           0
#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX2_OFFS         0x10

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_TABLE_INDEX2(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX2_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX2_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX2_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_TABLE_INDEX2(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX2_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX2_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX2_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX3_EB           57
#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX3_SB           32
#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX3_OFFS         0x10

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_TABLE_INDEX3(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX3_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX3_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX3_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_TABLE_INDEX3(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX3_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX3_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX3_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_V2_EB           0
#define TFC_MPC_TBL_EM_INSERT_CMPL_V2_SB           0
#define TFC_MPC_TBL_EM_INSERT_CMPL_V2_OFFS         0x18

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_V2(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_V2_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_V2_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_V2_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_V2(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_V2_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_V2_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_V2_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX4_EB           26
#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX4_SB           1
#define TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX4_OFFS         0x18

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_TABLE_INDEX4(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX4_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX4_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX4_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_TABLE_INDEX4(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX4_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX4_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_TABLE_INDEX4_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_BKT_NUM_EB           39
#define TFC_MPC_TBL_EM_INSERT_CMPL_BKT_NUM_SB           32
#define TFC_MPC_TBL_EM_INSERT_CMPL_BKT_NUM_OFFS         0x18

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_BKT_NUM(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_BKT_NUM_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_BKT_NUM_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_BKT_NUM_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_BKT_NUM(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_BKT_NUM_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_BKT_NUM_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_BKT_NUM_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_NUM_ENTRIES_EB           42
#define TFC_MPC_TBL_EM_INSERT_CMPL_NUM_ENTRIES_SB           40
#define TFC_MPC_TBL_EM_INSERT_CMPL_NUM_ENTRIES_OFFS         0x18

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_NUM_ENTRIES(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_NUM_ENTRIES_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_NUM_ENTRIES_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_NUM_ENTRIES_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_NUM_ENTRIES(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_NUM_ENTRIES_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_NUM_ENTRIES_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_NUM_ENTRIES_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_CHAIN_UPD_EB           43
#define TFC_MPC_TBL_EM_INSERT_CMPL_CHAIN_UPD_SB           43
#define TFC_MPC_TBL_EM_INSERT_CMPL_CHAIN_UPD_OFFS         0x18

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_CHAIN_UPD(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_CHAIN_UPD_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_CHAIN_UPD_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_CHAIN_UPD_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_CHAIN_UPD(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_CHAIN_UPD_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_CHAIN_UPD_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_CHAIN_UPD_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_REPLACED_ENTRY_EB           44
#define TFC_MPC_TBL_EM_INSERT_CMPL_REPLACED_ENTRY_SB           44
#define TFC_MPC_TBL_EM_INSERT_CMPL_REPLACED_ENTRY_OFFS         0x18

#define TFC_MPC_TBL_EM_INSERT_CMPL_SET_REPLACED_ENTRY(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_REPLACED_ENTRY_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_REPLACED_ENTRY_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_REPLACED_ENTRY_SB)
#define TFC_MPC_TBL_EM_INSERT_CMPL_GET_REPLACED_ENTRY(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_INSERT_CMPL_REPLACED_ENTRY_OFFS), \
		     TFC_MPC_TBL_EM_INSERT_CMPL_REPLACED_ENTRY_EB, \
		     TFC_MPC_TBL_EM_INSERT_CMPL_REPLACED_ENTRY_SB)

#define TFC_MPC_TBL_EM_INSERT_CMPL_SIZE     32

/*
 * CFA Table EM Delete Completion Record:
 *
 * OK status indicates that an ENTRY_PTR matching TABLE_INDEX was found in the
 * static bucket chain specified and was therefore deleted. EM_MISS status
 * indicates that no match was found.
 * TABLE_INDEX is from the command. It is the index of the entry to delete.
 * TABLE_INDEX2 is from the command. It is the static bucket address.
 * TABLE_INDEX3 is the index of the tail bucket of the static bucket chain
 * prior to processing the command.
 * TABLE_INDEX4 is the index of the tail bucket of the static bucket chain
 * after processing the command.
 * If CHAIN_UPD=1 and TABLE_INDEX4==TABLE_INDEX2, the static bucket was the
 * tail bucket, it became empty after the delete, the scope is a locked scope,
 * and CHAIN_PTR was 0. In this case, the static bucket has been evicted from
 * the cache.
 * Otherwise, if CHAIN_UPD=1, the original tail bucket given by TABLE_INDEX3
 * was removed from the chain because it went empty. It can therefore be de-
 * allocated.
 * Offset  63              0
 * 0x0     opaque  unused(8)       opcode  mp_client       status  unused(2)
 * type
 * 0x8     unused(6)       table_index     unused(3)       table_scope
 * unused(8)       hash_msb        unused(3)       v1
 * 0x10    unused(6)       table_index3    unused(6)       table_index2
 * 0x18    unused(20)      chain_upd       num_entries     bkt_num unused(5)
 * table_index4    v2
 *
 *    type (Offset:0x0[5:0], Size: 6)
 *       This field indicates the exact type of the
 *       completion. By convention, the LSB identifies the
 *       length of the record in 16B units. Even values
 *       indicate 16B records. Odd values indicate 32B
 *       records **(EXCEPT no_op!!!!)** .
 *       Value   Enum    Enumeration Description
 *          31      mid_path_long
 *          Mid Path Long Completion : Completion of a Mid
 *          Path Command. Length = 32B
 *
 *    status (Offset:0x0[11:8], Size: 4)
 *       The command processing status.
 *       Value   Enum    Enumeration Description
 *          0       OK
 *          Completed without error.
 *          1       UNSPRT_ERR
 *          The CFA OPCODE is an unsupported value.
 *          2       FMT_ERR
 *          Indicates a CFA command formatting error. This
 *          error can occur on any of the supported CFA
 *          commands.
 *          Error conditions:
 *          DATA_SIZE[2:0] outside range of [1, 4]. (Does
 *          not apply to READ_CLR, EM_DELETE, or EM_CHAIN
 *          commands as they do not have a DATA_SIZE field)
 *          HOST_ADDRESS[1:0] != 0 (Only applies to READ,
 *          READ_CLR, and EVENT_COLLECTION as other commands
 *          do not have a HOST_ADDRESS field.
 *          3       SCOPE_ERR
 *          Access to TABLE_SCOPE is disabled for the SVIF.
 *          Indates that the bit indexed by (SVIF,
 *          TABLE_SCOPE) in the TAI_SVIF_SCOPE memory is set
 *          to 0.
 *          4       ADDR_ERR
 *          This error can only occur for commands having
 *          TABLE_TYPE present and set to EM and not having
 *          any of the previous errors, or for any of the
 *          EM* commands, for which a TABLE_TYPE of EM is
 *          implied.
 *          It indicates that an EM address (TABLE_INDEX*)
 *          in the command is invalid based on (EM_BUCKETS,
 *          EM_SIZE) parameters configured for TABLE_SCOPE.
 *          All addresses must be in the range [0,
 *          EM_SIZE). Static bucket addresses must be within
 *          the range determined by EM_BUCKETS. Dynamic
 *          bucket addresses and entries must be outside of
 *          the static bucket range.
 *          5       CACHE_ERR
 *          One of more cache responses signaled an error
 *          while processing the command.
 *          6       EM_MISS
 *          No matching entry found.
 *
 *    mp_client (Offset:0x0[15:12], Size: 4)
 *       This field represents the Mid-Path client that
 *       generated the completion.
 *       Value   Enum    Enumeration Description
 *          2       TE_CFA
 *          TE-CFA
 *          3       RE_CFA
 *          RE-CFA
 *
 *    opcode (Offset:0x0[23:16], Size: 8)
 *       OPCODE from the command.
 *       Value   Enum    Enumeration Description
 *          10      EM_DELETE
 *          This command deletes an entry from the exact
 *          match table. CFA searches for the specified
 *          entry address in the bucket chain at the static
 *          bucket address given.
 *
 *    opaque (Offset:0x0[63:32], Size: 32)
 *       This is a copy of the opaque field from the mid
 *       path BD of this command.
 *
 *    v1 (Offset:0x8[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    hash_msb (Offset:0x8[15:4], Size: 12)
 *       For EM_SEARCH and EM_INSERT commands without
 *       errors that abort the command processing prior to
 *       the hash computation, set to HASH[35:24] of the
 *       hash computed from the exact match entry key in the
 *       command.
 *       For all other cases, set to 0 except for the
 *       following error conditions, which carry debug
 *       information in this field as shown by error status
 *       below:
 *       FMT_ERR:
 *       Set to {7'd0, HOST_ADDRESS[1:0],
 *       DATA_SIZE[2:0]}.
 *       If HOST_ADDRESS or DATA_SIZE field not
 *       present they are set to 0.
 *       SCOPE_ERR:
 *       Set to {1'b0, SVIF[10:0]}.
 *       ADDR_ERR:
 *       Only possible when TABLE_TYPE=EM or for EM*
 *       commands
 *       Set to {1'b0, TABLE_INDEX[2:0], 5'd0,
 *       DATA_SIZE[2:0]}
 *       TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error
 *       TABLE_INDEX[1]=1 if TABLE_INDEX2 had an error
 *       TABLE_INDEX[0]=1 if TABLE_INDEX had an error
 *       TABLE_INDEX[n]=0 if the completion does not
 *       have the corresponding TABLE_INDEX field above.
 *       CACHE_ERR:
 *       Set to {9'd0, DATA_SIZE[2:0]}
 *
 *    table_scope (Offset:0x8[28:24], Size: 5)
 *       TABLE_SCOPE from the command.
 *
 *    table_index (Offset:0x8[57:32], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       TABLE_INDEX from the command, which is the index
 *       of the entry to delete.
 *
 *    table_index2 (Offset:0x10[25:0], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       TABLE_INDEX2 from the command.
 *
 *    table_index3 (Offset:0x10[57:32], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       For OK or EM_MISS status, the index of the tail
 *       bucket of the chain prior to processing the
 *       command. If CHAIN_UPD=1, the bucket was removed and
 *       this index can be de-allocated. For other status
 *       values, it is set to 0.
 *
 *    v2 (Offset:0x18[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    table_index4 (Offset:0x18[26:1], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       For OK or EM_MISS status, the index of the tail
 *       bucket of the chain prior to after the command. If
 *       CHAIN_UPD=0 (always for EM_MISS status), it is
 *       always equal to TABLE_INDEX3 as the chain was not
 *       updated. For other status values, it is set to 0.
 *
 *    bkt_num (Offset:0x18[39:32], Size: 8)
 *       BKT_NUM is the bucket number in chain of the tail
 *       bucket after finishing processing the command,
 *       except when the command stops processing before the
 *       tail bucket. NUM_ENTRIES is the number of valid
 *       entries in the BKT_NUM bucket. The following
 *       describes the cases where BKT_NUM and NUM_ENTRIES
 *       are not for the tail bucket after finishing
 *       processing of the command:
 *       For UNSPRT_ERR, FMT_ERR, SCOPE_ERR, or ADDR_ERR
 *       completion status, BKT_NUM will be set to 0.
 *       For CACHE_ERR completion status, BKT_NUM will be
 *       set to the bucket number that was last read without
 *       error. If ERR=1 in the response to the static
 *       bucket read, BKT_NUM and NUM_ENTRIES are set to 0.
 *       The static bucket is number 0, BKT_NUM increments
 *       for each new bucket in the chain, and saturates at
 *       255. Therefore, if the value is 255, BKT_NUM may or
 *       may not be accurate. In this case, though,
 *       NUM_ENTRIES will still be the correct value as
 *       described above for the bucket.
 *       For OK status, BKT_NUM and NUM_ENTRIES will be for
 *       the tail bucket after processing.
 *
 *    num_entries (Offset:0x18[42:40], Size: 3)
 *       See BKT_NUM description.
 *
 *    chain_upd (Offset:0x18[43], Size: 1)
 *       Specifies if the chain was updated while
 *       processing the command:
 *       Set to 1 when a bucket is removed from the static
 *       bucket chain. This occurs if after the delete, the
 *       tail bucket is a dynamic bucket and no longer has
 *       any valid entries. In this case, software should
 *       de-allocate the dynamic bucket at TABLE_INDEX3.
 *       It is also set to 1 when the static bucket is
 *       evicted, which only occurs for locked scopes. See
 *       the EM_DELETE command description for details.
 *       When the CFA updates the static bucket chain by
 *       adding a bucket during inserts or removing one
 *       during deletes, it always sets CHAIN=0 in the new
 *       tail bucket and sets CHAIN_PTR to that of the
 *       original tail bucket. This is done to preserve the
 *       background chaining. EM_CHAIN provides a means to
 *       coherently update the CHAIN_PTR in the tail bucket
 *       separately if desired.
 *       This structure is used to inform the host of an
 *       event within the NIC.
 */
#define TFC_MPC_TBL_EM_DELETE_CMPL_TYPE_MID_PATH_LONG    31

#define TFC_MPC_TBL_EM_DELETE_CMPL_TYPE_EB           5
#define TFC_MPC_TBL_EM_DELETE_CMPL_TYPE_SB           0
#define TFC_MPC_TBL_EM_DELETE_CMPL_TYPE_OFFS         0x0

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TYPE_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_TYPE_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TYPE_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_OK            0
#define TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_UNSPRT_ERR    1
#define TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_FMT_ERR       2
#define TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_SCOPE_ERR     3
#define TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_ADDR_ERR      4
#define TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_CACHE_ERR     5
#define TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_EM_MISS       6

#define TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_EB           11
#define TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_SB           8
#define TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_OFFS         0x0

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_STATUS(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_STATUS(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_STATUS_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_MP_CLIENT_TE_CFA    2
#define TFC_MPC_TBL_EM_DELETE_CMPL_MP_CLIENT_RE_CFA    3

#define TFC_MPC_TBL_EM_DELETE_CMPL_MP_CLIENT_EB           15
#define TFC_MPC_TBL_EM_DELETE_CMPL_MP_CLIENT_SB           12
#define TFC_MPC_TBL_EM_DELETE_CMPL_MP_CLIENT_OFFS         0x0

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_MP_CLIENT(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_MP_CLIENT_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_MP_CLIENT_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_MP_CLIENT(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_MP_CLIENT_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_MP_CLIENT_SB)

#define TFC_MPC_CMD_OPCODE_EM_DELETE    10

#define TFC_MPC_TBL_EM_DELETE_CMPL_OPCODE_EB           23
#define TFC_MPC_TBL_EM_DELETE_CMPL_OPCODE_SB           16
#define TFC_MPC_TBL_EM_DELETE_CMPL_OPCODE_OFFS         0x0

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_OPCODE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_OPCODE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_OPCODE_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_OPCODE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_OPCODE_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_OPCODE_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_OPAQUE_EB           63
#define TFC_MPC_TBL_EM_DELETE_CMPL_OPAQUE_SB           32
#define TFC_MPC_TBL_EM_DELETE_CMPL_OPAQUE_OFFS         0x0

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_OPAQUE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_OPAQUE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_OPAQUE_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_OPAQUE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_OPAQUE_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_OPAQUE_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_V1_EB           0
#define TFC_MPC_TBL_EM_DELETE_CMPL_V1_SB           0
#define TFC_MPC_TBL_EM_DELETE_CMPL_V1_OFFS         0x8

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_V1(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_V1_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_V1_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_V1_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_V1(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_V1_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_V1_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_V1_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_HASH_MSB_EB           15
#define TFC_MPC_TBL_EM_DELETE_CMPL_HASH_MSB_SB           4
#define TFC_MPC_TBL_EM_DELETE_CMPL_HASH_MSB_OFFS         0x8

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_HASH_MSB(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_HASH_MSB_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_HASH_MSB_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_HASH_MSB(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_HASH_MSB_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_HASH_MSB_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_SCOPE_EB           28
#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_SCOPE_SB           24
#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_SCOPE_OFFS         0x8

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_SCOPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_SCOPE_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_TABLE_SCOPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_SCOPE_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_SCOPE_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX_EB           57
#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX_SB           32
#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX_OFFS         0x8

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_TABLE_INDEX(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX2_EB           25
#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX2_SB           0
#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX2_OFFS         0x10

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_TABLE_INDEX2(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX2_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX2_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX2_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_TABLE_INDEX2(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX2_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX2_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX2_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX3_EB           57
#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX3_SB           32
#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX3_OFFS         0x10

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_TABLE_INDEX3(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX3_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX3_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX3_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_TABLE_INDEX3(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX3_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX3_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX3_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_V2_EB           0
#define TFC_MPC_TBL_EM_DELETE_CMPL_V2_SB           0
#define TFC_MPC_TBL_EM_DELETE_CMPL_V2_OFFS         0x18

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_V2(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_V2_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_V2_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_V2_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_V2(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_V2_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_V2_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_V2_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX4_EB           26
#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX4_SB           1
#define TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX4_OFFS         0x18

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_TABLE_INDEX4(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX4_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX4_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX4_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_TABLE_INDEX4(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX4_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX4_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_TABLE_INDEX4_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_BKT_NUM_EB           39
#define TFC_MPC_TBL_EM_DELETE_CMPL_BKT_NUM_SB           32
#define TFC_MPC_TBL_EM_DELETE_CMPL_BKT_NUM_OFFS         0x18

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_BKT_NUM(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_BKT_NUM_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_BKT_NUM_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_BKT_NUM_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_BKT_NUM(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_BKT_NUM_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_BKT_NUM_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_BKT_NUM_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_NUM_ENTRIES_EB           42
#define TFC_MPC_TBL_EM_DELETE_CMPL_NUM_ENTRIES_SB           40
#define TFC_MPC_TBL_EM_DELETE_CMPL_NUM_ENTRIES_OFFS         0x18

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_NUM_ENTRIES(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_NUM_ENTRIES_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_NUM_ENTRIES_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_NUM_ENTRIES_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_NUM_ENTRIES(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_NUM_ENTRIES_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_NUM_ENTRIES_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_NUM_ENTRIES_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_CHAIN_UPD_EB           43
#define TFC_MPC_TBL_EM_DELETE_CMPL_CHAIN_UPD_SB           43
#define TFC_MPC_TBL_EM_DELETE_CMPL_CHAIN_UPD_OFFS         0x18

#define TFC_MPC_TBL_EM_DELETE_CMPL_SET_CHAIN_UPD(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_CHAIN_UPD_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_CHAIN_UPD_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_CHAIN_UPD_SB)
#define TFC_MPC_TBL_EM_DELETE_CMPL_GET_CHAIN_UPD(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_DELETE_CMPL_CHAIN_UPD_OFFS), \
		     TFC_MPC_TBL_EM_DELETE_CMPL_CHAIN_UPD_EB, \
		     TFC_MPC_TBL_EM_DELETE_CMPL_CHAIN_UPD_SB)

#define TFC_MPC_TBL_EM_DELETE_CMPL_SIZE     32

/*
 * CFA Table EM Chain Completion Record:
 *
 * OK status indicates that the CHAIN_PTR of the tail bucket was successfully
 * updated.
 * TABLE_INDEX is from the command. It is the value of the new CHAIN_PTR.
 * TABLE_INDEX2 is from the command.
 * TABLE_INDEX3 is the index of the tail bucket of the static bucket chain.
 * Offset  63              0
 * 0x0     opaque  unused(8)       opcode  mp_client       status  unused(2)
 * type
 * 0x8     unused(6)       table_index     unused(3)       table_scope
 * unused(8)       hash_msb        unused(3)       v1
 * 0x10    unused(6)       table_index3    unused(6)       table_index2
 * 0x18    unused(20)      chain_upd       num_entries     bkt_num unused(31)
 * v2
 *
 *    type (Offset:0x0[5:0], Size: 6)
 *       This field indicates the exact type of the
 *       completion. By convention, the LSB identifies the
 *       length of the record in 16B units. Even values
 *       indicate 16B records. Odd values indicate 32B
 *       records **(EXCEPT no_op!!!!)** .
 *       Value   Enum    Enumeration Description
 *          31      mid_path_long
 *          Mid Path Long Completion : Completion of a Mid
 *          Path Command. Length = 32B
 *
 *    status (Offset:0x0[11:8], Size: 4)
 *       The command processing status.
 *       Value   Enum    Enumeration Description
 *          0       OK
 *          Completed without error.
 *          1       UNSPRT_ERR
 *          The CFA OPCODE is an unsupported value.
 *          2       FMT_ERR
 *          Indicates a CFA command formatting error. This
 *          error can occur on any of the supported CFA
 *          commands.
 *          Error conditions:
 *          DATA_SIZE[2:0] outside range of [1, 4]. (Does
 *          not apply to READ_CLR, EM_DELETE, or EM_CHAIN
 *          commands as they do not have a DATA_SIZE field)
 *          HOST_ADDRESS[1:0] != 0 (Only applies to READ,
 *          READ_CLR, and EVENT_COLLECTION as other commands
 *          do not have a HOST_ADDRESS field.
 *          3       SCOPE_ERR
 *          Access to TABLE_SCOPE is disabled for the SVIF.
 *          Indates that the bit indexed by (SVIF,
 *          TABLE_SCOPE) in the TAI_SVIF_SCOPE memory is set
 *          to 0.
 *          4       ADDR_ERR
 *          This error can only occur for commands having
 *          TABLE_TYPE present and set to EM and not having
 *          any of the previous errors, or for any of the
 *          EM* commands, for which a TABLE_TYPE of EM is
 *          implied.
 *          It indicates that an EM address (TABLE_INDEX*)
 *          in the command is invalid based on (EM_BUCKETS,
 *          EM_SIZE) parameters configured for TABLE_SCOPE.
 *          All addresses must be in the range [0,
 *          EM_SIZE). Static bucket addresses must be within
 *          the range determined by EM_BUCKETS. Dynamic
 *          bucket addresses and entries must be outside of
 *          the static bucket range.
 *          5       CACHE_ERR
 *          One of more cache responses signaled an error
 *          while processing the command.
 *
 *    mp_client (Offset:0x0[15:12], Size: 4)
 *       This field represents the Mid-Path client that
 *       generated the completion.
 *       Value   Enum    Enumeration Description
 *          2       TE_CFA
 *          TE-CFA
 *          3       RE_CFA
 *          RE-CFA
 *
 *    opcode (Offset:0x0[23:16], Size: 8)
 *       OPCODE from the command.
 *       Value   Enum    Enumeration Description
 *          11      EM_CHAIN
 *          This command updates CHAIN_PTR in the tail
 *          bucket of a static bucket chain, supplying both
 *          the static bucket and the new CHAIN_PTR value.
 *
 *    opaque (Offset:0x0[63:32], Size: 32)
 *       This is a copy of the opaque field from the mid
 *       path BD of this command.
 *
 *    v1 (Offset:0x8[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    hash_msb (Offset:0x8[15:4], Size: 12)
 *       For EM_SEARCH and EM_INSERT commands without
 *       errors that abort the command processing prior to
 *       the hash computation, set to HASH[35:24] of the
 *       hash computed from the exact match entry key in the
 *       command.
 *       For all other cases, set to 0 except for the
 *       following error conditions, which carry debug
 *       information in this field as shown by error status
 *       below:
 *       FMT_ERR:
 *       Set to {7'd0, HOST_ADDRESS[1:0],
 *       DATA_SIZE[2:0]}.
 *       If HOST_ADDRESS or DATA_SIZE field not
 *       present they are set to 0.
 *       SCOPE_ERR:
 *       Set to {1'b0, SVIF[10:0]}.
 *       ADDR_ERR:
 *       Only possible when TABLE_TYPE=EM or for EM*
 *       commands
 *       Set to {1'b0, TABLE_INDEX[2:0], 5'd0,
 *       DATA_SIZE[2:0]}
 *       TABLE_INDEX[2]=1 if TABLE_INDEX3 had an error
 *       TABLE_INDEX[1]=1 if TABLE_INDEX2 had an error
 *       TABLE_INDEX[0]=1 if TABLE_INDEX had an error
 *       TABLE_INDEX[n]=0 if the completion does not
 *       have the corresponding TABLE_INDEX field above.
 *       CACHE_ERR:
 *       Set to {9'd0, DATA_SIZE[2:0]}
 *
 *    table_scope (Offset:0x8[28:24], Size: 5)
 *       TABLE_SCOPE from the command.
 *
 *    table_index (Offset:0x8[57:32], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       TABLE_INDEX from the command, which is the new
 *       CHAIN_PTR for the tail bucket of the static bucket
 *       chain.
 *
 *    table_index2 (Offset:0x10[25:0], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       TABLE_INDEX2 from the command.
 *
 *    table_index3 (Offset:0x10[57:32], Size: 26)
 *       A 32B index into the EM table identified by
 *       TABLE_SCOPE.
 *       For OK status, the index of the tail bucket of the
 *       chain. Otherwise, set to 0.
 *
 *    v2 (Offset:0x18[0], Size: 1)
 *       This value is written by the NIC such that it will
 *       be different for each pass through the completion
 *       queue. The even passes will write 1. The odd passes
 *       will write 0.
 *
 *    bkt_num (Offset:0x18[39:32], Size: 8)
 *       BKT_NUM is the bucket number in chain of the tail
 *       bucket after finishing processing the command,
 *       except when the command stops processing before the
 *       tail bucket. NUM_ENTRIES is the number of valid
 *       entries in the BKT_NUM bucket. The following
 *       describes the cases where BKT_NUM and NUM_ENTRIES
 *       are not for the tail bucket after finishing
 *       processing of the command:
 *       For UNSPRT_ERR, FMT_ERR, SCOPE_ERR, or ADDR_ERR
 *       completion status, BKT_NUM will be set to 0.
 *       For CACHE_ERR completion status, BKT_NUM will be
 *       set to the bucket number that was last read without
 *       error. If ERR=1 in the response to the static
 *       bucket read, BKT_NUM and NUM_ENTRIES are set to 0.
 *       The static bucket is number 0, BKT_NUM increments
 *       for each new bucket in the chain, and saturates at
 *       255. Therefore, if the value is 255, BKT_NUM may or
 *       may not be accurate. In this case, though,
 *       NUM_ENTRIES will still be the correct value as
 *       described above for the bucket.
 *       For OK status, BKT_NUM and NUM_ENTRIES will be for
 *       the tail bucket.
 *
 *    num_entries (Offset:0x18[42:40], Size: 3)
 *       See BKT_NUM description.
 *
 *    chain_upd (Offset:0x18[43], Size: 1)
 *       Set to 1 when the scope is a locked scope, the
 *       tail bucket is the static bucket, the bucket is
 *       empty (all of its ENTRY_PTR values are 0), and
 *       TABLE_INDEX=0 in the command. In this case, the
 *       static bucket is evicted. For all other cases, it
 *       is set to 0.
 *       When the CFA updates the static bucket chain by
 *       adding a bucket during inserts or removing one
 *       during deletes, it always sets CHAIN=0 in the new
 *       tail bucket and sets CHAIN_PTR to that of the
 *       original tail bucket. This is done to preserve the
 *       background chaining. EM_CHAIN provides a means to
 *       coherently update the CHAIN_PTR in the tail bucket
 *       separately if desired.
 *       This structure is used to inform the host of an
 *       event within the NIC.
 */
#define TFC_MPC_TBL_EM_CHAIN_CMPL_TYPE_MID_PATH_LONG    31

#define TFC_MPC_TBL_EM_CHAIN_CMPL_TYPE_EB           5
#define TFC_MPC_TBL_EM_CHAIN_CMPL_TYPE_SB           0
#define TFC_MPC_TBL_EM_CHAIN_CMPL_TYPE_OFFS         0x0

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_TYPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_TYPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TYPE_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_TYPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_TYPE_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TYPE_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TYPE_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_OK            0
#define TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_UNSPRT_ERR    1
#define TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_FMT_ERR       2
#define TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_SCOPE_ERR     3
#define TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_ADDR_ERR      4
#define TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_CACHE_ERR     5

#define TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_EB           11
#define TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_SB           8
#define TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_OFFS         0x0

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_STATUS(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_STATUS(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_STATUS_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_MP_CLIENT_TE_CFA    2
#define TFC_MPC_TBL_EM_CHAIN_CMPL_MP_CLIENT_RE_CFA    3

#define TFC_MPC_TBL_EM_CHAIN_CMPL_MP_CLIENT_EB           15
#define TFC_MPC_TBL_EM_CHAIN_CMPL_MP_CLIENT_SB           12
#define TFC_MPC_TBL_EM_CHAIN_CMPL_MP_CLIENT_OFFS         0x0

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_MP_CLIENT(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_MP_CLIENT_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_MP_CLIENT_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_MP_CLIENT(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_MP_CLIENT_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_MP_CLIENT_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_MP_CLIENT_SB)

#define TFC_MPC_CMD_OPCODE_EM_CHAIN    11

#define TFC_MPC_TBL_EM_CHAIN_CMPL_OPCODE_EB           23
#define TFC_MPC_TBL_EM_CHAIN_CMPL_OPCODE_SB           16
#define TFC_MPC_TBL_EM_CHAIN_CMPL_OPCODE_OFFS         0x0

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_OPCODE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_OPCODE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_OPCODE_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_OPCODE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_OPCODE_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_OPCODE_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_OPCODE_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_OPAQUE_EB           63
#define TFC_MPC_TBL_EM_CHAIN_CMPL_OPAQUE_SB           32
#define TFC_MPC_TBL_EM_CHAIN_CMPL_OPAQUE_OFFS         0x0

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_OPAQUE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_OPAQUE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_OPAQUE_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_OPAQUE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_OPAQUE_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_OPAQUE_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_OPAQUE_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_V1_EB           0
#define TFC_MPC_TBL_EM_CHAIN_CMPL_V1_SB           0
#define TFC_MPC_TBL_EM_CHAIN_CMPL_V1_OFFS         0x8

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_V1(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_V1_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_V1_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_V1_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_V1(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_V1_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_V1_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_V1_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_HASH_MSB_EB           15
#define TFC_MPC_TBL_EM_CHAIN_CMPL_HASH_MSB_SB           4
#define TFC_MPC_TBL_EM_CHAIN_CMPL_HASH_MSB_OFFS         0x8

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_HASH_MSB(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_HASH_MSB_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_HASH_MSB_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_HASH_MSB(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_HASH_MSB_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_HASH_MSB_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_HASH_MSB_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_SCOPE_EB           28
#define TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_SCOPE_SB           24
#define TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_SCOPE_OFFS         0x8

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_TABLE_SCOPE(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_SCOPE_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_SCOPE_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_TABLE_SCOPE(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_SCOPE_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_SCOPE_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_SCOPE_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX_EB           57
#define TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX_SB           32
#define TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX_OFFS         0x8

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_TABLE_INDEX(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_TABLE_INDEX(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX2_EB           25
#define TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX2_SB           0
#define TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX2_OFFS         0x10

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_TABLE_INDEX2(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX2_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX2_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX2_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_TABLE_INDEX2(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX2_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX2_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX2_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX3_EB           57
#define TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX3_SB           32
#define TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX3_OFFS         0x10

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_TABLE_INDEX3(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX3_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX3_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX3_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_TABLE_INDEX3(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX3_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX3_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_TABLE_INDEX3_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_V2_EB           0
#define TFC_MPC_TBL_EM_CHAIN_CMPL_V2_SB           0
#define TFC_MPC_TBL_EM_CHAIN_CMPL_V2_OFFS         0x18

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_V2(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_V2_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_V2_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_V2_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_V2(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_V2_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_V2_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_V2_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_BKT_NUM_EB           39
#define TFC_MPC_TBL_EM_CHAIN_CMPL_BKT_NUM_SB           32
#define TFC_MPC_TBL_EM_CHAIN_CMPL_BKT_NUM_OFFS         0x18

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_BKT_NUM(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_BKT_NUM_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_BKT_NUM_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_BKT_NUM_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_BKT_NUM(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_BKT_NUM_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_BKT_NUM_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_BKT_NUM_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_NUM_ENTRIES_EB           42
#define TFC_MPC_TBL_EM_CHAIN_CMPL_NUM_ENTRIES_SB           40
#define TFC_MPC_TBL_EM_CHAIN_CMPL_NUM_ENTRIES_OFFS         0x18

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_NUM_ENTRIES(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_NUM_ENTRIES_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_NUM_ENTRIES_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_NUM_ENTRIES_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_NUM_ENTRIES(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_NUM_ENTRIES_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_NUM_ENTRIES_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_NUM_ENTRIES_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_CHAIN_UPD_EB           43
#define TFC_MPC_TBL_EM_CHAIN_CMPL_CHAIN_UPD_SB           43
#define TFC_MPC_TBL_EM_CHAIN_CMPL_CHAIN_UPD_OFFS         0x18

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SET_CHAIN_UPD(buf, val) \
	SET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_CHAIN_UPD_OFFS), \
		     (u64)(val), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_CHAIN_UPD_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_CHAIN_UPD_SB)
#define TFC_MPC_TBL_EM_CHAIN_CMPL_GET_CHAIN_UPD(buf) \
	GET_BITFLD64(TO_P64((buf), TFC_MPC_TBL_EM_CHAIN_CMPL_CHAIN_UPD_OFFS), \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_CHAIN_UPD_EB, \
		     TFC_MPC_TBL_EM_CHAIN_CMPL_CHAIN_UPD_SB)

#define TFC_MPC_TBL_EM_CHAIN_CMPL_SIZE     32

#endif /* __CFA_P70_MPC_CMPLS_H__ */
