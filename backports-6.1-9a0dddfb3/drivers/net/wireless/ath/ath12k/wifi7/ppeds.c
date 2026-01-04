// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/bitfield.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/cacheflush.h>
#include "../hif.h"
#include "../hal.h"
#include "../debug.h"
#include "hw.h"
#include "dp.h"
#include "dp_tx.h"
#include "../ppe.h"
#include "ppeds.h"
#include "../ppe_public.h"

/* PPE-DS release interrupt */
irqreturn_t ath12k_dp_ppeds_handle_tx_comp(int irq, void *ctxt)
{
	struct ath12k_base *ab = (struct ath12k_base *)ctxt;
	struct ath12k_ppeds_napi *napi_ctxt = &ab->dp->ppe.ppeds_napi_ctxt;

	ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_PPE_WBM2SW_REL);
	napi_schedule(&napi_ctxt->napi);
	return IRQ_HANDLED;
}

irqreturn_t ath12k_wifi7_ds_ppe2tcl_irq_handler(int irq, void *ctxt)
{
	struct ath12k_base *ab = (struct ath12k_base *)ctxt;

	if (ab->dp->ppe.nss_plugin_ops)
		ab->dp->ppe.nss_plugin_ops->ds_inst_ppe2tcl_intr(ab->dp->ppe.ds_node_id);

	return IRQ_HANDLED;
}

irqreturn_t ath12k_wifi7_ds_reo2ppe_irq_handler(int irq, void *ctxt)
{
	struct ath12k_base *ab = (struct ath12k_base *)ctxt;

	if (ab->dp->ppe.nss_plugin_ops)
		ab->dp->ppe.nss_plugin_ops->ds_inst_reo2ppe_intr(ab->dp->ppe.ds_node_id);
	return IRQ_HANDLED;
}

void ath12k_ppeds_set_tcl_prod_idx_v2(int ds_node_id, u16 tcl_prod_idx)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;

	srng = &ab->hal.srng_list[dp->ppe.ppe2tcl_ring.ring_id];
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.tcl_prod_cnt++;

	srng->u.src_ring.hp = tcl_prod_idx * srng->entry_size;
	ath12k_hal_srng_access_end(ab, srng);
}
EXPORT_SYMBOL(ath12k_ppeds_set_tcl_prod_idx_v2);

u16 ath12k_ppeds_get_tcl_cons_idx_v2(int ds_node_id)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;
	u32 tp;

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.tcl_cons_cnt++;

	srng = &ab->hal.srng_list[dp->ppe.ppe2tcl_ring.ring_id];
	tp = READ_ONCE(*(u32 *)srng->u.src_ring.tp_addr);
	dma_rmb();

	return tp / srng->entry_size;
}
EXPORT_SYMBOL(ath12k_ppeds_get_tcl_cons_idx_v2);

void ath12k_ppeds_set_reo_cons_idx_v2(int ds_node_id, u16 reo_cons_idx)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;

	srng = &ab->hal.srng_list[dp->ppe.reo2ppe_ring.ring_id];
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.reo_cons_cnt++;

	srng->u.src_ring.hp = reo_cons_idx * srng->entry_size;
	ath12k_hal_srng_access_end(ab, srng);
}
EXPORT_SYMBOL(ath12k_ppeds_set_reo_cons_idx_v2);

u16 ath12k_ppeds_get_reo_prod_idx_v2(int ds_node_id)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;
	u32 hp;

	srng = &ab->hal.srng_list[dp->ppe.reo2ppe_ring.ring_id];
	hp = READ_ONCE(*(u32 *)srng->u.dst_ring.hp_addr);
	dma_rmb();
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.reo_prod_cnt++;
	return hp / srng->entry_size;
}
EXPORT_SYMBOL(ath12k_ppeds_get_reo_prod_idx_v2);

/* enable/disable PPE2TCL irq */
void ath12k_ppeds_enable_srng_intr_v2(int ds_node_id, bool enable)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];

	if (enable) {
		if (!ab->stats_disable)
			ab->dp->ppe.ppeds_stats.enable_intr_cnt++;

		ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_PPE2TCL);
	} else {
		if (!ab->stats_disable)
			ab->dp->ppe.ppeds_stats.disable_intr_cnt++;

		ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_PPE2TCL);
	}
}
EXPORT_SYMBOL(ath12k_ppeds_enable_srng_intr_v2);

