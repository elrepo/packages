/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_ULP_H_
#define _BNXT_ULP_H_

#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/limits.h>
#include <linux/mutex.h>

#include "bnxt_compat.h"
#include "bnxt.h"

#include "tf_core.h"
#include "cfa_types.h"
#include "ulp_template_db_enum.h"
#include "bnxt_tf_common.h"

/* NAT defines to reuse existing inner L2 SMAC and DMAC */
#define BNXT_ULP_NAT_INNER_L2_HEADER_SMAC	0x2000
#define BNXT_ULP_NAT_OUTER_MOST_L2_HDR_SMAC	0x6000
#define BNXT_ULP_NAT_OUTER_MOST_L2_VLAN_TAGS	0xc00
#define BNXT_ULP_NAT_INNER_L2_HEADER_DMAC	0x100
#define BNXT_ULP_NAT_OUTER_MOST_L2_HDR_DMAC	0x300
#define BNXT_ULP_NAT_OUTER_MOST_FLAGS (BNXT_ULP_NAT_OUTER_MOST_L2_HDR_SMAC |\
					BNXT_ULP_NAT_OUTER_MOST_L2_VLAN_TAGS |\
					BNXT_ULP_NAT_OUTER_MOST_L2_HDR_DMAC)

/* defines for the ulp_flags */
#define BNXT_ULP_VF_REP_ENABLED		0x1
#define BNXT_ULP_SHARED_SESSION_ENABLED 0x2
#define BNXT_ULP_APP_DEV_UNSUPPORTED	0x4
#define BNXT_ULP_HIGH_AVAIL_ENABLED	0x8
#define BNXT_ULP_APP_UNICAST_ONLY	0x10
#define BNXT_ULP_APP_SOCKET_DIRECT	0x20
#define BNXT_ULP_APP_TOS_PROTO_SUPPORT  0x40
#define BNXT_ULP_APP_BC_MC_SUPPORT      0x80
#define BNXT_ULP_STATIC_VXLAN_SUPPORT	0x100
#define BNXT_ULP_MULTI_SHARED_SUPPORT	0x200
#define BNXT_ULP_APP_HA_DYNAMIC		0x400
#define BNXT_ULP_APP_SRV6		0x800
#define BNXT_ULP_APP_L2_ETYPE		0x1000
#define BNXT_ULP_SHARED_TBL_SCOPE_ENABLED 0x2000
#define BNXT_ULP_APP_DSCP_REMAP_ENABLED	0x4000
#define BNXT_ULP_DYNAMIC_VXLAN_SUPPORT	0x8000
#define BNXT_ULP_APP_NIC_FLOWS_SUPPORT  0x10000

#define ULP_VF_REP_IS_ENABLED(flag)	((flag) & BNXT_ULP_VF_REP_ENABLED)
#define ULP_SHARED_SESSION_IS_ENABLED(flag) ((flag) &\
					     BNXT_ULP_SHARED_SESSION_ENABLED)
#define ULP_APP_DEV_UNSUPPORTED_ENABLED(flag)	((flag) &\
						 BNXT_ULP_APP_DEV_UNSUPPORTED)
#define ULP_HIGH_AVAIL_IS_ENABLED(flag)	((flag) & BNXT_ULP_HIGH_AVAIL_ENABLED)
#define ULP_DSCP_REMAP_IS_ENABLED(flag)	((flag) & BNXT_ULP_APP_DSCP_REMAP_ENABLED)
#define ULP_SOCKET_DIRECT_IS_ENABLED(flag) ((flag) & BNXT_ULP_APP_SOCKET_DIRECT)
#define ULP_APP_TOS_PROTO_SUPPORT(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_APP_TOS_PROTO_SUPPORT)
#define ULP_APP_BC_MC_SUPPORT(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_APP_BC_MC_SUPPORT)
#define ULP_MULTI_SHARED_IS_SUPPORTED(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_MULTI_SHARED_SUPPORT)
#define ULP_APP_HA_IS_DYNAMIC(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_APP_HA_DYNAMIC)

