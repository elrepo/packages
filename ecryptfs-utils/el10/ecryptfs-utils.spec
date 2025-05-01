# FIXME rhbz#533920, automake does not support python3
%bcond_with python3

%bcond_without unittests

%global confflags %{shrink: \
 --enable-pywrap --enable-tspi --enable-nss \
 --enable-pkcs11-helper --enable-tests \
 --with-pamdir=%{_libdir}/security \
}

%global pydesc \
The package contains a module that permits\
applications written in the Python programming language to use\
the interface supplied by the %{name} library.

Name: ecryptfs-utils
Version: 111
Release: 21.2%{?dist}
Summary: The eCryptfs mount helper and support libraries
License: GPLv2+
URL: https://launchpad.net/ecryptfs

Source0: http://launchpad.net/ecryptfs/trunk/%{version}/+download/%{name}_%{version}.orig.tar.gz
Source1: ecryptfs-mount-private.png

### upstream patches
# rhbz#1384023, openssl 1.1.x
Patch1: https://code.launchpad.net/~jelle-vdwaa/ecryptfs/ecryptfs/+merge/319746/+preview-diff/792383/+files/preview.diff#/%{name}-openssl11.patch

### downstream patches
# rhbz#500829, do not use ubuntu/debian only service
Patch92: %{name}-75-nocryptdisks.patch

# rhbz#553629, fix usage of salt together with file_passwd
Patch93: %{name}-83-fixsalt.patch

# fedora/rhel specific, rhbz#486139, remove nss dependency from umount.ecryptfs
Patch94: %{name}-83-splitnss.patch

# rhbz#664474, fix unsigned < 0 test
Patch95: %{name}-84-fixsigness.patch

# fix man pages
Patch98: %{name}-86-manpage.patch

# autoload ecryptfs module in ecryptfs-setup-private when needed, rhbz#707608
Patch99: %{name}-87-autoload.patch

# fedora/rhel specific, check for pam ecryptfs module before home migration
Patch911: %{name}-87-authconfig.patch

# using return after fork() in pam module has some nasty side effects, rhbz#722445
Patch914: %{name}-87-fixpamfork.patch

# we need gid==ecryptfs in pam module before mount.ecryptfs_private execution
Patch915: %{name}-87-fixexecgid.patch

# do not use zombie process, it causes lock ups at least for ssh login
Patch916: %{name}-87-nozombies.patch

# if we do not use zombies, we have to store passphrase in pam_data and init keyring later
Patch917: %{name}-87-pamdata.patch

# patch17 needs propper const on some places
Patch918: %{name}-87-fixconst.patch

Patch919: %{name}-87-syslog.patch

# if e-m-p fails, check if user is member of ecryptfs group
Patch921: %{name}-96-groupcheck.patch
Patch922: %{name}-99-selinux.patch

# rhbz#868330
Patch923: %{name}-100-sudokeyring.patch

# for e-u < 112
Patch924: %{name}-111-cve_2016_5224.patch

# do not crash if no password is available #1339714
Patch925: %{name}-111-nopasswd.patch

# Authconfig should no longer be used since F28
Patch926: %{name}-111-authselect.patch

### patches for general cleanup, should be kept and executed after all others
# allow building with -Werror
Patch999: %{name}-75-werror.patch

BuildRequires: python-rpm-macros
BuildRequires: swig >= 1.3.31
BuildRequires: libgcrypt-devel keyutils-libs-devel openssl-devel pam-devel
BuildRequires: trousers-devel nss-devel desktop-file-utils intltool
BuildRequires: pkcs11-helper-devel
BuildRequires: automake autoconf libtool glib2-devel gettext-devel perl-podlators libattr-devel

Requires: kernel-modules, keyutils, util-linux, gettext
Recommends: kmod-ecryptfs

%description
eCryptfs is a stacked cryptographic filesystem that ships in Linux
kernel versions 2.6.19 and above. This package provides the mount
helper and supporting libraries to perform key management and mount
functions.

Install %{name} if you would like to mount eCryptfs.

%package devel
Summary: The eCryptfs userspace development package
Requires: %{name} = %{version}-%{release}
Requires: keyutils-libs-devel %{name} = %{version}-%{release}
Requires: pkgconfig

%description devel
Userspace development files for eCryptfs.

%if %{with python3}
%package -n python%{python3_pkgversion}-%{name}
Summary: Python bindings for the eCryptfs utils
Requires: %{name} = %{version}-%{release}
BuildRequires: python%{python3_pkgversion}-devel
Provides: %{name}-python
Obsoletes:  %{name}-python < %{version}-%{release}
%{?python_provide:%python_provide python%{python3_pkgversion}-%{name}}

