// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "hif.h"
#include "hal.h"
#include "debug.h"

void ath12k_mgmt_irq_grp_setup(struct ath12k_mgmt *mgmt)
{
	int i;

	for (i = 0; i < mgmt->num_irq_grp; i++) {
		struct ath12k_mgmt_irq_grp *irq_grp = &mgmt->irq_grp[i];
#if LINUX_VERSION_IS_GEQ(6, 13, 0)
		INIT_WORK(&irq_grp->intr_wq, irq_grp->irq_grp_handler);
#else
		tasklet_setup(&irq_grp->intr_tq, irq_grp->irq_grp_handler);
#endif
	}
}
EXPORT_SYMBOL(ath12k_mgmt_irq_grp_setup);

void ath12k_mgmt_irq_grp_cleanup(struct ath12k_mgmt *mgmt)
{
	int i;

	for (i = 0; i < mgmt->num_irq_grp; i++) {
		struct ath12k_mgmt_irq_grp *irq_grp = &mgmt->irq_grp[i];
#if LINUX_VERSION_IS_GEQ(6, 13, 0)
		cancel_work_sync(&irq_grp->intr_wq);
#else
		tasklet_kill(&irq_grp->intr_tq);
#endif
	}
}
EXPORT_SYMBOL(ath12k_mgmt_irq_grp_cleanup);

void ath12k_mgmt_irq_grp_enable(struct ath12k_mgmt_irq_grp *irq_grp)
{
	int i;

	for (i = 0; i < irq_grp->num_irq; i++)
		enable_irq(irq_grp->irqs[i]);
}
EXPORT_SYMBOL(ath12k_mgmt_irq_grp_enable);

void ath12k_mgmt_irq_grp_disable(struct ath12k_mgmt_irq_grp *irq_grp)
{
	int i;

	for (i = 0; i < irq_grp->num_irq; i++)
		disable_irq_nosync(irq_grp->irqs[i]);
}

int ath12k_mgmt_device_init(struct ath12k_mgmt *mgmt)
{
	/* Skip device_init if mgmt is not supported */
	if (!mgmt)
		return 0;

	return ath12k_mgmt_arch_op_device_init(mgmt);
}

void ath12k_mgmt_device_deinit(struct ath12k_mgmt *mgmt)
{
	if (!mgmt)
		return;

	ath12k_mgmt_arch_op_device_deinit(mgmt);
}

static void ath12k_mgmt_srng_msi_setup(struct ath12k_base *ab,
				       struct hal_srng_params *ring_params,
				       enum hal_ring_type type, int grp_id)
{
	int num_vectors;
	u32 base_user_data, base_vector, addr_lo, addr_hi;
	int ret;

	switch (type) {
	case HAL_REO_DST_MGMT:
	case HAL_REO_EXCEPTION_MGMT:
		break;
	default:
		ath12k_dbg(ab, ATH12K_DBG_PCI,
			   "Ring (type=%u) not part of mgmt_group for msi setup", type);
		return;
	}

	ret = ath12k_hif_get_user_msi_vector(ab, "MGMT",
					     &num_vectors, &base_user_data,
					     &base_vector);
	if (ret)
		return;

	if (grp_id < 0) {
		ath12k_dbg(ab, ATH12K_DBG_PCI,
			   "Ring part of invalid mgmt_group; ring_type: %u, grp_id: %d",
			   type, grp_id);
		ring_params->msi_addr = 0;
		ring_params->msi_data = 0;
		return;
	}

	ath12k_hif_get_msi_address(ab, &addr_lo, &addr_hi);

	ring_params->msi_addr = addr_lo;
	ring_params->msi_addr |= (dma_addr_t)(((uint64_t)addr_hi) << 32);
	ring_params->msi_data = base_user_data + (grp_id % num_vectors);
	ring_params->flags |= HAL_SRNG_FLAGS_MSI_INTR;
}

int ath12k_mgmt_srng_setup(struct ath12k_base *ab, struct mgmt_srng *ring,
			   enum hal_ring_type type, int ring_num,
			   int mac_id, int grp_id, int num_entries)
{
	struct hal_srng_params params = { 0 };
	int entry_sz = ath12k_hal_srng_get_entrysize(ab, type);
	int max_entries = ath12k_hal_srng_get_max_entries(ab, type);
	int ret;
	bool cached = false;

	if (max_entries < 0 || entry_sz < 0)
		return -EINVAL;

	if (num_entries > max_entries)
		num_entries = max_entries;

	ring->size = (num_entries * entry_sz) + HAL_RING_BASE_ALIGN - 1;

#ifndef CONFIG_IO_COHERENCY
	if (ab->hw_params->alloc_cacheable_memory) {
		/* Allocate the reo dst rings from cacheable memory */
		if (type == HAL_REO_DST_MGMT)
			cached = true;
		else
			cached = false;
	}
#else
	cached = true;
#endif

	if (cached) {
		ring->vaddr_unaligned = kzalloc(ring->size, GFP_KERNEL);
		ring->paddr_unaligned = virt_to_phys(ring->vaddr_unaligned);
	} else {
		ring->vaddr_unaligned = dma_alloc_coherent(ab->dev, ring->size,
							   &ring->paddr_unaligned,
							   GFP_KERNEL);
	}
	if (!ring->vaddr_unaligned)
		return -ENOMEM;

	ring->vaddr = PTR_ALIGN(ring->vaddr_unaligned, HAL_RING_BASE_ALIGN);
	ring->paddr = ring->paddr_unaligned + ((unsigned long)ring->vaddr -
		      (unsigned long)ring->vaddr_unaligned);

	params.ring_base_vaddr = ring->vaddr;
	params.ring_base_paddr = ring->paddr;
	params.num_entries = num_entries;

	ath12k_mgmt_srng_msi_setup(ab, &params, type, grp_id);

	switch (type) {
	case HAL_REO_DST_MGMT:
		params.intr_batch_cntr_thres_entries = HAL_SRNG_INT_BATCH_THRESHOLD_RX;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_REO_EXCEPTION_MGMT:
		params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_OTHER;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_OTHER;
		break;
	default:
		ath12k_warn(ab, "Not a valid ring type for MGMT :%d", type);
		return -EINVAL;
	}

	if (cached) {
		params.flags |= HAL_SRNG_FLAGS_CACHED;
		ring->cached = 1;
	}

	ret = ath12k_hal_srng_setup_idx(ab, type, ring_num, mac_id, &params, 0);
	if (ret < 0) {
		ath12k_warn(ab, "Failed to setup mgmt srng: %d ring_id %d",
			    ret, ring_num);
		return ret;
	}

	ring->ring_id = ret;

	return 0;
}
EXPORT_SYMBOL(ath12k_mgmt_srng_setup);

void ath12k_mgmt_srng_cleanup(struct ath12k_base *ab, struct mgmt_srng *ring)
{
	if (!ring->vaddr_unaligned)
		return;

	if (ring->cached)
		kfree(ring->vaddr_unaligned);
	else
		ath12k_hal_dma_free_coherent(ab->dev, ring->size,
					     ring->vaddr_unaligned,
					     ring->paddr_unaligned);

	ring->vaddr_unaligned = NULL;
}
EXPORT_SYMBOL(ath12k_mgmt_srng_cleanup);
