// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Broadcom Corporation
 */

#include "bnxt_fwctl_core_linux_compat.h"
#if !defined(HAVE_FWCTL_DRIVER) && defined(HAVE_FWCTL_CLEANUP)

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include "bnxt_fwctl_core_uapi_compat.h"
#include "bnxt_fwctl_bnxt_uapi_compat.h"

/* FIXME need a include/linux header for the aux related definitions */
#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_ulp.h"

struct bnxtctl_uctx {
	struct fwctl_uctx uctx;
	u32 uctx_caps;
};

struct bnxtctl_dev {
	struct fwctl_device fwctl;
	struct bnxt_aux_priv *aux_priv;
	void *dma_virt_addr[MAX_NUM_DMA_INDICATIONS];
	/* dma_addr to hold the DMA addresses*/
	dma_addr_t dma_addr[MAX_NUM_DMA_INDICATIONS];
};

DEFINE_FREE(bnxtctl, struct bnxtctl_dev *, if (_T) fwctl_put(&_T->fwctl))

static int bnxtctl_open_uctx(struct fwctl_uctx *uctx)
{
	struct bnxtctl_uctx *bnxtctl_uctx =
		container_of(uctx, struct bnxtctl_uctx, uctx);

	bnxtctl_uctx->uctx_caps = BIT(FWCTL_BNXT_QUERY_COMMANDS) |
				  BIT(FWCTL_BNXT_SEND_COMMAND);
	return 0;
}

static void bnxtctl_close_uctx(struct fwctl_uctx *uctx)
{
}

static void *bnxtctl_info(struct fwctl_uctx *uctx, size_t *length)
{
	struct bnxtctl_uctx *bnxtctl_uctx =
		container_of(uctx, struct bnxtctl_uctx, uctx);
	struct fwctl_info_bnxt *info;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return ERR_PTR(-ENOMEM);

	info->uctx_caps = bnxtctl_uctx->uctx_caps;

	*length = sizeof(*info);
	return info;
}

static bool bnxtctl_validate_rpc(struct bnxt_en_dev *edev,
				 struct bnxt_fw_msg *hwrm_in)
{
	struct input *req = (struct input *)hwrm_in->msg;

	mutex_lock(&edev->en_dev_lock);
	if (edev->flags & BNXT_EN_FLAG_ULP_STOPPED) {
		mutex_unlock(&edev->en_dev_lock);
		return false;
	}
	mutex_unlock(&edev->en_dev_lock);

	if (req->req_type <= HWRM_LAST)
		return true;

	return false;
}

static int bnxt_fw_setup_input_dma(struct bnxtctl_dev *bnxt_dev,
				   struct device *dev,
				   int num_dma,
				   struct fwctl_dma_info_bnxt *msg,
				   struct bnxt_fw_msg *fw_msg)
{
	u8 i, num_allocated = 0;
	void *dma_ptr;
	int rc = 0;

	for (i = 0; i < num_dma; i++) {
		if (msg->len == 0 || msg->len > MAX_DMA_MEM_SIZE) {
			rc = -EINVAL;
			goto err;
		}
		bnxt_dev->dma_virt_addr[i] = dma_alloc_coherent(dev->parent,
								msg->len,
								&bnxt_dev->dma_addr[i],
								GFP_KERNEL);
		if (!bnxt_dev->dma_virt_addr[i]) {
			rc = -ENOMEM;
			goto err;
		}
		num_allocated++;
		if (!(msg->read_from_device)) {
			if (copy_from_user(bnxt_dev->dma_virt_addr[i],
					   u64_to_user_ptr(msg->data),
					   msg->len)) {
				rc = -EFAULT;
				goto err;
			}
		}
		dma_ptr = fw_msg->msg + msg->offset;

		if ((PTR_ALIGN(dma_ptr, 8) == dma_ptr) &&
		    (msg->offset < fw_msg->msg_len)) {
			__le64 *dmap = dma_ptr;

			*dmap = cpu_to_le64(bnxt_dev->dma_addr[i]);
		} else {
			rc = -EINVAL;
			goto err;
		}
		msg += 1;
	}
	return rc;
err:
	for (i = 0; i < num_allocated; i++)
		dma_free_coherent(dev->parent,
				  msg->len,
				  bnxt_dev->dma_virt_addr[i],
				  bnxt_dev->dma_addr[i]);
	return rc;
}

