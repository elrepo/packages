/*
 * Copyright (c) 2015-2025, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: IB Verbs interpreter (header)
 */

#ifndef __BNXT_RE_IB_VERBS_H__
#define __BNXT_RE_IB_VERBS_H__

#include <linux/mm.h>
#include <rdma/ib_verbs.h>

#include "bnxt_re-abi.h"
#include "compat.h"
#include "bnxt_re.h"

#define BNXT_RE_ROCE_V2_UDP_SPORT	0x8CD1
#define BNXT_RE_QP_RANDOM_QKEY		0x81818181

#ifdef HAVE_IB_ARG_CONST_CHANGE
#define CONST_STRUCT const struct
#else
#define CONST_STRUCT struct
#endif

#ifdef HAVE_RDMA_AH_ATTR
typedef struct rdma_ah_attr RDMA_AH_ATTR;
#else
typedef struct ib_ah_attr RDMA_AH_ATTR;
#endif
#ifdef HAVE_RDMA_AH_INIT_ATTR
typedef struct rdma_ah_init_attr RDMA_AH_ATTR_IN;
#else
typedef RDMA_AH_ATTR RDMA_AH_ATTR_IN;
#endif

struct bnxt_re_gid_ctx {
	u32			idx;
	u32			refcnt;
};

struct bnxt_re_legacy_fence_data {
	u32 size;
	void *va;
	dma_addr_t dma_addr;
	struct bnxt_re_mr *mr;
	struct ib_mw *mw;
	struct bnxt_qplib_swqe bind_wqe;
	u32 bind_rkey;
};

struct bnxt_re_pd {
	struct ib_pd		ib_pd;
	struct bnxt_re_dev	*rdev;
	struct bnxt_qplib_pd	qplib_pd;
	struct bnxt_re_legacy_fence_data fence;
};

#define BNXT_RE_AH_DEST_DELAY	10
#define BNXT_RE_AH_DEST_RETRY	10000
#define BNXT_RE_AH_UNINIT_RETRY	100

struct bnxt_destroy_ah {
	struct delayed_work	dwork;
	struct bnxt_re_dev	*rdev;
	struct bnxt_qplib_ah	qplib_ah;
	refcount_t		*ah_ref_cnt;
	u32			retry;
};

struct bnxt_re_ah {
	struct ib_ah		ib_ah;
	struct bnxt_re_dev	*rdev;
	struct bnxt_qplib_ah	qplib_ah;
	refcount_t		*ref_cnt;
};

struct bnxt_re_flow {
	struct ib_flow		ib_flow;
	struct list_head	list;
	struct bnxt_re_dev	*rdev;
	struct bnxt_re_qp	*qp;
	enum ib_flow_attr_type	type;
	struct bnxt_dcqcn_session_entry *session;
};

struct bnxt_re_srq {
	struct ib_srq		ib_srq;
	struct list_head	res_list;
	struct list_head	srq_list;
	struct bnxt_re_ucontext *uctx;
	struct bnxt_re_dev	*rdev;
	struct bnxt_qplib_srq	qplib_srq;
	struct ib_umem		*umem;
	spinlock_t		lock;
	void			*srq_toggle_page;
	struct rdma_user_mmap_entry *srq_toggle_mmap; /*ABI v7 support */
	struct hlist_node	hash_entry;
	struct rdma_user_mmap_entry *srq_hdbr_mmap;
};

union ip_addr {
	u32 ipv4_addr;
	u8  ipv6_addr[16];
};

struct bnxt_re_qp_info_entry {
	union ib_gid		sgid;
	union ib_gid 		dgid;
	union ip_addr		s_ip;
	union ip_addr		d_ip;
	u16			s_port;
#define BNXT_RE_QP_DEST_PORT	4791
	u16			d_port;
	u32			rate_limit;
};

