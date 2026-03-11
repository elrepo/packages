/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2021-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include "bnxt_compat.h"
#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_coredump.h"
#include "bnxt_ulp.h"
#include "bnxt_tfc.h"

const char *bnxt_trace_to_dbgfs_file[] = {
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_SRT_TRACE]		= "srt",
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_CRT_TRACE]		= "crt",
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_SRT2_TRACE]		= "srt2",
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_CRT2_TRACE]		= "crt2",
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_RIGP0_TRACE]		= "rigp0",
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_L2_HWRM_TRACE]		= "l2_hwrm",
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_ROCE_HWRM_TRACE]		= "roce_hwrm",
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_CA0_TRACE]		= "ca0",
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_CA1_TRACE]		= "ca1",
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_CA2_TRACE]		= "ca2",
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_RIGP1_TRACE]		= "rigp1",
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_AFM_KONG_HWRM_TRACE]	= "afm_hwrm",
	[DBG_LOG_BUFFER_FLUSH_REQ_TYPE_ERR_QPC_TRACE]		= "err_qpc",
};

const u16 bnxt_bstore_to_seg_id[] = {
	[BNXT_CTX_QP]			= BNXT_CTX_MEM_SEG_QP,
	[BNXT_CTX_SRQ]			= BNXT_CTX_MEM_SEG_SRQ,
	[BNXT_CTX_CQ]			= BNXT_CTX_MEM_SEG_CQ,
	[BNXT_CTX_VNIC]			= BNXT_CTX_MEM_SEG_VNIC,
	[BNXT_CTX_STAT]			= BNXT_CTX_MEM_SEG_STAT,
	[BNXT_CTX_STQM]			= BNXT_CTX_MEM_SEG_STQM,
	[BNXT_CTX_FTQM]			= BNXT_CTX_MEM_SEG_FTQM,
	[BNXT_CTX_MRAV]			= BNXT_CTX_MEM_SEG_MRAV,
	[BNXT_CTX_TIM]			= BNXT_CTX_MEM_SEG_TIM,
	[BNXT_CTX_TCK]			= BNXT_CTX_MEM_SEG_TCK,
	[BNXT_CTX_RCK]			= BNXT_CTX_MEM_SEG_RCK,
	[BNXT_CTX_MTQM]			= BNXT_CTX_MEM_SEG_MTQM,
	[BNXT_CTX_SQDBS]		= BNXT_CTX_MEM_SEG_SQDBS,
	[BNXT_CTX_RQDBS]		= BNXT_CTX_MEM_SEG_RQDBS,
	[BNXT_CTX_SRQDBS]		= BNXT_CTX_MEM_SEG_SRQDBS,
	[BNXT_CTX_CQDBS]		= BNXT_CTX_MEM_SEG_CQDBS,
	[BNXT_CTX_SRT_TRACE]		= BNXT_CTX_MEM_SEG_SRT,
	[BNXT_CTX_SRT2_TRACE]		= BNXT_CTX_MEM_SEG_SRT2,
	[BNXT_CTX_CRT_TRACE]		= BNXT_CTX_MEM_SEG_CRT,
	[BNXT_CTX_CRT2_TRACE]		= BNXT_CTX_MEM_SEG_CRT2,
	[BNXT_CTX_RIGP0_TRACE]		= BNXT_CTX_MEM_SEG_RIGP0,
	[BNXT_CTX_L2_HWRM_TRACE]	= BNXT_CTX_MEM_SEG_L2HWRM,
	[BNXT_CTX_ROCE_HWRM_TRACE]	= BNXT_CTX_MEM_SEG_REHWRM,
	[BNXT_CTX_CA0_TRACE]		= BNXT_CTX_MEM_SEG_CA0,
	[BNXT_CTX_CA1_TRACE]		= BNXT_CTX_MEM_SEG_CA1,
	[BNXT_CTX_CA2_TRACE]		= BNXT_CTX_MEM_SEG_CA2,
	[BNXT_CTX_RIGP1_TRACE]		= BNXT_CTX_MEM_SEG_RIGP1,
	[BNXT_CTX_AFM_KONG_HWRM_TRACE]	= BNXT_CTX_MEM_SEG_AFMKONG,
	[BNXT_CTX_ERR_QPC_TRACE]	= BNXT_CTX_MEM_SEG_QPC,
};

static int bnxt_dbg_hwrm_log_buffer_flush(struct bnxt *bp, u16 type, u32 flags, u32 *offset)
{
	struct hwrm_dbg_log_buffer_flush_output *resp;
	struct hwrm_dbg_log_buffer_flush_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_DBG_LOG_BUFFER_FLUSH);
	if (rc)
		return rc;

	req->flags = cpu_to_le32(flags);
	req->type = cpu_to_le16(type);
	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (!rc)
		*offset = le32_to_cpu(resp->current_buffer_offset);
	hwrm_req_drop(bp, req);
	return rc;
}