static void *bnxtctl_fw_rpc(struct fwctl_uctx *uctx,
			    enum fwctl_rpc_scope scope,
			    void *in, size_t in_len, size_t *out_len)
{
	struct bnxtctl_dev *bnxtctl =
		container_of(uctx->fwctl, struct bnxtctl_dev, fwctl);
	struct bnxt_aux_priv *bnxt_aux_priv = bnxtctl->aux_priv;
	struct fwctl_dma_info_bnxt *dma_buf = NULL;
	struct device *dev = &uctx->fwctl->dev;
	struct fwctl_rpc_bnxt *msg = in;
	struct bnxt_fw_msg rpc_in;
	int i, rc, err = 0;
	int dma_buf_size;

	rpc_in.msg = kzalloc(msg->req_len, GFP_KERNEL);
	if (!rpc_in.msg) {
		err = -ENOMEM;
		goto err_out;
	}
	if (copy_from_user(rpc_in.msg, u64_to_user_ptr(msg->req),
			   msg->req_len)) {
		dev_dbg(dev, "Failed to copy in_payload from user\n");
		err = -EFAULT;
		goto err_out;
	}

	if (!bnxtctl_validate_rpc(bnxt_aux_priv->edev, &rpc_in))
		return ERR_PTR(-EPERM);

	rpc_in.msg_len = msg->req_len;
	rpc_in.resp = kzalloc(*out_len, GFP_KERNEL);
	if (!rpc_in.resp) {
		err = -ENOMEM;
		goto err_out;
	}

	rpc_in.resp_max_len = *out_len;
	if (!msg->timeout)
		rpc_in.timeout = DFLT_HWRM_CMD_TIMEOUT;
	else
		rpc_in.timeout = msg->timeout;

	if (msg->num_dma) {
		if (msg->num_dma > MAX_NUM_DMA_INDICATIONS) {
			dev_err(dev, "Failed to allocate dma buffers\n");
			err = -EINVAL;
			goto err_out;
		}
		dma_buf_size = msg->num_dma * sizeof(*dma_buf);
		dma_buf = kzalloc(dma_buf_size, GFP_KERNEL);
		if (!dma_buf) {
			dev_err(dev, "Failed to allocate dma buffers\n");
			err = -ENOMEM;
			goto err_out;
		}

		if (copy_from_user(dma_buf, u64_to_user_ptr(msg->payload),
				   dma_buf_size)) {
			dev_dbg(dev, "Failed to copy dma payload from user\n");
			err = -EFAULT;
			goto err_out;
		}

		rc = bnxt_fw_setup_input_dma(bnxtctl, dev, msg->num_dma,
					     dma_buf, &rpc_in);
		if (rc) {
			err = -EOPNOTSUPP;
			goto err_out;
		}
	}

	rc = bnxt_send_msg(bnxt_aux_priv->edev, &rpc_in);
	if (rc) {
		struct output *resp = rpc_in.resp;

		/* Copy the response to user always, as it contains
		 * detailed status of the command failure
		 */
		if (!resp->error_code)
			/* bnxt_send_msg() returned much before FW
			 * received the command.
			 */
			resp->error_code = rc;

		goto err_out;
	}

	for (i = 0; i < msg->num_dma; i++) {
		if (dma_buf[i].read_from_device) {
			if (copy_to_user(u64_to_user_ptr(dma_buf[i].data),
					 bnxtctl->dma_virt_addr[i],
					 dma_buf[i].len)) {
				err = -EFAULT;
			}
		}
	}
	for (i = 0; i < msg->num_dma; i++)
		dma_free_coherent(dev->parent, dma_buf[i].len,
				  bnxtctl->dma_virt_addr[i],
				  bnxtctl->dma_addr[i]);

err_out:
	kfree(dma_buf);
	kfree(rpc_in.msg);

	if (err)
		return ERR_PTR(err);

	return rpc_in.resp;
}

static const struct fwctl_ops bnxtctl_ops = {
	.device_type = FWCTL_DEVICE_TYPE_BNXT,
	.uctx_size = sizeof(struct bnxtctl_uctx),
	.open_uctx = bnxtctl_open_uctx,
	.close_uctx = bnxtctl_close_uctx,
	.info = bnxtctl_info,
	.fw_rpc = bnxtctl_fw_rpc,
};

static int bnxtctl_probe(struct auxiliary_device *adev,
			 const struct auxiliary_device_id *id)
{
	struct bnxt_aux_priv *aux_priv =
		container_of(adev, struct bnxt_aux_priv, aux_dev);
	struct bnxtctl_dev *bnxtctl __free(bnxtctl) =
		fwctl_alloc_device(&aux_priv->edev->pdev->dev, &bnxtctl_ops,
				   struct bnxtctl_dev, fwctl);
	int rc;

	if (!bnxtctl)
		return -ENOMEM;

	bnxtctl->aux_priv = aux_priv;

	rc = fwctl_register(&bnxtctl->fwctl);
	if (rc)
		return rc;

	auxiliary_set_drvdata(adev, no_free_ptr(bnxtctl));
	return 0;
}

static void bnxtctl_remove(struct auxiliary_device *adev)
{
	struct bnxtctl_dev *ctldev = auxiliary_get_drvdata(adev);

	fwctl_unregister(&ctldev->fwctl);
	fwctl_put(&ctldev->fwctl);
}

static const struct auxiliary_device_id bnxtctl_id_table[] = {
	{ .name = "bnxt_en.fwctl", },
	{},
};
MODULE_DEVICE_TABLE(auxiliary, bnxtctl_id_table);

static struct auxiliary_driver bnxtctl_driver = {
	.name = "bnxt_fwctl",
	.probe = bnxtctl_probe,
	.remove = bnxtctl_remove,
	.id_table = bnxtctl_id_table,
};

/* module_auxiliary_driver(bnxtctl_driver); */

int __init init_bnxt_fwctl(void)
{
	auxiliary_driver_register(&bnxtctl_driver);
	return 0;
}

void exit_bnxt_fwctl(void)
{
	auxiliary_driver_unregister(&bnxtctl_driver);
}
MODULE_IMPORT_NS("FWCTL");
MODULE_DESCRIPTION("BNXT fwctl driver");
MODULE_AUTHOR("Broadcom Corporation");
MODULE_LICENSE("GPL");
#else
int init_bnxt_fwctl(void)
{
	return 0;
}

void exit_bnxt_fwctl(void)
{
}
#endif
