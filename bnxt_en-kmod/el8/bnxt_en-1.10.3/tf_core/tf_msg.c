// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */
#include <linux/types.h>
#include "tf_msg.h"
#include "tf_util.h"
#include "bnxt_hwrm.h"

#define HWRM_TF_SESSION_OPEN_OUTPUT_FLAGS_SHARED_SESSION_CREATOR	\
	TF_SESSION_OPEN_RESP_FLAGS_SHARED_SESSION_CREATOR

#define HWRM_TF_SESSION_RESC_QCAPS_OUTPUT_FLAGS_SESS_RESV_STRATEGY_MASK	\
	TF_SESSION_RESC_QCAPS_RESP_FLAGS_SESS_RESV_STRATEGY_MASK

#define HWRM_TF_EM_INSERT_INPUT_FLAGS_DIR_TX	\
	TF_EM_INSERT_REQ_FLAGS_DIR_TX

#define HWRM_TF_EM_INSERT_INPUT_FLAGS_DIR_RX	\
	TF_EM_INSERT_REQ_FLAGS_DIR_RX

#define HWRM_TF_EM_DELETE_INPUT_FLAGS_DIR_TX	\
	TF_EM_DELETE_REQ_FLAGS_DIR_TX

#define HWRM_TF_EM_DELETE_INPUT_FLAGS_DIR_RX	\
	TF_EM_DELETE_REQ_FLAGS_DIR_RX

#define HWRM_TF_TCAM_SET_INPUT_FLAGS_DIR_TX	\
	TF_TCAM_SET_REQ_FLAGS_DIR_TX

#define HWRM_TF_TCAM_SET_INPUT_FLAGS_DMA	\
	TF_TCAM_SET_REQ_FLAGS_DMA

#define HWRM_TF_TCAM_GET_INPUT_FLAGS_DIR_TX	\
	TF_TCAM_GET_REQ_FLAGS_DIR_TX

#define HWRM_TF_TCAM_FREE_INPUT_FLAGS_DIR_TX	\
	TF_TCAM_FREE_REQ_FLAGS_DIR_TX

#define HWRM_TF_GLOBAL_CFG_GET_INPUT_FLAGS_DIR_TX	\
	TF_GLOBAL_CFG_GET_REQ_FLAGS_DIR_TX

#define HWRM_TF_GLOBAL_CFG_GET_INPUT_FLAGS_DIR_RX	\
	TF_GLOBAL_CFG_GET_REQ_FLAGS_DIR_RX

#define HWRM_TF_GLOBAL_CFG_SET_INPUT_FLAGS_DIR_TX	\
	TF_GLOBAL_CFG_SET_REQ_FLAGS_DIR_TX

#define HWRM_TF_GLOBAL_CFG_SET_INPUT_FLAGS_DIR_RX	\
	TF_GLOBAL_CFG_SET_REQ_FLAGS_DIR_RX
#define HWRM_TF_IF_TBL_GET_INPUT_FLAGS_DIR_TX	\
	TF_IF_TBL_GET_REQ_FLAGS_DIR_TX

#define HWRM_TF_IF_TBL_GET_INPUT_FLAGS_DIR_RX	\
	TF_IF_TBL_GET_REQ_FLAGS_DIR_RX

#define HWRM_TF_IF_TBL_SET_INPUT_FLAGS_DIR_TX	\
	TF_IF_TBL_SET_REQ_FLAGS_DIR_TX

#define HWRM_TF_IF_TBL_SET_INPUT_FLAGS_DIR_RX	\
	TF_IF_TBL_SET_REQ_FLAGS_DIR_RX

/* Specific msg size defines as we cannot use defines in tf.yaml. This
 * means we have to manually sync hwrm with these defines if the
 * tf.yaml changes.
 */
#define TF_MSG_SET_GLOBAL_CFG_DATA_SIZE  8
#define TF_MSG_EM_INSERT_KEY_SIZE        64
#define TF_MSG_EM_INSERT_RECORD_SIZE     96
#define TF_MSG_TBL_TYPE_SET_DATA_SIZE    88

/* Compile check - Catch any msg changes that we depend on, like the
 * defines listed above for array size checking.
 *
 * Checking array size is dangerous in that the type could change and
 * we wouldn't be able to catch it. Thus we check if the complete msg
 * changed instead. Best we can do.
 *
 * If failure is observed then both msg size (defines below) and the
 * array size (define above) should be checked and compared.
 */
#define TF_MSG_SIZE_HWRM_TF_GLOBAL_CFG_SET 56
#define TF_MSG_SIZE_HWRM_TF_EM_INSERT      104
#define TF_MSG_SIZE_HWRM_TF_TBL_TYPE_SET   128

/* This is the MAX data we can transport across regular HWRM */
#define TF_PCI_BUF_SIZE_MAX 88

/* This is the length of shared session name "tf_share" */
#define TF_SHARED_SESSION_NAME_LEN 9

/* Max uint8_t value */
#define UINT8_MAX 0xff

/* If data bigger than TF_PCI_BUF_SIZE_MAX then use DMA method */
struct tf_msg_dma_buf {
	void		*va_addr;
	dma_addr_t	pa_addr;
};

