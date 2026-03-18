// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/ieee80211.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <crypto/hash.h>
#include "../core.h"
#include "hal.h"
#include "../debug.h"
#include "../peer.h"
#include "dp_peer.h"
#include "../hw.h"
#include "../dp_rx.h"
#include "../debugfs_htt_stats.h"
#include "../dp_tx.h"
#include "../dp_mon.h"
#include "hal_rx.h"
#include "dp_rx.h"
#include "hal_qcn9274.h"
#include "hal_wcn7850.h"
#include "../debugfs.h"
#include "../dp.h"

static void ath12k_wifi7_dp_rx_h_err_update_peer_stats(struct ath12k_pdev_dp *dp_pdev,
						       struct ath12k_skb_rxcb *rxcb)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_peer *peer;

	peer = ath12k_dp_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
						   rxcb->peer_id);

	if (peer) {
		int err = rxcb->err_code;
		u8 link_id = ath12k_dp_peer_get_stats_link_id(dp->ab, peer,
							      rxcb->hw_link_id);
		switch (rxcb->err_rel_src) {
		case HAL_WBM_REL_SRC_MODULE_REO:
			DP_PEER_LINK_STATS_CNT(peer, wbm_err.reo_error[err],
					       1, link_id);
			break;
		case HAL_WBM_REL_SRC_MODULE_RXDMA:
			DP_PEER_LINK_STATS_CNT(peer, wbm_err.rxdma_error[err],
					       1, link_id);
			break;
		default:
			break;
		}
	} else {
		DP_DEVICE_STATS_INC(dp, wbm_err.drop[WBM_ERR_INVALID_PEER_ID_ERROR], 1);
	}
}

static void
ath12k_wifi7_dp_rx_null_q_desc_sg_drop(struct ath12k_dp *dp,
				       struct ath12k_pdev_dp *dp_pdev, int msdu_len,
				       struct sk_buff_head *msdu_list)
{
	struct sk_buff *skb, *tmp;
	struct ath12k_skb_rxcb *rxcb;
	int n_buffs;

	n_buffs = DIV_ROUND_UP(msdu_len,
			       (DP_RX_BUFFER_SIZE - dp->ab->hal.hal_desc_sz));

	skb_queue_walk_safe(msdu_list, skb, tmp) {
		rxcb = ATH12K_SKB_RXCB(skb);
		if (rxcb->err_rel_src == HAL_WBM_REL_SRC_MODULE_REO &&
		    rxcb->err_code == HAL_REO_DEST_RING_ERROR_CODE_DESC_ADDR_ZERO) {
			if (!n_buffs)
				break;
			__skb_unlink(skb, msdu_list);
			dev_kfree_skb_any(skb);
			n_buffs--;
		}
	}
}

static int ath12k_wifi7_dp_rx_h_null_q_desc(struct ath12k_pdev_dp *dp_pdev,
					    struct sk_buff *msdu,
					    struct ieee80211_rx_status *status,
					    struct sk_buff_head *msdu_list,
					    struct hal_rx_desc_data *rx_desc_data)
{
	struct rx_msdu_desc_info rx_msdu_info;
	struct rx_mpdu_desc_info rx_mpdu_info;
	struct rx_tlv_info_1 tlv_info = {0};
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	u16 msdu_len = rx_desc_data->msdu_len;
	struct hal_rx_desc *desc = (struct hal_rx_desc *)msdu->data;
	u8 l3pad_bytes = rx_desc_data->l3_pad_bytes;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	u32 hal_rx_desc_sz = dp->ab->hal.hal_desc_sz;
	bool fast_rx = false;
	int ret = 0;
	struct ath12k_dp_peer *peer = NULL;
	struct ath12k_vif *ahvif;

	if (!rxcb->is_frag && ((msdu_len + hal_rx_desc_sz) > DP_RX_BUFFER_SIZE)) {
		/* First buffer will be freed by the caller, so deduct it's length */
		msdu_len = msdu_len - (DP_RX_BUFFER_SIZE - hal_rx_desc_sz);
		ath12k_wifi7_dp_rx_null_q_desc_sg_drop(dp, dp_pdev, msdu_len, msdu_list);
		DP_DEVICE_STATS_INC(dp, wbm_err.drop[WBM_ERR_DROP_SCATTER_GATHER], 1);
		return -EINVAL;
	}

	/* Even after cleaning up the sg buffers in the msdu list with above check
	 * any msdu received with continuation flag needs to be dropped as invalid.
	 * This protects against some random err frame with continuation flag.
	 */
	if (rxcb->is_continuation)
		return -EINVAL;

