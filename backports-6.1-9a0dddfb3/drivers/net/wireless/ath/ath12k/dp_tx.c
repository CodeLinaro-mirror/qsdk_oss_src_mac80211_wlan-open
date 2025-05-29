// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "core.h"
#include "dp.h"
#include "dp_tx.h"
#include "debug.h"
#include "debugfs.h"
#include "hw.h"
#include "peer.h"
#include "mac.h"
#include "ppe.h"
#include "hal.h"

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static void
ath12k_dp_ppeds_tx_release_desc_list_bulk(struct ath12k_dp *dp,
					  struct list_head *local_list,
					  int local_list_len)
{
	struct ath12k_ppeds_tx_desc_info *desc = NULL, *first_desc = NULL, *last_desc = NULL, *tmp;
	int hotlist_remaining_len;
	struct sk_buff_head free_list_head;
	struct sk_buff *skb;
	int count = 0;
	struct list_head local_list_for_reuse;

	spin_lock_bh(&dp->ppe.ppeds_tx_desc_lock);

	hotlist_remaining_len = ath12k_ppeds_desc_params.ppeds_hotlist_len -
						dp->ppe.ppeds_tx_desc_reuse_list_len;

	if (likely(hotlist_remaining_len >= local_list_len)) {
		list_splice_tail(local_list, &dp->ppe.ppeds_tx_desc_reuse_list);
		dp->ppe.ppeds_tx_desc_reuse_list_len += local_list_len;
		spin_unlock_bh(&dp->ppe.ppeds_tx_desc_lock);
		return;
	}

	if (hotlist_remaining_len == 0)
		goto skip_reuse_list;

	/* Identify the first and last descriptors in local_list with length of
	 * available room in hotlist
	 */
	list_for_each_entry_safe(desc, tmp, local_list, list) {
		if (count == 0)
			first_desc = desc;

		if (count == (hotlist_remaining_len - 1)) {
			last_desc = desc;
			break;
		}

		count++;
	}

	INIT_LIST_HEAD(&local_list_for_reuse);

	if (first_desc && last_desc) {
		/* cut the local_list into local_list_for_reuse and local_list */
		list_cut_position(&local_list_for_reuse, local_list, &last_desc->list);

		/* merge local_list_for_reuse into global dp->ppe.ppeds_tx_desc_reuse_list */
		list_splice_tail(&local_list_for_reuse, &dp->ppe.ppeds_tx_desc_reuse_list);
		dp->ppe.ppeds_tx_desc_reuse_list_len += count + 1;
	}

skip_reuse_list:
	skb_queue_head_init(&free_list_head);

	list_for_each_entry_safe(desc, tmp, local_list, list) {
		skb = desc->skb;
		desc->skb = NULL;
		desc->paddr = (dma_addr_t)NULL;
		desc->in_use = false;
		if (!skb) {
			pr_err("no skb in ds completion path");
			continue;
		}

		if (likely(skb->is_from_recycler))
			__skb_queue_head(&free_list_head, skb);
		else
			dev_kfree_skb(skb);
	}

	/* Add the remaining descriptors to the free list */
	list_splice_tail(local_list, &dp->ppe.ppeds_tx_desc_free_list);

	spin_unlock_bh(&dp->ppe.ppeds_tx_desc_lock);

	dev_kfree_skb_list_fast(&free_list_head);
}
#endif

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
static inline
void ath12k_dp_ppeds_tx_comp_get_desc(struct ath12k_base *ab,
				      struct hal_wbm_completion_ring_tx *tx_status,
				      struct ath12k_ppeds_tx_desc_info **tx_desc)
{
	u64 desc_va = 0;
	u32 desc_id;

	if (likely(HAL_WBM_COMPL_TX_INFO0_CC_DONE & tx_status->info0)) {
		/* HW done cookie conversion */
		desc_va = ((u64)tx_status->buf_va_hi << 32 |
			   tx_status->buf_va_lo);
		*tx_desc = (struct ath12k_ppeds_tx_desc_info *)((unsigned long)desc_va);
	} else {
		/* SW does cookie conversion to VA */
		desc_id = u32_get_bits(tx_status->buf_va_hi,
				       BUFFER_ADDR_INFO1_SW_COOKIE);

		*tx_desc = ath12k_dp_get_ppeds_tx_desc(ab, desc_id);
	}
}

