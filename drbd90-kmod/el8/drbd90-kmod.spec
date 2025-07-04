# Define the kmod package name here.
%define kmod_name		drbd90
%define real_name 		drbd

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 4.18.0-553.46.1.el8_10}

%{!?dist: %define dist .el8}

Name:		kmod-%{kmod_name}
Version:	9.2.14
Release:	1%{?dist}
Summary:	%{kmod_name} kernel module(s)
Group:		System Environment/Kernel
License:	GPLv2
URL:		http://www.drbd.org/

# Sources
Source0:	drbd-%{version}.tar.gz
Source5:	GPL-v2.0.txt

# Fix for the SB-signing issue caused by a bug in /usr/lib/rpm/brp-strip
# https://bugzilla.redhat.com/show_bug.cgi?id=1967291

%define __spec_install_post /usr/lib/rpm/check-buildroot \
                             /usr/lib/rpm/redhat/brp-ldconfig \
                             /usr/lib/rpm/brp-compress \
                             /usr/lib/rpm/brp-strip-comment-note /usr/bin/strip /usr/bin/objdump \
                             /usr/lib/rpm/brp-strip-static-archive /usr/bin/strip \
                             /usr/lib/rpm/brp-python-bytecompile "" 1 \
                             /usr/lib/rpm/brp-python-hardlink \
                             PYTHON3="/usr/libexec/platform-python" /usr/lib/rpm/redhat/brp-mangle-shebangs

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
BuildRequires:	kernel-abi-whitelists
BuildRequires:	kernel-rpm-macros
BuildRequires:	redhat-rpm-config

Provides:	kernel-modules >= %{kmod_kernel_version}.%{_arch}
Provides:	kmod-%{kmod_name} = %{?epoch:%{epoch}:}%{version}-%{release}

Requires(post):	%{_sbindir}/weak-modules
Requires(postun):	%{_sbindir}/weak-modules
Requires:	kernel >= %{kmod_kernel_version}

%description
DRBD is a distributed replicated block device. It mirrors a
block device over the network to another machine. Think of it
as networked raid 1. It is a building block for setting up
high availability (HA) clusters.
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -n %{real_name}-%{version}
# %patch0 -p1
echo "override drbd * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "override drbd_transport_tcp * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf

