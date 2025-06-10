// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022, 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "core.h"
#include "dp_peer.h"
#include "debug.h"
#include "debugfs.h"

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_vdev_id_and_addr(struct ath12k_dp *dp,
					     int vdev_id, const u8 *addr)
{
	struct ath12k_dp_link_peer *peer;

	lockdep_assert_held(&dp->dp_lock);

	list_for_each_entry(peer, &dp->peers, list) {
		if (peer->vdev_id != vdev_id)
			continue;
		if (!ether_addr_equal(peer->addr, addr))
			continue;

		return peer;
	}

	return NULL;
}

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_pdev_idx(struct ath12k_dp *dp, u8 pdev_idx,
				     const u8 *addr)
{
	struct ath12k_dp_link_peer *peer;

	lockdep_assert_held(&dp->dp_lock);

	list_for_each_entry(peer, &dp->peers, list) {
		if (peer->pdev_idx != pdev_idx)
			continue;
		if (!ether_addr_equal(peer->addr, addr))
			continue;

		return peer;
	}

	return NULL;
}

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_addr(struct ath12k_dp *dp, const u8 *addr)
{
	lockdep_assert_held(&dp->dp_lock);

	if (!dp->rhead_peer_addr)
		return NULL;

	return rhashtable_lookup_fast(dp->rhead_peer_addr, addr,
				      dp->rhash_peer_addr_param);
}
EXPORT_SYMBOL(ath12k_dp_link_peer_find_by_addr);

static struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_ml_id(struct ath12k_dp *dp, int ml_peer_id)
{
	struct ath12k_dp_link_peer *peer;

	lockdep_assert_held(&dp->dp_lock);

	list_for_each_entry(peer, &dp->peers, list)
		if (ml_peer_id == peer->ml_id)
			return peer;

	return NULL;
}

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_id(struct ath12k_dp *dp, int peer_id)
{
	struct ath12k_dp_link_peer *peer;

	lockdep_assert_held(&dp->dp_lock);

	if (peer_id == ATH12K_PEER_ID_INVALID)
		return NULL;

	if (peer_id & ATH12K_PEER_ML_ID_VALID)
		return ath12k_dp_link_peer_find_by_ml_id(dp, peer_id);

	list_for_each_entry(peer, &dp->peers, list)
		if (peer_id == peer->peer_id)
			return peer;

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_link_peer_find_by_id);

/* ToDO: Need to see it it can be optimized */
static struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_ml_vdev_id(struct ath12k_dp *dp,
				       int ml_peer_id,
				       int vdev_id)
{
	struct ath12k_dp_link_peer *peer;

	lockdep_assert_held(&dp->dp_lock);

	list_for_each_entry(peer, &dp->peers, list)
		if (ml_peer_id == peer->ml_id &&
		    vdev_id == peer->vdev_id)
			return peer;

	return NULL;
}

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_ml_peer_vdev_id(struct ath12k_dp *dp,
					    int peer_id,
					    int vdev_id)
{
	struct ath12k_dp_link_peer *peer;

	lockdep_assert_held(&dp->dp_lock);

	if (peer_id == ATH12K_PEER_ID_INVALID)
		return NULL;

	if (peer_id & ATH12K_PEER_ML_ID_VALID)
		return ath12k_dp_link_peer_find_by_ml_vdev_id(dp,
							      peer_id,
							      vdev_id);

	list_for_each_entry(peer, &dp->peers, list)
		if (peer_id == peer->peer_id)
			return peer;

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_link_peer_find_by_ml_peer_vdev_id);

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_ast(struct ath12k_dp *dp,
				int ast_hash)
{
	struct ath12k_dp_link_peer *peer;

	lockdep_assert_held(&dp->dp_lock);

	list_for_each_entry(peer, &dp->peers, list)
		if (ast_hash == peer->ast_hash)
			return peer;

	return NULL;
}

