%define		nvidialibdir	%{_libdir}/nvidia
%define		nvidialib32dir	%{_prefix}/lib/nvidia

%define		debug_package	%{nil}

Name:		nvidia-x11-drv-304xx
Version:	304.84
Release:	1%{?dist}
Group:		User Interface/X Hardware Support
License:	Distributable
Summary:	NVIDIA 304xx OpenGL X11 display driver files
URL:		http://www.nvidia.com/

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	i386 x86_64

# Sources.
Source0:  ftp://download.nvidia.com/XFree86/Linux-x86/%{version}/NVIDIA-Linux-x86-%{version}.run
Source1:  ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
# Source0: http://us.download.nvidia.com/XFree86/Linux-x86/%{version}/NVIDIA-Linux-x86-%{version}.run
# Source1: http://us.download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run

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

Requires:	nvidia-304xx-kmod = %{?epoch:%{epoch}:}%{version}
Requires(post):	nvidia-304xx-kmod = %{?epoch:%{epoch}:}%{version}

Requires(post):	/sbin/ldconfig

# for nvidia-config-display
Requires(post):	 pyxf86config
Requires(preun): pyxf86config

# elrepo
Conflicts:	nvidia-x11-drv
Conflicts:	nvidia-x11-drv-32bit
Conflicts:	nvidia-x11-drv-173xx
Conflicts:	nvidia-x11-drv-173xx-32bit
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
This package provides the proprietary NVIDIA 304xx OpenGL X11 display driver files.

%package 32bit
Summary:	Compatibility 32-bit files for the 64-bit Proprietary NVIDIA driver
Group:		User Interface/X Hardware Support
Requires:	%{name} = %{version}-%{release}
Requires(post):	/sbin/ldconfig

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
pushd nvidiapkg
%{__mv} LICENSE NVIDIA_Changelog pkg-history.txt README.txt html/
popd
find nvidiapkg/html/ -type f | xargs chmod 0644

%build
# Nothing to build

%install
%{__rm} -rf $RPM_BUILD_ROOT

pushd nvidiapkg

# Install nvidia tools
%{__mkdir_p} $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-bug-report.sh $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-cuda-proxy-control $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-cuda-proxy-server $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-debugdump $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-settings $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-smi $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-xconfig $RPM_BUILD_ROOT%{_bindir}/

# Install OpenCL Vendor file
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/OpenCL/vendors/
%{__install} -p -m 0644 nvidia.icd $RPM_BUILD_ROOT%{_sysconfdir}/OpenCL/vendors/nvidia.icd

# Install GL, tls and vdpau libs
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/vdpau/
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialibdir}/tls/
%{__install} -p -m 0755 libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/
%{__install} -p -m 0755 libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGL.la $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvcuvid.so in 260.xx series driver
%{__install} -p -m 0755 libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-compiler.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-glcore.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-opencl.so in 304.xx series driver
%{__install} -p -m 0755 libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libOpenCL.so.* $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0644 libXvMCNVIDIA.a $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libXvMCNVIDIA.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 tls/*.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/tls/

%ifarch x86_64
# Install 32bit compat GL, tls and vdpau libs
%{__mkdir_p} $RPM_BUILD_ROOT%{_prefix}/lib/vdpau/
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialib32dir}/tls/
%{__install} -p -m 0755 32/libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_prefix}/lib/vdpau/
%{__install} -p -m 0755 32/libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGL.la $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-compiler.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-glcore.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libOpenCL.so.* $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/tls/*.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/tls/
%endif

# Install X driver and extension 
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/
%{__install} -p -m 0755 nvidia_drv.so $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__install} -p -m 0755 libglx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/
%{__install} -p -m 0755 libnvidia-wfb.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/

# Create the symlinks
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libcuda.so
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libcuda.so.1
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGL.so
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGL.so.1
# Added libnvcuvid.so in 260.xx series driver
%{__ln_s} libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvcuvid.so
%{__ln_s} libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvcuvid.so.1
%{__ln_s} libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-cfg.so
%{__ln_s} libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-cfg.so.1
# Added libnvidia-ml.so in 270.xx series driver
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-ml.so
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-ml.so.1
# Added libnvidia-opencl.so in 304.xx series driver
%{__ln_s} libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-opencl.so.1
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialibdir}/libOpenCL.so
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialibdir}/libOpenCL.so.1
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialibdir}/libOpenCL.so.1.0
%{__ln_s} libXvMCNVIDIA.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libXvMCNVIDIA_dynamic.so
%{__ln_s} libXvMCNVIDIA.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libXvMCNVIDIA_dynamic.so.1
%{__ln_s} libglx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/libglx.so
%{__ln_s} libnvidia-wfb.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/libwfb.so
%{__ln_s} libnvidia-wfb.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/libnvidia-wfb.so.1
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/libvdpau_nvidia.so
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/libvdpau_nvidia.so.1

%ifarch x86_64
# Create the 32-bit symlinks
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libcuda.so
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libcuda.so.1
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGL.so
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGL.so.1
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-ml.so
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-ml.so.1
%{__ln_s} libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-opencl.so.1
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libOpenCL.so
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libOpenCL.so.1
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libOpenCL.so.1.0
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_prefix}/lib/vdpau/libvdpau_nvidia.so
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_prefix}/lib/vdpau/libvdpau_nvidia.so.1
%endif

# Install man pages
%{__mkdir_p} $RPM_BUILD_ROOT%{_mandir}/man1/
%{__install} -p -m 0644 nvidia-{cuda-proxy-control,settings,smi,xconfig}.1.gz $RPM_BUILD_ROOT%{_mandir}/man1/

# Install pixmap for the desktop entry
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/pixmaps/
%{__install} -p -m 0644 nvidia-settings.png $RPM_BUILD_ROOT%{_datadir}/pixmaps/

# Desktop entry for nvidia-settings
# Remove "__UTILS_PATH__/" before the Exec command name
# Replace "__PIXMAP_PATH__/" with the proper pixmaps path
%{__perl} -pi -e 's|(Exec=).*/(.*)|$1$2|g;
                  s|(Icon=).*/(.*)|$1%{_datadir}/pixmaps/$2|g' \
    nvidia-settings.desktop

