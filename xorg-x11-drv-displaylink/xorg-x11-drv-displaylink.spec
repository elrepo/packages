%define tarball xf-video-udlfb
%define moduledir %{_libdir}/xorg/modules
%define driverdir %{moduledir}/drivers

Summary:   Xorg X11 displaylink video driver
Name:      xorg-x11-drv-displaylink
Version:   0.0.1
Release:   1%{?dist}
URL:       http://git.plugable.com/gitphp/index.php?p=xf-video-udlfb
License:   GPLv2
Group:     User Interface/X Hardware Support
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

# Source tarball created from:
# http://git.plugable.com/gitphp/index.php?p=xf-video-udlfb&a=snapshot&h=ace449b4d1f51e3ac94636a82ad56f80e6870ba4
Source0:   %{tarball}.tar

BuildRequires: autoconf automake libtool
BuildRequires: xorg-x11-server-devel
BuildRequires: redhat-rpm-config

Requires:  xorg-x11-server-Xorg

%description 
X.Org X11 DisplayLink video driver.

%prep
%setup -q -n %{tarball}

%build
%configure --disable-static --with-xorg-module-dir=%{moduledir}
%{__make}

%install
%{__rm} -rf %{buildroot}
%{__make} install DESTDIR=%{buildroot}

# Install the docs
%{__install} -d %{buildroot}%{_defaultdocdir}/%{name}-%{version}
%{__install} -p -m 0644 ChangeLog COPYING README %{buildroot}%{_defaultdocdir}/%{name}-%{version}/

# FIXME: Remove all libtool archives (*.la) from modules directory.  This
# should be fixed in upstream Makefile.am or whatever.
find %{buildroot} -regex ".*\.la$" | xargs rm -f --

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{driverdir}/displaylink_drv.so
%dir %{_defaultdocdir}/%{name}-%{version}/
%{_defaultdocdir}/%{name}-%{version}/*

%changelog
* Sat Jul 02 2011 Philip J Perry <phil@elrepo.org> 0.0.1-1
- Initial spec file for displaylink video driver.
