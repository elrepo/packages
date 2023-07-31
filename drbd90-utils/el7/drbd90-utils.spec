%define real_name drbd-utils

Name:    drbd90-utils
Version: 9.25.0
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
# Build Requires rubygem-asciidoctor from EPEL
BuildRequires: asciidoctor

Requires: udev
Requires(post):   systemd-units
Requires(preun):  systemd-units
Requires(postun): systemd-units

### Virtual provides that people may use
Provides: drbd = %{version}-%{release}
Provides: drbd90 = %{version}-%{release}

### Conflict with older Linbit packages
Conflicts: drbd < 9.0
Conflicts: drbd-utils < 9.0

### Conflict with older CentOS packages
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

%prep
%setup -n %{real_name}-%{version}
%patch1 -p1

%build
%configure \
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
%doc %{_mandir}/man5/drbd.conf.5*
%doc %{_mandir}/man5/drbd.conf-*
%doc %{_mandir}/man8/drbd*
%doc %{_mandir}/ja/man5/drbd.conf.5*
%doc %{_mandir}/ja/man5/drbd.conf-*
%doc %{_mandir}/ja/man8/drbd*
%doc %{_mandir}/man7/*
%config %{_sysconfdir}/bash_completion.d/drbdadm
%config %{_prefix}/lib/udev/rules.d/65-drbd.rules
%config(noreplace) %{_sysconfdir}/drbd.conf
%config(noreplace) %{_sysconfdir}/multipath/conf.d/drbd.conf
%dir %{_sysconfdir}/drbd.d/
%config(noreplace) %{_sysconfdir}/drbd.d/global_common.conf
%config %{_unitdir}/drbd.service
%dir %{_localstatedir}/lib/drbd/
%dir /lib/drbd/
/lib/drbd/drbdadm-83
/lib/drbd/drbdsetup-83
/lib/drbd/drbdadm-84
/lib/drbd/drbdsetup-84
%dir /lib/drbd/scripts/
/lib/drbd/scripts/*
%{_sbindir}/drbdadm
%{_sbindir}/drbdmeta
%{_sbindir}/drbdsetup
%{_sbindir}/drbdmon
%dir %{_prefix}/lib/drbd/
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
%{_prefix}/lib/systemd/system/*
%{_prefix}/lib/tmpfiles.d/drbd.conf

### heartbeat
%{_sysconfdir}/ha.d/resource.d/drbddisk
%{_sysconfdir}/ha.d/resource.d/drbdupper

### pacemaker
%{_prefix}/lib/drbd/crm-fence-peer.sh
%{_prefix}/lib/drbd/crm-unfence-peer.sh
%{_prefix}/lib/ocf/resource.d/linbit/*
%{_prefix}/lib/drbd/crm-fence-peer.9.sh
%{_prefix}/lib/drbd/crm-unfence-peer.9.sh
%{_prefix}/lib/ocf/resource.d/linbit/drbd.shellfuncs.sh

### rgmanager / rhcs
%{_datadir}/cluster/drbd.sh
%{_datadir}/cluster/drbd.metadata
%{_prefix}/lib/drbd/rhcs_fence

### xen
%{_sysconfdir}/xen/scripts/block-drbd

%files sysvinit
%defattr(-,root,root)
%config %{_initrddir}/drbd

%changelog
* Sun Jul 30 2023 Akemi Yagi <toracat@elrepo.org> - 9.25.0-1
- Updated to 9.25.0-1
- elrepo patch updated to v2.

* Fri Jun 16 2023 Akemi Yagi <toracat@elrepo.org> - 9.24.0-1
- Updated to 9.24.0

* Sat Mar 18 2023 Akemi Yagi <toracat@elrepo.org> - 9.23.0-1
- Updated to 9.23.1
  [https://elrepo.org/bugs/view.php?id=1332]

* Sun Oct 09 2022 Akemi Yagi <toracat@elrepo.org> - 9.22.0-1
- Updated to 9.22.0
  [https://elrepo.org/bugs/view.php?id=1278]

* Tue May 24 2022 Akemi Yagi <toracat@elrepo.org> - 9.21.1-1
- Updated to 9.21.1

* Fri Apr 01 2022 Akemi Yagi <toracat@elrepo.org> - 9.20.2-1
- Updated to 9.20.2
  [https://elrepo.org/bugs/view.php?id=1217]

* Mon Dec 13 2021 Philip J Perry <phil@elrepo.org> - 9.19.1-1
- Updated to 9.19.1
  [https://elrepo.org/bugs/view.php?id=1171]

* Fri Jul 30 2021 Philip J Perry <phil@elrepo.org> - 9.18.1-1
- Updated to 9.18.1
  [https://elrepo.org/bugs/view.php?id=1124]

* Sat Apr 04 2020 Akemi Yagi <toracat@elrepo.org> - 9.12.2-1
- Updated to 9.12.2

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
