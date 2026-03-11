// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2024 Broadcom
 * All rights reserved.
 */
#include <linux/debugfs.h>
#include <linux/slab.h>

#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_sriov.h"
#include <bnxt_tfc.h>

#include "tfc.h"
#include "tfo.h"
#include "tfc_em.h"
#include "cfa_types.h"

#include "sys_util.h"
#include "tfc_util.h"
#include "ulp_flow_db.h"
#include "bnxt_tf_ulp_p7.h"
#include "tfc_debug.h"

#define TFC_STRING_LENGTH_32  32
#define TFC_STRING_LENGTH_64  64
#define TFC_STRING_LENGTH_96  96
#define TFC_STRING_LENGTH_256 256

/* Enable this flag if you want to dump all TCAM records,
 * including the default L2 context records and profile TCAM
 * entries. This method is sub-optimal, but can used for lack of
 * a better way to walk and dump flow DB resources for particular
 * flow types.
 * Disabling this flag will dump WC TCAM entries and their
 * associated action-records by default.
 */
#define TFC_DEBUG_DUMP_ALL_FLOWS 1

/*
 * Function pointer type for custom processing resources
 */
typedef int (*FDB_RESOURCE_PROCFUNC)(struct ulp_flow_db_res_params *rp,
				     void *frp_ctxt);
extern
void act_process(struct seq_file *m,
		 uint32_t act_rec_ptr,
		 struct act_info_t *act_info,
		 struct tfc_ts_mem_cfg *act_mem_cfg);
extern
void act_show(struct seq_file *m, struct act_info_t *act_info, uint32_t offset);

static
void hex_buf_dump(struct seq_file *m, const char *hdr, u8 *msg,
		  int msglen, int prtwidth, int linewidth);

struct wc_frp_context {
	struct seq_file *m;
	struct bnxt_ulp_context *ulp_ctxt;
	struct tfc_ts_mem_cfg *act_mem_cfg;
};

struct wc_lrec_t {
	bool valid;
	u8 rec_size;
	u16 epoch0;
	u16 epoch1;
	u8 opcode;
	u8 strength;
	u8 act_hint;
	u32 act_rec_ptr;	/* Not FAST */
	u32 destination;	/* Just FAST */
	u8 tcp_direction;	/* Just CT */
	u8 tcp_update_en;
	u8 tcp_win;
	u32 tcp_msb_loc;
	u32 tcp_msb_opp;
	u8 tcp_msb_opp_init;
	u8 state;
	u8 timer_value;
	u16 ring_table_idx;	/* Not CT and not RECYCLE */
	u8 act_rec_size;
	u8 paths_m1;
	u8 fc_op;
	u8 fc_type;
	u32 fc_ptr;
	u8 recycle_dest;	/* Just Recycle */
	u8 prof_func;
	u8 meta_prof;
	u32 metadata;
	u8 range_profile;
	u16 range_index;
	struct act_info_t act_info;
};

/* L2 context TCAM key formats
 *
 * IPv4
 * ----
 * valid                       255       1   TCAM entry is valid
 * spare                       254:253   2   Spare bits.
 * mpass_cnt                   252:251   2   Multi-pass cycle count ? {0,1,2,3}
 * rcyc[3:0]                   250:247   4   Recycle count from prof_in
 * loopback                    246       1   loopback input from prof_in
 * spif                        245:244   2   Source network port from prof_in
 * parif                       243:239   5   Partition provided by input block
 * svif                        238:228   11  Source of the packet: Ethernet network port or
 *                                           vnic; provided on prof_in
 * metadata                    227:196   32  Metadata provided by Input block
 * l2ip_func                   195:188   8   Used to create logical (feature specific) context
 *                                           TCAM tables. Provided from ILT or Recycle.
 * roce                        187       1   ROCE Packet detected by the Parser
 * pure_llc                    186       1   Pure LLC Packet detected by the Parser. If set
 *                                           the etype field will contain the DSAP/SSAP from
 *                                           LLC header.
 * ot_hdr_type                 185:181   5   5b encoded Outer Tunnel Type (see Table 4-12)
 * t_hdr_type                  180:176   5   5b encoded Tunnel Type (see Table 4-12)
 * tunnel_id/context/L4        175:144   32  Tunnel ID/Tunnel Context/L4 ports selected.
 * ADDR0                       143:96    48  ADDR0: DMAC/SMAC/IPv4 selected.
 * ADDR1                       95:48     48  ADDR1: DMAC/SMAC/IPv4 selected.
 * otl2/tl2/l2_vtag_present    47        1   1+ VLAN tags present (L2 selected)
 * otl2/tl2/l2_two_vtags       46        1   2 VLAN tags present (comp. flds_num_vtags)
 * otl2/tl2/l2_ovlan_vid       45:34     12  VID from outer VLAN tag if present (L2 selected)
 * otl2/tl2/l2_ovlan_tpid_sel  33:31     3   3b encoding for TPID (L2 selected)
 * otl2/tl2/l2_ivlan_vid       30:19     12  VID from inner VLAN tag if present (L2 selected)
 * otl2/tl2/l2_ivlan_tpid_sel  18:16     3   3b encoding for TPID (L2 selected)
 * otl2/tl2/l2_etype           15:0      16  L2 Header Ethertype (L2 selected)
 *
 * IPv6
 * ----
 * valid                 255      1    TCAM entry is valid
 * spare                 254:253  2    Spare bits.
 * mpass_cnt             252:251  2    Multi-pass cycle count ? {0,1,2,3}
 * rcyc[3:0]             250:247  4    Recycle count from prof_in
 * loopback              246      1    loopback input from prof_in
 * spif                  245:244  2    Source network port from prof_in
 * parif                 243:239  5    Partition provided by input block
 * svif                  238:228  11   Source of the packet: Ethernet network port or
 *                                     vnic; provided on prof_in
 * metadata              227:196  32   Metadata provided by Input block
 * l2ip_func             195:188  8    Used to create logical (feature specific) context
 *                                     TCAM tables. Provided from ILT or Recycle.
 * roce                  187      1    ROCE Packet detected by the Parser
 * pure_llc              186      1    Pure LLC Packet detected by the Parser. If set
 *                                     the etype field will contain the DSAP/SSAP from
 *                                     LLC header.
 * ot_hdr_type           185:181  5    5b encoded Outer Tunnel Type (see Table 4-12)
 * t_hdr_type            180:176  5    5b encoded Tunnel Type (see Table 4-12)
 * tunnel_id/context/L4  175:144  32   Tunnel ID/Tunnel Context/L4 ports selected.
 * ADDR0                 143:16   128  ADDR0: IPv6 selected.
 * otl2/tl2/l2_etype     15:0     16   L2 Header Ethertype (L2 selected)
 */
struct l2ctx_tcam_key_t {
	u8 valid;
	u8 spare;
	u8 mpass_cnt;
	u8 rcyc;
	u8 loopback;
	u8 spif;
	u8 parif;
	u16 svif;
	u32 metadata;
	u8 l2ip_func;
	u8 roce;
	u8 pure_llc;
	u8 ot_hdr_type;
	u8 t_hdr_type;
	u32 tunnel_id_context_L4;
	union {
		struct ipv4_key_t {
			u64 ADDR0;
			u64 ADDR1;
			u8 otl2_tl2_l2_vtag_present;
			u8 otl2_tl2_l2_two_vtags;
			u16 otl2_tl2_l2_ovlan_vid;
			u8 otl2_tl2_l2_ovlan_tpid_sel;
			u16 otl2_tl2_l2_ivlan_vid;
			u8 otl2_tl2_l2_ivlan_tpid_sel;
			u16 otl2_tl2_l2_etype;
		} ipv4;
		struct ipv6_key_t {
			u64 ADDR0[2];
			u16 otl2_tl2_l2_etype;
		} ipv6;
	};
};

/* L2 context TCAM remap
 *
 * prsv_parif      126      1   Preserve incoming partition, i.e. don?t remap.
 * parif           125:121  5   Partition. Replaces parif from Input block
 * prsv_l2ip_ctxt  120      1   Preserve incoming l2ip_ctxt, i.e. don?t remap.
 * l2ip_ctxt       119:109  11  May be used in EM and WC Lookups to support logical
 *                              partitions of these tables
 * prsv_prof_func  108      1   Preserve incoming PROF_FUNC, i.e. don?t remap.
 * prof_func       107:100  8   Allow Profile TCAM Lookup Table to be logically partitioned.
 * ctxt_opcode     99:98    2   0: BYPASS_CFA
 *                              1: BYPASS_LKUP
 *                              2: NORMAL_FLOW
 *                              3: DROP
 * l2ip_meta_enb   97       1   Enables remap of meta_data from Input block.
 * l2ip_meta       96:62    35  l2ip_meta_prof[2:0] = l2ip_meta[34:32]
 *                              l2ip_meta_data[31:0] = l2ip_meta[31:0]
 * l2ip_act_enb    61       1   Enables remap of Action Record pointer from Input block.
 * l2ip_act_data   60:28    33  l2ip_act_hint[1:0] = l2ip_act_data[32:31]
 *                              l2ip_act_scope[4:0] = l2ip_act_data[30:26]
 *                              l2ip_act_rec_ptr[25:0] = l2ip_act_data[25:0]
 * l2ip_rfs_enb    27       1   Enables remap of ring_table_idx and sets rfs_valid.
 * l2ip_rfs_data   26:18    9   ring_table_idx[8:0] = l2ip_rfs_data[8:0] (RX only)
 * l2ip_dest_enb   17       1   Enables remap of destination from Input block.
 * l2ip_dest_data  16:0     17  destination[16:0] = l2ip_dest_data[16:0]
 */
struct l2ctx_tcam_remap_t {
	u8 prsv_parif;
	u8 parif;
	u8 prsv_l2ip_ctxt;
	u16 l2ip_ctxt;
	u8 prsv_prof_func;
	u8 prof_func;
	u8 ctxt_opcode;
	u8 l2ip_meta_enb;
	u8 l2ip_meta_prof;
	u32 l2ip_meta_data;
	u8 l2ip_act_enb;
	u8 l2ip_act_hint;
	u8 l2ip_act_scope;
	u32 l2ip_act_ptr;
	u8 l2ip_rfs_enb;
	u16 l2ip_rfs_data;
	u8 l2ip_dest_enb;
	u32 l2ip_dest_data;
	struct act_info_t act_info;
};

