/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Broadcom
 * All rights reserved.
 */

#ifndef _ULP_UDCC_H_
#define _ULP_UDCC_H_

#include <linux/rhashtable.h>
#include <linux/refcount.h>

struct bnxt_ulp_udcc_v6_subnet_key {
	u16 src_fid;
	struct in6_addr dst;
	struct in6_addr dmsk;
};

struct bnxt_ulp_udcc_v6_subnet_data {
	u8 dmac[ETH_ALEN];
	u8 smac[ETH_ALEN];
	u16 subnet_hndl; /* Template FDB needs this to flush */
};

struct bnxt_ulp_udcc_v6_subnet_node {
	struct bnxt_ulp_udcc_v6_subnet_key key;
	struct rhash_head node;
	struct bnxt_ulp_udcc_v6_subnet_data data;
	refcount_t ref;
};

#if defined(CONFIG_BNXT_FLOWER_OFFLOAD)

/* Add subnet in the hash table
 * @key[in]: Key struct with src_fid, prefix and mask
 * @data[in]: pointer to data to store in the subnet node
 * return 0 on success and -ve on failure
 */
int bnxt_ulp_udcc_v6_subnet_add(struct bnxt *bp,
				u16 *src_fid, u8 *v6dst, u8 *v6msk,
				u8 *dmac, u8 *smac,
				u16 *subnet_hndl);

/* Delete subnet in the hash table by handl
 * ULP template handler can clean resource by hndl ONLY
 * @subnet_hndl[in]: ULP template resource handle
 * @retuns 0 on success and -ve on failure
 */
int bnxt_ulp_udcc_v6_subnet_del(struct bnxt *bp, u16 subnet_hndl);

/* Subnet Checking for UDCC application
 * @src_fid[in]: FID of the function
 * @dst[in]: Dest IPv6
 * @dmac[out]: pointer to modify dmac
 * @smac[out]: pointer to modify smac
 * @returns:
 *   0 when a valid subnet with modify dmac and smac is found,
 *   -ENOENT when subnet is NOT found,
 *   -EPERM  the subnets modify dmac/smac are invalid (e.g. VFtoVF)
 *   -ve on other failures
 */
int bnxt_ulp_udcc_v6_subnet_check(struct bnxt *bp,
				  u16 src_fid,
				  const struct in6_addr *dst,
				  u8 *dmac, u8 *smac);

#endif /* #if defined(CONFIG_BNXT_FLOWER_OFFLOAD) */
#endif /* #ifndef _ULP_UDCC_H_ */
