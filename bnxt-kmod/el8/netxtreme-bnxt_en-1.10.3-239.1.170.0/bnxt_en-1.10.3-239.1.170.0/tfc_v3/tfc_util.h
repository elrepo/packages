/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_UTIL_H_
#define _TFC_UTIL_H_

#include "cfa_types.h"
#include "cfa_resources.h"

/**
 * Helper function converting direction to text string
 *
 * @dir: Receive or transmit direction identifier
 *
 * Returns:
 *   Pointer to a char string holding the string for the direction
 */
const char *tfc_dir_2_str(enum cfa_dir dir);

/**
 * Helper function converting identifier subtype to text string
 *
 * @id_stype: Identifier subtype
 *
 * Returns:
 *   Pointer to a char string holding the string for the identifier
 */
const char *tfc_ident_2_str(enum cfa_resource_subtype_ident id_stype);

/**
 * Helper function converting tcam subtype to text string
 *
 * @tcam_stype: TCAM subtype
 *
 * Returns:
 *   Pointer to a char string holding the string for the tcam
 */
const char *tfc_tcam_2_str(enum cfa_resource_subtype_tcam tcam_stype);

/**
 * Helper function converting index tbl subtype to text string
 *
 * @idx_tbl_stype: Index table subtype
 *
 * Returns:
 *   Pointer to a char string holding the string for the table subtype
 */
const char *tfc_idx_tbl_2_str(enum cfa_resource_subtype_idx_tbl idx_tbl_stype);

/**
 * Helper function converting table scope lkup/act type and direction (region)
 * to string
 *
 * @region_type: Region type
 * @dir: Direction
 *
 * Returns:
 *   Pointer to a char string holding the string for the table subtype
 */
const char *
tfc_ts_region_2_str(enum cfa_region_type region, enum cfa_dir dir);

/**
 * Helper function converting if tbl subtype to text string
 *
 * @if_tbl_stype: If table subtype
 *
 * Returns:
 *   Pointer to a char string holding the string for the table subtype
 */
const char *tfc_if_tbl_2_str(enum cfa_resource_subtype_if_tbl if_tbl_stype);

/**
 * Helper function converting the scope type to text string
 *
 * @scope_type: table scope type
 *
 * Returns:
 *   Pointer to a char string holding the string for the scope type
 */
const char *tfc_scope_type_2_str(enum cfa_scope_type scope_type);

/**
 * Helper function retrieving field value from the buffer
 *
 * @data: buffer
 * @offset: field start bit position in the buffer
 * @blen: field length in bit
 *
 * Returns:
 *   field value
 */
u32 tfc_getbits(u32 *data, int offset, int blen);

#endif /* _TFC_UTIL_H_ */
