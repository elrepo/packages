%define kmod_name		e1000e
%define kmod_driver_version	3.4.2.1
%define kmod_rpm_release	1
%define kmod_kernel_version	4.18.0-32.el8
%define kmod_headers_version    %(rpm -qa kernel-devel | sed 's/^kernel-devel-//')
%define kmod_kbuild_dir         .
%define kmod_dependencies       %{nil}
%define kmod_build_dependencies	%{nil}
%define kmod_devel_package	0

%{!?dist: %define dist .el8}
%{!?make_build: %define make_build make}

Source0:	%{kmod_name}-%{kmod_driver_version}.tar.gz
# Source code patches

%define findpat %( echo "%""P" )

Name:		kmod-e1000e
Version:	%{kmod_driver_version}
Release:	%{kmod_rpm_release}%{?dist}
Summary:	%{kmod_name} kernel module(s)
License:	GPLv2
URL:		http://www.intel.com/
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildRequires:  elfutils-libelf-devel
BuildRequires:  glibc
BuildRequires:  kernel-devel >= %{kmod_kernel_version}

BuildRequires:  libuuid-devel
BuildRequires:  redhat-rpm-config
ExclusiveArch:	x86_64

%global kernel_source() /usr/src/kernels/%{kmod_headers_version}

%global _use_internal_dependency_generator 0
Provides:	kernel-modules = %kmod_kernel_version.%{_target_cpu}
Provides:	kmod-%{kmod_name} = %{?epoch:%{epoch}:}%{version}-%{release}
Requires(post):	%{_sbindir}/weak-modules
Requires(postun):	%{_sbindir}/weak-modules
Requires:       kernel >= %{kmod_kernel_version}

%if "%{kmod_build_dependencies}" != ""
BuildRequires:  %{kmod_build_dependencies}
%endif
%if "%{kmod_dependencies}" != ""
Requires:       %{kmod_dependencies}
%endif

%description
This package provides the %{kmod_name} kernel module(s) for the
Intel® 82563/6/7, 82571/2/3/4/7/8/9 and 82583 PCI-E based Ethernet NICs.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%post
modules=( $(find /lib/modules/%{kmod_headers_version}/extra/kmod-%{kmod_name} | grep '\.ko$') )
printf '%s\n' "${modules[@]}" >> /var/lib/rpm-kmod-posttrans-weak-modules-add
### printf '%s\n' "${modules[@]}" | %{sbindir}/weak-modules --add-modules --no-initramfs

%pretrans -p <lua>
posix.unlink("/var/lib/rpm-kmod-posttrans-weak-modules-add")

%posttrans
if [ -f "/var/lib/rpm-kmod-posttrans-weak-modules-add" ]; then
        modules=( $(cat /var/lib/rpm-kmod-posttrans-weak-modules-add) )
        rm -rf /var/lib/rpm-kmod-posttrans-weak-modules-add
        printf '%s\n' "${modules[@]}" | %{_sbindir}/weak-modules --dracut=/usr/bin/dracut --add-modules
fi

%preun
rpm -ql kmod-%{kmod_name}-%{kmod_driver_version}-%{kmod_rpm_release}%{?dist}.$(arch) | grep '\.ko$' > /var/run/rpm-kmod-%{kmod_name}-modules
# Check whether module is loaded, and if so attempt to remove it.  A
# failure to unload means there is still something using the module.  To make
# sure the user is aware, we print a warning with recommended instructions.
for module in %{kmod_name}; do
  if grep -q "^${module}" /proc/modules; then
    warnMessage="WARNING: ${module} in use.  Changes will take effect after a reboot."
    modprobe -r ${module} 2>/dev/null || echo ${warnMessage} && /usr/bin/true
  fi
done

%postun
modules=( $(cat /var/run/rpm-kmod-%{kmod_name}-modules) )
rm /var/run/rpm-kmod-%{kmod_name}-modules
printf '%s\n' "${modules[@]}" | %{_sbindir}/weak-modules --dracut=/usr/bin/dracut --remove-modules

