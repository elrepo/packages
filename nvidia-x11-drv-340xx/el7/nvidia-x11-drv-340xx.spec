# Define the Max Xorg version (ABI) that this driver release supports
# See README.txt, Chapter 2. Minimum Software Requirements or
# ftp://download.nvidia.com/XFree86/Linux-x86_64/340.106/README/minimumrequirements.html
%define		max_xorg_ver	1.19.99

%define		nvidialibdir	%{_libdir}/nvidia
%define		nvidialib32dir	%{_prefix}/lib/nvidia

%define		debug_package	%{nil}

Name:		nvidia-x11-drv-340xx
Version:	340.106
Release:	1%{?dist}
Group:		User Interface/X Hardware Support
License:	Distributable
Summary:	NVIDIA OpenGL X11 display driver files
URL:		http://www.nvidia.com/

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	x86_64

# Sources.
Source0:	ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
NoSource: 0

Source1:	nvidia-xorg.conf
Source2:	99-nvidia.conf
Source3:	nvidia.ld.so.conf
Source4:	alternate-install-present

# Fix broken SONAME dependency chain
Provides: libnvcuvid.so()(64bit)
Provides: libGL.so()(64bit)

# Provides for CUDA
Provides:	cuda-driver = %{version}
Provides:	cuda-drivers = %{version}
Provides:	nvidia-drivers = %{version}

# provides desktop-file-install
BuildRequires:	desktop-file-utils
BuildRequires:	perl

Requires:	perl
Requires:	xorg-x11-server-Xorg <= %{max_xorg_ver}
Requires:	nvidia-340xx-kmod = %{?epoch:%{epoch}:}%{version}
Requires(post):	nvidia-340xx-kmod = %{?epoch:%{epoch}:}%{version}

Requires(post):	/sbin/ldconfig

Requires(post):	 dracut

Requires(post):	 grubby
Requires(preun): grubby

# elrepo
Conflicts:	nvidia-x11-drv
Conflicts:	nvidia-x11-drv-32bit
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

%description
This package provides the proprietary NVIDIA OpenGL X11 display driver files.

%package 32bit
Summary:	Compatibility 32-bit files for the 64-bit Proprietary NVIDIA driver
Group:		User Interface/X Hardware Support
Requires:	%{name} = %{version}-%{release}
Requires(post):	/sbin/ldconfig
# Fix broken SONAME dependency chain
Provides: libnvcuvid.so
Provides: libGL.so

%description 32bit
Compatibility 32-bit files for the 64-bit Proprietary NVIDIA driver.

%prep
%setup -q -c -T
sh %{SOURCE0} --extract-only --target nvidiapkg