bool ath12k_dp_link_peer_exist_by_vdev_id(struct ath12k_dp *dp, int vdev_id)
{
	struct ath12k_dp_link_peer *peer;

	spin_lock_bh(&dp->dp_lock);

	list_for_each_entry(peer, &dp->peers, list) {
		if (vdev_id == peer->vdev_id) {
			spin_unlock_bh(&dp->dp_lock);
			return true;
		}
	}
	spin_unlock_bh(&dp->dp_lock);
	return false;
}

void ath12k_peer_unmap_event(struct ath12k_base *ab, u16 peer_id)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_id(dp, peer_id);
	if (!peer) {
		ath12k_warn(ab, "peer-unmap-event: unknown peer id %d\n",
			    peer_id);
		goto exit;
	}

	ath12k_dbg(ab, ATH12K_DBG_PEER, "htt peer unmap vdev %d peer %pM id %d\n",
		   peer->vdev_id, peer->addr, peer_id);

	list_del(&peer->list);
	kfree(peer);
	wake_up(&ab->peer_mapping_wq);

exit:
	spin_unlock_bh(&dp->dp_lock);
}

void ath12k_peer_map_event(struct ath12k_base *ab, u8 vdev_id, u16 peer_id,
			   u8 *mac_addr, u16 ast_hash, u16 hw_peer_id)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, mac_addr);
	if (!peer) {
		peer = kzalloc(sizeof(*peer), GFP_ATOMIC);
		if (!peer)
			goto exit;

		peer->vdev_id = vdev_id;
		peer->peer_id = peer_id;
		peer->ast_hash = ast_hash;
		peer->hw_peer_id = hw_peer_id;
		ether_addr_copy(peer->addr, mac_addr);
		list_add(&peer->list, &dp->peers);
		wake_up(&ab->peer_mapping_wq);
		ewma_avg_rssi_init(&peer->avg_rssi);
	}
	ath12k_dbg(ab, ATH12K_DBG_PEER, "htt peer map vdev %d peer %pM id %d\n",
		   vdev_id, mac_addr, peer_id);

exit:
	spin_unlock_bh(&dp->dp_lock);
}

void ath12k_peer_mlo_map_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_htt_mlo_peer_map_msg *msg;
	struct ath12k_dp_link_peer *peer;
	u16 ml_peer_id;
	u16 mld_mac_h16;
	u8 mld_addr[ETH_ALEN];
	u16 ast_idx;
	u16 cache_num;

	msg = (struct ath12k_htt_mlo_peer_map_msg *)skb->data;

	ml_peer_id = FIELD_GET(ATH12K_HTT_MLO_PEER_MAP_INFO0_PEER_ID, msg->info0);

	ml_peer_id |= ATH12K_PEER_ML_ID_VALID;

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_id(dp, ml_peer_id);

	/* TODO a sync wait to check ml peer map success or delete
	 * ml peer info in all link peers and make peer assoc failure
	 * TBA after testing basic changes
	 */
	if (!peer) {
		ath12k_warn(ab, "peer corresponding to ml peer id %d not found", ml_peer_id);
		spin_unlock_bh(&dp->dp_lock);
		return;
	}
	mld_mac_h16 = FIELD_GET(ATH12K_HTT_MLO_PEER_MAP_MAC_ADDR_H16,
				msg->mac_addr.mac_addr_h16);
	ast_idx = FIELD_GET(ATH12K_HTT_MLO_PEER_MAP_AST_IDX, msg->info1);
	cache_num = FIELD_GET(ATH12K_HTT_MLO_PEER_MAP_CACHE_SET_NUM, msg->info1);
	ath12k_dp_get_mac_addr(msg->mac_addr.mac_addr_l32, mld_mac_h16, mld_addr);

	peer->hw_peer_id = ast_idx;
	peer->ast_hash = cache_num;

	WARN_ON(memcmp(mld_addr, peer->ml_addr, ETH_ALEN));

	spin_unlock_bh(&dp->dp_lock);

	ath12k_dbg(ab, ATH12K_DBG_PEER, "htt MLO peer map peer %pM id %d\n",
		   mld_addr, ml_peer_id);

	/* TODO rx queue setup for the ML peer */
}

