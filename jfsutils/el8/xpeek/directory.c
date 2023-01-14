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
 *   FUNCTION:  Display/modify directory/dtree and file/xtree
 */
#include <config.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xpeek.h"
#include "jfs_dtree.h"
#include "jfs_xtree.h"
#include "jfs_filsys.h"
#include "unicode_to_utf8.h"
#include "jfs_endian.h"
#include "jfs_unicode.h"

/* libfs includes */
#include <inode.h>

extern unsigned type_jfs;

void print_direntry(struct dtslot *, uint8_t);
char display_leaf_slots(struct dtslot *, int8_t *, int8_t, int *);
char display_internal_slots(struct dtslot *, int8_t *, int8_t, int *);
char display_leaf_xads(xad_t *, short, int *);
char display_internal_xads(xad_t *, short, int *);
char display_internal_xtpage(xad_t);
char display_slot(struct dtslot *, int8_t, int, int *);
char display_slot(struct dtslot *, int8_t, int, int *);
char display_extent_page(int64_t);
void display_xtpage(xtpage_t *);

void strToUcs(UniChar *, char *, int);
char UTF8_Buffer[8 * JFS_PATH_MAX];

void directory()
{
	char cmd_line[80];
	dtpage_t dtree;
	int i;
	struct dinode inode;
	int64_t inode_address;
	unsigned inum;
	int64_t node_address;
	struct idtentry *node_entry;
	dtroot_t *root;
	uint8_t *stbl;
	char *token;
	unsigned which_table = FILESYSTEM_I;

	token = strtok(0, "     \n");
	if (token == 0) {
		fputs("directory: Please enter: inum [fileset]\ndirectory> ", stdout);
		fgets(cmd_line, 80, stdin);
		token = strtok(cmd_line, "        \n");
		if (token == 0)
			return;
	}
	errno = 0;
	inum = strtoul(token, 0, 0);
	if (inum == 0 && errno) {
		fputs("directory: invalid inum\n\n", stderr);
		return;
	}
	token = strtok(0, "     \n");
	if (token) {
		if (token[0] != '0') {
			fputs("directory: invalid fileset\n\n", stderr);
			return;
		}
	}
	if (strtok(0, "         \n")) {
		fputs("directory: Too many arguments\n\n", stderr);
		return;
	}

	if (find_inode(inum, which_table, &inode_address) ||
	    xRead(inode_address, sizeof (struct dinode), (char *) &inode)) {
		fputs("directory: error reading inode\n\n", stderr);
		return;
	}

	/* swap if on big endian machine */
	ujfs_swap_dinode(&inode, GET, type_jfs);

	if ((inode.di_mode & IFMT) != IFDIR) {
		fputs("directory: Not a directory!\n", stderr);
		return;
	}

	root = (dtroot_t *) & (inode.di_btroot);
	printf("idotdot = %d\n\n", root->header.idotdot);

	if (root->header.flag & BT_LEAF) {
		if (root->header.nextindex == 0) {
			fputs("Empty directory.\n", stdout);
			return;
		}

		for (i = 0; i < root->header.nextindex; i++) {
			print_direntry(root->slot, root->header.stbl[i]);
		}
		return;
	}

	/* Root is not a leaf node, we must descend to the leftmost leaf */

	node_entry = (struct idtentry *) & (root->slot[root->header.stbl[0]]);
      descend:
	node_address = addressPXD(&(node_entry->xd)) << l2bsize;
	if (xRead(node_address, sizeof (dtpage_t), (char *) &dtree)) {
		fputs("Directory:  Error reading dtree node\n", stderr);
		return;
	}

	/* swap if on big endian machine */
	ujfs_swap_dtpage_t(&dtree, type_jfs);

	stbl = (uint8_t *) & (dtree.slot[dtree.header.stblindex]);
	if (!(dtree.header.flag & BT_LEAF)) {
		node_entry = (struct idtentry *) & (dtree.slot[stbl[0]]);
		goto descend;
	}

	/* dtree (contained in node) is the left-most leaf node */

      next_leaf:
	for (i = 0; i < dtree.header.nextindex; i++) {
		print_direntry(dtree.slot, stbl[i]);
	}
	if (dtree.header.next) {
		if (xRead(dtree.header.next << l2bsize, sizeof (dtpage_t), (char *) &dtree)) {
			fputs("directory: Error reading leaf node\n", stderr);
			return;
		}

		/* swap if on big endian machine */
		ujfs_swap_dtpage_t(&dtree, type_jfs);

		stbl = (uint8_t *) & (dtree.slot[dtree.header.stblindex]);
		goto next_leaf;
	}
	return;
}

