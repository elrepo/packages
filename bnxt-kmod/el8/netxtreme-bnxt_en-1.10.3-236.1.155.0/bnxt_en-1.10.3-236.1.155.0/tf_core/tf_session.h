/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_SESSION_H_
#define _TF_SESSION_H_

#include <linux/types.h>
#include "bitalloc.h"
#include "tf_core.h"
#include "tf_device.h"
#include "tf_rm.h"

/* The Session module provides session control support. A session is
 * to the ULP layer known as a session_info instance. The session
 * private data is the actual session.
 *
 * Session manages:
 *   - The device and all the resources related to the device.
 *   - Any session sharing between ULP applications
 */

/* Session defines */
#define TF_SESSION_ID_INVALID     0xFFFFFFFF /** Invalid Session ID define */

/* At this stage we are using fixed size entries so that each
 * stack entry represents either 2 or 4 RT (f/n)blocks. So we
 * take the total block allocation for truflow and divide that
 * by either 2 or 4.
 */
#ifdef TF_EM_ENTRY_IPV4_ONLY
#define TF_SESSION_EM_ENTRY_SIZE 2 /* 2 blocks per entry */
#else
#define TF_SESSION_EM_ENTRY_SIZE 4 /* 4 blocks per entry */
#endif

/**
 * TF Session
 *
 * @ver:		TruFlow Version. Used to control the structure
 *			layout when sharing sessions. No guarantee that a
 *			secondary process would come from the same version
 *			of an executable.
 * @session_id:		Session ID, allocated by FW on tf_open_session()
 * @shared_session:	Boolean controlling the use and availability of
 *			shared session. Shared session will allow the
 *			application to share resources on the firmware side
 *			without having to allocate them on firmware.
 *			Additional private session core_data will be
 *			allocated if this boolean is set to 'true', default
 *			'false'.
 * @shared_session_creator: This flag indicates the shared session on
 *			firmware side is created by this session. Some
 *			privileges may be assigned to this session.
 * @shadow_copy:	Boolean controlling the use and availability of
 *			shadow copy. Shadow copy will allow the TruFlow
 *			Core to keep track of resource content on the
 *			firmware side without having to query firmware.
 *			Additional private session core_data will be
 *			allocated if this boolean is set to 'true',
 *			default 'false'. Size of memory depends on the
 *			NVM Resource settings for the control channel.
 * @ref_cnt:		Session Reference Count. To keep track of functions
 *			per session the ref_count is updated. There is also a
 *			parallel TruFlow Firmware ref_count in case the
 *			TruFlow	Core goes away without informing the Firmware.
 * @ref_count_attach:	Session Reference Count for attached sessions.
 *			To keep track of application sharing of a session
 *			the ref_count_attach is updated.
 * @dev:		Device handle.
 * @dev_init:		Device init flag. False if Device is not fully
 *			initialized, else true.
 * @client_ll:		Linked list of clients registered for this session
 * @em_ext_db_handle:	em ext db reference for the session
 * @tcam_db_handle:	tcam db reference for the session
 * @tbl_db_handle:	table db reference for the session
 * @id_db_handle:	identifier db reference for the session
 * @em_db_handle:	em db reference for the session
 * @em_pool:		EM allocator for session
 * #ifdef TF_TCAM_SHARED
 * @tcam_shared_db_handle:	tcam db reference for the session
 * #endif
 * @sram_handle:	SRAM db reference for the session
 * @if_tbl_db_handle:	if table db reference for the session
 * @global_db_handle:	global db reference for the session
 * @wc_num_slices_per_row:	Number of slices per row for WC TCAM
 * @tcam_mgr_control:	Indicates if TCAM is controlled by TCAM Manager
 *
 * Shared memory containing private TruFlow session information.
 * Through this structure the session can keep track of resource
 * allocations and (if so configured) any shadow copy of flow
 * information. It also holds info about Session Clients.
 *
 * Memory is assigned to the Truflow instance by way of
 * tf_open_session. Memory is allocated and owned by i.e. ULP.
 *
 * Access control to this shared memory is handled by the spin_lock in
 * tf_session_info.
 */
