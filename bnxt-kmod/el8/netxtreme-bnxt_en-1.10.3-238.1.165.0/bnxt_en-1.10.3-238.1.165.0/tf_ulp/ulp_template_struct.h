/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_TEMPLATE_STRUCT_H_
#define _ULP_TEMPLATE_STRUCT_H_

#include "tf_core.h"
#include "cfa_resources.h"
#include "cfa_types.h"

/* Number of fields for each protocol */
#define BNXT_ULP_PROTO_HDR_SVIF_NUM	2
#define BNXT_ULP_PROTO_HDR_ETH_NUM	3
#define BNXT_ULP_PROTO_HDR_S_VLAN_NUM	3
#define BNXT_ULP_PROTO_HDR_VLAN_NUM	6
#define BNXT_ULP_PROTO_HDR_IPV4_NUM	10
#define BNXT_ULP_PROTO_HDR_IPV6_NUM	8
#define BNXT_ULP_PROTO_HDR_UDP_NUM	4
#define BNXT_ULP_PROTO_HDR_TCP_NUM	9
#define BNXT_ULP_PROTO_HDR_VXLAN_NUM	4
#define BNXT_ULP_PROTO_HDR_VXLAN_GPE_NUM 5
#define BNXT_ULP_PROTO_HDR_GENEVE_NUM   4
#define BNXT_ULP_PROTO_HDR_GRE_NUM	2
#define BNXT_ULP_PROTO_HDR_ICMP_NUM	5
#define BNXT_ULP_PROTO_HDR_ECPRI_NUM	2
#define	BNXT_ULP_PROTO_HDR_IPV6_EXT_NUM	1
#define BNXT_ULP_PROTO_HDR_SRV6_NUM	7
#define BNXT_ULP_PROTO_HDR_MAX		128
#define BNXT_ULP_PROTO_HDR_ENCAP_MAX	64
#define BNXT_ULP_PROTO_HDR_FIELD_SVIF_IDX	1
#define BNXT_ULP_PROTO_HDR_BTH_NUM	3
#define BNXT_ULP_PROTO_HDR_L2_FILTER_NUM 1
#define BNXT_ULP_PROTO_HDR_MPLS_NUM	4
#define BNXT_ULP_PROTO_HDR_IFA_NUM	1

/* Direction attributes */
#define BNXT_ULP_FLOW_ATTR_TRANSFER	0x1
#define BNXT_ULP_FLOW_ATTR_INGRESS	0x2
#define BNXT_ULP_FLOW_ATTR_EGRESS	0x4

struct ulp_tc_hdr_bitmap {
	u64	bits;
};

struct ulp_tc_field_bitmap {
	u64	bits;
};

/* Structure to store the protocol fields */
#define TC_PARSER_FLOW_HDR_FIELD_SIZE		16
struct ulp_tc_hdr_field {
	u8		spec[TC_PARSER_FLOW_HDR_FIELD_SIZE];
	u8		mask[TC_PARSER_FLOW_HDR_FIELD_SIZE];
	u32		size;
};

/* Structure to hold the action property details. */
struct ulp_tc_act_prop {
	u8	act_details[BNXT_ULP_ACT_PROP_IDX_LAST];
};

