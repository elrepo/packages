#!/usr/bin/make
# Makefile for building Linux Broadcom Gigabit ethernet driver as a module.
# $id$
KVER=
ifeq ($(KVER),)
  KVER=$(shell uname -r)
endif

KVER_MAJ=$(shell echo $(KVER) | cut -d "." -f1)

__ARCH=$(shell uname -m)

# PREFIX may be set by the RPM build to set the effective root.
PREFIX=
ifeq ($(shell ls /lib/modules/$(KVER)/build > /dev/null 2>&1 && echo build),)
# SuSE source RPMs
  _KVER=$(shell echo $(KVER) | cut -d "-" -f1,2)
  _KFLA=$(shell echo $(KVER) | cut -d "-" -f3)
  _ARCH=$(shell file -b /lib/modules/$(shell uname -r)/build | cut -d "/" -f5)
  ifeq ($(_ARCH),)
    _ARCH=$(__ARCH)
  endif
  ifeq ($(shell ls /usr/src/linux-$(_KVER)-obj > /dev/null 2>&1 && echo linux),)
    ifeq ($(shell ls /usr/src/kernels/$(KVER)-$(__ARCH) > /dev/null 2>&1 && echo linux),)
      LINUX=
    else
      LINUX=/usr/src/kernels/$(KVER)-$(__ARCH)
      LINUXSRC=$(LINUX)
    endif
  else
    LINUX=/usr/src/linux-$(_KVER)-obj/$(_ARCH)/$(_KFLA)
    LINUXSRC=/usr/src/linux-$(_KVER)
  endif
else
  LINUX=/lib/modules/$(KVER)/build
  ifeq ($(shell ls /lib/modules/$(KVER)/source > /dev/null 2>&1 && echo source),)
    LINUXSRC=$(LINUX)
  else
    LINUXSRC=/lib/modules/$(KVER)/source
  endif
endif

KDIR ?= $(srctree)
ifneq ($(KDIR),)
  LINUX=$(KDIR)
  LINUXSRC=$(LINUX)
endif

ifeq ($M,)
  BNXT_SRC=$(CURDIR)
else
  BNXT_SRC=$(M)
endif

ifeq ($(shell ls $(LINUXSRC)/include/uapi/linux > /dev/null 2>&1 && echo uapi),)
  UAPI=
else
  UAPI=uapi
endif

