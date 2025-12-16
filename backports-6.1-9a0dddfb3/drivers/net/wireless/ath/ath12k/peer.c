// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "peer.h"
#include "dp_peer.h"
#include "debug.h"
#ifdef CPTCFG_MAC80211_PPE_SUPPORT
#include "ppe.h"
#endif
#include "telemetry_agent_if.h"

/* Timer callback for peer deletion timeout */
static void ath12k_peer_del_timeout(struct timer_list *t)
{
	struct ath12k_peer_del_entry *entry = from_timer(entry, t, timer);
	struct ath12k_pdev *pdev = entry->pdev;
	struct ath12k_peer_del_tracker *tracker;
	struct ath12k_base *ab;
	bool removed = false;
	u8 addr[ETH_ALEN];
	u32 vdev_id;

	if (!pdev)
		return;

	tracker = pdev->peer_del_tracker;
	if (!tracker)
		return;

	ab = ath12k_pdev_to_ab(pdev);

	ether_addr_copy(addr, entry->addr);
	vdev_id = entry->vdev_id;

	/* Remove the peer from hash table */
	spin_lock_bh(&tracker->lock);
	/* Verify entry is still in the hash before removing */
	if (rhashtable_lookup_fast(&tracker->peer_del_hash, entry->addr,
				   tracker->hash_params) == entry) {
		rhashtable_remove_fast(&tracker->peer_del_hash,
				       &entry->rhash_node,
				       tracker->hash_params);
		removed = true;
	}
	spin_unlock_bh(&tracker->lock);

	if (removed) {
		ath12k_warn(ab, "peer delete timeout for %pM vdev %d, removing from tracker\n",
			    addr, vdev_id);

		/* Wake up any waiters on hash_delete_queue */
		wake_up_all(&tracker->hash_delete_queue);

		/* The WARN_ON check is skipped because the firmware does not send a peer
		 * delete response to the host while a recovery is in progress.
		 */
		if (!test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags))
			WARN_ON(1);

		kfree_rcu(entry, rcu_head);
		atomic_dec_if_positive(&pdev->peer_del_tracker_entries);
	}
}

/* Initialize peer deletion tracker for a pdev */
int ath12k_peer_del_tracker_init(struct ath12k_pdev *pdev)
{
	struct ath12k_peer_del_tracker *tracker;
	int ret;

	tracker = kzalloc(sizeof(*tracker), GFP_KERNEL);
	if (!tracker)
		return -ENOMEM;

	spin_lock_init(&tracker->lock);
	init_waitqueue_head(&tracker->hash_delete_queue);

	/* Initialize hash table parameters */
	tracker->hash_params.key_offset = offsetof(struct ath12k_peer_del_entry, addr);
	tracker->hash_params.head_offset = offsetof(struct ath12k_peer_del_entry,
						    rhash_node);
	tracker->hash_params.key_len = ETH_ALEN;
	tracker->hash_params.automatic_shrinking = true;

	ret = rhashtable_init(&tracker->peer_del_hash, &tracker->hash_params);
	if (ret) {
		ath12k_warn(ath12k_pdev_to_ab(pdev),
			    "failed to init peer_del hash table for pdev %d: %d\n",
			    pdev->pdev_id, ret);
		kfree(tracker);
		return ret;
	}

	pdev->peer_del_tracker = tracker;
	atomic_set(&pdev->peer_del_tracker_entries, 0);
	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "peer deletion tracker initialized for pdev %d\n", pdev->pdev_id);

	return 0;
}

/* Destroy peer deletion tracker */
void ath12k_peer_del_tracker_destroy(struct ath12k_pdev *pdev)
{
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	struct ath12k_peer_del_entry *entry;
	struct rhashtable_iter iter;

	if (!tracker)
		return;

	/* Prevent new entries from being added */
	spin_lock_bh(&tracker->lock);
	pdev->peer_del_tracker = NULL;
	spin_unlock_bh(&tracker->lock);

	/* Clean up all remaining entries */
	rhashtable_walk_enter(&tracker->peer_del_hash, &iter);
	rhashtable_walk_start(&iter);

	while ((entry = rhashtable_walk_next(&iter)) != NULL) {
		if (IS_ERR(entry))
			continue;

		/* Cancel the timer before removing entry */
		del_timer_sync(&entry->timer);

		rhashtable_remove_fast(&tracker->peer_del_hash,
				       &entry->rhash_node,
				       tracker->hash_params);
		kfree_rcu(entry, rcu_head);
		atomic_dec_if_positive(&pdev->peer_del_tracker_entries);
	}

	rhashtable_walk_stop(&iter);
	rhashtable_walk_exit(&iter);

	rhashtable_destroy(&tracker->peer_del_hash);
	kfree(tracker);
	atomic_set(&pdev->peer_del_tracker_entries, 0);

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "peer deletion tracker destroyed for pdev %d\n", pdev->pdev_id);
}

