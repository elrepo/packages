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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_MNTENT_H
#include <mntent.h>
#undef HAVE_GETMNTINFO		/* we don't want both */
#endif

#if HAVE_GETMNTINFO
#include <paths.h>
#include <sys/param.h>
#include <sys/mount.h>
#endif

#if HAVE_SYS_STATVFS_H

#include <sys/statvfs.h>
#define STATFS statvfs
#define NOWAIT ST_NOWAIT

#else

#define STATFS statfs
#define NOWAIT MNT_NOWAIT

#endif

#include "devices.h"
#include "message.h"

/* forward references */
int Is_Root_Mounted_RO(void);

/*--------------------------------------------------------------------
 * NAME: Is_Root_Mounted_RO
 *
 * FUNCTION: Determine if root is mounted read only
 *
 * RETURNS:
 *      0 if root is not mounted READ ONLY
 *    > 0 if root is mounted READ ONLY
 *
 * NOTES: borrowed some of this routine from e2fsck
 */
int Is_Root_Mounted_RO()
{
	int fd, rc = 0;

#define TEST_FILE "/.ismount-test-file"

	fd = open(TEST_FILE, O_RDWR | O_CREAT, S_IRWXU);

	if (fd < 0) {
		if (errno == EROFS)
			rc = MSG_JFS_VOLUME_IS_MOUNTED_RO;
	} else {
		close(fd);
		(void) unlink(TEST_FILE);
	}

	return rc;
}

#if HAVE_MNTENT_H
/*--------------------------------------------------------------------
 * NAME: Is_Device_Mounted
 *
 * FUNCTION: Determine if the device specified is mounted.
 *
 * PRE CONDITIONS: Device_Name must be the name of the device.
 *
 * POST CONDITIONS:
 *
 * PARAMETERS: Device_Name - The name of the device.
 *
 * RETURNS:
 *      0 if the device is not mounted.
 *    > 0 if the device is mounted or an error occurs.
 *
 * NOTES: borrowed some of this routine from e2fsck
 */
int Is_Device_Mounted(char *Device_Name)
{

	FILE *Mount_Records;	/* Pointer for system's mount records */
	struct mntent *Mount_Data;	/* Holds data for entry in mount list */
	int Mounted_RC = 0;	/* Holds return code.             */
	int root_not_jfs = 0;

#define ROOT_DIR "/"

	/* Attempt to open /proc/mounts for access. */
	if ((Mount_Records = setmntent("/proc/mounts", "r")) == NULL) {
		/* Unable to access list of mounted devices in /proc/mounts! */
		/* Attempt to open /etc/mtab for access.                     */
		if ((Mount_Records = setmntent(MOUNTED, "r")) == NULL) {
			/* Unable to access list of mounted devices in /etc/mtab!  */
			return MSG_JFS_MNT_LIST_ERROR;
		}
	}

	/* Attempt to find specified device name in mount records */
	while ((Mount_Data = getmntent(Mount_Records)) != NULL) {
		/*
		 * There may be more than one entry for /.  Trust the
		 * latest one
		 */
		if (strcmp(ROOT_DIR, Mount_Data->mnt_dir) == 0) {
			if (strcmp("jfs", Mount_Data->mnt_type) == 0)
				root_not_jfs = 0;
			else
				root_not_jfs = 1;
		}
		if (strcmp(Device_Name, Mount_Data->mnt_fsname) == 0)
			break;
	}

	if (Mount_Data == 0) {
#ifndef __GNU__			/* The GNU hurd is broken with respect to stat devices */
		struct stat st_root, st_file;
		/*
		   * If the specified device name was not found in the mount records,
		   * do an extra check to see if this is the root device.  We can't
		   * trust /etc/mtab, and /proc/mounts will only list /dev/root for
		   * the root filesystem.  Argh.  Instead we will check if the given
		   * device has the same major/minor number as the device that the
		   * root directory is on.
		 */
		if (stat("/", &st_root) == 0 && stat(Device_Name, &st_file) == 0) {
			if (st_root.st_dev == st_file.st_rdev) {
				if (Is_Root_Mounted_RO())
					Mounted_RC = MSG_JFS_VOLUME_IS_MOUNTED_RO;
				else
					Mounted_RC = MSG_JFS_VOLUME_IS_MOUNTED;
				/*
				 * Make a best effort to ensure that
				 * the root file system type is jfs
				 */
				if (root_not_jfs)
					/* is mounted, found type, is not type jfs */
					Mounted_RC = MSG_JFS_NOT_JFS;
			}
		}
#endif

	} else {
		if (strcmp(Mount_Data->mnt_type, "jfs") == 0) {
			/* is mounted, is type jfs */
			Mounted_RC = MSG_JFS_VOLUME_IS_MOUNTED;

			/* See if we're mounted 'ro' (read only)          */
			/* If we are booting on a jfs device, hasmntopt   */
			/* may return 'rw' even though we're mounted 'ro' */
			/* so let's check for that situation              */
			if (!strcmp(Mount_Data->mnt_dir, "/")) {
				if (Is_Root_Mounted_RO())
					Mounted_RC = MSG_JFS_VOLUME_IS_MOUNTED_RO;
			} else {
				/* Check to see if the 'ro' option is set */
				if (hasmntopt(Mount_Data, MNTOPT_RO))
					Mounted_RC = MSG_JFS_VOLUME_IS_MOUNTED_RO;
			}
		} else {
			/* is mounted, is not type jfs */
			Mounted_RC = MSG_JFS_NOT_JFS;
		}
	}

	/* Close the stream. */
	endmntent(Mount_Records);

	/* Return the appropriate value. */
	return Mounted_RC;
}

