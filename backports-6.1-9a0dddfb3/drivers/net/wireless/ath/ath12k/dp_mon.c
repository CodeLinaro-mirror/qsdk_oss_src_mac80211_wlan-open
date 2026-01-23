// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_mon.h"
#include "debug.h"
#include "dp_rx.h"
#include "dp_tx.h"
#include "peer.h"
#include "debugfs.h"
#include "dp_mon_filter.h"
#include "telemetry_agent_if.h"

static inline u32
ath12k_dp_mon_rx_ul_ofdma_ru_size_to_width(enum ath12k_eht_ru_size ru_size)
{
	switch (ru_size) {
	case ATH12K_EHT_RU_26:
		return RU_26;
	case ATH12K_EHT_RU_52:
		return RU_52;
	case ATH12K_EHT_RU_52_26:
		return RU_52_26;
	case ATH12K_EHT_RU_106:
		return RU_106;
	case ATH12K_EHT_RU_106_26:
		return RU_106_26;
	case ATH12K_EHT_RU_242:
		return RU_242;
	case ATH12K_EHT_RU_484:
		return RU_484;
	case ATH12K_EHT_RU_484_242:
		return RU_484_242;
	case ATH12K_EHT_RU_996:
		return RU_996;
	case ATH12K_EHT_RU_996_484:
		return RU_996_484;
	case ATH12K_EHT_RU_996_484_242:
		return RU_996_484_242;
	case ATH12K_EHT_RU_996x2:
		return RU_2X996;
	case ATH12K_EHT_RU_996x2_484:
		return RU_2X996_484;
	case ATH12K_EHT_RU_996x3:
		return RU_3X996;
	case ATH12K_EHT_RU_996x3_484:
		return RU_3X996_484;
	case ATH12K_EHT_RU_996x4:
		return RU_4X996;
	default:
		return RU_INVALID;
	}
}

void
ath12k_dp_mon_fill_rx_stats_info(struct hal_rx_mon_ppdu_info *ppdu_info,
				 struct ieee80211_rx_status *rx_status)
{
	u32 center_freq = ppdu_info->freq;

	rx_status->freq = center_freq;
	rx_status->bw = ath12k_mac_bw_to_mac80211_bw(ppdu_info->bw);
	rx_status->nss = ppdu_info->nss;
	rx_status->rate_idx = 0;
	rx_status->encoding = RX_ENC_LEGACY;
	rx_status->flag |= RX_FLAG_NO_SIGNAL_VAL;

	if (center_freq >= ATH12K_MIN_6GHZ_FREQ &&
	    center_freq <= ATH12K_MAX_6GHZ_FREQ) {
		rx_status->band = NL80211_BAND_6GHZ;
	} else if (center_freq >= ATH12K_MIN_2GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_2GHZ_FREQ) {
		rx_status->band = NL80211_BAND_2GHZ;
	} else if (center_freq >= ATH12K_MIN_5GHZ_FREQ &&
		   center_freq <= ATH12K_MAX_5GHZ_FREQ) {
		rx_status->band = NL80211_BAND_5GHZ;
	} else {
		rx_status->band = NUM_NL80211_BANDS;
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_fill_rx_stats_info);

static void
ath12k_dp_mon_rx_update_radiotap_he(struct hal_rx_mon_ppdu_info *rx_status,
				    u8 *rtap_buf)
{
	u32 rtap_len = 0;

	put_unaligned_le16(rx_status->he_data1, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data2, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data3, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data4, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data5, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_data6, &rtap_buf[rtap_len]);
}

static void
ath12k_dp_mon_rx_update_radiotap_he_mu(struct hal_rx_mon_ppdu_info *rx_status,
				       u8 *rtap_buf)
{
	u32 rtap_len = 0;

	put_unaligned_le16(rx_status->he_flags1, &rtap_buf[rtap_len]);
	rtap_len += 2;

	put_unaligned_le16(rx_status->he_flags2, &rtap_buf[rtap_len]);
	rtap_len += 2;

	rtap_buf[rtap_len] = rx_status->he_RU[0];
	rtap_len += 1;

	rtap_buf[rtap_len] = rx_status->he_RU[1];
	rtap_len += 1;

	rtap_buf[rtap_len] = rx_status->he_RU[2];
	rtap_len += 1;

	rtap_buf[rtap_len] = rx_status->he_RU[3];
}

void ath12k_dp_mon_update_radiotap(struct ath12k_pdev_dp *dp_pdev,
				   struct hal_rx_mon_ppdu_info *ppduinfo,
				   struct sk_buff *mon_skb,
				   struct ieee80211_rx_status *rxs)
{
	struct ieee80211_supported_band *sband;
	u8 *ptr = NULL;

	rxs->flag |= RX_FLAG_MACTIME_START;
	rxs->signal = (s8)(ppduinfo->rssi_comb + dp_pdev->ar->rssi_offsets.rssi_offset);
	rxs->flag &= ~RX_FLAG_NO_SIGNAL_VAL;
	rxs->noise = dp_pdev->ar->rssi_offsets.avg_nf_dbm;
	rxs->nss = ppduinfo->nss + 1;

	if (ppduinfo->userstats[ppduinfo->userid].ampdu_present) {
		rxs->flag |= RX_FLAG_AMPDU_DETAILS;
		rxs->ampdu_reference = ppduinfo->userstats[ppduinfo->userid].ampdu_id;
	}

	if (ppduinfo->is_eht || ppduinfo->eht_usig) {
		struct ieee80211_radiotap_tlv *tlv;
		struct ieee80211_radiotap_eht *eht;
		struct ieee80211_radiotap_eht_usig *usig;
		u16 len = 0, i, eht_len = 0, usig_len;
		u8 user;

		if (ppduinfo->is_eht) {
			eht_len = struct_size(eht,
					      user_info,
					      ppduinfo->eht_info.num_user_info);
			len += sizeof(*tlv) + eht_len;
		}

		if (ppduinfo->eht_usig) {
			usig_len = sizeof(*usig);
			len += sizeof(*tlv) + usig_len;
		}

		rxs->flag |= RX_FLAG_RADIOTAP_TLV_AT_END;
		rxs->encoding = RX_ENC_EHT;

		skb_reset_mac_header(mon_skb);

		tlv = skb_push(mon_skb, len);

		if (ppduinfo->is_eht) {
			tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_EHT);
			tlv->len = cpu_to_le16(eht_len);

			eht = (struct ieee80211_radiotap_eht *)tlv->data;
			eht->known = ppduinfo->eht_info.eht.known;

			for (i = 0;
			     i < ARRAY_SIZE(eht->data) &&
			     i < ARRAY_SIZE(ppduinfo->eht_info.eht.data);
			     i++)
				eht->data[i] = ppduinfo->eht_info.eht.data[i];

			for (user = 0; user < ppduinfo->eht_info.num_user_info; user++)
				put_unaligned_le32(ppduinfo->eht_info.user_info[user],
						   &eht->user_info[user]);

			tlv = (struct ieee80211_radiotap_tlv *)&tlv->data[eht_len];
		}

		if (ppduinfo->eht_usig) {
			tlv->type = cpu_to_le16(IEEE80211_RADIOTAP_EHT_USIG);
			tlv->len = cpu_to_le16(usig_len);

			usig = (struct ieee80211_radiotap_eht_usig *)tlv->data;
			*usig = ppduinfo->u_sig_info.usig;
		}
	} else if (ppduinfo->he_mu_flags) {
		rxs->flag |= RX_FLAG_RADIOTAP_HE_MU;
		rxs->encoding = RX_ENC_HE;
		ptr = skb_push(mon_skb, sizeof(struct ieee80211_radiotap_he_mu));
		ath12k_dp_mon_rx_update_radiotap_he_mu(ppduinfo, ptr);
	} else if (ppduinfo->he_flags) {
		rxs->flag |= RX_FLAG_RADIOTAP_HE;
		rxs->encoding = RX_ENC_HE;
		ptr = skb_push(mon_skb, sizeof(struct ieee80211_radiotap_he));
		ath12k_dp_mon_rx_update_radiotap_he(ppduinfo, ptr);
		rxs->rate_idx = ppduinfo->rate;
	} else if (ppduinfo->vht_flags) {
		rxs->encoding = RX_ENC_VHT;
		rxs->rate_idx = ppduinfo->rate;
	} else if (ppduinfo->ht_flags) {
		rxs->encoding = RX_ENC_HT;
		rxs->rate_idx = ppduinfo->rate;
	} else {
		rxs->encoding = RX_ENC_LEGACY;
		if (rxs->band < NUM_NL80211_BANDS) {
			sband = &dp_pdev->ar->mac.sbands[rxs->band];
			rxs->rate_idx =
				ath12k_mac_hw_rate_to_idx(sband, ppduinfo->rate,
							  ppduinfo->cck_flag);
		}
	}

	rxs->mactime = ppduinfo->tsft;
}
EXPORT_SYMBOL(ath12k_dp_mon_update_radiotap);

void ath12k_dp_mon_rx_deliver_skb(struct ath12k_pdev_dp *dp_pdev,
				  struct napi_struct *napi,
				  struct sk_buff *msdu,
				  struct ieee80211_rx_status *status,
				  struct hal_rx_mon_ppdu_info *ppduinfo)
{
	static const struct ieee80211_radiotap_he known = {
		.data1 = cpu_to_le16(IEEE80211_RADIOTAP_HE_DATA1_DATA_MCS_KNOWN |
				     IEEE80211_RADIOTAP_HE_DATA1_BW_RU_ALLOC_KNOWN),
		.data2 = cpu_to_le16(IEEE80211_RADIOTAP_HE_DATA2_GI_KNOWN),
	};
	struct ieee80211_rx_status *rx_status;
	struct ieee80211_radiotap_he *he = NULL;

	status->link_valid = 0;
	status->link_id = 0;

	if ((status->encoding == RX_ENC_HE) && !(status->flag & RX_FLAG_RADIOTAP_HE) &&
	    !(status->flag & RX_FLAG_SKIP_MONITOR)) {
		he = skb_push(msdu, sizeof(known));
		memcpy(he, &known, sizeof(known));
		status->flag |= RX_FLAG_RADIOTAP_HE;
	}

	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	if (!napi)
		ieee80211_rx_ni(ath12k_dp_pdev_to_hw(dp_pdev), msdu);
	else
		ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), NULL, msdu, napi);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_deliver_skb);

int ath12k_dp_mon_rx_set_pktlen(struct sk_buff *skb, u32 len)
{
	if (skb->len > len) {
		skb_trim(skb, len);
	} else {
		if (skb_tailroom(skb) < len - skb->len) {
			if ((pskb_expand_head(skb, 0,
					      len - skb->len - skb_tailroom(skb),
					      GFP_ATOMIC))) {
				return -ENOMEM;
			}
		}
		skb_put(skb, (len - skb->len));
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_set_pktlen);

static void
ath12k_dp_mon_handle_mon_desc(struct ath12k_dp *dp, struct ath12k_dp_mon_desc *mon_desc)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	u8 *mon_buf = mon_desc->mon_buf;

	if (mon_buf) {
		ath12k_core_dma_unmap_page(dp->dev, mon_desc->paddr,
					   ATH12K_DP_MON_RX_BUF_SIZE,
					   DMA_FROM_DEVICE);
		page_frag_free(mon_buf);
		dp_mon->num_frag_free++;
		mon_desc->mon_buf = NULL;
	}

	spin_lock_bh(&dp->dp_mon->mon_desc_lock);
	list_del(&mon_desc->list);
	list_add_tail(&mon_desc->list, &dp_mon->mon_desc_free_list);
	spin_unlock_bh(&dp->dp_mon->mon_desc_lock);
}

