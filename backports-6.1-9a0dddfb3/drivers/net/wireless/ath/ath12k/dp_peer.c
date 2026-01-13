// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "dp_peer.h"
#include "debug.h"
#include "debugfs.h"
#include "telemetry_agent_if.h"

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
EXPORT_SYMBOL(ath12k_dp_link_peer_find_by_vdev_id_and_addr);

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

void ath12k_link_peer_free(struct ath12k_dp_link_peer *peer)
{
	if (!peer)
		return;

	list_del(&peer->list);

	kfree(peer->peer_stats.rx_stats);
	kfree(peer->peer_stats.tx_stats);
	kfree(peer->peer_stats.qos_stats);

	kfree(peer);
}
EXPORT_SYMBOL(ath12k_link_peer_free);

void ath12k_peer_unmap_event(struct ath12k_base *ab, u16 peer_id, bool is_wds)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (is_wds)
		return;

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_id(dp, peer_id);
	if (!peer) {
		ath12k_dbg(ab, ATH12K_DBG_PEER, "peer-unmap-event: unknown peer id %d\n",
			   peer_id);
		goto exit;
	}

	ath12k_dbg(ab, ATH12K_DBG_PEER, "htt peer unmap vdev %d peer %pM id %d\n",
		   peer->vdev_id, peer->addr, peer_id);

	ath12k_link_peer_free(peer);

exit:
	spin_unlock_bh(&dp->dp_lock);
}

void ath12k_peer_map_event(struct ath12k_base *ab, u8 vdev_id, u16 peer_id,
			   u8 *mac_addr, u16 ast_hash, u16 hw_peer_id, bool is_wds)
{
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (is_wds)
		return;

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
		ewma_avg_snr_init(&peer->signal_stats.avg_snr);
		ewma_avg_snr_dp_init(&peer->signal_stats.avg_snr_dp);
		ewma_avg_rssi_init(&peer->signal_stats.avg_rssi);
		ewma_avg_rssi_dp_init(&peer->signal_stats.avg_rssi_dp);
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
EXPORT_SYMBOL(ath12k_dp_peer_find);

struct ath12k_dp_peer *ath12k_dp_peer_find_by_addr_and_sta(struct ath12k_dp_hw *dp_hw, u8 *addr,
							   struct ieee80211_sta *sta)
{
	struct ath12k_dp_peer *dp_peer;

	lockdep_assert_held(&dp_hw->peer_lock);

	list_for_each_entry(dp_peer, &dp_hw->peers, list) {
		if (ether_addr_equal(dp_peer->addr, addr) && (dp_peer->sta == sta))
			return dp_peer;
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_peer_find_by_addr_and_sta);

struct ath12k_dp_peer *ath12k_dp_vdev_peer_find(struct ath12k_dp_hw *dp_hw,
						u8 *addr, u8 hw_link_id)
{
	struct ath12k_dp_peer *dp_peer;

	lockdep_assert_held(&dp_hw->peer_lock);

	list_for_each_entry(dp_peer, &dp_hw->peers, list) {
		if (ether_addr_equal(dp_peer->addr, addr) &&
		    dp_peer->hw_link_id == hw_link_id)
			return dp_peer;
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_vdev_peer_find);

struct ath12k_dp_peer *ath12k_dp_peer_create_find(struct ath12k_dp_hw *dp_hw, u8 *addr,
						  struct ieee80211_sta *sta,
						  bool mlo_peer)
{
	struct ath12k_dp_peer *dp_peer;

	lockdep_assert_held(&dp_hw->peer_lock);

	list_for_each_entry(dp_peer, &dp_hw->peers, list) {
		if (ether_addr_equal(dp_peer->addr, addr)) {
			if (!sta || mlo_peer || dp_peer->is_mlo ||
			    dp_peer->sta == sta)
				return dp_peer;
		}
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_dp_peer_create_find);

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

	if (dp_pdev->hw_link_id >= ATH12K_GROUP_MAX_RADIO)
		return NULL;

	dp_peer = ath12k_dp_peer_find_by_peerid_index(dp, dp_pdev, peer_id);
	if (!dp_peer)
		return NULL;

	link_id = dp_peer->hw_links[dp_pdev->hw_link_id];

	return rcu_dereference(dp_peer->link_peers[link_id]);
}
EXPORT_SYMBOL(ath12k_dp_link_peer_find_by_peerid_index);

u16 ath12k_dp_peer_get_peer_id(struct ath12k_dp_hw *dp_hw, u8 *addr)
{
	struct ath12k_dp_peer *dp_peer;
	int peer_id = ATH12K_MLO_PEER_ID_INVALID;

	spin_lock_bh(&dp_hw->peer_lock);

	dp_peer = ath12k_dp_peer_find(dp_hw, addr);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_lock);
		return ATH12K_MLO_PEER_ID_INVALID;
	}

	peer_id = dp_peer->peer_id;
	spin_unlock_bh(&dp_hw->peer_lock);
	return peer_id;
}

u16 ath12k_dp_peer_get_sta_id(struct ath12k_dp_hw *dp_hw, u8 *addr)
{
	struct ath12k_dp_peer *dp_peer;
	int sta_id = ATH12K_STA_ID_INVALID;

	spin_lock_bh(&dp_hw->peer_lock);

	dp_peer = ath12k_dp_peer_find(dp_hw, addr);
	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_lock);
		return ATH12K_STA_ID_INVALID;
	}

	sta_id = dp_peer->sta_id;
	spin_unlock_bh(&dp_hw->peer_lock);
	return sta_id;
}

