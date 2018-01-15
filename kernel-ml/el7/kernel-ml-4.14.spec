%global __spec_install_pre %{___build_pre}

# Define the version of the Linux Kernel Archive tarball.
%define LKAver 4.14.13

# Define the buildid, if required.
#define buildid .

# The following build options are enabled by default.
# Use either --without <option> on your rpmbuild command line
# or force the values to 0, here, to disable them.

# kernel-ml
%define with_default %{?_without_default: 0} %{?!_without_default: 1}
# kernel-ml-doc
%define with_doc     %{?_without_doc:     0} %{?!_without_doc:     1}
# kernel-ml-headers
%define with_headers %{?_without_headers: 0} %{?!_without_headers: 1}
# perf
%define with_perf    %{?_without_perf:    0} %{?!_without_perf:    1}
# tools
%define with_tools   %{?_without_tools:   0} %{?!_without_tools:   1}

# These architectures install vdso/ directories.
%define vdso_arches i686 x86_64

# Architecture defaults.
%define asmarch x86
%define buildarch %{_target_cpu}
%define hdrarch %{_target_cpu}

# Per-architecture tweaks.

%ifarch noarch
# Documentation only.
%define with_default 0
%define with_headers 0
%define with_perf 0
%define with_tools 0
%endif

%ifarch i686
# 32-bit kernel-ml, headers, perf & tools.
%define buildarch i386
%define hdrarch i386
%define with_doc 0
%endif

%ifarch x86_64
# 64-bit kernel-ml, headers, perf & tools.
%define with_doc 0
%endif

# Determine the sublevel number and set pkg_version.
%define sublevel %(echo %{LKAver} | %{__awk} -F\. '{ print $3 }')
%if "%{sublevel}" == ""
%define pkg_version %{LKAver}.0
%else
%define pkg_version %{LKAver}
%endif

# Set pkg_release.
%define pkg_release 1%{?buildid}%{?dist}

#
# Three sets of minimum package version requirements in the form of Conflicts.
#

#
# First the general kernel required versions, as per Documentation/Changes.
#
%define kernel_dot_org_conflicts ppp < 2.4.3-3, isdn4k-utils < 3.2-32, nfs-utils < 1.0.7-12, e2fsprogs < 1.37-4, util-linux < 2.12, jfsutils < 1.1.7-2, reiserfs-utils < 3.6.19-2, xfsprogs < 2.6.13-4, procps < 3.2.5-6.3, oprofile < 0.9.1-2, device-mapper-libs < 1.02.63-2, mdadm < 3.2.1-5

#
# Then a series of requirements that are distribution specific, either because
# the older versions have problems with the newer kernel or lack certain things
# that make integration in the distro harder than needed.
#
%define package_conflicts initscripts < 7.23, udev < 063-6, iptables < 1.3.2-1, ipw2200-firmware < 2.4, iwl4965-firmware < 228.57.2, selinux-policy-targeted < 1.25.3-14, squashfs-tools < 4.0, wireless-tools < 29-3

#
# We moved the drm include files into kernel-headers, make sure there's
# a recent enough libdrm-devel on the system that doesn't have those.
#
%define kernel_headers_conflicts libdrm-devel < 2.4.0-0.15

#
# Packages that need to be installed before the kernel because the %%post scripts make use of them.
#
%define kernel_prereq fileutils, module-init-tools >= 3.16-2, initscripts >= 8.11.1-1, grubby >= 8.28-2
%define initrd_prereq dracut >= 001-7

Name: kernel-ml
Summary: The Linux kernel. (The core of any Linux-based operating system.)
Group: System Environment/Kernel
License: GPLv2
URL: https://www.kernel.org/
Version: %{pkg_version}
Release: %{pkg_release}
ExclusiveArch: noarch x86_64
ExclusiveOS: Linux
Provides: kernel = %{version}-%{release}
Provides: kernel-%{_target_cpu} = %{version}-%{release}
Provides: kernel-uname-r = %{version}-%{release}.%{_target_cpu}
Provides: kernel-drm = 4.3.0
Provides: kernel-drm-nouveau = 16
Provides: kernel-modeset = 1
Provides: %{name} = %{version}-%{release}
Provides: %{name}-%{_target_cpu} = %{version}-%{release}
Provides: %{name}-uname-r = %{version}-%{release}.%{_target_cpu}
Provides: %{name}-drm = 4.3.0
Provides: %{name}-drm-nouveau = 16
Provides: %{name}-modeset = 1
Requires(pre): %{kernel_prereq}
Requires(pre): %{initrd_prereq}
Requires(pre): linux-firmware >= 20100806-2
Requires(post): %{_sbindir}/new-kernel-pkg
Requires(preun): %{_sbindir}/new-kernel-pkg
Conflicts: %{kernel_dot_org_conflicts}
Conflicts: %{package_conflicts}
# We can't let RPM do the dependencies automatically because it'll then pick up
# a correct but undesirable perl dependency from the module headers which
# isn't required for the kernel-ml proper to function.
AutoReq: no
AutoProv: yes

#
# List the packages used during the kernel-ml build.
#
BuildRequires: asciidoc, bash >= 2.03, bc, binutils >= 2.12, diffutils
BuildRequires: findutils, gawk, gcc >= 3.4.2 gzip, hostname, m4
BuildRequires: make >= 3.78, module-init-tools, net-tools, newt-devel
BuildRequires: openssl, openssl-devel, patch >= 2.5.4, perl
BuildRequires: redhat-rpm-config >= 9.1.0-55, sh-utils, tar, xmlto, xz
%if %{with_perf}
BuildRequires: audit-libs-devel, binutils-devel, bison, elfutils-devel
BuildRequires: java-1.8.0-openjdk-devel, numactl-devel, perl(ExtUtils::Embed)
BuildRequires: python-devel, slang-devel, xz-devel, zlib-devel
%endif
%if %{with_tools}
BuildRequires: gettext, ncurses-devel, pciutils-devel
%endif

# Sources.
Source0: https://www.kernel.org/pub/linux/kernel/v4.x/linux-%{LKAver}.tar.xz
Source1: config-%{version}-x86_64
Source2: cpupower.service
Source3: cpupower.config

# Do not package the source tarball.
NoSource: 0

%description
This package provides the Linux kernel (vmlinuz), the core of any
Linux-based operating system. The kernel handles the basic functions
of the OS: memory allocation, process allocation, device I/O, etc.

%package devel
Summary: Development package for building kernel modules to match the kernel.
Group: System Environment/Kernel
Provides: kernel-devel = %{version}-%{release}
Provides: kernel-devel-%{_target_cpu} = %{version}-%{release}
Provides: kernel-devel-uname-r = %{version}-%{release}.%{_target_cpu}
Provides: %{name}-devel = %{version}-%{release}
Provides: %{name}-devel-%{_target_cpu} = %{version}-%{release}
Provides: %{name}-devel-uname-r = %{version}-%{release}.%{_target_cpu}
AutoReqProv: no
Requires(pre): /usr/bin/find
Requires: perl
%description devel
This package provides the kernel header files and makefiles
sufficient to build modules against the kernel package.

%if %{with_doc}
%package doc
Summary: Various bits of documentation found in the kernel sources.
Group: Documentation
Provides: kernel-doc = %{version}-%{release}
Provides: %{name}-doc = %{version}-%{release}
Conflicts: kernel-doc < %{version}-%{release}
%description doc
This package provides documentation files from the kernel sources.
Various bits of information about the Linux kernel and the device
drivers shipped with it are documented in these files.

You'll want to install this package if you need a reference to the
options that can be passed to the kernel modules at load time.
%endif

%if %{with_headers}
%package headers
Summary: Header files of the kernel, for use by glibc.
Group: Development/System
Obsoletes: glibc-kernheaders < 3.0-46
Provides: glibc-kernheaders = 3.0-46
Provides: kernel-headers = %{version}-%{release}
Provides: %{name}-headers = %{version}-%{release}
Conflicts: kernel-headers < %{version}-%{release}
%description headers
This package provides the C header files that specify the interface
between the Linux kernel and userspace libraries & programs. The
header files define structures and constants that are needed when
building most standard programs. They are also required when
rebuilding the glibc package.
%endif

%if %{with_perf}
%package -n perf
Summary: Performance monitoring of the kernel.
Group: Development/System
License: GPLv2
%description -n perf
This package provides the perf tool and the supporting documentation
for performance monitoring of the Linux kernel.

%package -n python-perf
Summary: Python bindings for applications that will manipulate perf events.
Group: Development/Libraries
%description -n python-perf
This package provides a module that permits applications written in the
Python programming language to use the interface to manipulate perf events.

%{!?python_sitearch: %global python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}
%endif