int tf_msg_session_open(struct bnxt *bp, char *ctrl_chan_name,
			u8 *fw_session_id, u8 *fw_session_client_id,
			bool *shared_session_creator)
{
	struct hwrm_tf_session_open_output *resp;
	struct hwrm_tf_session_open_input *req;
	char *session_name;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_TF_SESSION_OPEN);
	if (rc)
		return rc;

	session_name = strstr(ctrl_chan_name, "tf_shared");
	if (session_name)
		memcpy(&req->session_name, session_name,
		       TF_SHARED_SESSION_NAME_LEN);
	else
		memcpy(&req->session_name, ctrl_chan_name,
		       TF_SESSION_NAME_MAX);

	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (rc) {
		hwrm_req_drop(bp, req);
		return rc;
	}

	if ((le32_to_cpu(resp->fw_session_id) > UINT8_MAX) ||
	    (le32_to_cpu(resp->fw_session_client_id) > UINT8_MAX)) {
		hwrm_req_drop(bp, req);
		return -EINVAL;
	}
	*fw_session_id = (u8)le32_to_cpu(resp->fw_session_id);
	*fw_session_client_id =
		(u8)le32_to_cpu(resp->fw_session_client_id);
	*shared_session_creator =
		(bool)le32_to_cpu(resp->flags &
				  TF_SESSION_OPEN_RESP_FLAGS_SHARED_SESSION_CREATOR);

	netdev_dbg(bp->dev,
		   "fw_session_id: 0x%x, fw_session_client_id: 0x%x\n",
		   *fw_session_id, *fw_session_client_id);

	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_session_client_register(struct tf *tfp, char *ctrl_channel_name,
				   u8 fw_session_id, u8 *fw_session_client_id)
{
	struct hwrm_tf_session_register_output *resp;
	struct hwrm_tf_session_register_input *req;
	struct bnxt *bp = tfp->bp;
	char *session_name;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_TF_SESSION_REGISTER);
	if (rc)
		return rc;

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);

	session_name = strstr(ctrl_channel_name, "tf_shared");
	if (session_name)
		memcpy(&req->session_client_name, session_name,
		       TF_SHARED_SESSION_NAME_LEN);
	else
		memcpy(&req->session_client_name, ctrl_channel_name,
		       TF_SESSION_NAME_MAX);

	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (rc) {
		hwrm_req_drop(bp, req);
		return rc;
	}

	if (le32_to_cpu(resp->fw_session_client_id) > UINT8_MAX) {
		hwrm_req_drop(bp, req);
		return -EINVAL;
	}
	*fw_session_client_id =
		(u8)le32_to_cpu(resp->fw_session_client_id);

	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_session_client_unregister(struct tf *tfp, u8 fw_session_id,
				     u8 fw_session_client_id)
{
	struct hwrm_tf_session_unregister_input *req;
	struct bnxt *bp = tfp->bp;
	int rc;

	/* Populate the request */
	rc = hwrm_req_init(bp, req, HWRM_TF_SESSION_UNREGISTER);
	if (rc)
		return rc;

	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->fw_session_client_id = cpu_to_le32(fw_session_client_id);

	rc = hwrm_req_send(bp, req);
	return rc;
}

int tf_msg_session_close(struct tf *tfp, u8 fw_session_id)
{
	struct hwrm_tf_session_close_input *req;
	int rc;

	rc = hwrm_req_init(tfp->bp, req, HWRM_TF_SESSION_CLOSE);
	if (rc)
		return rc;

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);

	rc = hwrm_req_send(tfp->bp, req);
	return rc;
}

int tf_msg_session_qcfg(struct tf *tfp, u8 fw_session_id)
{
	struct hwrm_tf_session_qcfg_input *req;
	int rc;

	rc = hwrm_req_init(tfp->bp, req, HWRM_TF_SESSION_QCFG);
	if (rc)
		return rc;

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);

	rc = hwrm_req_send(tfp->bp, req);
	return rc;
}

int tf_msg_session_resc_qcaps(struct tf *tfp, enum tf_dir dir, u16 size,
			      struct tf_rm_resc_req_entry *query,
			      enum tf_rm_resc_resv_strategy *resv_strategy,
			      u8 *sram_profile)
{
	struct hwrm_tf_session_resc_qcaps_output *resp;
	struct hwrm_tf_session_resc_qcaps_input *req;
	struct tf_msg_dma_buf qcaps_buf = { 0 };
	struct tf_rm_resc_req_entry *data;
	struct bnxt *bp;
	int dma_size;
	int rc;
	int i;

	if (!tfp || !query || !resv_strategy)
		return -EINVAL;

	bp = tfp->bp;

	rc = hwrm_req_init(bp, req, HWRM_TF_SESSION_RESC_QCAPS);
	if (rc)
		return rc;

	resp = hwrm_req_hold(bp, req);
	hwrm_req_alloc_flags(bp, req, GFP_KERNEL | __GFP_ZERO);

	/* Prepare DMA buffer */
	dma_size = size * sizeof(struct tf_rm_resc_req_entry);
	qcaps_buf.va_addr = dma_alloc_coherent(&bp->pdev->dev, dma_size,
					       &qcaps_buf.pa_addr, GFP_KERNEL);
	if (!qcaps_buf.va_addr) {
		rc = -ENOMEM;
		goto cleanup;
	}

	/* Populate the request */
	req->fw_session_id = 0;
	req->flags = cpu_to_le16(dir);
	req->qcaps_size = cpu_to_le16(size);
	req->qcaps_addr = cpu_to_le64(qcaps_buf.pa_addr);

	rc = hwrm_req_send(bp, req);
	if (rc)
		goto cleanup;

	/* Process the response
	 * Should always get expected number of entries
	 */
	if (le32_to_cpu(resp->size) != size) {
		netdev_warn(bp->dev,
			    "%s: QCAPS message size error:%d req %d resp %d\n",
			    tf_dir_2_str(dir), EINVAL, size, resp->size);
	}

	netdev_dbg(bp->dev, "QCAPS Count: %d\n", le32_to_cpu(resp->size));
	netdev_dbg(bp->dev, "\nQCAPS Dir:%s\n", tf_dir_2_str(dir));

	/* Post process the response */
	data = (struct tf_rm_resc_req_entry *)qcaps_buf.va_addr;
	for (i = 0; i < size; i++) {
		query[i].type = le32_to_cpu(data[i].type);
		query[i].min = le16_to_cpu(data[i].min);
		query[i].max = le16_to_cpu(data[i].max);
	}

	*resv_strategy = resp->flags &
	      HWRM_TF_SESSION_RESC_QCAPS_OUTPUT_FLAGS_SESS_RESV_STRATEGY_MASK;

	if (sram_profile)
		*sram_profile = resp->sram_profile;

cleanup:
	if (qcaps_buf.va_addr)
		dma_free_coherent(&bp->pdev->dev, dma_size,
				  qcaps_buf.va_addr, qcaps_buf.pa_addr);
	hwrm_req_drop(bp, req);

	if (!rc) {
		netdev_dbg(bp->dev, "%s: dir:%s Success\n", __func__,
			   tf_dir_2_str(dir));
	} else {
		netdev_dbg(bp->dev, "%s: dir:%s Failure\n", __func__,
			   tf_dir_2_str(dir));
	}
	return rc;
}