	if (!rx_desc_data->msdu_done) {
		ath12k_warn(ab,
			    "msdu_done bit not set in null_q_des processing\n");
		__skb_queue_purge(msdu_list);
		return -EIO;
	}

	/* Handle NULL queue descriptor violations arising out a missing
	 * REO queue for a given peer or a given TID. This typically
	 * may happen if a packet is received on a QOS enabled TID before the
	 * ADDBA negotiation for that TID, when the TID queue is setup. Or
	 * it may also happen for MC/BC frames if they are not routed to the
	 * non-QOS TID queue, in the absence of any other default TID queue.
	 * This error can show up both in a REO destination or WBM release ring.
	 */

	if (rxcb->is_frag) {
		skb_pull(msdu, hal_rx_desc_sz);
	} else {
		if ((hal_rx_desc_sz + l3pad_bytes + msdu_len) > DP_RX_BUFFER_SIZE)
			return -EINVAL;

		skb_put(msdu, hal_rx_desc_sz + l3pad_bytes + msdu_len);
		skb_pull(msdu, hal_rx_desc_sz + l3pad_bytes);
	}
	if (unlikely(!ath12k_dp_rx_check_nwifi_hdr_len_valid(dp, rx_desc_data->decap,
							     msdu))) {
		int err_code = WBM_ERR_DROP_INVALID_NWIFI_HDR_LEN;

		DP_DEVICE_STATS_INC(dp, wbm_err.drop[err_code], 1);
		return -EINVAL;
	}

	rx_msdu_info.to_ds = rx_desc_data->is_to_ds;
	rx_msdu_info.fr_ds = rx_desc_data->is_from_ds;
	rx_msdu_info.da_is_mcbc = rx_desc_data->is_mcbc;
	rx_msdu_info.tcp_udp_chksum_fail = rx_desc_data->l4_csum_fail;
	rx_msdu_info.ip_chksum_fail = rx_desc_data->ip_csum_fail;
	rx_mpdu_info.flow_info.peer_id = rxcb->peer_id;
	rx_mpdu_info.tid = rx_desc_data->tid;
	tlv_info.mesh_ctrl_present = rx_desc_data->mesh_ctrl_present;
	tlv_info.decap = rx_desc_data->decap;
	tlv_info.rate_mcs = rx_desc_data->rate_mcs;
	tlv_info.freq = rx_desc_data->freq;
	tlv_info.nss = rx_desc_data->nss;
	tlv_info.pkt_type = rx_desc_data->pkt_type;
	tlv_info.bw = rx_desc_data->bw;
	tlv_info.sgi = rx_desc_data->sgi;

	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, status, &tlv_info,
					(ATH12K_SKB_RXCB(msdu))->err_rel_src);
	if (unlikely(ret))
		return -EINVAL;

	rcu_read_lock();
	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
						   rx_mpdu_info.flow_info.peer_id);
	ret = ath12k_wifi7_dp_rx_h_mpdu(dp_pdev, msdu, desc, status, &rx_msdu_info,
					&rx_mpdu_info, &tlv_info,
					rx_desc_data->err_bitmap, &fast_rx,
					peer);
	if (unlikely(ret)) {
		rcu_read_unlock();
		return -EINVAL;
	}

	rxcb->tid = rx_desc_data->tid;

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		if (peer) {
			ahvif = ath12k_vif_to_ahvif(ath12k_dp_peer_get_vif(peer));
			ath12k_tid_rx_stats(ahvif, rxcb->tid, msdu_len, ATH_RX_HW_PKTS);
		}
	}

	rcu_read_unlock();
	/* Please note that caller will having the access to msdu and completing
	 * rx with mac80211. Need not worry about cleaning up amsdu_list.
	 */

	return 0;
}

static void ath12k_fill_reo_drop_reason(struct ath12k_skb_rxcb *rxcb,
					u32 drop_reason)
{
	switch (rxcb->err_code) {
	case HAL_REO_DEST_RING_ERROR_CODE_DESC_INVALID:
			drop_reason = ATH_RX_DESC_INVALID;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_AMPDU_IN_NON_BA:
			drop_reason = ATH_RX_NON_BA;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_NON_BA_DUPLICATE:
			drop_reason = ATH_RX_NON_BA_DUP;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_BA_DUPLICATE:
			drop_reason = ATH_RX_BA_DUP;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_FRAME_2K_JUMP:
	case HAL_REO_DEST_RING_ERROR_CODE_BAR_2K_JUMP:
			drop_reason = ATH_RX_2K_JUMP;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_FRAME_OOR:
	case HAL_REO_DEST_RING_ERROR_CODE_BAR_OOR:
			drop_reason = ATH_RX_ERR_OOR;
			break;
	case  HAL_REO_DEST_RING_ERROR_CODE_NO_BA_SESSION:
			drop_reason = ATH_RX_NO_BA;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_FRAME_SN_EQUALS_SSN:
			drop_reason = ATH_RX_EQUALS_SSN;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_2K_ERR_FLAG_SET:
	case HAL_REO_DEST_RING_ERROR_CODE_PN_ERR_FLAG_SET:
			drop_reason = ATH_RX_ERR_FLAG_SET;
			break;
	case HAL_REO_DEST_RING_ERROR_CODE_DESC_BLOCKED:
			drop_reason = ATH_RX_DESC_BLOCKED;
			break;
	}
}

