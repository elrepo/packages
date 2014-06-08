Name: lm_sensors
Version: 2.10.8
Release: 3%{?dist}
URL: http://secure.netroedge.com/~lm78/
Source: http://secure.netroedge.com/~lm78/archive/lm_sensors-%{version}.tar.gz
Source1: lm_sensors.sysconfig
Source2: lm_sensors.init
#Patch1: lm_sensors-2.5.5-glibc22.patch
Patch2: lm_sensors-2.8.3-redhat.patch
Patch3: lm_sensors-2.10.7-utf8.patch
#Patch4: lm_sensors-2.8.2-expr.patch
#Patch5: lm_sensors-2.8.3-local.patch
Patch6: lm_sensors-2.8.3-rpath.patch
Patch7: lm_sensors-2.8.7-udev.patch
#Patch8: lm_sensors-2.10.0-kernel26.patch
#Patch9: lm_sensors-2.10.7-driver.patch
#Patch99: lm_sensors-2.10.7-coretemp.patch
#Patch100: lm_sensors-2.10.7-i7.patch
Patch101: lm_sensors-2.10.7-k10temp.patch
Patch102: lm_sensors-2.10.8-emc6d103.patch
Summary: Hardware monitoring tools.
Group: Applications/System
License: GPL
Buildroot: %{_tmppath}/%{name}-root
Requires: /usr/sbin/dmidecode
Requires(preun): chkconfig
Requires(post): chkconfig
BuildRequires: kernel-headers >= 2.2.16, bison, libsysfs-devel, flex
ExclusiveArch: alpha %{ix86} x86_64

%description
The lm_sensors package includes a collection of modules for general SMBus
access and hardware monitoring.  NOTE: this requires special support which
is not in standard 2.2-vintage kernels.

%package devel
Summary: Development files for programs which will use lm_sensors.
Group: Development/System
Requires: lm_sensors = %{version}-%{release} 

%description devel
The lm_sensors-devel package includes a header files and libraries for use
when building applications that make use of sensor data.

%prep
%setup -q 
#%patch1 -p1 -b .glibc22
%patch2 -p1 -b .redhat
%patch3 -p1 -b .utf8
#%patch4 -p1 -b .expr
#%patch5 -p1 -b .local
%patch6 -p1 -b .rpath
%patch7 -p1 -b .udev
#%patch8 -p1 -b .kernel26
#%patch9 -p1 -b .driver
# added patch99 to reinstate coretemp driver detection removed in patch9 - PJP
#%patch99 -p1 -b .coretemp
# added patch100 to add i7 (Nehalem) coretemp driver  detection support - PJP
#%patch100 -p1 -b .i7
%patch101 -p1 -b .k10temp
%patch102 -p1 -b .emc6d103

