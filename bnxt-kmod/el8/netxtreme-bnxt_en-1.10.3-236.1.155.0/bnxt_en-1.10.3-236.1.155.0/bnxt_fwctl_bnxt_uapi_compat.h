/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2024, Broadcom Corporation
 *
 */

#ifndef _UAPI_FWCTL_BNXT_H_
#define _UAPI_FWCTL_BNXT_H_

#if !defined(HAVE_FWCTL_DRIVER) && defined(HAVE_FWCTL_CLEANUP)
#include <linux/types.h>

#define MAX_DMA_MEM_SIZE		0x10000 /*64K*/

enum fwctl_bnxt_commands {
	FWCTL_BNXT_QUERY_COMMANDS = 0,
	FWCTL_BNXT_SEND_COMMAND,
};

/**
 * struct fwctl_info_bnxt - ioctl(FWCTL_INFO) out_device_data
 * @uctx_caps: The command capabilities driver accepts.
 *
 * Return basic information about the FW interface available.
 */
struct fwctl_info_bnxt {
	__u32 uctx_caps;
	__u32 reserved;
};

#define MAX_NUM_DMA_INDICATIONS 10

/**
 * struct fwctl_dma_info_bnxt - Describe the buffer that should be DMAed
 * @data: DMA-intended buffer
 * @len: Length of the @data
 * @offset: Offset at which HWRM input structure needs DMA address
 * @read_from_device: DMA direction, 0 or 1
 */
struct fwctl_dma_info_bnxt {
	__u64 data;
	__u32 len;
	__u16 offset;
	__u8 read_from_device;
	__u8 unused;
};

/**
 * struct fwctl_rpc_bnxt - describe the fwctl message for bnxt
 * @req: The HWRM command input structure
 * @req_len: Length of @req
 * @timeout: If the user wants to override the driver default
 * @num_dma: Number of DMA buffers to be added to @req
 * @payload: DMA intended details in struct fwctl_dma_info_bnxt format
 */
struct fwctl_rpc_bnxt {
	__aligned_u64 req;
	__u32 req_len;
	__u32 timeout;
	__u32 num_dma;
	__aligned_u64 payload;
};
#endif
#endif
