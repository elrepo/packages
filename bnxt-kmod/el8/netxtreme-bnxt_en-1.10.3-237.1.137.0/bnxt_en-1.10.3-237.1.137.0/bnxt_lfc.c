/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2017-2018 Broadcom Limited
 * Copyright (c) 2018-2025 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/rtnetlink.h>

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_ulp.h"
#include "bnxt_lfc.h"
#include "bnxt_lfc_ioctl.h"

#ifdef CONFIG_BNXT_LFC

#ifdef HAVE_MODULE_IMPORT_NS_DMA_BUF
MODULE_IMPORT_NS("DMA_BUF");
#endif

#define MAX_LFC_CACHED_NET_DEVICES 37
#define PRIME_1 29
#define PRIME_2 31

static struct bnxt_gloabl_dev blfc_global_dev;
static struct bnxt_lfc_dev_array blfc_array[MAX_LFC_CACHED_NET_DEVICES];

static bool bnxt_lfc_inited;
static bool is_domain_available;
static int domain_no;

static int lfc_device_event(struct notifier_block *unused,
			    unsigned long event, void *ptr)
{
	u32 i;
	struct bnxt_lfc_dev *blfc_dev;
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);

	switch (event) {
	case NETDEV_REGISTER:
		mutex_lock(&blfc_global_dev.bnxt_lfc_lock);
		for (i = 0; i < MAX_LFC_CACHED_NET_DEVICES; i++)
			blfc_array[i].removed = 0;
		mutex_unlock(&blfc_global_dev.bnxt_lfc_lock);
		break;
	case NETDEV_UNREGISTER:
		mutex_lock(&blfc_global_dev.bnxt_lfc_lock);
		for (i = 0; i < MAX_LFC_CACHED_NET_DEVICES; i++) {
			blfc_dev = blfc_array[i].bnxt_lfc_dev;
			if (blfc_dev && blfc_dev->ndev == dev) {
				dev_put(blfc_dev->ndev);
				kfree(blfc_dev);
				blfc_array[i].bnxt_lfc_dev = NULL;
				blfc_array[i].taken = 0;
				blfc_array[i].removed = 1;
				break;
			}
		}
		mutex_unlock(&blfc_global_dev.bnxt_lfc_lock);
		break;
	default:
		/* do nothing */
		break;
	}
	return 0;
}

struct notifier_block lfc_device_notifier = {
     .notifier_call = lfc_device_event
};

static u32 bnxt_lfc_get_hash_key(u32 bus, u32 devfn)
{
	return ((bus * PRIME_1 + devfn) * PRIME_2) % MAX_LFC_CACHED_NET_DEVICES;
}

static int bnxt_lfc_send_hwrm(struct bnxt *bp, struct bnxt_fw_msg *fw_msg)
{
	struct output *resp;
	struct input *req;
	u32 resp_len;
	int rc;

	if (bp->fw_reset_state)
		return -EBUSY;

	rc = hwrm_req_init(bp, req, 0 /* don't care */);
	if (rc)
		return rc;

	rc = hwrm_req_replace(bp, req, fw_msg->msg, fw_msg->msg_len);
	if (rc)
		goto drop_req;

	hwrm_req_timeout(bp, req, fw_msg->timeout);
	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send_silent(bp, req);
	resp_len = le16_to_cpu(resp->resp_len);
	if (resp_len) {
		if (fw_msg->resp_max_len < resp_len)
			resp_len = fw_msg->resp_max_len;

		memcpy(fw_msg->resp, resp, resp_len);
	}
drop_req:
	hwrm_req_drop(bp, req);
	return rc;
}

static bool bnxt_lfc_is_valid_pdev(struct pci_dev *pdev)
{
	int32_t idx;

	if (!pdev) {
		BNXT_LFC_ERR(NULL, "No such PCI device\n");
		return false;
	}

	if (pdev->vendor != PCI_VENDOR_ID_BROADCOM) {
		pci_dev_put(pdev);
		BNXT_LFC_ERR(NULL, "Not a Broadcom PCI device\n");
		return false;
	}

	if (strncmp(dev_driver_string(&pdev->dev), "bnxt_en", 7)) {
		BNXT_LFC_DEBUG(&pdev->dev,
			       "This device is not owned by bnxt_en, instead owned by %s\n",
			       dev_driver_string(&pdev->dev));
		pci_dev_put(pdev);
		return false;
	}

	for (idx = 0; bnxt_pci_tbl[idx].device != 0; idx++) {
		if (pdev->device == bnxt_pci_tbl[idx].device) {
			BNXT_LFC_DEBUG(&pdev->dev, "Found valid PCI device\n");
			return true;
		}
	}
	pci_dev_put(pdev);
	BNXT_LFC_ERR(NULL, "PCI device not supported\n");
	return false;
}