struct bnxt_re_qp {
	struct ib_qp		ib_qp;
	struct list_head	list;
	struct list_head	res_list;
	struct bnxt_re_dev	*rdev;
	spinlock_t		sq_lock;
	spinlock_t		rq_lock;
	struct bnxt_qplib_qp	qplib_qp;
	struct ib_umem		*sumem;
	struct ib_umem		*rumem;
	struct ib_umem		*sqprod;
	struct ib_umem		*sqcons;
	struct ib_umem		*rqprod;
	struct ib_umem		*rqcons;
	/* QP1 */
	u32			send_psn;
	struct ib_ud_header	qp1_hdr;
	struct bnxt_re_cq	*scq;
	struct bnxt_re_cq	*rcq;
	struct dentry		*qp_info_pdev_dentry;
	struct bnxt_re_qp_info_entry qp_info_entry;
	void			*qp_data;
	bool			is_snapdump_captured;
	struct rdma_user_mmap_entry *sq_hdbr_mmap;
	struct rdma_user_mmap_entry *rq_hdbr_mmap;
	struct list_head	flow_list;
	/* Serialize access to flow list */
	struct mutex		flow_lock;
#define BNXT_RE_QPERR_DUMP_SLOTS 8
	struct work_struct	dump_slot_task;
	u8			req_err_state_reason;
	u8			res_err_state_reason;
	u16			sq_cons_idx;
	u16			rq_cons_idx;

	/* Below members added for DV support */
	bool			is_dv_qp;
	struct bnxt_re_dv_umem *sq_umem;
	struct bnxt_re_dv_umem *rq_umem;
	const struct ib_gid_attr *sgid_attr;
};

struct bnxt_re_cq {
	struct ib_cq		ib_cq;
	struct list_head	res_list;
	struct list_head	cq_list;
	struct bnxt_re_dev	*rdev;
	struct bnxt_re_ucontext *uctx;
	spinlock_t              cq_lock;
	u16			cq_count;
	u16			cq_period;
	struct bnxt_qplib_cq	qplib_cq;
	struct bnxt_qplib_cqe	*cql;
#define MAX_CQL_PER_POLL	1024
	u32			max_cql;
	struct ib_umem		*umem;
	struct ib_umem		*resize_umem;
	struct ib_ucontext	*context;
	int			resize_cqe;
	struct hlist_node	hash_entry;
	/* list of cq per uctx. Used only for Thor-2 */
	void			*cq_toggle_page;
	void			*dbr_recov_cq_page;
	struct rdma_user_mmap_entry *cq_toggle_mmap; /* ABI v7 support */
	struct rdma_user_mmap_entry *cq_hdbr_mmap;
	bool			is_dbr_soft_cq;
	bool			is_snapdump_captured;
	struct ib_umem		*cqprod;
	struct ib_umem		*cqcons;
	bool			is_dv_cq;
	struct bnxt_re_dv_umem	*umem_handle;
};

struct bnxt_re_mr {
	struct bnxt_re_dev	*rdev;
	struct list_head	res_list;
	struct ib_mr		ib_mr;
	struct ib_umem		*ib_umem;
	struct bnxt_qplib_mrw	qplib_mr;
#ifdef HAVE_IB_ALLOC_MR
	u32			npages;
	u64			*pages;
	struct bnxt_qplib_frpl	qplib_frpl;
#endif
#ifdef CONFIG_INFINIBAND_PEER_MEM
	atomic_t		invalidated;
	struct completion	invalidation_comp;
	bool			is_peer_mem;
#endif
	bool                    is_invalcb_active;
	bool			is_dmabuf;
};

#ifdef HAVE_IB_FMR
struct bnxt_re_fmr {
	struct bnxt_re_dev	*rdev;
	struct ib_fmr		ib_fmr;
	struct bnxt_qplib_mrw	qplib_fmr;
};
#endif

struct bnxt_re_mw {
	struct bnxt_re_dev	*rdev;
	struct ib_mw		ib_mw;
	struct bnxt_qplib_mrw	qplib_mw;
};

#ifdef HAVE_DISASSOCIATE_UCNTX
#ifndef HAVE_RDMA_USER_MMAP_IO
struct bnxt_re_vma_data {
	struct list_head	vma_list;
	struct mutex		*list_mutex;
	struct vm_area_struct	*vma;
};
#endif
#endif

