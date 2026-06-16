// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "../core.h"
#include "../debug.h"
#include "../dp_cmn.h"
#include "../dp_peer.h"
#include "dp.h"
#include "dp_peer.h"
#include "dp_tx_queue.h"
#include "dp_tx_flow_info.h"
#include "../telemetry_agent_if.h"
#include "dp_tx.h"
#include "../dp_tx.h"
#include "dp_telemetry.h"

#define ATH12K_DP_MAX_SEQ_NUM	0xFFF
#define ATH12K_DP_MAX_POSSIBLE_BA_WIN	0x400
#define ATH12K_DP_INVALID_MLSN_OFFSET	0xFFFF

static u16 ath12k_wifi8_peer_id_alloc(struct ath12k_dp_hw *dp_hw)
{
	u16 peer_id;
	int i;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	peer_id = dp_hw->last_peer_id;
	for (i = 0; i < ATH12K_MAX_PEER_ID; i++) {
		peer_id = (peer_id + 1) % ATH12K_MAX_PEER_ID;

		if (!peer_id)
			continue;

		if (test_bit(peer_id, dp_hw->free_peer_id_map))
			continue;

		set_bit(peer_id, dp_hw->free_peer_id_map);
		break;
	}

	dp_hw->last_peer_id = peer_id;
	if (i >= ATH12K_MAX_PEER_ID)
		peer_id = ATH12K_MLO_PEER_ID_INVALID;

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	ath12k_dbg(NULL, ATH12K_DBG_PEER, "Allocated peer_id:%d", peer_id);

	return peer_id;
}

static u16 ath12k_wifi8_sta_id_alloc(struct ath12k_dp_hw *dp_hw)
{
	u16 sta_id;
	int i;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	sta_id = dp_hw->last_sta_id;
	for (i = 0; i < ATH12K_MAX_STA_ID; i++) {
		sta_id = (sta_id + 1) % ATH12K_MAX_STA_ID;

		if (test_bit(sta_id, dp_hw->free_sta_id_map))
			continue;

		set_bit(sta_id, dp_hw->free_sta_id_map);
		break;
	}

	dp_hw->last_sta_id = sta_id;
	if (i >= ATH12K_MAX_STA_ID)
		sta_id = ATH12K_STA_ID_INVALID;

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	ath12k_dbg(NULL, ATH12K_DBG_PEER, "Allocated sta_id:%d", sta_id);

	return sta_id;
}

static u16
ath12k_wifi8_ucast_stats_id_alloc(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
				  u16 dp_peer_id, u8 tid, u8 hw_link_id,
				  unsigned int num_peer)
{
	u16 stats_id;
	u16 max_stats_id = ATH12K_DEFAULT_UCAST_STATS_ID;
	int i;

	if (num_peer >= ATH12K_DEFAULT_UCAST_STATS_ID)
		max_stats_id = ATH12K_MAX_EXT_UCAST_STATS_ID;

	stats_id = dp_hw_grp_wifi8->last_ucast_stats_id;

	for (i = 0; i < max_stats_id; i++) {
		stats_id++;

		if (stats_id >= max_stats_id)
			stats_id = 0;

		if (test_bit(stats_id, dp_hw_grp_wifi8->free_stats_id))
			continue;

		set_bit(stats_id, dp_hw_grp_wifi8->free_stats_id);
		dp_hw_grp_wifi8->last_ucast_stats_id = stats_id;
		dp_hw_grp_wifi8->stats_id_map[stats_id].dp_peer_id = dp_peer_id;
		dp_hw_grp_wifi8->stats_id_map[stats_id].tid = tid;
		dp_hw_grp_wifi8->stats_id_map[stats_id].hw_link_id = hw_link_id;

		ath12k_dbg(NULL, ATH12K_DBG_PEER, "allocated stats_id:%d",
			   stats_id);
		return stats_id;
	}
	return ATH12K_MAX_STATS_ID;
}

static u16
ath12k_wifi8_downlink_gcast_stats_id(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
				     u16 dp_peer_id, u8 tid, u8 hw_link_id)
{
	u16 stats_id;
	int i;

	/* If last_gcast_stats_id is out of range, start with minimum value */
	if (dp_hw_grp_wifi8->last_gcast_stats_id < ATH12K_MIN_DGCAST_STATS_ID ||
	    dp_hw_grp_wifi8->last_gcast_stats_id >= ATH12K_MAX_DGCAST_STATS_ID - 1)
		stats_id = ATH12K_MIN_DGCAST_STATS_ID - 1;
	else
		stats_id = dp_hw_grp_wifi8->last_gcast_stats_id;

	for (i = 0; i < ATH12K_MAX_DGCAST_STATS_ID - ATH12K_MIN_DGCAST_STATS_ID; i++) {
		stats_id++;

		if (stats_id >= ATH12K_MAX_DGCAST_STATS_ID)
			stats_id = ATH12K_MIN_DGCAST_STATS_ID;

		if (test_bit(stats_id, dp_hw_grp_wifi8->free_stats_id))
			continue;

		set_bit(stats_id, dp_hw_grp_wifi8->free_stats_id);
		dp_hw_grp_wifi8->last_gcast_stats_id = stats_id;
		dp_hw_grp_wifi8->stats_id_map[stats_id].dp_peer_id = dp_peer_id;
		dp_hw_grp_wifi8->stats_id_map[stats_id].tid = tid;
		dp_hw_grp_wifi8->stats_id_map[stats_id].hw_link_id = hw_link_id;

		ath12k_dbg(NULL, ATH12K_DBG_PEER, "allocated stats_id:%d", stats_id);
		return stats_id;
	}
	return ATH12K_MAX_STATS_ID;
}

static void
ath12k_wifi8_stats_id_map_update_hw_link(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
						 struct ath12k_dp_peer *dp_peer,
						 struct ath12k_dp_link_peer *removed_peer)
{
	struct stats_to_peer_id_map *stats_map;
	struct ath12k_dp_link_peer *link_peer;
	u8 hw_link_id = ATH12K_INVALID_LINK_ID;
	int i;

	if (!dp_peer || dp_peer->stats_id >= ATH12K_MAX_STATS_ID)
		return;

	stats_map = &dp_hw_grp_wifi8->stats_id_map[dp_peer->stats_id];
	if (stats_map->dp_peer_id != dp_peer->peer_id ||
	    stats_map->hw_link_id != removed_peer->hw_link_id)
		return;

	rcu_read_lock();
	for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
		link_peer = rcu_dereference(dp_peer->link_peers[i]);
		if (!link_peer || link_peer == removed_peer)
			continue;

		hw_link_id = link_peer->hw_link_id;
		break;
	}
	rcu_read_unlock();

	stats_map->hw_link_id = hw_link_id;
}

static u16
ath12k_wifi8_link_band_id_alloc(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8)
{
	u16 link_band_id;
	int i;

	for (i = 0; i < ATH12K_MAX_STATS_ID; i++) {
		link_band_id = i;

		if (test_bit(link_band_id, dp_hw_grp_wifi8->free_link_band_id))
			continue;

		set_bit(link_band_id, dp_hw_grp_wifi8->free_link_band_id);
		ath12k_dbg(NULL, ATH12K_DBG_PEER, "allocated link_band_id:%d",
			   link_band_id);
		return link_band_id;
	}

	return ATH12K_MAX_STATS_ID;
}

int ath12k_wifi8_dp_peer_create(struct ath12k_hw *ah, u8 *addr,
				struct ath12k_dp_peer_create_params *params,
				struct ieee80211_vif *vif)
{
	u8 i = 0, tid;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	struct wireless_dev *wdev;
	struct ath12k_sta *ahsta = NULL;
	struct ath12k_pdev_dp *dp_pdev;
	int ret;
	struct ath12k_dp_rx_tid *rx_tid;
	unsigned int num_peers;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(ah->ag->dp_hw_grp);

	if (params->sta)
		ahsta = ath12k_sta_to_ahsta(params->sta);