/* Add a peer to the deletion tracking hash */
int ath12k_peer_del_tracker_add(struct ath12k_pdev *pdev, u32 vdev_id, const u8 *addr)
{
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	struct ath12k_peer_del_entry *entry;
	int ret;

	if (!tracker)
		return -EINVAL;

	entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry)
		return -ENOMEM;

	entry->vdev_id = vdev_id;
	ether_addr_copy(entry->addr, addr);
	entry->pdev = pdev;

	timer_setup(&entry->timer, ath12k_peer_del_timeout, 0);

	spin_lock_bh(&tracker->lock);
	ret = rhashtable_insert_fast(&tracker->peer_del_hash,
				     &entry->rhash_node,
				     tracker->hash_params);
	spin_unlock_bh(&tracker->lock);

	if (ret) {
		ath12k_warn(ath12k_pdev_to_ab(pdev),
			    "failed to add peer %pM vdev %d to del tracker: %d\n",
			    addr, vdev_id, ret);
		kfree(entry);
		return ret;
	}

	/* Start the 10 second timer */
	mod_timer(&entry->timer,
		  jiffies + msecs_to_jiffies(ATH12K_PEER_DELETE_TIMEOUT_MS));

	atomic_inc(&pdev->peer_del_tracker_entries);

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "added peer %pM vdev %d to deletion tracker with timer\n",
		   addr, vdev_id);

	return 0;
}

/* Remove a peer from the deletion tracking hash */
void ath12k_peer_del_tracker_remove(struct ath12k_pdev *pdev, u32 vdev_id, const u8 *addr)
{
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	struct ath12k_peer_del_entry *entry;

	if (!tracker)
		return;

	spin_lock_bh(&tracker->lock);
	entry = rhashtable_lookup_fast(&tracker->peer_del_hash, addr,
				       tracker->hash_params);
	if (entry && entry->vdev_id == vdev_id) {
		rhashtable_remove_fast(&tracker->peer_del_hash,
				       &entry->rhash_node,
				       tracker->hash_params);
		spin_unlock_bh(&tracker->lock);

		del_timer_sync(&entry->timer);

		ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
			   "removed peer %pM vdev %d from deletion tracker and cancelled timer\n",
			   addr, vdev_id);

		/* Wake up any waiters on hash_delete_queue */
		wake_up_all(&tracker->hash_delete_queue);

		kfree_rcu(entry, rcu_head);
		atomic_dec_if_positive(&pdev->peer_del_tracker_entries);
	} else {
		spin_unlock_bh(&tracker->lock);
		if (!entry) {
			ath12k_err(ath12k_pdev_to_ab(pdev),
				   "peer %pM not found in deletion tracker\n", addr);
		} else {
			ath12k_err(ath12k_pdev_to_ab(pdev),
				   "peer %pM vdev mismatch (expected %d, found %d)\n",
				   addr, vdev_id, entry->vdev_id);
		}
	}
}

/* Check if a peer is in the deletion tracking hash */
bool ath12k_peer_del_tracker_check(struct ath12k_pdev *pdev, const u8 *addr)
{
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;

	if (!tracker)
		return false;

	spin_lock_bh(&tracker->lock);
	if (rhashtable_lookup_fast(&tracker->peer_del_hash, addr,
				   tracker->hash_params)) {
		spin_unlock_bh(&tracker->lock);
		return true;
	}

	spin_unlock_bh(&tracker->lock);
	return false;
}

/* Wait for a peer to be removed from deletion tracker */
int ath12k_peer_del_tracker_wait(struct ath12k_pdev *pdev, const u8 *addr,
				 unsigned long timeout_ms)
{
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	long ret;

	if (!tracker)
		return -EINVAL;

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "waiting for peer %pM to be removed from deletion tracker (timeout %lu ms)\n",
		   addr, timeout_ms);

	ret = wait_event_timeout(tracker->hash_delete_queue,
				 !ath12k_peer_del_tracker_check(pdev, addr),
				 msecs_to_jiffies(timeout_ms));

	if (!ret) {
		ath12k_warn(ath12k_pdev_to_ab(pdev),
			    "timeout waiting for peer %pM deletion\n", addr);
		return -ETIMEDOUT;
	}

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "peer %pM removed from deletion tracker\n", addr);

	return 0;
}

