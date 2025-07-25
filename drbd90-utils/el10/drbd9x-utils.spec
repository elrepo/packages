%define real_name drbd-utils

Name:    drbd9x-utils
Version: 9.31.0
Release: 1%{?dist}
Group:   System Environment/Kernel
License: GPLv2+
Summary: Management utilities for DRBD
URL:     http://www.drbd.org/

Source0:   http://oss.linbit.com/drbd/drbd-utils-%{version}.tar.gz

Patch1: elrepo-selinux-bug695v2.patch

BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: flex
BuildRequires: udev
BuildRequires: libxslt
BuildRequires: docbook-style-xsl
BuildRequires: po4a
# as of 9.26.0
BuildRequires: keyutils-libs-devel

Requires: udev
Requires(post):   systemd-units
Requires(preun):  systemd-units
Requires(postun): systemd-units

### Virtual provides that people may use
Provides: drbd = %{version}-%{release}
Provides: drbd9x = %{version}-%{release}

### Conflict with older Linbit packages
Conflicts: drbd < 9.0
Conflicts: drbd-utils < 9.0

### Conflict with older packages
Conflicts: drbd82 <= %{version}-%{release}
Conflicts: drbd82-utils <= %{version}-%{release}
Conflicts: drbd83 <= %{version}-%{release}
Conflicts: drbd83-utils <= %{version}-%{release}
Conflicts: drbd84 <= %{version}-%{release}
Conflicts: drbd84-utils <= %{version}-%{release}

%package sysvinit
Summary: The SysV initscript to manage the DRBD.
Group: System Environment/Daemons
Requires: %{name}%{?_isa} = %{version}-%{release}

%description
DRBD mirrors a block device over the network to another machine.
Think of it as networked raid 1. It is a building block for
setting up high availability (HA) clusters.

This packages includes the DRBD administration tools and integration
scripts for heartbeat, pacemaker, rgmanager and xen.

%description sysvinit
DRBD mirrors a block device over the network to another machine.
Think of it as networked raid 1. It is a building block for
setting up high availability (HA) clusters.

This package contains the SysV init script to manage the DRBD when
running a legacy SysV-compatible init system.

It is not required when the init system used is systemd.

### To prevent empty debug files  -ay
%define debug_package %{nil}

%prep
%setup -n %{real_name}-%{version}
%patch 1 -p1

%build
%configure \
    --with-prebuiltman \
    --with-initdir="%{_initrddir}" \
    --with-rgmanager \
    --with-initscripttype=both
WITH_HEARTBEAT=yes %{__make} %{?_smp_mflags}

%install
%{__rm} -rf %{buildroot}
WITH_HEARTBEAT=yes %{__make} install DESTDIR="%{buildroot}"
pushd scripts
WITH_HEARTBEAT=yes %{__make} install-heartbeat DESTDIR="%{buildroot}"
popd


%clean
%{__rm} -rf %{buildroot}

%post
%systemd_post drbd.service

for i in $(seq 0 15); do
    if [ ! -b /dev/drbd$i ]; then
        mknod -m0660 /dev/drbd$i b 147 $i
    fi
done

if /usr/bin/getent group | grep -q ^haclient; then
    chgrp haclient /usr/sbin/drbdsetup
    chmod o-x,u+s /usr/sbin/drbdsetup
    chgrp haclient /usr/sbin/drbdmeta
    chmod o-x,u+s /usr/sbin/drbdmeta
fi

%preun
%systemd_preun drbd.service

%postun
%systemd_postun_with_restart drbd.service

%files
%defattr(-, root, root, 0755)
%doc ChangeLog COPYING README.md scripts/drbd.conf.example
%doc %{_mandir}/man5/drbd.conf-*
%doc %{_mandir}/man8/drbd*
%doc %{_mandir}/ja/man5/drbd.conf-*
%doc %{_mandir}/ja/man8/drbd*
%doc %{_mandir}/man7/ocf_linbit_drbd.7.gz
%doc %{_mandir}/man7/ocf_linbit_drbd-attr.7.gz
%doc %{_mandir}/man7/drbd-lvchange@.service.7.gz
%doc %{_mandir}/man7/drbd-promote@.service.7.gz
%doc %{_mandir}/man7/drbd-reconfigure-suspend-or-error@.service.7.gz
%doc %{_mandir}/man7/drbd-services@.target.7.gz
%doc %{_mandir}/man7/drbd-wait-promotable@.service.7.gz
%doc %{_mandir}/man7/drbd.service.7.gz
%doc %{_mandir}/man7/drbd@.service.7.gz
%doc %{_mandir}/man7/drbd@.target.7.gz
%doc %{_mandir}/man7/ocf.ra@.service.7.gz
%doc %{_mandir}/man7/drbd-configured.target.7.gz
%doc %{_mandir}/man7/drbd-graceful-shutdown.service.7.gz

