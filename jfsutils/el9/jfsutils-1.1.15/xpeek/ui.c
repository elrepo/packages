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
 *	 ui.c - User Interface routines
 */
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
	m_parse - parse parameters to 'm'odify subcommand

	NOTE:  Assumes last call to strtok() parsed "m" subcommand from
	command line.
*/
int m_parse(char *cmd_line, int n_fields, char **value)
{
	int field_number;
	char *token;

	token = strtok(0, " 	\n");
	if (token == 0) {
		fputs("Please enter: field-number value > ", stdout);
		fgets(cmd_line, 80, stdin);
		token = strtok(cmd_line, " 	\n");
		if (token == 0)
			return 0;
	}
	field_number = strtol(token, 0, 0);
	if (field_number < 1 || field_number > n_fields) {
		fputs("Invalid field number\n", stderr);
		return 0;
	}
	*value = strtok(0, " 	\n");
	if (*value == 0) {
		fputs("Not enough arguments\n", stderr);
		return 0;
	}
	if (strtok(0, " 	\n")) {
		fputs("Too many arguments\n", stderr);
		return 0;
	}
	return field_number;
}
