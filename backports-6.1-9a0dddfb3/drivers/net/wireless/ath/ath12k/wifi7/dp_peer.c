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
#include "../telemetry_agent_if.h"
#include "../dp_stats.h"

static u16 ath12k_wifi7_peer_ml_id_alloc(struct ath12k_hw *ah)
{
	u16 ml_peer_id;
	int i;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (ah->num_ml_peers >= ah->max_ml_peers_supported) {
		ath12k_err(NULL, "Failed to create ML peer limit %d[%d]\n",
			   ah->max_ml_peers_supported, ah->num_ml_peers);
		return ATH12K_MLO_PEER_ID_INVALID;
	}

	ml_peer_id = ah->last_ml_peer_id;
	for (i = 0; i <= ah->max_ml_peer_ids; i++) {
		ml_peer_id = (ml_peer_id + 1) % ah->max_ml_peer_ids;

		if (!ml_peer_id)
			continue;

		if (test_bit(ml_peer_id, ah->free_ml_peer_id_map))
			continue;

		set_bit(ml_peer_id, ah->free_ml_peer_id_map);
		break;
	}

	ah->last_ml_peer_id = ml_peer_id;
	if (i == ah->max_ml_peer_ids)
		ml_peer_id = ATH12K_MLO_PEER_ID_INVALID;

	ath12k_dbg(NULL, ATH12K_DBG_PEER, "Allocated ml_peer_id:%d", ml_peer_id);

	return ml_peer_id;
}

static void ath12k_wifi7_peer_ml_id_free(struct ath12k_hw *ah,
					 struct ath12k_sta *ahsta)
{
	if (ahsta->ml_peer_id == ATH12K_MLO_PEER_ID_INVALID)
		return;

	clear_bit(ahsta->ml_peer_id, ah->free_ml_peer_id_map);
	ahsta->ml_peer_id = ATH12K_MLO_PEER_ID_INVALID;
	ah->num_ml_peers--;
}

int ath12k_wifi7_dp_peer_create(struct ath12k_hw *ah, u8 *addr,
				struct ath12k_dp_peer_create_params *params,
				struct ieee80211_vif *vif)
{
	struct ath12k_dp_peer *dp_peer;
	struct ieee80211_sta *sta = NULL;
	struct ath12k_sta *ahsta = NULL;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct wireless_dev *wdev;

	if (params->sta) {
		sta = params->sta;
		ahsta = ath12k_sta_to_ahsta(sta);
	}

	spin_lock_bh(&dp_hw->peer_lock);

	if (!params->is_vdev_peer)
		dp_peer = ath12k_dp_peer_create_find(dp_hw, addr, params->sta,
						     params->is_mlo);
	else
		dp_peer = ath12k_dp_vdev_peer_find(dp_hw, addr, params->hw_link_id);

	if (dp_peer) {
		spin_unlock_bh(&dp_hw->peer_lock);
		ath12k_hw_warn(ah, "dp peer already exists %pM", addr);
		return -EEXIST;
	}

	spin_unlock_bh(&dp_hw->peer_lock);

	if (sta && sta->mlo) {
		ahsta->ml_peer_id = ath12k_wifi7_peer_ml_id_alloc(ah);

		if (ahsta->ml_peer_id == ATH12K_MLO_PEER_ID_INVALID) {
			ath12k_hw_warn(ah, "unable to allocate ML peer id for sta %pM",
				       sta->addr);
			return -ENOMEM;
		}
		params->peer_id = ahsta->ml_peer_id | ATH12K_PEER_ML_ID_VALID;
		ah->num_ml_peers++;
	}

	dp_peer = kzalloc(sizeof(*dp_peer), GFP_KERNEL);
	if (!dp_peer) {
		if (sta && sta->mlo)
			ath12k_wifi7_peer_ml_id_free(ah, ahsta);
		return -ENOMEM;
	}

	dp_peer->link_peer_delete_stats = ath12k_dp_alloc_preserved_stats();
	if (!dp_peer->link_peer_delete_stats) {
		ath12k_err(NULL, "Failed to allocate link peer delete stats");
		if (sta && sta->mlo)
			ath12k_wifi7_peer_ml_id_free(ah, ahsta);
		kfree(dp_peer);
		return -ENOMEM;
	}
	ether_addr_copy(dp_peer->addr, addr);
	dp_peer->sta = params->sta;
	dp_peer->is_mlo = params->is_mlo;
	dp_peer->peer_id = params->is_mlo ? params->peer_id : ATH12K_DP_PEER_ID_INVALID;
	dp_peer->sta_id = ATH12K_DP_PEER_ID_INVALID;
	dp_peer->is_vdev_peer = params->is_vdev_peer;
	/* Update hw_link_id for self bss peer */
	if (dp_peer->is_vdev_peer)
		dp_peer->hw_link_id = params->hw_link_id;
	dp_peer->sec_type = HAL_ENCRYPT_TYPE_OPEN;
	dp_peer->sec_type_grp = HAL_ENCRYPT_TYPE_OPEN;

	/* cache net dev here and reuse it during process rx */
	wdev = ieee80211_vif_to_wdev(vif);
	if (wdev)
		dp_peer->dev = wdev->netdev;

	spin_lock_bh(&dp_hw->peer_lock);

	list_add(&dp_peer->list, &dp_hw->peers);

	if (dp_peer->is_mlo && dp_peer->peer_id < MAX_DP_PEER_LIST_SIZE)
		rcu_assign_pointer(dp_hw->dp_peer_list[dp_peer->peer_id], dp_peer);

	spin_unlock_bh(&dp_hw->peer_lock);

	params->peer_id = ATH12K_DP_PEER_ID_INVALID;
	params->sta_id = ATH12K_DP_PEER_ID_INVALID;

	return 0;
}

void ath12k_wifi7_dp_peer_delete(struct ath12k_dp *dp, struct ath12k_hw *ah, u8 *addr,
				 struct ieee80211_sta *sta, u8 hw_link_id)
{
	struct ath12k_dp_peer *dp_peer;
	u16 peerid_index;
	struct ath12k_sta *ahsta;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;

	spin_lock_bh(&dp_hw->peer_lock);

	if (sta)
		dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, addr, sta);
	else
		dp_peer = ath12k_dp_vdev_peer_find(dp_hw, addr, hw_link_id);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_lock);
		ath12k_dbg(NULL, ATH12K_DBG_PEER, "Failed to find peer:%pM", addr);
		return;
	}

	if (dp_peer->is_mlo) {
		peerid_index = dp_peer->peer_id;
		rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);
		ahsta = ath12k_sta_to_ahsta(dp_peer->sta);
		ath12k_wifi7_peer_ml_id_free(ah, ahsta);
	}

	list_del(&dp_peer->list);

	if (dp_peer->qos && dp_peer->qos->telemetry_peer_ctx)
		ath12k_telemetry_peer_ctx_free(dp_peer->qos->telemetry_peer_ctx);

	spin_unlock_bh(&dp_hw->peer_lock);

	synchronize_rcu();
	kfree(dp_peer->qos);
	ath12k_dp_free_preserved_stats(dp_peer->link_peer_delete_stats);
	kfree(dp_peer);
}

int ath12k_wifi7_dp_peer_assoc(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			       struct ath12k_dp_vif *dp_vif, u8 *addr)
{
	return 0;
}

int ath12k_wifi7_dp_link_peer_create(struct ath12k_base *ab, u32 vdev_id, u8 *addr)
{
	return 0;
}
