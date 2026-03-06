/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

/* Exported functions for CFA HW programming */

#ifndef _HCAPI_CFA_DEFS_H_
#define _HCAPI_CFA_DEFS_H_

#include <linux/types.h>

#define CFA_BITS_PER_BYTE (8)
#define CFA_BITS_PER_WORD (sizeof(u32) * CFA_BITS_PER_BYTE)
#define __CFA_ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define CFA_ALIGN(x, a) __CFA_ALIGN_MASK((x), (a) - 1)
#define CFA_ALIGN_256(x) CFA_ALIGN(x, 256)
#define CFA_ALIGN_128(x) CFA_ALIGN(x, 128)
#define CFA_ALIGN_32(x) CFA_ALIGN(x, 32)

#define NUM_WORDS_ALIGN_32BIT(x) (CFA_ALIGN_32(x) / CFA_BITS_PER_WORD)
#define NUM_WORDS_ALIGN_128BIT(x) (CFA_ALIGN_128(x) / CFA_BITS_PER_WORD)
#define NUM_WORDS_ALIGN_256BIT(x) (CFA_ALIGN_256(x) / CFA_BITS_PER_WORD)

/* TODO: redefine according to chip variant */
#define CFA_GLOBAL_CFG_DATA_SZ (100)

#ifndef SUPPORT_CFA_HW_P4
#define SUPPORT_CFA_HW_P4 (0)
#endif

#ifndef SUPPORT_CFA_HW_P45
#define SUPPORT_CFA_HW_P45 (0)
#endif

#ifndef SUPPORT_CFA_HW_P58
#define SUPPORT_CFA_HW_P58 (0)
#endif

#ifndef SUPPORT_CFA_HW_P59
#define SUPPORT_CFA_HW_P59 (0)
#endif

#if SUPPORT_CFA_HW_P4 && SUPPORT_CFA_HW_P45 && SUPPORT_CFA_HW_P58 &&           \
	SUPPORT_CFA_HW_P59
#define SUPPORT_CFA_HW_ALL (1)
#endif

#if SUPPORT_CFA_HW_ALL
#include "hcapi_cfa_p4.h"
#include "hcapi_cfa_p58.h"

#define CFA_PROF_L2CTXT_TCAM_MAX_FIELD_CNT CFA_P58_PROF_L2_CTXT_TCAM_MAX_FLD
#define CFA_PROF_L2CTXT_REMAP_MAX_FIELD_CNT CFA_P58_PROF_L2_CTXT_RMP_DR_MAX_FLD
#define CFA_PROF_MAX_KEY_CFG_SZ sizeof(struct cfa_p58_prof_key_cfg)
#define CFA_KEY_MAX_FIELD_CNT CFA_P58_KEY_FLD_ID_MAX
#define CFA_ACT_MAX_TEMPLATE_SZ sizeof(struct cfa_p58_action_template)
#else
#if SUPPORT_CFA_HW_P4 || SUPPORT_CFA_HW_P45
#include "hcapi_cfa_p4.h"
#define CFA_PROF_L2CTXT_TCAM_MAX_FIELD_CNT CFA_P40_PROF_L2_CTXT_TCAM_MAX_FLD
#define CFA_PROF_L2CTXT_REMAP_MAX_FIELD_CNT CFA_P40_PROF_L2_CTXT_RMP_DR_MAX_FLD
#define CFA_PROF_MAX_KEY_CFG_SZ sizeof(struct cfa_p4_prof_key_cfg)
#define CFA_KEY_MAX_FIELD_CNT CFA_P40_KEY_FLD_ID_MAX
#define CFA_ACT_MAX_TEMPLATE_SZ sizeof(struct cfa_p4_action_template)
#endif
#if SUPPORT_CFA_HW_P58
#include "hcapi_cfa_p58.h"
#define CFA_PROF_L2CTXT_TCAM_MAX_FIELD_CNT CFA_P58_PROF_L2_CTXT_TCAM_MAX_FLD
#define CFA_PROF_L2CTXT_REMAP_MAX_FIELD_CNT CFA_P58_PROF_L2_CTXT_RMP_DR_MAX_FLD
#define CFA_PROF_MAX_KEY_CFG_SZ sizeof(struct cfa_p5_prof_key_cfg)
#define CFA_KEY_MAX_FIELD_CNT CFA_P58_KEY_FLD_ID_MAX
#define CFA_ACT_MAX_TEMPLATE_SZ sizeof(struct cfa_p58_action_template)
#endif
#if SUPPORT_CFA_HW_P59
#include "hcapi_cfa_p59.h"
#define CFA_PROF_L2CTXT_TCAM_MAX_FIELD_CNT CFA_P59_PROF_L2_CTXT_TCAM_MAX_FLD
#define CFA_PROF_L2CTXT_REMAP_MAX_FIELD_CNT CFA_P59_PROF_L2_CTXT_RMP_DR_MAX_FLD
#define CFA_PROF_MAX_KEY_CFG_SZ sizeof(struct cfa_p59_prof_key_cfg)
#define CFA_KEY_MAX_FIELD_CNT CFA_P59_EM_KEY_LAYOUT_MAX_FLD
#define CFA_ACT_MAX_TEMPLATE_SZ sizeof(struct cfa_p59_action_template)
#endif
#endif /* SUPPORT_CFA_HW_ALL */

