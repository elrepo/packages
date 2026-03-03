/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_BLD_DEFS_H_
#define _CFA_BLD_DEFS_H_

#include "cfa_resources.h"
#include "cfa_types.h"

/**
 *  @addtogroup CFA_BLD CFA Builder Library
 *  \ingroup CFA_V3
 *  The CFA builder library is a set of APIs provided the following services:
 *
 *  1. Provide users generic put service to convert software programming data
 *     into a hardware data bit stream according to a HW layout representation,
 *     or generic get service to extract value of a field or values of a number
 *     of fields from the raw hardware data bit stream according to a HW layout.
 *
 *     - A software programming data is represented in {field_idx, val}
 *        structure.
 *     - A HW layout is represented with array of CFA field structures with
 *        {bitpos, bitlen} and identified by a layout id corresponding to a CFA
 *        HW table.
 *     - A HW data bit stream are bits that is formatted according to a HW
 *        layout representation.
 *
 *  2. Provide EM/WC key and action related service APIs to compile layout,
 *     init, and manipulate key and action data objects.
 *
 *  3. Provide CFA mid-path message building APIs. (TBD)
 *
 *  The CFA builder library is designed to run in the primate firmware and also
 *  as part of the following host base diagnostic software.
 *  - Lcdiag
 *  - Truflow CLI
 *  - coredump decorder
 *
 *  @{
 */

/** @name CFA Builder Common Definition
 * CFA builder common structures and enumerations
 */

/**@{*/
/**
 * CFA HW KEY CONTROL OPCODE definition
 */
enum cfa_key_ctrlops {
	CFA_KEY_CTRLOPS_INSERT, /**< insert control bits */
	CFA_KEY_CTRLOPS_STRIP,  /**< strip control bits */
	CFA_KEY_CTRLOPS_MAX
};

/**
 * CFA HW field structure definition
 */
struct cfa_field {
	/** [in] Starting bit position pf the HW field within a HW table
	 *  entry.
	 */
	u16 bitpos;
	/** [in] Number of bits for the HW field. */
	u16 bitlen;
};

/**
 * CFA HW table entry layout structure definition
 */
struct cfa_layout {
	/** [out] Bit order of layout
	 * if swap_order_bitpos is non-zero, the bit order of the layout
	 * will be swapped after this bit.  swap_order_bitpos must be a
	 * multiple of 64.  This is currently only used for inlined action
	 * records where the AR is lsb and the following inlined actions
	 * must be msb.
	 */
	bool is_msb_order;
	/** [out] Reverse is_msb_order after this bit if non-zero */
	u16 swap_order_bitpos;
	/** [out] Size in bits of entry */
	u32 total_sz_in_bits;
	/** [in/out] data pointer of the HW layout fields array */
	struct cfa_field *field_array;
	/** [out] number of HW field entries in the HW layout field array */
	u32 array_sz;
	/** [out] layout_id - layout id associated with the layout */
	u16 layout_id;
};

/**
 * CFA HW data object definition
 */
struct cfa_data_obj {
	/** [in] HW field identifier. Used as an index to a HW table layout */
	u16 field_id;
	/** [in] Value of the HW field */
	u64 val;
};

/**@}*/

/** @name CFA Builder PUT_FIELD APIs
 *  CFA Manager apis used for generating hw layout specific data objects that
 *  can be programmed to the hardware
 */

/**@{*/
/**
 * @brief This API provides the functionality to program a specified value to a
 * HW field based on the provided programming layout.
 *
 * @param[in,out] data_buf
 *   A data pointer to a CFA HW key/mask data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in] field_id
 *   ID of the HW field to be programmed
 *
 * @param[in] val
 *   Value of the HW field to be programmed
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int cfa_put_field(u64 *data_buf, const struct cfa_layout *layout,
		  u16 field_id, u64 val);

/**
 * @brief This API provides the functionality to program an array of field
 * values with corresponding field IDs to a number of profiler sub-block fields
 * based on the fixed profiler sub-block hardware programming layout.
 *
 * @param[in, out] obj_data
 *   A pointer to a CFA profiler key/mask object data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in] field_tbl
 *   A pointer to an array that consists of the object field
 *   ID/value pairs
 *
 * @param[in] field_tbl_sz
 *   Number of entries in the table
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int cfa_put_fields(u64 *obj_data, const struct cfa_layout *layout,
		   struct cfa_data_obj *field_tbl, u16 field_tbl_sz);

/**
 * @brief This API provides the functionality to program an array of field
 * values with corresponding field IDs to a number of profiler sub-block fields
 * based on the fixed profiler sub-block hardware programming layout. This
 * API will swap the n byte blocks before programming the field array.
 *
 * @param[in, out] obj_data
 *   A pointer to a CFA profiler key/mask object data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in] field_tbl
 *   A pointer to an array that consists of the object field
 *   ID/value pairs
 *
 * @param[in] field_tbl_sz
 *   Number of entries in the table
 *
 * @param[in] data_size
 *   size of the data in bytes
 *
 * @param[in] n
 *   block size in bytes
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int cfa_put_fields_swap(u64 *obj_data, const struct cfa_layout *layout,
			struct cfa_data_obj *field_tbl, u16 field_tbl_sz,
			u16 data_size, u16 n);

/**
 * @brief This API provides the functionality to write a value to a
 * field within the bit position and bit length of a HW data
 * object based on a provided programming layout.
 *
 * @param[in, out] obj_data
 *   A pointer of the action object to be initialized
 *
 * @param[in] layout
 *   A pointer of the programming layout
 *
 * @param field_id
 *   [in] Identifier of the HW field
 *
 * @param[in] bitpos_adj
 *   Bit position adjustment value
 *
 * @param[in] bitlen_adj
 *   Bit length adjustment value
 *
 * @param[in] val
 *   HW field value to be programmed
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int cfa_put_field_rel(u64 *obj_data, const struct cfa_layout *layout,
		      u16 field_id, int16_t bitpos_adj, int16_t bitlen_adj,
		      u64 val);

/**@}*/