/* Clear all peers for a specific vdev from deletion tracker */
int ath12k_peer_del_tracker_clear_vdev(struct ath12k_pdev *pdev, u32 vdev_id)
{
	struct ath12k *ar = pdev->ar;
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	struct ath12k_peer_del_entry *entry, **entries_to_remove;
	struct rhashtable_iter iter;
	int count = 0, i, num_entries = 0;
	int max_entries;

	if (!tracker)
		return -EINVAL;

	if (!atomic_read(&pdev->peer_del_tracker_entries))
		return 0;

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "clearing deletion tracker for vdev %d\n", vdev_id);

	max_entries = ar->max_num_stations;

	entries_to_remove = kcalloc(max_entries, sizeof(*entries_to_remove),
				    GFP_KERNEL);
	if (!entries_to_remove)
		return -ENOMEM;

	rhashtable_walk_enter(&tracker->peer_del_hash, &iter);
	rhashtable_walk_start(&iter);

	while ((entry = rhashtable_walk_next(&iter)) != NULL) {
		if (IS_ERR(entry))
			continue;

		if (entry->vdev_id == vdev_id && num_entries < max_entries)
			entries_to_remove[num_entries++] = entry;
	}

	rhashtable_walk_stop(&iter);
	rhashtable_walk_exit(&iter);

	for (i = 0; i < num_entries; i++) {
		entry = entries_to_remove[i];
		del_timer_sync(&entry->timer);

		spin_lock_bh(&tracker->lock);
		rhashtable_remove_fast(&tracker->peer_del_hash,
				       &entry->rhash_node,
				       tracker->hash_params);
		spin_unlock_bh(&tracker->lock);

		ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
			   "removed peer %pM from deletion tracker (vdev %d)\n",
			   entry->addr, vdev_id);

		kfree_rcu(entry, rcu_head);
		atomic_dec_if_positive(&pdev->peer_del_tracker_entries);
		count++;
	}

	if (count > 0)
		wake_up_all(&tracker->hash_delete_queue);

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "cleared %d peers from deletion tracker for vdev %d\n",
		   count, vdev_id);

	kfree(entries_to_remove);

	return 0;
}

int ath12k_peer_del_tracker_clear_pdev(struct ath12k_pdev *pdev)
{
	struct ath12k_peer_del_tracker *tracker = pdev->peer_del_tracker;
	struct ath12k_peer_del_entry *entry, **entries_to_remove;
	struct rhashtable_iter iter;
	int count = 0, i, num_entries = 0;
	int max_entries = ATH12K_PEER_DEL_TRACKER_MAX_ENTRIES;

	if (!tracker)
		return -EINVAL;

	if (!atomic_read(&pdev->peer_del_tracker_entries))
		return 0;

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "clearing deletion tracker for pdev %d\n", pdev->pdev_id);

	entries_to_remove = kcalloc(max_entries, sizeof(*entries_to_remove),
				    GFP_KERNEL);
	if (!entries_to_remove)
		return -ENOMEM;

	rhashtable_walk_enter(&tracker->peer_del_hash, &iter);
	rhashtable_walk_start(&iter);

	while ((entry = rhashtable_walk_next(&iter)) != NULL) {
		if (IS_ERR(entry))
			continue;

		if (num_entries < max_entries)
			entries_to_remove[num_entries++] = entry;
	}

	rhashtable_walk_stop(&iter);
	rhashtable_walk_exit(&iter);

	for (i = 0; i < num_entries; i++) {
		entry = entries_to_remove[i];
		del_timer_sync(&entry->timer);

		spin_lock_bh(&tracker->lock);
		rhashtable_remove_fast(&tracker->peer_del_hash, &entry->rhash_node,
				       tracker->hash_params);
		spin_unlock_bh(&tracker->lock);

		kfree_rcu(entry, rcu_head);
		atomic_dec_if_positive(&pdev->peer_del_tracker_entries);
		count++;
	}

	if (count > 0)
		wake_up_all(&tracker->hash_delete_queue);

	ath12k_dbg(ath12k_pdev_to_ab(pdev), ATH12K_DBG_PEER,
		   "cleared %d peers from deletion tracker for pdev %d\n",
		   count, pdev->pdev_id);

	kfree(entries_to_remove);

	return 0;
}

static int ath12k_wait_for_peer_common(struct ath12k_base *ab, int vdev_id,
				       const u8 *addr, bool expect_mapped)
{
	int ret;

	ret = wait_event_timeout(ab->peer_mapping_wq, ({
				bool mapped;

				spin_lock_bh(&ab->dp->dp_lock);
				mapped = !!ath12k_dp_link_peer_find_by_vdev_id_and_addr(ab->dp, vdev_id, addr);
				spin_unlock_bh(&ab->dp->dp_lock);

				(mapped == expect_mapped ||
				 test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags));
				}), 3 * HZ);

	if (ret <= 0)
		return -ETIMEDOUT;

	return 0;
}

