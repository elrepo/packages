/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _CFA_UTIL_H_
#define _CFA_UTIL_H_

/*
 * CFA specific utility macros
 */

/* Bounds (closed interval) check helper macro */
#define CFA_CHECK_BOUNDS(x, l, h) (((x) >= (l)) && ((x) <= (h)))
#define CFA_CHECK_UPPER_BOUNDS(x, h) ((x) <= (h))

#define CFA_ALIGN_LN2(x) (((x) < 3U) ? (x) : 32U - __builtin_clz((x) - 1U) + 1U)

#endif /* _CFA_UTIL_H_ */