%config %{_sysconfdir}/bash_completion.d/drbdadm
%config %{_prefix}/lib/udev/rules.d/65-drbd.rules
%config(noreplace) %{_sysconfdir}/drbd.conf
%config(noreplace) %{_sysconfdir}/multipath/conf.d/drbd.conf
%dir %{_sysconfdir}/drbd.d/
%config(noreplace) %{_sysconfdir}/drbd.d/global_common.conf
%config %{_unitdir}/drbd.service
%dir %{_localstatedir}/lib/drbd/
%dir /usr/lib/drbd/
# /lib/drbd/drbdadm-83
# /lib/drbd/drbdsetup-83
/usr/lib/drbd/drbdadm-84
/usr/lib/drbd/drbdsetup-84
/usr/lib/drbd/scripts/drbd
/usr/lib/drbd/scripts/drbd-service-shim.sh
/usr/lib/drbd/scripts/drbd-wait-promotable.sh
/usr/lib/drbd/scripts/ocf.ra.wrapper.sh
/usr/lib/systemd/system-preset/50-drbd.preset
/usr/lib/systemd/system/drbd-configured.target
/usr/lib/systemd/system/drbd-demote-or-escalate@.service
/usr/lib/systemd/system/drbd-lvchange@.service
/usr/lib/systemd/system/drbd-promote@.service
/usr/lib/systemd/system/drbd-reconfigure-suspend-or-error@.service
/usr/lib/systemd/system/drbd-services@.target
/usr/lib/systemd/system/drbd-wait-promotable@.service
/usr/lib/systemd/system/drbd@.service
/usr/lib/systemd/system/drbd@.target
/usr/lib/systemd/system/ocf.ra@.service
/usr/lib/systemd/system/drbd-graceful-shutdown.service
%{_sbindir}/drbdadm
%{_sbindir}/drbdmeta
%{_sbindir}/drbdsetup
%{_sbindir}/drbdmon
%{_sbindir}/drbd-events-log-supplier
# %dir %%{_prefix}/lib/drbd/
%{_prefix}/lib/drbd/notify-out-of-sync.sh
%{_prefix}/lib/drbd/notify-split-brain.sh
%{_prefix}/lib/drbd/notify-emergency-reboot.sh
%{_prefix}/lib/drbd/notify-emergency-shutdown.sh
%{_prefix}/lib/drbd/notify-io-error.sh
%{_prefix}/lib/drbd/notify-pri-lost-after-sb.sh
%{_prefix}/lib/drbd/notify-pri-lost.sh
%{_prefix}/lib/drbd/notify-pri-on-incon-degr.sh
%{_prefix}/lib/drbd/notify.sh
%{_prefix}/lib/drbd/outdate-peer.sh
%{_prefix}/lib/drbd/snapshot-resync-target-lvm.sh
%{_prefix}/lib/drbd/stonith_admin-fence-peer.sh
%{_prefix}/lib/drbd/unsnapshot-resync-target-lvm.sh
%{_prefix}/lib/tmpfiles.d/drbd.conf
%{_prefix}/lib/ocf/resource.d/linbit/drbd-attr

### heartbeat
%{_sysconfdir}/ha.d/resource.d/drbddisk
%{_sysconfdir}/ha.d/resource.d/drbdupper

### pacemaker
%{_prefix}/lib/drbd/crm-fence-peer.sh
%{_prefix}/lib/drbd/crm-unfence-peer.sh
%{_prefix}/lib/ocf/resource.d/linbit/drbd
%{_prefix}/lib/drbd/crm-fence-peer.9.sh
%{_prefix}/lib/drbd/crm-unfence-peer.9.sh
%{_prefix}/lib/ocf/resource.d/linbit/drbd.shellfuncs.sh

### rgmanager / rhcs
%{_datadir}/cluster/drbd.sh
%{_datadir}/cluster/drbd.metadata
%{_prefix}/lib/drbd/rhcs_fence

%files sysvinit
%defattr(-,root,root)
%config %{_initrddir}/drbd

%changelog
* Thu May 22 2025 Akemi Yagi <toracat@elrepo.org> - 9.31.0-1.el10
- Version updated to 9.31.0
- Rebuilt against RHEL 10 GA kernel
- elrepo-drbd-graceful-shutdown.patch no longer needed

