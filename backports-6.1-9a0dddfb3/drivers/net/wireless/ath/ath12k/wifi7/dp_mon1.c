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
#include "hal_mon.h"

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
	.mon_rx_srng_process = ath12k_wifi7_dp_mon_rx_quad_ring_process,
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
		cookie = u32_encode_bits(buf_id, DP_MON_RXDMA_BUF_COOKIE_BUF_ID);

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

static struct sk_buff
*ath12k_wifi7_dp_mon_rx_alloc_status_buf(struct ath12k_base *ab,
					 struct dp_rxdma_mon_ring *rx_ring,
					 int *buf_id)
{
	struct sk_buff *skb;
	dma_addr_t paddr;

	skb = dev_alloc_skb(RX_MON_STATUS_BUF_SIZE);

	if (!skb)
		goto fail_alloc_skb;

	if (!IS_ALIGNED((unsigned long)skb->data,
			RX_MON_STATUS_BUF_ALIGN)) {
		skb_pull(skb, PTR_ALIGN(skb->data, RX_MON_STATUS_BUF_ALIGN) -
			 skb->data);
	}

	paddr = dma_map_single(ab->dev, skb->data,
			       skb->len + skb_tailroom(skb),
			       DMA_FROM_DEVICE);
	if (unlikely(dma_mapping_error(ab->dev, paddr)))
		goto fail_free_skb;

	spin_lock_bh(&rx_ring->idr_lock);
	*buf_id = idr_alloc(&rx_ring->bufs_idr, skb, 0,
			    rx_ring->bufs_max, GFP_ATOMIC);
	spin_unlock_bh(&rx_ring->idr_lock);
	if (*buf_id < 0)
		goto fail_dma_unmap;

	ATH12K_SKB_RXCB(skb)->paddr = paddr;
	return skb;

fail_dma_unmap:
	dma_unmap_single(ab->dev, paddr, skb->len + skb_tailroom(skb),
			 DMA_FROM_DEVICE);
fail_free_skb:
	dev_kfree_skb_any(skb);
fail_alloc_skb:
	return NULL;
}

static enum dp_mon_status_buf_state
ath12k_wifi7_dp_mon_rx_buf_done(struct ath12k_base *ab, struct hal_srng *srng,
				struct dp_rxdma_mon_ring *rx_ring)
{
	struct ath12k_skb_rxcb *rxcb;
	struct hal_tlv_64_hdr *tlv;
	struct sk_buff *skb;
	struct ath12k_buffer_addr *status_desc;
	dma_addr_t paddr;
	u32 cookie;
	int buf_id;
	u8 rbm;

	status_desc = ath12k_hal_srng_src_next_peek(ab, srng);
	if (!status_desc)
		return DP_MON_STATUS_NO_DMA;

	ath12k_hal_rx_buf_addr_info_get(status_desc, &paddr, &cookie, &rbm);

	buf_id = u32_get_bits(cookie, DP_MON_RXDMA_BUF_COOKIE_BUF_ID);

	spin_lock_bh(&rx_ring->idr_lock);
	skb = idr_find(&rx_ring->bufs_idr, buf_id);
	spin_unlock_bh(&rx_ring->idr_lock);

	if (!skb)
		return DP_MON_STATUS_NO_DMA;

	rxcb = ATH12K_SKB_RXCB(skb);
	dma_sync_single_for_cpu(ab->dev, rxcb->paddr,
				skb->len + skb_tailroom(skb),
				DMA_FROM_DEVICE);

	tlv = (struct hal_tlv_64_hdr *)skb->data;
	if (le64_get_bits(tlv->tl, HAL_TLV_HDR_TAG) != HAL_RX_STATUS_BUFFER_DONE)
		return DP_MON_STATUS_NO_DMA;

	return DP_MON_STATUS_REPLINISH;
}

static int ath12k_wifi7_dp_mon_rx_reap_status_ring(struct ath12k_base *ab, int mac_id,
						   int *budget,
						   struct sk_buff_head *skb_list)
{
	const struct ath12k_hw_hal_params *hal_params;
	int buf_id, srng_id, num_buffs_reaped = 0;
	enum dp_mon_status_buf_state reap_status;
	struct dp_rxdma_mon_ring *rx_ring;
	struct ath12k_mon_data *pmon;
	struct ath12k_skb_rxcb *rxcb;
	struct hal_tlv_64_hdr *tlv;
	struct ath12k_buffer_addr *rx_mon_status_desc;
	struct hal_srng *srng;
	struct ath12k_dp *dp;
	struct ath12k_dp_mon *dp_mon;
	struct sk_buff *skb;
	struct ath12k *ar;
	dma_addr_t paddr;
	u32 cookie;
	u8 rbm;

