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
#ifndef _MESSAGE_H_
#define _MESSAGE_H_

/*
 *  message_user (mesg_no, param, param_cnt, msg_file)
 *
 *  ENTRY    mesg_no   - index into the array of messages
 *           param     - list of pointers to parameters in the message
 *           param_cnt - count of parameters in the param variable
 *           msg_file  - OSO_MSG/JFS_MSG which message file to use
 *
 */
extern void message_user(unsigned, char **, unsigned, int);

#define OSO_MSG 0
#define JFS_MSG 1

#define MSG_JFS_PART_SMALL           1
#define MSG_JFS_LOG_LARGE            2
#define MSG_JFS_BAD_PART             5
#define MSG_JFS_NOT_JFS            450
#define MSG_JFS_BAD_SUPERBLOCK     451
#define MSG_JFS_DIRTY              452
#define MSG_JFS_EXTENDFS_COMPLETE  453
#define MSG_JFS_EXTENDFS_USAGE     454
#define MSG_JFS_EXTENDFS_FAILED    455
#define MSG_JFS_DEFRAGMENTING      456
#define MSG_JFS_DEFRAGFS_COMPLETE  457
#define MSG_JFS_NUM_DEFRAGED       458
#define MSG_JFS_SKIPPED_FREE       459
#define MSG_JFS_SKIPPED_FULL       460
#define MSG_JFS_SKIPPED_CONTIG     461
#define MSG_JFS_TOTAL_AGS          462
#define MSG_JFS_DEFRAGFS_USAGE     463
#define MSG_JFS_NUM_CANDIDATE      464
#define MSG_JFS_AVG_FREE_RUNS      465
#define MSG_JFS_UNEXPECTED_ERROR   466
#define MSG_JFS_VOLUME_IS_MOUNTED  467
#define MSG_JFS_MNT_LIST_ERROR     468
#define MSG_JFS_VOLUME_IS_MOUNTED_RO  469
#define MSG_JFS_DEV_NOT_IN_TABLE   470

#define MSG_OSO_NOT_ENOUGH_MEMORY    8
#define MSG_OSO_CANT_FIND_DRIVE     15
#define MSG_OSO_CANT_OPEN          110
#define MSG_OSO_DISK_FULL          112

#define MSG_OSO_FORMAT_FAILED      528
#define MSG_OSO_INSUFF_MEMORY      529
#define MSG_OSO_DISK_SPACE         535
#define MSG_OSO_FREE_SPACE         537
#define MSG_OSO_PERCENT_FORMAT     538
#define MSG_OSO_DISK_LOCKED        566
#define MSG_OSO_DISK_SPACE2        568
#define MSG_OSO_FORMAT_COMPLETE    635
#define MSG_OSO_ERR_ACCESSING_DISK 868
#define MSG_OSO_VALUE_NOT_ALLOWED  873
#define MSG_OSO_DESTROY_DATA      1271
#define MSG_OSO_READ_ERROR        1566
#define MSG_OSO_NOT_VALID_BLOCK_DEVICE 1567

#endif
