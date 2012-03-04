# Define the kmod package name here.
%define	 kmod_name alsa
%define  src_name alsa-driver

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.18-238.el5}

Name:	 %{kmod_name}-kmod
Version: 1.0.25
Release: 3%{?dist}
Group:	 System Environment/Kernel
License: GPLv2
Summary: ALSA driver modules
URL:	 http://www.alsa-project.org/

BuildRequires: redhat-rpm-config
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

# Sources.
Source0:  %{src_name}-%{version}.tar.bz2
Source10: kmodtool-%{kmod_name}-el5.sh

# Patches.
Patch0:   alsa-1.0.25-adriver.patch
Patch1:   alsa-1.0.25-usb-makefile.patch

# Define the variants for each architecture.
%define basevar ""
%ifarch i686
%define paevar PAE
%endif
%ifarch i686 x86_64
%define xenvar xen
%endif

# If kvariants isn't defined on the rpmbuild line, build all variants for this architecture.
%{!?kvariants: %define kvariants %{?basevar} %{?xenvar} %{?paevar}}

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate_kmp %{kmod_name} %{kversion} %{kvariants})}

# Disable the building of the debug package(s).
%define debug_package %{nil}

# Define the filter.
%define __find_requires sh %{_builddir}/%{buildsubdir}/filter-requires.sh

%description
This package provides the Advanced Linux Sound Architecture (ALSA) driver modules.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T -a 0
%patch0 -p0 -b .orig
%patch1 -p0 -b .orig
echo "/usr/lib/rpm/redhat/find-requires | %{__sed} -e '/^ksym.*/d'" > filter-requires.sh
%{__grep} -rl '#include <linux/config.h>' . | xargs %{__perl} -pi -e's,#include <linux/config.h>,#include <linux/autoconf.h>,'
for kvariant in %{kvariants} ; do
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    %{__cp} -a %{src_name}-%{version} _kmod_build_$kvariant
    pushd _kmod_build_$kvariant
    ./configure -q --with-kernel="${ksrc}" --with-build="${ksrc}" \
        --with-redhat=yes --with-isapnp=no
    popd
done

%build
for kvariant in %{kvariants} ; do
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant
    %{__make}
    popd
done

%install
%{__rm} -rf %{buildroot}
for kvariant in %{kvariants} ; do
    %{__install} -d %{buildroot}/lib/modules/%{kversion}${kvariant}/extra/%{kmod_name}/
    %{__install} _kmod_build_$kvariant/modules/*.ko %{buildroot}/lib/modules/%{kversion}${kvariant}/extra/%{kmod_name}/
    modules_dir=%{buildroot}/lib/modules/%{kversion}${kvariant}/extra/%{kmod_name}/
    :>kmod-alsa.conf
    for filename in $modules_dir*.ko
    do
        module="$( echo $filename | sed 's/.*\/\(.*\)\.ko/\1/' )"
        echo "override $module * weak-updates/alsa" >> kmod-%{kmod_name}.conf
    done
    %{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
    %{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
    %{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
    %{__install} _kmod_build_$kvariant/{CARDS-STATUS,COPYING,FAQ,README,SUPPORTED_KERNELS,TODO,WARNING} \
        %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
done
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Thu Mar 01 2012 Phil Schaffner <pschaff2@verizon.net> - 1.0.25-3.el6.elrepo
- Add alsa-1.0.25-adriver.patch and alsa-1.0.25-usb-makefile.patch
- Remove redundant alsa-1.0.25-dev_name.patch
- Generate overrides on the fly rather than by hand.

* Sun Feb 26 2012 Phil Schaffner <pschaff2@verizon.net> - 1.0.25-2.el6.elrepo
- Update overrides.

* Wed Feb 22 2012 Phil Schaffner <pschaff2@verizon.net> - 1.0.25-1.el5.elrepo
- Updated to 1.0.25.

* Sat Apr 23 2011 Philip J Perry <phil@elrepo.org> - 1.0.24-1.el5.elrepo
- Update to 1.0.24.
- Fixed build issue on RHEL5 [alsa-1.0.24-dev_name.patch]

* Sat Apr 17 2010 Philip J Perry <phil@elrepo.org> - 1.0.23-2.el5.elrepo
- Rebuilt against kernel-2.6.18-164.el5 for wider kABI compatibility.

* Sat Apr 17 2010 Philip J Perry <phil@elrepo.org> - 1.0.23-1.el5.elrepo
- Rebase to version 1.0.23.
- Update kmodtool to latest version.
- Obsoletes kmod-ctxfi.

* Wed Jun 17 2009 Philip J Perry <phil@elrepo.org>
- Renamed package to kmod-alsa. 1.0.20-1.el5.elrepo

* Sun Jun 14 2009 Philip J Perry <phil@elrepo.org>
- Rebuilt against 2.6.18-128 for release. 1.0.20-1.el5.elrepo

* Fri Jun 12 2009 Philip J Perry <phil@elrepo.org>
- Fixed the format of alsa-driver.conf source files.
- snd-trident-synth.ko from the distro is deprecated.
- http://elrepo.org/bugs/view.php?id=15

* Fri Jun 12 2009 Alan Bartlett <ajb@elrepo.org>
- Add code to install the alsa-driver.conf file.

* Wed May 13 2009 Alan Bartlett <ajb@elrepo.org>
- Initial build of the kmod packages.
