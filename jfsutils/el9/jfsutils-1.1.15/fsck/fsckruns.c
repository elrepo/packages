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
#include <signal.h>
#include <string.h>
#include <unistd.h>

/* defines and includes common among the fsck.jfs modules */
#include "xfsckint.h"

extern char *MsgText[];

/*****************************************************************************
 * NAME: fsck_hbeat
 *
 * FUNCTION:
 *
 * PARAMETERS:  none
 *
 * NOTES:
 *	This is racy, but we don't care.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
void fsck_hbeat(int unused)
{
	static volatile unsigned long current_heartbeat = 0;

	switch (current_heartbeat) {
	case 0:
		printf("%s", msg_defs[fsck_HEARTBEAT0].msg_txt);
		current_heartbeat++;
		break;
	case 1:
		printf("%s", msg_defs[fsck_HEARTBEAT1].msg_txt);
		current_heartbeat++;
		break;
	case 2:
		printf("%s", msg_defs[fsck_HEARTBEAT2].msg_txt);
		current_heartbeat++;
		break;
	case 3:
		printf("%s", msg_defs[fsck_HEARTBEAT3].msg_txt);
		current_heartbeat++;
		break;
	case 4:
		printf("%s", msg_defs[fsck_HEARTBEAT4].msg_txt);
		current_heartbeat++;
		break;
	case 5:
		printf("%s", msg_defs[fsck_HEARTBEAT5].msg_txt);
		current_heartbeat++;
		break;
	case 6:
		printf("%s", msg_defs[fsck_HEARTBEAT6].msg_txt);
		current_heartbeat++;
		break;
	case 7:
		printf("%s", msg_defs[fsck_HEARTBEAT7].msg_txt);
		current_heartbeat++;
		break;
	case 8:
		printf("%s", msg_defs[fsck_HEARTBEAT8].msg_txt);
		current_heartbeat++;
		break;
	case 9:
		printf("%s", msg_defs[fsck_HEARTBEAT7].msg_txt);
		current_heartbeat++;
		break;
	case 10:
		printf("%s", msg_defs[fsck_HEARTBEAT6].msg_txt);
		current_heartbeat++;
		break;
	case 11:
		printf("%s", msg_defs[fsck_HEARTBEAT5].msg_txt);
		current_heartbeat++;
		break;
	case 12:
		printf("%s", msg_defs[fsck_HEARTBEAT4].msg_txt);
		current_heartbeat++;
		break;
	case 13:
		printf("%s", msg_defs[fsck_HEARTBEAT3].msg_txt);
		current_heartbeat++;
		break;
	case 14:
		printf("%s", msg_defs[fsck_HEARTBEAT2].msg_txt);
		current_heartbeat++;
		break;
	case 15:
		printf("%s", msg_defs[fsck_HEARTBEAT1].msg_txt);
		current_heartbeat = 0;
		break;
	default:
		printf("%s", msg_defs[fsck_HEARTBEAT0].msg_txt);
		current_heartbeat = 1;
	}

	fflush(stdout);
	alarm(1);
}

/*****************************************************************************
 * NAME: fsck_hbeat_start
 *
 * FUNCTION:
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
void fsck_hbeat_start(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof (sa));
	sa.sa_handler = &fsck_hbeat;
	sa.sa_flags = SA_RESTART;

	sigaction(SIGALRM, &sa, NULL);
	alarm(1);
}

/*****************************************************************************
 * NAME: fsck_hbeat_stop
 *
 * FUNCTION:
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
void fsck_hbeat_stop(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof (sa));
	sa.sa_handler = SIG_DFL;

	sigaction(SIGALRM, &sa, NULL);
	alarm(0);
}
