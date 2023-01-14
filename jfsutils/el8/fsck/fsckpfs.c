/*
 *   Copyright (C) International Business Machines Corp., 2000-2004
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
#include <string.h>
#include <unistd.h>
/* defines and includes common among the fsck.jfs modules */
#include "xfsckint.h"
#include "xchkdsk.h"
#include "jfs_byteorder.h"
#include "devices.h"
#include "utilsubs.h"

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * superblock buffer pointer
 *
 *      defined in xchkdsk.c
 */
extern struct superblock *sb_ptr;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * fsck aggregate info structure pointer
 *
 *      defined in xchkdsk.c
 */
extern struct fsck_agg_record *agg_recptr;

/* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
 *
 * For message processing
 *
 *      defined in xchkdsk.c
 */
extern char *Vol_Label;

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV
 *
 * The following are internal to this file
 *
 */

int imapleaf_get(int64_t, xtpage_t **);

int open_device_read(const char *);

int open_device_rw(const char *);

uint32_t checksum(uint8_t *, uint32_t);

/* VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV */

/*****************************************************************************
 * NAME: ait_node_get
 *
 * FUNCTION:  Read the specified AIT xTree node into the specified buffer
 *
 * PARAMETERS:
 *      node_fsblk_offset  - input - offset, in aggregate blocks, into the
 *                                   aggregate, of the xTree node wanted
 *      xtpage_ptr            - input - pointer an fsck buffer into which the
 *                                   xTree node should be read.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int ait_node_get(int64_t node_fsblk_offset, xtpage_t * xtpage_ptr)
{
	int anodg_rc = FSCK_OK;
	int64_t node_start_byte;
	uint32_t bytes_read;

	node_start_byte = node_fsblk_offset * sb_ptr->s_bsize;

	if ((agg_recptr->ondev_wsp_fsblk_offset != 0)
	    && (node_fsblk_offset > agg_recptr->ondev_wsp_fsblk_offset)) {
		/* the offset is beyond the range valid for fileset objects */
		/*
		 * This case is not caused by an I/O error, but by
		 * invalid data in an inode.  Let the caller handle
		 * the consequences.
		 */
		anodg_rc = FSCK_BADREADTARGET2;
	} else {
		anodg_rc = readwrite_device(node_start_byte,
					    XTPAGE_SIZE,
					    &(bytes_read), (void *) xtpage_ptr,
					    fsck_READ);
		if (anodg_rc == FSCK_OK) {
			/* read appears successful */
			if (bytes_read < XTPAGE_SIZE) {
				/* didn't get the minimum number of bytes */
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVREAD,
					      fsck_ref_msg(fsck_metadata),
					      Vol_Label);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BADREAD_NODE1,
					      anodg_rc, fsck_READ, (long long) node_start_byte,
					      XTPAGE_SIZE, bytes_read);
				anodg_rc = FSCK_FAILED_BADREAD_NODE1;
			} else {
				/* swap if on big endian machine */
				ujfs_swap_xtpage_t(xtpage_ptr);
			}
		} else {
			/* bad return code from read */
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVREAD,
				      fsck_ref_msg(fsck_metadata), Vol_Label);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_READ_NODE, anodg_rc,
				      fsck_READ, (long long) node_start_byte,
				      XTPAGE_SIZE, bytes_read);
			anodg_rc = FSCK_FAILED_READ_NODE;
		}
	}

	return (anodg_rc);
}

/*****************************************************************************
 * NAME: ait_node_put
 *
 * FUNCTION:  Write the specified buffer into the specified AIT xTree node
 *
 * PARAMETERS:
 *      node_fsblk_offset  - input - offset, in aggregate blocks, to which
 *                                           the buffer is to be written
 *      xtpage_ptr         - input - pointer to the buffer to write
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int ait_node_put(int64_t node_fsblk_offset, xtpage_t * xtpage_ptr)
{
	int anodp_rc = FSCK_OK;
	int64_t node_start_byte;
	uint32_t bytes_written;

	node_start_byte = node_fsblk_offset * sb_ptr->s_bsize;

	ujfs_swap_xtpage_t(xtpage_ptr);
	anodp_rc = readwrite_device(node_start_byte, PSIZE,
				    &bytes_written, (void *) xtpage_ptr,
				    fsck_WRITE);
	ujfs_swap_xtpage_t(xtpage_ptr);

	if (anodp_rc == FSCK_OK) {
		if (bytes_written != PSIZE) {
			/* didn't write correct number of bytes */
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
				      Vol_Label, 2);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_BADWRITE_FBLKMP, anodp_rc,
				      fsck_WRITE, (long long) node_start_byte,
				      PSIZE, bytes_written);

			anodp_rc = FSCK_BADWRITE_FBLKMP;
		}
	} else {
		/*
		 * message to user
		 */
		fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
			      Vol_Label, 3);
		/*
		 * message to debugger
		 */
		fsck_send_msg(fsck_ERRONAGG, FSCK_BADWRITE_FBLKMP, anodp_rc,
			      fsck_WRITE, (long long) node_start_byte,
			      PSIZE, bytes_written);

		anodp_rc = FSCK_BADWRITE_FBLKMP;
	}			/* end else the write was not successful */

	return (anodp_rc);
}

/*****************************************************************************
 * NAME: ait_special_read_ext1
 *
 * FUNCTION:  Reads the first extent of either the Primary or Secondary
 *            Aggregate Inode Table into the fsck inode buffer.
 *
 * PARAMETERS:
 *      which_ait  - input - { fsck_primary | fsck_secondary }
 *
 * NOTES:  This routine is used during the early stages of fsck processing
 *         when the normal mechanisms for reading inodes have not yet been
 *         established.
 *
 *         This routine may also be used later in fsck processing as a fast
 *         read routine for the inodes in the first extent of the AIT.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int ait_special_read_ext1(int which_ait)
{
	int aree_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	int64_t offset_1stext;

	aree_rc = inodes_flush();

	/*
	 * calculate the byte offset of the first extent
	 */
	if ((which_ait == fsck_primary)) {
		offset_1stext = AITBL_OFF;
	} else {
		/* must be secondary */
		offset_1stext = addressPXD(&(sb_ptr->s_ait2)) * sb_ptr->s_bsize;
	}

	if (agg_recptr->ino_buf_agg_offset != offset_1stext) {
		/* we don't already have the one we want */
		intermed_rc = readwrite_device(offset_1stext,
					       INODE_IO_BUFSIZE,
					       &(agg_recptr->ino_buf_data_len),
					       (void *) agg_recptr->ino_buf_ptr,
					       fsck_READ);
		if (intermed_rc != FSCK_OK) {
			/* didn't get anything */
			aree_rc = FSCK_CANTREADAITEXT1;
			fsck_send_msg(fsck_CANTREADAITEXT1, fsck_ref_msg(which_ait));
		} else {
			/* got something */
			/* swap if on big endian machine */
			ujfs_swap_inoext((struct dinode *) agg_recptr->
					 ino_buf_ptr, GET, sb_ptr->s_flag);

			agg_recptr->ino_for_aggregate = -1;
			agg_recptr->ino_which_it = which_ait;
			agg_recptr->ino_buf_1st_ino = 0;
			agg_recptr->ino_fsnum = 0;
			agg_recptr->ino_buf_agg_offset = offset_1stext;
			PXDaddress(&(agg_recptr->ino_ixpxd),
				   offset_1stext / sb_ptr->s_bsize);
			PXDlength(&(agg_recptr->ino_ixpxd),
				  INODE_EXTENT_SIZE / sb_ptr->s_bsize);

			if (agg_recptr->ino_buf_data_len < INODE_EXTENT_SIZE) {
				/* didn't get enough */
				aree_rc = FSCK_CANTREADAITEXT1;
				fsck_send_msg(fsck_CANTREADEAITEXT1,
					      fsck_ref_msg(which_ait));
			}
		}
	}
	return (aree_rc);
}

/*****************************************************************************
 * NAME: blkmap_find_bit
 *
 * FUNCTION:  Calculate the position, in the fsck workspace block map,
 *            of the bit representing the given aggregate block.
 *
 * PARAMETERS:
 *      blk_number   - input - ordinal number of the aggregate block whose
 *                             bit is to be located
 *      page_number  - input - pointer to a variable in which to return
 *                             the ordinal number of the page, in the fsck
 *                             workspace block map, containing the bit
 *                             for the given block
 *      byte_offset  - input - pointer to a variable in which to return
 *                             the ordinal number of the byte, in page_number
 *                             page, containing the bit for the given block
 *      mask_ptr     - input - pointer to a variable in which to return
 *                             a mask to apply to the byte at byte_offset
 *                             in order to reference the bit for the given
 *                             block
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blkmap_find_bit(int64_t blk_number,
		    int64_t * page_number,
		    uint32_t * byte_offset, uint32_t * mask_ptr)
{
	int bfb_rc = FSCK_OK;
	uint64_t remainder;
	uint32_t bit_position;

	*page_number = blk_number >> log2BITSPERPAGE;
	remainder = blk_number - ((*page_number) << log2BITSPERPAGE);
	*byte_offset = (remainder >> log2BITSPERDWORD) * BYTESPERDWORD;
	bit_position = remainder - ((*byte_offset) << log2BITSPERBYTE);
	*mask_ptr = 0x80000000u >> bit_position;

	return (bfb_rc);
}

/*****************************************************************************
 * NAME: blkmap_flush
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate
 *            and the current block map buffer has been updated since
 *            the most recent read operation, write the buffer contents to
 *            the device.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blkmap_flush()
{
	int bmpf_rc = FSCK_OK;
	uint32_t bytes_written;
	struct fsck_blk_map_page *fsck_bmpt_ptr;

	if (agg_recptr->blkmp_buf_write) {
		/* buffer has been updated since most recent write */

		/* swap if on big endian machine */
		fsck_bmpt_ptr = agg_recptr->blkmp_buf_ptr;
		swap_multiple(ujfs_swap_fsck_blk_map_page, fsck_bmpt_ptr, 4);
		bmpf_rc = readwrite_device(agg_recptr->blkmp_agg_offset,
					   agg_recptr->blkmp_buf_data_len,
					   &bytes_written,
					   (void *) agg_recptr->blkmp_buf_ptr,
					   fsck_WRITE);
		fsck_bmpt_ptr = agg_recptr->blkmp_buf_ptr;
		swap_multiple(ujfs_swap_fsck_blk_map_page, fsck_bmpt_ptr, 4);

		if (bmpf_rc == FSCK_OK) {
			if (bytes_written == agg_recptr->blkmp_buf_data_len) {
				/* buffer has been written
				 * to the device and won't need to be
				 * written again unless/until the
				 * buffer contents have been altered.
				 */
				agg_recptr->blkmp_buf_write = 0;
			} else {
				/* didn't write correct number of bytes */
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
					      Vol_Label, 2);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONWSP, FSCK_FAILED_FBMAP_BADFLUSH,
					      bmpf_rc, fsck_WRITE,
					      (long long) agg_recptr->blkmp_agg_offset,
					      agg_recptr->blkmp_buf_data_len,
					      bytes_written);

				bmpf_rc = FSCK_FAILED_FBMAP_BADFLUSH;
			}
		} else {
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
				      Vol_Label, 3);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONWSP, FSCK_FAILED_FBMAP_FLUSH,
				      bmpf_rc, fsck_WRITE,
				      (long long) agg_recptr->blkmp_agg_offset,
				      agg_recptr->blkmp_buf_data_len,
				      bytes_written);

			bmpf_rc = FSCK_FAILED_FBMAP_FLUSH;
		}
	}
	return (bmpf_rc);
}

/*****************************************************************************
 * NAME: blkmap_get_ctl_page
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate,
 *            write the contents of the given buffer over the current fsck
 *            fsck workspace block map control page on the device.
 *
 * PARAMETERS:
 *      blk_ctlptr  - input -  pointer to the buffer into the current fsck
 *                              workspace block map control page should be read.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blkmap_get_ctl_page(struct fsck_blk_map_hdr *blk_ctlptr)
{
	int bmgcp_rc = FSCK_OK;
	uint32_t bytes_read;

	if (agg_recptr->processing_readwrite) {
		/* have write access */
		bmgcp_rc = readwrite_device(agg_recptr->ondev_wsp_byte_offset,
					    BYTESPERPAGE,
					    &bytes_read,
					    (void *) agg_recptr->blkmp_ctlptr,
					    fsck_READ);
		if (bmgcp_rc == FSCK_OK) {

			/* swap if on big endian machine */
			ujfs_swap_fsck_blk_map_hdr(agg_recptr->blkmp_ctlptr);

			if (bytes_read != (uint32_t) BYTESPERPAGE) {
				/* didn't read correct number of bytes */
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONWSP, FSCK_BADREAD_FBLKMP,
					      bmgcp_rc, fsck_READ,
					      (long long) agg_recptr->ondev_wsp_byte_offset,
					      BYTESPERPAGE, bytes_read);

				bmgcp_rc = FSCK_BADREAD_FBLKMP;
			}
		} else {
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONWSP, FSCK_BADREAD_FBLKMP, bmgcp_rc,
				      fsck_READ,
				      (long long) agg_recptr->ondev_wsp_byte_offset,
				      BYTESPERPAGE, bytes_read);

			bmgcp_rc = FSCK_BADREAD_FBLKMP;
		}
	}
	return (bmgcp_rc);
}