int ath12k_dp_mon_buf_replenish(struct ath12k_dp *dp,
				struct dp_rxdma_mon_ring *buf_ring,
				struct list_head *used_list,
				int req_entries)
{
	struct ath12k_base *ab = dp->ab;
	void *mon_buf_desc;
	struct hal_srng *srng;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_mon_desc *mon_desc, *tmp_mon_desc;
	struct page *page;
	u64 offset, cookie;
	u32 addr_lo, addr_hi;
	dma_addr_t paddr;
	int ret = 0;
	u8 *mon_buf;

	list_for_each_entry_safe(mon_desc, tmp_mon_desc, used_list, list) {
		if (unlikely(mon_desc->in_use != DP_MON_DESC_REPLENISH)) {
			ath12k_warn(dp,
				    "Invalid in_use %d, possibly desc from freelist\n",
				    mon_desc->in_use);
			ath12k_dp_mon_handle_mon_desc(dp, mon_desc);
			continue;
		}

		mon_buf = page_frag_alloc(&dp_mon->rx_mon_pf_cache,
					  ATH12K_DP_MON_RX_BUF_SIZE,
					  GFP_ATOMIC);
		if (unlikely(!mon_buf)) {
			ret = -ENOMEM;
			goto out;
		}

		page = virt_to_head_page(mon_buf);
		offset = ((void *)mon_buf) - page_address(page);
		paddr = ath12k_core_dma_map_page(ab->dev, page, offset,
						 ATH12K_DP_MON_RX_BUF_SIZE,
						 DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(ab->dev, paddr))) {
			page_frag_free(mon_buf);
			mon_desc->mon_buf = NULL;
			dp_mon->num_frag_free++;
			ret = -EIO;
			goto out;
		}

		mon_desc->mon_buf = mon_buf;
		mon_desc->paddr = paddr;
		mon_desc->magic = ATH12K_MON_MAGIC_VALUE;
		mon_desc->in_use = DP_MON_DESC_TO_HW;
		mon_desc->buf_len = 0;
		mon_desc->end_of_ppdu = 0;
		dp_mon->num_frag_replenish++;
	}

	srng = &ab->hal.srng_list[buf_ring->refill_buf_ring.ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (req_entries > 0) {
		mon_desc = list_first_entry_or_null(used_list,
						    struct ath12k_dp_mon_desc, list);
		if (unlikely(!mon_desc)) {
			ret = -ENOSPC;
			goto ring_unlock;
		}

		mon_buf_desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
		if (unlikely(!mon_buf_desc)) {
			ret = -ENOSPC;
			goto ring_unlock;
		}

		list_del(&mon_desc->list);
		addr_lo = cpu_to_le32(lower_32_bits(mon_desc->paddr));
		addr_hi = cpu_to_le32(upper_32_bits(mon_desc->paddr));
		cookie = cpu_to_le64((uintptr_t)mon_desc);
		ath12k_hal_mon_set_mon_buf_desc(&ab->hal, mon_buf_desc, addr_lo,
						addr_hi, cookie);
		req_entries--;
	}

ring_unlock:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

out:
	if (unlikely(!list_empty(used_list))) {
		/* Reset the use flag */
		list_for_each_entry_safe(mon_desc, tmp_mon_desc, used_list, list) {
			mon_buf = mon_desc->mon_buf;
			if (mon_buf) {
				ath12k_core_dma_unmap_page(ab->dev, mon_desc->paddr,
							   ATH12K_DP_MON_RX_BUF_SIZE,
							   DMA_FROM_DEVICE);
				page_frag_free(mon_buf);
				mon_desc->mon_buf = NULL;
				dp_mon->num_frag_free++;
			}

			ath12k_dp_mon_desc_reset(mon_desc);
			mon_desc->in_use = DP_MON_DESC_H_REPLENISH_ERR;
		}

		spin_lock_bh(&dp_mon->mon_desc_lock);
		list_splice_tail(used_list, &dp_mon->mon_desc_free_list);
		spin_unlock_bh(&dp_mon->mon_desc_lock);
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_buf_replenish);

size_t ath12k_dp_mon_list_cut_nodes(struct list_head *list, struct list_head *head,
				    size_t count)
{
	struct list_head *cur;
	struct ath12k_dp_mon_desc *mon_desc;
	size_t nodes = 0;

	if (!count) {
		INIT_LIST_HEAD(list);
		goto out;
	}

	list_for_each(cur, head) {
		if (!count)
			break;

		mon_desc = list_entry(cur, struct ath12k_dp_mon_desc, list);
		ath12k_dp_mon_desc_reset(mon_desc);
		mon_desc->in_use = DP_MON_DESC_REPLENISH;

		count--;
		nodes++;
	}

	list_cut_before(list, head, cur);

out:
	return nodes;
}

size_t ath12k_dp_mon_get_req_entries_from_buf_ring(struct ath12k_dp *dp,
						   struct dp_rxdma_mon_ring *rx_ring,
						   struct list_head *list)
{
	struct hal_srng *srng;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	size_t num_free, req_entries;

	srng = &dp->hal->srng_list[rx_ring->refill_buf_ring.ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);
	num_free = ath12k_hal_srng_src_num_free(ab, srng, true);
	if (!num_free) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&srng->lock);
		return 0;
	}
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	spin_lock_bh(&dp_mon->mon_desc_lock);
	req_entries = ath12k_dp_mon_list_cut_nodes(list,
						   &dp_mon->mon_desc_free_list,
						   num_free);
	spin_unlock_bh(&dp_mon->mon_desc_lock);

	return req_entries;
}

static struct dp_mon_tx_ppdu_info *
ath12k_dp_mon_tx_get_ppdu_info(struct ath12k_mon_data *pmon,
			       unsigned int ppdu_id,
			       enum dp_mon_tx_ppdu_info_type type)
{
	struct dp_mon_tx_ppdu_info *tx_ppdu_info;

	if (type == DP_MON_TX_PROT_PPDU_INFO) {
		tx_ppdu_info = pmon->tx_prot_ppdu_info;

		if (tx_ppdu_info && !tx_ppdu_info->is_used)
			return tx_ppdu_info;
		kfree(tx_ppdu_info);
	} else {
		tx_ppdu_info = pmon->tx_data_ppdu_info;

		if (tx_ppdu_info && !tx_ppdu_info->is_used)
			return tx_ppdu_info;
		kfree(tx_ppdu_info);
	}

	/* allocate new tx_ppdu_info */
	tx_ppdu_info = kzalloc(sizeof(*tx_ppdu_info), GFP_ATOMIC);
	if (!tx_ppdu_info)
		return NULL;

	tx_ppdu_info->is_used = 0;
	tx_ppdu_info->tx_info.ppdu_id = ppdu_id;

	if (type == DP_MON_TX_PROT_PPDU_INFO)
		pmon->tx_prot_ppdu_info = tx_ppdu_info;
	else
		pmon->tx_data_ppdu_info = tx_ppdu_info;

	return tx_ppdu_info;
}


#define MAX_MONITOR_HEADER 512
#define MAX_DUMMY_FRM_BODY 128

struct sk_buff *ath12k_dp_mon_tx_alloc_skb(void)
{
	struct sk_buff *skb;

	skb = dev_alloc_skb(MAX_MONITOR_HEADER + MAX_DUMMY_FRM_BODY);
	if (!skb)
		return NULL;

	skb_reserve(skb, MAX_MONITOR_HEADER);

	if (!IS_ALIGNED((unsigned long)skb->data, 4))
		skb_pull(skb, PTR_ALIGN(skb->data, 4) - skb->data);

	return skb;
}

static int
ath12k_dp_mon_tx_gen_cts2self_frame(struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	struct sk_buff *skb;
	struct ieee80211_cts *cts;

	skb = ath12k_dp_mon_tx_alloc_skb();
	if (!skb)
		return -ENOMEM;

	cts = (struct ieee80211_cts *)skb->data;
	memset(cts, 0, MAX_DUMMY_FRM_BODY);
	cts->frame_control =
		cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_CTS);
	cts->duration = cpu_to_le16(tx_ppdu_info->tx_info.rx_status.rx_duration);
	memcpy(cts->ra, tx_ppdu_info->tx_info.rx_status.addr1, sizeof(cts->ra));

	skb_put(skb, sizeof(*cts));
	tx_ppdu_info->tx_mon_mpdu->head = skb;
	tx_ppdu_info->tx_mon_mpdu->tail = NULL;
	list_add_tail(&tx_ppdu_info->tx_mon_mpdu->list,
		      &tx_ppdu_info->dp_tx_mon_mpdu_list);

	return 0;
}

static int
ath12k_dp_mon_tx_gen_rts_frame(struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	struct sk_buff *skb;
	struct ieee80211_rts *rts;

	skb = ath12k_dp_mon_tx_alloc_skb();
	if (!skb)
		return -ENOMEM;

	rts = (struct ieee80211_rts *)skb->data;
	memset(rts, 0, MAX_DUMMY_FRM_BODY);
	rts->frame_control =
		cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_RTS);
	rts->duration = cpu_to_le16(tx_ppdu_info->tx_info.rx_status.rx_duration);
	memcpy(rts->ra, tx_ppdu_info->tx_info.rx_status.addr1, sizeof(rts->ra));
	memcpy(rts->ta, tx_ppdu_info->tx_info.rx_status.addr2, sizeof(rts->ta));

	skb_put(skb, sizeof(*rts));
	tx_ppdu_info->tx_mon_mpdu->head = skb;
	tx_ppdu_info->tx_mon_mpdu->tail = NULL;
	list_add_tail(&tx_ppdu_info->tx_mon_mpdu->list,
		      &tx_ppdu_info->dp_tx_mon_mpdu_list);

	return 0;
}

static int
ath12k_dp_mon_tx_gen_3addr_qos_null_frame(struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	struct sk_buff *skb;
	struct ieee80211_qos_hdr *qhdr;

	skb = ath12k_dp_mon_tx_alloc_skb();
	if (!skb)
		return -ENOMEM;

	qhdr = (struct ieee80211_qos_hdr *)skb->data;
	memset(qhdr, 0, MAX_DUMMY_FRM_BODY);
	qhdr->frame_control =
		cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_NULLFUNC);
	qhdr->duration_id = cpu_to_le16(tx_ppdu_info->tx_info.rx_status.rx_duration);
	memcpy(qhdr->addr1, tx_ppdu_info->tx_info.rx_status.addr1, ETH_ALEN);
	memcpy(qhdr->addr2, tx_ppdu_info->tx_info.rx_status.addr2, ETH_ALEN);
	memcpy(qhdr->addr3, tx_ppdu_info->tx_info.rx_status.addr3, ETH_ALEN);

	skb_put(skb, sizeof(*qhdr));
	tx_ppdu_info->tx_mon_mpdu->head = skb;
	tx_ppdu_info->tx_mon_mpdu->tail = NULL;
	list_add_tail(&tx_ppdu_info->tx_mon_mpdu->list,
		      &tx_ppdu_info->dp_tx_mon_mpdu_list);

	return 0;
}

static int
ath12k_dp_mon_tx_gen_4addr_qos_null_frame(struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	struct sk_buff *skb;
	struct dp_mon_qosframe_addr4 *qhdr;

	skb = ath12k_dp_mon_tx_alloc_skb();
	if (!skb)
		return -ENOMEM;

	qhdr = (struct dp_mon_qosframe_addr4 *)skb->data;
	memset(qhdr, 0, MAX_DUMMY_FRM_BODY);
	qhdr->frame_control =
		cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_NULLFUNC);
	qhdr->duration = cpu_to_le16(tx_ppdu_info->tx_info.rx_status.rx_duration);
	memcpy(qhdr->addr1, tx_ppdu_info->tx_info.rx_status.addr1, ETH_ALEN);
	memcpy(qhdr->addr2, tx_ppdu_info->tx_info.rx_status.addr2, ETH_ALEN);
	memcpy(qhdr->addr3, tx_ppdu_info->tx_info.rx_status.addr3, ETH_ALEN);
	memcpy(qhdr->addr4, tx_ppdu_info->tx_info.rx_status.addr4, ETH_ALEN);

	skb_put(skb, sizeof(*qhdr));
	tx_ppdu_info->tx_mon_mpdu->head = skb;
	tx_ppdu_info->tx_mon_mpdu->tail = NULL;
	list_add_tail(&tx_ppdu_info->tx_mon_mpdu->list,
		      &tx_ppdu_info->dp_tx_mon_mpdu_list);

	return 0;
}

static int
ath12k_dp_mon_tx_gen_ack_frame(struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	struct sk_buff *skb;
	struct dp_mon_frame_min_one *fbmhdr;

	skb = ath12k_dp_mon_tx_alloc_skb();
	if (!skb)
		return -ENOMEM;

	fbmhdr = (struct dp_mon_frame_min_one *)skb->data;
	memset(fbmhdr, 0, MAX_DUMMY_FRM_BODY);
	fbmhdr->frame_control =
		cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_CFACK);
	memcpy(fbmhdr->addr1, tx_ppdu_info->tx_info.rx_status.addr1, ETH_ALEN);

	/* set duration zero for ack frame */
	fbmhdr->duration = 0;

	skb_put(skb, sizeof(*fbmhdr));
	tx_ppdu_info->tx_mon_mpdu->head = skb;
	tx_ppdu_info->tx_mon_mpdu->tail = NULL;
	list_add_tail(&tx_ppdu_info->tx_mon_mpdu->list,
		      &tx_ppdu_info->dp_tx_mon_mpdu_list);

	return 0;
}

static int
ath12k_dp_mon_tx_gen_prot_frame(struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	int ret = 0;

	switch (tx_ppdu_info->tx_info.rx_status.medium_prot_type) {
	case DP_MON_TX_MEDIUM_RTS_LEGACY:
	case DP_MON_TX_MEDIUM_RTS_11AC_STATIC_BW:
	case DP_MON_TX_MEDIUM_RTS_11AC_DYNAMIC_BW:
		ret = ath12k_dp_mon_tx_gen_rts_frame(tx_ppdu_info);
		break;
	case DP_MON_TX_MEDIUM_CTS2SELF:
		ret = ath12k_dp_mon_tx_gen_cts2self_frame(tx_ppdu_info);
		break;
	case DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_3ADDR:
		ret = ath12k_dp_mon_tx_gen_3addr_qos_null_frame(tx_ppdu_info);
		break;
	case DP_MON_TX_MEDIUM_QOS_NULL_NO_ACK_4ADDR:
		ret = ath12k_dp_mon_tx_gen_4addr_qos_null_frame(tx_ppdu_info);
		break;
	}

	return ret;
}

