                               README Notes
                        Broadcom bnxt_en Linux Driver
                              Version 1.10.3

                              Broadcom Inc.
                           15101 Alton Parkway,
                            Irvine, CA 92618

                 Copyright (c) 2015 - 2016 Broadcom Corporation
                   Copyright (c) 2016 - 2018 Broadcom Limited
                     Copyright (c) 2018 - 2025 Broadcom Inc.
                           All rights reserved


Table of Contents
=================

  Introduction
  Limitations
  Port Speeds
  BNXT_EN Driver Dependencies
  BNXT_EN Driver Compilation
  BNXT_EN Driver Settings
  Autoneg
  Energy Efficient Ethernet
  Enabling Receive Side Scaling (RSS)
  Enabling Accelerated Receive Flow Steering (RFS)
  Enabling Busy Poll Sockets
  Enabling SR-IOV
  Virtual Ethernet Bridge (VEB)
  Hardware QoS
  PTP Hardware Clock
  Set IRQ Balance manually
  BNXT_EN Driver Parameters
  BNXT_EN Driver Defaults
  Statistics
  Unloading and Removing Driver
  Updating Firmware for Broadcom NetXtreme-C and NetXtreme-E devices
  Updating Firmware for Broadcom Nitro device
  Devlink
  Error Recovery
  Multi-root NUMA Direct
  Dynamic Interrupt Moderation
  HWMON support
  USHI based NICCLI

Introduction
============

This file describes the bnxt_en Linux driver for the BCM573xx, BCM574xx,
BCM575xx, BCM576xx, NetXtreme-S BCM5880x (up to 400 Gbps) Ethernet Network
Controllers and Broadcom Nitro BCM58700 4-port 1/2.5/10 Gbps Ethernet Network
Controller.


Limitations
===========

1. The current version of the driver should compile on any contemporary
Linux distributions. For a complete list of the Operating Systems supported
with this release consult the Broadcom Ethernet Network Adapter User Guide.

2. The driver build depends on installed kernel headers and associated build
infrastructure. Present kernels depend on GCC version 4.9, or later, and
as a direct consequence, the BNXT_EN driver inherits these constraints
(refer to https://www.kernel.org/doc/html/latest/process/changes.html for
details). Furthermore, depending on the distribution's packaging choices,
the installed headers may have been built using a later version of the
toolchain. This may result in the installed headers being configured in
ways that are incompatible with older tools. Thus, it is recommended
practice to use the latest stable version of the toolchain to build the
driver.

3. Laser needs to be brought up for Nitro BCM58700 Ethernet controller
using the following command, to bring up the Link.

    i2cset -f -y 1 0x70 0 7 && i2cset -f -y 1 0x24 0xff 0x0

4. Each device supports hundreds of MSIX vectors.  The driver will enable
all MSIX vectors when it loads.  On some systems running on some kernels,
the system may run out of interrupt descriptors, especially when using
multiple devices or NPAR devices.

5. In general, configuring VF devices depends on the associated PF being in the
ifup state. This is because the PF driver actively participates in allocation
and management of shared device resources, while the present software design
precludes maintaining the necessary communication with the device firmware to
do so when it is down.

6. Disabling devlink health auto recovery for the firmware reporter is not
currently supported. Conversely, enabling auto dump has no effect. While
crash dumps do not occur during the ordinary devlink health report lifecycle,
the device itself does automatically capture this information prior to reset.
Thus, the desirable crash context can still be retrieved via devlink health
after the fact, but only if recovery has been allowed to proceed. These lazy
retrieval semantics may still have user perceptible implications in that no
dump is stored in the devlink reporter context until such time as it is first
requested. Specifically, an earlier dump will be lost if a subsequent event
occurs before capture, whereas devlink has the opposite semantics, retaining
the earliest dump while dropping subsequent events. Improved firmware support
for more accurate devlink health semantics is planned.

7. The socket configuration control ioctls, viz SIOCGMIIPHY, SIOCGMIIREG and
SIOCSMIIREG, are valid only when used for an external PHY connected to the
adapter. Users should not rely on the information returned by these when used
for embedded PHY.

8. Ethtool offline selftest is not supported with RoCE driver loaded.

Port Speeds
===========

On some dual-port devices, the port speed of each port must be compatible
with the port speed of the other port.  10Gbps and 25Gbps are not compatible
speeds.  For example, if one port is set to 10Gbps and link is up, the other
port cannot be set to 25Gbps.  However, the driver will allow incompatible
speeds to be set on the two ports if link is not up yet.  Subsequent link up
on one port will render the incompatible speed on the other port to become
unsupported.  A console message like this may appear when this scenario
happens:

   bnxt_en 0000:04:00.0 eth0: Link speed 25000 no longer supported

If the link is up on one port, the driver will not allow the other port to
be set to an incompatible speed.  An attempt to do that will result in an
error.  For example, eth0 and eth1 are the 2 ports of the dual-port device,
eth0 is set to 10Gbps and link is up.

   ethtool -s eth1 speed 25000
   Cannot set new settings: Invalid argument
     not setting speed

This operation will only be allowed when the link goes down on eth0 or if
eth0 is brought down using ifconfig/ip.

On some NPAR (NIC partitioning) devices where one port is shared by multiple
PCI functions, the port speed is pre-configured and cannot be changed by
the driver.

See Autoneg section below for additional information.


BNXT_EN Driver Dependencies
===========================

The driver has no dependencies on user-space firmware packages as all necessary
firmware must be programmed in NVRAM(or QSPI for Nitro BCM58700 devices).
Starting with driver version 1.0.0, the goal is that the driver will be
compatible with all future versions of production firmware. All future versions
of the driver will be backwards compatible with firmware as far back as the
first production firmware.

The first production firmware is version 20.1.11 using Hardware Resource
Manager (HWRM) spec. 1.0.0.

ethtool -i displays the firmware versions.  For example:

   ethtool -i eth0

will show among other things:

   firmware-version: 20.1.11/1.0.0 pkg 20.02.00.03

In this example, the first version number (20.1.11) is the firmware version,
the second version number (1.0.0) is the HWRM spec. version.  The third
version number (20.02.00.03) is the package version of all the different
firmware components in NVRAM.  The package version may not be available on
all devices.

Using kernels older than 4.7, if CONFIG_VLAN_MODULE kernel option is set as a
module option, the vxlan.ko module must be loaded before the bnxt_en.ko module.

Using newer kernels, the hwmon.ko module may need to be loaded first if
CONFIG_HWMON_MODULE kernel option is set as a module option.

Using kernel versions 4.6 or higher, devlink.ko module may need to be loaded
first if CONFIG_NET_DEVLINK kernel option is set as a module option. Some Linux
distributions, notably Red Hat Enterprise Linux, backport certain features to
earlier kernels. For example, certain 3.10 RHEL kernels also provide a devlink
module. Note that in such cases, the feature may not be fully supported. Please
consult distribution release notes and documentation for comprehensive details.

Using kernel versions 5.4 or higher (or distribution kernels such as RHEL8.x
with TLS backports), the tls.ko module may need to be loaded first if
CONFIG_TLS and CONFIG_TLS_DEVICE are enabled as module options in the kernel.

BNXT_EN Driver Compilation
==========================

As noted under "Limitations" above, building the BNXT_EN driver depends on
installed kernel headers and build infrastructure. These details are Linux
distribution specific, and as such, are beyond the scope of this README. As an
example, on Red Hat systems the kernel-devel and kernel-headers packages are
required. Standard development packages such as gcc, make, awk, sed, grep,
etc. are also needed. Building drivers from source is not a typical user
activity. It is inherently specialized subject matter that assumes certain
domain knowledge. Thus, the target audience is advanced users and software
developers, who should be able to resolve trivial dependencies in response
to any "command not found" errors discovered during the build.

Before compilation, the integrity of the individual files included in the
source archive can be verified against the MANIFEST:

    $ sha512sum -c MANIFEST

Absent further authentication of the manifest, this step alone does not provide
any security, since it would be trivial for an attacker to replace the manifest
itself. However, checking may well catch inadvertent modifications since the
time that the source archive was produced, given that the manifest is generated
by tooling as part of the packaging process. To aid debugging, the manifest is
also used to update the build version reported by the driver if the source code
has altered from what was packaged.

Assuming all the build dependencies are satisfied, the Makefile will attempt
to discover the installed location of required components based on the running
kernel version. Thus, under normal circumstances, all that is required is a
simple call to make:

    $ make

which should result in the build producing the bnxt_en.ko kernel module.

This module can subsequently be installed by performing:

    $ make install

It may also be necessary to rebuild the initrd in order to make the updated
module available during early boot. This process is again distribution
specific (dracut on Red Hat, update-initramfs on Debian derived distributions,
such as Ubuntu, etc). On some distributions, the 'make install' step above
triggers distribution specific tooling via the INSTALLKERNEL Kbuild hook and
the initrd is updated automatically.

