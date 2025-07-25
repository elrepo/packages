# Define the Max Xorg version (ABI) that this driver release supports
# See README.txt, Chapter 2. Minimum Software Requirements or
# https://download.nvidia.com/XFree86/Linux-x86_64/570.172.08/README/minimumrequirements.html

%define		max_xorg_ver	1.20.99
%define		debug_package	%{nil}

Name:		nvidia-x11-drv
Version:	570.172.08
Release:	1%{?dist}
Group:		User Interface/X Hardware Support
License:	MIT and Redistributable, no modification permitted
Summary:	NVIDIA OpenGL X11 display driver files
URL:		https://www.nvidia.com/

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	i686 x86_64

# Sources.
Source0:	https://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run

%if %{?_with_src:0}%{!?_with_src:1}
NoSource: 0
%endif

Source1:	alternate-install-present
Source2:	nvidia-xorg.conf

# Provides for CUDA
Provides:	cuda-driver = %{version}
Provides:	cuda-drivers = %{version}
Provides:	nvidia-drivers = %{version}

# Fix broken SONAME dependency chain
Provides:	libglxserver_nvidia.so()(64bit)

# provides desktop-file-install
BuildRequires:	desktop-file-utils
BuildRequires:	perl
# For systemd_ scriptlets
BuildRequires:	systemd-rpm-macros

Requires:	perl-interpreter
Requires:	xorg-x11-server-Xorg <= %{max_xorg_ver}

Requires:	%{name}-libs%{?_isa} = %{?epoch:%{epoch}:}%{version}-%{release}
Requires:	kmod-nvidia = %{?epoch:%{epoch}:}%{version}
Requires(post):	kmod-nvidia = %{?epoch:%{epoch}:}%{version}

Requires(post):	/sbin/ldconfig
Requires(post):	 dracut
Requires(post):	 grubby
Requires(preun): grubby

# elrepo
Conflicts:	nvidia-x11-drv-470xx
Conflicts:	nvidia-x11-drv-390xx
Conflicts:	nvidia-x11-drv-367xx
Conflicts:	nvidia-x11-drv-340xx
Conflicts:	nvidia-x11-drv-304xx
Conflicts:	nvidia-x11-drv-173xx
Conflicts:	nvidia-x11-drv-96xx

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
Summary:	Libraries for the NVIDIA OpenGL X11 display driver files
Group:		User Interface/X Hardware Support
## Remove requires for nvidia-x11-drv to allow installation of
## nvidia-x11-drv-libs on headless systems. See bug 
## https://elrepo.org/bugs/view.php?id=926
## Requires:	%%{name} = %%{?epoch:%%{epoch}:}%%{version}-%%{release}
Requires:	xorg-x11-server-Xorg <= %{max_xorg_ver}
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

Conflicts:	egl-gbm%{?_isa}

Conflicts:	nvidia-x11-drv-470xx-libs
Conflicts:	nvidia-x11-drv-390xx-libs
Conflicts:	nvidia-x11-drv-367xx-libs
Conflicts:	nvidia-x11-drv-340xx-libs
Conflicts:	nvidia-x11-drv-304xx-libs
Conflicts:	nvidia-x11-drv-173xx-libs
Conflicts:	nvidia-x11-drv-96xx-libs
Conflicts:	nvidia-x11-drv-470xx-32bit
Conflicts:	nvidia-x11-drv-390xx-32bit
Conflicts:	nvidia-x11-drv-367xx-32bit
Conflicts:	nvidia-x11-drv-340xx-32bit
Conflicts:	nvidia-x11-drv-304xx-32bit
Conflicts:	nvidia-x11-drv-173xx-32bit
Conflicts:	nvidia-x11-drv-96xx-32bit

%description libs
This package provides libraries for the proprietary NVIDIA OpenGL X11 display driver files.

%prep
%setup -q -c -T
sh %{SOURCE0} --extract-only --target nvidiapkg

# Lets just take care of all the docs here rather than during install
pushd nvidiapkg
%{__mkdir_p} html/samples/systemd/
%{__mkdir_p} html/samples/systemd/system/
%{__mkdir_p} html/samples/systemd/system-sleep/
%{__mv} nvidia-persistenced-init.tar.bz2 html/samples/
%{__mv} supported-gpus/LICENSE supported-gpus/LICENSE.supported-gpus
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

%ifarch x86_64
# Install nvidia tools
%{__mkdir_p} $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-bug-report.sh $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-cuda-mps-control $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-cuda-mps-server $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-debugdump $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-modprobe $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-ngx-updater $RPM_BUILD_ROOT%{_bindir}/
%{__install} -p -m 0755 nvidia-pcc $RPM_BUILD_ROOT%{_bindir}/
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

