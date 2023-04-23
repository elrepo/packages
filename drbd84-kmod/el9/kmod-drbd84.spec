# Define the kmod package name here.
%define kmod_name drbd84
%define real_name drbd

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 5.14.0-162.23.1.el9_1}

%{!?dist: %define dist .el9}

Name:		kmod-%{kmod_name}
Version:	8.4.11
%define 	original_release 1
Release:	%{original_release}.22%{?dist}
Summary:	%{kmod_name} kernel module(s)
Group:		System Environment/Kernel
License:	GPLv2
URL:		http://www.kernel.org/

# Sources
Source0:	%{real_name}-%{version}-%{original_release}.tar.gz
Source5:	GPL-v2.0.txt

# Fix for the SB-signing issue caused by a bug in /usr/lib/rpm/brp-strip
# https://bugzilla.redhat.com/show_bug.cgi?id=1967291

%define __spec_install_post \
		/usr/lib/rpm/check-buildroot \
		/usr/lib/rpm/redhat/brp-ldconfig \
		/usr/lib/rpm/brp-compress \
		/usr/lib/rpm/brp-strip-comment-note /usr/bin/strip /usr/bin/objdump \
		/usr/lib/rpm/brp-strip-static-archive /usr/bin/strip \
		/usr/lib/rpm/redhat/brp-python-bytecompile "" "1" "0" \
		/usr/lib/rpm/brp-python-hardlink \
		/usr/lib/rpm/redhat/brp-mangle-shebangs

# Source code patches

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
BuildRequires:	kernel-abi-stablelists
BuildRequires:	kernel-rpm-macros
BuildRequires:	redhat-rpm-config
BuildRequires:  rpm-build
BuildRequires:  gcc
BuildRequires:  make

Provides:	kernel-modules >= %{kmod_kernel_version}.%{_arch}
Provides:	kmod-%{kmod_name} = %{?epoch:%{epoch}:}%{version}-%{release}

Requires:	kernel >= %{kmod_kernel_version}
Requires:	kernel-core-uname-r >= %{kmod_kernel_version}

Requires(post):		%{_sbindir}/depmod
Requires(postun):	%{_sbindir}/depmod
Requires(post):		%{_sbindir}/weak-modules
Requires(postun):	%{_sbindir}/weak-modules

%description
DRBD is a distributed replicated block device. It mirrors a
block device over the network to another machine. Think of it
as networked raid 1. It is a building block for setting up
high availability (HA) clusters.
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -n %{real_name}-%{version}-%{original_release}

# %patch0 -p1
echo "override %{real_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
%{__make} -C %{kernel_source}  %{?_smp_mflags} modules M=$PWD  KVER=%{kversion} CONFIG_BLK_DEV_DRBD=m

whitelist="/lib/modules/kabi-current/kabi_stablelist_%{_target_cpu}"
for modules in $( find . -name "*.ko" -type f -printf "%{findpat}\n" | sed 's|\.ko$||' | sort -u ) ; do
	# update greylist
	nm -u ./$modules.ko | sed 's/.*U //' |  sed 's/^\.//' | sort -u | while read -r symbol; do
		grep -q "^\s*$symbol\$" $whitelist || echo "$symbol" >> ./greylist
	done
done
sort -u greylist | uniq > greylist.txt

%install
%{__install} -d %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} drbd.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
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
		if [ -e "/$k_dir/symvers.gz" ]; then
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
* Sun Apr 23 2023 Akemi Yagi <toracat@elrepo.org> - 8.4.11-1.22.el9
- Correct kmod-drbd90.conf

* Sun Apr 23 2023 Akemi Yagi <toracat@elrepo.org> - 8.4.11-1.21.el9
- Built against RHEL kernel-5.14.0-162.23.1.el9_1

* Sat Nov 19 2022 Akemi Yagi <toracat@elrepo.org> - 8.4.11-1.20.el9
- Source code from RHEL 9.1 GA kernel 5.14.0-162.6.1.el9_1
- Built against RHEL kernel-5.14.0-162.6.1.el9_1

* Tue May 17 2022 Akemi Yagi <toracat@elrepo.org> - 8.4.11-1.18.el9
- Rebuilt against RHEL 9.0 GA kernel 5.14.0-70.13.1.el9_0
- Source code from kernel-5.14.0-70.13.1.el9_0

* Sat Dec 18 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.17.el8
- Source code from kernel-4.18.0-348.el8

* Wed Dec 15 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.16.el8
- Added missing lru_cache.c to the source code
- kmod-lru_cache no longer required as dependency
- Makefile corrected
- Built against RHEL 8.5 GA kernel 4.18.0-348.el8

* Thu Nov 18 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.15.el8
- Rebuilt against kernel-4.18.0-348.2.1.el8_5

* Tue Nov 09 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.14.el8
- Rebuilt against RHEL 8.5 GA kernel 4.18.0-348.el8

* Wed Nov 03 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.13.el8
- Rebuilt against kernel-4.18.0-305.251.el8_4

* Thu Sep 16 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.12.el8
-Rebuilt against kernel-4.18.0-305.19.1.el8_4

* Tue Sep 07 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.11.el8
-Rebuilt against kernel-4.18.0-305.17.1.el8_4

* Tue Aug 10 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.10.el8
- Rebuilt against kernel-4.18.0-305.12.1.el8_4

* Wed Jul 21 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.9.el8
- Rebuilt against kernel-4.18.0-305.10.2.el8_4

* Thu Jul 01 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.8.el8
- Rebuilt against kernel-4.18.0-305.7.1.el8_4

* Mon Jun 07 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.7.el8
- Rebuilt against kernel-4.18.0-305.3.1.el8_4
- Fix SB-signing issue caused by /usr/lib/rpm/brp-strip
  [https://bugzilla.redhat.com/show_bug.cgi?id=1967291]
- kmod-lru_cache added as a Requires

* Wed May 26 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.6.el8
- Rebuilt against RHEL 8.4 (kernel-4.18.0-305.el8)
- Source code from kernel-4.18.0-305.el8
- Fix updating of initramfs image
  [https://elrepo.org/bugs/view.php?id=1060]

* Mon Feb 22 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.5.el8
- Added lib/lru_cache.c to the drbd source code
  [https://elrepo.org/bugs/view.php?id=1075#c7455]

* Thu Feb 18 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.3.el8
- Rebuilt against kernel-4.18.0-240.10.1.el8_3

* Tue Feb 16 2021 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1.el8
- Initial build for RHEL 8
- Source code from RHEL kernel-4.18.0-240.el8
  [https://elrepo.org/bugs/view.php?id=1075]
