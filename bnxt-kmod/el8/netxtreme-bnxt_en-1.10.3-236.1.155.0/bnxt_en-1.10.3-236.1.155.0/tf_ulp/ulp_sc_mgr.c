// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2024 Broadcom
 * All rights reserved.
 */
#include "linux/ktime.h"
#include "linux/time64.h"
#include "linux/timekeeping.h"
#include "bnxt_compat.h"
#include "bnxt.h"
#include "bnxt_ulp.h"
#include "bnxt_tf_common.h"
#include "bnxt_tf_ulp.h"
#include "bnxt_tfc.h"
#include "ulp_linux.h"
#include "ulp_sc_mgr.h"
#include "ulp_flow_db.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "tfc.h"
#include "tfc_debug.h"
#include "tfc_action_handle.h"

#define ULP_TFC_CNTR_READ_BYTES 32
#define ULP_TFC_CNTR_ALIGN 32
#define ULP_TFC_ACT_WORD_SZ 32

/* We want to use the flow-id as an index in to an array for speed but
 * flow-id will not start at 0 if there are default flows created. The
 * default flows are not part of ULP and will not use the stats counter
 * and since we don't, and can't, know the first flow-id we increase
 * the stats table size by a bit to accommodate default flow-id range.
 * These additional entries will be unused but will enable us to
 * directly index the table using the flow-id.
 */
static inline u32 bnxt_ulp_sc_num_counters(u32 n) { return (n + (n / 10)); }

/* Tests have shown that if MPC commands can be batched a 10x throughput
 * per second improvement can be acheived. This change introduces a
 * background thread that will periodically identify active flows and
 * batch MPC read-clears to harvest the stats for those flows. Apps such
 * as OVS will read the flow stats from the cache rather than directly
 * through MPC table read-clear commands, this will enable the app to
 * handle a greater number of flows.
 *
 * Tests have shown that batches of 64 MPC commands seems to give the
 * best MPC throughput performance so this change uses 64 as the target
 * number for batching. If there are less than 64 active flows then the
 * batch size will simply be the number of active flows.
 */

static const struct bnxt_ulp_sc_core_ops *
bnxt_ulp_sc_ops_get(struct bnxt_ulp_context *ctxt)
{
	const struct bnxt_ulp_sc_core_ops *func_ops;
	enum bnxt_ulp_device_id  dev_id;
	int rc;

	rc = bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id);
	if (rc)
		return NULL;

	switch (dev_id) {
	case BNXT_ULP_DEVICE_ID_THOR2:
		func_ops = &ulp_sc_tfc_core_ops;
		break;
	case BNXT_ULP_DEVICE_ID_THOR:
	case BNXT_ULP_DEVICE_ID_STINGRAY:
	case BNXT_ULP_DEVICE_ID_WH_PLUS:
	default:
		func_ops = NULL;
		break;
	}
	return func_ops;
}

