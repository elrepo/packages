### ELRepo -- begin
# MAIN is ee0667aa201e7d725ec87b1e4cf08de1d748d64f
# CHECKOUT is ee0667aa
# DSTAMP is 20220301
### ELRepo -- end

%global checkout ee0667aa
%global firmware_release 80.3

%global _firmwarepath /usr/lib/firmware
%define _binaries_in_noarch_packages_terminate_build 0

Name:		linux-firmware
Version:	20220301
Release:	%{firmware_release}.git%{checkout}%{?dist}
Summary:	Firmware files used by the Linux kernel
License:	GPL+ and GPLv2+ and MIT and Redistributable, no modification permitted
URL:		https://git.kernel.org/cgit/linux/kernel/git/firmware/linux-firmware.git/
BuildArch:	noarch

### ELRepo -- begin
### Source0 creation:
###
### git clone git://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git
### cd linux-firmware
### CHECKOUT=$(cut -c1-8 .git/refs/heads/main)
### echo "checkout $CHECKOUT"
### DSTAMP=$(date +%Y%m%d)
### echo "version $DSTAMP"
### git archive --format=tar --prefix=linux-firmware-${CHECKOUT}/ HEAD | xz -9 -c > ../linux-firmware-${DSTAMP}.tar.xz
### ELRepo -- end
Source0:	%{name}-%{version}.tar.xz

Provides:	kernel-firmware = %{version} xorg-x11-drv-ati-firmware = 7.0
Obsoletes:	kernel-firmware < %{version} xorg-x11-drv-ati-firmware <= 6.13.0-0.22
Obsoletes:	ueagle-atm4-firmware <= 1.0-5
Obsoletes:	netxen-firmware <= 4.0.534-9
Obsoletes:	ql2100-firmware <= 1.19.38-7
Obsoletes:	ql2200-firmware <= 2.02.08-7
Obsoletes:	ql23xx-firmware <= 3.03.28-5
Obsoletes:	ql2400-firmware <= 5.08.00-2
Obsoletes:	ql2500-firmware <= 5.08.00-2
Obsoletes:	rt61pci-firmware <= 1.2-11
Obsoletes:	rt73usb-firmware <= 1.8-11
### ELRepo -- begin
#Obsoletes:	cx18-firmware <= 20080628-10
# ivtv-firmware also provided the file v4l-cx25840.fw in older releases until
# version 2:20080701-28 in Fedora, when this conflicting file was removed in
# favour of the same file provided by linux-firmware. Fedora dropped the
# Obsoletes (see Fedora bugs 1211055, 1232773). RHEL 8 doesn't build so far any
# ivtv-firmware version, and we need to obsolete the conflicting versions to
# avoid upgrade errors (see bug 1589055)
#Obsoletes:	ivtv-firmware < 2:20080701-28
### ELRepo -- end

BuildRequires:	make

%description
This package provides firmware files required for some devices to operate.

%package -n iwl100-firmware
Summary:	Firmware for Intel(R) Wireless WiFi Link 100 Series Adapters
License:	Redistributable, no modification permitted
Version:	39.31.5.1
Release:	%{firmware_release}%{?dist}
Obsoletes:	iwl100-firmware < 39.31.5.1-4
%description -n iwl100-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux to support the iwl100 hardware.  Usage of the firmware
is subject to the terms and conditions contained inside the provided
LICENSE file. Please read it carefully.

%package -n iwl105-firmware
Summary:	Firmware for Intel(R) Centrino Wireless-N 105 Series Adapters
License:	Redistributable, no modification permitted
Version:	18.168.6.1
Release:	%{firmware_release}%{?dist}
%description -n iwl105-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux to support the iwl105 hardware.  Usage of the firmware
is subject to the terms and conditions contained inside the provided
LICENSE file. Please read it carefully.

%package -n iwl135-firmware
Summary:	Firmware for Intel(R) Centrino Wireless-N 135 Series Adapters
License:	Redistributable, no modification permitted
Version:	18.168.6.1
Release:	%{firmware_release}%{?dist}
%description -n iwl135-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux to support the iwl135 hardware.  Usage of the firmware
is subject to the terms and conditions contained inside the provided
LICENSE file. Please read it carefully.

%package -n iwl1000-firmware
Summary:	Firmware for Intel® PRO/Wireless 1000 B/G/N network adaptors
License:	Redistributable, no modification permitted
Version:	39.31.5.1
Epoch:		1
Release:	%{firmware_release}%{?dist}
Obsoletes:	iwl1000-firmware < 1:39.31.5.1-3
%description -n iwl1000-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux to support the iwl1000 hardware.  Usage of the firmware
is subject to the terms and conditions contained inside the provided
LICENSE file. Please read it carefully.

%package -n iwl2000-firmware
Summary:	Firmware for Intel(R) Centrino Wireless-N 2000 Series Adapters
License:	Redistributable, no modification permitted
Version:	18.168.6.1
Release:	%{firmware_release}%{?dist}
%description -n iwl2000-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux to support the iwl2000 hardware.  Usage of the firmware
is subject to the terms and conditions contained inside the provided
LICENSE file. Please read it carefully.

