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
#ifndef H_INODES
#define H_INODES

#include "devices.h"

typedef enum { inline_data, extent_data, max_extent_data, no_data } ino_data_type;

int init_aggr_inode_table(int, FILE *, struct dinode *, int, int64_t, int64_t, int, unsigned);
int init_fileset_inode_table(int, FILE *, int64_t *, int *, int64_t, int64_t, unsigned);
int init_fileset_inodes(int, FILE *, int64_t, int, int64_t, unsigned);

void init_inode(struct dinode *, int, unsigned, int64_t, int64_t, int64_t, mode_t,
		ino_data_type, int64_t, int, unsigned);

#endif