# Install Vulkan, VulkanSC and EGL loaders
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/vulkan/icd.d/
%{__install} -p -m 0644 nvidia_icd.json $RPM_BUILD_ROOT%{_datadir}/vulkan/icd.d/nvidia_icd.json
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/vulkan/implicit_layer.d/
%{__install} -p -m 0644 nvidia_layers.json $RPM_BUILD_ROOT%{_datadir}/vulkan/implicit_layer.d/nvidia_layers.json
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/vulkansc/icd.d/
%{__install} -p -m 0644 nvidia_icd_vksc.json $RPM_BUILD_ROOT%{_datadir}/vulkansc/icd.d/nvidia_icd.json
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/glvnd/egl_vendor.d/
%{__install} -p -m 0644 10_nvidia.json $RPM_BUILD_ROOT%{_datadir}/glvnd/egl_vendor.d/10_nvidia.json

# Install container runtime environments file
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/nvidia/files.d/
%{__install} -p -m 0644 sandboxutils-filelist.json $RPM_BUILD_ROOT%{_datadir}/nvidia/files.d/sandboxutils-filelist.json
%endif

%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/gbm/

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
%{__install} -p -m 0755 libnvidia-egl-gbm.so.1.1.2 $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-egl-xcb.so.1.0.2 $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-egl-xlib.so.1.0.2 $RPM_BUILD_ROOT%{_libdir}/
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
%if 0%{?rhel} <= 8
%{__install} -p -m 0755 libnvidia-pkcs11.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%elif 0%{?rhel} >= 9
%{__install} -p -m 0755 libnvidia-pkcs11-openssl3.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%endif
%{__install} -p -m 0755 libnvidia-ptxjitcompiler.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-rtcore.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-sandboxutils.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%ifarch x86_64
%{__install} -p -m 0755 libnvidia-vksc-core.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvidia-wayland-client.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%{__install} -p -m 0755 libnvoptix.so.%{version} $RPM_BUILD_ROOT%{_libdir}/
%endif
%{__install} -p -m 0755 libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/
%ifarch x86_64
# NGX Wine libs
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/nvidia/wine/
%{__install} -p -m 0755 _nvngx.dll $RPM_BUILD_ROOT%{_libdir}/nvidia/wine/
%{__install} -p -m 0755 nvngx.dll $RPM_BUILD_ROOT%{_libdir}/nvidia/wine/
%{__install} -p -m 0755 nvngx_dlssg.dll $RPM_BUILD_ROOT%{_libdir}/nvidia/wine/
%endif
%ifarch i686
popd
%endif

# Install X driver and extension 
%ifarch x86_64
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/
%{__install} -p -m 0755 nvidia_drv.so $RPM_BUILD_ROOT%{_libdir}/xorg/modules/drivers/
%{__install} -p -m 0755 libglxserver_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/
%endif

