# Define the Max Xorg version (ABI) that this driver release supports
# See README.txt, Chapter 2. Minimum Software Requirements or
# http://us.download.nvidia.com/XFree86/Linux-x86_64/390.132/README/minimumrequirements.html

%define		max_xorg_ver	1.20.99
%define		debug_package	%{nil}

Name:		nvidia-x11-drv-390xx
Version:	390.132
Release:	2%{?dist}
Group:		User Interface/X Hardware Support
License:	Distributable
Summary:	NVIDIA OpenGL X11 display driver files
URL:		http://www.nvidia.com/

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	i686 x86_64

# Sources.
Source0:	http://us.download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
NoSource: 0

Source1:	alternate-install-present
Source2:	nvidia-xorg.conf

# Provides for CUDA
Provides:	cuda-driver = %{version}
Provides:	cuda-drivers = %{version}
Provides:	nvidia-drivers = %{version}

# provides desktop-file-install
BuildRequires:	desktop-file-utils
BuildRequires:	perl

Requires:	perl
Requires:	xorg-x11-server-Xorg <= %{max_xorg_ver}
Requires:	%{name}-libs%{?_isa} = %{?epoch:%{epoch}:}%{version}-%{release}
Requires:	kmod-nvidia-390xx = %{?epoch:%{epoch}:}%{version}
Requires(post):	kmod-nvidia-390xx = %{?epoch:%{epoch}:}%{version}

Requires(post):	/sbin/ldconfig
Requires(post):	 dracut
Requires(post):	 grubby
Requires(preun): grubby

# elrepo
Conflicts:	nvidia-x11-drv
Conflicts:	nvidia-x11-drv-32bit
Conflicts:	nvidia-x11-drv-libs
Conflicts:	nvidia-x11-drv-367xx
Conflicts:	nvidia-x11-drv-367xx-32bit
Conflicts:	nvidia-x11-drv-340xx
Conflicts:	nvidia-x11-drv-340xx-32bit
Conflicts:	nvidia-x11-drv-304xx
Conflicts:	nvidia-x11-drv-304xx-32bit
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
Conflicts:	xorg-x11-drv-nvidia-304xx
Conflicts:	xorg-x11-drv-nvidia-340xx
Conflicts:	xorg-x11-drv-nvidia-367xx

%description
This package provides the proprietary NVIDIA OpenGL X11 display driver files.

%package libs
Summary:	Libraries for the Proprietary NVIDIA driver
Group:		User Interface/X Hardware Support
Requires:	%{name} = %{?epoch:%{epoch}:}%{version}-%{release}
Requires(post):	/sbin/ldconfig
Requires:	libvdpau%{?_isa} >= 1.0
Requires:	libglvnd%{?_isa} >= 1.0
Requires:	libglvnd-egl%{?_isa} >= 1.0
Requires:	libglvnd-gles%{?_isa} >= 1.0
Requires:	libglvnd-glx%{?_isa} >= 1.0
Requires:	libglvnd-opengl%{?_isa} >= 1.0
Requires:	egl-wayland%{?_isa}
Requires:	opencl-filesystem
Requires:	ocl-icd
Requires:	vulkan-loader

%description libs
This package provides libraries for the Proprietary NVIDIA driver.

%prep
%setup -q -c -T
sh %{SOURCE0} --extract-only --target nvidiapkg

# Lets just take care of all the docs here rather than during install
pushd nvidiapkg
%{__mv} LICENSE NVIDIA_Changelog pkg-history.txt README.txt html/
%{__mv} nvidia-persistenced-init.tar.bz2 html/
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
%{__install} -p -m 0755 nvidia-cuda-mps-control $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-cuda-mps-server $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-debugdump $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-modprobe $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-persistenced $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-settings $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-smi $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-xconfig $RPM_BUILD_ROOT%{_bindir}/