%build
mkdir -p kernel/include/linux
ln -sf /usr/include/linux/* kernel/include/linux
export CFLAGS="%{optflags}"
make prefix=/usr exec_prefix=/usr bindir=/usr/bin sbindir=/usr/sbin sysconfdir=/etc datadir=/usr/share includedir=/usr/include libdir=%{_libdir} libexecdir=/usr/libexec localstatedir=/var sharedstatedir=/usr/com mandir=/usr/share/man infodir=/usr/share/info user

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -fr $RPM_BUILD_ROOT
make prefix=/usr exec_prefix=/usr bindir=/usr/bin sbindir=/usr/sbin sysconfdir=/etc datadir=/usr/share includedir=/usr/include libdir=%{_libdir} libexecdir=/usr/libexec localstatedir=/var sharedstatedir=/usr/com mandir=/usr/share/man infodir=/usr/share/info DESTDIR=$RPM_BUILD_ROOT user_install
chmod 755 $RPM_BUILD_ROOT%{_libdir}/*.so*

mv prog/init/README prog/init/README.initscripts

# Remove userland kernel headers, belong in glibc-kernheaders.
rm -rf $RPM_BUILD_ROOT%{_includedir}/linux

mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig
install -m 0644 %{SOURCE1} $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/lm_sensors
mkdir -p $RPM_BUILD_ROOT%{_initrddir}
install -m 0755 %{SOURCE2} $RPM_BUILD_ROOT%{_initrddir}/lm_sensors

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -fr $RPM_BUILD_ROOT

%pre
if [ -f /var/lock/subsys/sensors ]; then
	mv -f /var/lock/subsys/sensors /var/lock/subsys/lm_sensors
fi

%post
/sbin/ldconfig
/sbin/chkconfig --add lm_sensors

%postun -p /sbin/ldconfig

%preun
if [ $1 = 0 ]; then
    /sbin/chkconfig --del lm_sensors
fi


%files
%defattr(-,root,root)
%doc BACKGROUND BUGS CHANGES CONTRIBUTORS COPYING doc INSTALL QUICKSTART README* TODO prog/init/*
%config(noreplace) %{_sysconfdir}/sensors.conf
%{_bindir}/*
%{_libdir}/*.so.*
%{_mandir}/man*/*
%{_sbindir}/*
%config %{_initrddir}/lm_sensors
%config(noreplace) %{_sysconfdir}/sysconfig/lm_sensors

%files devel
%defattr(-,root,root)
%{_includedir}/sensors
%{_libdir}/lib*.a
%{_libdir}/lib*.so

%changelog
* Thu Sep 12 2013 Philip J Perry <phil@elrepo.org> - 2.10.8-3
- Backport EMC6D103 support (requires lm85 module)

* Sat Jul 11 2009 Wes Wright <info@riversedgesoftware.com> - 2.10.8-2
- AMD K10 support (requires k10temp module)

* Sat May 03 2009 Philip J Perry <phil@elrepo.org> - 2.10.8-1
- Rebase to lm_sensors-2.10.8

* Wed Apr 08 2009 Philip J Perry <phil@elrepo.org> - 2.10.7-4.2
- Added patch for i7 (Nehalem) coretemp support [ John Clover ]

* Fri Mar 06 2009 Philip J Perry <phil@elrepo.org> - 2.10.7-4.1
- Reinstate coretemp driver to support kmod-coretemp

* Fri Nov 28 2008 Phil Knirsch <pknirsch@redhat.com> - 2.10.7-4
- Final fix for the unsupported driver problem
- Resolves: #473069

* Wed Nov 26 2008 Phil Knirsch <pknirsch@redhat.com> - 2.10.7-3
- Removed detection of unsupported drivers for current kernel
- Resolves: #473069

* Thu Aug 14 2008 Phil Knirsch <pknirsch@redhat.com> - 2.10.7-2
- Fixed requires of development package (#428990)

* Tue Aug 12 2008 Phil Knirsch <pknirsch@redhat.com> - 2.10.7-1
- Rebase to lm_sensors-2.10.7 (#428990)

* Wed Jul 12 2006 Jesse Keating <jkeating@redhat.com> - 2.10.0-3.1
- rebuild

* Sun Jul 09 2006 Warren Togami <wtogami@redhat.com> 2.10.0-3
- change buildreq from sysfsutils-devel to libsysfs-devel (#198055)

* Mon Jun 05 2006 Jesse Keating <jkeating@redhat.com> 2.10.0-2
- Fix BuildRequires, added flex. (#193511)  Changed to Requires(post) and 
  (postun)

* Fri May 12 2006 Phil Knirsch <pknirsch@redhat.com> 2.10.0-1
- Update to lm_sensors-2.10.0
- Added missing buildprereq on sysfsutils-devel (#189196)
- Added missing prereq on chkconfig (#182838)
- Some fiddling to make it build on latest kernels

* Wed Feb 15 2006 Phil Knirsch <pknirsch@redhat.com> 2.9.2-2
- Added missing dependency to chkconfig

* Fri Feb 10 2006 Phil Knirsch <pknirsch@redhat.com> 2.9.2-1
- Update to lm_sensors-2.9.2
- Fixed wrong subsys locking (#176965)
- Removed lm_sensors pwmconfig, has been fixed upstream now

* Tue Feb 07 2006 Jesse Keating <jkeating@redhat.com> - 2.9.1-6.1
- rebuilt for new gcc4.1 snapshot and glibc changes

* Mon Jan 16 2006 Peter Jones <pjones@redhat.com> 2.9.1-6
- fix initscript subsys locking

* Fri Dec 16 2005 Jesse Keating <jkeating@redhat.com> 2.9.1-5.1
- rebuilt for new gcj

* Tue Nov 08 2005 Phil Knirsch <pknirsch@redhat.com> 2.9.1-5
- Fixed lm_sensors pwmconfig patch.

* Tue Sep 01 2005 Phil Knirsch <pknirsch@redhat.com> 2.9.1-4
- Fixed CAN-2005-2672 lm_sensors pwmconfig insecure temporary file usage
  (#166673)
- Fixed missing optflags during build (#166910)

* Mon May 23 2005 Phil Knirsch <pknirsch@redhat.com> 2.9.1-3
- Update to lm_sensors-2.9.1
- Fixed wrong/missing location variables for make user
- Fixed missing check for /etc/modprobe.conf in sensors-detect (#139245)

* Wed Mar 02 2005 Phil Knirsch <pknirsch@redhat.com> 2.8.8-5
- bump release and rebuild with gcc 4

* Tue Jan 11 2005 Dave Jones <davej@redhat.com> 2.8.8-4
- Add dependancy on dmidecode rather than the obsolete kernel-utils.
- Don't delete dmidecode from the buildroot.

* Thu Dec 23 2004 Phil Knirsch <pknirsch@redhat.com> 2.8.8-2
- Fixed typo in initscript (#139030)

* Tue Dec 21 2004 Phil Knirsch <pknirsch@redhat.com> 2.8.8-1
- Added Buildprereq for bison (#138888)
- Update to lm_sensors-2.8.8

* Thu Oct 14 2004 Harald Hoyer <harald@redhat.com> 2.8.7-2
- added initial /etc/sysconfig/lm_sensors
- added initscript
- MAKEDEV the initial i2c devices in initscript and sensors-detect

* Tue Jul 06 2004 Phil Knirsch <pknirsch@redhat.com> 2.8.7-1
- Update to latest upstream version.

* Tue Jun 15 2004 Elliot Lee <sopwith@redhat.com>
- rebuilt

* Tue Apr 13 2004 Phil Knirsch <pknirsch@redhat.com> 2.8.6-1
- Update to latest upstream version.
- Enabled build for x86_64.

* Mon Mar 08 2004 Phil Knirsch <pknirsch@redhat.com> 2.8.3-5
- Fixed initscript to work with 2.6 kernel and made it more quiet (#112286).
- Changed proposed location of sensors (#116496).
- Fixed rpath issue.

* Fri Feb 13 2004 Elliot Lee <sopwith@redhat.com>
- rebuilt

* Thu Feb 05 2004 Phil Knirsch <pknirsch@redhat.com> 2.8.3-3
- Modified sensors.conf to a noreplace config file.

* Wed Feb 04 2004 Phil Knirsch <pknirsch@redhat.com> 2.8.3-2
- Fixed newly included initscript (#114608).

* Thu Jan 29 2004 Phil Knirsch <pknirsch@redhat.com> 2.8.3-1
- Updated to latest upstream version 2.8.3

* Thu Jan 08 2004 Phil Knirsch <pknirsch@redhat.com> 2.8.2-1
- Update to latest upstream version 2.8.2
- Fixed wrong & usage in if expression.
- Included several new perl tools.

* Fri Oct 24 2003 Phil Knirsch <pknirsch@redhat.com> 2.8.1-1
- Update to latest upstream version 2.8.1

* Wed Jul 23 2003 Phil Knirsch <pknirsch@redhat.com> 2.8.0-1
- Update to latest upstream version 2.8.0

* Fri Jun 27 2003 Phil Knirsch <pknirsch@redhat.com> 2.6.5-6.1
- rebuilt

* Fri Jun 27 2003 Phil Knirsch <pknirsch@redhat.com> 2.6.5-6
- Included prog/init scripts and README (#90606).
- Require kernel-utils for dmidecode (#88367, #65057).

* Wed Jan 22 2003 Tim Powers <timp@redhat.com> 2.6.5-5
- rebuilt

* Wed Dec 04 2002 Phil Knirsch <pknirsch@redhat.com> 2.6.5-4
- Bump release and try to rebuild.

* Tue Dec  3 2002 Tim Powers <timp@redhat.com> 2.6.5-3
- don't include dmidecode, conflicts with kernel-utils

* Fri Nov 29 2002 Phil Knirsch <pknirsch@redhat.com> 2.6.5-2
- Added patch to fix utf8 problem with sensors-detect.
- Fixed Copyright: to License: in specfile

* Fri Nov 29 2002 Phil Knirsch <pknirsch@redhat.com> 2.6.5-1
- Updated userlevel to 2.6.5.
- Include all the /usr/sbin/ apps (like dmidecode).

* Fri Oct 04 2002 Phil Knirsch <pknirsch@redhat.com> 2.6.3-3
- Removed Serverworks patch as it is already in sensors-detect.

* Fri Jun 21 2002 Tim Powers <timp@redhat.com> 2.6.3-2
- automated rebuild

* Tue Jun 18 2002 Phil Knirsch <pknirsch@redhat.com> 2.6.3-1
- Updated of userland package to 2.6.3
- Fixed file packaging bug (#66126).

* Thu May 23 2002 Tim Powers <timp@redhat.com> 2.6.2-2
- automated rebuild

* Mon Jan 28 2002 Phil Knirsch <pknirsch@redhat.com> 2.6.2-1
- Update to version 2.6.2

* Wed Aug 22 2001 Philipp Knirsch <pknirsch@redhat.de> 2.5.5-6
- Added the SMBus CSB5 detection (#50468)

* Mon Jul  9 2001 Philipp Knirsch <pknirsch@redhat.de>
- Fixed duplicate Summary: entry for devel package (#47714)

* Sun Jun 24 2001 Elliot Lee <sopwith@redhat.com>
- Bump release + rebuild.

* Thu Feb 15 2001 Philipp Knirsch <pknirsch@redhat.de>
- Removed the i2c block patch as our newest kernel doesn't need it anymore.

* Mon Feb  5 2001 Matt Wilson <msw@redhat.com>
- added patch to not include sys/perm.h, as it's gone now.
- added alpha to ExclusiveArch
- use make "LINUX_HEADERS=/usr/include" to get kernel headers

* Tue Jan 16 2001 Philipp Knirsch <pknirsch@redhat.de>
- Updated to 2.5.5 which includes the Serverworks drivers. Kernel modules are
  not included though es they have to go into the kernel package
- Had to remove all references to I2C_SMBUS_I2C_BLOCK_DATA from
  kernel/busses/i2c-i801.c and prog/dump/i2cdump.c as this is not defined in
  our current kernel package

* Tue Dec 19 2000 Philipp Knirsch <pknirsch@redhat.de>
- update to 2.5.4
- updated URL and Source entries to point to new home of lm-sensors
- rebuild

* Wed Aug 16 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix summary

* Fri Jul 28 2000 Harald Hoyer <harald@redhat.de>
- added static library to devel package

* Thu Jul 20 2000 Nalin Dahyabhai <nalin@redhat.com>
- update to 2.5.2
- build against a kernel that actually has new i2c code in it

* Wed Jul 12 2000 Prospector <bugzilla@redhat.com>
- automatic rebuild

* Mon Jun 12 2000 Nalin Dahyabhai <nalin@redhat.com>
- initial package without kernel support
