// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/* This file contains the definitions related to the monitor quad ring
 * model
 */

#include "../dp_mon.h"
#include "dp_mon1.h"
#include "../debug.h"

struct ath12k_dp_arch_mon_ops ath12k_wifi7_dp_arch_mon_quad_ring_ops = {
	.rx_srng_setup = ath12k_wifi7_dp_mon_rx_srng_setup,
	.rx_srng_cleanup = ath12k_wifi7_dp_mon_rx_srng_cleanup,
	.rx_buf_setup = ath12k_wifi7_dp_mon_rx_buf_setup,
	.rx_buf_free = ath12k_wifi7_dp_mon_rx_buf_free,
	.rx_htt_srng_setup = ath12k_wifi7_dp_mon_rx_htt_srng_setup,
	.mon_pdev_alloc = ath12k_dp_mon_pdev_alloc,
	.mon_pdev_free = ath12k_dp_mon_pdev_free,
	.mon_pdev_rx_srng_setup = NULL,
	.mon_pdev_rx_srng_cleanup = NULL,
	.mon_pdev_rx_htt_srng_setup = NULL,
	.mon_pdev_rx_attach = ath12k_dp_mon_pdev_rx_attach,
	.mon_pdev_rx_mpdu_list_init = NULL,
};

int ath12k_wifi7_dp_mon_rx_srng_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_srng *srng;
	int i, ret;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		idr_init(&dp_mon->rx_mon_status_refill_ring[i].bufs_idr);
		spin_lock_init(&dp_mon->rx_mon_status_refill_ring[i].idr_lock);
	}

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		srng = &dp_mon->rx_mon_status_refill_ring[i].refill_buf_ring;
		ret = ath12k_dp_srng_setup(ab, srng,
					   HAL_RXDMA_MONITOR_STATUS, 0, i,
					   DP_RXDMA_MON_STATUS_RING_SIZE);
		if (ret) {
			ath12k_warn(dp, "failed to setup mon status ring %d\n", i);
			return ret;
		}
	}

	return 0;
}

void ath12k_wifi7_dp_mon_rx_srng_cleanup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_srng *srng;
	int i;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		srng = &dp_mon->rx_mon_status_refill_ring[i].refill_buf_ring;
		ath12k_dp_srng_cleanup(ab, srng);
	}
}

static int
ath12k_wifi7_dp_mon_rx_status_bufs_replenish(struct ath12k_base *ab,
					      struct dp_rxdma_mon_ring *rx_ring,
					      int req_entries)
{
	enum hal_rx_buf_return_buf_manager mgr =
		ab->hal.hal_params->rx_buf_rbm;
	int num_free, num_remain, buf_id;
	void *desc;
	struct hal_srng *srng;
	struct sk_buff *skb;
	dma_addr_t paddr;
	u32 cookie;

	req_entries = min(req_entries, rx_ring->bufs_max);

	srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	num_free = ath12k_hal_srng_src_num_free(ab, srng, true);
	if (!req_entries && (num_free > (rx_ring->bufs_max * 3) / 4))
		req_entries = num_free;

	req_entries = min(num_free, req_entries);
	num_remain = req_entries;

	while (num_remain > 0) {
		skb = dev_alloc_skb(RX_MON_STATUS_BUF_SIZE);
		if (!skb)
			break;

		if (!IS_ALIGNED((unsigned long)skb->data,
				RX_MON_STATUS_BUF_ALIGN)) {
			skb_pull(skb,
				 PTR_ALIGN(skb->data, RX_MON_STATUS_BUF_ALIGN) -
				 skb->data);
		}

		paddr = dma_map_single(ab->dev, skb->data,
				       skb->len + skb_tailroom(skb),
				       DMA_FROM_DEVICE);
		if (dma_mapping_error(ab->dev, paddr))
			goto fail_free_skb;

		spin_lock_bh(&rx_ring->idr_lock);
		buf_id = idr_alloc(&rx_ring->bufs_idr, skb, 0,
				   rx_ring->bufs_max * 3, GFP_ATOMIC);
		spin_unlock_bh(&rx_ring->idr_lock);
		if (buf_id < 0)
			goto fail_dma_unmap;
		cookie = u32_encode_bits(buf_id, DP_RXDMA_BUF_COOKIE_BUF_ID);

		desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
		if (!desc)
			goto fail_buf_unassign;

		ATH12K_SKB_RXCB(skb)->paddr = paddr;

		num_remain--;

		ath12k_hal_rx_buf_addr_info_set(desc, paddr, cookie, mgr);
	}

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	return req_entries - num_remain;

fail_buf_unassign:
	spin_lock_bh(&rx_ring->idr_lock);
	idr_remove(&rx_ring->bufs_idr, buf_id);
	spin_unlock_bh(&rx_ring->idr_lock);
fail_dma_unmap:
	dma_unmap_single(ab->dev, paddr, skb->len + skb_tailroom(skb),
			 DMA_FROM_DEVICE);
fail_free_skb:
	dev_kfree_skb_any(skb);

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	return req_entries - num_remain;
}

int ath12k_wifi7_dp_mon_rx_buf_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_rxdma_mon_ring *rx_ring;
	int i;
	int num_entries;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		rx_ring = &dp_mon->rx_mon_status_refill_ring[i];
		num_entries = rx_ring->refill_buf_ring.size /
			ath12k_hal_srng_get_entrysize(ab, HAL_RXDMA_MONITOR_STATUS);
		rx_ring->bufs_max = num_entries;

		ath12k_wifi7_dp_mon_rx_status_bufs_replenish(ab, rx_ring, num_entries);
	}

	return 0;
}

void ath12k_wifi7_dp_mon_rx_buf_free(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_rxdma_mon_ring *rx_ring;
	int i;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		rx_ring = &dp_mon->rx_mon_status_refill_ring[i];
		ath12k_dp_rxdma_mon_buf_ring_free(ab, rx_ring);
	}
}

int ath12k_wifi7_dp_mon_rx_htt_srng_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	u32 ring_id;
	int i, ret;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		ring_id =
			dp_mon->rx_mon_status_refill_ring[i].refill_buf_ring.ring_id;

		ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id, i,
						  HAL_RXDMA_MONITOR_STATUS);
		if (ret) {
			ath12k_warn(ab,
				    "failed to configure mon_status_refill_ring%d %d\n",
				    i, ret);
			return ret;
		}
	}

	return 0;
}