static int
ath12k_dp_mon_tx_process_status_tlv(u32 tlv_status,
				    struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	int ret = 0;
	struct dp_mon_mpdu *mon_mpdu = tx_ppdu_info->tx_mon_mpdu;

	switch (tlv_status) {
	case HAL_TX_MON_MPDU_START:
		mon_mpdu = kzalloc(sizeof(*mon_mpdu), GFP_ATOMIC);
		break;
	case HAL_TX_MON_MPDU_END:
		list_add_tail(&tx_ppdu_info->tx_mon_mpdu->list,
			      &tx_ppdu_info->dp_tx_mon_mpdu_list);
		break;
	case HAL_TX_MON_FES_STATUS_PROT:
		ret = ath12k_dp_mon_tx_gen_prot_frame(tx_ppdu_info);
		break;
	case HAL_TX_MON_FRAME_BITMAP_ACK:
		ret = ath12k_dp_mon_tx_gen_ack_frame(tx_ppdu_info);
		break;
	case HAL_RX_MON_RESPONSE_REQUIRED_INFO:
		if (tx_ppdu_info->tx_info.rx_status.reception_type == 0)
			ret = ath12k_dp_mon_tx_gen_cts2self_frame(tx_ppdu_info);
		break;
	}

	return ret;
}

static void
ath12k_dp_mon_tx_process_ppdu_info(struct ath12k_pdev_dp *dp_pdev,
				   struct napi_struct *napi,
				   struct dp_mon_tx_ppdu_info *tx_ppdu_info)
{
	struct dp_mon_mpdu *tmp, *mon_mpdu;

	list_for_each_entry_safe(mon_mpdu, tmp,
				 &tx_ppdu_info->dp_tx_mon_mpdu_list, list) {
		list_del(&mon_mpdu->list);
		/* TODO: Call ath12k_dp_mon_rx_deliver while enabling TX monitor
		if (mon_mpdu->head)
			ath12k_dp_mon_rx_deliver(dp_pdev, mon_mpdu,
						 &tx_ppdu_info->tx_info.rx_status, napi);
		 */
		kfree(mon_mpdu);
	}
}

enum hal_tx_mon_status
ath12k_dp_mon_tx_parse_mon_status(struct ath12k_pdev_dp *dp_pdev,
				  struct ath12k_mon_data *pmon,
				  struct sk_buff *skb,
				  struct napi_struct *napi,
				  u32 ppdu_id)
{
	struct dp_mon_tx_ppdu_info *tx_prot_ppdu_info, *tx_data_ppdu_info;
	struct dp_mon_tx_ppdu_info *tx_ppdu_info;
	struct hal_tlv_hdr *tlv;
	u8 *ptr = skb->data;
	u16 tlv_tag;
	u16 tlv_len;
	u32 tlv_userid = 0;
	u8 num_user;
	enum hal_tx_mon_status hal_status = HAL_TX_MON_STATUS_PPDU_NOT_DONE;

	tx_prot_ppdu_info = ath12k_dp_mon_tx_get_ppdu_info(pmon, ppdu_id,
							   DP_MON_TX_PROT_PPDU_INFO);
	if (!tx_prot_ppdu_info)
		return -ENOMEM;

	tlv = (struct hal_tlv_hdr *)ptr;
	dp_pdev->dp->hal->hal_ops->hal_get_tlv_tag_params(tlv->tl, &tlv_tag, &tlv_userid,
							  &tlv_len);

	hal_status = ath12k_hal_mon_tx_status_get_num_user(dp_pdev->dp->hal,
							   tlv_tag, tlv, &num_user);
	if (hal_status == HAL_TX_MON_STATUS_PPDU_NOT_DONE || !num_user)
		return -EINVAL;

	tx_data_ppdu_info = ath12k_dp_mon_tx_get_ppdu_info(pmon, ppdu_id,
							   DP_MON_TX_DATA_PPDU_INFO);
	if (!tx_data_ppdu_info)
		return -ENOMEM;

	do {
		tlv = (struct hal_tlv_hdr *)ptr;
		dp_pdev->dp->hal->hal_ops->hal_get_tlv_tag_params(tlv->tl, &tlv_tag,
								  &tlv_userid,
								  &tlv_len);

		tx_ppdu_info = ath12k_hal_mon_tx_ppdu_info(dp_pdev->dp->hal,
							   pmon,
							   tlv_tag);

		hal_status = ath12k_hal_mon_tx_parse_status(dp_pdev->dp->hal,
							    &tx_ppdu_info->tx_info,
							    tlv_tag, ptr,
							    tlv_userid);
		ath12k_dp_mon_tx_process_status_tlv(hal_status,
						    tx_ppdu_info);

		ptr += tlv_len;
		ptr = PTR_ALIGN(ptr, HAL_TLV_ALIGN);
		if ((ptr - skb->data) >= DP_TX_MONITOR_BUF_SIZE)
			break;
	} while (hal_status != HAL_TX_MON_FES_STATUS_END);

	ath12k_dp_mon_tx_process_ppdu_info(dp_pdev, napi, tx_data_ppdu_info);
	ath12k_dp_mon_tx_process_ppdu_info(dp_pdev, napi, tx_prot_ppdu_info);

	return hal_status;
}

static void
ath12k_dp_mon_rx_update_peer_rate_table_stats(struct ath12k_rx_peer_stats *rx_stats,
					      struct hal_rx_mon_ppdu_info *ppdu_info,
					      struct hal_rx_user_status *user_stats,
					      u32 num_msdu)
{
	struct ath12k_rx_peer_rate_stats *stats;
	u32 mcs_idx = (user_stats) ? user_stats->mcs : ppdu_info->mcs;
	u32 nss_idx = (user_stats) ? user_stats->nss - 1 : ppdu_info->nss - 1;
	u32 bw_idx = ppdu_info->bw;
	u32 gi_idx = ppdu_info->gi;
	u32 len;

	if (mcs_idx > HAL_RX_MAX_MCS_BE || nss_idx >= HAL_RX_MAX_NSS ||
	    bw_idx >= HAL_RX_BW_MAX || gi_idx >= HAL_RX_GI_MAX) {
		return;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11AX ||
	    ppdu_info->preamble_type == HAL_RX_PREAMBLE_11BE)
		gi_idx = ath12k_he_gi_to_nl80211_he_gi(ppdu_info->gi);

	rx_stats->pkt_stats.rx_rate[bw_idx][gi_idx][nss_idx][mcs_idx] += num_msdu;
	stats = &rx_stats->byte_stats;

	if (user_stats)
		len = user_stats->mpdu_ok_byte_count;
	else
		len = ppdu_info->mpdu_len;

	stats->rx_rate[bw_idx][gi_idx][nss_idx][mcs_idx] += len;
}

/**
 * ath12k_dp_mon_rx_calculate_avg_rate() - Calculate the average rate using a
 * low-pass filter.
 * @avg_rate: The current average rate.
 * @new_rate_sample: The new rate sample to include in the average.
 *
 * This function applies a low-pass filter to average the data rate. It gives
 * more weight to the previous average, creating a smoothing effect.
 *
 * Return: The new calculated average rate.
 */
static inline u64 ath12k_dp_mon_rx_calculate_avg_rate(u64 avg_rate, int new_rate_sample)
{
	new_rate_sample = DP_ATH_RATE_IN(new_rate_sample);
	if (avg_rate == DUMMY_MARKER)
		return new_rate_sample;

	return ((avg_rate << 3) + new_rate_sample - avg_rate) >> 3;
}

/**
 * ath12k_dp_mon_rx_get_output_rate() - Convert an internal rate representation to an
 * output rate.
 * @internal_rate: The internal rate value to be converted.
 *
 * This function converts an internal rate representation to a final
 * output rate, including rounding logic.
 *
 * Return: The converted output rate or DUMMY_MARKER if the input is a dummy.
 */
static inline u64 ath12k_dp_mon_rx_get_output_rate(u64 internal_rate)
{
	const int multiplier = DP_ATH_RATE_EP_MULTIPLIER;

	if (internal_rate == DUMMY_MARKER)
		return DUMMY_MARKER;

	if ((internal_rate % multiplier) >= (multiplier / 2))
		return (internal_rate + (multiplier - 1)) / multiplier;
	else
		return internal_rate / multiplier;
}

static inline u16 ath12k_dp_get_avg_rate_stats_filter_val(void)
{
	/* Note: Returning default as ini config is absent */
	return DP_AVG_RATE_FILTER_DEFAULT;
}

static inline bool ath12k_dp_mon_eval_avg_rate_filter(u32 ratekbps, u32 avg_rx_rate)
{
	u16 filter_val = 0;

	filter_val = ath12k_dp_get_avg_rate_stats_filter_val();

	if (!filter_val || avg_rx_rate < filter_val || ratekbps > filter_val)
		return true;

	return false;
}

static void ath12k_dp_rx_update_rate_stats(struct ath12k_rx_peer_stats *rx_stats,
					   struct rate_info *rate)
{
	u32 ratekbps, ppdu_rx_rate;

	if (!rx_stats || !rate)
		return;

	/* Converting cfg80211_calculate_bitrate(100Kbps) to Kbps */
	ratekbps = cfg80211_calculate_bitrate(rate) * 100;
	rx_stats->last_rx_rate = ratekbps;

	if (likely(ath12k_dp_mon_eval_avg_rate_filter(ratekbps, rx_stats->avg_rx_rate))) {
		rx_stats->avg_rx_rate =
			ath12k_dp_mon_rx_calculate_avg_rate(rx_stats->avg_rx_rate,
							    ratekbps);
	}

	ppdu_rx_rate = ath12k_dp_mon_rx_get_output_rate(rx_stats->avg_rx_rate);
	rx_stats->rnd_avg_rx_rate = ppdu_rx_rate;

	if (rx_stats->preamble_info < HAL_RX_PREAMBLE_11N)
		rx_stats->rx_ratecode = ath12k_mac_get_rate_hw_value(ratekbps);
	else
		rx_stats->rx_ratecode =
			ATH12K_HW_RATE_CODE(rate->mcs, rate->nss,
					    rx_stats->preamble_info);
}

static u8 ath12k_dp_rx_rate_convert_bw(u8 bw)
{
	u8 ret = 0;

	switch (bw) {
	case CMN_BW_20MHZ:
		ret = RATE_INFO_BW_20;
		break;
	case CMN_BW_40MHZ:
		ret = RATE_INFO_BW_40;
		break;
	case CMN_BW_80MHZ:
		ret = RATE_INFO_BW_80;
		break;
	case CMN_BW_160MHZ:
		ret = RATE_INFO_BW_160;
		break;
	case CMN_BW_320MHZ:
		ret = RATE_INFO_BW_320;
		break;
	default:
		ret = RATE_INFO_BW_20;
		break;
	}

	return ret;
}

static void ath12k_dp_rx_fill_rate_info(struct rate_info *rate,
					struct hal_rx_mon_ppdu_info *ppdu_info,
					struct hal_rx_user_status *user_stats,
					bool is_su)
{
	u8 mcs, nss, preamble_type;
	u8 rix = 0, ret;
	u16 bitrate = 0;

	if (!rate || !ppdu_info)
		return;

	mcs = (user_stats) ? user_stats->mcs : ppdu_info->mcs;
	nss = (user_stats) ? user_stats->nss : ppdu_info->nss;
	preamble_type = ppdu_info->preamble_type;

	rate->nss = nss;
	rate->bw = ath12k_mac_bw_to_mac80211_bw(ppdu_info->bw);

	switch (preamble_type) {
	case HAL_RX_PREAMBLE_11A:
	case HAL_RX_PREAMBLE_11B:
		ret = ath12k_mac_hw_ratecode_to_legacy_rate(mcs, preamble_type,
							    &rix, &bitrate);
		if (ret < 0)
			return;
		rate->legacy = bitrate;
		break;

	case HAL_RX_PREAMBLE_11N:
		if (mcs > HAL_RX_MAX_MCS_HT || nss < 1 || nss > HAL_RX_MAX_NSS)
			return;
		rate->mcs = mcs + 8 * (nss - 1);
		rate->flags = RATE_INFO_FLAGS_MCS;
		if (ppdu_info->sgi)
			rate->flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;

	case HAL_RX_PREAMBLE_11AC:
		if (mcs > HAL_RX_MAX_MCS_VHT)
			return;
		rate->mcs = mcs;
		rate->flags = RATE_INFO_FLAGS_VHT_MCS;
		if (ppdu_info->sgi)
			rate->flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;

	case HAL_RX_PREAMBLE_11AX:
		if (mcs > HAL_RX_MAX_MCS_HE)
			return;
		rate->mcs = mcs;
		rate->flags = RATE_INFO_FLAGS_HE_MCS;
		rate->he_gi = ath12k_he_gi_to_nl80211_he_gi(ppdu_info->sgi);
		if (is_su) {
			rate->bw = ath12k_dp_rx_rate_convert_bw(ppdu_info->bw);
		} else {
			rate->bw = RATE_INFO_BW_HE_RU;
			rate->he_ru_alloc = ppdu_info->ru_alloc;
		}
		break;

	case HAL_RX_PREAMBLE_11BE:
		if (mcs > HAL_RX_MAX_MCS_BE)
			return;
		rate->mcs = mcs;
		rate->flags = RATE_INFO_FLAGS_EHT_MCS;
		rate->eht_gi = ath12k_eht_gi_to_nl80211_eht_gi(ppdu_info->sgi);
		if (is_su) {
			rate->bw = ath12k_dp_rx_rate_convert_bw(ppdu_info->bw);
		} else {
			rate->bw = RATE_INFO_BW_EHT_RU;
			rate->eht_ru_alloc = ppdu_info->ru_alloc;
		}
		break;

	default:
		return;
	}
}

