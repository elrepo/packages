# Define the Max Xorg version (ABI) that this driver release supports
# See README.txt, Chapter 2. Minimum Software Requirements or
# http://us.download.nvidia.com/XFree86/Linux-x86_64/450.66/README/minimumrequirements.html

%define		max_xorg_ver	1.20.99

%define		debug_package	%{nil}
%define		_use_internal_dependency_generator	0

Name:		nvidia-x11-drv
Version:	450.66
Release:	1%{?dist}
Group:		User Interface/X Hardware Support
License:	Distributable
Summary:	NVIDIA OpenGL X11 display driver files
URL:		http://www.nvidia.com/

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	i686 x86_64

# Sources.
Source0:	http://us.download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
NoSource: 0

Source1:    nvidia-provides.sh
Source2:	nvidia-xorg.conf
Source3:	alternate-install-present

# Define for nvidia-provides
%define __find_provides %{SOURCE1}

# Provides for CUDA
Provides:	cuda-driver = %{version}
Provides:	cuda-drivers = %{version}
Provides:	nvidia-drivers = %{version}

# provides desktop-file-install
BuildRequires:	desktop-file-utils
BuildRequires:	perl

Requires:	perl
Requires:	vulkan-filesystem
Requires:	xorg-x11-server-Xorg <= %{max_xorg_ver}
Requires:	yum-plugin-nvidia >= 1.0.2

Requires:	%{name}-libs%{?_isa} = %{?epoch:%{epoch}:}%{version}-%{release}
Requires:	nvidia-kmod = %{?epoch:%{epoch}:}%{version}
Requires(post):	nvidia-kmod = %{?epoch:%{epoch}:}%{version}

Requires(post):	/sbin/ldconfig
Requires(post):	 dracut
Requires(post):	 grubby
Requires(preun): grubby

# epel
# libnvidia-egl-wayland conflicts with
Conflicts:	egl-wayland
Conflicts:	egl-wayland-devel
# libOpenCL conflists with ocl-icd
Conflicts:	opencl-filesystem
Conflicts:	ocl-icd
Conflicts:	ocl-icd-devel

# elrepo
Conflicts:	nvidia-x11-drv-390xx
Conflicts:	nvidia-x11-drv-390xx-32bit
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

# negativo17.org
Conflicts:	nvidia-kmod-common

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
Conflicts:	xorg-x11-drv-nvidia-390xx

%description
This package provides the proprietary NVIDIA OpenGL X11 display driver files.

%package libs
Summary:	Libraries for the Proprietary NVIDIA driver
Group:		User Interface/X Hardware Support
## Remove requires for nvidia-x11-drv to allow installation of
## nvidia-x11-drv-libs on headless systems. See bug 
## https://elrepo.org/bugs/view.php?id=926
## Requires:	%{name} = %{?epoch:%{epoch}:}%{version}-%{release}
Requires:	xorg-x11-server-Xorg <= %{max_xorg_ver}
Requires(post):	/sbin/ldconfig
Obsoletes:	nvidia-x11-drv-32bit < %{?epoch:%{epoch}:}%{version}-%{release}
Requires:	libvdpau%{?_isa} >= 1.0
Requires:	libglvnd%{?_isa} >= 1.0
Requires:	libglvnd-egl%{?_isa} >= 1.0
Requires:	libglvnd-gles%{?_isa} >= 1.0
Requires:	libglvnd-glx%{?_isa} >= 1.0
Requires:	libglvnd-opengl%{?_isa} >= 1.0

%description libs
This package provides libraries for the Proprietary NVIDIA driver.

%prep
%setup -q -c -T
sh %{SOURCE0} --extract-only --target nvidiapkg

# Lets just take care of all the docs here rather than during install
pushd nvidiapkg
%{__mkdir_p} html/samples/systemd/
%{__mv} LICENSE NVIDIA_Changelog pkg-history.txt README.txt supported-gpus.json html/
%{__mv} nvidia-persistenced-init.tar.bz2 html/samples/
%{__mv} nvidia nvidia-sleep.sh nvidia-*.service html/samples/systemd/
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
# Install vulkan and EGL loaders
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/vulkan/icd.d/
%{__install} -p -m 0644 nvidia_icd.json $RPM_BUILD_ROOT%{_datadir}/vulkan/icd.d/nvidia_icd.json
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/vulkan/implicit_layer.d/
%{__install} -p -m 0644 nvidia_layers.json $RPM_BUILD_ROOT%{_datadir}/vulkan/implicit_layer.d/nvidia_layers.json
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/glvnd/egl_vendor.d/
%{__install} -p -m 0644 10_nvidia.json $RPM_BUILD_ROOT%{_datadir}/glvnd/egl_vendor.d/10_nvidia.json
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/egl/egl_external_platform.d/
%{__install} -p -m 0644 10_nvidia_wayland.json $RPM_BUILD_ROOT%{_datadir}/egl/egl_external_platform.d/10_nvidia_wayland.json

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
%{__install} -p -m 0755 libnvidia-allocator.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-cbl.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-compiler.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-eglcore.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-egl-wayland.so.1.1.4 $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-glcore.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-glsi.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-glvkspirv.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-gtk3.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-ngx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-opticalflow.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-ptxjitcompiler.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-rtcore.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvoptix.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/
%ifarch i686
popd
%endif

