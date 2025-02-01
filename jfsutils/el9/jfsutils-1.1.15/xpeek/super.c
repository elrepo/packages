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
 *   FUNCTION:	Display and Alter primary or secondary superblock
 */
#include <config.h>
#include <stdlib.h>
#include <string.h>

#include "xpeek.h"
#include "jfs_filsys.h"
#include "jfs_byteorder.h"
#include "super.h"

void superblock()
{
	char *arg;
	int is_primary = 1;
	struct superblock super;

	if ((arg = strtok(0, " 	\n"))) {	/* space & tab  */
		if (strcmp(arg, "p") == 0)
			is_primary = 1;
		else if (strcmp(arg, "s") == 0)
			is_primary = 0;
		else {
			fprintf(stderr, "superblock: invalid parameter '%s'\n\n", arg);
			return;
		}
	} else if (strtok(0, " 	\n")) {
		fputs("superblock: too many arguments.\n\n", stderr);
		return;
	}

	if (ujfs_get_superblk(fp, &super, is_primary)) {
		fputs("superblock: error returned from ujfs_get_superblk\n\n", stderr);
		return;
	}

	if (display_super(&super) == XPEEK_CHANGED)
		if (ujfs_put_superblk(fp, &super, (short) is_primary))
			fputs("superblock: error returned from ujfs_put_superblk\n\n", stderr);
	return;
}

/*****************************************************************************
 ****************  Sample output of display_super():

[1] s_magic:		'JFS1'		[15] s_ait2.addr1:	0x00
[2] s_version:		2		[16] s_ait2.addr2:	0x00000037
[3] s_size:	0x00000000001f5e68	     s_ait2.address:	55
[4] s_bsize:		4096		[17] s_logdev:		0x00003f06
[5] s_l2bsize:		12		[18] s_logserial:	0x00000000
[6] s_l2bfactor:	3		[19] s_logpxd.len:	0
[7] s_pbsize:		512		[20] s_logpxd.addr1:	0x00
[8] s_l2pbsize:		9		[21] s_logpxd.addr2:	0x00000000
[9] pad:		Not Displayed	     s_logpxd.address:	0
[10] s_agsize:		0x00002000	[22] s_fsckpxd.len:	59
[11] s_flag:		0x10200100	[23] s_fsckpxd.addr1:	0x00
			JFS_LINUX	[24] s_fsckpxd.addr2:	0x0003ebcd
	JFS_COMMIT	JFS_GROUPCOMMIT	     s_fsckpxd.address:	256973
					[25] s_time.tv_sec:	0x3d332347
					[26] s_time.tv_nsec:	0x00000000
					[27] s_fpack:		'        '
[12] s_state:		0x00000000	[28] s_uuid:		13d0dbde-2cca-468a-a3d8-2ef2c016bab5
		 FM_CLEAN         	[29] s_label:		'                '
[13] s_compress:	0		[30] s_loguuid:		641a9557-484a-4de4-9ab5-4827c32910b9
[14] s_ait2.len:	4

******************************************************************************/

