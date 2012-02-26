Name: bbswitch
Version: 0.4.1
Release:	1%{?dist}
Summary: bbswitch is a kernel module which automatically detects the required ACPI calls for two kinds of Optimus laptops. 

Group: System Environment/Daemons		
License: GPL	
URL:https://github.com/Bumblebee-Project/bbswitch 
Source0: bbswitch-%{version}.tar.gz	
BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires: dkms
Requires: kernel-headers

%description
bbswitch is a kernel module which automatically detects the required ACPI calls for two kinds of Optimus laptops. It has been verified to work with "real" Optimus and "legacy" Optimus laptops (at least, that is how I call them). The machines on which these tests has performed are:
    Clevo B7130 - GT 425M ("real" Optimus, Lekensteyns laptop)
    Dell Vostro 3500 - GT 310M ("legacy" Optimus, Samsagax' laptop)
(note: there is no need to add more supported laptops here as the universal calls should work for every laptop model supporting either Optimus calls)

It's preferred over manually hacking with the acpi_call module because it can detect the correct handle preceding _DSM and has some built-in safeguards:

You're not allowed to disable a card if a driver (nouveau, nvidia) is loaded.
Before suspend, the card is automatically enabled. When resuming, it's disabled again if that was the case before suspending. Hibernation should work, but it not tested.


%prep
%setup -q -n bbswitch-%{version}


%build
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make load
mkdir -p %{buildroot}/usr/src/bbswitch-%{version}
cp * %{buildroot}/usr/src/bbswitch-%{version}/

%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%dir /usr/src/bbswitch-%{version}
%attr (644, root, root) /usr/src/bbswitch-%{version}/

%post
/usr/sbin/dkms add -m bbswitch -v 0.4.1
/usr/sbin/dkms build -m bbswitch -v 0.4.1
/usr/sbin/dkms install -m bbswitch -v 0.4.1

%preun
/usr/sbin/dkms remove -m bbswitch -v 0.4.1 --all

%changelog
* Sun Feb 26 2012 Rob Mokkink rob@mokkinksystems.com
- initial version