bool ath12k_ppeds_free_rx_desc_v2(struct ppe_ds_wlan_rxdesc_elem *arr,
				  struct ath12k_base *ab, int index,
				  u16 *idx_of_ab)
{
	struct ath12k_rx_desc_info *rx_desc;
	struct sk_buff *skb;

	rx_desc = (struct ath12k_rx_desc_info *)arr[idx_of_ab[index]].cookie;

	if (rx_desc->device_id != ab->device_id)
		return false;

	skb = rx_desc->skb;
	rx_desc->skb = NULL;

	spin_lock_bh(&ab->dp->rx_desc_lock);
	list_add_tail(&rx_desc->list, &ab->dp->rx_desc_free_list);
	spin_unlock_bh(&ab->dp->rx_desc_lock);

	if (!skb) {
		ath12k_err(ab, "ppeds rx desc with no skb when freeing\n");
		return false;
	}

	/* When recycled_for_ds is set, packet is used by DS rings and never has
	 * touched by host. So, buffer unmap can be skipped.
	 */
	if (!skb->recycled_for_ds) {
		ath12k_core_dmac_inv_range_no_dsb(skb->data, skb->data + (skb->len +
						  skb_tailroom(skb)));
		ath12k_core_dma_unmap_single_attrs(ab->dev, ATH12K_SKB_RXCB(skb)->paddr,
						   skb->len + skb_tailroom(skb),
						   DMA_FROM_DEVICE,
						   DMA_ATTR_SKIP_CPU_SYNC);
	}

	skb->recycled_for_ds = 0;
	skb->fast_recycled = 0;
	dev_kfree_skb_any(skb);
	return true;
}
EXPORT_SYMBOL(ath12k_ppeds_free_rx_desc_v2);

int ath12k_wifi7_dp_rx_bufs_replenish_ppeds(struct ath12k_base *ab,
					    int req_entries, u16 *idx_of_ab,
					    struct ppe_ds_wlan_rxdesc_elem *arr)
{
	struct dp_rxdma_ring *rx_ring = &ab->dp->rx_refill_buf_ring;
	struct hal_srng *rxdma_srng;
	struct ath12k_buffer_addr *rxdma_desc;
	u32 cookie;
	dma_addr_t paddr;
	struct ath12k_rx_desc_info *rx_desc;
	int count = 0, num_remain, i;
	u8 mgr = ab->hal.hal_params->rx_buf_rbm;

	rxdma_srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];

	spin_lock_bh(&rxdma_srng->lock);
	ath12k_hal_srng_access_begin(ab, rxdma_srng);

	num_remain = req_entries;
	for (i = 0 ; i < req_entries; i++) {
		if ((i + 1) < (req_entries - 1))
			prefetch((struct ath12k_rx_desc_info *)
						arr[idx_of_ab[i + 1]].cookie);

		rx_desc = (struct ath12k_rx_desc_info *)arr[idx_of_ab[i]].cookie;
		if (!rx_desc)
			break;

		if (!rx_desc->skb) {
			ath12k_err(ab, "ppeds rx desc with no skb when reusing!\n");
			break;
		}

		cookie = rx_desc->cookie;
		paddr = rx_desc->paddr;

		rxdma_desc = ath12k_hal_srng_src_get_next_entry(ab, rxdma_srng);
		if (!rxdma_desc)
			break;

		ath12k_hal_rx_buf_addr_info_set(rxdma_desc, paddr, cookie, mgr);
		num_remain--;
	}

	ath12k_hal_srng_access_end(ab, rxdma_srng);
	spin_unlock_bh(&rxdma_srng->lock);

	/* move any remaining descriptors to free list */
	for (; i < req_entries; i++)
		count += ath12k_ppeds_free_rx_desc_v2(arr, ab, i, idx_of_ab);

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.num_rx_desc_freed += count;

	return 0;
}