static void bnxt_lfc_init_req_hdr(struct input *req_hdr, u16 req_type,
				u16 cpr_id, u16 tgt_id)
{
	req_hdr->req_type = cpu_to_le16(req_type);
	req_hdr->cmpl_ring = cpu_to_le16(cpr_id);
	req_hdr->target_id = cpu_to_le16(tgt_id);
}

static void bnxt_lfc_prep_fw_msg(struct bnxt_fw_msg *fw_msg, void *msg,
				 int32_t msg_len, void *resp,
				 int32_t resp_max_len,
				 int32_t timeout)
{
	fw_msg->msg = msg;
	fw_msg->msg_len = msg_len;
	fw_msg->resp = resp;
	fw_msg->resp_max_len = resp_max_len;
	fw_msg->timeout = timeout;
}

static int32_t bnxt_lfc_process_nvm_flush(struct bnxt_lfc_dev *blfc_dev)
{
	struct bnxt *bp = blfc_dev->bp;
	int32_t rc = 0;

	struct hwrm_nvm_flush_input req = {0};
	struct hwrm_nvm_flush_output resp = {0};
	struct bnxt_fw_msg fw_msg;

	bnxt_lfc_init_req_hdr((void *)&req,
			      HWRM_NVM_FLUSH, -1, -1);
	bnxt_lfc_prep_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			     sizeof(resp), BNXT_NVM_FLUSH_TIMEOUT);
	rc = bnxt_lfc_send_hwrm(bp, &fw_msg);
	if (rc)
		BNXT_LFC_ERR(&blfc_dev->pdev->dev,
			     "Failed to send NVM_FLUSH FW msg, rc = 0x%x", rc);
	return rc;

}
static int32_t bnxt_lfc_process_nvm_get_var_req(struct bnxt_lfc_dev *blfc_dev,
						struct bnxt_lfc_nvm_get_var_req
						*nvm_get_var_req)
{
	int32_t rc;
	uint16_t len_in_bytes;
	struct pci_dev *pdev = blfc_dev->pdev;
	struct bnxt *bp = blfc_dev->bp;

	struct hwrm_nvm_get_variable_input req = {0};
	struct hwrm_nvm_get_variable_output resp = {0};
	struct bnxt_fw_msg fw_msg;
	void *dest_data_addr = NULL;
	dma_addr_t dest_data_dma_addr;

	if (nvm_get_var_req->len_in_bits == 0) {
		BNXT_LFC_ERR(&blfc_dev->pdev->dev, "Invalid Length\n");
		return -ENOMEM;
	}

	len_in_bytes = (nvm_get_var_req->len_in_bits + 7) / 8;
	dest_data_addr = dma_alloc_coherent(&pdev->dev,
					  len_in_bytes,
					  &dest_data_dma_addr,
					  GFP_KERNEL);

	if (dest_data_addr == NULL) {
		BNXT_LFC_ERR(&blfc_dev->pdev->dev,
			     "Failed to alloc mem for data\n");
		return -ENOMEM;
	}

	bnxt_lfc_init_req_hdr((void *)&req,
			      HWRM_NVM_GET_VARIABLE, -1, -1);
	req.dest_data_addr = cpu_to_le64(dest_data_dma_addr);
	req.data_len = cpu_to_le16(nvm_get_var_req->len_in_bits);
	req.option_num = cpu_to_le16(nvm_get_var_req->option_num);
	req.dimensions = cpu_to_le16(nvm_get_var_req->dimensions);
	req.index_0 = cpu_to_le16(nvm_get_var_req->index_0);
	req.index_1 = cpu_to_le16(nvm_get_var_req->index_1);
	req.index_2 = cpu_to_le16(nvm_get_var_req->index_2);
	req.index_3 = cpu_to_le16(nvm_get_var_req->index_3);

	bnxt_lfc_prep_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			     sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);

	rc = bnxt_lfc_send_hwrm(bp, &fw_msg);
	if (rc) {
		BNXT_LFC_ERR(&blfc_dev->pdev->dev,
			     "Failed to send NVM_GET_VARIABLE FW msg, rc = 0x%x",
			     rc);
		goto done;
	}

	rc = copy_to_user(nvm_get_var_req->out_val, dest_data_addr,
			  len_in_bytes);
	if (rc != 0) {
		BNXT_LFC_ERR(&blfc_dev->pdev->dev,
			     "Failed to send %d characters to the user\n", rc);
		rc = -EFAULT;
	}
