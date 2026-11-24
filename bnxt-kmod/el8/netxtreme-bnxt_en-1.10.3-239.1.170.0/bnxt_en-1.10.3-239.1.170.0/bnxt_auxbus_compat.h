/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2022-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef _BNXT_AUXILIARY_COMPAT_H_
#define _BNXT_AUXILIARY_COMPAT_H_

#ifndef AUXILIARY_NAME_SIZE
#define AUXILIARY_NAME_SIZE	32
#endif

#ifndef HAVE_AUX_DEVICE_ID
#include <linux/mod_devicetable.h>

struct auxiliary_device_id {
	char name[AUXILIARY_NAME_SIZE];
	kernel_ulong_t driver_data;
};
#endif

#ifdef HAVE_EXTERNAL_OFED
#include "auxiliary_bus.h"
#elif defined(HAVE_AUXILIARY_DRIVER)
#include <linux/auxiliary_bus.h>
#endif

#ifndef HAVE_EXTERNAL_OFED
#if defined(HAVE_AUXILIARY_DRIVER) && !defined(HAVE_AUX_GET_DRVDATA)
static inline void *auxiliary_get_drvdata(struct auxiliary_device *auxdev)
{
	return dev_get_drvdata(&auxdev->dev);
}

static inline void auxiliary_set_drvdata(struct auxiliary_device *auxdev, void *data)
{
	dev_set_drvdata(&auxdev->dev, data);
}
#endif

#ifndef HAVE_AUXILIARY_DRIVER

#include <linux/device.h>
#include <linux/mod_devicetable.h>
#ifndef HAVE_IDA_ALLOC
#include <linux/idr.h>
#endif

struct auxiliary_device {
	struct device dev;
	const char *name;
	u32 id;
	struct list_head list;
};

struct auxiliary_driver {
	int (*probe)(struct auxiliary_device *auxdev, const struct auxiliary_device_id *id);
	void (*remove)(struct auxiliary_device *auxdev);
	void (*shutdown)(struct auxiliary_device *auxdev);
	int (*suspend)(struct auxiliary_device *auxdev, pm_message_t state);
	int (*resume)(struct auxiliary_device *auxdev);
	const char *name;
	struct device_driver driver;
	const struct auxiliary_device_id *id_table;
	struct list_head list;
};

int auxiliary_device_init(struct auxiliary_device *auxdev);
int auxiliary_device_add(struct auxiliary_device *auxdev);
void auxiliary_device_uninit(struct auxiliary_device *auxdev);
void auxiliary_device_delete(struct auxiliary_device *auxdev);
int bnxt_auxiliary_driver_register(struct auxiliary_driver *auxdrv);
void bnxt_auxiliary_driver_unregister(struct auxiliary_driver *auxdrv);

#define auxiliary_driver_register bnxt_auxiliary_driver_register
#define auxiliary_driver_unregister bnxt_auxiliary_driver_unregister

static inline void *auxiliary_get_drvdata(struct auxiliary_device *auxdev)
{
	return dev_get_drvdata(&auxdev->dev);
}

static inline void auxiliary_set_drvdata(struct auxiliary_device *auxdev, void *data)
{
	dev_set_drvdata(&auxdev->dev, data);
}

static inline struct auxiliary_driver *to_auxiliary_drv(struct device_driver *drv)
{
	return container_of(drv, struct auxiliary_driver, driver);
}

#endif /* HAVE_AUXILIARY_DRIVER */
#endif

#ifndef HAVE_IDA_ALLOC
static inline int ida_alloc(struct ida *ida, gfp_t gfp)
{
	return ida_simple_get(ida, 0, 0, gfp);
}

static inline void ida_free(struct ida *ida, unsigned int id)
{
	ida_simple_remove(ida, id);
}
#endif /* HAVE_IDA_ALLOC */
#endif /* _BNXT_AUXILIARY_COMPAT_H_ */
