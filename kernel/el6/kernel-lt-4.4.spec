%global __spec_install_pre %{___build_pre}

# Define the version of the Linux Kernel Archive tarball.
%define LKAver 4.4.111

# Define the buildid, if required.
#define buildid .

# The following build options are enabled by default.
# Use either --without <option> on your rpmbuild command line
# or force the values to 0, here, to disable them.

# kernel-lt
%define with_std          %{?_without_std:          0} %{?!_without_std:          1}
# kernel-lt-NONPAE
%define with_nonpae       %{?_without_nonpae:       0} %{?!_without_nonpae:       1}
# kernel-lt-doc
%define with_doc          %{?_without_doc:          0} %{?!_without_doc:          1}
# kernel-lt-headers
%define with_headers      %{?_without_headers:      0} %{?!_without_headers:      1}
# perf subpackage
%define with_perf         %{?_without_perf:         0} %{?!_without_perf:         1}
# vdso directories installed
%define with_vdso_install %{?_without_vdso_install: 0} %{?!_without_vdso_install: 1}
# use dracut instead of mkinitrd
%define with_dracut       %{?_without_dracut:       0} %{?!_without_dracut:       1}

# Build only the kernel-lt-doc package.
%ifarch noarch
%define with_std 0
%define with_nonpae 0
%define with_headers 0
%define with_perf 0
%define with_vdso_install 0
%endif

# Build only the 32-bit kernel-lt-headers package.
%ifarch i386
%define with_std 0
%define with_nonpae 0
%define with_doc 0
%define with_perf 0
%define with_vdso_install 0
%endif

# Build only the 32-bit kernel-lt packages.
%ifarch i686
%define with_doc 0
%define with_headers 0
%endif

# Build only the 64-bit kernel-lt-headers & kernel-lt packages.
%ifarch x86_64
%define with_nonpae 0
%define with_doc 0
%endif

# Define the asmarch.
%define asmarch x86

# Define the correct buildarch.
%define buildarch x86_64
%ifarch i386 i686
%define buildarch i386
%endif

# Define the vdso_arches.
%if %{with_vdso_install}
%define vdso_arches i686 x86_64
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
%define kernel_dot_org_conflicts ppp < 2.4.3-3, isdn4k-utils < 3.2-32, nfs-utils < 1.0.7-12, e2fsprogs < 1.37-4, util-linux < 2.12, jfsutils < 1.1.7-2, reiserfs-utils < 3.6.19-2, xfsprogs < 2.6.13-4, procps < 3.2.5-6.3, oprofile < 0.9.1-2

#
# Then a series of requirements that are distribution specific, either because
# the older versions have problems with the newer kernel or lack certain things
# that make integration in the distro harder than needed.
#
%define package_conflicts initscripts < 7.23, udev < 145-11, iptables < 1.3.2-1, ipw2200-firmware < 2.4, iwl4965-firmware < 228.57.2, selinux-policy-targeted < 1.25.3-14, squashfs-tools < 4.0, wireless-tools < 29-3

#
# We moved the drm include files into kernel-headers, make sure there's
# a recent enough libdrm-devel on the system that doesn't have those.
#
%define kernel_headers_conflicts libdrm-devel < 2.4.0-0.15

#
# Packages that need to be installed before the kernel because the %%post scripts make use of them.
#
%define kernel_prereq fileutils, module-init-tools, initscripts >= 8.11.1-1, grubby >= 7.0.4-1
%if %{with_dracut}
%define initrd_prereq dracut-kernel >= 002-18.git413bcf78
%else
%define initrd_prereq mkinitrd >= 6.0.61-1
%endif

Name: kernel-lt
Summary: The Linux kernel. (The core of any Linux-based operating system.)
Group: System Environment/Kernel
License: GPLv2
URL: https://www.kernel.org/
Version: %{pkg_version}
Release: %{pkg_release}
ExclusiveArch: noarch i386 i686 x86_64
ExclusiveOS: Linux
Provides: kernel = %{version}-%{release}
Provides: kernel-%{_target_cpu} = %{version}-%{release}
Provides: kernel-drm = 4.3.0
Provides: kernel-drm-nouveau = 16
Provides: kernel-modeset = 1
Provides: kernel-uname-r = %{version}-%{release}.%{_target_cpu}
Provides: %{name} = %{version}-%{release}
Provides: %{name}-%{_target_cpu} = %{version}-%{release}
Provides: %{name}-drm = 4.3.0
Provides: %{name}-drm-nouveau = 16
Provides: %{name}-modeset = 1
Provides: %{name}-uname-r = %{version}-%{release}.%{_target_cpu}
Requires(pre): %{kernel_prereq}
Requires(pre): %{initrd_prereq}
Requires(post): /sbin/new-kernel-pkg
Requires(preun): /sbin/new-kernel-pkg
Conflicts: %{kernel_dot_org_conflicts}
Conflicts: %{package_conflicts}
Conflicts: %{kernel_headers_conflicts}
# We can't let RPM do the dependencies automatically because it'll then pick up
# a correct but undesirable perl dependency from the module headers which
# isn't required for the kernel-lt proper to function.
AutoReq: no
AutoProv: yes

