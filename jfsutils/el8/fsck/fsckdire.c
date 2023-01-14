/*
 *   Copyright (C) International Business Machines Corp., 2000-2005
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
 *   (created from dtree.c: directory B+-tree manager for fsck)
 */

#include <config.h>
#include <errno.h>
#include <string.h>

#include "xfsckint.h"
#include "jfs_byteorder.h"
#include "jfs_unicode.h"

extern struct superblock *sb_ptr;

 /* + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
  *
  * fsck aggregate info structure pointer
  *
  *      defined in xchkdsk.c
  */
extern struct fsck_agg_record *agg_recptr;

/*
 *	btree traversal stack
 *
 * record the path traversed during the search;
 * top frame record the leaf page/entry selected.
 */
#define	MAXTREEHEIGHT		8
struct btframe {		/* stack frame */
	int64_t bn;		/*  8: */
	int16_t index;		/*  2: */
	int16_t lastindex;	/*  2: */
	struct btpage *bp;	/*  4: */
};				/* (16) */

struct btstack {
	struct btframe *top;	/* 4: */
	int32_t nsplit;		/* 4: */
	struct btframe stack[MAXTREEHEIGHT];
};

#define BT_CLR(btstack)\
	(btstack)->top = (btstack)->stack

#define BT_PUSH(BTSTACK, BN, INDEX)\
{\
	(BTSTACK)->top->bn = BN;\
	(BTSTACK)->top->index = INDEX;\
	++(BTSTACK)->top;\
}

#define BT_POP(btstack)\
	( (btstack)->top == (btstack)->stack ? NULL : --(btstack)->top )

#define BT_STACK(btstack)\
	( (btstack)->top == (btstack)->stack ? NULL : (btstack)->top )

/* retrieve search results */
#define BT_GETSEARCH(IP, LEAF, BN, BP, TYPE, P, INDEX)\
{\
	BN = (LEAF)->bn;\
	BP = (LEAF)->bp;\
	if (BN)\
		P = (TYPE *)BP;\
	else\
		P = (TYPE *)&IP->di_btroot;\
	INDEX = (LEAF)->index;\
}

/* put the page buffer of search */
#define BT_PUTSEARCH(BTSTACK)\
{\
	if ((BTSTACK)->top->bn)\
		recon_dnode_put((BTSTACK)->top->bp);\
}

/* get page from buffer page */
#define FSCK_BT_PAGE(IP, BP, TYPE)\
	(BP->flag & BT_ROOT) ? (TYPE *)&IP->di_btroot : (TYPE *)BP

/* get the page buffer and the page for specified block address */
#define FSCK_BT_GETPAGE(IP, BN, BP, TYPE, P, RC)\
{\
	if ((BN) == 0)\
	{\
		BP = (struct btpage *)&IP->di_btroot;\
		P = (TYPE *)&IP->di_btroot;\
		RC = 0;\
	}\
	else\
	{\
		RC = recon_dnode_get(BN, (dtpage_t **)&BP);\
		if (RC == 0)\
			P = (TYPE *)BP;\
	}\
}

/* put the page buffer */
#define FSCK_BT_PUTPAGE(BP)\
{\
	if (!((BP)->flag & BT_ROOT))\
		recon_dnode_put((dtpage_t *)BP);\
}

#define DO_INDEX() (sb_ptr->s_flag & JFS_DIR_INDEX)

/* dtree split parameter */
struct dtsplit {
	struct btpage *bp;
	int16_t index;
	int16_t nslot;
	struct component_name *key;
	ddata_t *data;
	struct pxdlist *pxdlist;
};

/*
 * forward references
 */
static int dtSplitUp(struct dinode *ip, struct dtsplit *split,
		     struct btstack *btstack);

static int dtSplitPage(struct dinode *ip, struct dtsplit *split,
		       struct btpage **rbpp, pxd_t * rxdp);

static int dtSplitRoot(struct dinode *ip, struct dtsplit *split,
		       struct btpage **rbpp);

static int fsck_dtDeleteUp(struct dinode *ip, struct btpage *fbp,
			   struct btstack *btstack);

static int dtRelink(struct dinode *ip, dtpage_t * p);

static int dtCompare(struct component_name *key, dtpage_t * p, int32_t si);

static void dtGetKey(dtpage_t * p, int32_t i, struct component_name *key);

static int ciCompare(struct component_name *key, dtpage_t * p, int32_t si,
		     int32_t flag);

static void ciGetLeafPrefixKey(dtpage_t * lp, int32_t li, dtpage_t * rp,
			       int32_t ri, struct component_name *key,
			       int32_t flag);

#define ciToUpper(c)   UniStrupr((c)->name)

static void dtInsertEntry(struct dinode *ip, dtpage_t * p, int32_t index,
			  struct component_name *key, ddata_t * data);

static void dtMoveEntry(dtpage_t * sp, int32_t si, dtpage_t * dp);

static void fsck_dtDeleteEntry(dtpage_t * p, int32_t fi);

void fsck_dtInitRoot(struct dinode *ip, uint32_t idotdot);

/* copy memory */
#define bcopy(source, dest, count)	memcpy(dest, source, count)

/*
 *	fsck_dtSearch()
 *
 * function:
 *	Search for the entry with specified key
 *
 * parameter:
 *
 * return: 0 - search result on stack, leaf page pinned;
 *	   errno - I/O error
 */
