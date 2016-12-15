# $Id$
# Authority: dag

%define real_name drbd-utils

Summary: Management utilities for DRBD
Name: drbd84-utils
Version: 8.9.8
Release: 1%{?dist}
License: GPLv2+
Group: System Environment/Kernel
URL: http://www.drbd.org/

Source: http://oss.linbit.com/drbd/drbd-utils-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

BuildRequires: flex
BuildRequires: udev
BuildRequires: libxslt
BuildRequires: xmlto
Requires: chkconfig
Requires: udev

### Virtual provides that people may use
Provides: drbd = %{version}-%{release}
Provides: drbd84 = %{version}-%{release}

### Conflict with older Linbit packages
Conflicts: drbd < 8.4
Conflicts: drbd-utils < 8.4

### Conflict with older CentOS packages
Conflicts: drbd82 <= %{version}-%{release}
Conflicts: drbd82-utils <= %{version}-%{release}
Conflicts: drbd83 <= %{version}-%{release}
Conflicts: drbd83-utils <= %{version}-%{release}
Obsoletes: drbd84 <= %{version}-%{release}

%description
DRBD mirrors a block device over the network to another machine.
Think of it as networked raid 1. It is a building block for
setting up high availability (HA) clusters.

This packages includes the DRBD administration tools and integration
scripts for heartbeat, pacemaker, rgmanager and xen.

%prep
%setup -n %{real_name}-%{version}

%build
%configure \
    --with-initdir="%{_initrddir}" \
    --with-rgmanager \
    --with-initscripttype=sysv \
    --without-83support
WITH_HEARTBEAT=yes %{__make} %{?_smp_mflags}

%install
%{__rm} -rf %{buildroot}
WITH_HEARTBEAT=yes %{__make} install DESTDIR="%{buildroot}"
pushd scripts
WITH_HEARTBEAT=yes %{__make} install-heartbeat DESTDIR="%{buildroot}"
popd

# Moved to /usr/sbin, symlink to /sbin for compatibility
%{__mkdir_p} %{buildroot}/sbin/
%{__ln_s} %{_sbindir}/drbdadm %{buildroot}/sbin/
%{__ln_s} %{_sbindir}/drbdmeta %{buildroot}/sbin/
%{__ln_s} %{_sbindir}/drbdsetup %{buildroot}/sbin/

%clean
%{__rm} -rf %{buildroot}

%post
/sbin/chkconfig --add drbd

if /usr/bin/getent group | grep -q ^haclient; then
    chgrp haclient /sbin/drbdsetup
    chmod o-x,u+s /sbin/drbdsetup
    chgrp haclient /sbin/drbdmeta
    chmod o-x,u+s /sbin/drbdmeta
fi

%preun
if [ $1 -eq 0 ]; then
    %{_initrddir}/drbd stop &>/dev/null
    /sbin/chkconfig --del drbd
fi

%files
%defattr(-, root, root, 0755)
%doc ChangeLog COPYING README scripts/drbd.conf.example
%doc %{_mandir}/man5/drbd.conf.5*
%doc %{_mandir}/man5/drbd.conf-*
%doc %{_mandir}/man8/drbd.8*
%doc %{_mandir}/man8/drbd-*
%doc %{_mandir}/man8/drbdadm.8*
%doc %{_mandir}/man8/drbdadm-*
%doc %{_mandir}/man8/drbddisk-*
%doc %{_mandir}/man8/drbdmeta.8*
%doc %{_mandir}/man8/drbdmeta-*
%doc %{_mandir}/man8/drbdsetup.8*
%doc %{_mandir}/man8/drbdsetup-*
%config %{_initrddir}/drbd
%config %{_sysconfdir}/bash_completion.d/drbdadm*
%config(noreplace) %{_sysconfdir}/drbd.conf
%dir %{_sysconfdir}/drbd.d/
%config(noreplace) %{_sysconfdir}/drbd.d/global_common.conf
%dir %{_localstatedir}/lib/drbd/
/lib/drbd/drbdadm-84
/lib/drbd/drbdsetup-84
/lib/udev/rules.d/65-drbd.rules
/sbin/drbdadm
/sbin/drbdmeta
/sbin/drbdsetup
%{_sbindir}/drbdadm
%{_sbindir}/drbdmeta
%{_sbindir}/drbdsetup
%{_sbindir}/drbd-overview
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

### heartbeat
%{_sysconfdir}/ha.d/resource.d/drbddisk
%{_sysconfdir}/ha.d/resource.d/drbdupper

### pacemaker
%{_prefix}/lib/drbd/crm-fence-peer.sh
%{_prefix}/lib/drbd/crm-unfence-peer.sh
%{_prefix}/lib/ocf/resource.d/linbit/drbd

### rgmanager / rhcs
%{_datadir}/cluster/drbd.sh
%{_datadir}/cluster/drbd.metadata
%{_prefix}/lib/drbd/rhcs_fence

### xen
%{_sysconfdir}/xen/scripts/block-drbd

%changelog
* Tue Dec 13 2016 Akemi Yagi <toracat@elrepo.org> - 8.9.8-1
- Updated to version 8.9.8.
- Add in BuildRequires: xmlto to allow building in mock.

* Sun Jan  3 2016 Hiroshi Fujishima <h-fujishima@sakura.ad.jp> - 8.9.5-1
- Updated to version 8.9.5.

* Sun Aug 17 2014 Jun Futagawa <jfut@integ.jp> - 8.9.1-1
- Updated to version 8.9.1.

* Fri Oct 25 2013 Philip J Perry <phil@elrepo.org> - 8.4.4-2
- Add symlinks for drbd files moved from /sbin to /usr/sbin
  [http://elrepo.org/bugs/view.php?id=418]

* Sat Oct 12 2013 Philip J Perry <phil@elrepo.org> - 8.4.4-1
- Updated to release 8.4.4.

* Sat Jul 06 2013 Akemi Yagi <toracat@elrepo.org> - 8.4.3-1
-Updated to release 8.4.3.

* Thu Sep 06 2012 Dag Wieers <dag@elrepo.org> - 8.4.1-2
- Updated to release 8.4.2.

* Thu Apr 26 2012 Akemi Yagi <toracat@elrepo.org> - 8.4.1-2
- Updated to release 8.4.1-2.

* Wed Dec 21 2011 Dag Wieers <dag@elrepo.org> - 8.4.1-1
- Updated to release 8.4.1.

* Fri Aug 12 2011 Dag Wieers <dag@elrepo.org> - 8.4.0-2
- Conflicts with older drbd, drbd82 and drbd83 packages.

* Mon Aug 08 2011 Dag Wieers <dag@elrepo.org> - 8.4.0-1
- Initial package for RHEL6.
