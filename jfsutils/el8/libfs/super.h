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
#ifndef H_SUPER
#define	H_SUPER

#include "utilsubs.h"

struct superblock;
struct logsuper;

extern int ujfs_validate_logsuper(struct logsuper *);
extern int ujfs_validate_super(struct superblock *);
extern int ujfs_put_superblk(FILE *, struct superblock *, int16_t);
extern int ujfs_get_superblk(FILE *, struct superblock *, int32_t);
extern int ujfs_put_logsuper(FILE *, struct logsuper *);
extern int ujfs_get_logsuper(FILE *, struct logsuper *);
extern int inrange(uint32_t, uint32_t, uint32_t);

#endif
