// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include "cfa_resource_types.h"
#include "tf_rm.h"
#include "tf_util.h"
#include "tf_session.h"
#include "tf_device.h"
#include "tf_msg.h"

/**
 * Generic RM Element data type that an RM DB is build upon.
 * @cfg_type:		RM Element configuration type. If Private then the
 *			hcapi_type can be ignored. If Null then the element
 *			is not valid for the device.
 * @hcapi_type:		HCAPI RM Type for the element.
 * @slices:		Resource slices.  How many slices will fit in the
 *			resource pool chunk size.
 * @alloc:		HCAPI RM allocated range information for the element.
 * @parent_subtype:	If cfg_type == HCAPI_BA_CHILD, this field indicates
 *			the parent module subtype for look up into the parent
 *			pool.
 *			An example subtype is TF_TBL_TYPE_FULL_ACT_RECORD
 *			which is a module subtype of TF_MODULE_TYPE_TABLE.
 * @pool:		Bit allocator pool for the element. Pool size is
 *			controlled by the struct tf_session_resources at
 *			time of session creation. Null indicates that the
 *			pool is not used for the element.
 */
struct tf_rm_element {
	enum tf_rm_elem_cfg_type	cfg_type;
	u16				hcapi_type;
	u8				slices;
	struct tf_rm_alloc_info		alloc;
	u16				parent_subtype;
	struct bitalloc			*pool;
};

/**
 * TF RM DB definition
 * @num_entries:	Number of elements in the DB
 * @dir:		Direction this DB controls.
 * @module:		Module type, used for logging purposes.
 * @db:			The DB consists of an array of elements
 */
struct tf_rm_new_db {
	u16			num_entries;
	enum tf_dir		dir;
	enum tf_module_type	module;
	struct tf_rm_element	*db;
};

/**
 * Adjust an index according to the allocation information.
 *
 * All resources are controlled in a 0 based pool. Some resources, by
 * design, are not 0 based, i.e. Full Action Records (SRAM) thus they
 * need to be adjusted before they are handed out.
 *
 * @cfg:		Pointer to the DB configuration
 * @reservations:	Pointer to the allocation values associated with
 *			the module
 * @count:		Number of DB configuration elements
 * @valid_count:	Number of HCAPI entries with a reservation value
 *			greater than 0
 *
 * Returns:
 *     0          - Success
 *   - EOPNOTSUPP - Operation not supported
 */
static void tf_rm_count_hcapi_reservations(enum tf_dir dir,
					   enum tf_module_type module,
					   struct tf_rm_element_cfg *cfg,
					   u16 *reservations, u16 count,
					   u16 *valid_count)
{
	u16 cnt = 0;
	int i;

	for (i = 0; i < count; i++) {
		if (cfg[i].cfg_type != TF_RM_ELEM_CFG_NULL &&
		    reservations[i] > 0)
			cnt++;

		/* Only log msg if a type is attempted reserved and
		 * not supported. We ignore EM module as its using a
		 * split configuration array thus it would fail for
		 * this type of check.
		 */
		if (module != TF_MODULE_TYPE_EM &&
		    cfg[i].cfg_type == TF_RM_ELEM_CFG_NULL &&
		    reservations[i] > 0) {
			netdev_dbg(NULL,
				   "%s %s %s allocation of %d unsupported\n",
				   tf_module_2_str(module), tf_dir_2_str(dir),
				   tf_module_subtype_2_str(module, i),
				   reservations[i]);
		}
	}

	*valid_count = cnt;
}

/* Resource Manager Adjust of base index definitions. */
enum tf_rm_adjust_type {
	TF_RM_ADJUST_ADD_BASE, /* Adds base to the index */
	TF_RM_ADJUST_RM_BASE   /* Removes base from the index */
};

/**
 * Adjust an index according to the allocation information.
 *
 * All resources are controlled in a 0 based pool. Some resources, by
 * design, are not 0 based, i.e. Full Action Records (SRAM) thus they
 * need to be adjusted before they are handed out.
 *
 * @db:		Pointer to the db, used for the lookup
 * @action:	Adjust action
 * @subtype:	TF module subtype used as an index into the database.
 *		An example subtype is TF_TBL_TYPE_FULL_ACT_RECORD which is a
 *		module subtype of TF_MODULE_TYPE_TABLE.
 * @index:	Index to convert
 * @adj_index:	Adjusted index
 *
 * Returns:
 *     0          - Success
 *   - EOPNOTSUPP - Operation not supported
 */
static int tf_rm_adjust_index(struct tf_rm_element *db,
			      enum tf_rm_adjust_type action, u32 subtype,
			      u32 index, u32 *adj_index)
{
	u32 base_index;
	int rc = 0;

	base_index = db[subtype].alloc.entry.start;

	switch (action) {
	case TF_RM_ADJUST_RM_BASE:
		*adj_index = index - base_index;
		break;
	case TF_RM_ADJUST_ADD_BASE:
		*adj_index = index + base_index;
		break;
	default:
		return -EOPNOTSUPP;
	}

	return rc;
}

/**
 * Logs an array of found residual entries to the console.
 *
 * @dir:	Receive or transmit direction
 * @module:	Type of Device Module
 * @count:	Number of entries in the residual array
 * @residuals:	Pointer to an array of residual entries. Array is index
 *		same as the DB in which this function is used. Each entry
 *		holds residual value for that entry.
 */
