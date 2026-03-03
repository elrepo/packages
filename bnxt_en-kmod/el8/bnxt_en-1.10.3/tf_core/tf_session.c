// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include "tf_session.h"
#include "tf_msg.h"
#include "bnxt.h"

struct tf_session_client_create_parms {
	char	*ctrl_chan_name;	/* Control channel name string */
	union tf_session_client_id *session_client_id;	/* Firmware Session
							 * Client ID (out)
							 */
};

struct tf_session_client_destroy_parms {
	union tf_session_client_id session_client_id;	/* Firmware Session
							 * Client ID (out)
							 */
};

static int tfp_get_fid(struct tf *tfp, uint16_t *fw_fid)
{
	struct bnxt *bp = NULL;

	if (!tfp || !fw_fid)
		return -EINVAL;

	bp = (struct bnxt *)tfp->bp;
	if (!bp)
		return -EINVAL;

	*fw_fid = bp->pf.fw_fid;

	return 0;
}

/**
 * Creates a Session and the associated client.
 *
 * @tfp:	Pointer to TF handle
 * @parms:	Pointer to session client create parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 *   - (-ENOMEM) if max session clients has been reached.
 */
static int tf_session_create(struct tf *tfp,
			     struct tf_session_open_session_parms *parms)
{
	struct tf_session *session = NULL;
	struct tf_session_client *client;
	union tf_session_id *session_id;
	bool shared_session_creator;
	u8 fw_session_client_id;
	struct tf_dev_info dev;
	char *shared_name;
	u8 fw_session_id;
	void *core_data;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;
	tf_dev_bind_ops(parms->open_cfg->device_type,
			&dev);

	/* Open FW session and get a new session_id */
	rc = tf_msg_session_open(parms->open_cfg->bp,
				 parms->open_cfg->ctrl_chan_name,
				 &fw_session_id,
				 &fw_session_client_id,
				 &shared_session_creator);
	if (rc) {
		/* Log error */
		if (rc == -EEXIST) {
			netdev_dbg(tfp->bp->dev,
				   "Session is already open, rc:%d\n", rc);
		} else {
			netdev_dbg(tfp->bp->dev,
				   "Open message send failed, rc:%d\n", rc);
		}

		parms->open_cfg->session_id.id = TF_FW_SESSION_ID_INVALID;
		return rc;
	}
	/* Allocate session */
	tfp->session = vzalloc(sizeof(*tfp->session));
	if (!tfp->session) {
		rc = -ENOMEM;
		goto cleanup_fw_session;
	}

	/* Allocate core data for the session */
	core_data = vzalloc(sizeof(*session));
	if (!core_data) {
		rc = -ENOMEM;
		goto cleanup_session;
	}

	tfp->session->core_data = core_data;
	session_id = &parms->open_cfg->session_id;

	/* Update Session Info, which is what is visible to the caller */
	tfp->session->ver.major = 0;
	tfp->session->ver.minor = 0;
	tfp->session->ver.update = 0;

	tfp->session->session_id.internal.domain = session_id->internal.domain;
	tfp->session->session_id.internal.bus = session_id->internal.bus;
	tfp->session->session_id.internal.device = session_id->internal.device;
	tfp->session->session_id.internal.fw_session_id = fw_session_id;

	/* Initialize Session and Device, which is private */
	session = (struct tf_session *)tfp->session->core_data;
	session->ver.major = 0;
	session->ver.minor = 0;
	session->ver.update = 0;

	session->session_id.internal.domain = session_id->internal.domain;
	session->session_id.internal.bus = session_id->internal.bus;
	session->session_id.internal.device = session_id->internal.device;
	session->session_id.internal.fw_session_id = fw_session_id;
	/* Return the allocated session id */
	session_id->id = session->session_id.id;

	/* Init session client list */
	INIT_LIST_HEAD(&session->client_ll);

