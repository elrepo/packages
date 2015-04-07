# Define the kmod package name here.
%define kmod_name pf_ring

### BEWARE: The kernel version is also mentioned in kmodtool !
# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion:%define kversion 2.6.32-504.el6.%{_target_cpu}}

Name:           %{kmod_name}-kmod
Version:        6.0.2
Release:        1%{?dist}
Group:          System Environment/Kernel
License:        GPLv2
Summary:        %{kmod_name} kernel module(s)
URL:            http://www.ntop.org/products/pf_ring/

BuildRequires:  redhat-rpm-config
ExclusiveArch:  i686 x86_64

# Sources.
Source0:   http://sourceforge.net/projects/ntop/files/PF_RING/PF_RING-%{version}.tar.gz
Source5:   GPL-v2.0.txt
Source10:  kmodtool-%{kmod_name}-el6.sh
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n PF_RING-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
pushd kernel
%{__make} -C "${KSRC}" %{?_smp_mflags} \
    EXTRA_CFLAGS='-I%{_builddir}/PF_RING-%{version}/kernel -DSVN_REV="\"exported\""' \
    modules M=$PWD

%install
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=/lib/modules/%{kversion}/extra/%{kmod_name}
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__install} -d %{buildroot}/${INSTALL_MOD_DIR}
%{__install} kernel/%{kmod_name}.ko %{buildroot}/${INSTALL_MOD_DIR}
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Mon Apr  6 2015 Jakov Sosic <jsosic@gmail.com> - 0:6.0.2-1
- Initial kmod package