static void tf_rm_log_residuals(enum tf_dir dir, enum tf_module_type module,
				u16 count, u16 *residuals)
{
	int i;

	/* Walk the residual array and log the types that wasn't
	 * cleaned up to the console.
	 */
	for (i = 0; i < count; i++) {
		if (residuals[i] == 0)
			continue;
		netdev_dbg(NULL,
			   "%s, %s was not cleaned up, %d outstanding\n",
			   tf_dir_2_str(dir),
			   tf_module_subtype_2_str(module, i), residuals[i]);
	}
}

/**
 * Performs a check of the passed in DB for any lingering elements. If
 * a resource type was found to not have been cleaned up by the caller
 * then its residual values are recorded, logged and passed back in an
 * allocate reservation array that the caller can pass to the FW for
 * cleanup.
 *
 * @db:		Pointer to the db, used for the lookup
 * @resv_size:	Pointer to the reservation size of the generated reservation
 *		array.
 * resv:	Pointer to a reservation array. The reservation array is
 *		allocated after the residual scan and holds any found
 *		residual entries. Thus it can be smaller than the DB that
 *		the check was performed on. Array must be freed by the caller.
 * @residuals_present:	Pointer to a bool flag indicating if residual was
 *			present in the DB
 *
 * Returns:
 *     0          - Success
 *   - EOPNOTSUPP - Operation not supported
 */
static int tf_rm_check_residuals(struct tf_rm_new_db *rm_db, u16 *resv_size,
				 struct tf_rm_resc_entry **resv,
				 bool *residuals_present)
{
	struct tf_rm_resc_entry *local_resv = NULL;
	struct tf_rm_get_inuse_count_parms iparms;
	struct tf_rm_get_alloc_info_parms aparms;
	struct tf_rm_get_hcapi_parms hparms;
	struct tf_rm_alloc_info info;
	u16 *residuals = NULL;
	u16 hcapi_type;
	size_t len;
	u16 count;
	u16 found;
	int rc;
	int i;
	int f;

	*residuals_present = false;

	/* Create array to hold the entries that have residuals */
	len = rm_db->num_entries * sizeof(u16);
	residuals = vzalloc(len);
	if (!residuals)
		return -ENOMEM;

	/* Traverse the DB and collect any residual elements */
	iparms.rm_db = rm_db;
	iparms.count = &count;
	for (i = 0, found = 0; i < rm_db->num_entries; i++) {
		iparms.subtype = i;
		rc = tf_rm_get_inuse_count(&iparms);
		/* Not a device supported entry, just skip */
		if (rc == -EOPNOTSUPP)
			continue;
		if (rc)
			goto cleanup_residuals;

		if (count) {
			found++;
			residuals[i] = count;
			*residuals_present = true;
		}
	}

	if (*residuals_present) {
		/* Populate a reduced resv array with only the entries
		 * that have residuals.
		 */
		len = found * sizeof(struct tf_rm_resc_entry);
		local_resv = vzalloc(len);
		if (!local_resv) {
			rc = -ENOMEM;
			goto cleanup_residuals;
		}

		aparms.rm_db = rm_db;
		hparms.rm_db = rm_db;
		hparms.hcapi_type = &hcapi_type;
		for (i = 0, f = 0; i < rm_db->num_entries; i++) {
			if (residuals[i] == 0)
				continue;
			aparms.subtype = i;
			aparms.info = &info;
			rc = tf_rm_get_info(&aparms);
			if (rc)
				goto cleanup_all;

			hparms.subtype = i;
			rc = tf_rm_get_hcapi_type(&hparms);
			if (rc)
				goto cleanup_all;

			local_resv[f].type = hcapi_type;
			local_resv[f].start = info.entry.start;
			local_resv[f].stride = info.entry.stride;
			f++;
		}
		*resv_size = found;
	}

	tf_rm_log_residuals(rm_db->dir,
			    rm_db->module,
			    rm_db->num_entries,
			    residuals);
	vfree(residuals);
	*resv = local_resv;

	return 0;

 cleanup_all:
	vfree(local_resv);
	*resv = NULL;
 cleanup_residuals:
	vfree(residuals);

	return rc;
}

/**
 * Some resources do not have a 1:1 mapping between the Truflow type and the
 * cfa resource type (HCAPI RM). These resources have multiple Truflow types
 * which map to a single HCAPI RM type. In order to support this, one Truflow
 * type sharing the HCAPI resources is designated the parent. All other
 * Truflow types associated with that HCAPI RM type are designated the
 * children.
 *
 * This function updates the resource counts of any HCAPI_BA_PARENT with the
 * counts of the HCAPI_BA_CHILDREN.  These are read from the alloc_cnt and
 * written back to the req_cnt.
 *
 * @cfg:	Pointer to an array of module specific Truflow type indexed
 *		RM cfg items
 * @alloc_cnt:	Pointer to the tf_open_session() configured array of module
 *		specific Truflow type indexed requested counts.
 * @req_cnt:	Pointer to the location to put the updated resource counts.
 *
 * Returns:
 *     0          - Success
 *     -          - Failure if negative
 */