	spin_lock_bh(&dp_hw->peer_hash_lock);
	if (!params->is_vdev_peer)
		dp_peer = ath12k_dp_peer_create_find(dp_hw, addr, params->sta,
						     params->is_mlo);
	else
		dp_peer = ath12k_dp_vdev_peer_check(dp_hw, addr, params->hw_link_id);

	if (dp_peer) {
		ath12k_hw_warn(ah, "wifi8: dp peer already exists %pM vdev_peer %d\n",
			       addr, dp_peer->is_vdev_peer);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -EEXIST;
	}

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	dp_peer = kzalloc(sizeof(*dp_peer), GFP_KERNEL);
	if (!dp_peer)
		return -ENOMEM;

	dp_peer->qos = kzalloc(sizeof(*dp_peer->qos), GFP_KERNEL);
	if (!dp_peer->qos) {
		kfree(dp_peer);
		return -ENOMEM;
	}

	rcu_read_lock();
	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(ah->ag->dp_hw_grp, params->hw_link_id);
	ret = ath12k_dp_peer_stats_alloc(dp_peer, dp_pdev);
	if (ret) {
		rcu_read_unlock();
		kfree(dp_peer->qos);
		kfree(dp_peer);
		return ret;
	}
	rcu_read_unlock();

	for (tid = 0; tid < ATH12K_MAX_TIDS; tid++) {
		rx_tid = &dp_peer->rx_tid[tid];
		spin_lock_init(&rx_tid->tid_lock);
	}

	spin_lock_init(&dp_peer->qos->lock);
	spin_lock_init(&dp_peer->keys_lock);
	dp_peer->sta_id = ATH12K_STA_ID_INVALID;
	ether_addr_copy(dp_peer->addr, addr);
	dp_peer->sta = params->sta;
	dp_peer->vif = vif;
	dp_peer->is_mlo = params->is_mlo;
	dp_peer->peer_id = ath12k_wifi8_peer_id_alloc(dp_hw);
	if (dp_peer->peer_id == ATH12K_MLO_PEER_ID_INVALID) {
		ath12k_dp_peer_stats_free(dp_peer);
		kfree(dp_peer->qos);
		kfree(dp_peer);
		return -ENOMEM;
	}

	if (!params->is_vdev_peer) {
		dp_peer->sta_id = ath12k_wifi8_sta_id_alloc(dp_hw);
		if (dp_peer->sta_id == ATH12K_STA_ID_INVALID) {
			spin_lock_bh(&dp_hw->peer_hash_lock);
			clear_bit(dp_peer->peer_id, dp_hw->free_peer_id_map);
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			kfree(dp_peer->qos);
			ath12k_dp_peer_stats_free(dp_peer);
			kfree(dp_peer);
			return -ENOMEM;
		}
	}

	dp_peer->is_vdev_peer = params->is_vdev_peer;
	dp_peer->is_sta_bss_peer = params->is_sta_bss_peer;
	dp_peer->hw_link_id = ATH12K_INVALID_HW_LINKID;

	dp_peer->sec_type = HAL_ENCRYPT_TYPE_OPEN;
	dp_peer->sec_type_grp = HAL_ENCRYPT_TYPE_OPEN;

	for (i = 0; i < ATH12K_NUM_MAX_LINKS; i++)
		dp_peer->l2h_link_map[i] = ATH12K_DP_HW_LINK_ID_INVALID;

	/* Update hw_link_id for self bss peer */
	if (dp_peer->is_vdev_peer) {
		dp_peer->hw_link_id = params->hw_link_id;
	} else {
		ahsta->dp_peer_id = dp_peer->peer_id;
		rcu_assign_pointer(ahsta->dp_peer, dp_peer);
		dp_peer->assoc_hw_link_id = params->hw_link_id;
	}

	/* cache net dev here and reuse it during process rx */
	wdev = ieee80211_vif_to_wdev(vif);
	if (wdev) {
		dp_peer->dev = wdev->netdev;
		if (params->is_sta_bss_peer)
			dp_peer->is_sta_bss_peer_4addr = wdev->use_4addr;
	}

	spin_lock_bh(&dp_hw->peer_hash_lock);

	/* Assigning telemetry specific id's for stats update */
	if (dp_peer->is_vdev_peer) {
		dp_peer->stats_id =
			ath12k_wifi8_downlink_gcast_stats_id(dp_hw_grp_wifi8,
							     dp_peer->peer_id,
							     ATH12K_INVALID_TID,
							     params->hw_link_id);
	} else {
		num_peers = bitmap_weight(dp_hw->free_peer_id_map, ATH12K_MAX_PEER_ID);
		dp_peer->stats_id =
			ath12k_wifi8_ucast_stats_id_alloc(dp_hw_grp_wifi8,
							  dp_peer->peer_id,
							  ATH12K_INVALID_TID,
							  params->hw_link_id,
							  num_peers);
	}

	/* Add ath12k_dp_peer to the linked list holding peer_list_lock */
	spin_lock_bh(&dp_hw->peer_list_lock);
	list_add(&dp_peer->list, &dp_hw->peers);
	spin_unlock_bh(&dp_hw->peer_list_lock);

	ath12k_dp_peer_hash_table_add(dp_hw, dp_peer);

	rcu_assign_pointer(dp_hw->dp_peer_list[dp_peer->peer_id], dp_peer);

	spin_unlock_bh(&dp_hw->peer_hash_lock);

	params->peer_id = dp_peer->peer_id;
	params->sta_id = dp_peer->sta_id;
	dp_peer->dp_peer_state = ATH12K_DP_PEER_CREATED;

	return 0;
}

void ath12k_wifi8_dp_peer_cleanup(struct ath12k_dp_hw *dp_hw,
				  struct ath12k_dp_peer *dp_peer)
{
	clear_bit(dp_peer->peer_id, dp_hw->free_peer_id_map);
	rcu_assign_pointer(dp_hw->dp_peer_list[dp_peer->peer_id], NULL);
	if (dp_peer->qos) {
		if (dp_peer->qos->telemetry_peer_ctx)
			ath12k_telemetry_peer_ctx_free(dp_peer->qos->telemetry_peer_ctx);

		kfree(dp_peer->qos);
	}

	ath12k_dp_peer_stats_free(dp_peer);
	dp_peer->dp_peer_state = ATH12K_DP_PEER_DELETED;
}