* Thu Mar 20 2025 Akemi Yagi <toracat@elrepo.org> - 9.30.0-2.el10
- Rebuilt for RHEL 10 beta

* Mon Jan 27 2025 Akemi Yagi <toracat@elrepo.org> - 9.30.0-2.el9
- Added elrepo-drbd-graceful-shutdown.patch
  [https://elrepo.org/bugs/view.php?id=1488]

* Thu Jan 23 2025 Akemi Yagi <toracat@elrepo.org> - 9.30.0-1.el9
- Updated to 9.30.0

* Wed Oct 30 2024 Akemi Yagi <toracat@elrepo.org> - 9.29.0-1.el9
- Updated to 9.29.0

* Sat May 11 2024 Akemi Yagi <toracat@elrepo.org> - 9.28.0-1.el9
- Updated to 9.28.0

* Fri Dec 22 2023 Akemi Yagi <toracat@elrepo.org> - 9.27.0-1.el9
- Updated to 9.27.0

* Tue Oct 31 2023 Akemi Yagi <toracat@elrepo.org> - 9.26.0-1.el9
- Updated to 9.26.0
- BuildRequires: keyutils-libs-devel needed to build in mock.

* Sun Jul 30 2023 Akemi Yagi <toracat@elrepo.org> - 9.25.0-1.el9
- Updated to 9.25.0-1
- elrepo patch updated to v2.

* Fri Jun 16 2023 Akemi Yagi <toracat@elrepo.org> - 9.24.0-1.el9
- Updated to 9.24.0

* Sat Mar 18 2023 Akemi Yagi <toracat@elrepo.org> - 9.23.1-1.el9
- Updated to 9.23.1

* Sat Dec 24 2022 Akemi Yagi <toracat@elrepo.org> - 9.22.0-1.el9
- Updated to 9.22.0

* Tue May 24 2022 Akemi Yagi <toracat@elrepo.org> - 9.21.1-1.el9
- Renamed to drbd9x-utils
- Initial build for RHEL 9

* Sat May 21 2022 Akemi Yagi <toracat@elrepo.org> - 9.21.1-1
- Updated to 9.21.1

* Thu Jan 13 2022 Akemi Yagi <toracat@elrepo.org> - 9.20.0-1
- Updated to 9.20.0

* Mon Dec 13 2021 Akemi Yagi <toracat@elrepo.org> - 9.19.1-1
- Updated to 9.19.1

* Thu Jun 03 2021 Akemi Yagi <toracat@elrepo.org> - 9.17.0-1
- Updated to 9.17.0

* Sat Nov 07 2020 Akemi Yagi <toracat@elrepo.org> - 9.15.1-1
- Updated to 9.15.1

* Tue Jun 23 2020 Akemi Yagi <toracat@elrepo.org> - 9.13.1-1
- Updated to 9.13.1

* Thu Nov 07 2019 Akemi Yagi <toracat@elrepo.org> - 9.10.0-2
- Built for el8

* Thu Oct 17 2019 Akemi Yagi <toracat@elrepo.org> - 9.10.0-1
- Updated to 9.10.0

* Sat Nov 03 2018 Akemi Yagi <toracat@elrepo.org> - 9.6.0-1
- Updated to 9.6.0

* Wed Apr 18 2018 Akemi Yagi <toracat@elrepo.org> - 9.3.1-1
- Updated to 9.3.1

* Thu Sep 14 2017 Akemi Yagi <toracat@elrepo.org> - 9.1.0-1
- Updated to 9.1.0

* Mon Jun 12 2017 Akemi Yagi <toracat@elrepo.org> - 9.0.0-1
- Updated to 9.0.0
- xmlto replaced with docbook-style-xsl

* Sat Dec  3 2016 Akemi Yagi <toracat@elrepo.org> - 8.9.8-1
- update to version 8.9.8.
- Bug fix (elrepo bug #695)

* Wed Oct  5 2016 Hiroshi Fujishima <h-fujishima@sakura.ad.jp> - 8.9.6-1
- Update to version 8.9.6.
- BuildRequires: xmlto added by A. Yagi for building in mock.

* Mon Jan  4 2016 Hiroshi Fujishima <h-fujishima@sakura.ad.jp> - 8.9.5-1
- Update to version 8.9.5.

* Sat Aug 15 2015 Akemi Yagi <toracat@elrepo.org> - 8.9.3-1.1
- Patch drbd.ocf to the version from 8.9.3-2 (bugs #578 and #589)

* Wed Jun 24 2015 Hiroshi Fujishima <h-fujishima@sakura.ad.jp> - 8.9.3-1
- Initial package for RHEL7.