static void ath12k_dp_rx_rate_stats_update(struct ath12k_rx_peer_stats *rx_stats,
					   struct hal_rx_mon_ppdu_info *ppdu_info,
					   struct ath12k_dp_link_peer *peer, u32 uid)
{
	struct hal_rx_user_status *user_stats = NULL;
	bool is_su = true;

	if (!peer || !rx_stats || !ppdu_info)
		return;

	if (ppdu_info->reception_type != HAL_RX_RECEPTION_TYPE_SU) {
		user_stats = &ppdu_info->userstats[uid];
		is_su = false;
		if (!user_stats)
			return;
	}

	ath12k_dp_rx_fill_rate_info(&peer->rxrate, ppdu_info, user_stats, is_su);
}

void ath12k_dp_mon_rx_update_advance_stats(struct ath12k_rx_peer_stats *rx_stats,
					   struct hal_rx_mon_ppdu_info *ppdu_info,
					   u32 num_msdu, u32 uid)
{
	struct hal_rx_user_status *user_stats = NULL;
	u8 preamble_type, mcs, nss, ac, punc_mode, max_mcs, res_mcs, mu_type;
	u32 byte_count, tid;

	if (!rx_stats || !ppdu_info || uid >= HAL_MAX_UL_MU_USERS)
		return;

	if (ppdu_info->reception_type != HAL_RX_RECEPTION_TYPE_SU)
		user_stats = &ppdu_info->userstats[uid];

	preamble_type = user_stats ? user_stats->preamble_type : ppdu_info->preamble_type;
	mcs = user_stats ? user_stats->mcs : ppdu_info->mcs;
	nss = user_stats ? user_stats->nss : ppdu_info->nss;
	byte_count = user_stats ? user_stats->mpdu_ok_byte_count : ppdu_info->mpdu_len;
	tid = user_stats ? user_stats->tid : ppdu_info->tid;
	ac = ath12k_tid_to_ac(tid);
	punc_mode = ppdu_info->punc_bw;

	if (preamble_type >= HAL_RX_PREAMBLE_MAX)
		return;

	if (tid <= IEEE80211_NUM_TIDS && ac < WME_NUM_AC) {
		rx_stats->wme_ac_type[ac].total_pkts += num_msdu;
		rx_stats->wme_ac_type[ac].total_bytes += byte_count;
	}

	if (punc_mode < MAX_PUNCTURED_MODE)
		rx_stats->punc_bw[punc_mode] += num_msdu;

	rx_stats->num_bar += ppdu_info->ctrl_frm_info[uid].bar;
	rx_stats->num_ndpa += ppdu_info->ctrl_frm_info[uid].ndpa;

	max_mcs = max_mcs_by_preamble[preamble_type];
	res_mcs = (mcs < max_mcs) ? mcs : (MAX_MCS - 1);

	rx_stats->proto_type[preamble_type].mcs_count[res_mcs] += num_msdu;

	if (ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_SU) {
		rx_stats->su_ppdu_count[preamble_type].mcs_count[res_mcs] += 1;
		if (likely(nss) && (nss - 1) < HAL_RX_MAX_NSS)
			rx_stats->ppdu_nss[nss - 1] += 1;
	} else {
		if (!user_stats)
			return;

		/* Assumes any non-SU and non-MU-MIMO reception is MU-OFDMA.
		 * Update if new reception types are introduced.
		 */
		mu_type = (ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_MIMO) ?
			TXRX_TYPE_MU_MIMO : TXRX_TYPE_MU_OFDMA;

		rx_stats->rx_mu[preamble_type][mu_type].mpdu_cnt_fcs_ok +=
							user_stats->mpdu_cnt_fcs_ok;
		rx_stats->rx_mu[preamble_type][mu_type].ppdu.mcs_count[res_mcs] += 1;
		rx_stats->rx_mu[preamble_type][mu_type].mpdu_cnt_fcs_err +=
							user_stats->mpdu_cnt_fcs_err;
		if (likely(nss) && (nss - 1) < HAL_RX_MAX_NSS)
			rx_stats->rx_mu[preamble_type][mu_type].ppdu_nss[nss - 1] += 1;
	}
	rx_stats->ppdu_reception[ppdu_info->reception_type] += 1;

	if (mcs < MAX_MCS) {
		rx_stats->num_mpdu_count[mcs] += 1;
	} else {
		rx_stats->num_mpdu_count[MAX_MCS - 1] += 1;
	}
}

void ath12k_dp_mon_rx_update_basic_stats(struct ath12k_dp_link_peer *peer,
					 struct ath12k_rx_peer_stats *rx_stats,
					 struct hal_rx_mon_ppdu_info *ppdu_info,
					 u32 num_msdu, u32 uid)
{
	struct hal_rx_user_status *user_stats = NULL;
	u32 ru_width_factor, byte_count;
	u64 rx_duration_scaled;
	u16 rx_time_us, num_msdu_retry_count;
	u8 preamble_type, mcs, nss;

	if (!rx_stats || !ppdu_info)
		return;

	if (ppdu_info->reception_type != HAL_RX_RECEPTION_TYPE_SU)
		user_stats = &ppdu_info->userstats[uid];

	preamble_type = user_stats ? user_stats->preamble_type : ppdu_info->preamble_type;
	mcs = user_stats ? user_stats->mcs : ppdu_info->mcs;
	nss = user_stats ? user_stats->nss : ppdu_info->nss;
	byte_count = user_stats ? user_stats->mpdu_ok_byte_count : ppdu_info->mpdu_len;
	num_msdu_retry_count = user_stats ? user_stats->retried_msdu_count :
			       ppdu_info->retried_msdu_count;

	rx_stats->num_ppdus += 1;
	rx_stats->num_mpdu_retry_count += ppdu_info->mpdu_retry_cnt;
	rx_stats->num_msdu_bytes += byte_count;
	rx_stats->num_msdu_retry_count += num_msdu_retry_count;
	rx_stats->bw_info = ppdu_info->bw;
	rx_stats->gi_info = ppdu_info->gi;
	rx_stats->mcs_info = mcs;
	rx_stats->nss_info = nss;
	rx_stats->preamble_info = ppdu_info->preamble_type;
	if (ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_SU) {
		rx_time_us = ppdu_info->rx_duration;
		rx_stats->num_mpdus += ppdu_info->num_mpdu_fcs_ok +
				       ppdu_info->num_mpdu_fcs_err;
	} else {
		/* MU */
		ru_width_factor = ppdu_info->usr_nss_sum * ppdu_info->usr_ru_tones_sum;
		if (!ru_width_factor)
			ru_width_factor = 1;

		rx_duration_scaled = ppdu_info->rx_duration * user_stats->nss *
				     user_stats->ul_ofdma_ru_width;
		rx_time_us = (u16)div_u64(rx_duration_scaled, ru_width_factor);
		rx_stats->num_mpdus += user_stats->mpdu_cnt_fcs_ok +
				       user_stats->mpdu_cnt_fcs_err;
	}
	rx_stats->num_ppdu_duration += rx_time_us;

	ath12k_dp_rx_rate_stats_update(rx_stats, ppdu_info, peer, uid);
}

void ath12k_dp_mon_rx_update_peer_su_stats(struct ath12k_pdev_dp *pdev_dp,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_rx_peer_stats *rx_stats;
	u32 num_msdu;

	peer = ath12k_dp_link_peer_find_by_peerid_index(pdev_dp->dp, pdev_dp,
							ppdu_info->peer_id);
	if (!peer) {
		ath12k_dbg(pdev_dp->ar->ab, ATH12K_DBG_DATA,
			   "failed to find the peer with monitor peer_id %d\n",
			   ppdu_info->peer_id);
		return;
	}

	rx_stats = peer->peer_stats.rx_stats;
	peer->rssi_comb = ppdu_info->rssi_comb;
	ewma_avg_rssi_add(&peer->avg_rssi, ppdu_info->rssi_comb);

	if (!ath12k_extd_rx_stats_enabled(pdev_dp->ar) || !rx_stats)
		return;

	peer->peer_stats.rx_retries += ppdu_info->mpdu_retry;
	num_msdu = ppdu_info->tcp_msdu_count + ppdu_info->tcp_ack_msdu_count +
		   ppdu_info->udp_msdu_count + ppdu_info->other_msdu_count;

	rx_stats->num_msdu += num_msdu;
	rx_stats->tcp_msdu_count += ppdu_info->tcp_msdu_count +
				    ppdu_info->tcp_ack_msdu_count;
	rx_stats->udp_msdu_count += ppdu_info->udp_msdu_count;
	rx_stats->other_msdu_count += ppdu_info->other_msdu_count;

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11A ||
	    ppdu_info->preamble_type == HAL_RX_PREAMBLE_11B) {
		ppdu_info->nss = 1;
		ppdu_info->mcs = HAL_RX_MAX_MCS;
		ppdu_info->tid = IEEE80211_NUM_TIDS;
	}

	if (ppdu_info->ldpc < HAL_RX_SU_MU_CODING_MAX)
		rx_stats->coding_count[ppdu_info->ldpc] += num_msdu;

	if (ppdu_info->tid <= IEEE80211_NUM_TIDS)
		rx_stats->tid_count[ppdu_info->tid] += num_msdu;

	if (ppdu_info->preamble_type < HAL_RX_PREAMBLE_MAX)
		rx_stats->pream_cnt[ppdu_info->preamble_type] += num_msdu;

	if (ppdu_info->reception_type < HAL_RX_RECEPTION_TYPE_MAX)
		rx_stats->reception_type[ppdu_info->reception_type] += num_msdu;

	if (ppdu_info->is_stbc)
		rx_stats->stbc_count += num_msdu;

	if (ppdu_info->beamformed)
		rx_stats->beamformed_count += num_msdu;

	if (ppdu_info->num_mpdu_fcs_ok > 1)
		rx_stats->ampdu_msdu_count += num_msdu;
	else
		rx_stats->non_ampdu_msdu_count += num_msdu;

	rx_stats->num_mpdu_fcs_ok += ppdu_info->num_mpdu_fcs_ok;
	rx_stats->num_mpdu_fcs_err += ppdu_info->num_mpdu_fcs_err;
	rx_stats->dcm_count += ppdu_info->dcm;

	rx_stats->rx_duration += ppdu_info->rx_duration;
	peer->rx_duration = rx_stats->rx_duration;

	if (ppdu_info->nss > 0 && ppdu_info->nss <= HAL_RX_MAX_NSS) {
		rx_stats->pkt_stats.nss_count[ppdu_info->nss - 1] += num_msdu;
		rx_stats->byte_stats.nss_count[ppdu_info->nss - 1] += ppdu_info->mpdu_len;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11N &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_HT) {
		rx_stats->pkt_stats.ht_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.ht_mcs_count[ppdu_info->mcs] += ppdu_info->mpdu_len;
		/* To fit into rate table for HT packets */
		ppdu_info->mcs = ppdu_info->mcs % 8;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11AC &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_VHT) {
		rx_stats->pkt_stats.vht_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.vht_mcs_count[ppdu_info->mcs] += ppdu_info->mpdu_len;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11AX &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_HE) {
		rx_stats->pkt_stats.he_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.he_mcs_count[ppdu_info->mcs] += ppdu_info->mpdu_len;
	}

	if (ppdu_info->preamble_type == HAL_RX_PREAMBLE_11BE &&
	    ppdu_info->mcs <= HAL_RX_MAX_MCS_BE) {
		rx_stats->pkt_stats.be_mcs_count[ppdu_info->mcs] += num_msdu;
		rx_stats->byte_stats.be_mcs_count[ppdu_info->mcs] += ppdu_info->mpdu_len;
	}

	if ((ppdu_info->preamble_type == HAL_RX_PREAMBLE_11A ||
	     ppdu_info->preamble_type == HAL_RX_PREAMBLE_11B) &&
	     ppdu_info->rate < HAL_RX_LEGACY_RATE_INVALID) {
		rx_stats->pkt_stats.legacy_count[ppdu_info->rate] += num_msdu;
		rx_stats->byte_stats.legacy_count[ppdu_info->rate] += ppdu_info->mpdu_len;
	}

	if (ppdu_info->gi < HAL_RX_GI_MAX) {
		rx_stats->pkt_stats.gi_count[ppdu_info->gi] += num_msdu;
		rx_stats->byte_stats.gi_count[ppdu_info->gi] += ppdu_info->mpdu_len;
	}

	if (ppdu_info->bw < HAL_RX_BW_MAX) {
		rx_stats->pkt_stats.bw_count[ppdu_info->bw] += num_msdu;
		rx_stats->byte_stats.bw_count[ppdu_info->bw] += ppdu_info->mpdu_len;
	}

	ath12k_dp_mon_rx_update_peer_rate_table_stats(rx_stats, ppdu_info,
						      NULL, num_msdu);

	ath12k_dp_mon_rx_update_basic_stats(peer, rx_stats, ppdu_info, num_msdu, 0);
	/* Update Advance stats */
	if (ath12k_dp_stats_enabled(pdev_dp) &&
	    ath12k_dp_advance_stats_enabled(pdev_dp)) {
		ath12k_dp_mon_rx_update_advance_stats(rx_stats, ppdu_info, num_msdu, 0);
		ath12k_dp_rx_update_rate_stats(rx_stats, &peer->rxrate);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_update_peer_su_stats);