	/* Create the local session client, initialize and attach to
	 * the session
	 */
	client = vzalloc(sizeof(*client));
	if (!client) {
		rc = -ENOMEM;
		goto cleanup_core_data;
	}

	/* Register FID with the client */
	rc = tfp_get_fid(tfp, &client->fw_fid);
	if (rc)
		goto cleanup_client;

	client->session_client_id.internal.fw_session_id = fw_session_id;
	client->session_client_id.internal.fw_session_client_id =
		fw_session_client_id;

	memcpy(client->ctrl_chan_name, parms->open_cfg->ctrl_chan_name,
	       TF_SESSION_NAME_MAX);

	list_add(&client->ll_entry, &session->client_ll);
	session->ref_count++;

	/* Init session em_ext_db */
	session->em_ext_db_handle = NULL;

	/* Populate the request */
	shared_name = strstr(parms->open_cfg->ctrl_chan_name, "tf_shared");
	if (shared_name)
		session->shared_session = true;

	if (session->shared_session && shared_session_creator) {
		session->shared_session_creator = true;
		parms->open_cfg->shared_session_creator = true;
	}

	rc = tf_dev_bind(tfp,
			 parms->open_cfg->device_type,
			 &parms->open_cfg->resources,
			 parms->open_cfg->wc_num_slices,
			 &session->dev);

	/* Logging handled by dev_bind */
	if (rc)
		goto cleanup_client;

	session->dev_init = true;

	return 0;

cleanup_client:
	vfree(client);
cleanup_core_data:
	vfree(tfp->session->core_data);
cleanup_session:
	vfree(tfp->session);
	tfp->session = NULL;
cleanup_fw_session:
	if (tf_msg_session_close(tfp, fw_session_id))
		netdev_dbg(tfp->bp->dev, "FW Session close failed");

	return rc;
}

/**
 * Creates a Session Client on an existing Session.
 *
 * @tfp:	Pointer to TF handle
 * @parms:	Pointer to session client create parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 *   - (-ENOMEM) if max session clients has been reached.
 */
static int tf_session_client_create(struct tf *tfp,
				    struct tf_session_client_create_parms
				    *parms)
{
	union tf_session_client_id session_client_id;
	struct tf_session *session = NULL;
	struct tf_session_client *client;
	u8 fw_session_id;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	/* Using internal version as session client may not exist yet */
	rc = tf_session_get_session_internal(tfp, &session);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "Failed to lookup session, rc:%d\n",
			   rc);
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Session Firmware id lookup failed, rc:%d\n", rc);
		return rc;
	}

	client = tf_session_find_session_client_by_name(session,
							parms->ctrl_chan_name);
	if (client) {
		netdev_dbg(tfp->bp->dev,
			   "Client %s already registered with this session\n",
			   parms->ctrl_chan_name);
		return -EOPNOTSUPP;
	}

	rc = tf_msg_session_client_register
		    (tfp,
		     parms->ctrl_chan_name,
		     fw_session_id,
		     &session_client_id.internal.fw_session_client_id);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to create client on session, rc:%d\n", rc);
		return rc;
	}

	/* Create the local session client, initialize and attach to
	 * the session
	 */
	client = vzalloc(sizeof(*client));
	if (!client) {
		rc = -ENOMEM;
		goto cleanup;
	}

	/* Register FID with the client */
	rc = tfp_get_fid(tfp, &client->fw_fid);
	if (rc)
		return rc;

	/* Build the Session Client ID by adding the fw_session_id */
	session_client_id.internal.fw_session_id = fw_session_id;
	memcpy(client->ctrl_chan_name, parms->ctrl_chan_name,
	       TF_SESSION_NAME_MAX);

	client->session_client_id.id = session_client_id.id;

	list_add(&client->ll_entry, &session->client_ll);

	session->ref_count++;

	/* Build the return value */
	parms->session_client_id->id = session_client_id.id;

 cleanup:
	/* TBD - Add code to unregister newly create client from fw */

	return rc;
}

