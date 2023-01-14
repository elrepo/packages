
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/libfs -I$(top_srcdir)/fsck
LDADD = ../libfs/libfs.a

sbin_PROGRAMS = jfs_fscklog
man_MANS = jfs_fscklog.8

EXTRA_DIST = $(man_MANS)

jfs_fscklog_SOURCES = fscklog.c display.c extract.c jfs_fscklog.h