static int tf_rm_update_parent_reservations(struct tf *tfp,
					    struct tf_dev_info *dev,
					    struct tf_rm_element_cfg *cfg,
					    u16 *alloc_cnt, u16 num_elements,
					    u16 *req_cnt,
					    bool shared_session)
{
	const char *type_str = "Invalid";
	int parent, child;

	/* Search through all the elements */
	for (parent = 0; parent < num_elements; parent++) {
		u16 combined_cnt = 0;

		/* If I am a parent */
		if (cfg[parent].cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_PARENT) {
			u8 p_slices = cfg[parent].slices;

			WARN_ON(!p_slices);

			combined_cnt = alloc_cnt[parent] / p_slices;

			if (alloc_cnt[parent] % p_slices)
				combined_cnt++;

			if (alloc_cnt[parent]) {
				dev->ops->tf_dev_get_resource_str(tfp,
							 cfg[parent].hcapi_type,
							 &type_str);
				netdev_dbg(tfp->bp->dev,
					   "%s:%s cnt(%d) slices(%d)\n",
					   type_str, tf_tbl_type_2_str(parent),
					   alloc_cnt[parent],
					   p_slices);
			}

			/* Search again through all the elements */
			for (child = 0; child < num_elements; child++) {
				/* If this is one of my children */
				if (cfg[child].cfg_type ==
				    TF_RM_ELEM_CFG_HCAPI_BA_CHILD &&
				    cfg[child].parent_subtype == parent &&
				    alloc_cnt[child]) {
					u8 c_slices = 1;
					u16 cnt = 0;

					if (!shared_session)
						c_slices = cfg[child].slices;

					WARN_ON(!c_slices);

					dev->ops->tf_dev_get_resource_str(tfp,
							  cfg[child].hcapi_type,
							   &type_str);
					netdev_dbg(tfp->bp->dev,
						   "%s:%s cnt:%d slices:%d\n",
						   type_str,
						   tf_tbl_type_2_str(child),
						   alloc_cnt[child],
						   c_slices);

					/* Increment the parents combined
					 * count with each child's count
					 * adjusted for number of slices per
					 * RM alloc item.
					 */
					cnt = alloc_cnt[child] / c_slices;

					if (alloc_cnt[child] % c_slices)
						cnt++;

					combined_cnt += cnt;
					/* Clear the requested child count */
					req_cnt[child] = 0;
				}
			}
			/* Save the parent count to be requested */
			req_cnt[parent] = combined_cnt;
			netdev_dbg(tfp->bp->dev, "%s calculated total:%d\n\n",
				   type_str, req_cnt[parent]);
		}
	}
	return 0;
}

static void tf_rm_dbg_print_resc_qcaps(struct tf *tfp,
				       struct tf_dev_info *dev,
				       u16 size,
				       struct tf_rm_resc_req_entry *query)
{
	u16 i;

	for (i = 0; i < size; i++) {
		const char *type_str;

		dev->ops->tf_dev_get_resource_str(tfp, query[i].type,
						  &type_str);
		netdev_dbg(tfp->bp->dev, "type: %2d-%s\tmin:%d max:%d\n",
			   query[i].type, type_str, query[i].min,
			   query[i].max);
	}
}

static void tf_rm_dbg_print_resc(struct tf *tfp, struct tf_dev_info *dev,
				 u16 size, struct tf_rm_resc_entry *resv)
{
	u16 i;

	for (i = 0; i < size; i++) {
		const char *type_str;

		dev->ops->tf_dev_get_resource_str(tfp, resv[i].type, &type_str);
		netdev_dbg(tfp->bp->dev,
			   "%2d type: %d-%s\tstart:%d stride:%d\n",
			   i, resv[i].type, type_str, resv[i].start,
			   resv[i].stride);
	}
}

