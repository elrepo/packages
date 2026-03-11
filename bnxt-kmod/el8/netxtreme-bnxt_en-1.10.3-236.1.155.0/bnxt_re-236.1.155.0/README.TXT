                               README Notes
                    Broadcom bnxt_re Linux RoCE Driver

                              Broadcom Inc.
                         5300 California Avenue,
                            Irvine, CA 92617

                   Copyright (c) 2015 - 2016 Broadcom Corporation
                   Copyright (c) 2016 - 2018 Broadcom Limited
                   Copyright (c) 2018 - 2025 Broadcom Inc.
                           All rights reserved


Table of Contents
=================

  Introduction
  BNXT_RE Driver Dependencies
  BNXT_RE Driver compilation
  Configuration Tips
  Limitations
  BNXT_RE Dynamic Debug Messages
  BNXT_RE Compiler Switches
  DCB Settings
  Congestion Control
  SR-IOV and VF Resource Distribution
  Link Aggregation
  BNXT_RE Driver Statistics
  QP Information in debugfs
  UDCC RTT configuration
  Sysfs counters

Introduction
============

This file describes the bnxt_re Linux RoCE driver for the Broadcom NetXtreme-C
and NetXtreme-E 10/25/40/50/100/200/400 Gbps Ethernet Network Controllers.

Note: Starting from 219.0 release, driver supports only RoCE v2.
RoCE v1 support is deprecated. Please refer Broadcom Linux RoCE Configuration
Guide for details.

BNXT_RE Driver Dependencies
===========================

The RoCE driver has dependencies on the bnxt_en Ethernet counterpart.

  - It also has dependencies on the IB verbs kernel component
    (Details given below).


BNXT_RE Driver compilation
==========================

To compile bnxt_re:
        - Untar netxtreme-bnxt_en-<version>.tar.gz
	$make

 => The bnxt_en driver must be built first before building the bnxt_re driver.
    Also, while loading the drivers bnxt_en driver must be loaded first.

Configuration Tips
==================

- It is recommended to use same host OS version on client and server while
  running NFS-RDMA/iSER/NVMEoF tests. Heterogeneous host OS may lead to unexpected
  results. This is due to the incompatible ULP server and Client kernel modules.

- It is recommended to assign at least 3GB RAM to VMs used for memory intensive
  applications like NFSoRDMA, iSER, NVMoF etc.

- When using large number of QPs (close to maximum supported) along with large
  message sizes it is recommended to increase the `max_map_count` kernel parameter
  using sysctl to avoid memory map failures in the application.
  Please refer to https://www.kernel.org/doc/Documentation/sysctl/vm.txt on how to tune
  this kernel parameter.

- When TCP/IP and RoCE traffic are running simultaneously with high work load or
  RoCE traffic with high work load, high CPU utilization can result,
  leading to CPU soft lockup. Hence it is recommended to spread the workload
  across the available CPU cores. This can be achieved by setting the SMP
  affinity of the interrupts and RoCE applications.
  Please refer to OS documentation for setting smp_affinity and specific
  commands like taskset etc.

- To avoid the SQ full reported during iSER stress testing (kernels SLES 12 and later,
  RHEL 8.x and later), configure the minimum tx depth for the QPs to 4096.
  All connections getting established after setting min_tx_depth uses the user specified values.

  echo 4096 > /sys/kernel/config/bnxt_re/bnxt_re0/ports/1/tunables/min_tx_depth

- For heavy RDMA-READ workloads with large number of active QPs,
  a higher ack-timeout value is recommended.
  For example:
   ib_read_bw with -q 4096 would require ack timeout 18. Ack timeout is
   controlled by option "-u".
   ib_read_bw --report_gbits -F -m 4096 -q 4096 -d bnxt_re2 -x 3 -u 18 -D 60 -s 65536

  For lower speeds (<= 25 Gbps) case, we recommend:
  1.Use high ACK timeout (codepoint 20 or more) when QP count is between 2K and 32K;
  2.Use higher ACK timeout (codepoint 21 or more) when QP count is more than 32K.

- For use cases where the adapter QP limit is exercised or active qps are
  close to adapter limits, ack timeout needs to be increased to 24 to avoid
  retransmissions and loss of performance.
  For example:
   For multiple instances of ib_send_bw/ib_read_bw/ib_write_bw, which creates
   total of 64K QPs, specify higher ack timeout in each application instance
   using -u 24.

- Upon extensive QP scaling testing, it is highly possible that "oom-killer" invokes and
  kills the applications due to lack of memory.

  Example - 32 instances of below command need ~136GB RAM.
  ib_send_bw -d bnxt_re0 -F -i 1 -s 128K -m 1024 -x 3 -p $port -t 1000 -c RC -u 21 -Q 100 -D 10 -q 2048 -r 500 --mr_per_qp --send_sge_per_wqe 8

  It is required to observe the reason for oom-killer and check if enough RAM is available
  to support such workload.

Limitations
===========
- GIDs corresponding to IPv4 and IPv6 addresses maybe missing after
  device creation sequences such as  driver load or device error recovery.

  e.g. when RoCE v1 and RoCE v2 are enabled on the adapter,
  ibv_devinfo -d <device> -vvv
  Shows:
	GID[  0]: fe80:0000:0000:0000:5e6f:69ff:fe1e:2f3e, RoCE v1
	GID[  1]: fe80::5e6f:69ff:fe1e:2f3e, RoCE v2

  Should show:
	GID[  0]: fe80:0000:0000:0000:5e6f:69ff:fe1e:2f3e, RoCE v1
	GID[  1]: fe80::5e6f:69ff:fe1e:2f3e, RoCE v2
	GID[  2]: 0000:0000:0000:0000:0000:ffff:c0a8:0033, RoCE v1
	GID[  3]: ::ffff:192.168.0.51, RoCE v2
	GID[  4]: 2001:0000:0000:0000:0000:0000:0000:0051, RoCE v1
	GID[  5]: 2001::51, RoCE v2
  This is due to device creation sequence from netdev event context.
  The design change to avoid these failures will be available in future
  releases.  As a workaround, bring down the L2 interface (ifconfig down)
  and bring it up (ifconfg up). This will force stack to add the GIDs again.