int32_t ulp_sc_mgr_init(struct bnxt_ulp_context *ctxt)
{
	const struct bnxt_ulp_sc_core_ops *sc_ops;
	struct bnxt_ulp_device_params *dparms;
	struct bnxt_ulp_sc_info *ulp_sc_info;
	u32 stats_cache_tbl_sz;
	u32 dev_id;
	u32 i;

	if (!ctxt) {
		netdev_dbg(NULL, "Invalid ULP CTXT\n");
		return -EINVAL;
	}

	if (bnxt_ulp_cntxt_dev_id_get(ctxt, &dev_id)) {
		netdev_dbg(ctxt->bp->dev, "Failed to get device id\n");
		return -EINVAL;
	}

	dparms = bnxt_ulp_device_params_get(dev_id);
	if (!dparms || !dparms->ext_flow_db_num_entries) {
		/* No need for software counters, call fw directly */
		netdev_dbg(ctxt->bp->dev, "Sw flow counter support not enabled\n");
		return -EINVAL;
	}

	sc_ops = bnxt_ulp_sc_ops_get(ctxt);
	if (!sc_ops) {
		netdev_dbg(ctxt->bp->dev, "Failed to get the counter ops\n");
		return -EINVAL;
	}

	mutex_lock(&ctxt->cfg_data->sc_lock);

	ulp_sc_info = kzalloc(sizeof(*ulp_sc_info), GFP_KERNEL);
	if (!ulp_sc_info)
		goto unlock;

	ulp_sc_info->sc_ops = sc_ops;
	ulp_sc_info->flags = 0;
	ulp_sc_info->num_counters = dparms->ext_flow_db_num_entries;

	stats_cache_tbl_sz = sizeof(struct ulp_sc_tfc_stats_cache_entry) *
		bnxt_ulp_sc_num_counters(ulp_sc_info->num_counters);

	ulp_sc_info->stats_cache_tbl = vzalloc(stats_cache_tbl_sz);
	if (!ulp_sc_info->stats_cache_tbl)
		goto cleanup1;

	for (i = 0; i < ULP_SC_BATCH_SIZE; i++) {
		ulp_sc_info->data_va[i] = dma_zalloc_coherent(&ctxt->bp->pdev->dev,
							      ULP_SC_PAGE_SIZE,
							      &ulp_sc_info->data_pa[i],
							      GFP_KERNEL);
		if (!ulp_sc_info->data_va[i])
			goto cleanup2;
	}

	ulp_sc_info->batch_info = kzalloc(sizeof(*ulp_sc_info->batch_info),
					  GFP_KERNEL);
	if (!ulp_sc_info->batch_info)
		goto cleanup2;

	/* Add the SC info tbl to the ulp context. */
	bnxt_ulp_cntxt_ptr2_sc_info_set(ctxt, ulp_sc_info);
	mutex_unlock(&ctxt->cfg_data->sc_lock);

	return 0;

 cleanup2:
	for (i = 0; i < ULP_SC_BATCH_SIZE; i++) {
		if (!ulp_sc_info->data_va[i])
			continue;
		dma_free_coherent(&ctxt->bp->pdev->dev, ULP_SC_PAGE_SIZE,
				  ulp_sc_info->data_va[i],
				  ulp_sc_info->data_pa[i]);
	}
	vfree(ulp_sc_info->stats_cache_tbl);
 cleanup1:
	kfree(ulp_sc_info);
 unlock:
	mutex_unlock(&ctxt->cfg_data->sc_lock);

	return -ENOMEM;
}

/* Release temporary resources in the Flow Counter Manager for this ulp context
 *
 * ctxt [in] The ulp context for the Flow Counter manager
 */
void
ulp_sc_mgr_deinit(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_sc_info *ulp_sc_info;
	u32 i;

	mutex_lock(&ctxt->cfg_data->sc_lock);
	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);

	if (!ulp_sc_info) {
		mutex_unlock(&ctxt->cfg_data->sc_lock);
		return;
	}

	/* Safe to ignore on deinit */
	(void)bnxt_ulp_cntxt_ptr2_sc_info_set(ctxt, NULL);
	mutex_unlock(&ctxt->cfg_data->sc_lock);

	if (ulp_sc_info->flags & ULP_FLAG_SC_THREAD)
		cancel_delayed_work_sync(&ctxt->cfg_data->sc_work);

	if (ctxt->cfg_data->sc_wq)
		destroy_workqueue(ctxt->cfg_data->sc_wq);

	if (ulp_sc_info->stats_cache_tbl)
		vfree(ulp_sc_info->stats_cache_tbl);

	for (i = 0; i < ULP_SC_BATCH_SIZE; i++) {
		if (!ulp_sc_info->data_va[i])
			continue;
		dma_free_coherent(&ctxt->bp->pdev->dev, ULP_SC_PAGE_SIZE,
				  ulp_sc_info->data_va[i],
				  ulp_sc_info->data_pa[i]);
	}

	kfree(ulp_sc_info->batch_info);
	kfree(ulp_sc_info);
}

#define ULP_SC_US_PER_MS 1000000
#define ULP_SC_PERIOD_S 1
#define ULP_SC_PERIOD_MS (ULP_SC_PERIOD_S * 1000)
#define ULP_SC_PERIOD_NS (ULP_SC_PERIOD_MS * ULP_SC_US_PER_MS)