int tf_rm_create_db(struct tf *tfp, struct tf_rm_create_db_parms *parms)
{
	enum tf_rm_resc_resv_strategy resv_strategy;
	u16 max_types, hcapi_items, *req_cnt = NULL;
	struct tf_rm_resc_req_entry *query = NULL;
	struct tf_rm_resc_req_entry *req = NULL;
	struct tf_rm_resc_entry *resv = NULL;
	struct tf_rm_new_db *rm_db = NULL;
	struct tf_rm_element *db = NULL;
	struct tf_dev_info *dev;
	bool shared_session = 0;
	struct tf_session *tfs;
	u8 fw_session_id;
	size_t len;
	int i, j;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	/* Need device max number of elements for the RM QCAPS */
	rc = dev->ops->tf_dev_get_max_types(tfp, &max_types);

	/* Allocate memory for RM QCAPS request */
	len = max_types * sizeof(struct tf_rm_resc_req_entry);
	query = vzalloc(len);
	if (!query) {
		rc = -ENOMEM;
		return rc;
	}

	/* Get Firmware Capabilities */
	rc = tf_msg_session_resc_qcaps(tfp, parms->dir, max_types, query,
				       &resv_strategy, NULL);
	if (rc) {
		vfree(query);
		return rc;
	}

	tf_rm_dbg_print_resc_qcaps(tfp, dev, max_types, query);

	/* Copy requested counts (alloc_cnt) from tf_open_session() to local
	 * copy (req_cnt) so that it can be updated if required.
	 */
	len = parms->num_elements * sizeof(u16);
	req_cnt = vzalloc(len);
	if (!req_cnt) {
		rc = -ENOMEM;
		goto fail;
	}

	memcpy(req_cnt, parms->alloc_cnt, parms->num_elements * sizeof(u16));

	shared_session = tf_session_is_shared_session(tfs);

	/* Update the req_cnt based upon the element configuration */
	tf_rm_update_parent_reservations(tfp, dev, parms->cfg,
					 parms->alloc_cnt,
					 parms->num_elements,
					 req_cnt,
					 shared_session);

	/* Process capabilities against DB requirements. However, as a
	 * DB can hold elements that are not HCAPI we can reduce the
	 * req msg content by removing those out of the request yet
	 * the DB holds them all as to give a fast lookup. We can also
	 * remove entries where there are no request for elements.
	 */
	tf_rm_count_hcapi_reservations(parms->dir,
				       parms->module,
				       parms->cfg,
				       req_cnt,
				       parms->num_elements,
				       &hcapi_items);

	if (hcapi_items == 0) {
		netdev_dbg(tfp->bp->dev,
			   "%s: module: %s Empty RM DB create request\n",
			   tf_dir_2_str(parms->dir),
			   tf_module_2_str(parms->module));
		parms->rm_db = NULL;
		rc = -ENOMEM;
		goto fail;
	}

	/* Alloc request */
	req = vzalloc(hcapi_items * sizeof(struct tf_rm_resc_req_entry));
	if (!req) {
		rc = -ENOMEM;
		goto fail;
	}

	/* Alloc reservation */
	resv = vzalloc(hcapi_items * sizeof(struct tf_rm_resc_entry));
	if (!resv) {
		rc = -ENOMEM;
		goto fail;
	}

	/* Build the request */
	for (i = 0, j = 0; i < parms->num_elements; i++) {
		struct tf_rm_element_cfg *cfg = &parms->cfg[i];
		u16 hcapi_type = cfg->hcapi_type;

		/* Only perform reservation for requested entries
		 */
		if (req_cnt[i] == 0)
			continue;


		/* Skip any children or invalid entries in the request */
		if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_CHILD ||
		    cfg->cfg_type == TF_RM_ELEM_CFG_NULL)
			continue;
		/* Error if we cannot get the full count based on qcaps */
		if (req_cnt[i] > query[hcapi_type].max) {
			const char *type_str;

			dev->ops->tf_dev_get_resource_str(tfp,
							  hcapi_type,
							  &type_str);
			netdev_dbg(tfp->bp->dev,
				   "Failure, %s:%d:%s req:%d avail:%d\n",
				   tf_dir_2_str(parms->dir),
				   hcapi_type, type_str, req_cnt[i],
				   query[hcapi_type].max);
			rc = -EINVAL;
			goto fail;
		}
		/* Full amount available, fill element request */
		req[j].type = hcapi_type;
		req[j].min = req_cnt[i];
		req[j].max = req_cnt[i];
		j++;
	}

	/* Allocate all resources for the module type */
	rc = tf_msg_session_resc_alloc(tfp, parms->dir, hcapi_items, req,
				       fw_session_id, resv);
	if (rc)
		goto fail;

	tf_rm_dbg_print_resc(tfp, dev, hcapi_items, resv);

	/* Build the RM DB per the request */
	rm_db = vzalloc(sizeof(*rm_db));
	if (!rm_db) {
		rc = -ENOMEM;
		goto fail;
	}

	/* Build the DB within RM DB */
	len = parms->num_elements * sizeof(struct tf_rm_element);
	rm_db->db = vzalloc(len);
	if (!rm_db->db) {
		rc = -ENOMEM;
		goto fail;
	}

	db = rm_db->db;
	memset(db, 0, len);

	for (i = 0, j = 0; i < parms->num_elements; i++) {
		struct tf_rm_element_cfg *cfg = &parms->cfg[i];
		const char *type_str;

		dev->ops->tf_dev_get_resource_str(tfp,
						  cfg->hcapi_type,
						  &type_str);

		db[i].cfg_type = cfg->cfg_type;
		db[i].hcapi_type = cfg->hcapi_type;
		db[i].slices = cfg->slices;

		/* Save the parent subtype for later use to find the pool */
		if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
			db[i].parent_subtype = cfg->parent_subtype;

		/* If the element didn't request an allocation no need
		 * to create a pool nor verify if we got a reservation.
		 */
		if (req_cnt[i] == 0)
			continue;

		/* Skip any children or invalid */
		if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_CHILD ||
		    cfg->cfg_type == TF_RM_ELEM_CFG_NULL)
			continue;

		/* If the element had requested an allocation and that
		 * allocation was a success (full amount) then
		 * allocate the pool.
		 */
		if (req_cnt[i] == resv[j].stride) {
			db[i].alloc.entry.start = resv[j].start;
			db[i].alloc.entry.stride = resv[j].stride;

			/* Only alloc BA pool if a BA type but not BA_CHILD */
			if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA ||
			    cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_PARENT) {
				/* Create pool */
				db[i].pool = vzalloc(sizeof(*db[i].pool));
				if (!db[i].pool) {
					rc = -ENOMEM;
					goto fail_db;
				}

				rc = bnxt_ba_init(db[i].pool, resv[j].stride,
						  true);
				if (rc) {
					netdev_dbg(tfp->bp->dev,
						   "%s: Pool init failed rc:%d, type:%d:%s\n",
						   tf_dir_2_str(parms->dir),
						   rc,
						   cfg->hcapi_type, type_str);
					goto fail_db;
				}
			}
			j++;
		} else {
			/* Bail out as we want what we requested for
			 * all elements, not any less.
			 */
			netdev_dbg(tfp->bp->dev,
				   "%s: Alloc failed %d:%s req:%d, alloc:%d\n",
				   tf_dir_2_str(parms->dir), cfg->hcapi_type,
				   type_str, req_cnt[i], resv[j].stride);
			rc = -EINVAL;
			goto fail_db;
		}
	}

	rm_db->num_entries = parms->num_elements;
	rm_db->dir = parms->dir;
	rm_db->module = parms->module;
	*parms->rm_db = (void *)rm_db;

	netdev_dbg(tfp->bp->dev, "%s: module:%s\n", tf_dir_2_str(parms->dir),
		   tf_module_2_str(parms->module));

	vfree(req);
	vfree(resv);
	vfree(req_cnt);
	return 0;