- Stack traces seen with following message during link
  down and other administrative events like PFC enable/disable.
  "task ib_write_bw:406494 blocked for more than 120 seconds"
  This is because of the un-graceful destroy of the resources and FW
  can take more time to destroy these resources. RoCE driver can
  wait upto 240 seconds before hitting the timeout. These error messages
  will stop once all resources are destroyed.

- When the applications are run simultaneously, there is a chance of commands
  getting failed with an error message "send failed - retries:2000". This is due
  to the CMD Queue is getting full because applications are creating/destroying
  resources simultaneously. This is also observed, when the applications are
  ungracefully killed and if restarted before the active resources are cleaned
  up when killed. In any case if this issue is seen, restart the application with
  delay between the applications.

- For error recovery to succeed, the VF interface should be in the ifup state
  with no disruptions during the process that might reconfigure the device.
  In other words, for reliable error recovery, it is recommended to not run
  any configuration changes (such as unloading the RoCE Driver, bonding inerface,
  ethtool self-tests etc) while error recovery is in progress.
  If at all changes are done, and recovery does not succeed, try the below
  actions to recover:
   -  Unload and reload both the drivers roce and L2
   -  Unbind and rebind the PCIe function using sysfs

- Error messages seen during RoCE driver load/unload after a live FW update/FW reset,
  if L2 VF interface is down during the reset.
  To avoid errors bring up all ethernet interfaces up before loading RoCE driver.

- When remote directories are mounted using NFS-RDMA, unloading bnxt_re shall
  cause system hang and the system needs a reboot for normal operations.
  Always unmount all active NFS mounts over bnxt_re interface, before unloading
  bnxt_re driver.

- Using same interface MTU on both client and server is recommended.  User can
  see unexpected results if there is a mismatch in interface MTUs on Client
  and Server.

- Changing MAC address of the interface while bnxt_re is loaded can trigger failure
  during GID deletion. Unload bnxt_re driver before changing the interface MAC address.

- The legacy FMR Pool is not supported yet.

- When RoCE mirroring is supported by the firmware, the driver reserves one GID entry
  for Raw Ethertype QP to support packet mirroring. As a result, gid_tbl_len is
  reported as one less than the actual max_sgid.

- Tunnel is not supported yet.

- When RoCE VFs are created, destroying the VFs may take a longer time to complete.
   For example, 64 VFs destroy may take up to 20 sec.

- Avoid performing PCI reset when QPs are active. Driver has no way to know
  about this reset and eventually causes PCI Fatal errors and a system crash when
  QPs are active and Doorbell recovery/pacing enabled.

- On AMD64 chipset (recently noticed on AMD EPYC 9554) with IOMMU enabled systems,
  users can notice the below error string from bnxt_re driver.

  infiniband bnxt_re0: bnxt_re_build_reg_wqe: bnxt_re_mr 0xff211d4fb9eaa800  len (65536 > 4096)
  infiniband bnxt_re0: bnxt_re_build_reg_wqe: build_reg_wqe page[0] = 0xffffffffffff0000
  infiniband bnxt_re0: bad_wr seen with opcode = 0x20

  Primary issue is AMD IOMMU is providing iova reaching to max U64 and that is
  not expected. Contact Broadcom support for additional information.

- Driver is no longer supporting max_msix_vec module parameter.
  The num_comp_vectors in the output of "ibv_devinfo -v" is controlled
  by the L2 driver ring counts before loading bnxt_re driver. If users
  want more completion vectors for RoCE (ie. upto 64 or num_cpus), unload
  bnxt_re driver, reduce the L2 rings using ethtool and then load RoCE driver.

- Driver supports display of UDCC session statistics through debugfs.
  In case true flow module is not able to create the session for any
  reason, debugfs will endup having stale UDCC session entries and
  session queries of those returns "Input/output error". At later
  point of time if the sessions are created with the same session ids,
  the same entries will be reused.

BNXT_RE Dynamic Debug Messages
==============================
The bnxt_re driver supports the Linux dynamic debug feature.

All error, warning and info messages are logged by default.
Any debug messages if needed, could be enabled by writing to
the standard <debugfs>/dynamic_debug/control file.
Debug messages can be enabled/disabled at various granularities
like - module, file, function, a range of line numbers or a
specific line number.

The following kernel document describes this in detail with examples:
https://www.kernel.org/doc/Documentation/dynamic-debug-howto.txt

A few examples on how to use this with bnxt_re driver:

1) To check the debug messages that are available in bnxt_re:
# cat /sys/kernel/debug/dynamic_debug/control | grep bnxt_re

2) To enable all debug messages in bnxt_re during load time:
# insmod bnxt_re.ko  dyndbg==p

3) To enable all debug messages in bnxt_re after loading:
# echo "module bnxt_re +p" > /sys/kernel/debug/dynamic_debug/control

4) To disable all debug messages in bnxt_re after loading:
# echo "module bnxt_re -p" > /sys/kernel/debug/dynamic_debug/control

5) To enable a debug message at a specific line number in a file:
# echo -n "file qplib_fp.c line 2554 +p" > /sys/kernel/debug/dynamic_debug/control


BNXT_RE Compiler Switches
=========================

ENABLE_DEBUGFS - Enable debugFS operation

ENABLE_RE_FP_SPINLOCK - Enable spinlocks on the fast path bnxt_re_qp queue
			resources

ENABLE_FP_SPINLOCAK - Enable spinlocks on the fast path bnxt_qplib queue
		      resources

ENABLE_DEBUG_SGE - Enable the dumping of SGE info to the journal log