bool bnxt_bs_trace_dbgfs_available(struct bnxt *bp)
{
	int type;

	if (!bp->ctx)
		return false;

	for (type = BNXT_CTX_SRT_TRACE; type <= BNXT_CTX_ERR_QPC_TRACE; type++) {
		u32 flags = bp->ctx->ctx_arr[type].flags;

		if ((flags & BNXT_CTX_MEM_TYPE_VALID) &&
		    (flags & FUNC_BACKING_STORE_QCAPS_V2_RESP_FLAGS_FW_DBG_TRACE))
			return true;
	}
	return false;
}

void bnxt_bs_trace_dbgfs_copy(struct bnxt_bs_trace_info *bs_trace)
{
	size_t dbgfs_offset = 0, mem_size, last_offset, head;
	u32 flush_offset = 0, fw_status;
	struct bnxt_ctx_mem_type *ctxm;
	struct bnxt_ctx_mem_info *ctx;
	struct bnxt *bp;
	int rc = 0;

	bp = container_of(bs_trace, struct bnxt, bs_trace[bs_trace->trace_type]);
	ctx = bp->ctx;
	if (!ctx)
		return;

	ctxm = &ctx->ctx_arr[bs_trace->ctx_type];
	mem_size = ctxm->max_entries * ctxm->entry_size;
	if (!bs_trace->dbgfs_trace)
		bs_trace->dbgfs_trace = kmalloc(mem_size, GFP_KERNEL);
	if (!bs_trace->dbgfs_trace)
		return;
	fw_status = bnxt_fw_health_readl(bp, BNXT_FW_HEALTH_REG);
	if (BNXT_FW_IS_HEALTHY(fw_status)) {
		rc = bnxt_dbg_hwrm_log_buffer_flush(bp, bs_trace->trace_type, 0, &flush_offset);

		if (!rc)
			bnxt_bs_trace_check_wrapping(bs_trace, flush_offset);
	}
	last_offset = (size_t)bs_trace->last_offset;
	bs_trace->dbgfs_trace_size = bs_trace->wrapped * (mem_size - last_offset) + last_offset;
	head = 0;
	if (bs_trace->wrapped)
		head = last_offset;
	__bnxt_copy_ctx_mem(bp, ctxm, bs_trace->dbgfs_trace, dbgfs_offset,
			    head, last_offset);
}

void bnxt_bs_trace_dbgfs_clean(struct bnxt *bp)
{
	int i;

	for (i = 0; i < BNXT_TRACE_BUF_COUNT; i++) {
		kfree(bp->bs_trace[i].dbgfs_trace);
		bp->bs_trace[i].dbgfs_trace = NULL;
		bp->bs_trace[i].dbgfs_trace_size = 0;
	}
}

static int bnxt_hwrm_dbg_dma_data(struct bnxt *bp, void *msg,
				  struct bnxt_hwrm_dbg_dma_info *info)
{
	struct hwrm_dbg_cmn_input *cmn_req = msg;
	__le16 *seq_ptr = msg + info->seq_off;
	struct hwrm_dbg_cmn_output *cmn_resp;
	u16 seq = 0, len, segs_off;
	dma_addr_t dma_handle;
	void *dma_buf, *resp;
	int rc, off = 0;

	dma_buf = hwrm_req_dma_slice(bp, msg, info->dma_len, &dma_handle);
	if (!dma_buf) {
		hwrm_req_drop(bp, msg);
		return -ENOMEM;
	}

	hwrm_req_timeout(bp, msg, HWRM_COREDUMP_TIMEOUT);
	cmn_resp = hwrm_req_hold(bp, msg);
	resp = cmn_resp;

	segs_off = offsetof(struct hwrm_dbg_coredump_list_output, total_segments);
	cmn_req->host_dest_addr = cpu_to_le64(dma_handle);
	cmn_req->host_buf_len = cpu_to_le32(info->dma_len);
	while (1) {
		*seq_ptr = cpu_to_le16(seq);
		rc = hwrm_req_send(bp, msg);
		if (rc)
			break;

		len = le16_to_cpu(*((__le16 *)(resp + info->data_len_off)));
		if (!seq &&
		    cmn_req->req_type == cpu_to_le16(HWRM_DBG_COREDUMP_LIST)) {
			info->segs = le16_to_cpu(*((__le16 *)(resp + segs_off)));
			if (!info->segs) {
				rc = -EIO;
				break;
			}

			info->dest_buf_size = info->segs *
				sizeof(struct coredump_segment_record);
			info->dest_buf = kmalloc(info->dest_buf_size, GFP_KERNEL);
			if (!info->dest_buf) {
				rc = -ENOMEM;
				break;
			}
		}

		if (cmn_req->req_type == cpu_to_le16(HWRM_DBG_COREDUMP_RETRIEVE))
			info->dest_buf_size += len;

		if (info->dest_buf) {
			if ((info->seg_start + off + len) <=
			    BNXT_COREDUMP_BUF_LEN(info->buf_len)) {
				if ((info->dest_buf_size - off) >= len)
					memcpy(info->dest_buf + off, dma_buf, len);
				else
					break;
			} else {
				rc = -ENOBUFS;
				if (cmn_req->req_type == cpu_to_le16(HWRM_DBG_COREDUMP_LIST)) {
					kfree(info->dest_buf);
					info->dest_buf = NULL;
				}
				break;
			}
		}

		if (!(cmn_resp->flags & HWRM_DBG_CMN_FLAGS_MORE))
			break;

		seq++;
		off += len;
	}
	hwrm_req_drop(bp, msg);
	return rc;
}