#
# List the packages used during the kernel-lt build.
#
BuildRequires: asciidoc, bash >= 2.03, bc, binutils >= 2.12
BuildRequires: bzip2, diffutils, findutils, gawk
BuildRequires: gcc >= 3.4.2, gzip, m4, make >= 3.78
BuildRequires: module-init-tools, net-tools, patch >= 2.5.4
BuildRequires: patchutils, perl, python, redhat-rpm-config
BuildRequires: rpm-build >= 4.8.0-7, sh-utils, tar, xmlto
%if %{with_perf}
BuildRequires: audit-libs-devel, binutils-devel, bison, elfutils-devel
BuildRequires: elfutils-libelf-devel, flex, java-1.8.0-openjdk-devel, newt-devel
BuildRequires: numactl-devel, openssl-devel, perl(ExtUtils::Embed), python-devel
BuildRequires: slang-devel, systemtap-sdt-devel, xz-devel, zlib-devel
%endif

BuildConflicts: rhbuildsys(DiskFree) < 7Gb

# Sources.
Source0: https://www.kernel.org/pub/linux/kernel/v4.x/linux-%{LKAver}.tar.xz
Source1: config-%{version}-i686
Source2: config-%{version}-i686-NONPAE
Source3: config-%{version}-x86_64

# Do not package the source tarball.
NoSource: 0

%description
This package provides the Linux kernel (vmlinuz), the core of any
Linux-based operating system. The kernel handles the basic functions
of the OS: memory allocation, process allocation, device I/O, etc.

%package devel
Summary: Development package for building kernel modules to match the kernel.
Group: System Environment/Kernel
Provides: kernel-devel-%{_target_cpu} = %{version}-%{release}
Provides: kernel-devel = %{version}-%{release}
Provides: kernel-devel-uname-r = %{version}-%{release}.%{_target_cpu}
Provides: %{name}-devel-%{_target_cpu} = %{version}-%{release}
Provides: %{name}-devel = %{version}-%{release}
Provides: %{name}-devel-uname-r = %{version}-%{release}.%{_target_cpu}
Requires(pre): /usr/bin/find
AutoReqProv: no
%description devel
This package provides the kernel header files and makefiles
sufficient to build modules against the kernel package.

%if %{with_nonpae}
%package NONPAE
Summary: The Linux kernel for non-PAE capable processors.
Group: System Environment/Kernel
Provides: kernel = %{version}-%{release}
Provides: kernel-%{_target_cpu} = %{version}-%{release}NONPAE
Provides: kernel-NONPAE = %{version}-%{release}
Provides: kernel-NONPAE-%{_target_cpu} = %{version}-%{release}NONPAE
Provides: kernel-drm = 4.3.0
Provides: kernel-drm-nouveau = 16
Provides: kernel-modeset = 1
Provides: kernel-uname-r = %{version}-%{release}.%{_target_cpu}
Provides: %{name} = %{version}-%{release}
Provides: %{name}-%{_target_cpu} = %{version}-%{release}NONPAE
Provides: %{name}-NONPAE = %{version}-%{release}
Provides: %{name}-NONPAE-%{_target_cpu} = %{version}-%{release}NONPAE
Provides: %{name}-drm = 4.3.0
Provides: %{name}-drm-nouveau = 16
Provides: %{name}-modeset = 1
Provides: %{name}-uname-r = %{version}-%{release}.%{_target_cpu}
Requires(pre): %{kernel_prereq}
Requires(pre): %{initrd_prereq}
Requires(post): /sbin/new-kernel-pkg
Requires(preun): /sbin/new-kernel-pkg
Conflicts: %{kernel_dot_org_conflicts}
Conflicts: %{package_conflicts}
Conflicts: %{kernel_headers_conflicts}
# We can't let RPM do the dependencies automatically because it'll then pick up
# a correct but undesirable perl dependency from the module headers which
# isn't required for the kernel-lt proper to function.
AutoReq: no
AutoProv: yes
%description NONPAE
This package provides a version of the Linux kernel suitable for
processors without the Physical Address Extension (PAE) capability.
It can only address up to 4GB of memory.

%package NONPAE-devel
Summary: Development package for building kernel modules to match the non-PAE kernel.
Group: System Environment/Kernel
Provides: kernel-NONPAE-devel-%{_target_cpu} = %{version}-%{release}
Provides: kernel-NONPAE-devel = %{version}-%{release}NONPAE
Provides: kernel-NONPAE-devel-uname-r = %{version}-%{release}.%{_target_cpu}
Provides: %{name}-NONPAE-devel-%{_target_cpu} = %{version}-%{release}
Provides: %{name}-NONPAE-devel = %{version}-%{release}NONPAE
Provides: %{name}-NONPAE-devel-uname-r = %{version}-%{release}.%{_target_cpu}
Requires(pre): /usr/bin/find
AutoReqProv: no
%description NONPAE-devel
This package provides the kernel header files and makefiles
sufficient to build modules against the kernel package.
%endif

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
Summary: Header files of the Linux kernel for use by glibc.
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
Summary: Performance monitoring of the Linux kernel.
Group: Development/System
License: GPLv2
Provides: perl(Perf::Trace::Context) = 0.01
Provides: perl(Perf::Trace::Core) = 0.01
Provides: perl(Perf::Trace::Util) = 0.01
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

# Disable the building of the debug package(s).
%define debug_package %{nil}

%prep
%setup -q -n %{name}-%{version} -c
%{__mv} linux-%{LKAver} linux-%{version}-%{release}.%{_target_cpu}

