%define		nvidialibdir	%{_libdir}/nvidia
%define		nvidialib32dir	%{_prefix}/lib/nvidia

%define		debug_package	%{nil}
%define		debug_packages	%{nil}

Name:		nvidia-x11-drv-173xx
Version:	173.14.39
Release:	1%{?dist}
Group:		User Interface/X Hardware Support
License:	Distributable
Summary:	NVIDIA 174.14.xx OpenGL X11 display driver files
URL:		http://www.nvidia.com/

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	i386 x86_64

# Sources.
# x86: pkg1 contains precompiled modules we don't need
Source0:	ftp://download.nvidia.com/XFree86/Linux-x86/%{version}/NVIDIA-Linux-x86-%{version}-pkg0.run

# x86_64: only pkg2 contains the lib32 compatibility libs
Source1:	ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}-pkg2.run

NoSource: 0
NoSource: 1

# taken from the rpmforge dkms package
Source2:	nvidia.sh
Source3:	nvidia.csh
Source4:	nvidia-config-display
Source5:	nvidia.modprobe
Source6:	nvidia.nodes

# provides desktop-file-install
BuildRequires:	desktop-file-utils
BuildRequires:	perl

Requires:	nvidia-173xx-kmod = %{?epoch:%{epoch}:}%{version}-%{release}
Requires(post):	nvidia-173xx-kmod = %{?epoch:%{epoch}:}%{version}-%{release}

Requires(post):	/sbin/ldconfig

# for nvidia-config-display
Requires(post):	 pyxf86config
Requires(preun): pyxf86config

# elrepo
Conflicts:	nvidia-x11-drv
Conflicts:	nvidia-x11-drv-32bit
Conflicts:	nvidia-x11-drv-96xx
Conflicts:	nvidia-x11-drv-96xx-32bit
Conflicts:	nvidia-x11-drv-304xx
Conflicts:	nvidia-x11-drv-304xx-32bit

# rpmforge
Conflicts:	dkms-nvidia
Conflicts:	dkms-nvidia-x11-drv
Conflicts:	dkms-nvidia-x11-drv-32bit

Conflicts:	xorg-x11-drv-nvidia
Conflicts:	xorg-x11-drv-nvidia-beta
Conflicts:	xorg-x11-drv-nvidia-legacy
Conflicts:	xorg-x11-drv-nvidia-71xx
Conflicts:	xorg-x11-drv-nvidia-96xx
Conflicts:	xorg-x11-drv-nvidia-173xx

%description
This package provides the proprietary NVIDIA 173.14.xx OpenGL X11 display driver files.

%package 32bit
Summary:	Compatibility 32-bit files for the 64-bit Proprietary NVIDIA driver
Group:		User Interface/X Hardware Support
Requires:	%{name} = %{version}-%{release}

%description 32bit
Compatibility 32-bit files for the 64-bit Proprietary NVIDIA driver.

%prep
%setup -q -c -T

%ifarch i386
sh %{SOURCE0} --extract-only --target nvidiapkg
%endif

%ifarch x86_64
sh %{SOURCE1} --extract-only --target nvidiapkg
%endif

# Lets just take care of all the docs here rather than in install
%{__mv} nvidiapkg/LICENSE nvidiapkg/usr/share/doc/
find nvidiapkg/usr/share/doc/ -type f | xargs chmod 0644

%build
# Nothing to build

%install
%{__rm} -rf $RPM_BUILD_ROOT

pushd nvidiapkg

# Install nvidia tools from /bin
%{__mkdir_p} $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 usr/bin/{nvidia-bug-report.sh,nvidia-settings,nvidia-xconfig} \
    $RPM_BUILD_ROOT%{_bindir}/

