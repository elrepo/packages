%define brcmname tg3
%define brcmvers 3.139l
%define brcmfmly NetXtreme
%define brcmwork %{brcmname}-%{brcmvers}

%define debug_package %{nil}

Summary   : Broadcom %{brcmfmly} Gigabit ethernet driver
Name      : %{brcmname}
Version   : %{brcmvers}
Release   : 1
Vendor    : Broadcom Ltd.
License   : GPL
Group     : System/Kernel
Source    : %{brcmwork}.tar.bz2
BuildRoot : /var/tmp/%{name}-%{version}-%{release}-buildroot

%define brcmfilelist file.list.%{name}

%description
This package contains the Broadcom %{brcmfmly} Gigabit ethernet driver.

%prep
%setup -T -b 0 -n %{brcmwork}

%build
value=%{?KVER}
if [ -z "$value" ]; then
	KVER=$(uname -r)
else
	KVER=$value
fi
make KVER=$KVER

%install
value=%{?KVER}
if [ -z "$value" ]; then
	KVER=$(uname -r)
else
	KVER=$value
fi

BCM_KVER=`echo $KVER | cut -c1-3 | sed -e 's/\.//'`
if [ $BCM_KVER -gt 24 ];then
	BCM_DRV=%{brcmname}.ko
else
	BCM_DRV=%{brcmname}.o
fi

if [ -d /lib/modules/$KVER/updates ]; then
	BCMDSTDIR=updates
elif [ -f /etc/depmod.conf ]; then
	if grep -q "search.*[[:space:]]updates" /etc/depmod.conf; then
		BCMDSTDIR=updates
	fi
