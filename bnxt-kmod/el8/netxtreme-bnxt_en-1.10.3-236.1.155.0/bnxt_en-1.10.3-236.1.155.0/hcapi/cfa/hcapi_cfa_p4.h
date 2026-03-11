/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _HCAPI_CFA_P4_H_
#define _HCAPI_CFA_P4_H_

#include "cfa_p40_hw.h"

/* CFA phase 4 fix formatted table(layout) ID definition */
enum cfa_p4_tbl_id {
	CFA_P4_TBL_L2CTXT_TCAM = 0,
	CFA_P4_TBL_L2CTXT_REMAP,
	CFA_P4_TBL_PROF_TCAM,
	CFA_P4_TBL_PROF_TCAM_REMAP,
	CFA_P4_TBL_WC_TCAM,
	CFA_P4_TBL_WC_TCAM_REC,
	CFA_P4_TBL_WC_TCAM_REMAP,
	CFA_P4_TBL_VEB_TCAM,
	CFA_P4_TBL_SP_TCAM,
	CFA_P4_TBL_PROF_SPIF_DFLT_L2CTXT,
	CFA_P4_TBL_PROF_PARIF_DFLT_ACT_REC_PTR,
	CFA_P4_TBL_PROF_PARIF_ERR_ACT_REC_PTR,
	CFA_P4_TBL_LKUP_PARIF_DFLT_ACT_REC_PTR,
	CFA_P4_TBL_MAX
};

#define CFA_P4_PROF_MAX_KEYS 4
enum cfa_p4_mac_sel_mode {
	CFA_P4_MAC_SEL_MODE_FIRST = 0,
	CFA_P4_MAC_SEL_MODE_LOWEST = 1,
};

struct cfa_p4_prof_key_cfg {
	u8 mac_sel[CFA_P4_PROF_MAX_KEYS];
#define CFA_P4_PROF_MAC_SEL_DMAC0 BIT(0)
#define CFA_P4_PROF_MAC_SEL_T_MAC0 BIT(1)
#define CFA_P4_PROF_MAC_SEL_OUTERMOST_MAC0 BIT(2)
#define CFA_P4_PROF_MAC_SEL_DMAC1 BIT(3)
#define CFA_P4_PROF_MAC_SEL_T_MAC1 BIT(4)
#define CFA_P4_PROF_MAC_OUTERMOST_MAC1 BIT(5)
	u8 pass_cnt;
	enum cfa_p4_mac_sel_mode mode;
};

/* CFA action layout definition */

#define CFA_P4_ACTION_MAX_LAYOUT_SIZE 184

/**
 * Action object template structure
 *
 * Template structure presents data fields that are necessary to know
 * at the beginning of Action Builder (AB) processing. Like before the
 * AB compilation. One such example could be a template that is
 * flexible in size (Encap Record) and the presence of these fields
 * allows for determining the template size as well as where the
 * fields are located in the record.
 *
 * The template may also present fields that are not made visible to
 * the caller by way of the action fields.
 *
 * Template fields also allow for additional checking on user visible
 * fields. One such example could be the encap pointer behavior on a
 * CFA_P4_ACT_OBJ_TYPE_ACT or CFA_P4_ACT_OBJ_TYPE_ACT_SRAM.
 */
