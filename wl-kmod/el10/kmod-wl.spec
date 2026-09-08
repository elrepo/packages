# Define the kmod package name here.
%define kmod_name	wl

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 6.12.0-211.7.3.el10_2}

%{!?dist: %define dist .el10}

Name:		kmod-%{kmod_name}
Version:	6.30.223.271
Release:	1%{?dist}
Summary:	%{kmod_name} kernel module(s)
Group:		System Environment/Kernel
License:	Redistributable, no modification permitted
URL:		https://www.broadcom.com/site-search?q=802.11%20linux%20sta%20wireless%20driver

# Sources.
Source0:	https://docs.broadcom.com/docs-and-downloads/docs/linux_sta/hybrid-v35-nodebug-pcoem-6_30_223_271.tar.gz
Source1:	https://docs.broadcom.com/docs-and-downloads/docs/linux_sta/hybrid-v35_64-nodebug-pcoem-6_30_223_271.tar.gz
Source2:	README.txt
Source3:	blacklist-wl.conf
Source4:	kmod-wl-unload-on-shutdown.service
Source5:	90-kmod-wl-unload-on-shutdown.preset

%if %{?_with_src:0}%{!?_with_src:1}
NoSource:	0
NoSource:	1
%endif

# Source code patches.
Patch0: 	01-shipped-module.patch
Patch1: 	02-Parse-KERNELRELEASE-into-VERSION-PATCHLEVEL-and-SUBLEV.patch
Patch2: 	03-rename-to-wlan0.patch
Patch3: 	04-user_ioctl.patch
Patch4: 	05-remove-time-and-date-macros.patch
Patch5: 	06-broadcom-sta-6.30.223.248-linux-3.18-null-pointer-crash.patch
Patch6: 	07-rdtscl.patch
Patch7: 	08-linux47.patch
Patch8: 	09-linux48.patch
Patch9: 	10-fix-kernel-warnings.patch
Patch10: 	11-linux411.patch
Patch11: 	12-linux412.patch
Patch12: 	13-linux414.patch
Patch13: 	14-linux415.patch
Patch14: 	15-linux51.patch
Patch15: 	16-linux56.patch
Patch16: 	17-Get-rid-of-get_fs-set_fs-calls.patch
Patch17: 	18-wl-Make-sure-power_mgmt-settings-are-honored.patch
Patch18: 	19-wl-Fix-get-set-values-for-tx_power.patch
Patch19: 	20-wl-Fix-mac-address-setting.patch
Patch20: 	21-wl-Fix-misleading-indentation.patch
Patch21: 	22-wl-Fix-fall-through-warnings.patch
Patch22: 	23-wl-Avoid-disconnecting-invalid-interface.patch
Patch23: 	24-wl-Use-the-right-enums-for-cfg80211_get_bss.patch
Patch24: 	25-linux518.patch
Patch25: 	26-linux600.patch
Patch26: 	27-wl-Update-for-linux-5.17-deprecations.patch
Patch27: 	28-linux601.patch
Patch28: 	29-fix-version-parsing.patch
Patch29: 	30-6.12-unaligned-header-location.patch
Patch30: 	31-build-Provide-local-lib80211.h-header.patch
Patch31: 	32-Prepare-for-6.14.0-rc6.patch
Patch32: 	33-wl-Add-explanation-for-IBT-warnings-in-newer-kernels.patch
Patch33: 	34-wl-Remove-redefinition-warning-for-isprint.patch
Patch34: 	35-wl-Use-kernel-keyword-for-fallthrough.patch
Patch35: 	36-wl-Add-missing-braces-as-recommended-by-gcc.patch
Patch36: 	37-wl-Fix-memcpy-field-spanning-write-warning.patch
Patch37: 	38-build-don-t-use-deprecated-EXTRA_-FLAGS.patch
Patch38: 	39-wl-use-timer_delete-for-kernel-6.15.patch
Patch39: 	40-wl-add-MODULE_DESCRIPTION.patch
Patch40: 	41-wl-use-timer_container_of-for-kernel-6.16.patch
Patch41: 	42-build-dirty-fix-for-the-linking.patch
Patch42: 	43-wl-Adapt-to-linux-6.17-cfg80211-changes.patch
Patch43: 	44-wl-Do-not-flush-system-wide-queue.patch
Patch44: 	45-broadcom-wl-fix-linux-6.5.patch
Patch45: 	46-linux71.patch
Patch46: 	47-linux72.patch
Patch100: 	elrepo-wl-buildfix-el10_2.patch
Patch101: 	elrepo-wl-wpa_supplicant-2.11.patch
Patch102: 	elrepo-wl-readme-devices.patch

# For systemd_ scriptlets
BuildRequires:	systemd-rpm-macros

# Fix for the SB-signing issue caused by a bug in /usr/lib/rpm/brp-strip
# https://bugzilla.redhat.com/show_bug.cgi?id=1967291

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

BuildRoot:			%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

ExclusiveArch:		x86_64

BuildRequires:		elfutils-libelf-devel
BuildRequires:		kernel-abi-stablelists
BuildRequires:		kernel-devel = %{kmod_kernel_version}
BuildRequires:		kernel-rpm-macros
BuildRequires:		redhat-rpm-config
BuildRequires:		rpm-build
BuildRequires:		gcc
BuildRequires:		make

