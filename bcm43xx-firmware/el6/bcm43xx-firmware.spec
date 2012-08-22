Summary:	Firmwares for brcmsmac (PCIe/AXI) and brcmfmac (SDIO/USB) drivers
Name:		bcm43xx-firmware
Version:	0.0
Release:	1%{?dist}
License:	Redistributable, no modification permitted
Group:		System Environment/Kernel
URL:		http://git.kernel.org/?p=linux/kernel/git/firmware/linux-firmware.git

Source0: bcm43xx-0.fw
Source1: bcm43xx_hdr-0.fw
Source2: brcmfmac4329.bin
Source3: brcmfmac4330.bin
Source4: brcmfmac4334.bin
Source5: brcmfmac43236b.bin
Source6: bcm4329-fullmac-4.bin
Source7: LICENCE.broadcom_bcm43xx

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build

BuildArch: noarch

%description
This package provides the firmware required for the Broadcom
brcmsmac (PCIe/AXI) and brcmfmac (SDIO/USB) drivers supporting bcm43xx chipsets

For brcmfmac, create a symlink for the appropriate firmware. E.g,
ln -s /lib/firmware/brcm/brcmfmac4329.bin /lib/firmware/brcm/brcmfmac-sdio.bin
ln -s /lib/firmware/brcm/brcmfmac4330.bin /lib/firmware/brcm/brcmfmac-sdio.bin

%prep
# nothing to prep

%build
# nothing to build

%install
%{__rm} -rf %{buildroot}
%{__install} -d %{buildroot}/lib/firmware/brcm/
%{__install} -p -m 0644 %{SOURCE0} %{buildroot}/lib/firmware/brcm/
%{__install} -p -m 0644 %{SOURCE1} %{buildroot}/lib/firmware/brcm/
%{__install} -p -m 0644 %{SOURCE2} %{buildroot}/lib/firmware/brcm/
%{__install} -p -m 0644 %{SOURCE3} %{buildroot}/lib/firmware/brcm/
%{__install} -p -m 0644 %{SOURCE4} %{buildroot}/lib/firmware/brcm/
%{__install} -p -m 0644 %{SOURCE5} %{buildroot}/lib/firmware/brcm/
%{__install} -p -m 0644 %{SOURCE6} %{buildroot}/lib/firmware/brcm/
%{__install} -p -m 0644 %{SOURCE7} %{buildroot}/lib/firmware/brcm/

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
/lib/firmware/brcm/*

%changelog
* Thu Aug 16 2012 Philip J Perry <phil@elrepo.org> - 0.0-1
- Initial package of Broadcom bcm43xx firmware for elrepo.org