struct bnxt_re_ucontext {
	struct ib_ucontext	ib_uctx;
	struct bnxt_re_dev	*rdev;
	struct list_head	res_list;
	struct list_head	cq_list;
	struct list_head	srq_list;
	struct bnxt_qplib_dpi	dpi;
	struct bnxt_qplib_dpi	wcdpi;
#ifdef HAVE_DISASSOCIATE_UCNTX
#ifndef HAVE_RDMA_USER_MMAP_IO
	struct list_head	vma_list_head; /*All vma's on this context */
	struct mutex		list_mutex;
#endif
#endif
	uint64_t		cmask;
	struct mutex		cq_lock;	/* Protect cq list */
	struct mutex		srq_lock;	/* Protect srq list */
	void			*dbr_recov_cq_page;
	struct bnxt_re_cq	*dbr_recov_cq;
	void			*hdbr_app;
	bool			small_recv_wqe_sup;
	struct rdma_user_mmap_entry *dbr_recovery_cq_mmap;
	struct rdma_user_mmap_entry *dbr_pacing_mmap; /* ABI v7 support */
	struct rdma_user_mmap_entry *uc_db_mmap;
	struct rdma_user_mmap_entry *wc_db_mmap; /* ABI v7 support */
	struct rdma_user_mmap_entry *dbr_pacing_bar_mmap; /* ABI v7 support */
};

struct bnxt_re_ah_info {
	union ib_gid		sgid;
#ifdef RDMA_CORE_CAP_PROT_ROCE_UDP_ENCAP
	struct ib_gid_attr	sgid_attr;
#endif
	u16			vlan_tag;
	u8			nw_type;
};

struct bnxt_re_user_mmap_entry {
	struct rdma_user_mmap_entry rdma_entry;
	struct bnxt_re_ucontext *uctx;
	dma_addr_t dma_addr;
	u64 cpu_addr;
	u8 mmap_flag;
};

#ifdef HAVE_UAPI_DEF
struct bnxt_re_alloc_dbr_obj {
	struct bnxt_re_dev *rdev;
	struct bnxt_re_dv_db_region_attr attr;
	struct bnxt_qplib_dpi dpi;
	struct bnxt_re_user_mmap_entry *entry;
};
#endif

struct net_device *bnxt_re_get_netdev(struct ib_device *ibdev,
				      PORT_NUM port_num);

#ifdef HAVE_IB_QUERY_DEVICE_UDATA
int bnxt_re_query_device(struct ib_device *ibdev,
			 struct ib_device_attr *ib_attr,
			 struct ib_udata *udata);
#else
int bnxt_re_query_device(struct ib_device *ibdev,
			 struct ib_device_attr *device_attr);
#endif
int bnxt_re_modify_device(struct ib_device *ibdev,
			  int device_modify_mask,
			  struct ib_device_modify *device_modify);
int bnxt_re_query_port(struct ib_device *ibdev, PORT_NUM port_num,
		       struct ib_port_attr *port_attr);
#ifdef HAVE_IB_GET_PORT_IMMUTABLE
int bnxt_re_get_port_immutable(struct ib_device *ibdev, PORT_NUM port_num,
			       struct ib_port_immutable *immutable);
#endif
#ifdef HAVE_IB_GET_DEV_FW_STR
void bnxt_re_query_fw_str(struct ib_device *ibdev, char *str, size_t str_len);
#endif
int bnxt_re_query_pkey(struct ib_device *ibdev, PORT_NUM port_num,
		       u16 index, u16 *pkey);
#ifdef HAVE_IB_ADD_DEL_GID
#ifdef HAVE_SIMPLIFIED_ADD_DEL_GID
int bnxt_re_del_gid(const struct ib_gid_attr *attr, void **context);
#ifdef HAVE_SIMPLER_ADD_GID
int bnxt_re_add_gid(const struct ib_gid_attr *attr, void **context);
#else
int bnxt_re_add_gid(const union ib_gid *gid,
		    const struct ib_gid_attr *attr, void **context);
#endif
#else
int bnxt_re_del_gid(struct ib_device *ibdev, u8 port_num,
		    unsigned int index, void **context);
int bnxt_re_add_gid(struct ib_device *ibdev, u8 port_num,
		    unsigned int index, const union ib_gid *gid,
		    const struct ib_gid_attr *attr, void **context);
#endif
#endif
#ifdef HAVE_IB_MODIFY_GID
int bnxt_re_modify_gid(struct ib_device *ibdev, u8 port_num,
		    unsigned int index, const union ib_gid *gid,
		    const struct ib_gid_attr *attr, void **context);
#endif
int bnxt_re_query_gid(struct ib_device *ibdev, PORT_NUM port_num,
		      int index, union ib_gid *gid);
