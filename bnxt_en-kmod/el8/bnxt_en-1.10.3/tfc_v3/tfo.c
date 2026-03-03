// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include "bnxt_compat.h"
#include "bnxt.h"
#include "tfo.h"
#include "tfc_util.h"
#include "cfa_types.h"
#include "cfa_tim.h"
#include "cfa_tpm.h"

#define TFC_INVALID_FID 0xff

struct tfc_ts_pool_info {
	u16 max_contig_rec;	/* max contig records */
	u8 pool_sz_exp;		/* pool size exp */
};

/* Only 1 single CPM instance per region per scope for all scope types
 * except for the global table scope.  In the global table scope, if there
 * is a single device driver supporting multiple physical ports, each
 * function must have its own CPM instance associated with the scope.
 */
struct tfc_cpm_info {
	struct tfc_cpm *cpm; /* pointer to the CFA pool manager db */
	u16 fid; /* function associated with this CPM instance */
};

/* The maximum number of CPM instances per table scope */
#define MAX_CPM_INSTANCES 2

struct tfc_cpms {
	struct tfc_cpm_info cpm_info[MAX_CPM_INSTANCES];
};

/* Table scope stored configuration */
struct tfc_tsid_db {
	bool ts_valid;			/* Table scope is valid */
	enum cfa_scope_type scope_type;	/* non-shared, shared-app, global */
	bool ts_is_bs_owner;		/* Backing store alloced by this instance (PF) */
	u16 ts_max_pools;		/* maximum pools per CPM instance */
	enum cfa_app_type ts_app;	/* application type TF/AFM */
	struct tfc_ts_mem_cfg ts_mem[CFA_REGION_TYPE_MAX][CFA_DIR_MAX]; /* backing store mem cfg */
	struct tfc_ts_pool_info ts_pool[CFA_REGION_TYPE_MAX][CFA_DIR_MAX]; /* pool sizing */
	struct tfc_cpms ts_cpm[CFA_REGION_TYPE_MAX][CFA_DIR_MAX]; /* CFA pool managers per fid */
};

/* Only a single global scope is allowed
 */
#define TFC_GLOBAL_SCOPE_MAX 1

/* Linked list of all global objects (only 1 global object per device) */
HLIST_HEAD(tfc_global_obj_list);

/* Mutex to synchronize tfc global object operations */
DEFINE_MUTEX(tfc_global_mutex);

struct bnxt_tfc_pci_info {
	u32	domain;
	u8	bus;
};

/* TFC Global Object
 * The global object is not per port, it is global.  It is only
 * used when a global table scope is created.  There is a single global
 * table scope per device allowed.
 */
#define DEVICE_SERIAL_NUM_SIZE 8
#define BOARD_SERIAL_NUM_SIZE 32

struct tfc_global_object {
	struct hlist_node next;				/* next device's global scope */
	struct bnxt_tfc_pci_info pci_info;		/* device info */
	struct mutex global_mutex;			/* lock for global db */
	u8 client_cnt;					/* global scope dev clients */
	u8 dsn[DEVICE_SERIAL_NUM_SIZE];			/* device serial number */
	u8 bsn[BOARD_SERIAL_NUM_SIZE];			/* board serial number */
	u8 gtsid;					/* global tsid */
	struct tfc_tsid_db gtsid_db;			/* global db */
	void *gts_tim;					/* a single global ts instance */
};

/* TFC Object Signature
 * This signature identifies the tfc object database and
 * is used for pointer validation
 */
#define TFC_OBJ_SIGNATURE 0xABACABAF

/* TFC Object
 * This data structure contains all data stored per bnxt port
 * Access is restricted through set/get APIs.
 *
 * If a module (e.g. tbl_scope needs to store data, it should
 * be added here and accessor functions created.
 */