The BNXT_EN module relies on Linux's standard Kbuild infrastructure. As such,
documentation pertaining to building the kernel is generally applicable in
the unlikely event that the reader runs into any unexpected difficulties:

    https://kernelnewbies.org/KernelBuild
    https://www.kernel.org/doc/html/latest/kbuild/index.html
    https://www.kernel.org/doc/Documentation/kbuild/modules.txt

If kernel headers are installed in a non-standard location, the build can be
directed to a specific path via the KDIR environment variable:

    $ KDIR=<path> make

Alternatively, if multiple versions are installed in the standard distribution
locations, the build can be directed to use a specific version using the KVER
environment variable:

    $ KVER=<version> make

Other than the options to locate kernel dependencies, the BNXT_EN driver
exposes no other compile time customizable features.

BNXT_EN Driver Settings
=======================

The bnxt_en driver settings can be queried and changed using ethtool. The
latest ethtool can be downloaded from
ftp://ftp.kernel.org/pub/software/network/ethtool if it is not already
installed. The following are some common examples on how to use ethtool. See
the ethtool man page for more information. ethtool settings do not persist
across reboot or module reload. The ethtool commands can be put in a startup
script such as /etc/rc.local to preserve the settings across a reboot. On
Red Hat distributions, "ethtool -s" parameters can be specified in the
ifcfg-ethx scripts using the ETHTOOL_OPTS keyword.

Some ethtool examples:

1. Show current speed, duplex, and link status:

   ethtool eth0

Note that if auto-negotiation is off, ethtool will always show the speed
setting whether link is up or down.  If auto-negotiation is on, ethtool will
show the negotiated speed when link is up, and unknown speed when link is
down.

2. Set speed:

Example: Set speed to 10Gbps with autoneg off:

   ethtool -s eth0 speed 10000 autoneg off

Example: Set speed to 25Gbps with autoneg off:

   ethtool -s eth0 speed 25000 autoneg off

On some NPAR (NIC partitioning) devices, the port speed and flow control
settings cannot be changed by the driver.

See Autoneg section below for additional information on configuring
Auto-Negotiation.

3. Show offload settings:

   ethtool -k eth0

4. Change offload settings:

Example: Turn off TSO (TCP Segmentation Offload)

   ethtool -K eth0 tso off

Example: Turn off hardware GRO (Generic Receive Offload)

   ethtool -K eth0 rx-gro-hw off

Note that "rx-gro-hw" (hardware GRO) setting is available in newer kernels
such as 4.16.  When "rx-gro-hw" is turned off, there is no effect on software
GRO.  Prior to the introduction of "rx-gro-hw", hardware GRO settings can only
be controlled by controlling "gro", which applies to both GRO and hardware GRO.

   ethtool -K eth0 gro off

Example: Turn off hardware LRO (Large Receive Offload)

   ethtool -K eth0 lro off

Note that hardware GRO and hardware LRO are mutually exclusive.  Hardware
GRO is generally better than LRO because the former is reversible and
is compatible with bridging and routing (including bridging to Virtual
Machines).  LRO must be turned off when bridging or routing is enabled.

Example: Turn on hardware GRO

   ethtool -K eth0 rx-gro-hw on

If "rx-gro-hw" is not available on older kernels, use "gro".

   ethtool -K eth0 gro on

Note that if both "gro" and "lro" are set on older kernels that don't support
"rx-gro-hw", the driver will use hardware GRO.

Note that "rx-gro-hw" and "lro" will be automatically disabled by the driver
when the MTU exceeds 4096 to work-around a hardware performance limitation
on older BCM573xx and BCM574xx chips.  When the MTU drops back to 4096 or
below, the orginal setting should be automatically restored.  On some older
kernels, the user may need to restore the setting manually.

5. Show ring sizes:

   ethtool -g eth0

6. Change ring sizes:

   ethtool -G eth0 rx N

Note that the RX Jumbo ring size is set automatically when needed and
cannot be changed by the user.

7. Get statistics:

   ethtool -S eth0

8. Show number of channels (rings):

   ethtool -l eth0

9. Set number of channels (rings):

   ethtool -L eth0 rx N tx N combined 0

   ethtool -L eth0 rx 0 tx 0 combined M

Note that the driver can support either all combined or all rx/tx channels,
but not a combination of combined and rx/tx channels.  The default is
combined channels to match the number of CPUs up to 8.  Combined channels
use less system resources but may have lower performance than rx/tx channels
under very high traffic stress.  rx and tx channels can have different numbers
for rx and tx but must both be non-zero.

Note that if RDMA is enabled on adapter, L2 tries to reserve <= 64 MSIx vectors
and if bnxt_re is loaded, L2 pre-set maximum would be a smaller value because
RoCE has used up the resources. If L2 needs more rings, unload bnxt_re and
increase the number of rings/channels used by L2 and then load the bnxt_re.
RoCE driver shall be loaded with the available number of MSIx vectors.

10. Show interrupt coalescing settings:

    ethtool -c eth0

Please refer to the section on Dynamic Interrupt moderation
on how these can be dynamically altered by the stack.

11. Set interrupt coalescing settings:

    ethtool -C eth0 rx-frames N

    Note that only these parameters are supported:
    rx-usecs, rx-frames, rx-usecs-irq, rx-frames-irq,
    tx-usecs, tx-frames, tx-usecs-irq, tx-frames-irq,
    stats-block-usecs.

Note that on 5.15 and newer kernels, CQE coalescing timer mode can be
enabled or disabled on some devices.  CQE mode means that the relevant
timer gets reset when a new interrupt generating event is ready.

Example: Enable CQE mode on RX:

    ethtool -C eth0 cqe-mode-rx on rx-usecs 20 rx-frames 10

With this setting, the RX coalescing timer will start after the first RX
frame is received.  If a new RX frame is received within 20 us, the RX
coalescing timer will restart counting from 0.  As long as a new RX frame
is received within 20 us since the last RX frame, the interrupt will be
delayed until 10 RX frames have been received.

Example: Disable CQE mode on RX:

    ethtool -C eth0 cqe-mode-rx off rx-usecs 20 rx-frames 10

With CQE mode disabled, the RX coalescing timer will start and will not be
reset once the first RX frame is received.  To coalesce the interrupt for
10 RX frames, all 10 RX frames have to be received within 20 us after the
first one has been received.

12. Show RSS flow hash indirection table and RSS hash key:

    ethtool -x eth0

Note that the RSS indirection table size may vary depending on the device
and the number of RX channels.

13. Set 40-byte RSS hash key:

    ethtool -X eth0 hkey 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f:10:11:12:13:14:15:16:17:18:19:1a:1b:1c:1d:1e:1f:20:21:22:23:24:25:26:27

14. Set RSS indirection table:

    ethtool -X eth0 [start N] [ equal N | weight W0 W1 ... | default ]

Example: Set RSS indirection table to have equal distribution for the first
         four channels:

    ethtool -X eth0 equal 4

Example: Set RSS indirection table to have equal distribution for six
         channels starting from channel 2:

    ethtool -X eth0 start 2 equal 6

Note that the start parameter is 0-based.

Example: Set RSS indirection table to have weight distributions of 1:2:3:4
         for four channels starting from channel 4:

    ethtool -X eth0 start 4 weight 1 2 3 4

Note that the number of channels being configured must be valid and must not
exceed the number of RX or combined channels.  The configured settings will be
preserved whenever possible even when the number of RX or combined channels is
changed.  In some cases when the settings cannot be preserved, the indirection
table will revert back to default even distribution for all channels.

On 5750X and newer chips, the size of the indirection table may change as
the number of RX channel changes.  If the indirection table is set to
non-default, the RX/combined channel number changes will be restricted to the
range that does not change the indirection table size.

Example: Set RSS indirection table when the combined channel number is 8:

    ethtool -L eth0 combined 8 rx 0 tx 0
    ethtool -X eth0 start 2 weight 1 2 3 4 0 0

On a 5750X chip, this uses an indirection table size of 64.  When the
RX/combined channel number changes to 65 or above, the indirection table
size increases and such a change will fail:

    ethtool -L eth0 combined 65 rx 0 tx 0
    netlink error: Invalid argument

The kernel log will show:

bnxt_en 0000:04:00.0 eth0: RSS table size change required, RSS table entries must be default to proceed

The indirection table must be reverted back to default first before changing
the channels to 65 or above in this example:

    ethtool -X eth0 default
    ethtool -L eth0 combined 65 rx 0 tx 0

15. Run self test:

    ethtool -t eth0

    Note that only single function PFs can execute self tests.  If a PF has
    active VFs, only online tests can be executed.

16. Collect Firmware Coredump:

    ethtool -w eth0 data FILENAME