#define ULP_APP_STATIC_VXLAN_PORT_EN(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_STATIC_VXLAN_SUPPORT)
#define ULP_APP_DYNAMIC_VXLAN_PORT_EN(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_DYNAMIC_VXLAN_SUPPORT)

#define ULP_APP_SRV6_SUPPORT(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_APP_SRV6)
#define ULP_APP_L2_ETYPE_SUPPORT(ctx)	((ctx)->cfg_data->ulp_flags &\
					BNXT_ULP_APP_L2_ETYPE)
#define ULP_APP_NIC_FLOWS_SUPPORTED(ctx)	((ctx)->cfg_data->ulp_flags &\
						 BNXT_ULP_APP_NIC_FLOWS_SUPPORT)

/* defines for mirror enable/disable bit */
#define MIRROR_REG_BIT BIT(31)

enum bnxt_ulp_flow_mem_type {
	BNXT_ULP_FLOW_MEM_TYPE_INT = 0,
	BNXT_ULP_FLOW_MEM_TYPE_EXT = 1,
	BNXT_ULP_FLOW_MEM_TYPE_BOTH = 2,
	BNXT_ULP_FLOW_MEM_TYPE_LAST = 3
};

enum bnxt_tc_flow_item_type {
	BNXT_TC_FLOW_ITEM_TYPE_END = (u32)INT_MIN,
	BNXT_TC_FLOW_ITEM_TYPE_VXLAN_DECAP,
	BNXT_TC_FLOW_ITEM_TYPE_LAST
};

enum bnxt_tc_flow_action_type {
	BNXT_TC_FLOW_ACTION_TYPE_END = S32_MIN,
	BNXT_TC_FLOW_ACTION_TYPE_VXLAN_DECAP,
	BNXT_TC_FLOW_ACTION_TYPE_LAST
};

enum bnxt_session_type {
	BNXT_SESSION_TYPE_REGULAR = 0,
	BNXT_SESSION_TYPE_SHARED_COMMON,
	BNXT_SESSION_TYPE_SHARED_WC,
	BNXT_SESSION_TYPE_LAST
};

struct bnxt_ulp_df_rule_info {
	u32			def_port_flow_id;
	u8				valid;
};

struct bnxt_ulp_vfr_rule_info {
	u32			vfr_flow_id;
	u16			parent_port_id;
	u8				valid;
};

enum bnxt_ulp_meter_color {
	MTR_PROF_CLR_GREEN = 0,
	MTR_PROF_CLR_YELLOW,
	MTR_PROF_CLR_RED,
	MTR_PROF_CLR_MAX
};

#define	MTR_PROF_CLR_INVALID	MTR_PROF_CLR_MAX
#define BNXT_ULP_DSCP_INVALID	-1

struct bnxt_dscp_remap_vf {
	__be32	dscp_remap_val;
	u32	dscp_remap_ref;
};

#define BNXT_ULP_DSCP_INSERT_CAP(dscp_remap)	\
		((dscp_remap)->sriov_dscp_insert)

struct bnxt_ulp_dscp_remap {
	bool				sriov_dscp_insert;
	bool				dscp_remap_initialized;
	u32				meter_prof_id[MTR_PROF_CLR_MAX];
	u32				meter_id[MTR_PROF_CLR_MAX];
	u32				*dscp_global_cfg;
	struct bnxt_dscp_remap_vf	dscp_remap_vf[BNXT_DSCP_REMAP_ROWS][MTR_PROF_CLR_MAX];
};

