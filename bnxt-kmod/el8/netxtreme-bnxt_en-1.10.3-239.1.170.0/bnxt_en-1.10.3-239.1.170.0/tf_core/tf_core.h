/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_CORE_H_
#define _TF_CORE_H_

#include <linux/types.h>
#include "hcapi_cfa_defs.h"

/* Truflow Core API Header File */

/********** BEGIN Truflow Core DEFINITIONS **********/

#define TF_KILOBYTE  1024
#define TF_MEGABYTE  (1024 * 1024)

/* direction */
enum tf_dir {
	TF_DIR_RX,  /* Receive */
	TF_DIR_TX,  /* Transmit */
	TF_DIR_MAX
};

/* Memory choice */
enum tf_mem {
	TF_MEM_INTERNAL, /* Internal */
	TF_MEM_EXTERNAL, /* External */
	TF_MEM_MAX
};

/* External memory control channel type */
enum tf_ext_mem_chan_type {
	TF_EXT_MEM_CHAN_TYPE_DIRECT = 0, /* Direct memory write(Wh+/SR) */
	TF_EXT_MEM_CHAN_TYPE_RING_IF,	 /* Ring interface MPC */
	TF_EXT_MEM_CHAN_TYPE_FW,	 /* Use HWRM message to firmware */
	TF_EXT_MEM_CHAN_TYPE_RING_IF_FW, /* Use ring_if message to firmware */
	TF_EXT_MEM_CHAN_TYPE_MAX
};

/* WC TCAM number of slice per row that devices supported */
enum tf_wc_num_slice {
	TF_WC_TCAM_1_SLICE_PER_ROW = 1,
	TF_WC_TCAM_2_SLICE_PER_ROW = 2,
	TF_WC_TCAM_4_SLICE_PER_ROW = 4,
	TF_WC_TCAM_8_SLICE_PER_ROW = 8,
};

/* Bank identifier */
enum tf_sram_bank_id {
	TF_SRAM_BANK_ID_0,		/* SRAM Bank 0 id */
	TF_SRAM_BANK_ID_1,		/* SRAM Bank 1 id */
	TF_SRAM_BANK_ID_2,		/* SRAM Bank 2 id */
	TF_SRAM_BANK_ID_3,		/* SRAM Bank 3 id */
	TF_SRAM_BANK_ID_MAX		/* SRAM Bank index limit */
};

/* EEM record AR helper
 *
 * Helper to handle the Action Record Pointer in the EEM Record Entry.
 *
 * Convert absolute offset to action record pointer in EEM record entry
 * Convert action record pointer in EEM record entry to absolute offset
 */
#define TF_ACT_REC_OFFSET_2_PTR(offset) ((offset) >> 4)
#define TF_ACT_REC_PTR_2_OFFSET(offset) ((offset) << 4)

/********** BEGIN API FUNCTION PROTOTYPES/PARAMETERS **********/

/**
 * Session Version
 *
 * The version controls the format of the tf_session and
 * tf_session_info structure. This is to ensure upgrade between
 * versions can be supported.
 */
#define TF_SESSION_VER_MAJOR  1   /* Major Version */
#define TF_SESSION_VER_MINOR  0   /* Minor Version */
#define TF_SESSION_VER_UPDATE 0   /* Update Version */

/**
 * Session Name
 *
 * Name of the TruFlow control channel interface.
 */
#define TF_SESSION_NAME_MAX       64

#define TF_FW_SESSION_ID_INVALID  0xFF  /* Invalid FW Session ID */

/**
 * Session Identifier
 *
 * Unique session identifier which includes PCIe bus info to
 * distinguish the PF and session info to identify the associated
 * TruFlow session. Session ID is constructed from the passed in
 * ctrl_chan_name in tf_open_session() together with an allocated
 * fw_session_id. Done by TruFlow on tf_open_session().
 */
union tf_session_id {
	u32 id;
	struct {
		u8 domain;
		u8 bus;
		u8 device;
		u8 fw_session_id;
	} internal;
};

/**
 * Session Client Identifier
 *
 * Unique identifier for a client within a session. Session Client ID
 * is constructed from the passed in session and a firmware allocated
 * fw_session_client_id. Done by TruFlow on tf_open_session().
 */
union tf_session_client_id {
	u16 id;
	struct {
		u8 fw_session_id;
		u8 fw_session_client_id;
	} internal;
};

/**
 * Session Version
 *
 * The version controls the format of the tf_session and
 * tf_session_info structure. This is to ensure upgrade between
 * versions can be supported.
 *
 * Please see the TF_VER_MAJOR/MINOR and UPDATE defines.
 */
struct tf_session_version {
	u8 major;
	u8 minor;
	u8 update;
};

/**
 * Session supported device types
 */
enum tf_device_type {
	TF_DEVICE_TYPE_P4 = 0,
	TF_DEVICE_TYPE_P5,
	TF_DEVICE_TYPE_MAX     /* Maximum   */
};

/**
 * Module types
 */
enum tf_module_type {
	TF_MODULE_TYPE_IDENTIFIER,	/* Identifier module */
	TF_MODULE_TYPE_TABLE,		/* Table type module */
	TF_MODULE_TYPE_TCAM,		/* TCAM module */
	TF_MODULE_TYPE_EM,		/* EM module */
	TF_MODULE_TYPE_MAX
};

/**
 * Identifier resource types
 */
enum tf_identifier_type {
	TF_IDENT_TYPE_L2_CTXT_HIGH,	/* WH/SR/TH
					 * The L2 Context is returned from the
					 * L2 Ctxt TCAM lookup and can be used
					 * in WC TCAM or EM keys to virtualize
					 * further lookups.
					 */
	TF_IDENT_TYPE_L2_CTXT_LOW,	/* WH/SR/TH
					 * The L2 Context is returned from the
					 * L2 Ctxt TCAM lookup and can be used
					 * in WC TCAM or EM keys to virtualize
					 * further lookups.
					 */
	TF_IDENT_TYPE_PROF_FUNC,	/* WH/SR/TH
					 * The WC profile func is returned
					 * from the L2 Ctxt TCAM lookup to
					 * enable virtualization of the
					 * profile TCAM.
					 */
	TF_IDENT_TYPE_WC_PROF,		/* WH/SR/TH
					 * The WC profile ID is included in
					 * the WC lookup key to enable
					 * virtualization of the WC TCAM
					 * hardware.
					 */
	TF_IDENT_TYPE_EM_PROF,		/* WH/SR/TH
					 * The EM profile ID is included in
					 * the EM lookup key to enable
					 * virtualization of the EM hardware.
					 */
	TF_IDENT_TYPE_L2_FUNC,		/* TH
					 * The L2 func is included in the ILT
					 * result and from recycling to
					 * enable virtualization of further
					 * lookups.
					 */
	TF_IDENT_TYPE_MAX
};