done:
	dma_free_coherent(&pdev->dev, (nvm_get_var_req->len_in_bits),
			  dest_data_addr,
			  dest_data_dma_addr);
	return rc;
}

static int32_t bnxt_lfc_process_nvm_set_var_req(struct bnxt_lfc_dev *blfc_dev,
						struct bnxt_lfc_nvm_set_var_req
						*nvm_set_var_req)
{
	int32_t rc;
	uint16_t len_in_bytes;
	struct pci_dev *pdev = blfc_dev->pdev;
	struct bnxt *bp = blfc_dev->bp;

	struct hwrm_nvm_set_variable_input req = {0};
	struct hwrm_nvm_set_variable_output resp = {0};
	struct bnxt_fw_msg fw_msg;
	void *src_data_addr = NULL;
	dma_addr_t src_data_dma_addr;

	if (nvm_set_var_req->len_in_bits == 0) {
		BNXT_LFC_ERR(&blfc_dev->pdev->dev, "Invalid Length\n");
		return -ENOMEM;
	}

	len_in_bytes = (nvm_set_var_req->len_in_bits + 7) / 8;
	src_data_addr = dma_alloc_coherent(&pdev->dev,
					 len_in_bytes,
					 &src_data_dma_addr,
					 GFP_KERNEL);

	if (src_data_addr == NULL) {
		BNXT_LFC_ERR(&blfc_dev->pdev->dev,
			     "Failed to alloc mem for data\n");
		return -ENOMEM;
	}

	rc = copy_from_user(src_data_addr,
			   nvm_set_var_req->in_val,
			   len_in_bytes);

	if (rc != 0) {
		BNXT_LFC_ERR(&blfc_dev->pdev->dev,
			     "Failed to send %d bytes from the user\n", rc);
		rc = -EFAULT;
		goto done;
	}

	bnxt_lfc_init_req_hdr((void *)&req,
			      HWRM_NVM_SET_VARIABLE, -1, -1);
	req.src_data_addr = cpu_to_le64(src_data_dma_addr);
	req.data_len = cpu_to_le16(nvm_set_var_req->len_in_bits);
	req.option_num = cpu_to_le16(nvm_set_var_req->option_num);
	req.dimensions = cpu_to_le16(nvm_set_var_req->dimensions);
	req.index_0 = cpu_to_le16(nvm_set_var_req->index_0);
	req.index_1 = cpu_to_le16(nvm_set_var_req->index_1);
	req.index_2 = cpu_to_le16(nvm_set_var_req->index_2);
	req.index_3 = cpu_to_le16(nvm_set_var_req->index_3);

	bnxt_lfc_prep_fw_msg(&fw_msg, (void *)&req, sizeof(req), (void *)&resp,
			     sizeof(resp), DFLT_HWRM_CMD_TIMEOUT);

	rc = bnxt_lfc_send_hwrm(bp, &fw_msg);
	if (rc)
		BNXT_LFC_ERR(&blfc_dev->pdev->dev,
			     "Failed to send NVM_SET_VARIABLE FW msg, rc = 0x%x", rc);
done:
	dma_free_coherent(&pdev->dev, len_in_bytes,
			  src_data_addr,
			  src_data_dma_addr);
	return rc;
}

static int32_t bnxt_lfc_fill_fw_msg(struct pci_dev *pdev,
				    struct bnxt_fw_msg *fw_msg,
				    struct blfc_fw_msg *msg)
{
	int32_t rc = 0;

	if (copy_from_user(fw_msg->msg,
			   (void __user *)((unsigned long)msg->usr_req),
			   msg->len_req)) {
		BNXT_LFC_ERR(&pdev->dev, "Failed to copy data from user\n");
		return -EFAULT;
	}

	fw_msg->msg_len = msg->len_req;
	fw_msg->resp_max_len = msg->len_resp;
	if (!msg->timeout)
		fw_msg->timeout = DFLT_HWRM_CMD_TIMEOUT;
	else
		fw_msg->timeout = msg->timeout;
	return rc;
}

