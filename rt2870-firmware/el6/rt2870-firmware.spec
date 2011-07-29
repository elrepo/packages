Name:		rt2870-firmware
Version:	22
Release:	1%{?dist}
Summary:	Firmware for Ralink RT28XX/RT30XX series USB network adaptors
Group:		System Environment/Kernel
License:	Redistributable, no modification permitted
URL:		http://www.ralinktech.com

Source0:	http://www.ralinktech.com.tw/data/drivers/RT2870_Firmware_V%{version}.zip

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch:	noarch

Provides:	rt2870-firmware = %{version}-%{release}

%description
This package contains the firmware required by Ralink RT28XX/RT30XX series
(RT2870/RT2770/RT3572/RT3070) USB network adaptors.

%prep
%setup -q -n RT2870_Firmware_V%{version}
sed -i 's/\r//' LICENSE.ralink-firmware.txt

%build
# Nothing to build

%install
%{__rm} -rf %{buildroot}
%{__mkdir_p} %{buildroot}/lib/firmware/
%{__install} -p -m 0644 rt2870.bin %{buildroot}/lib/firmware/

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc LICENSE.ralink-firmware.txt
/lib/firmware/rt2870.bin

%changelog
* Thu Mar 17 2010 Philip J Perry <phil@elrepo.org> - 22.1.el6.elrepo
- Rebuild for el6
- Update to version 22

* Sun Aug 30 2009 Philip J Perry <phil@elrepo.org> - 8.1.el5.elrepo
- Initial build for elrepo.org
