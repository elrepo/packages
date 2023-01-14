/*
 *   Copyright (c) International Business Machines Corp., 2000-2007
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
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "jfs_types.h"
#include "jfs_endian.h"
#include "jfs_filsys.h"
#include "jfs_superblock.h"
#include "jfs_logmgr.h"
#include "devices.h"
#include "utilsubs.h"

/*
 * LogOpenMode is kind of clunky, but it avoids passing a flag through several
 * layers of functions.  It should be either O_RDWR|O_EXCL or O_RDONLY.
 * Nothing should be writing to the journal device if any mounted file system
 * is using it.
 */
int LogOpenMode = O_RDWR|O_EXCL;

FILE * open_check_label(char *device, uuid_t uuid, int is_label, int is_log,
		     int *in_use)
{
	FILE *fp = NULL;
	struct logsuper logsup;
	struct superblock super;

	if (in_use)
		*in_use = 0;

	if (LogOpenMode == O_RDONLY)
		fp = fopen(device, "r");
	else {
		fp = fopen_excl(device, "r+");

		if ((fp == NULL) && in_use) {
			*in_use = 1;
			fp = fopen(device, "r");
		}
	}
	if (fp == NULL)
		return fp;

	if (is_log) {
		ujfs_rw_diskblocks(fp, LOGSUPER_B << L2LOGPSIZE,
				   sizeof (struct logsuper), &logsup, GET);
		ujfs_swap_logsuper(&logsup);
		if (logsup.magic == LOGMAGIC) {
			if (is_label) {
				if (!strncmp(uuid, logsup.label, 16))
					return fp;
			} else {
				if (!uuid_compare(uuid, logsup.uuid))
					return fp;
			}
		}
	} else {
		ujfs_rw_diskblocks(fp, SUPER1_OFF, sizeof (super), &super, GET);
		ujfs_swap_superblock(&super);
		if (!memcmp(super.s_magic, JFS_MAGIC, 4)) {
			if (is_label) {
				if (!strncmp(uuid, super.s_label, 16))
					return fp;
			} else {
				if (!uuid_compare(uuid, super.s_uuid))
					return fp;
			}
		}
	}
	fclose(fp);
	return (NULL);
}

/*--------------------------------------------------------------------
 * NAME: walk_dir
 *
 * FUNCTION: Helper for open_by_label to recursively search a directory
 *	     for block devices with the specified label or uuid
 *
 * PARAMETERS:	path - path of directory to search, returns found device
 *		uuid - label or uuid
 * 		is_label - 1 if label, 0 if uuid
 * 		is_log - 0 if filesystem, 1 if external log
 *
 * RETURNS:
 *    >= 0 file descriptor of matching volume
 *    < 0  matching volume not found
 */
FILE * walk_dir(char *path, uuid_t uuid, int is_label, int is_log, int *in_use)
{
	FILE *fp = NULL;
	DIR *lv_dir;
	struct dirent *lv_ent;
	int path_len;
	struct stat st;

	lv_dir = opendir(path);
	if (lv_dir) {
		strcat(path, "/");
		path_len = strlen(path);
		while ((lv_ent = readdir(lv_dir))) {
			path[path_len] = 0;
			if (!strcmp(lv_ent->d_name, ".") ||
			    !strcmp(lv_ent->d_name, "..") ||
			    !strcmp(lv_ent->d_name, ".nodes"))
				continue;
			strcat(path, lv_ent->d_name);
			if (stat(path, &st))
				continue;
			if (S_ISBLK(st.st_mode))
				fp = open_check_label(path, uuid, is_label,
						      is_log, in_use);
			else if (S_ISDIR(st.st_mode))
				fp = walk_dir(path, uuid, is_label, is_log,
					      in_use);
			else
				continue;
			if (fp) {
				closedir(lv_dir);
				return fp;
			}
		}
		closedir(lv_dir);
	}
	return NULL;
}

/*--------------------------------------------------------------------
 * NAME: open_by_label
 *
 * FUNCTION: Search /proc/partitions for volume having specified
 *	     label or uuid
 *
 * PARAMETERS:	uuid - label or uuid
 * 		is_label - 1 if label, 0 if uuid
 * 		is_log - 0 if filesystem, 1 if external log
 *		dev - if not null, returns copy of device path
 *
 * RETURNS:
 *    >= 0 file descriptor of matching volume
 *    < 0  matching volume not found
 *
 * NOTE: We may want to cache the uuids already found
 */