fi
if [ -z "$BCMDSTDIR" -a -d /etc/depmod.d ]; then
	if grep -q "search.*[[:space:]]updates" /etc/depmod.d/*; then
		BCMDSTDIR=updates
	fi
fi
if [ -z "$BCMDSTDIR" ]; then
	BCMDSTDIR=kernel/drivers/net
fi

echo "%defattr(-,root,root)"                           > %{brcmfilelist}

echo "/lib/modules/$KVER/$BCMDSTDIR/$BCM_DRV" >> %{brcmfilelist}
mkdir -p $RPM_BUILD_ROOT/lib/modules/$KVER/$BCMDSTDIR

echo "/usr/share/man/man4/%{brcmname}.4.*"            >> %{brcmfilelist}
mkdir -p $RPM_BUILD_ROOT/usr/share/man/man4

make install PREFIX=$RPM_BUILD_ROOT KVER=$KVER

%post
depmod -a > /dev/null 2> /dev/null
exit 0

%postun
depmod -a > /dev/null 2> /dev/null
exit 0

%clean
rm -rf $RPM_BUILD_ROOT

%files -f %{brcmfilelist}
%doc LICENSE README.TXT ChangeLog

%changelog
*Wed Mar 11 2024 Shantiprasad Shettar <shantiprasad.shettar@broadcom.com> 3.139l
- Fix compilation on Debian12.4 OS
- Add support for Oracle 9 UEK R7U2 kernel

*Wed Aug 09 2023 Pavan Chebbi <pavan.chebbi@broadcom.com> 3.139k
- Add support for SIMPLE_DEV_PM_OPS

*Mon Jan 16 2023 Pavan Chebbi <pavan.chebbi@broadcom.com> 3.139j
- Add additional support for kernel > 5.14

*Wed Oct 19 2022 Pavan Chebbi <pavan.chebbi@broadcom.com> 3.139i
- Add support for kernel > 5.14

*Tue Aug 03 2022 Pavan Chebbi <pavan.chebbi@broadcom.com> 3.139h
- Fix fw upgrade on RHEL 9.0

*Tue Jul 12 2022 Pavan Chebbi <pavan.chebbi@broadcom.com> 3.139g
- Fix compilation on SLES15 SP2

*Thu Jun 23 2022 Pavan Chebbi <pavan.chebbi@broadcom.com> 3.139f
- Fix compilation on SLES15 SP4
- Fix compilation on RHEL 8.6

*Fri April 29 2022 Pavan Chebbi <pavan.chebbi@broadcom.com> 3.139e
- Fix compilation on RHEL 9 Pre-GA builds

*Fri April 08 2022 Pavan Chebbi <pavan.chebbi@broadcom.com> 3.139d
- Fix compilation warning on RHEL 8.5

*Tue February 15 2022 Pavan Chebbi <pavan.chebbi@broadcom.com> 3.139c
- Add support for RHEL 9
- Support RPS by adding queue information into skb
- Various upstream patches backported

*Fri September 04 2020 Wei Hou <wei.hou@broadcom.com> 3.139b
- Backport upstream - pass the stuck queue to the timeout handler %minor_change
- Fix race condition between tg3_phy_stop() and tg3_timer_stop() %medium_change
- Backport upstream - release spinlock before calling synchronize_irq() %medium_change

*Fri May 15 2020 Wei Hou <wei.hou@broadcom.com> 3.139a
- Backport upstream to use new API ethtool_{get|set}_link_ksettings
- Backport upstream to make phy_ethtool_ksettings_get return void

*Wed Mar 25 2020 Wei Hou <wei.hou@broadcom.com> 3.138b
- Fix the issue that MTU can't be set to above 1500 in RHEL8.x

*Fri Feb 15 2019 Siva Reddy Kallam <siva.kallam@broadcom.com> 3.138a
- Fix Panic with SLES12SP3 %minor_change

*Tue Feb 05 2019 Siva Reddy Kallam <siva.kallam@broadcom.com> 3.137z
- Add RHEL 8.0 support %medium_change

*Thu Oct 11 2018 Siva Reddy Kallam <siva.kallam@broadcom.com> 3.137y
- Add private ioctl support for register read/write %medium_change

*Tue Jul 03 2018 Siva Reddy Kallam <siva.kallam@broadcom.com> 3.137x
- Add Ubuntu 18.04 support  %minor_change
- Fix race between tg3_get_stats64() and tg3_free_consistent() %minor_change
- Avoid usleep_range() with holding spin lock %minor_change

*Mon Jan 29 2018 Siva Reddy Kallam <siva.kallam@broadcom.com> 3.137w
- Add SLES 15 support  %medium_change
- Add RHEL 7.5 support %medium_change
- Fix RHEL 6.4 compilation %medium_change

*Mon Jan 01 2018 Siva Reddy Kallam <siva.kallam@broadcom.com> 3.137v
- Backport upstream patches  %medium_change
- Add PHY reset workaround in change MTU path for 5717/19/20  %medium_change
- Update the copyright %minor_change

*Thu Sep 07 2017 Siva Reddy Kallam <siva.kallam@broadcom.com> 3.137u
- Add support to overide clock for 5762  %medium_change

*Wed Aug 16 2017 Siva Reddy Kallam <siva.kallam@broadcom.com> 3.137t
- Add support to notify min/max MTUs through min_mtu & max_mtu %medium_change

*Fri Jun 23 2017 Siva Reddy Kallam <siva.kallam@broadcom.com> 3.137s
- Add support for RHEL 7.4 and SLES12SP2 %medium_change

*Wed Jun 13 2017 Siva Reddy Kallam <siva.kallam@broadcom.com> 3.137r
- Add workaround for MRRS of 5762  %medium_change

*Wed Sep 21 2016 Siva Reddy Kallam <siva.kallam@broadcom.com> 3.137q
- Add support for SLES12SP2 %medium_change

*Fri Jun 17 2016 Siva Reddy Kallam <siva.kallam@broadcom.com> 3.137p
- Update the copyright information for tg3 source files %minor_change

*Tue Dec 15 2015 Sanjeev Bansal <sanjeevb.bansal@broadcom.com> 3.137o
- Add work around HW/FW limitations with vlan encapsulated frames %minor_change
- Fix for transmit queue 0 timed out when too many gso_segs %medium_change

* Wed Nov 04 2015 Sanjeev Bansal <sanjeevb.bansal@broadcom.com> 3.137n
- Add support for 5720 inverting serdes signal detect feature %minor_change

* Wed Sep 09 2015 Deepak Khungar <deepak.khungar@broadcom.com> 3.137m
- Add support for RHEL 7.2 %minor_change

* Thu Apr 02 2015 Sanjeev Bansal <sanjeevb.bansal@broadcom.com>  3.137k
- Add support for linux 3.16.0 kernel %minor_change
- Add support for RHEL 4.8 %minor_change
- KVM PCI passthrough failed for tg3 on SUSE 11.3 %minor_change
- Add support for RHEL 5.11 %minor_change
- (SLES11SP4) After installation of tg3 driver, interface(s) don't come up %minor_change

* Wed Mar 25 2015 Siva Reddy Kallam <siva.kallam@broadcom.com>  3.137j
- Add support for SLES11SP4 %minor_change
- Add support for Debian 7.7 %minor_change
- Fixed delay in getting OperationalStatus of the nic on system reboot %minor_change

* Sat Dec 20 2014 Prashant Sreedharan <prashant.sreedharan@broadcom.com> 3.137h
- tg3_disable_ints using uninitialized mailbox value to disable interrupts %minor_change
- Enhancement for recoverable/unrecoverable errors %minor_change
- Add support for RHEL 6.6 %minor_change
- Readme file does not indicate tg3.spec location %minor_change