void ath12k_peer_cleanup(struct ath12k *ar, u32 vdev_id)
{
	struct ath12k_dp_link_peer *peer, *tmp;
	struct ath12k_base *ab = ar->ab;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&ab->dp->dp_lock);
	list_for_each_entry_safe(peer, tmp, &ab->dp->peers, list) {
		if (peer->vdev_id == vdev_id && peer->sta) {
			ath12k_warn(ab,
				    "removing stale remote peer %pM from vdev_id %d\n",
				    peer->addr, vdev_id);

			ath12k_link_peer_free(peer);
			ar->num_peers--;
		}
	}

	spin_unlock_bh(&ab->dp->dp_lock);
}

static int ath12k_peer_delete_send(struct ath12k *ar, u32 vdev_id, const u8 *addr,
				   u32 mlo_hw_link_id_bitmap)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_link_peer *peer;
	int ret;
	struct ath12k_sta *ahsta;
	u16 link_id_mask = 0;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&ar->ab->dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(ab->dp,
							    vdev_id, addr);
	if (peer && !peer->is_bridge_peer) {
		ret = ath12k_telemetry_peer_agent_delete_handler(ar, vdev_id,
								 addr);
		if (ret && ret != -EOPNOTSUPP) {
			ath12k_dbg(ab, ATH12K_DBG_PEER,
				   "failed to delete peer reference in TA for vdev_id %d addr %pM ret %d\n",
				   vdev_id, addr, ret);
		}
	}
	 spin_unlock_bh(&ar->ab->dp->dp_lock);

	if (peer && peer->sta) {
		ahsta = ath12k_sta_to_ahsta(peer->sta);
		if (ahsta) {
			ahsta->peer_delete_cmd_sent_bitmap |= BIT(peer->link_id);
			link_id_mask = BIT(peer->link_id);
		}
	}

	if (ar->pdev->peer_del_tracker) {
		ret = ath12k_peer_del_tracker_add(ar->pdev, vdev_id, addr);
		if (ret)
			ath12k_warn(ab, "failed to add peer %pM to deletion tracker: %d\n",
				    addr, ret);
	}

	ret = ath12k_wmi_send_peer_delete_cmd(ar, addr, vdev_id, mlo_hw_link_id_bitmap);
	if (ret) {
		ath12k_warn(ab,
			    "failed to delete peer vdev_id %d addr %pM ret %d\n",
			    vdev_id, addr, ret);

		if (link_id_mask)
			ahsta->peer_delete_cmd_sent_bitmap &= ~link_id_mask;

		/* Remove the peer entry from the peer_del tracker when sending the WMI
		 * peer delete command fails.
		 */
		ath12k_peer_del_tracker_remove(ar->pdev, vdev_id, addr);

		return ret;
	}

	return 0;
}

static int __ath12k_peer_delete(struct ath12k *ar, u32 vdev_id, u8 *addr,
				bool skip_peer_del, u32 mlo_hw_link_id_bitmap)
{
	int ret;
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_base *ab = ar->ab;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ath12k_dp_link_peer_unassign(ar, vdev_id, addr);

	ath12k_dp_arch_link_peer_delete(ar->ab->dp, ar->ab, vdev_id, addr);

	if (test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags)) {
		ath12k_warn(ar->ab, "skipped peer delete cmd for vdev_id %d addr %pM during recovery ret:%d\n",
				vdev_id, addr, -EHOSTDOWN);

		return -EHOSTDOWN;
	}

	if (skip_peer_del)
		return 0;

	ret = ath12k_peer_delete_send(ar, vdev_id, addr, mlo_hw_link_id_bitmap);
	if (ret)
		return ret;

	rcu_read_lock();
	arvif = ath12k_mac_get_arvif(ar, vdev_id);
	if (!arvif) {
		ath12k_warn(ab,"failed to get arvif with vdev_id %d,"
			    "skip ppeds ast override\n",
			    vdev_id);
	} else if (arvif->ahvif->vif->type == NL80211_IFTYPE_STATION) {
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		ath12k_dp_tx_ppeds_cfg_astidx_cache_mapping(ab, arvif, false);
#endif
	}

	rcu_read_unlock();

	if (arvif)
		arvif->num_peers--;

	return 0;
}

int ath12k_peer_delete(struct ath12k *ar, u32 vdev_id, u8 *addr,
		       bool skip_peer_del, u32 mlo_hw_link_id_bitmap)
{
	int ret;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	ret = __ath12k_peer_delete(ar, vdev_id, addr, skip_peer_del,
				   mlo_hw_link_id_bitmap);
	if (ret && ret != -EHOSTDOWN)
		return ret;

	ar->num_peers--;

	return ret;
}

