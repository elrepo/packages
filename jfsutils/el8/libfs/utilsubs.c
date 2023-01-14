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
#include <stdlib.h>
#include <string.h>

#include "jfs_types.h"
#include "utilsubs.h"

/*
 *	log2shift()
 */
int32_t log2shift(uint32_t n)
{
	uint32_t shift = 0;

	while (n > 1) {
		/* n is not power of 2 */
		if (n & 1)
			return -1;

		shift++;
		n >>= 1;
	}

	return shift;
}

/*
 *	ui
 *	==
 */
/*
 *	prompt()
 */
char prompt(char *str)
{
	char cmd[81];

	fputs(str, stdout);
	fflush(stdout);

	/* get NULL terminated input */
	fgets(cmd, 81, stdin);

	return cmd[0];		/* return response letter */
}

/*
 *	more()
 */
int more(void)
{
	char cmd[81];

	fputs("- hit Enter to continue, e[x]it -", stdout);
	fflush(stdout);

	/* get NULL terminated input */
	fgets(cmd, 80, stdin);

	if (cmd[0] == 'x')
		return 1;	/* do NOT continue */
	else
		return 0;	/* continue */
}
