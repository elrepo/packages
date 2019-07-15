### Name: ELRepo.org Community Enterprise Linux Repository for el7
### URL: http://elrepo.org/

Summary: ELRepo.org Community Enterprise Linux Repository release file
Name: elrepo-release
Version: 7.0
Release: 4%{?dist}
License: GPLv2
Group: System Environment/Base
URL: http://elrepo.org/

Source0: elrepo.repo
Source1: RPM-GPG-KEY-elrepo.org
Source2: SECURE-BOOT-KEY-elrepo.org.der

BuildArch: noarch

# To prevent users installing on the wrong dist
Requires: glibc = 2.17

%description
This package contains yum configuration for the ELRepo.org Community Enterprise Linux Repository, as well as the public GPG keys used to sign packages.

%prep
%setup -c -T
%{__cp} -a %{SOURCE1} .

# %build

%install
%{__rm} -rf %{buildroot}
%{__install} -Dpm 0644 %{SOURCE0} %{buildroot}%{_sysconfdir}/yum.repos.d/elrepo.repo
%{__install} -Dpm 0644 %{SOURCE1} %{buildroot}%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-elrepo.org
%{__install} -Dpm 0644 %{SOURCE2} %{buildroot}%{_sysconfdir}/pki/elrepo/SECURE-BOOT-KEY-elrepo.org.der

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%pubkey RPM-GPG-KEY-elrepo.org
%dir %{_sysconfdir}/yum.repos.d/
%config(noreplace) %{_sysconfdir}/yum.repos.d/elrepo.repo
%dir %{_sysconfdir}/pki/rpm-gpg/
%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-elrepo.org
%dir %{_sysconfdir}/pki/elrepo/
%{_sysconfdir}/pki/elrepo/SECURE-BOOT-KEY-elrepo.org.der

%changelog
* Mon Jul 15 2019 Philip J Perry <phil@elrepo.org> - 7.0-4
- Remove stale mirror site.

* Sun Jul 23 2017 Philip J Perry <phil@elrepo.org> - 7.0-3
- Remove stale mirror site.

* Tue Jul 08 2014 Philip J Perry <phil@elrepo.org> - 7.0-2
- Add Secure Boot public key.

* Tue Jun 10 2014 Philip J Perry <phil@elrepo.org> - 7.0-1
- Rebuilt for rhel-7.0 release.
- Changed requires to glibc to allow for kernel removal.
  [http://elrepo.org/bugs/view.php?id=463]

* Tue May 20 2014 Philip J Perry <phil@elrepo.org> - 7-0
- Initial elrepo-release package for rhel7rc.