struct tfc_object {
	u32 signature;					/* TF object signature */
	u16 sid;					/* Session ID */
	bool is_pf;					/* port is a PF */
	struct cfa_bld_mpcinfo mpc_info;		/* MPC ops handle */
	struct tfc_tsid_db tsid_db[TFC_TBL_SCOPE_MAX];	/* tsid database */
	/* TIM instance pointer (PF) - this is where the 4 instances
	 *  of the TPM (rx/tx_lkup, rx/tx_act) will be stored per
	 *  table scope.  Only valid on a PF.
	 */
	void *ts_tim;
	struct tfc_global_object *tfgo;			/* pointer to global */
};

/* Check if an global object is already allocated for a specific PCI
 * domain & bus. If it is already allocated simply return the tfgo
 * pointer, otherwise return NULL.
 */
static struct tfc_global_object *tfc_get_global_obj(struct bnxt *bp)
{
	struct tfc_global_object *tfgo;
	struct hlist_node *node;

	hlist_for_each_entry_safe(tfgo, node, &tfc_global_obj_list, next) {
		if (bp->flags & BNXT_FLAG_DSN_VALID) {
			if (!memcmp(tfgo->dsn, bp->dsn, sizeof(bp->dsn)))
				return tfgo;
		} else {
			if (!memcmp(tfgo->bsn, bp->board_serialno,
				    sizeof(bp->board_serialno)))
				return tfgo;
		}
	}
	return NULL;
}

/* Allocate and Initialize a TFC global object.
 * If it's already initialized increment client count and return the already
 * existing object.
 */
static struct tfc_global_object *tfc_global_obj_init(struct bnxt *bp)
{
	struct tfc_global_object *tfgo;
	enum cfa_region_type region;
	enum cfa_dir dir;
	u32 tim_db_size;
	int i, rc;

	mutex_lock(&tfc_global_mutex);
	tfgo = tfc_get_global_obj(bp);
	if (!tfgo) {
		netdev_dbg(bp->dev, "Global object not initialized, allocate it\n");
		/* Global object not found, allocate a new one */
		tfgo = vzalloc(sizeof(*tfgo));
		if (!tfgo) {
			mutex_unlock(&tfc_global_mutex);
			return NULL;
		}
		/* Add it to the queue */
		if (bp->flags & BNXT_FLAG_DSN_VALID)
			memcpy(tfgo->dsn, bp->dsn, sizeof(bp->dsn));
		else
			memcpy(tfgo->bsn, bp->board_serialno,
			       sizeof(bp->board_serialno));
		mutex_init(&tfgo->global_mutex);
		hlist_add_head(&tfgo->next, &tfc_global_obj_list);
		tfgo->client_cnt = 0;

		/* Initialize the global table scope object */
		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
				for (i = 0; i < MAX_CPM_INSTANCES; i++) {
					tfgo->gtsid_db.ts_cpm[region][dir].cpm_info[i].fid =
						TFC_INVALID_FID;
					tfgo->gtsid_db.ts_cpm[region][dir].cpm_info[i].cpm =
						NULL;
				}
			}
		}
		if (!tfgo->gts_tim) {
			/* Allocate global scope TIM database */
			rc = cfa_tim_query(TFC_TBL_SCOPE_MAX, CFA_REGION_TYPE_MAX,
					   &tim_db_size);
			if (rc)
				goto cleanup;

			tfgo->gts_tim = kzalloc(tim_db_size, GFP_KERNEL);
			if (!tfgo->gts_tim)
				goto cleanup;

			rc = cfa_tim_open(tfgo->gts_tim,
					  tim_db_size,
					  TFC_TBL_SCOPE_MAX,
					  CFA_REGION_TYPE_MAX);
			if (rc) {
				kfree(tfgo->gts_tim);
				tfgo->gts_tim = NULL;
				goto cleanup;
			}
		}
		tfgo->gtsid = INVALID_TSID;
	}
	tfgo->client_cnt++;
	netdev_dbg(bp->dev, "Global object initiaiized clients(%d)\n",
		   tfgo->client_cnt);

	mutex_unlock(&tfc_global_mutex);

	return tfgo;
