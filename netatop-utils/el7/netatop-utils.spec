# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-123.el7.%{_target_cpu}}

Name:      netatop-utils
Version:   0.3
Release:   1%{?dist}
URL:       http://www.atoptool.nl
Summary:   Advanced System and Process Monitor
License:   GPL
Group:     System Environment

BuildRequires: systemd

Requires: kmod-netatop
Requires: atop

# Sources
Source0:  http://www.atoptool.nl/download/netatop-%{version}.tar.gz
Source1:  netatopd.service

%description
This package contains the necessary userland utilities for netatop

%prep
%setup -q -n netatop-%{version}

%build
pushd daemon
%{__make} KERNDIR=%{_usrsrc}/kernels/%{kversion}
popd


%install
rm    -rf 			  $RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT/%{_unitdir}/
mkdir -p $RPM_BUILD_ROOT/usr/sbin/
mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/systemd/system/
install -pm 644 %{SOURCE1} $RPM_BUILD_ROOT/%{_unitdir}/
install -m 775 daemon/netatopd -t $RPM_BUILD_ROOT/usr/sbin

%clean
rm -rf    $RPM_BUILD_ROOT

%post
# Configure atop for systemd
/bin/systemctl daemon-reload >/dev/null 2>&1
/bin/systemctl enable netatopd.service >/dev/null 2>&1
/bin/systemctl start netatopd.service >/dev/null 2>&1

%files
%defattr(-,root,root)
%{_unitdir}/netatopd.service
/usr/sbin/netatopd


%changelog
* Wed Aug 6 2014 Rob Mokkink <rob@mokkinksystems.com> 0.3-1
- Initial Build
