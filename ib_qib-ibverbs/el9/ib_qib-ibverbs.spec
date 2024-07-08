Name: ib_qib-ibverbs
Version: 48.0
Release: 1%{?dist}
Summary: Intel QLogic InfiniPath HCA plug-in ibverbs userspace driver

# Almost everything is licensed under the OFA dual GPLv2, 2 Clause BSD license
#  providers/ipathverbs/ Dual licensed using a BSD license with an extra patent clause
#  providers/rxe/ Incorporates code from ipathverbs and contains the patent clause
#  providers/hfi1verbs Uses the 3 Clause BSD license
License: GPLv2 or BSD
Url: https://github.com/linux-rdma/rdma-core
Source: https://github.com/linux-rdma/rdma-core/releases/download/v%{version}/rdma-core-%{version}.tar.gz
Patch9000: 0003-CMakeLists-disable-providers-that-were-not-enabled-i.patch
Patch9001: 0004-CMakeLists-enable-provider-ipathverbs.patch
Patch9998: 9998-kernel-boot-Do-not-perform-device-rename-on-OPA-devi.patch
Patch9999: 9999-udev-keep-NAME_KERNEL-as-default-interface-naming-co.patch
# Do not build static libs by default.
%define with_static %{?_with_static: 1} %{?!_with_static: 0}

# 32-bit arm is missing required arch-specific memory barriers,
ExcludeArch: %{arm}

BuildRequires: binutils
BuildRequires: cmake >= 2.8.11
BuildRequires: gcc
BuildRequires: libudev-devel
BuildRequires: pkgconfig
BuildRequires: pkgconfig(libnl-3.0)
BuildRequires: pkgconfig(libnl-route-3.0)
BuildRequires: /usr/bin/rst2man
BuildRequires: valgrind-devel
BuildRequires: systemd
BuildRequires: systemd-devel
%if 0%{?rhel} >= 8 || 0%{?fedora} >= 30
BuildRequires: python3
BuildRequires: python3-docutils
%else
BuildRequires: python
BuildRequires: python-docutils
%endif
%if 0%{?fedora} >= 21 || 0%{?rhel} >= 8
BuildRequires: sed
BuildRequires: perl-generators
%endif

Requires: pciutils

# Since we recommend developers use Ninja, so should packagers, for consistency.
%define CMAKE_FLAGS %{nil}
%if 0%{?fedora} >= 23 || 0%{?rhel} >= 8
# Ninja was introduced in FC23
BuildRequires: ninja-build
%define CMAKE_FLAGS -GNinja
%if 0%{?fedora} >= 33 || 0%{?rhel} >= 9
%define make_jobs ninja-build -C %{_vpath_builddir} -v %{?_smp_mflags}
%define cmake_install DESTDIR=%{buildroot} ninja-build -C %{_vpath_builddir} install
%else
%define make_jobs ninja-build -v %{?_smp_mflags}
%define cmake_install DESTDIR=%{buildroot} ninja-build install
%endif
%else
# Fallback to make otherwise
BuildRequires: make
%define make_jobs make VERBOSE=1 %{?_smp_mflags}
%define cmake_install DESTDIR=%{buildroot} make install
%endif

%if 0%{?fedora} >= 25 || 0%{?rhel} == 8
# pandoc was introduced in FC25, Centos8
BuildRequires: pandoc
%endif

Provides: libipathverbs = %{version}-%{release}
Obsoletes: libipathverbs < %{version}-%{release}
Requires: libibverbs%{?_isa} = %{version}

%description
Device-specific plug-in ibverbs userspace driver:
- libipathverbs: Intel QLogic InfiniPath HCA

%prep
%setup -q -n rdma-core-%{version}
%if 0%{?fedora}
%patch9998 -p1
%endif
%if 0%{?rhel}
%patch9000 -p1
%patch9001 -p1
%patch9999 -p1
%endif

%build

# New RPM defines _rundir, usually as /run
%if 0%{?_rundir:1}
%else
%define _rundir /var/run
%endif

%{!?EXTRA_CMAKE_FLAGS: %define EXTRA_CMAKE_FLAGS %{nil}}