%package -n iwl2030-firmware
Summary:	Firmware for Intel(R) Centrino Wireless-N 2030 Series Adapters
License:	Redistributable, no modification permitted
Version:	18.168.6.1
Release:	%{firmware_release}%{?dist}
%description -n iwl2030-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux to support the iwl2030 hardware.  Usage of the firmware
is subject to the terms and conditions contained inside the provided
LICENSE file. Please read it carefully.

%package -n iwl3160-firmware
Summary:	Firmware for Intel(R) Wireless WiFi Link 3160 Series Adapters
License:	Redistributable, no modification permitted
Epoch:		1
Version:	25.30.13.0
Release:	%{firmware_release}%{?dist}
%description -n iwl3160-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl3945-firmware
Summary:	Firmware for Intel® PRO/Wireless 3945 A/B/G network adaptors
License:	Redistributable, no modification permitted
Version:	15.32.2.9
Release:	%{firmware_release}%{?dist}
Obsoletes:	iwl3945-firmware < 15.32.2.9-7
%description -n iwl3945-firmware
This package contains the firmware required by the iwl3945 driver
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl4965-firmware
Summary:	Firmware for Intel® PRO/Wireless 4965 A/G/N network adaptors
License:	Redistributable, no modification permitted
Version:	228.61.2.24
Release:	%{firmware_release}%{?dist}
Obsoletes:	iwl4965-firmware < 228.61.2.24-5
%description -n iwl4965-firmware
This package contains the firmware required by the iwl4965 driver
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl5000-firmware
Summary:	Firmware for Intel® PRO/Wireless 5000 A/G/N network adaptors
License:	Redistributable, no modification permitted
Version:	8.83.5.1_1
Release:	%{firmware_release}%{?dist}
Obsoletes:	iwl5000-firmware < 8.83.5.1_1-3
%description -n iwl5000-firmware
This package contains the firmware required by the iwl5000 driver
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl5150-firmware
Summary:	Firmware for Intel® PRO/Wireless 5150 A/G/N network adaptors
License:	Redistributable, no modification permitted
Version:	8.24.2.2
Release:	%{firmware_release}%{?dist}
Obsoletes:	iwl5150-firmware < 8.24.2.2-4
%description -n iwl5150-firmware
This package contains the firmware required by the iwl5150 driver
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl6000-firmware
Summary:	Firmware for Intel(R) Wireless WiFi Link 6000 AGN Adapter
License:	Redistributable, no modification permitted
Version:	9.221.4.1
Release:	%{firmware_release}%{?dist}
Obsoletes:	iwl6000-firmware < 9.221.4.1-4
%description -n iwl6000-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl6000g2a-firmware
Summary:	Firmware for Intel(R) Wireless WiFi Link 6005 Series Adapters
License:	Redistributable, no modification permitted
Version:	18.168.6.1
Release:	%{firmware_release}%{?dist}
Obsoletes:	iwl6000g2a-firmware < 17.168.5.3-3
%description -n iwl6000g2a-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl6000g2b-firmware
Summary:	Firmware for Intel(R) Wireless WiFi Link 6030 Series Adapters
License:	Redistributable, no modification permitted
Version:	18.168.6.1
Release:	%{firmware_release}%{?dist}
Obsoletes:	iwl6000g2b-firmware < 17.168.5.2-3
%description -n iwl6000g2b-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl6050-firmware
Summary:	Firmware for Intel(R) Wireless WiFi Link 6050 Series Adapters
License:	Redistributable, no modification permitted
Version:	41.28.5.1
Release:	%{firmware_release}%{?dist}
Obsoletes:	iwl6050-firmware < 41.28.5.1-5
%description -n iwl6050-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl7260-firmware
Summary:	Firmware for Intel(R) Wireless WiFi Link 726x/8000/9000 Series Adapters
License:	Redistributable, no modification permitted
Epoch:		1
Version:	25.30.13.0
Release:	%{firmware_release}%{?dist}
# Obsolete iwl7265 sub-package which existed on RHEL 7, looking at git history
# Fedora never provided such sub-package (see bug 1589056)
Obsoletes:	iwl7265-firmware
%description -n iwl7260-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n libertas-usb8388-firmware
Summary:	Firmware for Marvell Libertas USB 8388 Network Adapter
License:	Redistributable, no modification permitted
Epoch:		2 
Obsoletes:	libertas-usb8388-firmware < 2:5.110.22.p23-8
%description -n libertas-usb8388-firmware
Firmware for Marvell Libertas USB 8388 Network Adapter

%package -n libertas-usb8388-olpc-firmware
Summary:	OLPC firmware for Marvell Libertas USB 8388 Network Adapter
License:	Redistributable, no modification permitted
%description -n libertas-usb8388-olpc-firmware
Firmware for Marvell Libertas USB 8388 Network Adapter with OLPC mesh network
support.

