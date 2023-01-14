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
 *	 dmap - display/modify disk map
 */
#include <config.h>
#include <stdlib.h>
#include <string.h>

#include "xpeek.h"
#include "jfs_filsys.h"
#include "jfs_dmap.h"
#include "jfs_endian.h"
#include "inode.h"
#include "devices.h"

/* ACTIONS */
#define DMAP_EXIT	0
#define DISPLAY_DBMAP	1
#define DISPLAY_CPAGE	2
#define DISPLAY_LEAF	3

/* DMAP BLOCK TYPES	*/
#define DMAP	-1
#define LEVEL0	0
#define LEVEL1	1
#define LEVEL2	2

/* DMAP BLOCK CALCULATIONS */
#define L0FACTOR	(LPERCTL + 1)
#define L1FACTOR	((L0FACTOR * LPERCTL) + 1)
#define L1PAGE(l1)	(2 + ((l1) * L1FACTOR))
#define L0PAGE(l1, l0)	(L1PAGE(l1) + 1 + ((l0) * L0FACTOR))
#define DMAPPAGE(l1, l0, d)	(L0PAGE(l1, l0) + 1 + (d))

/* forward references */
int decode_pagenum(int64_t page, int *l1, int *l0, int *dmap);
int display_agfree(int64_t * agfree);
int display_cpage(struct dinode *, int64_t *);
int display_dbmap(struct dbmap *, int64_t *, int64_t);
int display_leaf(struct dinode *, int64_t *);
int display_tree(struct dmapctl *, int64_t *, int *);

/* external references */
int display_map(unsigned *, int);	/* iag.c */

/* Global Data */
int dmap_level;			/* Maximum level of dtree */
int dmap_l2bpp;			/* log 2 of blocks per page */

extern unsigned type_jfs;

void dmap()
{
	int action;
	int64_t address;
	struct dinode bmap_inode;
	int64_t cntl_addr;
	struct dbmap cntl_page;
	int64_t lblock;		/* Logical block of BMAP to be displayed */

	lblock = 0;
	action = DISPLAY_DBMAP;

	/* Read block allocation map Inode */
	if (find_inode(BMAP_I, AGGREGATE_I, &address) ||
	    xRead(address, sizeof (struct dinode), (char *) &bmap_inode)) {
		fputs("dmap: Error reading block allocation map inode\n", stderr);
		return;
	}

	/* swap if on big endian machine */
	ujfs_swap_dinode(&bmap_inode, GET, type_jfs);

	/* Read overall control page for the map */

	if (ujfs_rwdaddr(fp, &cntl_addr, &bmap_inode, (int64_t) 0, GET, bsize) ||
	    xRead(cntl_addr, sizeof (struct dbmap), (char *) &cntl_page)) {
		fputs("dmap: Error reading aggregate dmap control page\n", stderr);
		return;
	}

	/* swap if on big endian machine */
	ujfs_swap_dbmap(&cntl_page);

	/* Useful stuff */
	dmap_level = BMAPSZTOLEV(cntl_page.dn_mapsize);
	dmap_l2bpp = cntl_page.dn_l2nbperpage;

	while (action != DMAP_EXIT) {
		switch (action) {
		case DISPLAY_DBMAP:
			action = display_dbmap(&cntl_page, &lblock, cntl_addr);
			break;
		case DISPLAY_CPAGE:
			action = display_cpage(&bmap_inode, &lblock);
			break;
		case DISPLAY_LEAF:
			action = display_leaf(&bmap_inode, &lblock);
			break;
		default:
			fputs("dmap: Internal Error!\n", stderr);
			return;
		}
	}
	return;
}

/*****************************************************************************
 ***********************   Sample output from display_dbmap()

Block allocation map control page at block 128

[1] dn_mapsize:		0x00000031fa0	[9] dn_agheigth:	0
[2] dn_nfree:		0x00000030ed8	[10] dn_agwidth:	1
[3] dn_l2nbperpage:	3		[11] dn_agstart:	341
[4] dn_numag:		25		[12] dn_agl2size:	13
[5] dn_maxlevel:	0		[13] dn_agfree:		type 'f'
[6] dn_maxag:		16		[14] dn_agsize:		8192
[7] dn_agpref:		16		[15] pad:		Not Displayed
[8] dn_aglevel:		0

*****************************************************************************/