static int bnxt_hwrm_dbg_coredump_list(struct bnxt *bp,
				       struct bnxt_coredump *coredump)
{
	struct bnxt_hwrm_dbg_dma_info info = {NULL};
	struct hwrm_dbg_coredump_list_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_DBG_COREDUMP_LIST);
	if (rc)
		return rc;

	info.dma_len = COREDUMP_LIST_BUF_LEN;
	info.seq_off = offsetof(struct hwrm_dbg_coredump_list_input, seq_no);
	info.data_len_off = offsetof(struct hwrm_dbg_coredump_list_output,
				     data_len);

	rc = bnxt_hwrm_dbg_dma_data(bp, req, &info);
	if (!rc) {
		coredump->data = info.dest_buf;
		coredump->data_size = info.dest_buf_size;
		coredump->total_segs = info.segs;
	}
	return rc;
}

static int bnxt_hwrm_dbg_coredump_initiate(struct bnxt *bp, u16 component_id,
					   u16 segment_id)
{
	struct hwrm_dbg_coredump_initiate_input *req;
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_DBG_COREDUMP_INITIATE);
	if (rc)
		return rc;

	hwrm_req_timeout(bp, req, HWRM_COREDUMP_TIMEOUT);
	req->component_id = cpu_to_le16(component_id);
	req->segment_id = cpu_to_le16(segment_id);
	if (bp->dump_flag == BNXT_DUMP_LIVE_WITH_CTX_L1_CACHE)
		req->seg_flags = DBG_COREDUMP_INITIATE_REQ_SEG_FLAGS_COLLECT_CTX_L1_CACHE;

	return hwrm_req_send(bp, req);
}

static int bnxt_hwrm_dbg_coredump_retrieve(struct bnxt *bp, u16 component_id,
					   u16 segment_id, u32 *seg_len,
					   void *buf, u32 buf_len, u32 offset)
{
	struct hwrm_dbg_coredump_retrieve_input *req;
	struct bnxt_hwrm_dbg_dma_info info = {NULL};
	int rc;

	rc = hwrm_req_init(bp, req, HWRM_DBG_COREDUMP_RETRIEVE);
	if (rc)
		return rc;

	req->component_id = cpu_to_le16(component_id);
	req->segment_id = cpu_to_le16(segment_id);

	info.dma_len = COREDUMP_RETRIEVE_BUF_LEN;
	info.seq_off = offsetof(struct hwrm_dbg_coredump_retrieve_input,
				seq_no);
	info.data_len_off = offsetof(struct hwrm_dbg_coredump_retrieve_output,
				     data_len);
	if (buf) {
		info.dest_buf = buf + offset;
		info.buf_len = buf_len;
		info.seg_start = offset;
	}

	rc = bnxt_hwrm_dbg_dma_data(bp, req, &info);
	if (!rc)
		*seg_len = info.dest_buf_size;

	return rc;
}

int bnxt_hwrm_dbg_coredump_capture(struct bnxt *bp)
{
	struct hwrm_dbg_coredump_capture_input *req;
	int rc;

	if (!(bp->fw_dbg_cap & DBG_QCAPS_RESP_FLAGS_COREDUMP_HOST_CAPTURE) ||
	    !(bp->fw_crash_mem || (bp->fw_cap & BNXT_FW_CAP_HOST_COREDUMP)))
		return 0;

	rc = hwrm_req_init(bp, req, HWRM_DBG_COREDUMP_CAPTURE);
	if (rc)
		return rc;

	hwrm_req_timeout(bp, req, HWRM_COREDUMP_TIMEOUT);
	return hwrm_req_send(bp, req);
}