int fsck_dtSearch(struct dinode *ip,
		  struct component_name *key,
		  uint32_t * data, struct btstack *btstack, uint32_t flag)
{
	int rc = 0;
	int32_t cmp = 1;	/* init for empty page */
	int64_t bn;
	struct btpage *bp;
	dtpage_t *p = 0;
	int8_t *stbl;
	int32_t base, index, lim;
	struct btframe *btsp;
	pxd_t *pxd;
	uint32_t inumber;
	UniChar ciKeyName[JFS_NAME_MAX + 1];
	struct component_name ciKey = { 0, ciKeyName };

	/* uppercase search key for c-i directory */
	UniStrcpy(ciKeyName, key->name);
	ciKey.namlen = key->namlen;

	/* only uppercase if case-insensitive support is on */

	if ((sb_ptr->s_flag & JFS_OS2) == JFS_OS2) {
		ciToUpper(&ciKey);
	}

	BT_CLR(btstack);	/* reset stack */

	/* init level count for max pages to split */
	btstack->nsplit = 1;

	/*
	 *      search down tree from root:
	 *
	 * between two consecutive entries of <Ki, Pi> and <Kj, Pj> of
	 * internal page, child page Pi contains entry with k, Ki <= K < Kj.
	 *
	 * if entry with search key K is not found
	 * internal page search find the entry with largest key Ki
	 * less than K which point to the child page to search;
	 * leaf page search find the entry with smallest key Kj
	 * greater than K so that the returned index is the position of
	 * the entry to be shifted right for insertion of new entry.
	 * for empty tree, search key is greater than any key of the tree.
	 *
	 * by convention, root bn = 0.
	 */
	for (bn = 0;;) {
		/* get/pin the page to search */
		FSCK_BT_GETPAGE(ip, bn, bp, dtpage_t, p, rc);
		if (rc)
			return rc;

		/* get sorted entry table of the page */
		stbl = DT_GETSTBL(p);

		/*
		 * binary search with search key K on the current page.
		 */
		for (base = 0, lim = p->header.nextindex; lim; lim >>= 1) {
			index = base + (lim >> 1);

			if (p->header.flag & BT_LEAF) {
				/* uppercase leaf name to compare */
				cmp =
				    ciCompare(&ciKey, p, stbl[index],
					      sb_ptr->s_flag);
			} else {
				/* router key is in uppercase */
				cmp = dtCompare(&ciKey, p, stbl[index]);
			}

			if (cmp == 0) {
				/* search hit - leaf page:
				 * return the entry found
				 */
				if (p->header.flag & BT_LEAF) {
					inumber =
					    ((struct ldtentry *) &p->
					     slot[stbl[index]])->inumber;

					/*
					 * search for JFS_CREATE
					 */
					if (flag == JFS_CREATE) {
						*data = inumber;
						rc = EEXIST;
						goto out;
					}

					/*
					 * search for JFS_REMOVE
					 */
					if (flag == JFS_REMOVE
					    && *data != inumber) {
						rc = ESTALE;
						goto out;
					}

					/*
					 * JFS_REMOVE
					 */
					/* save search result */
					*data = inumber;
					btsp = btstack->top;
					btsp->bn = bn;
					btsp->index = index;
					btsp->bp = bp;

					return 0;
				}

				/* search hit - internal page:
				 * descend/search its child page
				 */
				goto getChild;
			}

			if (cmp > 0) {
				base = index + 1;
				--lim;
			}
		}

		/*
		 *      search miss
		 *
		 * base is the smallest index with key (Kj) greater than
		 * search key (K) and may be zero or (maxindex + 1) index.
		 */
		/*
		 * search miss - leaf page
		 *
		 * return location of entry (base) where new entry with
		 * search key K is to be inserted.
		 */
		if (p->header.flag & BT_LEAF) {
			/*
			 * search for JFS_LOOKUP, JFS_REMOVE, or JFS_RENAME
			 */
			if (flag == JFS_REMOVE) {
				rc = ENOENT;
				goto out;
			}

			/*
			 * search for JFS_CREATE
			 *
			 * save search result
			 */
			*data = 0;
			btsp = btstack->top;
			btsp->bn = bn;
			btsp->index = base;
			btsp->bp = bp;

			return 0;
		}

		/*
		 * search miss - internal page
		 *
		 * if base is non-zero, decrement base by one to get the parent
		 * entry of the child page to search.
		 */
		index = base ? base - 1 : base;

		/*
		 * go down to child page
		 */
	      getChild:
		/* update max. number of pages to split */
		/* assert(btstack->nsplit < 8); */
		btstack->nsplit++;

		/* push (bn, index) of the parent page/entry */
		BT_PUSH(btstack, bn, index);

		/* get the child page block number */
		pxd = (pxd_t *) & p->slot[stbl[index]];
		bn = addressPXD(pxd);

		/* unpin the parent page */
		FSCK_BT_PUTPAGE(bp);
	}

      out:
	FSCK_BT_PUTPAGE(bp);
	return rc;
}

/*
 *	fsck_dtInsert()
 *
 * function: insert an entry to directory tree
 *
 * parameter:
 *	ip	- parent directory
 *	name 	- entry name;
 *	fsn	- entry i_number;
 *
 * return: 0 - success;
 *	   errno - failure;
 */
int fsck_dtInsert(struct dinode *ip, struct component_name *name,
		  uint32_t * fsn)
{
	int rc = 0;
	struct btpage *bp;	/* page buffer */
	dtpage_t *p;		/* base B+-tree index page */
	int64_t bn;
	int32_t index;
	struct dtsplit split;	/* split information */
	ddata_t data;
	int32_t n;
	struct btstack btstack;
	uint32_t ino;

	/*
	 *      search for the entry to insert:
	 *
	 * fsck_dtSearch() returns (leaf page pinned, index at which to insert).
	 */
	rc = fsck_dtSearch(ip, name, &ino, &btstack, JFS_CREATE);
	if (rc)
		return rc;

	/*
	 *      retrieve search result
	 *
	 * fsck_dtSearch() returns (leaf page pinned, index at which to insert).
	 * n.b. fsck_dtSearch() may return index of (maxindex + 1) of
	 * the full page.
	 */
	BT_GETSEARCH(ip, btstack.top, bn, bp, dtpage_t, p, index);

	/*
	 *      insert entry for new key
	 */
	if (DO_INDEX())
		n = NDTLEAF(name->namlen);
	else
		n = NDTLEAF_LEGACY(name->namlen);
	data.leaf.ino = *fsn;

	/*
	 *      leaf page does not have enough room for new entry:
	 *
	 *      extend/split the leaf page;
	 *
	 * dtSplitUp() will insert the entry and unpin the leaf page.
	 */
	if (n > p->header.freecnt) {
		split.bp = bp;
		split.index = index;
		split.nslot = n;
		split.key = name;
		split.data = &data;
		rc = dtSplitUp(ip, &split, &btstack);
		return rc;
	}

	/*
	 *      leaf page does have enough room for new entry:
	 *
	 *      insert the new data entry into the leaf page;
	 */
	dtInsertEntry(ip, p, index, name, &data);

	/* unpin the leaf page */
	FSCK_BT_PUTPAGE(bp);

	return 0;
}

/*
 *	dtSplitUp()
 *
 * function: propagate insertion bottom up;
 *
 * parameter:
 *
 * return: 0 - success;
 *	   errno - failure;
 * 	leaf page unpinned;
 */
static int dtSplitUp(struct dinode *ip, struct dtsplit *split,
		     struct btstack *btstack)
{
	int rc = 0;
	struct btpage *sbp;
	dtpage_t *sp;		/* split page */
	struct btpage *rbp;
	dtpage_t *rp;		/* new right page split from sp */
	pxd_t rpxd;		/* new right page extent descriptor */
	struct btpage *lbp;
	dtpage_t *lp;		/* left child page */
	int32_t skip;		/* index of entry of insertion */
	struct btframe *parent;	/* parent page entry on traverse stack */
	int64_t xaddr;
	int32_t xlen;
	struct pxdlist pxdlist;
	pxd_t *pxd;
	UniChar name[JFS_NAME_MAX + 1];
	struct component_name key = { 0, name };
	ddata_t *data = split->data;
	int32_t n;

	/* get split page */
	sbp = split->bp;
	sp = FSCK_BT_PAGE(ip, sbp, dtpage_t);

	/*
	 *      split leaf page
	 *
	 *  The split routines insert the new entry
	 *
	 *      split root leaf page:
	 */
	if (sp->header.flag & BT_ROOT) {
		/*
		 * allocate a single extent child page
		 */
		xlen = agg_recptr->blksperpg;
		rc = fsck_alloc_fsblks(xlen, &xaddr);
		if (rc)
			return rc;

		pxdlist.maxnpxd = 1;
		pxdlist.npxd = 0;
		pxd = &pxdlist.pxd[0];
		PXDaddress(pxd, xaddr);
		PXDlength(pxd, xlen);
		split->pxdlist = &pxdlist;
		rc = dtSplitRoot(ip, split, &rbp);

		FSCK_BT_PUTPAGE(rbp);
		FSCK_BT_PUTPAGE(sbp);

		ip->di_size = xlen << agg_recptr->log2_blksize;

		return rc;
	}

