Name:    nvidia-detect
Version: 384.111
Release: 1%{?dist}
Group:   Applications/System
License: GPLv2
Summary: NVIDIA graphics card detection utility
URL:     https://github.com/elrepo/packages/tree/master/nvidia-detect

BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)

Source0:  %{name}-%{version}.tar.gz

Requires:	hwdata
Requires:	pciutils
BuildRequires:	pciutils-devel >= 2.2.4

%description
A utility to detect NVIDIA graphics cards.

%prep
%setup -q

%build
%{__make}

%install
%{__rm} -rf %{buildroot}
%{__mkdir_p} %{buildroot}%{_bindir}/
%{__install} -p -m 0755 nvidia-detect %{buildroot}%{_bindir}/

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,0755)
%doc CHANGELOG COPYING README
%{_bindir}/nvidia-detect

%changelog
* Fri Jan 05 2018 Philip J Perry <phil@elrepo.org> - 384.111-1
- Add support for detection of devices added to 384.111 driver release

* Fri Nov 03 2017 Philip J Perry <phil@elrepo.org> - 384.98-1
- Add support for detection of devices added to 384.98 driver release

* Thu Jul 27 2017 Philip J Perry <phil@elrepo.org> - 384.90-1
- Add support for detection of devices added to 384.90 driver release
- Fix Xorg Video Driver ABI for 367xx driver

* Thu Jul 27 2017 Philip J Perry <phil@elrepo.org> - 384.59-1
- Add support for detection of devices added to 384.59 driver release

* Thu May 11 2017 Philip J Perry <phil@elrepo.org> - 375.66-1
- Add support for detection of devices added to 375.66 driver release
- Reinstate support for GRID K520 to latest driver

