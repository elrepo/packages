
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/libfs
LDADD = ../libfs/libfs.a -luuid

sbin_PROGRAMS = jfs_debugfs
man_MANS = jfs_debugfs.8

EXTRA_DIST = $(man_MANS)

jfs_debugfs_SOURCES = alter.c display.c fsckcbbl.c iag.c io.c super2.c xpeek.c \
         directory.c dmap.c help.c inode.c super.c ui.c xpeek.h