fail_db:
	for (i = 0; i < parms->num_elements; i++) {
		if (!rm_db->db[i].pool)
			continue;
		bnxt_ba_deinit(rm_db->db[i].pool);
		vfree(rm_db->db[i].pool);
	}
fail:
	vfree(req);
	vfree(resv);
	vfree(db);
	vfree(rm_db);
	vfree(req_cnt);
	vfree(query);
	parms->rm_db = NULL;

	return rc;
}

int tf_rm_create_db_no_reservation(struct tf *tfp,
				   struct tf_rm_create_db_parms *parms)
{
	struct tf_rm_resc_req_entry *req = NULL;
	struct tf_rm_resc_entry *resv = NULL;
	struct tf_rm_new_db *rm_db = NULL;
	u16 hcapi_items, *req_cnt = NULL;
	struct tf_rm_element *db = NULL;
	bool shared_session = 0;
	struct tf_dev_info *dev;
	struct tf_session *tfs;
	u8 fw_session_id;
	size_t len;
	int i, j;
	int rc;

	if (!tfp || !parms)
		return -EINVAL;

	/* Retrieve the session information */
	rc = tf_session_get_session_internal(tfp, &tfs);
	if (rc)
		return rc;

	/* Retrieve device information */
	rc = tf_session_get_device(tfs, &dev);
	if (rc)
		return rc;

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	/* Copy requested counts (alloc_cnt) from tf_open_session() to local
	 * copy (req_cnt) so that it can be updated if required.
	 */
	len = parms->num_elements * sizeof(u16);
	req_cnt = vzalloc(len);
	if (!req_cnt) {
		rc = -ENOMEM;
		return rc;
	}

	memcpy(req_cnt, parms->alloc_cnt, len);

	shared_session = tf_session_is_shared_session(tfs);

	/* Update the req_cnt based upon the element configuration */
	tf_rm_update_parent_reservations(tfp, dev, parms->cfg,
					 parms->alloc_cnt, parms->num_elements,
					 req_cnt, shared_session);
	/* Process capabilities against DB requirements. However, as a
	 * DB can hold elements that are not HCAPI we can reduce the
	 * req msg content by removing those out of the request yet
	 * the DB holds them all as to give a fast lookup. We can also
	 * remove entries where there are no request for elements.
	 */
	tf_rm_count_hcapi_reservations(parms->dir,
				       parms->module,
				       parms->cfg,
				       req_cnt,
				       parms->num_elements,
				       &hcapi_items);

	if (hcapi_items == 0) {
		netdev_dbg(tfp->bp->dev,
			   "%s: module:%s Empty RM DB create request\n",
			   tf_dir_2_str(parms->dir),
			   tf_module_2_str(parms->module));

		parms->rm_db = NULL;
		rc = -ENOMEM;
		goto fail;
	}

	/* Alloc request */
	req = vzalloc(hcapi_items * sizeof(struct tf_rm_resc_req_entry));
	if (!req) {
		rc = -ENOMEM;
		goto fail;
	}

	/* Alloc reservation */
	resv = vzalloc(hcapi_items * sizeof(struct tf_rm_resc_entry));
	if (!resv) {
		rc = -ENOMEM;
		goto fail;
	}

