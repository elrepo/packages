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
/*
 *   io.c - I/O routines
 */
#include <config.h>
#include <stdlib.h>
#include <string.h>
#include "xpeek.h"

/* libfs includes */
#include <devices.h>

int xRead(int64_t address, unsigned count, char *buffer)
{
	int64_t block_address;
	char *block_buffer;
	int64_t length;
	unsigned offset;

	offset = address & (bsize - 1);
	length = (offset + count + bsize - 1) & ~(bsize - 1);

	if ((offset == 0) & (length == count))
		return ujfs_rw_diskblocks(fp, address, count, buffer, GET);

	block_address = address - offset;
	block_buffer = (char *) malloc(length);
	if (block_buffer == 0)
		return 1;

	if (ujfs_rw_diskblocks(fp, block_address, length, block_buffer, GET)) {
		free(block_buffer);
		return 1;
	}
	memcpy(buffer, block_buffer + offset, count);
	free(block_buffer);
	return 0;
}

int xWrite(int64_t address, unsigned count, char *buffer)
{
	int64_t block_address;
	char *block_buffer;
	int64_t length;
	unsigned offset;

	offset = address & (bsize - 1);
	length = (offset + count + bsize - 1) & ~(bsize - 1);

	if ((offset == 0) & (length == count))
		return ujfs_rw_diskblocks(fp, address, count, buffer, PUT);

	block_address = address - offset;
	block_buffer = (char *) malloc(length);
	if (block_buffer == 0)
		return 1;

	if (ujfs_rw_diskblocks(fp, block_address, length, block_buffer, GET)) {
		free(block_buffer);
		return 1;
	}
	memcpy(block_buffer + offset, buffer, count);
	if (ujfs_rw_diskblocks(fp, block_address, length, block_buffer, PUT)) {
		free(block_buffer);
		return 1;
	}
	free(block_buffer);
	return 0;
}