/**
 * Enumeration of TruFlow table types. A table type is used to identify a
 * resource object.
 *
 * NOTE: The table type TF_TBL_TYPE_EXT is unique in that it is
 * the only table type that is connected with a table scope.
 */
enum tf_tbl_type {
	/* Internal */

	TF_TBL_TYPE_FULL_ACT_RECORD,	/* Wh+/SR/TH Action Record */
	TF_TBL_TYPE_COMPACT_ACT_RECORD,	/* TH Compact Action Record */
	TF_TBL_TYPE_MCAST_GROUPS,	/* (Future) Multicast Groups */
	TF_TBL_TYPE_ACT_ENCAP_8B,	/* Wh+/SR/TH Action Encap 8 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_16B,	/* Wh+/SR/TH Action Encap 16 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_32B,	/* WH+/SR/TH Action Encap 32 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_64B,	/* Wh+/SR/TH Action Encap 64 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_128B,	/* TH Action Encap 128 Bytes */
	TF_TBL_TYPE_ACT_SP_SMAC,	/* WH+/SR/TH Action Src Props SMAC */
	TF_TBL_TYPE_ACT_SP_SMAC_IPV4,	/* Wh+/SR/TH Action Src Props SMAC
					 * IPv4
					 */
	TF_TBL_TYPE_ACT_SP_SMAC_IPV6,	/* Wh+/SR/TH Action Src Props SMAC
					 * IPv6
					 */
	TF_TBL_TYPE_ACT_STATS_64,	/* Wh+/SR/TH Action Stats 64 Bits */
	TF_TBL_TYPE_ACT_MODIFY_IPV4,	/* Wh+/SR Action Modify IPv4 Source */
	TF_TBL_TYPE_ACT_MODIFY_8B,	/* TH 8B Modify Record */
	TF_TBL_TYPE_ACT_MODIFY_16B,	/* TH 16B Modify Record */
	TF_TBL_TYPE_ACT_MODIFY_32B,	/* TH 32B Modify Record */
	TF_TBL_TYPE_ACT_MODIFY_64B,	/* TH 64B Modify Record */
	TF_TBL_TYPE_METER_PROF,		/* (Future) Meter Profiles */
	TF_TBL_TYPE_METER_INST,		/* (Future) Meter Instance */
	TF_TBL_TYPE_MIRROR_CONFIG,	/* Wh+/SR/Th Mirror Config */
	TF_TBL_TYPE_UPAR,		/* (Future) UPAR */
	TF_TBL_TYPE_METADATA,		/* (Future) TH Metadata  */
	TF_TBL_TYPE_CT_STATE,		/* (Future) TH CT State  */
	TF_TBL_TYPE_RANGE_PROF,		/* (Future) TH Range Profile  */
	TF_TBL_TYPE_EM_FKB,		/* TH EM Flexible Key builder */
	TF_TBL_TYPE_WC_FKB,		/* TH WC Flexible Key builder */
	TF_TBL_TYPE_METER_DROP_CNT,	/* Meter Drop Counter */

	/* External */

	/**
	 * External table type - initially 1 poolsize entries.
	 * All External table types are associated with a table
	 * scope. Internal types are not.  Currently this is
	 * a pool of 64B entries.
	 */
	TF_TBL_TYPE_EXT,
	TF_TBL_TYPE_MAX
};

/**
 * TCAM table type
 */
enum tf_tcam_tbl_type {
	TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,	/* L2 Context TCAM */
	TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,	/* L2 Context TCAM */
	TF_TCAM_TBL_TYPE_PROF_TCAM,		/* Profile TCAM */
	TF_TCAM_TBL_TYPE_WC_TCAM,		/* Wildcard TCAM */
	TF_TCAM_TBL_TYPE_SP_TCAM,		/* Source Properties TCAM */
	TF_TCAM_TBL_TYPE_CT_RULE_TCAM,    /* Connection Tracking Rule TCAM */
	TF_TCAM_TBL_TYPE_VEB_TCAM,		/* Virtual Edge Bridge TCAM */
	TF_TCAM_TBL_TYPE_WC_TCAM_HIGH,		/* Wildcard TCAM HI Priority */
	TF_TCAM_TBL_TYPE_WC_TCAM_LOW,		/* Wildcard TCAM Low Priority */
	TF_TCAM_TBL_TYPE_MAX
};

/**
 * SEARCH STATUS
 */
enum tf_search_status {
	MISS,	/* entry not found; but an idx allocated if requested */
	HIT,	/* entry found; result/idx are valid */
	REJECT	/* entry not found; table is full */
};

/**
 * EM Resources
 * These defines are provisioned during
 * tf_open_session()
 */
enum tf_em_tbl_type {
	TF_EM_TBL_TYPE_EM_RECORD,    /* # internal EM records for session */
	TF_EM_TBL_TYPE_TBL_SCOPE,    /* # table scopes requested */
	TF_EM_TBL_TYPE_MAX
};

/**
 * TruFlow Session Information
 *
 * @ver:	TrueFlow Version. Used to control the structure layout
 *		when sharing sessions. TruFlow initializes this variable
 *		on tf_open_session().
 * @session_id: Session ID is a unique identifier for the session.
 *              TruFlow initializes this variable during tf_open_session()
 *              processing.
 * @core_data:	The core_data holds the TruFlow tf_session data structure.
 *		This memory is allocated and owned by TruFlow on
 *		tf_open_session(). TruFlow uses this memory for session
 *		management control until the session is closed by ULP.
 *		The ULP is expected to synchronize access to this before
 *		it invokes Core APIs. Please see tf_open_session_parms for
 *		specification details on this variable.
 * @core_data_sz: The field specifies the size of core_data in bytes.
 *		  The size is set by TruFlow on tf_open_session().
 *		  Please see tf_open_session_parms for specification details
 *		  on this variable.
 *
 * Structure defining a TruFlow Session, also known as a Management
 * session. This structure is initialized at time of
 * tf_open_session(). It is passed to all of the TruFlow APIs as way
 * to prescribe and isolate resources between different TruFlow ULP
 * Applications.
 */
struct tf_session_info {
	struct tf_session_version	ver;
	union tf_session_id		session_id;
	void				*core_data;
	u32				core_data_sz_bytes;
};

/**
 * TruFlow handle
 *
 * Contains a pointer to the session info. Allocated by ULP and passed
 * to TruFlow using tf_open_session(). TruFlow will populate the
 * session info at that time. A TruFlow Session can be used by more
 * than one PF/VF by using the tf_open_session().
 */
struct tf {
	struct tf_session_info	*session;    /* session_info (shared) */
	struct bnxt		*bp;	     /* back pointer to parent bp */
};

/**
 * Identifier resource definition
 * @cnt:  Array of TF Identifiers where each entry is expected to be
 *	  set to the requested resource number of that specific type.
 *	  The index used is tf_identifier_type.
 */
struct tf_identifier_resources {
	u16 cnt[TF_IDENT_TYPE_MAX];
};

