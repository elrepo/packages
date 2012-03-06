Name: bumblebee		
Version: 3.0	
Release: 2%{?dist}
Summary: Bumblebee is a project that enables Linux to utilize the Nvidia Optimus Hybrid cards.
Group: System Environment/Daemons		
License: GPLv3	
URL: https://github.com/Bumblebee-Project		

BuildRequires: libbsd pkgconfig autoconf
Requires: libbsd 	

# Sources
Source0: %{name}-%{version}.tar.gz	
Source1: bumblebeed
Source5: GPL-v3.0.txt

%description
The %{name} package is a project that enables Linux to utilize 
the Nvidia Optimus Hybrid cards.

%prep
%setup -q -n %{name}-%{version}
%{__cp} -a %{SOURCE5} .

%build
%configure \
	--prefix=/usr \
	--sysconfdir=/etc
%{__make} -s %{?_smp_mflags}

%install
%{__make} -s install DESTDIR="%{buildroot}"
%{__install} -D -m0755 %{SOURCE1} %{buildroot}%{_initrddir}/bumlebeed
%{__install} -d %{buildroot}%{_defaultdocdir}/%{name}-%{version}/
%{__install} GPL-v3.0.txt %{buildroot}%{_defaultdocdir}/%{name}-%{version}/

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc /usr/share/doc/%{name}-%{version}/GPL-v3.0.txt
%dir /etc/bumblebee
%dir /usr/share/doc/bumblebee
%{_sbindir}/bumblebeed
%{_bindir}/bumblebee-bugreport
%{_initrddir}/bumlebeed
%attr (644, root, root) /etc/bash_completion.d/bumblebee
%attr (644, root, root) /etc/bumblebee/bumblebee.conf
%attr (644, root, root) /etc/bumblebee/xorg.conf.nouveau
%attr (644, root, root) /etc/bumblebee/xorg.conf.nvidia
%attr (755, root, root) /usr/bin/optirun
%attr (644, root, root) /usr/share/doc/bumblebee/README.markdown
%attr (644, root, root) /usr/share/doc/bumblebee/RELEASE_NOTES_3_0
%attr (644, root, root) /usr/share/man/man1/bumblebeed.1.gz
%attr (644, root, root) /usr/share/man/man1/optirun.1.gz

%post
chkconfig --add bumblebeed
chkconfig bumblebeed on

%changelog
* Tue Mar 06 2012 Rob Mokkink <rob@mokkinksystems.com> - 3.0-2
- ELRepo standards [Akemi Yagi]
- Add GPLv3 license file

* Sun Feb 26 2012 Rob Mokkink <rob@mokkinksystems.com> - 3.0-1
- initial version