static int32_t bnxt_lfc_prepare_dma_operations(struct bnxt_lfc_dev *blfc_dev,
					       struct blfc_fw_msg *msg,
					       struct bnxt_fw_msg *fw_msg)
{
	int32_t rc = 0;
	uint8_t i, num_allocated = 0;
	void *dma_ptr;

	for (i = 0; i < msg->num_dma_indications; i++) {
		if (msg->dma[i].length == 0 ||
		    msg->dma[i].length > MAX_DMA_MEM_SIZE) {
			BNXT_LFC_ERR(&blfc_dev->pdev->dev,
				     "Invalid DMA memory length\n");
			rc = -EINVAL;
			goto err;
		}
		blfc_dev->dma_virt_addr[i] = dma_alloc_coherent(
						  &blfc_dev->pdev->dev,
						  msg->dma[i].length,
						  &blfc_dev->dma_addr[i],
						  GFP_KERNEL);
		if (!blfc_dev->dma_virt_addr[i]) {
			BNXT_LFC_ERR(&blfc_dev->pdev->dev,
			       "Failed to allocate memory for data_addr[%d]\n",
			       i);
			rc = -ENOMEM;
			goto err;
		}
		num_allocated++;
		if (!(msg->dma[i].read_or_write)) {
			if (copy_from_user(blfc_dev->dma_virt_addr[i],
					   (void __user *)(
					   (unsigned long)(msg->dma[i].data)),
					   msg->dma[i].length)) {
				BNXT_LFC_ERR(&blfc_dev->pdev->dev,
					     "Failed to copy data from user for data_addr[%d]\n",
					     i);
				rc = -EFAULT;
				goto err;
			}
		}
		dma_ptr = fw_msg->msg + msg->dma[i].offset;

		if ((PTR_ALIGN(dma_ptr, 8) == dma_ptr) &&
		    (msg->dma[i].offset < msg->len_req)) {
			__le64 *dmap = dma_ptr;

			*dmap = cpu_to_le64(blfc_dev->dma_addr[i]);
		} else {
			BNXT_LFC_ERR(&blfc_dev->pdev->dev,
				     "Wrong input parameter\n");
			rc = -EINVAL;
			goto err;
		}
	}
	return rc;
err:
	for (i = 0; i < num_allocated; i++)
		dma_free_coherent(&blfc_dev->pdev->dev,
				  msg->dma[i].length,
				  blfc_dev->dma_virt_addr[i],
				  blfc_dev->dma_addr[i]);
	return rc;
}

static int32_t bnxt_lfc_process_hwrm(struct bnxt_lfc_dev *blfc_dev,
			struct bnxt_lfc_req *lfc_req)
{
	int32_t rc = 0, i, hwrm_err = 0;
	struct bnxt *bp = blfc_dev->bp;
	struct pci_dev *pdev = blfc_dev->pdev;
	struct bnxt_fw_msg fw_msg;
	struct blfc_fw_msg msg, *msg2 = NULL;

	if (copy_from_user(&msg,
			   (void __user *)((unsigned long)lfc_req->req.hreq),
			   sizeof(msg))) {
		BNXT_LFC_ERR(&pdev->dev, "Failed to copy data from user\n");
		return -EFAULT;
	}

	if (msg.len_req > blfc_dev->bp->hwrm_max_ext_req_len ||
	    msg.len_resp > BNXT_LFC_MAX_HWRM_RESP_LENGTH) {
		BNXT_LFC_ERR(&pdev->dev,
			     "Invalid length\n");
		return -EINVAL;
	}

	fw_msg.msg = kmalloc(msg.len_req, GFP_KERNEL);
	if (!fw_msg.msg) {
		BNXT_LFC_ERR(&pdev->dev,
			     "Failed to allocate input req memory\n");
		return -ENOMEM;
	}

	fw_msg.resp = kmalloc(msg.len_resp, GFP_KERNEL);
	if (!fw_msg.resp) {
		BNXT_LFC_ERR(&pdev->dev,
			     "Failed to allocate resp memory\n");
		rc = -ENOMEM;
		goto err;
	}

	rc = bnxt_lfc_fill_fw_msg(pdev, &fw_msg, &msg);
	if (rc) {
		BNXT_LFC_ERR(&pdev->dev,
			     "Failed to fill the FW data\n");
		goto err;
	}