void
bnxt_fill_coredump_seg_hdr(struct bnxt *bp,
			   struct bnxt_coredump_segment_hdr *seg_hdr,
			   struct coredump_segment_record *seg_rec, u32 seg_len,
			   int status, u32 duration, u32 instance, u32 comp_id,
			   u32 seg_id)
{
	memset(seg_hdr, 0, sizeof(*seg_hdr));
	memcpy(seg_hdr->signature, "sEgM", 4);
	if (seg_rec) {
		seg_hdr->component_id = (__force __le32)seg_rec->component_id;
		seg_hdr->segment_id = (__force __le32)seg_rec->segment_id;
		seg_hdr->low_version = seg_rec->version_low;
		seg_hdr->high_version = seg_rec->version_hi;
		seg_hdr->flags = seg_rec->compress_flags;
	} else {
		seg_hdr->component_id = cpu_to_le32(comp_id);
		seg_hdr->segment_id = cpu_to_le32(seg_id);
	}
	if (BNXT_PF(bp))
		seg_hdr->function_id = bp->pf.fw_fid;
	else
		seg_hdr->function_id = bp->vf.fw_fid;
	seg_hdr->length = cpu_to_le32(seg_len);
	seg_hdr->status = cpu_to_le32(status);
	seg_hdr->duration = cpu_to_le32(duration);
	seg_hdr->data_offset = cpu_to_le32(sizeof(*seg_hdr));
	seg_hdr->instance = cpu_to_le32(instance);
}

struct bnxt_time bnxt_get_current_time(struct bnxt *bp)
{
	struct bnxt_time time;
#if defined(HAVE_TIME64)
	time64_to_tm(ktime_get_real_seconds(), -sys_tz.tz_minuteswest * 60, &time.tm);
#else
	struct timeval tv;

	do_gettimeofday(&tv);
	time_to_tm(tv.tv_sec, -sys_tz.tz_minuteswest * 60, &time.tm);
#endif
	time.tm.tm_mon += 1;
	time.tm.tm_year += 1900;

	return time;
}

static void bnxt_fill_cmdline(struct bnxt_coredump_record *record)
{
	struct mm_struct *mm = current->mm;

	if (mm) {
		unsigned long len = mm->arg_end - mm->arg_start;
		int i, last = 0;

		len = min(len, sizeof(record->commandline) - 1);
		if (len && !copy_from_user(record->commandline,
		    (char __user *) mm->arg_start, len)) {
			for (i = 0; i < len; i++) {
				if (record->commandline[i])
					last = i;
				else
					record->commandline[i] = ' ';
			}
			record->commandline[last + 1] = 0;
			return;
		}
	}

	strscpy(record->commandline, current->comm, TASK_COMM_LEN);
}

static void
bnxt_fill_coredump_record(struct bnxt *bp, struct bnxt_coredump_record *record,
			  struct bnxt_time start, s16 start_utc, u16 total_segs,
			  int status)
{
	struct bnxt_time end = bnxt_get_current_time(bp);
	u32 os_ver_major = 0, os_ver_minor = 0;

	memset(record, 0, sizeof(*record));
	memcpy(record->signature, "cOrE", 4);
	record->flags = 0;
	record->low_version = 0;
	record->high_version = 1;
	record->asic_state = 0;
	strscpy(record->system_name, utsname()->nodename,
		sizeof(record->system_name));
	record->year = cpu_to_le16(start.tm.tm_year);
	record->month = cpu_to_le16(start.tm.tm_mon);
	record->day = cpu_to_le16(start.tm.tm_mday);
	record->hour = cpu_to_le16(start.tm.tm_hour);
	record->minute = cpu_to_le16(start.tm.tm_min);
	record->second = cpu_to_le16(start.tm.tm_sec);
	record->utc_bias = cpu_to_le16(start_utc);
	bnxt_fill_cmdline(record);
	record->total_segments = cpu_to_le32(total_segs);

	if (sscanf(utsname()->release, "%u.%u", &os_ver_major, &os_ver_minor) != 2)
		netdev_warn(bp->dev, "Unknown OS release in coredump\n");
	record->os_ver_major = cpu_to_le32(os_ver_major);
	record->os_ver_minor = cpu_to_le32(os_ver_minor);

	strscpy(record->os_name, utsname()->sysname, sizeof(record->os_name));
	record->end_year = cpu_to_le16(end.tm.tm_year);
	record->end_month = cpu_to_le16(end.tm.tm_mon);
	record->end_day = cpu_to_le16(end.tm.tm_mday);
	record->end_hour = cpu_to_le16(end.tm.tm_hour);
	record->end_minute = cpu_to_le16(end.tm.tm_min);
	record->end_second = cpu_to_le16(end.tm.tm_sec);
	record->end_utc_bias = cpu_to_le16(sys_tz.tz_minuteswest);
	record->asic_id1 = cpu_to_le32(bp->chip_num << 16 |
	bp->ver_resp.chip_rev << 8 |
	bp->ver_resp.chip_metal);
	record->asic_id2 = 0;
	record->coredump_status = cpu_to_le32(status);
	record->ioctl_low_version = 0;
	record->ioctl_high_version = 0;
}

