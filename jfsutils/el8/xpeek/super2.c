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
#include "jfs_byteorder.h"
#include "jfs_filsys.h"
#include "super.h"

/* Prototypes for local functions. */
int display_super2(struct superblock *sb);

void superblock2(void)
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
	if (display_super2(&super) == XPEEK_CHANGED)
		if (ujfs_put_superblk(fp, &super, (short) is_primary))
			fputs("superblock: error returned from ujfs_put_superblk\n\n", stderr);
	return;
}

/*****************************************************************************
 ****************  Sample output of display_super2():

[1] s_magic:		'JFS1'		[16] s_aim2.len:	2
[2] s_version:		2		[17] s_aim2.addr1:	0x00
[3] s_size:	0x00000000001f5e68	[18] s_aim2.addr2:	0x00000035
[4] s_bsize:		4096		     s_aim2.address:	53
[5] s_l2bsize:		12		[19] s_logdev:		0x00003f06
[6] s_l2bfactor:	3		[20] s_logserial:	0x00000000
[7] s_pbsize:		512		[21] s_logpxd.len:	0
[8] s_l2pbsize:		9		[22] s_logpxd.addr1:	0x00
[9]  s_agsize:		0x00002000	[23] s_logpxd.addr2:	0x00000000
[10] s_flag:		0x10200100	     s_logpxd.address:	0
			 LINUX          [24] s_fsckpxd.len:	59
	GROUPCOMMIT                 	[25] s_fsckpxd.addr1:	0x00
					[26] s_fsckpxd.addr2:	0x0003ebcd
					     s_fsckpxd.address:	256973
[11] s_state:		0x00000000	[27] s_fsckloglen:	50
			CLEAN		[28] s_fscklog:		2
[12] s_compress:	0		[29] s_fpack:		'  bubba1'
[13] s_ait2.len:	4		[30] s_uuid:		87654321-4321-4321-4321-cba987654321
[14] s_ait2.addr1:	0x00		[31] s_label:		'          bubba4'
[15] s_ait2.addr2:	0x00000037	[32] s_loguuid:		12345678-1234-1234-1234-123456789abc
     s_ait2.address:	55

******************************************************************************/