DCB Settings
============

The current software requires the following Traffic Class mapping.

TC0: L2 traffic
TC1: RoCE Traffic
TC2: CNP Traffic and L2 Traffic
TC3 – TC7: L2 traffic

Each TC can be mapped to different priority. So while mapping priority to traffic
class, make sure that TC1 is mapped for RoCE priority and TC2 is mapped for CNP priority.
RoCE traffic class support only one DSCP value programmed through the DSCP App TLV.

Since the CNP Traffic class (TC2) is shared between CNP and L2 traffic, multiple DSCP
values are supported for this traffic class.  The current solution requires that the DSCP
App TLV for CNP should be programmed at the end, after programming other App TLVs. If
there are any changes(add/delete) in the dscp configuration of CNP traffic class,
user needs to re-program the dscp values used for CNP packets.

Users can program using tools such as niccli/lldptool or
by installing the bnxt_re_conf package distributed by Broadcom.
While adding new settings, please make sure that the existing settings
(say, app TLVs, ETS configuration, PFC config etc) are removed.

Example usages of lldptool is given below.

lldptool
--------
Note: If the switches are capable of handling RoCE TLVs, the following
settings are not required as adapter will override local settings, if any,
with the switch settings.

Following steps are recommended to configure
the local adapter to set DCB parameters, in case switches are not capable
of DCB negotiations.

# Load L2 driver and make sure port and Link are  UP
 service lldpad start
 lldptool -L -i p6p1 adminStatus=rxtx
#Disable PFC
lldptool -T -i <ethx> -V PFC enabled=none
#Delete the existing app TLVs. For eg:
lldptool -T -i <ethx> -V APP -d app=3,5,26
lldptool -T -i <ethx> -V APP -d app=3,3,4791
#For RoCE-V2 protocol with Priority-5
 lldptool -T -i p6p1 -V APP app=5,3,4791
 lldptool -T -i p6p1 -V ETS-CFG tsa=0:ets,1:ets,2:strict,3:strict,4:strict,5:strict,6:strict,7:strict \
    up2tc=0:0,1:0,2:0,3:0,4:0,5:1,6:0,7:0  tcbw=10,90,0,0,0,0,0,0
 lldptool -T -i p6p1 -V PFC enabled=5
 service lldpad restart

Note: Please refer man pages of lldptool, lldptool-app,
lldptool-ets, lldptool-pfc, etc. for more details

Note: VF inherits the PFC settings of the PF. VF doesn't have privilege to
set DCB parameters using lldptool. No need of running lldpad service on the VM.

Note: The driver supports only one priority for RoCE traffic.

Note: The driver by default supports Priority VLAN Tagging i.e it adds a NULL
VLAN tag if a priority is configured for RoCE Traffic, without VLANs being
configured. However, for customers who are interested only in PFC via DSCP,
driver provides a knob to disable the auto VLAN 0 tag insertion.

echo 1 > /sys/kernel/config/bnxt_re/bnxt_re0/ports/1/cc/disable_prio_vlan_tx

Guidelines for changing DCB settings
------------------------------------

Sample programming for multiple DSCP values for TC2.
TC1 RoCE pri – 5
TC1 RoCE dscp – 59
TC2 CNP pri – 6
TC2 CNP dscp – 49
TC2 L2 dscp – 55
TC2 L2 dscp – 54
All other priorities are mapped to remaining traffic classes.

bnxt_re_conf package to program DCB
-----------------------------------

A udev rule should be installed to program the default DCB setting upon RoCE driver
load. User will have the flexibility to modify the default values programmed
by the udev rule through a config file. User can also opt to not run the udev rule
upon a RoCE driver load.
The udev rule, scripts and config file will be part of an installer package(bnxt_re_conf).

The entire mechanism(udev->bnxt_re_conf.sh->bnxt_setupcc.sh) involves a
udev rule(90-bnxt_re.rules) that is triggered when the bnxt_re device is added.
The udev rule invokes a wrapper script(bnxt_re_conf.sh) that would take values
of the required parameters from a config file as mentioned below and will run
the configuration using bnxt_setupcc.sh.

The config file parameters are as follows:

ENABLE_FC: Enables flow control. Default 1.
           If set to 0, then no configuration action is taken. User has to manually configure the RoCE lossless configuration.

FC_MODE -m [1-3]: Selects the mode of operation regarding Priority Flow Control (PFC) and Congestion Control (CC).
                  1: PFC only
                  2: CC only
                  3: Both PFC and CC

ROCE_PRI -r [0-7]: Sets the RoCE packet priority. The range 0-7 corresponds to the possible priority levels in Ethernet frames.

ROCE_DSCP -s VALUE: Sets the RoCE Packet DSCP (Differentiated Services Code Point) value, used in IP headers to determine traffic handling.

CNP_PRI -c [0-7]: Sets the RoCE CNP packet priority.

CNP_DSCP -p VALUE: Sets the DSCP value for RoCE CNP packets.

ROCE_BW -b VALUE: Sets the bandwidth percentage for ETS (Enhanced Transmission Selection) configuration. Default is typically 50%.

UTILITY: Enables the user to select the utility tool to program.
	 4: dcb (default)
	 3: niccli

NOTE: If the user is using the tarball, these files are to be copied to their respected paths as follows:
        cp bnxt_re.conf /etc/bnxt_re/bnxt_re.conf
        cp bnxt_re_conf.sh /usr/bin/bnxt_re_conf.sh
        cp bnxt_setupcc.sh /usr/bin/bnxt_setupcc.sh
        cp 90-bnxt_re.rules /usr/lib/udev/rules.d/90-bnxt_re.rules

Note: Load the drivers and make sure the udev rule gets invoked correctly.
      For device with index 1:
         $ niccli -i 1 get_qos

      Users can see the output of udev rule commands in “/tmp/bnxt_setup_IB_device.log”.
      Users have to wait until the udev rule has executed successfully on all the IB devices.
      If there are multiple IB devices registered, running the udev rule on all devices may take time as the operation is serial.

