### Name: ELRepo.org Community Enterprise Linux Repository for el8
### URL: http://elrepo.org/

Summary: ELRepo.org Community Enterprise Linux Repository release file
Name: elrepo-release
Version: 8.1
Release: 1%{?dist}
License: GPLv2
Group: System Environment/Base
URL: http://elrepo.org/

Source0: elrepo.repo
Source1: RPM-GPG-KEY-elrepo.org
Source2: SECURE-BOOT-KEY-elrepo.org.der

BuildArch: noarch

# To prevent users installing on the wrong dist
Requires: glibc = 2.28

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
* Sun Feb 16 2020 Philip J Perry <phil@elrepo.org> - 8.1-1
- Remove unknown configuration option protect = 0
- Replace stale mirror site.

* Mon Jul 15 2019 Philip J Perry <phil@elrepo.org> - 8.0-2
- Remove stale mirror site.

* Wed May 08 2019 Philip J Perry <phil@elrepo.org> - 8.0-1
- Initial elrepo-release package for rhel-8.0 release.