pushd linux-%{version}-%{release}.%{_target_cpu} > /dev/null

%{__cp} %{SOURCE1} .
%{__cp} %{SOURCE2} .
%{__cp} %{SOURCE3} .

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

%if %{with_dracut}
    # We estimate the size of the initramfs because rpm needs to take this size
    # into consideration when performing disk space calculations. (See bz #530778)
    dd if=/dev/zero of=$RPM_BUILD_ROOT/boot/initramfs-%{KVRFA}.img bs=1M count=20
%else
    dd if=/dev/zero of=$RPM_BUILD_ROOT/boot/initrd-%{KVRFA}.img bs=1M count=5
%endif

    %{__cp} arch/x86/boot/bzImage $RPM_BUILD_ROOT/boot/vmlinuz-%{KVRFA}
    %{__chmod} 755 $RPM_BUILD_ROOT/boot/vmlinuz-%{KVRFA}

    %{__mkdir_p} $RPM_BUILD_ROOT/lib/modules/%{KVRFA}
    # Override $(mod-fw) because we don't want it to install any firmware
    # We'll do that ourselves with 'make firmware_install'
    %{__make} -s ARCH=%{buildarch} INSTALL_MOD_PATH=$RPM_BUILD_ROOT KERNELRELEASE=%{KVRFA} modules_install mod-fw=

%ifarch %{vdso_arches}
    %{__make} -s ARCH=%{buildarch} INSTALL_MOD_PATH=$RPM_BUILD_ROOT KERNELRELEASE=%{KVRFA} vdso_install
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
    %{__cp} --parents `/usr/bin/find  -type f -name "Makefile*" -o -name "Kconfig*"` $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build
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
    /usr/bin/find $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/build -type f -name "*.cmd" | xargs --no-run-if-empty %{__rm} -f

    /usr/bin/find $RPM_BUILD_ROOT/lib/modules/%{KVRFA} -type f -name "*.ko" > modnames

    # Mark the modules executable, so that strip-to-file can strip them.
    xargs --no-run-if-empty %{__chmod} u+x < modnames

    # Generate a list of modules for block and networking.
    fgrep /drivers/ modnames | xargs --no-run-if-empty nm -upA | sed -n 's,^.*/\([^/]*\.ko\):  *U \(.*\)$,\1 \2,p' > drivers.undef

    collect_modules_list()
    {
      sed -r -n -e "s/^([^ ]+) \\.?($2)\$/\\1/p" drivers.undef | LC_ALL=C sort -u > $RPM_BUILD_ROOT/lib/modules/%{KVRFA}/modules.$1
    }

    collect_modules_list networking \
        'register_netdev|ieee80211_register_hw|usbnet_probe|phy_driver_register'

    collect_modules_list block \
        'ata_scsi_ioctl|scsi_add_host|scsi_add_host_with_dma|blk_init_queue|register_mtd_blktrans|scsi_esp_register|scsi_register_device_handler'

    collect_modules_list drm \
        'drm_open|drm_init'

    collect_modules_list modesetting \
        'drm_crtc_init'

    # Detect any missing or incorrect license tags.
    %{__rm} -f modinfo

    while read i
    do
        echo -n "${i#$RPM_BUILD_ROOT/lib/modules/%{KVRFA}/} " >> modinfo
        /sbin/modinfo -l $i >> modinfo
    done < modnames

    egrep -v 'GPL( v2)?$|Dual BSD/GPL$|Dual MPL/GPL$|GPL and additional rights$' modinfo && exit 1

    %{__rm} -f modinfo modnames

    # Remove all the files that will be auto generated by depmod at the kernel install time.
    for i in alias alias.bin builtin ccwmap dep dep.bin ieee1394map inputmap isapnpmap ofmap order pcimap seriomap softdep symbols symbols.bin usbmap
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

pushd linux-%{version}-%{release}.%{_target_cpu} > /dev/null

%if %{with_nonpae}
BuildKernel NONPAE
%endif

%if %{with_std}
BuildKernel
%endif

%if %{with_doc}
# Make the HTML and man pages.
%{__make} -s %{?_smp_mflags} htmldocs 2> /dev/null
%{__make} -s %{?_smp_mflags} mandocs 2> /dev/null

# Sometimes non-world-readable files sneak into the kernel source tree.
%{__chmod} -Rf a+rX,u+w,g-w,o-w Documentation
%endif

%if %{with_perf}
%global perf_make \
    %{__make} -s -C tools/perf %{?_smp_mflags} prefix=%{_prefix} lib=%{_lib} WERROR=0 HAVE_CPLUS_DEMANGLE=1 NO_GTK2=1 NO_LIBUNWIND=1 NO_PERF_READ_VDSO32=1 NO_PERF_READ_VDSOX32=1 NO_STRLCPY=1

%{perf_make} all
%{perf_make} man
%endif

popd > /dev/null

%install
pushd linux-%{version}-%{release}.%{_target_cpu} > /dev/null

%if %{with_doc}
DOCDIR=$RPM_BUILD_ROOT%{_datadir}/doc/%{name}-doc-%{version}
MAN9DIR=$RPM_BUILD_ROOT%{_datadir}/man/man9