/**
 * Table type resource definition
 * @cnt:  Array of TF Table types where each entry is expected to be
 *	  set to the requested resource number of that specific
 *	  type. The index used is tf_tbl_type.
 */
struct tf_tbl_resources {
	u16 cnt[TF_TBL_TYPE_MAX];
};

/**
 * TCAM type resource definition
 * @cnt:  Array of TF TCAM types where each entry is expected to be
 *	  set to the requested resource number of that specific
 *	  type. The index used is tf_tcam_tbl_type.
 */
struct tf_tcam_resources {
	u16 cnt[TF_TCAM_TBL_TYPE_MAX];
};

/**
 * EM type resource definition
 * @cnt:  Array of TF EM table types where each entry is expected to
 *	  be set to the requested resource number of that specific
 *	  type. The index used is tf_em_tbl_type.
 */
struct tf_em_resources {
	u16 cnt[TF_EM_TBL_TYPE_MAX];
};

/**
 * tf_session_resources parameter definition.
 * @ident_cnt:	Requested Identifier Resources Number of identifier
 *		resources requested for the session.
 * @tbl_cnt:	Requested Index Table resource counts. The number of
 *		index table resources requested for the session.
 * @tcam_cnt:	Requested TCAM Table resource counts. The number of
 *		TCAM table resources requested for the session.
 * @em_cnt:	Requested EM resource counts. The number of internal
 *		EM table resources requested for the session.
 */
struct tf_session_resources {
	struct tf_identifier_resources	ident_cnt[TF_DIR_MAX];
	struct tf_tbl_resources		tbl_cnt[TF_DIR_MAX];
	struct tf_tcam_resources	tcam_cnt[TF_DIR_MAX];
	struct tf_em_resources		em_cnt[TF_DIR_MAX];
};

/**
 * tf_open_session parameters definition.
 * @ctrl_chan_name:  String containing name of control channel interface to
 *		     be used for this session to communicate with firmware.
 *		     ctrl_chan_name will be used as part of a name for any
 *		     shared memory allocation. The ctrl_chan_name is usually
 *		     in format 0000:02:00.0. The name for shared session is
 *		     0000:02:00.0-tf_shared.
 * @shadow_copy:     Boolean controlling the use and availability of shadow
 *		     copy. Shadow copy will allow the TruFlow to keep track of
 *		     resource content on the firmware side without having to
 *		     query firmware. Additional private session core_data will
 *		     be allocated if this boolean is set to 'true', default
 *		     'false'.
 *
 *		     Size of memory depends on the NVM Resource settings for
 *		     the control channel.
 *
 * @session_id:      Session_id is unique per session. Session_id is
 *		     composed of domain, bus, device and fw_session_id.
 *		     The construction is done by parsing the ctrl_chan_name
 *		     together with allocation of a fw_session_id.
 *		     The session_id allows a session to be shared between
 *		     devices.
 * @session_client_id:	Session_client_id is unique per client.
 *			It is composed of session_id and the
 *			fw_session_client_id fw_session_id. The
 *			construction is done by parsing the ctrl_chan_name
 *			together with allocation of a fw_session_client_id
 *			during tf_open_session(). A reference count will be
 *			incremented in the session on which a client is
 *			created. A session can first be closed if there is
 *			one Session Client left. Session Clients should
 *			be closed using tf_close_session().
 * @device_type:	Device type for the session.
 * @resources:		Resource allocation for the session.
 * @bp:			The pointer to the parent bp struct. This is only
 *			used for HWRM message passing within the portability
 *			layer. The type is struct bnxt.
 * @wc_num_slices:	The number of slices per row for WC TCAM entry.
 * @shared_session_creator:	Indicates whether the application created
 *			the session if set. Otherwise the shared session
 *			already existed. Just for information purposes.
 */
struct tf_open_session_parms {
	char				ctrl_chan_name[TF_SESSION_NAME_MAX];
	union tf_session_id		session_id;
	union tf_session_client_id	session_client_id;
	enum tf_device_type		device_type;
	struct tf_session_resources	resources;
	void				*bp;
	enum tf_wc_num_slice		wc_num_slices;
	int				shared_session_creator;
};

/**
 * tf_open_session: Opens a new TruFlow Session or session client.
 *
 * @tfp:    Pointer to TF handle
 * @parms:  Pointer to open parameters
 *
 * What gets created depends on the passed in tfp content. If the tfp does not
 * have prior session data a new session with associated session client. If tfp
 * has a session already a session client will be created. In both cases the
 * session client is created using the provided ctrl_chan_name.
 *
 * In case of session creation TruFlow will allocate session specific memory to
 * hold its session data. This data is private to TruFlow.
 *
 * No other TruFlow APIs will succeed unless this API is first called
 * and succeeds.
 *
 * tf_open_session() returns a session id and session client id.  These are
 * also stored within the tfp structure passed in to all other APIs.
 *
 * A Session or session client can be closed using tf_close_session().
 *
 * There are 2 types of sessions - shared and not.  For non-shared all
 * the allocated resources are owned and managed by a single session instance.
 * No other applications have access to the resources owned by the non-shared
 * session. For a shared session, resources are shared between 2 applications.
 *
 * When the caller of tf_open_session() sets the ctrl_chan_name[] to a name
 * like "0000:02:00.0-tf_shared", it is a request to create a new "shared"
 * session in the firmware or access the existing shared session. There is
 * only 1 shared session that can be created. If the shared session has
 * already been created in the firmware, this API will return this indication
 * by clearing the shared_session_creator flag. Only the first shared session
 * create will have the shared_session_creator flag set.
 *
 * The shared session should always be the first session to be created by
 * application and the last session closed due to RM management preference.
 *
 * Sessions remain open in the firmware until the last client of the session
 * closes the session (tf_close_session()).
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_open_session(struct tf *tfp, struct tf_open_session_parms *parms);

/**
 * General internal resource info
 *
 * TODO: remove tf_rm_new_entry structure and use this structure
 * internally.
 */
struct tf_resource_info {
	u16 start;
	u16 stride;
};

/**
 * Identifier resource definition
 * @info: Array of TF Identifiers. The index used is tf_identifier_type.
 */
struct tf_identifier_resource_info {
	struct tf_resource_info info[TF_IDENT_TYPE_MAX];
};

/**
 * Table type resource info definition
 * @info: Array of TF Table types. The index used is tf_tbl_type.
 */
struct tf_tbl_resource_info {
	struct tf_resource_info info[TF_TBL_TYPE_MAX];
};

/**
 * TCAM type resource definition
 * @info: Array of TF TCAM types. The index used is tf_tcam_tbl_type.
 */
struct tf_tcam_resource_info {
	struct tf_resource_info info[TF_TCAM_TBL_TYPE_MAX];
};

/**
 * EM type resource definition
 * @info: Array of TF EM table types. The index used is tf_em_tbl_type.
 */