/**
 * Destroys a Session Client on an existing Session.
 *
 * @tfp:	Pointer to TF handle
 * @parms:	Pointer to the session client destroy parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 *   - (-ENOTFOUND) error, client not owned by the session.
 *   - (-EOPNOTSUPP) error, unable to destroy client as its the last
 *                 client. Please use the tf_session_close().
 */
static int tf_session_client_destroy(struct tf *tfp,
				     struct tf_session_client_destroy_parms
				     *parms)
{
	struct tf_session_client *client;
	u8 fw_session_client_id;
	struct tf_session *tfs;
	u8 fw_session_id;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	fw_session_client_id =
		parms->session_client_id.internal.fw_session_client_id;

	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "Failed to lookup session, rc:%d\n",
			   rc);
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Session Firmware id lookup failed, rc:%d\n", rc);
		return rc;
	}

	/* Check session owns this client and that we're not the last client */
	client = tf_session_get_session_client(tfs,
					       parms->session_client_id);
	if (!client) {
		netdev_dbg(tfp->bp->dev,
			   "Client %d, not found within this session\n",
			   parms->session_client_id.id);
		return -EINVAL;
	}

	/* If last client the request is rejected and cleanup should
	 * be done by session close.
	 */
	if (tfs->ref_count == 1)
		return -EOPNOTSUPP;

	rc = tf_msg_session_client_unregister(tfp, fw_session_id,
					      fw_session_client_id);

	/* Log error, but continue. If FW fails we do not really have
	 * a way to fix this but the client would no longer be valid
	 * thus we remove from the session.
	 */
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Client destroy on FW Failed, rc:%d\n", rc);
	}

	list_del(&client->ll_entry);

	/* Decrement the session ref_count */
	tfs->ref_count--;

	vfree(client);

	return rc;
}

int tf_session_open_session(struct tf *tfp,
			    struct tf_session_open_session_parms *parms)
{
	struct tf_session_client_create_parms scparms;
	int rc;

	if (!tfp || !parms || !parms->open_cfg->bp)
		return -EINVAL;

	tfp->bp = parms->open_cfg->bp;
	/* Decide if we're creating a new session or session client */
	if (!tfp->session) {
		rc = tf_session_create(tfp, parms);
		if (rc) {
			netdev_dbg(tfp->bp->dev,
				   "Failed to create session: %s, rc:%d\n",
				   parms->open_cfg->ctrl_chan_name, rc);
			return rc;
		}

		netdev_dbg(tfp->bp->dev,
			   "Session created, session_client_id:%d, session_id:0x%08x, fw_session_id:%d\n",
			   parms->open_cfg->session_client_id.id,
			   parms->open_cfg->session_id.id,
			   parms->open_cfg->session_id.internal.fw_session_id);
		return 0;
	}

	scparms.ctrl_chan_name = parms->open_cfg->ctrl_chan_name;
	scparms.session_client_id = &parms->open_cfg->session_client_id;

	/* Create the new client and get it associated with
	 * the session.
	 */
	rc = tf_session_client_create(tfp, &scparms);
	if (rc) {
		netdev_dbg(tfp->bp->dev,
			   "Failed to create client on session 0x%x, rc:%d\n",
			   parms->open_cfg->session_id.id, rc);
		return rc;
	}

	netdev_dbg(tfp->bp->dev,
		   "Session Client:%d registered on session:0x%8x\n",
		   scparms.session_client_id->internal.fw_session_client_id,
		   tfp->session->session_id.id);

	return 0;
}

int tf_session_attach_session(struct tf *tfp,
			      struct tf_session_attach_session_parms *parms)
{
	int rc = -EOPNOTSUPP;

	if (!tfp || !parms)
		return -EINVAL;

	netdev_dbg(tfp->bp->dev, "Attach not yet supported, rc:%d\n", rc);
	return rc;
}