Note: If the user doesn't want to install the bnxt_re_conf package and the utilities,
      please configure the RoCE lossless configuration by using bnxt_setupcc.sh or steps mentioned in the above section.

Note: The bnxt_re_conf script udev rule supports programing NIC profiles with 3 TCs(MP 12) or with 8 TCs(MP 17) only.
      For other profiles, user has to disable the udev rule and program manually.

Removing the DCB settings
-------------------------
RoCE driver gets auto loaded by a udev rule as soon as L2 interface is detected.
If users want to change the Traffic class mapping using tools like tc qdisc,
lldptool, niccli etc, remove the existing settings configured
to avoid Traffic class mapping errors.

Use the following commands as reference to remove the settings.
Users can setup new configuration after this.

#Example commands using niccli ( Get the interface id from command 'niccli.x86_64 --list')

#Conifgure only 1 traffic class
niccli.x86_64 -i 5 set_ets tsa=0:strict,1:strict,2:strict priority2tc=0:0,1:0,2:0,3:0,4:0,5:0,6:0,7:0 tcbw=100
#Disable PFC
niccli.x86_64 -i 5 set_pfc enabled=none
#Delete the app TLVs configured by RoCE driver
niccli.x86_64 -i 5 set_apptlv -d app=7,5,48
niccli.x86_64 -i 5 set_apptlv -d app=3,5,26
niccli.x86_64 -i 5 set_apptlv -d app=3,3,4791

#Example commands using lldptool
lldptool -T -i ens7f0np0 -V ETS-CFG tsa="0:strict,1:strict,2:strict" up2tc=0:0,1:0,2:0,3:0,4:0,5:0,6:0,7:0  tcbw=100
lldptool -T -i ens7f0np0 -V PFC enabled=none
lldptool -T -i ens7f0np0 -V APP -d app=7,5,48
lldptool -T -i ens7f0np0 -V APP -d app=3,5,26
lldptool -T -i ens7f0np0 -V APP -d app=3,3,479

Note: Users can also avoid auto-loading of bnxt_re driver by disabling the udev rule in
/usr/lib/udev/rules.d/90-rdma-hw-modules.rules

Congestion Control
===================

Explicit Congestion Notification (ECN) is a congestion avoidance mechanism.
In this protocol a Congestion Notification Packet(CNP) signals the existence
of congestion to the remote transmitter. Reacting to CNP, the transmitter reduces
the transmit rate on a transmit-flow for a given time quanta. CNP is generated
by the receiver when it detects congestion in the receive processing pipe.

To export the tuning parameters RoCE driver uses configfs support from linux
kernel. Following are the steps to configure congestion control parameters.

	1. Pre-requisites
	   ===============
		1.a Host base lldpad is configured for RoCE-v2 protocol
		    and a valid priority is assigned to RoCE-v2.
			ref: "lldptool" section of this document.

	2. Mount per-port-configfs interface
	   ===================================
		2.a Load RoCE driver
		2.b ls /sys/kernel/config should list directory "bnxt_re"
		2.c Create a directory in configfs-path with the RoCE device name.
		    E.g. for bnxt_re1 use following:
			mkdir -p /sys/kernel/config/bnxt_re/bnxt_re1
		2.d ls /sys/kernel/config/bnxt_re/bnxt_re1
			ls /sys/kernel/config/bnxt_re/bnxt_re1/ports/1/cc/
				cnp_dscp  cnp_prio  apply  cc_mode roce_prio
				ecn_enable  g  inact_cp  init_cr  init_tr
				nph_per_state  rtt  tcp_cp  roce_dscp  ecn_marking
		2.e To enable CC, set 1 to ecn_enable, To disable, set 0
			E.g.
			    echo -n 0x1 > ecn_marking
			    echo -n 0x0 > ecn_enable
			Note: There are other tunables under same directory. Use these fields as
			      needed.
			...
		2.f Check for "service prof type" is supported, by using the
			following command:-
			cat /sys/kernel/debug/bnxt_re/<Device name>/info | grep fw_service_prof_type_sup
			E.g. for bnxt_re0 use following:
			cat /sys/kernel/debug/bnxt_re/bnxt_re0/info | grep fw_service_prof_type_sup

			If "service prof type" is supported, refer to
			"DCB settings" section of this document.
			If "service prof type" is *not* supported, follow the
			steps below.
		2.g Change the value of a specific parameter
                        echo <value> > init_cp
		2.h For some Wh+ specific parameters, to apply the changes to hardware,
                    follow the commands as below:
			echo <roce_prio> > roce_prio
			echo <cnp_prio> > cnp_prio
			echo <roce_dscp> > roce_dscp
			echo <cnp_dscp> > cnp_dscp
			echo -n 0x01 > apply

		2.i Read back a specific parameter
			cat roce_dscp
			...

		2.j Read back all parameters
			cat apply
			cat ext_settings
		Note: "cat apply" is displaying the basic and advanced parameters set from HW
                      except DCN and rate limit tables.
		      "cat ext_settings" is displaying the extended parameter set from HW.

	3. Unmount per-port-configfs interface
	   ====================================
		3.a remove all per-port-configfs mounts as following:
			rmdir /sys/kernel/config/bnxt_re/bnxt_re1
			rmdir /sys/kernel/config/bnxt_re/bnxt_re0
			...

		Note: If configfs is mounted rmmod bnxt_re will fail.
		      It is must to perform step 3.a before issuing
		      rmmod bnxt_re.

SR-IOV and VF Resource Distribution
===================================
RDMA SR-IOV is supported on BCM575xx and BCM576xx devices only, with NPAR disabled.

