
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/libfs
LDADD = ../libfs/libfs.a -luuid

sbin_PROGRAMS = jfs_tune
man_MANS = jfs_tune.8

EXTRA_DIST = $(man_MANS)

jfs_tune_SOURCES = tune.c super.c
