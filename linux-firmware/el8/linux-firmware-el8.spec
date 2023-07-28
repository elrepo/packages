### ELRepo -- begin
###
### MAIN is b6ea35ff6b9869470a0c68813f1668acb3d356a8
### CHECKOUT is b6ea35ff
### DSTAMP is 20230725
###
### linux-firmware-20230725.tar.xz
###
### ELRepo -- end

%global checkout b6ea35ff

%global firmware_release 110.2

%global _firmwarepath	/usr/lib/firmware

%define _binaries_in_noarch_packages_terminate_build 0

Name:		linux-firmware
Version:	20230725
Release:	%{firmware_release}.git%{checkout}%{?dist}
Summary:	Firmware files used by the Linux kernel
License:	GPL+ and GPLv2+ and MIT and Redistributable, no modification permitted
URL:		https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/
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
Obsoletes:	kernel-firmware < %{version} xorg-x11-drv-ati-firmware < 6.13.0-0.22
Obsoletes:	ueagle-atm4-firmware < 1.0-5
Obsoletes:	netxen-firmware < 4.0.534-9
Obsoletes:	ql2100-firmware < 1.19.38-8
Obsoletes:	ql2200-firmware < 2.02.08-8
Obsoletes:	ql23xx-firmware < 3.03.28-6
Obsoletes:	ql2400-firmware < 5.08.00-2
Obsoletes:	ql2500-firmware < 5.08.00-2
Obsoletes:	rt61pci-firmware < 1.2-11
Obsoletes:	rt73usb-firmware < 1.8-11
Obsoletes:	cx18-firmware < 20080628-10
Conflicts:	microcode_ctl < 2.1-0
# ivtv-firmware also provided the file v4l-cx25840.fw in older releases until
# version 2:20080701-28 in Fedora, when this conflicting file was removed in
# favour of the same file provided by linux-firmware. Fedora dropped the
# Obsoletes (see Fedora bugs 1211055, 1232773). RHEL 8 doesn't build so far any
# ivtv-firmware version, and we need to obsolete the conflicting versions to
# avoid upgrade errors (see bug 1589055)
Obsoletes:	ivtv-firmware < 2:20080701-28

BuildRequires:	make

%description
This package provides firmware files required for some devices to operate.

%package -n iwl100-firmware
Summary:	Firmware for Intel(R) Wireless WiFi Link 100 Series Adapters
License:	Redistributable, no modification permitted
Version:	39.31.5.1
Release:	%{firmware_release}.2%{?dist}
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
Release:	%{firmware_release}.2%{?dist}
%description -n iwl105-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux to support the iwl105 hardware.  Usage of the firmware
is subject to the terms and conditions contained inside the provided
LICENSE file. Please read it carefully.

%package -n iwl135-firmware
Summary:	Firmware for Intel(R) Centrino Wireless-N 135 Series Adapters
License:	Redistributable, no modification permitted
Version:	18.168.6.1
Release:	%{firmware_release}.2%{?dist}
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
Release:	%{firmware_release}.2%{?dist}
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
Release:	%{firmware_release}.2%{?dist}
%description -n iwl2000-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux to support the iwl2000 hardware.  Usage of the firmware
is subject to the terms and conditions contained inside the provided
LICENSE file. Please read it carefully.