ifeq ($(BCMMODDIR),)
  ifeq ($(shell ls /lib/modules/$(KVER)/updates > /dev/null 2>&1 && echo 1),1)
    BCMMODDIR=/lib/modules/$(KVER)/updates
  else
    ifeq ($(shell grep -q "search.*[[:space:]]updates" /etc/depmod.conf > /dev/null 2>&1 && echo 1),1)
      BCMMODDIR=/lib/modules/$(KVER)/updates
    else
      ifeq ($(shell grep -q "search.*[[:space:]]updates" /etc/depmod.d/* > /dev/null 2>&1 && echo 1),1)
        BCMMODDIR=/lib/modules/$(KVER)/updates
      else
        ifeq ($(shell expr $(KVER_MAJ) \>= 3), 1)
          BCMMODDIR=/lib/modules/$(KVER)/updates
        else
          BCMMODDIR=/lib/modules/$(KVER)/kernel/drivers/net
        endif
      endif
    endif
  endif
endif

ifeq ($(shell type ofed_info >/dev/null 2>&1 && echo 1),1)
ifneq ($(shell ls /usr/src/ofa_kernel/$(__ARCH) | grep -s $(KVER)),)
OFED_VERSION=$(shell ofed_info -s | grep -oP '(?<=OFED-|OFED-internal-|OFED_LINUX-)[0-9]+\.[0-9]+' | sort | uniq)
ifneq ($(OFED_VERSION), )
    $(warning Using OFED version $(OFED_VERSION))
    DISTRO_CFLAG = -DHAVE_EXTERNAL_OFED
    OFA_KERNEL = /usr/src/ofa_kernel/$(__ARCH)/$(KVER)/
    OFA_INC += -I$(OFA_KERNEL)/ \
               -I$(OFA_KERNEL)/include/ \
               -I$(OFA_KERNEL)/include/linux \
               -I$(OFA_KERNEL)/include/uapi
    OFA_INC += -D__OFED_BUILD__
    KBUILD_EXTRA_SYMBOLS += $(OFA_KERNEL)/Module.symvers
    export OFA_KERNEL
endif
endif
else
    DISTRO_CFLAG =
endif

ifneq ($(shell grep -o "pci_enable_msix_range" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_MSIX_RANGE
else
  DISTRO_CFLAG +=
endif

ifneq ($(shell grep -o "pci_msix_can_alloc_dyn" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_MSIX_DYN
endif

ifneq ($(shell grep -so "GENL_ID_GENERATE" $(LINUXSRC)/include/uapi/linux/genetlink.h),)
  DISTRO_CFLAG += -DHAVE_GENL_ID_GENERATE
endif

ifneq ($(shell grep -w "inet_pernet_hashinfo_alloc" $(LINUXSRC)/include/net/inet_hashtables.h),)
  DISTRO_CFLAG += -DHAVE_PERNET_HASH
endif

ifneq ($(shell grep "genl_register_family_with_ops" $(LINUXSRC)/include/net/genetlink.h),)
  DISTRO_CFLAG += -DHAVE_GENL_REG_FAMILY_WITH_OPS
endif

ifneq ($(shell grep -A 8 "genl_family {" $(LINUXSRC)/include/net/genetlink.h | grep -o "struct nla_policy"),)
  DISTRO_CFLAG += -DHAVE_GENL_POLICY
endif

ifneq ($(shell grep "hlist_for_each_entry_safe" $(LINUXSRC)/include/linux/list.h | grep "tpos" > /dev/null 2>&1 && echo tpos),)
  DISTRO_CFLAG += -DHAVE_OLD_HLIST
endif

ifneq ($(shell grep -o "csum_level" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DHAVE_CSUM_LEVEL
endif

ifneq ($(shell grep -o "build_skb" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DHAVE_BUILD_SKB
  ifneq ($(shell grep "build_skb" $(LINUXSRC)/include/linux/skbuff.h | grep "int frag_size" > /dev/null 2>&1 && echo frag_size),)
    DISTRO_CFLAG += -DHAVE_NEW_BUILD_SKB
  endif
  ifneq ($(shell grep -o "napi_alloc_frag" $(LINUXSRC)/include/linux/skbuff.h),)
    DISTRO_CFLAG += -DHAVE_NAPI_ALLOC_FRAG
  endif
  ifneq ($(shell grep -o "skb_free_frag" $(LINUXSRC)/include/linux/skbuff.h),)
    DISTRO_CFLAG += -DHAVE_SKB_FREE_FRAG
  endif
  ifneq ($(shell grep -o "^struct sk_buff \*napi_build_skb" $(LINUXSRC)/include/linux/skbuff.h),)
    DISTRO_CFLAG += -DHAVE_NAPI_BUILD_SKB
  endif
endif

ifneq ($(shell grep -o "inner_network_offset" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DHAVE_INNER_NETWORK_OFFSET
  ifneq ($(shell grep -o "inner_eth_hdr" $(LINUXSRC)/include/linux/if_ether.h),)
    DISTRO_CFLAG += -DHAVE_INNER_ETH_HDR
  endif
endif

ifeq ($(shell grep -o "skb_frag_size" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DNO_SKB_FRAG_SIZE
endif

ifneq ($(shell grep -so "flow_keys" $(LINUXSRC)/include/net/flow_keys.h),)
  DISTRO_CFLAG += -DHAVE_FLOW_KEYS
endif

ifneq ($(shell grep -o "PKT_HASH_TYPE" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DHAVE_SKB_HASH_TYPE
endif

ifneq ($(shell grep -o "SKB_GSO_UDP_TUNNEL_CSUM" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DHAVE_SKB_GSO_UDP_TUNNEL_CSUM
else
ifneq ($(shell grep -o "SKB_GSO_UDP_TUNNEL" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DHAVE_SKB_GSO_UDP_TUNNEL
endif
endif

ifneq ($(shell grep -so "define NAPI_GRO_CB" $(LINUXSRC)/include/net/gro.h),)
  DISTRO_CFLAG += -DHAVE_GRO_H
endif

ifneq ($(shell grep -so "define _LINUX_NET_QUEUES_H" $(LINUXSRC)/include/net/netdev_queues.h),)
  DISTRO_CFLAG += -DHAVE_NETDEV_QUEUES_H
endif

ifneq ($(shell grep -so "struct netdev_queue_mgmt_ops" $(LINUXSRC)/include/net/netdev_queues.h),)
  DISTRO_CFLAG += -DHAVE_NETDEV_QMGMT_OPS
endif

ifneq ($(shell grep -so "struct netdev_stat_ops" $(LINUXSRC)/include/net/netdev_queues.h),)
  DISTRO_CFLAG += -DHAVE_NETDEV_STAT_OPS
endif

ifneq ($(shell grep -so "struct netdev_queue_mgmt_ops" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_QMGMT_OPS_IN_NETDEV_H
endif

ifneq ($(shell grep -so "netdev_rx_queue_restart" $(LINUXSRC)/include/net/netdev_rx_queue.h),)
  DISTRO_CFLAG += -DHAVE_NETDEV_RX_Q_RESTART
endif

ifneq ($(shell grep -so "page_pool_disable_direct_recycling" $(LINUXSRC)/include/net/page_pool/types.h),)
  DISTRO_CFLAG += -DHAVE_PAGE_POOL_DISABLE_DIRECT_RECYCLING
endif

ifneq ($(shell grep -o "skb_frag_page" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DHAVE_SKB_FRAG_PAGE
endif

ifneq ($(shell grep -o "skb_frag_off_add" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DHAVE_SKB_FRAG_ACCESSORS
endif

ifneq ($(shell grep -o "typedef struct bio_vec skb_frag_t" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DSKB_FRAG_USES_BIO
endif

ifneq ($(shell grep -o "skb_frag_fill_page_desc" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DHAVE_SKB_FRAG_FILL_PAGE_DESC
endif

ifneq ($(shell grep "skb_checksum_none_assert" $(LINUXSRC)/include/linux/skbuff.h > /dev/null 2>&1 && echo skb_cs_none_assert),)
  DISTRO_CFLAG += -DHAVE_SKB_CHECKSUM_NONE_ASSERT
endif

ifneq ($(shell grep -o "xmit_more" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DHAVE_SKB_XMIT_MORE
endif

ifneq ($(shell grep -so "skb_mark_for_recycle" $(LINUXSRC)/include/linux/skbuff.h),)
    DISTRO_CFLAG += -DHAVE_SKB_MARK_RECYCLE
  ifneq ($(shell grep "skb_mark_for_recycle" $(LINUXSRC)/include/linux/skbuff.h | grep "struct page"),)
    DISTRO_CFLAG += -DHAVE_OLD_SKB_MARK_RECYCLE
  endif
endif

ifneq ($(shell grep -so "min_tx_rate" $(LINUXSRC)/include/$(UAPI)/linux/if_link.h),)
  DISTRO_CFLAG += -DHAVE_IFLA_TX_RATE
endif

ifneq ($(shell grep -so "IFLA_XDP_PROG_ID" $(LINUXSRC)/include/$(UAPI)/linux/if_link.h),)
  DISTRO_CFLAG += -DHAVE_IFLA_XDP_PROG_ID
endif

ifneq ($(shell grep -o "dma_set_mask_and_coherent" $(LINUXSRC)/include/linux/dma-mapping.h),)
  DISTRO_CFLAG += -DHAVE_SET_MASK_AND_COHERENT
endif

ifneq ($(shell grep -o "dma_set_coherent_mask" $(LINUXSRC)/include/linux/dma-mapping.h),)
  DISTRO_CFLAG += -DHAVE_SET_COHERENT_MASK
endif

ifneq ($(shell grep -w "vmap" $(LINUXSRC)/include/linux/dma-buf.h | grep "struct dma_buf_map"),)
  DISTRO_CFLAG += -DHAVE_DMABUF_NEW_VMAP
endif

ifneq ($(shell grep -w "vmap" $(LINUXSRC)/include/linux/dma-buf.h | grep "struct iosys_map"),)
  DISTRO_CFLAG += -DHAVE_IOSYS_VMAP
endif

ifneq ($(shell ls $(LINUXSRC)/include/linux/dma-attrs.h > /dev/null 2>&1 && echo dma_attrs),)
  DISTRO_CFLAG += -DHAVE_DMA_ATTRS_H
endif

ifneq ($(shell grep -o "dma_map_page_attrs" $(LINUXSRC)/include/linux/dma-mapping.h),)
  DISTRO_CFLAG += -DHAVE_DMA_MAP_PAGE_ATTRS
else
  ifneq ($(shell grep -so "dma_map_page_attrs" $(LINUXSRC)/include/asm-generic/dma-mapping-common.h),)
    DISTRO_CFLAG += -DHAVE_DMA_MAP_PAGE_ATTRS
  endif
endif

ifneq ($(shell grep -o "dma_zalloc_coherent" $(LINUXSRC)/include/linux/dma-mapping.h),)
  DISTRO_CFLAG += -DHAVE_DMA_ZALLOC_COHERENT
endif

ifneq ($(shell grep -o "dma_pool_zalloc" $(LINUXSRC)/include/linux/dmapool.h),)
  DISTRO_CFLAG += -DHAVE_DMA_POOL_ZALLOC
endif

ifneq ($(shell grep -o "ndo_udp_tunnel_add" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_UDP_TUNNEL
  ifneq ($(shell grep -A 24 "net_device_ops_extended" $(LINUXSRC)/include/linux/netdevice.h | grep -o "ndo_udp_tunnel_add"),)
    DISTRO_CFLAG += -DHAVE_NDO_UDP_TUNNEL_RH
  endif
else
  ifneq ($(shell grep -o "ndo_add_vxlan_port" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DHAVE_NDO_ADD_VXLAN
    ifneq ($(shell grep -so "vxlan_get_rx_port" $(LINUXSRC)/include/net/vxlan.h),)
      DISTRO_CFLAG += -DHAVE_VXLAN_GET_RX_PORT
    endif
  endif
endif

ifneq ($(shell grep -o "struct dev_addr_list" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_DEV_ADDR_LIST
endif

ifneq ($(shell grep -o "ipv6_hopopt_jumbo_remove" $(LINUXSRC)/include/net/ipv6.h),)
  DISTRO_CFLAG += -DHAVE_IPV6_HOPOPT_JUMBO_REMOVE
endif

ifneq ($(shell grep "netif_set_real_num_tx" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo real_tx),)
  DISTRO_CFLAG += -DHAVE_NETIF_SET_REAL_NUM_TX
else
  DISTRO_CFLAG += -DVOID_NETIF_SET_NUM_TX
endif

ifneq ($(shell grep "netif_set_real_num_tx" $(LINUXSRC)/include/linux/netdevice.h | grep void > /dev/null 2>&1 && echo netif_set_real),)
  DISTRO_CFLAG += -DVOID_NETIF_SET_NUM_TX
endif

ifneq ($(shell grep -o "netdev_tx_sent_queue" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETDEV_TX_QUEUE_CTRL
endif

ifneq ($(shell ls $(LINUXSRC)/include/net/flow_dissector.h > /dev/null 2>&1 && echo flow),)
  DISTRO_CFLAG += -DHAVE_FLOW_DISSECTOR
  ifneq ($(shell grep -so "static inline bool skb_flow_dissect_flow_keys" $(LINUXSRC)/include/linux/skbuff.h),)
    ifneq ($(shell grep -A 2 "static inline bool skb_flow_dissect_flow_keys" $(LINUXSRC)/include/linux/skbuff.h | grep -o "unsigned int flags"),)
      DISTRO_CFLAG += -DHAVE_SKB_FLOW_DISSECT_WITH_FLAGS
    endif
    ifneq ($(shell grep -o "FLOW_DIS_ENCAPSULATION" $(LINUXSRC)/include/net/flow_dissector.h),)
      DISTRO_CFLAG += -DHAVE_FLOW_KEY_CONTROL_FLAGS
    endif
  endif
endif

ifneq ($(shell ls $(LINUXSRC)/include/net/flow_offload.h > /dev/null 2>&1 && echo flow_offload),)
  DISTRO_CFLAG += -DHAVE_FLOW_OFFLOAD_H
  ifneq ($(shell grep -so "struct flow_cls_offload" $(LINUXSRC)/include/net/flow_offload.h),)
    DISTRO_CFLAG += -DHAVE_TC_FLOW_CLS_OFFLOAD
  endif
  ifneq ($(shell grep -o "flow_block_cb_setup_simple" $(LINUXSRC)/include/net/flow_offload.h),)
    DISTRO_CFLAG += -DHAVE_SETUP_TC_BLOCK_HELPER
  endif
  ifneq ($(shell grep -o "__flow_indr_block_cb_register" $(LINUXSRC)/include/net/flow_offload.h ||	\
	  grep -o "flow_indr_block_bind_cb_t" $(LINUXSRC)/include/net/flow_offload.h),)
    DISTRO_CFLAG += -DHAVE_FLOW_INDR_BLOCK_CB
    ifneq ($(shell grep -A 1 "void flow_indr_dev_unregister" $(LINUXSRC)/include/net/flow_offload.h | grep -o "flow_setup_cb_t \*setup_cb"),)
      DISTRO_CFLAG += -DHAVE_OLD_FLOW_INDR_DEV_UNRGTR
    endif
    ifneq ($(shell grep -A 5 "flow_indr_block_cb_alloc" $(LINUXSRC)/include/net/flow_offload.h | grep -o "Qdisc \*sch"),)
      DISTRO_CFLAG += -DHAVE_FLOW_INDR_BLOCK_CB_QDISC
    endif
    ifneq ($(shell grep -A 6 "struct vxlan_rdst {" $(LINUXSRC)/include/net/vxlan.h | grep -o "\*remote_dev"),)
      DISTRO_CFLAG += -DHAVE_VXLAN_RDST_RDEV
    endif
  endif
  ifneq ($(shell grep -o "FLOW_ACTION_POLICE" $(LINUXSRC)/include/net/flow_offload.h),)
    DISTRO_CFLAG += -DHAVE_FLOW_ACTION_POLICE
  endif
  ifneq ($(shell grep -o "flow_action_basic_hw_stats_check" $(LINUXSRC)/include/net/flow_offload.h),)
    DISTRO_CFLAG += -DHAVE_FLOW_ACTION_BASIC_HW_STATS_CHECK
  endif
  ifneq ($(shell grep -o "flow_indr_dev_register" $(LINUXSRC)/include/net/flow_offload.h),)
    DISTRO_CFLAG += -DHAVE_FLOW_INDR_DEV_RGTR
  endif
  ifneq ($(shell grep -A 2 "flow_stats_update" $(LINUXSRC)/include/net/flow_offload.h | grep -o drops),)
    DISTRO_CFLAG += -DHAVE_FLOW_STATS_DROPS
  endif
  ifneq ($(shell grep -A 3 "flow_indr_block_bind_cb_t" $(LINUXSRC)/include/net/flow_offload.h | grep -o cleanup),)
    DISTRO_CFLAG += -DHAVE_FLOW_INDR_BLOCK_CLEANUP
  endif
  ifneq ($(shell grep -o "cb_list_head" $(LINUXSRC)/include/net/flow_offload.h),)
    DISTRO_CFLAG += -DHAVE_FLOW_INDIR_BLK_PROTECTION
  endif
  ifneq ($(shell grep -o "FLOW_ACTION_MIRRED_INGRESS" $(LINUXSRC)/include/net/flow_offload.h),)
    DISTRO_CFLAG += -DHAVE_FLOW_ACTION_MIRRED_INGRESS
  endif
  ifneq ($(shell grep -o "flow_stats_update" $(LINUXSRC)/include/net/flow_offload.h),)
    DISTRO_CFLAG += -DHAVE_FLOW_STATS_UPDATE
  endif
  ifneq ($(shell grep -o "flow_rule_match_has_control_flags" $(LINUXSRC)/include/net/flow_offload.h),)
    DISTRO_CFLAG += -DHAVE_FLOW_RULE_MATCH_CONTROL_FLAGS
  endif
endif

ifneq ($(shell grep -o "bitmap_zalloc" $(LINUXSRC)/include/linux/bitmap.h),)
    DISTRO_CFLAG += -DHAVE_BITMAP_ZALLOC
endif

ifneq ($(shell ls $(LINUXSRC)/include/net/udp_tunnel.h > /dev/null 2>&1 && echo udp_tunnel),)
  DISTRO_CFLAG += -DHAVE_UDP_TUNNEL_H
endif

ifneq ($(shell ls $(LINUXSRC)/include/net/tc_act/tc_mpls.h > /dev/null 2>&1 && echo tc_mpls),)
  DISTRO_CFLAG += -DHAVE_TC_MPLS_H
endif

ifeq ($(shell grep -F DEFINE_FREE $(LINUXSRC)/include/linux/slab.h | grep -Fq kvfree; echo $$?),0)
  DISTRO_CFLAG += -DHAVE_CLEANUP_KVFREE
endif

ifneq ($(shell grep -o "ether_addr_equal" $(LINUXSRC)/include/linux/etherdevice.h),)
  DISTRO_CFLAG += -DHAVE_ETHER_ADDR_EQUAL
endif

ifneq ($(shell grep -o "ether_addr_copy" $(LINUXSRC)/include/linux/etherdevice.h),)
  DISTRO_CFLAG += -DHAVE_ETHER_ADDR_COPY
endif

ifneq ($(shell grep -o "eth_broadcast_addr" $(LINUXSRC)/include/linux/etherdevice.h),)
  DISTRO_CFLAG += -DHAVE_ETH_BROADCAST_ADDR
endif

ifneq ($(shell grep -o "eth_get_headlen" $(LINUXSRC)/include/linux/etherdevice.h),)
  DISTRO_CFLAG += -DHAVE_ETH_GET_HEADLEN
endif

ifneq ($(shell grep -o "eth_hw_addr_random" $(LINUXSRC)/include/linux/etherdevice.h),)
  DISTRO_CFLAG += -DHAVE_ETH_HW_ADDR_RANDOM
endif

ifneq ($(shell grep -s "eth_get_headlen" $(LINUXSRC)/include/linux/etherdevice.h | grep -o "struct net_device"),)
  DISTRO_CFLAG += -DHAVE_ETH_GET_HEADLEN_NEW
endif

ifneq ($(shell grep -o "eth_hw_addr_set" $(LINUXSRC)/include/linux/etherdevice.h),)
  DISTRO_CFLAG += -DHAVE_ETH_HW_ADDR_SET
endif

ifneq ($(shell grep -A 2 "get_ringparam" $(LINUXSRC)/include/linux/ethtool.h | sed '/UEK_KABI_USE/,+2 d' | grep -o "struct kernel_ethtool_ringparam"),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_GET_RING_EXT
  ifneq ($(shell grep -so "ETHTOOL_TCP_DATA_SPLIT_ENABLED" $(LINUXSRC)/include/uapi/linux/ethtool_netlink*.h),)
    DISTRO_CFLAG += -DHAVE_ETHTOOL_TCP_DATA_SPLIT
  endif
endif

ifneq ($(shell grep -o "ETHTOOL_RING_USE_HDS_THRS" $(LINUXSRC)/include/linux/ethtool.h),)
  ifneq ($(shell grep -so "struct netdev_config" $(LINUXSRC)/include/net/netdev_queues.h),)
    DISTRO_CFLAG += -DHAVE_ETHTOOL_RING_USE_HDS_THRS
  endif
endif

ifneq ($(shell grep -o "get_rxnfc" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_RXNFC
  ifneq ($(shell grep -A 2 "get_rxnfc" $(LINUXSRC)/include/linux/ethtool.h | grep -o "void"),)
    DISTRO_CFLAG += -DHAVE_RXNFC_VOID
  endif
endif

ifneq ($(shell grep -o "get_rxfh_key_size" $(LINUXSRC)/include/linux/ethtool.h),)
  ifneq ($(shell grep -o "ETH_RSS_HASH_TOP" $(LINUXSRC)/include/linux/ethtool.h),)
    DISTRO_CFLAG += -DHAVE_GET_RXFH_KEY_SIZE
  endif
endif

ifneq ($(shell grep -o "(\*set_rxfh)" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_SET_RXFH
endif

ifneq ($(shell grep -o "(\*set_rxfh_fields)" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_SET_RXFH_FIELDS
endif

ifneq ($(shell grep -o "rxfh_per_ctx_key" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_RXFH_PERCTX_KEY
endif

ifneq ($(shell grep -o "get_rxfh_indir_size" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_RXFH_INDIR_SIZE
endif

ifneq ($(shell grep -o "set_phys_id" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_SET_PHYS_ID
endif

ifneq ($(shell grep -o "irq_update_affinity_hint" $(LINUXSRC)/include/linux/interrupt.h),)
  DISTRO_CFLAG += -DHAVE_IRQ_UPDATE_AFFINITY_HINT
endif

ifneq ($(shell grep -A 2 "get_coalesce" $(LINUXSRC)/include/linux/ethtool.h | \
               grep -o "kernel_ethtool_coalesce"),)
  DISTRO_CFLAG += -DHAVE_CQE_ETHTOOL_COALESCE
endif

ifneq ($(shell grep -o "kernel_ethtool_ts_info" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_KERNEL_ETHTOOL_TS_INFO
endif

ifneq ($(shell grep -o "ethtool_sprintf" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_SPRINTF
endif

ifneq ($(shell grep -o "ethtool_puts" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_PUTS
endif

ifneq ($(shell grep -so "ethtool_tcpip6_spec" $(LINUXSRC)/include/$(UAPI)/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_IP6_SPEC
endif

ifneq ($(shell grep -so "linkmode_set_bit" $(LINUXSRC)/include/linux/linkmode.h),)
  DISTRO_CFLAG += -DHAVE_LINKMODE
  ifneq ($(shell grep -so "linkmode_resolve_pause" $(LINUXSRC)/include/linux/linkmode.h),)
    DISTRO_CFLAG += -DHAVE_NEW_LINKMODE
  endif
endif

ifeq ($(shell grep -o "rx_cpu_rmap" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DNO_NETDEV_CPU_RMAP
else
  ifneq ($(shell grep -o "irq_run_affinity_notifiers" $(LINUXSRC)/include/linux/interrupt.h),)
    DISTRO_CFLAG += -DNO_NETDEV_CPU_RMAP
  endif
endif

ifneq ($(shell grep -o "hw_features" $(LINUXSRC)/include/linux/netdevice.h),)
  ifeq ($(shell grep -o "get_netdev_hw_features" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DNETDEV_HW_FEATURES
  endif
endif

ifneq ($(shell grep -o "netdev_notifier_info_to_dev" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETDEV_NOTIFIER_INFO_TO_DEV
endif

ifneq ($(shell grep -o "netif_napi_add_config" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETIF_NAPI_ADD_CONFIG
endif

ifneq ($(shell grep "register_netdevice_notifier_rh" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_REGISTER_NETDEVICE_NOTIFIER_RH
endif

ifneq ($(shell grep -o "hw_enc_features" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DNETDEV_HW_ENC_FEATURES
endif

ifneq ($(shell grep -o "sriov_configure" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DPCIE_SRIOV_CONFIGURE
  ifneq ($(shell grep -A 2 "pci_driver_rh" $(LINUXSRC)/include/linux/pci.h | \
                 grep -o "sriov_configure"),)
    DISTRO_CFLAG += -DSRIOV_CONF_DEF_IN_PCI_DRIVER_RH
  endif
endif

ifneq ($(shell grep -o "pci_vfs_assigned" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCI_VFS_ASSIGNED
endif

ifneq ($(shell grep -o "pci_num_vf" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCI_NUM_VF
endif

ifneq ($(shell grep -o "ndo_fix_features" $(LINUXSRC)/include/linux/netdevice.h),)
  ifeq ($(shell grep -o "net_device_ops_ext" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DNETDEV_FEATURE_CONTROL
  endif
  ifneq ($(shell grep -o "net_device_ops_extended" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DNETDEV_FEATURE_CONTROL
  endif
endif

ifneq ($(shell grep -o "ndo_features_check" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_FEATURES_CHECK
endif

ifneq ($(shell grep -o "ndo_rx_flow_steer" $(LINUXSRC)/include/linux/netdevice.h),)
  ifeq ($(shell grep -o "netdev_rfs_info" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DNETDEV_RX_FLOW_STEER
  endif
endif

ifneq ($(shell grep -o "ndo_busy_poll" $(LINUXSRC)/include/linux/netdevice.h),)
  ifeq ($(shell grep -o "net_device_extended" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DNETDEV_BUSY_POLL
  endif
endif

ifneq ($(shell grep -o "ndo_get_stats64" $(LINUXSRC)/include/linux/netdevice.h),)
  ifeq ($(shell grep -o "net_device_ops_ext" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DNETDEV_GET_STATS64
  endif
  ifneq ($(shell grep -o "net_device_ops_extended" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DNETDEV_GET_STATS64
  endif
  ifneq ($(shell grep "ndo_get_stats64" $(LINUXSRC)/include/linux/netdevice.h | grep -o "void"),)
    DISTRO_CFLAG += -DNETDEV_GET_STATS64_VOID
  endif
endif

ifneq ($(shell grep -o "ndo_get_vf_config" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_GET_VF_CONFIG
endif

ifneq ($(shell grep -A 2 "ndo_bridge_getlink" $(LINUXSRC)/include/linux/netdevice.h | grep -o "nlflags"),)
  ifneq ($(shell grep -A 3 "ndo_dflt_bridge_getlink" $(LINUXSRC)/include/linux/rtnetlink.h | grep -o "filter_mask"),)
    DISTRO_CFLAG += -DHAVE_NDO_BRIDGE_GETLINK
  endif
endif

ifneq ($(shell grep -A 4 "ndo_bridge_setlink" $(LINUXSRC)/include/linux/netdevice.h | grep -o "netlink_ext_ack"),)
  DISTRO_CFLAG += -DHAVE_NDO_BRIDGE_SETLINK_EXTACK
endif

ifneq ($(shell grep -o "ndo_set_vf_link_state" $(LINUXSRC)/include/linux/netdevice.h),)
  ifeq ($(shell grep -o "net_device_ops_ext" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DHAVE_NDO_SET_VF_LINK_STATE
  endif
  ifneq ($(shell grep -o "net_device_ops_extended" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DHAVE_NDO_SET_VF_LINK_STATE
  endif
endif

ifneq ($(shell grep -o "ndo_set_vf_spoofchk" $(LINUXSRC)/include/linux/netdevice.h),)
  ifeq ($(shell grep -o "net_device_ops_ext" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DHAVE_VF_SPOOFCHK
  endif
  ifneq ($(shell grep -o "net_device_ops_extended" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DHAVE_VF_SPOOFCHK
  endif
endif

ifneq ($(shell grep -A 2 "ndo_set_vf_vlan" $(LINUXSRC)/include/linux/netdevice.h | grep -o "proto"),)
  ifeq ($(shell grep -o "RH_KABI_EXTEND(struct net_device_ops_extended extended)" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DNEW_NDO_SET_VF_VLAN
  endif
endif

ifneq ($(shell grep -o "ndo_set_vf_vlan_rh73" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_SET_VF_VLAN_RH73
endif

ifneq ($(shell grep -o "ndo_set_vf_trust" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_SET_VF_TRUST
  ifneq ($(shell grep -A 3 "net_device_ops_extended" $(LINUXSRC)/include/linux/netdevice.h | grep -o "ndo_set_vf_trust"),)
    DISTRO_CFLAG += -DHAVE_NDO_SET_VF_TRUST_RH
  endif
endif

ifneq ($(shell grep -o "ndo_change_mtu_rh74" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_CHANGE_MTU_RH74
endif

ifneq ($(shell grep -o "RH_KABI_USE_P(16, struct net_device_extended \*extended)" $(LINUXSRC)/include/linux/netdevice.h),)
        DISTRO_CFLAG += -DHAVE_NET_DEVICE_EXT
endif

ifneq ($(shell grep -o "RH_KABI_EXTEND(struct net_device_ops_extended extended)" $(LINUXSRC)/include/linux/netdevice.h),)
  ifneq ($(shell grep -o "ndo_get_phys_port_name" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DHAVE_EXT_GET_PHYS_PORT_NAME
  endif
endif

ifneq ($(shell grep -o "ndo_setup_tc_rh72" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_SETUP_TC_RH72
endif

ifneq ($(shell grep -o "(\*ndo_setup_tc_rh)" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_SETUP_TC_RH
endif

ifneq ($(shell grep -o "tc_setup_cb_egdev_call" $(LINUXSRC)/include/net/act_api.h),)
  DISTRO_CFLAG += -DHAVE_TC_CB_EGDEV
endif

ifneq ($(shell grep -o "ndo_setup_tc" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_SETUP_TC
  ifneq ($(shell grep -o "struct tc_etf_qopt_offload" $(LINUXSRC)/include/net/pkt_sched.h),)
    ifneq ($(shell grep -o "skb_txtime_consumed" $(LINUXSRC)/include/net/pkt_sched.h),)
      DISTRO_CFLAG += -DHAVE_ETF_QOPT_OFFLOAD
    endif
  endif
  ifneq ($(shell grep -o "struct tc_to_netdev" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DHAVE_TC_TO_NETDEV
    ifneq ($(shell grep -o "struct tc_mqprio_qopt" $(LINUXSRC)/include/linux/netdevice.h),)
      DISTRO_CFLAG += -DHAVE_MQPRIO_QOPT
    endif
    ifneq ($(shell grep -A 1 "ndo_setup_tc" $(LINUXSRC)/include/linux/netdevice.h | grep -o "u32 chain_index"),)
      DISTRO_CFLAG += -DHAVE_CHAIN_INDEX
    endif
  endif
  ifneq ($(shell grep -o "enum tc_setup_type" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DHAVE_TC_SETUP_TYPE
  endif
  ifneq ($(shell grep -so "tc_cls_flower_offload" $(LINUXSRC)/include/net/pkt_cls.h),)
    DISTRO_CFLAG += -DHAVE_TC_CLS_FLOWER_OFFLOAD
  endif
  ifneq ($(shell grep -o "TC_SETUP_BLOCK" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DHAVE_TC_SETUP_BLOCK
  endif
  ifneq ($(shell grep -o "TC_SETUP_QDISC_MQPRIO" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DHAVE_TC_SETUP_QDISC_MQPRIO
  endif
  ifneq ($(shell grep -so "tcf_mirred_dev" $(LINUXSRC)/include/net/tc_act/tc_mirred.h),)
    DISTRO_CFLAG += -DHAVE_TCF_MIRRED_DEV
  endif
  ifneq ($(shell grep -so "tc_cls_can_offload_and_chain0" $(LINUXSRC)/include/net/pkt_cls.h),)
    DISTRO_CFLAG += -DHAVE_TC_CLS_CAN_OFFLOAD_AND_CHAIN0
  endif
  ifneq ($(shell grep -s -A 2 "tcf_block_cb_register" $(LINUXSRC)/include/net/pkt_cls.h | grep -o "netlink_ext_ack"),)
    DISTRO_CFLAG += -DHAVE_TC_CB_REG_EXTACK
  endif
  ifneq ($(shell grep -so "tcf_exts_for_each_action" $(LINUXSRC)/include/net/pkt_cls.h),)
    DISTRO_CFLAG += -DHAVE_TC_EXTS_FOR_ACTION
  endif
  ifneq ($(shell grep -s -A 3 "struct tc_cls_matchall_offload" $(LINUXSRC)/include/net/pkt_cls.h | grep -o "flow_rule"),)
    DISTRO_CFLAG += -DHAVE_TC_MATCHALL_FLOW_RULE
  endif
  ifneq ($(shell grep -so "tc_can_offload_extack" $(LINUXSRC)/include/net/pkt_cls.h),)
    DISTRO_CFLAG += -DHAVE_TC_CAN_OFFLOAD_EXTACK
  endif
endif

ifneq ($(shell grep -so "FLOW_DISSECTOR_KEY_ICMP" $(LINUXSRC)/include/net/flow_dissector.h),)
  DISTRO_CFLAG += -DHAVE_FLOW_DISSECTOR_KEY_ICMP
endif

ifneq ($(shell grep -so "FLOW_DISSECTOR_KEY_ENC_IP," $(LINUXSRC)/include/net/flow_dissector.h),)
  DISTRO_CFLAG += -DHAVE_FLOW_DISSECTOR_KEY_ENC_IP
endif

ifneq ($(shell grep -s -A 9 "struct flow_dissector_key_vlan" $(LINUXSRC)/include/net/flow_dissector.h | grep -o "vlan_tpid"),)
    DISTRO_CFLAG += -DHAVE_FLOW_DISSECTOR_KEY_VLAN_TPID
endif

ifneq ($(shell grep -so "struct flow_dissector_mpls_lse" $(LINUXSRC)/include/net/flow_dissector.h),)
    DISTRO_CFLAG += -DHAVE_FLOW_DISSECTOR_KEY_MPLS_LSE
endif

ifneq ($(shell grep -so "rhashtable" $(LINUXSRC)/include/linux/rhashtable.h),)
    DISTRO_CFLAG += -DHAVE_RHASHTABLE
endif

ifneq ($(shell grep -so "tcf_exts_to_list" $(LINUXSRC)/include/net/pkt_cls.h),)
    DISTRO_CFLAG += -DHAVE_TCF_EXTS_TO_LIST
endif

ifneq ($(shell grep -so "tcf_exts_stats_update" $(LINUXSRC)/include/net/pkt_cls.h),)
    DISTRO_CFLAG += -DHAVE_TCF_STATS_UPDATE
endif

ifneq ($(shell grep -so "tcf_exts_has_actions" $(LINUXSRC)/include/net/pkt_cls.h),)
    DISTRO_CFLAG += -DHAVE_TCF_EXTS_HAS_ACTIONS
endif

ifneq ($(shell grep -so "is_tcf_tunnel_set" $(LINUXSRC)/include/net/tc_act/tc_tunnel_key.h),)
  DISTRO_CFLAG += -DHAVE_TCF_TUNNEL
endif

ifneq ($(shell grep -o "netdev_get_num_tc" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_GET_NUM_TC
endif

ifneq ($(shell grep -so "__netif_txq_maybe_wake" $(LINUXSRC)/include/net/netdev_queues.h),)
  DISTRO_CFLAG += -DHAVE_TXQ_MAYBE_WAKE
endif

ifneq ($(shell grep -so "netdev_features_t" $(LINUXSRC)/include/linux/netdev_features.h ||	\
	 grep -o "netdev_features_t" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETDEV_FEATURES_T
endif

ifneq ($(shell grep -o "ndo_fix_features" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_FIX_FEATURES
endif

ifneq ($(shell grep -o "netif_set_real_num_rx" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETIF_SET_REAL_NUM_RX
endif

ifneq ($(shell grep -o "netif_queue_set_napi" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETIF_QUEUE_SET_NAPI
endif

ifneq ($(shell grep -o "netif_get_num_default_rss_queues" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETIF_GET_DEFAULT_RSS
endif

ifneq ($(shell grep -o "ndo_vlan_rx_register" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_VLAN_RX_REGISTER
endif

ifneq ($(shell grep -o "ndo_get_port_parent_id" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_GET_PORT_PARENT_ID
endif

ifneq ($(shell grep -o "net_device_ops_extended" $(LINUXSRC)/include/linux/netdevice.h),)
  ifneq ($(shell grep -o "ndo_xdp_xmit" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DHAVE_EXT_NDO_XDP_XMIT
  endif
else ifneq ($(shell grep -o "ndo_xdp" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_XDP
  ifneq ($(shell grep -o "ndo_bpf" $(LINUXSRC)/include/linux/netdevice.h),)
    DISTRO_CFLAG += -DHAVE_NDO_BPF
  endif
  ifneq ($(shell ls $(LINUXSRC)/include/linux/bpf_trace.h > /dev/null 2>&1 && echo bpf_trace),)
    DISTRO_CFLAG += -DHAVE_BPF_TRACE
  endif
  ifneq ($(shell grep -o "skb_metadata_set" $(LINUXSRC)/include/linux/skbuff.h),)
    DISTRO_CFLAG += -DHAVE_XDP_DATA_META
  endif
  ifneq ($(shell grep -o "void bpf_prog_add" $(LINUXSRC)/include/linux/bpf.h),)
    DISTRO_CFLAG += -DHAVE_VOID_BPF_PROG_ADD
  endif
  ifneq ($(shell grep "void bpf_warn_invalid_xdp_action" $(LINUXSRC)/include/linux/filter.h | grep -o "struct net_device"),)
    DISTRO_CFLAG += -DHAVE_BPF_WARN_INVALID_XDP_ACTION_EXT
  endif
endif

ifneq ($(shell grep -so "xdp_frags_size" $(LINUXSRC)/include/linux/skbuff.h),)
  DISTRO_CFLAG += -DHAVE_XDP_MULTI_BUFF
endif

ifneq ($(shell grep -so " __skb_frag_ref" $(LINUXSRC)/include/linux/skbuff_ref.h),)
  DISTRO_CFLAG += -DHAVE_SKBUFF_REF
endif

ifneq ($(shell grep -so "enum xdp_action" $(LINUXSRC)/include/uapi/linux/bpf.h),)
  DISTRO_CFLAG += -DHAVE_XDP_ACTION
endif
ifneq ($(shell grep -so "XDP_REDIRECT" $(LINUXSRC)/include/uapi/linux/bpf.h),)
  DISTRO_CFLAG += -DHAVE_XDP_REDIRECT
endif
ifneq ($(shell grep -so "struct xdp_frame" $(LINUXSRC)/include/net/xdp.h),)
  DISTRO_CFLAG += -DHAVE_XDP_FRAME
endif
ifneq ($(shell grep -so "xdp_features_set_redirect_target_locked" $(LINUXSRC)/include/net/xdp.h),)
  DISTRO_CFLAG += -DHAVE_XDP_SET_REDIR_TARGET_LOCKED
  DISTRO_CFLAG += -DHAVE_XDP_SET_REDIR_TARGET
else ifneq ($(shell grep -so "xdp_features_set_redirect_target" $(LINUXSRC)/include/net/xdp.h),)
  DISTRO_CFLAG += -DHAVE_XDP_SET_REDIR_TARGET
endif
ifneq ($(shell grep -so "enum xdp_mem_type" $(LINUXSRC)/include/net/xdp.h),)
  DISTRO_CFLAG += -DHAVE_XDP_MEM_TYPE
endif
ifneq ($(shell grep -so "xdp_get_shared_info_from_buff" $(LINUXSRC)/include/net/xdp.h),)
  DISTRO_CFLAG += -DHAVE_XDP_SHARED_INFO_FROM_BUFF
endif
ifneq ($(shell grep -so "struct xdp_rxq_info" $(LINUXSRC)/include/net/xdp.h),)
  DISTRO_CFLAG += -DHAVE_XDP_RXQ_INFO
  ifneq ($(shell grep -A 1 "xdp_rxq_info_reg" $(LINUXSRC)/include/net/xdp.h | grep -o napi_id),)
    DISTRO_CFLAG += -DHAVE_NEW_XDP_RXQ_INFO_REG
  endif
  ifneq ($(shell grep -o "xdp_rxq_info_is_reg" $(LINUXSRC)/include/net/xdp.h),)
      DISTRO_CFLAG += -DHAVE_XDP_RXQ_INFO_IS_REG
  endif
endif
ifneq ($(shell grep -so "xdp_init_buff" $(LINUXSRC)/include/net/xdp.h),)
  DISTRO_CFLAG += -DHAVE_XDP_INIT_BUFF
endif
ifneq ($(shell grep -so "xdp_do_flush(void)" $(LINUXSRC)/include/linux/filter.h),)
  DISTRO_CFLAG += -DHAVE_XDP_DO_FLUSH
endif

ifneq ($(shell grep -so "xdp_data_hard_end" $(LINUXSRC)/include/net/xdp.h),)
  DISTRO_CFLAG += -DHAVE_XDP_FRAME_SZ
endif

ifeq ($(shell ls  $(LINUXSRC)/include/net/page_pool/helpers.h > /dev/null 2>&1 && echo 1),1)
  DISTRO_CFLAG += -DHAVE_PAGE_POOL_HELPERS_H
  DISTRO_CFLAG += -DHAVE_PAGE_POOL_PAGE_FRAG
  DISTRO_CFLAG += -DHAVE_PAGE_POOL_GET_DMA_ADDR
  ifneq ($(shell grep -s "PP_FLAG_PAGE_FRAG" $(LINUXSRC)/include/net/page_pool/types.h),)
	DISTRO_CFLAG += -DHAVE_PAGE_POOL_PP_FRAG_BIT
  endif
  ifneq ($(shell grep -s "page_pool_free_va" $(LINUXSRC)/include/net/page_pool/helpers.h),)
	DISTRO_CFLAG += -DHAVE_PAGE_POOL_FREE_VA
  endif
  ifneq ($(shell grep -so "netmem_ref" $(LINUXSRC)/include/net/netmem.h),)
    DISTRO_CFLAG += -DHAVE_NETMEM
    ifneq ($(shell grep -so "page_pool_alloc_frag_netmem" $(LINUXSRC)/include/net/page_pool/types.h),)
      DISTRO_CFLAG += -DHAVE_PAGE_POOL_NETMEM
      ifneq ($(shell grep -so "page_pool_alloc_netmems" $(LINUXSRC)/include/net/page_pool/types.h),)
        DISTRO_CFLAG += -DHAVE_PAGE_POOL_ALLOC_NETMEMS
      endif
      ifneq ($(shell grep -so "page_pool_is_unreadable" $(LINUXSRC)/include/net/page_pool/helpers.h),)
        DISTRO_CFLAG += -DHAVE_PAGE_POOL_IS_UNREADABLE
      endif
      ifneq ($(shell grep -so "netmem_is_pfmemalloc" $(LINUXSRC)/include/net/netmem.h),)
        DISTRO_CFLAG += -DHAVE_NETMEM_IS_PFMEMALLOC
      endif
      ifneq ($(shell grep -so "page_pool_dma_sync_for_cpu" $(LINUXSRC)/include/net/page_pool/helpers.h),)
        DISTRO_CFLAG += -DHAVE_PAGE_POOL_DMA_SYNC_FOR_CPU
      endif
      ifneq ($(shell grep -so "page_pool_dma_sync_netmem_for_cpu" $(LINUXSRC)/include/net/page_pool/helpers.h),)
        DISTRO_CFLAG += -DHAVE_PAGE_POOL_DMA_SYNC_NETMEM_FOR_CPU
      endif
      ifneq ($(shell grep -so "skb_frags_readable" $(LINUXSRC)/include/linux/skbuff.h),)
        DISTRO_CFLAG += -DHAVE_SKB_FRAGS_READABLE
      endif
    endif
  endif
endif

ifeq ($(shell ls  $(LINUXSRC)/include/net/page_pool.h > /dev/null 2>&1 && echo 1),1)
  ifneq ($(shell grep -so "page_pool_release_page" $(LINUXSRC)/include/net/page_pool.h),)
    DISTRO_CFLAG += -DHAVE_PAGE_POOL_RELEASE_PAGE
  endif
  ifneq ($(shell grep -so "page_pool_dev_alloc_frag" $(LINUXSRC)/include/net/page_pool.h),)
    DISTRO_CFLAG += -DHAVE_PAGE_POOL_PAGE_FRAG
    DISTRO_CFLAG += -DHAVE_PAGE_POOL_PP_FRAG_BIT
  endif
  ifneq ($(shell grep -so "page_pool_get_dma_addr" $(LINUXSRC)/include/net/page_pool.h),)
    DISTRO_CFLAG += -DHAVE_PAGE_POOL_GET_DMA_ADDR
  endif
endif

ifneq ($(shell grep -o "netdev_name" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETDEV_NAME
endif

ifneq ($(shell grep -o "netdev_update_features" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETDEV_UPDATE_FEATURES
endif

ifneq ($(shell grep -o "napi_hash_add" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NAPI_HASH_ADD
endif

ifneq ($(shell grep -o "napi_hash_del" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NAPI_HASH_DEL
endif

ifneq ($(shell grep "napi_complete_done" $(LINUXSRC)/include/linux/netdevice.h | grep -o "bool"),)
  DISTRO_CFLAG += -DHAVE_NEW_NAPI_COMPLETE_DONE
endif

ifneq ($(shell grep -o "min_mtu" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_MIN_MTU
endif

ifneq ($(shell grep -o "prog_attached" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_PROG_ATTACHED
endif

ifneq ($(shell grep -o "netdev_xmit_more" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETDEV_XMIT_MORE
endif

ifneq ($(shell grep -o "netif_xmit_stopped" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETIF_XMIT_STOPPED
endif

ifneq ($(shell grep "tx_dropped" $(LINUXSRC)/include/linux/netdevice.h | grep -o atomic_long_t),)
  DISTRO_CFLAG += -DHAVE_NETDEV_TX_DROPPED
endif

ifneq ($(shell grep "rh_tx_dropped" $(LINUXSRC)/include/linux/netdevice.h | grep -o atomic_long_t),)
  DISTRO_CFLAG += -DHAVE_NETDEV_RH_TX_DROPPED
endif

ifneq ($(shell grep "tx_dropped" $(LINUXSRC)/include/linux/netdevice.h | grep -o local_t),)
  DISTRO_CFLAG += -DHAVE_NETDEV_TX_DROPPED -DHAVE_NETDEV_TX_DROPPED_CORE_STATS
endif

ifneq ($(shell grep -A 1 "ndo_tx_timeout" $(LINUXSRC)/include/linux/netdevice.h | grep -o txqueue),)
  DISTRO_CFLAG += -DHAVE_NDO_TX_TIMEOUT_QUEUE
endif

ifneq ($(shell grep -o "udp_tunnel_nic" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_UDP_TUNNEL_NIC
endif

ifneq ($(shell grep -o "XDP_QUERY_PROG" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_XDP_QUERY_PROG
endif

ifneq ($(shell grep -o "__netif_napi_del" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETIF_NAPI_DEL_NEW
endif

OPEN_PARAN := (
ifneq ($(shell grep -A 2 "netif_napi_add${OPEN_PARAN}" $(LINUXSRC)/include/linux/netdevice.h | grep -o "int weight"),)
  DISTRO_CFLAG += -DHAVE_NETIF_NAPI_ADD_WITH_WEIGHT_ARG
endif

ifneq ($(shell grep -o "ndo_eth_ioctl" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_ETH_IOCTL
endif

ifneq ($(shell grep -o "netif_is_rxfh_configured" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETIF_IS_RXFH_CONFIGURED
endif

ifneq ($(shell grep -so "netdev_lock_dereference" $(LINUXSRC)/include/net/netdev_lock.h),)
  DISTRO_CFLAG += -DHAVE_NETDEV_LOCK
endif

ifneq ($(shell grep -o "netpoll_poll_dev" $(LINUXSRC)/include/linux/netpoll.h),)
  DISTRO_CFLAG += -DHAVE_NETPOLL_POLL_DEV
endif

ifneq ($(shell grep -o "prandom_bytes" $(LINUXSRC)/include/linux/random.h),)
  DISTRO_CFLAG += -DHAVE_PRANDOM_BYTES
endif

ifneq ($(shell grep -o "tcp_v6_check" $(LINUXSRC)/include/net/ip6_checksum.h),)
  DISTRO_CFLAG += -DHAVE_TCP_V6_CHECK
endif

ifneq ($(shell grep -o "skb_tcp_all_headers" $(LINUXSRC)/include/linux/tcp.h),)
  DISTRO_CFLAG += -DHAVE_SKB_TCP_ALL_HEADERS
endif

ifneq ($(shell grep -o "usleep_range" $(LINUXSRC)/include/linux/delay.h),)
  DISTRO_CFLAG += -DHAVE_USLEEP_RANGE
endif

ifneq ($(shell grep -o "vzalloc" $(LINUXSRC)/include/linux/vmalloc.h),)
  DISTRO_CFLAG += -DHAVE_VZALLOC
endif

ifneq ($(shell grep -o "kmalloc_array" $(LINUXSRC)/include/linux/slab.h),)
  DISTRO_CFLAG += -DHAVE_KMALLOC_ARRAY
endif

ifneq ($(shell grep -o "pcie_capability_read_word" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCIE_CAPABILITY_READ_WORD
endif

ifneq ($(shell grep -o "pcie_link_width" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCI_LINK_WIDTH
endif

ifneq ($(shell grep -o "PCIE_SPEED_2_5GT" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCIE_BUS_SPEED
endif

ifneq ($(shell grep -o "pci_is_bridge" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCI_IS_BRIDGE
endif

ifneq ($(shell grep -o "pci_upstream_bridge" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCI_UPSTREAM_BRIDGE
endif

ifneq ($(shell grep -o "pcie_print_link_status" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCI_PRINT_LINK_STATUS
endif

ifneq ($(shell grep -o "_genl_register_family_with_ops_grps" $(LINUXSRC)/include/net/genetlink.h),)
  DISTRO_CFLAG += -DHAVE_GENL_REG_OPS_GRPS
endif

ifneq ($(shell grep -o "pci_physfn" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCI_PHYSFN
endif

ifneq ($(shell grep -o "pcie_flr" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCIE_FLR
endif

ifneq ($(shell grep -o "pci_get_dsn" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCI_GET_DSN
endif

ifneq ($(shell grep "pci_vpd_find_tag" $(LINUXSRC)/include/linux/pci.h | grep "unsigned int off"),)
  DISTRO_CFLAG += -DHAVE_OLD_VPD_FIND_TAG
endif

ifneq ($(shell grep -o "pci_vpd_alloc" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCI_VPD_ALLOC
endif

ifneq ($(shell grep -o "eth_type_vlan" $(LINUXSRC)/include/linux/if_vlan.h),)
  DISTRO_CFLAG += -DHAVE_ETH_TYPE_VLAN
endif

ifneq ($(shell ls $(LINUXSRC)/include/$(UAPI)/linux/net_tstamp.h > /dev/null 2>&1 && echo net_tstamp),)
  ifneq ($(shell ls $(LINUXSRC)/include/linux/timecounter.h > /dev/null 2>&1 && echo timecounter),)
    ifneq ($(shell ls $(LINUXSRC)/include/linux/timekeeping.h > /dev/null 2>&1 && echo timekeeping),)
      ifneq ($(shell grep -o "HWTSTAMP_FILTER_PTP_V2_EVENT" $(LINUXSRC)/include/$(UAPI)/linux/net_tstamp.h),)
        DISTRO_CFLAG += -DHAVE_IEEE1588_SUPPORT
        ifneq ($(shell grep -o "HWTSTAMP_FLAG_BONDED_PHC_INDEX" $(LINUXSRC)/include/$(UAPI)/linux/net_tstamp.h),)
          DISTRO_CFLAG += -DHAVE_HWTSTAMP_FLAG_BONDED_PHC_INDEX
        endif
      endif
    endif
  endif
endif

ifneq ($(shell grep -so "PTP_CLASS_V2" $(LINUXSRC)/include/linux/ptp_classify.h),)
  DISTRO_CFLAG += -DHAVE_PTP_CLASSES
endif

ifneq ($(shell grep -so "ptp_header" $(LINUXSRC)/include/linux/ptp_classify.h),)
  DISTRO_CFLAG += -DHAVE_PTP_HEADER
endif

ifneq ($(shell grep -so "ptp_classify_raw" $(LINUXSRC)/include/linux/ptp_classify.h),)
  DISTRO_CFLAG += -DHAVE_PTP_CLASSIFY_RAW
endif

ifneq ($(shell grep -so "ptp_parse_header" $(LINUXSRC)/include/linux/ptp_classify.h),)
  DISTRO_CFLAG += -DHAVE_PTP_PARSE_HEADER
endif

ifneq ($(shell grep -so "ptp_system_timestamp" $(LINUXSRC)/include/linux/ptp_clock_kernel.h),)
  DISTRO_CFLAG += -DHAVE_PTP_SYS_TIMESTAMP
endif

ifneq ($(shell grep -so "adjphase" $(LINUXSRC)/include/linux/ptp_clock_kernel.h),)
  DISTRO_CFLAG += -DHAVE_PTP_ADJPHASE
endif

ifneq ($(shell grep -so "do_aux_work" $(LINUXSRC)/include/linux/ptp_clock_kernel.h),)
  DISTRO_CFLAG += -DHAVE_PTP_DO_AUX_WORK
  ifneq ($(shell grep -so "ptp_cancel_worker_sync" $(LINUXSRC)/include/linux/ptp_clock_kernel.h),)
    DISTRO_CFLAG += -DHAVE_PTP_CANCEL_WORKER_SYNC
  endif
endif

ifneq ($(shell grep -so "adjust_by_scaled_ppm" $(LINUXSRC)/include/linux/ptp_clock_kernel.h),)
  DISTRO_CFLAG += -DHAVE_SCALED_PPM
endif

ifneq ($(shell grep -so "*gettimex64" $(LINUXSRC)/include/linux/ptp_clock_kernel.h),)
  DISTRO_CFLAG += -DHAVE_PTP_GETTIMEX64
else
  ifneq ($(shell grep -so "timespec64" $(LINUXSRC)/include/linux/time64.h),)
    DISTRO_CFLAG += -DHAVE_TIMESPEC64
  else
    ifneq ($(shell grep -so "timespec64" $(LINUXSRC)/include/linux/time.h),)
      DISTRO_CFLAG += -DHAVE_TIMESPEC64
    endif
  endif
endif

ifneq ($(shell grep -so "convert_art_ns_to_tsc" $(LINUXSRC)/arch/x86/include/asm/tsc.h),)
  DISTRO_CFLAG += -DHAVE_ARTNS_TO_TSC
endif

ifneq ($(shell grep -o "time64_to_tm" $(LINUXSRC)/include/linux/time.h),)
  DISTRO_CFLAG += -DHAVE_TIME64
endif

ifneq ($(shell grep -o "timer_setup" $(LINUXSRC)/include/linux/timer.h),)
  DISTRO_CFLAG += -DHAVE_TIMER_SETUP
endif

ifneq ($(shell grep -o "timer_delete_sync" $(LINUXSRC)/include/linux/timer.h),)
  DISTRO_CFLAG += -DHAVE_TIMER_DELETE_SYNC
endif

ifneq ($(shell grep -s "devlink_ops" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK
  ifeq ($(shell grep -o "devlink_register(struct devlink \*devlink);" $(LINUXSRC)/include/net/devlink.h),)
    DISTRO_CFLAG += -DHAVE_DEVLINK_REGISTER_DEV
  endif
endif

ifneq ($(shell grep -s "devlink_param" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_PARAM
  ifneq ($(shell grep -s -A 2 "int (\*validate)" $(LINUXSRC)/include/net/devlink.h | grep "struct netlink_ext_ack \*extack"),)
    DISTRO_CFLAG += -DHAVE_DEVLINK_VALIDATE_NEW
  endif
  ifneq ($(shell grep -A 1 "struct devlink_param_gset_ctx \*ctx," $(LINUXSRC)/include/net/devlink.h | grep -o "struct netlink_ext_ack \*extack"),)
    DISTRO_CFLAG += -DHAVE_DEVLINK_PARAM_SET_EXTACK
  endif
endif

ifneq ($(shell grep -s -A 7 "devlink_port_attrs" $(LINUXSRC)/include/net/devlink.h | grep -o "netdev_phys_item_id"),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_PORT_ATTRS
endif

ifneq ($(shell grep -s -A 1 "devlink_port_attrs_set" $(LINUXSRC)/include/net/devlink.h | grep -o "struct devlink_port_attrs"),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_PORT_ATTRS_SET_NEW
endif

ifneq ($(shell grep -w "ndo_get_devlink_port" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NDO_DEVLINK_PORT
endif

ifneq ($(shell grep -s "devlink_params_publish" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_PARAM_PUBLISH
endif

ifneq ($(shell grep -s "enable_roce" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_PARAM_ENABLE_ROCE
endif

ifneq ($(shell grep -s -A 1 "eswitch_mode_set" $(LINUXSRC)/include/net/devlink.h | grep -o "netlink_ext_ack"),)
  DISTRO_CFLAG += -DHAVE_ESWITCH_MODE_SET_EXTACK
endif

ifneq ($(shell grep -so "DEVLINK_PARAM_GENERIC_ID_IGNORE_ARI" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_IGNORE_ARI
endif

ifneq ($(shell grep -so "info_get" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_INFO
  ifneq ($(shell grep -so "devlink_info_board_serial_number_put" $(LINUXSRC)/include/net/devlink.h),)
    DISTRO_CFLAG += -DHAVE_DEVLINK_INFO_BSN_PUT
  endif
endif

ifneq ($(shell grep -so "flash_update" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_FLASH_UPDATE
endif

ifneq ($(shell grep -so "devlink_flash_update_status_notify" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_FLASH_UPDATE_STATUS
endif

ifneq ($(shell grep -so "devlink_flash_update_begin_notify" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_FLASH_UPDATE_BEGIN
endif

ifneq ($(shell grep -so "devlink_flash_update_params" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_FLASH_PARAMS
  ifneq ($(shell grep -s -A 1 "devlink_flash_update_params" $(LINUXSRC)/include/net/devlink.h | grep "struct firmware"),)
    DISTRO_CFLAG += -DHAVE_DEVLINK_FLASH_PARAMS_NEW
  endif
endif

ifneq ($(shell grep -so "struct devlink_health_reporter" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_HEALTH_REPORT
endif

ifneq ($(shell grep -so "devlink_health_reporter_state_update" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_HEALTH_REPORTER_STATE_UPDATE
endif

ifneq ($(shell grep -s -A 1 "(*recover)" $(LINUXSRC)/include/net/devlink.h | grep netlink_ext_ack),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_HEALTH_REPORT_EXTACK
endif

ifneq ($(shell grep -so "devlink_health_reporter_recovery_done" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_HEALTH_REPORTER_RECOVERY_DONE
endif

ifneq ($(shell grep -s -A 2 "devlink_health_reporter_create" $(LINUXSRC)/include/net/devlink.h | grep auto_recover),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_HEALTH_AUTO_RECOVER
endif

ifneq ($(shell grep -so "reload_actions" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_RELOAD_ACTION
endif

ifneq ($(shell grep -so "devlink_reload_disable" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_RELOAD_DISABLE
endif

ifneq ($(shell grep -so "devlink_set_features" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_SET_FEATURES
endif

ifneq ($(shell grep -so "*selftest_check" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_SELFTESTS_FEATURES
endif

ifneq ($(shell grep -so "DEVLINK_PARAM_GENERIC_ID_ENABLE_REMOTE_DEV_RESET" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_REMOTE_DEV_RESET
endif

ifneq ($(shell grep -so "devlink_info_driver_name_put" $(LINUXSRC)/include/net/devlink.h),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_INFO_DRIVER_NAME
endif

ifneq ($(shell grep -A 1 "devlink_fmsg_string_pair_put" $(LINUXSRC)/include/net/devlink.h | grep -o "void"),)
  DISTRO_CFLAG += -DHAVE_DEVLINK_FMSG_STRING_PAIR_PUT_VOID
endif

# Check if the file exists or not
ifneq ($(shell grep -s "switchdev_ops" $(LINUXSRC)/include/net/switchdev.h),)
  DISTRO_CFLAG += -DHAVE_SWITCHDEV
endif

ifneq ($(shell grep -s "METADATA_HW_PORT_MUX" $(LINUXSRC)/include/net/dst_metadata.h),)
  DISTRO_CFLAG += -DHAVE_METADATA_HW_PORT_MUX
endif

ifneq ($(shell grep -so "(*ieee_delapp)" $(LINUXSRC)/include/net/dcbnl.h),)
  DISTRO_CFLAG += -DHAVE_IEEE_DELAPP
endif

ifneq ($(shell grep -so "dcb_ieee_getapp_prio_dscp_mask_map" $(LINUXSRC)/include/net/dcbnl.h),)
  DISTRO_CFLAG += -DHAVE_DSCP_MASK_MAP
endif

ifneq ($(shell grep -o cpumask_local_spread $(LINUXSRC)/include/linux/cpumask.h),)
  DISTRO_CFLAG += -DHAVE_CPUMASK_LOCAL_SPREAD
endif

ifneq ($(shell grep -o cpumask_set_cpu_local_first $(LINUXSRC)/include/linux/cpumask.h),)
  DISTRO_CFLAG += -DHAVE_CPUMASK_LOCAL_FIRST
endif

ifeq ($(shell grep -so "ETH_RESET_AP" $(LINUXSRC)/include/$(UAPI)/linux/ethtool.h),)
  DISTRO_CFLAG += -DNO_ETH_RESET_AP
endif

ifneq ($(shell grep -o "get_pause_stats" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_GET_PAUSE_STATS
endif

ifneq ($(shell grep -so "ETH_TEST_FL_EXTERNAL_LB" $(LINUXSRC)/include/$(UAPI)/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETH_TEST_FL_EXTERNAL_LB
endif

ifneq ($(shell ls $(LINUXSRC)/include/linux/dim.h > /dev/null 2>&1 && echo dim),)
  DISTRO_CFLAG += -DHAVE_DIM
  ifneq ($(shell grep -A 1 "net_dim" $(LINUXSRC)/include/linux/dim.h | grep "const struct dim_sample *"),)
    DISTRO_CFLAG += -DHAVE_DIM_SAMPLE_PTR
  endif
endif

ifneq ($(shell grep -o simple_open $(LINUXSRC)/include/linux/fs.h),)
  DISTRO_CFLAG += -DHAVE_SIMPLE_OPEN
endif

ifneq ($(shell grep -o hwmon_device_register_with_info $(LINUXSRC)/include/linux/hwmon.h),)
  DISTRO_CFLAG += -DHAVE_NEW_HWMON_API
endif

ifneq ($(shell grep -o ETH_RESET_CRASHDUMP $(LINUXSRC)/include/uapi/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_RESET_CRASHDUMP
endif

ifneq ($(shell grep -o "struct ethtool_link_ksettings" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_LINK_KSETTINGS
endif

ifneq ($(shell grep -o "struct ethtool_ts_stats" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_PTP_STATS
endif

ifneq ($(shell grep -o "get_module_eeprom_by_page" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_MODULE_EEPROM_BY_PAGE
  ifneq ($(shell grep -o "set_module_eeprom_by_page" $(LINUXSRC)/include/linux/ethtool.h),)
    DISTRO_CFLAG += -DHAVE_SET_MODULE_EEPROM_BY_PAGE
endif
endif

ifneq ($(shell grep -o "struct ethtool_rxfh_param" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_RXFH_PARAM
endif

ifneq ($(shell grep -o "FLOW_RSS" $(LINUXSRC)/include/uapi/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETH_RXNFC_RSSCTX
endif

ifneq (,$(shell \
  sed -n '/struct ethtool_rxfh_param\s*{/,/};/p' \
      $(LINUXSRC)/include/linux/ethtool.h \
    | grep rss_context))
  DISTRO_CFLAG += -DHAVE_ETHTOOL_RSS_CONTEXT
endif

ifneq ($(shell grep -o "\*get_rxfh_context" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETH_RXFH_CONTEXT_ALLOC
endif

ifneq ($(shell grep -o "create_rxfh_context" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_NEW_RSSCTX_INTERFACE
  ifneq ($(shell grep -o "cap_rss_ctx_supported" $(LINUXSRC)/include/linux/ethtool.h),)
    DISTRO_CFLAG += -DHAVE_CAP_RSS_CTX_SUPPORTED
  endif
endif

ifneq ($(shell grep -o "strscpy" $(LINUXSRC)/include/linux/string.h),)
  DISTRO_CFLAG += -DHAVE_STRSCPY
  ifneq ($(shell grep -o "__must_check strscpy" $(LINUXSRC)/include/linux/string.h),)
    DISTRO_CFLAG += -DHAVE_OLD_STRSCPY
  endif
endif

ifneq ($(shell grep -so "lo_hi_writeq" $(LINUXSRC)/include/linux/io-64-nonatomic-lo-hi.h),)
  DISTRO_CFLAG += -DHAVE_LO_HI_WRITEQ
endif

ifneq ($(shell grep -so "static_key_initialized" $(LINUXSRC)/include/linux/jump_label.h),)
  DISTRO_CFLAG += -DHAVE_STATIC_KEY_INITIALIZED
  ifneq ($(shell grep -so "DEFINE_STATIC_KEY_FALSE" $(LINUXSRC)/include/linux/jump_label.h),)
    DISTRO_CFLAG += -DHAVE_DEFINE_STATIC_KEY
  endif
endif

ifneq ($(shell grep -so "DECLARE_STATIC_KEY_FALSE" $(LINUXSRC)/include/linux/jump_label.h),)
  DISTRO_CFLAG += -DHAVE_DECLARE_STATIC_KEY
endif

ifneq ($(shell $(BNXT_SRC)/find_src.awk -v struct=ethtool_link_ksettings pattern=lanes $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_LANES
endif

ifneq ($(shell $(BNXT_SRC)/find_src.awk -v struct=ethtool_link_ksettings pattern=ethtool_link_mode_bit_indices $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_LINK_MODE
endif

ifneq ($(shell grep -o ethtool_params_from_link_mode $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_PARAMS_FROM_LINK_MODE
endif

ifneq ($(shell grep -o "^struct ethtool_keee" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_KEEE
endif

ifneq ($(shell grep -so ETHTOOL_A_LINKSTATE_EXT_DOWN_CNT $(LINUXSRC)/include/uapi/linux/ethtool_netlink.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_A_LINKSTATE_EXT_DOWN_CNT
endif

ifneq ($(shell grep -so ETHTOOL_A_LINKSTATE_EXT_STATE $(LINUXSRC)/include/uapi/linux/ethtool_netlink*.h),)
  ifneq ($(shell grep -so ETHTOOL_LINK_EXT_STATE_MODULE $(LINUXSRC)/include/uapi/linux/ethtool.h),)
    DISTRO_CFLAG += -DHAVE_ETHTOOL_A_LINKSTATE_EXT_STATE
  endif
endif

ifneq ($(shell grep -so ETHTOOL_A_LINKMODES_LANES $(LINUXSRC)/include/uapi/linux/ethtool_netlink.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_A_LINKMODES_LANES
endif

ifneq ($(shell grep -o .get_per_queue_coalesce $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_GET_PER_QUEUE_COAL
endif

ifneq ($(shell grep -so synchronize_rcu_bh $(LINUXSRC)/include/linux/rcutree.h),)
  DISTRO_CFLAG += -DHAVE_LEGACY_RCU_BH
endif

ifneq ($(shell grep -o "ALIGN" $(LINUXSRC)/include/linux/kernel.h),)
DISTRO_CFLAG +=
else
DISTRO_CFLAG += -DHAVE_ALIGN
endif

ifneq ($(shell grep -so "struct netlink_ext_ack" $(LINUXSRC)/include/linux/netlink.h),)
  DISTRO_CFLAG += -DHAVE_NETLINK_EXT_ACK
endif

ifneq ($(shell grep -so "ida_alloc" $(LINUXSRC)/include/linux/idr.h),)
  DISTRO_CFLAG += -DHAVE_IDA_ALLOC
endif

ifneq ($(shell grep -o "struct auxiliary_device_id" $(LINUXSRC)/include/linux/mod_devicetable.h),)
  DISTRO_CFLAG += -DHAVE_AUX_DEVICE_ID
endif

ifneq ($(shell ls $(LINUXSRC)/include/linux/fwctl.h > /dev/null 2>&1 && echo fwctl),)
  DISTRO_CFLAG += -DHAVE_FWCTL_DRIVER
endif

ifneq ($(shell grep -s -A1 -w "devnode" $(LINUXSRC)/include/linux/device/class.h \
              | grep -q "const struct device \*dev"; echo $$?),1)
  DISTRO_CFLAG += -DHAVE_DEVNODE_CONST
endif

ifneq ($(shell ls $(LINUXSRC)/include/linux/cleanup.h > /dev/null 2>&1 && echo cleanup),)
  DISTRO_CFLAG += -DHAVE_FWCTL_CLEANUP
endif

ifdef CONFIG_AUXILIARY_BUS
ifneq ($(shell ls $(LINUXSRC)/include/linux/auxiliary_bus.h > /dev/null 2>&1 && echo auxiliary_driver),)
  DISTRO_CFLAG += -DHAVE_AUXILIARY_DRIVER
endif

ifneq ($(shell grep -so "auxiliary_get_drvdata" $(LINUXSRC)/include/linux/auxiliary_bus.h),)
  DISTRO_CFLAG += -DHAVE_AUX_GET_DRVDATA
endif
endif

ifneq ($(shell grep -so "tls_driver_ctx" $(LINUXSRC)/include/net/tls.h),)
  ifneq ($(shell grep -so "tls_is_sk_rx_device_offloaded" $(LINUXSRC)/include/net/tls.h),)
    ifneq ($(shell grep -so "TLS_DRIVER_STATE_SIZE_RX" $(LINUXSRC)/include/net/tls.h),)
      DISTRO_CFLAG += -DHAVE_KTLS
    endif
  endif
endif

ifneq ($(shell grep -so "hwmon_notify_event" $(LINUXSRC)/include/linux/hwmon.h),)
  DISTRO_CFLAG += -DHAVE_HWMON_NOTIFY_EVENT
endif

ifneq ($(shell grep -so "vm_flags_set" $(LINUXSRC)/include/linux/mm.h),)
  DISTRO_CFLAG += -DHAVE_VM_FLAGS_SET
endif

ifneq ($(shell grep -so "sysfs_emit" $(LINUXSRC)/include/linux/sysfs.h),)
  DISTRO_CFLAG += -DHAVE_SYSFS_EMIT
endif

ifneq ($(shell grep -so "default_groups" $(LINUXSRC)/include/linux/kobject.h),)
  DISTRO_CFLAG += -DHAVE_KOBJ_DEFAULT_GROUPS
endif

ifneq ($(shell grep "unsigned long long" $(LINUXSRC)/include/net/flow_dissector.h | grep used_keys),)
  DISTRO_CFLAG += -DHAVE_FLOW_USED_KEY_SIZE_LONG_LONG
endif

ifneq ($(shell grep -so "pci_enable_pcie_error_reporting" $(LINUXSRC)/include/linux/aer.h),)
  DISTRO_CFLAG += -DHAVE_PCIE_ERROR_REPORTING
endif

ifneq ($(shell grep -so "tls_is_skb_tx_device_offloaded" $(LINUXSRC)/include/net/tls.h),)
  DISTRO_CFLAG += -DHAVE_TLS_IS_SKB_TX_DEVICE_OFFLOADED
endif

ifneq ($(shell grep -so "*napi" $(LINUXSRC)/include/net/page_pool.h),)
  DISTRO_CFLAG += -DHAVE_PAGE_POOL_NAPI_MAPPING
endif

ifneq ($(shell grep -so "*netdev" $(LINUXSRC)/include/net/page_pool/types.h),)
  DISTRO_CFLAG += -DHAVE_PAGE_POOL_NETDEV_MAPPING
endif

ifdef CONFIG_XDP_SOCKETS
ifneq ($(shell grep -so "xsk_pool_dma_map" $(LINUXSRC)/include/net/xdp_sock_drv.h),)
  DISTRO_CFLAG += -DHAVE_XSK_SUPPORT
ifneq ($(shell grep "xsk_buff_dma_sync_for_cpu" $(LINUXSRC)/include/net/xdp_sock_drv.h | grep "struct xsk_buff_pool"),)
  DISTRO_CFLAG += -DHAVE_OLD_XSK_BUFF_DMA_SYNC
endif
endif
endif

ifdef CONFIG_PCIE_TPH
ifneq ($(shell grep -so "pcie_tph_set_st_entry" $(LINUXSRC)/include/linux/pci-tph.h),)
  DISTRO_CFLAG += -DHAVE_PCIE_TPH_SET_ST
endif
endif

# Valid values for wc_slices: 1, 2, 4
DISTRO_CFLAG += -DTF_TCAM_WC_SLICES=2

ifeq ($(BNXT_INC),)
  BNXT_INC:=$(shell pwd)/
  export BNXT_INC
endif

ifeq ($(TFCORE_INC),)
  TFCORE_INC:=$(shell pwd)/tf_core
  export TFCORE_INC
endif

ifeq ($(TFC_V3_INC),)
  TFC_V3_INC:=$(shell pwd)/tfc_v3
  export TFC_V3_INC
endif

ifeq ($(HCAPI_INC),)
  HCAPI_INC:=$(shell pwd)/hcapi
  export HCAPI_INC
endif

ifeq ($(HCAPI_CFA_INC),)
  HCAPI_CFA_INC:=$(shell pwd)/hcapi/cfa
  export HCAPI_CFA_INC
endif

ifeq ($(CFA_V3_INC),)
  CFA_V3_INC:=$(shell pwd)/hcapi/cfa_v3/include
  export CFA_V3_INC
endif

ifeq ($(CFA_V3_BLD_INC),)
  CFA_V3_BLD_INC:=$(shell pwd)/hcapi/cfa_v3/mpc/include
  export CFA_V3_MPC_INC
endif

ifeq ($(CFA_V3_MM_INC),)
  CFA_V3_MM_INC:=$(shell pwd)/hcapi/cfa_v3/mm/include
  export CFA_V3_MM_INC
endif

ifeq ($(CFA_V3_TIM_INC),)
  CFA_V3_TIM_INC:=$(shell pwd)/hcapi/cfa_v3/tim/include
  export CFA_V3_TIM_INC
endif

ifeq ($(CFA_V3_TPM_INC),)
  CFA_V3_TPM_INC:=$(shell pwd)/hcapi/cfa_v3/tpm/include
  export CFA_V3_TPM_INC
endif

ifeq ($(GENERIC_TEMPLATES_INC),)
  GENERIC_TEMPLATES_INC:=$(shell pwd)/tf_ulp/generic_templates
  export GENERIC_TEMPLATES_INC
endif

ifeq ($(TFULP_INC),)
  TFULP_INC:=$(shell pwd)/tf_ulp
  export TFULP_INC
endif

DISTRO_CFLAG += -DBNXT_TF_LINUX

ifneq ($(shell grep -so synchronize_rcu_bh $(LINUXSRC)/include/linux/rcutree.h),)
  DISTRO_CFLAG += -DHAVE_LEGACY_RCU_BH
endif

ifeq ($(custom_flow_offload),1)
  DISTRO_CFLAG += -DCONFIG_BNXT_CUSTOM_FLOWER_OFFLOAD
endif

ifeq ($(enable_truflow),1)
  DISTRO_CFLAG += -DBNXT_ENABLE_TRUFLOW
endif

# Set feature bits to enable
# BNXT_ULP_FEATURE_BIT_PARENT_DMAC = 1
# BNXT_ULP_FEATURE_BIT_PORT_DMAC = 2
# BNXT_ULP_FEATURE_BIT_MULTI_TUNNEL_FLOW = 4
# BNXT_ULP_FEATURE_BIT_SOCKET_DIRECT = 8
# For example, to enable Multi_tunnel_flow and port dmac,
# set feature_bits=6
# Use make -j feature_bits=val to compile with these options

ifeq ($(feature_bits),)
  DISTRO_CFLAG += -DTC_BNXT_TF_FEAT_BITS=0
else
  DISTRO_CFLAG += -DTC_BNXT_TF_FEAT_BITS=$(feature_bits)
endif

ifeq ($(bnxt_quic),1)
  KCONFIG_TLS := $(shell grep -E '^CONFIG_TLS=y|^CONFIG_TLS=m' $(LINUXSRC)/.config 2>/dev/null || true)
  ifeq ($(KCONFIG_TLS),)
    $(error QUIC cannot be supported without CONFIG_TLS=y or CONFIG_TLS=m in the kernel)
  endif
  DISTRO_CFLAG += -DHAVE_BNXT_QUIC
endif

override ccflags-y += ${DISTRO_CFLAG} -g -Werror -Wno-error=unused-variable -Wno-error=unused-function -DCHIMP_FW -D__LINUX -DCONFIG_BNXT_SRIOV -DCONFIG_BNXT_DCB -DHSI_DBG_DISABLE -DCONFIG_BNXT_LFC -DSUPPORT_CFA_HW_ALL=1 -DSUPPORT_CFA_EM_FOR_TC=0 -I$(TFCORE_INC)  -I$(TFC_V3_INC) -I$(BNXT_INC) -I$(CFA_V3_INC) -I$(CFA_V3_MPC_INC) -I$(CFA_V3_MM_INC) -I$(CFA_V3_TPM_INC) -I$(CFA_V3_TIM_INC) -I$(HCAPI_INC) -I$(HCAPI_CFA_INC) -I$(GENERIC_TEMPLATES_INC) -I$(TFULP_INC)

ifneq ($(OFED_VERSION), )
override ccflags-y += -I$(OFA_INC)
autoconf_h = $(shell /bin/ls -1 $(OFA_KERNEL)/include/*/autoconf.h 2> /dev/null | head -1)
ifneq ($(autoconf_h),)
override ccflags-y += -include $(autoconf_h)
endif
endif

override ccflags-y += -I$(src)/
override ccflags-y += -I$(src)/tf_core/
override ccflags-y += -I$(src)/tfc_v3/
override ccflags-y += -I$(src)/hcapi/
override ccflags-y += -I$(src)/hcapi/cfa/
override ccflags-y += -I$(src)/hcapi/cfa_v3/include
override ccflags-y += -I$(src)/hcapi/cfa_v3/mm
override ccflags-y += -I$(src)/hcapi/cfa_v3/mm/include
override ccflags-y += -I$(src)/hcapi/cfa_v3/mpc
override ccflags-y += -I$(src)/hcapi/cfa_v3/mpc/include/
override ccflags-y += -I$(src)/hcapi/cfa_v3/tim
override ccflags-y += -I$(src)/hcapi/cfa_v3/tim/include
override ccflags-y += -I$(src)/hcapi/cfa_v3/tpm
override ccflags-y += -I$(src)/hcapi/cfa_v3/tpm/include
override ccflags-y += -I$(src)/tf_ulp/
override ccflags-y += -I$(src)/tf_ulp/generic_templates/

define cols
	$$(awk '{ if (m < NF) m=NF; } END { print m; }' $1 2>/dev/null || echo 0)
endef

define bnxt_ver
	BNXT_EXTRA_VER_STATUS=$$(git status -s bnxt_extra_ver.h 2>/dev/null);  \
	BNXT_EXTERNAL=$$?;						       \
	if [ $$BNXT_EXTERNAL -eq 0 ] && [ -z "$$BNXT_EXTRA_VER_STATUS" ]; then \
		BNXT_GIT_VER="-$$(git rev-parse --abbrev-ref HEAD)";	       \
		BNXT_GIT_VER="$${BNXT_GIT_VER}-$$(			       \
					GIT_CONFIG_COUNT=1		       \
					GIT_CONFIG_KEY_0=core.abbrev	       \
					GIT_CONFIG_VALUE_0=8		       \
					git rev-parse --short HEAD	       \
				)";					       \
		if [ -n "$$(git diff --name-only)" ]; then		       \
			BNXT_GIT_VER="$${BNXT_GIT_VER}+";		       \
		fi;							       \
	fi;								       \
	BNXT_VER=$$(sed -n -E "s/(.*DRV_MODULE_VERSION\s+\")(.*)(\".*)/\2/p"   \
			bnxt.h);					       \
	if [ -n "$$BNXT_GIT_VER" ]; then				       \
		BNXT_EXTRA_VER="$${BNXT_GIT_VER}";			       \
		BNXT_VER="$${BNXT_VER}$${BNXT_EXTRA_VER}";		       \
	else								       \
		BNXT_SRC_DELTA=0;					       \
		if [ -x "$$(which sha512sum 2>/dev/null)" ] &&		       \
		   [ $(call cols, MANIFEST) -eq 2 ]; then		       \
			$$(sha512sum -c MANIFEST 2>/dev/null >/dev/null);      \
			BNXT_SRC_DELTA=$$?;				       \
		fi;							       \
		BNXT_SRC_EXTRA_VER=$$(sed -n -E				       \
			"s/(.*DRV_MODULE_EXTRA_VER\s+\")(.*)(\")/\2/p"	       \
			bnxt_extra_ver.h);				       \
		if [ $$BNXT_SRC_DELTA -eq 0 ]; then			       \
			BNXT_VER="$${BNXT_VER}$${BNXT_SRC_EXTRA_VER}";	       \
		else							       \
			BNXT_EXTRA_VER="$${BNXT_SRC_EXTRA_VER}+";	       \
			touch .BNXT_SRC_DELTA.modified;	       \
			BNXT_VER="$${BNXT_VER}$${BNXT_EXTRA_VER}";	       \
		fi;							       \
	fi
endef

BCM_DRV = bnxt_en.ko
ifneq ($(KERNELRELEASE),)

ifneq ($(BNXT_EXTRA_VER),)
override EXTRA_CFLAGS += -DDRV_MODULE_EXTRA_VER=\"$(BNXT_EXTRA_VER)\"
endif

ifeq ($(shell expr $(KVER_MAJ) \>= 5), 1)
        BNXT_DBGFS_OBJ = bnxt_debugfs.o
else
        BNXT_DBGFS_OBJ = bnxt_debugfs_cpt.o
endif

TF_CORE_OBJ = tf_core/tf_msg.o tf_core/tf_util.o tf_core/tf_session.o tf_core/tf_rm.o tf_core/tf_tcam.o tf_core/tf_tbl.o tf_core/tf_identifier.o tf_core/dpool.o tf_core/tf_em_internal.o tf_core/tf_em_hash_internal.o tf_core/tf_if_tbl.o tf_core/tf_global_cfg.o tf_core/tf_sram_mgr.o tf_core/tf_tbl_sram.o tf_core/rand.o hcapi/cfa/hcapi_cfa_p4.o hcapi/cfa/hcapi_cfa_p58.o tf_core/tf_device_p4.o tf_core/tf_device_p58.o tf_core/tf_device.o tf_core/tf_core.o tf_core/tf_tcam_mgr_msg.o tf_core/cfa_tcam_mgr_hwop_msg.o tf_core/cfa_tcam_mgr.o tf_core/cfa_tcam_mgr_p4.o tf_core/cfa_tcam_mgr_p58.o

HCAPI_OBJ = hcapi/bitalloc.o

TFC_V3_OBJ = tfc_v3/tfc_act.o tfc_v3/tfc_cpm.o tfc_v3/tfc_em.o tfc_v3/tfc_global_id.o tfc_v3/tfc_ident.o tfc_v3/tfc_idx_tbl.o tfc_v3/tfc_init.o tfc_v3/tfc_msg.o tfc_v3/tfc_priv.o tfc_v3/tfc_session.o tfc_v3/tfc_tbl_scope.o tfc_v3/tfc_tcam.o tfc_v3/tfc_tcam_debug.o tfc_v3/tfc_util.o tfc_v3/tfo.o tfc_v3/tfc_vf2pf_msg.o tfc_v3/tfc_if_tbl.o tfc_v3/tfc_mpc_table.o

CFA_V3_OBJ = hcapi/cfa_v3/mm/cfa_mm.o hcapi/cfa_v3/mpc/cfa_bld_p70_mpc.o hcapi/cfa_v3/mpc/cfa_bld_mpc.o hcapi/cfa_v3/mpc/cfa_bld_p70_mpcops.o hcapi/cfa_v3/mpc/cfa_bld_p70_host_mpc_wrapper.o hcapi/cfa_v3/tpm/cfa_tpm.o hcapi/cfa_v3/tim/cfa_tim.o
ifeq ($(custom_flow_offload),1)
  TF_ULP_OBJ = tf_ulp/ulp_utils.o tf_ulp/ulp_template_debug.o tf_ulp/ulp_tf_debug.o tf_ulp/bnxt_tf_ulp.o tf_ulp/bnxt_tf_ulp_p5.o tf_ulp/bnxt_tf_ulp_p7.o tf_ulp/ulp_fc_mgr.o tf_ulp/ulp_fc_mgr_p5.o tf_ulp/ulp_fc_mgr_p7.o tf_ulp/ulp_flow_db.o tf_ulp/ulp_gen_tbl.o tf_ulp/ulp_mapper.o tf_ulp/ulp_mapper_p5.o tf_ulp/ulp_mapper_p7.o tf_ulp/ulp_mark_mgr.o tf_ulp/ulp_port_db.o tf_ulp/ulp_matcher.o tf_ulp/ulp_def_rules.o tf_ulp/bnxt_ulp_linux_flow.o tf_ulp/ulp_tc_parser.o tf_ulp/ulp_tc_handler_tbl.o tf_ulp/ulp_tc_custom_offload.o tf_ulp/bnxt_tf_tc_shim.o tf_ulp/ulp_tc_rte_flow_gen.o tf_ulp/ulp_alloc_tbl.o
  TF_ULP_OBJ += tf_ulp/ulp_udcc.o tf_ulp/ulp_nic_flow.o tf_ulp/ulp_generic_flow_offload.o tf_ulp/bnxt_tf_tc_shim.o tf_ulp/bnxt_ulp_meter.o tf_ulp/ulp_sc_mgr.o tf_ulp/ulp_sc_mgr_tfc.o
else
  TF_ULP_OBJ = tf_ulp/ulp_utils.o tf_ulp/ulp_template_debug.o tf_ulp/ulp_tf_debug.o tf_ulp/bnxt_tf_ulp.o tf_ulp/bnxt_tf_ulp_p5.o tf_ulp/bnxt_tf_ulp_p7.o tf_ulp/ulp_fc_mgr.o tf_ulp/ulp_fc_mgr_p5.o tf_ulp/ulp_fc_mgr_p7.o tf_ulp/ulp_flow_db.o tf_ulp/ulp_gen_tbl.o tf_ulp/ulp_mapper.o tf_ulp/ulp_mapper_p5.o tf_ulp/ulp_mapper_p7.o tf_ulp/ulp_mark_mgr.o tf_ulp/ulp_port_db.o tf_ulp/ulp_matcher.o tf_ulp/ulp_def_rules.o tf_ulp/bnxt_ulp_linux_flow.o tf_ulp/ulp_tc_parser.o tf_ulp/ulp_tc_handler_tbl.o tf_ulp/ulp_alloc_tbl.o
  TF_ULP_OBJ += tf_ulp/ulp_udcc.o tf_ulp/ulp_nic_flow.o tf_ulp/ulp_generic_flow_offload.o tf_ulp/bnxt_tf_tc_shim.o tf_ulp/bnxt_ulp_meter.o tf_ulp/ulp_sc_mgr.o tf_ulp/ulp_sc_mgr_tfc.o
endif

GENERIC_TEMPLATES_OBJ = tf_ulp/generic_templates/ulp_template_db_wh_plus_class.o tf_ulp/generic_templates/ulp_template_db_wh_plus_act.o tf_ulp/generic_templates/ulp_template_db_thor_class.o tf_ulp/generic_templates/ulp_template_db_thor_act.o tf_ulp/generic_templates/ulp_template_db_thor2_class.o tf_ulp/generic_templates/ulp_template_db_thor2_act.o tf_ulp/generic_templates/ulp_template_db_tbl.o tf_ulp/generic_templates/ulp_template_db_class.o tf_ulp/generic_templates/ulp_template_db_act.o
obj-m += bnxt_en.o

bnxt_en-y := bnxt.o bnxt_hwrm.o bnxt_ethtool_compat.o bnxt_sriov.o bnxt_dcb.o bnxt_ulp.o bnxt_xdp.o bnxt_ptp.o bnxt_vfr.o bnxt_nic_flow.o bnxt_tc.o bnxt_devlink.o bnxt_lfc.o bnxt_dim.o bnxt_coredump.o bnxt_auxbus_compat.o bnxt_mpc.o bnxt_ktls.o bnxt_quic.o bnxt_hdbr.o bnxt_hwmon.o bnxt_sriov_sysfs.o bnxt_tfc.o bnxt_udcc.o bnxt_dcqcn.o bnxt_xsk.o bnxt_fwctl_core_compat.o bnxt_fwctl_bnxt_compat.o $(BNXT_DBGFS_OBJ) $(TF_CORE_OBJ) $(TFC_V3_OBJ) $(CFA_V3_OBJ) $(TF_ULP_OBJ) $(HCAPI_OBJ) $(GENERIC_TEMPLATES_OBJ)#decode_hsi.o

else

BNXT_EXTRA_VER=$(shell $(call bnxt_ver); echo $$BNXT_EXTRA_VER)

define fwd_ver
$(shell if [ -n "$(BNXT_EXTRA_VER)" ] ; then echo "BNXT_EXTRA_VER=$(BNXT_EXTRA_VER) "; fi)
endef

default:
ifeq ($(CROSS_COMPILE),)
	make -C $(LINUX) M=$(shell pwd) $(call fwd_ver)modules
else ifneq ($(CROSS_COMPILE),)
	make -C $(LINUXSRC) M=$(shell pwd) $(call fwd_ver)modules CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH)
