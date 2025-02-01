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
 *   FUNCTIONS: no-frills substitutes for fsck routines
 *              used by logredo modules outside of fsck
 */
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "fsck_message.h"
#include "jfs_types.h"

#define STDOUT_HANDLE  1
#define STDERR_HANDLE  2

extern short MsgProtocol[][2];

extern char *msgprms[];
extern short msgprmidx[];

extern unsigned long msgs_txt_maxlen;

/****************************************************************************
 * NAME: alloc_wrksp
 *
 * FUNCTION:  Allocates and initializes (to guarantee the storage is backed)
 *            dynamic storage for the caller.
 *
 * PARAMETERS:
 *      length         - input - the number of bytes of storage which are needed
 *      dynstg_object  - input - a constant (see xfsck.h) identifying the purpose
 *                               for which the storage is needed (Used in error
 *                               message if the request cannot be satisfied.
 *      addr_wrksp_ptr - input - the address of a variable in which this routine
 *                               will return the address of the dynamic storage
 *                               allocated for the caller
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int alloc_wrksp(unsigned length, int dynstg_object, int for_logredo, void **addr_wrksp_ptr)
{
	int awsp_rc = 0;
	unsigned min_length;

	*addr_wrksp_ptr = NULL;	/* initialize return value */
	min_length = ((length + 7) / 4) * 4;	/* round up to an 4 byte boundary */

	*addr_wrksp_ptr = (char *) malloc(min_length);

	return (awsp_rc);
}

/*****************************************************************************
 * NAME: v_fsck_send_msg
 *
 * FUNCTION:
 *
 * PARAMETERS:
 *      ?                 - input -
 *      ?                 - returned -
 *
 * RETURNS:
 * 	nothing
 */
int v_fsck_send_msg(int msg_num, const char *file_name, int line_number, ...) {
	struct fsck_message *message = &msg_defs[msg_num];

	char msg_string[max_log_entry_length - 4];
	char debug_detail[100];
	va_list args;

	va_start(args, line_number);
	vsnprintf(msg_string, sizeof(msg_string), message->msg_txt, args);
	va_end(args);

	sprintf(debug_detail, " [%s:%d]\n", file_name, line_number);

	printf(msg_string);
	printf(debug_detail);

	return 0;
}
