
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/libfs
LDADD = ../libfs/libfs.a -luuid

AM_CPPFLAGS = -DONE_FILESET_PER_AGGR

sbin_PROGRAMS = jfs_mkfs
man_MANS = jfs_mkfs.8

EXTRA_DIST = $(man_MANS)

jfs_mkfs_SOURCES = initmap.c inodemap.c inodes.c mkfs.c initmap.h \
                 inodemap.h inodes.h

install-exec-hook:
	$(LN) -f $(DESTDIR)$(sbindir)/jfs_mkfs $(DESTDIR)$(sbindir)/mkfs.jfs

install-data-hook:
	$(LN) -f $(DESTDIR)$(man8dir)/jfs_mkfs.8 $(DESTDIR)$(man8dir)/mkfs.jfs.8

uninstall-local:
	rm -f $(DESTDIR)$(sbindir)/mkfs.jfs
	rm -f $(DESTDIR)$(man8dir)/mkfs.jfs.8