cleanup:
	if (tfgo) {
		if (tfgo->gts_tim) {
			cfa_tim_close(tfgo->gts_tim);
			kfree(tfgo->gts_tim);
			tfgo->gts_tim = NULL;
		}
		/* Remove from list if it was added */
		if (tfgo->client_cnt == 0) {
			hlist_del(&tfgo->next);
			mutex_destroy(&tfgo->global_mutex);
			vfree(tfgo);
		}
	}
	mutex_unlock(&tfc_global_mutex);
	return NULL;
}

/* Remove the client from the global object and clean up if the last entry
 */
static void tfc_global_obj_deinit(struct bnxt *bp)
{
	struct tfc_global_object *tfgo;

	tfgo = tfc_get_global_obj(bp);
	if (!tfgo)
		return;

	mutex_lock(&tfc_global_mutex);

	if (tfgo->client_cnt) {
		netdev_dbg(bp->dev, "%s: client_cnt(%d)\n", __func__, tfgo->client_cnt);
		tfgo->client_cnt--;
	}

	if (tfgo->client_cnt == 0) {
		netdev_dbg(bp->dev, "%s: last client removed\n", __func__);
		if (tfgo->gts_tim) {
			cfa_tim_close(tfgo->gts_tim);
			kfree(tfgo->gts_tim);
		}
		hlist_del(&tfgo->next);
		mutex_destroy(&tfgo->global_mutex);
		vfree(tfgo);
	}
	mutex_unlock(&tfc_global_mutex);
}

/* Get the correct tsid database given the table scope id */
static inline struct tfc_tsid_db *get_tsid_db(struct tfc_object *tfco, u8 ts_tsid)
{
	struct tfc_global_object *tfgo;

	tfgo = tfco->tfgo;
	if (tfgo && tfgo->gtsid == ts_tsid)
		return &tfgo->gtsid_db;
	else
		return &tfco->tsid_db[ts_tsid];
}

void tfo_open(void **tfo, struct bnxt *bp, bool is_pf)
{
	struct tfc_object *tfco = NULL;
	enum cfa_region_type region;
	struct tfc_tsid_db *tsid_db;
	enum cfa_dir dir;
	u32 tim_db_size;
	int i, id, rc;

	tfco = kzalloc(sizeof(*tfco), GFP_KERNEL);
	if (!tfco)
		return;

	tfco->signature = TFC_OBJ_SIGNATURE;
	tfco->is_pf = is_pf;
	tfco->sid = INVALID_SID;
	tfco->ts_tim = NULL;

	/* Bind to the MPC builder */
	rc = cfa_bld_mpc_bind(CFA_P70, &tfco->mpc_info);
	if (rc) {
		netdev_dbg(NULL, "%s: MPC bind failed\n", __func__);
		goto cleanup;
	}
	if (is_pf) {
		/* Allocate per bp TIM database */
		rc = cfa_tim_query(TFC_TBL_SCOPE_MAX, CFA_REGION_TYPE_MAX,
				   &tim_db_size);
		if (rc)
			goto cleanup;

		tfco->ts_tim = kzalloc(tim_db_size, GFP_KERNEL);
		if (!tfco->ts_tim)
			goto cleanup;

		rc = cfa_tim_open(tfco->ts_tim,
				  tim_db_size,
				  TFC_TBL_SCOPE_MAX,
				  CFA_REGION_TYPE_MAX);
		if (rc) {
			kfree(tfco->ts_tim);
			tfco->ts_tim = NULL;
			goto cleanup;
		}
	}

	/* Initialize all table scopes in the tfo object */
	for (id = 0; id < TFC_TBL_SCOPE_MAX; id++) {
		tsid_db = &tfco->tsid_db[id];
		for (dir = 0; dir < CFA_DIR_MAX; dir++) {
			for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
				for (i = 0; i < MAX_CPM_INSTANCES; i++) {
					tsid_db->ts_cpm[region][dir].cpm_info[i].fid =
						TFC_INVALID_FID;
					tsid_db->ts_cpm[region][dir].cpm_info[i].cpm = NULL;
				}
			}
		}
	}

	/* Initialize the global table scope object */
	tfco->tfgo = tfc_global_obj_init(bp);
	if (!tfco->tfgo) {
		netdev_err(bp->dev, "global object init failure\n");
		goto cleanup;
	}
	*tfo = tfco;
	return;