void ath12k_wifi8_dp_peer_delete(struct ath12k_dp *dp, struct ath12k_hw *ah, u8 *addr,
				 struct ieee80211_sta *sta, u8 hw_link_id)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	struct ath12k_base *central_ab = NULL;
	struct ath12k_dp_wifi8 *dp_wifi8 = NULL;
	struct hal_srng *srng = NULL;
	u16 link_band_id[HAL_TASC_BAND_MAX];
	struct ath12k_dp *umac_dp;
	unsigned int num_peers;
	u16 peerid_index;
	struct ath12k_sta *ahsta;
	int i, ret;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);

	spin_lock_bh(&dp_hw->peer_hash_lock);

	if (sta) {
		ahsta = ath12k_sta_to_ahsta(sta);
		rcu_assign_pointer(ahsta->dp_peer, NULL);

		dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, addr, sta);
	} else {
		dp_peer = ath12k_dp_vdev_peer_find(dp_hw, addr, hw_link_id);
	}

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	ath12k_dp_peer_hash_table_delete(dp_hw, dp_peer);

	spin_lock_bh(&dp_hw->peer_list_lock);
	list_del(&dp_peer->list);
	spin_unlock_bh(&dp_hw->peer_list_lock);

	/* reset the telemetry peer stats based configs */
	for (i = 0; i < HAL_TASC_BAND_MAX; i++)
		link_band_id[i] = DP_TELEMETRY_INVALID_LINK_BAND_ID;

	umac_dp = dp_hw_grp_wifi8->cumac_dp;
	ath12k_wifi8_dp_telemetry_peer_delete(umac_dp, dp_peer->stats_id, link_band_id);

	dp_hw_grp_wifi8->stats_id_map[dp_peer->stats_id].dp_peer_id =
		ATH12K_MLO_PEER_ID_INVALID;
	dp_hw_grp_wifi8->stats_id_map[dp_peer->stats_id].tid = ATH12K_INVALID_TID;
	dp_hw_grp_wifi8->stats_id_map[dp_peer->stats_id].hw_link_id =
		ATH12K_INVALID_LINK_ID;
	clear_bit(dp_peer->stats_id, dp_hw_grp_wifi8->free_stats_id);

	clear_bit(dp_peer->sta_id, dp_hw->free_sta_id_map);

	/* Send peer clear command over all links.
	 * TODO: In v2 hardware, a new link_mask field allows a single SAM command
	 * to be sent for all links, with bits set for each link.
	 */
	if (!dp_peer->is_vdev_peer && dp_hw_grp_wifi8 && dp_hw_grp_wifi8->cumac_dp) {
		central_ab = dp_hw_grp_wifi8->cumac_dp->ab;
		dp_wifi8 = ath12k_get_dp_wifi8(dp_hw_grp_wifi8->cumac_dp);
		if (central_ab && dp_wifi8)
			srng = &central_ab->hal.srng_list[dp_wifi8->sam_cmd_ring.ring_id];

		if (srng) {
			for (i = 0; i < HAL_TX_NUM_MAX_LINKS; i++) {
				ret = ath12k_wifi8_hal_tx_sam_cmd_send
						(central_ab, srng, i,
						 HAL_SAM_PEER_CLEAR_PROGRAMMING_BO,
						 dp_peer->sta_id, false, false);

				if (ret < 0)
					ath12k_warn(central_ab,
						    "failed to send SAM peer clear command for link %d: %d\n",
						    i, ret);
			}
		}
	}

	if (dp_peer->dp_peer_state >= ATH12K_DP_PEER_LOGICALLY_DELETED) {
		ath12k_wifi8_dp_peer_cleanup(dp_hw, dp_peer);
		num_peers = bitmap_weight(dp_hw->free_peer_id_map, ATH12K_MAX_PEER_ID);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_wifi8_dp_telemetry_peer_count_update(umac_dp->ab, num_peers);
		synchronize_rcu();
		kfree(dp_peer);

		return;
	} else {
		/* peer delete before the peer assoc complete */
		if (!dp_peer->peer_ext_ctx) {
			peerid_index = dp_peer->peer_id;
			rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);
			ath12k_wifi8_dp_peer_cleanup(dp_hw, dp_peer);
			num_peers = bitmap_weight(dp_hw->free_peer_id_map,
						  ATH12K_MAX_PEER_ID);
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			ath12k_wifi8_dp_telemetry_peer_count_update(umac_dp->ab,
								    num_peers);
			synchronize_rcu();
			kfree(dp_peer);
			return;
		}
		ath12k_dp_ast_entry_delete(dp->dp_hw_grp,
					   dp_peer->peer_ext_ctx->ast_index);

		dp_peer->dp_peer_state = ATH12K_DP_PEER_LOGICALLY_DELETED;
	}

	spin_unlock_bh(&dp_hw->peer_hash_lock);
}

int ath12k_wifi8_dp_peer_assoc(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			       struct ath12k_dp_vif *dp_vif, u8 *addr)
{
	struct ath12k_ast_entry_config_params ast_param = {0};
	struct ath12k_dp_hw_group_wifi8 *dp_hw_group_wifi8;
	struct ath12k_dp_peer_ext_ctx *peer_ext_ctx = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;
	u16 link_band_id[HAL_TASC_BAND_MAX];
	dma_addr_t pn_counter_paddr = 0;
	struct ath12k_dp_peer *dp_peer;
	dma_addr_t tx_classify_paddr;
	struct ath12k_dp *umac_dp;
	void *tx_classify_vaddr;
	unsigned int num_peers;
	bool is_qos = true;
	int ret, i;
	int vdev_peer_link_id;
	struct ath12k_dp_link_vif *dp_link_vif;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	u32 ppeds_idx_map_val = 0;
#endif

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	peer_ext_ctx = kzalloc(sizeof(*peer_ext_ctx), GFP_ATOMIC);
	if (!peer_ext_ctx) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOMEM;
	}

	for (i = 0; i < HAL_TASC_BAND_MAX; i++)
		link_band_id[i] = DP_TELEMETRY_INVALID_LINK_BAND_ID;

	dp_hw_group_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	umac_dp = dp_hw_group_wifi8->cumac_dp;

	dp_peer->peer_ext_ctx = peer_ext_ctx;
	spin_lock_init(&peer_ext_ctx->tx_flow_info.tx_q_lock);
	rcu_read_lock();
	for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
		link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, i);
		if (!link_peer)
			continue;

		set_bit(link_peer->hw_link_id,
			&peer_ext_ctx->tx_flow_info.assoc_hw_links_bitmap);

		link_band_id[link_peer->hw_link_id] = link_peer->link_band_id;

		if (dp_peer->is_vdev_peer) {
			vdev_peer_link_id = dp_peer->hw_links[link_peer->hw_link_id];
			break;
		}
	}

	rcu_read_unlock();

	num_peers = bitmap_weight(dp_hw->free_peer_id_map, ATH12K_MAX_PEER_ID);
	ath12k_wifi8_dp_telemetry_peer_count_update(umac_dp->ab, num_peers);

	/* Configure peer registers for telemetry stats */
	ath12k_wifi8_dp_telemetry_peer_config(umac_dp, dp_peer->stats_id, link_band_id);

	ret = ath12k_dp_tx_classify_info_alloc(dp->dp_hw_grp,
					       &tx_classify_paddr,
					       &tx_classify_vaddr);
	if (ret)
		goto free_peer_ext_ctx;

	peer_ext_ctx->tx_flow_info.hw_who_classify_info_vaddr = tx_classify_vaddr;
	peer_ext_ctx->tx_flow_info.hw_who_classify_info_paddr = tx_classify_paddr;
	pn_counter_paddr = ath12k_dp_get_page_paddr(dp->dp_hw_grp, dp_peer->peer_id);
	if (!pn_counter_paddr) {
		ret = -ENOMEM;
		goto free_tx_classify_info;
	}

	if (dp_peer->is_vdev_peer) {
		ret = ath12k_peer_alloc_mcast_queues(dp->dp_hw_grp, dp_peer, dp_vif);
		if (ret)
			goto free_queues_info;
	} else {
		if (ath12k_dp_peer_get_sta(dp_peer)->wme)
			is_qos = true;
		else
			is_qos = false;

		ret = ath12k_peer_alloc_default_queues(dp->dp_hw_grp, dp_peer,
						       dp_vif, is_qos);
		if (ret)
			goto free_queues_info;

		ret = ath12k_peer_alloc_hol_queues(dp->dp_hw_grp, dp_peer,
						   dp_vif, is_qos);
		if (ret)
			goto free_queues_info;

		ret = ath12k_peer_alloc_mgmt_queues(dp->dp_hw_grp, dp_peer, dp_vif);
		if (ret)
			goto free_queues_info;
	}

	memcpy(ast_param.mac_addr, addr, ETH_ALEN);
	ast_param.peer_id = dp_peer->peer_id;
	ast_param.tx_classify_info_paddr = tx_classify_paddr;
	ast_param.ast_entry_flags |= ATH12K_AST_ENTRY_IS_USE_ADDRX;
	ret = ath12k_dp_ast_entry_create(dp->dp_hw_grp, &ast_param);
	if (ret)
		goto free_queues_info;

	peer_ext_ctx->ast_index = ast_param.ast_index;
	peer_ext_ctx->ast_hash = ast_param.ast_hash;

	if (dp_peer->is_sta_bss_peer) {
		dp_vif->ast_idx = ast_param.ast_index;
		dp_vif->ast_hash = ast_param.ast_hash;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ppeds_idx_map_val |=
			u32_encode_bits(dp_vif->ast_idx, HAL_TX_PPEDS_CFG_SEARCH_IDX) |
			u32_encode_bits(dp_vif->ast_hash, HAL_TX_PPEDS_CFG_CACHE_SET);
		ath12k_wifi8_hal_ppeds_cfg_ast(dp->ab, dp_vif->ppe_vp_num,
						ppeds_idx_map_val);

		ath12k_dbg(NULL, ATH12K_DBG_PEER, "STA ast_idx:%d hash:%d ppe_vp:%d\n",
				dp_vif->ast_idx,
				dp_vif->ast_hash,
				dp_vif->ppe_vp_num);
#endif
	} else if (dp_peer->is_vdev_peer) {
		dp_link_vif = &dp_vif->dp_link_vif[vdev_peer_link_id];
		dp_link_vif->ast_idx = ast_param.ast_index;
		dp_link_vif->ast_hash =	ast_param.ast_hash;
	}

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	return 0;