Provides:			kernel-modules >= %{kmod_kernel_version}.%{_arch}
Provides:			kmod-%{kmod_name} = %{?epoch:%{epoch}:}%{version}-%{release}

Requires:			kernel >= %{kmod_kernel_version}
Requires:			kernel-core-uname-r >= %{kmod_kernel_version}

Requires(post):		%{_sbindir}/depmod
Requires(postun):	%{_sbindir}/depmod
Requires(post):		%{_sbindir}/weak-modules
Requires(postun):	%{_sbindir}/weak-modules

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
# Ubuntu patches require both i386 and amd64 source dirs
%setup -q -c -T
%__mkdir i386
cd i386
%__tar xzf %{SOURCE0}
%__cp -p lib/wlc_hybrid.o_shipped lib/wlc_hybrid.o_i386
%__cp -p lib/LICENSE.txt ..
cd ..
%__mkdir amd64
cd amd64
%__tar xzf %{SOURCE1}
%__cp -p lib/wlc_hybrid.o_shipped lib/wlc_hybrid.o_amd64
%__cp -p lib/LICENSE.txt ..
cd ..
%__cp -p %{SOURCE2} .

echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

# Apply patch(es).
%patch -P0   -p1
%patch -P1   -p1
%patch -P2   -p1
%patch -P3   -p1
%patch -P4   -p1
%patch -P5   -p1
%patch -P6   -p1
%patch -P7   -p1
%patch -P8   -p1
%patch -P9   -p1
%patch -P10  -p1
%patch -P11  -p1
%patch -P12  -p1
%patch -P13  -p1
%patch -P14  -p1
%patch -P15  -p1
%patch -P16  -p1
%patch -P17  -p1
%patch -P18  -p1
%patch -P19  -p1
%patch -P20  -p1
%patch -P21  -p1
%patch -P22  -p1
%patch -P23  -p1
%patch -P24  -p1
%patch -P25  -p1
%patch -P26  -p1
%patch -P27  -p1
%patch -P28  -p1
%patch -P29  -p1
%patch -P30  -p1
%patch -P31  -p1
%patch -P32  -p1
%patch -P33  -p1
%patch -P34  -p1
%patch -P35  -p1
%patch -P36  -p1
%patch -P37  -p1
%patch -P38  -p1
%patch -P39  -p1
%patch -P40  -p1
%patch -P41  -p1
%patch -P42  -p1
%patch -P43  -p1
%patch -P44  -p1
%patch -P45  -p1
%patch -P46  -p1
%patch -P100 -p1
%patch -P101 -p1
%patch -P102 -p1

%build
cd amd64
%{__make} -C %{kernel_source} %{?_smp_mflags} modules M=$PWD objtool=/usr/bin/true
cd ..

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
%{__install} */%{kmod_name}.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_prefix}/lib/modprobe.d/
%{__install} -m 0644 %{SOURCE3} %{buildroot}%{_prefix}/lib/modprobe.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -m 0644 greylist.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -m 0644 README.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# Install the systemd files
%{__mkdir_p} $RPM_BUILD_ROOT%{_unitdir}/
%{__install} -p -m 0644 %{SOURCE4} $RPM_BUILD_ROOT%{_unitdir}/
%{__mkdir_p} $RPM_BUILD_ROOT%{_presetdir}/
%{__install} -p -m 0644 %{SOURCE5} $RPM_BUILD_ROOT%{_presetdir}/

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
modules=( $(find /lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name} | grep '\.ko$') )
printf '%s\n' "${modules[@]}" | %{_sbindir}/weak-modules --add-modules --no-initramfs

mkdir -p "%{kver_state_dir}"
touch "%{kver_state_file}"

%systemd_post kmod-wl-unload-on-shutdown.service

exit 0

%posttrans
# We have to re-implement part of weak-modules here because it doesn't allow
# calling initramfs regeneration separately
if [ -f "%{kver_state_file}" ]; then
        kver_base="%{kmod_kernel_version}"
        kvers=$(ls -d "/lib/modules/${kver_base%%%%-*}"*)

        for k_dir in $kvers; do
                k="${k_dir#/lib/modules/}"

                tmp_initramfs="/boot/initramfs-$k.tmp"
                dst_initramfs="/boot/initramfs-$k.img"

                # The same check as in weak-modules: we assume that the kernel present
                # if the symvers file exists.
                if [ -e "/$k_dir/symvers.xz" ]; then
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

%systemd_preun kmod-wl-unload-on-shutdown.service

exit 0

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
%config %{_sysconfdir}/depmod.d/kmod-%{kmod_name}.conf
%config(noreplace) %{_prefix}/lib/modprobe.d/blacklist*.conf
%doc %{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%license LICENSE.txt
%{_unitdir}/*.service
%{_presetdir}/*.preset

%changelog
* Mon Sep 07 2026 Tuan Hoang <tqhoang@elrepo.org> - 6.30.223.271-1
- Initial build for RHEL 10.2
- Built against RHEL 10.2 GA kernel 6.12.0-211.7.3.el10_2
- Original patches from Ubuntu Linux
  [https://git.launchpad.net/ubuntu/+source/broadcom-sta]
