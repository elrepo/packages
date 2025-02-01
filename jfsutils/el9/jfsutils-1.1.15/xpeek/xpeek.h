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
 *   FUNCTION: common data & function prototypes
 */
#include <stdio.h>
#include "jfs_types.h"
#include "jfs_dinode.h"
#include "jfs_imap.h"
#include "jfs_superblock.h"

/* Defines */
#define AGGREGATE_2ND_I -1

#define XPEEK_OK        0x00
#define XPEEK_CHANGED   0x01
#define XPEEK_REDISPLAY 0x10
#define XPEEK_ERROR     -1

/* Global Data */
extern int bsize;
extern FILE *fp;
extern short l2bsize;

/* xpeek functions */

void alter(void);
void cbblfsck(void);
void directory(void);
void display(void);
void display_iag(struct iag *);
void display_inode(struct dinode *);
int display_super(struct superblock *);
void dmap(void);
void dtree(void);
void help(void);
int find_iag(unsigned iagnum, unsigned which_table, int64_t * address);
int find_inode(unsigned inum, unsigned which_table, int64_t * address);
void fsckwsphdr(void);
void iag(void);
void inode(void);
void logsuper(void);
int m_parse(char *, int, char **);
int more(void);
char prompt(char *);
void superblock(void);
void s2perblock(void);
void xtree(void);

int xRead(int64_t, unsigned, char *);
int xWrite(int64_t, unsigned, char *);

#define fputs(string,fd) { fputs(string,fd); fflush(fd); }