free_queues_info:
	ath12k_peer_free_static_queues(dp->dp_hw_grp, dp_peer);
free_tx_classify_info:
	ath12k_dp_tx_classify_info_free(dp->dp_hw_grp,
					tx_classify_paddr,
					tx_classify_vaddr);
free_peer_ext_ctx:
	kfree(peer_ext_ctx);
	dp_peer->peer_ext_ctx = NULL;
	spin_unlock_bh(&dp_hw->peer_hash_lock);
	return ret;
}

void ath12k_wifi8_dp_link_peer_assign_id(struct ath12k_dp *dp, struct ath12k *ar,
					 struct ath12k_dp_link_peer *peer)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	peer->link_band_id = ath12k_wifi8_link_band_id_alloc(dp_hw_grp_wifi8);
}

void ath12k_wifi8_dp_link_peer_unassign_id(struct ath12k_dp *dp, struct ath12k *ar,
					   struct ath12k_dp_link_peer *peer)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	ath12k_wifi8_stats_id_map_update_hw_link(dp_hw_grp_wifi8, peer->dp_peer,
						       peer);
	clear_bit(peer->link_band_id, dp_hw_grp_wifi8->free_link_band_id);
}

int ath12k_dp_tqm_update_mpduq_sn_pn(struct ath12k_base *ab,
				     struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr,
				     struct ath12k_dp_peer *dp_peer,
				     u16 sn, u8 *pn)
{
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(ab->dp->dp_hw_grp);
	struct ath12k_hal_tqm_cmd cmd = {0};
	int ret = 0;
	void *pn_vaddr = NULL;
	dma_addr_t pn_paddr;

	if (!sw_mpduq_ptr)
		return ret;

	if (sn >= ATH12K_DP_MAX_SEQ_NUM) {
		ath12k_err(ab, "Error: invalid sn peer = %d", dp_peer->peer_id);
		return -EINVAL;
	}

	pn_vaddr = ath12k_dp_get_page_vaddr(ab->dp->dp_hw_grp, dp_peer->peer_id);
	if (!pn_vaddr) {
		ath12k_err(ab, "Error: pn_addr is NULL peer = %d", dp_peer->peer_id);
		return -EINVAL;
	}
	memcpy(pn_vaddr, pn, ATH12K_DP_PN_COUNTER_SIZE);
	pn_paddr = ath12k_dp_get_page_paddr(ab->dp->dp_hw_grp, dp_peer->peer_id);
	ath12k_core_dma_sync_single_for_device(dev, pn_paddr,
					       ATH12K_DP_PN_COUNTER_SIZE,
					       DMA_BIDIRECTIONAL);

	cmd.std.peer_id = (u16)dp_peer->peer_id;
	cmd.update_mpduq.mpdu_q_paddr = sw_mpduq_ptr->mpdu_q_paddr;
	cmd.update_mpduq.sn_num_valid = 1;
	cmd.update_mpduq.sn_num = sn;

	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_UPDATE_MPDUQ_BO, &cmd,
					   NULL, NULL);
	return ret;
}

int ath12k_dp_tqm_update_mpduq_max_lsn(struct ath12k_base *ab,
				       struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr,
				       struct ath12k_dp_peer *dp_peer,
				       u16 max_lsn)
{
	struct ath12k_hal_tqm_cmd cmd = {0};
	int ret = 0;

	if (!sw_mpduq_ptr)
		return ret;

	cmd.std.peer_id = (u16)dp_peer->peer_id;
	cmd.update_mpduq.mpdu_q_paddr = sw_mpduq_ptr->mpdu_q_paddr;
	cmd.update_mpduq.max_lsn_valid = 1;
	cmd.update_mpduq.max_lsn = max_lsn;

	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_UPDATE_MPDUQ_BO, &cmd,
					   NULL, NULL);
	return ret;
}

int ath12k_wifi8_peer_tx_tid_update_for_smd(struct ath12k_base *ab,
					    struct ath12k_dp_hw *dp_hw,
					    const u8 *peer_addr,
					    struct ath12k_tx_smd_ctx_per_tid *tx_tid_ctx)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	struct ath12k_dp_peer *dp_peer;
	int ret;

	if (!dp_hw || !peer_addr || !tx_tid_ctx) {
		ath12k_err(ab, "invalid args for SMD TX update\n");
		return -EINVAL;
	}

	if (tx_tid_ctx->ssn > ATH12K_DP_MAX_SEQ_NUM) {
		ath12k_warn(ab, "Invalid TX SSN 0x%x for tid %d\n",
			    tx_tid_ctx->ssn, tx_tid_ctx->tid);
		return -EINVAL;
	}
	if (tx_tid_ctx->tid > ATH12K_SMD_MGMT_TID) {
		ath12k_warn(ab, "SMD update Invalid TX tid %d\n", tx_tid_ctx->tid);
		return -EINVAL;
	}

	spin_lock_bh(&dp_hw->peer_hash_lock);

	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, (u8 *)peer_addr);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_warn(ab, "failed to find peer %pM for SMD TX update\n",
			    peer_addr);
		return -ENOENT;
	}

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	if (!tx_flow_info) {
		ath12k_err(ab, "SMD TX update invalid tx flow info peer %pM",
			   dp_peer->addr);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -EINVAL;
	}

	spin_lock_bh(&tx_flow_info->tx_q_lock);
	if (tx_tid_ctx->tid < ATH12K_MAX_NUM_DATA_TIDS)
		sw_mpduq_ptr = tx_flow_info->tid_info[tx_tid_ctx->tid].mpduq;
	else if (tx_tid_ctx->tid == ATH12K_SMD_MGMT_TID)
		sw_mpduq_ptr = tx_flow_info->mgmt_mpduq;

	if (!sw_mpduq_ptr) {
		spin_unlock_bh(&tx_flow_info->tx_q_lock);

		/*
		 * for data TIDs the mpduq is allocated only when a BA session is
		 * established. If it is not yet set up on the new AP, skip the
		 * datapath update — the SSN/PN context is already cached in
		 * ahsta->smd_info and will be pushed to FW via WMI.
		 */
		if (tx_tid_ctx->tid != TQM_NON_DATA_TID) {
			ath12k_dbg(ab, ATH12K_DBG_DP_TX,
				   "SMD TX update: no mpduq for tid %d peer %pM, skipping\n",
				   tx_tid_ctx->tid, dp_peer->addr);
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			return 0;
		}
		ath12k_err(ab, "SMD TX update invalid tx MPDUQ peer %pM",
			   dp_peer->addr);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -EINVAL;
	}

	ret = ath12k_dp_tqm_update_mpduq_sn_pn(ab, sw_mpduq_ptr, dp_peer,
					       tx_tid_ctx->ssn,
					       tx_tid_ctx->pn_number);
	if (ret) {
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_err(ab, "SMD UPDATE MPDUQ failed tid %d peer %pM id %d\n",
			   tx_tid_ctx->tid, dp_peer->addr, dp_peer->peer_id);
		return ret;
	}

	if (tx_tid_ctx->lsn_offset > 0 &&
	    tx_tid_ctx->lsn_offset <=  ATH12K_DP_MAX_POSSIBLE_BA_WIN) {
		ret = ath12k_dp_tqm_update_mpduq_max_lsn(ab, sw_mpduq_ptr,
							 dp_peer,
							 tx_tid_ctx->lsn_offset);
		if (ret) {
			ath12k_err(ab,
				   "SMD UPDATE LSN failed tid %d peer %pM id %d\n",
				   tx_tid_ctx->tid, dp_peer->addr, dp_peer->peer_id);
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			return ret;
		}
	}
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	ath12k_dbg(ab, ATH12K_DBG_DP_TX,
		   "SMD TX update done for peer %pM tid %d: SSN=0x%x\n",
		   peer_addr, tx_tid_ctx->tid, tx_tid_ctx->ssn);

	return 0;
}