/* Hashing defines */
#define HCAPI_CFA_LKUP_SEED_MEM_SIZE 512

/* CRC32i support for Key0 hash */
extern const u32 crc32tbl[];
#define ucrc32(ch, crc) (crc32tbl[((crc) ^ (ch)) & 0xff] ^ ((crc) >> 8))

/* CFA HW version definition */
enum hcapi_cfa_ver {
	HCAPI_CFA_P40 = 0, /* CFA phase 4.0 */
	HCAPI_CFA_P45 = 1, /* CFA phase 4.5 */
	HCAPI_CFA_P58 = 2, /* CFA phase 5.8 */
	HCAPI_CFA_P59 = 3, /* CFA phase 5.9 */
	HCAPI_CFA_PMAX = 4
};

/* CFA direction definition */
enum hcapi_cfa_dir {
	HCAPI_CFA_DIR_RX = 0, /* Receive */
	HCAPI_CFA_DIR_TX = 1, /* Transmit */
	HCAPI_CFA_DIR_MAX = 2
};

/* CFA HW OPCODE definition */
enum hcapi_cfa_hwops {
	HCAPI_CFA_HWOPS_PUT,   /* Write to HW operation */
	HCAPI_CFA_HWOPS_GET,   /* Read from HW operation */
	HCAPI_CFA_HWOPS_ADD,   /* For operations which require more then
				* simple writes to HW, this operation is
				* used.  The distinction with this operation
				* when compared to the PUT ops is that this
				* operation is used in conjunction with
				* the HCAPI_CFA_HWOPS_DEL op to remove
				* the operations issued by the ADD OP.
				*/
	HCAPI_CFA_HWOPS_DEL,   /* Beside to delete from the hardware, this
				* operation is also undo the add operation
				* performed by the HCAPI_CFA_HWOPS_ADD op.
				*/
	HCAPI_CFA_HWOPS_EVICT, /* This operaton is used to evict entries from
				* CFA cache memories. This operation is only
				* applicable to tables that use CFA caches.
				*/
	HCAPI_CFA_HWOPS_MAX
};

/* CFA HW KEY CONTROL OPCODE definition */
enum hcapi_cfa_key_ctrlops {
	HCAPI_CFA_KEY_CTRLOPS_INSERT, /* insert control bits */
	HCAPI_CFA_KEY_CTRLOPS_STRIP,  /* strip control bits */
	HCAPI_CFA_KEY_CTRLOPS_MAX
};

/**
 * CFA HW field structure definition
 * @bitops:	Starting bit position pf the HW field within a HW table
 *		entry.
 * @bitlen:	Number of bits for the HW field.
 */
struct hcapi_cfa_field {
	u16	bitpos;
	u16	bitlen;
};