void ath12k_peer_mlo_unmap_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_htt_mlo_peer_unmap_msg *msg;
	u16 ml_peer_id;

	msg = (struct ath12k_htt_mlo_peer_unmap_msg *)skb->data;

	ml_peer_id = FIELD_GET(ATH12K_HTT_MLO_PEER_UNMAP_PEER_ID, msg->info0);

	ml_peer_id |= ATH12K_PEER_ML_ID_VALID;

	ath12k_dbg(ab, ATH12K_DBG_PEER, "htt MLO peer unmap peer ml id %d\n", ml_peer_id);
}

static int ath12k_dp_link_peer_rhash_addr_tbl_init(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct rhashtable_params *param;
	struct rhashtable *rhash_addr_tbl;
	int ret;
	size_t size;

	lockdep_assert_held(&dp->tbl_mtx_lock);

	if (dp->rhead_peer_addr)
		return 0;

	size = sizeof(*dp->rhead_peer_addr);
	rhash_addr_tbl = kzalloc(size, GFP_KERNEL);
	if (!rhash_addr_tbl)
		return -ENOMEM;

	param = &dp->rhash_peer_addr_param;

	param->key_offset = offsetof(struct ath12k_dp_link_peer, addr);
	param->head_offset = offsetof(struct ath12k_dp_link_peer, rhash_addr);
	param->key_len = sizeof_field(struct ath12k_dp_link_peer, addr);
	param->automatic_shrinking = true;
	param->nelem_hint = dp->num_radios * ath12k_core_get_max_peers_per_radio(ab);

	ret = rhashtable_init(rhash_addr_tbl, param);
	if (ret) {
		ath12k_warn(ab, "failed to init peer addr rhash table %d\n", ret);
		goto err_free;
	}

	if (!dp->rhead_peer_addr)
		dp->rhead_peer_addr = rhash_addr_tbl;
	else
		goto cleanup_tbl;

	return 0;

cleanup_tbl:
	rhashtable_destroy(rhash_addr_tbl);
err_free:
	kfree(rhash_addr_tbl);

	return ret;
}

int ath12k_dp_link_peer_rhash_tbl_init(struct ath12k_dp *dp)
{
	int ret;

	mutex_lock(&dp->tbl_mtx_lock);
	ret = ath12k_dp_link_peer_rhash_addr_tbl_init(dp);
	mutex_unlock(&dp->tbl_mtx_lock);

	return ret;
}

void ath12k_dp_link_peer_rhash_tbl_destroy(struct ath12k_dp *dp)
{
	mutex_lock(&dp->tbl_mtx_lock);

	if (!dp->rhead_peer_addr)
		goto unlock;

	rhashtable_destroy(dp->rhead_peer_addr);
	kfree(dp->rhead_peer_addr);
	dp->rhead_peer_addr = NULL;

unlock:
	mutex_unlock(&dp->tbl_mtx_lock);
}

static int ath12k_dp_link_peer_rhash_insert(struct ath12k_dp *dp,
					    struct rhashtable *rtbl,
					    struct rhash_head *rhead,
					    struct rhashtable_params *params,
					    void *key)
{
	struct ath12k_peer *tmp;

	lockdep_assert_held(&dp->dp_lock);

	tmp = rhashtable_lookup_get_insert_fast(rtbl, rhead, *params);

	if (!tmp)
		return 0;
	else if (IS_ERR(tmp))
		return PTR_ERR(tmp);
	else
		return -EEXIST;
}

static int ath12k_dp_link_peer_rhash_remove(struct ath12k_dp *dp,
					    struct rhashtable *rtbl,
					    struct rhash_head *rhead,
					    struct rhashtable_params *params)
{
	int ret;

	lockdep_assert_held(&dp->dp_lock);

	ret = rhashtable_remove_fast(rtbl, rhead, *params);
	if (ret && ret != -ENOENT)
		return ret;

	return 0;
}

