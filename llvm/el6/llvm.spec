# Build options:
#
# --with doxygen
#   The doxygen docs are HUGE, so they are not built by default.

%ifarch s390 s390x sparc64
  # No ocaml on these arches
  %bcond_with ocaml
%else
  %bcond_without ocaml
%endif

%if 0%{?rhel} >= 7
%global with_clang 0
ExcludeArch: s390 s390x ppc ppc64
%else
%global with_clang 1
%endif

#global prerel rcX
%global downloadurl http://llvm.org/%{?prerel:pre-}releases/%{version}%{?prerel:/%{prerel}}

Name:           llvm
Version:        3.0
Release:        5%{?dist}
Summary:        The Low Level Virtual Machine

Group:          Development/Languages
License:        NCSA
URL:            http://llvm.org/
Source0:        %{downloadurl}/llvm-%{version}%{?prerel:%{prerel}.src}.tar.gz
Source1:        %{downloadurl}/clang-%{version}%{?prerel:%{prerel}.src}.tar.gz
# multilib fixes
Source2:        llvm-Config-config.h
Source3:        llvm-Config-llvm-config.h

# Data files should be installed with timestamps preserved
Patch0:         llvm-2.6-timestamp.patch
# clang link failure if system GCC version is unknown
# http://llvm.org/bugs/show_bug.cgi?id=8897
Patch1:         clang-2.9-add_gcc_vers.patch
# Operator.h incompatibility with GCC 4.6 in C++0x mode
# http://llvm.org/bugs/show_bug.cgi?id=9869
#Patch2:         llvm-2.9-PR9869_operator_destructor.patch

BuildRequires:  bison
BuildRequires:  chrpath
BuildRequires:  flex
BuildRequires:  gcc-c++ >= 3.4
BuildRequires:  groff
BuildRequires:  libffi-devel
BuildRequires:  libtool-ltdl-devel
%if %{with ocaml}
BuildRequires:  ocaml-ocamldoc
%endif
BuildRequires:  zip
# for DejaGNU test suite
BuildRequires:  dejagnu tcl-devel python
# for doxygen documentation
%if 0%{?_with_doxygen}
BuildRequires:  doxygen graphviz
%endif
Requires:       llvm-libs = %{version}-%{release}

%description
LLVM is a compiler infrastructure designed for compile-time,
link-time, runtime, and idle-time optimization of programs from
arbitrary programming languages.  The compiler infrastructure includes
mirror sets of programming tools as well as libraries with equivalent
functionality.


%package devel
Summary:        Libraries and header files for LLVM
Group:          Development/Languages
Requires:       %{name} = %{version}-%{release}
Requires:       libffi-devel
Requires:       libstdc++-devel >= 3.4
Provides:       llvm-static = %{version}-%{release}

Requires(posttrans): /usr/sbin/alternatives
Requires(postun):    /usr/sbin/alternatives

%description devel
This package contains library and header files needed to develop new
native programs that use the LLVM infrastructure.


%package doc
Summary:        Documentation for LLVM
Group:          Documentation
BuildArch:      noarch
Requires:       %{name} = %{version}-%{release}
# might seem redundant, but needed to kill off the old arch-ed -doc
# subpackage
Obsoletes:      %{name}-doc < %{version}-%{release}

%description doc
Documentation for the LLVM compiler infrastructure.


%package libs
Summary:        LLVM shared libraries
Group:          System Environment/Libraries

%description libs
Shared libraries for the LLVM compiler infrastructure.


%if %{with_clang}
%package -n clang
Summary:        A C language family front-end for LLVM
License:        NCSA
Group:          Development/Languages
Requires:       llvm = %{version}-%{release}
# clang requires gcc; clang++ gcc-c++
Requires:       gcc-c++

%description -n clang
clang: noun
    1. A loud, resonant, metallic sound.
    2. The strident call of a crane or goose.
    3. C-language family front-end toolkit.

The goal of the Clang project is to create a new C, C++, Objective C
and Objective C++ front-end for the LLVM compiler. Its tools are built
as libraries and designed to be loosely-coupled and extensible.


