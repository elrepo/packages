%define thiscell openafs.org

%ifarch %{ix86}
%define sysname i386_linux26
%endif
%ifarch ppc ppc64
%define sysname ppc_linux26
%endif
%ifarch x86_64
%define sysname amd64_linux26
%endif

Summary:        Enterprise Network File System
Name:           openafs
Version:        1.6.5
Release:        1%{?dist}
License:        IBM
Group:          System Environment/Daemons
URL:            http://www.openafs.org
Source0:        http://www.openafs.org/dl/openafs/%{version}/%{name}-%{version}-src.tar.bz2
Source1		    http://www.openafs.org/dl/openafs/%{version}/%{name}-%{version}-doc.tar.bz2
Source11:        CellServDB
Source12:        cacheinfo
Source13:        openafs.init
Source14:        afs.conf

BuildRoot:      %{_tmppath}/%{name}-root
BuildRequires:  krb5-devel, pam-devel, ncurses-devel, flex, byacc, bison
BuildRequires:	automake, autoconf

Patch0:         openafs-1.6.0-fPIC.patch
Patch1:         openafs-1.6.2-fPIC-1.patch

%description
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides common files shared across all the various
OpenAFS packages but are not necessarily tied to a client or server.


%package client
Summary: OpenAFS Filesystem client
Group: System Environment/Daemons
Requires(post): bash, coreutils, chkconfig, selinux-policy-targeted
Requires: %{name}-kmod  >= %{version}
Requires: openafs = %{version}
Provides: %{name}-kmod-common = %{version}

%description client
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides basic client support to mount and manipulate
AFS.  


%package devel
Summary: OpenAFS development header files and static libraries
Group: Development/Libraries
Requires: openafs = %{version}-%{release}
Requires(post): /sbin/ldconfig
Provides:       openafs-static = %{version}-%{release}
 
%description devel
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides static development libraries and headers needed
to compile AFS applications.  Note: AFS currently does not provide
shared libraries.

 
%package server
Summary: OpenAFS Filesystem Server
Group: System Environment/Daemons
Requires: openafs-client = %{version}, openafs = %{version}
 
%description server
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides basic server support to host files in an AFS
Cell.

%prep
%setup -q -b 1 -n openafs-%{version}

# This changes osconf.m4 to build with -fPIC on i386 and x86_64
%patch0
%patch1

# Convert the licese to UTF-8
mv src/LICENSE src/LICENSE~
iconv -f ISO-8859-1 -t UTF8 src/LICENSE~ > src/LICENSE
rm src/LICENSE~

%build

# do the main build
buildIt() {
# Can't regen autoconf on RHEL 5 as autoconf 2.60 is required and RHEL 5
# only ships with 2.59.  This changed as of 1.6.1.
# ./regen.sh  

# build the user-space bits for base architectures
    ./configure \
        --prefix=%{_prefix} \
        --libdir=%{_libdir} \
        --bindir=%{_bindir} \
        --sbindir=%{_sbindir} \
        --sysconfdir=%{_sysconfdir} \
        --localstatedir=%{_var} \
        --with-afs-sysname=%{sysname} \
        --with-linux-kernel-headers=%{ksource_dir} \
        --disable-kernel-module \
        --disable-strip-binaries \
        --enable-supergroups  \
        --with-krb5-conf=/usr/bin/krb5-config

    # Build is not SMP compliant
    make $RPM_OPT_FLGS all_nolibafs

}

buildIt

%install
rm -rf ${RPM_BUILD_ROOT}
make DESTDIR=$RPM_BUILD_ROOT install

