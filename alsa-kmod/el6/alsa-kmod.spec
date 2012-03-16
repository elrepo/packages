# Define the kmod package name here.
%define	 kmod_name alsa
%define  src_name alsa-driver

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-220.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 1.0.25
Release: 4%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: ALSA driver kernel modules
URL:     http://www.alsa-project.org/

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0:  %{src_name}-%{version}.tar.bz2
Source10: kmodtool-%{kmod_name}-el6.sh

# Patches.
Patch0:   alsa-1.0.25-adriver.patch
Patch1:   alsa-1.0.25-usb-makefile.patch

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the Advanced Linux Sound Architecture (ALSA) driver modules.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T -a 0
%patch0 -p0 -b .orig
%patch1 -p0 -b .orig
%{__cp} -a %{src_name}-%{version} _kmod_build_
KSRC=%{_usrsrc}/kernels/%{kversion}
pushd _kmod_build_
./configure -q --with-kernel="${KSRC}" --with-build="${KSRC}" \
    --with-redhat=yes --with-isapnp=no
popd

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
pushd _kmod_build_
%{__make}
popd

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} _kmod_build_/modules/*.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
MODULES_DIR=%{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}
:>kmod-%{kmod_name}.conf
for FILENAME in $MODULES_DIR/*.ko
do
    MODULE="$( echo $FILENAME | sed 's/.*\/\(.*\)\.ko/\1/' )"
    echo "override $MODULE * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
done
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} _kmod_build_/{CARDS-STATUS,COPYING,FAQ,README,SUPPORTED_KERNELS,TODO,WARNING} \
    %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Sat Mar 10 2012 Phil Schaffner <pschaff2@verizon.net> - 1.0.25-4.el6.elrepo
- Build agaist latest kernel due to upstream changes
- per http://elrepo.org/bugs/view.php?id=244

* Fri Mar 02 2012 Phil Schaffner <pschaff2@verizon.net> - 1.0.25-3.el6.elrepo
- Include alsa-1.0.25-adriver.patch and alsa-1.0.25-usb-makefile.patch
- Generate overrides automatically.

* Sun Feb 26 2012 Phil Schaffner <pschaff2@verizon.net> - 1.0.25-2.el6.elrepo
- Update overrides.

* Wed Feb 22 2012 Phil Schaffner <pschaff2@verizon.net> - 1.0.25-1.el6.elrepo
- Update to 1.0.25.

* Fri Apr 29 2011 Philip J Perry <phil@elrepo.org> - 1.0.24-1.el6.elrepo
- Update to 1.0.24.

* Mon Jan 24 2011 Philip J Perry <phil@elrepo.org> - 1.0.23-1.el6.elrepo
- Initial el6 build of the kmod package.
