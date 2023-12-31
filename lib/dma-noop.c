// SPDX-License-Identifier: GPL-2.0
/*
 *	lib/dma-noop.c
 *
 * DMA operations that map to physical addresses without flushing memory.
 */
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/pfn.h>

static void *dma_noop_alloc(struct device *dev, size_t size,
			    dma_addr_t *dma_handle, gfp_t gfp,
			    unsigned long attrs)
{
	void *ret;

	ret = (void *)__get_free_pages(gfp, get_order(size));
	if (ret)
		*dma_handle = virt_to_phys(ret) - PFN_PHYS(dev->dma_pfn_offset);

	return ret;
}

static void dma_noop_free(struct device *dev, size_t size,
			  void *cpu_addr, dma_addr_t dma_addr,
			  unsigned long attrs)
{
	free_pages((unsigned long)cpu_addr, get_order(size));
}

static dma_addr_t dma_noop_map_page(struct device *dev, struct page *page,
				      unsigned long offset, size_t size,
				      enum dma_data_direction dir,
				      unsigned long attrs)
{
	return page_to_phys(page) + offset - PFN_PHYS(dev->dma_pfn_offset);
}

static int dma_noop_map_sg(struct device *dev, struct scatterlist *sgl, int nents,
			     enum dma_data_direction dir,
			     unsigned long attrs)
{
	int i;
	struct scatterlist *sg;

	for_each_sg(sgl, sg, nents, i) {
		dma_addr_t offset = PFN_PHYS(dev->dma_pfn_offset);
		void *va;

		BUG_ON(!sg_page(sg));
		va = sg_virt(sg);
		sg_dma_address(sg) = (dma_addr_t)virt_to_phys(va) - offset;
		sg_dma_len(sg) = sg->length;
	}

	return nents;
}

static int dma_noop_supported(struct device *dev, u64 mask)
{
#ifdef CONFIG_ZONE_DMA
	if (mask < DMA_BIT_MASK(ARCH_ZONE_DMA_BITS))
	return 0;
#else
	/*
	 * Because 32-bit DMA masks are so common we expect every architecture
	 * to be able to satisfy them - either by not supporting more physical
	 * memory, or by providing a ZONE_DMA32.  If neither is the case, the
	 * architecture needs to use an IOMMU instead of the direct mapping.
	 */
	if (dev->bus_dma_mask && mask > dev->bus_dma_mask)
		return 0;
#endif
	return 1;
}

const struct dma_map_ops dma_noop_ops = {
	.alloc			= dma_noop_alloc,
	.free			= dma_noop_free,
	.map_page		= dma_noop_map_page,
	.map_sg			= dma_noop_map_sg,
	dma_supported		= dma_noop_supported
};

EXPORT_SYMBOL(dma_noop_ops);
