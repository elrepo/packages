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

#include "fscklog.h"

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  * record for xchklog processing
  *
  */
struct fscklog_record {
	int64_t ondev_wsp_byte_length;
	int64_t ondev_wsp_byte_offset;
	uint32_t ondev_wsp_fsblk_length;
	int64_t ondev_wsp_fsblk_offset;
	uint32_t ondev_fscklog_fsblk_length;
	int64_t ondev_fscklog_fsblk_offset;
	int64_t ondev_fscklog_byte_length;
	int64_t ondev_fscklog_byte_offset;
	char *infile_buf_ptr;
	uint32_t infile_buf_length;
	uint32_t infile_buf_data_len;
	int64_t infile_agg_offset;
	int64_t infile_log_offset;
	struct fscklog_entry_hdr * last_msghdr;
	char *outfile_buf_ptr;
	uint32_t outfile_buf_length;
	uint32_t outfile_buf_data_len;
	int ag_blk_size;
	int8_t which_log;
	int8_t device_is_open;
	int8_t outfile_is_open;
	int8_t this_buffer_last_message;
	int8_t fscklog_last_read;
	int8_t file_name_specified;
	unsigned highest_msg_num;
	int8_t infile_is_open;
};

#define  default_logfile_new		"fscklog.new"
#define  default_logfile_old		"fscklog.old"
#define  default_logfile_name_len	11
#define  NEWLOG  1
#define  OLDLOG  2

/*
 * module return code and possible values
 */
#define XCHKDMP_FAILED             -1
#define XCHKDMP_CANTOPENINFILE     -2
#define XCHKDMP_MTORBADREAD        -3
#define XCHKDMP_BADFORMAT          -4
#define XCHKLOG_CANTOPENOUTFILE    -5

extern int xchklog(struct fscklog_record *local_recptr);
extern int xchkdmp(struct fscklog_record *local_recptr);
