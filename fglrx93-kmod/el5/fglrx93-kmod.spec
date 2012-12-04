# Define the kmod package name here.
%define kmod_name fglrx93

# If kversion isn't defined on the rpmbuild line, define it here.
# Due to CVE-2010-3081 patch, won't build against x86_64 kernels prior to 2.6.18-194.11.4.el5
# %{!?kversion: %define kversion 2.6.18-194.26.1.el5}
%{!?kversion: %define kversion 2.6.18-308.el5}

Name:    %{kmod_name}-kmod
Version: 9.3
Release: 2%{?dist}
Group:   System Environment/Kernel
License: Proprietary
Summary: AMD %{kmod_name} kernel module(s)
URL:     http://support.amd.com/us/gpudownload/linux/Pages/radeon_linux.aspx

BuildRequires: redhat-rpm-config
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

# Sources.
Source0:  http://www2.ati.com/drivers/linux/ati-driver-installer-9-3-x86.x86_64.run
Source10: kmodtool-%{kmod_name}-el5.sh
NoSource: 0
Patch0:   fglrx93-kcl_ioctl-compat_alloc.patch

# Define the variants for each architecture.
%define basevar ""
%ifarch i686
%define paevar PAE
%endif

# If kvariants isn't defined on the rpmbuild line, build all variants for this architecture.
%{!?kvariants: %define kvariants %{?basevar} %{?paevar}}

# Magic hidden here.
%define kmodtool sh %{SOURCE10}
%{expand:%(%{kmodtool} rpmtemplate_kmp %{kmod_name} %{kversion} %{kvariants} 2>/dev/null)}

# Disable the building of the debug package(s).
%define debug_package %{nil}

# Define the filter.
%define __find_requires sh %{_builddir}/%{buildsubdir}/filter-requires.sh

%description
This package provides the proprietary AMD Display Driver %{kmod_name} kernel module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "/usr/lib/rpm/redhat/find-requires | %{__sed} -e '/^ksym.*/d'" > filter-requires.sh
echo "override fglrx * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
%{__mkdir_p} fglrx
sh %{SOURCE0} --extract atipkg

%ifarch i686
%{__cp} -a atipkg/common/* atipkg/arch/x86/* fglrx/
%endif

%ifarch x86_64
%{__cp} -a atipkg/common/* atipkg/arch/x86_64/* fglrx/
pushd fglrx
%patch0 -p0
popd
%endif

# Suppress warning message
echo 'This is a dummy file created to suppress this warning: could not find /lib/modules/fglrx/build_mod/2.6.x/.libfglrx_ip.a.GCC4.cmd for /lib/modules/fglrx/build_mod/2.6.x/libfglrx_ip.a.GCC4' > fglrx/lib/modules/fglrx/build_mod/2.6.x/.libfglrx_ip.a.GCC4.cmd

# proper permissions
find fglrx/lib/modules/fglrx/build_mod/ -type f | xargs chmod 0644

for kvariant in %{kvariants} ; do
    %{__cp} -a fglrx _kmod_build_$kvariant
done

%build
for kvariant in %{kvariants} ; do
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant/lib/modules/fglrx/build_mod/2.6.x
    %{__make} CC="gcc" PAGE_ATTR_FIX=0 KVER=%{kversion} KDIR="${ksrc}" \
    MODFLAGS="-DMODULE -DATI -DFGL" \
    LIBIP_PREFIX="%{_builddir}/%{buildsubdir}/_kmod_build_${kvariant}/lib/modules/fglrx/build_mod"
    popd
done

%install
%{__rm} -rf %{buildroot}
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
for kvariant in %{kvariants} ; do
    %{__install} -d %{buildroot}/lib/modules/%{kversion}${kvariant}/${INSTALL_MOD_DIR}
    %{__install} _kmod_build_$kvariant/lib/modules/fglrx/build_mod/2.6.x/fglrx.ko \
    %{buildroot}/lib/modules/%{kversion}${kvariant}/${INSTALL_MOD_DIR}
done
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -p -m 0644 atipkg/ATI_LICENSE.TXT %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# Strip the module.
find %{buildroot} -type f -name \*.ko -exec %{__strip} --strip-debug \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Tue Dec 04 2012 Philip J Perry <phil@elrepo.org> - 9.3-2.el5.elrepo
- Rebuild against rhel5.8 kernel for bug [http://elrepo.org/bugs/view.php?id=330]

* Sun Dec 26 2010 Philip J Perry <phil@elrepo.org> - 9.3-1.el5.elrepo
- Rename package to fglrx93
- Suppress warning message during compile.
- Set MODFLAGS as ATI does in build_mod/make.sh

* Tue Dec 21 2010 Philip J Perry <phil@elrepo.org> - 9.3-0.1
- Backport patch fglrx-kcl_ioctl-compat_alloc.patch [CVE-2010-3081]

* Sun Dec 19 2010 Marco Giunta <marco.giunta AT sissa DOT it>
- Adapt to legacy version 9.3
