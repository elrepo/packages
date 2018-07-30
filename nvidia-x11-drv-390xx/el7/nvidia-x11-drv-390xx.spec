# Define the Max Xorg version (ABI) that this driver release supports
# See README.txt, Chapter 2. Minimum Software Requirements or
# http://us.download.nvidia.com/XFree86/Linux-x86_64/390.77/README/minimumrequirements.html

%define		max_xorg_ver	1.20.99

%define		nvidialibdir	%{_libdir}/nvidia
%define		nvidialib32dir	%{_prefix}/lib/nvidia

%define		debug_package	%{nil}
%define		_use_internal_dependency_generator	0

Name:		nvidia-x11-drv-390xx
Version:	390.77
Release:	1%{?dist}
Group:		User Interface/X Hardware Support
License:	Distributable
Summary:	NVIDIA OpenGL X11 display driver files
URL:		http://www.nvidia.com/

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	x86_64

# Sources.
Source0:	http://us.download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
NoSource: 0

Source1:	nvidia-xorg.conf
Source2:	99-nvidia.conf
Source3:	nvidia.ld.so.conf
Source4:	alternate-install-present
Source5:	nvidia.sh
Source6:	nvidia.csh

# Provides for CUDA
Provides:	cuda-driver = %{version}
Provides:	cuda-drivers = %{version}
Provides:	nvidia-drivers = %{version}

# provides desktop-file-install
BuildRequires:	desktop-file-utils
BuildRequires:	perl

Requires:	perl
Requires:	xorg-x11-server-Xorg <= %{max_xorg_ver}
Requires:	nvidia-390xx-kmod = %{?epoch:%{epoch}:}%{version}
Requires(post):	nvidia-390xx-kmod = %{?epoch:%{epoch}:}%{version}

Requires(post):	/sbin/ldconfig

Requires(post):	 dracut

Requires(post):	 grubby
Requires(preun): grubby

# elrepo
Conflicts:	nvidia-x11-drv
Conflicts:	nvidia-x11-drv-32bit
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

%package 32bit
Summary:	Compatibility 32-bit files for the 64-bit Proprietary NVIDIA driver
Group:		User Interface/X Hardware Support
Requires:	%{name} = %{version}-%{release}
Requires(post):	/sbin/ldconfig

%description 32bit
Compatibility 32-bit files for the 64-bit Proprietary NVIDIA driver.

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
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/vulkan/icd.d/
%{__install} -p -m 0644 nvidia_icd.json.template $RPM_BUILD_ROOT%{_sysconfdir}/vulkan/icd.d/nvidia_icd.json
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/glvnd/egl_vendor.d/
%{__install} -p -m 0644 10_nvidia.json $RPM_BUILD_ROOT%{_datadir}/glvnd/egl_vendor.d/10_nvidia.json
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/egl/egl_external_platform.d/
%{__install} -p -m 0644 10_nvidia_wayland.json $RPM_BUILD_ROOT%{_datadir}/egl/egl_external_platform.d/10_nvidia_wayland.json