int tf_msg_session_resc_alloc(struct tf *tfp, enum tf_dir dir, u16 size,
			      struct tf_rm_resc_req_entry *request,
			      u8 fw_session_id, struct tf_rm_resc_entry *resv)
{
	struct hwrm_tf_session_resc_alloc_output *resp = NULL;
	struct hwrm_tf_session_resc_alloc_input *req = NULL;
	struct tf_msg_dma_buf resv_buf = { 0 };
	struct tf_msg_dma_buf req_buf = { 0 };
	struct tf_rm_resc_req_entry *req_data;
	struct tf_rm_resc_entry *resv_data;
	int dma_size_1 = 0;
	int dma_size_2 = 0;
	struct bnxt *bp;
	int rc;
	int i;

	if (!tfp || !request || !resv)
		return -EINVAL;

	bp = tfp->bp;

	rc = hwrm_req_init(bp, req, HWRM_TF_SESSION_RESC_ALLOC);
	if (rc)
		return rc;

	resp = hwrm_req_hold(bp, req);
	hwrm_req_alloc_flags(bp, req, GFP_KERNEL | __GFP_ZERO);

	/* Prepare DMA buffers */
	dma_size_1 = size * sizeof(struct tf_rm_resc_req_entry);
	req_buf.va_addr = dma_alloc_coherent(&bp->pdev->dev, dma_size_1,
					     &req_buf.pa_addr, GFP_KERNEL);
	if (!req_buf.va_addr) {
		rc = -ENOMEM;
		goto cleanup;
	}

	dma_size_2 = size * sizeof(struct tf_rm_resc_entry);
	resv_buf.va_addr = dma_alloc_coherent(&bp->pdev->dev, dma_size_2,
					      &resv_buf.pa_addr, GFP_KERNEL);
	if (!resv_buf.va_addr) {
		rc = -ENOMEM;
		goto cleanup;
	}

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->flags = cpu_to_le16(dir);
	req->req_size = cpu_to_le16(size);

	req_data = (struct tf_rm_resc_req_entry *)req_buf.va_addr;
	for (i = 0; i < size; i++) {
		req_data[i].type = cpu_to_le32(request[i].type);
		req_data[i].min = cpu_to_le16(request[i].min);
		req_data[i].max = cpu_to_le16(request[i].max);
	}

	req->req_addr = cpu_to_le64(req_buf.pa_addr);
	req->resc_addr = cpu_to_le64(resv_buf.pa_addr);

	rc = hwrm_req_send(bp, req);
	if (rc)
		goto cleanup;

	/* Process the response
	 * Should always get expected number of entries
	 */
	if (le32_to_cpu(resp->size) != size) {
		netdev_dbg(bp->dev, "%s: Alloc message size error, rc:%d\n",
			   tf_dir_2_str(dir), EINVAL);
		rc = -EINVAL;
		goto cleanup;
	}

	netdev_dbg(bp->dev, "\nRESV: %s\n", tf_dir_2_str(dir));
	netdev_dbg(bp->dev, "size: %d\n", le32_to_cpu(resp->size));

	/* Post process the response */
	resv_data = (struct tf_rm_resc_entry *)resv_buf.va_addr;
	for (i = 0; i < size; i++) {
		resv[i].type = le32_to_cpu(resv_data[i].type);
		resv[i].start = le16_to_cpu(resv_data[i].start);
		resv[i].stride = le16_to_cpu(resv_data[i].stride);
	}

cleanup:
	if (req_buf.va_addr)
		dma_free_coherent(&bp->pdev->dev, dma_size_1,
				  req_buf.va_addr, req_buf.pa_addr);
	if (resv_buf.va_addr)
		dma_free_coherent(&bp->pdev->dev, dma_size_2,
				  resv_buf.va_addr, resv_buf.pa_addr);
	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_session_resc_info(struct tf *tfp, enum tf_dir dir, u16 size,
			     struct tf_rm_resc_req_entry *request,
			     u8 fw_session_id, struct tf_rm_resc_entry *resv)
{
	struct hwrm_tf_session_resc_info_output *resp;
	struct hwrm_tf_session_resc_info_input *req;
	struct tf_msg_dma_buf resv_buf = { 0 };
	struct tf_msg_dma_buf req_buf = { 0 };
	struct tf_rm_resc_req_entry *req_data;
	struct tf_rm_resc_entry *resv_data;
	int dma_size_1 = 0;
	int dma_size_2 = 0;
	struct bnxt *bp;
	int rc;
	int i;

	if (!tfp || !request || !resv)
		return -EINVAL;

	bp = tfp->bp;

	rc = hwrm_req_init(bp, req, HWRM_TF_SESSION_RESC_INFO);
	if (rc)
		return rc;

	resp = hwrm_req_hold(bp, req);
	hwrm_req_alloc_flags(bp, req, GFP_KERNEL | __GFP_ZERO);

	/* Prepare DMA buffers */
	dma_size_1 = size * sizeof(struct tf_rm_resc_req_entry);
	req_buf.va_addr = dma_alloc_coherent(&bp->pdev->dev, dma_size_1,
					     &req_buf.pa_addr, GFP_KERNEL);
	if (!req_buf.va_addr) {
		rc = -ENOMEM;
		goto cleanup;
	}

	dma_size_2 = size * sizeof(struct tf_rm_resc_entry);
	resv_buf.va_addr = dma_alloc_coherent(&bp->pdev->dev, dma_size_2,
					      &resv_buf.pa_addr, GFP_KERNEL);
	if (!resv_buf.va_addr) {
		rc = -ENOMEM;
		goto cleanup;
	}

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->flags = cpu_to_le16(dir);
	req->req_size = cpu_to_le16(size);

	req_data = (struct tf_rm_resc_req_entry *)req_buf.va_addr;
	for (i = 0; i < size; i++) {
		req_data[i].type = cpu_to_le32(request[i].type);
		req_data[i].min = cpu_to_le16(request[i].min);
		req_data[i].max = cpu_to_le16(request[i].max);
	}

	req->req_addr = cpu_to_le64(req_buf.pa_addr);
	req->resc_addr = cpu_to_le64(resv_buf.pa_addr);

	rc = hwrm_req_send(bp, req);
	if (rc)
		goto cleanup;

	/* Process the response
	 * Should always get expected number of entries
	 */
	if (le32_to_cpu(resp->size) != size) {
		netdev_dbg(bp->dev, "%s: Alloc message size error, rc:%d\n",
			   tf_dir_2_str(dir), EINVAL);
		rc = -EINVAL;
		goto cleanup;
	}

	netdev_dbg(bp->dev, "\nRESV: %s\n", tf_dir_2_str(dir));
	netdev_dbg(bp->dev, "size: %d\n", le32_to_cpu(resp->size));

	/* Post process the response */
	resv_data = (struct tf_rm_resc_entry *)resv_buf.va_addr;
	for (i = 0; i < size; i++) {
		resv[i].type = le32_to_cpu(resv_data[i].type);
		resv[i].start = le16_to_cpu(resv_data[i].start);
		resv[i].stride = le16_to_cpu(resv_data[i].stride);
	}

cleanup:
	if (req_buf.va_addr)
		dma_free_coherent(&bp->pdev->dev, dma_size_1,
				  req_buf.va_addr, req_buf.pa_addr);
	if (resv_buf.va_addr)
		dma_free_coherent(&bp->pdev->dev, dma_size_2,
				  resv_buf.va_addr, resv_buf.pa_addr);
	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_session_resc_flush(struct tf *tfp, enum tf_dir dir, u16 size,
			      u8 fw_session_id, struct tf_rm_resc_entry *resv)
{
	struct hwrm_tf_session_resc_flush_input *req;
	struct tf_msg_dma_buf resv_buf = { 0 };
	struct tf_rm_resc_entry *resv_data;
	struct bnxt *bp;
	int dma_size;
	int rc;
	int i;

	if (!tfp || !resv)
		return -EINVAL;

	bp = tfp->bp;

	rc = hwrm_req_init(bp, req, HWRM_TF_SESSION_RESC_FLUSH);
	if (rc)
		return rc;

	hwrm_req_hold(bp, req);

	/* Prepare DMA buffers */
	dma_size = size * sizeof(struct tf_rm_resc_entry);
	resv_buf.va_addr = dma_alloc_coherent(&bp->pdev->dev, dma_size,
					      &resv_buf.pa_addr, GFP_KERNEL);
	if (!resv_buf.va_addr) {
		rc = -ENOMEM;
		goto cleanup;
	}

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->flags = cpu_to_le16(dir);
	req->flush_size = cpu_to_le16(size);

	resv_data = (struct tf_rm_resc_entry *)resv_buf.va_addr;
	for (i = 0; i < size; i++) {
		resv_data[i].type = cpu_to_le32(resv[i].type);
		resv_data[i].start = cpu_to_le16(resv[i].start);
		resv_data[i].stride = cpu_to_le16(resv[i].stride);
	}

	req->flush_addr = cpu_to_le64(resv_buf.pa_addr);
	rc = hwrm_req_send(bp, req);

cleanup:
	if (resv_buf.va_addr)
		dma_free_coherent(&bp->pdev->dev, dma_size,
				  resv_buf.va_addr, resv_buf.pa_addr);
	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_insert_em_internal_entry(struct tf *tfp,
				    struct tf_insert_em_entry_parms *em_parms,
				    u8 fw_session_id, u16 *rptr_index,
				    u8 *rptr_entry, u8 *num_of_entries)
{
	struct tf_em_64b_entry *em_result =
		(struct tf_em_64b_entry *)em_parms->em_record;
	struct hwrm_tf_em_insert_output *resp = NULL;
	struct hwrm_tf_em_insert_input *req = NULL;
	struct bnxt *bp = tfp->bp;
	u8 msg_key_size;
	u16 flags;
	int rc;

	BUILD_BUG_ON_MSG(sizeof(struct hwrm_tf_em_insert_input) !=
			 TF_MSG_SIZE_HWRM_TF_EM_INSERT,
			 "HWRM message size changed: hwrm_tf_em_insert_input");

	rc = hwrm_req_init(bp, req, HWRM_TF_EM_INSERT);
	if (rc)
		return rc;
	resp = hwrm_req_hold(bp, req);

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);

	/* Check for key size conformity */
	msg_key_size = (em_parms->key_sz_in_bits + 7) / 8;
	if (msg_key_size > TF_MSG_EM_INSERT_KEY_SIZE) {
		rc = -EINVAL;
		netdev_dbg(bp->dev,
			   "%s: Invalid parameters for msg type, rc:%d\n",
			   tf_dir_2_str(em_parms->dir), rc);
		goto cleanup;
	}

	memcpy(req->em_key, em_parms->key, msg_key_size);

	flags = (em_parms->dir == TF_DIR_TX ?
		 HWRM_TF_EM_INSERT_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_EM_INSERT_INPUT_FLAGS_DIR_RX);
	req->flags = cpu_to_le16(flags);
	req->strength = (cpu_to_le16(em_result->hdr.word1) &
			CFA_P4_EEM_ENTRY_STRENGTH_MASK) >>
			CFA_P4_EEM_ENTRY_STRENGTH_SHIFT;
	req->em_key_bitlen = cpu_to_le16(em_parms->key_sz_in_bits);
	req->action_ptr = cpu_to_le32(em_result->hdr.pointer);
	req->em_record_idx = cpu_to_le16(*rptr_index);

	rc = hwrm_req_send(bp, req);
	if (rc)
		goto cleanup;

	*rptr_entry = resp->rptr_entry;
	*rptr_index = le16_to_cpu(resp->rptr_index);
	*num_of_entries = resp->num_of_entries;

cleanup:
	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_hash_insert_em_internal_entry(struct tf *tfp,
					 struct tf_insert_em_entry_parms
					 *em_parms, u32 key0_hash,
					 u32 key1_hash, u8 fw_session_id,
					 u16 *rptr_index, u8 *rptr_entry,
					 u8 *num_of_entries)
{
	struct hwrm_tf_em_hash_insert_output *resp = NULL;
	struct hwrm_tf_em_hash_insert_input *req = NULL;
	struct bnxt *bp = tfp->bp;
	u8 msg_record_size;
	u16 flags;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_TF_EM_HASH_INSERT);
	if (rc)
		return rc;
	resp = hwrm_req_hold(bp, req);

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);

	/* Check for key size conformity */
	msg_record_size = (em_parms->em_record_sz_in_bits + 7) / 8;

	if (msg_record_size > TF_MSG_EM_INSERT_RECORD_SIZE) {
		rc = -EINVAL;
		netdev_dbg(bp->dev, "%s: Record size too large, rc:%d\n",
			   tf_dir_2_str(em_parms->dir), rc);
		goto cleanup;
	}

	memcpy((char *)req->em_record, em_parms->em_record, msg_record_size);

	flags = (em_parms->dir == TF_DIR_TX ?
		 HWRM_TF_EM_INSERT_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_EM_INSERT_INPUT_FLAGS_DIR_RX);
	req->flags = cpu_to_le16(flags);
	req->em_record_size_bits = em_parms->em_record_sz_in_bits;
	req->em_record_idx = *rptr_index;
	req->key0_hash = key0_hash;
	req->key1_hash = key1_hash;

	rc = hwrm_req_send(bp, req);
	if (rc)
		goto cleanup;

	*rptr_entry = resp->rptr_entry;
	*rptr_index = resp->rptr_index;
	*num_of_entries = resp->num_of_entries;

cleanup:
	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_delete_em_entry(struct tf *tfp,
			   struct tf_delete_em_entry_parms *em_parms,
			   u8 fw_session_id)
{
	struct hwrm_tf_em_delete_output *resp = NULL;
	struct hwrm_tf_em_delete_input *req = NULL;
	struct bnxt *bp = tfp->bp;
	u16 flags;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_TF_EM_DELETE);
	if (rc)
		return rc;
	resp = hwrm_req_hold(bp, req);

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);

	flags = (em_parms->dir == TF_DIR_TX ?
		 HWRM_TF_EM_DELETE_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_EM_DELETE_INPUT_FLAGS_DIR_RX);
	req->flags = cpu_to_le16(flags);
	req->flow_handle = cpu_to_le64(em_parms->flow_handle);

	rc = hwrm_req_send(bp, req);
	if (rc)
		goto cleanup;

	em_parms->index = le16_to_cpu(resp->em_index);

cleanup:
	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_move_em_entry(struct tf *tfp,
			 struct tf_move_em_entry_parms *em_parms,
			 u8 fw_session_id)
{
	struct hwrm_tf_em_move_output *resp = NULL;
	struct hwrm_tf_em_move_input *req = NULL;
	struct bnxt *bp = tfp->bp;
	u16 flags;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_TF_EM_MOVE);
	if (rc)
		return rc;
	resp = hwrm_req_hold(bp, req);

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);

	flags = (em_parms->dir == TF_DIR_TX ?
		 HWRM_TF_EM_DELETE_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_EM_DELETE_INPUT_FLAGS_DIR_RX);
	req->flags = cpu_to_le16(flags);
	req->flow_handle = cpu_to_le64(em_parms->flow_handle);
	req->new_index = cpu_to_le32(em_parms->new_index);

	rc = hwrm_req_send(bp, req);
	if (rc)
		goto cleanup;

	em_parms->index = le16_to_cpu(resp->em_index);

