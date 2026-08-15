# Define the kmod package name here.
%define kmod_name	b43

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 5.14.0-687.5.3.el9_8}

%{!?dist: %define dist .el9}

Name:		kmod-%{kmod_name}
Version:	0.0
Release:	1%{?dist}
Summary:	%{kmod_name} kernel module(s)
Group:		System Environment/Kernel
License:	GPLv2
URL:		http://www.kernel.org/

# Sources.
Source0:	%{kmod_name}-%{version}.tar.gz
Source1:	modprobe-b43.conf
Source2:	modprobe-b43legacy.conf
Source5:	GPL-v2.0.txt

# Source code patches.

# bcma upstream patches
Patch0: 	0001-bcma-don-t-register-devices-disabled-in-OF.patch

# ssb upstream patches
Patch5: 	0001-ssb-Fix-potential-NULL-pointer-dereference-in-ssb_de.patch
Patch6: 	0002-ssb-Fix-division-by-zero-issue-in-ssb_calc_clock_rat.patch

# b43 upstream patches
Patch10: 	0001-b43-fix-a-lower-bounds-test.patch
Patch11: 	0002-b43-Fix-assigning-negative-value-to-unsigned-variabl.patch
# Patch12: 	0003-wifi-mac80211-split-bss_info_changed-method.patch
# Patch13: 	0004-wifi-mac80211-return-a-beacon-for-a-specific-link.patch
# Patch14: 	0005-wifi-mac80211-change-QoS-settings-API-to-take-link-i.patch
Patch15: 	0006-wifi-b43-fix-repeated-words-in-comments.patch
Patch16: 	0007-wifi-b43-do-not-initialise-static-variable-to-0.patch
# Patch17: 	0008-wifi-move-from-strlcpy-with-unused-retval-to-strscpy.patch
Patch18: 	0009-wifi-b43-remove-empty-switch-statement.patch
# Patch19: 	0010-wifi-mac80211-add-wake_tx_queue-callback-to-drivers.patch
Patch20: 	0011-wifi-b43-remove-reference-to-removed-config-B43_PCMC.patch
# Patch21: 	0012-wifi-b43-fix-incorrect-__packed-annotation.patch
Patch22: 	0013-wifi-b43-Stop-wake-correct-queue-in-DMA-Tx-path-when.patch
Patch23: 	0014-wifi-b43-Stop-wake-correct-queue-in-PIO-Tx-path-when.patch
Patch24: 	0015-wifi-b43-Stop-correct-queue-in-DMA-worker-when-QoS-i.patch
Patch25: 	0016-wifi-b43-Disable-QoS-for-bcm4331.patch
Patch26: 	0017-wifi-b43-enforce-bounds-check-on-firmware-key-index-.patch

# b43legacy upstream patches
Patch30:	0001-b43legacy-fix-a-lower-bounds-test.patch
Patch31:	0002-b43legacy-Fix-assigning-negative-value-to-unsigned-v.patch
# Patch32:	0003-wifi-mac80211-split-bss_info_changed-method.patch
# Patch33:	0004-wifi-mac80211-return-a-beacon-for-a-specific-link.patch
# Patch34:	0005-wifi-mac80211-change-QoS-settings-API-to-take-link-i.patch
Patch35:	0006-wifi-b43legacy-clean-up-one-inconsistent-indenting.patch
# Patch36:	0007-wifi-move-from-strlcpy-with-unused-retval-to-strscpy.patch
# Patch37:	0008-wifi-mac80211-add-wake_tx_queue-callback-to-drivers.patch
# Patch38:	0009-wifi-b43legacy-remove-unused-freq_r3A_value-function.patch
# Patch39:	0010-wifi-b43legacy-Remove-the-unused-function-prev_slot.patch
# Patch40:	0011-wifi-b43-fix-incorrect-__packed-annotation.patch
Patch41:	0012-wifi-b43legacy-enforce-bounds-check-on-firmware-key-.patch

Recommends:	b43-fwcutter
Recommends:	b43-openfwwf
Recommends:	b43-tools

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
%setup -q -n %{kmod_name}-%{version}
cat /dev/null > kmod-%{kmod_name}.conf
echo "override bcma * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override ssb * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override b43 * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override b43legacy * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf

# Apply patch(es).

# bcma upstream patches
%patch0 -p2

# ssb upstream patches
%patch5 -p2
%patch6 -p2

# b43 upstream patches
%patch10 -p5
%patch11 -p5
# patch12 -p5
# patch13 -p5
# patch14 -p5
%patch15 -p5
%patch16 -p5
# patch17 -p5
%patch18 -p5
# patch19 -p5
%patch20 -p5
# patch21 -p5
%patch22 -p5
%patch23 -p5
%patch24 -p5
%patch25 -p5
%patch26 -p5

