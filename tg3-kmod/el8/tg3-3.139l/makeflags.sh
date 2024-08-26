#!/bin/sh
# Copyright (C) 2008-2019 Broadcom.  All Rights Reserved.
# The term "Broadcom" refers to Broadcom Limited and/or its subsidiaries.

if [ $# -lt 1 ]; then
	echo "$0: No kernel source directory provided." 1>&2
	exit 255
fi

srcdir=$1
shift

while [ $# != 0 ]; do
	case $1 in
		TG3_NO_EEE)
			echo "#define TG3_DISABLE_EEE_SUPPORT"
			;;
		*)
			;;
	esac
	shift
done

UAPI=
if [ -d $srcdir/include/uapi/linux ]
then
	UAPI=uapi
fi

if grep -q "netdump_mode" $srcdir/include/linux/kernel.h ; then
	echo "#define BCM_HAS_NETDUMP_MODE"
fi

if grep -q "bool" $srcdir/include/linux/types.h ; then 
	echo "#define BCM_HAS_BOOL"
fi


if [ -f $srcdir/include/$UAPI/linux/types.h ]; then
	if grep -q "__le32" $srcdir/include/$UAPI/linux/types.h; then
		echo "#define BCM_HAS_LE32"
	fi
elif [ -f $srcdir/include/linux/types.h ]; then
	if grep -q "__le32" $srcdir/include/linux/types.h; then
		echo "#define BCM_HAS_LE32"
	fi
fi

if grep -q "resource_size_t" $srcdir/include/linux/types.h ; then
	echo "#define BCM_HAS_RESOURCE_SIZE_T"
fi

if grep -q "kzalloc" $srcdir/include/linux/slab.h ; then
	echo "#define BCM_HAS_KZALLOC"
fi

for symbol in jiffies_to_usecs usecs_to_jiffies msecs_to_jiffies; do
	if [ -f $srcdir/include/linux/jiffies.h ]; then
		if grep -q "$symbol" $srcdir/include/linux/jiffies.h ; then
			echo "#define BCM_HAS_`echo $symbol | tr '[a-z]' '[A-Z]'`"
			continue
		fi
	fi
	if [ -f $srcdir/include/linux/time.h ]; then
		if grep -q "$symbol" $srcdir/include/linux/time.h ; then
			echo "#define BCM_HAS_`echo $symbol | tr '[a-z]' '[A-Z]'`"
			continue
		fi
	fi
	if [ -f $srcdir/include/linux/delay.h ]; then
		if grep -q "$symbol" $srcdir/include/linux/delay.h ; then
			echo "#define BCM_HAS_`echo $symbol | tr '[a-z]' '[A-Z]'`"
			continue
		fi
	fi
done

if grep -q "msleep" $srcdir/include/linux/delay.h ; then
	echo "#define BCM_HAS_MSLEEP"
fi

if grep -q "msleep_interruptible" $srcdir/include/linux/delay.h ; then
	echo "#define BCM_HAS_MSLEEP_INTERRUPTIBLE"
fi

if grep -q "skb_copy_from_linear_data" $srcdir/include/linux/skbuff.h ; then
	echo "#define BCM_HAS_SKB_COPY_FROM_LINEAR_DATA"
fi

if grep -q "skb_is_gso_v6" $srcdir/include/linux/skbuff.h ; then
	echo "#define BCM_HAS_SKB_IS_GSO_V6"
fi

if grep -q "skb_checksum_none_assert" $srcdir/include/linux/skbuff.h ; then
	echo "#define BCM_HAS_SKB_CHECKSUM_NONE_ASSERT"
fi

if grep -q "skb_shared_hwtstamps" $srcdir/include/linux/skbuff.h ; then
	echo "#define BCM_KERNEL_SUPPORTS_TIMESTAMPING"
fi

if grep -q "union skb_shared_tx" $srcdir/include/linux/skbuff.h ; then
	echo "#define BCM_HAS_SKB_SHARED_TX_UNION"
fi

if grep -q "skb_tx_timestamp" $srcdir/include/linux/skbuff.h ; then
	echo "#define BCM_HAS_SKB_TX_TIMESTAMP"
fi

