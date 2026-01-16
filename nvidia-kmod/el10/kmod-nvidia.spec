# Define the kmod package name here.
# Add the option '--with open' to build the kernel-open driver
%if %{?_with_open:1}%{!?_with_open:0}
%define kmod_open	-open
%endif

%define kmod_basename	nvidia
%define kmod_name	%{kmod_basename}%{?kmod_open}

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 6.12.0-124.8.1.el10_1}

%{!?dist: %define dist .el10}

Name:		kmod-%{kmod_name}
Version:	580.126.09
Release:	1%{?dist}
Summary:	%{kmod_name} kernel module(s)
Group:		System Environment/Kernel
License:	MIT and Redistributable, no modification permitted
URL:		http://www.nvidia.com/

# Sources
Source0:	https://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
Source1:	blacklist-nouveau.conf
Source2:	dracut-nvidia.conf
Source3:	modprobe-nvidia.conf

%if %{?_with_src:0}%{!?_with_src:1}
NoSource: 0
%endif

%define __spec_install_post \
		/usr/lib/rpm/check-buildroot \
		/usr/lib/rpm/redhat/brp-ldconfig \
		/usr/lib/rpm/brp-compress \
		/usr/lib/rpm/brp-strip-comment-note /usr/bin/strip /usr/bin/objdump \
		/usr/lib/rpm/brp-strip-static-archive /usr/bin/strip

%define findpat %( echo "%""P" )
%define __find_requires /usr/lib/rpm/redhat/find-requires.ksyms
%define __find_provides /usr/lib/rpm/redhat/find-provides.ksyms %{kmod_name} %{?epoch:%{epoch}:}%{version}-%{release}
%define dup_state_dir %{_localstatedir}/lib/rpm-state/kmod-dups
%define kver_state_dir %{dup_state_dir}/kver
%define kver_state_file %{kver_state_dir}/%{kmod_kernel_version}.%{_arch}
%define dup_module_list %{dup_state_dir}/rpm-kmod-%{kmod_name}-modules
%define debug_package %{nil}

%global kernel_source() %{_usrsrc}/kernels/%{kmod_kernel_version}.%{_arch}

BuildRoot:		%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

ExclusiveArch:		x86_64

BuildRequires:		elfutils-libelf-devel
BuildRequires:		kernel-abi-stablelists
BuildRequires:		kernel-devel = %{kmod_kernel_version}
BuildRequires:		kernel-rpm-macros
BuildRequires:		redhat-rpm-config
BuildRequires:		rpm-build
BuildRequires:		gcc
BuildRequires:		make

Provides:		kernel-modules >= %{kmod_kernel_version}.%{_arch}
Provides:		kmod-%{kmod_basename} = %{?epoch:%{epoch}:}%{version}-%{release}

%if %{?_with_open:1}%{!?_with_open:0}
Conflicts:		kmod-%{kmod_basename}
%else
Conflicts:		kmod-%{kmod_basename}-open
%endif

Requires:		kernel >= %{kmod_kernel_version}
Requires:		kernel-core-uname-r >= %{kmod_kernel_version}
Requires:		nvidia-x11-drv = %{?epoch:%{epoch}:}%{version}

Requires(post): 	%{_sbindir}/depmod
Requires(postun):	%{_sbindir}/depmod
Requires(post): 	%{_sbindir}/weak-modules
Requires(postun):	%{_sbindir}/weak-modules

%description
This package provides the NVIDIA OpenGL kernel%{?kmod_open} driver modules.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "override %{kmod_basename} * weak-updates/%{kmod_basename}" > kmod-%{kmod_name}.conf
echo "override %{kmod_basename}-drm * weak-updates/%{kmod_basename}" >> kmod-%{kmod_name}.conf
echo "override %{kmod_basename}-modeset * weak-updates/%{kmod_basename}" >> kmod-%{kmod_name}.conf
echo "override %{kmod_basename}-peermem * weak-updates/%{kmod_basename}" >> kmod-%{kmod_name}.conf
echo "override %{kmod_basename}-uvm * weak-updates/%{kmod_basename}" >> kmod-%{kmod_name}.conf
sh %{SOURCE0} --extract-only --target nvidiapkg
%{__cp} -a nvidiapkg _kmod_build_

%build
## %{__make} -C %{kernel_source} %{?_smp_mflags} V=1 modules M=$PWD
export SYSSRC=%{_usrsrc}/kernels/%{kmod_kernel_version}.%{_arch}
pushd _kmod_build_/kernel%{?kmod_open}
%{__make} %{?_smp_mflags} module
popd

whitelist="/lib/modules/kabi-current/kabi_stablelist_%{_target_cpu}"
for modules in $( find . -name "*.ko" -type f -printf "%{findpat}\n" | sed 's|\.ko$||' | sort -u ) ; do
	# update greylist
	nm -u ./$modules.ko | sed 's/.*U //' |  sed 's/^\.//' | sort -u | while read -r symbol; do
		grep -q "^\s*$symbol\$" $whitelist || echo "$symbol" >> ./greylist
	done