# Install OpenCL Vendor file
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/OpenCL/vendors/
%{__install} -p -m 0644 nvidia.icd $RPM_BUILD_ROOT%{_sysconfdir}/OpenCL/vendors/nvidia.icd
# Set lib in vulkan icd template
%{__perl} -pi -e 's|__NV_VK_ICD__|libGLX_nvidia.so.0|' nvidia_icd.json.template
# Install vulkan and EGL loaders
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/vulkan/icd.d/
%{__install} -p -m 0644 nvidia_icd.json.template $RPM_BUILD_ROOT%{_datadir}/vulkan/icd.d/nvidia_icd.json
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/glvnd/egl_vendor.d/
%{__install} -p -m 0644 10_nvidia.json $RPM_BUILD_ROOT%{_datadir}/glvnd/egl_vendor.d/10_nvidia.json

# Install GL, tls and vdpau libs
%ifarch i686
pushd 32
%endif
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/vdpau/
%{__install} -p -m 0755 libcuda.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libEGL_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libGLESv1_CM_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libGLESv2_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libGLX_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-compiler.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-eglcore.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-fatbinaryloader.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-glcore.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-glsi.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-gtk3.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-ptxjitcompiler.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 tls/*.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/
%ifarch i686
popd
%endif

# Install X driver and extension 
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/
%{__install} -p -m 0755 nvidia_drv.so $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__install} -p -m 0755 libglx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/

# Create the symlinks
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libcuda.so
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libcuda.so.1
%{__ln_s} libEGL_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libEGL_nvidia.so.0
%{__ln_s} libGLESv1_CM_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libGLESv1_CM_nvidia.so.1
%{__ln_s} libGLESv2_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libGLESv2_nvidia.so.2
%{__ln_s} libGLX_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libGLX_nvidia.so.0
%{__ln_s} libGLX_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libGLX_indirect.so.0
%{__ln_s} libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvcuvid.so
%{__ln_s} libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvcuvid.so.1
%ifarch x86_64
%{__ln_s} libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-cfg.so.1
%endif
%{__ln_s} libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-encode.so
%{__ln_s} libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-encode.so.1
%{__ln_s} libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-fbc.so
%{__ln_s} libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-fbc.so.1
%{__ln_s} libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ifr.so
%{__ln_s} libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ifr.so.1
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ml.so
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ml.so.1
%{__ln_s} libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-opencl.so.1
%{__ln_s} libnvidia-ptxjitcompiler.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ptxjitcompiler.so.1
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/libvdpau_nvidia.so.1
%ifarch x86_64
%{__ln_s} libglx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/libglx.so
%endif

# Install man pages
%{__mkdir_p} $RPM_BUILD_ROOT%{_mandir}/man1/
%{__install} -p -m 0644 nvidia-{cuda-mps-control,modprobe,persistenced,settings,smi,xconfig}.1.gz $RPM_BUILD_ROOT%{_mandir}/man1/

# Install pixmap for the desktop entry
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/pixmaps/
%{__install} -p -m 0644 nvidia-settings.png $RPM_BUILD_ROOT%{_datadir}/pixmaps/

# Desktop entry for nvidia-settings
# GNOME: System > Administration
# KDE: Applications > Administration
# Remove "__UTILS_PATH__/" before the Exec command name
# Replace "__PIXMAP_PATH__/" with the proper pixmaps path
%{__perl} -pi -e 's|(Exec=).*/(.*)|$1$2|g;
                  s|(Icon=).*/(.*)|$1%{_datadir}/pixmaps/$2|g' \
    nvidia-settings.desktop

# GNOME requires category=System on RHEL6
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/applications/
desktop-file-install \
    --dir $RPM_BUILD_ROOT%{_datadir}/applications/ \
    --add-category System \
    nvidia-settings.desktop

# Install application profiles
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/nvidia/
%{__install} -p -m 0644 nvidia-application-profiles-%{version}-rc $RPM_BUILD_ROOT%{_datadir}/nvidia/
%{__install} -p -m 0644 nvidia-application-profiles-%{version}-key-documentation $RPM_BUILD_ROOT%{_datadir}/nvidia/

