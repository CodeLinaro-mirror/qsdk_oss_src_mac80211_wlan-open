// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../debug.h"
#include "../mgmt_rx.h"
#include "../hif.h"
#include "mgmt_rx.h"
#include "hal.h"
#include "hal_qcn9625.h"

static void ath12k_wifi8_mgmt_rx_ring_free(struct ath12k_base *ab);

void ath12k_wifi8_mgmt_service_srng(struct ath12k_base *ab,
				    struct ath12k_mgmt_irq_grp *irq_grp)
{}

#if LINUX_VERSION_IS_GEQ(6, 13, 0)
void ath12k_wifi8_mgmt_workqueue(struct work_struct *w)
{
	struct ath12k_mgmt_irq_grp *irq_grp = from_work(irq_grp, work, intr_wq);

	ath12k_wifi8_mgmt_service_srng(irq_grp->ab, irq_grp);

	ath12k_mgmt_irq_grp_enable(irq_grp);
}
#else
void ath12k_wifi8_mgmt_tasklet(struct tasklet_struct *t)
{
	struct ath12k_mgmt_irq_grp *irq_grp = from_tasklet(irq_grp, t, intr_tq);

	ath12k_wifi8_mgmt_service_srng(irq_grp->ab, irq_grp);

	ath12k_mgmt_irq_grp_enable(irq_grp);
}
#endif

static void ath12k_wifi8_mgmt_rx_replenish_buffs(struct ath12k_mgmt *mgmt,
						 struct mgmt_srng *rx_refill_ring,
						 struct list_head *desc_used_list)
{
	struct ath12k_base *ab = mgmt->ab;
	struct ath12k_buffer_addr *desc;
	struct sk_buff *skb;
	dma_addr_t paddr;
	struct ath12k_rx_desc_info *rx_desc, *tmp_rx_desc;
	struct hal_srng *srng;
	u8 manager = mgmt->hal->hal_params->rx_buf_rbm;
	int allocated_entries = 0;

	list_for_each_entry_safe(rx_desc, tmp_rx_desc, desc_used_list, list) {
		skb = dev_alloc_skb(MGMT_RX_BUFFER_SIZE +
				    MGMT_RX_BUFFER_ALIGN_SIZE);
		if (!skb)
			break;

		if (!IS_ALIGNED((unsigned long)skb->data,
				MGMT_RX_BUFFER_ALIGN_SIZE))
			skb_pull(skb,
				 PTR_ALIGN(skb->data, MGMT_RX_BUFFER_ALIGN_SIZE) -
				 skb->data);

		paddr = ath12k_core_dma_map_single(mgmt->dev, skb->data,
						   skb->len + skb_tailroom(skb),
						   DMA_FROM_DEVICE);
		if (unlikely(!paddr)) {
			dev_kfree_skb_any(skb);
			break;
		}

		allocated_entries++;
		rx_desc->skb = skb;
		rx_desc->paddr = paddr;
		rx_desc->vaddr = skb->data;
		rx_desc->is_frag = 0;
	}