void print_direntry(struct dtslot *slot, uint8_t head_index)
{
	struct ldtentry *entry;
	int len;
	UniChar *n;
	UniChar *name;
	int namlen;
	int next;
	struct dtslot *s;

	entry = (struct ldtentry *) & (slot[head_index]);
	namlen = entry->namlen;
	name = (UniChar *) malloc(sizeof (UniChar) * (namlen + 1));
	if (name == 0) {
		fputs("dirname: malloc error!\n", stderr);
		return;
	}
	name[namlen] = 0;
	len = MIN(namlen, DTLHDRDATALEN);
	UniStrncpy(name, entry->name, len);
	next = entry->next;
	n = name + len;

	while (next >= 0) {
		s = &(slot[next]);
		namlen -= len;
		len = MIN(namlen, DTSLOTDATALEN);
		UniStrncpy(n, s->name, len);
		next = s->next;
		n += len;
	}
	/* Clear the UTF8 conversion buffer. */
	memset(UTF8_Buffer, 0, sizeof (UTF8_Buffer));

	/* Convert the name into UTF8 */
	Unicode_String_to_UTF8_String(UTF8_Buffer, name, entry->namlen);
	printf("%d\t%s\n", entry->inumber, UTF8_Buffer);
	free(name);
}

void dtree()
{
	int changed = 0;
	char cmd_line[80];
	dtpage_t *dtree;
	int field;
	char flag_names[64];
	struct dinode inode;
	int64_t inode_address;
	unsigned inum;
	char result;
	dtroot_t *root;
	char *token;
	unsigned which_table = FILESYSTEM_I;

	token = strtok(0, "     \n");
	if (token == 0) {
		fputs("dtree: Please enter: inum [fileset]\ndtree> ", stdout);
		fgets(cmd_line, 80, stdin);
		token = strtok(cmd_line, "        \n");
		if (token == 0)
			return;
	}
	errno = 0;
	inum = strtoul(token, 0, 0);
	if (inum == 0 && errno) {
		fputs("dtree: invalid inum\n\n", stderr);
		return;
	}
	token = strtok(0, "     \n");
	if (token) {
		if (token[0] != '0') {
			fputs("dtree: invalide fileset\n\n", stderr);
			return;
		}
	}
	if (strtok(0, "         \n")) {
		fputs("dtree: Too many arguments\n\n", stderr);
		return;
	}

	if (find_inode(inum, which_table, &inode_address) ||
	    xRead(inode_address, sizeof (struct dinode), (char *) &inode)) {
		fputs("dtree: error reading inode\n\n", stderr);
		return;
	}

	/* swap if on big endian machine */
	ujfs_swap_dinode(&inode, GET, type_jfs);

	if ((inode.di_mode & IFMT) != IFDIR) {
		fputs("dtree: Not a directory!\n", stderr);
		return;
	}

	dtree = (dtpage_t *) & (inode.di_btroot);

      redisplay:

	if (!(dtree->header.flag & BT_ROOT))
		fputs("dtree: Should be at root of dtree, but BTROOT not set!\n", stderr);
	root = (dtroot_t *) dtree;

	*flag_names = 0;
	if (root->header.flag & BT_ROOT)
		strcat(flag_names, "BT_ROOT  ");
	if (root->header.flag & BT_LEAF)
		strcat(flag_names, "BT_LEAF  ");
	if (root->header.flag & BT_INTERNAL)
		strcat(flag_names, "BT_INTERNAL  ");
	if (root->header.flag & BT_RIGHTMOST)
		strcat(flag_names, "BT_RIGHTMOST  ");
	if (root->header.flag & BT_LEFTMOST)
		strcat(flag_names, "BT_LEFTMOST  ");

	printf("Root D-Tree Node of inode %d\n\n", inode.di_number);
	printf("[1] DASDlimit\t%lld\n", (long long) DASDLIMIT(&(root->header.DASD)));
	printf("[2] DASDused\t%lld\n", (long long) DASDUSED(&(root->header.DASD)));
	printf("[3] thresh (%%)\t%d\n", root->header.DASD.thresh);
	printf("[4] delta (%%)\t%d\n", root->header.DASD.delta);
	printf("\n");
	printf("[5] flag\t0x%02x\t\t%s\n", root->header.flag, flag_names);
	printf("[6] nextindex\t%d\n", root->header.nextindex);
	printf("[7] freecnt\t%d\n", root->header.freecnt);
	printf("[8] freelist\t%d\n", root->header.freelist);
	printf("[9] idotdot\t%d\n", root->header.idotdot);
	printf("[10] stbl\t{%d,%d,%d,%d,%d,%d,%d,%d}\n",
	       root->header.stbl[0], root->header.stbl[1],
	       root->header.stbl[2], root->header.stbl[3],
	       root->header.stbl[4], root->header.stbl[5],
	       root->header.stbl[6], root->header.stbl[7]);

      retry:
	if (root->header.nextindex) {
		fputs("dtree: Hit enter to see entries, [m]odify, or e[x]it: ", stdout);
	} else {
		fputs("dtree: [m]odify, or e[x]it: ", stdout);
	}

	fgets(cmd_line, 80, stdin);
	token = strtok(cmd_line, "      \n");
	if (token) {
		if (*token == 'x')
			return;

		if (*token == 'm') {
			field = m_parse(cmd_line, 9, &token);
			if (field == 0)
				goto retry;

			switch (field) {
			case 1:
				setDASDLIMIT(&(root->header.DASD), strtoll(token, 0, 0));
				break;
			case 2:
				setDASDUSED(&(root->header.DASD), strtoll(token, 0, 0));
				break;
			case 3:
				root->header.DASD.thresh = strtoul(token, 0, 0);
				break;
			case 4:
				root->header.DASD.delta = strtoul(token, 0, 0);
				break;
			case 5:
				root->header.flag = strtoul(token, 0, 16);
				break;
			case 6:
				root->header.nextindex = strtoul(token, 0, 0);
				break;
			case 7:
				root->header.freecnt = strtoul(token, 0, 0);
				break;
			case 8:
				root->header.freelist = strtoul(token, 0, 0);
				break;
			case 9:
				root->header.idotdot = strtoul(token, 0, 0);
				break;
			}

			/* swap if on big endian machine */
			ujfs_swap_dinode(&inode, PUT, type_jfs);

			if (xWrite(inode_address, sizeof (struct dinode), (char *) &inode)) {
				fputs("dtree: error writing inode\n\n", stderr);
				/* swap back if on big endian machine */
				ujfs_swap_dinode(&inode, GET, type_jfs);
				return;
			}

			/* swap back if on big endian machine */
			ujfs_swap_dinode(&inode, GET, type_jfs);

			goto redisplay;
		}
	}
	if (root->header.nextindex == 0)
		return;

	if (root->header.flag & BT_LEAF)
		result = display_leaf_slots(root->slot, root->header.stbl,
					    root->header.nextindex, &changed);
	else
		result = display_internal_slots(root->slot, root->header.stbl,
						root->header.nextindex, &changed);

	if (changed) {

		/* swap if on big endian machine */
		ujfs_swap_dinode(&inode, PUT, type_jfs);

		if (xWrite(inode_address, sizeof (struct dinode), (char *) &inode)) {
			fputs("dtree: error writing inode\n\n", stderr);
			/* swap back if on big endian machine */
			ujfs_swap_dinode(&inode, GET, type_jfs);
			return;
		}

		/* swap back if on big endian machine */
		ujfs_swap_dinode(&inode, GET, type_jfs);

		changed = 0;
	}

	if (result == 'u')
		goto redisplay;

	return;
}