%if %{with_tools}
%package -n %{name}-tools
Summary: Assortment of tools for the kernel.
Group: Development/System
License: GPLv2
Provides:  cpupowerutils = 1:009-0.6.p1
Obsoletes: cpupowerutils < 1:009-0.6.p1
Provides:  cpufreq-utils = 1:009-0.6.p1
Obsoletes: cpufreq-utils < 1:009-0.6.p1
Provides:  cpufrequtils = 1:009-0.6.p1
Obsoletes: cpufrequtils < 1:009-0.6.p1
Obsoletes: cpuspeed < 1:2.0
Requires: %{name}-tools-libs = %{version}-%{release}
Conflicts: kernel-tools < %{version}-%{release}
%description -n %{name}-tools
This package contains the tools/ directory and its supporting
documentation, derived from the kernel source.

%package -n %{name}-tools-libs
Summary: Libraries for the kernel tools.
Group: Development/System
License: GPLv2
Conflicts: kernel-tools-libs < %{version}-%{release}
%description -n %{name}-tools-libs
This package contains the libraries built from the
tools/ directory, derived from the kernel source.

%package -n %{name}-tools-libs-devel
Summary: Development package for the kernel tools libraries.
Group: Development/System
License: GPLv2
Requires: %{name}-tools = %{version}-%{release}
Requires: %{name}-tools-libs = %{version}-%{release}
Provides:  cpupowerutils-devel = 1:009-0.6.p1
Obsoletes: cpupowerutils-devel < 1:009-0.6.p1
Provides: %{name}-tools-devel
Conflicts: kernel-tools-libs-devel < %{version}-%{release}
%description -n %{name}-tools-libs-devel
This package contains the development files for the tools/ directory
libraries, derived from the kernel source.
%endif

# Disable the building of the debug package(s).
%define debug_package %{nil}

%prep
%setup -q -n %{name}-%{version} -c
%{__mv} linux-%{LKAver} linux-%{version}-%{release}.%{_target_cpu}

pushd linux-%{version}-%{release}.%{_target_cpu} > /dev/null

%{__cp} %{SOURCE1} .

# Run make listnewconfig over all the configuration files.
%ifarch i686 || x86_64
for C in config-*-%{_target_cpu}*
do
    %{__cp} $C .config
    %{__make} -s ARCH=%{buildarch} listnewconfig | grep -E '^CONFIG_' > .newoptions || true
    if [ -s .newoptions ]; then
        cat .newoptions
    exit 1
    fi
    %{__rm} .newoptions
done
%endif

# Remove unnecessary files.
/usr/bin/find . -type f \( -name .gitignore -o -name .mailmap \) | xargs --no-run-if-empty %{__rm} -f

popd > /dev/null

