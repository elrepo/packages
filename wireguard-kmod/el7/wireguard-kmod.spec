# Define the kmod package name here.
%define kmod_name wireguard

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-1127.el7.%{_target_cpu}}

# define epoch to equal minor point release to ensure
# newer versions are not installed on older kernels
%if "%{kversion}" == "3.10.0-1127.el7.%{_target_cpu}"
Epoch:	8
%else
Epoch:	7
%endif

Name:    %{kmod_name}-kmod
Version: 1.0.20200908
Release: 1%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     https://git.zx2c4.com/wireguard-linux-compat/

BuildRequires: redhat-rpm-config, perl, bc
ExclusiveArch: x86_64

# Sources.
Source0:  https://git.zx2c4.com/wireguard-linux-compat/snapshot/wireguard-linux-compat-%{version}.tar.xz
Source5:  https://raw.githubusercontent.com/elrepo/packages/master/wireguard-kmod/el7/GPL-v2.0.txt
Source10: https://raw.githubusercontent.com/elrepo/packages/master/wireguard-kmod/el7/kmodtool-%{kmod_name}-el7.sh

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%autosetup -p1 -n wireguard-linux-compat-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD/src

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} src/%{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# Strip the modules(s).
find %{buildroot} -type f -name \*.ko -exec %{__strip} --strip-debug \{\} \;

# Sign the modules(s).
%if %{?_with_modsign:1}%{!?_with_modsign:0}
# If the module signing keys are not defined, define them here.
%{!?privkey: %define privkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.priv}
%{!?pubkey: %define pubkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.der}
for module in $(find %{buildroot} -type f -name \*.ko);
do %{__perl} /usr/src/kernels/%{kversion}/scripts/sign-file \
    sha256 %{privkey} %{pubkey} $module;
done
%endif

%clean
%{__rm} -rf %{buildroot}

%changelog
* Tue Sep 08 2020 Philip J Perry <phil@elrepo.org> 1.0.20200908-1
- Update to 1.0.20200908

* Wed Jul 29 2020 Philip J Perry <phil@elrepo.org> 1.0.20200729-1
- Update to 1.0.20200729
- Define epoch to equal minor point release

* Mon Jul 13 2020 Philip J Perry <phil@elrepo.org> 1.0.20200712-1
- Update to 1.0.20200712

* Wed Jun 24 2020 Philip J Perry <phil@elrepo.org> 1.0.20200623-1
- Update to 1.0.20200623

* Thu Jun 11 2020 Philip J Perry <phil@elrepo.org> 1.0.20200611-1
- Update to 1.0.20200611

* Thu May 21 2020 Philip J Perry <phil@elrepo.org> 1.0.20200520-1
- Update to 1.0.20200520
- qemu: use newer iproute2 for gcc-10
- qemu: add -fcommon for compiling ping with gcc-10
- noise: read preshared key while taking lock
- queueing: preserve flow hash across packet scrubbing
- noise: separate receive counter from send counter
- compat: support RHEL 8 as 8.2, drop 8.1 support
- compat: support CentOS 8 explicitly
- compat: RHEL7 backported the skb hash renamings
- compat: backport renamed/missing skb hash members
- compat: ip6_dst_lookup_flow was backported to 4.14, 4.9, and 4.4

* Wed May 06 2020 Joe Doss <joe@solidadmin.com> 1.0.20200506-1
- Update to 1.0.20200506
- compat: timeconst.h is a generated artifact
- qemu: loop entropy adding until getrandom doesn't block
- compat: detect Debian's backport of ip6_dst_lookup_flow into 4.19.118
- compat: use bash instead of bc for HZ-->USEC calculation
- qemu: use normal kernel stack size on ppc64
- socket: remove errant restriction on looping to self
- send: cond_resched() when processing tx ringbuffers
- compat: Ubuntu 19.10 and 18.04-hwe backported skb_reset_redirect
- selftests: initalize ipv6 members to NULL to squelch clang warning
- send/receive: use explicit unlikely branch instead of implicit coalescing

* Thu Apr 30 2020 Joe Doss <joe@solidadmin.com> 1.0.20200429-1
- Update to 1.0.20200429
- receive: use tunnel helpers for decapsulating ECN markings
- compat: ip6_dst_lookup_flow was backported to 3.16.83
- compat: ip6_dst_lookup_flow was backported to 4.19.119

* Mon Apr 27 2020 Joe Doss <joe@solidadmin.com> 1.0.20200426-1
- Update to 1.0.20200426
- crypto: do not export symbols
- compat: include sch_generic.h header for skb_reset_tc
- compat: import latest fixes for ptr_ring
- compat: don't assume READ_ONCE barriers on old kernels
- compat: kvmalloc_array is not required anyway
- queueing: cleanup ptr_ring in error path of packet_queue_init
- main: mark as in-tree
- compat: prefix icmp[v6]_ndo_send with __compat

* Tue Apr 14 2020 Joe Doss <joe@solidadmin.com> 1.0.20200413-1
- Update to 1.0.20200413
- compat: support latest suse 15.1 and 15.2
- compat: support RHEL 7.8 faulty siphash backport
- compat: error out if bc is missing
- compat: backport hsiphash_1u32 for tests

* Wed Apr 1 2020 Joe Doss <joe@solidadmin.com> 1.0.20200401-1
- Update to 1.0.20200401
- compat: queueing: skb_reset_redirect change has been backported to 5.[45]
- qemu: bump default kernel to 5.5.14

* Fri Feb 14 2020 Akemi Yagi <toracat@elrepo.org> - 0.0.20200205-1
- Initial el7 build.
- [http://elrepo.org/bugs/view.php?id=989]