void xtree()
{
	int changed = 0;
	char cmd_line[80];
	xtpage_t *xtree;
	int field;
	struct dinode inode;
	int64_t inode_address;
	unsigned inum;
	char result;
	char *token;
	unsigned which_table = FILESYSTEM_I;

	token = strtok(0, "     \n");
	if (token == 0) {
		fputs("xtree: Please enter: inum [fileset]\ndtree> ", stdout);
		fgets(cmd_line, 80, stdin);
		token = strtok(cmd_line, "        \n");
		if (token == 0)
			return;
	}
	errno = 0;
	inum = strtoul(token, 0, 0);
	if (inum == 0 && errno) {
		fputs("xtree: invalid inum\n\n", stderr);
		return;
	}
	token = strtok(0, "     \n");
	if (token) {
		if (token[0] == 'a')
			which_table = AGGREGATE_I;
		else if (token[0] == 's')
			which_table = AGGREGATE_2ND_I;
		else if (token[0] != '0') {
			fputs("inode: invalide fileset\n\n", stderr);
			return;
		}
	}
	if (strtok(0, "         \n")) {
		fputs("xtree: Too many arguments\n\n", stderr);
		return;
	}

	if (find_inode(inum, which_table, &inode_address) ||
	    xRead(inode_address, sizeof (struct dinode), (char *) &inode)) {
		fputs("xtree: error reading inode\n\n", stderr);
		return;
	}

	/* swap if on big endian machine */
	ujfs_swap_dinode(&inode, GET, type_jfs);

	if ((inode.di_mode & IFMT) == IFDIR)
		xtree = (xtpage_t *) & (inode.di_dirtable);
	else
		xtree = (xtpage_t *) & (inode.di_btroot);

      redisplay:

	printf("Root X-Tree Node of inode %d\n\n", inode.di_number);
	display_xtpage(xtree);

      retry:
	if (xtree->header.nextindex > 2) {
		fputs("xtree: Hit enter to see entries, [m]odify, or e[x]it: ", stdout);
	} else {
		fputs("xtree: [m]odify, or e[x]it: ", stdout);
	}

	fgets(cmd_line, 80, stdin);
	token = strtok(cmd_line, "      \n");
	if (token) {
		if (*token == 'x')
			return;

		if (*token == 'm') {
			field = m_parse(cmd_line, 6, &token);
			if (field == 0)
				goto retry;

			switch (field) {
			case 1:
				xtree->header.flag = strtoul(token, 0, 16);
				break;
			case 2:
				xtree->header.nextindex = strtoul(token, 0, 0);
				break;
			case 3:
				xtree->header.maxentry = strtoul(token, 0, 0);
				break;
			case 4:
				xtree->header.self.len = strtoul(token, 0, 0);
				break;
			case 5:
				xtree->header.self.addr1 = strtoul(token, 0, 0);
				break;
			case 6:
				xtree->header.self.addr2 = strtoul(token, 0, 0);
				break;
			}

			/* swap if on big endian machine */
			ujfs_swap_dinode(&inode, PUT, type_jfs);

			if (xWrite(inode_address, sizeof (struct dinode), (char *) &inode)) {
				fputs("xtree: error writing inode\n\n", stderr);
				/* swap back if on big endian machine */
				ujfs_swap_dinode(&inode, GET, type_jfs);
				return;
			}

			/* swap back if on big endian machine */
			ujfs_swap_dinode(&inode, GET, type_jfs);

			goto redisplay;
		}
	}
	if (xtree->header.nextindex <= 2)
		return;

	if (xtree->header.flag & BT_LEAF)
		result = display_leaf_xads(xtree->xad, xtree->header.nextindex, &changed);
	else
		result = display_internal_xads(xtree->xad, xtree->header.nextindex, &changed);

	if (changed) {

		/* swap if on big endian machine */
		ujfs_swap_dinode(&inode, PUT, type_jfs);

		if (xWrite(inode_address, sizeof (struct dinode), (char *) &inode)) {
			fputs("xtree: error writing inode\n\n", stderr);
			/* swap back if on big endian machine */
			ujfs_swap_dinode(&inode, GET, type_jfs);
			return;
		}

		/* swap back if on big endian machine */
		ujfs_swap_dinode(&inode, GET, type_jfs);

		changed = 0;
	}

	if (result == 'u')
		goto redisplay;

	return;
}