void ath12k_ppeds_release_rx_desc_v2(int ds_node_id,
				     struct ppe_ds_wlan_rxdesc_elem *arr,
				     u16 count)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	struct ath12k_base *src_ab = NULL;
	struct ath12k_hw_group *ag = ab->ag;
	u32 rx_bufs_reaped[ATH12K_MAX_SOCS] = {0};
	struct ath12k_rx_desc_info *rx_desc;
	int device_id;
	u32 i = 0, new_size, num_free_desc;
	u16 *idx_of_ab;
	u16 *tmp;

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.release_rx_desc_cnt += count;

	if (unlikely(count > ab->dp->ppe.ppeds_rx_num_elem)) {
		new_size = sizeof(u16) * count;
		for (device_id = 0; device_id < ag->num_devices; device_id++) {
			tmp = krealloc(ab->dp->ppe.ppeds_rx_idx[device_id], new_size,
				       GFP_ATOMIC);
			if (!tmp) {
				ath12k_err(ab, "ppeds: rx desc realloc failed for size %u\n",
					   count);
				goto err_h_alloc_failure;
			}

			ab->dp->ppe.ppeds_rx_idx[device_id] = tmp;
		}

		ab->dp->ppe.ppeds_rx_num_elem = count;
		ab->dp->ppe.ppeds_stats.num_rx_desc_realloc += device_id;
	}

	for (i = 0; i < count; i++) {
		if ((i + 1) < (count - 1))
			prefetch((struct ath12k_rx_desc_info *)arr[i + 1].cookie);

		rx_desc = (struct ath12k_rx_desc_info *)arr[i].cookie;
		if (!rx_desc) {
			ath12k_err(ab, "error: rx desc is null\n");
			continue;
		}

		device_id = rx_desc->device_id;
		/* Maintain indexes of arr per ab separately, which can accessed easily
		 * during per ab's rxdma srng replenish
		 */
		ab->dp->ppe.ppeds_rx_idx[device_id][rx_bufs_reaped[device_id]] = i;
		rx_bufs_reaped[device_id]++;
	}

	for (device_id = 0; device_id < ag->num_devices; device_id++) {
		if (!rx_bufs_reaped[device_id])
			continue;

		src_ab = ag->ab[device_id];
		ath12k_wifi7_dp_rx_bufs_replenish_ppeds(src_ab, rx_bufs_reaped[device_id],
							&ppe->ppeds_rx_idx[device_id][0],
							arr);
	}

	return;

err_h_alloc_failure:
	for (device_id = 0; device_id < ag->num_devices; device_id++) {
		src_ab = ag->ab[device_id];
		idx_of_ab = &src_ab->dp->ppe.ppeds_rx_idx[device_id][0];
		num_free_desc = 0;
		for (i = 0; i < count; i++)
			num_free_desc +=
				ath12k_ppeds_free_rx_desc_v2(arr, src_ab, i, idx_of_ab);
		if (!src_ab->stats_disable)
			src_ab->dp->ppe.ppeds_stats.num_rx_desc_freed += num_free_desc;
	}
}
EXPORT_SYMBOL(ath12k_ppeds_release_rx_desc_v2);

void ath12k_ppeds_release_tx_desc_single_v2(int ds_node_id, u32 cookie)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.release_tx_single_cnt++;
}
EXPORT_SYMBOL(ath12k_ppeds_release_tx_desc_single_v2);

u32 ath12k_ppeds_get_batched_tx_desc_v2(int ds_node_id,
					struct ppe_ds_wlan_txdesc_elem *arr,
					u32 num_buff_req,
					u32 buff_size,
					u32 headroom)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	int i = 0;
	int allocated = 0;
	struct sk_buff *skb = NULL;
	int flags = GFP_ATOMIC;
	dma_addr_t paddr;
	struct ath12k_ppeds_tx_desc_info *desc = NULL, *tmp;
	struct ath12k_ppeds_stats *ppeds_stats = &ab->dp->ppe.ppeds_stats;

#if LINUX_VERSION_IS_GEQ(4, 4, 0)
	flags = flags & ~__GFP_KSWAPD_RECLAIM;