cleanup:
	if (tfco->tfgo)
		tfc_global_obj_deinit(bp);
	if (tfco->ts_tim) {
		cfa_tim_close(tfco->ts_tim);
		kfree(tfco->ts_tim);
	}
	kfree(tfco);
	*tfo = NULL;
}

void tfo_close(void **tfo, struct bnxt *bp)
{
	struct tfc_object *tfco = (struct tfc_object *)(*tfo);
	void *tim = NULL, *tpm = NULL;
	enum cfa_region_type region;
	int dir, rc, tsid;

	if (*tfo && tfco->signature == TFC_OBJ_SIGNATURE) {

		/* This loop will clean up both the global and the
		 * tfo table scope instances as tfo_tim_get() will
		 * get the appropriate tim instance.
		 */
		for (tsid = 0; tsid < TFC_TBL_SCOPE_MAX; tsid++) {
			if (tfo_tim_get(*tfo, &tim, tsid))
				continue;
			if (!tim)
				continue;
			/* If the client count is 1, this is the last instance */
			if (tfco->tfgo && (tfco->tfgo->gtsid == tsid) &&
			    tfco->tfgo->client_cnt > 1)
				continue;
			for (region = 0; region < CFA_REGION_TYPE_MAX; region++) {
				for (dir = 0; dir < CFA_DIR_MAX; dir++) {
					tpm = NULL;
					rc = cfa_tim_tpm_inst_get(tim, tsid, region, dir, &tpm);
					if (!rc && tpm) {
						cfa_tpm_close(tpm);
						kfree(tpm);
						cfa_tim_tpm_inst_set(tim, tsid, region, dir, NULL);
					}
				}
			}
		}
		kfree(tfco->ts_tim);
		tfco->ts_tim = NULL;
		tfc_global_obj_deinit(bp);
		tfco->tfgo = NULL;
		kfree(*tfo);
		*tfo = NULL;
	}
}

int tfo_mpcinfo_get(void *tfo, struct cfa_bld_mpcinfo **mpc_info)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;

	if (!tfo)
		return -EINVAL;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}

	*mpc_info = &tfco->mpc_info;

	return 0;
}

int tfo_ts_validate(void *tfo, u8 ts_tsid, bool *ts_valid)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_tsid_db *tsid_db;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}

	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		netdev_dbg(NULL, "%s: Invalid tsid %d\n", __func__, ts_tsid);
		return -EINVAL;
	}

	tsid_db = get_tsid_db(tfo, ts_tsid);
	if (ts_valid)
		*ts_valid = tsid_db->ts_valid;

	return 0;
}

int tfo_ts_set(void *tfo, u8 ts_tsid, enum cfa_scope_type scope_type,
	       enum cfa_app_type ts_app, bool ts_valid, u16 ts_max_pools)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;
	struct tfc_tsid_db *tsid_db;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}

	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		netdev_dbg(NULL, "%s: Invalid tsid %d\n", __func__, ts_tsid);
		return -EINVAL;
	}

	/* Set the correct tsid database based upon whether
	 * the scope type passed in indicates global
	 */
	tfgo = tfco->tfgo;
	if (!tfgo) {
		netdev_dbg(NULL, "Global object not initialized\n");
		return -ENODATA;
	}
	if (scope_type == CFA_SCOPE_TYPE_GLOBAL) {
		tsid_db = &tfgo->gtsid_db;
		tfgo->gtsid = ts_tsid;
	} else if (scope_type == CFA_SCOPE_TYPE_INVALID && tfgo &&
		   ts_tsid == tfgo->gtsid) {
		tfgo->gtsid = INVALID_TSID;
		tsid_db = &tfgo->gtsid_db;
	} else {
		tsid_db = &tfco->tsid_db[ts_tsid];
	}

	tsid_db->ts_valid = ts_valid;
	tsid_db->scope_type = scope_type;
	tsid_db->ts_app = ts_app;
	tsid_db->ts_max_pools = ts_max_pools;

	return 0;
}