/**
 * CFA HW table entry layout structure definition
 * @is_msb_order:	Bit order of layout
 * @total_sz_in_bits:	Size in bits of entry
 * @field_array:	data pointer of the HW layout fields array
 * @array_sz:		number of HW field entries in the HW layout field array
 * @layout_id:		layout id associated with the layout
 */
struct hcapi_cfa_layout {
	bool			is_msb_order;
	u32			total_sz_in_bits;
	struct hcapi_cfa_field	*field_array;
	u32			array_sz;
	u16			layout_id;
};

/**
 * CFA HW data object definition
 * @field_id:	HW field identifier. Used as an index to a HW table layout
 * @val:	Value of the HW field
 */
struct hcapi_cfa_data_obj {
	u16	field_id;
	u64	val;
};

/**
 * CFA HW definition
 * @base_addr:	HW table base address for the operation with optional device
 *		handle. For on-chip HW table operation, this is the either
 *		the TX or RX CFA HW base address. For off-chip table, this
 *		field is the base memory address of the off-chip table.
 * @handle:	Optional opaque device handle. It is generally used to access
 *		an GRC register space through PCIE BAR and passed to the BAR
 *		memory accessor routine.
 */
struct hcapi_cfa_hw {
	u64	base_addr;
	void	*handle;
};

/**
 * CFA HW operation definition
 * @opcode:	HW opcode
 * @hw:		CFA HW information used by accessor routines
 */
struct hcapi_cfa_hwop {
	enum hcapi_cfa_hwops	opcode;
	struct hcapi_cfa_hw	hw;
};

/**
 * CFA HW data structure definition
 * @union:	physical offset to the HW table for the data to be
 *		written to.  If this is an array of registers, this is the
 *		index into the array of registers.  For writing keys, this
 *		is the byte pointer into the memory where the key should be
 *		written.
 * @data:	HW data buffer pointer
 * @data_mask:	HW data mask buffer pointer. When the CFA data is a FKB and
 *		data_mask pointer is NULL, then the default mask to enable
 *		all bit will be used.
 * @data_sz:	size of the HW data buffer in bytes
 */
struct hcapi_cfa_data {
	union {
		u32	index;
		u32	byte_offset;
	};
	u8	*data;
	u8	*data_mask;
	u16	data_sz;
};

/********************** Truflow start ***************************/
enum hcapi_cfa_pg_tbl_lvl {
	TF_PT_LVL_0,
	TF_PT_LVL_1,
	TF_PT_LVL_2,
	TF_PT_LVL_MAX
};

enum hcapi_cfa_em_table_type {
	TF_KEY0_TABLE,
	TF_KEY1_TABLE,
	TF_RECORD_TABLE,
	TF_EFC_TABLE,
	TF_ACTION_TABLE,
	TF_EM_LKUP_TABLE,
	TF_MAX_TABLE
};

struct hcapi_cfa_em_page_tbl {
	u32	pg_count;
	u32	pg_size;
	void	**pg_va_tbl;
	u64	*pg_pa_tbl;
};

struct hcapi_cfa_em_table {
	int				type;
	u32				num_entries;
	u16				ctx_id;
	u32				entry_size;
	int				num_lvl;
	u32				page_cnt[TF_PT_LVL_MAX];
	u64				num_data_pages;
	void				*l0_addr;
	u64				l0_dma_addr;
	struct hcapi_cfa_em_page_tbl	pg_tbl[TF_PT_LVL_MAX];
};

struct hcapi_cfa_em_ctx_mem_info {
	struct hcapi_cfa_em_table em_tables[TF_MAX_TABLE];
};

/********************** Truflow end ****************************/

/**
 * CFA HW key table definition
 * Applicable to EEM and off-chip EM table only.
 * @base0:	For EEM, this is the KEY0 base mem pointer. For off-chip EM,
 *		this is the base mem pointer of the key table.
 * @size:	total size of the key table in bytes. For EEM, this size is
 *		same for both KEY0 and KEY1 table.
 * @num_buckets:	number of key buckets, applicable for newer chips
 * @base1:	For EEM, this is KEY1 base mem pointer. Fo off-chip EM,
 *		this is the key record memory base pointer within the key
 *		table, applicable for newer chip
 * @bs_db:	Optional - If the table is managed by a Backing Store
 *		database, then this object can be use to configure the EM Key.
 * @page_size:	Page size for EEM tables
 */