	/* Build the request */
	for (i = 0, j = 0; i < parms->num_elements; i++) {
		struct tf_rm_element_cfg *cfg = &parms->cfg[i];
		u16 hcapi_type = cfg->hcapi_type;

		/* Only perform reservation for requested entries */
		if (req_cnt[i] == 0)
			continue;

		/* Skip any children in the request */
		if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI ||
		    cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA ||
		    cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_PARENT) {
			req[j].type = hcapi_type;
			req[j].min = req_cnt[i];
			req[j].max = req_cnt[i];
			j++;
		}
	}

	/* Get all resources info for the module type */
	rc = tf_msg_session_resc_info(tfp, parms->dir, hcapi_items, req,
				      fw_session_id, resv);
	if (rc)
		goto fail;

	tf_rm_dbg_print_resc(tfp, dev, hcapi_items, resv);

	/* Build the RM DB per the request */
	rm_db = vzalloc(sizeof(*rm_db));
	if (!rm_db) {
		rc = -ENOMEM;
		goto fail;
	}

	/* Build the DB within RM DB */
	len = parms->num_elements * sizeof(struct tf_rm_element);
	rm_db->db = vzalloc(len);
	if (!rm_db->db) {
		rc = -ENOMEM;
		goto fail;
	}

	db = rm_db->db;
	memset(db, 0, len);

	for (i = 0, j = 0; i < parms->num_elements; i++) {
		struct tf_rm_element_cfg *cfg = &parms->cfg[i];
		const char *type_str;

		dev->ops->tf_dev_get_resource_str(tfp,
						  cfg->hcapi_type,
						  &type_str);

		db[i].cfg_type = cfg->cfg_type;
		db[i].hcapi_type = cfg->hcapi_type;
		db[i].slices = cfg->slices;

		/* Save the parent subtype for later use to find the pool */
		if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
			db[i].parent_subtype = cfg->parent_subtype;

		/* If the element didn't request an allocation no need
		 * to create a pool nor verify if we got a reservation.
		 */
		if (req_cnt[i] == 0)
			continue;

		/* Skip any children or invalid */
		if (cfg->cfg_type != TF_RM_ELEM_CFG_HCAPI &&
		    cfg->cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
		    cfg->cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT)
			continue;

		/* If the element had requested an allocation and that
		 * allocation was a success (full amount) then
		 * allocate the pool.
		 */
		if (req_cnt[i] == resv[j].stride) {
			db[i].alloc.entry.start = resv[j].start;
			db[i].alloc.entry.stride = resv[j].stride;

			/* Only allocate BA pool if a BA type not a child */
			if (cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA ||
			    cfg->cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_PARENT) {
				db[i].pool = vzalloc(sizeof(*db[i].pool));
				if (!db[i].pool) {
					rc = -ENOMEM;
					goto fail;
				}

				rc = bnxt_ba_init(db[i].pool,
						  resv[j].stride,
						  true);
				if (rc) {
					netdev_dbg(tfp->bp->dev,
						   "%s: Pool init failed rc:%d, type:%d:%s\n",
						   tf_dir_2_str(parms->dir),
						   rc,
						   cfg->hcapi_type, type_str);
					goto fail;
				}
			}
			j++;
		} else {
			/* Bail out as we want what we requested for
			 * all elements, not any less.
			 */
			netdev_dbg(tfp->bp->dev,
				   "%s: Alloc failed %d:%s req:%d alloc:%d\n",
				   tf_dir_2_str(parms->dir), cfg->hcapi_type,
				   type_str, req_cnt[i], resv[j].stride);
			rc = -EINVAL;
			goto fail;
		}
	}

	rm_db->num_entries = parms->num_elements;
	rm_db->dir = parms->dir;
	rm_db->module = parms->module;
	*parms->rm_db = (void *)rm_db;

	netdev_dbg(tfp->bp->dev, "%s: module:%s\n", tf_dir_2_str(parms->dir),
		   tf_module_2_str(parms->module));

	vfree(req);
	vfree(resv);
	vfree(req_cnt);
	return 0;

 fail:
	vfree(req);
	vfree(resv);
	if (rm_db && rm_db->db) {
		for (i = 0; i < parms->num_elements; i++) {
			if (!rm_db->db[i].pool)
				continue;
			bnxt_ba_deinit(rm_db->db[i].pool);
			vfree(rm_db->db[i].pool);
		}
	}
	vfree(db);
	vfree(rm_db);
	vfree(req_cnt);
	parms->rm_db = NULL;

	return rc;
}

/* Device unbind happens when the TF Session is closed and the
 * session ref count is 0. Device unbind will cleanup each of
 * its support modules, i.e. Identifier, thus we're ending up
 * here to close the DB.
 *
 * On TF Session close it is assumed that the session has already
 * cleaned up all its resources, individually, while
 * destroying its flows.
 *
 * To assist in the 'cleanup checking' the DB is checked for any
 * remaining elements and logged if found to be the case.
 *
 * Any such elements will need to be 'cleared' ahead of
 * returning the resources to the HCAPI RM.
 *
 * RM will signal FW to flush the DB resources. FW will
 * perform the invalidation. TF Session close will return the
 * previous allocated elements to the RM and then close the
 * HCAPI RM registration. That then saves several 'free' msgs
 * from being required.
 */