int tfo_ts_get(void *tfo, u8 ts_tsid, enum cfa_scope_type *scope_type,
	       enum cfa_app_type *ts_app, bool *ts_valid,
	       u16 *ts_max_pools)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_tsid_db *tsid_db;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}

	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		netdev_dbg(NULL, "%s: Invalid tsid %d\n", __func__, ts_tsid);
		return -EINVAL;
	}

	tsid_db = get_tsid_db(tfo, ts_tsid);
	if (ts_valid)
		*ts_valid = tsid_db->ts_valid;

	if (scope_type)
		*scope_type = tsid_db->scope_type;

	if (ts_app)
		*ts_app = tsid_db->ts_app;

	if (ts_max_pools)
		*ts_max_pools = tsid_db->ts_max_pools;

	return 0;
}

/* Set the table scope memory configuration for this direction */
int tfo_ts_set_mem_cfg(void *tfo, uint8_t ts_tsid, enum cfa_dir dir,
		       enum cfa_region_type region, bool is_bs_owner,
		       struct tfc_ts_mem_cfg *mem_cfg)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_tsid_db *tsid_db;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}

	if (!mem_cfg) {
		netdev_dbg(NULL, "%s: Invalid mem_cfg pointer\n", __func__);
		return -EINVAL;
	}

	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		netdev_dbg(NULL, "%s: Invalid tsid %d\n", __func__, ts_tsid);
		return -EINVAL;
	}

	tsid_db = get_tsid_db(tfo, ts_tsid);

	tsid_db->ts_mem[region][dir] = *mem_cfg;
	tsid_db->ts_is_bs_owner = is_bs_owner;

	return 0;
}

/* Get the table scope memory configuration for this direction */
int tfo_ts_get_mem_cfg(void *tfo, u8 ts_tsid, enum cfa_dir dir,
		       enum cfa_region_type region, bool *is_bs_owner,
		       struct tfc_ts_mem_cfg *mem_cfg)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_tsid_db *tsid_db;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}

	if (!mem_cfg) {
		netdev_dbg(NULL, "%s: Invalid mem_cfg pointer\n", __func__);
		return -EINVAL;
	}

	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		netdev_dbg(NULL, "%s: Invalid tsid %d\n", __func__, ts_tsid);
		return -EINVAL;
	}

	tsid_db = get_tsid_db(tfo, ts_tsid);

	*mem_cfg = tsid_db->ts_mem[region][dir];
	if (is_bs_owner)
		*is_bs_owner = tsid_db->ts_is_bs_owner;

	return 0;
}