%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/applications/
desktop-file-install --vendor elrepo \
    --dir $RPM_BUILD_ROOT%{_datadir}/applications/ \
    --add-category System \
    --add-category Application \
    --add-category GNOME \
    nvidia-settings.desktop

# Install profile.d files
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/profile.d/
%{__install} -p -m 0644 %{SOURCE2} $RPM_BUILD_ROOT%{_sysconfdir}/profile.d/nvidia.sh
%{__install} -p -m 0644 %{SOURCE3} $RPM_BUILD_ROOT%{_sysconfdir}/profile.d/nvidia.csh

# Install X configuration script
%{__mkdir_p} $RPM_BUILD_ROOT%{_sbindir}/
%{__install} -p -m 0755 %{SOURCE4} $RPM_BUILD_ROOT%{_sbindir}/nvidia-config-display

# Install modprobe.d file
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/modprobe.d/
%{__install} -p -m 0644 %{SOURCE5} $RPM_BUILD_ROOT%{_sysconfdir}/modprobe.d/nvidia

# Install udev configuration file
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/udev/makedev.d/
%{__install} -p -m 0644 %{SOURCE6} $RPM_BUILD_ROOT%{_sysconfdir}/udev/makedev.d/60-nvidia.nodes

# Install ld.so.conf.d file
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/
echo %{nvidialibdir} > $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nvidia.conf
echo %{_libdir}/vdpau >> $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nvidia.conf
%ifarch x86_64
echo %{nvidialib32dir} >> $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nvidia.conf
echo %{_prefix}/lib/vdpau >> $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nvidia.conf
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

%post 32bit
/sbin/ldconfig

%preun
# Disable proprietary nvidia driver on uninstall
if [ "$1" -eq "0" ]; then
    test -f %{_sbindir}/nvidia-config-display && %{_sbindir}/nvidia-config-display disable &>/dev/null || :
fi

%postun
/sbin/ldconfig

%postun 32bit
/sbin/ldconfig

%triggerin -- xorg-x11-server-Xorg
# Enable the proprietary nvidia driver
# Required since xorg-x11-server-Xorg empties the "Files" section
test -f %{_sbindir}/nvidia-config-display && %{_sbindir}/nvidia-config-display enable &>/dev/null || :

