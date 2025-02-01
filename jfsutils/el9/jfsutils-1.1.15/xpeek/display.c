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
 *   FUNCTION: Displays data in a variety of formats
 */
#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xpeek.h"
#include "jfs_endian.h"
#include "jfs_xtree.h"
#include "devices.h"

static void display_hex(char *, unsigned, unsigned);

extern unsigned type_jfs;

void display()
{
	int64_t addr;
	int64_t block;
	char *buffer;
	char cmdline[512];
	unsigned data_size;
	int format = 'a';
	int64_t len;
	unsigned length = 0;
	unsigned offset = 0;
	char *token;

	token = strtok(0, " 	\n");
	if (token == 0) {
		fputs("display: Please enter: block [offset [format [count]]]\ndisplay> ", stdout);
		fgets(cmdline, 512, stdin);
		token = strtok(cmdline, " 	\n");
		if (token == 0)
			return;
	}
	errno = 0;
	block = strtoull(token, 0, 0);
	if (block == 0 && errno) {
		fputs("display: invalid block\n\n", stderr);
		return;
	}
	if ((token = strtok(0, " 	\n"))) {
		offset = strtoul(token, 0, 16);
		if (offset == 0 && errno) {
			fputs("display: invalid offset\n\n", stderr);
			return;
		}
	}
	if ((token = strtok(0, " 	\n")))
		format = token[0];

	if ((token = strtok(0, " 	\n"))) {
		length = strtoul(token, 0, 0);
		if (length == 0 && errno) {
			fputs("display: invalid length\n\n", stderr);
			return;
		}
	}

	if (strtok(0, " 	\n")) {
		fputs("display: Too many arguments\n\n", stderr);
		return;
	}

	switch (format) {
	case 'a':
		data_size = 1;
		if (length == 0)
			length = bsize;
		break;
	case 'i':
		data_size = sizeof (struct dinode);
		if (length == 0)
			length = 1;
		break;
	case 'I':
		data_size = sizeof (struct iag);
		if (length == 0)
			length = 1;
		break;
	case 's':
		data_size = sizeof (struct superblock);
		if (length == 0)
			length = 1;
		break;
	case 'x':
		data_size = 4;
		if (length == 0)
			length = bsize / 4;
		break;
	case 'X':
		data_size = sizeof (xad_t);
		if (length == 0)
			length = 1;
		break;
	default:
		fputs("display:  invalid format\n\n", stderr);
		return;
	}

	addr = block << l2bsize;
	len = ((length * data_size) + offset + bsize - 1) & (~(bsize - 1));
	buffer = malloc(len);
	if (buffer == 0) {
		fputs("display: error calling malloc\n\n", stderr);
		return;
	}

	if (ujfs_rw_diskblocks(fp, addr, len, buffer, GET)) {
		fputs("display: ujfs_rw_diskblocks failed\n\n", stderr);
		free(buffer);
		return;
	}

	printf("Block: %lld     Real Address 0x%llx\n", (long long) block, (long long) addr);
	switch (format) {
	case 'a':
	case 'x':
		display_hex(&buffer[offset], length, offset);
		break;
	case 'i':
		{
			int i;
			struct dinode *inode = (struct dinode *) &buffer[offset];
			for (i = 0; i < length; i++, inode++) {
				/* swap if on big endian machine */
				ujfs_swap_dinode(inode, GET, type_jfs);
				display_inode(inode);
				if (more())
					return;
			}
		}
		break;
	case 'I':
		{
			int i;
			struct iag *iag = (struct iag *) & buffer[offset];
			for (i = 0; i < length; i++, iag++) {
				/* swap if on big endian machine */
				ujfs_swap_iag(iag);
				display_iag(iag);
				if (more())
					return;
			}
		}
		break;

	case 's':
		/* swap if on big endian machine */
		ujfs_swap_superblock((struct superblock *) &buffer[offset]);
		if (display_super((struct superblock *) &buffer[offset]) == XPEEK_CHANGED) {
			/* swap if on big endian machine */
			ujfs_swap_superblock((struct superblock *) &buffer[offset]);
			if (ujfs_rw_diskblocks(fp, addr, len, buffer, PUT))
				fputs("Display:  Error writing superblock!\n", stderr);
		}

		break;
	default:
		fputs("display:  specified format not yet supported\n\n", stderr);
		break;
	}

	free(buffer);
	return;
}

/*
 *	display_hex: display region in hex/ascii
 */
static void display_hex(char *addr, unsigned length, unsigned offset)
{
	uint8_t hextext[37];
	uint8_t asciitxt[17];
	uint8_t *x = (uint8_t *) addr, x1, x2;
	int i, j, k, l;

	hextext[36] = '\0';
	asciitxt[16] = '\0';	/* null end of string */

	l = 0;

	for (i = 1; i <= ((length + 15) / 16); i++) {
		if (i > 1 && ((i - 1) % 16) == 0)
			if (more())
				break;

		/* print address/offset */
		printf("%08x: ", offset + l);

		/* print 16 bytes per line */
		for (j = 0, k = 0; j < 16; j++, x++, l++) {
			if ((j % 4) == 0)
				hextext[k++] = ' ';
			if (l < length) {
				hextext[k++] = ((x1 = ((*x & 0xf0) >> 4)) < 10)
				    ? ('0' + x1) : ('A' + x1 - 10);
				hextext[k++] = ((x2 = (*x & 0x0f)) < 10)
				    ? ('0' + x2) : ('A' + x2 - 10);
				asciitxt[j] = ((*x < 0x20) || (*x >= 0x7f)) ? '.' : *x;
			} else {	/* byte not in range */
				hextext[k++] = ' ';
				hextext[k++] = ' ';
				asciitxt[j] = '.';
			}
		}
		printf("%s   |%s|\n", hextext, asciitxt);
	}
}