%description -n python%{python3_pkgversion}-%{name} %pydesc
%endif


%prep
%setup -q

%patch1 -p0 -b .openssl11

%patch92 -p1 -b .nocryptdisks
%patch93 -p1 -b .fixsalt
%patch94 -p1 -b .splitnss
%patch95 -p1 -b .fixsigness
%patch98 -p1 -b .manfix
%patch99 -p1 -b .autoload
%patch911 -p1 -b .authconfig
%patch914 -p1 -b .fixpamfork
%patch915 -p1 -b .fixexecgid
%patch916 -p1 -b .nozombies
%patch917 -p1 -b .pamdata
%patch918 -p1 -b .fixconst
%patch919 -p1 -b .syslog
%patch921 -p1 -b .groupcheck
%patch922 -p1 -b .selinux
%patch923 -p1 -b .sudokeyring
%patch924 -p1 -b .cve_2016_5224
%patch925 -p1 -b .nopasswd
%patch926 -p1 -b .authselect

%patch999 -p1 -b .werror

sed -i -r 's:^_syslog\(LOG:ecryptfs_\0:' src/pam_ecryptfs/pam_ecryptfs.c

# snprintf directive output may be truncated
sed -i -r 's:(snprintf.*"\%)(s/\%)(s"):\1.42\2.23\3:' \
 tests/kernel/inotify/test.c

# fix usr-move
sed -i -r 's:(rootsbindir=).*:\1"%{_sbindir}":' configure.ac
autoreconf -fiv

%build
# openssl 1.1 marks some functions as deprecated
export ERRFLAGS="-Werror -Wtype-limits -Wno-unused -Wno-error=deprecated-declarations"

%if %{with python3}
export PYTHON_VERSION=3
export PYTHON=%{__python3}
export PYTHON_NOVERSIONCHECK=1
export PY3FLAGS='%(pkg-config --cflags --libs python3)'
export CFLAGS="$RPM_OPT_FLAGS $PY3FLAGS $ERRFLAGS"
%configure %{confflags}
%else
%configure %{confflags} --disable-pywrap
%endif
%make_build

%install
%make_install

find $RPM_BUILD_ROOT/ -name '*.la' -print -delete
rm -rf $RPM_BUILD_ROOT%{_docdir}/%{name}

#install files Makefile forgot to install
install -p -m644 %{SOURCE1} $RPM_BUILD_ROOT%{_datadir}/%{name}/ecryptfs-mount-private.png
printf "Encoding=UTF-8\n" >>$RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-mount-private.desktop
printf "Encoding=UTF-8\n" >>$RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-setup-private.desktop
printf "Icon=%{_datadir}/%{name}/ecryptfs-mount-private.png\n" >>$RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-mount-private.desktop
printf "Icon=%{_datadir}/%{name}/ecryptfs-mount-private.png\n" >>$RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-setup-private.desktop
sed -i 's|^_||' $RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-mount-private.desktop
sed -i 's|^_||' $RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-setup-private.desktop
chmod +x $RPM_BUILD_ROOT%{_datadir}/%{name}/ecryptfs-mount-private.desktop
chmod +x $RPM_BUILD_ROOT%{_datadir}/%{name}/ecryptfs-setup-private.desktop
for file in $(find py2/src/desktop -name ¸*.desktop) ; do
 touch -r $file $RPM_BUILD_ROOT%{_datadir}/%{name}/$(basename $file)
done
rm -f $RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-record-passphrase

#we need ecryptfs kernel module
mkdir -p $RPM_BUILD_ROOT/usr/lib/modules-load.d/
echo -e "# ecryptfs module is needed before ecryptfs mount, so mount helper can \n# check for file name encryption support\necryptfs" \
 >$RPM_BUILD_ROOT/usr/lib/modules-load.d/ecryptfs.conf

%find_lang %{name}