int display_super2(struct superblock *sb)
{
	char *os2_platform;
	char *linux_platform;
	char cmdline[512];
	int field;
	int rc = XPEEK_OK;
	char *state;
	char *token;
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
		state = "CLEAN";
		break;
	case FM_MOUNT:
		state = "MOUNT";
		break;
	case FM_DIRTY:
		state = "DIRTY";
		break;
	case FM_LOGREDO:
		state = "LOGREDO";
		break;
	default:
		state = "Unknown State";
		break;
	}
	if (sb->s_flag & JFS_LINUX)
		linux_platform = "LINUX";
	else
		linux_platform = "     ";
	if (sb->s_flag & JFS_OS2)
		os2_platform = "OS2";
	else
		os2_platform = "   ";
	if (sb->s_flag & JFS_GROUPCOMMIT)
		groupcommit = "GROUPCOMMIT";
	else
		groupcommit = "           ";
	if (sb->s_flag & JFS_LAZYCOMMIT)
		lazycommit = "LAZYCOMMIT";
	else
		lazycommit = "          ";
	if (sb->s_flag & JFS_INLINELOG)
		inlinelog = "INLINELOG";
	else
		inlinelog = "         ";
	if (sb->s_flag & JFS_BAD_SAIT)
		badsait = "BAD_SAIT";
	else
		badsait = "        ";
	if (sb->s_flag & JFS_SPARSE)
		sparse = "SPARSE";
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
	printf("[16] s_aim2.len:\t%d\n", __cpu_to_le24(sb->s_aim2.len));
	printf("[2] s_version:\t\t%d\t\t", sb->s_version);
	printf("[17] s_aim2.addr1:\t0x%02x\n", sb->s_aim2.addr1);
	printf("[3] s_size:\t0x%016llx\t", (long long) sb->s_size);
	printf("[18] s_aim2.addr2:\t0x%08x\n", __cpu_to_le32(sb->s_aim2.addr2));
	printf("[4] s_bsize:\t\t%d\t\t", sb->s_bsize);
	printf("     s_aim2.address:\t%lld\n", (long long) addressPXD(&sb->s_aim2));
	printf("[5] s_l2bsize:\t\t%d\t\t", sb->s_l2bsize);
	printf("[19] s_logdev:\t\t0x%08x\n", sb->s_logdev);
	printf("[6] s_l2bfactor:\t%d\t\t", sb->s_l2bfactor);
	printf("[20] s_logserial:\t0x%08x\n", sb->s_logserial);
	printf("[7] s_pbsize:\t\t%d\t\t", sb->s_pbsize);
	printf("[21] s_logpxd.len:\t%d\n", __cpu_to_le24(sb->s_logpxd.len));
	printf("[8] s_l2pbsize:\t\t%d\t\t", sb->s_l2pbsize);
	printf("[22] s_logpxd.addr1:\t0x%02x\n", sb->s_logpxd.addr1);
	printf("[9]  s_agsize:\t\t0x%08x\t", sb->s_agsize);
	printf("[23] s_logpxd.addr2:\t0x%08x\n", __cpu_to_le32(sb->s_logpxd.addr2));
	printf("[10] s_flag:\t\t0x%08x\t", sb->s_flag);
	printf("     s_logpxd.address:\t%lld\n", (long long) addressPXD(&sb->s_logpxd));
	printf("         %s %s                    \t", os2_platform, linux_platform);
	printf("[24] s_fsckpxd.len:\t%d\n", __cpu_to_le24(sb->s_fsckpxd.len));
	printf("    %s   %s   %s\t", groupcommit, lazycommit, badsait);
	printf("[25] s_fsckpxd.addr1:\t0x%02x\n", sb->s_fsckpxd.addr1);
	printf("    %s   %s             \t", sparse, inlinelog);
	printf("[26] s_fsckpxd.addr2:\t0x%08x\n", __cpu_to_le32(sb->s_fsckpxd.addr2));
	printf("    %s   %s\t\t", dasdenabled, dasdprime);
	printf("     s_fsckpxd.address:\t%lld\n", (long long) addressPXD(&sb->s_fsckpxd));
	printf("[11] s_state:\t\t0x%08x\t", sb->s_state);
	printf("[27] s_fsckloglen:\t%d\t\n", sb->s_fsckloglen);
	printf("\t%13s\t\t\t", state);
	printf("[28] s_fscklog:\t\t%d\t\n", sb->s_fscklog);
	printf("[12] s_compress:\t%d\t\t", sb->s_compress);
	printf("[29] s_fpack:\t\t'%8s'\n", sb->s_fpack);
	printf("[13] s_ait2.len:\t%d\t\t", __cpu_to_le24(sb->s_ait2.len));
	if (sb->s_version == JFS_VERSION) {
		uuid_unparse(sb->s_uuid, uuid_unparsed);
		printf("[30] s_uuid:\t\t%s", uuid_unparsed);
	}
	printf("\n");
	printf("[14] s_ait2.addr1:\t0x%02x\t\t", sb->s_ait2.addr1);
	if (sb->s_version == JFS_VERSION) {
		printf("[31] s_label:\t\t'%16.16s'", sb->s_label);
	}
	printf("\n");
	printf("[15] s_ait2.addr2:\t0x%08x\t", __cpu_to_le32(sb->s_ait2.addr2));
	if (sb->s_version == JFS_VERSION) {
		uuid_unparse(sb->s_loguuid, uuid_unparsed);
		printf("[32] s_loguuid:\t\t%s", uuid_unparsed);
	}
	printf("\n");
	printf("     s_ait2.address:\t%lld\t\t", (long long) addressPXD(&sb->s_ait2));
	printf("\n");

      retry:
	fputs("display_super2: [m]odify or e[x]it: ", stdout);
	fgets(cmdline, 512, stdin);
	token = strtok(cmdline, " 	\n");
	if (token == 0 || token[0] != 'm')
		return rc;

	if (sb->s_version == JFS_VERSION) {
		field = m_parse(cmdline, 32, &token);
	} else {
		field = m_parse(cmdline, 29, &token);
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
		sb->s_agsize = strtoul(token, 0, 16);
		break;
	case 10:
		sb->s_flag = strtoul(token, 0, 16);
		break;
	case 11:
		sb->s_state = strtoul(token, 0, 16);
		break;
	case 12:
		sb->s_compress = strtoul(token, 0, 0);
		break;
	case 13:
		sb->s_ait2.len = __le24_to_cpu(strtoul(token, 0, 0));
		break;
	case 14:
		sb->s_ait2.addr1 = strtoul(token, 0, 16);
		break;
	case 15:
		sb->s_ait2.addr2 = __le32_to_cpu(strtoul(token, 0, 16));
		break;
	case 16:
		sb->s_aim2.len = __le24_to_cpu(strtoul(token, 0, 0));
		break;
	case 17:
		sb->s_aim2.addr1 = strtoul(token, 0, 16);
		break;
	case 18:
		sb->s_aim2.addr2 = __le32_to_cpu(strtoul(token, 0, 16));
		break;
	case 19:
		sb->s_logdev = strtoul(token, 0, 16);
		break;
	case 20:
		sb->s_logserial = strtol(token, 0, 16);
		break;
	case 21:
		sb->s_logpxd.len = __le24_to_cpu(strtoul(token, 0, 0));
		break;
	case 22:
		sb->s_logpxd.addr1 = strtoul(token, 0, 16);
		break;
	case 23:
		sb->s_logpxd.addr2 = __le32_to_cpu(strtoul(token, 0, 16));
		break;
	case 24:
		sb->s_fsckpxd.len = __le24_to_cpu(strtoul(token, 0, 0));
		break;
	case 25:
		sb->s_fsckpxd.addr1 = strtoul(token, 0, 16);
		break;
	case 26:
		sb->s_fsckpxd.addr2 = __le32_to_cpu(strtoul(token, 0, 16));
		break;
	case 27:
		sb->s_fsckloglen = strtol(token, 0, 0);
		break;
	case 28:
		sb->s_fscklog = strtol(token, 0, 0);
		break;
	case 29:
		strncpy(sb->s_fpack, token, 8);
		break;
	case 30:
		strncpy(uuid_unparsed, token, 36);
		uuid_rc = uuid_parse(uuid_unparsed, sb->s_uuid);
		if (uuid_rc) {
			fputs
			    ("\ndisplay_super2: uuid_parse() FAILED.  uuid entered in improper format.\n\n",
			     stderr);
			goto retry;
		}
		break;
	case 31:
		strncpy(sb->s_label, token, 16);
		break;
	case 32:
		strncpy(uuid_unparsed, token, 36);
		uuid_rc = uuid_parse(uuid_unparsed, sb->s_loguuid);
		if (uuid_rc) {
			fputs
			    ("\ndisplay_super2: uuid_parse() FAILED.  uuid entered in improper format.\n\n",
			     stderr);
			goto retry;
		}
		break;
	default:
		fputs("display_super2: Field number out of range\n", stderr);
		goto retry;
	}
	rc = XPEEK_CHANGED;
	goto changed;
}