void ath12k_dp_mon_rx_process_ulofdma_stats(struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct hal_rx_user_status *rx_user_status;
	u32 num_users, i, mu_ul_user_v0_word0, mu_ul_user_v0_word1, ru_size;
	u32 ru_width;

	if (!(ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_MIMO ||
	      ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA ||
	      ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA_MIMO))
		return;

	num_users = ppdu_info->num_users;
	if (num_users > HAL_MAX_UL_MU_USERS)
		num_users = HAL_MAX_UL_MU_USERS;

	for (i = 0; i < num_users; i++) {
		rx_user_status = &ppdu_info->userstats[i];
		mu_ul_user_v0_word0 =
			rx_user_status->ul_ofdma_user_v0_word0;
		mu_ul_user_v0_word1 =
			rx_user_status->ul_ofdma_user_v0_word1;

		if (u32_get_bits(mu_ul_user_v0_word0,
				 HAL_RX_UL_OFDMA_USER_INFO_V0_W0_VALID) &&
		    !u32_get_bits(mu_ul_user_v0_word0,
				  HAL_RX_UL_OFDMA_USER_INFO_V0_W0_VER)) {
			rx_user_status->mcs =
				u32_get_bits(mu_ul_user_v0_word1,
					     HAL_RX_UL_OFDMA_USER_INFO_V0_W1_MCS);
			rx_user_status->nss =
				u32_get_bits(mu_ul_user_v0_word1,
					     HAL_RX_UL_OFDMA_USER_INFO_V0_W1_NSS) + 1;

			ppdu_info->usr_nss_sum += rx_user_status->nss;
			rx_user_status->ofdma_info_valid = 1;
			rx_user_status->ul_ofdma_ru_start_index =
				u32_get_bits(mu_ul_user_v0_word1,
					     HAL_RX_UL_OFDMA_USER_INFO_V0_W1_RU_START);

			ru_size = u32_get_bits(mu_ul_user_v0_word1,
					       HAL_RX_UL_OFDMA_USER_INFO_V0_W1_RU_SIZE);
			rx_user_status->ul_ofdma_ru_width = ru_size;
			ru_width = ath12k_dp_mon_rx_ul_ofdma_ru_size_to_width(ru_size);
			rx_user_status->ul_ofdma_ru_width = ru_width;
			rx_user_status->ul_ofdma_ru_size = ru_size;
		}
		rx_user_status->ldpc = u32_get_bits(mu_ul_user_v0_word1,
						    HAL_RX_UL_OFDMA_USER_INFO_V0_W1_LDPC);
	}
	ppdu_info->ldpc = 1;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_process_ulofdma_stats);

static void
ath12k_dp_mon_rx_update_user_stats(struct ath12k_pdev_dp *pdev_dp,
				   struct hal_rx_mon_ppdu_info *ppdu_info,
				   u32 uid)
{
	struct ath12k_rx_peer_stats *rx_stats = NULL;
	struct hal_rx_user_status *user_stats = &ppdu_info->userstats[uid];
	struct ath12k_pdev_dp_stats *pdev_stats = &pdev_dp->stats;
	struct ath12k_dp_link_peer *peer;
	u32 num_msdu;
	struct ath12k_dp *dp = pdev_dp->dp;
	struct ath12k_base *ab = dp->ab;

	if (ppdu_info->peer_id == HAL_INVALID_PEERID)
		return;

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp, pdev_dp,
							user_stats->sw_peer_id);
	if (!peer) {
		ath12k_dbg(ab, ATH12K_DBG_DP_MON, "peer with peer id %d can't be found\n",
			   ppdu_info->peer_id);
		return;
	}

	peer->peer_stats.rx_retries = user_stats->mpdu_retry;

	if (!ath12k_extd_rx_stats_enabled(pdev_dp->ar))
		return;

	rx_stats = peer->peer_stats.rx_stats;
	if (!rx_stats)
		return;

	ppdu_info->usr_nss_sum += user_stats->nss;
	ppdu_info->usr_ru_tones_sum += user_stats->ul_ofdma_ru_width;

	peer->rssi_comb = ppdu_info->rssi_comb;
	ewma_avg_rssi_add(&peer->avg_rssi, ppdu_info->rssi_comb);

	num_msdu = user_stats->tcp_msdu_count + user_stats->tcp_ack_msdu_count +
		   user_stats->udp_msdu_count + user_stats->other_msdu_count;

	rx_stats->num_msdu += num_msdu;
	rx_stats->tcp_msdu_count += user_stats->tcp_msdu_count +
				    user_stats->tcp_ack_msdu_count;
	rx_stats->udp_msdu_count += user_stats->udp_msdu_count;
	rx_stats->other_msdu_count += user_stats->other_msdu_count;

	if (ppdu_info->ldpc < HAL_RX_SU_MU_CODING_MAX)
		rx_stats->coding_count[ppdu_info->ldpc] += num_msdu;

	if (user_stats->tid <= IEEE80211_NUM_TIDS)
		rx_stats->tid_count[user_stats->tid] += num_msdu;

	if (user_stats->preamble_type < HAL_RX_PREAMBLE_MAX)
		rx_stats->pream_cnt[user_stats->preamble_type] += num_msdu;

	if (ppdu_info->reception_type < HAL_RX_RECEPTION_TYPE_MAX)
		rx_stats->reception_type[ppdu_info->reception_type] += num_msdu;

	if (ppdu_info->is_stbc)
		rx_stats->stbc_count += num_msdu;

	if (ppdu_info->beamformed)
		rx_stats->beamformed_count += num_msdu;

	if (user_stats->mpdu_cnt_fcs_ok > 1)
		rx_stats->ampdu_msdu_count += num_msdu;
	else
		rx_stats->non_ampdu_msdu_count += num_msdu;

	rx_stats->num_mpdu_fcs_ok += user_stats->mpdu_cnt_fcs_ok;
	rx_stats->num_mpdu_fcs_err += user_stats->mpdu_cnt_fcs_err;
	rx_stats->dcm_count += ppdu_info->dcm;
	if (ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA ||
	    ppdu_info->reception_type == HAL_RX_RECEPTION_TYPE_MU_OFDMA_MIMO)
		rx_stats->ru_alloc_cnt[user_stats->ul_ofdma_ru_size] += num_msdu;

	rx_stats->rx_duration += ppdu_info->rx_duration;
	peer->rx_duration = rx_stats->rx_duration;

	if (user_stats->nss > 0 && user_stats->nss <= HAL_RX_MAX_NSS) {
		rx_stats->pkt_stats.nss_count[user_stats->nss - 1] += num_msdu;
		rx_stats->byte_stats.nss_count[user_stats->nss - 1] +=
						user_stats->mpdu_ok_byte_count;
	}

	if (user_stats->preamble_type == HAL_RX_PREAMBLE_11AX &&
	    user_stats->mcs <= HAL_RX_MAX_MCS_HE) {
		rx_stats->pkt_stats.he_mcs_count[user_stats->mcs] += num_msdu;
		rx_stats->byte_stats.he_mcs_count[user_stats->mcs] +=
						user_stats->mpdu_ok_byte_count;
	}

	if (ppdu_info->gi < HAL_RX_GI_MAX) {
		rx_stats->pkt_stats.gi_count[ppdu_info->gi] += num_msdu;
		rx_stats->byte_stats.gi_count[ppdu_info->gi] +=
						user_stats->mpdu_ok_byte_count;
	}

	if (ppdu_info->bw < HAL_RX_BW_MAX) {
		rx_stats->pkt_stats.bw_count[ppdu_info->bw] += num_msdu;
		rx_stats->byte_stats.bw_count[ppdu_info->bw] +=
						user_stats->mpdu_ok_byte_count;
	}

	ath12k_dp_mon_rx_update_peer_rate_table_stats(rx_stats, ppdu_info,
						      user_stats, num_msdu);

	pdev_stats->telemetry_stats.rx_data_msdu_cnt = rx_stats->num_msdu;
	pdev_stats->telemetry_stats.total_rx_data_bytes = user_stats->mpdu_ok_byte_count;

	ath12k_dp_mon_rx_update_basic_stats(peer, rx_stats, ppdu_info, num_msdu, uid);
	/* Update Advance stats */
	if (ath12k_dp_stats_enabled(pdev_dp) &&
	    ath12k_dp_advance_stats_enabled(pdev_dp)) {
		ath12k_dp_mon_rx_update_advance_stats(rx_stats, ppdu_info, num_msdu, uid);
		ath12k_dp_rx_update_rate_stats(rx_stats, &peer->rxrate);
	}
}

void
ath12k_dp_mon_rx_update_peer_mu_stats(struct ath12k_pdev_dp *pdev_dp,
				      struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 num_users, i;

	if (!ath12k_extd_rx_stats_enabled(pdev_dp->ar))
		return;

	num_users = ppdu_info->num_users;
	if (num_users > HAL_MAX_UL_MU_USERS)
		num_users = HAL_MAX_UL_MU_USERS;

	for (i = 0; i < num_users; i++)
		ath12k_dp_mon_rx_update_user_stats(pdev_dp, ppdu_info, i);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_update_peer_mu_stats);

static void
ath12k_dp_mon_ppdu_per_user_rx_time_update(struct ath12k_pdev_dp *dp_pdev,
                                          struct hal_rx_mon_ppdu_info *ppdu_info,
                                          u32 uid)
{
       struct hal_rx_user_status *user_stats = &ppdu_info->userstats[uid];
       struct ath12k_dp_link_peer_stats *stats = NULL;
       struct ath12k_dp_link_peer *peer;
       u32 nss_ru_width_sum = 0;
       u64 temp_result = 0;
       u16 rx_time_us = 0;
       u8 ac = 0;

       if (!dp_pdev)
               return;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(), "PPDU per user rx time update called without rcu lock\n");
	lockdep_assert_held(&dp_pdev->dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
							user_stats->sw_peer_id);
       if (!peer || !peer->sta) {
               ath12k_dbg(dp_pdev->ar->ab, ATH12K_DBG_PEER,
                          "peer stats not found on ppdu peer id %d\n",
                          user_stats->sw_peer_id);
               return;
       }

       nss_ru_width_sum = ppdu_info->usr_nss_sum * ppdu_info->usr_ru_tones_sum;
       if (!nss_ru_width_sum)
               nss_ru_width_sum = 1;

       if (ppdu_info->reception_type != HAL_RX_RECEPTION_TYPE_SU) {
               temp_result = ppdu_info->rx_duration * user_stats->nss *
                             user_stats->ul_ofdma_ru_width;
               rx_time_us = (u16)div_u64(temp_result, nss_ru_width_sum);
       } else
               rx_time_us = ppdu_info->rx_duration;

       ac = ath12k_tid_to_ac(ppdu_info->tid);
       stats = &peer->peer_stats;
       stats->dp_mon_stats.mon_stats.rx_airtime_consumption[ac].consumption += rx_time_us;
       ath12k_dbg(dp_pdev->ar->ab, ATH12K_DBG_DP_HTT, "peer: %pM tid: %d ac: %d sum nss: %d tones: %d per user nss: %d tone: %d time: %d cons: %d\n",
                  peer->addr,
                  ppdu_info->tid, ac,
                  ppdu_info->usr_nss_sum, ppdu_info->usr_ru_tones_sum,
                  user_stats->nss, user_stats->ul_ofdma_ru_width,
                  rx_time_us,
                  stats->dp_mon_stats.mon_stats.rx_airtime_consumption[ac].consumption);
}

#define RSSI_OFFSET 100
static void
ath12k_dp_calc_rx_peer_rssi(struct ath12k_pdev_dp *dp_pdev,
			    struct ath12k_dp_link_peer *link_peer)
{
	struct ath12k *ar = dp_pdev->ar;
	struct ath12k_dp_link_peer_rx_signal_stats *stats;
	s8 rssi, rssi_dp;