/* Loop through the stats cache looking for valid entries and reading them in
 * batches of 64. The collected stats are saved in the cache and can be read
 * by an application directly from the cache.
 *
 * Tests have shown that batching up to 64 MPC requests give optimal MPC
 * performance and yeilds an approximately 10x improvement over single
 * MPC requests.
 */
static void
ulp_stats_cache_alarm_cb(struct work_struct *work)
{
	u16 words = DIV_ROUND_UP(ULP_TFC_CNTR_READ_BYTES, ULP_TFC_ACT_WORD_SZ);
	struct ulp_sc_tfc_stats_cache_entry *sce_end;
	struct bnxt_ulp_sc_info *ulp_sc_info = NULL;
	struct ulp_sc_tfc_stats_cache_entry *count;
	const struct bnxt_ulp_sc_core_ops *sc_ops;
	struct ulp_sc_tfc_stats_cache_entry *sce;
	struct bnxt_ulp_context *ctxt = NULL;
	struct delayed_work *sc_work = NULL;
	struct bnxt_ulp_data *cfg_data;
	struct tfc *tfcp = NULL;
	static u64 max_total;
	static u32 delay;
	struct bnxt *bp;
	u32 batch_size;
	ktime_t total;
	ktime_t start;
	ktime_t end;
	u64 *cptr;
	u32 batch;
	int rc;

	start = ktime_get_boottime();

	cfg_data = container_of(work, struct bnxt_ulp_data, sc_work.work);
	sc_work = &cfg_data->sc_work;

	bnxt_ulp_cntxt_lock_acquire();
	ctxt = bnxt_ulp_cntxt_entry_lookup(cfg_data);
	if (!ctxt)
		goto terminate2;
	if (!ctxt->cfg_data)
		goto terminate2;

	bp = ctxt->bp;
	if (test_bit(BNXT_STATE_IN_FW_RESET, &bp->state))
		goto terminate2;

	mutex_lock(&ctxt->cfg_data->sc_lock);

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);
	if (!ulp_sc_info)
		goto terminate1;

	sc_ops = ulp_sc_info->sc_ops;

	sce = ulp_sc_info->stats_cache_tbl;
	sce_end = sce + bnxt_ulp_sc_num_counters(ulp_sc_info->num_counters);

	if (!ulp_sc_info->num_entries)
		goto terminate1;

	netdev_dbg(ctxt->bp->dev, "Stats cache %d flows sce:%p scp-end:%p num:%d\n",
		   ulp_sc_info->num_entries, sce, sce_end,
		   ulp_sc_info->num_counters);

	while (ulp_sc_info->num_entries && (sce < sce_end)) {

		rc = tfc_mpc_batch_start(ulp_sc_info->batch_info);
		if (rc) {
			netdev_dbg(bp->dev,
				   "MPC batch start failed rc:%d\n", rc);
			goto err;
		}

		for (batch = 0; (batch < ULP_SC_BATCH_SIZE) && (sce < sce_end);) {
			if (!(sce->flags & ULP_SC_ENTRY_FLAG_VALID)) {
				sce++;
				continue;
			}

			tfcp = ctxt->ops->ulp_tfp_get(sce->ctxt,
						      BNXT_ULP_SESSION_TYPE_DEFAULT);

			if (!tfcp)
				goto err;

			/* Store the entry pointer to use for counter update */
			ulp_sc_info->batch_info->em_hdl[ulp_sc_info->batch_info->count] = (u64)sce;

			rc = sc_ops->ulp_stats_cache_update(tfcp,
							    sce->dir,
							    ulp_sc_info->data_pa[batch],
							    sce->handle,
							    &words,
							    ulp_sc_info->batch_info,
							    true);
			if (rc) {
				if (rc == -ENETDOWN) {
					/* If the interface is down then ignore and move on */
					sce++;
					continue;
				}

				netdev_dbg(bp->dev, "read_counter() failed:%d\n", rc);
				tfc_mpc_batch_end(bp, tfcp, ulp_sc_info->batch_info);
				goto err;
			}

			if (sce->reset)
				sce->reset = false;

			/* Next */
			batch++;
			sce++;
		}

		batch_size = ulp_sc_info->batch_info->count;
		rc = tfc_mpc_batch_end(bp, tfcp, ulp_sc_info->batch_info);
		netdev_dbg(bp->dev,
			   "MPC batch end size:%d\n",
			   batch_size);
		if (rc) {
			netdev_dbg(bp->dev,
				   "MPC batch end failed rc:%d\n",
				   rc);
			ulp_sc_info->batch_info->enabled = false;
			break;
		}

		for (batch = 0; batch < batch_size; batch++) {
			/* Check for error in completion */
			if (ulp_sc_info->batch_info->result[batch]) {
				netdev_dbg(bp->dev,
					   "batch:%d result:%d\n",
					   batch,
					   ulp_sc_info->batch_info->result[batch]);
			} else {
				count =
		(struct ulp_sc_tfc_stats_cache_entry *)ulp_sc_info->batch_info->em_hdl[batch];
				cptr = ulp_sc_info->data_va[batch];
				/* Only update if there's something to update */
				if (*cptr != 0) {
					count->count_fields.packet_count += *cptr;
					cptr++;
					count->count_fields.byte_count += *cptr;
				}
				netdev_dbg(ctxt->bp->dev,
					   "Stats cache count:%llu\n",
					   count->count_fields.packet_count);
			}
		}
	}