if grep -q "skb_frag_size" $srcdir/include/linux/skbuff.h ; then
	echo "#define BCM_HAS_SKB_FRAG_SIZE"
fi

if grep -q "skb_frag_dma_map" $srcdir/include/linux/skbuff.h ; then
	echo "#define BCM_HAS_SKB_FRAG_DMA_MAP"
fi

if grep -q "build_skb.*frag_size" $srcdir/include/linux/skbuff.h ; then
	echo "#define BCM_HAS_BUILD_SKB"
fi

if grep -q "pci_pcie_cap" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_PCIE_CAP"
fi

if grep -q "pcie_capability_set_word" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCIE_CAP_RW"
fi

if grep -q "pci_is_pcie" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_IS_PCIE"
fi

if grep -q "pci_ioremap_bar" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_IOREMAP_BAR"
fi

if grep -q "pci_read_vpd" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_READ_VPD"
fi

if grep -q "PCI_DEV_FLAGS_MSI_INTX_DISABLE_BUG" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_INTX_MSI_WORKAROUND"
fi

if grep -q "pci_target_state" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_TARGET_STATE"
fi

if grep -q "pci_choose_state" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_CHOOSE_STATE"
fi

if grep -q "pci_pme_capable" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_PME_CAPABLE"
fi

if grep -q "pci_enable_wake" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_ENABLE_WAKE"
fi

if grep -q "pci_wake_from_d3" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_WAKE_FROM_D3"
fi

if grep -q "pci_set_power_state" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_SET_POWER_STATE"
fi

if grep -q "err_handler" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_EEH_SUPPORT"
fi

if grep -q "busn_res" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_BUSN_RES"
fi

if grep -q "pci_is_enabled" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_IS_ENABLED"
fi

if grep -q "pci_device_is_present" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCI_DEV_IS_PRESENT"
fi

if [ -e "$srcdir/include/linux/pm_wakeup.h" ]; then
	TGT_H="$srcdir/include/linux/pm_wakeup.h"
elif [ -e "$srcdir/include/linux/pm.h" ]; then
	TGT_H="$srcdir/include/linux/pm.h"
fi

if [ -n "$TGT_H" ]; then
	if grep -q "device_can_wakeup"        $TGT_H && \
	   grep -q "device_may_wakeup"        $TGT_H && \
	   grep -q "device_set_wakeup_enable" $TGT_H ; then
		echo "#define BCM_HAS_DEVICE_WAKEUP_API"
	fi
	if grep -q "device_set_wakeup_capable" $TGT_H ; then
		echo "#define BCM_HAS_DEVICE_SET_WAKEUP_CAPABLE"
	fi
fi

if [ -f $srcdir/include/linux/pci-dma-compat.h ]; then
	TGT_H=$srcdir/include/linux/pci-dma-compat.h
elif [ -f $srcdir/include/asm-generic/pci-dma-compat.h ]; then
	TGT_H=$srcdir/include/asm-generic/pci-dma-compat.h
fi

if [ -n "TGT_H" ]; then
	num_args=`awk '/pci_dma_mapping_error/,/[;{]/ {printf $0; next}' $TGT_H | awk -F ',' '{print NF}'`
	if [ $num_args -eq 2 ]; then
		echo "#define BCM_HAS_NEW_PCI_DMA_MAPPING_ERROR"
	elif grep -q "pci_dma_mapping_error" $TGT_H ; then
		echo "#define BCM_HAS_PCI_DMA_MAPPING_ERROR"
	fi
fi

if grep -q "pcie_get_readrq" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCIE_GET_READRQ"
fi

if grep -q "pcie_set_readrq" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_PCIE_SET_READRQ"
fi

if grep -q "print_mac" $srcdir/include/linux/if_ether.h ; then
	echo "#define BCM_HAS_PRINT_MAC"
fi

# ethtool_op_set_tx_ipv6_csum() first appears in linux-2.6.23
if grep -q "ethtool_op_set_tx_ipv6_csum" $srcdir/include/linux/ethtool.h ; then
	echo "#define BCM_HAS_ETHTOOL_OP_SET_TX_IPV6_CSUM"
fi