# Copy the documentation over.
%{__mkdir_p} $DOCDIR
%{__tar} -f - --exclude=man --exclude='.*' -c Documentation | %{__tar} xf - -C $DOCDIR

# Install the man pages for the kernel API.
%{__mkdir_p} $MAN9DIR
/usr/bin/find Documentation/DocBook/man -type f -name '*.9.gz' -print0 \
    | xargs -0 --no-run-if-empty %{__cp} -u -t $MAN9DIR
/usr/bin/find $MAN9DIR -type f -name '*.9.gz' -print0 \
    | xargs -0 --no-run-if-empty %{__chmod} 444
%endif

%if %{with_headers}
# Install the kernel headers.
%{__make} -s ARCH=%{buildarch} INSTALL_HDR_PATH=$RPM_BUILD_ROOT/usr headers_install

# Do a headers_check but don't die if it fails.
%{__make} -s ARCH=%{buildarch} INSTALL_HDR_PATH=$RPM_BUILD_ROOT/usr headers_check > hdrwarnings.txt || :
if grep -q exist hdrwarnings.txt; then
    sed s:^$RPM_BUILD_ROOT/usr/include/:: hdrwarnings.txt
    # Temporarily cause a build failure if there are header inconsistencies.
    # exit 1
fi

# Remove the unrequired files.
/usr/bin/find $RPM_BUILD_ROOT/usr/include -type f \
    \( -name .install -o -name .check -o -name ..install.cmd -o -name ..check.cmd \) | \
    xargs --no-run-if-empty %{__rm} -f

# For now, glibc provides the scsi headers.
%{__rm} -rf $RPM_BUILD_ROOT/usr/include/scsi
%{__rm} -f $RPM_BUILD_ROOT/usr/include/asm*/atomic.h
%{__rm} -f $RPM_BUILD_ROOT/usr/include/asm*/io.h
%{__rm} -f $RPM_BUILD_ROOT/usr/include/asm*/irq.h
%endif

%if %{with_perf}
# perf tool binary and supporting scripts/binaries.
%{perf_make} DESTDIR=$RPM_BUILD_ROOT install

# perf-python extension.
%{perf_make} DESTDIR=$RPM_BUILD_ROOT install-python_ext

# perf man pages. (Note: implicit rpm magic compresses them later.)
%{perf_make} DESTDIR=$RPM_BUILD_ROOT try-install-man
%endif

popd > /dev/null

%clean
%{__rm} -rf $RPM_BUILD_ROOT

# Scripts section.
%if %{with_std}
%posttrans
NEWKERNARGS=""
(/sbin/grubby --info=`/sbin/grubby --default-kernel`) 2> /dev/null | grep -q crashkernel
if [ $? -ne 0 ]; then
        NEWKERNARGS="--kernel-args=\"crashkernel=auto\""
fi
%if %{with_dracut}
/sbin/new-kernel-pkg --package %{name} --mkinitrd --dracut --depmod --update %{version}-%{release}.%{_target_cpu} $NEWKERNARGS || exit $?
%else
/sbin/new-kernel-pkg --package %{name} --mkinitrd --depmod --update %{version}-%{release}.%{_target_cpu} $NEWKERNARGS || exit $?
%endif
/sbin/new-kernel-pkg --package %{name} --rpmposttrans %{version}-%{release}.%{_target_cpu} || exit $?
if [ -x /sbin/weak-modules ]; then
    /sbin/weak-modules --add-kernel %{version}-%{release}.%{_target_cpu} || exit $?
fi

%post
if [ `uname -i` == "i386" ] && [ -f /etc/sysconfig/kernel ]; then
    /bin/sed -r -i -e 's/^DEFAULTKERNEL=kernel-lt-NONPAE$/DEFAULTKERNEL=kernel-lt/' /etc/sysconfig/kernel || exit $?
fi
if grep --silent '^hwcap 0 nosegneg$' /etc/ld.so.conf.d/kernel-*.conf 2> /dev/null; then
    /bin/sed -i '/^hwcap 0 nosegneg$/ s/0/1/' /etc/ld.so.conf.d/kernel-*.conf
fi
/sbin/new-kernel-pkg --package %{name} --install %{version}-%{release}.%{_target_cpu} || exit $?

%preun
/sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{version}-%{release}.%{_target_cpu} || exit $?
if [ -x /sbin/weak-modules ]; then
    /sbin/weak-modules --remove-kernel %{version}-%{release}.%{_target_cpu} || exit $?
fi

%post devel
if [ -f /etc/sysconfig/kernel ]; then
    . /etc/sysconfig/kernel || exit $?
fi
if [ "$HARDLINK" != "no" -a -x /usr/sbin/hardlink ]; then
    pushd /usr/src/kernels/%{version}-%{release}.%{_target_cpu} > /dev/null
    /usr/bin/find . -type f | while read f; do
        hardlink -c /usr/src/kernels/*.fc*.*/$f $f
    done
    popd > /dev/null
fi
%endif

%if %{with_nonpae}
%posttrans NONPAE
NEWKERNARGS=""
(/sbin/grubby --info=`/sbin/grubby --default-kernel`) 2> /dev/null | grep -q crashkernel
if [ $? -ne 0 ]; then
    NEWKERNARGS="--kernel-args=\"crashkernel=auto\""