static int ath12k_wait_for_peer_created(struct ath12k *ar, int vdev_id, const u8 *addr)
{
	return ath12k_wait_for_peer_common(ar->ab, vdev_id, addr, true);
}

static int ath12k_wait_for_peer_create_done(struct ath12k *ar, u32 vdev_id,
					    const u8 *addr)
{
	int ret;
	unsigned long time_left;

	ret = ath12k_wait_for_peer_created(ar, vdev_id, addr);
	if (ret) {
		ath12k_warn(ar->ab, "failed wait for peer create peer_addr : %pM\n",
			    addr);
		WARN_ON(1);
		return ret;
	}

	time_left = wait_for_completion_timeout(&ar->peer_create_done,
						3 * HZ);
	if (time_left == 0) {
		ath12k_warn(ar->ab, "Timeout in receiving peer create conf peer_addr : %pM\n",
			    addr);
		return -ETIMEDOUT;
	}

	return 0;
}

int ath12k_peer_create(struct ath12k *ar, struct ath12k_link_vif *arvif,
		       struct ieee80211_sta *sta,
		       struct ath12k_wmi_peer_create_arg *arg)
{
	struct ieee80211_vif *vif = ath12k_ahvif_to_vif(arvif->ahvif);
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_link_sta *arsta;
	u8 link_id = arvif->link_id;
	struct ath12k_dp_link_peer *peer;
	struct ath12k_sta *ahsta;
	u16 ml_peer_id;
	int ret;
	struct ath12k_dp_link_vif *dp_link_vif = &ahvif->dp_vif.dp_link_vif[link_id];
	struct ath12k_dp *dp = ath12k_ab_to_dp(ar->ab);

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	if (sta)
		ahsta = ath12k_sta_to_ahsta(sta);

	/* Check if peer is in deletion tracker and wait if necessary */
	if (ar->pdev->peer_del_tracker &&
	    ath12k_peer_del_tracker_check(ar->pdev, arg->peer_addr)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_PEER,
			   "peer %pM is pending deletion, waiting up to 3 seconds\n",
			   arg->peer_addr);