* Sun Apr 23 2017 Philip J Perry <phil@elrepo.org> - 375.39-2
- Add support for 367.xx legacy package
- Change Optimus text [https://www.centos.org/forums/viewtopic.php?f=49&t=61853]

* Wed Feb 22 2017 Philip J Perry <phil@elrepo.org> - 375.39-1
- Add support for detection of devices added to 375.39 driver release

* Sat Dec 17 2016 Philip J Perry <phil@elrepo.org> - 375.26-1
- Add support for detection of devices added to 375.26 driver release

* Sun Nov 20 2016 Philip J Perry <phil@elrepo.org> - 375.20-1
- Add support for detection of devices added to 375.20 driver release
- Drop support for RHEL5

* Wed Aug 24 2016 Philip J Perry <phil@elrepo.org> - 367.44-1
- Add support for detection of devices added to 367.44 driver release

* Sat Jun 18 2016 Philip J Perry <phil@elrepo.org> - 367.27-1
- Add support for detection of devices added to 367.27 driver release

* Wed May 25 2016 Philip J Perry <phil@elrepo.org> - 361.45.11-1
- Add support for detection of devices added to 361.45.11 driver release

* Thu Mar 31 2016 Philip J Perry <phil@elrepo.org> - 361.42-1
- Add support for detection of devices added to 361.42 driver release

* Sun Mar 06 2016 Philip J Perry <phil@elrepo.org> - 361.28-1
- Add support for detection of devices added to 361.28 driver release

* Wed Nov 25 2015 Philip J Perry <phil@elrepo.org> - 352.63-1
- Add support for detection of devices added to 352.63 driver release
- Update Xorg Video Driver ABI versions

* Sat Oct 17 2015 Philip J Perry <phil@elrepo.org> - 352.55-1
- Add support for detection of devices added to 352.55 driver release

* Sat Aug 29 2015 Philip J Perry <phil@elrepo.org> - 352.41-1
- Add support for detection of devices added to 352.41 driver release

* Sat Aug 01 2015 Philip J Perry <phil@elrepo.org> - 352.30-1
- Add support for detection of devices added to 352.30 driver release

* Wed Jun 17 2015 Philip J Perry <phil@elrepo.org> - 352.21-1
- Add support for detection of devices added to 352.21 driver release

* Wed Apr 08 2015 Philip J Perry <phil@elrepo.org> - 346.59-1
- Add support for detection of devices added to 346.59 driver release

* Wed Feb 25 2015 Philip J Perry <phil@elrepo.org> - 346.47-1
- Add support for detection of devices added to 346.47 driver release

* Sat Feb 14 2015 Philip J Perry <phil@elrepo.org> - 346.35-2
- Make output terse returning only package name by default
- Add verbose option to restore previously verbose output

* Sat Jan 17 2015 Philip J Perry <phil@elrepo.org> - 346.35-1
- Add support for detection of devices added to 346.35 driver release

* Tue Dec 16 2014 Philip J Perry <phil@elrepo.org> - 343.36-1
- Add support for detection of devices added to 343.36 driver release
- Add support for 340.xx legacy driver

* Thu Nov 06 2014 Philip J Perry <phil@elrepo.org> - 340.58-1
- Add support for detection of devices added to 340.58 driver release

* Sat Aug 16 2014 Philip J Perry <phil@elrepo.org> - 340.32-1
- Add support for detection of devices added to 340.32 driver release

* Thu Jul 17 2014 Philip J Perry <phil@elrepo.org> - 340.24-1
- Add support for detection of devices added to 340.24 driver release
- Update Xorg Video Driver ABI versions

* Sun Jul 06 2014 Philip J Perry <phil@elrepo.org> - 331.89-1
- Add support for detection of devices added to 331.89 driver release

* Mon Jun 23 2014 Philip J Perry <phil@elrepo.org> - 331.79-1
- Bump release to match latest upstream release, no new devices added
- Add support for RHEL 7
- Fix Xorg detection

* Wed Apr 09 2014 Philip J Perry <phil@elrepo.org> - 331.67-1
- Add support for detection of devices added to 331.67 driver release

* Wed Mar 05 2014 Philip J Perry <phil@elrepo.org> - 331.49-2
- Add mask to device_class to detect all display classes
  [http://elrepo.org/bugs/view.php?id=462]

* Wed Feb 19 2014 Philip J Perry <phil@elrepo.org> - 331.49-1
- Add support for detection of devices added to 331.49 driver release

* Sat Jan 18 2014 Philip J Perry <phil@elrepo.org> - 331.38-1
- Add support for detection of devices added to 331.38 driver release
- Update Xorg ABIs

* Wed Nov 20 2013 Philip J Perry <phil@elrepo.org> - 331.20-2
- Fix Xorg detection issue [http://elrepo.org/bugs/view.php?id=426]

* Mon Nov 11 2013 Philip J Perry <phil@elrepo.org> - 331.20-1
- Add support for detection of devices added to 331.20 driver release.
- Move many features into functions.
- Fix device ID listing bug [http://elrepo.org/bugs/view.php?id=423]
- Add --list option to list all supported NVIDIA devices
- Add checks to see if we are running on RHEL
- Add --xorg option for Xorg compatibility check

* Mon Aug 05 2013 Philip J Perry <phil@elrepo.org> - 325.15-1
- Add support for detection of devices added to 325.15 driver release.

* Mon Jul 01 2013 Philip J Perry <phil@elrepo.org> - 319.32-1
- Add support for detection of devices added to 319.32 driver release.

* Mon May 06 2013 Philip J Perry <phil@elrepo.org> - 319.17-1
- Add support for detection of devices added to 319.17 driver release.
- Remove pci_fill_info(), it is not required to get device_class.
- Add versioned BR for pciutils-devel for PCI_FILL_CLASS.
- Add some simple error handling.
- Clean up text output.

* Sat Mar 09 2013 Philip J Perry <phil@elrepo.org> - 310.40-1
- Add support for detection of devices added to 310.40 driver release.
- Add support for detection of Optimus hardware configurations.

* Wed Feb 06 2013 Philip J Perry <phil@elrepo.org> - 310.32-1
- Initial build of the package.
