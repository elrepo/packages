/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _SYS_UTIL_H_
#define _SYS_UTIL_H_

#include "linux/kernel.h"

#define INVALID_U64 U64_MAX
#define INVALID_U32 U32_MAX
#define INVALID_U16 U16_MAX
#define INVALID_U8 U8_MAX

#define ALIGN_256(x) ALIGN(x, 256)
#define ALIGN_128(x) ALIGN(x, 128)
#define ALIGN_64(x) ALIGN(x, 64)
#define ALIGN_32(x) ALIGN(x, 32)
#define ALIGN_16(x) ALIGN(x, 16)
#define ALIGN_8(x) ALIGN(x, 8)
#define ALIGN_4(x) ALIGN(x, 4)

#define NUM_ALIGN_UNITS(x, unit) (((x) + (unit) - (1)) / (unit))
#define SYS_NUM_WORDS_ALIGN_32BIT(x) (ALIGN_32(x) / BITS_PER_WORD)
#define SYS_NUM_WORDS_ALIGN_64BIT(x) (ALIGN_64(x) / BITS_PER_WORD)
#define SYS_NUM_WORDS_ALIGN_128BIT(x) (ALIGN_128(x) / BITS_PER_WORD)
#define SYS_NUM_WORDS_ALIGN_256BIT(x) (ALIGN_256(x) / BITS_PER_WORD)

#ifndef MAX
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif

#ifndef MIN
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif

#ifndef STRINGIFY
#define STRINGIFY(X) #X
#endif

/* Helper macros to get/set/clear Nth bit in a u8 bitmap */
#define BMP_GETBIT(BMP, N)                                                     \
	((*((u8 *)(BMP) + ((N) / 8)) >> ((N) % 8)) & 0x1)
#define BMP_SETBIT(BMP, N)                                                     \
	do {                                                                   \
		u32 n = (N);                                              \
		*((u8 *)(BMP) + (n / 8)) |= (0x1U << (n % 8));            \
	} while (0)
#define BMP_CLRBIT(BMP, N)                                                     \
	do {                                                                   \
		u32 n = (N);                                              \
		*((u8 *)(BMP) + (n / 8)) &=                               \
			(u8)(~(0x1U << (n % 8)));                         \
	} while (0)

#endif /* _SYS_UTIL_H_ */