int display_super(struct superblock *sb)
{
	char *os2_platform;
	char *linux_platform;
	char cmdline[512];
	int field;
	int rc = XPEEK_OK;
	char *state;
	char *token;
	char *commit;
	char *groupcommit;
	char *lazycommit;
	char *inlinelog;
	char *badsait;
	char *sparse;
	char *dasdenabled;
	char *dasdprime;
	char uuid_unparsed[37];
	int uuid_rc;

      changed:
	switch (sb->s_state) {
	case FM_CLEAN:
		state = "FM_CLEAN";
		break;
	case FM_MOUNT:
		state = "FM_MOUNT";
		break;
	case FM_DIRTY:
		state = "FM_DIRTY";
		break;
	case FM_LOGREDO:
		state = "FM_LOGREDO";
		break;
	default:
		state = "Unknown State";
		break;
	}
	if (sb->s_flag & JFS_LINUX)
		linux_platform = "JFS_LINUX";
	else
		linux_platform = "         ";

	if (sb->s_flag & JFS_OS2)
		os2_platform = "JFS_OS2";
	else
		os2_platform = "       ";

	if (sb->s_flag & JFS_COMMIT)
		commit = "JFS_COMMIT";
	else
		commit = "          ";

	if (sb->s_flag & JFS_GROUPCOMMIT)
		groupcommit = "JFS_GROUPCOMMIT";
	else
		groupcommit = "               ";

	if (sb->s_flag & JFS_LAZYCOMMIT)
		lazycommit = "JFS_LAZYCOMMIT";
	else
		lazycommit = "              ";

	if (sb->s_flag & JFS_INLINELOG)
		inlinelog = "JFS_INLINELOG";
	else
		inlinelog = "         ";

	if (sb->s_flag & JFS_BAD_SAIT)
		badsait = "JFS_BAD_SAIT";
	else
		badsait = "         ";

	if (sb->s_flag & JFS_SPARSE)
		sparse = "JFS_SPARSE";
	else
		sparse = "         ";

	if (sb->s_flag & JFS_DASD_ENABLED)
		dasdenabled = "DASD_ENABLED";
	else
		dasdenabled = "         ";

	if (sb->s_flag & JFS_DASD_PRIME)
		dasdprime = "DASD_PRIME";
	else
		dasdprime = "         ";

	printf("[1] s_magic:\t\t'%4.4s'\t\t", sb->s_magic);
	printf("[15] s_ait2.addr1:\t0x%02x\t\t\n", sb->s_ait2.addr1);
	printf("[2] s_version:\t\t%d\t\t", sb->s_version);
	printf("[16] s_ait2.addr2:\t0x%08x\n", __cpu_to_le32(sb->s_ait2.addr2));
	printf("[3] s_size:\t0x%016llx\t", (long long) sb->s_size);
	printf("     s_ait2.address:\t%lld\n", (long long) addressPXD(&sb->s_ait2));
	printf("[4] s_bsize:\t\t%d\t\t", sb->s_bsize);
	printf("[17] s_logdev:\t\t0x%08x\n", sb->s_logdev);
	printf("[5] s_l2bsize:\t\t%d\t\t", sb->s_l2bsize);
	printf("[18] s_logserial:\t0x%08x\n", sb->s_logserial);
	printf("[6] s_l2bfactor:\t%d\t\t", sb->s_l2bfactor);
	printf("[19] s_logpxd.len:\t%d\n", __cpu_to_le24(sb->s_logpxd.len));
	printf("[7] s_pbsize:\t\t%d\t\t", sb->s_pbsize);
	printf("[20] s_logpxd.addr1:\t0x%02x\n", sb->s_logpxd.addr1);
	printf("[8] s_l2pbsize:\t\t%d\t\t", sb->s_l2pbsize);
	printf("[21] s_logpxd.addr2:\t0x%08x\n", __cpu_to_le32(sb->s_logpxd.addr2));
	printf("[9] pad:\t\tNot Displayed\t");
	printf("     s_logpxd.address:\t%lld\n", (long long) addressPXD(&sb->s_logpxd));
	printf("[10] s_agsize:\t\t0x%08x\t", sb->s_agsize);
	printf("[22] s_fsckpxd.len:\t%d\n", __cpu_to_le24(sb->s_fsckpxd.len));
	printf("[11] s_flag:\t\t0x%08x\t", sb->s_flag);
	printf("[23] s_fsckpxd.addr1:\t0x%02x\n", sb->s_fsckpxd.addr1);
	printf("\t        %s %s\t", os2_platform, linux_platform);
	printf("[24] s_fsckpxd.addr2:\t0x%08x\n", __cpu_to_le32(sb->s_fsckpxd.addr2));
	printf("\t%s\t%s\t", commit, groupcommit);
	printf("     s_fsckpxd.address:\t%lld\n", (long long) addressPXD(&sb->s_fsckpxd));
	printf("\t%s\t%s\t", lazycommit, inlinelog);
	printf("[25] s_time.tv_sec:\t0x%08x\n", sb->s_time.tv_sec);
	printf("\t%s\t%s\t", badsait, sparse);
	printf("[26] s_time.tv_nsec:\t0x%08x\n", sb->s_time.tv_nsec);
	printf("\t%s\t%s\t", dasdenabled, dasdprime);
	printf("[27] s_fpack:\t\t'%.11s'\n", sb->s_fpack);
	printf("[12] s_state:\t\t0x%08x\t", sb->s_state);
	if (sb->s_version == JFS_VERSION) {
		uuid_unparse(sb->s_uuid, uuid_unparsed);
		printf("[28] s_uuid:\t\t%s", uuid_unparsed);
	}
	printf("\n");
	printf("\t%13s\t\t\t", state);
	if (sb->s_version == JFS_VERSION) {
		printf("[29] s_label:\t\t'%.16s'", sb->s_label);
	}
	printf("\n");
	printf("[13] s_compress:\t%d\t\t", sb->s_compress);
	if (sb->s_version == JFS_VERSION) {
		uuid_unparse(sb->s_loguuid, uuid_unparsed);
		printf("[30] s_loguuid:\t\t%s", uuid_unparsed);
	}
	printf("\n");
	printf("[14] s_ait2.len:\t%d\t\t\n", __cpu_to_le24(sb->s_ait2.len));
	printf("\n");

      retry:
	fputs("display_super: [m]odify or e[x]it: ", stdout);
	fgets(cmdline, 512, stdin);
	token = strtok(cmdline, " 	\n");
	if (token == 0 || token[0] != 'm')
		return rc;

	if (sb->s_version == JFS_VERSION) {
		field = m_parse(cmdline, 30, &token);
	} else {
		field = m_parse(cmdline, 27, &token);
	}
	if (field == 0)
		goto retry;

	switch (field) {
	case 1:
		strncpy(sb->s_magic, token, 4);
		break;
	case 2:
		sb->s_version = strtoul(token, 0, 0);
		break;
	case 3:
		sb->s_size = strtoll(token, 0, 16);
		break;
	case 4:
		sb->s_bsize = strtol(token, 0, 0);
		break;
	case 5:
		sb->s_l2bsize = strtol(token, 0, 0);
		break;
	case 6:
		sb->s_l2bfactor = strtol(token, 0, 0);
		break;
	case 7:
		sb->s_pbsize = strtol(token, 0, 0);
		break;
	case 8:
		sb->s_l2pbsize = strtol(token, 0, 0);
		break;
	case 9:
		fputs("display_super: Can't change this field\n", stderr);
		goto retry;
		break;
	case 10:
		sb->s_agsize = strtoul(token, 0, 16);
		break;
	case 11:
		sb->s_flag = strtoul(token, 0, 16);
		break;
	case 12:
		sb->s_state = strtoul(token, 0, 16);
		break;
	case 13:
		sb->s_compress = strtoul(token, 0, 0);
		break;
	case 14:
		sb->s_ait2.len = __le24_to_cpu(strtoul(token, 0, 0));
		break;
	case 15:
		sb->s_ait2.addr1 = strtoul(token, 0, 16);
		break;
	case 16:
		sb->s_ait2.addr2 = __le32_to_cpu(strtoul(token, 0, 16));
		break;
	case 17:
		sb->s_logdev = strtoul(token, 0, 16);
		break;
	case 18:
		sb->s_logserial = strtol(token, 0, 16);
		break;
	case 19:
		sb->s_logpxd.len = __le24_to_cpu(strtoul(token, 0, 0));
		break;
	case 20:
		sb->s_logpxd.addr1 = strtoul(token, 0, 16);
		break;
	case 21:
		sb->s_logpxd.addr2 = __le32_to_cpu(strtoul(token, 0, 16));
		break;
	case 22:
		sb->s_fsckpxd.len = __le24_to_cpu(strtoul(token, 0, 0));
		break;
	case 23:
		sb->s_fsckpxd.addr1 = strtoul(token, 0, 16);
		break;
	case 24:
		sb->s_fsckpxd.addr2 = __le32_to_cpu(strtoul(token, 0, 16));
		break;
	case 25:
		sb->s_time.tv_sec = strtoul(token, 0, 16);
		break;
	case 26:
		sb->s_time.tv_nsec = strtoul(token, 0, 16);
		break;
	case 27:
		strncpy(sb->s_fpack, token, 11);
		break;
	case 28:
		strncpy(uuid_unparsed, token, 36);
		uuid_rc = uuid_parse(uuid_unparsed, sb->s_uuid);
		if (uuid_rc) {
			fputs
			    ("\ndisplay_super: uuid_parse() FAILED.  uuid entered in improper format.\n\n",
			     stderr);
			goto retry;
		}
		break;
	case 29:
		strncpy(sb->s_label, token, 16);
		break;
	case 30:
		strncpy(uuid_unparsed, token, 36);
		uuid_rc = uuid_parse(uuid_unparsed, sb->s_loguuid);
		if (uuid_rc) {
			fputs
			    ("\ndisplay_super: uuid_parse() FAILED.  uuid entered in improper format.\n\n",
			     stderr);
			goto retry;
		}
		break;
	}
	rc = XPEEK_CHANGED;
	goto changed;
}