static u32 bnxt_dump_drv_version(struct bnxt *bp, void *buf, u32 buf_len)
{
	u32 len;

	len = snprintf(buf, buf_len, "\nInterface: %s  driver version: %s\n",
		       bp->dev->name, DRV_MODULE_VERSION);
	return (len >= buf_len) ? buf_len : len;
}

static u32 bnxt_dump_tx_sw_state(struct bnxt_napi *bnapi, void *buf,
				 u32 buf_len)
{
	struct bnxt_tx_ring_info *txr;
	int i = bnapi->index, j;
	u32 len = 0;

	bnxt_for_each_napi_tx(j, bnapi, txr) {
		len += snprintf(buf + len, buf_len - len,
				"[%d.%d]: tx{fw_ring: %d prod: %x cons: %x}\n",
				i, j, txr->tx_ring_struct.fw_ring_id,
				txr->tx_prod, txr->tx_cons);
		if (len >= buf_len)
			return buf_len;
	}
	return len;
}

static u32 bnxt_dump_rx_sw_state(struct bnxt_napi *bnapi, void *buf,
				 u32 buf_len)
{
	struct bnxt_rx_ring_info *rxr = bnapi->rx_ring;
	int i = bnapi->index;
	u32 len;

	if (!rxr)
		return 0;

	len = snprintf(buf, buf_len, "[%d]: rx{fw_ring: %d prod: %x} rx_agg{fw_ring: %d agg_prod: %x sw_agg_prod: %x}\n",
		       i, rxr->rx_ring_struct.fw_ring_id, rxr->rx_prod,
		       rxr->rx_agg_ring_struct.fw_ring_id, rxr->rx_agg_prod,
		       rxr->rx_sw_agg_prod);
	return (len >= buf_len) ? buf_len : len;
}

static u32 bnxt_dump_cp_sw_state(struct bnxt_napi *bnapi, void *buf,
				 u32 buf_len)
{
	struct bnxt_cp_ring_info *cpr = &bnapi->cp_ring, *cpr2;
	int i = bnapi->index, j;
	u32 len = 0;

	len = snprintf(buf, buf_len, "[%d]: cp{fw_ring: %d raw_cons: %x}\n",
		       i, cpr->cp_ring_struct.fw_ring_id, cpr->cp_raw_cons);
	if (len >= buf_len)
		return buf_len;
	for (j = 0; j < cpr->cp_ring_count; j++) {
		cpr2 = &cpr->cp_ring_arr[j];
		if (!cpr2->bnapi)
			continue;
		len += snprintf(buf + len, buf_len - len,
				"[%d.%d]: cp{fw_ring: %d raw_cons: %x}\n",
				i, j, cpr2->cp_ring_struct.fw_ring_id,
				cpr2->cp_raw_cons);
		if (len >= buf_len)
			return buf_len;
	}
	return len;
}

static u32 bnxt_get_l2_coredump(struct bnxt *bp, void *buf, u32 offset,
				u32 buf_len)
{
	u32 len, seg_len = BNXT_L2_COREDUMP_BUF_LEN;
	struct bnxt_coredump_segment_hdr seg_hdr;
	struct bnxt_napi *bnapi;
	int i;

	if (!buf)
		return buf_len;

	buf += offset;
	bnxt_fill_coredump_seg_hdr(bp, &seg_hdr, NULL, seg_len, 0, 0, 0,
				   DRV_COREDUMP_COMP_ID, 0);
	len = BNXT_SEG_HDR_LEN;
	memcpy(buf, &seg_hdr, len);
	len += bnxt_dump_drv_version(bp, buf + len, buf_len - len);

	if (!netif_running(bp->dev))
		return buf_len;

	for (i = 0; i < bp->cp_nr_rings; i++) {
		bnapi = bp->bnapi[i];
		len += bnxt_dump_tx_sw_state(bnapi, buf + len, buf_len - len);
		if (len == buf_len)
			break;
		len += bnxt_dump_rx_sw_state(bnapi, buf + len, buf_len - len);
		if (len == buf_len)
			break;
		len += bnxt_dump_cp_sw_state(bnapi, buf + len, buf_len - len);
		if (len == buf_len)
			break;
	}
	return buf_len;
}

static void bnxt_fill_drv_seg_record(struct bnxt *bp,
				     struct bnxt_driver_segment_record *driver_seg_record,
				     struct bnxt_ctx_mem_type *ctxm, u16 type)
{
	struct bnxt_bs_trace_info *bs_trace = &bp->bs_trace[type];
	u32 offset = 0;
	int rc = 0;

	driver_seg_record->max_entries = cpu_to_le32(ctxm->max_entries);
	driver_seg_record->entry_size = cpu_to_le32(ctxm->entry_size);

	rc = bnxt_dbg_hwrm_log_buffer_flush(bp, type, 0, &offset);
	if (rc)
		return;

