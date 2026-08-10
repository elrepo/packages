/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_MSG_H_
#define _TF_MSG_H_

#include <linux/types.h>
#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "tf_tbl.h"
#include "tf_rm.h"
#include "tf_tcam.h"
#include "tf_global_cfg.h"
#include "tf_em.h"

struct tf;

/* HWRM Direct messages */

/**
 * tf_msg_session_open: Sends session open request to Firmware
 *
 * @bp: Pointer to bnxt handle
 * @ctrl_chan_name: PCI name of the control channel
 * @fw_session_id: Pointer to the fw_session_id that is allocated
 *                 on firmware side (output)
 * @fw_session_client_id: Pointer to the fw_session_client_id that
 *                        is allocated on firmware side (output)
 * @shared_session_creator: Pointer to the shared_session_creator
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_open(struct bnxt *bp, char *ctrl_chan_name,
			u8 *fw_session_id, u8 *fw_session_client_id,
			bool *shared_session_creator);

/**
 * tf_msg_session_client_register: Sends session client register request
 *                                 to Firmware
 *
 * @session: Pointer to session handle
 * @ctrl_chan_name: PCI name of the control channel
 * @fw_session_id: FW session id
 * @fw_session_client_id: Pointer to the fw_session_client_id that
 *                        is allocated on firmware side (output)
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_client_register(struct tf *tfp, char *ctrl_channel_name,
				   u8 fw_session_id, u8 *fw_session_client_id);

/**
 * tf_msg_session_client_unregister: Sends session client unregister
 *                                   request to Firmware
 *
 * @fw_session_id: FW session id
 * @fw_session_client_id: Pointer to the fw_session_client_id that
 *                        is allocated on firmware side
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_client_unregister(struct tf *tfp, u8 fw_session_id,
				     u8 fw_session_client_id);

/**
 * tf_msg_session_close: Sends session close request to Firmware
 * @session: Pointer to session handle
 * @fw_session_id: fw session id
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_close(struct tf *tfp, u8 fw_session_id);

/**
 * tf_msg_session_qcfg: Sends session query config request to TF Firmware
 * @session: Pointer to session handle
 * @fw_session_id: fw session id
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_qcfg(struct tf *tfp, u8 fw_session_id);

/**
 * tf_msg_session_resc_qcaps: Sends session HW resource query
 *                            capability request to TF Firmware
 *
 * @tfp: Pointer to TF handle
 * @dir: Receive or Transmit direction
 * @size: Number of elements in the query. Should be set to the max
 *        elements for the device type
 * @query: Pointer to an array of query elements (output)
 * @resv_strategy: Pointer to the reservation strategy (output)
 * @sram_profile:  Pointer to the sram profile
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_resc_qcaps(struct tf *tfp, enum tf_dir dir, u16 size,
			      struct tf_rm_resc_req_entry *query,
			      enum tf_rm_resc_resv_strategy *resv_strategy,
			      u8 *sram_profile);

/**
 * tf_msg_session_resc_alloc: Sends session HW resource allocation
 *                            request to TF Firmware
 *
 * @tfp: Pointer to TF handle
 * @dir: Receive or Transmit direction
 * @size: Number of elements in the req and resv arrays
 * @req: Pointer to an array of request elements
 * @fw_session_id: fw session id
 * @resv: Pointer to an array of reserved elements
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_resc_alloc(struct tf *tfp, enum tf_dir dir, u16 size,
			      struct tf_rm_resc_req_entry *request,
			      u8 fw_session_id, struct tf_rm_resc_entry *resv);

/**
 * tf_msg_session_resc_info: Sends session HW resource allocation
 *                           request to TF Firmware
 * @tfp: Pointer to TF handle
 * @dir: Receive or Transmit direction
 * @size: Number of elements in the req and resv arrays
 * @req: Pointer to an array of request elements
 * @fw_session_id: fw session id
 * @resv: Pointer to an array of reserved elements
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_resc_info(struct tf *tfp, enum tf_dir dir, u16 size,
			     struct tf_rm_resc_req_entry *request,
			     u8 fw_session_id, struct tf_rm_resc_entry *resv);

/**
 * tf_msg_session_resc_flush: Sends session resource flush request
 *                            to TF Firmware
 * @tfp: Pointer to TF handle
 * @dir: Receive or Transmit direction
 * @size: Number of elements in the req and resv arrays
 * @fw_session_id: fw session id
 * @resv: Pointer to an array of reserved elements that needs to be flushed
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_session_resc_flush(struct tf *tfp, enum tf_dir dir, u16 size,
			      u8 fw_session_id, struct tf_rm_resc_entry *resv);

/**
 * Sends EM internal insert request to Firmware
 *
 * @tfp:		Pointer to TF handle
 * @params:		Pointer to em insert parameter list
 * @fw_session_id:	fw session id
 * @rptr_index:		Record ptr index
 * @rptr_entry:		Record ptr entry
 * @num_of_entries:	Number of entries to insert
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_insert_em_internal_entry(struct tf *tfp,
				    struct tf_insert_em_entry_parms *em_parms,
				    u8 fw_session_id, u16 *rptr_index,
				    u8 *rptr_entry, u8 *num_of_entries);

/**
 * Sends EM hash internal insert request to Firmware
 *
 * @tfp:		Pointer to TF handle
 * @params:		Pointer to em insert parameter list
 * @key0_hash:		CRC32 hash of key
 * @key1_hash:		Lookup3 hash of key
 * @fw_session_id:	fw session id
 * @rptr_index:		Record ptr index
 * @rptr_entry:		Record ptr entry
 * @num_of_entries:	Number of entries to insert
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_hash_insert_em_internal_entry(struct tf *tfp,
					 struct tf_insert_em_entry_parms
					 *em_parms, u32 key0_hash,
					 u32 key1_hash, u8 fw_session_id,
					 u16 *rptr_index, u8 *rptr_entry,
					 u8 *num_of_entries);

/**
 * Sends EM internal delete request to Firmware
 *
 * @tfp:	Pointer to TF handle
 * @em_parms:	Pointer to em delete parameters
 * @fw_session_id:	fw session id
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_delete_em_entry(struct tf *tfp,
			   struct tf_delete_em_entry_parms *em_parms,
			   u8 fw_session_id);

/**
 * Sends EM internal move request to Firmware
 *
 * @tfp:	Pointer to TF handle
 * @em_parms:	Pointer to em move parameters
 * @fw_session_id:	fw session id
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_move_em_entry(struct tf *tfp,
			 struct tf_move_em_entry_parms *em_parms,
			 u8 fw_session_id);

/**
 * Sends tcam entry 'set' to the Firmware.
 *
 * @tfp:	Pointer to session handle
 * @parms:	Pointer to set parameters
 * @fw_session_id:	fw session id
 *
 * Returns:
 *  0 on Success else internal Truflow error
 */