fi
%if %{with_dracut}
/sbin/new-kernel-pkg --package %{name}-NONPAE --mkinitrd --dracut --depmod --update %{version}-%{release}NONPAE.%{_target_cpu} $NEWKERNARGS || exit $?
%else
/sbin/new-kernel-pkg --package %{name}-NONPAE --mkinitrd --depmod --update %{version}-%{release}NONPAE.%{_target_cpu} $NEWKERNARGS || exit $?
%endif
/sbin/new-kernel-pkg --package %{name}-NONPAE --rpmposttrans %{version}-%{release}NONPAE.%{_target_cpu} || exit $?
if [ -x /sbin/weak-modules ]; then
    /sbin/weak-modules --add-kernel %{version}-%{release}NONPAE.%{_target_cpu} || exit $?
fi

%post NONPAE
if [ `uname -i` == "i386" ] && [ -f /etc/sysconfig/kernel ]; then
    /bin/sed -r -i -e 's/^DEFAULTKERNEL=kernel-lt$/DEFAULTKERNEL=kernel-lt-NONPAE/' /etc/sysconfig/kernel || exit $?
fi
/sbin/new-kernel-pkg --package %{name}-NONPAE --install %{version}-%{release}NONPAE.%{_target_cpu} || exit $?

%preun NONPAE
/sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{version}-%{release}NONPAE.%{_target_cpu} || exit $?
if [ -x /sbin/weak-modules ]; then
    /sbin/weak-modules --remove-kernel %{version}-%{release}NONPAE.%{_target_cpu} || exit $?
fi

%post NONPAE-devel
if [ -f /etc/sysconfig/kernel ]; then
    . /etc/sysconfig/kernel || exit $?
fi
if [ "$HARDLINK" != "no" -a -x /usr/sbin/hardlink ]; then
    pushd /usr/src/kernels/%{version}-%{release}NONPAE.%{_target_cpu} > /dev/null
    /usr/bin/find . -type f | while read f; do
        hardlink -c /usr/src/kernels/*.fc*.*/$f $f
    done
    popd > /dev/null
fi
%endif

# Files section.
%if %{with_std}
%files
%defattr(-,root,root)
/boot/vmlinuz-%{version}-%{release}.%{_target_cpu}
%attr(600,root,root) /boot/System.map-%{version}-%{release}.%{_target_cpu}
/boot/symvers-%{version}-%{release}.%{_target_cpu}.gz
/boot/config-%{version}-%{release}.%{_target_cpu}
%dir /lib/modules/%{version}-%{release}.%{_target_cpu}
/lib/modules/%{version}-%{release}.%{_target_cpu}/kernel
/lib/modules/%{version}-%{release}.%{_target_cpu}/extra
/lib/modules/%{version}-%{release}.%{_target_cpu}/build
/lib/modules/%{version}-%{release}.%{_target_cpu}/source
/lib/modules/%{version}-%{release}.%{_target_cpu}/updates
/lib/modules/%{version}-%{release}.%{_target_cpu}/weak-updates
%ifarch %{vdso_arches}
/lib/modules/%{version}-%{release}.%{_target_cpu}/vdso
/etc/ld.so.conf.d/%{name}-%{version}-%{release}.%{_target_cpu}.conf
%endif
/lib/modules/%{version}-%{release}.%{_target_cpu}/modules.*
%if %{with_dracut}
%ghost /boot/initramfs-%{version}-%{release}.%{_target_cpu}.img
%else
%ghost /boot/initrd-%{version}-%{release}.%{_target_cpu}.img
%endif

%files devel
%defattr(-,root,root)
%dir /usr/src/kernels
/usr/src/kernels/%{version}-%{release}.%{_target_cpu}
%endif

%if %{with_nonpae}
%files NONPAE
%defattr(-,root,root)
/boot/vmlinuz-%{version}-%{release}NONPAE.%{_target_cpu}
/boot/System.map-%{version}-%{release}NONPAE.%{_target_cpu}
/boot/symvers-%{version}-%{release}NONPAE.%{_target_cpu}.gz
/boot/config-%{version}-%{release}NONPAE.%{_target_cpu}
%dir /lib/modules/%{version}-%{release}NONPAE.%{_target_cpu}
/lib/modules/%{version}-%{release}NONPAE.%{_target_cpu}/kernel
/lib/modules/%{version}-%{release}NONPAE.%{_target_cpu}/extra
/lib/modules/%{version}-%{release}NONPAE.%{_target_cpu}/build
/lib/modules/%{version}-%{release}NONPAE.%{_target_cpu}/source
/lib/modules/%{version}-%{release}NONPAE.%{_target_cpu}/updates
/lib/modules/%{version}-%{release}NONPAE.%{_target_cpu}/weak-updates
%ifarch %{vdso_arches}
/lib/modules/%{version}-%{release}NONPAE.%{_target_cpu}/vdso
/etc/ld.so.conf.d/%{name}-%{version}-%{release}NONPAE.%{_target_cpu}.conf
%endif
/lib/modules/%{version}-%{release}NONPAE.%{_target_cpu}/modules.*
%if %{with_dracut}
%ghost /boot/initramfs-%{version}-%{release}NONPAE.%{_target_cpu}.img
%else
%ghost /boot/initrd-%{version}-%{release}NONPAE.%{_target_cpu}.img
%endif

%files NONPAE-devel
%defattr(-,root,root)
%dir /usr/src/kernels
/usr/src/kernels/%{version}-%{release}NONPAE.%{_target_cpu}
%endif

