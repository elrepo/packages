# Define the kmod package name here.
%define kmod_name	mptsas

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 5.14.0-611.5.1.el9_7}

%{!?dist: %define dist .el9}

Name:		kmod-%{kmod_name}
Version:	3.04.20
Release:	12%{?dist}
Summary:	%{kmod_name} kernel module(s)
Group:		System Environment/Kernel
License:	GPLv2
URL:		http://www.kernel.org/

# Sources.
Source0:	%{kmod_name}-%{version}.tar.gz
Source5:	GPL-v2.0.txt

# Source code patches
Patch0:		elrepo-mptsas-rhel_differences.el9_4.patch
Patch1:		elrepo-mptspi-rhel_differences.el9_4.patch

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
# Combines and replaces kmod-mptfc and kmod-mptspi
Provides:			kmod-mptfc = %{?epoch:%{epoch}:}%{version}-%{release}
Provides:			kmod-mptspi = %{?epoch:%{epoch}:}%{version}-%{release}
Obsoletes:			kmod-mptfc < %{?epoch:%{epoch}:}%{version}-%{release}
Obsoletes:			kmod-mptspi < %{?epoch:%{epoch}:}%{version}-%{release}

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
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "override mptctl * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override mptfc * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override mptspi * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf

# Apply patch(es)
%patch0 -p0
%patch1 -p0

%build
%{__make} -C %{kernel_source} %{?_smp_mflags} V=1 modules M=$PWD \
	CONFIG_FUSION_SPI=m \
	CONFIG_FUSION_FC=m \
	CONFIG_FUSION_SAS=m \
	CONFIG_FUSION_MAX_SGE=128 \
	CONFIG_FUSION_CTL=m \
	EXTRA_CFLAGS+='-DCONFIG_FUSION_SPI' \
	EXTRA_CFLAGS+='-DCONFIG_FUSION_FC' \
	EXTRA_CFLAGS+='-DCONFIG_FUSION_SAS' \
	EXTRA_CFLAGS+='-DCONFIG_FUSION_MAX_SGE=128' \
	EXTRA_CFLAGS+='-DCONFIG_FUSION_CTL' \


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
%{__install} %{kmod_name}.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} mptctl.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} mptfc.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} mptspi.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
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
%doc %{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

%changelog
* Fri Jan 02 2026 Tuan Hoang <tqhoang@elrepo.org> - 3.04.20-12
- Fix problems in posttrans section
- Fix macro usage in files section

* Mon Nov 17 2025 Tuan Hoang <tqhoang@elrepo.org> - 3.04.20-11
- Rebuilt against RHEL 9.7 GA kernel
- Source code unchanged in 9.7 GA kernel
- Fix hard-coded arch in post section

* Wed May 14 2025 Tuan Hoang <tqhoang@elrepo.org> - 3.04.20-10
- Rebuilt against RHEL 9.6 GA kernel
- Source code from kernel-5.14.0-570.12.1.el9_6

* Tue Nov 12 2024 Philip J Perry <phil@elrepo.org> - 3.04.20-9
- Rebuilt for RHEL 9.5
- Source updated from RHEL 9.5 GA kernel

* Tue Apr 30 2024 Akemi Yagi <toracat@elrepo.org> - 3.04.20-8
- Source updated from RHEL 9.4 kernel
- Rebuilt for RHEL 9.4

* Sat Jan 27 2024 Philip J Perry <phil@elrepo.org> - 3.04.20-7
- Rebuilt against kernel 5.14.0-362.18.1.el9_3

* Sat Dec 16 2023 Tuan Hoang <tqhoang@elrepo.org> - 3.04.20-6
- Rebuilt against RHEL 9.3 errata kernel 5.14.0-362.13.1.el9_3

* Tue Nov 07 2023 Philip J Perry <phil@elrepo.org> - 3.04.20-5
- Rebuilt for RHEL 9.3
- Source updated from RHEL 9.3 kernel

* Tue May 09 2023 Philip J Perry <phil@elrepo.org> - 3.04.20-4
- Rebuilt for RHEL 9.2 
- Source updated from RHEL 9.2 kernel

* Sat Jan 14 2023 Philip J Perry <phil@elrepo.org> - 3.04.20-3
- Added mptctl module [https://elrepo.org/bugs/view.php?id=1315]

* Tue Nov 15 2022 Philip J Perry <phil@elrepo.org> - 3.04.20-2
- Rebuilt for RHEL 9.1
- Source updated from RHEL 9.1 kernel
- Obsoletes kmod-mptfc and kmod-mptspi

* Tue May 17 2022 Philip J Perry <phil@elrepo.org> - 3.04.20-1
- Initial build for RHEL 9
- Backported from kernel-5.14.0-70.13.1.el9_0
