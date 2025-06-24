// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "dp_mon.h"
#include "debug.h"
#include "dp_rx.h"
#include "dp_tx.h"
#include "peer.h"
#include "wifi7/hal_qcn9274.h"
#include "debugfs.h"
#include "dp_mon_filter.h"

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

static void
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

static void
ath12k_dp_mon_fill_rx_rate(struct ath12k *ar,
			   struct hal_rx_mon_ppdu_info *ppdu_info,
			   struct ieee80211_rx_status *rx_status)
{
	struct ieee80211_supported_band *sband;
	enum rx_msdu_start_pkt_type pkt_type;
	u8 rate_mcs, nss, sgi;
	bool is_cck;

	pkt_type = ppdu_info->preamble_type;
	rate_mcs = ppdu_info->rate;
	nss = ppdu_info->nss;
	sgi = ppdu_info->gi;

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
		rx_status->he_gi = ath12k_he_gi_to_nl80211_he_gi(sgi);
		break;
	default:
		ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
			   "monitor receives invalid preamble type %d",
			    pkt_type);
		break;
	}
}

static void ath12k_dp_mon_rx_msdus_set_payload(struct ath12k_base *ab,
					       struct sk_buff *head_msdu,
					       struct sk_buff *tail_msdu)
{
	u32 rx_pkt_offset, l2_hdr_offset, total_offset;

	if (ath12k_dp_get_mon_type(ab->dp) == ATH12K_DP_MON_TYPE_DUAL_RING) {
		total_offset = ATH12K_MON_RX_PKT_OFFSET;
	} else {
		rx_pkt_offset = ab->hal.hal_desc_sz;
		l2_hdr_offset =
			ath12k_hal_rx_h_l3pad_get(&ab->hal,
						  (struct hal_rx_desc *)tail_msdu->data);

		total_offset = rx_pkt_offset + l2_hdr_offset;
	}

	skb_pull(head_msdu, total_offset);
}

