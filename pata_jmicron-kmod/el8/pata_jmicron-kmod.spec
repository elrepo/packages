# Define the kmod package name here.
%define kmod_name		pata_jmicron

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 4.18.0-553.el8_10}

%{!?dist: %define dist .el8}

Name:           kmod-%{kmod_name}
Version:        0.1.5
Release:        6%{?dist}
Summary:        %{kmod_name} kernel module(s)
Group:          System Environment/Kernel
License:        GPLv2
URL:            http://www.kernel.org/

# Sources.
Source0:  drivers-ata.tar.gz
Source5:  GPL-v2.0.txt
Source7:  scsi_transport_api.h

%define __spec_install_post	/usr/lib/rpm/check-buildroot \
				/usr/lib/rpm/redhat/brp-ldconfig \
				/usr/lib/rpm/brp-compress \
				/usr/lib/rpm/brp-strip-comment-note /usr/bin/strip /usr/bin/objdump \
				/usr/lib/rpm/brp-strip-static-archive /usr/bin/strip \
				/usr/lib/rpm/brp-python-bytecompile "" 1 \
				/usr/lib/rpm/brp-python-hardlink \
				PYTHON3="/usr/libexec/platform-python" /usr/lib/rpm/redhat/brp-mangle-shebangs

# Source code patches
Patch0:  elrepo-libata-eh.patch
Patch1:  elrepo-libata.patch

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

BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

ExclusiveArch:  x86_64

BuildRequires:  elfutils-libelf-devel
BuildRequires:  kernel-devel = %{kmod_kernel_version}
BuildRequires:  kernel-abi-whitelists
BuildRequires:  kernel-rpm-macros
BuildRequires:  redhat-rpm-config

Provides:       kernel-modules >= %{kmod_kernel_version}.%{_arch}
Provides:       kmod-%{kmod_name} = %{?epoch:%{epoch}:}%{version}-%{release}

Requires(post): %{_sbindir}/weak-modules
Requires(postun):       %{_sbindir}/weak-modules
Requires:       kernel >= %{kmod_kernel_version}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.


%prep
%setup -q -n drivers-ata
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%{__cp} %{SOURCE7} .

# Apply patch(es)
%patch0 -p1
%patch1 -p1

%build
%{__make} -C %{kernel_source} %{?_smp_mflags} modules M=$PWD CONFIG_PATA_JMICRON=m

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
%{__install} %{kmod_name}.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
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
        kver_base="%{kmod_kernel_version}"
        kvers=$(ls -d "/lib/modules/${kver_base%%.*}"*)

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
%config /etc/depmod.d/kmod-%{kmod_name}.conf
%doc /usr/share/doc/kmod-%{kmod_name}-%{version}/

%changelog
* Fri May 24 2024 Tuan Hoang <tqhoang@elrepo.org> - 0.1.5-6
- Rebuilt against RHEL 8.10 GA kernel 4.18.0-553.el8_10
- ATA source code is unchanged in 8.10

* Sun Nov 19 2023 Tuan Hoang <tqhoang@elrepo.org> - 0.1.5-5
- Rebuilt against RHEL 8.9 GA kernel
- Source code from kernel-4.18.0-513.5.1.el8_9

* Mon Aug 07 2023 Akemi Yagi <toracat@elrepo.org> - 0.1.5-4
- Rebuilt against RHEL 8.8 GA kernel
- Source code from kernel-4.18.0-477.10.1

* Tue May 10 2022 Akemi Yagi <toracat@elrepo.org> - 0.1.5-3
- Rebuilt against RHEL 8.6 GA kernel 4.18.0-372.9.1.el8
- Source code from kernel-4.18.0-372.9.1
- Fix SB-signing issue caused by /usr/lib/rpm/brp-strip
  [https://bugzilla.redhat.com/show_bug.cgi?id=1967291]

* Tue May 18 2021 Philip J Perry <phil@elrepo.org> - 0.1.5-2
- Rebuilt against RHEL 8.4 kernel
- Source code from kernel-4.18.0-305
- Fix updating of initramfs image
  [https://elrepo.org/bugs/view.php?id=1060]
- Revert addition of dracut conf file

* Sat Jan 23 2020 Akemi Yagi <toracat@elrepo.org> - 0.1.5-1
- Add dracut conf file to ensure module is in initramfs
- Built against RHEL 8.3 kernel
- Source code from kernel-4.18.0-240