# Create the symlinks
%ifarch x86_64
%{__ln_s} libcudadebugger.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libcudadebugger.so.1
%{__ln_s} libcudadebugger.so.1 $RPM_BUILD_ROOT%{_libdir}/libcudadebugger.so
%endif
%{__ln_s} libcuda.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libcuda.so.1
%{__ln_s} libcuda.so.1 $RPM_BUILD_ROOT%{_libdir}/libcuda.so
%{__ln_s} libEGL_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libEGL_nvidia.so.0
%{__ln_s} libEGL_nvidia.so.0 $RPM_BUILD_ROOT%{_libdir}/libEGL_nvidia.so
%{__ln_s} libGLESv1_CM_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libGLESv1_CM_nvidia.so.1
%{__ln_s} libGLESv1_CM_nvidia.so.1 $RPM_BUILD_ROOT%{_libdir}/libGLESv1_CM_nvidia.so
%{__ln_s} libGLESv2_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libGLESv2_nvidia.so.2
%{__ln_s} libGLESv2_nvidia.so.2 $RPM_BUILD_ROOT%{_libdir}/libGLESv2_nvidia.so
%{__ln_s} libGLX_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libGLX_nvidia.so.0
%{__ln_s} libGLX_nvidia.so.0 $RPM_BUILD_ROOT%{_libdir}/libGLX_nvidia.so
%{__ln_s} libGLX_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libGLX_indirect.so.0
%{__ln_s} libGLX_indirect.so.0 $RPM_BUILD_ROOT%{_libdir}/libGLX_indirect.so
%{__ln_s} libnvcuvid.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvcuvid.so.1
%{__ln_s} libnvcuvid.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvcuvid.so
%{__ln_s} libnvidia-allocator.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-allocator.so.1
%{__ln_s} libnvidia-allocator.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-allocator.so
pushd $RPM_BUILD_ROOT%{_libdir}/gbm
%{__ln_s} ../libnvidia-allocator.so.1 nvidia-drm_gbm.so
popd
%ifarch x86_64
%{__ln_s} libnvidia-api.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-api.so
%{__ln_s} libnvidia-cfg.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-cfg.so.1
%{__ln_s} libnvidia-cfg.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-cfg.so
%endif
%{__ln_s} libnvidia-eglcore.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-eglcore.so
%{__ln_s} libnvidia-egl-gbm.so.1.1.2 $RPM_BUILD_ROOT%{_libdir}/libnvidia-egl-gbm.so.1
%{__ln_s} libnvidia-egl-gbm.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-egl-gbm.so
%{__ln_s} libnvidia-egl-xcb.so.1.0.2 $RPM_BUILD_ROOT%{_libdir}/libnvidia-egl-xcb.so.1
%{__ln_s} libnvidia-egl-xcb.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-egl-xcb.so
%{__ln_s} libnvidia-egl-xlib.so.1.0.2 $RPM_BUILD_ROOT%{_libdir}/libnvidia-egl-xlib.so.1
%{__ln_s} libnvidia-egl-xlib.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-egl-xlib.so
%{__ln_s} libnvidia-encode.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-encode.so.1
%{__ln_s} libnvidia-encode.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-encode.so
%{__ln_s} libnvidia-fbc.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-fbc.so.1
%{__ln_s} libnvidia-fbc.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-fbc.so
%{__ln_s} libnvidia-glcore.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-glcore.so
%{__ln_s} libnvidia-glsi.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-glsi.so
%{__ln_s} libnvidia-glvkspirv.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-glvkspirv.so
%{__ln_s} libnvidia-gpucomp.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-gpucomp.so
%ifarch x86_64
%{__ln_s} libnvidia-gtk3.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-gtk3.so
%endif
%{__ln_s} libnvidia-ml.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ml.so.1
%{__ln_s} libnvidia-ml.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-ml.so
%ifarch x86_64
%{__ln_s} libnvidia-ngx.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ngx.so.1
%{__ln_s} libnvidia-ngx.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-ngx.so
%endif
%{__ln_s} libnvidia-nvvm.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-nvvm.so.4
%{__ln_s} libnvidia-nvvm.so.4 $RPM_BUILD_ROOT%{_libdir}/libnvidia-nvvm.so
%{__ln_s} libnvidia-opencl.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-opencl.so.1
%{__ln_s} libnvidia-opencl.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-opencl.so
%{__ln_s} libnvidia-opticalflow.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-opticalflow.so.1
%{__ln_s} libnvidia-opticalflow.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-opticalflow.so
%ifarch x86_64
%if 0%{?rhel} <= 8
%{__ln_s} libnvidia-pkcs11.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-pkcs11.so
%elif 0%{?rhel} >= 9
%{__ln_s} libnvidia-pkcs11-openssl3.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-pkcs11-openssl3.so
%endif
%endif
%{__ln_s} libnvidia-ptxjitcompiler.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-ptxjitcompiler.so.1
%{__ln_s} libnvidia-ptxjitcompiler.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-ptxjitcompiler.so
%ifarch x86_64
%{__ln_s} libnvidia-rtcore.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-rtcore.so
%{__ln_s} libnvidia-sandboxutils.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-sandboxutils.so.1
%{__ln_s} libnvidia-sandboxutils.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-sandboxutils.so
%endif
%{__ln_s} libnvidia-tls.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-tls.so
%ifarch x86_64
%{__ln_s} libnvidia-vksc-core.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-vksc-core.so.1
%{__ln_s} libnvidia-vksc-core.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvidia-vksc-core.so
%{__ln_s} libnvidia-wayland-client.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvidia-wayland-client.so
%{__ln_s} libnvoptix.so.%{version} $RPM_BUILD_ROOT%{_libdir}/libnvoptix.so.1
%{__ln_s} libnvoptix.so.1 $RPM_BUILD_ROOT%{_libdir}/libnvoptix.so
%endif
%{__ln_s} libvdpau_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/vdpau/libvdpau_nvidia.so.1
%{__ln_s} libvdpau_nvidia.so.1 $RPM_BUILD_ROOT%{_libdir}/vdpau/libvdpau_nvidia.so
%ifarch x86_64
%{__ln_s} libglxserver_nvidia.so.%{version} $RPM_BUILD_ROOT%{_libdir}/xorg/modules/extensions/libglxserver_nvidia.so
%endif