#Install the output class config file. Requires xorg-x11-server-Xorg >= 1.16 and kernel >=3.9 with CONFIG_DRM enabled
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/X11/xorg.conf.d/
%{__install} -p -m 0644 nvidia-drm-outputclass.conf $RPM_BUILD_ROOT%{_datadir}/X11/xorg.conf.d/

# Install the Xorg conf files
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/X11/
%{__install} -p -m 0644 %{SOURCE2} $RPM_BUILD_ROOT%{_sysconfdir}/X11/nvidia-xorg.conf
# Install alternate-install-present file
# This file tells the NVIDIA installer that a packaged version of the driver is already present on the system
# The location is hardcoded in the NVIDIA.run installer as /user/lib/nvidia/
%{__mkdir_p} $RPM_BUILD_ROOT%{_prefix}/lib/nvidia/
%{__install} -p -m 0644 %{SOURCE1} $RPM_BUILD_ROOT%{_prefix}/lib/nvidia/alternate-install-present

popd

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%post
if [ "$1" -eq "1" ]; then # new install
    # Check if xorg.conf exists, if it does, backup and remove [BugID # 0000127]
    [ -f %{_sysconfdir}/X11/xorg.conf ] && \
      mv %{_sysconfdir}/X11/xorg.conf %{_sysconfdir}/X11/pre-nvidia.xorg.conf.elreposave &>/dev/null
    # xorg.conf now shouldn't exist so copy new one
    [ ! -f %{_sysconfdir}/X11/xorg.conf ] && \
      cp -p %{_sysconfdir}/X11/nvidia-xorg.conf %{_sysconfdir}/X11/xorg.conf &>/dev/null
    # Disable the nouveau driver
    [ -f %{_sysconfdir}/default/grub ] && \
      %{__perl} -pi -e 's|(GRUB_CMDLINE_LINUX=".*)"|$1 nouveau\.modeset=0 rd\.driver\.blacklist=nouveau plymouth\.ignore-udev"|g' \
        %{_sysconfdir}/default/grub
    if [ -x /usr/sbin/grubby ]; then
      # get installed kernels
      for KERNEL in $(rpm -q --qf '%{v}-%{r}.%{arch}\n' kernel); do
      VMLINUZ="/boot/vmlinuz-"$KERNEL
      # Check kABI compatibility
        for KABI in $(find /lib/modules -name nvidia.ko | cut -d / -f 4); do
          if [[ "$KERNEL" == "$KABI" && -e "$VMLINUZ" ]]; then
            /usr/bin/dracut --add-drivers nvidia -f /boot/initramfs-$KERNEL.img $KERNEL
            /usr/sbin/grubby --update-kernel="$VMLINUZ" \
              --args='nouveau.modeset=0 rd.driver.blacklist=nouveau plymouth.ignore-udev' &>/dev/null
          fi
        done
      done
    fi
fi || :

/sbin/ldconfig

%post libs
/sbin/ldconfig

%preun
if [ "$1" -eq "0" ]; then # uninstall
    # Backup and remove xorg.conf
    [ -f %{_sysconfdir}/X11/xorg.conf ] && \
      mv %{_sysconfdir}/X11/xorg.conf %{_sysconfdir}/X11/post-nvidia.xorg.conf.elreposave &>/dev/null
    # Clear grub option to disable nouveau for all RHEL kernels
    if [ -f %{_sysconfdir}/default/grub ]; then
      %{__perl} -pi -e 's|(GRUB_CMDLINE_LINUX=.*) nouveau\.modeset=0|$1|g' %{_sysconfdir}/default/grub
      %{__perl} -pi -e 's|(GRUB_CMDLINE_LINUX=.*) rd\.driver\.blacklist=nouveau|$1|g' %{_sysconfdir}/default/grub
      %{__perl} -pi -e 's|(GRUB_CMDLINE_LINUX=.*) plymouth\.ignore-udev|$1|g' %{_sysconfdir}/default/grub
    fi
    if [ -x /usr/sbin/grubby ]; then
      # get installed kernels
      for KERNEL in $(rpm -q --qf '%{v}-%{r}.%{arch}\n' kernel); do
        VMLINUZ="/boot/vmlinuz-"$KERNEL
        if [[ -e "$VMLINUZ" ]]; then
          /usr/sbin/grubby --update-kernel="$VMLINUZ" \
            --remove-args='nouveau.modeset=0 rd.driver.blacklist=nouveau plymouth.ignore-udev' &>/dev/null
        fi
      done
    fi
