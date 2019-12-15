Name: reiserfs-utils
Version: 3.6.27
Release: 1%{?dist}
Summary: Tools for creating, repairing and debugging ReiserFS filesystems.
URL: https://mirrors.edge.kernel.org/pub/linux/kernel/people/jeffm/reiserfsprogs/
Source: https://mirrors.edge.kernel.org/pub/linux/kernel/people/jeffm/reiserfsprogs/reiserfsprogs-3.6.27.tar.gz
License: GPLv2
Group: System Environment/Base
Prefix: %{_prefix}
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
BuildRequires: e2fsprogs-devel
BuildRequires: redhat-rpm-config
BuildRequires: libacl-devel libuuid-devel libcom_err-devel

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
The %{name} package contains a number of utilities for
creating, checking, modifying, and correcting any inconsistencies in
ReiserFS filesystems, including reiserfsck (used to repair filesystem
inconsistencies), mkreiserfs (used to initialize a partition to
contain an empty ReiserFS filesystem), debugreiserfs (used to examine
the internal structure of a filesystem, to manually repair a corrupted
filesystem, or to create test cases for reiserfsck) and other ReiserFS
filesystem utilities.

%prep
%setup -q -n reiserfsprogs-%{version}

%build
find . -name config.cache | xargs %{__rm} -f
export CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS"
%configure -quiet --prefix=/
%{__make} -s %{?_smp_mflags}

%install
%{__rm} -rf $RPM_BUILD_ROOT
%{__make} -s install DESTDIR="$RPM_BUILD_ROOT"
%{__mv} -f $RPM_BUILD_ROOT/usr/sbin $RPM_BUILD_ROOT/sbin
pushd $RPM_BUILD_ROOT/sbin
%{__ln_s} -f mkreiserfs mkfs.reiserfs
%{__ln_s} -f reiserfsck fsck.reiserfs
popd
%{__mkdir_p} $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}
%{__install} -m 644 ChangeLog $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}/
%{__install} -m 644 COPYING $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}/
%{__install} -m 644 CREDITS $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}/
%{__install} -m 644 INSTALL $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}/
%{__install} -m 644 README $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}/

%files
%defattr(-,root,root)
%doc %{_docdir}/%{name}-%{version}/
%doc %{_mandir}/man8/
/sbin/debugreiserfs
/sbin/fsck.reiserfs
/sbin/mkfs.reiserfs
/sbin/mkreiserfs
/sbin/reiserfsck
/sbin/reiserfstune
/sbin/resize_reiserfs
/sbin/debugfs.reiserfs
/sbin/tunefs.reiserfs
/usr/include/reiserfs/io.h
/usr/include/reiserfs/misc.h
/usr/include/reiserfs/reiserfs_err.h
/usr/include/reiserfs/reiserfs_fs.h
/usr/include/reiserfs/reiserfs_lib.h
/usr/include/reiserfs/swab.h
/usr/lib64/libreiserfscore.a
/usr/lib64/libreiserfscore.la
/usr/lib64/libreiserfscore.so
/usr/lib64/libreiserfscore.so.0
/usr/lib64/libreiserfscore.so.0.0.0
/usr/lib64/pkgconfig/reiserfscore.pc

%clean
%{__rm} -rf $RPM_BUILD_ROOT $RPM_BUILD_DIR/%{name}-%{version}

%changelog
* Sat Dec 07 2019 Akemi Yagi <toracat@elrepo.org> - 3.6.27-1
- Initial build for RHEL 8
