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
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include "jfs_types.h"
#include "jfs_endian.h"
#include "jfs_filsys.h"
#include "jfs_superblock.h"
#include "inode.h"
#include "super.h"
#include "jfs_version.h"
#include "utilsubs.h"

#define EXIT(fd, rc)  {fclose(fd); exit(rc);}

static int J_flag, l_flag, L_flag, U_flag;
char *new_label, *new_UUID;
char *device;
char logdev[255] = { '\0' };
FILE *log_fd = NULL;
int log_desc = -1;
char *OpenMode = "r";

extern int LogOpenMode;
extern FILE * open_by_label(uuid_t, int, int, char *, int *);
extern void display_logsuper(struct logsuper *);
extern void display_super(struct superblock *);

void tune_usage(void)
{
	printf("\nUsage:  jfs_tune [-J options] [-l] [-L vol_label] [-U uuid] [-V] device\n"
	       "\nEmergency help:\n"
	       " -J options    Set external journal options.\n"
	       " -l            Display superblock\n"
	       " -L vol_label  Set volume label.\n"
	       " -U uuid       Set UUID.\n"
	       " -V            Print version information only.\n");
	exit(-1);
}

/*--------------------------------------------------------------------
 * NAME: parse_journal_opts
 *
 * FUNCTION: parse journal (-J) options
 *           set log file descriptor (global log_fd)
 *           set log device name (global logdev)
 *
 * PARAMETERS:
 *      opts - options string
 */
void parse_journal_opts(const char *opts)
{
	int journal_usage = 0;
	uuid_t log_uuid;
	int in_use;

	LogOpenMode = O_RDONLY;

	if (strncmp(opts, "device=", 7) == 0) {
		if (strncmp(opts + 7, "UUID=", 5) == 0) {
			if (uuid_parse((char *) opts + 7 + 5, log_uuid)) {
				fputs("\nError: UUID entered in improper format.\n",
				      stderr);
				exit(-1);
			} else {
				log_fd = open_by_label(log_uuid, 0, 1, logdev,
						       &in_use);
			}
		} else if (strncmp(opts + 7, "LABEL=", 6) == 0) {
			log_fd = open_by_label((char *) opts + 7 + 6, 1, 1,
					       logdev, &in_use);
		} else {
			strcpy(logdev, ((char *) opts + 7));
			if (logdev)
				log_fd = fopen(logdev, "r");
			else
				journal_usage++;
		}
	} else
		journal_usage++;

	if (journal_usage) {
		fprintf(stderr, "\nInvalid journal option(s) specified.\n\n"
				"Valid options for -J are:\n"
				"\tdevice=<journal device>\n"
				"\tdevice=UUID=<UUID of journal device>\n"
				"\tdevice=LABEL=<label of journal device>\n\n");
		exit(1);
	}

	return;
}

/*--------------------------------------------------------------------
 * NAME: parse_tune_options
 *
 * FUNCTION: parse tune options
 *
 * PARAMETERS:
 *      argc - number of passed arguments
 *      argv - string of arguments
 *
 * RETURNS:
 *      success: 0
 *      failure: any other value
 */
static void parse_tune_options(int argc, char *argv[])
{
	int c;

	while ((c = getopt(argc, argv, "J:lL:U:V")) != EOF) {
		switch (c) {
		case 'J':
			/* attach external journal device */
			parse_journal_opts(optarg);
			J_flag = 1;
			OpenMode = "r+";
			break;
		case 'l':
			/* display superblock */
			l_flag = 1;
			break;
		case 'L':
			/* set volume label */
			new_label = optarg;
			L_flag = 1;
			OpenMode = "r+";
			break;
		case 'U':
			/* set UUID */
			new_UUID = optarg;
			U_flag = 1;
			OpenMode = "r+";
			break;
		case 'V':
			/* print version and exit */
			exit(0);
			break;
		default:
			tune_usage();
			break;
		}
	}

	if (optind != argc - 1) {
		printf("\nError: Device not specified or command format error.\n");
		tune_usage();
	}

	if (!J_flag && !l_flag && !L_flag && !U_flag) {
		printf("\nError: No options selected.\n");
		tune_usage();
	}

	device = argv[optind];

	return;
}

/*--------------------------------------------------------------------
 * NAME: main
 *
 * FUNCTION: adjust JFS tunable parameters
 *
 * PARAMETERS:
 *      argc - number of passed arguments
 *      argv - string of arguments
 *
 * RETURNS:
 *      success: 0
 *      failure: any other value
 */
