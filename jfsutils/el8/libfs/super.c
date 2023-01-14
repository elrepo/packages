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
#include <time.h>
#include <errno.h>

#include "jfs_endian.h"
#include "jfs_types.h"
#include "jfs_filsys.h"
#include "jfs_superblock.h"
#include "libjufs.h"
#include "devices.h"
#include "jfs_dmap.h"
#include "utilsubs.h"

/*
 * NAME: inrange
 *
 * FUNCTION: Checks to see that <num> is a power-of-2 multiple of <low> that is
 *      less than or equal to <high>.
 *
 * PRE CONDITIONS:
 *
 * POST CONDITIONS:
 *
 * PARAMETERS:
 *
 * NOTES:
 *
 * DATA STRUCTURES:
 *
 * RETURNS: If it is, it returns 1, else 0
 */
int inrange(uint32_t num, uint32_t low, uint32_t high)
{
	if (low) {
		for (; low <= high; low <<= 1) {
			if (low == num) {
				return 1;
			}
		}
	}
	return 0;
}

/*
 * NAME: validate_sizes
 *
 * FUNCTION: Ensure that all configurable sizes fall within their respective
 *      version specific limits.
 *
 * PRE CONDITIONS:
 *
 * POST CONDITIONS:
 *
 * PARAMETERS:
 *      sb      - superblock to check sizes
 *
 * NOTES:
 *
 * RECOVERY OPERATION:
 *
 * DATA STRUCTURES:
 *
 * RETURNS:
 *      success: 0
 *      failure:  any other value
 */
static int validate_sizes(struct superblock *sb)
{
	/*
	 * Make sure AG size is at least 32M
	 */
	if (sb->s_agsize >= (1 << L2BPERDMAP)) {
		return 0;
	}

	return EINVAL;
}

/*
 * NAME: ujfs_validate_super
 *
 * FUNCTION: Check if superblock is valid
 *
 * PRE CONDITIONS:
 *
 * POST CONDITIONS:
 *
 * PARAMETERS:
 *      sb      - superblock to validate
 *
 * NOTES:
 *
 * RECOVERY OPERATION:
 *
 * DATA STRUCTURES:
 *
 * RETURNS:
 *      success: 0
 *      failure: LIBFS_CORRUPTSUPER, LIBFS_BADVERSION, LIBFS_BADMAGIC
 */
int ujfs_validate_super(struct superblock *sb)
{
	if (memcmp(sb->s_magic, JFS_MAGIC, sizeof (sb->s_magic)) == 0) {
		if (sb->s_version > JFS_VERSION)
			return LIBFS_BADVERSION;
		if (validate_sizes(sb) == EINVAL)
			return LIBFS_CORRUPTSUPER;
	} else {
		return LIBFS_BADMAGIC;
	}
	return 0;
}

/*
 * NAME: ujfs_put_superblk
 *
 * FUNCTION: Write primary or secondary aggregate superblock
 *
 * PRE CONDITIONS:
 *
 * POST CONDITIONS:
 *
 * PARAMETERS:
 *      fp              - open port for device to write superblock to
 *      sb              - pointer to struct superblock to be written
 *      is_primary      - 0 value means we are putting the secondary superblock;
 *                        non-zero value means we are putting the primary
 *                        superblock.
 *
 * NOTES: The sizeof(struct superblock) is less than the amount of disk space
 *      being allowed for the superblock (SIZE_OF_SUPER).  This function will
 *      write 0's to the space following the actual superblock structure to fill
 *      the entire allocated disk space.
 *
 * RECOVERY OPERATION:
 *
 * DATA STRUCTURES:
 *
 * RETURNS:
 *      success: 0
 *      failure: any other value
 */
int ujfs_put_superblk(FILE *fp, struct superblock *sb, int16_t is_primary)
{
	char buf[SIZE_OF_SUPER];
	int rc;

	memset(buf, 0, SIZE_OF_SUPER);
	memcpy(buf, sb, sizeof (*sb));

	/* swap if on big endian machine */
	ujfs_swap_superblock((struct superblock *) buf);

	rc = ujfs_rw_diskblocks(fp, (is_primary ? SUPER1_OFF : SUPER2_OFF),
				SIZE_OF_SUPER, buf, PUT);

	return rc;
}

