/*
 *   Copyright (C) International Business Machines Corp., 2000-2009
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
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#if HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif
#include <jfs_types.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include "jfs_endian.h"
#include "jfs_filsys.h"
#include "jfs_dinode.h"
#include "jfs_superblock.h"
#include "initmap.h"
#include "inodes.h"
#include "inode.h"
#include "inodemap.h"
#include "super.h"
#include "logform.h"
#include "jfs_dmap.h"
#include "message.h"
#include "debug.h"
#include "jfs_version.h"
#include "utilsubs.h"

unsigned type_jfs;
char *program_name;

extern int LogOpenMode;
FILE *open_by_label(uuid_t, int, int, char *, int *);

static int AGsize;

/* Define a parameter array for messages */
#define MAXPARMS        9
#define MAXSTR          128
char *msg_parms[MAXPARMS];
char msgstr[MAXSTR];

#define L2MEGABYTE      20
#define MEGABYTE        (1 << L2MEGABYTE)
#define MAX_LOG_PERCENTAGE  10	/* Log can be at most 10% of disk */
#define create_journal_only  1
#define use_existing_journal 2

/*
 * The following macro defines the initial number of aggregate inodes which
 * are initialized when a new aggregate is created.  This does not include
 * any fileset inodes as those are initialized separately.
 */
#define INIT_NUM_AGGR_INODES    (BADBLOCK_I + 1)

static struct dinode aggr_inodes[INIT_NUM_AGGR_INODES];

void mkfs_usage(void)
{
	printf("\nUsage:  %s [-cOqV] [-j log_device] [-J options] "
	       "[-L vol_label] [-s log_size] device [ blocks ]\n",
	       program_name);
	printf("\nEmergency help:\n"
	       " -c            Check device for bad blocks before building file system.\n"
	       " -O            Provide case-insensitive support for OS/2 compatability.\n"
	       " -q            Quiet execution.\n"
	       " -V            Print version information only.\n"
	       " -j log_device Set external journal device.\n"
	       " -J options    Set external journal options.\n"
	       " -L vol_label  Set volume label for the file system.\n"
	       " -s log_size   Set log size (in megabytes).\n\n"
	       "NOTE: -j and -J cannot be used at the same time.\n");
	return;
}

/*--------------------------------------------------------------------
 * NAME: create_fileset
 *
 * FUNCTION: Do all work to create a fileset in an aggregate
 *
 * PARAMETERS:
 *      dev_ptr         - open port of device to write to
 *      aggr_block_size - block size for aggregate
 *      start_block     - Number of blocks used by aggregate metadata, indicates
 *                        first block available for writing fileset metadata.
 *                        All fileset writes will be offsets from this.
 *      inostamp        - Inode stamp value to be used.
 *
 * RETURNS:
 *      success: 0
 *      failure: any other value
 */
static int create_fileset(FILE *dev_ptr, int aggr_block_size,
			  int64_t start_block, unsigned inostamp)
{
	int rc;
	int64_t inode_table_loc, inode_map_loc;
	int inode_table_size, inode_map_size;

	/*
	 * Find space for the inode allocation map page
	 *
	 * Also find the fileset inode map location on disk (inode_map_loc).
	 * We need to know this in order to initialize the fileset inodes
	 * with the proper iag value.
	 *
	 * Since we only have one fileset per aggregate in the first release,
	 * we always know where the inode map will start.  Therefore, currently
	 * we use a hard-coded value.  When we add multiple filesets per
	 * aggregate this will need to be modified to find the space for the
	 * inode map by looking in the block allocation map for available space.
	 */
	inode_map_size = SIZE_OF_MAP_PAGE << 1;
#ifdef ONE_FILESET_PER_AGGR
	/*
	 * The first extent of the fileset inode allocation map follows the
	 * first extent of the first extent of the fileset inodes at the
	 * beginning of the fileset
	 */
	inode_map_loc = start_block + INODE_EXTENT_SIZE / aggr_block_size;
#else
	inode_map_loc = get_space(inode_map_size);
#endif

	/*
	 * Allocate Aggregate Inodes for Fileset
	 */
	rc = init_fileset_inodes(aggr_block_size, dev_ptr, inode_map_loc,
				 inode_map_size, start_block, inostamp);
	if (rc != 0)
		return (rc);

	/*
	 * Create Fileset Inode Table - first extent
	 */
	rc = init_fileset_inode_table(aggr_block_size, dev_ptr,
				      &inode_table_loc, &inode_table_size,
				      start_block, inode_map_loc, inostamp);
	if (rc != 0)
		return (rc);

	/*
	 * Create Fileset Inode Allocation Map - first extent
	 */
	rc = init_inode_map(aggr_block_size, dev_ptr, inode_table_loc,
			    inode_table_size, inode_map_loc, inode_map_size,
			    (ACL_I + 1), AGsize, FILESYSTEM_I);

	return rc;
}