cleanup:
	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_tcam_entry_set(struct tf *tfp, struct tf_tcam_set_parms *parms,
			  u8 fw_session_id)
{
	struct hwrm_tf_tcam_set_input *req = NULL;
	struct tf_msg_dma_buf buf = { 0 };
	struct bnxt *bp = tfp->bp;
	int data_size = 0;
	u8 *data = NULL;
	int rc;

	if (!bp)
		return 0;

	if (bp && test_bit(BNXT_STATE_IN_FW_RESET, &bp->state))
		return 0;

	rc = hwrm_req_init(bp, req, HWRM_TF_TCAM_SET);
	if (rc)
		return rc;

	hwrm_req_hold(bp, req);

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->type = parms->hcapi_type;
	req->idx = cpu_to_le16(parms->idx);
	if (parms->dir == TF_DIR_TX)
		req->flags |= HWRM_TF_TCAM_SET_INPUT_FLAGS_DIR_TX;

	req->key_size = parms->key_size;
	req->mask_offset = parms->key_size;

	/* Result follows after key and mask, thus multiply by 2 */
	req->result_offset = 2 * parms->key_size;
	req->result_size = parms->result_size;
	data_size = 2 * req->key_size + req->result_size;

	if (data_size <= TF_PCI_BUF_SIZE_MAX) {
		/* use pci buffer */
		data = &req->dev_data[0];
	} else {
		/* use dma buffer */
		req->flags |= HWRM_TF_TCAM_SET_INPUT_FLAGS_DMA;
		buf.va_addr = dma_alloc_coherent(&bp->pdev->dev, data_size,
						 &buf.pa_addr, GFP_KERNEL);
		if (!buf.va_addr) {
			rc = -ENOMEM;
			goto cleanup;
		}
		data = buf.va_addr;
		memcpy(&req->dev_data[0], &buf.pa_addr, sizeof(buf.pa_addr));
	}

	memcpy(&data[0], parms->key, parms->key_size);
	memcpy(&data[parms->key_size], parms->mask, parms->key_size);
	memcpy(&data[req->result_offset], parms->result, parms->result_size);

	rc = hwrm_req_send(bp, req);

cleanup:
	if (buf.va_addr)
		dma_free_coherent(&bp->pdev->dev, data_size, buf.va_addr,
				  buf.pa_addr);
	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_tcam_entry_get(struct tf *tfp, struct tf_tcam_get_parms *parms,
			  u8 fw_session_id)
{
	struct hwrm_tf_tcam_get_output *resp = NULL;
	struct hwrm_tf_tcam_get_input *req = NULL;
	struct bnxt *bp = tfp->bp;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_TF_TCAM_GET);
	if (rc)
		return rc;
	resp = hwrm_req_hold(bp, req);

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->type = parms->hcapi_type;
	req->idx = cpu_to_le16(parms->idx);
	if (parms->dir == TF_DIR_TX)
		req->flags |= HWRM_TF_TCAM_GET_INPUT_FLAGS_DIR_TX;

	rc = hwrm_req_send(bp, req);
	if (rc)
		goto cleanup;

	if (parms->key_size < resp->key_size ||
	    parms->result_size < resp->result_size) {
		rc = -EINVAL;
		netdev_dbg(bp->dev,
			   "%s: Key buffer(%d) is < the key(%d), rc:%d\n",
			   tf_dir_2_str(parms->dir), parms->key_size,
			   resp->key_size, rc);
		goto cleanup;
	}
	parms->key_size = resp->key_size;
	parms->result_size = resp->result_size;
	memcpy(parms->key, resp->dev_data, resp->key_size);
	memcpy(parms->mask, &resp->dev_data[resp->key_size], resp->key_size);
	memcpy(parms->result, &resp->dev_data[resp->result_offset],
	       resp->result_size);

cleanup:
	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_tcam_entry_free(struct tf *tfp, struct tf_tcam_free_parms *in_parms,
			   u8 fw_session_id)
{
	struct hwrm_tf_tcam_free_input *req = NULL;
	struct bnxt *bp = tfp->bp;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_TF_TCAM_FREE);
	if (rc)
		return rc;

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->type = in_parms->hcapi_type;
	req->count = 1;
	req->idx_list[0] = cpu_to_le16(in_parms->idx);
	if (in_parms->dir == TF_DIR_TX)
		req->flags |= HWRM_TF_TCAM_FREE_INPUT_FLAGS_DIR_TX;

	rc = hwrm_req_send(bp, req);
	return rc;
}

