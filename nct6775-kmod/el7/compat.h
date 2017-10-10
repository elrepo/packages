#ifndef __COMPAT_H
#define __COMPAT_H

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
#error This driver is for kernel versions 2.6.32 and later
#endif

#if !defined (CONFIG_HWMON_VID) && !defined(CONFIG_HWMON_VID_MODULE)
int vid_from_reg(int val, u8 vrm)
{
	return 0;
}

u8 vid_which_vrm(void)
{
	return 0;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
#if !(defined RHEL_MAJOR && RHEL_MAJOR == 7)
#if !(defined RHEL_MAJOR && RHEL_MAJOR == 6 && RHEL_MINOR >= 7)
static int sysfs_create_groups(struct kobject *kobj,
			       const struct attribute_group **groups)
{
	int error = 0;
	int i;

	if (!groups)
		return 0;

	for (i = 0; groups[i]; i++) {
		error = sysfs_create_group(kobj, groups[i]);
		if (error) {
			while (--i >= 0)
				sysfs_remove_group(kobj, groups[i]);
			break;
		}
	}
	return error;
}
#endif

static void sysfs_remove_groups(struct kobject *kobj,
				const struct attribute_group **groups)
{
	int i;

	if (!groups)
		return;
	for (i = 0; groups[i]; i++)
		sysfs_remove_group(kobj, groups[i]);
}
#endif

#if !(defined RHEL_MAJOR && RHEL_MAJOR == 7)
#if !(defined RHEL_MAJOR && RHEL_MAJOR == 6 && RHEL_MINOR >= 7)
static inline int __must_check PTR_ERR_OR_ZERO(__force const void *ptr)
{
	if (IS_ERR(ptr))
		return PTR_ERR(ptr);
	else
		return 0;
}
#endif
#endif
#endif

#ifdef __NEED_I2C__

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 21)
#if !(defined RHEL_MAJOR && RHEL_MAJOR == 5 && RHEL_MINOR >= 6)
/* Simplified version for compatibility */
struct i2c_board_info {
	char		type[I2C_NAME_SIZE];
	unsigned short	flags;
	unsigned short	addr;
};
#endif
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 25)
/* Some older kernels have a different, useless struct i2c_device_id */
#define i2c_device_id i2c_device_id_compat
struct i2c_device_id {
	char name[I2C_NAME_SIZE];
	kernel_ulong_t driver_data      /* Data private to the driver */
			__attribute__((aligned(sizeof(kernel_ulong_t))));
};
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
static unsigned short empty_i2c[] =  { I2C_CLIENT_END };
static struct i2c_client_address_data addr_data = {
	.normal_i2c	= normal_i2c,
	.probe		= empty_i2c,
	.ignore		= empty_i2c,
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
static inline s32
i2c_smbus_read_word_swapped(const struct i2c_client *client, u8 command)
{
	s32 value = i2c_smbus_read_word_data(client, command);

	return (value < 0) ? value : swab16(value);
}

static inline s32
i2c_smbus_write_word_swapped(const struct i2c_client *client,
			     u8 command, u16 value)
{
	return i2c_smbus_write_word_data(client, command, swab16(value));
}
#endif

/* Red Hat EL5 includes backports of these functions, so we can't redefine
 * our own. */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 24)
#if !(defined RHEL_MAJOR && RHEL_MAJOR == 5 && RHEL_MINOR >= 5)
static inline int strict_strtoul(const char *cp, unsigned int base,
				 unsigned long *res)
{
	*res = simple_strtoul(cp, NULL, base);
	return 0;
}

static inline int strict_strtol(const char *cp, unsigned int base, long *res)
{
	*res = simple_strtol(cp, NULL, base);
	return 0;
}
#endif
#endif

#endif	/* __NEED_I2C__ */

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 28)
/*
 * Divide positive or negative dividend by positive divisor and round
 * to closest integer. Result is undefined for negative divisors and
 * for negative dividends if the divisor variable type is unsigned.
 */
#define DIV_ROUND_CLOSEST(x, divisor)(			\
{							\
	typeof(x) __x = x;				\
	typeof(divisor) __d = divisor;			\
	(((typeof(x))-1) > 0 ||				\
	 ((typeof(divisor))-1) > 0 || (__x) > 0) ?	\
		(((__x) + ((__d) / 2)) / (__d)) :	\
		(((__x) - ((__d) / 2)) / (__d));	\
}							\
)
#endif

#ifndef module_driver
/**
 * module_driver() - Helper macro for drivers that don't do anything
 * special in module init/exit. This eliminates a lot of boilerplate.
 * Each module may only use this macro once, and calling it replaces
 * module_init() and module_exit().
 *
 * @__driver: driver name
 * @__register: register function for this driver type
 * @__unregister: unregister function for this driver type
 * @...: Additional arguments to be passed to __register and __unregister.
 *
 * Use this macro to construct bus specific macros for registering
 * drivers, and do not use it on its own.
 */
#define module_driver(__driver, __register, __unregister, ...) \
static int __init __driver##_init(void) \
{ \
	return __register(&(__driver) , ##__VA_ARGS__); \
} \
module_init(__driver##_init); \
static void __exit __driver##_exit(void) \
{ \
	__unregister(&(__driver) , ##__VA_ARGS__); \
} \
module_exit(__driver##_exit);
#endif

#ifdef __NEED_I2C__

#ifndef module_i2c_driver
/**
 * module_i2c_driver() - Helper macro for registering a I2C driver
 * @__i2c_driver: i2c_driver struct
 *
 * Helper macro for I2C drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
#define module_i2c_driver(__i2c_driver) \
	module_driver(__i2c_driver, i2c_add_driver, \
			i2c_del_driver)
#endif

#endif /* __NEED_I2C__ */

#ifndef clamp_val
#define clamp_val SENSORS_LIMIT
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
#ifndef kstrtol
#define kstrtol strict_strtol
#endif
#ifndef kstrtoul
#define kstrtoul strict_strtoul
#endif
#endif

#ifndef request_muxed_region
#define request_muxed_region(a, b, c) (true)
#endif
#ifndef release_region
#define release_region(a, b)
#endif

#ifndef pr_warn
/* pr_warn macro not introduced until 2.6.35 */
#define pr_warn pr_warning
#endif
#ifndef pr_warn_ratelimited
#define pr_warn_ratelimited pr_warning_ratelimited
#endif

#ifndef sysfs_attr_init
#define sysfs_attr_init(attr) do {} while (0)
#endif

#endif /* __COMPAT_H */