int main(int argc, char *argv[])
{
	FILE *fp = NULL;
	int rc = 0;
	int superblock_type;
	bool mounted = false;
	struct superblock sb;
	struct logsuper logsup;

#define FS_SUPER_SECONDARY  0
#define FS_SUPER_PRIMARY    1
#define LOG_SUPER           2

	printf("jfs_tune version %s, %s\n", VERSION, JFSUTILS_DATE);

	parse_tune_options(argc, argv);

	/*
	 * Check if device is mounted.  -l is the only parameter
	 * supported on a mounted device, so if any others are
	 * selected, let the user know.  If the device is mounted
	 * and -l was not specified, get out.
	 */
	rc = Is_Device_Mounted(device);
	if (rc) {
		mounted = true;
		if (J_flag || L_flag || U_flag) {
			fprintf(stderr, "\n%s is mounted.\n"
				"While mounted, the only supported jfs_tune parameter is -l.\n",
				device);
			if (!l_flag) {
				exit(-1);
			}
		}
	}

	/* Open device */
	if (J_flag)
		fp = fopen_excl(device, "r+");
	else
		fp = fopen(device, OpenMode);
	if (fp == NULL) {
		fprintf(stderr, "Error: Cannot open device %s.\n", device);
		exit(-1);
	}

	/* Get and validate primary JFS superblock */
	if ((rc = ujfs_get_superblk(fp, &sb, 1)) == 0) {
		if ((rc = ujfs_validate_super(&sb)) == 0) {
			superblock_type = FS_SUPER_PRIMARY;
		}
	}

	/* If failure retrieving primary superblock, get/validate secondary superblock */
	if (rc) {
		if ((rc = ujfs_get_superblk(fp, &sb, 0)) == 0) {
			if ((rc = ujfs_validate_super(&sb)) == 0) {
				superblock_type = FS_SUPER_SECONDARY;
			}
		}
	}

	/* If no valid FS superblock, see if we have a log superblock */
	if (rc) {
		if ((rc = ujfs_get_logsuper(fp, &logsup)) == 0) {
			if ((rc = ujfs_validate_logsuper(&logsup)) == 0) {
				superblock_type = LOG_SUPER;
				/*
				 * We know this is an external journal device.
				 * Now check to see if it is attached to a
				 * mounted file system.  If so, the only
				 * valid option is -l.
				 */
				if (logsup.state == LOGMOUNT) {
					mounted = true;
					if (J_flag || L_flag || U_flag) {
						fprintf(stderr,
							"\n%s contains an external journal for a mounted filesystem.\n"
							"While mounted, the only supported jfs_tune parameter is -l.\n",
							device);
						if (!l_flag) {
							EXIT(fp, -1);
						}
					}
				}
			}
		}
	}

	/* If we couldn't find/read a valid JFS FS or log superblock, warn user and exit */
	if (rc) {
		printf("\nCould not read valid JFS FS or log superblock on device %s.\n",
		       device);
		EXIT(fp, -1);
	}

	/*
	 * Account for bug in mkfs.jfs 1.0.18 and 1.0.19
	 * that didn't properly set the file system version
	 * number to 2 when using an external journal.
	 */
	if (!mounted && (superblock_type < LOG_SUPER) &&
	    (sb.s_version < JFS_VERSION) && !(sb.s_flag & JFS_INLINELOG)) {
		sb.s_version = JFS_VERSION;
		rc = ujfs_put_superblk(fp, &sb, superblock_type);
		if (rc) {
			printf("\nCould not update JFS version number properly on %s.\n",
			       device);
			EXIT(fp, rc);
		}
	}

	/*
	 * set volume label on unmounted device
	 */
	if (L_flag && !mounted) {
		if (superblock_type < LOG_SUPER) {
			/* change label in JFS file system superblock */
			/*
			 * The superblock in JFS releases before 1.0.18 stores
			 * the label in s_fpack[11].  The superblock in JFS
			 * releases 1.0.18 and greater has s_fpack, but uses
			 * the new field s_label[16] to store the label.
			 * s_label is in an area of the superblock that was
			 * allocated but unused in pre 1.0.18, so if per chance
			 * the user is using an old JFS file system, setting
			 * s_label will not be a problem.
			 */
			memset(sb.s_fpack, 0, sizeof (sb.s_fpack));
			strncpy(sb.s_fpack, new_label, sizeof (sb.s_fpack));
			if (strlen(new_label) > sizeof (sb.s_label))
				fprintf(stderr, "Warning: label too long, truncating.\n");
			memset(sb.s_label, 0, sizeof (sb.s_label));
			strncpy(sb.s_label, new_label, sizeof (sb.s_label));
			rc = ujfs_put_superblk(fp, &sb, superblock_type);
		} else {
			/* change label in JFS log superblock */
			if (strlen(new_label) > sizeof (logsup.label))
				fprintf(stderr, "Warning: label too long, truncating.\n");
			memset(logsup.label, 0, sizeof (logsup.label));
			strncpy(logsup.label, new_label, sizeof (logsup.label));
			rc = ujfs_put_logsuper(fp, &logsup);
		}
		if (rc) {
			printf("\nError writing superblock to disk.  Label unchanged.\n");
			EXIT(fp, rc);
		} else {
			printf("Volume label updated successfully.\n");
		}
	}

	/*
	 * set UUID on umounted device
	 */
	if (U_flag && !mounted) {
		uuid_t *uu;
		uu = ((superblock_type < LOG_SUPER) ? &sb.s_uuid : &logsup.uuid);
		if ((strcasecmp(new_UUID, "null") == 0) ||
		    (strcasecmp(new_UUID, "clear") == 0)) {
			uuid_clear(*uu);
		} else if (strcasecmp(new_UUID, "time") == 0) {
			uuid_generate_time(*uu);
		} else if (strcasecmp(new_UUID, "random") == 0) {
			uuid_generate(*uu);
		} else if (uuid_parse(new_UUID, *uu)) {
			fprintf(stderr, "Invalid UUID format.\n");
			EXIT(fp, -1);
		}

		if (superblock_type < LOG_SUPER) {
			/* mount(8) won't recognize uuid if jfs_version == 1 */
			if (sb.s_version == 1) {
				sb.s_version = 2;
				/* Make sure s_label is set.  If it's valid,
				 * the first 11 characters will match s_fpack
				 */
				if (strncmp(sb.s_fpack, sb.s_label, 11)) {
					strncpy(sb.s_label, sb.s_fpack, 11);
					sb.s_label[11] = 0;
				}
			}
			rc = ujfs_put_superblk(fp, &sb, superblock_type);
		} else {
			rc = ujfs_put_logsuper(fp, &logsup);
		}

		if (rc) {
			printf("\nError writing superblock to disk.  UUID unchanged.\n");
			EXIT(fp, rc);
		} else {
			printf("UUID updated successfully.\n");
		}
	}

	/*
	 * attach external journal to JFS file system
	 */
	if (J_flag && !mounted) {
		/*
		 * NOTE: If we ever allow attaching more than one file system to a
		 * single log file, we'll have to change the conditions of the above
		 * 'if' to account for a log file that is in use by one file system
		 * (state LOGMOUNT), but is being attached to by another file system.
		 */
		struct stat st;

		/* make sure device to be attached to is a JFS file system */
		if (superblock_type >= LOG_SUPER) {
			printf("\nError: %s does not contain a JFS file system.\n",device);
			EXIT(fp, -1);
		}

		/* log_fd was set in parse_journal_opts */
		if (log_fd == NULL) {
			printf("\nError: Could not find/open specified external journal device.\n");
			EXIT(fp, -1);
		}

		/* get valid log superblock */
		if ((rc = ujfs_get_logsuper(log_fd, &logsup)) == 0) {
			if ((rc = ujfs_validate_logsuper(&logsup)) == 0) {
				if (fstat(fileno(log_fd), &st)) {
					rc = -1;
				} else {
					/* update FS superblock */
					sb.s_logdev = st.st_rdev;
					uuid_copy(sb.s_loguuid, logsup.uuid);
					sb.s_version = JFS_VERSION;
					sb.s_flag &= (~JFS_INLINELOG);
					memset(&sb.s_logpxd, 0, sizeof (pxd_t));
					rc = ujfs_put_superblk(fp, &sb, superblock_type);
				}
			}
		}

		/* If we could't find/read a valid JFS log superblock, let user know */
		if (rc) {
			printf("\nError attaching JFS external journal to JFS FS.\n");
		} else {
			printf("Attached JFS external journal to JFS FS successfully.\n");
		}

		fclose(log_fd);
	}

	/*
	 * display superblock
	 */
	if (l_flag) {
		if (superblock_type < LOG_SUPER) {
			display_super(&sb);
		} else {
			display_logsuper(&logsup);
		}
	}

	fclose(fp);
	return rc;
}