/* Structure to be used for passing all the parser functions */
struct ulp_tc_parser_params {
	struct hlist_node		next;
	struct ulp_tc_hdr_bitmap	hdr_bitmap;
	struct ulp_tc_hdr_bitmap	act_bitmap;
	struct ulp_tc_hdr_bitmap	enc_hdr_bitmap;
	struct ulp_tc_hdr_bitmap	hdr_fp_bit;
	struct ulp_tc_field_bitmap	fld_bitmap;
	struct ulp_tc_field_bitmap	fld_s_bitmap;
	struct ulp_tc_hdr_field		hdr_field[BNXT_ULP_PROTO_HDR_MAX];
	struct ulp_tc_hdr_field		enc_field[BNXT_ULP_PROTO_HDR_ENCAP_MAX];
	u64				comp_fld[BNXT_ULP_CF_IDX_LAST];
	u32				field_idx;
	struct ulp_tc_act_prop		act_prop;
	u32				dir_attr;
	u32				priority;
	u32				match_chain_id;
	u32				fid;
	u32				parent_flow;
	u32				child_flow;
	u16				func_id;
	u16				port_id;
	u32				class_id;
	u32				act_tmpl;
	struct bnxt_ulp_context		*ulp_ctx;
	u32				hdr_sig_id;
	u64				flow_sig_id;
	u32				flow_pattern_id;
	u32				act_pattern_id;
	u8				app_id;
	u8				tun_idx;
	u16				class_info_idx;
	u16				act_info_idx;
	u64				wc_field_bitmap;
	u64				cf_bitmap;
	u64				exclude_field_bitmap;
	u16				n_proto;
	u16				n_proto_mask;
	u8				ip_proto;
	u8				ip_proto_mask;
	u16				addr_type;
	u32				action_flags;
	u16				tnl_addr_type;
	u8				tnl_dmac[BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_DMAC];
	u8				tnl_smac[BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_SMAC];
	u16				tnl_ether_type;
	void				*tnl_key;
	void				*neigh_key;
	u16				vlan_tpid;
	u16				vlan_tpid_mask;
	bool				implicit_eth_parsed;
	bool				implicit_ipv4_parsed;
	bool				implicit_ipv6_parsed;
	u32				dscp_remap_val;
};

/* Flow Parser Header Information Structure */
struct bnxt_ulp_tc_hdr_info {
	enum bnxt_ulp_hdr_type					hdr_type;
	/* Flow Parser Protocol Header Function Prototype */
	int (*proto_hdr_func)(struct bnxt *bp,
			      struct ulp_tc_parser_params *params,
			      void *match_arg);
};

/* Flow Parser Header Information Structure Array defined in template source*/
extern struct bnxt_ulp_tc_hdr_info	ulp_hdr_info[];
extern struct bnxt_ulp_tc_hdr_info	ulp_vendor_hdr_info[];

/* Flow Parser Action Information Structure */
struct bnxt_ulp_tc_act_info {
	enum bnxt_ulp_act_type					act_type;
	/* Flow Parser Protocol Action Function Prototype */
	int (*proto_act_func)(struct bnxt *bp,
			      struct ulp_tc_parser_params *params,
			      void *action_arg);
};

/* Flow Parser Action Information Structure Array defined in template source*/
extern struct bnxt_ulp_tc_act_info	ulp_act_info[];
extern struct bnxt_ulp_tc_act_info	ulp_vendor_act_info[];

/* Flow Matcher structures */
struct bnxt_ulp_header_match_info {
	struct ulp_tc_hdr_bitmap		hdr_bitmap;
	u32				start_idx;
	u32				num_entries;
	u32				class_tmpl_id;
	u32				act_vnic;
};

struct ulp_tc_bitmap {
	u64	bits;
};

struct bnxt_ulp_class_match_info {
	struct ulp_tc_bitmap	hdr_sig;
	struct ulp_tc_bitmap	field_sig;
	u32			class_hid;
	u32			class_tid;
	u8			act_vnic;
	u8			wc_pri;
	u8			app_sig;
	u32			hdr_sig_id;
	u64			flow_sig_id;
	u32			flow_pattern_id;
	u8			app_id;
	struct ulp_tc_bitmap	hdr_bitmap;
	u64			field_man_bitmap;
	u64			field_opt_bitmap;
	u64			field_exclude_bitmap;
	u8			field_list[BNXT_ULP_GLB_FIELD_TBL_SIZE + 1];
};

/* Flow Matcher templates Structure for class entries */
extern u16 ulp_class_sig_tbl[];
extern struct bnxt_ulp_class_match_info ulp_class_match_list[];

/* Flow Matcher Action structures */
struct bnxt_ulp_act_match_info {
	struct ulp_tc_bitmap	act_bitmap;
	u32			act_tid;
};

