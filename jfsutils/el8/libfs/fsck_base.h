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
#ifndef _FSCK_BASE_H_
#define _FSCK_BASE_H_

/*
   --------- Defines shared among the fsck modules ---------
*/

#define ISDIR(m)         (((m)&(IFMT)) == (IFDIR))
#define ISREG(m)         (((m)&(IFMT)) == (IFREG))
#define ISLNK(m)         (((m)&(IFMT)) == (IFLNK))
#define ISBLK(m)         (((m)&(IFMT)) == (IFBLK))
#define ISCHR(m)         (((m)&(IFMT)) == (IFCHR))
#define ISFIFO(m)        (((m)&(IFMT)) == (IFFIFO))
#define ISSOCK(m)        (((m)&(IFMT)) == (IFSOCK))

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 * some useful constants
 */
#define BYTESPERPAGE      4096
#define log2BYTESPERPAGE  12
#define BITSPERPAGE       (4096*8)
#define log2BITSPERPAGE   15
#define BITSPERDWORD      32
#define log2BITSPERDWORD  5
#define BYTESPERDWORD     4
#define log2BYTESPERDWORD 2
#define BITSPERBYTE       8
#define log2BITSPERBYTE   3
#define MEMSEGSIZE        (64*1024)
#define log2BYTESPERKBYTE 10

#endif