/*
 * NAME: ujfs_get_superblk
 *
 * FUNCTION: read either the primary or secondary superblock from the specified
 *      device
 *
 * PRE CONDITIONS:
 *
 * POST CONDITIONS:
 *
 * PARAMETERS:
 *      fp              - open port for device to read superblock from
 *      sb              - pointer to struct superblock to be filled in on return
 *      is_primary      - 0 indicates to retrieve secondary superblock,
 *                        otherwise retrieve primary superblock
 *
 * NOTES:
 *
 * RECOVERY OPERATION:
 *
 * DATA STRUCTURES:
 *
 * RETURNS:
 *      success: 0
 *      failure: any other value
 */
int ujfs_get_superblk(FILE *fp, struct superblock *sb, int32_t is_primary)
{
	int rc;
	char buf[SIZE_OF_SUPER];
	struct superblock *sblk = (struct superblock *) buf;

	rc = ujfs_rw_diskblocks(fp, (is_primary ? SUPER1_OFF : SUPER2_OFF),
				SIZE_OF_SUPER, sblk, GET);

	ujfs_swap_superblock(sblk);

	if (rc != 0)
		return rc;

	memcpy(sb, sblk, sizeof (*sb));
	return 0;
}

/*
 * NAME: ujfs_validate_logsuper
 *
 * FUNCTION: Check if log superblock is valid
 *
 * PRE CONDITIONS:
 *
 * POST CONDITIONS:
 *
 * PARAMETERS:
 *      logsup - log superblock to validate
 *
 * NOTES:
 *
 * RECOVERY OPERATION:
 *
 * DATA STRUCTURES:
 *
 * RETURNS:
 *      success: 0
 *      failure: LIBFS_BADVERSION, LIBFS_BADMAGIC
 */
int ujfs_validate_logsuper(struct logsuper *logsup)
{
	/* see if superblock is JFS log superblock */
	if (logsup->magic == LOGMAGIC) {
		if (logsup->version != LOGVERSION) {
			return LIBFS_BADVERSION;
		}
	} else {
		return LIBFS_BADMAGIC;
	}

	return 0;
}

/*
 * NAME: ujfs_put_logsuper
 *
 * FUNCTION: Write log superblock to disk
 *
 * PRE CONDITIONS:
 *
 * POST CONDITIONS:
 *
 * PARAMETERS:
 *      fp      - open port for device to write superblock to
 *      logsup  - pointer to log superblock to be written
 *
 * NOTES:
 *
 * RECOVERY OPERATION:
 *
 * DATA STRUCTURES:
 *
 * RETURNS:
 *      success: 0
 *      failure: any other value
 */
int ujfs_put_logsuper(FILE *fp, struct logsuper *logsup)
{
	char buf[sizeof (struct logsuper)];
	int rc = 0;

	memcpy(buf, logsup, sizeof (struct logsuper));

	/* swap if on big endian machine */
	ujfs_swap_logsuper((struct logsuper *) buf);

	rc = ujfs_rw_diskblocks(fp, LOGPSIZE, sizeof (struct logsuper), buf, PUT);

	return rc;
}

/*
 * NAME: ujfs_get_logsuper
 *
 * FUNCTION: read the log superblock from the specified device
 *
 * PRE CONDITIONS:
 *
 * POST CONDITIONS:
 *
 * PARAMETERS:
 *      fp      - open port for device to read superblock from
 *      logsup  - pointer to log superblock to be filled in on return
 *
 * NOTES:
 *
 * RECOVERY OPERATION:
 *
 * DATA STRUCTURES:
 *
 * RETURNS:
 *      success: 0
 *      failure: any other value
 */
int ujfs_get_logsuper(FILE *fp, struct logsuper *logsup)
{
	int rc = 0;
	char buf[sizeof (struct logsuper)];
	struct logsuper *logsup_buf = (struct logsuper *) buf;

	rc = ujfs_rw_diskblocks(fp, LOGPSIZE, sizeof (struct logsuper), logsup_buf, GET);

	if (!rc) {
		ujfs_swap_logsuper(logsup_buf);
		memcpy(logsup, logsup_buf, sizeof (struct logsuper));
	}

	return rc;
}
