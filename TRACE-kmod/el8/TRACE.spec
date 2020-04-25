#!/bin/sh
# Define the kmod package name here.
%define kmod_name TRACE

# If you want to use another kernel version, run this command:
#  rpmbuild -ba --define 'kversion 3.10.0-957.5.1.el7.x86_64' mykmod.spec
# Default is current running kernel
# %%{!?kversion: %%define kversion %%(uname -r | sed -e "s/\.`uname -p`//").%%{_target_cpu}}

# If you want a custom version:
# rpmbuild -ba --define 'trace_version myversion'
# If you want a custom revision:
# rpmbuild -ba --define 'trace_revision myrevision'

###########################################################
###########################################################

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 4.18.0-147.el8}

%{!?dist: %define dist .el8}

Name:    kmod-%{kmod_name}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     https://cdcvs.fnal.gov/redmine/projects/trace
Packager:	Fermilab Real-Time Software Infrastructure
# Sources
Source0:  %{kmod_name}.tar.bz2
# Source10: kmodtool-%%{kmod_name}.sh

%define findpat %( echo "%""P" )
%define __find_requires /usr/lib/rpm/redhat/find-requires.ksyms
%define __find_provides /usr/lib/rpm/redhat/find-provides.ksyms %{kmod_name} %{?epoch:%{epoch}:}%{version}-%{release}
%define dup_state_dir %{_localstatedir}/lib/rpm-state/kmod-dups
%define kver_state_dir %{dup_state_dir}/kver
%define kver_state_file %{kver_state_dir}/%{kmod_kernel_version}.%{_arch}
%define dup_module_list %{dup_state_dir}/rpm-kmod-%{kmod_name}-modules

%global _use_internal_dependency_generator 0
%global kernel_source() %{_usrsrc}/kernels/%{kmod_kernel_version}.%{_arch}

# Determine the Version from source if not specified
### untar the source, ask and remove the untar'd copy
### source should be in a format of [0-9].[0-9][0-9].[0-9][0-9] i.e. v3_15_03 or 3.15.03
%if "x%{?trace_version}" == "x"
%define trace_version %(mkdir -p %{_builddir}/%{kmod_name}; cd  %{_builddir}/%{kmod_name} ; tar xf %{SOURCE0} ;\
                        grep 'TRACE VERSION' CMakeLists.txt 2>/dev/null | egrep -o 'v*[0-9]([^0-9][0-9][0-9]*)*'; rm -rf %{_builddir}/%{kmod_name}) 
%endif

# Determine the svn revision from source if not specified
### untar the source, ask and remove the untar'd copy
# If I need a revision w/o svn: find . -name \*.[chs]* -o -name \*ake\* -o -name \*.spec | egrep -v '/.svn|~' | xargs grep -o 'Revision: [0-9]*' | awk '{print$2}' | sort -n | tail -1
%if "x%{?trace_revision}" == "x"
%define trace_revision r%(mkdir -p %{_builddir}/%{kmod_name}; cd  %{_builddir}/%{kmod_name} ; tar xf %{SOURCE0} ;\
				find . -name '*.[chs]*' -o -name '*ake*' -o -name '*.spec' | egrep -v '/.svn|~' | xargs grep -o 'Revision: [0-9]*' | awk '{print$2}' | sort -n | tail -1;\
				rm -rf %{_builddir}/%{kmod_name})
%endif

Version: %{trace_version}
# Add the ".1" as a place where you can increment a value to resolve a packaging issue
Release: r1309.1%{?dist}

BuildRequires:  elfutils-libelf-devel
BuildRequires:  kernel-devel = %{kmod_kernel_version}
BuildRequires:  kernel-abi-whitelists
BuildRequires:  kernel-rpm-macros
BuildRequires:  redhat-rpm-config

ExclusiveArch:  x86_64

Provides:       kernel-modules >= %{kmod_kernel_version}.%{_arch}
Provides:       kmod-%{kmod_name} = %{?epoch:%{epoch}:}%{version}-%{release}

Requires(post): %{_sbindir}/weak-modules
Requires(postun):       %{_sbindir}/weak-modules
Requires:       kernel >= %{kmod_kernel_version}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).

It controls tracing via environment variables and dynamically from
outside program.

It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

###########################################################
%package -n %{kmod_name}-utils
Summary: Utilities for %{kmod_name} kmod
Requires: %{kmod_name}-kmod
Requires: perl, bash

%description -n %{kmod_name}-utils
Utilities for %{kmod_name} kmod