/*****************************************************************************
 * NAME: blkmap_get_page
 *
 * FUNCTION:  Read the requested fsck workspace block map page into and/or
 *            locate the requested fsck workspace block map page in the
 *            fsck block map buffer.
 *
 * PARAMETERS:
 *      page_num       - input - ordinal number of the fsck workspace
 *                               block map page which is needed
 *      addr_page_ptr  - input - pointer to a variable in which to return
 *                               the address of the page in an fsck buffer
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blkmap_get_page(int64_t page_num, struct fsck_blk_map_page **addr_page_ptr)
{
	int bgp_rc = FSCK_OK;
	int64_t page_start_byte, page_end_byte;
	struct fsck_blk_map_page *fsck_bmpt_ptr;

	page_start_byte = page_num * BYTESPERPAGE;
	page_end_byte = page_start_byte + BYTESPERPAGE - 1;
	if ((page_start_byte >= agg_recptr->blkmp_blkmp_offset)
	    && (page_end_byte <=
		(agg_recptr->blkmp_blkmp_offset +
		 agg_recptr->blkmp_buf_data_len))) {
		/* the desired page is already in the buffer */
		*addr_page_ptr =
		    (struct fsck_blk_map_page *) ((char *) agg_recptr->
						  blkmp_buf_ptr +
						  (page_start_byte -
						   agg_recptr->
						   blkmp_blkmp_offset));
	} else {
		/* else need to read it in from dasd */
		if (!agg_recptr->processing_readwrite) {
			/* this isn't supposed
			 * to happen.  If we don't have write access
			 * to the aggregate then we're always supposed
			 * to get a hit in the buffer!
			 */
			bgp_rc = FSCK_INTERNAL_ERROR_6;
		} else {
			/* we have read/write access */
			/* if the buffer has been modified, write it to dasd */
			bgp_rc = blkmap_flush();
			if (bgp_rc == FSCK_OK) {
				/* successful write */
				agg_recptr->blkmp_blkmp_offset =
				    page_start_byte;
				/*
				 * The byte offset in the fsck block map plus
				 * one page of control information plus the
				 * aggregate bytes which precede the on-dasd
				 * fsck workspace
				 */
				agg_recptr->blkmp_agg_offset =
				    page_start_byte + (BYTESPERPAGE * 1) +
				    agg_recptr->ondev_wsp_byte_offset;
				bgp_rc =
				    readwrite_device(agg_recptr->
						     blkmp_agg_offset,
						     agg_recptr->
						     blkmp_buf_length,
						     &(agg_recptr->
						       blkmp_buf_data_len),
						     (void *) agg_recptr->
						     blkmp_buf_ptr, fsck_READ);

				if (bgp_rc == FSCK_OK) {
					/* successful read */
					/* swap if on big endian machine */
					fsck_bmpt_ptr =
					    agg_recptr->blkmp_buf_ptr;
					swap_multiple
					    (ujfs_swap_fsck_blk_map_page,
					     fsck_bmpt_ptr, 4);
					if (agg_recptr->blkmp_buf_data_len >=
					    BYTESPERPAGE) {
						*addr_page_ptr =
						    agg_recptr->blkmp_buf_ptr;
					} else {
						/* but didn't get enough to continue */
						/*
						 * message to user
						 */
						fsck_send_msg(fsck_URCVREAD,
							      fsck_ref_msg(fsck_metadata),
							      Vol_Label);
						/*
						 * message to debugger
						 */
						fsck_send_msg(fsck_ERRONWSP,
							      FSCK_FAILED_BADREAD_FBLKMP,
							      bgp_rc, fsck_READ,
							      (long long)agg_recptr->blkmp_agg_offset,
							      agg_recptr->blkmp_buf_length,
							      agg_recptr->blkmp_buf_data_len);

						bgp_rc =
						    FSCK_FAILED_BADREAD_FBLKMP;
					}
				} else {
					/* read failed */
					/*
					 * message to user
					 */
					fsck_send_msg(fsck_URCVREAD,
						      fsck_ref_msg(fsck_metadata),
						      Vol_Label);
					/*
					 * message to debugger
					 */
					fsck_send_msg(fsck_ERRONWSP,
						      FSCK_FAILED_READ_FBLKMP,
						      bgp_rc, fsck_READ,
						      (long long)agg_recptr->blkmp_agg_offset,
						      agg_recptr->blkmp_buf_length,
						      agg_recptr->blkmp_buf_data_len);

					bgp_rc = FSCK_FAILED_READ_FBLKMP;
				}
			}
		}
	}
	return (bgp_rc);
}

/*****************************************************************************
 * NAME: blkmap_put_ctl_page
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate,
 *            write the contents of the given buffer over the current fsck
 *            fsck workspace block map control page on the device.
 *
 * PARAMETERS:
 *      blk_ctlptr  - input -  pointer to the buffer which should be written
 *                             over the current fsck workspace block map
 *                             control page.
 *
 * NOTES:  Unlike most _put_ routines in this module, blkmap_put_ctl_page
 *         actually writes to the device.  This is done because the block
 *         map control page contains state and footprint information which
 *         provide crucial serviceability should the fsck session be
 *         interrupted.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blkmap_put_ctl_page(struct fsck_blk_map_hdr *blk_ctlptr)
{
	int bmpcp_rc = FSCK_OK;
	uint32_t bytes_written;

	if (agg_recptr->processing_readwrite) {
		/* have write access */

		/* swap if on big endian machine */
		ujfs_swap_fsck_blk_map_hdr(agg_recptr->blkmp_ctlptr);
		bmpcp_rc = readwrite_device(agg_recptr->ondev_wsp_byte_offset,
					    BYTESPERPAGE,
					    &bytes_written,
					    (void *) agg_recptr->blkmp_ctlptr,
					    fsck_WRITE);
		ujfs_swap_fsck_blk_map_hdr(agg_recptr->blkmp_ctlptr);

		if (bmpcp_rc == FSCK_OK) {
			if (bytes_written != (uint32_t) BYTESPERPAGE) {
				/* didn't write correct number of bytes */
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVWRT,
					      fsck_ref_msg(fsck_metadata),
					      Vol_Label, 4);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONWSP,
					      FSCK_FAILED_BADWRITE_FBLKMP,
					      bmpcp_rc, fsck_WRITE,
					      (long long)agg_recptr->ondev_wsp_byte_offset,
					      BYTESPERPAGE, bytes_written);

				bmpcp_rc = FSCK_FAILED_BADWRITE_FBLKMP;
			}
		} else {
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
				      Vol_Label, 5);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONWSP, FSCK_FAILED_WRITE_FBLKMP,
				      bmpcp_rc, fsck_WRITE,
				      (long long)agg_recptr->ondev_wsp_byte_offset,
				      BYTESPERPAGE, bytes_written);

			bmpcp_rc = FSCK_FAILED_WRITE_FBLKMP;
		}		/* end else the write was not successful */
	}
	return (bmpcp_rc);
}

/*****************************************************************************
 * NAME: blkmap_put_page
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate,
 *            note, in the fsck workspace, that the current fsck workspace
 *            block map buffer has been modified and should be written to
 *            the device in the next flush operation on this buffer.
 *
 * PARAMETERS:
 *      page_num  - input - ordinal number of the page in the fsck workspace
 *                          block map to write from the buffer to the device
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blkmap_put_page(int64_t page_num)
{
	int bpp_rc = FSCK_OK;

	if (agg_recptr->processing_readwrite) {
		agg_recptr->blkmp_buf_write = 1;
	}

	return (bpp_rc);
}

/*****************************************************************************
 * NAME: blktbl_ctl_page_put
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate,
 *            note, in the fsck workspace, that the current JFS Block Map
 *            control page buffer has been modified and should be written
 *            to the device in the next flush operation on this buffer.
 *
 * PARAMETERS:
 *      ctlpage_ptr  - input - the address, in an fsck buffer, of the page
 *                             which has been modified.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blktbl_ctl_page_put(struct dbmap *ctlpage_ptr)
{
	int bcpp_rc = FSCK_OK;

	if (agg_recptr->processing_readwrite) {
		/* swap if on big endian machine */
		ujfs_swap_dbmap(ctlpage_ptr);
		bcpp_rc = mapctl_put((void *) ctlpage_ptr);
	}

	return (bcpp_rc);
}

/*****************************************************************************
 * NAME: blktbl_dmap_get
 *
 * FUNCTION: Read the JFS Block Table dmap page describing the specified
 *           aggregate block into and/or locate the JFS Block Table dmap
 *           locate the requested page describing the specified
 *           aggregate block in the fsck dmap buffer.
 *
 * PARAMETERS:
 *      for_block           - input - ordinal number of the aggregate block
 *                                    whose dmap page is needed
 *      addr_dmap_page_ptr  - input - pointer to a variable in which to return
 *                                    the address of the found dmap page in
 *                                    an fsck buffer
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blktbl_dmap_get(int64_t for_block, struct dmap **addr_dmap_page_ptr)
{
	int bdg_rc = FSCK_OK;
	int64_t dmp_logical_fsblk_offset;
	int64_t dmp_logical_byte_offset;
	int64_t ext_logical_byte_offset;
	int64_t ext_byte_offset;
	int64_t last_in_buffer;
	struct dinode *bmap_inoptr;
	xad_t *xad_ptr;
	int8_t offset_found;
	int which_it;
	uint32_t bytes_read;
	int64_t ext_bytes, ext_bytes_left, offset_into_extent;
	struct dmap *dmap_t_ptr;

	dmp_logical_fsblk_offset =
	    BLKTODMAP(for_block, agg_recptr->log2_blksperpg);
	dmp_logical_byte_offset = dmp_logical_fsblk_offset * sb_ptr->s_bsize;
	*addr_dmap_page_ptr = NULL;

	if (dmp_logical_byte_offset >= agg_recptr->bmapdm_logical_offset) {
		last_in_buffer = agg_recptr->bmapdm_logical_offset +
		    agg_recptr->bmapdm_buf_data_len - 1;
		if ((dmp_logical_byte_offset + (int64_t) sizeof (struct dmap) -
		     1) <= last_in_buffer) {
			/*
			 * the one we want is already in the buffer
			 */
			*addr_dmap_page_ptr =
			    (struct dmap *) (agg_recptr->bmapdm_buf_ptr +
					     dmp_logical_byte_offset -
					     agg_recptr->bmapdm_logical_offset);
		}
	}

	if (*addr_dmap_page_ptr != NULL)
		goto bdg_exit;

	/* we have to read it in */
	/* perform any pending writes */
	bdg_rc = blktbl_dmaps_flush();
	if (bdg_rc != FSCK_OK)
		goto bdg_exit;

	/* flush worked ok */
	if (agg_recptr->primary_ait_4part1) {
		which_it = fsck_primary;
	} else {
		which_it = fsck_secondary;
	}
	bdg_rc = ait_special_read_ext1(which_it);
	if (bdg_rc != FSCK_OK) {
		report_readait_error(bdg_rc, FSCK_FAILED_CANTREADAITEXTF,
				     which_it);
		bdg_rc = FSCK_FAILED_CANTREADAITEXTF;
		goto bdg_exit;
	}

	/* got the first agg extent */
	bmap_inoptr = (struct dinode *)(agg_recptr->ino_buf_ptr +
	     BMAP_I * sizeof (struct dinode));
	bdg_rc = xTree_search(bmap_inoptr, dmp_logical_fsblk_offset,
			      &xad_ptr, &offset_found);
	if (bdg_rc != FSCK_OK)
		goto bdg_exit;

	/* nothing extraordinary happened */
	if (!offset_found) {
		bdg_rc = FSCK_INTERNAL_ERROR_51;
		goto bdg_exit;
	}

	/* we have the xad which describes the dmap */
	ext_logical_byte_offset = offsetXAD(xad_ptr) * sb_ptr->s_bsize;
	ext_byte_offset = addressXAD(xad_ptr) * sb_ptr->s_bsize;
	agg_recptr->bmapdm_agg_offset = ext_byte_offset +
	    dmp_logical_byte_offset - ext_logical_byte_offset;
	bdg_rc = readwrite_device(agg_recptr->bmapdm_agg_offset,
	     agg_recptr->bmapdm_buf_length, &bytes_read,
	     (void *) agg_recptr->bmapdm_buf_ptr, fsck_READ);
	if (bdg_rc == FSCK_OK) {
		/* swap if on big endian machine */
		dmap_t_ptr = (struct dmap *)agg_recptr->bmapdm_buf_ptr;
		swap_multiple(ujfs_swap_dmap, dmap_t_ptr, 4);
		agg_recptr->bmapdm_logical_offset = dmp_logical_byte_offset;
		*addr_dmap_page_ptr = (struct dmap *)agg_recptr->bmapdm_buf_ptr;
		/*
		 * we need to set the buffer data length to the number of
		 * bytes with actual bmap data.  That is, we may have read
		 * beyond the end of the extent, and if so, we need to
		 * ignore the tag-along data.
		 */
		ext_bytes = (int64_t)lengthXAD(xad_ptr) * sb_ptr->s_bsize;
		offset_into_extent = dmp_logical_byte_offset -
		    ext_logical_byte_offset;
		ext_bytes_left = ext_bytes - offset_into_extent;
		agg_recptr->bmapdm_buf_data_len =
		    MIN(bytes_read, ext_bytes_left);
	} else {
		/*
		 * message to user
		 */
		fsck_send_msg(fsck_URCVREAD, fsck_ref_msg(fsck_metadata), Vol_Label);
		/*
		 * message to debugger
		 */
		fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_READ_BMPDM,
			      bdg_rc, fsck_READ,
			      (long long)agg_recptr->bmapdm_agg_offset,
			      agg_recptr->bmapdm_buf_length,
			      agg_recptr->bmapdm_buf_data_len);

		bdg_rc = FSCK_FAILED_READ_BMPDM;
	}

      bdg_exit:
	return (bdg_rc);
}

/*****************************************************************************
 * NAME: blktbl_dmap_put
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate,
 *            note, in the fsck workspace, that the current fsck dmap
 *            buffer has been modified and should be written to the device
 *            in the next flush operation on this buffer.
 *
 * PARAMETERS:
 *      dmap_page_ptr  - input - address of the dmap page, in the fsck buffer,
 *                               which has been modified.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blktbl_dmap_put(struct dmap *dmap_page_ptr)
{
	int bdp_rc = FSCK_OK;

	if (agg_recptr->processing_readwrite) {
		agg_recptr->bmapdm_buf_write = 1;
	}

	return (bdp_rc);
}

/*****************************************************************************
 * NAME: blktbl_dmaps_flush
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate
 *            and the current dmap buffer has been updated since the most
 *            recent read operation, write the buffer contents to the device.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blktbl_dmaps_flush()
{
	int bdf_rc = FSCK_OK;
	uint32_t bytes_written;
	struct dmap *dmap_t_ptr;

	if (agg_recptr->bmapdm_buf_write) {
		/* buffer has been updated since
		 * most recent write
		 */

		/* swap if on big endian machine */
		dmap_t_ptr = (struct dmap *) agg_recptr->bmapdm_buf_ptr;
		swap_multiple(ujfs_swap_dmap, dmap_t_ptr, 4);
		bdf_rc = readwrite_device(agg_recptr->bmapdm_agg_offset,
					  agg_recptr->bmapdm_buf_data_len,
					  &bytes_written,
					  (void *) agg_recptr->bmapdm_buf_ptr,
					  fsck_WRITE);
		dmap_t_ptr = (struct dmap *) agg_recptr->bmapdm_buf_ptr;
		swap_multiple(ujfs_swap_dmap, dmap_t_ptr, 4);

		if (bdf_rc == FSCK_OK) {
			if (bytes_written == agg_recptr->bmapdm_buf_data_len) {
				/* buffer has been written to
				 * the device and won't need to be
				 * written again unless/until the
				 * buffer contents have been altered again.
				 */
				agg_recptr->bmapdm_buf_write = 0;
			} else {
				/* didn't write the correct number of bytes */
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVWRT,
					      fsck_ref_msg(fsck_metadata),
					      Vol_Label, 6);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BMPDM_BADFLUSH,
					      bdf_rc, fsck_WRITE,
					      (long long) agg_recptr->bmapdm_agg_offset,
					      agg_recptr->bmapdm_buf_data_len,
					      bytes_written);

				bdf_rc = FSCK_FAILED_BMPDM_BADFLUSH;
			}
		} else {
			/* else the write was not successful */
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
				      Vol_Label, 7);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BMPDM_FLUSH,
				      bdf_rc, fsck_WRITE, (long long) agg_recptr->bmapdm_agg_offset,
				      agg_recptr->bmapdm_buf_data_len,
				      agg_recptr->bmapdm_buf_data_len,
				      bytes_written);

			bdf_rc = FSCK_FAILED_BMPDM_FLUSH;
		}
	}
	return (bdf_rc);
}