enum rdma_link_layer bnxt_re_get_link_layer(struct ib_device *ibdev,
					    PORT_NUM port_num);

ALLOC_PD_RET bnxt_re_alloc_pd(ALLOC_PD_IN *pd_in,
#ifdef HAVE_UCONTEXT_IN_ALLOC_PD
		struct ib_ucontext *ucontext,
#endif
		struct ib_udata *udata);

#ifdef HAVE_DEALLOC_PD_UDATA
DEALLOC_PD_RET bnxt_re_dealloc_pd(struct ib_pd *pd, struct ib_udata *udata);
#else
DEALLOC_PD_RET bnxt_re_dealloc_pd(struct ib_pd *pd);
#endif

#ifdef HAVE_IB_CREATE_AH_UDATA
CREATE_AH_RET bnxt_re_create_ah(CREATE_AH_IN *ah_in,
				RDMA_AH_ATTR_IN *attr,
#ifndef HAVE_RDMA_AH_INIT_ATTR
#ifdef HAVE_SLEEPABLE_AH
				u32 flags,
#endif
#endif
				struct ib_udata *udata);
#else
struct ib_ah *bnxt_re_create_ah(struct ib_pd *pd,
				RDMA_AH_ATTR_IN *attr);
#endif

int bnxt_re_query_ah(struct ib_ah *ah, RDMA_AH_ATTR *ah_attr);

#ifdef HAVE_SLEEPABLE_AH
DESTROY_AH_RET bnxt_re_destroy_ah(struct ib_ah *ib_ah, u32 flags);
#else
int bnxt_re_destroy_ah(struct ib_ah *ah);
#endif

CREATE_SRQ_RET bnxt_re_create_srq(CREATE_SRQ_IN *srq_in,
				  struct ib_srq_init_attr *srq_init_attr,
				  struct ib_udata *udata);
int bnxt_re_modify_srq(struct ib_srq *srq, struct ib_srq_attr *srq_attr,
		       enum ib_srq_attr_mask srq_attr_mask,
		       struct ib_udata *udata);
int bnxt_re_query_srq(struct ib_srq *srq, struct ib_srq_attr *srq_attr);
DESTROY_SRQ_RET bnxt_re_destroy_srq(struct ib_srq *srq
#ifdef HAVE_DESTROY_SRQ_UDATA
	, struct ib_udata *udata
#endif
	);
int bnxt_re_post_srq_recv(struct ib_srq *ib_srq, CONST_STRUCT ib_recv_wr *wr,
			  CONST_STRUCT ib_recv_wr **bad_wr);
ALLOC_QP_RET bnxt_re_create_qp(ALLOC_QP_IN *qp_in,
			       struct ib_qp_init_attr *qp_init_attr,
			       struct ib_udata *udata);
int bnxt_re_modify_qp(struct ib_qp *qp, struct ib_qp_attr *qp_attr,
		      int qp_attr_mask, struct ib_udata *udata);
int bnxt_re_query_qp(struct ib_qp *qp, struct ib_qp_attr *qp_attr,
		     int qp_attr_mask, struct ib_qp_init_attr *qp_init_attr);
int bnxt_re_destroy_qp(struct ib_qp *qp
#ifdef HAVE_DESTROY_QP_UDATA
		, struct ib_udata *udata
#endif
		);
struct ib_flow *bnxt_re_create_flow(struct ib_qp *ib_qp, struct ib_flow_attr *attr,
#if defined(HAVE_CREATE_FLOW_DOMAIN_UDATA)
				    int domain, struct ib_udata *udata);
#elif defined(HAVE_CREATE_FLOW_UDATA)
				    struct ib_udata *udata);
#else
				    int domain);
#endif
int bnxt_re_destroy_flow(struct ib_flow *flow_id);
int bnxt_re_post_send(struct ib_qp *ib_qp, CONST_STRUCT ib_send_wr *wr,
		      CONST_STRUCT ib_send_wr **bad_wr);
int bnxt_re_post_recv(struct ib_qp *ib_qp, CONST_STRUCT ib_recv_wr *wr,
		      CONST_STRUCT ib_recv_wr **bad_wr);
