### Name: ELRepo.org Community Enterprise Linux Repository for el6
### URL: https://elrepo.org/

Summary: ELRepo.org Community Enterprise Linux Repository release file
Name: elrepo-release
Version: 6
Release: 11%{?dist}
License: GPLv2
Group: System Environment/Base
URL: https://elrepo.org/

Source0: elrepo.repo
Source1: RPM-GPG-KEY-elrepo.org

BuildArch: noarch

# To prevent users installing on the wrong dist
Requires: glibc = 2.12

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
%config %{_sysconfdir}/yum.repos.d/elrepo.repo
%dir %{_sysconfdir}/pki/rpm-gpg/
%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-elrepo.org

%changelog
* Thu Dec 03 2020 Philip J Perry <phil@elrepo.org> - 6-11
- Prepare for el6 end of life

* Mon Jun 15 2020 Philip J Perry <phil@elrepo.org> - 6-10
- Replace stale mirror site.

* Mon Jul 15 2019 Philip J Perry <phil@elrepo.org> - 6-9
- Remove stale mirror site.

* Sun Jul 23 2017 Philip J Perry <phil@elrepo.org> - 6-8
- Remove stale mirror site.

* Tue Jun 10 2014 Philip J Perry <phil@elrepo.org> - 6-7
- Changed requires to glibc to allow for kernel removal.
  [http://elrepo.org/bugs/view.php?id=463]

* Sun Feb 09 2014 S.Tindall <s10dal@elrepo.org> - 6-6
- Added multiple baseurl= entries
- Used mirrors.elrepo.org in mirrorlist= URL

* Mon Jan 07 2013 Philip J Perry <phil@elrepo.org> - 6-5
- Add requires for kernel to prevent installing on wrong dist.
- Fix name of Extras repository
  [http://elrepo.org/bugs/view.php?id=339]

* Wed Jun 15 2011 Akemi Yagi <toracat@elrepo.org> - 6-4
- Added extras repo
- Enable elrepo repo by default
- Clean up the code in the spec file [Alan Bartlett <ajb@elrepo.org>]

* Sun Jan 30 2011 Philip J Perry <phil@elrepo.org> - 6-3
- Added mirrorlist
- Update license to GPLv2
- Fixed capitalisation in Description and Summary

* Mon Nov 15 2010 Akemi Yagi <toracat@elrepo.org> - 6-2
- Incorrect tag corrected.

* Thu Nov 11 2010 Akemi Yagi <toracat@elrepo.org> - 6-1
- First 6.x version 

* Thu Oct 21 2010 Akemi Yagi <toracat@elrepo.org> - 5-1
- Release number now has 5-x format
- Added kernel repo
- Removed fasttrack repo
- Added mirror sites

* Sat Mar 14 2009 Philip J Perry <phil@elrepo.org> - 0.1-1
- Initial elrepo-release package.