endif

yocto_all:
	$(MAKE) -C $(LINUXSRC) M=$(shell pwd)

modules_install:
	$(MAKE) -C $(LINUXSRC) M=$(shell pwd) modules_install

endif

install: default
	@if [ "$(KDIR)" != "" ]; then \
	echo "Cannot use install with KDIR option"; exit 2;\
	fi
	mkdir -p $(PREFIX)$(BCMMODDIR);
	install -m 444 $(BCM_DRV) $(PREFIX)$(BCMMODDIR);
	@if [ "$(PREFIX)" = "" ]; then /sbin/depmod -a ;\
	else echo " *** Run '/sbin/depmod -a' to update the module database.";\
	fi

.PHONEY: all clean install

define src_pkg_cleanup
	rm -f $1;							       \
	if [ -n "$$BNXT_GIT_VER" ]; then				       \
		git checkout $2;					       \
	fi
endef

src_pkg: MANIFEST bnxt.h bnxt_extra_ver.h
	@$(call bnxt_ver);						       \
	if [ -n "$$BNXT_GIT_VER" ]; then				       \
		sed -i -E						       \
		"s/(DRV_MODULE_EXTRA_VER\s+\")(.*)(\")/\1$$BNXT_GIT_VER\3/"    \
		bnxt_extra_ver.h;					       \
	fi;								       \
	BNXT_FILES=$$(cat $< | sed -E "s/.*\s+(.*)/\1/");		       \
	if [ $(call cols, $<) -eq 1 ]; then				       \
		if sha512sum $$BNXT_FILES > $<.hash; then		       \
			BNXT_FILES="$${BNXT_FILES}\n$<.hash";		       \
		else							       \
			$(call src_pkg_cleanup, $<.hash, bnxt_extra_ver.h);    \
			exit 1;						       \
		fi;							       \
	else								       \
		BNXT_FILES="$${BNXT_FILES}\n$<";			       \
	fi;								       \
	printf "$${BNXT_FILES}" | tar czvf bnxt_en-$$BNXT_VER.tar.gz	       \
		--owner=0 --group=0 -T -				       \
		--xform s/$<.hash/$</ --xform s@^@bnxt_en-$$BNXT_VER/@;	       \
	$(call src_pkg_cleanup, $<.hash, bnxt_extra_ver.h)