%check
desktop-file-validate $RPM_BUILD_ROOT%{_datadir}/%{name}/*.desktop

if ldd $RPM_BUILD_ROOT%{_sbindir}/umount.ecryptfs | grep -q '%{_prefix}/'
then
  exit 1
fi

%if %{with unittests}
for folder in $(find . -name py\* -type d) ; do
 export LD_LIBRARY_PATH=${folder}/src/libecryptfs/.libs
 make check -C $folder
done
%endif

%pre
groupadd -r -f ecryptfs

%files -f %{name}.lang
%license COPYING
%doc README AUTHORS NEWS THANKS
%doc doc/ecryptfs-faq.html
%doc doc/ecryptfs-pkcs11-helper-doc.txt
%{_sbindir}/mount.ecryptfs
%{_sbindir}/umount.ecryptfs
%attr(4750,root,ecryptfs) %{_sbindir}/mount.ecryptfs_private
%{_sbindir}/umount.ecryptfs_private
%{_bindir}/ecryptfs-add-passphrase
%{_bindir}/ecryptfs-find
%{_bindir}/ecryptfs-generate-tpm-key
%{_bindir}/ecryptfs-insert-wrapped-passphrase-into-keyring
%{_bindir}/ecryptfs-manager
%{_bindir}/ecryptfs-migrate-home
%{_bindir}/ecryptfs-mount-private
%{_bindir}/ecryptfs-recover-private
%{_bindir}/ecryptfs-rewrap-passphrase
%{_bindir}/ecryptfs-rewrite-file
%{_bindir}/ecryptfs-setup-private
%{_bindir}/ecryptfs-setup-swap
%{_bindir}/ecryptfs-stat
%{_bindir}/ecryptfs-umount-private
%{_bindir}/ecryptfs-unwrap-passphrase
%{_bindir}/ecryptfs-verify
%{_bindir}/ecryptfs-wrap-passphrase
%{_bindir}/ecryptfsd
%{_libdir}/ecryptfs
%{_libdir}/libecryptfs.so.*
%{_libdir}/security/pam_ecryptfs.so
%{_prefix}/lib/modules-load.d/ecryptfs.conf
%dir %{_datadir}/%{name}
%{_datadir}/%{name}/ecryptfs-mount-private.txt
%{_datadir}/%{name}/ecryptfs-mount-private.desktop
%{_datadir}/%{name}/ecryptfs-mount-private.png
%{_datadir}/%{name}/ecryptfs-setup-private.desktop
%{_mandir}/man1/ecryptfs-add-passphrase.1.gz
%{_mandir}/man1/ecryptfs-find.1*
%{_mandir}/man1/ecryptfs-generate-tpm-key.1.gz
%{_mandir}/man1/ecryptfs-insert-wrapped-passphrase-into-keyring.1.gz
%{_mandir}/man1/ecryptfs-mount-private.1.gz
%{_mandir}/man1/ecryptfs-recover-private.1.gz
%{_mandir}/man1/ecryptfs-rewrap-passphrase.1.gz
%{_mandir}/man1/ecryptfs-rewrite-file.1.gz
%{_mandir}/man1/ecryptfs-setup-private.1.gz
%{_mandir}/man1/ecryptfs-setup-swap.1.gz
%{_mandir}/man1/ecryptfs-stat.1.gz
%{_mandir}/man1/ecryptfs-umount-private.1.gz
%{_mandir}/man1/ecryptfs-unwrap-passphrase.1.gz
%{_mandir}/man1/ecryptfs-verify.1*
%{_mandir}/man1/ecryptfs-wrap-passphrase.1.gz
%{_mandir}/man1/mount.ecryptfs_private.1.gz
%{_mandir}/man1/umount.ecryptfs_private.1.gz
%{_mandir}/man7/ecryptfs.7.gz
%{_mandir}/man8/ecryptfs-manager.8.gz
%{_mandir}/man8/ecryptfs-migrate-home.8*
%{_mandir}/man8/ecryptfsd.8.gz
%{_mandir}/man8/mount.ecryptfs.8.gz
%{_mandir}/man8/pam_ecryptfs.8.gz
%{_mandir}/man8/umount.ecryptfs.8.gz

%files devel
%{_libdir}/libecryptfs.so
%{_libdir}/pkgconfig/libecryptfs.pc
%{_includedir}/ecryptfs.h

%if %{with python3}
%files -n python%{python3_pkgversion}-%{name}
%{python3_sitearch}/%{name}/
%{python3_sitelib}/%{name}/
%endif


%changelog
* Tue Apr 29 2025 Akemi Yagi <toracat@elrepo.org> - 111-21.2
- Rebuilt for RHEL 10
 
* Thu Dec 01 2022 Akemi Yagi <toracat@elrepo.org> - 111-21.1
- cryptsetup-luks removed from Requires:
- added 'Recommends: kmod-ecryptfs'

* Fri May 20 2022 Akemi Yagi <toracat@elrepo.org> - 111-20.1
- Rebuilt for RHEL 9

* Mon Jul 27 2020 Akemi Yagi <toracat@elrepo.org> - 111-19.1.el7.elrepo
- %post and %postun were dropped because the authselect rpm shipping with EL8 
  doesn't contain the 'with-ecryptfs' feature profiles.
  [https://elrepo.org/bugs/view.php?id=1026]

* Fri Jul 24 2020 Akemi Yagi <toracat@elrepo.org> - 111-19.el7.elrepo
- rebuilt for elrepo.

* Wed Jul 24 2019 Fedora Release Engineering <releng@fedoraproject.org> - 111-19
- Rebuilt for https://fedoraproject.org/wiki/Fedora_31_Mass_Rebuild

* Tue Jun 25 2019 Michal Hlavinka <mhlavink@redhat.com> - 111-18
- require kernel-modules, where ecryptfs kernel module lives

* Thu Jan 31 2019 Fedora Release Engineering <releng@fedoraproject.org> - 111-17
- Rebuilt for https://fedoraproject.org/wiki/Fedora_30_Mass_Rebuild

* Fri Nov 16 2018 Michal Hlavinka <mhlavink@redhat.com> - 111-16
- drop python2 subpackage is python2 is no longer supported in Fedora 30+ (#1627433)

* Fri Sep 07 2018 Michal Hlavinka <mhlavink@redhat.com> - 111-15
- switch to authselect since it replaced authconfig in F28 (RHBZ#1577174)

* Tue Jul 31 2018 Florian Weimer <fweimer@redhat.com> - 111-14
- Rebuild with fixed binutils

* Thu Jul 12 2018 Fedora Release Engineering <releng@fedoraproject.org> - 111-13
- Rebuilt for https://fedoraproject.org/wiki/Fedora_29_Mass_Rebuild

* Mon Mar 05 2018 Raphael Groner <projects.rg@smart.ms> - 111-12
- avoid unversioned python executable
- add python3 subpackage (experimental, found odd bug in automake)
- optimize generally here and there

* Fri Feb 09 2018 Igor Gnatenko <ignatenkobrain@fedoraproject.org> - 111-11
- Escape macros in %%changelog

* Wed Feb 07 2018 Fedora Release Engineering <releng@fedoraproject.org> - 111-10
- Rebuilt for https://fedoraproject.org/wiki/Fedora_28_Mass_Rebuild

* Sun Aug 20 2017 Zbigniew Jędrzejewski-Szmek <zbyszek@in.waw.pl> - 111-9
- Add Provides for the old name without %%_isa

* Sat Aug 19 2017 Zbigniew Jędrzejewski-Szmek <zbyszek@in.waw.pl> - 111-8
- Python 2 binary package renamed to python2-ecryptfs-utils
  See https://fedoraproject.org/wiki/FinalizingFedoraSwitchtoPython3

* Wed Aug 02 2017 Fedora Release Engineering <releng@fedoraproject.org> - 111-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Binutils_Mass_Rebuild

* Wed Jul 26 2017 Fedora Release Engineering <releng@fedoraproject.org> - 111-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Mass_Rebuild

* Sun Jun 04 2017 Raphael Groner <projects.rg@smart.ms> - 111-5
- add patch for openssl 1.1.x, rhbz#1384023
- mark patches of upstream and downstream
- fix legacy patches to still work, drop obsolete patch for memcpyfix
- general modernization according to guidelines, drop obsolete commands

* Fri Feb 10 2017 Fedora Release Engineering <releng@fedoraproject.org> - 111-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_26_Mass_Rebuild

* Wed Feb 01 2017 Michal Hlavinka <mhlavink@redhat.com> - 111-3
- do not crash when using fingerprint reader #1339714

* Tue Jul 19 2016 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 111-2
- https://fedoraproject.org/wiki/Changes/Automatic_Provides_for_Python_RPM_Packages

* Fri Jul 15 2016 Michal Hlavinka <mhlavink@redhat.com> - 111-1
- %%{name} updated to 111
- fix ecryptfs-setup-swap improperly configures encrypted swap when using GPT 
  partitioning on a NVMe or MMC drive (CVE-2016-6224, rhbz#1356828)

* Mon Feb 29 2016 Michal Hlavinka <mhlavink@redhat.com> - 110-1
- %%{name} updated to 110

* Wed Feb 03 2016 Fedora Release Engineering <releng@fedoraproject.org> - 109-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_24_Mass_Rebuild

* Tue Jan 26 2016 Michal Hlavinka <mhlavink@redhat.com> - 109-1
- %%{name} updated to 109

* Tue Aug 11 2015 Michal Hlavinka <mhlavink@redhat.com> - 108-1
- %%{name} updated to 108

* Wed Jun 17 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 106-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Mon Mar 30 2015 Michal Hlavinka <mhlavink@redhat.com> - 106-1
- %%{name} updated to 106

* Mon Jan 26 2015 Michal Hlavinka <mhlavink@redhat.com> - 104-3
- fix pam sigsegv (#1184645)

* Sat Aug 16 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 104-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Wed Jul 23 2014 Michal Hlavinka <mhlavink@redhat.com> - 104-1
- %%{name} updated to 104

* Sat Jun 07 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 103-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Wed Nov 13 2013 Michal Hlavinka <mhlavink@redhat.com> - 103-4
- ecryptfs-migrate-home did not restore selinux labels (#1017402)

* Sat Aug 03 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 103-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Thu May 23 2013 Michal Hlavinka <mhlavink@redhat.com> - 103-2
- make executables hardened (#965505)

* Wed Jan 30 2013 Michal Hlavinka <mhlavink@redhat.com> - 103-1
- %%{name} updated to 103

* Mon Oct 29 2012 Michal Hlavinka <mhlavink@redhat.com> - 101-1
- %%{name} updated to 101

* Thu Oct 25 2012 Michal Hlavinka <mhlavink@redhat.com> - 100-6
- home migration did not work under sudo (#868330)

* Mon Oct 22 2012 Michal Hlavinka <mhlavink@redhat.com> - 100-5
- set selinux boolean only if not already set (#868298)

* Thu Oct 18 2012 Michal Hlavinka <mhlavink@redhat.com> - 100-4
- fix typo in restorecon path (#865839)

* Thu Sep 27 2012 Michal Hlavinka <mhlavink@redhat.com> - 100-3
- do not crash in pam module when non-existent user name is used (#859766)

* Mon Aug 20 2012 Michal Hlavinka <mhlavink@redhat.com> - 100-2
- fix Werror messages in new build environment

* Fri Aug 03 2012 Michal Hlavinka <mhlavink@redhat.com> - 100-1
- %%{name} updated to 100

* Tue Jul 24 2012 Michal Hlavinka <mhlavink@redhat.com> - 99-1
- %%{name} updated to 99
- fixes: suid helper does not restrict mounting filesystems with 
  nosuid, nodev leading to possible privilege escalation (CVE-2012-3409)

* Wed Jul 18 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 97-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Mon Jun 25 2012 Michal Hlavinka <mhlavink@redhat.com> - 97-1
- %%{name} updated to 97

* Mon Jun 04 2012 Michal Hlavinka <mhlavink@redhat.com> - 96-3
- for file name encryption support check, module must be loaded already

* Mon Apr 16 2012 Michal Hlavinka <mhlavink@redhat.com> - 96-2
- when ecryptfs-mount-fails, check if user is member of ecryptfs group

* Mon Feb 20 2012 Michal Hlavinka <mhlavink@redhat.com> - 96-1
- %%{name} updated to 96

* Mon Feb 13 2012 Michal Hlavinka <mhlavink@redhat.com> - 95-3
- blowfish and twofish support check did not work with on 3.2.x kernels (#785036)

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 95-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Fri Dec 16 2011 Michal Hlavinka <mhlavink@redhat.com> - 95-1
- updated to v. 95

* Wed Dec 07 2011 Michal Hlavinka <mhlavink@redhat.com> - 93-2
- update pam config in post install phase

* Mon Oct 31 2011 Michal Hlavinka <mhlavink@redhat.com> - 93-1
- updated to v. 93

* Wed Aug 31 2011 Michal Hlavinka <mhlavink@redhat.com> - 90-2
- set the group id in mount.ecryptfs_private (CVE-2011-3145)

* Thu Aug 11 2011 Michal Hlavinka <mhlavink@redhat.com> - 90-1
- security fixes:
- privilege escalation via mountpoint race conditions (CVE-2011-1831, CVE-2011-1832)
- race condition when checking source during mount (CVE-2011-1833)
- mtab corruption via improper handling (CVE-2011-1834)
- key poisoning via insecure temp directory handling (CVE-2011-1835)
- information disclosure via recovery mount in /tmp (CVE-2011-1836)
- arbitrary file overwrite via lock counter race (CVE-2011-1837)

* Tue Aug 09 2011 Michal Hlavinka <mhlavink@redhat.com> - 87-9
- improve logging messages of ecryptfs pam module
- keep own copy of passphrase, pam clears it too early

* Wed Aug 03 2011 Michal Hlavinka <mhlavink@redhat.com> - 87-8
- keyring from auth stack does not survive, use pam_data and delayed 
  keyring initialization

* Thu Jul 21 2011 Michal Hlavinka <mhlavink@redhat.com> - 87-7
- fix pam module to set ecryptfs gid before mount helper execution
- do not use zombie process, it causes lock ups in ssh

* Tue Jul 19 2011 Michal Hlavinka <mhlavink@redhat.com> - 87-6
- do not use memcpy for overlaping areas
- fix broken pam module resulting in session with wrong gid

* Mon Jul 11 2011 Michal Hlavinka <mhlavink@redhat.com> - 87-5
- fix mtab handling everywhere

* Thu Jun 09 2011 Michal Hlavinka <mhlavink@redhat.com> - 87-4
- check for ecryptfs pam module before home dir migration

* Tue Jun 07 2011 Michal Hlavinka <mhlavink@redhat.com> - 87-3
- update of mtab does not work if it's a symlink (#706911)

* Thu May 26 2011 Michal Hlavinka <mhlavink@redhat.com> - 87-2
- auto-load ecryptfs module in ecryptfs-setup-private

* Tue May 24 2011 Michal Hlavinka <mhlavink@redhat.com> - 87-1
- updated to v. 87

* Fri Mar 11 2011 Michal Hlavinka <mhlavink@redhat.com> - 86-3
- fix man pages

* Wed Mar 02 2011 Michal Hlavinka <mhlavink@redhat.com> - 86-2
- fix selinux context

* Fri Feb 25 2011 Michal Hlavinka <mhlavink@redhat.com> - 86-1
- updated to v. 86

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 85-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Tue Feb 01 2011 Michal Hlavinka <mhlavink@redhat.com> - 85-1
- %%{name} updated to 85

* Tue Jan 11 2011 Dan Horák <dan[at]danny.cz> - 84-3
- fix build on arches where char is unsigned by default

* Tue Jan 04 2011 Michal Hlavinka <mhlavink@redhat.com> - 84-2
- fix unsigned < 0 test (#664474)

* Mon Dec 20 2010 Michal Hlavinka <mhlavink@redhat.com> - 84-1
- %%{name} updated to 84

* Wed Sep 29 2010 jkeating - 83-9
- Rebuilt for gcc bug 634757

* Wed Sep 22 2010 Michal Hlavinka <mhlavink@redhat.com> - 83-8
- add missing gettext require (#630212)

* Mon Jul 26 2010 Michal Hlavinka <mhlavink@redhat.com> - 83-7
- fix ftbfs for python mass rebuild

* Wed Jul 21 2010 David Malcolm <dmalcolm@redhat.com> - 83-6
- Rebuilt for https://fedoraproject.org/wiki/Features/Python_2.7/MassRebuild

* Tue May 04 2010 Michal Hlavinka <mhlavink@redhat.com> - 83-5
- remove nss dependency from umount.ecryptfs

* Fri Apr 16 2010 Michal Hlavinka <mhlavink@redhat.com> - 83-4
- make salt working together with passwd_file

* Mon Mar 22 2010 Michal Hlavinka <mhlavink@redhat.com> - 83-3
- enable PKCS#11 support

* Wed Mar 10 2010 Michal Hlavinka <mhlavink@redhat.com> - 83-2
- blkid moved from e2fsprogs to util-linux-ng, follow the change (#569996)

* Thu Feb 18 2010 Michal Hlavinka <mhlavink@redhat.com> - 83-1
- updated to v. 83

* Wed Jan 27 2010 Michal Hlavinka <mhlavink@redhat.com> - 82-2
- better fix for (#486139)

* Wed Nov 11 2009 Michal Hlavinka <mhlavink@redhat.com> - 82-1
- updated to 82

* Mon Nov 09 2009 Michal Hlavinka <mhlavink@redhat.com> - 81-2
- fix getext typos (#532732)

* Tue Sep 29 2009 Michal Hlavinka <mhlavink@redhat.com> - 81-1
- updated to 81

* Fri Aug 21 2009 Tomas Mraz <tmraz@redhat.com> - 79-2
- rebuilt with new openssl

* Tue Aug 18 2009 Michal Hlavinka <mhlavink@redhat.com> - 79-1
- updated to 79

* Wed Jul 29 2009 Michal Hlavinka <mhlavink@redhat.com> - 78-2
- ecryptfs-dot-private is no longer used

* Wed Jul 29 2009 Michal Hlavinka <mhlavink@redhat.com> - 78-1
- updated to 78

* Fri Jul 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 76-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Mon Jul 20 2009 Michal Hlavinka <mhlavink@redhat.com> 76-1
- updated to 76

* Thu May 21 2009 Michal Hlavinka <mhlavink@redhat.com> 75-1
- removed executable permission from ecryptfs-dot-private (#500817)
- require cryptsetup-luks for encrypted swap (#500824)
- use blkid instead of vol_id (#500820)
- don't rely on cryptdisks service (#500829)
- add icon for Access-Your-Private-Data.desktop file
- updated to 75
- restrict mount.ecryptfs_private to ecryptfs group members only

* Thu Apr 23 2009 Michal Hlavinka <mhlavink@redhat.com> 74-1
- updated to 74

* Sat Mar 21 2009 Michal Hlavinka <mhlavink@redhat.com> 73-1
- updated to 73
- move libs from /usr/lib to /lib (#486139)
- fix symlinks created by ecryptfs-setup-private (#486146)

* Tue Feb 24 2009 Michal Hlavinka <mhlavink@redhat.com> 71-1
- updated to 71
- remove .la files

* Mon Feb 16 2009 Michal Hlavinka <mhlavink@redhat.com> 70-1
- updated to 70
- fix: #479762 - ecryptfsecryptfs-setup-private broken
- added umount option to clear per-user keyring

* Mon Feb 02 2009 Michal Hlavinka <mhlavink@redhat.com> 69-4
- fix list of onwed directories

* Tue Jan 27 2009 Michal Hlavinka <mhlavink@redhat.com> 69-3
- add missing requires: keyutils

* Tue Jan 27 2009 Michal Hlavinka <mhlavink@redhat.com> 69-2
- bump release for rebuild

* Tue Jan 27 2009 Michal Hlavinka <mhlavink@redhat.com> 69-1
- updated to 69

* Mon Jan 12 2009 Michal Hlavinka <mhlavink@redhat.com> 68-0
- updated to 68
- fix #478464 - /usr/bin/ecryptfs-setup-private errors out

* Mon Dec 29 2008 Michal Hlavinka <mhlavink@redhat.com> 67-1
- bump release for rebuild

* Mon Dec 29 2008 Michal Hlavinka <mhlavink@redhat.com> 67-0
- updated to 67

* Wed Oct 22 2008 Mike Halcrow <mhalcrow@us.ibm.com> 61-0
- Add support for filename encryption enablement (future kernel feature)
- Replace uint32_t with size_t for x86_64 compatibility (patch by Eric Sandeen)

* Fri Oct 17 2008 Eric Sandeen <sandeen@redhat.com> 59-2
- Remove duplicate doc files from rpm

* Tue Oct 07 2008 Mike Halcrow <mhalcrow@us.ibm.com> 59-1
- Put attr declaration in the right spot

* Tue Oct 07 2008 Mike Halcrow <mhalcrow@us.ibm.com> 59-0
- Make /sbin/*ecryptfs* files setuid
- Add /sbin path to ecryptfs-setup-private

* Mon Oct 06 2008 Mike Halcrow <mhalcrow@us.ibm.com> 58-0
- TSPI key module update to avoid flooding TrouSerS library with requests
- OpenSSL key module parameter fixes
- Updates to mount-on-login utilities

* Wed Aug 13 2008 Mike Halcrow <mhalcrow@us.ibm.com> 56-0
- Namespace fixes for the key module parameter aliases
- Updates to the man page and the README

* Wed Jul 30 2008 Eric Sandeen <sandeen@redhat.com> 53-0
- New upstream version
- Many new manpages, new ecryptfs-stat util

* Thu Jul 17 2008 Tom "spot" Callaway <tcallawa@redhat.com> 50-1
- fix license tag

* Fri Jun 27 2008 Mike Halcrow <mhalcrow@us.ibm.com> 50-0
- Add umount.ecryptfs_private symlink
- Add pam_mount session hooks for mount and unmount

* Fri Jun 27 2008 Eric Sandeen <sandeen@redhat.com> 49-1
- build with TrouSerS key module

* Fri Jun 27 2008 Eric Sandeen <sandeen@redhat.com> 49-0
- New upstream version

* Tue Jun 03 2008 Eric Sandeen <sandeen@redhat.com> 46-0
- New upstream version

* Mon Feb 18 2008 Mike Halcrow <mhalcrow@us.ibm.com> 40-0
- Enable passwd_file option in openssl key module

* Wed Feb 13 2008 Mike Halcrow <mhalcrow@us.ibm.com> 39-0
- Fix include upstream

* Wed Feb 13 2008 Karsten Hopp <karsten@redhat.com> 38-1
- fix includes

* Tue Jan 8 2008 Mike Halcrow <mhalcrow@us.ibm.com> 38-0
 - Fix passthrough mount option prompt
 - Clarify man page
 - Add HMAC option (for future kernel versions)
 - Bump to version 38

* Wed Dec 19 2007 Mike Halcrow <mhalcrow@us.ibm.com> 37-0
- Remove unsupported ciphers; bump to version 37

* Tue Dec 18 2007 Mike Halcrow <mhalcrow@us.ibm.com> 36-0
- Cipher selection detects .gz ko files; bump to version 36

* Mon Dec 17 2007 Mike Halcrow <mhalcrow@us.ibm.com> 35-0
- Cleanups to cipher selection; bump to version 35

* Mon Dec 17 2007 Mike Halcrow <mhalcrow@us.ibm.com> 34-0
- Fix OpenSSL key module; bump to version 34

* Fri Dec 14 2007 Mike Halcrow <mhalcrow@us.ibm.com> 33-1
- Add files to package

* Fri Dec 14 2007 Mike Halcrow <mhalcrow@us.ibm.com> 33-0
- update to version 33

* Thu Dec 13 2007 Karsten Hopp <karsten@redhat.com> 32-1
- update to version 32

* Thu Nov 29 2007 Karsten Hopp <karsten@redhat.com> 30-2
- fix ia64 libdir
- build initial RHEL-5 version

* Thu Nov 29 2007 Karsten Hopp <karsten@redhat.com> 30-1
- build version 30

* Fri Oct 05 2007 Mike Halcrow <mhalcrow@us.ibm.com> - 30-0
- Bump to version 30. Several bugfixes. Key modules are overhauled
  with a more sane API.
* Wed Aug 29 2007 Fedora Release Engineering <rel-eng at fedoraproject dot org> - 18-1
- Rebuild for selinux ppc32 issue.

* Thu Jun 28 2007 Mike Halcrow <mhalcrow@us.ibm.com> - 18-0
- Bump to version 18 with an OpenSSL key module fix
* Thu Jun 21 2007 Kevin Fenzi <kevin@tummy.com> - 17-1
- Change kernel Requires to Conflicts
- Remove un-needed devel buildrequires
* Wed Jun 20 2007 Mike Halcrow <mhalcrow@us.ibm.com>  - 17-0
- Provide built-in fallback passphrase key module. Remove keyutils,
  openssl, and pam requirements (library dependencies take care of
  this). Include wrapped passphrase executables in file set.
* Fri Apr 20 2007 Mike Halcrow <mhalcrow@us.ibm.com>  - 15-1
- Change permission of pam_ecryptfs.so from 644 to 755.
* Thu Apr 19 2007 Mike Halcrow <mhalcrow@us.ibm.com>  - 15-0
- Fix mount option parse segfault. Fix pam_ecryptfs.so semaphore
  issue when logging in via ssh.
* Thu Mar 01 2007 Mike Halcrow <mhalcrow@us.ibm.com>  - 10-0
- Remove verbose syslog() calls; change key module build to allow
  OpenSSL module to be disabled from build; add AUTHORS, NEWS, and
  THANKS to docs; update Requires with variables instead of hardcoded
  name and version.
* Tue Feb 06 2007 Mike Halcrow <mhalcrow@us.ibm.com>  - 9-1
- Minor update in README, add dist tag to Release, add --disable-rpath
  to configure step, and remove keyutils-libs from Requires.
* Tue Jan 09 2007 Mike Halcrow <mhalcrow@us.ibm.com>  - 9-0
- Correct install directories for 64-bit; add support for xattr and
  encrypted_view mount options
* Tue Jan 02 2007 Mike Halcrow <mhalcrow@us.ibm.com>  - 8-0
- Introduce build support for openCryptoki key module.  Fix -dev build
  dependencies for devel package
* Mon Dec 11 2006 Mike Halcrow <mhalcrow@us.ibm.com>  - 7-0
- Initial package creation