%package -n libertas-sd8686-firmware
Summary:	Firmware for Marvell Libertas SD 8686 Network Adapter
License:	Redistributable, no modification permitted
Obsoletes:	libertas-sd8686-firmware < 9.70.20.p0-4
%description -n libertas-sd8686-firmware
Firmware for Marvell Libertas SD 8686 Network Adapter

%package -n libertas-sd8787-firmware
Summary:	Firmware for Marvell Libertas SD 8787 Network Adapter
License:	Redistributable, no modification permitted
%description -n libertas-sd8787-firmware
Firmware for Marvell Libertas SD 8787 Network Adapter

%prep
%setup -q -n linux-firmware-%{checkout}

%install
mkdir -p $RPM_BUILD_ROOT/%{_firmwarepath}
mkdir -p $RPM_BUILD_ROOT/%{_firmwarepath}/updates
make DESTDIR=%{buildroot}/ FIRMWAREDIR=%{_firmwarepath} install

pushd $RPM_BUILD_ROOT/%{_firmwarepath}

# Remove firmware shipped in separate packages already
# Perhaps these should be built as subpackages of linux-firmware?
rm -rf ess korg sb16 yamaha

# Remove source files we don't need to install
rm -f usbdux/*dux */*.asm
rm -rf carl9170fw

# No need to install old firmware versions where we also provide newer versions
# which are preferred and support the same (or more) hardware
rm -f libertas/sd8686_v8*
rm -f libertas/usb8388_v5.bin

# Remove firmware for Creative CA0132 HD as it's in alsa-firmware
rm -f ctefx.bin ctspeq.bin

# Remove cxgb3 (T3 adapter) firmware (see bug 1503721)
rm -rf cxgb3

### ELRepo -- begin
# Remove v4l-* (provided by ivtv-firmware package)
rm -f v4l-*
### ELRepo -- end

# Remove superfluous infra files
rm -f check_whence.py configure Makefile README

popd

# Create file list but exclude firmwares that we place in subpackages
# and netronome/nic_AMDA* symlinks
FILEDIR=`pwd`
pushd $RPM_BUILD_ROOT/%{_firmwarepath}
find . \! -type d > $FILEDIR/linux-firmware.files
find . -type d | sed -e '/^.$/d' > $FILEDIR/linux-firmware.dirs
popd
sed -i -e 's:^./::' linux-firmware.{files,dirs}
sed -i -e '/^iwlwifi/d' \
	-i -e '/^libertas\/sd8686/d' \
	-i -e '/^libertas\/usb8388/d' \
	-i -e '/^mrvl\/sd8787/d' \
	-i -e '/^netronome\/nic_AMDA/d' \
	linux-firmware.files
sed -i -e 's:^:/usr/lib/firmware/:' linux-firmware.{files,dirs}
sed -i -e 's:^:":;s:$:":' linux-firmware.files
sed -e 's:^:%%dir :' linux-firmware.dirs >> linux-firmware.files

%post
# This pkg carries AMD microcode and it's important to early enable it in
# case it was updated. Because of that rebuild initrd after this pkg is 
# updated and only if it's an AMD CPU.
if [ $1 -gt 1 ]; then
	if [ -d /run/systemd/system ]; then
		if grep -q AuthenticAMD /proc/cpuinfo ; then
			dracut -f
		fi
	fi
fi

%files -n iwl100-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-100-5.ucode

%files -n iwl105-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-105-*.ucode

%files -n iwl135-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-135-*.ucode

%files -n iwl1000-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-1000-*.ucode

%files -n iwl2000-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-2000-*.ucode

%files -n iwl2030-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-2030-*.ucode

%files -n iwl3160-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-3160-*.ucode
%{_firmwarepath}/iwlwifi-3168-*.ucode

%files -n iwl3945-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-3945-*.ucode

%files -n iwl4965-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-4965-*.ucode

%files -n iwl5000-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-5000-*.ucode

%files -n iwl5150-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-5150-*.ucode

%files -n iwl6000-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-6000-*.ucode

%files -n iwl6000g2a-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-6000g2a-*.ucode

%files -n iwl6000g2b-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-6000g2b-*.ucode

%files -n iwl6050-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-6050-*.ucode

%files -n iwl7260-firmware
%license WHENCE LICENCE.iwlwifi_firmware
%{_firmwarepath}/iwlwifi-7260-*.ucode
%{_firmwarepath}/iwlwifi-7265-*.ucode
%{_firmwarepath}/iwlwifi-7265D-*.ucode
%{_firmwarepath}/iwlwifi-8000C-*.ucode
%{_firmwarepath}/iwlwifi-8265-*.ucode
%{_firmwarepath}/iwlwifi-9000-*.ucode
%{_firmwarepath}/iwlwifi-9260-*.ucode
%{_firmwarepath}/iwlwifi-cc-a0-*.ucode
%{_firmwarepath}/iwlwifi-Qu*.ucode
%{_firmwarepath}/iwlwifi-ty-a0-gf-a0-*.ucode
%{_firmwarepath}/iwlwifi-ty-a0-gf-a0*.pnvm