%package -n clang-devel
Summary:        Header files for clang
Group:          Development/Languages
Requires:       clang = %{version}-%{release}

%description -n clang-devel
This package contains header files for the Clang compiler.


%package -n clang-analyzer
Summary:        A source code analysis framework
License:        NCSA
Group:          Development/Languages
Requires:       clang = %{version}-%{release}
# not picked up automatically since files are currently not instaled
# in standard Python hierarchies yet
Requires:       python

%description -n clang-analyzer
The Clang Static Analyzer consists of both a source code analysis
framework and a standalone tool that finds bugs in C and Objective-C
programs. The standalone tool is invoked from the command-line, and is
intended to run in tandem with a build of a project or code base.


%package -n clang-doc
Summary:        Documentation for Clang
Group:          Documentation
BuildArch:      noarch
Requires:       %{name} = %{version}-%{release}

%description -n clang-doc
Documentation for the Clang compiler front-end.
%endif


%if 0%{?_with_doxygen}
%package apidoc
Summary:        API documentation for LLVM
Group:          Development/Languages
BuildArch:      noarch
Requires:       %{name}-doc = %{version}-%{release}


%description apidoc
API documentation for the LLVM compiler infrastructure.


%if %{with_clang}
%package -n clang-apidoc
Summary:        API documentation for Clang
Group:          Development/Languages
BuildArch:      noarch
Requires:       clang-doc = %{version}-%{release}


%description -n clang-apidoc
API documentation for the Clang compiler.
%endif
%endif


%if %{with ocaml}
%package        ocaml
Summary:        OCaml binding for LLVM
Group:          Development/Libraries
Requires:       %{name} = %{version}-%{release}
Requires:       ocaml-runtime

%description    ocaml
OCaml binding for LLVM.


%package        ocaml-devel
Summary:        Development files for %{name}-ocaml
Group:          Development/Libraries
Requires:       %{name}-devel = %{version}-%{release}
Requires:       %{name}-ocaml = %{version}-%{release}
Requires:       ocaml

%description    ocaml-devel
The %{name}-ocaml-devel package contains libraries and signature files
for developing applications that use %{name}-ocaml.


%package ocaml-doc
Summary:        Documentation for LLVM's OCaml binding
Group:          Documentation
BuildArch:      noarch
Requires:       %{name}-ocaml = %{version}-%{release}

%description ocaml-doc
HTML documentation for LLVM's OCaml binding.
%endif


%prep
%setup -q -n llvm-%{version}%{?prerel}.src %{?with_clang:-a1} %{?_with_gcc:-a2}
rm -r -f tools/clang
%if %{with_clang}
mv clang-%{version}%{?prerel}.src tools/clang
%endif

# llvm patches
%patch0 -p1 -b .timestamp
#patch2 -p1 -b .pr9869_operator_destructor

# clang patches
#pushd tools/clang
#patch1 -p1 -b .add_gcc_ver
#popd

# fix ld search path
# TODO: remove /%{_lib} after usrmove migration is final
sed -i 's|/lib /usr/lib $lt_ld_extra|/%{_lib} %{_libdir} $lt_ld_extra|' \
    ./configure


%build
# Disabling assertions now, rec. by pure and needed for OpenGTL
# TESTFIX no PIC on ix86: http://llvm.org/bugs/show_bug.cgi?id=3801
%configure \
  --prefix=%{_prefix} \
  --libdir=%{_libdir}/%{name} \
%if 0%{?_with_doxygen}
  --enable-doxygen \
%endif
%if 0%{?rhel} >= 7
  --enable-targets=host \
%endif
  --disable-assertions \
  --enable-debug-runtime \
  --enable-jit \
  --enable-libffi \
  --enable-shared

# FIXME file this
# configure does not properly specify libdir
sed -i 's|(PROJ_prefix)/lib|(PROJ_prefix)/%{_lib}/%{name}|g' Makefile.config

make %{_smp_mflags} REQUIRES_RTTI=1 VERBOSE=1 \
%ifarch ppc
  OPTIMIZE_OPTION="%{optflags} -fno-var-tracking-assignments"
%else
  OPTIMIZE_OPTION="%{optflags}"