#endif
	spin_lock_bh(&dp->ppe.ppeds_tx_desc_lock);

	list_for_each_entry_safe(desc, tmp, &dp->ppe.ppeds_tx_desc_reuse_list, list) {
		if (!num_buff_req)
			break;

		list_del(&desc->list);
		desc->in_use = true;

		dp->ppe.ppeds_tx_desc_reuse_list_len--;

		prefetch(list_next_entry(desc, list));
		num_buff_req--;

		arr[i].opaque_lo = desc->desc_id;
		arr[i].opaque_hi = 0;
		arr[i].buff_addr = desc->paddr;
		allocated++;
		i++;
	}

	if (!num_buff_req) {
		spin_unlock_bh(&dp->ppe.ppeds_tx_desc_lock);
		goto update_stats_and_ret;
	}

	list_for_each_entry_safe(desc, tmp, &dp->ppe.ppeds_tx_desc_free_list, list) {
		if (!num_buff_req)
			break;

		list_del(&desc->list);
		desc->in_use = true;

		if (likely(!desc->skb)) {
		       /* In skb recycler, if recyler module allocates the buffers
			* already used by DS module to DS, then memzero, shinfo
			* reset can be avoided, since the DS packets were not
			* processed by SW
			*/
			skb = __netdev_alloc_skb_no_skb_reset(NULL, buff_size, flags);
			if (unlikely(!skb)) {
				desc->in_use = false;
				list_add_tail(&desc->list,
					      &dp->ppe.ppeds_tx_desc_free_list);
				break;
			}

			skb_reserve(skb, headroom);
			if (!skb->recycled_for_ds) {
				ath12k_core_dmac_inv_range_no_dsb((void *)skb->data,
								  ((void *)skb->data +
								  buff_size - headroom));
				skb->recycled_for_ds = 1;
			}

			paddr = virt_to_phys(skb->data);

			desc->skb = skb;
			desc->paddr = paddr;
			desc->in_use = true;
		} else {
			pr_warn("skb found in ppeds_tx_desc_free_list");
		}

		prefetch(list_next_entry(desc, list));
		num_buff_req--;

		arr[i].opaque_lo = desc->desc_id;
		arr[i].opaque_hi = 0;
		arr[i].buff_addr = desc->paddr;
		allocated++;
		i++;
	}

	spin_unlock_bh(&dp->ppe.ppeds_tx_desc_lock);

	dsb(st);

update_stats_and_ret:
	if (unlikely(num_buff_req))
		ppeds_stats->tx_desc_alloc_fails += num_buff_req;

	if (unlikely(!ab->stats_disable)) {
		ppeds_stats->get_tx_desc_cnt++;
		ppeds_stats->tx_desc_allocated += allocated;
	}

	return allocated;
}
EXPORT_SYMBOL(ath12k_ppeds_get_batched_tx_desc_v2);

void ath12k_ppeds_notify_napi_done_v2(int ds_node_id)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;

	clear_bit(ATH12K_DP_PPEDS_NAPI_DONE_BIT, &dp->service_rings_running);

	if (ab->dp_umac_reset.umac_pre_reset_in_prog)
		ath12k_umac_reset_notify_pre_reset_done(ab);
}
EXPORT_SYMBOL(ath12k_ppeds_notify_napi_done_v2);

struct ppe_ds_wlan_ops_v2 ppeds_wlanops_v2 = {
	.get_tx_desc_many = ath12k_ppeds_get_batched_tx_desc_v2,
	.release_tx_desc_single = ath12k_ppeds_release_tx_desc_single_v2,
	.enable_tx_consume_intr = ath12k_ppeds_enable_srng_intr_v2,
	.set_tcl_prod_idx  = ath12k_ppeds_set_tcl_prod_idx_v2,
	.set_reo_cons_idx = ath12k_ppeds_set_reo_cons_idx_v2,
	.get_tcl_cons_idx = ath12k_ppeds_get_tcl_cons_idx_v2,
	.get_reo_prod_idx = ath12k_ppeds_get_reo_prod_idx_v2,
	.release_rx_desc = ath12k_ppeds_release_rx_desc_v2,
	.notify_napi_done = ath12k_ppeds_notify_napi_done_v2,
};

struct ath12k_ppeds_arch_ops ath12k_wifi7_arch_ppeds_ops  = {
	.ppe2tcl_irq_handler = ath12k_wifi7_ds_ppe2tcl_irq_handler,
	.reo2ppe_irq_handler = ath12k_wifi7_ds_reo2ppe_irq_handler,
	.ppe2tcl_tx_compln = ath12k_dp_ppeds_handle_tx_comp,
};
