Summary:	Firmware for Chelsio Communications Inc network adapters
Name:		cxgb3-firmware
Version:	1.1.5
Release:	20200721.1%{?dist}
License:	GPLv2
Group:		System Environment/Kernel
URL:		https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/tree/cxgb3

Source0: cxgb3-firmware-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build

BuildArch: noarch

%description
This package provides the firmware required for using
Chelsio Communications Inc T320 10GbE Dual Port Adapters
with the Linux kernel cxgb3 driver.

%prep
%setup -q -n %{name}-%{version}

%build
# nothing to build

%install
%{__rm} -rf %{buildroot}
%{__mkdir_p} %{buildroot}/usr/lib/firmware/cxgb3/
%{__install} -p -m 0644 cxgb3/*.bin %{buildroot}/usr/lib/firmware/cxgb3/

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%dir /usr/lib/firmware/cxgb3
/usr/lib/firmware/cxgb3/*

%changelog
* Tue Feb 21 2023 Philip J Perry <phil@elrepo.org> - 1.1.5-20200721.1
- Initial RPM package for RHEL9.