# b43legacy upstream patches
%patch30 -p5
%patch31 -p5
# patch32 -p5
# patch33 -p5
# patch34 -p5
%patch35 -p5
# patch36 -p5
# patch37 -p5
# patch38 -p5
# patch39 -p5
# patch40 -p5
%patch41 -p5

%build
pushd bcma
%{__make} -C %{kernel_source} %{?_smp_mflags} V=1 modules M=$PWD \
	CONFIG_BCMA=m \
	CONFIG_BCMA_BLOCKIO=y \
	CONFIG_BCMA_HOST_PCI_POSSIBLE=y \
	CONFIG_BCMA_HOST_PCI=y \
	CONFIG_BCMA_DRIVER_PCI=y \
	CONFIG_BCMA_DRIVER_GMAC_CMN=y \
	CONFIG_BCMA_DRIVER_GPIO=y \
	EXTRA_CFLAGS+='-DCONFIG_BCMA -DCONFIG_BCMA_BLOCKIO -DCONFIG_BCMA_HOST_PCI_POSSIBLE -DCONFIG_BCMA_HOST_PCI -DCONFIG_BCMA_DRIVER_PCI -DCONFIG_BCMA_DRIVER_GMAC_CMN -DCONFIG_BCMA_DRIVER_GPIO' \

KBES_BCMA=$PWD
popd

pushd ssb
%{__make} -C %{kernel_source} %{?_smp_mflags} V=1 modules M=$PWD \
	CONFIG_SSB=m \
	CONFIG_SSB_SPROM=y \
	CONFIG_SSB_BLOCKIO=y \
	CONFIG_SSB_PCIHOST_POSSIBLE=y \
	CONFIG_SSB_PCIHOST=y \
	CONFIG_SSB_B43_PCI_BRIDGE=y \
	EXTRA_CFLAGS+='-DCONFIG_SSB -DCONFIG_SSB_SPROM -DCONFIG_SSB_BLOCKIO -DCONFIG_SSB_PCIHOST_POSSIBLE -DCONFIG_SSB_PCIHOST -DCONFIG_SSB_B43_PCI_BRIDGE' \

KBES_SSB=$PWD
popd

pushd b43
%{__make} -C %{kernel_source} %{?_smp_mflags} V=1 modules M=$PWD \
	CONFIG_B43=m \
	CONFIG_B43_BUSES_BCMA_AND_SSB=y \
	CONFIG_B43_BCMA=y \
	CONFIG_B43_SSB=y \
	CONFIG_B43_PCI_AUTOSELECT=y \
	CONFIG_B43_PIO=y \
	CONFIG_B43_PHY_G=y \
	CONFIG_B43_PHY_N=y \
	CONFIG_B43_PHY_LP=y \
	CONFIG_B43_PHY_HT=y \
	CONFIG_B43_LEDS=y \
	CONFIG_B43_HWRNG=y \
	CONFIG_B43_DEBUG=n \
	EXTRA_CFLAGS+='-DCONFIG_B43 -DCONFIG_B43_BUSES_BCMA_AND_SSB -DCONFIG_B43_BCMA -DCONFIG_B43_SSB -DCONFIG_B43_PCI_AUTOSELECT -DCONFIG_B43_PIO -DCONFIG_B43_PHY_G -DCONFIG_B43_PHY_N -DCONFIG_B43_PHY_LP -DCONFIG_B43_PHY_HT -DCONFIG_B43_LEDS -DCONFIG_B43_HWRNG' \
	\
	CONFIG_BCMA=m \
	CONFIG_BCMA_BLOCKIO=y \
	CONFIG_BCMA_HOST_PCI_POSSIBLE=y \
	CONFIG_BCMA_HOST_PCI=y \
	CONFIG_BCMA_DRIVER_PCI=y \
	CONFIG_BCMA_DRIVER_GMAC_CMN=y \
	CONFIG_BCMA_DRIVER_GPIO=y \
	EXTRA_CFLAGS+='-DCONFIG_BCMA -DCONFIG_BCMA_BLOCKIO -DCONFIG_BCMA_HOST_PCI_POSSIBLE -DCONFIG_BCMA_HOST_PCI -DCONFIG_BCMA_DRIVER_PCI -DCONFIG_BCMA_DRIVER_GMAC_CMN -DCONFIG_BCMA_DRIVER_GPIO' \
	\
	CONFIG_SSB=m \
	CONFIG_SSB_SPROM=y \
	CONFIG_SSB_BLOCKIO=y \
	CONFIG_SSB_PCIHOST_POSSIBLE=y \
	CONFIG_SSB_PCIHOST=y \
	CONFIG_SSB_B43_PCI_BRIDGE=y \
	EXTRA_CFLAGS+='-DCONFIG_SSB -DCONFIG_SSB_SPROM -DCONFIG_SSB_BLOCKIO -DCONFIG_SSB_PCIHOST_POSSIBLE -DCONFIG_SSB_PCIHOST -DCONFIG_SSB_B43_PCI_BRIDGE' \
	\
	KBUILD_EXTRA_SYMBOLS+="${KBES_BCMA}/Module.symvers" \
	KBUILD_EXTRA_SYMBOLS+="${KBES_SSB}/Module.symvers" \

