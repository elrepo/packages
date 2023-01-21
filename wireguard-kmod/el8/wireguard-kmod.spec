# Define the kmod package name here.
%define kmod_name		wireguard

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 4.18.0-425.10.1.el8_7}

%{!?dist: %define dist .el8}

# define epoch to equal minor point release to ensure
# newer versions are not installed on older kernels
%if "%{kmod_kernel_version}" == "4.18.0-372.13.1.el8_6"
Epoch:	6
%else
Epoch:	7
%endif

Name:		kmod-%{kmod_name}
Version:	1.0.20220627
Release:	4%{?dist}
Summary:	%{kmod_name} kernel module(s)
Group:		System Environment/Kernel
License:	GPLv2
URL:		https://git.zx2c4.com/wireguard-linux-compat/

# Sources
Source0:  https://git.zx2c4.com/wireguard-linux-compat/snapshot/wireguard-linux-compat-%{version}.tar.xz
Source5:  https://raw.githubusercontent.com/elrepo/packages/master/wireguard-kmod/el8/GPL-v2.0.txt

# Source code patches
Patch0:		elrepo-wireguard-backports.el8_7.patch

%define __spec_install_post /usr/lib/rpm/check-buildroot \
                            /usr/lib/rpm/redhat/brp-ldconfig \
                            /usr/lib/rpm/brp-compress \
                            /usr/lib/rpm/brp-strip-comment-note /usr/bin/strip /usr/bin/objdump \
                            /usr/lib/rpm/brp-strip-static-archive /usr/bin/strip \
                            /usr/lib/rpm/brp-python-bytecompile "" 1 \
                            /usr/lib/rpm/brp-python-hardlink \
                            PYTHON3="/usr/libexec/platform-python" /usr/lib/rpm/redhat/brp-mangle-shebangs
%define findpat %( echo "%""P" )
%define __find_requires /usr/lib/rpm/redhat/find-requires.ksyms
%define __find_provides /usr/lib/rpm/redhat/find-provides.ksyms %{kmod_name} %{?epoch:%{epoch}:}%{version}-%{release}
%define dup_state_dir %{_localstatedir}/lib/rpm-state/kmod-dups
%define kver_state_dir %{dup_state_dir}/kver
%define kver_state_file %{kver_state_dir}/%{kmod_kernel_version}.%{_arch}
%define dup_module_list %{dup_state_dir}/rpm-kmod-%{kmod_name}-modules
%define debug_package %{nil}

%global _use_internal_dependency_generator 0
%global kernel_source() %{_usrsrc}/kernels/%{kmod_kernel_version}.%{_arch}

BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

ExclusiveArch:	x86_64

BuildRequires:	elfutils-libelf-devel
BuildRequires:	kernel-devel = %{kmod_kernel_version}
BuildRequires:	kernel-abi-whitelists
BuildRequires:	kernel-rpm-macros
BuildRequires:	redhat-rpm-config

Provides:	kernel-modules >= %{kmod_kernel_version}.%{_arch}
Provides:	kmod-%{kmod_name} = %{?epoch:%{epoch}:}%{version}-%{release}

Requires(post):	%{_sbindir}/weak-modules
Requires(postun):	%{_sbindir}/weak-modules
Requires:	kernel >= %{kmod_kernel_version}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%autosetup -p1 -n wireguard-linux-compat-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

# Apply patch(es)
## autosetup used, no need to specify patches here

%build

%{__make} -C %{kernel_source} %{?_smp_mflags} V=1 modules M=$PWD/src

whitelist="/lib/modules/kabi-current/kabi_whitelist_%{_target_cpu}"
for modules in $( find . -name "*.ko" -type f -printf "%{findpat}\n" | sed 's|\.ko$||' | sort -u ) ; do
	# update greylist
	nm -u ./$modules.ko | sed 's/.*U //' |  sed 's/^\.//' | sort -u | while read -r symbol; do
		grep -q "^\s*$symbol\$" $whitelist || echo "$symbol" >> ./greylist
	done
done
sort -u greylist | uniq > greylist.txt

%install
%{__install} -d %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} src/%{kmod_name}.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -m 0644 %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -m 0644 greylist.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# strip the modules(s)
find %{buildroot} -name \*.ko -type f | xargs --no-run-if-empty %{__strip} --strip-debug

# Sign the modules(s)
%if %{?_with_modsign:1}%{!?_with_modsign:0}
# If the module signing keys are not defined, define them here.
%{!?privkey: %define privkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.priv}
%{!?pubkey: %define pubkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.der}
for module in $(find %{buildroot} -type f -name \*.ko);
do %{_usrsrc}/kernels/%{kmod_kernel_version}.%{_arch}/scripts/sign-file \
sha256 %{privkey} %{pubkey} $module;
done
%endif

%clean
%{__rm} -rf %{buildroot}

%post
modules=( $(find /lib/modules/%{kmod_kernel_version}.x86_64/extra/%{kmod_name} | grep '\.ko$') )
printf '%s\n' "${modules[@]}" | %{_sbindir}/weak-modules --add-modules --no-initramfs

mkdir -p "%{kver_state_dir}"
touch "%{kver_state_file}"

exit 0