/* Get the Pool Manager instance */
int tfo_ts_get_cpm_inst(void *tfo, u8 ts_tsid, u16 fid, enum cfa_dir dir,
			enum cfa_region_type region, struct tfc_cpm **cpm)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_tsid_db *tsid_db;
	struct tfc_cpms *ts_cpm;
	bool found;
	int i;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}
	if (!cpm) {
		netdev_dbg(NULL, "%s: Invalid cpm pointer\n", __func__);
		return -EINVAL;
	}

	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		netdev_dbg(NULL, "%s: Invalid tsid %d\n", __func__, ts_tsid);
		return -EINVAL;
	}
	if (dir >= CFA_DIR_MAX) {
		netdev_dbg(NULL, "%s: Invalid dir %d\n", __func__, ts_tsid);
		return -EINVAL;
	}
	if (region >= CFA_REGION_TYPE_MAX) {
		netdev_dbg(NULL, "%s: Invalid region %d\n", __func__, ts_tsid);
		return -EINVAL;
	}
	tsid_db = get_tsid_db(tfo, ts_tsid);

	ts_cpm = &tsid_db->ts_cpm[region][dir];

	found = 0;
	for (i = 0; i < MAX_CPM_INSTANCES; i++) {
		if (fid != TFC_INVALID_FID && ts_cpm->cpm_info[i].fid == fid) {
			*cpm = ts_cpm->cpm_info[i].cpm;
			netdev_dbg(NULL, "%s: tsid:%d_%s CPM[%d] entry found fid(%d) cpm(%p)\n",
				   __func__, ts_tsid, tfc_ts_region_2_str(region, dir), i, fid,
				   *cpm);
			found = 1;
			break;
		}
	}
	if (!found) {
		netdev_dbg(NULL, "%s: no CPM entry found for fid(%d)\n", __func__,
			   fid);
		return -EINVAL;
	}
	return 0;
}

/* Set the Pool Manager instance */
int tfo_ts_set_cpm_inst(void *tfo, u8 ts_tsid, u16 fid, enum cfa_dir dir,
			enum cfa_region_type region, struct tfc_cpm *cpm)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_tsid_db *tsid_db;
	struct tfc_cpms *ts_cpm;
	bool found;
	int i;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}

	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		netdev_dbg(NULL, "%s: Invalid tsid %d\n", __func__, ts_tsid);
		return -EINVAL;
	}
	if (dir >= CFA_DIR_MAX) {
		netdev_dbg(NULL, "%s: Invalid dir %d\n", __func__, ts_tsid);
		return -EINVAL;
	}
	if (region >= CFA_REGION_TYPE_MAX) {
		netdev_dbg(NULL, "%s: Invalid region %d\n", __func__, ts_tsid);
		return -EINVAL;
	}

	tsid_db = get_tsid_db(tfo, ts_tsid);

	ts_cpm = &tsid_db->ts_cpm[region][dir];

	found = 0;
	for (i = 0; i < MAX_CPM_INSTANCES; i++) {
		/* search for matching fid */
		if (fid != TFC_INVALID_FID && ts_cpm->cpm_info[i].fid == fid) {
			ts_cpm->cpm_info[i].fid = fid;
			ts_cpm->cpm_info[i].cpm = cpm;
			found = 1;
			netdev_dbg(NULL, "%s: tsid:%d_%s CPM[%d] entry found fid(%d) cpm(%p)\n",
				   __func__, ts_tsid, tfc_ts_region_2_str(region, dir), i, fid,
				   cpm);
			break;
		}
	}
	if (!found) {
		/* No matching fid, search for empty entry indicated by INVALID_FID */
		for (i = 0; i < MAX_CPM_INSTANCES; i++) {
			if (ts_cpm->cpm_info[i].fid == TFC_INVALID_FID) {
				ts_cpm->cpm_info[i].cpm = cpm;
				ts_cpm->cpm_info[i].fid = fid;
				found = 1;
				netdev_dbg(NULL, "%s: tsid:%d_%s CPM[%d] add fid(%d) cpm(%p)\n",
					   __func__, ts_tsid, tfc_ts_region_2_str(region, dir), i,
					   fid, cpm);
				break;
			}
		}
	}
	/* Neither an empty or a matching entry was found */
	if (!found) {
		netdev_dbg(NULL, "%s: no tsid(%d) entry found for fid(%d) region(%s)\n",
			   __func__, fid, ts_tsid, tfc_ts_region_2_str(region, dir));
		return -EINVAL;
	}
	return 0;
}

