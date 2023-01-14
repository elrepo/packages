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
#include <string.h>
#include <stdio.h>

/* JFS includes	*/
#include "jfs_filsys.h"
#include "jfs_superblock.h"
#include "jfs_logmgr.h"

/*--------------------------------------------------------------------
 * NAME: build_flag_string
 *
 * FUNCTION: build a string of text descriptions of
 *           the set flags in the superblock
 *
 * PARAMETERS:
 *      flags       - flags field from the superblock
 *      flag_string - returned string of flag descriptions
 */
void build_flag_string(uint32_t flags, char *flag_string)
{
	char *string_ptr = flag_string;

	if (flags & JFS_LINUX) {
		strcpy(string_ptr, "JFS_LINUX  ");
		string_ptr += 11;
	}
	if (flags & JFS_OS2) {
		strcpy(string_ptr, "JFS_OS2  ");
		string_ptr += 9;
	}
	if (flags & JFS_COMMIT) {
		strcpy(string_ptr, "JFS_COMMIT  ");
		string_ptr += 12;
	}
	if (flags & JFS_GROUPCOMMIT) {
		strcpy(string_ptr, "JFS_GROUPCOMMIT  ");
		string_ptr += 17;
	}
	if (flags & JFS_LAZYCOMMIT) {
		strcpy(string_ptr, "JFS_LAZYCOMMIT  ");
		string_ptr += 16;
	}
	if (flags & JFS_INLINELOG) {
		strcpy(string_ptr, "JFS_INLINELOG  ");
		string_ptr += 15;
	}
	if (flags & JFS_BAD_SAIT) {
		strcpy(string_ptr, "JFS_BAD_SAIT  ");
		string_ptr += 14;
	}
	if (flags & JFS_SPARSE) {
		strcpy(string_ptr, "JFS_SPARSE  ");
		string_ptr += 12;
	}
	if (flags & JFS_DASD_ENABLED) {
		strcpy(string_ptr, "JFS_DASD_ENABLED  ");
		string_ptr += 18;
	}
	if (flags & JFS_DASD_PRIME) {
		strcpy(string_ptr, "JFS_DASD_PRIME  ");
		string_ptr += 16;
	}
	strcpy(string_ptr, "\0");

	return;
}

/*--------------------------------------------------------------------
 * NAME: display_super
 *
 * FUNCTION: display the JFS file system superblock
 *
 * PARAMETERS:
 *      sb  - pointer to the file system superblock
 *
 * SAMPLE OUTPUT:

 JFS magic number:	'JFS1'
 JFS version:		2
 JFS state:		clean
 JFS flags:		JFS_LINUX  JFS_COMMIT  JFS_GROUPCOMMIT
 Aggregate block size:	4096 bytes
 Aggregate size:	2055784 blocks
 Physical block size:	512 bytes
 Allocation group size:	8192 aggregate blocks
 Log device number:	0x3f06
 Filesystem creation:	Fri Aug  2 14:48:53 2002
 File system UUID:	7940a3b8-0bb0-4c0d-a389-630619783c6e
 Volume label:		'                '
 External log UUID:	47c80a1d-7f25-4c61-9596-f33e463e5b38

 */