int tf_msg_tcam_entry_set(struct tf *tfp, struct tf_tcam_set_parms *parms,
			  u8 fw_session_id);

/**
 * Sends tcam entry 'get' to the Firmware.
 *
 * @tfp:	Pointer to session handle
 * @parms:	Pointer to get parameters
 * @fw_session_id:	fw session id
 *
 * Returns:
 *  0 on Success else internal Truflow error
 */
int tf_msg_tcam_entry_get(struct tf *tfp, struct tf_tcam_get_parms *parms,
			  u8 fw_session_id);

/**
 * Sends tcam entry 'free' to the Firmware.
 *
 * @tfp:	Pointer to session handle
 * @parms:	Pointer to free parameters
 * @fw_session_id:	fw session id
 *
 * Returns:
 *  0 on Success else internal Truflow error
 */
int tf_msg_tcam_entry_free(struct tf *tfp, struct tf_tcam_free_parms *in_parms,
			   u8 fw_session_id);

/**
 * Sends Set message of a Table Type element to the firmware.
 *
 * @tfp:	Pointer to session handle
 * @dir:	Direction location of the element to set
 * @hcapi_type:	Type of the object to set
 * @size:	Size of the data to set
 * @data:	Data to set
 * @index:	Index to set
 * @fw_session_id:	fw session id
 *
 * Returns:
 *   0 - Success
 */