/* Set the table scope pool memory configuration for this direction */
int tfo_ts_set_pool_info(void *tfo, u8 ts_tsid, enum cfa_dir dir,
			 enum cfa_region_type region, u16 max_contig_rec,
			 u8 pool_sz_exp)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_tsid_db *tsid_db;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}

	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		netdev_dbg(NULL, "%s: Invalid tsid %d\n", __func__, ts_tsid);
		return -EINVAL;
	}

	tsid_db = get_tsid_db(tfo, ts_tsid);
	tsid_db->ts_pool[region][dir].max_contig_rec = max_contig_rec;
	tsid_db->ts_pool[region][dir].pool_sz_exp = pool_sz_exp;
	netdev_dbg(NULL, "%s: tsid:%d_%s max_contig_rec(%d) pool_sz_exp(%d)\n", __func__, ts_tsid,
		   tfc_ts_region_2_str(region, dir), max_contig_rec, pool_sz_exp);
	return 0;
}

/* Get the table scope pool memory configuration for this direction */
int tfo_ts_get_pool_info(void *tfo, u8 ts_tsid, enum cfa_dir dir,
			 enum cfa_region_type region, u16 *max_contig_rec,
			 u8 *pool_sz_exp)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_tsid_db *tsid_db;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}
	if (ts_tsid >= TFC_TBL_SCOPE_MAX) {
		netdev_dbg(NULL, "%s: Invalid tsid %d\n", __func__, ts_tsid);
		return -EINVAL;
	}

	tsid_db = get_tsid_db(tfo, ts_tsid);
	if (max_contig_rec)
		*max_contig_rec = tsid_db->ts_pool[region][dir].max_contig_rec;
	if (pool_sz_exp)
		*pool_sz_exp = tsid_db->ts_pool[region][dir].pool_sz_exp;
	netdev_dbg(NULL, "%s: tsid:%d_%s max_contig_rec(%d) pool_sz_exp(%d)\n", __func__, ts_tsid,
		   tfc_ts_region_2_str(region, dir), max_contig_rec ? *max_contig_rec : 0,
		   pool_sz_exp ? *pool_sz_exp : 0);
	return 0;
}

int tfo_sid_set(void *tfo, uint16_t sid)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}

	if (tfco->sid != INVALID_SID && sid != INVALID_SID &&
	    tfco->sid != sid) {
		netdev_dbg(NULL, "%s: Cannot set SID %u, current session is %u\n",
			   __func__, sid, tfco->sid);
		return -EINVAL;
	}

	tfco->sid = sid;

	return 0;
}

int tfo_sid_get(void *tfo, uint16_t *sid)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}

	if (!sid) {
		netdev_dbg(NULL, "%s: Invalid sid pointer\n", __func__);
		return -EINVAL;
	}

	if (tfco->sid == INVALID_SID) {
		/* Session has not been created */
		return -ENODATA;
	}

	*sid = tfco->sid;

	return 0;
}

int tfo_tim_get(void *tfo, void **tim, u8 ts_tsid)
{
	struct tfc_object *tfco = (struct tfc_object *)tfo;
	struct tfc_global_object *tfgo;

	if (tfco->signature != TFC_OBJ_SIGNATURE) {
		netdev_dbg(NULL, "%s: Invalid tfo object\n", __func__);
		return -EINVAL;
	}

	if (!tim) {
		netdev_dbg(NULL, "%s: Invalid tim pointer to pointer\n", __func__);
		return -EINVAL;
	}

	/* Get the correct tsid database based upon whether
	 * the tsid passed in configured as the global tsid
	 */
	tfgo = tfco->tfgo;
	if (!tfgo) {
		netdev_dbg(NULL, "Invalid global object\n");
		return -ENODATA;
	}
	if (ts_tsid == tfgo->gtsid) {
		if (!tfgo->gts_tim)
		/* ts tim could be null, no need to log error message */
			return -ENODATA;
		*tim = tfgo->gts_tim;
	} else {
		if (!tfco->ts_tim)
			/* ts tim could be null, no need to log error message */
			return -ENODATA;
		*tim = tfco->ts_tim;
	}

	return 0;
}
