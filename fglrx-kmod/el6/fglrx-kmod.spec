# Define the kmod package name here.
%define kmod_name fglrx

# If kversion isn't defined on the rpmbuild line, define it here.
# Due to CVE-2010-3081 patch, won't build against x86_64 kernels prior to 2.6.32-71.7.1.el6
%{!?kversion: %define kversion 2.6.32-131.0.15.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 11.7
Release: 1%{?dist}
Group:   System Environment/Kernel
License: Proprietary
Summary: AMD %{kmod_name} kernel module(s)
URL:     http://support.amd.com/us/gpudownload/linux/Pages/radeon_linux.aspx

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0:  http://www2.ati.com/drivers/linux/ati-driver-installer-11-7-x86.x86_64.run
Source10: kmodtool-%{kmod_name}-el6.sh
NoSource: 0

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the proprietary AMD Display Driver %{kmod_name} kernel module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
%{__mkdir_p} _kmod_build_
sh %{SOURCE0} --extract atipkg

%ifarch i686
%{__cp} -a atipkg/common/* atipkg/arch/x86/* _kmod_build_/
%endif

%ifarch x86_64
%{__cp} -a atipkg/common/* atipkg/arch/x86_64/* _kmod_build_/
%endif

# Suppress warning message
echo 'This is a dummy file created to suppress this warning: could not find /lib/modules/fglrx/build_mod/2.6.x/.libfglrx_ip.a.GCC4.cmd for /lib/modules/fglrx/build_mod/2.6.x/libfglrx_ip.a.GCC4' > _kmod_build_/lib/modules/fglrx/build_mod/2.6.x/.libfglrx_ip.a.GCC4.cmd

# proper permissions
find _kmod_build_/lib/modules/fglrx/build_mod/ -type f | xargs chmod 0644

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
pushd _kmod_build_/lib/modules/fglrx/build_mod/2.6.x
    CFLAGS_MODULE="-DMODULE -DATI -DFGL -DPAGE_ATTR_FIX=0 -DCOMPAT_ALLOC_USER_SPACE=arch_compat_alloc_user_space -D__SMP__ -DMODVERSIONS"
    %{__make} CC="gcc" PAGE_ATTR_FIX=0 KVER=%{kversion} KDIR="${KSRC}" \
    MODFLAGS="$CFLAGS_MODULE" \
    CFLAGS_MODULE="$CFLAGS_MODULE" \
    LIBIP_PREFIX="%{_builddir}/%{buildsubdir}/_kmod_build_/lib/modules/fglrx/build_mod"
popd

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} _kmod_build_/lib/modules/fglrx/build_mod/2.6.x/fglrx.ko \
%{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -p -m 0644 atipkg/ATI_LICENSE.TXT \
%{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Thu Jul 28 2011 Philip J Perry <phil@elrepo.org> - 11.7-1.el6.elrepo
- Update to version 11.7.

* Sat Jun 25 2011 Philip J Perry <phil@elrepo.org> - 11.6-1.el6.elrepo
- Update to version 11.6.

* Sun Jun 19 2011 Philip J Perry <phil@elrepo.org> - 11.5-4.el6.elrepo
- Rebuild for version 11.5-4.

* Fri Jun 17 2011 Philip J Perry <phil@elrepo.org> - 11.5-3.el6.elrepo
- Rebuild for version 11.5-3.

* Sat Jun 11 2011 Philip J Perry <phil@elrepo.org> - 11.5-2.el6.elrepo
- Rebuild for version 11.5-2.

* Fri Jun 03 2011 Philip J Perry <phil@elrepo.org> - 11.5-1.el6.elrepo
- Update to version 11.5.

* Fri Feb 11 2011 Philip J Perry <phil@elrepo.org> - 10.12-1.2.el6.elrepo
- bump to match fglrx-x11-drv version.

* Fri Jan 28 2011 Philip J Perry <phil@elrepo.org> - 10.12-1.el6.elrepo
- Update to version 10.12.
- Suppress warning message during compile.
- Set MODFLAGS as ATI does in build_mod/make.sh

* Fri Dec 10 2010 Philip J Perry <phil@elrepo.org> - 10.11-0.3
- Make patch fglrx-kcl_ioctl-compat_alloc.patch x86_64 specific.

* Wed Dec 08 2010 Philip J Perry <phil@elrepo.org> - 10.11-0.2
- Strip the module - ATI does in build_mod/make.sh
- Install the License

* Mon Dec 06 2010 Philip J Perry <phil@elrepo.org> - 10.11-0.1
- Patch fglrx-kcl_ioctl-compat_alloc.patch [CVE-2010-3081]
- Initial el6 build of the kmod package.