# ethtool_op_set_tx_hw_csum() first appears in linux-2.6.12
if grep -q "ethtool_op_set_tx_hw_csum" $srcdir/include/linux/ethtool.h ; then
	echo "#define BCM_HAS_ETHTOOL_OP_SET_TX_HW_CSUM"
fi

if grep -q "ethtool_op_set_sg" $srcdir/include/linux/ethtool.h ; then
	echo "#define BCM_HAS_ETHTOOL_OP_SET_SG"
fi

if grep -q "ethtool_op_set_tso" $srcdir/include/linux/ethtool.h ; then
	echo "#define BCM_HAS_ETHTOOL_OP_SET_TSO"
fi

if grep -q "eth_tp_mdix" $srcdir/include/$UAPI/linux/ethtool.h ; then
	echo "#define BCM_HAS_MDIX_STATUS"
fi

if grep -q "(*set_phys_id)" $srcdir/include/linux/ethtool.h ; then
	echo "#define BCM_HAS_SET_PHYS_ID"
fi

# set_tx_csum first appears in linux-2.4.23
if grep -q "(*set_tx_csum)" $srcdir/include/linux/ethtool.h ; then
	echo "#define BCM_HAS_SET_TX_CSUM"
fi

# include/uapi/linux/ethtool.h first appeared in linux-3.7
if [ -f $srcdir/include/$UAPI/linux/ethtool.h ]; then
	if grep -q "ethtool_cmd_speed_set" $srcdir/include/$UAPI/linux/ethtool.h ; then
		echo "#define BCM_HAS_ETHTOOL_CMD_SPEED_SET"
	fi

	if grep -q "ethtool_cmd_speed(" $srcdir/include/$UAPI/linux/ethtool.h ; then
		echo "#define BCM_HAS_ETHTOOL_CMD_SPEED"
	fi
elif [ -f $srcdir/include/linux/ethtool.h ]; then
	if grep -q "ethtool_cmd_speed_set" $srcdir/include/linux/ethtool.h ; then
		echo "#define BCM_HAS_ETHTOOL_CMD_SPEED_SET"
	fi

	if grep -q "ethtool_cmd_speed(" $srcdir/include/linux/ethtool.h ; then
		echo "#define BCM_HAS_ETHTOOL_CMD_SPEED"
	fi
fi

if grep -q "ETH_TEST_FL_EXTERNAL_LB_DONE" $srcdir/include/linux/ethtool.h ; then
	echo "#define BCM_HAS_EXTERNAL_LB_DONE"
fi

RXNFC=`sed -ne "/get_rxnfc).*$/{ N; s/\n/d/; P }" $srcdir/include/linux/ethtool.h`
if [ ! -z "$RXNFC" ]; then
	if `echo $RXNFC | grep -q "void"` ; then
		echo "#define BCM_HAS_OLD_GET_RXNFC_SIG"
	fi 
	echo "#define BCM_HAS_GET_RXNFC"
fi

if grep -q "get_rxfh_indir" $srcdir/include/linux/ethtool.h ; then
	if grep -q "ethtool_rxfh_indir_default" $srcdir/include/linux/ethtool.h ; then
		echo "#define BCM_HAS_ETHTOOL_RXFH_INDIR_DEFAULT"
	fi
	if grep -q "get_rxfh_indir_size" $srcdir/include/linux/ethtool.h ; then
		echo "#define BCM_HAS_GET_RXFH_INDIR_SIZE"
	fi
	echo "#define BCM_HAS_GET_RXFH_INDIR"
fi

if grep -q "lp_advertising" $srcdir/include/$UAPI/linux/ethtool.h ; then
	echo "#define BCM_HAS_LP_ADVERTISING"
fi

if grep "supported_ring_params" $srcdir/include/linux/ethtool.h | grep u32 | grep -q -v "UEK_KABI_USE" ; then
        echo "#define BCM_HAS_RINGPARAMS"
fi

if grep -q "kernel_ethtool_ringparam" $srcdir/include/linux/ethtool.h ; then
        echo "#define BCM_HAS_ETHTOOL_RINGPARAMS_STRUCTURE"
fi

