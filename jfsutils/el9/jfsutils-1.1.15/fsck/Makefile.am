
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/libfs
LDADD = ../libfs/libfs.a -luuid

sbin_PROGRAMS = jfs_fsck
man_MANS = jfs_fsck.8

EXTRA_DIST = $(man_MANS)


jfs_fsck_SOURCES = fsckbmap.c fsckconn.c fsckdire.c fsckdtre.c fsckea.c \
          fsckimap.c fsckino.c fsckmeta.c fsckpfs.c  dirindex.c \
          fsckwsp.c fsckxtre.c xchkdsk.c fsckruns.c fsck_message.c \
          fsckpfs.h xchkdsk.h xfsck.h xfsckint.h

install-exec-hook:
	$(LN) -f $(DESTDIR)$(sbindir)/jfs_fsck $(DESTDIR)$(sbindir)/fsck.jfs

install-data-hook:
	$(LN) -f $(DESTDIR)$(man8dir)/jfs_fsck.8 $(DESTDIR)$(man8dir)/fsck.jfs.8

uninstall-local:
	rm -f $(DESTDIR)$(sbindir)/fsck.jfs
	rm -f $(DESTDIR)$(man8dir)/fsck.jfs.8