# Install GL and tls libs
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialibdir}/tls/
%{__install} -p -m 0755 usr/lib/*.so.%{version} \
    $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 usr/lib/libGL.la \
    $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 usr/lib/tls/*.so.%{version} \
    $RPM_BUILD_ROOT%{nvidialibdir}/tls/

%ifarch x86_64
# Install 32bit compat GL and tls libs
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialib32dir}/tls/
%{__install} -p -m 0755 usr/lib32/*.so.%{version} \
    $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 usr/lib32/libGL.la \
    $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 usr/lib32/tls/*.so.%{version} \
    $RPM_BUILD_ROOT%{nvidialib32dir}/tls/
%endif

# Install libXvMCNVIDIA
%{__install} -p -m 0755 usr/X11R6/lib/libXvMCNVIDIA.so.%{version} \
    $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0644 usr/X11R6/lib/libXvMCNVIDIA.a \
    $RPM_BUILD_ROOT%{nvidialibdir}/

# Install X driver and extension 
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/
%{__install} -p -m 0755 usr/X11R6/lib/modules/drivers/nvidia_drv.so \
    $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__install} -p -m 0755 usr/X11R6/lib/modules/extensions/libglx.so.%{version} \
    $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/
%{__install} -p -m 0755 usr/X11R6/lib/modules/libnvidia-wfb.so.%{version} \
    $RPM_BUILD_ROOT%{_libdir}/xorg/modules/


# Create the symlinks
%{__ln_s} libGLcore.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGLcore.so.1
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGL.so
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGL.so.1
%{__ln_s} libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-cfg.so
%{__ln_s} libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-cfg.so.1
%{__ln_s} libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-tls.so.1
%{__ln_s} libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/tls/libnvidia-tls.so.1
%{__ln_s} libvdpau.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libvdpau.so
%{__ln_s} libvdpau.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libvdpau.so.1
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libvdpau_nvidia.so
%{__ln_s} libvdpau_trace.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libvdpau_trace.so
%{__ln_s} libXvMCNVIDIA.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libXvMCNVIDIA_dynamic.so
%{__ln_s} libXvMCNVIDIA.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libXvMCNVIDIA_dynamic.so.1
%{__ln_s} libglx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/libglx.so
%{__ln_s} libnvidia-wfb.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/libwfb.so
%{__ln_s} libnvidia-wfb.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/libnvidia-wfb.so.1

%ifarch x86_64
# Create the 32-bit symlinks
%{__ln_s} libGLcore.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGLcore.so.1
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGL.so
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGL.so.1
%{__ln_s} libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-tls.so.1
%{__ln_s} libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/tls/libnvidia-tls.so.1
%{__ln_s} libvdpau.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libvdpau.so
%{__ln_s} libvdpau.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libvdpau.so.1
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libvdpau_nvidia.so
%{__ln_s} libvdpau_trace.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libvdpau_trace.so
%endif

# Install man pages
%{__mkdir_p} $RPM_BUILD_ROOT%{_mandir}/man1/
%{__install} -p -m 0644 usr/share/man/man1/nvidia-{settings,xconfig}.1.gz \
    $RPM_BUILD_ROOT%{_mandir}/man1/

# Install pixmap for the desktop entry
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/pixmaps/
%{__install} -p -m 0644 usr/share/pixmaps/nvidia-settings.png \
    $RPM_BUILD_ROOT%{_datadir}/pixmaps/

# Desktop entry for nvidia-settings
# Remove "__UTILS_PATH__/" before the Exec command name
# Replace "__PIXMAP_PATH__/" with the proper pixmaps path
%{__perl} -pi -e 's|(Exec=).*/(.*)|$1$2|g;
                  s|(Icon=).*/(.*)|$1%{_datadir}/pixmaps/$2|g' \
    usr/share/applications/nvidia-settings.desktop

%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/applications/
desktop-file-install --vendor elrepo \
    --dir $RPM_BUILD_ROOT%{_datadir}/applications/ \
    --add-category System \
    --add-category Application \
    --add-category GNOME \
    usr/share/applications/nvidia-settings.desktop

# Install profile.d files
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/profile.d/
%{__install} -p -m 0644 %{SOURCE2} \
    $RPM_BUILD_ROOT%{_sysconfdir}/profile.d/nvidia.sh
