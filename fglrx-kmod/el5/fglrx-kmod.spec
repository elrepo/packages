# Define the kmod package name here.
%define kmod_name fglrx

# If kversion isn't defined on the rpmbuild line, define it here.
# kABI compatible with kernel 2.6.18-128.el5 upwards.
# Due to CVE-2010-3081 patch, won't build against x86_64 kernels prior to 2.6.18-194.11.4.el5
%{!?kversion: %define kversion 2.6.18-238.el5}

Name:    %{kmod_name}-kmod
Version: 11.8
Release: 1%{?dist}
Group:   System Environment/Kernel
License: Proprietary
Summary: AMD %{kmod_name} kernel module(s)
URL:     http://support.amd.com/us/gpudownload/linux/Pages/radeon_linux.aspx

BuildRequires: redhat-rpm-config
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

# Sources.
Source0:  http://www2.ati.com/drivers/linux/ati-driver-installer-11-8-x86.x86_64.run
Source10: kmodtool-%{kmod_name}-el5.sh
NoSource: 0

# Define the variants for each architecture.
%define basevar ""
%ifarch i686
%define paevar PAE
%endif

# If kvariants isn't defined on the rpmbuild line, build all variants for this architecture.
%{!?kvariants: %define kvariants %{?basevar} %{?paevar}}

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate_kmp %{kmod_name} %{kversion} %{kvariants})}

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
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
%{__mkdir_p} fglrx
sh %{SOURCE0} --extract atipkg

%ifarch i686
%{__cp} -a atipkg/common/* atipkg/arch/x86/* fglrx/
%endif

%ifarch x86_64
%{__cp} -a atipkg/common/* atipkg/arch/x86_64/* fglrx/
%endif

# Suppress warning message
echo 'This is a dummy file created to suppress this warning: could not find /lib/modules/fglrx/build_mod/2.6.x/.libfglrx_ip.a.cmd for /lib/modules/fglrx/build_mod/2.6.x/libfglrx_ip.a' > fglrx/lib/modules/fglrx/build_mod/2.6.x/.libfglrx_ip.a.cmd

# proper permissions
find fglrx/lib/modules/fglrx/build_mod/ -type f | xargs chmod 0644

for kvariant in %{kvariants} ; do
    %{__cp} -a fglrx _kmod_build_$kvariant
done

%build
for kvariant in %{kvariants} ; do
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant/lib/modules/fglrx/build_mod/2.6.x
    CFLAGS_MODULE="-DMODULE -DATI -DFGL -DPAGE_ATTR_FIX=1 -DCOMPAT_ALLOC_USER_SPACE=arch_compat_alloc_user_space -D__SMP__ -DMODVERSIONS"
    %{__make} CC="gcc" PAGE_ATTR_FIX=1 KVER=%{kversion} KDIR="${ksrc}" \
    MODFLAGS="$CFLAGS_MODULE" \
    CFLAGS_MODULE="$CFLAGS_MODULE" \
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
%{__install} -p -m 0644 atipkg/LICENSE.TXT %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Sat Sep 10 2011 Philip J Perry <phil@elrepo.org> - 11.8-1.el5.elrepo
- Update to version 11.8.

* Thu Jul 28 2011 Philip J Perry <phil@elrepo.org> - 11.7-1.el5.elrepo
- Update to version 11.7.

* Sat Jun 25 2011 Philip J Perry <phil@elrepo.org> - 11.6-1.el5.elrepo
- Update to version 11.6.

* Sun Jun 19 2011 Philip J Perry <phil@elrepo.org> - 11.5-4.el5.elrepo
- Rebuild for version 11.5-4.

* Fri Jun 17 2011 Philip J Perry <phil@elrepo.org> - 11.5-3.el5.elrepo
- Rebuild for version 11.5-3.

* Mon May 23 2011 Marco Giunta <giunta AT sissa DOT it> - 11.5-1.el5.elrepo
- Update to version 11.5.

* Thu May 05 2011 Marco Giunta <giunta AT sissa DOT it> - 11.4-1.el5.elrepo
- Update to version 11.4.

* Thu Mar 31 2011 Marco Giunta <giunta AT sissa.it> - 11.3-1.el5.elrepo
- Update to version 11.3.

* Sat Feb 19 2011 Marco Giunta <giunta AT sissa.it> - 11.2-1.el5.elrepo
- Update to version 11.2.
- Removed patch fglrx-kcl_ioctl-compat_alloc.patch, fixed upstream since version 10.10. 

* Fri Jan 28 2011 Marco Giunta <giunta AT sissa.it> - 11.1-1.el5.elrepo
- Update to version 11.1.
- Rebuilt against RHEL-5.6 base kernel 2.6.18-238.el5

* Tue Dec 21 2010 Philip J Perry <phil@elrepo.org>
- Suppress warning message during compile.
- Set MODFLAGS as ATI does in build_mod/make.sh

* Wed Dec 15 2010 Philip J Perry <phil@elrepo.org> - 10.12-1.el5.elrepo
- Update to version 10.12.
- Don't build for xen.

* Mon Dec 13 2010 Philip J Perry <phil@elrepo.org> - 10.11-1.el5.elrepo
- Rebuilt for release.

* Fri Dec 10 2010 Philip J Perry <phil@elrepo.org> - 10.11-0.3
- Make patch fglrx-kcl_ioctl-compat_alloc.patch x86_64 specific.

* Wed Dec 08 2010 Philip J Perry <phil@elrepo.org> - 10.11-0.2
- Strip the module - ATI does in build_mod/make.sh
- Install the License

* Mon Dec 06 2010 Philip J Perry <phil@elrepo.org> - 10.11-0.1
- Patch fglrx-kcl_ioctl-compat_alloc.patch [CVE-2010-3081]
- Initial el5 build of the kmod package.