if grep -q "get_link_ksettings" $srcdir/include/linux/ethtool.h ; then
	echo "#define BCM_HAS_ETHTOOL_LINK_KSETTINGS"
fi

if grep -q "skb_transport_offset" $srcdir/include/linux/skbuff.h ; then
	echo "#define BCM_HAS_SKB_TRANSPORT_OFFSET"
fi

if grep -q "skb_get_queue_mapping" $srcdir/include/linux/skbuff.h ; then
	echo "#define BCM_HAS_SKB_GET_QUEUE_MAPPING"
fi

if grep -q "ip_hdr" $srcdir/include/linux/ip.h ; then
	echo "#define BCM_HAS_IP_HDR"
fi

if grep -q "ip_hdrlen" $srcdir/include/net/ip.h ; then
	echo "#define BCM_HAS_IP_HDRLEN"
fi

if grep -q "tcp_hdr" $srcdir/include/linux/tcp.h ; then
	echo "#define BCM_HAS_TCP_HDR"
fi

if grep -q "tcp_hdrlen" $srcdir/include/linux/tcp.h ; then
	echo "#define BCM_HAS_TCP_HDRLEN"
fi

if grep -q "tcp_optlen" $srcdir/include/linux/tcp.h ; then
	echo "#define BCM_HAS_TCP_OPTLEN"
fi

TGT_H=$srcdir/include/linux/netdevice.h
if grep -q "netdev_err" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_ERR"
fi

if grep -q "netdev_warn" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_WARN"
fi

if grep -q "netdev_notice" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_NOTICE"
fi

if grep -q "netdev_info" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_INFO"
fi

if grep -q "struct netdev_queue" $TGT_H ; then
	echo "#define BCM_HAS_STRUCT_NETDEV_QUEUE"
else
	num_args=`awk '/ netif_rx_complete\(struct/,/\)/ {printf $0; next}' $TGT_H | awk -F ',' '{print NF}'`
	if [ -n "$num_args" -a $num_args -eq 2 ]; then
		# Define covers netif_rx_complete, netif_rx_schedule,
		# __netif_rx_schedule, and netif_rx_schedule_prep
		echo "#define BCM_HAS_NEW_NETIF_INTERFACE"
	fi
fi

if grep -q "netif_set_real_num_tx_queues" $TGT_H ; then
	echo "#define BCM_HAS_NETIF_SET_REAL_NUM_TX_QUEUES"
fi

if grep -q "netif_set_real_num_rx_queues" $TGT_H ; then
	echo "#define BCM_HAS_NETIF_SET_REAL_NUM_RX_QUEUES"
fi

if grep -q "netdev_priv" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_PRIV"
fi

if grep -q "netdev_tx_t" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_TX_T"
fi

if grep -q "netdev_hw_addr" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_HW_ADDR"
fi

if grep -q "netdev_name" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_NAME"
fi

if grep -q "netdev_sent_queue" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_SENT_QUEUE"
fi

if grep -q "netdev_tx_sent_queue" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_TX_SENT_QUEUE"
fi

if grep -q "netdev_completed_queue" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_COMPLETED_QUEUE"
fi

if grep -q "netdev_tx_completed_queue" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_TX_COMPLETED_QUEUE"
fi

if grep -q "netdev_reset_queue" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_RESET_QUEUE"
fi

if grep -q "netdev_tx_reset_queue" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_TX_RESET_QUEUE"
fi

if grep -q "struct net_device_ops" $TGT_H ; then
	echo "#define BCM_HAS_NET_DEVICE_OPS"
fi

if grep -q "(*ndo_get_stats64)" $TGT_H ; then
	echo "#define BCM_HAS_GET_STATS64"
	TGT_H=$srcdir/include/linux/netdevice.h
	num_args=`awk '/ndo_get_stats64\)\(struct/,/\)/ {printf $0; exit}' $TGT_H | grep void | wc -l`
	if [ $num_args -eq 1 ]; then
		echo "#define BCM_HAS_STATS64_WITH_VOID"
	fi
fi

if grep -q "(*ndo_set_multicast_list)" $TGT_H ; then
	echo "#define BCM_HAS_SET_MULTICAST_LIST"
fi