%build
%{__make} %{?_smp_mflags} module KDIR=%{kernel_source}  KVER=%{kversion}

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
%{__install} drbd/build-current/*.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -m 0644 %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -m 0644 greylist.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# strip the modules(s)
find %{buildroot} -type f -name \*.ko -exec %{__strip} --strip-debug \{\} \;

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
* Sat Jun 07 2025 Akemi Yagi <toracat@elrepo.org> - 9.2.14-1.el8_10
- Version updated to 9.2.14
- Rebuilt against kernel-4.18.0-553.46.1.el8_10

* Thu Mar 27 2025 Akemi Yagi <toracat@elrepo.org> - 9.2.13-1.el8_10
- Version updated to 9.2.13
- Rebuilt against kernel-4.18.0-553.46.1.el8_10

* Tue Nov 19 2024 Akemi Yagi <toracat@elrepo.org> - 9.2.12-2.el8_10
- Rebuilt against kernel-4.18.0-553.27.1.el8_10

* Mon Nov 18 2024 Akemi Yagi <toracat@elrepo.org> - 9.2.12.el8_10
- Version updated to 9.2.12

* Mon Aug 12 2024 Akemi Yagi <toracat@elrepo.org> - 9.2.11.el8_10
- Version updated to 9.2.11

* Sat Jun 08 2024 Akemi Yagi <toracat@elrepo.org> - 9.1.21-1.el8_10
- Version updated to 9.1.21

* Wed May 22 2024 Akemi Yagi <toracat@elrepo.org> - 9.1.20-1.el8_10
- Rebuilt against el8.10 GA kernel-4.18.0-553.el8_10

* Sun May 12 2024 Akemi Yagi <toracat@elrepo.org> - 9.1.20-1.el8_9
- Version updated to 9.1.20

* Tue Mar 05 2024 Akemi Yagi <toracat@elrepo.org> - 9.1.19-1.el8_9
- Version updated to 9.1.19

* Fri Dec 22 2023 Akemi Yagi <toracat@elrepo.org> - 9.1.18-1.el8_9
- Version updated to 9.1.18

* Tue Nov 14 2023 Akemi Yagi <toracat@elrepo.org> -9.1.17-2.el8_9
- Rebuilt against el8.9 GA kernel-4.18.0-513.5.1.el8_9

* Tue Oct 31 2023 Akemi Yagi <toracat@elrepo.org> - 9.1.17-1.el8_8
- Version updated to 9.1.17

* Sat Aug 12 2023 Akemi Yagi <toracat@elrepo.org> - 9.1.16-1.el8_8
- Version updated to 9.1.16

* Tue Jun 06 2023 Akemi Yagi <toracat@elrepo.org> - 9.1.15.1.el8_8
- Version updated to 9.1.15.1

* Tue May 16 2023 Akemi Yagi <toracat@elrepo.org> - 9.1.14-4.el8_8
- Rebuilt against el8.8 GA kernel-4.18.0-477.10.1.el8_8

* Tue Apr 18 2023 Akemi Yagi <toracat@elrepo.org> - 9.1.14-3.el8_7
- Add missing drbd_transport_tcp to kmod-drbd90.conf

* Fri Apr 14 2023 Akemi Yagi <toracat@elrepo.org> - 9.1.14-2.el8_7
- Rebuilt against kernel-425.19.2.el8_7

* Sat Apr 08 2023 Akemi Yagi <toracat@elrepo.org> - 9.1.14-1.el8_7
- Updated to 9.1.14

* Mon Jan 30 2023 Akemi Yagi <toracat@elrepo.org> - 9.1.13-1.el8_7
- Updated to 9.1.13

* Sun Jan 15 2023 Akemi Yagi <toracat@elrepo.org> - 9.1.12-2.el8_7
- Rebuilt against kernel-4.18.0-425.10.1.el8_7 due to a bug in the RHEL kernel
  [https://access.redhat.com/solutions/6985596]

* Tue Nov 22 2022 Akemi Yagi <toracat@elrepo.org> - 9.1.12-1.el8_7
- Updated to 9.1.12

* Tue Nov 08 2022 Akemi Yagi <toracat@elrepo.org> - 9.1.11-2.el8_7
- Rebuilt against RHEL 8.7 GA kernel 4.18.0-425.3.1.el8

* Tue Oct 11 2022 Akemi Yagi <toracat@elrepo.org> - 9.1.11-1.el8_6
- Updated to 9.1.11

* Sat Aug 20 2022 Akemi Yagi <toracat@elrepo.org> - 9.1.8-2.el8_6
- Patch applied from https://github.com/LINBIT/drbd/issues/45
  (commit d7d76aa)

* Wed Jul 27 2022 Akemi Yagi <toracat@elrepo.org> - 9.1.8-1.el8_6
- Updated to 9.1.8

* Sat May 21 2022 Akemi Yagi <toracat@elrepo.org> - 9.1.7-1.el8_6
- Updated to 9.1.7
- Rebuilt against RHEL 8.6 GA kernel 4.18.0-372.9.1.el8

* Sat Dec 18 2021 Akemi Yagi <toracat@elrepo.org> - 9.1.5-1.el8_5
- Updated to 9.1.5

* Tue Nov 09 2021 Akemi Yagi <toracat@elrepo.org> - 9.1.4-2.el8_5
- Rebuilt against RHEL 8.5 GA kernel 4.18.0-348.el8

* Wed Oct 06 2021 Akemi Yagi <toracat@elrepo.org> - 9.1.4-1.el8_4
- Updated to 9.1.4

* Mon Sep 27 2021 Akemi Yagi <toracat@elrepo.org> - 9.0.30-1.el8_4
- Updated to 9.0.30

* Thu Jun 03 2021 Akemi Yagi <toracat@elrepo.org> - 9.0.29-1.el8_4
- Updated to 9.0.29
- Fix SB-signing issue caused by /usr/lib/rpm/brp-strip
  [https://bugzilla.redhat.com/show_bug.cgi?id=1967291]

* Wed May 26 2021 Akemi Yagi <toracat@elrepo.org> - 9.0.25-3.el8_4
- Rebuilt against RHEL 8.4 kernel
- Fix updating of initramfs image
  [https://elrepo.org/bugs/view.php?id=1060]

* Sat Nov 07 2020 Akemi Yagi <toracat@elrepo.org> - 9.0.25-2.el8_3
- Updated to 9.0.25
- Rebuilt against RHEL 8.3 kernel

* Tue Jun 23 2020 Akemi Yagi <toracat@elrepo.org> - 9.0.23-1.el8_2
- Updated to 9.0.23

* Tue Jun 23 2020 Akemi Yagi <toracat@elrepo.org> - 9.0.21-4.el8_2
- Rebuilt against kernel-4.18.0-193.6.3.el8_2

* Fri May 01 2020 Akemi Yagi <toracat@elrepo.org> - 9.0.21-3.el8_2
- Rebuilt against RHEL 8.2 kernel

* Sun Nov 24 2019 Akemi Yagi <toracat@elrepo.org> - 9.0.21-2.el8_1
- Updated to 9.0.21

* Fri Nov 22 2019 Akemi Yagi <toracat@elrepo.org> - 9.0.20-3.el8_1
- Rebuilt against RHEL 8.1 kernel

* Wed Nov 06 2019 Akemi Yagi <toracat@elrepo.org> - 9.0.20-2.el8_0
- Initial el8 build of the kmod package.
  [https://elrepo.org/bugs/view.php?id=965]