/* Flow Matcher templates Structure for action entries */
extern	u16 ulp_act_sig_tbl[];
extern struct bnxt_ulp_act_match_info ulp_act_match_list[];

/* Device Specific Tables for mapper */
struct bnxt_ulp_mapper_cond_info {
	enum bnxt_ulp_cond_opc cond_opcode;
	u64 cond_operand;
};

struct bnxt_ulp_mapper_cond_list_info {
	enum bnxt_ulp_cond_list_opc cond_list_opcode;
	u32 cond_start_idx;
	u32 cond_nums;
	int cond_true_goto;
	int cond_false_goto;
};

struct bnxt_ulp_mapper_func_info {
	enum bnxt_ulp_func_opc		func_opc;
	enum bnxt_ulp_func_src		func_src1;
	enum bnxt_ulp_func_src		func_src2;
	u64			func_opr1;
	u64			func_opr2;
	u16			func_dst_opr;
	u32			func_oper_size;
};

struct bnxt_ulp_template_device_tbls {
	struct bnxt_ulp_mapper_tmpl_info *tmpl_list;
	u32 tmpl_list_size;
	struct bnxt_ulp_mapper_tbl_info *tbl_list;
	u32 tbl_list_size;
	struct bnxt_ulp_mapper_key_info *key_info_list;
	u32 key_info_list_size;
	struct bnxt_ulp_mapper_field_info *key_ext_list;
	u32 key_ext_list_size;
	struct bnxt_ulp_mapper_field_info *result_field_list;
	u32 result_field_list_size;
	struct bnxt_ulp_mapper_ident_info *ident_list;
	u32 ident_list_size;
	struct bnxt_ulp_mapper_cond_info *cond_list;
	u32 cond_list_size;
	struct bnxt_ulp_mapper_cond_list_info *cond_oper_list;
	u32 cond_oper_list_size;

};

struct bnxt_ulp_dyn_size_map {
	u32		slab_size;
	enum tf_tbl_type	tbl_type;
};

/* Device specific parameters */
struct bnxt_ulp_device_params {
	u8				description[16];
	enum bnxt_ulp_byte_order	key_byte_order;
	enum bnxt_ulp_byte_order	result_byte_order;
	enum bnxt_ulp_byte_order	encap_byte_order;
	enum bnxt_ulp_byte_order	wc_key_byte_order;
	enum bnxt_ulp_byte_order	em_byte_order;
	u8				encap_byte_swap;
	u8				num_phy_ports;
	u32			mark_db_lfid_entries;
	u64			mark_db_gfid_entries;
	u64			int_flow_db_num_entries;
	u64			ext_flow_db_num_entries;
	u32			flow_count_db_entries;
	u32			fdb_parent_flow_entries;
	u32			num_resources_per_flow;
	u32			ext_cntr_table_type;
	u64			byte_count_mask;
	u64			packet_count_mask;
	u32			byte_count_shift;
	u32			packet_count_shift;
	u32			wc_dynamic_pad_en;
	u32			em_dynamic_pad_en;
	u32			dynamic_sram_en;
	u32			dyn_encap_list_size;
	struct bnxt_ulp_dyn_size_map	dyn_encap_sizes[5];
	u32			dyn_modify_list_size;
	struct bnxt_ulp_dyn_size_map	dyn_modify_sizes[4];
	u16			em_blk_size_bits;
	u16			em_blk_align_bits;
	u16			em_key_align_bytes;
	u16			em_result_size_bits;
	u16			wc_slice_width;
	u16			wc_max_slices;
	u32			wc_mode_list[4];
	u32			wc_mod_list_max_size;
	u32			wc_ctl_size_bits;
	u32			dev_features;
	const struct bnxt_ulp_generic_tbl_params *gen_tbl_params;
	const struct bnxt_ulp_allocator_tbl_params *allocator_tbl_params;
	const struct bnxt_ulp_template_device_tbls *dev_tbls;
};

/* Flow Mapper */
struct bnxt_ulp_mapper_tmpl_info {
	u32		device_name;
	u32		start_tbl_idx;
	u32		num_tbls;
	struct bnxt_ulp_mapper_cond_list_info reject_info;
};

