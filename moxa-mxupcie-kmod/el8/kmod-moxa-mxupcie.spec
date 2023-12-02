# Define the kmod package name here.
%define kmod_name		moxa-mxupcie

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 4.18.0-513.5.1.el8_9}

%{!?dist: %define dist .el8}

Summary:	Moxa %{kmod_name} kernel module(s)
Name:		kmod-%{kmod_name}
Version:	4.1
Release:	4%{?dist}
License:	GPLv2+
URL:		https://www.moxa.com/

# Sources
Source0:	moxa-linux-kernel-4.x.x-driver-v4.1.tgz
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
Patch0:	moxa-linux-kernel-4.x.x-driver-v4.1-access_ok.diff
Patch1: mxpcie.c.diff

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
MOXA Smartio/Industio Family Multiport Board Device Driver.

This package provides the mxupcie kernel module.

It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%package -n	%{kmod_name}-tools
Summary:	Tools for %{kmod_name} kernel modules
Obsoletes:	kmod-%{kmod_name}-tools
Provides:	kmod-%{kmod_name}-tools

%description -n	%{kmod_name}-tools
This package provides tools for %{kmod_name} kernel modules.

%prep
%setup -q -n mxser
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

# Apply patch(es)
%patch0 -p1
%patch1 -p1

%build
%{__make} -C $PWD/utility %{?_smp_mflags}

%{__make} -C %{kernel_source} %{?_smp_mflags} modules M=$PWD/driver/kernel4.x

whitelist="/lib/modules/kabi-current/kabi_whitelist_%{_target_cpu}"
for modules in $( find . -name "*.ko" -type f -printf "%{findpat}\n" | sed 's|\.ko$||' | sort -u ) ; do
	# update greylist
	nm -u ./$modules.ko | sed 's/.*U //' |  sed 's/^\.//' | sort -u | while read -r symbol; do
		grep -q "^\s*$symbol\$" $whitelist || echo "$symbol" >> ./greylist
	done
done
sort -u greylist | uniq > greylist.txt

%install

%{__install} -d %{buildroot}%{_bindir}
%{__install} -m0755 utility/conf/muestty %{buildroot}%{_bindir}/muestty
%{__install} -m0755 utility/diag/msdiag %{buildroot}%{_bindir}/msdiag
%{__install} -m0755 utility/mon/msmon %{buildroot}%{_bindir}/msmon
%{__install} -m0755 utility/term/msterm %{buildroot}%{_bindir}/msterm

%{__install} -d %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
# %{__install} -m0755 driver/kernel4.x/mxser.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} -m0755 driver/kernel4.x/mxupcie.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/

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

%files -n %{kmod_name}-tools
%{_bindir}/muestty
%{_bindir}/msdiag
%{_bindir}/msmon
%{_bindir}/msterm

%files
%doc COPYING* VERSION.txt readme.txt
%doc /usr/share/doc/kmod-%{kmod_name}-%{version}/
%config /etc/depmod.d/kmod-%{kmod_name}.conf
/lib/modules/%{kmod_kernel_version}.%{_arch}/

%changelog
* Sun Nov 19 2023 Tuan Hoang <tqhoang@elrepo.org> - 4.1-4
- Fix naming of tools sub-package
- Rebuilt against RHEL 8.9 GA kernel
- Source code from kernel-4.18.0-513.5.1.el8_9

* Tue May 31 2022 Akemi Yagi <toracat@elrepo.org> - 4.1-3
- Rebuilt against RHEL 8.6 GA kernel 4.18.0-372.9.1.el8
- Patch mxpcie.c.diff applied
  [https://elrepo.org/bugs/view.php?id=1229]

* Thu Feb 10 2022 Akemi Yagi <toracat@elrepo.org> - 4.1-2
- updated to match the current ELRepo spec template
- Rebuilt against RHEL 8.5 GA kernel

* Thu Feb 10 2022 Oden Eriksson <oe@nux.se> - 4.1-1
- fix permissions
- rename the package and use only mxupcie
- don't prefix the binaries as it's confusing

* Wed Feb 09 2022 Oden Eriksson <oe@nux.se> - 4.1-2
- different build fix this time...

* Wed Feb 09 2022 Oden Eriksson <oe@nux.se> - 4.1-1
- 4.1
- deactivate P0

* Fri Sep 17 2021 Oden Eriksson <oe@nux.se> - 4.0-1
- initial package
