Summary: Packaged 'getinfo.sh' script for RPM based distros
Name: getinfo
Version: 1.1
Release: 1%{?dist}
License: GPLv2
Group: Applications/System
URL: https://github.com/elrepo/packages/tree/master/getinfo/el6

Source0: https://raw.github.com/elrepo/packages/master/getinfo/el6/getinfo.sh
Source1: https://raw.github.com/elrepo/packages/master/getinfo/el6/GPL-v2.0.txt
Source2: https://raw.github.com/elrepo/packages/master/getinfo/el6/ReadMe.txt

BuildArch: noarch
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)

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
* Mon Apr 23 2012 Phil Schaffner <pschaff2@verizon.net> - 1.1-1
- Enhancements to network information
- Add error checking for missing commands
- Arguments now require only 3 characters and are case-insensitive

* Sat Apr 7 2012 Phil Schaffner <pschaff2@verizon.net> - 1.0-1
- Package for ELRepo for el6

* Thu Apr 05 2012 Trevor Hemsley <trevor.hemsley@ntlworld.com> - 1.0-0
- Initial spec.