17. Set coredump flags:

    ethtool -W eth0 N

    Note that the following are supported values for N:
       0  Collection for live dump
       1  Collection for crash dump
          This setting is allowed in either of following cases
          a) PFs on platforms that have kernel config. option CONFIG_TEE_BNXT_FW enabled.
             This option is only enabled on some ARM SoCs.
          b) PFs which are configured to support crash dump using host memory.
       2  Collection for driver dump plus host context memory dump
       3  Collection for live dump plus CFCx L1 cached context dump

18. Reset the device:

    ethtool --reset eth0 [flags N] [type]

    Note that driver supports 'ap' and 'all' type of resets. Also, '--reset'
    option is available from ethtool version 4.15 or newer.

19. Add receive network flow classification filters.

    ethtool -N eth0 flow-type ether|ipv4|ipv6|tcp4|udp4|tcp6|udp6 FLOW_SPEC

    This feature requires n-tuple filters to be enabled (default is enabled):

    ethtool -K eth0 ntuple on

Example: Ethernet filter to rx queue 0

    ethtool -N eth0 flow-type ether dst 00:11:22:33:44:55 action 0
    Note: flow-type ether is not supported on BCM575xx series chipsets.

Example: TCP/IPv4 5-tuple filter to rx queue 1

    ethtool -N eth0 flow-type tcp4 dst-ip 192.168.0.1 src-ip 192.168.0.2 \
    dst-port 80 src-port 32768 action 1

Example: UDP/IPv4 4-tuple filter to rx queue 2

    ethtool -N eth0 flow-type udp4 dst-ip 192.168.0.1 src-ip 192.168.0.2 \
    dst-port 2049 action 2

Example: IPv4 4-tuple filter to drop with wildcard match i.e. TCP/UDP/ICMP

    ethtool -N eth0 flow-type ipv4 dst-ip 192.168.0.1 src-ip 192.168.0.2 \
    l4proto 255 action -1

The action parameter must be greater than or equal to 0 to specify the
RX queue/ring number.  The standard negative action parameters for
Wake-on-LAN is not supported.  Note, however, negative actions other than
drop are used to extend the ethtool interface for mapping flows to sockets, as
detailed in the Multi-root NUMA Direct section below.
Note that ipv4/ipv6 flows supports only ICMPV4/ICMPV6 protocols and reserved
protocol (255). Reserved protocol is used for wildcard match i.e. TCP/UDP/ICMP.

The flow-type is counted as the first tuple and must always be specified.
At least one additional tuple must be specified for TCP/UDP filters.
Partial wildcard tuples with incomplete masks are supported using the
normal ethtool syntax.  For example, to match the 192.168.1.0/24 subnet:

    ethtool -N eth0 flow-type udp dst-ip 192.168.1.0 m 0.0.0.255 action 1

When supplied, the mask is counterintuitively specified as the inverse of
the way subnet masks are typically specified.  That is, ethtool masks have
ones in the bits that are to be ignored in the match - a quirk of ethtool's
backwards compatibility with the way masks were specified using the legacy
kernel ntuple interface.  Note that tuple masks are optional and are assumed
by ethtool to be the complete mask (all zeroes) when the tuple alone is
specified.

It is possible to create a 5-tuple filter that is inside the domain of
another 4-tuple filter, for example.  In general, the more specific
5-tuple filter will take precedence.

If the filter is created succesfully, the ID of the filter will be returned
by ethtool.  ethtool -n will also display the current list of filters with
their IDs.  A specific filter can be deleted by specifying the ID.  The
"loc" parameter that allows the user to specify the location of the filter
is not supported.  It must be the default 0xffffffff which means that the
driver will choose the location/ID.

Example: Delete filter ID 3

    ethtool -N eth0 delete 3

Note that if accelerated RFS is enabled and it has added some 5-tuple
filters, any duplicate 5-tuple filters added by ethtool will be rejected.
It is generally not recommended to enable accelerated RFS and create
static 5-tuple filters on the same function.

20. Show Forward Error Correction (FEC) configured and active settings:

    ethtool --show-fec eth0

21. Set Forward Error Correction (FEC) settings:

    ethtool --set-fec eth0 encoding auto|off|baser|rs|llrs

Example: set FEC to autonegotiate:

    ethtool --set-fec eth0 encoding auto

    Note that a new FEC setting will always result in a link toggle.  In FEC
    autoneg code, the advertised FEC settings will be shown by the main
    ethtool command together with other link settings:

    ethtool eth0
    Settings for eth0:
            Supported ports: [ FIBRE ]
            Supported link modes:   10000baseT/Full
                                    40000baseCR4/Full
                                    25000baseCR/Full
                                    50000baseCR2/Full
            Supported pause frame use: Symmetric Receive-only
            Supports auto-negotiation: Yes
            Supported FEC modes: BaseR RS
            Advertised link modes:  10000baseT/Full
                                    40000baseCR4/Full
                                    25000baseCR/Full
                                    50000baseCR2/Full
            Advertised pause frame use: No
            Advertised auto-negotiation: Yes
            Advertised FEC modes: BaseR RS
            Speed: 25000Mb/s
            Duplex: Full
            Port: Direct Attach Copper
            PHYAD: 1
            Transceiver: internal
            Auto-negotiation: on
            Supports Wake-on: d
            Wake-on: d
            Current message level: 0x00000000 (0)

            Link detected: yes

Example: set FEC to forced Clause 91 (Reed Solomon):

    ethtool --set-fec eth0 encoding rs

    Note that a newer kernel such as 4.20 is required for full FEC support.

22. Dump registers:

    ethtool -d eth0

    This will dump some PCIe registers for diagnostics purposes.  Note that
    ethtool 5.10 or newer will provide formatting and decoding of the
    register output.

23. See ethtool man page for more options.


Autoneg
=======

The bnxt_en driver supports Autonegotiation of speed and flow control on
most devices.  Some dual-port 25G devices do not support Autoneg.  Autoneg
must be enabled for 10GBase-T devices.

Note that parallel detection is not supported when autonegotiating
100GBase-CR4, 50GBase-CR2, 40GBase-CR4, 25GBase-CR, 10GbE SFP+.
If one side is autonegoatiating and the other side is not,
link will not come up.

25G, 50G and 100G advertisements are newer standards first defined in the 4.7
kernel's ethtool interface.  To fully support these new advertisement speeds
for autonegotiation, 4.7 (or newer) kernel and a newer ethtool utility are
required. Similarly, PAM4 speeds are only supported with post 5.1 kernels.

Below are some examples to illustrate the limitations when using 4.6 and
older kernels:

1. Enable Autoneg with all supported speeds advertised when the device
currently has Autoneg disabled:

   ethtool -s eth0 autoneg on advertise 0x0

Note that to advertise all supported speeds (including 25G, 50G and 100G),
the device must initially have Autoneg disabled.  advertise is a hexadecimal
value specifying one or more advertised speed.  0x0 is special value that
means all supported speeds.  See ethtool man page.  These advertise values
are supported by the driver:

0x020           1000baseT Full
0x1000          10000baseT Full
0x1000000       40000baseCR4 Full

2. Enable Autoneg with only 10G advertised:

   ethtool -s eth0 autoneg on advertise 0x1000

or:

   ethtool -s eth0 autoneg on speed 10000 duplex full


3. Enable Autoneg with only 40G advertised:

   ethtool -s eth0 autoneg on advertise 0x01000000

4. Enable Autoneg with 40G and 10G advertised:

   ethtool -s eth0 autoneg on advertise 0x01001000

Note that the "Supported link modes" and "Advertised link modes" will not
show 25G, 50G and 100G even though they may be supported or advertised.  For
example, on a device that is supporting and advertising 10G, 25G, 40G, 50G and
100G, and linking up at 50G, ethtool will show the following:

   ethtool eth0
   Settings for eth0:
           Supported ports: [ FIBRE ]
           Supported link modes:   10000baseT/Full
                                   40000baseCR4/Full
           Supported pause frame use: Symmetric Receive-only
           Supports auto-negotiation: Yes
           Advertised link modes:  10000baseT/Full
                                   40000baseCR4/Full
           Advertised pause frame use: Symmetric
           Advertised auto-negotiation: Yes
           Speed: 50000Mb/s
           Duplex: Full
           Port: FIBRE
           PHYAD: 1
           Transceiver: internal
           Auto-negotiation: on
           Current message level: 0x00000000 (0)

           Link detected: yes