struct tf_session {
	struct tf_session_version	ver;
	union tf_session_id		session_id;
	bool				shared_session;
	bool				shared_session_creator;
	u8				ref_count;
	u8				ref_count_attach;
	struct tf_dev_info		dev;
	bool				dev_init;
	struct list_head		client_ll;
	void				*em_ext_db_handle;
	void				*tcam_db_handle;
	void				*tbl_db_handle;
	void				*id_db_handle;
	void				*em_db_handle;
	void				*em_pool[TF_DIR_MAX];
	void				*sram_handle;
	void				*if_tbl_db_handle;
	void				*global_db_handle;
	u16				wc_num_slices_per_row;
	int		tcam_mgr_control[TF_DIR_MAX][TF_TCAM_TBL_TYPE_MAX];
	void				*tcam_mgr_handle;
};

/**
 * Session Client
 *
 * @ll_entry:		Linked list of clients. For inserting in link list,
 *			must be first field of struct.
 * @ctrl_chan_name:	String containing name of control channel interface
 *			to be used for this session to communicate with
 *			firmware. ctrl_chan_name will be used as part of a
 *			name for any shared memory allocation.
 * @fw_fid:		Firmware FID, learned at time of Session Client create.
 * @session_client_id:	Session Client ID, allocated by FW on
 *			tf_register_session()
 *
 * Shared memory for each of the Session Clients. A session can have
 * one or more clients.
 */
struct tf_session_client {
	struct list_head		ll_entry;
	char				ctrl_chan_name[TF_SESSION_NAME_MAX];
	u16				fw_fid;
	union tf_session_client_id	session_client_id;
};

/**
 * Session open parameter definition
 * @open_cfg: Pointer to the TF open session configuration
 */
struct tf_session_open_session_parms {
	struct tf_open_session_parms *open_cfg;
};

/**
 * Session attach parameter definition
 * @attach_cfg:	Pointer to the TF attach session configuration
 */
struct tf_session_attach_session_parms {
	struct tf_attach_session_parms *attach_cfg;
};

/* Session close parameter definition */
struct tf_session_close_session_parms {
	u8			*ref_count;
	union tf_session_id	*session_id;
};

/**
 * Creates a host session with a corresponding firmware session.
 *
 * @tfp:	Pointer to TF handle
 * @parms:	Pointer to the session open parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_open_session(struct tf *tfp,
			    struct tf_session_open_session_parms *parms);

/**
 * Attaches a previous created session.
 *
 * @tfp:	Pointer to TF handle
 * @parms:	Pointer to the session attach parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_attach_session(struct tf *tfp,
			      struct tf_session_attach_session_parms *parms);

/**
 * Closes a previous created session. Only possible if previous
 * registered Clients had been unregistered first.
 *
 * @tfp:	Pointer to TF handle
 * @parms:	Pointer to the session close parameters.
 *
 * Returns
 *   - (0) if successful.
 *   - (-EUSERS) if clients are still registered with the session.
 *   - (-EINVAL) on failure.
 */
int tf_session_close_session(struct tf *tfp,
			     struct tf_session_close_session_parms *parms);

/**
 * Verifies that the fid is supported by the session. Used to assure
 * that a function i.e. client/control channel is registered with the
 * session.
 *
 * @tfs:	Pointer to TF Session handle
 * @fid:	FID value to check
 *
 * Returns
 *   - (true) if successful, else false
 *   - (-EINVAL) on failure.
 */
bool tf_session_is_fid_supported(struct tf_session *tfs, u16 fid);

/**
 * Looks up the private session information from the TF session
 * info. Does not perform a fid check against the registered
 * clients. Should be used if tf_session_get_session() was used
 * previously i.e. at the TF API boundary.
 *
 * @tfp:	Pointer to TF handle
 * @tfs:	Pointer to the session
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_session_internal(struct tf *tfp, struct tf_session **tfs);

/**
 * Looks up the private session information from the TF session
 * info. Performs a fid check against the clients on the session.
 *
 * @tfp:	Pointer to TF handle
 * @tfs:	Pointer to the session
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_session(struct tf *tfp, struct tf_session **tfs);

/**
 * Looks up client within the session.
 *
 * @tfs:		Pointer to the session
 * @session_client_id:	Client id to look for within the session
 *
 * Returns
 *   client if successful.
 *   - (NULL) on failure, client not found.
 */
struct tf_session_client *
tf_session_get_session_client(struct tf_session *tfs,
			      union tf_session_client_id session_client_id);

/**
 * Looks up client using name within the session.
 *
 * @tfs:		pointer to the session
 * @ctrl_chan_name:	name of the client to lookup in the session
 *
 * Returns:
 *   - Pointer to the session, if found.
 *   - (NULL) on failure, client not found.
 */