static bool ath12k_wifi7_dp_rx_h_reo_err(struct ath12k_pdev_dp *dp_pdev,
					 struct sk_buff *msdu,
					 struct ieee80211_rx_status *status,
					 struct sk_buff_head *msdu_list,
					 struct hal_rx_desc_data *rx_desc_data)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)msdu->data;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k *ar = dp_pdev->ar;
	bool drop = false;
	u32 drop_reason = 0;
	u16 msdu_len;

	DP_DEVICE_STATS_INC(dp, wbm_err.reo_error[rxcb->err_code], 1);
	ath12k_wifi7_dp_rx_h_err_update_peer_stats(dp_pdev, rxcb);

	switch (rxcb->err_code) {
	case HAL_REO_DEST_RING_ERROR_CODE_DESC_ADDR_ZERO:
		if (ath12k_wifi7_dp_rx_h_null_q_desc(dp_pdev, msdu, status, msdu_list,
						     rx_desc_data)) {
			drop = true;
			drop_reason = ATH_RX_NULL_Q_DESC;
		}

		/* CCE rule configured for the ERP feature on REO RELEASE RING gets
		 * handled under this specific error code
		 */
		if (!drop && ar->erp_trigger_set)
			queue_work(ar->ab->workqueue, &ar->erp_handle_trigger_work);

		break;
	case HAL_REO_DEST_RING_ERROR_CODE_PN_CHECK_FAILED:
		/* TODO: Do not drop PN failed packets in the driver;
		 * instead, it is good to drop such packets in mac80211
		 * after incrementing the replay counters.
		 */
		fallthrough;
	default:
		/* TODO: Review other errors and process them to mac80211
		 * as appropriate.
		 */
		DP_DEVICE_STATS_INC(dp, wbm_err.drop[WBM_ERR_DROP_REO_GENERIC], 1);
		drop_reason = ATH_RX_REO_ERR;
		if (ath12k_dp_stats_enabled(dp_pdev) &&
		    ath12k_tid_stats_enabled(dp_pdev))
			ath12k_fill_reo_drop_reason(rxcb, drop_reason);
		drop = true;
		break;
	}

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		msdu_len = le32_get_bits(rx_desc->u.qcn9274.msdu_end.info10,
					 RX_MSDU_END_INFO10_MSDU_LENGTH);

		rcu_read_lock();
		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
							      rxcb->peer_id);
		if (dp_peer) {
			ahvif = ath12k_vif_to_ahvif(ath12k_dp_peer_get_vif(dp_peer));
			if (drop)
				ath12k_tid_drop_rx_stats(ahvif, rxcb->tid, msdu_len,
							 drop_reason);
			else
				ath12k_tid_rx_stats(ahvif, rxcb->tid, msdu_len,
						    ATH_RX_REO_ERR_PKTS);
		}
		rcu_read_unlock();
	}

	return drop;
}

static bool ath12k_wifi7_dp_rx_h_tkip_mic_err(struct ath12k_pdev_dp *dp_pdev,
					      struct sk_buff *msdu,
					      struct ieee80211_rx_status *status,
					      struct hal_rx_desc_data *rx_desc_data)
{
	struct rx_msdu_desc_info rx_msdu_info;
	struct rx_tlv_info_1 tlv_info;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	u16 msdu_len = rx_desc_data->msdu_len;
	struct hal_rx_desc *desc = (struct hal_rx_desc *)msdu->data;
	u8 l3pad_bytes = rx_desc_data->l3_pad_bytes;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	u32 hal_rx_desc_sz = dp->ab->hal.hal_desc_sz;
	int ret;

	rxcb->is_first_msdu = rx_desc_data->is_first_msdu;
	rxcb->is_last_msdu = rx_desc_data->is_last_msdu;

