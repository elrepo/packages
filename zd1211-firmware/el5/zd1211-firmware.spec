Summary:	Firmware for ZyDAS ZD1211 based network adapters
Name:		zd1211-firmware
Version:	1.4
Release:	2%{?dist}
License:	GPLv2
Group:		System Environment/Kernel
URL:		http://linuxwireless.org/en/users/Drivers/zd1211rw

Source0: http://sourceforge.net/projects/zd1211/files/zd1211-firmware/%{version}/zd1211-firmware-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build

BuildArch: noarch

%description
This package provides the firmware required for using
ZyDAS ZD1211(b) 802.11a/b/g USB WLAN based network adapters
with the Linux kernel zd1211rw driver.

%prep
%setup -q -n %{name}

%build
# nothing to build

%install
%{__rm} -rf %{buildroot}
%{__mkdir_p} %{buildroot}/lib/firmware/zd1211/
%{__install} -p -m 0644 zd1211* %{buildroot}/lib/firmware/zd1211/

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%doc COPYING README
%dir /lib/firmware/zd1211
/lib/firmware/zd1211/*

%changelog
*Sat Aug 01 2009 Philip J Perry <phil@elrepo.org> 1.4-2.elrepo
- Update package description.
- Fix typos in SPEC file.

* Fri Jul 31 2009 Philip J Perry <phil@elrepo.org>
- Initial RPM package. 1.4-1.elrepo