int tf_msg_set_tbl_entry(struct tf *tfp, enum tf_dir dir, u16 hcapi_type,
			 u16 size, u8 *data, u32 index, u8 fw_session_id);

/**
 * Sends get message of a Table Type element to the firmware.
 *
 * @tfp:	Pointer to session handle
 * @dir:	Direction location of the element to get
 * @hcapi_type:	Type of the object to get
 * @size:	Size of the data read
 * @data:	Data read
 * @index:	Index to get
 * @fw_session_id:	fw session id
 *
 * Returns:
 *   0 - Success
 */
int tf_msg_get_tbl_entry(struct tf *tfp, enum tf_dir dir, u16 hcapi_type,
			 u16 size, u8 *data, u32 index, bool clear_on_read,
			 u8 fw_session_id);

/* HWRM Tunneled messages */

/**
 * Sends global cfg read request to Firmware
 *
 * @tfp:	Pointer to TF handle
 * @params:	Pointer to read parameters
 * @fw_session_id:	fw session id
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_get_global_cfg(struct tf *tfp, struct tf_global_cfg_parms *params,
			  u8 fw_session_id);

/**
 * Sends global cfg update request to Firmware
 *
 * @tfp:	Pointer to TF handle
 * @params:	Pointer to write parameters
 * @fw_session_id:	fw session id
 *
 * Returns:
 *   0 on Success else internal Truflow error
 */
int tf_msg_set_global_cfg(struct tf *tfp, struct tf_global_cfg_parms *params,
			  u8 fw_session_id);

/**
 * Sends bulk get message of a Table Type element to the firmware.
 *
 * @tfp:	Pointer to session handle
 * @parms:	Pointer to table get bulk parameters
 *
 * Returns:
 *  0 on Success else internal Truflow error
 */
int tf_msg_bulk_get_tbl_entry(struct tf *tfp, enum tf_dir dir, u16 hcapi_type,
			      u32 starting_idx, u16 num_entries,
			      u16 entry_sz_in_bytes, u64 physical_mem_addr,
			      bool clear_on_read);

/**
 * Sends Set message of a IF Table Type element to the firmware.
 *
 * @tfp:	Pointer to session handle
 * @parms:	Pointer to IF table set parameters
 * @fw_session_id:	fw session id
 *
 * Returns:
 *  0 on Success else internal Truflow error
 */
int tf_msg_set_if_tbl_entry(struct tf *tfp,
			    struct tf_if_tbl_set_parms *params,
			    u8 fw_session_id);

/**
 * Sends get message of a IF Table Type element to the firmware.
 *
 * @tfp:	Pointer to session handle
 * @parms:	Pointer to IF table get parameters
 * @fw_session_id:	fw session id
 *
 * Returns:
 *  0 on Success else internal Truflow error
 */
int tf_msg_get_if_tbl_entry(struct tf *tfp,
			    struct tf_if_tbl_get_parms *params,
			    u8 fw_session_id);

/**
 * Send get version request to the firmware.
 *
 * @bp:		Pointer to bnxt handle
 * @dev:	Pointer to the associated device
 * @parms:	Pointer to the version info parameter
 *
 * Returns:
 *  0 on Success else internal Truflow error
 */
int tf_msg_get_version(struct bnxt *bp, struct tf_dev_info *dev,
		       struct tf_get_version_parms *parms);

#endif  /* _TF_MSG_H_ */