	ar = ab->pdevs[ath12k_hw_mac_id_to_pdev_id(ab->hw_params, mac_id)].ar;
	dp = ab->dp;
	dp_mon = dp->dp_mon;
	pmon = &ar->dp.dp_mon_pdev->mon_data;
	srng_id = ath12k_hw_mac_id_to_srng_id(ab->hw_params, mac_id);
	rx_ring = &dp_mon->rx_mon_status_refill_ring[srng_id];

	srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	while (*budget) {
		*budget -= 1;
		rx_mon_status_desc = ath12k_hal_srng_src_peek(ab, srng);
		if (!rx_mon_status_desc) {
			pmon->buf_state = DP_MON_STATUS_REPLINISH;
			break;
		}
		ath12k_hal_rx_buf_addr_info_get(rx_mon_status_desc, &paddr,
						&cookie, &rbm);
		if (paddr) {
			buf_id = u32_get_bits(cookie, DP_MON_RXDMA_BUF_COOKIE_BUF_ID);

			spin_lock_bh(&rx_ring->idr_lock);
			skb = idr_find(&rx_ring->bufs_idr, buf_id);
			spin_unlock_bh(&rx_ring->idr_lock);

			if (!skb) {
				ath12k_warn(ab, "rx monitor status with invalid buf_id %d\n",
					    buf_id);
				pmon->buf_state = DP_MON_STATUS_REPLINISH;
				goto move_next;
			}

			rxcb = ATH12K_SKB_RXCB(skb);

			dma_sync_single_for_cpu(ab->dev, rxcb->paddr,
						skb->len + skb_tailroom(skb),
						DMA_FROM_DEVICE);

			tlv = (struct hal_tlv_64_hdr *)skb->data;
			if (le64_get_bits(tlv->tl, HAL_TLV_HDR_TAG) !=
					HAL_RX_STATUS_BUFFER_DONE) {
				pmon->buf_state = DP_MON_STATUS_NO_DMA;
				ath12k_warn(ab,
					    "mon status DONE not set %llx, buf_id %d\n",
					    le64_get_bits(tlv->tl, HAL_TLV_HDR_TAG),
					    buf_id);
				/* RxDMA status done bit might not be set even
				 * though tp is moved by HW.
				 */

				/* If done status is missing:
				 * 1. As per MAC team's suggestion,
				 *    when HP + 1 entry is peeked and if DMA
				 *    is not done and if HP + 2 entry's DMA done
				 *    is set. skip HP + 1 entry and
				 *    start processing in next interrupt.
				 * 2. If HP + 2 entry's DMA done is not set,
				 *    poll onto HP + 1 entry DMA done to be set.
				 *    Check status for same buffer for next time
				 *    dp_rx_mon_status_srng_process
				 */
				reap_status = ath12k_wifi7_dp_mon_rx_buf_done(ab, srng,
									       rx_ring);
				if (reap_status == DP_MON_STATUS_NO_DMA)
					continue;

				spin_lock_bh(&rx_ring->idr_lock);
				idr_remove(&rx_ring->bufs_idr, buf_id);
				spin_unlock_bh(&rx_ring->idr_lock);

				dma_unmap_single(ab->dev, rxcb->paddr,
						 skb->len + skb_tailroom(skb),
						 DMA_FROM_DEVICE);

				dev_kfree_skb_any(skb);
				pmon->buf_state = DP_MON_STATUS_REPLINISH;
				goto move_next;
			}

			spin_lock_bh(&rx_ring->idr_lock);
			idr_remove(&rx_ring->bufs_idr, buf_id);
			spin_unlock_bh(&rx_ring->idr_lock);

			dma_unmap_single(ab->dev, rxcb->paddr,
					 skb->len + skb_tailroom(skb),
					 DMA_FROM_DEVICE);

			if (ath12k_dp_pkt_set_pktlen(skb, RX_MON_STATUS_BUF_SIZE)) {
				dev_kfree_skb_any(skb);
				goto move_next;
			}
			__skb_queue_tail(skb_list, skb);
		} else {
			pmon->buf_state = DP_MON_STATUS_REPLINISH;
		}
move_next:
		skb = ath12k_wifi7_dp_mon_rx_alloc_status_buf(ab, rx_ring,
							       &buf_id);

		if (!skb) {
			ath12k_warn(ab, "failed to alloc buffer for status ring\n");
			hal_params = ab->hal.hal_params;
			ath12k_hal_rx_buf_addr_info_set(rx_mon_status_desc, 0,
							0,
							hal_params->rx_buf_rbm);
			num_buffs_reaped++;
			break;
		}
		rxcb = ATH12K_SKB_RXCB(skb);

		cookie = u32_encode_bits(mac_id, DP_MON_RXDMA_BUF_COOKIE_PDEV_ID) |
			 u32_encode_bits(buf_id, DP_MON_RXDMA_BUF_COOKIE_BUF_ID);

		ath12k_hal_rx_buf_addr_info_set(rx_mon_status_desc, rxcb->paddr,
						cookie,
						ab->hal.hal_params->rx_buf_rbm);
		ath12k_hal_srng_src_get_next_entry(ab, srng);
		num_buffs_reaped++;
	}
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return num_buffs_reaped;
}