%if %{with_doc}
%files doc
%defattr(-,root,root)
%{_datadir}/doc/%{name}-doc-%{version}/Documentation/*
%dir %{_datadir}/doc/%{name}-doc-%{version}/Documentation
%dir %{_datadir}/doc/%{name}-doc-%{version}
%{_datadir}/man/man9/*
%endif

%if %{with_headers}
%files headers
%defattr(-,root,root)
%{_includedir}/*
%endif

%if %{with_perf}
%files -n perf
%defattr(-,root,root)
%{_bindir}/perf
%{_bindir}/trace
%dir %{_libdir}/traceevent/plugins
%{_libdir}/traceevent/plugins/*
%dir %{_libexecdir}/perf-core
%{_libexecdir}/perf-core/*
%{_mandir}/man[1-8]/perf*
%config(noreplace) %{_sysconfdir}/bash_completion.d/perf
%dir %{_datadir}/perf-core/strace/groups
%{_datadir}/perf-core/strace/groups/*
%doc linux-%{version}-%{release}.%{_target_cpu}/tools/perf/Documentation/examples.txt

%files -n python-perf
%defattr(-,root,root)
%{python_sitearch}
%endif

%changelog
* Wed Jan 10 2018 Alan Bartlett <ajb@elrepo.org> - 4.4.111-1
- Updated with the 4.4.111 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.111]

* Fri Jan 05 2018 Alan Bartlett <ajb@elrepo.org> - 4.4.110-1
- Updated with the 4.4.110 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.110]
- CONFIG_PAGE_TABLE_ISOLATION=y

* Tue Jan 02 2018 Alan Bartlett <ajb@elrepo.org> - 4.4.109-1
- Updated with the 4.4.109 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.109]

* Mon Dec 25 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.108-1
- Updated with the 4.4.108 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.108]

* Wed Dec 20 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.107-1
- Updated with the 4.4.107 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.107]
- CONFIG_DRM_AMDGPU_CIK=Y
- [https://elrepo.org/bugs/view.php?id=807]

* Sat Dec 16 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.106-1
- Updated with the 4.4.106 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.106]

* Sat Dec 09 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.105-1
- Updated with the 4.4.105 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.105]
- Adjusted the list of files to be removed, as they will be
- created by depmod at the package installation time.
- [https://elrepo.org/bugs/view.php?id=803]

* Tue Dec 05 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.104-1
- Updated with the 4.4.104 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.104]

* Thu Nov 30 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.103-1
- Updated with the 4.4.103 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.103]

* Fri Nov 24 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.102-1
- Updated with the 4.4.102 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.102]

* Fri Nov 24 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.101-1
- Updated with the 4.4.101 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.101]

* Tue Nov 21 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.100-1
- Updated with the 4.4.100 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.100]

* Sat Nov 18 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.99-1
- Updated with the 4.4.99 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.99]

* Wed Nov 15 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.98-1
- Updated with the 4.4.98 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.98]

* Wed Nov 08 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.97-1
- Updated with the 4.4.97 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.97]

* Sun Nov 05 2017 Alan Bartlett <ajb@elrepo.org> - 4.4.96-1
- Updated with the 4.4.96 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v4.x/ChangeLog-4.4.96]

* Sun Nov 05 2017 Alan Bartlett <ajb@elrepo.org> - 3.10.108-1
- Updated with the 3.10.108 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.108]

* Tue Jun 27 2017 Alan Bartlett <ajb@elrepo.org> - 3.10.107-1
- Updated with the 3.10.107 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.107]

* Fri Jun 16 2017 Alan Bartlett <ajb@elrepo.org> - 3.10.106-1
- Updated with the 3.10.106 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.106]
- Added NO_PERF_READ_VDSO32=1 and NO_PERF_READ_VDSOX32=1
- directives to the %%global perf_make line.
- [https://elrepo.org/bugs/view.php?id=719]
- CONFIG_MEMCG_SWAP_ENABLED=y [http://elrepo.org/bugs/view.php?id=744]

* Fri Feb 10 2017 Alan Bartlett <ajb@elrepo.org> - 3.10.105-1
- Updated with the 3.10.105 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.105]

* Fri Oct 21 2016 Alan Bartlett <ajb@elrepo.org> - 3.10.104-1
- Updated with the 3.10.104 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.104]

* Sun Aug 28 2016 Alan Bartlett <ajb@elrepo.org> - 3.10.103-1
- Updated with the 3.10.103 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.103]

* Tue Jun 14 2016 Alan Bartlett <ajb@elrepo.org> - 3.10.102-1
- Updated with the 3.10.102 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.102]

* Wed Mar 16 2016 Alan Bartlett <ajb@elrepo.org> - 3.10.101-1
- Updated with the 3.10.101 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.101]

* Thu Mar 10 2016 Alan Bartlett <ajb@elrepo.org> - 3.10.100-1
- Updated with the 3.10.100 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.100]

* Fri Mar 04 2016 Alan Bartlett <ajb@elrepo.org> - 3.10.99-1
- Updated with the 3.10.99 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.99]

* Thu Feb 25 2016 Alan Bartlett <ajb@elrepo.org> - 3.10.98-1
- Updated with the 3.10.98 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.98]

* Sat Feb 20 2016 Alan Bartlett <ajb@elrepo.org> - 3.10.97-1
- Updated with the 3.10.97 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.97]

* Fri Jan 29 2016 Alan Bartlett <ajb@elrepo.org> - 3.10.96-1
- Updated with the 3.10.96 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.96]

* Sat Jan 23 2016 Alan Bartlett <ajb@elrepo.org> - 3.10.95-1
- Updated with the 3.10.95 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.95]

* Thu Dec 10 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.94-1
- Updated with the 3.10.94 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.94]
- CONFIG_VXFS_FS=m [https://elrepo.org/bugs/view.php?id=606]

* Tue Nov 10 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.93-1
- Updated with the 3.10.93 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.93]

* Tue Oct 27 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.92-1
- Updated with the 3.10.92 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.92]

* Fri Oct 23 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.91-1
- Updated with the 3.10.91 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.91]

* Thu Oct 01 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.90-1
- Updated with the 3.10.90 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.90]

* Mon Sep 21 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.89-1
- Updated with the 3.10.89 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.89]

* Mon Sep 14 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.88-1
- Updated with the 3.10.88 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.88]

* Mon Aug 17 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.87-1
- Updated with the 3.10.87 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.87]

* Tue Aug 11 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.86-1
- Updated with the 3.10.86 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.86]

* Tue Aug 04 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.85-1
- Updated with the 3.10.85 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.85]

* Sat Jul 11 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.84-1
- Updated with the 3.10.84 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.84]

* Sat Jul 04 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.83-1
- Updated with the 3.10.83 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.83]

* Mon Jun 29 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.82-1
- Updated with the 3.10.82 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.82]

* Tue Jun 23 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.81-1
- Updated with the 3.10.81 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.81]

* Sun Jun 07 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.80-1
- Updated with the 3.10.80 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.80]

* Mon May 18 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.79-1
- Updated with the 3.10.79 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.79]

* Wed May 13 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.78-1
- Updated with the 3.10.78 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.78]

* Thu May 07 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.77-1
- Updated with the 3.10.77 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.77]
- CONFIG_SUNRPC_DEBUG=y [Jamie Bainbridge]

* Wed Apr 29 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.76-1
- Updated with the 3.10.76 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.76]

* Sun Apr 19 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.75-1
- Updated with the 3.10.75 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.75]

* Mon Apr 13 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.74-1
- Updated with the 3.10.74 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.74]

* Thu Mar 26 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.73-1
- Updated with the 3.10.73 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.73]

* Wed Mar 18 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.72-1
- Updated with the 3.10.72 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.72]

* Sat Mar 07 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.71-1
- Updated with the 3.10.71 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.71]

* Fri Feb 27 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.70-1
- Updated with the 3.10.70 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.70]

* Wed Feb 11 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.69-1
- Updated with the 3.10.69 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.69]

* Fri Feb 06 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.68-1
- Updated with the 3.10.68 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.68]

* Fri Jan 30 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.67-1
- Updated with the 3.10.67 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.67]

* Wed Jan 28 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.66-1
- Updated with the 3.10.66 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.66]

* Sat Jan 17 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.65-1
- Updated with the 3.10.65 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.65]

* Fri Jan 09 2015 Alan Bartlett <ajb@elrepo.org> - 3.10.64-1
- Updated with the 3.10.64 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.64]

* Tue Dec 16 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.63-1
- Updated with the 3.10.63 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.63]

* Sun Dec 07 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.62-1
- Updated with the 3.10.62 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.62]

* Sat Nov 22 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.61-1
- Updated with the 3.10.61 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.61]

* Sat Nov 15 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.60-1
- Updated with the 3.10.60 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.60]

* Fri Oct 31 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.59-1
- Updated with the 3.10.59 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.59]

* Wed Oct 15 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.58-1
- Updated with the 3.10.58 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.58]

* Fri Oct 10 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.57-1
- Updated with the 3.10.57 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.57]

* Mon Oct 06 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.56-1
- Updated with the 3.10.56 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.56]
- CONFIG_NUMA_BALANCING=y and CONFIG_NUMA_BALANCING_DEFAULT_ENABLED=y
- [https://elrepo.org/bugs/view.php?id=509]
- CONFIG_9P_FS=m, CONFIG_9P_FSCACHE=y and CONFIG_9P_FS_POSIX_ACL=y
- [https://elrepo.org/bugs/view.php?id=510]

* Thu Sep 18 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.55-1
- Updated with the 3.10.55 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.55]

* Sat Sep 06 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.54-1
- Updated with the 3.10.54 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.54]
- CONFIG_RCU_NOCB_CPU=y and CONFIG_RCU_NOCB_CPU_ALL=y
- [https://elrepo.org/bugs/view.php?id=505]

* Thu Aug 14 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.53-1
- Updated with the 3.10.53 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.53]

* Fri Aug 08 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.52-1
- Updated with the 3.10.52 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.52]
- CONFIG_ATH9K_DEBUGFS=y, CONFIG_ATH9K_MAC_DEBUG=y and
- CONFIG_ATH9K_HTC_DEBUGFS=y [https://elrepo.org/bugs/view.php?id=501]

* Fri Aug 01 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.51-1
- Updated with the 3.10.51 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.51]

* Mon Jul 28 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.50-1
- Updated with the 3.10.50 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.50]
- CONFIG_INTEL_MEI=m and CONFIG_INTEL_MEI_ME=m
- [https://elrepo.org/bugs/view.php?id=493]

* Fri Jul 18 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.49-1
- Updated with the 3.10.49 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.49]

* Thu Jul 10 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.48-1
- Updated with the 3.10.48 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.48]

* Mon Jul 07 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.47-1
- Updated with the 3.10.47 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.47]

* Tue Jul 01 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.46-1
- Updated with the 3.10.46 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.46]

* Fri Jun 27 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.45-1
- Updated with the 3.10.45 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.45]

* Tue Jun 17 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.44-1
- Updated with the 3.10.44 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.44]

* Thu Jun 12 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.43-1
- Updated with the 3.10.43 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.43]

* Sun Jun 08 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.42-1
- Updated with the 3.10.42 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.42]

* Sun Jun 01 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.41-1
- Updated with the 3.10.41 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.41]

* Tue May 13 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.40-1
- Updated with the 3.10.40 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.40]

* Tue May 06 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.39-1
- Updated with the 3.10.39 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.39]

* Sun Apr 27 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.38-1
- Updated with the 3.10.38 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.38]
- CONFIG_FANOTIFY=y [https://elrepo.org/bugs/view.php?id=470]

* Mon Apr 14 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.37-1
- Updated with the 3.10.37 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.37]

* Fri Apr 04 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.36-1
- Updated with the 3.10.36 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.36]

* Mon Mar 31 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.35-1
- Updated with the 3.10.35 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.35]

* Mon Mar 24 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.34-1
- Updated with the 3.10.34 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.34]

* Fri Mar 07 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.33-1
- Updated with the 3.10.33 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.33]
- CONFIG_CIFS_SMB2=y [https://elrepo.org/bugs/view.php?id=461]

* Sun Feb 23 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.32-1
- Updated with the 3.10.32 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.32]

* Fri Feb 21 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.31-1
- Updated with the 3.10.31 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.31]
- CONFIG_ACPI_HOTPLUG_MEMORY=y [https://elrepo.org/bugs/view.php?id=454]

* Fri Feb 14 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.30-1
- Updated with the 3.10.30 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.30]

* Fri Feb 07 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.29-1
- Updated with the 3.10.29 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.29]

* Sun Jan 26 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.28-1
- Updated with the 3.10.28 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.28]

* Thu Jan 16 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.27-1
- Updated with the 3.10.27 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.27]

* Fri Jan 10 2014 Alan Bartlett <ajb@elrepo.org> - 3.10.26-1
- Updated with the 3.10.26 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.26]
- CONFIG_L2TP=m, CONFIG_PPPOL2TP=m [https://elrepo.org/bugs/view.php?id=443]

* Fri Dec 20 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.25-1
- Updated with the 3.10.25 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.25]

* Thu Dec 12 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.24-1
- Updated with the 3.10.24 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.24]

* Mon Dec 09 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.23-1
- Updated with the 3.10.23 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.23]

* Thu Dec 05 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.22-1
- Updated with the 3.10.22 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.22]

* Sat Nov 30 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.21-1
- Updated with the 3.10.21 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.21]

* Thu Nov 21 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.20-1
- Updated with the 3.10.20 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.20]
- CONFIG_HFS_FS=m and CONFIG_HFSPLUS_FS=m [https://elrepo.org/bugs/view.php?id=427]

* Wed Nov 13 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.19-1
- Updated with the 3.10.19 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.19]

* Mon Nov 04 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.18-1
- Updated with the 3.10.18 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.18]

* Fri Oct 18 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.17-1
- Updated with the 3.10.17 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.17]

* Mon Oct 14 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.16-1
- Updated with the 3.10.16 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.16]

* Sat Oct 05 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.15-1
- Updated with the 3.10.15 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.15]

* Wed Oct 02 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.14-1
- Updated with the 3.10.14 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.14]

* Fri Sep 27 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.13-1
- Updated with the 3.10.13 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.13]

* Mon Sep 16 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.12-1
- Updated with the 3.10.12 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.12]
- CONFIG_BCACHE=m [https://elrepo.org/bugs/view.php?id=407]

* Sun Sep 08 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.11-1
- Updated with the 3.10.11 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.11]

* Thu Aug 29 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.10-1
- Updated with the 3.10.10 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.10]

* Wed Aug 21 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.9-1
- Updated with the 3.10.9 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.9]

* Tue Aug 20 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.8-1
- Updated with the 3.10.8 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.8]

* Thu Aug 15 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.7-1
- Updated with the 3.10.7 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.7]

* Mon Aug 12 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.6-1
- Updated with the 3.10.6 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.6]

* Sun Aug 04 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.5-1
- Updated with the 3.10.5 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.5]

* Mon Jul 29 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.4-1
- Updated with the 3.10.4 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.4]

* Fri Jul 26 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.3-1
- Updated with the 3.10.3 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.3]

* Mon Jul 22 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.2-1
- Updated with the 3.10.2 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.2]

* Sun Jul 14 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.1-1
- Updated with the 3.10.1 source tarball.
- [https://www.kernel.org/pub/linux/kernel/v3.x/ChangeLog-3.10.1]

* Mon Jul 01 2013 Alan Bartlett <ajb@elrepo.org> - 3.10.0-1
- Updated with the 3.10 source tarball.
