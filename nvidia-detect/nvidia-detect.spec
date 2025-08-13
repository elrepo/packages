Name:    nvidia-detect
Version: 580.76.05
Release: 1%{?dist}
Group:   Applications/System
License: GPLv2
Summary: NVIDIA graphics card detection utility
URL:     https://github.com/elrepo/packages/tree/master/nvidia-detect

BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)

Source0:  %{name}-%{version}.tar.gz

Requires:	hwdata
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
* Tue Aug 12 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.76.05-1
- Add support for detection of devices added to 580.76.05 driver release

* Thu Aug 07 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.181-1
- Add support for detection of devices added to 570.181 driver release

* Fri Jul 18 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.172.08-1
- Add support for detection of devices added to 570.172.08 driver release

* Thu Jun 19 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.169-1
- Add support for detection of devices added to 570.169 driver release

* Tue May 20 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.153.02-1
- Add support for detection of devices added to 570.153.02 driver release

* Sat Apr 26 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.144-1
- Add support for detection of devices added to 570.144 driver release

* Sat Mar 29 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.133.07-1
- Add support for detection of devices added to 570.133.07 driver release
- Add support for RHEL 10

* Tue Jan 28 2025 Tuan Hoang <tqhoang@elrepo.org> - 550.144.03-1
- Add support for detection of devices added to 550.144.03 driver release

* Thu Dec 26 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.142-1
- Add support for detection of devices added to 550.142 driver release

* Tue Nov 19 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.135-1
- Add support for detection of devices added to 550.135 driver release

* Tue Oct 22 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.127.05-1
- Add support for detection of devices added to 550.127.05 driver release

* Sat Oct 05 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.120-1
- Add support for detection of devices added to 550.120 driver release

* Thu Aug 01 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.107.02-1
- Add support for detection of devices added to 550.107.02 driver release

* Wed Jun 05 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.90.07-1
- Add support for detection of devices added to 550.90.07 driver release

* Sat Apr 20 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.76-1
- Add support for detection of devices added to 550.76 driver release

* Thu Mar 21 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.67-1
- Add support for detection of devices added to 550.67 driver release

* Fri Mar 01 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.54.14-2
- Add support for RHEL9

* Mon Feb 26 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.54.14-1
- Add support for detection of devices added to 550.54.14 driver release

* Wed Jan 17 2024 Tuan Hoang <tqhoang@elrepo.org> - 535.154.05-1
- Add support for detection of devices added to 535.154.05 driver release
- Add additional pci ids from laptopvideo2go.com
- Resort current pci ids

* Sun Jan 22 2023 Philip J Perry <phil@elrepo.org> - 525.85.05-1
- Add support for detection of devices added to 525.85.05 driver release

* Sat Jul 23 2022 Philip J Perry <phil@elrepo.org> - 515.57-1
- Add support for detection of devices added to 515.57 driver release

* Wed Mar 30 2022 Philip J Perry <phil@elrepo.org> - 510.60.02-1
- Add support for detection of devices added to 510.60.02 driver release
- Fix typo in usage() [https://github.com/elrepo/packages/issues/244]

* Thu Feb 03 2022 Philip J Perry <phil@elrepo.org> - 510.47.03-1
- Add support for detection of devices added to 510.47.03 driver release
- Add support for 470.xx legacy driver

* Wed Feb 02 2022 Philip J Perry <phil@elrepo.org> - 470.103.01-1
- Add support for detection of devices added to 470.103.01 driver release

* Thu Dec 02 2021 Philip J Perry <phil@elrepo.org> - 470.86-1
- Add support for detection of devices added to 470.86 driver release

* Wed Jun 23 2021 Philip J Perry <phil@elrepo.org> - 460.84-1
- Add support for detection of devices added to 460.84 driver release
- Call pci_fill_info() to retrieve device_class data
  [https://elrepo.org/bugs/view.php?id=1112]

* Tue May 11 2021 Philip J Perry <phil@elrepo.org> - 460.73.01-1
- Add support for detection of devices added to 460.73.01 driver release
- Drop support for RHEL6

* Mon Mar 30 2020 Philip J Perry <phil@elrepo.org> - 440.64-1
- Add support for detection of devices added to 440.64 driver release
- Add support for RHEL8

* Fri Nov 29 2019 Philip J Perry <phil@elrepo.org> - 440.36-1
- Add support for detection of devices added to 440.36 driver release

* Tue Jul 30 2019 Philip J Perry <phil@elrepo.org> - 430.40-1
- Add support for detection of devices added to 430.40 driver release

* Wed Jul 10 2019 Philip J Perry <phil@elrepo.org> - 430.34-1
- Add support for detection of devices added to 430.34 driver release

* Tue Jun 11 2019 Philip J Perry <phil@elrepo.org> - 430.26-1
- Add support for detection of devices added to 430.26 driver release

* Sat May 11 2019 Philip J Perry <phil@elrepo.org> - 418.74-2
- Fix device ID for RTX 2070

* Sat May 11 2019 Philip J Perry <phil@elrepo.org> - 418.74-1
- Add support for detection of devices added to 418.74 driver release

* Sat Mar 02 2019 Philip J Perry <phil@elrepo.org> - 418.43-1
- Add support for detection of devices added to 418.43 driver release

* Sat Jan 05 2019 Philip J Perry <phil@elrepo.org> - 410.93-1
- Add support for detection of devices added to 410.93 driver release

* Fri Nov 16 2018 Philip J Perry <phil@elrepo.org> - 410.78-1
- Add support for detection of devices added to 410.78 driver release

* Tue Oct 16 2018 Philip J Perry <phil@elrepo.org> - 410.66-1
- Update to 410.66
- Add support for 390.xx legacy driver
- Drop 32-bit OS support

* Mon Aug 20 2018 Philip J Perry <phil@elrepo.org> - 390.77-1
- Update to 390.77

* Wed Jul 11 2018 Philip J Perry <phil@elrepo.org> - 390.67-1
- Add support for detection of devices added to 390.67 driver release

* Sun May 20 2018 Philip J Perry <phil@elrepo.org> - 390.59-1
- Add support for detection of devices added to 390.59 driver release
- Update Xorg Video Driver ABI versions

* Sat Apr 14 2018 Philip J Perry <phil@elrepo.org> - 390.48-2
- Fix error handling [http://elrepo.org/bugs/view.php?id=839]

* Sat Mar 31 2018 Philip J Perry <phil@elrepo.org> - 390.48-1
- Add support for detection of devices added to 390.48 driver release

* Wed Jan 31 2018 Philip J Perry <phil@elrepo.org> - 390.25-1
- Add support for detection of devices added to 390.25 driver release

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