struct tf_em_resource_info {
	struct tf_resource_info info[TF_EM_TBL_TYPE_MAX];
};

/**
 * tf_session_resources parameter definition.
 * @ident:	Requested Identifier Resources. Number of identifier
 *		resources requested for the session.
 * @tbl:	Requested Index Table resource counts. The number of
 *		index table resources requested for the session.
 * @tcam:	Requested TCAM Table resource counts. The number of
 *		TCAM table resources requested for the session.
 * @em:		Requested EM resource counts. The number of internal
 *		EM table resources requested for the session.
 */
struct tf_session_resource_info {
	struct tf_identifier_resource_info	ident[TF_DIR_MAX];
	struct tf_tbl_resource_info		tbl[TF_DIR_MAX];
	struct tf_tcam_resource_info		tcam[TF_DIR_MAX];
	struct tf_em_resource_info		em[TF_DIR_MAX];
};

/**
 * tf_get_session_resources parameter definition.
 * @session_info:	the structure is used to return the information
 *			of allocated resources.
 */
struct tf_get_session_info_parms {
	struct tf_session_resource_info session_info;
};

/** (experimental)
 * Gets info about a TruFlow Session
 *
 * @tfp:	Pointer to TF handle
 * @parms:	Pointer to get parameters
 *
 * Get info about the session which has been created.  Whether it exists and
 * what resource start and stride offsets are in use. This API is primarily
 * intended to be used by an application which has created a shared session
 * This application needs to obtain the resources which have already been
 * allocated for the shared session.
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_get_session_info(struct tf *tfp,
			struct tf_get_session_info_parms *parms);
/**
 * Experimental
 *
 * tf_attach_session parameters definition.
 * @ctrl_chan_name:	String containing name of control channel interface
 *			to be used for this session to communicate with
 *			firmware. The ctrl_chan_name will be used as part of
 *			a name for any shared memory allocation.
 * @attach_chan_name:	String containing name of attach channel interface
 *			to be used for this session. The attach_chan_name
 *			must be given to a 2nd process after the primary
 *			process has been created. This is the ctrl_chan_name
 *			of the primary process and is used to find the shared
 *			memory for the session that the attach is going
 *			to use.
 * @session_id:		Session_id is unique per session. For Attach the
 *			session_id should be the session_id that was returned
 *			on the first open. Session_id is composed of domain,
 *			bus, device and fw_session_id. The construction is
 *			done by parsing the ctrl_chan_name together with
 *			allocation of a fw_session_id during tf_open_session().
 *			A reference count will be incremented on attach.
 *			A session is first fully closed when reference count
 *			is zero by calling tf_close_session().
 */
struct tf_attach_session_parms {
	char			ctrl_chan_name[TF_SESSION_NAME_MAX];
	char			attach_chan_name[TF_SESSION_NAME_MAX];
	union tf_session_id	session_id;
};

/**
 * Experimental
 *
 * Allows a 2nd application instance to attach to an existing
 * session. Used when a session is to be shared between two processes.
 *
 * Attach will increment a ref count as to manage the shared session data.
 *
 * @tfp:	Pointer to TF handle
 * @parms:	Pointer to attach parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_attach_session(struct tf *tfp,
		      struct tf_attach_session_parms *parms);

/**
 * Closes an existing session client or the session it self. The
 * session client is default closed and if the session reference count
 * is 0 then the session is closed as well.
 *
 * On session close all hardware and firmware state associated with
 * the TruFlow application is cleaned up.
 *
 * The session client is extracted from the tfp. Thus tf_close_session()
 * cannot close a session client on behalf of another function.
 *
 * Returns success or failure code.
 */
int tf_close_session(struct tf *tfp);

/**
 * tf_alloc_identifier parameter definition
 *
 * @dir:	receive or transmit direction
 * @ident_type:	Identifier type
 * @id:		Allocated identifier [out]
 */
struct tf_alloc_identifier_parms {
	enum tf_dir		dir;
	enum tf_identifier_type	ident_type;
	u32			id;
};

/**
 * tf_free_identifier parameter definition
 *
 * @dir:	receive or transmit direction
 * @ident_type:	Identifier type
 * @id:		ID to free
 * @refcnt:	Current refcnt after free
 */
struct tf_free_identifier_parms {
	enum tf_dir		dir;
	enum tf_identifier_type	ident_type;
	u32			id;
	u32			ref_cnt;
};

/**
 * allocate identifier resource
 *
 * TruFlow core will allocate a free id from the per identifier resource type
 * pool reserved for the session during tf_open().  No firmware is involved.
 *
 * If shadow copy is enabled, the internal ref_cnt is set to 1 in the
 * shadow table for a newly allocated resource.
 *
 * Returns success or failure code.
 */
int tf_alloc_identifier(struct tf *tfp,
			struct tf_alloc_identifier_parms *parms);

/**
 * free identifier resource
 *
 * TruFlow core will return an id back to the per identifier resource type pool
 * reserved for the session.  No firmware is involved.  During tf_close, the
 * complete pool is returned to the firmware.
 *
 * additional operation (experimental)
 * Decrement reference count.  Only release resource once refcnt goes to 0 if
 * shadow copy is enabled.
 *
 * Returns success or failure code.
 */
int tf_free_identifier(struct tf *tfp,
		       struct tf_free_identifier_parms *parms);

/* DRAM Table Scope Interface
 *
 * If we allocate the EEM memory from the core, we need to store it in
 * the shared session data structure to make sure it can be freed later.
 * (for example if the PF goes away)
 *
 * Current thought is that memory is allocated within core.
 */

/**
 * tf_alloc_tbl_scope_parms definition
 *
 * @rx_max_key_sz_in_bits:		All Maximum key size required
 * @rx_max_action_entry_sz_in_bits:	Maximum Action size required (includes
 *					inlined items)
 * @rx_mem_size_in_mb:			Memory size in Megabytes Total memory
 *					size allocated by user to be divided
 *					up for actions, hash, counters. Only
 *					inline external actions. Use this
 *					variable or the number of flows, do
 *					not set both.
 * @rx_num_flows_in_k:			Number of flows * 1000. If set,
 *					rx_mem_size_in_mb must equal 0.
 * @tx_max_key_sz_in_bits:		All Maximum key size required.
 * @tx_max_action_entry_sz_in_bits:	Maximum Action size required (includes
 *					inlined items)
 * @tx_mem_size_in_mb:			Memory size in Megabytes Total memory
 *					size allocated by user to be divided
 *					up for actions, hash, counters. Only
 *					inline external actions.
 * @tx_num_flows_in_k:			Number of flows * 1000
 * @hw_flow_cache_flush_timer:		Flush pending HW cached flows every
 *					1/10th of value set in seconds, both
 *					idle and active flows are flushed
 *					from the HW cache. If set to 0, this
 *					feature will be disabled.
 * @tbl_scope_id:			table scope identifier
 *
 */