	if (msg.num_dma_indications) {
		if (msg.num_dma_indications > MAX_NUM_DMA_INDICATIONS) {
			BNXT_LFC_ERR(&pdev->dev,
				     "Invalid DMA indications\n");
			rc = -EINVAL;
			goto err1;
		}
		msg2 = kmalloc((sizeof(struct blfc_fw_msg) +
		       (msg.num_dma_indications * sizeof(struct dma_info))),
		       GFP_KERNEL);
		if (!msg2) {
			BNXT_LFC_ERR(&pdev->dev,
				     "Failed to allocate memory\n");
			rc = -ENOMEM;
			goto err1;
		}
		if (copy_from_user((void *)msg2,
				    (void __user *)((unsigned long)lfc_req->req.hreq),
				    (sizeof(struct blfc_fw_msg) +
				    (msg.num_dma_indications *
				    sizeof(struct dma_info))))) {
			BNXT_LFC_ERR(&pdev->dev,
				     "Failed to copy data from user\n");
			rc = -EFAULT;
			goto err1;
		}
		rc = bnxt_lfc_prepare_dma_operations(blfc_dev, msg2, &fw_msg);
		if (rc) {
			BNXT_LFC_ERR(&pdev->dev,
				     "Failed to perform DMA operations\n");
			goto err1;
		}
	}

	hwrm_err = bnxt_lfc_send_hwrm(bp, &fw_msg);
	if (hwrm_err) {
		struct input *req = fw_msg.msg;

		BNXT_LFC_DEBUG(&pdev->dev,
			       "Failed to send FW msg type = 0x%x, error = 0x%x",
			       req->req_type, hwrm_err);
		goto err;
	}

	for (i = 0; i < msg.num_dma_indications; i++) {
		if (msg2->dma[i].read_or_write) {
			if (copy_to_user((void __user *)
					 ((unsigned long)msg2->dma[i].data),
					 blfc_dev->dma_virt_addr[i],
					 msg2->dma[i].length)) {
				BNXT_LFC_ERR(&pdev->dev,
					     "Failed to copy data to user\n");
				rc = -EFAULT;
				goto err;
			}
		}
	}
err:
	for (i = 0; i < msg.num_dma_indications; i++)
		dma_free_coherent(&pdev->dev, msg2->dma[i].length,
				  blfc_dev->dma_virt_addr[i],
				  blfc_dev->dma_addr[i]);

	if (hwrm_err != -EBUSY && hwrm_err != -E2BIG) {
		if (copy_to_user((void __user *)((unsigned long)msg.usr_resp),
				 fw_msg.resp,
				 msg.len_resp)) {
			BNXT_LFC_ERR(&pdev->dev,
				     "Failed to copy data to user\n");
			rc = -EFAULT;
		}
	}
err1:
	kfree(msg2);
	kfree(fw_msg.msg);
	kfree(fw_msg.resp);

	/* If HWRM command fails, return the response error code */
	if (hwrm_err)
		return hwrm_err;
	return rc;
}

static int32_t bnxt_lfc_process_physmem_req(struct bnxt_lfc_dev *blfc_dev, unsigned long arg)
{
	struct pci_dev *pdev = blfc_dev->pdev;
	struct alloc_phys_mem_data data;
	struct bnxt_lfc_req lfc;
	uint64_t addr, bus_addr;
	unsigned long i, size;

	/* Get user data */
	if (copy_from_user(&lfc, (void __user *)arg, sizeof(struct bnxt_lfc_req))) {
		BNXT_LFC_ERR(&blfc_dev->pdev->dev, "lfc_req copy_from_user failed\n");
		return -EFAULT;
	}

	data = lfc.req.pdata;
	/* Allocate phys contigious memory */
	addr = (uint64_t)dma_alloc_coherent(&pdev->dev, data.size, &bus_addr, GFP_KERNEL);
	if (unlikely(!addr))
		return -ENOMEM;

	BNXT_LFC_DEBUG(&blfc_dev->pdev->dev, "virt addr is %p, bus addr is %p\n",
		       (void *)addr, (void *)bus_addr);

	/* Translate kernel logical address to physical address */
	addr = virt_to_phys((void *)addr);
	BNXT_LFC_DEBUG(&blfc_dev->pdev->dev, "phys addr is %p\n", (void *)addr);

	/* Reserve calculated pages as similar to cdrv */
	i = addr;
	size = data.size;
	while (i < addr + size) {
		SetPageReserved(virt_to_page(phys_to_virt(i)));
		i += PAGE_SIZE;
	}

	/* Copy physical address to user */
	if (copy_to_user((void __user *)((ulong)(data.phys_addr_ptr)),
			 &addr, sizeof(uint64_t))) {
		BNXT_LFC_ERR(&blfc_dev->pdev->dev, "phys_addr_ptr copy_to_user failed\n");
		return -EFAULT;
	}

	/* Copy PCI bus address to user */
	if (copy_to_user((void __user *)((unsigned long)(data.bus_addr_ptr)),
			 &bus_addr, sizeof(uint64_t))) {
		BNXT_LFC_ERR(&blfc_dev->pdev->dev, "bus_addr_ptr copy_to_user failed\n");
		return -EFAULT;
	}

	return 0;
}