struct hcapi_cfa_key_tbl {
	u8			*base0;
	u32			size;
	u32			num_buckets;
	u8			*base1;
	struct hcapi_cfa_bs_db	*bs_db;
	u32			page_size;
};

/**
 * CFA HW key buffer definition
 * @data: pointer to the key data buffer
 * @len: buffer len in bytes
 * @layout: Pointer to the key layout
 */
struct hcapi_cfa_key_obj {
	u32				*data;
	u32				len;
	struct hcapi_cfa_key_layout	*layout;
};

/**
 * CFA HW key data definition
 * @offset:	For on-chip key table, it is the offset in unit of smallest
 *		key. For off-chip key table, it is the byte offset relative
 *		to the key record memory base and adjusted for page and
 *		entry size.
 * @data:	HW key data buffer pointer
 * @size:	size of the key in bytes
 * @tbl_scope:	optional table scope ID
 * @metadata:	the fid owner of the key
 *		stored with the bucket which can be used by
 *		the caller to retrieve later via the GET HW OP.
 */
struct hcapi_cfa_key_data {
	u32	offset;
	u8	*data;
	u16	size;
	u8	tbl_scope;
	u64	metadata;
};

/**
 * CFA HW key location definition
 * @bucket_mem_ptr:	on-chip EM bucket offset or off-chip EM bucket
 *			mem pointer
 * @mem_ptr:		off-chip EM key offset mem pointer
 * @bucket_mem_idx:	index within the array of the EM buckets
 * @bucket_idx:		index within the EM bucket
 * @mem_idx:		index within the EM records
 */
struct hcapi_cfa_key_loc {
	u64 bucket_mem_ptr;
	u64 mem_ptr;
	u32 bucket_mem_idx;
	u8  bucket_idx;
	u32 mem_idx;
};

/**
 * CFA HW layout table definition
 * @tbl:	data pointer to an array of fix formatted layouts supported.
 *		The index to the array is the CFA HW table ID
 * @num_layouts:  number of fix formatted layouts in the layout array
 */
struct hcapi_cfa_layout_tbl {
	const struct hcapi_cfa_layout	*tbl;
	u16				num_layouts;
};

/**
 * Key template consists of key fields that can be enabled/disabled
 * individually.
 * @field_en:		key field enable field array, set 1 to the
 *			correspeonding field enable to make a field valid
 * @is_wc_tcam_key:	Identify if the key template is for TCAM. If false,
 *			the key template is for EM. This field is
 *			mandantory for device that only support fix key
 *			formats.
 * @is_ipv6_key:	Identify if the key template will be use for
 *			IPv6 Keys.
 */
struct hcapi_cfa_key_template {
	u8	field_en[CFA_KEY_MAX_FIELD_CNT];
	bool	is_wc_tcam_key;
	bool	is_ipv6_key;
};

/**
 * key layout consist of field array, key bitlen, key ID, and other meta data
 * pertain to a key
 * @layout:	key layout data
 * @bitlen:	actual key size in number of bits
 * @id:		key identifier and this field is only valid for device
 *		that supports fix key formats
 * @is_wc_tcam_key:	Identified the key layout is WC TCAM key
 * @is_ipv6_key:	Identify if the key template will be use for IPv6 Keys.
 * @slices_size:	total slices size, valid for WC TCAM key only. It can
 *			be used by the user to determine the total size of WC
 *			TCAM key slices in bytes.
 */
struct hcapi_cfa_key_layout {
	struct hcapi_cfa_layout	*layout;
	u16			bitlen;
	u16			id;
	bool			is_wc_tcam_key;
	bool			is_ipv6_key;
	u16			slices_size;
};

/**
 * key layout memory contents
 * @key_layout:	key layouts
 * @layout:	layout
 * @field_array: fields
 */
