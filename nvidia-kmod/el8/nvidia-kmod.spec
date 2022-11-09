# Define the kmod package name here.
%define kmod_name		nvidia

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 4.18.0-425.3.1.el8}

%{!?dist: %define dist .el8}

Name:		kmod-%{kmod_name}
Version:	515.76
Release:	2%{?dist}
Summary:	NVIDIA OpenGL kernel driver module
Group:		System Environment/Kernel
License:	Proprietary
URL:		http://www.nvidia.com/

# Sources
Source0:  ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
Source1:  blacklist-nouveau.conf
Source2:  dracut-nvidia.conf

NoSource: 0

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

# ensure version of gcc matches that used to build the kernel
%if "%{kmod_kernel_version}" == "4.18.0-80.el8"
BuildRequires:	gcc = 8.2.1
%endif
%if "%{kmod_kernel_version}" == "4.18.0-147.el8"
BuildRequires:	gcc = 8.3.1
%endif
%if "%{kmod_kernel_version}" == "4.18.0-305.el8"
BuildRequires:	gcc = 8.4.1
%endif
%if "%{kmod_kernel_version}" >= "4.18.0-348.el8"
BuildRequires:	gcc = 8.5.0
%endif

Provides:	kernel-modules >= %{kmod_kernel_version}.%{_arch}
Provides:	kmod-%{kmod_name} = %{?epoch:%{epoch}:}%{version}-%{release}

Requires:	nvidia-x11-drv = %{?epoch:%{epoch}:}%{version}
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
echo "override %{kmod_name}-peermem * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.config
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
%{__install} %{kmod_name}-peermem.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} %{kmod_name}-uvm.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
popd
pushd _kmod_build_
# Install GPU System Processor (GSP) firmware
%{__install} -d %{buildroot}/lib/firmware/nvidia/%{version}/
%{__install} -p -m 0755 firmware/gsp.bin %{buildroot}/lib/firmware/nvidia/%{version}/gsp.bin
popd
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_prefix}/lib/modprobe.d/
%{__install} -m 0644 %{SOURCE1} %{buildroot}%{_prefix}/lib/modprobe.d/blacklist-nouveau.conf
%{__install} -d %{buildroot}%{_sysconfdir}/dracut.conf.d/
%{__install} -m 0644 %{SOURCE2} %{buildroot}%{_sysconfdir}/dracut.conf.d/dracut-nvidia.conf
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
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
#		if [ -e "$k_dir/symvers.gz" ]; then
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
%dir /lib/firmware/nvidia/%{version}/
/lib/firmware/nvidia/%{version}/gsp.bin

%changelog
* Tue Nov 08 2022 Philip J Perry <phil@elrepo.org> - 515.76-2
- Rebuilt for RHEL 8.7

* Sat Sep 24 2022 Philip J Perry <phil@elrepo.org> - 515.76-1
- Updated to version 515.76

* Sun Aug 07 2022 Philip J Perry <phil@elrepo.org> - 515.65.01-1
- Updated to version 515.65.01

* Wed Jun 29 2022 Philip J Perry <phil@elrepo.org> - 515.57-1
- Updated to version 515.57

* Mon Jun 27 2022 Philip J Perry <phil@elrepo.org> - 515.48.07-1
- Updated to version 515.48.07

* Mon May 23 2022 Philip J Perry <phil@elrepo.org> - 470.129.06-1
- Updated to version 470.129.06

* Tue May 10 2022 Philip J Perry <phil@elrepo.org> - 470.103.01-2
- Rebuilt for RHEL 8.6

* Tue Feb 01 2022 Philip J Perry <phil@elrepo.org> - 470.103.01-1
- Updated to version 470.103.01

* Tue Dec 14 2021 Philip J Perry <phil@elrepo.org> - 470.94-1
- Updated to version 470.94

* Thu Nov 11 2021 Philip J Perry <phil@elrepo.org> - 470.86-1
- Updated to version 470.86
- Fix modes on source files

* Wed Nov 10 2021 Philip J Perry <phil@elrepo.org> - 470.82.00-2
- Rebuilt for RHEL8.5
- Fix SB-signing issue caused by /usr/lib/rpm/brp-strip
  [https://bugzilla.redhat.com/show_bug.cgi?id=1967291]
- Update stripping of modules

* Thu Oct 28 2021 Philip J Perry <phil@elrepo.org> - 470.82.00-1
- Updated to version 470.82.00

* Tue Sep 21 2021 Philip J Perry <phil@elrepo.org> - 470.74-1
- Updated to version 470.74
- Removed conflict with centos-stream-release
  [https://elrepo.org/bugs/view.php?id=1139]

* Wed Aug 11 2021 Philip J Perry <phil@elrepo.org> - 470.63.01-1
- Updated to version 470.63.01
- Add firmware for nvidia.ko module

* Mon Jul 19 2021 Philip J Perry <phil@elrepo.org> - 470.57.02-1
- Updated to version 470.57.02
- Adds nvidia-peermem kernel module

* Fri Jun 04 2021 Philip J Perry <phil@elrepo.org> - 460.84-1
- Updated to version 460.84

* Tue May 18 2021 Philip J Perry <phil@elrepo.org> - 460.80-2
- Rebuilt for RHEL 8.4

* Wed May 12 2021 Philip J Perry <phil@elrepo.org> - 460.80-1
- Updated to version 460.80

* Wed Apr 14 2021 Philip J Perry <phil@elrepo.org> - 460.73.01-1
- Updated to version 460.73.01

* Fri Mar 19 2021 Philip J Perry <phil@elrepo.org> - 460.67-1
- Updated to version 460.67

* Fri Feb 26 2021 Philip J Perry <phil@elrepo.org> - 460.56-1
- Updated to version 460.56

* Mon Feb 01 2021 Philip J Perry <phil@elrepo.org> - 460.39-1
- Updated to version 460.39

* Sat Dec 19 2020 Philip J Perry <phil@elrepo.org> - 455.45.01-1
- Updated to version 455.45.01
  [https://elrepo.org/bugs/view.php?id=1061]

* Wed Nov 04 2020 Philip J Perry <phil@elrepo.org> - 450.80.02-3
- Rebuilt for RHEL 8.3

* Wed Oct 21 2020 Philip J Perry <phil@elrepo.org> - 450.80.02-2
- Rebuilt for kernel-4.18.0-193.28.1.el8_2.x86_64
  [4.18.0-193.28.1.el8_2.x86_64]

* Fri Oct 02 2020 Philip J Perry <phil@elrepo.org> - 450.80.02-1
- Updated to version 450.80.02

* Wed Aug 19 2020 Philip J Perry <phil@elrepo.org> - 450.66-1
- Updated to version 450.66
- Add missing requires for nvidia-x11-drv package

* Fri Jul 10 2020 Philip J Perry <phil@elrepo.org> - 450.57-1
- Updated to version 450.57

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