/*--------------------------------------------------------------------
 * NAME: Is_Device_Type_JFS
 *
 * FUNCTION: Determine if the device specified is of type JFS in /etc/fstab
 *
 * PRE CONDITIONS: Device_Name must be the name of the device.
 *
 * POST CONDITIONS:
 *
 * PARAMETERS: Device_Name - The name of the device.
 *
 * RETURNS:
 *      0 if the device is type JFS in /etc/fstab
 *    > 0 if the device is not type JFS or an error occurs.
 *
 */
int Is_Device_Type_JFS(char *Device_Name)
{

	FILE *FS_Info_File;	/* Pointer for system's mount records */
	struct mntent *FS_Data;	/* Holds data for entry in mount list */
	int Is_JFS_RC = 0;	/* Holds return code.             */

	/* Attempt to open /etc/fstab for access. */
	if ((FS_Info_File = setmntent(_PATH_MNTTAB, "r")) == NULL) {
		/* Unable to access mount description file /etc/fstab!  */
		return MSG_JFS_MNT_LIST_ERROR;
	}

	/* Attempt to find specified device name in filesystem records */
	while ((FS_Data = getmntent(FS_Info_File)) != NULL)
		if (strcmp(Device_Name, FS_Data->mnt_fsname) == 0)
			break;

	if (FS_Data) {
		if (strcmp(FS_Data->mnt_type, "jfs") != 0)
			/* is mounted, is type jfs */
			Is_JFS_RC = MSG_JFS_NOT_JFS;
	} else {
		Is_JFS_RC = MSG_JFS_DEV_NOT_IN_TABLE;
	}

	/* Close the stream. */
	endmntent(FS_Info_File);

	/* Return the appropriate value. */
	return Is_JFS_RC;
}
#endif				/* HAVE_MNTENT_H */

#if HAVE_GETMNTINFO
/*--------------------------------------------------------------------
 * NAME: Is_Device_Mounted
 *
 * FUNCTION: Determine if the device specified is mounted.
 *
 * PRE CONDITIONS: Device_Name must be the name of the device.
 *
 * POST CONDITIONS:
 *
 * PARAMETERS: Device_Name - The name of the device.
 *
 * RETURNS:
 *      0 if the device is not mounted.
 *    > 0 if the device is mounted or an error occurs.
 *
 * NOTES: borrowed some of this routine from e2fsck
 */
int Is_Device_Mounted(char *Device_Name)
{
	struct STATFS *mp;
	int len, n;
	char *s1, *s2;

	n = getmntinfo(&mp, NOWAIT);
	if (!n)
		return MSG_JFS_MNT_LIST_ERROR;

	len = sizeof (_PATH_DEV) - 1;
	s1 = Device_Name;
	if (strncmp(_PATH_DEV, s1, len) == 0)
		s1 += len;

	while (--n >= 0) {
		s2 = mp->f_mntfromname;
		if (strncmp(_PATH_DEV, s2, len) == 0) {
			s2 += len - 1;
			*s2 = 'r';
		}
		if (strcmp(s1, s2) == 0 || strcmp(s1, &s2[1]) == 0) {
			if (strcmp(mp->f_fstypename, "jfs") == 0) {
				/* is mounted, is type jfs */
				/* XXX check for read-only somehow */
				return MSG_JFS_VOLUME_IS_MOUNTED;
			} else {
				/* is mounted, is not type jfs */
				return MSG_JFS_NOT_JFS;
			}
		}
		++mp;
	}

	return 0;
}

/*--------------------------------------------------------------------
 * NAME: Is_Device_Type_JFS
 *
 * FUNCTION: Determine if the device specified is of type JFS in /etc/fstab
 *
 * PRE CONDITIONS: Device_Name must be the name of the device.
 *
 * POST CONDITIONS:
 *
 * PARAMETERS: Device_Name - The name of the device.
 *
 * RETURNS:
 *      0 if the device is type JFS in /etc/fstab
 *    > 0 if the device is not type JFS or an error occurs.
 *
 */
int Is_Device_Type_JFS(char *Device_Name)
{
	struct STATFS *mp;
	int len, n;
	char *s1, *s2;

	n = getmntinfo(&mp, NOWAIT);
	if (!n)
		return MSG_JFS_MNT_LIST_ERROR;

	len = sizeof (_PATH_DEV) - 1;
	s1 = Device_Name;
	if (strncmp(_PATH_DEV, s1, len) == 0)
		s1 += len;

	while (--n >= 0) {
		s2 = mp->f_mntfromname;
		if (strncmp(_PATH_DEV, s2, len) == 0) {
			s2 += len - 1;
			*s2 = 'r';
		}
		if (strcmp(s1, s2) == 0 || strcmp(s1, &s2[1]) == 0) {
			if (strcmp(mp->f_fstypename, "jfs") == 0) {
				/* is mounted, is type jfs */
				/* XXX check for read-only somehow */
				return 0;
			} else {
				/* is mounted, is not type jfs */
				return MSG_JFS_NOT_JFS;
			}
		}
		++mp;
	}

	return MSG_JFS_DEV_NOT_IN_TABLE;
}
#endif				/* HAVE_GETMNTINFO */