static int32_t bnxt_lfc_process_physmem_free_req(struct bnxt_lfc_dev *blfc_dev, unsigned long arg)
{
	struct pci_dev *pdev = blfc_dev->pdev;
	struct free_phys_mem_data data;
	unsigned long i, offset, size;
	struct bnxt_lfc_req lfc;

	/* Get user data */
	if (copy_from_user(&lfc, (void __user *)arg, sizeof(struct bnxt_lfc_req))) {
		BNXT_LFC_ERR(&blfc_dev->pdev->dev, "lfc copy_from_user failed\n");
		return -EFAULT;
	}

	data = lfc.req.pdata_free;

	/* Return the rsvd pages before calling to dma_free_consistent */
	offset = data.phys_addr;
	i = offset;
	size = data.size;

	while (i < offset + size) {
		ClearPageReserved(virt_to_page(phys_to_virt(i)));
		i += PAGE_SIZE;
	}

	/* Free physical memory */
	BNXT_LFC_DEBUG(&blfc_dev->pdev->dev, "phys addr is %p virt addr is %p\n",
		       (void *)offset, phys_to_virt((ulong)(offset)));

	dma_free_coherent(&pdev->dev, size,
			  (void *)phys_to_virt((ulong)(offset)),
			  (dma_addr_t)(data.bus_addr));

	return 0;
}

static int32_t bnxt_lfc_process_req(struct bnxt_lfc_dev *blfc_dev,
			struct bnxt_lfc_req *lfc_req)
{
	int32_t rc;

	switch (lfc_req->hdr.req_type) {
	case BNXT_LFC_NVM_GET_VAR_REQ:
		rc = bnxt_lfc_process_nvm_get_var_req(blfc_dev,
					&lfc_req->req.nvm_get_var_req);
		break;
	case BNXT_LFC_NVM_SET_VAR_REQ:
		rc = bnxt_lfc_process_nvm_set_var_req(blfc_dev,
					&lfc_req->req.nvm_set_var_req);
		break;
	case BNXT_LFC_NVM_FLUSH_REQ:
		rc = bnxt_lfc_process_nvm_flush(blfc_dev);
		break;
	case BNXT_LFC_GENERIC_HWRM_REQ:
		rc = bnxt_lfc_process_hwrm(blfc_dev, lfc_req);
		break;
	default:
		BNXT_LFC_DEBUG(&blfc_dev->pdev->dev,
			       "No valid request found\n");
		return -EINVAL;
	}
	return rc;
}

static int32_t bnxt_lfc_open(struct inode *inode, struct file *flip)
{
	BNXT_LFC_DEBUG(NULL, "open is called");

	return 0;
}

static ssize_t bnxt_lfc_read(struct file *filp, char __user *buff,
			     size_t length, loff_t *offset)
{
	return -EINVAL;
}

static ssize_t bnxt_lfc_write(struct file *filp, const char __user *ubuff,
			      size_t len, loff_t *offset)
{
	struct bnxt_lfc_generic_msg kbuff;

	if (len != sizeof(kbuff)) {
		BNXT_LFC_ERR(NULL, "Invalid length provided (%zu)\n", len);
		return -EINVAL;
	}

	if (copy_from_user(&kbuff, (void __user *)ubuff, len)) {
		BNXT_LFC_ERR(NULL, "Failed to copy data from user application\n");
		return -EFAULT;
	}

	switch (kbuff.key) {
	case BNXT_LFC_KEY_DOMAIN_NO:
		is_domain_available = true;
		domain_no = kbuff.value;
		break;
	default:
		BNXT_LFC_ERR(NULL, "Invalid Key provided (%u)\n", kbuff.key);
		return -EINVAL;
	}
	return len;
}