int tf_msg_set_tbl_entry(struct tf *tfp, enum tf_dir dir, u16 hcapi_type,
			 u16 size, u8 *data, u32 index, u8 fw_session_id)
{
	struct hwrm_tf_tbl_type_set_input *req = NULL;
	struct bnxt *bp = tfp->bp;
	int rc;

	BUILD_BUG_ON_MSG(sizeof(struct hwrm_tf_tbl_type_set_input) !=
			 TF_MSG_SIZE_HWRM_TF_TBL_TYPE_SET,
			 "HWRM message size changed: tf_tbl_type_set_input");

	rc = hwrm_req_init(bp, req, HWRM_TF_TBL_TYPE_SET);
	if (rc)
		return rc;

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->flags = cpu_to_le16(dir);
	req->type = cpu_to_le32(hcapi_type);
	req->size = cpu_to_le16(size);
	req->index = cpu_to_le32(index);

	/* Check for data size conformity */
	if (size > TF_MSG_TBL_TYPE_SET_DATA_SIZE) {
		rc = -EINVAL;
		netdev_dbg(bp->dev,
			   "%s: Invalid parameters for msg type, rc:%d\n",
			   tf_dir_2_str(dir), rc);
		return rc;
	}

	memcpy(&req->data, data, size);

	rc = hwrm_req_send(bp, req);
	return rc;
}