Note: Before enabling the VFs, both bnxt_en and bnxt_re drivers should be loaded.
      Loading bnxt_re driver after creating VFs is not supported. Removal of bond
      interface while VFs present is also not supported, as removal of bond
      interface creates the RoCE base interfaces which is similar to loading
      bnxt_re driver.

      In distros that support auto loading of bnxt_re based on udev rules,
      (ie. having an entry  ENV{ID_NET_DRIVER}=="bnxt_en", RUN{builtin}+="kmod load bnxt_re"
      in udev rules file 90-rdma-hw-modules.rules)
      Note: The location of the file is distro specific.
            RHEL: /usr/lib/udev/rules.d/90-rdma-hw-modules.rules
            UBUNTU: /lib/udev/rules.d/90-rdma-hw-modules.rules
      If the bnxt_re driver is unloaded before creating VFs, vf creation loads bnxt_re
      driver. This operation throws error in dmesg as this is considered as loading
      driver after creating VFs. Disable RoCE on the adapter if RoCE feature is not
      required or disable this udev rule to prevent auto loading of the bnxt_re driver.

If SR-IOV is supported on the adapter, QPs, SRQs, CQs and MRs are distributed
across VF by the bnxt_re driver.

Driver allocates 64K of QPs, SRQs and CQs for the PF pool. It creates 256K MRs
for the PF pool.
For VFs, the driver is restricting the total number of resources as follows

Max QPs - 6144
Max MRs - 6144
Max CQs - 6144
Max SRQs - 4096

For eg: Active number of VFs can be obtained from the following command.
	$cat /sys/class/net/p6p1/device/sriov_numvfs

If sriov_numvfs is 2, half of the above values will be supported by each
VF.

Note: Since PF is in privileged mode, it is allowed to use the
entire PF pool resources. But VFs are restricted to create max configured
by the above calculation. User must ensure that total resources created by
PF and its VFs shall be less than Max configured (64K for QPs/SRQs/CQs and 256K for MRs).

Use following command to get the active resource count.
$cat /sys/kernel/debug/bnxt_re/<Device name>/info

Presence of active RoCE traffic on the VF undergoing Function Level Reset (FLR)
or on any other PFs/VFs impacts the function initialization time
of the VF undergoing FLR. Function initialization time scales linearly as
the cumulative active QP count across all PFs and VFs increases.
The increased function initialization time may lead to VF probe failures
and periodic HWRM timeouts when the cumulative active QP count is greater than 6K QPs.


Link Aggregation
================
Link aggregation is a common technique that is used to provide
additional aggregate bandwidth and high availability for logical
interfaces that aggregate multiple physical interfaces. Additional
aggregate bandwidth can be achieved by balancing the traffic load
across multiple physical interfaces. High availability can be achieved
by reconfiguring the loads across the active links when one of the
physical links fails.
The concepts of link aggregation can be applied to RoCE also.

The current solution allows a link aggregation only if all of the
following conditions are met:

-> The netdev associated with each RDMA interface is a
   part of an upper level device.
-> The two netdev interfaces part of  same bond device.
-> Two netdevs on the same physical device are added to the bond.
-> The link aggregate cannot span separate physical devices.
-> The bond interface has exactly two non-NPAR physical interfaces.
-> The bond mode is one of the following modes:
   round-robin (mode0), active-backup (mode 1), xor (mode 2),
   or 8023ad (mode 4).

Note: mode 0, 2 and 4 will be handled as active-active mode in HW.

When a LAG is created roce device interface is visible
with name bnxt_re_bond0.

Note: RoCE LAG is not supported on multi host or multi root configs.
Note: If VFs are created on any of the functions of the bond, RoCE Bond device
      will not be created. If RoCE bond is created before VF creation, RoCE bond
      will continue to work on the PFs. But VF RoCE devices will not be supported.
Note: If the adapter has more than 2 RoCE enabled functions (4 port adapter, etc.),
      RoCE bond device will not be created.
      There should be exactly two RoCE devices from an adapter when bond is
      created. If L2 bond is enabled on this adapter, RoCE doesn't work on
      the bnxt_re devices created for the physical interfaces.
Note: RoCE Bond is created only if there are two ethernet functions added
      to the bond and the ethernet devices are from the same physical
       adapter. Multiple adapters are not supported.
Note: When LAG is enabled, driver creates all QPs on PF0 and firmware
      does the load balancing between the 2 LAG ports. In the current
      algorithm, firmware will do load balancing on a per DPI (application)
      basis. If we have 100 applications creating 1 QP each then all the
      QPs will get created on the same port. Similarly if we have 100
      applications each creating odd number of QPs then the QP count
      difference between the ports can be up to 100. Only when all the
      applications are creating even number of QPs does the firmware
      guarantee that the difference in QP count between both ports
      is <= 2.
Note: On BCM9574xx devices to enable entropy for RoCE V2 UDP source port
      firmware limits the number of GIDs available to 8 across all PFs on
      Performance NIC and to 128 on Smart NIC. If host tries to create more
      GID entries than these limits then firmware will fail the GID add
      command and as a result QP data traffic will fail.
Note: RoCE LAG solution involves a HW pipeline configuration that enables
      RoCE traffic to be directed to the right port using an internal GID
      to port mapping logic. However, the HW transmit queues and ring
      shapers used for RoCE traffic are associated only with port 0.
      The GID to port mapping enables re-direction to the correct port as
      port status changes.

      To enable transmit endpoint shaping with RoCE LAG, even for an
      active-backup mode, the transmit endpoint shapers associated with
      port 0 always need to be enabled.

      The TX traffic out of port 0 would be 40Gpbs when port 0 is active.
      And when port 1 became active, the TX traffic out also would be 40Gpbs.
      This is because the shapers are associated with port 0 in active-backup
      mode. Please note in the example above if port_idx was set to 1
      in active-backup mode, the setting for port 1 will be set but not used.

      Another example, in active-backup mode when port 0 goes down, port 1 becomes
      active, transmit per COS statistics will not reflect the current active
      port stats. RoCE statistics available from debugfs interface are updated
      accurately and can be used.