	bnxt_bs_trace_check_wrapping(bs_trace, offset);
	driver_seg_record->offset = cpu_to_le32(bs_trace->last_offset);
	driver_seg_record->wrapped = bs_trace->wrapped;
}

static u32 bnxt_get_ctx_coredump(struct bnxt *bp, void *buf, u32 offset,
				 u32 *segs)
{
	struct bnxt_driver_segment_record record = {};
	struct bnxt_coredump_segment_hdr seg_hdr;
	struct bnxt_ctx_mem_info *ctx = bp->ctx;
	u32 comp_id = DRV_COREDUMP_COMP_ID;
	void *data = NULL;
	size_t len = 0;
	u16 type;

	*segs = 0;
	if (!ctx)
		return 0;

	if (buf)
		buf += offset;
	for (type = 0 ; type < BNXT_CTX_V2_MAX; type++) {
		struct bnxt_ctx_mem_type *ctxm = &ctx->ctx_arr[type];
		bool trace = bnxt_bs_trace_available(bp, type);
		u32 seg_id = bnxt_bstore_to_seg_id[type];
		size_t seg_len, extra_hlen = 0;

		if (!ctxm->mem_valid || !seg_id)
			continue;

		if (trace) {
			extra_hlen = BNXT_SEG_RCD_LEN;
			if (buf) {
				u16 trace_type = bnxt_bstore_to_trace[type];

				bnxt_fill_drv_seg_record(bp, &record, ctxm,
							 trace_type);
			}
		}

		if (buf)
			data = buf + BNXT_SEG_HDR_LEN + extra_hlen;

		seg_len = bnxt_copy_ctx_mem(bp, ctxm, data, 0) + extra_hlen;
		if (buf) {
			bnxt_fill_coredump_seg_hdr(bp, &seg_hdr, NULL, seg_len,
						   0, 0, 0, comp_id, seg_id);
			memcpy(buf, &seg_hdr, BNXT_SEG_HDR_LEN);
			buf += BNXT_SEG_HDR_LEN;
			if (trace)
				memcpy(buf, &record, BNXT_SEG_RCD_LEN);
			buf += seg_len;
		}
		len += BNXT_SEG_HDR_LEN + seg_len;
		*segs += 1;
	}
	return len;
}