%package -n iwl2030-firmware
Summary:	Firmware for Intel(R) Centrino Wireless-N 2030 Series Adapters
License:	Redistributable, no modification permitted
Version:	18.168.6.1
Release:	%{firmware_release}.2%{?dist}
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
Release:	%{firmware_release}.2%{?dist}
%description -n iwl3160-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl3945-firmware
Summary:	Firmware for Intel® PRO/Wireless 3945 A/B/G network adaptors
License:	Redistributable, no modification permitted
Version:	15.32.2.9
Release:	%{firmware_release}.2%{?dist}
Obsoletes:	iwl3945-firmware < 15.32.2.9-7
%description -n iwl3945-firmware
This package contains the firmware required by the iwl3945 driver
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl4965-firmware
Summary:	Firmware for Intel® PRO/Wireless 4965 A/G/N network adaptors
License:	Redistributable, no modification permitted
Version:	228.61.2.24
Release:	%{firmware_release}.2%{?dist}
Obsoletes:	iwl4965-firmware < 228.61.2.24-5
%description -n iwl4965-firmware
This package contains the firmware required by the iwl4965 driver
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl5000-firmware
Summary:	Firmware for Intel® PRO/Wireless 5000 A/G/N network adaptors
License:	Redistributable, no modification permitted
Version:	8.83.5.1_1
Release:	%{firmware_release}.2%{?dist}
Obsoletes:	iwl5000-firmware < 8.83.5.1_1-3
%description -n iwl5000-firmware
This package contains the firmware required by the iwl5000 driver
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl5150-firmware
Summary:	Firmware for Intel® PRO/Wireless 5150 A/G/N network adaptors
License:	Redistributable, no modification permitted
Version:	8.24.2.2
Release:	%{firmware_release}.2%{?dist}
Obsoletes:	iwl5150-firmware < 8.24.2.2-4
%description -n iwl5150-firmware
This package contains the firmware required by the iwl5150 driver
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl6000-firmware
Summary:	Firmware for Intel(R) Wireless WiFi Link 6000 AGN Adapter
License:	Redistributable, no modification permitted
Version:	9.221.4.1
Release:	%{firmware_release}.2%{?dist}
Obsoletes:	iwl6000-firmware < 9.221.4.1-4
%description -n iwl6000-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl6000g2a-firmware
Summary:	Firmware for Intel(R) Wireless WiFi Link 6005 Series Adapters
License:	Redistributable, no modification permitted
Version:	18.168.6.1
Release:	%{firmware_release}.2%{?dist}
Obsoletes:	iwl6000g2a-firmware < 17.168.5.3-3
%description -n iwl6000g2a-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl6000g2b-firmware
Summary:	Firmware for Intel(R) Wireless WiFi Link 6030 Series Adapters
License:	Redistributable, no modification permitted
Version:	18.168.6.1
Release:	%{firmware_release}.2%{?dist}
Obsoletes:	iwl6000g2b-firmware < 17.168.5.2-3
%description -n iwl6000g2b-firmware
This package contains the firmware required by the Intel wireless drivers
for Linux.  Usage of the firmware is subject to the terms and conditions
contained inside the provided LICENSE file. Please read it carefully.

%package -n iwl6050-firmware
Summary:	Firmware for Intel(R) Wireless WiFi Link 6050 Series Adapters
License:	Redistributable, no modification permitted
Version:	41.28.5.1
Release:	%{firmware_release}.2%{?dist}
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
Release:	%{firmware_release}.2%{?dist}
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

# Remove obsolete and password-protected vgxe firmware (see bug 2108051)
rm -rf vxge

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
sed -i -e 's!^!/usr/lib/firmware/!' linux-firmware.{files,dirs}
sed -i -e 's/^/"/;s/$/"/' linux-firmware.files
sed -e 's/^/%%dir /' linux-firmware.dirs >> linux-firmware.files

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
%{_firmwarepath}/iwlwifi-so-a0-*.ucode
%{_firmwarepath}/iwlwifi-so-a0-*.pnvm

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