int tf_session_close_session(struct tf *tfp,
			     struct tf_session_close_session_parms *parms)
{
	struct tf_session_client_destroy_parms scdparms;
	struct tf_session_client *client;
	struct tf_dev_info *tfd = NULL;
	struct tf_session *tfs = NULL;
	u8 fw_session_id = 1;
	u16 fid;
	int rc;

	if (!tfp || !parms || !tfp->session)
		return -EINVAL;

	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "Session lookup failed, rc:%d\n",
			   rc);
		return rc;
	}

	if (tfs->session_id.id == TF_SESSION_ID_INVALID) {
		rc = -EINVAL;
		netdev_dbg(tfp->bp->dev,
			   "Invalid session id, unable to close, rc:%d\n",
			   rc);
		return rc;
	}

	/* Get the client, we need it independently of the closure
	 * type (client or session closure).
	 *
	 * We find the client by way of the fid. Thus one cannot close
	 * a client on behalf of someone else.
	 */
	rc = tfp_get_fid(tfp, &fid);
	if (rc)
		return rc;

	client = tf_session_find_session_client_by_fid(tfs,
						       fid);
	if (!client) {
		rc = -EINVAL;
		netdev_dbg(tfp->bp->dev,
			   "%s: Client not part of session, rc:%d\n",
			   __func__, rc);
		return rc;
	}

	/* In case multiple clients we chose to close those first */
	if (tfs->ref_count > 1) {
		/* Linaro gcc can't static init this structure */
		memset(&scdparms,
		       0,
		       sizeof(struct tf_session_client_destroy_parms));

		scdparms.session_client_id = client->session_client_id;
		/* Destroy requested client so its no longer
		 * registered with this session.
		 */
		rc = tf_session_client_destroy(tfp, &scdparms);
		if (rc) {
			netdev_dbg(tfp->bp->dev,
				   "Failed to unregister Client %d, rc:%d\n",
				   client->session_client_id.id, rc);
			return rc;
		}

		netdev_dbg(tfp->bp->dev,
			   "Closed session client, session_client_id:%d\n",
			   client->session_client_id.id);

		netdev_dbg(tfp->bp->dev, "session_id:0x%08x, ref_count:%d\n",
			   tfs->session_id.id, tfs->ref_count);

		return 0;
	}

	/* Record the session we're closing so the caller knows the
	 * details.
	 */
	*parms->session_id = tfs->session_id;

	rc = tf_session_get_device(tfs, &tfd);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "Device lookup failed, rc:%d\n", rc);
		return rc;
	}

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "Unable to lookup FW id, rc:%d\n",
			   rc);
		return rc;
	}

	/* Unbind the device */
	rc = tf_dev_unbind(tfp, tfd);
	if (rc) {
		/* Log error */
		netdev_dbg(tfp->bp->dev, "Device unbind failed, rc:%d\n", rc);
	}

	rc = tf_msg_session_close(tfp, fw_session_id);
	if (rc) {
		/* Log error */
		netdev_dbg(tfp->bp->dev, "FW Session close failed, rc:%d\n",
			   rc);
	}

	/* Final cleanup as we're last user of the session thus we
	 * also delete the last client.
	 */
	list_del(&client->ll_entry);
	vfree(client);

	tfs->ref_count--;

	netdev_dbg(tfp->bp->dev,
		   "Closed session, session_id:0x%08x, ref_count:%d\n",
		   tfs->session_id.id, tfs->ref_count);

	tfs->dev_init = false;

	vfree(tfp->session->core_data);
	vfree(tfp->session);
	tfp->session = NULL;

	return 0;
}

bool tf_session_is_fid_supported(struct tf_session *tfs, u16 fid)
{
	struct tf_session_client *client;
	struct list_head *c_entry;

	list_for_each(c_entry, &tfs->client_ll) {
		client = list_entry(c_entry, struct tf_session_client,
				    ll_entry);
		if (client->fw_fid == fid)
			return true;
	}

	return false;
}