%{__install} -p -m 0644 %{SOURCE3} \
    $RPM_BUILD_ROOT%{_sysconfdir}/profile.d/nvidia.csh

# Install X configuration script
%{__mkdir_p} $RPM_BUILD_ROOT%{_sbindir}/
%{__install} -p -m 0755 %{SOURCE4} \
    $RPM_BUILD_ROOT%{_sbindir}/nvidia-config-display

# Install modprobe.d file
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/modprobe.d/
%{__install} -p -m 0644 %{SOURCE5} \
    $RPM_BUILD_ROOT%{_sysconfdir}/modprobe.d/nvidia

# Install udev configuration file
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/udev/makedev.d/
%{__install} -p -m 0644 %{SOURCE6} \
    $RPM_BUILD_ROOT%{_sysconfdir}/udev/makedev.d/60-nvidia.nodes

# Install ld.so.conf.d file
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/
echo %{nvidialibdir} > $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nvidia.conf
%ifarch x86_64
echo %{nvidialib32dir} >> $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nvidia.conf
%endif

popd

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%post
if [ "$1" -eq "1" ]; then
    # If xorg.conf doesn't exist, create it
    [ ! -f %{_sysconfdir}/X11/xorg.conf ] && %{_bindir}/nvidia-xconfig &>/dev/null
    # Make sure we have a Files section in xorg.conf, otherwise create an empty one
    XORGCONF=/etc/X11/xorg.conf
    [ -w ${XORGCONF} ] && ! grep -q 'Section "Files"' ${XORGCONF} && \
      echo -e 'Section "Files"\nEndSection' >> ${XORGCONF}
    # Enable the proprietary nvidia driver
    %{_sbindir}/nvidia-config-display enable &>/dev/null
fi || :

/sbin/ldconfig

%postun
/sbin/ldconfig

%preun
# Disable proprietary nvidia driver on uninstall
if [ "$1" -eq "0" ]; then
    test -f %{_sbindir}/nvidia-config-display && %{_sbindir}/nvidia-config-display disable &>/dev/null || :
fi

%post 32bit
/sbin/ldconfig

%postun 32bit
/sbin/ldconfig

%triggerin -- xorg-x11-server-Xorg
# Enable the proprietary nvidia driver
# Required since xorg-x11-server-Xorg empties the "Files" section
test -f %{_sbindir}/nvidia-config-display && %{_sbindir}/nvidia-config-display enable &>/dev/null || :