static struct sk_buff *
ath12k_dp_mon_rx_merg_msdus(struct ath12k_pdev_dp *dp_pdev,
			    struct dp_mon_mpdu *mon_mpdu,
			    struct hal_rx_mon_ppdu_info *ppdu_info,
			    struct ieee80211_rx_status *rxs)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k *ar = dp_pdev->ar;
	struct sk_buff *msdu, *mpdu_buf, *prev_buf, *head_frag_list;
	struct sk_buff *head_msdu, *tail_msdu;
	struct hal_rx_desc *rx_desc;
	u8 *hdr_desc, *dest, decap_format = mon_mpdu->decap_format;
	struct ieee80211_hdr_3addr *wh;
	struct ieee80211_channel *channel;
	u32 frag_list_sum_len = 0;
	u8 channel_num = ppdu_info->chan_num;

	mpdu_buf = NULL;
	head_msdu = mon_mpdu->head;
	tail_msdu = mon_mpdu->tail;

	if (!head_msdu || !tail_msdu)
		goto err_merge_fail;

	ath12k_dp_mon_fill_rx_stats_info(ppdu_info, rxs);

	if (unlikely(rxs->band == NUM_NL80211_BANDS ||
		     !ath12k_dp_pdev_to_hw(dp_pdev)->wiphy->bands[rxs->band])) {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "sband is NULL for status band %d channel_num %d center_freq %d pdev_id %d\n",
			   rxs->band, channel_num, ppdu_info->freq, ar->pdev_idx);

		spin_lock_bh(&ar->data_lock);
		channel = ar->rx_channel;
		if (channel) {
			rxs->band = channel->band;
			channel_num =
				ieee80211_frequency_to_channel(channel->center_freq);
		}
		spin_unlock_bh(&ar->data_lock);
	}

	if (rxs->band < NUM_NL80211_BANDS)
		rxs->freq = ieee80211_channel_to_frequency(channel_num,
							   rxs->band);

	ath12k_dp_mon_fill_rx_rate(ar, ppdu_info, rxs);

	if (decap_format == DP_RX_DECAP_TYPE_RAW) {
		ath12k_dp_mon_rx_msdus_set_payload(ab, head_msdu, tail_msdu);

		prev_buf = head_msdu;
		msdu = head_msdu->next;
		head_frag_list = NULL;

		while (msdu) {
			ath12k_dp_mon_rx_msdus_set_payload(ab, head_msdu, tail_msdu);

			if (!head_frag_list)
				head_frag_list = msdu;

			frag_list_sum_len += msdu->len;
			prev_buf = msdu;
			msdu = msdu->next;
		}

		prev_buf->next = NULL;

		skb_trim(prev_buf, prev_buf->len);
		if (head_frag_list) {
			skb_shinfo(head_msdu)->frag_list = head_frag_list;
			head_msdu->data_len = frag_list_sum_len;
			head_msdu->len += head_msdu->data_len;
			head_msdu->next = NULL;
		}
	} else if (decap_format == DP_RX_DECAP_TYPE_NATIVE_WIFI) {
		u8 qos_pkt = 0;

		rx_desc = (struct hal_rx_desc *)head_msdu->data;
		hdr_desc =
		ath12k_wifi7_hal_rx_desc_get_msdu_payload_qcn9274(rx_desc);

		/* Base size */
		wh = (struct ieee80211_hdr_3addr *)hdr_desc;

		if (ieee80211_is_data_qos(wh->frame_control))
			qos_pkt = 1;

		msdu = head_msdu;

		while (msdu) {
			ath12k_dp_mon_rx_msdus_set_payload(ab, head_msdu, tail_msdu);
			if (qos_pkt) {
				dest = skb_push(msdu, sizeof(__le16));
				if (!dest)
					goto err_merge_fail;
				memcpy(dest, hdr_desc, sizeof(struct ieee80211_qos_hdr));
			}
			prev_buf = msdu;
			msdu = msdu->next;
		}
		dest = skb_put(prev_buf, HAL_RX_FCS_LEN);
		if (!dest)
			goto err_merge_fail;

		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "mpdu_buf %p mpdu_buf->len %u",
			   prev_buf, prev_buf->len);
	} else {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "decap format %d is not supported!\n",
			   decap_format);
		goto err_merge_fail;
	}

	return head_msdu;

err_merge_fail:
	if (mpdu_buf && decap_format != DP_RX_DECAP_TYPE_RAW) {
		ath12k_dbg(ab, ATH12K_DBG_DATA,
			   "err_merge_fail mpdu_buf %p", mpdu_buf);
		/* Free the head buffer */
		dev_kfree_skb_any(mpdu_buf);
	}
	return NULL;
}

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

static void ath12k_dp_mon_update_radiotap(struct ath12k_pdev_dp *dp_pdev,
					  struct hal_rx_mon_ppdu_info *ppduinfo,
					  struct sk_buff *mon_skb,
					  struct ieee80211_rx_status *rxs)
{
	struct ieee80211_supported_band *sband;
	u8 *ptr = NULL;

	rxs->flag |= RX_FLAG_MACTIME_START;
	rxs->signal = (s8)(ppduinfo->rssi_comb + dp_pdev->ar->rssi_offsets.rssi_offset);
	rxs->nss = ppduinfo->nss + 1;

	if (ppduinfo->userstats[ppduinfo->userid].ampdu_present) {
		rxs->flag |= RX_FLAG_AMPDU_DETAILS;
		rxs->ampdu_reference = ppduinfo->userstats[ppduinfo->userid].ampdu_id;
	}

	if (ppduinfo->is_eht || ppduinfo->eht_usig) {
		struct ieee80211_radiotap_tlv *tlv;
		struct ieee80211_radiotap_eht *eht;
		struct ieee80211_radiotap_eht_usig *usig;
		u16 len = 0, i, eht_len, usig_len;
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
		sband = &dp_pdev->ar->mac.sbands[rxs->band];
		rxs->rate_idx = ath12k_mac_hw_rate_to_idx(sband, ppduinfo->rate,
							  ppduinfo->cck_flag);
	}

