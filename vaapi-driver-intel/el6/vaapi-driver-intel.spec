%define name	vaapi-driver-intel
%define oname	intel-driver
%define version	1.0.15
%define rel	1

Summary:	Is the VA-API implementation for Intel G45 chipsets
Name:		%{name}
Version:	%{version}
Release:	4%{?dist}
Group:		Video
License:	GPLv2+
URL:		http://cgit.freedesktop.org/vaapi/intel-driver/
Source:		http://cgit.freedesktop.org/vaapi/intel-driver/snapshot/%{oname}-%{version}.zip
BuildRequires:	libva-devel => 1.0.15
BuildRequires:  autoconf automake libtool pkgconfig
BuildRequires:  libdrm-devel => 2.4.31
BuildRequires:  libX11-devel
Provides:	%{oname} = %{version}-%{release}

%description
libva-driver-intel is the VA-API implementation for Intel G45 chipsets
and Intel HD Graphics for Intel Core processor family.


%prep
%setup -q -n %oname-%version
#% apply_patches

%build
autoreconf -v --install
%configure
make

%install
rm -rf %{buildroot}
make install DESTDIR=$RPM_BUILD_ROOT
rm -f %{buildroot}%{_libdir}/dri/*.la

%files
%defattr(-,root,root)
%doc AUTHORS NEWS
%{_libdir}/dri/*_drv_video.so


%changelog
* Sun Feb 26 2012 Phil Schaffner <pschaff2@verison.net> - 1.0.15-3
Update for Mesa 8.0.1

* Sun Jan 01 2012 Phil Schaffner <p.r.schaffner@ieee.org> - 1.0.15-2
- Initial ELRepo build

* Sat Nov 05 2011 Alexander Khrukin <akhrukin@mandriva.org> 1.0.15-2
+ Revision: 719700
- description fix removed all vdpau comments
- bs libva-devel fix again
- testing libva again
- release bump for testing libva-devel 1.0.15
- BuildReq fix for libva
- spec fixes for mandriva package policy
- imported package vaapi-driver-intel