# Install X driver and extension 
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/
%{__install} -p -m 0755 nvidia_drv.so $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__install} -p -m 0755 libglxserver_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/

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
%{__ln_s} libnvidia-allocator.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-allocator.so
%{__ln_s} libnvidia-allocator.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-allocator.so.1
%ifarch x86_64
%{__ln_s} libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-cfg.so.1
%{__ln_s} libnvidia-egl-wayland.so.1.1.4 $RPM_BUILD_ROOT%{_libdir}/libnvidia-egl-wayland.so
%{__ln_s} libnvidia-egl-wayland.so.1.1.4 $RPM_BUILD_ROOT%{_libdir}/libnvidia-egl-wayland.so.1
%endif
%{__ln_s} libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-encode.so
%{__ln_s} libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-encode.so.1
%{__ln_s} libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-fbc.so
%{__ln_s} libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-fbc.so.1
%{__ln_s} libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ifr.so
%{__ln_s} libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ifr.so.1
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ml.so
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ml.so.1
%ifarch x86_64
%{__ln_s} libnvidia-ngx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ngx.so
%{__ln_s} libnvidia-ngx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ngx.so.1
%endif
%{__ln_s} libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-opencl.so.1
%{__ln_s} libnvidia-opticalflow.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-opticalflow.so
%{__ln_s} libnvidia-opticalflow.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-opticalflow.so.1
%{__ln_s} libnvidia-ptxjitcompiler.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ptxjitcompiler.so.1
%ifarch x86_64
%{__ln_s} libnvoptix.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvoptix.so.1
%endif
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{_libdir}/libOpenCL.so
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{_libdir}/libOpenCL.so.1
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/libvdpau_nvidia.so.1
%ifarch x86_64
%{__ln_s} libglxserver_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/libglxserver_nvidia.so
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
# added in 319.17
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/nvidia/
%{__install} -p -m 0644 nvidia-application-profiles-%{version}-rc $RPM_BUILD_ROOT%{_datadir}/nvidia/
# added in 340.24
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
%{__install} -p -m 0644 %{SOURCE3} $RPM_BUILD_ROOT%{_prefix}/lib/nvidia/alternate-install-present

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
    # Clear grub option to disable nouveau for all RHEL7 kernels
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
%{_datadir}/egl/egl_external_platform.d/10_nvidia_wayland.json
%{_datadir}/glvnd/egl_vendor.d/10_nvidia.json
%{_datadir}/vulkan/icd.d/nvidia_icd.json
%{_datadir}/vulkan/implicit_layer.d/nvidia_layers.json
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
%{_sysconfdir}/OpenCL/
%{_sysconfdir}/OpenCL/vendors/nvidia.icd
%dir %{_prefix}/lib/nvidia/
%{_prefix}/lib/nvidia/alternate-install*
%{_libdir}/xorg/modules/drivers/nvidia_drv.so
%{_libdir}/xorg/modules/extensions/libglxserver_nvidia.*

%files libs
%defattr(-,root,root,-)
%{_libdir}/lib*
%{_libdir}/vdpau/libvdpau_nvidia.*

%changelog
* Wed Aug 19 2020 Philip J Perry <phil@elrepo.org> - 450.66-1
- Updated to version 450.66
- Add conflicts for nvidia-kmod-common

* Fri Jul 10 2020 Philip J Perry <phil@elrepo.org> - 450.57-1
- Updated to version 450.57

* Thu Jun 25 2020 Philip J Perry <phil@elrepo.org> - 440.100-1
- Updated to version 440.100

* Wed Apr 08 2020 Philip J Perry <phil@elrepo.org> - 440.82-1
- Updated to version 440.82

* Sun Mar 01 2020 Philip J Perry <phil@elrepo.org> - 440.64-1
- Updated to version 440.64

* Sat Feb 08 2020 Philip J Perry <phil@elrepo.org> - 440.59-1
- Updated to version 440.59

* Sat Dec 14 2019 Philip J Perry <phil@elrepo.org> - 440.44-1
- Updated to version 440.44

* Sat Nov 23 2019 Philip J Perry <phil@elrepo.org> - 440.36-1
- Updated to version 440.36