static loff_t bnxt_lfc_seek(struct file *filp, loff_t offset, int32_t whence)
{
	return -EINVAL;
}

static long bnxt_lfc_ioctl(struct file *flip, unsigned int cmd,
			   unsigned long args)
{
	int32_t rc;
	struct bnxt_lfc_req lfc_req;
	u32 index;
	struct bnxt_lfc_dev *blfc_dev = NULL;

	rc = copy_from_user(&lfc_req, (void __user *)args, sizeof(lfc_req));
	if (rc) {
		BNXT_LFC_ERR(NULL,
			     "Failed to send %d bytes from the user\n", rc);
		return -EINVAL;
	}

	switch (cmd) {
	case BNXT_LFC_REQ:
	case BNXT_LFC_IOCALLOC_PHYS_MEM:
	case BNXT_LFC_IOCFREE_PHYS_MEM:
		break;
	default:
		BNXT_LFC_ERR(NULL, "No Valid IOCTL found\n");
		return -EINVAL;
	}

	/* derive or create blfc_dev */
	mutex_lock(&blfc_global_dev.bnxt_lfc_lock);
	if ((lfc_req.hdr.ver & BNXT_LFC_VER_BITS) > 1)
		domain_no = lfc_req.hdr.ver >> BNXT_LFC_VER_LEN;

	index = bnxt_lfc_get_hash_key(lfc_req.hdr.bus, lfc_req.hdr.devfn);
	if (blfc_array[index].removed) {
		BNXT_LFC_DEBUG(NULL, "PCI device removed\n");
		mutex_unlock(&blfc_global_dev.bnxt_lfc_lock);
		return -ENODEV;
	}
	if (blfc_array[index].taken) {
		if (lfc_req.hdr.devfn != blfc_array[index].bnxt_lfc_dev->devfn ||
		    lfc_req.hdr.bus != blfc_array[index].bnxt_lfc_dev->bus ||
		    domain_no != blfc_array[index].bnxt_lfc_dev->domain) {
			/* we have a false hit. Free the older blfc device
			 * store the new one
			 */
			dev_put(blfc_array[index].bnxt_lfc_dev->ndev);
			kfree(blfc_array[index].bnxt_lfc_dev);
			blfc_array[index].bnxt_lfc_dev = NULL;
			blfc_array[index].taken = 0;
			goto not_taken;
		}
		blfc_dev = blfc_array[index].bnxt_lfc_dev;
		pci_dev_get(blfc_dev->pdev);
	} else {
not_taken:
		blfc_dev = kzalloc(sizeof(*blfc_dev), GFP_KERNEL);
		if (!blfc_dev) {
			mutex_unlock(&blfc_global_dev.bnxt_lfc_lock);
			return -EINVAL;
		}
		blfc_dev->pdev = pci_get_domain_bus_and_slot(((is_domain_available) ?
							      domain_no : 0),
							     lfc_req.hdr.bus,
							     lfc_req.hdr.devfn);

		if (bnxt_lfc_is_valid_pdev(blfc_dev->pdev) != true) {
			mutex_unlock(&blfc_global_dev.bnxt_lfc_lock);
			kfree(blfc_dev);
			return -EINVAL;
		}

		blfc_dev->ndev = pci_get_drvdata(blfc_dev->pdev);
		if (!blfc_dev->ndev) {
			BNXT_LFC_ERR(NULL, "Driver with provided BDF doesn't exist\n");
			pci_dev_put(blfc_dev->pdev);
			mutex_unlock(&blfc_global_dev.bnxt_lfc_lock);
			kfree(blfc_dev);
			return -EINVAL;
		}

		dev_hold(blfc_dev->ndev);
		if (try_module_get(blfc_dev->pdev->driver->driver.owner)) {
			blfc_dev->bp = netdev_priv(blfc_dev->ndev);
			if (!blfc_dev->bp)
				rc = -EINVAL;
			module_put(blfc_dev->pdev->driver->driver.owner);
		} else {
			rc = -EINVAL;
		}

		if (rc) {
			pci_dev_put(blfc_dev->pdev);
			dev_put(blfc_dev->ndev);
			kfree(blfc_dev);
			is_domain_available = false;
			mutex_unlock(&blfc_global_dev.bnxt_lfc_lock);
			return -EINVAL;
		}

		blfc_dev->bus = lfc_req.hdr.bus;
		blfc_dev->devfn = lfc_req.hdr.devfn;
		blfc_dev->domain = domain_no;
		blfc_array[index].bnxt_lfc_dev = blfc_dev;
		blfc_array[index].taken = 1;
		blfc_global_dev.cdiag_index = index;
	}

	switch (cmd) {
	case BNXT_LFC_REQ:
		rc = bnxt_lfc_process_req(blfc_dev, &lfc_req);
		break;
	case BNXT_LFC_IOCALLOC_PHYS_MEM:
		rc = bnxt_lfc_process_physmem_req(blfc_dev, args);
		break;
	case BNXT_LFC_IOCFREE_PHYS_MEM:
		rc = bnxt_lfc_process_physmem_free_req(blfc_dev, args);
		break;
	}

	pci_dev_put(blfc_dev->pdev);
	mutex_unlock(&blfc_global_dev.bnxt_lfc_lock);
	return rc;
}