	srng = &ab->hal.srng_list[rx_refill_ring->ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (allocated_entries > 0) {
		rx_desc = list_first_entry_or_null(desc_used_list,
						   struct ath12k_rx_desc_info, list);
		if (unlikely(!rx_desc))
			goto out;

		desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
		if (unlikely(!desc))
			goto out;

		list_del(&rx_desc->list);

		allocated_entries--;
		ath12k_hal_rx_buf_addr_info_set(desc, rx_desc->paddr, rx_desc->cookie,
						manager);
	}

out:
	ath12k_hal_srng_access_end(ab, srng);

	/* add the remaining descriptors to the free_list */
	if (!list_empty(desc_used_list)) {
		struct ath12k_rx_desc_info *rx_desc, *safe;
		struct sk_buff *skb;

		list_for_each_entry_safe(rx_desc, safe, desc_used_list, list) {
			rx_desc->in_use = false;
			rx_desc->is_frag = 0;
			skb = rx_desc->skb;
			ath12k_core_dma_unmap_single(mgmt->dev, rx_desc->paddr,
						     skb->len + skb_tailroom(skb),
						     DMA_FROM_DEVICE);
			dev_kfree_skb_any(skb);
		}

		spin_lock_bh(&mgmt->rx_desc_lock);
		list_splice_tail(desc_used_list, &mgmt->rx_desc_free_list);
		spin_unlock_bh(&mgmt->rx_desc_lock);
	}

	spin_unlock_bh(&srng->lock);
}

static int ath12k_wifi8_mgmt_rx_refill_ring_setup(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	int ret;
	LIST_HEAD(used_list);

	/* WBM Idle Buffer ring */
	ret = ath12k_mgmt_srng_setup(ab, &mgmt_wifi8->wbm_idle_buf_ring,
				     HAL_WBM_BUF_MGMT, 0, 0,
				     ATH12K_MGMT_IRQ_GRP_ID_INVALID,
				     MGMT_WBM_IDLE_BUF_RING_SIZE);
	if (ret) {
		ath12k_err(ab, "Failed to setup mgmt wbm_idle_buf_ring: %d", ret);
		return ret;
	}

	/* WBM Refill ring */
	ret = ath12k_mgmt_srng_setup(ab, &mgmt_wifi8->wbm_refill_ring,
				     HAL_WBM_IDLE_BUF_MGMT, 0, 0,
				     ATH12K_MGMT_IRQ_GRP_ID_INVALID,
				     MGMT_REFILL_RING_SIZE);
	if (ret) {
		ath12k_err(ab, "Failed to setup mgmt wbm_refill_ring: %d", ret);
		return ret;
	}

	return 0;
}

static void ath12k_wifi8_mgmt_rx_refill_ring_init(struct ath12k_base *ab)
{
	LIST_HEAD(list);
	size_t req_entries;
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	struct mgmt_srng *rx_refill_ring = &mgmt_wifi8->wbm_refill_ring;

	req_entries = ath12k_mgmt_get_req_entries_from_refill_ring(ab, rx_refill_ring,
								   &list);
	if (req_entries)
		ath12k_wifi8_mgmt_rx_replenish_buffs(mgmt, rx_refill_ring, &list);
}

static int ath12k_wifi8_mgmt_rx_ring_setup(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);
	int ring_num = 0, err_ring_num = 0, ret;

	/* Mgmt Rx ring */
	ret = ath12k_mgmt_srng_setup(ab, &mgmt_wifi8->reo_dst_rx_ring,
				     HAL_REO_DST_MGMT, ring_num++, 0, 0,
				     MGMT_REO_DST_RING_SIZE);
	if (ret) {
		ath12k_err(ab, "Failed to initialize mgmt reo_dst_ring: %d", ret);
		return ret;
	}

	/* Mgmt Rx Error/Exception ring */
	ret = ath12k_mgmt_srng_setup(ab, &mgmt_wifi8->reo_dst_rx_err_ring,
				     HAL_REO_EXCEPTION_MGMT, err_ring_num++, 0,
				     0, MGMT_REO_EXCEPTION_RING_SIZE);
	if (ret) {
		ath12k_err(ab, "Failed to initialize mgmt reo_dst_err_ring: %d", ret);
		goto err_srng_cleanup;
	}

	/* Mgmt Rx Refill rings */
	ret = ath12k_wifi8_mgmt_rx_refill_ring_setup(ab);
	if (ret) {
		ath12k_err(ab, "Failed to initialize mgmt refill rings: %d", ret);
		return ret;
	}

	/* Initialize WBM ring with descriptors and buffers */
	ath12k_wifi8_mgmt_rx_refill_ring_init(ab);

	return 0;

err_srng_cleanup:
	ath12k_wifi8_mgmt_rx_ring_free(ab);
	return ret;
}