%endif


%install
rm -rf %{buildroot}
# workaround for http://llvm.org/bugs/show_bug.cgi?id=11177
%if %{with ocaml}
cp -p bindings/ocaml/llvm/META.llvm bindings/ocaml/llvm/Release/
%endif

make install DESTDIR=%{buildroot} \
     PROJ_docsdir=/moredocs

# multilib fixes
mv %{buildroot}%{_bindir}/llvm-config{,-%{__isa_bits}}

pushd %{buildroot}%{_includedir}/llvm/Config
mv config.h config-%{__isa_bits}.h
cp -p %{SOURCE2} config.h
mv llvm-config.h llvm-config-%{__isa_bits}.h
cp -p %{SOURCE3} llvm-config.h
popd

# Create ld.so.conf.d entry
mkdir -p %{buildroot}%{_sysconfdir}/ld.so.conf.d
cat >> %{buildroot}%{_sysconfdir}/ld.so.conf.d/llvm-%{_arch}.conf << EOF
%{_libdir}/llvm
EOF

%if %{with_clang}
# Static analyzer not installed by default:
# http://clang-analyzer.llvm.org/installation#OtherPlatforms
mkdir -p %{buildroot}%{_libdir}/clang-analyzer
# create launchers
for f in scan-{build,view}; do
  ln -s %{_libdir}/clang-analyzer/$f/$f %{buildroot}%{_bindir}/$f
done

(cd tools/clang/tools && cp -pr scan-{build,view} \
 %{buildroot}%{_libdir}/clang-analyzer/)
%endif