if grep -q "(*ndo_fix_features)" $TGT_H ; then
	echo "#define BCM_HAS_FIX_FEATURES"
fi

if grep -q "hw_features" $TGT_H ; then
	echo "#define BCM_HAS_HW_FEATURES"
fi

if grep -q "vlan_features" $TGT_H ; then
	echo "#define BCM_HAS_VLAN_FEATURES"
fi

if grep -q "netdev_update_features" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_UPDATE_FEATURES"
fi

if grep "skb_checksum_help" $TGT_H | grep -q "inward" ; then
	echo "#define BCM_HAS_SKB_CHECKSUM_HELP_OLD"
fi

if grep -q "alloc_etherdev_mq" $srcdir/include/linux/etherdevice.h ; then
	echo "#define BCM_HAS_ALLOC_ETHERDEV_MQ"
fi

if grep -q "napi_gro_receive" $TGT_H ; then
	echo "#define BCM_HAS_NAPI_GRO_RECEIVE"
fi

if grep -q "netif_tx_lock" $TGT_H ; then
	echo "#define BCM_HAS_NETIF_TX_LOCK"
fi

if grep -q "txq_trans_update" $TGT_H ; then
	echo "#define BCM_HAS_TXQ_TRANS_UPDATE"
fi

if grep -q "netif_trans_update" $TGT_H ; then
	echo "#define BCM_HAS_NETIF_TRANS_UPDATE"
fi

if grep -q "netdev_features_t" $TGT_H ; then
	echo "#define BCM_HAS_NETDEV_FEATURES_T"
fi

if grep -q "netif_get_num_default_rss_queues" $TGT_H ; then
	echo "#define BCM_HAS_GET_NUM_DFLT_RSS_QS"
fi

if grep -q "vlan_gro_receive" $srcdir/include/linux/if_vlan.h ; then
	echo "#define BCM_HAS_VLAN_GRO_RECEIVE"
elif grep -q "__vlan_hwaccel_put_tag" $srcdir/include/linux/if_vlan.h &&
     ! grep -q "vlan_hwaccel_receive_skb" $srcdir/include/linux/if_vlan.h ; then
	echo "#define BCM_HAS_NEW_VLAN_INTERFACE"
fi

if grep -q "vlan_group_set_device" $srcdir/include/linux/if_vlan.h ; then
	echo "#define BCM_HAS_VLAN_GROUP_SET_DEVICE"
fi

if [ -f $srcdir/include/linux/device.h ]; then
	if grep -q "dev_driver_string" $srcdir/include/linux/device.h ; then
		echo "#define BCM_HAS_DEV_DRIVER_STRING"
	fi

	if grep -q "dev_name" $srcdir/include/linux/device.h ; then
		echo "#define BCM_HAS_DEV_NAME"
	fi
fi

if [ -f $srcdir/include/linux/mdio.h ]; then
	echo "#define BCM_HAS_MDIO_H"
fi

if [ -f $srcdir/include/linux/mii.h ]; then
	if grep -q "mii_resolve_flowctrl_fdx" $srcdir/include/linux/mii.h ; then
		echo "#define BCM_HAS_MII_RESOLVE_FLOWCTRL_FDX"
	fi
	if grep -q "mii_advertise_flowctrl" $srcdir/include/linux/mii.h ; then
		echo "#define BCM_HAS_MII_ADVERTISE_FLOWCTRL"
	fi

	if grep -q "ethtool_adv_to_mii_adv_t" $srcdir/include/linux/mii.h ; then
		echo "#define BCM_HAS_ETHTOOL_ADV_TO_MII_ADV_T"
	fi
fi

if [ -f $srcdir/include/linux/phy.h ]; then
	if grep -q "mdiobus_alloc" $srcdir/include/linux/phy.h ; then
		echo "#define BCM_HAS_MDIOBUS_ALLOC"
	fi

	if grep -q "struct device *parent" $srcdir/include/linux/phy.h ; then
		echo "#define BCM_MDIOBUS_HAS_PARENT"
	fi
fi

