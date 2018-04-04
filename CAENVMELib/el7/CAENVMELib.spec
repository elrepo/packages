Name:		CAENVMELib
Version:	2.50
Release:	2%{?dist}
Summary:	Set of functions for the control and the use of CAEN VME Bridges
Group:		Development/Libraries
License:	Other
URL:		http://www.caen.it/jsp/Template2/CaenProd.jsp?parent=43&idmod=689
Source0:	CAENVMELib-2.50.tgz

# Fix broken SONAME dependency chain
%ifarch i686
Provides:	libCAENVME.so
%endif
%ifarch x86_64
Provides:	libCAENVME.so()(64bit)
%endif

Requires:	glibc >= 2.12

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
Set of functions for the control and the use of CAEN VME Bridges

Available for C/C++ enviroment (Windows, Linux) and LabVIEW enviroment (Windows)

Supported Boards:
        Mod. V1718 / VX1718 - VME-USB2.0 Bridge
        Mod. V2718 / VX2718 - VME-PCI Optical Link Bridge
        Mod. A2818 - PCI CONET Controller
        Mod. A3818 - PCI Express CONET2 Controller

%prep
%setup -q

%build
echo done

%install
rm -rf %{buildroot}

%{__mkdir_p} %{buildroot}%{_includedir}
install -D -m 0644 include/* %{buildroot}/%{_includedir}

%{__mkdir_p} %{buildroot}%{_libdir}
%ifarch x86_64
install -D -m 0755 lib/x64/libCAENVME.so.2.50 %{buildroot}/%{_libdir}/libCAENVME.so.2.50
%endif
%ifarch i686
install -D -m 0755 lib/x86/libCAENVME.so.2.50 %{buildroot}/%{_libdir}/libCAENVME.so.2.50
%endif

pushd %{buildroot}/%{_libdir}
%{__ln_s} libCAENVME.so.2.50 libCAENVME.so
popd

%clean
rm -rf %{buildroot}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(0644,root,root,0755)
%doc CAEN_License_Agreement.txt CAENVMElibReadme.txt CAENVMElibReleaseNotes.txt
%attr(0755,root,root) %{_includedir}/*
%attr(0755,root,root) %{_libdir}/*

%changelog
* Wed Apr 04 2018 Philip J Perry <phil@elrepo.org> - 2.50-2
- Fix circular dependency [https://elrepo.org/bugs/view.php?id=837]

* Thu Jul 27 2017 Akemi Yagi <toracat@elrepo.org> - 2.50-1
- Initial build for el7.