int tf_msg_get_tbl_entry(struct tf *tfp, enum tf_dir dir, u16 hcapi_type,
			 u16 size, u8 *data, u32 index, bool clear_on_read,
			 u8 fw_session_id)
{
	struct hwrm_tf_tbl_type_get_output *resp = NULL;
	struct hwrm_tf_tbl_type_get_input *req = NULL;
	struct bnxt *bp = tfp->bp;
	u32 flags = 0;
	int rc;

	flags = (dir == TF_DIR_TX ?
		 TF_TBL_TYPE_GET_REQ_FLAGS_DIR_TX :
		 TF_TBL_TYPE_GET_REQ_FLAGS_DIR_RX);

	if (clear_on_read)
		flags |= TF_TBL_TYPE_GET_REQ_FLAGS_CLEAR_ON_READ;

	rc = hwrm_req_init(bp, req, HWRM_TF_TBL_TYPE_GET);
	if (rc)
		return rc;
	resp = hwrm_req_hold(bp, req);

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->flags = cpu_to_le16(flags);
	req->type = cpu_to_le32(hcapi_type);
	req->index = cpu_to_le32(index);

	rc = hwrm_req_send(bp, req);
	if (rc)
		goto cleanup;

	/* The response will be 64 bytes long, the response size will
	 * be in words (16). All we can test for is that the response
	 * size is < to the requested size.
	 */
	if ((le32_to_cpu(resp->size) * 4) < size) {
		rc = -EINVAL;
		goto cleanup;
	}

	/* Copy the requested number of bytes */
	memcpy(data, &resp->data, size);

cleanup:
	hwrm_req_drop(bp, req);
	return rc;
}