clean:
	-rm -f bnxt_en-*.tar.gz bnxt.o bnxt.mod.c bnxt.mod.o .bnxt*.cmd *.cmd *.markers *.order *.symvers decode_hsi.o .decode_*
	-rm -rf .tmp_versions
	-rm -rf bnxt_en.o bnxt_en.ko bnxt_en.mod.o bnxt_en.mod.c .bnxt_en.* bnxt_sriov.o .bnxt_sriov.* bnxt_ethtool_compat.o .bnxt_ethtool_compat.* bnxt_dcb.o .bnxt_dcb.* bnxt_ulp.o .bnxt_ulp.* bnxt_ptp.o .bnxt_ptp.* bnxt_xdp.o .bnxt_xdp.* bnxt_lfc.o .bnxt_lfc.* bnxt_hwrm.o .bnxt_hwrm.* bnxt_sriov_sysfs.o .bnxt_sriov_sysfs.* bnxt_udcc.o .bnxt_udcc.* bnxt_dcqcn.o .bnxt_dcqcn.* bnxt_fwctl_core_compat.o .bnxt_fwctl_core_compat.* bnxt_fwctl_bnxt_compat.o .bnxt_fwctl_bnxt_compat.*
	-rm -rf bnxt_vfr.o .bnxt_vfr.* bnxt_nic_flow.o .bnxt_nic_flow.* bnxt_tc.o .bnxt_tc.* bnxt_devlink.o .bnxt_devlink.*

	-rm -rf bnxt_dim.o .bnxt_dim.* bnxt_debugfs*.o .bnxt_debugfs* bnxt_coredump.o .bnxt_coredump.* bnxt_auxbus_compat.o .bnxt_auxbus_compat.* bnxt_mpc.o .bnxt_mpc.* bnxt_ktls.o .bnxt_ktls.* bnxt_quic.o .bnxt_quic.* bnxt_hdbr.o .bnxt_hdbr.* bnxt_tfc.o .bnxt_tfc.* bnxt_hwmon.o .bnxt_hwmon.* bnxt_xsk.o .bnxt_xsk.*
	-rm -f Module.markers Module.symvers modules.order .Module.symvers.cmd .modules.order.cmd

	-rm -f tf_core/tf_msg.o tf_core/tf_util.o tf_core/tf_session.o tf_core/tf_rm.o tf_core/tf_tcam.o tf_core/tf_tbl.o tf_core/tf_identifier.o tf_core/dpool.o tf_core/tf_em_internal.o tf_core/tf_em_hash_internal.o tf_core/tf_if_tbl.o tf_core/tf_global_cfg.o tf_core/tf_sram_mgr.o tf_core/tf_tbl_sram.o tf_core/rand.o hcapi/cfa/hcapi_cfa_p4.o hcapi/cfa/hcapi_cfa_p58.o tf_core/tf_device_p4.o tf_core/tf_device_p58.o tf_core/tf_device.o tf_core/tf_core.o tf_core/tf_tcam_mgr_msg.o tf_core/cfa_tcam_mgr_hwop_msg.o tf_core/cfa_tcam_mgr.o tf_core/cfa_tcam_mgr_p4.o tf_core/cfa_tcam_mgr_p58.o
	-rm -f tf_core/.*.cmd hcapi/cfa/.*.cmd
	-rm -f hcapi/bitalloc.o hcapi/.bitalloc.o.cmd

	-rm -f hcapi/cfa_v3/mm/cfa_mm.o hcapi/cfa_v3/mpc/cfa_bld_p70_mpc.o hcapi/cfa_v3/mpc/cfa_bld_mpc.o hcapi/cfa_v3/mpc/cfa_bld_p70_mpcops.o hcapi/cfa_v3/mpc/cfa_bld_p70_host_mpc_wrapper.o hcapi/cfa_v3/tim/cfa_tim.o hcapi/cfa_v3/tpm/cfa_tpm.o
	-rm -f tfc_v3/tfc_act.o tfc_v3/tfc_cpm.o tfc_v3/tfc_em.o tfc_v3/tfc_global_id.o tfc_v3/tfc_ident.o tfc_v3/tfc_idx_tbl.o tfc_v3/tfc_init.o tfc_v3/tfc_msg.o tfc_v3/tfc_priv.o tfc_v3/tfc_session.o tfc_v3/tfc_tbl_scope.o tfc_v3/tfc_tcam.o tfc_v3/tfc_tcam_debug.o tfc_v3/tfc_util.o tfc_v3/tfo.o tfc_v3/tfc_vf2pf_msg.o tfc_v3/tfc_if_tbl.o tfc_v3/tfc_mpc_table.o
	-rm -f tfc_v3/.*.cmd hcapi/cfa_v3/.*.cmd hcapi/cfa_v3/mpc/.*.cmd hcapi/cfa_v3/mm/.*.cmd hcapi/cfa_v3/tim/.*.cmd hcapi/cfa_v3/tpm/.*.cmd

	-rm -f tf_ulp/ulp_utils.o tf_ulp/.ulp_utils.o.cmd tf_ulp/ulp_template_debug.o tf_ulp/ulp_tf_debug.o tf_ulp/.ulp_template_debug.o.cmd tf_ulp/.ulp_tf_debug.o.cmd tf_ulp/bnxt_tf_ulp.o tf_ulp/ulp_fc_mgr.o tf_ulp/ulp_flow_db.o tf_ulp/ulp_gen_tbl.o tf_ulp/ulp_mapper.o tf_ulp/ulp_mark_mgr.o tf_ulp/ulp_port_db.o tf_ulp/ulp_matcher.o tf_ulp/ulp_def_rules.o tf_ulp/bnxt_ulp_linux_flow.o tf_ulp/ulp_tc_parser.o tf_ulp/ulp_tc_handler_tbl.o tf_ulp/.bnxt_tf_ulp.o.cmd tf_ulp/.ulp_fc_mgr.o.cmd tf_ulp/.ulp_flow_db.o.cmd tf_ulp/.ulp_gen_tbl.o.cmd tf_ulp/.ulp_mapper.o.cmd tf_ulp/.ulp_mark_mgr.o.cmd tf_ulp/.ulp_port_db.o.cmd tf_ulp/.ulp_matcher.o.cmd tf_ulp/.ulp_def_rules.o.cmd tf_ulp/.bnxt_ulp_linux_flow.o.cmd tf_ulp/.ulp_tc_parser.o.cmd tf_ulp/.ulp_tc_handler_tbl.o.cmd tf_ulp/ulp_tc_custom_offload.o tf_ulp/.ulp_tc_custom_offload.o.cmd tf_ulp/ulp_udcc.o tf_ulp/ulp_udcc.o.cmd tf_ulp/ulp_nic_flow.o tf_ulp/.ulp_nic_flow.o.cmd tf_ulp/ulp_generic_flow_offload.o tf_ulp/.ulp_generic_flow_offload.o.cmd tf_ulp/bnxt_tf_tc_shim.o tf_ulp/.bnxt_tf_tc_shim.o.cmd tf_ulp/ulp_tc_rte_flow_gen.o tf_ulp/.ulp_tc_rte_flow_gen.o.cmd tf_ulp/ulp_alloc_tbl.o tf_ulp/.ulp_alloc_tbl.o.cmd
	-rm -f tf_ulp/*.o tf_ulp/.*.o.cmd
	-rm -f tf_ulp/generic_templates/ulp_template_db_wh_plus_class.o tf_ulp/generic_templates/ulp_template_db_wh_plus_act.o tf_ulp/generic_templates/ulp_template_db_thor_class.o tf_ulp/generic_templates/ulp_template_db_thor_act.o tf_ulp/generic_templates/ulp_template_db_tbl.o tf_ulp/generic_templates/ulp_template_db_thor2_class.o  tf_ulp/generic_templates/ulp_template_db_thor2_act.o tf_ulp/generic_templates/ulp_template_db_class.o  tf_ulp/generic_templates/ulp_template_db_act.o tf_ulp/generic_templates/.*.cmd