Note: When the L2 bond is created and the RoCE LAG is not created by the driver
      due to the RoCE LAG not supported in the device, error messages are seen
      in the dmesg for GID add/delete.
Note: GID add/remove failures are expected while a roce bond is being created/destroyed.

=> Instructions to create/Destroy RoCE LAG

   - Load bnxt_en and bnxt_re driver
   - Follow the distro specific commands to create L2 bond. RoCE bond will be
     created in the background
   - ibv_devices shows bnxt_re_bond0 device once the L2 bond is created.

Note: If stable name is set by udev rule, the RoCE bond device name will point to the
device name of the first child device of the bond.

Known Issues with Link aggregation:

-> bnxt_re and bnxt_en drivers need to be loaded before creating bond interface.
-> Changing bond mode when RoCE driver is in use can cause system hang.
   E.g. changing the bonding mode while running a user application,
   can cause a system hang.
   Please make sure that no reference to bnxt_re is taken while changing the bond mode.
   Use the following command to check the module usage count
	#lsmod|grep bnxt_re
   For proper removal of bnxt_re devices or update the bond state:
   1. Unmount all active NFS RDMA mounts.
   2. Stop the ibacm service (or any similar service) on systems where OFED is
      installed using the command:
	# service ibacm stop
   3. Stop all user space RoCE applications.

-> User has to delete the configfs entry created for the bond device before
   a slave is removed from the bond. Without that, user would see error messages
   on the terminal and may cause a hang.
-> RoCE vfs are supported in a bond mode only if HW LAG is enabled.
-> Bond device must be created first before enabling VFs on the slave PFs.
-> If the bond is re-configured after VFs creation, then VFs need to be recreated
   for the proper functioning of RoCE VFs.
-> Create / destroy bond in a loop:
   Make sure that enough delay is provided (i.e. 5-10 sec) after create and destroy
   of the bond. This is to avoid hang and call traces related to the rtnl_lock usage.
-> When there is a link toggle, bnxt_re driver communicates that to the fw to switch over.
   If there are parallel outstanding FW cmds, it can take time for the fail over command
   to reach the FW. The QP timeout value should be high enough to accommodate this.
   It is recommended to use a timeout value 19.

- If the error recovery process fails for some reasons when the LAG is created,
  any subsequent administrative operations like de-slaving interfaces, unloading
  the bonding driver and bringing up base interfaces would cause unexpected
  behavior (can be a system crash).

BNXT_RE Driver Statistics
=======================

The bnxt_re driver supports debugFS which allows statistics and debug parameters be accessed.
To access this information, read the /sys/kernel/debug/bnxt_re/bnxt_re<x>/info file. Each port will be
listed with associated state. The available statistics will vary based on hardware capability, eg:

# cat /sys/kernel/debug/bnxt_re/bnxt_re0/info

bnxt_re debug info:
=====[ IBDEV bnxt_re0 ]=============================
	link state: UP
	Max QP: 0xff7f
	Max SRQ: 0xffff
	Max CQ: 0xffff
	Max MR: 0x10000
	Max MW: 0x10000
	Active QP: 0x2
	Active SRQ: 0x0
	Active CQ: 0x21
	Active MR: 0x4
	Active MW: 0x0
...


Field Explanation:

Device resource limits:
Max QP 		Max number of QP limit
Max SRQ		Max number of SRQ limit
Max CQ		Max number of CQs limit
Max MR		Max number of memory region limit
Max MW		Max number of memory window limit

Active Resources:
Active QP		Number of active QPs
Active SRQ		Number of active SRQs
Active CQ		Number of active CQs
Active MR		Number of active Memory Regions
Active DMABUF MR 	Number of active DMABUF Memory Regions
Active PEERMEM MR       Number of active PEERMEM Memory Regions
Active MW		Number of active Memory Windows
Active RC QP		Number of active RC QPs
Active UD QP		Number of active UD QPs

Note: HW uses the same resource pages for MR and MW.
 So the total number of  Active MR and Active MW should
 be less than or equal to Max MR/MW.

Note: For doorbell recovery, the driver uses a software-only CQ that
doesn't correspond to any hardware component. This means that the
driver CQ count will not match the actual hardware CQ count.

Resource Watermarks:
QP Watermark		Max QPs active after driver load
SRQ Watermark   	Max SRQs active after driver load
CQ Watermark    	Max CQs active after driver load
MR Watermark    	Max MRs active after driver load
DMABUF MR Watermark 	Max DMABUF MRs active after driver load
PEERMEM MR Watermark    Max PEERMEM MRs active after driver load
MW Watermark    	Max MWs active after driver load
AH Watermark    	Max AHs active after driver load
PD Watermark   	 	Max PDs active after driver load
RC QP Watermark		Max RC QPs active after driver load
UD QP Watermark		Max UD QPs active after driver load

Byte and Packet Counters:
Rx Pkts	 	Number of RoCE packets received
Rx Bytes	Number of RoCE bytes received
Tx Pkts		Number of RoCE packets transmitted
Tx Bytes	Number of RoCE bytes transmitted

Congestion Notification Counters:

Note: CNP counters are per port counter for Gen P5 and P7 adapters.
PF counters increments for VF CNP packets also.

CNP Tx Pkts	Number of RoCE CNP packets received
CNP Tx Bytes	Number of RoCE CNP bytes received
CNP Rx Pkts	Number of RoCE CNP packets transmitted
CNP Rx Bytes	Number of RoCE CNP bytes transmitted

RDMA operation Counters:
tx_atomic_req	Number of atomic requests transmitted
rx_atomic_requests	Number of atomic requests received
tx_read_req	Number of read requests transmitted
tx_read_resp	Number of read responses transmitted
rx_read_requests	Number of read requests received
rx_read_resp	Number of read responses received
tx_write_req	Number of write requests transmitted
rx_write_requests	Number of write request received
tx_send_req	Number of send requests transmitted
rx_send_req	Number of send requests received

