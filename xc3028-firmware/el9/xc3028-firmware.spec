Summary:	Firmware for Xceive XC3028/XC2028 TV tuners
Name:		xc3028-firmware
Version:	0.0
Release:	1%{?dist}
License:	Xceive License
Group:		System Environment/Kernel
URL:		https://www.linuxtv.org/wiki/index.php/Xceive_XC3028/XC2028

# Firmware: xc3028-v24.fw
# https://www.driverguide.com/driver/detail.php?driverid=1271137 (working)
# http://www.twinhan.com/files/AW/BDA%20T/20080303_V1.0.6.7.zip  (dead)
# http://www.stefanringel.de/pub/20080303_V1.0.6.7.zip           (dead)
Source0:	http://www.twinhan.com/files/AW/BDA%20T/20080303_V1.0.6.7.zip
# Firmware: xc3028-v27.fw
Source1:	http://www.steventoth.net/linux/xc5000/HVR-12x0-14x0-17x0_1_25_25271_WHQL.zip
# Firmware: xc3028L-v36.fw
Source2:	http://steventoth.net/linux/hvr1400/xc3028L-v36.fw
# Firmware extraction script
Source3:	https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/scripts/extract_xc3028.pl

%if %{?_with_src:0}%{!?_with_src:1}
NoSource:	0
NoSource:	1
NoSource:	2
%endif

BuildRequires:	perl-interpreter
BuildRequires:	unzip

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build

BuildArch:	noarch

%description
This package provides the firmware required for using
Xceive XC3028/XC2028 TV tuners with the Linux kernel xc2028 driver.

%prep
%{__rm} -rf $RPM_BUILD_DIR/%{name}-%{version}
%{__mkdir_p} $RPM_BUILD_DIR/%{name}-%{version}
cd $RPM_BUILD_DIR/%{name}-%{version}
%{__unzip} -jo %{SOURCE0} 20080303_V1.0.6.7/UDXTTM6000.sys
%{__unzip} -jo %{SOURCE1} Driver85/hcw85bda.sys
%{__install} -m0644 %{SOURCE2} .
%{__install} -m0755 %{SOURCE3} .

%build
# extract the firmware files
cd $RPM_BUILD_DIR/%{name}-%{version}
%{__perl} `basename %{SOURCE3}`

%install
%{__rm} -rf %{buildroot}
%{__mkdir_p} %{buildroot}/usr/lib/firmware
cd $RPM_BUILD_DIR/%{name}-%{version}
%{__xz} -f *.fw
%{__install} -p -m 0644 *.fw.xz %{buildroot}/usr/lib/firmware

%clean
%{__rm} -rf %{buildroot}
%{__rm} -rf $RPM_BUILD_DIR/%{name}-%{version}

%files
%defattr(-,root,root,-)
/usr/lib/firmware/*

%changelog
* Tue Sep 23 2025 Tuan Hoang <tqhoang@elrepo.org> - 0.0-1
- Initial RPM package for RHEL9.