		ret = ath12k_peer_del_tracker_wait(ar->pdev, arg->peer_addr,
						   ATH12K_PEER_DEL_TRACKER_TIMEOUT_MS);
		if (ret) {
			ath12k_err(ar->ab,
				   "peer %pM still in deletion tracker after timeout, cannot create\n",
				   arg->peer_addr);
			return -EBUSY;
		}
	}

	if (ar->num_peers >= (ar->max_num_peers - 1)) {
		ath12k_warn(ar->ab,
			    "failed to create peer due to insufficient peer entry resource in firmware\n");
		return -ENOBUFS;
	}

	spin_lock_bh(&dp->dp_lock);
	peer = ath12k_dp_link_peer_find_by_pdev_idx(dp, ar->pdev_idx,
						    arg->peer_addr);
	if (peer) {
		spin_unlock_bh(&dp->dp_lock);
		return -EINVAL;
	}
	spin_unlock_bh(&dp->dp_lock);

	ret = ath12k_dp_arch_link_peer_create(dp, ar->ab,
					      arg->vdev_id, arg->peer_addr);
	if (ret) {
		ath12k_warn(ar->ab, "Unable to create link peer %pM\n", arg->peer_addr);
		return -EINVAL;
	}

	reinit_completion(&ar->peer_create_done);

	ret = ath12k_wmi_send_peer_create_cmd(ar, arg);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send peer create vdev_id %d ret %d\n",
			    arg->vdev_id, ret);
		ath12k_dp_arch_link_peer_delete(dp, ar->ab,
						arg->vdev_id, arg->peer_addr);
		return ret;
	}

	ret = ath12k_wait_for_peer_create_done(ar, arg->vdev_id,
					       arg->peer_addr);
	if (ret) {
		ath12k_dp_arch_link_peer_delete(dp, ar->ab,
						arg->vdev_id, arg->peer_addr);
		return ret;
	}

	spin_lock_bh(&dp->dp_lock);

	peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp, arg->vdev_id,
							    arg->peer_addr);
	if (!peer) {
		spin_unlock_bh(&dp->dp_lock);
		ath12k_warn(ar->ab, "failed to find peer %pM on vdev %i after creation\n",
			    arg->peer_addr, arg->vdev_id);

		ret = __ath12k_peer_delete(ar, arg->vdev_id, arg->peer_addr,
					   false, ahsta->mlo_hw_link_id_bitmap);
		if (ret)
			ath12k_warn(ar->ab, "failed to delete peer vdev_id %d addr %pM\n",
				    arg->vdev_id, arg->peer_addr);

		return -ENOENT;
	}

	peer->pdev_idx = ar->pdev_idx;
	peer->sta = sta;
	peer->vif = vif;
	peer->is_bridge_peer = arg->mlo_bridge_peer;

	if (vif->type == NL80211_IFTYPE_STATION) {
		dp_link_vif->ast_hash = peer->ast_hash;
		dp_link_vif->ast_idx = peer->hw_peer_id;

	}

	if (sta) {
		arsta = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
					  ahsta->link[link_id]);

		peer->link_id = arsta->link_id;

		if (!peer->is_bridge_peer) {
			ret = ath12k_telemetry_peer_agent_create_handler(ar,
					arg->vdev_id,
					arg->peer_addr);
			if (ret && ret != -EOPNOTSUPP) {
				ath12k_dbg(ar->ab, ATH12K_DBG_PEER,
						"failed to create peer reference in TA for vdev_id %d addr %pM ret %d\n",
						arg->vdev_id, arg->peer_addr, ret);
			}
		}

		/* Fill ML info into created peer */
		if (sta->mlo) {
			ml_peer_id = ahsta->ml_peer_id;
			peer->ml_id = ml_peer_id | ATH12K_PEER_ML_ID_VALID;
			ether_addr_copy(peer->ml_addr, sta->addr);

			peer->mlo = true;
		} else {
			peer->ml_id = ATH12K_MLO_PEER_ID_INVALID;
			peer->mlo = false;
		}
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_PEER, "peer created %pM\n", arg->peer_addr);

	ar->num_peers++;

	spin_unlock_bh(&dp->dp_lock);

	ret = ath12k_dp_link_peer_assign(ar, arvif->vdev_id,
					 sta, arg->peer_addr,
					 link_id, ar->hw_link_id, vif,
					 arvif->ahvif->dp_vif.ppe_vp_type,
					 arvif->ahvif->dp_vif.ppe_vp_num);

	if (ret) {
		ath12k_peer_delete(ar, arg->vdev_id, arg->peer_addr, false, 0);
		spin_lock_bh(&dp->dp_lock);
		peer = ath12k_dp_link_peer_find_by_vdev_id_and_addr(dp,
								    arg->vdev_id,
								    arg->peer_addr);
		ath12k_link_peer_free(peer);
		spin_unlock_bh(&dp->dp_lock);
	}

	return ret;
}

int ath12k_peer_dp_cp_link_peer_delete(struct ath12k_link_vif *arvif,
				       struct ath12k_sta *ahsta, u8 link_id,
				       bool peer_del_all, int link_going_down,
				       u8 *addr)
{
	u32 mlo_hw_link_id_bitmap = 0;
	struct ath12k *ar;
	int ret;

	if (!arvif)
		return 0;

	ar = arvif->ar;

	if (ahsta)
		mlo_hw_link_id_bitmap = ahsta->mlo_hw_link_id_bitmap;

	ath12k_dp_peer_cleanup(ar, arvif->vdev_id, addr);
	ath12k_dp_cp_link_peer_unassign(ar, arvif, ahsta, link_id, addr);
	ath12k_dp_arch_link_peer_delete(ar->ab->dp, ar->ab,
					arvif->vdev_id, addr);

	if (peer_del_all && link_going_down == arvif->link_id &&
	    ahsta->primary_link_id == link_going_down) {
		ath12k_dbg(ar->ab, ATH12K_DBG_PEER,
			   "Skipping peer delete for %pM due to peer_del_all:%d link_going_down:%d\n",
			   addr, peer_del_all, link_going_down);
		return 0;
	}

	if (test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags)) {
		ath12k_warn(ar->ab,
			    "skipped peer delete cmd for vdev_id %d addr %pM during recovery ret:%d\n",
			    arvif->vdev_id, addr, -EHOSTDOWN);
		return -EHOSTDOWN;
	}

	ret = ath12k_peer_delete_send(ar, arvif->vdev_id, addr,
				      mlo_hw_link_id_bitmap);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to delete peer vdev_id %d addr %pM ret %d\n",
			    arvif->vdev_id, addr, ret);
	}

	return ret;
}

