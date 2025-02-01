/*  The code in this module was taken from:
 *
 * linux/fs/nls.c
 *
 * Native language support--charsets and unicode translations.
 * By Gordon Chaffee 1996, 1997
 *
 */
#include <config.h>
#include "unicode_to_utf8.h"

/*
 * Sample implementation from Unicode home page.
 * http://www.stonehand.com/unicode/standard/fss-utf.html
 */
struct utf8_table {
	int cmask;
	int cval;
	int shift;
	long lmask;
	long lval;
};

static struct utf8_table utf8_table[] = {
	{0x80, 0x00, 0 * 6, 0x7F, 0, /* 1 byte sequence */ },
	{0xE0, 0xC0, 1 * 6, 0x7FF, 0x80, /* 2 byte sequence */ },
	{0xF0, 0xE0, 2 * 6, 0xFFFF, 0x800, /* 3 byte sequence */ },
	{0xF8, 0xF0, 3 * 6, 0x1FFFFF, 0x10000, /* 4 byte sequence */ },
	{0xFC, 0xF8, 4 * 6, 0x3FFFFFF, 0x200000, /* 5 byte sequence */ },
	{0xFE, 0xFC, 5 * 6, 0x7FFFFFFF, 0x4000000, /* 6 byte sequence */ },
	{0, /* end of table    */ }
};

int Unicode_Character_to_UTF8_Character(uint8_t * s, uint16_t wc, int maxlen)
{
	long l;
	int c, nc;
	struct utf8_table *t;

	if (s == 0)
		return 0;

	l = wc;
	nc = 0;
	for (t = utf8_table; t->cmask && maxlen; t++, maxlen--) {
		nc++;
		if (l <= t->lmask) {
			c = t->shift;
			*s = t->cval | (l >> c);
			while (c > 0) {
				c -= 6;
				s++;
				*s = 0x80 | ((l >> c) & 0x3F);
			}
			return nc;
		}
	}
	return -1;
}

int Unicode_String_to_UTF8_String(uint8_t * s, const uint16_t * pwcs, int maxlen)
{
	const uint16_t *ip;
	uint8_t *op;
	int size;

	op = s;
	ip = pwcs;
	while (*ip && maxlen > 0) {
		if (*ip > 0x7f) {
			size = Unicode_Character_to_UTF8_Character(op, *ip, maxlen);
			if (size == -1) {
				/* Ignore character and move on */
				maxlen--;
			} else {
				op += size;
				maxlen -= size;
			}
		} else {
			*op++ = (uint8_t) * ip;
			maxlen--;
		}
		ip++;
	}
	return (op - s);
}

int UTF8_Character_To_Unicode_Character(uint16_t * p, const uint8_t * s, int maxLen)
{
	long l;
	int c0, c, nc;
	struct utf8_table *t;

	nc = 0;
	c0 = *s;
	l = c0;
	for (t = utf8_table; t->cmask; t++) {
		nc++;
		if ((c0 & t->cmask) == t->cval) {
			l &= t->lmask;
			if (l < t->lval)
				return -1;
			*p = l;
			return nc;
		}
		if (maxLen <= nc)
			return -1;
		s++;
		c = (*s ^ 0x80) & 0xFF;
		if (c & 0xC0)
			return -1;
		l = (l << 6) | c;
	}
	return -1;
}

int UTF8_String_To_Unicode_String(uint16_t * pwcs, const uint8_t * s, int maxLen)
{
	uint16_t *op;
	const uint8_t *ip;
	int size;

	op = pwcs;
	ip = s;
	while (*ip && maxLen > 0) {
		if (*ip & 0x80) {
			size = UTF8_Character_To_Unicode_Character(op, ip, maxLen);
			if (size == -1) {
				/* Ignore character and move on */
				ip++;
				maxLen--;
			} else {
				op += size;
				ip += size;
				maxLen -= size;
			}
		} else {
			*op++ = *ip++;
		}
	}
	return (op - pwcs);
}