/* HWRM Tunneled messages */
int tf_msg_get_global_cfg(struct tf *tfp, struct tf_global_cfg_parms *params,
			  u8 fw_session_id)
{
	struct hwrm_tf_global_cfg_get_output *resp = NULL;
	struct hwrm_tf_global_cfg_get_input *req = NULL;
	struct bnxt *bp = tfp->bp;
	u16 resp_size = 0;
	u32 flags = 0;
	int rc = 0;

	rc = hwrm_req_init(bp, req, HWRM_TF_GLOBAL_CFG_GET);
	if (rc)
		return rc;
	resp = hwrm_req_hold(bp, req);

	flags = (params->dir == TF_DIR_TX ?
		 HWRM_TF_GLOBAL_CFG_GET_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_GLOBAL_CFG_GET_INPUT_FLAGS_DIR_RX);

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->flags = cpu_to_le32(flags);
	req->type = cpu_to_le32(params->type);
	req->offset = cpu_to_le32(params->offset);
	req->size = cpu_to_le32(params->config_sz_in_bytes);

	rc = hwrm_req_send(bp, req);
	if (rc)
		goto cleanup;

	/* Verify that we got enough buffer to return the requested data */
	resp_size = le16_to_cpu(resp->size);
	if (resp_size < params->config_sz_in_bytes) {
		rc = -EINVAL;
		goto cleanup;
	}

	if (params->config)
		memcpy(params->config, resp->data, resp_size);
	else
		rc = -EFAULT;

cleanup:
	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_set_global_cfg(struct tf *tfp, struct tf_global_cfg_parms *params,
			  u8 fw_session_id)
{
	struct hwrm_tf_global_cfg_set_input *req = NULL;
	struct tf_msg_dma_buf buf = { 0 };
	struct bnxt *bp = tfp->bp;
	int data_size;
	u32 flags;
	u8 *data;
	u8 *mask;
	int rc;
	int i;

	BUILD_BUG_ON_MSG(sizeof(struct hwrm_tf_global_cfg_set_input) !=
			 TF_MSG_SIZE_HWRM_TF_GLOBAL_CFG_SET,
			 "HWRM message size changed: tf_global_cfg_set_input");

	rc = hwrm_req_init(bp, req, HWRM_TF_GLOBAL_CFG_SET);
	if (rc)
		return rc;

	flags = (params->dir == TF_DIR_TX ?
		 HWRM_TF_GLOBAL_CFG_SET_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_GLOBAL_CFG_SET_INPUT_FLAGS_DIR_RX);
	hwrm_req_hold(bp, req);

	data_size = 2 * params->config_sz_in_bytes;	/* data + mask */
	if (data_size <= TF_PCI_BUF_SIZE_MAX) {
		/* use pci buffer */
		data = &req->data[0];
		mask = &req->mask[0];
	} else {
		/* use dma buffer */
		netdev_dbg(bp->dev, "%s: Using dma data\n", __func__);
		flags |= TF_GLOBAL_CFG_SET_REQ_FLAGS_DMA;
		buf.va_addr = dma_alloc_coherent(&bp->pdev->dev, data_size,
						 &buf.pa_addr, GFP_KERNEL);
		if (!buf.va_addr) {
			rc = -ENOMEM;
			goto cleanup;
		}
		data = buf.va_addr;
		mask = data + params->config_sz_in_bytes;

		/* set dma address in the request */
		memcpy(&req->data[0], &buf.pa_addr, sizeof(buf.pa_addr));
	}

	/* copy data and mask to req */
	memcpy(data, params->config, params->config_sz_in_bytes);
	if (!params->config_mask) {
		for (i = 0; i < params->config_sz_in_bytes; i++)
			mask[i] = 0xff;
	} else {
		memcpy(mask, params->config_mask, params->config_sz_in_bytes);
	}
	netdev_dbg(bp->dev, "HWRM_TF_GLOBAL_CFG_SET: data: %*ph\n",
		   params->config_sz_in_bytes, (void *)data);

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->flags = cpu_to_le32(flags);
	req->type = cpu_to_le32(params->type);
	req->offset = cpu_to_le32(params->offset);
	req->size = cpu_to_le32(params->config_sz_in_bytes);

	rc = hwrm_req_send(bp, req);

cleanup:
	if (buf.va_addr)
		dma_free_coherent(&bp->pdev->dev, data_size, buf.va_addr,
				  buf.pa_addr);
	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_bulk_get_tbl_entry(struct tf *tfp, enum tf_dir dir,
			      u16 hcapi_type, u32 starting_idx,
			      u16 num_entries, u16 entry_sz_in_bytes,
			      u64 physical_mem_addr, bool clear_on_read)
{
	/* TBD */
	return -EINVAL;
}

int tf_msg_get_if_tbl_entry(struct tf *tfp,
			    struct tf_if_tbl_get_parms *params,
			    u8 fw_session_id)
{
	struct hwrm_tf_if_tbl_get_output *resp = NULL;
	struct hwrm_tf_if_tbl_get_input *req = NULL;
	struct bnxt *bp = tfp->bp;
	u32 flags = 0;
	int rc = 0;

	rc = hwrm_req_init(bp, req, HWRM_TF_IF_TBL_GET);
	if (rc)
		return rc;
	resp = hwrm_req_hold(bp, req);

	flags = (params->dir == TF_DIR_TX ?
		 HWRM_TF_IF_TBL_GET_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_IF_TBL_GET_INPUT_FLAGS_DIR_RX);

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->flags = flags;
	req->type = params->hcapi_type;
	req->index = cpu_to_le16(params->idx);
	req->size = cpu_to_le16(params->data_sz_in_bytes);

	rc = hwrm_req_send(bp, req);
	if (rc)
		goto cleanup;

	memcpy(&params->data[0], resp->data, req->size);

cleanup:
	hwrm_req_drop(bp, req);
	return rc;
}

int tf_msg_set_if_tbl_entry(struct tf *tfp,
			    struct tf_if_tbl_set_parms *params,
			    u8 fw_session_id)
{
	struct hwrm_tf_if_tbl_set_input *req = NULL;
	struct bnxt *bp = tfp->bp;
	u32 flags = 0;
	int rc = 0;

	rc = hwrm_req_init(bp, req, HWRM_TF_IF_TBL_SET);
	if (rc)
		return rc;

	flags = (params->dir == TF_DIR_TX ?
		 HWRM_TF_IF_TBL_SET_INPUT_FLAGS_DIR_TX :
		 HWRM_TF_IF_TBL_SET_INPUT_FLAGS_DIR_RX);

	/* Populate the request */
	req->fw_session_id = cpu_to_le32(fw_session_id);
	req->flags = flags;
	req->type = params->hcapi_type;
	req->index = cpu_to_le32(params->idx);
	req->size = cpu_to_le32(params->data_sz_in_bytes);
	memcpy(&req->data[0], params->data, params->data_sz_in_bytes);

	rc = hwrm_req_send(bp, req);
	return rc;
}

int
tf_msg_get_version(struct bnxt *bp,
		   struct tf_dev_info *dev,
		   struct tf_get_version_parms *params)

{
	struct hwrm_tf_version_get_output *resp;
	struct hwrm_tf_version_get_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_TF_VERSION_GET);
	if (rc)
		return rc;

	resp = hwrm_req_hold(bp, req);

	rc = hwrm_req_send(bp, req);
	if (rc)
		goto cleanup;

	params->major = resp->major;
	params->minor = resp->minor;
	params->update = resp->update;

	dev->ops->tf_dev_map_hcapi_caps(resp->dev_caps_cfg,
					&params->dev_ident_caps,
					&params->dev_tcam_caps,
					&params->dev_tbl_caps,
					&params->dev_em_caps);

cleanup:
	hwrm_req_drop(bp, req);

	return rc;
}
