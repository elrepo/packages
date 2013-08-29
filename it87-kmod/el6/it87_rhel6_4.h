/*
 * Backported functions for it87 on RHEL6u4
 */

#ifndef IT87_RHEL6_4
#define IT87_RHEL6_4

#ifndef request_muxed_region
#define request_muxed_region(a, b, c)	request_region(a, b, c)
#endif

#endif /* IT87_RHEL6_4 */