err:
	if (sc_work) {
		end = ktime_get_boottime();
		total = end - start;
		if (total > ULP_SC_PERIOD_NS)
			delay = ULP_SC_PERIOD_MS;
		else
			delay = (ULP_SC_PERIOD_NS - delay) / ULP_SC_US_PER_MS;

		if (total > max_total) {
			max_total = total;
			if (ctxt)
				netdev_dbg(ctxt->bp->dev, "Stats cache max_time:%llu\n", max_total);
		}

		if (ctxt && ctxt->cfg_data)
			queue_delayed_work(ctxt->cfg_data->sc_wq,
					   sc_work,
					   msecs_to_jiffies(delay));
	}

	mutex_unlock(&ctxt->cfg_data->sc_lock);
	bnxt_ulp_cntxt_lock_release();
	return;

terminate1:
	mutex_unlock(&ctxt->cfg_data->sc_lock);
terminate2:
	if (ulp_sc_info)
		ulp_sc_info->flags &= ~ULP_FLAG_SC_THREAD;

	bnxt_ulp_cntxt_lock_release();
}

/* Setup the Flow counter timer thread that will fetch/accumulate raw counter
 * data from the chip's internal flow counters
 *
 * SC will use it's own work queue because the amount of time spent
 * processing the flow stats will be dependent on the number of
 * configured flows and since the number of flows can be large the
 * processing time can be significant. By using it's own work queue
 * the risk is removed of SC blocking other time critical tasks
 * performing their operations in a timely manor.
 *
 * ctxt [in] The ulp context for the flow counter manager
 */
int
ulp_sc_mgr_thread_start(struct bnxt_ulp_context *ctxt)
{
	struct delayed_work *work = &ctxt->cfg_data->sc_work;
	struct bnxt_ulp_sc_info *ulp_sc_info;

	netdev_dbg(ctxt->bp->dev, "Thread start\n");

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);
	if (!ulp_sc_info) {
		netdev_dbg(ctxt->bp->dev, "Unable to get ulp sc info.\n");
		return -ENODEV;
	}

	ctxt->cfg_data->sc_wq = create_singlethread_workqueue("bnxt_ulp_sc_wq");
	if (!ctxt->cfg_data->sc_wq) {
		netdev_err(ctxt->bp->dev, "Unable to create workqueue.\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(work, ulp_stats_cache_alarm_cb);
	queue_delayed_work(ctxt->cfg_data->sc_wq, work, msecs_to_jiffies(ULP_SC_PERIOD_MS));

	ulp_sc_info->flags |= ULP_FLAG_SC_THREAD;
	return 0;
}

/* Check if the alarm thread that walks through the flows is started
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 */
bool ulp_sc_mgr_thread_isstarted(struct bnxt_ulp_context *ctxt)
{
	struct bnxt_ulp_sc_info *ulp_sc_info;

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);

	if (ulp_sc_info)
		return !!(ulp_sc_info->flags & ULP_FLAG_SC_THREAD);

	return false;
}