int ath12k_dp_link_peer_rhash_add(struct ath12k_dp *dp,
				  struct ath12k_dp_link_peer *peer)
{
	int ret;

	lockdep_assert_held(&dp->dp_lock);

	if (!dp->rhead_peer_addr)
		return -EPERM;

	if (peer->rhash_done)
		return 0;

	ret = ath12k_dp_link_peer_rhash_insert(dp, dp->rhead_peer_addr, &peer->rhash_addr,
					       &dp->rhash_peer_addr_param, &peer->addr);
	if (ret) {
		ath12k_warn(dp, "failed to add peer %pM with id %d in rhash_addr ret %d\n",
			    peer->addr, peer->peer_id, ret);
		peer->rhash_done = false;
	} else {
		peer->rhash_done = true;
	}

	return ret;
}

int ath12k_dp_link_peer_rhash_delete(struct ath12k_dp *dp,
				     struct ath12k_dp_link_peer *peer)
{
	int ret;

	lockdep_assert_held(&dp->dp_lock);

	if (!dp->rhead_peer_addr)
		return -EPERM;

	if (!peer->rhash_done)
		return 0;

	ret = ath12k_dp_link_peer_rhash_remove(dp, dp->rhead_peer_addr, &peer->rhash_addr,
					       &dp->rhash_peer_addr_param);
	if (ret) {
		ath12k_warn(dp, "failed to remove peer %pM with id %d in rhash_addr ret %d\n",
			    peer->addr, peer->peer_id, ret);
		return ret;
	}

	peer->rhash_done = false;

	return 0;
}

struct ath12k_dp_peer *ath12k_dp_peer_find(struct ath12k_dp_hw *dp_hw, u8 *addr)
{
	struct ath12k_dp_peer *peer;

	lockdep_assert_held(&dp_hw->peer_lock);

	list_for_each_entry(peer, &dp_hw->peers, list) {
		if (!ether_addr_equal(peer->addr, addr))
			continue;

		return peer;
	}

	return NULL;
}

#define PEER_TABLE_SOC_ID_SHIFT        10

u16 ath12k_dp_peer_get_peerid_index(struct ath12k_dp *dp, u16 peer_id)
{
	return (peer_id & ATH12K_PEER_ML_ID_VALID) ? peer_id :
		((dp->device_id << PEER_TABLE_SOC_ID_SHIFT) | peer_id);
}

struct ath12k_dp_peer *ath12k_dp_peer_find_by_peerid_index(struct ath12k_dp *dp,
							   struct ath12k_pdev_dp *dp_pdev,
							   u16 peer_id)
{
	u16 index;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(),
			 "ath12k dp peer find by peerid index called without rcu lock");

	if (peer_id >= ATH12K_PEER_ID_INVALID)
		return NULL;

	index = ath12k_dp_peer_get_peerid_index(dp, peer_id);

	return rcu_dereference(dp_pdev->dp_hw->dp_peer_list[index]);
}
EXPORT_SYMBOL(ath12k_dp_peer_find_by_peerid_index);

struct ath12k_dp_link_peer *
ath12k_dp_link_peer_find_by_peerid_index(struct ath12k_dp *dp,
					 struct ath12k_pdev_dp *dp_pdev, u16 peer_id)
{
	struct ath12k_dp_peer *dp_peer = NULL;
	u8 link_id;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(),
			 "ath12k dp link peer find by peerid index called without rcu lock");

	if (dp_pdev->hw_link_id >= ATH12K_NUM_MAX_LINKS)
		return NULL;

	dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (!dp_peer)
		return NULL;

	link_id = dp_peer->hw_links[dp_pdev->hw_link_id];

	return rcu_dereference(dp_peer->link_peers[link_id]);
}

int ath12k_dp_peer_create(struct ath12k_dp_hw *dp_hw, u8 *addr,
			  struct ath12k_dp_peer_create_params *params,
			  struct ieee80211_vif *vif)
{
	struct ath12k_dp_peer *dp_peer;
	struct wireless_dev *wdev;