void ath12k_dp_peer_get_mpdu_queues_stats_status(struct ath12k_dp *dp,
						 void *ctx,
						 struct hal_tqm_status *tqm_status)
{
	struct ath12k_base *ab = dp->ab;

	if (!tqm_status) {
		ath12k_err(ab, "Error: TQM STATUS is not valid");
		return;
	}
	if (tqm_status->status_hdr.cmd_execution_status != HAL_TQM_SUCCESSFUL_EXECUTION) {
		ath12k_err(ab, "Error: TQM STATUS FAILED with reason %d",
			   tqm_status->status_hdr.cmd_execution_status);
		return;
	}
}

int ath12k_dp_tqm_get_mpdu_queue_stats(struct ath12k_base *ab,
				       struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr,
				       struct ath12k_dp_peer *dp_peer,
				       bool clear_stats,
				       u16 cookie)
{
	struct ath12k_hal_tqm_cmd cmd = {0};
	struct ath12k_dp_tx_queue data =  {0};
	int ret = -EINVAL;

	if (!sw_mpduq_ptr)
		return ret;

	cmd.std.peer_id = (u16)dp_peer->peer_id;
	cmd.get_mpduq_stats.mpdu_q_paddr = sw_mpduq_ptr->mpdu_q_paddr;
	cmd.get_mpduq_stats.clear_stats = clear_stats;

	data.peer_id = dp_peer->peer_id;
	data.cookie = cookie;
	memcpy(&data.addr, dp_peer->addr, ETH_ALEN);

	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_GET_MPDUQ_STATS_BO, &cmd,
					   &data,
					   ath12k_dp_peer_get_mpdu_queues_stats_status);

	return ret;
}

u16 ath12k_dp_peer_compute_max_lsn(u16 ba_size)
{
	return ba_size/2;
}

int ath12k_dp_peer_fetch_smd_tx_ctx(struct ath12k_base *ab,
				    struct ath12k_dp_peer *dp_peer,
				    u32 tx_tid_bitmap,
				    u16 *tx_tid_ba_win_size)
{	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	int tid;
	int ret = 0;

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	if (!tx_flow_info) {
		ath12k_err(ab, "invalid tx flow info peer %pM", dp_peer->addr);
		return -EINVAL;
	}
	spin_lock_bh(&tx_flow_info->tx_q_lock);
	/* data tids */
	for (tid = 0; tid < ATH12K_MAX_NUM_DATA_TIDS; tid++) {
		u16 lsn_offset_tap = ATH12K_DP_INVALID_MLSN_OFFSET;
		u16 lsn_offset_sap;

		if (!(tx_tid_bitmap & BIT(tid)))
			continue;

		sw_mpduq_ptr = tx_flow_info->tid_info[tid].mpduq;
		if (!sw_mpduq_ptr)
			continue;

		/* update the max sequence number */
		if (tx_tid_ba_win_size[tid] <= ATH12K_DP_MAX_POSSIBLE_BA_WIN) {
			lsn_offset_sap =
				ath12k_dp_peer_compute_max_lsn(tx_tid_ba_win_size[tid]);
			ret = ath12k_dp_tqm_update_mpduq_max_lsn(ab, sw_mpduq_ptr,
								 dp_peer,
								 lsn_offset_sap);
			if (ret) {
				ath12k_err(ab,
					   "UPDATE MPDUQ failed tid %d peer %pM id %d\n",
					   tid, dp_peer->addr, dp_peer->peer_id);
				spin_unlock_bh(&tx_flow_info->tx_q_lock);
				return ret;
			}
			lsn_offset_tap = tx_tid_ba_win_size[tid] - lsn_offset_sap;
		}
		/* fetch SN and PN */
		ret = ath12k_dp_tqm_get_mpdu_queue_stats(ab, sw_mpduq_ptr,
							 dp_peer, false,
							 lsn_offset_tap);
		if (ret) {
			ath12k_err(ab, "GET MPDUQ failed tid %d peer %pM id %d\n",
				   tid, dp_peer->addr, dp_peer->peer_id);
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			return ret;
		}
	}

	/* mgmt tid */
	if (tx_tid_bitmap & BIT(ATH12K_SMD_MGMT_TID)) {
		sw_mpduq_ptr = tx_flow_info->mgmt_mpduq;
		ret = ath12k_dp_tqm_get_mpdu_queue_stats(ab, sw_mpduq_ptr,
							 dp_peer, false, 0);
		if (ret) {
			ath12k_err(ab, "GET MPDUQ failed tid %d peer %pM id %d\n",
				   tid, dp_peer->addr, dp_peer->peer_id);
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			return ret;
		}
	}
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	return ret;
}

int ath12k_dp_tqm_remove_msduq_send(struct ath12k_base *ab,
				    struct ath12k_dp_msdu_q_info *sw_msduq_ptr,
				    struct ath12k_dp_peer *dp_peer,
				    u16 count)
{
	struct ath12k_hal_tqm_cmd cmd;
	int ret = 0;

	if (!sw_msduq_ptr)
		return ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.std.peer_id = (u16)dp_peer->peer_id;
	cmd.remove_msdu_params.block_tx_notify_frame_removal = 0;
	cmd.remove_msdu_params.count = count;
	cmd.remove_msdu_params.qtype = sw_msduq_ptr->flow_info.flow_type;
	cmd.remove_msdu_params.msdu_q_paddr = sw_msduq_ptr->msdu_q_paddr;

	if (count == ATH12K_MAX_MSDU_COUNT)
		cmd.remove_msdu_params.type = HAL_WIFIREMOVE_MSDUS_AND_DISABLE_FLOW;
	else
		cmd.remove_msdu_params.type = HAL_WIFIREMOVE_HEAD_MSDUS;

	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_REMOVE_MSDU_BO, &cmd,
					   NULL, NULL);

	return ret;
}


int ath12k_dp_tqm_remove_mpduq_send(struct ath12k_base *ab,
				    struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr,
				    struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_hal_tqm_cmd cmd;
	int ret = 0;

	if (!sw_mpduq_ptr)
		return ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.std.peer_id = (u16)dp_peer->peer_id;
	cmd.remove_mpdu_params.type = HAL_WIFIREMOVE_MPDUS_AND_DISABLE_QUEUE;
	cmd.remove_mpdu_params.block_tx_notify_frame_removal = 0;
	cmd.remove_mpdu_params.count = 0xFFF; //need to check if ffff?
	cmd.remove_mpdu_params.mpdu_q_paddr = sw_mpduq_ptr->mpdu_q_paddr;

	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_REMOVE_MPDU_BO, &cmd,
					   NULL, NULL);

	return ret;
}


int ath12k_dp_tqm_remove_mcast_queues(struct ath12k_base *ab,
				      struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	int ret = 0;

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	spin_lock_bh(&tx_flow_info->tx_q_lock);

	sw_msduq_ptr = tx_flow_info->mcast_msduq;
	ret = ath12k_dp_tqm_remove_msduq_send(ab,
					      sw_msduq_ptr,
					      dp_peer,
					      ATH12K_MAX_MSDU_COUNT);
	if (ret) {
		ath12k_err(ab,
			   "TQM MSDUQ send failed for MCAST frame for peer %pM id %d",
			   dp_peer->addr, dp_peer->peer_id);
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		return ret;
	}
	if (sw_msduq_ptr)
		sw_msduq_ptr->tqm_send = 1;

	sw_mpduq_ptr = tx_flow_info->mcast_mpduq;
	ret = ath12k_dp_tqm_remove_mpduq_send(ab,
					      sw_mpduq_ptr,
					      dp_peer);
	if (ret) {
		ath12k_err(ab,
			   "TQM MPDUQ send failed for MCAST frame for peer %pM id %d",
			   dp_peer->addr, dp_peer->peer_id);
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		return ret; //whether to ret or retry?
	}
	if (sw_mpduq_ptr)
		sw_mpduq_ptr->tqm_send = 1;

	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	return ret;
}