struct tf_alloc_tbl_scope_parms {
	u16	rx_max_key_sz_in_bits;
	u16	rx_max_action_entry_sz_in_bits;
	u32	rx_mem_size_in_mb;
	u32	rx_num_flows_in_k;
	u16	tx_max_key_sz_in_bits;
	u16	tx_max_action_entry_sz_in_bits;
	u32	tx_mem_size_in_mb;
	u32	tx_num_flows_in_k;
	u8	hw_flow_cache_flush_timer;
	u32	tbl_scope_id;
};

/**
 * tf_free_tbl_scope_parms definition
 *
 * @tbl_scope_id:	table scope identifier
 */
struct tf_free_tbl_scope_parms {
	u32 tbl_scope_id;
};

/**
 * tf_map_tbl_scope_parms definition
 *
 * @tbl_scope_id:	table scope identifier
 * @parif_bitmask:	Which parifs are associated with this table scope.
 *			Bit 0 indicates parif 0.
 */
struct tf_map_tbl_scope_parms {
	u32 tbl_scope_id;
	u16 parif_bitmask;
};

/**
 * allocate a table scope
 *
 * The scope is a software construct to identify an EEM table. This function
 * will divide the hash memory/buckets and records according to the device
 * constraints based upon calculations using either the number of flows
 * requested or the size of memory indicated.  Other parameters passed in
 * determine the configuration (maximum key size, maximum external action
 * record size).
 *
 * A single API is used to allocate a common table scope identifier in both
 * receive and transmit CFA. The scope identifier is common due to nature of
 * connection tracking sending notifications between RX and TX direction.
 *
 * The receive and transmit table access identifiers specify which rings will
 * be used to initialize table DRAM.  The application must ensure mutual
 * exclusivity of ring usage for table scope allocation and any table update
 * operations.
 *
 * The hash table buckets, EM keys, and EM lookup results are stored in the
 * memory allocated based on the rx_em_hash_mb/tx_em_hash_mb parameters.  The
 * hash table buckets are stored at the beginning of that memory.
 *
 * NOTE:  No EM internal setup is done here. On chip EM records are managed
 * internally by TruFlow core.
 *
 * Returns success or failure code.
 */
int tf_alloc_tbl_scope(struct tf *tfp,
		       struct tf_alloc_tbl_scope_parms *parms);

/**
 * map a table scope (legacy device only Wh+/SR)
 *
 * Map a table scope to one or more partition interfaces (parifs).
 * The parif can be remapped in the L2 context lookup for legacy devices.  This
 * API allows a number of parifs to be mapped to the same table scope.  On
 * legacy devices a table scope identifies one of 16 sets of EEM table base
 * addresses and is associated with a PF communication channel.  The associated
 * PF must be onfigured for the table scope to operate.
 *
 * An L2 context TCAM lookup returns a remapped parif value used to
 * index into the set of 16 parif_to_pf registers which are used to map to one
 * of the 16 table scopes.  This API allows the user to map the parifs in the
 * mask to the previously allocated table scope (EEM table).

 * Returns success or failure code.
 */
int tf_map_tbl_scope(struct tf *tfp, struct tf_map_tbl_scope_parms *parms);

/**
 * free a table scope
 *
 * Firmware checks that the table scope ID is owned by the TruFlow
 * session, verifies that no references to this table scope remains
 * or Profile TCAM entries for either CFA (RX/TX) direction,
 * then frees the table scope ID.
 *
 * Returns success or failure code.
 */
int tf_free_tbl_scope(struct tf *tfp, struct tf_free_tbl_scope_parms *parms);

/**
 * tf_alloc_tcam_entry parameter definition
 *
 * @dir:		receive or transmit direction
 * @tcam_tbl_type:	TCAM table type
 * @search_enable:	Enable search for matching entry
 * @key:		Key data to match on (if search)
 * @key_sz_in_bits:	key size in bits (if search)
 * @mask:		Mask data to match on (if search)
 * @priority:		Priority of entry requested (definition TBD)
 * @hit:		If search, set if matching entry found
 * @ref_cnt:		Current refcnt after allocation [out]
 * @idx:		Idx allocated
 */
struct tf_alloc_tcam_entry_parms {
	enum tf_dir		dir;
	enum tf_tcam_tbl_type	tcam_tbl_type;
	u8			search_enable;
	u8			*key;
	u16			key_sz_in_bits;
	u8			*mask;
	u32			priority;
	u8			hit;
	u16			ref_cnt;
	u16			idx;
};

/**
 * allocate TCAM entry
 *
 * Allocate a TCAM entry - one of these types:
 *
 * L2 Context
 * Profile TCAM
 * WC TCAM
 * VEB TCAM
 *
 * This function allocates a TCAM table record.	 This function
 * will attempt to allocate a TCAM table entry from the session
 * owned TCAM entries or search a shadow copy of the TCAM table for a
 * matching entry if search is enabled.	 Key, mask and result must match for
 * hit to be set.  Only TruFlow core data is accessed.
 * A hash table to entry mapping is maintained for search purposes.  If
 * search is not enabled, the first available free entry is returned based
 * on priority and alloc_cnt is set to 1.  If search is enabled and a matching
 * entry to entry_data is found, hit is set to TRUE and alloc_cnt is set to 1.
 * RefCnt is also returned.
 *
 * Also returns success or failure code.
 */
int tf_alloc_tcam_entry(struct tf *tfp,
			struct tf_alloc_tcam_entry_parms *parms);

/**
 * tf_set_tcam_entry parameter definition
 *
 * @dir:		receive or transmit direction
 * @tcam_tbl_type:	TCAM table type
 * @idx:		base index of the entry to program
 * @key:		struct containing key
 * @mask:		struct containing mask fields
 * @key_sz_in_bits:	key size in bits (if search)
 * @result:		struct containing result
 * @result_sz_in_bits:	struct containing result size in bits
 */
struct	tf_set_tcam_entry_parms {
	enum tf_dir		dir;
	enum tf_tcam_tbl_type	tcam_tbl_type;
	u16			idx;
	u8			*key;
	u8			*mask;
	u16			key_sz_in_bits;
	u8			*result;
	u16			result_sz_in_bits;
};

/**
 * set TCAM entry
 *
 * Program a TCAM table entry for a TruFlow session.
 *
 * If the entry has not been allocated, an error will be returned.
 *
 * Returns success or failure code.
 */
int tf_set_tcam_entry(struct tf	*tfp, struct tf_set_tcam_entry_parms *parms);

/**
 * tf_get_tcam_entry parameter definition
 * @dir:		receive or transmit direction
 * @tcam_tbl_type:	TCAM table type
 * @idx:		index of the entry to get
 * @key:		struct containing key [out]
 * @mask:		struct containing mask fields [out]
 * @key_sz_in_bits:	key size in bits
 * @result:		struct containing result
 * @result_sz_in_bits:	struct containing result size in bits
 */
