# $Id$
# Authority: dag

%{!?kversion:%define kversion 2.6.18-8.el5}

# Define the kmod package name here.
%define kmod_name lin_tape

Summary: IBM Tape SCSI Device Driver for Linux
Name: lin_tape-kmod
Version: 1.41.1
Release: 1%{?dist}
License: GPL
Group: System Environment/Kernel
URL: ftp://ftp.software.ibm.com/storage/devdrvr/Linux/archive/lin_tape_source-lin_taped/

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
ExclusiveArch: i686 x86_64

# Sources.
Source0: http://delivery04.dhe.ibm.com/sar/CMA/STA/010nr/0/lin_tape-1.41.1-1.src.rpm
#Source0: ftp://ftp.software.ibm.com/storage/devdrvr/Linux/archive/lin_tape_source-lin_taped/lin_tape/lin_tape-%{version}-1.src.rpm.bin
Source1: lin_tape-depmod.conf
Source2: lin_tape-98-lin_tape.permissions
Source10: kmodtool-%{kmod_name}

# Define the variants for each architecture.
%define basevar ""
%ifarch i686
%define paevar PAE
%endif
%ifarch i686 x86_64
%define xenvar xen
%endif

# If kvariants isn't defined on the rpmbuild line, build all variants for this architecture.
%{!?kvariants: %define kvariants %{?basevar} %{?xenvar} %{?paevar}}

# Magic hidden here.
%define kmodtool sh %{SOURCE10}
%{expand:%(%{kmodtool} rpmtemplate_kmp %{kmod_name} %{kversion} %{kvariants} 2>/dev/null)}

%description
This package provides the The IBM Tape Device Driver driver module for
IBM TotalStorage and SystemStorage tape devices.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -c -T
rpm2cpio %{SOURCE0} | cpio -i -d --verbose
tar -xvzf lin_tape-%{version}.tgz
for kvariant in %{kvariants} ; do
    %{__cp} -a %{kmod_name}-%{version} _kmod_build_$kvariant
    %{__cat} <<-EOF >_kmod_build_$kvariant/%{kmod_name}.conf
override %{kmod_name} * weak-updates/%{kmod_name}
EOF
done

### Move documentation in place
(cd %{kmod_name}-%{version}; %{__cp} -av COPYING* lin_tape.fixlist lin_tape_*.ReadMe messages ..)

%build
for kvariant in %{kvariants} ; do
    ksrc="%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}"
    pushd _kmod_build_$kvariant
    %{__make} -C "${ksrc}" modules M="$PWD"
    popd
done

%install
export INSTALL_MOD_PATH="%{buildroot}"
export INSTALL_MOD_DIR="extra/%{kmod_name}"
for kvariant in %{kvariants} ; do
    ksrc="%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}"
    pushd _kmod_build_$kvariant
    %{__make} -C "${ksrc}" modules_install M="$PWD"
    %{__install} -Dp -m0644 %{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/%{kmod_name}.conf
    popd
done
# Strip the module(s).
find %{buildroot} -type f -name \*.ko -exec strip --strip-debug \{\} \;

### The udev stuff was removed somewhere after 1.34.0 (no longer necessary?)
#%{__install} -Dp -m0644 %{kmod_name}-%{version}/98-lin_tape.rules %{buildroot}%{_sysconfdir}/udev/rules/98-lin_tape.rules
#%{__install} -Dp -m0755 %{kmod_name}-%{version}/udev.get_lin_tape_id.sh %{buildroot}%{_sbindir}/udev.get_lin_tape_id.sh
%{__install} -Dp -m0644 %{SOURCE2} %{buildroot}%{_sysconfdir}/udev/permissions.d/98-lin_tape.permissions

%clean
%{__rm} -rf %{buildroot}

%changelog
* Thu Aug 12 2010 Dag Wieërs <dag@wieers.com> - 1.41.1-1
- Updated to release 1.41.1.

* Fri Jun 18 2010 Dag Wieërs <dag@wieers.com> - 1.34.0-1
- Updated to release 1.34.0.

* Tue Jan 12 2010 Philip J Perry <phil@elrepo.org> - 1.29.0-1
- Initial build of the kmod package.
