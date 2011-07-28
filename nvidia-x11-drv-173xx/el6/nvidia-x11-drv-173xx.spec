%define		nvidialibdir	%{_libdir}/nvidia
%define		nvidialib32dir	%{_prefix}/lib/nvidia

%define		debug_package	%{nil}

Name:		nvidia-x11-drv-173xx
Version:	173.14.28
Release:	1%{?dist}
Group:		User Interface/X Hardware Support
License:	Distributable
Summary:	NVIDIA 174.14.xx OpenGL X11 display driver files
URL:		http://www.nvidia.com/

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	i686 x86_64

# Sources.
# x86: pkg1 contains precompiled modules we don't need
Source0: ftp://download.nvidia.com/XFree86/Linux-x86/%{version}/NVIDIA-Linux-x86-%{version}-pkg0.run

# x86_64: only pkg2 contains the lib32 compatibility libs
Source1: ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}-pkg2.run

Source2:	nvidia-config-display
Source3:	blacklist-nouveau.conf
Source4:	nvidia.nodes

# provides desktop-file-install
BuildRequires:	desktop-file-utils
BuildRequires:	perl

Requires:	nvidia-173xx-kmod = %{?epoch:%{epoch}:}%{version}-%{release}
Requires(post):	nvidia-173xx-kmod = %{?epoch:%{epoch}:}%{version}-%{release}

Requires(post):	/sbin/ldconfig

# for nvidia-config-display
Requires(post):	 pyxf86config
Requires(preun): pyxf86config

Requires(post):	 grubby
Requires(preun): grubby

# elrepo
Conflicts:	nvidia-x11-drv
Conflicts:	nvidia-x11-drv-32bit
Conflicts:	nvidia-x11-drv-96xx
Conflicts:	nvidia-x11-drv-96xx-32bit

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
Requires(post):	/sbin/ldconfig

%description 32bit
Compatibility 32-bit files for the 64-bit Proprietary NVIDIA driver.

%prep
%setup -q -c -T

%ifarch i686
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


# Create the symlinks
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libcuda.so
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libcuda.so.1
%{__ln_s} libGLcore.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGLcore.so.1
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGL.so
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGL.so.1
%{__ln_s} libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-cfg.so
%{__ln_s} libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-cfg.so.1
%{__ln_s} libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-tls.so.1
%{__ln_s} libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/tls/libnvidia-tls.so.1
%{__ln_s} libXvMCNVIDIA.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libXvMCNVIDIA_dynamic.so
%{__ln_s} libXvMCNVIDIA.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libXvMCNVIDIA_dynamic.so.1
%{__ln_s} libglx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/libglx.so

%ifarch x86_64
# Create the 32-bit symlinks
%{__ln_s} libGLcore.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGLcore.so.1
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGL.so
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGL.so.1
%{__ln_s} libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-tls.so.1
%{__ln_s} libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/tls/libnvidia-tls.so.1
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
desktop-file-install \
    --dir $RPM_BUILD_ROOT%{_datadir}/applications/ \
    --add-category System \
    usr/share/applications/nvidia-settings.desktop

# Install X configuration script
%{__mkdir_p} $RPM_BUILD_ROOT%{_sbindir}/
%{__install} -p -m 0755 %{SOURCE2} $RPM_BUILD_ROOT%{_sbindir}/nvidia-config-display

# Blacklist the nouveau driver
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/modprobe.d/
%{__install} -p -m 0644 %{SOURCE3} $RPM_BUILD_ROOT%{_sysconfdir}/modprobe.d/blacklist-nouveau.conf

# Install udev configuration file
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/udev/makedev.d/
%{__install} -p -m 0644 %{SOURCE4} $RPM_BUILD_ROOT%{_sysconfdir}/udev/makedev.d/60-nvidia.nodes

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
    # Enable nvidia driver when installing
    %{_sbindir}/nvidia-config-display enable &>/dev/null
    # Disable the nouveau driver
    if [ -x /sbin/grubby ]; then
        GRUBBYLASTKERNEL=`/sbin/grubby --default-kernel`
        /sbin/grubby --update-kernel=${GRUBBYLASTKERNEL} --args='nouveau.modeset=0 rdblacklist=nouveau' &>/dev/null
    fi
fi || :

/sbin/ldconfig

%post 32bit
/sbin/ldconfig

%preun
if [ "$1" -eq "0" ]; then
    # Disable proprietary nvidia driver on uninstall
    [ -f %{_sbindir}/nvidia-config-display ] && %{_sbindir}/nvidia-config-display disable &>/dev/null
    # Clear grub option to disable nouveau for all RHEL6 kernels
    if [ -x /sbin/grubby ]; then
      KERNELS=`ls /boot/vmlinuz-2.6.32-*.el6.$(uname -m)`
      for kernel in ${KERNELS} ; do
      /sbin/grubby --update-kernel=${kernel} \
        --remove-args='nouveau.modeset=0 rdblacklist=nouveau nomodeset' &>/dev/null
      done
    fi
    # Backup and remove xorg.conf
    [ -f %{_sysconfdir}/X11/xorg.conf ] && \
      mv %{_sysconfdir}/X11/xorg.conf %{_sysconfdir}/X11/xorg.conf.uninstalled-nvidia &>/dev/null
fi ||:

%postun
/sbin/ldconfig

%postun 32bit
/sbin/ldconfig

%files
%defattr(-,root,root,-)
%doc nvidiapkg/usr/share/doc/*
%{_mandir}/man1/nvidia*.*
%{_datadir}/pixmaps/nvidia-settings.png
%{_datadir}/applications/*nvidia-settings.desktop
%{_bindir}/nvidia*
%{_sbindir}/nvidia-config-display
%config(noreplace) %{_sysconfdir}/modprobe.d/blacklist-nouveau.conf
%config %{_sysconfdir}/ld.so.conf.d/nvidia.conf
%config %{_sysconfdir}/udev/makedev.d/60-nvidia.nodes

# now the libs
%dir %{nvidialibdir}
%{nvidialibdir}/lib*
%dir %{nvidialibdir}/tls
%{nvidialibdir}/tls/lib*
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
* Fri Feb 04 2011 Philip J Perry <phil@elrepo.org> - 173.14.28-1.el6.elrepo
- Fork to el6.
- Update to version 173.14.28.
- Adds libcuda.so

* Sat Aug 21 2010 Philip J Perry <phil@elrepo.org> - 173.14.27-1.el5.elrepo
- Update to version 173.14.27.

* Fri Apr 16 2010 Philip J Perry <phil@elrepo.org> - 173.14.25-3.el5.elrepo
- Run ldconfig on 32bit subpackages [BugID: 0000058]

* Thu Feb 04 2010 Philip J Perry <phil@elrepo.org> - 173.14.25-2.el5.elrepo
- Split 32-bit compatibility files into a sub-package on x86_64.

* Wed Feb 03 2010 Philip J Perry <phil@elrepo.org> - 173.14.25-1.el5.elrepo
- Forked to create nvidia-x11-drv-173xx legacy release.
- Added Conflicts against other nvidia packages.
