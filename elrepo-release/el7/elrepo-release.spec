### Name: ELRepo.org Community Enterprise Linux Repository for el7
### URL: http://elrepo.org/

Summary: ELRepo.org Community Enterprise Linux Repository release file
Name: elrepo-release
Version: 7
Release: 0%{?dist}
License: GPLv2
Group: System Environment/Base
URL: http://elrepo.org/

Source0: elrepo.repo
Source1: RPM-GPG-KEY-elrepo.org

BuildArch: noarch

# To prevent users installing on the wrong dist
Requires: kernel = 3.10.0

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

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%pubkey RPM-GPG-KEY-elrepo.org
%dir %{_sysconfdir}/yum.repos.d/
%config(noreplace) %{_sysconfdir}/yum.repos.d/elrepo.repo
%dir %{_sysconfdir}/pki/rpm-gpg/
%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-elrepo.org

%changelog
* Tue May 20 2014 Philip J Perry <phil@elrepo.org> - 7-0
- Initial elrepo-release package for rhel7.