	/*
	 *      split leaf page <sp> into <sp> and a new right page <rp>.
	 *
	 *  return <rp> pinned and its extent descriptor <rpxd>
	 *
	 *  allocate new directory page extent and
	 *  new index page(s) to cover page split(s)
	 *
	 *  allocation hint: ?
	 */
	n = btstack->nsplit;
	pxdlist.maxnpxd = pxdlist.npxd = 0;
	xlen = agg_recptr->blksperpg;
	for (pxd = pxdlist.pxd; n > 0; n--, pxd++) {
		rc = fsck_alloc_fsblks(xlen, &xaddr);
		if (rc == 0) {
			PXDaddress(pxd, xaddr);
			PXDlength(pxd, xlen);
			pxdlist.maxnpxd++;
			continue;
		}

		FSCK_BT_PUTPAGE(sbp);

		/* undo allocation */
		goto splitOut;
	}

	split->pxdlist = &pxdlist;
	rc = dtSplitPage(ip, split, &rbp, &rpxd);
	if (rc) {
		FSCK_BT_PUTPAGE(sbp);

		/* undo allocation */
		goto splitOut;
	}

	ip->di_size += PSIZE;

	/*
	 * propagate up the router entry for the leaf page just split
	 *
	 * insert a router entry for the new page into the parent page,
	 * propagate the insert/split up the tree by walking back the stack
	 * of (bn of parent page, index of child page entry in parent page)
	 * that were traversed during the search for the page that split.
	 *
	 * the propagation of insert/split up the tree stops if the root
	 * splits or the page inserted into doesn't have to split to hold
	 * the new entry.
	 *
	 * the parent entry for the split page remains the same, and
	 * a new entry is inserted at its right with the first key and
	 * block number of the new right page.
	 *
	 * There are a maximum of 4 pages pinned at any time:
	 * two children, left parent and right parent (when the parent splits).
	 * keep the child pages pinned while working on the parent.
	 * make sure that all pins are released at exit.
	 */
	while ((parent = BT_POP(btstack)) != NULL) {
		/* parent page specified by stack frame <parent> */
		/* keep current child pages (<lp>, <rp>) pinned */
		lbp = sbp;
		lp = sp;
		rp = FSCK_BT_PAGE(ip, rbp, dtpage_t);

		/*
		 * insert router entry in parent for new right child page <rp>
		 *
		 * get the parent page <sp>
		 */
		FSCK_BT_GETPAGE(ip, parent->bn, sbp, dtpage_t, sp, rc);
		if (rc) {
			FSCK_BT_PUTPAGE(lbp);
			FSCK_BT_PUTPAGE(rbp);
			goto splitOut;
		}

		/*
		 * The new key entry goes ONE AFTER the index of parent entry,
		 * because the split was to the right.
		 */
		skip = parent->index + 1;

		/*
		 * compute the key for the router entry
		 *
		 * key suffix compression:
		 * for internal pages that have leaf pages as children,
		 * retain only what's needed to distinguish between
		 * the new entry and the entry on the page to its left.
		 * If the keys compare equal, retain the entire key.
		 *
		 * note that compression is performed only at computing
		 * router key at the lowest internal level.
		 * further compression of the key between pairs of higher
		 * level internal pages loses too much information and
		 * the search may fail.
		 * (e.g., two adjacent leaf pages of {a, ..., x} {xx, ...,}
		 * results in two adjacent parent entries (a)(xx).
		 * if split occurs between these two entries, and
		 * if compression is applied, the router key of parent entry
		 * of right page (x) will divert search for x into right
		 * subtree and miss x in the left subtree.)
		 *
		 * the entire key must be retained for the next-to-leftmost
		 * internal key at any level of the tree, or search may fail
		 * (e.g., ?)
		 */
		switch (rp->header.flag & BT_TYPE) {
		case BT_LEAF:
			/*
			 * compute the length of prefix for suffix compression
			 * between last entry of left page and first entry
			 * of right page
			 */
			if ((sp->header.flag & BT_ROOT && skip > 1) ||
			    sp->header.prev != 0 || skip > 1) {
				/* compute uppercase router prefix key */
				ciGetLeafPrefixKey(lp, lp->header.nextindex - 1,
						   rp, 0, &key, sb_ptr->s_flag);
			} else {
				/* next to leftmost entry of lowest internal level */
				/* compute uppercase router key */
				dtGetKey(rp, 0, &key);

				if ((sb_ptr->s_flag & JFS_OS2) == JFS_OS2)
					ciToUpper(&key);
			}

			n = NDTINTERNAL(key.namlen);
			break;

		case BT_INTERNAL:
			dtGetKey(rp, 0, &key);
			n = NDTINTERNAL(key.namlen);
			break;

		default:
#ifdef _JFS_DEBUG
			printf("dtSplitUp(): UFO!\n");
#endif
			break;
		}

		/* unpin left child page */
		FSCK_BT_PUTPAGE(lbp);

		/*
		 * compute the data for the router entry
		 */
		/* child page xd */
		data->xd = rpxd;

		/*
		 * parent page is full - split the parent page
		 */
		if (n > sp->header.freecnt) {
			/* init for parent page split */
			split->bp = sbp;
			/* index at insert */
			split->index = skip;
			split->nslot = n;
			split->key = &key;
			/* split->data = data; */

			/* unpin right child page */
			FSCK_BT_PUTPAGE(rbp);

			/* The split routines insert the new entry,
			 * acquire txLock as appropriate.
			 * return <rp> pinned and its block number <rbn>.
			 */
			rc = (sp->header.flag & BT_ROOT) ?
			    dtSplitRoot(ip, split, &rbp) : dtSplitPage(ip,
								       split,
								       &rbp,
								       &rpxd);
			if (rc) {
				FSCK_BT_PUTPAGE(sbp);
				goto splitOut;
			}

			/* sbp and rbp are pinned */
		} else {
			/*
			 * parent page is not full - insert router entry in parent page
			 */
			dtInsertEntry(ip, sp, skip, &key, data);

			/* exit propagate up */
			break;
		}
	}

	/* unpin current split and its right page */
	FSCK_BT_PUTPAGE(sbp);
	FSCK_BT_PUTPAGE(rbp);

	/*
	 * free remaining extents allocated for split
	 */
      splitOut:
	n = pxdlist.npxd;
	pxd = &pxdlist.pxd[n];
	for (; n < pxdlist.maxnpxd; n++, pxd++)
		fsck_dealloc_fsblks(lengthPXD(pxd), (int64_t) addressPXD(pxd));

	return rc;
}

/*
 *	dtSplitPage()
 *
 * function: Split a non-root page of a btree.
 *
 * parameter:
 *
 * return: 0 - success;
 *	   errno - failure;
 *	return split and new page pinned;
 */
static int dtSplitPage(struct dinode *ip, struct dtsplit *split,
		       struct btpage **rbpp, pxd_t * rpxdp)
{
	int rc = 0;
	struct btpage *sbp;
	dtpage_t *sp;
	struct btpage *rbp;
	dtpage_t *rp;		/* new right page allocated */
	int64_t rbn;		/* new right page block number */
	struct btpage *bp;
	dtpage_t *p = 0;
	int64_t nextbn;
	struct pxdlist *pxdlist;
	pxd_t *pxd;
	int32_t skip, nextindex, half, left, nxt, off, si;
	struct ldtentry *ldtentry;
	struct idtentry *idtentry;
	uint8_t *stbl;
	struct dtslot *f;
	int32_t fsi, stblsize;
	int32_t n;