int ath12k_dp_link_peer_assign(struct ath12k *ar, u8 vdev_id,
			       struct ieee80211_sta *sta, u8 *addr, u8 link_id,
			       u32 hw_link_id, struct ieee80211_vif *vif,
			       u8 vp_type, int vp_num)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_hw *dp_hw = &ar->ah->dp_hw;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_dp_link_peer *peer, *temp_peer;
	u16 peerid_index;
	int ret;
	u8 *dp_peer_mac = !sta ? addr : sta->addr;
	bool is_vdev_peer = false;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, addr);
	if (!peer) {
		ret = -ENOENT;
		goto err_peer;
	}

	if (!sta)
		is_vdev_peer = true;

	spin_lock_bh(&dp_hw->peer_lock);

	if (!is_vdev_peer)
		dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw, dp_peer_mac, sta);
	else
		dp_peer = ath12k_dp_vdev_peer_find(dp_hw, dp_peer_mac, hw_link_id);

	if (!dp_peer) {
		ret = -ENOENT;
		goto err_dp_peer;
	}

	peer->dp_peer = dp_peer;
	peer->hw_link_id = hw_link_id;
	peer->tcl_metadata |= u32_encode_bits(0, HTT_TCL_META_DATA_TYPE) |
			      u32_encode_bits(peer->peer_id, HTT_TCL_META_DATA_PEER_ID);
	peer->tcl_metadata &= ~HTT_TCL_META_DATA_VALID_HTT;

	if (ath12k_extd_rx_stats_enabled(dp_pdev->ar) &&
	    !peer->peer_stats.rx_stats) {
		peer->peer_stats.rx_stats = kzalloc(sizeof(*peer->peer_stats.rx_stats), GFP_ATOMIC);
	}

	if (ath12k_extd_tx_stats_enabled(dp_pdev->ar) &&
	    !peer->peer_stats.tx_stats) {
		peer->peer_stats.tx_stats = kzalloc(sizeof(*peer->peer_stats.tx_stats),
						    GFP_ATOMIC);
		peer->peer_stats.tx_stats->avg_ack_rssi = INVALID_RSSI;
		peer->peer_stats.tx_stats->avg_tx_rate = INVALID_RATE;
	}

	dp_peer->qos_stats_lvl = (ar->dp.qos_stats &
				  ATH12K_QOS_STATS_COLLECTION_MASK) >> 2;

	dp_peer->hw_links[peer->hw_link_id] = link_id;

	if (vif->type == NL80211_IFTYPE_AP)
		dp_peer->is_reset_mcbc = true;

	/* Do not deliver frames to PPE in fast rx incase of RFS
	 * RFS is supported only in SFE Mode
	 */
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (vp_type == PPE_VP_USER_TYPE_ACTIVE || vp_type == PPE_VP_USER_TYPE_DS)
		dp_peer->ppe_vp_num = vp_num;
#endif

	if (peer->peer_id != ATH12K_MLO_PEER_ID_INVALID) {
		peerid_index = ath12k_dp_peer_get_peerid_index(dp, peer->peer_id);
		rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], dp_peer);
		/* Set peer_id in dp_peer for non-mlo client, peer_id for mlo client is
		 * set during dp_peer create
		 */
		if (!dp_peer->is_mlo) {
			dp_peer->peer_id = peer->peer_id;
			if (!is_vdev_peer)
				ahsta->dp_peer_id = peer->peer_id;
		}
	}

	if (!dp_peer->is_vdev_peer)
		dp_peer->peer_links_map |= BIT(link_id);

	rcu_assign_pointer(dp_peer->link_peers[peer->link_id], peer);

	spin_unlock_bh(&dp_hw->peer_lock);

	/* In case of Split PHY and roaming scenario, pdev idx
	 * might differ but both the pdev will share same rhash
	 * table. In that case update the rhash table if link_peer is
	 * already present
	 */
	temp_peer = ath12k_dp_link_peer_find_by_addr(dp, addr);
	if (temp_peer && temp_peer->hw_link_id != ar->hw_link_id)
		ath12k_dp_link_peer_rhash_delete(dp, temp_peer);

	ath12k_dp_link_peer_rhash_add(dp, peer);

	peer->is_assigned = true;

	spin_unlock_bh(&dp->dp_lock);

	return 0;

