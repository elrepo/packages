Name: bumblebee		
Version: 3.1	
Release: 1%{?dist}
Summary: Bumblebee is a project that enables Linux to utilize the Nvidia Optimus Hybrid cards.
Group: System Environment/Daemons		
License: GPLv3	
URL: https://github.com/Bumblebee-Project		

BuildRequires: libbsd-devel pkgconfig autoconf help2man glib2-devel libX11-devel
Requires: libbsd 	
Requires: VirtualGL

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
%{__install} -D -m0755 %{SOURCE1} %{buildroot}%{_initrddir}/bumblebeed
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
%{_initrddir}/bumblebeed
%attr (644, root, root) %{_sysconfdir}/bash_completion.d/%{name}
%attr (644, root, root) %{_sysconfdir}/%{name}/%{name}.conf
%attr (644, root, root) %{_sysconfdir}/%{name}/xorg.conf.nouveau
%attr (644, root, root) %{_sysconfdir}/%{name}/xorg.conf.nvidia
%attr (644, root, root) /lib/udev/rules.d/99-remove-nvidia-dev.rules
%attr (755, root, root) %{_usr}/bin/optirun
%attr (644, root, root) %{_defaultdocdir}/%{name}-%{version}/GPL-v3.0.txt
%attr (644, root, root) %{_defaultdocdir}/%{name}-%{version}/README.markdown
%attr (644, root, root) %{_defaultdocdir}/%{name}-%{version}/RELEASE_NOTES_3_1
%attr (644, root, root) %{_mandir}/man1/bumblebeed.1.gz
%attr (644, root, root) %{_mandir}/man1/optirun.1.gz

%post
chkconfig --add bumblebeed
chkconfig bumblebeed on

# Check if the file /etc/X11/xorg.conf is present, if so rename it
if [[ -f /etc/X11/xorg.conf ]]
then
   # Moving the original xorg.conf to xorg.conf.bumblebee.backup
   mv /etc/X11/xorg.conf
fi

# Create the bumblebee group on the system if it doesn't exist
if [[ $(grep -c bumblebee /etc/group) -ne 1 ]]
then
   # add the group bumblebee
   groupadd bumblebee
fi 
   
%changelog
* Sat Mar 16 2013 Rob Mokkink <rob@mokkinksystems.com> - 3.1.1
- Upgrade to bumblebee version 3.1

* Sat Jun 23 2012 Rob Mokkink <rob@mokkinksystems.com> - 3.0-3
- Add VirtualGL package as required
- Move /etc/X11/xorg.conf to /etc/X11/xorg.config.bumblebee.backup
- Check if the group bumblebee exists

* Wed Mar 07 2012 Rob Mokkink <rob@mokkinksystems.com> - 3.0-2
- Added help2man, glib2-devel and libX11-devel to the spec file
- Added the sysvinit script to the spec file
- Modified the sysvinit script
- ELRepo Project standards [Akemi Yagi, Alan Bartlett]
- Added GPLv3 license file [Akemi Yagi]

* Sun Feb 26 2012 Rob Mokkink <rob@mokkinksystems.com> - 3.0-1
- Initial version