struct bnxt_ulp_data {
	u32				tbl_scope_id;
	struct bnxt_ulp_mark_tbl	*mark_tbl;
	u32				dev_id;
	u32				ref_cnt;
	struct bnxt_ulp_flow_db		*flow_db;
	/* Serialize flow db operations */
	struct mutex			flow_db_lock; /* flow db lock */
	void				*mapper_data;
	void				*matcher_data;
	struct bnxt_ulp_port_db		*port_db;
	struct bnxt_ulp_fc_info		*fc_info;
	u32				ulp_flags;
#define	BNXT_TC_MAX_PORTS 1024
	struct bnxt_ulp_df_rule_info	df_rule_info[BNXT_TC_MAX_PORTS];
	struct bnxt_ulp_vfr_rule_info	vfr_rule_info[BNXT_TC_MAX_PORTS];
	enum bnxt_ulp_flow_mem_type	mem_type;
#define	BNXT_ULP_TUN_ENTRY_INVALID	-1
#define	BNXT_ULP_MAX_TUN_CACHE_ENTRIES	16
	u8				app_id;
	u8				num_shared_clients;
	u32				default_priority;
	u32				max_def_priority;
	u32				min_flow_priority;
	u32				max_flow_priority;
	u32				vxlan_port;
	u32				vxlan_gpe_port;
	u32				vxlan_ip_port;
	u32				ecpri_udp_port;
	u32				hu_session_type;
	u32				max_pools;
	u32				num_rx_flows;
	u32				num_tx_flows;
	u16				act_rx_max_sz;
	u16				act_tx_max_sz;
	u16				em_rx_key_max_sz;
	u16				em_tx_key_max_sz;
	u32				page_sz;
	u8				hu_reg_state;
	u8				hu_reg_cnt;
	u8				ha_pool_id;
	u8				tunnel_next_proto;
	u8				em_multiplier;
	enum bnxt_ulp_session_type	def_session_type;
	u16				num_key_recipes_per_dir;
	struct delayed_work             fc_work;
	struct delayed_work             sc_work;
	struct workqueue_struct        *sc_wq;
	u64				feature_bits;
	u64				default_class_bits;
	u64				default_act_bits;
	bool				meter_initialized;
	bool				force_mirror_en;
	/* Below structure is protected by flow_db_lock */
	struct bnxt_ulp_dscp_remap	dscp_remap;
	struct ulp_fc_tfc_stats_cache_entry *stats_cache;
	struct bnxt_ulp_sc_info		*sc_info;
	struct mutex			sc_lock; /* Stats cache lock */
};

enum bnxt_ulp_tfo_type {
	BNXT_ULP_TFO_TYPE_INVALID = 0,
	BNXT_ULP_TFO_TYPE_P5,
	BNXT_ULP_TFO_TYPE_P7
};

#define BNXT_ULP_SESSION_MAX 3
#define BNXT_ULP_TFO_SID_FLAG (1)
#define BNXT_ULP_TFO_TSID_FLAG (2)

struct bnxt_ulp_context {
	struct bnxt_ulp_data	*cfg_data;
	struct bnxt		*bp;
	enum bnxt_ulp_tfo_type	tfo_type;
	union  {
		void		*g_tfp[BNXT_ULP_SESSION_MAX];
		struct {
			u32	tfo_flags;
			void	*tfcp;
			u16	sid;
			u8	tsid;
		};
	};
	const struct bnxt_ulp_core_ops *ops;
	u32 veb_flow_id;
};

struct bnxt_ulp_pci_info {
	u32	domain;
	u8		bus;
};

#define BNXT_ULP_DEVICE_SERIAL_NUM_SIZE 8
#define BNXT_ULP_BOARD_SERIAL_NUM_SIZE 32
struct bnxt_ulp_session_state {
	struct hlist_node			next;
	bool					bnxt_ulp_init;
	/* Serialize session operations */
	struct mutex				bnxt_ulp_mutex; /* ulp lock */
	struct bnxt_ulp_pci_info		pci_info;
	u8					dsn[BNXT_ULP_DEVICE_SERIAL_NUM_SIZE];
	u8					bsn[BNXT_ULP_BOARD_SERIAL_NUM_SIZE];
	struct bnxt_ulp_data			*cfg_data;
	struct tf				*g_tfp[BNXT_ULP_SESSION_MAX];
	u32					session_opened[BNXT_ULP_SESSION_MAX];
	/* Need to revisit a union for the tf related data */
	u16					session_id;
};

/* ULP flow id structure */
struct tc_tf_flow {
	u32	flow_id;
};

