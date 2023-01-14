/*
 *   Copyright (c) Christoph Hellwig, 2002
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _H_JFS_BYTEORDER
#define	_H_JFS_BYTEORDER

#if HAVE_SYS_BYTEORDER_H
# include <sys/byteorder.h>
#elif HAVE_MACHINE_ENDIAN_H
# include <machine/endian.h>
#elif HAVE_ENDIAN_H
# include <endian.h>
#endif

#define __swab16(x) \
({ \
	uint16_t __x = (x); \
	((uint16_t)( \
		(((uint16_t)(__x) & (uint16_t)0x00ffU) << 8) | \
		(((uint16_t)(__x) & (uint16_t)0xff00U) >> 8) )); \
})

#define __swab24(x) \
({ \
	uint32_t __x = (x); \
	((uint32_t)( \
		((__x & (uint32_t)0x000000ffUL) << 16) | \
		 (__x & (uint32_t)0x0000ff00UL)        | \
		((__x & (uint32_t)0x00ff0000UL) >> 16) )); \
})

#define __swab32(x) \
({ \
	uint32_t __x = (x); \
	((uint32_t)( \
		(((uint32_t)(__x) & (uint32_t)0x000000ffUL) << 24) | \
		(((uint32_t)(__x) & (uint32_t)0x0000ff00UL) <<  8) | \
		(((uint32_t)(__x) & (uint32_t)0x00ff0000UL) >>  8) | \
		(((uint32_t)(__x) & (uint32_t)0xff000000UL) >> 24) )); \
})

#define __swab64(x) \
({ \
	uint64_t __x = (x); \
	((uint64_t)( \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000000000ffULL) << 56) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000000000ff00ULL) << 40) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000000000ff0000ULL) << 24) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00000000ff000000ULL) <<  8) | \
	    (uint64_t)(((uint64_t)(__x) & (uint64_t)0x000000ff00000000ULL) >>  8) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x0000ff0000000000ULL) >> 24) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0x00ff000000000000ULL) >> 40) | \
		(uint64_t)(((uint64_t)(__x) & (uint64_t)0xff00000000000000ULL) >> 56) )); \
})


#if (BYTE_ORDER == LITTLE_ENDIAN)
	#define __cpu_to_le16(x) ((uint16_t)(x))
	#define __cpu_to_le24(x) ((uint32_t)(x))
	#define __cpu_to_le32(x) ((uint32_t)(x))
	#define __cpu_to_le64(x) ((uint64_t)(x))
	#define __le16_to_cpu(x) ((uint16_t)(x))
	#define __le24_to_cpu(x) ((uint32_t)(x))
	#define __le32_to_cpu(x) ((uint32_t)(x))
	#define __le64_to_cpu(x) ((uint64_t)(x))
#elif (BYTE_ORDER == BIG_ENDIAN)
	#define __cpu_to_le16(x) __swab16(x)
	#define __cpu_to_le24(x) __swab24(x)
	#define __cpu_to_le32(x) __swab32(x)
	#define __cpu_to_le64(x) __swab64(x)
	#define __le16_to_cpu(x) __swab16(x)
	#define __le24_to_cpu(x) __swab24(x)
	#define __le32_to_cpu(x) __swab32(x)
	#define __le64_to_cpu(x) __swab64(x)
#else
# error "JFS works only on big- or littleendian machines"
#endif

#endif				/* !_H_JFS_BYTEORDER */