fi ||:

%postun
/sbin/ldconfig

%postun libs
/sbin/ldconfig

%files
%defattr(-,root,root,-)
%doc nvidiapkg/html/*
%{_mandir}/man1/nvidia*.*
%{_datadir}/pixmaps/nvidia-settings.png
%{_datadir}/applications/*nvidia-settings.desktop
%{_datadir}/glvnd/egl_vendor.d/10_nvidia.json
%{_datadir}/vulkan/icd.d/nvidia_icd.json
%dir %{_datadir}/nvidia
%{_datadir}/nvidia/nvidia-application-profiles-*
%{_datadir}/X11/xorg.conf.d/nvidia-drm-outputclass.conf
%{_bindir}/nvidia-bug-report.sh
%{_bindir}/nvidia-cuda-mps-control
%{_bindir}/nvidia-cuda-mps-server
%{_bindir}/nvidia-debugdump
%attr(4755, root, root) %{_bindir}/nvidia-modprobe
%{_bindir}/nvidia-persistenced
%{_bindir}/nvidia-settings
%{_bindir}/nvidia-smi
%{_bindir}/nvidia-xconfig
%config %{_sysconfdir}/X11/nvidia-xorg.conf
%{_sysconfdir}/OpenCL/vendors/nvidia.icd
%dir %{_prefix}/lib/nvidia/
%{_prefix}/lib/nvidia/alternate-install*
%{_libdir}/xorg/modules/drivers/nvidia_drv.so
%dir %{_libdir}/xorg/modules/extensions/nvidia
%{_libdir}/xorg/modules/extensions/nvidia/libglx.*

%files libs
%defattr(-,root,root,-)
%{_libdir}/lib*
%{_libdir}/vdpau/libvdpau_nvidia.*

%changelog
* Sat May 02 2020 Philip J Perry <phil@elrepo.org> - 390.132-2
- Rebuilt for RHEL 8.2

* Mon Mar 30 2020 Philip J Perry <phil@elrepo.org> - 390.132-1
- Ported to RHEL 8.1

* Sat Feb 15 2020 Philip J Perry <phil@elrepo.org> - 390.116-2
- Rebuilt to match kmod package for RHEL 7.7

* Sun Feb 24 2019 Philip J Perry <phil@elrepo.org> - 390.116-1
- Updated to version 390.116

* Sat Jan 12 2019 Philip J Perry <phil@elrepo.org> - 390.87-2
- Fix compatibility with GLVND for RHEL7.6
  [https://elrepo.org/bugs/view.php?id=889]

* Fri Sep 07 2018 Philip J Perry <phil@elrepo.org> - 390.87-1
- Updated to version 390.87

* Sat Jul 28 2018 Philip J Perry <phil@elrepo.org> - 390.77-1
- Fork to legacy release nvidia-390xx

* Tue Jul 17 2018 Philip J Perry <phil@elrepo.org> - 390.77-1
- Updated to version 390.77

* Wed Jun 06 2018 Philip J Perry <phil@elrepo.org> - 390.67-1
- Updated to version 390.67

* Fri May 18 2018 Philip J Perry <phil@elrepo.org> - 390.59-1
- Updated to version 390.59
- Adds support for Xorg 1.20 (Video Driver ABI 24)

* Fri Mar 30 2018 Philip J Perry <phil@elrepo.org> - 390.48-1
- Updated to version 390.48

* Fri Mar 16 2018 Philip J Perry <phil@elrepo.org> - 390.42-1
- Updated to version 390.42

* Tue Jan 30 2018 Philip J Perry <phil@elrepo.org> - 390.25-1
- Updated to version 390.25