%posttrans
# We have to re-implement part of weak-modules here because it doesn't allow
# calling initramfs regeneration separately
if [ -f "%{kver_state_file}" ]; then
	kver_base="%{kmod_kernel_version}"
	kvers=$(ls -d "/lib/modules/${kver_base%%.*}"*)

	for k_dir in $kvers; do
		k="${k_dir#/lib/modules/}"

		tmp_initramfs="/boot/initramfs-$k.tmp"
		dst_initramfs="/boot/initramfs-$k.img"

		# The same check as in weak-modules: we assume that the kernel present
		# if the symvers file exists.
		if [ -e "$k_dir/symvers.gz" ]; then
			/usr/bin/dracut -f "$tmp_initramfs" "$k" || exit 1
			cmp -s "$tmp_initramfs" "$dst_initramfs"
			if [ "$?" = 1 ]; then
				mv "$tmp_initramfs" "$dst_initramfs"
			else
				rm -f "$tmp_initramfs"
			fi
		fi
	done

	rm -f "%{kver_state_file}"
	rmdir "%{kver_state_dir}" 2> /dev/null
fi

rmdir "%{dup_state_dir}" 2> /dev/null

exit 0

%preun
if rpm -q --filetriggers kmod 2> /dev/null| grep -q "Trigger for weak-modules call on kmod removal"; then
	mkdir -p "%{kver_state_dir}"
	touch "%{kver_state_file}"
fi

mkdir -p "%{dup_state_dir}"
rpm -ql kmod-%{kmod_name}-%{version}-%{release}.%{_arch} | grep '\.ko$' > "%{dup_module_list}"

%postun
if rpm -q --filetriggers kmod 2> /dev/null| grep -q "Trigger for weak-modules call on kmod removal"; then
	initramfs_opt="--no-initramfs"
else
	initramfs_opt=""
fi

modules=( $(cat "%{dup_module_list}") )
rm -f "%{dup_module_list}"
printf '%s\n' "${modules[@]}" | %{_sbindir}/weak-modules --remove-modules $initramfs_opt

rmdir "%{dup_state_dir}" 2> /dev/null

exit 0

%files
%defattr(644,root,root,755)
/lib/modules/%{kmod_kernel_version}.%{_arch}/
%config /etc/depmod.d/kmod-%{kmod_name}.conf
%doc /usr/share/doc/kmod-%{kmod_name}-%{version}/

%changelog
* Sat Jan 14 2023 Philip J Perry <phil@elrepo.org> 1.0.20220627-4
- Rebuilt against kernel-4.18.0-425.10.1.el8_7
  [http://lists.elrepo.org/pipermail/elrepo/2023-January/006336.html]

* Thu Nov 10 2022 Philip J Perry <phil@elrepo.org> 1.0.20220627-3
- Rebuilt for RHEL 8.7
- Fix backport of ktime_get_coarse_boottime_ns on RHEL 8.7
  [https://elrepo.org/bugs/view.php?id=1283]

* Sat Jul 02 2022 Philip J Perry <phil@elrepo.org> 1.0.20220627-2
- Rebuild against kernel-4.18.0-372.13.1.el8_6 for kABI breakage

* Mon Jun 27 2022 Philip J Perry <phil@elrepo.org> 1.0.20220627-1
- Update to 1.0.20220627

* Tue May 10 2022 Philip J Perry <phil@elrepo.org> 1.0.20211208-2
- Rebuilt for RHEL 8.6

* Wed Dec 08 2021 Philip J Perry <phil@elrepo.org> 1.0.20211208-1
- Update to 1.0.20211208

* Thu Nov 11 2021 Philip J Perry <phil@elrepo.org> 1.0.20210808-1
- Update to 1.0.20210808 git snapshot
- Rebuilt for RHEL 8.5
- Fix SB-signing issue caused by /usr/lib/rpm/brp-strip
  [https://bugzilla.redhat.com/show_bug.cgi?id=1967291]
- Update stripping of modules

* Sun Jun 06 2021 Philip J Perry <phil@elrepo.org> 1.0.20210606-1
- Update to 1.0.20210606

* Tue May 18 2021 Philip J Perry <phil@elrepo.org> 1.0.20210424-2
- Rebuilt for RHEL 8.4

* Sun Apr 25 2021 Philip J Perry <phil@elrepo.org> 1.0.20210424-1
- Update to 1.0.20210424

* Fri Feb 19 2021 Philip J Perry <phil@elrepo.org> 1.0.20210219-1
- Update to 1.0.20210219

* Sun Jan 24 2021 Philip J Perry <phil@elrepo.org> 1.0.20210124-1
- Update to 1.0.20210124

* Mon Dec 21 2020 Philip J Perry <phil@elrepo.org> 1.0.20201221-1
- Update to 1.0.20201221
- Fix updating of initramfs image [https://elrepo.org/bugs/view.php?id=1060]

* Fri Nov 13 2020 Philip J Perry <phil@elrepo.org> 1.0.20201112-1
- Update to 1.0.20201112

* Tue Nov 03 2020 Philip J Perry <phil@elrepo.org> 1.0.20200908-2
- Rebuilt for RHEL 8.3

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

* Tue Apr 28 2020 Philip J Perry <phil@elrepo.org> 1.0.20200426-2
- Rebuilt for RHEL 8.2

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

* Sat Feb 15 2020 Akemi Yagi <toracat@elrepo.org> 0.0.20200205-1
- Initial build for RHEL 8.1