# Pass all of the rpm paths directly to GNUInstallDirs and our other defines.
%cmake %{CMAKE_FLAGS} \
         -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_BINDIR:PATH=%{_bindir} \
         -DCMAKE_INSTALL_SBINDIR:PATH=%{_sbindir} \
         -DCMAKE_INSTALL_LIBDIR:PATH=%{_libdir} \
         -DCMAKE_INSTALL_LIBEXECDIR:PATH=%{_libexecdir} \
         -DCMAKE_INSTALL_LOCALSTATEDIR:PATH=%{_localstatedir} \
         -DCMAKE_INSTALL_SHAREDSTATEDIR:PATH=%{_sharedstatedir} \
         -DCMAKE_INSTALL_INCLUDEDIR:PATH=%{_includedir} \
         -DCMAKE_INSTALL_INFODIR:PATH=%{_infodir} \
         -DCMAKE_INSTALL_MANDIR:PATH=%{_mandir} \
         -DCMAKE_INSTALL_SYSCONFDIR:PATH=%{_sysconfdir} \
         -DCMAKE_INSTALL_SYSTEMD_SERVICEDIR:PATH=%{_unitdir} \
         -DCMAKE_INSTALL_INITDDIR:PATH=%{_initrddir} \
         -DCMAKE_INSTALL_RUNDIR:PATH=%{_rundir} \
         -DCMAKE_INSTALL_DOCDIR:PATH=%{_docdir}/%{name} \
         -DCMAKE_INSTALL_UDEV_RULESDIR:PATH=%{_udevrulesdir} \
         -DCMAKE_INSTALL_PERLDIR:PATH=%{perl_vendorlib} \
         -DENABLE_IBDIAGS_COMPAT:BOOL=False \
%if %{with_static}
         -DENABLE_STATIC=1 \
%endif
         %{EXTRA_CMAKE_FLAGS} \
%if %{defined __python3}
         -DPYTHON_EXECUTABLE:PATH=%{__python3} \
         -DCMAKE_INSTALL_PYTHON_ARCH_LIB:PATH=%{python3_sitearch} \
%endif
         -DNO_PYVERBS=1
%make_jobs

%install
%cmake_install

# Remove everything except what we need
pushd %{buildroot}/%{_sysconfdir}
%{__rm} -f *.conf
%{__rm} -rf %{buildroot}/%{_sysconfdir}/{infiniband-diags,modprobe.d,rc.d,rdma}
pushd libibverbs.d
%{__mv} -f ipathverbs.driver ..
%{__rm} -f *.*
%{__mv} -f ../ipathverbs.driver .
popd
popd
%{__rm} -rf %{buildroot}/%{_bindir}
%{__rm} -rf %{buildroot}/%{_docdir}
%{__rm} -rf %{buildroot}/%{_includedir}
%{__rm} -rf %{buildroot}/%{_infodir}
%{__rm} -rf %{buildroot}/%{_libexecdir}
%{__rm} -rf %{buildroot}/%{_localstatedir}
%{__rm} -rf %{buildroot}/%{_mandir}
%{__rm} -rf %{buildroot}/%{perl_vendorlib}
%{__rm} -rf %{buildroot}/%{_sharedstatedir}
%{__rm} -rf %{buildroot}/%{_sbindir}
%{__rm} -rf %{buildroot}/%{_unitdir}
pushd %{buildroot}/%{_udevrulesdir}
cd ../..
%{__rm} -rf udev
popd
pushd %{buildroot}/%{_libdir}
%{__rm} -f *.so*
%{__rm} -rf {ibacm,pkgconfig,rsocket}
pushd libibverbs
%{__mv} -f *ipathverbs* ..
%{__rm} -f *.*
%{__mv} -f ../*ipathverbs* .
popd
popd

%ldconfig_scriptlets

%files
%config(noreplace) %{_sysconfdir}/libibverbs.d/*.driver
%{_libdir}/libibverbs/*.so

%changelog
* Mon Jul 08 2024 Tuan Hoang <tqhoang@elrepo.org> - 48.0-1
- Initial build to provide ib_qib ibverbs driver