struct tf_session_client *
tf_session_find_session_client_by_name(struct tf_session *tfs,
				       const char *ctrl_chan_name);

/**
 * Looks up client using the fid.
 *
 * @session:	pointer to the session
 * @fid:	fid of the client to find
 *
 * Returns:
 *   - Pointer to the session, if found.
 *   - (NULL) on failure, client not found.
 */
struct tf_session_client *
tf_session_find_session_client_by_fid(struct tf_session *tfs, u16 fid);

/**
 * Looks up the device information from the TF Session.
 *
 * @tfp:	Pointer to TF handle
 * @tfd:	Pointer to the device [out]
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_device(struct tf_session *tfs, struct tf_dev_info **tfd);

/**
 * Returns the session and the device from the tfp.
 *
 * @tfp:	Pointer to TF handle
 * @tfs:	Pointer to the session
 * @tfd:	Pointer to the device

 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get(struct tf *tfp, struct tf_session **tfs,
		   struct tf_dev_info **tfd);

/**
 * Looks up the FW Session id the requested TF handle.
 *
 * @tfp:	Pointer to TF handle
 * @session_id:	Pointer to the session_id [out]
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_fw_session_id(struct tf *tfp, u8 *fw_session_id);

/**
 * Looks up the Session id the requested TF handle.
 *
 * @tfp:	Pointer to TF handle
 * @session_id:	Pointer to the session_id [out]
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_session_id(struct tf *tfp, union tf_session_id *session_id);

/**
 * API to get the em_ext_db from tf_session.
 *
 * @tfp:		Pointer to TF handle
 * @em_ext_db_handle:	pointer to eem handle
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_em_ext_db(struct tf *tfp, void **em_ext_db_handle);

/**
 * API to set the em_ext_db in tf_session.
 *
 * @tfp:		Pointer to TF handle
 * @em_ext_db_handle:	pointer to eem handle
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_set_em_ext_db(struct tf *tfp, void *em_ext_db_handle);

/**
 * API to get the db from tf_session.
 *
 * @tfp:	Pointer to TF handle
 * @db_handle:	pointer to db handle
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_db(struct tf *tfp, enum tf_module_type type,
		      void **db_handle);

/**
 * API to set the db in tf_session.
 *
 * @tfp:	Pointer to TF handle
 * @db_handle:	pointer to db handle
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_set_db(struct tf *tfp, enum tf_module_type type,
		      void *db_handle);

/**
 * Check if the session is shared session.
 *
 * @session: pointer to the session
 *
 * Returns:
 *   - true if it is shared session
 *   - false if it is not shared session
 */
static inline bool tf_session_is_shared_session(struct tf_session *tfs)
{
	return tfs->shared_session;
}

/**
 * Check if the session is the shared session creator
 *
 * @session: pointer to the session
 *
 * Returns:
 *   - true if it is the shared session creator
 *   - false if it is not the shared session creator
 */
static inline bool tf_session_is_shared_session_creator(struct tf_session *tfs)
{
	return tfs->shared_session_creator;
}

/**
 * Get the pointer to the parent bnxt struct
 *
 * @session: pointer to the session
 *
 * Returns:
 *   - the pointer to the parent bnxt struct
 */
static inline struct bnxt*
tf_session_get_bp(struct tf *tfp)
{
	return tfp->bp;
}

/**
 * Set the pointer to the SRAM database
 *
 * @session:	pointer to the session
 *
 * Returns:
 *   - the pointer to the parent bnxt struct
 */
int tf_session_set_sram_db(struct tf *tfp, void *sram_handle);

/**
 * Get the pointer to the SRAM database
 *
 * @session:	pointer to the session
 *
 * Returns:
 *   - the pointer to the parent bnxt struct
 */
int tf_session_get_sram_db(struct tf *tfp, void **sram_handle);

/**
 * Set the pointer to the global cfg database
 *
 * @session: pointer to the session
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_set_global_db(struct tf *tfp, void *global_handle);

/**
 * Get the pointer to the global cfg database
 *
 * @session: pointer to the session
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_global_db(struct tf *tfp, void **global_handle);

/**
 * Set the pointer to the if table cfg database
 *
 * @session: pointer to the session
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_set_if_tbl_db(struct tf *tfp, void *if_tbl_handle);

/**
 * Get the pointer to the if table cfg database
 *
 * @session: pointer to the session
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_if_tbl_db(struct tf *tfp, void **if_tbl_handle);

#endif /* _TF_SESSION_H_ */
