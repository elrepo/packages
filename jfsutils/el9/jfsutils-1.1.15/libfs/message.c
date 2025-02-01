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
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "message.h"
#include "debug.h"

#define MAX_STRING_ACCEPTED 80
char Current_String[MAX_STRING_ACCEPTED + 1];

/*  *** message_user -
 *
 *  This function is
 *
 *  message_user (mesg_no, param, param_cnt, msg_file)
 *
 *  ENTRY    mesg_no   - index into the array of messages
 *           param     - list of pointers to parameters in the message
 *           param_cnt - count of parameters in the param variable
 *           msg_file  - OSO_MSG/JFS_MSG which message file to use
 *
 *  EXIT     Returns pointer to buffer contain the user's response
 *           or NULL if no response needed.
 *
 *  CALLS
 */

void message_user(unsigned mesg_no, char **param, unsigned param_cnt, int msg_file)
{
	if (msg_file == OSO_MSG) {
		switch (mesg_no) {
		case MSG_OSO_NOT_ENOUGH_MEMORY:
			printf("There is not enough memory available to process this command.\n");
			break;
		case MSG_OSO_CANT_FIND_DRIVE:
			printf("\nThe system cannot find the specified device.\n");
			break;
		case MSG_OSO_CANT_OPEN:
			printf("The system cannot open the device or file specified.\n");
			break;
		case MSG_OSO_ERR_ACCESSING_DISK:
			printf(" An error occurred when accessing the hard disk.\n");
			break;
		case MSG_OSO_DISK_LOCKED:
			printf(" The specified disk is being used by another process.\n");
			break;
		case MSG_OSO_VALUE_NOT_ALLOWED:
			printf("The value %s is not allowed for parameter %s.\n", param[0],
			       param[1]);
			break;
		case MSG_OSO_FORMAT_FAILED:
			printf("The specified disk did not finish formatting.\n");
			break;
		case MSG_OSO_INSUFF_MEMORY:
			printf("Not enough memory is available to run mkfs.jfs.\n");
			break;
		case MSG_OSO_DESTROY_DATA:
			printf("Warning!  All data on device %s will be lost!\n", param[0]);
			break;
		case MSG_OSO_FORMAT_COMPLETE:
			printf("\n\nFormat completed successfully.\n\n");
			break;
		case MSG_OSO_DISK_SPACE:
			printf("\n%s kilobytes of total disk space.\n", param[0]);
			break;
		case MSG_OSO_DISK_SPACE2:
			printf("%s kilobytes total disk space.\n", param[0]);
			break;
		case MSG_OSO_FREE_SPACE:
			printf("%s kilobytes are available.\n", param[0]);
			break;
		case MSG_OSO_PERCENT_FORMAT:
			printf("%s percent of the disk has been formatted.\r", param[0]);
			break;
		case MSG_OSO_READ_ERROR:
			printf("An error occurred when reading data from the hard disk.\n");
			break;
		case MSG_OSO_NOT_VALID_BLOCK_DEVICE:
			printf("%s is not a valid block device.\n", param[0]);
			break;
		default:	/* Error!  Unknown message number! */
			break;
		}

	} else {

		switch (mesg_no) {
		case MSG_JFS_PART_SMALL:
			printf("Partition must be at least %s megabytes.\n", param[0]);
			break;
		case MSG_JFS_LOG_LARGE:
			printf("Log too large, no space for file system.\n");
			break;
		case MSG_JFS_BAD_PART:
			/* This message is basically from ext2 */
			printf("Device size reported to be zero.  Invalid partition specified, or\n"
			       "\tpartition table wasn't reread after running fsck, fdisk, etc.\n"
			       "\tYou may need to reboot to re-read your partition table.\n");
			break;
		case 52:
			printf("Invalid data format detected in root directory.\n");
			break;
		case 68:
			printf
			    ("Superblock is corrupt and cannot be repaired because \nboth primary and secondary copies are corrupt.\n\nFSCK CANNOT CONTINUE.\n\n");
			break;
		case 69:
			printf
			    ("Primary superblock is corrupt and cannot be repaired without write access.  FSCK continuing.\n");
			break;
		case 76:
			printf("Invalid format detected in root directory.\n");
			break;
		case 79:
			printf
			    ("Secondary file/directory allocation structure (%s) is not a correct redundant copy of primary structure.\n",
			     param[0]);
			break;
		case 80:
			printf
			    ("Unable to replicate primary file/directory allocation structure (%s) \nto secondary.  FUTURE RECOVERY CAPABILITY MAY BE COMPROMISED.\n",
			     param[0]);
			break;
		case 82:
			printf
			    ("Cannot create directory lost+found in root directory.  \nMKDIR lost+found in the root directory then start FSCK with the -f parameter \nto reconnect lost files and/or directories.\n");
			break;
		case 83:
			printf("Fileset object %s%s%s:  No paths found.\n", param[0], param[1],
			       param[2]);
			break;
		case 84:
			printf("The paths refer to an unallocated file.\n");
			break;
		case 86:
			printf("The paths refer to an unallocated file. Will remove.\n");
			break;
		case 94:
			printf("Unable to write primary superblock.\n");
			break;
		case 96:
			printf("Multiple parent directories for directory %s%s.\n", param[0],
			       param[1]);
			break;
		case 101:
			printf
			    ("Insufficient dynamic storage available for required workspace\n(%s,%s). FSCK CANNOT CONTINUE.\n",
			     param[0], param[1]);
			break;
		case 103:
			printf("File system is currently mounted.\n");
			break;
		case 105:
			printf("Block size in bytes:  %s\n", param[0]);
			break;
		case 106:
			printf("File system size in blocks:  %s\n", param[0]);
			break;
		case 117:
			printf
			    ("Unable to get path for link from directory %s%s to fileset object %s%s%s.\n",
			     param[0], param[1], param[2], param[3], param[4]);
			break;
		case 118:
			printf("Format error in Extended Attributes Space or descriptor.\n");
			break;
		case 121:
			printf
			    ("Directory %s%s entry '..' refers to an incorrect parent directory (%s%s).\n",
			     param[0], param[1], param[2], param[3]);
			break;
		case 129:
			printf("File system object %s%s%s is linked as: %s\n", param[0], param[1],
			       param[2], param[3]);
			break;
		case 134:
			printf
			    ("Insufficient dynamic storage to validate extended attributes format.\n");
			break;
		case 135:
			printf("logredo failed (rc=%s).  FSCK continuing.\n", param[0]);
			break;
		case 137:
			printf("Unable to create a lost+found directory in root directory.\n");
			break;
		case 138:
			printf
			    ("Checking a mounted file system does not produce dependable results.\n");
			break;
		case 142:
			printf("%s blocks are missing.\n", param[0]);
			break;
		case 143:
			printf("Unable to write to boot sector.  FSCK continuing.\n");
			break;
		case 145:
			printf("Incorrect link counts detected in the file system.\n");
			break;
		case 148:
			printf("Unrecoverable error reading %s from %s.  FSCK CANNOT CONTINUE.\n",
			       param[0], param[1]);
			break;
		case 149:
			printf("Phase 0 - Replay Journal Log\n");
			break;
		case 150:
			printf
			    ("Phase 1 - Check Blocks, Files/Directories, and Directory Entries.\n");
			break;
		case 151:
			printf("Phase 2 - Count Links.\n");
			break;
		case 152:
			printf
			    ("Phase 3 - Rescan for Duplicate Blocks and Verify Directory Tree.\n");
			break;
		case 153:
			printf("Phase 4 - Report Problems.\n");
			break;
		case 154:
			printf("Phase 5 - Check Connectivity.\n");
			break;
		case 155:
			printf("Phase 6 - Perform Approved Corrections.\n");
			break;
		case 156:
			printf("Phase 7 - Rebuild File/Directory Allocation Maps.\n");
			break;
		case 157:
			printf("Phase 8 - Rebuild Disk Allocation Maps.\n");
			break;
		case 158:
			printf("Phase 9 - Reformat File System Log.\n");
			break;
		case 159:
			printf("Directory has entry for unallocated file %s%s. Will remove.\n",
			       param[0], param[1]);
			break;
		case 161:
			printf
			    ("Format error in Extended Attributes Space or descriptor.  Will clear.\n");
			break;
		case 165:
			printf
			    ("Mutually exclusive 'check READ ONLY' and 'fix file system' options specified.\n");
			break;
		case 166:
			printf("Usage:  fsck.jfs [-a] [-f] [-n] [-o] [-p] [-v] [-V] <device>\n");
			break;
		case 167:
			printf("Unrecognized -f parameter value detected:   %s\n", param[0]);
			break;
		case 168:
			printf("Unsupported parameter:   %s\n", param[0]);
			break;
		case 169:
			printf("logformat failed (rc=%s).  FSCK continuing.\n", param[0]);
			break;
		case 171:
			printf
			    ("Unable to read device characteristics.  Boot sector cannot be refreshed.  FSCK continuing.\n");
			break;
		case 174:
			printf
			    ("Cannot repair an allocation error for files and/or directories %s through %s.\n",
			     param[0], param[1]);
			break;
		case 186:
			printf
			    ("Cannot recover files and/or directories %s through %s. FSCK CANNOT CONTINUE.\n",
			     param[0], param[1]);
			break;
		case 187:
			printf("Unrecoverable error writing %s to %s.  FSCK CANNOT CONTINUE.\n",
			       param[0], param[1]);
			break;
		case 188:
			printf("The root directory has an invalid data format.  Will correct.\n");
			break;
		case 189:
			printf("The root directory has an invalid format.  Will correct.\n");
			break;
		case 190:
			printf
			    ("Cannot recover files and/or directories %s through %s.  Will release.\n",
			     param[0], param[1]);
			break;
		case 191:
			printf("File claims cross linked blocks.\n");
			break;
		case 192:
			printf("Cannot repair the data format error(s) in this file.\n");
			break;
		case 193:
			printf("Cannot repair the format error(s) in this file.\n");
			break;
		case 194:
			printf("Cannot repair %s%s%s.\n", param[0], param[1], param[2]);
			break;
		case 195:
			printf("The current device is:  %s\n", param[0]);
			break;
		case 197:
			printf("Cannot repair %s%s%s.  Will release.\n", param[0], param[1],
			       param[2]);
			break;
		case 200:
			printf("Multiple parent directories for directory %s%s.  Will correct.\n",
			       param[0], param[1]);
			break;
		case 201:
			printf
			    ("Directory %s%s entry '..' refers to an incorrect parent directory\n(%s%s). Will correct.\n",
			     param[0], param[1], param[2], param[3]);
			break;
		case 202:
			printf("%s unexpected blocks detected.\n", param[0]);
			break;
		case 203:
			printf("Directories with illegal hard links have been detected.\n");
			break;
		case 204:
			printf
			    ("Directory entries ('..') referring to incorrect parent directories have been detected.\n");
			break;
		case 205:
			printf
			    ("Directory entries for unallocated files and/or directories were detected.\n");
			break;
		case 206:
			printf("Unable to write secondary superblock.\n");
			break;
		case 207:
			printf("Incorrect link counts have been detected. Will correct.\n");
			break;
		case 209:
			printf
			    ("Duplicate block references have been detected in meta-data. \nFSCK CANNOT CONTINUE.\n");
			break;
		case 214:
			printf("Unrecoverable error during UNLOCK processing.\n");
			break;
		case 215:
			printf("Unrecoverable error during CLOSE processing!\n");
			break;
		case 217:
			printf("%s appears to be the correct path for directory %s%s.\n", param[0],
			       param[1], param[2]);
			break;
		case 221:
			printf
			    ("ERRORS HAVE BEEN DETECTED.  Run fsck with the -f\nparameter to repair.\n");
			break;
		case 223:
			printf("%s directory reconnected to /lost+found/.\n", param[0]);
			break;
		case 224:
			printf("Unable to reconnect %s directory.  FSCK continuing.\n", param[0]);
			break;
		case 230:
			printf
			    ("Files and/or directories not connected to the directory tree\nhave been detected.\n");
			break;
		case 231:
			printf
			    ("Directory entries for unallocated files have been detected.  Will remove.\n");
			break;
		case 232:
			printf
			    ("Files and/or directories not connected to the directory tree\nhave been detected.  Will reconnect.\n");
			break;
		case 233:
			printf
			    ("Directories with illegal hard links have been detected. Will correct.\n");
			break;
		case 234:
			printf
			    ("Directories (entries '..') referring to incorrect files have been detected.  Will correct.\n");
			break;
		case 236:
			printf("File system is clean.\n");
			break;
		case 237:
			printf
			    ("File system is clean but is marked dirty.  Start FSCK with the\n-f parameter to fix.\n");
			break;
		case 238:
			printf("File system is dirty.\n");
			break;
		case 239:
			printf
			    ("File system is dirty but is marked clean.  In its present state, the\nresults of accessing %s (except by this utility) are undefined.\n",
			     param[0]);
			break;
		case 241:
			printf
			    ("FSCK has marked the file system dirty because it contains critical errors.\nFile system may be unrecoverable.\n");
			break;
		case 250:
			printf("Directory has an entry for unallocated file %s%s.\n", param[0],
			       param[1]);
			break;
		case 265:
			printf("Errors detected in the Fileset File/Directory Allocation Map.\n");
			break;
		case 267:
			printf
			    ("Errors detected in the Fileset File/Directory Allocation Map control information.\n");
			break;
		case 279:
			printf("Incorrect data detected in disk allocation structures.\n");
			break;
		case 280:
			printf("Incorrect data detected in disk allocation control structures.\n");
			break;
		case 321:
			printf("Directory claims cross linked blocks.\n");
			break;
		case 322:
			printf("File system object claims cross linked blocks.\n");
			break;
		case 323:
			printf("File system is formatted for sparse files.\n");
			break;
		case 325:
			printf("%s directories reconnected to /lost+found/.\n", param[0]);
			break;
		case 326:
			printf("%s file reconnected to /lost+found/.\n", param[0]);
			break;
		case 327:
			printf("%s files reconnected to /lost+found/.\n", param[0]);
			break;
		case 328:
			printf("Cannot repair the data format error(s) in this directory.\n");
			break;
		case 329:
			printf("Cannot repair the data error(s) in this directory.\n");
			break;
		case 330:
			printf
			    ("Cannot repair the data format error(s) in this file system object.\n");
			break;
		case 331:
			printf("Cannot repair the format error(s) in this file system object.\n");
			break;
		case 332:
			printf("Phase 7 - Verify File/Directory Allocation Maps.\n");
			break;
		case 333:
			printf("Phase 8 - Verify Disk Allocation Maps.\n");
			break;
		case 335:
			printf
			    ("NOTE: The file system type for %s is not listed \nas jfs in the file system description file /etc/fstab.\n\n",
			     param[0]);
			break;
		case 343:
			printf("Unable to read the secondary File/Directory Allocation Table.\n");
			break;
		case 344:
			printf
			    ("Errors detected in the File System File/Directory Allocation Map.\n");
			break;
		case 345:
			printf
			    ("Errors detected in the File System File/Directory Allocation Map control information.\n");
			break;
		case 346:
			printf
			    ("Errors detected in the secondary File/Directory Allocation Table.\n");
			break;
		case 351:
			printf("Unable to reconnect %s directories.  FSCK continuing.\n", param[0]);
			break;
		case 352:
			printf("Unable to reconnect %s file.  FSCK continuing.\n", param[0]);
			break;
		case 353:
			printf("Unable to reconnect %s files.  FSCK continuing.\n", param[0]);
			break;
		case 374:
			printf("Unable to read the primary File/Directory Allocation Table.\n");
			break;
		case 375:
			printf("Errors detected in the primary File/Directory Allocation Table.\n");
			break;
		case 376:
			printf("CANNOT CONTINUE.\n");
			break;
		case 392:
			printf
			    ("The LVM has detected bad blocks in the partition.\nStart FSCK with the -b parameter to transfer entries from the LVM bad block\ntable to the JFS bad block table.\n");
			break;
		case 395:
			printf("THE FILE SYSTEM IS NOW AVAILABLE.\n");
			break;
		case 396:
			printf
			    ("Transferring entries from the LVM Bad Block Table for this file system to the JFS Bad Block Table for this file system.\n");
			break;
		case 397:
			printf("INTERNAL ERROR (%s,%s,%s,%s). FSCK CANNOT CONTINUE.\n", param[0],
			       param[1], param[2], param[3]);
			break;
		case 402:
			printf
			    ("LVM reports %s bad blocks.  Of these, %s have been transferred to the JFS Bad Block List.\n",
			     param[0], param[1]);
			break;
		case 406:
			printf
			    ("Device unavailable or locked by another process.  FSCK CANNOT CONTINUE.\n");
			break;
		case 408:
			printf("File system object %s%s%s is illegally linked as: %s\n", param[0],
			       param[1], param[2], param[3]);
			break;
		case 409:
			printf
			    ("Insufficient storage (%s) available to continue reconnecting lost files and/or directories. Continuing.\n",
			     param[0]);
			break;
		case 410:
			printf("Format error in Access Control List space or descriptor.\n");
			break;
		case 411:
			printf
			    ("Format error in Access Control List space or descriptor. Will clear.\n");
			break;
		case 415:
			printf("Minor format error detected.\n");
			break;
		case 417:
			printf("Minor format error detected. Will fix.\n");
			break;
		case 420:
			printf("File system checked READ ONLY.\n");
			break;
		case 421:
			printf("Error: device does not exist.\n");
			break;
		case 422:
			printf("Error: no device given.\n");
			break;
		case MSG_JFS_NOT_JFS:
			printf("The specified device is mounted, but is not a JFS file system.\n");
			break;
		case MSG_JFS_BAD_SUPERBLOCK:
			printf
			    ("The file system version is incompatible with this program, or the superblock is corrupted.\n");
			break;
		case MSG_JFS_DIRTY:
			printf("The file system is marked dirty.\n");
			break;
		case MSG_JFS_EXTENDFS_COMPLETE:
			printf("EXTENDFS completed successfully.\n");
			break;
		case MSG_JFS_EXTENDFS_USAGE:
			printf(" Usage:  EXTENDFS [-LS:logSize] <device>.\n");
			break;
		case MSG_JFS_EXTENDFS_FAILED:
			printf("The system failed to extend the file system.\n");
			break;
		case MSG_JFS_DEFRAGMENTING:
			printf("Defragmenting device %s.  Please wait.\n", param[0]);
			break;
		case MSG_JFS_DEFRAGFS_COMPLETE:
			printf("\nDEFRAGFS completed successfully.\n");
			break;
		case MSG_JFS_NUM_DEFRAGED:
			printf("%s allocation groups defragmented.\n", param[0]);
			break;
		case MSG_JFS_SKIPPED_FREE:
			printf("%s allocation groups skipped - entirely free.\n", param[0]);
			break;
		case MSG_JFS_SKIPPED_FULL:
			printf("%s allocation groups skipped - too few free blocks.\n", param[0]);
			break;
		case MSG_JFS_SKIPPED_CONTIG:
			printf
			    ("%s allocation groups skipped - contains a large contiguous free space.\n",
			     param[0]);
			break;
		case MSG_JFS_TOTAL_AGS:
			printf("\nTotal allocation groups: %s\n", param[0]);
			break;
		case MSG_JFS_DEFRAGFS_USAGE:
			printf("Usage:  defragfs [-q] <device>\n");
			break;
		case MSG_JFS_NUM_CANDIDATE:
			printf("%s allocation groups are candidates for defragmenting.\n",
			       param[0]);
			break;
		case MSG_JFS_AVG_FREE_RUNS:
			printf("Average number of free runs in candidate allocation groups: %s.\n",
			       param[0]);
			break;
		case MSG_JFS_UNEXPECTED_ERROR:
			printf("\n      DEFRAGFS FATAL ERROR (%s).  DEFRAGFS CANNOT CONTINUE.\n",
			       param[0]);
			break;
		case MSG_JFS_VOLUME_IS_MOUNTED:
			printf
			    ("\nThe specified device is currently mounted!  Please unmount the device and try again.\n");
			break;
		case MSG_JFS_MNT_LIST_ERROR:
			printf
			    ("\nError accessing the list of mounted devices!  Operation aborted!\n");
			break;
		case MSG_JFS_VOLUME_IS_MOUNTED_RO:
			printf
			    ("\nThe specified device is currently mounted read only!\nfsck will report, but not repair, any errors.\n");
			break;
		default:	/* Error!  Unknown message number! */
			break;
		}

	}
}
