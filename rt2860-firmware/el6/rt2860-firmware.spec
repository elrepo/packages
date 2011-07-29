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
This package contains the firmware required by Ralink RT28XX/RT30XX
PCI/mPCI/PCIe/CardBus series network adaptors.

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
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc LICENSE.ralink-firmware.txt
/lib/firmware/rt2860.bin

%changelog
* Thu Jan 20 2011 Akemi Yagi <toracat@elrepo.org> - 26.1.el6.elrepo
- Initial EL6 build for elrepo.org
