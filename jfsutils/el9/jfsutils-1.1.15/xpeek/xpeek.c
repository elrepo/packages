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
 *   FUNCTION: Display and Alter structures in an unmounted filesystem
 */
#include <config.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xpeek.h"
#include "jfs_endian.h"
#include "jfs_logmgr.h"
#include "jfs_version.h"
#include "devices.h"
#include "inode.h"
#include "super.h"

extern void superblock2(void);

/* endian routines */
unsigned type_jfs;

/* Global Data */
int bsize;			/* aggregate block size         */
FILE *fp;				/* Used by libfs routines       */
short l2bsize;			/* log2 of aggregate block size */
int64_t AIT_2nd_offset;		/* Used by find_iag routines    */
int64_t fsckwsp_offset;		/* Used by fsckcbbl routines    */
int64_t jlog_super_offset;	/* Used by fsckcbbl routines    */

void usage(void);

void usage()
{
	fputs("Usage: jfs_debugfs <block device>\n", stderr);
}

int main(int argc, char **argv)
{
	int cmd_len;
	char *command;
	char command_line[512];
	char *device;
	struct superblock sb;

	printf("jfs_debugfs version %s, %s\n", VERSION, JFSUTILS_DATE);

	/* Put the console into UTF8 mode. */
	printf("\33%s", "%G");

	/* Check arguments */
	if (argc != 2) {
		usage();
		exit(1);
	}
	device = argv[1];

	/* Open device */
	fp = fopen(device, "r+");
	if (fp == NULL) {
		fprintf(stderr, "Error: Cannot open device %s.\n", device);
		exit(1);
	}

	/* Get block size information from the superblock       */
	if (ujfs_get_superblk(fp, &sb, 1)) {
		fputs("jfs_debugfs: error reading primary superblock\n", stderr);
		if (ujfs_get_superblk(fp, &sb, 0)) {
			fputs("jfs_debugfs: error reading secondary superblock\n", stderr);
			goto errorout;
		} else
			fputs("jfs_debugfs: using secondary superblock\n", stderr);
	}

	type_jfs = sb.s_flag;

	bsize = sb.s_bsize;
	l2bsize = sb.s_l2bsize;
	AIT_2nd_offset = addressPXD(&(sb.s_ait2)) * bsize;
	fsckwsp_offset = addressPXD(&(sb.s_fsckpxd)) << l2bsize;
	jlog_super_offset = (addressPXD(&(sb.s_logpxd)) << l2bsize)
	    + LOGPSIZE;

	printf("\nAggregate Block Size: %d\n\n", bsize);

	/* Main Loop */

	fputs("> ", stdout);
	fflush(stdout);
	while (fgets(command_line, 512, stdin)) {
		command = strtok(command_line, " \t\n");	/* space or tab */
		if (command && *command) {
			cmd_len = strlen(command);
			if (strncmp(command, "alter", cmd_len) == 0)
				alter();
			else if (strncmp(command, "btree", cmd_len) == 0) {
				fputs("btree command is not yet implemented.\n", stderr);
			} else if (cmd_len > 1 && strncmp(command, "cbblfsck", cmd_len) == 0)
				cbblfsck();
			else if (cmd_len > 2 && strncmp(command, "directory", cmd_len) == 0)
				directory();
			else if (cmd_len > 1 && strncmp(command, "dmap", cmd_len) == 0)
				dmap();
			else if (cmd_len > 1 && strncmp(command, "dtree", cmd_len) == 0)
				dtree();
			else if (cmd_len > 1 && strncmp(command, "xtree", cmd_len) == 0)
				xtree();
			else if (strncmp(command, "display", cmd_len) == 0)
				display();
			else if (cmd_len > 4 && strncmp(command, "fsckwsphdr", cmd_len) == 0)
				fsckwsphdr();
			else if (strncmp(command, "help", cmd_len) == 0)
				help();
			else if (cmd_len > 1 && strncmp(command, "iag", cmd_len) == 0)
				iag();
			else if (strncmp(command, "inode", cmd_len) == 0)
				inode();
			else if (cmd_len > 3 && strncmp(command, "logsuper", cmd_len) == 0)
				logsuper();
			else if (strncmp(command, "quit", cmd_len) == 0)
				break;
			else if ((cmd_len > 1) && strncmp(command, "set", cmd_len) == 0) {
				fputs("set command is not yet implemented.\n", stderr);
			} else if ((cmd_len > 1) && strncmp(command, "superblock", cmd_len) == 0)
				superblock();
			else if (cmd_len > 2 && strncmp(command, "s2perblock", cmd_len) == 0)
				superblock2();
			else if (strncmp(command, "unset", cmd_len) == 0) {
				fputs("unset command is not yet implemented.\n", stderr);
			} else
				fprintf(stderr, "Invalid command: %s\n", command);
		}
		fputs("> ", stdout);
		fflush(stdout);
	}
      errorout:
	ujfs_flush_dev(fp);
	fclose(fp);
	return 0;
}