static void ath12k_wifi8_mgmt_rx_ring_free(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8 = ath12k_get_mgmt_wifi8(mgmt);

	ath12k_mgmt_srng_cleanup(ab, &mgmt_wifi8->wbm_refill_ring);
	ath12k_mgmt_srng_cleanup(ab, &mgmt_wifi8->wbm_idle_buf_ring);
	ath12k_mgmt_srng_cleanup(ab, &mgmt_wifi8->reo_dst_rx_err_ring);
	ath12k_mgmt_srng_cleanup(ab, &mgmt_wifi8->reo_dst_rx_ring);
}

int ath12k_wifi8_mgmt_op_device_init(struct ath12k_mgmt *mgmt)
{
	struct ath12k_base *ab = mgmt->ab;
	int ret;

	ret = ath12k_mgmt_rx_desc_init(ab);
	if (ret) {
		ath12k_warn(ab, "Failed to initialize mgmt rx desc: %d", ret);
		return ret;
	}

	ret = ath12k_wifi8_mgmt_rx_ring_setup(ab);
	if (ret) {
		ath12k_warn(ab, "Failed to setup mgmt rx REO rings: %d", ret);
		goto fail_rx_desc_cleanup;
	}

	ath12k_mgmt_irq_grp_setup(mgmt);
	ret = ath12k_hif_mgmt_irq_setup(ab, mgmt);
	if (ret) {
		ath12k_warn(ab, "Failed to configure mgmt IRQs: %d", ret);
		goto fail_srng_free;
	}

	return 0;

fail_srng_free:
	ath12k_wifi8_mgmt_rx_ring_free(ab);

fail_rx_desc_cleanup:
	ath12k_mgmt_rx_desc_cleanup(ab);

	return ret;
}

void ath12k_wifi8_mgmt_op_device_deinit(struct ath12k_mgmt *mgmt)
{
	struct ath12k_base *ab = mgmt->ab;

	ath12k_mgmt_rx_desc_cleanup(ab);
	ath12k_mgmt_irq_grp_cleanup(mgmt);
	ath12k_hif_mgmt_irq_cleanup(ab);
	ath12k_wifi8_mgmt_rx_ring_free(ab);
}

static struct ath12k_mgmt_arch_ops ath12k_wifi8_mgmt_arch_ops = {
	.mgmt_op_device_init = ath12k_wifi8_mgmt_op_device_init,
	.mgmt_op_device_deinit = ath12k_wifi8_mgmt_op_device_deinit,
};

struct ath12k_mgmt *ath12k_wifi8_mgmt_init(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt;
	struct ath12k_mgmt_wifi8 *mgmt_wifi8;
	struct ath12k_mgmt_irq_grp *irq_grp;

	mgmt = kzalloc(sizeof(*mgmt) + sizeof(*mgmt_wifi8), GFP_KERNEL);
	if (!mgmt)
		return NULL;

	irq_grp = kzalloc(sizeof(*irq_grp), GFP_KERNEL);
	if (!irq_grp) {
		kfree(mgmt);
		return NULL;
	}

	*irq_grp = (struct ath12k_mgmt_irq_grp){
		.grp_id = 0,
#if LINUX_VERSION_IS_GEQ(6, 13, 0)
		.irq_grp_handler = ath12k_wifi8_mgmt_workqueue,
#else
		.irq_grp_handler = ath12k_wifi8_mgmt_tasklet,
#endif
		.num_irq = 1,
	};

	mgmt->ab = ab;
	mgmt->dev = ab->dev;
	mgmt->hal = &ab->hal;
	mgmt->arch_ops = &ath12k_wifi8_mgmt_arch_ops;
	mgmt->hw_params = ab->hw_params;
	mgmt->irq_grp = irq_grp;
	mgmt->num_irq_grp = 1;

	return mgmt;
}

void ath12k_wifi8_mgmt_deinit(struct ath12k_mgmt *mgmt)
{
	kfree(mgmt->irq_grp);
	kfree(mgmt);
}