int tf_rm_free_db(struct tf *tfp, struct tf_rm_free_db_parms *parms)
{
	struct tf_rm_resc_entry *resv;
	bool residuals_found = false;
	struct tf_rm_new_db *rm_db;
	u16 resv_size = 0;
	u8 fw_session_id;
	int rc;
	int i;

	if (!parms || !parms->rm_db)
		return -EINVAL;

	rc = tf_session_get_fw_session_id(tfp, &fw_session_id);
	if (rc)
		return rc;

	rm_db = (struct tf_rm_new_db *)parms->rm_db;

	/* Check for residuals that the client didn't clean up */
	rc = tf_rm_check_residuals(rm_db, &resv_size, &resv, &residuals_found);
	if (rc)
		goto cleanup;

	/* Invalidate any residuals followed by a DB traversal for
	 * pool cleanup.
	 */
	if (residuals_found) {
		rc = tf_msg_session_resc_flush(tfp, parms->dir, resv_size,
					       fw_session_id, resv);
		vfree(resv);
		/* On failure we still have to cleanup so we can only
		 * log that FW failed.
		 */
		if (rc)
			netdev_dbg(tfp->bp->dev,
				   "%s: Internal Flush error, module:%s\n",
				   tf_dir_2_str(parms->dir),
				   tf_module_2_str(rm_db->module));
	}

 cleanup:
	/* No need to check for configuration type, even if we do not
	 * have a BA pool we just delete on a null ptr, no harm
	 */
	for (i = 0; i < rm_db->num_entries; i++) {
		bnxt_ba_deinit(rm_db->db[i].pool);
		vfree(rm_db->db[i].pool);
	}

	vfree(rm_db->db);
	vfree(parms->rm_db);

	return rc;
}

/**
 * Get the bit allocator pool associated with the subtype and the db
 *
 * @rm_db:	Pointer to the DB
 * @subtype:	Module subtype used to index into the module specific
 *		database. An example subtype is TF_TBL_TYPE_FULL_ACT_RECORD
 *		which is a module subtype of TF_MODULE_TYPE_TABLE.
 * @pool:	Pointer to the bit allocator pool used
 * @new_subtype:	Pointer to the subtype of the actual pool used
 *
 * Returns:
 *     0          - Success
 *   - EOPNOTSUPP    - Operation not supported
 */
static int tf_rm_get_pool(struct tf_rm_new_db *rm_db, u16 subtype,
			  struct bitalloc **pool, u16 *new_subtype)
{
	u16 tmp_subtype = subtype;
	int rc = 0;