static int __bnxt_get_coredump(struct bnxt *bp, u16 dump_type, void *buf,
			       u32 *dump_len)
{
	u32 ver_get_resp_len = sizeof(struct hwrm_ver_get_output);
	u32 offset = 0, seg_hdr_len, seg_record_len, buf_len = 0;
	struct coredump_segment_record *seg_record = NULL;
	struct bnxt_coredump_segment_hdr seg_hdr;
	struct bnxt_coredump coredump = {NULL};
	u32 bstore_bytes = 0, tf_seg = 0;
	struct bnxt_time start_time;
	s16 start_utc;
	int rc = 0, i;

	if (buf)
		buf_len = *dump_len;

	start_time = bnxt_get_current_time(bp);
	start_utc = sys_tz.tz_minuteswest;
	seg_hdr_len = sizeof(seg_hdr);

	/* First segment should be hwrm_ver_get response.
	 * For hwrm_ver_get response Component id = 2 and Segment id = 0
	 */
	*dump_len = seg_hdr_len + ver_get_resp_len;
	if (buf) {
		bnxt_fill_coredump_seg_hdr(bp, &seg_hdr, NULL, ver_get_resp_len,
					   0, 0, 0, BNXT_VER_GET_COMP_ID, 0);
		memcpy(buf + offset, &seg_hdr, seg_hdr_len);
		offset += seg_hdr_len;
		memcpy(buf + offset, &bp->ver_resp, ver_get_resp_len);
		offset += ver_get_resp_len;
	}

	if (BNXT_TRUFLOW_EN(bp)) {
		rc = tfc_corefile_bs_size(bp, &bstore_bytes);
		if (rc) {
			netdev_warn(bp->dev, "Failed to get backingstore size\n");
		} else if (buf && bstore_bytes) {
			rc = tfc_corefile_bs_save(bp, buf + offset, &bstore_bytes);
			if (rc) {
				netdev_warn(bp->dev, "Failed to save backingstore\n");
			} else {
				offset += seg_hdr_len + bstore_bytes;
				tf_seg++;
			}
		}

		if (!rc && bstore_bytes)
			*dump_len += seg_hdr_len + bstore_bytes;
	}

	if (dump_type == BNXT_DUMP_DRIVER) {
		u32 drv_len, drv_segs, segs = 0;
		void *drv_buf = NULL;

		drv_len = bnxt_get_l2_coredump(bp, buf, offset,
					       BNXT_L2_COREDUMP_LEN);
		drv_segs = 1;
		drv_len += bnxt_get_ctx_coredump(bp, buf, offset +
						 drv_len, &segs);
		drv_segs += segs;
		segs = 0;
		if (buf)
			drv_buf = buf + offset + drv_len;
		drv_len += bnxt_get_ulp_dump(bp, dump_type, drv_buf, &segs);
		drv_segs += segs;
		*dump_len += drv_len;
		offset += drv_len;
		if (buf)
			coredump.total_segs += drv_segs;
		goto fw_coredump_err;
	}

	seg_record_len = sizeof(*seg_record);
	rc = bnxt_hwrm_dbg_coredump_list(bp, &coredump);
	if (rc) {
		netdev_err(bp->dev, "Failed to get coredump segment list\n");
		goto fw_coredump_err;
	}

	*dump_len += seg_hdr_len * coredump.total_segs;

	seg_record = (struct coredump_segment_record *)coredump.data;
	seg_record_len = sizeof(*seg_record);

	for (i = 0; i < coredump.total_segs; i++) {
		u16 comp_id = le16_to_cpu(seg_record->component_id);
		u16 seg_id = le16_to_cpu(seg_record->segment_id);
		u32 duration = 0, seg_len = 0;
		unsigned long start, end;

		if (buf && ((offset + seg_hdr_len) >
			    BNXT_COREDUMP_BUF_LEN(buf_len))) {
			rc = -ENOBUFS;
			goto fw_coredump_err;
		}

		start = jiffies;

		rc = bnxt_hwrm_dbg_coredump_initiate(bp, comp_id, seg_id);
		if (rc) {
			netdev_err(bp->dev,
				   "Failed to initiate coredump for seg = %d\n",
				   seg_record->segment_id);
			goto next_seg;
		}

		/* Write segment data into the buffer */
		rc = bnxt_hwrm_dbg_coredump_retrieve(bp, comp_id, seg_id,
						     &seg_len, buf, buf_len,
						     offset + seg_hdr_len);
		if (rc && rc == -ENOBUFS)
			goto fw_coredump_err;
		else if (rc)
			netdev_err(bp->dev,
				   "Failed to retrieve coredump for seg = %d\n",
				   seg_record->segment_id);
next_seg:
		end = jiffies;
		duration = jiffies_to_msecs(end - start);
		bnxt_fill_coredump_seg_hdr(bp, &seg_hdr, seg_record, seg_len,
					   rc, duration, 0, 0, 0);

		if (buf) {
			/* Write segment header into the buffer */
			memcpy(buf + offset, &seg_hdr, seg_hdr_len);
			offset += seg_hdr_len + seg_len;
		}

		*dump_len += seg_len;
		seg_record =
			(struct coredump_segment_record *)((u8 *)seg_record +
							   seg_record_len);
	}

fw_coredump_err:
	if (buf)
		bnxt_fill_coredump_record(bp, buf + offset, start_time,
					  start_utc, coredump.total_segs + tf_seg + 1,
					  rc);

	kfree(coredump.data);
	if (!rc) {
		*dump_len += sizeof(struct bnxt_coredump_record);
		/* The actual coredump length can be smaller than the FW
		 * reported length earlier. Use the ethtool provided length.
		 */
		if (buf_len)
			*dump_len = buf_len;
	} else if (rc == -ENOBUFS) {
		netdev_err(bp->dev, "Firmware returned large coredump buffer\n");
	}
	return rc;
}

static u32 bnxt_copy_crash_data(struct bnxt_ring_mem_info *rmem, void *buf,
				u32 dump_len)
{
	u32 data_copied = 0;
	u32 data_len;
	int i;

	for (i = 0; i < rmem->nr_pages; i++) {
		data_len = rmem->page_size;
		if (data_copied + data_len > dump_len)
			data_len = dump_len - data_copied;
		memcpy(buf + data_copied, rmem->pg_arr[i], data_len);
		data_copied += data_len;
		if (data_copied >= dump_len)
			break;
	}
	return data_copied;
}

static int bnxt_copy_crash_dump(struct bnxt *bp, void *buf, u32 dump_len)
{
	struct bnxt_ring_mem_info *rmem;
	u32 offset = 0;

	if (!bp->fw_crash_mem)
		return -ENOENT;

	rmem = &bp->fw_crash_mem->ring_mem;

	if (rmem->depth > 1) {
		int i;

		for (i = 0; i < rmem->nr_pages; i++) {
			struct bnxt_ctx_pg_info *pg_tbl;

			pg_tbl = bp->fw_crash_mem->ctx_pg_tbl[i];
			offset += bnxt_copy_crash_data(&pg_tbl->ring_mem,
						       buf + offset, dump_len - offset);
			if (offset >= dump_len)
				break;
		}
	} else {
		bnxt_copy_crash_data(rmem, buf, dump_len);
	}

	return 0;
}