/* Profile TCAM key
 *
 * valid                 183      1  Valid(1)/Invalid(0) TCAM entry.
 * spare                 182:181  2  Spare bits.
 * loopback              180      1  END.loopback
 * pkt_type              179:176  4  Packet type directly from END bus.
 * rcyc[3:0]             175:172  4  Recycle count from prof_in
 * metadata              171:140  32 From previous stage.
 * agg_error             139      1  Aggregate error flag from Input stage.
 * l2ip_func             138:131  8  L2-IP Context function from Input Lookup stage.
 * prof_func             130:123  8  Profile function from L2-IP Context Lookup stage.
 * hrec_next             122:121  2  From FLDS Input, General Status
 *                                   1=tunnel/0=no tunnel
 * int_hdr_type          120:119  2  INT header type directly from FLDS.
 * int_hdr_group         118:117  2  INT header group directly from FLDS.
 * int_ifa_tail          116      1  INT metadata is tail stamp.
 * otl2_hdr_valid        115      1  !(flds_otl2_hdr_valid==stop_w_error |
 *                                     flds_otl2_hdr_valid==not_reached)
 * otl2_hdr_type         114:113  2  Outer Tunnel L2 header type directly from FLDS.
 * otl2_uc_mc_bc         112:111  2  flds_otl2_dst_type remapped: UC(0)/MC(2)/BC(3)
 * otl2_vtag_present     110      1  1+ VLAN tags present (comp. lds_otl2_num_vtags)
 * otl2_two_vtags        109      1  2 VLAN tags present (comp. flds_otl2_num_vtags)
 * otl3_hdr_valid        108      1  !(flds_otl3_hdr_valid== stop_w_error |
 *                                     flds_otl3_hdr_valid== not_reached )
 * otl3_hdr_error        107      1  flds_tl3_hdr_valid == stop_w_error
 * otl3_hdr_type         106:103  4  Outer Tunnel L3 header type directly from FLDS.
 * otl3_hdr_isip         102      1  Outer Tunnel L3 header is IPV4 or IPV6.
 * otl4_hdr_valid        101      1  !(flds_otl4_hdr_valid== stop_w_error |
 *                                     flds_otl4_hdr_valid== not_reached )
 * otl4_hdr_error        100      1  flds_otl4_hdr_valid == stop_w_error
 * otl4_hdr_type         99:96    4  Outer Tunnel L4 header type directly from FLDS.
 * otl4_hdr_is_udp_tcp   95       1  OTL4 header is UDP-TCP. (comp. flds_otl4_hdr_type)
 * ot_hdr_valid          94       1  !(flds_ot_hdr_valid== stop_w_error |
 *                                     flds_ot_hdr_valid== not_reached )
 * ot_hdr_error          93       1  flds_ot_hdr_valid == stop_w_error
 * ot_hdr_type           92:88    5  Outer Tunnel header type directly from FLDS.
 * ot_hdr_flags          87:80    8  Outer Tunnel header flags directly from FLDS.
 * tl2_hdr_valid         79       1  !(flds_tl2_hdr_valid==stop_w_error |
 *                                     flds_tl2_hdr_valid==not_reached)
 * tl2_hdr_type          78:77    2  Tunnel L2 header type directly from FLDS.
 * tl2_uc_mc_bc          76:75    2  flds_tl2_dst_type remapped: UC(0)/MC(2)/BC(3)
 * tl2_vtag_present      74       1  1+ VLAN tags present (comp. lds_tl2_num_vtags)
 * tl2_two_vtags         73       1  2 VLAN tags present (comp. flds_tl2_num_vtags)
 * tl3_hdr_valid         72       1  !(flds_tl3_hdr_valid== stop_w_error |
 *                                     flds_tl3_hdr_valid== not_reached )
 * tl3_hdr_error         71       1  flds_tl3_hdr_valid == stop_w_error
 * tl3_hdr_type          70:67    4  Tunnel L3 header type directly from FLDS.
 * tl3_hdr_isip          66       1  Tunnel L3 header is IPV4 or IPV6.
 * tl4_hdr_valid         65       1  !(flds_tl4_hdr_valid== stop_w_error |
 *                                     flds_tl4_hdr_valid== not_reached )
 * tl4_hdr_error         64       1  flds_tl4_hdr_valid == stop_w_error
 * tl4_hdr_type          63:60    4  Tunnel L4 header type directly from FLDS.
 * tl4_hdr_is_udp_tcp    59       1  TL4 header is UDP or TCP. (comp. flds_tl4_hdr_type)
 * t_hdr_valid           58       1  !(flds_tun_hdr_valid== stop_w_error |
 *                                     flds_tun_hdr_valid== not_reached )
 * t_hdr_error           57       1  flds_tun_hdr_valid == stop_w_error
 * t_hdr_type            56:52    5  Tunnel header type directly from FLDS.
 * t_hdr_flags           51:44    8  Tunnel header flags directly from FLDS.
 * l2_hdr_valid          43       1  !(flds_l2_hdr_valid== stop_w_error |
 *                                     flds_l2_hdr_valid== not_reached )
 * l2_hdr_error          42       1  flds_l2_hdr_valid == stop_w_error
 * l2_hdr_type           41:40    2  L2 header type directly from FLDS.
 * l2_uc_mc_bc           39:38    2  flds_l2_dst_type remapped: UC(0)/MC(2)/BC(3)
 * l2_vtag_present       37       1  1+ VLAN tags present (comp. flds_l2_num_vtags)
 * l2_two_vtags          36       1  2 VLAN tags present (comp. flds_l2_num_vtags)
 * l3_hdr_valid          35       1  !(flds_l3_hdr_valid== stop_w_error |
 *                                     flds_l3_hdr_valid== not_reached )
 * l3_hdr_error          34       1  flds_l3_hdr_valid == stop_w_error
 * l3_hdr_type           33:30    4  L3 header type directly from FLDS.
 * l3_hdr_isip           29       1  L3 header is IPV4 or IPV6.
 * l3_protocol           28:21    8  L3 header next protocol directly from FLDS.
 * l4_hdr_valid          20       1  !(flds_l4_hdr_valid== stop_w_error |
 *                                     flds_l4_hdr_valid== not_reached )
 * l4_hdr_error          19       1  flds_l4_hdr_valid == stop_w_error
 * l4_hdr_type           18:15    4  L4 header type directly from FLDS.
 * l4_hdr_is_udp_tcp     14       1  L4 header is UDP or TCP (comp. flds_l4_hdr_type)
 * l4_hdr_subtype        13:11    3  L4 header sub-type directly from FLDS.
 * l4_flags              10:2     9  L4 header flags directly from FLDS.
 * l4_dcn_present        1:0      2  DCN present bits directly from L4 header FLDS.
 */

struct prof_tcam_key_t {
	u8 valid;
	u8 spare;
	u8 loopback;
	u8 pkt_type;
	u8 rcyc;
	u32 metadata;
	u8 agg_error;
	u8 l2ip_func;
	u8 prof_func;
	u8 hrec_next;
	u8 int_hdr_type;
	u8 int_hdr_group;
	u8 int_ifa_tail;
	u8 otl2_hdr_valid;
	u8 otl2_hdr_type;
	u8 otl2_uc_mc_bc;
	u8 otl2_vtag_present;
	u8 otl2_two_vtags;
	u8 otl3_hdr_valid;
	u8 otl3_hdr_error;
	u8 otl3_hdr_type;
	u8 otl3_hdr_isip;
	u8 otl4_hdr_valid;
	u8 otl4_hdr_error;
	u8 otl4_hdr_type;
	u8 otl4_hdr_is_udp_tcp;
	u8 ot_hdr_valid;
	u8 ot_hdr_error;
	u8 ot_hdr_type;
	u8 ot_hdr_flags;
	u8 tl2_hdr_valid;
	u8 tl2_hdr_type;
	u8 tl2_uc_mc_bc;
	u8 tl2_vtag_present;
	u8 tl2_two_vtags;
	u8 tl3_hdr_valid;
	u8 tl3_hdr_error;
	u8 tl3_hdr_type;
	u8 tl3_hdr_isip;
	u8 tl4_hdr_valid;
	u8 tl4_hdr_error;
	u8 tl4_hdr_type;
	u8 tl4_hdr_is_udp_tcp;
	u8 t_hdr_valid;
	u8 t_hdr_error;
	u8 t_hdr_type;
	u8 t_hdr_flags;
	u8 l2_hdr_valid;
	u8 l2_hdr_error;
	u8 l2_hdr_type;
	u8 l2_uc_mc_bc;
	u8 l2_vtag_present;
	u8 l2_two_vtags;
	u8 l3_hdr_valid;
	u8 l3_hdr_error;
	u8 l3_hdr_type;
	u8 l3_hdr_isip;
	u8 l3_protocol;
	u8 l4_hdr_valid;
	u8 l4_hdr_error;
	u8 l4_hdr_type;
	u8 l4_hdr_is_udp_tcp;
	u8 l4_hdr_subtype;
	u16 l4_flags;
	u8 l4_dcn_present;
};

/*
 * Profile TCAM remap record:
 *
 * pl_byp_lkup_en   1  42     When set to ?0? remaining bits are defined below.
 * em_search_en     1  41     Enable search in EM database
 * em_profile_id    8  40:33  Selected key structure for EM search. This is used as part of
 *                            the EM keys to differentiate common key types.
 * em_key_id        7  32:26  Exact match key template select
 * em_scope         5  25:21  Exact Match Lookup scope. Action scope on EM hit.
 * tcam_search_en   1  20     Enable search in TCAM database
 * tcam_profile_id  8  19:12  Selected key structure for TCAM search. This is used as part of
 *                            the TCAM keys to differentiate common key types.
 * tcam_key_id      7  11:5   TCAM key template select
 * tcam_scope       5  4:0    Wild Card Lookup Action table scope (used if WC hits).
 */
struct prof_tcam_remap_t {
	bool pl_byp_lkup_en;
	bool em_search_en;
	u8 em_profile_id;
	u8 em_key_id;
	u8 em_scope;
	bool tcam_search_en;
	u8 tcam_profile_id;
	u8 tcam_key_id;
	u8 tcam_scope;
};

/* Internal function to read the tcam entry */
static int
tfc_tcam_entry_read(struct bnxt_ulp_context *ulp_ctxt,
		    u8 dir,
		    u8 res_type,
		    u16 res_idx,
		    u8 *key,
		    u8 *mask,
		    u8 *remap,
		    u16 *key_size,
		    u16 *remap_size)
{
	struct tfc_tcam_info tfc_info = {0};
	struct tfc_tcam_data tfc_data = {0};
	struct tfc *tfcp = NULL;
	u16 fw_fid;
	int rc;

	tfcp = bnxt_ulp_cntxt_tfcp_get(ulp_ctxt, BNXT_ULP_SESSION_TYPE_DEFAULT);
	if (!tfcp) {
		netdev_dbg(ulp_ctxt->bp->dev, "Failed to get tfcp pointer\n");
		return -EINVAL;
	}

	rc = bnxt_ulp_cntxt_fid_get(ulp_ctxt, &fw_fid);
	if (rc)
		return rc;

	tfc_info.dir = dir;
	tfc_info.rsubtype = res_type;
	tfc_info.id = res_idx;

	tfc_data.key = key;
	tfc_data.mask = mask;
	tfc_data.remap = remap;
	tfc_data.key_sz_in_bytes = *key_size;
	tfc_data.remap_sz_in_bytes = *remap_size;

	if (tfc_tcam_get(tfcp, fw_fid, &tfc_info, &tfc_data)) {
		netdev_err(ulp_ctxt->bp->dev, "tcam[%s][%s][%x] read failed.\n",
			   tfc_tcam_2_str(tfc_info.rsubtype),
			   tfc_dir_2_str(tfc_info.dir), tfc_info.id);
		return -EIO;
	}

	*key_size = (u16)tfc_data.key_sz_in_bytes;
	*remap_size = (u16)tfc_data.remap_sz_in_bytes;

	return rc;
}

/*
 * bnxt_tfc_buf_dump: Pretty-prints a buffer using the following options
 *
 * Parameters:
 * hdr       - A header that is printed as-is
 * msg       - This is a pointer to the uint8_t buffer to be dumped
 * prtwidth  - The width of the words to be printed, allowed options 1, 2, 4
 *             Defaults to 1 if either:
 *             1) any other value
 *             2) if buffer length is not a multiple of width
 * linewidth - The length of the lines printed (in items/words)
 */
static
void hex_buf_dump(struct seq_file *m, const char *hdr, u8 *msg,
		  int msglen, int prtwidth, int linewidth)
{
	char msg_line[128];
	int msg_i = 0, i;
	uint16_t *sw_msg = (uint16_t *)msg;
	uint32_t *lw_msg = (uint32_t *)msg;

	if (hdr)
		seq_printf(m, "%s\n", hdr);

	if (msglen % prtwidth) {
		seq_printf(m, "msglen[%u] not aligned on width[%u]\n",
			   msglen, prtwidth);
		prtwidth = 1;
	}

	for (i = 0; i < msglen / prtwidth; i++) {
		if ((i % linewidth == 0) && i)
			seq_printf(m, "%s\n", msg_line);
		if (i % linewidth == 0) {
			msg_i = 0;
			msg_i += snprintf(&msg_line[msg_i],
					  (sizeof(msg_line) - msg_i),
					  "0x%04x: ", (i * prtwidth));
		}
		switch (prtwidth) {
		case 2:
			msg_i += snprintf(&msg_line[msg_i],
					  (sizeof(msg_line) - msg_i),
					  "0x%04x ", sw_msg[i]);
			break;

		case 4:
			msg_i += snprintf(&msg_line[msg_i],
					  (sizeof(msg_line) - msg_i),
					  "0x%08x ", lw_msg[i]);
			break;

		case 1:
		default:
			msg_i += snprintf(&msg_line[msg_i],
					  (sizeof(msg_line) - msg_i),
					  "0x%02x ", msg[i]);
			break;
		}
	}
	seq_printf(m, "%s\n", msg_line);
}

#define L2CTX_KEY_INFO_VALID(kptr)				tfc_getbits(kptr, 255, 1)
#define L2CTX_KEY_INFO_SPARE(kptr)				tfc_getbits(kptr, 253, 2)
#define L2CTX_KEY_INFO_MPASS_CNT(kptr)				tfc_getbits(kptr, 251, 2)
#define L2CTX_KEY_INFO_RCYC(kptr)				tfc_getbits(kptr, 247, 4)
#define L2CTX_KEY_INFO_LOOPBACK(kptr)				tfc_getbits(kptr, 246, 1)
#define L2CTX_KEY_INFO_SPIF(kptr)				tfc_getbits(kptr, 244, 2)
#define L2CTX_KEY_INFO_PARIF(kptr)				tfc_getbits(kptr, 239, 5)
#define L2CTX_KEY_INFO_SVIF(kptr)				tfc_getbits(kptr, 228, 11)
#define L2CTX_KEY_INFO_METADATA(kptr)				tfc_getbits(kptr, 196, 32)
#define L2CTX_KEY_INFO_L2IP_FUNC(kptr)				tfc_getbits(kptr, 188, 8)
#define L2CTX_KEY_INFO_ROCE(kptr)				tfc_getbits(kptr, 187, 1)
#define L2CTX_KEY_INFO_PURE_LLC(kptr)				tfc_getbits(kptr, 186, 1)
#define L2CTX_KEY_INFO_OT_HDR_TYPE(kptr)			tfc_getbits(kptr, 181, 5)
#define L2CTX_KEY_INFO_T_HDR_TYPE(kptr)				tfc_getbits(kptr, 176, 5)
#define L2CTX_KEY_INFO_TUNNEL_ID_CONTEXT_L4(kptr)		tfc_getbits(kptr, 144, 32)

#define L2CTX_KEY_INFO_IPV6_ADDR0_1(kptr)			tfc_getbits(kptr, 80, 64)
#define L2CTX_KEY_INFO_IPV6_ADDR0_0(kptr)			tfc_getbits(kptr, 16, 64)
#define L2CTX_KEY_INFO_IPV6_OTL2_TL2_L2_ETYPE(kptr)		tfc_getbits(kptr, 0, 16)

#define L2CTX_KEY_INFO_IPV4_ADDR0(kptr)				tfc_getbits(kptr, 96, 48)
#define L2CTX_KEY_INFO_IPV4_ADDR1(kptr)				tfc_getbits(kptr, 48, 48)
#define L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_VTAG_PRESENT(kptr)	tfc_getbits(kptr, 47, 1)
#define L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_TWO_VTAGS(kptr)		tfc_getbits(kptr, 46, 1)
#define L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_OVLAN_VID(kptr)		tfc_getbits(kptr, 34, 12)
#define L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_OVLAN_TPID_SEL(kptr)	tfc_getbits(kptr, 31, 3)
#define L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_IVLAN_VID(kptr)		tfc_getbits(kptr, 19, 12)
#define L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_IVLAN_TPID_SEL(kptr)	tfc_getbits(kptr, 16, 3)
#define L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_ETYPE(kptr)		tfc_getbits(kptr, 0, 16)

