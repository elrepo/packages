# $Id$
# Authority: dag

%define real_name drbd

Summary: Management utilities for DRBD %{version}
Name: drbd84-utils
Version: 8.4.0
Release: 1%{?dist}
License: GPLv2+
Group: System Environment/Kernel
URL: http://www.drbd.org/

Source: http://oss.linbit.com/drbd/8.4/drbd-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

BuildRequires: flex
BuildRequires: udev
Requires: chkconfig
Requires: udev

### Virtual provides that people may use
Provides: drbd = %{version}-%{release}
Provides: drbd83 = %{version}-%{release}
Provides: drbd84 = %{version}-%{release}

### Package drbd from CentOS is drbd 8.0, but from Linbit is 8.2 or 8.3
Conflicts: drbd < 8.2
Conflicts: drbd-utils < 8.2

### Obsolete packages from Linbit (included in this package)
Obsoletes: drbd <= %{version}-%{release}
Obsoletes: drbd-bash-completion <= %{version}-%{release}
Obsoletes: drbd-heartbeat <= %{version}-%{release}
Obsoletes: drbd-pacemaker <= %{version}-%{release}
Obsoletes: drbd-rgmanager <= %{version}-%{release}
Obsoletes: drbd-udev <= %{version}-%{release}
Obsoletes: drbd-utils <= %{version}-%{release}
Obsoletes: drbd-xen <= %{version}-%{release}

### Upgrade path from CentOS drbd82 to drbd83 is guaranteed
Obsoletes: drbd82 <= %{version}-%{release}
Obsoletes: drbd82-bash-completion <= %{version}-%{release}
Obsoletes: drbd82-heartbeat <= %{version}-%{release}
Obsoletes: drbd82-pacemaker <= %{version}-%{release}
Obsoletes: drbd82-rgmanager <= %{version}-%{release}
Obsoletes: drbd82-udev <= %{version}-%{release}
Obsoletes: drbd82-utils <= %{version}-%{release}
Obsoletes: drbd82-xen <= %{version}-%{release}

### Obsolete older CentOS packages
Obsoletes: drbd83 <= %{version}-%{release}
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
    --without-km \
    --with-initdir="%{_initrddir}" \
    --with-rgmanager \
    --with-utils
%{__make} %{?_smp_mflags}

%install
%{__rm} -rf %{buildroot}
%{__make} install DESTDIR="%{buildroot}"

%clean
%{__rm} -rf %{buildroot}

%post
/sbin/chkconfig --add drbd

for i in $(seq 0 15); do
    if [ ! -b /dev/drbd$i ]; then
        mknod -m0660 /dev/drbd$i b 147 $i
    fi
done

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
%doc %{_mandir}/man8/drbd.8*
%doc %{_mandir}/man8/drbdadm.8*
%doc %{_mandir}/man8/drbddisk.8*
%doc %{_mandir}/man8/drbdmeta.8*
%doc %{_mandir}/man8/drbdsetup.8*
%config %{_initrddir}/drbd
%config %{_sysconfdir}/bash_completion.d/drbdadm*
%config %{_sysconfdir}/udev/rules.d/65-drbd.rules*
%config(noreplace) %{_sysconfdir}/drbd.conf
%dir %{_sysconfdir}/drbd.d/
%config(noreplace) %{_sysconfdir}/drbd.d/global_common.conf
%dir %{_localstatedir}/lib/drbd/
/lib/drbd/drbdadm-83
/lib/drbd/drbdsetup-83
/sbin/drbdsetup
/sbin/drbdadm
/sbin/drbdmeta
%{_sbindir}/drbd-overview
%dir %{_prefix}/lib/drbd/
%{_prefix}/lib/drbd/outdate-peer.sh
%{_prefix}/lib/drbd/snapshot-resync-target-lvm.sh
%{_prefix}/lib/drbd/unsnapshot-resync-target-lvm.sh
%{_prefix}/lib/drbd/notify-out-of-sync.sh
%{_prefix}/lib/drbd/notify-split-brain.sh
%{_prefix}/lib/drbd/notify-emergency-reboot.sh
%{_prefix}/lib/drbd/notify-emergency-shutdown.sh
%{_prefix}/lib/drbd/notify-io-error.sh
%{_prefix}/lib/drbd/notify-pri-lost-after-sb.sh
%{_prefix}/lib/drbd/notify-pri-lost.sh
%{_prefix}/lib/drbd/notify-pri-on-incon-degr.sh
%{_prefix}/lib/drbd/notify.sh

### heartbeat
%{_sysconfdir}/ha.d/resource.d/drbddisk
%{_sysconfdir}/ha.d/resource.d/drbdupper

### pacemaker
%{_prefix}/lib/drbd/crm-fence-peer.sh
%{_prefix}/lib/drbd/crm-unfence-peer.sh
%{_prefix}/lib/ocf/resource.d/linbit/drbd

### rgmanager
%{_datadir}/cluster/drbd.sh
%{_datadir}/cluster/drbd.metadata

### xen
%{_sysconfdir}/xen/scripts/block-drbd

%changelog
* Mon Aug 08 2011 Dag Wieers <dag@elrepo.org> - 8.4.0-1
- Initial package for RHEL5.