void display_xtpage(xtpage_t * xtree)
{

	char flag_names[64];

	*flag_names = 0;
	if (xtree->header.flag & BT_ROOT)
		strcat(flag_names, "BT_ROOT  ");
	if (xtree->header.flag & BT_LEAF)
		strcat(flag_names, "BT_LEAF  ");
	if (xtree->header.flag & BT_INTERNAL)
		strcat(flag_names, "BT_INTERNAL  ");
	if (xtree->header.flag & BT_RIGHTMOST)
		strcat(flag_names, "BT_RIGHTMOST  ");
	if (xtree->header.flag & BT_LEFTMOST)
		strcat(flag_names, "BT_LEFTMOST  ");

	printf("[1] flag\t0x%02x\t%s\n", xtree->header.flag, flag_names);
	printf("[2] nextindex\t%d\t\t", xtree->header.nextindex);
	printf("[5] self.addr1\t0x%02x\n", xtree->header.self.addr1);
	printf("[3] maxentry\t%d\t\t", xtree->header.maxentry);
	printf("[6] self.addr2\t0x%08x\n", xtree->header.self.addr2);
	printf("[4] self.len\t0x%06x\t", xtree->header.self.len);
	printf("    self.addr\t%lld\n", (long long) addressPXD(&xtree->header.self));
}				/* end display_xtpage */