static void l2ctx_tcam_key_decode(struct seq_file *m,
				  uint32_t *l2ctx_key_ptr,
				  struct l2ctx_tcam_key_t *l2ctx_key_info)
{
	l2ctx_key_info->valid                = L2CTX_KEY_INFO_VALID(l2ctx_key_ptr);
	l2ctx_key_info->spare                = L2CTX_KEY_INFO_SPARE(l2ctx_key_ptr);
	l2ctx_key_info->mpass_cnt            = L2CTX_KEY_INFO_MPASS_CNT(l2ctx_key_ptr);
	l2ctx_key_info->rcyc                 = L2CTX_KEY_INFO_RCYC(l2ctx_key_ptr);
	l2ctx_key_info->loopback             = L2CTX_KEY_INFO_LOOPBACK(l2ctx_key_ptr);
	l2ctx_key_info->spif                 = L2CTX_KEY_INFO_SPIF(l2ctx_key_ptr);
	l2ctx_key_info->parif                = L2CTX_KEY_INFO_PARIF(l2ctx_key_ptr);
	l2ctx_key_info->svif                 = L2CTX_KEY_INFO_SVIF(l2ctx_key_ptr);
	l2ctx_key_info->metadata             = L2CTX_KEY_INFO_METADATA(l2ctx_key_ptr);
	l2ctx_key_info->l2ip_func            = L2CTX_KEY_INFO_L2IP_FUNC(l2ctx_key_ptr);
	l2ctx_key_info->roce                 = L2CTX_KEY_INFO_ROCE(l2ctx_key_ptr);
	l2ctx_key_info->pure_llc             = L2CTX_KEY_INFO_PURE_LLC(l2ctx_key_ptr);
	l2ctx_key_info->ot_hdr_type          = L2CTX_KEY_INFO_OT_HDR_TYPE(l2ctx_key_ptr);
	l2ctx_key_info->t_hdr_type           = L2CTX_KEY_INFO_T_HDR_TYPE(l2ctx_key_ptr);
	l2ctx_key_info->tunnel_id_context_L4 = L2CTX_KEY_INFO_TUNNEL_ID_CONTEXT_L4(l2ctx_key_ptr);

	if (l2ctx_key_info->t_hdr_type == 0x5 ||
	    l2ctx_key_info->ot_hdr_type == 0x5) {
		l2ctx_key_info->ipv6.ADDR0[1] = L2CTX_KEY_INFO_IPV6_ADDR0_1(l2ctx_key_ptr);
		l2ctx_key_info->ipv6.ADDR0[0] = L2CTX_KEY_INFO_IPV6_ADDR0_0(l2ctx_key_ptr);
		l2ctx_key_info->ipv6.otl2_tl2_l2_etype =
					L2CTX_KEY_INFO_IPV6_OTL2_TL2_L2_ETYPE(l2ctx_key_ptr);
	} else {
		l2ctx_key_info->ipv4.ADDR0 = L2CTX_KEY_INFO_IPV4_ADDR0(l2ctx_key_ptr);
		l2ctx_key_info->ipv4.ADDR1 = L2CTX_KEY_INFO_IPV4_ADDR1(l2ctx_key_ptr);
		l2ctx_key_info->ipv4.otl2_tl2_l2_vtag_present =
				L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_VTAG_PRESENT(l2ctx_key_ptr);
		l2ctx_key_info->ipv4.otl2_tl2_l2_two_vtags =
				L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_TWO_VTAGS(l2ctx_key_ptr);
		l2ctx_key_info->ipv4.otl2_tl2_l2_ovlan_vid =
				L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_OVLAN_VID(l2ctx_key_ptr);
		l2ctx_key_info->ipv4.otl2_tl2_l2_ovlan_tpid_sel =
				L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_OVLAN_TPID_SEL(l2ctx_key_ptr);
		l2ctx_key_info->ipv4.otl2_tl2_l2_ivlan_vid =
				L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_IVLAN_VID(l2ctx_key_ptr);
		l2ctx_key_info->ipv4.otl2_tl2_l2_ivlan_tpid_sel =
				L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_IVLAN_TPID_SEL(l2ctx_key_ptr);
		l2ctx_key_info->ipv4.otl2_tl2_l2_etype =
				L2CTX_KEY_INFO_IPV4_OTL2_TL2_L2_ETYPE(l2ctx_key_ptr);
	}
}

#define L2CTX_RMP_INFO_PRSV_PARIF(kptr)      tfc_getbits(l2ctx_rmp_ptr, 126, 1)
#define L2CTX_RMP_INFO_PARIF(kptr)           tfc_getbits(l2ctx_rmp_ptr, 121, 5)
#define L2CTX_RMP_INFO_PRSV_L2IP_CTXT(kptr)  tfc_getbits(l2ctx_rmp_ptr, 120, 1)
#define L2CTX_RMP_INFO_L2IP_CTXT(kptr)       tfc_getbits(l2ctx_rmp_ptr, 109, 11)
#define L2CTX_RMP_INFO_PRSV_PROF_FUNC(kptr)  tfc_getbits(l2ctx_rmp_ptr, 108, 1)
#define L2CTX_RMP_INFO_PROF_FUNC(kptr)       tfc_getbits(l2ctx_rmp_ptr, 100, 8)
#define L2CTX_RMP_INFO_CTXT_OPCODE(kptr)     tfc_getbits(l2ctx_rmp_ptr, 98, 2)
#define L2CTX_RMP_INFO_L2IP_META_ENB(kptr)   tfc_getbits(l2ctx_rmp_ptr, 97, 1)
#define L2CTX_RMP_INFO_L2IP_META_PROF(kptr)  tfc_getbits(l2ctx_rmp_ptr, 94, 3)
#define L2CTX_RMP_INFO_L2IP_META_DATA(kptr)  tfc_getbits(l2ctx_rmp_ptr, 62, 32)
#define L2CTX_RMP_INFO_L2IP_ACT_ENB(kptr)    tfc_getbits(l2ctx_rmp_ptr, 61, 1)
#define L2CTX_RMP_INFO_L2IP_ACT_HINT(kptr)   tfc_getbits(l2ctx_rmp_ptr, 59, 2)
#define L2CTX_RMP_INFO_L2IP_ACT_SCOPE(kptr)  tfc_getbits(l2ctx_rmp_ptr, 54, 5)
#define L2CTX_RMP_INFO_L2IP_ACT_PTR(kptr)    tfc_getbits(l2ctx_rmp_ptr, 28, 26)
#define L2CTX_RMP_INFO_L2IP_RFS_ENB(kptr)    tfc_getbits(l2ctx_rmp_ptr, 27, 1)
#define L2CTX_RMP_INFO_L2IP_RFS_DATA(kptr)   tfc_getbits(l2ctx_rmp_ptr, 18, 9)
#define L2CTX_RMP_INFO_L2IP_DEST_ENB(kptr)   tfc_getbits(l2ctx_rmp_ptr, 17, 1)
#define L2CTX_RMP_INFO_L2IP_DEST_DATA(kptr)  tfc_getbits(l2ctx_rmp_ptr, 0, 17)

static void l2ctx_tcam_remap_decode(struct seq_file *m,
				    uint32_t *l2ctx_rmp_ptr,
				    struct l2ctx_tcam_remap_t *l2ctx_rmp_info,
				    struct tfc_ts_mem_cfg *act_mem_cfg)
{
	l2ctx_rmp_info->prsv_parif      = L2CTX_RMP_INFO_PRSV_PARIF(l2ctx_rmp_ptr);
	l2ctx_rmp_info->parif           = L2CTX_RMP_INFO_PARIF(l2ctx_rmp_ptr);
	l2ctx_rmp_info->prsv_l2ip_ctxt  = L2CTX_RMP_INFO_PRSV_L2IP_CTXT(l2ctx_rmp_ptr);
	l2ctx_rmp_info->l2ip_ctxt       = L2CTX_RMP_INFO_L2IP_CTXT(l2ctx_rmp_ptr);
	l2ctx_rmp_info->prsv_prof_func  = L2CTX_RMP_INFO_PRSV_PROF_FUNC(l2ctx_rmp_ptr);
	l2ctx_rmp_info->prof_func       = L2CTX_RMP_INFO_PROF_FUNC(l2ctx_rmp_ptr);
	l2ctx_rmp_info->ctxt_opcode     = L2CTX_RMP_INFO_CTXT_OPCODE(l2ctx_rmp_ptr);
	l2ctx_rmp_info->l2ip_meta_enb   = L2CTX_RMP_INFO_L2IP_META_ENB(l2ctx_rmp_ptr);
	l2ctx_rmp_info->l2ip_meta_prof  = L2CTX_RMP_INFO_L2IP_META_PROF(l2ctx_rmp_ptr);
	l2ctx_rmp_info->l2ip_meta_data  = L2CTX_RMP_INFO_L2IP_META_DATA(l2ctx_rmp_ptr);
	l2ctx_rmp_info->l2ip_act_enb    = L2CTX_RMP_INFO_L2IP_ACT_ENB(l2ctx_rmp_ptr);
	l2ctx_rmp_info->l2ip_act_hint   = L2CTX_RMP_INFO_L2IP_ACT_HINT(l2ctx_rmp_ptr);
	l2ctx_rmp_info->l2ip_act_scope  = L2CTX_RMP_INFO_L2IP_ACT_SCOPE(l2ctx_rmp_ptr);
	l2ctx_rmp_info->l2ip_act_ptr    = L2CTX_RMP_INFO_L2IP_ACT_PTR(l2ctx_rmp_ptr);
	l2ctx_rmp_info->l2ip_rfs_enb    = L2CTX_RMP_INFO_L2IP_RFS_ENB(l2ctx_rmp_ptr);
	l2ctx_rmp_info->l2ip_rfs_data   = L2CTX_RMP_INFO_L2IP_RFS_DATA(l2ctx_rmp_ptr);
	l2ctx_rmp_info->l2ip_dest_enb   = L2CTX_RMP_INFO_L2IP_DEST_ENB(l2ctx_rmp_ptr);
	l2ctx_rmp_info->l2ip_dest_data  = L2CTX_RMP_INFO_L2IP_DEST_DATA(l2ctx_rmp_ptr);
	act_process(m, l2ctx_rmp_info->l2ip_act_ptr, &l2ctx_rmp_info->act_info, act_mem_cfg);
}

static void l2ctx_tcam_show(struct seq_file *m,
			    struct l2ctx_tcam_key_t *l2ctx_key_info,
			    struct l2ctx_tcam_key_t *l2ctx_mask_info,
			    struct l2ctx_tcam_remap_t *l2ctx_rmp_info)
{
	char *line1 = NULL;
	char *line2 = NULL;
	char *line3 = NULL;
	char *line4 = NULL;
	char *line5 = NULL;

	line1 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	line2 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	line3 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	line4 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	line5 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	if (!line1 || !line2 || !line3 || !line4 || !line5) {
		kfree(line1);
		kfree(line2);
		kfree(line3);
		kfree(line4);
		kfree(line5);
		seq_printf(m, "%s: Failed to allocate temp buffer\n",
			   __func__);
		return;
	}

	snprintf(line1, TFC_STRING_LENGTH_256, "+-+--+---+----+---+----+-----+----+---------"
		 "+------+----+----+---+---+----------+\n");
	snprintf(line2, TFC_STRING_LENGTH_256, "|V|Sp|mpc|rcyc|lbk|spif|parif|svif| metadata"
		 "|l2func|roce|pllc|OTH| TH|TID/ctx/L4|\n");
	snprintf(line3, TFC_STRING_LENGTH_256, "+-+--+---+----+---+----+-----+----+---------"
		 "+------+----+----+---+---+----------+\n");
	snprintf(line4, TFC_STRING_LENGTH_256, " %01x  %01x  %01x   x%01x   %01x    %01x   x%02x"
		 "  x%03x x%08x   x%02x    %01x    %01x  x%02x x%02x  x%08x  key\n",
		 l2ctx_key_info->valid,
		 l2ctx_key_info->spare,
		 l2ctx_key_info->mpass_cnt,
		 l2ctx_key_info->rcyc,
		 l2ctx_key_info->loopback,
		 l2ctx_key_info->spif,
		 l2ctx_key_info->parif,
		 l2ctx_key_info->svif,
		 l2ctx_key_info->metadata,
		 l2ctx_key_info->l2ip_func,
		 l2ctx_key_info->roce,
		 l2ctx_key_info->pure_llc,
		 l2ctx_key_info->ot_hdr_type,
		 l2ctx_key_info->t_hdr_type,
		 l2ctx_key_info->tunnel_id_context_L4);
	snprintf(line5, TFC_STRING_LENGTH_256, " %01x  %01x  %01x   x%01x   %01x    %01x   x%02x"
		 "  x%03x x%08x   x%02x    %01x    %01x  x%02x x%02x  x%08x  mask\n",
		 l2ctx_mask_info->valid,
		 l2ctx_mask_info->spare,
		 l2ctx_mask_info->mpass_cnt,
		 l2ctx_mask_info->rcyc,
		 l2ctx_mask_info->loopback,
		 l2ctx_mask_info->spif,
		 l2ctx_mask_info->parif,
		 l2ctx_mask_info->svif,
		 l2ctx_mask_info->metadata,
		 l2ctx_mask_info->l2ip_func,
		 l2ctx_mask_info->roce,
		 l2ctx_mask_info->pure_llc,
		 l2ctx_mask_info->ot_hdr_type,
		 l2ctx_mask_info->t_hdr_type,
		 l2ctx_mask_info->tunnel_id_context_L4);
	seq_printf(m, "%s%s%s%s%s",
		   line1,
		   line2,
		   line3,
		   line5,
		   line4);

