// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/* This file contains the definitions related to the monitor dual ring
 * model
 */
#include "../dp_mon.h"
#include "../debug.h"
#include "hal_qcn9274.h"
#include "hal_mon.h"
#include "../peer.h"
#include "../debugfs.h"
#include "../dp_mon_filter.h"

const struct ath12k_dp_arch_mon_ops ath12k_wifi7_dp_arch_mon_dual_ring_ops = {
	.rx_srng_setup = ath12k_dp_mon_rx_srng_setup,
	.rx_srng_cleanup = ath12k_dp_mon_rx_srng_cleanup,
	.rx_buf_setup = ath12k_dp_mon_rx_buf_setup,
	.rx_buf_free = ath12k_dp_mon_rx_buf_free,
	.rx_htt_srng_setup = ath12k_dp_mon_rx_htt_srng_setup,
	.mon_pdev_alloc = ath12k_dp_mon_pdev_alloc,
	.mon_pdev_free = ath12k_dp_mon_pdev_free,
	.mon_pdev_rx_srng_setup = ath12k_dp_mon_pdev_rx_srng_setup,
	.mon_pdev_rx_srng_cleanup = ath12k_dp_mon_pdev_rx_srng_cleanup,
	.mon_pdev_rx_htt_srng_setup = ath12k_dp_mon_pdev_rx_htt_srng_setup,
	.mon_pdev_rx_attach = ath12k_dp_mon_pdev_rx_attach,
	.mon_pdev_rx_mpdu_list_init = ath12k_dp_mon_pdev_rx_mpdu_list_init,
	.mon_rx_srng_process = ath12k_dp_mon_rx_dual_ring_process,
	.update_telemetry_stats = ath12k_dp_mon_pdev_update_telemetry_stats,
	.rx_filter_alloc = ath12k_dp_mon_rx_filter_alloc,
	.rx_filter_free = ath12k_dp_mon_rx_filter_free,
	.rx_stats_enable = ath12k_dp_mon_rx_stats_enable,
	.rx_stats_disable = ath12k_dp_mon_rx_stats_disable,
	.rx_filter_update = ath12k_dp_mon_rx_update_ring_filter,
	.rx_monitor_mode_set = ath12k_dp_mon_rx_monitor_mode_set,
	.rx_monitor_mode_reset = ath12k_dp_mon_rx_monitor_mode_reset,
	.rx_nrp_set = ath12k_dp_mon_rx_nrp_set,
	.rx_nrp_reset = ath12k_dp_mon_rx_nrp_reset,
	.mon_rx_wmask = ath12k_dp_mon_rx_wmask_subscribe,
	.rx_enable_packet_filters = ath12k_dp_mon_rx_enable_packet_filters,
};

static inline void
ath12k_wifi7_dp_mon_rx_memset_ppdu_info(struct hal_rx_mon_ppdu_info *ppdu_info)
{
	memset(ppdu_info, 0, sizeof(*ppdu_info));
	ppdu_info->peer_id = HAL_INVALID_PEERID;
}

static inline void
ath12k_wifi7_dp_mon_rx_parse_status_msdu_end(struct ath12k_mon_data *pmon,
					     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	pmon->err_bitmap = ppdu_info->errmap;
	pmon->decap_format = ppdu_info->decap_format;
}

static int
ath12k_wifi7_dp_mon_rx_parse_status_buf(struct ath12k_pdev_dp *dp_pdev,
					struct ath12k_mon_data *pmon,
					const struct dp_mon_packet_info *packet_info)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_rxdma_mon_ring *buf_ring = &dp_mon->rxdma_mon_buf_ring;
	struct sk_buff *msdu;
	int buf_id;
	u32 offset;

	buf_id = u32_get_bits(packet_info->cookie, DP_RXDMA_BUF_COOKIE_BUF_ID);

	spin_lock_bh(&buf_ring->idr_lock);
	msdu = idr_remove(&buf_ring->bufs_idr, buf_id);
	spin_unlock_bh(&buf_ring->idr_lock);

	if (unlikely(!msdu)) {
		ath12k_warn(ab, "mon dest desc with inval buf_id %d\n", buf_id);
		return 0;
	}

	ath12k_core_dma_unmap_single(ab->dev, ATH12K_SKB_RXCB(msdu)->paddr,
				     msdu->len + skb_tailroom(msdu),
				     DMA_FROM_DEVICE);

	offset = packet_info->dma_length + ATH12K_MON_RX_DOT11_OFFSET;
	if (ath12k_dp_mon_rx_set_pktlen(msdu, offset)) {
		dev_kfree_skb_any(msdu);
		goto buf_replenish;
	}

	if (!pmon->mon_mpdu) {
		dev_kfree_skb_any(msdu);
		goto buf_replenish;
	}

	if (!pmon->mon_mpdu->head)
		pmon->mon_mpdu->head = msdu;
	else
		pmon->mon_mpdu->tail->next = msdu;

	pmon->mon_mpdu->tail = msdu;