%files -n %{kmod_name}-utils
%defattr(0644,root,root,0755)
%doc %{_mandir}/man1/*
%doc %{_mandir}/man1p/*
%doc %{_defaultdocdir}/%{kmod_name}-utils-%{version}/*
%attr(0755,root,root) %{_bindir}/trace_addr2line
%attr(0755,root,root) %{_bindir}/trace_cntl
%attr(0755,root,root) %{_bindir}/trace_delta
%attr(0755,root,root) %{_bindir}/bitN_to_mask
%attr(0755,root,root) %{_bindir}/trace_envvars
%{_sysconfdir}/profile.d/trace.sh
%{_includedir}/TRACE/*

###########################################################
%prep
# Prep kernel module
%setup -q -c -n %{kmod_name}

%{__mkdir_p} build
## Write ABI tracking file
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > build/kmod-%{kmod_name}.conf

###########################################################
%build
%{__mkdir_p} build
# Build all (TRACE packages its own implementation of module-build)
#  when the distro has gcc 4.9+, (and std is less than c11) then add XTRA_CFLAGS=-std=c11
%{__make} OUT=${PWD}/build XTRA_CFLAGS=-D_GNU_SOURCE XTRA_CXXFLAGS=-std=c++11 all KDIR=%{_usrsrc}/kernels/%{kmod_kernel_version}.%{_arch}

whitelist="/lib/modules/kabi-current/kabi_whitelist_%{_target_cpu}"
for modules in $( find . -name "*.ko" -type f -printf "%{findpat}\n" | sed 's|\.ko$||' | sort -u ) ; do
        # update greylist
        nm -u ./$modules.ko | sed 's/.*U //' |  sed 's/^\.//' | sort -u | while read -r symbol; do
                grep -q "^\s*$symbol\$" $whitelist || echo "$symbol" >> ./greylist
        done
done
sort -u greylist | uniq > greylist.txt

###########################################################
%install
rm -rf %{buildroot}
# Install kernel module
%{__install} -d %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} build/module/%{kmod_kernel_version}.%{_arch}/%{kmod_name}.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -m644 build/kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -m 0644 greylist.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

## Strip the modules(s).
find %{buildroot} -type f -name \*.ko -exec %{__strip} --strip-debug \{\} \;

## Sign the modules(s).
%if %{?_with_modsign:1}%{!?_with_modsign:0}
## If the module signing keys are not defined, define them here.
%{!?privkey: %define privkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.priv}
%{!?pubkey: %define pubkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.der}
for module in $(find %{buildroot} -type f -name \*.ko);
do /usr/src/kernels/%{kmod_kernel_version}.%{_arch}/scripts/sign-file \
    sha256 %{privkey} %{pubkey} $module;
done
%endif

# Install headers
%{__install} -d %{buildroot}%{_includedir}/TRACE
%{__install} -m644 include/trace.h %{buildroot}%{_includedir}/TRACE/trace.h
%{__install} -m644 include/tracemf.h %{buildroot}%{_includedir}/TRACE/tracemf.h

# Install shell profile
%{__install} -d %{buildroot}%{_sysconfdir}/profile.d/
%{__install} script/trace_functions.sh %{buildroot}%{_sysconfdir}/profile.d/trace.sh

# Install utils
%{__install} -d %{buildroot}%{_bindir}
%{__install} %(echo build/Linux*/bin/trace_cntl) %{buildroot}%{_bindir}/trace_cntl
%{__install} script/trace_addr2line %{buildroot}%{_bindir}/trace_addr2line
%{__install} script/trace_delta %{buildroot}%{_bindir}/trace_delta
%{__install} script/bitN_to_mask %{buildroot}%{_bindir}/bitN_to_mask
%{__install} script/trace_envvars %{buildroot}%{_bindir}/trace_envvars

# Install manpages
%{__install} -d %{buildroot}%{_mandir}/man1
%{__install} doc/t*.1 %{buildroot}%{_mandir}/man1/
%{__install} -d %{buildroot}%{_mandir}/man1p
%{__install} doc/t*.1p %{buildroot}%{_mandir}/man1p/

# Install kmod docs
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -m644 doc/users_guide.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/example_module
%{__cp} -r src_example/module %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/example_module/
%{__cp} -r src_example/module1 %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/example_module/

# Install util docs
%{__install} -d %{buildroot}%{_defaultdocdir}/%{kmod_name}-utils-%{version}/
%{__install} -d %{buildroot}%{_defaultdocdir}/%{kmod_name}-utils-%{version}/example_util
%{__cp} -r src_example/userspace %{buildroot}%{_defaultdocdir}/%{kmod_name}-utils-%{version}/example_util


###########################################################
%clean
%{__rm} -rf %{buildroot}

###########################################################

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
		if [ -e "/boot/symvers-$k.gz" ]; then
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
* Fri Apr 24 2020 Akemi Yagi <toracat@elrepo.org> - 3.15.09-r1309.1
- Updated to r1309

* Sat Apr 11 2020 Akemi Yagi <toracat@elrepo.org> - 3.15.07-r1293.1
- Updated to r1293

* Wed Mar 18 2020 Akemi Yagi <toracat@elrepo.org> - 3.15.07-r1273.1
- Initial build for RHEL 8
- Submitted by Pat Riehecky (elrepo bug #996)
