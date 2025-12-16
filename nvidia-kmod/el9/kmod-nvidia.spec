# Define the kmod package name here.
# Add the option '--with open' to build the kernel-open driver
%if %{?_with_open:1}%{!?_with_open:0}
%define kmod_open	-open
%endif

%define kmod_basename	nvidia
%define kmod_name	%{kmod_basename}%{?kmod_open}

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 5.14.0-611.8.1.el9_7}

%{!?dist: %define dist .el9}

Name:		kmod-%{kmod_name}
Version:	580.119.02
Release:	1.1%{?dist}
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
		/usr/lib/rpm/brp-strip-static-archive /usr/bin/strip \
		/usr/lib/rpm/redhat/brp-python-bytecompile "" "1" "0" \
		/usr/lib/rpm/brp-python-hardlink \
		/usr/lib/rpm/redhat/brp-mangle-shebangs
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
# One user requires X11 with IndirectGLX (IGLX)
# With modeset=1, the X11 session crashes and goes back to GDM
# Need to check if IndirectGLX is configured and set modeset accordingly
HAS_INDIRECT_GLX=`grep IndirectGLX %{_sysconfdir}/X11/xorg.conf %{_sysconfdir}/X11/xorg.conf.d/*.conf 2>/dev/null`
if [ -n "${HAS_INDIRECT_GLX}" ]; then
	sed -i 's/^options nvidia_drm modeset=1/#options nvidia_drm modeset=1/g' %{_sysconfdir}/modprobe.d/modprobe-nvidia.conf
else
	sed -i 's/#options nvidia_drm modeset=1/options nvidia_drm modeset=1/g' %{_sysconfdir}/modprobe.d/modprobe-nvidia.conf
fi

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
* Fri Dec 12 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.119.02-1.1
- Rebuilt against RHEL 9.7 errata kernel 5.14.0-611.8.1.el9_7

* Fri Dec 12 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.119.02-1
- Updated to version 580.119.02
- Built against RHEL 9.7 GA kernel

* Tue Nov 18 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.105.08-1.1
- Rebuilt against RHEL 9.7 errata kernel 5.14.0-611.8.1.el9_7

* Tue Nov 11 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.105.08-1
- Updated to version 580.105.08
- Built against RHEL 9.7 GA kernel

* Thu Oct 02 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.95.05-1.1
- Rebuilt against RHEL 9.6 errata kernel 5.14.0-570.42.2.el9_6

* Thu Oct 02 2025 Tuan Hoang <tqhoang@elrepo.org> - 580.95.05-1
- Updated to version 580.95.05
- Built against RHEL 9.6 GA kernel

* Thu Oct 02 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.195.03-1.1
- Rebuilt against RHEL 9.6 errata kernel 5.14.0-570.42.2.el9_6

* Thu Oct 02 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.195.03-1
- Updated to version 570.195.03
- Built against RHEL 9.6 GA kernel

* Thu Sep 11 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.190-1.1
- Rebuilt against RHEL 9.6 errata kernel 5.14.0-570.42.2.el9_6

* Thu Sep 11 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.190-1
- Updated to version 570.190
- Built against RHEL 9.6 GA kernel

* Wed Sep 03 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.181-2.1
- Rebuilt against RHEL 9.6 errata kernel 5.14.0-570.37.1.el9_6

* Wed Sep 03 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.181-2
- Built against RHEL 9.6 GA kernel
- Add workaround to prevent X11 crash with IndirectGLX

* Thu Aug 21 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.181-1.1
- Rebuilt against RHEL 9.6 errata kernel 5.14.0-570.35.1.el9_6

* Thu Aug 21 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.181-1
- Updated to version 570.181
- Built against RHEL 9.6 GA kernel
- Add modprobe-nvidia.conf

* Fri Jul 18 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.172.08-2
- Built against RHEL 9.6 errata kernel 5.14.0-570.26.1.el9_6

* Fri Jul 18 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.172.08-1
- Updated to version 570.172.08

* Thu Jun 19 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.169-1
- Updated to version 570.169

* Tue May 20 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.153.02-1
- Updated to version 570.153.02

* Wed May 14 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.144-2
- Rebuilt against RHEL 9.6 GA kernel

* Sat Apr 26 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.144-1
- Updated to version 570.144

* Sat Apr 05 2025 Tuan Hoang <tqhoang@elrepo.org> - 570.133.07-1
- Updated to version 570.133.07
- Add option to build kernel-open driver
- Add LICENSE file

* Tue Jan 28 2025 Tuan Hoang <tqhoang@elrepo.org> - 550.144.03-1
- Updated to version 550.144.03
- Rebuilt against RHEL 9.5 GA kernel

* Thu Dec 26 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.142-1
- Updated to version 550.142
- Rebuilt against RHEL 9.5 GA kernel

* Tue Nov 19 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.135-1
- Updated to version 550.135
- Rebuilt against RHEL 9.5 GA kernel

* Tue Nov 12 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.127.05-2
- Rebuilt against RHEL 9.5 GA kernel

* Tue Oct 22 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.127.05-1
- Updated to version 550.127.05

* Sat Oct 05 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.120-1
- Updated to version 550.120

* Thu Aug 01 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.107.02-1
- Updated to version 550.107.02

* Tue Jul 09 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.100-1
- Updated to version 550.100

* Wed Jun 05 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.90.07-1
- Updated to version 550.90.07

* Wed May 01 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.78-1
- Updated to version 550.78
- Built against RHEL 9.4 GA kernel

* Thu Apr 18 2024 Philip J Perry <phil@elrepo.org> - 550.76-1
- Updated to version 550.76

* Sat Mar 23 2024 Philip J Perry <phil@elrepo.org> - 550.67-1
- Updated to version 550.67

* Sun Feb 25 2024 Philip J Perry <phil@elrepo.org> - 550.54.14-1
- Updated to version 550.54.14
- Fork for RHEL9