%changelog
* Wed Jul 26 2023 Akemi Yagi <toracat@elrepo.org> - 20230725-110.2.gitb6ea35ff
- Updated to the latest from the upstream linux-firmware git tree at
- kernel.org as of this date. [https://elrepo.org/bugs/view.php?id=1374]

* Fri Mar 17 2023 Alan Bartlett <ajb@elrepo.org> -- 20230317-110.1gitc761dbe8
- Updated to the latest from the upstream linux-firmware git tree at
- kernel.org as of this date. [https://elrepo.org/bugs/view.php?id=1329]

* Tue Jul 26 2022 Jarod Wilson <jarod@redhat.com> - 20220726-110.git150864a4
- Omit unused password-protected vxge firmware files from package (rhbz 2108051)
- Pick up latest AMD GPU firmwares

* Wed Jul 13 2022 Jarod Wilson <jarod@redhat.com> - 20220713-109.gitdfa29317
- Update to latest upstream linux-firmware image for assorted updates
- Include even newer qed firmware update (rhbz 2040269)

* Fri Jun 03 2022 Jarod Wilson <jarod@redhat.com> - 20220517-108.git251d2900
- Bump NVR due to conflict with late 8.6 iwl firmware package versioning

* Wed May 18 2022 Jarod Wilson <jarod@redhat.com> - 20220517-107.git251d2900
- Update to latest upstream linux-firmware image for assorted updates
- Include latest qed firmware update (rhbz 2040269)
- Include latest bnx2x firmware update (rhbz 2040273)
- Include latest ice firmware update (rhbz 2059384, rhbz 2081543)
- Include latest Qualcomm firmware update (rhbz 2062871)

* Thu Feb 10 2022 Augusto Caringi <acaringi@redhat.com> - 20220209-106.git6342082c
- Update to latest upstream linux-firmware image for assorted updates
- Include AMD GPU firmware fix (rhbz 2031172)
- Support QCA WCN6856 v2.1 Module: Bluetooth firmware (rhbz 2007904)

* Mon Dec 06 2021 Bruno Meneguele <bmeneg@redhat.com> - 20211119-105.gitf5d51956
- Update to latest upstream linux-firmware image for assorted updates
- Update to latest AMD GPU firmware (rhbz 1986660)

* Thu Oct 07 2021 Bruno Meneguele <bmeneg@redhat.com> - 20211007-104.git7a300505
- Update to latest upstream linux-firmware image for assorted updates
- Certain amdgpu firmware files cause random hangs with AMD Picasso/Raven Ridge APUs (rhbz 2000879)
- Support Intel CNVi AX211 Garfield Peak2 on ADL - Bluetooth (rhbz 1923179)
- Support Intel CNVi AX211 Garfield Peak2 on ADL - WIFI (rhbz 1923175)

* Fri Jul 02 2021 Bruno Meneguele <bmeneg@redhat.com> - 20210702-103.gitd79c2677
- Update to latest upstream linux-firmware image for assorted updates
- Include support for the Qualcomm AX500-DBS (QCA6390) 802.11ax Wireless (rhbz 1725913)
- Firmware for Chelsio T4/T5/T6 adapters (rhbz 1961398)
- Incorrect NVIDIA GPU firmware used for TU102 and TU104 GPUs (rhbz 1965312)
- Package the new iwl7260 ucode files and also the new binary format PNVM

* Wed Jan 27 2021 Jan Stancek <jstancek@redhat.com> - 20201218-102.git05789708
- Update to latest upstream linux-firmware image for assorted updates (rhbz 1918613)

* Fri Dec 04 2020 Jan Stancek <jstancek@redhat.com> - 20201118-101.git7455a360
- Update to latest upstream linux-firmware image for assorted updates
- Include to support Intel AX210 Typhoon Peak - Bluetooth firmware support (rhbz 1897069)
- Include support for the Qualcomm AX500-DBS (QCA6390) 802.11ax Wireless - firmware support (rhbz 1725913)

* Thu Nov 05 2020 Jan Stancek <jstancek@redhat.com> - 20201022-100.gitdae4b4cd
- Update to latest upstream linux-firmware image for assorted updates
- ice: Pull Comms Market Segment Package (rhbz 1861491)
- ice: Update to the Default OS DDP Package for ice driver (rhbz 1879278)

* Fri Jun 19 2020 Frantisek Hrbata <fhrbata@redhat.com> - 20200619-99.git3890db36
- Update to latest upstream linux-firmware image for assorted updates (rhbz 1847992)

* Tue May 12 2020 Frantisek Hrbata <fhrbata@redhat.com> - 20200512-98.gitb2cad6a2
- Update to latest upstream linux-firmware image for assorted updates
- Update qed zipped/unzipped firmware to latest upstream (rhbz 1791030)
- Update bnx2x firmware to latest upstream (rhbz 1791031)
- Firmware - nvidia: add TU102/TU104/TU106 / TU116/117 signed firmware (rhbz 1801025)
- Update latest Signed FW for AMD NAVI 10 Graphic (rhbz 1809846)

* Thu Dec 12 2019 Bruno Meneguele <bmeneg@redhat.com> - 20191202-97.gite8a0f4c9
- firmware files marked as configuration files (rhbz 1782329)

* Mon Dec 02 2019 Bruno Meneguele <bmeneg@redhat.com> - 20191202-96.gite8a0f4c9
- Fix .files file dir, wrongly modified in the last release

* Mon Dec 02 2019 Bruno Meneguele <bmeneg@redhat.com> - 20191202-95.gite8a0f4c9
- Update to latest upstream linux-firmware image for assorted updates
- Update qed zipped/unzipped firmware to latest upstream (rhbz 1720394)
- Update bnx2x firmware to latest upstream (rhbz 1720395)
- AX200(Cyclone Peak 2 or CcP2) Bluetooth support (rhbz 1722693)
- Firmware for Chelsio T4/T5/T6 adapters (rhbz 1725821)
- Include to support Intel AX201 Harrison Peak(CNVi) on CML-U (rhbz 1725873)
- Icelake Bluetooth firmware update (AX201, Harrison Peak, or HrP2) (rhbz 1726182)
- DDP Package to support Columbiaville NIC (rhbz 1726454)
- Add iwlwifi-cc-a0-48.ucode to linux-firmware (rhbz 1753962)
- Change install section to use usptream installation process

* Thu May 16 2019 Herton R. Krzesinski <herton@redhat.com> - 20190516-94.git711d3297
- Update to latest upstream linux-firmware image for assorted updates
- Update iwlwifi firmware to support the Cyclone Peak Dual Band 2x2 802.11ax (rhbz 1622442)
- Pull new version of Netronome firmware (rhbz 1663989)
- Update firmware for Chelsio T4/T5/T6 adapters (rhbz 1664678)
- Update Netronome firmware that include bug fixes (rhbz 1673890)
- Include firmware support for Intel Bluetooth 22261 (rhbz 1678095)

* Fri Mar 29 2019 Herton R. Krzesinski <herton@redhat.com> - 20190111-93.gitd9fb2ee6
- Remove cxgb3 (T3 adapter) firmware (rhbz 1503721)

* Fri Jan 11 2019 Herton R. Krzesinski <herton@redhat.com> - 20190111-92.gitd9fb2ee6
- Update linux-firmware package for bugfixes for Netronome (rhbz 1657892)

* Tue Nov 20 2018 Herton R. Krzesinski <herton@redhat.com> - 20181109-91.git1baa3486
- Make netronome/nic_AMDA* symbolic links be config noreplace files (Pablo Cascón) [1644273]

* Fri Nov 09 2018 Herton R. Krzesinski <herton@redhat.com> - 20181109-90.git1baa3486
- Update to latest upstream linux-firmware image for assorted updates
- Include BPF version of Netronome firmware (rhbz 1637558)

* Mon Oct 15 2018 Herton R. Krzesinski <herton@redhat.com> - 20181015-89.gitc6b6265d
- Remove liquidio/lio_23xx_vsw.bin from linux-firmware until GPL violation is
  fully resolved (rhbz 1637694)

* Thu Oct 11 2018 Herton R. Krzesinski <herton@redhat.com> - 20181011-88.gitc6b6265d
- Update to latest upstream linux-firmware image for assorted updates
- Include latest firmware for Nvidia Graphics (rhbz 1504667)

* Tue Jul 17 2018 Herton R. Krzesinski <herton@redhat.com> - 20180717-87.git8d69bab7
- Update to latest upstream linux-firmware image for assorted updates
- cxgb4: update firmware to revision 1.20.8.0 (rhbz 1503622)

* Fri Jul 06 2018 Herton R. Krzesinski <herton@redhat.com> - 20180706-86.gitd1147327
- Update to latest upstream linux-firmware image for assorted updates
- qed: Add firmware 8.37.2.0 (rhbz 1584853)

* Mon Jun 11 2018 Herton R. Krzesinski <herton@redhat.com> - 20180524-85.git2a9b2cf5
- Add Obsoletes for conflicting ivtv-firmware versions (rhbz 1589055)
- Add Obsoletes for iwl7265-firmware which is shipped with RHEL 7 (rhbz 1589056)

* Thu May 24 2018 Herton R. Krzesinski <herton@redhat.com> - 20180524-84.git2a9b2cf5
- Update to latest upstream linux-firmware image for assorted updates
- Update Intel Omni-Path Architecture (OPA) Firmware (rhbz 1483737)
- Drop separate microcode_amd_fam17h.bin, same file is now included in linux-firmware
  upstream

* Mon Apr 02 2018 Josh Boyer <jwboyer@fedoraproject.org> - 20180402-83.git8c1e439c
- Latest upstream snapshot

* Fri Feb 09 2018 Igor Gnatenko <ignatenkobrain@fedoraproject.org> - 20171215-82.git2451bb22.1
- Escape macros in %%changelog

* Fri Jan 05 2018 Josh Boyer <jwboyer@fedoraproject.org> 20171215-92.git2451bb22
- Add amd-ucode for fam17h

* Fri Dec 15 2017 Josh Boyer <jwboyer@fedoraproject.org> 20171215-81.git2451bb22
- Updated skl DMC, cnl audio, netronome SmartNIC, amdgpu vega10 and raven,
  intel bluetooth, brcm CYW4373, and liquidio vswitch firmwares

* Sun Nov 26 2017 Josh Boyer <jwboyer@fedoraproject.org> 20171126-80.git17e62881
- Updated bcm 4339 4354 4356 4358 firmware, new bcm 43430
- Fixes CVE-2016-0801 CVE-2017-0561 CVE-2017-9417

* Thu Nov 23 2017 Peter Robinson <pbrobinson@fedoraproject.org> 20171123-79.git90436ce
- Updated Intel GPU, amdgpu, iwlwifi, mvebu wifi, liquidio, QCom a530 & Venus, mlxsw, qed
- Add iwlwifi 9000 series

* Wed Oct 11 2017 Peter Robinson <pbrobinson@fedoraproject.org> 20171009-78.gitbf04291
- Updated cxgb4, QCom gpu, Intel OPA IB, amdgpu, rtlwifi
- Ship the license in %%license for all sub packages
- Modernise spec

* Mon Sep 18 2017 Josh Boyer <jwboyer@fedoraproject.org> - 20170828-77.gitb78acc9
- Add patches to fix ath10k regression (rhbz 1492161)

* Mon Aug 28 2017 Josh Boyer <jwboyer@fedoraproject.org> - 20170828-76.gitb78acc9
- Update to latest upstream snapshot
- ath10k, iwlwifi, kabylake, liquidio, amdgpu, and cavium crypot updates

* Thu Jun 22 2017 Josh Boyer <jwboyer@fedoraproject.org> - 20170622-75.gita3a26af2
- Update to latest upstream snapshot
- imx, qcom, and tegra ARM related updates

* Mon Jun 05 2017 Josh Boyer <jwboyer@fedoraproject.org> - 20170605-74.git37857004
- Update to latest upstream snapshot

* Wed Apr 19 2017 Josh Boyer <jwboyer@fedoraproject.org> - 20170419-73.gitb1413458
- Update to the latest upstream snapshot
- New nvidia, netronome, and marvell firmware
- Updated intel audio firmware

* Mon Mar 13 2017 Josh Boyer <jwboyer@fedoraproject.org> - 20170313-72.git695f2d6d
- Update to the latest upstream snapshot
- New nvidia, AMD, and i915 GPU firmware
- Updated iwlwifi and intel bluetooth firmware

* Mon Feb 13 2017 Josh Boyer <jwboyer@fedoraproject.org> - 20170213-71.git6d3bc888
- Update to the latest upstream snapshot

* Wed Feb 01 2017 Stephen Gallagher <sgallagh@redhat.com> - 20161205-70.git91ddce49
- Add missing %%license macro

* Mon Dec 05 2016 Josh Boyer <jwboyer@fedoraproject.org> 20161205-69.git91ddce49
- Update to the latest upstream snapshot
- New intel bluetooth and rtlwifi firmware

* Fri Sep 23 2016 Josh Boyer <jwboyer@fedoraproject.org> 20160923-68.git42ad5367
- Update to the latest upstream snapshot
- ath10k, amdgpu, mediatek, brcm, marvell updates
 
* Tue Aug 16 2016 Josh Boyer <jwboyer@fedoraproject.org> 20160816-67.git7c3dfc0b
- Update to the latest upstream snapshot (rhbz 1367203)
- Intel audio, rockchip, amdgpu, iwlwifi, nvidia pascal updates

* Thu Jun 09 2016 Josh Boyer <jwboyer@fedoraproject.org> 20160609-66.gita4bbc811
- Update to the latest upstream snapshot
- Intel bluetooth, radeon smc, Intel braswell/broxton audio, cxgb4 updates

* Thu May 26 2016 Josh Boyer <jwboyer@fedoraproject.org> 20160526-65.git80d463be
- Update to the latest upstream snapshot
- amdgpu, Skylake audio, and rt2xxx wifi firmware updates

* Thu May 05 2016 Josh Boyer <jwboyer@fedoraproject.org> 20160505-64.git8afadbe5
- Update to the latest upstream snapshot
- AMD, intel, and QCA6xxx updates (rhbz 1294263)

* Mon Mar 21 2016 Josh Boyer <jwboyer@fedoraproject.org> 20160321-63.git5f8ca0c
- Update to latest upstream snapshot
- New Skylake GuC and audio firmware, AMD ucode updates

* Wed Mar 16 2016 Josh Boyer <jwboyer@fedoraproject.org> 20160316-62.gitdeb1d836
- Update to latest upstream snapshot
- New firmware for iwlwifi 3168, 7265D, 8000C, and 8265 devices

* Thu Feb 04 2016 Josh Boyer <jwboyer@fedoraproject.org> 20160204-61.git91d5dd13
- Update to latest upstream snashot
- rtlwifi, iwlwifi, intel bluetooth, skylake audio updates

* Thu Feb 04 2016 Fedora Release Engineering <releng@fedoraproject.org> - 20151214-60.gitbbe4917c.1
- Rebuilt for https://fedoraproject.org/wiki/Fedora_24_Mass_Rebuild

* Mon Dec 14 2015 Josh Boyer <jwboyer@fedoraproject.org> 20151214-60.gitbbe4917c
- Update to latest upstream snapshot
- Includes firmware for mt7601u (rhbz 1264631)

* Mon Nov 30 2015 Josh Boyer <jwboyer@fedoraproject.org> 20151130-59.gita109a8ff
- Update to latest upstream snapshot
- Includes -16 ucode for iwlwifi, skylake dmc and audio updates, brcm updates
  bnx2x, and others

* Fri Oct 30 2015 Josh Boyer <jwboyer@fedoraproject.org> 20151030-58.git66d3d8d7
- Update to latest upstream snapshot
- Includes ath10k and mwlwifi firmware updates (rhbz 1276360)

* Mon Oct 12 2015 Josh Boyer <jwboyer@fedoraproject.org> 20151012-57.gitd82d3c1e
- Update to latest upstream snapshot
- Includes skylake and intel bluetooth firmware updates

* Fri Sep 04 2015 Josh Boyer <jwboyer@fedoraproject.org> 20150904-56.git6ebf5d57
- Update to latest upstream git snapshot
- Includes amdgpu firmware and skylake updates

* Thu Sep 03 2015 Josh Boyer <jwboyer@fedoraproject.org> 20150903-55.git38358cfc
- Add firmware from Alex Deucher for amdgpu driver (rhbz 1259542)

* Thu Sep 03 2015 Josh Boyer <jwboyer@fedoraproject.org>
- Update to latest upstream git snapshot
- Updates for nvidia, bnx2x, and atmel devices

* Wed Jul 15 2015 Josh Boyer <jwboyer@fedoraproject.org> 20150715-54.git69640304
- Update to latest upstream git snapshot
- New iwlwifi firmware for 726x/316x/8000 devices
- New firmware for i915 skylake and radeon devices
- Various other updates

* Tue Jun 23 2015 Josh Boyer <jwboyer@fedoraproject.org> 20150521-53.git3161bfa4
- Don't obsolete ivtv-firmware any longer (rhbz 1232773)

* Wed Jun 17 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20150521-52.git3161bfa4.1
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Thu May 21 2015 Josh Boyer <jwboyer@fedoraproject.org> 20150521-52.git3161bfa4
- Update to latest upstream git snapshot
- Updated iwlwifi 316x/726x firmware
- Add cx18-firmware Obsoletes from David Ward (rhbz 1222164)

* Wed May 06 2015 Josh Boyer <jwboyer@fedoraproject.org> 20150415-51.gitec89525b
- Obsoletes ivtv-firmware (rbhz 1211055)

* Fri May 01 2015 Josh Boyer <jwboyer@fedoraproject.org> 20150415-50.gitec89525b
- Add v4l-cx25840.fw back now that ivtv-firmware is retired (rhbz 1211055)

* Tue Apr 14 2015 Josh Boyer <jwboyer@fedoraproject.org> 20150415-49.gitec89525b
- Fix conflict with ivtv-firmware (rhbz 1203385)

* Fri Apr 10 2015 Josh Boyer <jwboyer@fedoraproject.org> 20150415-47.gitec89525b
- Update to the latest upstream git snapshot

* Thu Mar 19 2015 Josh Boyer <jwboyer@fedoraproject.org>
- Ship the cx18x firmware files (rhbz 1203385)

* Mon Mar 16 2015 Josh Boyer <jwboyer@fedoraproject.org> 20150316-46.git020e534e
- Update to latest upstream git snapshot

* Fri Feb 13 2015 Josh Boyer <jwboyer@fedoraproject.org> 20150213-45.git17657c35
- Update to latest upstream git snapshot
- Firmware for Surface Pro 3 WLAN/Bluetooth (rhbz 1185804)

* Thu Jan 15 2015 Josh Boyer <jwboyer@fedoraproject.org> 20150115-44.git78535e88.fc22
- Update to latest upstream git snapshot
- Adjust iwl{3160,7260} version numbers (rhbz 1167695)

* Tue Oct 14 2014 Josh Boyer <jwboyer@fedoraproject.org> 20141013-43.git0e5f6377.fc22
- Fix 3160/7260 version numbers (rhbz 1110522)

* Mon Oct 13 2014 Josh Boyer <jwboyer@fedoraproject.org> 20141013-42.git0e5f6377.fc22
- Update to latest upstream git snapshot

* Fri Sep 12 2014 Josh Boyer <jwboyer@fedoraproject.org> 20140912-41.git365e80cce.fc22
- Update to the latest upstream git snapshot

* Thu Aug 28 2014 Josh Boyer <jwboyer@fedoraproject.org>
- Update to latest upstream git snapshot for new radeon firmware (rhbz 1130738)
- Fix versioning after mass rebuild and for iwl5000-firmware (rhbz 1130979)

* Fri Aug 08 2014 Kyle McMartin <kyle@fedoraproject.org> 20140808-39.gitce64fa89.1
- Update from upstream linux-firmware.
- Nuke unapplied radeon patches.

* Sat Jun 07 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20140605-38.gita4f3bc03.1
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Thu Jun 05 2014 Josh Boyer <jwboyer@fedoraproject.org> - 20140605-38.gita4f3bc03
- Updates for Intel 3160/7260/7265 firmware (1087717)
- Add firmware for rtl8723be (rhbz 1091753)
- Updates for radeon CIK, SI/CI, and Mullins/Beema GPUs (rhbz 1094153)
- Various other firmware updates

* Mon Mar 17 2014 Josh Boyer <jwboyer@fedoraproject.org>
- Updates for Intel 3160/7260 and BCM43362 (rhbz 1071590)

* Tue Mar 04 2014 Josh Boyer <jwboyer@fedoraproject.org>
- Fixup Intel wireless package descriptions and Source0 (rhbz 1070600)

* Fri Jan 31 2014 Josh Boyer <jwboyer@fedoraproject.org> - 20140131-35.gitd7f8a7c8
- Update to new snapshot
- Updates for Intel 3160/7260, radeon HAWAII GPUs, and some rtlwifi chips
- Fixes bugs 815579 1046935

* Tue Oct 01 2013 Kyle McMartin <kyle@fedoraproject.org> - 20131001-32.gitb8ac7c7e
- Update to a new git snapshot, drop radeon patches.

* Mon Sep 16 2013 Josh Boyer <jwboyer@fedoraproject.org> - 20130724-31.git31f6b30
- Obsolete ql2x00-firmware packages again (rhbz 864959)

* Sat Jul 27 2013 Josh Boyer <jwboyer@redhat.com> - 20130724-30.git31f6b30
- Add AMD ucode back in now that microcode_ctl doesn't provide it

* Fri Jul 26 2013 Dave Airlie <airlied@redhat.com> 20130724-29.git31f6b30
- add radeon firmware which are lost on the way upstream (#988268)

* Thu Jul 25 2013 Josh Boyer <jwboyer@redhat.com> - 20130724-28.git31f6b30
- Temporarily remove AMD microcode (rhbz 988263)
- Remove Creative CA0132 HD-audio files as they're in alsa-firmware

* Wed Jul 24 2013 Josh Boyer <jwboyer@redhat.com> - 20130724-27.git31f6b30
- Update to latest upstream
- New rtl, iwl, and amd firmware

* Fri Jun 07 2013 Josh Boyer <jwboyer@redhat.com> - 20130607-26.git2892af0
- Update to latest upstream release
- New radeon, bluetooth, rtl, and wl1xxx firmware

* Mon May 20 2013 Kyle McMartin <kyle@redhat.com> - 20130418-25.gitb584174
- Use a common version number for both the iwl*-firmware packages and
  linux-firmware itself.
- Don't reference old kernel-firmware package in %%description

* Mon May 20 2013 Kyle McMartin <kyle@redhat.com> - 20130418-0.3.gitb584174
- Bump iwl* version numbers as well...

* Mon May 20 2013 Kyle McMartin <kyle@redhat.com> - 20130418-0.2.gitb584174
- UsrMove: move firmware to /usr/lib/firmware
- Remove duplicate /usr/lib/firmware/updates entry (already in linux-firmware.dirs)
- Simplify sed by using '!' instead of '/' as regexp delimiter
- Fix date error (commited on Mon Feb 04, so change that entry)

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
