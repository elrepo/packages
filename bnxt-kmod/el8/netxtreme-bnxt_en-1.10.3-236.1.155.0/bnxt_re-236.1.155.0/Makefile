#!/usr/bin/make
# Makefile for building Linux Broadcom Gigabit ethernet RDMA driver as a module.
# $id$
KVER=
ifeq ($(KVER),)
  KVER=$(shell uname -r)
endif

__ARCH=$(shell uname -m)

ifeq ($(BNXT_EN_INC),)
  BNXT_EN_INC=$(shell pwd)/../bnxt_en
  export BNXT_EN_INC
endif

ifeq ($(BNXT_QPLIB_INC),)
  BNXT_QPLIB_INC=$(shell pwd)
  export BNXT_QPLIB_INC
endif

ifneq ($(SPARSE_EXEC_PATH), )
  BNXT_SPARSE_CMD=CHECK="$(SPARSE_EXEC_PATH) -p=kernel" C=2 CF="-D__CHECK_ENDIAN__"
endif

ifneq ($(SMATCH_EXEC_PATH), )
  BNXT_SMATCH_CMD=CHECK="$(SMATCH_EXEC_PATH) -p=kernel" C=1
endif

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

ifneq ($(KDIR),)
  LINUX=$(KDIR)
  LINUXSRC=$(LINUX)
else
  KDIR=$(LINUX)
endif

ifeq ($(shell ls $(LINUXSRC)/include/uapi > /dev/null 2>&1 && echo uapi),)
  UAPI=
else
  UAPI=uapi
endif

