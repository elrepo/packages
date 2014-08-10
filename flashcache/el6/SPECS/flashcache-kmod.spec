# Define the kmod package name here.
%define kmod_name flashcache

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-431.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 3.1.2
Release: 1%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://github.com/facebook/flashcache/

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.gz
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh

# Patches.
Patch0: ELRepo-%{kmod_name}.patch

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
Flashcache is a write back block cache kernel module and was built
primarily as a block cache for InnoDB but it is general purpose and
can also be used by other applications.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{kmod_name}-%{version}
%{__cp} -a %{SOURCE5} .
%patch0 -p1
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
%{__make} KERNEL_TREE=%{_usrsrc}/kernels/%{kversion} modules

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} src/%{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} doc/%{kmod_name}-doc.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} doc/%{kmod_name}-sa-guide.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Sat Jul 26 2014 Alan Bartlett <ajb@elrepo.org> - 3.1.2-1
- Updated to the flashcache-3.1.2 sources.
- Built against kernel-2.6.32-431.el6

* Mon Dec 24 2012 Akemi Yagi <toracat@elrepo.org> - 0.0-3
- Updated to flashcache-stable_v2.
- Built against kernel 2.6.32-279.el6.

* Sat Mar 03 2012 Akemi Yagi <toracat@elrepo.org> - 0.0-2
- Packaging style now conforms to the ELRepo standard. [Alan Bartlett]
- Built against kernel 2.6.32-220.el6.

* Sun Feb 12 2012 Akemi Yagi <toracat@elrepo.org> - 0.0-1
- Initial el6 build of the kmod package.