%files -n libertas-usb8388-firmware
%license WHENCE LICENCE.Marvell
%dir %{_firmwarepath}/libertas
%{_firmwarepath}/libertas/usb8388_v9.bin

%files -n libertas-usb8388-olpc-firmware
%license WHENCE LICENCE.Marvell
%dir %{_firmwarepath}/libertas
%{_firmwarepath}/libertas/usb8388_olpc.bin

%files -n libertas-sd8686-firmware
%license WHENCE LICENCE.Marvell
%dir %{_firmwarepath}/libertas
%{_firmwarepath}/libertas/sd8686*

%files -n libertas-sd8787-firmware
%license WHENCE LICENCE.Marvell
%dir %{_firmwarepath}/mrvl
%{_firmwarepath}/mrvl/sd8787*

%files -f linux-firmware.files
%dir %{_firmwarepath}
%license WHENCE LICENCE.*
%config(noreplace) %{_firmwarepath}/netronome/nic_AMDA*
### ELRepo -- begin
# Now list all the excluded Intel files that
# are not placed in their own subpackage.
%{_firmwarepath}/iwlwifi-so-a0-gf-a0-64.ucode
%{_firmwarepath}/iwlwifi-so-a0-gf-a0-67.ucode
%{_firmwarepath}/iwlwifi-so-a0-gf-a0-68.ucode
%{_firmwarepath}/iwlwifi-so-a0-gf-a0.pnvm
%{_firmwarepath}/iwlwifi-so-a0-gf4-a0-67.ucode
%{_firmwarepath}/iwlwifi-so-a0-gf4-a0-68.ucode
%{_firmwarepath}/iwlwifi-so-a0-gf4-a0.pnvm
%{_firmwarepath}/iwlwifi-so-a0-hr-b0-64.ucode
%{_firmwarepath}/iwlwifi-so-a0-hr-b0-68.ucode
%{_firmwarepath}/iwlwifi-so-a0-jf-b0-64.ucode
%{_firmwarepath}/iwlwifi-so-a0-jf-b0-68.ucode
### ELRepo -- end