int display_dbmap(struct dbmap *hdr, int64_t *lblock, int64_t hdr_addr)
{
	char cmdline[80];
	int field;
	int rc;
	char *token;

      dbmap_redisplay:
	printf("\nBlock allocation map control page at block %lld\n\n",
	       (long long) (hdr_addr >> l2bsize));
	printf("[1] dn_mapsize:\t\t0x%011llx\t", (long long) hdr->dn_mapsize);
	printf("[9] dn_agheigth:\t%d\n", hdr->dn_agheigth);
	printf("[2] dn_nfree:\t\t0x%011llx\t", (long long) hdr->dn_nfree);
	printf("[10] dn_agwidth:\t%d\n", hdr->dn_agwidth);
	printf("[3] dn_l2nbperpage:\t%d\t\t", hdr->dn_l2nbperpage);
	printf("[11] dn_agstart:\t%d\n", hdr->dn_agstart);
	printf("[4] dn_numag:\t\t%d\t\t", hdr->dn_numag);
	printf("[12] dn_agl2size:\t%d\n", hdr->dn_agl2size);
	printf("[5] dn_maxlevel:\t%d\t\t", hdr->dn_maxlevel);
	printf("[13] dn_agfree:\t\ttype 'f'\n");
	printf("[6] dn_maxag:\t\t%d\t\t", hdr->dn_maxag);
	printf("[14] dn_agsize:\t\t%lld\n", (long long) hdr->dn_agsize);
	printf("[7] dn_agpref:\t\t%d\t\t", hdr->dn_agpref);
	printf("[15] pad:\t\tNot Displayed\n");
	printf("[8] dn_aglevel:\t\t%d\n", hdr->dn_aglevel);

      dbmap_retry:
	fputs("display_dbmap: [m]odify, [f]ree count, [t]ree, e[x]it > ", stdout);
	fgets(cmdline, 80, stdin);
	token = strtok(cmdline, " 	\n");
	if ((token == 0) || (token[0] == 't')) {
		switch (dmap_level) {
		case 2:
			*lblock = 1;	/* L2 page */
			break;
		case 1:
			*lblock = 2;	/* L1 page */
			break;
		case 0:
			*lblock = 3;	/* L0 page */
			break;
		default:
			fprintf(stderr, "display_dbmap:  dmap_level = %d\n", dmap_level);
			return DMAP_EXIT;
		}
		return DISPLAY_CPAGE;
	}
	if (token[0] == 'f') {
		rc = display_agfree(hdr->dn_agfree);

		/* swap if on big endian machine */
		ujfs_swap_dbmap(hdr);

		if ((rc & XPEEK_CHANGED) && xWrite(hdr_addr, sizeof (struct dbmap), (char *) hdr)) {
			fputs("display_dbmap: error writing control header\n\n", stderr);
			/* swap if on big endian machine */
			ujfs_swap_dbmap(hdr);
			return DMAP_EXIT;
		}

		/* swap if on big endian machine */
		ujfs_swap_dbmap(hdr);

		if (rc & XPEEK_REDISPLAY)
			goto dbmap_redisplay;

		return DMAP_EXIT;
	}
	if (token[0] != 'm') {	/* assume 'x' */
		return DMAP_EXIT;
	}

	field = m_parse(cmdline, 14, &token);
	if (field == 0)
		goto dbmap_retry;

	switch (field) {
	case 1:
		hdr->dn_mapsize = strtoull(token, 0, 16);
		break;
	case 2:
		hdr->dn_nfree = strtoull(token, 0, 16);
		break;
	case 3:
		hdr->dn_l2nbperpage = strtoul(token, 0, 0);
		break;
	case 4:
		hdr->dn_numag = strtoul(token, 0, 0);
		break;
	case 5:
		hdr->dn_maxlevel = strtoul(token, 0, 0);
		break;
	case 6:
		hdr->dn_maxag = strtoul(token, 0, 0);
		break;
	case 7:
		hdr->dn_agpref = strtoul(token, 0, 0);
		break;
	case 8:
		hdr->dn_aglevel = strtoul(token, 0, 0);
		break;
	case 9:
		hdr->dn_agheigth = strtoul(token, 0, 0);
		break;
	case 10:
		hdr->dn_agwidth = strtoul(token, 0, 0);
		break;
	case 11:
		hdr->dn_agstart = strtoul(token, 0, 0);
		break;
	case 12:
		hdr->dn_agl2size = strtoul(token, 0, 0);
		break;
	case 13:
		fputs("display_dbmap: Can't change this field from here.\n", stderr);
		goto dbmap_retry;
	case 14:
		hdr->dn_agsize = strtoull(token, 0, 0);
		break;
	}

	/* swap if on big endian machine */
	ujfs_swap_dbmap(hdr);

	if (xWrite(hdr_addr, sizeof (struct dbmap), (char *) hdr)) {
		fputs("display_dbmap: error writing control header\n\n", stderr);
		/* swap back if on big endian machine */
		ujfs_swap_dbmap(hdr);
		return DMAP_EXIT;
	}

	/* swap back if on big endian machine */
	ujfs_swap_dbmap(hdr);

	goto dbmap_redisplay;
}