/*****************************************************************************
 * NAME: blktbl_Ln_page_get
 *
 * FUNCTION: Read the JFS Block Map page describing the specified aggregate
 *           block at the specified summary level into and/or locate the
 *           locate the requested JFS Block Map page describing the specified
 *           aggregate block at the specified summary level in the
 *           fsck Level n page buffer.
 *
 * PARAMETERS:
 *      level             - input - Summary level of the page to get
 *      for_block         - input - ordinal number of the aggregate block
 *                                  whose summary page is needed
 *      addr_Ln_page_ptr  - input - pointer to a variable in which to return
 *                                  the address of the requested page in an
 *                                  fsck buffer
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blktbl_Ln_page_get(int8_t level, int64_t for_block,
		       struct dmapctl **addr_Ln_page_ptr)
{
	int blpg_rc = FSCK_OK;
	int64_t Lnpg_logical_fsblk_offset;
	int64_t Lnpg_logical_byte_offset;
	int64_t ext_logical_byte_offset;
	int64_t ext_byte_offset;
	int64_t last_in_buffer;
	struct dinode *bmap_inoptr;
	xad_t *xad_ptr;
	int8_t offset_found;
	int which_it;

	Lnpg_logical_fsblk_offset =
	    BLKTOCTL(for_block, agg_recptr->log2_blksperpg, level);
	Lnpg_logical_byte_offset = Lnpg_logical_fsblk_offset * sb_ptr->s_bsize;

	*addr_Ln_page_ptr = NULL;

	if (Lnpg_logical_byte_offset >= agg_recptr->bmaplv_logical_offset) {
		last_in_buffer = agg_recptr->bmaplv_logical_offset +
		    agg_recptr->bmaplv_buf_data_len;
		if ((Lnpg_logical_byte_offset +
		     (int64_t) sizeof (struct dmapctl)) <= last_in_buffer) {
			/*
			 * the one we want is already in the buffer
			 */
			*addr_Ln_page_ptr = (struct dmapctl *)
					    (agg_recptr->bmaplv_buf_ptr +
					    (Lnpg_logical_byte_offset -
					    agg_recptr->bmaplv_logical_offset));
		}
	}

	if (*addr_Ln_page_ptr != NULL)
		goto blpg_exit;

	/* we have to read it in */
	/* perform any pending writes */
	blpg_rc = blktbl_Ln_pages_flush();
	if (blpg_rc != FSCK_OK)
		goto blpg_exit;

	/* flush worked ok */
	if (agg_recptr->primary_ait_4part1) {
		which_it = fsck_primary;
	} else {
		which_it = fsck_secondary;
	}
	blpg_rc = ait_special_read_ext1(which_it);
	if (blpg_rc != FSCK_OK) {
		report_readait_error(blpg_rc, FSCK_FAILED_CANTREADAITEXTG,
				     which_it);
		blpg_rc = FSCK_FAILED_CANTREADAITEXTG;
		goto blpg_exit;
	}

	/* got the first agg extent */
	bmap_inoptr = (struct dinode *)(agg_recptr->ino_buf_ptr +
	     BMAP_I * sizeof (struct dinode));
	blpg_rc = xTree_search(bmap_inoptr, Lnpg_logical_fsblk_offset,
			       &xad_ptr, &offset_found);
	if (blpg_rc != FSCK_OK)
		goto blpg_exit;

	/* nothing extraordinary happened */
	if (!offset_found) {
		/* didn't find it! */
		blpg_rc = FSCK_INTERNAL_ERROR_52;
		goto blpg_exit;
	}

	/* we have the xad which describes the page */
	ext_logical_byte_offset = offsetXAD(xad_ptr) * sb_ptr->s_bsize;
	ext_byte_offset = addressXAD(xad_ptr) * sb_ptr->s_bsize;
	agg_recptr->bmaplv_agg_offset = ext_byte_offset +
	    Lnpg_logical_byte_offset - ext_logical_byte_offset;

	blpg_rc = readwrite_device(agg_recptr->bmaplv_agg_offset,
				   agg_recptr->bmaplv_buf_length,
				   &(agg_recptr->bmaplv_buf_data_len),
				   (void *) agg_recptr->bmaplv_buf_ptr,
				   fsck_READ);
	if (blpg_rc == FSCK_OK) {
		/* got the page */
		/* swap if on big endian machine */
		ujfs_swap_dmapctl((struct dmapctl *)agg_recptr->bmaplv_buf_ptr);

		agg_recptr->bmaplv_current_level = level;
		agg_recptr->bmaplv_logical_offset = Lnpg_logical_byte_offset;
		*addr_Ln_page_ptr = (struct dmapctl *)
		    agg_recptr->bmaplv_buf_ptr;
	} else {
		/*
		 * message to user
		 */
		fsck_send_msg(fsck_URCVREAD, fsck_ref_msg(fsck_metadata), Vol_Label);
		/*
		 * message to debugger
		 */
		fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_READ_BMPLV,
			      blpg_rc, fsck_READ,
			      (long long)agg_recptr->bmaplv_agg_offset,
			      agg_recptr->bmaplv_buf_length,
			      agg_recptr->bmaplv_buf_data_len);

		blpg_rc = FSCK_FAILED_READ_BMPLV;
	}

      blpg_exit:
	return (blpg_rc);
}

/*****************************************************************************
 * NAME: blktbl_Ln_page_put
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate,
 *            note, in the fsck workspace, that the current fsck Level n
 *            buffer has been modified and should be written to the device
 *            in the next flush operation on this buffer.
 *
 * PARAMETERS:
 *      Ln_page_ptr  - input - Address, in an fsck buffer, of the block map
 *                             summary page which has been modified.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blktbl_Ln_page_put(struct dmapctl *Ln_page_ptr)
{
	int blpp_rc = FSCK_OK;

	if (agg_recptr->processing_readwrite) {
		agg_recptr->bmaplv_buf_write = 1;
	}

	return (blpp_rc);
}

/*****************************************************************************
 * NAME: blktbl_Ln_pages_flush
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate
 *            and the current Level n Page buffer has been updated since
 *            the most recent read operation, write the buffer contents to
 *            the device.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int blktbl_Ln_pages_flush()
{
	int blpf_rc = FSCK_OK;
	uint32_t bytes_written;

	if (!agg_recptr->bmaplv_buf_write)
		goto blpf_exit;

	/* buffer has been updated since
	 * most recent write
	 */

	/* swap if on big endian machine */
	ujfs_swap_dmapctl((struct dmapctl *) agg_recptr->bmaplv_buf_ptr);
	blpf_rc = readwrite_device(agg_recptr->bmaplv_agg_offset,
				   agg_recptr->bmaplv_buf_data_len,
				   &bytes_written,
				   (void *) agg_recptr->bmaplv_buf_ptr,
				   fsck_WRITE);
	ujfs_swap_dmapctl((struct dmapctl *) agg_recptr->bmaplv_buf_ptr);

	if (blpf_rc == FSCK_OK) {
		if (bytes_written == agg_recptr->bmaplv_buf_data_len) {
			/* buffer has been written to
			 * the device and won't need to be
			 * written again unless/until the
			 * buffer contents have been altered again.
			 */
			agg_recptr->bmaplv_buf_write = 0;
		} else {	/* didn't write the correct number of bytes */
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
				      Vol_Label, 8);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BMPLV_BADFLUSH,
				      blpf_rc, fsck_WRITE,
				      (long long) agg_recptr->bmaplv_agg_offset,
				      agg_recptr->bmaplv_buf_data_len,
				      bytes_written);

			blpf_rc = FSCK_FAILED_BMPLV_BADFLUSH;
		}
	} else {
		/*
		 * message to user
		 */
		fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
			      Vol_Label, 9);
		/*
		 * message to debugger
		 */
		fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BMPLV_FLUSH,
			      blpf_rc, fsck_WRITE,
			      (long long) agg_recptr->bmaplv_agg_offset,
			      agg_recptr->bmaplv_buf_data_len,
			      bytes_written);

		blpf_rc = FSCK_FAILED_BMPLV_FLUSH;
	}

      blpf_exit:
	return (blpf_rc);
}

/**************************************************************************
 * NAME: close_volume
 *
 * FUNCTION:  Flush all data to disk and close the device containing the
 * 	      aggregate.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int close_volume()
{
	int rc;

	rc = ujfs_flush_dev(Dev_IOPort);

	if (fclose(Dev_IOPort))
		return ERROR_INVALID_HANDLE;
	return rc;
}

/*****************************************************************************
 * NAME: dnode_get
 *
 * FUNCTION: Read the requested dnode page into and/or locate the requested
 *           dnode page in the fsck dnode buffer.
 *
 * PARAMETERS:
 *      dnode_fsblk_offset  - input - offset, in aggregate blocks, into the
 *                                    aggregate of the dnode to read
 *      dnode_length        - input - number of bytes in the dnode
 *      addr_dtpage_ptr     - input - pointer to a variable in which to return
 *                                    the address of the requested dnode in an
 *                                    fsck buffer
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int dnode_get(int64_t dnode_fsblk_offset, uint32_t dnode_length,
	      dtpage_t ** addr_dtpage_ptr)
{
	int dnodg_rc = FSCK_OK;
	int64_t dnode_start_byte, dnode_end_byte;
	dtpage_t *dtp;

	dnode_start_byte = dnode_fsblk_offset * sb_ptr->s_bsize;
	dnode_end_byte = dnode_start_byte + dnode_length - 1;

	if ((agg_recptr->ondev_wsp_fsblk_offset != 0)
	    && (dnode_fsblk_offset > agg_recptr->ondev_wsp_fsblk_offset)) {
		/*
		 * the offset is beyond the range
		 * valid for fileset objects
		 */
		/*
		 * This case is not caused by an I/O error, but by
		 * invalid data in an inode.  Let the caller handle
		 * the consequences.
		 */
		dnodg_rc = FSCK_BADREADTARGET;
	} else if ((dnode_start_byte >= agg_recptr->dnode_agg_offset)
		   && (dnode_end_byte <=
		       (agg_recptr->dnode_agg_offset +
			agg_recptr->dnode_buf_data_len))) {
		/*
		 * the target dir node is already in
		 * the buffer
		 */
		dtp = *addr_dtpage_ptr = (dtpage_t *)
		    (agg_recptr->dnode_buf_ptr + dnode_start_byte -
		     agg_recptr->dnode_agg_offset);

		/* swap if on big endian machine */
		if (!(dtp->header.flag & BT_SWAPPED)) {
			ujfs_swap_dtpage_t(dtp, sb_ptr->s_flag);
			dtp->header.flag |= BT_SWAPPED;
		}
	} else {
		/* else we'll have to read it from the disk */
		agg_recptr->dnode_agg_offset = dnode_start_byte;
		dnodg_rc = readwrite_device(agg_recptr->dnode_agg_offset,
					    agg_recptr->dnode_buf_length,
					    &(agg_recptr->dnode_buf_data_len),
					    (void *) agg_recptr->dnode_buf_ptr,
					    fsck_READ);
		if (dnodg_rc == FSCK_OK) {
			/* read appears successful */
			if (agg_recptr->dnode_buf_data_len >= dnode_length) {
				/*
				 * we may not have gotten all we asked for,
				 * but we got enough to cover the dir node
				 * we were after
				 */

				dtp = *addr_dtpage_ptr =
				    (dtpage_t *) agg_recptr->dnode_buf_ptr;

				/* swap if on big endian machine */
				ujfs_swap_dtpage_t(dtp, sb_ptr->s_flag);
				dtp->header.flag |= BT_SWAPPED;

			} else {
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVREAD,
					      fsck_ref_msg(fsck_metadata),
					      Vol_Label);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BADREAD_DNODE,
					      dnodg_rc, fsck_READ,
					      (long long) agg_recptr->dnode_agg_offset,
					      agg_recptr->dnode_buf_length,
					      agg_recptr->dnode_buf_data_len);

				dnodg_rc = FSCK_FAILED_BADREAD_DNODE;
			}
		} else {
			/* bad return code from read */
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVREAD, fsck_ref_msg(fsck_metadata),
				      Vol_Label);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_READ_DNODE,
				      dnodg_rc, fsck_READ,
				      (long long) agg_recptr->dnode_agg_offset,
				      agg_recptr->dnode_buf_length,
				      agg_recptr->dnode_buf_data_len);

			dnodg_rc = FSCK_FAILED_READ_DNODE;
		}
	}
	return (dnodg_rc);
}