Using kernels 4.7 or newer and ethtool version 4.8 or newer, 25G, 50G and 100G
advertisement speeds can be properly configured and displayed, without any
of the limitations described above.  ethtool version 4.8 has a bug that
ignores the advertise parameter, so it is recommended to use ethtool 4.10.
Example ethtool 4.10 output showing 10G/25G/40G/50G/100G advertisement settings:

   ethtool eth0
   Settings for eth0:
           Supported ports: [ FIBRE ]
           Supported link modes:   10000baseT/Full
                                   40000baseCR4/Full
                                   25000baseCR/Full
                                   50000baseCR2/Full
                                   100000baseCR4/Full
           Supported pause frame use: Symmetric Receive-only
           Supports auto-negotiation: Yes
           Advertised link modes:  10000baseT/Full
                                   40000baseCR4/Full
                                   25000baseCR/Full
                                   50000baseCR2/Full
                                   100000baseCR4/Full
           Advertised pause frame use: No
           Advertised auto-negotiation: Yes
           Speed: 50000Mb/s
           Duplex: Full
           Port: Direct Attach Copper
           PHYAD: 1
           Transceiver: internal
           Auto-negotiation: on
           Supports Wake-on: d
           Wake-on: d
           Current message level: 0x00000000 (0)

           Link detected: yes

These are the complete advertise values supported by the driver using 4.7
kernel or newer and a compatible version of ethtool supporting the new
values:

0x020           1000baseT Full
0x1000          10000baseT Full
0x1000000       40000baseCR4 Full
0x80000000      25000baseCR Full
0x400000000     50000baseCR2 Full
0x4000000000    100000baseCR4 Full

Note that older drivers (prior to 2.21) did not make a distinction on the
exact physical layer encoding and media type for a link speed.  For example,
at 50G, the device may support 50000baseCR2 and 50000baseSR2 for copper and
multimode fiber cables respectively.  Regardless of what cabling is used
for 50G, these drivers used only the ethtool value defined for 50000baseCR2
to cover all variants of the 50G media types.  The same applies to all
other advertise value for other link speeds listed above.

More recent drivers report the correct media types in ethtool link modes.
In particular, if no media is detected, all supported modes should be now
reported. For instance, on a BCM575xx card one might find:

           Advertised link modes:  10000baseT/Full
                                   10000baseKX4/Full
                                   10000baseKR/Full
                                   25000baseCR/Full
                                   25000baseSR/Full
                                   50000baseCR2/Full
                                   100000baseSR4/Full
                                   100000baseCR4/Full
                                   100000baseLR4_ER4/Full
                                   50000baseSR2/Full
                                   10000baseCR/Full
                                   10000baseSR/Full
                                   10000baseLR/Full
                                   200000baseSR4/Full
                                   200000baseLR4_ER4_FR4/Full
                                   200000baseCR4/Full

Note, there is a many to one relationship between the fully specified link
modes and the underlying hardware support for autonegotiated speeds. For
example, 25000baseCR/Full and 25000baseSR/Full refer to the same underlying
hardware configuration, differing only in the media that is physically
attached. Enabling or disabling one will affect the other corresponding
modes and vise versa. Thus, after issuing:

    ethtool -s eth0 advertise 200000baseCR4/Full off

all three of the above supported 4 lane 200Gbps configurations are dropped
from the advertised list:

           Advertised link modes:  10000baseT/Full
                                   10000baseKX4/Full
                                   10000baseKR/Full
                                   25000baseCR/Full
                                   25000baseSR/Full
                                   50000baseCR2/Full
                                   100000baseSR4/Full
                                   100000baseCR4/Full
                                   100000baseLR4_ER4/Full
                                   50000baseSR2/Full
                                   10000baseCR/Full
                                   10000baseSR/Full
                                   10000baseLR/Full

When the media type is detected by the hardware, only those modes supported
by the fitted media are relevant:

           Advertised link modes:  25000baseSR/Full
                                   50000baseSR2/Full
                                   100000baseSR4/Full
                                   10000baseSR/Full

COMPATIBILITY NOTE:

In the above case, SR optics are installed. Because older drivers reported
copper modes for all media types, the driver is still tolerant of the
incorrect mode being used. Note, however, that the fitted media will take
precedence when adding advertised speeds. That is, while modes can be added
using a mismatched media type, they cannot be removed without also clearing
the bit associated with the specific attached media. It is therefore possible
to add 200000baseSR4/Full to the above list by requesting the corresponding
200000baseCR4/Full mode, in a backward compatible fashion, but the converse
is not true. If SR media is attached and the 200000baseSR4/Full mode is
listed, then it must be explicitly removed from the active list in order to
disable it.

Also of note, newer drivers will report 1000baseX/Full for gigabit Ethernet
when a DAC module is attached, whereas older drivers reported 1000baseT/Full
regardless of media.


Energy Efficient Ethernet
=========================

The driver supports Energy Efficient Ethernet (EEE) settings on 10GBase-T
devices.  If enabled, and connected to a link partner that advertises EEE,
EEE will become active.  EEE saves power by entering Low Power Idle (LPI)
state when the transmitter is idle.  The downside is increased latency as
it takes a few microseconds to exit LPI to start transmitting again.

On a 10GBase-T device that supports EEE, the link up console message will
include the current state of EEE.  For example:

   bnxt_en 0000:05:00.0 eth0: NIC Link is Up, 10000 Mbps full duplex, Flow control: none
   bnxt_en 0000:05:00.0 eth0: EEE is active

The active state means that EEE is negotiated to be active during
autonegotiation.  Additional EEE parameters can be obtained using ethtool:

   ethtool --show-eee eth0

   EEE Settings for eth0:
           EEE status: enabled - active
           Tx LPI: 8 (us)
           Supported EEE link modes:  10000baseT/Full
           Advertised EEE link modes:  10000baseT/Full
           Link partner advertised EEE link modes:  10000baseT/Full

The tx LPI timer of 8 microseconds is currently fixed and cannot be adjusted.
EEE is only supported on 10GBase-T.  1GBase-T does not currently support EEE.

To disable EEE:

   ethtool --set-eee eth0 eee off

To enable EEE, but disable LPI:

   ethtool --set-eee eth0 eee on tx-lpi off

This setting will negotiate EEE with the link partner but the transmitter on
eth0 will not enter LPI during idle.  The link partner may independently
choose to enter LPI when its transmitter is idle.


Enabling Receive Side Scaling (RSS)
===================================

By default, the driver enables RSS by allocating receive rings to match the
the number of CPUs (up to 8).  Incoming packets are run through a 4-tuple
or 2-tuple hash function for TCP/IP packets and IP packets respectively.
Non fragmented UDP packets are run through a 4-tuple hash function on newer
devices (2-tuple on older devices).  See below for more information about
4-tuple and 2-tuple and how to configure it.

The computed hash value will determine the receive ring number for the
packet.  This way, RSS distributes packets to multiple receive rings while
guaranteeing that all packets from the same flow will be steered to the same
receive ring.  The processing of each receive ring can be done in parallel
by different CPUs to achieve higher performance.  For example, irqbalance
will distribute the MSIX vector of each RSS receive ring across CPUs.
However, RSS does not guarantee even distribution or optimal distribution of
packets.

To disable RSS, set the number of receive channels (or combined channels) to 1:

   ethtool -L eth0 rx 1 combined 0

or

   ethtool -L eth0 combined 1 rx 0 tx 0

To re-enable RSS, set the number of receive channels or (combined channels) to
a value higher than 1.

The RSS hash can be configured for 4-tuple or 2-tuple for various flow types.
4-tuple means that the source, destination IP addresses and layer 4 port
numbers are included in the hash function.  2-tuple means that only the source
and destination IP addresses are included.  4-tuple generally gives better
results.  Below are some examples on how to set and display the hash function.

To display the current hash for TCP over IPv4:

   ethtool -u eth0 rx-flow-hash tcp4

To disable 4-tuple (enable 2-tuple) for UDP over IPv4:

   ethtool -U eth0 rx-flow-hash udp4 sd

To enable 4-tuple for UDP over IPv4:

   ethtool -U eth0 rx-flow-hash udp4 sdfn


Enabling Accelerated Receive Flow Steering (RFS)
================================================

RSS distributes packets based on n-tuple hash to multiple receive rings.
The destination receive ring of a packet flow is solely determined by the
hash value.  This receive ring may or may not be processed in the kernel by
the CPU where the sockets application consuming the packet flow is running.

Accelerated RFS will steer incoming packet flows to the ring whose MSI-X
vector will interrupt the CPU running the sockets application consuming
the packets.  The benefit is higher cache locality of the packet data from
the moment it is processed by the kernel until it is consumed by the
application.

Accelerated RFS requires n-tuple filters to be supported.  On older
devices, only Physical Functions (PFs, see SR-IOV below) support n-tuple
filters.  On the latest devices, n-tuple filters are supported and enabled
by default on all functions.  Use ethtool to disable n-tuple filters:

   ethtool -K eth0 ntuple off

To re-enable n-tuple filters:

   ethtool -K eth0 ntuple on

