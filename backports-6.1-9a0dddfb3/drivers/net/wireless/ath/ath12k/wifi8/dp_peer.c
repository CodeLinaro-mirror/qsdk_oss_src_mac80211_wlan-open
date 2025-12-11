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

static u16 ath12k_wifi8_peer_id_alloc(struct ath12k_dp_hw *dp_hw)
{
	u16 peer_id;
	int i;

	spin_lock_bh(&dp_hw->peer_lock);
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

	spin_unlock_bh(&dp_hw->peer_lock);
	ath12k_dbg(NULL, ATH12K_DBG_PEER, "Allocated peer_id:%d", peer_id);

	return peer_id;
}

static u16 ath12k_wifi8_sta_id_alloc(struct ath12k_dp_hw *dp_hw)
{
	u16 sta_id;
	int i;

	spin_lock_bh(&dp_hw->peer_lock);
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

	spin_unlock_bh(&dp_hw->peer_lock);
	ath12k_dbg(NULL, ATH12K_DBG_PEER, "Allocated sta_id:%d", sta_id);

	return sta_id;
}

int ath12k_wifi8_dp_peer_create(struct ath12k_hw *ah, u8 *addr,
				struct ath12k_dp_peer_create_params *params,
				struct ieee80211_vif *vif)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	struct wireless_dev *wdev;

	spin_lock_bh(&dp_hw->peer_lock);
	if (!params->is_vdev_peer)
		dp_peer = ath12k_dp_peer_create_find(dp_hw, addr, params->sta,
						     params->is_mlo);
	else
		dp_peer = ath12k_dp_vdev_peer_find(dp_hw, addr, params->hw_link_id);

	if (dp_peer) {
		spin_unlock_bh(&dp_hw->peer_lock);
		return -EEXIST;
	}

	spin_unlock_bh(&dp_hw->peer_lock);
	dp_peer = kzalloc(sizeof(*dp_peer), GFP_KERNEL);
	if (!dp_peer)
		return -ENOMEM;

	dp_peer->sta_id = ATH12K_STA_ID_INVALID;
	ether_addr_copy(dp_peer->addr, addr);
	dp_peer->sta = params->sta;
	dp_peer->is_mlo = params->is_mlo;
	dp_peer->peer_id = ath12k_wifi8_peer_id_alloc(dp_hw);
	if (dp_peer->peer_id == ATH12K_MLO_PEER_ID_INVALID) {
		kfree(dp_peer);
		return -ENOMEM;
	}

	if (!params->is_vdev_peer) {
		dp_peer->sta_id = ath12k_wifi8_sta_id_alloc(dp_hw);
		if (dp_peer->sta_id == ATH12K_STA_ID_INVALID) {
			spin_lock_bh(&dp_hw->peer_lock);
			clear_bit(dp_peer->peer_id, dp_hw->free_peer_id_map);
			spin_unlock_bh(&dp_hw->peer_lock);
			kfree(dp_peer);
			return -ENOMEM;
		}
	}

	dp_peer->is_vdev_peer = params->is_vdev_peer;

	dp_peer->sec_type = HAL_ENCRYPT_TYPE_OPEN;
	dp_peer->sec_type_grp = HAL_ENCRYPT_TYPE_OPEN;

	/* cache net dev here and reuse it during process rx */
	wdev = ieee80211_vif_to_wdev(vif);
	if (wdev)
		dp_peer->dev = wdev->netdev;

	spin_lock_bh(&dp_hw->peer_lock);

	list_add(&dp_peer->list, &dp_hw->peers);

	rcu_assign_pointer(dp_hw->dp_peer_list[dp_peer->peer_id], dp_peer);

	spin_unlock_bh(&dp_hw->peer_lock);

	params->peer_id = dp_peer->peer_id;
	params->sta_id = dp_peer->sta_id;
	return 0;
}

void ath12k_wifi8_dp_peer_delete(struct ath12k_dp *dp, struct ath12k_hw *ah, u8 *addr,
				 struct ieee80211_sta *sta, u8 hw_link_id)
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_hw *dp_hw = &ah->dp_hw;
	u16 peerid_index;

	spin_lock_bh(&dp_hw->peer_lock);

	if (sta)
		dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, addr, sta);
	else
		dp_peer = ath12k_dp_vdev_peer_find(dp_hw, addr, hw_link_id);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_lock);
		return;
	}

	peerid_index = dp_peer->peer_id;
	rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);

	list_del(&dp_peer->list);

	clear_bit(dp_peer->peer_id, dp_hw->free_peer_id_map);
	clear_bit(dp_peer->sta_id, dp_hw->free_sta_id_map);
	spin_unlock_bh(&dp_hw->peer_lock);

	synchronize_rcu();
	kfree(dp_peer);
}

u16 ath12k_wifi8_dp_peer_get_peerid_index(struct ath12k_dp *dp, u16 peer_id)
{
	return peer_id;
}

int ath12k_wifi8_dp_link_peer_create(struct ath12k_base *ab, u32 vdev_id, u8 *addr)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	spin_lock_bh(&dp->dp_lock);
	peer = kzalloc(sizeof(*peer), GFP_ATOMIC);
	if (!peer) {
		spin_unlock_bh(&dp->dp_lock);
		return -ENOMEM;
	}

	peer->vdev_id = vdev_id;
	peer->peer_id = ATH12K_MLO_PEER_ID_INVALID;
	ether_addr_copy(peer->addr, addr);
	list_add(&peer->list, &dp->peers);
	ewma_avg_rssi_init(&peer->avg_rssi);

	spin_unlock_bh(&dp->dp_lock);

	return 0;
}

void ath12k_wifi8_dp_link_peer_delete(struct ath12k_base *ab, u32 vdev_id, u8 *addr)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ab->dp, vdev_id, addr);
	if (!peer)
		goto exit;

	list_del(&peer->list);
	kfree(peer);
exit:
	spin_unlock_bh(&dp->dp_lock);
}
