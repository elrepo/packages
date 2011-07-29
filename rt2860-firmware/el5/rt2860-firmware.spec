Name:		rt2860-firmware
Version:	26
Release:	1%{?dist}
Summary:	Firmware for Ralink RT28XX/RT30XX PCI/mPCI/PCIe/CardBus series network adaptors
Group:		System Environment/Kernel
License:	Redistributable, no modification permitted
URL:		http://www.ralinktech.com

Source0:	http://www.ralinktech.com.tw/data/drivers/RT2860_Firmware_V%{version}.zip

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch:	noarch

Provides:	rt2860-firmware = %{version}-%{release}

%description
This package contains the firmware required by Ralink RT28XX/RT30XX series
(RT2760/RT2790/RT2860/RT2890/RT3060/RT3062/RT3562/RT2860/RT2760/RT2890/RT2790/RT3090)
PCI/mPCI/PCIe/CardBus network adaptors.

%prep
%setup -q -n RT2860_Firmware_V%{version}
sed -i 's/\r//' LICENSE.ralink-firmware.txt

%build
# Nothing to build

%install
%{__rm} -rf %{buildroot}
%{__mkdir_p} %{buildroot}/lib/firmware/
%{__install} -p -m 0644 rt2860.bin %{buildroot}/lib/firmware/

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc LICENSE.ralink-firmware.txt
/lib/firmware/rt2860.bin

%changelog
* Mon Oct 04 2010 Philip J Perry <phil@elrepo.org> - 26-1.el5.elrepo
- Update to version 26

* Sun Aug 30 2009 Philip J Perry <phil@elrepo.org> - 11.1.el5.elrepo
- Initial build for elrepo.org