struct bnxt_ulp_mapper_tbl_info {
	enum bnxt_ulp_resource_func	resource_func;
	u32			resource_type; /* TF_ enum type */
	enum bnxt_ulp_resource_sub_type	resource_sub_type;
	struct bnxt_ulp_mapper_cond_list_info execute_info;
	struct bnxt_ulp_mapper_func_info func_info;
	enum bnxt_ulp_cond_opc cond_opcode;
	u32 cond_operand;
	u8				direction;
	enum bnxt_ulp_pri_opc		pri_opcode;
	u32			pri_operand;

	/* conflict resolution opcode */
	enum bnxt_ulp_accept_opc	accept_opcode;

	enum bnxt_ulp_critical_resource		critical_resource;

	/* Information for accessing the key in ulp_key_field_list */
	u32	key_start_idx;
	u16	key_bit_size;
	u16	key_num_fields;

	/* Information for accessing the partial key in ulp_key_field_list */
	u32	partial_key_start_idx;
	u16	partial_key_bit_size;
	u16	partial_key_num_fields;

	/* Size of the blob that holds the key */
	u16	blob_key_bit_size;
	u16	record_size;

	/* Information for accessing the ulp_class_result_field_list */
	u32	result_start_idx;
	u16	result_bit_size;
	u16	result_num_fields;
	u16	encap_num_fields;

	/* Information for accessing the ulp_ident_list */
	u32	ident_start_idx;
	u16	ident_nums;

	enum bnxt_ulp_mark_db_opc	mark_db_opcode;

	/* Table opcode for table operations */
	u32			tbl_opcode;
	u32			tbl_operand;
	enum bnxt_ulp_generic_tbl_lkup_type gen_tbl_lkup_type;

	/* FDB table opcode */
	enum bnxt_ulp_fdb_opc		fdb_opcode;
	u32			fdb_operand;

	/* Manage ref_cnt via opcode for generic tables */
	enum bnxt_ulp_ref_cnt_opc	ref_cnt_opcode;

	/* Shared session */
	enum bnxt_ulp_session_type	session_type;

	/* Track by session or by function */
	enum cfa_track_type		track_type;

	/* Return driver specific errnos from control table in template */
	int error_code;

	/* Key recipes for generic templates */
	enum bnxt_ulp_key_recipe_opc key_recipe_opcode;
	u32 key_recipe_operand;

	/* control table messages */
	const char			*false_message;
	const char			*true_message;
	const char			*description;
};

struct bnxt_ulp_mapper_field_info {
	u8				description[64];
	u16			field_bit_size;
	enum bnxt_ulp_field_opc		field_opc;
	enum bnxt_ulp_field_src		field_src1;
	u8				field_opr1[16];
	enum bnxt_ulp_field_src		field_src2;
	u8				field_opr2[16];
	enum bnxt_ulp_field_src		field_src3;
	u8				field_opr3[16];
};

struct bnxt_ulp_mapper_key_info {
	struct bnxt_ulp_mapper_field_info	field_info_spec;
	struct bnxt_ulp_mapper_field_info	field_info_mask;
};

struct bnxt_ulp_mapper_ident_info {
	u8		description[64];
	u32	resource_func;

	u16	ident_type;
	u16	ident_bit_size;
	u16	ident_bit_pos;
	enum bnxt_ulp_rf_idx	regfile_idx;
};

struct bnxt_ulp_glb_resource_info {
	u8				app_id;
	enum bnxt_ulp_device_id		device_id;
	enum tf_dir			direction;
	enum bnxt_ulp_session_type	session_type;
	enum bnxt_ulp_resource_func	resource_func;
	u32			resource_type; /* TF_ enum type */
	enum bnxt_ulp_glb_rf_idx	glb_regfile_index;
};