	if (!ar || !link_peer)
		return;

	stats = &link_peer->signal_stats;
	rssi = ath12k_dp_get_rssi_value(stats->snr, stats, &ar->rssi_offsets,
					link_peer, false);
	stats->rssi = rssi;
	rssi_dp = ath12k_dp_get_rssi_value(stats->snr_dp, stats,
					   &ar->rssi_offsets, link_peer, false);
	stats->rssi_dp = rssi_dp;
	ewma_avg_rssi_add(&stats->avg_rssi, (stats->rssi + RSSI_OFFSET) << 8);
	stats->rssi_avg =
		(ewma_avg_rssi_read(&stats->avg_rssi) >> 8) - RSSI_OFFSET;
	ewma_avg_rssi_dp_add(&stats->avg_rssi_dp, (stats->rssi_dp + RSSI_OFFSET) << 8);
	stats->rssi_dp_avg =
		(ewma_avg_rssi_dp_read(&stats->avg_rssi_dp) >> 8) - RSSI_OFFSET;
}

static void
ath12k_dp_mon_link_peer_signal_stats(struct ath12k_pdev_dp *dp_pdev,
				     struct hal_rx_mon_ppdu_info *ppdu_info,
				     u32 uid)
{
	struct hal_rx_user_status *user_stats = &ppdu_info->userstats[uid];
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_link_peer_rx_signal_stats *stats = NULL;

	if (!dp_pdev)
		return;

	lockdep_assert_held(&dp_pdev->dp->dp_lock);
	rcu_read_lock();
	peer = ath12k_dp_link_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
							user_stats->sw_peer_id);
	if (!peer) {
		ath12k_dbg(dp_pdev->ar->ab, ATH12K_DBG_PEER,
			   "peer stats not found on ppdu peer id %d\n",
			   user_stats->sw_peer_id);
		rcu_read_unlock();
		return;
	}

	stats = &peer->signal_stats;
	stats->snr = ppdu_info->rssi_comb;
	stats->rssi_region_offset = ppdu_info->rssi_region_offset;
	ewma_avg_snr_add(&stats->avg_snr, stats->snr);
	stats->snr_avg = ewma_avg_snr_read(&stats->avg_snr);

	if (likely(ppdu_info->fc_valid)) {
		switch (ppdu_info->frame_control & 0x00F0) {
		case IEEE80211_STYPE_DATA:
		case IEEE80211_STYPE_DATA_CFACK:
		case IEEE80211_STYPE_DATA_CFPOLL:
		case IEEE80211_STYPE_DATA_CFACKPOLL:
		case IEEE80211_STYPE_QOS_DATA:
		case IEEE80211_STYPE_QOS_DATA_CFACK:
		case IEEE80211_STYPE_QOS_DATA_CFPOLL:
		case IEEE80211_STYPE_QOS_DATA_CFACKPOLL:
			if ((ppdu_info->preamble_type != HAL_RX_PREAMBLE_11A &&
			     ppdu_info->preamble_type != HAL_RX_PREAMBLE_11B)) {
				stats->snr_dp = ppdu_info->rssi_comb;
				ewma_avg_snr_dp_add(&stats->avg_snr_dp, stats->snr_dp);
				stats->snr_dp_avg =
					ewma_avg_snr_dp_read(&stats->avg_snr_dp);
			}
			break;
		default:
			break;
		}
	}
	ath12k_dp_calc_rx_peer_rssi(dp_pdev, peer);

	if (peer->peer_stats.rx_stats &&
	    IS_VALID_RATE(peer->peer_stats.rx_stats->last_rx_rate) &&
	    IS_VALID_RSSI(stats->rssi)) {
		u32 last_rx_rate = peer->peer_stats.rx_stats->last_rx_rate;
		u8 soc_id = ath12k_get_ab_device_id(dp_pdev->ar->ab);

		ath12k_telemetry_update_rssi_rate_breach(soc_id,
							 peer->peer_id,
							 peer->addr,
							 PATH_TYPE_RX,
							 stats->rssi,
							 last_rx_rate);
	}

	rcu_read_unlock();
}

static void
ath12k_dp_mon_per_user_ppdu_rssi_update(struct ath12k_pdev_dp *dp_pdev,
					struct hal_rx_mon_ppdu_info *ppdu_info,
					u32 uid)
{
	struct hal_rx_user_status *user_stats = &ppdu_info->userstats[uid];
	struct ath12k_dp_mon_peer_stats *stats = NULL;
	struct ath12k_dp_link_peer *peer;
	u8 rssi_comb;

	if (!dp_pdev)
		return;

	lockdep_assert_held(&dp_pdev->dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
							user_stats->sw_peer_id);
	if (!peer || !peer->sta) {
		ath12k_dbg(dp_pdev->ar->ab, ATH12K_DBG_PEER,
			   "peer stats not found on ppdu peer id %d\n",
			   user_stats->sw_peer_id);
		return;
	}

	rssi_comb = ppdu_info->rssi_comb;
	stats = &peer->peer_stats.dp_mon_stats;
	stats->snr = rssi_comb;
	if (unlikely(stats->avg_snr == SNR_INVALID))
		stats->avg_snr = WEIGHTED_AVG_IN(stats->snr);
	else
		WEIGHTED_AVG_UPDATE(stats->avg_snr, stats->snr);
}

void ath12k_dp_mon_ppdu_rx_time_update(struct ath12k_pdev_dp *dp_pdev,
				       struct hal_rx_mon_ppdu_info *ppdu_info,
				       bool is_stat)
{
       u32 num_users, uid;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(), "PPDU rx time update called without rcu lock\n");
	lockdep_assert_held(&dp_pdev->dp->dp_lock);

       num_users = ppdu_info->num_users;
       if (num_users > HAL_MAX_UL_MU_USERS)
               num_users = HAL_MAX_UL_MU_USERS;

       for (uid = 0; uid < num_users; uid++)
               ath12k_dp_mon_ppdu_per_user_rx_time_update(dp_pdev, ppdu_info, uid);
}
EXPORT_SYMBOL(ath12k_dp_mon_ppdu_rx_time_update);

void ath12k_dp_mon_ppdu_rssi_update(struct ath12k_pdev_dp *dp_pdev,
				    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 num_users, uid;

	num_users = ppdu_info->num_users;
	if (num_users > HAL_MAX_UL_MU_USERS)
		num_users = HAL_MAX_UL_MU_USERS;

	for (uid = 0; uid < num_users; uid++) {
		ath12k_dp_mon_per_user_ppdu_rssi_update(dp_pdev, ppdu_info, uid);
		ath12k_dp_mon_link_peer_signal_stats(dp_pdev, ppdu_info, uid);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_ppdu_rssi_update);

void ath12k_dp_rxdma_mon_buf_ring_free(struct ath12k_dp *dp,
				       struct dp_rxdma_mon_ring *rx_ring)
{
	struct ath12k_base *ab = dp->ab;
	struct sk_buff *skb;
	int buf_id;

	spin_lock_bh(&rx_ring->idr_lock);
	idr_for_each_entry(&rx_ring->bufs_idr, skb, buf_id) {
		idr_remove(&rx_ring->bufs_idr, buf_id);
		/* TODO: Understand where internal driver does this dma_unmap
		 * of rxdma_buffer.
		 */
		dma_unmap_single(ab->dev, ATH12K_SKB_RXCB(skb)->paddr,
				 skb->len + skb_tailroom(skb), DMA_FROM_DEVICE);
		dev_kfree_skb_any(skb);
	}

	idr_destroy(&rx_ring->bufs_idr);
	spin_unlock_bh(&rx_ring->idr_lock);
}
EXPORT_SYMBOL(ath12k_dp_rxdma_mon_buf_ring_free);

int ath12k_dp_mon_rx_srng_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	int ret;

	ret = ath12k_dp_srng_setup(ab,
				   &dp_mon->rxdma_mon_buf_ring.refill_buf_ring,
				   HAL_RXDMA_MONITOR_BUF, 0, 0,
				   DP_RXDMA_MONITOR_BUF_RING_SIZE);
	if (ret) {
		ath12k_warn(dp, "failed to setup HAL_RXDMA_MONITOR_BUF %d\n",
			    ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_srng_setup);

void ath12k_dp_mon_rx_srng_cleanup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_srng *srng;

	srng = &dp_mon->rxdma_mon_buf_ring.refill_buf_ring;

	ath12k_dp_srng_cleanup(ab, srng);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_srng_cleanup);

int ath12k_dp_mon_rx_buf_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_rxdma_mon_ring *rx_ring;
	LIST_HEAD(list);
	size_t req_entries;
	int num_entries, ret = -EINVAL, i;

	INIT_LIST_HEAD(&dp_mon->mon_desc_free_list);
	spin_lock_init(&dp_mon->mon_desc_lock);

	spin_lock_bh(&dp_mon->mon_desc_lock);
	dp_mon->mon_desc_pool = kcalloc(DP_RXDMA_MONITOR_BUF_RING_SIZE,
					sizeof(*dp_mon->mon_desc_pool),
					GFP_ATOMIC);
	if (!dp_mon->mon_desc_pool) {
		spin_unlock_bh(&dp_mon->mon_desc_lock);
		ath12k_warn(dp, "failed to allocate memory for mon desc pool\n");
		ret = -ENOMEM;
		return ret;
	}

	for (i = 0; i < DP_RXDMA_MONITOR_BUF_RING_SIZE; i++) {
		dp_mon->mon_desc_pool[i].magic = ATH12K_MON_MAGIC_VALUE;
		INIT_LIST_HEAD(&dp_mon->mon_desc_pool[i].list);
		list_add_tail(&dp_mon->mon_desc_pool[i].list,
			      &dp_mon->mon_desc_free_list);
	}

	spin_unlock_bh(&dp_mon->mon_desc_lock);

	rx_ring = &dp_mon->rxdma_mon_buf_ring;

	num_entries =  rx_ring->refill_buf_ring.size /
		ath12k_hal_srng_get_entrysize(ab, HAL_RXDMA_MONITOR_BUF);
	rx_ring->bufs_max = num_entries;

	req_entries = ath12k_dp_mon_get_req_entries_from_buf_ring(dp, rx_ring, &list);
	if (req_entries)
		ret = ath12k_dp_mon_buf_replenish(dp, rx_ring, &list, req_entries);
	else
		ath12k_warn(dp, "No required entries available for mon buf ring\n");

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_buf_setup);

void ath12k_dp_mon_rx_buf_free(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	int i;
	u8 *mon_buf;

	spin_lock_bh(&dp_mon->mon_desc_lock);
	if (!dp_mon->mon_desc_pool) {
		spin_unlock_bh(&dp_mon->mon_desc_lock);
		return;
	}

	for (i = 0; i < DP_RXDMA_MONITOR_BUF_RING_SIZE; i++) {
		if (dp_mon->mon_desc_pool[i].in_use != DP_MON_DESC_TO_HW)
			continue;

		mon_buf = dp_mon->mon_desc_pool[i].mon_buf;
		if (!mon_buf)
			goto reset_mon_desc;

		ath12k_core_dma_unmap_page(dp->dev, dp_mon->mon_desc_pool[i].paddr,
					   ATH12K_DP_MON_RX_BUF_SIZE, DMA_FROM_DEVICE);
		page_frag_free(mon_buf);
		dp_mon->mon_desc_pool[i].mon_buf = NULL;
		dp_mon->num_frag_free++;

reset_mon_desc:
		ath12k_dp_mon_desc_reset(&dp_mon->mon_desc_pool[i]);
	}

	kfree(dp_mon->mon_desc_pool);
	dp_mon->mon_desc_pool = NULL;
	spin_unlock_bh(&dp_mon->mon_desc_lock);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_buf_free);