	spin_lock_bh(&dp_hw->peer_lock);
	dp_peer = ath12k_dp_peer_find(dp_hw, addr);
	spin_unlock_bh(&dp_hw->peer_lock);

	if (dp_peer)
		return -EEXIST;

	dp_peer = kzalloc(sizeof(*dp_peer), GFP_ATOMIC);
	if (!dp_peer)
		return -ENOMEM;

	ether_addr_copy(dp_peer->addr, addr);
	dp_peer->sta = params->sta;
	dp_peer->is_mlo = params->is_mlo;
	dp_peer->peer_id = params->is_mlo ? params->peer_id : ATH12K_DP_PEER_ID_INVALID;
	dp_peer->is_vdev_peer = params->is_vdev_peer;

	dp_peer->sec_type = HAL_ENCRYPT_TYPE_OPEN;
	dp_peer->sec_type_grp = HAL_ENCRYPT_TYPE_OPEN;

	/* cache net dev here and reuse it during process rx */
	wdev = ieee80211_vif_to_wdev(vif);
	if (wdev)
		dp_peer->dev = wdev->netdev;

	spin_lock_bh(&dp_hw->peer_lock);

	list_add(&dp_peer->list, &dp_hw->peers);

	if (dp_peer->is_mlo)
		rcu_assign_pointer(dp_hw->dp_peer_list[dp_peer->peer_id], dp_peer);

	spin_unlock_bh(&dp_hw->peer_lock);

	return 0;
}

void ath12k_dp_peer_delete(struct ath12k_dp_hw *dp_hw, u8 *addr)
{
	struct ath12k_dp_peer *dp_peer;
	u16 peerid_index;

	spin_lock_bh(&dp_hw->peer_lock);

	dp_peer = ath12k_dp_peer_find(dp_hw, addr);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_lock);
		return;
	}

	if (dp_peer->is_mlo) {
		peerid_index = dp_peer->peer_id;
		rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);
	}

	list_del(&dp_peer->list);

	spin_unlock_bh(&dp_hw->peer_lock);

	synchronize_rcu();
	kfree(dp_peer);
}

int ath12k_dp_link_peer_assign(struct ath12k *ar, u8 vdev_id,
			       u8 *dp_peer_addr, u8 *addr, u8 link_id,
			       u32 hw_link_id, struct ieee80211_vif *vif)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_hw *dp_hw = &ar->ah->dp_hw;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_link_peer *peer;
	u16 peerid_index;
	int ret;
	u8 *dp_peer_mac = dp_peer_addr;

	if (!dp_peer_addr) {
		struct ath12k_dp_peer_create_params params = {0};

		params.is_vdev_peer = true;

		ath12k_dp_peer_create(dp_hw, addr, &params, vif);

		dp_peer_mac = addr;
	}

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, addr);
	if (!peer) {
		ret = -ENOENT;
		goto err_peer;
	}

	spin_lock_bh(&dp_hw->peer_lock);

	dp_peer = ath12k_dp_peer_find(dp_hw, dp_peer_mac);
	if (!dp_peer) {
		ret = -ENOENT;
		goto err_dp_peer;
	}

	peer->dp_peer = dp_peer;
	peer->hw_link_id = hw_link_id;
	peer->tcl_metadata |= u32_encode_bits(0, HTT_TCL_META_DATA_TYPE) |
			      u32_encode_bits(peer->peer_id, HTT_TCL_META_DATA_PEER_ID);
	peer->tcl_metadata &= ~HTT_TCL_META_DATA_VALID_HTT;

	if (ath12k_debugfs_is_extd_rx_stats_enabled(dp_pdev->ar) &&
				!peer->peer_stats.rx_stats) {
		peer->peer_stats.rx_stats = kzalloc(sizeof(*peer->peer_stats.rx_stats), GFP_ATOMIC);
	}

	dp_peer->hw_links[peer->hw_link_id] = link_id;

	peerid_index = ath12k_dp_peer_get_peerid_index(dp, peer->peer_id);

	rcu_assign_pointer(dp_peer->link_peers[peer->link_id], peer);

	rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], dp_peer);

	spin_unlock_bh(&dp_hw->peer_lock);

	ath12k_dp_link_peer_rhash_add(dp, peer);

	spin_unlock_bh(&dp->dp_lock);

	return 0;