ifeq ($(BCMMODDIR),)
  ifeq ($(shell ls /lib/modules/$(KVER)/updates > /dev/null 2>&1 && echo 1),1)
    BCMMODDIR=/lib/modules/$(KVER)/updates/drivers/infiniband/hw/bnxt_re
  else
    ifeq ($(shell grep -q "search.*[[:space:]]updates" /etc/depmod.conf > /dev/null 2>&1 && echo 1),1)
      BCMMODDIR=/lib/modules/$(KVER)/updates/drivers/infiniband/hw/bnxt_re
    else
      ifeq ($(shell grep -q "search.*[[:space:]]updates" /etc/depmod.d/* > /dev/null 2>&1 && echo 1),1)
        BCMMODDIR=/lib/modules/$(KVER)/updates/drivers/infiniband/hw/bnxt_re
      else
        BCMMODDIR=/lib/modules/$(KVER)/kernel/drivers/infiniband/hw/bnxt_re
      endif
    endif
  endif
endif

ifeq ($(shell type ofed_info >/dev/null 2>&1 && echo 1),1)
ifneq ($(shell ls /usr/src/ofa_kernel/$(__ARCH) | grep -s $(KVER)),)
OFED_VERSION=$(shell ofed_info -s | grep -oP '(?<=OFED-|OFED-internal-|OFED_LINUX-)[0-9]+\.[0-9]+' | sort | uniq)
endif
endif

ifeq ($(OFED_VERSION), )
     $(warning Using native IB stack)
     OFED_VERSION=OFED-NATIVE
endif

ifneq (OFED-NATIVE, $(findstring OFED-NATIVE, $(OFED_VERSION)))
$(warning Using OFED version $(OFED_VERSION))
OFA_BUILD_PATH=/usr/src/ofa_kernel/$(__ARCH)/$(KVER)
OFA_KERNEL_PATH=/usr/src/ofa_kernel/$(__ARCH)/$(KVER)
ofed_5_x=$(filter 5.%, $(OFED_VERSION))
ifneq ($(ofed_5_x), )
override ccflags-y += -DOFED_5_x
endif
override ccflags-y += -D__OFED_BUILD__
override ccflags-y += -include $(OFA_KERNEL_PATH)/include/linux/compat-2.6.h

autoconf_h = $(shell /bin/ls -1 $(LINUX)/include/*/autoconf.h 2> /dev/null | head -1)
kconfig_h=$(shell /bin/ls -1 $(LINUX)/include/*/kconfig.h 2> /dev/null | head -1)
ifeq ($(kconfig_h),)
  kconfig_h=$(shell /bin/ls -1 $(LINUXSRC)/include/*/kconfig.h 2> /dev/null | head -1)
endif

ifneq ($(autoconf_h),)
AUTOCONF_H = -include $(autoconf_h)
endif

ifneq ($(kconfig_h),)
KCONFIG_H = -include $(kconfig_h)
endif

override ccflags-y += -DHAVE_EXTERNAL_OFED
endif

ifeq (OFED-NATIVE, $(findstring OFED-NATIVE, $(OFED_VERSION)))
OFA_KERNEL_PATH=$(LINUXSRC)
OFA_BUILD_PATH=$(LINUX)
else
# Add OFED symbols only if external OFED is used
KBUILD_EXTRA_SYMBOLS := $(OFA_BUILD_PATH)/Module.symvers
endif

ifneq ($(BNXT_PEER_MEM_INC),)
KBUILD_EXTRA_SYMBOLS += $(BNXT_PEER_MEM_INC)/Module.symvers
endif

ifeq ($(shell ls /lib/modules/$(KVER)/source > /dev/null 2>&1 && echo source),)
OFA_KERNEL_PATH=$(OFA_BUILD_PATH)
endif

override ccflags-y += -I$(BNXT_EN_INC)

# Distro specific compilation flags
DISTRO_CFLAG = -D__LINUX

ifneq ($(shell grep "register_netdevice_notifier_rh" $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo register_net),)
  DISTRO_CFLAG += -DHAVE_REGISTER_NETDEVICE_NOTIFIER_RH
endif

ifneq ($(shell grep "__vlan_find_dev_deep_rcu" $(LINUXSRC)/include/linux/if_vlan.h > /dev/null 2>&1 && echo vlan_find_dev_deep_rcu),)
  DISTRO_CFLAG += -DHAVE_VLAN_FIND_DEV_DEEP_RCU
endif

ifneq ($(shell grep -so "ib_mw_type" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo ib_mw_type),)
  DISTRO_CFLAG += -DHAVE_IB_MW_TYPE
endif

ifneq ($(shell grep -A 3 "alloc_mw" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h |grep "struct ib_udata" > /dev/null 2>&1 && echo ib_udata),)
  DISTRO_CFLAG += -DHAVE_ALLOW_MW_WITH_UDATA
endif

ifneq ($(shell grep -A 4 "create_flow" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h |grep "struct ib_udata" > /dev/null 2>&1 && echo ib_udata),)
  DISTRO_CFLAG += -DHAVE_CREATE_FLOW_UDATA
  ifneq ($(shell grep -A 4 "create_flow" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h |grep "int domain" > /dev/null 2>&1 && echo domain),)
   DISTRO_CFLAG += -DHAVE_CREATE_FLOW_DOMAIN_UDATA
  endif
endif

ifneq ($(shell grep -A 3 "reg_user_mr_dmabuf" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h |grep -o "struct uverbs_attr_bundle" > /dev/null 2>&1 && echo reg_user_mr_dmabuf),)
  DISTRO_CFLAG += -DHAVE_REG_USER_MR_DMABUF_HAS_UVERBS_ATTR_BUNDLE
endif

ifneq ($(shell grep "ib_fmr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo ib_fmr),)
  DISTRO_CFLAG += -DHAVE_IB_FMR
endif

ifneq ($(shell grep "rdma_ah_init_attr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo ib_fmr),)
  DISTRO_CFLAG += -DHAVE_RDMA_AH_INIT_ATTR
endif

ifneq ($(shell grep "ib_flow" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo ib_flow),)
  DISTRO_CFLAG += -DHAVE_IB_FLOW
endif

ifneq ($(shell grep "rereg_user_mr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo rereg_user_mr),)
  DISTRO_CFLAG += -DHAVE_IB_REREG_USER_MR
endif

ifneq ($(shell grep "fill_res_mr_entry" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo fill_res_mr_entry),)
  DISTRO_CFLAG += -DHAVE_RDMA_RESTRACK_OPS
  ifneq ($(shell grep "fill_res_srq_entry" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo fill_res_srq_entry),)
    DISTRO_CFLAG += -DHAVE_IB_RES_SRQ_ENTRY
    ifneq ($(shell grep "fill_res_srq_entry_raw" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo fill_res_srq_entry_raw),)
      DISTRO_CFLAG += -DHAVE_IB_RES_SRQ_ENTRY_RAW
    endif
  endif
endif

ifneq ($(shell grep "MEM_WINDOW_TYPE" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo mem_window_type),)
  DISTRO_CFLAG += -DHAVE_IB_MEM_WINDOW_TYPE
endif

ifneq ($(shell grep "odp_caps" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo odp_caps),)
  DISTRO_CFLAG += -DHAVE_IB_ODP_CAPS
endif

ifneq ($(shell grep "IP_BASED_GIDS" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo ip_based_gids),)
  DISTRO_CFLAG += -DHAVE_IB_BASED_GIDS
endif

ifneq ($(shell grep "IB_GID_TYPE_ROCE_UDP_ENCAP" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h |grep "IB_UVERBS_GID_TYPE_ROCE_V2" ),)
  DISTRO_CFLAG += -DHAVE_GID_TYPE_ROCE_UDP_ENCAP_ROCEV2
endif

ifneq ($(shell grep "dmac" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo dmac),)
  DISTRO_CFLAG += -DHAVE_IB_AH_DMAC
endif

ifneq ($(shell grep "IB_ZERO_BASED" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo zero),)
  DISTRO_CFLAG += -DHAVE_IB_ZERO_BASED
endif

ifneq ($(shell grep "IB_ACCESS_ON_DEMAND" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo demand),)
  DISTRO_CFLAG += -DHAVE_IB_ACCESS_ON_DEMAND
endif

ifneq ($(shell grep "sg_table" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h > /dev/null 2>&1 && echo sg_table),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_SG_TABLE
endif

ifneq ($(shell grep "sg_append_table" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h > /dev/null 2>&1 && echo sg_append_table),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_SG_APPEND_TABLE
endif

ifneq ($(shell grep "ib_mr_init_attr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo mr_init_attr),)
  DISTRO_CFLAG += -DHAVE_IB_MR_INIT_ATTR
endif

# add_gid/del_gid replaced the modify_gid
ifneq ($(shell grep "add_gid" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo add_gid),)
  DISTRO_CFLAG += -DHAVE_IB_ADD_DEL_GID
endif

ifneq ($(shell grep "modify_gid" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo modify_gid),)
  DISTRO_CFLAG += -DHAVE_IB_MODIFY_GID
endif

ifneq ($(shell grep -A 3 "struct ib_gid_attr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo struct ib_gid_attr),)
  DISTRO_CFLAG += -DHAVE_IB_GID_ATTR
endif

ifneq ($(shell grep -A 3 "struct ib_bind_mw_wr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo struct ib_bind_mw_wr),)
  DISTRO_CFLAG += -DHAVE_IB_BIND_MW_WR
endif

ifneq ($(shell grep "alloc_mr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo alloc_mr),)
  DISTRO_CFLAG += -DHAVE_IB_ALLOC_MR
endif

ifneq ($(shell grep "map_mr_sg" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo map_mr_sg),)
  DISTRO_CFLAG += -DHAVE_IB_MAP_MR_SG
endif

ifneq ($(shell grep -A 2 "int ib_map_mr_sg" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep sg_offset > /dev/null 2>&1 && echo sg_offset),)
  DISTRO_CFLAG += -DHAVE_IB_MAP_MR_SG_OFFSET
endif

ifneq ($(shell grep -A 2 "int ib_map_mr_sg" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep page_size > /dev/null 2>&1 && echo page_size),)
  DISTRO_CFLAG += -DHAVE_IB_MAP_MR_SG_PAGE_SIZE
endif

ifneq ($(shell grep "IB_WR_REG_MR" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo wr_reg_mr),)
  DISTRO_CFLAG += -DHAVE_IB_REG_MR_WR
endif

ifneq ($(shell grep "ib_mw_bind_info" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo bind_mw),)
  DISTRO_CFLAG += -DHAVE_IB_MW_BIND_INFO
endif

ifneq ($(shell grep "rdma_wr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo rdma_wr),)
  DISTRO_CFLAG += -DHAVE_IB_RDMA_WR
endif

ifneq ($(shell grep "ud_wr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo ud_wr),)
  DISTRO_CFLAG += -DHAVE_IB_UD_WR
endif

ifneq ($(shell grep "get_netdev" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo get_netdev),)
  DISTRO_CFLAG += -DHAVE_IB_GET_NETDEV
endif

ifneq ($(shell grep "get_port_immutable" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo port_immutable),)
  DISTRO_CFLAG += -DHAVE_IB_GET_PORT_IMMUTABLE
endif

ifneq ($(shell grep -o "get_dev_fw_str" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo get_dev_fw_str),)
  DISTRO_CFLAG += -DHAVE_IB_GET_DEV_FW_STR
ifneq ($(shell grep "get_dev_fw_str" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h|grep -o "str_len" > /dev/null 2>&1 && echo str_len),)
  DISTRO_CFLAG += -DIB_GET_DEV_FW_STR_HAS_STRLEN
endif
endif

ifneq ($(shell grep "WIDTH_2X" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo width_2x),)
  DISTRO_CFLAG += -DHAVE_IB_WIDTH_2X
endif

ifneq ($(shell grep -A 2 "process_mad" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep "u32 port_num"),)
  DISTRO_CFLAG += -DHAVE_PROCESS_MAD_U32_PORT
else
  ifneq ($(shell grep "ib_mad_hdr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo ib_mad_hdr),)
    DISTRO_CFLAG += -DHAVE_PROCESS_MAD_IB_MAD_HDR
    endif
endif

ifneq ($(shell grep "query_device" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h -A2 | grep udata > /dev/null 2>&1 && echo query_device),)
  DISTRO_CFLAG += -DHAVE_IB_QUERY_DEVICE_UDATA
endif

ifneq ($(shell grep "drain_rq" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo drain_rq),)
  DISTRO_CFLAG += -DHAVE_IB_DRAIN
endif

ifneq ($(shell grep "RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo roce_v2_enable),)
  DISTRO_CFLAG +=  -DENABLE_ROCEV2_QP1
endif

ifneq (,$(shell grep -so "ETHTOOL_LINK_MODE_25000baseCR_Full_BIT" $(LINUXSRC)/include/$(UAPI)/linux/ethtool.h $(LINUXSRC)/include/linux/ethtool.h))
  DISTRO_CFLAG += -DHAVE_ETHTOOL_GLINKSETTINGS_25G
endif

ifneq ($(shell grep -o "^struct ethtool_keee" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_KEEE
endif

ifneq ($(shell grep -Pzo "struct ethtool_link_ksettings[\s\S]*lanes;" $(LINUXSRC)/include/linux/ethtool.h),)
  DISTRO_CFLAG += -DHAVE_ETHTOOL_LINK_KSETTINGS_LANES
endif

ifneq (,$(shell grep -so "IB_USER_VERBS_EX_CMD_MODIFY_QP" $(OFA_KERNEL_PATH)/include/$(UAPI)/rdma/ib_user_verbs.h))
  DISTRO_CFLAG += -DHAVE_IB_USER_VERBS_EX_CMD_MODIFY_QP
endif

ifneq (,$(shell grep -so "struct ib_mr \*(\*rereg_user_mr)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h))
  DISTRO_CFLAG += -DHAVE_REREG_USER_MR_RET_PTR
endif

ifneq (,$(shell grep -so "uverbs_ex_cmd_mask" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h))
  DISTRO_CFLAG += -DHAVE_IB_UVERBS_CMD_MASK_IN_DRIVER
endif

ifneq (,$(shell grep -so "IB_QP_ATTR_STANDARD_BITS" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h))
  DISTRO_CFLAG += -DHAVE_IB_QP_ATTR_STANDARD_BITS
endif

ifneq ($(shell grep -o "rdma_addr_find_l2_eth_by_grh" $(OFA_KERNEL_PATH)/include/rdma/ib_addr.h),)
  DISTRO_CFLAG += -DHAVE_RDMA_ADDR_FIND_L2_ETH_BY_GRH
endif

ifneq ($(shell grep "rdma_addr_find_l2_eth_by_grh" $(OFA_KERNEL_PATH)/include/rdma/ib_addr.h -A2 | grep net_device ),)
  DISTRO_CFLAG += -DHAVE_RDMA_ADDR_FIND_L2_ETH_BY_GRH_WITH_NETDEV
endif

ifneq ($(shell grep -A 2 "rdma_addr_find_dmac_by_grh" $(OFA_KERNEL_PATH)/include/rdma/ib_addr.h | grep if_index),)
  DISTRO_CFLAG += -DHAVE_RDMA_ADDR_FIND_DMAC_BY_GRH_V2
endif

ifneq ($(shell grep "create_ah" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h -A2 | grep udata > /dev/null 2>&1 && echo create_ah),)
  DISTRO_CFLAG += -DHAVE_IB_CREATE_AH_UDATA
endif

ifneq ($(shell grep "*create_user_ah" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo create_user_ah),)
  DISTRO_CFLAG += -DHAVE_IB_CREATE_USER_AH
endif

ifneq ($(shell grep -o "ether_addr_copy" $(LINUXSRC)/include/linux/etherdevice.h),)
  DISTRO_CFLAG += -DHAVE_ETHER_ADDR_COPY
endif

ifneq ($(shell grep "X86_FEATURE_MOVDIR64B" $(LINUXSRC)/arch/$(SRCARCH)/include/asm/cpufeatures.h > /dev/null 2>&1 && echo movdir64b),)
  DISTRO_CFLAG += -DHAVE_X86_FEATURE_MOVDIR64B
endif

ifneq ($(shell grep -o "page_shift" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_PAGE_SHIFT
endif

ifneq ($(shell grep -o "ib_umem_page_count" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_PAGE_COUNT
endif

ifneq ($(shell grep -o "npages" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_NPAGES_IB_UMEM
endif

ifneq ($(shell grep -o "page_size" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_PAGE_SIZE
endif

ifneq ($(shell grep -o "rdma_ah_attr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_RDMA_AH_ATTR
endif

ifneq ($(shell grep -o "roce_ah_attr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_ROCE_AH_ATTR
endif

ifneq ($(shell grep -o "ib_resolve_eth_dmac" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_IB_RESOLVE_ETH_DMAC
endif

ifneq ($(shell grep -o "rdma_umem_for_each_dma_block" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_RDMA_UMEM_FOR_EACH_DMA_BLOCK
endif

ifneq ($(shell grep -o "disassociate_ucontext" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_DISASSOCIATE_UCNTX
endif

ifneq ($(shell if [ -e $(LINUXSRC)/include/net/bonding.h ]; then echo y; fi),)
  DISTRO_CFLAG += -DHAVE_NET_BONDING_H
endif

ifneq ($(shell if [ -e $(LINUXSRC)/include/linux/sched/mm.h ]; then echo y; fi),)
  DISTRO_CFLAG += -DHAVE_SCHED_MM_H
endif

ifneq ($(shell if [ -e $(LINUXSRC)/include/linux/sched/task.h ]; then echo y; fi),)
  DISTRO_CFLAG += -DHAVE_SCHED_TASK_H
endif

ifneq ($(shell grep "ib_register_device" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep name),)
ifneq ($(shell grep "ib_register_device" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h -A1 | grep "dma_device"),)
  DISTRO_CFLAG += -DHAVE_DMA_DEVICE_IN_IB_REGISTER_DEVICE
else
  DISTRO_CFLAG += -DHAVE_NAME_IN_IB_REGISTER_DEVICE
endif
endif


ifneq ($(shell grep "ib_modify_qp_is_ok" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h -A2 | grep rdma_link_layer),)
  DISTRO_CFLAG += -DHAVE_LL_IN_IB_MODIFY_QP_IS_OK
endif

ifneq ($(shell grep "rdma_user_mmap_io" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_RDMA_USER_MMAP_IO
endif

ifneq ($(shell grep "rdma_user_mmap_io" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h -A2 |grep rdma_user_mmap_entry),)
  DISTRO_CFLAG += -DHAVE_RDMA_USER_MMAP_IO_USE_MMAP_ENTRY
endif

ifneq ($(shell grep "ib_counters_read_attr" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
# Kind of a misnomer to handle disassociate_ucontext in RH8.0. This is the best key
# in ib_verbs.h.
  DISTRO_CFLAG += -DHAVE_NO_MM_MMAP_SEM
endif

# Configfs stuff
ifneq ($(shell grep -w "CONFIGFS_ATTR" $(LINUXSRC)/include/linux/configfs.h|grep -o _pfx),)
  HAVE_CONFIGFS_ENABLED=y
  override ccflags-y += -DHAVE_CONFIGFS_ENABLED
  ifneq ($(shell grep -o "configfs_add_default_group" $(LINUXSRC)/include/linux/configfs.h),)
    DISTRO_CFLAG += -DHAVE_CFGFS_ADD_DEF_GRP
  endif
else
  ifneq ($(shell grep -w "__CONFIGFS_ATTR" $(LINUXSRC)/include/linux/configfs.h|grep -o _show),)
    HAVE_CONFIGFS_ENABLED=y
    override ccflags-y += -DHAVE_CONFIGFS_ENABLED -DHAVE_OLD_CONFIGFS_API
  endif
endif

ifneq ($(shell grep -o "ib_umem_get_flags" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_GET_FLAGS -DCONFIG_INFINIBAND_PEER_MEM
endif

ifneq ($(shell grep -o "ib_umem_get_peer" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_GET_PEER -DCONFIG_INFINIBAND_PEER_MEM
endif

ifneq ($(shell grep -o "ib_umem_dmabuf_get" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_DMABUF
  ifneq ($(shell grep -o "ib_umem_dmabuf_get_pinned" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
    DISTRO_CFLAG += -DHAVE_IB_UMEM_DMABUF_PINNED
  endif
endif

ifneq ($(shell grep -o "ib_umem_stop_invalidation_notifier" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_STOP_INVALIDATION
endif

ifneq ($(shell grep -o "NETDEV_BONDING_FAILOVER" $(LINUXSRC)/include/linux/netdevice.h),)
  ifeq ("$(shell test  -e $(LINUXSRC)/include/net/bonding.h && echo test)", "test")
      DISTRO_CFLAG += -DLEGACY_BOND_SUPPORT
  endif
endif

ifneq ($(shell grep -o "NETDEV_BONDING_INFO" $(LINUXSRC)/include/linux/netdevice.h),)
  DISTRO_CFLAG += -DHAVE_NETDEV_BONDING_INFO
endif

ifneq ($(BNXT_PEER_MEM_INC),)
      export BNXT_PEER_MEM_INC
      ifneq ($(shell grep -o "ib_umem_get_flags" $(BNXT_PEER_MEM_INC)/peer_umem.h),)
          DISTRO_CFLAG += -DHAVE_IB_UMEM_GET_FLAGS -DCONFIG_INFINIBAND_PEER_MEM
      endif
      override ccflags-y += -DIB_PEER_MEM_MOD_SUPPORT
      override ccflags-y += -I$(BNXT_PEER_MEM_INC)
endif

ifneq ($(shell ls $(LINUXSRC)/include/linux/dim.h > /dev/null 2>&1 && echo dim),)
  DISTRO_CFLAG += -DHAVE_DIM
endif

ifneq ($(shell ls $(OFA_KERNEL_PATH)/include/rdma/uverbs_ioctl.h > /dev/null 2>&1 && echo uverbs_ioctl),)
  DISTRO_CFLAG += -DHAVE_UVERBS_IOCTL_H
  ifneq ($(shell grep "rdma_udata_to_drv_context" $(OFA_KERNEL_PATH)/include/rdma/uverbs_ioctl.h),)
     DISTRO_CFLAG += -DHAVE_RDMA_UDATA_TO_DRV_CONTEXT
  endif
  ifneq ($(shell grep "uverbs_get_raw_fd" $(OFA_KERNEL_PATH)/include/rdma/uverbs_ioctl.h),)
     DISTRO_CFLAG += -DHAVE_UVERBS_GET_RAW_FD
  endif
endif

ifneq ($(shell grep -A 1 "(*post_srq_recv)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h  | grep -o "const struct ib_recv_wr"),)
  DISTRO_CFLAG += -DHAVE_IB_ARG_CONST_CHANGE
endif

ifneq ($(shell grep -A 1 "(*get_netdev)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h  | grep -o "u32 port_num"),)
  DISTRO_CFLAG += -DHAVE_IB_SUPPORT_MORE_RDMA_PORTS
endif

ifneq ($(shell grep -A 4 "(*create_flow)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h  | grep -o "struct ib_udata"),)
  DISTRO_CFLAG += -DHAVE_UDATA_FOR_CREATE_FLOW
endif

ifneq ($(shell grep -A 15 "struct ib_device_attr {" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h  | grep -o "max_send_sge"),)
  DISTRO_CFLAG += -DHAVE_SEPARATE_SEND_RECV_SGE
endif

ifneq ($(shell grep "ib_get_cached_gid" $(OFA_KERNEL_PATH)/include/rdma/ib_cache.h > /dev/null 2>&1 && echo ib_get_cached_gid),)
  DISTRO_CFLAG += -DHAVE_IB_GET_CACHED_GID
endif

ifneq ($(shell grep "rdma_read_gid_l2_fields" $(OFA_KERNEL_PATH)/include/rdma/ib_cache.h > /dev/null 2>&1 && echo rdma_read_gid_l2_fields),)
  DISTRO_CFLAG += -DHAVE_RDMA_READ_GID_L2_FIELDS
endif

ifneq ($(shell grep "rdma_create_user_ah" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h > /dev/null 2>&1 && echo rdma_create_user_ah),)
  DISTRO_CFLAG += -DHAVE_CREATE_USER_AH
endif

ifeq ($(ofed_major), OFED-4.17)
  ifeq ($(shell grep -A 1 "(*add_gid)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "port_num"),)
    DISTRO_CFLAG += -DHAVE_SIMPLIFIED_ADD_DEL_GID
  endif
else
  ifneq ($(shell grep -A 2 "struct ib_gid_attr {" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h  | grep -o "struct ib_device"),)
    ifeq ($(shell grep -A 1 "(*add_gid)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "port_num"),)
      DISTRO_CFLAG += -DHAVE_SIMPLIFIED_ADD_DEL_GID
    endif
    ifeq ($(shell grep -A 1 "(*add_gid)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "union ib_gid"),)
      DISTRO_CFLAG += -DHAVE_SIMPLER_ADD_GID
    endif
  endif
endif

ifneq ($(shell grep -A 6 "struct ib_ah {" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h  | grep -o "sgid_attr"),)
  DISTRO_CFLAG += -DHAVE_GID_ATTR_IN_IB_AH
endif
ifneq ($(shell grep "rdma_gid_attr_network_type" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_RDMA_GID_ATTR_NETWORK_TYPE
endif

ifneq ($(shell grep "ib_set_device_ops" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_IB_SET_DEV_OPS
endif

ifneq ($(shell grep "RDMA_CREATE_AH_SLEEPABLE" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_SLEEPABLE_AH
endif

ifneq ($(shell grep "IB_POLL_UNBOUND_WORKQUEUE" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_IB_POLL_UNBOUND_WORKQUEUE
endif

ifneq ($(shell grep "dma_zalloc_coherent" $(LINUXSRC)/include/linux/dma-mapping.h),)
  DISTRO_CFLAG += -DHAVE_DMA_ZALLOC_COHERENT
endif

ifneq ($(shell grep "for_each_sg_dma_page" $(LINUXSRC)/include/linux/scatterlist.h),)
  DISTRO_CFLAG += -DHAVE_FOR_EACH_SG_DMA_PAGE
endif

ifneq ($(shell grep "sg_append_table" $(LINUXSRC)/include/linux/scatterlist.h),)
  DISTRO_CFLAG += -DHAS_SG_APPEND_TABLE
endif

ifneq ($(shell grep "has_secondary_link" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAS_PCI_SECONDARY_LINK
endif

ifneq ($(shell grep "pci_enable_atomic_ops_to_root" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAGS += -DHAS_ENABLE_ATOMIC_OPS
endif

ifneq ($(shell grep "tasklet_setup" $(LINUXSRC)/include/linux/interrupt.h),)
  DISTRO_CFLAG += -DHAS_TASKLET_SETUP
endif

ifneq ($(shell grep "irq_affinity_desc" $(LINUXSRC)/include/linux/interrupt.h),)
  DISTRO_CFLAG += -DHAVE_IRQ_AFFINITY_DESC
endif

ifneq ($(shell grep "sysfs_emit" $(LINUXSRC)/include/linux/sysfs.h),)
  DISTRO_CFLAG += -DHAS_SYSFS_EMIT
endif

ifneq ($(shell grep "DECLARE_RDMA_OBJ_SIZE" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "ib_pd"),)
  DISTRO_CFLAG += -DHAVE_PD_ALLOC_IN_IB_CORE
endif

ifneq ($(shell grep "DECLARE_RDMA_OBJ_SIZE" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "ib_cq"),)
  DISTRO_CFLAG += -DHAVE_CQ_ALLOC_IN_IB_CORE
endif

ifneq ($(shell grep "DECLARE_RDMA_OBJ_SIZE" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "ib_qp"),)
  DISTRO_CFLAG += -DHAVE_QP_ALLOC_IN_IB_CORE
endif

ifneq ($(shell grep "(\*alloc_pd)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "ib_ucontext"),)
  DISTRO_CFLAG += -DHAVE_UCONTEXT_IN_ALLOC_PD
endif

ifneq ($(shell grep "(\*alloc_pd)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h -A1| grep -o "ib_ucontext"),)
  DISTRO_CFLAG += -DHAVE_UCONTEXT_IN_ALLOC_PD
endif

ifneq ($(shell grep "ib_device_ops" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h -A1| grep -o "owner"),)
  DISTRO_CFLAG += -DHAVE_IB_OWNER_IN_DEVICE_OPS
endif

ifneq ($(shell grep "DECLARE_RDMA_OBJ_SIZE" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "ib_ah"),)
  DISTRO_CFLAG += -DHAVE_AH_ALLOC_IN_IB_CORE
endif

ifneq ($(shell grep "DECLARE_RDMA_OBJ_SIZE" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "ib_srq"),)
  DISTRO_CFLAG += -DHAVE_SRQ_CREATE_IN_IB_CORE
endif

ifneq ($(shell grep "(\*dealloc_pd)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "udata"),)
  DISTRO_CFLAG += -DHAVE_DEALLOC_PD_UDATA
endif
ifneq ($(shell grep "(\*dealloc_pd)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "void"),)
  DISTRO_CFLAG += -DHAVE_DEALLOC_PD_RET_VOID
endif
ifneq ($(shell grep "(\*destroy_srq)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "udata"),)
  DISTRO_CFLAG += -DHAVE_DESTROY_SRQ_UDATA
endif

ifneq ($(shell grep "(\*destroy_cq)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "udata"),)
  DISTRO_CFLAG += -DHAVE_DESTROY_CQ_UDATA
endif

ifneq ($(shell grep "(\*destroy_qp)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "udata"),)
  DISTRO_CFLAG += -DHAVE_DESTROY_QP_UDATA
endif

ifneq ($(shell grep "(\*destroy_ah)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "void"),)
  DISTRO_CFLAG += -DHAVE_DESTROY_AH_RET_VOID
endif

ifneq ($(shell grep "(\*destroy_srq)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "void"),)
  DISTRO_CFLAG += -DHAVE_DESTROY_SRQ_RET_VOID
endif

ifneq ($(shell grep "(\*alloc_mw)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "int"),)
  DISTRO_CFLAG += -DHAVE_ALLOC_MW_RET_INT
endif

ifneq ($(shell grep "(\*destroy_cq)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "void"),)
  DISTRO_CFLAG += -DHAVE_DESTROY_CQ_RET_VOID
endif

ifneq ($(shell grep "(\*create_cq)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h -A2 | grep -o "ib_ucontext"),)
  DISTRO_CFLAG += -DHAVE_CREATE_CQ_UCONTEXT
endif

ifneq ($(shell grep "(\*dereg_mr)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "udata"),)
  DISTRO_CFLAG += -DHAVE_DEREG_MR_UDATA
endif

ifneq ($(shell grep "(\*alloc_mr)" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h -A1 | grep -o "ib_udata"),)
  DISTRO_CFLAG += -DHAVE_ALLOC_MR_UDATA
endif

ifneq ($(shell grep "DECLARE_RDMA_OBJ_SIZE" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "ib_ucontext"),)
  DISTRO_CFLAG += -DHAVE_UCONTEXT_ALLOC_IN_IB_CORE
endif

ifneq ($(shell grep "DECLARE_RDMA_OBJ_SIZE" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "ib_mw"),)
  DISTRO_CFLAG += -DHAVE_ALLOC_MW_IN_IB_CORE
endif

ifneq ($(shell grep "ib_alloc_device" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "member"),)
  DISTRO_CFLAG += -DHAVE_MEMBER_IN_IB_ALLOC_DEVICE
endif

ifneq ($(shell grep "ib_umem_get" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h | grep -o "udata"),)
  DISTRO_CFLAG += -DHAVE_UDATA_IN_IB_UMEM_GET
endif

ifneq ($(shell grep "ib_umem_get" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h | grep -o "ib_device"),)
  DISTRO_CFLAG += -DHAVE_IB_DEVICE_IN_IB_UMEM_GET
endif

ifneq ($(shell grep "ib_umem_get" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h -A1| grep -o "dmasync"),)
  DISTRO_CFLAG += -DHAVE_DMASYNC_IB_UMEM_GET
endif

ifneq ($(shell grep "ib_umem_num_pages" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_NUM_PAGES
endif

ifneq ($(shell grep -o "size_t ib_umem_num_dma_blocks" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_NUM_DMA_BLOCKS
endif

ifneq ($(shell grep -o "long ib_umem_find_best_pgsz" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_FIND_BEST_PGSZ
endif

ifneq ($(shell grep "init_port" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_VERB_INIT_PORT
endif

ifneq ($(shell grep "rdma_set_device_sysfs_group" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_RDMA_SET_DEVICE_SYSFS_GROUP
endif

ifneq ($(shell grep "ib_device_set_netdev" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_IB_DEVICE_SET_NETDEV
endif

ifneq ($(shell grep "rdma_driver_id" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_RDMA_DRIVER_ID
endif

ifneq ($(shell grep "rdma_for_each_block" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_DMA_BLOCK_ITERATOR
endif

ifneq ($(shell grep "ib_port_phys_state" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_PHYS_PORT_STATE_ENUM
endif

ifneq ($(shell grep "ib_get_eth_speed" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_IB_GET_ETH_SPEED
endif

ifneq ($(shell grep -r "ib_get_eth_speed" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -E "u16\s+\*speed"),)
  DISTRO_CFLAG += -DHAVE_IB_GET_ETH_SPEED_U16
endif

ifneq ($(shell grep "vlan_id" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_IB_WC_VLAN_ID
endif

ifneq ($(shell grep "smac" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_IB_WC_SMAC
endif

ifneq ($(shell grep "pci_num_vf" $(LINUXSRC)/include/linux/pci.h),)
  DISTRO_CFLAG += -DHAVE_PCI_NUM_VF
endif

ifneq ($(shell grep "ib_kernel_cap_flags" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_IB_KERNEL_CAP_FLAGS
endif

ifneq ($(shell grep -so "ida_alloc" $(LINUXSRC)/include/linux/idr.h),)
  DISTRO_CFLAG += -DHAVE_IDA_ALLOC
endif

ifneq ($(shell grep -o "struct auxiliary_device_id" $(OFA_KERNEL_PATH)/include/linux/mod_devicetable.h),)
  DISTRO_CFLAG += -DHAVE_AUX_DEVICE_ID
endif

ifdef CONFIG_AUXILIARY_BUS
ifneq ($(shell ls $(OFA_KERNEL_PATH)/include/linux/auxiliary_bus.h > /dev/null 2>&1 && echo auxiliary_driver),)
  DISTRO_CFLAG += -DHAVE_AUXILIARY_DRIVER
endif

ifneq ($(shell grep -so "auxiliary_get_drvdata" $(OFA_KERNEL_PATH)/include/linux/auxiliary_bus.h),)
  DISTRO_CFLAG += -DHAVE_AUX_GET_DRVDATA
endif

ifneq ($(shell grep -sw "(\*remove)"  $(OFA_KERNEL_PATH)/include/linux/auxiliary_bus.h | grep -o "int"),)
  DISTRO_CFLAG += -DHAVE_AUDEV_REM_RET_INT
endif
endif

ifneq ($(shell grep -o "struct rdma_stat_desc {" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_RDMA_STAT_DESC
endif

ifneq ($(shell grep -o "alloc_hw_port_stats" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_ALLOC_HW_PORT_STATS
endif

ifneq ($(shell grep -so "vmalloc_array" $(LINUXSRC)/include/linux/vmalloc.h),)
  DISTRO_CFLAG += -DHAVE_VMALLOC_ARRAY
endif

ifneq ($(shell grep -so "rdma_user_mmap_entry_insert" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_RDMA_XARRAY_MMAP_V1
  ifneq ($(shell grep -so "rdma_user_mmap_entry_insert_range" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
    DISTRO_CFLAG += -DHAVE_RDMA_XARRAY_MMAP_V2
	ifneq ($(shell grep -so "uapi_definition" $(OFA_KERNEL_PATH)/include/rdma/uverbs_ioctl.h),)
	    DISTRO_CFLAG += -DHAVE_UAPI_DEF
	endif
  endif
endif


ifneq ($(shell grep -A 2 "create_cq" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep "struct uverbs_attr_bundle" > /dev/null 2>&1 && echo uverbs_attr_bundle),)
  DISTRO_CFLAG += -DHAVE_IB_CQ_CREATE_HAS_UVERBS_ATTR_BUNDLE
endif

ifneq ($(shell grep -so "addrconf_addr_eui48" $(LINUXSRC)/include/net/addrconf.h),)
  DISTRO_CFLAG += -DHAVE_ADDRCONF_ADDR_EUI48
endif

ifneq ($(shell grep "IB_ACCESS_RELAXED_ORDERING" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_IB_ACCESS_RELAXED_ORDERING
endif

ifneq ($(shell grep -A 7 "struct ib_port_attr {" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h  | grep -o "ip_gids"),)
  DISTRO_CFLAG += -DHAVE_IP_GIDS_IN_PORT_ATTR
endif

ifneq ($(shell grep "rdma_ah_get_sl" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h),)
  DISTRO_CFLAG += -DHAVE_RDMA_AH_GET_SL
endif

ifneq ($(shell grep "ib_umem_dma_offset" $(OFA_KERNEL_PATH)/include/rdma/ib_umem.h),)
  DISTRO_CFLAG += -DHAVE_IB_UMEM_DMA_OFFSET
endif

ifneq ($(shell grep "ib_check_mr_access" $(OFA_KERNEL_PATH)/include/rdma/ib_verbs.h | grep -o "struct ib_device"),)
  DISTRO_CFLAG += -DHAVE_DEV_IN_IB_CHECK_MR_ACCESS
endif

ifneq ($(shell grep "debugfs_change_name" $(LINUXSRC)/include/linux/debugfs.h),)
  DISTRO_CFLAG += -DHAVE_DEBUGFS_CHANGE_NAME
endif

KBUILD_EXTRA_SYMBOLS += $(BNXT_EN_INC)/Module.symvers

override ccflags-y += ${DISTRO_CFLAG} -DFPGA -g -DCONFIG_BNXT_SRIOV 		\
		-DCONFIG_BNXT_DCB -DENABLE_DEBUGFS -DCONFIG_BNXT_RE	\
		-DPOST_QP1_DUMMY_WQE

override ccflags-y += -I$(BNXT_QPLIB_INC) -I$(BNXT_EN_INC)

BCM_DRV = bnxt_re.ko

KSRC=$(LINUXSRC)

ifneq (OFED-NATIVE, $(findstring OFED-NATIVE, $(OFED_VERSION)))
OFED_INCLUDES := LINUXINCLUDE=' \
                $(AUTOCONF_H) \
                $(KCONFIG_H) \
                -I$(OFA_KERNEL_PATH)/include \
		-I$(OFA_KERNEL_PATH)/include/linux \
                -I$(OFA_KERNEL_PATH)/include/uapi \
		 $$(if $$(CONFIG_XEN),-D__XEN_INTERFACE_VERSION__=$$(CONFIG_XEN_INTERFACE_VERSION)) \
		 $$(if $$(CONFIG_XEN),-I$$(KSRC)/arch/x86/include/mach-xen) \
                -I$(OFA_KERNEL_PATH)/arch/$$(SRCARCH)/include/generated/uapi \
                -I$(OFA_KERNEL_PATH)/arch/$$(SRCARCH)/include/generated \
                -Iinclude \
                -I$(KSRC)/include \
                -I$(KSRC)/arch/$$(SRCARCH)/include \
                -I$(KSRC)/include/generated/uapi \
                -I$(KSRC)/include/uapi \
                -I$(KSRC)/arch/$$(SRCARCH)/include/uapi \
                -I$(KSRC)/arch/$$(SRCARCH)/include/generated \
                -I$(KSRC)/arch/$$(SRCARCH)/include/generated/uapi \
                -I$(KDIR)/include/generated/uapi \
                -I$(KDIR)/arch/$$(SRCARCH)/include/generated \
                -I$(KDIR)/arch/$$(SRCARCH)/include/generated/uapi'

OFA_KERNEL_LINK = $(OFA_KERNEL_PATH)
OFA_BUILD_LINK = $(OFA_BUILD_PATH)
endif

ifneq ($(KERNELRELEASE),)

obj-m += bnxt_re.o
bnxt_re-y := main.o ib_verbs.o		\
	     debugfs.o compat.o		\
	     qplib_res.o qplib_rcfw.o	\
	     qplib_sp.o qplib_fp.o	\
	     stats.o hdbr.o	\
	     hw_counters.o		\
	     mmap_compat.o dv_verbs.o

bnxt_re-$(HAVE_CONFIGFS_ENABLED) += configfs.o

endif

default:
	$(MAKE) -C $(LINUX) M=$(shell pwd) $(OFED_INCLUDES) \
	$(BNXT_SPARSE_CMD) $(BNXT_SMATCH_CMD) modules

yocto_all:
	$(MAKE) -C $(LINUXSRC) M=$(shell pwd)

modules_install:
	$(MAKE) -C $(LINUXSRC) M=$(shell pwd) modules_install


install: default
	echo $(PREFIX)
	echo $(BCMMODDIR)
	echo $(BCM_DRV)
	mkdir -p $(PREFIX)/$(BCMMODDIR);
	install -m 444 $(BCM_DRV) $(PREFIX)/$(BCMMODDIR);
	@if [ "$$OFED_VERSION" != "OFED-NATIVE" ] && ls /etc/depmod.d/*-mlnx-ofa_kernel-bnxt_re.conf 1> /dev/null 2>&1; then\
		echo 'override bnxt_re * updates/drivers/infiniband/hw/bnxt_re' > /etc/depmod.d/zzz99-bcm-bnxt_re.conf;\
	fi;
	@if [ "$(PREFIX)" = "" ]; then /sbin/depmod -a ;\
	else echo " *** Run '/sbin/depmod -a' to update the module database.";\
	fi

.PHONEY: all clean install

clean:
	$(MAKE) -C $(LINUX) M=$(shell pwd) clean