#ifdef HAVE_IB_CQ_CREATE_HAS_UVERBS_ATTR_BUNDLE
ALLOC_CQ_RET bnxt_re_create_cq(ALLOC_CQ_IN * cq_in,
			       const struct ib_cq_init_attr *attr,
			       struct uverbs_attr_bundle *attrs);
#else
ALLOC_CQ_RET bnxt_re_create_cq(ALLOC_CQ_IN *cq_in,
			       const struct ib_cq_init_attr *attr,
#ifdef HAVE_CREATE_CQ_UCONTEXT
				struct ib_ucontext *context,
#endif
				struct ib_udata *udata);
#endif
int bnxt_re_modify_cq(struct ib_cq *cq, u16 cq_count, u16 cq_period);
DESTROY_CQ_RET bnxt_re_destroy_cq(struct ib_cq *cq
#ifdef HAVE_DESTROY_CQ_UDATA
	       , struct ib_udata *udata
#endif
		);
int bnxt_re_resize_cq(struct ib_cq *cq, int cqe, struct ib_udata *udata);
int bnxt_re_poll_cq(struct ib_cq *cq, int num_entries, struct ib_wc *wc);
int bnxt_re_req_notify_cq(struct ib_cq *cq, enum ib_cq_notify_flags flags);
struct ib_mr *bnxt_re_get_dma_mr(struct ib_pd *pd, int mr_access_flags);
#ifdef HAVE_IB_MAP_MR_SG
int bnxt_re_map_mr_sg(struct ib_mr *ib_mr, struct scatterlist *sg, int sg_nents
#ifdef HAVE_IB_MAP_MR_SG_PAGE_SIZE
		      , unsigned int *sg_offset
#else
#ifdef HAVE_IB_MAP_MR_SG_OFFSET
		      , unsigned int sg_offset
#endif
#endif
		      );
#endif
#ifdef HAVE_IB_ALLOC_MR
struct ib_mr *bnxt_re_alloc_mr(struct ib_pd *ib_pd, enum ib_mr_type mr_type,
			       u32 max_num_sg
#ifdef HAVE_ALLOC_MR_UDATA
			       , struct ib_udata *udata
#endif
			       );
#endif
int bnxt_re_dereg_mr(struct ib_mr *mr
#ifdef HAVE_DEREG_MR_UDATA
		, struct ib_udata *udata
#endif
		);
#ifdef HAVE_IB_MW_TYPE
ALLOC_MW_RET bnxt_re_alloc_mw
#ifndef HAVE_ALLOC_MW_IN_IB_CORE
	(struct ib_pd *ib_pd, enum ib_mw_type type
#else
	(struct ib_mw *mw
#endif /* HAVE_ALLOC_MW_RET_IB_MW*/
#ifdef HAVE_ALLOW_MW_WITH_UDATA
	 , struct ib_udata *udata
#endif
	 );
#else
ALLOC_MW_RET bnxt_re_alloc_mw(struct ib_pd *ib_pd);
#endif
int bnxt_re_dealloc_mw(struct ib_mw *mw);
struct ib_mr *bnxt_re_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
				  u64 virt_addr, int mr_access_flags,
				  struct ib_udata *udata);
#ifdef HAVE_IB_UMEM_DMABUF
struct ib_mr *bnxt_re_reg_user_mr_dmabuf(struct ib_pd *ib_pd, u64 start,
					 u64 length, u64 virt_addr,
					 int fd, int mr_access_flags,
#ifdef HAVE_REG_USER_MR_DMABUF_HAS_UVERBS_ATTR_BUNDLE
					 struct uverbs_attr_bundle *attrs);
#else
					 struct ib_udata *udata);
#endif
#endif
#ifdef HAVE_IB_REREG_USER_MR
REREG_USER_MR_RET
bnxt_re_rereg_user_mr(struct ib_mr *mr, int flags, u64 start, u64 length,
		      u64 virt_addr, int mr_access_flags, struct ib_pd *pd,
		      struct ib_udata *udata);
#endif

ALLOC_UCONTEXT_RET bnxt_re_alloc_ucontext(ALLOC_UCONTEXT_IN *uctx_in,
					  struct ib_udata *udata);