err_dp_peer:
	spin_unlock_bh(&dp_hw->peer_lock);

err_peer:
	spin_unlock_bh(&dp->dp_lock);

	return ret;
}

/**
 * ath12k_dp_capture_link_peer_stats - Capture all stats from link peer
 * @aggr_stats: Aggregation structure to capture stats into
 * @peer: Link peer whose stats need to be captured
 * @dp_peer: MLD peer associated with the link peer
 * @stats_link_id: Link ID for stats array indexing
 *
 * Captures HTT TX stats, per-packet TX stats (all TCL rings), RX peer stats,
 * and per-packet RX stats (all REO rings) from a link peer into the provided
 * aggregation structure. This is used to preserve statistics before link peer
 * deletion.
 *
 */

static void
ath12k_dp_capture_link_peer_stats(struct ath12k_dp_preserved_stats *aggr_stats,
				  struct ath12k_dp_link_peer *peer,
				  struct ath12k_dp_peer *dp_peer,
				  u8 stats_link_id)
{
	int i;

	if (peer->peer_stats.tx_stats)
		ath12k_dp_aggr_htt_tx_stats(&aggr_stats->tx_stats,
					    peer->peer_stats.tx_stats);

	for (i = 0; i < DP_TCL_NUM_RING_MAX; i++)
		ath12k_dp_aggr_per_pkt_tx_stats(&aggr_stats->per_pkt_tx[i],
						&dp_peer->stats[stats_link_id].tx[i]);

	if (peer->peer_stats.rx_stats)
		ath12k_dp_aggr_rx_peer_stats(&aggr_stats->rx_stats,
					     peer->peer_stats.rx_stats);

	for (i = 0; i < DP_REO_DST_RING_MAX; i++)
		ath12k_dp_aggr_per_pkt_rx_stats(&aggr_stats->per_pkt_rx[i],
						&dp_peer->stats[stats_link_id].rx[i]);
	/* Capture WBM RX error stats */
	ath12k_dp_aggr_wbm_rx_stats(&aggr_stats->wbm_err,
				    &dp_peer->stats[stats_link_id].wbm_err);
}

/**
 * ath12k_dp_aggr_link_peer_to_mld_peer - Aggregate link peer stats to MLD peer
 * @peer: Link peer whose stats need to be aggregated
 * @dp_peer: MLD peer to aggregate stats into
 * @stats_link_id: Link ID for stats indexing
 *
 * This function aggregates statistics from a link peer to its corresponding
 * MLD peer before the link peer is deleted. Caller must hold the locks before
 * calling this.
 */
static void ath12k_dp_aggr_link_peer_to_mld_peer(struct ath12k_dp_link_peer *peer,
						 struct ath12k_dp_peer *dp_peer,
						 u8 stats_link_id)
{
	if (!peer || !dp_peer)
		return;

	ath12k_dp_capture_link_peer_stats(dp_peer->link_peer_delete_stats,
					  peer, dp_peer, stats_link_id);
}

/**
 * ath12k_dp_aggr_link_peer_to_link_vif - Aggregate link peer stats to link VIF
 * @dp_link_vif: Link VIF to aggregate stats into
 * @peer: Link peer whose stats need to be aggregated
 * @dp_peer: MLD peer associated with the link peer
 * @stats_link_id: Link ID for stats array indexing
 *
 * Aggregates statistics from a link peer to its associated link VIF before
 * the link peer is deleted. This preserves per-VIF statistics across peer
 * lifecycle events. Returns early if any pointer is NULL.
 */

void ath12k_dp_aggr_link_peer_to_link_vif(struct ath12k_dp_link_vif *dp_link_vif,
					  struct ath12k_dp_link_peer *peer,
					  struct ath12k_dp_peer *dp_peer,
					  u8 stats_link_id)
{
	if (!peer || !dp_peer || !dp_link_vif)
		return;

	ath12k_dp_capture_link_peer_stats(dp_link_vif->link_peer_delete_stats,
					  peer, dp_peer, stats_link_id);
}