int tf_session_get_session_internal(struct tf *tfp, struct tf_session **tfs)
{
	/* Skip if device is unsupported */
	if (!BNXT_CHIP_P4(tfp->bp) && !BNXT_CHIP_P5(tfp->bp)) {
		netdev_dbg(tfp->bp->dev, "Unsupported device type\n");
		return -EOPNOTSUPP;
	}

	/* Skip using the check macro as we want to control the error msg */
	if (!tfp->session || !tfp->session->core_data) {
		netdev_dbg(tfp->bp->dev, "Session not created\n");
		return -EINVAL;
	}

	*tfs = (struct tf_session *)(tfp->session->core_data);

	return 0;
}

int tf_session_get_session(struct tf *tfp, struct tf_session **tfs)
{
	bool supported = false;
	u16 fw_fid;
	int rc;

	rc = tf_session_get_session_internal(tfp,
					     tfs);
	/* Logging done by tf_session_get_session_internal */
	if (rc)
		return rc;

	/* As session sharing among functions aka 'individual clients'
	 * is supported we have to assure that the client is indeed
	 * registered before we get deep in the TruFlow api stack.
	 */
	rc = tfp_get_fid(tfp, &fw_fid);
	if (rc) {
		netdev_dbg(tfp->bp->dev, "Internal FID lookup\n, rc:%d\n", rc);
		return rc;
	}

	supported = tf_session_is_fid_supported(*tfs, fw_fid);
	if (!supported) {
		netdev_dbg(tfp->bp->dev,
			   "Ctrl channel not registered\n, rc:%d\n", rc);
		return -EINVAL;
	}

	return rc;
}

int tf_session_get(struct tf *tfp, struct tf_session **tfs,
		   struct tf_dev_info **tfd)
{
	int rc;

	rc = tf_session_get_session_internal(tfp, tfs);
	/* Logging done by tf_session_get_session_internal */
	if (rc)
		return rc;

	rc = tf_session_get_device(*tfs, tfd);
	return rc;
}

struct tf_session_client *
tf_session_get_session_client(struct tf_session *tfs,
			      union tf_session_client_id session_client_id)
{
	struct tf_session_client *client;
	struct list_head *c_entry;

	/* Skip using the check macro as we just want to return */
	if (!tfs)
		return NULL;

	list_for_each(c_entry, &tfs->client_ll) {
		client = list_entry(c_entry, struct tf_session_client,
				    ll_entry);
		if (client->session_client_id.id == session_client_id.id)
			return client;
	}

	return NULL;
}

struct tf_session_client *
tf_session_find_session_client_by_name(struct tf_session *tfs,
				       const char *ctrl_chan_name)
{
	struct tf_session_client *client;
	struct list_head *c_entry;

	/* Skip using the check macro as we just want to return */
	if (!tfs || !ctrl_chan_name)
		return NULL;

	list_for_each(c_entry, &tfs->client_ll) {
		client = list_entry(c_entry, struct tf_session_client,
				    ll_entry);
		if (strncmp(client->ctrl_chan_name,
			    ctrl_chan_name,
			    TF_SESSION_NAME_MAX) == 0)
			return client;
	}

	return NULL;
}

struct tf_session_client *
tf_session_find_session_client_by_fid(struct tf_session *tfs, u16 fid)
{
	struct tf_session_client *client;
	struct list_head *c_entry;

	/* Skip using the check macro as we just want to return */
	if (!tfs)
		return NULL;

	list_for_each(c_entry, &tfs->client_ll) {
		client = list_entry(c_entry, struct tf_session_client,
				    ll_entry);
		if (client->fw_fid == fid)
			return client;
	}

	return NULL;
}

int tf_session_get_device(struct tf_session *tfs, struct tf_dev_info **tfd)
{
	*tfd = &tfs->dev;

	return 0;
}