struct tf_get_tcam_entry_parms {
	enum tf_dir		dir;
	enum tf_tcam_tbl_type	tcam_tbl_type;
	u16			idx;
	u8			*key;
	u8			*mask;
	u16			key_sz_in_bits;
	u8			*result;
	u16			result_sz_in_bits;
};

/**
 * get TCAM entry
 *
 * Program a TCAM table entry for a TruFlow session.
 *
 * If the entry has not been allocated, an error will be returned.
 *
 * Returns success or failure code.
 */
int tf_get_tcam_entry(struct tf *tfp, struct tf_get_tcam_entry_parms *parms);

/**
 * tf_free_tcam_entry parameter definition
 *
 * @dir:		receive or transmit direction
 * @tcam_tbl_type:	TCAM table type
 * @idx:		Index to free
 * @ref_cnt:		reference count after free
 */
struct tf_free_tcam_entry_parms {
	enum tf_dir		dir;
	enum tf_tcam_tbl_type	tcam_tbl_type;
	u16			idx;
	u16			ref_cnt;
};

/**
 * Free TCAM entry.
 *
 * Firmware checks to ensure the TCAM entries are owned by the TruFlow
 * session.  TCAM entry will be invalidated.  All-ones mask.
 * writes to hw.
 *
 * WCTCAM profile id of 0 must be used to invalidate an entry.
 *
 * Returns success or failure code.
 */
int tf_free_tcam_entry(struct tf *tfp,
		       struct tf_free_tcam_entry_parms *parms);

/**
 * tf_alloc_tbl_entry parameter definition
 *
 * @dir:		Receive or transmit direction
 * @type:		Type of the allocation
 * @tbl_scope_id:	Table scope identifier (ignored unless TF_TBL_TYPE_EXT)
 * @idx:		Idx of allocated entry
 */
struct tf_alloc_tbl_entry_parms {
	enum tf_dir		dir;
	enum tf_tbl_type	type;
	u32			tbl_scope_id;
	u32			idx;
};

/**
 * allocate index table entries
 *
 * Internal types:
 *
 * Allocate an on chip index table entry or search for a matching
 * entry of the indicated type for this TruFlow session.
 *
 * Allocates an index table record. This function will attempt to
 * allocate an index table entry.
 *
 * External types:
 *
 * These are used to allocate inlined action record memory.
 *
 * Allocates an external index table action record.
 *
 * NOTE:
 * Implementation of the internals of the external function will be a stack with
 * push and pop.
 *
 * Returns success or failure code.
 */
int tf_alloc_tbl_entry(struct tf *tfp,
		       struct tf_alloc_tbl_entry_parms *parms);

/**
 * tf_free_tbl_entry parameter definition
 *
 * @dir:	Receive or transmit direction
 * @type:	Type of the allocation
 * @tbl_scope_id:  Table scope identifier (ignored unless TF_TBL_TYPE_EXT)
 * @idx:	Index to free
 */
struct tf_free_tbl_entry_parms {
	enum tf_dir		dir;
	enum tf_tbl_type	type;
	u32			tbl_scope_id;
	u32			idx;
};

/**
 * free index table entry
 *
 * Used to free a previously allocated table entry.
 *
 * Internal types:
 *
 * The element is freed and given back to the session pool.
 *
 * External types:
 *
 * Frees an external index table action record.
 *
 * NOTE:
 * Implementation of the internals of the external table will be a stack with
 * push and pop.
 *
 * Returns success or failure code.
 */
int tf_free_tbl_entry(struct tf *tfp, struct tf_free_tbl_entry_parms *parms);

/**
 * tf_set_tbl_entry parameter definition
 *
 * @tbl_scope_id:	Table scope identifier
 * @dir:		Receive or transmit direction
 * @type:		Type of object to set
 * @data:		Entry data
 * @data_sz_in_bytes:	Entry size
 * @chan_type:		External memory channel type to use
 * @idx:		Entry index to write to
 */
struct tf_set_tbl_entry_parms {
	u32				tbl_scope_id;
	enum tf_dir			dir;
	enum tf_tbl_type		type;
	u8				*data;
	u16				data_sz_in_bytes;
	enum tf_ext_mem_chan_type	chan_type;
	u32				idx;
};

/**
 * set index table entry
 *
 * Used to set an application programmed index table entry into a
 * previous allocated table location.
 *
 * Returns success or failure code.
 */
int tf_set_tbl_entry(struct tf *tfp, struct tf_set_tbl_entry_parms *parms);

/**
 * tf_get_shared_tbl_increment parameter definition
 *
 * @dir:		Receive or transmit direction
 * @type:		Type of object to get
 * @increment_cnt:	Value to increment by for resource type
 */
struct tf_get_shared_tbl_increment_parms {
	enum tf_dir		dir;
	enum tf_tbl_type	type;
	u32			increment_cnt;
};

/**
 * tf_get_shared_tbl_increment
 *
 * This API is currently only required for use in the shared
 * session for Thor (p58) actions.  An increment count is returned per
 * type to indicate how much to increment the start by for each
 * entry (see tf_resource_info)
 *
 * Returns success or failure code.
 */
int tf_get_shared_tbl_increment(struct tf *tfp,
				struct tf_get_shared_tbl_increment_parms
				*parms);

/**
 * tf_get_tbl_entry parameter definition
 *
 * @dir:		Receive or transmit direction
 * @type:		Type of object to get
 * @data:		Entry data
 * @data_sz_in_bytes:	Entry size
 * @chan_type:		External memory channel type to use
 * @idx:		Entry index to read
 */
struct tf_get_tbl_entry_parms {
	enum tf_dir			dir;
	enum tf_tbl_type		type;
	u8				*data;
	u16				data_sz_in_bytes;
	enum tf_ext_mem_chan_type	chan_type;
	u32				idx;
};

/**
 * get index table entry
 *
 * Used to retrieve a previous set index table entry.
 *
 * Reads and compares with the shadow table copy (if enabled) (only
 * for internal objects).
 *
 * Returns success or failure code. Failure will be returned if the
 * provided data buffer is too small for the data type requested.
 */
int tf_get_tbl_entry(struct tf *tfp,
		     struct tf_get_tbl_entry_parms *parms);

/**
 * tf_bulk_get_tbl_entry parameter definition
 *
 * @dir:		Receive or transmit direction
 * @type:		Type of object to get
 * @starting_idx:	Starting index to read from
 * @num_entries:	Number of sequential entries
 * @entry_sz_in_bytes:	Size of the single entry
 * @physical_mem_addr:	Host physical address, where the data will be copied
 *			to by the firmware.
 * @chan_type:		External memory channel type to use
 */
struct tf_bulk_get_tbl_entry_parms {
	enum tf_dir			dir;
	enum tf_tbl_type		type;
	u32				starting_idx;
	u16				num_entries;
	u16				entry_sz_in_bytes;
	u64				physical_mem_addr;
	enum tf_ext_mem_chan_type	chan_type;
};