DEALLOC_UCONTEXT_RET bnxt_re_dealloc_ucontext(struct ib_ucontext *ib_uctx);
int bnxt_re_mmap(struct ib_ucontext *context, struct vm_area_struct *vma);
void bnxt_re_mmap_free(struct rdma_user_mmap_entry *rdma_entry);
bool bnxt_re_mmap_entry_insert_compat(struct bnxt_re_ucontext *uctx,
				      u64 cpu_addr, dma_addr_t dma_addr,
				      u8 user_mmap_hint, u64 *offset,
				      struct rdma_user_mmap_entry **rdma_entry);
void bnxt_re_mmap_entry_remove_compat(struct rdma_user_mmap_entry *entry);

#ifdef HAVE_PROCESS_MAD_U32_PORT
int bnxt_re_process_mad(struct ib_device *device, int process_mad_flags,
			u32 port_num, const struct ib_wc *in_wc,
			const struct ib_grh *in_grh,
			const struct ib_mad *in_mad, struct ib_mad *out_mad,
			size_t *out_mad_size, u16 *out_mad_pkey_index);
#else
#ifndef HAVE_PROCESS_MAD_IB_MAD_HDR
int bnxt_re_process_mad(struct ib_device *ibdev, int mad_flags, u8 port_num,
			const struct ib_wc *wc, const struct ib_grh *grh,
			const struct ib_mad *in_mad, struct ib_mad *out_mad,
			size_t *out_mad_size, u16 *out_mad_pkey_index);
#else
int bnxt_re_process_mad(struct ib_device *device, int process_mad_flags,
			u8 port_num, const struct ib_wc *in_wc,
			const struct ib_grh *in_grh,
			const struct ib_mad_hdr *in_mad, size_t in_mad_size,
			struct ib_mad_hdr *out_mad, size_t *out_mad_size,
			u16 *out_mad_pkey_index);
#endif
#endif

unsigned long bnxt_re_lock_cqs(struct bnxt_re_qp *qp);
void bnxt_re_unlock_cqs(struct bnxt_re_qp *qp, unsigned long flags);

#ifdef HAVE_DISASSOCIATE_UCNTX
void bnxt_re_disassociate_ucntx(struct ib_ucontext *ibcontext);
#ifndef HAVE_RDMA_USER_MMAP_IO
int bnxt_re_set_vma_data(struct bnxt_re_ucontext *uctx,
			 struct vm_area_struct *vma);
#endif
#endif

int bnxt_re_capture_qpdump(struct bnxt_re_qp *qp, void *qp_dump_buf,
			   u32 buf_len, bool is_live);
int bnxt_re_modify_udp_sport(struct bnxt_re_qp *qp, uint32_t port);
int bnxt_re_modify_rate_limit(struct bnxt_re_qp *qp, uint32_t rate_limit);

static inline enum ib_qp_type  __from_hw_to_ib_qp_type(u8 type)
{
	switch (type) {
	case CMDQ_CREATE_QP1_TYPE_GSI:
	case CMDQ_CREATE_QP_TYPE_GSI:
		return IB_QPT_GSI;
	case CMDQ_CREATE_QP_TYPE_RC:
		return IB_QPT_RC;
	case CMDQ_CREATE_QP_TYPE_UD:
		return IB_QPT_UD;
	case CMDQ_CREATE_QP_TYPE_RAW_ETHERTYPE:
		return IB_QPT_RAW_ETHERTYPE;
	default:
		return IB_QPT_MAX;
	}
}

static inline u8 __from_ib_qp_state(enum ib_qp_state state)
{
	switch (state) {
	case IB_QPS_RESET:
		return CMDQ_MODIFY_QP_NEW_STATE_RESET;
	case IB_QPS_INIT:
		return CMDQ_MODIFY_QP_NEW_STATE_INIT;
	case IB_QPS_RTR:
		return CMDQ_MODIFY_QP_NEW_STATE_RTR;
	case IB_QPS_RTS:
		return CMDQ_MODIFY_QP_NEW_STATE_RTS;
	case IB_QPS_SQD:
		return CMDQ_MODIFY_QP_NEW_STATE_SQD;
	case IB_QPS_SQE:
		return CMDQ_MODIFY_QP_NEW_STATE_SQE;
	case IB_QPS_ERR:
	default:
		return CMDQ_MODIFY_QP_NEW_STATE_ERR;
	}
}

