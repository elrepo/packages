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
#ifndef H_FSCKLOG
#define	H_FSCKLOG

#include <jfs_types.h>

/*----------------------------------------------------------------------------
 *
 * The in-aggregate fsck service log
 */

#define flog_eyecatcher_string "fscklog "


struct fscklog_entry_hdr {
	int16_t entry_length;
};

struct fscklog_error {
	int64_t err_offset;	/*  8 -- start of attempted write */
	int32_t bytes_written;	/*  4 -- number actually written  */
	int32_t io_retcode;	/*  4 -- return code from write   */
};				/*    total: 16 bytes */

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 *  The first log message number for the JFS fsck Service Log
 *
 *
 */

#define JFSCHKLOG_FIRSTMSGNUM 10000

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 *  The format of the log entry in the file created by
 *  extracting the contents of the fsck service log
 *  (stored in the fsck workspace) into a file.
 *
 *
 * N.B. The log entries are blocked such that no log entry crosses
 *      an XCHKLOG_BUFSIZE byte boundary in the file.
 */

#define XCHKLOG_BUFSIZE  8192

#define jfs_chklog_eyecatcher "JFS chkdskSvcLog"

struct chklog_entry_hdr {
	int16_t entry_length;
};

#endif
