/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2022-2024 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef __HDBR_H__
#define __HDBR_H__

#include "bnxt.h"
#include "bnxt_hdbr.h"

/* RoCE HW based doorbell drop recovery defination */
struct hdbr_pg {
	struct list_head pg_node;
	__le64		 *kptr;
	dma_addr_t	 da;
	int		 grp_size;
	int		 blk_size;
	u64		 stride_n_size;
	int		 first_avail;
	int		 first_empty;
	int		 size;
	int		 blk_avail;
	int		 ktbl_idx;
};

struct hdbr_pg_lst {
	int		 group;
	int		 blk_avail;
	struct list_head pg_head;
	struct mutex	 lst_lock; /* protect pg_list */
};

struct bnxt_re_hdbr_app {
	struct list_head	lst;
	struct hdbr_pg_lst	pg_lst[DBC_GROUP_MAX];
};

struct bnxt_re_hdbr_dbgfs_file_data {
	struct bnxt_re_dev *rdev;
	int		   group;
	bool		   user;
};

struct bnxt_re_hdbr_dfs_data {
	struct dentry *hdbr_dir;
	struct bnxt_re_hdbr_dbgfs_file_data file_data[2][DBC_GROUP_MAX];
};

struct bnxt_re_hdbr_free_pg_work {
	struct work_struct	work;
	struct bnxt_re_dev	*rdev;
	struct hdbr_pg		*pg;
	size_t			size;
	int			group;
};

#define DBC_GROUP_SIZE_SQ_RQ	1
#define DBC_GROUP_SIZE_SRQ	3
#define DBC_GROUP_SIZE_CQ	4

static inline int bnxt_re_hdbr_group_size(int group)
{
	if (group == DBC_GROUP_SRQ)
		return DBC_GROUP_SIZE_SRQ;
	if (group == DBC_GROUP_CQ)
		return DBC_GROUP_SIZE_CQ;
	return DBC_GROUP_SIZE_SQ_RQ;
}

int bnxt_re_hdbr_init(struct bnxt_re_dev *rdev);
void bnxt_re_hdbr_uninit(struct bnxt_re_dev *rdev);
struct bnxt_re_hdbr_app *bnxt_re_hdbr_alloc_app(struct bnxt_re_dev *rdev, bool user);
void bnxt_re_hdbr_dealloc_app(struct bnxt_re_dev *rdev, struct bnxt_re_hdbr_app *app);
void bnxt_re_hdbr_db_unreg_srq(struct bnxt_re_dev *rdev, struct bnxt_re_srq *srq);
void bnxt_re_hdbr_db_unreg_qp(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp);
void bnxt_re_hdbr_db_unreg_cq(struct bnxt_re_dev *rdev, struct bnxt_re_cq *cq);
int bnxt_re_hdbr_db_reg_srq(struct bnxt_re_dev *rdev, struct bnxt_re_srq *srq,
			    struct bnxt_re_ucontext *cntx, struct bnxt_re_srq_resp *resp);
int bnxt_re_hdbr_db_reg_qp(struct bnxt_re_dev *rdev, struct bnxt_re_qp *qp,
			   struct bnxt_re_pd *pd, struct bnxt_re_qp_resp *resp);
int bnxt_re_hdbr_db_reg_cq(struct bnxt_re_dev *rdev, struct bnxt_re_cq *cq,
			   struct bnxt_re_ucontext *cntx, struct bnxt_re_cq_resp *resp,
			   struct bnxt_re_cq_req *ureq);
char *bnxt_re_hdbr_dump(struct bnxt_re_dev *rdev, int group, bool user);

dma_addr_t bnxt_re_hdbr_kaddr_to_dma(struct bnxt_re_hdbr_app *app, u64 kaddr);
#endif