struct bnxt_ulp_resource_resv_info {
	u8				app_id;
	enum bnxt_ulp_device_id		device_id;
	enum tf_dir			direction;
	enum bnxt_ulp_session_type	session_type;
	enum bnxt_ulp_resource_func	resource_func;
	u32			resource_type; /* TF_ enum type */
	u32			count;
};

struct bnxt_ulp_app_capabilities_info {
	u8			app_id;
	u32			default_priority;
	u32			max_def_priority;
	u32			min_flow_priority;
	u32			max_flow_priority;
	u32			vxlan_port;
	u32			vxlan_ip_port;
	u32			ecpri_udp_port;
	enum bnxt_ulp_device_id		device_id;
	u32			upgrade_fw_update;
	u8			ha_pool_id;
	u8			ha_reg_state;
	u8			ha_reg_cnt;
	u8			tunnel_next_proto;
	u32			flags;
	u32			max_pools;
	u8			em_multiplier;
	u32			num_rx_flows;
	u32			num_tx_flows;
	u16			act_rx_max_sz;
	u16			act_tx_max_sz;
	u16			em_rx_key_max_sz;
	u16			em_tx_key_max_sz;
	u32			pbl_page_sz_in_bytes;
	u16			num_key_recipes_per_dir;
	u64			feature_bits;
	u64			default_feature_bits;
	u64			default_class_bits;
	u64			default_act_bits;
};

struct bnxt_ulp_cache_tbl_params {
	u16 num_entries;
};

struct bnxt_ulp_generic_tbl_params {
	const char			*name;
	enum bnxt_ulp_gen_tbl_type	gen_tbl_type;
	u16			result_num_entries;
	u16			result_num_bytes;
	enum bnxt_ulp_byte_order	result_byte_order;
	u32			hash_tbl_entries;
	u16			num_buckets;
	u16			key_num_bytes;
	u16			partial_key_num_bytes;
};

struct bnxt_ulp_allocator_tbl_params  {
	const char		*name;
	u16			num_entries;
};

struct bnxt_ulp_shared_act_info {
	u64 act_bitmask;
};

/* Flow Mapper Static Data Externs:
 * Access to the below static data should be done through access functions and
 * directly throughout the code.
 */

/* The ulp_device_params is indexed by the dev_id.
 * This table maintains the device specific parameters.
 */
extern struct bnxt_ulp_device_params ulp_device_params[];

/* The ulp_act_prop_map_table provides the mapping to index and size of action
 * properties.
 */
extern u32 ulp_act_prop_map_table[];

/* The ulp_glb_resource_tbl provides the list of global resources that need to
 * be initialized and where to store them.
 */
extern struct bnxt_ulp_glb_resource_info ulp_glb_resource_tbl[];

/* The ulp_app_glb_resource_tbl provides the list of shared resources required
 * in the event that shared session is enabled.
 */
extern struct bnxt_ulp_glb_resource_info ulp_app_glb_resource_tbl[];

/* The ulp_resource_resv_list provides the list of tf resources required when
 * calling tf_open.
 */
extern struct bnxt_ulp_resource_resv_info ulp_resource_resv_list[];

/* The ulp_app_resource_resv_list provides the list of tf resources required
 * when calling tf_open.
 */
extern struct bnxt_ulp_resource_resv_info ulp_app_resource_resv_list[];

/* The_app_cap_info_list provides the list of ULP capabilities per app/device.
 */
extern struct bnxt_ulp_app_capabilities_info ulp_app_cap_info_list[];

/* The ulp_cache_tbl_parms table provides the sizes of the cache tables the
 * mapper must dynamically allocate during initialization.
 */
extern struct bnxt_ulp_cache_tbl_params ulp_cache_tbl_params[];

/* The ulp_generic_tbl_parms table provides the sizes of the generic tables the
 * mapper must dynamically allocate during initialization.
 */
extern struct bnxt_ulp_generic_tbl_params ulp_generic_tbl_params[];
/* The ulp_global template table is used to initialize default entries
 * that could be reused by other templates.
 */
extern u32 ulp_glb_template_tbl[];

#endif /* _ULP_TEMPLATE_STRUCT_H_ */