* Wed Nov 06 2019 Philip J Perry <phil@elrepo.org> - 440.31-1
- Updated to version 440.31

* Thu Sep 12 2019 Philip J Perry <phil@elrepo.org> - 430.50-1
- Updated to version 430.50
- Remove requires for nvidia-x11-drv from -libs 
  [https://elrepo.org/bugs/view.php?id=926]

* Tue Jul 30 2019 Philip J Perry <phil@elrepo.org> - 430.40-1
- Updated to version 430.40

* Wed Jul 10 2019 Philip J Perry <phil@elrepo.org> - 430.34-1
- Updated to version 430.34

* Tue Jun 11 2019 Philip J Perry <phil@elrepo.org> - 430.26-1
- Updated to version 430.26

* Tue May 14 2019 Philip J Perry <phil@elrepo.org> - 430.14-1
- Updated to version 430.14
- Install systemd samples

* Tue May 07 2019 Philip J Perry <phil@elrepo.org> - 418.74-1
- Updated to version 418.74

* Thu Mar 21 2019 Philip J Perry <phil@elrepo.org> - 418.56-1
- Updated to version 418.56

* Sat Mar 02 2019 Philip J Perry <phil@elrepo.org> - 418.43-1
- Updated to version 418.43

* Sat Jan 05 2019 Philip J Perry <phil@elrepo.org> - 410.93-1
- Updated to version 410.93

* Sat Dec 08 2018 Philip J Perry <phil@elrepo.org> - 410.78-3
- Add conflicts for opencl-filesystem
- Clean up installation of libs and symlinks
- Remove OpenCL.so.1.0 symlink
- Remove requires for mesa runtime libs packages
- Remove profile scripts to set __GLX_VENDOR_LIBRARY_NAME

* Sat Dec 01 2018 Philip J Perry <phil@elrepo.org> - 410.78-2
- Updated to version 410.78

* Sat Dec 01 2018 Philip J Perry <phil@elrepo.org> - 410.73-3
- Make arch-specific nvidia-x11-drv-libs packages

* Thu Oct 25 2018 Philip J Perry <phil@elrepo.org> - 410.73-2
- Use RHEL7 glvnd package
- Use %%{_libdir} for libs now conflicts are resolved
- Add wayland components, now supported on RHEL as a tech preview

* Thu Oct 25 2018 Philip J Perry <phil@elrepo.org> - 410.73-1
- Updated to version 410.73

* Tue Oct 16 2018 Philip J Perry <phil@elrepo.org> - 410.66-1
- Updated to version 410.66

* Sat Oct 06 2018 Philip J Perry <phil@elrepo.org> - 410.57-2
- Install libglxserver_nvidia.so to default location
- Remove libnvidia-gtk2.so on RHEL7

* Sat Sep 22 2018 Philip J Perry <phil@elrepo.org> - 410.57-1
- Updated to version 410.57 beta driver

* Mon Sep 17 2018 Philip J Perry <phil@elrepo.org> - 396.54-1
- Updated to version 396.54

* Mon Aug 27 2018 Philip J Perry <phil@elrepo.org> - 390.87-1
- Updated to version 390.87

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

* Fri Jan 05 2018 Philip J Perry <phil@elrepo.org> - 384.111-1
- Updated to version 384.111

* Tue Nov 07 2017 Philip J Perry <phil@elrepo.org> - 384.98-2
- Add CUDA provides for nvidia-drivers

* Fri Nov 03 2017 Philip J Perry <phil@elrepo.org> - 384.98-1
- Updated to version 384.98

* Sat Sep 23 2017 Philip J Perry <phil@elrepo.org> - 384.90-1
- Updated to version 384.90
- Add 32-bit libnvidia-ptxjitcompiler.so.1 symlink

* Sun Sep 10 2017 Philip J Perry <phil@elrepo.org> - 384.69-2
- Add missing symlink for libnvidia-ptxjitcompiler.so.1
  [http://elrepo.org/bugs/view.php?id=765]
- Install profile.d scripts to set GLX vendor name, revised fix for
  [http://elrepo.org/bugs/view.php?id=714]

* Sat Sep 02 2017 Akemi Yagi <toracat@elrepo.org> - 384.69-1
- Updated to version 384.69

* Thu Aug 31 2017 Akemi Yagi <toracat@elrepo.org> - 384.66-1
- Updated to version 384.66

* Fri Aug 18 2017 Akemi Yagi <toracat@elrepo.org> - 384.59-2
- Set vulkan icd file name (http://elrepo.org/bugs/view.php?id=770)

* Tue Jul 25 2017 Philip J Perry <phil@elrepo.org> - 384.59-1
- Updated to version 384.59
- Reinstate support for GRID K520
- Fix bug http://elrepo.org/bugs/view.php?id=714
- Add conflicts for legacy 367xx packages
- Remove obsolete checks for glamoregl
- Remove obsolete broken SONAME fix

* Wed May 10 2017 Philip J Perry <phil@elrepo.org> - 375.66-1
- Updated to version 375.66

* Wed Feb 22 2017 Philip J Perry <phil@elrepo.org> - 375.39-1
- Updated to version 375.39
- Use plymouth.ignore-udev to allow text mode booting [David Bell]

* Thu Dec 15 2016 Philip J Perry <phil@elrepo.org> - 375.26-1
- Updated to version 375.26

* Sat Nov 19 2016 Philip J Perry <phil@elrepo.org> - 375.20-1
- Updated to version 375.20
- Adds support for Xorg 1.19 (Video Driver ABI 23)
- Enable GLVND
- Install nvidia-persistenced

* Tue Oct 11 2016 Philip J Perry <phil@elrepo.org> - 367.57-1
- Updated to version 367.57

* Sat Aug 27 2016 Philip J Perry <phil@elrepo.org> - 367.44-1
- Updated to version 367.44

* Sat Jul 16 2016 Philip J Perry <phil@elrepo.org> - 367.35-1
- Updated to version 367.35

* Tue Jun 14 2016 Philip J Perry <phil@elrepo.org> - 367.27-1
- Updated to version 367.27

* Wed May 25 2016 Philip J Perry <phil@elrepo.org> - 361.45.11-1
- Updated to version 361.45.11

* Thu Mar 31 2016 Philip J Perry <phil@elrepo.org> - 361.42-1
- Updated to version 361.42

* Tue Mar 01 2016 Philip J Perry <phil@elrepo.org> - 361.28-1
- Updated to version 361.28
- Adds GLVND support
- This package ships the legacy non-GLVND enabled LibGL.so by default

* Sun Jan 31 2016 Philip J Perry <phil@elrepo.org> - 352.79-1
- Updated to version 352.79

* Fri Nov 20 2015 Philip J Perry <phil@elrepo.org> - 352.63-1
- Updated to version 352.63
- Adds support for Xorg 1.18 (Video Driver ABI 20)

* Sat Oct 17 2015 Philip J Perry <phil@elrepo.org> - 352.55-1
- Updated to version 352.55

* Sat Aug 29 2015 Philip J Perry <phil@elrepo.org> - 352.41-1
- Updated to version 352.41
- Add CUDA provides

* Sat Aug 01 2015 Philip J Perry <phil@elrepo.org> - 352.30-1
- Updated to version 352.30
- Add requires for yum-plugin-nvidia

* Fri Jul 03 2015 Philip J Perry <phil@elrepo.org> - 352.21-3
- Add blacklist() provides.
- Revert modalias() provides.

* Wed Jul 01 2015 Philip J Perry <phil@elrepo.org> - 352.21-2
- Add modalias() provides.

* Wed Jun 17 2015 Philip J Perry <phil@elrepo.org> - 352.21-1
- Updated to version 352.21

* Wed Apr 08 2015 Philip J Perry <phil@elrepo.org> - 346.59-1
- Updated to version 346.59

* Wed Feb 25 2015 Philip J Perry <phil@elrepo.org> - 346.47-1
- Updated to version 346.47

* Sat Jan 17 2015 Philip J Perry <phil@elrepo.org> - 346.35-1
- Updated to version 346.35
- Drops support of older G8x, G9x, and GT2xx GPUs

* Fri Dec 12 2014 Philip J Perry <phil@elrepo.org> - 340.65-1
- Updated to version 340.65
- Adds support for Xorg 1.17 (Video Driver ABI 19)

* Thu Nov 06 2014 Philip J Perry <phil@elrepo.org> - 340.58-1
- Updated to version 340.58

* Sat Oct 04 2014 Philip J Perry <phil@elrepo.org> - 340.46-1
- Updated to version 340.46

* Sat Aug 30 2014 Philip J Perry <phil@elrepo.org> - 340.32-2
- Revert to /sbin/ldconfig

* Sat Aug 16 2014 Philip J Perry <phil@elrepo.org> - 340.32-1
- Updated to version 340.32
- Disable nouveau in /etc/default/grub

* Wed Jul 09 2014 Philip J Perry <phil@elrepo.org> - 340.24-1
- Updated to version 340.24
- Adds support for Xorg 1.16

* Sat Jul 05 2014 Philip J Perry <phil@elrepo.org> - 331.89-1
- Updated to version 331.89
- Update initramfs images for kABI compatible kernels

* Tue Jun 10 2014 Philip J Perry <phil@elrepo.org> - 331.79-2
- Rebuilt for rhel-7.0 release

* Sat May 03 2014 Philip J Perry <phil@elrepo.org> - 331.79-1
- Initial build for RHEL7RC
