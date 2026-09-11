%define commit_hash		90261ae2934329f6ca84dd6c72d10d0777bf4b0e
%define short_commit_hash	%(c=%{commit_hash}; echo ${c:0:7})
%define commit_date		20250629

%define base_name		dvb-firmware 

# RHEL 10 has existing package
%if %{?rhel} >= 10
%define base_name_suffix	-extra
%endif

Summary:	Firmware for various DVB broadcast receivers
Name:		%{base_name}%{?base_name_suffix}
Version:	%{commit_date}
Release:	1%{?dist}
License:	Redistributable, no modification permitted
URL:		https://github.com/LibreELEC/dvb-firmware

Source0:	https://github.com/LibreELEC/dvb-firmware/archive/%{commit_hash}/%{base_name}-%{short_commit_hash}.tar.gz

%if %{?_with_src:0}%{!?_with_src:1}
NoSource:	0
%endif

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build

BuildArch:	noarch

BuildRequires:	linux-firmware
Recommends:	linux-firmware

%if %{?rhel} >= 10
BuildRequires:	dvb-firmware
Recommends:	dvb-firmware
%endif

# Overrides existing ELRepo package
Obsoletes:	xc3028-firmware < %{version}
Provides:	xc3028-firmware = %{version}

# Conflicts existing RPM Fusion package
Conflicts:	dvb-firmware-nonfree

%description
Firmware for various DVB broadcast receivers.

%prep
%autosetup -p1 -n %{base_name}-%{commit_hash}

%build

%install
%{__mkdir} -p %{buildroot}/lib/firmware
%{__cp} -pr firmware/* %{buildroot}/lib/firmware

# Change to buildroot lib directory because 'find' command
# below will not work with dot '.'
pushd %{buildroot}/lib

# Remove firmware that conflict with existing linux-firmware
for FW in $(find firmware ! -type d) ; do
    if [ -e /lib/${FW}.xz ] || [ -e /lib/${FW} ] ; then
        echo "Removing ${FW}"
        %{__rm} -f ${FW}
    fi
done

cd firmware

# Remove empty directories
find . -empty -type d -delete

# Remove text files
%{__rm} -fv *.txt *LICENCE* *LICENSE* *README*

# Compress all remaining firmware files
# https://www.kernel.org/doc/html/latest/staging/xz.html
find . -type f -exec %{__xz} --check=crc32 --force {} \+

popd

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc README.md firmware/*README*
%license firmware/{*.txt,*LICENCE*,*LICENSE*}
/lib/firmware/*

%changelog
* Wed Sep 09 2026 Tuan Hoang <tqhoang@elrepo.org> - 20250629-1
- Initial package for RHEL9.
- Based on RPM Fusion dvb-firmware
  [https://github.com/rpmfusion/dvb-firmware]