popd

pushd b43legacy
%{__make} -C %{kernel_source} %{?_smp_mflags} V=1 modules M=$PWD \
	CONFIG_B43LEGACY=m \
	CONFIG_B43LEGACY_PCI_AUTOSELECT=y \
	CONFIG_B43LEGACY_DMA_AND_PIO_MODE=y \
	CONFIG_B43LEGACY_DMA_MODE=y \
	CONFIG_B43LEGACY_DMA=y \
	CONFIG_B43LEGACY_PIO_MODE=y \
	CONFIG_B43LEGACY_PIO=y \
	CONFIG_B43LEGACY_LEDS=y \
	CONFIG_B43LEGACY_HWRNG=y \
	CONFIG_B43LEGACY_DEBUG=y \
	EXTRA_CFLAGS+='-DCONFIG_B43LEGACY -DCONFIG_B43LEGACY_PCI_AUTOSELECT -DCONFIG_B43LEGACY_DMA_AND_PIO_MODE -DCONFIG_B43LEGACY_DMA_MODE -DCONFIG_B43LEGACY_DMA -DCONFIG_B43LEGACY_PIO_MODE -DCONFIG_B43LEGACY_PIO -DCONFIG_B43LEGACY_LEDS -DCONFIG_B43LEGACY_HWRNG -DCONFIG_B43LEGACY_DEBUG' \
	\
	CONFIG_BCMA=m \
	CONFIG_BCMA_BLOCKIO=y \
	CONFIG_BCMA_HOST_PCI_POSSIBLE=y \
	CONFIG_BCMA_HOST_PCI=y \
	CONFIG_BCMA_DRIVER_PCI=y \
	CONFIG_BCMA_DRIVER_GMAC_CMN=y \
	CONFIG_BCMA_DRIVER_GPIO=y \
	EXTRA_CFLAGS+='-DCONFIG_BCMA -DCONFIG_BCMA_BLOCKIO -DCONFIG_BCMA_HOST_PCI_POSSIBLE -DCONFIG_BCMA_HOST_PCI -DCONFIG_BCMA_DRIVER_PCI -DCONFIG_BCMA_DRIVER_GMAC_CMN -DCONFIG_BCMA_DRIVER_GPIO' \
	\
	CONFIG_SSB=m \
	CONFIG_SSB_SPROM=y \
	CONFIG_SSB_BLOCKIO=y \
	CONFIG_SSB_PCIHOST_POSSIBLE=y \
	CONFIG_SSB_PCIHOST=y \
	CONFIG_SSB_B43_PCI_BRIDGE=y \
	EXTRA_CFLAGS+='-DCONFIG_SSB -DCONFIG_SSB_SPROM -DCONFIG_SSB_BLOCKIO -DCONFIG_SSB_PCIHOST_POSSIBLE -DCONFIG_SSB_PCIHOST -DCONFIG_SSB_B43_PCI_BRIDGE' \
	\
	KBUILD_EXTRA_SYMBOLS+="${KBES_BCMA}/Module.symvers" \
	KBUILD_EXTRA_SYMBOLS+="${KBES_SSB}/Module.symvers" \

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
%{__install} -d %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} */*.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} -m 0644 %{SOURCE1} %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} -m 0644 %{SOURCE2} %{buildroot}%{_sysconfdir}/modprobe.d/
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
modules=( $(find /lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name} | grep '\.ko$') )
printf '%s\n' "${modules[@]}" | %{_sbindir}/weak-modules --add-modules --no-initramfs

mkdir -p "%{kver_state_dir}"
touch "%{kver_state_file}"

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
%config %{_sysconfdir}/depmod.d/kmod-%{kmod_name}.conf
%config(noreplace) %{_sysconfdir}/modprobe.d/modprobe-%{kmod_name}*.conf
%doc %{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

%changelog
* Wed Aug 12 2026 Tuan Hoang <tqhoang@elrepo.org> - 0.0-1
- Initial build for EL9.8
- Source from RHEL 9.8 GA kernel 5.14.0-687.5.3.el9_8
- Added upstream patches from linux 6.6.151
- Built against RHEL 9.8 GA kernel 5.14.0-687.5.3.el9_8