%files
%defattr(644,root,root,755)
/lib/modules/%{kmod_headers_version}
/etc/depmod.d/%{kmod_name}.conf
/usr/share/doc/kmod-%{kmod_name}/greylist.txt

%prep
%setup -n %{kmod_name}-%{kmod_driver_version}

# % patch0 -p1
### set -- *
### mkdir source
### mv "$@" source/
### mkdir obj

%build
### rm -rf obj
### cp -r source obj
### % {make_build} -C %{kernel_source} V=1 M=$PWD/obj/%{kmod_kbuild_dir} \
### 	NOSTDINC_FLAGS="-I $PWD/obj/include -I $PWD/obj/include/uapi" \
### 	EXTRA_CFLAGS="-mindirect-branch=thunk-inline -mindirect-branch-register" \
### 	%{nil}

pushd src >/dev/null
%{make_build} -C %{kernel_source} V=1 M=$PWD \
EXTRA_CFLAGS="-mindirect-branch=thunk-inline -mindirect-branch-register" \
%{nil}
popd >/dev/null

# mark modules executable so that strip-to-file can strip them
### find obj/%{kmod_kbuild_dir} -name "*.ko" -type f -exec chmod u+x '{}' +
find . -name "*.ko" -type f -exec chmod u+x '{}' +

whitelist="/lib/modules/kabi-current/kabi_whitelist_%{_target_cpu}"

for modules in $( find . -name "*.ko" -type f -printf "%{findpat}\n" | sed 's|\.ko$||' | sort -u ) ; do
	# update depmod.conf
	module_weak_path=$(echo $modules | sed 's/[\/]*[^\/]*$//')
	if [ -z "$module_weak_path" ]; then
		module_weak_path=%{name}
	else
		module_weak_path=%{name}/$module_weak_path
	fi
	echo "override $(echo $modules | sed 's/.*\///') * weak-updates/$module_weak_path" >> depmod.conf

	# update greylist
	nm -u ./$modules.ko | sed 's/.*U //' |  sed 's/^\.//' | sort -u | while read -r symbol; do
		grep -q "^\s*$symbol\$" $whitelist || echo "$symbol" >> ./greylist
	done
done
sort -u greylist | uniq > greylist.txt

%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT
export INSTALL_MOD_DIR=extra/%{name}
pushd src >/dev/null
make -C %{kernel_source} modules_install \
	M=$PWD
popd >/dev/null
# Cleanup unnecessary kernel-generated module dependency files.
find $INSTALL_MOD_PATH/lib/modules -iname 'modules.*' -exec rm {} \;

install -m 644 -D depmod.conf $RPM_BUILD_ROOT/etc/depmod.d/%{kmod_name}.conf
install -m 644 -D greylist.txt $RPM_BUILD_ROOT/usr/share/doc/kmod-%{kmod_name}/greylist.txt

%clean
rm -rf $RPM_BUILD_ROOT

%changelog
* Sun Dec 23 2018 Akemi Yagi <toracat@elrepo.org> 3.4.2.1
- Initial build for RHEL 8.0
- Updated to version 3.4.2

* Thu Mar 15 2018 Eugene Syromiatnikov <esyr@redhat.com> 2.1.14_k_dup7.4-2.1
- Added modinfo flag for retpoline.
- Resolves: #bz1549985

* Thu Mar 01 2018 Eugene Syromiatnikov <esyr@redhat.com> 3.0.1_k_dup7.4-2
- Rebuilt with -mindirect-branch=thunk-inline -mindirect-branch-register flags.

* Tue Feb 27 2018 Eugene Syromiatnikov <esyr@redhat.com> 3.0.1_k_dup7.4-1
- 141332170ff595319ce42a2cfc2944c2cbb11b4f
- i40evf module for Driver Update Program