static enum hal_rx_mon_status
ath12k_wifi7_dp_mon_rx_parse_dest(struct ath12k_pdev_dp *dp_pdev,
				  struct sk_buff *skb)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_mon_data *pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;
	struct hal_tlv_64_hdr *tlv;
	struct ath12k_skb_rxcb *rxcb;
	enum hal_rx_mon_status hal_status;
	u16 tlv_tag, tlv_len;
	u8 *ptr = skb->data;

	do {
		tlv = (struct hal_tlv_64_hdr *)ptr;
		tlv_tag = le64_get_bits(tlv->tl, HAL_TLV_64_HDR_TAG);

		/* The actual length of PPDU_END is the combined length of many PHY
		 * TLVs that follow. Skip the TLV header and
		 * rx_rxpcu_classification_overview that follows the header to get to
		 * next TLV.
		 */

		if (tlv_tag == HAL_RX_PPDU_END)
			tlv_len = sizeof(struct hal_rx_rxpcu_classification_overview);
		else
			tlv_len = le64_get_bits(tlv->tl, HAL_TLV_64_HDR_LEN);

		hal_status = ath12k_hal_mon_rx_parse_status(dp_pdev->dp->hal,
							    &pmon->mon_ppdu_info,
							    tlv);
		ptr += sizeof(*tlv) + tlv_len;
		ptr = PTR_ALIGN(ptr, HAL_TLV_64_ALIGN);

		if ((ptr - skb->data) > skb->len)
			break;

	} while ((hal_status == HAL_RX_MON_STATUS_PPDU_NOT_DONE) ||
		 (hal_status == HAL_RX_MON_STATUS_BUF_ADDR) ||
		 (hal_status == HAL_RX_MON_STATUS_MPDU_START) ||
		 (hal_status == HAL_RX_MON_STATUS_MPDU_END) ||
		 (hal_status == HAL_RX_MON_STATUS_MSDU_END));

	rxcb = ATH12K_SKB_RXCB(skb);
	if (rxcb->is_end_of_ppdu)
		hal_status = HAL_RX_MON_STATUS_PPDU_DONE;

	return hal_status;
}

int ath12k_wifi7_dp_mon_rx_quad_ring_process(struct ath12k_pdev_dp *pdev_dp, int mac_id,
					     struct napi_struct *napi, int *budget)
{
	struct ath12k *ar = pdev_dp->ar;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = ar->dp.dp_mon_pdev;
	struct ath12k_mon_data *pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;
	struct ath12k_pdev_mon_stats *rx_mon_stats = &pmon->rx_mon_stats;
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	enum hal_rx_mon_status hal_status;
	struct sk_buff_head skb_list;
	int num_buffs_reaped;
	struct sk_buff *skb;

	__skb_queue_head_init(&skb_list);

	num_buffs_reaped = ath12k_wifi7_dp_mon_rx_reap_status_ring(ar->ab, mac_id,
								   budget, &skb_list);
	if (!num_buffs_reaped)
		goto exit;

	while ((skb = __skb_dequeue(&skb_list))) {
		memset(ppdu_info, 0, sizeof(*ppdu_info));
		ppdu_info->peer_id = HAL_INVALID_PEERID;

		hal_status = ath12k_wifi7_dp_mon_rx_parse_dest(&ar->dp, skb);

		if (ar->monitor_started &&
		    pmon->mon_ppdu_status == DP_PPDU_STATUS_START &&
		    hal_status == HAL_TLV_STATUS_PPDU_DONE) {
			rx_mon_stats->status_ppdu_done++;
			pmon->mon_ppdu_status = DP_PPDU_STATUS_DONE;
			ath12k_dp_rx_mon_dest_process(ar, mac_id, *budget, napi);
			pmon->mon_ppdu_status = DP_PPDU_STATUS_START;
		}

		dev_kfree_skb_any(skb);
	}
exit:
	return num_buffs_reaped;
}