	rxs->mactime = ppduinfo->tsft;
}

static void ath12k_dp_mon_rx_deliver_msdu(struct ath12k_pdev_dp *dp_pdev, struct napi_struct *napi,
					  struct sk_buff *msdu,
					  struct ieee80211_rx_status *status,
					  struct hal_rx_mon_ppdu_info *ppduinfo,
					  u8 decap)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	static const struct ieee80211_radiotap_he known = {
		.data1 = cpu_to_le16(IEEE80211_RADIOTAP_HE_DATA1_DATA_MCS_KNOWN |
				     IEEE80211_RADIOTAP_HE_DATA1_BW_RU_ALLOC_KNOWN),
		.data2 = cpu_to_le16(IEEE80211_RADIOTAP_HE_DATA2_GI_KNOWN),
	};
	struct ieee80211_rx_status *rx_status;
	struct ieee80211_radiotap_he *he = NULL;
	struct ieee80211_sta *pubsta = NULL;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	bool is_mcbc = rxcb->is_mcbc;
	bool is_eapol_tkip = rxcb->is_eapol;

	status->link_valid = 0;
	status->link_id = 0;

	if ((status->encoding == RX_ENC_HE) && !(status->flag & RX_FLAG_RADIOTAP_HE) &&
	    !(status->flag & RX_FLAG_SKIP_MONITOR)) {
		he = skb_push(msdu, sizeof(known));
		memcpy(he, &known, sizeof(known));
		status->flag |= RX_FLAG_RADIOTAP_HE;
	}

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_id(dp, ppduinfo->peer_id);
	if (peer && peer->sta) {
		pubsta = peer->sta;
		if (pubsta->valid_links) {
			status->link_valid = 1;
			status->link_id = peer->link_id;
		}
	}

	spin_unlock_bh(&dp->dp_lock);

	ath12k_dbg(ab, ATH12K_DBG_DATA,
		   "rx skb %p len %u peer %pM %u %s %s%s%s%s%s%s%s%s %srate_idx %u vht_nss %u freq %u band %u flag 0x%x fcs-err %i mic-err %i amsdu-more %i\n",
		   msdu,
		   msdu->len,
		   peer ? peer->addr : NULL,
		   rxcb->tid,
		   (is_mcbc) ? "mcast" : "ucast",
		   (status->encoding == RX_ENC_LEGACY) ? "legacy" : "",
		   (status->encoding == RX_ENC_HT) ? "ht" : "",
		   (status->encoding == RX_ENC_VHT) ? "vht" : "",
		   (status->encoding == RX_ENC_HE) ? "he" : "",
		   (status->bw == RATE_INFO_BW_40) ? "40" : "",
		   (status->bw == RATE_INFO_BW_80) ? "80" : "",
		   (status->bw == RATE_INFO_BW_160) ? "160" : "",
		   (status->bw == RATE_INFO_BW_320) ? "320" : "",
		   status->enc_flags & RX_ENC_FLAG_SHORT_GI ? "sgi " : "",
		   status->rate_idx,
		   status->nss,
		   status->freq,
		   status->band, status->flag,
		   !!(status->flag & RX_FLAG_FAILED_FCS_CRC),
		   !!(status->flag & RX_FLAG_MMIC_ERROR),
		   !!(status->flag & RX_FLAG_AMSDU_MORE));

	ath12k_dbg_dump(ab, ATH12K_DBG_DP_RX, NULL, "dp rx msdu: ",
			msdu->data, msdu->len);
	rx_status = IEEE80211_SKB_RXCB(msdu);
	*rx_status = *status;

	/* TODO: trace rx packet */

	/* PN for multicast packets are not validate in HW,
	 * so skip 802.3 rx path
	 * Also, fast_rx expects the STA to be authorized, hence
	 * eapol packets are sent in slow path.
	 */
	if (decap == DP_RX_DECAP_TYPE_ETHERNET2_DIX && !is_eapol_tkip &&
	    !(is_mcbc && rx_status->flag & RX_FLAG_DECRYPTED))
		rx_status->flag |= RX_FLAG_8023;

	ieee80211_rx_napi(ath12k_dp_pdev_to_hw(dp_pdev), pubsta, msdu, napi);
}

