%global selinux_variants mls targeted

Name: selinux-bumblebee		
Version: 1.0	
Release: 1%{?dist}
Summary: Custom selinux policy to allow bumblebee to interact with bbswitch	

Group: System Environment/SELinux		
License: GPLv3	
URL: http://bumblebee-project.org	

# Check selinux-policy
%{!?_selinux_policy_version: %global _selinux_policy_version %(sed -e 's,.*selinux-policy-\\([^/]*\\)/.*,\\1,' /usr/share/selinux/devel/policyhelp 2>/dev/null)}
%if "%{_selinux_policy_version}" != ""
Requires:      selinux-policy >= %{_selinux_policy_version}
%endif


Source0: bumblebee-bbswitch.te	
Source1: bumblebee-bbswitch.fc
Source2: bumblebee-bbswitch.if

BuildRequires: selinux-policy-devel	
BuildRequires: hardlink
Requires(post):   /usr/sbin/semodule, /sbin/restorecon, /sbin/fixfiles
Requires(postun): /usr/sbin/semodule, /sbin/restorecon, /sbin/fixfiles

%description
Custom selinux policy to allow bumblebee to interact with bbswitch

%prep
mkdir SELinux
cp -p %{SOURCE0} %{SOURCE1} %{SOURCE2} SELinux


%build
cd SELinux
for selinuxvariant in %{selinux_variants}
do
  make NAME=${selinuxvariant} -f /usr/share/selinux/devel/Makefile
  mv bumblebee-bbswitch.pp bumblebee-bbswitch.pp.${selinuxvariant}
  make NAME=${selinuxvariant} -f /usr/share/selinux/devel/Makefile clean
done
cd -


%install
for selinuxvariant in %{selinux_variants}
do
  install -d %{buildroot}%{_datadir}/selinux/${selinuxvariant}
  install -p -m 644 SELinux/bumblebee-bbswitch.pp.${selinuxvariant} \
    %{buildroot}%{_datadir}/selinux/${selinuxvariant}/bumblebee-bbswitch.pp
done

/usr/sbin/hardlink -cv %{buildroot}%{_datadir}/selinux


%post
for selinuxvariant in %{selinux_variants}
do
  /usr/sbin/semodule -s ${selinuxvariant} -i \
    %{_datadir}/selinux/${selinuxvariant}/bumblebee-bbswitch.pp &> /dev/null || :
done

%postun
if [ $1 -eq 0 ] ; then
  for selinuxvariant in %{selinux_variants}
  do
    /usr/sbin/semodule -s ${selinuxvariant} -r bumblebee-bbswitch &> /dev/null || :
  done
fi

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,0755)
%doc SELinux/*
%{_datadir}/selinux/*/bumblebee-bbswitch.pp


%changelog
* Sat Apr 04 2015 Rob Mokkink <rob@mokkinksystems.com> - 1.0-1
- Initial version