	if ((hal_rx_desc_sz + l3pad_bytes + msdu_len) > DP_RX_BUFFER_SIZE) {
		ath12k_warn(ab, "invalid msdu len in tkip mirc err %u\n", msdu_len);
		ath12k_dbg_dump(ab, ATH12K_DBG_DATA, NULL, "", desc,
				sizeof(struct hal_rx_desc));
		return true;
	}

	skb_put(msdu, hal_rx_desc_sz + l3pad_bytes + msdu_len);
	skb_pull(msdu, hal_rx_desc_sz + l3pad_bytes);

	if (unlikely(!ath12k_dp_rx_check_nwifi_hdr_len_valid(dp, rx_desc_data->decap,
							     msdu)))
		return true;

	rx_msdu_info.to_ds = rx_desc_data->is_to_ds;
	rx_msdu_info.fr_ds = rx_desc_data->is_from_ds;
	rx_msdu_info.da_is_mcbc = rx_desc_data->is_mcbc;
	tlv_info.mesh_ctrl_present = rx_desc_data->mesh_ctrl_present;
	tlv_info.decap = rx_desc_data->decap;
	tlv_info.freq = rx_desc_data->freq;
	tlv_info.pkt_type = rx_desc_data->pkt_type;
	tlv_info.bw = rx_desc_data->bw;
	tlv_info.sgi = rx_desc_data->sgi;
	tlv_info.rate_mcs = rx_desc_data->rate_mcs;
	tlv_info.nss = rx_desc_data->nss;

	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, status, &tlv_info,
					(ATH12K_SKB_RXCB(msdu))->err_rel_src);
	if (unlikely(ret))
		return true;

	status->flag |= (RX_FLAG_MMIC_STRIPPED | RX_FLAG_MMIC_ERROR |
				     RX_FLAG_DECRYPTED);

	ret = ath12k_wifi7_dp_rx_h_undecap(dp_pdev, msdu, desc,
					   HAL_ENCRYPT_TYPE_TKIP_MIC, status, false,
					   false, &rx_msdu_info, &tlv_info, NULL,
					   rxcb->peer_id,
					   rx_desc_data->tid);
	if (ret)
		return true;

	return false;
}

static bool ath12k_dp_rx_h_mec_drop(struct ath12k_pdev_dp *dp_pdev,
				    struct hal_rx_desc_data *rx_desc_data)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_link_sta *arsta = NULL;
	struct ath12k_dp_link_peer *peer;

	rcu_read_lock();
	peer = ath12k_dp_link_peer_find_by_peerid_index(dp_pdev->dp, dp_pdev,
							rx_desc_data->peer_id);
	if (!peer)
		goto drop;

	if (ath12k_dp_link_peer_get_vif(peer) &&
	    ath12k_dp_link_peer_get_vif_type(peer) != NL80211_IFTYPE_STATION) {
		ath12k_warn(ab, "vif type is not station for peer with peer_id %u\n",
			    rx_desc_data->peer_id);
		goto drop;
	}

	if (peer && ath12k_dp_link_peer_get_sta(peer)) {
		arsta = ath12k_peer_get_link_sta(ab, peer);
		if (arsta) {
			spin_lock_bh(&arsta->arvif->link_stats_lock);
			arsta->arvif->link_stats.rx_dropped++;
			spin_unlock_bh(&arsta->arvif->link_stats_lock);
		}
	}
drop:
	rcu_read_unlock();
	return true;
}

static inline void
ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(struct ath12k_dp *dp,
					struct sk_buff *msdu,
					enum ath12k_wbm_err_drop_reason drop_reason)
{
	DP_DEVICE_STATS_INC(dp, wbm_err.drop[drop_reason], 1);
	dev_kfree_skb_any(msdu);
}

static int ath12k_wifi7_dp_rx_h_unauth_wds_err(struct ath12k_pdev_dp *dp_pdev,
					       struct sk_buff *msdu,
					       struct ieee80211_rx_status *status,
					       struct hal_rx_desc_data *rx_desc_data)
{
	struct hal_rx_desc *desc = (struct hal_rx_desc *)msdu->data;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	struct ath12k_dp *dp = dp_pdev->dp;
	u32 hdr_len, hal_rx_desc_sz = dp->ab->hal.hal_desc_sz;
	u8 l3pad_bytes = rx_desc_data->l3_pad_bytes;
	u16 msdu_len = rx_desc_data->msdu_len;
	struct rx_msdu_desc_info rx_msdu_info;
	struct rx_mpdu_desc_info rx_mpdu_info;
	struct ath12k_dp_rx_rfc1042_hdr *llc;
	struct ath12k_dp_peer *peer = NULL;
	struct rx_tlv_info_1 tlv_info;
	struct ieee80211_hdr *hdr;
	bool fast_rx = false;
	bool drop = false;
	bool is_null;
	int ret;

