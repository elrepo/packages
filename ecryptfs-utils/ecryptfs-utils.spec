%{!?python_sitelib: %define python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib()")}
%{!?python_sitearch: %define python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}
%global _libdir /%{_lib}
%global _sbindir /sbin

Name: ecryptfs-utils
Version: 111
Release: 1%{?dist}
Summary: The eCryptfs mount helper and support libraries
Group: System Environment/Base
License: GPLv2+
URL: https://launchpad.net/ecryptfs
Source0: http://launchpad.net/ecryptfs/trunk/%{version}/+download/%{name}_%{version}.orig.tar.gz
Source1: ecryptfs-mount-private.png

# rhbz#500829, do not use ubuntu/debian only service
Patch2: ecryptfs-utils-75-nocryptdisks.patch

# rhbz#553629, fix usage of salt together with file_passwd
Patch3: ecryptfs-utils-83-fixsalt.patch

# fedora/rhel specific, rhbz#486139, remove nss dependency from umount.ecryptfs
Patch4: ecryptfs-utils-83-splitnss.patch

# rhbz#664474, fix unsigned < 0 test
Patch5: ecryptfs-utils-84-fixsigness.patch

# fix man pages
Patch8: ecryptfs-utils-86-manpage.patch

# autoload ecryptfs module in ecryptfs-setup-private when needed, rhbz#707608
Patch9: ecryptfs-utils-87-autoload.patch

# fedora/rhel specific, check for pam ecryptfs module before home migration
Patch11: ecryptfs-utils-87-authconfig.patch

# memcpy can't be used on overlaping areas
Patch12: ecryptfs-utils-87-memcpyfix.patch

# allow building with -Werror
Patch999: ecryptfs-utils-75-werror.patch

# using return after fork() in pam module has some nasty side effects, rhbz#722445
Patch14: ecryptfs-utils-87-fixpamfork.patch

# we need gid==ecryptfs in pam module before mount.ecryptfs_private execution
Patch15: ecryptfs-utils-87-fixexecgid.patch

# do not use zombie process, it causes lock ups at least for ssh login
Patch16: ecryptfs-utils-87-nozombies.patch

# if we do not use zombies, we have to store passphrase in pam_data and init keyring later
Patch17: ecryptfs-utils-87-pamdata.patch

# patch17 needs propper const on some places
Patch18: ecryptfs-utils-87-fixconst.patch

Patch19: ecryptfs-utils-87-syslog.patch

# if e-m-p fails, check if user is member of ecryptfs group
Patch21: ecryptfs-utils-96-groupcheck.patch
Patch22: ecryptfs-utils-99-selinux.patch

# rhbz#868330
Patch23: ecryptfs-utils-100-sudokeyring.patch

# for e-u < 112
Patch24: ecryptfs-utils-111-cve_2016_5224.patch

BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
Requires: keyutils, cryptsetup, util-linux, gettext
BuildRequires: libgcrypt-devel keyutils-libs-devel openssl-devel pam-devel
BuildRequires: trousers-devel nss-devel desktop-file-utils intltool
BuildRequires: pkcs11-helper-devel
BuildRequires: automake autoconf libtool glib2-devel gettext-devel perl-podlators libattr-devel

%description
eCryptfs is a stacked cryptographic filesystem that ships in Linux
kernel versions 2.6.19 and above. This package provides the mount
helper and supporting libraries to perform key management and mount
functions.

Install ecryptfs-utils if you would like to mount eCryptfs.

%package devel
Summary: The eCryptfs userspace development package
Group: System Environment/Base
Requires: keyutils-libs-devel %{name} = %{version}-%{release}
Requires: pkgconfig

%description devel
Userspace development files for eCryptfs.

%package python
Summary: Python bindings for the eCryptfs utils
Group: System Environment/Base
Requires: ecryptfs-utils %{name} = %{version}-%{release}
BuildRequires: python python-devel swig >= 1.3.31

%description python
The ecryptfs-utils-python package contains a module that permits 
applications written in the Python programming language to use 
the interface supplied by the ecryptfs-utils library.

%prep
%setup -q
%patch2 -p1 -b .nocryptdisks
%patch3 -p1 -b .fixsalt
%patch4 -p1 -b .splitnss
%patch5 -p1 -b .fixsigness
%patch8 -p1 -b .manfix
%patch9 -p1 -b .autoload
%patch11 -p1 -b .authconfig
#%patch12 -p1 -b .memcpyfix
%patch999 -p1 -b .werror
%patch14 -p1 -b .fixpamfork
%patch15 -p1 -b .fixexecgid
%patch16 -p1 -b .nozombies
%patch17 -p1 -b .pamdata
%patch18 -p1 -b .fixconst
%patch19 -p1 -b .syslog
%patch21 -p1 -b .groupcheck
%patch22 -p1 -b .selinux
%patch23 -p1 -b .sudokeyring
%patch24 -p1 -b .cve_2016_5224

%build
%global _hardened_build 1
export CFLAGS="$RPM_OPT_FLAGS -Werror -Wtype-limits -Wno-unused"
#we're modifing Makefile.am
autoreconf -fiv
%configure --disable-rpath --enable-tspi --enable-nss --enable-pkcs11-helper --enable-tests
make clean
#disable rpath
sed -i 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g' libtool
sed -i 's|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool

make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
find $RPM_BUILD_ROOT/ -name '*.la' | xargs rm -f
rm -rf $RPM_BUILD_ROOT%{_docdir}/%{name}
#install files Makefile forgot install
install -m644 %{SOURCE1} $RPM_BUILD_ROOT%{_datadir}/%{name}/ecryptfs-mount-private.png
printf "Encoding=UTF-8\n" >>$RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-mount-private.desktop
printf "Encoding=UTF-8\n" >>$RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-setup-private.desktop
printf "Icon=%{_datadir}/%{name}/ecryptfs-mount-private.png\n" >>$RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-mount-private.desktop
printf "Icon=%{_datadir}/%{name}/ecryptfs-mount-private.png\n" >>$RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-setup-private.desktop
sed -i 's|^_||' $RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-mount-private.desktop
sed -i 's|^_||' $RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-setup-private.desktop
desktop-file-validate $RPM_BUILD_ROOT%{_datadir}/%{name}/ecryptfs-mount-private.desktop
desktop-file-validate $RPM_BUILD_ROOT%{_datadir}/%{name}/ecryptfs-setup-private.desktop
chmod +x $RPM_BUILD_ROOT%{_datadir}/%{name}/ecryptfs-mount-private.desktop
chmod +x $RPM_BUILD_ROOT%{_datadir}/%{name}/ecryptfs-setup-private.desktop
touch -r src/desktop/ecryptfs-mount-private.desktop \
     $RPM_BUILD_ROOT%{_datadir}/%{name}/ecryptfs-mount-private.desktop
touch -r src/desktop/ecryptfs-setup-private.desktop \
     $RPM_BUILD_ROOT%{_datadir}/%{name}/ecryptfs-mount-private.desktop
rm -f $RPM_BUILD_ROOT/%{_datadir}/%{name}/ecryptfs-record-passphrase

#we need ecryptfs kernel module
mkdir -p $RPM_BUILD_ROOT/usr/lib/modules-load.d/
echo -e "# ecryptfs module is needed before ecryptfs mount, so mount helper can \n# check for file name encryption support\necryptfs" >$RPM_BUILD_ROOT/usr/lib/modules-load.d/ecryptfs.conf

%find_lang %{name}

%check
if ldd $RPM_BUILD_ROOT%{_sbindir}/umount.ecryptfs | grep -q '/usr/'
then
  exit 1
fi
export LD_LIBRARY_PATH=$(pwd)/src/libecryptfs/.libs
make check

%pre
groupadd -r -f ecryptfs

%post 
/sbin/ldconfig
if [ $1 -eq 1 ] ; then 
    # Initial installation 
    authconfig --enableecryptfs --update
fi

%postun 
/sbin/ldconfig
if [ $1 -eq 0 ] ; then
    # Package removal, not upgrade
    authconfig --disableecryptfs --update
fi

%clean
rm -rf $RPM_BUILD_ROOT

%files -f %{name}.lang
%defattr(-,root,root,-)
%doc README COPYING AUTHORS NEWS THANKS
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
/usr/lib/modules-load.d/ecryptfs.conf
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
%defattr(-,root,root,-)
%{_libdir}/libecryptfs.so
%{_libdir}/pkgconfig/libecryptfs.pc
%{_includedir}/ecryptfs.h

%files python
%defattr(-,root,root,-)
%dir %{python_sitelib}/ecryptfs-utils
%{python_sitelib}/ecryptfs-utils/libecryptfs.py
%{python_sitelib}/ecryptfs-utils/libecryptfs.pyc
%{python_sitelib}/ecryptfs-utils/libecryptfs.pyo
%dir %{python_sitearch}/ecryptfs-utils
%{python_sitearch}/ecryptfs-utils/_libecryptfs.so.0
%{python_sitearch}/ecryptfs-utils/_libecryptfs.so.0.0.0
%{python_sitearch}/ecryptfs-utils/_libecryptfs.so

%changelog
* Sat Oct 15 2016 Akemi Yagi <toracat@elrepo.org> - 111-1.el7.elrepo
- rebuilt for elrepo.

* Fri Jul 15 2016 Michal Hlavinka <mhlavink@redhat.com> - 111-1
- ecryptfs-utils updated to 111
- fix ecryptfs-setup-swap improperly configures encrypted swap when using GPT 
  partitioning on a NVMe or MMC drive (CVE-2016-6224, rhbz#1356828)

* Mon Feb 29 2016 Michal Hlavinka <mhlavink@redhat.com> - 110-1
- ecryptfs-utils updated to 110

* Wed Feb 03 2016 Fedora Release Engineering <releng@fedoraproject.org> - 109-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_24_Mass_Rebuild

* Tue Jan 26 2016 Michal Hlavinka <mhlavink@redhat.com> - 109-1
- ecryptfs-utils updated to 109

* Tue Aug 11 2015 Michal Hlavinka <mhlavink@redhat.com> - 108-1
- ecryptfs-utils updated to 108

* Wed Jun 17 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 106-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Mon Mar 30 2015 Michal Hlavinka <mhlavink@redhat.com> - 106-1
- ecryptfs-utils updated to 106

* Mon Jan 26 2015 Michal Hlavinka <mhlavink@redhat.com> - 104-3
- fix pam sigsegv (#1184645)

* Sat Aug 16 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 104-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Wed Jul 23 2014 Michal Hlavinka <mhlavink@redhat.com> - 104-1
- ecryptfs-utils updated to 104

* Sat Jun 07 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 103-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Wed Nov 13 2013 Michal Hlavinka <mhlavink@redhat.com> - 103-4
- ecryptfs-migrate-home did not restore selinux labels (#1017402)

* Sat Aug 03 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 103-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Thu May 23 2013 Michal Hlavinka <mhlavink@redhat.com> - 103-2
- make executables hardened (#965505)

* Wed Jan 30 2013 Michal Hlavinka <mhlavink@redhat.com> - 103-1
- ecryptfs-utils updated to 103

* Mon Oct 29 2012 Michal Hlavinka <mhlavink@redhat.com> - 101-1
- ecryptfs-utils updated to 101

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
- ecryptfs-utils updated to 100

* Tue Jul 24 2012 Michal Hlavinka <mhlavink@redhat.com> - 99-1
- ecryptfs-utils updated to 99
- fixes: suid helper does not restrict mounting filesystems with 
  nosuid, nodev leading to possible privilege escalation (CVE-2012-3409)

* Wed Jul 18 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 97-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Mon Jun 25 2012 Michal Hlavinka <mhlavink@redhat.com> - 97-1
- ecryptfs-utils updated to 97

* Mon Jun 04 2012 Michal Hlavinka <mhlavink@redhat.com> - 96-3
- for file name encryption support check, module must be loaded already

* Mon Apr 16 2012 Michal Hlavinka <mhlavink@redhat.com> - 96-2
- when ecryptfs-mount-fails, check if user is member of ecryptfs group

* Mon Feb 20 2012 Michal Hlavinka <mhlavink@redhat.com> - 96-1
- ecryptfs-utils updated to 96

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
- ecryptfs-utils updated to 85

* Tue Jan 11 2011 Dan Horák <dan[at]danny.cz> - 84-3
- fix build on arches where char is unsigned by default

* Tue Jan 04 2011 Michal Hlavinka <mhlavink@redhat.com> - 84-2
- fix unsigned < 0 test (#664474)

* Mon Dec 20 2010 Michal Hlavinka <mhlavink@redhat.com> - 84-1
- ecryptfs-utils updated to 84

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
