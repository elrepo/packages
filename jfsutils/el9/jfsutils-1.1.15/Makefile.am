
SUBDIRS = libfs include fsck fscklog logdump mkfs tune xpeek

EXTRA_DIST = jfsutils.spec.in

dist-hook: jfsutils.spec
	cp jfsutils.spec $(distdir)
	echo "#define JFSUTILS_DATE \"`date +%d-%b-%Y`\"" > $(distdir)/include/jfs_version.h