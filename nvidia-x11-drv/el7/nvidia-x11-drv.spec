# Define the Max Xorg version (ABI) that this driver release supports
# See README.txt, Chapter 2. Minimum Software Requirements or
# https://download.nvidia.com/XFree86/Linux-x86_64/550.144.03/README/minimumrequirements.html

%define		max_xorg_ver	1.20.99

%define		debug_package	%{nil}
%define		_use_internal_dependency_generator	0

Name:		nvidia-x11-drv
Version:	550.144.03
Release:	1%{?dist}
Group:		User Interface/X Hardware Support
License:	Distributable
Summary:	NVIDIA OpenGL X11 display driver files
URL:		https://www.nvidia.com/

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	i686 x86_64

# Sources.
Source0:	https://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run

%if %{?_with_src:0}%{!?_with_src:1}
NoSource: 0
%endif

Source1:	nvidia-provides.sh
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
# For systemd_ scriptlets
BuildRequires:	systemd

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
Conflicts:	nvidia-x11-drv-470xx
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
Conflicts:	xorg-x11-drv-nvidia-470xx

%description
This package provides the proprietary NVIDIA OpenGL X11 display driver files.

%package libs
Summary:	Libraries for the Proprietary NVIDIA driver
Group:		User Interface/X Hardware Support
## Remove requires for nvidia-x11-drv to allow installation of
## nvidia-x11-drv-libs on headless systems. See bug 
## https://elrepo.org/bugs/view.php?id=926
## Requires:	%%{name} = %%{?epoch:%%{epoch}:}%%{version}-%%{release}
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
%{__mkdir_p} html/samples/systemd/system/
%{__mkdir_p} html/samples/systemd/system-sleep/
%{__mv} LICENSE NVIDIA_Changelog pkg-history.txt README.txt html/
%{__mv} nvidia-persistenced-init.tar.bz2 html/samples/
%{__mv} systemd/nvidia-sleep.sh html/samples/systemd/
%{__mv} systemd/system/nvidia-*.service html/samples/systemd/system/
%{__mv} systemd/system-sleep/nvidia html/samples/systemd/system-sleep/
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
%{__install} -p -m 0755 nvidia-ngx-updater $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-persistenced $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-powerd $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-settings $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-smi $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-xconfig $RPM_BUILD_ROOT%{_bindir}/

%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/dbus-1/system.d/
%{__install} -p -m 0644 nvidia-dbus.conf $RPM_BUILD_ROOT%{_datadir}/dbus-1/system.d/nvidia-dbus.conf
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
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/gbm/
%{__install} -p -m 0644 15_nvidia_gbm.json $RPM_BUILD_ROOT%{_datadir}/egl/egl_external_platform.d/15_nvidia_gbm.json

# Install GL, tls and vdpau libs
%ifarch i686
pushd 32
%endif
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/vdpau/
%ifarch x86_64
%{__install} -p -m 0755 libcudadebugger.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libcuda.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libEGL_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libGLESv1_CM_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libGLESv2_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libGLX_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-allocator.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-api.so.1 $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-eglcore.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-egl-gbm.so.1.1.1 $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-egl-wayland.so.1.1.13 $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-glcore.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-glsi.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-glvkspirv.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-gpucomp.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-gtk3.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-ngx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-nvvm.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-opticalflow.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
# Required for RHEL7 and RHEL8
%{__install} -p -m 0755 libnvidia-pkcs11.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
# Required on RHEL9
## %{__install} -p -m 0755 libnvidia-pkcs11-openssl3.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-ptxjitcompiler.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-rtcore.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-wayland-client.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvoptix.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/
%ifarch x86_64
# NGX Wine libs
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/nvidia/wine/
%{__install} -p -m 0755 _nvngx.dll $RPM_BUILD_ROOT%{_libdir}/nvidia/wine/
%{__install} -p -m 0755 nvngx.dll $RPM_BUILD_ROOT%{_libdir}/nvidia/wine/
%endif
%ifarch i686
popd
%endif

# Install X driver and extension 
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/
%{__install} -p -m 0755 nvidia_drv.so $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__install} -p -m 0755 libglxserver_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/

# Create the symlinks
%ifarch x86_64
%{__ln_s} libcudadebugger.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libcudadebugger.so.1
%endif
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
pushd $RPM_BUILD_ROOT%{_libdir}/gbm
%{__ln_s} ../libnvidia-allocator.so.%{version} nvidia-drm_gbm.so
popd
%ifarch x86_64
%{__ln_s} libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-cfg.so.1
%{__ln_s} libnvidia-egl-gbm.so.1.1.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-egl-gbm.so
%{__ln_s} libnvidia-egl-gbm.so.1.1.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-egl-gbm.so.1
%{__ln_s} libnvidia-egl-wayland.so.1.1.13 $RPM_BUILD_ROOT%{_libdir}/libnvidia-egl-wayland.so
%{__ln_s} libnvidia-egl-wayland.so.1.1.13 $RPM_BUILD_ROOT%{_libdir}/libnvidia-egl-wayland.so.1
%endif
%{__ln_s} libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-encode.so
%{__ln_s} libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-encode.so.1
%{__ln_s} libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-fbc.so
%{__ln_s} libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-fbc.so.1
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ml.so
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ml.so.1
%ifarch x86_64
%{__ln_s} libnvidia-ngx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ngx.so
%{__ln_s} libnvidia-ngx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ngx.so.1
%endif
%{__ln_s} libnvidia-nvvm.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-nvvm.so.4
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
# Install data file for nvoptix
%{__install} -p -m 0644 nvoptix.bin $RPM_BUILD_ROOT%{_datadir}/nvidia/

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

