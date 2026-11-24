/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef TF_GLOBAL_CFG_H_
#define TF_GLOBAL_CFG_H_

#include "tf_core.h"

/* The global cfg module provides processing of global cfg types.  */

/* struct tf; */

/* Internal type not available to user
 * but available internally within Truflow
 */
enum tf_global_config_internal_type {
	TF_GLOBAL_CFG_INTERNAL_PARIF_2_PF = TF_GLOBAL_CFG_TYPE_MAX,
	TF_GLOBAL_CFG_INTERNAL_TYPE_MAX
};

/**
 * Global cfg configuration enumeration.
 */
enum tf_global_cfg_cfg_type {
	TF_GLOBAL_CFG_CFG_NULL,		/* No configuration */
	TF_GLOBAL_CFG_CFG_HCAPI,	/* HCAPI 'controlled' */
};

/**
 * Global cfg configuration structure, used by the Device to configure
 * how an individual global cfg type is configured in regard to the HCAPI type.
 *
 * @cfg_type:	Global cfg config controls how the DB for that element
 *		is processed.
 * @hcapi_type:	HCAPI Type for the element. Used for TF to HCAPI type
 *		conversion.
 */
struct tf_global_cfg_cfg {
	enum tf_global_cfg_cfg_type	cfg_type;
	u16				hcapi_type;
};

/**
 * Global Cfg configuration parameters
 * @num_elements:	Number of table types in the configuration array
 * @cfg:		Table Type element configuration array
 */
struct tf_global_cfg_cfg_parms {
	u16				num_elements;
	struct tf_global_cfg_cfg	*cfg;
};

/**
 * Initializes the Global Cfg module with the requested DBs. Must be
 * invoked as the first thing before any of the access functions.
 *
 * @tfp:	Pointer to TF handle
 * @parms:	Pointer to Global Cfg configuration parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-ENOMEM) on failure.
 */
int tf_global_cfg_bind(struct tf *tfp, struct tf_global_cfg_cfg_parms *parms);

/**
 * Cleans up the private DBs and releases all the data.
 *
 * @tfp:	Pointer to TF handle
 * @parms:	Pointer to Global Cfg configuration parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_global_cfg_unbind(struct tf *tfp);

/**
 * Updates the global configuration table
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to global cfg parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_global_cfg_set(struct tf *tfp, struct tf_global_cfg_parms *parms);

/**
 * Get global configuration
 *
 * @tfp:	Pointer to TF handle, used for HCAPI communication
 * @parms:	Pointer to global cfg parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_global_cfg_get(struct tf *tfp, struct tf_global_cfg_parms *parms);

#endif /* TF_GLOBAL_CFG_H */
