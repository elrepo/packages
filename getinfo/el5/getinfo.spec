Summary: Package CentOS 'getinfo.sh' script as an rpm
Name: getinfo
Version: 1.0
Release: 1%{?dist}
License: GPL
Group: Applications/System
URL: http://www.centos.org/

Packager: Phil Schaffner <pschaff2@verizon.net>
Vendor: ELRepo

Source0: http://github.com/elrepo/packages/tree/master/getinfo/el5/getinfo.sh
Source1: http://github.com/elrepo/packages/tree/master/getinfo/el5/GPL-v2.0.txt
Source2: http://github.com/elrepo/packages/tree/master/getinfo/el5/ReadMe.txt

BuildArch: noarch
#BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
Bash script to supply system information for use on support fora.

%install
%{__rm} -rf %{buildroot}
%{__mkdir_p} %{buildroot}/usr/local/bin
%{__mkdir_p} %{buildroot}/usr/local/share/doc/%{name}-%{version}/
%{__install} -D -m 755 %{SOURCE0} %{buildroot}/usr/local/bin/
%{__install} -D -m 644 %{SOURCE1} %{buildroot}/usr/local/share/doc/%{name}-%{version}/
%{__install} -D -m 644 %{SOURCE2} %{buildroot}/usr/local/share/doc/%{name}-%{version}/

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%attr(755,root,root) %{_usr}/local/bin/getinfo.sh
%dir %{_usr}/local/share/doc/%{name}-%{version}
%doc %{_usr}/local/share/doc/%{name}-%{version}/GPL-v2.0.txt
%doc %{_usr}/local/share/doc/%{name}-%{version}/ReadMe.txt


%changelog
* Thu Apr 05 2012 Phil Schaffner <pschaff2@verizon.net> - 1.0-1
- Package for ELRepo

* Thu Apr 05 2012 Trevor Hemsley <trevor.hemsley@ntlworld.com> - 1.0-0
- Initial spec.