char display_leaf_slots(struct dtslot *slot, int8_t * stbl, int8_t nextindex, int *changed)
{
	char cmd_line[512];
	int i;
	int field;
	struct ldtentry *leaf;
	char result = 'u';	/* default returned if no leaf->next */
	int slot_number;
	char *token;
	int nfields = (type_jfs & JFS_DIR_INDEX) ? 5 : 4;

	for (i = 0; i < nextindex; i++) {
		slot_number = stbl[i];
		leaf = (struct ldtentry *) & (slot[slot_number]);

	      redisplay2:
		printf("stbl[%d] = %d\n", i, slot_number);
		printf("[1] inumber\t%d\n", leaf->inumber);
		printf("[2] next\t%d\n", leaf->next);
		printf("[3] namlen\t%d\n", leaf->namlen);
		/* Clear the UTF8 conversion buffer. */
		memset(UTF8_Buffer, 0, sizeof (UTF8_Buffer));

		/* Convert the name into UTF8 */
		Unicode_String_to_UTF8_String(UTF8_Buffer, leaf->name, leaf->namlen);

		printf("[4] name\t%s\n", UTF8_Buffer);
		if (type_jfs & JFS_DIR_INDEX)
			printf("[5] index\t%d\n", leaf->index);

	      retry2:
		fputs("dtree: Press enter for next, [m]odify, [u]p, or e[x]it > ", stdout);
		fgets(cmd_line, 512, stdin);
		token = strtok(cmd_line, "      \n");
		if (token) {
			if (*token == 'u' || *token == 'x')
				return *token;
			if (*token == 'm') {
				field = m_parse(cmd_line, nfields, &token);
				if (field == 0)
					goto retry2;

				switch (field) {
				case 1:
					leaf->inumber = strtoul(token, 0, 0);
					break;
				case 2:
					leaf->next = strtoul(token, 0, 0);
					break;
				case 3:
					leaf->namlen = strtoul(token, 0, 0);
					break;
				case 4:
					strToUcs(leaf->name, token, DTLHDRDATALEN);
					break;
				case 5:
					leaf->index = strtoul(token, 0, 0);
					break;
				}
				*changed = 1;
				goto redisplay2;
			}
		}

		if (leaf->next >= 0) {
			result = display_slot(slot, leaf->next, 1, changed);
			if (result == 'u' || result == 'x')
				return result;
		}
	}
	return result;
}

