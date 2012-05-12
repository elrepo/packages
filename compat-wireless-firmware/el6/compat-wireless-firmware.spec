# This is a meta package to require all of the individual firmware packages.

Summary:	Firmwares for the compat-wireless package
Name:		compat-wireless-firmware
Version:	3.3
Release:	1%{?dist}
License:	Redistributable, no modification permitted
Group:		System Environment/Kernel
URL:		http://linuxwireless.org/

BuildArch: noarch

# Require the individual firmware packages
Requires: ath9k_htc-firmware


%description
This package installs all of the individual firmware packages
required for the compat-wireless drivers package.

%prep
# nothing to prep

%build
# nothing to build

%install
%{__rm} -rf %{buildroot}
%{__mkdir_p} %{buildroot}

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,-)

%changelog
* Sat May 12 2012 Philip J Perry <phil@elrepo.org> - 3.3-1
- Initial package for compat-wireless firmwares
  [http://elrepo.org/bugs/view.php?id=273]
