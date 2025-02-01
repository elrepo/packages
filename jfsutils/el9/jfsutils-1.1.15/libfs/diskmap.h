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
#ifndef H_DISKMAP
#define H_DISKMAP

#include <jfs_types.h>		/* int8_t, int32_t, int64_t, uint32_t */

struct dmap;

int8_t ujfs_maxbuddy(unsigned char *);
int8_t ujfs_adjtree(int8_t *, int32_t, int32_t);
void ujfs_complete_dmap(struct dmap *, int64_t, int8_t *);
void ujfs_idmap_page(struct dmap *, uint32_t);
int32_t ujfs_getagl2size(int64_t, int32_t);

#endif				/* H_DISKMAP */