int ath12k_peer_mlo_link_peers_delete(struct ath12k_vif *ahvif,
				      struct ath12k_sta *ahsta,
				      bool peer_del_all,
				      int link_going_down)
{
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(ahsta);
	struct ath12k_hw *ah = ahvif->ah;
	struct ath12k_link_vif *arvif;
	struct ath12k_link_sta *arsta;
	char link_addr[ATH12K_NUM_MAX_LINKS][ETH_ALEN];
	unsigned long links;
	struct ath12k *ar;
	int ret, err_ret = 0, primary_link_id;
	u8 link_id;

	lockdep_assert_wiphy(ah->hw->wiphy);

	if (!sta->mlo)
		return -EINVAL;

	/* FW expects delete of all link peers at once before waiting for reception
	 * of peer unmap or delete responses
	 */
	links = ahsta->links_map;
	primary_link_id = ahsta->primary_link_id;
	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arvif = wiphy_dereference(ah->hw->wiphy, ahvif->link[link_id]);
		arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);

		ar = arvif->ar;
		if (!ar)
			continue;

		memcpy(link_addr[link_id], arsta->addr, ETH_ALEN);
		ret = ath12k_peer_dp_cp_link_peer_delete(arvif, ahsta, link_id,
							 peer_del_all, link_going_down,
							 link_addr[link_id]);
		if (ret)
			err_ret = ret;

		if (peer_del_all && link_going_down == arvif->link_id &&
		    primary_link_id == link_going_down)
			continue;

		if (test_bit(ATH12K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags) ||
		    test_bit(ATH12K_FLAG_RECOVERY, &ar->ab->dev_flags) ||
		    test_bit(ATH12K_FLAG_UMAC_RECOVERY_START, &ar->ab->dev_flags))
			continue;

		if (ahsta->peer_delete_cmd_sent_bitmap & BIT(link_id)) {
			ar->num_peers--;
			arvif->num_peers--;
		}
	}

	return err_ret;
}

void ath12k_mac_peer_disassoc(struct ath12k_base *ab, struct ieee80211_sta *sta,
			      struct ath12k_sta *ahsta,
			      enum ath12k_debug_mask debug_mask)
{
	if (!ahsta->low_ack_sent) {
		ath12k_dbg(ab, debug_mask, "sending low ack for/disassoc:%pM\n",
			   sta->addr);
		/* set num of packets to maximum so that we distinguish in
		 * the hostapd to send disassoc irrespective of hostapd conf
		 */
		ieee80211_report_low_ack(sta, ATH12K_REPORT_LOW_ACK_NUM_PKT);
		/* Using this flag to avoid certain known warnings which
		 * will be triggerred when umac reset is happening
		 */
		ahsta->low_ack_sent = true;
	}
}

static inline int ath12k_link_sta_rhash_insert(struct ath12k_base *ab,
					       struct rhashtable *rtbl,
					       struct rhash_head *rhead,
					       struct rhashtable_params *params)
{
	struct ath12k_link_sta *tmp;

	lockdep_assert_held(&ab->base_lock);

	tmp = rhashtable_lookup_get_insert_fast(rtbl, rhead, *params);

	if (!tmp)
		return 0;
	else if (IS_ERR(tmp))
		return PTR_ERR(tmp);
	else
		return -EEXIST;
}

static inline int ath12k_link_sta_rhash_remove(struct ath12k_base *ab,
					       struct rhashtable *rtbl,
					       struct rhash_head *rhead,
					       struct rhashtable_params *params)
{
	int ret;

	lockdep_assert_held(&ab->base_lock);

	ret = rhashtable_remove_fast(rtbl, rhead, *params);
	if (ret && ret != -ENOENT)
		return ret;

	return 0;
}

int ath12k_link_sta_rhash_add(struct ath12k_base *ab,
			      struct ath12k_link_sta *arsta)
{
	int ret;

	lockdep_assert_held(&ab->base_lock);

	if (!ab->rhead_sta_addr)
		return -EPERM;

	if (arsta->rhash_done)
		return 0;

	ret = ath12k_link_sta_rhash_insert(ab, ab->rhead_sta_addr,
					   &arsta->rhash_addr,
					   &ab->rhash_sta_addr_param);

	if (ret) {
		ath12k_warn(ab, "failed to add arsta %pM in rhash_addr ret %d\n",
			    arsta->addr, ret);
		arsta->rhash_done = false;
	} else {
		arsta->rhash_done = true;
	}

	return ret;
}

int ath12k_link_sta_rhash_delete(struct ath12k_base *ab,
				 struct ath12k_link_sta *arsta)
{
	int ret;

	lockdep_assert_held(&ab->base_lock);

	if (!ab->rhead_sta_addr)
		return -EPERM;

	if (!arsta->rhash_done)
		return 0;

	ret = ath12k_link_sta_rhash_remove(ab, ab->rhead_sta_addr,
					   &arsta->rhash_addr,
					   &ab->rhash_sta_addr_param);