struct cfa_p4_action_template {
	/** Action Object type
	 *
	 * Controls the type of the Action Template
	 */
	enum {
		/** Select this type to build an Action Record Object
		 */
		CFA_P4_ACT_OBJ_TYPE_ACT,
		/** Select this type to build an Action Statistics
		 * Object
		 */
		CFA_P4_ACT_OBJ_TYPE_STAT,
		/** Select this type to build a SRAM Action Record
		 * Object.
		 */
		CFA_P4_ACT_OBJ_TYPE_ACT_SRAM,
		/** Select this type to build a SRAM Action
		 * Encapsulation Object.
		 */
		CFA_P4_ACT_OBJ_TYPE_ENCAP_SRAM,
		/** Select this type to build a SRAM Action Modify
		 * Object, with IPv4 capability.
		 */
		/* In case of Stingray the term Modify is used for the 'NAT
		 * action'. Action builder is leveraged to fill in the NAT
		 * object which then can be referenced by the action
		 * record.
		 */
		CFA_P4_ACT_OBJ_TYPE_MODIFY_IPV4_SRAM,
		/** Select this type to build a SRAM Action Source
		 * Property Object.
		 */
		/* In case of Stingray this is not a 'pure' action record.
		 * Action builder is leveraged to full in the Source Property
		 * object which can then be referenced by the action
		 * record.
		 */
		CFA_P4_ACT_OBJ_TYPE_SRC_PROP_SRAM,
		/** Select this type to build a SRAM Action Statistics
		 * Object
		 */
		CFA_P4_ACT_OBJ_TYPE_STAT_SRAM,
	} obj_type;

	/** Action Control
	 *
	 * Controls the internals of the Action Template
	 *
	 * act is valid when:
	 * (obj_type == CFA_P4_ACT_OBJ_TYPE_ACT)
	 */
	/* Stat and encap are always inline for EEM as table scope
	 * allocation does not allow for separate Stats allocation,
	 * but has the xx_inline flags as to be forward compatible
	 * with Stingray 2, always treated as TRUE.
	 */
	struct {
		/** Set to CFA_HCAPI_TRUE to enable statistics
		 */
		u8 stat_enable;
		/** Set to CFA_HCAPI_TRUE to enable statistics to be inlined
		 */
		u8 stat_inline;

		/** Set to CFA_HCAPI_TRUE to enable encapsulation
		 */
		u8 encap_enable;
		/** Set to CFA_HCAPI_TRUE to enable encapsulation to be inlined
		 */
		u8 encap_inline;
	} act;

	/** Modify Setting
	 *
	 * Controls the type of the Modify Action the template is
	 * describing
	 *
	 * modify is valid when:
	 * (obj_type == CFA_P4_ACT_OBJ_TYPE_MODIFY_SRAM)
	 */
	enum {
		/** Set to enable Modify of Source IPv4 Address
		 */
		CFA_P4_MR_REPLACE_SOURCE_IPV4 = 0,
		/** Set to enable Modify of Destination IPv4 Address
		 */
		CFA_P4_MR_REPLACE_DEST_IPV4
	} modify;

	/** Encap Control
	 * Controls the type of encapsulation the template is
	 * describing
	 *
	 * encap is valid when:
	 * ((obj_type == CFA_P4_ACT_OBJ_TYPE_ACT) &&
	 *   act.encap_enable) ||
	 * ((obj_type == CFA_P4_ACT_OBJ_TYPE_SRC_PROP_SRAM)
	 */
	struct {
		/* Direction is required as Stingray Encap on RX is
		 * limited to l2 and VTAG only.
		 */
		/** Receive or Transmit direction
		 */
		u8 direction;
		/** Set to CFA_HCAPI_TRUE to enable L2 capability in the
		 *  template
		 */
		u8 l2_enable;
		/** vtag controls the Encap Vector - VTAG Encoding, 4 bits
		 *
		 *
		 * CFA_P4_ACT_ENCAP_VTAGS_PUSH_0, default, no VLAN
		 * Tags applied
		 * CFA_P4_ACT_ENCAP_VTAGS_PUSH_1, adds capability to
		 * set 1 VLAN Tag. Action Template compile adds
		 * the following field to the action object
		 * ::TF_ER_VLAN1
		 * CFA_P4_ACT_ENCAP_VTAGS_PUSH_2, adds capability to
		 * set 2 VLAN Tags. Action Template compile adds
		 * the following fields to the action object
		 * ::TF_ER_VLAN1 and ::TF_ER_VLAN2
		 *
		 */
		enum { CFA_P4_ACT_ENCAP_VTAGS_PUSH_0 = 0,
		       CFA_P4_ACT_ENCAP_VTAGS_PUSH_1,
		       CFA_P4_ACT_ENCAP_VTAGS_PUSH_2 } vtag;