struct ulp_tlv_param {
	enum bnxt_ulp_df_param_type type;
	u32 length;
	u8 value[16];
};

struct ulp_context_list_entry {
	struct hlist_node			next;
	struct bnxt_ulp_context			*ulp_ctx;
};

struct bnxt_ulp_core_ops {
	int
	(*ulp_init)(struct bnxt *bp,
		    struct bnxt_ulp_session_state *session,
		    enum cfa_app_type app_type);
	void
	(*ulp_deinit)(struct bnxt *bp,
		      struct bnxt_ulp_session_state *session);
	int
	(*ulp_ctx_attach)(struct bnxt *bp,
			  struct bnxt_ulp_session_state *session,
			  enum cfa_app_type app_type);
	void
	(*ulp_ctx_detach)(struct bnxt *bp,
			  struct bnxt_ulp_session_state *session);
	void *
	(*ulp_tfp_get)(struct bnxt_ulp_context *ulp,
		       enum bnxt_ulp_session_type s_type);
	int
	(*ulp_vfr_session_fid_add)(struct bnxt_ulp_context *ulp_ctx,
				   u16 rep_fid);
	int
	(*ulp_vfr_session_fid_rem)(struct bnxt_ulp_context *ulp_ctx,
				   u16 rep_fid);
};

extern const struct bnxt_ulp_core_ops bnxt_ulp_tf_core_ops;
extern const struct bnxt_ulp_core_ops bnxt_ulp_tfc_core_ops;

int
bnxt_ulp_devid_get(struct bnxt *bp, enum bnxt_ulp_device_id  *ulp_dev_id);

/* Allow the deletion of context only for the bnxt device that
 * created the session
 */
bool
ulp_ctx_deinit_allowed(struct bnxt_ulp_context *ulp_ctx);

/* Function to set the device id of the hardware. */
int
bnxt_ulp_cntxt_dev_id_set(struct bnxt_ulp_context *ulp_ctx, u32 dev_id);

/* Function to get the device id of the hardware. */
int
bnxt_ulp_cntxt_dev_id_get(struct bnxt_ulp_context *ulp_ctx, u32 *dev_id);

/* Function to get whether or not ext mem is used for EM */
int
bnxt_ulp_cntxt_mem_type_get(struct bnxt_ulp_context *ulp_ctx,
			    enum bnxt_ulp_flow_mem_type *mem_type);

/* Function to set whether or not ext mem is used for EM */
int
bnxt_ulp_cntxt_mem_type_set(struct bnxt_ulp_context *ulp_ctx,
			    enum bnxt_ulp_flow_mem_type mem_type);

/* Function to set the table scope id of the EEM table. */
int
bnxt_ulp_cntxt_tbl_scope_id_set(struct bnxt_ulp_context *ulp_ctx,
				u32 tbl_scope_id);

/* Function to get the table scope id of the EEM table. */
int
bnxt_ulp_cntxt_tbl_scope_id_get(struct bnxt_ulp_context *ulp_ctx,
				u32 *tbl_scope_id);

int
bnxt_ulp_cntxt_bp_set(struct bnxt_ulp_context *ulp, struct bnxt *bp);

/* Function to get the bp associated with the ulp_ctx */
struct bnxt *
bnxt_ulp_cntxt_bp_get(struct bnxt_ulp_context *ulp);

/* Function to set the v3 table scope id, only works for tfc objects */
int
bnxt_ulp_cntxt_tsid_set(struct bnxt_ulp_context *ulp_ctx, u8 tsid);

/*
 * Function to set the v3 table scope id, only works for tfc objects
 * There isn't a known invalid value for tsid, so this is necessary in order to
 * know that the tsid is not set.
 */
void
bnxt_ulp_cntxt_tsid_reset(struct bnxt_ulp_context *ulp_ctx);

/* Function to set the v3 table scope id, only works for tfc objects */
int
bnxt_ulp_cntxt_tsid_get(struct bnxt_ulp_context *ulp_ctx, u8 *tsid);

