### Name: ELRepo.org Community Enterprise Linux Repository for el10
### URL: https://elrepo.org/

Summary: ELRepo.org Community Enterprise Linux Repository release file
Name: elrepo-release
Version: 10.0
Release: 1%{?dist}
License: GPLv2
Group: System Environment/Base
URL: https://elrepo.org/

Source0: elrepo.repo
Source1: RPM-GPG-KEY-v2-elrepo.org
Source2: SECURE-BOOT-KEY-elrepo.org.der

BuildArch: noarch

# To prevent users installing on the wrong dist
Requires: system-release >= 10
Requires: system-release < 11

%description
This package contains yum configuration for the ELRepo.org Community Enterprise Linux Repository, as well as the public GPG keys used to sign packages.

%prep
%setup -c -T
%{__cp} -a %{SOURCE1} .

# %build

%install
%{__rm} -rf %{buildroot}
%{__install} -Dpm 0644 %{SOURCE0} %{buildroot}%{_sysconfdir}/yum.repos.d/elrepo.repo
%{__install} -Dpm 0644 %{SOURCE1} %{buildroot}%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-v2-elrepo.org
%{__install} -Dpm 0644 %{SOURCE2} %{buildroot}%{_sysconfdir}/pki/elrepo/SECURE-BOOT-KEY-elrepo.org.der

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%pubkey RPM-GPG-KEY-v2-elrepo.org
%config(noreplace) %{_sysconfdir}/yum.repos.d/elrepo.repo
%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-v2-elrepo.org
%dir %{_sysconfdir}/pki/elrepo/
%{_sysconfdir}/pki/elrepo/SECURE-BOOT-KEY-elrepo.org.der

%changelog
* Wed Feb 26 2025 Akemi Yagi <toracat@elrepo.org> - 10.0-1
- Initial elrepo-release package for rhel10
