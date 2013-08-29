/*
 * Backported functions for it87 on RHEL5u9
 */

#ifndef IT87_RHEL5_9
#define IT87_RHEL5_9

#ifndef pr_err
#define pr_err(fmt, ...) \
        printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef pr_info
#define pr_info(fmt, ...) \
        printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifndef pr_notice
#define pr_notice(fmt, ...) \
        printk(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#endif

#ifndef request_muxed_region
#define request_muxed_region(a, b, c)	request_region(a, b, c)
#endif

#ifndef DIV_ROUND_CLOSEST
#define DIV_ROUND_CLOSEST(x, divisor)(			\
{							\
	typeof(divisor) __divisor = divisor;		\
	(((x) + ((__divisor) / 2)) / (__divisor));	\
}							\
)
#endif

#define kstrtoul(a, b, c)	strict_strtoul(a, b, c)
#define kstrtol(a, b, c)	strict_strtol(a, b, c)

#undef devm_request_region
#define devm_request_region(a, b, c, d)		request_region(b, c, d)

static inline int acpi_check_resource_conflict(struct resource *res)
{
	return 0;
}

#endif /* IT87_RHEL5_9 */