struct hcapi_cfa_key_layout_contents {
	struct hcapi_cfa_key_layout	key_layout;
	struct hcapi_cfa_layout		layout;
	struct hcapi_cfa_field		field_array[CFA_KEY_MAX_FIELD_CNT];
};

/**
 * Action template consists of action fields that can be enabled/disabled
 * individually.
 * @hw_ver:	CFA version for the action template
 * @data:	action field enable field array, set 1 to the correspeonding
 *		field enable to make a field valid
 */
struct hcapi_cfa_action_template {
	enum hcapi_cfa_ver	hw_ver;
	u8			data[CFA_ACT_MAX_TEMPLATE_SZ];
};

/**
 * Action record info
 * @blk_id:	action SRAM block ID for on-chip action records or table
 *		scope of the action backing store
 * @offset:	offset
 */
struct hcapi_cfa_action_addr {
	u16 blk_id;
	u32 offset;
};

/**
 * Action data definition
 * @addr:	action record addr info for on-chip action records
 * @data:	pointer to the action data buffer
 * @len:	action data buffer len in bytes
 */
struct hcapi_cfa_action_data {
	struct hcapi_cfa_action_addr	addr;
	u32				*data;
	u32				len;
};

/**
 * Action object definition
 * @data:	pointer to the action data buffer
 * @len:	buffer len in bytes
 * @layout:	pointer to the action layout
 */
struct hcapi_cfa_action_obj {
	u32				*data;
	u32				len;
	struct hcapi_cfa_action_layout	*layout;
};

/**
 * action layout consist of field array, action wordlen and action format ID
 * @id:		action identifier
 * @layout:	action layout data
 * @bitlen:	actual action record size in number of bits
 */
struct hcapi_cfa_action_layout {
	u16			id;
	struct hcapi_cfa_layout	*layout;
	u16			bitlen;
};

/* CFA backing store type definition */
enum hcapi_cfa_bs_type {
	HCAPI_CFA_BS_TYPE_LKUP, /* EM LKUP backing store type */
	HCAPI_CFA_BS_TYPE_ACT,  /* Action backing store type */
	HCAPI_CFA_BS_TYPE_MAX
};

/* CFA backing store configuration data object */
struct hcapi_cfa_bs_cfg {
	enum hcapi_cfa_bs_type	type;
	u16			tbl_scope;
	struct hcapi_cfa_bs_db	*bs_db;
};

/**
 * CFA backing store data base object
 * @signature:	memory manager database signature
 * @mgmt_db:	memory manager database base pointer  (VA)
 * @mgmt_db_sz:	memory manager database size in bytes
 * @bs_ptr:	Backing store memory pool base pointer
 *		(VA – backed by IOVA which is DMA accessible)
 * @offset:	bs_offset - byte offset to the section of the backing
 *		store memory managed by the backing store memory manager.
 *		For EM backing store, this is the starting byte offset
 *		to the EM record memory. For Action backing store, this
 *		offset is 0.
 * @bs_sz:	backing store memory pool size in bytes
 */
struct hcapi_cfa_bs_db {
	u32	signature;
#define HCAPI_CFA_BS_SIGNATURE 0xCFA0B300
	void	*mgmt_db;
	u32	mgmt_db_sz;
	void	*bs_ptr;
	u32	offset;
	u32	bs_sz;
};

/**
 *  defgroup CFA_HCAPI_PUT_API
 *  HCAPI used for writing to the hardware
 */

