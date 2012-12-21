Summary:	Firmware for Atheros AR3011/AR3012 Bluetooth chipsets
Name:		ath3k-firmware
Version:	1.0
Release:	1%{?dist}
License:	Redistributable, no modification permitted
Group:		System Environment/Kernel
URL:		http://wireless.kernel.org/en/users/Drivers/ath3k

Source0: ath3k-1.fw
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build

BuildArch: noarch

%description
This package provides the firmware required for Atheros
AR3011/AR3012 Bluetooth adapters using the ath3k driver.

%prep
# nothing to prep

%build
# nothing to build

%install
%{__rm} -rf %{buildroot}
%{__install} -d %{buildroot}/lib/firmware/
%{__install} -p -m 0644 %{SOURCE0} %{buildroot}/lib/firmware/

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
/lib/firmware/ath3k-1.fw

%changelog
* Sat Dec 21 2012 Philip J Perry <phil@elrepo.org> - 1.0-1
- Initial package of Atheros ath3k firmware for elrepo.org
  [http://elrepo.org/bugs/view.php?id=336]
