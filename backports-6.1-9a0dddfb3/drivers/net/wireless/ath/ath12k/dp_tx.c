// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "core.h"
#include "dp_tx.h"
#include "debug.h"
#include "debugfs.h"
#include "hw.h"
#include "peer.h"
#include "mac.h"

void ath12k_dp_tx_encap_nwifi(struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (void *)skb->data;
	u8 *qos_ctl;

	if (!ieee80211_is_data_qos(hdr->frame_control))
		return;

	qos_ctl = ieee80211_get_qos_ctl(hdr);
	memmove(skb->data + IEEE80211_QOS_CTL_LEN,
		skb->data, (void *)qos_ctl - (void *)skb->data);
	skb_pull(skb, IEEE80211_QOS_CTL_LEN);

	hdr = (void *)skb->data;
	hdr->frame_control &= ~__cpu_to_le16(IEEE80211_STYPE_QOS_DATA);
}
EXPORT_SYMBOL(ath12k_dp_tx_encap_nwifi);

void ath12k_dp_tx_release_txbuf(struct ath12k_dp *dp,
				struct ath12k_tx_desc_info *tx_desc,
				u8 pool_id)
{
	spin_lock_bh(&dp->tx_desc_lock[pool_id]);
	tx_desc->skb_ext_desc = NULL;
	list_move_tail(&tx_desc->list, &dp->tx_desc_free_list[pool_id]);
	spin_unlock_bh(&dp->tx_desc_lock[pool_id]);
}
EXPORT_SYMBOL(ath12k_dp_tx_release_txbuf);

struct ath12k_tx_desc_info *ath12k_dp_tx_assign_buffer(struct ath12k_dp *dp,
						       u8 pool_id)
{
	struct ath12k_tx_desc_info *desc;

	spin_lock_bh(&dp->tx_desc_lock[pool_id]);
	desc = list_first_entry_or_null(&dp->tx_desc_free_list[pool_id],
					struct ath12k_tx_desc_info,
					list);
	if (!desc) {
		spin_unlock_bh(&dp->tx_desc_lock[pool_id]);
		ath12k_warn(dp->ab, "failed to allocate data Tx buffer\n");
		return NULL;
	}

	list_move_tail(&desc->list, &dp->tx_desc_used_list[pool_id]);
	spin_unlock_bh(&dp->tx_desc_lock[pool_id]);

	return desc;
}
EXPORT_SYMBOL(ath12k_dp_tx_assign_buffer);

enum hal_encrypt_type ath12k_dp_tx_get_encrypt_type(u32 cipher)
{
	switch (cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
		return HAL_ENCRYPT_TYPE_WEP_40;
	case WLAN_CIPHER_SUITE_WEP104:
		return HAL_ENCRYPT_TYPE_WEP_104;
	case WLAN_CIPHER_SUITE_TKIP:
		return HAL_ENCRYPT_TYPE_TKIP_MIC;
	case WLAN_CIPHER_SUITE_CCMP:
		return HAL_ENCRYPT_TYPE_CCMP_128;
	case WLAN_CIPHER_SUITE_CCMP_256:
		return HAL_ENCRYPT_TYPE_CCMP_256;
	case WLAN_CIPHER_SUITE_GCMP:
		return HAL_ENCRYPT_TYPE_GCMP_128;
	case WLAN_CIPHER_SUITE_GCMP_256:
		return HAL_ENCRYPT_TYPE_AES_GCMP_256;
	default:
		return HAL_ENCRYPT_TYPE_OPEN;
	}
}
EXPORT_SYMBOL(ath12k_dp_tx_get_encrypt_type);

void *ath12k_dp_metadata_align_skb(struct sk_buff *skb, u8 tail_len)
{
	struct sk_buff *tail;
	void *metadata;

	if (unlikely(skb_cow_data(skb, tail_len, &tail) < 0))
		return NULL;

	metadata = pskb_put(skb, tail, tail_len);
	memset(metadata, 0, tail_len);
	return metadata;
}
EXPORT_SYMBOL(ath12k_dp_metadata_align_skb);

static void ath12k_dp_tx_move_payload(struct sk_buff *skb,
				      unsigned long delta,
				      bool head)
{
	unsigned long len = skb->len;

	if (head) {
		skb_push(skb, delta);
		memmove(skb->data, skb->data + delta, len);
		skb_trim(skb, len);
	} else {
		skb_put(skb, delta);
		memmove(skb->data + delta, skb->data, len);
		skb_pull(skb, delta);
	}
}

int ath12k_dp_tx_align_payload(struct ath12k_dp *dp, struct sk_buff **pskb)
{
	u32 iova_mask = dp->hw_params->iova_mask;
	unsigned long offset, delta1, delta2;
	struct sk_buff *skb2, *skb = *pskb;
	unsigned int headroom = skb_headroom(skb);
	int tailroom = skb_tailroom(skb);
	int ret = 0;

	offset = (unsigned long)skb->data & iova_mask;
	delta1 = offset;
	delta2 = iova_mask - offset + 1;

	if (headroom >= delta1) {
		ath12k_dp_tx_move_payload(skb, delta1, true);
	} else if (tailroom >= delta2) {
		ath12k_dp_tx_move_payload(skb, delta2, false);
	} else {
		skb2 = skb_realloc_headroom(skb, iova_mask);
		if (!skb2) {
			ret = -ENOMEM;
			goto out;
		}

		dev_kfree_skb_any(skb);

		offset = (unsigned long)skb2->data & iova_mask;
		if (offset)
			ath12k_dp_tx_move_payload(skb2, offset, true);
		*pskb = skb2;
	}

out:
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_tx_align_payload);

int
ath12k_dp_tx_htt_h2t_vdev_stats_ol_req(struct ath12k *ar, u64 reset_bitmask)
{
	struct ath12k_base *ab = ar->ab;
	struct htt_h2t_msg_type_vdev_txrx_stats_req *cmd;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct sk_buff *skb;
	int len = sizeof(*cmd), ret;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_h2t_msg_type_vdev_txrx_stats_req *)skb->data;
	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr = FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_MSG_TYPE,
			      HTT_H2T_MSG_TYPE_VDEV_TXRX_STATS_CFG);
	cmd->hdr |= FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_PDEV_ID,
			       ar->pdev->pdev_id);
	cmd->hdr |= FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_ENABLE, true);

	/* Periodic interval is calculated as 1 units = 8 ms.
	* Ex: 125 -> 1000 ms
	*/
	cmd->hdr |= FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_INTERVAL,
			       (ATH12K_STATS_TIMER_DUR_1SEC >> 3));
	cmd->hdr |= FIELD_PREP(HTT_H2T_VDEV_TXRX_HDR_RESET_STATS, true);
	cmd->vdev_id_lo_bitmask = (reset_bitmask & HTT_H2T_VDEV_TXRX_LO_BITMASK);
	cmd->vdev_id_hi_bitmask = ((reset_bitmask &
				    HTT_H2T_VDEV_TXRX_HI_BITMASK) >> 32);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
		ath12k_warn(ab, "failed to send htt type vdev stats offload request: %d",
			    ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}