void display_super(struct superblock *sb)
{
	char *state;
	char uuid_unparsed[37];
	char flag_string[142];
	time_t tm;

	switch (sb->s_state) {
	case FM_CLEAN:
		state = "clean";
		break;
	case FM_MOUNT:
		state = "mounted";
		break;
	case FM_DIRTY:
		state = "dirty";
		break;
	case FM_LOGREDO:
		state = "logredo";
		break;
	default:
		state = "unknown";
		break;
	}

	build_flag_string(sb->s_flag, flag_string);

	printf("\nJFS filesystem superblock:\n\n");
	printf("JFS magic number:\t'%4.4s'\n", sb->s_magic);
	printf("JFS version:\t\t%d\n", sb->s_version);
	printf("JFS state:\t\t%s\n", state);
	printf("JFS flags:\t\t%s\n", flag_string);
	printf("Aggregate block size:\t%d bytes\n", sb->s_bsize);
	printf("Aggregate size:\t\t%lld blocks\n", (long long) sb->s_size);
	printf("Physical block size:\t%d bytes\n", sb->s_pbsize);
	printf("Allocation group size:\t%d aggregate blocks\n", sb->s_agsize);
	printf("Log device number:\t0x%x\n", sb->s_logdev);
	tm = sb->s_time.tv_sec;
	printf("Filesystem creation:\t%s", ctime(&tm));
	/*
	 * JFS version 2 incorporated new fields for
	 * UUID, log UUID, and a 16 char volume label
	 * into the superblock.  Account for those here.
	 */
	if (sb->s_version == JFS_VERSION) {
		uuid_unparse(sb->s_uuid, uuid_unparsed);
		printf("File system UUID:\t%s\n", uuid_unparsed);
		printf("Volume label:\t\t'%.16s'\n", sb->s_label);
		uuid_unparse(sb->s_loguuid, uuid_unparsed);
		printf("External log UUID:\t%s\n", uuid_unparsed);
	} else {
		printf("Volume label:\t\t'%.11s'\n", sb->s_fpack);
	}
	printf("\n");

	return;
}

/*--------------------------------------------------------------------
 * NAME: display_logsuper
 *
 * FUNCTION: display the JFS log superblock
 *
 * PARAMETERS:
 *      lsp  - pointer to the log superblock
 *
 * SAMPLE OUTPUT:

 JFS log magic number:  0x87654321
 JFS log version:	1
 Log opened/mounted:	3
 Logical block size:	4096 bytes
 Log size:		32768 blocks
 Log flags:		JFS_LINUX  JFS_COMMIT  JFS_GROUPCOMMIT
 Log state:		LOGMOUNT
 Log device UUID:	47c80a1d-7f25-4c61-9596-f33e463e5b38
 Log volume label:	'       JFSLogVol'
 Active file systems:
     active[0]:  7940a3b8-0bb0-4c0d-a389-630619783c6e

 */
void display_logsuper(struct logsuper * lsp)
{
	char *state;
	char uuid_unparsed[37];
	int i;
	bool active_fs = false;
	char flag_string[142];

	switch (lsp->state) {
	case LOGMOUNT:
		state = "LOGMOUNT";
		break;
	case LOGREDONE:
		state = "LOGREDONE";
		break;
	case LOGWRAP:
		state = "LOGWRAP";
		break;
	case LOGREADERR:
		state = "LOGREADERR";
		break;
	default:
		state = "Unknown";
		break;
	}

	build_flag_string(lsp->flag, flag_string);

	printf("\nJFS external log superblock:\n\n");
	printf("JFS log magic number:\t0x%x\n", lsp->magic);
	printf("JFS log version:\t%d\n", lsp->version);
	printf("Log opened/mounted:\t%d\n", lsp->serial);
	printf("Logical block size:\t%d bytes\n", lsp->bsize);
	printf("Log size:\t\t%d blocks\n", lsp->size);
	printf("Log flags:\t\t%s\n", flag_string);
	printf("Log state:\t\t%s\n", state);
	uuid_unparse(lsp->uuid, uuid_unparsed);
	printf("Log device UUID:\t%s\n", uuid_unparsed);
	printf("Log volume label:\t'%16s'\n", lsp->label);
	printf("Active file systems:");
	for (i = 0; i < MAX_ACTIVE; i++) {
		uuid_unparse(lsp->active[i], uuid_unparsed);
		/* only print active (non-zero uuid) file systems */
		if (strncmp(uuid_unparsed, "00000000-0000-0000-0000-000000000000", 36)) {
			printf("\n    active[%d]:  %s", i, uuid_unparsed);
			active_fs = true;
		}
	}

	if (!active_fs) {
		printf("\tNone active.\n");
	}
	printf("\n");

	return;
}