if [ -f $srcdir/include/linux/dma-mapping.h ]; then
	if grep -q "dma_data_direction" $srcdir/include/linux/dma-mapping.h ; then
		echo "#define BCM_HAS_DMA_DATA_DIRECTION"
	fi

	if grep -q "dma_unmap_addr" $srcdir/include/linux/dma-mapping.h ; then
		echo "#define BCM_HAS_DMA_UNMAP_ADDR"
		echo "#define BCM_HAS_DMA_UNMAP_ADDR_SET"
	fi
fi

if grep -q dma_zalloc_coherent $srcdir/include/linux/dma-mapping.h; then
	echo "#define BCM_HAS_DMA_ZALLOC_COHERENT"
fi

if grep -q pci_unmap_single $srcdir/include/linux/dma-mapping.h; then
	echo "#define BCM_HAS_PCI_DMA_API"
fi

if [ -f $srcdir/include/$UAPI/linux/net_tstamp.h ]; then
	if grep -q "HWTSTAMP_FILTER_PTP_V2_EVENT" $srcdir/include/$UAPI/linux/net_tstamp.h ; then
		echo "#define BCM_HAS_IEEE1588_SUPPORT"
	fi
elif [ -f $srcdir/include/linux/net_tstamp.h ]; then
	if grep -q "HWTSTAMP_FILTER_PTP_V2_EVENT" $srcdir/include/linux/net_tstamp.h ; then
		echo "#define BCM_HAS_IEEE1588_SUPPORT"
	fi
fi

if [ -f $srcdir/include/linux/ptp_clock_kernel.h ]; then
	TGT_H=$srcdir/include/linux/ptp_clock_kernel.h
	num_args=`awk '/ptp_clock_register\(struct/,/\)/ {printf $0; exit}' $TGT_H | awk -F ',' '{print NF}'`
	if [ $num_args -eq 2 ]; then
		echo "#define BCM_HAS_PTP_CLOCK_REG_HAS_PARENT"
	fi

	if grep -q "gettime64" $TGT_H ; then
	        echo "#define BCM_HAS_PTPTIME64"
	fi
fi

if grep -q mmd_eee_adv_to_ethtool_adv_t $srcdir/include/linux/mdio.h; then
	echo "#define BCM_HAS_MMD_EEE_ADV_TO_ETHTOOL"
fi

if grep -q "shutdown" $srcdir/include/linux/pci.h ; then
        echo "#define BCM_HAS_PCI_PMOPS_SHUTDOWN"
fi

if grep -q "set_rxfh_indir" $srcdir/include/linux/ethtool.h ; then
        echo "#define BCM_HAS_OLD_RXFH_INDIR"
elif grep -A 2 "set_rxfh" $srcdir/include/linux/ethtool.h | grep -q "hfunc" ; then
		echo "#define BCM_HAS_NEW_RXFH_HASH"
fi

if grep -q "pci_channel_offline" $srcdir/include/linux/pci.h ; then
        echo "#define BCM_HAS_PCI_CHANNEL_OFFLINE"
fi

if grep -q "pci_channel_io_normal" $srcdir/include/linux/pci.h ; then
        echo "#define BCM_HAS_PCI_CHANNEL_IO_NORMAL_ENUM"
fi

if grep -q "vlan_tx_tag_present" $srcdir/include/linux/if_vlan.h ; then
        echo "#define BCM_HAS_VLAN_TX_TAG_PRESENT"
fi

if grep -q "vlan_tx_tag_get" $srcdir/include/linux/if_vlan.h ; then
        echo "#define BCM_HAS_VLAN_TX_TAG_GET"
fi

if cat $srcdir/include/linux/netdevice.h | awk '/{/{p&&p++}/struct net_device {/{p=1}{if(p)print}/}/{p&&p--}' | grep -q max_mtu ; then
	echo "#define BCM_HAS_MAX_MTU"
fi

if grep -A 1 "ndo_tx_timeout" $srcdir/include/linux/netdevice.h | \
   grep -q -o txqueue; then
	echo "#define BCM_HAS_NEW_TX_TIMEOUT"
fi

if grep -q "skb_free_frag" $srcdir/include/linux/skbuff.h ; then
	echo "#define BCM_HAS_SKB_FREE_FRAG"
fi