After n-tuple filters are enabled, Accelerated RFS will be automatically
enabled when RFS is enabled.  These are example steps to enable RFS on
a device with 8 rx rings:

echo 32768 > /proc/sys/net/core/rps_sock_flow_entries
echo 2048 > /sys/class/net/eth0/queues/rx-0/rps_flow_cnt
echo 2048 > /sys/class/net/eth0/queues/rx-1/rps_flow_cnt
echo 2048 > /sys/class/net/eth0/queues/rx-2/rps_flow_cnt
echo 2048 > /sys/class/net/eth0/queues/rx-3/rps_flow_cnt
echo 2048 > /sys/class/net/eth0/queues/rx-4/rps_flow_cnt
echo 2048 > /sys/class/net/eth0/queues/rx-5/rps_flow_cnt
echo 2048 > /sys/class/net/eth0/queues/rx-6/rps_flow_cnt
echo 2048 > /sys/class/net/eth0/queues/rx-7/rps_flow_cnt

These steps will set the global flow table to have 32K entries and each
receive ring to have 2K entries.  These values can be adjusted based on
usage.

Note that for Accelerated RFS to be effective, the number of receive channels
(or combined channels) should generally match the number of CPUs.  Use
ethtool -L to fine-tune the number of receive channels (or combined channels)
if necessary.  Accelerated RFS has precedence over RSS.  If a packet matches an
n-tuple filter rule, it will be steered to the RFS specified receive ring.
If the packet does not match any n-tuple filter rule, it will be steered
according to RSS hash.

To display the active n-tuple filters setup for Accelerated RFS:

   ethtool -n eth0

Note that if there are a large number of filters and they are constantly
changing, ethtool may report some retrieval failures.  These errors are
normal.

The Accelerated RFS filters added by the stack are subject to aging based
on activity.  It is normal for a small number of these filters to remain
after all traffic has stopped.  New filters will eventually trigger the
removal of these old filters.

IPv6, GRE and IP-inIP n-tuple filters are supported on 4.5 and newer kernels.
Note that RFS will only steer non-fragmented UDP packets to a connected UDP
socket.  Fragmented UDP packets or UDP packets to a connectionless socket
will fall back to RSS hashing.

n-tuple filters can also be added statically using ethtool (See
BNXT_EN Driver Settings section above).


Enabling Busy Poll Sockets
==========================

Using 3.11 and newer kernels (also backported to some major distributions),
Busy Poll Sockets are supported by the bnxt_en driver if
CONFIG_NET_RX_BUSY_POLL is enabled.  Individual sockets can set the
SO_BUSY_POLL option, or it can be enabled globally using sysctl:

    sysctl -w net.core.busy_read=50

This sets the time to busy read the device's receive ring to 50 usecs.
For socket applications waiting for data to arrive, using this method
can decrease latency by 2 or 3 usecs typically at the expense of
higher CPU utilization.  The value to use depends on the expected
time the socket will wait for data to arrive.  Use 50 usecs as a
starting recommended value.

In addition, the following sysctl parameter should also be set:

    sysctl -w net.core.busy_poll=50

This sets the time to busy poll for socket poll and select to 50 usecs.
50 usecs is a recommended value for a small number of polling sockets.


Enabling SR-IOV
===============

The Broadcom NetXtreme-C and NetXtreme-E devices support Single Root I/O
Virtualization (SR-IOV) with Physical Functions (PFs) and Virtual Functions
(VFs) sharing the Ethernet port.  The same bnxt_en driver is used for both
PFs and VFs under Linux.

Only the PFs are automatically enabled.  If a PF supports SR-IOV, lspci
will show that it has the SR-IOV capability and the total number of VFs
supported.  To enable one or more VFs, write the desired number of VFs
to the following sysfs file:

    /sys/bus/pci/devices/<domain>:<bus>:<device>:<function>/sriov_numvfs

For example, to enable 4 VFs on bus 82 device 0 function 0:

    echo 4 > /sys/bus/pci/devices/0000:82:00.0/sriov_numvfs

To disable the VFs, write 0 to the same sysfs file.  Note that to change
the number of VFs, 0 must first be written before writing the new number
of VFs.

On older 2.6 kernels that do not support the sysfs method to enable SR-IOV,
the driver uses the module parameter "num_vfs" to enable the desired number
of VFs.  Note that this is a global parameter that applies to all PF
devices in the system.  For example, to enable 4 VFs on all supported PFs:

    modprobe bnxt_en num_vfs=4

The 4 VFs of each supported PF will be enabled when the PF is brought up.

The VF and the PF operate almost identically under the same Linux driver
but not all operations supported on the PF are supported on the VF.

The resources needed by each VF are assigned by the PF based on how many
VFs are requested to be enabled and the resources currently used by the PF.
It is important to fully configure the PF first with all the desired features,
such as number of RSS/TSS channels, jumbo MTU, CoS profile, etc, before enabling SR-IOV.
After enabling SR-IOV, there may not be enough resources left to reconfigure
the PF.

The resources are evenly divided among the VFs.  Enabling a large number of
VFs will result in less resources (such as RSS/TSS channels) for each VF.

Refer to other documentation on how to map a VF to a VM or a Linux Container.

Some attributes of a VF can be set using iproute2 through the PF.  SR-IOV
must be enabled by setting the number of desired VFs before any attributes
can be set.  Some examples:

1. Set VF MAC address:

   ip link set <pf> vf <vf_index> mac <vf_mac>

Example:

   ip link set eth0 vf 0 mac 00:12:34:56:78:9a

Note that if the VF MAC address is not set as shown, a random MAC address will
be used for the VF.  If the VF MAC address is changed while the VF driver has
already brought up the VF, it is necessary to bring down and up the VF before
the new MAC address will take effect.

2. Set VF link state:

   ip link set <pf> vf <vf_index> state auto|enable|disable

The default is "auto" which reflects the true link state.  Setting the VF
link to "enable" allows loopback traffic regardless of the true link state.

Example:

   ip link set eth0 vf 0 state enable

3. Set VF default VLAN:

   ip link set <pf> vf <vf_index> vlan <vlan id>

Example:

   ip link set eth0 vf 0 vlan 100

4. Set VF MAC address spoof check:

   ip link set <pf> vf <vf_index> spoofchk on|off

Example:

   ip link set eth0 vf 0 spoofchk on

Note that spoofchk is only effective if a VF MAC address has been set as
shown in #1 above.

5. Set VF trust:

   ip link set <pf> vf <vf_index> trust on|off

Example:

   ip link set eth0 vf 0 trust on

A VF with trust enabled can change its MAC address even if a MAC address has
been set by the PF as shown in #1 above.  This will be useful in some
bonding configurations where MAC address changes may be required.

Note VF trust attribute is supported on kernel 4.4 or newer and iproute utility
4.5 or newer.

Virtual Ethernet Bridge (VEB)
=============================

The NetXtreme-C/E devices contain an internal hardware Virtual Ethernet
Bridge (VEB) to bridge traffic between virtual ports enabled by SR-IOV.
VEB is normally turned on by default.  VEB can be switched to VEPA
(Virtual Ethernet Port Aggregator) mode if an external VEPA switch is used
to provide bridging between the virtual ports.

Use the bridge command to switch between VEB/VEPA mode.  Note that only
the PF driver will accept the command for all virtual ports belonging to the
same physical port.  The bridge mode cannot be changed if there are multiple
PFs sharing the same physical port (e.g. NPAR or Multi-Host).

To set the bridge mode:

   bridge link set dev <pf> hwmode {veb/vepa}

To show the bridge mode:

   bridge link show dev <pf>

Example:

   bridge link set dev eth0 hwmode vepa

Note that older firmware does not support VEPA mode.  This operation is
also not supported on older kernels.


Hardware QoS
============

The NetXtreme-C/E devices support hardware QoS.  The hardware has multiple
internal queues, each can be configured to support different QoS attributes,
such as latency, bandwidth, lossy or lossless data delivery.  These QoS
attributes are specified in the IEEE Data Center Bridging (DCB) standard
extensions to Ethernet.  DCB parameters include Enhanced Transmission
Selection (ETS) and Priority-based Flow Control (PFC).  In a DCB network,
all traffic will be classified into multiple Traffic Classes (TCs), each
of which is assigned different DCB parameters.

Typically, all traffic is VLAN tagged with a 3-bit priority in the VLAN
tag.  The VLAN priority is mapped to a TC.  For example, a network with
3 TCs may have the following priority to TC mapping:

0:0,1:0,2:0,3:2,4:1,5:0,6:0,7:0

This means that priorities 0,1,2,5,6,7 are mapped to TC0, priority 3 to TC2,
and priority 4 to TC1.  ETS allows bandwidth assignment for the TCs.  For
example, the ETS bandwidth assignment may be 40%, 50%, and 10% to TC0, TC1,
and TC2 respectively.  PFC provides link level flow control for each VLAN
priority independently.  For example, if PFC is enabled on VLAN priority 4,
then only TC1 will be subject to flow control without affecting the other
two TCs.