Driver Debug counters:
Resize CQ count		 Debug counter for CQ resize ops after driver load
num_irq_started		 Debug counter for IRQs started after device creation
num_irq_stopped		 Debug counter for IRQs stopped after device creation
poll_in_intr_en  	 Debug counter for indicating control path polling when
			 interrupt enabled
poll_in_intr_dis 	 Debug counter for indicating control path polling when
			 interrupt are disabled
cmdq_full_dbg_cnt 	 Debug counter to indicate control path CMDQ full
fw_service_prof_type_sup Debug info to indicate the current service profile config
dbq_int_recv		 Debug counter to indicate the DBQ interrupt received
dbq_int_en		 Debug counter to indicate the number of iterations dbq
                         interrupt is enabled
dbq_pacing_resched	 Debug counter to indicate the number of times pacing thread
			 rescheduled
dbq_pacing_complete	 Debug counter to indicate the count where the pacing thread
			 completed
dbq_pacing_alerts	 Debug counter to indicate the number of times userlibs alerted
			 the driver for onset congestion
dbq_dbr_fifo_reg	 Debug counter to monitor the HW FIFO reg
dbr_drop_recov_epoch		Debug counter to indicate epoch of latest DBR drop event
dbr_drop_recov_events		Debug counter to indicate the number of DBR drop events
dbr_drop_recov_timeouts		Debug counter to indicate the DBR drop events scheduled to the
				user space and failed to complete within the timeout.
dbr_drop_recov_timeout_users	Debug counter to indicate the number of user instances that
				experienced timeout when driver finishes the recovery thread.
dbr_drop_recov_event_skips	Debug counter to indicate the number of DBR drop events ignored
				(skipped) by the driver because of one or more outstanding event.
latency_slab		Each slab is of 1 second granularity. The Counters of each slab represent
			the total number of rcfw commands completed in that range.
			Upto 128 seconds latency is tracked.
rx_dcn_payload_cut	Number of received DCN payload cut packets.
te_bypassed		Number of transmitted packets that bypassed the transmit engine.
tx_dcn_cnp		Number of transmitted DCN CNP packets.
rx_dcn_cnp		Number of received DCN CNP packets.
rx_payload_cut		Number of received DCN payload cut packets.
rx_payload_cut_ignored	Number of received DCN payload cut packets that are ignored
			because they failed the PSN checks.
rx_dcn_cnp_ignored	Number of received DCN CNP packets that are ignored either
			because the ECN is not enabled on the QP or the ECN is enabled
			but the CNP packets do not pass the packet validation checks.

Recoverable Errors:
Recoverable Errors	Number of recoverable errors detected.  Recoverable errors are
	    		detected by the HW.  HW instructs FW to initiate the recovery
			process.  RC connection does not teardown as a result of these errors.
local_ack_timeout_err	Number of retransmission requests
rnr_nak_retry_err	Number of RNR (Receiver-Not-Ready) NAKs received.
duplicate_requef duplicated requests detected.
implied_nak_seq_err	Number of responses missing
packet_seq_err		Number of PSN sequencing error NAKs received
res_oob_drop_count	Number of packets dropped because of no host buffers
out_of_sequence		Number of  out of sequence packets received
rx_roce_discard_pkts	Number of discard packets received
rx_roce_error_pkts	Number of error packets received


Fatal Errors:
max_retry_exceeded	Number of retransmission requests exceeded the max
unrecoverable_err	Number of unrecoverable errors detected
bad_resp_err		Number of bad response errors detected
local_qp_op_err		Number of QP local operation errors detected
local_protection_err	Number of local protection errors detected
mem_mgmt_op_err		Number of times HW detected an error because of illegal bind/fast
			register/invalidate attempted by the driver
req_remote_invalid_requesNumber of invalid request received from the remote rdma initiator.
req_remote_access_error	Number of times H/W received a REMOTE ACCESS ERROR NAK from the peer.
remote_op_err		Number of times HW received a REMOTE OPERATIONAL ERROR NAK from the peer.
req_cqe_error		The number of times the requester detected CQEs completed with error

Responder errors:
res_exceed_max		Number of times HW detected incoming Send, RDMA write or RDMA read
			messages which exceed the maximum transfer length.
resp_local_length_error	Number of times HW detected incoming RDMA write message payload
			size does not match write length in the RETH.
res_exceeds_wqe		Number of times HW detected Send payload exceeds RQ/SRQ RQE buffer capacity.
res_opcode_err		Number of times HW detected First, Only, Middle, Last packets for
			incoming requests are improperly ordered with respect to the previous packet.
res_rx_invalid_rkey	Number of times HW detected a incoming request with an R_KEY that
			did not reference a valid MR/MW.
res_rx_domain_err	Number of times HW detected a incoming request with an R_KEY that
			referenced a MR/MW that was not in the same PD as the QP on which the
			request arrived.
res_rx_no_perm		Number of times HW detected a incoming RDMA write request with an
			R_KEY that referenced a MR/MW which did not have the access permission
			needed for the operation.
res_rx_range_err	Number of times HW detected an incoming RDMA write request that had
			a combination of R_KEY, VA and length that was out of bounds of the
			associated MR/MW.
res_tx_invalid_rkey	Number of times HW detected a R_KEY that did not reference a valid
			MR/MW while processing incoming read request.
res_tx_domain_err	Number of times HW detected a incoming request with an R_KEY that
			referenced a MR/MW that was not in the same PD as the QP on which
			the RDMA read request is received.
res_tx_no_perm		Number of times HW detected a incoming RDMA read request with an R_KEY
			that referenced a MR/MW which did not have the access permission needed
			for the operation.
res_tx_range_err	Number of times HW detected an incoming RDMA read request that had a
			combination of R_KEY, VA and length that was out of bounds of the associated MR/MW.