	if (l2ctx_key_info->t_hdr_type == 0x5 ||
	    l2ctx_key_info->ot_hdr_type == 0x5) {
		snprintf(line1, TFC_STRING_LENGTH_256, "+------IPv6-------+------IPv6-------"
			 "+-----+\n");
		snprintf(line2, TFC_STRING_LENGTH_256, "|      ADDR0      |      ADDR1      "
			 "|etype|\n");
		snprintf(line3, TFC_STRING_LENGTH_256, "+-----------------+-----------------"
			 "+-----+\n");
		snprintf(line4, TFC_STRING_LENGTH_256, " x%016llx x%016llx x%04x  key\n",
			 l2ctx_key_info->ipv6.ADDR0[1],
			 l2ctx_key_info->ipv6.ADDR0[0],
			 l2ctx_key_info->ipv6.otl2_tl2_l2_etype);
		snprintf(line5, TFC_STRING_LENGTH_256, " x%016llx x%016llx x%04x  mask\n",
			 l2ctx_mask_info->ipv6.ADDR0[1],
			 l2ctx_mask_info->ipv6.ADDR0[0],
			 l2ctx_mask_info->ipv6.otl2_tl2_l2_etype);
	} else {
		snprintf(line1, TFC_STRING_LENGTH_256, "+----IPv4-----+----IPv4-----+--+---"
			 "+----+-----+----+-----+-----+\n");
		snprintf(line2, TFC_STRING_LENGTH_256, "|    ADDR0    |    ADDR1    |VT|2VT"
			 "|ovid|otpid|ivid|itpid|etype|\n");
		snprintf(line3, TFC_STRING_LENGTH_256, "+-------------+-------------+--+---"
			 "+----+-----+----+-----+-----+\n");
		snprintf(line4, TFC_STRING_LENGTH_256, " x%012llx x%012llx  %01x  %01x  x%03x"
			 "    %01x  x%03x   %01x   x%04x  key\n",
			 l2ctx_key_info->ipv4.ADDR0,
			 l2ctx_key_info->ipv4.ADDR1,
			 l2ctx_key_info->ipv4.otl2_tl2_l2_vtag_present,
			 l2ctx_key_info->ipv4.otl2_tl2_l2_two_vtags,
			 l2ctx_key_info->ipv4.otl2_tl2_l2_ovlan_vid,
			 l2ctx_key_info->ipv4.otl2_tl2_l2_ovlan_tpid_sel,
			 l2ctx_key_info->ipv4.otl2_tl2_l2_ivlan_vid,
			 l2ctx_key_info->ipv4.otl2_tl2_l2_ivlan_tpid_sel,
			 l2ctx_key_info->ipv4.otl2_tl2_l2_etype);
		snprintf(line5, TFC_STRING_LENGTH_256, " x%012llx x%012llx  %01x  %01x  x%03x"
			 "    %01x  x%03x   %01x   x%04x  mask\n",
			 l2ctx_mask_info->ipv4.ADDR0,
			 l2ctx_mask_info->ipv4.ADDR1,
			 l2ctx_mask_info->ipv4.otl2_tl2_l2_vtag_present,
			 l2ctx_mask_info->ipv4.otl2_tl2_l2_two_vtags,
			 l2ctx_mask_info->ipv4.otl2_tl2_l2_ovlan_vid,
			 l2ctx_mask_info->ipv4.otl2_tl2_l2_ovlan_tpid_sel,
			 l2ctx_mask_info->ipv4.otl2_tl2_l2_ivlan_vid,
			 l2ctx_mask_info->ipv4.otl2_tl2_l2_ivlan_tpid_sel,
			 l2ctx_mask_info->ipv4.otl2_tl2_l2_etype);
	}
	seq_printf(m, "%s%s%s%s%s",
		   line1,
		   line2,
		   line3,
		   line5,
		   line4);

	seq_printf(m, ":L2CTX TCAM: remap\n");
	snprintf(line1, TFC_STRING_LENGTH_256, "+---+----+----+----+---+---+-----+---"
		 "+-----+---------+\n");
	snprintf(line2, TFC_STRING_LENGTH_256, "|PIP|prif|PL2C| L2C|PPF|PRF|ctxop|mde"
		 "|mprof| metadata|\n");
	snprintf(line3, TFC_STRING_LENGTH_256, "+---+----+----+----+---+---+-----+---"
		 "+-----+---------+\n");
	snprintf(line4, TFC_STRING_LENGTH_256, "  %01x   x%02x   %01x  x%03x  %01x  x%02x"
		 "    %01x    %01x    %01x  x%08x\n",
		 l2ctx_rmp_info->prsv_parif,
		 l2ctx_rmp_info->parif,
		 l2ctx_rmp_info->prsv_l2ip_ctxt,
		 l2ctx_rmp_info->l2ip_ctxt,
		 l2ctx_rmp_info->prsv_prof_func,
		 l2ctx_rmp_info->prof_func,
		 l2ctx_rmp_info->ctxt_opcode,
		 l2ctx_rmp_info->l2ip_meta_enb,
		 l2ctx_rmp_info->l2ip_meta_prof,
		 l2ctx_rmp_info->l2ip_meta_data);
	seq_printf(m, "%s%s%s%s",
		   line1,
		   line2,
		   line3,
		   line4);

	snprintf(line1, TFC_STRING_LENGTH_256, "+----+----+----+---------+----+-------"
		 "+----+--------+\n");
	snprintf(line2, TFC_STRING_LENGTH_256, "|acte|ahnt|ascp|   act   |rfse|rfsdata"
		 "|dste|dst_data|\n");
	snprintf(line3, TFC_STRING_LENGTH_256, "+----+----+----+---------+----+-------"
		 "+----+--------+\n");
	snprintf(line4, TFC_STRING_LENGTH_256, "   %01x    %01x   x%02x x%08x   %01x"
		 "    x%03x    %01x   x%05x\n",
		 l2ctx_rmp_info->l2ip_act_enb,
		 l2ctx_rmp_info->l2ip_act_hint,
		 l2ctx_rmp_info->l2ip_act_scope,
		 l2ctx_rmp_info->l2ip_act_ptr,
		 l2ctx_rmp_info->l2ip_rfs_enb,
		 l2ctx_rmp_info->l2ip_rfs_data,
		 l2ctx_rmp_info->l2ip_dest_enb,
		 l2ctx_rmp_info->l2ip_dest_data);
	seq_printf(m, "%s%s%s%s",
		   line1,
		   line2,
		   line3,
		   line4);

	act_show(m, &l2ctx_rmp_info->act_info, l2ctx_rmp_info->l2ip_act_ptr << 5);

	kfree(line1);
	kfree(line2);
	kfree(line3);
	kfree(line4);
	kfree(line5);
}

#define PTKEY_INFO_VALID(kptr)                tfc_getbits(kptr, 183, 1)
#define PTKEY_INFO_SPARE(kptr)                tfc_getbits(kptr, 181, 2)
#define PTKEY_INFO_LOOPBACK(kptr)             tfc_getbits(kptr, 180, 1)
#define PTKEY_INFO_PKT_TYPE(kptr)             tfc_getbits(kptr, 176, 4)
#define PTKEY_INFO_RCYC(kptr)                 tfc_getbits(kptr, 172, 4)
#define PTKEY_INFO_METADATA(kptr)             tfc_getbits(kptr, 140, 32)
#define PTKEY_INFO_AGG_ERROR(kptr)            tfc_getbits(kptr, 139, 1)
#define PTKEY_INFO_L2IP_FUNC(kptr)            tfc_getbits(kptr, 131, 8)
#define PTKEY_INFO_PROF_FUNC(kptr)            tfc_getbits(kptr, 123, 8)
#define PTKEY_INFO_HREC_NEXT(kptr)            tfc_getbits(kptr, 121, 2)
#define PTKEY_INFO_INT_HDR_TYPE(kptr)         tfc_getbits(kptr, 119, 2)
#define PTKEY_INFO_INT_HDR_GROUP(kptr)        tfc_getbits(kptr, 117, 2)
#define PTKEY_INFO_INT_IFA_TAIL(kptr)         tfc_getbits(kptr, 116, 1)

#define PTKEY_INFO_OTL2_HDR_VALID(kptr)       tfc_getbits(kptr, 115, 1)
#define PTKEY_INFO_OTL2_HDR_TYPE(kptr)        tfc_getbits(kptr, 113, 2)
#define PTKEY_INFO_OTL2_UC_MC_BC(kptr)        tfc_getbits(kptr, 111, 2)
#define PTKEY_INFO_OTL2_VTAG_PRESENT(kptr)    tfc_getbits(kptr, 110, 1)
#define PTKEY_INFO_OTL2_TWO_VTAGS(kptr)       tfc_getbits(kptr, 109, 1)

#define PTKEY_INFO_OTL3_HDR_VALID(kptr)       tfc_getbits(kptr, 108, 1)
#define PTKEY_INFO_OTL3_HDR_ERROR(kptr)       tfc_getbits(kptr, 107, 1)
#define PTKEY_INFO_OTL3_HDR_TYPE(kptr)        tfc_getbits(kptr, 103, 4)
#define PTKEY_INFO_OTL3_HDR_ISIP(kptr)        tfc_getbits(kptr, 102, 1)

#define PTKEY_INFO_OTL4_HDR_VALID(kptr)       tfc_getbits(kptr, 101, 1)
#define PTKEY_INFO_OTL4_HDR_ERROR(kptr)       tfc_getbits(kptr, 100, 1)
#define PTKEY_INFO_OTL4_HDR_TYPE(kptr)        tfc_getbits(kptr, 96, 4)
#define PTKEY_INFO_OTL4_HDR_IS_UDP_TCP(kptr)  tfc_getbits(kptr, 95, 1)

#define PTKEY_INFO_OT_HDR_VALID(kptr)         tfc_getbits(kptr, 94, 1)
#define PTKEY_INFO_OT_HDR_ERROR(kptr)         tfc_getbits(kptr, 93, 1)
#define PTKEY_INFO_OT_HDR_TYPE(kptr)          tfc_getbits(kptr, 88, 5)
#define PTKEY_INFO_OT_HDR_FLAGS(kptr)         tfc_getbits(kptr, 80, 8)

#define PTKEY_INFO_TL2_HDR_VALID(kptr)        tfc_getbits(kptr, 79, 1)
#define PTKEY_INFO_TL2_HDR_TYPE(kptr)         tfc_getbits(kptr, 77, 2)
#define PTKEY_INFO_TL2_UC_MC_BC(kptr)         tfc_getbits(kptr, 75, 2)
#define PTKEY_INFO_TL2_VTAG_PRESENT(kptr)     tfc_getbits(kptr, 74, 1)
#define PTKEY_INFO_TL2_TWO_VTAGS(kptr)        tfc_getbits(kptr, 73, 1)

#define PTKEY_INFO_TL3_HDR_VALID(kptr)        tfc_getbits(kptr, 72, 1)
#define PTKEY_INFO_TL3_HDR_ERROR(kptr)        tfc_getbits(kptr, 71, 1)
#define PTKEY_INFO_TL3_HDR_TYPE(kptr)         tfc_getbits(kptr, 67, 4)
#define PTKEY_INFO_TL3_HDR_ISIP(kptr)         tfc_getbits(kptr, 66, 1)

#define PTKEY_INFO_TL4_HDR_VALID(kptr)        tfc_getbits(kptr, 65, 1)
#define PTKEY_INFO_TL4_HDR_ERROR(kptr)        tfc_getbits(kptr, 64, 1)
#define PTKEY_INFO_TL4_HDR_TYPE(kptr)         tfc_getbits(kptr, 60, 4)
#define PTKEY_INFO_TL4_HDR_IS_UDP_TCP(kptr)   tfc_getbits(kptr, 59, 1)

#define PTKEY_INFO_T_HDR_VALID(kptr)          tfc_getbits(kptr, 58, 1)
#define PTKEY_INFO_T_HDR_ERROR(kptr)          tfc_getbits(kptr, 57, 1)
#define PTKEY_INFO_T_HDR_TYPE(kptr)           tfc_getbits(kptr, 52, 5)
#define PTKEY_INFO_T_HDR_FLAGS(kptr)          tfc_getbits(kptr, 44, 8)

#define PTKEY_INFO_L2_HDR_VALID(kptr)         tfc_getbits(kptr, 43, 1)
#define PTKEY_INFO_L2_HDR_ERROR(kptr)         tfc_getbits(kptr, 42, 1)
#define PTKEY_INFO_L2_HDR_TYPE(kptr)          tfc_getbits(kptr, 40, 2)
#define PTKEY_INFO_L2_UC_MC_BC(kptr)          tfc_getbits(kptr, 38, 2)
#define PTKEY_INFO_L2_VTAG_PRESENT(kptr)      tfc_getbits(kptr, 37, 1)
#define PTKEY_INFO_L2_TWO_VTAGS(kptr)         tfc_getbits(kptr, 36, 1)

#define PTKEY_INFO_L3_HDR_VALID(kptr)         tfc_getbits(kptr, 35, 1)
#define PTKEY_INFO_L3_HDR_ERROR(kptr)         tfc_getbits(kptr, 34, 1)
#define PTKEY_INFO_L3_HDR_TYPE(kptr)          tfc_getbits(kptr, 30, 4)
#define PTKEY_INFO_L3_HDR_ISIP(kptr)          tfc_getbits(kptr, 29, 1)
#define PTKEY_INFO_L3_PROTOCOL(kptr)          tfc_getbits(kptr, 21, 8)