int ath12k_dp_mon_rx_deliver(struct ath12k_pdev_dp *dp_pdev,
			     struct dp_mon_mpdu *mon_mpdu,
			     struct hal_rx_mon_ppdu_info *ppduinfo,
			     struct napi_struct *napi)
{
	struct sk_buff *mon_skb, *skb_next, *header;
	struct ieee80211_rx_status *rxs = &dp_pdev->dp_mon_pdev->rx_status;
	u8 decap = DP_RX_DECAP_TYPE_RAW;

	mon_skb = ath12k_dp_mon_rx_merg_msdus(dp_pdev, mon_mpdu, ppduinfo, rxs);
	if (!mon_skb)
		goto mon_deliver_fail;

	header = mon_skb;
	rxs->flag = 0;

	if (mon_mpdu->err_bitmap & HAL_RX_MPDU_ERR_FCS)
		rxs->flag = RX_FLAG_FAILED_FCS_CRC;

	do {
		skb_next = mon_skb->next;
		if (!skb_next)
			rxs->flag &= ~RX_FLAG_AMSDU_MORE;
		else
			rxs->flag |= RX_FLAG_AMSDU_MORE;

		if (mon_skb == header) {
			header = NULL;
			rxs->flag &= ~RX_FLAG_ALLOW_SAME_PN;
		} else {
			rxs->flag |= RX_FLAG_ALLOW_SAME_PN;
		}
		rxs->flag |= RX_FLAG_ONLY_MONITOR;

		if (!(rxs->flag & RX_FLAG_ONLY_MONITOR))
			decap = mon_mpdu->decap_format;

		ath12k_dp_mon_update_radiotap(dp_pdev, ppduinfo, mon_skb, rxs);
		ath12k_dp_mon_rx_deliver_msdu(dp_pdev, napi, mon_skb, rxs, ppduinfo, decap);
		mon_skb = skb_next;
	} while (mon_skb);
	rxs->flag = 0;