res_irrq_oflow		Number of times HW detected that peer sent us more RDMA read or atomic
			requests that the negotiated maximum
res_unsup_opcode	Number of times HW detected that peer sent us a request with an opcode
			for a request type that is not supported on this QP.
res_unaligned_atomic	Number of times HW detected that VA of an atomic request is on a memory
			boundary that prevents atomic execution.
res_rem_inv_err		Number of times HW detected a incoming send with invalidate request in
			which the R_KEY to invalidate did not MR/MW which could be invalidated.
res_mem_error64		Number of times HW detected a RQ/SRQ SGE which points to an inaccessible memory.
res_srq_err		Number of times HW detected a QP moving to error state because the associated
			SRQ is in error.
res_cmp_err		Number of time HW detected that there is no CQE space available on CQ or
			CQ is not in valid state.
res_invalid_dup_rkey	Number of times HW detected invalid R_KEY while re-sending responses to
			duplicate read requests.
res_wqe_format_err	Number of times HW detected error in the format of the WQE in the RQ/SRQ.
res_cq_load_err		Number of times HW detected error while attempting to load the CQ context.
res_srq_load_err	Number of times HW detected error while attempting to load the SRQ context.
resp_cqe_error		Number of times the responder detected CQEs completed with errors.
resp_remote_access_errors	Number of times the responder detected remote access errors.


Note: When a LAG is created, all the statistics are reported on function 0 of the device.

Driver provides additional debug information useful for dev debugging.
This can be read from /sys/kernel/debug/bnxt_re/bnxt_re<x>/drv_dbg_stats.

BNXT_RE SLOW PATH PERF STATS
============================

The bnxt_re driver supports debugFS which accesses slow path perf statistics.
To access this information, read the /sys/kernel/debug/bnxt_re/bnxt_re<x>/sp_perf_stats file.


# cat /sys/kernel/debug/bnxt_re/bnxt_re0/sp_perf_stats

bnxt_re perf stats: Enabled shadow qd 64 Driver Version - 216.0.54.0
        latency_slab [0 - 1] msec = 267
        latency_slab [1 - 2] msec = 845
<qp_create> 1 <qp_destroy> 5 <mr_create> 1 <mr_destroy> 8 <qp_modify_to_err> 1
Total qp_create 1 in msec 1
Total qp_destroy 0 in msec 0
Total mr_create 0 in msec 0
Total mr_destroy 0 in msec 0
Total qp_modify_err_total 0 in msec 0

Field Explanation:

Latency slab:
Each entry in the slab is a histogram representing latency_slab in msec granularity.
For eg:
In case of below entries, it indicates total 267 HWRM command completed
within a max 1 msec latency and 845 commands completed within a max 2 msec
but all the latency of those 845 commands are more than 1 msec.
   latency_slab [0 - 1] msec = 267
   latency_slab [1 - 2] msec = 845

Latency for operations:
The latency for operations are tracked individually and are stored in the driver's internal
tracking array. Up to 256K entries are tracked.
For eg:
<qp_create> 1 <qp_destroy> 5 <mr_create> 1 <mr_destroy> 8 <qp_modify> 1
Here the highest latency was observed for the "memory region destroyed".

Accumulated latency:
The driver prints the tracking instances and accumulated time taken to complete those in msec.
The values here represent the total entries which were tracked and not the total entries
overall. Up to 256K entries are tracked. The total number of qp_create will be much larger
than this number.

UDCC RTT configuration
======================
The bnxt_re driver has below debugfs entry to read/configure RTT bucket values

Example:

To know current RTT bucket values and max no. of buckets:
	cat /sys/kernel/debug/bnxt_re/<PCI ID>/udcc/config

To configure new RTT bucket values:
	echo "RTT_CFG,10000,20000,90000,4294967295" > /sys/kernel/debug/bnxt_re/<PCI ID>/udcc/config

Note:
- RTT_CFG is the keyword and comma separated unsigned int values are provided for each bucket.
- User must configure each bucket as prescribed below
	bucket 0 < bucket 1 < bucket 2 < bucket 3
- User must configure all the buckets

To know RTT counts per session:
	cat /sys/kernel/debug/bnxt_re/<PCI ID>/udcc/<session>/session_query

To reset RTT counts per session:
	echo "RESET_RTT" > /sys/kernel/debug/bnxt_re/<PCI ID>/udcc/<session>/session_query


Sysfs counters
==============
The bnxt_re driver supports the sysfs counters as mentioned in the link
https://www.kernel.org/doc/Documentation/ABI/stable/sysfs-class-infiniband

These counters can be accessed by reading the corresponding file under the following path:
/sys/class/infiniband/bnxt_re<x>/ports/1/counters/<counter_name>

Example:
# cat /sys/class/infiniband/bnxt_re5/ports/1/counters/port_rcv_data
3003699129

Limitation:
Please note that the counters listed above are limited to 16bit or 8bit, as detailed below:
 port_rcv_errors - 16bits
 port_xmit_discards - 16bits
 port_xmit_constraint_errors - 8bits
 port_rcv_constraint_errors - 8bits

As a result of these bit-width limitations, the corresponding sysFS statistics for these counters
may not match the values reported by debugFS once the counters overflow their
respective 16-bit or 8-bit limits.

The following counters have not been mapped and are currently reported with a value of 0
 link_error_recvery
 local_link_integrity_errors
 multicast_rcv_packets
 multicast_xmit_packets
 port_rcv_remote_physical_errors
 port_rcv_switch_relay_errors
 port_xmit_wait
 VL15_dropped
 req_cqe_flush_error
 resp_cqe_flush_error
 roce_adp_retrans
 roce_adp_retrans_to
 roce_slow_restart
 roce_slow_restart_cnps
 roce_slow_restart_trans
 rp_cnp_ignored
 rx_icrc_encapsulated