	if (ret) {
		ath12k_warn(ab,
			    "failed to remove arsta %pM in rhash_addr ret %d\n",
			    arsta->addr, ret);
		return ret;
	}

	arsta->rhash_done = false;

	return ret;
}

static int ath12k_link_sta_rhash_addr_tbl_init(struct ath12k_base *ab)
{
	struct rhashtable_params *param;
	struct rhashtable *rhash_addr_tbl;
	int ret;
	size_t size;

	lockdep_assert_held(&ab->tbl_mtx_lock);

	if (ab->rhead_sta_addr)
		return 0;

	size = sizeof(*ab->rhead_sta_addr);
	rhash_addr_tbl = kzalloc(size, GFP_KERNEL);
	if (!rhash_addr_tbl)
		return -ENOMEM;

	param = &ab->rhash_sta_addr_param;

	param->key_offset = offsetof(struct ath12k_link_sta, addr);
	param->head_offset = offsetof(struct ath12k_link_sta, rhash_addr);
	param->key_len = sizeof_field(struct ath12k_link_sta, addr);
	param->automatic_shrinking = true;
	param->nelem_hint = ab->num_radios * ath12k_core_get_max_peers_per_radio(ab);

	ret = rhashtable_init(rhash_addr_tbl, param);
	if (ret) {
		ath12k_warn(ab, "failed to init peer addr rhash table %d\n",
			    ret);
		goto err_free;
	}

	if (!ab->rhead_sta_addr)
		ab->rhead_sta_addr = rhash_addr_tbl;
	else
		goto cleanup_tbl;

	return 0;

cleanup_tbl:
	rhashtable_destroy(rhash_addr_tbl);
err_free:
	kfree(rhash_addr_tbl);

	return ret;
}

int ath12k_link_sta_rhash_tbl_init(struct ath12k_base *ab)
{
	int ret;

	mutex_lock(&ab->tbl_mtx_lock);
	ret = ath12k_link_sta_rhash_addr_tbl_init(ab);
	mutex_unlock(&ab->tbl_mtx_lock);

	return ret;
}

void ath12k_link_sta_rhash_tbl_destroy(struct ath12k_base *ab)
{
	mutex_lock(&ab->tbl_mtx_lock);

	if (!ab->rhead_sta_addr)
		goto unlock;

	rhashtable_destroy(ab->rhead_sta_addr);
	kfree(ab->rhead_sta_addr);
	ab->rhead_sta_addr = NULL;

unlock:
	mutex_unlock(&ab->tbl_mtx_lock);
}

struct ath12k_link_sta *ath12k_link_sta_find_by_addr(struct ath12k_base *ab,
						     const u8 *addr)
{
	lockdep_assert_held(&ab->base_lock);

	if (!ab->rhead_sta_addr)
		return NULL;

	return rhashtable_lookup_fast(ab->rhead_sta_addr, addr,
				      ab->rhash_sta_addr_param);
}

int ath12k_peer_send_assoc_vendor_response(const struct ath12k_dp_link_peer *peer,
					   bool is_assoc)
{
	struct ieee80211_link_sta *link_sta;
	struct ath12k_link_sta *arsta;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	struct ath12k *ar;

	if (!peer) {
		ath12k_dbg(NULL, ATH12K_DBG_PEER,
			   "Invalid peer skipped assoc vendor response\n");
		return -EINVAL;
	}

	sta = peer->sta;
	if (!sta) {
		ath12k_dbg(NULL, ATH12K_DBG_PEER,
			   "Invalid sta skipped assoc vendor response\n");
		return -EINVAL;
	}

	if (peer->link_id < 0) {
		ath12k_dbg(NULL, ATH12K_DBG_PEER,
			   "Invalid peer link id skipped assoc vendor response\n");
		return -EINVAL;
	}
	rcu_read_lock();
	ahsta = ath12k_sta_to_ahsta(sta);
	arsta = ahsta->link[peer->link_id];
	if (!(arsta && arsta->arvif)) {
		rcu_read_unlock();
		ath12k_dbg(NULL, ATH12K_DBG_PEER,
			   "invalid arsta for peer: %pM skipped assoc vendor response\n",
			   peer->addr);
		return -EINVAL;
	}

	ar = arsta->arvif->ar;

	link_sta = ath12k_mac_get_link_sta(arsta);

	if (!link_sta) {
		rcu_read_unlock();
		ath12k_warn(ar->ab, "unable to access link sta skipped assoc vendor response\n");
		return -EINVAL;
	}
	rcu_read_unlock();

	if (is_assoc)
		ath12k_mac_vendor_send_assoc_event(arsta, link_sta, true);
	else
		ath12k_mac_vendor_send_disassoc_event(arsta, link_sta);

	return 0;
}
