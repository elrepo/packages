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
#ifndef H_XFSCKINT
#define H_XFSCKINT

#include <stdio.h>
#include "xfsck.h"
#include "fsck_message.h"
#include "fsckpfs.h"
#include "fsckwsp.h"
#include "jfs_endian.h"
 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  * Device information.
  *
  *      defined in xchkdsk.c
  */
extern FILE *Dev_IOPort;
extern uint32_t Dev_blksize;
extern int32_t Dev_SectorSize;
extern int64_t ondev_jlog_byte_offset;

/*
  ---------------------- functions defined in dirtable ---------------------
*/

int allocate_dir_index_buffers(void);

void verify_dir_index(struct fsck_inode_record *, struct dinode *,
		      struct dtreeQelem *, int, uint);

/*
  ---------------------- functions defined in fsckbmap ---------------------
*/

int rebuild_blkall_map(void);

int verify_blkall_map(void);

/*
  ---------------------- functions defined in fsckconn ---------------------
*/

int adjust_parents(struct fsck_inode_record *, uint32_t);

int check_connectedness(void);

int check_dir_integrity(void);

int check_link_counts(void);

/*
  ---------------------- functions defined in fsckdire ---------------------
*/

int fsck_dtDelete(struct dinode *, struct component_name *, uint32_t *);

int fsck_dtInsert(struct dinode *, struct component_name *, uint32_t *);

/*
  ---------------------- functions defined in fsckdtre ---------------------
*/

int direntry_add(struct dinode *, uint32_t, UniChar *);

int direntry_get_inonum(uint32_t, int, UniChar *, int, UniChar *, uint32_t *);

int direntry_get_objnam(uint32_t, uint32_t, int *, UniChar *);

int direntry_remove(struct dinode *, uint32_t);

int dTree_processing(struct dinode *, uint32_t, struct fsck_inode_record *, struct fsck_ino_msg_info *, int);

int dTree_search(struct dinode *, UniChar *, uint32_t, UniChar *,
		 uint32_t, struct dtslot **, int8_t *, struct fsck_inode_record *);

int find_first_dir_leaf(struct dinode *, dtpage_t **, int64_t *, int8_t *, int8_t *);

void init_dir_tree(dtroot_t *);

int process_valid_dir_data(struct dinode *, uint32_t, struct fsck_inode_record *,
			   struct fsck_ino_msg_info *, int);

int reconnect_fs_inodes(void);

int rebuild_dir_index(struct dinode *, struct fsck_inode_record *);

/*
  ---------------------- functions defined in fsckimap ---------------------
*/

int AIS_redundancy_check(void);

int AIS_replication(void);

int rebuild_agg_iamap(void);

int rebuild_fs_iamaps(void);

int record_dupchk_inode_extents(void);

int verify_agg_iamap(void);

int verify_fs_iamaps(void);

/*
  ---------------------- functions defined in fsckino ---------------------
*/

#define inode_type_recognized(iptr)\
              ( ISDIR(iptr->di_mode) || \
                ISREG(iptr->di_mode) || \
                ISLNK(iptr->di_mode) || \
                ISBLK(iptr->di_mode) || \
                ISCHR(iptr->di_mode) || \
                ISFIFO(iptr->di_mode)|| \
                ISSOCK(iptr->di_mode)       )

int backout_ACL(struct dinode *, struct fsck_inode_record *);

int backout_EA(struct dinode *, struct fsck_inode_record *);

int clear_ACL_field(struct fsck_inode_record *, struct dinode *);

int clear_EA_field(struct fsck_inode_record *, struct dinode *);

int display_path(uint32_t, int, uint32_t, char *, struct fsck_inode_record *);

int display_paths(uint32_t, struct fsck_inode_record *, struct fsck_ino_msg_info *);

int first_ref_check_inode(struct dinode *, uint32_t, struct fsck_inode_record *, struct fsck_ino_msg_info *);

int get_path(uint32_t, uint32_t, char **, struct fsck_inode_record *);

int in_inode_data_check(struct fsck_inode_record *, struct fsck_ino_msg_info *);

int inode_is_in_use(struct dinode *, uint32_t);

int parent_count(struct fsck_inode_record *);

int record_valid_inode(struct dinode *, uint32_t, struct fsck_inode_record *, struct fsck_ino_msg_info *);

int release_inode(uint32_t, struct fsck_inode_record *, struct dinode *);

int unrecord_valid_inode(struct dinode *, uint32_t, struct fsck_inode_record *, struct fsck_ino_msg_info *);

int validate_ACL(struct dinode *, uint32_t, struct fsck_inode_record *, struct fsck_ino_msg_info *);

int validate_data(struct dinode *, uint32_t, struct fsck_inode_record *, struct fsck_ino_msg_info *);

int validate_dir_data(struct dinode *, uint32_t, struct fsck_inode_record *, struct fsck_ino_msg_info *);

int validate_EA(struct dinode *, uint32_t, struct fsck_inode_record *, struct fsck_ino_msg_info *);

int validate_record_fileset_inode(uint32_t, uint32_t, struct dinode *, struct fsck_ino_msg_info *);

/*
  ---------------------- functions defined in fsckmeta ---------------------
*/

int agg_clean_or_dirty(void);

int fatal_dup_check(void);

int first_ref_check_agg_metadata(void);

int first_ref_check_fixed_metadata(void);

int first_ref_check_fs_metadata(void);

int first_ref_check_inode_extents(void);

int record_fixed_metadata(void);

int replicate_superblock(void);

int validate_fs_metadata(void);

int validate_repair_superblock(void);

int validate_select_agg_inode_table(void);