/*****************************************************************************
 * NAME: ea_get
 *
 * FUNCTION: Read the specified Extended Attributes data (ea) into
 *           the specified buffer.
 *
 * PARAMETERS:
 *      ea_fsblk_offset  - input - offset, in aggregate blocks, into the
 *                                    aggregate of the ea to read
 *      ea_byte_length   - input - length, in bytes, of the ea to read
 *      eabuf_ptr        - input - the address (in dynamic storage) of the
 *                                 buffer into which to read the ea
 *      eabuf_length     - input - pointer to a variable in which contains
 *                                 the length of the buffer at eabuf_ptr
 *      ea_data_length   - input - pointer to a variable in which to return
 *                                 the number of bytes actually read from the
 *                                 device
 *      ea_agg_offset    - input - pointer to a variable in which to return
 *                                 the offset, in bytes, into the aggregate
 *                                 of the ea to read
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int ea_get(int64_t ea_fsblk_offset,
	   uint32_t ea_byte_length,
	   char *eabuf_ptr,
	   uint32_t * eabuf_length,
	   uint32_t * ea_data_length, int64_t * ea_agg_offset)
{
	int ea_rc = FSCK_OK;
	int64_t start_byte;

	start_byte = ea_fsblk_offset * sb_ptr->s_bsize;
	ea_rc = readwrite_device(start_byte, ea_byte_length, ea_data_length,
				 (void *) eabuf_ptr, fsck_READ);

	/* swap if on big endian machine, currently unused */

	if (ea_rc == FSCK_OK) {	/* read appears successful */
		*ea_agg_offset = start_byte;
		if ((*ea_data_length) < ea_byte_length) {
			/* we didn't get enough */
			*ea_agg_offset = 0;
			*ea_data_length = 0;
			ea_rc = FSCK_BADEADESCRIPTOR;
		}
	} else {
		/* bad return code from read */
		ea_rc = FSCK_CANTREADEA;
	}
	return (ea_rc);
}

/*****************************************************************************
 * NAME: fscklog_put_buffer
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate,
 *            and if the in-aggregate fsck log is not full, write the
 *            contents of the current fscklog buffer into the in-aggregate
 *            fsck log.
 *
 * PARAMETERS:  none
 *
 * NOTES:  o Unlike most _put_ routines in this module, _buffer
 *           actually writes to the device.  This is done because the fsck
 *           log contains information which provides crucial serviceability
 *           should the fsck session be interrupted.
 *
 *         o Errors here are recorded in the control page of the fsck
 *           in-aggregate workspace but never affect other fsck processing.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int fscklog_put_buffer()
{
	int flpb_rc = FSCK_OK;
	int io_rc = FSCK_OK;
	uint32_t bytes_written = 0;
	uint32_t log_bytes_left;
	int32_t num_log_errors;
	struct fscklog_error *log_error_recptr;

	if ((!agg_recptr->fscklog_full) && (agg_recptr->processing_readwrite)) {
		/* have write access */

		/* swap if on big endian machine in proper format */

		io_rc = readwrite_device(agg_recptr->fscklog_agg_offset,
					 agg_recptr->fscklog_buf_length,
					 &bytes_written,
					 (void *) agg_recptr->fscklog_buf_ptr,
					 fsck_WRITE);
		if ((io_rc != FSCK_OK)
		    || (bytes_written !=
			(uint32_t) agg_recptr->fscklog_buf_length)) {
			/*
			 * write failed or didn't write correct
			 * number of bytes
			 */
			/* This prevents infinite recursion */
			agg_recptr->fscklog_full = 1;
			if (agg_recptr->blkmp_ctlptr) {
				num_log_errors =
				    agg_recptr->blkmp_ctlptr->hdr.
				    num_logwrite_errors;
				if (num_log_errors < 120) {
					log_error_recptr =
					    &(agg_recptr->blkmp_ctlptr->hdr.
					      logerr[num_log_errors]);
					log_error_recptr->err_offset =
					    agg_recptr->fscklog_agg_offset;
					log_error_recptr->bytes_written =
					    bytes_written;
					log_error_recptr->io_retcode = io_rc;
				}
				agg_recptr->blkmp_ctlptr->hdr.
				    num_logwrite_errors += 1;
			}
			/*
			 * message to debugger
			 *
			 * N.B. This is NOT a fatal condition!
			 */
			fsck_send_msg(fsck_ERRONLOG, FSCK_BADWRITE_FSCKLOG,
				      io_rc, fsck_WRITE,
				      (long long) agg_recptr->fscklog_agg_offset,
				      agg_recptr->fscklog_buf_length,
				      bytes_written);
		}
	}
	/*
	   * We want to reset the buffer no matter what.
	   * It is useful to refill the buffer even if logging is not
	   * active because it may provide diagnostic information in
	   * a dump.
	 */
	agg_recptr->fscklog_agg_offset += agg_recptr->fscklog_buf_length;
	agg_recptr->fscklog_log_offset += agg_recptr->fscklog_buf_length;
	agg_recptr->fscklog_buf_data_len = 0;
	log_bytes_left = (agg_recptr->ondev_fscklog_byte_length / 2) -
	    agg_recptr->fscklog_log_offset;
	if (log_bytes_left < agg_recptr->fscklog_buf_length) {
		/*
		 * can't fit another buffer full
		 * into the log
		 */
		if (!agg_recptr->initializing_fscklog) {
			/* this is a false
			 * condition if doing log initialization
			 */
			agg_recptr->fscklog_full = -1;
			agg_recptr->blkmp_ctlptr->hdr.fscklog_full = -1;
		}
	}
	return (flpb_rc);
}

/*****************************************************************************
 * NAME: iag_get
 *
 * FUNCTION: Read the requested iag into and/or locate the requested iag
 *           in the fsck iag buffer.
 *
 * PARAMETERS:
 *      is_aggregate  - input -  0 => the iag is owned by the fileset
 *                              !0 => the iag is owned by the aggregate
 *      which_it      - input - ordinal number of the aggregate inode
 *                              representing the inode table to which the
 *                              iag belongs.
 *      which_ait     - input - the aggregate inode table { fsck_primary |
 *                              fsck_secondary } containing the version of
 *                              which_it to use for this operation
 *      iag_num       - input - ordinal number of the iag needed
 *      addr_iag_ptr  - input - pointer to a variable in which to return
 *                              the address, in an fsck buffer, of the
 *                              requested iag.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iag_get(int is_aggregate, int which_it, int which_ait, int32_t iag_num,
	    struct iag **addr_iag_ptr)
{
	int iagg_rc = FSCK_OK;
	int64_t imap_logical_block, extent_offset;
	int64_t offset_in_extent = 0;
	struct dinode *imap_inoptr;
	xad_t *xad_ptr;
	int8_t offset_found;
	int which_agg_inode = 0;

	*addr_iag_ptr = NULL;
	if ((agg_recptr->iag_buf_1st_inode == (iag_num << L2INOSPERIAG)) &&
	    (agg_recptr->iag_for_aggregate == is_aggregate) &&
	    (agg_recptr->iag_which_it == which_it)) {
		/*
		 * the target iag is already in the buffer
		 */
		*addr_iag_ptr = (struct iag *) agg_recptr->iag_buf_ptr;
		goto iagg_exit;
	}
	/* need to get the iag */
	/* perform any pending writes */
	iagg_rc = iags_flush();
	if (iagg_rc != FSCK_OK)
		goto iagg_exit;

	/* flush worked ok */
	if (is_aggregate) {
		/* this is an IAG describing aggregate inodes */
		if (iag_num < agg_recptr->agg_imap.num_iags) {
			/* in bounds */
			which_agg_inode = AGGREGATE_I;
			iagg_rc = ait_special_read_ext1(which_ait);
			if (iagg_rc != FSCK_OK) {
				/* read ait failed */
				report_readait_error(iagg_rc,
						     FSCK_FAILED_CANTREADAITEXTH,
						     which_ait);
				iagg_rc = FSCK_FAILED_CANTREADAITEXTH;
			}
		} else {
			/* invalid request */
			iagg_rc = FSCK_IAGNOOOAGGBOUNDS;
		}
	} else {
		/* an IAG describing fileset inodes */
		if (iag_num < agg_recptr->fset_imap.num_iags) {
			/* in bounds */
			which_agg_inode = FILESYSTEM_I;
			iagg_rc = ait_special_read_ext1(which_ait);
			if (iagg_rc != FSCK_OK) {
				/* read ait failed */
				report_readait_error(iagg_rc,
						     FSCK_FAILED_CANTREADAITEXTK,
						     which_ait);
				iagg_rc = FSCK_FAILED_CANTREADAITEXTK;
			}
		} else {
			/* invalid request */
			iagg_rc = FSCK_IAGNOOOFSETBOUNDS;
		}
	}
	if (iagg_rc != FSCK_OK)
		goto iagg_exit;

	/* got the extent */
	imap_inoptr = (struct dinode *)(agg_recptr->ino_buf_ptr +
	     (which_agg_inode * sizeof (struct dinode)));
	imap_logical_block = IAGTOLBLK(iag_num, agg_recptr->log2_blksperpg);
	iagg_rc = xTree_search(imap_inoptr, imap_logical_block, &xad_ptr,
			       &offset_found);
	if (iagg_rc != FSCK_OK)
		goto iagg_exit;

	/* nothing extraordinary happened */
	if (!offset_found) {
		iagg_rc = FSCK_INTERNAL_ERROR_50;
	} else {
		/* we have the xad which describes the iag */
		extent_offset = offsetXAD(xad_ptr);
		if (extent_offset != imap_logical_block) {
			offset_in_extent = imap_logical_block - extent_offset;
		}
		agg_recptr->iag_agg_offset = sb_ptr->s_bsize *
		    (addressXAD(xad_ptr) + offset_in_extent);
		iagg_rc = readwrite_device(agg_recptr->iag_agg_offset,
					   agg_recptr->iag_buf_length,
					   &(agg_recptr->iag_buf_data_len),
					   (void *) agg_recptr->iag_buf_ptr,
					   fsck_READ);
		if (iagg_rc == FSCK_OK) {
			/* got the iag */
			/* swap if on big endian machine */
			ujfs_swap_iag((struct iag *)agg_recptr->iag_buf_ptr);
			agg_recptr->iag_buf_1st_inode = iag_num << L2INOSPERIAG;
			agg_recptr->iag_fsnum = imap_inoptr->di_fileset;
			agg_recptr->iag_for_aggregate = is_aggregate;
			agg_recptr->iag_which_it = which_it;
			*addr_iag_ptr = (struct iag *)agg_recptr->iag_buf_ptr;
		} else {
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVREAD, fsck_ref_msg(fsck_metadata),
				      Vol_Label);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_READ_IAG,
				      iagg_rc, fsck_READ,
				      (long long)agg_recptr->iag_agg_offset,
				      agg_recptr->iag_buf_length,
				      agg_recptr->iag_buf_data_len);
			iagg_rc = FSCK_FAILED_READ_IAG;
		}
	}

      iagg_exit:
	return (iagg_rc);
}

/*****************************************************************************
 * NAME: iag_get_first
 *
 * FUNCTION: Read the first iag in the specified inode table into and/or
 *           locate the first iag in the specified inode table in the
 *           fsck iag buffer.  Set up for sequential access on the iag's
 *           in this table.
 *
 * PARAMETERS:
 *      is_aggregate  - input -  0 => the iag is owned by the fileset
 *                              !0 => the iag is owned by the aggregate
 *      which_it      - input - ordinal number of the aggregate inode
 *                              representing the inode table to which the
 *                              iag belongs.
 *      which_ait     - input - the aggregate inode table { fsck_primary |
 *                              fsck_secondary } containing the version of
 *                              which_it to use for this operation
 *      addr_iag_ptr  - input - pointer to a variable in which to return
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iag_get_first(int is_aggregate, int it_number, int which_ait,
		  struct iag **addr_iag_ptr)
{
	int iaggf_rc = FSCK_OK;
	struct dinode *imap_inoptr;
	xad_t *xadptr;
	int it_tbl_is_agg_owned = -1;	/* the table may not describe the
					 * aggregate inodes, but the inode
					 * describing the table is ALWAYS
					 * an aggregate inode
					 */
	agg_recptr->fais.this_iagnum = 0;
	agg_recptr->fais.this_inoidx = 0;
	/* assume it's not a rootleaf */
	agg_recptr->fais.rootleaf_imap = 0;
	/*
	 * get the first leaf (xtpage) of the imap into the mapleaf buffer
	 *
	 * Note: if the imap B+ Tree has a root-leaf then the
	 *       first (and only) leaf is located in the dinode.
	 */
	/*
	 * save inode_get from having to read an
	 * iag as an intermediate step.
	 */
	iaggf_rc = ait_special_read_ext1(which_ait);
	if (iaggf_rc != FSCK_OK) {	/* read ait failed */
		report_readait_error(iaggf_rc, FSCK_FAILED_CANTREADAITEXTJ,
				     which_ait);
		iaggf_rc = FSCK_FAILED_CANTREADAITEXTJ;
	} else {
		/* got the inode extent */
		if (!is_aggregate) {
			/* after the fileset inode table */
			if (agg_recptr->fset_imap.imap_is_rootleaf) {
				/* it's a root-leaf */
				agg_recptr->fais.rootleaf_imap = -1;
				/* read the imap inode into the inode buffer */
				iaggf_rc =
				    inode_get(it_tbl_is_agg_owned, which_ait,
					      it_number, &imap_inoptr);
				if (iaggf_rc != FSCK_OK) {
					/* something went wrong */
					iaggf_rc = FSCK_FAILED_AGFS_READ5;
				}
			}
		} else {
			/* want the aggregate inode table */
			if (agg_recptr->agg_imap.imap_is_rootleaf) {
				/* it's a rootleaf */
				agg_recptr->fais.rootleaf_imap = -1;
				/* read the imap inode into the inode buffer */
				iaggf_rc =
				    inode_get(it_tbl_is_agg_owned, it_number,
					      AGGREGATE_I, &imap_inoptr);
				if (iaggf_rc != FSCK_OK) {
					/* something went wrong */
					iaggf_rc = FSCK_FAILED_AGFS_READ5;
				}
			}
		}
	}
	if ((iaggf_rc == FSCK_OK) && (agg_recptr->fais.rootleaf_imap)) {
		/*
		 * root-leaf imap and we have the inode
		 */
		/* copy the inode into the imap leaf buf */
		memcpy((void *) (agg_recptr->mapleaf_buf_ptr),
		       (void *) imap_inoptr, sizeof (struct dinode));
		agg_recptr->mapleaf_buf_data_len = 0;
		agg_recptr->mapleaf_agg_offset = 0;
		agg_recptr->mapleaf_for_aggregate = is_aggregate;
		agg_recptr->mapleaf_which_it = it_number;
		imap_inoptr = (struct dinode *) (agg_recptr->mapleaf_buf_ptr);
		agg_recptr->fais.this_mapleaf =
		    (xtpage_t *) & (imap_inoptr->di_btroot);
	}
	if ((iaggf_rc == FSCK_OK) && (!agg_recptr->fais.rootleaf_imap)) {
		/*
		 * something below the root
		 */
		if (!is_aggregate) {
			/* after the fileset inode table */
			/* get the first leaf into the mapleaf buffer */
			iaggf_rc =
			    imapleaf_get(agg_recptr->fset_imap.
					 first_leaf_offset,
					 &(agg_recptr->fais.this_mapleaf));
		} else {
			/* must want the aggregate inode table */
			/* get the first leaf into the mapleaf buffer */
			iaggf_rc =
			    imapleaf_get(agg_recptr->agg_imap.first_leaf_offset,
					 &(agg_recptr->fais.this_mapleaf));
		}
	}
	if (iaggf_rc == FSCK_OK) {
		/* the first imap leaf is in the buf */
		/* first in the leaf */
		agg_recptr->fais.iagidx_now = XTENTRYSTART;
		agg_recptr->fais.iagidx_max =
		    agg_recptr->fais.this_mapleaf->header.nextindex - 1;
		/*
		 * get the first iag of the imap into the iag buffer
		 */
		iaggf_rc = iags_flush();
		if (iaggf_rc == FSCK_OK) {
			/* flushed ok */
			xadptr =
			    &(agg_recptr->fais.this_mapleaf->
			      xad[agg_recptr->fais.iagidx_now]);
			/*
			 * the first iag is preceded by the IT control page.
			 */
			agg_recptr->iag_agg_offset =
			    (sb_ptr->s_bsize * addressXAD(xadptr)) +
			    sizeof (struct dinomap);
			agg_recptr->iag_buf_1st_inode =
			    agg_recptr->fais.this_inoidx;
			agg_recptr->iag_fsnum = it_number;
			agg_recptr->iag_for_aggregate = is_aggregate;
			agg_recptr->iag_fsnum = it_number;
			iaggf_rc = readwrite_device(agg_recptr->iag_agg_offset,
						    agg_recptr->iag_buf_length,
						    &(agg_recptr->
						      iag_buf_data_len),
						    (void *) (agg_recptr->
							      iag_buf_ptr),
						    fsck_READ);

			/* swap if on big endian machine */
			ujfs_swap_iag((struct iag *) agg_recptr->iag_buf_ptr);

		}
	}
	if (iaggf_rc == FSCK_OK) {
		/* first iag is in iag buffer */
		agg_recptr->fais.iagptr =
		    (struct iag *) agg_recptr->iag_buf_ptr;
		agg_recptr->fais.extidx_now = 0;
		agg_recptr->fais.extidx_max = EXTSPERIAG - 1;
		*addr_iag_ptr = agg_recptr->fais.iagptr;
	} else {
		/*
		 * message to user
		 */
		fsck_send_msg(fsck_URCVREAD, fsck_ref_msg(fsck_metadata),
			      Vol_Label);
		/*
		 * message to debugger
		 */
		fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BADREAD_IAG,
			      iaggf_rc, fsck_READ,
			      (long long) agg_recptr->iag_agg_offset,
			      agg_recptr->iag_buf_length,
			      agg_recptr->iag_buf_data_len);
		iaggf_rc = FSCK_FAILED_BADREAD_IAG;
	}
	return (iaggf_rc);
}

