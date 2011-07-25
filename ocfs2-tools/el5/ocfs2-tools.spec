# $Id$
# Authority: dag

%define python_sitearch %(%{__python} -c 'from distutils import sysconfig; print sysconfig.get_python_lib(1)')

Summary: Tools for managing the Oracle Cluster Filesystem 2
Name: ocfs2-tools
Version: 1.4.4
Release: 1%{?dist}
License: GPL
Group: System Environment/Kernel
URL: http://oss.oracle.com/projects/ocfs2-tools/

Source: http://oss.oracle.com/projects/ocfs2-tools/dist/files/source/v1.4/ocfs2-tools-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

BuildRequires: compat-libcom_err
BuildRequires: e2fsprogs-devel
BuildRequires: glib2-devel >= 2.2.3
BuildRequires: ncurses-devel
BuildRequires: pygtk2 >= 1.99.16
BuildRequires: python-devel >= 2.3
BuildRequires: readline-devel
BuildRequires: util-linux >= 2.12j
Requires: bash
Requires: coreutils
Requires: chkconfig
Requires: e2fsprogs
Requires: glib2 >= 2.2.3
Requires: modutils
Requires: net-tools
Requires: redhat-lsb
Requires: util-linux >= 2.12j
Requires: which

%description
Tools to manage Oracle Cluster Filesystem 2 volumes.

%package devel
Summary: Headers and static archives for ocfs2-tools
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
Requires: e2fsprogs-devel  
Requires: glib2-devel >= 2.2.3

%description devel
ocfs2-tools-devel contains the libraries and header files needed to
develop ocfs2 filesystem-specific programs.

%package -n ocfs2console
Summary: GUI frontend for OCFS2 management
Group: System Environment/Kernel
Requires: %{name} = %{version}-%{release}
Requires: e2fsprogs
Requires: glib2 >= 2.2.3
Requires: pygtk2 >= 1.99.16
Requires: python >= 2.3
Requires: vte >= 0.11.10

%description -n ocfs2console
GUI frontend for management and debugging of Oracle Cluster Filesystem 2
volumes.

%prep
%setup

### (According to Oracle) Red Hat chkconfig is completely and utterly broken
%{__perl} -pi -e 'BEGIN() { $k=0;} if (/^###/) { $k++ } elsif ($k == 1) { printf "#"};' vendor/common/*.init

%build
%configure --disable-debug

%{__make} #%{?_smp_mflags}

%install
%{__rm} -rf %{buildroot}
%{__make} install DESTDIR="%{buildroot}"

%{__install} -Dp -m0755 vendor/common/o2cb.init %{buildroot}%{_initrddir}/o2cb
%{__install} -Dp -m0644 vendor/common/o2cb.sysconfig %{buildroot}%{_sysconfdir}/sysconfig/o2cb
%{__install} -Dp -m0755 vendor/common/ocfs2.init %{buildroot}%{_initrddir}/ocfs2

%post
/sbin/chkconfig --add o2cb &>/dev/null
/sbin/chkconfig --add ocfs2 &>/dev/null

%preun
if [ $1 -eq 0 ]; then
    /sbin/chkconfig --del ocfs2 &>/dev/null
    /sbin/chkconfig --del o2cb &>/dev/null
fi

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%doc COPYING CREDITS MAINTAINERS README*
%doc documentation/*
%doc %{_mandir}/man7/o2cb.7*
%doc %{_mandir}/man8/debugfs.ocfs2.8*
%doc %{_mandir}/man8/fsck.ocfs2.8*
%doc %{_mandir}/man8/fsck.ocfs2.checks.8*
%doc %{_mandir}/man8/mkfs.ocfs2.8*
%doc %{_mandir}/man8/mount.ocfs2.8*
%doc %{_mandir}/man8/mounted.ocfs2.8*
%doc %{_mandir}/man8/o2cb_ctl.8*
%doc %{_mandir}/man8/o2image.8*
%doc %{_mandir}/man8/ocfs2_hb_ctl.8*
%doc %{_mandir}/man8/tunefs.ocfs2.8*
%config %{_initrddir}/o2cb
%config %{_initrddir}/ocfs2
%config(noreplace) %{_sysconfdir}/sysconfig/o2cb
/sbin/debugfs.ocfs2
/sbin/fsck.ocfs2
/sbin/mkfs.ocfs2
/sbin/mount.ocfs2
/sbin/mounted.ocfs2
/sbin/o2cb_ctl
/sbin/o2image
/sbin/ocfs2_hb_ctl
/sbin/tunefs.ocfs2

%files devel
%defattr(-, root, root, 0755)
%{_includedir}/o2cb/
%{_includedir}/o2dlm/
%{_includedir}/ocfs2-kernel/
%{_includedir}/ocfs2/
%{_libdir}/libo2cb.a
%{_libdir}/libo2dlm.a
%{_libdir}/libocfs2.a
%{_libdir}/pkgconfig/o2cb.pc
%{_libdir}/pkgconfig/o2dlm.pc
%{_libdir}/pkgconfig/ocfs2.pc

%files -n ocfs2console
%defattr(-, root, root, 0755)
%doc %{_mandir}/man8/ocfs2console.8*
%{python_sitearch}/ocfs2interface/
%{_sbindir}/ocfs2console

%changelog
* Wed Jun 16 2010 Dag Wieers <dag@elrepo.org> - 1.4.4-1
- Initial package.