	/* get split page */
	sbp = split->bp;
	sp = FSCK_BT_PAGE(ip, sbp, dtpage_t);

	/*
	 * allocate the new right page for the split
	 */
	pxdlist = split->pxdlist;
	pxd = &pxdlist->pxd[pxdlist->npxd];
	pxdlist->npxd++;
	rbn = addressPXD(pxd);
	recon_dnode_assign(rbn, (dtpage_t **) & rbp);
	/* rbp->b_lblkno = rbn; */
#ifdef _JFS_DEBUG
	printf("split: ip:0x%08x sbp:0x%08x rbp:0x%08x\n", ip, sbp, rbp);
#endif
	rp = (dtpage_t *) rbp;
	rp->header.self = *pxd;

	/*
	 * initialize/update sibling pointers between sp and rp
	 */
	nextbn = sp->header.next;
	rp->header.next = nextbn;
	rp->header.prev = addressPXD(&sp->header.self);
	sp->header.next = rbn;

	/*
	 * initialize new right page
	 */
	rp->header.flag = sp->header.flag;

	/* compute sorted entry table at start of extent data area */
	rp->header.nextindex = 0;
	rp->header.stblindex = 1;

	n = PSIZE >> L2DTSLOTSIZE;
	rp->header.maxslot = n;
	/* in unit of slot */
	stblsize = (n + 31) >> L2DTSLOTSIZE;

	/* init freelist */
	fsi = rp->header.stblindex + stblsize;
	rp->header.freelist = fsi;
	rp->header.freecnt = rp->header.maxslot - fsi;

	/*
	 *      sequential append at tail: append without split
	 *
	 * If splitting the last page on a level because of appending
	 * a entry to it (skip is maxentry), it's likely that the access is
	 * sequential. Adding an empty page on the side of the level is less
	 * work and can push the fill factor much higher than normal.
	 * If we're wrong it's no big deal, we'll just do the split the right
	 * way next time.
	 * (It may look like it's equally easy to do a similar hack for
	 * reverse sorted data, that is, split the tree left,
	 * but it's not. Be my guest.)
	 */
	if (nextbn == 0 && split->index == sp->header.nextindex) {
		/*
		 * initialize freelist of new right page
		 */
		f = &rp->slot[fsi];
		for (fsi++; fsi < rp->header.maxslot; f++, fsi++)
			f->next = fsi;
		f->next = -1;

		/* insert entry at the first entry of the new right page */
		dtInsertEntry(ip, rp, 0, split->key, split->data);

		goto out;
	}

	/*
	 *      non-sequential insert (at possibly middle page)
	 *
	 *  update prev pointer of previous right sibling page;
	 */
	if (nextbn != 0) {
		FSCK_BT_GETPAGE(ip, nextbn, bp, dtpage_t, p, rc);
		if (rc) {
			recon_dnode_release((dtpage_t *) rbp);
			return rc;
		}

		p->header.prev = rbn;

		FSCK_BT_PUTPAGE(bp);
	}

	/*
	 * split the data between the split and right pages.
	 */
	skip = split->index;
	half = (PSIZE >> L2DTSLOTSIZE) >> 1;	/* swag */
	left = 0;

	/*
	 *      compute fill factor for split pages
	 *
	 *  <nxt> traces the next entry to move to rp
	 *  <off> traces the next entry to stay in sp
	 */
	stbl = (uint8_t *) & sp->slot[sp->header.stblindex];
	nextindex = sp->header.nextindex;
	for (nxt = off = 0; nxt < nextindex; ++off) {
		if (off == skip)
			/* check for fill factor with new entry size */
			n = split->nslot;
		else {
			si = stbl[nxt];
			switch (sp->header.flag & BT_TYPE) {
			case BT_LEAF:
				ldtentry = (struct ldtentry *) &sp->slot[si];
				if (DO_INDEX())
					n = NDTLEAF(ldtentry->namlen);
				else
					n = NDTLEAF_LEGACY(ldtentry->namlen);
				break;

			case BT_INTERNAL:
				idtentry = (struct idtentry *) &sp->slot[si];
				n = NDTINTERNAL(idtentry->namlen);
				break;

			default:
				break;
			}

			++nxt;	/* advance to next entry to move in sp */
		}

		left += n;
		if (left >= half)
			break;
	}

	/* <nxt> poins to the 1st entry to move */

	/*
	 *      move entries to right page
	 *
	 *  dtMoveEntry() initializes rp and reserves entry for insertion
	 */
	dtMoveEntry(sp, nxt, rp);

	sp->header.nextindex = nxt;

	/*
	 * finalize freelist of new right page
	 */
	fsi = rp->header.freelist;
	f = &rp->slot[fsi];
	for (fsi++; fsi < rp->header.maxslot; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	/*
	 * Update directory index table for entries now in right page
	 */
	if ((rp->header.flag & BT_LEAF) && DO_INDEX()) {
		stbl = DT_GETSTBL(rp);
		for (n = 0; n < rp->header.nextindex; n++) {
			ldtentry = (struct ldtentry *) &rp->slot[stbl[n]];
			modify_index(ip, rbn, n, ldtentry->index);
		}
	}

	/*
	 * the skipped index was on the left page,
	 */
	if (skip <= off) {
		/* insert the new entry in the split page */
		dtInsertEntry(ip, sp, skip, split->key, split->data);
	} else {
		/*
		 * the skipped index was on the right page,
		 */
		/* adjust the skip index to reflect the new position */
		skip -= nxt;

		/* insert the new entry in the right page */
		dtInsertEntry(ip, rp, skip, split->key, split->data);
	}

      out:
	*rbpp = rbp;
	*rpxdp = *pxd;

#ifdef _JFS_DEBUG
	printf("split: ip:0x%08x sbp:0x%08x rbp:0x%08x\n", ip, sbp, rbp);
#endif

	ip->di_nblocks += lengthPXD(pxd);
	return 0;
}

/*
 *	dtSplitRoot()
 *
 * function:
 *	split the full root page into
 *	original/root/split page and new right page
 *	i.e., root remains fixed in tree anchor (inode) and
 *	the root is copied to a single new right child page
 *	since root page << non-root page, and
 *	the split root page contains a single entry for the
 *	new right child page.
 *
 * parameter:
 *
 * return: 0 - success;
 *	   errno - failure;
 *	return new page pinned;
 */
static int dtSplitRoot(struct dinode *ip, struct dtsplit *split,
		       struct btpage **rbpp)
{
	dtroot_t *sp;
	struct btpage *rbp;
	dtpage_t *rp;
	int64_t rbn;
	int32_t xlen;
	int32_t xsize;
	struct dtslot *f;
	int8_t *stbl;
	int32_t fsi, stblsize, n;
	struct idtentry *s;
	pxd_t *ppxd;
	struct pxdlist *pxdlist;
	pxd_t *pxd;

	/* get split root page */
	sp = (dtroot_t *) & ip->di_btroot;

