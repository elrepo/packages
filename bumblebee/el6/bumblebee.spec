Name: bumblebee		
Version: 3.0	
Release: 2%{?dist}
Summary: Bumblebee is a project that enables Linux to utilize the Nvidia Optimus Hybrid cards.
Group: System Environment/Daemons		
License: GPLv3	
URL: https://github.com/Bumblebee-Project		

BuildRequires: libbsd pkgconfig autoconf help2man glib2-devel libX11-devel
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

%build
%configure \
	--prefix=%{_usr} \
	--sysconfdir=%{_sysconfdir}
%{__make} -s %{?_smp_mflags}

%install
%{__make} -s install DESTDIR="%{buildroot}"
%{__install} -D -m0755 %{SOURCE1} %{buildroot}%{_initrddir}/bumlebeed
%{__install} -d %{buildroot}%{_defaultdocdir}/%{name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/%{name}-%{version}/
%{__mv} %{buildroot}%{_defaultdocdir}/%{name}/* %{buildroot}%{_defaultdocdir}/%{name}-%{version}/
%{__rm} -rf %{buildroot}%{_defaultdocdir}/%{name}

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,-)
%dir %{_sysconfdir}/%{name}
%dir %{_defaultdocdir}/%{name}-%{version}
%{_sbindir}/bumblebeed
%{_bindir}/%{name}-bugreport
%{_initrddir}/bumlebeed
%attr (644, root, root) %{_sysconfdir}/bash_completion.d/%{name}
%attr (644, root, root) %{_sysconfdir}/%{name}/%{name}.conf
%attr (644, root, root) %{_sysconfdir}/%{name}/xorg.conf.nouveau
%attr (644, root, root) %{_sysconfdir}/%{name}/xorg.conf.nvidia
%attr (755, root, root) %{_usr}/bin/optirun
%attr (644, root, root) %{_defaultdocdir}/%{name}-%{version}/GPL-v3.0.txt
%attr (644, root, root) %{_defaultdocdir}/%{name}-%{version}/README.markdown
%attr (644, root, root) %{_defaultdocdir}/%{name}-%{version}/RELEASE_NOTES_3_0
%attr (644, root, root) %{_mandir}/man1/bumblebeed.1.gz
%attr (644, root, root) %{_mandir}/man1/optirun.1.gz

%post
chkconfig --add bumblebeed
chkconfig bumblebeed on

%changelog
* Wed Mar 07 2012 Rob Mokkink <rob@mokkinksystems.com> - 3.0-2
- ELRepo Project standards [Akemi Yagi, Alan Bartlett]
- Added GPLv3 license file

* Sun Feb 26 2012 Rob Mokkink <rob@mokkinksystems.com> - 3.0-1
- Initial version