static inline u32 __from_ib_mtu(enum ib_mtu mtu)
{
	switch (mtu) {
	case IB_MTU_256:
		return CMDQ_MODIFY_QP_PATH_MTU_MTU_256;
	case IB_MTU_512:
		return CMDQ_MODIFY_QP_PATH_MTU_MTU_512;
	case IB_MTU_1024:
		return CMDQ_MODIFY_QP_PATH_MTU_MTU_1024;
	case IB_MTU_2048:
		return CMDQ_MODIFY_QP_PATH_MTU_MTU_2048;
	case IB_MTU_4096:
		return CMDQ_MODIFY_QP_PATH_MTU_MTU_4096;
/*	case IB_MTU_8192:
 *	return CMDQ_MODIFY_QP_PATH_MTU_MTU_8192;
 */
	default:
		return CMDQ_MODIFY_QP_PATH_MTU_MTU_2048;
	}
}

static inline enum ib_mtu __to_ib_mtu(u32 mtu)
{
	switch (mtu & CREQ_QUERY_QP_RESP_SB_PATH_MTU_MASK) {
	case CMDQ_MODIFY_QP_PATH_MTU_MTU_256:
		return IB_MTU_256;
	case CMDQ_MODIFY_QP_PATH_MTU_MTU_512:
		return IB_MTU_512;
	case CMDQ_MODIFY_QP_PATH_MTU_MTU_1024:
		return IB_MTU_1024;
	case CMDQ_MODIFY_QP_PATH_MTU_MTU_2048:
		return IB_MTU_2048;
	case CMDQ_MODIFY_QP_PATH_MTU_MTU_4096:
		return IB_MTU_4096;
	case CMDQ_MODIFY_QP_PATH_MTU_MTU_8192:
		return IB_MTU_8192;
	default:
		return IB_MTU_2048;
	}
}

static inline enum ib_qp_state __to_ib_qp_state(u8 state)
{
	switch (state) {
	case CMDQ_MODIFY_QP_NEW_STATE_RESET:
		return IB_QPS_RESET;
	case CMDQ_MODIFY_QP_NEW_STATE_INIT:
		return IB_QPS_INIT;
	case CMDQ_MODIFY_QP_NEW_STATE_RTR:
		return IB_QPS_RTR;
	case CMDQ_MODIFY_QP_NEW_STATE_RTS:
		return IB_QPS_RTS;
	case CMDQ_MODIFY_QP_NEW_STATE_SQD:
		return IB_QPS_SQD;
	case CMDQ_MODIFY_QP_NEW_STATE_SQE:
		return IB_QPS_SQE;
	case CMDQ_MODIFY_QP_NEW_STATE_ERR:
	default:
		return IB_QPS_ERR;
	}
}

static inline const char *__to_qp_type_str(u8 type)
{
	switch (type) {
	case CMDQ_CREATE_QP1_TYPE_GSI:
	case CMDQ_CREATE_QP_TYPE_GSI:
		return "GSI";
	case CMDQ_CREATE_QP_TYPE_RC:
		return "RC";
	case CMDQ_CREATE_QP_TYPE_UD:
		return "UD";
	case CMDQ_CREATE_QP_TYPE_RAW_ETHERTYPE:
		return "RAW_ETH";
	default:
		return "NotSupp";
	}
}

static inline const char  *__to_qp_state_str(u8 state)
{
	switch (state) {
	case CMDQ_MODIFY_QP_NEW_STATE_RESET:
		return "RESET";
	case CMDQ_MODIFY_QP_NEW_STATE_INIT:
		return "INIT";
	case CMDQ_MODIFY_QP_NEW_STATE_RTR:
		return "RTR";
	case CMDQ_MODIFY_QP_NEW_STATE_RTS:
		return "RTS";
	case CMDQ_MODIFY_QP_NEW_STATE_SQD:
		return "SQD";
	case CMDQ_MODIFY_QP_NEW_STATE_SQE:
		return "SQE";
	case CMDQ_MODIFY_QP_NEW_STATE_ERR:
		return "ERR";
	default:
		return "NotSupp";
	}
}

static inline const char *__to_wqe_mode_str(u8 mode)
{
	switch (mode) {
	case BNXT_QPLIB_WQE_MODE_STATIC:
		return "STATIC";
	case BNXT_QPLIB_WQE_MODE_VARIABLE:
		return "VARIABLE";
	default:
		return "NotSupp";
	}
}

static inline u32 __to_ib_port_num(u16 port_id)
{
	return (u32)port_id + 1;
}