#define PTKEY_INFO_L4_HDR_VALID(kptr)         tfc_getbits(kptr, 20, 1)
#define PTKEY_INFO_L4_HDR_ERROR(kptr)         tfc_getbits(kptr, 19, 1)
#define PTKEY_INFO_L4_HDR_TYPE(kptr)          tfc_getbits(kptr, 15, 4)
#define PTKEY_INFO_L4_HDR_IS_UDP_TCP(kptr)    tfc_getbits(kptr, 14, 1)
#define PTKEY_INFO_L4_HDR_SUBTYPE(kptr)       tfc_getbits(kptr, 11, 3)
#define PTKEY_INFO_L4_FLAGS(kptr)             tfc_getbits(kptr, 2, 9)
#define PTKEY_INFO_L4_DCN_PRESENT(kptr)       tfc_getbits(kptr, 0, 2)

static void prof_tcam_key_decode(struct seq_file *m,
				 uint32_t *ptc_key_ptr,
				 struct prof_tcam_key_t *ptkey_info)
{
	ptkey_info->valid               = PTKEY_INFO_VALID(ptc_key_ptr);
	ptkey_info->spare               = PTKEY_INFO_SPARE(ptc_key_ptr);
	ptkey_info->loopback            = PTKEY_INFO_LOOPBACK(ptc_key_ptr);
	ptkey_info->pkt_type            = PTKEY_INFO_PKT_TYPE(ptc_key_ptr);
	ptkey_info->rcyc                = PTKEY_INFO_RCYC(ptc_key_ptr);
	ptkey_info->metadata            = PTKEY_INFO_METADATA(ptc_key_ptr);
	ptkey_info->agg_error           = PTKEY_INFO_AGG_ERROR(ptc_key_ptr);
	ptkey_info->l2ip_func           = PTKEY_INFO_L2IP_FUNC(ptc_key_ptr);
	ptkey_info->prof_func           = PTKEY_INFO_PROF_FUNC(ptc_key_ptr);
	ptkey_info->hrec_next           = PTKEY_INFO_HREC_NEXT(ptc_key_ptr);
	ptkey_info->int_hdr_type        = PTKEY_INFO_INT_HDR_TYPE(ptc_key_ptr);
	ptkey_info->int_hdr_group       = PTKEY_INFO_INT_HDR_GROUP(ptc_key_ptr);
	ptkey_info->int_ifa_tail        = PTKEY_INFO_INT_IFA_TAIL(ptc_key_ptr);

	ptkey_info->otl2_hdr_valid      = PTKEY_INFO_OTL2_HDR_VALID(ptc_key_ptr);
	ptkey_info->otl2_hdr_type       = PTKEY_INFO_OTL2_HDR_TYPE(ptc_key_ptr);
	ptkey_info->otl2_uc_mc_bc       = PTKEY_INFO_OTL2_UC_MC_BC(ptc_key_ptr);
	ptkey_info->otl2_vtag_present   = PTKEY_INFO_OTL2_VTAG_PRESENT(ptc_key_ptr);
	ptkey_info->otl2_two_vtags      = PTKEY_INFO_OTL2_TWO_VTAGS(ptc_key_ptr);

	ptkey_info->otl3_hdr_valid      = PTKEY_INFO_OTL3_HDR_VALID(ptc_key_ptr);
	ptkey_info->otl3_hdr_error      = PTKEY_INFO_OTL3_HDR_ERROR(ptc_key_ptr);
	ptkey_info->otl3_hdr_type       = PTKEY_INFO_OTL3_HDR_TYPE(ptc_key_ptr);
	ptkey_info->otl3_hdr_isip       = PTKEY_INFO_OTL3_HDR_ISIP(ptc_key_ptr);

	ptkey_info->otl4_hdr_valid      = PTKEY_INFO_OTL4_HDR_VALID(ptc_key_ptr);
	ptkey_info->otl4_hdr_error      = PTKEY_INFO_OTL4_HDR_ERROR(ptc_key_ptr);
	ptkey_info->otl4_hdr_type       = PTKEY_INFO_OTL4_HDR_TYPE(ptc_key_ptr);
	ptkey_info->otl4_hdr_is_udp_tcp = PTKEY_INFO_OTL4_HDR_IS_UDP_TCP(ptc_key_ptr);

	ptkey_info->ot_hdr_valid        = PTKEY_INFO_OT_HDR_VALID(ptc_key_ptr);
	ptkey_info->ot_hdr_error        = PTKEY_INFO_OT_HDR_ERROR(ptc_key_ptr);
	ptkey_info->ot_hdr_type         = PTKEY_INFO_OT_HDR_TYPE(ptc_key_ptr);
	ptkey_info->ot_hdr_flags        = PTKEY_INFO_OT_HDR_FLAGS(ptc_key_ptr);

	ptkey_info->tl2_hdr_valid       = PTKEY_INFO_TL2_HDR_VALID(ptc_key_ptr);
	ptkey_info->tl2_hdr_type        = PTKEY_INFO_TL2_HDR_TYPE(ptc_key_ptr);
	ptkey_info->tl2_uc_mc_bc        = PTKEY_INFO_TL2_UC_MC_BC(ptc_key_ptr);
	ptkey_info->tl2_vtag_present    = PTKEY_INFO_TL2_VTAG_PRESENT(ptc_key_ptr);
	ptkey_info->tl2_two_vtags       = PTKEY_INFO_TL2_TWO_VTAGS(ptc_key_ptr);

	ptkey_info->tl3_hdr_valid       = PTKEY_INFO_TL3_HDR_VALID(ptc_key_ptr);
	ptkey_info->tl3_hdr_error       = PTKEY_INFO_TL3_HDR_ERROR(ptc_key_ptr);
	ptkey_info->tl3_hdr_type        = PTKEY_INFO_TL3_HDR_TYPE(ptc_key_ptr);
	ptkey_info->tl3_hdr_isip        = PTKEY_INFO_TL3_HDR_ISIP(ptc_key_ptr);

	ptkey_info->tl4_hdr_valid       = PTKEY_INFO_TL4_HDR_VALID(ptc_key_ptr);
	ptkey_info->tl4_hdr_error       = PTKEY_INFO_TL4_HDR_ERROR(ptc_key_ptr);
	ptkey_info->tl4_hdr_type        = PTKEY_INFO_TL4_HDR_TYPE(ptc_key_ptr);
	ptkey_info->tl4_hdr_is_udp_tcp  = PTKEY_INFO_TL4_HDR_IS_UDP_TCP(ptc_key_ptr);

	ptkey_info->t_hdr_valid         = PTKEY_INFO_T_HDR_VALID(ptc_key_ptr);
	ptkey_info->t_hdr_error         = PTKEY_INFO_T_HDR_ERROR(ptc_key_ptr);
	ptkey_info->t_hdr_type          = PTKEY_INFO_T_HDR_TYPE(ptc_key_ptr);
	ptkey_info->t_hdr_flags         = PTKEY_INFO_T_HDR_FLAGS(ptc_key_ptr);

	ptkey_info->l2_hdr_valid        = PTKEY_INFO_L2_HDR_VALID(ptc_key_ptr);
	ptkey_info->l2_hdr_error        = PTKEY_INFO_L2_HDR_ERROR(ptc_key_ptr);
	ptkey_info->l2_hdr_type         = PTKEY_INFO_L2_HDR_TYPE(ptc_key_ptr);
	ptkey_info->l2_uc_mc_bc         = PTKEY_INFO_L2_UC_MC_BC(ptc_key_ptr);
	ptkey_info->l2_vtag_present     = PTKEY_INFO_L2_VTAG_PRESENT(ptc_key_ptr);
	ptkey_info->l2_two_vtags        = PTKEY_INFO_L2_TWO_VTAGS(ptc_key_ptr);

	ptkey_info->l3_hdr_valid        = PTKEY_INFO_L3_HDR_VALID(ptc_key_ptr);
	ptkey_info->l3_hdr_error        = PTKEY_INFO_L3_HDR_ERROR(ptc_key_ptr);
	ptkey_info->l3_hdr_type         = PTKEY_INFO_L3_HDR_TYPE(ptc_key_ptr);
	ptkey_info->l3_hdr_isip         = PTKEY_INFO_L3_HDR_ISIP(ptc_key_ptr);
	ptkey_info->l3_protocol         = PTKEY_INFO_L3_PROTOCOL(ptc_key_ptr);

	ptkey_info->l4_hdr_valid        = PTKEY_INFO_L4_HDR_VALID(ptc_key_ptr);
	ptkey_info->l4_hdr_error        = PTKEY_INFO_L4_HDR_ERROR(ptc_key_ptr);
	ptkey_info->l4_hdr_type         = PTKEY_INFO_L4_HDR_TYPE(ptc_key_ptr);
	ptkey_info->l4_hdr_is_udp_tcp   = PTKEY_INFO_L4_HDR_IS_UDP_TCP(ptc_key_ptr);
	ptkey_info->l4_hdr_subtype      = PTKEY_INFO_L4_HDR_SUBTYPE(ptc_key_ptr);
	ptkey_info->l4_flags            = PTKEY_INFO_L4_FLAGS(ptc_key_ptr);
	ptkey_info->l4_dcn_present      = PTKEY_INFO_L4_DCN_PRESENT(ptc_key_ptr);
}

#define PTRMP_INFO_PL_BYP_LKUP_EN(kptr)   tfc_getbits(ptc_rmp_ptr, 42, 1)
#define PTRMP_INFO_EM_SEARCH_EN(kptr)     tfc_getbits(ptc_rmp_ptr, 41, 1)
#define PTRMP_INFO_EM_PROFILE_ID(kptr)    tfc_getbits(ptc_rmp_ptr, 33, 8)
#define PTRMP_INFO_EM_KEY_ID(kptr)        tfc_getbits(ptc_rmp_ptr, 26, 7)
#define PTRMP_INFO_EM_SCOPE(kptr)         tfc_getbits(ptc_rmp_ptr, 21, 5)
#define PTRMP_INFO_TCAM_SEARCH_EN(kptr)   tfc_getbits(ptc_rmp_ptr, 20, 1)
#define PTRMP_INFO_TCAM_PROFILE_ID(kptr)  tfc_getbits(ptc_rmp_ptr, 12, 8)
#define PTRMP_INFO_TCAM_KEY_ID(kptr)      tfc_getbits(ptc_rmp_ptr, 5, 7)
#define PTRMP_INFO_TCAM_SCOPE(kptr)       tfc_getbits(ptc_rmp_ptr, 0, 5)

static void prof_tcam_remap_decode(struct seq_file *m,
				   uint32_t *ptc_rmp_ptr,
				   struct prof_tcam_remap_t *ptrmp_info)
{
	ptrmp_info->pl_byp_lkup_en  = PTRMP_INFO_PL_BYP_LKUP_EN(ptc_rmp_ptr) ? true : false;
	ptrmp_info->em_search_en    = PTRMP_INFO_EM_SEARCH_EN(ptc_rmp_ptr) ? true : false;
	ptrmp_info->em_profile_id   = PTRMP_INFO_EM_PROFILE_ID(ptc_rmp_ptr);
	ptrmp_info->em_key_id       = PTRMP_INFO_EM_KEY_ID(ptc_rmp_ptr);
	ptrmp_info->em_scope        = PTRMP_INFO_EM_SCOPE(ptc_rmp_ptr);
	ptrmp_info->tcam_search_en  = PTRMP_INFO_TCAM_SEARCH_EN(ptc_rmp_ptr) ? true : false;
	ptrmp_info->tcam_profile_id = PTRMP_INFO_TCAM_PROFILE_ID(ptc_rmp_ptr);
	ptrmp_info->tcam_key_id     = PTRMP_INFO_TCAM_KEY_ID(ptc_rmp_ptr);
	ptrmp_info->tcam_scope      = PTRMP_INFO_TCAM_SCOPE(ptc_rmp_ptr);
}

static void prof_tcam_show(struct seq_file *m,
			   struct prof_tcam_key_t *ptkey_info,
			   struct prof_tcam_key_t *ptmask_info,
			   struct prof_tcam_remap_t *ptrmp_info)
{
	char tmph[TFC_STRING_LENGTH_64];
	char tmp1[TFC_STRING_LENGTH_64];
	char tmp2[TFC_STRING_LENGTH_64];
	char tmp3[TFC_STRING_LENGTH_64];
	char tmp4[TFC_STRING_LENGTH_64];
	char tmp5[TFC_STRING_LENGTH_64];
	char *lineh = NULL;
	char *line1 = NULL;
	char *line2 = NULL;
	char *line3 = NULL;
	char *line4 = NULL;
	char *line5 = NULL;

	lineh = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	line1 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	line2 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	line3 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	line4 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	line5 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	if (!lineh || !line1 || !line2 || !line3 || !line4 || !line5) {
		kfree(lineh);
		kfree(line1);
		kfree(line2);
		kfree(line3);
		kfree(line4);
		kfree(line5);
		seq_printf(m, "%s: Failed to allocate temp buffer\n",
			   __func__);
		return;
	}