%files
%defattr(-,root,root,-)
%doc nvidiapkg/usr/share/doc/*
%{_mandir}/man1/nvidia*.*
%{_datadir}/pixmaps/nvidia-settings.png
%{_datadir}/applications/*nvidia-settings.desktop
%config(noreplace) %{_sysconfdir}/profile.d/nvidia.csh
%config(noreplace) %{_sysconfdir}/profile.d/nvidia.sh
%{_bindir}/nvidia*
%{_sbindir}/nvidia-config-display
%config %{_sysconfdir}/modprobe.d/nvidia
%config %{_sysconfdir}/ld.so.conf.d/nvidia.conf
%config %{_sysconfdir}/udev/makedev.d/60-nvidia.nodes

# now the libs
%dir %{nvidialibdir}
%{nvidialibdir}/lib*
%dir %{nvidialibdir}/tls
%{nvidialibdir}/tls/lib*
%{_libdir}/xorg/modules/libwfb.so
%{_libdir}/xorg/modules/libnvidia-wfb.so.*
%{_libdir}/xorg/modules/drivers/nvidia_drv.so
%dir %{_libdir}/xorg/modules/extensions/nvidia
%{_libdir}/xorg/modules/extensions/nvidia/libglx.*

# 32-bit compatibility libs
%ifarch x86_64
%files 32bit
%defattr(-,root,root,-)
%dir %{nvidialib32dir}
%{nvidialib32dir}/lib*
%dir %{nvidialib32dir}/tls
%{nvidialib32dir}/tls/lib*
%endif

%changelog
* Mon Feb 10 2014 Philip J Perry <phil@elrepo.org> - 173.14.39-1.el5.elrepo
- Update to version 173.14.39.

* Sat Mar 02 2013 Philip J Perry <phil@elrepo.org> - 173.14.36-1.el5.elrepo
- Update to version 173.14.36.
- Make package nosrc.
- Update conflicts for 304xx legacy package.
- Update post and preun scriptlets.

* Sat Aug 21 2010 Philip J Perry <phil@elrepo.org> - 173.14.27-1.el5.elrepo
- Update to version 173.14.27.

* Fri Apr 16 2010 Philip J Perry <phil@elrepo.org> - 173.14.25-3.el5.elrepo
- Run ldconfig on 32bit subpackages [BugID: 0000058]

* Thu Feb 04 2010 Philip J Perry <phil@elrepo.org> - 173.14.25-2.el5.elrepo
- Split 32-bit compatibility files into a sub-package on x86_64.

* Wed Feb 03 2010 Philip J Perry <phil@elrepo.org> - 173.14.25-1.el5.elrepo
- Forked to create nvidia-x11-drv-173xx legacy release.
- Added Conflicts against other nvidia packages.

* Sun Nov 01 2009 Philip J Perry <phil@elrepo.org> - 190.42-1.el5.elrepo
- Updated to version 190.42.

* Sat Sep 12 2009 Akemi Yagi <toracat@elrepo.org> - 185.18.36-2.el5.elrepo
- To match the version of kmod-nvidia 185.18.36-2

* Sat Sep 12 2009 Akemi Yagi <toracat@elrepo.org> - 185.18.36-1.el5.elrepo
- Update to version 185.18.36.

* Mon Aug 10 2009 Philip J Perry <phil@elrepo.org> - 185.18.31-1.el5.elrepo
- Update to version 185.18.31.

* Tue Jul 14 2009 Philip J Perry <phil@elrepo.org> - 185.18.14-1.el5.elrepo
- Rebuilt for release.

* Mon Jul 13 2009 Akemi Yagi <toracat@elrepo.org>, Dag Wieers <dag@wieers.com>
- Fix udev device creation.

* Fri Jul 10 2009 Dag Wieers <dag@wieers.com>
- Fix Requires to nvidia-kmod.

* Fri Jul 10 2009 Philip J Perry <phil@elrepo.org>
- Fixed permissions on nvidia.(c)sh that cause Xorg to fail.
- Don't create debug or strip the package (NVIDIA doesn't).

* Mon Jul 06 2009 Philip J Perry <phil@elrepo.org>
- Complete rewrite of SPEC file
- Added nvidia.(c)sh, nvidia.modprobe
- Added nvidia-config-display to enable/disable driver
- Added Requires (pyxf86config) and BuildRequires (desktop-file-utils, perl)

* Wed Jun 10 2009 Alan Bartlett <ajb@elrepo.org>
- Updated the package to 185.18.14 version.

* Wed Jun 10 2009 Philip J Perry <phil@elrepo.org>
- Added the Requires: /sbin/ldconfig line.
- Added the lines to create the /etc/ld.so.conf.d/nvidia.conf file.

* Wed May 27 2009 Philip J Perry <phil@elrepo.org>, Steve Tindall <s10dal@elrepo.org>
- Corrected the symlinks and their removal.

* Wed May 27 2009 Alan Bartlett <ajb@elrepo.org>
- Corrected typos in the pre & postun sections.

* Thu May 21 2009 Dag Wieers <dag@wieers.com>
- Adjusted the package name.

* Fri May 15 2009 Akemi Yagi <toracat@elrepo.org>
- Corrected the Requires: kmod-nvidia = %{?epoch:%{epoch}:}%{version}-%{release}

* Thu May 14 2009 Alan Bartlett <ajb@elrepo.org>
- Initial build of the x11-drv package.