/**
 * This API provides the functionality to program a specified value to a
 * HW field based on the provided programming layout.
 *
 * @data_buf:	A data pointer to a CFA HW key/mask data
 * @layout:	A pointer to CFA HW programming layout
 * @field_id:	ID of the HW field to be programmed
 * @val:	Value of the HW field to be programmed
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_put_field(u64 *data_buf, const struct hcapi_cfa_layout *layout,
			u16 field_id, u64 val);

/**
 * This API provides the functionality to program an array of field values
 * with corresponding field IDs to a number of profiler sub-block fields
 * based on the fixed profiler sub-block hardware programming layout.
 *
 * @obj_data:	A pointer to a CFA profiler key/mask object data
 * @layout:	A pointer to CFA HW programming layout
 * @field_tbl:	A pointer to an array that consists of the object field
 *		ID/value pairs
 * @field_tbl_sz:	Number of entries in the table
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_put_fields(u64 *obj_data, const struct hcapi_cfa_layout *layout,
			 struct hcapi_cfa_data_obj *field_tbl,
			 u16 field_tbl_sz);
/**
 * This API provides the functionality to program an array of field values
 * with corresponding field IDs to a number of profiler sub-block fields
 * based on the fixed profiler sub-block hardware programming layout. This
 * API will swap the n byte blocks before programming the field array.
 *
 * @obj_data:	A pointer to a CFA profiler key/mask object data
 * @layout:	A pointer to CFA HW programming layout
 * @field_tbl:	A pointer to an array that consists of the object field
 *		ID/value pairs
 * @field_tbl_sz:	Number of entries in the table
 * @data_size:	size of the data in bytes
 * @n:		block size in bytes
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_put_fields_swap(u64 *obj_data,
			      const struct hcapi_cfa_layout *layout,
			      struct hcapi_cfa_data_obj *field_tbl,
			      u16 field_tbl_sz, u16 data_size,
			      u16 n);
/**
 * This API provides the functionality to write a value to a
 * field within the bit position and bit length of a HW data
 * object based on a provided programming layout.
 *
 * @act_obj:	A pointer of the action object to be initialized
 * @layout:	A pointer of the programming layout
 * @field_id:	Identifier of the HW field
 * @bitpos_adj:	Bit position adjustment value
 * @bitlen_adj:	Bit length adjustment value
 * @val:	HW field value to be programmed
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_put_field_rel(u64 *obj_data,
			    const struct hcapi_cfa_layout *layout,
			    u16 field_id, int16_t bitpos_adj,
			    s16 bitlen_adj, u64 val);

/**
 *  defgroup CFA_HCAPI_GET_API
 *  HCAPI used for reading from the hardware
 */

/**
 * This API provides the functionality to get the word length of
 * a layout object.
 *
 * @layout:	A pointer of the HW layout
 * @return:
 *   Word length of the layout object
 */
u16 hcapi_cfa_get_wordlen(const struct hcapi_cfa_layout *layout);

/**
 * The API provides the functionality to get bit offset and bit
 * length information of a field from a programming layout.
 *
 * @layout:	A pointer of the action layout
 * @slice:	A pointer to the action offset info data structure
 *
 * @return:
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_get_slice(const struct hcapi_cfa_layout *layout,
			u16 field_id, struct hcapi_cfa_field *slice);

/**
 * This API provides the functionality to read the value of a
 * CFA HW field from CFA HW data object based on the hardware
 * programming layout.
 *
 * @obj_data:	A pointer to a CFA HW key/mask object data
 * @layout:	A pointer to CFA HW programming layout
 * @field_id:	ID of the HW field to be programmed
 * @val:	Value of the HW field
 *
 * @return:
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_get_field(u64 *obj_data,
			const struct hcapi_cfa_layout *layout,
			u16 field_id, u64 *val);

/**
 * This API provides the functionality to read 128-bit value of
 * a CFA HW field from CFA HW data object based on the hardware
 * programming layout.
 *
 * @obj_data:	A pointer to a CFA HW key/mask object data
 * @layout:	A pointer to CFA HW programming layout
 * @field_id:	ID of the HW field to be programmed
 * @val_msb:	Msb value of the HW field
 * @val_lsb:	Lsb value of the HW field
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_get128_field(u64 *obj_data,
			   const struct hcapi_cfa_layout *layout,
			   u16 field_id, u64 *val_msb,
			   u64 *val_lsb);

/**
 * This API provides the functionality to read a number of
 * HW fields from a CFA HW data object based on the hardware
 * programming layout.
 *
 * @obj_data:	A pointer to a CFA profiler key/mask object data
 * @layout:	A pointer to CFA HW programming layout
 * @field_tbl:	A pointer to an array that consists of the object field
 *		ID/value pairs
 * @field_tbl_sz:	Number of entries in the table
 *
 * @return:
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_get_fields(u64 *obj_data,
			 const struct hcapi_cfa_layout *layout,
			 struct hcapi_cfa_data_obj *field_tbl,
			 u16 field_tbl_sz);

/**
 * This API provides the functionality to read a number of
 * HW fields from a CFA HW data object based on the hardware
 * programming layout.This API will swap the n byte blocks before
 * retrieving the field array.
 *
 * @obj_data:	A pointer to a CFA profiler key/mask object data
 * @layout:	A pointer to CFA HW programming layout
 * @field_tbl:	A pointer to an array that consists of the object field
 *		ID/value pairs
 * @field_tbl_sz:	Number of entries in the table
 * @data_size:	size of the data in bytes
 * @n:		block size in bytes
 *
 * @return:
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_get_fields_swap(u64 *obj_data,
			      const struct hcapi_cfa_layout *layout,
			      struct hcapi_cfa_data_obj *field_tbl,
			      u16 field_tbl_sz, u16 data_size,
			      u16 n);

/**
 * Get a value to a specific location relative to a HW field
 *
 * This API provides the functionality to read HW field from
 * a section of a HW data object identified by the bit position
 * and bit length from a given programming layout in order to avoid
 * reading the entire HW data object.
 *
 * @obj_data:	A pointer of the data object to read from
 * @layout:	A pointer of the programming layout
 * @field_id:	Identifier of the HW field
 * @bitpos_adj:	Bit position adjustment value
 * @bitlen_adj:	Bit length adjustment value
 * @val:	Value of the HW field
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_get_field_rel(u64 *obj_data,
			    const struct hcapi_cfa_layout *layout,
			    u16 field_id, int16_t bitpos_adj,
			    s16 bitlen_adj, u64 *val);

/**
 * Get the length of the layout in words
 *
 * @layout:	A pointer to the layout to determine the number of words
 *		required
 *
 * @return
 *   number of words needed for the given layout
 */