Typically, DCB parameters are automatically configured using the DCB
Capabilities Exchange protocol (DCBX).  The bnxt_en driver currently
supports the Linux lldpad DCBX agent.  lldpad supports all versions of
DCBX but the bnxt_en driver currently only supports the IEEE DCBX version.
Typically, the DCBX enabled switch will convey the DCB parameters to lldpad
which will then send the hardware QoS parameters to bnxt_en to configure
the device.  Refer to the lldpad(8) and lldptool(8) man pages for further
information on how to setup the lldpad DCBX agent.

Note that the embedded firmware DCBX/LLDP agent must be disabled in order
to run the lldpad agent in host software.  Refer to other Broadcom
documentation on how to disable the firmware agent in NVRAM.

To support hardware TCs, the proper Linux qdisc must be used to classify
outgoing traffic into their proper hardware TCs.  For example, the mqprio
qdisc may be used.  A simple example using mqprio qdisc is illustrated below.
Refer to the tc-mqprio(8) man page for more information.

   tc qdisc add dev eth0 root mqprio num_tc 3 map 0 0 0 2 1 0 0 0 hw 1

The above command creates the mqprio qdisc with 3 hardware TCs.  The priority
to TC mapping is the same as the example at the beginning of the section.
The bnxt_en driver will create 3 groups of tx rings, with each group mapping
to an internal hardware TC.

Once this is created, SKBs with different priorities will be mapped to the
3 TCs according to the specified map above.  Note that this SKB priority
is only used to direct packets within the kernel stack to the proper hardware
ring.  If the outgoing packets are VLAN tagged, the SKB priority does not
automatically map to the VLAN priority of the packet.  The VLAN egress map
has to be set up to have the proper VLAN priority for each packet.

In the current example, if VLAN 100 is used for all traffic, the VLAN egress
map can be set up like this:

   ip link add link eth0 name eth0.100 type vlan id 100 \
      egress 0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7

This creates a one-to-one mapping of SKB priority to VLAN egress priority.
In other words, SKB priority 0 maps VLAN priority 0, SKB priority 1 maps to
VLAN priority 1, etc.  This one-to-one mapping should generally be used.

Instead of using VLAN priority to map to TCs in the network, it is also
possible to use DSCP (Differentiated Services Code Point) in the IP header
to do the mapping.  Obviously, only IP traffic in the network can be mapped
this way, whereas VLAN priority will work universally for all traffic types.
The DSCP to priority mapping is specified using a new Application Priority TLV
recently added to the IEEE DCBX spec.  This is supported by the driver's
interface to lldpad.  Note that the application is responsible to set
the proper DSCP value in the IP header for outgoing traffic on a Linux host.
For example, iptables may be used to set the proper DSCP values for outgoing
traffic.  This will replace the VLAN egress mapping mentioned earlier if DSCP
is used instead of VLAN.  The rest of the steps are the same between VLAN and
DSCP.

If each TC has more than one ring, TSS will be performed to select a tx ring
within the TC.

To display the current qdisc configuration:

   tc qdisc show

Example output:

    qdisc mqprio 8010: dev eth0 root  tc 3 map 0 0 0 2 1 0 0 0 0 0 0 0 0 0 0 0
                 queues:(0:3) (4:7) (8:11)

The example above shows that bnxt_en has allocated 4 tx rings for each of the
3 TCs.  SKBs with priorities 0,1,2,5,6,7 will be transmitted using tx rings
0 to 3 (TC0).  SKBs with priority 4 will be transmitted using rings 4 to 7
(TC1).  SKBs with priority 3 will be transmitted using rings 8 to 11 (TC2).

Next, SKB priorities have to be set for different applications so that the
packets from the different applications will be mapped to the proper TCs.
By default, the SKB priority is set to 0.  There are multiple methods to set
SKB priorities.  net_prio cgroup is a convenient way to do this.  Refer to the
link below for more information:

https://www.kernel.org/doc/Documentation/cgroup-v1/net_prio.txt

As mentioned previously, the DCB attributes of each TC are normally configured
by the DCBX agent in lldpad.  It is also possible to set the DCB attributes
manually in a simple network or for test purposes.  The following example
will manually set up eth0 with the example DCB local parameters mentioned at
the beginning of the section.

   lldpad -d
   lldptool -T -i eth0 -V ETS-CFG tsa=0:ets,1:ets,2:ets \
            up2tc=0:0,1:0,2:0,3:2,4:1,5:0,6:0,7:0 \
            tcbw=40,50,10
   lldptool -T -i eth0 -V PFC enabled=4

Note that the ETS bandwidth distribution will only be evident when all
traffic classes are transmitting and reaching the link capacity.

RoCE APP TLV can also be set.  For example, to map RoCE v2 traffic
to priority 4:

lldptool -T -i eth0 -V APP app=4,3,4791

The bnxt_re driver will automatically obtain the proper priority and TC
mapping for offloaded RoCE traffic (RoCE v2 traffic mapped to priority 4 and
TC1 with PFC enabled in this example).

Usage of strict priority for a given CoS can result in starvation of
other rings configured.  This may result in the inability to transmit
packets on ets rings and may result in the kernel reporting transmit
timeouts.  Only configure strict priority on rings with high-priority,
low throughput traffic to prevent consuming resources.

See lldptool-ets(8), lldptool-pfc(8), lldptool-app(8) man pages for more
information.

On an NPAR device with multiple partitions sharing the same network port,
DCBX cannot be run on more than one partition.  In other words, the lldpad
admin Status can be set to rxtx on no more than one partition.  The same is
true for SRIOV virtual functions.  DCBX cannot be run on the VFs.

On these multi-function devices, the hardware TCs are generally shared
between all the functions.  The DCB parameters negotiated and setup on
the main function (NPAR or PF function) will be the same on the other
functions sharing the same port.  Note that the standard lldptool will
not be able to show the DCB parameters on the other functions which have
admin Status disabled.


PTP Hardware Clock
==================

The NetXtreme-C/E devices support PTP Hardware Clock which provides hardware
timestamps for PTP v2 packets.  The Linux PTP project contains more
information about this feature.  A newer 4.x kernel and newer firmware
(2.6.134 or newer) are required to use this feature.  Only the first PF
of the network port has access to the hardware PTP feature.  Use ethtool -T
to check if PTP Hardware Clock is supported.

On BCM574xx and BCM575xx chips, occasionally ptp4l application may report
timeout error while trying to retrieve tx timestamp and might allude to a
bug in driver.
Though application resumes the functioning immediately, unless an explicit
TX timestamp failure message is logged by bnxt_en in the dmesg, increasing
the default tx_timestamp_timeout of ptp4l to a suitable value will fix the
problem. On most systems 25ms works as optimal value.
For BCM574xx chips, 100ms would be the recommended value.

BCM575xx series chips support timestamping on VFs but not on VFs that have
transparent VLAN configured.

Set IRQ Balance Manually
========================

On newer 4.x kernels, the driver does IRQ affinity for higher performance.
But in older kernels, if driver is reloaded IRQ affinity will not be set
properly. In the case where IRQ affinity is not properly set, the interrupts
will be manually associated with a CPU using SMP affinity.

To manually balance interrupts, the `irqbalance` service needs to be stopped.

   service irqbalance stop

View the CPU cores where NetXtreme-C/E device's interrupt is allowed to be
received.

   grep "ethX" /proc/interrupts
   cat /proc/irq/<irq #>/smp_affinity_list

Associate each interrupt with a CPU core.

   echo <CPU #> > /proc/irq/<irq #>/smp_affinity_list

Note: User configured SMP affinity may change after unloading/loading RoCE
      driver, or other driver configuration changes that need to reinitialize
      IRQs. The user may need to configure it again.

BNXT_EN Module Parameters
=========================

On newer 3.x/4.x kernels, the driver does not support any driver parameters.
Please use standard tools (sysfs, ethtool, iproute2, etc) to configure the
driver.

The only exception is the "num_vfs" module parameter supported on older 2.6
kernels to enable SR-IOV.  Please see the SR-IOV section above.


BNXT_EN Driver Defaults
=======================

Speed :                    1G/2.5G/10G/25G/40G/50G/100G/200G/400G depending on the board.

Flow control :             None

MTU :                      1500 (range 60 - 9500) Maximum MTU controlled by
			   firmware and set during driver initialization.

Rx Ring Size :             511 (range 0 - 2047)

Rx Jumbo Ring Size :       2044 (range 0 - 8191) automatically adjusted by the
                           driver.

Tx Ring Size :             511 (range (MAX_SKB_FRAGS+2) - 2047)

                           MAX_SKB_FRAGS varies on different kernels and
                           different architectures. On most kernels for
                           x86, MAX_SKB_FRAGS is 17.