%ifarch x86_64
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
%{__install} -p -m 0644 %{SOURCE1} $RPM_BUILD_ROOT%{_prefix}/lib/nvidia/alternate-install-present

# Extract and install nvidia-persistenced systemd script
%{__tar} xf html/samples/nvidia-persistenced-init.tar.bz2
%{__mkdir_p} $RPM_BUILD_ROOT%{_unitdir}/
%{__install} -p -m 0644 nvidia-persistenced-init/systemd/nvidia-persistenced.service.template \
  $RPM_BUILD_ROOT%{_unitdir}/nvidia-persistenced.service
# Set the username for the daemon to root
%{__sed} -i -e "s/__USER__/root/" $RPM_BUILD_ROOT%{_unitdir}/nvidia-persistenced.service
%endif

popd

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%post
%ifarch x86_64
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
%endif

%post libs
/sbin/ldconfig

%preun
%ifarch x86_64
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
%endif

%postun
%ifarch x86_64
/sbin/ldconfig
%systemd_postun_with_restart nvidia-persistenced.service
%endif

%postun libs
/sbin/ldconfig

%files
%defattr(-,root,root,-)
%ifarch x86_64
%license nvidiapkg/LICENSE
%doc nvidiapkg/NVIDIA_Changelog nvidiapkg/pkg-history.txt nvidiapkg/README.txt nvidiapkg/html/
%doc nvidiapkg/supported-gpus/supported-gpus.json nvidiapkg/supported-gpus/LICENSE.supported-gpus
%{_mandir}/man1/nvidia*.*
%{_datadir}/pixmaps/nvidia-settings.png
%{_datadir}/applications/*nvidia-settings.desktop
%{_datadir}/dbus-1/system.d/nvidia-dbus.conf
%{_datadir}/glvnd/egl_vendor.d/10_nvidia.json
%{_datadir}/vulkan/icd.d/nvidia_icd.json
%{_datadir}/vulkan/implicit_layer.d/nvidia_layers.json
%{_datadir}/vulkansc/icd.d/nvidia_icd.json
%dir %{_datadir}/nvidia/
%{_datadir}/nvidia/files.d/sandboxutils-filelist.json
%{_datadir}/nvidia/nvidia-application-profiles-*
%{_datadir}/nvidia/*.bin
%{_datadir}/X11/xorg.conf.d/nvidia-drm-outputclass.conf
%{_bindir}/nvidia-bug-report.sh
%{_bindir}/nvidia-cuda-mps-control
%{_bindir}/nvidia-cuda-mps-server
%{_bindir}/nvidia-debugdump
%attr(4755, root, root) %{_bindir}/nvidia-modprobe
%{_bindir}/nvidia-ngx-updater
%{_bindir}/nvidia-pcc
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
%endif

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
* Fri Jul 18 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.172.08-1
- Updated to version 570.172.08

* Thu Jun 19 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.169-1
- Updated to version 570.169

* Tue May 20 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.153.02-1
- Updated to version 570.153.02

* Wed May 14 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.144-2
- Rebuilt against RHEL 9.6 GA kernel

* Sat Apr 26 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.144-1
- Updated to version 570.144
- Remove unnecessary gtk2 files

* Sat Apr 05 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.133.07-1
- Updated to version 570.133.07
- Add LICENSE and supported-gpus.json files
- Add missing files (gtk2, vulkan, vulkansc, wayland, wine)
- Add more library symlinks

* Tue Jan 28 2025 Tuan Hoang <tqhoang@elrepo.org> - 550.144.03-1
- Updated to version 550.144.03
- Rebuilt against RHEL 9.5 GA kernel

* Thu Dec 26 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.142-1
- Updated to version 550.142
- Rebuilt against RHEL 9.5 GA kernel

* Tue Nov 19 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.135-1
- Updated to version 550.135
- Rebuilt against RHEL 9.5 GA kernel

* Tue Nov 12 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.127.05-2
- Rebuilt against RHEL 9.5 GA kernel

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

* Wed May 01 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.78-1
- Updated to version 550.78

* Thu Apr 18 2024 Philip J Perry <phil@elrepo.org> - 550.76-1
- Updated to version 550.76

* Sat Mar 23 2024 Philip J Perry <phil@elrepo.org> - 550.67-1
- Updated to version 550.67

* Sun Feb 25 2024 Philip J Perry <phil@elrepo.org> - 550.54.14-1
- Updated to version 550.54.14
- Fork for RHEL9
