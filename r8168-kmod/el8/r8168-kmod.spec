# Define the kmod package name here.
%define kmod_name		r8168

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 4.18.0-553.el8_10}

%{!?dist: %define dist .el8}

Name:		kmod-%{kmod_name}
Version:	8.055.00
Release:	1%{?dist}
Summary:	%{kmod_name} kernel module(s)
Group:		System Environment/Kernel
License:	GPLv2
URL:		http://www.realtek.com/en

# Sources
Source0:	%{kmod_name}-%{version}.tar.bz2
Source5:	GPL-v2.0.txt
Source10:	blacklist-r8169.conf
Source20:	ELRepo-Makefile-%{kmod_name}


# Patches
Patch0: ELRepo-r8168.patch

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
%setup -q -n %{kmod_name}-%{version}
%patch0 -p1
%{__rm} -f src/Makefile*
%{__cp} -a %{SOURCE20} src/Makefile
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
%{__make} -C %{kernel_source} %{?_smp_mflags} modules M=$PWD/src

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
%{__install} -d %{buildroot}%{_prefix}/lib/modprobe.d/
%{__install} -m 0644 %{SOURCE10} %{buildroot}%{_prefix}/lib/modprobe.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -m 0644 %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -m 0644 greylist.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

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
%config /usr/lib/modprobe.d/blacklist-r8169.conf
%doc /usr/share/doc/kmod-%{kmod_name}-%{version}/

%changelog
* Sat Mar 01 2025 Tuan Hoang <tqhoang@elrepo.org> - 8.055.00-1
- Update to version 8.054.00
- Add blacklist file to source

* Mon Oct 28 2024 Tuan Hoang <tqhoang@elrepo.org> - 8.054.00-1
- Update to version 8.054.00
- Revised patch to not overwrite the RTL_NAPI_CONFIG() macro

* Fri May 24 2024 Tuan Hoang <tqhoang@elrepo.org> - 8.053.00-1
- Rebuilt against RHEL 8.10 GA kernel 4.18.0-553.el8_10
- Update to version 8.053.00

* Tue Nov 14 2023 Philip J Perry <phil@elrepo.org> 8.052.01-2
- Rebuilt for RHEL 8.9

* Wed Oct 25 2023 Philip J Perry <phil@elrepo.org> 8.052.01-1
- Update to version 8.052.01
- Rebuilt for RHEL 8.8

* Fri Dec 16 2022 Akemi Yagi <toracat@elrepo.org> 8.051.02-1
- Update to version 8.051.02
- Rebuilt for RHEL 8.7
- Updated the patch (ELRepo-r8168-8.051.02.patch)

* Wed Sep 21 2022 Akemi Yagi <toracat@elrepo.org> 8.050.03-1
- Update to version 8.050.03

* Tue May 10 2022 Philip J Perry <phil@elrepo.org> 8.049.02-4
- Rebuilt for RHEL 8.6

* Mon Mar 07 2022 Philip J Perry <phil@elrepo.org> 8.049.02-3
- Enabled -DCONFIG_R8168_VLAN, -DCONFIG_ASPM, -DENABLE_S5WOL, -DENABLE_EEE,
          -DCONFIG_DYNAMIC_ASPM -DENABLE_USE_FIRMWARE_FILE 

* Sun Mar 06 2022 Philip J Perry <phil@elrepo.org> 8.049.02-2
- Fix SB-signing issue caused by /usr/lib/rpm/brp-strip
  [https://bugzilla.redhat.com/show_bug.cgi?id=1967291]

* Sun Mar 06 2022 Philip J Perry <phil@elrepo.org> 8.049.02-1
- Update to version 8.049.02

* Tue May 18 2021 Philip J Perry <phil@elrepo.org> 8.048.03-2
- Rebuilt for RHEL8.4

* Sun Mar 21 2021 Philip J Perry <phil@elrepo.org> 8.048.03-1
- Initial el8 build of the kmod package.
  [https://elrepo.org/bugs/view.php?id=1083]
