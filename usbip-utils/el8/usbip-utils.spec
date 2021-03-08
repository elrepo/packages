Name: usbip-utils
Version: 0.0
Release: 1%{?dist}
Summary: Utility applications for usbip.

Group: System Environment/Base	
License: GPLv2	
URL: http://www.kernel.org
BuildRequires: sysfsutils autoconf automake libtool libsysfs-devel
BuildRequires: libudev-devel
ExclusiveArch: i386 x86_64

# Sources
Source0: usbip-%{version}.tar.gz
Source1: usbip.h

# Patches
# Patch1: usbip_common_h.patch

%description
The %{name} package contains utilities for usbip,
usbip and usbipd

%prep
%setup -q -n usbip-%{version}
# %patch1 -p1

%build
%{__cp} %{SOURCE1} userspace/libsrc/
pushd userspace
./autogen.sh
## %configure --with-tcp-wrappers=no --with-usbids-dir=/
%configure
%{__make} -s %{?_smp_mflags}
popd

%install
%{__rm} -rf %{buildroot}
pushd userspace
%{__make} -s install DESTDIR="%{buildroot}"
%{__mkdir_p} $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}
%{__install} -m 644 COPYING %{buildroot}%{_docdir}/%{name}-%{version}/
%{__install} -m 644 AUTHORS %{buildroot}%{_docdir}/%{name}-%{version}/
%{__install} -m 644 INSTALL %{buildroot}%{_docdir}/%{name}-%{version}/
%{__install} -m 644 README %{buildroot}%{_docdir}/%{name}-%{version}/
popd

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc %{_docdir}/%{name}-%{version}/
## %doc COPYING README AUTHORS INSTALL
%doc %{_mandir}/man8/usbip.8.gz
%doc %{_mandir}/man8/usbipd.8.gz
%dir %{_includedir}/usbip/
%{_includedir}/usbip/usbip_common.h
%{_includedir}/usbip/usbip_host_driver.h
%{_includedir}/usbip/vhci_driver.h
%{_includedir}/usbip/list.h
%{_includedir}/usbip/sysfs_utils.h
%{_includedir}/usbip/usbip_host_common.h
%{_libdir}/libusbip.a
%{_libdir}/libusbip.la
%{_libdir}/libusbip.so
%{_libdir}/libusbip.so.0
%{_libdir}/libusbip.so.0.0.1
%{_sbindir}/usbip
%{_sbindir}/usbipd

%changelog
* Wed Dec 18 2019 Akemi Yagi <toracat@elrepo.org> - 0.0-1
- Initial build for EL8
