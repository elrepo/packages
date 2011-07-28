%define		atilibdir	%{_libdir}/fglrx
%define		atilib32dir	%{_prefix}/lib/fglrx
%define		desktop_vendor	ati
%define		debug_package	%{nil}

%ifarch i386
  %define atiarch x86
  %define xorgver x710
%endif

%ifarch x86_64
  %define atiarch x86_64
  %define xorgver x710_64a
%endif

Name:		fglrx93-x11-drv
Version:	9.3
Release:	1%{?dist}
Group:		User Interface/X Hardware Support
License:	Proprietary 
Summary:	AMD's proprietary driver for ATI graphic cards
URL:		http://support.amd.com/us/gpudownload/linux/Pages/radeon_linux.aspx

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	i386 x86_64

# Sources
Source0:	http://www2.ati.com/drivers/linux/ati-driver-installer-9-3-x86.x86_64.run

# taken from the rpmforge dkms package
Source2:	ati.sh
Source3:	ati.csh
Source4:	ati-config-display
Source5:	ati.nodes

# provides desktop-file-install
BuildRequires:	desktop-file-utils

Requires:	fglrx93-kmod = %{?epoch:%{epoch}:}%{version}-%{release}
Requires(post):	fglrx93-kmod = %{?epoch:%{epoch}:}%{version}-%{release}

Requires(post):  chkconfig
Requires(preun): chkconfig
Requires(post):  /sbin/ldconfig

# for ati-config-display
Requires(post):  pyxf86config
Requires(preun): pyxf86config

# ATI RPM
Conflicts:       fglrx_7_1_0
Conflicts:       fglrx64_7_1_0

# elrepo
Conflicts:	fglrx-kmod
Conflicts:	fglrx-x11-drv
Conflicts:	fglrx-x11-drv-32bit

# rpmforge
Conflicts:	dkms-ati
Conflicts:	dkms-fglrx
Conflicts:	ati-x11-drv

# rpmfusion
Conflicts:	catalyst-kmod
Conflicts:	xorg-x11-drv-catalyst

%description
This package provides the proprietary AMD OpenGL X11 display driver files.

%package 32bit
Summary:	Compatibility 32-bit files for the 64-bit Proprietary AMD driver
Group:		User Interface/X Hardware Support
Requires:	%{name} = %{version}-%{release}
Requires(post):	/sbin/ldconfig

%description 32bit
Compatibility 32-bit files for the 64-bit Proprietary AMD driver.

%package devel
Summary:	Development files for AMD OpenGL X11 display driver.
Group:		Development/Libraries
Requires:	%{name} = %{version}-%{release}

%description devel
This package provides the development files for the AMD OpenGL X11 display driver,
such as OpenGL headers.

%prep
%setup -q -c -T

%{_buildshell} %{SOURCE0} --extract .

# fix doc perms
find common/usr/share/doc -type f -exec chmod 0644 {} \;

%build
# Nothing to build

%install
%{__rm} -rf %{buildroot}

