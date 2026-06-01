# Define the kmod package name here.
%define kmod_name	game-controllers

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 6.12.0-211.7.3.el10_2}

%{!?dist: %define dist .el10}

Name:		kmod-%{kmod_name}
Version:	0.0
Release:	1%{?dist}
Summary:	%{kmod_name} kernel module(s)
Group:		System Environment/Kernel
License:	GPLv2
URL:		http://www.kernel.org/

# Sources.
Source0:	drivers-leds.tar.gz
Source1:	drivers-hid.tar.gz
Source2:	drivers-input-joystick.tar.gz
Source5:	GPL-v2.0.txt

# Source code patches.
#
# led-class-multicolor upstream patches
# - Patch is from linux-6.18.y
#
Patch10: 	0001-leds-multicolor-Fix-intensity-setting-while-SW-blink.patch
#
# hid-nintendo upstream patches
# - Patch is from linux-6.18.y
#
Patch20: 	0001-HID-nintendo-Rate-limit-IMU-compensation-message.patch
#
# hid-playstation upstream patches
# - Patch30 is from linux-6.12.y (apply reversed)
# - Patch31+ are from linux-6.18.y
#
Patch30: 	0000-HID-playstation-Fix-memory-leak-in-dualshock4_get_ca.patch
Patch31: 	0001-HID-playstation-Make-use-of-bitfield-macros.patch
Patch32: 	0002-HID-playstation-Add-spaces-around-arithmetic-operato.patch
Patch33: 	0003-HID-playstation-Simplify-locking-with-guard-and-scop.patch
Patch34: 	0004-HID-playstation-Replace-uint-32-16-8-_t-with-u-32-16.patch
Patch35: 	0005-HID-playstation-Correct-spelling-in-comment-sections.patch
Patch36: 	0006-HID-playstation-Fix-all-alignment-and-line-length-is.patch
Patch37: 	0007-HID-playstation-Document-spinlock_t-usage.patch
Patch38: 	0008-HID-playstation-Prefer-kzalloc-sizeof-buf.patch
Patch39: 	0009-HID-playstation-Redefine-DualSense-input-report-stat.patch
Patch40: 	0010-HID-playstation-Support-DualSense-audio-jack-hotplug.patch
Patch41: 	0011-HID-playstation-Support-DualSense-audio-jack-event-r.patch
Patch42: 	0012-HID-playstation-Update-SP-preamp-gain-comment-line.patch
Patch43: 	0013-HID-playstation-Silence-sparse-warnings-for-locking-.patch
Patch44: 	0014-HID-playstation-Switch-to-scoped_guard-in-dualsense-.patch
Patch45: 	0015-HID-playstation-Fix-memory-leak-in-dualshock4_get_ca.patch
Patch46: 	0016-HID-playstation-Center-initial-joystick-axes-to-prev.patch
Patch47: 	0017-HID-playstation-Add-missing-check-for-input_ff_creat.patch
Patch48: 	0018-HID-playstation-Clamp-num_touch_reports.patch
#
# xpad upstream patches
# - Patches are from linux master (7.1-rc6)
#
Patch50: 	0001-Input-xpad-add-support-for-CRKD-Guitars.patch
Patch51: 	0004-Input-xpad-remove-stale-TODO-and-changelog-header.patch
Patch52: 	0005-Input-xpad-add-RedOctane-Games-vendor-id.patch
Patch53: 	0006-Input-xpad-add-support-for-Razer-Wolverine-V3-Pro.patch
Patch54: 	0007-Input-xpad-add-support-for-BETOP-BTP-KP50B-C-control.patch
Patch55: 	0008-Input-xpad-fix-out-of-bounds-access-for-Share-button.patch
Patch56: 	0009-Input-xpad-add-support-for-ASUS-ROG-RAIKIRI-II.patch
Patch57: 	0010-Input-xpad-add-Nova-2-Lite-from-GameSir.patch

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
%setup -q -c -n %{kmod_name}-%{version}

# List of HID modules and dependencies
# Keep in sync with make command args below
%define GAME_CONTROLLER_MODULES "led-class-multicolor hid-nintendo hid-playstation xpad"

cat /dev/null > kmod-%{kmod_name}.conf
for modules in `echo -n %{GAME_CONTROLLER_MODULES}`
do
	echo "override $modules * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
done

%{__tar} -xf %{SOURCE0}
%{__tar} -xf %{SOURCE1}
%{__tar} -xf %{SOURCE2}

# Apply patch(es).
pushd drivers-leds
%patch -P10 -p3
popd
pushd drivers-hid
%patch -P20 -p3
%patch -P30 -p3 -R
%patch -P31 -p3
%patch -P32 -p3
%patch -P33 -p3
%patch -P34 -p3
%patch -P35 -p3
%patch -P36 -p3
%patch -P37 -p3
%patch -P38 -p3
%patch -P39 -p3
%patch -P40 -p3
%patch -P41 -p3
%patch -P42 -p3
%patch -P43 -p3
%patch -P44 -p3
%patch -P45 -p3
%patch -P46 -p3
%patch -P47 -p3
%patch -P48 -p3
popd
pushd drivers-input-joystick
%patch -P50 -p4
%patch -P51 -p4
%patch -P52 -p4
%patch -P53 -p4
%patch -P54 -p4
%patch -P55 -p4
%patch -P56 -p4
%patch -P57 -p4
popd

%build
# The EXTRA_CFLAGS is required for any drivers that use ifdef or IS_REACHABLE macros
pushd drivers-leds
KBES=$PWD
%{__make} -C %{kernel_source} %{?_smp_mflags} V=1 modules M=$PWD \
	CONFIG_LEDS_CLASS_MULTICOLOR=m \
        EXTRA_CFLAGS+='-DCONFIG_LEDS_CLASS_MULTICOLOR'
popd
pushd drivers-hid
%{__make} -C %{kernel_source} %{?_smp_mflags} V=1 modules M=$PWD \
	CONFIG_HID_NINTENDO=m \
	CONFIG_NINTENDO_FF=y \
	CONFIG_HID_PLAYSTATION=m \
	CONFIG_PLAYSTATION_FF=y \
        EXTRA_CFLAGS+='-DCONFIG_HID_NINTENDO' \
        EXTRA_CFLAGS+='-DCONFIG_NINTENDO_FF' \
        EXTRA_CFLAGS+='-DCONFIG_HID_PLAYSTATION' \
        EXTRA_CFLAGS+='-DCONFIG_PLAYSTATION_FF' \
	KBUILD_EXTRA_SYMBOLS+="${KBES}/Module.symvers"
popd
pushd drivers-input-joystick
%{__make} -C %{kernel_source} %{?_smp_mflags} V=1 modules M=$PWD \
	CONFIG_INPUT_JOYSTICK=y \
	CONFIG_JOYSTICK_XPAD=m \
        EXTRA_CFLAGS+='-DCONFIG_INPUT_JOYSTICK' \
        EXTRA_CFLAGS+='-DCONFIG_JOYSTICK_XPAD'
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
for modules in `echo -n %{GAME_CONTROLLER_MODULES}`
do
	find . -name ${modules}.ko -exec \
	%{__install} -t %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/ \
	{} +
done
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
%doc %{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

%changelog
* Sun May 31 2026 Tuan Hoang <tqhoang@elrepo.org> - 0.0-1
- Initial build for game controllers (hid-nintendo, hid-playstation, xpad)
- Source code from RHEL 10.2 GA kernel-6.12.0-211.7.3.el10_2
- Built against RHEL 10.2 GA kernel-6.12.0-211.7.3.el10_2
