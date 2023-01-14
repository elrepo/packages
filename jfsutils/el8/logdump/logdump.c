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
 *   FUNCTION: dumps the contents of the journal log on the
 *             specified JFS partition into "./jfslog.dmp"
 */
#include <config.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pwd.h>

#include "jfs_types.h"
#include "devices.h"
#include "jfs_version.h"

#define LOGDMP_OK  0
#define LOGDMP_FAILED -1

#define  FULLLOG  -1
#define  CURRLOG   1

int jfs_logdump(char *, FILE *, int);
int parse_parms(int, char **);

char log_device[1];		/* This avoids linker error */

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  * Device information.
  *
  *     values are assigned when (if) the device is opened.
  */
FILE *Dev_IOPort;
unsigned Dev_blksize;

char *Vol_Label = NULL;

int dump_all = 0;

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  * For message processing in called routines
  *
  */

extern caddr_t prog;

extern void fsck_send_msg(int, int);	/* defined in fsckmsg.c */
extern int alloc_wrksp(unsigned, int, int, void **);	/* defined in fsckwsp.c */

/****************************************************************************
 *
 * NAME: main
 *
 * FUNCTION: call jfs_logdump()
 *
 * INTERFACE:
 *            jfs_logdump [-a] <block device>
 *
 *  where -a => dump entire contents of log instead of just
 *    committed transactions since last synch point.
 *
 * RETURNS:
 *      success: 0
 *      failure: something else
 */
int main(int argc, char **argv)
{
	int rc = LOGDMP_OK;

	printf("jfs_logdump version %s, %s\n", VERSION, JFSUTILS_DATE);
	prog = "jfs_logdump";

	rc = parse_parms(argc, argv);	/* parse the parms and record
					 * them in the aggregate wsp record
					 */
	if (rc != 0)
		return 0;

	printf("Device Name: %s\n", Vol_Label);

	Dev_IOPort = fopen(Vol_Label, "r");
	if (Dev_IOPort == NULL) {
		fprintf(stderr, "Error: Cannot open device %s, rc = %d.\n", Vol_Label, rc);
		return (-1);
	}

	jfs_logdump(Vol_Label, Dev_IOPort, dump_all);
	return fclose(Dev_IOPort);
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
 *      success: LOGDMP_OK
 *      failure: something else
 */
int parse_parms(int argc, char **argv)
{
	int pp_rc = LOGDMP_OK;
	int i = 0;

	char *argp;

	for (i = 1; i < argc; i++) {	/* for all parms on command line */
		argp = argv[i];

		if (*argp == '-') {	/* leading - */
			argp++;
			if (*argp == 'a' || *argp == 'A') {	/* debug */
				/*
				 * Dump entire log Option
				 */
				dump_all = -1;
			} else {
				/*
				 * unrecognized keyword parm
				 */
				printf("JFS_LOGDUMP:  unrecognized keyword detected:   %s\n", argp);
				return (LOGDMP_FAILED);
			}

		} else if (argp[0] == '/') {	/* 2nd char is / */
			Vol_Label = argp;

		} else {	/* unrecognized parm */
			printf("JFS_LOGDUMP:  unsupported parameter detected:   %s\n", argp);
			return (LOGDMP_FAILED);
		}
	}

	if (Vol_Label == NULL) {	/* no device specified */
		printf("Error: no device given.\n");
		printf("Usage: jfs_logdump [-a] <block device>\n");
		return (LOGDMP_FAILED);
	}
	return (pp_rc);
}