/***************************************************************************
 ***********************   Example Output of display_cpage()

Level 0 Control Page at block 136

[1] nleafs:    1024			[5] budmin:	13
[2] l2nleafs:  10			[6] stree:	hit <enter>
[3] leafidx:   341			[7] pad:	Not Displayed
[4] height:    5

****************************************************************************/
int display_cpage(struct dinode * bmap_inode, int64_t * lblock)
{
	int64_t address;
	int changed = 0;
	char cmdline[80];
	struct dmapctl ctl_page;
	int field;
	int rc;
	char *token;
	int dmap, l0, l1, type;

	type = decode_pagenum(*lblock, &l1, &l0, &dmap);

	if (ujfs_rwdaddr(fp, &address, bmap_inode, (*lblock) << dmap_l2bpp,
			 GET, bsize) || xRead(address, sizeof (struct dmapctl), (char *) &ctl_page)) {
		fputs("display_cpage: Error reading control page!\n", stderr);
		return DMAP_EXIT;
	}

	/* swap if on big endian machine */
	ujfs_swap_dmapctl(&ctl_page);

      cpage_redisplay:
	printf("\nLevel %d Control Page at block %lld\n\n", type, (long long) (address >> l2bsize));

	printf("[1] nleafs:\t%d\t\t\t", ctl_page.nleafs);
	printf("[5] budmin:\t%d\n", ctl_page.budmin);
	printf("[2] l2nleafs:\t%d\t\t\t", ctl_page.l2nleafs);
	printf("[6] stree:\thit <enter>\n");
	printf("[3] leafidx:\t%d\t\t\t", ctl_page.leafidx);
	printf("[7] pad:\tNot Displayed\n");
	printf("[4] height:\t%d\n", ctl_page.height);

      cpage_retry:
	fputs("[m]odify, [u]p, [r]ight or [l]eft sibling, e[x]it, [s]tree > ", stdout);

	fgets(cmdline, 80, stdin);
	token = strtok(cmdline, " 	\n");
	if ((token == 0) || (token[0] == 's')) {
		rc = display_tree(&ctl_page, lblock, &changed);

		/* swap if on big endian machine */
		ujfs_swap_dmapctl(&ctl_page);

		if (changed && xWrite(address, sizeof (struct dmapctl), (char *) &ctl_page)) {
			fputs("display_cpage: Error writing control page!\n", stderr);
			/* swap back if on big endian machine */
			ujfs_swap_dmapctl(&ctl_page);
			return DMAP_EXIT;
		}

		/* swap if on big endian machine */
		ujfs_swap_dmapctl(&ctl_page);

		return rc;
	}
	if (token[0] == 'u') {
		if (type == LEVEL2) {
			*lblock = 0;
			return DISPLAY_DBMAP;
		}
		if (type == LEVEL1) {
			if (dmap_level > 1) {
				*lblock = 1;
				return DISPLAY_CPAGE;
			} else {
				*lblock = 0;
				return DISPLAY_DBMAP;
			}
		}
		if (type == LEVEL0) {
			if (dmap_level > 0) {
				*lblock = L1PAGE(l1);
				return DISPLAY_CPAGE;
			} else {
				*lblock = 0;
				return DISPLAY_DBMAP;
			}
		}
	}
	if ((token[0] == 'r') || (token[0] == 'l')) {
		if (type == LEVEL2) {
			fputs("Level 2 node has no siblings!\n", stderr);
			goto cpage_retry;
		}
		if (type == LEVEL1) {
			if (dmap_level < 2) {
				fputs("Level 1 node has no siblings!\n", stderr);
				goto cpage_retry;
			}
			if (token[0] == 'r') {
				if (l1 == LPERCTL - 1) {
					fputs("No right sibling!\n", stderr);
					goto cpage_retry;
				}
				l1++;
			} else {
				if (l1 == 0) {
					fputs("No left sibling!\n", stderr);
					goto cpage_retry;
				}
				l1--;
			}
			*lblock = L1PAGE(l1);
			return DISPLAY_CPAGE;
		}
		if (type == LEVEL0) {
			if (dmap_level < 1) {
				fputs("Level 0 node has no siblings!\n", stderr);
				goto cpage_retry;
			}
			if (token[0] == 'r') {
				if (l0 == LPERCTL - 1) {
					if ((dmap_level == 1) || (l1 == LPERCTL - 1)) {
						fputs("No right sibling!\n", stderr);
						goto cpage_retry;
					}
					l1++;
					l0 = 0;
				} else
					l0++;
			} else {
				if (l0 == 0) {
					if (l1 == 0) {
						fputs("No left sibling!\n", stderr);
						goto cpage_retry;
					}
					l1--;
					l0 = LPERCTL - 1;
				} else
					l0--;
			}
			*lblock = L0PAGE(l1, l0);
			return DISPLAY_CPAGE;
		}
		fprintf(stderr, "display_cpage: decode_pagenum returned type %d\n", type);
		return DMAP_EXIT;
	}
	if (token[0] == 'm') {
		field = m_parse(cmdline, 5, &token);
		if (field == 0)
			goto cpage_retry;

		switch (field) {
		case 1:
			ctl_page.nleafs = strtoul(token, 0, 0);
			break;
		case 2:
			ctl_page.l2nleafs = strtoul(token, 0, 0);
			break;
		case 3:
			ctl_page.leafidx = strtoul(token, 0, 0);
			break;
		case 4:
			ctl_page.height = strtoul(token, 0, 0);
			break;
		case 5:
			ctl_page.budmin = strtoul(token, 0, 0);
			break;
		}

		/* swap if on big endian machine */
		ujfs_swap_dmapctl(&ctl_page);

		if (xWrite(address, sizeof (struct dmapctl), (char *) &ctl_page)) {
			fputs("display_cpage: Error writing control page!\n", stderr);
			/* swap back if on big endian machine */
			ujfs_swap_dmapctl(&ctl_page);
			return DMAP_EXIT;
		}

		/* swap back if on big endian machine */
		ujfs_swap_dmapctl(&ctl_page);

		goto cpage_redisplay;
	}
	return DMAP_EXIT;
}