static bool bnxt_crash_dump_avail(struct bnxt *bp)
{
	u32 sig = 0;

	/* First 4 bytes(signature) of crash dump is always non-zero */
	bnxt_copy_crash_dump(bp, &sig, sizeof(u32));
	return !!sig;
}

static int bnxt_get_hdr_len(int segments, int record_len)
{
	return segments * record_len + sizeof(struct bnxt_coredump_segment_hdr) +
	       sizeof(struct bnxt_coredump_record);
}

int bnxt_get_coredump(struct bnxt *bp, u16 dump_type, void *buf, u32 *dump_len)
{
	if (dump_type == BNXT_DUMP_CRASH) {
		if (bp->fw_dbg_cap & DBG_QCAPS_RESP_FLAGS_CRASHDUMP_HOST_DDR)
			return bnxt_copy_crash_dump(bp, buf, *dump_len);
#ifdef CONFIG_TEE_BNXT_FW
		else if (bp->fw_dbg_cap & DBG_QCAPS_RESP_FLAGS_CRASHDUMP_SOC_DDR)
			return tee_bnxt_copy_coredump(buf, 0, *dump_len);
#endif
		else
			return -EOPNOTSUPP;
	} else {
		return  __bnxt_get_coredump(bp, dump_type, buf, dump_len);
	}
}

int bnxt_hwrm_get_dump_len(struct bnxt *bp, u16 dump_type, u32 *dump_len)
{
	struct hwrm_dbg_qcfg_output *resp;
	struct hwrm_dbg_qcfg_input *req;
	int rc, hdr_len = 0;

	if (!(bp->fw_cap & BNXT_FW_CAP_DBG_QCAPS) ||
	    dump_type == BNXT_DUMP_DRIVER)
		return -EOPNOTSUPP;

	if (dump_type == BNXT_DUMP_CRASH &&
	    !(bp->fw_dbg_cap & DBG_QCAPS_RESP_FLAGS_CRASHDUMP_SOC_DDR ||
	     (bp->fw_dbg_cap & DBG_QCAPS_RESP_FLAGS_CRASHDUMP_HOST_DDR)))
		return -EOPNOTSUPP;

	rc = hwrm_req_init(bp, req, HWRM_DBG_QCFG);
	if (rc)
		return rc;

	req->fid = cpu_to_le16(0xffff);
	if (dump_type == BNXT_DUMP_CRASH) {
		if (bp->fw_dbg_cap & DBG_QCAPS_RESP_FLAGS_CRASHDUMP_SOC_DDR)
			req->flags = cpu_to_le16(BNXT_DBG_FL_CR_DUMP_SIZE_SOC);
		else
			req->flags = cpu_to_le16(BNXT_DBG_FL_CR_DUMP_SIZE_HOST);
	}
#ifdef BNXT_FPGA
	/* FPGA takes a long time to calcuate coredump and crashdump length */
	hwrm_req_timeout(bp, req, HWRM_COREDUMP_TIMEOUT);
#endif
	resp = hwrm_req_hold(bp, req);
	rc = hwrm_req_send(bp, req);
	if (rc)
		goto get_dump_len_exit;

	if (dump_type == BNXT_DUMP_CRASH) {
		if (bp->fw_dbg_cap & DBG_QCAPS_RESP_FLAGS_CRASHDUMP_SOC_DDR)
			*dump_len = BNXT_CRASH_DUMP_LEN;
		else
			*dump_len = le32_to_cpu(resp->crashdump_size);
	} else {
		/* Driver adds coredump header and "HWRM_VER_GET response"
		 * segment additionally to coredump.
		 */
		hdr_len = bnxt_get_hdr_len(1, sizeof(struct hwrm_ver_get_output));
		*dump_len = le32_to_cpu(resp->coredump_size) + hdr_len;
	}
	if (*dump_len <= hdr_len)
		rc = -EINVAL;

get_dump_len_exit:
	hwrm_req_drop(bp, req);
	return rc;
}

u32 bnxt_get_coredump_length(struct bnxt *bp, u16 dump_type)
{
	u32 len = 0, bstore_bytes = 0;

	if (dump_type == BNXT_DUMP_CRASH &&
	    bp->fw_dbg_cap & DBG_QCAPS_RESP_FLAGS_CRASHDUMP_HOST_DDR &&
	    bp->fw_crash_mem) {
		if (!bnxt_crash_dump_avail(bp))
			return 0;

		return bp->fw_crash_len;
	}

	if (bnxt_hwrm_get_dump_len(bp, dump_type, &len))
		__bnxt_get_coredump(bp, dump_type, NULL, &len);

	if (BNXT_TRUFLOW_EN(bp)) {
		if (tfc_corefile_bs_size(bp, &bstore_bytes))
			netdev_warn(bp->dev, "Failed to get backingstore size\n");
		else
			len += bstore_bytes;
	}

	return len;
}