		/* The remaining fields are NOT supported when
		 * direction is RX and ((obj_type ==
		 * CFA_P4_ACT_OBJ_TYPE_ACT) && act.encap_enable).
		 * ab_compile_layout will perform the checking and
		 * skip remaining fields.
		 */
		/** L3 Encap controls the Encap Vector - L3 Encoding,
		 *  3 bits. Defines the type of L3 Encapsulation the
		 *  template is describing.
		 *
		 * CFA_P4_ACT_ENCAP_L3_NONE, default, no L3
		 * Encapsulation processing.
		 * CFA_P4_ACT_ENCAP_L3_IPV4, enables L3 IPv4
		 * Encapsulation.
		 * CFA_P4_ACT_ENCAP_L3_IPV6, enables L3 IPv6
		 * Encapsulation.
		 * CFA_P4_ACT_ENCAP_L3_MPLS_8847, enables L3 MPLS
		 * 8847 Encapsulation.
		 * CFA_P4_ACT_ENCAP_L3_MPLS_8848, enables L3 MPLS
		 * 8848 Encapsulation.
		 *
		 */
		enum {
			/** Set to disable any L3 encapsulation
			 * processing, default
			 */
			CFA_P4_ACT_ENCAP_L3_NONE = 0,
			/** Set to enable L3 IPv4 encapsulation
			 */
			CFA_P4_ACT_ENCAP_L3_IPV4 = 4,
			/** Set to enable L3 IPv6 encapsulation
			 */
			CFA_P4_ACT_ENCAP_L3_IPV6 = 5,
			/** Set to enable L3 MPLS 8847 encapsulation
			 */
			CFA_P4_ACT_ENCAP_L3_MPLS_8847 = 6,
			/** Set to enable L3 MPLS 8848 encapsulation
			 */
			CFA_P4_ACT_ENCAP_L3_MPLS_8848 = 7
		} l3;

#define CFA_P4_ACT_ENCAP_MAX_MPLS_LABELS 8
		/** 1-8 labels, valid when
		 * (l3 == CFA_P4_ACT_ENCAP_L3_MPLS_8847) ||
		 * (l3 == CFA_P4_ACT_ENCAP_L3_MPLS_8848)
		 *
		 * MAX number of MPLS Labels 8.
		 */
		u8 l3_num_mpls_labels;

		/** Set to CFA_HCAPI_TRUE to enable L4 capability in the
		 * template.
		 *
		 * CFA_HCAPI_TRUE adds ::TF_EN_UDP_SRC_PORT and
		 * ::TF_EN_UDP_DST_PORT to the template.
		 */
		u8 l4_enable;