/*--------------------------------------------------------------------
 * NAME: create_aggregate
 *
 * FUNCTION: Do all work to create an aggregate
 *
 * PARAMETERS:
 *      dev_ptr                 - open port of device to write to
 *      volume_label            - label for volume
 *      number_of_blocks        - number of blocks for aggregate
 *      aggr_block_size         - block size for aggregate
 *      phys_block_size         - physical block size of device
 *      type_jfs                - JFS type to create
 *      verify_blocks           - indicates if we should verify every block
 *      log_uuid                - uuid of log device
 *
 * NOTES: The superblocks are the last things written to disk.  In the event
 *        of a system crash during mkfs this device will not appear to have
 *        a real JFS filesystem.  This should prevent us from attempting to
 *        mount a partially initialized filesystem.
 *
 * RETURNS:
 *      success: 0
 *      failure: any other value
 */
static int create_aggregate(FILE *dev_ptr,
			    char *volume_label,
			    int64_t number_of_blocks,
			    int aggr_block_size,
			    int phys_block_size,
			    unsigned type_jfs,
			    char *logdev,
			    int64_t logloc,
			    int logsize,
			    bool verify_blocks,
			    uuid_t log_uuid)
{
	struct superblock aggr_superblock;
	void *buffer;
	int rc;
	int64_t index;
	int64_t first_block, last_block;
	int64_t reserved_size;
	int64_t aggr_inode_map_loc;
	int aggr_inode_map_sz;
	xad_t inode_map_dscr;
	int64_t secondary_ait_address, secondary_aimap_address;
	int64_t secondary_ait_end;
	int64_t fsck_wspace_address, num_bits;
	int64_t fsck_wspace_length, fsck_svclog_length, npages;
	unsigned inostamp;
	struct dinode fileset_inode;

	/*
	 * Find where fsck working space will live on disk and mark those
	 * blocks.  The fsck working space is always at the very end of the
	 * aggregate so once we know how big it is we can back up from the
	 * end to determine where it needs to start.
	 *
	 * Need enough 4k pages to cover:
	 *  - 1 bit per block in aggregate rounded up to BPERDMAP boundary
	 *  - 1 extra 4k page to handle control page, intermediate level pages
	 *  - 50 extra 4k pages for the chkdsk service log
	 */
	num_bits =
	    ((number_of_blocks + BPERDMAP - 1) >> L2BPERDMAP) << L2BPERDMAP;
	npages = ((num_bits + (BITSPERPAGE - 1)) / BITSPERPAGE) + 1 + 50;
	fsck_wspace_length = (npages << L2PSIZE) / aggr_block_size;
	fsck_wspace_address = number_of_blocks - fsck_wspace_length;
	fsck_svclog_length = (50 << L2PSIZE) / aggr_block_size;

	/*
	 * Now we want the fsck working space to be ignored as actually being
	 * part of the filesystem
	 */
	number_of_blocks -= fsck_wspace_length;

	/*
	 * Initialize disk block map, so blocks can be marked as they are used
	 * Blocks used for fsck working space will be marked here since we
	 * don't want those blocks to be accounted for when maxag is set
	 */
	inostamp = (unsigned) time(NULL);
	rc = calc_map_size(number_of_blocks, aggr_inodes, aggr_block_size,
			   &AGsize, inostamp);
	if (rc != 0)
		return rc;

	/*
	 * Initialize and clear reserved disk blocks
	 */
	reserved_size = AGGR_RSVD_BYTES;
	buffer = calloc(reserved_size, sizeof (char));
	if (buffer == NULL) {
		message_user(MSG_OSO_INSUFF_MEMORY, NULL, 0, OSO_MSG);
		return (ENOMEM);
	}
	rc = ujfs_rw_diskblocks(dev_ptr, 0, reserved_size, buffer, PUT);
	if (rc != 0)
		return rc;
	for (index = 0;
	     ((index < reserved_size / aggr_block_size) && (rc == 0));
	     index++) {
		rc = markit(index, ALLOC);
	}
	if (rc != 0)
		return rc;

	/*
	 * In case mkfs does not complete, but we have an old superblock
	 * already on this device, we will zero the superblock disk blocks at
	 * the beginning and then write the real superblock to disk last.
	 * (This keeps the device from appearing to have a complete filesystem
	 * when initialization is not complete.)
	 */
	rc = ujfs_rw_diskblocks(dev_ptr, SUPER1_OFF, SIZE_OF_SUPER, buffer,
				PUT);
	if (rc != 0) {
		free(buffer);
		return rc;
	}
	rc = ujfs_rw_diskblocks(dev_ptr, SUPER2_OFF, SIZE_OF_SUPER, buffer,
				PUT);
	if (rc != 0) {
		free(buffer);
		return rc;
	}
	free(buffer);

	/* Mark blocks allocated for superblocks. */
	first_block = SUPER1_OFF / aggr_block_size;
	last_block = first_block + (SIZE_OF_SUPER / aggr_block_size);
	for (index = first_block; ((index < last_block) && (rc == 0)); index++) {
		rc = markit(index, ALLOC);
	}
	if (rc != 0)
		return rc;

	first_block = SUPER2_OFF / aggr_block_size;
	last_block = first_block + (SIZE_OF_SUPER / aggr_block_size);
	for (index = first_block; ((index < last_block) && (rc == 0)); index++) {
		rc = markit(index, ALLOC);
	}
	if (rc != 0)
		return rc;

	/*
	 * Initialize First Extent of Aggregate Inode Allocation Map
	 */
	aggr_inode_map_loc = AIMAP_OFF;
	aggr_inode_map_sz = SIZE_OF_MAP_PAGE << 1;
	rc = init_inode_map(aggr_block_size, dev_ptr, AITBL_OFF,
			    INODE_EXTENT_SIZE,
			    aggr_inode_map_loc / aggr_block_size,
			    aggr_inode_map_sz, INIT_NUM_AGGR_INODES + 1, AGsize,
			    AGGREGATE_I);
	if (rc != 0)
		return rc;

	/*
	 * Initialize first inode extent of Aggregate Inode Table
	 */
	rc = init_aggr_inode_table(aggr_block_size, dev_ptr, aggr_inodes,
				   INIT_NUM_AGGR_INODES, AITBL_OFF,
				   aggr_inode_map_loc / aggr_block_size,
				   aggr_inode_map_sz, inostamp);
	if (rc != 0)
		return rc;

	/*
	 * Now initialize the secondary aggregate inode table and map
	 *
	 * We can use the same aggr_inodes we already initialized except for
	 * the aggregate self inode.  This will be updated by the call to
	 * init_aggr_inode_table() to point to the secondary table instead of
	 * the primary table.
	 *
	 * First we need to determine the location; it will follow the block
	 * map.  Since the block map might be sparse we need to use the number
	 * of blocks instead of the length of the extents.  This works since
	 * the extents are in the inode for mkfs
	 */
	inode_map_dscr =
	    ((xtpage_t *) & (aggr_inodes[BMAP_I].di_DASD))->xad[XTENTRYSTART];
	secondary_aimap_address =
	    addressXAD(&inode_map_dscr) + aggr_inodes[BMAP_I].di_nblocks;
	secondary_ait_address =
	    (secondary_aimap_address * aggr_block_size) +
	    (SIZE_OF_MAP_PAGE << 1);
	secondary_ait_end =
	    (secondary_ait_address + INODE_EXTENT_SIZE) / aggr_block_size;

	rc = init_inode_map(aggr_block_size, dev_ptr, secondary_ait_address,
			    INODE_EXTENT_SIZE, secondary_aimap_address,
			    aggr_inode_map_sz, INIT_NUM_AGGR_INODES + 1, AGsize,
			    AGGREGATE_I);
	if (rc != 0)
		return rc;

	/*
	 * Modify the aggregate inodes ixpxd fields
	 */
	PXDaddress(&(aggr_inodes[BMAP_I].di_ixpxd),
		   secondary_ait_address / aggr_block_size);
	rc = init_aggr_inode_table(aggr_block_size, dev_ptr, aggr_inodes,
				   INIT_NUM_AGGR_INODES, secondary_ait_address,
				   secondary_aimap_address, aggr_inode_map_sz,
				   inostamp);
	if (rc != 0)
		return rc;

	/*
	 * Mark blocks for the block map
	 */
	first_block = BMAP_OFF / aggr_block_size;
	last_block = first_block + aggr_inodes[BMAP_I].di_nblocks;
	for (index = first_block; ((index < last_block) && (rc == 0)); index++) {
		rc = markit(index, ALLOC);
	}
	if (rc != 0)
		return rc;

	/*
	 * Now we will create a fileset as necessary.
	 *
	 * Determine the end of the metadata written for the aggregate to tell
	 * where to put the fileset to be created.  Since the secondary
	 * aggregate inode table is the last thing written to the aggregate,
	 * the fileset will start following it.
	 */
	rc = create_fileset(dev_ptr, aggr_block_size, secondary_ait_end,
			    inostamp);
	if (rc != 0)
		return rc;

	/*
	 * Copy the fileset inode to the secondary aggregate inode table
	 */
	rc = ujfs_rwinode(dev_ptr, &fileset_inode, FILESYSTEM_I, GET,
			  aggr_block_size, AGGREGATE_I, type_jfs);
	if (rc != 0)
		return rc;

	PXDaddress(&(fileset_inode.di_ixpxd),
		   secondary_ait_address / aggr_block_size);

	/* swap if on big endian machine */
	ujfs_swap_dinode(&fileset_inode, PUT, type_jfs);
	rc = ujfs_rw_diskblocks(dev_ptr,
				secondary_ait_address +
				FILESYSTEM_I * sizeof (struct dinode),
				sizeof (fileset_inode), &fileset_inode, PUT);
	ujfs_swap_dinode(&fileset_inode, GET, type_jfs);

	if (rc != 0)
		return rc;

	/*
	 * If we are supposed to verify all blocks, now is the time to do it
	 *
	 * First we tell the LVM to stop doing Bad Block Relocation so we can
	 * catch (and record) any bad blocks ourselves.  Next we run through
	 * the available file system space looking for bad blocks.  Finally
	 * we tell the LVM to go back to doing Bad Block Relocation.
	 */
	PXDaddress(&(aggr_inodes[BADBLOCK_I].di_ixpxd),
		   AITBL_OFF / aggr_block_size);
	if (verify_blocks == true) {
		rc = verify_last_blocks(dev_ptr, aggr_block_size,
					&(aggr_inodes[BADBLOCK_I]));
		if (rc != 0)
			return rc;
	}

	/*
	 * Copy the bad block inode to the secondary aggregate inode table
	 */
	PXDaddress(&(aggr_inodes[BADBLOCK_I].di_ixpxd),
		   secondary_ait_address / aggr_block_size);

	/* swap if on big endian machine */
	ujfs_swap_dinode(&aggr_inodes[BADBLOCK_I], PUT, type_jfs);
	rc = ujfs_rw_diskblocks(dev_ptr,
				secondary_ait_address +
				BADBLOCK_I * sizeof (struct dinode),
				sizeof (struct dinode),
				&(aggr_inodes[BADBLOCK_I]), PUT);
	ujfs_swap_dinode(&aggr_inodes[BADBLOCK_I], GET, type_jfs);

	if (rc != 0)
		return rc;

	/*
	 * Now our block allocation map should be complete, write to disk
	 */
	rc = write_block_map(dev_ptr, number_of_blocks, aggr_block_size);
	if (rc != 0)
		return rc;

	/*
	 * Initialize Aggregate Superblock - Both primary and secondary
	 */
	memcpy(aggr_superblock.s_magic, JFS_MAGIC, strlen(JFS_MAGIC));
	/*
	 * JFS_VERSION should have been upped to 2 when the linux-native format
	 * split from OS/2.  However, we don't want to force the use of the
	 * latest JFS kernel code unless we're using an outline log.  In that
	 * case we want to set s_version to 2 to mandate a recent JFS driver.
	 */
	aggr_superblock.s_version =
	    (type_jfs & JFS_INLINELOG) ? 1 : JFS_VERSION;
	aggr_superblock.s_size =
	    number_of_blocks * (aggr_block_size / phys_block_size);
	aggr_superblock.s_bsize = aggr_block_size;
	aggr_superblock.s_l2bsize = log2shift(aggr_block_size);
	aggr_superblock.s_l2bfactor =
	    log2shift(aggr_block_size / phys_block_size);
	aggr_superblock.s_pbsize = phys_block_size;
	aggr_superblock.s_l2pbsize = log2shift(phys_block_size);
	aggr_superblock.pad = 0;
	aggr_superblock.s_agsize = AGsize;
	aggr_superblock.s_flag = type_jfs;
	aggr_superblock.s_state = FM_CLEAN;
	aggr_superblock.s_compress = 0;
	PXDaddress(&aggr_superblock.s_ait2,
		   secondary_ait_address / aggr_block_size);
	PXDlength(&aggr_superblock.s_ait2, INODE_EXTENT_SIZE / aggr_block_size);
	PXDaddress(&aggr_superblock.s_aim2, secondary_aimap_address);
	PXDlength(&aggr_superblock.s_aim2, aggr_inode_map_sz / aggr_block_size);

	if (logdev[0]) {
		struct stat st;

		if (stat(logdev, &st))
			return errno;

		aggr_superblock.s_logdev = st.st_rdev;
		memset(&aggr_superblock.s_logpxd, 0, sizeof (pxd_t));
		uuid_copy(aggr_superblock.s_loguuid, log_uuid);
	} else {
		aggr_superblock.s_logdev = 0;
		PXDaddress(&aggr_superblock.s_logpxd, logloc);
		PXDlength(&aggr_superblock.s_logpxd, logsize);
	}

	aggr_superblock.s_logserial = 0;
	PXDaddress(&aggr_superblock.s_fsckpxd, fsck_wspace_address);
	PXDlength(&aggr_superblock.s_fsckpxd, fsck_wspace_length);
	aggr_superblock.s_time.tv_sec = time(NULL);
	aggr_superblock.s_time.tv_nsec = 0;
	aggr_superblock.s_fsckloglen = fsck_svclog_length;
	aggr_superblock.s_fscklog = 0;
	strncpy(aggr_superblock.s_fpack, volume_label, LV_NAME_SIZE);

	/* extendfs stuff */
	aggr_superblock.s_xsize = 0;
	memset(&aggr_superblock.s_xfsckpxd, 0, sizeof (pxd_t));
	memset(&aggr_superblock.s_xlogpxd, 0, sizeof (pxd_t));

	uuid_generate(aggr_superblock.s_uuid);
	strncpy(aggr_superblock.s_label, volume_label, 16);

	/* TODO: store log uuid */

	/* Write both the primary and secondary superblocks to disk */
	rc = ujfs_validate_super(&aggr_superblock);
	if (rc)
		return rc;

	rc = ujfs_put_superblk(dev_ptr, &aggr_superblock, 1);
	if (rc)
		return rc;

	rc = ujfs_put_superblk(dev_ptr, &aggr_superblock, 0);
	return rc;
}

