Summary:	Firmware for Atheros AR7010/AR9271 series USB wireless network adapters
Name:		ath9k_htc-firmware
Version:	1.3
Release:	1%{?dist}
License:	Redistributable, no modification permitted
Group:		System Environment/Kernel
URL:		http://linuxwireless.org/en/users/Drivers/ath9k_htc

Source0: http://linuxwireless.org/download/htc_fw/1.3/htc_7010.fw
Source1: http://linuxwireless.org/download/htc_fw/1.3/htc_9271.fw
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build

BuildArch: noarch

%description
This package provides the firmware required for Atheros AR7010/AR9271
series USB wireless network adapters using the ath9k_htc driver.

%prep
# nothing to prep

%build
# nothing to build

%install
%{__rm} -rf %{buildroot}
%{__install} -d %{buildroot}/lib/firmware/
%{__install} -p -m 0644 %{SOURCE0} %{buildroot}/lib/firmware/
%{__install} -p -m 0644 %{SOURCE1} %{buildroot}/lib/firmware/

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
/lib/firmware/htc_7010.fw
/lib/firmware/htc_9271.fw

%changelog
* Sat May 12 2012 Philip J Perry <phil@elrepo.org> - 1.3-1
- Initial package of Atheros ath9k_htc firmware for elrepo.org
  [http://elrepo.org/bugs/view.php?id=273]