char display_slot(struct dtslot *slot, int8_t index, int isleaf, int *changed)
{
	char result;

	printf("[1] next\t%d\n", slot[index].next);
	printf("[2] cnt\t\t%d\n", slot[index].cnt);
	/* Clear the UTF8 conversion buffer. */
	memset(UTF8_Buffer, 0, sizeof (UTF8_Buffer));

	/* Convert the name into UTF8 */
	Unicode_String_to_UTF8_String(UTF8_Buffer, slot[index].name, JFS_PATH_MAX);

	printf("[3] name\t%.15s\n", UTF8_Buffer);

	if (isleaf)
		result = prompt("dtree: press enter for next or [u]p or e[x]it > ");
	else
		result = prompt("dtree: press enter for next or [u]p, [d]own or e[x]it > ");

	if (result == 'u' || result == 'd' || result == 'x')
		return result;

	if (slot[index].next >= 0)
		return display_slot(slot, slot[index].next, isleaf, changed);
	else
		return result;
}

char display_internal_slots(struct dtslot *slot, int8_t *stbl, int8_t nextindex, int *changed)
{
	int i;
	struct idtentry *entry;
	int64_t node_address;
	char result = 0;
	int slot_number;

	for (i = 0; i < nextindex; i++) {
		slot_number = stbl[i];
		entry = (struct idtentry *) & (slot[slot_number]);
		node_address = addressPXD(&(entry->xd));

		printf("stbl[%d] = %d\n", i, slot_number);
		printf("[1] xd.len\t    0x%06x\t\t", entry->xd.len);
		printf("[4] next\t%d\n", entry->next);
		printf("[2] xd.addr1\t  0x%02x\t\t\t", entry->xd.addr1);
		printf("[5] namlen\t%d\n", entry->namlen);
		printf("[3] xd.addr2\t  0x%08x\t\t", entry->xd.addr2);
		printf("     xd.addr\t%lld\n", (long long) node_address);
		/* Clear the UTF8 conversion buffer. */
		memset(UTF8_Buffer, 0, sizeof (UTF8_Buffer));

		/* Convert the name into UTF8 */
		Unicode_String_to_UTF8_String(UTF8_Buffer, entry->name, entry->namlen);

		printf("[6] name\t%.11s\n", UTF8_Buffer);
		printf("addressPXD(xd)\t%lld\n", (long long) node_address);

		result = prompt("dtree: press enter for next or [u]p, [d]own or e[x]it > ");
		if (result == 'x' || result == 'u')
			return result;
		else if (result != 'd' && entry->next >= 0) {
			result = display_slot(slot, entry->next, 0, changed);
			if (result == 'x' || result == 'u')
				return result;
		}
		if (result == 'd')
			/* descend to the child node */
			return (display_extent_page(node_address));
	}
	return result;
}