/** @name CFA Builder GET_FIELD APIs
 *  CFA Manager apis used for extract hw layout specific fields from CFA HW
 *  data objects
 */

/**@{*/
/**
 * @brief The API provides the functionality to get bit offset and bit
 * length information of a field from a programming layout.
 *
 * @param[in] layout
 *   A pointer of the action layout
 *
 * @param[in] field_id
 *   The field for which to retrieve the slice
 *
 * @param[out] slice
 *   A pointer to the action offset info data structure
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int cfa_get_slice(const struct cfa_layout *layout, u16 field_id,
		  struct cfa_field *slice);

/**
 * @brief This API provides the functionality to read the value of a
 * CFA HW field from CFA HW data object based on the hardware
 * programming layout.
 *
 * @param[in] obj_data
 *   A pointer to a CFA HW key/mask object data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in] field_id
 *   ID of the HW field to be programmed
 *
 * @param[out] val
 *   Value of the HW field
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int cfa_get_field(u64 *obj_data, const struct cfa_layout *layout,
		  u16 field_id, u64 *val);

/**
 * @brief This API provides the functionality to read 128-bit value of
 * a CFA HW field from CFA HW data object based on the hardware
 * programming layout.
 *
 * @param[in] obj_data
 *   A pointer to a CFA HW key/mask object data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in] field_id
 *   ID of the HW field to be programmed
 *
 * @param[out] val_msb
 *   Msb value of the HW field
 *
 * @param[out] val_lsb
 *   Lsb value of the HW field
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int cfa_get128_field(u64 *obj_data, const struct cfa_layout *layout,
		     u16 field_id, u64 *val_msb, u64 *val_lsb);

/**
 * @brief This API provides the functionality to read a number of
 * HW fields from a CFA HW data object based on the hardware
 * programming layout.
 *
 * @param[in] obj_data
 *   A pointer to a CFA profiler key/mask object data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in, out] field_tbl
 *   A pointer to an array that consists of the object field
 *   ID/value pairs
 *
 * @param[in] field_tbl_sz
 *   Number of entries in the table
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int cfa_get_fields(u64 *obj_data, const struct cfa_layout *layout,
		   struct cfa_data_obj *field_tbl, u16 field_tbl_sz);

/**
 * @brief This API provides the functionality to read a number of
 * HW fields from a CFA HW data object based on the hardware
 * programming layout.This API will swap the n byte blocks before
 * retrieving the field array.
 *
 * @param[in] obj_data
 *   A pointer to a CFA profiler key/mask object data
 *
 * @param[in] layout
 *   A pointer to CFA HW programming layout
 *
 * @param[in, out] field_tbl
 *   A pointer to an array that consists of the object field
 *   ID/value pairs
 *
 * @param[in] field_tbl_sz
 *   Number of entries in the table
 *
 * @param[in] data_size
 *   size of the data in bytes
 *
 * @param[in] n
 *   block size in bytes
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int cfa_get_fields_swap(u64 *obj_data, const struct cfa_layout *layout,
			struct cfa_data_obj *field_tbl, u16 field_tbl_sz,
			u16 data_size, u16 n);

/**
 * @brief Get a value to a specific location relative to a HW field
 * This API provides the functionality to read HW field from
 * a section of a HW data object identified by the bit position
 * and bit length from a given programming layout in order to avoid
 * reading the entire HW data object.
 *
 * @param[in] obj_data
 *   A pointer of the data object to read from
 *
 * @param[in] layout
 *   A pointer of the programming layout
 *
 * @param[in] field_id
 *   Identifier of the HW field
 *
 * @param[in] bitpos_adj
 *   Bit position adjustment value
 *
 * @param[in] bitlen_adj
 *   Bit length adjustment value
 *
 * @param[out] val
 *   Value of the HW field
 *
 * @return
 *   0 for SUCCESS, negative value for FAILURE
 */
int cfa_get_field_rel(u64 *obj_data, const struct cfa_layout *layout,
		      u16 field_id, int16_t bitpos_adj, int16_t bitlen_adj,
		      u64 *val);

/**
 * @brief Get the length of the layout in words
 *
 * @param[in] layout
 *   A pointer to the layout to determine the number of words
 *   required
 *
 * @return
 *   number of words needed for the given layout
 */
u16 cfa_get_wordlen(const struct cfa_layout *layout);

/**@}*/

/**@}*/
#endif /* _CFA_BLD_DEFS_H_*/