		/** Tunnel Encap controls the Encap Vector - Tunnel
		 *  Encap, 3 bits. Defines the type of Tunnel
		 *  encapsulation the template is describing
		 *
		 * CFA_P4_ACT_ENCAP_TNL_NONE, default, no Tunnel
		 * Encapsulation processing.
		 * CFA_P4_ACT_ENCAP_TNL_GENERIC_FULL
		 * CFA_P4_ACT_ENCAP_TNL_VXLAN. NOTE: Expects
		 * l4_enable set to CFA_P4_TRUE;
		 * CFA_P4_ACT_ENCAP_TNL_NGE. NOTE: Expects l4_enable
		 * set to CFA_P4_TRUE;
		 * CFA_P4_ACT_ENCAP_TNL_NVGRE. NOTE: only valid if
		 * l4_enable set to CFA_HCAPI_FALSE.
		 * CFA_P4_ACT_ENCAP_TNL_GRE.NOTE: only valid if
		 * l4_enable set to CFA_HCAPI_FALSE.
		 * CFA_P4_ACT_ENCAP_TNL_GENERIC_AFTER_TL4
		 * CFA_P4_ACT_ENCAP_TNL_GENERIC_AFTER_TNL
		 *
		 */
		enum {
			/** Set to disable Tunnel header encapsulation
			 * processing, default
			 */
			CFA_P4_ACT_ENCAP_TNL_NONE = 0,
			/** Set to enable Tunnel Generic Full header
			 * encapsulation
			 */
			CFA_P4_ACT_ENCAP_TNL_GENERIC_FULL,
			/** Set to enable VXLAN header encapsulation
			 */
			CFA_P4_ACT_ENCAP_TNL_VXLAN,
			/** Set to enable NGE (VXLAN2) header encapsulation
			 */
			CFA_P4_ACT_ENCAP_TNL_NGE,
			/** Set to enable NVGRE header encapsulation
			 */
			CFA_P4_ACT_ENCAP_TNL_NVGRE,
			/** Set to enable GRE header encapsulation
			 */
			CFA_P4_ACT_ENCAP_TNL_GRE,
			/** Set to enable Generic header after Tunnel
			 * L4 encapsulation
			 */
			CFA_P4_ACT_ENCAP_TNL_GENERIC_AFTER_TL4,
			/** Set to enable Generic header after Tunnel
			 * encapsulation
			 */
			CFA_P4_ACT_ENCAP_TNL_GENERIC_AFTER_TNL
		} tnl;

		/** Number of bytes of generic tunnel header,
		 * valid when
		 * (tnl == CFA_P4_ACT_ENCAP_TNL_GENERIC_FULL) ||
		 * (tnl == CFA_P4_ACT_ENCAP_TNL_GENERIC_AFTER_TL4) ||
		 * (tnl == CFA_P4_ACT_ENCAP_TNL_GENERIC_AFTER_TNL)
		 */
		u8 tnl_generic_size;
		/** Number of 32b words of nge options,
		 * valid when
		 * (tnl == CFA_P4_ACT_ENCAP_TNL_NGE)
		 */
		u8 tnl_nge_op_len;
		/* Currently not planned */
		/* Custom Header */
		/*	u8 custom_enable; */
	} encap;
};

/**
 * Enumeration of SRAM entry types, used for allocation of
 * fixed SRAM entities. The memory model for CFA HCAPI
 * determines if an SRAM entry type is supported.
 * NOTE: Any additions to this enum must be reflected on FW
 * side as well.
 */
enum cfa_p4_action_sram_entry_type {
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_FULL_ACTION,  /* SRAM Action Record */

	CFA_P4_ACTION_SRAM_ENTRY_TYPE_FORMAT_0_ACTION,
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_FORMAT_1_ACTION,
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_FORMAT_2_ACTION,
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_FORMAT_3_ACTION,
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_FORMAT_4_ACTION,

	CFA_P4_ACTION_SRAM_ENTRY_TYPE_ENCAP_8B,	/* SRAM Action Encap
						 * 8 Bytes
						 */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_ENCAP_16B,/* SRAM Action Encap
						 * 16 Bytes
						 */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_ENCAP_64B,/* SRAM Action Encap
						 * 64 Bytes
						 */

	CFA_P4_ACTION_SRAM_ENTRY_TYPE_MODIFY_PORT_SRC,
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_MODIFY_PORT_DEST,

	CFA_P4_ACTION_SRAM_ENTRY_TYPE_MODIFY_IPV4_SRC,	/* SRAM Action Modify
							 * IPv4 Source
							 */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_MODIFY_IPV4_DEST,	/* SRAM Action Modify
							 * IPv4 Destination
							 */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_SP_SMAC,		/* SRAM Action Source
							 * Properties SMAC
							 */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_SP_SMAC_IPV4,	/* SRAM Action Source
							 * Props SMAC IPv4
							 */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_SP_SMAC_IPV6,	/* SRAM Action Source
							 * Props SMAC IPv6
							 */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_STATS_64,		/* SRAM Action
							 * Stats 64 Bits
							 */
	CFA_P4_ACTION_SRAM_ENTRY_TYPE_MAX
};