char display_leaf_xads(xad_t * xad, short nextindex, int *changed)
{
	int i;
	char result = 0;

	for (i = 2; i < nextindex; i++) {
		printf("XAD # = %d\n", i);
		printf("[1] xad.flag\t  %x\t\t", xad[i].flag);
		printf("[4] xad.len\t  0x%06x\n", xad[i].len);
		printf("[2] xad.off1\t  0x%02x\t\t", xad[i].off1);
		printf("[5] xad.addr1\t  0x%02x\n", xad[i].addr1);
		printf("[3] xad.off2\t  0x%08x\t", xad[i].off2);
		printf("[6] xad.addr2\t  0x%08x\n", xad[i].addr2);
		printf("    xad.off  \t  %lld\t\t", (long long) offsetXAD(&(xad[i])));
		printf("    xad.addr\t  %lld\n", (long long) addressXAD(&(xad[i])));

		result = prompt("xtree: press enter for next or e[x]it > ");
		if (result == 'x' || result == 'u')
			return result;
	}
	return result;
}
char display_internal_xads(xad_t * xad, short nextindex, int *changed)
{
	int i;
	char result = 0;

	for (i = 2; i < nextindex; i++) {
		printf("XAD # = %d\n", i);
		printf("[1] xad.flag\t  %x\t\t", xad[i].flag);
		printf("[4] xad.len\t  0x%06x\n", xad[i].len);
		printf("[2] xad.off1\t  0x%02x\t\t", xad[i].off1);
		printf("[5] xad.addr1\t  0x%02x\n", xad[i].addr1);
		printf("[3] xad.off2\t  0x%08x\t", xad[i].off2);
		printf("[6] xad.addr2\t  0x%08x\n", xad[i].addr2);
		printf("    xad.off  \t  %lld\t\t", (long long) offsetXAD(&(xad[i])));
		printf("    xad.addr\t  %lld\n", (long long) addressXAD(&(xad[i])));

		result = prompt("xtree: press enter for next or [u]p, [d]own or e[x]it > ");
		if (result == 'x' || result == 'u')
			return result;
		else if (result == 'd') {
			result = display_internal_xtpage(xad[i]);
			return result;
		}
	}
	return result;
}

char display_internal_xtpage(xad_t xad)
{
	int changed = 0;
	char cmd_line[80];
	xtpage_t xtree_area;
	xtpage_t *xtree = &xtree_area;
	int field;
	int64_t xtpage_address;
	char result = 'u';
	char *token;

	xtpage_address = addressXAD(&xad);
	xtpage_address = xtpage_address * bsize;

	if (xRead(xtpage_address, sizeof (xtpage_t), (char *) xtree)) {
		fputs("xtree: error reading xtpage\n\n", stderr);
	} else {

		/* swap if on big endian machine */
		ujfs_swap_xtpage_t(xtree);

	      redisplay:

		display_xtpage(xtree);

	      retry:
		if (xtree->header.nextindex > 2) {
			fputs("xtree: Hit enter to see entries, [m]odify, or e[x]it: ", stdout);
		} else {
			fputs("xtree: [m]odify, or e[x]it: ", stdout);
		}

		fgets(cmd_line, 80, stdin);
		token = strtok(cmd_line, "      \n");
		if (token) {
			if (*token == 'x')
				return result;

			if (*token == 'm') {
				field = m_parse(cmd_line, 6, &token);
				if (field == 0)
					goto retry;

				switch (field) {
				case 1:
					xtree->header.flag = strtoul(token, 0, 16);
					break;
				case 2:
					xtree->header.nextindex = strtoul(token, 0, 0);
					break;
				case 3:
					xtree->header.maxentry = strtoul(token, 0, 0);
					break;
				case 4:
					xtree->header.self.len = strtoul(token, 0, 0);
					break;
				case 5:
					xtree->header.self.addr1 = strtoul(token, 0, 0);
					break;
				case 6:
					xtree->header.self.addr2 = strtoul(token, 0, 0);
					break;
				}

				/* swap if on big endian machine */
				ujfs_swap_xtpage_t(xtree);

				if (xWrite(xtpage_address, sizeof (xtpage_t), (char *) xtree)) {
					fputs("xtree: error writing xtpage\n\n", stderr);
					/* swap back if on big endian machine */
					ujfs_swap_xtpage_t(xtree);
					return result;
				}

				/* swap back if on big endian machine */
				ujfs_swap_xtpage_t(xtree);

				goto redisplay;
			}
		}
		if (xtree->header.nextindex <= 2)
			return result;

		if (xtree->header.flag & BT_LEAF)
			result = display_leaf_xads(xtree->xad, xtree->header.nextindex, &changed);
		else
			result = display_internal_xads(xtree->xad,
						       xtree->header.nextindex, &changed);

		if (changed) {

			/* swap if on big endian machine */
			ujfs_swap_xtpage_t(xtree);

			if (xWrite(xtpage_address, sizeof (xtpage_t), (char *) xtree)) {
				fputs("xtree: error writing xtpage\n\n", stderr);
				/* swap back if on big endian machine */
				ujfs_swap_xtpage_t(xtree);
				return result;
			}

			/* swap back if on big endian machine */
			ujfs_swap_xtpage_t(xtree);

			changed = 0;
		}

		if (result == 'u')
			goto redisplay;
	}			/* end else */

	return result;
}

