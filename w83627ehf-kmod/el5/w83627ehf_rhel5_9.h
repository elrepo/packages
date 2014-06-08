/*
 * Backported functions for w83627ehf on RHEL5u9
 */

#ifndef W83627EHF_RHEL5_9
#define W83627EHF_RHEL5_9

#ifndef DIV_ROUND_CLOSEST
#define DIV_ROUND_CLOSEST(x, divisor)(			\
{							\
	typeof(divisor) __divisor = divisor;		\
	(((x) + ((__divisor) / 2)) / (__divisor));	\
}							\
)
#endif

int acpi_check_resource_conflict(const struct resource *res)
{
	return 0;
}

#endif