/**
 * ath12k_dp_aggr_clear_per_pkt_stats - Clear per-packet stats after aggregation
 * @peer: MLD peer whose per-packet stats need to be cleared
 * @stats_link_id: Link ID for stats array indexing
 *
 * Clears per-packet TX stats (all TCL rings) and per-packet RX stats (all REO
 * rings) for the specified link ID to prevent double-counting when stats are
 * reused or aggregated multiple times. Should be called after stats have been
 * aggregated to MLD peer or link VIF.
 */

static void ath12k_dp_aggr_clear_per_pkt_stats(struct ath12k_dp_peer *peer,
					       u8 stats_link_id)
{
	ath12k_dp_clear_per_pkt_tx_stats(&peer->stats[stats_link_id]);
	ath12k_dp_clear_per_pkt_rx_stats(&peer->stats[stats_link_id]);
	ath12k_dp_clear_wbm_rx_stats(&peer->stats[stats_link_id].wbm_err);
}

static void __ath12k_dp_link_peer_unassign(struct ath12k *ar,
					   struct ath12k_dp *dp,
					   struct ath12k_dp_hw *dp_hw,
					   struct ath12k_dp_link_peer *peer,
					   struct ath12k_dp_link_vif *link_vif,
					   u8 *addr)
{
	struct ath12k_dp_link_peer *temp_peer;
	struct ath12k_dp_peer *dp_peer;
	int stats_link_id;
	u16 peerid_index;

	spin_lock_bh(&dp_hw->peer_lock);

	dp_peer = peer->dp_peer;
	stats_link_id = peer->link_id;
	dp_peer->hw_links[peer->hw_link_id] = 0;

	if (!dp_peer->is_vdev_peer) {
		dp_peer->peer_links_map &= ~BIT(peer->link_id);
		/* Preserve link peer stats to MLD peer before deletion */
		ath12k_dp_aggr_link_peer_to_mld_peer(peer, dp_peer, stats_link_id);
		/* Preserve link peer stats to link VIF before deletion */
		if (link_vif)
			ath12k_dp_aggr_link_peer_to_link_vif(link_vif, peer, dp_peer,
							     stats_link_id);
		/* Clear per-packet stats to prevent double-counting on reuse */
		ath12k_dp_aggr_clear_per_pkt_stats(dp_peer, stats_link_id);
	}

	rcu_assign_pointer(dp_peer->link_peers[peer->link_id], NULL);

	if (peer->peer_id != ATH12K_MLO_PEER_ID_INVALID) {
		peerid_index = ath12k_dp_peer_get_peerid_index(dp, peer->peer_id);
		rcu_assign_pointer(dp_hw->dp_peer_list[peerid_index], NULL);
	}

	/* To handle roaming and split phy scenario */
	temp_peer = ath12k_dp_link_peer_find_by_addr(dp, addr);
	if (temp_peer && temp_peer->hw_link_id == ar->hw_link_id)
		ath12k_dp_link_peer_rhash_delete(dp, peer);

	peer->is_assigned = false;
	spin_unlock_bh(&dp_hw->peer_lock);
}

void ath12k_dp_cp_link_peer_unassign(struct ath12k *ar,
				     struct ath12k_link_vif *arvif,
				     struct ath12k_sta *ahsta, u8 link_id,
				     u8 *addr)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_hw *dp_hw = &ar->ah->dp_hw;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_dp_link_vif *link_vif = NULL;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_link_sta *arsta;
	struct ath12k_hw *ah = ar->ah;
	u8 vdev_id = arvif->vdev_id;

	lockdep_assert_wiphy(ah->hw->wiphy);

	ahvif = arvif->ahvif;
	if (ahvif)
		link_vif = &ahvif->dp_vif.dp_link_vif[arvif->link_id];

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, addr);
	if (!peer || !peer->is_assigned) {
		spin_unlock_bh(&dp->dp_lock);
		return;
	}

	__ath12k_dp_link_peer_unassign(ar, dp, dp_hw, peer, link_vif, addr);

	spin_unlock_bh(&dp->dp_lock);

	if (WARN_ON(link_id >= ATH12K_NUM_MAX_LINKS))
		return;

	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);
	if (WARN_ON(!arsta))
		return;

	ahsta->links_map &= ~BIT(link_id);
	ahsta->device_bitmap &= ~BIT(ab->wsi_info.index);
	ahsta->mlo_hw_link_id_bitmap &= ~BIT(arvif->ar->pdev->hw_link_id);
	ahsta->num_peer--;
	rcu_assign_pointer(ahsta->link[link_id], NULL);

	ath12k_cfr_decrement_peer_count(ar, arsta);
	spin_lock_bh(&ar->ab->base_lock);
	ath12k_link_sta_rhash_delete(ar->ab, arsta);
	spin_unlock_bh(&ar->ab->base_lock);
	synchronize_rcu();

	if (arsta == &ahsta->deflink) {
		arsta->link_id = ATH12K_INVALID_LINK_ID;
		arsta->ahsta = NULL;
		arsta->arvif = NULL;
	} else {
		kfree(arsta);
	}
}

