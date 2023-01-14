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
#include "xfsck.h"
#include "jfs_byteorder.h"

/* some macros for dealing with variable length EA lists. */
#define FEA_SIZE(ea) (__le16_to_cpu((ea)->cbValue) + (ea)->cbName + 1 + \
		      sizeof (struct FEA))
#define	NEXT_FEA(ea) ((struct FEA*)(((char *) (ea)) + (FEA_SIZE (ea))))

#define GEA_SIZE(ea) ((ea)->cbName + sizeof (GEA))
#define	NEXT_GEA(ea) ((GEA*)(((char *) (ea)) + (GEA_SIZE (ea))))

/* an extended attribute consists of a <name,value> double with <name>
 * being restricted to a subset of the 8 bit ASCII character set.  this
 * table both defines valid characters for <name> and provides a lower-case
 * to upper-case mapping.
 */

#define CH_BAD_EA	'\0'

/*
 * jfs_ValidateFEAList -- validate structure of an FEALIST
 */

int jfs_ValidateFEAList(struct FEALIST *pfeal, int size, unsigned long *poError)
{
	unsigned int cbLeft;	/* count of bytes left in FEA list */
	struct FEA *pfea = pfeal->list;	/* pointer to current FEA */
	unsigned int cbFEA;	/* count of bytes in current FEA */

	cbLeft = __le32_to_cpu(pfeal->cbList);
	if (size !=  cbLeft)
		return ERROR_EA_LIST_INCONSISTENT;

	cbLeft -= sizeof (pfeal->cbList);

	if (cbLeft == 0)
		return 0;

	do {
		/* check for our reserved bits
		 */
		if (pfea->fEA & ~(FEA_NEEDEA) || cbLeft < sizeof *pfea)
			return ERROR_EA_LIST_INCONSISTENT;

		cbFEA = FEA_SIZE(pfea);
		pfea = NEXT_FEA(pfea);

		if (cbLeft < cbFEA) {
			*poError = (((char *) pfea) - ((char *) pfeal));
			return ERROR_EA_LIST_INCONSISTENT;
		}

	} while ((cbLeft -= cbFEA) > 0);

	return 0;
}