/****************************************************************************
 ************************  Example output of display_leaf()

Dmap Page at block 144

[1] nblocks:		8192		[8] tree.budmin:	5
[2] nfree:		7624		[9] tree.stree:		Hit <enter>
[3] start:		0		[10] tree.pad:		Not Displayed
[4] tree.nleafs:	256		[11] pad:		Not Displayed
[5] tree.l2nleafs:	8		[12] wmap:		Type 'w'
[6] tree.leafidx:	85		[13] pmap:		Type 'p'
[7] tree.height:	4

 ***************************************************************************/

int display_leaf(struct dinode * bmap_inode, int64_t * lblock)
{
	int64_t address;
	int changed = 0;
	char cmdline[80];
	struct dmap d_map;
	int field;
	int rc;
	char *token;
	int dmap, l0, l1;

	decode_pagenum(*lblock, &l1, &l0, &dmap);

	if (ujfs_rwdaddr(fp, &address, bmap_inode, (*lblock) << dmap_l2bpp,
			 GET, bsize) || xRead(address, sizeof (struct dmap), (char *) &d_map)) {
		fputs("display_leaf: Error reading dmap page!\n", stderr);
		return DMAP_EXIT;
	}

	/* swap if on big endian machine */
	ujfs_swap_dmap(&d_map);

      leaf_redisplay:
	printf("\nDmap Page at block %lld\n\n", (long long) (address >> l2bsize));

	printf("[1] nblocks:\t\t%d\t\t", d_map.nblocks);
	printf("[8] tree.budmin:\t%d\n", d_map.tree.budmin);
	printf("[2] nfree:\t\t%d\t\t", d_map.nfree);
	printf("[9] tree.stree:\t\tHit <enter>\n");
	printf("[3] start:\t\t%lld\t\t", (long long) d_map.start);
	printf("[10] tree.pad:\t\tNot Displayed\n");
	printf("[4] tree.nleafs:\t%d\t\t", d_map.tree.nleafs);
	printf("[11] pad:\t\tNot Displayed\n");
	printf("[5] tree.l2nleafs:\t%d\t\t", d_map.tree.l2nleafs);
	printf("[12] wmap:\t\tType 'w'\n");
	printf("[6] tree.leafidx:\t%d\t\t", d_map.tree.leafidx);
	printf("[13] pmap:\t\tType 'p'\n");
	printf("[7] tree.height:\t%d\n", d_map.tree.height);

      leaf_retry:
	fputs("[m]odify, [u]p, [r]ight or [l]eft, [w]map, [p]map, e[x]it, [s]tree > ", stdout);
	fgets(cmdline, 80, stdin);
	token = strtok(cmdline, " 	\n");

	if ((token == 0) || (token[0] == 's')) {
		rc = display_tree((struct dmapctl *) & d_map.tree, lblock, &changed);
		/* swap if on big endian machine */
		ujfs_swap_dmap(&d_map);

		if (changed && xWrite(address, sizeof (struct dmap), (char *) &d_map)) {
			fputs("display_leaf: Error writing dmap page!\n", stderr);
			/* swap back if on big endian machine */
			ujfs_swap_dmap(&d_map);
			return DMAP_EXIT;
		}

		/* swap back if on big endian machine */
		ujfs_swap_dmap(&d_map);

		return rc;
	}
	if (token[0] == 'u') {
		*lblock = L0PAGE(l1, l0);
		return DISPLAY_CPAGE;
	}
	if (token[0] == 'r') {
		if (dmap < LPERCTL - 1) {
			*lblock = DMAPPAGE(l1, l0, dmap + 1);
			return (DISPLAY_LEAF);
		}
		if (dmap_level > 0) {
			if (l0 < LPERCTL - 1) {
				*lblock = DMAPPAGE(l1, l0 + 1, 0);
				return (DISPLAY_LEAF);
			}
			if ((dmap_level == 2) && (l1 < LPERCTL - 1)) {
				*lblock = DMAPPAGE(l1 + 1, 0, 0);
				return (DISPLAY_LEAF);
			}
		}
		fputs("display_leaf: No right sibling.\n", stderr);
		goto leaf_retry;
	}
	if (token[0] == 'l') {
		if (dmap > 0) {
			*lblock = DMAPPAGE(l1, l0, dmap - 1);
			return (DISPLAY_LEAF);
		}
		if (dmap_level > 0) {
			if (l0 > 0) {
				*lblock = DMAPPAGE(l1, l0 - 1, LPERCTL - 1);
				return (DISPLAY_LEAF);
			}
			if ((dmap_level == 2) && (l1 > 0)) {
				*lblock = DMAPPAGE(l1 - 1, LPERCTL - 1, LPERCTL - 1);
				return (DISPLAY_LEAF);
			}
		}
		fputs("display_leaf: No left sibling.\n", stderr);
		goto leaf_retry;
	}
	if ((token[0] == 'p') || (token[0] == 'w')) {
		if (token[0] == 'p')
			rc = display_map(d_map.pmap, LPERDMAP);
		else
			rc = display_map(d_map.wmap, LPERDMAP);

		/* swap if on big endian machine */
		ujfs_swap_dmap(&d_map);

		if ((rc & XPEEK_CHANGED) && xWrite(address, sizeof (struct dmap), (char *) &d_map)) {
			fputs("display_leaf: Error writing dmap page!\n", stderr);
			/* swap back if on big endian machine */
			ujfs_swap_dmap(&d_map);
			return DMAP_EXIT;
		}

		/* swap back if on big endian machine */
		ujfs_swap_dmap(&d_map);

		if (rc & XPEEK_REDISPLAY)
			goto leaf_redisplay;
		return DMAP_EXIT;
	}
	if (token[0] != 'm')	/* assume 'x'     */
		return DMAP_EXIT;

	field = m_parse(cmdline, 8, &token);
	if (field == 0)
		goto leaf_retry;

	switch (field) {
	case 1:
		d_map.nblocks = strtoul(token, 0, 0);
		break;
	case 2:
		d_map.nfree = strtoul(token, 0, 0);
		break;
	case 3:
		d_map.start = strtoull(token, 0, 0);
		break;
	case 4:
		d_map.tree.nleafs = strtoul(token, 0, 0);
		break;
	case 5:
		d_map.tree.l2nleafs = strtoul(token, 0, 0);
		break;
	case 6:
		d_map.tree.leafidx = strtoul(token, 0, 0);
		break;
	case 7:
		d_map.tree.height = strtoul(token, 0, 0);
		break;
	case 8:
		d_map.tree.budmin = strtoul(token, 0, 0);
		break;
	}
	goto leaf_redisplay;
}