%files
%defattr(-,root,root,-)
%doc nvidiapkg/html/*
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
%{_sysconfdir}/OpenCL/vendors/nvidia.icd

# now the libs
%dir %{nvidialibdir}
%{nvidialibdir}/lib*
%dir %{nvidialibdir}/tls
%{nvidialibdir}/tls/lib*
%{_libdir}/vdpau/libvdpau_nvidia.*
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
%{_prefix}/lib/vdpau/libvdpau_nvidia.*
%endif

%changelog
* Sat Mar 09 2013 Philip J Perry <phil@elrepo.org> - 304.84-1.el5.elrepo
- Updated to version 304.84

* Sun Dec 30 2012 Philip J Perry <phil@elrepo.org> - 304.64-1.el5.elrepo
- Fork to legacy release nvidia-x11-drv-304xx

* Sat Nov 10 2012 Philip J Perry <phil@elrepo.org> - 304.64-1.el5.elrepo
- Updated to version 304.64
- Install missing 32-bit libs

* Fri Oct 19 2012 Philip J Perry <phil@elrepo.org> - 304.60-1.el5.elrepo
- Updated to version 304.60

* Fri Sep 28 2012 Philip J Perry <phil@elrepo.org> - 304.51-1.el5.elrepo
- Updated to version 304.51
- Add missing lib and symlink for OpenCL [http://elrepo.org/bugs/view.php?id=304]

* Tue Aug 28 2012 Philip J Perry <phil@elrepo.org> - 304.43-1.el5.elrepo
- Updated to version 304.43

* Tue Aug 14 2012 Philip J Perry <phil@elrepo.org> - 304.37-1.el5.elrepo
- Updated to version 304.37
- Add nvidia-cuda-proxy-control, nvidia-cuda-proxy-server and associated manpage

* Wed Aug 08 2012 Philip J Perry <phil@elrepo.org> - 295.71-1.el5.elrepo
- Updated to version 295.71
- Fixes http://permalink.gmane.org/gmane.comp.security.full-disclosure/86747

* Tue Jun 19 2012 Philip J Perry <phil@elrepo.org> - 302.17-1.el5.elrepo
- Updated to version 302.17

* Sat Jun 16 2012 Philip J Perry <phil@elrepo.org> - 295.59-1.el5.elrepo
- Updated to version 295.59

* Thu May 17 2012 Philip J Perry <phil@elrepo.org> - 295.53-1.el5.elrepo
- Updated to version 295.53

* Fri May 04 2012 Philip J Perry <phil@elrepo.org> - 295.49-1.el5.elrepo
- Updated to version 295.49

* Wed Apr 11 2012 Philip J Perry <phil@elrepo.org> - 295.40-1.el5.elrepo
- Updated to version 295.40
- Fixes CVE-2012-0946

* Fri Mar 23 2012 Philip J Perry <phil@elrepo.org> - 295.33-1.el5.elrepo
- Updated to version 295.33

* Mon Feb 13 2012 Philip J Perry <phil@elrepo.org> - 295.20-1.el5.elrepo
- Updated to version 295.20
- Fix permissions on device file(s)

* Wed Nov 23 2011 Philip J Perry <phil@elrepo.org> - 290.10-1.el5.elrepo
- Updated to version 290.10

* Fri Oct 07 2011 Philip J Perry <phil@elrepo.org> - 285.05.09-1.el5.elrepo
- Updated to version 285.05.09
- Adds nvidia-debugdump

* Tue Aug 02 2011 Philip J Perry <phil@elrepo.org> - 280.13-1.el5.elrepo
- Updated to version 280.13

* Fri Jul 22 2011 Philip J Perry <phil@elrepo.org> - 275.21-1.el5.elrepo
- Updated to version 275.21

* Fri Jul 15 2011 Philip J Perry <phil@elrepo.org> - 275.19-1.el5.elrepo
- Updated to version 275.19

* Fri Jun 17 2011 Philip J Perry <phil@elrepo.org> - 275.09.07-1.el5.elrepo
- Updated to version 275.09.07

* Sat Apr 16 2011 Philip J Perry <phil@elrepo.org> - 270.41.03-1.el5.elrepo
- Updated to version 270.41.03 for release

* Fri Mar 25 2011 Philip J Perry <phil@elrepo.org>
- Updated to version 270.30 beta
- Adds libnvidia-ml library.
- Remove vdpau wrapper libs from package (libvdpau from EPEL provides these).
  Move vendor-specific libvdpau_nvidia.so libs to /usr/lib/vdpau.
  Update ldconf path to include /usr/lib/vdpau.
  [http://elrepo.org/bugs/view.php?id=123]

* Fri Jan 21 2011 Philip J Perry <phil@elrepo.org> - 260.19.36-1.el5.elrepo
- Updated to version 260.19.36
- Test for xorg.conf and create one if missing.

* Thu Dec 16 2010 Philip J Perry <phil@elrepo.org> - 260.19.29-1.el5.elrepo
- Updated to version 260.19.29

* Wed Nov 10 2010 Philip J Perry <phil@elrepo.org> - 260.19.21-1.el5.elrepo
- Updated to version 260.19.21

* Fri Oct 15 2010 Philip J Perry <phil@elrepo.org> - 260.19.12-1.el5.elrepo
- Updated to version 260.19.12

* Fri Sep 10 2010 Philip J Perry <phil@elrepo.org> - 260.19.04-1.el5.elrepo
- Updated to version 260.19.04, internal beta build.
- Adds libnvcuvid library.

* Tue Aug 31 2010 Philip J Perry <phil@elrepo.org> - 256.53-1.el5.elrepo
- Updated to version 256.53

* Sat Jul 30 2010 Philip J Perry <phil@elrepo.org> - 256.44-1.el5.elrepo
- Updated to version 256.44

* Sat Jun 19 2010 Philip J Perry <phil@elrepo.org> - 256.35-1.el5.elrepo
- Updated to version 256.35

* Sat Jun 12 2010 Philip J Perry <phil@elrepo.org> - 195.36.31-1.el5.elrepo
- Updated to version 195.36.31.

* Fri Apr 23 2010 Philip J Perry <phil@elrepo.org> - 195.36.24-1.el5.elrepo
- Updated to version 195.36.24.
- Run ldconfig on 32bit subpackages
  [http://elrepo.org/bugs/view.php?id=58]

* Sat Mar 20 2010 Philip J Perry <phil@elrepo.org> - 195.36.15-1.el5.elrepo
- Updated to version 195.36.15.
- Added OpenCL libraries.
- Added libnvidia-compiler runtime libs.

* Sun Feb 21 2010 Philip J Perry <phil@elrepo.org> - 190.53-1.el5.elrepo
- Updated to version 190.53.
- Fixed vdpau symlinks.
- Split 32-bit compatibility files into a sub-package on x86_64.

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

* Wed Jun 10 2009 Phil Perry <phil@elrepo.org>
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