void ath12k_dp_link_peer_unassign(struct ath12k *ar, u8 vdev_id, u8 *addr)
{
	struct ath12k_pdev_dp *dp_pdev = &ar->dp;
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_dp_hw *dp_hw = &ar->ah->dp_hw;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_link_vif *link_vif = NULL;

	arvif = ath12k_mac_get_arvif(ar, vdev_id);
	if (arvif) {
		ahvif = arvif->ahvif;
		if (ahvif)
			link_vif = &ahvif->dp_vif.dp_link_vif[arvif->link_id];
	}

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, vdev_id, addr);
	if (!peer || !peer->is_assigned) {
		spin_unlock_bh(&dp->dp_lock);
		return;
	}

	__ath12k_dp_link_peer_unassign(ar, dp, dp_hw, peer, link_vif, addr);

	spin_unlock_bh(&dp->dp_lock);

	synchronize_rcu();
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
	rate_info->rx_retries = link_peer->peer_stats.rx_retries;

	spin_unlock_bh(&dp->dp_lock);
}

bool ath12k_dp_link_peer_reset_tx_stats(struct ath12k_dp *dp, const u8 *addr)
{
	struct ath12k_htt_tx_stats *tx_stats = NULL;
        struct ath12k_dp_link_peer *link_peer;

        spin_lock_bh(&dp->dp_lock);
        link_peer = ath12k_dp_link_peer_find_by_addr(dp, addr);
        if (!link_peer) {
                spin_unlock_bh(&dp->dp_lock);
                return false;
        }

        if (!link_peer->peer_stats.tx_stats) {
                spin_unlock_bh(&dp->dp_lock);
                return false;
        }

        tx_stats = link_peer->peer_stats.tx_stats;
        memset(tx_stats, 0, sizeof(*tx_stats));

        spin_unlock_bh(&dp->dp_lock);
        return true;
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

struct ath12k_dp_peer_qos *
ath12k_dp_peer_qos_get(struct ath12k_dp *dp,
		       struct ath12k_dp_peer *peer)
{
	struct ath12k_dp_peer_qos *qos = NULL;

	if (peer && peer->qos)
		qos = peer->qos;

	return qos;
}

struct ath12k_dp_peer_qos *
ath12k_dp_peer_qos_alloc(struct ath12k_dp *dp,
			 struct ath12k_dp_peer *peer)
{
	struct ath12k_dp_peer_qos *qos;

	if (!peer)
		return NULL;

	qos = kzalloc(sizeof(*qos), GFP_ATOMIC);
	if (!qos)
		return NULL;

	peer->qos = qos;

	ath12k_dbg(dp->ab, ATH12K_DBG_QOS, "Peer QoS allocated");
	return peer->qos;
}

bool ath12k_dp_qos_stats_alloc(struct ath12k *ar,
			       struct ieee80211_vif *vif,
			       struct ath12k_dp_link_peer *peer)
{
	struct ath12k_qos_stats *qos_stats = NULL;

	/* already allocated */
	if (peer->peer_stats.qos_stats)
		return true;

	if (vif->type != NL80211_IFTYPE_AP ||
	    peer->dp_peer->is_vdev_peer ||
	    !ath12k_debugfs_is_qos_stats_enabled(ar))
		return false;

	qos_stats = kzalloc(sizeof(*qos_stats), GFP_ATOMIC);
	if (!qos_stats) {
		ath12k_err(ar->ab, "Peer QoS stats allocation failed for peer: %pM link_id: %u\n",
			   peer->addr, peer->link_id);
		return false;
	}

	peer->peer_stats.qos_stats = qos_stats;

	ath12k_dbg(ar->ab, ATH12K_DBG_QOS, "Peer QoS stats allocated for peer: %pM link_id: %u\n",
		   peer->addr, peer->link_id);
	return true;
}

static u16 ath12k_get_tid_msduq(struct ath12k_base *ab,
				struct ath12k_dp_peer_qos *qos,
				struct ath12k_dp_link_peer *link_peer,
				struct ath12k *ar,
				u16 qos_id, u8 svc_id, u8 tid)
{
	struct ath12k_dp_peer *mld_peer;
	void *telemetry_peer_ctx;
	u8 q, msduq = QOS_INVALID_MSDUQ;

	/* Find matching msduq with qos_id in the reserved pool*/
	for (q = 0; q < QOS_TID_MDSUQ_MAX; ++q) {
		if (qos->msduq_map[tid][q].reserved &&
		    qos->msduq_map[tid][q].qos_id == qos_id) {
			msduq = qos->msduq_map[tid][q].msduq;
			ath12k_dbg(ab, ATH12K_DBG_QOS,
				   "Res:msduq 0x%x:tid %u usrdefq %u\n",
				   msduq, tid, q);
			break;
		}
	}

	if (msduq == QOS_INVALID_MSDUQ) {
		/* Reserve a new one */
		for (q = 0; q < QOS_TID_MDSUQ_MAX; ++q) {
			if (!qos->msduq_map[tid][q].reserved) {
				qos->msduq_map[tid][q].reserved = true;
				qos->msduq_map[tid][q].qos_id = qos_id;
				msduq = u16_encode_bits(q, MSDUQ_MASK) |
							u16_encode_bits(tid, MSDUQ_TID_MASK);
				msduq = msduq + MSDUQ_MAX_DEF;
				qos->msduq_map[tid][q].msduq = msduq;
				ath12k_dbg(ab, ATH12K_DBG_QOS,
					   "New: msduq 0x%x:tid %u usrdefq %u",
					   msduq, tid, q);
				if (!qos->telemetry_peer_ctx &&
				    link_peer && ar && ar->ah) {
					mld_peer = link_peer->dp_peer;
					telemetry_peer_ctx =
						ath12k_telemetry_peer_ctx_alloc(&ar->ah->dp_hw,
										mld_peer,
										link_peer->addr,
										svc_id,
										(msduq - MSDUQ_MAX_DEF));
					if (telemetry_peer_ctx) {
						qos->telemetry_peer_ctx = telemetry_peer_ctx;

					ath12k_dbg(ab, ATH12K_DBG_QOS, "telemetry peer"
						   " ctx allocation with msduq_id:0x%x\n",
						   msduq - MSDUQ_MAX_DEF);
					}
				}
				break;
			}
		}
	}

	return msduq;
}

u16 ath12k_dp_peer_qos_msduq(struct ath12k_base *ab,
			     struct ath12k_dp_peer_qos *qos,
			     struct ath12k_dp_link_peer *link_peer,
			     struct ath12k *ar,
			     u16 qos_id, u8 svc_id)
{
	u16 msduq;
	u8 qos_tid;

	qos_tid = ath12k_qos_get_tid(ab, qos_id);
	if (qos_tid >= QOS_TID_MAX) {
		ath12k_err(ab, "Invalid TID: %d", qos_tid);
		return QOS_INVALID_MSDUQ;
	}

	/* Get MSDUQ for excat QoS profile TID */
	msduq = ath12k_get_tid_msduq(ab, qos, link_peer, ar, qos_id,
				     svc_id, qos_tid);

	/* Get MSDUQ for lower QoS profile TID */
	if (msduq == QOS_INVALID_MSDUQ && qos_tid != 0) {
		u8 tid = qos_tid - 1;

		while (tid < qos_tid) {
			msduq = ath12k_get_tid_msduq(ab, qos, link_peer, ar,
						     qos_id, svc_id, tid);
			if (msduq != QOS_INVALID_MSDUQ)
				break;
			--tid;
		}
	}
	/* Get MSDUQ for higher QoS profile TID */
	if (msduq == QOS_INVALID_MSDUQ) {
		u8 tid = qos_tid + 1;

		while (tid < QOS_MAX_TID) {
			msduq = ath12k_get_tid_msduq(ab, qos, link_peer, ar,
						     qos_id, svc_id, tid);
			if (msduq != QOS_INVALID_MSDUQ)
				break;
			++tid;
		}
	}
	return msduq;
}

int ath12k_dp_peer_scs_add(struct ath12k_base *ab,
			   struct ath12k_dp_peer_qos *qos,
			   u8 scs_id, u16 qos_id)
{
	struct ath12k_dl_scs *scs;
	u16 qos_data;

	if (scs_id >= QOS_MAX_SCS_ID) {
		ath12k_err(ab, "ath12k: Invalid SCS ID");
		return -EINVAL;
	}

	scs = &qos->scs_map[scs_id];
	qos_data = u16_encode_bits(QOS_INVALID_MSDUQ, SCS_MSDUQ_MASK) |
		   u16_encode_bits(qos_id, SCS_QOS_ID_MASK);
	scs->qos_id_msduq = qos_data;

	ath12k_info(ab, "Peer QoS add scs_id:%d | msduq:%d| qos_id:%d",
		    scs_id, u16_get_bits(qos_data, SCS_MSDUQ_MASK),
		    u16_get_bits(qos_data, SCS_QOS_ID_MASK));

	return 0;
}

int ath12k_dp_peer_scs_del(struct ath12k_base *ab,
			   struct ath12k_dp_peer_qos *qos,
			   u8 scs_id)
{
	struct ath12k_dl_scs *scs;
	u16 qos_data;

	if (!qos) {
		ath12k_err(ab, "Invalid QoS");
		return -EINVAL;
	}

	scs = &qos->scs_map[scs_id];
	qos_data = u16_encode_bits(QOS_INVALID_MSDUQ, SCS_MSDUQ_MASK) |
		   u16_encode_bits(QOS_ID_INVALID, SCS_QOS_ID_MASK);
	scs->qos_id_msduq = qos_data;

	ath12k_info(ab, "Peer QoS del scs_id:%d | msduq:%d| qos_id:%d",
		    scs_id, u16_get_bits(qos_data, SCS_MSDUQ_MASK),
		    u16_get_bits(qos_data, SCS_QOS_ID_MASK));
	return 0;
}

u16 ath12k_dp_peer_scs_get_qos_id(struct ath12k_base *ab,
				  struct ath12k_dp_peer_qos *qos, u8 scs_id)
{
	u16 qos_id = QOS_ID_INVALID;

	if (!qos)
		return qos_id;

	qos_id = u16_get_bits(qos->scs_map[scs_id].qos_id_msduq,
			      SCS_QOS_ID_MASK);

	ath12k_dbg(ab, ATH12K_DBG_QOS,
		   "Peer QoS Get QoS ID: %d| qos_id:%d",
		   scs_id, u16_get_bits(qos->scs_map[scs_id].qos_id_msduq,
					SCS_QOS_ID_MASK));
	return qos_id;
}

int ath12k_dp_peer_scs_data(struct ath12k_dp *dp,
			    struct ath12k_dp_peer_qos *qos, u8 scs_id,
			    u16 *queue, u16 *id)
{
	u16 qos_data;
	u16 msduq = QOS_INVALID_MSDUQ, qos_id = QOS_ID_INVALID;
	int ret = -EINVAL;

	if (!qos)
		goto ret;

	msduq = u16_get_bits(qos->scs_map[scs_id].qos_id_msduq,
			     SCS_MSDUQ_MASK);
	qos_id = u16_get_bits(qos->scs_map[scs_id].qos_id_msduq,
			      SCS_QOS_ID_MASK);

	if (msduq != QOS_INVALID_MSDUQ && qos_id < QOS_ID_INVALID) {
		ret = 0;
		goto ret;
	}

	if (qos_id >= QOS_ID_INVALID) {
		ath12k_err(dp->ab, "Invalid QoS ID: %d", qos_id);
		goto ret;
	}

	if (qos_id > QOS_UL_ID_MAX && qos_id < QOS_ID_INVALID)
		msduq = qos_id - QOS_LEGACY_DL_ID_MIN;
	else
		msduq = ath12k_dp_peer_qos_msduq(dp->ab, qos, NULL,
						 NULL, qos_id, scs_id);

	if (msduq == QOS_INVALID_MSDUQ)
		goto ret;

	qos_data = u16_encode_bits(msduq, SCS_MSDUQ_MASK) |
		   u16_encode_bits(qos_id, SCS_QOS_ID_MASK);
	qos->scs_map[scs_id].qos_id_msduq = qos_data;

	ath12k_dbg(dp->ab, ATH12K_DBG_QOS,
		   "Peer QoS get scs_id:%d | msduq:%d| qos_id:%d",
		   scs_id, u16_get_bits(qos_data, SCS_MSDUQ_MASK),
		   u16_get_bits(qos_data, SCS_QOS_ID_MASK));

	ret = 0;
ret:
	*queue = msduq;
	*id = qos_id;
	return ret;
}
EXPORT_SYMBOL(ath12k_dp_peer_scs_data);

u16 dp_peer_msduq_qos_id(struct ath12k_base *ab,
			 struct ath12k_dp_peer_qos *qos,
			 u16 msduq)
{
	u8 tid, q;

	if (msduq < (ab->def_tid_msduq * SDWF_MAX_TID_SUPPORT)) {
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "Invalid msduq: 0x%x in mark\n", msduq);
		return 0;
	}

	msduq = msduq - MSDUQ_MAX_DEF;
	q = u16_get_bits(msduq, MSDUQ_MASK);
	tid = u16_get_bits(msduq, MSDUQ_TID_MASK);

	if (qos->msduq_map[tid][q].reserved)
		return qos->msduq_map[tid][q].qos_id;

	return QOS_ID_INVALID;
}
EXPORT_SYMBOL(dp_peer_msduq_qos_id);