/* Function to set the v3 session id, only works for tfc objects */
int
bnxt_ulp_cntxt_sid_set(struct bnxt_ulp_context *ulp_ctx, u16 session_id);

/*
 * Function to reset the v3 session id, only works for tfc objects
 * There isn't a known invalid value for sid, so this is necessary in order to
 * know that the sid is not set.
 */
void
bnxt_ulp_cntxt_sid_reset(struct bnxt_ulp_context *ulp_ctx);

/* Function to get the v3 session id, only works for tfc objects */
int
bnxt_ulp_cntxt_sid_get(struct bnxt_ulp_context *ulp_ctx, u16 *sid);

int
bnxt_ulp_cntxt_fid_get(struct bnxt_ulp_context *ulp, u16 *fid);

struct tf *
bnxt_ulp_bp_tfp_get(struct bnxt *bp, enum bnxt_ulp_session_type type);

/* Get the device table entry based on the device id. */
struct bnxt_ulp_device_params *
bnxt_ulp_device_params_get(u32 dev_id);

int
bnxt_ulp_cntxt_ptr2_mark_db_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_mark_tbl *mark_tbl);

struct bnxt_ulp_mark_tbl *
bnxt_ulp_cntxt_ptr2_mark_db_get(struct bnxt_ulp_context *ulp_ctx);

/* Function to set the flow database to the ulp context. */
int
bnxt_ulp_cntxt_ptr2_flow_db_set(struct bnxt_ulp_context	*ulp_ctx,
				struct bnxt_ulp_flow_db	*flow_db);

/* Function to get the flow database from the ulp context. */
struct bnxt_ulp_flow_db	*
bnxt_ulp_cntxt_ptr2_flow_db_get(struct bnxt_ulp_context	*ulp_ctx);

/* Function to get the tunnel cache table info from the ulp context. */
struct bnxt_tun_cache_entry *
bnxt_ulp_cntxt_ptr2_tun_tbl_get(struct bnxt_ulp_context	*ulp_ctx);

/* Function to get the ulp context from eth device. */
struct bnxt_ulp_context	*
bnxt_ulp_bp_ptr2_cntxt_get(struct bnxt *bp);

/* Function to add the ulp mapper data to the ulp context */
int
bnxt_ulp_cntxt_ptr2_mapper_data_set(struct bnxt_ulp_context *ulp_ctx,
				    void *mapper_data);

/* Function to get the ulp mapper data from the ulp context */
void *
bnxt_ulp_cntxt_ptr2_mapper_data_get(struct bnxt_ulp_context *ulp_ctx);

/* Function to add the ulp matcher data to the ulp context */
int
bnxt_ulp_cntxt_ptr2_matcher_data_set(struct bnxt_ulp_context *ulp_ctx,
				     void *matcher_data);

/* Function to get the ulp matcher data from the ulp context */
void *
bnxt_ulp_cntxt_ptr2_matcher_data_get(struct bnxt_ulp_context *ulp_ctx);

/* Function to set the port database to the ulp context. */
int
bnxt_ulp_cntxt_ptr2_port_db_set(struct bnxt_ulp_context	*ulp_ctx,
				struct bnxt_ulp_port_db	*port_db);

/* Function to get the port database from the ulp context. */
struct bnxt_ulp_port_db *
bnxt_ulp_cntxt_ptr2_port_db_get(struct bnxt_ulp_context	*ulp_ctx);

/* Function to create default flows. */
int
ulp_default_flow_create(struct bnxt *bp,
			struct ulp_tlv_param *param_list,
			u32 ulp_class_tid,
			u16 port_id,
			u32 *flow_id);

/* Function to destroy default flows. */
int
ulp_default_flow_destroy(struct bnxt *bp,
			 u32 flow_id);

int
bnxt_ulp_cntxt_ptr2_fc_info_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_fc_info *ulp_fc_info);

struct bnxt_ulp_fc_info *
bnxt_ulp_cntxt_ptr2_fc_info_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_ptr2_ulp_flags_get(struct bnxt_ulp_context *ulp_ctx,
				  u32 *flags);