/* Return the number of packets and bytes for the specified
 * flow from the stats cache.
 *
 * ctxt [in] The ulp context for the flow counter manager
 *
 * flow_id [in] The HW flow ID
 *
 * packets [out] Number of packets
 *
 * bytes [out] Number of bytes
 *
 * lastused [out] not used
 */
int ulp_sc_mgr_query_count_get(struct bnxt_ulp_context *ctxt,
			       u32 flow_id,
			       u64 *packets,
			       u64 *bytes,
			       unsigned long *lastused)
{
	struct ulp_sc_tfc_stats_cache_entry *sce;
	struct bnxt_ulp_sc_info *ulp_sc_info;
	int rc = 0;

	mutex_lock(&ctxt->cfg_data->sc_lock);

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ctxt);
	if (!ulp_sc_info) {
		rc = -ENODEV;
		goto unlock;
	}

	sce = ulp_sc_info->stats_cache_tbl;
	if (!sce) {
		rc = -ENODEV;
		goto unlock;
	}

	if (flow_id > ulp_sc_info->num_counters) {
		rc = -EINVAL;
		goto unlock;
	}

	sce += flow_id;

	/* If entry is not valid return an error */
	if (!(sce->flags & ULP_SC_ENTRY_FLAG_VALID)) {
		rc = -EBUSY;
		goto unlock;
	}

	*packets = sce->count_fields.packet_count;
	*bytes   = sce->count_fields.byte_count;
	sce->count_fields.packet_count = 0;
	sce->count_fields.byte_count = 0;
	*lastused = jiffies;

 unlock:
	mutex_unlock(&ctxt->cfg_data->sc_lock);
	return rc;
}

int ulp_sc_mgr_entry_alloc(struct bnxt_ulp_mapper_parms *parms,
			   u64 counter_handle,
			   struct bnxt_ulp_mapper_tbl_info *tbl)
{
	struct ulp_sc_tfc_stats_cache_entry *sce;
	struct bnxt_ulp_sc_info *ulp_sc_info;
	int rc = 0;

	mutex_lock(&parms->ulp_ctx->cfg_data->sc_lock);

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(parms->ulp_ctx);
	if (!ulp_sc_info) {
		rc = -ENODEV;
		goto unlock;
	}

	sce = ulp_sc_info->stats_cache_tbl;
	sce += parms->flow_id;

	/* If entry is not free return an error */
	if (sce->flags & ULP_SC_ENTRY_FLAG_VALID) {
		rc = -EBUSY;
		goto unlock;
	}

	memset(sce, 0, sizeof(*sce));
	sce->ctxt = parms->ulp_ctx;
	sce->flags |= ULP_SC_ENTRY_FLAG_VALID;
	sce->handle = counter_handle;
	sce->dir = tbl->direction;
	ulp_sc_info->num_entries++;

 unlock:
	mutex_unlock(&parms->ulp_ctx->cfg_data->sc_lock);
	return rc;
}

void ulp_sc_mgr_entry_free(struct bnxt_ulp_context *ulp,
			   u32 fid)
{
	struct ulp_sc_tfc_stats_cache_entry *sce;
	struct bnxt_ulp_sc_info *ulp_sc_info;

	mutex_lock(&ulp->cfg_data->sc_lock);

	ulp_sc_info = bnxt_ulp_cntxt_ptr2_sc_info_get(ulp);
	if (!ulp_sc_info)
		goto unlock;

	sce = ulp_sc_info->stats_cache_tbl;
	sce += fid;

	if (!(sce->flags & ULP_SC_ENTRY_FLAG_VALID))
		goto unlock;

	sce->flags = 0;
	ulp_sc_info->num_entries--;

 unlock:
	mutex_unlock(&ulp->cfg_data->sc_lock);
}