u16 cfa_hw_get_wordlen(const struct hcapi_cfa_layout *layout);

/**
 * This function is used to initialize a layout_contents structure
 *
 * The struct hcapi_cfa_key_layout is complex as there are three
 * layers of abstraction.  Each of those layer need to be properly
 * initialized.
 *
 * @contents:	A pointer of the layout contents to initialize
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_init_key_contents(struct hcapi_cfa_key_layout_contents
				*contents);

/**
 * This function is used to validate a key template
 *
 * The struct hcapi_cfa_key_template is complex as there are three
 * layers of abstraction.  Each of those layer need to be properly
 * validated.
 *
 * @key_template:	A pointer of the key template contents to validate
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_is_valid_key_template(struct hcapi_cfa_key_template
					*key_template);

/**
 * This function is used to validate a key layout
 *
 * The struct hcapi_cfa_key_layout is complex as there are three
 * layers of abstraction.  Each of those layer need to be properly
 * validated.
 *
 * @key_layout:	A pointer of the key layout contents to validate
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_is_valid_key_layout(struct hcapi_cfa_key_layout *key_layout);

/**
 * This function is used to hash E/EM keys
 *
 * @key_data:	A pointer of the key
 * @bitlen:	Number of bits in the key
 *
 * @return
 *   CRC32 and Lookup3 hashes of the input key
 */
u64 hcapi_cfa_key_hash(u8 *key_data, u16 bitlen);

/**
 * This function is used to execute an operation
 *
 * @op:		Operation
 * @key_tbl:	Table
 * @key_obj:	Key data
 * @key_key_loc: Key location
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int hcapi_cfa_key_hw_op(struct hcapi_cfa_hwop *op,
			struct hcapi_cfa_key_tbl *key_tbl,
			struct hcapi_cfa_key_data *key_obj,
			struct hcapi_cfa_key_loc *key_loc);

u64 hcapi_get_table_page(struct hcapi_cfa_em_table *mem, u32 page);
u64 hcapi_cfa_p4_key_hash(u8 *key_data, u16 bitlen);
u64 hcapi_cfa_p58_key_hash(u8 *key_data, u16 bitlen);
#endif /* HCAPI_CFA_DEFS_H_ */