/*--------------------------------------------------------------------
 * NAME: parse_journal_opts
 *
 * FUNCTION: parse journal (-J) options
 *               set log device name (global logdev)
 *               set appropriate external journal flag
 *
 * PARAMETERS:
 *      opts - options string
 *      ext_journal_opt - external journal flag
 *      journal_device - external journal device name
 */
void parse_journal_opts(const char *opts, int *ext_journal_opt,
			char *journal_device)
{
	int journal_usage = 0;
	FILE *log_fd = NULL;
	uuid_t log_uuid;
	int in_use;

	/*
	 * -J device= means we're going to attach an existing
	 * external journal to a newly formatted file system
	 */
	if (strncmp(opts, "device=", 7) == 0) {
		LogOpenMode = O_RDONLY;
		/* see if device is specified by UUID */
		if (strncmp(opts + 7, "UUID=", 5) == 0) {
			if (uuid_parse((char *) opts + 7 + 5, log_uuid)) {
				fprintf(stderr,
					"\nError: UUID entered in improper format.\n");
				exit(-1);
			} else {
				log_fd =
				    open_by_label(log_uuid, 0, 1,
						  journal_device, &in_use);
				/*
				 * If successful, open_by_label returns a file
				 * descriptor to an open file.  For jfs_mkfs
				 * purposes, we do not yet need that file to be
				 * open, so close it here.
				 */
				if (log_fd == NULL) {
					fclose(log_fd);
				} else {
					fprintf(stderr,
						"\nError: Could not find/open device specified by UUID %s.\n",
						(char *) opts + 7 + 5);
					exit(-1);
				}
			}
			/* see if device is specified by volume label */
		} else if (strncmp(opts + 7, "LABEL=", 6) == 0) {
			log_fd =
			    open_by_label((char *) opts + 7 + 6, 1, 1,
					  journal_device, &in_use);
			/*
			 * If successful, open_by_label returns a file descriptor
			 * to an open file.  For jfs_mkfs purposes, we do not yet
			 * need that file to be open, so close it here.
			 */
			if (log_fd == NULL) {
				fclose(log_fd);
			} else {
				fprintf(stderr,
					"\nError: Could not find/open device specified by LABEL %s.\n",
					(char *) opts + 7 + 6);
				exit(-1);
			}
			/* device is specified by device name */
		} else {
			strcpy(journal_device, ((char *) opts + 7));
		}

		if (journal_device) {
			*ext_journal_opt = use_existing_journal;
		} else {
			journal_usage++;
		}
		/*
		 * -J journal_dev means we're going to
		 * format a new external journal ONLY
		 */
	} else if (strncmp(opts, "journal_dev", 11) == 0) {
		*ext_journal_opt = create_journal_only;
	} else
		/* error in specified options */
		journal_usage++;

	if (journal_usage) {
		fprintf(stderr, "\nInvalid journal options specified.\n\n"
			"Valid options for -J are:\n"
			"\tdevice=<journal device>\n"
			"\tdevice=UUID=<UUID of journal device>\n"
			"\tdevice=LABEL=<label of journal device>\n"
			"\tjournal_dev\n\n");
		exit(1);
	}

	return;
}