	rcu_read_lock();
	peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
						   rxcb->peer_id);
	if (!peer) {
		ath12k_dbg(dp->ab, ATH12K_DBG_DATA,
			   "failed to find the peer to process unauth wds err handling peer_id %d\n",
			   rxcb->peer_id);
		drop = true;
		goto exit;
	}

	if ((hal_rx_desc_sz + l3pad_bytes + msdu_len) > DP_RX_BUFFER_SIZE) {
		drop = true;
		goto exit;
	}

	skb_put(msdu, hal_rx_desc_sz + l3pad_bytes + msdu_len);
	skb_pull(msdu, hal_rx_desc_sz + l3pad_bytes);

	if (unlikely(!ath12k_dp_rx_check_nwifi_hdr_len_valid(dp, rx_desc_data->decap,
							     msdu))) {
		drop = true;
		goto exit;
	}

	rx_msdu_info.to_ds = rx_desc_data->is_to_ds;
	rx_msdu_info.fr_ds = rx_desc_data->is_from_ds;
	rx_msdu_info.da_is_mcbc = rx_desc_data->is_mcbc;
	rx_msdu_info.tcp_udp_chksum_fail = rx_desc_data->l4_csum_fail;
	rx_msdu_info.ip_chksum_fail = rx_desc_data->ip_csum_fail;
	rx_mpdu_info.flow_info.peer_id = rxcb->peer_id;
	rx_mpdu_info.tid = rx_desc_data->tid;
	tlv_info.mesh_ctrl_present = rx_desc_data->mesh_ctrl_present;
	tlv_info.decap = rx_desc_data->decap;
	tlv_info.freq = rx_desc_data->freq;
	tlv_info.pkt_type = rx_desc_data->pkt_type;
	tlv_info.bw = rx_desc_data->bw;
	tlv_info.sgi = rx_desc_data->sgi;
	tlv_info.rate_mcs = rx_desc_data->rate_mcs;
	tlv_info.nss = rx_desc_data->nss;

	ret = ath12k_wifi7_dp_rx_h_ppdu(dp_pdev, status, &tlv_info,
					(ATH12K_SKB_RXCB(msdu))->err_rel_src);
	if (unlikely(ret)) {
		drop = true;
		goto exit;
	}

	ret = ath12k_wifi7_dp_rx_h_mpdu(dp_pdev, msdu, desc, status, &rx_msdu_info,
					&rx_mpdu_info, &tlv_info,
					rx_desc_data->err_bitmap, &fast_rx,
					peer);
	if (unlikely(ret)) {
		drop = true;
		goto exit;
	}

	rxcb->tid = rx_desc_data->tid;

	hdr = (struct ieee80211_hdr *)msdu->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);
	llc = (struct ath12k_dp_rx_rfc1042_hdr *)(msdu->data + hdr_len);
	is_null = ieee80211_is_qos_nullfunc(hdr->frame_control);

	if (!(llc->snap_type == cpu_to_be16(ETH_P_PAE) || is_null))
		drop = true;

exit:
	rcu_read_unlock();
	return drop;
}

static bool ath12k_wifi7_dp_rx_h_rxdma_err(struct ath12k_pdev_dp *dp_pdev,
					   struct sk_buff *msdu,
					   struct ieee80211_rx_status *status,
					   struct hal_rx_desc_data *rx_desc_data)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)msdu->data;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_peer *dp_peer;
	u32 drop_reason = 0;
	u16 msdu_len;
	bool drop = false;

	DP_DEVICE_STATS_INC(dp, wbm_err.rxdma_error[rxcb->err_code], 1);
	ath12k_wifi7_dp_rx_h_err_update_peer_stats(dp_pdev, rxcb);

	switch (rxcb->err_code) {
	case HAL_REO_ENTR_RING_RXDMA_ECODE_UNAUTH_WDS_ERR:
		drop = ath12k_wifi7_dp_rx_h_unauth_wds_err(dp_pdev, msdu, status,
							   rx_desc_data);
		if (drop)
			drop_reason = ATH_RX_UNAUTH_WDS_ERR;
		break;
	case HAL_REO_ENTR_RING_RXDMA_ECODE_MULTICAST_ECHO_ERR:
		drop = ath12k_dp_rx_h_mec_drop(dp_pdev, rx_desc_data);
		if (drop)
			drop_reason = ATH_RX_ECHO_ERR;
		break;
	case HAL_REO_ENTR_RING_RXDMA_ECODE_DECRYPT_ERR:
	case HAL_REO_ENTR_RING_RXDMA_ECODE_TKIP_MIC_ERR:
		if (rx_desc_data->err_bitmap & HAL_RX_MPDU_ERR_TKIP_MIC) {
			drop = ath12k_wifi7_dp_rx_h_tkip_mic_err(dp_pdev, msdu, status,
								 rx_desc_data);
			break;
		}
		fallthrough;
	default:
		/* TODO: Review other rxdma error code to check if anything is
		 * worth reporting to mac80211
		 */
		drop = true;
		drop_reason = ATH_RX_RXDMA_ERR;
		break;
	}

	if (ath12k_dp_stats_enabled(dp_pdev) &&
	    ath12k_tid_stats_enabled(dp_pdev)) {
		msdu_len = le32_get_bits(rx_desc->u.qcn9274.msdu_end.info10,
					 RX_MSDU_END_INFO10_MSDU_LENGTH);

		rcu_read_lock();
		dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev,
							      rxcb->peer_id);
		if (dp_peer) {
			ahvif = ath12k_vif_to_ahvif(ath12k_dp_peer_get_vif(dp_peer));
			if (drop)
				ath12k_tid_drop_rx_stats(ahvif, rxcb->tid, msdu_len,
							 drop_reason);
			else
				ath12k_tid_rx_stats(ahvif, rxcb->tid, msdu_len,
						    ATH_RX_RXDMA_PKTS);
		}
		rcu_read_unlock();
	}

	return drop;
}