%changelog
* Tue Mar 08 2022 Alan Bartlett <ajb@elrepo.org> - 20220301-80.3.gitee0667aa
- Updated to the latest upstream (kernel.org) linux-firmware as of this date.
- Do not obsolete the cx18-firmware and ivtv-firmware packages.
- Remove all v4l-* as they are packaged in the ivtv-firmware.
- [https://elrepo.org/bugs/view.php?id=1208]

* Sat Dec 12 2020 Augusto Caringi <acaringi@redhat.com> - 20200421-80.git78c0348
- Update Intel Bluetooth firmwares (rhbz 1895787)

* Tue May 5 2020 Jan Stancek <jstancek@redhat.com> - 20200421-79.git78c0348
- Update to latest upstream linux-firmware image for assorted updates (rhbz 1828390)

* Tue Apr 7 2020 Jan Stancek <jstancek@redhat.com> - 20200323-78.git8eb0b28
- Set higher compression (rhbz 1817410)

* Mon Mar 23 2020 Jan Stancek <jstancek@redhat.com> - 20200323-77.git8eb0b28
- Update to latest upstream linux-firmware image for assorted updates (rhbz 1813818)

* Tue Dec 17 2019 Jan Stancek <jstancek@redhat.com> - 20191203-76.gite8a0f4c
- Fix missing firmware symlinks (rhbz 1783196)

* Tue Dec 3 2019 Jan Stancek <jstancek@redhat.com> - 20191203-75.gite8a0f4c
- Update to latest upstream linux-firmware image for assorted updates
- Update qed zipped/unzipped firmware to latest upstream (rhbz 1720385)

* Wed Sep 18 2019 Jan Stancek <jstancek@redhat.com> - 20190918-74.git6c6918a
- Update to latest upstream linux-firmware image for assorted updates
- Update bnx2x firmware to latest upstream (rhbz 1720386)

* Fri Aug 16 2019 Jan Stancek <jstancek@redhat.com> - 20190815-73.git07b925b
- Update to latest upstream linux-firmware image for assorted updates (rhbz 1741356)
- iwlwifi- Update to support the Cyclone Peak Dual Band 2x2 802.11ax
- iwl7265-firmware sub-package merged into iwl7260-firmware sub-package

* Mon Apr 29 2019 Bruno E. O. Meneguele <bmeneg@redhat.com> - 20190429-72.gitddde598
- Update to latest upstream linux-firmware image for assorted updates
- cxgb4: update firmware to revision 1.23.4.0 (rhbz 1690727)
- linux-firmware: Add firmware file for Intel Bluetooth 22161 (rhbz 1622438)

* Mon Mar 18 2019 Bruno E. O. Meneguele <bmeneg@redhat.com> - 20190318-71.git283373f
- Update to latest upstream linux-firmware image for assorted updates
- cxgb4: update firmware to revision 1.23.3.0 (rhbz 1642422)

* Tue Feb 26 2019 Bruno E. O. Meneguele <bmeneg@redhat.com> - 20190225-70.git80dee31
- cxgb4: update firmware to revision 1.22.9.0 (rhbz 1671610)
- nfp: update Agilio SmartNIC flower firmware to rev AOTC-2.10.A.13 (rhbz 1637996)
- linux-firmware: Update firmware patch for Intel Bluetooth 8260 (rhbz 1649148)
- qed: Add 8.37.7.0 firmware image (rhbz 1654809)
- liquidio: fix GPL compliance issue (rhbz 1622521)
- Update Intel OPA hfi1 firmware (rhbz 1637240)
- qed: Add firmware 8.37.7.0 (rhbz 1643554)

* Tue Oct 09 2018 Bruno E. O. Meneguele <bmeneg@redhat.com> - 20180911-69.git85c5d90
- liquidio: remove firmware that violates GPL license (rhbz 1637696)

* Tue Sep 11 2018 Bruno E. O. Meneguele <bmeneg@redhat.com> - 20180911-68.git85c5d90
- Update to latest upstream linux-firmware image for assorted updates
- nvidia: update firmware for Pascal GPUs (rhbz 1625514)

* Tue Jul 17 2018 Bruno E. O. Meneguele <bmeneg@redhat.com> - 20180717-67.git8d69bab
- Update to latest upstream linux-firmware image for assorted netdrv updates
- chelsio: update firmware to revision 1.20.8.0 (rhbz 1523202)

* Mon Jun 25 2018 Bruno E. O. Meneguele <bmeneg@redhat.com> - 20180529-66.git7518922
- Only update initrd when the package is being updated (rhbz 1584178)

* Tue May 29 2018 Bruno E. O. Meneguele <bmeneg@redhat.com> - 20180529-65.git7518922
- Update to latest upstream linux-firmware image for assorted updates
- amd: update microcode for family 15h (rhbz 1574575)

* Tue May 22 2018 Bruno E. O. Meneguele <bmeneg@redhat.com> - 20180518-64.git2a9b2cf
- Add early initramfs update in case of AMD host (rhbz 1580584)

* Fri May 18 2018 Bruno E. O. Meneguele <bmeneg@redhat.com> - 20180518-63.git2a9b2cf
- Update to latest upstream linux-firmware image for assorted netdrv updates
- amd: add microcode for family 17h and update for family 15h (rhbz 1574575)

* Tue Feb 20 2018 Rafael Aquini <aquini@redhat.com> - 20180220-62.git6d51311
- nfp: NIC stops transmitting for small MSS values when TSO is enabled (rhbz 1542263)
- chelsio: pull in latests firmware 1.17.14.0 (rhbz 1538590)

* Tue Jan 16 2018 Rafael Aquini <aquini@redhat.com> - 20180113-61.git65b1c68
- Adjust Requires(post) for functional scripting due to ucode revert (rhbz 1533945)

* Sat Jan 13 2018 Rafael Aquini <aquini@redhat.com> - 20180113-60.git65b1c68
- Update to latest upstream linux-firmware image for assorted updates
- netr: Add Netronome TC flower firmware (rhbz 1518900)
- Revert amd-ucode for fam17h (rhbz 1533945)

* Wed Jan 03 2018 Rafael Aquini <aquini@redhat.com> - 20171127-59.git17e6288
- Add amd-ucode for fam17h

* Mon Nov 27 2017 Rafael Aquini <aquini@redhat.com> - 20171127-58.git17e6288
- Update to latest upstream linux-firmware image for assorted updates
- iwlwifi: Firmware update for 3160, 3168, 7260, 7265 and 7265D WIFI devices (rhbz 1508709)
- iwlwifi: Firmware update for 8260 and 8265 WIFI devices (rhbz 1508711)
- iwlwifi: Firmware update for 9260 and 9560 WIFI devices (rhbz 1501166)
- Fixes CVE-2016-0801 CVE-2017-0561 CVE-2017-9417

* Wed Oct 11 2017 Rafael Aquini <aquini@redhat.com> - 20171011-57.gitbf04291
- Update to latest upstream linux-firmware image for assorted updates
- ath10k: Update to the latest firmware released version (rhbz 1457363)
- opa: update Intel Omni-Path Architecture firmware (rhbz 1452785)
- chelsio: Pull in the latest firmware 1.16.63.0 (rhbz 1458328)
- qla2xxx: Update firmware version to 8.07.00 (rhbz 1427148)

* Mon Jun 26 2017 Rafael Aquini <aquini@redhat.com> - 20170606-56.gitc990aae
- opa: Revert switch firmware back to 0.47 (rhbz 1464629)

* Tue Jun 06 2017 Rafael Aquini <aquini@redhat.com> - 20170606-55.gitc990aae
- chelsio: Pull in the latest firmware 1.16.45.0 (rhbz 1457631)

* Tue May 30 2017 Rafael Aquini <aquini@redhat.com> - 20170530-54.gitdf40d15
- Update to latest upstream linux-firmware image for assorted updates
- opa: HFI firmware problems with new OPA switch firmware (rhbz 1452253)

* Tue May 23 2017 Rafael Aquini <aquini@redhat.com> - 20170328-53.git44d8e8d
- chelsio: Pull in the latest firmware 1.16.43.0 (rhbz 1451387)

* Tue Mar 28 2017 Rafael Aquini <aquini@redhat.com> - 20170328-52.git44d8e8d
- Update to latest upstream linux-firmware image for assorted updates
- nfp: add firmware for Netronome Ethernet Driver (rhbz 1432592)

* Tue Mar 07 2017 Rafael Aquini <aquini@redhat.com> - 20170307-51.git87714d8
- Update to latest upstream linux-firmware image for assorted updates
- chelsio: Update to the latest firmware released version (rhbz 1385911)
- drm: Update for radeon and amdgpu latest firmware released version (rhbz 1425197)

* Fri Feb 17 2017 Rafael Aquini <aquini@redhat.com> - 20170217-50.git6d3bc88
- Update to latest upstream linux-firmware image for assorted updates
- chelsio: Update to the latest firmware released version (rhbz 1395322)
- iwlwifi: Update to the latest firmware released version (rhbz 1385749)
- ath10k: Update to the latest firmware released version (rhbz 1385743)
- hfi1: Update to the latest firmware released version (rhbz 1382805)

* Tue Aug 30 2016 Rafael Aquini <aquini@redhat.com> - 20160830-49.git7534e19
- Update to latest upstream linux-firmware image for assorted updates
- Update QCA6174/hw3.0/board-2.bin file to support ath10k devices (rhbz 1368159)

* Tue Aug 23 2016 Rafael Aquini <aquini@redhat.com> - 20160728-48.git0daeaf3
- Clean up t{4,5}fw.bin symlink leftovers from bug 1262128 (rhbz 1365763)

* Thu Jul 28 2016 Rafael Aquini <aquini@redhat.com> - 20160728-47.git0daeaf3
- Update to latest upstream linux-firmware image for assorted updates
- Core14 firmware release for 7265, 7260, 3160 wireless devices (rhbz 1358566)
- Core19 firmware release for 7265D, 8260, 8265, 3168 wireless devices (rhbz 1358568)

* Wed Jun 15 2016 Rafael Aquini <aquini@redhat.com> - 20160615-46.gita4bbc81
- Update to latest upstream linux-firmware image for assorted updates
- Nuke old cxgb4 firmware blobs tarball (rhbz 1336906)
- Update to latest Chelsio firmware release version (rhbz 1336906)

* Tue Apr 26 2016 Rafael Aquini <aquini@redhat.com> - 20160426-45.git986a5a8
- Update to latest upstream linux-firmware image for assorted updates
- Restore AMD-ucode firmware blob again (rhbz 866700)
- qat: Update to the latest upstream firmware (rhbz 1173792)
- WiFi on Windstorm Peak Wireless Adapter 8265 - firmware support (rhbz 1315534)
- WiFi on Sandy Peak Wireless Adapter 3168 - firmware support (rhbz 1315536)

* Thu Mar 10 2016 Rafael Aquini <aquini@redhat.com> - 20160223-44.git8d1fd61
- Update to latest upstream linux-firmware image for assorted updates
- Update Intel Omni-Path Architecture hfi1 Firmware (rhbz 1267015)

* Mon Sep 14 2015 Jarod Wilson <jarod@redhat.com> - 20150904-43.git6ebf5d5
- Add more old chelsio firmwares, they nuked the one the driver in
  RHEL7.2 is expecting from upstream (rhbz 1262128)
- Remove amd-ucode again, it simply breaks too many systems (rhbz 1246393)

* Fri Sep 04 2015 Rafael Aquini <aquini@redhat.com> - 20150904-42.git6ebf5d5
- Add Intel Omni-Path Architecture hfi1 Firmware (rhbz 1194910)
- Update skl firmware for gpu (rhbz 1210012)

* Wed Aug 12 2015 Rafael Aquini <aquini@redhat.com> - 20150727-41.git75cc3ef
- Declare obsolecence for (old) removed firmware subpackages (rhbz 1232315)

* Mon Jul 27 2015 Rafael Aquini <aquini@redhat.com> - 20150727-40.git75cc3ef
- Add firmware support for the "Snowfield Peak" wireless adapter (rhbz 1169604)

* Tue Jul 21 2015 Rafael Aquini <aquini@redhat.com> - 20150612-39.git3161bfa
- Restore AMD-ucode firmware blob again (rhbz 1016168)

* Thu Jun 18 2015 Rafael Aquini <aquini@redhat.com> - 20150612-38.git3161bfa
- Reintroduce upstream nuked cxgb4 firmware old blobs (rhbz 1189256)

* Fri Jun 12 2015 Rafael Aquini <aquini@redhat.com> - 20150612-37.git3161bfa
- Update to latest upstream linux-firmware image for assorted updates
- cxgb4: Update firmware to revision 1.13.32.0 (rhbz 1189256)
- qat: Update to the latest upstream firmware (rhbz 1173792)
- Use a common version number for both the iwl*-firmware packages and linux-firmware itself

* Thu Sep 11 2014 Jarod Wilson <jarod@redhat.com> - 20140911-0.1.git365e80c
- Update to latest upstream linux-firmware image for assorted updates
- Adds Intel Quick Assist Technology firmware (rhbz 1127338)
- Updates bnx2x adapter firmware to version 7.10.51 (rhbz 1089403)
- Updates myri10ge adapter firmware to version 1.4.57 (rhbz 1063702)
- Removes firmware for drivers not shipped with Red Hat Enterprise Linux 7 (rhbz 1016595)

* Mon Aug 04 2014 Jarod Wilson <jarod@redhat.com> - 20140804-0.1.git6bce2b0
- Update to latest linux-firmware to pick up new Qlogic firmware (rhbz 1089364)

* Mon Mar 24 2014 Prarit Bhargava <prarit@redhat.com> - 20140213-0.3.git4164c23
- Revert AMD firmware update again (rhbz 1079114)

* Tue Mar 11 2014 Jarod Wilson <jarod@redhat.com> - 20140213-0.2.git4164c23
- Restore amd-ucode (rhbz 866700)

* Thu Feb 13 2014 Jarod Wilson <jarod@redhat.com> - 20140213-0.1.git4164c23
- Add bnx2x FW 7.8.19 to fix FCoE on 4-port cards (rhbz 1061351)

* Tue Jan 07 2014 Jarod Wilson <jarod@redhat.com> - 20140102-0.2.git52d77db
- Fix Obsoletes for iwl100-firmware (rhbz 1035459)

* Thu Jan 02 2014 Jarod Wilson <jarod@redhat.com> - 20140102-0.1.git52d77db
- Update to latest linux-firmware to pick up new Brocade firmware (rhbz 1030677)

* Fri Dec 27 2013 Daniel Mach <dmach@redhat.com> - 20131106-0.4.git7d0c7a8
- Mass rebuild 2013-12-27

* Tue Dec 03 2013 Jarod Wilson <jarod@redhat.com> - 20131106-0.3.git7d0c7a8
- Add new fwdir define for /usr/lib/firmware and use it (rhbz 884107)

* Thu Nov 14 2013 Jarod Wilson <jarod@redhat.com> - 20131106-0.2.git7d0c7a8
- Temporarily add old brocade firmwares to work with not-yet-updated bfa driver (rhbz 1030532)

* Wed Nov 06 2013 Jarod Wilson <jarod@redhat.com> - 20131106-0.1.git7d0c7a8
- Update to latest upstream linux-firmware to pick up bfa firmware (rhbz 1013426)
- Fix up Obsoletes to all use <= comparisons

* Fri Sep 20 2013 Jarod Wilson <jarod@redhat.com> - 20130820-0.4.git600caef
- Drop amd-ucode to avoid bug 1007411 for now

* Tue Aug 20 2013 Jarod Wilson <jarod@redhat.com> - 20130820-0.3.git600caef
- Put in proper URL and URL-less Source0 location, and a note about how
  we generate the tarball by hand from a git tree

* Tue Aug 20 2013 Jarod Wilson <jarod@redhat.com> - 20130820-0.2.git600caef
- Fix up build breakage from split nvr for iwlwifi

* Tue Aug 20 2013 Jarod Wilson <jarod@redhat.com> - 20130820-0.1.git600caef
- Update to latest upstream git tree

* Thu Apr 18 2013 Josh Boyer <jwboyer@redhat.com> - 20130418-0.1.gitb584174
- Update to latest upstream git tree

* Tue Mar 19 2013 Josh Boyer <jwboyer@redhat.com>
- Own the firmware directories (rhbz 919249)

* Thu Feb 21 2013 Josh Boyer <jwboyer@redhat.com> - 20130201-0.4.git65a5163
- Obsolete netxen-firmware.  Again.  (rhbz 913680)

* Mon Feb 04 2013 Josh Boyer <jwboyer@redhat.com> - 20130201-0.3.git65a5163
- Obsolete ql2[45]00-firmware packages (rhbz 906898)
 
* Fri Feb 01 2013 Josh Boyer <jwboyer@redhat.com> 
- Update to latest upstream release
- Provide firmware for carl9170 (rhbz 866051)

* Wed Jan 23 2013 Ville Skyttä <ville.skytta@iki.fi> - 20121218-0.2.gitbda53ca
- Own subdirs created in /lib/firmware (rhbz 902005)

* Wed Jan 23 2013 Josh Boyer <jwboyer@redhat.com>
- Correctly obsolete the libertas-usb8388-firmware packages (rhbz 902265)

* Tue Dec 18 2012 Josh Boyer <jwboyer@redhat.com>
- Update to latest upstream.  Adds brcm firmware updates

* Wed Oct 10 2012 Josh Boyer <jwboyer@redhat.com>
- Consolidate rt61pci-firmware and rt73usb-firmware packages (rhbz 864959)
- Consolidate netxen-firmware and ql2[123]xx-firmware packages (rhbz 864959)

* Tue Sep 25 2012 Josh Boyer <jwboyer@redhat.com>
- Update to latest upstream.  Adds marvell wifi updates (rhbz 858388)

* Tue Sep 18 2012 Josh Boyer <jwboyer@redhat.com>
- Add patch to create libertas subpackages from Daniel Drake (rhbz 853198)

* Fri Sep 07 2012 Josh Boyer <jwboyer@redhat.com> 20120720-0.2.git7560108
- Add epoch to iwl1000 subpackage to preserve upgrade patch (rhbz 855426)

* Fri Jul 20 2012 Josh Boyer <jwboyer@redhat.com> 20120720-0.1.git7560108
- Update to latest upstream.  Adds more realtek firmware and bcm4334

* Tue Jul 17 2012 Josh Boyer <jwboyer@redhat.com> 20120717-0.1.gitf1f86bb
- Update to latest upstream.  Adds updated realtek firmware

* Thu Jun 07 2012 Josh Boyer <jwboyer@redhat.com> 20120510-0.5.git375e954
- Bump release to get around koji

* Thu Jun 07 2012 Josh Boyer <jwboyer@redhat.com> 20120510-0.4.git375e954
- Drop udev requires.  Systemd now provides udev

* Tue Jun 05 2012 Josh Boyer <jwboyer@redhat.com> 20120510-0.3.git375e954
- Fix location of BuildRequires so git is inclued in the buildroot
- Create iwlXXXX-firmware subpackages (rhbz 828050)

* Thu May 10 2012 Josh Boyer <jwboyer@redhat.com> 20120510-0.1.git375e954
- Update to latest upstream.  Adds new bnx2x and radeon firmware

* Wed Apr 18 2012 Josh Boyer <jwboyer@redhat.com> 20120418-0.1.git85fbcaa
- Update to latest upstream.  Adds new rtl and ath firmware

* Wed Mar 21 2012 Dave Airlie <airlied@redhat.com> 20120206-0.3.git06c8f81
- use git to apply the radeon firmware

* Wed Mar 21 2012 Dave Airlie <airlied@redhat.com> 20120206-0.2.git06c8f81
- add radeon southern islands/trinity firmware

* Tue Feb 07 2012 Josh Boyer <jwboyer@redhat.com> 20120206-0.1.git06c8f81
- Update to latest upstream git snapshot.  Fixes rhbz 786937

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20110731-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Thu Aug 04 2011 Tom Callaway <spot@fedoraproject.org> 20110731-2
- resolve conflict with netxen-firmware

* Wed Aug 03 2011 David Woodhouse <dwmw2@infradead.org> 20110731-1
- Latest firmware release with v1.3 ath9k firmware (#727702)

* Sun Jun 05 2011 Peter Lemenkov <lemenkov@gmail.com> 20110601-2
- Remove duplicated licensing files from /lib/firmware

* Wed Jun 01 2011 Dave Airlie <airlied@redhat.com> 20110601-1
- Latest firmware release with AMD llano support.

* Thu Mar 10 2011 Dave Airlie <airlied@redhat.com> 20110304-1
- update to latest upstream for radeon ni/cayman, drop nouveau fw we don't use it anymore

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20110125-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Tue Jan 25 2011 David Woodhouse <dwmw2@infradead.org> 20110125-1
- Update to linux-firmware-20110125 (new bnx2 firmware)

* Fri Jan 07 2011 Dave Airlie <airlied@redhat.com> 20101221-1
- rebase to upstream release + add new radeon NI firmwares.

* Thu Aug 12 2010 Hicham HAOUARI <hicham.haouari@gmail.com> 20100806-4
- Really obsolete ueagle-atm4-firmware

* Thu Aug 12 2010 Hicham HAOUARI <hicham.haouari@gmail.com> 20100806-3
- Obsolete ueagle-atm4-firmware

* Fri Aug 06 2010 David Woodhouse <dwmw2@infradead.org> 20100806-2
- Remove duplicate radeon firmwares; they're upstream now

* Fri Aug 06 2010 David Woodhouse <dwmw2@infradead.org> 20100806-1
- Update to linux-firmware-20100806 (more legacy firmwares from kernel source)

* Fri Apr 09 2010 Dave Airlie <airlied@redhat.com> 20100106-4
- Add further radeon firmwares

* Wed Feb 10 2010 Dave Airlie <airlied@redhat.com> 20100106-3
- add radeon RLC firmware - submitted upstream to dwmw2 already.

* Tue Feb 09 2010 Ben Skeggs <bskeggs@redhat.com> 20090106-2
- Add firmware needed for nouveau to operate correctly (this is Fedora
  only - do not upstream yet - we just moved it here from Fedora kernel)

* Wed Jan 06 2010 David Woodhouse <David.Woodhouse@intel.com> 20090106-1
- Update

* Fri Aug 21 2009 David Woodhouse <David.Woodhouse@intel.com> 20090821-1
- Update, fix typos, remove some files which conflict with other packages.

* Thu Mar 19 2009 David Woodhouse <David.Woodhouse@intel.com> 20090319-1
- First standalone kernel-firmware package.
