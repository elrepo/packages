# Define the kmod package name here.
%define kmod_name	b43

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 4.18.0-553.el8_10}

%{!?dist: %define dist .el8}

Name:		kmod-%{kmod_name}
Version:	0.0
Release:	1%{?dist}
Summary:	%{kmod_name} kernel module(s)
Group:		System Environment/Kernel
License:	GPLv2
URL:		http://www.kernel.org/

# Sources
Source0:	%{kmod_name}-%{version}.tar.gz
Source1:	dracut-b43.conf
Source2:	dracut-b43legacy.conf
Source3:	modprobe-b43.conf
Source4:	modprobe-b43legacy.conf
Source5:	GPL-v2.0.txt

# Source code patches.

# bcma upstream patches
Patch0: 	0001-bcma-Allow-selection-of-this-driver-when-COMPILE_TES.patch
Patch1: 	0002-bcma-fix-incorrect-update-of-BCMA_CORE_PCI_MDIO_DATA.patch
Patch2: 	0003-bcma-Fix-memory-leak-for-internally-handled-cores.patch
Patch3: 	6601-bcma-don-t-register-devices-disabled-in-OF.patch

# ssb upstream patches
Patch10: 	0001-ssb-driver_gige-use-true-and-false-for-boolean-value.patch
Patch11: 	0002-ssb-Remove-home-grown-printk-wrappers.patch
Patch12: 	0003-ssb-Remove-SSB_WARN_ON-SSB_BUG_ON-and-SSB_DEBUG.patch
Patch13: 	0004-ssb-Fix-possible-NULL-pointer-dereference-in-ssb_hos.patch
Patch14: 	0005-ssb-sdio-Don-t-overwrite-const-buffer-if-block_write.patch
Patch15: 	0006-ssb-Fix-error-return-code-in-ssb_bus_scan.patch
Patch16: 	0007-ssb-treewide-Remove-uninitialized_var-usage.patch
Patch17: 	0008-ssb-Fix-division-by-zero-issue-in-ssb_calc_clock_rat.patch
Patch18: 	6601-ssb-Fix-potential-NULL-pointer-dereference-in-ssb_de.patch

# b43 upstream patches
Patch20: 	0001-b43-leds-Ensure-NUL-termination-of-LED-name-string.patch
Patch21: 	0002-b43-fix-DMA-error-related-regression-with-proprietar.patch
Patch22: 	0003-b43-Fix-error-in-cordic-routine.patch
Patch23: 	0004-b43-shut-up-clang-Wuninitialized-variable-warning.patch
Patch24: 	0005-b43-Fix-connection-problem-with-WPA3.patch
Patch25: 	0006-b43-N-PHY-Fix-the-update-of-coef-for-the-PHY-revisio.patch
Patch26: 	0007-b43-fix-a-lower-bounds-test.patch
Patch27: 	0008-b43-Fix-assigning-negative-value-to-unsigned-variabl.patch
Patch28: 	0009-wifi-b43-fix-incorrect-__packed-annotation.patch
Patch29: 	0010-b43-treewide-Remove-uninitialized_var-usage.patch
Patch30: 	0011-b43-dma-Fix-use-true-false-for-bool-type-variable.patch
Patch31: 	0012-wifi-b43-Stop-wake-correct-queue-in-DMA-Tx-path-when.patch
Patch32: 	0013-wifi-b43-Stop-wake-correct-queue-in-PIO-Tx-path-when.patch
Patch33: 	0014-b43-main-Fix-use-true-false-for-bool-type.patch
Patch34: 	0015-wifi-b43-Stop-correct-queue-in-DMA-worker-when-QoS-i.patch
Patch35: 	0016-wifi-b43-Disable-QoS-for-bcm4331.patch
Patch36: 	6601-wifi-b43-enforce-bounds-check-on-firmware-key-index-.patch

# b43legacy upstream patches
Patch40: 	0001-b43legacy-leds-Ensure-NUL-termination-of-LED-name-st.patch
Patch41: 	0002-b43legacy-Fix-Wcast-function-type.patch
Patch42: 	0003-b43legacy-Fix-case-where-channel-status-is-corrupted.patch
Patch43: 	0004-b43legacy-Fix-connection-problem-with-WPA3.patch
Patch44: 	0005-b43legacy-fix-a-lower-bounds-test.patch
Patch45: 	0006-b43legacy-Fix-assigning-negative-value-to-unsigned-v.patch
Patch46: 	0007-b43legacy-fix-incorrect-__packed-annotation.patch
Patch47: 	0008-b43legacy-treewide-Remove-uninitialized_var-usage.patch
Patch48: 	6601-wifi-b43legacy-enforce-bounds-check-on-firmware-key-.patch

Recommends:	b43-fwcutter
Recommends:	b43-tools
# Recommends:	b43-openfwwf

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
%patch1 -p2
%patch2 -p2
%patch3 -p2

# ssb upstream patches
%patch10 -p2
#patch11 -p2
#patch12 -p2
%patch13 -p2
%patch14 -p2
%patch15 -p2
%patch16 -p2
%patch17 -p2
%patch18 -p2

# b43 upstream patches
%patch20 -p5
%patch21 -p5
%patch22 -p5
%patch23 -p5
%patch24 -p5
#patch25 -p5
%patch26 -p5
%patch27 -p5
#patch28 -p5
%patch29 -p5
#patch30 -p5
%patch31 -p5
%patch32 -p5
#patch33 -p5
%patch34 -p5
%patch35 -p5
%patch36 -p5

# b43legacy upstream patches
%patch40 -p5
%patch41 -p5
%patch42 -p5
%patch43 -p5
%patch44 -p5
%patch45 -p5
#patch46 -p5
%patch47 -p5
%patch48 -p5

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
%{__install} */*.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_sysconfdir}/dracut.conf.d/
%{__install} -m 0644 %{SOURCE1} %{buildroot}%{_sysconfdir}/dracut.conf.d/
%{__install} -m 0644 %{SOURCE2} %{buildroot}%{_sysconfdir}/dracut.conf.d/
%{__install} -d %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} -m 0644 %{SOURCE3} %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} -m 0644 %{SOURCE4} %{buildroot}%{_sysconfdir}/modprobe.d/
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
%config %{_sysconfdir}/depmod.d/kmod-%{kmod_name}.conf
%config %{_sysconfdir}/dracut.conf.d/dracut-%{kmod_name}*.conf
%config(noreplace) %{_sysconfdir}/modprobe.d/modprobe-%{kmod_name}*.conf
%doc %{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

%changelog
* Sun Aug 30 2026 Tuan Hoang <tqhoang@elrepo.org> - 0.0-1
- Initial build for EL8.10
- Built against RHEL 8.10 GA kernel 4.18.0-553.el8_10
- Source from RHEL 8.10 GA kernel 4.18.0-553.el8_10
- Added upstream patches from linux 4.19.325 (00xx prefix)
- Added upstream patches from linux 6.6.151 (66xx prefix)
