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
 * Notes:
 *     These APIs are based on the C library functions.  The semantics
 *     should match the C functions but with expanded size operands.
 *
 *     The upper/lower functions are based on a table created by mkupr.
 *     This is a compressed table of upper and lower case conversion.
 */

struct UNICASERANGE {
	UniChar start;
	UniChar end;
	signed char *table;
};

extern signed char UniUpperTable[512];
extern struct UNICASERANGE UniUpperRange[];

/*
 * UniStrcpy:  Copy a string
 */
static inline UniChar *UniStrcpy(UniChar * ucs1, const UniChar * ucs2)
{
	UniChar *anchor = ucs1;	/* save the start of result string */

	while ((*ucs1++ = *ucs2++));
	return anchor;
}


/*
 * UniStrlen:  Return the length of a string
 */
static inline size_t UniStrlen(const UniChar * ucs1)
{
	int i = 0;

	while (*ucs1++)
		i++;
	return i;
}


/*
 * UniStrncmp:  Compare length limited string
 */
static inline int UniStrncmp(const UniChar * ucs1, const UniChar * ucs2,
			     size_t n)
{
	if (!n)
		return 0;	/* Null strings are equal */
	while ((*ucs1 == *ucs2) && *ucs1 && --n) {
		ucs1++;
		ucs2++;
	}
	return (int) *ucs1 - (int) *ucs2;
}


/*
 * UniStrncpy:  Copy length limited string with pad
 */
static inline UniChar *UniStrncpy(UniChar * ucs1, const UniChar * ucs2,
				  size_t n)
{
	UniChar *anchor = ucs1;

	while (n-- && *ucs2)	/* Copy the strings */
		*ucs1++ = *ucs2++;

	n++;
	while (n--)		/* Pad with nulls */
		*ucs1++ = 0;
	return anchor;
}


/*
 * UniToupper:  Convert a unicode character to upper case
 */
static inline UniChar UniToupper(UniChar uc)
{
	struct UNICASERANGE *rp;

	if (uc < sizeof(UniUpperTable)) {	/* Latin characters */
		return uc + UniUpperTable[uc];	/* Use base tables */
	} else {
		rp = UniUpperRange;	/* Use range tables */
		while (rp->start) {
			if (uc < rp->start)	/* Before start of range */
				return uc;	/* Uppercase = input */
			if (uc <= rp->end)	/* In range */
				return uc + rp->table[uc - rp->start];
			rp++;	/* Try next range */
		}
	}
	return uc;		/* Past last range */
}


/*
 * UniStrupr:  Upper case a unicode string
 */
static inline UniChar *UniStrupr(UniChar * upin)
{
	UniChar *up;

	up = upin;
	while (*up) {		/* For all characters */
		*up = UniToupper(*up);
		up++;
	}
	return upin;		/* Return input pointer */
}