buf_replenish:
	ath12k_dp_mon_buf_replenish(ab, buf_ring, 1);

	return 0;
}

static int
ath12k_wifi7_dp_mon_rx_parse_dest_tlv(struct ath12k_pdev_dp *dp_pdev,
				      struct ath12k_mon_data *pmon,
				      enum hal_rx_mon_status hal_status,
				      const void *tlv_data)
{
	switch (hal_status) {
	case HAL_RX_MON_STATUS_MPDU_START:
		/* TODO: Hardware may encounter error scenarios such as DROP
		   descriptor, flush reason, or truncated reason. In these cases, some
		   status buffers associated with a PPDU may be freed prematurely, while
		   others continue to be processed. Current code assumes all TLVs for a
		   PPDU are present and accessible. When some buffers are missing due to
		   the above hardware conditions, driver may attempt to access NULL
		   pointers, leading to crashes or kernel warnings. This issue should be
		   addressed by adding full support for handling these hardware error
		   reasons which is spread across multiple buffers.
		   */
		if (pmon->mon_mpdu)
			break;

		pmon->mon_mpdu = kzalloc(sizeof(*pmon->mon_mpdu), GFP_ATOMIC);
		if (!pmon->mon_mpdu)
			return -ENOMEM;
		break;
	case HAL_RX_MON_STATUS_BUF_ADDR:
		return ath12k_wifi7_dp_mon_rx_parse_status_buf(dp_pdev, pmon, tlv_data);
	case HAL_RX_MON_STATUS_MPDU_END:
		/* If no MSDU then free empty MPDU */
		if (!pmon->mon_mpdu)
			break;

		if (pmon->mon_mpdu->tail) {
			pmon->mon_mpdu->tail->next = NULL;
			list_add_tail(&pmon->mon_mpdu->list, &pmon->dp_rx_mon_mpdu_list);
		} else {
			kfree(pmon->mon_mpdu);
		}
		pmon->mon_mpdu = NULL;
		break;
	case HAL_RX_MON_STATUS_MSDU_END:
		ath12k_wifi7_dp_mon_rx_parse_status_msdu_end(pmon,
							     &pmon->mon_ppdu_info);
		pmon->mon_mpdu->decap_format = pmon->decap_format;
		pmon->mon_mpdu->err_bitmap = pmon->err_bitmap;
		break;
	default:
		break;
	}

	return 0;
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
	struct ath12k *ar = dp_pdev->ar;

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

		hal_status =
			ath12k_wifi7_hal_mon_rx_parse_status_tlv(dp_pdev->dp->hal,
								 &pmon->mon_ppdu_info,
								 tlv);

		if (ar->monitor_started &&
		    ath12k_wifi7_dp_mon_rx_parse_dest_tlv(dp_pdev, pmon, hal_status,
							  tlv->value))
			return HAL_RX_MON_STATUS_PPDU_DONE;

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

enum hal_rx_mon_status
ath12k_wifi7_dp_mon_rx_parse_ppdu_status(struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_mon_data *pmon,
					 struct sk_buff *skb,
					 struct napi_struct *napi)
{
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	struct dp_mon_mpdu *tmp;
	struct dp_mon_mpdu *mon_mpdu = pmon->mon_mpdu;
	enum hal_rx_mon_status hal_status;

	hal_status = ath12k_wifi7_dp_mon_rx_parse_dest(dp_pdev, skb);
	if (hal_status != HAL_RX_MON_STATUS_PPDU_DONE)
		return hal_status;

	list_for_each_entry_safe(mon_mpdu, tmp, &pmon->dp_rx_mon_mpdu_list, list) {
		list_del(&mon_mpdu->list);

		if (mon_mpdu->head && mon_mpdu->tail)
			ath12k_dp_mon_rx_deliver(dp_pdev, mon_mpdu, ppdu_info, napi);

		kfree(mon_mpdu);
	}

	return hal_status;
}

int ath12k_dp_mon_rx_dual_ring_process(struct ath12k_pdev_dp *pdev_dp, int mac_id,
				       struct napi_struct *napi, int *budget)
{
	struct ath12k_dp *dp = pdev_dp->dp;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = pdev_dp->dp_mon_pdev;
	struct ath12k_mon_data *pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	struct hal_mon_dest_desc *mon_dst_desc;
	struct sk_buff *skb;
	struct ath12k_skb_rxcb *rxcb;
	struct dp_srng *mon_dst_ring;
	struct hal_srng *srng;
	struct dp_rxdma_mon_ring *buf_ring;
	struct ath12k_link_sta *arsta;
	struct ath12k_dp_link_peer *peer;
	struct sk_buff_head skb_list;
	struct ath12k_neighbor_peer *nrp, *tmp;
	u64 cookie;
	int num_buffs_reaped = 0, srng_id, buf_id;
	u32 hal_status, end_offset, info0, end_reason;
	u8 pdev_idx = ath12k_hw_mac_id_to_pdev_id(ab->hw_params, pdev_dp->mac_id);
	u8 filter_category = 0;

	__skb_queue_head_init(&skb_list);
	srng_id = ath12k_hw_mac_id_to_srng_id(ab->hw_params, pdev_idx);
	mon_dst_ring = &pdev_dp->dp_mon_pdev->rxdma_mon_dst_ring[srng_id];
	buf_ring = &dp_mon->rxdma_mon_buf_ring;

	srng = &ab->hal.srng_list[mon_dst_ring->ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (likely(*budget)) {
		mon_dst_desc = ath12k_hal_srng_dst_peek(ab, srng);
		if (unlikely(!mon_dst_desc))
			break;

		/* In case of empty descriptor, the cookie in the ring descriptor
		 * is invalid. Therefore, this entry is skipped, and ring processing
		 * continues.
		 */
		info0 = le32_to_cpu(mon_dst_desc->info0);
		if (u32_get_bits(info0, HAL_MON_DEST_INFO0_EMPTY_DESC))
			goto move_next;

		cookie = le32_to_cpu(mon_dst_desc->cookie);
		buf_id = u32_get_bits(cookie, DP_RXDMA_BUF_COOKIE_BUF_ID);

		spin_lock_bh(&buf_ring->idr_lock);
		skb = idr_remove(&buf_ring->bufs_idr, buf_id);
		spin_unlock_bh(&buf_ring->idr_lock);

		if (unlikely(!skb)) {
			ath12k_warn(ab, "monitor destination with invalid buf_id %d\n",
				    buf_id);
			goto move_next;
		}

		rxcb = ATH12K_SKB_RXCB(skb);
		ath12k_core_dma_unmap_single(ab->dev, rxcb->paddr,
					     skb->len + skb_tailroom(skb),
					     DMA_FROM_DEVICE);

		end_reason = u32_get_bits(info0, HAL_MON_DEST_INFO0_END_REASON);

		/* HAL_MON_FLUSH_DETECTED implies that an rx flush received at the end of
		 * rx PPDU and HAL_MON_PPDU_TRUNCATED implies that the PPDU got
		 * truncated due to a system level error. In both the cases, buffer data
		 * can be discarded
		 */
		if ((end_reason == HAL_MON_FLUSH_DETECTED) ||
		    (end_reason == HAL_MON_PPDU_TRUNCATED)) {
			ath12k_dbg(ab, ATH12K_DBG_DATA,
				   "Monitor dest descriptor end reason %d", end_reason);
			dev_kfree_skb_any(skb);
			goto move_next;
		}

		/* Calculate the budget when the ring descriptor with the
		 * HAL_MON_END_OF_PPDU to ensure that one PPDU worth of data is always
		 * reaped. This helps to efficiently utilize the NAPI budget.
		 */
		if (end_reason == HAL_MON_END_OF_PPDU) {
			*budget -= 1;
			rxcb->is_end_of_ppdu = true;
		}

		end_offset = u32_get_bits(info0, HAL_MON_DEST_INFO0_END_OFFSET);
		if (likely(end_offset <= DP_RX_MON_BUFFER_SIZE)) {
			skb_put(skb, end_offset);
		} else {
			ath12k_warn(ab,
				    "invalid offset on mon stats destination %u\n",
				    end_offset);
			skb_put(skb, DP_RX_MON_BUFFER_SIZE);
		}

		__skb_queue_tail(&skb_list, skb);

move_next:
		ath12k_dp_mon_buf_replenish(ab, buf_ring, 1);
		ath12k_hal_srng_dst_get_next_entry(ab, srng);
		num_buffs_reaped++;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	if (!num_buffs_reaped)
		return 0;

	/* In some cases, one PPDU worth of data can be spread across multiple NAPI
	 * schedules, To avoid losing existing parsed ppdu_info information, skip
	 * the memset of the ppdu_info structure and continue processing it.
	 */
	if (!ppdu_info->ppdu_continuation)
		ath12k_wifi7_dp_mon_rx_memset_ppdu_info(ppdu_info);

	while ((skb = __skb_dequeue(&skb_list))) {
		hal_status = ath12k_wifi7_dp_mon_rx_parse_ppdu_status(pdev_dp, pmon,
								      skb, napi);
		if (hal_status != HAL_RX_MON_STATUS_PPDU_DONE) {
			ppdu_info->ppdu_continuation = true;
			dev_kfree_skb_any(skb);
			continue;
		}

		if (ppdu_info->peer_id == HAL_INVALID_PEERID)
			goto free_skb;

		filter_category = ppdu_info->userstats[ppdu_info->userid].filter_category;
		rcu_read_lock();
		spin_lock_bh(&dp->dp_lock);

		if (!list_empty(&dp->neighbor_peers)) {
			list_for_each_entry_safe(nrp, tmp, &dp->neighbor_peers, list) {
				if (filter_category == DP_MPDU_FILTER_CATEGORY_MD &&
					ether_addr_equal(nrp->addr,
							 ppdu_info->nrp_info.mac_addr2)) {
					nrp->rssi = ppdu_info->rssi_comb;
					nrp->timestamp = ktime_to_ms(ktime_get_real());
					goto next_skb;
				}
			}
		}

		peer = ath12k_dp_link_peer_find_by_id(dp, ppdu_info->peer_id);
		if (!peer || !peer->sta) {
			ath12k_dbg(ab, ATH12K_DBG_DATA,
				   "failed to find the peer with monitor peer_id %d\n",
				   ppdu_info->peer_id);
			goto next_skb;
		}

		if (ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_SU) {
			arsta = ath12k_peer_get_link_sta(ab, peer);
			if (!arsta) {
				ath12k_warn(ab, "link sta not found on peer %pM id %d\n",
					    peer->addr, peer->peer_id);
				spin_unlock_bh(&dp->dp_lock);
				rcu_read_unlock();
				dev_kfree_skb_any(skb);
				continue;
			}
			ath12k_dp_mon_rx_update_peer_su_stats(pdev_dp, peer,
							      ppdu_info);
			ath12k_dp_mon_ppdu_rx_time_update(pdev_dp, ppdu_info, 0);
		} else if ((ppdu_info->fc_valid) &&
			   (ppdu_info->ast_index != HAL_AST_IDX_INVALID)) {
			ath12k_dp_mon_rx_process_ulofdma_stats(ppdu_info);
			ath12k_dp_mon_rx_update_peer_mu_stats(pdev_dp, ppdu_info);
			ath12k_dp_mon_ppdu_rx_time_update(pdev_dp, ppdu_info, 0);
		}

next_skb:
		spin_unlock_bh(&dp->dp_lock);
		rcu_read_unlock();
free_skb:
		dev_kfree_skb_any(skb);
		ath12k_wifi7_dp_mon_rx_memset_ppdu_info(ppdu_info);
	}

	return num_buffs_reaped;
}
