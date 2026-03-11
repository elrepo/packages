/* Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2025 Broadcom Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef HAVE_BUILD_SKB
static struct sk_buff *_bnxt_alloc_rx_frag(struct bnxt *bp, dma_addr_t *mapping, gfp_t gfp)
{
	struct pci_dev *pdev = bp->pdev;
	struct sk_buff *skb;
	u8 *data;

	skb = netdev_alloc_skb(bp->dev, bp->rx_buf_size);
	if (!skb)
		return NULL;
	data = skb->data;
	*mapping = dma_map_single_attrs(&pdev->dev, data + bp->rx_dma_offset,
					bp->rx_buf_use_size, bp->rx_dir,
					DMA_ATTR_WEAK_ORDERING);
	if (dma_mapping_error(&pdev->dev, *mapping)) {
		dev_kfree_skb(skb);
		return NULL;
	}
	return skb;
}

static struct sk_buff *bnxt_rx_skb(struct bnxt *bp,
				   struct bnxt_rx_ring_info *rxr, u16 cons,
				   void *data, u8 *data_ptr,
				   dma_addr_t dma_addr,
				   unsigned int offset_and_len)
{
	struct sk_buff *skb = data;
	u16 prod = rxr->rx_prod;
	int err;

	err = bnxt_alloc_rx_data(bp, rxr, prod, GFP_ATOMIC);
	if (unlikely(err)) {
		bnxt_reuse_rx_data(rxr, cons, skb);
		return NULL;
	}
	dma_unmap_single_attrs(&bp->pdev->dev, dma_addr, bp->rx_buf_use_size,
			       bp->rx_dir, DMA_ATTR_WEAK_ORDERING);
	skb_reserve(skb, bp->rx_offset);
	skb_put(skb, offset_and_len & 0xffff);
	return skb;
}

void bnxt_free_one_rx_ring(struct bnxt *bp, struct bnxt_rx_ring_info *rxr)
{
	int i, max_idx = bp->rx_nr_pages * RX_DESC_CNT;
	struct pci_dev *pdev = bp->pdev;

	for (i = 0; i < max_idx; i++) {
		struct bnxt_sw_rx_bd *rx_buf = &rxr->rx_buf_ring[i];
		dma_addr_t mapping = rx_buf->mapping;
		struct sk_buff *data = rx_buf->data;

		if (!data)
			continue;

		dma_unmap_single_attrs(&pdev->dev, mapping,
				       bp->rx_buf_use_size,
				       bp->rx_dir,
				       DMA_ATTR_WEAK_ORDERING);
		dev_kfree_skb_any(data);
		rx_buf->data = NULL;
	}
}

static struct page *__bnxt_alloc_rx_page(struct bnxt *bp, dma_addr_t *mapping,
					 struct bnxt_rx_ring_info *rxr, gfp_t gfp)
{
	return NULL;
}
#else
#ifndef HAVE_PAGE_POOL_FREE_VA

static inline u8 *_bnxt_alloc_rx_frag(struct bnxt *bp, dma_addr_t *mapping, gfp_t gfp)
{
	struct pci_dev *pdev = bp->pdev;
	u8 *data;

	if (gfp == GFP_ATOMIC)
		data = napi_alloc_frag(bp->rx_buf_size);
	else
		data = netdev_alloc_frag(bp->rx_buf_size);
	if (!data)
		return NULL;

	*mapping = dma_map_single_attrs(&pdev->dev, data + bp->rx_dma_offset,
					bp->rx_buf_use_size, bp->rx_dir,
					DMA_ATTR_WEAK_ORDERING);

	if (dma_mapping_error(&pdev->dev, *mapping)) {
		skb_free_frag(data);
		data = NULL;
	}
	return data;
}

static struct sk_buff *bnxt_rx_skb(struct bnxt *bp, struct bnxt_rx_ring_info *rxr,
				   u16 cons, void *data, u8 *data_ptr,
				   dma_addr_t dma_addr, unsigned int offset_and_len)
{
	struct sk_buff *skb;
	u16 prod;
	int err;

	prod = rxr->rx_prod;

	err = bnxt_alloc_rx_data(bp, rxr, prod, GFP_ATOMIC);
	if (unlikely(err)) {
		bnxt_reuse_rx_data(rxr, cons, data);
		return NULL;
	}

	skb = napi_build_skb(data, bp->rx_buf_size);
	dma_unmap_single_attrs(&bp->pdev->dev, dma_addr, bp->rx_buf_use_size,
			       bp->rx_dir, DMA_ATTR_WEAK_ORDERING);
	if (!skb) {
		skb_free_frag(data);
		return NULL;
	}

	skb_reserve(skb, bp->rx_offset);
	skb_put(skb, offset_and_len & 0xffff);
	return skb;
}
#endif

#ifndef CONFIG_PAGE_POOL
static struct page *__bnxt_alloc_rx_page(struct bnxt *bp, dma_addr_t *mapping,
					 struct bnxt_rx_ring_info *rxr,
					 unsigned int *page_offset, gfp_t gfp)
{
	struct device *dev = &bp->pdev->dev;
	unsigned int offset = 0;
	struct page *page;

	if (PAGE_SIZE <= BNXT_RX_PAGE_SIZE) {
		page = alloc_page(gfp);
		if (!page)
			return NULL;
		goto map_page;
	}
	page = rxr->rx_page;
	if (!page) {
		page = alloc_page(gfp);
		if (!page)
			return NULL;
		rxr->rx_page = page;
		rxr->rx_page_offset = 0;
	}
	offset = rxr->rx_page_offset;
	rxr->rx_page_offset += BNXT_RX_PAGE_SIZE;
	if (rxr->rx_page_offset == PAGE_SIZE)
		rxr->rx_page = NULL;
	else
		get_page(page);

map_page:
	*mapping = dma_map_page_attrs(dev, page, offset, BNXT_RX_PAGE_SIZE,
				      bp->rx_dir, DMA_ATTR_WEAK_ORDERING);
	if (dma_mapping_error(&bp->pdev->dev, *mapping)) {
		__free_page(page);
		return NULL;
	}

	if (page_offset)
		*page_offset = offset;

	return page;
}

static netmem_ref __bnxt_alloc_rx_netmem(struct bnxt *bp, dma_addr_t *mapping,
					 struct bnxt_rx_ring_info *rxr,
					 unsigned int *offset, gfp_t gfp)
{
	return __bnxt_alloc_rx_page(bp, mapping, rxr, offset, gfp);
}
#endif
#endif