Number of RSS/TSS channels:Up to 64 combined channels or match the number of
                           CPUs whichever is higher, subject to chip limits.
			   In the case of NPAR, this will be upto 16 combined
			   channels.

TSO :                      Enabled

GRO (hardware) :           Enabled

LRO :                      Disabled

Coalesce rx usecs :        6 usec

Coalesce rx usecs irq :    1 usec

Coalesce rx frames :       6 frames

Coalesce rx frames irq :   1 frame

Coalesce tx usecs :        28 usec

Coalesce tx usecs irq :    2 usec

Coalesce tx frames :       30 frames

Coalesce tx frames irq :   2 frame

Coalesce stats usecs :     1000000 usec (range 250000 - 1000000, 0 to disable)


Statistics
==========

The driver reports all major standard network counters to the stack.  These
counters are reported in /proc/net/dev or by other standard tools such as
netstat -i.

Note that the counters are updated every second by the firmware by
default.  To increase the frequency of these updates, ethtool -C can
be used to increase the frequency to 0.25 seconds if necessary.

More detailed statistics are reported by ethtool -S.  Some of the counters
reported by ethtool -S are for diagnostics purposes only.  For example,
the "rx_drops" counter reported by ethtool -S includes dropped packets
that don't match the unicast and multicast filters in the hardware.  A
non-zero count is normal and does not generally reflect any error conditions.
This counter should not be confused with the "RX-DRP" counter reported by
netstat -i.  The latter reflects dropped packets due to buffer overflow
conditions.

Another example is the "tpa_aborts" counter reported by ethtool -S.  It
counts the LRO (Large Receive Offload) aggregation aborts due to normal
TCP conditions.  A high tpa_aborts count is generally not an indication
of any errors.

The "rx_ovrsz_frames" counter reported by ethtool -S may count all
packets bigger than 1518 bytes when using earlier versions of the firmware.
Newer version of the firmware has reprogrammed the counter to count
packets bigger than 9600 bytes.

If the "rx_discards" and "rx_buf_errors" counters are high compared to the
total recieve packets for that ring, it generally means that the host CPU
is not processing the incoming packets fast enough and causing packet drops.
The number of receive packets dropped is indicated by these counters.
Increasing the receive ring size may reduce the number of dropped packets (See
BNXT_EN Driver Settings section above, examples 5 and 6).

On BCM573xx and BCM574xx devices, the condition that triggers the
"rx_buf_errors" counter to increment requires a reset of the ring.  The
driver will print a warning message one time only when a reset is required as
shown in this example:

bnxt_en 0000:07:00.0 eth0: RX buffer error 2260004

This warning message will appear only one time even if there are multiple
of these errors requiring reset from one or multiple devices.  The "rx_resets"
counter will also increment for each reset in response to this condition.  If
the "rx_resets" counter is high, it is recommended to increase the RX ring size
to reduce these reset events which are disruptive.

The "rx_l4_csum_errors" counter will increment for every TCP/UDP checksum
error detected by hardware on each ring if RX checksum offload is enabled.
Such packets will be rejected by the stack and similar stack error
counters for TCP/UDP will also increment.  Note that IPv4 checksum is
always verified by the stack and not offloaded.


Unloading and Removing Driver
=============================

rmmod bnxt_en

Note that if SR-IOV is enabled and there are active VFs running in VMs, the
PF driver should never be unloaded.  It can cause catastrophic failures such
as kernel panics or reboots.  The only time the PF driver can be unloaded
with active VFs is when all the VFs and the PF are running in the same host
kernel environment with one driver instance controlling the PF and all the
VFs.  Using Linux Containers is one such example where the PF driver can be
unloaded to gracefully shutdown the PF and all the VFs.


Updating Firmware for Broadcom NetXtreme-C and NetXtreme-E devices
==================================================================

Controller firmware may be updated using the Linux request_firmware interface
in conjunction with the ethtool "flash device" interface.

Using the ethtool utility, the controller's boot processor firmware may be
updated by copying the 2 "boot code" firmware files to the local /lib/firmware/
directory:

    cp bc1_cm_a.bin bc2_cm_a.bin /lib/firmware

and then issuing the following 2 ethtool commands (both are required):

    ethtool -f <device> bc1_cm_a.bin 4

    ethtool -f <device> bc2_cm_a.bin 18

NVM packages (*.pkg files) containing controller firmware, microcode, 
pre-boot software and configuration data may be installed into a controller's
NVRAM using the ethtool utility by first copying the .pkg file to the local
/lib/firmware/ directory and then executing a single command:

    ethtool -f <device> <filename.pkg>

Note: do not specify the full path to the file on the ethtool -f command-line.

Note: root privileges are required to successfully execute these commands.

After "flashing" new firmware into the controller's NVRAM, a cold restart of 
the system is required for the new firmware to take effect. This requirement
will be removed in future firmware and driver versions.

Updating Firmware for Broadcom Nitro device
===========================================

Nitro controller firmware should be updated from Uboot prompt by following the
below steps

    sf probe
    sf erase 0x50000 0x30000
    tftpboot 0x85000000 <location>/chimp_xxx.bin
    sf write 0x85000000 0x50000 <size in hex>

Devlink
=======

In kernel versions 4.6 or higher, some operations on bnxt_en driver can be done
using devlink.

Devlink tool is part of iproute2 routing commands and utilities. Latest devlink
can be downloaded from http://www.kernel.org/pub/linux/utils/net/iproute2/,
if it is not already installed.

As devlink tool is evolving, use latest kernel and iproute2 tool available for
all features via devlink.

Following are some examples on how to use devlink. See the devlink man page
for more information.

Some devlink examples:

1. Command to display board information and firmware versions:

   devlink dev info [DEV]

Example:
   devlink dev info pci/0000:3b:00.0

will show:

pci/0000:3b:00.0:
  driver bnxt_en
  serial_number B0-26-28-FF-FE-C8-85-20
  versions:
      fixed:
        board.id BCM957508-P2100G
        asic.id 0x1750
        asic.rev B1
      running:
        fw.psid 0.0.6
        fw 218.1.220.0
        fw.mgmt 218.1.202.0
        fw.mgmt.api 1.10.2
      stored:
        fw.psid 0.0.6
        fw 218.1.220.0
        fw.mgmt 218.1.202.0

Note that 'info' is supported in 5.1 or higher kernel versions.

2. Updating firmware:

   devlink dev flash DEV file PATH

Example:
   devlink dev flash pci/0000:3b:00.0 file BCM957454A4540C.pkg

Note: File path is relative to /lib/firmware directory.

Note that 'flash' is supported in 5.1 or higher kernel versions.

3. Dump device level driver parameter information:

   devlink dev param show

4. Display the device level driver parameter information:

   devlink dev param show [ DEV name PARAMETER ]

Example:
   devlink dev param show pci/0000:3b:00.0 name enable_sriov

will show:

pci/0000:3b:00.0:
  name enable_sriov type generic
    values:
      cmode permanent value true

Note that 'param' is supported in 4.19 or higher kernel versions.

5. Set the device level driver parameter information:

   devlink dev param set DEV name PARAMETER value VALUE cmode \
                         { runtime | driverinit | permanent }

Example:
   devlink dev param set pci/0000:3b:00.0 name enable_sriov \
               value false cmode permanent

6. Dump health reporter information:

   devlink health show [ DEV reporter REPORTER ]

Example:
   devlink health show pci/0000:3b:00.0 reporter fw

might show:

pci/0000:3b:00.0:
  name fw
    state healthy error 2 recover 2 grace_period 0 auto_recover true

Note that health reporters are created only when corresponding features
are enabled in NVM configuration.

7. Run diagnostics via health reporter:

   devlink health diagnose DEV reporter REPORTER

Example:
   devlink health diag pci/0008:01:00.1 reporter fw

might show:

Status: healthy Severity: normal Resets: 7 Arrests: 0 Survivals: 0 Discoveries: 0 Fatalities: 0 Diagnoses: 0

The diagnose output may expose certain implementation details. In particular,
the various counters constitute debugging information intended for internal
use only and should not be interpreted by the user.

8. Extract device coredump via health reporter:

  devlink health dump show DEV reporter REPORTER

Example:
  devlink health dump show pci/0008:01:00.1 reporter fw

The binary output rendered is useful to developers for debugging purposes and
is not intended to be interpreted by the user. The devlink core stores the most
recent dump and will not capture a new one until an existing dump is cleared
using:

  devlink health dump clear DEV reporter REPORTER

Devlink health dumps may be captured automatically on errors. If no stored dump
exists, then devlink health dump show will trigger a capture.

9. Reset firmware using reload command:

   devlink dev reload DEV action fw_activate

Example:
   devlink dev reload pci/0000:3b:00.0 action fw_activate