int
bnxt_ulp_get_df_rule_info(u8 port_id, struct bnxt_ulp_context *ulp_ctx,
			  struct bnxt_ulp_df_rule_info *info);

struct bnxt_ulp_vfr_rule_info*
bnxt_ulp_cntxt_ptr2_ulp_vfr_info_get(struct bnxt_ulp_context *ulp_ctx,
				     u32 port_id);

int
bnxt_ulp_cntxt_acquire_fdb_lock(struct bnxt_ulp_context	*ulp_ctx);

void
bnxt_ulp_cntxt_release_fdb_lock(struct bnxt_ulp_context	*ulp_ctx);

struct bnxt_ulp_shared_act_info *
bnxt_ulp_shared_act_info_get(u32 *num_entries);

struct bnxt_ulp_glb_resource_info *
bnxt_ulp_app_glb_resource_info_list_get(u32 *num_entries);

int
bnxt_ulp_cntxt_app_id_set(struct bnxt_ulp_context *ulp_ctx, u8 app_id);

int
bnxt_ulp_cntxt_app_id_get(struct bnxt_ulp_context *ulp_ctx, u8 *app_id);

bool
bnxt_ulp_cntxt_shared_session_enabled(struct bnxt_ulp_context *ulp_ctx);

bool
bnxt_ulp_cntxt_multi_shared_session_enabled(struct bnxt_ulp_context *ulp_ctx);

struct bnxt_ulp_app_capabilities_info *
bnxt_ulp_app_cap_list_get(u32 *num_entries);

struct bnxt_ulp_resource_resv_info *
bnxt_ulp_app_resource_resv_list_get(u32 *num_entries);

struct bnxt_ulp_resource_resv_info *
bnxt_ulp_resource_resv_list_get(u32 *num_entries);

bool
bnxt_ulp_cntxt_ha_enabled(struct bnxt_ulp_context *ulp_ctx);

struct bnxt_ulp_context *
bnxt_ulp_cntxt_entry_lookup(void *cfg_data);

void
bnxt_ulp_cntxt_lock_acquire(void);

void
bnxt_ulp_cntxt_lock_release(void);

int
bnxt_ulp_cntxt_num_shared_clients_set(struct bnxt_ulp_context *ulp_ctx,
				      bool incr);

struct bnxt_flow_app_tun_ent *
bnxt_ulp_cntxt_ptr2_app_tun_list_get(struct bnxt_ulp_context *ulp);

/* Function to get the truflow app id. This defined in the build file */
u32
bnxt_ulp_default_app_id_get(void);

int
bnxt_ulp_vxlan_port_set(struct bnxt_ulp_context *ulp_ctx,
			u32 vxlan_port);
unsigned int
bnxt_ulp_vxlan_port_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_vxlan_ip_port_set(struct bnxt_ulp_context *ulp_ctx,
			   u32 vxlan_ip_port);
unsigned int
bnxt_ulp_vxlan_ip_port_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_ecpri_udp_port_set(struct bnxt_ulp_context *ulp_ctx,
			    u32 ecpri_udp_port);

unsigned int
bnxt_ulp_ecpri_udp_port_get(struct bnxt_ulp_context *ulp_ctx);

u32
bnxt_ulp_vxlan_gpe_next_proto_set(struct bnxt_ulp_context *ulp_ctx,
				  u8 tunnel_next_proto);

u8
bnxt_ulp_vxlan_gpe_next_proto_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_vxlan_port_set(struct bnxt_ulp_context *ulp_ctx,
			      u32 vxlan_port);
unsigned int
bnxt_ulp_cntxt_vxlan_port_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_default_app_priority_set(struct bnxt_ulp_context *ulp_ctx, u32 prio);

unsigned int
bnxt_ulp_default_app_priority_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_max_def_priority_set(struct bnxt_ulp_context *ulp_ctx, u32 prio);

unsigned int
bnxt_ulp_max_def_priority_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_min_flow_priority_set(struct bnxt_ulp_context *ulp_ctx, u32 prio);