static inline bool ath12k_dp_tx_completion_valid(struct hal_wbm_release_ring *desc)
{
	struct htt_tx_wbm_completion *status_desc;

	if (FIELD_GET(HAL_WBM_COMPL_TX_INFO0_REL_SRC_MODULE, desc->info0) ==
			HAL_WBM_REL_SRC_MODULE_FW) {
		status_desc = (struct htt_tx_wbm_completion *)(((u8 *)desc) + HTT_TX_WBM_COMP_STATUS_OFFSET);

		/* Dont consider HTT_TX_COMP_STATUS_MEC_NOTIFY */
		if (FIELD_GET(HTT_TX_WBM_COMP_INFO0_STATUS, status_desc->info0) ==
				HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY)
			return false;
	}
	return true;
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_hal_srng_ppeds_dst_inv_entry(struct ath12k_base *ab,
					 struct hal_srng *srng, int entries)
{
	u32 *desc, *last_desc;
	u32 tp, hp;
	u32 remaining_entries;

	if (!(srng->flags & HAL_SRNG_FLAGS_CACHED) || !entries)
		return;

	tp = srng->u.dst_ring.tp;
	hp = srng->u.dst_ring.cached_hp;

	desc = srng->ring_base_vaddr + tp;
	if (hp > tp) {
		last_desc = ((void *)desc + entries * srng->entry_size * sizeof(u32));
		dmac_inv_range_no_dsb((void *)desc,
				      (void *)last_desc);
	} else {
		remaining_entries = srng->ring_size - tp;
		last_desc = ((void *)desc + remaining_entries * sizeof(u32));
		dmac_inv_range_no_dsb((void *)desc, (void *)last_desc);

		last_desc = ((void *)srng->ring_base_vaddr + hp * sizeof(u32));
		dmac_inv_range_no_dsb((void *)srng->ring_base_vaddr, (void *)last_desc);
	}

	dsb(st);
}
#endif

int ath12k_ppeds_tx_completion_handler(struct ath12k_base *ab, int budget)
{
	struct ath12k_dp *dp = ab->dp;
	struct dp_ppeds_tx_comp_ring *tx_ring = &dp->ppe.ppeds_comp_ring;
	int hal_ring_id = tx_ring->ppe_wbm2sw_ring.ring_id;
	struct hal_srng *status_ring = &ab->hal.srng_list[hal_ring_id];
	struct ath12k_ppeds_tx_desc_info *tx_desc = NULL;
	int valid_entries, count = 0;
	struct hal_wbm_release_ring *desc;
	struct hal_wbm_completion_ring_tx *tx_status;
	struct htt_tx_wbm_completion *status_desc;
	enum hal_wbm_rel_src_module buf_rel_source;
	int htt_status;
	struct list_head local_list;
	size_t stat_size;

	BUG_ON(budget > DP_PPEDS_SERVICE_BUDGET);

	if (likely(ab->stats_disable))
		/* only need buf_addr_info and info0 */
		stat_size = 3 * sizeof(u32);
	else
		stat_size = sizeof(struct hal_wbm_release_ring);
	INIT_LIST_HEAD(&local_list);
	spin_lock_bh(&status_ring->lock);

	ath12k_hal_srng_access_begin(ab, status_ring);

	valid_entries = ath12k_hal_srng_dst_num_free(ab, status_ring, false);
	if (!valid_entries) {
		ath12k_hal_srng_access_end(ab, status_ring);
		spin_unlock_bh(&status_ring->lock);
		return count;
	}

	if (valid_entries >= budget)
		valid_entries = budget;

	ath12k_hal_srng_ppeds_dst_inv_entry(ab, status_ring, valid_entries);

	while (likely(valid_entries--)) {
		desc = (struct hal_wbm_release_ring *)
			ath12k_hal_srng_dst_get_next_entry(ab, status_ring);
		if (!desc || !ath12k_dp_tx_completion_valid(desc))
			continue;

		tx_status = (struct hal_wbm_completion_ring_tx *)desc;
		if (likely(!ab->stats_disable))
			memcpy(&tx_ring->tx_status[count], desc, stat_size);

		buf_rel_source = FIELD_GET(HAL_WBM_RELEASE_INFO0_REL_SRC_MODULE,
					   tx_status->info0);

		ath12k_dp_ppeds_tx_comp_get_desc(ab, tx_status, &tx_desc);
		if (unlikely(!tx_desc)) {
			ath12k_warn(ab, "unable to retrieve ppe ds tx_desc!");
			continue;
		}
		tx_ring->macid[count] = tx_desc->mac_id;

		if (unlikely(buf_rel_source == HAL_WBM_REL_SRC_MODULE_FW)) {
			status_desc = ((void *)tx_status) + HTT_TX_WBM_COMP_STATUS_OFFSET;
			htt_status = u32_get_bits(status_desc->info0,
						  HTT_TX_WBM_COMP_INFO0_STATUS);
			if (htt_status != HAL_WBM_REL_HTT_TX_COMP_STATUS_OK) {
				ab->dp->ppe.ppeds_stats.fw2wbm_pkt_drops++;
				ath12k_dbg(ab, ATH12K_DBG_PPE,
					   "ath12k: Frame received from unexpected source %d status %d!\n",
					   buf_rel_source, htt_status);
			}
			tx_ring->macid[count] = 0xF;
		}
		/* add descriptor to local list to process in bulk */
		tx_desc->in_use = false;
		list_add_tail(&tx_desc->list, &local_list);
		count++;
	}
	ath12k_hal_srng_access_end(ab, status_ring);
	spin_unlock_bh(&status_ring->lock);

	ath12k_dp_ppeds_tx_release_desc_list_bulk(dp, &local_list, count);
	return count;
}
#endif


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
	tx_desc->in_use = false;
	list_add_tail(&tx_desc->list, &dp->tx_desc_free_list[pool_id]);
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

	list_del(&desc->list);
	desc->in_use = true;
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

int ath12k_dp_tx_htt_pri_link_migr_msg(struct ath12k_base *ab, u16 vdev_id,
				       u16 peer_id, u16 ml_peer_id, u8 pdev_id,
				       u8 chip_id, u16 src_info, bool status)
{
	struct ath12k_htt_pri_link_migr_h2t_msg *cmd;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	bool src_info_valid = false;
	int len = sizeof(*cmd);
	struct sk_buff *skb;
	int ret;

	skb = ath12k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);

	cmd = (struct ath12k_htt_pri_link_migr_h2t_msg *)skb->data;
	memset(cmd, 0, sizeof(*cmd));

	cmd->info0 = le32_encode_bits(HTT_H2T_MSG_TYPE_PRIMARY_LINK_PEER_MIGRATE_RESP,
				      ATH12K_HTT_PRI_LINK_MIGR_MSG_TYPE) |
		     le32_encode_bits(chip_id, ATH12K_HTT_PRI_LINK_MIGR_CHIP_ID) |
		     le32_encode_bits(pdev_id, ATH12K_HTT_PRI_LINK_MIGR_PDEV_ID) |
		     le32_encode_bits(vdev_id, ATH12K_HTT_PRI_LINK_MIGR_VDEV_ID);

	ml_peer_id &= ~ATH12K_PEER_ML_ID_VALID;

	cmd->info1 = le32_encode_bits(peer_id, ATH12K_HTT_PRI_LINK_MIGR_PEER_ID) |
		     le32_encode_bits(ml_peer_id, ATH12K_HTT_PRI_LINK_MIGR_ML_PEER_ID);

	/* TODO: Need to update src_info once DS support is added */
	if (src_info != 0)
		src_info_valid = true;

	cmd->info2 = le32_encode_bits(status, ATH12K_HTT_PRI_LINK_MIGR_STATUS) |
		     le32_encode_bits(src_info, ATH12K_HTT_PRI_LINK_MIGR_SRC_INFO) |
		     le32_encode_bits(src_info_valid,
		     		      ATH12K_HTT_PRI_LINK_MIGR_SRC_INFO_VALID);

	ath12k_dbg(ab, ATH12K_DBG_DP_HTT,
		   "htt MLO send pri link migr resp for peer_id 0x%x ml_peer_id 0x%x vdev_id 0x%x pdev_id 0x%x chip_id 0x%x status %u\n",
		   peer_id, ml_peer_id, vdev_id, pdev_id, chip_id, status);

	ret = ath12k_htc_send(&ab->htc, dp->eid, skb);
	if (ret)
		dev_kfree_skb_any(skb);

	return ret;
}