will show, if it is successful:

reload_actions_performed:
    driver_reinit fw_activate

Note that 'reload' actions are supported in 5.10 or higher kernel versions.

10. Reload stats can be seen using command:

   devlink dev show -s

Example:

$ devlink dev show -s
pci/0000:3b:00.0:
  stats:
      reload:
          fw_activate:
            unspecified 2
      remote_reload:
          driver_reinit:
            unspecified 0
          fw_activate:
            unspecified 0 no_reset 0
pci/0000:3b:00.1:
  stats:
      reload:
          fw_activate:
            unspecified 0
      remote_reload:
          driver_reinit:
            unspecified 2
          fw_activate:
            unspecified 2 no_reset 0

11. See devlink man pages for more options.

Error Recovery
==============

Error recovery is a new feature that can facilitate the automatic recovery
from some fatal firmware or hardware errors.  Without this feature, such
errors often cause prolonged outage, sometimes requiring cold boot to
fully recover.

When the feature is enabled, both the firmware and the driver will take
part in monitoring the health of the adapter.  If the firmware detects an
error, a notification is sent to the driver and a coordinated reset of
the adapter will be initiated.  If the driver detects that the firmware is
unresponsive, it can also initiate a reset.  The reset will generally take
seconds to complete and network functions will be automatically restored
after the reset.  In cases where heavy or converged traffic are used,
transmit timeouts may be reported.

If the kernel supports the devlink health framework, health related
information and counters will be reported to devlink and visible to the
user.

Known limitations:

1. The error recovery process generally takes seconds to complete. If
the device is brought down (ifdown) before error recovery has completed,
the error recovery process will abort and the device will be brought down.
Kernel message below will be displayed:

bnxt_en 0000:04:00.0 eth0: FW reset in progress during close,	\
                           FW reset will be aborted

2. If the above happens or if the error recovery process fails for other
reasons, bringing up the device will fail and the kernel message below will
be displayed:

bnxt_en 0000:04:00.0 eth0: A previous firmware reset did not complete, aborting

Reloading the driver is required.  If errors persist while reloading the
driver, a reboot may be required.

3. If the driver is in the middle of loading or initializing on a PCIe
function or in the middle of a transmit timeout recovery while firmware
detects an error, the recovery will sometimes not succeed on this function.
The driver may abort loading or initializing.  Kernel messages similar to below
will be displayed:

bnxt_en 0000:65:00.0 (unnamed net_device) (uninitialized): Firmware not responding, status: 0x448100
OR
bnxt_en 0000:65:00.0 eth0: Abandoning msg {0xb1 0xb7bd} len: 0 due to firmware status: 0x448100

If this happens, try to unload and reload the driver again, or unbind and rebind
the PCIe function using sysfs.

4. For error recovery to succeed, the interface should have no disruptions
during the process that might reconfigure the device.
In other words, for reliable error recovery, it is recommended to not run
any configuration changes (such as unloading the RoCE Driver, ethtool self-tests etc)
while error recovery is in progress.
If at all changes are done, and recovery does not succeed, try the below actions to recover:
a. Bring up the device
b. Unload and reload both the drivers
c. Unbind and rebind the PCIe function using sysfs

5. PCIe FLR is not supported in driver.

Multi-root NUMA Direct
======================

On multi-root systems, the NUMA Direct feature can be enabled via the
'numa_direct' ethtool private flag:

    ethtool --set-priv-flags eth0 numa_direct on

and confirmed as follows:

    ethtool --show-priv-flags eth0

    Private flags for eth0:
    numa_direct: on

Note, if the device does not advertise multi-root capability, then the
set command will return an operation not supported error when attempting
to enable the feature.

Once enabled, it is possible to add special ntuple filters in order to
direct RX traffic from any port via the PF attached to the desired NUMA
node by providing a flow specification.

The ntuple filters are managed in same manner as detailed in section 19
of 'BNXT_EN Driver Settings' above, except that the target action is
different. An action of '-9999' will direct traffic matching the flow
specification to the network device where the rule is installed. For
example:

    ethtool -N eth0 flow-type tcp4 dst-ip 10.0.0.1 action -9999
    ethtool -N eth1 flow-type tcp4 dst-ip 10.0.0.2 action -9999

would direct the matching TCP traffic destined to 10.0.0.1 via eth0 while
the traffic destined for 10.0.0.2 would be delivered via the eth1 PF. If
these devices are attached to different NUMA nodes, then this effectively
directs traffic to the desired socket, with RSS still directing traffic
automatically to the subset of cores within this node.

On BCM575xx and later parts, it is also possible to steer traffic to a
specific queue. The queues are numbered backwards, with -10000 corresponding
to queue 0, -10001 to queue 1 and so on. This can be used to not only
direct traffic to a desired socket, but also to a specific core where the
the traffic should be sunk.

The driver will make a best effort to ensure that descriptor and packet
memory is allocated on the appropriate node, but the user should ideally
limit the number of device queues configured for each PF to not exceed the
number of cores in each socket. This also requires that the interrupts
associated with a given PF are also routed to cores within its attached
NUMA node. The 'Set IRQ Balance Manually' section details how this can be
achieved.

Firmware Core Reset on TX timeout
=================================

Firmware core reset on TX timeout can be enabled via the 'core_reset_tx_timeout'
ethtool private flag:

    ethtool --set-priv-flags eth0 core_reset_tx_timeout on

Once enabled, a core reset will be issued to the firmware when TX timeout is
detected by the driver.

DIM (Dynamic Interrupt Moderation)
==================================

Dynamic Interrupt Moderation refers to changing the interrupt
moderation configuration of a channel in order to optimize packet
processing. The mechanism includes an algorithm which decides if and how to
change moderation parameters for a channel, usually based on performing an
analysis on runtime data sampled from the system.

Run the following ethtool command to check whether `Adaptive Rx`(DIM) is ON.

ethtool -c eth0

Coalesce parameters for eth0:
Adaptive RX: on  TX: n/a

The bnxt_en driver does not support DIM in the Tx direction.

To view the coalesce settings altered dynamically by the networking stack's
DIM algorithm, run the below ethtool command:

ethtool --per-queue eth0 --show-coalesce (To see for all rings)

OR

ethtool -Q eth0 queue_mask 0x1 --show-coalesce (To see for ring#0)

Note that the queue number corresponding to the queue_mask always starts from
0 for combined channels, as well as for separate Rx channels and Tx channels.

Note that this per-queue command and kernel infrastructure support is available
only in newer kernels.

HWMON support
=============

On newer kernels (from 4.9 onwards) with CONFIG_HWMON enabled, bnxt_en driver
creates the standard sysfs infrastructure in the hardware monitoring core to
display NIC temperature attributes.

Driver will expose the following attributes:
1) Input temperature(temp1_input): current temperature of the device
2) Warning temperature(temp1_max): warning threshold temperature
3) Critical temperature(temp1_crit): critical threshold temperature
4) Emergency Temperature(temp1_emergency): Emergency threshold temperature
5) Shutdown Temperature(temp1_shutdown): Shutdown threshold temperature

Some boards may not have all the threshold temperatures defined and if
it is not defined, the driver will not expose that threshold attribute.

Each threshold temperature has an associated alarm file, containing a
boolean value. 1 means that an alarm condition exists. i.e, the current
device temperature is greater than the threshold temperature. 0 means no alarm.

For example, for dualport BCM957508-P2100G will have 2 hwmon directories
(one for each PCI function) under "/sys/class/hwmon/hwmon[X,Y]".

# grep -H -d skip . /sys/class/hwmon/hwmon2/*
/sys/class/hwmon/hwmon2/name:bnxt_en
/sys/class/hwmon/hwmon2/temp1_crit:100000
/sys/class/hwmon/hwmon2/temp1_crit_alarm:0
/sys/class/hwmon/hwmon2/temp1_emergency:110000
/sys/class/hwmon/hwmon2/temp1_emergency_alarm:0
/sys/class/hwmon/hwmon2/temp1_input:78000
/sys/class/hwmon/hwmon2/temp1_max:95000
/sys/class/hwmon/hwmon2/temp1_max_alarm:0
/sys/class/hwmon/hwmon2/temp1_shutdown:105000
/sys/class/hwmon/hwmon2/temp1_shutdown_alarm:0

USHI based NICCLI
=================

USHI stands for the User Space HWRM Interface. It is an interface which allows
NICCLI to send HWRM commands to the firmware without depending on a driver.

But USHI cannot work if secure kernel mode is enabled.
The setting is typically selected during the kernel compilation by specifying
CONFIG_IO_STRICT_DEVMEM=y. In such a case, iomem must be set to 'relaxed'
mode to allow USHI based NICCLI to work. This can be done by specifying
iomem=relaxed in the kernel command line in the grub config.