/****************************************************************************
 ************************  Example output of display_leaf()

Level 4				  [ 0] 10
                              /-------/  \-------\
                     /-------/			  \-------\
            /-------/					   \-------\
           /							    \
   [  0]  9	       [  1]  8		   [  2] 10	       [  3] 10
         /\		     /\ 		 /\		     /\
       /    \		   /	\	       /    \		   /	\
     /	      \ 	 /	  \	     /	      \ 	 /	  \
   /		\      /	    \	   /		\      /	    \
  6    8    8    9    8    8    8    8   10   -1    8    9   10   -1    9    8

   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15

 ***************************************************************************/

int display_tree(struct dmapctl *ctlpage, int64_t *lblock, int *changed)
{
	char cmdline[80];
	int i, j;
	int level;
	int index;
	char *token;

	/* tree_offset[height of tree - level] is offset of first element
	   of that level */

	static int tree_offset[6] = { 0, 1, 5, 21, 85, 341 };
	int dmap, l0, l1, type;
	int offset;
	int top_index = 0;
	int top_level = ctlpage->height;

	if ((ctlpage->height < 1) || (ctlpage->height > 5)) {
		fprintf(stderr,
			"display_tree is confused.  ctlpage->height = %d\n", ctlpage->height);
		return DMAP_EXIT;
	}

	type = decode_pagenum(*lblock, &l1, &l0, &dmap);

      redisplay_tree:
	offset = tree_offset[ctlpage->height - top_level];
	printf("\nLevel %d\t\t\t\t  [%2d] %2d\n", top_level, top_index,
	       ctlpage->stree[offset + top_index]);

	i = top_index << 2;
	level = top_level - 1;
	offset = tree_offset[ctlpage->height - level];
	printf("\t\t\t      /-------/  \\-------\\\n");
	printf("\t\t     /-------/\t\t\t  \\-------\\\n");
	printf("\t    /-------/\t\t\t\t\t   \\-------\\\n");
	printf("\t   /\t\t\t\t\t\t\t    \\\n");
	printf("   [%3d] %2d\t       [%3d] %2d\t\t   [%3d] %2d\t       [%3d] %2d\n",
	       i, ctlpage->stree[offset + i],
	       i + 1, ctlpage->stree[offset + i + 1],
	       i + 2, ctlpage->stree[offset + i + 2], i + 3, ctlpage->stree[offset + i + 3]);

	if (level > 0) {
		i = top_index << 4;
		level--;
		offset = tree_offset[ctlpage->height - level];
		printf("\t /\\\t\t     /\\ \t\t /\\\t\t     /\\\n");
		printf("       /    \\\t\t   /\t\\\t       /    \\\t\t   /\t\\\n");
		printf("     /\t      \\ \t /\t  \\\t     /\t      \\ \t /\t  \\\n");
		printf("   /\t\t\\      /\t    \\\t   /\t\t\\      /\t    \\\n");
		printf(" %2d", ctlpage->stree[offset + i]);
		for (j = i + 1; j < i + 16; j++)
			printf("   %2d", ctlpage->stree[offset + j]);
		printf("\n\n%4d", i);
		for (j = i + 1; j < i + 16; j++)
			printf(" %4d", j);
		printf("\n");
	}
      tree_retry:
	fputs("\n[b]ack, [d]escend, [g]oto, [m]odify, [r]ight, [l]eft, [u]p, e[x]it > ", stdout);
	fgets(cmdline, 80, stdin);
	token = strtok(cmdline, " 	\n");
	if ((token == 0) || (token[0] == 'x'))
		return DMAP_EXIT;
	if (token[0] == 'l') {
		if (top_index == 0) {	/* Need to move to another page */
			fputs("For now, go [b]ack, then go left\n", stdout);
			goto tree_retry;
		}
		top_index--;
		goto redisplay_tree;
	}
	if (token[0] == 'r') {
		if (top_index == (1 << ((ctlpage->height - top_level) << 1)) - 1) {
			fputs("For now, [b]ack then go right\n", stdout);
			goto tree_retry;
		}
		top_index++;
		goto redisplay_tree;
	}
	if (token[0] == 'g') {
		token = strtok(0, " 	\n");
		if (token == 0) {
			fputs("Please enter: level index > ", stdout);
			fgets(cmdline, 80, stdin);
			token = strtok(cmdline, " 	\n");
			if (token == 0)
				goto tree_retry;
		}
		level = strtoul(token, 0, 0);
		token = strtok(0, " 	\n");
		if (token == 0) {
			fputs("Not enough arguments\n", stderr);
			goto tree_retry;
		}
		index = strtoul(token, 0, 0);
		if ((level < 0) || (level > ctlpage->height) || (index < 0) ||
		    (index >= (1 << ((ctlpage->height - level) << 1)))) {
			fputs("Invalid level and/or index\n", stderr);
			goto tree_retry;
		}
		if ((level == 1) && ctlpage->height > 1) {
			level++;
			index >>= 2;
		} else if (level == 0) {
			if (ctlpage->height == 1) {	/* is this possible? */
				level++;
				index >>= 2;
			} else {
				level += 2;
				index >>= 4;
			}
		}
		top_level = level;
		top_index = index;
		goto redisplay_tree;
	}
	if (token[0] == 'u') {
		if (top_level == ctlpage->height) {	/* At top of tree */
			fputs("Already at top of tree.\n", stdout);
			goto tree_retry;
		}
		/* Move up one level */
		top_level++;
		top_index >>= 2;
		goto redisplay_tree;
	}
	if (token[0] == 'd') {
		if (type == DMAP) {
			fputs("[d]escend only valid for control pages\n", stderr);
			goto tree_retry;
		}
		token = strtok(0, " 	\n");
		if (token == 0) {
			fputs("Please enter: leaf# > ", stdout);
			fgets(cmdline, 80, stdin);
			token = strtok(cmdline, " 	\n");
			if (token == 0)
				goto tree_retry;
		}
		index = strtoul(token, 0, 0);
		if ((index < 0) || (index >= ctlpage->nleafs)) {
			fputs("Invalid leaf index\n", stderr);
			goto tree_retry;
		}
		if (type == LEVEL0) {
			*lblock = DMAPPAGE(l1, l0, index);
			return DISPLAY_LEAF;
		}
		if (type == LEVEL1)
			*lblock = L0PAGE(l1, index);
		else		/* LEVEL2 */
			*lblock = L1PAGE(index);

		return DISPLAY_CPAGE;
	}
	if (token[0] != 'm') {	/* Assuming 'b' */
		if (type == DMAP)
			return DISPLAY_LEAF;
		else
			return DISPLAY_CPAGE;
	}
	token = strtok(0, " 	\n");
	if (token == 0) {
		fputs("Please enter: level index value > ", stdout);
		fgets(cmdline, 80, stdin);
		token = strtok(0, " 	\n");
		if (token == 0)
			goto tree_retry;
	}
	level = strtol(token, 0, 0);
	token = strtok(0, " 	\n");
	if (token == 0) {
		fputs("Not enough arguments!\n", stderr);
		goto tree_retry;
	}
	index = strtol(token, 0, 0);
	token = strtok(0, " 	\n");
	if (token == 0) {
		fputs("Not enough arguments!\n", stderr);
		goto tree_retry;
	}
	if ((level < 0) || (level > ctlpage->height) || (index < 0) ||
	    (index >= (1 << ((ctlpage->height - level) << 1)))) {
		fputs("Invalid level and/or index\n", stderr);
		goto tree_retry;
	}
	ctlpage->stree[tree_offset[ctlpage->height - level] + index] = strtol(token, 0, 0);
	*changed = 1;
	goto redisplay_tree;
}