	/*
	 *      allocate/initialize a single (right) child page
	 *
	 * N.B. In normal processing,
	 *             at first split, a one (or two) block to fit
	 *             new entry is allocated; at subsequent split,
	 *             a full page is allocated;
	 *      During fsck processing,
	 *             at first split a full page is allocated.
	 */
	pxdlist = split->pxdlist;
	pxd = &pxdlist->pxd[pxdlist->npxd];
	pxdlist->npxd++;
	rbn = addressPXD(pxd);
	xlen = lengthPXD(pxd);
	xsize = xlen << agg_recptr->log2_blksize;
	recon_dnode_assign(rbn, (dtpage_t **) & rbp);
	/* rbp->b_lblkno = rbn; */

	rp = (dtpage_t *) rbp;
	rp->header.flag = (sp->header.flag & BT_LEAF) ? BT_LEAF : BT_INTERNAL;
	rp->header.self = *pxd;

	/* initialize sibling pointers */
	rp->header.next = 0;
	rp->header.prev = 0;

	/*
	 *      move in-line root page into new right page extent
	 */
	n = xsize >> L2DTSLOTSIZE;
	rp->header.maxslot = n;
	stblsize = (n + 31) >> L2DTSLOTSIZE;

	/* copy old stbl to new stbl at start of extended area */
	rp->header.stblindex = DTROOTMAXSLOT;
	stbl = (int8_t *) & rp->slot[DTROOTMAXSLOT];
	bcopy(sp->header.stbl, stbl, sp->header.nextindex);
	rp->header.nextindex = sp->header.nextindex;

	/* copy old data area to start of new data area */
	bcopy(&sp->slot[1], &rp->slot[1], IDATASIZE);