# Move documentation back to build directory
# 
mv %{buildroot}/moredocs .
rm -f moredocs/*.tar.gz
rm -f moredocs/ocamldoc/html/*.tar.gz

# and separate the apidoc
%if 0%{?_with_doxygen}
mv moredocs/html/doxygen apidoc
mv tools/clang/docs/doxygen/html clang-apidoc
%endif

# And prepare Clang documentation
#
%if %{with_clang}
mkdir clang-docs
for f in LICENSE.TXT NOTES.txt README.txt; do # TODO.txt; do
  ln tools/clang/$f clang-docs/
done
rm -rf tools/clang/docs/{doxygen*,Makefile*,*.graffle,tools}
%endif


#find %%{buildroot} -name .dir -print0 | xargs -0r rm -f
file %{buildroot}/%{_bindir}/* | awk -F: '$2~/ELF/{print $1}' | xargs -r chrpath -d
file %{buildroot}/%{_libdir}/llvm/*.so | awk -F: '$2~/ELF/{print $1}' | xargs -r chrpath -d
#chrpath -d %%{buildroot}/%%{_libexecdir}/clang-cc

# Get rid of erroneously installed example files.
rm %{buildroot}%{_libdir}/%{name}/*LLVMHello.*

# FIXME file this bug
sed -i 's,ABS_RUN_DIR/lib",ABS_RUN_DIR/%{_lib}/%{name}",' \
  %{buildroot}%{_bindir}/llvm-config-%{__isa_bits}

chmod -x %{buildroot}%{_libdir}/%{name}/*.a

# remove documentation makefiles:
# they require the build directory to work
find examples -name 'Makefile' | xargs -0r rm -f


%check
# the Koji build server does not seem to have enough RAM
# for the default 16 threads

# LLVM test suite failing on PPC64 and s390(x)
make check LIT_ARGS="-v -j4" \
%ifarch ppc64 s390 s390x
 || :
%else
 %{nil}
%endif

%if %{with_clang}
# clang test suite failing on PPC and s390(x)
# FIXME:
# unexpected failures on all platforms with GCC 4.7.0.
# capture logs
make -C tools/clang/test TESTARGS="-v -j4" \
     | tee clang-testlog.txt
#ifarch ppc ppc64 s390 s390x
# || :
#else
# %{nil}
#endif
%endif


%post libs -p /sbin/ldconfig
%postun libs -p /sbin/ldconfig

%if %{with_clang}
%post -n clang -p /sbin/ldconfig
%postun -n clang -p /sbin/ldconfig
%endif


%posttrans devel
# link llvm-config to the platform-specific file;
# use ISA bits as priority so that 64-bit is preferred
# over 32-bit if both are installed
alternatives \
  --install \
  %{_bindir}/llvm-config \
  llvm-config \
  %{_bindir}/llvm-config-%{__isa_bits} \
  %{__isa_bits}

%postun devel
if [ $1 -eq 0 ]; then
  alternatives --remove llvm-config \
    %{_bindir}/llvm-config-%{__isa_bits}
fi
exit 0


%files
%defattr(-,root,root,-)
%doc CREDITS.TXT LICENSE.TXT README.txt
%{_bindir}/bugpoint
%{_bindir}/llc
%{_bindir}/lli
%exclude %{_bindir}/llvm-config-%{__isa_bits}
%{_bindir}/llvm*
%{_bindir}/macho-dump
%{_bindir}/opt
%if %{with_clang}
%exclude %{_mandir}/man1/clang.1.*
%endif
%doc %{_mandir}/man1/*.1.*

%files devel
%defattr(-,root,root,-)
%{_bindir}/llvm-config-%{__isa_bits}
%{_includedir}/%{name}
%{_includedir}/%{name}-c
%{_libdir}/%{name}/*.a

%files libs
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/ld.so.conf.d/llvm-%{_arch}.conf
%dir %{_libdir}/%{name}
%if %{with_clang}
%exclude %{_libdir}/%{name}/libclang.so
%endif
%{_libdir}/%{name}/*.so

%if %{with_clang}
%files -n clang
%defattr(-,root,root,-)
%doc clang-docs/* clang-testlog.txt
%{_bindir}/clang*
%{_bindir}/c-index-test
%{_libdir}/%{name}/libclang.so
%{_prefix}/lib/clang
%doc %{_mandir}/man1/clang.1.*

%files -n clang-devel
%defattr(-,root,root,-)
%{_includedir}/clang
%{_includedir}/clang-c

%files -n clang-analyzer
%defattr(-,root,root,-)
%{_bindir}/scan-build
%{_bindir}/scan-view
%{_libdir}/clang-analyzer

%files -n clang-doc
%defattr(-,root,root,-)
%doc tools/clang/docs/*
%endif

%files doc
%defattr(-,root,root,-)
%doc examples moredocs/html

%if %{with ocaml}
%files ocaml
%defattr(-,root,root,-)
%{_libdir}/ocaml/*.cma
%{_libdir}/ocaml/*.cmi
%{_libdir}/ocaml/META.llvm

%files ocaml-devel
%defattr(-,root,root,-)
%{_libdir}/ocaml/*.a
%{_libdir}/ocaml/*.cmx*
%{_libdir}/ocaml/*.mli

%files ocaml-doc
%defattr(-,root,root,-)
%doc moredocs/ocamldoc/html/*
%endif

%if 0%{?_with_doxygen}
%files apidoc
%defattr(-,root,root,-)
%doc apidoc/*

%if %{with_clang}
%files -n clang-apidoc
%defattr(-,root,root,-)
%doc clang-apidoc/*
%endif
%endif

%changelog
* Sun Feb 26 2012 Phil Schaffner <pschaff2@verison.net> 3.0-5
- Build for EL6

* Sun Feb  5 2012 Michel Salim <salimma@fedoraproject.org> - 3.0-5
- Clang test suite yields unexpected failures with GCC 4.7.0. Make
  this non-fatal and save the results
- Multilib fix for harcoded ld search path in ./configure script

* Sat Jan 07 2012 Richard W.M. Jones <rjones@redhat.com> - 3.0-4
- Rebuild for OCaml 3.12.1.

* Wed Dec 14 2011 Adam Jackson <ajax@redhat.com> 3.0-3
- Also ExcludeArch: ppc* in RHEL

* Tue Dec 13 2011 Adam Jackson <ajax@redhat.com> 3.0-2
- ExcludeArch: s390* in RHEL since the native backend has disappeared in 3.0

* Sun Dec 11 2011 Michel Salim <salimma@fedoraproject.org> - 3.0-1
- Update to final 3.0 release

* Mon Dec 05 2011 Adam Jackson <ajax@redhat.com> 3.0-0.2.rc3
- RHEL customization: disable clang, --enable-targets=host

* Fri Nov 11 2011 Michel Salim <salimma@fedoraproject.org> - 3.0-0.1.rc3
- Update to 3.0rc3

* Tue Oct 11 2011 Dan Horák <dan[at]danny.cz> - 2.9-5
- don't fail the build on failing tests on ppc(64) and s390(x)

* Fri Sep 30 2011 Michel Salim <salimma@fedoraproject.org> - 2.9-4
- Apply upstream patch for Operator.h C++0x incompatibility (# 737365)

* Sat Aug  6 2011 Michel Salim <salimma@fedoraproject.org> - 2.9-3
- Disable LLVM test suite on ppc64 architecture  (# 728604)
- Disable clang test suite on ppc* architectures (-)

* Wed Aug  3 2011 Michel Salim <salimma@fedoraproject.org> - 2.9-2
- Add runtime dependency of -devel on libffi-devel

* Mon Aug  1 2011 Michel Salim <salimma@fedoraproject.org> - 2.9-1
- Update to 2.9
- Depend on libffi to allow the LLVM interpreter to call external functions
- Build with RTTI enabled, needed by e.g. Rubinius (# 722714)
- Fix multilib installation (# 699416)
- Fix incorrect platform-specific include path on i686

* Tue May 31 2011 Karsten Hopp <karsten@redhat.com> 2.9-0.4.rc2
- enable ppc64 build

* Fri Mar 25 2011 Michel Salim <salimma@fedoraproject.org> - 2.9-0.3.rc2
- Update to 2.9rc2

* Thu Mar 18 2011 Michel Salim <salimma@fedoraproject.org> - 2.9-0.2.rc1
- Split shared libraries into separate subpackage
- Don't include test logs; breaks multilib (# 666195)
- clang++: also search for platform-specific include files (# 680644)

* Thu Mar 10 2011 Michel Salim <salimma@fedoraproject.org> - 2.9-0.1.rc1
- Update to 2.9rc1

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.8-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Mon Jan 10 2011 Richard W.M. Jones <rjones@redhat.com> - 2.8-6
- Rebuild for OCaml 3.12 (http://fedoraproject.org/wiki/Features/OCaml3.12).

* Sat Nov 27 2010 Michel Salim <salimma@fedoraproject.org> - 2.8-5
- clang now requires gcc-c++ for linking and headers (bug #654560)

* Fri Nov 12 2010 Michel Salim <salimma@fedoraproject.org> - 2.8-4
- Backport support for C++0x (# 648990)

* Fri Oct 15 2010 Michel Salim <salimma@fedoraproject.org> - 2.8-3
- Re-add omitted %%{_includedir}

* Thu Oct 14 2010 Michel Salim <salimma@fedoraproject.org> - 2.8-2
- Add correct C include directory at compile time (# 641500)

* Tue Oct 12 2010 Michel Salim <salimma@fedoraproject.org> - 2.8-1
- Update to 2.8 release

* Wed Sep 29 2010 jkeating - 2.7-10
- Rebuilt for gcc bug 634757

* Mon Sep 20 2010 Michel Salim <salimma@fedoraproject.org> - 2.7-9
- Dynamically determine C++ include path at compile time (# 630474)
- Remove unneeded BuildRoot field and clean section

* Wed Sep 15 2010 Dennis Gilmore <dennis@ausil.us> - 2.7-8
- disable ocaml support on sparc64

* Wed Aug 11 2010 David Malcolm <dmalcolm@redhat.com> - 2.7-7
- recompiling .py files against Python 2.7 (rhbz#623332)

* Sat Jul 17 2010 Dan Horák <dan[at]danny.cz> - 2.7-6
- conditionalize ocaml support

* Mon Jun  7 2010 Michel Salim <salimma@fedoraproject.org> - 2.7-5
- Make the new noarch -doc obsoletes older (arched) subpackages

* Sat Jun  5 2010 Michel Salim <salimma@fedoraproject.org> - 2.7-4
- Add F-12/x86_64 and F-13 C++ header paths

* Wed May 26 2010 Michel Salim <salimma@fedoraproject.org> - 2.7-3
- Revert to disabling apidoc by default

* Mon May 24 2010 Michel Salim <salimma@fedoraproject.org> - 2.7-2
- Exclude llm-gcc manpages
- Turn on apidoc generation
- Build with srcdir=objdir, otherwise clang doxygen build fails

* Sun May  2 2010 Michel Salim <salimma@fedoraproject.org> - 2.7-1
- Update to final 2.7 release

* Sun Mar 28 2010 Michel Salim <salimma@fedoraproject.org> - 2.7-0.1.pre1
- Update to first 2.7 pre-release

* Fri Sep 18 2009 Michel Salim <salimma@fedoraproject.org> - 2.6-0.6.pre2
- Update to 2.6 pre-release2
- -devel subpackage now virtually provides -static

* Wed Sep  9 2009 Michel Salim <salimma@fedoraproject.org> - 2.6-0.5.pre1
- Disable var tracking assignments on PPC

* Wed Sep  9 2009 Michel Salim <salimma@fedoraproject.org> - 2.6-0.4.pre1
- Don't adjust clang include dir; files there are noarch (bz#521893)
- Enable clang unit tests
- clang and clang-analyzer renamed; no longer depend on llvm at runtime

* Mon Sep  7 2009 Michel Salim <salimma@fedoraproject.org> - 2.6-0.3.pre1
- Package Clang's static analyzer tools

* Mon Sep  7 2009 Michel Salim <salimma@fedoraproject.org> - 2.6-0.2.pre1
- PIC is now enabled by default; explicitly disable on %%{ix86}

* Mon Sep  7 2009 Michel Salim <salimma@fedoraproject.org> - 2.6-0.1.pre1
- First 2.6 prerelease
- Enable Clang front-end
- Enable debuginfo generation

* Sat Sep  5 2009 Michel Salim <salimma@fedoraproject.org> - 2.5-6
- Disable assertions (needed by OpenGTL, bz#521261)
- Align spec file with upstream build instructions
- Enable unit tests

* Sat Aug 22 2009 Michel Salim <salimma@fedoraproject.org> - 2.5-5
- Only disable PIC on %%ix86; ppc actually needs it

* Sat Aug 22 2009 Michel Salim <salimma@fedoraproject.org> - 2.5-4
- Disable use of position-independent code on 32-bit platforms
  (buggy in LLVM <= 2.5)

* Sat Jul 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.5-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Wed Mar  4 2009 Michel Salim <salimma@fedoraproject.org> - 2.5-2
- Remove build scripts; they require the build directory to work

* Wed Mar  4 2009 Michel Salim <salimma@fedoraproject.org> - 2.5-1
- Update to 2.5
- Package build scripts (bug #457881)

* Tue Dec  2 2008 Michel Salim <salimma@fedoraproject.org> - 2.4-2
- Patched build process for the OCaml binding

* Tue Dec  2 2008 Michel Salim <salimma@fedoraproject.org> - 2.4-1
- Update to 2.4
- Package Ocaml binding

* Wed Jun 18 2008 Bryan O'Sullivan <bos@serpentine.com> - 2.3-2
- Add dependency on groff

* Wed Jun 18 2008 Bryan O'Sullivan <bos@serpentine.com> - 2.3-1
- LLVM 2.3

* Thu May 29 2008 Tom "spot" Callaway <tcallawa@redhat.com> - 2.2-4
- fix license tags

* Wed Mar  5 2008 Bryan O'Sullivan <bos@serpentine.com> - 2.2-3
- Fix compilation problems with gcc 4.3

* Mon Feb 18 2008 Fedora Release Engineering <rel-eng@fedoraproject.org> - 2.2-2
- Autorebuild for GCC 4.3

* Sun Jan 20 2008 Bryan O'Sullivan <bos@serpentine.com> - 2.1-2
- Fix review comments

* Sun Jan 20 2008 Bryan O'Sullivan <bos@serpentine.com> - 2.1-1
- Initial version
