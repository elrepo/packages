Name:    nvidia-detect
Version: 340.24
Release: 1%{?dist}
Group:   Applications/System
License: GPLv2
Summary: NVIDIA graphics card detection utility
URL:     https://github.com/elrepo/packages/tree/master/nvidia-detect

BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)

Source0:  %{name}-%{version}.tar.bz2

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
%doc COPYING README
%{_bindir}/nvidia-detect

%changelog
* Fri Jul 11 2014 Philip J Perry <phil@elrepo.org> - 340.24-1
- Add support for detection of devices added to 340.24 driver release

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
