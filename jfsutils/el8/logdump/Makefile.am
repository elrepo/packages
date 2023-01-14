
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/libfs
LDADD = ../libfs/libfs.a -luuid

sbin_PROGRAMS = jfs_logdump
man_MANS = jfs_logdump.8

EXTRA_DIST = $(man_MANS)

jfs_logdump_SOURCES = logdump.c helpers.c