/*--------------------------------------------------------------------
 * NAME: format
 *
 * FUNCTION:            format the specified partition as a JFS file system.
 *
 * PARAMETERS:
 *
 * RETURNS:
 *      success: 0
 *      failure: any other value
 */
int main(int argc, char *argv[])
{
	int c;
	int l2absize;
	char *device_name = NULL;
	int rc = 0;
        char volume_label[16] = { "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" };
	int64_t number_of_bytes = 0, bytes_on_device;
	int64_t number_of_blocks = 0, logloc = 0, logsize_in_bytes = 0;
	int64_t requested_blocks = 0;
	char logdev[255] = { '\0' };	/* Need to use a macro for this size */
	int aggr_block_size = PSIZE;	/* 4096 */
	int phys_block_size = PBSIZE, logsize = 0;
	FILE *dev_handle = NULL;
	unsigned type_commit = JFS_GROUPCOMMIT;
	bool verify_blocks = false;
	bool no_questions_asked = false;
	uuid_t log_uuid;
	int ext_journal_opt = 0;
	bool j_selected = false;
	char *label_ptr = NULL;

	if (argc && **argv)
		program_name = *argv;
	else
		program_name = "jfs_mkfs";

	printf("%s version %s, %s\n", program_name, VERSION, JFSUTILS_DATE);

	type_jfs = JFS_LINUX | JFS_DIR_INDEX;

	/*
	 * Parse command line arguments
	 */

	while ((c = getopt(argc, argv, "cj:J:fL:Oqs:V")) != EOF) {
		switch (c) {
#ifdef BLOCK_SIZE		/* add b: to getopt call */
		case 'b':
			/* block size */
			aggr_block_size = strtol(optarg, NULL, 0);
			break;
#endif
		case 'c':
			/* check for bad blocks */
			verify_blocks = true;
			break;
		case 'j':
			/* external journal device */
			if (j_selected) {
				mkfs_usage();
				return EINVAL;
			} else {
				strcpy(logdev, optarg);
				j_selected = true;
			}
			break;
		case 'J':
			/* external journal device */
			if (j_selected) {
				mkfs_usage();
				return EINVAL;
			} else {
				parse_journal_opts(optarg, &ext_journal_opt,
						   logdev);
				j_selected = true;
			}
			break;
		case 'L':
			/* volume label */
			strncpy(volume_label, optarg, 16);
			break;
		case 'O':
			/* case insensitive for OS/2 compatibility */
			type_jfs |= JFS_OS2;
			type_jfs &= ~(JFS_LINUX | JFS_DIR_INDEX);
			break;
		case 'f':
		case 'q':
			/* quiet, don't ask questions */
			no_questions_asked = true;
			break;
		case 's':
			/* log size */
			logsize = strtol(optarg, NULL, 0);
			if (!logsize) {
				printf("\nLog size not specified for -s.\n");
				mkfs_usage();
				return EINVAL;
			}
			logsize_in_bytes = ((int64_t) logsize) * MEGABYTE;
			break;
		case 'V':
			/* print version and exit */
			exit(0);
			break;
		default:
			mkfs_usage();
			return EINVAL;
		}
	}

	if ((optind < argc - 2) || (optind > argc - 1)) {
		printf
		    ("\nError: Device not specified or command format error\n");
		mkfs_usage();
		return EINVAL;
	}

	device_name = argv[optind];

	/*
	 * If we are only creating an external journal, set the
	 * log device name for later formatting of the log device
	 */
	if (ext_journal_opt == create_journal_only) {
		strcpy(logdev, argv[optind]);
		goto format_journal;
	}

	optind++;
	if (optind < argc)
		requested_blocks = strtoll(argv[optind], (char **)NULL, 10);

	/* Check for block device */
	{
		struct stat stat_data;
		int rc;

		rc = stat(device_name, &stat_data);
		if (rc != 0) {
			message_user(MSG_OSO_CANT_FIND_DRIVE, NULL, 0, OSO_MSG);
			return EINVAL;
		}
		/* Do we have a block special device or regular file? */
		if (ujfs_device_is_valid(NULL, &stat_data))
		{
			msg_parms[0] = device_name;
			message_user(MSG_OSO_NOT_VALID_BLOCK_DEVICE,
				     msg_parms, 1, OSO_MSG);
			return EINVAL;
		}
	}

	/* Is the device mounted?  We will NOT format a mounted device! */
	rc = Is_Device_Mounted(device_name);
	if (rc) {
		message_user(rc, NULL, 0, JFS_MSG);
		return ERROR_INVALID_ACCESS;
	}

	/*
	 * Open the device and lock it from all others
	 * Get the physical block size of the device.
	 */
	dev_handle = fopen_excl(device_name, "r+");
	if (dev_handle == NULL) {
		printf("\nError: Cannot open device %s.\n", device_name);
		return (ERROR_FILE_NOT_FOUND);
	}

	if (ujfs_device_is_valid(dev_handle, NULL) != 0) {
		message_user(MSG_OSO_DISK_LOCKED, NULL, 0, OSO_MSG);
		return (ERROR_FILE_NOT_FOUND);
	}

	if (aggr_block_size < phys_block_size) {
		/*
		 * Make sure the aggr_block_size is not smaller than the
		 * logical volume block size.
		 */
		sprintf(msgstr, "%d", aggr_block_size);
		msg_parms[0] = msgstr;
		msg_parms[1] = "BS";
		message_user(MSG_OSO_VALUE_NOT_ALLOWED, msg_parms, 2, OSO_MSG);
		rc = EINVAL;
		fclose(dev_handle);
		return (rc);
	} else {
		/*
		 * Validate user specified aggregate block size
		 */
		if (!inrange(aggr_block_size, MINBLOCKSIZE, MAXBLOCKSIZE)) {
			sprintf(msgstr, "%d", aggr_block_size);
			msg_parms[0] = msgstr;
			msg_parms[1] = "BS";
			message_user(MSG_OSO_VALUE_NOT_ALLOWED, msg_parms, 2,
				     OSO_MSG);
			rc = EINVAL;
			fclose(dev_handle);
			return (rc);
		}
	}

	/*
	 * get size of the logical volume
	 */
	rc = ujfs_get_dev_size(dev_handle, &bytes_on_device);
	if (rc != 0) {
		DBG_ERROR(("ujfs_get_dev_size: FAILED rc = %lld\n", rc))
		    message_user(MSG_OSO_FORMAT_FAILED, NULL, 0, OSO_MSG);
		fclose(dev_handle);
		return (rc);
	}

	number_of_bytes = bytes_on_device;
	DBG_TRACE(("ujfs_get_dev_size: size = %lld\n", number_of_bytes))

	    /*
	     * Make sure we have at least MINJFS for our file system
	     * Notes: The operating system takes some of the bytes from the
	     * partition to use for its own information.  The end user is not
	     * aware of this space, so we want to compare a posted minimum
	     * size to the actual size of the partition, not just the space
	     * available for our use.
	     */
	    if (number_of_bytes < MINJFS) {
		if (number_of_bytes == 0) {
			/* Not readable at all */
			message_user(MSG_JFS_BAD_PART, msg_parms, 0, JFS_MSG);
		} else {
			/* Really too small */
			strcpy(msgstr, MINJFSTEXT);
			msg_parms[0] = msgstr;
			message_user(MSG_JFS_PART_SMALL, msg_parms, 1, JFS_MSG);
		}
		rc = EINVAL;
		fclose(dev_handle);
		return (rc);
	}

	/*
	 * Size of filesystem in terms of block size
	 */
	number_of_blocks = number_of_bytes / aggr_block_size;

	if (requested_blocks) {
		if (requested_blocks > number_of_blocks) {
			printf("Requested blocks exceed number of blocks on device: %lld.\n",
			       number_of_blocks);
			fclose(dev_handle);
			return EINVAL;
		} else if (requested_blocks < (MINJFS / aggr_block_size)) {
			strcpy(msgstr, MINJFSTEXT);
			msg_parms[0] = msgstr;
			message_user(MSG_JFS_PART_SMALL, msg_parms, 1, JFS_MSG);
			fclose(dev_handle);
			return EINVAL;
		}
		number_of_blocks = requested_blocks;
	}
	DBG_TRACE(("number of blocks = %lld\n", number_of_blocks))

	/* now ask the user if he really wants to destroy his data */
	if (no_questions_asked == false) {
		if (ext_journal_opt != create_journal_only) {
			msg_parms[0] = device_name;
			message_user(MSG_OSO_DESTROY_DATA, msg_parms, 1,
				     OSO_MSG);
		}
		if (logdev[0] && (ext_journal_opt != use_existing_journal)) {
			msg_parms[0] = logdev;
			message_user(MSG_OSO_DESTROY_DATA, msg_parms, 1,
				     OSO_MSG);
		}
		do {
			printf("\nContinue? (Y/N) ");
			c = getchar();

			if (c == 'n' || c == 'N') {
				fclose(dev_handle);
				return 0;
			} else if (c == EOF)
				abort();
		} while (c != 'y' && c != 'Y');
	}

	/*
	 * Create journal log for aggregate to use
	 *
	 * For the prototype we will only create a journal log if one was
	 * specified on the command line.  Eventually we will always need one
	 * and we will need a default method of finding and creating the log.
	 */
	if (logdev[0] != '\0') {
		FILE *fp;

format_journal:
		/* A log device was specified on the command line.  Call
		   * jfs_logform() to initialize the log */
		fp = fopen_excl(logdev, "r+");
		if (fp == NULL) {
			message_user(MSG_OSO_FORMAT_FAILED, NULL, 0, OSO_MSG);
			fclose(dev_handle);
			return -1;
		}
		/*
		 * If the user specified logging to an existing external
		 * journal device, find it and validate its superblock.
		 */
		if (ext_journal_opt == use_existing_journal) {
			struct logsuper logsup;
			rc = ujfs_get_logsuper(fp, &logsup);
			if (!rc) {
				/* see if superblock is JFS log superblock */
				rc = ujfs_validate_logsuper(&logsup);
				if (!rc) {
					uuid_copy(log_uuid, logsup.uuid);
				} else {
					rc = -1;
					printf
					    ("\n%s does not contain a valid log superblock.\n"
					     "You may only attach a previously formatted \n"
					     "JFS external journal to a JFS file system.\n\n",
					     logdev);
				}
			} else {
				printf
				    ("\nError: Couldn't read log superblock from %s\n",
				     logdev);
			}
		} else {
			/* create new external journal device */
			uuid_clear(log_uuid);
			/*
			 * If a volume label has been specified and we're only
			 * creating an external journal, apply the label to the
			 * external journal.
			 */
			if ((ext_journal_opt == create_journal_only)
			    && volume_label) {
				label_ptr = volume_label;
			}
			rc = jfs_logform(fp, aggr_block_size,
					 log2shift(aggr_block_size),
					 type_jfs | type_commit, 0, 0, log_uuid,
					 label_ptr);
		}
		fclose(fp);
		if (rc != 0) {
			message_user(MSG_OSO_FORMAT_FAILED, NULL, 0, OSO_MSG);
			DBG_ERROR(("Internal error: Format failed rc=%x\n", rc))
			if (dev_handle)
				fclose(dev_handle);
			return (rc);
		} else if (ext_journal_opt == create_journal_only) {
			printf
			    ("\nJFS external journal created SUCCESSFULLY on %s.\n",
			     logdev);
			return (rc);
		}
	} else {
		type_jfs |= JFS_INLINELOG;
		if (logsize == 0) {
			/* If no size specified, let's default to .4 of
			 * aggregate size; Which for simplicity is
			 * 4/1024 == 2**2/2**10 == 1/2**8 == >> 8
			 *
			 * Round logsize up to a megabyte boundary */

			/* BYTES / 256 */
			logsize_in_bytes = number_of_bytes >> 8;
			/* round up to meg */
			logsize_in_bytes =
			    (logsize_in_bytes + MEGABYTE - 1) & ~(MEGABYTE - 1);
			/*
			 * jfs_logform enforces a 128 megabyte limit on an
			 * external journal.  Assuming that's reasonable,
			 * let's do the same for the internal journal.
			 */
			if (logsize_in_bytes > 128 * MEGABYTE)
				logsize_in_bytes = 128 * MEGABYTE;
		}

		/* Convert logsize into aggregate blocks */
		logsize = logsize_in_bytes / aggr_block_size;

		if (logsize >= (number_of_blocks / MAX_LOG_PERCENTAGE)) {
			message_user(MSG_JFS_LOG_LARGE, NULL, 0, JFS_MSG);
			rc = ENOSPC;
			message_user(MSG_OSO_FORMAT_FAILED, NULL, 0, OSO_MSG);
			DBG_ERROR(("Internal error: Format failed rc=%x\n", rc))
			fclose(dev_handle);
			return (rc);
		}

		logloc = number_of_blocks - logsize;
		number_of_blocks -= logsize;
		l2absize = log2shift(aggr_block_size);
		rc = jfs_logform(dev_handle, aggr_block_size, l2absize,
				 type_jfs | type_commit, logloc, logsize, NULL,
				 NULL);
		if (rc != 0) {
			message_user(MSG_OSO_FORMAT_FAILED, NULL, 0, OSO_MSG);
			DBG_ERROR(("Internal error: Format failed rc=%x\n", rc))
			fclose(dev_handle);
			return (rc);
		}
	}

	/*
	 * Create aggregate, which will also create a fileset as necessary
	 */
	rc = create_aggregate(dev_handle, volume_label, number_of_blocks,
			      aggr_block_size, phys_block_size,
			      type_jfs | type_commit, logdev, logloc, logsize,
			      verify_blocks, log_uuid);
	ujfs_flush_dev(dev_handle);
	fclose(dev_handle);
	/* Format Complete message */
	if (rc == 0) {
		message_user(MSG_OSO_FORMAT_COMPLETE, NULL, 0, OSO_MSG);
		sprintf(msgstr, "%lld", (long long) (number_of_bytes / 1024));
		msg_parms[0] = msgstr;	/* total disk space msg           */
		message_user(MSG_OSO_DISK_SPACE2, msg_parms, 1, OSO_MSG);

	}

	if (rc) {
		message_user(MSG_OSO_FORMAT_FAILED, NULL, 0, OSO_MSG);
		DBG_ERROR(("Internal error: Format failed rc=%x\n", rc))
	}

	return rc;
}