static void ath12k_wifi7_dp_rx_wbm_err(struct ath12k_pdev_dp *dp_pdev,
				       struct napi_struct *napi,
				       struct sk_buff *msdu,
				       struct sk_buff_head *msdu_list)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(msdu);
	struct hal_rx_desc_data rx_desc_data = {0};
	struct ieee80211_rx_status rxs = {0};
	bool drop = true;
	struct ieee80211_hdr *hdr;
	struct ath12k_dp_rx_rfc1042_hdr *llc;
	enum ath12k_dp_eapol_key_type subtype;
	size_t hdr_len;
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)msdu->data;

	ath12k_wifi7_dp_extract_rx_desc_data(dp, &rx_desc_data, rx_desc, rx_desc);

	switch (rxcb->err_rel_src) {
	case HAL_WBM_REL_SRC_MODULE_REO:
		drop = ath12k_wifi7_dp_rx_h_reo_err(dp_pdev, msdu, &rxs, msdu_list,
						    &rx_desc_data);
		break;
	case HAL_WBM_REL_SRC_MODULE_RXDMA:
		drop = ath12k_wifi7_dp_rx_h_rxdma_err(dp_pdev, msdu, &rxs, &rx_desc_data);
		if (drop)
			DP_DEVICE_STATS_INC(dp,
					    wbm_err.drop[WBM_ERR_DROP_RXDMA_GENERIC], 1);
		break;
	default:
		/* msdu will get freed */
		break;
	}

	if (drop) {
		dev_kfree_skb_any(msdu);
		return;
	}
	rxs.flag |= RX_FLAG_SKIP_MONITOR;

	hdr = (struct ieee80211_hdr *)msdu->data;
	hdr_len = ieee80211_hdrlen(hdr->frame_control);
	llc = (struct ath12k_dp_rx_rfc1042_hdr *)(msdu->data + hdr_len);
	if (llc->snap_type == cpu_to_be16(ETH_P_PAE)) {
		u8 *data = msdu->data + hdr_len + LLC_SNAP_HDR_LEN;

		dp->device_stats.rx_eapol[ab->device_id]++;
		subtype = ath12k_dp_get_eapol_subtype(data);
		if (subtype != DP_EAPOL_KEY_TYPE_MAX && subtype > 0) {
			dp->device_stats.rx_eapol_type[subtype - 1][ab->device_id]++;
			ath12k_dbg_level(ab, ATH12K_DBG_EAPOL, ATH12K_DBG_L0,
					 "Received %s%d EAPOL frame from STA %pM\n",
					 subtype <= 4 ? "M" : "G",
					 subtype <= 4 ? subtype : (subtype - 4),
					 hdr->addr2);
		}
	}

	ath12k_dp_rx_deliver_msdu(dp_pdev, napi, msdu, &rxs, rxcb->hw_link_id,
				  rx_desc_data.is_mcbc, rx_desc_data.peer_id,
				  rx_desc_data.tid);
}

