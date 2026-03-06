/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_EM_H_
#define _TFC_EM_H_

/* Derived from CAS document */
#define TFC_MPC_MAX_TX_BYTES 188
#define TFC_MPC_MAX_RX_BYTES 188

#define TFC_MPC_HEADER_SIZE_BYTES 16

#define TFC_MPC_BYTES_PER_WORD 32
#define TFC_MPC_MAX_TABLE_READ_WORDS 4
#define TFC_MPC_MAX_TABLE_READ_BYTES \
	(TFC_MPC_BYTES_PER_WORD * TFC_MPC_MAX_TABLE_READ_WORDS)

#define TFC_EM_LREC_SZ_32_BIT_WORDS 8

#define TFC_BUCKET_ENTRIES 6

/* MPC opaque currently unused */
#define TFC_MPC_OPAQUE_VAL 0

#define TFC_MOD_STRING_LENGTH  512
#define TFC_STAT_STRING_LENGTH 128
#define TFC_ENC_STRING_LENGTH  256

struct act_compact_info_t {
	bool drop;
	uint8_t vlan_del_rep;
	uint8_t dest_op;
	uint16_t vnic_vport;
	uint8_t decap_func;
	uint8_t mirror;
	uint16_t meter_ptr;
	uint8_t stat0_ctr_type;
	bool stat0_ing_egr;
	uint8_t stat0_offs;
	uint8_t mod_offs;
	uint8_t enc_offs;
	uint8_t src_offs;
	char mod_str[512];
	char stat0_str[128];
	char enc_str[256];
};

struct act_full_info_t {
	bool drop;
	uint8_t vlan_del_rep;
	uint8_t dest_op;
	uint16_t vnic_vport;
	uint8_t decap_func;
	uint16_t mirror;
	uint16_t meter_ptr;
	uint8_t stat0_ctr_type;
	bool stat0_ing_egr;
	uint32_t stat0_ptr;
	uint8_t stat1_ctr_type;
	bool stat1_ing_egr;
	uint32_t stat1_ptr;
	uint32_t mod_ptr;
	uint32_t enc_ptr;
	uint32_t src_ptr;
	char mod_str[512];
	char stat0_str[128];
	char stat1_str[128];
	char enc_str[256];
};

struct act_mcg_info_t {
	uint8_t src_ko_en;
	uint32_t nxt_ptr;
	uint8_t act_hint0;
	uint32_t act_rec_ptr0;
	uint8_t act_hint1;
	uint32_t act_rec_ptr1;
	uint8_t act_hint2;
	uint32_t act_rec_ptr2;
	uint8_t act_hint3;
	uint32_t act_rec_ptr3;
	uint8_t act_hint4;
	uint32_t act_rec_ptr4;
	uint8_t act_hint5;
	uint32_t act_rec_ptr5;
	uint8_t act_hint6;
	uint32_t act_rec_ptr6;
	uint8_t act_hint7;
	uint32_t act_rec_ptr7;
};

struct act_info_t {
	bool valid;
	uint8_t vector;
	union {
		struct act_compact_info_t compact;
		struct act_full_info_t full;
		struct act_mcg_info_t mcg;
	};
};

struct em_info_t {
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
	u8 *key;
	struct act_info_t act_info;
};

struct sb_entry_t {
	u16 hash_msb;
	u32 entry_ptr;
};

struct bucket_info_t {
	bool valid;
	bool chain;
	u32 chain_ptr;
	struct sb_entry_t entries[TFC_BUCKET_ENTRIES];
	struct em_info_t em_info[TFC_BUCKET_ENTRIES];
};

#define CALC_NUM_RECORDS_IN_POOL(a, b, c)

/* Calculates number of 32Byte records from total size in 32bit words */
#define CALC_NUM_RECORDS(result, key_sz_words) \
	(*(result) = (((key_sz_words) + 7) / 8))

/* Calculates the entry offset */
#define CREATE_OFFSET(result, pool_sz_exp, pool_id, record_offset) \
	(*(result) = (((pool_id) << (pool_sz_exp)) | (record_offset)))

#define REMOVE_POOL_FROM_OFFSET(pool_sz_exp, record_offset) \
	(((1 << (pool_sz_exp)) - 1) & (record_offset))

int tfc_em_delete_raw(struct tfc *tfcp,
		      u8 tsid,
		      enum cfa_dir dir,
		      u32 offset,
		      u32 static_bucket,
		      struct tfc_mpc_batch_info_t *batch_info);

int tfc_em_delete_entries_by_pool_id(struct tfc *tfcp,
				     u8 tsid,
				     enum cfa_dir dir,
				     u16 pool_id,
				     u8 debug,
				     void *data_va,
				     dma_addr_t data_pa);

int tfc_act_set_response(struct bnxt *bp,
			 struct cfa_bld_mpcinfo *mpc_info,
			 struct bnxt_mpc_mbuf *mpc_msg_out,
			 u8 *rx_msg);

int tfc_act_get_only_response(struct bnxt *bp,
			      struct cfa_bld_mpcinfo *mpc_info,
			      struct bnxt_mpc_mbuf *mpc_msg_out,
			      uint8_t *rx_msg,
			      uint16_t *data_sz_words);

int tfc_act_get_clear_response(struct bnxt *bp,
			       struct cfa_bld_mpcinfo *mpc_info,
			       struct bnxt_mpc_mbuf *mpc_msg_out,
			       uint8_t *rx_msg,
			       uint16_t *data_sz_words);

int tfc_mpc_send(struct bnxt *bp,
		 struct bnxt_mpc_mbuf *in_msg,
		 struct bnxt_mpc_mbuf *out_msg,
		 u32 *opaque,
		 int type);

#endif /* _TFC_EM_H_ */