%build
BuildKernel() {
    Flavour=$1

    %{__make} -s distclean

    # Select the correct flavour configuration file.
    if [ -z "${Flavour}" ]; then
        %{__cp} config-%{version}-%{_target_cpu} .config
    else
        %{__cp} config-%{version}-%{_target_cpu}-${Flavour} .config
    fi

    %define KVRFA %{version}-%{release}${Flavour}.%{_target_cpu}

    # Set the EXTRAVERSION string in the main Makefile.
    %{__perl} -p -i -e "s/^EXTRAVERSION.*/EXTRAVERSION = -%{release}${Flavour}.%{_target_cpu}/" Makefile

    %{__make} -s ARCH=%{buildarch} %{?_smp_mflags} bzImage
    %{__make} -s ARCH=%{buildarch} %{?_smp_mflags} modules

    # Install the results into the RPM_BUILD_ROOT directory.
    %{__mkdir_p} $RPM_BUILD_ROOT/boot
    %{__install} -m 644 .config $RPM_BUILD_ROOT/boot/config-%{KVRFA}
    %{__install} -m 644 System.map $RPM_BUILD_ROOT/boot/System.map-%{KVRFA}

    # We estimate the size of the initramfs because rpm needs to take this size
    # into consideration when performing disk space calculations. (See bz #530778)
    dd if=/dev/zero of=$RPM_BUILD_ROOT/boot/initramfs-%{KVRFA}.img bs=1M count=20

    %{__cp} arch/x86/boot/bzImage $RPM_BUILD_ROOT/boot/vmlinuz-%{KVRFA}
    %{__chmod} 755 $RPM_BUILD_ROOT/boot/vmlinuz-%{KVRFA}

    %{__mkdir_p} $RPM_BUILD_ROOT/lib/modules/%{KVRFA}
    %{__mkdir_p} $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/kernel
    # Override $(mod-fw) because we don't want it to install any firmware.
    # We'll get it from the linux-firmware package and we don't want conflicts.
    %{__make} -s ARCH=%{buildarch} INSTALL_MOD_PATH=$RPM_BUILD_ROOT KERNELRELEASE=%{KVRFA} modules_install mod-fw=

%ifarch %{vdso_arches}
    %{__make} -s ARCH=%{buildarch} INSTALL_MOD_PATH=$RPM_BUILD_ROOT KERNELRELEASE=%{KVRFA} vdso_install
    /usr/bin/find $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/vdso -type f -name 'vdso*.so' | xargs --no-run-if-empty %{__strip}
    if grep '^CONFIG_XEN=y$' .config > /dev/null; then
        echo > ldconfig-%{name}.conf "\
# This directive teaches ldconfig to search in nosegneg subdirectories
# and cache the DSOs there with extra bit 1 set in their hwcap match
# fields.  In Xen guest kernels, the vDSO tells the dynamic linker to
# search in nosegneg subdirectories and to match this extra hwcap bit
# in the ld.so.cache file.
hwcap 1 nosegneg"
    fi
    if [ ! -s ldconfig-%{name}.conf ]; then
        echo > ldconfig-%{name}.conf "\
# Placeholder file, no vDSO hwcap entries used in this kernel."
    fi
    %{__install} -D -m 444 ldconfig-%{name}.conf $RPM_BUILD_ROOT/etc/ld.so.conf.d/%{name}-%{KVRFA}.conf
%endif

    # Save the headers/makefiles, etc, for building modules against.
    #
    # This looks scary but the end result is supposed to be:
    #
    # - all arch relevant include/ files
    # - all Makefile & Kconfig files
    # - all script/ files
    #
    %{__rm} -f $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build
    %{__rm} -f $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/source
    %{__mkdir_p} $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build
    pushd $RPM_BUILD_ROOT/lib/modules/%{KVRFA} > /dev/null
    %{__ln_s} build source
    popd > /dev/null
    %{__mkdir_p} $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/extra
    %{__mkdir_p} $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/updates
    %{__mkdir_p} $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/weak-updates

    # First copy everything . . .
    %{__cp} --parents `/usr/bin/find  -type f -name 'Makefile*' -o -name 'Kconfig*'` $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build
    %{__cp} Module.symvers $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build
    %{__cp} System.map $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build
    if [ -s Module.markers ]; then
        %{__cp} Module.markers $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build
    fi

    %{__gzip} -c9 < Module.symvers > $RPM_BUILD_ROOT/boot/symvers-%{KVRFA}.gz

    # . . . then drop all but the needed Makefiles & Kconfig files.
    %{__rm} -rf $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/Documentation
    %{__rm} -rf $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/scripts
    %{__rm} -rf $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/include
    %{__cp} .config $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build
    %{__cp} -a scripts $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build
    if [ -d arch/%{buildarch}/scripts ]; then
        %{__cp} -a arch/%{buildarch}/scripts $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/arch/%{_arch} || :
    fi
    if [ -f arch/%{buildarch}/*lds ]; then
        %{__cp} -a arch/%{buildarch}/*lds $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/arch/%{_arch}/ || :
    fi
    %{__rm} -f $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/scripts/*.o
    %{__rm} -f $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/scripts/*/*.o
    if [ -d arch/%{asmarch}/include ]; then
        %{__cp} -a --parents arch/%{asmarch}/include $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/
    fi
    if [ -d arch/%{asmarch}/syscalls ]; then
        %{__cp} -a --parents arch/%{asmarch}/syscalls $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/
    fi
    %{__mkdir_p} $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/include
    pushd include > /dev/null
    %{__cp} -a * $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/include/
    popd > /dev/null
    %{__rm} -f $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/include/Kbuild
    # Ensure a copy of the version.h file is in the include/linux/ directory.
    %{__cp} usr/include/linux/version.h $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/include/linux/
    # Copy the generated autoconf.h file to the include/linux/ directory.
    %{__cp} include/generated/autoconf.h $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/include/linux/
    # Copy .config to include/config/auto.conf so a "make prepare" is unnecessary.
    %{__cp} $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/.config $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/include/config/auto.conf
    # Now ensure that the Makefile, .config, auto.conf, autoconf.h and version.h files
    # all have matching timestamps so that external modules can be built.
    touch -r $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/Makefile $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/.config
    touch -r $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/Makefile $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/include/config/auto.conf
    touch -r $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/Makefile $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/include/linux/autoconf.h
    touch -r $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/Makefile $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/include/linux/version.h
    touch -r $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/Makefile $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/include/generated/autoconf.h
    touch -r $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/Makefile $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build/include/generated/uapi/linux/version.h

    # Remove any 'left-over' .cmd files.
    /usr/bin/find $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build -type f -name '*.cmd' | xargs --no-run-if-empty %{__rm} -f

    /usr/bin/find $RPM_BUILD_ROOT/lib/modules/%{KVRFA} -type f -name '*.ko' > modnames

    # Mark the modules executable, so that strip-to-file can strip them.
    xargs --no-run-if-empty %{__chmod} u+x < modnames

    # Generate a list of modules for block and networking.
    grep -F /drivers/ modnames | xargs --no-run-if-empty nm -upA | sed -n 's,^.*/\([^/]*\.ko\):  *U \(.*\)$,\1 \2,p' > drivers.undef

    collect_modules_list()
    {
        sed -r -n -e "s/^([^ ]+) \\.?($2)\$/\\1/p" drivers.undef | LC_ALL=C sort -u > $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/modules.$1
        if [ ! -z "$3" ]; then
            sed -r -e "/^($3)\$/d" -i $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/modules.$1
        fi
    }

    collect_modules_list networking \
        'register_netdev|ieee80211_register_hw|usbnet_probe|phy_driver_register|rt2x00(pci|usb)_probe|register_netdevice'

    collect_modules_list block \
        'ata_scsi_ioctl|scsi_add_host|scsi_add_host_with_dma|blk_alloc_queue|blk_init_queue|register_mtd_blktrans|scsi_esp_register|scsi_register_device_handler|blk_queue_physical_block_size|pktcdvd.ko|dm-mod.ko'

    collect_modules_list drm \
        'drm_open|drm_init'

    collect_modules_list modesetting \
        'drm_crtc_init'

    # Detect any missing or incorrect license tags.
    %{__rm} -f modinfo

    while read i
    do
        echo -n "${i#$RPM_BUILD_ROOT/lib/modules/%{KVRFA}/} " >> modinfo
        %{_sbindir}/modinfo -l $i >> modinfo
    done < modnames

    grep -E -v 'GPL( v2)?$|Dual BSD/GPL$|Dual MPL/GPL$|GPL and additional rights$' modinfo && exit 1

    %{__rm} -f modinfo modnames

    # Remove all the files that will be auto generated by depmod at the kernel install time.
    for i in alias alias.bin builtin.bin ccwmap dep dep.bin devname ieee1394map inputmap isapnpmap ofmap pcimap seriomap softdep symbols symbols.bin usbmap
    do
        %{__rm} -f $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/modules.$i
    done

    # Move the development files out of the /lib/modules/ file system.
    %{__mkdir_p} $RPM_BUILD_ROOT/usr/src/kernels
    %{__mv} $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build $RPM_BUILD_ROOT/usr/src/kernels/%{KVRFA}
    %{__ln_s} -f /usr/src/kernels/%{KVRFA} $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build
}

# Prepare the directories.
%{__rm} -rf $RPM_BUILD_ROOT
%{__mkdir_p} $RPM_BUILD_ROOT/boot
%{__mkdir_p} $RPM_BUILD_ROOT%{_libexecdir}

pushd linux-%{version}-%{release}.%{_target_cpu} > /dev/null

%if %{with_default}
BuildKernel
%endif

%if %{with_perf}
%global perf_make \
    %{__make} -s -C tools/perf %{?_smp_mflags} prefix=%{_prefix} lib=%{_lib} WERROR=0 HAVE_CPLUS_DEMANGLE=1 NO_GTK2=1 NO_LIBUNWIND=1 NO_PERF_READ_VDSO32=1 NO_PERF_READ_VDSOX32=1 NO_STRLCPY=1

%{perf_make} all
%{perf_make} man
%endif

%if %{with_tools}
%ifarch x86_64
# Make sure that version-gen.sh is executable.
%{__chmod} +x tools/power/cpupower/utils/version-gen.sh
pushd tools/power/cpupower > /dev/null
%{__make} -s CPUFREQ_BENCH=false
popd > /dev/null
pushd tools/power/cpupower/debug/x86_64 > /dev/null
%{__make} -s centrino-decode
%{__make} -s powernow-k8-decode
popd > /dev/null
pushd tools/power/x86/x86_energy_perf_policy > /dev/null
%{__make} -s
popd > /dev/null
pushd tools/power/x86/turbostat > /dev/null
%{__make} -s
popd > /dev/null
%endif
pushd tools/thermal/tmon > /dev/null
%{__make} -s
popd > /dev/null
%endif

popd > /dev/null

%install
pushd linux-%{version}-%{release}.%{_target_cpu} > /dev/null

%if %{with_headers}
# We have to do the headers install before the tools install because the
# kernel headers_install will remove any header files in /usr/include that
# it doesn't install itself.

# Install kernel headers
%{__make} -s ARCH=%{hdrarch} INSTALL_HDR_PATH=$RPM_BUILD_ROOT/usr headers_install

# Do headers_check but don't die if it fails.
%{__make} -s ARCH=%{hdrarch} INSTALL_HDR_PATH=$RPM_BUILD_ROOT/usr headers_check > hdrwarnings.txt || :
if grep -q exist hdrwarnings.txt; then
    sed s:^$RPM_BUILD_ROOT/usr/include/:: hdrwarnings.txt
    # Temporarily cause a build failure if header inconsistencies.
    # exit 1
fi

# Remove the unrequired files.
/usr/bin/find $RPM_BUILD_ROOT/usr/include -type f \
    \( -name .install -o -name .check -o -name ..install.cmd -o -name ..check.cmd \) | \
    xargs --no-run-if-empty %{__rm} -f
%endif

%if %{with_doc}
DOCDIR=$RPM_BUILD_ROOT%{_datadir}/doc/%{name}-doc-%{version}

# Sometimes non-world-readable files sneak into the kernel source tree.
%{__chmod} -Rf a+rX,u+w,go-w Documentation

# Copy the documentation over.
%{__mkdir_p} $DOCDIR
%{__tar} -f - --exclude=man --exclude='.*' -c Documentation | %{__tar} xf - -C $DOCDIR
%endif

%if %{with_perf}
# perf tool binary and supporting scripts/binaries.
%{perf_make} DESTDIR=$RPM_BUILD_ROOT install

# perf-python extension.
%{perf_make} DESTDIR=$RPM_BUILD_ROOT install-python_ext

# perf man pages. (Note: implicit rpm magic compresses them later.)
%{perf_make} DESTDIR=$RPM_BUILD_ROOT try-install-man
%endif

%if %{with_tools}
%ifarch x86_64
%{__make} -s -C tools/power/cpupower DESTDIR=$RPM_BUILD_ROOT libdir=%{_libdir} mandir=%{_mandir} CPUFREQ_BENCH=false install
%{__rm} -f %{buildroot}%{_libdir}/*.{a,la}
%find_lang cpupower
mv cpupower.lang ../
pushd tools/power/cpupower/debug/x86_64 > /dev/null
%{__install} -m755 centrino-decode %{buildroot}%{_bindir}/centrino-decode
%{__install} -m755 powernow-k8-decode %{buildroot}%{_bindir}/powernow-k8-decode
popd > /dev/null
%{__chmod} 0755 %{buildroot}%{_libdir}/libcpupower.so*
%{__mkdir_p} %{buildroot}%{_unitdir}
%{__install} -m644 %{SOURCE2} %{buildroot}%{_unitdir}/cpupower.service
%{__mkdir_p} %{buildroot}%{_sysconfdir}/sysconfig
%{__install} -m644 %{SOURCE3} %{buildroot}%{_sysconfdir}/sysconfig/cpupower
%{__mkdir_p} %{buildroot}%{_mandir}/man8
pushd tools/power/x86/x86_energy_perf_policy > /dev/null
%{__make} -s DESTDIR=%{buildroot} install
popd > /dev/null
pushd tools/power/x86/turbostat > /dev/null
%{__make} -s DESTDIR=%{buildroot} install
popd > /dev/null
/usr/bin/find %{buildroot}%{_mandir} -type f -print0 \
    | xargs -0 --no-run-if-empty %{__chmod} 644
%endif
pushd tools/thermal/tmon > /dev/null
%{__install} -m755 tmon %{buildroot}%{_bindir}/tmon
popd > /dev/null
%endif

popd > /dev/null

%clean
%{__rm} -rf $RPM_BUILD_ROOT

# Scripts section.
%if %{with_default}
%posttrans
%{_sbindir}/new-kernel-pkg --package %{name} --mkinitrd --dracut --depmod --update %{version}-%{release}.%{_target_cpu} || exit $?
%{_sbindir}/new-kernel-pkg --package %{name} --rpmposttrans %{version}-%{release}.%{_target_cpu} || exit $?
if [ -x %{_sbindir}/weak-modules ]; then
    %{_sbindir}/weak-modules --add-kernel %{version}-%{release}.%{_target_cpu} || exit $?
fi

%post
%{_sbindir}/new-kernel-pkg --package %{name} --install %{version}-%{release}.%{_target_cpu} || exit $?

%preun
%{_sbindir}/new-kernel-pkg --rminitrd --rmmoddep --remove %{version}-%{release}.%{_target_cpu} || exit $?
if [ -x %{_sbindir}/weak-modules ]; then
    %{_sbindir}/weak-modules --remove-kernel %{version}-%{release}.%{_target_cpu} || exit $?
fi

%postun
for i in `ls /boot/initramfs*kdump.img 2>/dev/null`; do
    KDVER=`echo $i | sed -e's/^.*initramfs-//' -e's/kdump.*$//'`
    if [ ! -e /boot/vmlinuz-$KDVER ]; then
        %{__rm} -f $i
    fi
done

%post devel
if [ -f /etc/sysconfig/kernel ]; then
    . /etc/sysconfig/kernel || exit $?
fi
if [ "$HARDLINK" != "no" -a -x /usr/sbin/hardlink ]; then
    pushd /usr/src/kernels/%{version}-%{release}.%{_target_cpu} > /dev/null
    /usr/bin/find . -type f | while read f; do
        /usr/sbin/hardlink -c /usr/src/kernels/*.fc*.*/$f $f
    done
    popd > /dev/null
fi
%endif

%if %{with_tools}
%post -n %{name}-tools
%{_sbindir}/ldconfig || exit $?

%postun -n %{name}-tools
%{_sbindir}/ldconfig || exit $?
%endif

# Files section.
%if %{with_default}
%files
%defattr(-,root,root)
/boot/vmlinuz-%{version}-%{release}.%{_target_cpu}
%attr(600,root,root) /boot/System.map-%{version}-%{release}.%{_target_cpu}
/boot/symvers-%{version}-%{release}.%{_target_cpu}.gz
/boot/config-%{version}-%{release}.%{_target_cpu}
%dir /lib/modules/%{version}-%{release}.%{_target_cpu}
/lib/modules/%{version}-%{release}.%{_target_cpu}/kernel
/lib/modules/%{version}-%{release}.%{_target_cpu}/build
/lib/modules/%{version}-%{release}.%{_target_cpu}/source
/lib/modules/%{version}-%{release}.%{_target_cpu}/extra
/lib/modules/%{version}-%{release}.%{_target_cpu}/updates
/lib/modules/%{version}-%{release}.%{_target_cpu}/weak-updates
%ifarch %{vdso_arches}
/lib/modules/%{version}-%{release}.%{_target_cpu}/vdso
/etc/ld.so.conf.d/%{name}-%{version}-%{release}.%{_target_cpu}.conf
%endif
/lib/modules/%{version}-%{release}.%{_target_cpu}/modules.*
%ghost /boot/initramfs-%{version}-%{release}.%{_target_cpu}.img

%files devel
%defattr(-,root,root)
%dir /usr/src/kernels
/usr/src/kernels/%{version}-%{release}.%{_target_cpu}
%endif

%if %{with_headers}
%files headers
%defattr(-,root,root)
%{_includedir}/*
%endif

%if %{with_doc}
%files doc
%defattr(-,root,root)
%{_datadir}/doc/%{name}-doc-%{version}/Documentation/*
%dir %{_datadir}/doc/%{name}-doc-%{version}/Documentation
%dir %{_datadir}/doc/%{name}-doc-%{version}
%endif

%if %{with_perf}
%files -n perf
%defattr(-,root,root)
%{_bindir}/perf
%{_bindir}/trace
%{_libdir}/libperf-jvmti.so
%dir %{_libdir}/traceevent/plugins
%{_libdir}/traceevent/plugins/*
%dir %{_libexecdir}/perf-core
%{_libexecdir}/perf-core/*
%{_mandir}/man[1-8]/perf*
%config(noreplace) %{_sysconfdir}/bash_completion.d/perf
%dir %{_datadir}/perf-core/strace/groups
%{_datadir}/perf-core/strace/groups/*
%dir %{_datadir}/doc/perf-tip
%{_datadir}/doc/perf-tip/*
%doc linux-%{version}-%{release}.%{_target_cpu}/tools/perf/Documentation/examples.txt

%files -n python-perf
%defattr(-,root,root)
%{python_sitearch}
%endif

%if %{with_tools}
%files -n %{name}-tools -f cpupower.lang
%defattr(-,root,root)

%ifarch x86_64
%{_bindir}/cpupower
%{_bindir}/centrino-decode
%{_bindir}/powernow-k8-decode
%{_bindir}/x86_energy_perf_policy
%{_bindir}/turbostat
%{_bindir}/tmon
%config(noreplace) %{_sysconfdir}/sysconfig/cpupower
%{_unitdir}/cpupower.service
%{_mandir}/man[1-8]/cpupower*
%{_mandir}/man8/x86_energy_perf_policy*
%{_mandir}/man8/turbostat*

%files -n %{name}-tools-libs
%defattr(-,root,root)
%{_libdir}/libcpupower.so.0
%{_libdir}/libcpupower.so.0.0.1

%files -n %{name}-tools-libs-devel
%defattr(-,root,root)
%{_libdir}/libcpupower.so
%{_includedir}/cpufreq.h
%{_includedir}/cpuidle.h
%endif
%endif

%changelog
* Wed Jan 10 2018 Alan Bartlett <ajb@elrepo.org> - 4.14.13-1
- Updated with the 4.14.13 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.13]

* Fri Jan 05 2018 Alan Bartlett <ajb@elrepo.org> - 4.14.12-1
- Updated with the 4.14.12 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.12]

* Tue Jan 02 2018 Alan Bartlett <ajb@elrepo.org> - 4.14.11-1
- Updated with the 4.14.11 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.11]
- CONFIG_PAGE_TABLE_ISOLATION=y

* Sat Dec 30 2017 Alan Bartlett <ajb@elrepo.org> - 4.14.10-1
- Updated with the 4.14.10 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.10]

* Mon Dec 25 2017 Alan Bartlett <ajb@elrepo.org> - 4.14.9-1
- Updated with the 4.14.9 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.9]

* Wed Dec 20 2017 Alan Bartlett <ajb@elrepo.org> - 4.14.8-1
- Updated with the 4.14.8 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.8]

* Sun Dec 17 2017 Alan Bartlett <ajb@elrepo.org> - 4.14.7-1
- Updated with the 4.14.7 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.7]
- CONFIG_XEN_PVCALLS_BACKEND=y
- CONFIG_TLS=m
- [https://elrepo.org/bugs/view.php?id=806]

* Thu Dec 14 2017 Alan Bartlett <ajb@elrepo.org> - 4.14.6-1
- Updated with the 4.14.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.6]
- CONFIG_PINCTRL_AMD=m, CONFIG_PINCTRL_MCP23S08=m,
- CONFIG_PINCTRL_CHERRYVIEW=m, CONFIG_PINCTRL_INTEL=m,
- CONFIG_PINCTRL_BROXTON=m, CONFIG_PINCTRL_CANNONLAKE=m,
- CONFIG_PINCTRL_DENVERTON=m, CONFIG_PINCTRL_GEMINILAKE=m,
- CONFIG_PINCTRL_LEWISBURG=m and CONFIG_PINCTRL_SUNRISEPOINT=m
- [https://elrepo.org/bugs/view.php?id=804]
- CONFIG_DRM_AMDGPU_SI=Y and CONFIG_DRM_AMDGPU_CIK=Y
- [https://elrepo.org/bugs/view.php?id=805]

* Sun Dec 10 2017 Alan Bartlett <ajb@elrepo.org> - 4.14.5-1
- Updated with the 4.14.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.5]
- Adjusted the list of files to be removed, as they will be
- created by depmod at the package installation time.
- [https://elrepo.org/bugs/view.php?id=803]

* Tue Dec 05 2017 Alan Bartlett <ajb@elrepo.org> - 4.14.4-1
- Updated with the 4.14.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.4]

* Thu Nov 30 2017 Alan Bartlett <ajb@elrepo.org> - 4.14.3-1
- Updated with the 4.14.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.3]

* Fri Nov 24 2017 Alan Bartlett <ajb@elrepo.org> - 4.14.2-1
- Updated with the 4.14.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.2]

* Tue Nov 21 2017 Alan Bartlett <ajb@elrepo.org> - 4.14.1-1
- Updated with the 4.14.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.14.1]

* Mon Nov 13 2017 Alan Bartlett <ajb@elrepo.org> - 4.14.0-1
- Updated with the 4.14 source tarball.

* Wed Nov 08 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.12-1
- Updated with the 4.13.12 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.13.12]

* Thu Nov 02 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.11-1
- Updated with the 4.13.11 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.13.11]

* Fri Oct 27 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.10-1
- Updated with the 4.13.10 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.13.10]

* Sun Oct 22 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.9-1
- Updated with the 4.13.9 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.13.9]

* Wed Oct 18 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.8-1
- Updated with the 4.13.8 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.13.8]

* Sat Oct 14 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.7-1
- Updated with the 4.13.7 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.13.7]

* Thu Oct 12 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.6-1
- Updated with the 4.13.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.13.6]

* Thu Oct 05 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.5-1
- Updated with the 4.13.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.13.5]

* Wed Sep 27 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.4-1
- Updated with the 4.13.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.13.4]

* Wed Sep 20 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.3-1
- Updated with the 4.13.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.13.3]

* Wed Sep 13 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.2-1
- Updated with the 4.13.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.13.2]

* Sun Sep 10 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.1-1
- Updated with the 4.13.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.13.1]

* Sun Sep 03 2017 Alan Bartlett <ajb@elrepo.org> - 4.13.0-1
- Updated with the 4.13 source tarball.

* Wed Aug 30 2017 Alan Bartlett <ajb@elrepo.org> - 4.12.10-1
- Updated with the 4.12.10 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.12.10]

* Fri Aug 25 2017 Alan Bartlett <ajb@elrepo.org> - 4.12.9-1
- Updated with the 4.12.9 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.12.9]

* Thu Aug 17 2017 Alan Bartlett <ajb@elrepo.org> - 4.12.8-1
- Updated with the 4.12.8 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.12.8]

* Mon Aug 14 2017 Alan Bartlett <ajb@elrepo.org> - 4.12.7-1
- Updated with the 4.12.7 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.12.7]
- CONFIG_MOUSE_PS2_VMMOUSE=y [https://elrepo.org/bugs/view.php?id=767]

* Fri Aug 11 2017 Alan Bartlett <ajb@elrepo.org> - 4.12.6-1
- Updated with the 4.12.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.12.6]

* Sun Aug 06 2017 Alan Bartlett <ajb@elrepo.org> - 4.12.5-1
- Updated with the 4.12.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.12.5]

* Fri Jul 28 2017 Alan Bartlett <ajb@elrepo.org> - 4.12.4-1
- Updated with the 4.12.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.12.4]

* Fri Jul 21 2017 Alan Bartlett <ajb@elrepo.org> - 4.12.3-1
- Updated with the 4.12.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.12.3]

* Sat Jul 15 2017 Alan Bartlett <ajb@elrepo.org> - 4.12.2-1
- Updated with the 4.12.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.12.2]

* Thu Jul 13 2017 Alan Bartlett <ajb@elrepo.org> - 4.12.1-1
- Updated with the 4.12.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.12.1]

* Mon Jul 03 2017 Alan Bartlett <ajb@elrepo.org> - 4.12.0-1
- Updated with the 4.12 source tarball.

* Thu Jun 29 2017 Alan Bartlett <ajb@elrepo.org> - 4.11.8-1
- Updated with the 4.11.8 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.11.8]

* Sat Jun 24 2017 Alan Bartlett <ajb@elrepo.org> - 4.11.7-1
- Updated with the 4.11.7 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.11.7]

* Sat Jun 17 2017 Alan Bartlett <ajb@elrepo.org> - 4.11.6-1
- Updated with the 4.11.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.11.6]

* Wed Jun 14 2017 Alan Bartlett <ajb@elrepo.org> - 4.11.5-1
- Updated with the 4.11.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.11.5]

* Wed Jun 07 2017 Alan Bartlett <ajb@elrepo.org> - 4.11.4-1
- Updated with the 4.11.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.11.4]
- CONFIG_HAMRADIO=y, CONFIG_AX25=m, CONFIG_AX25_DAMA_SLAVE=y,
- CONFIG_NETROM=m, CONFIG_ROSE=m, CONFIG_MKISS=m,
- CONFIG_6PACK=m, CONFIG_BPQETHER=m, CONFIG_BAYCOM_SER_FDX=m,
- CONFIG_BAYCOM_SER_HDX=m, CONFIG_BAYCOM_PAR=m and CONFIG_YAM=m
- [https://elrepo.org/bugs/view.php?id=745]

* Fri May 26 2017 Alan Bartlett <ajb@elrepo.org> - 4.11.3-1
- Updated with the 4.11.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.11.3]

* Sun May 21 2017 Alan Bartlett <ajb@elrepo.org> - 4.11.2-1
- Updated with the 4.11.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.11.2]

* Sun May 14 2017 Alan Bartlett <ajb@elrepo.org> - 4.11.1-1
- Updated with the 4.11.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.11.1]
- CONFIG_DETECT_HUNG_TASK=y, CONFIG_DEFAULT_HUNG_TASK_TIMEOUT=120
- and CONFIG_BOOTPARAM_HUNG_TASK_PANIC_VALUE=0
- [https://elrepo.org/bugs/view.php?id=733]

* Mon May 01 2017 Alan Bartlett <ajb@elrepo.org> - 4.11.0-1
- Updated with the 4.11 source tarball.

* Thu Apr 27 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.13-1
- Updated with the 4.10.13 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.13]
- CONFIG_MLX5_CORE_EN=y and CONFIG_MLX5_CORE_EN_DCB=y
- [https://elrepo.org/bugs/view.php?id=730]

* Fri Apr 21 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.12-1
- Updated with the 4.10.12 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.12]

* Tue Apr 18 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.11-1
- Updated with the 4.10.11 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.11]
- CONFIG_USERFAULTFD=y
- [https://lists.elrepo.org/pipermail/elrepo/2017-April/003540.html]

* Wed Apr 12 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.10-1
- Updated with the 4.10.10 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.10]

* Sat Apr 08 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.9-1
- Updated with the 4.10.9 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.9]

* Fri Mar 31 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.8-1
- Updated with the 4.10.8 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.8]

* Thu Mar 30 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.7-1
- Updated with the 4.10.7 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.7]

* Sun Mar 26 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.6-1
- Updated with the 4.10.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.6]

* Wed Mar 22 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.5-1
- Updated with the 4.10.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.5]

* Sat Mar 18 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.4-1
- Updated with the 4.10.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.4]

* Wed Mar 15 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.3-1
- Updated with the 4.10.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.3]

* Sun Mar 12 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.2-1
- Updated with the 4.10.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.2]

* Sun Feb 26 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.1-1
- Updated with the 4.10.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.10.1]
- Added NO_PERF_READ_VDSO32=1 and NO_PERF_READ_VDSOX32=1
- directives to the %%global perf_make line.
- [https://elrepo.org/bugs/view.php?id=719]

* Sun Feb 19 2017 Alan Bartlett <ajb@elrepo.org> - 4.10.0-1
- Updated with the 4.10 source tarball.
- CONFIG_NVME_FC=m and CONFIG_NVME_TARGET_FC=m
- [https://elrepo.org/bugs/view.php?id=705]

* Sat Feb 18 2017 Alan Bartlett <ajb@elrepo.org> - 4.9.11-1
- Updated with the 4.9.11 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.9.11]
- CONFIG_MODVERSIONS=y [https://elrepo.org/bugs/view.php?id=718]

* Wed Feb 15 2017 Alan Bartlett <ajb@elrepo.org> - 4.9.10-1
- Updated with the 4.9.10 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.9.10]
- CONFIG_CPU_FREQ_STAT=y and CONFIG_CPU_FREQ_STAT_DETAILS=y
- [https://elrepo.org/bugs/view.php?id=717]

* Thu Feb 09 2017 Alan Bartlett <ajb@elrepo.org> - 4.9.9-1
- Updated with the 4.9.9 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.9.9]

* Sat Feb 04 2017 Alan Bartlett <ajb@elrepo.org> - 4.9.8-1
- Updated with the 4.9.8 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.9.8]

* Wed Feb 01 2017 Alan Bartlett <ajb@elrepo.org> - 4.9.7-1
- Updated with the 4.9.7 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.9.7]

* Thu Jan 26 2017 Alan Bartlett <ajb@elrepo.org> - 4.9.6-1
- Updated with the 4.9.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.9.6]
- Remove any orphaned initramfs-xxxkdump.img file
- found post kernel uninstall. [Akemi Yagi]

* Fri Jan 20 2017 Alan Bartlett <ajb@elrepo.org> - 4.9.5-1
- Updated with the 4.9.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.9.5]

* Mon Jan 16 2017 Alan Bartlett <ajb@elrepo.org> - 4.9.4-1
- Updated with the 4.9.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.9.4]

* Fri Jan 13 2017 Alan Bartlett <ajb@elrepo.org> - 4.9.3-1
- Updated with the 4.9.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.9.3]

* Tue Jan 10 2017 Alan Bartlett <ajb@elrepo.org> - 4.9.2-1
- Updated with the 4.9.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.9.2]
- CONFIG_CAN=m, CONFIG_CAN_RAW=m, CONFIG_CAN_BCM=m, CONFIG_CAN_GW=m,
- CONFIG_CAN_VCAN=m, CONFIG_CAN_SLCAN=m, CONFIG_CAN_DEV=m,
- CONFIG_CAN_CALC_BITTIMING=y, CONFIG_CAN_LEDS=y, CONFIG_CAN_JANZ_ICAN3=m,
- CONFIG_CAN_C_CAN=m, CONFIG_CAN_C_CAN_PLATFORM=m, CONFIG_CAN_C_CAN_PCI=m,
- CONFIG_CAN_CC770=m, CONFIG_CAN_CC770_ISA=m, CONFIG_CAN_CC770_PLATFORM=m,
- CONFIG_CAN_IFI_CANFD=m, CONFIG_CAN_M_CAN=m, CONFIG_CAN_SJA1000=m,
- CONFIG_CAN_SJA1000_ISA=m, CONFIG_CAN_SJA1000_PLATFORM=m,
- CONFIG_CAN_EMS_PCMCIA=m, CONFIG_CAN_EMS_PCI=m, CONFIG_CAN_PEAK_PCMCIA=m,
- CONFIG_CAN_PEAK_PCI=m, CONFIG_CAN_PEAK_PCIEC=y, CONFIG_CAN_KVASER_PCI=m,
- CONFIG_CAN_PLX_PCI=m, CONFIG_CAN_SOFTING=m, CONFIG_CAN_SOFTING_CS=m,
- CONFIG_CAN_MCP251X=m, CONFIG_CAN_EMS_USB=m, CONFIG_CAN_ESD_USB2=m,
- CONFIG_CAN_GS_USB=m, CONFIG_CAN_KVASER_USB=m, CONFIG_CAN_PEAK_USB=m
- and CONFIG_CAN_8DEV_USB=m [https://elrepo.org/bugs/view.php?id=707]

* Fri Jan 06 2017 Alan Bartlett <ajb@elrepo.org> - 4.9.1-1
- Updated with the 4.9.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.9.1]
- CONFIG_NVME_CORE=m, CONFIG_NVME_FABRICS=m, CONFIG_NVME_RDMA=m,
- CONFIG_NVME_TARGET=m, CONFIG_NVME_TARGET_LOOP=m and
- CONFIG_NVME_TARGET_RDMA=m [https://elrepo.org/bugs/view.php?id=705]

* Mon Dec 12 2016 Alan Bartlett <ajb@elrepo.org> - 4.9.0-1
- Updated with the 4.9 source tarball.

* Fri Dec 09 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.13-1
- Updated with the 4.8.13 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.8.13]

* Fri Dec 02 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.12-1
- Updated with the 4.8.12 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.8.12]

* Sat Nov 26 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.11-1
- Updated with the 4.8.11 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.8.11]

* Mon Nov 21 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.10-1
- Updated with the 4.8.10 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.8.10]

* Sat Nov 19 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.9-1
- Updated with the 4.8.9 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.8.9]
- CONFIG_BPF_SYSCALL=y and CONFIG_BPF_EVENTS=y
- [https://elrepo.org/bugs/view.php?id=690]

* Thu Nov 10 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.7-1
- Updated with the 4.8.7 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.8.7]
- CONFIG_ORANGEFS_FS=m [https://elrepo.org/bugs/view.php?id=677]
- CONFIG_FMC=m, CONFIG_FMC_CHARDEV=m and CONFIG_FMC_WRITE_EEPROM=m
- [https://elrepo.org/bugs/view.php?id=680]

* Mon Oct 31 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.6-1
- Updated with the 4.8.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.8.6]

* Fri Oct 28 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.5-1
- Updated with the 4.8.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.8.5]

* Sat Oct 22 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.4-1
- Updated with the 4.8.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.8.4]

* Thu Oct 20 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.3-1
- Updated with the 4.8.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.8.3]

* Mon Oct 17 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.2-1
- Updated with the 4.8.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.8.2]

* Fri Oct 07 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.1-1
- Updated with the 4.8.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.8.1]

* Mon Oct 03 2016 Alan Bartlett <ajb@elrepo.org> - 4.8.0-1
- Updated with the 4.8 source tarball.

* Fri Sep 30 2016 Alan Bartlett <ajb@elrepo.org> - 4.7.6-1
- Updated with the 4.7.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.7.6]

* Sat Sep 24 2016 Alan Bartlett <ajb@elrepo.org> - 4.7.5-1
- Updated with the 4.7.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.7.5]

* Thu Sep 15 2016 Alan Bartlett <ajb@elrepo.org> - 4.7.4-1
- Updated with the 4.7.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.7.4]

* Wed Sep 07 2016 Alan Bartlett <ajb@elrepo.org> - 4.7.3-1
- Updated with the 4.7.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.7.3]
- Disabled CONFIG_FW_LOADER_USER_HELPER_FALLBACK
- [https://elrepo.org/bugs/view.php?id=671]

* Sat Aug 20 2016 Alan Bartlett <ajb@elrepo.org> - 4.7.2-1
- Updated with the 4.7.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.7.2]

* Wed Aug 17 2016 Alan Bartlett <ajb@elrepo.org> - 4.7.1-1
- Updated with the 4.7.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.7.1]

* Sun Jul 24 2016 Alan Bartlett <ajb@elrepo.org> - 4.7.0-1
- Updated with the 4.7 source tarball.

* Tue Jul 12 2016 Alan Bartlett <ajb@elrepo.org> - 4.6.4-1
- Updated with the 4.6.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.6.4]

* Sat Jun 25 2016 Alan Bartlett <ajb@elrepo.org> - 4.6.3-1
- Updated with the 4.6.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.6.3]

* Wed Jun 08 2016 Alan Bartlett <ajb@elrepo.org> - 4.6.2-1
- Updated with the 4.6.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.6.2]

* Thu Jun 02 2016 Alan Bartlett <ajb@elrepo.org> - 4.6.1-1
- Updated with the 4.6.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.6.1]

* Mon May 16 2016 Alan Bartlett <ajb@elrepo.org> - 4.6.0-1
- Updated with the 4.6 source tarball.

* Thu May 12 2016 Alan Bartlett <ajb@elrepo.org> - 4.5.4-1
- Updated with the 4.5.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.5.4]

* Thu May 05 2016 Alan Bartlett <ajb@elrepo.org> - 4.5.3-1
- Updated with the 4.5.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.5.3]

* Wed Apr 20 2016 Alan Bartlett <ajb@elrepo.org> - 4.5.2-1
- Updated with the 4.5.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.5.2]

* Sat Apr 16 2016 Alan Bartlett <ajb@elrepo.org> - 4.5.1-1
- Updated with the 4.5.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.5.1]

* Mon Mar 14 2016 Alan Bartlett <ajb@elrepo.org> - 4.5.0-1
- Updated with the 4.5 source tarball.

* Thu Mar 10 2016 Alan Bartlett <ajb@elrepo.org> - 4.4.5-1
- Updated with the 4.4.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.5]

* Fri Mar 04 2016 Alan Bartlett <ajb@elrepo.org> - 4.4.4-1
- Updated with the 4.4.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.4]

* Thu Feb 25 2016 Alan Bartlett <ajb@elrepo.org> - 4.4.3-1
- Updated with the 4.4.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.3]

* Thu Feb 18 2016 Alan Bartlett <ajb@elrepo.org> - 4.4.2-1
- Updated with the 4.4.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.2]

* Sun Jan 31 2016 Alan Bartlett <ajb@elrepo.org> - 4.4.1-1
- Updated with the 4.4.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.1]

* Tue Jan 26 2016 Alan Bartlett <ajb@elrepo.org> - 4.4.0-2
- CONFIG_SCSI_MPT2SAS=m [https://elrepo.org/bugs/view.php?id=628]

* Mon Jan 11 2016 Alan Bartlett <ajb@elrepo.org> - 4.4.0-1
- Updated with the 4.4 source tarball.

* Tue Dec 15 2015 Alan Bartlett <ajb@elrepo.org> - 4.3.3-1
- Updated with the 4.3.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.3.3]

* Thu Dec 10 2015 Alan Bartlett <ajb@elrepo.org> - 4.3.2-1
- Updated with the 4.3.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.3.2]

* Thu Dec 10 2015 Alan Bartlett <ajb@elrepo.org> - 4.3.1-1
- Updated with the 4.3.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.3.1]
- CONFIG_VXFS_FS=m [https://elrepo.org/bugs/view.php?id=606]

* Mon Nov 02 2015 Alan Bartlett <ajb@elrepo.org> - 4.3.0-1
- Updated with the 4.3 source tarball.

* Tue Oct 27 2015 Alan Bartlett <ajb@elrepo.org> - 4.2.5-1
- Updated with the 4.2.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.2.5]
- CONFIG_SCHEDSTATS=y [https://elrepo.org/bugs/view.php?id=603]

* Fri Oct 23 2015 Alan Bartlett <ajb@elrepo.org> - 4.2.4-1
- Updated with the 4.2.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.2.4]

* Sat Oct 03 2015 Alan Bartlett <ajb@elrepo.org> - 4.2.3-1
- Updated with the 4.2.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.2.3]

* Wed Sep 30 2015 Alan Bartlett <ajb@elrepo.org> - 4.2.2-1
- Updated with the 4.2.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.2.2]

* Mon Sep 21 2015 Alan Bartlett <ajb@elrepo.org> - 4.2.1-1
- Updated with the 4.2.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.2.1]
- CONFIG_BACKLIGHT_GPIO=m, CONFIG_BCMA_DRIVER_GPIO=y, CONFIG_CHARGER_GPIO=m,
- CONFIG_CHARGER_MANAGER=y, CONFIG_CLKDEV_LOOKUP=y, CONFIG_COMMON_CLK=y,
- CONFIG_EXTCON=y, CONFIG_GENERIC_IRQ_CHIP=y, CONFIG_GPIO_ACPI=y,
- CONFIG_GPIO_ADP5588=m, CONFIG_GPIO_AMD8111=m, CONFIG_GPIO_DEVRES=y,
- CONFIG_GPIO_DWAPB=m, CONFIG_GPIO_F7188X=m, CONFIG_GPIO_GENERIC=m,
- CONFIG_GPIO_GENERIC_PLATFORM=m, CONFIG_GPIO_ICH=m, CONFIG_GPIO_INTEL_MID=y,
- CONFIG_GPIO_IT8761E=m, CONFIG_GPIO_JANZ_TTL=m, CONFIG_GPIO_KEMPLD=m,
- CONFIG_GPIOLIB_IRQCHIP=y, CONFIG_GPIOLIB=y, CONFIG_GPIO_LP3943=m,
- CONFIG_GPIO_LYNXPOINT=m, CONFIG_GPIO_MAX7300=m, CONFIG_GPIO_MAX7301=m,
- CONFIG_GPIO_MAX730X=m, CONFIG_GPIO_MAX732X=m, CONFIG_GPIO_MC33880=m,
- CONFIG_GPIO_MCP23S08=m, CONFIG_GPIO_ML_IOH=m, CONFIG_GPIO_PCA953X=m,
- CONFIG_GPIO_PCF857X=m, CONFIG_GPIO_RDC321X=m, CONFIG_GPIO_SCH311X=m,
- CONFIG_GPIO_SCH=m, CONFIG_GPIO_SX150X=y, CONFIG_GPIO_SYSFS=y,
- CONFIG_GPIO_VX855=m, CONFIG_HAVE_CLK_PREPARE=y, CONFIG_HAVE_CLK=y,
- CONFIG_I2C_CBUS_GPIO=m, CONFIG_I2C_DESIGNWARE_PLATFORM=m, CONFIG_I2C_GPIO=m,
- CONFIG_I2C_MUX_GPIO=m, CONFIG_I2C_MUX_PCA954x=m, CONFIG_I2C_MUX_PINCTRL=m,
- CONFIG_LEDS_GPIO=m, CONFIG_LEDS_PCA9532_GPIO=y, CONFIG_LEDS_TRIGGER_GPIO=m,
- CONFIG_MDIO_GPIO=m, CONFIG_MFD_INTEL_QUARK_I2C_GPIO=m, CONFIG_MFD_SM501_GPIO=y,
- CONFIG_PINCTRL_BAYTRAIL=y, CONFIG_PINCTRL=y, CONFIG_PM_CLK=y,
- CONFIG_REGULATOR_GPIO=m, CONFIG_SENSORS_GPIO_FAN=m, CONFIG_SENSORS_SHT15=m,
- CONFIG_SND_COMPRESS_OFFLOAD=m, CONFIG_SND_DESIGNWARE_I2S=m,
- CONFIG_SND_DMAENGINE_PCM=m, CONFIG_SND_SOC_GENERIC_DMAENGINE_PCM=y,
- CONFIG_SND_SOC_I2C_AND_SPI=m, CONFIG_SND_SOC=m, CONFIG_SPI_GPIO=m,
- CONFIG_SPI_PXA2XX_DMA=y, CONFIG_SPI_PXA2XX=m, CONFIG_SPI_PXA2XX_PCI=m,
- CONFIG_SSB_DRIVER_GPIO=y, CONFIG_USB_DWC3_DUAL_ROLE=y, CONFIG_USB_F_MASS_STORAGE=m,
- CONFIG_USB_GADGET=m, CONFIG_USB_GADGET_STORAGE_NUM_BUFFERS=2,
- CONFIG_USB_GADGET_VBUS_DRAW=2, CONFIG_USB_LIBCOMPOSITE=m,
- CONFIG_USB_MASS_STORAGE=m and CONFIG_X86_INTEL_LPSS=y
- [https://elrepo.org/bugs/view.php?id=592]

* Mon Aug 31 2015 Alan Bartlett <ajb@elrepo.org> - 4.2.0-1
- Updated with the 4.2 source tarball.

* Mon Aug 17 2015 Alan Bartlett <ajb@elrepo.org> - 4.1.6-1
- Updated with the 4.1.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.1.6]

* Tue Aug 11 2015 Alan Bartlett <ajb@elrepo.org> - 4.1.5-1
- Updated with the 4.1.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.1.5]

* Wed Aug 05 2015 Alan Bartlett <ajb@elrepo.org> - 4.1.4-1
- Updated with the 4.1.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.1.4]

* Wed Jul 22 2015 Alan Bartlett <ajb@elrepo.org> - 4.1.3-1
- Updated with the 4.1.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.1.3]

* Sat Jul 11 2015 Alan Bartlett <ajb@elrepo.org> - 4.1.2-1
- Updated with the 4.1.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.1.2]

* Mon Jun 29 2015 Alan Bartlett <ajb@elrepo.org> - 4.1.1-1
- Updated with the 4.1.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.1.1]
- CONFIG_BLK_DEV_DRBD=m [https://elrepo.org/bugs/view.php?id=575]

* Mon Jun 22 2015 Alan Bartlett <ajb@elrepo.org> - 4.1.0-1
- Updated with the 4.1 source tarball.
- CONFIG_BRIDGE_NETFILTER=y [https://elrepo.org/bugs/view.php?id=573]

* Sun Jun 07 2015 Alan Bartlett <ajb@elrepo.org> - 4.0.5-1
- Updated with the 4.0.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.0.5]

* Sun May 17 2015 Alan Bartlett <ajb@elrepo.org> - 4.0.4-1
- Updated with the 4.0.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.0.4]

* Wed May 13 2015 Alan Bartlett <ajb@elrepo.org> - 4.0.3-1
- Updated with the 4.0.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.0.3]

* Thu May 07 2015 Alan Bartlett <ajb@elrepo.org> - 4.0.2-1
- Updated with the 4.0.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.0.2]

* Thu Apr 30 2015 Alan Bartlett <ajb@elrepo.org> - 4.0.1-1
- Updated with the 4.0.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.0.1]

* Mon Apr 13 2015 Alan Bartlett <ajb@elrepo.org> - 4.0.0-1
- Updated with the 4.0 source tarball.

* Thu Mar 26 2015 Alan Bartlett <ajb@elrepo.org> - 3.19.3-1
- Updated with the 3.19.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.19.3]

* Wed Mar 18 2015 Alan Bartlett <ajb@elrepo.org> - 3.19.2-1
- Updated with the 3.19.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.19.2]

* Sat Mar 07 2015 Alan Bartlett <ajb@elrepo.org> - 3.19.1-1
- Updated with the 3.19.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.19.1]

* Mon Feb 09 2015 Alan Bartlett <ajb@elrepo.org> - 3.19.0-1
- Updated with the 3.19 source tarball.

* Fri Feb 06 2015 Alan Bartlett <ajb@elrepo.org> - 3.18.6-1
- Updated with the 3.18.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.18.6]

* Fri Jan 30 2015 Alan Bartlett <ajb@elrepo.org> - 3.18.5-1
- Updated with the 3.18.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.18.5]

* Wed Jan 28 2015 Alan Bartlett <ajb@elrepo.org> - 3.18.4-1
- Updated with the 3.18.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.18.4]
- CONFIG_THUNDERBOLT=m [https://lists.elrepo.org/pipermail/elrepo/2015-January/002516.html]
- CONFIG_OVERLAY_FS=m [https://elrepo.org/bugs/view.php?id=548]

* Fri Jan 16 2015 Alan Bartlett <ajb@elrepo.org> - 3.18.3-1
- Updated with the 3.18.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.18.3]

* Fri Jan 09 2015 Alan Bartlett <ajb@elrepo.org> - 3.18.2-1
- Updated with the 3.18.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.18.2]

* Tue Dec 16 2014 Alan Bartlett <ajb@elrepo.org> - 3.18.1-1
- Updated with the 3.18.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.18.1]

* Mon Dec 08 2014 Alan Bartlett <ajb@elrepo.org> - 3.18.0-1
- Updated with the 3.18 source tarball.

* Mon Dec 08 2014 Alan Bartlett <ajb@elrepo.org> - 3.17.6-1
- Updated with the 3.17.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.17.6]

* Sun Dec 07 2014 Alan Bartlett <ajb@elrepo.org> - 3.17.5-1
- Updated with the 3.17.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.17.5]

* Sun Nov 30 2014 Alan Bartlett <ajb@elrepo,org> - 3.17.4-2
- CONFIG_BLK_DEV_NBD=m [https://elrepo.org/bugs/view.php?id=538]

* Sat Nov 22 2014 Alan Bartlett <ajb@elrepo.org> - 3.17.4-1
- Updated with the 3.17.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.17.4]
- CONFIG_CHROME_PLATFORMS=y, CONFIG_CHROMEOS_LAPTOP=m and
- CONFIG_CHROMEOS_PSTORE=m [https://elrepo.org/bugs/view.php?id=532]

* Sat Nov 15 2014 Alan Bartlett <ajb@elrepo.org> - 3.17.3-1
- Updated with the 3.17.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.17.3]
- CONFIG_BLK_DEV_RBD=m [https://elrepo.org/bugs/view.php?id=521]

* Fri Oct 31 2014 Alan Bartlett <ajb@elrepo.org> - 3.17.2-1
- Updated with the 3.17.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.17.2]

* Wed Oct 15 2014 Alan Bartlett <ajb@elrepo.org> - 3.17.1-1
- Updated with the 3.17.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.17.1]

* Mon Oct 06 2014 Alan Bartlett <ajb@elrepo.org> - 3.17.0-1
- Updated with the 3.17 source tarball.
- CONFIG_9P_FS=m, CONFIG_9P_FSCACHE=y and CONFIG_9P_FS_POSIX_ACL=y
- [https://elrepo.org/bugs/view.php?id=510]

* Thu Sep 18 2014 Alan Bartlett <ajb@elrepo.org> - 3.16.3-1
- Updated with the 3.16.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.16.3]

* Sat Sep 06 2014 Alan Bartlett <ajb@elrepo.org> - 3.16.2-1
- Updated with the 3.16.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.16.2]

* Thu Aug 14 2014 Alan Bartlett <ajb@elrepo.org> - 3.16.1-1
- Updated with the 3.16.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.16.1]
- CONFIG_ATH9K_DEBUGFS=y, CONFIG_ATH9K_HTC_DEBUGFS=y and
- CONFIG_ATH10K_DEBUGFS=y [https://elrepo.org/bugs/view.php?id=501]

* Mon Aug 04 2014 Alan Bartlett <ajb@elrepo.org> - 3.16.0-1
- Updated with the 3.16 source tarball.
- CONFIG_XEN_PCIDEV_BACKEND=y and CONFIG_XEN_FBDEV_FRONTEND=m [Mark Pryor]

* Fri Aug 01 2014 Alan Bartlett <ajb@elrepo.org> - 3.15.8-1
- Updated with the 3.15.8 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.15.8]

* Mon Jul 28 2014 Alan Bartlett <ajb@elrepo.org> - 3.15.7-1
- Updated with the 3.15.7 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.15.7]

* Fri Jul 18 2014 Alan Bartlett <ajb@elrepo.org> - 3.15.6-1
- Updated with the 3.15.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.15.6]

* Thu Jul 10 2014 Alan Bartlett <ajb@elrepo.org> - 3.15.5-1
- Updated with the 3.15.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.15.5]

* Mon Jul 07 2014 Alan Bartlett <ajb@elrepo.org> - 3.15.4-1
- Updated with the 3.15.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.15.4]

* Tue Jul 01 2014 Alan Bartlett <ajb@elrepo.org> - 3.15.3-1
- Updated with the 3.15.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.15.3]

* Fri Jun 27 2014 Alan Bartlett <ajb@elrepo.org> - 3.15.2-1
- Updated with the 3.15.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.15.2]

* Tue Jun 17 2014 Alan Bartlett <ajb@elrepo.org> - 3.15.1-1
- Updated with the 3.15.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.15.1]

* Thu Jun 12 2014 Alan Bartlett <ajb@elrepo.org> - 3.15.0-1
- General availability.

* Sun Jun 08 2014 Alan Bartlett <ajb@elrepo.org> - 3.15.0-0.rc8
- Updated with the 3.15 source tarball.
- The eighth release candidate of a kernel-ml package set for EL7.

* Sun Jun 08 2014 Alan Bartlett <ajb@elrepo.org> - 3.14.6-0.rc7
- Updated with the 3.14.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.14.6]
- The seventh release candidate of a kernel-ml package set for EL7.

* Wed Jun 04 2014 Alan Bartlett <ajb@elrepo.org> - 3.14.5-0.rc6
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.14.5]
- The sixth release candidate of a kernel-ml package set for EL7.
- Added a "Conflicts:" line for the kernel-ml-doc package.

* Mon Jun 02 2014 Alan Bartlett <ajb@elrepo.org> - 3.14.5-0.rc5
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.14.5]
- The fifth release candidate of a kernel-ml package set for EL7.
- CONFIG_SECURITY_TOMOYO_ACTIVATION_TRIGGER="/usr/lib/systemd/systemd"
- Corrected the "Conflicts:" line for the kernel-ml-tools-libs-devel
- package. [Akemi Yagi]

* Sun Jun 01 2014 Alan Bartlett <ajb@elrepo.org> - 3.14.5-0.rc4
- Updated with the 3.14.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.14.5]
- The fourth release candidate of a kernel-ml package set for EL7.
- Added a "Conflicts:" line for the kernel-ml-tools,
- kernel-ml-tools-libs and kernel-ml-tools-devel packages.

* Wed May 28 2014 Alan Bartlett <ajb@elrepo.org> - 3.14.4-0.rc3
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.14.4]
- The third release candidate of a kernel-ml package set for EL7.
- Fix a problem with the symlink between the /usr/src/$(uname -r)/
- directory and the /lib/modules/$(uname -r)/build directory.

* Sat May 24 2014 Alan Bartlett <ajb@elrepo.org> - 3.14.4-0.rc2
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.14.4]
- The second release candidate of a kernel-ml package set for EL7.
- Add calls of weak-modules to the %%posttrans and %%preun scripts.

* Tue May 20 2014 Alan Bartlett <ajb@elrepo.org> - 3.14.4-0.rc1
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.14.4]
- Skip the beta phase.
- The first release candidate of a kernel-ml package set for EL7.

* Mon May 19 2014 Alan Bartlett <ajb@elrepo.org> - 3.14.4-0.alpha3
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.14.4]
- The third attempt to build a kernel-ml package set for EL7.

* Sun May 18 2014 Alan Bartlett <ajb@elrepo.org> - 3.14.4-0.alpha2
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.14.4]
- The second attempt to build a kernel-ml package set for EL7.

* Sat May 17 2014 Alan Bartlett <ajb@elrepo.org> - 3.14.4-0.alpha1
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.14.4]
- The first attempt to build a kernel-ml package set for EL7.