# Set the executable bit on libraries in libdir, so rpmbuild knows to 
# create "Provides" entries in the package metadata for the libraries
chmod +x ${RPM_BUILD_ROOT}%{_libdir}/*.so*

# install config info
mkdir -p ${RPM_BUILD_ROOT}%{_sysconfdir}/openafs
install -p -m 644 %{SOURCE11} ${RPM_BUILD_ROOT}%{_sysconfdir}/openafs
install -p -m 644 %{SOURCE12} ${RPM_BUILD_ROOT}%{_sysconfdir}/openafs
echo %{thiscell} > ${RPM_BUILD_ROOT}%{_sysconfdir}/openafs/ThisCell

# install the init script
mkdir -p ${RPM_BUILD_ROOT}%{_sysconfdir}/rc.d/init.d
install -m 755 %{SOURCE13} ${RPM_BUILD_ROOT}%{_sysconfdir}/rc.d/init.d/openafs

# sysconfig file
mkdir -p ${RPM_BUILD_ROOT}%{_sysconfdir}/sysconfig
install -m 644 %{SOURCE14} ${RPM_BUILD_ROOT}%{_sysconfdir}/sysconfig/openafs

# Include the vlclient binary
install -m 755 src/vlserver/vlclient ${RPM_BUILD_ROOT}/usr/sbin/vlclient

# Include kpasswd as kapasswd so I can change my admin tokens
mv ${RPM_BUILD_ROOT}/usr/bin/kpasswd ${RPM_BUILD_ROOT}/usr/bin/kapasswd

# Rename /usr/bin/backup to not conflict with Coda 
# (Future AFS upstream change)
mv ${RPM_BUILD_ROOT}/usr/sbin/backup ${RPM_BUILD_ROOT}/usr/sbin/afsbackup

# Put the PAM modules in a sane place
mkdir -p ${RPM_BUILD_ROOT}/%{_lib}/security
mv ${RPM_BUILD_ROOT}%{_libdir}/pam_afs.krb.so.1 \
    ${RPM_BUILD_ROOT}/%{_lib}/security/pam_afs.krb.so
mv ${RPM_BUILD_ROOT}%{_libdir}/pam_afs.so.1 \
    ${RPM_BUILD_ROOT}/%{_lib}/security/pam_afs.so

# Remove utilities related to DCE
rm -f ${RPM_BUILD_ROOT}/usr/bin/dlog
rm -f ${RPM_BUILD_ROOT}/usr/bin/dpass

# Install man pages
tar cf - -C doc/man-pages man1 man5 man8 | \
    tar xf - -C $RPM_BUILD_ROOT%{_mandir}

# remove unused man pages
for x in afs_ftpd afs_inetd afs_login afs_rcp afs_rlogind afs_rsh \
    dkload knfs package runntp symlink symlink_list symlink_make \
    symlink_remove; do
        rm -f $RPM_BUILD_ROOT%{_mandir}/man1/${x}.1
done

# rename man page kpasswd to kapasswd
mv $RPM_BUILD_ROOT%{_mandir}/man1/kpasswd.1 \
   $RPM_BUILD_ROOT%{_mandir}/man1/kapasswd.1

# rename backup man page to afsbackup
mv $RPM_BUILD_ROOT%{_mandir}/man8/backup.8 \
   $RPM_BUILD_ROOT%{_mandir}/man8/afsbackup.8

# Create the cache directory
install -d -m 700 $RPM_BUILD_ROOT%{_localstatedir}/cache/openafs

# don't restart in %post because kernel modules could well have changed
%post
/sbin/ldconfig
if [ $1 = 1 ]; then
        /sbin/chkconfig --add openafs
fi

%post client
# if this is owned by the package, upgrades with afs running can't work
if [ ! -d /afs ] ; then
        mkdir -m 700 /afs
        [ -x /sbin/restorecon ] && /sbin/restorecon /afs
fi 
exit 0

%preun
if [ "$1" = 0 ] ; then
        /sbin/chkconfig --del openafs
        %{_sysconfdir}/rc.d/init.d/openafs stop && rmdir /afs
fi
exit 0

%postun -p /sbin/ldconfig

%post devel -p /sbin/ldconfig

%postun devel -p /sbin/ldconfig

%clean
rm -fr $RPM_BUILD_ROOT

%files
%defattr(-, root, root, -)
%doc src/LICENSE README NEWS README.DEVEL README.GIT README.PTHREADED_UBIK
%doc README.WARNINGS README-WINDOWS
%config(noreplace) %{_sysconfdir}/sysconfig/*
%{_sysconfdir}/rc.d/init.d/*
%{_bindir}/aklog
%{_bindir}/bos
%{_bindir}/fs
%{_bindir}/klog
%{_bindir}/klog.krb
%{_bindir}/klog.krb5
%{_bindir}/knfs
%{_bindir}/kapasswd
%{_bindir}/kpwvalid
%{_bindir}/livesys
%{_bindir}/pts
%{_bindir}/sys
%{_bindir}/pagsh
%{_bindir}/pagsh.krb
%{_bindir}/restorevol
%{_bindir}/tokens
%{_bindir}/tokens.krb
%{_bindir}/udebug
%{_bindir}/unlog
%{_bindir}/up
%{_bindir}/translate_et
%{_sbindir}/afsbackup
%{_sbindir}/butc
%{_sbindir}/fstrace
%{_sbindir}/rxdebug
%{_sbindir}/vos
%{_sbindir}/kas
%{_libdir}/libafsauthent.so.*
%{_libdir}/libafsrpc.so.*
%{_libdir}/libkopenafs.so.*
## %{_libdir}/libafssetpag.so.*
/%{_lib}/security/*.so
%{_mandir}/man1/*
%{_mandir}/man5/*
%{_mandir}/man8/*
%{_datadir}/openafs/C/afszcm.cat

%files client
%defattr(-, root, root)
%dir %{_sysconfdir}/openafs
%dir %{_localstatedir}/cache/openafs
%config(noreplace) %{_sysconfdir}/openafs/CellServDB
%config(noreplace) %{_sysconfdir}/openafs/ThisCell
%config(noreplace) %{_sysconfdir}/openafs/cacheinfo
%{_bindir}/afsio
%{_bindir}/cmdebug
%{_bindir}/xstat_cm_test
%{_sbindir}/afsd

%files server
%defattr(-,root,root)
%{_bindir}/afsmonitor
%{_bindir}/asetkey
%{_bindir}/scout
%{_bindir}/udebug
%{_bindir}/xstat_fs_test
%{_libexecdir}/openafs
%{_sbindir}/dafssync-debug
%{_sbindir}/fssync-debug
%{_sbindir}/salvsync-debug
%{_sbindir}/state_analyzer
%{_sbindir}/bosserver
%{_sbindir}/fms
%{_sbindir}/prdb_check
%{_sbindir}/pt_util
%{_sbindir}/read_tape
%{_sbindir}/uss
%{_sbindir}/vlclient
%{_sbindir}/vldb_check
%{_sbindir}/vldb_convert
%{_sbindir}/volinfo
%{_sbindir}/voldump
%{_sbindir}/bos_util
%{_sbindir}/kadb_check
%{_sbindir}/ka-forwarder
%{_sbindir}/kdb
%{_sbindir}/kpwvalid
%{_sbindir}/rmtsysd

%files devel
%defattr(-,root,root)
%{_bindir}/rxgen
%{_bindir}/afs_compile_et
%{_includedir}/afs
%{_includedir}/rx
%{_includedir}/*.h
%{_sbindir}/vsys
## %{_sbindir}/kdump
%{_libdir}/libafsauthent.so
%{_libdir}/libafsrpc.so
%{_libdir}/libkopenafs.so
## %{_libdir}/libafssetpag.so
%{_libdir}/lib*.a
%{_libdir}/afs

%changelog
* Wed Jul 24 2013 Jack Neely <jjneely@ncsu.edu> 0:1.6.5-1
- Update to 1.6.5
- CVE-2013-4134
- CVE-2013-4135

* Wed Jun 26 2013 Jack Neely <jjneely@ncsu.edu> 0:1.6.4-1
- Update to 1.6.4

* Mon Mar 04 2013 Jack Neely <jjneely@ncsu.edu> 0:1.6.2-1
- Update to 1.6.2
- CVE-2013-1794 and CVE-2013-1795 are corrected by this release
- openafs-1.6.2-fPIC-1.patch added to address another fPIC issue

* Thu Aug 23 2012 Jack Neely <jjneely@ncsu.edu> 0:1.6.1-4
- Update /etc/sysconfig/openafs to have some more current
  default settings.
- AFS_CACHE_DIR is now pre-poplated with the default value
- Encryption is used by default
- Patch to allow fileservers to report correct partition 
  usage data on partitions sizes greater than 2TB.

* Tue Jul 03 2012 Jack Neely <jjneely@ncsu.edu> 0:1.6.1-3
- Require selinux-policy-targeted (RPM Fusion bug #2138).

* Mon May 07 2012 Jack Neely <jjneely@ncsu.edu> 0:1.6.1-2
- Provide openafs-static (RPM Fusion bug #2310)
- Update to OpenAFS 1.6.1 final
- As of 1.6.1 OpenAFS requires autoconf 2.60 and RHEL 5 only has 2.59
  so we disable rebuilding of autoconf during the package build.

* Fri Mar 23 2012 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-3
- Require(post) the restorecon binary for openafs-client
- Patch from Ken Dreyer

* Mon Mar 12 2012 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-3
- refer to /etc/sysconfig/openafs in its comments
- Always fallback to a sysname from /usr/bin/sys
- Set the executable bit on the libraries installed in libdir, so that
  rpmbuild will generate Provides metadata for these libraries.
  See upstream git commit 3f7d8ec2. Fixes RPM Fusion bug #2215

* Tue Jan 17 2012 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-2
- Log an error if we couldn't load the kernel module (RF BZ #2006)

* Wed Sep 07 2011 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-1
- Update to OpenAFS 1.6.0 final

* Thu Aug 04 2011 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-0.pre7.3
- BuildReq autoconf
- Patch osconf.m4 to force building with -fPIC as there were still
  objects used in libraries that were not build with -fPIC
- Removed the CCOBJ environment variable with the configure line

* Fri Jul 29 2011 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-0.pre7.2
- Backport to RHEL 5

* Mon Jul 25 2011 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-0.pre7.1
- rpmFusion Bug 1783
- Populate and set SELinux context for the AFS cache directory
  even though we use memcache by default

* Thu Jul 21 2011 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-0.pre7
- Update to OpenAFS 1.6.0 pre-release 7
- Rename /usr/sbin/backup to /usr/sbin/afsbackup rpmFusion Bug # 1727
- Add the -fakestat option to the default options for afsd
  rpmfusion Bug # 1837

* Wed Jun 08 2011 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-0.pre6
- rpmFusion Bug # 1782
- rename the kpasswd binary to kapasswd to match its man page
- Update to OpenAFS 1.6.0 pre-release 6

* Wed Apr 13 2011 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-0.pre4
- Update to 1.6.0 pre4

* Fri Mar 04 2011 Jack Neely <jjneely@ncsu.edu> 0:1.4.14-3
- rpmFusion Bug #1649
- Include the static libraries in openafs-devel as they are required
  for other tools to use the OpenAFS C APIs
- Set CCOBJ=-fPIC so that the static libraries work properly in x86_64 land
- Enable support for supergroups

* Wed Mar 02 2011 Jack Neely <jjneely@ncsu.edu> 0:1.4.14-2
- Update the CellServDB to the current list from
  http://dl.central.org/dl/cellservdb/CellServDB
- This addresses rpmFusion Bug #1646

* Mon Jan 10 2011 Jack Neely <jjneely@ncsu.edu> 0:1.4.14-1
- Build 1.4.14

* Thu Jul 15 2010 Jack Neely <jjneely@ncsu.edu> 0:1.4.12.1-4
- RPMFusion Bug #1333

* Tue Jun 29 2010 Jack Neely <jjneely@ncsu.edu> 0:1.4.12.1-3
- RPMFusion Bug #1047 - Fix SELinux contexts on /afs
- RPMFusion Bug #1275 - service openafs status now sets the exit code

* Wed Jun 16 2010 Jack Neely <jjneely@ncsu.edu> 0:1.4.12.1-2
- RPMFusion Bug #1274 - OpenAFS wont start without an IP address
- RPMFusion Bug #1277 - Include OpenAFS man pages
- Avoid using the rpm configure macro and call configure directly

* Thu May 27 2010 Jack Neely <jjneely@ncsu.edu> 0:1.4.12.1-1
- Build for F-13
- Port forward to 1.4.12.1

* Mon Mar 15 2010 Jack Neely <jjneely@ncsu.edu> 0:1.4.12-1
- Update to OpenAFS 1.4.12
- OpenAFS has moved compile_et to afs_compile_et so that it
  can be safely included without causing package conflicts

* Mon Jan 04 2010 Jack Neely <jjneely@ncsu.edu> 0:1.4.11-10
- Add a BuildRequires for bison to build on PPC

* Tue Nov 03 2009 Jack Neely <jjneely@ncsu.edu> 0:1.4.11-9
- Remove the epoch tags as they are generally not accepted by the
  Fedora Packaging Guidelines

* Wed Oct 21 2009 Jack Neely <jjneely@ncsu.edu> 0:1.4.11-8
- Clarify the comment about removing the compile_et binary
- This package produces no kernel module cruft so we don't need
  to rm -f those files.

* Wed Oct 14 2009 Jack Neely <jjneely@ncsu.edu> 0:1.4.11-7
- Further static library cleanups

* Wed Oct 14 2009 Jack Neely <jjneely@ncsu.edu> 0:1.4.11-6
- Remove static libraries from the devel package
- Install the PAM modules (although most folks probably should use
  pam_krb5afs from krb5)
- Document the spec a bit more for the files that are removed

* Tue Oct 13 2009 Jack Neely <jjneely@ncsu.edu> 0:1.4.11-5
- replace /etc with _sysconfdir macro
- Honor $RPM_OPT_FLGS
- Build is not SMP compliant -- no _smp_mflags macro
- remove the makeinstall macro
- remove /var/cache/openafs as we use the memcache option for its speed

* Tue Oct 13 2009 Jack Neely <jjneely@ncsu.edu> 0:1.4.11-4
- Corrected URL tag

* Wed Sep 02 2009 Jack Neely <jjneely@ncsu.edu> 0:1.4.11-3
- rebuild for 1.4.11
- Remove NCSU custom configuration
- Add the /usr/bin/aklog binary back

* Wed Jun 03 2009 Jack Neely <jjneely@ncsu.edu> 1.4.10-2
- Work for fc11 versions
- init.d file as well as sysconfig file rename afs => openafs
- Prereq/Require cleanups

* Thu May 16 2008 Jack Neely <jjneely@ncsu.edu> 1.4.6-7
- Include the AFS version of asetkey
- Build with --with-krb5-conf which enables aklog and asetkey's builds

* Wed May  7 2008 Jack Neely <jjneely@ncsu.edu> 1.4.6-5
- Include /usr/sbin/vlclient in the -server package
- Include kpasswd.kas

* Wed Apr 09 2008 Jack Neely <jjneely@ncsu.edu> 1.4.6-4
- Removed the -fakestat-all flag from the AFS configuration as
  it doesn't work properly with our current server version.
- Change /etc/sysconfig/afs to be a noreplace config

* Tue Feb 12 2008 Jack Neely <jjneely@ncsu.edu> 1.4.6-1
- OpenAFS 1.4.6

* Mon Jul 11 2007 Jack Neely <jjneely@ncsu.edu> 1.4.4-3
- RHEL 5 does not include the krbafs-utils package and we require
  the pagsh program.

* Tue Mar 27 2007 Jack Neely <jjneely@ncsu.edu>
- Bug #413 - security vulnerability in OpenAFS < 1.4.4

* Wed Jun 28 2006 Jack Neely <jjneely@ncsu.edu> 1.4.1-4
- Migrate to the Fedora Kernel Module packaging guidelines
- These are the userland packages

* Thu Jan 19 2006 Jack Neely <jjneely@ncsu.edu> 1.4.0-1
- Use 1.4.0 final which is identical to rc8 IIRC

* Thu Jul 21 2005 Jack Neely <jjneely@pams.ncsu.edu>
- Ported openafs-2.6 specs to work on i386 and x86_64

* Wed Jan 05 2005 Jack Neely <jjneely@pams.ncsu.edu>
- RHEL4 betas have a new kernel-devel package that's oh such an improvement
- Much work for 2.6 kernels

* Wed Jun 09 2004 Jack Neely <jjneely@pams.ncsu.edu>
- There are no more enterprise kernels, so I've removed all the if/else
  clauses for it.
- Added support for the 2.6 kernel.  You'll need to set the define
  k_minor

* Mon Jan 12 2004 Jack Neely <jjneely@pams.ncsu.edu>
- Sorted out the installed but not packaged files
- We now include kdump, libafsauthent, libafsrpc, and translate_et
  while other files are rm -f'd.

* Fri Nov 07 2003 Jack Neely <jjneely@pams.ncsu.edu>
- No longer build i386 kernel modules
- Only build enterprise kernel modules when build_enterprise is true
- Upgraded to Openafs 1.2.10

* Wed Apr 23 2003 Jack Neely <slack@quackmaster.net>
- Upgraded to OpenAFS 1.2.9 final

* Wed Mar 26 2003 Jack Neely <slack@quackmaster.net>
- Built for RHL9

* Fri Jan 10 2003 Jack Neely <slack@quackmaster.net
- Build OpenAFS 1.2.8
- We can now define our sysname list in /etc/sysconfig/afs
- the init script will set above sysname list

* Wed Oct 09 2002 Jack Neely <slack@quackmaster.net>
- Build of OpenAFS 1.2.7
- Incorperated pcb's spec file changes so we now have builds for
  multiple archs, up and smp
- Build for RHL 8.0

* Tue May 28 2002 Jack Neely <slack@quackmaster.net>
- Build of OpenAFS 1.2.4

* Wed May 08 2002 Jack Neely <jjneely@pams.ncsu.edu>
- rebuilt on 2.4.18-3

* Fri Mar 21 2002 Jack Neely <slack@quackmaster.net>
- upgraded to openafs-1.2.3
- rebuilt on 2.4.18-0.4

* Thu Mar 13 2002 Jack Neely <slack@quackmaster.net>
- rebuild on 2.4.9-31
- Added kernel depend on the kernel package

* Sun Oct 21 2001 Jeremy Katz <katzj@redhat.com>
- rebuild on 2.4.9-7
- make kernel subpackage require kernel

* Tue Oct 16 2001 Jeremy Katz <katzj@redhat.com>
- update to 1.2.2 which integrates my patches

* Tue Oct  9 2001 Jeremy Katz <katzj@redhat.com>
- minor scriptlet fixes
- update paths for server bins in initscript
- take network_on function from upstream rc script and tweak

* Sat Oct  6 2001 Jeremy Katz <katzj@redhat.com>
- update to 1.2.1
- add config vars in /etc/sysconfig/afs for cachedir and confdir

* Sat Oct  6 2001 Jeremy Katz <katzj@redhat.com>
- use /etc/openafs instead of /etc/afsws
- switch to using a 100M cache in /var/cache/openafs

* Fri Oct  5 2001 Jeremy Katz <katzj@redhat.com>
- update to 1.2.0, wow that build system has changed a lot
- patch configure.in so that makeinstall will work properly
- use redhat fix stuff (adapted from upstream specfile)
- more complete filelist
- macro-ize
- fixups for the post, pre scripts to correspond to the right packages
- fix dirpath.h
- kill /usr/vice/etc
- in theory alpha, ia64, ppc, and s390 builds should work now (untested)
- add kernel-source subpackage

* Mon Apr 30 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- update to 1.0.4 final
- for now, you need to actually change /boot/kernel.h to get modules built
  for arch != arch of kernel running on build host

* Thu Apr 26 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- tweak the defines, maybe it'll work right now :-)

* Wed Apr 25 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- apply patches before making links
- added kernel subpackage with kernel module.  Note that you now need to
  rebuild normally which will get you i386 kernel modules and then rebuild
  with --target=i586 and --target=i686 to get i586 and i686 kernel module 
  packages respectively

* Tue Apr 24 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- add more fixes to look for kernel headers in the right place

* Tue Apr 24 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- merge patches up to openafs CVS from 2001-04-24 to see if it fixes SMP
  problems 

* Tue Apr 10 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- rebuild on 2.4.2-2

* Mon Apr  2 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- rebuild on 2.4.2-0.1.49

* Wed Mar 28 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- rebuild on 2.4.2-0.1.40

* Thu Mar 22 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- rebuild on 2.4.2-0.1.32

* Wed Mar 21 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- rebuild on 2.4.2-0.1.29

* Fri Mar  9 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- update to openafs 1.0.3
- modversions patch is merged so we don't need to apply it now
- merge other patches as needed
- rebuild on 2.4.2-0.1.28

* Thu Mar  8 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- fix typo in the initscript
- rebuild on 2.4.2-0.1.22
- let the post for the client run depmod for the proper kernel version

* Tue Mar  6 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- new initscript that matches Red Hat init scripts better
- minor specfile tweaks
- rebuild on 2.4.2-0.1.19

* Tue Feb 20 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- really fix #153
- add some minor fixes to the init script

* Mon Feb 19 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- ensure network is up to start (#153)
- rebuild on 2.4.1-0.1.9

* Wed Feb 14 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- resolve bug #148 (thanks to Mike Sklar of Morgan Stanley for suggestions)
  run posts for the proper subpackages (#148)
  add a pre to rpmsave /usr/vice/etc (#148)
  add $AFS_MOUNT_POINT to /etc/sysconfig/afs 
- minor init script fixes
- rebuild on 2.4.0-0.99.24

* Thu Feb  8 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- fix unresolved symbols on SMP build
- rebuild on 2.4.0-0.99.23

* Tue Jan 30 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- get /usr/vice/etc right
- clean up the post and preun

* Tue Jan 30 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- include modversions patch from Michael Duggan <md5i@cs.cmu.edu>

* Tue Jan 30 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- add a new openafs base package which contains common files
- move lots of stuff around; bins now in /usr/bin and /usr/sbin
  for commonly used bins, modules in /lib/modules/$(uname -r)/kernel/fs
  (some of this is from the upstream openafs spec file)
- new init script which tries to be start and do a few retries on shutdown

* Mon Jan 29 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- don't build a lot of the obsolete stuff (ftpd, inetd, etc)

* Sun Jan 28 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- finally get working with ac kernels.  struct vnode is evil
- rebuild on 2.4.0-0.99.11

* Wed Jan 24 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- patch, patch, patch all day long

* Wed Jan 24 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- patch to fix dirents problem and hang on shutdown
- build on 2.4.0-0.99.10

* Fri Jan 19 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- let's try 1.0.2

* Thu Jan 18 2001 Jeremy Katz <jlkatz@eos.ncsu.edu>
- build 1.0.1 with patch from Chas Williams <chas@cmf.nrl.navy.mil> for 2.4
- next step is cleaning up this package

* Mon Nov  6 2000 Jeremy Katz <jlkatz@eos.ncsu.edu>
- patch from Matt Wilson <msw@redhat.com> to clean up the kernel
  version mangling, add prelim alpha_linux22 support, build lwp with
  kgcc to avoid a compiler bug
- make thiscell definable for easier builds at other sites

* Thu Nov  2 2000 Jeremy Katz <jlkatz@eos.ncsu.edu>
- conflicts with arla and afs packages

* Thu Nov  2 2000 Jeremy Katz <jlkatz@eos.ncsu.edu>
- initial RPM build (largely based off of NCSU's AFS nosrc spec file)