	/* If we are a child, get the parent table index */
	if (rm_db->db[subtype].cfg_type == TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
		tmp_subtype = rm_db->db[subtype].parent_subtype;

	*pool = rm_db->db[tmp_subtype].pool;

	/* Bail out if the pool is not valid, should never happen */
	if (!rm_db->db[tmp_subtype].pool) {
		rc = -EOPNOTSUPP;
		netdev_dbg(NULL, "%s: Invalid pool for this type:%d, rc:%d\n",
			   tf_dir_2_str(rm_db->dir), tmp_subtype, rc);
		return rc;
	}
	*new_subtype = tmp_subtype;
	return rc;
}

int tf_rm_allocate(struct tf_rm_allocate_parms *parms)
{
	enum tf_rm_elem_cfg_type cfg_type;
	struct tf_rm_new_db *rm_db;
	struct bitalloc *pool;
	u16 subtype;
	u32 index;
	int rc;
	int id;

	if (!parms || !parms->rm_db)
		return -EINVAL;

	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	if (!rm_db->db)
		return -EINVAL;

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
		return -EOPNOTSUPP;

	rc = tf_rm_get_pool(rm_db, parms->subtype, &pool, &subtype);
	if (rc)
		return rc;
	/* priority  0: allocate from top of the tcam i.e. high
	 * priority !0: allocate index from bottom i.e lowest
	 */
	if (parms->priority)
		id = bnxt_ba_alloc_reverse(pool);
	else
		id = bnxt_ba_alloc(pool);
	if (id < 0) {
		rc = -ENOSPC;
		netdev_dbg(NULL, "%s: Allocation failed, rc:%d\n",
			   tf_dir_2_str(rm_db->dir), rc);
		return rc;
	}

	/* Adjust for any non zero start value */
	rc = tf_rm_adjust_index(rm_db->db,
				TF_RM_ADJUST_ADD_BASE,
				subtype,
				id,
				&index);
	if (rc) {
		netdev_dbg(NULL,
			   "%s: Alloc adjust of base index failed, rc:%d\n",
			   tf_dir_2_str(rm_db->dir), rc);
		return -EINVAL;
	}

	*parms->index = index;
	if (parms->base_index)
		*parms->base_index = id;

	return rc;
}

int tf_rm_free(struct tf_rm_free_parms *parms)
{
	enum tf_rm_elem_cfg_type cfg_type;
	struct tf_rm_new_db *rm_db;
	struct bitalloc *pool;
	u32 adj_index;
	u16 subtype;
	int rc;

	if (!parms || !parms->rm_db)
		return -EINVAL;
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	if (!rm_db->db)
		return -EINVAL;

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
		return -EOPNOTSUPP;

	rc = tf_rm_get_pool(rm_db, parms->subtype, &pool, &subtype);
	if (rc)
		return rc;

	/* Adjust for any non zero start value */
	rc = tf_rm_adjust_index(rm_db->db,
				TF_RM_ADJUST_RM_BASE,
				subtype,
				parms->index,
				&adj_index);
	if (rc)
		return rc;

	rc = bnxt_ba_free(pool, adj_index);
	/* No logging direction matters and that is not available here */
	if (rc)
		return rc;

	return rc;
}

int
tf_rm_is_allocated(struct tf_rm_is_allocated_parms *parms)
{
	enum tf_rm_elem_cfg_type cfg_type;
	struct tf_rm_new_db *rm_db;
	struct bitalloc *pool;
	u32 adj_index;
	u16 subtype;
	int rc;

	if (!parms || !parms->rm_db)
		return -EINVAL;
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	if (!rm_db->db)
		return -EINVAL;

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not controlled by RM */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
		return -EOPNOTSUPP;

	rc = tf_rm_get_pool(rm_db, parms->subtype, &pool, &subtype);
	if (rc)
		return rc;

	/* Adjust for any non zero start value */
	rc = tf_rm_adjust_index(rm_db->db,
				TF_RM_ADJUST_RM_BASE,
				subtype,
				parms->index,
				&adj_index);
	if (rc)
		return rc;

	if (parms->base_index)
		*parms->base_index = adj_index;

	*parms->allocated = bnxt_ba_inuse(pool, adj_index);

	return rc;
}

int
tf_rm_get_info(struct tf_rm_get_alloc_info_parms *parms)
{
	enum tf_rm_elem_cfg_type cfg_type;
	struct tf_rm_new_db *rm_db;

	if (!parms || !parms->rm_db)
		return -EINVAL;
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	if (!rm_db->db)
		return -EINVAL;

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not controlled by HCAPI */
	if (cfg_type == TF_RM_ELEM_CFG_NULL)
		return -EOPNOTSUPP;

	memcpy(parms->info, &rm_db->db[parms->subtype].alloc,
	       sizeof(struct tf_rm_alloc_info));

	return 0;
}

int tf_rm_get_all_info(struct tf_rm_get_alloc_info_parms *parms, int size)
{
	struct tf_rm_get_alloc_info_parms gparms;
	int i;
	int rc;

	if (!parms)
		return -EINVAL;

	gparms = *parms;

	/* Walk through all items */
	for (i = 0; i < size; i++) {

		gparms.subtype = i;

		/* Get subtype info */
		rc = tf_rm_get_info(&gparms);

		if (rc && (rc != -EOPNOTSUPP))
			return rc;

		/* Next subtype memory location */
		gparms.info++;
	}
	return 0;
}

int tf_rm_get_hcapi_type(struct tf_rm_get_hcapi_parms *parms)
{
	enum tf_rm_elem_cfg_type cfg_type;
	struct tf_rm_new_db *rm_db;

	if (!parms || !parms->rm_db)
		return -EINVAL;
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	if (!rm_db->db)
		return -EINVAL;

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not controlled by HCAPI */
	if (cfg_type == TF_RM_ELEM_CFG_NULL)
		return -EOPNOTSUPP;

	*parms->hcapi_type = rm_db->db[parms->subtype].hcapi_type;

	return 0;
}

int tf_rm_get_slices(struct tf_rm_get_slices_parms *parms)
{
	enum tf_rm_elem_cfg_type cfg_type;
	struct tf_rm_new_db *rm_db;

	if (!parms || !parms->rm_db)
		return -EINVAL;
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	if (!rm_db->db)
		return -EINVAL;

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not controlled by HCAPI */
	if (cfg_type == TF_RM_ELEM_CFG_NULL)
		return -EOPNOTSUPP;

	*parms->slices = rm_db->db[parms->subtype].slices;

	return 0;
}

int tf_rm_get_inuse_count(struct tf_rm_get_inuse_count_parms *parms)
{
	enum tf_rm_elem_cfg_type cfg_type;
	struct tf_rm_new_db *rm_db;
	int rc = 0;

	if (!parms || !parms->rm_db)
		return -EINVAL;
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	if (!rm_db->db)
		return -EINVAL;

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not a BA pool */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
		return -EOPNOTSUPP;

	/* Bail silently (no logging), if the pool is not valid there
	 * was no elements allocated for it.
	 */
	if (!rm_db->db[parms->subtype].pool) {
		*parms->count = 0;
		return 0;
	}

	*parms->count = bnxt_ba_inuse_count(rm_db->db[parms->subtype].pool);

	return rc;
}

/* Only used for table bulk get at this time
 */
int tf_rm_check_indexes_in_range(struct tf_rm_check_indexes_in_range_parms
				 *parms)
{
	enum tf_rm_elem_cfg_type cfg_type;
	struct tf_rm_new_db *rm_db;
	struct bitalloc *pool;
	u32 base_index;
	u16 subtype;
	u32 stride;
	int rc = 0;

	if (!parms || !parms->rm_db)
		return -EINVAL;
	rm_db = (struct tf_rm_new_db *)parms->rm_db;
	if (!rm_db->db)
		return -EINVAL;

	cfg_type = rm_db->db[parms->subtype].cfg_type;

	/* Bail out if not a BA pool */
	if (cfg_type != TF_RM_ELEM_CFG_HCAPI_BA &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_PARENT &&
	    cfg_type != TF_RM_ELEM_CFG_HCAPI_BA_CHILD)
		return -EOPNOTSUPP;

	rc = tf_rm_get_pool(rm_db, parms->subtype, &pool, &subtype);
	if (rc)
		return rc;

	base_index = rm_db->db[subtype].alloc.entry.start;
	stride = rm_db->db[subtype].alloc.entry.stride;

	if (parms->starting_index < base_index ||
	    parms->starting_index + parms->num_entries > base_index + stride)
		return -EINVAL;

	return rc;
}
