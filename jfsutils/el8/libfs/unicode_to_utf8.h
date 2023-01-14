/*  The code in this module was taken from:
 *
 * linux/fs/nls.c
 *
 * Native language support--charsets and unicode translations.
 * By Gordon Chaffee 1996, 1997
 *
 */
#ifndef _UNICODE_TO_UTF8_H
#define _UNICODE_TO_UTF8_H

#include "jfs_types.h"

int Unicode_Character_to_UTF8_Character(uint8_t * s, uint16_t wc, int maxlen);

int Unicode_String_to_UTF8_String(uint8_t * s, const uint16_t * pwcs, int maxlen);

int UTF8_String_To_Unicode_String(uint16_t * pwcs, const uint8_t * s, int maxLen);

int UTF8_Character_To_Unicode_Character(uint16_t * p, const uint8_t * s, int maxLen);

#endif