char display_extent_page(int64_t address)
{
	int changed = 0;
	char flag_names[64];
	dtpage_t node;
	char result;
	int8_t *stbl;

	if (xRead(address << l2bsize, sizeof (dtpage_t), (char *) &node)) {
		fprintf(stderr, "display_extent_page: Error reading node\n");
		return 'u';
	}

	/* swap if on big endian machine */
	ujfs_swap_dtpage_t(&node, type_jfs);

      redisplay5:
	*flag_names = 0;
	if (node.header.flag & BT_ROOT) {
		fputs("display_extent_page:  Warning!  extent dtree page has BT_ROOT flag set!\n",
		      stderr);
		strcat(flag_names, "BT_ROOT  ");
	}
	if (node.header.flag & BT_LEAF)
		strcat(flag_names, "BT_LEAF  ");
	if (node.header.flag & BT_INTERNAL)
		strcat(flag_names, "BT_INTERNAL  ");
	if (node.header.flag & BT_RIGHTMOST)
		strcat(flag_names, "BT_RIGHTMOST  ");
	if (node.header.flag & BT_LEFTMOST)
		strcat(flag_names, "BT_LEFTMOST  ");

	printf("Internal D-tree node at block %lld\n", (long long) address);

	printf("[1] flag\t0x%02x\t\t%s\n", node.header.flag, flag_names);
	printf("[2] nextindex\t%3d\n", node.header.nextindex);
	printf("[3] freecnt\t%3d\t\t", node.header.freecnt);
	printf("[7] rsrvd\tNOT DISPLAYED\n");
	printf("[4] freelist\t%3d\t\t", node.header.freelist);
	printf("[8] self.len\t0x%06x\n", node.header.self.len);
	printf("[5] maxslot\t%3d\t\t", node.header.maxslot);
	printf("[8] self.addr1\t0x%02x\n", node.header.self.addr1);
	printf("[6] stblindex\t%d\t\t", node.header.stblindex);
	printf("[9] \t0x%08x\n", node.header.self.addr2);

	if (node.header.nextindex) {
		result = prompt("dtree: Hit enter to see entries, [u]p or e[x]it: ");
		if (result == 'u' || result == 'x')
			return (result);
	} else {
		fputs("display_extent_page: Strange ... empty d-tree node.\n", stderr);
		return 'u';
	}

	stbl = (int8_t *) & (node.slot[node.header.stblindex]);

	if (node.header.flag & BT_LEAF)
		result = display_leaf_slots(node.slot, stbl, node.header.nextindex, &changed);
	else
		result = display_internal_slots(node.slot, stbl, node.header.nextindex, &changed);

	if (changed) {

		/* swap if on big endian machine */
		ujfs_swap_dtpage_t(&node, type_jfs);

		if (xWrite(address << l2bsize, sizeof (dtpage_t), (char *) &node)) {
			fputs("display_extent_page: error writing node\n\n", stderr);
			/* swap back if on big endian machine */
			ujfs_swap_dtpage_t(&node, type_jfs);
			return 0;
		}

		/* swap back if on big endian machine */
		ujfs_swap_dtpage_t(&node, type_jfs);

		changed = 0;
	}
	if (result == 'u')
		goto redisplay5;

	return result;
}

void strToUcs(UniChar * target, char *source, int len)
{
	while ((*(target++) = *(source++)) && --len) ;
	return;
}