static inline int bnxt_re_init_pow2_flag(struct bnxt_re_uctx_req *req,
					 struct bnxt_re_uctx_resp *resp)
{
	resp->comp_mask |= BNXT_RE_COMP_MASK_UCNTX_POW2_DISABLED;
	if (!(req->comp_mask & BNXT_RE_COMP_MASK_REQ_UCNTX_POW2_SUPPORT)) {
		resp->comp_mask &= ~BNXT_RE_COMP_MASK_UCNTX_POW2_DISABLED;
		return -EINVAL;
	}
	return 0;
}

/* TBD - Improve BNXT_RE_COMP_MASK_UCNTX_POW2_DISABLED handling */
static inline u32 bnxt_re_init_depth(u32 ent, struct bnxt_re_ucontext *uctx)
{
	return uctx ? (uctx->cmask & BNXT_RE_COMP_MASK_UCNTX_POW2_DISABLED) ?
		       ent : roundup_pow_of_two(ent) : ent;
}

static inline int bnxt_re_init_rsvd_wqe_flag(struct bnxt_re_uctx_req *req,
					     struct bnxt_re_uctx_resp *resp,
					     bool genp5)
{
	resp->comp_mask |= BNXT_RE_COMP_MASK_UCNTX_RSVD_WQE_DISABLED;
	if (!(req->comp_mask & BNXT_RE_COMP_MASK_REQ_UCNTX_RSVD_WQE)) {
		resp->comp_mask &= ~BNXT_RE_COMP_MASK_UCNTX_RSVD_WQE_DISABLED;
		return -EINVAL;
	} else if (!genp5) {
		resp->comp_mask &= ~BNXT_RE_COMP_MASK_UCNTX_RSVD_WQE_DISABLED;
	}
	return 0;
}

static inline u32 bnxt_re_get_diff(struct bnxt_re_ucontext *uctx,
				   struct bnxt_qplib_chip_ctx *cctx)
{
	if (!uctx) {
		/* return res-wqe only for gen p4 for user resource */
		return _is_chip_p5_plus(cctx) ? 0 : BNXT_QPLIB_RESERVED_QP_WRS;
	} else if (uctx->cmask & BNXT_RE_COMP_MASK_UCNTX_RSVD_WQE_DISABLED) {
		return 0;
	}
	/* old lib */
	return BNXT_QPLIB_RESERVED_QP_WRS;
}

static inline int bnxt_re_init_qpmtu(struct bnxt_re_qp *qp, int mtu,
				     int mask, struct ib_qp_attr *qp_attr)
{
	int qpmtu, qpmtu_int;
	int ifmtu, ifmtu_int;

	ifmtu = iboe_get_mtu(mtu);
	ifmtu_int = ib_mtu_enum_to_int(ifmtu);
	qpmtu = ifmtu;
	qpmtu_int = ifmtu_int;
	if (mask & IB_QP_PATH_MTU) {
		qpmtu = qp_attr->path_mtu;
		qpmtu_int = ib_mtu_enum_to_int(qpmtu);
		if (qpmtu_int > ifmtu_int)
			return -EINVAL;
	}
	qp->qplib_qp.path_mtu = __from_ib_mtu(qpmtu);
	qp->qplib_qp.mtu = qpmtu_int;
	qp->qplib_qp.modify_flags |=
		CMDQ_MODIFY_QP_MODIFY_MASK_PATH_MTU;

	return 0;
}

void bnxt_re_update_shadow_ah(struct bnxt_re_dev *rdev);
void bnxt_re_handle_cqn(struct bnxt_qplib_cq *cq);
void bnxt_re_set_sginfo(struct bnxt_re_dev *rdev, struct ib_umem *umem,
			__u64 va, struct bnxt_qplib_sg_info *sginfo, u8 res);
u8 __from_ib_qp_type(enum ib_qp_type type);
int bnxt_re_setup_qp_hwqs(struct bnxt_re_qp *qp, bool is_dv_qp);
void bnxt_re_qp_free_umem(struct bnxt_re_qp *qp);
int bnxt_re_query_qp_attr(struct bnxt_re_qp *qp, struct ib_qp_attr *qp_attr,
			  int attr_mask, struct ib_qp_init_attr *qp_init_attr);
#endif
