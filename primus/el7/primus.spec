# Filter the provides for libGL.so.1
# see bug http://elrepo.org/bugs/view.php?id=759
%global __provides_exclude ^libGL\\.so.*$

Name:           primus
Version:        20150328
Release:        2%{?dist}
Summary:        Faster OpenGL offloading for Bumblebee
License:        HPND
Group:          Hardware/Other
Url:            https://github.com/amonakov/primus
Source0:        %{name}-master.zip
BuildRequires:  mesa-libGL-devel
BuildRequires:  libX11-devel
Requires:       bumblebee
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
ExclusiveArch:	x86_64

%description
Primus is a shared library that provides OpenGL and GLX APIs and
implements low-overhead local-only client-side OpenGL offloading via GLX
forking, similar to VirtualGL. It intercepts GLX calls and redirects GL
rendering to a secondary X display, presumably driven by a faster GPU.
On swapping buffers, rendered contents are read back using a PBO and
copied onto the drawable it was supposed to be rendered on in the first
place.

%prep
%setup -q -n primus-master

%build
export CXXFLAGS="%{optflags}"
export LIBDIR=lib64
%{__make} %{?_smp_mflags}

%install
%{__install} -D %{_libdir}/libGL.so.1 %{buildroot}%{_libdir}/primus/libGL.so.1
%{__install} -D primusrun %{buildroot}%{_bindir}/primusrun

%files
%defattr(-,root,root)
%doc LICENSE.txt README.md technotes.md
%dir %{_libdir}/primus
%{_libdir}/primus/libGL.so.1
%{_bindir}/primusrun

%changelog
* Fri Jul 21 2017 Philip J Perry <phil@elrepo.org> - 20150328-2
- Filter the provides for libGL.so.1 [http://elrepo.org/bugs/view.php?id=759]
- Install technotes.md to docs
- Remove Source1, duplicate of primusrun in Source0
- Remove Source2, not used.
- Remove BR for gcc-c++ and unzip
- export LIBDIR for x86_64
- Clean up SPEC file

* Thu May 18 2017 Akemi Yagi <toracat@elrepo.org> - 20150328-1
- Rebuilt for ELRepo.
- Updated to version 20150328.

* Fri Jul 11 2014 schwab@linux-m68k.org
- type-directive.patch: Use %% in .type directive since @ is a comment
  character on arm
* Tue Jul  1 2014 tchvatal@suse.com
- Install the license and readme files properly.
- Respect optflags.
* Wed Jun 25 2014 tchvatal@suse.com
- Add baselibs.conf to filelist.
* Wed Jun 25 2014 cfarrell@suse.com
- license update: HPND
  See LICENSE.txt
- Install the license file.
* Tue Jan 14 2014 tchvatal@suse.com
- Needs fixing of the tarball fetching and update of license.
* Tue Jan 14 2014 tchvatal@suse.com
- Cleanup the package up to the openSUSE requirements.
* Thu Nov 29 2012 arnaldo.coelho@gmail.com
- first package version