void ath12k_peer_qos_queue_ind_handler(struct ath12k_base *ab,
				       struct sk_buff *skb)
{
	struct htt_t2h_qos_info_ind *resp;
	struct ath12k_dp_peer_qos *qos;
	struct ath12k_dp_link_peer *peer = NULL;
	struct ath12k_dp_peer *mld_peer = NULL;
	u32 htt_qtype, remapped_tid, peer_id;
	u32 def_tid_msduq, max_def_msduq, qos_tid_msduq;
	u32 hlos_tid, flow_or, ast_idx, who_cl, tgt_opaque_id;
	u32 max_qos_msduq;
	u8 msduq_index, q_id;

	resp = (struct htt_t2h_qos_info_ind *)skb->data;
	htt_qtype = u32_get_bits(__le32_to_cpu(resp->info0),
				 HTT_T2H_QOS_MSDUQ_INFO_0_IND_HTT_QTYPE_ID);
	peer_id = u32_get_bits(__le32_to_cpu(resp->info0),
			       HTT_T2H_QOS_MSDUQ_INFO_0_IND_PEER_ID);

	remapped_tid = u32_get_bits(__le32_to_cpu(resp->info1),
				    HTT_T2H_QOS_MSDUQ_INFO_1_IND_REMAP_TID_ID);
	hlos_tid = u32_get_bits(__le32_to_cpu(resp->info1),
				HTT_T2H_QOS_MSDUQ_INFO_1_IND_HLOS_TID_ID);
	who_cl = u32_get_bits(__le32_to_cpu(resp->info1),
			      HTT_T2H_QOS_MSDUQ_INFO_1_IND_WHO_CLSFY_INFO_SEL_ID);
	flow_or = u32_get_bits(__le32_to_cpu(resp->info1),
			       HTT_T2H_QOS_MSDUQ_INFO_1_IND_FLOW_OVERRIDE_ID);
	ast_idx = u32_get_bits(__le32_to_cpu(resp->info1),
			       HTT_T2H_QOS_MSDUQ_INFO_1_IND_AST_INDEX_ID);