/**
 * Bulk get index table entry
 *
 * Used to retrieve a set of index table entries.
 *
 * Entries within the range may not have been allocated using
 * tf_alloc_tbl_entry() at the time of access. But the range must
 * be within the bounds determined from tf_open_session() for the
 * given table type.  Currently, this is only used for collecting statistics.
 *
 * Returns success or failure code. Failure will be returned if the
 * provided data buffer is too small for the data type requested.
 */
int tf_bulk_get_tbl_entry(struct tf *tfp,
			  struct tf_bulk_get_tbl_entry_parms *parms);

/**
 * Exact Match Table
 */

/**
 * tf_insert_em_entry parameter definition
 *
 * @dir:		Receive or transmit direction
 * @mem:		internal or external
 * @tbl_scope_id:	ID of table scope to use (external only)
 * @key:		ptr to structure containing key fields
 * @key_sz_in_bits:	key bit length
 * @em_record:		ptr to structure containing result field
 * @em_record_sz_in_bits:	result size in bits
 * @dup_check:		duplicate check flag
 * @chan_type:		External memory channel type to use
 * @flow_handle:	Flow handle value for the inserted entry. This is
 *			encoded as the entries[4]:bucket[2]:hashId[1]:hash[14]
 * @flow_id:		Flow id is returned as null (internal). Flow id is
 *			the GFID value for the inserted entry (external).
 *			This is the value written to the BD and useful
 *			information for mark.
 */
struct tf_insert_em_entry_parms {
	enum tf_dir			dir;
	enum tf_mem			mem;
	u32				tbl_scope_id;
	u8				*key;
	u16				key_sz_in_bits;
	u8				*em_record;
	u16				em_record_sz_in_bits;
	u8				dup_check;
	enum tf_ext_mem_chan_type	chan_type;
	u64				flow_handle;
	u64				flow_id;
};

/**
 * tf_delete_em_entry parameter definition
 *
 * @dir:		Receive or transmit direction
 * @mem:		internal or external
 * @tbl_scope_id:	ID of table scope to use (external only)
 * @index:		The index of the entry
 * @chan_type:		External memory channel type to use
 * @flow_handle:	structure containing flow delete handle information
 *
 */
struct tf_delete_em_entry_parms {
	enum tf_dir			dir;
	enum tf_mem			mem;
	u32				tbl_scope_id;
	u16				index;
	enum tf_ext_mem_chan_type	chan_type;
	u64				flow_handle;
};

/**
 * tf_move_em_entry parameter definition
 *
 * @dir:		Receive or transmit direction
 * @mem:		internal or external
 * @tbl_scope_id:	ID of table scope to use (external only)
 * @tbl_if_id:		ID of table interface to use (SR2 only)
 * @epochs:		epoch group IDs of entry to delete 2 element array
 *			with 2 ids. (SR2 only)
 * @index:		The index of the entry
 * @chan_type:		External memory channel type to use
 * @new_index:		The index of the new EM record
 * @flow_handle:	structure containing flow delete handle information
 */
struct tf_move_em_entry_parms {
	enum tf_dir			dir;
	enum				tf_mem mem;
	u32				tbl_scope_id;
	u32				tbl_if_id;
	u16				*epochs;
	u16				index;
	enum tf_ext_mem_chan_type	chan_type;
	u32				new_index;
	u64				flow_handle;
};

/**
 * insert em hash entry in internal table memory
 *
 * Internal:
 *
 * This API inserts an exact match entry into internal EM table memory
 * of the specified direction.
 *
 * Note: The EM record is managed within the TruFlow core and not the
 * application.
 *
 * Shadow copy of internal record table an association with hash and 1,2, or 4
 * associated buckets
 *
 * External:
 * This API inserts an exact match entry into DRAM EM table memory of the
 * specified direction and table scope.
 *
 * The insertion of duplicate entries in an EM table is not permitted.	If a
 * TruFlow application can guarantee that it will never insert duplicates, it
 * can disable duplicate checking by passing a zero value in the  dup_check
 * parameter to this API.  This will optimize performance. Otherwise, the
 * TruFlow library will enforce protection against inserting duplicate entries.
 *
 * Flow handle is defined in this document:
 *
 * https://docs.google.com
 * /document/d/1NESu7RpTN3jwxbokaPfYORQyChYRmJgs40wMIRe8_-Q/edit
 *
 * Returns success or busy code.
 *
 */
int tf_insert_em_entry(struct tf *tfp,
		       struct tf_insert_em_entry_parms *parms);

/**
 * delete em hash entry table memory
 *
 * Internal:
 *
 * This API deletes an exact match entry from internal EM table memory of the
 * specified direction. If a valid flow ptr is passed in then that takes
 * precedence over the pointer to the complete key passed in.
 *
 *
 * External:
 *
 * This API deletes an exact match entry from EM table memory of the specified
 * direction and table scope. If a valid flow handle is passed in then that
 * takes precedence over the pointer to the complete key passed in.
 *
 * The TruFlow library may release a dynamic bucket when an entry is deleted.
 *
 *
 * Returns success or not found code
 *
 *
 */
int tf_delete_em_entry(struct tf *tfp,
		       struct tf_delete_em_entry_parms *parms);

/**
 * Tunnel Encapsulation Offsets
 */
enum tf_tunnel_encap_offsets {
	TF_TUNNEL_ENCAP_L2,
	TF_TUNNEL_ENCAP_NAT,
	TF_TUNNEL_ENCAP_MPLS,
	TF_TUNNEL_ENCAP_VXLAN,
	TF_TUNNEL_ENCAP_GENEVE,
	TF_TUNNEL_ENCAP_NVGRE,
	TF_TUNNEL_ENCAP_GRE,
	TF_TUNNEL_ENCAP_FULL_GENERIC
};

/**
 * Global Configuration Table Types
 */
enum tf_global_config_type {
	TF_TUNNEL_ENCAP,  /* Tunnel Encap Config(TECT) */
	TF_ACTION_BLOCK,  /* Action Block Config(ABCR) */
	TF_COUNTER_CFG,   /* Counter Configuration (CNTRS_CTRL) */
	TF_METER_CFG,		/* Meter Config(ACTP4_FMTCR) */
	TF_METER_INTERVAL_CFG,	/* METER Interval Config(FMTCR_INTERVAL) */
	TF_DSCP_RMP_CFG,	/* Remap IPv6 DSCP */
	TF_MIRROR_CFG,		/* Mirror config */
	TF_ACT_MTR_CFG,	  /* Drop on Red, Packet mode length Config(ACT_MTR_CFG) */
	TF_GLOBAL_CFG_TYPE_MAX
};

/**
 * tf_global_cfg parameter definition
 * @dir:		receive or transmit direction
 * @type:		Global config type
 * @offset:		Offset @ the type
 * @config:		Value of the configuration.
 *			set - Read, Modify and Write
 *			get - Read the full configuration
 * @config_mask:	Configuration mask
 *			set - Read, Modify with mask and Write
 *			get - unused
 * @config_sz_in_bytes: struct containing size
 */