done
sort -u greylist | uniq > greylist.txt

%install
%{__install} -d %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_basename}/
pushd _kmod_build_/kernel%{?kmod_open}
%{__install} %{kmod_basename}.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_basename}/
%{__install} %{kmod_basename}-drm.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_basename}/
%{__install} %{kmod_basename}-modeset.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_basename}/
%{__install} %{kmod_basename}-peermem.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_basename}/
%{__install} %{kmod_basename}-uvm.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_basename}/
popd
pushd _kmod_build_
# Install GPU System Processor (GSP) firmware
%{__install} -d %{buildroot}/lib/firmware/nvidia/%{version}/
%{__install} -p -m 0755 firmware/gsp_ga10x.bin %{buildroot}/lib/firmware/nvidia/%{version}/gsp_ga10x.bin
%{__install} -p -m 0755 firmware/gsp_tu10x.bin %{buildroot}/lib/firmware/nvidia/%{version}/gsp_tu10x.bin
popd
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_prefix}/lib/modprobe.d/
%{__install} -m 0644 %{SOURCE1} %{buildroot}%{_prefix}/lib/modprobe.d/blacklist-nouveau.conf
%{__install} -d %{buildroot}%{_sysconfdir}/dracut.conf.d/
%{__install} -m 0644 %{SOURCE2} %{buildroot}%{_sysconfdir}/dracut.conf.d/dracut-nvidia.conf
%{__install} -d %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} -m 0644 %{SOURCE3} %{buildroot}%{_sysconfdir}/modprobe.d/modprobe-nvidia.conf
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
modules=( $(find /lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_basename} | grep '\.ko$') )
printf '%s\n' "${modules[@]}" | %{_sbindir}/weak-modules --add-modules --no-initramfs

mkdir -p "%{kver_state_dir}"
touch "%{kver_state_file}"

exit 0

%posttrans
# We have to re-implement part of weak-modules here because it doesn't allow
# calling initramfs regeneration separately
if [ -f "%{kver_state_file}" ]; then
#        kver_base="%{kmod_kernel_version}"
#        kvers=$(ls -d "/lib/modules/${kver_base%%%%-*}"*)
#
#        for k_dir in $kvers; do
#                k="${k_dir#/lib/modules/}"
#
#                tmp_initramfs="/boot/initramfs-$k.tmp"
#                dst_initramfs="/boot/initramfs-$k.img"
#
#                # The same check as in weak-modules: we assume that the kernel present
#                # if the symvers file exists.
#                if [ -e "/$k_dir/symvers.gz" ]; then
#                        /usr/bin/dracut -f "$tmp_initramfs" "$k" || exit 1
#                        cmp -s "$tmp_initramfs" "$dst_initramfs"
#                        if [ "$?" = 1 ]; then
#                                mv "$tmp_initramfs" "$dst_initramfs"
#                        else
#                                rm -f "$tmp_initramfs"
#                        fi
#                fi
#        done

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
%license nvidiapkg/LICENSE
/lib/modules/%{kmod_kernel_version}.%{_arch}/
%config %{_sysconfdir}/depmod.d/kmod-%{kmod_name}.conf
%config %{_sysconfdir}/dracut.conf.d/dracut-nvidia.conf
%config(noreplace) %{_sysconfdir}/modprobe.d/modprobe-nvidia.conf
%config %{_prefix}/lib/modprobe.d/blacklist-nouveau.conf
%doc %{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%dir /lib/firmware/nvidia/%{version}/
/lib/firmware/nvidia/%{version}/*.bin

%changelog
* Thu Jan 15 2026 Tuan Hoang <tqhoang@elrepo.org> - 580.126.09-1
- Updated to version 580.126.09
- Built against RHEL 10.1 GA kernel

* Fri Dec 12 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.119.02-1
- Updated to version 580.119.02
- Built against RHEL 10.1 GA kernel

* Tue Nov 11 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.105.08-1
- Updated to version 580.105.08
- Built against RHEL 10.1 GA kernel

* Sat Oct 04 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.95.05-1.1
- Rebuilt against RHEL 10.0 errata kernel 6.12.0-55.32.1.el10_0

* Sat Oct 04 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.95.05-1
- Updated to version 580.95.05
- Built against RHEL 10.0 GA kernel

* Thu Sep 11 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.82.09-1.1
- Rebuilt against RHEL 10.0 errata kernel 6.12.0-55.32.1.el10_0

* Thu Sep 11 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.82.09-1
- Updated to version 580.82.09
- Built against RHEL 10.0 GA kernel

* Mon Sep 08 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.82.07-1.1
- Rebuilt against RHEL 10.0 errata kernel 6.12.0-55.31.1.el10_0

* Wed Sep 03 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.82.07-1
- Updated to version 580.82.07

* Tue Aug 12 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.76.05-1
- Updated to version 580.76.05
- Built against RHEL 10.0 GA kernel
- Fork for RHEL10
