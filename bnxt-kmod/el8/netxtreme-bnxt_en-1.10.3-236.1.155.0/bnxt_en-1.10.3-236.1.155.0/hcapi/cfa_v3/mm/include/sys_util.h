/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _SYS_UTIL_H_
#define _SYS_UTIL_H_

#define Y_NUM_ALIGN_UNITS(x, unit) (((x) + (unit) - (1)) / (unit))
#define Y_IS_POWER_2(x) (((x) != 0) && (((x) & ((x) - (1))) == 0))

#endif /* _SYS_UTIL_H_ */

