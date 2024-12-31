# Define upstream file name here.
%define upstream_name		mbgtools-lx
# Define the kmod package name here.
%define kmod_name		mbgclock

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 4.18.0-553.el8_10}

%{!?dist: %define dist .el8}

Name:		kmod-%{kmod_name}
Version:	4.2.28
Release:	1%{?dist}
Summary:	%{kmod_name} kernel module(s)
Group:		System Environment/Kernel
License:	GPLv2
URL:		https://www.meinbergglobal.com/english/sw/#linux

# Sources
Source0:	https://www.meinbergglobal.com/download/drivers/%{upstream_name}-%{version}.tar.gz
Source5:	GPL-v2.0.txt

# Fix for the SB-signing issue caused by a bug in /usr/lib/rpm/brp-strip
# https://bugzilla.redhat.com/show_bug.cgi?id=1967291

%define __spec_install_post	/usr/lib/rpm/check-buildroot \
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
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%package -n %{kmod_name}-utils
Summary: Userspace utilities for %{kmod_name}
Group: System Environment/Kernel

%description -n %{kmod_name}-utils
Userspace utilities for %{kmod_name}

%prep
%setup -n %{upstream_name}-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

# Apply patch(es)
# % patch0 -p1

%build
# %%{__make} -C %%{kernel_source} %%{?_smp_mflags} V=1 modules M=$PWD
# KSRC=%%{_usrsrc}/kernels/%%{kversion}
# Note: This driver's Makefile is not stable with _smp_mflags 
%{__make} SUPP_SYN1588=1 -C $PWD BUILD_DIR="%{kernel_source}"

whitelist="/lib/modules/kabi-current/kabi_whitelist_%{_target_cpu}"
for modules in $( find . -name "*.ko" -type f -printf "%{findpat}\n" | sed 's|\.ko$||' | sort -u ) ; do
	# update greylist
	nm -u ./$modules.ko | sed 's/.*U //' |  sed 's/^\.//' | sort -u | while read -r symbol; do
		grep -q "^\s*$symbol\$" $whitelist || echo "$symbol" >> ./greylist
	done
done
sort -u greylist | uniq > greylist.txt

%install
# Install udev rules for kmod device
%{__install} -Dp -m0644 udev/55-mbgclock.rules %{buildroot}/etc/udev/rules.d/55-mbgclock.rules

%{__install} -d %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install}  mbgclock/*.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
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

# Quick and dirty loop for mbg utils
for binary in $(ls); do
    if [[ -x ${binary}/${binary} ]]; then
        %{__install} -Dp -m0755 ${binary}/${binary} %{buildroot}%{_sbindir}/${binary}
    fi
done

%clean
%{__rm} -rf %{buildroot}

%files -n %{kmod_name}-utils
%defattr(0644,root,root,-)
%attr(0755,root,root) %{_sbindir}/mbg*
/etc/udev/rules.d/55-mbgclock.rules

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
* Tue Dec 31 2024 Tuan Hoang <tqhoang@elrepo.org> - 4.2.28-1
- Updated to version 4.2.28
- Enable support for SYN1588 PCIe NICs

* Fri May 24 2024 Tuan Hoang <tqhoang@elrepo.org> - 4.2.26-2
- Rebuilt against RHEL 8.10 GA kernel 4.18.0-553.el8_10

* Fri Apr 12 2024 Tuan Hoang <tqhoang@elrepo.org> - 4.2.26-1
- Updated to version 4.2.26

* Sun Nov 19 2023 Tuan Hoang <tqhoang@elrepo.org> - 4.2.24-2
- Rebuilt against RHEL 8.9 GA kernel

* Thu Nov 16 2023 Tuan Hoang <tqhoang@elrepo.org> - 4.2.24-1
- Updated to version 4.2.24
- Restrict make to single thread for build stability
- Rebuilt against RHEL 8.8 GA kernel 4.18.0-477.10.1

* Thu May 12 2022 Akemi Yagi <toracat@elrepo.org> - 4.2.18-1
- Updated to version 4.2.18
- Rebuilt against RHEL 8.6 GA kernel 4.18.0-372.9.1.el8

* Fri Nov 12 2021 Akemi Yagi <toracat@elrepo.org> - 4.2.10-5
- Rebuilt against RHEL 8.5 kernel

* Tue May 18 2021 Philip J Perry <phil@elrepo.org> - 4.2.10-4
- Rebuilt against RHEL 8.4 kernel
- Fix updating of initramfs image
  [https://elrepo.org/bugs/view.php?id=1060]

* Sun Nov 08 2020 Akemi Yagi <toracat@elrepo.org> - 4.2.10-3
- Rebuilt against RHEL 8.3 kernel

* Thu Apr 30 2020 Akemi Yagi <toracat@elrepo.org> - 4.2.10-2
- Rebuilt against RHEL 8.2 kernel

* Sat Apr 25 2020 Akemi Yagi <toracat@elrepo.org> 4.2.10-1
- Initial build for RHEL 8. 1 [http://elrepo.org/bugs/view.php?id=1003]
