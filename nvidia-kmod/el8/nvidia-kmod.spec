# Define the kmod package name here.
%define kmod_name		nvidia

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 4.18.0-193.el8}

%{!?dist: %define dist .el8}

Name:		kmod-%{kmod_name}
Version:	440.100
Release:	1%{?dist}
Summary:	NVIDIA OpenGL kernel driver module
Group:		System Environment/Kernel
License:	Proprietary
URL:		http://www.nvidia.com/

# Sources
Source0:  ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
Source1:  blacklist-nouveau.conf
Source2:  dracut-nvidia.conf

NoSource: 0

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

# ensure version of gcc matches that used to build the kernel
%if "%{kmod_kernel_version}" == "4.18.0-80.el8"
BuildRequires:	gcc = 8.2.1
%endif
%if "%{kmod_kernel_version}" == "4.18.0-147.el8"
BuildRequires:	gcc = 8.3.1
%endif

Provides:	kernel-modules >= %{kmod_kernel_version}.%{_arch}
Provides:	kmod-%{kmod_name} = %{?epoch:%{epoch}:}%{version}-%{release}

Requires(post):	%{_sbindir}/weak-modules
Requires(postun):	%{_sbindir}/weak-modules
Requires:	kernel >= %{kmod_kernel_version}

%description
This package provides the proprietary NVIDIA OpenGL kernel driver module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "override %{kmod_name}-drm * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override %{kmod_name}-modeset * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override %{kmod_name}-uvm * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.config
sh %{SOURCE0} --extract-only --target nvidiapkg
%{__cp} -a nvidiapkg _kmod_build_

%build
# export IGNORE_CC_MISMATCH=1
export SYSSRC=%{_usrsrc}/kernels/%{kmod_kernel_version}.%{_arch}
pushd _kmod_build_/kernel
%{__make} %{?_smp_mflags} module
popd

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
pushd _kmod_build_/kernel
%{__install} %{kmod_name}.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} %{kmod_name}-drm.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} %{kmod_name}-modeset.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} %{kmod_name}-uvm.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
popd
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_prefix}/lib/modprobe.d/
%{__install} %{SOURCE1} %{buildroot}%{_prefix}/lib/modprobe.d/blacklist-nouveau.conf
%{__install} -d %{buildroot}%{_sysconfdir}/dracut.conf.d/
%{__install} %{SOURCE2} %{buildroot}%{_sysconfdir}/dracut.conf.d/dracut-nvidia.conf
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
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
#	kver_base="%{kmod_kernel_version}"
#	kvers=$(ls -d "/lib/modules/${kver_base%%.*}"*)
#
#	for k_dir in $kvers; do
#		k="${k_dir#/lib/modules/}"
#
#		tmp_initramfs="/boot/initramfs-$k.tmp"
#		dst_initramfs="/boot/initramfs-$k.img"
#
#		# The same check as in weak-modules: we assume that the kernel present
#		# if the symvers file exists.
#		if [ -e "/boot/symvers-$k.gz" ]; then
#			/usr/bin/dracut -f "$tmp_initramfs" "$k" || exit 1
#			cmp -s "$tmp_initramfs" "$dst_initramfs"
#			if [ "$?" = 1 ]; then
#				mv "$tmp_initramfs" "$dst_initramfs"
#			else
#				rm -f "$tmp_initramfs"
#			fi
#		fi
#	done

	rm -f "%{kver_state_file}"
	rmdir "%{kver_state_dir}" 2> /dev/null
fi

rmdir "%{dup_state_dir}" 2> /dev/null

# Update initramfs for all kABI compatible kernels
if [ -x /usr/bin/dracut ]; then
	# get installed kernels
	for KERNEL in $(rpm -q --qf '%{v}-%{r}.%{arch}\n' kernel); do
		VMLINUZ="/boot/vmlinuz-"$KERNEL
		# Check kABI compatibility
		for KABI in $(find /lib/modules -name nvidia.ko | cut -d / -f 4); do
			if [[ "$KERNEL" == "$KABI" && -e "$VMLINUZ" ]]; then
				/usr/bin/dracut --add-drivers nvidia -f /boot/initramfs-$KERNEL.img $KERNEL
			fi
		done
	done
fi

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
%config /etc/dracut.conf.d/dracut-nvidia.conf
%config /usr/lib/modprobe.d/blacklist-nouveau.conf
%doc /usr/share/doc/kmod-%{kmod_name}-%{version}/

%changelog
* Thu Jun 25 2020 Philip J Perry <phil@elrepo.org> - 440.100-1
- Updated to version 440.100

* Sat May 02 2020 Philip J Perry <phil@elrepo.org> - 440.82-2
- Rebuilt for RHEL 8.2

* Wed Apr 08 2020 Philip J Perry <phil@elrepo.org> - 440.82-1
- Updated to version 440.82
- Update initramfs for all kABI compatible kernels
  [https://elrepo.org/bugs/view.php?id=999]

* Tue Mar 31 2020 Philip J Perry <phil@elrepo.org> - 440.64-1
- Updated to version 440.64

* Sat Feb 08 2020 Philip J Perry <phil@elrepo.org> - 440.59-1
- Updated to version 440.59
- Add dracut conf file to omit nouveau and add nvidia modules

* Sat Jan 25 2020 Philip J Perry <phil@elrepo.org> 440.44-1
- Updated to version 440.44
- Rebuilt against RHEL 8.1 kernel

* Sun Dec 01 2019 Philip J Perry <phil@elrepo.org> 440.36-1
- Initial el8 build of the kmod package.