int ath12k_dp_mon_rx_htt_srng_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	u32 ring_id;
	int ret;

	ring_id = dp_mon->rxdma_mon_buf_ring.refill_buf_ring.ring_id;
	ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id,
					  0, HAL_RXDMA_MONITOR_BUF);
	if (ret) {
		ath12k_warn(ab, "failed to configure rxdma_mon_buf_ring %d\n",
			    ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_htt_srng_setup);

int ath12k_dp_mon_pdev_rx_srng_setup(struct ath12k_pdev_dp *dp_pdev,
				     u32 mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	int i;
	int ret;

	for (i = 0; i < dp->hw_params->num_rxdma_per_pdev; i++) {
		ret = ath12k_dp_srng_setup(dp->ab,
					   &dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[i],
					   HAL_RXDMA_MONITOR_DST,
					   0, mac_id + i,
					   DP_RXDMA_MONITOR_DST_RING_SIZE);
		if (ret) {
			ath12k_warn(dp->ab,
				    "failed to setup HAL_RXDMA_MONITOR_DST\n");
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_srng_setup);

int ath12k_dp_mon_pdev_rx_htt_srng_setup(struct ath12k_pdev_dp *dp_pdev,
					 u32 mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	u32 ring_id;
	int i;
	int ret;

	for (i = 0; i < dp->hw_params->num_rxdma_per_pdev; i++) {
		ring_id = dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[i].ring_id;
		ret = ath12k_dp_tx_htt_srng_setup(dp->ab, ring_id,
						  mac_id + i,
						  HAL_RXDMA_MONITOR_DST);
		if (ret) {
			ath12k_warn(dp->ab,
				    "failed to configure rxdma_mon_dst_ring %d %d\n",
				    i, ret);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_htt_srng_setup);

void ath12k_dp_mon_pdev_rx_mpdu_list_init(struct ath12k_mon_data *pmon)
{
	INIT_LIST_HEAD(&pmon->dp_rx_mon_mpdu_list);
	pmon->mon_mpdu = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_mpdu_list_init);

void ath12k_dp_mon_pdev_rx_srng_cleanup(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	int i;

	for (i = 0; i < dp->hw_params->num_rxdma_per_pdev; i++)
		ath12k_dp_srng_cleanup(dp->ab,
				       &dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[i]);
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_srng_cleanup);

int ath12k_dp_mon_init(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon;

	dp_mon = kzalloc(sizeof(*dp_mon), GFP_KERNEL);
	if (!dp_mon)
		return -ENOMEM;

	dp_mon->dp = dp;
	dp->dp_mon = dp_mon;

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_init);

void ath12k_dp_mon_deinit(struct ath12k_dp *dp)
{
	if (dp->dp_mon)
		kfree(dp->dp_mon);
	dp->dp_mon = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_deinit);

int ath12k_dp_mon_pdev_alloc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;

	dp_mon_pdev = kzalloc(sizeof(*dp_mon_pdev), GFP_KERNEL);
	if (!dp_mon_pdev)
		return -ENOMEM;

	dp_mon_pdev->dp_pdev = dp_pdev;
	dp_mon_pdev->dp_mon = dp_pdev->dp->dp_mon;
	dp_pdev->dp_mon_pdev = dp_mon_pdev;

	dp_mon_pdev->smart_mon_filter =
		ath12k_mac_get_cached_smart_mon_filter(dp_pdev);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_alloc);

void ath12k_dp_mon_pdev_free(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_mac_cache_smart_mon_filter(dp_pdev,
					  dp_pdev->dp_mon_pdev->smart_mon_filter);
	kfree(dp_pdev->dp_mon_pdev);
	dp_pdev->dp_mon_pdev = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_free);

void ath12k_dp_mon_pdev_rx_attach(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_mon_data *pmon = &dp_mon_pdev->mon_data;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	int i;

	skb_queue_head_init(&pmon->rx_status_q);

	pmon->mon_ppdu_status = DP_PPDU_STATUS_START;

	memset(&pmon->rx_mon_stats, 0,
	       sizeof(pmon->rx_mon_stats));

	pmon->mon_last_linkdesc_paddr = 0;
	pmon->mon_last_buf_cookie = DP_RX_DESC_COOKIE_MAX + 1;
	spin_lock_init(&pmon->mon_lock);

	mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);

	if (mon_ops && mon_ops->mon_pdev_rx_mpdu_list_init)
		mon_ops->mon_pdev_rx_mpdu_list_init(pmon);

	INIT_LIST_HEAD(&dp_mon_pdev->mon_desc_used_list);

	for (i = 0; i < HAL_MAX_UL_MU_USERS; i++)
		skb_queue_head_init(&ppdu_info->mpdu_q[i]);

	ppdu_info->peer_id = HAL_INVALID_PEERID;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_attach);

void ath12k_dp_mon_rx_stats_enable(struct ath12k_pdev_dp *dp_pdev,
				   enum dp_mon_stats_mode mode)
{
	struct ath12k *ar = dp_pdev->ar;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ath12k_extd_rx_stats_enabled(ar))
		mode = ATH12k_DP_MON_EXTD_STATS;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ar->ab->dev_flags))
		 mode = ATH12k_DP_MON_EXTD_STATS;
#endif
	ath12k_dp_mon_rx_stats_config_filter(dp_pdev, mode, true);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_stats_enable);

