/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_ACTION_HANDLE_H_
#define _TFC_ACTION_HANDLE_H_

#define TFC_POOL_TSID_ACTION_HANDLE_MASK	  0x0000003F000000000ULL
#define TFC_POOL_TSID_ACTION_HANDLE_SFT		  36
#define TFC_RECORD_SIZE_ACTION_HANDLE_MASK	  0x00000000F00000000ULL
#define TFC_RECORD_SIZE_ACTION_HANDLE_SFT	  32
#define TFC_EM_REC_OFFSET_ACTION_HANDLE_MASK	  0x00000000007FFFFFFULL
#define TFC_EM_REC_OFFSET_ACTION_HANDLE_SFT	  0

#define TFC_ACTION_HANDLE_MASK ( \
	TFC_POOL_TSID_ACTION_HANDLE_MASK | \
	TFC_RECORD_SIZE_ACTION_HANDLE_MASK | \
	TFC_EM_REC_OFFSET_ACTION_HANDLE_MASK)

static inline void tfc_get_fields_from_action_handle(const u64 *act_handle, u8 *tsid,
						     u32 *record_size, u32 *action_offset)
{
	*tsid = (u8)((*act_handle & TFC_POOL_TSID_ACTION_HANDLE_MASK) >>
		 TFC_POOL_TSID_ACTION_HANDLE_SFT);
	*record_size =
		(u32)((*act_handle & TFC_RECORD_SIZE_ACTION_HANDLE_MASK) >>
		 TFC_RECORD_SIZE_ACTION_HANDLE_SFT);
	*action_offset =
		(u32)((*act_handle & TFC_EM_REC_OFFSET_ACTION_HANDLE_MASK) >>
		 TFC_EM_REC_OFFSET_ACTION_HANDLE_SFT);
}

static inline u64 tfc_create_action_handle(u8 tsid, u32 record_size, u32 action_offset)
{
	u64 act_handle = 0ULL;

	act_handle |=
		((((u64)tsid) << TFC_POOL_TSID_ACTION_HANDLE_SFT) &
		TFC_POOL_TSID_ACTION_HANDLE_MASK);
	act_handle |=
		((((u64)record_size) << TFC_RECORD_SIZE_ACTION_HANDLE_SFT) &
		TFC_RECORD_SIZE_ACTION_HANDLE_MASK);
	act_handle |=
		((((u64)action_offset) << TFC_EM_REC_OFFSET_ACTION_HANDLE_SFT) &
		TFC_EM_REC_OFFSET_ACTION_HANDLE_MASK);

	return act_handle;
}

#define TFC_ACTION_GET_POOL_ID(action_offset, pool_sz_exp) \
	((action_offset) >> (pool_sz_exp))

#define TFC_GET_32B_OFFSET_ACT_HANDLE(act_32byte_offset, act_handle)     \
	{                                                                \
		(act_32byte_offset) = (u32)((*(act_handle) &	 \
				TFC_EM_REC_OFFSET_ACTION_HANDLE_MASK) >> \
				TFC_EM_REC_OFFSET_ACTION_HANDLE_SFT);    \
	}

#define TFC_GET_8B_OFFSET(act_8byte_offset, act_32byte_offset) \
	{ (act_8byte_offset) = ((act_32byte_offset) << 2); }

#endif /* _TFC_ACTION_HANDLE_H_ */