int decode_pagenum(int64_t page, int *l1, int *l0, int *dmap)
{
	int remainder;

	if (page == 0)
		return -1;

	if (page == 1)
		return LEVEL2;

	*l1 = (page - 2) / L1FACTOR;
	remainder = (page - 2) % L1FACTOR;
	if (remainder == 0)
		return LEVEL1;

	*l0 = (remainder - 1) / L0FACTOR;
	remainder = (remainder - 1) % L0FACTOR;
	if (remainder == 0)
		return LEVEL0;

	*dmap = remainder - 1;
	return DMAP;
}

int display_agfree(int64_t * agfree)
{
	char cmdline[80];
	int end;
	int i;
	int index;
	int rc = XPEEK_OK;
	int start = 0;
	char *token;

      agfree_display:
	end = MIN(start + 64, MAXAG);
	for (i = start; i < end; i += 4)
		printf("%3d 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n", i, (long long) agfree[i],
		       (long long) agfree[i + 1], (long long) agfree[i + 2],
		       (long long) agfree[i + 3]);
      agfree_retry:
	fputs("display_agfree: [m]odify, [b]ack, e[x]it > ", stdout);
	fgets(cmdline, 80, stdin);
	token = strtok(cmdline, " 	\n");
	if (token == 0) {
		start = (end < MAXAG) ? end : 0;
		goto agfree_display;
	}
	if (token[0] == 'x')
		return rc;
	if (token[0] != 'm') {	/* assuming 'b' */
		return (rc | XPEEK_REDISPLAY);
	}

	index = m_parse(cmdline, MAXAG - 1, &token);
	if (index == 0)
		goto agfree_retry;

	agfree[index] = strtoull(token, 0, 16);
	rc = XPEEK_CHANGED;
	goto agfree_display;
}