static int32_t bnxt_lfc_release(struct inode *inode, struct file *filp)
{
	BNXT_LFC_DEBUG(NULL, "release is called");
	return 0;
}

static int bnxt_lfc_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size,
			    vma->vm_page_prot))
		return -EAGAIN;
	return 0;
}

int32_t __init bnxt_lfc_init(void)
{
	int32_t rc;

	rc = alloc_chrdev_region(&blfc_global_dev.d_dev, 0, 1, BNXT_LFC_DEV_NAME);
	if (rc < 0) {
		BNXT_LFC_ERR(NULL, "Allocation of char dev region is failed\n");
		return rc;
	}

	blfc_global_dev.d_class = class_create(THIS_MODULE, BNXT_LFC_DEV_NAME);
	if (IS_ERR(blfc_global_dev.d_class)) {
		BNXT_LFC_ERR(NULL, "Class creation is failed\n");
		unregister_chrdev_region(blfc_global_dev.d_dev, 1);
		return -1;
	}

	if (IS_ERR(device_create(blfc_global_dev.d_class, NULL, blfc_global_dev.d_dev, NULL,
			  BNXT_LFC_DEV_NAME))) {
		BNXT_LFC_ERR(NULL, "Device creation is failed\n");
		class_destroy(blfc_global_dev.d_class);
		unregister_chrdev_region(blfc_global_dev.d_dev, 1);
		return -1;
	}

	blfc_global_dev.fops.owner          = THIS_MODULE;
	blfc_global_dev.fops.open           = bnxt_lfc_open;
	blfc_global_dev.fops.read           = bnxt_lfc_read;
	blfc_global_dev.fops.write          = bnxt_lfc_write;
	blfc_global_dev.fops.llseek         = bnxt_lfc_seek;
	blfc_global_dev.fops.unlocked_ioctl = bnxt_lfc_ioctl;
	blfc_global_dev.fops.release        = bnxt_lfc_release;
	blfc_global_dev.fops.mmap           = bnxt_lfc_mmap;

	cdev_init(&blfc_global_dev.c_dev, &blfc_global_dev.fops);
	if (cdev_add(&blfc_global_dev.c_dev, blfc_global_dev.d_dev, 1) == -1) {
		BNXT_LFC_ERR(NULL, "Char device addition is failed\n");
		device_destroy(blfc_global_dev.d_class, blfc_global_dev.d_dev);
		class_destroy(blfc_global_dev.d_class);
		unregister_chrdev_region(blfc_global_dev.d_dev, 1);
		return -1;
	}
	mutex_init(&blfc_global_dev.bnxt_lfc_lock);
	bnxt_lfc_inited = true;

	memset(blfc_array, 0, sizeof(struct bnxt_lfc_dev_array)
	       * MAX_LFC_CACHED_NET_DEVICES);

	rc = bnxt_en_register_netdevice_notifier(&lfc_device_notifier);
	if (rc) {
		BNXT_LFC_ERR(NULL, "Error on register NETDEV event notifier\n");
		return -1;
	}
	return 0;
}

void bnxt_lfc_exit(void)
{
	if (!bnxt_lfc_inited)
		return;

	bnxt_en_unregister_netdevice_notifier(&lfc_device_notifier);

	cdev_del(&blfc_global_dev.c_dev);
	device_destroy(blfc_global_dev.d_class, blfc_global_dev.d_dev);
	class_destroy(blfc_global_dev.d_class);
	unregister_chrdev_region(blfc_global_dev.d_dev, 1);
}
#endif
