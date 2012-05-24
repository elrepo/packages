Name:		xvba-sdk
Version:	0.74
Release:	404001.1%{?dist}

Summary:	X-Video Bitstream Acceleration (XvBA) SDK by AMD
Group:		Development/Libraries
Source0:	http://developer.amd.com/Downloads/xvba-sdk-0.74-404001.tar.gz
URL:		http://developer.amd.com/ZONES/OPENSOURCE/Pages/default.aspx
License:	BSD
BuildArch:	noarch
Requires:	fglrx-x11-drv


%description
This package contains the AMD X-Video Bitstream Acceleration (XvBA)
software development kit (SDK) for Linux. It provides the header file
and the XvBA specification.

XvBA is AMD's video acceleration API for Linux. It allows Linux applications
to take advantage of the UVD engine in AMD GPUs to accelerate video decoding.


%prep
%setup -c -q


%build


%install
rm -rf %{buildroot}
# Install the header file
install -d -m755 %{buildroot}%{_includedir}
install -m644 include/amdxvba.h %{buildroot}%{_includedir}
# Create a symbolic link to the XvBAW shared library
install -d -m755 %{buildroot}%{_libdir}
ln -s %{_libdir}/fglrx/libXvBAW.so.1 %{buildroot}%{_libdir}/libXvBAW.so


%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,0755)
%doc LICENSE README doc/AMD_XvBA_Spec_v0_74_01_AES_2.pdf
%{_includedir}/amdxvba.h
%{_libdir}/libXvBAW.so


%changelog
* Thu May 17 2012 LTN Packager <packager-el6rpms@LinuxTECH.NET> - 0.74-404001.1
- spec-file imported from Mandriva
- cleaned up spec-file
- merged with spec-file from Mageia