FILE *open_by_label(uuid_t uuid, int is_label, int is_log, char *dev, int *in_use)
{
	char device[100];
	FILE *fp;
	char line[100];
	DIR *lv_dir;
	char lv_dirname[100];
	struct dirent *lv_ent;
	int size;
	FILE *part_fd;
	char part_name[95];
	DIR *vg_dir;
	struct dirent *vg_ent;

// #ifndef __DragonFly__
	/*
	 * Check for EVMS Release 1 volumes
	 */
	part_fd = fopen("/proc/evms/volumes", "r");
	if (part_fd) {
		/* evms/volumes should be complete.  If it exists, don't
		 * search /proc/partitions or /proc/lvm
		 */
		while (fgets(line, sizeof (line), part_fd)) {
			if (sscanf(line, " %*d %*d %*d %*s %*s %s", device)
			    != 1)
				continue;
			fp = open_check_label(device, uuid, is_label, is_log,
					      in_use);
			if (fp != NULL) {
				if (dev)
					strcpy(dev, device);
				fclose(part_fd);
				return fp;
			}
		}
		fclose(part_fd);

		printf("Could not locate device by label or uuid!\n");

		return (NULL);
	}

	/*
	 * Check for evms release 2 volumes
	 */
	strcpy(device, "/dev/evms");
	fp = walk_dir(device, uuid, is_label, is_log, in_use);
	if (fp != NULL) {
		if (dev)
			strcpy(dev, device);
		return fp;
	}

	/*
	 * For RAID, check /proc/mdstat before /proc/partitions.
	 * We don't want to find a device in /proc/partitions that is a
	 * subset of a journal on a raid device
	 */
	part_fd = fopen("/proc/mdstat", "r");
	if (part_fd) {
		while (fgets(line, sizeof(line), part_fd)) {
			char tmp[4];

			/* reading tmp requires matching "active" */
			if (sscanf(line, "%s : active %1s", part_name, tmp) != 2)
				continue;

			sprintf(device, "/dev/%s", part_name);
			fp = open_check_label(device, uuid, is_label, is_log,
					      in_use);
			if (fp != NULL) {
				if (dev)
					strcpy(dev, device);
				fclose(part_fd);
				return fp;
			}
		}
		fclose(part_fd);
	}

	/* Nothing yet.  Check /proc/partitions */

	part_fd = fopen("/proc/partitions", "r");
	if (part_fd) {
		while (fgets(line, sizeof (line), part_fd)) {
			if (sscanf(line, " %*d %*d %d %s", &size, part_name) != 2)
				continue;
			if (size == 1)	/* extended partition */
				continue;
			sprintf(device, "/dev/%s", part_name);
			fp = open_check_label(device, uuid, is_label, is_log,
					      in_use);
			if (fp != NULL) {
				if (dev)
					strcpy(dev, device);
				fclose(part_fd);
				return fp;
			}
		}
		fclose(part_fd);
	} else
		printf("Could not open /proc/partitions!\n");

	/* Not found yet.  Check for lvm volumes */
	vg_dir = opendir("/proc/lvm/VGs");
	if (vg_dir) {
		seekdir(vg_dir, 2);
		while ((vg_ent = readdir(vg_dir))) {
			sprintf(lv_dirname, "/proc/lvm/VGs/%s/LVs", vg_ent->d_name);
			lv_dir = opendir(lv_dirname);
			if (lv_dir == NULL) {
				printf("can't open %s\n", lv_dirname);
				continue;
			}
			seekdir(lv_dir, 2);
			while ((lv_ent = readdir(lv_dir))) {
				sprintf(device, "/dev/%s/%s", vg_ent->d_name, lv_ent->d_name);
				fp = open_check_label(device, uuid, is_label,
						      is_log, in_use);
				if (fp != NULL) {
					if (dev)
						strcpy(dev, device);
					closedir(lv_dir);
					closedir(vg_dir);
					return fp;
				}
			}
			closedir(lv_dir);
		}
		closedir(vg_dir);
	}
	printf("Could not locate device by label or uuid!\n");
/* #endif */
	return (NULL);
}
