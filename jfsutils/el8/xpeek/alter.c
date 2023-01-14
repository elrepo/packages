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
#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xpeek.h"
#include "devices.h"
#include "utilsubs.h"

void alter()
{
	int64_t address;
	int64_t block;
	uint8_t *buffer;
	uint8_t byte;
	char cmdline[512];
	char *hexstring;
	unsigned hex_length;
	unsigned length;
	unsigned offset;
	char *ptr;
	char *token;

	token = strtok(0, " 	\n");
	if (token == 0) {
		fputs("alter: Please enter: block offset hex-digits> ", stdout);
		fgets(cmdline, 512, stdin);
		token = strtok(cmdline, " 	\n");
		if (token == 0)
			return;
	}
	errno = 0;
	block = strtoull(token, 0, 0);
	if (block == 0 && errno) {
		fputs("alter: invalid block\n\n", stderr);
		return;
	}
	address = block << l2bsize;

	token = strtok(0, " 	\n");
	if (!token) {
		fputs("alter:  Not enought arguments!\n", stderr);
		return;
	}
	offset = strtoul(token, 0, 16);
	if (offset == 0 && errno) {
		fputs("alter: invalid offset\n", stderr);
		return;
	}
	hexstring = strtok(0, " 	\n");
	if (!hexstring) {
		fputs("alter: Not enough arguments!\n", stderr);
		return;
	}
	if (strtok(0, " 	\n")) {
		fputs("alter: Too many arguments!\n", stderr);
		return;
	}
	hex_length = strlen(hexstring);
	if (hex_length & 1) {
		/* odd number of hex digits */
		fputs("alter: hex string must have even number of digits!\n", stderr);
		return;
	}

	/* Round length of data up to next full physical block */

	length = (offset + hex_length + bsize - 1) & ~(bsize - 1);

	buffer = (char *) malloc(length);
	if (!buffer) {
		fputs("alter: malloc failure!\n", stderr);
		return;
	}
	if (ujfs_rw_diskblocks(fp, address, length, buffer, GET)) {
		fputs("alter: failed reading disk data!\n", stderr);
		free(buffer);
		return;
	}
	for (ptr = hexstring; *ptr; ptr++) {
		if (*ptr >= '0' && *ptr <= '9')
			byte = *ptr - '0';
		else if (*ptr >= 'a' && *ptr <= 'f')
			byte = *ptr - 'a' + 10;
		else if (*ptr >= 'A' && *ptr <= 'F')
			byte = *ptr - 'A' + 10;
		else {
			fputs("alter: invalid hex digit!\n", stderr);
			free(buffer);
			return;
		}

		ptr++;
		byte <<= 4;

		if (*ptr >= '0' && *ptr <= '9')
			byte += *ptr - '0';
		else if (*ptr >= 'a' && *ptr <= 'f')
			byte += *ptr - 'a' + 10;
		else if (*ptr >= 'A' && *ptr <= 'F')
			byte += *ptr - 'A' + 10;
		else {
			fputs("alter: invalid hex digit!\n", stderr);
			free(buffer);
			return;
		}

		buffer[offset++] = byte;
	}
	if (ujfs_rw_diskblocks(fp, address, length, buffer, PUT))
		fputs("alter: failed writing disk data!\n", stderr);

	free(buffer);
	return;
}