/*****************************************************************************
 * NAME: iag_get_next
 *
 * FUNCTION: Read the next iag in the specified inode table into and/or
 *           locate the next iag in the specified inode table in the
 *           fsck iag buffer.
 *
 * PARAMETERS:
 *      addr_iag_ptr  - input - pointer to a variable in which to return
 *                              the address, in an fsck buffer, of the next
 *                              iag in the aggregate inode table currently
 *                              being traversed.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iag_get_next(struct iag **addr_iag_ptr)
{
	int iaggn_rc = FSCK_OK;
	int8_t iag_found = 0;
	int8_t end_of_imap = 0;
	int64_t leaf_byteaddr;
	xad_t *xadptr;

	agg_recptr->fais.this_iagnum++;
	iaggn_rc = iags_flush();
	if (iaggn_rc == FSCK_OK) {
		/* flushed the iags ok */
		while ((!iag_found) && (!end_of_imap) && (iaggn_rc == FSCK_OK)) {
			agg_recptr->fais.iagidx_now++;
			if (agg_recptr->fais.iagidx_now <=
			    agg_recptr->fais.iagidx_max) {
				/*
				 * the imap leaf is in the buffer already
				 */
				xadptr =
				    &(agg_recptr->fais.this_mapleaf->
				      xad[agg_recptr->fais.iagidx_now]);
				agg_recptr->iag_agg_offset =
				    sb_ptr->s_bsize * addressXAD(xadptr);
				agg_recptr->iag_buf_1st_inode =
				    agg_recptr->fais.this_inoidx;
				iaggn_rc =
				    readwrite_device(agg_recptr->iag_agg_offset,
						     agg_recptr->iag_buf_length,
						     &(agg_recptr->
						       iag_buf_data_len),
						     (void *) (agg_recptr->
							       iag_buf_ptr),
						     fsck_READ);
				if (iaggn_rc == FSCK_OK) {
					/* got the iag */

					/* swap if on big endian machine */
					ujfs_swap_iag((struct iag *)
						      agg_recptr->iag_buf_ptr);

					agg_recptr->fais.iagptr =
					    (struct iag *) (agg_recptr->
							    iag_buf_ptr);
					agg_recptr->fais.extidx_now = 0;
					agg_recptr->fais.this_inoidx =
					    agg_recptr->fais.this_iagnum *
					    NUM_INODE_PER_IAG;
					agg_recptr->iag_buf_1st_inode =
					    agg_recptr->fais.this_inoidx;
					agg_recptr->fais.extidx_max =
					    EXTSPERIAG - 1;
					iag_found = -1;
				} else {
					/*
					 * message to user
					 */
					fsck_send_msg(fsck_URCVREAD,
						      fsck_ref_msg(fsck_metadata),
						      Vol_Label);
					/*
					 * message to debugger
					 */
					fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BADREAD1_IAG,
						      iaggn_rc, fsck_READ,
						      (long long) agg_recptr->iag_agg_offset,
						      agg_recptr->iag_buf_length,
						      agg_recptr->iag_buf_data_len);
					iaggn_rc = FSCK_FAILED_BADREAD1_IAG;
				}
			} else {
				/* we need to get the next imap leaf (if any) */
				if (agg_recptr->fais.rootleaf_imap) {
					/*
					 * there aren't any more imap leafs
					 */
					end_of_imap = -1;
				} else if (agg_recptr->fais.this_mapleaf->
					   header.next == ((int64_t) 0)) {
					/*
					 * there aren't any more imap leafs
					 */
					end_of_imap = -1;
				} else {
					/* there is another leaf */
					leaf_byteaddr = sb_ptr->s_bsize *
					    agg_recptr->fais.this_mapleaf->
					    header.next;
					iaggn_rc =
					    imapleaf_get(leaf_byteaddr,
							 &(agg_recptr->fais.
							   this_mapleaf));
					if (iaggn_rc == FSCK_OK) {
						/* got the imap leaf */
						agg_recptr->fais.iagidx_now =
						    XTENTRYSTART - 1;
						agg_recptr->fais.iagidx_max =
						    agg_recptr->fais.
						    this_mapleaf->header.
						    nextindex - 1;
					}
				}
			}
		}
	}
	if (end_of_imap) {
		/* there aren't any more iags */
		agg_recptr->fais.iagptr = NULL;
		agg_recptr->fais.iagidx_now = -1;
	}
	if (iaggn_rc == FSCK_OK) {
		/* everything worked! */
		*addr_iag_ptr = agg_recptr->fais.iagptr;
	}
	return (iaggn_rc);
}

/*****************************************************************************
 * NAME: iag_put
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate,
 *            note, in the fsck workspace, that the current iag buffer
 *            has been modified and should be written to the device in the
 *            next flush operation on this buffer.
 *
 * PARAMETERS:
 *      iagptr  - input -  pointer to the iag, in an fsck buffer, which has
 *                         been modified.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iag_put(struct iag *iagptr)
{
	int iagp_rc = FSCK_OK;

	if (agg_recptr->processing_readwrite) {
		/* we have write access */
		agg_recptr->iag_buf_write = 1;
	}
	return (iagp_rc);
}

/*****************************************************************************
 * NAME: iags_flush
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate
 *            and the current iag buffer has been updated since
 *            the most recent read operation, write the buffer contents to
 *            the device.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int iags_flush()
{
	int iagf_rc = FSCK_OK;
	uint32_t bytes_written;

	if (agg_recptr->iag_buf_write) {
		/* buffer has been updated since
		 * most recent write
		 */

		/* swap if on big endian machine */
		ujfs_swap_iag((struct iag *) agg_recptr->iag_buf_ptr);
		iagf_rc = readwrite_device(agg_recptr->iag_agg_offset,
					   agg_recptr->iag_buf_data_len,
					   &bytes_written,
					   (void *) agg_recptr->iag_buf_ptr,
					   fsck_WRITE);
		ujfs_swap_iag((struct iag *) agg_recptr->iag_buf_ptr);

		if (iagf_rc == FSCK_OK) {
			if (bytes_written == agg_recptr->iag_buf_data_len) {
				/* buffer has been written to
				 * the device and won't need to be
				 * written again unless/until the
				 * buffer contents have been altered again.
				 */
				agg_recptr->iag_buf_write = 0;
			} else {
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVWRT,
					      fsck_ref_msg(fsck_metadata),
					      Vol_Label, 10);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_IAG_BADFLUSH,
					      iagf_rc, fsck_WRITE,
					      (long long) agg_recptr->iag_agg_offset,
					      agg_recptr->iag_buf_data_len,
					      bytes_written);
				iagf_rc = FSCK_FAILED_IAG_BADFLUSH;
			}
		} else {
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
				      Vol_Label, 11);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_IAG_FLUSH,
				      iagf_rc, fsck_WRITE,
				      (long long) agg_recptr->iag_agg_offset,
				      agg_recptr->iag_buf_data_len,
				      bytes_written);
			iagf_rc = FSCK_FAILED_IAG_FLUSH;
		}
	}
	return (iagf_rc);
}

/*****************************************************************************
 * NAME: imapleaf_get
 *
 * FUNCTION: Read the specified inode map leaf node into and/or
 *           locate the specified inode map leaf node in the
 *           fsck inode map leaf buffer.
 *
 * PARAMETERS:
 *      leaf_start_byte  - input - offset, in bytes, in the aggregate, of
 *                                 the needed map leaf node.
 *      addr_leaf_ptr    - input - pointer to a variable in which to return
 *                                 the address of the map leaf in an fsck
 *                                 buffer.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int imapleaf_get(int64_t leaf_start_byte, xtpage_t ** addr_leaf_ptr)
{
	int imlfg_rc = FSCK_OK;
	int64_t leaf_end_byte;
	xtpage_t *xtpage_t_ptr;

	leaf_end_byte = leaf_start_byte + sizeof (xtpage_t) - 1;
	if ((leaf_start_byte >= agg_recptr->mapleaf_agg_offset) &&
	    (leaf_end_byte <= (agg_recptr->mapleaf_agg_offset +
			       agg_recptr->mapleaf_buf_data_len))) {
		/*
		 * the target leaf is already in
		 * the buffer
		 */
		*addr_leaf_ptr = (xtpage_t *) (agg_recptr->mapleaf_buf_ptr +
					       leaf_start_byte -
					       agg_recptr->mapleaf_agg_offset);
	} else {
		/* else we'll have to read it from the disk */
		agg_recptr->mapleaf_agg_offset = leaf_start_byte;
		imlfg_rc = readwrite_device(agg_recptr->mapleaf_agg_offset,
					    agg_recptr->mapleaf_buf_length,
					    &(agg_recptr->mapleaf_buf_data_len),
					    (void *) agg_recptr->
					    mapleaf_buf_ptr, fsck_READ);

		if (imlfg_rc == FSCK_OK) {
			/* read appears successful */
			if (agg_recptr->mapleaf_buf_data_len >=
			    (sizeof (xtpage_t))) {
				/*
				 * we may not have gotten all we asked for,
				 * but we got enough to cover the node we
				 * were after
				 */

				/* swap if on big endian machine */
				xtpage_t_ptr =
				    (xtpage_t *) agg_recptr->mapleaf_buf_ptr;
				swap_multiple(ujfs_swap_xtpage_t, xtpage_t_ptr,
					      4);

				*addr_leaf_ptr =
				    (xtpage_t *) agg_recptr->mapleaf_buf_ptr;
			} else {
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVREAD,
					      fsck_ref_msg(fsck_metadata),
					      Vol_Label);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BADREAD_IMPLF,
					      imlfg_rc, fsck_READ,
					      (long long) agg_recptr->node_agg_offset,
					      agg_recptr->node_buf_length,
					      agg_recptr->node_buf_data_len);
				imlfg_rc = FSCK_FAILED_BADREAD_IMPLF;
			}
		} else {
			/* bad return code from read */
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVREAD,
				      fsck_ref_msg(fsck_metadata),
				      Vol_Label);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_READ_IMPLF,
				      imlfg_rc, fsck_READ,
				      (long long) agg_recptr->node_agg_offset,
				      agg_recptr->node_buf_length,
				      agg_recptr->node_buf_data_len);
			imlfg_rc = FSCK_FAILED_READ_IMPLF;
		}
	}
	return (imlfg_rc);
}