	snprintf(line1, TFC_STRING_LENGTH_256, "+-+--+----+----+----+---------+------"
		 "+--------+-------+-----+---+---+---+\n");
	snprintf(line2, TFC_STRING_LENGTH_256, "|V|Sp|lpbk|ptyp|rcyc|    MD   |aggerr"
		 "|l2ipfunc|profunc|hrnxt|IHT|IHG|IIT|\n");
	snprintf(line3, TFC_STRING_LENGTH_256, "+-+--+----+----+----+---------+------"
		 "+--------+-------+-----+---+---+---+\n");
	snprintf(line4, TFC_STRING_LENGTH_256, " %01x x%01x   %01x   x%01x  x%01x  x%08x"
		 "    %01x     x%02x      x%02x     x%01x   x%01x  x%01x   %01x  key\n",
		 ptkey_info->valid,
		 ptkey_info->spare,
		 ptkey_info->loopback,
		 ptkey_info->pkt_type,
		 ptkey_info->rcyc,
		 ptkey_info->metadata,
		 ptkey_info->agg_error,
		 ptkey_info->l2ip_func,
		 ptkey_info->prof_func,
		 ptkey_info->hrec_next,
		 ptkey_info->int_hdr_type,
		 ptkey_info->int_hdr_group,
		 ptkey_info->int_ifa_tail);
	snprintf(line5, TFC_STRING_LENGTH_256, " %01x x%01x   %01x   x%01x  x%01x  x%08x"
		 "    %01x     x%02x      x%02x     x%01x   x%01x  x%01x   %01x  mask\n",
		 ptmask_info->valid,
		 ptmask_info->spare,
		 ptmask_info->loopback,
		 ptmask_info->pkt_type,
		 ptmask_info->rcyc,
		 ptmask_info->metadata,
		 ptmask_info->agg_error,
		 ptmask_info->l2ip_func,
		 ptmask_info->prof_func,
		 ptmask_info->hrec_next,
		 ptmask_info->int_hdr_type,
		 ptmask_info->int_hdr_group,
		 ptmask_info->int_ifa_tail);
	seq_printf(m, "%s%s%s%s%s",
		   line1,
		   line2,
		   line3,
		   line5,
		   line4);

	snprintf(lineh, TFC_STRING_LENGTH_256, "|OTL2 hdr       |");
	snprintf(line1, TFC_STRING_LENGTH_256, "+-+--+---+--+---+");
	snprintf(line2, TFC_STRING_LENGTH_256, "|V|HT|UMB|VT|2VT|");
	snprintf(line3, TFC_STRING_LENGTH_256, "+-+--+---+--+---+");
	snprintf(line4, TFC_STRING_LENGTH_256, " %01x x%01x  x%01x  %01x  %01x ",
		 ptkey_info->otl2_hdr_valid,
		 ptkey_info->otl2_hdr_type,
		 ptkey_info->otl2_uc_mc_bc,
		 ptkey_info->otl2_vtag_present,
		 ptkey_info->otl2_two_vtags);
	snprintf(line5, TFC_STRING_LENGTH_256, " %01x x%01x  x%01x  %01x  %01x ",
		 ptmask_info->otl2_hdr_valid,
		 ptmask_info->otl2_hdr_type,
		 ptmask_info->otl2_uc_mc_bc,
		 ptmask_info->otl2_vtag_present,
		 ptmask_info->otl2_two_vtags);

	snprintf(tmph, TFC_STRING_LENGTH_64, "OTL3 hdr  |");
	snprintf(tmp1, TFC_STRING_LENGTH_64, "-+--+--+--+");
	snprintf(tmp2, TFC_STRING_LENGTH_64, "V|HE|HT|IP|");
	snprintf(tmp3, TFC_STRING_LENGTH_64, "-+--+--+--+");
	snprintf(tmp4, TFC_STRING_LENGTH_64, " %01x  %01x x%01x  %01x ",
		 ptkey_info->otl3_hdr_valid,
		 ptkey_info->otl3_hdr_error,
		 ptkey_info->otl3_hdr_type,
		 ptkey_info->otl3_hdr_isip);
	snprintf(tmp5, TFC_STRING_LENGTH_256, " %01x  %01x x%01x  %01x ",
		 ptmask_info->otl3_hdr_valid,
		 ptmask_info->otl3_hdr_error,
		 ptmask_info->otl3_hdr_type,
		 ptmask_info->otl3_hdr_isip);

	strcat(lineh, tmph);
	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);
	strcat(line5, tmp5);

	snprintf(tmph, TFC_STRING_LENGTH_64, "OTL4 hdr  |");
	snprintf(tmp1, TFC_STRING_LENGTH_64, "-+--+--+--+");
	snprintf(tmp2, TFC_STRING_LENGTH_64, "V|HE|HT|IP|");
	snprintf(tmp3, TFC_STRING_LENGTH_64, "-+--+--+--+");
	snprintf(tmp4, TFC_STRING_LENGTH_64, "%01x  %01x x%01x  %01x ",
		 ptkey_info->otl4_hdr_valid,
		 ptkey_info->otl4_hdr_error,
		 ptkey_info->otl4_hdr_type,
		 ptkey_info->otl4_hdr_is_udp_tcp);
	snprintf(tmp5, TFC_STRING_LENGTH_64, "%01x  %01x x%01x  %01x ",
		 ptmask_info->otl4_hdr_valid,
		 ptmask_info->otl4_hdr_error,
		 ptmask_info->otl4_hdr_type,
		 ptmask_info->otl4_hdr_is_udp_tcp);

	strcat(lineh, tmph);
	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);
	strcat(line5, tmp5);

	snprintf(tmph, TFC_STRING_LENGTH_64, "OT hdr      |\n");
	snprintf(tmp1, TFC_STRING_LENGTH_64, "-+--+---+---+\n");
	snprintf(tmp2, TFC_STRING_LENGTH_64, "V|HE| HT|flg|\n");
	snprintf(tmp3, TFC_STRING_LENGTH_64, "-+--+---+---+\n");
	snprintf(tmp4, TFC_STRING_LENGTH_64, "%01x  %01x x%02x x%02x  key\n",
		 ptkey_info->ot_hdr_valid,
		 ptkey_info->ot_hdr_error,
		 ptkey_info->ot_hdr_type,
		 ptkey_info->ot_hdr_flags);
	snprintf(tmp5, TFC_STRING_LENGTH_64, "%01x  %01x x%02x x%02x  mask\n",
		 ptmask_info->ot_hdr_valid,
		 ptmask_info->ot_hdr_error,
		 ptmask_info->ot_hdr_type,
		 ptmask_info->ot_hdr_flags);

	strcat(lineh, tmph);
	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);
	strcat(line5, tmp5);

	seq_printf(m, "%s%s%s%s%s%s",
		   lineh,
		   line1,
		   line2,
		   line3,
		   line5,
		   line4);

	snprintf(lineh, TFC_STRING_LENGTH_256, "|TL2 hdr        |");
	snprintf(line1, TFC_STRING_LENGTH_256, "+-+--+---+--+---+");
	snprintf(line2, TFC_STRING_LENGTH_256, "|V|HT|UMB|VT|2VT|");
	snprintf(line3, TFC_STRING_LENGTH_256, "+-+--+---+--+---+");
	snprintf(line4, TFC_STRING_LENGTH_256, " %01x x%01x  x%01x  %01x  %01x ",
		 ptkey_info->tl2_hdr_valid,
		 ptkey_info->tl2_hdr_type,
		 ptkey_info->tl2_uc_mc_bc,
		 ptkey_info->tl2_vtag_present,
		 ptkey_info->tl2_two_vtags);
	snprintf(line5, TFC_STRING_LENGTH_256, " %01x x%01x  x%01x  %01x  %01x ",
		 ptmask_info->tl2_hdr_valid,
		 ptmask_info->tl2_hdr_type,
		 ptmask_info->tl2_uc_mc_bc,
		 ptmask_info->tl2_vtag_present,
		 ptmask_info->tl2_two_vtags);

	snprintf(tmph, TFC_STRING_LENGTH_64, "TL3 hdr   |");
	snprintf(tmp1, TFC_STRING_LENGTH_64, "-+--+--+--+");
	snprintf(tmp2, TFC_STRING_LENGTH_64, "V|HE|HT|IP|");
	snprintf(tmp3, TFC_STRING_LENGTH_64, "-+--+--+--+");
	snprintf(tmp4, TFC_STRING_LENGTH_64, " %01x  %01x x%01x  %01x ",
		 ptkey_info->tl3_hdr_valid,
		 ptkey_info->tl3_hdr_error,
		 ptkey_info->tl3_hdr_type,
		 ptkey_info->tl3_hdr_isip);
	snprintf(tmp5, TFC_STRING_LENGTH_256, " %01x  %01x x%01x  %01x ",
		 ptmask_info->tl3_hdr_valid,
		 ptmask_info->tl3_hdr_error,
		 ptmask_info->tl3_hdr_type,
		 ptmask_info->tl3_hdr_isip);

	strcat(lineh, tmph);
	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);
	strcat(line5, tmp5);

	snprintf(tmph, TFC_STRING_LENGTH_64, "TL4 hdr   |");
	snprintf(tmp1, TFC_STRING_LENGTH_64, "-+--+--+--+");
	snprintf(tmp2, TFC_STRING_LENGTH_64, "V|HE|HT|IP|");
	snprintf(tmp3, TFC_STRING_LENGTH_64, "-+--+--+--+");
	snprintf(tmp4, TFC_STRING_LENGTH_64, "%01x  %01x x%01x  %01x ",
		 ptkey_info->tl4_hdr_valid,
		 ptkey_info->tl4_hdr_error,
		 ptkey_info->tl4_hdr_type,
		 ptkey_info->tl4_hdr_is_udp_tcp);
	snprintf(tmp5, TFC_STRING_LENGTH_64, "%01x  %01x x%01x  %01x ",
		 ptmask_info->tl4_hdr_valid,
		 ptmask_info->tl4_hdr_error,
		 ptmask_info->tl4_hdr_type,
		 ptmask_info->tl4_hdr_is_udp_tcp);

	strcat(lineh, tmph);
	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);
	strcat(line5, tmp5);

	snprintf(tmph, TFC_STRING_LENGTH_64, "T hdr       |\n");
	snprintf(tmp1, TFC_STRING_LENGTH_64, "-+--+---+---+\n");
	snprintf(tmp2, TFC_STRING_LENGTH_64, "V|HE| HT|flg|\n");
	snprintf(tmp3, TFC_STRING_LENGTH_64, "-+--+---+---+\n");
	snprintf(tmp4, TFC_STRING_LENGTH_64, "%01x  %01x x%02x x%02x  key\n",
		 ptkey_info->t_hdr_valid,
		 ptkey_info->t_hdr_error,
		 ptkey_info->t_hdr_type,
		 ptkey_info->t_hdr_flags);
	snprintf(tmp5, TFC_STRING_LENGTH_64, "%01x  %01x x%02x x%02x  mask\n",
		 ptmask_info->t_hdr_valid,
		 ptmask_info->t_hdr_error,
		 ptmask_info->t_hdr_type,
		 ptmask_info->t_hdr_flags);

	strcat(lineh, tmph);
	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);
	strcat(line5, tmp5);

	seq_printf(m, "%s%s%s%s%s%s",
		   lineh,
		   line1,
		   line2,
		   line3,
		   line5,
		   line4);

	snprintf(lineh, TFC_STRING_LENGTH_256, "|L2 hdr         |");
	snprintf(line1, TFC_STRING_LENGTH_256, "+-+--+---+--+---+");
	snprintf(line2, TFC_STRING_LENGTH_256, "|V|HT|UMB|VT|2VT|");
	snprintf(line3, TFC_STRING_LENGTH_256, "+-+--+---+--+---+");
	snprintf(line4, TFC_STRING_LENGTH_256, " %01x x%01x  x%01x  %01x  %01x ",
		 ptkey_info->l2_hdr_valid,
		 ptkey_info->l2_hdr_type,
		 ptkey_info->l2_uc_mc_bc,
		 ptkey_info->l2_vtag_present,
		 ptkey_info->l2_two_vtags);
	snprintf(line5, TFC_STRING_LENGTH_256, " %01x x%01x  x%01x  %01x  %01x ",
		 ptmask_info->l2_hdr_valid,
		 ptmask_info->l2_hdr_type,
		 ptmask_info->l2_uc_mc_bc,
		 ptmask_info->l2_vtag_present,
		 ptmask_info->l2_two_vtags);

	snprintf(tmph, TFC_STRING_LENGTH_64, "L3 hdr         |");
	snprintf(tmp1, TFC_STRING_LENGTH_64, "-+--+--+--+----+");
	snprintf(tmp2, TFC_STRING_LENGTH_64, "V|HE|HT|IP|prot|");
	snprintf(tmp3, TFC_STRING_LENGTH_64, "-+--+--+--+----+");
	snprintf(tmp4, TFC_STRING_LENGTH_64, " %01x  %01x x%01x  %01x  x%02x ",
		 ptkey_info->l3_hdr_valid,
		 ptkey_info->l3_hdr_error,
		 ptkey_info->l3_hdr_type,
		 ptkey_info->l3_hdr_isip,
		 ptkey_info->l3_protocol);
	snprintf(tmp5, TFC_STRING_LENGTH_64, " %01x  %01x x%01x  %01x  x%02x ",
		 ptmask_info->l3_hdr_valid,
		 ptmask_info->l3_hdr_error,
		 ptmask_info->l3_hdr_type,
		 ptmask_info->l3_hdr_isip,
		 ptmask_info->l3_protocol);

	strcat(lineh, tmph);
	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);
	strcat(line5, tmp5);

	snprintf(tmph, TFC_STRING_LENGTH_64, "L4 hdr                 |\n");
	snprintf(tmp1, TFC_STRING_LENGTH_64, "-+--+--+--+---+----+---+\n");
	snprintf(tmp2, TFC_STRING_LENGTH_64, "V|HE|HT|IP|HST|flgs|DCN|\n");
	snprintf(tmp3, TFC_STRING_LENGTH_64, "-+--+--+--+---+----+---+\n");
	snprintf(tmp4, TFC_STRING_LENGTH_64, "%01x  %01x x%01x  %01x  x%01x x%03x  x%01x  key\n",
		 ptkey_info->l4_hdr_valid,
		 ptkey_info->l4_hdr_error,
		 ptkey_info->l4_hdr_type,
		 ptkey_info->l4_hdr_is_udp_tcp,
		 ptkey_info->l4_hdr_subtype,
		 ptkey_info->l4_flags,
		 ptkey_info->l4_dcn_present);
	snprintf(tmp5, TFC_STRING_LENGTH_64, "%01x  %01x x%01x  %01x  x%01x x%03x  x%01x  mask\n",
		 ptmask_info->l4_hdr_valid,
		 ptmask_info->l4_hdr_error,
		 ptmask_info->l4_hdr_type,
		 ptmask_info->l4_hdr_is_udp_tcp,
		 ptmask_info->l4_hdr_subtype,
		 ptmask_info->l4_flags,
		 ptmask_info->l4_dcn_present);

	strcat(lineh, tmph);
	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);
	strcat(line5, tmp5);

	seq_printf(m, "%s%s%s%s%s%s",
		   lineh,
		   line1,
		   line2,
		   line3,
		   line5,
		   line4);

	seq_puts(m, "\n:Profile TCAM: remap\n");
	snprintf(line1, TFC_STRING_LENGTH_256, "+-+--+---+---+---+--+---+---+---+\n");
	snprintf(line2, TFC_STRING_LENGTH_256, "|B|EM|PID|KId|Scp|WC|PID|KId|Scp|\n");
	snprintf(line3, TFC_STRING_LENGTH_256, "+-+--+---+---+---+--+---+---+---+\n");
	snprintf(line4, TFC_STRING_LENGTH_256, " %c  %c x%02x x%02x x%02x  %c x%02x x%02x x%02x\n",
		 ptrmp_info->pl_byp_lkup_en ? 'Y' : 'N',
		 ptrmp_info->em_search_en ? 'Y' : 'N',
		 ptrmp_info->em_profile_id,
		 ptrmp_info->em_key_id,
		 ptrmp_info->em_scope,
		 ptrmp_info->tcam_search_en ? 'Y' : 'N',
		 ptrmp_info->tcam_profile_id,
		 ptrmp_info->tcam_key_id,
		 ptrmp_info->tcam_scope);

	seq_printf(m, "%s%s%s%s",
		   line1,
		   line2,
		   line3,
		   line4);

	kfree(lineh);
	kfree(line1);
	kfree(line2);
	kfree(line3);
	kfree(line4);
	kfree(line5);
}