struct tf_global_cfg_parms {
	enum tf_dir			dir;
	enum tf_global_config_type	type;
	u32				offset;
	u8				*config;
	u8				*config_mask;
	u16				config_sz_in_bytes;
};

/**
 * Get global configuration
 *
 * Retrieve the configuration
 *
 * Returns success or failure code.
 */
int tf_get_global_cfg(struct tf *tfp,
		      struct tf_global_cfg_parms *parms);

/**
 * Update the global configuration table
 *
 * Read, modify write the value.
 *
 * Returns success or failure code.
 */
int tf_set_global_cfg(struct tf *tfp,
		      struct tf_global_cfg_parms *parms);

/**
 * Enumeration of TruFlow interface table types.
 */
enum tf_if_tbl_type {
	TF_IF_TBL_TYPE_PROF_SPIF_DFLT_L2_CTXT,	     /* Default Profile L2
						      * Context Entry
						      */
	TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR,  /* Default Profile TCAM/
						      * Lookup Action Record
						      * Pointer Table
						      */
	TF_IF_TBL_TYPE_PROF_PARIF_ERR_ACT_REC_PTR,   /* Error Profile TCAM
						      * Miss Action Record
						      * Pointer Table
						      */
	TF_IF_TBL_TYPE_LKUP_PARIF_DFLT_ACT_REC_PTR,  /* Default Error Profile
						      * TCAM Miss Action
						      * Record Pointer Table
						      */
	TF_IF_TBL_TYPE_ILT,			     /* Ingress lookup table */
	TF_IF_TBL_TYPE_VSPT,			     /* VNIC/SVIF Props Tbl */
	TF_IF_TBL_TYPE_MAX
};

/**
 * tf_set_if_tbl_entry parameter definition
 *
 * @dir:	Receive or transmit direction
 * @type:	Type of object to set
 * @data:	Entry data
 * @data_sz_in_bytes:	Entry size
 * @idx:	Interface to write
 */
struct tf_set_if_tbl_entry_parms {
	enum tf_dir		dir;
	enum tf_if_tbl_type	type;
	u8			*data;
	u16			data_sz_in_bytes;
	u32			idx;
};

/**
 * set interface table entry
 *
 * Used to set an interface table. This API is used for managing tables indexed
 * by SVIF/SPIF/PARIF interfaces. In current implementation only the value is
 * set.
 * Returns success or failure code.
 */
int tf_set_if_tbl_entry(struct tf *tfp,
			struct tf_set_if_tbl_entry_parms *parms);

/**
 * tf_get_if_tbl_entry parameter definition
 *
 * @dir:	Receive or transmit direction
 * @type:	Type of object to get
 * @data:	Entry data
 * @data_sz_in_bytes:	Entry size
 * @idx:	Entry index to read
 */
struct tf_get_if_tbl_entry_parms {
	enum tf_dir		dir;
	enum tf_if_tbl_type	type;
	u8			*data;
	u16			data_sz_in_bytes;
	u32			idx;
};

/**
 * get interface table entry
 *
 * Used to retrieve an interface table entry.
 *
 * Reads the interface table entry value
 *
 * Returns success or failure code. Failure will be returned if the
 * provided data buffer is too small for the data type requested.
 */
int tf_get_if_tbl_entry(struct tf *tfp,
			struct tf_get_if_tbl_entry_parms *parms);

/**
 * tf_get_version parameters definition.
 *
 * @device_type:	Device type for the session.
 * @bp:			The pointer to the parent bp struct. This is only used
 *			for HWRM message passing within the portability layer.
 *			The type is struct bnxt.
 * @major:		Version Major number.
 * @minor:		Version Minor number.
 * @update:		Version Update number.
 * @dev_ident_caps:	fw available identifier resource list
 * @dev_tbl_caps:	fw available table resource list
 * @dev_tcam_caps:	fw available tcam resource list
 * @dev_em_caps:	fw available em resource list
 */
struct tf_get_version_parms {
	enum tf_device_type	device_type;
	void			*bp;
	u8			major;
	u8			minor;
	u8			update;
	u32			dev_ident_caps;
	u32			dev_tbl_caps;
	u32			dev_tcam_caps;
	u32			dev_em_caps;
};

/**
 * Get tf fw version
 * Used to retrieve Truflow fw version information.
 * Returns success or failure code.
 */
int tf_get_version(struct tf *tfp, struct tf_get_version_parms *parms);

/**
 * tf_query_sram_resources parameter definition
 *
 * @device_type:	Device type for the session.
 * @bp:			The pointer to the parent bp struct. This is only used
 *			for HWRM message passing within the portability layer.
 *			The type is struct bnxt.
 * @dir:		Receive or transmit direction
 * @bank_resc_count:	Bank resource count in 8 bytes entry
 * @dynamic_sram_capable:	Dynamic SRAM Enable
 * @sram_profile:	SRAM profile
 */
struct tf_query_sram_resources_parms {
	enum tf_device_type	device_type;
	void			*bp;
	enum tf_dir		dir;
	u32			bank_resc_count[TF_SRAM_BANK_ID_MAX];
	bool			dynamic_sram_capable;
	u8			sram_profile;
};

/**
 * Get SRAM resources information
 * Used to retrieve sram bank partition information
 * Returns success or failure code.
 */
int tf_query_sram_resources(struct tf *tfp,
			    struct tf_query_sram_resources_parms *parms);

/**
 * tf_set_sram_policy parameter definition
 *
 * @device_type:	Device type for the session.
 * @dir:		Receive or transmit direction
 * @bank_id:		Array of Bank id for each truflow tbl type
 */
struct tf_set_sram_policy_parms {
	enum tf_device_type	device_type;
	enum tf_dir		dir;
	enum tf_sram_bank_id	bank_id[TF_TBL_TYPE_ACT_MODIFY_64B + 1];
};

/**
 * Set SRAM policy
 * Used to assign SRAM bank index to all truflow table type.
 * Returns success or failure code.
 */
int tf_set_sram_policy(struct tf *tfp, struct tf_set_sram_policy_parms *parms);

/**
 * tf_get_sram_policy parameter definition
 *
 * @device_type:	Device type for the session.
 * @dir:		Receive or transmit direction
 * @bank_id:		Array of Bank id for each truflow tbl type
 */
struct tf_get_sram_policy_parms {
	enum tf_device_type	device_type;
	enum tf_dir		dir;
	enum tf_sram_bank_id	bank_id[TF_TBL_TYPE_ACT_MODIFY_64B + 1];
};

/**
 * Get SRAM policy
 * Used to get the assigned bank of table types.
 * Returns success or failure code.
 */
int tf_get_sram_policy(struct tf *tfp, struct tf_get_sram_policy_parms *parms);

#endif /* _TF_CORE_H_ */