/*****************************************************************************
 * NAME: inode_get
 *
 * FUNCTION: Read the specified inode into and/or located the specified inode
 *           in the fsck inode buffer.
 *
 * PARAMETERS:
 *      is_aggregate  - input -  0 => the inode is owned by the fileset
 *                              !0 => the inode is owned by the aggregate
 *      which_it      - input - ordinal number of the aggregate inode
 *                              representing the inode table to which the
 *                              inode belongs.
 *      ino_idx       - input - ordinal number of the inode to read
 *      addr_ino_ptr  - input - pointer to a variable in which to return
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int inode_get(int is_aggregate, int it_number, uint32_t ino_idx,
	      struct dinode **addr_ino_ptr)
{
	int inog_rc = FSCK_OK;
	int which_ait;
	int32_t iag_num;
	struct iag *iag_ptr;
	int32_t iag_extidx;
	int64_t inoext_fsblk_addr;
	int64_t inoext_byte_addr;

	*addr_ino_ptr = NULL;
	/*
	 * see if the requested inode is already in the buffer
	 */
	if ((is_aggregate && agg_recptr->ino_for_aggregate) ||
	    (((!is_aggregate) && (!agg_recptr->ino_for_aggregate)) &&
	     (it_number == agg_recptr->ino_fsnum))) {
		/*
		 * buffer contains inodes from the proper table
		 */
		if ((ino_idx >= agg_recptr->ino_buf_1st_ino) &&
		    (ino_idx <=
		     (agg_recptr->ino_buf_1st_ino + INOSPEREXT - 1))) {
			/*
			 * the requested inode number is in the
			 * range for the extent in the buffer now
			 */
			*addr_ino_ptr = (struct dinode *)
			    ((ino_idx - agg_recptr->ino_buf_1st_ino) * DISIZE +
			     agg_recptr->ino_buf_ptr);
		}
	}
	if (*addr_ino_ptr != NULL)
		goto inog_exit;

	/* it isn't in the buffer */
	/* handle any pending deferred writes */
	inog_rc = inodes_flush();
	/*
	 * get the iag describing its extent
	 */
	if (inog_rc != FSCK_OK)
		goto inog_exit;

	/* flushed cleanly */
	if (is_aggregate) {
		if (ino_idx < FIRST_FILESET_INO) {
			/* in part 1 of AIT */
			if (agg_recptr->primary_ait_4part1) {
				which_ait = fsck_primary;
			} else {
				which_ait = fsck_secondary;
			}
		} else {
			/* in part 2 of AIT */
			if (agg_recptr->primary_ait_4part2) {
				which_ait = fsck_primary;
			} else {
				which_ait = fsck_secondary;
			}
		}
	} else {
		/* fileset inode */
		if (agg_recptr->primary_ait_4part2) {
			which_ait = fsck_primary;
		} else {
			which_ait = fsck_secondary;
		}
	}
	/* figure out which IAG describes the
	 * extent containing the requested inode
	 */
	iag_num = INOTOIAG(ino_idx);
	inog_rc = iag_get(is_aggregate, it_number, which_ait,
			  iag_num, &iag_ptr);
	if (inog_rc != FSCK_OK)
		goto inog_exit;

	/*
	 * this is the index into the ixpxd array (in the
	 * iag) for the entry describing the extent with
	 * the desired inode.
	 */
	iag_extidx = (ino_idx - agg_recptr->iag_buf_1st_inode)
	    >> L2INOSPEREXT;
	inoext_fsblk_addr = addressPXD(&(iag_ptr->inoext[iag_extidx]));
	if (inoext_fsblk_addr == 0) {
		/* the extent isn't allocated */
		inog_rc = FSCK_INOEXTNOTALLOC;
		goto inog_exit;
	}

	/* the inode extent is allocated */
	inoext_byte_addr = inoext_fsblk_addr * sb_ptr->s_bsize;
	inog_rc = readwrite_device(inoext_byte_addr, agg_recptr->ino_buf_length,
				   &(agg_recptr->ino_buf_data_len),
				   (void *)agg_recptr->ino_buf_ptr, fsck_READ);

	if (inog_rc == FSCK_OK) {
		agg_recptr->ino_buf_agg_offset = inoext_byte_addr;
		agg_recptr->ino_buf_1st_ino = (iag_num << L2INOSPERIAG)
		    + (iag_extidx << L2INOSPEREXT);
		agg_recptr->ino_fsnum = it_number;
		memcpy((void *)&(agg_recptr->ino_ixpxd),
		       (void *) &(iag_ptr->inoext[iag_extidx]), sizeof (pxd_t));
		is_aggregate ? (agg_recptr->ino_for_aggregate = -1) :
		    (agg_recptr->ino_for_aggregate = 0);
		agg_recptr->ino_which_it = it_number;

		*addr_ino_ptr = (struct dinode *)((ino_idx -
		    agg_recptr->ino_buf_1st_ino) * DISIZE +
		    agg_recptr->ino_buf_ptr);
		/* swap if on big endian machine */
		ujfs_swap_inoext((struct dinode *)agg_recptr->ino_buf_ptr,
				 GET, sb_ptr->s_flag);

	} else {
		/* bad return code from read */
		/*
		 * message to user
		 */
		fsck_send_msg(fsck_URCVREAD, fsck_ref_msg(fsck_metadata),
			      Vol_Label);
		/*
		 * message to debugger
		 */
		fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BADREAD_INODE1,
			      inog_rc, fsck_READ,
			      (long long)inoext_byte_addr,
			      agg_recptr->ino_buf_length,
			      agg_recptr->ino_buf_data_len);
		inog_rc = FSCK_FAILED_BADREAD_INODE1;
	}

      inog_exit:
	return (inog_rc);
}

/*****************************************************************************
 * NAME: inode_get_first_fs
 *
 * FUNCTION: Read the first inode in the specified fileset into and/or
 *           locate the first inode in the specified fileset in the
 *           fsck inode buffer.
 *
 * PARAMETERS:
 *      it_number     - input - ordinal number of the aggregate inode
 *                              representing the inode table to which the
 *                              inode belongs.
 *      ino_idx       - input - pointer to a variable in which to return
 *                              the ordinal number of the first inode in
 *                              use in the aggregate.
 *      addr_ino_ptr  - input - pointer to a variable in which to return
 *                              the address, in an fsck buffer, of the first
 *                              inode now being used in the aggregate.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int inode_get_first_fs(int it_number, uint32_t * ino_idx,
		       struct dinode **addr_ino_ptr)
{
	int igff_rc = FSCK_OK;
	int which_ait;
	int is_aggregate = 0;
	pxd_t *pxdptr;
	struct iag *iagptr;

	if (agg_recptr->primary_ait_4part2) {
		which_ait = fsck_primary;
	} else {
		which_ait = fsck_secondary;
	}
	/*
	 * get the first iag of the imap into the iag buffer
	 */
	igff_rc = iag_get_first(is_aggregate, it_number, which_ait, &iagptr);
	if (igff_rc == FSCK_OK) {
		/* first iag is in iag buffer */
		/*
		 * get the first inode extent described by the iag into the inode buf
		 *
		 * note: the very first one must be allocated since it contains
		 *       the root directory inode (which has been verified correct)
		 */
		igff_rc = inodes_flush();
		if (igff_rc == FSCK_OK) {
			/* flushed ok */
			pxdptr =
			    &(agg_recptr->fais.iagptr->
			      inoext[agg_recptr->fais.extidx_now]);
			agg_recptr->ino_buf_agg_offset =
			    sb_ptr->s_bsize * addressPXD(pxdptr);
			agg_recptr->ino_buf_1st_ino =
			    agg_recptr->fais.this_inoidx;
			agg_recptr->ino_fsnum = it_number;
			agg_recptr->ino_for_aggregate = is_aggregate;
			agg_recptr->ino_which_it = it_number;
			memcpy((void *) &(agg_recptr->ino_ixpxd),
			       (void *) pxdptr, sizeof (pxd_t));
			igff_rc =
			    readwrite_device(agg_recptr->ino_buf_agg_offset,
					     agg_recptr->ino_buf_length,
					     &(agg_recptr->ino_buf_data_len),
					     (void *) (agg_recptr->ino_buf_ptr),
					     fsck_READ);

			/* swap if on big endian machine */
			ujfs_swap_inoext((struct dinode *) agg_recptr->
					 ino_buf_ptr, GET, sb_ptr->s_flag);
		}
	}
	if (igff_rc == FSCK_OK) {
		/* the first inode extent is in buf */
		agg_recptr->fais.extptr =
		    (struct dinode *) (agg_recptr->ino_buf_ptr);
		/*
		 * FILESET_OBJECT_I is the inode number for the first
		 * fileset inode not reserved for metadata (excepting the
		 * root directory which is handled as a special case)
		 */
		agg_recptr->fais.this_inoidx = FILESET_OBJECT_I;
		agg_recptr->fais.inoidx_now = FILESET_OBJECT_I;
		agg_recptr->fais.inoidx_max = INOSPEREXT - 1;
		/*
		 * locate the first (regular fileset object) inode in the inode buf
		 */
		agg_recptr->fais.inoptr = (struct dinode *)
		    ((char *) agg_recptr->fais.extptr +
		     (agg_recptr->fais.inoidx_now * DISIZE));
	} else {
		/* bad return code from read */
		/*
		 * message to user
		 */
		fsck_send_msg(fsck_URCVREAD, fsck_ref_msg(fsck_metadata),
			      Vol_Label);
		/*
		 * message to debugger
		 */
		fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_READ_INODE,
			      igff_rc, fsck_READ,
			      (long long) agg_recptr->ino_buf_agg_offset,
			      agg_recptr->ino_buf_length,
			      agg_recptr->ino_buf_data_len);
		igff_rc = FSCK_FAILED_READ_INODE;
	}
	if (igff_rc == FSCK_OK) {
		*ino_idx = agg_recptr->fais.this_inoidx;
		*addr_ino_ptr = agg_recptr->fais.inoptr;
	}
	return (igff_rc);
}

/*****************************************************************************
 * NAME: inode_get_next
 *
 * FUNCTION: Read the next inode in the specified fileset into and/or
 *           locate the next inode in the specified fileset in the
 *           fsck inode buffer.
 *
 * PARAMETERS:
 *      ino_idx       - input - pointer to a variable in which to return
 *                              the ordinal number of the next inode in the
 *                              current inode table traversal.
 *      addr_ino_ptr  - input - pointer to a variable in which to return
 *                              the address, in an fsck buffer, of the next
 *                              inode in the current inode table traversal.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int inode_get_next(uint32_t * ino_idx, struct dinode **addr_ino_ptr)
{
	int ign_rc = FSCK_OK;
	int8_t ext_found, end_of_fileset;
	int64_t ext_blkaddr;
	pxd_t *pxdptr;
	struct iag *iagptr;

	/* increment inode ordinal number. If this one
	 * is allocated then it's the one we want
	 */
	agg_recptr->fais.this_inoidx++;
	/* increment index into current extent */
	agg_recptr->fais.inoidx_now++;
	if (agg_recptr->fais.inoidx_now <= agg_recptr->fais.inoidx_max) {
		/*
		 * the inode is in the buffer already
		 */
		agg_recptr->fais.inoptr = (struct dinode *)
		    ((char *) agg_recptr->fais.inoptr + DISIZE);
		goto ign_set_exit;
	}
	/* we need the next allocated inode extent */
	ign_rc = inodes_flush();
	if (ign_rc != FSCK_OK)
		goto ign_set_exit;

	/* flushed inodes ok */
	ext_found = 0;
	end_of_fileset = 0;
	/* increment index into current iag */
	agg_recptr->fais.extidx_now++;
	while ((!ext_found) && (!end_of_fileset) && (ign_rc == FSCK_OK)) {
		if (agg_recptr->fais.extidx_now <=
		    agg_recptr->fais.extidx_max) {
			/*
			 * the iag is in the buffer already
			 */
			pxdptr = &(agg_recptr->fais.iagptr->inoext
				   [agg_recptr->fais.extidx_now]);
			ext_blkaddr = addressPXD(pxdptr);
			if (ext_blkaddr == 0) {
				/* this extent isn't allocated */
				agg_recptr->fais.extidx_now++;
				agg_recptr->fais.this_inoidx += INOSPEREXT;
			} else {
				/* this extent is allocated */
				agg_recptr->ino_buf_agg_offset =
				    sb_ptr->s_bsize * ext_blkaddr;
				agg_recptr->ino_buf_1st_ino =
				    agg_recptr->fais.this_inoidx;
				memcpy((void *)&(agg_recptr->ino_ixpxd),
				       (void *) pxdptr, sizeof (pxd_t));
				ign_rc = readwrite_device(
				     agg_recptr->ino_buf_agg_offset,
				     agg_recptr->ino_buf_length,
				     &(agg_recptr->ino_buf_data_len),
				     (void *) (agg_recptr->ino_buf_ptr),
				     fsck_READ);

				if (ign_rc == FSCK_OK) {
					/* got the extent */
					/* swap if on big endian machine */
					ujfs_swap_inoext((struct dinode *)
							 agg_recptr->ino_buf_ptr,
							 GET, sb_ptr->s_flag);
					ext_found = -1;
					agg_recptr->fais.extptr =
					    (struct dinode *)
					    (agg_recptr->ino_buf_ptr);
					agg_recptr->fais.inoidx_now = 0;
					agg_recptr->fais.inoidx_max =
					    INOSPEREXT - 1;
					ext_found = -1;
					agg_recptr->fais.inoptr =
					    (struct dinode*)
					    (agg_recptr->ino_buf_ptr);
				} else {
					/* bad return code from read */
					/*
					 * message to user
					 */
					fsck_send_msg(fsck_URCVREAD,
						      fsck_ref_msg(fsck_metadata),
						      Vol_Label);
					/*
					 * message to debugger
					 */
					fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BADREAD_INODE,
						      ign_rc, fsck_READ,
						      (long long)agg_recptr->ino_buf_agg_offset,
						      agg_recptr->ino_buf_length,
						      agg_recptr->ino_buf_data_len);
					ign_rc = FSCK_FAILED_BADREAD_INODE;
				}
			}
		} else {
			/* we need to get the next iag (if any) */
			ign_rc = iag_get_next(&iagptr);
			if ((ign_rc == FSCK_OK) && (iagptr == NULL)) {
				end_of_fileset = -1;
			}
		}
	}
	if (end_of_fileset) {
		/* there aren't any more extents */
		agg_recptr->fais.inoptr = NULL;
		agg_recptr->fais.this_inoidx = -1;
	}

      ign_set_exit:
	if (ign_rc == FSCK_OK) {
		/* everything worked! */
		*ino_idx = agg_recptr->fais.this_inoidx;
		*addr_ino_ptr = agg_recptr->fais.inoptr;
	}
	return (ign_rc);
}

/*****************************************************************************
 * NAME: inode_put
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate,
 *            note, in the fsck workspace, that the current inode buffer
 *            has been modified and should be written to the device in the
 *            next flush operation on this buffer.
 *
 * PARAMETERS:
 *      ino_ptr  - input - address, in an fsck buffer, of the inode which has
 *                         been updated.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int inode_put(struct dinode *ino_ptr)
{
	int ip_rc = FSCK_OK;

	if (agg_recptr->processing_readwrite) {
		/* buffer has been updated and needs to be written to the device */
		agg_recptr->ino_buf_write = -1;
	}
	return (ip_rc);
}

