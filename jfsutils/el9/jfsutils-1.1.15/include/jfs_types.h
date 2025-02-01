/*
 *   Copyright (c) International Business Machines Corp., 2000-2002
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
#ifndef _H_JFS_TYPES
#define	_H_JFS_TYPES

#include <sys/types.h> /* Neccessary for *BSD systems */
/*
 *	jfs_types.h:
 *
 * basic type/utility  definitions
 *
 * note: this header file must be the 1st include file
 * of JFS include list in all JFS .c file.
 */

#ifdef HAVE_STDINT_H
# include <stdint.h>
#else
# include <sys/types.h>
typedef u_int8_t uint8_t;
typedef u_int16_t uint16_t;
typedef u_int32_t uint32_t;
typedef u_int64_t uint64_t;
#endif

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
typedef enum {
	false = 0,
	true = 1
} bool;
#endif /* HAVE_STDBOOL_H */


/*
 * Unicase character.  Part of the ondisk format.
 */
typedef uint16_t UniChar;


/*
 * Almost identical to Linux's timespec, but not quite
 */
struct timestruc_t {
	uint32_t  tv_sec;
	uint32_t  tv_nsec;
};

/*
 *	handy
 */
#undef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#undef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#undef ROUNDUP
#define	ROUNDUP(x, y)	( ((x) + ((y) - 1)) & ~((y) - 1) )

#define LEFTMOSTONE	0x80000000
#define	HIGHORDER	0x80000000u	/* high order bit on            */
#define	ONES		0xffffffffu	/* all bit on                   */

/*
 *	physical xd (pxd)
 */
typedef struct {
	unsigned  len:24;
	unsigned  addr1:8;
	uint32_t  addr2;
} pxd_t;

/* xd_t field construction */

#define	PXDlength(pxd, length32)	((pxd)->len = __cpu_to_le24(length32))
#define	PXDaddress(pxd, address64)\
{\
	(pxd)->addr1 = ((int64_t)address64) >> 32;\
	(pxd)->addr2 = __cpu_to_le32((address64) & 0xffffffff);\
}

/* xd_t field extraction */
#define	lengthPXD(pxd)	__le24_to_cpu((pxd)->len)
#define	addressPXD(pxd)\
	( ((int64_t)((pxd)->addr1)) << 32 | __le32_to_cpu((pxd)->addr2))

/* pxd list */
struct pxdlist {
	int16_t  maxnpxd;
	int16_t  npxd;
	pxd_t    pxd[8];
};


/*
 *	data extent descriptor (dxd)
 */
typedef struct {
	unsigned  flag:8;	/* 1: flags */
	unsigned  rsrvd:24;	/* 3: */
	uint32_t  size;		/* 4: size in byte */
	unsigned  len:24;	/* 3: length in unit of fsblksize */
	unsigned  addr1:8;	/* 1: address in unit of fsblksize */
	uint32_t  addr2;	/* 4: address in unit of fsblksize */
} dxd_t;			/* - 16 - */

/* dxd_t flags */
#define	DXD_INDEX	0x80	/* B+-tree index */
#define	DXD_INLINE	0x40	/* in-line data extent */
#define	DXD_EXTENT	0x20	/* out-of-line single extent */
#define	DXD_FILE	0x10	/* out-of-line file (inode) */
#define DXD_CORRUPT	0x08	/* Inconsistency detected */

/* dxd_t field construction
 *	Conveniently, the PXD macros work for DXD
 */
#define	DXDlength	PXDlength
#define	DXDaddress	PXDaddress
#define	lengthDXD	lengthPXD
#define	addressDXD	addressPXD

/*
 *      directory entry argument
 */
struct component_name {
	int namlen;
	UniChar *name;
};


/*
 *	DASD limit information - stored in directory inode
 */
struct dasd {
	uint8_t   thresh;		/* Alert Threshold (in percent) */
	uint8_t   delta;		/* Alert Threshold delta (in percent)   */
	uint8_t   rsrvd1;
	uint8_t   limit_hi;		/* DASD limit (in logical blocks)       */
	uint32_t  limit_lo;		/* DASD limit (in logical blocks)       */
	uint8_t   rsrvd2[3];
	uint8_t   used_hi;		/* DASD usage (in logical blocks)       */
	uint32_t  used_lo;		/* DASD usage (in logical blocks)       */
};

#define DASDLIMIT(dasdp) \
	(((uint64_t)((dasdp)->limit_hi) << 32) + __le32_to_cpu((dasdp)->limit_lo))
#define setDASDLIMIT(dasdp, limit)\
{\
	(dasdp)->limit_hi = ((uint64_t)limit) >> 32;\
	(dasdp)->limit_lo = __cpu_to_le32(limit);\
}
#define DASDUSED(dasdp) \
	(((uint64_t)((dasdp)->used_hi) << 32) + __le32_to_cpu((dasdp)->used_lo))
#define setDASDUSED(dasdp, used)\
{\
	(dasdp)->used_hi = ((uint64_t)used) >> 32;\
	(dasdp)->used_lo = __cpu_to_le32(used);\
}

#endif				/* !_H_JFS_TYPES */