# Extract and install nvidia-persistenced systemd script
%{__tar} xf html/samples/nvidia-persistenced-init.tar.bz2
%{__mkdir_p} $RPM_BUILD_ROOT%{_unitdir}/
%{__install} -p -m 0644 nvidia-persistenced-init/systemd/nvidia-persistenced.service.template \
  $RPM_BUILD_ROOT%{_unitdir}/nvidia-persistenced.service
# Set the username for the daemon to root
%{__sed} -i -e "s/__USER__/root/" $RPM_BUILD_ROOT%{_unitdir}/nvidia-persistenced.service

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
%systemd_post nvidia-persistenced.service

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
%systemd_preun nvidia-persistenced.service

%postun
/sbin/ldconfig
%systemd_postun_with_restart nvidia-persistenced.service

%postun libs
/sbin/ldconfig

%files
%defattr(-,root,root,-)
%doc nvidiapkg/html/*
%{_mandir}/man1/nvidia*.*
%{_datadir}/pixmaps/nvidia-settings.png
%{_datadir}/applications/*nvidia-settings.desktop
%{_datadir}/dbus-1/system.d/nvidia-dbus.conf
%{_datadir}/egl/egl_external_platform.d/10_nvidia_wayland.json
%{_datadir}/egl/egl_external_platform.d/15_nvidia_gbm.json
%{_datadir}/glvnd/egl_vendor.d/10_nvidia.json
%{_datadir}/vulkan/icd.d/nvidia_icd.json
%{_datadir}/vulkan/implicit_layer.d/nvidia_layers.json
%dir %{_datadir}/nvidia/
%{_datadir}/nvidia/nvidia-application-profiles-*
%{_datadir}/nvidia/*.bin
%{_datadir}/X11/xorg.conf.d/nvidia-drm-outputclass.conf
%{_bindir}/nvidia-bug-report.sh
%{_bindir}/nvidia-cuda-mps-control
%{_bindir}/nvidia-cuda-mps-server
%{_bindir}/nvidia-debugdump
%attr(4755, root, root) %{_bindir}/nvidia-modprobe
%{_bindir}/nvidia-ngx-updater
%{_bindir}/nvidia-persistenced
%{_bindir}/nvidia-powerd
%{_bindir}/nvidia-settings
%{_bindir}/nvidia-smi
%{_bindir}/nvidia-xconfig
%config %{_sysconfdir}/X11/nvidia-xorg.conf
%{_sysconfdir}/OpenCL/vendors/nvidia.icd
%dir %{_prefix}/lib/nvidia/
%{_prefix}/lib/nvidia/alternate-install*
%{_libdir}/xorg/modules/drivers/nvidia_drv.so
%{_libdir}/xorg/modules/extensions/libglxserver_nvidia.*
%{_unitdir}/nvidia-persistenced.service

%files libs
%defattr(-,root,root,-)
%{_libdir}/lib*
%dir %{_libdir}/gbm/
%{_libdir}/gbm/nvidia*
%{_libdir}/vdpau/libvdpau_nvidia.*
%ifarch x86_64
%dir %{_libdir}/nvidia/
%{_libdir}/nvidia/wine/*.dll
%endif

%changelog
* Tue Jan 28 2025 Tuan Hoang <tqhoang@elrepo.org> - 550.144.03-1
- Updated to version 550.144.03

* Thu Dec 26 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.142-1
- Updated to version 550.142

* Tue Nov 19 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.135-1
- Updated to version 550.135

* Tue Oct 22 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.127.05-1
- Updated to version 550.127.05

* Sat Oct 05 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.120-1
- Updated to version 550.120

* Thu Aug 01 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.107.02-1
- Updated to version 550.107.02

* Tue Jul 09 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.100-1
- Updated to version 550.100

* Thu Jun 06 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.90.07-2
- Fix broken symlink nvidia-drm_gbm.so

* Wed Jun 05 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.90.07-1
- Updated to version 550.90.07

* Tue May 14 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.78-1
- Updated to version 550.78

* Thu Apr 18 2024 Philip J Perry <phil@elrepo.org> - 550.76-1
- Updated to version 550.76

* Sat Mar 23 2024 Philip J Perry <phil@elrepo.org> - 550.67-1
- Updated to version 550.67

* Sun Feb 25 2024 Philip J Perry <phil@elrepo.org> - 550.54.14-1
- Updated to version 550.54.14

* Wed Jan 17 2024 Tuan Hoang <tqhoang@elrepo.org> - 535.154.05-1
- Updated to version 535.154.05
- Fix 'file listed twice' rpmbuild warning for nvidia.icd

* Wed Nov 08 2023 Philip J Perry <phil@elrepo.org> - 535.129.03-1
- Updated to version 535.129.03

* Mon Sep 25 2023 Philip J Perry <phil@elrepo.org> - 535.113.01-1
- Updated to version 535.113.01

* Wed Aug 23 2023 Philip J Perry <phil@elrepo.org> - 535.104.05-1
- Updated to version 535.104.05

* Wed Aug 09 2023 Philip J Perry <phil@elrepo.org> - 535.98-1
- Updated to version 535.98

* Thu Jul 20 2023 Philip J Perry <phil@elrepo.org> - 535.86.05-1
- Updated to version 535.86.05

* Sun Jun 25 2023 Philip J Perry <phil@elrepo.org> - 535.54.03-1
- Updated to version 535.54.03

* Wed May 10 2023 Philip J Perry <phil@elrepo.org> - 525.116.04-1
- Updated to version 525.116.04

* Wed Apr 26 2023 Philip J Perry <phil@elrepo.org> - 525.116.03-1
- Updated to version 525.116.03

* Fri Mar 31 2023 Philip J Perry <phil@elrepo.org> - 525.105.17-1
- Updated to version 525.105.17

* Thu Mar 30 2023 Philip J Perry <phil@elrepo.org> - 525.89.02-1
- Updated to version 525.89.02

* Sat Jan 21 2023 Philip J Perry <phil@elrepo.org> - 525.85.05-1
- Updated to version 525.85.05

* Fri Jan 06 2023 Philip J Perry <phil@elrepo.org> - 525.78.01-1
- Updated to version 525.78.01

* Tue Nov 29 2022 Philip J Perry <phil@elrepo.org> - 525.60.11-1
- Updated to version 525.60.11

* Sun Nov 27 2022 Philip J Perry <phil@elrepo.org> - 515.86.01-1
- Updated to version 515.86.01

* Sat Nov 12 2022 - jthiltges
- Install systemd unit file for nvidia-persistenced
  [https://github.com/elrepo/packages/commit/86005affaab9ecf13f4c294f0562976d5d06d441]

* Sat Sep 24 2022 Philip J Perry <phil@elrepo.org> - 515.76-1
- Updated to version 515.76

* Sun Aug 07 2022 Philip J Perry <phil@elrepo.org> - 515.65.01-1
- Updated to version 515.65.01

* Wed Jun 29 2022 Philip J Perry <phil@elrepo.org> - 515.57-1
- Updated to version 515.57

* Fri Jun 03 2022 Philip J Perry <phil@elrepo.org> - 515.48.07-1
- Updated to version 515.48.07

* Mon May 23 2022 Philip J Perry <phil@elrepo.org> - 510.73.05-1
- Updated to version 510.73.05

* Wed Apr 27 2022 Philip J Perry <phil@elrepo.org> - 510.68.02-1
- Updated to version 510.68.02

* Sat Mar 26 2022 Philip J Perry <phil@elrepo.org> - 510.60.02-1
- Updated to version 510.60.02

* Tue Feb 15 2022 Philip J Perry <phil@elrepo.org> - 510.54-1
- Updated to version 510.54

* Thu Feb 03 2022 Philip J Perry <phil@elrepo.org> - 510.47.03-1
- Updated to version 510.47.03
- Drops support for older legacy kepler GPUs

* Tue Feb 01 2022 Philip J Perry <phil@elrepo.org> - 470.103.01-1
- Updated to version 470.103.01

* Tue Dec 14 2021 Philip J Perry <phil@elrepo.org> - 470.94-1
- Updated to version 470.94
- Fix broken SONAME dependency chain for libnvidia-vulkan-producer.so
  [https://elrepo.org/bugs/view.php?id=1159]

* Thu Nov 11 2021 Philip J Perry <phil@elrepo.org> - 470.86-1
- Updated to version 470.86
- Add libnvidia-vulkan-producer.so

* Thu Oct 28 2021 Philip J Perry <phil@elrepo.org> - 470.82.00-1
- Updated to version 470.82.00

* Tue Sep 21 2021 Philip J Perry <phil@elrepo.org> - 470.74-1
- Updated to version 470.74

* Wed Aug 11 2021 Philip J Perry <phil@elrepo.org> - 470.63.01-1
- Updated to version 470.63.01
- Move Wine libs to %%{_libdir}/nvidia/wine/
- Move firmware into kmod package as is for nvidia.ko module

* Mon Jul 19 2021 Philip J Perry <phil@elrepo.org> - 470.57.02-1
- Updated to version 470.57.02