/*****************************************************************************
 * NAME: inodes_flush
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate
 *            and the current inode buffer has been updated since the most
 *            recent read operation, write the buffer contents to the device.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int inodes_flush()
{
	int inof_rc = FSCK_OK;
	uint32_t bytes_written;

	if (agg_recptr->ino_buf_write) {
		/* buffer has been updated since
		 * most recent write
		 */

		/* swap if on big endian machine */
		ujfs_swap_inoext((struct dinode *) agg_recptr->ino_buf_ptr, PUT,
				 sb_ptr->s_flag);
		inof_rc =
		    readwrite_device(agg_recptr->ino_buf_agg_offset,
				     agg_recptr->ino_buf_data_len,
				     &bytes_written,
				     (void *) agg_recptr->ino_buf_ptr,
				     fsck_WRITE);
		ujfs_swap_inoext((struct dinode *) agg_recptr->ino_buf_ptr, GET,
				 sb_ptr->s_flag);

		if (inof_rc == FSCK_OK) {
			if (bytes_written == agg_recptr->ino_buf_data_len) {
				/* buffer has been written
				 * to the device and won't need to be
				 * written again unless/until the
				 * buffer contents have been altered.
				 */
				agg_recptr->ino_buf_write = 0;
			} else {
				/* didn't write the correct number of bytes */
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVWRT,
					      fsck_ref_msg(fsck_metadata),
					      Vol_Label, 12);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_INODE_BADFLUSH,
					      inof_rc, fsck_WRITE,
					      (long long)agg_recptr->ino_buf_agg_offset,
					      agg_recptr->ino_buf_data_len,
					      bytes_written);
				inof_rc = FSCK_FAILED_INODE_BADFLUSH;
			}
		} else {
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVWRT,
				      fsck_ref_msg(fsck_metadata), Vol_Label, 14);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_INODE_FLUSH,
				      inof_rc, fsck_WRITE,
				      (long long) agg_recptr->ino_buf_agg_offset,
				      agg_recptr->ino_buf_data_len,
				      bytes_written);
			inof_rc = FSCK_FAILED_INODE_FLUSH;
		}
	}
	return (inof_rc);
}

/*****************************************************************************
 * NAME: inotbl_get_ctl_page
 *
 * FUNCTION: Read the control page for the specified inode table into and/or
 *           locate the control page for the specified inode table in the
 *           fsck inode table control page buffer.
 *
 * PARAMETERS:
 *      is_aggregate      - input -  0 => the inode table is fileset owned
 *                                  !0 => the inode table is aggregate owned
 *      addr_ctlpage_ptr  - input - pointer to a variable in which to return
 *                                  the address of the control page in an
 *                                  fsck buffer
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int inotbl_get_ctl_page(int is_aggregate, struct dinomap **addr_ctlpage_ptr)
{
	int igcp_rc = FSCK_OK;
	int intermed_rc = FSCK_OK;
	int64_t offset_ctlpage = 0;
	int which_it;
	int which_ait;
	struct dinode *inoptr;
	xtpage_t *xtpg_ptr;
	/*
	 * get the byte offset of the control page
	 */
	if (is_aggregate) {
		/* aggregate inode table wanted */
		if (agg_recptr->primary_ait_4part1) {
			offset_ctlpage = AIMAP_OFF / sb_ptr->s_bsize;
		} else {
			offset_ctlpage = addressPXD(&(sb_ptr->s_aim2));
		}
	} else {
		/* fileset inode table wanted */
		which_it = FILESYSTEM_I;
		if (agg_recptr->primary_ait_4part2) {
			which_ait = fsck_primary;
		} else {
			which_ait = fsck_secondary;
		}
		if (agg_recptr->fset_imap.imap_is_rootleaf) {
			/* root leaf inode */
			intermed_rc = ait_special_read_ext1(which_ait);
			if (intermed_rc != FSCK_OK) {
				report_readait_error(intermed_rc,
						     FSCK_FAILED_AGFS_READ4,
						     which_ait);
				igcp_rc = FSCK_FAILED_AGFS_READ4;
			} else {
				/* got the agg inode extent */
				inoptr = (struct dinode *)
				    (agg_recptr->ino_buf_ptr +
				     which_it * sizeof (struct dinode));
				xtpg_ptr = (xtpage_t *) & (inoptr->di_btroot);
				offset_ctlpage =
				    addressXAD(&(xtpg_ptr->xad[XTENTRYSTART]));
			}
		} else {
			/* tree doesn't have a root-leaf */
			intermed_rc =
			    imapleaf_get(agg_recptr->fset_imap.
					 first_leaf_offset, &xtpg_ptr);
			if (intermed_rc != FSCK_OK) {
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVREAD,
					      fsck_ref_msg(fsck_metadata), Vol_Label);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_READ_NODE3,
					      igcp_rc, fsck_READ,
					      (long long)agg_recptr->ino_buf_agg_offset,
					      agg_recptr->ino_buf_length,
					      agg_recptr->ino_buf_data_len);
				igcp_rc = FSCK_FAILED_READ_NODE3;
			} else {
				/* got the first leaf node */
				offset_ctlpage =
				    addressXAD(&(xtpg_ptr->xad[XTENTRYSTART]));
			}
		}
	}
	/*
	 * read the control page into the buffer
	 */
	if (igcp_rc == FSCK_OK) {
		/* we have an offset */
		igcp_rc = mapctl_get(offset_ctlpage, (void **) &xtpg_ptr);
		if (igcp_rc != FSCK_OK) {
			/* this is fatal */
			igcp_rc = FSCK_FAILED_CANTREADAITCTL;
		} else {
			/* the control page is in the buffer */
			*addr_ctlpage_ptr = (struct dinomap *) xtpg_ptr;

			/* swap if on big endian machine */
			ujfs_swap_dinomap(*addr_ctlpage_ptr);
		}
	}
	return (igcp_rc);
}

/*****************************************************************************
 * NAME: inotbl_put_ctl_page
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate,
 *            note, in the fsck workspace, that the current fsck inode table
 *            control page buffer has been modified and should be written
 *            to the device in the next flush operation on this buffer.
 *
 * PARAMETERS:
 *      is_aggregate  - input -  0 => the inode table is fileset owned
 *                              !0 => the inode table is aggregate owned
 *      ctlpage_ptr   - input - the address, in an fsck buffer, of the
 *                               control page which has been updated
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int inotbl_put_ctl_page(int is_aggregate, struct dinomap *ctlpage_ptr)
{
	int ipcp_rc = FSCK_OK;

	if (agg_recptr->processing_readwrite) {
		/* swap if on big endian machine */
		ujfs_swap_dinomap(ctlpage_ptr);
		ipcp_rc = mapctl_put((void *) ctlpage_ptr);
	}
	return (ipcp_rc);
}

/*****************************************************************************
 * NAME: mapctl_get
 *
 * FUNCTION: Read the specified map control page into and/or locate the
 *           requested specified map control page in the fsck map control
 *           page buffer.
 *
 * PARAMETERS:
 *      mapctl_fsblk_offset  - input - offset, in aggregate blocks, into the
 *                                     aggregate, of the needed map control
 *                                     page.
 *      addr_mapctl_ptr      - input - pointer to a variable in which to return
 *                                     the address, in an fsck buffer, of the
 *                                     map control page which has been read.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int mapctl_get(int64_t mapctl_fsblk_offset, void **addr_mapctl_ptr)
{
	int mg_rc = FSCK_OK;
	int64_t start_byte;

	start_byte = mapctl_fsblk_offset * sb_ptr->s_bsize;
	if (start_byte == agg_recptr->mapctl_agg_offset) {
		/*
		 * the target control page is already in
		 * the buffer
		 */
		*addr_mapctl_ptr = (void *) (agg_recptr->mapctl_buf_ptr);
	} else {
		/* else we'll have to read it from the disk */
		/* handle any pending deferred writes */
		mg_rc = mapctl_flush();
		if (mg_rc == FSCK_OK) {
			/* flushed ok */
			agg_recptr->mapctl_agg_offset = start_byte;
			mg_rc = readwrite_device(agg_recptr->mapctl_agg_offset,
						 agg_recptr->mapctl_buf_length,
						 &(agg_recptr->
						   mapctl_buf_data_len),
						 (void *) agg_recptr->
						 mapctl_buf_ptr, fsck_READ);

			/* endian - can't swap here, don't know if it's struct dmap
			   or struct dinomap. swap before calling this routine.           */

			if (mg_rc == FSCK_OK) {
				/* read appears successful */
				if (agg_recptr->mapctl_buf_data_len >=
				    BYTESPERPAGE) {
					/*
					 * we may not have gotten all we asked for,
					 * but we got enough to cover the page we
					 * were after
					 */
					*addr_mapctl_ptr =
					    (void *) agg_recptr->mapctl_buf_ptr;
				} else {
					/* didn't get the minimum number of bytes */
					/*
					 * message to user
					 */
					fsck_send_msg(fsck_URCVREAD,
						      fsck_ref_msg(fsck_metadata),
						      Vol_Label);
					/*
					 * message to debugger
					 */
					fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BADREAD_MAPCTL,
						      mg_rc, fsck_READ,
						      (long long)agg_recptr->mapctl_agg_offset,
						      agg_recptr->mapctl_buf_length,
						      agg_recptr->mapctl_buf_data_len);
					mg_rc = FSCK_FAILED_BADREAD_MAPCTL;
				}
			} else {
				/* bad return code from read */
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVREAD,
					      fsck_ref_msg(fsck_metadata),
					      Vol_Label);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_READ_MAPCTL,
					      mg_rc, fsck_READ,
					      (long long)agg_recptr->mapctl_agg_offset,
					      agg_recptr->mapctl_buf_length,
					      agg_recptr->mapctl_buf_data_len);
				mg_rc = FSCK_FAILED_READ_MAPCTL;
			}
		}
	}
	return (mg_rc);
}

/*****************************************************************************
 * NAME: mapctl_put
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate,
 *            note, in the fsck workspace, that the current fsck map control
 *            page buffer has been modified and should be written to the
 *            device in the next flush operation on this buffer.
 *
 * PARAMETERS:
 *      mapctl_ptr  - input - address, in an fsck buffer, of the map control
 *                            page which has been updated.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int mapctl_put(void *mapctl_ptr)
{
	int mp_rc = FSCK_OK;

	if (agg_recptr->processing_readwrite) {
		/* buffer has been updated and needs to be written to the device */
		agg_recptr->mapctl_buf_write = -1;
	}
	return (mp_rc);
}

/*****************************************************************************
 * NAME: mapctl_flush
 *
 * FUNCTION:  If the current fsck session has write access to the aggregate
 *            and the current map control buffer has been updated since
 *            the most recent read operation, write the buffer contents to
 *            the device.
 *
 * PARAMETERS:  none
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int mapctl_flush()
{
	int mf_rc = FSCK_OK;
	uint32_t bytes_written;

	if (agg_recptr->mapctl_buf_write) {
		/* buffer has been updated since
		 * most recent write
		 */

		/* endian - swap before calling this routine */

		mf_rc = readwrite_device(agg_recptr->mapctl_agg_offset,
					 agg_recptr->mapctl_buf_data_len,
					 &bytes_written,
					 (void *) agg_recptr->mapctl_buf_ptr,
					 fsck_WRITE);
		if (mf_rc == FSCK_OK) {
			if (bytes_written == agg_recptr->mapctl_buf_length) {
				/* buffer has been written to the
				 * device and won't need to be written again
				 * unless/until it the buffer contents have
				 * been altered.
				 */
				agg_recptr->mapctl_buf_write = 0;
			} else {
				/* didn't write the correct number of bytes */
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVWRT,
					      fsck_ref_msg(fsck_metadata),
					      Vol_Label, 15);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_MAPCTL_BADFLUSH,
					      mf_rc, fsck_WRITE,
					      (long long)agg_recptr->mapctl_agg_offset,
					      agg_recptr->mapctl_buf_data_len,
					      bytes_written);
				mf_rc = FSCK_FAILED_MAPCTL_BADFLUSH;
			}
		} else {
			/* else the write was not successful */
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
				      Vol_Label, 16);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_MAPCTL_FLUSH,
				      mf_rc, fsck_WRITE,
				      (long long)agg_recptr->mapctl_agg_offset,
				      agg_recptr->mapctl_buf_data_len,
				      bytes_written);
			mf_rc = FSCK_FAILED_MAPCTL_FLUSH;
		}
	}
	return (mf_rc);
}

/*****************************************************************************
 * NAME: node_get
 *
 * FUNCTION: Read the specified xTree node into and/or locate the specified
 *           xTree node in the fsck xTree node buffer.
 *
 * PARAMETERS:
 *      node_fsblk_offset  - input - offset, in aggregate blocks, into the
 *                                   aggregate, of the xTree node wanted
 *      addr_xtpage_ptr    - input - pointer to a variable in which to return
 *                                   the address, in an fsck buffer, of the
 *                                   of the xTree node which has been read.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int node_get(int64_t node_fsblk_offset, xtpage_t ** addr_xtpage_ptr)
{
	int nodg_rc = FSCK_OK;
	int64_t node_start_byte, node_end_byte;
	xtpage_t *xtpage_t_ptr;

	node_start_byte = node_fsblk_offset * sb_ptr->s_bsize;
	node_end_byte = node_start_byte + sizeof (xtpage_t) - 1;

	if ((agg_recptr->ondev_wsp_fsblk_offset != 0) &&
	    (node_fsblk_offset > agg_recptr->ondev_wsp_fsblk_offset)) {
		/*
		 * the offset is beyond the range
		 * valid for fileset objects
		 */
		/*
		 * This case is not caused by an I/O error, but by
		 * invalid data in an inode.  Let the caller handle
		 * the consequences.
		 */
		nodg_rc = FSCK_BADREADTARGET1;
	} else if ((node_start_byte >= agg_recptr->node_agg_offset) &&
		   (node_end_byte <= (agg_recptr->node_agg_offset +
				      agg_recptr->node_buf_data_len))) {
		/*
		 * the target node is already in
		 * the buffer
		 */
		*addr_xtpage_ptr = (xtpage_t *)
		    (agg_recptr->node_buf_ptr + node_start_byte -
		     agg_recptr->node_agg_offset);
	} else {
		/* else we'll have to read it from the disk */
		agg_recptr->node_agg_offset = node_start_byte;
		nodg_rc = readwrite_device(agg_recptr->node_agg_offset,
					   agg_recptr->node_buf_length,
					   &(agg_recptr->node_buf_data_len),
					   (void *) agg_recptr->node_buf_ptr,
					   fsck_READ);
		if (nodg_rc == FSCK_OK) {
			/* read appears successful */
			if (agg_recptr->node_buf_data_len >=
			    (sizeof (xtpage_t))) {
				/*
				 * we may not have gotten all we asked for,
				 * but we got enough to cover the node we
				 * were after
				 */

				/* swap if on big endian machine */
				xtpage_t_ptr =
				    (xtpage_t *) agg_recptr->node_buf_ptr;
				swap_multiple(ujfs_swap_xtpage_t, xtpage_t_ptr,
					      4);

				*addr_xtpage_ptr =
				    (xtpage_t *) agg_recptr->node_buf_ptr;
			} else {
				/* didn't get the minimum number of bytes */
				/*
				 * message to user
				 */
				fsck_send_msg(fsck_URCVREAD,
					      fsck_ref_msg(fsck_metadata),
					      Vol_Label);
				/*
				 * message to debugger
				 */
				fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_BADREAD_NODE,
					      nodg_rc, fsck_READ,
					      (long long)agg_recptr->node_agg_offset,
					      agg_recptr->node_buf_length,
					      agg_recptr->node_buf_data_len);
				nodg_rc = FSCK_FAILED_BADREAD_NODE;
			}
		} else {
			/* bad return code from read */
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVREAD,
				      fsck_ref_msg(fsck_metadata), Vol_Label);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_FAILED_READ_NODE,
				      nodg_rc, fsck_READ,
				      (long long)agg_recptr->node_agg_offset,
				      agg_recptr->node_buf_length,
				      agg_recptr->node_buf_data_len);
			nodg_rc = FSCK_FAILED_READ_NODE;
		}
	}
	return (nodg_rc);
}