unsigned int
bnxt_ulp_min_flow_priority_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_max_flow_priority_set(struct bnxt_ulp_context *ulp_ctx, u32 prio);

unsigned int
bnxt_ulp_max_flow_priority_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_vxlan_ip_port_set(struct bnxt_ulp_context *ulp_ctx,
				 u32 vxlan_ip_port);
unsigned int
bnxt_ulp_cntxt_vxlan_ip_port_get(struct bnxt_ulp_context *ulp_ctx);
int
bnxt_ulp_cntxt_ecpri_udp_port_set(struct bnxt_ulp_context *ulp_ctx,
				  u32 ecpri_udp_port);
unsigned int
bnxt_ulp_cntxt_ecpri_udp_port_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_flow_meter_init(struct bnxt *bp);

int
bnxt_flow_meter_deinit(struct bnxt *bp);

u32
bnxt_ulp_cntxt_convert_dev_id(struct bnxt *bp, u32 ulp_dev_id);

struct tf *
bnxt_ulp_bp_tfp_get(struct bnxt *bp, enum bnxt_ulp_session_type type);

int
bnxt_ulp_cntxt_ha_reg_set(struct bnxt_ulp_context *ulp_ctx,
			  u8 state, u8 cnt);

u32
bnxt_ulp_cntxt_ha_reg_state_get(struct bnxt_ulp_context *ulp_ctx);

u32
bnxt_ulp_cntxt_ha_reg_cnt_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cntxt_list_init(void);

int
bnxt_ulp_cntxt_list_add(struct bnxt_ulp_context *ulp_ctx);

void
bnxt_ulp_cntxt_list_del(struct bnxt_ulp_context *ulp_ctx);

void
bnxt_ulp_num_key_recipes_set(struct bnxt_ulp_context *ulp_ctx,
			     u16 recipes);

int
bnxt_ulp_num_key_recipes_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_create_df_rules(struct bnxt *bp);

void
bnxt_ulp_destroy_df_rules(struct bnxt *bp, bool global);

int32_t
ulp_flow_template_process(struct bnxt *bp,
			  struct ulp_tlv_param *param_list,
			  u64 *comp_fld,
			  u32 ulp_class_tid,
			  u16 port_id,
			  u32 flow_id);

/* Function to check if allowing multicast and broadcast flow offload. */
bool
bnxt_ulp_validate_bcast_mcast(struct bnxt *bp);

u64
bnxt_ulp_feature_bits_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_vfr_session_fid_add(struct bnxt_ulp_context *ulp_ctx,
			     u16 vfr_fid);
int
bnxt_ulp_vfr_session_fid_rem(struct bnxt_ulp_context *ulp_ctx,
			     u16 vfr_fid);
void
bnxt_ulp_cntxt_ptr2_default_class_bits_set(struct bnxt_ulp_context *ulp_ctx,
					   u64 bits);

u64
bnxt_ulp_cntxt_ptr2_default_class_bits_get(struct bnxt_ulp_context *ulp_ctx);

void
bnxt_ulp_cntxt_ptr2_default_act_bits_set(struct bnxt_ulp_context *ulp_ctx,
					 u64 bits);
u64
bnxt_ulp_cntxt_ptr2_default_act_bits_get(struct bnxt_ulp_context *ulp_ctx);

int
bnxt_ulp_cap_feat_process(u64 feat_bits, u64 def_feat_bits, u64 *out_bits);

/* Function to set the flow counter info into the context */
int
bnxt_ulp_cntxt_ptr2_sc_info_set(struct bnxt_ulp_context *ulp_ctx,
				struct bnxt_ulp_sc_info *ulp_sc_info);

/* Function to retrieve the flow counter info from the context. */
struct bnxt_ulp_sc_info *
bnxt_ulp_cntxt_ptr2_sc_info_get(struct bnxt_ulp_context *ulp_ctx);

int bnxt_ulp_pf_veb_flow_create(struct bnxt *bp);
int bnxt_ulp_pf_veb_flow_delete(struct bnxt *bp);
#endif /* _BNXT_ULP_H_ */