err_dp_peer:
	spin_unlock_bh(&dp_hw->peer_lock);

err_peer:
	spin_unlock_bh(&dp->dp_lock);

	return ret;
}

void ath12k_dp_link_peer_unassign(struct ath12k *ar, u8 vdev_id, u8 *addr)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_hw *dp_hw = &ar->ah->dp_hw;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_link_peer *peer;
	u16 peerid_index;

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, addr);
	if (!peer) {
		spin_unlock_bh(&dp->dp_lock);
		return;
	}

	spin_lock_bh(&dp_hw->peer_lock);

	dp_peer = peer->dp_peer;
	dp_peer->hw_links[peer->hw_link_id] = 0;

	peerid_index = ath12k_dp_peer_get_peerid_index(dp, peer->peer_id);

	rcu_assign_pointer(dp_peer->link_peers[peer->link_id], NULL);

	rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);

	spin_unlock_bh(&dp_hw->peer_lock);

	if (peer->peer_stats.rx_stats)
		kfree(peer->peer_stats.rx_stats);

	ath12k_dp_link_peer_rhash_delete(dp, peer);

	peer->dp_peer = NULL;

	spin_unlock_bh(&dp->dp_lock);

	synchronize_rcu();

	if (dp_peer->is_vdev_peer)
		ath12k_dp_peer_delete(dp_hw, addr);
}

void ath12k_link_peer_get_sta_rate_info_stats(struct ath12k_dp *dp, const u8 *addr,
					      struct ath12k_dp_link_peer_rate_info *rate_info)
{
	struct ath12k_dp_link_peer *link_peer;

	spin_lock_bh(&dp->dp_lock);
	link_peer = ath12k_dp_link_peer_find_by_addr(dp, addr);
	if (!link_peer) {
		spin_unlock_bh(&dp->dp_lock);
		return;
	}

	rate_info->rx_duration = link_peer->rx_duration;
	rate_info->tx_duration = link_peer->tx_duration;
	rate_info->txrate.legacy = link_peer->txrate.legacy;
	rate_info->txrate.mcs = link_peer->txrate.mcs;
	rate_info->txrate.nss = link_peer->txrate.nss;
	rate_info->txrate.bw = link_peer->txrate.bw;
	rate_info->txrate.he_gi = link_peer->txrate.he_gi;
	rate_info->txrate.he_dcm = link_peer->txrate.he_dcm;
	rate_info->txrate.he_ru_alloc = link_peer->txrate.he_ru_alloc;
	rate_info->txrate.flags = link_peer->txrate.flags;
	rate_info->rssi_comb = link_peer->rssi_comb;
	rate_info->signal_avg = ewma_avg_rssi_read(&link_peer->avg_rssi);
	rate_info->tx_retry_count = link_peer->tx_retry_count;
	rate_info->tx_retry_failed = link_peer->tx_retry_failed;

	spin_unlock_bh(&dp->dp_lock);
}

bool ath12k_dp_link_peer_reset_rx_stats(struct ath12k_dp *dp, const u8 *addr)
{
	struct ath12k_rx_peer_stats *rx_stats = NULL;
	struct ath12k_dp_link_peer *link_peer;

	spin_lock_bh(&dp->dp_lock);
	link_peer = ath12k_dp_link_peer_find_by_addr(dp, addr);
	if (!link_peer) {
		spin_unlock_bh(&dp->dp_lock);
		return false;
	}

	if (!link_peer->peer_stats.rx_stats) {
		spin_unlock_bh(&dp->dp_lock);
		return false;
	}

	rx_stats = link_peer->peer_stats.rx_stats;
	memset(rx_stats, 0, sizeof(*rx_stats));

	spin_unlock_bh(&dp->dp_lock);
	return true;
}