/* Offset all WC LREC fields by -24  as per CFA EAS */
#define WC_INFO_VALID(kptr)		tfc_getbits(kptr, (127 - 24), 1)
#define WC_INFO_REC_SIZE(kptr)		tfc_getbits(kptr, (125 - 24), 2)
#define WC_INFO_EPOCH0(kptr)		tfc_getbits(kptr, (113 - 24), 12)
#define WC_INFO_EPOCH1(kptr)		tfc_getbits(kptr, (107 - 24), 6)
#define WC_INFO_OPCODE(kptr)		tfc_getbits(kptr, (103 - 24), 4)
#define WC_INFO_STRENGTH(kptr)		tfc_getbits(kptr, (101 - 24), 2)
#define WC_INFO_ACT_HINT(kptr)		tfc_getbits(kptr, (99 - 24), 2)

#define WC_INFO_ACT_REC_PTR(kptr)	tfc_getbits(kptr, (73 - 24), 26)

#define WC_INFO_DESTINATION(kptr)	tfc_getbits(kptr, (73 - 24), 17)

#define WC_INFO_TCP_DIRECTION(kptr)	tfc_getbits(kptr, (72 - 24), 1)
#define WC_INFO_TCP_UPDATE_EN(kptr)	tfc_getbits(kptr, (71 - 24), 1)
#define WC_INFO_TCP_WIN(kptr)		tfc_getbits(kptr, (66 - 24), 5)
#define WC_INFO_TCP_MSB_LOC(kptr)	tfc_getbits(kptr, (48 - 24), 18)
#define WC_INFO_TCP_MSB_OPP(kptr)	tfc_getbits(kptr, (30 - 24), 18)
#define WC_INFO_TCP_MSB_OPP_INIT(kptr)	tfc_getbits(kptr, (29 - 24), 1)
#define WC_INFO_STATE(kptr)		tfc_getbits(kptr, (24 - 24), 5)

#define WC_INFO_RING_TABLE_IDX(kptr)	tfc_getbits(kptr, (64 - 24), 9)
#define WC_INFO_ACT_REC_SIZE(kptr)	tfc_getbits(kptr, (59 - 24), 5)
#define WC_INFO_PATHS_M1(kptr)		tfc_getbits(kptr, (55 - 24), 4)
#define WC_INFO_FC_OP(kptr)		tfc_getbits(kptr, (54 - 24), 1)
#define WC_INFO_FC_TYPE(kptr)		tfc_getbits(kptr, (52 - 24), 2)
#define WC_INFO_FC_PTR(kptr)		tfc_getbits(kptr, (24 - 24), 28)

#define WC_INFO_RECYCLE_DEST(kptr)	tfc_getbits(kptr, (72 - 24), 1)
#define WC_INFO_PROF_FUNC(kptr)		tfc_getbits(kptr, (64 - 24), 8)
#define WC_INFO_META_PROF(kptr)		tfc_getbits(kptr, (61 - 24), 3)
#define WC_INFO_METADATA(kptr)		tfc_getbits(kptr, (29 - 24), 32)

static void wc_tcam_decode(struct seq_file *m,
			   uint32_t *wc_res_ptr,
			   struct wc_lrec_t *wc_info,
			   struct tfc_ts_mem_cfg *act_mem_cfg)
{
	wc_info->valid    = WC_INFO_VALID(wc_res_ptr);
	wc_info->rec_size = WC_INFO_REC_SIZE(wc_res_ptr);
	wc_info->epoch0   = WC_INFO_EPOCH0(wc_res_ptr);
	wc_info->epoch1   = WC_INFO_EPOCH1(wc_res_ptr);
	wc_info->opcode   = WC_INFO_OPCODE(wc_res_ptr);
	wc_info->strength = WC_INFO_STRENGTH(wc_res_ptr);
	wc_info->act_hint = WC_INFO_ACT_HINT(wc_res_ptr);

	if (wc_info->opcode != 2 && wc_info->opcode != 3) {
		/* All but FAST */
		wc_info->act_rec_ptr = WC_INFO_ACT_REC_PTR(wc_res_ptr);
		act_process(m, wc_info->act_rec_ptr, &wc_info->act_info, act_mem_cfg);
	} else {
		/* Just FAST */
		wc_info->destination = WC_INFO_DESTINATION(wc_res_ptr);
	}

	if (wc_info->opcode == 4 || wc_info->opcode == 6) {
		/* CT only */
		wc_info->tcp_direction    = WC_INFO_TCP_DIRECTION(wc_res_ptr);
		wc_info->tcp_update_en    = WC_INFO_TCP_UPDATE_EN(wc_res_ptr);
		wc_info->tcp_win          = WC_INFO_TCP_WIN(wc_res_ptr);
		wc_info->tcp_msb_loc      = WC_INFO_TCP_MSB_LOC(wc_res_ptr);
		wc_info->tcp_msb_opp      = WC_INFO_TCP_MSB_OPP(wc_res_ptr);
		wc_info->tcp_msb_opp_init = WC_INFO_TCP_MSB_OPP_INIT(wc_res_ptr);
		wc_info->state            = WC_INFO_STATE(wc_res_ptr);
	} else if (wc_info->opcode != 8) {
		/* Not CT and nor RECYCLE */
		wc_info->ring_table_idx = WC_INFO_RING_TABLE_IDX(wc_res_ptr);
		wc_info->act_rec_size   = WC_INFO_ACT_REC_SIZE(wc_res_ptr);
		wc_info->paths_m1       = WC_INFO_PATHS_M1(wc_res_ptr);
		wc_info->fc_op          = WC_INFO_FC_OP(wc_res_ptr);
		wc_info->fc_type        = WC_INFO_FC_TYPE(wc_res_ptr);
		wc_info->fc_ptr         = WC_INFO_FC_PTR(wc_res_ptr);
	} else {
		/* Recycle */
		wc_info->recycle_dest = WC_INFO_RECYCLE_DEST(wc_res_ptr);
		wc_info->prof_func    = WC_INFO_PROF_FUNC(wc_res_ptr);
		wc_info->meta_prof    = WC_INFO_META_PROF(wc_res_ptr);
		wc_info->metadata     = WC_INFO_METADATA(wc_res_ptr);
	}
}

static void wc_tcam_show(struct seq_file *m, struct wc_lrec_t *wc_info)
{
	char *line1 = NULL;
	char *line2 = NULL;
	char *line3 = NULL;
	char *line4 = NULL;
	char tmp1[TFC_STRING_LENGTH_64];
	char tmp2[TFC_STRING_LENGTH_64];
	char tmp3[TFC_STRING_LENGTH_64];
	char tmp4[TFC_STRING_LENGTH_64];

	line1 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	line2 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	line3 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	line4 = kmalloc(TFC_STRING_LENGTH_256, GFP_KERNEL);
	if (!line1 || !line2 || !line3 || !line4) {
		kfree(line1);
		kfree(line2);
		kfree(line3);
		kfree(line4);
		seq_printf(m, "%s: Failed to allocate temp buffer\n",
			   __func__);
		return;
	}

	seq_printf(m, ":LREC: opcode:%s\n", get_lrec_opcode_str(wc_info->opcode));

	snprintf(line1, TFC_STRING_LENGTH_256, "+-+--+-Epoch-+--+--+--+");
	snprintf(line2, TFC_STRING_LENGTH_256, " V|rs|  0  1 |Op|St|ah|");
	snprintf(line3, TFC_STRING_LENGTH_256, "+-+--+----+--+--+--+--+");
	snprintf(line4, TFC_STRING_LENGTH_256, " %1d %2d %4d %2d %2d %2d %2d ",
		 wc_info->valid,
		 wc_info->rec_size,
		 wc_info->epoch0,
		 wc_info->epoch1,
		 wc_info->opcode,
		 wc_info->strength,
		 wc_info->act_hint);

	if (wc_info->opcode != 2 && wc_info->opcode != 3) {
		/* All but FAST */
		snprintf(tmp1, TFC_STRING_LENGTH_64, "-Act Rec--+");
		snprintf(tmp2, TFC_STRING_LENGTH_64, " Ptr      |");
		snprintf(tmp3, TFC_STRING_LENGTH_64, "----------+");
		snprintf(tmp4, TFC_STRING_LENGTH_64, "0x%08x ",
			 wc_info->act_rec_ptr);
	} else {
		/* Just FAST */
		snprintf(tmp1, TFC_STRING_LENGTH_64, "-------+");
		snprintf(tmp2, TFC_STRING_LENGTH_64, " Dest  |");
		snprintf(tmp3, TFC_STRING_LENGTH_64, "-------+");
		snprintf(tmp4, TFC_STRING_LENGTH_64, "0x05%x ",
			 wc_info->destination);
	}

	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);

	if (wc_info->opcode == 4 || wc_info->opcode == 6) {
		/* CT only */
		snprintf(tmp1, TFC_STRING_LENGTH_64, "--+--+-------------TCP-------+--+---+");
		snprintf(tmp2, TFC_STRING_LENGTH_64, "Dr|ue| Win|   lc  |   op  |oi|st|tmr|");
		snprintf(tmp3, TFC_STRING_LENGTH_64, "--+--+----+-------+-------+--+--+---+");
		snprintf(tmp4, TFC_STRING_LENGTH_64, "%2d %2d %4d %0x5x %0x5x %2d %2d %3d ",
			 wc_info->tcp_direction,
			 wc_info->tcp_update_en,
			 wc_info->tcp_win,
			 wc_info->tcp_msb_loc,
			 wc_info->tcp_msb_opp,
			 wc_info->tcp_msb_opp_init,
			 wc_info->state,
			 wc_info->timer_value);
	} else if (wc_info->opcode != 8) {
		/* Not CT and nor RECYCLE */
		snprintf(tmp1, TFC_STRING_LENGTH_64, "--+--+--+-------FC-------+");
		snprintf(tmp2, TFC_STRING_LENGTH_64, "RI|as|pm|op|tp|     Ptr  |");
		snprintf(tmp3, TFC_STRING_LENGTH_64, "--+--+--+--+--+----------+");
		snprintf(tmp4, TFC_STRING_LENGTH_64, "%2d %2d %2d %2d %2d 0x%08x ",
			 wc_info->ring_table_idx,
			 wc_info->act_rec_size,
			 wc_info->paths_m1,
			 wc_info->fc_op,
			 wc_info->fc_type,
			 wc_info->fc_ptr);
	} else {
		snprintf(tmp1, TFC_STRING_LENGTH_64, "--+--+--+---------+");
		snprintf(tmp2, TFC_STRING_LENGTH_64, "RD|pf|mp| cMData  |");
		snprintf(tmp3, TFC_STRING_LENGTH_64, "--+--+--+---------+");
		snprintf(tmp4, TFC_STRING_LENGTH_64, "%2d 0x%2x %2d %08x ",
			 wc_info->recycle_dest,
			 wc_info->prof_func,
			 wc_info->meta_prof,
			 wc_info->metadata);
	}

	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);

	snprintf(tmp1, TFC_STRING_LENGTH_64, "-----Range-+\n");
	snprintf(tmp2, TFC_STRING_LENGTH_64, "Prof|  Idx |\n");
	snprintf(tmp3, TFC_STRING_LENGTH_64, "----+------+\n");
	snprintf(tmp4, TFC_STRING_LENGTH_64, "0x%02x 0x%04x\n",
		 wc_info->range_profile,
		 wc_info->range_index);

	strcat(line1, tmp1);
	strcat(line2, tmp2);
	strcat(line3, tmp3);
	strcat(line4, tmp4);

	seq_printf(m, "%s%s%s%s",
		   line1,
		   line2,
		   line3,
		   line4);

	if (wc_info->opcode != 2 && wc_info->opcode != 3)
		act_show(m, &wc_info->act_info, wc_info->act_rec_ptr << 5);

	kfree(line1);
	kfree(line2);
	kfree(line3);
	kfree(line4);
}