int ath12k_dp_tqm_remove_data_queues(struct ath12k_base *ab,
				     struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	int tid, q;
	int ret = 0;

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	spin_lock_bh(&tx_flow_info->tx_q_lock);

	sw_msduq_ptr = tx_flow_info->hol_msduq;
	ret = ath12k_dp_tqm_remove_msduq_send(ab,
					      sw_msduq_ptr,
					      dp_peer,
					      ATH12K_MAX_MSDU_COUNT);
	if (ret) {
		ath12k_err(ab,
			   "TQM MSDUQ send failed for HOL frame for peer %pM id %d",
			   dp_peer->addr, dp_peer->peer_id);
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		return ret;
	}
	if (sw_msduq_ptr)
		sw_msduq_ptr->tqm_send = 1;

	for (tid = 0; tid < ATH12K_MAX_NUM_DATA_TIDS; tid++) {
		for (q = 0; q < ATH12K_MAX_DP_MSDUQ_PER_TID; q++) {
			sw_msduq_ptr = tx_flow_info->tid_info[tid].msduq[q];
			ret = ath12k_dp_tqm_remove_msduq_send(ab,
							      sw_msduq_ptr,
							      dp_peer,
							      ATH12K_MAX_MSDU_COUNT);
			if (ret) {
				ath12k_err(
				ab,
				"TQM MSDUQ fail: data frame %d tid %d peer %pM id %d",
				q, tid, dp_peer->addr, dp_peer->peer_id);
				spin_unlock_bh(&tx_flow_info->tx_q_lock);
				return ret; //whether to return or continue
			}
			if (sw_msduq_ptr)
				sw_msduq_ptr->tqm_send = 1;
		}
		sw_mpduq_ptr = tx_flow_info->tid_info[tid].mpduq;
		ret = ath12k_dp_tqm_remove_mpduq_send(ab,
						      sw_mpduq_ptr,
						      dp_peer);
		if (ret) {
			ath12k_err(ab,
				   "TQM MPDUQ fail: data frame tid %d peer %pM id %d",
				   tid, dp_peer->addr, dp_peer->peer_id);
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			return ret; //whether to return or continue
		}
		if (sw_mpduq_ptr)
			sw_mpduq_ptr->tqm_send = 1;
	}

	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	return ret;
}

int ath12k_dp_tqm_remove_mgmt_queues(struct ath12k_base *ab,
				     struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_mpdu_q_info *sw_mpduq_ptr = NULL;
	int q;
	int ret = 0;

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	spin_lock_bh(&tx_flow_info->tx_q_lock);

	for (q = 0; q < MGMT_MSDUQ_TYPE_MAX; q++) {
		sw_msduq_ptr = tx_flow_info->mgmt_msduq[q];
		if (!sw_msduq_ptr || sw_msduq_ptr->tqm_send)
			continue;
		ret = ath12k_dp_tqm_remove_msduq_send(ab,
						      sw_msduq_ptr,
						      dp_peer,
						      ATH12K_MAX_MSDU_COUNT);
		if (ret) {
			ath12k_err(ab,
				   "TQM MSDUQ fail: MGMT frame %d peer %pM id %d",
				   q, dp_peer->addr, dp_peer->peer_id);
			spin_unlock_bh(&tx_flow_info->tx_q_lock);
			return ret;
		}
		if (sw_msduq_ptr)
			sw_msduq_ptr->tqm_send = 1;
	}
	sw_mpduq_ptr = tx_flow_info->mgmt_mpduq;
	ret = ath12k_dp_tqm_remove_mpduq_send(ab,
					      sw_mpduq_ptr,
					      dp_peer);
	if (ret) {
		ath12k_err(ab,
			   "TQM MPDUQ fail: MGMT frame peer %pM id %d",
			   dp_peer->addr, dp_peer->peer_id);
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		return ret; //whether to return or continue
	}
	if (sw_mpduq_ptr)
		sw_mpduq_ptr->tqm_send = 1;

	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	return ret;
}

static inline
int ath12k_dp_tqm_remove_link_mgmt_queues(struct ath12k_base *ab,
					  struct ath12k_dp_peer *dp_peer,
					  u8 hw_link_id)
{
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	int ret = 0;
	u8 idx;

	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	spin_lock_bh(&tx_flow_info->tx_q_lock);

	idx = ATH12K_LINK_TO_MGMT_TYPE(hw_link_id);
	sw_msduq_ptr = tx_flow_info->mgmt_msduq[idx];
	ret = ath12k_dp_tqm_remove_msduq_send(ab, sw_msduq_ptr,
					      dp_peer,
					      ATH12K_MAX_MSDU_COUNT);
	if (ret) {
		ath12k_err(ab,
			   "TQM Link mgmt MSDUQ fail: peer %pM linkid %d",
			   dp_peer->addr, hw_link_id);
		spin_unlock_bh(&tx_flow_info->tx_q_lock);
		return ret;
	}
	if (sw_msduq_ptr)
		sw_msduq_ptr->tqm_send = 1;
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	return ret;
}

static inline
int ath12k_dp_tqm_sync_remove_queues(struct ath12k_base *ab,
				     struct ath12k_dp_peer *dp_peer,
				     struct ath12k_dp_tx_queue *data)
{
	struct ath12k_hal_tqm_cmd cmd;
	int ret = 0;

	memset(&cmd, 0, sizeof(cmd));
	cmd.std.peer_id = 0xFFFF;
	cmd.tqm_sync_params.cb_func = ath12k_dp_peer_cleanup_tqm_sync;
	cmd.tqm_sync_params.cb_ctxt = dp_peer;
	cmd.tqm_sync_params.cb_data = 0;
	if (!cmd.tqm_sync_params.cb_func)
		cmd.tqm_sync_params.data_only = 1;

	ret = ath12k_wifi8_dp_tqm_cmd_send(ab, HAL_TQM_SYNC_CMD_BO, &cmd, data,
					   ath12k_dp_peer_cleanup_tqm_sync);
	return ret;
}

static inline
int ath12k_dp_tqm_remove_mgmt_link_queues(struct ath12k_base *ab,
				    struct ath12k_dp_peer *dp_peer,
				    u8 hw_link_id)
{
	struct ath12k_dp *central_dp = ath12k_get_central_dp(ab->dp);
	int ret_mgmt = 0;

	ab = central_dp->ab;

	if (dp_peer->is_vdev_peer) {
		ath12k_dbg(ab, ATH12K_DBG_PEER, "peer %d is mcast peer",
			   dp_peer->peer_id);
		return 0;
	}
	ret_mgmt = ath12k_dp_tqm_remove_link_mgmt_queues(ab, dp_peer, hw_link_id);
	if (ret_mgmt) {
		ath12k_err(ab,
			   "Error: TQM Remove link MGMT queue peer %d link_id %d",
			   dp_peer->peer_id, hw_link_id);
		return ret_mgmt;
	}
	return 0;
}

