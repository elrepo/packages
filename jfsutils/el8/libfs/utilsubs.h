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
#ifndef _H_UJFS_UTILSUBS
#define _H_UJFS_UTILSUBS

#include <stdio.h>
#include <fcntl.h>

/*
 *	utilsubs.h
 */

/*
 *	function prototypes
 */
int32_t log2shift(uint32_t n);
char prompt(char *str);
int more(void);

static inline FILE *fopen_excl(const char *path, const char *mode)
{
	int fd;

	/* Yeah, mode is ignored, we only use this to open for read/write */
	fd = open(path, O_RDWR | O_EXCL, 0);
	if (fd < 0)
		return NULL;
	return fdopen(fd, mode);
}

int Is_Device_Mounted(char *);
int Is_Device_Type_JFS(char *);
#endif				/* _H_UJFS_UTILSUBS */
