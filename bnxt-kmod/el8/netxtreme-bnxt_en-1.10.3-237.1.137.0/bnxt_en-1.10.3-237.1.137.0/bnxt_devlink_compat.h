/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */
#ifndef _BNXT_DEVLINK_COMPAT_H_
#define _BNXT_DEVLINK_COMPAT_H_

#ifdef HAVE_DEVLINK
#include <net/devlink.h>
#endif

#ifdef HAVE_DEVLINK_HEALTH_REPORT
#ifndef HAVE_DEVLINK_FMSG_STRING_PAIR_PUT_VOID
static int bnxt_fw_diagnose_compat(struct devlink_health_reporter *reporter,
				   struct devlink_fmsg *fmsg)
{
	struct bnxt *bp = devlink_health_reporter_priv(reporter);
	struct bnxt_fw_health *h = bp->fw_health;
	u32 fw_status, fw_resets;
	int rc;

	if (test_bit(BNXT_STATE_IN_FW_RESET, &bp->state))
		return devlink_fmsg_string_pair_put(fmsg, "Status", "recovering");

	if (!h->status_reliable)
		return devlink_fmsg_string_pair_put(fmsg, "Status", "unknown");

	mutex_lock(&h->lock);
	fw_status = bnxt_fw_health_readl(bp, BNXT_FW_HEALTH_REG);
	if (BNXT_FW_IS_BOOTING(fw_status)) {
		rc = devlink_fmsg_string_pair_put(fmsg, "Status", "initializing");
		if (rc)
			goto unlock;
	} else if (h->severity || fw_status != BNXT_FW_STATUS_HEALTHY) {
		if (!h->severity) {
			h->severity = SEVERITY_FATAL;
			h->remedy = REMEDY_POWER_CYCLE_DEVICE;
			h->diagnoses++;
			devlink_health_report(h->fw_reporter,
					      "FW error diagnosed", h);
		}
		rc = devlink_fmsg_string_pair_put(fmsg, "Status", "error");
		if (rc)
			goto unlock;
		rc = devlink_fmsg_u32_pair_put(fmsg, "Syndrome", fw_status);
		if (rc)
			goto unlock;
	} else {
		rc = devlink_fmsg_string_pair_put(fmsg, "Status", "healthy");
		if (rc)
			goto unlock;
	}

	rc = devlink_fmsg_string_pair_put(fmsg, "Severity",
					  bnxt_health_severity_str(h->severity));
	if (rc)
		goto unlock;

	if (h->severity) {
		rc = devlink_fmsg_string_pair_put(fmsg, "Remedy",
						  bnxt_health_remedy_str(h->remedy));
		if (rc)
			goto unlock;
		if (h->remedy == REMEDY_DEVLINK_RECOVER) {
			rc = devlink_fmsg_string_pair_put(fmsg, "Impact",
							  "traffic+ntuple_cfg");
			if (rc)
				goto unlock;
		}
	}

unlock:
	mutex_unlock(&h->lock);
	if (rc || !h->resets_reliable)
		return rc;

	fw_resets = bnxt_fw_health_readl(bp, BNXT_FW_RESET_CNT_REG);
	rc = devlink_fmsg_u32_pair_put(fmsg, "Resets", fw_resets);
	if (rc)
		return rc;
	rc = devlink_fmsg_u32_pair_put(fmsg, "Arrests", h->arrests);
	if (rc)
		return rc;
	rc = devlink_fmsg_u32_pair_put(fmsg, "Survivals", h->survivals);
	if (rc)
		return rc;
	rc = devlink_fmsg_u32_pair_put(fmsg, "Discoveries", h->discoveries);
	if (rc)
		return rc;
	rc = devlink_fmsg_u32_pair_put(fmsg, "Fatalities", h->fatalities);
	if (rc)
		return rc;
	return devlink_fmsg_u32_pair_put(fmsg, "Diagnoses", h->diagnoses);
}
#endif
#endif
#endif