int
tf_session_get_fw_session_id(struct tf *tfp, uint8_t *fw_session_id)
{
	struct tf_session *tfs = NULL;
	int rc;

	/* Skip using the check macro as we want to control the error msg */
	if (!tfp->session) {
		rc = -EINVAL;
		netdev_dbg(tfp->bp->dev, "Session not created, rc:%d\n", rc);
		return rc;
	}

	if (!fw_session_id) {
		rc = -EINVAL;
		netdev_dbg(tfp->bp->dev, "Invalid Argument(s), rc:%d\n", rc);
		return rc;
	}

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	*fw_session_id = tfs->session_id.internal.fw_session_id;

	return 0;
}

int tf_session_get_session_id(struct tf *tfp, union tf_session_id *session_id)
{
	struct tf_session *tfs = NULL;
	int rc;

	if (!tfp->session) {
		rc = -EINVAL;
		netdev_dbg(tfp->bp->dev, "Session not created, rc:%d\n", rc);
		return rc;
	}

	if (!session_id) {
		rc = -EINVAL;
		netdev_dbg(tfp->bp->dev, "Invalid Argument(s), rc:%d\n", rc);
		return rc;
	}

	/* Using internal version as session client may not exist yet */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	*session_id = tfs->session_id;

	return 0;
}

int tf_session_get_db(struct tf *tfp, enum tf_module_type type,
		      void **db_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	*db_handle = NULL;

	if (!tfp)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	switch (type) {
	case TF_MODULE_TYPE_IDENTIFIER:
		if (tfs->id_db_handle)
			*db_handle = tfs->id_db_handle;
		else
			rc = -ENOMEM;
		break;
	case TF_MODULE_TYPE_TABLE:
		if (tfs->tbl_db_handle)
			*db_handle = tfs->tbl_db_handle;
		else
			rc = -ENOMEM;

		break;
	case TF_MODULE_TYPE_TCAM:
		if (tfs->tcam_db_handle)
			*db_handle = tfs->tcam_db_handle;
		else
			rc = -ENOMEM;
		break;
	case TF_MODULE_TYPE_EM:
		if (tfs->em_db_handle)
			*db_handle = tfs->em_db_handle;
		else
			rc = -ENOMEM;
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

int tf_session_set_db(struct tf *tfp, enum tf_module_type type,
		      void *db_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	if (!tfp)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	switch (type) {
	case TF_MODULE_TYPE_IDENTIFIER:
		tfs->id_db_handle = db_handle;
		break;
	case TF_MODULE_TYPE_TABLE:
		tfs->tbl_db_handle = db_handle;
		break;
	case TF_MODULE_TYPE_TCAM:
		tfs->tcam_db_handle = db_handle;
		break;
	case TF_MODULE_TYPE_EM:
		tfs->em_db_handle = db_handle;
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

int tf_session_get_global_db(struct tf *tfp, void **global_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	*global_handle = NULL;

	if (!tfp)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	*global_handle = tfs->global_db_handle;
	return rc;
}

int tf_session_set_global_db(struct tf *tfp, void *global_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	if (!tfp)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tfs->global_db_handle = global_handle;
	return rc;
}

int tf_session_get_sram_db(struct tf *tfp, void **sram_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	*sram_handle = NULL;

	if (!tfp)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	*sram_handle = tfs->sram_handle;
	return rc;
}

int tf_session_set_sram_db(struct tf *tfp, void *sram_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	if (!tfp)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tfs->sram_handle = sram_handle;
	return rc;
}

int tf_session_get_if_tbl_db(struct tf *tfp, void **if_tbl_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	*if_tbl_handle = NULL;

	if (!tfp)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	*if_tbl_handle = tfs->if_tbl_db_handle;
	return rc;
}

int tf_session_set_if_tbl_db(struct tf *tfp, void *if_tbl_handle)
{
	struct tf_session *tfs = NULL;
	int rc = 0;

	if (!tfp)
		return (-EINVAL);

	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	tfs->if_tbl_db_handle = if_tbl_handle;
	return rc;
}
