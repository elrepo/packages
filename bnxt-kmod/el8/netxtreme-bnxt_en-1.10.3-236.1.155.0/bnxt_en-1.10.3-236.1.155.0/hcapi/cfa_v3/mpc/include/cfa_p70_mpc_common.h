/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */
#ifndef __CFA_P70_MPC_COMMON_H__
#define __CFA_P70_MPC_COMMON_H__

/* Convert a u8* ptr + offset to a u32* ptr */
#define TO_P32(buf, offs)              ((u32 *)((buf) + (offs)))
/* Convert a u8* ptr + offset to a u64* ptr */
#define TO_P64(buf, offs)              ((u64 *)((buf) + (offs)))

static inline u32 MASK_32_W(u8 eb, u8 sb)
{
	return ((1UL << ((eb - sb) + 1)) - 1);
}

static inline u32 MASK_32(u8 eb, u8 sb)
{
	return (((1UL << ((eb - sb) + 1)) - 1) << sb);
}

static inline u32 GET_BITFLD32(u32 *fld, u8 eb, u8 sb)
{
	return ((*fld >> sb) & MASK_32_W(eb, sb));
}

static inline void SET_BITFLD32(u32 *fld, u32 val, u8 eb, u8 sb)
{
	*fld &= ~MASK_32(eb, sb);
	*fld |= ((val << sb) & MASK_32(eb, sb));
}

static inline u32 GET_FLD32(u32 *fld)
{
	return *fld;
}

static inline void SET_FLD32(u32 *fld, u32 val)
{
	*fld = val;
}

static inline u64 MASK_64_W(u8 eb, u8 sb)
{
	return ((1ULL << ((eb - sb) + 1)) - 1);
}

static inline u64 MASK_64(u8 eb, u8 sb)
{
return (((1ULL << ((eb - sb) + 1)) - 1) << sb);
}

static inline u64 GET_BITFLD64(u64 *fld, u8 eb, u8 sb)
{
	return ((*fld >> sb) & MASK_64_W(eb, sb));
}

static inline void SET_BITFLD64(u64 *fld, u64 val, u8 eb, u8 sb)
{
	*fld &= ~MASK_64(eb, sb);
	*fld |= ((val << sb) & MASK_64(eb, sb));
}

static inline u64 GET_FLD64(u64 *fld)
{
	return *fld;
}

static inline void SET_FLD64(u64 *fld, u64 val)
{
	*fld = val;
}

#endif /* __CFA_P70_MPC_COMMON_H__ */
