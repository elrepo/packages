### Name: ELRepo.org Community Enterprise Linux Repository for el9
### URL: https://elrepo.org/

Summary: ELRepo.org Community Enterprise Linux Repository release file
Name: elrepo-release
Version: 9.5
Release: 2%{?dist}
License: GPLv2
Group: System Environment/Base
URL: https://elrepo.org/

Source0: elrepo.repo
Source1: RPM-GPG-KEY-elrepo.org
Source2: RPM-GPG-KEY-v2-elrepo.org
Source3: SECURE-BOOT-KEY-elrepo.org.der

BuildArch: noarch

# To prevent users installing on the wrong dist
Requires: system-release >= 9
Requires: system-release < 10

%description
This package contains yum configuration for the ELRepo.org Community Enterprise Linux Repository, as well as the public GPG keys used to sign packages.

%prep
%setup -c -T
%{__cp} -a %{SOURCE1} .
%{__cp} -a %{SOURCE2} .

# %build

%install
%{__rm} -rf %{buildroot}
%{__install} -Dpm 0644 %{SOURCE0} %{buildroot}%{_sysconfdir}/yum.repos.d/elrepo.repo
%{__install} -Dpm 0644 %{SOURCE1} %{buildroot}%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-elrepo.org
%{__install} -Dpm 0644 %{SOURCE2} %{buildroot}%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-v2-elrepo.org
%{__install} -Dpm 0644 %{SOURCE3} %{buildroot}%{_sysconfdir}/pki/elrepo/SECURE-BOOT-KEY-elrepo.org.der

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%pubkey RPM-GPG-KEY-elrepo.org
%pubkey RPM-GPG-KEY-v2-elrepo.org
%config(noreplace) %{_sysconfdir}/yum.repos.d/elrepo.repo
%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-elrepo.org
%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-v2-elrepo.org
%dir %{_sysconfdir}/pki/elrepo/
%{_sysconfdir}/pki/elrepo/SECURE-BOOT-KEY-elrepo.org.der

%changelog
* Sat Jan 11 2025 Philip J Perry <phil@elrepo.org> - 9.5-2
- Fix elrepo.repo file for multiple gpg keys

* Fri Jan 10 2025 Philip J Perry <phil@elrepo.org> - 9.5-1
- Add new v2 (4096-bit) RPM signing key
  
* Sat Jul 09 2022 Philip J Perry <phil@elrepo.org> - 9.1-1
- Remove dependency on glibc
  [https://elrepo.org/bugs/view.php?id=1242]
- Enable countme feature.

* Mon Jan 24 2022 Philip J Perry <phil@elrepo.org> - 9.0-1
- Initial elrepo-release package for rhel9