static inline
int ath12k_dp_tqm_remove_queues_cmd(struct ath12k_base *ab,
				    struct ath12k_dp_peer *dp_peer,
				    u8 hw_link_id)
{
	struct ath12k_dp *central_dp = ath12k_get_central_dp(ab->dp);
	struct ath12k_dp_tx_queue data;
	int ret_mcast = 0, ret_data = 0, ret_mgmt = 0, ret_sync = 0;

	ab = central_dp->ab;

	if (dp_peer->is_vdev_peer) {
		ret_mcast = ath12k_dp_tqm_remove_mcast_queues(ab, dp_peer);
		if (ret_mcast) {
			ath12k_err(ab,
				   "Error: TQM Remove MCAST queue peer %d",
				   dp_peer->peer_id);
			return ret_mcast;
		}
	} else {
		ret_data = ath12k_dp_tqm_remove_data_queues(ab, dp_peer);
		ret_mgmt = ath12k_dp_tqm_remove_mgmt_queues(ab, dp_peer);
		if (ret_data || ret_mgmt) {
			ath12k_err(
			ab,
			"Error: TQM Remove DATA/MGMT queue peer %d ret_data %d ret_mgmt %d",
			dp_peer->peer_id, ret_data, ret_mgmt);
			return ret_data ? ret_data : ret_mgmt;
		}
	}
	data.peer_id = dp_peer->peer_id;
	data.hw_link_id = hw_link_id;
	ret_sync = ath12k_dp_tqm_sync_remove_queues(ab, dp_peer, &data);
	if (ret_sync) {
		ath12k_err(ab, "Error: TQM SYNC peer %d",
			   dp_peer->peer_id);
		return ret_sync;
	}
	return 0;
}

void ath12k_dp_peer_cleanup_indication(struct ath12k_dp *dp,
				       u16 peer_id,
				       u8 hw_link_id)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_tx_flow_info *tx_info;
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k_dp_hw *dp_hw;
	struct ath12k_dp_peer *dp_peer;
	u8 pdev_id;
	int ret = 0;

	pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params,
					      dp_hw_grp->hw_links[hw_link_id].pdev_idx);
	rcu_read_lock();
	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
	if (!dp_pdev) {
		rcu_read_unlock();
		return;
	}

	dp_hw = dp_pdev->dp_hw;
	if (!dp_hw) {
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = rcu_dereference(dp_pdev->dp_hw->dp_peer_list[peer_id]);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return;
	}

	tx_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	if (!tx_info) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return;
	}

	clear_bit(hw_link_id, &tx_info->txq_hw_links_bitmap);

	ret = ath12k_dp_tqm_remove_mgmt_link_queues(dp->ab, dp_peer, hw_link_id);
	if (ret) {
		ath12k_err(dp->ab,
			   "ERROR: TQM REMOVE MGMT LINK QUEUE CMD peer %d link %d",
			   dp_peer->peer_id, hw_link_id);
	}
	/* Check whether event is for last link or not */
	if (tx_info->txq_hw_links_bitmap) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		rcu_read_unlock();
		return;
	}

	ret = ath12k_dp_tqm_remove_queues_cmd(dp->ab, dp_peer, hw_link_id);
	if (ret) {
		ath12k_err(dp->ab,
			   "ERROR: TQM REMOVE QUEUE CMD peer %d",
			   dp_peer->peer_id);
	}

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	rcu_read_unlock();
}

void ath12k_wifi8_dp_link_peer_assoc(struct ath12k_dp_hw *dp_hw,
				     struct ath12k_dp *dp,
				     u8 *addr, u32 hw_link_id)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_msdu_q_info *sw_msduq_ptr = NULL;
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	u8 idx;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	/* During link addition if existing queues are re-used; reset tqm_send */
	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	spin_lock_bh(&tx_flow_info->tx_q_lock);
	idx = ATH12K_LINK_TO_MGMT_TYPE(hw_link_id);
	sw_msduq_ptr = tx_flow_info->mgmt_msduq[idx];
	if (sw_msduq_ptr)
		sw_msduq_ptr->tqm_send = 0;
	spin_unlock_bh(&tx_flow_info->tx_q_lock);

	ath12k_dp_tx_peer_msduq_mpduq_setup(dp->dp_hw_grp, dp_peer, hw_link_id);
	spin_unlock_bh(&dp_hw->peer_hash_lock);
}

int ath12k_wifi8_get_mgmt_flowq(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
				u8 *addr,
				struct peer_assoc_flowq_params *flowq_params)
{
	struct ath12k_dp_tx_flow_info *tx_info;
	struct peer_assoc_msduq_params *msduq_params;
	struct peer_assoc_mpduq_params *mpduq_params;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer;
	enum htt_tx_tid_msduq_mpdu_type msduq_type;
	u64 dma_addr = 0;
	u8 hw_link_id;
	int ret = 0;
	u8 idx;
	int i;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr);

	if (!dp_peer || !dp_peer->peer_ext_ctx) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	tx_info = &dp_peer->peer_ext_ctx->tx_flow_info;
	spin_lock_bh(&tx_info->tx_q_lock);
	flowq_params->num_links = 0;
	rcu_read_lock();
	if (dp_peer->is_mlo) {
		for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
			link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, i);
			if (!link_peer)
				continue;

			hw_link_id = link_peer->hw_link_id;
			if (!tx_info->mgmt_msduq[ATH12K_LINK_TO_MGMT_TYPE(hw_link_id)])
				continue;
			idx = ATH12K_LINK_TO_MGMT_TYPE(hw_link_id);
			msduq_type = ATH12K_LINK_TO_MSDUQ_TYPE(hw_link_id);
			dma_addr = (u64)tx_info->mgmt_msduq[idx]->msdu_q_paddr;
			msduq_params =
				&flowq_params->msduq_params[flowq_params->num_links];
			msduq_params->mgmt_msduq_address =
					(u32)((dma_addr >> 0x8) & 0xFFFFFFFF);
			msduq_params->flow_type = msduq_type;
			msduq_params->link_id = hw_link_id;
			flowq_params->num_links++;
		}

		if (!tx_info->mgmt_msduq[MGMT_MSDUQ_LINK_CMN]) {
			ret = -ENOENT;
			goto exit;
		}
		dma_addr =
			(u64)tx_info->mgmt_msduq[MGMT_MSDUQ_LINK_CMN]->msdu_q_paddr;
		msduq_params = &flowq_params->msduq_params[flowq_params->num_links];
		msduq_params->mgmt_msduq_address =
				(u32)((dma_addr >> 0x8) & 0xFFFFFFFF);
		msduq_params->flow_type = HTT_TID_MSDUQ_MGMT_LINK_AGNOSTIC;
		flowq_params->num_links++;
	} else {
		if (!tx_info->mgmt_msduq[MGMT_MSDUQ_NON_ML]) {
			ret = -ENOENT;
			goto exit;
		}

		for (i = 0; i < ATH12K_DP_PEER_MAX_MLO_LINKS; i++) {
			link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, i);
			if (link_peer)
				break;
		}

		if (!link_peer) {
			ret = -ENOENT;
			goto exit;
		}

		dma_addr =
			(u64)tx_info->mgmt_msduq[MGMT_MSDUQ_NON_ML]->msdu_q_paddr;
		msduq_params = &flowq_params->msduq_params[flowq_params->num_links];
		msduq_params->mgmt_msduq_address =
				(u32)((dma_addr >> 0x8) & 0xFFFFFFFF);
		msduq_params->flow_type = HTT_TID_MSDUQ_MGMT_LINK_SPECIFIC_0;
		msduq_params->link_id = link_peer->hw_link_id;
		flowq_params->num_links++;
	}

	if (tx_info->mgmt_mpduq) {
		dma_addr = (u64)tx_info->mgmt_mpduq->mpdu_q_paddr;
		mpduq_params = &flowq_params->mpduq_params;
		mpduq_params->mgmt_mpduq_address =
				(u32)((dma_addr >> 0x8) & 0xFFFFFFFF);
		dma_addr = (u64)tx_info->mgmt_mpduq->pn_addr;
		mpduq_params->pn_addr_31_0 = (u32)lower_32_bits(dma_addr);
		mpduq_params->pn_addr_39_32 = (u8)(upper_32_bits(dma_addr) & 0x000000FF);
	}
	flowq_params->enabled = 1;

exit:
	rcu_read_unlock();
	spin_unlock_bh(&tx_info->tx_q_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);
	return ret;
}

