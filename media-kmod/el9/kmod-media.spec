# Define the kmod package name here.
%define kmod_name	media

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 5.14.0-570.42.2.el9_6}

%{!?dist: %define dist .el9}

Name:		kmod-%{kmod_name}
Version:	0.0
Release:	4%{?dist}
Summary:	%{kmod_name} kernel module(s)
Group:		System Environment/Kernel
License:	GPLv2
URL:		http://www.kernel.org/

# Sources.
Source0:	%{kmod_name}-%{version}.tar.gz
Source5:	GPL-v2.0.txt

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
Provides:		kmod-%{kmod_name} = %{?epoch:%{epoch}:}%{version}-%{release}

Requires:		kernel >= %{kmod_kernel_version}
Requires:		kernel-core-uname-r >= %{kmod_kernel_version}

Requires(post):		%{_sbindir}/depmod
Requires(postun):	%{_sbindir}/depmod
Requires(post):		%{_sbindir}/weak-modules
Requires(postun):	%{_sbindir}/weak-modules

Recommends:		xc3028-firmware

Obsoletes:		kmod-hdpvr <= 0.2.1
Provides:		kmod-hdpvr  = 0.2.1-99%{?dist}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{kmod_name}-%{version}

# List of multimedia modules
# Keep in sync with make command args below
%define media_modules "dvb-core lgdt330x zl10353 tvp5150 xc2028 rc-core rc-pinnacle-pctv-hd em28xx em28xx-alsa em28xx-dvb em28xx-rc em28xx-v4l hdpvr v4l2-fwnode v4l2-async"

cat /dev/null > kmod-%{kmod_name}.conf
for modules in `echo -n %{media_modules}`
do
	echo "override $modules * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
done

# Fix Makefile(s) to use relative $(src) instead of $(srctree)
# since local headers are not in kernel-devel
for MAKEFILE in */Makefile
do
	sed -i -e 's|-I.*$(srctree)\/drivers\/media|-I $(src)/..|' $MAKEFILE
done
for MAKEFILE in */*/Makefile
do
	sed -i -e 's|-I.*$(srctree)\/drivers\/media|-I $(src)/../..|' $MAKEFILE
done
for MAKEFILE in */*/*/Makefile
do
	sed -i -e 's|-I.*$(srctree)\/drivers\/media|-I $(src)/../../..|' $MAKEFILE
done

%build
# The EXTRA_CFLAGS is required for any drivers that use ifdef or IS_REACHABLE macros
%{__make} -C %{kernel_source} %{?_smp_mflags} V=1 modules M=$PWD \
	CONFIG_MEDIA_DIGITAL_TV_SUPPORT=y \
	CONFIG_DVB_CORE=m \
	CONFIG_DVB_LGDT330X=m \
	CONFIG_DVB_ZL10353=m \
	CONFIG_VIDEO_TVP5150=m \
	CONFIG_MEDIA_TUNER=m \
	CONFIG_MEDIA_TUNER_XC2028=m \
	CONFIG_RC_CORE=m \
	CONFIG_RC_MAP=m \
	CONFIG_MEDIA_CEC_RC=y \
	CONFIG_VIDEO_EM28XX=m \
	CONFIG_VIDEO_EM28XX_ALSA=m \
	CONFIG_VIDEO_EM28XX_DVB=m \
	CONFIG_VIDEO_EM28XX_RC=m \
	CONFIG_VIDEO_EM28XX_V4L2=m \
	CONFIG_VIDEO_HDPVR=m \
	CONFIG_V4L2_FWNODE=m \
	CONFIG_V4L2_ASYNC=m \
	EXTRA_CFLAGS='-DCONFIG_DVB_LGDT330X -DCONFIG_DVB_ZL10353 -DCONFIG_VIDEO_TVP5150 -DCONFIG_MEDIA_TUNER_XC2028 -DCONFIG_RC_CORE -DCONFIG_RC_MAP -DCONFIG_MEDIA_CEC_RC'

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
for modules in `echo -n %{media_modules}`
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
        kvers=$(ls -d "/lib/modules/${kver_base%%.*}"*)

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
%config /etc/depmod.d/kmod-%{kmod_name}.conf
%doc /usr/share/doc/kmod-%{kmod_name}-%{version}/

%changelog
* Tue Oct 07 2025 Tuan Hoang <tqhoang@elrepo.org> - 0.0-4
- Add support for module hdpvr
- Add provides/obsoletes for module hdpvr
- Change xc3028-firmware package from required to recommends

* Fri Sep 26 2025 Tuan Hoang <tqhoang@elrepo.org> - 0.0-3
- Improve makefile modifications
- Properly support 2304:0227 Pinnacle Systems, Inc. PCTV for Mac, HD Stick

* Tue Sep 23 2025 Tuan Hoang <tqhoang@elrepo.org> - 0.0-2
- Rebase source from RHEL 9.6 errata kernel 5.14.0-570.42.2.el9_6
- Rebuilt against RHEL 9.6 errata kernel 5.14.0-570.42.2.el9_6
- Removed tveeprom module since it is now in RHEL

* Tue Sep 23 2025 Tuan Hoang <tqhoang@elrepo.org> - 0.0-1
- Initial build for RHEL 9
  https://elrepo.org/bugs/view.php?id=1553
- Source from RHEL 9.6 GA kernel