	tgt_opaque_id = u32_get_bits(__le32_to_cpu(resp->info2),
				     HTT_T2H_QOS_MSDUQ_INFO_2_IND_TGT_OPAQUE_ID);

	ath12k_info(ab, "QoS MSDUQ Map Ind:\n");
	ath12k_info(ab,
		    "htt_qtype[0x%x]Peer_Id[0x%x]Remp_Tid[0x%x]Hlos_Tid[0x%x]",
		    htt_qtype, peer_id, remapped_tid, hlos_tid);
	ath12k_info(ab, "who_cl[0x%x]flow_or[0x%x]Ast[0x%x]Op[0x%x]",
		    who_cl, flow_or, ast_idx, tgt_opaque_id);

	spin_lock_bh(&ab->base_lock);
	def_tid_msduq = ab->def_tid_msduq;
	qos_tid_msduq = ab->max_tid_msduq - ab->def_tid_msduq;
	spin_unlock_bh(&ab->base_lock);

	max_def_msduq = def_tid_msduq * QOS_TID_MAX;
	max_qos_msduq = qos_tid_msduq * QOS_TID_MAX;
	msduq_index = ((who_cl * max_def_msduq) +
		      (flow_or * QOS_TID_MAX) + hlos_tid) -
		      max_def_msduq;

	spin_lock_bh(&ab->dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_id(ab->dp, peer_id);
	if (msduq_index < max_qos_msduq && peer && peer->dp_peer) {
		q_id = htt_qtype - def_tid_msduq;

		if (hlos_tid < QOS_TID_MAX &&
		    q_id < (qos_tid_msduq)) {
			qos = peer->dp_peer->qos;
			qos->msduq_map[hlos_tid][q_id].tgt_opaque_id =
							tgt_opaque_id;
		}

		mld_peer = peer->dp_peer;
		if (mld_peer->qos &&
		    mld_peer->qos->telemetry_peer_ctx)
			ath12k_telemetry_update_tid_msduq(mld_peer->qos->telemetry_peer_ctx,
							  msduq_index, remapped_tid,
							  (htt_qtype - def_tid_msduq));
		}
	spin_unlock_bh(&ab->dp->dp_lock);
}