	return 0;

mon_deliver_fail:
	mon_skb = mon_mpdu->head;
	while (mon_skb) {
		skb_next = mon_skb->next;
		dev_kfree_skb_any(mon_skb);
		mon_skb = skb_next;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_deliver);

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

int ath12k_dp_mon_buf_replenish(struct ath12k_base *ab,
				struct dp_rxdma_mon_ring *buf_ring,
				int req_entries)
{
	struct hal_mon_buf_ring *mon_buf;
	struct sk_buff *skb;
	struct hal_srng *srng;
	dma_addr_t paddr;
	u32 cookie;
	int buf_id;

	srng = &ab->hal.srng_list[buf_ring->refill_buf_ring.ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (req_entries > 0) {
		skb = dev_alloc_skb(DP_RX_MON_BUFFER_SIZE + DP_RX_BUFFER_ALIGN_SIZE);
		if (unlikely(!skb))
			goto fail_alloc_skb;

		if (!IS_ALIGNED((unsigned long)skb->data, DP_RX_BUFFER_ALIGN_SIZE)) {
			skb_pull(skb,
				 PTR_ALIGN(skb->data, DP_RX_BUFFER_ALIGN_SIZE) -
				 skb->data);
		}
#ifndef CONFIG_IO_COHERENCY
		paddr = dma_map_single(ab->dev, skb->data,
				       skb->len + skb_tailroom(skb),
				       DMA_FROM_DEVICE);

		if (unlikely(dma_mapping_error(ab->dev, paddr)))
			goto fail_free_skb;
#else
		paddr = virt_to_phys(skb->data);
		if(unlikely(!paddr))
			goto fail_free_skb;
#endif
		spin_lock_bh(&buf_ring->idr_lock);
		buf_id = idr_alloc(&buf_ring->bufs_idr, skb, 0,
				   buf_ring->bufs_max * 3, GFP_ATOMIC);
		spin_unlock_bh(&buf_ring->idr_lock);

		if (unlikely(buf_id < 0))
			goto fail_dma_unmap;

		mon_buf = ath12k_hal_srng_src_get_next_entry(ab, srng);
		if (unlikely(!mon_buf))
			goto fail_idr_remove;

		ATH12K_SKB_RXCB(skb)->paddr = paddr;

		cookie = u32_encode_bits(buf_id, DP_MON_RXDMA_BUF_COOKIE_BUF_ID);

		mon_buf->paddr_lo = cpu_to_le32(lower_32_bits(paddr));
		mon_buf->paddr_hi = cpu_to_le32(upper_32_bits(paddr));
		mon_buf->cookie = cpu_to_le64(cookie);

		req_entries--;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);
	return 0;

fail_idr_remove:
	spin_lock_bh(&buf_ring->idr_lock);
	idr_remove(&buf_ring->bufs_idr, buf_id);
	spin_unlock_bh(&buf_ring->idr_lock);
fail_dma_unmap:
	ath12k_core_dma_unmap_single(ab->dev, paddr, skb->len + skb_tailroom(skb),
				     DMA_FROM_DEVICE);
fail_free_skb:
	dev_kfree_skb_any(skb);
fail_alloc_skb:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);
	return -ENOMEM;
}
EXPORT_SYMBOL(ath12k_dp_mon_buf_replenish);

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

static struct dp_mon_tx_ppdu_info *
ath12k_dp_mon_hal_tx_ppdu_info(struct ath12k_mon_data *pmon,
			       u16 tlv_tag)
{
	switch (tlv_tag) {
	case HAL_TX_FES_SETUP:
	case HAL_TX_FLUSH:
	case HAL_PCU_PPDU_SETUP_INIT:
	case HAL_TX_PEER_ENTRY:
	case HAL_TX_QUEUE_EXTENSION:
	case HAL_TX_MPDU_START:
	case HAL_TX_MSDU_START:
	case HAL_TX_DATA:
	case HAL_MON_BUF_ADDR:
	case HAL_TX_MPDU_END:
	case HAL_TX_LAST_MPDU_FETCHED:
	case HAL_TX_LAST_MPDU_END:
	case HAL_COEX_TX_REQ:
	case HAL_TX_RAW_OR_NATIVE_FRAME_SETUP:
	case HAL_SCH_CRITICAL_TLV_REFERENCE:
	case HAL_TX_FES_SETUP_COMPLETE:
	case HAL_TQM_MPDU_GLOBAL_START:
	case HAL_SCHEDULER_END:
	case HAL_TX_FES_STATUS_USER_PPDU:
		break;
	case HAL_TX_FES_STATUS_PROT: {
		if (!pmon->tx_prot_ppdu_info->is_used)
			pmon->tx_prot_ppdu_info->is_used = true;

		return pmon->tx_prot_ppdu_info;
	}
	}

	if (!pmon->tx_data_ppdu_info->is_used)
		pmon->tx_data_ppdu_info->is_used = true;

	return pmon->tx_data_ppdu_info;
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

	switch (tlv_status) {
	case HAL_TX_MON_MPDU_START:
		struct dp_mon_mpdu *mon_mpdu = tx_ppdu_info->tx_mon_mpdu;

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

		if (mon_mpdu->head)
			ath12k_dp_mon_rx_deliver(dp_pdev, mon_mpdu,
						 &tx_ppdu_info->tx_info.rx_status, napi);

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
	tlv_tag = le32_get_bits(tlv->tl, HAL_TLV_HDR_TAG);

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
		tlv_tag = le32_get_bits(tlv->tl, HAL_TLV_HDR_TAG);
		tlv_len = le32_get_bits(tlv->tl, HAL_TLV_HDR_LEN);
		tlv_userid = le32_get_bits(tlv->tl, HAL_TLV_USR_ID);

		tx_ppdu_info = ath12k_dp_mon_hal_tx_ppdu_info(pmon,
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

void ath12k_dp_mon_rx_update_peer_su_stats(struct ath12k_pdev_dp *pdev_dp,
					   struct ath12k_dp_link_peer *peer,
					   struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_rx_peer_stats *rx_stats = peer->peer_stats.rx_stats;
	u32 num_msdu;

	peer->rssi_comb = ppdu_info->rssi_comb;
	ewma_avg_rssi_add(&peer->avg_rssi, ppdu_info->rssi_comb);

	if (!ath12k_debugfs_is_extd_rx_stats_enabled(pdev_dp->ar) || !rx_stats)
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
	struct ath12k_link_sta *arsta;
	struct ath12k_rx_peer_stats *rx_stats = NULL;
	struct hal_rx_user_status *user_stats = &ppdu_info->userstats[uid];
	struct ath12k_dp_link_peer *peer;
	u32 num_msdu;
	struct ath12k_dp *dp = pdev_dp->dp;
	struct ath12k_base *ab = dp->ab;

	if (ppdu_info->peer_id == HAL_INVALID_PEERID)
		return;

	peer = ath12k_dp_link_peer_find_by_ast(dp, user_stats->ast_index);
	if (!peer) {
		ath12k_warn(ab, "peer with peer id %d can't be found\n",
			    ppdu_info->peer_id);
		return;
	}

	arsta = ath12k_peer_get_link_sta(ab, peer);
	if (!arsta) {
		ath12k_warn(ab, "link sta not found on peer %pM id %d\n",
			    peer->addr, peer->peer_id);
		return;
	}

	peer->peer_stats.rx_retries = user_stats->mpdu_retry;

	if (!ath12k_debugfs_is_extd_rx_stats_enabled(pdev_dp->ar))
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
}

void
ath12k_dp_mon_rx_update_peer_mu_stats(struct ath12k_pdev_dp *pdev_dp,
				      struct hal_rx_mon_ppdu_info *ppdu_info)
{
	u32 num_users, i;

	if (!ath12k_debugfs_is_extd_rx_stats_enabled(pdev_dp->ar))
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
       struct ath12k_dp_peer_stats *stats = NULL;
       struct ath12k_link_sta *arsta;
       struct ath12k_dp_link_peer *peer;
       u32 nss_ru_width_sum = 0;
       u64 temp_result = 0;
       u16 rx_time_us = 0;
       u8 ac = 0;

       if (!dp_pdev)
               return;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(), "PPDU per user rx time update called without rcu lock\n");
	lockdep_assert_held(&dp_pdev->dp->dp_lock);

       peer = ath12k_dp_link_peer_find_by_id(dp_pdev->dp, user_stats->sw_peer_id);
       if (!peer || !peer->sta) {
               ath12k_dbg(dp_pdev->ar->ab, ATH12K_DBG_PEER,
                          "peer stats not found on ppdu peer id %d\n",
                          user_stats->sw_peer_id);
               return;
       }

       arsta = ath12k_peer_get_link_sta(dp_pdev->ar->ab, peer);
       if (!arsta) {
               ath12k_warn(dp_pdev->ar->ab, "link sta not found on peer %pM id %d\n",
                           peer->addr, peer->peer_id);
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

void ath12k_dp_rxdma_mon_buf_ring_free(struct ath12k_base *ab,
				       struct dp_rxdma_mon_ring *rx_ring)
{
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

	idr_init(&dp_mon->rxdma_mon_buf_ring.bufs_idr);
	spin_lock_init(&dp_mon->rxdma_mon_buf_ring.idr_lock);

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
	int num_entries, ret;

	rx_ring = &dp_mon->rxdma_mon_buf_ring;

	num_entries =  rx_ring->refill_buf_ring.size /
		ath12k_hal_srng_get_entrysize(ab, HAL_RXDMA_MONITOR_BUF);
	rx_ring->bufs_max = num_entries;

	ret = ath12k_dp_mon_buf_replenish(ab, rx_ring, num_entries);

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_rx_buf_setup);

void ath12k_dp_mon_rx_buf_free(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_rxdma_mon_ring *rx_ring;

	rx_ring = &dp_mon->rxdma_mon_buf_ring;

	ath12k_dp_rxdma_mon_buf_ring_free(ab, rx_ring);
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

	dp_pdev->dp_mon_pdev = dp_mon_pdev;

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_alloc);

void ath12k_dp_mon_pdev_free(struct ath12k_pdev_dp *dp_pdev)
{
	kfree(dp_pdev->dp_mon_pdev);
	dp_pdev->dp_mon_pdev = NULL;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_free);

void ath12k_dp_mon_pdev_rx_attach(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_mon_data *pmon = &dp_mon_pdev->mon_data;
	const struct ath12k_dp_arch_mon_ops *mon_ops;

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
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_rx_attach);

void ath12k_dp_mon_rx_stats_enable(struct ath12k_pdev_dp *dp_pdev,
				   enum dp_mon_stats_mode mode)
{
	struct ath12k *ar = dp_pdev->ar;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (ath12k_debugfs_is_extd_rx_stats_enabled(ar))
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

static void ath12k_dp_mon_clear_pdev_airtime_stats(struct ath12k *ar)
{
       struct ath12k_pdev_dp *pdev_dp = &ar->dp;
       u8 ac;

       for (ac = 0; ac < ATH12K_DP_WLAN_MAX_AC; ac++) {
               pdev_dp->stats.telemetry_stats.tx_link_airtime[ac] = 0;
               pdev_dp->stats.telemetry_stats.rx_link_airtime[ac] = 0;
       }
}

static inline u64 ath12k_get_timestamp_in_us(void)
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

       airtime_stats = &peer->peer_stats.dp_mon_stats.mon_stats;
       time_diff = (u32)(current_time - airtime_stats->last_update_time);

       for (ac = 0; ac < ATH12K_DP_WLAN_MAX_AC; ac++) {
               /* *_link_airtime refers to the amount of time a peer spends
                * transmitting or receiving data over the medium
                * To calculate total pdev's airtime cosumption, then store
                * each peer airtime consumption in pdev telemetry link airtime
                */

               /* Tx Airtime Consumption */
               peer_consump = &airtime_stats->tx_airtime_consumption[ac];
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
               peer_consump->consumption = 0;

               /* Rx Airtime Consumption */
               peer_consump = &airtime_stats->rx_airtime_consumption[ac];
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
       stats->snr = 0;
}
EXPORT_SYMBOL(ath12k_dp_mon_pdev_update_telemetry_stats);

int ath12k_dp_get_peer_telemetry_stats(struct ath12k_base *ab,
                                      const u8 *peer_addr,
                                      struct ath12k_peer_telemetry_stats *stats)
{
       struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	spin_lock_bh(&dp->dp_lock);

       peer = ath12k_dp_link_peer_find_by_addr(dp, peer_addr);
       if (!peer) {
		spin_unlock_bh(&dp->dp_lock);
               ath12k_dbg(ab,
                          ATH12K_DBG_DP_HTT,
                          "Failed to find peer at addr: %pM\n", peer_addr);
               return -EINVAL;
       }

       ath12k_dp_mon_peer_telemetry_stats(peer, stats);

	spin_unlock_bh(&dp->dp_lock);

       return 0;
}