int ath12k_wifi7_dp_rx_process_wbm_err(struct ath12k_dp *dp,
				       struct napi_struct *napi, int budget)
{
	struct list_head rx_desc_used_list[ATH12K_MAX_SOCS];
	struct ath12k *ar;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_pdev_dp *dp_pdev; //TODO: Check this
	struct ath12k_dp *partner_dp;
	struct dp_rxdma_ring *rx_ring;
	struct hal_rx_wbm_rel_info err_info;
	struct hal_srng *srng;
	struct hal_srng *refill_srng;
	struct sk_buff *msdu;
	struct sk_buff_head msdu_list, scatter_msdu_list;
	struct ath12k_skb_rxcb *rxcb;
	void *rx_desc;
	int num_buffs_reaped[ATH12K_MAX_SOCS] = {};
	int total_num_buffs_reaped = 0;
	struct ath12k_rx_desc_info *desc_info;
	struct ath12k_device_dp_stats *device_stats = &dp->device_stats;
	struct ath12k_dp_hw_link *hw_links = dp_hw_grp->hw_links;
	u8 hw_link_id, device_id;
	int ret, pdev_id;
	struct hal_rx_desc *msdu_data;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_peer *dp_peer;
	struct ieee80211_vif *vif;
	u16 peer_id;
	int drop_reason;

