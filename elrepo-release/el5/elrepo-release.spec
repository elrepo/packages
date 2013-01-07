### Name: ElRepo.org Community Enterprise Linux Repository for el5
### URL: http://elrepo.org/

Summary: ELRepo.org Community Enterprise Linux Repository release file
Name: elrepo-release
Version: 5
Release: 4%{?dist}
License: GPLv2
Group: System Environment/Base
URL: http://elrepo.org/

Source0: elrepo.repo
Source1: RPM-GPG-KEY-elrepo.org

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
BuildArch: noarch

# To prevent users installing on the wrong dist
Requires: kernel = 2.6.18

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
* Mon Jan 07 2013 Philip J Perry <phil@elrepo.org> - 5-4
- Add requires for kernel to prevent installing on wrong dist.
- Fix name of Extras repository
  [http://elrepo.org/bugs/view.php?id=339]

* Wed Jun 15 2011 Akemi Yagi <toracat@elrepo.org> - 5-3
- Added extras repo
- Enable elrepo repo by default
- Clean up the code in the spec file [Alan Bartlett <ajb@elrepo.org>]

* Sat Jan 22 2011 Philip J Perry <phil@elrepo.org> - 5-2
- Added mirrorlist
- Update license to GPLv2
- Fixed capitalisation in Description and Summary

* Thu Oct 21 2010 Akemi Yagi <toracat@elrepo.org> - 5-1
- Release number now has 5-x format
- Added kernel repo
- Removed fasttrack repo
- Added mirror sites

* Sat Mar 14 2009 Philip J Perry <phil@elrepo.org> - 0.1-1
- Initial elrepo-release package.