static int tfc_tcam_process(struct ulp_flow_db_res_params *rp,
			    void *frp_ctxt)
{
	struct wc_frp_context *wc_frp = (struct wc_frp_context *)frp_ctxt;
	struct l2ctx_tcam_key_t *l2ctx_key_info, *l2ctx_mask_info;
	struct prof_tcam_key_t *ptkey_info, *ptmask_info;
	struct l2ctx_tcam_remap_t *l2ctx_rmp_info;
	struct prof_tcam_remap_t *ptrmp_info;
	struct seq_file *m = wc_frp->m;
	struct wc_lrec_t *wc_lrec;
	u8 *key, *mask, *remap;
	u16 key_sz = 128, remap_sz = 128;
	int rc = -ENOMEM;

	/* Allocate all temp storage */
	wc_lrec = kzalloc(sizeof(*wc_lrec), GFP_KERNEL);
	l2ctx_key_info = kzalloc(sizeof(*l2ctx_key_info), GFP_KERNEL);
	l2ctx_mask_info = kzalloc(sizeof(*l2ctx_mask_info), GFP_KERNEL);
	l2ctx_rmp_info = kzalloc(sizeof(*l2ctx_rmp_info), GFP_KERNEL);
	ptkey_info = kzalloc(sizeof(*ptkey_info), GFP_KERNEL);
	ptmask_info = kzalloc(sizeof(*ptmask_info), GFP_KERNEL);
	ptrmp_info = kzalloc(sizeof(*ptrmp_info), GFP_KERNEL);
	key = kzalloc(key_sz, GFP_KERNEL);
	mask = kzalloc(key_sz, GFP_KERNEL);
	remap = kzalloc(remap_sz, GFP_KERNEL);

	if (!wc_lrec || !l2ctx_key_info || !l2ctx_mask_info ||
	    !l2ctx_rmp_info || !ptkey_info || !ptmask_info ||
	    !ptrmp_info || !key || !mask || !remap) {
		seq_puts(m, "Out of memory:\n");
		seq_printf(m, "%p:%p:%p\n", wc_lrec, l2ctx_key_info, l2ctx_mask_info);
		seq_printf(m, "%p:%p:%p\n", l2ctx_rmp_info, ptkey_info, ptmask_info);
		seq_printf(m, "%p:%p:%p:%p\n", ptrmp_info, key, mask, remap);
		goto tcam_process_error;
	}

	rc = tfc_tcam_entry_read(wc_frp->ulp_ctxt,
				 rp->direction,
				 rp->resource_type,
				 rp->resource_hndl,
				 key,
				 mask,
				 remap,
				 &key_sz,
				 &remap_sz);
	if (rc) {
		seq_printf(m, "TCAM read error rc[%d]\n", rc);
		rc = -EINVAL;
		goto tcam_process_error;
	}

	/*
	 * Decode result, and extract act_ptr, only for L2 ctx or WC TCAM
	 * entries
	 */
	switch (rp->resource_type) {
	case CFA_RSUBTYPE_TCAM_L2CTX:
		seq_printf(m, "\n:L2CTX TCAM [%u]:\n", (u16)rp->resource_hndl);
		hex_buf_dump(m, "Key:", key, (int)key_sz, 1, 16);
		hex_buf_dump(m, "Mask:", mask, (int)key_sz, 1, 16);
		l2ctx_tcam_key_decode(m, (uint32_t *)key, l2ctx_key_info);
		l2ctx_tcam_key_decode(m, (uint32_t *)mask, l2ctx_mask_info);
		l2ctx_tcam_remap_decode(m, (uint32_t *)remap,
					l2ctx_rmp_info, wc_frp->act_mem_cfg);
		l2ctx_tcam_show(m, l2ctx_key_info, l2ctx_mask_info, l2ctx_rmp_info);
		break;

	case CFA_RSUBTYPE_TCAM_WC:
		seq_printf(m, "\n:WC TCAM [%u]:\n", (u16)rp->resource_hndl);
		hex_buf_dump(m, "Key:", key, (int)key_sz, 1, 16);
		hex_buf_dump(m, "Mask:", mask, (int)key_sz, 1, 16);
		wc_tcam_decode(m, (uint32_t *)remap, wc_lrec, wc_frp->act_mem_cfg);
		wc_tcam_show(m, wc_lrec);
		break;

	case CFA_RSUBTYPE_TCAM_PROF_TCAM:
		seq_printf(m, "\n:Profile TCAM [%u]:\n", (u16)rp->resource_hndl);
		hex_buf_dump(m, "Key:", key, (int)key_sz, 1, 16);
		hex_buf_dump(m, "Mask:", mask, (int)key_sz, 1, 16);
		prof_tcam_key_decode(m, (uint32_t *)key, ptkey_info);
		prof_tcam_key_decode(m, (uint32_t *)mask, ptmask_info);
		prof_tcam_remap_decode(m, (uint32_t *)remap, ptrmp_info);
		prof_tcam_show(m, ptkey_info, ptmask_info, ptrmp_info);
		break;

	case CFA_RSUBTYPE_TCAM_CT_RULE:
	case CFA_RSUBTYPE_TCAM_VEB:
	case CFA_RSUBTYPE_TCAM_FEATURE_CHAIN:
		seq_printf(m, "Unsupported decode: %s\n", tfc_tcam_2_str(rp->resource_type));
	default:
		break;
	}

tcam_process_error:
	kfree(l2ctx_key_info);
	kfree(l2ctx_mask_info);
	kfree(l2ctx_rmp_info);
	kfree(wc_lrec);
	kfree(ptkey_info);
	kfree(ptmask_info);
	kfree(ptrmp_info);
	kfree(key);
	kfree(mask);
	kfree(remap);
	return rc;
}

/*
 * Check for the following conditions:
 * 1. res_dir == p_res_dir
 * 2. res_func == p_res_func
 * 3. res_type == p_res_type
 * 4. if res_dir == CFA_DIR_MAX, skip #1
 * 5. if res_func == BNXT_ULP_RESOURCE_FUNC_INVALID, skip #2
 * 6. if res_type == CFA_RSUBTYPE_TCAM_MAX, skip #3
 *
 * Bascally implements a wildcarded match for either-or-and all conditions.
 */
static bool tfc_flow_db_resource_filter(u32 p_res_dir, u32 p_res_func, u32 p_res_type,
					u32 res_dir, u32 res_func, u32 res_type)
{
	if (res_dir != CFA_DIR_MAX && p_res_dir != res_dir)
		return false;

	/* res_dir == CFA_DIR_MAX */
	if (res_func == -1 &&
	    res_type == -1)
		return true;
	else if (res_func == -1 &&
		 res_type == p_res_type)
		return true;
	else if (res_func == p_res_func &&
		 res_type == -1)
		return true;

	return false;
}

/**
 * Walk through a resource matching a spec (resource_func+resource_type) for a
 * particular flow (or all flows), and call a processing callback to handle
 * data per resource/type.
 *
 * @ulp_ctxt:      Ptr to ulp_context
 * @flow_type:     FDB flow type (default/regular)
 * @resource_func: if zero then all resource_funcs are dumped.
 * @resource_type: if zero then all resource_types are dumped.
 * @frp:           FDB resource processor function
 * @frp_ctxt:      FDB resource processor context
 *
 * returns 0 if success, error code if not
 */
static
int tfc_flow_db_resource_walk(struct bnxt_ulp_context *ulp_ctxt, u8 flow_type,
			      u32 resource_dir, u32 resource_func, u32 resource_type,
			      FDB_RESOURCE_PROCFUNC frp,
			      void *frp_ctxt)
{
	struct wc_frp_context *wc_frp = (struct wc_frp_context *)frp_ctxt;
	struct seq_file *m = wc_frp->m;
	struct ulp_flow_db_res_params params;
	struct bnxt_ulp_flow_tbl *flow_tbl;
	struct bnxt_ulp_flow_db *flow_db;
	u32 ridx, fid = 1;
	int rc;

	if (!ulp_ctxt || !ulp_ctxt->cfg_data)
		return -EINVAL;

	if (!frp) {
		seq_puts(m, "No FDB proc_func\n");
		return -EINVAL;
	}

	flow_db = bnxt_ulp_cntxt_ptr2_flow_db_get(ulp_ctxt);
	if (!flow_db) {
		seq_puts(m, "Invalid Arguments\n");
		return -EINVAL;
	}

	flow_tbl = &flow_db->flow_tbl;

#if (TFC_DEBUG_DUMP_ALL_FLOWS)
	for (fid = 1; fid < flow_tbl->num_flows; fid++) {
#else
	while (!ulp_flow_db_next_entry_get(ulp_ctxt, flow_db, flow_type, &fid)) {
#endif
		ridx = 0;

		rc = ulp_flow_db_resource_get(ulp_ctxt, flow_type, fid,
					      &ridx, &params);
		if (!rc) {
			if (tfc_flow_db_resource_filter(params.direction,
							params.resource_func,
							params.resource_type,
							resource_dir,
							resource_func,
							resource_type)) {
				(*frp)(&params, (void *)frp_ctxt);
			}

			do {
				rc = ulp_flow_db_resource_get(ulp_ctxt, flow_type, fid,
							      &ridx, &params);
				if (!rc) {
					if (tfc_flow_db_resource_filter(params.direction,
									params.resource_func,
									params.resource_type,
									resource_dir,
									resource_func,
									resource_type))
						(*frp)(&params, (void *)frp_ctxt);
				}
			} while (ridx);
		}
	}
	return 0;
}

int tfc_wc_show(struct seq_file *m, struct tfc *tfcp, uint8_t tsid, enum cfa_dir dir)
{
	struct tfc_ts_mem_cfg *act_mem_cfg;
	struct bnxt_ulp_context *ulp_ctx;
	enum cfa_scope_type scope_type;
	struct wc_frp_context wc_frp;
	bool is_bs_owner;
	struct bnxt *bp;
	bool valid;
	u16 num_vfs;
	int rc = 0;

	rc = tfo_ts_get(tfcp->tfo, tsid, &scope_type, NULL, &valid, NULL);
	if (rc != 0) {
		seq_printf(m, "%s: failed to get tsid: %d\n",
			   __func__, rc);
		return -EINVAL;
	}
	if (!valid) {
		seq_printf(m, "%s: tsid not allocated %d\n",
			   __func__, tsid);
		return -EINVAL;
	}

	act_mem_cfg = kzalloc(sizeof(*act_mem_cfg), GFP_KERNEL);
	if (!act_mem_cfg)
		return -ENOMEM;

	rc = tfo_ts_get_mem_cfg(tfcp->tfo, tsid,
				dir,
				CFA_REGION_TYPE_ACT,
				&is_bs_owner,
				act_mem_cfg);   /* Gets rec_cnt */
	if (rc != 0) {
		seq_printf(m, "%s: tfo_ts_get_mem_cfg() failed for ACT: %d\n",
			   __func__, rc);
		kfree(act_mem_cfg);
		return -EINVAL;
	}

	if (tfcp &&
	    tfcp->bp) {
		bp = (struct bnxt *)(tfcp->bp);
		num_vfs = pci_num_vf(bp->pdev);
		ulp_ctx = bp->ulp_ctx;

		if (ulp_ctx) {
			wc_frp.ulp_ctxt = ulp_ctx;
			wc_frp.m = m;
			wc_frp.act_mem_cfg = act_mem_cfg;

			/* Dump-decode all TCAM resources for default flows */
			seq_puts(m, "\nDefault flows TCAM:\n");
			seq_puts(m, "===================\n");
			tfc_flow_db_resource_walk(ulp_ctx, BNXT_ULP_FDB_TYPE_DEFAULT, dir,
						  BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
						  -1, &tfc_tcam_process, (void *)&wc_frp);
			/* Dump-decode all TCAM resources for resource-id flows */
			seq_puts(m, "\nRID flows TCAM:\n");
			seq_puts(m, "===============\n");
			tfc_flow_db_resource_walk(ulp_ctx, BNXT_ULP_FDB_TYPE_RID, dir,
						  BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
						  -1, &tfc_tcam_process, (void *)&wc_frp);
			/* Dump-decode all TCAM resources for regular flows */
			seq_puts(m, "\nRegular flows TCAM:\n");
			seq_puts(m, "===================\n");
			tfc_flow_db_resource_walk(ulp_ctx, BNXT_ULP_FDB_TYPE_REGULAR, dir,
						  BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
						  -1, &tfc_tcam_process, (void *)&wc_frp);
		}
	}

	kfree(act_mem_cfg);
	return rc;
}