/*
  ---------------------- functions defined in fsckpfs ---------------------
*/

int ait_node_get(int64_t, xtpage_t *);

int ait_node_put(int64_t, xtpage_t *);

int ait_special_read_ext1(int);

int blkmap_find_bit(int64_t, int64_t *, uint32_t *, uint32_t *);

int blkmap_flush(void);

int blkmap_get_ctl_page(struct fsck_blk_map_hdr *);

int blkmap_get_page(int64_t, struct fsck_blk_map_page **);

int blkmap_put_ctl_page(struct fsck_blk_map_hdr *);

int blkmap_put_page(int64_t);

int blktbl_ctl_page_put(struct dbmap *);

int blktbl_dmap_get(int64_t, struct dmap **);

int blktbl_dmap_put(struct dmap *);

int blktbl_dmaps_flush(void);

int blktbl_Ln_page_get(int8_t, int64_t, struct dmapctl **);

int blktbl_Ln_page_put(struct dmapctl *);

int blktbl_Ln_pages_flush(void);

int close_volume(void);

int default_volume(void);

int dnode_get(int64_t, uint32_t, dtpage_t **);

int ea_get(int64_t, uint32_t, char *, uint32_t *, uint32_t *, int64_t *);

int flush_index_pages(void);

int fscklog_put_buffer(void);

int iag_get(int, int, int, int32_t, struct iag **);

int iag_get_first(int, int, int, struct iag **);

int iag_get_next(struct iag **);

int iag_put(struct iag *);

int iags_flush(void);

int inodes_flush(void);

int inode_get(int, int, uint32_t, struct dinode **);

int inode_get_first_fs(int, uint32_t *, struct dinode **);

int inode_get_next(uint32_t *, struct dinode **);

int inode_put(struct dinode *);

int inotbl_get_ctl_page(int, struct dinomap **);

int inotbl_put_ctl_page(int, struct dinomap *);

int mapctl_get(int64_t, void **);

int mapctl_put(void *);

int mapctl_flush(void);

void modify_index(struct dinode *, int64_t, int, uint);

int node_get(int64_t, xtpage_t **);

int open_volume(char *);

int readwrite_device(int64_t, uint32_t, uint32_t *, void *, int);

int recon_dnode_assign(int64_t, dtpage_t **);

int recon_dnode_get(int64_t, dtpage_t **);

int recon_dnode_put(dtpage_t *);

int recon_dnode_release(dtpage_t *);

/*
  ---------------------- functions defined in fsckruns ---------------------
*/

void fsck_hbeat_start(void);

void fsck_hbeat_stop(void);

/*
  ---------------------- functions defined in fsckwsp ---------------------
*/

int alloc_vlarge_buffer(void);

int alloc_wrksp(uint32_t, int, int, void **);	/* called from both fsck modules
						 * and from logredo modules
						 */

int blkall_decrement_owners(int64_t);

int blkall_increment_owners(int64_t, int64_t, struct fsck_ino_msg_info *);

int blkall_ref_check(int64_t, int64_t);

int dire_buffer_alloc(dtpage_t **);

int dire_buffer_release(dtpage_t *);

int directory_buffers_alloc(void);

int directory_buffers_release(void);

int dtreeQ_dequeue(struct dtreeQelem **);

int dtreeQ_enqueue(struct dtreeQelem *);

int dtreeQ_get_elem(struct dtreeQelem **);

int dtreeQ_rel_elem(struct dtreeQelem *);

int establish_agg_workspace(void);

int establish_ea_iobuf(void);

int establish_fs_workspace(void);

int establish_io_buffers(void);

int establish_wsp_block_map_ctl(void);

int extent_record(int64_t, int64_t);

int extent_unrecord(int64_t, int64_t);

int fsck_alloc_fsblks(int32_t, int64_t *);

int fsck_dealloc_fsblks(int32_t, int64_t);

int fscklog_end(void);

int fscklog_init(void);

int fscklog_start(void);

int get_inode_extension(struct fsck_inode_ext_record **);

int get_inorecptr(int, int, uint32_t, struct fsck_inode_record **);

int get_inorecptr_first(int, uint32_t *, struct fsck_inode_record **);

int get_inorecptr_next(int, uint32_t *, struct fsck_inode_record **);

int init_agg_record(void);

int process_extent(struct fsck_inode_record *, uint32_t, int64_t, int8_t,
		   int8_t, struct fsck_ino_msg_info *, uint32_t *, int8_t *, int);

int release_inode_extension(struct fsck_inode_ext_record *);

int release_logredo_allocs(void);

int temp_inode_buf_alloc(char **);

int temp_inode_buf_release(char *);

int temp_node_buf_alloc(char **);

int temp_node_buf_release(char *);

int treeQ_dequeue(struct treeQelem **);

int treeQ_enqueue(struct treeQelem *);

int treeQ_get_elem(struct treeQelem **);

int treeQ_rel_elem(struct treeQelem *);

int workspace_release(void);

/*
  ---------------------- functions defined in fsckxtre ---------------------
*/

int find_first_leaf(struct dinode *, xtpage_t **, int64_t *, int8_t *, int8_t *);

int init_xtree_root(struct dinode *);

int process_valid_data(struct dinode *, uint32_t, struct fsck_inode_record *, struct fsck_ino_msg_info *, int);

int xTree_processing(struct dinode *, uint32_t, struct fsck_inode_record *, struct fsck_ino_msg_info *, int);

int xTree_search(struct dinode *, int64_t, xad_t **, int8_t *);

/*
  ---------------------- functions defined in xchkdsk.c ---------------------
*/

void report_readait_error(int, int, int);

#endif
