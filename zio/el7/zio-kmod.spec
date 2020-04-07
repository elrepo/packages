# Define the kmod package name here.
%define kmod_name zio
%define kmods_built %{kmod_name} %{kmod_name}-chardev %{kmod_name}-fakedev %{kmod_name}-trivial %{kmod_name}-write-eeprom

%define tagversion 1.2
%define packageversion %(echo %{tagversion} | sed 's/-/./g')

%define gitversion 0-g3d9cee9
%define sourceversion %(echo %{gitversion} | sed 's/-/./g')


# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-957.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 0.1
Release: %{packageversion}.%{sourceversion}.5%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.ohwr.org/projects/zio

BuildRequires: perl git emacs gawk texinfo texinfo-tex
BuildRequires: redhat-rpm-config
ExclusiveArch: x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.bz2
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el7.sh

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%package headers
Summary:	header files for %{kmod_name}-%{version}

%description headers
Provides the %{kmod_name} kernel headers.

%package tools
Summary:        tools for %{kmod_name}-%{version}

%description tools
Provides the %{kmod_name} kernel module tools.

%prep
%setup -q -n %{kmod_name}-%{version}
for thiskmod in %{kmods_built}; do
    echo "override ${thiskmod} * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
done

GIT_VERSION=$(git describe --dirty --long --tags)
ZIO_MAJOR_VERSION=$(echo ${GIT_VERSION} | cut -d '-' -f 2 | cut -d '.' -f 1; )
ZIO_MINOR_VERSION=$(echo ${GIT_VERSION} | cut -d '-' -f 2 | cut -d '.' -f 2; )
ZIO_PATCH_VERSION=$(echo ${GIT_VERSION} | cut -d '-' -f 3)

cat << EOF > zio-version-env
GIT_VERSION=${GIT_VERSION}
ZIO_VERSION=${GIT_VERSION}
ZIO_MAJOR_VERSION=${ZIO_MAJOR_VERSION}
ZIO_MINOR_VERSION=${ZIO_MINOR_VERSION}
ZIO_PATCH_VERSION=${ZIO_PATCH_VERSION}
EOF

echo "${ZIO_MAJOR_VERSION}.${ZIO_MINOR_VERSION}" > version


%build
%{__make} %{?_smp_mflags} LINUX=%{_usrsrc}/kernels/%{kversion}

cd doc
%{__make}
cd ..

# Cleanup the mappings
for line in $(cat modules.order); do
  kmod=$(basename ${line})
  echo "kernel/extra/${kmod}" >> zio.order
done

awk '{print $1 "\t" $2 "\textra/zio\t" $4}' Module.symvers > zio.symvers


%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/

%{__install} buffers/zio-buf-vmalloc.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} drivers/zio-ad788x.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} drivers/zio-fake-dtc.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} drivers/zio-gpio.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} drivers/zio-irq-tdc.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} drivers/zio-loop.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} drivers/zio-mini.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} drivers/zio-vmk8055.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} drivers/zio-zero.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} triggers/zio-trig-hrt.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} triggers/zio-trig-irq.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} triggers/zio-trig-timer.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} zio.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/

%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} zio.order %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/modules.order
%{__install} zio.symvers %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/Module.symvers


# strip the modules(s)
find %{buildroot} -type f -name \*.ko -exec %{__strip} --strip-debug \{\} \;

# Sign the modules(s)
%if %{?_with_modsign:1}%{!?_with_modsign:0}
# If the module signing keys are not defined, define them here.
%{!?privkey: %define privkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.priv}
%{!?pubkey: %define pubkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.der}
for module in $(find %{buildroot} -type f -name \*.ko);
do %{__perl} /usr/src/kernels/%{kversion}/scripts/sign-file \
sha256 %{privkey} %{pubkey} $module;
done
%endif

# Install headers
%{__install} -d %{buildroot}%{_usrsrc}/kmods/%{kmod_name}-%{version}/linux
%{__cp} include/linux/* %{buildroot}%{_usrsrc}/kmods/%{kmod_name}-%{version}/linux
%{__install} version %{buildroot}%{_usrsrc}/kmods/%{kmod_name}-%{version}/version
%{__install} zio-version-env %{buildroot}%{_usrsrc}/kmods/%{kmod_name}-%{version}/zio-version-env
%{__install} zio.order %{buildroot}%{_usrsrc}/kmods/%{kmod_name}-%{version}/modules.order
%{__install} zio.symvers %{buildroot}%{_usrsrc}/kmods/%{kmod_name}-%{version}/Module.symvers


# Install tools
%{__install} -d %{buildroot}%{_sbindir}
%{__install} tools/test-dtc %{buildroot}%{_sbindir}/
%{__install} tools/zio-cat-file %{buildroot}%{_sbindir}/
%{__install} tools/zio-dump %{buildroot}%{_sbindir}/

# Install infopage
%{__install} -d %{buildroot}%{_infodir}
%{__install} doc/zio-manual.info %{buildroot}%{_infodir}/

# Install docs
%{__install} doc/zio-manual.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__cp} -r Documentation/ABI %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

%clean
%{__rm} -rf %{buildroot}

%files headers
%defattr(0644,root,root,0755)
%{_usrsrc}/kmods/%{kmod_name}-%{version}

%files tools
%defattr(0644,root,root,0755)
%attr(0755,root,root) %{_sbindir}/*

%changelog
* Sun Jan 13 2019 Akemi Yagi <toracat@elrepo.org> - 0.1-1.2.0.g3d9cee9.5.el7_6.elrepo
- Rebuilt against RHEL 7.6 kernel

* Tue Aug 29 2017 Akemi Yagi <toracat@elrepo.org> - 0.1-1.2.0.g3d9cee9.5
- Rebuilt afainst the RHEL 7.4 kernel

* Mon Jul 10 2017 Akemi Yagi <toracat@elrepo.org> - 0.1-1.2.0.g3d9cee9.4
- Updated to 0.1 (submitted by jcpunk)

* Thu Jun 29 2017 Akemi Yagi <toracat@elrepo.org> - 0.0-3
-Initial build for el7.