	__skb_queue_head_init(&msdu_list);
	__skb_queue_head_init(&scatter_msdu_list);

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++)
		INIT_LIST_HEAD(&rx_desc_used_list[device_id]);

	srng = &ab->hal.srng_list[dp->rx_rel_ring.ring_id];
	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	while (budget) {
		rx_desc = ath12k_hal_srng_dst_get_next_entry(ab, srng);
		if (!rx_desc)
			break;

		ret = ath12k_wifi7_hal_wbm_desc_parse_err(dp, rx_desc,
							  &err_info);
		if (ret) {
			DP_DEVICE_STATS_INC(dp,
					    wbm_err.drop[WBM_ERR_DESC_PARSE_ERROR], 1);
			ath12k_warn(ab,
				    "failed to parse rx error in wbm_rel ring desc %d\n",
				    ret);
			continue;
		}

		desc_info = err_info.rx_desc;

		/* retry manual desc retrieval if hw cc is not done */
		if (!desc_info) {
			int err = WBM_ERR_GET_SW_DESC_FROM_CK_ERROR;

			DP_DEVICE_STATS_INC(dp, wbm_err.drop[err], 1);
			desc_info = ath12k_dp_get_rx_desc(dp, err_info.cookie);
			if (!desc_info) {
				err = WBM_ERR_GET_SW_DESC_ERROR;
				DP_DEVICE_STATS_INC(dp, wbm_err.drop[err], 1);
				ath12k_warn(ab, "Invalid cookie in DP WBM rx error descriptor retrieval: 0x%x\n",
					    err_info.cookie);
				continue;
			}
		}

		if (desc_info->magic != ATH12K_DP_RX_DESC_MAGIC)
			ath12k_warn(ab, "WBM RX err, Check HW CC implementation");

		msdu = desc_info->skb;
		desc_info->skb = NULL;

		IPA_SET_RX_BUF_SMMU_UNMAP(dp->ab, msdu, true);

		device_id = desc_info->device_id;
		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
		if (unlikely(!partner_dp)) {
			drop_reason = WBM_ERR_DROP_NULL_PARTNER_DP;
			ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(dp, msdu, drop_reason);

			/* In any case continuation bit is set
			 * in the previous record, cleanup scatter_msdu_list
			 */
			ath12k_wifi7_dp_clean_up_skb_list(&scatter_msdu_list);
			continue;
		}

		list_add_tail(&desc_info->list, &rx_desc_used_list[device_id]);

		rxcb = ATH12K_SKB_RXCB(msdu);

		num_buffs_reaped[device_id]++;
		total_num_buffs_reaped++;

		if (!err_info.continuation)
			budget--;

		msdu_data = (struct hal_rx_desc *)msdu->data;
		rxcb->err_rel_src = err_info.err_rel_src;
		rxcb->err_code = err_info.err_code;
		rxcb->is_first_msdu = err_info.first_msdu;
		rxcb->is_last_msdu = err_info.last_msdu;
		rxcb->is_continuation = err_info.continuation;
		rxcb->is_frag = desc_info->is_frag;
		rxcb->peer_id =
		ath12k_wifi7_dp_rx_get_peer_id(ab, dp->peer_metadata_ver,
					       err_info.peer_metadata);
		rxcb->rx_desc = msdu_data;

		desc_info->is_frag = 0;

		if (err_info.continuation) {
			__skb_queue_tail(&scatter_msdu_list, msdu);
			continue;
		}

		hw_link_id = ath12k_wifi7_dp_rx_get_msdu_src_link(partner_dp,
								  rxcb->rx_desc);

		if (hw_link_id >= ATH12K_GROUP_MAX_RADIO) {
			drop_reason = WBM_ERR_DROP_INVALID_HW_ID;

			ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(dp, msdu, drop_reason);

			/* In any case continuation bit is set
			 * in the previous record, cleanup scatter_msdu_list
			 */
			ath12k_wifi7_dp_clean_up_skb_list(&scatter_msdu_list);
			continue;
		}

		if (!skb_queue_empty(&scatter_msdu_list)) {
			struct sk_buff *msdu;

			skb_queue_walk(&scatter_msdu_list, msdu) {
				rxcb = ATH12K_SKB_RXCB(msdu);
				rxcb->hw_link_id = hw_link_id;
			}

			skb_queue_splice_tail_init(&scatter_msdu_list,
						   &msdu_list);
		}

		rxcb = ATH12K_SKB_RXCB(msdu);
		rxcb->hw_link_id = hw_link_id;
		__skb_queue_tail(&msdu_list, msdu);
	}

	/* In any case continuation bit is set in the
	 * last record, cleanup scatter_msdu_list
	 */
	ath12k_wifi7_dp_clean_up_skb_list(&scatter_msdu_list);

	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	if (!total_num_buffs_reaped)
		goto done;

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++) {
		if (!num_buffs_reaped[device_id])
			continue;

		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
		rx_ring = &partner_dp->rx_refill_buf_ring;
		refill_srng =
			&partner_dp->ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];
		ath12k_dp_rx_bufs_replenish(partner_dp, refill_srng,
					    &rx_desc_used_list[device_id], false);
	}

	rcu_read_lock();
	while ((msdu = __skb_dequeue(&msdu_list))) {
		rxcb = ATH12K_SKB_RXCB(msdu);
		hw_link_id = rxcb->hw_link_id;

		device_id = hw_links[hw_link_id].device_id;
		partner_dp = ath12k_dp_hw_grp_to_dp(dp_hw_grp, device_id);
		if (unlikely(!partner_dp)) {
			ath12k_dbg(ab, ATH12K_DBG_DATA,
				   "Unable to process WBM error msdu due to invalid hw link id %d device id %d\n",
				   hw_link_id, device_id);
			drop_reason = WBM_ERR_DROP_PROCESS_NULL_PARTNER_DP;
			ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(dp, msdu, drop_reason);
			continue;
		}

		pdev_id = ath12k_hw_mac_id_to_pdev_id(partner_dp->hw_params,
						      hw_links[hw_link_id].pdev_idx);
		dp_pdev = ath12k_dp_to_dp_pdev(partner_dp, pdev_id);
		if (!dp_pdev) {
			drop_reason = WBM_ERR_DROP_NULL_PDEV;
			ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(dp, msdu, drop_reason);
			continue;
		}
		ar = dp_pdev->ar;

		if (!ar || !rcu_dereference(ar->ab->pdevs_active[pdev_id])) {
			drop_reason = WBM_ERR_DROP_NULL_AR;
			ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(dp, msdu, drop_reason);
			continue;
		}

		if (test_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags)) {
			drop_reason = WBM_ERR_DROP_CAC_RUNNING;
			ath12k_wifi7_dp_rx_wbm_err_dev_free_skb(dp, msdu, drop_reason);
			continue;
		}

		if (rxcb->err_rel_src < HAL_WBM_REL_SRC_MODULE_MAX) {
			u8 src, device_id;

			if (ath12k_dp_stats_enabled(dp_pdev) &&
			    ath12k_tid_stats_enabled(dp_pdev)) {
				rcu_read_lock();
				peer_id = rxcb->peer_id;
				dp_peer = ath12k_dp_peer_find_by_peerid_index(dp,
									      dp_pdev,
									      peer_id);
				if (dp_peer) {
					vif = ath12k_dp_peer_get_vif(dp_peer);
					ahvif = ath12k_vif_to_ahvif(vif);
					ath12k_tid_rx_stats(ahvif, rxcb->tid, msdu->len,
							    ATH_RX_TOTAL_PKTS);
					ath12k_tid_rx_stats(ahvif, rxcb->tid, msdu->len,
							    ATH_RX_WBM_REL_TOTAL);
				}
				rcu_read_unlock();
			}
			src = rxcb->err_rel_src;
			device_id = ar->ab->device_id;
			device_stats->rx_wbm_rel_source[src][device_id]++;
		}

		ath12k_wifi7_dp_rx_wbm_err(dp_pdev, napi, msdu, &msdu_list);
	}

	rcu_read_unlock();
done:
	return total_num_buffs_reaped;
}
