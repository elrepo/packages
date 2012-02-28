Name: bumblebee		
Version: 3.0	
Release:	1%{?dist}
Summary: Bumblebee is a project that enables Linux to utilize the Nvidia Optimus Hybrid cards.

Group: System Environment/Daemons		
License: GPL	
URL: https://github.com/Bumblebee-Project		
Source0: bumblebee-%{version}.tar.gz	
Source1: bumblebeed
BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires: libbsd pkgconfig autoconf
Requires: libbsd 	

%description
Bumblebee is a project that enables Linux to utilize the Nvidia Optimus Hybrid cards.

%prep
%setup -q -n bumblebee-%{version}


%build
%configure \
	--prefix=/usr \
	 --sysconfdir=/etc
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
%{__install} -D -m0755 %{SOURCE1} %{buildroot}%{_initrddir}/bumlebeed
%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%doc
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
* Sun Feb 26 2012 Rob Mokkink rob@mokkinksystems.com
- initial version