int ath12k_wifi8_get_holq(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			  u8 *addr, struct peer_assoc_holq_params *holq_params)
{
	struct ath12k_dp_tx_flow_info *tx_info;
	struct ath12k_dp_peer *dp_peer;
	u64 dma_addr = 0;
	int ret = 0;
	u8 holq_tid;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr);

	if (!dp_peer || !dp_peer->peer_ext_ctx) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	tx_info = &dp_peer->peer_ext_ctx->tx_flow_info;
	spin_lock_bh(&tx_info->tx_q_lock);
	holq_tid = tx_info->holq_tid;
	if (!tx_info->hol_msduq || !tx_info->tid_info[holq_tid].mpduq) {
		spin_unlock_bh(&tx_info->tx_q_lock);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	holq_params->peer_id = tx_info->hol_msduq->flow_info.peer_id;
	holq_params->tid = tx_info->hol_msduq->flow_info.tid_num;
	holq_params->mpdu_type =
		tx_info->tid_info[holq_tid].mpduq->flow_info.flow_type;
	holq_params->msdu_type = tx_info->hol_msduq->flow_info.flow_type;

	dma_addr = (u64)tx_info->tid_info[holq_tid].mpduq->mpdu_q_paddr;
	holq_params->mpduq_address = (u32)((dma_addr >> 0x8) & 0xFFFFFFFF);

	dma_addr = (u64)tx_info->hol_msduq->msdu_q_paddr;
	holq_params->msduq_address = (u32)((dma_addr >> 0x8) & 0xFFFFFFFF);

	dma_addr = (u64)tx_info->tid_info[holq_tid].mpduq->pn_addr;
	holq_params->pn_addr_31_0 = (u32)lower_32_bits(dma_addr);
	holq_params->pn_addr_39_32 = (u8)(upper_32_bits(dma_addr) & 0x000000FF);
	holq_params->enabled = 1;

	spin_unlock_bh(&tx_info->tx_q_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);
	return ret;
}

int ath12k_wifi8_dp_get_peer_init_status(struct ath12k_dp *dp,
					 struct ath12k_dp_hw *dp_hw,
					 u8 *addr)
{
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (!(ath12k_wifi8_dp_ase_tx_cache_enabled(dp_hw_grp) ||
	      ath12k_wifi8_dp_ase_rx_cache_enabled(dp_hw_grp)))
		return 0;

	if (!wait_for_completion_timeout(&dp_hw_grp_wifi8->peer_init_done, 1 * HZ)) {
		ath12k_wifi8_invalidate_peer_ase_cache_table(dp_hw_grp);
		ath12k_warn(dp->ab, "peer init is not completed for %pM", addr);
		return -ETIMEDOUT;
	}

	return 0;
}

void ath12k_wifi8_dp_vif_update_4addr(struct ath12k_dp_hw *dp_hw,
				      struct ath12k_dp_vif *dp_vif,
				      u8 *addr)
{
	struct ath12k_dp_peer *dp_peer;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr);

	if (!dp_peer) {
		ath12k_dbg(NULL, ATH12K_DBG_PEER, "unable for find peer for mac addr in set 4 addr %pM",
			   addr);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}

	dp_vif->is_wds_4addr = true;
	dp_vif->ast_idx = dp_peer->peer_ext_ctx->ast_index;
	dp_vif->ast_hash = dp_peer->peer_ext_ctx->ast_hash;
	spin_unlock_bh(&dp_hw->peer_hash_lock);
}

void ath12k_wifi8_dp_assoc_link_update(struct ath12k_dp *dp,
				       struct ath12k_hw *ah,
				       struct ieee80211_sta *sta)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw_group *dp_hw_grp = dp->dp_hw_grp;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_tx_flow_info *tx_flow_info;
	struct ath12k_sta *ahsta;
	u8 assoc_link_id;
	struct hal_txpt_classify_data ti = {0};
	struct ath12k_dp_msdu_q_info *msdu_flow_ptr = NULL;
	struct hal_txpt_classify_info *tx_tid_ptr = NULL;
	dma_addr_t txpt_paddr;
	u64 msdu_flow_dma_ptr = 0;
	u8 tid_num, q;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, sta->addr);

	if (!dp_peer || !dp_peer->sta) {
		ath12k_err(dp->ab, "peer or peer sta is null");
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}
	tx_flow_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	if (!tx_flow_info) {
		ath12k_err(dp->ab, "tx_flow_info is null for %pM", dp_peer->addr);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return;
	}
	spin_lock_bh(&tx_flow_info->tx_q_lock);

	for (tid_num = 0; tid_num < ATH12K_MAX_NUM_DATA_TIDS; tid_num++) {
		for (q = 0; q < ATH12K_MAX_DP_MSDUQ_PER_TID; q++) {
			msdu_flow_ptr = tx_flow_info->tid_info[tid_num].msduq[q];
			if (!msdu_flow_ptr)
				continue;

			tx_tid_ptr = ath12k_get_txpt_info_ptr(
					dp_peer, (q / ATH12K_NUM_MSDU_Q_PER_TID),
					(tid_num + 1));
			txpt_paddr = ath12k_get_txpt_paddr(
					dp_peer, (q / ATH12K_NUM_MSDU_Q_PER_TID),
					(tid_num + 1));

			if (!tx_tid_ptr || txpt_paddr == 0)
				continue;

			msdu_flow_dma_ptr = (u64)msdu_flow_ptr->msdu_q_paddr;
			ti.msdu_paddr = (u32)((msdu_flow_dma_ptr >> 0x8) & 0xFFFFFFFF);
			ti.paddr = txpt_paddr;
			ti.flow_handler = HAL_WIFITXPT_TO_TQM;
			ti.flow_loop_handler = HAL_WIFITXPT_LOOP_TO_TQM;
			ti.msdu_drop = 0;
			ti.metadata = dp_peer ? dp_peer->peer_id : HAL_INVALID_PEERID;

			rcu_read_lock();
			ahsta = ath12k_sta_to_ahsta(dp_peer->sta);
			assoc_link_id = dp_peer->sta->mlo ?
					ahsta->assoc_link_id :
					ahsta->deflink.link_id;
			ti.assoc_link_id =
			ath12k_dp_peer_convert_logical_to_hw_link_id(dp_peer,
								     assoc_link_id);
			if (ti.assoc_link_id == ATH12K_INVALID_HW_LINKID) {
				rcu_read_unlock();
				goto end;
			}
			rcu_read_unlock();
			dp_peer->assoc_hw_link_id = ti.assoc_link_id;

			ath12k_wifi8_hal_txpt_classify_info_setup(dp_hw_grp,
								  tx_tid_ptr, &ti);
		}
	}

end:
	spin_unlock_bh(&tx_flow_info->tx_q_lock);
	spin_unlock_bh(&dp_hw->peer_hash_lock);
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
bool ath12k_wifi8_dp_peer_ast_param_get(struct ath12k_hw *ah,
					u16 *ast_info,
					u16 *hw_peer_id,
					u8 *addr)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct ath12k_dp_peer_ext_ctx *peer_ext_ctx;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, addr);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_dbg(NULL, ATH12K_DBG_PEER, "Invalid peer - ast param get failed\n");
		return false;
	}

	peer_ext_ctx = dp_peer->peer_ext_ctx;
	if (!peer_ext_ctx) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		ath12k_dbg(NULL, ATH12K_DBG_PEER, "Invalid peer ext ctx for peer_id:%d\n",
				dp_peer->peer_id);
		return false;
	}

	/*
	 * Populate AST information with index[15:4] and AST hash[3:0]
	 */
	*ast_info = (peer_ext_ctx->ast_index << ATH12K_AST_INDEX_SHIFT) |
		(peer_ext_ctx->ast_hash & ATH12K_AST_HASH_MASK);
	*hw_peer_id = dp_peer->peer_id;

	ath12k_dbg(NULL, ATH12K_DBG_PEER, "Peer param pid:%u ast_idx:%u ast_hash:%u\n",
		   dp_peer->peer_id, peer_ext_ctx->ast_index, peer_ext_ctx->ast_hash);

	spin_unlock_bh(&dp_hw->peer_hash_lock);
	return true;
}
#endif
