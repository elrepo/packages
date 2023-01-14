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
 *   COMPONENT_NAME: jfs_fscklog
 *
 *      This tool extracts the contents of the specified (or implied)
 *      fsck service log on the specified device.  The output is
 *      written to a file (name specified or defaulted).
 *
 *      This routine also displays the contents of an extracted
 *      fsck service log.  If no input file name is specified,
 *      a default path and name are used.
 *
 *      USAGE:
 *
 *         jfs_fscklog [-d] [-e <device>] [-f <file.name>] [-p] [-V]
 *
 *         where: o -d displays an fsck.jfs log already extracted with -e
 *                o -e extracts an fsck.jfs log from device
 *                o -f specifies the file name of the extracted log
 *                o <file.name> is assumed to be in the present working
 *                  directory unless it is fully qualified
 *                o <file.name> must be 127 characters or less in length
 *                o if <file.name> is not specified, the default path
 *                  and name are used:    <pwd>fscklog.new
 *                o -p specifies extracting the prior log
 *                o -V prints the verstion number and date, and exits
 *
 *      SAMPLE INVOCATIONS:
 *         To extract the most recent log on /dev/hda5 into <pwd>fscklog.new
 *                     jfs_fscklog -e /dev/hda5
 *
 *         To extract the most recent log on /dev/hda7 into <pwd>output.fil
 *                     jfs_fscklog -e /dev/hda7 -f output.fil
 *
 *         To extract and display the most recent log on /dev/hdb3
 *                     jfs_fscklog -e /dev/hdb3 -d
 *
 *         To extract and display the prior log on /dev/hdb4 into <pwd>fscklog.old
 *                     jfs_fscklog -p -e /dev/hdb4 -d
 *
 *         To display the contents of the fsck service log in <pwd>fscklog.new
 *                     jfs_fscklog -d
 *
 *         To display the contents of the fsck service log in <pwd>input.fil
 *                     jfs_fscklog  -d -f input.fil
 *
 */
#define _GNU_SOURCE	/* for basename() */
#include <config.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include "jfs_version.h"
#include "jfs_fscklog.h"
#include "xfsck.h"
#include "fsck_message.h"

struct fscklog_record fscklog_record;
struct fscklog_record *local_recptr;

char file_name[128];
char *Vol_Label = NULL;

bool extract_log;
bool display_log;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * The following are internal to this file
 *
 */
int parse_parms(int, char **);
int v_send_msg(int, const char *, int, ...);

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV */

void fscklog_usage(void)
{
	printf("\nUsage:  jfs_fscklog [-d] [-e device] [-f file.name] [-p] [-V]\n");
	printf("\nEmergency help:\n"
	       " -d            Display an already extracted fsck.jfs service log.\n"
	       " -e device     Extract the fsck.jfs service log from device.\n"
	       " -f file.name  Specify the file name that the fsck.jfs log will be extracted into, or\n"
	       "               the file name of the already extracted fsck.jfs log that will be displayed.\n"
	       " -p            Extract the previous fsck.jfs service log.\n"
	       " -V            Print version information only.\n"
	       "NOTE: -e and -d can be used together to extract and display the fsck.jfs service log.\n");
	return;
}

/*****************************************************************************
 * NAME: main
 *
 * FUNCTION: Entry point for jfs read/display aggregate fsck service log
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int main(int argc, char **argv)
{
	int rc = 0;

	printf("jfs_fscklog version %s, %s\n", VERSION, JFSUTILS_DATE);

	/*
	 * some basic initializations
	 */
	local_recptr = &fscklog_record;
	memset(local_recptr, 0, sizeof (fscklog_record));
	local_recptr->which_log = NEWLOG;
	local_recptr->file_name_specified = 0;
	extract_log = display_log = 0;
	memset((void *) file_name, 0, 128);

	/*
	 * Process the parameters given by the user
	 */
	rc = parse_parms(argc, argv);

	if ((rc == 0) && (extract_log)) {
		rc = xchklog(local_recptr);
	}

	if ((rc == 0) && (display_log)) {
		rc = xchkdmp(local_recptr);
	}

	return (rc);
}

/*****************************************************************************
 * NAME: parse_parms
 *
 * FUNCTION:  Parse the invocation parameters.  If any unrecognized
 *            parameters are detected, or if any required parameter is
 *            omitted, issue a message and exit.
 *
 * PARAMETERS:  as specified to main()
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int parse_parms(int argc, char **argv)
{
	int pp_rc = 0;
	int arg_len = 0;
	int c;
	FILE *file_p = NULL;

	while ((c = getopt(argc, argv, "de:f:pV")) != EOF) {
		switch (c) {
		case 'd':
			/* display extracted log */
			display_log = -1;
			break;
		case 'e':
			/* extract fsck.jfs log */
			extract_log = -1;

			Vol_Label = optarg;

			/* ensure volume is valid */
			file_p = fopen(Vol_Label, "r");
			if (file_p) {
				fclose(file_p);
			} else {
				send_msg(fsck_XCHKLOGBADDEVICE, Vol_Label);
				fscklog_usage();
				return (FSCK_FAILED);
			}

			break;
		case 'f':
			/* specify file name */
			arg_len = strlen(optarg);
			if (arg_len > 128) {
				/* filename too long */
				send_msg(fsck_XCHKDMPBADFNAME);
				return (XCHKDMP_FAILED);
			} else {
				/* go with the specified file name */
				strncpy(file_name, optarg, arg_len);
				local_recptr->file_name_specified = -1;
			}
			break;
		case 'p':
			/* extract old log */
			local_recptr->which_log = OLDLOG;
			break;
		case 'V':
			/* print version and exit */
			exit(0);
			break;
		default:
			fscklog_usage();
			return (XCHKDMP_FAILED);
		}
	}

	if (argc < 2) {
		fscklog_usage();
		return (XCHKDMP_FAILED);
	}

	return (pp_rc);
}


/*****************************************************************************
 * NAME: v_send_msg
 *
 * FUNCTION: according to the fsck message structure
 *
 * PARAMETERS:
 *
 * RETURNS:
 *	nothing
 */
int v_send_msg(int msg_num, const char *file_name, int line_number, ...) {
	struct fsck_message *message = &msg_defs[msg_num];

	char msg_string[max_log_entry_length - 4];
	char debug_detail[100];
	va_list args;

	va_start(args, line_number);
	vsnprintf(msg_string, sizeof(msg_string), message->msg_txt, args);
	va_end(args);

	sprintf(debug_detail, " [%s:%d]\n", basename(file_name), line_number);

	printf(msg_string);
	printf(debug_detail);

	return 0;
}