# Lets just take care of all the docs here rather than during install
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
%{__install} -p -m 0755 nvidia-cuda-mps-control $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-cuda-mps-server $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-debugdump $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-modprobe $RPM_BUILD_ROOT%{_bindir}/
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
# Added in 340.24 driver
%{__install} -p -m 0755 libEGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGLESv1_CM.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGLESv2.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGL.la $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvcuvid.so in 260.xx series driver
%{__install} -p -m 0755 libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-compiler.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-eglcore.so in 340.24 driver
%{__install} -p -m 0755 libnvidia-eglcore.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-encode.so in 310.19 driver
%{__install} -p -m 0755 libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-fbc.so in 331.20 driver
%{__install} -p -m 0755 libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-glcore.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-glsi.so in 340.24 driver
%{__install} -p -m 0755 libnvidia-glsi.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-ifr.so in 325.15 driver
%{__install} -p -m 0755 libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-ml.so in 270.xx series driver
%{__install} -p -m 0755 libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-vgxcfg.so in 325.15 driver and removed in 331.20 driver
# %{__install} -p -m 0755 libnvidia-vgxcfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-opencl.so in 304.xx series driver
%{__install} -p -m 0755 libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libOpenCL.so.* $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 tls/*.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/tls/

# Install 32bit compat GL, tls and vdpau libs
%{__mkdir_p} $RPM_BUILD_ROOT%{_prefix}/lib/vdpau/
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialib32dir}/tls/
%{__install} -p -m 0755 32/libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_prefix}/lib/vdpau/
%{__install} -p -m 0755 32/libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
# Added libEGL and libGLES libs in 331.20 driver
%{__install} -p -m 0755 32/libEGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGLESv1_CM.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGLESv2.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGL.la $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-compiler.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
# Added libnvidia-eglcore in 331.20 driver
%{__install} -p -m 0755 32/libnvidia-eglcore.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
# Added missing 32-bit libnvidia-fbc.so in 331.67 driver
%{__install} -p -m 0755 32/libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-glcore.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
# Added libnvidia-glsi in 331.20 driver
%{__install} -p -m 0755 32/libnvidia-glsi.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libOpenCL.so.* $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/tls/*.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/tls/

# Install X driver and extension 
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/
%{__install} -p -m 0755 nvidia_drv.so $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__install} -p -m 0755 libglx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/

# Create the symlinks
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libcuda.so
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libcuda.so.1
%{__ln_s} libEGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libEGL.so
%{__ln_s} libEGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libEGL.so.1
%{__ln_s} libGLESv1_CM.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGLESv1_CM.so
%{__ln_s} libGLESv1_CM.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGLESv1_CM.so.1
%{__ln_s} libGLESv2.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGLESv2.so
%{__ln_s} libGLESv2.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGLESv2.so.2
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGL.so
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGL.so.1
# Added libnvcuvid.so in 260.xx series driver
%{__ln_s} libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvcuvid.so
%{__ln_s} libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvcuvid.so.1
%{__ln_s} libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-cfg.so
%{__ln_s} libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-cfg.so.1
# Added libnvidia-encode.so in 310.19 driver
%{__ln_s} libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-encode.so
%{__ln_s} libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-encode.so.1
# Added libnvidia-fbc.so in 331.20 driver
%{__ln_s} libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-fbc.so
%{__ln_s} libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-fbc.so.1
# Added libnvidia-ifr.so in 325.15 driver
%{__ln_s} libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-ifr.so
%{__ln_s} libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-ifr.so.1
# Added libnvidia-ml.so in 270.xx series driver
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-ml.so
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-ml.so.1
# Added libnvidia-vgxcfg.so in 325.15 driver and removed in 331.20 driver
# %{__ln_s} libnvidia-vgxcfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-vgxcfg.so
# %{__ln_s} libnvidia-vgxcfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-vgxcfg.so.1
# Added libnvidia-opencl.so in 304.xx series driver
%{__ln_s} libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-opencl.so.1
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialibdir}/libOpenCL.so
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialibdir}/libOpenCL.so.1
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialibdir}/libOpenCL.so.1.0
%{__ln_s} libglx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/libglx.so
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/libvdpau_nvidia.so
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/libvdpau_nvidia.so.1

# Create the 32-bit symlinks for the 32-bit compat package
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libcuda.so
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libcuda.so.1
%{__ln_s} libEGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libEGL.so
%{__ln_s} libEGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libEGL.so.1
%{__ln_s} libGLESv1_CM.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGLESv1_CM.so
%{__ln_s} libGLESv1_CM.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGLESv1_CM.so.1
%{__ln_s} libGLESv2.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGLESv2.so
%{__ln_s} libGLESv2.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGLESv2.so.2
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGL.so
%{__ln_s} libGL.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGL.so.1
%{__ln_s} libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvcuvid.so
%{__ln_s} libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvcuvid.so.1
%{__ln_s} libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-encode.so
%{__ln_s} libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-encode.so.1
%{__ln_s} libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-fbc.so
%{__ln_s} libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-fbc.so.1
%{__ln_s} libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-ifr.so
%{__ln_s} libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-ifr.so.1
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-ml.so
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-ml.so.1
%{__ln_s} libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-opencl.so.1
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libOpenCL.so
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libOpenCL.so.1
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libOpenCL.so.1.0
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_prefix}/lib/vdpau/libvdpau_nvidia.so
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_prefix}/lib/vdpau/libvdpau_nvidia.so.1

# Install man pages
%{__mkdir_p} $RPM_BUILD_ROOT%{_mandir}/man1/
%{__install} -p -m 0644 nvidia-{cuda-mps-control,modprobe,settings,smi,xconfig}.1.gz $RPM_BUILD_ROOT%{_mandir}/man1/

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

# Install the Xorg conf files
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/X11/
%{__install} -p -m 0644 %{SOURCE1} $RPM_BUILD_ROOT%{_sysconfdir}/X11/nvidia-xorg.conf
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/X11/xorg.conf.d/
%{__install} -p -m 0644 %{SOURCE2} $RPM_BUILD_ROOT%{_sysconfdir}/X11/xorg.conf.d/99-nvidia.conf
# Install ld.so.conf.d file
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/
%{__install} -p -m 0644 %{SOURCE3} $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nvidia.conf
# Install alternate-install-present file
# This file tells the NVIDIA installer that a packaged version of the driver is already present on the system
%{__install} -p -m 0644 %{SOURCE4} $RPM_BUILD_ROOT%{nvidialibdir}/alternate-install-present

popd

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%pre
# Warn on libglamoregl
if [ -e /usr/share/X11/xorg.conf.d/glamor.conf ]; then
    echo "WARNING: libglamoregl conflicts with NVIDIA drivers"
    echo "         Disable glamoregl or uninstall xorg-x11-glamor"
    echo "         See: http://elrepo.org/tiki/kmod-nvidia (Known Issues) for more information"
fi

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

%post 32bit
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

%postun 32bit
/sbin/ldconfig

%files
%defattr(-,root,root,-)
%doc nvidiapkg/html/*
%{_mandir}/man1/nvidia*.*
%{_datadir}/pixmaps/nvidia-settings.png
%{_datadir}/applications/*nvidia-settings.desktop
%{_datadir}/nvidia/nvidia-application-profiles-*
%{_bindir}/nvidia-bug-report.sh
%{_bindir}/nvidia-cuda-mps-control
%{_bindir}/nvidia-cuda-mps-server
%{_bindir}/nvidia-debugdump
%attr(4755, root, root) %{_bindir}/nvidia-modprobe
%{_bindir}/nvidia-settings
%{_bindir}/nvidia-smi
%{_bindir}/nvidia-xconfig
%config %{_sysconfdir}/X11/nvidia-xorg.conf
%config %{_sysconfdir}/X11/xorg.conf.d/99-nvidia.conf
%config %{_sysconfdir}/ld.so.conf.d/nvidia.conf
%{_sysconfdir}/OpenCL/vendors/nvidia.icd

# now the libs
%dir %{nvidialibdir}
%{nvidialibdir}/lib*
%{nvidialibdir}/alternate-install*
%dir %{nvidialibdir}/tls
%{nvidialibdir}/tls/lib*
%{_libdir}/vdpau/libvdpau_nvidia.*
%{_libdir}/xorg/modules/drivers/nvidia_drv.so
%dir %{_libdir}/xorg/modules/extensions/nvidia
%{_libdir}/xorg/modules/extensions/nvidia/libglx.*

# 32-bit compatibility libs
%files 32bit
%defattr(-,root,root,-)
%dir %{nvidialib32dir}
%{nvidialib32dir}/lib*
%dir %{nvidialib32dir}/tls
%{nvidialib32dir}/tls/lib*
%{_prefix}/lib/vdpau/libvdpau_nvidia.*

%changelog
* Fri Feb 02 2018 Philip J Perry <phil@elrepo.org> - 340.106-1
- Updated to version 340.106
- Add CUDA provides for nvidia-drivers

* Sat Feb 25 2017 Philip J Perry <phil@elrepo.org> - 340.102-1
- Updated to version 340.102
- Use plymouth.ignore-udev to allow text mode booting [David Bell]

* Sat Dec 17 2016 Philip J Perry <phil@elrepo.org> - 340.101-1
- Updated to version 340.101
- Adds support for Xorg 1.19 (Video Driver ABI 23)

* Tue Nov 08 2016 Philip J Perry <phil@elrepo.org> - 340.98-1
- Updated to version 340.98

* Fri Nov 20 2015 Philip J Perry <phil@elrepo.org> - 340.96-1
- Updated to version 340.96
- Adds support for Xorg 1.18 (Video Driver ABI 20)

* Sat Sep 12 2015 Philip J Perry <phil@elrepo.org> - 340.93-1
- Updated to version 340.93
- Add CUDA provides

* Thu Feb 05 2015 Philip J Perry <phil@elrepo.org> - 340.76-1
- Updated to version 340.76

* Tue Dec 16 2014 Philip J Perry <phil@elrepo.org> - 340.65-1
- Updated to version 340.65
- Adds support for Xorg 1.17 (Video Driver ABI 19)

* Fri Sep 26 2014 Philip J Perry <phil@elrepo.org> - 340.32-2
- Fork to legacy release nvidia-340xx

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