/**
 * SRAM Action Record structure holding either an action index or an
 * action ptr.
 * @act_idx:	SRAM Action idx specifies the offset of the SRAM
 *		element within its SRAM Entry Type block. This
 *		index can be written into i.e. an L2 Context. Use
 *		this type for all SRAM Action Record types except
 *		SRAM Full Action records. Use act_ptr instead.
 * @act_ptr:	SRAM Full Action is special in that it needs an
 *		action record pointer. This pointer can be written
 *		into i.e. a Wildcard TCAM entry.
 */
union cfa_p4_action_sram_act_record {
	u16 act_idx;
	u32 act_ptr;
};

/**
 * cfa_p4_action_param parameter definition
 * @dir:	receive or transmit direction
 * @type:	type of the sram allocation type
 * @record:	action record to set. The 'type' specified lists the
 *		record definition to use in the passed in record.
 * @act_size:	number of elements in act_data
 * @act_data:	ptr to array of action data
 */
struct cfa_p4_action_param {
	u8					dir;
	enum cfa_p4_action_sram_entry_type	type;
	union cfa_p4_action_sram_act_record	record;
	u32					act_size;
	u64					*act_data;
};

/* EEM Key entry sizes */
#define CFA_P4_EEM_KEY_MAX_SIZE 52
#define CFA_P4_EEM_KEY_RECORD_SIZE 64

/**
 * cfa_eem_entry_hdr
 * @pointer:	eem entry pointer
 * @word1:	The header is made up of two words, this is the first word.
 *		This field has multiple subfields, there is no suitable
 *		single name for it so just going with word1.
 */
struct cfa_p4_eem_entry_hdr {
	u32 pointer;
	u32 word1;
#define CFA_P4_EEM_ENTRY_VALID_SHIFT 31
#define CFA_P4_EEM_ENTRY_VALID_MASK 0x80000000
#define CFA_P4_EEM_ENTRY_L1_CACHEABLE_SHIFT 30
#define CFA_P4_EEM_ENTRY_L1_CACHEABLE_MASK 0x40000000
#define CFA_P4_EEM_ENTRY_STRENGTH_SHIFT 28
#define CFA_P4_EEM_ENTRY_STRENGTH_MASK 0x30000000
#define CFA_P4_EEM_ENTRY_RESERVED_SHIFT 17
#define CFA_P4_EEM_ENTRY_RESERVED_MASK 0x0FFE0000
#define CFA_P4_EEM_ENTRY_KEY_SIZE_SHIFT 8
#define CFA_P4_EEM_ENTRY_KEY_SIZE_MASK 0x0001FF00
#define CFA_P4_EEM_ENTRY_ACT_REC_SIZE_SHIFT 3
#define CFA_P4_EEM_ENTRY_ACT_REC_SIZE_MASK 0x000000F8
#define CFA_P4_EEM_ENTRY_ACT_REC_INT_SHIFT 2
#define CFA_P4_EEM_ENTRY_ACT_REC_INT_MASK 0x00000004
#define CFA_P4_EEM_ENTRY_EXT_FLOW_CTR_SHIFT 1
#define CFA_P4_EEM_ENTRY_EXT_FLOW_CTR_MASK 0x00000002
#define CFA_P4_EEM_ENTRY_ACT_PTR_MSB_SHIFT 0
#define CFA_P4_EEM_ENTRY_ACT_PTR_MSB_MASK 0x00000001
};

/**
 *  cfa_p4_eem_key_entry
 *  @key:	Key is 448 bits - 56 bytes
 *  @hdr:	Header is 8 bytes long
 */
struct cfa_p4_eem_64b_entry {
	u8 key[CFA_P4_EEM_KEY_RECORD_SIZE -
			sizeof(struct cfa_p4_eem_entry_hdr)];
	struct cfa_p4_eem_entry_hdr hdr;
};

#endif /* _CFA_HW_P4_H_ */