# Install utilities, atieventsd program with its init script and man pages
%{__mkdir_p} %{buildroot}{%{_bindir},%{_sbindir},%{_sysconfdir}/ati}
%{__install} -p -m 0755 arch/%{atiarch}/usr/X11R6/bin/* %{buildroot}%{_bindir}/
%{__install} -p -m 0755 common/usr/X11R6/bin/* %{buildroot}%{_bindir}
%{__install} -p -m 0755 arch/%{atiarch}/usr/sbin/* %{buildroot}%{_sbindir}/
%{__install} -p -m 0755 common/usr/sbin/* %{buildroot}%{_sbindir}/
%{__install} -p -m 0644 common/etc/ati/* %{buildroot}%{_sysconfdir}/ati/
# fix permissions on authatieventsd.sh
%{__chmod} 755 %{buildroot}%{_sysconfdir}/ati/authatieventsd.sh
%{__install} -D -p -m 0755 packages/Fedora/atieventsd.init %{buildroot}%{_sysconfdir}/rc.d/init.d/atieventsd
%{__install} -D -p -m 0644 common/usr/share/man/man8/atieventsd.8 %{buildroot}%{_mandir}/man8/atieventsd.8
%{__install} -D -p -m 0755 %{SOURCE4} %{buildroot}%{_sbindir}/

# Install libraries
%{__mkdir_p} %{buildroot}%{atilibdir}
%{__install} -p -m 0755 arch/%{atiarch}/usr/X11R6/%{_lib}/*.* %{buildroot}%{atilibdir}
%{__install} -p -m 0755 arch/%{atiarch}/usr/%{_lib}/*.* %{buildroot}%{atilibdir}

# Driver modules
%{__mkdir_p} %{buildroot}%{_libdir}/dri/
%{__mkdir_p} %{buildroot}%{_libdir}/xorg/modules/{drivers,linux}
%{__mkdir_p} %{buildroot}%{_libdir}/xorg/modules/extensions/fglrx
%{__install} -p -m 0755 arch/%{atiarch}/usr/X11R6/%{_lib}/modules/dri/* %{buildroot}%{_libdir}/dri/
%{__install} -p -m 0755 %{xorgver}/usr/X11R6/%{_lib}/modules/drivers/* %{buildroot}%{_libdir}/xorg/modules/drivers/
%{__install} -p -m 0755 %{xorgver}/usr/X11R6/%{_lib}/modules/linux/* %{buildroot}%{_libdir}/xorg/modules/linux/
%{__install} -p -m 0755 %{xorgver}/usr/X11R6/%{_lib}/modules/*.* %{buildroot}%{_libdir}/xorg/modules/
%{__install} -p -m 0755 %{xorgver}/usr/X11R6/%{_lib}/modules/extensions/* %{buildroot}%{_libdir}/xorg/modules/extensions/fglrx/

# Install the 32-bit compatibility libs
%ifarch x86_64
  %{__mkdir_p} %{buildroot}%{atilib32dir}
  %{__mkdir_p} %{buildroot}%{_prefix}/lib/dri/
  %{__install} -p -m 0755 arch/x86/usr/X11R6/lib/modules/dri/* %{buildroot}%{_prefix}/lib/dri/
  %{__install} -p -m 0755 arch/x86/usr/X11R6/lib/*.* %{buildroot}%{atilib32dir}
  %{__install} -p -m 0755 arch/x86/usr/lib/*.* %{buildroot}%{atilib32dir}
%endif

# Create the symlinks
pushd %{buildroot}%{atilibdir}/
  %{__ln_s} libAMDXvBA.so.1.0 %{buildroot}%{atilibdir}/libAMDXvBA.so.1
  %{__ln_s} libfglrx_gamma.so.1.0 %{buildroot}%{atilibdir}/libfglrx_gamma.so.1
  %{__ln_s} libfglrx_tvout.so.1.0 %{buildroot}%{atilibdir}/libfglrx_tvout.so.1
  %{__ln_s} libGL.so.1.2 %{buildroot}%{atilibdir}/libGL.so.1
  %{__ln_s} libGL.so.1 %{buildroot}%{atilibdir}/libGL.so
  %{__ln_s} libXvBAW.so.1.0 %{buildroot}%{atilibdir}/libXvBAW.so.1
popd
%ifarch x86_64
  pushd %{buildroot}%{atilib32dir}/
    %{__ln_s} libAMDXvBA.so.1.0 %{buildroot}%{atilib32dir}/libAMDXvBA.so.1
    %{__ln_s} libfglrx_gamma.so.1.0 %{buildroot}%{atilib32dir}/libfglrx_gamma.so.1
    %{__ln_s} libfglrx_tvout.so.1.0 %{buildroot}%{atilib32dir}/libfglrx_tvout.so.1
    %{__ln_s} libGL.so.1.2 %{buildroot}%{atilib32dir}/libGL.so.1
    %{__ln_s} libGL.so.1 %{buildroot}%{atilib32dir}/libGL.so
    %{__ln_s} libXvBAW.so.1.0 %{buildroot}%{atilib32dir}/libXvBAW.so.1
  popd
%endif

# Install header files and sample
%{__mkdir_p} %{buildroot}%{_includedir}/GL
%{__mkdir_p} %{buildroot}%{_includedir}/X11/extensions
%{__mkdir_p} %{buildroot}%{_usrsrc}/ati/
%{__install} -p -m 0644 common/usr/include/GL/*.h %{buildroot}%{_includedir}/GL/
%{__install} -p -m 0644 common/usr/X11R6/include/X11/extensions/*.h %{buildroot}%{_includedir}/X11/extensions/
%{__install} -p -m 0644 common/usr/src/ati/fglrx_sample_source.tgz %{buildroot}%{_usrsrc}/ati/fglrx_sample_source.tgz

# Install icon
%{__mkdir_p} %{buildroot}%{_datadir}/icons
%{__install} -p -m 0644 common%{_datadir}/icons/* %{buildroot}%{_datadir}/icons/

# Install ACPI config and scripts
%{__mkdir_p} %{buildroot}%{_sysconfdir}/acpi/{actions,events}
%{__install} -p -m 0755 packages/Fedora/ati-powermode.sh %{buildroot}%{_sysconfdir}/acpi/actions/ati-powermode.sh
%{__install} -p -m 0644 packages/Fedora/a-ac-aticonfig %{buildroot}%{_sysconfdir}/acpi/events/ac-aticonfig
%{__install} -p -m 0644 packages/Fedora/a-lid-aticonfig %{buildroot}%{_sysconfdir}/acpi/events/lid-aticonfig

# Install ld.so.conf.d file
%{__mkdir_p} %{buildroot}%{_sysconfdir}/ld.so.conf.d/
echo %{atilibdir} > %{buildroot}%{_sysconfdir}/ld.so.conf.d/ati.conf.disable
%ifarch x86_64
  echo %{atilib32dir} >> %{buildroot}%{_sysconfdir}/ld.so.conf.d/ati.conf.disable
%endif

# Install profile.d files
%{__mkdir_p} %{buildroot}%{_sysconfdir}/profile.d/
%{__install} -p -m 0644 %{SOURCE2} %{buildroot}%{_sysconfdir}/profile.d/ati.sh
%{__install} -p -m 0644 %{SOURCE3} %{buildroot}%{_sysconfdir}/profile.d/ati.csh

# Desktop entries
%{__mkdir_p} %{buildroot}%{_datadir}/applications
%{__sed} -i -e '/=$/d' -e 's,^Icon=.*$,Icon=%{_datadir}/icons/ccc_large.xpm,' \
         -e '/^MiniIcon/g' -e '/^Terminal/g' -e '/^OnlyShowIn/g' -e '/^$/d' \
         common/usr/share/gnome/apps/*.desktop
%{_bindir}/desktop-file-install --vendor %{desktop_vendor} \
  --dir %{buildroot}%{_datadir}/applications \
  --add-category System \
  --add-category Settings \
  --add-category Application \
  --add-category GNOME \
  common/usr/share/gnome/apps/amdcccle.desktop
%{_bindir}/desktop-file-install --vendor %{desktop_vendor} \
  --dir %{buildroot}%{_datadir}/applications \
  --add-category System \
  --add-category Settings \
  --add-category Application \
  --add-category GNOME \
  common/usr/share/gnome/apps/amdccclesu.desktop

# Control panel files
%{__mkdir_p} %{buildroot}%{_datadir}/ati/amdcccle
%{__install} -p -m 0644 common%{_datadir}/ati/amdcccle/* %{buildroot}%{_datadir}/ati/amdcccle

# Set the correct path for gdm's Xauth file
%{__sed} -i 's|GDM_AUTH_FILE=/var/lib/gdm/$1.Xauth|GDM_AUTH_FILE=/var/gdm/$1.Xauth|' %{buildroot}%{_sysconfdir}/ati/authatieventsd.sh

# Install udev configuration file
%{__mkdir_p} %{buildroot}%{_sysconfdir}/udev/makedev.d
%{__install} -p -m 0644 %{SOURCE5} %{buildroot}%{_sysconfdir}/udev/makedev.d/60-ati.nodes

%clean
%{__rm} -rf %{buildroot}

%post
# Enable the proprietary driver
%{_sbindir}/ati-config-display enable &>/dev/null
if [ "${1}" -eq 1 ]; then
  %{_sbindir}/chkconfig --add atieventsd
  %{_sbindir}/service atieventsd start
fi || :
/sbin/ldconfig

%post 32bit
/sbin/ldconfig

%preun
if [ "${1}" -eq 0 ]; then
  # Disable the proprietary driver on final uninstall
  [ -f %{_sbindir}/ati-config-display ] && %{_sbindir}/ati-config-display disable  &>/dev/null
  $(%{_sbindir}/service atieventsd status &>/dev/null) && %{_sbindir}/service atieventsd stop &>/dev/null
  # Remove init script
  /sbin/chkconfig --del atieventsd
fi || :

%postun
/sbin/ldconfig

%postun 32bit
/sbin/ldconfig

%files
%defattr(-,root,root,-)
%config %{_sysconfdir}/ld.so.conf.d/ati.conf.disable
%config %{_sysconfdir}/udev/makedev.d/60-ati.nodes
%dir %{_sysconfdir}/ati
%{_sysconfdir}/ati/*
%{_sysconfdir}/acpi/actions/ati-powermode.sh
%{_sysconfdir}/acpi/events/*aticonfig
%{_sysconfdir}/profile.d/ati.*
%{_initrddir}/atieventsd
%{_sbindir}/*
%{_bindir}/*
%doc common/usr/share/doc/fglrx/*
%{_datadir}/ati
%{_datadir}/applications/*amdcccle*.desktop
%{_datadir}/icons/*.xpm
%{_mandir}/man[1-9]/atieventsd.*
# libs
%dir %{atilibdir}
%{atilibdir}/*.so*
%{atilibdir}/libAMDXvBA.cap
%{_libdir}/dri/fglrx_dri.so
%{_libdir}/xorg/modules/*.so
%{_libdir}/xorg/modules/extensions/fglrx
%{_libdir}/xorg/modules/drivers/fglrx_drv.so
%{_libdir}/xorg/modules/linux/libfglrxdrm.so

# 32-bit compatibility libs
%ifarch x86_64
%files 32bit
%defattr(-,root,root,-)
%dir %{atilib32dir}
%{atilib32dir}/*.so*
%{atilib32dir}/libAMDXvBA.cap
%{_prefix}/lib/dri/fglrx_dri.so
%endif

# development files
%files devel
%defattr(-,root,root,-)
%dir %{_usr}/src/ati
%doc %{_usr}/src/ati/fglrx_sample_source.tgz
%{atilibdir}/*.a
%ifarch x86_64
  %{atilib32dir}/*.a
%endif
%{_libdir}/xorg/modules/*.a
%{_includedir}/GL/*.h
%{_includedir}/X11/extensions/*.h

%changelog
* Sun Dec 26 2010 Philip J Perry <phil@elrepo.org> - 9.3-1.el5.elrepo
- Build for release.

* Sun Dec 19 2010 Marco Giunta <marco.giunta AT sissa DOT it>
- Adapt to 9.3 version.

* Wed Dec 15 2010 Philip J Perry <phil@elrepo.org> - 10.12-1.el5.elrepo
- Update to version 10.12.

* Mon Dec 13 2010 Philip J Perry <phil@elrepo.org> - 10.11-1.el5.elrepo
- Rebuilt for release.

* Sun Dec 12 2010 Philip J Perry <phil@elrepo.org> - 10.11-0.2
- Fixed the symlinks.

* Sun Dec 12 2010 Philip J Perry <phil@elrepo.org> - 10.11-0.1
- Merge into elrepo for el5.

* Sat Dec 11 2010 Marco Giunta <marco.giunta AT sissa DOT it>
- Initial RPM release.