/*****************************************************************************
 * NAME: open_device_read
 *
 * FUNCTION:  Open the specified device for read access.
 *
 * PARAMETERS:
 *      Device  - input - the device specification
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int open_device_read(const char *Device)
{
	Dev_IOPort = fopen(Device, "r");

	fsck_send_msg(fsck_DEVOPENRDRC, (Dev_IOPort == NULL) ?
			ERROR_FILE_NOT_FOUND : 0);
	if (Dev_IOPort == NULL)
		return ERROR_FILE_NOT_FOUND;
	Dev_blksize = Dev_SectorSize = PBSIZE;
	return 0;
}

/***************************************************************************
 * NAME: open_device_rw
 *
 * FUNCTION:  Open the device for read/write access.  Lock the device
 *            to prevent others from reading from or writing to the
 *            device, if possible.
 *
 * PARAMETERS:
 *      Device  - input -  The device specification
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int open_device_rw(const char *Device)
{
	Dev_IOPort = fopen_excl(Device, "r+");

	/*
	 * The 2.6.0 kernel no longer allows O_EXCL opens on a block device
	 * where a file system is mounted read-only.  Try again without the
	 * O_EXCL flag.
	 */
	if (Dev_IOPort == NULL)
		Dev_IOPort = fopen(Device, "r+");

	fsck_send_msg(fsck_DEVOPENRDWRRC, (Dev_IOPort == NULL) ?
			ERROR_FILE_NOT_FOUND : 0);
	if (Dev_IOPort == NULL)
		return ERROR_FILE_NOT_FOUND;
	Dev_blksize = Dev_SectorSize = PBSIZE;
	return 0;
}

/*****************************************************************************
 * NAME: open_volume
 *
 * FUNCTION:  Open the device on which the aggregate resides.
 *
 * PARAMETERS:
 *      volname  - input - The device specifier
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int open_volume(char *volname)
{
	int opnvol_rc = FSCK_OK;

	if (agg_recptr->parm_options[UFS_CHKDSK_LEVEL0] &&
	    agg_recptr->parm_options[UFS_CHKDSK_SKIPLOGREDO]) {
		/*
		 * read-only request
		 */
		opnvol_rc = open_device_read(volname);
		if (opnvol_rc == FSCK_OK) {
			/* successfully opened for Read */
			agg_recptr->processing_readonly = 1;
		}
	} else {
		/* read-write request */
		opnvol_rc = open_device_rw(volname);
		if (opnvol_rc == 0) {
			/* successfully opened for Read/Write */
			agg_recptr->processing_readwrite = 1;
		} else {
			/* unable to open for Read/Write */
			opnvol_rc = open_device_read(volname);
			if (opnvol_rc == FSCK_OK) {
				/* successfully opened for Read */
				agg_recptr->processing_readonly = 1;
			}
		}
	}

	if (opnvol_rc != FSCK_OK) {
		agg_recptr->fsck_is_done = 1;
	}

	return (opnvol_rc);
}

/*****************************************************************************
 * NAME: readwrite_device
 *
 * FUNCTION:  Read data from or write data to the device on which the
 *            aggregate resides.
 *
 * PARAMETERS:
 *      dev_offset           - input - the offset, in bytes, into the aggregate
 *                                     of the data to read or to which to write
 *                                     the data.
 *      requested_data_size  - input - the number of bytes requested
 *      actual_data_size     - input - pointer to a variable in which to return
 *                                     the number of bytes actually read or
 *                                     written
 *      data_buffer          - input - the address of the buffer in which to
 *                                     put the data or from which to write
 *                                     the data
 *      mode                 - input - { fsck_READ | fsck_WRITE }
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int readwrite_device(int64_t dev_offset,
		     uint32_t requested_data_size,
		     uint32_t * actual_data_size, void *data_buffer, int mode)
{
	int rwdb_rc = FSCK_OK;

	if ((ondev_jlog_byte_offset > 0)
	    && (ondev_jlog_byte_offset <= dev_offset)) {
		/* into jlog! */
#ifdef _JFS_DEBUG
		printf("READ/WRITE target is in JOURNAL!!\n");
		printf("       read/write offset = x%llx\r\n", dev_offset);
		printf("       jlog offset       = x%llx\r\n",
		       ondev_jlog_byte_offset);
		abort();
#endif
		rwdb_rc = FSCK_IOTARGETINJRNLLOG;
	} else {
		/* not trying to access the journal log */
		if ((dev_offset % Dev_SectorSize)
		    || (requested_data_size % Dev_SectorSize)) {
			rwdb_rc = FSCK_FAILED_SEEK;
		} else {
			/* offset seems ok */
			switch (mode) {
			case fsck_READ:
				rwdb_rc =
				    ujfs_rw_diskblocks(Dev_IOPort, dev_offset,
						       requested_data_size,
						       data_buffer, GET);
				break;
			case fsck_WRITE:
				rwdb_rc =
				    ujfs_rw_diskblocks(Dev_IOPort, dev_offset,
						       requested_data_size,
						       data_buffer, PUT);
				break;
			default:
				rwdb_rc = FSCK_INTERNAL_ERROR_3;
				break;
			}
		}

		if (rwdb_rc == FSCK_OK) {
			*actual_data_size = requested_data_size;
		} else {
			*actual_data_size = 0;
		}

	}

	return (rwdb_rc);
}

/*****************************************************************************
 * NAME: recon_dnode_assign
 *
 * FUNCTION:  Allocate a buffer for a new dnode at the specified aggregate
 *            offset.
 *
 * PARAMETERS:
 *      fsblk_offset  - input - The offset, in aggregate blocks, into the
 *                              aggregate, of the new dnode.
 *      addr_buf_ptr  - input - pointer to a variable in which to return
 *                              the address of the buffer allocated.
 *
 *
 * NOTES: The offset of the dnode being created is a required input because
 *        these buffers have a trailer record which contains the offset
 *         at which to write the dnode.  If this is not stored when the
 *         buffer is allocated for a new dnode, the other routines would
 *         have to treat a new dnode as a special case.
 *
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int recon_dnode_assign(int64_t fsblk_offset, dtpage_t ** addr_buf_ptr)
{
	int rda_rc = FSCK_OK;
	struct recon_buf_record *bufrec_ptr;

	rda_rc = dire_buffer_alloc(addr_buf_ptr);

	if (rda_rc != FSCK_OK) {
		fsck_send_msg(fsck_CANTRECONINSUFSTG, 1);
	} else {
		bufrec_ptr = (struct recon_buf_record *) *addr_buf_ptr;
		bufrec_ptr->dnode_blkoff = fsblk_offset;
		bufrec_ptr->dnode_byteoff = fsblk_offset * sb_ptr->s_bsize;
	}
	return (rda_rc);
}

/*****************************************************************************
 * NAME: recon_dnode_get
 *
 * FUNCTION: Allocate an fsck dnode buffer and read the JFS dnode page at
 *           the specified offset into it.
 *
 * PARAMETERS:
 *      fsblk_offset  - input - the offset, in aggregate blocks, into the
 *                              aggregate, of the desired dnode.
 *      addr_buf_ptr  - input - pointer to a variable in which to return
 *                              the fsck dnode buffer which has been allocated
 *                              and into which the requested dnode has been
 *                              read.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int recon_dnode_get(int64_t fsblk_offset, dtpage_t ** addr_buf_ptr)
{
	int rdg_rc = FSCK_OK;
	uint32_t bytes_read;
	struct recon_buf_record *bufrec_ptr;
	dtpage_t *dtp;

	rdg_rc = dire_buffer_alloc(addr_buf_ptr);

	if (rdg_rc != FSCK_OK) {
		fsck_send_msg(fsck_CANTRECONINSUFSTG, 2);
	} else {		/* got the buffer */
		bufrec_ptr = (struct recon_buf_record *) *addr_buf_ptr;
		bufrec_ptr->dnode_blkoff = fsblk_offset;
		bufrec_ptr->dnode_byteoff = fsblk_offset * sb_ptr->s_bsize;
		rdg_rc = readwrite_device(bufrec_ptr->dnode_byteoff,
					  BYTESPERPAGE,
					  &bytes_read, (void *) (*addr_buf_ptr),
					  fsck_READ);
		if (rdg_rc != FSCK_OK) {
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVREAD,
				      fsck_ref_msg(fsck_metadata), Vol_Label);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_CANTREADRECONDNODE,
				      rdg_rc, fsck_READ,
				      (long long) bufrec_ptr->dnode_byteoff,
				      BYTESPERPAGE,
				      bytes_read);
			rdg_rc = FSCK_CANTREADRECONDNODE;
			dire_buffer_release(*addr_buf_ptr);
		} else if (bytes_read != BYTESPERPAGE) {
			/*
			 * message to user
			 */
			fsck_send_msg(fsck_URCVREAD, fsck_ref_msg(fsck_metadata),
				      Vol_Label);
			/*
			 * message to debugger
			 */
			fsck_send_msg(fsck_ERRONAGG, FSCK_CANTREADRECONDNODE1,
				      rdg_rc, fsck_READ,
				      (long long) bufrec_ptr->dnode_byteoff,
				      BYTESPERPAGE,
				      bytes_read);
			rdg_rc = FSCK_CANTREADRECONDNODE1;
			dire_buffer_release(*addr_buf_ptr);
		} else {
			/* swap if on big endian machine */
			dtp = *addr_buf_ptr;
			ujfs_swap_dtpage_t(dtp, sb_ptr->s_flag);
			dtp->header.flag |= BT_SWAPPED;
		}
	}
	return (rdg_rc);
}

/*****************************************************************************
 * NAME: recon_dnode_put
 *
 * FUNCTION:  Write the dnode in the specified buffer into the aggregate
 *            and then release the buffer.
 *
 * PARAMETERS:
 *      buf_ptr  - input -  the address of the buffer containing the
 *                          dnode to write.
 *
 * NOTES:  Unlike most _put_ routines in this module, blkmap_put_ctl_page
 *         actually writes to the device.
 *
 *         The buffer has a trailer record which contains the offset
 *         at which to write the dnode.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int recon_dnode_put(dtpage_t * buf_ptr)
{
	int rdp_rc = FSCK_OK;
	uint32_t bytes_written;
	struct recon_buf_record *bufrec_ptr;

	bufrec_ptr = (struct recon_buf_record *) buf_ptr;

	/* swap if on big endian machine */
	ujfs_swap_dtpage_t(buf_ptr, sb_ptr->s_flag);
	buf_ptr->header.flag &= ~BT_SWAPPED;
	rdp_rc = readwrite_device(bufrec_ptr->dnode_byteoff,
				  BYTESPERPAGE, &bytes_written,
				  (void *) buf_ptr, fsck_WRITE);
	ujfs_swap_dtpage_t(buf_ptr, sb_ptr->s_flag);
	buf_ptr->header.flag |= BT_SWAPPED;

	if (rdp_rc != FSCK_OK) {
		/*
		 * message to user
		 */
		fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
			      Vol_Label, 17);
		/*
		 * message to debugger
		 */
		fsck_send_msg(fsck_ERRONAGG, FSCK_CANTWRITRECONDNODE,
			      rdp_rc, fsck_WRITE,
			      (long long) bufrec_ptr->dnode_byteoff,
			      BYTESPERPAGE, bytes_written);
		rdp_rc = FSCK_CANTWRITRECONDNODE;
	} else if (bytes_written != BYTESPERPAGE) {
		/*
		 * message to user
		 */
		fsck_send_msg(fsck_URCVWRT, fsck_ref_msg(fsck_metadata),
			      Vol_Label, 18);
		/*
		 * message to debugger
		 */
		fsck_send_msg(fsck_ERRONAGG, FSCK_CANTWRITRECONDNODE1,
			      rdp_rc, fsck_WRITE,
			      (long long) bufrec_ptr->dnode_byteoff,
			      BYTESPERPAGE, bytes_written);
		rdp_rc = FSCK_CANTWRITRECONDNODE1;
	}
	bufrec_ptr->dnode_blkoff = 0;
	bufrec_ptr->dnode_byteoff = 0;
	dire_buffer_release(buf_ptr);
	return (rdp_rc);
}

/*****************************************************************************
 * NAME: recon_dnode_release
 *
 * FUNCTION:  Release the specified fsck dnode buffer without writing its
 *            contents to the aggregate.
 *
 * PARAMETERS:
 *      buf_ptr  - input -  Address of the fsck dnode buffer to release.
 *
 * RETURNS:
 *      success: FSCK_OK
 *      failure: something else
 */
int recon_dnode_release(dtpage_t * buf_ptr)
{
	int rdr_rc = FSCK_OK;
	struct recon_buf_record *bufrec_ptr;

	bufrec_ptr = (struct recon_buf_record *) buf_ptr;
	bufrec_ptr->dnode_blkoff = 0;
	bufrec_ptr->dnode_byteoff = 0;
	rdr_rc = dire_buffer_release(buf_ptr);
	return (rdr_rc);
}