if grep -q "dev_consume_skb_any" $srcdir/include/linux/netdevice.h ; then
	echo "#define BCM_HAS_CONSUME_SKB"
fi

if grep -q "setup_timer" $srcdir/include/linux/timer.h ; then
	echo "#define BCM_HAS_SETUP_TIMER"
fi

if grep -q "timer_setup" $srcdir/include/linux/timer.h ; then
	echo "#define BCM_HAS_TIMER_SETUP"
fi

if [ -f $srcdir/include/linux/sched/signal.h ] ; then
	echo "#define BCM_HAS_SIGNAL_PENDING"
fi

if grep -q "pci_enable_msix_range" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_MSIX_RANGE"
fi

if grep -q "ndo_change_mtu_rh74" $srcdir/include/linux/netdevice.h ; then
	echo "#define BCM_HAS_CHANGE_MTU_RH74"
fi

if grep -q "napi_complete_done" $srcdir/include/linux/netdevice.h ; then
	echo "#define BCM_HAS_NAPI_COMPLETE_DONE"
fi

if [ -f $srcdir/include/linux/time64.h ]; then
	if grep -q "timespec64_to_ns" $srcdir/include/linux/time64.h; then
	        echo "#define BCM_HAS_TIMESPEC64"
	fi
fi

if grep -q "netdev_rss_key_fill" $srcdir/include/linux/netdevice.h ; then
	echo "#define BCM_HAS_NETDEV_RSS_KEY_FILL"
fi

if grep -q "ndo_eth_ioctl" $srcdir/include/linux/netdevice.h ; then
	echo "#define BCM_HAS_NDO_ETH_IOCTL"
fi

if grep -q "ndo_siocdevprivate" $srcdir/include/linux/netdevice.h ; then
	echo "#define BCM_HAS_NDO_SIOC_PRIV"
fi

if grep -q "eth_hw_addr_set" $srcdir/include/linux/etherdevice.h ; then
	echo "#define BCM_HAS_ETH_HW_ADDR_SET"
fi

if grep -q "pci_vpd_find_tag" $srcdir/include/linux/pci.h ; then
	if grep -q "pci_vpd_find_tag" $srcdir/include/linux/pci.h | grep -q "unsigned int off" ; then
		echo "#define BCM_HAS_OLD_VPD_FIND_TAG"
	else
		echo "#define BCM_HAS_OLD_VPD_FIND_TAG_WO_OFF"
	fi
fi

if grep -q "pci_vpd_alloc" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_NEW_VPD_SCHEME"
fi

if grep -q "pci_vpd_find_info_keyword" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_VPD_FIND_INFO_KEYWORD"
fi

if grep -q "pci_vpd_lrdt_size" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_VPD_LRDT_SIZE"
fi

if grep -q "pci_vpd_srdt_size" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_VPD_SRDT_SIZE"
fi

if grep -q "pci_vpd_info_field_size" $srcdir/include/linux/pci.h ; then
	echo "#define BCM_HAS_VPD_INFO_FIELD_SIZE"
fi

if [ -f $srcdir/include/linux/compiler_attributes.h ]; then
	if grep -q "fallthrough" $srcdir/include/linux/compiler_attributes.h ; then
		echo "#define BCM_HAS_FALLTHROUGH"
	fi
elif [ -f $srcdir/include/linux/compiler-gcc.h ]; then
	if grep -q "fallthrough" $srcdir/include/linux/compiler-gcc.h ; then
		echo "#define BCM_HAS_FALLTHROUGH"
	fi
fi

if grep -A 2 "get_coalesce" $srcdir/include/linux/ethtool.h | \
   grep -q -o "kernel_ethtool_coalesce"; then
	echo "#define BCM_HAS_CQE_ETHTOOL_COALESCE"
fi

if grep -A 2 "netif_napi_add(" $srcdir/include/linux/netdevice.h | grep -q -o "int weight"; then
	echo "#define BCM_HAS_OLD_NETIF_NAPI_ADD"
fi

if grep -q "do_aux_work" $srcdir/include/linux/ptp_clock_kernel.h ; then
	echo "#define BCM_HAS_PTP_AUX_WORK"
fi