# Install GL, tls and vdpau libs
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/vdpau/
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialibdir}/tls/
%{__install} -p -m 0755 libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libEGL_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libEGL.so.1.1.0 $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGLdispatch.so.0 $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGLESv1_CM_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGLESv1_CM.so.1.2.0 $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGLESv2_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGLESv2.so.2.1.0 $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGL.la $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGL.so.1.7.0 $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGLX.so.0 $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libGLX_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvcuvid.so in 260.xx series driver
%{__install} -p -m 0755 libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-compiler.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-eglcore.so in 340.24 driver
%{__install} -p -m 0755 libnvidia-eglcore.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-egl-wayland.so in 367.27 driver. Not supported on RHEL
# %{__install} -p -m 0755 libnvidia-egl-wayland.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-encode.so in 310.19 driver
%{__install} -p -m 0755 libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-fatbinaryloader.so in 361.28 driver
%{__install} -p -m 0755 libnvidia-fatbinaryloader.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-fbc.so in 331.20 driver
%{__install} -p -m 0755 libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-glcore.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-glsi.so in 340.24 driver
%{__install} -p -m 0755 libnvidia-glsi.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added in 346.35 driver
%{__install} -p -m 0755 libnvidia-gtk2.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-gtk3.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-ifr.so in 325.15 driver
%{__install} -p -m 0755 libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-ml.so in 270.xx series driver
%{__install} -p -m 0755 libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-vgxcfg.so in 325.15 driver and removed in 331.20 driver
# %{__install} -p -m 0755 libnvidia-vgxcfg.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-opencl.so in 304.xx series driver
%{__install} -p -m 0755 libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libnvidia-ptxjitcompiler.so in 361.28 driver
%{__install} -p -m 0755 libnvidia-ptxjitcompiler.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 tls/*.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/tls/
%{__install} -p -m 0755 libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialibdir}/
# Added libOpenGL.so in 361.28 driver
%{__install} -p -m 0755 libOpenGL.so.0 $RPM_BUILD_ROOT%{nvidialibdir}/
%{__install} -p -m 0755 libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/

# Install 32bit compat GL, tls and vdpau libs
%{__mkdir_p} $RPM_BUILD_ROOT%{_prefix}/lib/vdpau/
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__mkdir_p} $RPM_BUILD_ROOT%{nvidialib32dir}/tls/
%{__install} -p -m 0755 32/libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libEGL_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libEGL.so.1.1.0 $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGLdispatch.so.0 $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGLESv1_CM_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGLESv1_CM.so.1.2.0 $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGLESv2_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGLESv2.so.2.1.0 $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGL.la $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGL.so.1.7.0 $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGLX.so.0 $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libGLX_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-compiler.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
# Added libnvidia-eglcore in 331.20 driver
%{__install} -p -m 0755 32/libnvidia-eglcore.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
# Added libnvidia-fatbinaryloader.so in 361.28 driver
%{__install} -p -m 0755 32/libnvidia-fatbinaryloader.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
# Added missing 32-bit libnvidia-fbc.so in 331.67 driver
%{__install} -p -m 0755 32/libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-glcore.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
# Added libnvidia-glsi in 331.20 driver
%{__install} -p -m 0755 32/libnvidia-glsi.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-ifr.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
# Added libnvidia-ptxjitcompiler.so in 361.28 driver
%{__install} -p -m 0755 32/libnvidia-ptxjitcompiler.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/tls/*.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/tls/
%{__install} -p -m 0755 32/libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libOpenGL.so.0 $RPM_BUILD_ROOT%{nvidialib32dir}/
%{__install} -p -m 0755 32/libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_prefix}/lib/vdpau/


# Install X driver and extension 
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/
%{__install} -p -m 0755 nvidia_drv.so $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__install} -p -m 0755 libglx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/

# Create the symlinks
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libcuda.so
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libcuda.so.1
%{__ln_s} libEGL_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libEGL_nvidia.so.0
%{__ln_s} libEGL.so.1.1.0 $RPM_BUILD_ROOT%{nvidialibdir}/libEGL.so
%{__ln_s} libEGL.so.1.1.0 $RPM_BUILD_ROOT%{nvidialibdir}/libEGL.so.1
%{__ln_s} libGLESv1_CM_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGLESv1_CM_nvidia.so.1
%{__ln_s} libGLESv1_CM.so.1.2.0 $RPM_BUILD_ROOT%{nvidialibdir}/libGLESv1_CM.so
%{__ln_s} libGLESv1_CM.so.1.2.0 $RPM_BUILD_ROOT%{nvidialibdir}/libGLESv1_CM.so.1
%{__ln_s} libGLESv2_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGLESv2_nvidia.so.2
%{__ln_s} libGLESv2.so.2.1.0 $RPM_BUILD_ROOT%{nvidialibdir}/libGLESv2.so
%{__ln_s} libGLESv2.so.2.1.0 $RPM_BUILD_ROOT%{nvidialibdir}/libGLESv2.so.2
%{__ln_s} libGL.so.1.7.0 $RPM_BUILD_ROOT%{nvidialibdir}/libGL.so
%{__ln_s} libGL.so.1.7.0 $RPM_BUILD_ROOT%{nvidialibdir}/libGL.so.1
%{__ln_s} libGLX.so.0 $RPM_BUILD_ROOT%{nvidialibdir}/libGLX.so
%{__ln_s} libGLX_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGLX_nvidia.so.0
%{__ln_s} libGLX_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libGLX_indirect.so.0
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
%{__ln_s} libnvidia-ptxjitcompiler.so.%{version} $RPM_BUILD_ROOT%{nvidialibdir}/libnvidia-ptxjitcompiler.so.1
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialibdir}/libOpenCL.so
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialibdir}/libOpenCL.so.1
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialibdir}/libOpenCL.so.1.0
%{__ln_s} libOpenGL.so.0 $RPM_BUILD_ROOT%{nvidialibdir}/libOpenGL.so
%{__ln_s} libglx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/nvidia/libglx.so
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/libvdpau_nvidia.so
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/libvdpau_nvidia.so.1

# Create the 32-bit symlinks for the 32-bit compat package
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libcuda.so
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libcuda.so.1
%{__ln_s} libEGL_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libEGL_nvidia.so.0
%{__ln_s} libEGL.so.1.1.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libEGL.so
%{__ln_s} libEGL.so.1.1.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libEGL.so.1
%{__ln_s} libGLESv1_CM_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGLESv1_CM_nvidia.so.1
%{__ln_s} libGLESv1_CM.so.1.2.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libGLESv1_CM.so
%{__ln_s} libGLESv1_CM.so.1.2.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libGLESv1_CM.so.1
%{__ln_s} libGLESv2_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGLESv2_nvidia.so.2
%{__ln_s} libGLESv2.so.2.1.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libGLESv2.so
%{__ln_s} libGLESv2.so.2.1.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libGLESv2.so.2
%{__ln_s} libGL.so.1.7.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libGL.so
%{__ln_s} libGL.so.1.7.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libGL.so.1
%{__ln_s} libGLX.so.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libGLX.so
%{__ln_s} libGLX_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGLX_nvidia.so.0
%{__ln_s} libGLX_nvidia.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libGLX_indirect.so.0
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
%{__ln_s} libnvidia-ptxjitcompiler.so.%{version} $RPM_BUILD_ROOT%{nvidialib32dir}/libnvidia-ptxjitcompiler.so.1
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libOpenCL.so
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libOpenCL.so.1
%{__ln_s} libOpenCL.so.1.0.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libOpenCL.so.1.0
%{__ln_s} libOpenGL.so.0 $RPM_BUILD_ROOT%{nvidialib32dir}/libOpenGL.so
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_prefix}/lib/vdpau/libvdpau_nvidia.so
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_prefix}/lib/vdpau/libvdpau_nvidia.so.1

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
%{__install} -p -m 0644 %{SOURCE1} $RPM_BUILD_ROOT%{_sysconfdir}/X11/nvidia-xorg.conf
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/X11/xorg.conf.d/
%{__install} -p -m 0644 %{SOURCE2} $RPM_BUILD_ROOT%{_sysconfdir}/X11/xorg.conf.d/99-nvidia.conf
# Install ld.so.conf.d file
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/
%{__install} -p -m 0644 %{SOURCE3} $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nvidia.conf
# Install alternate-install-present file
# This file tells the NVIDIA installer that a packaged version of the driver is already present on the system
%{__install} -p -m 0644 %{SOURCE4} $RPM_BUILD_ROOT%{nvidialibdir}/alternate-install-present
# Install profile.d files
%{__mkdir_p} $RPM_BUILD_ROOT%{_sysconfdir}/profile.d/
%{__install} -p -m 0644 %{SOURCE5} $RPM_BUILD_ROOT%{_sysconfdir}/profile.d/nvidia.sh
%{__install} -p -m 0644 %{SOURCE6} $RPM_BUILD_ROOT%{_sysconfdir}/profile.d/nvidia.csh

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
%{_datadir}/egl/egl_external_platform.d/10_nvidia_wayland.json
%{_datadir}/glvnd/egl_vendor.d/10_nvidia.json
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
%config %{_sysconfdir}/X11/xorg.conf.d/99-nvidia.conf
%config %{_sysconfdir}/ld.so.conf.d/nvidia.conf
%config(noreplace) %{_sysconfdir}/profile.d/nvidia.csh
%config(noreplace) %{_sysconfdir}/profile.d/nvidia.sh
%{_sysconfdir}/OpenCL/vendors/nvidia.icd
%{_sysconfdir}/vulkan/icd.d/nvidia_icd.json

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