	/*
	 * append free region of newly extended area at tail of freelist
	 */
	/* init free region of newly extended area */
	fsi = n = DTROOTMAXSLOT + stblsize;
	f = &rp->slot[fsi];
	for (fsi++; fsi < rp->header.maxslot; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	/* append new free region at tail of old freelist */
	fsi = sp->header.freelist;
	if (fsi == -1)
		rp->header.freelist = n;
	else {
		rp->header.freelist = fsi;

		do {
			f = &rp->slot[fsi];
			fsi = f->next;
		} while (fsi != -1);

		f->next = n;
	}

	rp->header.freecnt = sp->header.freecnt + rp->header.maxslot - n;

	/*
	 * Update directory index table for entries now in right page;
	 */
	if ((rp->header.flag & BT_LEAF) && DO_INDEX()) {
		struct ldtentry *ldtentry;

		stbl = DT_GETSTBL(rp);
		for (n = 0; n < rp->header.nextindex; n++) {
			ldtentry= (struct ldtentry *) &rp->slot[stbl[n]];
			modify_index(ip, rbn, n, ldtentry->index);
		}
	}
	/*
	 * insert the new entry into the new right/child page
	 * (skip index in the new right page will not change)
	 */
	dtInsertEntry(ip, rp, split->index, split->key, split->data);

	/*
	 *      reset parent/root page
	 *
	 * set the 1st entry offset to 0, which force the left-most key
	 * at any level of the tree to be less than any search key.
	 *
	 * The btree comparison code guarantees that the left-most key on any
	 * level of the tree is never used, so it doesn't need to be filled in.
	 */
	/* update page header of root */
	if (sp->header.flag & BT_LEAF) {
		sp->header.flag &= ~BT_LEAF;
		sp->header.flag |= BT_INTERNAL;
	}

	/* init the first entry */
	s = (struct idtentry *) &sp->slot[DTENTRYSTART];
	ppxd = (pxd_t *) s;
	*ppxd = *pxd;
	s->next = -1;
	s->namlen = 0;

	stbl = sp->header.stbl;
	stbl[0] = DTENTRYSTART;
	sp->header.nextindex = 1;

	/* init freelist */
	fsi = DTENTRYSTART + 1;
	f = &sp->slot[fsi];

	/* init free region of remaining area */
	for (fsi++; fsi < DTROOTMAXSLOT; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	sp->header.freelist = DTENTRYSTART + 1;
	sp->header.freecnt = DTROOTMAXSLOT - (DTENTRYSTART + 1);

	*rbpp = rbp;

	ip->di_nblocks += lengthPXD(pxd);
	return 0;
}

/*
 *	fsck_dtDelete()
 *
 * function: delete the entry(s) referenced by a key.
 *
 * parameter:
 *	ip	- parent directory
 *	key 	- entry name;
 *	ino	- entry i_number;
 *
 * return:
 */
int fsck_dtDelete(struct dinode *ip, struct component_name *key, uint32_t * ino)
{
	int rc = 0;
	int64_t bn;
	struct btpage *bp;
	dtpage_t *p;
	int32_t index;
	struct btstack btstack;

	/*
	 *      search for the entry to delete:
	 *
	 * fsck_dtSearch() returns (leaf page pinned, index at which to delete).
	 */
	rc = fsck_dtSearch(ip, key, ino, &btstack, JFS_REMOVE);
	if (rc)
		return rc;

	/* retrieve search result */
	BT_GETSEARCH(ip, btstack.top, bn, bp, dtpage_t, p, index);

	/*
	 * the leaf page becomes empty, delete the page
	 */
	if (p->header.nextindex == 1) {
		/* delete empty page */
		rc = fsck_dtDeleteUp(ip, bp, &btstack);
	} else {
		/*
		 * the leaf page has other entries remaining:
		 *
		 * delete the entry from the leaf page.
		 */
		/* free the leaf entry */
		fsck_dtDeleteEntry(p, index);

		/*
		 * Update directory index table for entries moved in stbl
		 */
		if (DO_INDEX() && index < p->header.nextindex) {
			int i;
			struct ldtentry *ldtentry;
			int8_t *stbl;
			stbl = DT_GETSTBL(p);
			for (i = index; i < p->header.nextindex; i++) {
				ldtentry = (struct ldtentry *)&p->slot[stbl[i]];
				modify_index(ip, bn, i, ldtentry->index);
			}
		}

		FSCK_BT_PUTPAGE(bp);
	}

	return rc;
}

/*
 *	fsck_dtDeleteUp()
 *
 * function:
 *	free empty pages as propagating deletion up the tree
 *
 * parameter:
 *
 * return:
 */
static int fsck_dtDeleteUp(struct dinode *ip, struct btpage *fbp,
			   struct btstack *btstack)
{
	int rc = 0;
	struct btpage *bp;
	dtpage_t *fp, *p = 0;
	int32_t index, nextindex;
	int64_t xaddr;
	int32_t xlen;
	struct btframe *parent;

	/* get page to delete */
	fp = FSCK_BT_PAGE(ip, fbp, dtpage_t);

	/*
	 *      keep the root leaf page which has become empty
	 */
	if (fp->header.flag & BT_ROOT) {
		/*
		 * reset the root
		 *
		 * fsck_dtInitRoot() acquires txlock on the root
		 */
		fsck_dtInitRoot(ip, ip->di_parent);

		FSCK_BT_PUTPAGE(fbp);

		return 0;
	}

	/*
	 *      free the non-root leaf page
	 */
	/* update sibling pointers */
	rc = dtRelink(ip, fp);
	if (rc)
		return rc;

	xaddr = addressPXD(&fp->header.self);
	xlen = lengthPXD(&fp->header.self);
	ip->di_nblocks -= xlen;

	/* free backing extent */
	fsck_dealloc_fsblks(xlen, xaddr);

	/* free/invalidate its buffer page */
	recon_dnode_release((dtpage_t *) fbp);

	/*
	 *      propagate page deletion up the directory tree
	 *
	 * If the delete from the parent page makes it empty,
	 * continue all the way up the tree.
	 * stop if the root page is reached (which is never deleted) or
	 * if the entry deletion does not empty the page.
	 */
	while ((parent = BT_POP(btstack)) != NULL) {
		/* pin the parent page <sp> */
		FSCK_BT_GETPAGE(ip, parent->bn, bp, dtpage_t, p, rc);
		if (rc)
			return rc;

		/*
		 * free the extent of the child page deleted
		 */
		index = parent->index;

		/*
		 * delete the entry for the child page from parent
		 */
		nextindex = p->header.nextindex;

		/*
		 * the parent has the single entry being deleted:
		 *
		 * free the parent page which has become empty.
		 */
		if (nextindex == 1) {
			/*
			 * keep the root internal page which has become empty
			 */
			if (p->header.flag & BT_ROOT) {
				/*
				 * reset the root
				 *
				 * fsck_dtInitRoot() acquires txlock on the root
				 */
				fsck_dtInitRoot(ip, ip->di_parent);

				FSCK_BT_PUTPAGE(bp);

				return 0;
			} else {
				/*
				 * free the parent page
				 */
				/* update sibling pointers */
				rc = dtRelink(ip, p);
				if (rc)
					return rc;

				xaddr = addressPXD(&p->header.self);
				xlen = lengthPXD(&p->header.self);
				ip->di_nblocks -= xlen;

				/* free backing extent */
				fsck_dealloc_fsblks(xlen, xaddr);

				/* free/invalidate its buffer page */
				recon_dnode_release((dtpage_t *) bp);

				/* propagate up */
				continue;
			}
		}

		/*
		 * the parent has other entries remaining:
		 *
		 * delete the router entry from the parent page.
		 */
		/* free the router entry */
		fsck_dtDeleteEntry(p, index);

		/* unpin the parent page */
		FSCK_BT_PUTPAGE(bp);

		/* exit propagation up */
		break;
	}

	ip->di_size -= PSIZE;
	return 0;
}

/*
 *	dtRelink()
 *
 * function:
 *	link around a freed page.
 *
 * parameter:
 *	fp:	page to be freed
 *
 * return:
 */
static int dtRelink(struct dinode *ip, dtpage_t * p)
{
	int rc;
	struct btpage *bp;
	int64_t nextbn, prevbn;

	nextbn = p->header.next;
	prevbn = p->header.prev;

	/* update prev pointer of the next page */
	if (nextbn != 0) {
		FSCK_BT_GETPAGE(ip, nextbn, bp, dtpage_t, p, rc);
		if (rc)
			return rc;

		p->header.prev = prevbn;
		FSCK_BT_PUTPAGE(bp);
	}

	/* update next pointer of the previous page */
	if (prevbn != 0) {
		FSCK_BT_GETPAGE(ip, prevbn, bp, dtpage_t, p, rc);
		if (rc)
			return rc;

		p->header.next = nextbn;
		FSCK_BT_PUTPAGE(bp);
	}

	return 0;
}

/*
 *	fsck_dtInitRoot()
 *
 * initialize directory root (inline in inode)
 */
void fsck_dtInitRoot(struct dinode *ip, uint32_t idotdot)
{
	dtroot_t *p;
	int32_t fsi;
	struct dtslot *f;

	p = (dtroot_t *) & ip->di_btroot;

	p->header.flag = DXD_INDEX | BT_ROOT | BT_LEAF;

	p->header.nextindex = 0;

	/* init freelist */
	fsi = 1;
	f = &p->slot[fsi];

	/* init data area of root */
	for (fsi++; fsi < DTROOTMAXSLOT; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	p->header.freelist = 1;
	p->header.freecnt = 8;

	/* init '..' entry in btroot */
	p->header.idotdot = idotdot;

	ip->di_size = IDATASIZE;

	return;
}

/*
 *	dtCompare()
 *
 * function: compare search key with an (leaf/internal) entry
 *
 * return:
 *	< 0 if k is < record
 *	= 0 if k is = record
 *	> 0 if k is > record
 */
static int dtCompare(struct component_name *key,	/* search key */
		     dtpage_t * p,	/* directory page */
		     int32_t si)
{				/* entry slot index */
	int rc;
	UniChar *kname, *name;
	int32_t klen, namlen, len;
	struct ldtentry *lh;
	struct idtentry *ih;
	struct dtslot *t;

	/*
	 * force the left-most key on internal pages, at any level of
	 * the tree, to be less than any search key.
	 * this obviates having to update the leftmost key on an internal
	 * page when the user inserts a new key in the tree smaller than
	 * anything that has been stored.
	 *
	 * (? if/when fsck_dtSearch() narrows down to 1st entry (index = 0),
	 * at any internal page at any level of the tree,
	 * it descends to child of the entry anyway -
	 * ? make the entry as min size dummy entry)
	 *
	 * if (e->index == 0 && h->prevpg == P_INVALID && !(h->flags & BT_LEAF))
	 * return (1);
	 */

	kname = key->name;
	klen = key->namlen;

	/*
	 * leaf page entry
	 */
	if (p->header.flag & BT_LEAF) {
		lh = (struct ldtentry *) &p->slot[si];
		si = lh->next;
		name = lh->name;
		namlen = lh->namlen;
		if (DO_INDEX())
			len = MIN(namlen, DTLHDRDATALEN);
		else
			len = MIN(namlen, DTLHDRDATALEN_LEGACY);
	} else {
		/*
		 * internal page entry
		 */
		ih = (struct idtentry *) &p->slot[si];
		si = ih->next;
		name = ih->name;
		namlen = ih->namlen;
		len = MIN(namlen, DTIHDRDATALEN);
	}

	/* compare with head/only segment */
	len = MIN(klen, len);
	rc = *kname - *name;
	if (rc)
		return rc;
	rc = UniStrncmp(kname, name, len);
	if (rc)
		return rc;

	klen -= len;
	namlen -= len;

	/* compare with additional segment(s) */
	kname += len;
	while (klen > 0 && namlen > 0) {
		/* compare with next name segment */
		t = (struct dtslot *) &p->slot[si];
		len = MIN(namlen, DTSLOTDATALEN);
		len = MIN(klen, len);
		name = t->name;
		rc = *kname - *name;
		if (rc)
			return rc;
		rc = UniStrncmp(kname, name, len);
		if (rc)
			return rc;

		klen -= len;
		namlen -= len;
		kname += len;
		si = t->next;
	}

	return (klen - namlen);
}

/*
 *	ciCompare()
 *
 * function: compare search key with an (leaf/internal) entry
 *
 * return:
 *	< 0 if k is < record
 *	= 0 if k is = record
 *	> 0 if k is > record
 */
static int ciCompare(struct component_name *key,	/* search key */
		     dtpage_t * p,	/* directory page */
		     int32_t si,	/* entry slot index */
		     int32_t flag)
{
	int rc;
	UniChar *kname, *name, x;
	int32_t klen, namlen, len;
	struct ldtentry *lh;
	struct idtentry *ih;
	struct dtslot *t;
	int32_t i;

	/*
	 * force the left-most key on internal pages, at any level of
	 * the tree, to be less than any search key.
	 * this obviates having to update the leftmost key on an internal
	 * page when the user inserts a new key in the tree smaller than
	 * anything that has been stored.
	 *
	 * (? if/when fsck_dtSearch() narrows down to 1st entry (index = 0),
	 * at any internal page at any level of the tree,
	 * it descends to child of the entry anyway -
	 * ? make the entry as min size dummy entry)
	 *
	 * if (e->index == 0 && h->prevpg == P_INVALID && !(h->flags & BT_LEAF))
	 * return (1);
	 */

	kname = key->name;
	klen = key->namlen;

	/*
	 * leaf page entry
	 */
	if (p->header.flag & BT_LEAF) {
		lh = (struct ldtentry *) &p->slot[si];
		si = lh->next;
		name = lh->name;
		namlen = lh->namlen;
		if (DO_INDEX())
			len = MIN(namlen, DTLHDRDATALEN);
		else
			len = MIN(namlen, DTLHDRDATALEN_LEGACY);
	} else {
		/*
		 * internal page entry
		 */
		ih = (struct idtentry *) &p->slot[si];
		si = ih->next;
		name = ih->name;
		namlen = ih->namlen;
		len = MIN(namlen, DTIHDRDATALEN);
	}

	/* compare with head/only segment */
	len = MIN(klen, len);
	for (i = 0; i < len; i++, kname++, name++) {
		/* uppercase the characer to match */

		if ((flag & JFS_OS2) == JFS_OS2)
			x = UniToupper(*name);	/* uppercase the characer to match */
		else
			x = *name;	/* leave the character alone */
		rc = *kname - x;
		if (rc)
			return rc;
	}

	klen -= len;
	namlen -= len;

	/* compare with additional segment(s) */
	while (klen > 0 && namlen > 0) {
		/* compare with next name segment */
		t = (struct dtslot *) &p->slot[si];
		len = MIN(namlen, DTSLOTDATALEN);
		len = MIN(klen, len);
		name = t->name;
		for (i = 0; i < len; i++, kname++, name++) {

			if ((flag & JFS_OS2) == JFS_OS2)
				x = UniToupper(*name);	/* uppercase the characer to match */
			else
				x = *name;	/* leave the character alone */
			rc = *kname - x;
			if (rc)
				return rc;
		}

		klen -= len;
		namlen -= len;
		si = t->next;
	}

	return (klen - namlen);
}

/*
 *	ciGetLeafPrefixKey()
 *
 * function: compute prefix of suffix compression
 *	     from two adjacent leaf entries
 *	     across page boundary
 *
 * return:
 *	Number of prefix bytes needed to distinguish b from a.
 */
static void ciGetLeafPrefixKey(dtpage_t * lp,	/* left page */
			       int32_t li,	/* left entry index */
			       dtpage_t * rp,	/* right page */
			       int32_t ri,	/* right entry index */
			       struct component_name *key, int32_t flag)
{
	int32_t klen, namlen;
	UniChar *pl, *pr, *kname;
	UniChar lname[JFS_NAME_MAX + 1];
	struct component_name lkey = { 0, lname };
	UniChar rname[JFS_NAME_MAX + 1];
	struct component_name rkey = { 0, rname };

	/* get left and right key */
	dtGetKey(lp, li, &lkey);

	if ((flag & JFS_OS2) == JFS_OS2)
		ciToUpper(&lkey);

	dtGetKey(rp, ri, &rkey);

	if ((flag & JFS_OS2) == JFS_OS2)
		ciToUpper(&rkey);

	/* compute prefix */
	klen = 0;
	kname = key->name;
	namlen = MIN(lkey.namlen, rkey.namlen);
	for (pl = lkey.name, pr = rkey.name; namlen;
	     pl++, pr++, namlen--, klen++, kname++) {

		*kname = *pr;
		if (*pl != *pr) {
			key->namlen = klen + 1;
			return;
		}
	}

	/* l->namlen <= r->namlen since l <= r */
	if (lkey.namlen < rkey.namlen) {
		*kname = *pr;
		key->namlen = klen + 1;
	} else			/* l->namelen == r->namelen */
		key->namlen = klen;

	return;
}

/*
 *	dtGetKey()
 *
 * function: get key of the entry
 */
static void dtGetKey(dtpage_t * p, int32_t i,	/* entry index */
		     struct component_name *key)
{
	int32_t si;
	int8_t *stbl;
	struct ldtentry *lh;
	struct idtentry *ih;
	struct dtslot *t;
	int32_t namlen, len;
	UniChar *name, *kname;

	/* get entry */
	stbl = DT_GETSTBL(p);
	si = stbl[i];
	if (p->header.flag & BT_LEAF) {
		lh = (struct ldtentry *) &p->slot[si];
		si = lh->next;
		namlen = lh->namlen;
		name = lh->name;
		if (DO_INDEX())
			len = MIN(namlen, DTLHDRDATALEN);
		else
			len = MIN(namlen, DTLHDRDATALEN_LEGACY);
	} else {
		ih = (struct idtentry *) &p->slot[si];
		si = ih->next;
		namlen = ih->namlen;
		name = ih->name;
		len = MIN(namlen, DTIHDRDATALEN);
	}

	key->namlen = namlen;
	kname = key->name;

	/*
	 *  move head/only segment
	 */
	UniStrncpy(kname, name, len);
	/*
	 *  bcopy(name, kname, len);
	 */

	/*
	 * move additional segment(s)
	 */
	while (si >= 0) {
		/* get next segment */
		t = &p->slot[si];
		kname += len;
		namlen -= len;
		len = MIN(namlen, DTSLOTDATALEN);
		UniStrncpy(kname, t->name, len);
		/*
		 * bcopy(t->name, kname, len);
		 */

		si = t->next;
	}
}

/*
 *	dtInsertEntry()
 *
 * function: allocate free slot(s) and
 *	     write a leaf/internal entry
 *
 * return: entry slot index
 */
static void dtInsertEntry(struct dinode *ip,
			  dtpage_t * p,	/* directory page */
			  int32_t index,	/* new entry index */
			  struct component_name *key,	/* key */
			  ddata_t * data)
{				/* data */
	struct dtslot *h;
	struct dtslot *t;
	struct ldtentry *lh = NULL;
	struct idtentry *ih = NULL;
	int32_t hsi, fsi, klen, len, nextindex;
	UniChar *kname, *name;
	int8_t *stbl;
	pxd_t *xd;

	klen = key->namlen;
	kname = key->name;

	/* allocate a free slot */
	hsi = fsi = p->header.freelist;
	h = &p->slot[fsi];
	p->header.freelist = h->next;
	--p->header.freecnt;

	/* write head/only segment */
	if (p->header.flag & BT_LEAF) {
		lh = (struct ldtentry *) h;
		lh->next = h->next;
		lh->inumber = data->leaf.ino;
		lh->namlen = klen;
		name = lh->name;
		if (DO_INDEX()) {
			len = MIN(klen, DTLHDRDATALEN);
			lh->index = 0;	/* Until we create index properly */
		} else
			len = MIN(klen, DTLHDRDATALEN_LEGACY);
	} else {
		ih = (struct idtentry *) h;
		ih->next = h->next;
		xd = (pxd_t *) ih;
		*xd = data->xd;
		ih->namlen = klen;
		name = ih->name;
		len = MIN(klen, DTIHDRDATALEN);
	}

	UniStrncpy(name, kname, len);
	/*
	 * bcopy(kname, name, len);
	 */

	/* write additional segment(s) */
	t = h;
	klen -= len;
	while (klen) {
		/* get free slot */
		fsi = p->header.freelist;
		t = &p->slot[fsi];
		p->header.freelist = t->next;
		--p->header.freecnt;

		kname += len;
		len = MIN(klen, DTSLOTDATALEN);
		UniStrncpy(t->name, kname, len);
		/*
		 * bcopy(kname, t->name, len);
		 */

		klen -= len;
	}

	/* terminate last/only segment */
	if (h == t) {
		/* single segment entry */
		if (p->header.flag & BT_LEAF)
			lh->next = -1;
		else
			ih->next = -1;
	} else
		/* multi-segment entry */
		t->next = -1;

	/* if insert into middle, shift right succeeding entries in stbl */
	stbl = DT_GETSTBL(p);
	nextindex = p->header.nextindex;
	if (index < nextindex) {
		memmove(stbl + index + 1, stbl + index, nextindex - index);
		/* Fix up directory index table */
		if ((p->header.flag & BT_LEAF) && DO_INDEX()) {
			int i;
			int64_t bn = 0;
			if (!(p->header.flag & BT_ROOT))
				bn = addressPXD(&p->header.self);
			for (i = index + 1; i <= nextindex; i++) {
				lh = (struct ldtentry *) &(p->slot[stbl[i]]);
				modify_index(ip, bn, i, lh->index);
			}
		}
	}

	stbl[index] = hsi;

	/* advance next available entry index of stbl */
	++p->header.nextindex;
}

/*
 *	dtMoveEntry()
 *
 * function: move entries from split/left page to new/right page
 *
 *	nextindex of dst page and freelist/freecnt of both pages
 *	are updated.
 */
static void dtMoveEntry(dtpage_t * sp,	/* src page */
			int32_t si,	/* src start entry index to move */
			dtpage_t * dp)
{				/* dst page */
	int32_t ssi, next;	/* src slot index */
	int32_t di;		/* dst entry index */
	int32_t dsi;		/* dst slot index */
	int8_t *sstbl, *dstbl;	/* sorted entry table */
	int32_t snamlen, len;
	struct ldtentry *slh;
	struct ldtentry *dlh = NULL;
	struct idtentry *sih;
	struct idtentry *dih = NULL;
	struct dtslot *h;
	struct dtslot *s;
	struct dtslot *d;
	int32_t nd;
	int32_t sfsi;

	sstbl = (int8_t *) & sp->slot[sp->header.stblindex];
	dstbl = (int8_t *) & dp->slot[dp->header.stblindex];

	dsi = dp->header.freelist;	/* first (whole page) free slot */
	sfsi = sp->header.freelist;

	/*
	 * move entries
	 */
	nd = 0;
	for (di = 0; si < sp->header.nextindex; si++, di++) {
		ssi = sstbl[si];
		dstbl[di] = dsi;

		/*
		 * move head/only segment of an entry
		 */
		/* get dst slot */
		h = d = &dp->slot[dsi];

		/* get src slot and move */
		s = &sp->slot[ssi];
		if (sp->header.flag & BT_LEAF) {
			/* get source entry */
			slh = (struct ldtentry *) s;
			snamlen = slh->namlen;

			if (DO_INDEX())
				len = MIN(snamlen, DTLHDRDATALEN);
			else
				len = MIN(snamlen, DTLHDRDATALEN_LEGACY);
			dlh = (struct ldtentry *) h;
			bcopy(slh, dlh, 6 + len * 2);
			/*
			 * bcopy(slh, dlh, 6 + len);
			 */

			next = slh->next;

			/* update dst head/only segment next field */
			dsi++;
			dlh->next = dsi;
		} else {
			sih = (struct idtentry *) s;
			snamlen = sih->namlen;

			len = MIN(snamlen, DTIHDRDATALEN);
			dih = (struct idtentry *) h;
			bcopy(sih, dih, 10 + len * 2);
			/*
			 * bcopy(sih, dih, 10 + len);
			 */

			next = sih->next;

			dsi++;
			dih->next = dsi;
		}

		/* free src head/only segment */
		s->next = sfsi;
		s->cnt = 1;
		sfsi = ssi;

		nd++;

		/*
		 * move additional segment(s) of the entry
		 */
		snamlen -= len;
		while ((ssi = next) >= 0) {
			/* get next source segment */
			s = &sp->slot[ssi];

			/* get next destination free slot */
			d++;

			len = MIN(snamlen, DTSLOTDATALEN);
			UniStrncpy(d->name, s->name, len);
			/*
			 * bcopy(s->name, d->name, len);
			 */

			nd++;

			dsi++;
			d->next = dsi;

			/* free source segment */
			next = s->next;
			s->next = sfsi;
			s->cnt = 1;
			sfsi = ssi;

			snamlen -= len;
		}		/* end while */

		/* terminate dst last/only segment */
		if (h == d) {
			/* single segment entry */
			if (dp->header.flag & BT_LEAF)
				dlh->next = -1;
			else
				dih->next = -1;
		} else
			/* multi-segment entry */
			d->next = -1;
	}			/* end for */

	/* update source header */
	sp->header.freelist = sfsi;
	sp->header.freecnt += nd;

	/* update destination header */
	dp->header.nextindex = di;

	dp->header.freelist = dsi;
	dp->header.freecnt -= nd;
}

/*
 *	fsck_dtDeleteEntry()
 *
 * function: free a (leaf/internal) entry
 *
 * log freelist header, stbl, and each segment slot of entry
 * (even though last/only segment next field is modified,
 * physical image logging requires all segment slots of
 * the entry logged to avoid applying previous updates
 * to the same slots)
 */
static void fsck_dtDeleteEntry(dtpage_t * p,	/* directory page */
			       int32_t fi)
{				/* free entry index */
	int32_t fsi;		/* free entry slot index */
	int8_t *stbl;
	struct dtslot *t;
	int32_t si, freecnt;

	/* get free entry slot index */
	stbl = DT_GETSTBL(p);
	fsi = stbl[fi];

	/* get the head/only segment */
	t = &p->slot[fsi];
	if (p->header.flag & BT_LEAF)
		si = ((struct ldtentry *) t)->next;
	else
		si = ((struct idtentry *) t)->next;
	t->next = si;
	t->cnt = 1;

	freecnt = 1;

	/* find the last/only segment */
	while (si >= 0) {
		freecnt++;

		t = &p->slot[si];
		t->cnt = 1;
		si = t->next;
	}

	/* update freelist */
	t->next = p->header.freelist;
	p->header.freelist = fsi;
	p->header.freecnt += freecnt;

	/* if delete from middle,
	 * shift left the succedding entries in the stbl
	 */
	si = p->header.nextindex;
	if (fi < si - 1)
		memmove(&stbl[fi], &stbl[fi + 1], si - fi - 1);

	p->header.nextindex--;
}