void ath12k_dp_mon_rx_stats_disable(struct ath12k_pdev_dp *dp_pdev,
				    enum dp_mon_stats_mode mode)
{
	struct ath12k *ar = dp_pdev->ar;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (mode == ATH12k_DP_MON_EXTD_STATS) {
		/* while disabling extd rx stats, Basic stats filters
		 * to be configured.
		 */
		ath12k_dp_mon_rx_stats_config_filter(dp_pdev, mode,
						     false);
		mode = ATH12k_DP_MON_BASIC_STATS;
		ath12k_dp_mon_rx_stats_config_filter(dp_pdev, mode,
						     true);
	} else {
		ath12k_dp_mon_rx_stats_config_filter(dp_pdev, mode,
						     false);
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_stats_disable);

void ath12k_dp_mon_rx_monitor_mode_set(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_mon_mode_config_filter(dp_pdev, true);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_monitor_mode_set);

void ath12k_dp_mon_rx_monitor_mode_reset(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_mon_mode_config_filter(dp_pdev, false);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_monitor_mode_reset);

void ath12k_dp_mon_rx_nrp_set(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_nrp_config_filter(dp_pdev, true);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_nrp_set);

void ath12k_dp_mon_rx_nrp_reset(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_nrp_config_filter(dp_pdev, false);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_nrp_reset);

void ath12k_dp_mon_rx_smart_mon_set(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_smart_mon_config_filter(dp_pdev, true);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_smart_mon_set);

void ath12k_dp_mon_rx_smart_mon_reset(struct ath12k_pdev_dp *dp_pdev)
{
	ath12k_dp_mon_rx_smart_mon_config_filter(dp_pdev, false);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_smart_mon_reset);

static void ath12k_dp_mon_clear_pdev_airtime_stats(struct ath12k *ar)
{
       struct ath12k_pdev_dp *pdev_dp = &ar->dp;
       u8 ac;

       for (ac = 0; ac < ATH12K_DP_WLAN_MAX_AC; ac++) {
               pdev_dp->stats.telemetry_stats.tx_link_airtime[ac] = 0;
               pdev_dp->stats.telemetry_stats.rx_link_airtime[ac] = 0;
       }
}

u64 ath12k_get_timestamp_in_us(void)
{
       struct timespec64 ts;

       ktime_get_ts64(&ts);

       return ((u64)ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
}

void ath12k_dp_mon_peer_update_telemetry_stats(struct ath12k_base *ab,
                                              struct ath12k_dp_link_peer *peer,
                                              struct ath12k_pdev *pdev)
{
       struct ath12k_mon_peer_airtime_stats *airtime_stats;
       struct peer_airtime_consumption *peer_consump;
       u64 current_time = ath12k_get_timestamp_in_us();
       u32 remainder, time_diff;
       u32 usage;
       u16 consump_per_sec;
       struct ath12k *ar = pdev->ar;
       struct ath12k_pdev_dp *dp = &ar->dp;
       struct ath12k_pdev_dp_stats *pdev_stats = &dp->stats;
       u8 ac;
	struct ath12k_atf_peer_airtime *atf_airtime;

       airtime_stats = &peer->peer_stats.dp_mon_stats.mon_stats;
       time_diff = (u32)(current_time - airtime_stats->last_update_time);
	atf_airtime = &peer->atf_peer_airtime;

       for (ac = 0; ac < ATH12K_DP_WLAN_MAX_AC; ac++) {
               /* *_link_airtime refers to the amount of time a peer spends
                * transmitting or receiving data over the medium
                * To calculate total pdev's airtime cosumption, then store
                * each peer airtime consumption in pdev telemetry link airtime
                */

               /* Tx Airtime Consumption */
               peer_consump = &airtime_stats->tx_airtime_consumption[ac];
		atf_airtime->tx_airtime_consumption[ac].consumption +=
			airtime_stats->tx_airtime_consumption[ac].consumption;
		atf_airtime->tx_airtime_consumption[ac].avg_consumption_per_sec +=
			airtime_stats->tx_airtime_consumption[ac].avg_consumption_per_sec;
               usage = peer_consump->consumption;
               consump_per_sec = (u8)div_u64((u64)(usage * 100), time_diff);
               div_u64_rem((u64)(usage * 100), time_diff, &remainder);
               if (remainder < time_diff / 2) {
                       if (remainder && consump_per_sec == 0)
                               consump_per_sec++;
               } else {
                       if (consump_per_sec < 100)
                               consump_per_sec++;
               }
               peer_consump->avg_consumption_per_sec = consump_per_sec;
               pdev_stats->telemetry_stats.tx_link_airtime[ac] += peer_consump->consumption;
	       pdev_stats->atf_airtime.tx_airtime_consumption[ac] += peer_consump->consumption;
               peer_consump->consumption = 0;

               /* Rx Airtime Consumption */
               peer_consump = &airtime_stats->rx_airtime_consumption[ac];
		atf_airtime->rx_airtime_consumption[ac].consumption +=
			airtime_stats->rx_airtime_consumption[ac].consumption;
		atf_airtime->rx_airtime_consumption[ac].avg_consumption_per_sec +=
			airtime_stats->rx_airtime_consumption[ac].avg_consumption_per_sec;
               usage = peer_consump->consumption;
               consump_per_sec = (u8)div_u64((u64)(usage * 100), time_diff);
               div_u64_rem((u64)(usage * 100), time_diff, &remainder);
               if (remainder < time_diff / 2) {
                       if (remainder && consump_per_sec == 0)
                               consump_per_sec++;
               } else {
                       if (consump_per_sec < 100)
                               consump_per_sec++;
               }
               peer_consump->avg_consumption_per_sec = consump_per_sec;
               pdev_stats->telemetry_stats.rx_link_airtime[ac] += peer_consump->consumption;
	       pdev_stats->atf_airtime.rx_airtime_consumption[ac] += peer_consump->consumption;
               peer_consump->consumption = 0;

               ath12k_dbg(ab,
                          ATH12K_DBG_DP_HTT,
                          "peer: %pM time diff: %d link air tx: %d rx: %d cons tx: %d rx: %d\n",
                          peer->addr, time_diff,
                          pdev_stats->telemetry_stats.tx_link_airtime[ac],
                          pdev_stats->telemetry_stats.rx_link_airtime[ac],
                          airtime_stats->tx_airtime_consumption[ac].avg_consumption_per_sec,
                          airtime_stats->rx_airtime_consumption[ac].avg_consumption_per_sec);
       }

       airtime_stats->last_update_time = current_time;
}

static inline void ath12k_pdev_dp_iterate_peer(struct ath12k_base *ab,
                                              struct ath12k_pdev *pdev,
                                              void (*iter)(struct ath12k_base *ab, struct ath12k_dp_link_peer *peer, struct ath12k_pdev *pdev))
{
       struct ath12k_dp_link_peer *peer, *tmp;
       struct ieee80211_sta *sta;
       struct ath12k *ar;

       if (!pdev || !pdev->ar)
               return;
       ar = pdev->ar;
       spin_lock_bh(&ab->dp->dp_lock);
       list_for_each_entry_safe(peer, tmp, &ab->dp->peers, list) {
               if (!peer->vif)
                       continue;

               sta = peer->sta;
               if (!sta)
                       continue;
               /* In a split PHY scenario, if a pdev-level event occurs,
                * halt the operation if the peer belongs to a different pdev
                * than the one that triggered the event.
                */
               if (peer->pdev_idx != ar->pdev_idx)
                       continue;

               iter(ab, peer, pdev);
       }
       spin_unlock_bh(&ab->dp->dp_lock);
}

int ath12k_dp_mon_pdev_update_telemetry_stats(struct ath12k_base *ab,
                                             const int pdev_id)
{
       struct ath12k_pdev *pdev;

       rcu_read_lock();
       pdev = rcu_dereference(ab->pdevs_active[pdev_id]);
       if (!pdev) {
               rcu_read_unlock();
               return -EINVAL;
       }

       if (pdev->ar)
               ath12k_dp_mon_clear_pdev_airtime_stats(pdev->ar);

       ath12k_pdev_dp_iterate_peer(ab, pdev,
                                   ath12k_dp_mon_peer_update_telemetry_stats);
       rcu_read_unlock();

       return 0;
}

static void ath12k_dp_mon_peer_telemetry_stats(const struct ath12k_dp_link_peer *peer,
                                              struct ath12k_peer_telemetry_stats *stats)
{
       const struct ath12k_dp_mon_peer_stats *dp_stats;
       u8 ac;

       dp_stats = &peer->peer_stats.dp_mon_stats;
       for (ac = 0; ac < ATH12K_DP_WLAN_MAX_AC; ac++) {
               stats->tx_airtime_consumption[ac] =
                       dp_stats->mon_stats.tx_airtime_consumption[ac].avg_consumption_per_sec;
               stats->rx_airtime_consumption[ac] =
                       dp_stats->mon_stats.rx_airtime_consumption[ac].avg_consumption_per_sec;
       }
	stats->snr = dp_stats->avg_snr;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_update_telemetry_stats);

int ath12k_dp_get_peer_telemetry_stats(struct ath12k_base *ab,
                                      const u8 *peer_addr,
                                      struct ath12k_peer_telemetry_stats *stats)
{
       struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	lockdep_assert_held(&dp->dp_lock);
       peer = ath12k_dp_link_peer_find_by_addr(dp, peer_addr);
       if (!peer) {
               ath12k_dbg(ab,
                          ATH12K_DBG_DP_HTT,
                          "Failed to find peer at addr: %pM\n", peer_addr);
               return -EINVAL;
       }

       ath12k_dp_mon_peer_telemetry_stats(peer, stats);

       return 0;
}

static inline struct sk_buff *ath12k_mon_get_last_skb_from_fraglist(struct sk_buff *skb)
{
	struct sk_buff *last_skb;

	for (last_skb = skb_shinfo(skb)->frag_list;
	     last_skb && last_skb->next;
	     last_skb = last_skb->next)
		;

	return last_skb;
}

struct sk_buff *
ath12k_dp_mon_get_skb_valid_frag(struct ath12k_dp *dp, struct sk_buff *skb)
{
	struct sk_buff *last_skb;
	u32 num_frags;

	if (unlikely(!skb)) {
		ath12k_warn(dp, "invalid skb, cannot retrieve valid skb\n");
		return NULL;
	}

	num_frags = skb_shinfo(skb)->nr_frags;
	if (likely(num_frags < MAX_SKB_FRAGS))
		return skb;

	if (unlikely(!skb_has_frag_list(skb)))
		return NULL;

	last_skb = ath12k_mon_get_last_skb_from_fraglist(skb);
	if (unlikely(!last_skb)) {
		ath12k_warn(dp, "frag_list present but no valid last skb found\n");
		return NULL;
	}

	num_frags = skb_shinfo(last_skb)->nr_frags;
	if (likely(num_frags < MAX_SKB_FRAGS))
		return last_skb;

	ath12k_dbg(dp->ab, ATH12K_DBG_DP_MON,
		   "no skb with available frag slots found in skb or frag_list\n");
	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_get_skb_valid_frag);

void ath12k_dp_mon_update_skb_len(struct sk_buff *skb_head, u32 frag_len)
{
	skb_head->data_len += frag_len;
	skb_head->len += frag_len;
}
EXPORT_SYMBOL(ath12k_dp_mon_update_skb_len);

void ath12k_dp_mon_append_skb(struct sk_buff *skb, struct sk_buff *tmp_skb)
{
	struct sk_buff *last_skb;

	if (unlikely(!skb_has_frag_list(skb))) {
		skb_shinfo(skb)->frag_list = tmp_skb;
	} else {
		last_skb = ath12k_mon_get_last_skb_from_fraglist(skb);
		if (last_skb)
			last_skb->next = tmp_skb;
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_append_skb);

void ath12k_dp_mon_skb_remove_frag(struct ath12k_dp *dp, struct sk_buff *skb,
				   u16 idx, u16 truesize)
{
	struct page *page;
	u16 frag_len;

	page = skb_frag_page(&skb_shinfo(skb)->frags[idx]);
	if (unlikely(!page))
		return;

	frag_len = ath12k_dp_mon_get_frag_size_by_idx(dp, skb, idx);
	put_page(page);
	skb->len -= frag_len;
	skb->data_len -= frag_len;
	skb->truesize -= truesize;
	skb_shinfo(skb)->nr_frags--;
}
EXPORT_SYMBOL(ath12k_dp_mon_skb_remove_frag);

void ath12k_dp_mon_add_rx_frag(struct sk_buff *skb, const void *mon_buf,
			       int offset, int frag_len, bool take_frag_ref)
{
	struct page *page = virt_to_head_page(mon_buf);
	int frag_offset = mon_buf - page_address(page);
	int nr_frags = skb_shinfo(skb)->nr_frags;

	skb_add_rx_frag(skb, nr_frags, page,
			(frag_offset + offset), frag_len,
			ATH12K_DP_MON_RX_BUF_SIZE);

	if (unlikely(take_frag_ref))
		skb_frag_ref(skb, nr_frags);
}
EXPORT_SYMBOL(ath12k_dp_mon_add_rx_frag);

void ath12k_dp_mon_rx_process_low_thres(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_rxdma_mon_ring *rx_ring;
	struct hal_srng *srng;
	int num_free, req_entries;
	LIST_HEAD(list);

	rx_ring = &dp_mon->rxdma_mon_buf_ring;
	srng = &dp->hal->srng_list[rx_ring->refill_buf_ring.ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	num_free = ath12k_hal_srng_src_num_free(ab, srng, true);
	/* if ring is less than half filled need to replenish */
	if (num_free < (rx_ring->bufs_max / 2)) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&srng->lock);
		return;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	spin_lock_bh(&dp_mon->mon_desc_lock);
	req_entries = ath12k_dp_mon_list_cut_nodes(&list,
						   &dp->dp_mon->mon_desc_free_list,
						   num_free);
	spin_unlock_bh(&dp_mon->mon_desc_lock);

	if (req_entries)
		ath12k_dp_mon_buf_replenish(dp, rx_ring, &list, req_entries);
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_process_low_thres);

void
ath12k_dp_mon_cnt_skb_and_frags(struct sk_buff *skb, u32 *skb_count, u32 *frag_count)
{
	struct sk_buff *iter;
	u32 total_skb = 0, total_frags = 0;

	if (unlikely(!skb))
		return;

	total_skb++;
	total_frags += skb_shinfo(skb)->nr_frags;

	if (skb_has_frag_list(skb)) {
		for (iter = skb_shinfo(skb)->frag_list; iter; iter = iter->next) {
			total_skb++;
			total_frags += skb_shinfo(iter)->nr_frags;
		}
	}

	*skb_count += total_skb;
	*frag_count += total_frags;
}
EXPORT_SYMBOL(ath12k_dp_mon_cnt_skb_and_frags);

u32 ath12k_dp_mon_get_frag_size_by_idx(struct ath12k_dp *dp,
				       struct sk_buff *skb, u8 idx)
{
	u32 size = 0;

	if (likely(idx < MAX_SKB_FRAGS))
		size = skb_frag_size(&skb_shinfo(skb)->frags[idx]);

	return size;
}
EXPORT_SYMBOL(ath12k_dp_mon_get_frag_size_by_idx);

int ath12k_dp_mon_get_puncture_type(u16 puncture_pattern, u8 bw)
{
	u16 mask;
	u8 punctured_bits;

	if (!puncture_pattern)
		return NO_PUNCTURE;

	switch (bw) {
	case HAL_RX_BW_80MHZ:
		mask = PUNCTURE_80MHZ_MASK;
		break;
	case HAL_RX_BW_160MHZ:
		mask = PUNCTURE_160MHZ_MASK;
		break;
	case HAL_RX_BW_320MHZ:
		mask = PUNCTURE_320MHZ_MASK;
		break;
	default:
		return NO_PUNCTURE;
	}

	/* 0s in puncture pattern received in TLV indicates punctured 20Mhz,
	 * after complement, 1s will indicate punctured 20Mhz
	 */
	puncture_pattern = ~puncture_pattern;
	puncture_pattern &= mask;

	if (puncture_pattern) {
		punctured_bits = 0;
		while (puncture_pattern != 0) {
			punctured_bits++;
			puncture_pattern &= (puncture_pattern - 1);
		}

		if (bw == HAL_RX_BW_80MHZ) {
			if (punctured_bits == PUNC_MINUS20MHZ)
				return PUNCTURED_20MHZ;
			else
				return NO_PUNCTURE;
		} else if (bw == HAL_RX_BW_160MHZ) {
			if (punctured_bits == PUNC_MINUS20MHZ)
				return PUNCTURED_20MHZ;
			else if (punctured_bits == PUNC_MINUS40MHZ)
				return PUNCTURED_40MHZ;
			else
				return NO_PUNCTURE;
		} else if (bw == HAL_RX_BW_320MHZ) {
			if (punctured_bits == PUNC_MINUS40MHZ)
				return PUNCTURED_40MHZ;
			else if (punctured_bits == PUNC_MINUS80MHZ)
				return PUNCTURED_80MHZ;
			else if (punctured_bits == PUNC_MINUS120MHZ)
				return PUNCTURED_120MHZ;
			else
				return NO_PUNCTURE;
		}
	}
	return NO_PUNCTURE;
}

void *ath12k_dp_mon_skb_get_frag_addr(struct sk_buff *skb, u8 idx)
{
	void *frag = NULL;

	if (likely(idx < MAX_SKB_FRAGS))
		frag = skb_frag_address(&skb_shinfo(skb)->frags[idx]);

	return frag;
}
EXPORT_SYMBOL(ath12k_dp_mon_skb_get_frag_addr);

int ath12k_dp_mon_adj_frag_offset(struct sk_buff *skb, u8 idx, int offset)
{
	u32 frag_offset;
	skb_frag_t *frag;

	if (unlikely(idx >= skb_shinfo(skb)->nr_frags))
		return -EINVAL;

	frag = &skb_shinfo(skb)->frags[idx];
	frag_offset = skb_frag_off(frag);
	frag_offset += offset;
	skb_frag_off_set(frag, frag_offset);
	skb_coalesce_rx_frag(skb, idx, -(offset), 0);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_adj_frag_offset);

u32 ath12k_dp_mon_get_num_frags_in_fraglist(struct sk_buff *skb)
{
	struct sk_buff *list = NULL;
	u32 num_frags = skb_shinfo(skb)->nr_frags;

	skb_walk_frags(skb, list)
		num_frags += skb_shinfo(list)->nr_frags;

	return num_frags;
}
EXPORT_SYMBOL(ath12k_dp_mon_get_num_frags_in_fraglist);

void
ath12k_dp_mon_fill_rx_rate(struct ath12k_pdev_dp *dp_pdev,
			   struct hal_rx_mon_ppdu_info *ppdu_info,
			   struct ieee80211_rx_status *rx_status)
{
	struct ieee80211_supported_band *sband;
	struct ath12k *ar = dp_pdev->ar;
	enum rx_msdu_start_pkt_type pkt_type;
	u8 rate_mcs, nss, sgi, bw;
	bool is_cck;

	pkt_type = ppdu_info->preamble_type;
	rate_mcs = ppdu_info->mcs;
	nss = ppdu_info->nss;
	sgi = ppdu_info->gi;
	bw = ppdu_info->bw;

	switch (pkt_type) {
	case RX_MSDU_START_PKT_TYPE_11A:
	case RX_MSDU_START_PKT_TYPE_11B:
		is_cck = (pkt_type == RX_MSDU_START_PKT_TYPE_11B);
		if (rx_status->band < NUM_NL80211_BANDS) {
			sband = &ar->mac.sbands[rx_status->band];
			rx_status->rate_idx = ath12k_mac_hw_rate_to_idx(sband, rate_mcs,
									is_cck);
		}
		break;
	case RX_MSDU_START_PKT_TYPE_11N:
		rx_status->encoding = RX_ENC_HT;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		if (rate_mcs > ATH12K_HT_MCS_MAX) {
			ath12k_warn(ar->ab,
				    "Received with invalid mcs in HT mode %d\n",
				     rate_mcs);
			break;
		}
		rx_status->rate_idx = rate_mcs + (8 * (nss - 1));
		if (sgi)
			rx_status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		break;
	case RX_MSDU_START_PKT_TYPE_11AC:
		rx_status->encoding = RX_ENC_VHT;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		rx_status->nss = nss;
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_VHT_MCS_MAX) {
			ath12k_warn(ar->ab,
				    "Received with invalid mcs in VHT mode %d\n",
				     rate_mcs);
			break;
		}
		if (sgi)
			rx_status->enc_flags |= RX_ENC_FLAG_SHORT_GI;
		break;
	case RX_MSDU_START_PKT_TYPE_11AX:
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_HE_MCS_MAX) {
			ath12k_warn(ar->ab,
				    "Received with invalid mcs in HE mode %d\n",
				    rate_mcs);
			break;
		}
		rx_status->encoding = RX_ENC_HE;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		rx_status->nss = nss;
		rx_status->he_gi = ath12k_he_gi_to_nl80211_he_gi(sgi);
		break;
	case RX_MSDU_START_PKT_TYPE_11BE:
		rx_status->rate_idx = rate_mcs;
		if (rate_mcs > ATH12K_EHT_MCS_MAX) {
			ath12k_warn(ar->ab,
				    "Received with invalid mcs in EHT mode %d\n",
				    rate_mcs);
			break;
		}
		rx_status->encoding = RX_ENC_EHT;
		rx_status->bw = ath12k_mac_bw_to_mac80211_bw(bw);
		rx_status->nss = nss;
		rx_status->he_gi = ath12k_he_gi_to_nl80211_he_gi(sgi);
		break;
	default:
		ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
			   "monitor receives invalid preamble type %d",
			    pkt_type);
		break;
	}
}
EXPORT_SYMBOL(ath12k_dp_mon_fill_rx_rate);
